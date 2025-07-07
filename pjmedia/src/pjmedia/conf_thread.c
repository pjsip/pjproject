/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

/* Modifications related to multithreading, parallelization and vectorization
 * by Leonid Goltsblat <lgoltsblat@gmail.com> 2021-2025
 */

#include <pjmedia/conference.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/delaybuf.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pjmedia/resample.h>
#include <pjmedia/silencedet.h>
#include <pjmedia/sound_port.h>
#include <pjmedia/stereo.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/atomic_slist.h>

#if PJMEDIA_CONF_BACKEND == PJMEDIA_CONF_PARALLEL_BRIDGE_BACKEND

/* CONF_DEBUG enables detailed operation of the conference bridge.
 * Beware that it prints large amounts of logs (several lines per frame).
 */
//#define CONF_DEBUG
#ifdef CONF_DEBUG
#   include <stdio.h>
#   define TRACE_(x)   PJ_LOG(5,x)
#else
#   define TRACE_(x)
#endif


//#define CONF_DEBUG_EX
#ifdef CONF_DEBUG_EX
//#   include <stdio.h>
#   define TRACE_EX(x)   PJ_LOG(5,x)
#else
#   define TRACE_EX(x)
#endif


/* REC_FILE macro enables recording of the samples written to the sound
 * device. The file contains RAW PCM data with no header, and has the
 * same settings (clock rate etc) as the conference bridge.
 * This should only be enabled when debugging audio quality *only*.
 */
//#define REC_FILE    "confrec.pcm"
#ifdef REC_FILE
static FILE *fhnd_rec;
#endif

/* "Perfect" buffer alignment for vectorization (for SIMD instruction set)
 * Compilers can optimize code significantly if all data buffers are correctly 
 * aligned for the instruction set being used. The easiest way to ensure that 
 * all port buffers of a conference bridge are aligned is to set the default 
 * pool alignment at the port level.
 * 
 * It is unknown whether compilers exist in the real world 
 * that check this data alignment.
 * 
 * Comment this definition to use the PJSIP default alignment.
 */
#define SIMD_ALIGNMENT  32

#define CONF_CHECK_SUCCESS(expr, err_action) \
            { \
                pj_status_t tmp_status_ = (expr); \
                if (tmp_status_ != PJ_SUCCESS) { \
                    char errbuf[PJ_ERR_MSG_SIZE]; \
                    pj_strerror(tmp_status_, errbuf, sizeof(errbuf)); \
                    PJ_LOG(1,(THIS_FILE, "\"%s\" fails in %s:%d, " \
                              "status=%d (%s)", \
                              #expr, THIS_FILE, __LINE__, tmp_status_,errbuf)); \
                    err_action; \
                } \
            }

#define CONF_CHECK_NOT_NULL(expr, err_action)  \
            { \
                if ((expr)==0) { \
                    PJ_LOG(1,(THIS_FILE, "Test \"%s\" != 0 fails in " \
                                         "%s:%d", \
                              #expr, THIS_FILE,__LINE__));\
                    err_action; \
                } \
            }

#define THIS_FILE           "conf_thread.c"

#define RX_BUF_COUNT        PJMEDIA_SOUND_BUFFER_COUNT

#define BYTES_PER_SAMPLE    2

#define SIGNATURE           PJMEDIA_CONF_BRIDGE_SIGNATURE
#define SIGNATURE_PORT      PJMEDIA_SIG_PORT_CONF_PASV
/* Normal level is hardcodec to 128 in all over places */
#define NORMAL_LEVEL        128
#define SLOT_TYPE           unsigned
#define INVALID_SLOT        ((SLOT_TYPE)-1)


/* These are settings to control the adaptivity of changes in the
 * signal level of the ports, so that sudden change in signal level
 * in the port does not cause misaligned signal (which causes noise).
 */
#if defined(PJMEDIA_CONF_USE_AGC) && PJMEDIA_CONF_USE_AGC != 0
#   define ATTACK_A     ((conf->sampling_rate / conf->samples_per_frame) >> 4)
#   define ATTACK_B     1
#   define DECAY_A      0
#   define DECAY_B      1

#   define SIMPLE_AGC(last, target) \
    if (target >= last) \
        target = (ATTACK_A*(last+1)+ATTACK_B*target)/(ATTACK_A+ATTACK_B); \
    else \
        target = (DECAY_A*last+DECAY_B*target)/(DECAY_A+DECAY_B)
#else
#   define SIMPLE_AGC(last, target)
#endif

#define MAX_LEVEL   (32767)
#define MIN_LEVEL   (-32768)

#define IS_OVERFLOW(s) ((s > MAX_LEVEL) || (s < MIN_LEVEL))


/*
 * DON'T GET CONFUSED WITH TX/RX!!
 *
 * TX and RX directions are always viewed from the conference bridge's point
 * of view, and NOT from the port's point of view. So TX means the bridge
 * is transmitting to the port, RX means the bridge is receiving from the
 * port.
 */

/**
 * src port RX buffer to mix audiodata at the dest side
 */
typedef struct PJ_ATOMIC_SLIST_ALIGN_PREFIX rx_buffer_slist_node {
    PJ_DECL_ATOMIC_SLIST_MEMBER(struct rx_buffer_slist_node);

    pj_int16_t         *rx_frame_buf;/**< source data to mix                 */
    unsigned            listener_adj_level; /**< adjustment level for current
                                             * TX from current RX port       */
    struct conf_port   *rx_port;     /**< source (RX) port (for debuging)    */

} PJ_ATOMIC_SLIST_ALIGN_SUFFIX rx_buffer_slist_node;



/**
 * This is a port connected to conference bridge.
 */
struct conf_port
{
    pj_pool_t           *pool;          /**< for autonomous lifetime control
                                         * we need separate memory pool
                                         * this port created from           */
    pj_str_t             name;          /**< Port name.                     */
    pjmedia_port        *port;          /**< get_frame() and put_frame()    */
    pjmedia_port_op      rx_setting;    /**< Can we receive from this port  */
    pjmedia_port_op      tx_setting;    /**< Can we transmit to this port   */
    unsigned             listener_cnt;  /**< Number of listeners.           */
    SLOT_TYPE           *listener_slots;/**< Array of listeners.            */
    unsigned            *listener_adj_level;
                                        /**< Array of listeners' level
                                             adjustment.                    */
    unsigned             transmitter_cnt;/**<Number of transmitters.        */

    /* Shortcut for port info. */
    unsigned             sampling_rate; /**< Port's sampling rate.          */
    unsigned             samples_per_frame; /**< Port's samples per frame.  */
    unsigned             channel_count; /**< Port's channel count.          */

    /* Calculated signal levels: */
    unsigned             tx_level;      /**< Last tx level to this port.    */
    unsigned             rx_level;      /**< Last rx level from this port.  */

    /* The normalized signal level adjustment.
     * A value of 128 (NORMAL_LEVEL) means there's no adjustment.
     */
    unsigned             tx_adj_level;  /**< Adjustment for TX.             */
    unsigned             rx_adj_level;  /**< Adjustment for RX.             */
    pj_int16_t          *adj_level_buf; /**< The adjustment buffer.
                                         *   Works on TX (listener) side!   */

    /* Resample, for converting clock rate, if they're different. */
    pjmedia_resample    *rx_resample;
    pjmedia_resample    *tx_resample;

    /* RX buffer is temporary buffer to be used when there is mismatch
     * between port's sample rate or ptime with conference's sample rate
     * or ptime. The buffer is used for sampling rate conversion AND/OR to
     * buffer the samples until there are enough samples to fulfill a 
     * complete frame to be processed by the bridge.
     *
     * When both sample rate AND ptime of the port match the conference 
     * settings, this buffer will not be created.
     * 
     * This buffer contains samples at port's clock rate.
     * The size of this buffer is the sum between port's samples per frame
     * and bridge's samples per frame.
     */
    pj_int16_t          *rx_buf;        /**< The RX buffer.                 */
    unsigned             rx_buf_cap;    /**< Max size, in samples           */
    unsigned             rx_buf_count;  /**< # of samples in the buf.       */

    /* Mix buf is a temporary buffer used to mix all signal received
     * by this port from all other ports. The mixed signal will be 
     * automatically adjusted to the appropriate level whenever
     * there is possibility of clipping.
     *
     * This buffer contains samples at bridge's clock rate.
     * The size of this buffer is equal to samples per frame of the bridge.
     */

    int                  mix_adj;       /**< Adjustment level for mix_buf.  */
    int                  last_mix_adj;  /**< Last adjustment level.         */
    pj_int32_t          *mix_buf;       /**< Total sum of signal.           */

    /* Tx buffer is a temporary buffer to be used when there's mismatch 
     * between port's clock rate or ptime with conference's sample rate
     * or ptime. This buffer is used as the source of the sampling rate
     * conversion AND/OR to buffer the samples until there are enough
     * samples to fulfill a complete frame to be transmitted to the port.
     *
     * When both sample rate and ptime of the port match the bridge's 
     * settings, this buffer will not be created.
     * 
     * This buffer contains samples at port's clock rate.
     * The size of this buffer is the sum between port's samples per frame
     * and bridge's samples per frame.
     */
    pj_int16_t          *tx_buf;        /**< Tx buffer.                     */
    unsigned             tx_buf_cap;    /**< Max size, in samples.          */
    unsigned             tx_buf_count;  /**< # of samples in the buffer.    */

    /* When the port is not receiving signal from any other ports (e.g. when
     * no other ports is transmitting to this port), the bridge periodically
     * transmit NULL frame to the port to keep the port "alive" (for example,
     * a stream port needs this heart-beat to periodically transmit silence
     * frame to keep NAT binding alive).
     *
     * This NULL frame should be sent to the port at the port's ptime rate.
     * So if the port's ptime is greater than the bridge's ptime, the bridge
     * needs to delay the NULL frame until it's the right time to do so.
     *
     * This variable keeps track of how many pending NULL samples are being
     * "held" for this port. Once this value reaches samples_per_frame
     * value of the port, a NULL frame is sent. The samples value on this
     * variable is clocked at the port's clock rate.
     */
    unsigned             tx_heart_beat;

    /* Delay buffer is a special buffer for sound device port (port 0, master
     * port) and other passive ports (sound device port is also passive port).
     *
     * We need the delay buffer because we can not expect the mic and speaker 
     * thread to run equally after one another. In most systems, each thread 
     * will run multiple times before the other thread gains execution time. 
     * For example, in my system, mic thread is called three times, then 
     * speaker thread is called three times, and so on. This we call burst.
     *
     * There is also possibility of drift, unbalanced rate between put_frame
     * and get_frame operation, in passive ports. If drift happens, snd_buf
     * needs to be expanded or shrinked. 
     *
     * Burst and drift are handled by delay buffer.
     */
    pjmedia_delay_buf   *delay_buf;

    pj_bool_t            is_new;        /**< Newly added port, 
                                         * remove it synchronously.        */

    pj_bool_t            is_active_listener;/**< Port was added into
                                             * active_listener[] and must
                                             * be removed from this array 
                                             * in op_remove_port()         */

    /* parallel conference bridge support                                  */
    pj_int16_t          *rx_frame_buf;  /**< The RX buffer of size (in bytes)
                                         * conf->rx_frame_buf_cap used in
                                         * parallel bridge implementation.
                                         */
    pj_lock_t           *tx_lock;       /**< Lock to protect memory 
                                         * allocation                      */

    pj_timestamp         last_timestamp;/**< last transmited packet
                                         * timestamp. We set this when
                                         * first time put something into
                                         * the mix_buf.
                                         * If this time stamp is equals to
                                         * current frame timestamp,
                                         * we have data to transmite       */
    unsigned             mixed_cnt;     /**<The number of transmitters 
                                         * that mixed data into this port's 
                                         * mix_buf during the current tick.*/

    pj_atomic_slist     *buff_to_mix;    /**< queue to mix TX (source) data 
                                          * for current RX (listener) port */
    pj_atomic_t         *requests_to_mix;/**< counter of requests to data 
                                          * mixing used to monopolize 
                                          * mixing operation on RX side    */
    pj_atomic_slist     *free_node_cache;/**< cache of currently unused 
                                          * nodes for buff_to_mix          */

#ifdef CONF_DEBUG_EX
    SLOT_TYPE            slot;          /**< SLOT for debug purpose        */
#endif //CONF_DEBUG_EX
};


/* Forward declarations */
typedef struct op_entry op_entry;


/*
 * port_slot is an array item, which index is index in ports[] (i.e. ports[] slot)
 * port_slot used to quick find empty slot. Only array index is used for this purpose.
 */
typedef struct PJ_ATOMIC_SLIST_ALIGN_PREFIX port_slot {
    PJ_DECL_ATOMIC_SLIST_MEMBER(struct port_slot);
} PJ_ATOMIC_SLIST_ALIGN_SUFFIX port_slot;


/*
 * Conference bridge.
 */
struct pjmedia_conf
{
    pj_pool_t            *pool;         /**< Pool                           */
    unsigned              options;      /**< Bitmask options.               */
    unsigned              max_ports;    /**< Maximum ports.                 */
    unsigned              port_cnt;     /**< Current number of ports.       */

    unsigned              connect_cnt;  /**< Total number of connections    */
    pjmedia_snd_port     *snd_dev_port; /**< Sound device port.             */
    pjmedia_port         *master_port;  /**< Port zero's port.              */
    char                  master_name_buf[80]; /**< Port0 name buffer.      */
    pj_mutex_t           *mutex;        /**< Conference mutex.              */
    struct conf_port    **ports;        /**< Array of ports.                */
    unsigned              sampling_rate;/**< Sampling rate.                 */
    unsigned              channel_count;/**< Number of channels (1=mono).   */
    unsigned              samples_per_frame;    /**< Samples per frame.     */
    unsigned              bits_per_sample;      /**< Bits per sample.       */

    op_entry             *op_queue;     /**< Queue of operations.           */
    op_entry             *op_queue_free;/**< Queue of free entries.         */
    pjmedia_conf_op_cb    cb;           /**< OP callback.                   */


    pj_atomic_slist     *unused_slots;    /**< Unused port's slots.         */
    port_slot           *free_port_slots; /**< Persistent array of max_ports
                                           * size used for quick and locking
                                           * free finding unused ports[]
                                           * slot (i.e. index in ports[])   */

    SLOT_TYPE           *active_ports;    /**< array of port slots from 0
                                           * to upper_bound with no gaps.
                                           * Compacted port's array should 
                                           * help to distribute task 
                                           * throught team's threads        */
    pj_uint32_t          upper_bound;     /**< The next after greatest
                                           *   connected RX port's slot
                                           * current active_ports[] size    */
    SLOT_TYPE           *active_listener; /**< Packed array of registered 
                                           * ports, having not null put_frame
                                           * Such ports may be "listeners"
                                           * i.e. listen and transmit data
                                           * from other ports
                                           * listener may not be connected
                                           * but still should be able to 
                                           * transmit heartbeat frames      */
    pj_uint32_t          upper_bound_reg; /**< The next after greatest
                                           *   registered port's slot,
                                           * current active_listener[] size */

    /* parallel conference bridge support                                   */
    pj_bool_t            is_parallel;     /**< parallel bridge flag         */
    unsigned             rx_frame_buf_cap;/**< size in bytes to allocate
                                           * conf_port->rx_frame_buf        */

    /* native pjsip multithreading                                          */
    pj_atomic_value_t    threads;         /**< The number of threads to use.
                                           * 1 means the operations will be
                                           * done only by get_frame() thread*/
    pj_thread_t        **pool_threads;    /**< Thread pool's threads        */
    pj_barrier_t        *active_thread;   /**< entry barrier                */
    pj_barrier_t        *barrier;         /**< exit barrier                 */
    pj_atomic_t         *active_thread_cnt;/**< active worker thread counter*/
    pj_event_t          *barrier_evt;     /**< exit barrier                 */
    pj_bool_t            quit_flag;       /**< quit flag for threads        */
    pj_bool_t            running;         /**< thread pool is running       */
    pj_atomic_t         *active_ports_idx;/**< index of the element of the
                                           * active_ports[] array processed 
                                           * by the current thread          */
    pj_atomic_t         *listener_counter;/**< index of the element of the
                                           * active_listener[] array 
                                           * processed by the current thread*/
    pjmedia_frame       *frame;           /**< Frame buffer for conference 
                                           * bridge at the current tick.    */
    struct conf_port    *sound_port;      /**< Flag: if not NULL this port
                                           * has data in mix_buf to
                                           * return sound playback frame    */

};


/* Prototypes */
static pj_status_t put_frame(pjmedia_port *this_port, 
                             pjmedia_frame *frame);
static pj_status_t get_frame(pjmedia_port *this_port, 
                             pjmedia_frame *frame);
static pj_status_t destroy_port(pjmedia_port *this_port);

#if !DEPRECATED_FOR_TICKET_2234
static pj_status_t get_frame_pasv(pjmedia_port *this_port, 
                                  pjmedia_frame *frame);
static pj_status_t destroy_port_pasv(pjmedia_port *this_port);
#endif


static inline pj_int16_t *get_read_buffer(struct conf_port *conf_port, pjmedia_frame *frame);
static pj_status_t thread_pool_start(pjmedia_conf *conf);
static void perform_get_frame(pjmedia_conf *conf);
/* Conf thread pool's thread function.*/
static int conf_thread(void *arg);

/* mix and perhaps transmit data for listener from conf_port */
static void  mix_and_transmit(pjmedia_conf *conf, struct conf_port *listener, 
                              SLOT_TYPE listener_slot,
                              unsigned listener_adj_level, 
                              struct conf_port *conf_port,
                              pj_int16_t *p_in,
                              const pj_timestamp *timestamp);

static void destroy_conf_port(struct conf_port *conf_port);

/* As we don't hold mutex in the clock/get_frame(), some conference operations
 * that change conference states need to be synchronized with the clock.
 * So some steps of the operations needs to be executed within the clock tick
 * context, especially the steps related to changing ports connection.
 */

/* Synchronized operation list entry. */
typedef struct op_entry {
    PJ_DECL_LIST_MEMBER(struct op_entry);
    pjmedia_conf_op_type          type;
    pjmedia_conf_op_param         param;
} op_entry;

/* Prototypes of synchronized operation */
static pj_status_t op_add_port(pjmedia_conf *conf,
                               const pjmedia_conf_op_param *prm);
static pj_status_t op_remove_port(pjmedia_conf *conf,
                                  const pjmedia_conf_op_param *prm);
static pj_status_t op_connect_ports(pjmedia_conf *conf,
                                    const pjmedia_conf_op_param *prm);
static pj_status_t op_disconnect_ports(pjmedia_conf *conf,
                                       const pjmedia_conf_op_param *prm);

static op_entry* get_free_op_entry(pjmedia_conf *conf)
{
    op_entry *ope = NULL;

    /* Get from empty list if any, otherwise, allocate a new one */
    if (!pj_list_empty(conf->op_queue_free)) {
        ope = conf->op_queue_free->next;
        pj_list_erase(ope);
    } else {
        ope = PJ_POOL_ZALLOC_T(conf->pool, op_entry);
    }
    return ope;
}

static void handle_op_queue(pjmedia_conf *conf)
{
    /* The queue may grow while mutex is released, better put a limit? */
    enum { MAX_PROCESSED_OP = 100 };
    int i = 0;

    while (i++ < MAX_PROCESSED_OP) {
        op_entry *op;
        pjmedia_conf_op_type type;
        pjmedia_conf_op_param param;
        pj_status_t status;

        pj_mutex_lock(conf->mutex);

        /* Stop when queue empty */
        if (pj_list_empty(conf->op_queue)) {
            pj_mutex_unlock(conf->mutex);
            break;
        }

        /* Copy op */
        op = conf->op_queue->next;
        type = op->type;
        param = op->param;

        /* Free op */
        pj_list_erase(op);
        op->type = PJMEDIA_CONF_OP_UNKNOWN;
        pj_list_push_back(conf->op_queue_free, op);

        pj_mutex_unlock(conf->mutex);

        /* Process op */
        switch(type) {
        case PJMEDIA_CONF_OP_ADD_PORT:
            status = op_add_port(conf, &param);
            break;
        case PJMEDIA_CONF_OP_REMOVE_PORT:
            status = op_remove_port(conf, &param);
            break;
        case PJMEDIA_CONF_OP_CONNECT_PORTS:
            status = op_connect_ports(conf, &param);
            break;
        case PJMEDIA_CONF_OP_DISCONNECT_PORTS:
            status = op_disconnect_ports(conf, &param);
            break;
        default:
            status = PJ_EINVALIDOP;
            pj_assert(!"Invalid sync-op in conference");
            break;
        }
        if (conf->cb) {
            pjmedia_conf_op_info info = { 0 };

            pj_log_push_indent();
            info.op_type = type;
            info.status = status;
            info.op_param = param;
            (*conf->cb)(&info);
            pj_log_pop_indent();
        }
    }
}


/* Group lock handler */
static void conf_port_on_destroy(void *arg)
{
    struct conf_port *conf_port = (struct conf_port*)arg;
    destroy_conf_port(conf_port);
}

/*
* port is active (has listers or transmitters), i.e. conference bridge should not skip this port,
* as voice should be send to or receive from this port.
*/
static pj_bool_t is_port_connected(struct conf_port* p_conf_port)
{
    return p_conf_port && (p_conf_port->listener_cnt || p_conf_port->transmitter_cnt);
}

/*
 * Find empty port slot in the conference bridge and reserve this slot.
 * O(1) thread-safe operation
 */
static SLOT_TYPE conf_reserve_port(pjmedia_conf *conf);

/*
 * Return conf_port slot to unused slots cache.
 * O(1) thread-safe operation
 */
static pj_status_t conf_release_port(pjmedia_conf *conf, SLOT_TYPE slot);


/*
 * Create port.
 */
static pj_status_t create_conf_port( pj_pool_t *parent_pool,
                                     pjmedia_conf *conf,
                                     pjmedia_port *port,
                                     const pj_str_t *name,
                                     struct conf_port **p_conf_port)
{
    struct conf_port *conf_port = NULL;
    pj_pool_t *pool = NULL;
    char pname[PJ_MAX_OBJ_NAME];
    pj_status_t status = PJ_SUCCESS;
    rx_buffer_slist_node *free_node;

    /* Make sure pool name is NULL terminated */
    pj_assert(name);
    pj_ansi_strxcpy2(pname, name, sizeof(pname));

    /* Create own pool
     * replace pool to control it's lifetime 
     */
#ifdef SIMD_ALIGNMENT
    pool = pj_pool_aligned_create(parent_pool->factory, pname, 500, 500, 
                                  SIMD_ALIGNMENT, NULL);
#else
    pool = pj_pool_create(parent_pool->factory, pname, 500, 500, NULL);
#endif
    if (!pool) {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Create port. */
    conf_port = PJ_POOL_ZALLOC_T(pool, struct conf_port);
    PJ_ASSERT_ON_FAIL(conf_port, {status = PJ_ENOMEM; goto on_return;});
    conf_port->pool = pool;

    /* Increment port ref count to avoid premature destroy due to
     * asynchronous port removal.
     */
    if (port) {
        if (!port->grp_lock) {
            /* Create group lock if it does not have one */
            pjmedia_port_init_grp_lock(port, pool, NULL);
        }

        pj_grp_lock_add_ref(port->grp_lock);

        /* Pool may be used for creating port's group lock and the group lock
         * may be used by app, so pool destroy must be done via handler.
         */
        status = pj_grp_lock_add_handler(port->grp_lock, NULL, conf_port,
                                         &conf_port_on_destroy);
    }

    /* Set name */
    pj_strdup_with_null(pool, &conf_port->name, name);

    /* Default has tx and rx enabled. */
    conf_port->rx_setting = PJMEDIA_PORT_ENABLE;
    conf_port->tx_setting = PJMEDIA_PORT_ENABLE;

    /* Default level adjustment is 128 (which means no adjustment) */
    conf_port->tx_adj_level = NORMAL_LEVEL;
    conf_port->rx_adj_level = NORMAL_LEVEL;

    /* Create transmit flag array */
    conf_port->listener_slots = (SLOT_TYPE*) pj_pool_zalloc(pool, 
                                          conf->max_ports * sizeof(SLOT_TYPE));
    PJ_ASSERT_ON_FAIL(conf_port->listener_slots,
                      {status = PJ_ENOMEM; goto on_return;});

    /* Create adjustment level array */
    conf_port->listener_adj_level = (unsigned *) pj_pool_zalloc(pool, 
                                       conf->max_ports * sizeof(unsigned));
    PJ_ASSERT_ON_FAIL(conf_port->listener_adj_level,
                      {status = PJ_ENOMEM; goto on_return;});

    /* Save some port's infos, for convenience. */
    if (port) {
        pjmedia_audio_format_detail *afd;

        afd = pjmedia_format_get_audio_format_detail(&port->info.fmt, 1);
        conf_port->port = port;
        conf_port->sampling_rate = afd->clock_rate;
        conf_port->samples_per_frame = PJMEDIA_AFD_SPF(afd);
        conf_port->channel_count = afd->channel_count;
    } else {
        conf_port->port = NULL;
        conf_port->sampling_rate = conf->sampling_rate;
        conf_port->samples_per_frame = conf->samples_per_frame;
        conf_port->channel_count = conf->channel_count;
    }

    /* Create adjustment level buffer. */
    conf_port->adj_level_buf = (pj_int16_t*) pj_pool_zalloc(pool, 
                               conf->samples_per_frame * sizeof(pj_int16_t));
    PJ_ASSERT_ON_FAIL(conf_port->adj_level_buf,
                      {status = PJ_ENOMEM; goto on_return;});

    /* If port's clock rate is different than conference's clock rate,
     * create a resample sessions.
     */
    if (conf_port->sampling_rate != conf->sampling_rate) {

        pj_bool_t high_quality;
        pj_bool_t large_filter;

        high_quality = ((conf->options & PJMEDIA_CONF_USE_LINEAR)==0);
        large_filter = ((conf->options & PJMEDIA_CONF_SMALL_FILTER)==0);

        /* Create resample for rx buffer. */
        status = pjmedia_resample_create( pool, 
                                          high_quality,
                                          large_filter,
                                          conf->channel_count,
                                          conf_port->sampling_rate,/* Rate in */
                                          conf->sampling_rate, /* Rate out */
                                          conf->samples_per_frame * 
                                            conf_port->sampling_rate /
                                            conf->sampling_rate,
                                          &conf_port->rx_resample);
        if (status != PJ_SUCCESS)
            goto on_return;


        /* Create resample for tx buffer. */
        status = pjmedia_resample_create(pool,
                                         high_quality,
                                         large_filter,
                                         conf->channel_count,
                                         conf->sampling_rate,  /* Rate in */
                                         conf_port->sampling_rate, /* Rate out */
                                         conf->samples_per_frame,
                                         &conf_port->tx_resample);
        if (status != PJ_SUCCESS)
            goto on_return;
    }

    /*
     * Initialize rx and tx buffer, only when port's samples per frame or 
     * port's clock rate or channel number is different then the conference
     * bridge settings.
     */
    if (conf_port->sampling_rate != conf->sampling_rate ||
        conf_port->channel_count != conf->channel_count ||
        conf_port->samples_per_frame != conf->samples_per_frame)
    {
        unsigned port_ptime, conf_ptime, buff_ptime;

        port_ptime = conf_port->samples_per_frame / conf_port->channel_count *
            1000 / conf_port->sampling_rate;
        conf_ptime = conf->samples_per_frame / conf->channel_count *
            1000 / conf->sampling_rate;

        /* Check compatibility of sample rate and ptime.
         * Some combinations result in a fractional number of samples per frame
         * which we do not support.
         * One such case would be for example 10ms @ 22050Hz which would yield
         * 220.5 samples per frame.
         */
        if (0 != (port_ptime * conf_port->sampling_rate *
                  conf_port->channel_count % 1000))
        {
            PJ_LOG(3,(THIS_FILE,
                   "Cannot create conf port: incompatible sample rate/ptime"));
            status = PJMEDIA_ENOTCOMPATIBLE;
            goto on_return;
        }

        /* Calculate the size (in ptime) for the port buffer according to
         * this formula:
         *   - if either ptime is an exact multiple of the other, then use
         *     the larger ptime (e.g. 20ms and 40ms, use 40ms).
         *   - if not, then the ptime is sum of both ptimes (e.g. 20ms
         *     and 30ms, use 50ms)
         */
        if (port_ptime > conf_ptime) {
            buff_ptime = port_ptime;
            if (port_ptime % conf_ptime)
                buff_ptime += conf_ptime;
        } else {
            buff_ptime = conf_ptime;
            if (conf_ptime % port_ptime)
                buff_ptime += port_ptime;
        }

        /* Create RX buffer. */
        //conf_port->rx_buf_cap = (unsigned)(conf_port->samples_per_frame +
        //                                 conf->samples_per_frame * 
        //                                 conf_port->clock_rate * 1.0 /
        //                                 conf->clock_rate + 0.5);
        conf_port->rx_buf_cap = conf_port->sampling_rate * buff_ptime / 1000;
        if (conf_port->channel_count > conf->channel_count)
            conf_port->rx_buf_cap *= conf_port->channel_count;
        else
            conf_port->rx_buf_cap *= conf->channel_count;

        conf_port->rx_buf_count = 0;
        conf_port->rx_buf = (pj_int16_t*)
                            pj_pool_alloc(pool, conf_port->rx_buf_cap *
                                                sizeof(conf_port->rx_buf[0]));
        PJ_ASSERT_ON_FAIL(conf_port->rx_buf,
                          {status = PJ_ENOMEM; goto on_return;});

        /* Create TX buffer. */
        conf_port->tx_buf_cap = conf_port->rx_buf_cap;
        conf_port->tx_buf_count = 0;
        conf_port->tx_buf = (pj_int16_t*)
                            pj_pool_alloc(pool, conf_port->tx_buf_cap *
                                                sizeof(conf_port->tx_buf[0]));
        PJ_ASSERT_ON_FAIL(conf_port->tx_buf,
                          {status = PJ_ENOMEM; goto on_return;});
    }


    /* Create mix buffer. */
    conf_port->mix_buf = (pj_int32_t*)
                         pj_pool_zalloc(pool, conf->samples_per_frame *
                                              sizeof(conf_port->mix_buf[0]));
    PJ_ASSERT_ON_FAIL(conf_port->mix_buf,
                      {status = PJ_ENOMEM; goto on_return;});
    conf_port->last_mix_adj = NORMAL_LEVEL;

    /* init auto adjustment level for mixed signal. */
    conf_port->mix_adj = NORMAL_LEVEL;

    /* We need not reset mix_buf, we just want to copy the first
    * (and probably only) frame there.
    * The criteria for "this frame is from the first transmitter"
    *  condition is:
    * (conf_port->last_timestamp.u64 != frame->timestamp.u64)
    * 
    * Enforce "this frame is from the first transmitter" condition:
    * initially we set last_timestamp to -1, so the first frame will be copied.
    */
    conf_port->last_timestamp.u64 = (pj_uint64_t)-1;

    if (conf->is_parallel) {
        CONF_CHECK_NOT_NULL(conf_port->rx_frame_buf = (pj_int16_t*)pj_pool_zalloc(pool, conf->rx_frame_buf_cap), 
                         {status = PJ_ENOMEM;goto on_return;});
        CONF_CHECK_SUCCESS(status=pj_lock_create_simple_mutex(pool, "tx_lock", &conf_port->tx_lock), goto on_return);
    }

    CONF_CHECK_SUCCESS(status=pj_atomic_slist_create(pool, &conf_port->free_node_cache), goto on_return);
    CONF_CHECK_NOT_NULL(free_node = pj_atomic_slist_calloc(pool, 1, sizeof(rx_buffer_slist_node)), 
                     {status = PJ_ENOMEM;goto on_return;});
    CONF_CHECK_SUCCESS(status=pj_atomic_slist_push(conf_port->free_node_cache, free_node), goto on_return);
    CONF_CHECK_SUCCESS(status=pj_atomic_slist_create(pool, &conf_port->buff_to_mix), goto on_return);
    CONF_CHECK_SUCCESS(status=pj_atomic_create(pool, 0, &conf_port->requests_to_mix), goto on_return);

    /* Done */
    *p_conf_port = conf_port;

on_return:
    if (status != PJ_SUCCESS) {
        if (conf_port) {
            /* Destroy resample if this conf port has it. */
            if (conf_port->rx_resample)
                pjmedia_resample_destroy(conf_port->rx_resample);

            if (conf_port->tx_resample)
                pjmedia_resample_destroy(conf_port->tx_resample);

            if (conf_port->tx_lock)
                CONF_CHECK_SUCCESS(pj_lock_destroy(conf_port->tx_lock),(void)0);
            if (conf_port->free_node_cache)
                CONF_CHECK_SUCCESS(pj_atomic_slist_destroy(conf_port->free_node_cache),(void)0);
            if (conf_port->buff_to_mix)
                CONF_CHECK_SUCCESS(pj_atomic_slist_destroy(conf_port->buff_to_mix),(void)0);
            if (conf_port->requests_to_mix)
                CONF_CHECK_SUCCESS(pj_atomic_destroy(conf_port->requests_to_mix),(void)0);

            //TODO grp_lock ?
        }
        if (pool)
            pj_pool_release(pool);
    }

    return status;
}


/*
 * Add passive port.
 */
static pj_status_t create_pasv_port( pjmedia_conf *conf,
                                     pj_pool_t *pool,
                                     const pj_str_t *name,
                                     pjmedia_port *port,
                                     struct conf_port **p_conf_port)
{
    struct conf_port *conf_port;
    pj_status_t status;
    unsigned ptime;

    /* Create port */
    status = create_conf_port(pool, conf, port, name, &conf_port);
    if (status != PJ_SUCCESS)
        return status;

    pool = conf_port->pool;

    /* Passive port has delay buf. */
    ptime = conf->samples_per_frame * 1000 / conf->sampling_rate / 
            conf->channel_count;
    status = pjmedia_delay_buf_create(pool, name->ptr, 
                                      conf->sampling_rate,
                                      conf->samples_per_frame,
                                      conf->channel_count,
                                      RX_BUF_COUNT * ptime, /* max delay */
                                      0, /* options */
                                      &conf_port->delay_buf);
    if (status != PJ_SUCCESS) {
        /* Decrease conf port ref count */
        if (conf_port->port && conf_port->port->grp_lock)
            pj_grp_lock_dec_ref(conf_port->port->grp_lock);
        else
            destroy_conf_port(conf_port);
        return status;
    }

    *p_conf_port = conf_port;

    return PJ_SUCCESS;
}


/*
 * Create port zero for the sound device.
 */
static pj_status_t create_sound_port( pj_pool_t *pool,
                                      pjmedia_conf *conf )
{
    struct conf_port *conf_port;
    pj_str_t name = { "Master/sound", 12 };
    pj_status_t status;


    status = create_pasv_port(conf, pool, &name, NULL, &conf_port);
    if (status != PJ_SUCCESS)
        return status;


    /* Create sound device port: */

    if ((conf->options & PJMEDIA_CONF_NO_DEVICE) == 0) {
        pjmedia_aud_stream *strm;
        pjmedia_aud_param param;

        /*
         * If capture is disabled then create player only port.
         * Otherwise create bidirectional sound device port.
         */
        if (conf->options & PJMEDIA_CONF_NO_MIC)  {
            status = pjmedia_snd_port_create_player(pool, -1, conf->sampling_rate,
                                                    conf->channel_count,
                                                    conf->samples_per_frame,
                                                    conf->bits_per_sample, 
                                                    0,  /* options */
                                                    &conf->snd_dev_port);

        } else {
            status = pjmedia_snd_port_create( pool, -1, -1, conf->sampling_rate, 
                                              conf->channel_count, 
                                              conf->samples_per_frame,
                                              conf->bits_per_sample,
                                              0,    /* Options */
                                              &conf->snd_dev_port);

        }

        if (status != PJ_SUCCESS) {
            destroy_conf_port(conf_port);
            return status;
        }

        strm = pjmedia_snd_port_get_snd_stream(conf->snd_dev_port);
        status = pjmedia_aud_stream_get_param(strm, &param);
        if (status == PJ_SUCCESS) {
            pjmedia_aud_dev_info snd_dev_info;
            if (conf->options & PJMEDIA_CONF_NO_MIC)
                pjmedia_aud_dev_get_info(param.play_id, &snd_dev_info);
            else
                pjmedia_aud_dev_get_info(param.rec_id, &snd_dev_info);
            pj_strdup2_with_null(pool, &conf_port->name, snd_dev_info.name);
        }

        PJ_LOG(5,(THIS_FILE, "Sound device successfully created for port 0"));
    }

#ifdef CONF_DEBUG_EX
    conf_port->slot = 0;
#endif //CONF_DEBUG_EX

     /* Add the port to the bridge */
    conf->ports[0] = conf_port;
    ++conf->port_cnt;
    /* sound device (from 0 SLOT) to 0 idx, others to next idx */
    pj_assert(!conf->upper_bound_reg);
    conf->active_listener[conf->upper_bound_reg++] = 0;
    conf_port->is_active_listener = PJ_TRUE;

    return PJ_SUCCESS;
}

/*
 * Create conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_create( pj_pool_t *pool,
                                         unsigned max_slots,
                                         unsigned sampling_rate,
                                         unsigned channel_count,
                                         unsigned samples_per_frame,
                                         unsigned bits_per_sample,
                                         unsigned options,
                                         pjmedia_conf **p_conf )
{
    pjmedia_conf_param param;

    pjmedia_conf_param_default(&param);

    param.max_slots = max_slots;
    param.sampling_rate = sampling_rate;
    param.channel_count = channel_count;
    param.samples_per_frame = samples_per_frame;
    param.bits_per_sample = bits_per_sample;
    param.options = options;
    /* Let's skip setting the parameter for the number of worker threads here
     * to use the default number of worker threads.
     * param.worker_threads = PJMEDIA_CONF_THREADS-1;
     */

    return pjmedia_conf_create2(pool, &param, p_conf);
}

PJ_DEF(pj_status_t) pjmedia_conf_create2(pj_pool_t *pool_, 
                                         pjmedia_conf_param *param, 
                                         pjmedia_conf **p_conf)
{
    pj_pool_t *pool;
    pjmedia_conf *conf;
    const pj_str_t name = { "Conf", 4 };
    pj_status_t status;
    unsigned i;

    PJ_ASSERT_RETURN(param && p_conf, PJ_EINVAL);

    PJ_ASSERT_RETURN(param->samples_per_frame > 0, PJ_EINVAL);
    /* Can only accept 16bits per sample, for now.. */
    PJ_ASSERT_RETURN(param->bits_per_sample == 16, PJ_EINVAL);

#if defined(CONF_DEBUG_EX) || defined(CONF_DEBUG)
    pj_log_set_level(5);
#endif

    PJ_LOG(5, (THIS_FILE, "Creating conference bridge with %d ports",
               param->max_slots));

    /* Create own pool */
    pool = pj_pool_create(pool_->factory, name.ptr, 512, 512, NULL);
    if (!pool) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Create failed in alloc"));
        return PJ_ENOMEM;
    }

    /* Create and init conf structure. */
    conf = PJ_POOL_ZALLOC_T(pool, pjmedia_conf);
    PJ_ASSERT_ON_FAIL(conf,
                      {pj_pool_release(pool); return PJ_ENOMEM;});
    conf->pool = pool;

    conf->options = param->options;
    conf->max_ports = param->max_slots;
    conf->sampling_rate = param->sampling_rate;
    conf->channel_count = param->channel_count;
    conf->samples_per_frame = param->samples_per_frame;
    conf->bits_per_sample = param->bits_per_sample;
    conf->threads = param->worker_threads + 1;
    conf->is_parallel = (param->worker_threads>0);

    /* loading and storing a properly aligned pointer should be atomic 
     * at the processor level and not require mutex protection 
     */
    conf->ports = 
        pj_pool_aligned_alloc(pool, sizeof(struct conf_port*), 
                              conf->max_ports * sizeof(struct conf_port*));
    PJ_ASSERT_ON_FAIL(conf->ports, 
                              { status = PJ_ENOMEM; goto on_return; });
    pj_bzero(conf->ports, conf->max_ports * sizeof(struct conf_port*));

    conf->active_ports = 
        pj_pool_calloc(pool, conf->max_ports, sizeof(SLOT_TYPE));
    PJ_ASSERT_ON_FAIL(conf->active_ports, 
                              { status = PJ_ENOMEM; goto on_return; });

    pj_assert(conf->upper_bound == 0);        /* no connected ports yet */

    conf->active_listener = 
        pj_pool_calloc(pool, conf->max_ports, sizeof(SLOT_TYPE));
    PJ_ASSERT_ON_FAIL(conf->active_listener, 
                              { status = PJ_ENOMEM; goto on_return; });
    pj_assert(conf->upper_bound_reg == 0);

    /* Create and initialize the master port interface. */
    conf->master_port = PJ_POOL_ZALLOC_T(pool, pjmedia_port);
    PJ_ASSERT_RETURN(conf->master_port, PJ_ENOMEM);

    pjmedia_port_info_init(&conf->master_port->info, &name, SIGNATURE,
                           conf->sampling_rate, conf->channel_count,
                           conf->bits_per_sample, conf->samples_per_frame);

    conf->master_port->port_data.pdata = conf;
    conf->master_port->port_data.ldata = 0;

    conf->master_port->get_frame = &get_frame;
    conf->master_port->put_frame = &put_frame;
    conf->master_port->on_destroy = &destroy_port;

    /* Get the bytes_per_frame value, to determine the size of the
     * buffer.
     */
    conf->rx_frame_buf_cap = 
        PJMEDIA_AFD_AVG_FSZ(pjmedia_format_get_audio_format_detail(
                                &conf->master_port->info.fmt, PJ_TRUE));

    /* Create port zero for sound device. */
    status = create_sound_port(pool, conf);
    if (status != PJ_SUCCESS) {
        pjmedia_conf_destroy(conf);
        return status;
    }
    /* sound device (from 0 SLOT) to 0 idx, others to next idx */
    pj_assert(conf->upper_bound_reg == 1);
    pj_assert(conf->active_listener[0] == 0);

    /* Create mutex. */
    status = pj_mutex_create_recursive(pool, "conf", &conf->mutex);
    if (status != PJ_SUCCESS) {
        pjmedia_conf_destroy(conf);
        return status;
    }

    /* If sound device was created, connect sound device to the
     * master port.
     */
    if (conf->snd_dev_port) {
        status = pjmedia_snd_port_connect( conf->snd_dev_port, 
                                           conf->master_port );
        if (status != PJ_SUCCESS) {
            pjmedia_conf_destroy(conf);
            return status;
        }
    }


    /* Allocate synchronized operation queues */
    conf->op_queue = PJ_POOL_ZALLOC_T(pool, op_entry);
    conf->op_queue_free = PJ_POOL_ZALLOC_T(pool, op_entry);
    if (!conf->op_queue || !conf->op_queue_free) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Create failed in create queues"));
        pjmedia_conf_destroy(conf);
        return PJ_ENOMEM;
    }
    pj_list_init(conf->op_queue);
    pj_list_init(conf->op_queue_free);

    status = pj_atomic_slist_create(pool, &conf->unused_slots);
    if (status != PJ_SUCCESS) {
        pjmedia_conf_destroy(conf);
        return status;
    }
    conf->free_port_slots = pj_atomic_slist_calloc(pool, conf->max_ports, sizeof(port_slot));
    PJ_ASSERT_ON_FAIL(conf->free_port_slots, {pjmedia_conf_destroy(conf); return PJ_ENOMEM;});
    i = conf->max_ports;
    while (i--) {               /* prepare unused slots to later reservation, reverse order due to FILO */
        if (!conf->ports[i]) {  /* If sound device was created, skip it's slot */
            status = conf_release_port(conf, i);
            if (status != PJ_SUCCESS) {
                pjmedia_conf_destroy(conf);
                return status;
            }
        }
    }

    status = pj_atomic_create(conf->pool, 0, &conf->active_ports_idx);
    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, goto on_return);

    status = pj_atomic_create(conf->pool, 0, &conf->listener_counter);
    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, goto on_return);

    if (conf->is_parallel) {
        status = thread_pool_start(conf);
        PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, goto on_return);
    }

    /* Done */
    *p_conf = conf;

    return PJ_SUCCESS;

on_return:
    pjmedia_conf_destroy(conf);

    return status;

}


/*
 * Pause sound device.
 */
static pj_status_t pause_sound( pjmedia_conf *conf )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(conf);
    return PJ_SUCCESS;
}

/*
 * Resume sound device.
 */
static pj_status_t resume_sound( pjmedia_conf *conf )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(conf);
    return PJ_SUCCESS;
}


/**
 * Destroy conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_destroy( pjmedia_conf *conf )
{
    unsigned i;
    pj_int32_t rc;

    PJ_ASSERT_RETURN(conf != NULL, PJ_EINVAL);

    PJ_LOG(5, (THIS_FILE, "Audio conference bridge destroy requested"));

    pj_log_push_indent();

    /* Signal threads to quit */
    conf->quit_flag = PJ_TRUE;

    /* all threads have reached the barrier and the conference bridge thread no longer exists.
     * Should be a very short waiting.
     *
     * If we couldn't create all the threads from the pool, we shouldn't get close to the barrier.
     */
    if (conf->running && conf->active_thread) {
        TRACE_EX((THIS_FILE, "%s: timestamp=%llu, thread at barrier. quit_flag = %d.",
                  pj_thread_get_name(pj_thread_this()),
                  conf->frame ? conf->frame->timestamp.u64 : (pj_uint64_t)-1,
                  conf->quit_flag));

        rc = pj_barrier_wait(conf->active_thread, PJ_BARRIER_FLAGS_NO_DELETE | PJ_BARRIER_FLAGS_SPIN_ONLY);
        pj_assert(rc == PJ_TRUE || rc == PJ_FALSE);

        TRACE_EX((THIS_FILE, "%s: timestamp=%llu, barrier passed with return = %d. quit_flag = %d.",
                  pj_thread_get_name(pj_thread_this()),
                  conf->frame ? conf->frame->timestamp.u64 : (pj_uint64_t)-1,
                  rc, conf->quit_flag));
        PJ_UNUSED_ARG(rc);
    }

    /* Destroy thread pool */
    if (conf->pool_threads) {
        pj_thread_t **threads = conf->pool_threads;
        pj_thread_t **end = threads + (conf->threads-1);
        while (threads < end) {
            if (*threads) {
                pj_thread_join(*threads);
                pj_thread_destroy(*threads);
                *threads = NULL;
            }
            ++threads;
        }
    }
    if (conf->active_thread)
        pj_barrier_destroy(conf->active_thread);
    if (conf->barrier)
        pj_barrier_destroy(conf->barrier);

    /* active worker thread counter*/
    if (conf->active_thread_cnt)
        CONF_CHECK_SUCCESS(pj_atomic_destroy(conf->active_thread_cnt), (void)0);
    /* exit barrier event */
    if (conf->barrier_evt)
        CONF_CHECK_SUCCESS(pj_event_destroy(conf->barrier_evt), (void)0);


    /* Destroy sound device port. */
    if (conf->snd_dev_port) {
        pjmedia_snd_port_destroy(conf->snd_dev_port);
        conf->snd_dev_port = NULL;
    }

    /* Flush any pending operation (connect, disconnect, etc) */
    if (conf->op_queue)
        handle_op_queue(conf);

    /* Remove all ports (may destroy them too). */
    for (i=0; i<conf->max_ports; ++i) {
        if (conf->ports[i]) {
            pj_status_t status;
            pjmedia_conf_op_param oprm = {0};
            oprm.remove_port.port = i;
            status = op_remove_port(conf, &oprm);
            if (conf->cb) {
                pjmedia_conf_op_info op_info = { 0 };

                pj_log_push_indent();
                op_info.op_type = PJMEDIA_CONF_OP_REMOVE_PORT;
                op_info.status = status;
                op_info.op_param = oprm;

                (*conf->cb)(&op_info);
                pj_log_pop_indent();
            }
        }
    }

    if (conf->cb) {
        conf->cb = NULL;
    }

    /* Destroy slist */
    if (conf->unused_slots)
        pj_atomic_slist_destroy(conf->unused_slots);

    /* Destroy mutex */
    if (conf->mutex)
        pj_mutex_destroy(conf->mutex);

    /* Destroy atomic */
    if (conf->active_ports_idx)
        CONF_CHECK_SUCCESS(pj_atomic_destroy(conf->active_ports_idx), (void)0);
    if (conf->listener_counter)
        CONF_CHECK_SUCCESS(pj_atomic_destroy(conf->listener_counter), (void)0);

    /* Destroy pool */
    if (conf->pool)
        pj_pool_safe_release(&conf->pool);

    pj_log_pop_indent();

    PJ_LOG(4, (THIS_FILE, "Audio conference bridge destroyed"));

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_conf_set_op_cb(pjmedia_conf *conf,
                                           pjmedia_conf_op_cb cb)
{
    PJ_ASSERT_RETURN(conf, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);
    conf->cb = cb;
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

/*
 * Destroy the master port (will destroy the conference)
 */
static pj_status_t destroy_port(pjmedia_port *this_port)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    return pjmedia_conf_destroy(conf);
}

#if !DEPRECATED_FOR_TICKET_2234
static pj_status_t destroy_port_pasv(pjmedia_port *this_port) {
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    struct conf_port *port = conf->ports[this_port->port_data.ldata];
    pj_status_t status;

    status = pjmedia_delay_buf_destroy(port->delay_buf);
    if (status == PJ_SUCCESS)
        port->delay_buf = NULL;

    return status;
}
#endif

/*
 * Get port zero interface.
 */
PJ_DEF(pjmedia_port*) pjmedia_conf_get_master_port(pjmedia_conf *conf)
{
    /* Sanity check. */
    PJ_ASSERT_RETURN(conf != NULL, NULL);

    /* Can only return port interface when PJMEDIA_CONF_NO_DEVICE was
     * present in the option.
     */
    PJ_ASSERT_RETURN((conf->options & PJMEDIA_CONF_NO_DEVICE) != 0, NULL);
    
    return conf->master_port;
}


/*
 * Set master port name.
 */
PJ_DEF(pj_status_t) pjmedia_conf_set_port0_name(pjmedia_conf *conf,
                                                const pj_str_t *name)
{
    pj_size_t len;

    /* Sanity check. */
    PJ_ASSERT_RETURN(conf != NULL && name != NULL, PJ_EINVAL);

    len = name->slen;
    if (len > sizeof(conf->master_name_buf))
        len = sizeof(conf->master_name_buf);
    
    if (len > 0) pj_memcpy(conf->master_name_buf, name->ptr, len);

    conf->ports[0]->name.ptr = conf->master_name_buf;
    conf->ports[0]->name.slen = len;

    if (conf->master_port)
        conf->master_port->info.name = conf->ports[0]->name;

    return PJ_SUCCESS;
}

/*
 * Add stream port to the conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_port( pjmedia_conf *conf,
                                           pj_pool_t *pool,
                                           pjmedia_port *strm_port,
                                           const pj_str_t *port_name,
                                           unsigned *p_port )
{
    struct conf_port *conf_port;
    SLOT_TYPE index = INVALID_SLOT;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(conf && pool && strm_port, PJ_EINVAL);

    pj_log_push_indent();

    /* If port_name is not specified, use the port's name */
    if (!port_name)
        port_name = &strm_port->info.name;

    /* For this version of PJMEDIA, channel(s) number MUST be:
     * - same between port & conference bridge.
     * - monochannel on port or conference bridge.
     */
    if (PJMEDIA_PIA_CCNT(&strm_port->info) != conf->channel_count &&
        (PJMEDIA_PIA_CCNT(&strm_port->info) != 1 &&
         conf->channel_count != 1))
    {
        pj_assert(!"Number of channels mismatch");
        status = PJMEDIA_ENCCHANNEL;
        goto on_return;
    }

    /* Find empty port in the conference bridge. */
    index = conf_reserve_port(conf);
    if (index == INVALID_SLOT) {
        PJ_PERROR(3,(THIS_FILE, PJ_ETOOMANY, "Add port %s failed",
                     port_name->ptr));
        //pj_assert( !"Too many ports" );
        status = PJ_ETOOMANY;
        goto on_return;
    }
    pj_assert(index < conf->max_ports && conf->ports[index] == NULL);

    /* Create conf port structure. */
    status = create_conf_port(pool, conf, strm_port, port_name, &conf_port);
    if (status != PJ_SUCCESS)
        goto on_return;

    pj_assert(conf_port != NULL && !is_port_connected(conf_port));

#ifdef CONF_DEBUG_EX
    conf_port->slot = index;
#endif //CONF_DEBUG_EX

    conf_port->is_new = PJ_TRUE;

    /* Put the port to the reserved slot. */
    conf->ports[index] = conf_port;/*pointer assignment is processor level atomic*/

    /* Put the port, but don't add port counter yet */
    //conf->port_cnt++;

    pj_mutex_lock( conf->mutex );
    /* Queue the operation */
    op_entry *ope;
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_ADD_PORT;
        ope->param.add_port.port = index;
        pj_list_push_back(conf->op_queue, ope);
        pj_mutex_unlock(conf->mutex);
        PJ_LOG(4,(THIS_FILE, "Add port %d (%.*s) queued",
                             index, (int)port_name->slen, port_name->ptr));
    } else {
        pj_mutex_unlock(conf->mutex);
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Done. */
    if (p_port) {
        *p_port = index;
    }

on_return:
    if (status != PJ_SUCCESS) {
        if (index != INVALID_SLOT) {
            conf->ports[index] = NULL;
            conf_release_port( conf, index );
        }
    }

    pj_log_pop_indent();

    return status;
}

/*
 * Find empty port in the conference bridge.
 */
static SLOT_TYPE conf_reserve_port(pjmedia_conf *conf)
{
    port_slot *pslot;
    pslot = pj_atomic_slist_pop(conf->unused_slots);
    if (!pslot)
        return INVALID_SLOT;

    SLOT_TYPE slot = pslot - conf->free_port_slots;
    pj_assert( slot < conf->max_ports && conf->ports[slot] == NULL );
    return slot;
}

/*
 * Return conf_port slot to unused slots cache.
 */
static pj_status_t conf_release_port(pjmedia_conf *conf, SLOT_TYPE slot)
{
    /* Check arguments */
    PJ_ASSERT_RETURN( conf && slot < conf->max_ports, PJ_EINVAL );
    PJ_ASSERT_RETURN( conf->ports[slot] == NULL, PJ_EINVALIDOP );
    return pj_atomic_slist_push( conf->unused_slots, conf->free_port_slots + slot );
}


static pj_status_t op_add_port(pjmedia_conf *conf,
                               const pjmedia_conf_op_param *prm){
    unsigned port = prm->add_port.port;
    struct conf_port *cport = conf->ports[port];

    /* Port must be valid and flagged as new. */
    if (!cport || !cport->is_new)
        return PJ_EINVAL;

    /* Activate newly added port */
    cport->is_new = PJ_FALSE;

    pj_assert(conf->upper_bound_reg < conf->max_ports);
    /* skip all player only ports
     * In an IVR scenario this can reduce the size of active_listener[] 
     * by up to two times
     */
    if (cport->port && cport->port->put_frame) {
        conf->active_listener[conf->upper_bound_reg++] = port;
        cport->is_active_listener = PJ_TRUE;
    }

    ++conf->port_cnt;

    PJ_LOG(4,(THIS_FILE, "Added port %d (%.*s), port count=%d",
              port, (int)cport->name.slen, cport->name.ptr, conf->port_cnt));

    pj_assert(conf->port_cnt >= conf->upper_bound_reg);

    return PJ_SUCCESS;
}


#if !DEPRECATED_FOR_TICKET_2234
/*
 * Add passive port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_passive_port( pjmedia_conf *conf,
                                                   pj_pool_t *pool,
                                                   const pj_str_t *name,
                                                   unsigned clock_rate,
                                                   unsigned channel_count,
                                                   unsigned samples_per_frame,
                                                   unsigned bits_per_sample,
                                                   unsigned options,
                                                   unsigned *p_slot,
                                                   pjmedia_port **p_port )
{
    struct conf_port *conf_port = NULL;
    pjmedia_port *port = NULL;
    SLOT_TYPE index = INVALID_SLOT;
    pj_str_t tmp;
    pj_status_t status;

    PJ_LOG(1, (THIS_FILE, "This API has been deprecated since 1.3 and will "
                          "be removed in the future release!"));

    PJ_ASSERT_RETURN(conf && pool, PJ_EINVAL);

    /* For this version of PJMEDIA, channel(s) number MUST be:
     * - same between port & conference bridge.
     * - monochannel on port or conference bridge.
     */
    if (channel_count != conf->channel_count && 
        (channel_count != 1 && conf->channel_count != 1)) 
    {
        pj_assert(!"Number of channels mismatch");
        return PJMEDIA_ENCCHANNEL;
    }

    /* For this version, options must be zero */
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);
    PJ_UNUSED_ARG(options);

    /* Find empty port in the conference bridge. */
    index = conf_reserve_port(conf);
    if (index == INVALID_SLOT) {
        pj_assert(!"Too many ports");
        return PJ_ETOOMANY;
    }
    pj_assert(index < conf->max_ports && conf->ports[index] == NULL);

    if (name == NULL) {
        name = &tmp;

        tmp.ptr = (char*) pj_pool_alloc(pool, 32);
        tmp.slen = pj_ansi_snprintf(tmp.ptr, 32, "ConfPort#%u", index);
    }

    /* Create and initialize the media port structure. */
    port = PJ_POOL_ZALLOC_T(pool, pjmedia_port);
    PJ_ASSERT_RETURN(port, PJ_ENOMEM);
    
    pjmedia_port_info_init(&port->info, name, SIGNATURE_PORT,
                           clock_rate, channel_count, bits_per_sample,
                           samples_per_frame);

    port->port_data.pdata = conf;
    port->port_data.ldata = index;

    port->get_frame = &get_frame_pasv;
    port->put_frame = &put_frame;
    port->on_destroy = &destroy_port_pasv;

    
    /* Create conf port structure. */
    status = create_pasv_port(conf, pool, name, port, &conf_port);
    if (status != PJ_SUCCESS) {
        return status;
    }

    conf_port->is_new = PJ_TRUE;

    /* Put the port to the reserved slot. */
    conf->ports[index] = conf_port;/*pointer assignment is processor level atomic*/

    /* Put the port, but don't add port counter yet */
    //conf->port_cnt++;

    pj_mutex_lock( conf->mutex );
    /* Queue the operation */
    op_entry *ope;
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_ADD_PORT;
        ope->param.add_port.port = index;
        pj_list_push_back(conf->op_queue, ope);
        pj_mutex_unlock(conf->mutex);
        PJ_LOG(4,(THIS_FILE, "Add port %d (%.*s) queued",
                 index, (int)name->slen, name->ptr));
    } else {
        pj_mutex_unlock(conf->mutex);
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Done. */
    if (p_slot)
        *p_slot = index;
    if (p_port)
        *p_port = port;

    return PJ_SUCCESS;

on_return:
    if (status != PJ_SUCCESS) {
        if (conf_port) {
            /* Decrease conf port ref count */
            if (conf_port->port && conf_port->port->grp_lock)
                pj_grp_lock_dec_ref(conf_port->port->grp_lock);
            else
                destroy_conf_port(conf_port);
        }

        if (index != INVALID_SLOT) {
            conf->ports[index] = NULL;
            conf_release_port( conf, index );
        }
    }

    return status;

}
#endif



/*
 * Change TX and RX settings for the port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_configure_port( pjmedia_conf *conf,
                                                  unsigned slot,
                                                  pjmedia_port_op tx,
                                                  pjmedia_port_op rx)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    conf_port = conf->ports[slot];

    if (tx != PJMEDIA_PORT_NO_CHANGE)
        conf_port->tx_setting = tx;

    if (rx != PJMEDIA_PORT_NO_CHANGE)
        conf_port->rx_setting = rx;

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Connect port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_connect_port( pjmedia_conf *conf,
                                               unsigned src_slot,
                                               unsigned sink_slot,
                                               int adj_level )
{
    struct conf_port *src_port, *dst_port;
    pj_bool_t start_sound = PJ_FALSE;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports &&
                     sink_slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    pj_log_push_indent();

    PJ_LOG(5,(THIS_FILE, "Connect ports %d->%d requested",
                         src_slot, sink_slot));

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];
    if (!src_port || !dst_port) {
        status = PJ_EINVAL;
        goto on_return;
    }

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_CONNECT_PORTS;
        ope->param.connect_ports.src = src_slot;
        ope->param.connect_ports.sink = sink_slot;
        ope->param.connect_ports.adj_level = adj_level;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Connect ports %d->%d queued",
                             src_slot, sink_slot));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* This is first connection, start clock */
    if (conf->connect_cnt == 0)
        start_sound = 1;

on_return:
    pj_mutex_unlock(conf->mutex);

    /* Sound device must be started without mutex, otherwise the
     * sound thread will deadlock (?)
     */
    if (start_sound)
        resume_sound(conf);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Connect ports %d->%d failed",
                     src_slot, sink_slot));
    }

    pj_log_pop_indent();

    return status;
}

static pj_status_t op_connect_ports(pjmedia_conf *conf, 
                                    const pjmedia_conf_op_param *prm){
    unsigned src_slot, sink_slot;
    struct conf_port *src_port, *dst_port;
    unsigned i;

    /* Ports must be valid. */
    src_slot = prm->connect_ports.src;
    sink_slot = prm->connect_ports.sink;
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];

    if (!src_port || !dst_port) {
        PJ_PERROR(3,(THIS_FILE, PJ_EINVAL,
                     "Failed connecting %d->%d, make sure ports are valid",
                     src_slot, sink_slot));
        return PJ_EINVAL;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot) {
            PJ_LOG(3,(THIS_FILE, "Ports connection %d->%d already exists",
                      src_slot, sink_slot));
            return PJ_EEXISTS;
        }
    }

    src_port->listener_slots[src_port->listener_cnt] = sink_slot;

    /* Set normalized adjustment level. */
    src_port->listener_adj_level[src_port->listener_cnt] =
                                prm->connect_ports.adj_level + NORMAL_LEVEL;

    ++conf->connect_cnt;
    ++dst_port->transmitter_cnt;

    if (!src_port->listener_cnt++) {
        pj_assert(conf->upper_bound < conf->max_ports);
        conf->active_ports[conf->upper_bound++] = src_slot;
    }

    PJ_LOG(4,(THIS_FILE, "Port %d (%.*s) transmitting to port %d (%.*s)",
              src_slot,
              (int)src_port->name.slen,
              src_port->name.ptr,
              sink_slot,
              (int)dst_port->name.slen,
              dst_port->name.ptr));

    return PJ_SUCCESS;
}

/*
 * Disconnect port
 */
PJ_DEF(pj_status_t) pjmedia_conf_disconnect_port( pjmedia_conf *conf,
                                                  unsigned src_slot,
                                                  unsigned sink_slot )
{
    struct conf_port *src_port, *dst_port;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports &&
                     sink_slot<conf->max_ports, PJ_EINVAL);

    pj_log_push_indent();

    PJ_LOG(5,(THIS_FILE, "Disconnect ports %d->%d requested",
                         src_slot, sink_slot));

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];
    if (!src_port || !dst_port) {
        status = PJ_EINVAL;
        goto on_return;
    }

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_DISCONNECT_PORTS;
        ope->param.disconnect_ports.src = src_slot;
        ope->param.disconnect_ports.sink = sink_slot;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Disconnect ports %d->%d queued",
                             src_slot, sink_slot));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

on_return:
    pj_mutex_unlock(conf->mutex);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Disconnect ports %d->%d failed",
                     src_slot, sink_slot));
    }

    pj_log_pop_indent();

    return status;
}

static pj_status_t op_disconnect_ports(pjmedia_conf *conf,
                                       const pjmedia_conf_op_param *prm)
{
    SLOT_TYPE src_slot, sink_slot;
    struct conf_port *src_port = NULL, *dst_port = NULL;
    pj_uint32_t idx;

    /* Ports must be valid. */
    src_slot = prm->disconnect_ports.src;
    sink_slot = prm->disconnect_ports.sink;

    if (src_slot != INVALID_SLOT)
        src_port = conf->ports[src_slot];
    if (sink_slot != INVALID_SLOT)
        dst_port = conf->ports[sink_slot];

    /* Disconnect source -> sink */
    if (src_port && dst_port) {
        /* Check if connection has been made */
        for (idx=0; idx<src_port->listener_cnt; ++idx) {
            if (src_port->listener_slots[idx] == sink_slot)
                break;
        }
        if (idx == src_port->listener_cnt) {
            PJ_LOG(3,(THIS_FILE, "Ports connection %d->%d does not exist",
                      src_slot, sink_slot));
            return PJ_EINVAL;
        }

        pj_assert(src_port->listener_cnt > 0 &&
                  src_port->listener_cnt <= conf->max_ports);
        pj_assert(dst_port->transmitter_cnt > 0 &&
                  dst_port->transmitter_cnt <= conf->max_ports);
        pj_array_erase(src_port->listener_slots, sizeof(SLOT_TYPE),
                       src_port->listener_cnt, idx);
        pj_array_erase(src_port->listener_adj_level, sizeof(unsigned),
                       src_port->listener_cnt, idx);
        --conf->connect_cnt;
        --dst_port->transmitter_cnt;

        if (!--src_port->listener_cnt) {
            pj_assert(conf->upper_bound);
            for(idx=0; idx<conf->upper_bound; ++idx) {
                if (conf->active_ports[idx] == src_slot) {
                    pj_array_erase(conf->active_ports, sizeof(SLOT_TYPE),
                                   conf->upper_bound--, idx);

                    src_port->rx_level = 0;
                    break;
                }
            }
        }

        PJ_LOG(4,(THIS_FILE,
                  "Port %d (%.*s) stop transmitting to port %d (%.*s)",
                  src_slot,
                  (int)src_port->name.slen,
                  src_port->name.ptr,
                  sink_slot,
                  (int)dst_port->name.slen,
                  dst_port->name.ptr));

        /* if source port is passive port and has no listener,
         * reset delaybuf.
         */
        if (src_port->delay_buf && src_port->listener_cnt == 0)
            pjmedia_delay_buf_reset(src_port->delay_buf);

    /* Disconnect multiple conn: any -> sink */
    } else if (dst_port) {
        /* Remove this port from transmit array of other ports. */
        if (dst_port->transmitter_cnt) {
            PJ_LOG(4,(THIS_FILE,
                      "Stop any transmission to port %d (%.*s)",
                      sink_slot,
                      (int)dst_port->name.slen,
                      dst_port->name.ptr));

            /* We need to iterate backwards because upper_bound could 
             * potentially decrease due to src_slot being removed from 
             * active_ports[]
             */
            idx = conf->upper_bound;
            while(idx--) {
                pj_uint32_t j;

                src_slot = conf->active_ports[idx];
                src_port = conf->ports[src_slot];
                pj_assert(src_port && src_port->listener_cnt);

                /* We need to iterate backwards since the listener count
                 * can potentially decrease.
                 */
                j = src_port->listener_cnt; 
                while(j--) {
                    if (src_port->listener_slots[j] == sink_slot) {
                        pj_status_t status;
                        pjmedia_conf_op_param op_prm = {0};

                        op_prm.disconnect_ports.src = src_slot;
                        op_prm.disconnect_ports.sink = sink_slot;
                        status = op_disconnect_ports(conf, &op_prm);
                        if (status != PJ_SUCCESS) {
                            PJ_PERROR(4, (THIS_FILE, status,
                                "Fail to stop transmission from port "
                                "%u to port %u", 
                                src_slot, sink_slot));
                        }
                        break;
                    }
                }
            }
            pj_assert( !dst_port->transmitter_cnt );
        }

    /* Disconnect multiple conn: source -> any */
    } else if (src_port) {
        PJ_LOG(4,(THIS_FILE,
                  "Stop any transmission from port %d (%.*s)",
                  src_slot,
                  (int)src_port->name.slen,
                  src_port->name.ptr));

        /* We need to iterate backwards since the listener count
         * will keep decreasing.
         */
        idx = src_port->listener_cnt;
        while(idx--) {
            pj_status_t status;
            pjmedia_conf_op_param op_prm = {0};

            sink_slot = src_port->listener_slots[idx];
            op_prm.disconnect_ports.src = src_slot;
            op_prm.disconnect_ports.sink = sink_slot;
            status = op_disconnect_ports(conf, &op_prm);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4, (THIS_FILE, status,
                              "Fail to stop transmission from port "
                              "%d to port %d",
                              src_slot, sink_slot));
            }
        }
        pj_assert( !src_port->listener_cnt );

    /* Invalid ports */
    } else {
        PJ_ASSERT_RETURN(!"Invalid ports specified in conf disconnect", 
                         PJ_EINVAL);
    }

    /* Pause sound dev when there is no connection, the pause should be done
     * outside mutex to avoid possible deadlock.
     * Note that currently this is done with mutex, it is safe because
     * pause_sound() is a no-op (just maintaining old code).
     */
    if (conf->connect_cnt == 0) {
        pause_sound(conf);
    }
    return PJ_SUCCESS;
}

/*
 * Disconnect port from all sources
 */
PJ_DEF(pj_status_t)
pjmedia_conf_disconnect_port_from_sources( pjmedia_conf *conf,
                                           unsigned sink_slot)
{
    struct conf_port *dst_port;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && sink_slot<conf->max_ports, PJ_EINVAL);

    pj_log_push_indent();
    PJ_LOG(5,(THIS_FILE, "Disconnect ports any->%d requested",
                         sink_slot));

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    dst_port = conf->ports[sink_slot];
    if (!dst_port) {
        status = PJ_EINVAL;
        goto on_return;
    }

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_DISCONNECT_PORTS;
        ope->param.disconnect_ports.src = INVALID_SLOT;
        ope->param.disconnect_ports.sink = sink_slot;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Disconnect ports any->%d queued", sink_slot));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

on_return:
    pj_mutex_unlock(conf->mutex);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Disconnect ports any->%d failed",
                     sink_slot));
    }

    pj_log_pop_indent();

    return status;
}


/*
 * Disconnect port from all sinks
 */
PJ_DEF(pj_status_t)
pjmedia_conf_disconnect_port_from_sinks( pjmedia_conf *conf,
                                         unsigned src_slot)
{
    struct conf_port *src_port;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports, PJ_EINVAL);

    pj_log_push_indent();

    PJ_LOG(5,(THIS_FILE, "Disconnect ports %d->any requested",
                         src_slot));

    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    src_port = conf->ports[src_slot];
    if (!src_port) {
        status = PJ_EINVAL;
        goto on_return;
    }

    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_DISCONNECT_PORTS;
        ope->param.disconnect_ports.src = src_slot;
        ope->param.disconnect_ports.sink = INVALID_SLOT;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Disconnect ports %d->any queued", src_slot));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

on_return:
    pj_mutex_unlock(conf->mutex);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Disconnect ports %d->any failed",
                     src_slot));
    }

    pj_log_pop_indent();

    return status;
}


/*
 * Get number of ports currently registered to the conference bridge.
 */
PJ_DEF(unsigned) pjmedia_conf_get_port_count(pjmedia_conf *conf)
{
    return conf->port_cnt;
}

/*
 * Get total number of ports connections currently set up in the bridge.
 */
PJ_DEF(unsigned) pjmedia_conf_get_connect_count(pjmedia_conf *conf)
{
    return conf->connect_cnt;
}


/*
 * Remove the specified port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_remove_port( pjmedia_conf *conf,
                                              unsigned port )
{
    struct conf_port *conf_port;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    pj_log_push_indent();

    PJ_LOG(5,(THIS_FILE, "Remove port %d requested", port));

    PJ_ASSERT_RETURN(conf && port < conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[port];
    if (conf_port == NULL) {
        status = PJ_EINVAL;
        goto on_return;
    }

    /* If port is new, remove it synchronously */
    if (conf_port->is_new) {
        pj_bool_t found = PJ_FALSE;

        /* Find & cancel the add-op.
         * Also cancel all following ops involving the slot.
         * Note that after removed, the slot may be reused by another port
         * so if not cancelled, those following ops may be applied to the
         * wrong port.
         */
        ope = conf->op_queue->next;
        while (ope != conf->op_queue) {
            op_entry* cancel_op;

            cancel_op = NULL;
            if (ope->type == PJMEDIA_CONF_OP_ADD_PORT && 
                ope->param.add_port.port == port)
            {
                found = PJ_TRUE;
                cancel_op = ope;
            } else if (found && ope->type == PJMEDIA_CONF_OP_CONNECT_PORTS &&
                       (ope->param.connect_ports.src == port ||
                        ope->param.connect_ports.sink == port))
            {
                cancel_op = ope;
            } else if (found && ope->type == PJMEDIA_CONF_OP_DISCONNECT_PORTS &&
                       (ope->param.disconnect_ports.src == port ||
                        ope->param.disconnect_ports.sink == port))
            {
                cancel_op = ope;
            }

            ope = ope->next;

            /* Cancel op */
            if (cancel_op) {
                pjmedia_conf_op_info op_info = { 0 };

                op_info.op_type = cancel_op->type;
                op_info.op_param = cancel_op->param;

                pj_list_erase(cancel_op);
                cancel_op->type = PJMEDIA_CONF_OP_UNKNOWN;
                pj_list_push_back(conf->op_queue_free, cancel_op);

                if (conf->cb) {
                    pj_log_push_indent();
                    op_info.status = PJ_ECANCELLED;
                    (*conf->cb)(&op_info);
                    pj_log_pop_indent();
                }
            }
        }

        /* If the add-op is not found, it may be being executed,
         * do not remove it synchronously to avoid race condition.
         */
        if (found) {
            pjmedia_conf_op_param prm;

            /* Release mutex to avoid deadlock */
            pj_mutex_unlock(conf->mutex);

            /* Remove it */
            prm.remove_port.port = port;
            status = op_remove_port(conf, &prm);

            if (conf->cb) {
                pjmedia_conf_op_info op_info = { 0 };

                pj_log_push_indent();
                op_info.op_type = PJMEDIA_CONF_OP_REMOVE_PORT;
                op_info.status = status;
                op_info.op_param = prm;

                (*conf->cb)(&op_info);
                pj_log_pop_indent();
            }

            pj_log_pop_indent();
            return PJ_SUCCESS;
        }
    }

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_REMOVE_PORT;
        ope->param.remove_port.port = port;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Remove port %d queued", port));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

on_return:
    pj_mutex_unlock(conf->mutex);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Remove port %d failed", port));
    }

    pj_log_pop_indent();

    return status;
}


static pj_status_t op_remove_port(pjmedia_conf *conf,
                                  const pjmedia_conf_op_param *prm)
{
    unsigned port = prm->remove_port.port;
    struct conf_port *conf_port;
    pjmedia_conf_op_param op_prm;
    pj_status_t status;

    /* Port must be valid. */
    conf_port = conf->ports[port];
    if (conf_port == NULL) {
        PJ_PERROR(3, (THIS_FILE, PJ_ENOTFOUND, "Remove port failed"));
        return PJ_EINVAL;
    }

    conf_port->tx_setting = PJMEDIA_PORT_DISABLE;
    conf_port->rx_setting = PJMEDIA_PORT_DISABLE;

    /* disconnect port from all sources which are transmitting to it */
    pj_bzero(&op_prm, sizeof(op_prm));
    op_prm.disconnect_ports.src = INVALID_SLOT;
    op_prm.disconnect_ports.sink = port;
    status = op_disconnect_ports(conf, &op_prm);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4, (THIS_FILE, status,
                      "Fail to stop transmission from any->%d", port));
    }

    /* disconnect port from all sinks to which it is transmitting to */
    pj_bzero(&op_prm, sizeof(op_prm));
    op_prm.disconnect_ports.src = port;
    op_prm.disconnect_ports.sink = INVALID_SLOT;
    status = op_disconnect_ports(conf, &op_prm);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4, (THIS_FILE, status,
                      "Fail to stop transmission from %d->any", port));
    }

    pj_assert( !is_port_connected( conf_port ) );
    /* Remove the port. */
    //pj_mutex_lock(conf->mutex);
    conf->ports[port] = NULL;
    //pj_mutex_unlock(conf->mutex);

    if (!conf_port->is_new)
        --conf->port_cnt;

    if (conf_port->is_active_listener) {
        pj_uint32_t idx;
        pj_assert(conf->upper_bound_reg);
        for (idx = 0; idx < conf->upper_bound_reg; ++idx) {
            if (conf->active_listener[idx] == port) {
                pj_array_erase(conf->active_listener, sizeof(SLOT_TYPE),
                    conf->upper_bound_reg, idx);
                --conf->upper_bound_reg;
                break;
            }
        }
    }

    PJ_LOG(4, (THIS_FILE, "Removed port %d (%.*s), port count=%d",
               port, (int)conf_port->name.slen, conf_port->name.ptr,
               conf->port_cnt));

    pj_assert(conf->port_cnt >= conf->upper_bound_reg);

    /* Return conf_port slot to unused slots cache. */
    conf_release_port( conf, port );

    /* Decrease conf port ref count */
    if (conf_port->port && conf_port->port->grp_lock)
        pj_grp_lock_dec_ref(conf_port->port->grp_lock);
    else
        destroy_conf_port(conf_port);

    return PJ_SUCCESS;
}

static void destroy_conf_port( struct conf_port *conf_port )
{
    pj_assert( conf_port );

    TRACE_EX( (THIS_FILE, "%s: destroy_conf_port %p (%.*s, %d) transmitter_cnt=%d, listener_cnt=%d",
                pj_thread_get_name( pj_thread_this() ),
                conf_port,
                (int)conf_port->name.slen,
                conf_port->name.ptr,
                conf_port->slot,
                conf_port->transmitter_cnt,
                conf_port->listener_cnt) );

    /* Destroy resample if this conf port has it. */
    if (conf_port->rx_resample) {
        pjmedia_resample_destroy(conf_port->rx_resample);
        conf_port->rx_resample = NULL;
    }
    if (conf_port->tx_resample) {
        pjmedia_resample_destroy(conf_port->tx_resample);
        conf_port->tx_resample = NULL;
    }

    /* Destroy pjmedia port if this conf port is passive port,
     * i.e: has delay buf.
     */
    if (conf_port->delay_buf) {
        pjmedia_delay_buf_destroy(conf_port->delay_buf);
        conf_port->delay_buf = NULL;

        if (conf_port->port) {
            pjmedia_port_destroy(conf_port->port);
            conf_port->port = NULL;
        }
    }

    if (conf_port->tx_lock != NULL)
        pj_lock_destroy(conf_port->tx_lock);

    pj_pool_safe_release(&conf_port->pool);
}


/*
 * Enum ports.
 */
PJ_DEF(pj_status_t) pjmedia_conf_enum_ports( pjmedia_conf *conf,
                                             unsigned ports[],
                                             unsigned *p_count )
{
    unsigned i, count=0;

    PJ_ASSERT_RETURN(conf && p_count && ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    for (i=0; i<conf->max_ports && count<*p_count; ++i) {
        if (!conf->ports[i])
            continue;

        ports[count++] = i;
    }

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    *p_count = count;
    return PJ_SUCCESS;
}

/*
 * Get port info
 */
PJ_DEF(pj_status_t) pjmedia_conf_get_port_info( pjmedia_conf *conf,
                                                unsigned slot,
                                                pjmedia_conf_port_info *info)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    info->slot = slot;
    info->name = conf_port->name;
    if (conf_port->port) {
        pjmedia_format_copy(&info->format, &conf_port->port->info.fmt);
    } else {
        pj_bzero(&info->format, sizeof(info->format));
        info->format.id = (pj_uint32_t)PJMEDIA_FORMAT_INVALID;
    }
    info->tx_setting = conf_port->tx_setting;
    info->rx_setting = conf_port->rx_setting;
    info->listener_cnt = conf_port->listener_cnt;
    info->listener_slots = conf_port->listener_slots;
    info->transmitter_cnt = conf_port->transmitter_cnt;
    info->clock_rate = conf_port->sampling_rate;
    info->channel_count = conf_port->channel_count;
    info->samples_per_frame = conf_port->samples_per_frame;
    info->bits_per_sample = conf->bits_per_sample;
    info->tx_adj_level = conf_port->tx_adj_level - NORMAL_LEVEL;
    info->rx_adj_level = conf_port->rx_adj_level - NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_conf_get_ports_info(pjmedia_conf *conf,
                                                unsigned *size,
                                                pjmedia_conf_port_info info[])
{
    unsigned i, count=0;

    PJ_ASSERT_RETURN(conf && size && info, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    for (i=0; i<conf->max_ports && count<*size; ++i) {
        if (!conf->ports[i])
            continue;

        pjmedia_conf_get_port_info(conf, i, &info[count]);
        ++count;
    }

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    *size = count;
    return PJ_SUCCESS;
}


/*
 * Get signal level.
 */
PJ_DEF(pj_status_t) pjmedia_conf_get_signal_level( pjmedia_conf *conf,
                                                   unsigned slot,
                                                   unsigned *tx_level,
                                                   unsigned *rx_level)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    if (tx_level != NULL) {
        *tx_level = conf_port->tx_level;
    }

    if (rx_level != NULL) 
        *rx_level = conf_port->rx_level;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Adjust RX level of individual port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_rx_level( pjmedia_conf *conf,
                                                  unsigned slot,
                                                  int adj_level )
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127, at your own risk: 
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    /* Set normalized adjustment level. */
    conf_port->rx_adj_level = adj_level + NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Adjust TX level of individual port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_tx_level( pjmedia_conf *conf,
                                                  unsigned slot,
                                                  int adj_level )
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127,, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    /* Set normalized adjustment level. */
    conf_port->tx_adj_level = adj_level + NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

/*
 * Adjust level of individual connection.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_conn_level( pjmedia_conf *conf,
                                                    unsigned src_slot,
                                                    unsigned sink_slot,
                                                    int adj_level )
{
    struct conf_port *src_port, *dst_port;
    unsigned i;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports &&
                     sink_slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];
    if (!src_port || !dst_port) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot)
            break;
    }

    if (i == src_port->listener_cnt) {
        /* connection hasn't been made */
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    } 
    /* Set normalized adjustment level. */
    src_port->listener_adj_level[i] = adj_level + NORMAL_LEVEL;

    pj_mutex_unlock(conf->mutex);
    return PJ_SUCCESS;
}


/*
 * Read from port.
 */
static pj_status_t read_port( pjmedia_conf *conf,
                              struct conf_port *cport, pj_int16_t *frame,
                              pj_size_t count, pjmedia_frame_type *type )
{

    pj_assert(count == conf->samples_per_frame);

    TRACE_((THIS_FILE, "read_port %.*s: count=%d", 
                       (int)cport->name.slen, cport->name.ptr,
                       count));

    /* 
     * If port's samples per frame and sampling rate and channel count
     * matche conference bridge's settings, get the frame directly from
     * the port.
     */
    if (cport->rx_buf_cap == 0) {
        pjmedia_frame f;
        pj_status_t status;

        f.buf = frame;
        f.size = count * BYTES_PER_SAMPLE;

        TRACE_((THIS_FILE, "  get_frame %.*s: count=%d", 
                   (int)cport->name.slen, cport->name.ptr,
                   count));

        status = pjmedia_port_get_frame(cport->port, &f);

        *type = f.type;

        return status;

    } else {
        unsigned samples_req;

        /* Initialize frame type */
        if (cport->rx_buf_count == 0) {
            *type = PJMEDIA_FRAME_TYPE_NONE;
        } else {
            /* we got some samples in the buffer */
            *type = PJMEDIA_FRAME_TYPE_AUDIO;
        }

        /*
         * If we don't have enough samples in rx_buf, read from the port 
         * first. Remember that rx_buf may be in different clock rate and
         * channel count!
         */

        samples_req = (unsigned) (count * 1.0 * 
                      cport->sampling_rate / conf->sampling_rate + 0.5);

        while (cport->rx_buf_count < samples_req) {

            pjmedia_frame f;
            pj_status_t status;

            f.buf = cport->rx_buf + cport->rx_buf_count;
            f.size = cport->samples_per_frame * BYTES_PER_SAMPLE;

            TRACE_((THIS_FILE, "  get_frame, count=%d", 
                       cport->samples_per_frame));

            status = pjmedia_port_get_frame(cport->port, &f);

            if (status != PJ_SUCCESS) {
                /* Fatal error! */
                return status;
            }

            if (f.type != PJMEDIA_FRAME_TYPE_AUDIO) {
                TRACE_((THIS_FILE, "  get_frame returned non-audio"));
                pjmedia_zero_samples( cport->rx_buf + cport->rx_buf_count,
                                      cport->samples_per_frame);
            } else {
                /* We've got at least one frame */
                *type = PJMEDIA_FRAME_TYPE_AUDIO;
            }

            /* Adjust channels */
            if (cport->channel_count != conf->channel_count) {
                if (cport->channel_count == 1) {
                    pjmedia_convert_channel_1ton((pj_int16_t*)f.buf, 
                                                 (const pj_int16_t*)f.buf,
                                                 conf->channel_count, 
                                                 cport->samples_per_frame,
                                                 0);
                    cport->rx_buf_count += (cport->samples_per_frame * 
                                            conf->channel_count);
                } else { /* conf->channel_count == 1 */
                    pjmedia_convert_channel_nto1((pj_int16_t*)f.buf, 
                                                 (const pj_int16_t*)f.buf,
                                                 cport->channel_count, 
                                                 cport->samples_per_frame, 
                                                 PJMEDIA_STEREO_MIX, 0);
                    cport->rx_buf_count += (cport->samples_per_frame / 
                                            cport->channel_count);
                }
            } else {
                cport->rx_buf_count += cport->samples_per_frame;
            }

            TRACE_((THIS_FILE, "  rx buffer size is now %d",
                    cport->rx_buf_count));

            pj_assert(cport->rx_buf_count <= cport->rx_buf_cap);
        }

        /*
         * If port's clock_rate is different, resample.
         * Otherwise just copy.
         */
        if (cport->sampling_rate != conf->sampling_rate) {
            
            unsigned src_count;

            TRACE_((THIS_FILE, "  resample, input count=%d", 
                    pjmedia_resample_get_input_size(cport->rx_resample)));

            pjmedia_resample_run( cport->rx_resample,cport->rx_buf, frame);

            src_count = (unsigned)(count * 1.0 * cport->sampling_rate / 
                                   conf->sampling_rate + 0.5);
            cport->rx_buf_count -= src_count;
            if (cport->rx_buf_count) {
                pjmedia_move_samples(cport->rx_buf, cport->rx_buf+src_count,
                                     cport->rx_buf_count);
            }

            TRACE_((THIS_FILE, "  rx buffer size is now %d",
                    cport->rx_buf_count));

        } else {

            pjmedia_copy_samples(frame, cport->rx_buf, (unsigned)count);
            cport->rx_buf_count -= (unsigned)count;
            if (cport->rx_buf_count) {
                pjmedia_move_samples(cport->rx_buf, cport->rx_buf+count,
                                     cport->rx_buf_count);
            }
        }
    }

    return PJ_SUCCESS;
}


/*
 * Write the mixed signal to the port.
 */
static pj_status_t write_port(pjmedia_conf *conf, struct conf_port *cport,
                              const pj_timestamp *timestamp, 
                              pjmedia_frame_type *frm_type)
{
    pj_int16_t *buf;
    pj_int32_t *mix_buf;
    unsigned samples_per_frame;
    unsigned j, ts;
    pj_status_t status;
    pj_int32_t adj_level;
    pj_int32_t tx_level;
    unsigned dst_count;

    pj_assert( conf && cport && timestamp && frm_type );

    *frm_type = PJMEDIA_FRAME_TYPE_AUDIO;

    /* Skip port if it is disabled */
    if (cport->tx_setting == PJMEDIA_PORT_DISABLE) {
        cport->tx_level = 0;
        *frm_type = PJMEDIA_FRAME_TYPE_NONE;
        return PJ_SUCCESS;
    }
    /* If port is muted or nobody is transmitting to this port, 
     * transmit NULL frame. 
     */
    else if ((cport->tx_setting == PJMEDIA_PORT_MUTE) ||
              cport->last_timestamp.u64 != timestamp->u64)/*no data in mix_buf*/
    {
        TRACE_EX( (THIS_FILE, "%s: Transmit heart-beat frames to port %p (%.*s, %d, transmitter_cnt=%d) last_timestamp=%llu, timestamp=%llu",
                    pj_thread_get_name( pj_thread_this() ),
                    cport,
                    (int)cport->name.slen,
                    cport->name.ptr,
                    cport->slot,
                    cport->transmitter_cnt,
                    cport->last_timestamp.u64, timestamp->u64) );

        pjmedia_frame frame;

        /* Clear left-over samples in tx_buffer, if any, so that it won't
         * be transmitted next time we have audio signal.
         */
        cport->tx_buf_count = 0;

        /* Add sample counts to heart-beat samples */
        cport->tx_heart_beat += conf->samples_per_frame * cport->sampling_rate /
                                conf->sampling_rate * 
                                cport->channel_count / conf->channel_count;

        /* Set frame timestamp */
        frame.timestamp.u64 = timestamp->u64 * cport->sampling_rate /
                                conf->sampling_rate;
        frame.type = PJMEDIA_FRAME_TYPE_NONE;
        frame.buf = NULL;
        frame.size = 0;

        /* Transmit heart-beat frames (may transmit more than one NULL frame
         * if port's ptime is less than bridge's ptime.
         */
        if (cport->port && cport->port->put_frame) {
            while (cport->tx_heart_beat >= cport->samples_per_frame) {

                pjmedia_port_put_frame(cport->port, &frame);

                cport->tx_heart_beat -= cport->samples_per_frame;
                frame.timestamp.u64 += cport->samples_per_frame;
            }
        }

        cport->tx_level = 0;
        *frm_type = PJMEDIA_FRAME_TYPE_NONE;
        return PJ_SUCCESS;
    }

    /* Reset heart-beat sample count */
    cport->tx_heart_beat = 0;

    /* If there are sources in the mix buffer, convert the mixed samples
     * from 32bit to 16bit in the mixed samples itself. This is possible 
     * because mixed sample is 32bit.
     *
     * In addition to this process, if we need to change the level of
     * TX signal, we adjust is here too.
     */

    /* Calculate signal level and adjust the signal when needed. 
     * Two adjustments performed at once: 
     * 1. user setting adjustment (tx_adj_level). 
     * 2. automatic adjustment of overflowed mixed buffer (mix_adj).
     */

    /* Apply simple AGC to the mix_adj, the automatic adjust, to avoid 
     * dramatic change in the level thus causing noise because the signal 
     * is now not aligned with the signal from the previous frame.
     */
    SIMPLE_AGC(cport->last_mix_adj, cport->mix_adj);
    cport->last_mix_adj = cport->mix_adj;

    /* adj_level = cport->tx_adj_level * cport->mix_adj / NORMAL_LEVEL;*/
    adj_level = cport->tx_adj_level * cport->mix_adj;
    adj_level >>= 7;

    tx_level = 0;
    
    /* Loops that support the MSVS Auto-Vectorizer feature must at least 
     * not use global variables (create local copies of such variables) and 
     * as a general rule not contains control flow-for example, "if" or "?:".
     * All math functions support the Auto-Vectorizer feature.
     * 
     * You can specify the /Qvec-report (Auto-Vectorizer Reporting Level) 
     * command-line option to report either successfully vectorized loops 
     * only-/Qvec-report:1-or both successfully and unsuccessfully vectorized 
     * loops-/Qvec-report:2).
     * 
     * See more: https://learn.microsoft.com/en-us/cpp/parallel/auto-parallelization-and-auto-vectorization?view=msvc-170&redirectedfrom=MSDN
     * 
     * Here we create local copies to automatically vectorize the main loops.
     */

    buf = (pj_int16_t*) cport->mix_buf;
    mix_buf = cport->mix_buf;
    samples_per_frame = conf->samples_per_frame;

    if (adj_level != NORMAL_LEVEL) {
        //loop vectorized
        for (j=0; j<samples_per_frame; ++j) {
            pj_int32_t itemp = mix_buf[j];

            /* Adjust the level */
            /*itemp = itemp * adj_level / NORMAL_LEVEL;*/
            itemp = (itemp * adj_level) >> 7;

            /* Clip the signal if it's too loud */
            if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
            else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

            /* Put back in the buffer. */
            buf[j] = (pj_int16_t) itemp;

            //auto-vectorizing compatible:
            tx_level += abs(buf[j]);
            //prevent auto-vectorizing:
            //tx_level += (buf[j]>=0? buf[j] : -buf[j]);
        }
    } else {
        //loop vectorized
        for (j=0; j<samples_per_frame; ++j) {
            buf[j] = (pj_int16_t) mix_buf[j];
            tx_level += abs(buf[j]);
            //tx_level += (buf[j]>=0? buf[j] : -buf[j]);
        }
    }

    tx_level /= samples_per_frame;

    /* Convert level to 8bit complement ulaw */
    tx_level = pjmedia_linear2ulaw(tx_level) ^ 0xff;

    cport->tx_level = tx_level;

    /* If port has the same clock_rate and samples_per_frame and 
     * number of channels as the conference bridge, transmit the 
     * frame as is.
     */
    if (cport->sampling_rate == conf->sampling_rate &&
        cport->samples_per_frame == conf->samples_per_frame &&
        cport->channel_count == conf->channel_count)
    {
        if (cport->port != NULL) {
            pjmedia_frame frame;

            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
            frame.buf = buf;
            frame.size = conf->samples_per_frame * BYTES_PER_SAMPLE;
            /* No need to adjust timestamp, port has the same
             * clock rate as conference bridge 
             */
            frame.timestamp = *timestamp;

            TRACE_((THIS_FILE, "put_frame %.*s, count=%d, last_timestamp=%llu, timestamp=%llu",
                               (int)cport->name.slen, cport->name.ptr,
                               frame.size / BYTES_PER_SAMPLE,
                               cport->last_timestamp.u64, timestamp->u64));

            return pjmedia_port_put_frame(cport->port, &frame);
        } else
            return PJ_SUCCESS;
    }

    /* If it has different clock_rate, must resample. */
    if (cport->sampling_rate != conf->sampling_rate) {
        pjmedia_resample_run( cport->tx_resample, buf, 
                              cport->tx_buf + cport->tx_buf_count );
        dst_count = (unsigned)(conf->samples_per_frame * 1.0 *
                               cport->sampling_rate / conf->sampling_rate + 0.5);
    } else {
        /* Same clock rate.
         * Just copy the samples to tx_buffer.
         */
        pjmedia_copy_samples( cport->tx_buf + cport->tx_buf_count,
                              buf, conf->samples_per_frame );
        dst_count = conf->samples_per_frame;
    }

    /* Adjust channels */
    if (cport->channel_count != conf->channel_count) {
        pj_int16_t *tx_buf = cport->tx_buf + cport->tx_buf_count;
        if (conf->channel_count == 1) {
            pjmedia_convert_channel_1ton(tx_buf, tx_buf,
                                         cport->channel_count, 
                                         dst_count, 0);
            dst_count *= cport->channel_count;
        } else { /* cport->channel_count == 1 */
            pjmedia_convert_channel_nto1(tx_buf, tx_buf,
                                         conf->channel_count, 
                                         dst_count, PJMEDIA_STEREO_MIX, 0);
            dst_count /= conf->channel_count;
        }
    }

    cport->tx_buf_count += dst_count;

    pj_assert(cport->tx_buf_count <= cport->tx_buf_cap);

    /* Transmit while we have enough frame in the tx_buf. */
    status = PJ_SUCCESS;
    ts = 0;
    while (cport->tx_buf_count >= cport->samples_per_frame &&
           status == PJ_SUCCESS) 
    {
        
        TRACE_((THIS_FILE, "write_port %.*s: count=%d", 
                           (int)cport->name.slen, cport->name.ptr,
                           cport->samples_per_frame));

        if (cport->port) {
            pjmedia_frame frame;

            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
            frame.buf = cport->tx_buf;
            frame.size = cport->samples_per_frame * BYTES_PER_SAMPLE;
            /* Adjust timestamp as port may have different clock rate
             * than the bridge.
             */
            frame.timestamp.u64 = timestamp->u64 * cport->sampling_rate /
                                  conf->sampling_rate;

            /* Add timestamp for individual frame */
            frame.timestamp.u64 += ts;
            ts += cport->samples_per_frame;

            TRACE_((THIS_FILE, "put_frame %.*s, count=%d", 
                               (int)cport->name.slen, cport->name.ptr,
                               frame.size / BYTES_PER_SAMPLE));

            status = pjmedia_port_put_frame(cport->port, &frame);

        } else
            status = PJ_SUCCESS;

        cport->tx_buf_count -= cport->samples_per_frame;
        if (cport->tx_buf_count) {
            pjmedia_move_samples(cport->tx_buf, 
                                 cport->tx_buf + cport->samples_per_frame,
                                 cport->tx_buf_count);
        }

        TRACE_((THIS_FILE, " tx_buf count now is %d", 
                           cport->tx_buf_count));
    }

    return status;
}

static inline pj_int16_t *get_read_buffer(struct conf_port *conf_port, pjmedia_frame *frame)
{
    if (conf_port->rx_frame_buf)
        return conf_port->rx_frame_buf;   // parallel conference bridge
    else
        return (pj_int16_t *)frame->buf;  // sequential conference bridge
}

/*
 * Player callback.
 */
static pj_status_t get_frame(pjmedia_port *this_port, 
                             pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf *)this_port->port_data.pdata;

    TRACE_((THIS_FILE, "- clock -"));

    /* Check that correct size is specified. */
    pj_assert(frame->size == conf->samples_per_frame *
                             conf->bits_per_sample / 8);

#if 0
    /* Perform any queued operations that need to be synchronized with
     * the clock such as connect, disonnect, remove.
     */
    if (!pj_list_empty(conf->op_queue)) {
        pj_log_push_indent();
        handle_op_queue(conf);
        pj_log_pop_indent();
    }
#endif

    /* No mutex from this point! Otherwise it may cause deadlock as
     * put_frame()/get_frame() may invoke callback.
     *
     * Note that any changes on the conference connections must be
     * synchronized.
     */

     /* Step 1 Initialization */

    /* Force frame type NONE */
    frame->type = PJMEDIA_FRAME_TYPE_NONE;

    pj_atomic_set(conf->active_ports_idx, conf->upper_bound);
    pj_atomic_set(conf->listener_counter, conf->upper_bound_reg);

    conf->frame = frame;
    conf->sound_port = NULL;

    /* Step 2-3
     * Get frames from all ports, and "mix" the signal
     * to mix_buf of all listeners of the port and
     * transmit whatever listeners have in their buffer
     */
    if (conf->is_parallel) {
        pj_int32_t rc;
        /* Start the parallel team
         * all threads have reached the barrier already.
         * Should be a very short waiting.
         */
        TRACE_EX((THIS_FILE, "%s: timestamp=%llu, thread at barrier",
                    pj_thread_get_name(pj_thread_this()),
                    conf->frame ? conf->frame->timestamp.u64 : (pj_uint64_t)-1));

        rc = pj_barrier_wait(conf->active_thread, PJ_BARRIER_FLAGS_NO_DELETE | PJ_BARRIER_FLAGS_SPIN_ONLY);
        pj_assert(rc == PJ_TRUE || rc == PJ_FALSE);

        TRACE_EX((THIS_FILE, "%s: timestamp=%llu, thread activated, return = %d",
                    pj_thread_get_name(pj_thread_this()),
                    conf->frame ? conf->frame->timestamp.u64 : (pj_uint64_t)-1,
                    rc));
    }

    perform_get_frame(conf);

    if (conf->is_parallel) {
        /* wait until all worker threads have completed their work */
        CONF_CHECK_SUCCESS(pj_event_wait(conf->barrier_evt),(void)0);
        pj_atomic_set(conf->active_thread_cnt, conf->threads-1);
    }
    pj_assert(pj_atomic_get(conf->active_ports_idx) == -conf->threads);

    /* Return sound playback frame. */
    if (conf->sound_port != NULL) {
        TRACE_((THIS_FILE, "write to audio, count=%d",
                conf->samples_per_frame));
        pjmedia_copy_samples((pj_int16_t *)frame->buf,
                                (const pj_int16_t *)conf->sound_port->mix_buf,
                                conf->samples_per_frame);
        /* MUST set frame type */
        pj_assert(frame->type != PJMEDIA_FRAME_TYPE_NONE);
        conf->sound_port = NULL;
    }
    conf->frame = NULL;

    /* Perform any queued operations that need to be synchronized with
     * the clock such as connect, disonnect, remove.
     */
    if (!pj_list_empty(conf->op_queue)) {
        pj_log_push_indent();
        handle_op_queue(conf);
        pj_log_pop_indent();
    }

#ifdef REC_FILE
    if (fhnd_rec == NULL)
        fhnd_rec = fopen(REC_FILE, "wb");
    if (fhnd_rec)
        fwrite(frame->buf, frame->size, 1, fhnd_rec);
#endif

    return PJ_SUCCESS;
}


static pj_status_t thread_pool_start(pjmedia_conf *conf)
{
    int i;
    pj_assert(conf->is_parallel);

    CONF_CHECK_SUCCESS(pj_barrier_create(conf->pool,
                                      conf->threads,
                                      &conf->active_thread), 
                       return tmp_status_);

    CONF_CHECK_SUCCESS(pj_barrier_create(conf->pool,
                                      conf->threads,
                                      &conf->barrier), 
                       return tmp_status_);

    pj_atomic_value_t    worker_threads = conf->threads-1;
    /* active worker thread counter*/
    CONF_CHECK_SUCCESS(pj_atomic_create(conf->pool, worker_threads, &conf->active_thread_cnt), 
                       return tmp_status_);
    /* exit barrier event */
    CONF_CHECK_SUCCESS(pj_event_create(conf->pool, "barrier_evt", PJ_FALSE, PJ_FALSE, &conf->barrier_evt), 
                       return tmp_status_);


    /* Thread description's to register threads with pjsip */
    conf->pool_threads = (pj_thread_t **)pj_pool_calloc(conf->pool, worker_threads, sizeof(pj_thread_t *));
    PJ_ASSERT_RETURN(conf->pool_threads, PJ_ENOMEM);

    for (i = 0; i < worker_threads; i++) {
        char        obj_name[PJ_MAX_OBJ_NAME];
        pj_ansi_snprintf(obj_name, sizeof(obj_name), "conf_pool_%d", i);

        CONF_CHECK_SUCCESS(pj_thread_create(conf->pool, obj_name, &conf_thread, conf, 0, 0, &conf->pool_threads[i]), 
                           return tmp_status_);
    }

    conf->running = PJ_TRUE;

    return PJ_SUCCESS;
}

/*
 * Conf thread pool's thread function.
 */
static int conf_thread(void *arg)
{
    pjmedia_conf *conf = (pjmedia_conf *)arg;
    pj_int32_t rc;
    pj_assert(conf->is_parallel);

    /* don't go to the barrier while thread pool is creating
     * if we can not create all threads,
     * we should not go to the barrier because we can not leave it
     */
    while (!conf->running && !conf->quit_flag) {
        pj_thread_sleep(0);
        //TODO classic option for using condition variable
    }

    if (conf->running) {

        while (1) {
            TRACE_EX((THIS_FILE, "%s: timestamp=%llu, thread at barrier",
                      pj_thread_get_name(pj_thread_this()),
                      conf->frame ? conf->frame->timestamp.u64 : (pj_uint64_t)-1));

            /* long waiting for next timer tick. if supported, blocks immediately*/
            rc = pj_barrier_wait(conf->active_thread,
                                 PJ_BARRIER_FLAGS_NO_DELETE |
                                 PJ_BARRIER_FLAGS_BLOCK_ONLY);
            pj_assert(rc == PJ_TRUE || rc == PJ_FALSE);

            /* quit_flag should be checked only once per loop and strictly
             * after the active_thread barrier is crossed
             */
            if (conf->quit_flag) {
                TRACE_EX((THIS_FILE,
                          "%s: timestamp=%llu, thread exiting, barrier return = %d, quit_flag = %d",
                          pj_thread_get_name(pj_thread_this()),
                          conf->frame ? conf->frame->timestamp.u64 : (pj_uint64_t)-1,
                          rc, conf->quit_flag));
                break;
            } else {
                TRACE_EX((THIS_FILE,
                          "%s: timestamp=%llu, thread activated, barrier return = %d",
                          pj_thread_get_name(pj_thread_this()),
                          conf->frame ? conf->frame->timestamp.u64 : (pj_uint64_t)-1,
                          rc));
            }

            perform_get_frame(conf);

            /* signal to the get_frame() thread if all worker threads have completed their work */
            if (!pj_atomic_dec_and_get(conf->active_thread_cnt))
                CONF_CHECK_SUCCESS(pj_event_set(conf->barrier_evt), (void)0);
                //TODO a conditional variable would be preferable here
        }
    }

    return 0;
}

static void perform_get_frame(pjmedia_conf *conf)
{

    pj_status_t status;
    pj_atomic_value_t i;
    pjmedia_frame *frame = conf->frame;

    while ((i = pj_atomic_dec_and_get(conf->active_ports_idx)) >= 0) {
        pj_int16_t *p_in;
        unsigned j, samples_per_frame = conf->samples_per_frame;
        pj_int32_t cj, listener_cnt;
        pj_int32_t level = 0;
        SLOT_TYPE port_idx = conf->active_ports[i];
        pj_assert(port_idx < conf->max_ports);
        struct conf_port *conf_port = conf->ports[port_idx];
        PJ_ASSERT_ON_FAIL(conf_port, continue);
        unsigned rx_adj_level = conf_port->rx_adj_level; //for auto-vectorizer

        /* Skip if we're not allowed to receive from this port. */
        if (conf_port->rx_setting == PJMEDIA_PORT_DISABLE) {
            conf_port->rx_level = 0;
            continue;
        }

        p_in = get_read_buffer(conf_port, frame);
        /* Get frame from this port.
         * For passive ports, get the frame from the delay_buf.
         * For other ports, get the frame from the port. 
         */
        if (conf_port->delay_buf != NULL) {

            /* Check that correct size is specified. */
            pj_assert(frame->size == conf->rx_frame_buf_cap);
            /* read data to different buffers to different conf_port's parallel processing */
            status = pjmedia_delay_buf_get(conf_port->delay_buf, p_in);
            if (status != PJ_SUCCESS) {
                conf_port->rx_level = 0;
                TRACE_EX((THIS_FILE, "%s: No frame from the passive port (%.*s, %u, listener_cnt=%u)",
                          pj_thread_get_name(pj_thread_this()),
                          (int)conf_port->name.slen,
                          conf_port->name.ptr,
                          port_idx, conf_port->listener_cnt));
                continue;
            } 

        } else {
            pjmedia_frame_type frame_type;

            /* Check that correct size is specified. */
            pj_assert(frame->size == conf/*_port */->rx_frame_buf_cap);
            /* read data to different buffers to different conf_port's parallel processing */
            status = read_port(conf, conf_port, p_in,
                               conf->samples_per_frame, &frame_type);

            /* Check that the port is not removed when we call get_frame() */
            /*
            * if port is removed old conf_port may point to not authorized memory
            * We can not call conf_port->rx_level = 0; here!
            * "Port is not removed" check should take priority over the return code check
            *
            * However this check is not necessary for async conference bridge,
            * because application can not remove port while we are in get_frame() callback.
            * The only thing that can happen is that port removing will be sheduled
            * there but still will processed later (see Step 3).
            */
            if (conf->ports[port_idx] != conf_port) {
                //conf_port->rx_level = 0;
                PJ_LOG(4, (THIS_FILE, "Port %u is removed when we call get_frame()", port_idx));
                continue;
            }

            if (status != PJ_SUCCESS) {

                /* check status and disable port here.
                 * Prevent multiply eof callback invoke,
                 * if fileplayer has reached EOF (i.e. status == PJ_EEOF)
                 */
                if (status == PJ_EEOF) {
                    TRACE_((THIS_FILE, "Port %.*s reached EOF and is now disabled",
                            (int)conf_port->name.slen,
                            conf_port->name.ptr));
                    conf_port->rx_setting = PJMEDIA_PORT_DISABLE;
                }


                /* bennylp: why do we need this????
                 * Also see comments on similar issue with write_port().
                PJ_LOG(4,(THIS_FILE, "Port %.*s get_frame() returned %d. "
                                     "Port is now disabled",
                                     (int)conf_port->name.slen,
                                     conf_port->name.ptr,
                                     status));
                conf_port->rx_setting = PJMEDIA_PORT_DISABLE;
                 */
                conf_port->rx_level = 0;

                TRACE_EX((THIS_FILE, "%s: No frame from the port (%.*s, %u, listener_cnt=%u)",
                          pj_thread_get_name(pj_thread_this()),
                          (int)conf_port->name.slen,
                          conf_port->name.ptr,
                          port_idx, conf_port->listener_cnt));

                continue;
            }

            /* Ignore if we didn't get any frame */
            if (frame_type != PJMEDIA_FRAME_TYPE_AUDIO) {
                conf_port->rx_level = 0;
                TRACE_EX((THIS_FILE, "%s: frame_type %d != PJMEDIA_FRAME_TYPE_AUDIO from the port (%.*s, %u, listener_cnt=%u)",
                          pj_thread_get_name(pj_thread_this()),
                          frame_type,
                          (int)conf_port->name.slen,
                          conf_port->name.ptr,
                          port_idx, conf_port->listener_cnt));
                continue;
            }
        }

        /* Adjust the RX level from this port
         * and calculate the average level at the same time.
         */
        if (rx_adj_level != NORMAL_LEVEL) {
            //loop vectorized
            for (j=0; j<samples_per_frame; ++j) {
                /* For the level adjustment, we need to store the sample to
                 * a temporary 32bit integer value to avoid overflowing the
                 * 16bit sample storage.
                 */
                pj_int32_t itemp;

                itemp = p_in[j];
                /*itemp = itemp * adj / NORMAL_LEVEL;*/
                /* bad code (signed/unsigned badness):
                 *  itemp = (itemp * conf_port->rx_adj_level) >> 7;
                 */
                itemp *= rx_adj_level;
                itemp >>= 7;

                /* Clip the signal if it's too loud */
                if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                p_in[j] = (pj_int16_t) itemp;
                level += abs(p_in[j]);
                //level += (p_in[j]>=0 ? p_in[j] : -p_in[j]);
            }
        } else {
            //loop vectorized
            for (j=0; j<samples_per_frame; ++j) {
                level += abs(p_in[j]);
                //level += (p_in[j]>=0 ? p_in[j] : -p_in[j]);
            }
        }

        level /= samples_per_frame;

        /* Convert level to 8bit complement ulaw */
        level = pjmedia_linear2ulaw(level) ^ 0xff;

        /* Put this level to port's last RX level. */
        conf_port->rx_level = level;

        // Ticket #671: Skipping very low audio signal may cause noise 
        // to be generated in the remote end by some hardphones.
        /* Skip processing frame if level is zero */
        //if (level == 0)
        //    continue;

        /* Add the signal to all listeners. */
        for (cj = 0, listener_cnt = conf_port->listener_cnt; cj < listener_cnt; ++cj) {
            struct conf_port *listener;
            rx_buffer_slist_node *mix_node;
            SLOT_TYPE listener_slot = conf_port->listener_slots[cj];

            listener = conf->ports[listener_slot];

            /* Skip if this listener doesn't want to receive audio */
            if (listener->tx_setting != PJMEDIA_PORT_ENABLE) {
                TRACE_EX((THIS_FILE, "%s: listener (%.*s, %d, transmitter_cnt=%d) doesn't want to receive audio from the port (%.*s, %u, listener_cnt=%u)",
                          pj_thread_get_name(pj_thread_this()),
                          (int)listener->name.slen,
                          listener->name.ptr,
                          listener_slot,
                          listener->transmitter_cnt,
                          (int)conf_port->name.slen,
                          conf_port->name.ptr,
                          port_idx, conf_port->listener_cnt));
                continue;
            }

            mix_node = pj_atomic_slist_pop(listener->free_node_cache);
            pj_assert(mix_node || listener->transmitter_cnt > 1 && conf->is_parallel);
            pj_assert(conf->is_parallel == (listener->tx_lock != NULL));
            if (mix_node == NULL && listener->tx_lock) {
                pj_lock_acquire(listener->tx_lock);
                /* tx_lock protects listener->pool.
                 * The only operation that requires lock protection.
                 */
                mix_node = pj_atomic_slist_calloc(listener->pool, 1, sizeof(rx_buffer_slist_node));
                pj_lock_release(listener->tx_lock);
            }

            PJ_ASSERT_ON_FAIL(mix_node, continue);

            mix_node->rx_frame_buf = p_in;
            mix_node->rx_port = conf_port;
            mix_node->listener_adj_level = conf_port->listener_adj_level[cj];
            pj_atomic_slist_push(listener->buff_to_mix, mix_node);

            if (pj_atomic_inc_and_get(listener->requests_to_mix) == 1) {
                /* the first mixer appeared here - mix all for this listener! 
                 * This thread has monopolized the mixing operations on that listener port.
                 * Other threads can asynchronously push their data 
                 * into this listener's buff_to_mix.
                 */
                do {
                    while ((mix_node = pj_atomic_slist_pop(listener->buff_to_mix)) != NULL) {

                        unsigned listener_adj_level = mix_node->listener_adj_level;
                        struct conf_port *rx_port = mix_node->rx_port;
                        pj_int16_t *rx_buf = mix_node->rx_frame_buf;

                        /* return node into free_node_cache ASAP, 
                         * other threads may reuse it now 
                         */
                        pj_atomic_slist_push(listener->free_node_cache, mix_node);

                        /* only one thread at time call mix_and_transmit() for the
                         * same listener, no addition lock protection required here */
                        mix_and_transmit(conf,
                                         listener, listener_slot, listener_adj_level,
                                         rx_port, rx_buf, 
                                         &frame->timestamp);
                    }
                } while (pj_atomic_dec_and_get(listener->requests_to_mix));
            }

        } /* loop the listeners of conf port */

    } /* loop of all conf ports */

    if (conf->is_parallel) {
        pj_int32_t rc;
        TRACE_EX((THIS_FILE, "%s: timestamp=%llu, ARRIVE AT BARRIER",
                  pj_thread_get_name(pj_thread_this()),
                  frame->timestamp.u64));

        /* If we carefully balance the work, we won't have to wait long here.
         * let it be the default waiting (spin then block)
         */
        rc = pj_barrier_wait(conf->barrier, PJ_BARRIER_FLAGS_NO_DELETE);
        pj_assert(rc == PJ_TRUE || rc == PJ_FALSE);

        TRACE_EX((THIS_FILE, "%s: timestamp=%llu, BARRIER OVERCOME, return = %d",
                  pj_thread_get_name(pj_thread_this()),
                  frame->timestamp.u64,
                  rc));
        PJ_UNUSED_ARG(rc);
    }

    /* Step 3
     * Time for all ports to transmit whatever they have in their
     * buffer.
     */
    while ((i = pj_atomic_dec_and_get(conf->listener_counter)) >= 0) {
        SLOT_TYPE port_idx = conf->active_listener[i];
        struct conf_port *listener = conf->ports[port_idx];
        pjmedia_frame_type frame_type;
        status = write_port(conf, listener, &frame->timestamp, &frame_type);
#if 0
        if (status != PJ_SUCCESS) {
            /* bennylp: why do we need this????
            One thing for sure, put_frame()/write_port() may return
            non-successfull status on Win32 if there's temporary glitch
            on network interface, so disabling the port here does not
            sound like a good idea.

            PJ_LOG(4,(THIS_FILE, "Port %.*s put_frame() returned %d. "
            "Port is now disabled",
            (int)conf_port->name.slen,
            conf_port->name.ptr,
            status));
            conf_port->tx_setting = PJMEDIA_PORT_DISABLE;
            */
            continue;
        }
#endif
        /* Set the type of frame to be returned to sound playback
         * device.
         */
        if (status == PJ_SUCCESS && port_idx == 0 && listener->tx_level) {
            /* MUST set frame type */
            conf->frame->type = frame_type;
            conf->sound_port = listener;
        }
    }
}

static void mix_and_transmit(pjmedia_conf *conf, struct conf_port *listener, 
                             SLOT_TYPE listener_slot,
                             unsigned listener_adj_level, 
                             struct conf_port *conf_port,
                             pj_int16_t *p_in,
                             const pj_timestamp *timestamp) 
{
    PJ_UNUSED_ARG(conf_port);
    PJ_UNUSED_ARG(listener_slot);

    pj_int16_t *p_in_conn_leveled;
    unsigned k, samples_per_frame = conf->samples_per_frame;
    pj_int32_t *mix_buf = listener->mix_buf;

    /* apply connection level, if not normal */
    if (listener_adj_level != NORMAL_LEVEL) {
        /* take the leveled frame */
        p_in_conn_leveled = listener->adj_level_buf;
        //loop vectorized
        for (k = 0; k < samples_per_frame; ++k) {
            /* For the level adjustment, we need to store the sample to
             * a temporary 32bit integer value to avoid overflowing the
             * 16bit sample storage.
             */
            pj_int32_t itemp;

            itemp = p_in[k];
            /*itemp = itemp * adj / NORMAL_LEVEL;*/
            /* bad code (signed/unsigned badness):
             *  itemp = (itemp * conf_port->listsener_adj_level) >> 7;
             */
            itemp *= listener_adj_level;
            itemp >>= 7;

            /* Clip the signal if it's too loud */
            if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
            else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

            p_in_conn_leveled[k] = (pj_int16_t)itemp;
        }

    } else {
        /* take the frame as-is */
        p_in_conn_leveled = p_in;
    }

    if (listener->transmitter_cnt > 1) {
        /* Mixing signals,
         * and calculate appropriate level adjustment if there is
         * any overflowed level in the mixed signal.
         */
        pj_int32_t mix_buf_min = 0;
        pj_int32_t mix_buf_max = 0;

        if (listener->last_timestamp.u64 == timestamp->u64) {
            /* this frame is NOT from the first transmitter */
            //loop vectorized
            for (k = 0; k < samples_per_frame; ++k) {
                mix_buf[k] += p_in_conn_leveled[k]; // not the first - sum
                if (mix_buf[k] < mix_buf_min)
                    mix_buf_min = mix_buf[k];
                if (mix_buf[k] > mix_buf_max)
                    mix_buf_max = mix_buf[k];
            }
            TRACE_EX((THIS_FILE, "%s: listener (%.*s, %d, transmitter_cnt=%d) get (sum) audio from the port (%.*s, %d, listener_cnt=%d)",
                pj_thread_get_name(pj_thread_this()),
                (int)listener->name.slen,
                listener->name.ptr,
                listener_slot,
                listener->transmitter_cnt,
                (int)conf_port->name.slen,
                conf_port->name.ptr,
                port_idx, conf_port->listener_cnt));

        } else {
            /* this frame is from the first transmitter */
            listener->last_timestamp = *timestamp;
            /* Reset auto adjustment level for mixed signal. */
            listener->mix_adj = NORMAL_LEVEL;


            /* We do not want to reset buffer, we just copy the first frame there. */
            //loop vectorized
            for (k = 0; k < samples_per_frame; ++k) {
                mix_buf[k] = p_in_conn_leveled[k]; // the first - copy
                if (mix_buf[k] < mix_buf_min)
                    mix_buf_min = mix_buf[k];
                if (mix_buf[k] > mix_buf_max)
                    mix_buf_max = mix_buf[k];
            }
            TRACE_EX((THIS_FILE, "%s: listener %p (%.*s, %d, transmitter_cnt=%d) get (copy) audio from the port %p (%.*s, %d, listener_cnt=%d)",
                pj_thread_get_name(pj_thread_this()),
                listener,
                (int)listener->name.slen,
                listener->name.ptr,
                listener_slot,
                listener->transmitter_cnt,
                conf_port,
                (int)conf_port->name.slen,
                conf_port->name.ptr,
                port_idx, conf_port->listener_cnt));
        }

        /* Check if normalization adjustment needed. */
        if (mix_buf_min < MIN_LEVEL || mix_buf_max > MAX_LEVEL) {
            int tmp_adj;

            if (-mix_buf_min > mix_buf_max)
                mix_buf_max = -mix_buf_min;

            /* NORMAL_LEVEL * MAX_LEVEL / mix_buf_max; */
            tmp_adj = (MAX_LEVEL<<7) / mix_buf_max;
            if (tmp_adj < listener->mix_adj)
                listener->mix_adj = tmp_adj;
        }

    } else {
        /* this frame is from the only transmitter */
        pj_assert(listener->transmitter_cnt == 1 && listener->last_timestamp.u64 != timestamp->u64);

        /* Only 1 transmitter:
         * just copy the samples to the mix buffer
         * no mixing and level adjustment needed
         */

        listener->last_timestamp = *timestamp;
        /* Reset auto adjustment level for mixed signal. */
        listener->mix_adj = NORMAL_LEVEL;

        //loop vectorized
        for (k = 0; k < samples_per_frame; ++k) {
            mix_buf[k] = p_in_conn_leveled[k];  // here copying 16 bit value to 32 bit dst
        }
        TRACE_EX((THIS_FILE, "%s: listener %p (%.*s, %d, transmitter_cnt=%d)"
            " get audio from the (only) port %p (%.*s, %d, listener_cnt=%d) last_timestamp=%llu, timestamp=%llu",
            pj_thread_get_name(pj_thread_this()),
            listener,
            (int)listener->name.slen,
            listener->name.ptr,
            listener->slot,
            listener->transmitter_cnt,
            conf_port,
            (int)conf_port->name.slen,
            conf_port->name.ptr,
            conf_port->slot, conf_port->listener_cnt,
            listener->last_timestamp.u64, timestamp->u64));

    }

}

#if !DEPRECATED_FOR_TICKET_2234
/*
 * get_frame() for passive port
 */
static pj_status_t get_frame_pasv(pjmedia_port *this_port, 
                                  pjmedia_frame *frame)
{
    pj_assert(0);
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return -1;
}
#endif


/*
 * Recorder (or passive port) callback.
 */
static pj_status_t put_frame(pjmedia_port *this_port, 
                             pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    struct conf_port *port = conf->ports[this_port->port_data.ldata];
    pj_status_t status;

    /* Check for correct size. */
    PJ_ASSERT_RETURN( frame->size == conf->samples_per_frame *
                                     conf->bits_per_sample / 8,
                      PJMEDIA_ENCSAMPLESPFRAME);

    /* Check existance of delay_buf instance */
    PJ_ASSERT_RETURN( port->delay_buf, PJ_EBUG );

    /* Skip if this port is muted/disabled. */
    if (port->rx_setting != PJMEDIA_PORT_ENABLE) {
        return PJ_SUCCESS;
    }

    /* Skip if no port is listening to the microphone */
    if (port->listener_cnt == 0) {
        return PJ_SUCCESS;
    }

    status = pjmedia_delay_buf_put(port->delay_buf, (pj_int16_t*)frame->buf);

    return status;
}


/*
 * Add destructor handler.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_destroy_handler(
                                            pjmedia_conf* conf,
                                            unsigned slot,
                                            void* member,
                                            pj_grp_lock_handler handler)
{
    struct conf_port *cport;
    pj_grp_lock_t *grp_lock;

    PJ_ASSERT_RETURN(conf && handler && slot < conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid and has group lock. */
    cport = conf->ports[slot];
    if (!cport || !cport->port || !cport->port->grp_lock) {
        pj_mutex_unlock(conf->mutex);
        return cport? PJ_EINVALIDOP : PJ_EINVAL;
    }
    grp_lock = cport->port->grp_lock;

    pj_mutex_unlock(conf->mutex);

    return pj_grp_lock_add_handler(grp_lock, NULL, member, handler);
}


/*
 * Remove previously registered destructor handler.
 */
PJ_DEF(pj_status_t) pjmedia_conf_del_destroy_handler(
                                            pjmedia_conf* conf,
                                            unsigned slot,
                                            void* member,
                                            pj_grp_lock_handler handler)
{
    struct conf_port* cport;
    pj_grp_lock_t* grp_lock;

    PJ_ASSERT_RETURN(conf && handler && slot < conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid and has group lock. */
    cport = conf->ports[slot];
    if (!cport || !cport->port || !cport->port->grp_lock) {
        pj_mutex_unlock(conf->mutex);
        return cport ? PJ_EINVALIDOP : PJ_EINVAL;
    }
    grp_lock = cport->port->grp_lock;

    pj_mutex_unlock(conf->mutex);

    return pj_grp_lock_del_handler(grp_lock, member, handler);
}


#endif /* PJMEDIA_CONF_BACKEND == PJMEDIA_CONF_PARALLEL_BRIDGE_BACKEND */

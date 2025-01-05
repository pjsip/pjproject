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

#if defined(PJ_STACK_IMPLEMENTATION)
#include <pj/stack.h>
#endif

#if !defined(PJMEDIA_CONF_USE_SWITCH_BOARD) || PJMEDIA_CONF_USE_SWITCH_BOARD==0

#ifdef _OPENMP
#   include <omp.h>

/* Open MP related modification by Leonid Goltsblat <lgoltsblat@gmail.com> 2021-2025
 * 
 * Enabling OpenMP support makes it possible to parallelize the processing of audio frames.
 * To activate OpenMP support you must perform the appropriate development environment setup.
 * For example, in Visual Studio, you must enable OpenMP support in the project settings.
 * see: https://learn.microsoft.com/en-us/cpp/build/reference/openmp-enable-openmp-2-0-support?view=msvc-170
 * In GCC, you must use the appropriate compiler options.
 * 
 * Current implementation uses only basic subset of OpenMP features corresponding to OpenMP 2.0 
 * and should be compatible with any OpenMP 2.0 compliant compiler.
 */

#   ifndef PJ_OPENMP_FOR_CLAUSES
#       define PJ_OPENMP_FOR_CLAUSES
/* for example
 * #define PJSIP_OMP_CHUNK_SIZE    16
 * #define PJ_OPENMP_FOR_CLAUSES   schedule(dynamic, PJSIP_OMP_CHUNK_SIZE) if (max_thread > 1) num_threads(max_thread)
 * will produce
 * #pragma omp parallel for schedule(dynamic, PJSIP_OMP_CHUNK_SIZE) if (max_thread > 1) num_threads(max_thread)
 */
#   endif
#endif  //_OPENMP

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


#define THIS_FILE       "conference.c"

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
#   define ATTACK_A     ((conf->clock_rate / conf->samples_per_frame) >> 4)
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
    unsigned             clock_rate;    /**< Port's clock rate.             */
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
    pj_int16_t          *adj_level_buf; /**< The adjustment buffer.         */

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

#if 0
    /* We always know lower_bound and upper_bound and never use port_cnt 
     * There is no need for asynchronous port creation at all.
     * This flag has become redundant.
     */
    pj_bool_t            is_new;        /**< Newly added port, avoid read/write
                                             data from/to.                  */
#endif

#ifdef _OPENMP
    pj_int16_t          *rx_frame_buf;  /**< The RX buffer.                */
    unsigned             rx_frame_buf_cap;/**< Max size, in bytes          */

    omp_lock_t           tx_Lock;       /**< Lock to protect transmit
                                             buffer                        */
#endif  //_OPENMP

    pj_timestamp         last_timestamp;/**< last transmited packet
                                         * timestamp. We set this when
                                         * first time put something into
                                         * the mix_buf.
                                         * If this time stamp is equals to
                                         * current frame timestamp,
                                         * we have data to transmite       */

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
#if defined(PJ_STACK_IMPLEMENTATION)

typedef struct PJ_STACK_ALIGN_PREFIX port_slot {
    PJ_DECL_STACK_MEMBER(struct port_slot);
} PJ_STACK_ALIGN_SUFFIX port_slot;

typedef pj_stack_type    unused_slots_cache; /**< Container to store unused 
                                              *   port's slots.             */

#else

typedef struct port_slot {
    PJ_DECL_LIST_MEMBER(struct port_slot);
} port_slot;

typedef port_slot        unused_slots_cache; /**< Container to store unused
                                              *   port's slots.             */

#endif


/*
 * Conference bridge.
 */
struct pjmedia_conf
{
    pj_pool_t            *pool;         /**< Pool                           */
    unsigned              options;      /**< Bitmask options.               */
    unsigned              max_ports;    /**< Maximum ports.                 */
#if 0
    unsigned              port_cnt;     /**< Current number of ports.       */
#endif //0 
    /**< lower and upper boundaries to scan ports[]                         */
    pj_uint32_t           lower_bound;  /**< The least connected port's slot*/
    pj_uint32_t           upper_bound;  /**< The next after greatest
                                         *   connected port's slot          */

    unsigned              connect_cnt;  /**< Total number of connections    */
    pjmedia_snd_port     *snd_dev_port; /**< Sound device port.             */
    pjmedia_port         *master_port;  /**< Port zero's port.              */
    char                  master_name_buf[80]; /**< Port0 name buffer.      */
    pj_mutex_t           *mutex;        /**< Conference mutex.              */
    struct conf_port    **ports;        /**< Array of ports.                */
    unsigned              clock_rate;   /**< Sampling rate.                 */
    unsigned              channel_count;/**< Number of channels (1=mono).   */
    unsigned              samples_per_frame;    /**< Samples per frame.     */
    unsigned              bits_per_sample;      /**< Bits per sample.       */

    op_entry             *op_queue;     /**< Queue of operations.           */
    op_entry             *op_queue_free;/**< Queue of free entries.         */


    unused_slots_cache  *unused_slots;    /**< Unused port's slots.         */
    port_slot           *free_port_slots; /**< Persistent array of max_ports
                                           * size used for quick and locking
                                           * free finding unused ports[]
                                           * slot (i.e. index in ports[])   */
    pj_int32_t           listener_counter;/**< current quantity of
                                           * active_listener                */
    struct conf_port   **active_listener; /**< listener with data to transmit */

    pj_int32_t          *active_ports;    /**< array of port slots from 0
                                           * to upper_bound with no gaps.
                                           * Compacted port's array should 
                                           * help OpenMP to distribute task 
                                           * throught team's threads        */


#ifdef _OPENMP
    pj_thread_desc      *omp_threads;     /**< Thread description's
                                           * to register omp threads
                                           * with pjsip                     */
    int                  omp_max_threads; /**< omp_threads[] dimension      */
    int                  omp_threads_idx; /**< omp_threads[] current idx    */

#endif  //_OPENMP

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


#ifdef _OPENMP
/* register omp thread with pjsip */
static void register_omp_thread(pjmedia_conf* conf);
#endif //_OPENMP


static void destroy_conf_port(struct conf_port *conf_port);

/* As we don't hold mutex in the clock/get_frame(), some conference operations
 * that change conference states need to be synchronized with the clock.
 * So some steps of the operations needs to be executed within the clock tick
 * context, especially the steps related to changing ports connection.
 */

/* Synchronized operation type enumeration. */
typedef enum op_type
{
    OP_UNKNOWN,
#if 0
    OP_ADD_PORT,
#endif 
    OP_REMOVE_PORT,
    OP_CONNECT_PORTS,
    OP_DISCONNECT_PORTS,
} op_type;

/* Synchronized operation parameter. */
typedef union op_param
{
#if 0
    struct {
        unsigned port;
    } add_port;
#endif 

    struct {
        unsigned port;
    } remove_port;

    struct {
        unsigned src;
        unsigned sink;
        int adj_level;
    } connect_ports;

    struct {
        unsigned src;
        unsigned sink;
    } disconnect_ports;

} op_param;

/* Synchronized operation list entry. */
typedef struct op_entry {
    PJ_DECL_LIST_MEMBER(struct op_entry);
    op_type          type;
    op_param         param;
} op_entry;

/* Prototypes of synchronized operation */
#if 0
static void op_add_port(pjmedia_conf *conf, const op_param *prm);
#endif 
static void op_remove_port(pjmedia_conf *conf, const op_param *prm);
static void op_connect_ports(pjmedia_conf *conf, const op_param *prm);
static void op_disconnect_ports(pjmedia_conf *conf, const op_param *prm);

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
    op_entry *op = NULL;

    while (1) {

        /* Only op_queue_free and op_queue are accessed concurrently
         * and require mutex protection. 
         * The function itself cannot be invoked simultaneously from 
         * different threads, so no other data requires mutex protection.
         */
        pj_mutex_lock(conf->mutex);

        if (op)
            pj_list_push_back(conf->op_queue_free, op);

        op = conf->op_queue->next;
        if (op != conf->op_queue)
            pj_list_erase(op);

        pj_mutex_unlock(conf->mutex);

        if (op == conf->op_queue)
            break;

        pj_assert(op);

        switch(op->type) {
#if 0
            case OP_ADD_PORT:
                op_add_port(conf, &op->param);
                break;
#endif 
            case OP_REMOVE_PORT:
                op_remove_port(conf, &op->param);
                break;
            case OP_CONNECT_PORTS:
                op_connect_ports(conf, &op->param);
                break;
            case OP_DISCONNECT_PORTS:
                op_disconnect_ports(conf, &op->param);
                break;
            default:
                pj_assert(!"Invalid sync-op in conference");
                break;
        }

        op->type = OP_UNKNOWN;
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
PJ_INLINE(pj_bool_t) is_port_active(struct conf_port* p_conf_port)
{
    return p_conf_port && (p_conf_port->listener_cnt || p_conf_port->transmitter_cnt);
}

PJ_INLINE(void) correct_port_boundary(pjmedia_conf *conf, SLOT_TYPE src_slot)
{
    pj_assert(conf && src_slot < conf->max_ports);

    if (is_port_active(conf->ports[src_slot])) {

        if (src_slot >= conf->upper_bound)
            conf->upper_bound = src_slot + 1;
        if (src_slot < conf->lower_bound)
            conf->lower_bound = src_slot;

        pj_assert(conf->lower_bound < conf->upper_bound);

    } else {
        if (src_slot + 1 >= conf->upper_bound) {
            while (conf->lower_bound < conf->upper_bound && is_port_active(conf->ports[conf->upper_bound - 1])) {
                pj_assert(conf->upper_bound);
                --conf->upper_bound;
            }
        }
        if (src_slot <= conf->lower_bound) {
            while (conf->lower_bound < conf->upper_bound && !is_port_active(conf->ports[conf->lower_bound])) {
                pj_assert(conf->lower_bound < conf->max_ports);
                ++conf->lower_bound;
            }
        }
        if (conf->lower_bound >= conf->upper_bound) {
            conf->lower_bound = conf->max_ports;
            conf->upper_bound = 0;
        }
    }

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

    /* Make sure pool name is NULL terminated */
    pj_assert(name);
    pj_ansi_strxcpy2(pname, name, sizeof(pname));

    /* Create own pool */
    pool = pj_pool_create(parent_pool->factory, pname, 500, 500, NULL);
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
        conf_port->clock_rate = afd->clock_rate;
        conf_port->samples_per_frame = PJMEDIA_AFD_SPF(afd);
        conf_port->channel_count = afd->channel_count;
    } else {
        conf_port->port = NULL;
        conf_port->clock_rate = conf->clock_rate;
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
    if (conf_port->clock_rate != conf->clock_rate) {

        pj_bool_t high_quality;
        pj_bool_t large_filter;

        high_quality = ((conf->options & PJMEDIA_CONF_USE_LINEAR)==0);
        large_filter = ((conf->options & PJMEDIA_CONF_SMALL_FILTER)==0);

        /* Create resample for rx buffer. */
        status = pjmedia_resample_create( pool, 
                                          high_quality,
                                          large_filter,
                                          conf->channel_count,
                                          conf_port->clock_rate,/* Rate in */
                                          conf->clock_rate, /* Rate out */
                                          conf->samples_per_frame * 
                                            conf_port->clock_rate /
                                            conf->clock_rate,
                                          &conf_port->rx_resample);
        if (status != PJ_SUCCESS)
            goto on_return;


        /* Create resample for tx buffer. */
        status = pjmedia_resample_create(pool,
                                         high_quality,
                                         large_filter,
                                         conf->channel_count,
                                         conf->clock_rate,  /* Rate in */
                                         conf_port->clock_rate, /* Rate out */
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
    if (conf_port->clock_rate != conf->clock_rate ||
        conf_port->channel_count != conf->channel_count ||
        conf_port->samples_per_frame != conf->samples_per_frame)
    {
        unsigned port_ptime, conf_ptime, buff_ptime;

        port_ptime = conf_port->samples_per_frame / conf_port->channel_count *
            1000 / conf_port->clock_rate;
        conf_ptime = conf->samples_per_frame / conf->channel_count *
            1000 / conf->clock_rate;

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
        conf_port->rx_buf_cap = conf_port->clock_rate * buff_ptime / 1000;
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


#ifdef _OPENMP
    /* Get the bytes_per_frame value, to determine the size of the
     * buffer.
     */
    conf_port->rx_frame_buf_cap = PJMEDIA_AFD_AVG_FSZ(pjmedia_format_get_audio_format_detail(&conf->master_port->info.fmt, PJ_TRUE));
    conf_port->rx_frame_buf = (pj_int16_t *)pj_pool_zalloc(pool, conf_port->rx_frame_buf_cap);

    omp_init_lock(&conf_port->tx_Lock);
#endif //_OPENMP


    /* Done */
    *p_conf_port = conf_port;

on_return:
    if (status != PJ_SUCCESS) {
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

    /* Passive port has delay buf. */
    ptime = conf->samples_per_frame * 1000 / conf->clock_rate / 
            conf->channel_count;
    status = pjmedia_delay_buf_create(pool, name->ptr, 
                                      conf->clock_rate,
                                      conf->samples_per_frame,
                                      conf->channel_count,
                                      RX_BUF_COUNT * ptime, /* max delay */
                                      0, /* options */
                                      &conf_port->delay_buf);
    if (status != PJ_SUCCESS)
        return status;

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
            status = pjmedia_snd_port_create_player(pool, -1, conf->clock_rate,
                                                    conf->channel_count,
                                                    conf->samples_per_frame,
                                                    conf->bits_per_sample, 
                                                    0,  /* options */
                                                    &conf->snd_dev_port);

        } else {
            status = pjmedia_snd_port_create( pool, -1, -1, conf->clock_rate, 
                                              conf->channel_count, 
                                              conf->samples_per_frame,
                                              conf->bits_per_sample,
                                              0,    /* Options */
                                              &conf->snd_dev_port);

        }

        if (status != PJ_SUCCESS)
            return status;

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
#if 0
    conf->port_cnt++;   // the port will become active only when connected
#endif

    return PJ_SUCCESS;
}

/*
 * Create conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_create( pj_pool_t *pool_,
                                         unsigned max_ports,
                                         unsigned clock_rate,
                                         unsigned channel_count,
                                         unsigned samples_per_frame,
                                         unsigned bits_per_sample,
                                         unsigned options,
                                         pjmedia_conf **p_conf )
{
    pj_pool_t *pool;
    pjmedia_conf *conf;
    const pj_str_t name = { "Conf", 4 };
    pj_status_t status;

    PJ_ASSERT_RETURN(samples_per_frame > 0, PJ_EINVAL);
    /* Can only accept 16bits per sample, for now.. */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Creating conference bridge with %d ports",
              max_ports));

    /* Create own pool */
    pool = pj_pool_create(pool_->factory, name.ptr, 512, 512, NULL);
    if (!pool) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Create failed in alloc"));
        return PJ_ENOMEM;
    }

    /* Create and init conf structure. */
    conf = PJ_POOL_ZALLOC_T(pool, pjmedia_conf);
    PJ_ASSERT_RETURN(conf, PJ_ENOMEM);
    conf->pool = pool;

    conf->ports = pj_pool_calloc(pool, max_ports, sizeof(void*));
    PJ_ASSERT_ON_FAIL( conf->ports, { pjmedia_conf_destroy( conf ); return PJ_ENOMEM; } );

    conf->active_ports = pj_pool_calloc(pool, max_ports, sizeof(pj_int32_t) );
    PJ_ASSERT_ON_FAIL( conf->active_ports, { pjmedia_conf_destroy( conf ); return PJ_ENOMEM; } );

    conf->active_listener = pj_pool_calloc(pool, max_ports, sizeof(struct conf_port*));
    PJ_ASSERT_ON_FAIL( conf->active_listener, { pjmedia_conf_destroy( conf ); return PJ_ENOMEM; } );

    conf->options = options;
    conf->max_ports = max_ports;
    conf->clock_rate = clock_rate;
    conf->channel_count = channel_count;
    conf->samples_per_frame = samples_per_frame;
    conf->bits_per_sample = bits_per_sample;

    conf->lower_bound = max_ports;  // no connected ports yet
    conf->upper_bound = 0;          // no connected ports yet

    
    /* Create and initialize the master port interface. */
    conf->master_port = PJ_POOL_ZALLOC_T(pool, pjmedia_port);
    PJ_ASSERT_RETURN(conf->master_port, PJ_ENOMEM);
    
    pjmedia_port_info_init(&conf->master_port->info, &name, SIGNATURE,
                           clock_rate, channel_count, bits_per_sample,
                           samples_per_frame);

    conf->master_port->port_data.pdata = conf;
    conf->master_port->port_data.ldata = 0;

    conf->master_port->get_frame = &get_frame;
    conf->master_port->put_frame = &put_frame;
    conf->master_port->on_destroy = &destroy_port;


    /* Create port zero for sound device. */
    status = create_sound_port(pool, conf);
    if (status != PJ_SUCCESS) {
        pjmedia_conf_destroy(conf);
        return status;
    }

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


#if defined(PJ_STACK_IMPLEMENTATION)
    status = pj_stack_create( pool, &conf->unused_slots );
    if (status != PJ_SUCCESS) {
        pjmedia_conf_destroy( conf );
        return status;
    }
#else
    conf->unused_slots = PJ_POOL_ZALLOC_T(pool, unused_slots_cache);
    pj_list_init(conf->unused_slots);
#endif
    conf->free_port_slots = pj_pool_calloc(pool, max_ports, sizeof(port_slot));
    PJ_ASSERT_ON_FAIL( conf->free_port_slots, { pjmedia_conf_destroy( conf ); return PJ_ENOMEM; } );
    unsigned i = conf->max_ports;
    while (i--) {               /* prepare unused slots to later reservation, reverse order due to FILO */
        if (!conf->ports[i]) {  /* If sound device was created, skip it's slot */
            status = conf_release_port( conf, i );
            if (status != PJ_SUCCESS) {
                pjmedia_conf_destroy( conf );
                return status;
            }
        }
    }

#ifdef _OPENMP
    conf->omp_max_threads = omp_get_max_threads();
    /* Thread description's to register omp threads with pjsip */
    conf->omp_threads = (pj_thread_desc *)pj_pool_calloc( pool, 2 * conf->omp_max_threads, sizeof(pj_thread_desc) );
#endif  //_OPENMP

    /* Done */

    *p_conf = conf;

    return PJ_SUCCESS;
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

    PJ_ASSERT_RETURN(conf != NULL, PJ_EINVAL);

    pj_log_push_indent();

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
            op_param oprm = {0};
            oprm.remove_port.port = i;
            op_remove_port(conf, &oprm);
        }
    }

#if defined(PJ_STACK_IMPLEMENTATION)
    /* Destroy stack */
    if (conf->unused_slots)
        pj_stack_destroy(conf->unused_slots);
#endif

    /* Destroy mutex */
    if (conf->mutex)
        pj_mutex_destroy(conf->mutex);

    /* Destroy pool */
    if (conf->pool)
        pj_pool_safe_release(&conf->pool);

    pj_log_pop_indent();

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

    pj_assert(conf_port != NULL && !is_port_active(conf_port));

#ifdef CONF_DEBUG_EX
    conf_port->slot = index;
#endif //CONF_DEBUG_EX

#if 0
    /* Audio data flow is not protected, avoid processing this newly
     * added port.
     */
    conf_port->is_new = PJ_TRUE;

    /* Put the port, but don't add port counter yet */
    conf->ports[index] = conf_port;
    //conf->port_cnt++;
    pj_mutex_lock( conf->mutex );

    /* Queue the operation */
    op_entry *ope;
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = OP_ADD_PORT;
        ope->param.add_port.port = index;
        pj_list_push_back(conf->op_queue, ope);
        PJ_LOG(4,(THIS_FILE, "Add port %d (%.*s) queued",
                             index, (int)port_name->slen, port_name->ptr));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }
#endif //0

    /* Put the port to the reserved slot. */
    conf->ports[index] = conf_port;     /* - the port will become active only when connected
                                         * - pointer assignment is processor level atomic 
                                         */
    PJ_LOG(4,(THIS_FILE, "Added port %d (%.*s)",
              index, (int)port_name->slen, port_name->ptr));

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
#if defined(PJ_STACK_IMPLEMENTATION)
    pslot = pj_stack_pop(conf->unused_slots);
#else
    pj_mutex_lock(conf->mutex);
    if (!pj_list_empty(conf->unused_slots)) {
        pslot = conf->unused_slots->next;
        pj_list_erase(pslot);
    } else {
        pslot = NULL;
    }
    pj_mutex_unlock(conf->mutex);
#endif
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
#if defined(PJ_STACK_IMPLEMENTATION)
    return pj_stack_push( conf->unused_slots, conf->free_port_slots + slot );
#else
    pj_mutex_lock(conf->mutex);
    pj_list_push_front(conf->unused_slots, conf->free_port_slots + slot);
    pj_mutex_unlock(conf->mutex);
    return PJ_SUCCESS;
#endif
}


#if 0
static void op_add_port(pjmedia_conf *conf, const op_param *prm)
{
    unsigned port = prm->add_port.port;
    struct conf_port *cport = conf->ports[port];

    /* Port must be valid and flagged as new. */
    if (!cport || !cport->is_new)
        return;

    /* Activate newly added port */
    cport->is_new = PJ_FALSE;
#if 0
    ++conf->port_cnt;

    PJ_LOG(4,(THIS_FILE, "Added port %d (%.*s), port count=%d",
              port, (int)cport->name.slen, cport->name.ptr, conf->port_cnt));
#endif
}
#endif 


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
    struct conf_port *conf_port;
    pjmedia_port *port;
    SLOT_TYPE index;
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
    slot = conf_reserve_port(conf);
    if (slot == INVALID_SLOT) {
        pj_assert(!"Too many ports");
        return PJ_ETOOMANY;
    }
    pj_assert(slot < conf->max_ports && conf->ports[slot] == NULL);

    if (name == NULL) {
        name = &tmp;

        tmp.ptr = (char*) pj_pool_alloc(pool, 32);
        tmp.slen = pj_ansi_snprintf(tmp.ptr, 32, "ConfPort#%d", index);
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

    /* Put the port to the reserved slot. */
    conf->ports[index] = conf_port;
#if 0
    conf->port_cnt++;
#endif

    /* Done. */
    if (p_slot)
        *p_slot = index;
    if (p_port)
        *p_port = port;

    return PJ_SUCCESS;
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
        ope->type = OP_CONNECT_PORTS;
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

static void op_connect_ports(pjmedia_conf *conf, const op_param *prm)
{
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
        return;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot) {
            PJ_LOG(3,(THIS_FILE, "Ports connection %d->%d already exists",
                      src_slot, sink_slot));
            return;
        }
    }

    src_port->listener_slots[src_port->listener_cnt] = sink_slot;

    /* Set normalized adjustment level. */
    src_port->listener_adj_level[src_port->listener_cnt] =
                                prm->connect_ports.adj_level + NORMAL_LEVEL;

    ++conf->connect_cnt;
    ++src_port->listener_cnt;
    ++dst_port->transmitter_cnt;

    correct_port_boundary( conf, src_slot );
    correct_port_boundary( conf, sink_slot );
    pj_assert( conf->lower_bound < conf->upper_bound );

    PJ_LOG( 4, (THIS_FILE, "Port %d (%.*s) transmitting to port %d (%.*s)",
                src_slot,
                (int)src_port->name.slen,
                src_port->name.ptr,
                sink_slot,
                (int)dst_port->name.slen,
                dst_port->name.ptr));
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
        ope->type = OP_DISCONNECT_PORTS;
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

static void op_disconnect_ports(pjmedia_conf *conf,
                                const op_param *prm)
{
    SLOT_TYPE src_slot, sink_slot;
    struct conf_port *src_port = NULL, *dst_port = NULL;
    SLOT_TYPE i;

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
        for (i=0; i<src_port->listener_cnt; ++i) {
            if (src_port->listener_slots[i] == sink_slot)
                break;
        }
        if (i == src_port->listener_cnt) {
            PJ_LOG(3,(THIS_FILE, "Ports connection %d->%d does not exist",
                      src_slot, sink_slot));
            return;
        }

        pj_assert(src_port->listener_cnt > 0 &&
                  src_port->listener_cnt < conf->max_ports);
        pj_assert(dst_port->transmitter_cnt > 0 &&
                  dst_port->transmitter_cnt < conf->max_ports);
        pj_array_erase(src_port->listener_slots, sizeof(SLOT_TYPE),
                       src_port->listener_cnt, i);
        pj_array_erase(src_port->listener_adj_level, sizeof(unsigned),
                       src_port->listener_cnt, i);
        --conf->connect_cnt;
        --src_port->listener_cnt;
        --dst_port->transmitter_cnt;

        correct_port_boundary( conf, src_slot );
        if (src_port != dst_port)
            correct_port_boundary( conf, sink_slot );

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

            for (i = conf->lower_bound; i < conf->upper_bound; ++i) {
                int j;

                src_port = conf->ports[i];
                if (!src_port || src_port->listener_cnt == 0)
                    continue;

                /* We need to iterate backwards since the listener count
                 * can potentially decrease.
                 */
                for (j = src_port->listener_cnt - 1; j >= 0; --j) {
                    if (src_port->listener_slots[j] == sink_slot) {
                        op_param op_prm = { 0 };
                        op_prm.disconnect_ports.src = i;
                        op_prm.disconnect_ports.sink = sink_slot;
                        op_disconnect_ports( conf, &op_prm );
                        break;
                    }
                }
            }
            pj_assert( !dst_port->transmitter_cnt );
        }

    /* Disconnect multiple conn: source -> any */
    } else if (src_port) {
        if (src_port->listener_cnt) {
            int j;  /* should be signed! */
            PJ_LOG(4,(THIS_FILE,
                      "Stop any transmission from port %d (%.*s)",
                      src_slot,
                      (int)src_port->name.slen,
                      src_port->name.ptr));

            /* We need to iterate backwards since the listener count
             * will keep decreasing.
             */
            for (j = src_port->listener_cnt - 1; j >= 0; --j) {
                op_param op_prm = {0};
                op_prm.disconnect_ports.src = src_slot;
                op_prm.disconnect_ports.sink = src_port->listener_slots[j];
                op_disconnect_ports( conf, &op_prm );
            }
            pj_assert( !src_port->listener_cnt );
        }

    /* Invalid ports */
    } else {
        pj_assert(!"Invalid ports specified in conf disconnect");
    }

    /* Pause sound dev when there is no connection, the pause should be done
     * outside mutex to avoid possible deadlock.
     * Note that currently this is done with mutex, it is safe because
     * pause_sound() is a no-op (just maintaining old code).
     */
    if (conf->connect_cnt == 0) {
        pause_sound(conf);
    }
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
        ope->type = OP_DISCONNECT_PORTS;
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
        ope->type = OP_DISCONNECT_PORTS;
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
#if defined(PJ_STACK_IMPLEMENTATION)
    return conf->max_ports - pj_stack_size(conf->unused_slots);    //O(1)
#else
    return conf->max_ports - pj_list_size(conf->unused_slots);     //O(n)
#endif

#if 0
    return conf->port_cnt;
#endif
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

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = OP_REMOVE_PORT;
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


static void op_remove_port(pjmedia_conf *conf, const op_param *prm)
{
    unsigned port = prm->remove_port.port;
    struct conf_port *conf_port;
    op_param op_prm;

    /* Port must be valid. */
    conf_port = conf->ports[port];
    if (conf_port == NULL) {
        PJ_PERROR(3, (THIS_FILE, PJ_ENOTFOUND, "Remove port failed"));
        return;
    }

    conf_port->tx_setting = PJMEDIA_PORT_DISABLE;
    conf_port->rx_setting = PJMEDIA_PORT_DISABLE;

    /* disconnect port from all sources which are transmitting to it */
    pj_bzero(&op_prm, sizeof(op_prm));
    op_prm.disconnect_ports.src = INVALID_SLOT;
    op_prm.disconnect_ports.sink = port;
    op_disconnect_ports(conf, &op_prm);

    /* disconnect port from all sinks to which it is transmitting to */
    pj_bzero(&op_prm, sizeof(op_prm));
    op_prm.disconnect_ports.src = port;
    op_prm.disconnect_ports.sink = INVALID_SLOT;
    op_disconnect_ports(conf, &op_prm);

    pj_assert( !is_port_active( conf_port ) );
    /* Remove the port. */
    conf->ports[port] = NULL;
#if 0
    if (!conf_port->is_new)
        --conf->port_cnt;

    PJ_LOG(4,(THIS_FILE,"Removed port %d (%.*s), port count=%d",
              port, (int)conf_port->name.slen, conf_port->name.ptr,
              conf->port_cnt));
#endif
    PJ_LOG(4,(THIS_FILE,"Removed port %d (%.*s)",
              port, (int)conf_port->name.slen, conf_port->name.ptr));

    /* Return conf_port slot to unused slots cache. */
    conf_release_port( conf, port );

    /* Decrease conf port ref count */
    if (conf_port->port && conf_port->port->grp_lock)
        pj_grp_lock_dec_ref(conf_port->port->grp_lock);
    else
        destroy_conf_port(conf_port);
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

#ifdef _OPENMP
    if (conf_port->tx_Lock != NULL)
        omp_destroy_lock( &conf_port->tx_Lock );
#endif // _OPENMP

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
    info->clock_rate = conf_port->clock_rate;
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
                      cport->clock_rate / conf->clock_rate + 0.5);

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
        if (cport->clock_rate != conf->clock_rate) {
            
            unsigned src_count;

            TRACE_((THIS_FILE, "  resample, input count=%d", 
                    pjmedia_resample_get_input_size(cport->rx_resample)));

            pjmedia_resample_run( cport->rx_resample,cport->rx_buf, frame);

            src_count = (unsigned)(count * 1.0 * cport->clock_rate / 
                                   conf->clock_rate + 0.5);
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
              cport->last_timestamp.u64 != timestamp->u64   // no data in mix_buf
              /*(cport->transmitter_cnt == 0)*/) {

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
        cport->tx_heart_beat += conf->samples_per_frame * cport->clock_rate /
                                conf->clock_rate * 
                                cport->channel_count / conf->channel_count;

        /* Set frame timestamp */
        frame.timestamp.u64 = timestamp->u64 * cport->clock_rate /
                                conf->clock_rate;
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

    buf = (pj_int16_t*) cport->mix_buf;

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

    if (adj_level != NORMAL_LEVEL) {
        for (j=0; j<conf->samples_per_frame; ++j) {
            pj_int32_t itemp = cport->mix_buf[j];

            /* Adjust the level */
            /*itemp = itemp * adj_level / NORMAL_LEVEL;*/
            itemp = (itemp * adj_level) >> 7;

            /* Clip the signal if it's too loud */
            if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
            else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

            /* Put back in the buffer. */
            buf[j] = (pj_int16_t) itemp;

            tx_level += (buf[j]>=0? buf[j] : -buf[j]);
        }
    } else {
        for (j=0; j<conf->samples_per_frame; ++j) {
            buf[j] = (pj_int16_t) cport->mix_buf[j];
            tx_level += (buf[j]>=0? buf[j] : -buf[j]);
        }
    }

    tx_level /= conf->samples_per_frame;

    /* Convert level to 8bit complement ulaw */
    tx_level = pjmedia_linear2ulaw(tx_level) ^ 0xff;

    cport->tx_level = tx_level;

    /* If port has the same clock_rate and samples_per_frame and 
     * number of channels as the conference bridge, transmit the 
     * frame as is.
     */
    if (cport->clock_rate == conf->clock_rate &&
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
    if (cport->clock_rate != conf->clock_rate) {
        pjmedia_resample_run( cport->tx_resample, buf, 
                              cport->tx_buf + cport->tx_buf_count );
        dst_count = (unsigned)(conf->samples_per_frame * 1.0 *
                               cport->clock_rate / conf->clock_rate + 0.5);
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
            frame.timestamp.u64 = timestamp->u64 * cport->clock_rate /
                                  conf->clock_rate;

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


/*
 * Player callback.
 */
static pj_status_t get_frame(pjmedia_port *this_port, 
                             pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    pjmedia_frame_type speaker_frame_type = PJMEDIA_FRAME_TYPE_NONE;

    //parallelization requires signed int
    pj_int32_t i,
        begin, end,    /* this is lower_bound and upper_bound for conf->ports[] array */
        upper_bound;   /* this is upper_bound for conf->active_ports[] array */

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
        pj_mutex_lock(conf->mutex);
        handle_op_queue(conf);
        pj_mutex_unlock(conf->mutex);
        pj_log_pop_indent();
    }
#endif

    /* No mutex from this point! Otherwise it may cause deadlock as
     * put_frame()/get_frame() may invoke callback.
     *
     * Note that any changes on the conference connections must be
     * synchronized.
     */

    begin = conf->lower_bound;
    end = conf->upper_bound;

    conf->listener_counter = 0;
    pj_int32_t listener_counter = 0;

#if defined(_OPENMP) && defined(CONF_DEBUG)
    int threads[64] = {0};
#endif

    /* Step 1 initialization 
     * Single threaded loop to get the active_ports[] (transmitters) 
     * and active_listener[] (receivers) arrays.
     */
    for (i = begin, upper_bound = 0; i < end; ++i) {
        pj_assert( (unsigned)i < conf->max_ports );
        struct conf_port *conf_port = conf->ports[i];

        /* Skip empty port. 
         * Newly added ports are not connected yet 
         * and so we skip them as not active
         */
        if (is_port_active( conf_port ))
        {
            /* Reset auto adjustment level for mixed signal. */
            conf_port->mix_adj = NORMAL_LEVEL;
#if 0
            /* We need not reset buffer, we just want to copy the first (and probably only) frame there. */
            if (conf_port->transmitter_cnt > 1) {
                pj_bzero( conf_port->mix_buf,
                          conf->samples_per_frame * sizeof(conf_port->mix_buf[0]) );
            }
#endif
            if (conf_port->transmitter_cnt && conf_port->tx_setting != PJMEDIA_PORT_DISABLE) {

                /* In the final loop of the get_frame() function, 
                 * we might call write_port() either concurrently with 
                 * or even after the OP_PORT_REMOVE operation. 
                 * Therefore, ports that are being written to 
                 * (i.e., those with conf_port->transmitter_cnt != 0) 
                 * require protection from deletion. 
                 * For such ports, we must increment the ref_count 
                 * (and decrement it after transmission, 
                 * which may result in deferred deletion of the port).
                 * 
                 * At least "Master/sound" has no media_port.
                 * We don't manage the lifetime of such ports,
                 * their lifetime is the conference bridge lifetime.
                 */
                if (conf_port->port && conf_port->port->grp_lock)
                    pj_grp_lock_add_ref(conf_port->port->grp_lock);

                //sound device (from 0 SLOT) to 0 idx, other to next idx
                conf->active_listener[i == 0 ? 0 : ++conf->listener_counter] = conf_port;

                /* We need not reset mix_buf, we just want to copy the first 
                 * (and probably only) frame there.
                 * The criteria for "this frame is from the first transmitter"
                 *  condition is:
                 * (conf_port->last_timestamp.u64 != frame->timestamp.u64)
                 */
                if (conf_port->last_timestamp.u64 == frame->timestamp.u64)
                {   //this port have not yet received data on this timer tick
                    // enforce "this frame is from the first transmitter" condition
                    //we usually shouldn't come here
                    conf_port->last_timestamp.u64 = (frame->timestamp.u64 ? PJ_UINT64(0) : (pj_uint64_t)-1);
                }
                pj_assert( conf_port->last_timestamp.u64 != frame->timestamp.u64 );
            }

            /* Skip if we're not allowed to receive from this port. */
            if (conf_port->rx_setting == PJMEDIA_PORT_DISABLE) {
                conf_port->rx_level = 0;
                continue;
            }

            /* Also skip if this port doesn't have listeners. */
            if (conf_port->listener_cnt == 0) {
                conf_port->rx_level = 0;
                continue;
            }

            /* compacted transmitter's array should help OpenMP to distribute task throught team's threads */
            conf->active_ports[upper_bound++] = i;
            pj_assert( upper_bound <= (end - begin) );

        }
    }

    struct conf_port *sound_port = NULL;

#ifdef _OPENMP
#   pragma omp parallel for PJ_OPENMP_FOR_CLAUSES
#endif //_OPENMP
    /* Step 2
     * Get frames from all ports, and "mix" the signal
     * to mix_buf of all listeners of the port.
     * 
     * Here we use the current switching states, 
     * which must be stable during this cycle, 
     * i.e. must not change in parallel.
     * 
     * To receive frames from all ports in parallel,
     * we receive data from each port into separate 
     * buffers conf_port->rx_frame_buf
     */
    for (i = 0; i < upper_bound; ++i) {
        pj_int32_t port_idx = conf->active_ports[i];
        pj_assert( (unsigned)port_idx < conf->max_ports );
        struct conf_port *conf_port = conf->ports[port_idx];

#ifdef _OPENMP
        /* register omp thread with pjsip */
        register_omp_thread( conf );

#   ifdef CONF_DEBUG
        {
            int num = omp_get_thread_num();
            if (num < PJ_ARRAY_SIZE( threads ))
                threads[num]++;
        }
#   endif
#endif  //_OPENMP

        /* Get frame from this port.
         * For passive ports, get the frame from the delay_buf.
         * For other ports, get the frame from the port. 
         */
        if (conf_port->delay_buf != NULL) {
            pj_status_t status;

#ifdef _OPENMP
            /* Check that correct size is specified. */
            pj_assert( frame->size == conf_port->rx_frame_buf_cap );
            /* read data to different buffers to different conf_port's parallel processing */
            status = pjmedia_delay_buf_get( conf_port->delay_buf, conf_port->rx_frame_buf );
#else   //_OPENMP
            status = pjmedia_delay_buf_get( conf_port->delay_buf,
                                            (pj_int16_t *)frame->buf );
#endif  //_OPENMP
            if (status != PJ_SUCCESS) {
                conf_port->rx_level = 0;
                TRACE_EX( (THIS_FILE, "%s: No frame from the passive port (%.*s, %d, listener_cnt=%d)",
                            pj_thread_get_name( pj_thread_this() ),
                            (int)conf_port->name.slen,
                            conf_port->name.ptr,
                            port_idx, conf_port->listener_cnt) );
                continue;
            }           

        } else {

            pj_status_t status;
            pjmedia_frame_type frame_type;

#ifdef _OPENMP

            /* Check that correct size is specified. */
            pj_assert( frame->size == conf_port->rx_frame_buf_cap );
            /* read data to different buffers to different conf_port's parallel processing */
            status = read_port( conf, conf_port, conf_port->rx_frame_buf,
                                conf->samples_per_frame, &frame_type );

#else   //_OPENMP
            status = read_port( conf, conf_port, (pj_int16_t *)frame->buf,
                                conf->samples_per_frame, &frame_type );
#endif  //_OPENMP

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
                PJ_LOG( 4, (THIS_FILE, "Port %d is removed when we call get_frame()", port_idx) );
                continue;
            }

            if (status != PJ_SUCCESS) {

//#ifdef _OPENMP
                /* check status and disable port here.
                 * Prevent multiply eof callback invoke, 
                 * if fileplayer has reached EOF (i.e. status == PJ_EEOF)
                 */
                if (status == PJ_EEOF) {
                    TRACE_( (THIS_FILE, "Port %.*s reached EOF and is now disabled",
                              (int)conf_port->name.slen,
                              conf_port->name.ptr) );
                    conf_port->rx_setting = PJMEDIA_PORT_DISABLE;
                }
//#endif  //_OPENMP


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

                TRACE_EX( (THIS_FILE, "%s: No frame from the port (%.*s, %d, listener_cnt=%d)",
                            pj_thread_get_name( pj_thread_this() ),
                            (int)conf_port->name.slen,
                            conf_port->name.ptr,
                            port_idx, conf_port->listener_cnt) );

                continue;
            }

#if 0
            /* Check that the port is not removed when we call get_frame() */
            if (conf->ports[i] == NULL) {
                /* if port is removed old conf_port may point to not authorized memory */
                conf_port->rx_level = 0;
                continue;
            }
#endif      

            /* Ignore if we didn't get any frame */
            if (frame_type != PJMEDIA_FRAME_TYPE_AUDIO) {
                conf_port->rx_level = 0;
                TRACE_EX( (THIS_FILE, "%s: frame_type %d != PJMEDIA_FRAME_TYPE_AUDIO from the port (%.*s, %d, listener_cnt=%d)",
                            pj_thread_get_name( pj_thread_this() ),
                            frame_type,
                            (int)conf_port->name.slen,
                            conf_port->name.ptr,
                            port_idx, conf_port->listener_cnt) );
                continue;
            }           
        }

        pj_int32_t level = 0;
        pj_int16_t *p_in;
        unsigned j;

#ifdef _OPENMP
        p_in = conf_port->rx_frame_buf;
#else   //_OPENMP
        p_in = (pj_int16_t *)frame->buf;
#endif  //_OPENMP

        /* Adjust the RX level from this port
         * and calculate the average level at the same time.
         */
        if (conf_port->rx_adj_level != NORMAL_LEVEL) {
            for (j=0; j<conf->samples_per_frame; ++j) {
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
                itemp *= conf_port->rx_adj_level;
                itemp >>= 7;

                /* Clip the signal if it's too loud */
                if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                p_in[j] = (pj_int16_t) itemp;
                level += (p_in[j]>=0? p_in[j] : -p_in[j]);
            }
        } else {
            for (j=0; j<conf->samples_per_frame; ++j) {
                level += (p_in[j]>=0? p_in[j] : -p_in[j]);
            }
        }

        level /= conf->samples_per_frame;

        /* Convert level to 8bit complement ulaw */
        level = pjmedia_linear2ulaw(level) ^ 0xff;

        /* Put this level to port's last RX level. */
        conf_port->rx_level = level;

        // Ticket #671: Skipping very low audio signal may cause noise 
        // to be generated in the remote end by some hardphones.
        /* Skip processing frame if level is zero */
        //if (level == 0)
        //    continue;

        pj_int32_t cj, listener_cnt;  //parallelization requires signed int

        /* Add the signal to all listeners. */
        for (cj = 0, listener_cnt = conf_port->listener_cnt; cj < listener_cnt; ++cj)
        {
            struct conf_port *listener;
            pj_int16_t *p_in_conn_leveled;

            listener = conf->ports[conf_port->listener_slots[cj]];

            /* Skip if this listener doesn't want to receive audio */
            if (listener->tx_setting != PJMEDIA_PORT_ENABLE)
            {
                TRACE_EX( (THIS_FILE, "%s: listener (%.*s, %d, transmitter_cnt=%d) doesn't want to receive audio from the port (%.*s, %d, listener_cnt=%d)",
                            pj_thread_get_name( pj_thread_this() ),
                            (int)listener->name.slen,
                            listener->name.ptr,
                            conf_port->listener_slots[cj],
                            listener->transmitter_cnt,
                            (int)conf_port->name.slen,
                            conf_port->name.ptr,
                            port_idx, conf_port->listener_cnt) );
                continue;
            }

            /* apply connection level, if not normal */
            if (conf_port->listener_adj_level[cj] != NORMAL_LEVEL) {
                unsigned k = 0;
                for (; k < conf->samples_per_frame; ++k) {
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
                    itemp *= conf_port->listener_adj_level[cj];
                    itemp >>= 7;

                    /* Clip the signal if it's too loud */
                    if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                    else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                    conf_port->adj_level_buf[k] = (pj_int16_t)itemp;
                }

                /* take the leveled frame */
                p_in_conn_leveled = conf_port->adj_level_buf;
            } else {
                /* take the frame as-is */
                p_in_conn_leveled = p_in;
            }

            pj_int32_t *mix_buf;
            mix_buf = listener->mix_buf;

            if (listener->transmitter_cnt > 1) {
                /* Mixing signals,
                 * and calculate appropriate level adjustment if there is
                 * any overflowed level in the mixed signal.
                 */
                unsigned k, samples_per_frame = conf->samples_per_frame;
                pj_int32_t mix_buf_min = 0;
                pj_int32_t mix_buf_max = 0;

#ifdef _OPENMP
                //protect listener->mix_buf, listener->mix_adj, listener->last_timestamp
                omp_set_lock( &listener->tx_Lock );
#endif
                if (listener->last_timestamp.u64 == frame->timestamp.u64) {
                    //this frame is NOT from the first transmitter
                    for (k = 0; k < samples_per_frame; ++k) {
                        mix_buf[k] += p_in_conn_leveled[k]; // not the first - sum
                        if (mix_buf[k] < mix_buf_min)
                            mix_buf_min = mix_buf[k];
                        if (mix_buf[k] > mix_buf_max)
                            mix_buf_max = mix_buf[k];
                    }
                    TRACE_EX( (THIS_FILE, "%s: listener (%.*s, %d, transmitter_cnt=%d) get (sum) audio from the port (%.*s, %d, listener_cnt=%d)",
                                pj_thread_get_name( pj_thread_this() ),
                                (int)listener->name.slen,
                                listener->name.ptr,
                                conf_port->listener_slots[cj],
                                listener->transmitter_cnt,
                                (int)conf_port->name.slen,
                                conf_port->name.ptr,
                                port_idx, conf_port->listener_cnt) );

                } else {
                    //this frame is from the first transmitter
                    listener->last_timestamp = frame->timestamp;

                    /* We do not want to reset buffer, we just copy the first frame there. */
                    for (k = 0; k < samples_per_frame; ++k) {
                        mix_buf[k] = p_in_conn_leveled[k]; // the first - copy
                        if (mix_buf[k] < mix_buf_min)
                            mix_buf_min = mix_buf[k];
                        if (mix_buf[k] > mix_buf_max)
                            mix_buf_max = mix_buf[k];
                    }
                    TRACE_EX( (THIS_FILE, "%s: listener %p (%.*s, %d, transmitter_cnt=%d) get (copy) audio from the port %p (%.*s, %d, listener_cnt=%d)",
                                pj_thread_get_name( pj_thread_this() ),
                                listener,
                                (int)listener->name.slen,
                                listener->name.ptr,
                                conf_port->listener_slots[cj],
                                listener->transmitter_cnt,
                                conf_port,
                                (int)conf_port->name.slen,
                                conf_port->name.ptr,
                                port_idx, conf_port->listener_cnt) );
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
#ifdef _OPENMP
                omp_unset_lock( &listener->tx_Lock );
#endif
            } else {
                //this frame is from the only transmitter
                pj_assert( listener->transmitter_cnt == 1 && listener->last_timestamp.u64 != frame->timestamp.u64 );
                listener->last_timestamp = frame->timestamp;

                /* Only 1 transmitter:
                 * just copy the samples to the mix buffer
                 * no mixing and level adjustment needed
                 */
                unsigned k, samples_per_frame = conf->samples_per_frame;

                for (k = 0; k < samples_per_frame; ++k) {
                    mix_buf[k] = p_in_conn_leveled[k];  // here copying 16 bit value to 32 bit dst
                }
                TRACE_EX( (THIS_FILE, "%s: listener %p (%.*s, %d, transmitter_cnt=%d)"
                            " get audio from the (only) port %p (%.*s, %d, listener_cnt=%d) last_timestamp=%llu, timestamp=%llu",
                            pj_thread_get_name( pj_thread_this() ),
                            listener,
                            (int)listener->name.slen,
                            listener->name.ptr,
                            listener->slot,
                            listener->transmitter_cnt,
                            conf_port,
                            (int)conf_port->name.slen,
                            conf_port->name.ptr,
                            conf_port->slot, conf_port->listener_cnt,
                            listener->last_timestamp.u64, frame->timestamp.u64) );

            }

        } /* loop the listeners of conf port */
    } /* loop of all conf ports */

//#if 0
    /* all ports have data in their buffers
     * and may do all work independently.
     * We must garantee the lifetime of
     * conf_port see listener->ref_counter
     * and may release mutex now.        */
//    pj_mutex_unlock( conf->mutex );
//#endif

    listener_counter = conf->listener_counter + 1;


#ifdef _OPENMP
#   pragma omp parallel
    {
#endif //_OPENMP
        /* Perform any queued operations that need to be synchronized with
         * the clock such as connect, disonnect, remove.
         * All those operations performed on the single thread
         * but perhaps not on the master thread
         */
#ifdef _OPENMP
#       pragma omp single nowait
        {
#endif //_OPENMP
            if (!pj_list_empty(conf->op_queue)) {

#ifdef _OPENMP
                /* register omp thread with pjsip */
                register_omp_thread(conf);
#   ifdef CONF_DEBUG
                {
                    int num = omp_get_thread_num();
                    if (num < 16)
                        threads[num]++;
                }
#   endif
#endif //_OPENMP

                pj_log_push_indent();
                /* Calling any callback while a mutex is locked can result in a deadlock,
                 * since operations can lock other mutexes in an arbitrary order.
                 * At least OP_REMOVE_PORT invokes grp_lock handlers callbacks.
                 * We should move lock inside each operations
                 */
                 //pj_mutex_lock(conf->mutex);
                handle_op_queue(conf);
                //pj_mutex_unlock(conf->mutex);
                pj_log_pop_indent();
            }
#ifdef _OPENMP
        }   //pragma omp single
#endif //_OPENMP

        /* Step 3
         * Time for all ports to transmit whatever they have in their
         * buffer.
         */
#ifdef _OPENMP
#       pragma omp for nowait
#endif //_OPENMP
        for (i = 0; i < listener_counter; ++i) {

            pjmedia_frame_type frm_type;
            pj_status_t status;

            //sound device (from port[0]) has 0 idx here too
            struct conf_port* conf_port = conf->active_listener[i];
            conf->active_listener[i] = NULL;
            if (!conf_port)
                continue;

#ifdef _OPENMP
            /* register omp thread with pjsip */
            register_omp_thread(conf);
#   ifdef CONF_DEBUG
            {
                int num = omp_get_thread_num();
                if (num < 16)
                    threads[num]++;
            }
#   endif
#endif //_OPENMP


            status = write_port(conf, conf_port, &frame->timestamp,
                                &frm_type);

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
            if (status == PJ_SUCCESS && i == 0) {
                speaker_frame_type = frm_type;
                sound_port = conf_port;
            }

            //At least "Master/sound" may have no media_port
            if (conf_port->port && conf_port->port->grp_lock)
                pj_grp_lock_dec_ref(conf_port->port->grp_lock);// conf_port may be destroyed here

        }

#ifdef _OPENMP
    }  //pragma omp parallel
#endif //_OPENMP

    /* Return sound playback frame. */
    if (sound_port != NULL) {
        if (sound_port->tx_level) {
            TRACE_( (THIS_FILE, "write to audio, count=%d",
                      conf->samples_per_frame) );
            pjmedia_copy_samples( (pj_int16_t *)frame->buf,
                                  (const pj_int16_t *)sound_port->mix_buf,
                                  conf->samples_per_frame );
        } else {
            /* Force frame type NONE */
            speaker_frame_type = PJMEDIA_FRAME_TYPE_NONE;
        }
    }

    /* MUST set frame type */
    frame->type = speaker_frame_type;

#ifdef _OPENMP

    TRACE_( (THIS_FILE, "Ports processed by omp team's threads 0:%d, 1:%d, 2:%d, 3:%d, 4:%d, 5:%d, 6:%d, 7:%d, 8:%d, 9:%d, 10:%d, 11:%d, 12:%d, 13:%d, 14:%d, 15:%d.",
              threads[0], threads[1], threads[2], threads[3], threads[4], threads[5], threads[6], threads[7],
              threads[8], threads[9], threads[10], threads[11], threads[12], threads[13], threads[14], threads[15]) );

#endif

#ifdef REC_FILE
    if (fhnd_rec == NULL)
        fhnd_rec = fopen(REC_FILE, "wb");
    if (fhnd_rec)
        fwrite(frame->buf, frame->size, 1, fhnd_rec);
#endif

    return PJ_SUCCESS;
}

#ifdef _OPENMP

/* register omp thread with pjsip */
static void register_omp_thread( pjmedia_conf * conf )
{
    if (!pj_thread_is_registered())
    {
        pj_thread_t *p;
        int num;
#pragma omp critical (register_conf_bridge_omp_thread)
        {
            num = conf->omp_threads_idx++;
        }
        pj_assert( num < 2 * conf->omp_max_threads );
        char        obj_name[PJ_MAX_OBJ_NAME];
        pj_ansi_snprintf( obj_name, sizeof(obj_name), "omp_conf_%d", num );
        pj_thread_register( obj_name, conf->omp_threads[num], &p );
    }
}
#endif //_OPENMP


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

#endif

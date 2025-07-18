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

#if PJMEDIA_CONF_BACKEND == PJMEDIA_CONF_SERIAL_BRIDGE_BACKEND

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
    pj_pool_t           *pool;          /**< Pool.                          */
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

    pj_bool_t            is_new;        /**< Newly added port, avoid read/write
                                             data from/to.                  */
};


/* Forward declarations */
typedef struct op_entry op_entry;


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
    unsigned              clock_rate;   /**< Sampling rate.                 */
    unsigned              channel_count;/**< Number of channels (1=mono).   */
    unsigned              samples_per_frame;    /**< Samples per frame.     */
    unsigned              bits_per_sample;      /**< Bits per sample.       */

    op_entry             *op_queue;     /**< Queue of operations.           */
    op_entry             *op_queue_free;/**< Queue of free entries.         */
    pjmedia_conf_op_cb    cb;           /**< OP callback.                   */
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
    if (conf_port->pool)
        pj_pool_safe_release(&conf_port->pool);
}


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
    if (!pool)
        return PJ_ENOMEM;

    /* Create port. */
    conf_port = PJ_POOL_ZALLOC_T(pool, struct conf_port);
    PJ_ASSERT_ON_FAIL(conf_port, {pj_pool_release(pool); return PJ_ENOMEM;});
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

        /* Check compatibility of sample rate and ptime.
         * Some combinations result in a fractional number of samples per frame
         * which we do not support.
         * One such case would be for example 10ms @ 22050Hz which would yield
         * 220.5 samples per frame.
         */
        if (0 != (port_ptime * conf_port->clock_rate *
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


    /* Done */
    *p_conf_port = conf_port;

on_return:
    if (status != PJ_SUCCESS) {
        /* Destroy resample if this conf port has it. */
        if (conf_port && conf_port->rx_resample)
            pjmedia_resample_destroy(conf_port->rx_resample);

        if (conf_port && conf_port->tx_resample)
            pjmedia_resample_destroy(conf_port->tx_resample);

        if (port && port->grp_lock) {
            pj_grp_lock_dec_ref(port->grp_lock);
        } else if (pool) {
            pj_pool_release(pool);
        }
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

    //TODO: potential bug! Is this intended? delay_buf is created in 
    //a different pool than conf_port. need poll = conf_port->pool?

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
        //TODO: resource leak! Newly created conf_port is leaked here.
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
            //TODO: resource leak! Newly created conf_port is leaked here.
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


     /* Add the port to the bridge */
    conf->ports[0] = conf_port;
    conf->port_cnt++;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_conf_create2(pj_pool_t *pool,
                                         pjmedia_conf_param *param,
                                         pjmedia_conf **p_conf)
{
    PJ_ASSERT_RETURN(param, PJ_EINVAL);
    return pjmedia_conf_create(pool, 
                               param->max_slots, param->sampling_rate, 
                               param->channel_count, param->samples_per_frame, 
                               param->bits_per_sample, param->options, p_conf);
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

    conf->ports = (struct conf_port**) 
                  pj_pool_zalloc(pool, max_ports*sizeof(void*));
    PJ_ASSERT_RETURN(conf->ports, PJ_ENOMEM);

    conf->options = options;
    conf->max_ports = max_ports;
    conf->clock_rate = clock_rate;
    conf->channel_count = channel_count;
    conf->samples_per_frame = samples_per_frame;
    conf->bits_per_sample = bits_per_sample;

    
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

    PJ_LOG(5, (THIS_FILE, "Audio conference bridge destroy requested"));

    pj_log_push_indent();

    /* Destroy sound device port. */
    if (conf->snd_dev_port) {
        pjmedia_snd_port_destroy(conf->snd_dev_port);
        conf->snd_dev_port = NULL;
    }

    /* Flush any pending operation (connect, disconnect, etc) */
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

    /* Destroy mutex */
    if (conf->mutex)
        pj_mutex_destroy(conf->mutex);

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
    unsigned index;
    op_entry *ope;
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

    pj_mutex_lock(conf->mutex);

    /* Find empty port in the conference bridge. */
    for (index=0; index < conf->max_ports; ++index) {
        if (conf->ports[index] == NULL)
            break;
    }

    if (index == conf->max_ports) {
        PJ_PERROR(3,(THIS_FILE, PJ_ETOOMANY, "Add port %s failed",
                     port_name->ptr));
        status = PJ_ETOOMANY;
        goto on_return;
    }

    /* Create conf port structure. */
    status = create_conf_port(pool, conf, strm_port, port_name, &conf_port);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Audio data flow is not protected, avoid processing this newly
     * added port.
     */
    conf_port->is_new = PJ_TRUE;

    /* Put the port, but don't add port counter yet */
    conf->ports[index] = conf_port;
    //conf->port_cnt++;

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_ADD_PORT;
        ope->param.add_port.port = index;
        pj_list_push_back(conf->op_queue, ope);
        PJ_LOG(4,(THIS_FILE, "Add port %d (%.*s) queued",
                             index, (int)port_name->slen, port_name->ptr));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Done. */
    if (p_port) {
        *p_port = index;
    }

on_return:
    pj_mutex_unlock(conf->mutex);
    pj_log_pop_indent();

    return status;
}


static pj_status_t op_add_port(pjmedia_conf *conf,
                               const pjmedia_conf_op_param *prm)
{
    unsigned port = prm->add_port.port;
    struct conf_port *cport = conf->ports[port];

    /* Port must be valid and flagged as new. */
    if (!cport || !cport->is_new)
        return PJ_EINVAL;

    /* Activate newly added port */
    cport->is_new = PJ_FALSE;
    ++conf->port_cnt;

    PJ_LOG(4,(THIS_FILE, "Added port %d (%.*s), port count=%d",
              port, (int)cport->name.slen, cport->name.ptr, conf->port_cnt));

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
    struct conf_port *conf_port;
    pjmedia_port *port;
    unsigned index;
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

    pj_mutex_lock(conf->mutex);

    if (conf->port_cnt >= conf->max_ports) {
        pj_assert(!"Too many ports");
        pj_mutex_unlock(conf->mutex);
        return PJ_ETOOMANY;
    }

    /* Find empty port in the conference bridge. */
    for (index=0; index < conf->max_ports; ++index) {
        if (conf->ports[index] == NULL)
            break;
    }

    pj_assert(index != conf->max_ports);

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
        pj_mutex_unlock(conf->mutex);
        return status;
    }


    /* Put the port. */
    conf->ports[index] = conf_port;
    conf->port_cnt++;

    /* Done. */
    if (p_slot)
        *p_slot = index;
    if (p_port)
        *p_port = port;

    pj_mutex_unlock(conf->mutex);

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
                                    const pjmedia_conf_op_param *prm)
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
    ++src_port->listener_cnt;
    ++dst_port->transmitter_cnt;

    PJ_LOG(4,(THIS_FILE,"Port %d (%.*s) transmitting to port %d (%.*s)",
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
    unsigned src_slot, sink_slot;
    struct conf_port *src_port = NULL, *dst_port = NULL;
    int i;

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
        for (i=0; i<(int)src_port->listener_cnt; ++i) {
            if (src_port->listener_slots[i] == sink_slot)
                break;
        }
        if (i == (int)src_port->listener_cnt) {
            PJ_LOG(3,(THIS_FILE, "Ports connection %d->%d does not exist",
                      src_slot, sink_slot));
            return PJ_EINVAL;
        }

        pj_assert(src_port->listener_cnt > 0 &&
                  src_port->listener_cnt <= conf->max_ports);
        pj_assert(dst_port->transmitter_cnt > 0 &&
                  dst_port->transmitter_cnt <= conf->max_ports);
        pj_array_erase(src_port->listener_slots, sizeof(SLOT_TYPE),
                       src_port->listener_cnt, i);
        pj_array_erase(src_port->listener_adj_level, sizeof(unsigned),
                       src_port->listener_cnt, i);
        --conf->connect_cnt;
        --src_port->listener_cnt;
        --dst_port->transmitter_cnt;

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
        PJ_LOG(4,(THIS_FILE,
                  "Stop any transmission to port %d (%.*s)",
                  sink_slot,
                  (int)dst_port->name.slen,
                  dst_port->name.ptr));

        for (i=0; i<(int)conf->max_ports; ++i) {
            int j;

            src_port = conf->ports[i];
            if (!src_port || src_port->listener_cnt == 0)
                continue;

            /* We need to iterate backwards since the listener count
             * can potentially decrease.
             */
            for (j=src_port->listener_cnt-1; j>=0; --j) {
                if (src_port->listener_slots[j] == sink_slot) {
                    pj_status_t status;
                    pjmedia_conf_op_param op_prm = {0};

                    op_prm.disconnect_ports.src = i;
                    op_prm.disconnect_ports.sink = sink_slot;
                    status = op_disconnect_ports(conf, &op_prm);
                    if (status != PJ_SUCCESS) {
                        PJ_PERROR(4, (THIS_FILE, status,
                                      "Fail to stop transmission from port "
                                      "%d to port %d", 
                                      i, sink_slot));
                    }
                    break;
                }
            }
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
        for (i=src_port->listener_cnt-1; i>=0; --i) {
            pj_status_t status;
            pjmedia_conf_op_param op_prm = {0};

            op_prm.disconnect_ports.src = src_slot;
            op_prm.disconnect_ports.sink = src_port->listener_slots[i];
            status = op_disconnect_ports(conf, &op_prm);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4, (THIS_FILE, status,
                              "Fail to stop transmission from port "
                              "%d to port %d",
                              src_slot, src_port->listener_slots[i]));
            }
        }

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

        if (conf_port->port)
            pjmedia_port_destroy(conf_port->port);
        conf_port->port = NULL;
    }

    /* Remove the port. */
    pj_mutex_lock(conf->mutex);
    conf->ports[port] = NULL;
    pj_mutex_unlock(conf->mutex);

    if (!conf_port->is_new)
        --conf->port_cnt;

    PJ_LOG(4,(THIS_FILE,"Removed port %d (%.*s), port count=%d",
              port, (int)conf_port->name.slen, conf_port->name.ptr,
              conf->port_cnt));

    /* Decrease conf port ref count */
    if (conf_port->port && conf_port->port->grp_lock)
        pj_grp_lock_dec_ref(conf_port->port->grp_lock);
    else
        conf_port_on_destroy(conf_port);

    return PJ_SUCCESS;
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
             (cport->transmitter_cnt == 0)) {
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

            TRACE_((THIS_FILE, "put_frame %.*s, count=%d", 
                               (int)cport->name.slen, cport->name.ptr,
                               frame.size / BYTES_PER_SAMPLE));

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
    unsigned ci, cj, i, j;
    pj_int16_t *p_in;
    
    TRACE_((THIS_FILE, "- clock -"));

    /* Check that correct size is specified. */
    pj_assert(frame->size == conf->samples_per_frame *
                             conf->bits_per_sample / 8);

    /* Perform any queued operations that need to be synchronized with
     * the clock such as connect, disonnect, remove.
     */
    if (!pj_list_empty(conf->op_queue)) {
        pj_log_push_indent();
        handle_op_queue(conf);
        pj_log_pop_indent();
    }

    /* No mutex from this point! Otherwise it may cause deadlock as
     * put_frame()/get_frame() may invoke callback.
     *
     * Note that any changes on the conference connections must be
     * synchronized.
     */

    /* Reset port source count. We will only reset port's mix
     * buffer when we have someone transmitting to it.
     */
    for (i=0, ci=0; i<conf->max_ports && ci < conf->port_cnt; ++i) {
        struct conf_port *conf_port = conf->ports[i];

        /* Skip empty or new port. */
        if (!conf_port || conf_port->is_new)
            continue;

        /* Var "ci" is to count how many ports have been visited so far. */
        ++ci;

        /* Skip if we're not allowed to transmit to this port. */
        if (conf_port->tx_setting != PJMEDIA_PORT_ENABLE)
            continue;

        /* Reset buffer (only necessary if the port has transmitter) and
         * reset auto adjustment level for mixed signal.
         */
        conf_port->mix_adj = NORMAL_LEVEL;
        if (conf_port->transmitter_cnt) {
            pj_bzero(conf_port->mix_buf,
                     conf->samples_per_frame*sizeof(conf_port->mix_buf[0]));
        }
    }

    /* Get frames from all ports, and "mix" the signal 
     * to mix_buf of all listeners of the port.
     */
    for (i=0, ci=0; i < conf->max_ports && ci < conf->port_cnt; ++i) {
        struct conf_port *conf_port = conf->ports[i];
        pj_int32_t level = 0;

        /* Skip empty or new port. */
        if (!conf_port || conf_port->is_new)
            continue;

        /* Var "ci" is to count how many ports have been visited so far. */
        ++ci;

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

        /* Get frame from this port.
         * For passive ports, get the frame from the delay_buf.
         * For other ports, get the frame from the port. 
         */
        if (conf_port->delay_buf != NULL) {
            pj_status_t status;
        
            status = pjmedia_delay_buf_get(conf_port->delay_buf,
                                  (pj_int16_t*)frame->buf);
            if (status != PJ_SUCCESS) {
                conf_port->rx_level = 0;
                continue;
            }           

        } else {

            pj_status_t status;
            pjmedia_frame_type frame_type;

            status = read_port(conf, conf_port, (pj_int16_t*)frame->buf, 
                               conf->samples_per_frame, &frame_type);
            
            if (status != PJ_SUCCESS) {
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
                continue;
            }

            /* Check that the port is not removed when we call get_frame() */
            if (conf->ports[i] == NULL) {
                conf_port->rx_level = 0;
                continue;
            }
                

            /* Ignore if we didn't get any frame */
            if (frame_type != PJMEDIA_FRAME_TYPE_AUDIO) {
                conf_port->rx_level = 0;
                continue;
            }           
        }

        p_in = (pj_int16_t*) frame->buf;

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

        /* Add the signal to all listeners. */
        for (cj=0; cj < conf_port->listener_cnt; ++cj) 
        {
            struct conf_port *listener;
            pj_int32_t *mix_buf;            
            pj_int16_t *p_in_conn_leveled;

            listener = conf->ports[conf_port->listener_slots[cj]];

            /* Skip if this listener doesn't want to receive audio */
            if (listener->tx_setting != PJMEDIA_PORT_ENABLE)
                continue;

            mix_buf = listener->mix_buf;

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

            if (listener->transmitter_cnt > 1) {
                /* Mixing signals,
                 * and calculate appropriate level adjustment if there is
                 * any overflowed level in the mixed signal.
                 */
                unsigned k, samples_per_frame = conf->samples_per_frame;
                pj_int32_t mix_buf_min = 0;
                pj_int32_t mix_buf_max = 0;

                for (k = 0; k < samples_per_frame; ++k) {
                    mix_buf[k] += p_in_conn_leveled[k];
                    if (mix_buf[k] < mix_buf_min)
                        mix_buf_min = mix_buf[k];
                    if (mix_buf[k] > mix_buf_max)
                        mix_buf_max = mix_buf[k];
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
                /* Only 1 transmitter:
                 * just copy the samples to the mix buffer
                 * no mixing and level adjustment needed
                 */
                unsigned k, samples_per_frame = conf->samples_per_frame;

                for (k = 0; k < samples_per_frame; ++k) {
                    mix_buf[k] = p_in_conn_leveled[k];
                }
            }
        } /* loop the listeners of conf port */
    } /* loop of all conf ports */

    /* Time for all ports to transmit whetever they have in their
     * buffer. 
     */
    for (i=0, ci=0; i<conf->max_ports && ci<conf->port_cnt; ++i) {
        struct conf_port *conf_port = conf->ports[i];
        pjmedia_frame_type frm_type;
        pj_status_t status;

        if (!conf_port || conf_port->is_new)
            continue;

        /* Var "ci" is to count how many ports have been visited. */
        ++ci;

        status = write_port( conf, conf_port, &frame->timestamp,
                             &frm_type);
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

        /* Set the type of frame to be returned to sound playback
         * device.
         */
        if (i == 0)
            speaker_frame_type = frm_type;
    }

    /* Return sound playback frame. */
    if (conf->ports[0]->tx_level) {
        TRACE_((THIS_FILE, "write to audio, count=%d", 
                           conf->samples_per_frame));
        pjmedia_copy_samples( (pj_int16_t*)frame->buf, 
                              (const pj_int16_t*)conf->ports[0]->mix_buf, 
                              conf->samples_per_frame);
    } else {
        /* Force frame type NONE */
        speaker_frame_type = PJMEDIA_FRAME_TYPE_NONE;
    }

    /* MUST set frame type */
    frame->type = speaker_frame_type;

#ifdef REC_FILE
    if (fhnd_rec == NULL)
        fhnd_rec = fopen(REC_FILE, "wb");
    if (fhnd_rec)
        fwrite(frame->buf, frame->size, 1, fhnd_rec);
#endif

    return PJ_SUCCESS;
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


#endif   /* PJMEDIA_CONF_BACKEND == PJMEDIA_CONF_SERIAL_BRIDGE_BACKEND */

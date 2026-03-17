/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/ai_port.h>
#include <pjmedia/silencedet.h>
#include <pjmedia/circbuf.h>
#include <pjmedia/errno.h>
#include <pjmedia/resample.h>
#include <pj/assert.h>
#include <pj/file_io.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE           "ai_port.c"
#define SIGNATURE           PJMEDIA_SIG_PORT_AI

/* Set to 1 to dump raw RX JSON messages to ai_rx_log.json */
#ifndef AI_PORT_DUMP_RX_JSON
#   define AI_PORT_DUMP_RX_JSON 0
#endif

/* TX circular buffer: ~500ms at native rate */
#define TX_BUF_MSEC         500

/* TX flush interval in milliseconds. Audio frames are accumulated in
 * the TX circular buffer and flushed as a single WebSocket message
 * every TX_FLUSH_MSEC. Reduces message overhead vs sending every
 * 20ms frame individually.
 */
#define TX_FLUSH_MSEC       100

/* RX circular buffer: ~10s at native rate.
 * AI services deliver audio much faster than real-time (a 5s response can
 * arrive in <1s). The buffer must be large enough to absorb the entire
 * burst without dropping samples.
 */
#define RX_BUF_MSEC         10000

/* Encode buffer size: base64 overhead + JSON wrapper for ~500ms audio */
#define ENCODE_BUF_SIZE     65536

/* Decode buffer: max samples from a single RX message (~1s at 24kHz).
 * OpenAI delta chunks can be up to ~50KB base64 (~18750 samples).
 */
#define DECODE_BUF_SAMPLES  24000

/* Pre-buffer threshold in milliseconds. Delay audio output until this
 * much audio is buffered, to absorb network jitter and frame
 * accumulation delays. Once playback starts, it continues even if
 * the level drops below the threshold.
 */
#define PREBUF_MSEC         500

/**
 * AI port state.
 */
typedef enum ai_port_state
{
    AI_STATE_IDLE,
    AI_STATE_CONNECTING,
    AI_STATE_CONNECTED,
    AI_STATE_DISCONNECTING
} ai_port_state;

/**
 * AI media port structure.
 */
struct pjmedia_ai_port
{
    pjmedia_port             base;
    pj_pool_t               *pool;

    /* Parameters */
    pjmedia_ai_port_cb       cb;
    void                    *user_data;
    pjmedia_ai_backend      *backend;

    /* WebSocket */
    pj_websock              *ws;
    ai_port_state            state;

    /* Circular buffers (samples at backend native rate) */
    pjmedia_circ_buf        *tx_buf;
    pjmedia_circ_buf        *rx_buf;

    /* Encode buffer for outgoing JSON messages */
    char                    *encode_buf;

    /* Decode buffer for incoming audio samples (RX path) */
    pj_int16_t              *decode_buf;

    /* Read buffer for outgoing audio samples (TX path) */
    pj_int16_t              *tx_read_buf;

    /* Temporary pool for JSON parsing */
    pj_pool_t               *tmp_pool;

    /* Send state */
    pj_bool_t                send_pending;

    /* RX pre-buffer: delay output until threshold is reached */
    pj_bool_t                rx_started;
    unsigned                 prebuf_samples;

    /* TX flush threshold: accumulate this many samples before sending */
    unsigned                 tx_flush_samples;

    /* TX VAD: skip sending silence frames over WebSocket */
    pjmedia_silence_det     *vad;

#if AI_PORT_DUMP_RX_JSON
    /* Debug: JSON log file */
    pj_oshandle_t            log_file;
#endif
};


/* Forward declarations */
static pj_status_t ai_port_put_frame(pjmedia_port *this_port,
                                     pjmedia_frame *frame);
static pj_status_t ai_port_get_frame(pjmedia_port *this_port,
                                     pjmedia_frame *frame);
static pj_status_t ai_port_on_destroy(pjmedia_port *this_port);

/* WebSocket callbacks */
static void on_ws_connect(pj_websock *ws, pj_status_t status);
static void on_ws_rx_msg(pj_websock *ws, pj_websock_opcode opcode,
                         const void *data, pj_size_t len);
static void on_ws_close(pj_websock *ws, pj_uint16_t status_code,
                        const pj_str_t *reason);


/*
 * Flush TX circular buffer: read available samples, encode, and send.
 * Only flushes when enough samples have accumulated (TX_FLUSH_MSEC
 * worth), unless force is PJ_TRUE (e.g. on disconnect).
 * Must be called with grp_lock held.
 */
static void flush_tx_buf(pjmedia_ai_port *aip, pj_bool_t force)
{
    unsigned avail;
    pj_status_t status;
    int enc_len;

    if (aip->state != AI_STATE_CONNECTED || aip->send_pending)
        return;

    avail = pjmedia_circ_buf_get_len(aip->tx_buf);
    if (avail == 0)
        return;

    /* Wait until we have enough samples, unless forced */
    if (!force && avail < aip->tx_flush_samples)
        return;

    /* Read samples from circular buffer into TX-specific buffer */
    if (avail > DECODE_BUF_SAMPLES)
        avail = DECODE_BUF_SAMPLES;

    status = pjmedia_circ_buf_read(aip->tx_buf, aip->tx_read_buf, avail);
    if (status != PJ_SUCCESS)
        return;

    /* Encode via backend */
    enc_len = ENCODE_BUF_SIZE;
    status = aip->backend->op->encode_audio(aip->backend,
                                            aip->tx_read_buf, avail,
                                            aip->encode_buf, &enc_len);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4, (THIS_FILE, status, "encode_audio failed"));
        return;
    }

    PJ_LOG(6, (THIS_FILE, "TX: %u samples, %d bytes encoded",
               avail, enc_len));

    /* Release lock before sending to avoid holding lock during I/O */
    pj_grp_lock_release(aip->base.grp_lock);

    status = pj_websock_send(aip->ws, PJ_WEBSOCK_OP_TEXT,
                             aip->encode_buf, (pj_size_t)enc_len);

    pj_grp_lock_acquire(aip->base.grp_lock);

    if (status == PJ_SUCCESS) {
        aip->send_pending = PJ_FALSE;
    } else if (status == PJ_EPENDING) {
        aip->send_pending = PJ_TRUE;
    } else {
        PJ_PERROR(4, (THIS_FILE, status, "websock send failed"));
    }
}


/*
 * put_frame: receive audio from conference bridge, buffer and send.
 */
static pj_status_t ai_port_put_frame(pjmedia_port *this_port,
                                     pjmedia_frame *frame)
{
    pjmedia_ai_port *aip = (pjmedia_ai_port*)this_port;
    pj_int16_t *samples;
    unsigned sample_count;

    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO || frame->size == 0)
        return PJ_SUCCESS;

    pj_grp_lock_acquire(aip->base.grp_lock);

    if (aip->state != AI_STATE_CONNECTED) {
        pj_grp_lock_release(aip->base.grp_lock);
        return PJ_SUCCESS;
    }

    samples = (pj_int16_t*)frame->buf;
    sample_count = (unsigned)(frame->size / 2);

    /* VAD: skip silence to save bandwidth */
    if (aip->vad) {
        pj_bool_t is_silence;

        is_silence = pjmedia_silence_det_detect(aip->vad, samples,
                                                sample_count, NULL);
        if (is_silence) {
            /* Flush any accumulated speech before going silent */
            flush_tx_buf(aip, PJ_TRUE);
            pj_grp_lock_release(aip->base.grp_lock);
            return PJ_SUCCESS;
        }
    }

    /* Write to TX circular buffer, drop oldest if full */
    if (pjmedia_circ_buf_get_len(aip->tx_buf) +
        sample_count > aip->tx_buf->capacity)
    {
        unsigned to_discard = pjmedia_circ_buf_get_len(aip->tx_buf) +
                              sample_count - aip->tx_buf->capacity;
        pjmedia_circ_buf_adv_read_ptr(aip->tx_buf, to_discard);
    }
    pjmedia_circ_buf_write(aip->tx_buf, samples, sample_count);

    /* Try to flush */
    flush_tx_buf(aip, PJ_FALSE);

    pj_grp_lock_release(aip->base.grp_lock);
    return PJ_SUCCESS;
}


/*
 * get_frame: provide audio from AI service to conference bridge.
 *
 * Uses pre-buffering: delay output until PREBUF_MSEC worth of audio
 * is buffered to absorb network jitter.  Once playback has started,
 * continue delivering even if the level drops below the threshold
 * (the buffer will refill because AI audio arrives faster than
 * real-time). Reset when the buffer drains completely (response ended).
 */
static pj_status_t ai_port_get_frame(pjmedia_port *this_port,
                                     pjmedia_frame *frame)
{
    pjmedia_ai_port *aip = (pjmedia_ai_port*)this_port;
    unsigned port_spf;
    pj_int16_t *dst;
    unsigned avail;

    port_spf = PJMEDIA_PIA_SPF(&aip->base.info);
    dst = (pj_int16_t*)frame->buf;

    pj_grp_lock_acquire(aip->base.grp_lock);

    avail = pjmedia_circ_buf_get_len(aip->rx_buf);

    /* Decide the minimum fill level to start delivering */
    if (!aip->rx_started) {
        if (avail >= aip->prebuf_samples) {
            aip->rx_started = PJ_TRUE;
            PJ_LOG(5, (THIS_FILE, "RX prebuf reached (%u samples), "
                       "starting playback", avail));
        }
    } else {
        /* Reset pre-buffer state when buffer drains completely
         * (i.e. between AI responses).
         */
        if (avail == 0)
            aip->rx_started = PJ_FALSE;
    }

    if (aip->rx_started && avail >= port_spf) {
        pjmedia_circ_buf_read(aip->rx_buf, dst, port_spf);
        pj_grp_lock_release(aip->base.grp_lock);

        frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame->size = port_spf * 2;
        return PJ_SUCCESS;
    }

    pj_grp_lock_release(aip->base.grp_lock);

    /* Not enough data or pre-buffering, return silence */
    frame->type = PJMEDIA_FRAME_TYPE_NONE;
    return PJ_SUCCESS;
}


/*
 * WebSocket on_connect callback.
 */
static void on_ws_connect(pj_websock *ws, pj_status_t status)
{
    pjmedia_ai_port *aip;
    pjmedia_ai_event ev;

    aip = (pjmedia_ai_port*)pj_websock_get_user_data(ws);
    PJ_ASSERT_ON_FAIL(aip != NULL, return);

    pj_grp_lock_acquire(aip->base.grp_lock);

    if (status == PJ_SUCCESS) {
        aip->state = AI_STATE_CONNECTED;

        /* Let backend send session init */
        aip->backend->op->on_ws_connected(aip->backend, aip->ws);

        pj_grp_lock_release(aip->base.grp_lock);

        pj_bzero(&ev, sizeof(ev));
        ev.type = PJMEDIA_AI_EVENT_CONNECTED;
        ev.status = PJ_SUCCESS;
        if (aip->cb.on_event)
            aip->cb.on_event(aip, &ev);
    } else {
        aip->state = AI_STATE_IDLE;
        pj_grp_lock_release(aip->base.grp_lock);

        PJ_PERROR(3, (THIS_FILE, status, "AI WebSocket connect failed"));

        pj_bzero(&ev, sizeof(ev));
        ev.type = PJMEDIA_AI_EVENT_DISCONNECTED;
        ev.status = status;
        if (aip->cb.on_event)
            aip->cb.on_event(aip, &ev);
    }
}


/*
 * WebSocket on_rx_msg callback.
 */
static void on_ws_rx_msg(pj_websock *ws, pj_websock_opcode opcode,
                         const void *data, pj_size_t len)
{
    pjmedia_ai_port *aip;
    pjmedia_ai_event ev;
    unsigned sample_count;
    const char *reply = NULL;
    pj_size_t reply_len = 0;
    pj_status_t status;

    PJ_UNUSED_ARG(opcode);

    aip = (pjmedia_ai_port*)pj_websock_get_user_data(ws);
    PJ_ASSERT_ON_FAIL(aip != NULL, return);

#if AI_PORT_DUMP_RX_JSON
    /* Debug: dump raw JSON to log file */
    if (aip->log_file) {
        static const char sep[] = "\n---\n";
        pj_ssize_t ws_len = (pj_ssize_t)len;
        pj_ssize_t sep_len = sizeof(sep) - 1;
        pj_file_write(aip->log_file, data, &ws_len);
        pj_file_write(aip->log_file, sep, &sep_len);
        pj_file_flush(aip->log_file);
    }
#endif

    /* Reset tmp pool for parsing */
    pj_pool_reset(aip->tmp_pool);

    pj_bzero(&ev, sizeof(ev));
    ev.type = (pjmedia_ai_event_type)-1;
    sample_count = DECODE_BUF_SAMPLES;

    status = aip->backend->op->on_rx_msg(aip->backend,
                                         aip->tmp_pool,
                                         data, len,
                                         aip->decode_buf,
                                         &sample_count,
                                         &ev,
                                         &reply, &reply_len);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4, (THIS_FILE, status, "Backend on_rx_msg failed"));
        return;
    }

    /* Send reply if backend provided one */
    if (reply && reply_len > 0) {
        status = pj_websock_send(aip->ws, PJ_WEBSOCK_OP_TEXT,
                                 reply, reply_len);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4, (THIS_FILE, status, "Failed to send reply"));
        }
    }

    /* Write decoded audio to RX circular buffer */
    if (sample_count > 0) {
        pj_grp_lock_acquire(aip->base.grp_lock);

        /* Drop oldest if full */
        if (pjmedia_circ_buf_get_len(aip->rx_buf) +
            sample_count > aip->rx_buf->capacity)
        {
            unsigned to_discard =
                pjmedia_circ_buf_get_len(aip->rx_buf) +
                sample_count - aip->rx_buf->capacity;
            pjmedia_circ_buf_adv_read_ptr(aip->rx_buf, to_discard);
        }
        pjmedia_circ_buf_write(aip->rx_buf, aip->decode_buf,
                               sample_count);

        pj_grp_lock_release(aip->base.grp_lock);
    }

    /* Deliver event to application */
    if ((int)ev.type != -1 && aip->cb.on_event) {
        aip->cb.on_event(aip, &ev);
    }
}


/*
 * WebSocket on_close callback.
 */
static void on_ws_close(pj_websock *ws, pj_uint16_t status_code,
                        const pj_str_t *reason)
{
    pjmedia_ai_port *aip;
    pjmedia_ai_event ev;

    PJ_UNUSED_ARG(status_code);
    PJ_UNUSED_ARG(reason);

    aip = (pjmedia_ai_port*)pj_websock_get_user_data(ws);
    PJ_ASSERT_ON_FAIL(aip != NULL, return);

    pj_grp_lock_acquire(aip->base.grp_lock);
    aip->state = AI_STATE_IDLE;
    aip->send_pending = PJ_FALSE;
    pjmedia_circ_buf_reset(aip->tx_buf);
    pjmedia_circ_buf_reset(aip->rx_buf);
    pj_grp_lock_release(aip->base.grp_lock);

    pj_bzero(&ev, sizeof(ev));
    ev.type = PJMEDIA_AI_EVENT_DISCONNECTED;
    ev.status = PJ_SUCCESS;
    if (aip->cb.on_event)
        aip->cb.on_event(aip, &ev);
}


/*
 * Port destroy handler.
 */
static pj_status_t ai_port_on_destroy(pjmedia_port *this_port)
{
    pjmedia_ai_port *aip = (pjmedia_ai_port*)this_port;
    pj_pool_t *pool;

    PJ_LOG(4, (THIS_FILE, "AI port destroying"));

#if AI_PORT_DUMP_RX_JSON
    if (aip->log_file) {
        pj_file_close(aip->log_file);
        aip->log_file = NULL;
    }
#endif

    if (aip->ws) {
        pj_websock_destroy(aip->ws);
        aip->ws = NULL;
    }

    if (aip->backend && aip->backend->op->destroy) {
        aip->backend->op->destroy(aip->backend);
        aip->backend = NULL;
    }

    if (aip->tmp_pool) {
        pj_pool_release(aip->tmp_pool);
        aip->tmp_pool = NULL;
    }

    pool = aip->pool;
    aip->pool = NULL;
    if (pool)
        pj_pool_release(pool);

    return PJ_SUCCESS;
}


/*
 * Initialize parameters with defaults.
 */
PJ_DEF(void) pjmedia_ai_port_param_default(pjmedia_ai_port_param *param)
{
    PJ_ASSERT_ON_FAIL(param != NULL, return);
    pj_bzero(param, sizeof(*param));
    param->ptime_msec = 20;
    param->vad_enabled = PJ_FALSE;
}


/*
 * Create AI port.
 */
PJ_DEF(pj_status_t) pjmedia_ai_port_create(
                                    pj_pool_t *pool,
                                    const pjmedia_ai_port_param *param,
                                    pjmedia_ai_port **p_ai_port)
{
    pjmedia_ai_port *aip;
    pj_pool_t *own_pool;
    pj_str_t name;
    unsigned clock_rate;
    unsigned channel_count;
    unsigned bits_per_sample;
    unsigned ptime;
    unsigned port_spf;
    unsigned tx_buf_samples, rx_buf_samples;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && param && p_ai_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->ioqueue && param->timer_heap, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->backend, PJ_EINVAL);

    *p_ai_port = NULL;

    /* Derive port settings from backend */
    clock_rate = param->backend->native_clock_rate;
    channel_count = param->backend->native_channel_count;
    bits_per_sample = param->backend->native_bits_per_sample;
    ptime = param->ptime_msec ? param->ptime_msec : 20;
    port_spf = clock_rate * ptime / 1000;

    /* Create own pool */
    own_pool = pj_pool_create(pool->factory, "aiport%p", 4096, 4096, NULL);
    PJ_ASSERT_RETURN(own_pool != NULL, PJ_ENOMEM);

    aip = PJ_POOL_ZALLOC_T(own_pool, pjmedia_ai_port);
    aip->pool = own_pool;

    /* Store params */
    aip->cb = param->cb;
    aip->user_data = param->user_data;
    aip->backend = param->backend;
    aip->state = AI_STATE_IDLE;

    /* Initialize port info at backend's native rate */
    name = pj_str("ai-port");
    status = pjmedia_port_info_init(&aip->base.info, &name, SIGNATURE,
                                    clock_rate, channel_count,
                                    bits_per_sample, port_spf);
    if (status != PJ_SUCCESS) {
        pj_pool_release(own_pool);
        return status;
    }

    aip->base.info.dir = PJMEDIA_DIR_ENCODING_DECODING;
    aip->base.put_frame = &ai_port_put_frame;
    aip->base.get_frame = &ai_port_get_frame;
    aip->base.on_destroy = &ai_port_on_destroy;

    /* Initialize group lock */
    status = pjmedia_port_init_grp_lock(&aip->base, own_pool, NULL);
    if (status != PJ_SUCCESS) {
        pj_pool_release(own_pool);
        return status;
    }

    /* Create circular buffers */
    tx_buf_samples = clock_rate * TX_BUF_MSEC / 1000;
    rx_buf_samples = clock_rate * RX_BUF_MSEC / 1000;

    status = pjmedia_circ_buf_create(own_pool, tx_buf_samples,
                                     &aip->tx_buf);
    if (status != PJ_SUCCESS) {
        pjmedia_port_destroy(&aip->base);
        return status;
    }

    status = pjmedia_circ_buf_create(own_pool, rx_buf_samples,
                                     &aip->rx_buf);
    if (status != PJ_SUCCESS) {
        pjmedia_port_destroy(&aip->base);
        return status;
    }

    /* Pre-buffer threshold */
    aip->prebuf_samples = clock_rate * PREBUF_MSEC / 1000;

    /* TX flush threshold */
    aip->tx_flush_samples = clock_rate * TX_FLUSH_MSEC / 1000;

    /* Create TX VAD if enabled */
    if (param->vad_enabled) {
        status = pjmedia_silence_det_create(own_pool, clock_rate,
                                            port_spf, &aip->vad);
        if (status != PJ_SUCCESS) {
            pjmedia_port_destroy(&aip->base);
            return status;
        }
    }

    /* Allocate encode/decode buffers */
    aip->encode_buf = (char*)pj_pool_alloc(own_pool, ENCODE_BUF_SIZE);
    aip->decode_buf = (pj_int16_t*)
        pj_pool_alloc(own_pool, DECODE_BUF_SAMPLES * sizeof(pj_int16_t));
    aip->tx_read_buf = (pj_int16_t*)
        pj_pool_alloc(own_pool, DECODE_BUF_SAMPLES * sizeof(pj_int16_t));

    /* Create temporary pool for JSON parsing */
    aip->tmp_pool = pj_pool_create(pool->factory, "aiport_tmp%p",
                                   4096, 4096, NULL);

    /* Create WebSocket (share group lock) */
    {
        pj_websock_param ws_param;

        pj_websock_param_default(&ws_param);
        ws_param.ioqueue = param->ioqueue;
        ws_param.timer_heap = param->timer_heap;
        ws_param.cb.on_connect = &on_ws_connect;
        ws_param.cb.on_rx_msg = &on_ws_rx_msg;
        ws_param.cb.on_close = &on_ws_close;
        ws_param.user_data = aip;
        ws_param.ssl_param = param->ssl_param;

        status = pj_websock_create(own_pool, &ws_param, &aip->ws);
        if (status != PJ_SUCCESS) {
            pjmedia_port_destroy(&aip->base);
            return status;
        }
    }

#if AI_PORT_DUMP_RX_JSON
    /* Debug: open JSON log file */
    {
        pj_status_t fst;
        fst = pj_file_open(own_pool, "ai_rx_log.json",
                           PJ_O_WRONLY, &aip->log_file);
        if (fst != PJ_SUCCESS) {
            PJ_LOG(4, (THIS_FILE, "Could not open ai_rx_log.json "
                       "(non-fatal)"));
            aip->log_file = NULL;
        }
    }
#endif

    PJ_LOG(4, (THIS_FILE, "AI port created: rate=%u/%uch/%ubit, "
               "spf=%u (%ums), tx_buf=%u, rx_buf=%u, prebuf=%u, "
               "vad=%s",
               clock_rate, channel_count, bits_per_sample,
               port_spf, ptime,
               tx_buf_samples, rx_buf_samples,
               aip->prebuf_samples,
               aip->vad ? "yes" : "no"));

    *p_ai_port = aip;
    return PJ_SUCCESS;
}


PJ_DEF(pjmedia_port*) pjmedia_ai_port_get_port(pjmedia_ai_port *ai_port)
{
    PJ_ASSERT_RETURN(ai_port, NULL);
    return &ai_port->base;
}


PJ_DEF(pj_status_t) pjmedia_ai_port_connect(pjmedia_ai_port *ai_port,
                                             const pj_str_t *url,
                                             const pj_str_t *auth_token)
{
    pj_websock_connect_param cparam;
    pj_status_t status;

    PJ_ASSERT_RETURN(ai_port && url, PJ_EINVAL);

    pj_grp_lock_acquire(ai_port->base.grp_lock);

    if (ai_port->state != AI_STATE_IDLE) {
        pj_grp_lock_release(ai_port->base.grp_lock);
        return PJ_EBUSY;
    }

    ai_port->state = AI_STATE_CONNECTING;
    pj_grp_lock_release(ai_port->base.grp_lock);

    /* Let backend prepare connect params (extra headers, etc.) */
    pj_websock_connect_param_default(&cparam);
    if (ai_port->backend->op->prepare_connect) {
        status = ai_port->backend->op->prepare_connect(
                    ai_port->backend, ai_port->pool,
                    auth_token, &cparam);
        if (status != PJ_SUCCESS) {
            pj_grp_lock_acquire(ai_port->base.grp_lock);
            ai_port->state = AI_STATE_IDLE;
            pj_grp_lock_release(ai_port->base.grp_lock);
            return status;
        }
    }

    status = pj_websock_connect(ai_port->ws, url, &cparam);
    if (status != PJ_SUCCESS) {
        pj_grp_lock_acquire(ai_port->base.grp_lock);
        ai_port->state = AI_STATE_IDLE;
        pj_grp_lock_release(ai_port->base.grp_lock);
        PJ_PERROR(3, (THIS_FILE, status, "WebSocket connect failed"));
    }

    return status;
}


PJ_DEF(pj_status_t) pjmedia_ai_port_disconnect(pjmedia_ai_port *ai_port)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(ai_port, PJ_EINVAL);

    pj_grp_lock_acquire(ai_port->base.grp_lock);

    if (ai_port->state != AI_STATE_CONNECTED) {
        pj_grp_lock_release(ai_port->base.grp_lock);
        return PJ_EINVALIDOP;
    }

    ai_port->state = AI_STATE_DISCONNECTING;
    pj_grp_lock_release(ai_port->base.grp_lock);

    status = pj_websock_close(ai_port->ws, 1000, NULL);
    return status;
}


PJ_DEF(void*) pjmedia_ai_port_get_user_data(pjmedia_ai_port *ai_port)
{
    PJ_ASSERT_RETURN(ai_port, NULL);
    return ai_port->user_data;
}

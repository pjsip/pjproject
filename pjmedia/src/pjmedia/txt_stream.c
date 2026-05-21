/*
 * Copyright (C) 2024-2025 Teluu Inc. (http://www.teluu.com)
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

#include <pjmedia/txt_stream.h>
#include <pjmedia/clock.h>
#include <pjmedia/errno.h>
#include <pjmedia/jbuf.h>
#include <pjmedia/port.h>
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/sdp_neg.h>
#include <pjmedia/transport.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>

#define THIS_FILE                       "txt_stream.c"
#define LOGERR_(expr)                   PJ_PERROR(4,expr)
#define TRC_(expr)                      PJ_LOG(5,expr)

#if 0
#   define TRACE_(log)  PJ_LOG(3, log)
#else
#   define TRACE_(log)
#endif

/* Default and max buffering time if not specified.
 * The RFC recommends a buffering time of 300 ms. Note that the maximum
 * according to the RFC is 500 ms.
 */
#define BUFFERING_TIME          300
#define MAX_BUFFERING_TIME      500

/* Buffer size to store the characters buffered during BUFFERING_TIME.
 * The default cps (character per second) is 30, but it's over a 10-second
 * interval, so we need a larger buffer to handle the burst.
 */
#define BUFFER_SIZE             512

/* The number of buffers must be the max redundancy we want to support +
 * plus one more to store the primary data.
 */
#define NUM_BUFFERS             PJMEDIA_TXT_STREAM_MAX_RED_LEVELS + 1

/* 5.4. Compensation for Packets Out of Order:
 * If analysis of a received packet reveals a gap in the sequence,
 * we need to wait for the missing packet(s) to arrive.  It is
 * RECOMMENDED the waiting time be limited to 1 second (1000 ms).
 */
#define MAX_RX_WAITING_TIME     1000

/* Maximum number of text packets that can be kept in the jitter buffer.
 * The value should be roughly our MAX_RX_WAITING_TIME over remote's
 * buffering time (which unfortunately we don't know).
 */
#define JBUF_MAX_COUNT          8


/* Buffer to store redundancy and primary data. */
typedef struct red_buf
{
    unsigned timestamp;
    unsigned length;
    char     buf[BUFFER_SIZE];
} red_buf;

typedef struct pjmedia_txt_stream
{
    pjmedia_stream_common       base;
    pjmedia_txt_stream_info     si;             /**< Creation parameter.    */

    pjmedia_clock              *clock;          /**< Clock.                 */
    unsigned                    buf_time;       /**< Buffering time.        */
    pj_bool_t                   is_idle;        /**< Is idle?               */

    pj_timestamp                rtcp_last_tx;   /**< Last RTCP tx time.     */
    pj_timestamp                tx_last_ts;     /**< Timestamp of last tx.  */
    red_buf                     tx_buf[NUM_BUFFERS];/**< Tx buffer.         */
    int                         tx_buf_idx;     /**< Index to current buffer*/
    int                         tx_nred;        /**< Num of redundant data. */

    int                         rx_last_seq;    /**< Sequence of last rx.   */
    pj_bool_t                   is_waiting;     /**< Is waiting?            */
    pj_timestamp                rx_wait_ts;     /**< Ts of waiting start.   */

    /* Incoming text callback. */
    void                      (*cb)(pjmedia_txt_stream *, void *,
                                    const pjmedia_txt_stream_data *);
    void                       *cb_user_data;

} pjmedia_txt_stream;


static void clock_cb(const pj_timestamp *ts, void *user_data);


#include "stream_imp_common.c"

static void on_stream_destroy(void *arg)
{
    pjmedia_txt_stream *stream = (pjmedia_txt_stream *)arg;

    if (stream->clock) {
        pjmedia_clock_destroy(stream->clock);
        stream->clock = NULL;
    }
}

/*
 * Destroy stream.
 */
PJ_DEF(pj_status_t) pjmedia_txt_stream_destroy( pjmedia_txt_stream *stream )
{
    pjmedia_stream_common *c_strm = (pjmedia_stream_common *)stream;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    PJ_LOG(4,(c_strm->port.info.name.ptr, "Stream destroying"));

    stream->cb = NULL;

    if (stream->clock)
        pjmedia_clock_stop(stream->clock);

    /* Send RTCP BYE (also SDES & XR) */
    if (c_strm->transport && !c_strm->rtcp_sdes_bye_disabled) {
#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
        send_rtcp(c_strm, PJ_TRUE, PJ_TRUE, c_strm->rtcp.xr_enabled,
                  PJ_FALSE, PJ_FALSE, PJ_FALSE);
#else
        send_rtcp(c_strm, PJ_TRUE, PJ_TRUE, PJ_FALSE, PJ_FALSE,
                  PJ_FALSE, PJ_FALSE);
#endif
    }

    /* Unsubscribe from RTCP session events
     * Currently unused
     */
    //pjmedia_event_unsubscribe(NULL, &stream_event_cb, stream, &c_strm->rtcp);

    /* Detach from transport
     * MUST NOT hold stream mutex while detaching from transport, as
     * it may cause deadlock. See ticket #460 for the details.
     */
    if (c_strm->transport) {
        pjmedia_transport_detach(c_strm->transport, c_strm);
        //c_strm->transport = NULL;
    }

    if (c_strm->grp_lock) {
        pj_grp_lock_dec_ref(c_strm->grp_lock);
    } else {
        on_destroy(c_strm);
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t)
pjmedia_txt_stream_create( pjmedia_endpt *endpt,
                           pj_pool_t *pool,
                           const pjmedia_txt_stream_info *info,
                           pjmedia_transport *tp,
                           void *user_data,
                           pjmedia_txt_stream **p_stream)

{
    enum { M = 32 };
    pjmedia_txt_stream *stream;
    pjmedia_stream_common *c_strm;
    pj_str_t name;
    pj_pool_t *own_pool = NULL;
    char *p;
    unsigned buf_size;
    pj_status_t status;
    pjmedia_transport_attach_param att_param;
    pjmedia_clock_param cparam;

    PJ_ASSERT_RETURN(endpt && info && tp && p_stream, PJ_EINVAL);

    if (pool == NULL) {
        own_pool = pjmedia_endpt_create_pool( endpt, "tstrm%p",
                                              2000, 2000);
        PJ_ASSERT_RETURN(own_pool != NULL, PJ_ENOMEM);
        pool = own_pool;
    }

    /* Allocate the text stream: */
    stream = PJ_POOL_ZALLOC_T(pool, pjmedia_txt_stream);
    PJ_ASSERT_RETURN(stream != NULL, PJ_ENOMEM);
    c_strm = &stream->base;
    c_strm->own_pool = own_pool;

    /* Duplicate stream info */
    pj_memcpy(&stream->si, info, sizeof(*info));
    c_strm->si = (pjmedia_stream_info_common *)&stream->si;
    pj_strdup(pool, &stream->si.fmt.encoding_name, &info->fmt.encoding_name);
    pjmedia_rtcp_fb_info_dup(pool, &c_strm->si->loc_rtcp_fb,
                             &info->loc_rtcp_fb);
    pjmedia_rtcp_fb_info_dup(pool, &c_strm->si->rem_rtcp_fb,
                             &info->rem_rtcp_fb);

    /* Init stream/port name */
    name.ptr = (char*) pj_pool_alloc(pool, M);
    name.slen = pj_ansi_snprintf(name.ptr, M, "tstrm%p", stream);
    c_strm->port.info.name = name;

    /* Init stream: */
    c_strm->endpt = endpt;
    c_strm->dir = info->dir;
    c_strm->user_data = user_data;
    c_strm->rtcp_interval = (PJMEDIA_RTCP_INTERVAL-500 + (pj_rand()%1000)) *
                            info->fmt.clock_rate / 1000;
    c_strm->rtcp_sdes_bye_disabled = info->rtcp_sdes_bye_disabled;

    c_strm->jb_last_frm = PJMEDIA_JB_NORMAL_FRAME;
    c_strm->rtcp_fb_nack.pid = -1;
    stream->rx_last_seq = -1;
    stream->is_idle = PJ_TRUE;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    c_strm->use_ka = info->use_ka;
    c_strm->ka_interval = info->ka_cfg.ka_interval;
    c_strm->start_ka_count = info->ka_cfg.start_count;
    c_strm->start_ka_interval = info->ka_cfg.start_interval;
#endif

    c_strm->cname = info->cname;
    if (c_strm->cname.slen == 0) {
        /* Build random RTCP CNAME. CNAME has user@host format */
        c_strm->cname.ptr = p = (char*) pj_pool_alloc(pool, 20);
        pj_create_random_string(p, 5);
        p += 5;
        *p++ = '@'; *p++ = 'p'; *p++ = 'j';
        pj_create_random_string(p, 6);
        p += 6;
        *p++ = '.'; *p++ = 'o'; *p++ = 'r'; *p++ = 'g';
        c_strm->cname.slen = p - c_strm->cname.ptr;
    }

    /* Init buffering time and create clock. */
    stream->buf_time = info->buffer_time;
    if (stream->buf_time == 0)
        stream->buf_time = BUFFERING_TIME;
    cparam.usec_interval = stream->buf_time * 1000;
    cparam.clock_rate = 1000;
    status = pjmedia_clock_create2(pool, &cparam,
                                   PJMEDIA_CLOCK_NO_HIGHEST_PRIO,
                                   clock_cb, stream, &stream->clock);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Create mutex to protect jitter buffer: */
    status = pj_mutex_create_recursive(pool, NULL, &c_strm->jb_mutex);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Create jitter buffer */
    status = pjmedia_jbuf_create(pool, &c_strm->port.info.name,
                                 BUFFER_SIZE,
                                 stream->buf_time,
                                 JBUF_MAX_COUNT, &c_strm->jb);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Set up jitter buffer */
    pjmedia_jbuf_set_fixed(c_strm->jb, 0);
    pjmedia_jbuf_set_discard(c_strm->jb, PJMEDIA_JB_DISCARD_NONE);

    /* Create decoder channel: */
    status = create_channel( pool, c_strm, PJMEDIA_DIR_DECODING,
                             info->rx_pt, 0, c_strm->si, &c_strm->dec);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Create encoder channel: */
    buf_size = BUFFER_SIZE;
    status = create_channel( pool, c_strm, PJMEDIA_DIR_ENCODING,
                             info->tx_pt, buf_size, c_strm->si, &c_strm->enc);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Init RTCP session: */
    {
        pjmedia_rtcp_session_setting rtcp_setting;

        pjmedia_rtcp_session_setting_default(&rtcp_setting);
        rtcp_setting.name = c_strm->port.info.name.ptr;
        rtcp_setting.ssrc = info->ssrc;
        rtcp_setting.rtp_ts_base = pj_ntohl(c_strm->enc->rtp.out_hdr.ts);
        /* Fallback to T.140 standard */
        rtcp_setting.clock_rate =
            info->fmt.clock_rate > 0 ? info->fmt.clock_rate : 1000;
        rtcp_setting.samples_per_frame = 1;

        pjmedia_rtcp_init2(&c_strm->rtcp, &rtcp_setting);

        if (info->rtp_seq_ts_set) {
            c_strm->rtcp.stat.rtp_tx_last_seq = info->rtp_seq;
            c_strm->rtcp.stat.rtp_tx_last_ts = info->rtp_ts;
        }

        /* Subscribe to RTCP events */
        // Currently not needed, perhaps for future use.
        //pjmedia_event_subscribe(NULL, &stream_event_cb, stream,
        //                        &c_strm->rtcp);
    }

    /* Allocate outgoing RTCP buffer, should be enough to hold SR/RR, SDES,
     * BYE, and XR.
     */
    c_strm->out_rtcp_pkt_size =  sizeof(pjmedia_rtcp_sr_pkt) +
                                 sizeof(pjmedia_rtcp_common) +
                                 (4 + (unsigned)c_strm->cname.slen) +
                                 32;

    if (c_strm->out_rtcp_pkt_size > PJMEDIA_MAX_MTU)
        c_strm->out_rtcp_pkt_size = PJMEDIA_MAX_MTU;

    c_strm->out_rtcp_pkt = pj_pool_alloc(pool, c_strm->out_rtcp_pkt_size);
    pj_bzero(&att_param, sizeof(att_param));
    att_param.stream = stream;
    att_param.media_type = PJMEDIA_TYPE_TEXT;
    att_param.user_data = stream;

    pj_sockaddr_cp(&att_param.rem_addr, &info->rem_addr);
    pj_sockaddr_cp(&c_strm->rem_rtp_addr, &info->rem_addr);
    if (stream->si.rtcp_mux) {
        pj_sockaddr_cp(&att_param.rem_rtcp, &info->rem_addr);
    } else if (pj_sockaddr_has_addr(&info->rem_rtcp)) {
        pj_sockaddr_cp(&att_param.rem_rtcp, &info->rem_rtcp);
    }
    att_param.addr_len = pj_sockaddr_get_len(&info->rem_addr);
    att_param.rtp_cb2 = &on_rx_rtp;
    att_param.rtcp_cb = &on_rx_rtcp;

    /* Create group lock & attach handler */
    status = pj_grp_lock_create_w_handler(pool, NULL, stream,
                                          &on_destroy,
                                          &c_strm->grp_lock);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Add ref */
    pj_grp_lock_add_ref(c_strm->grp_lock);
    c_strm->port.grp_lock = c_strm->grp_lock;

    /* Only attach transport when stream is ready. */
    status = pjmedia_transport_attach2(tp, &att_param);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Also add ref the transport group lock */
    c_strm->transport = tp;
    if (c_strm->transport->grp_lock)
        pj_grp_lock_add_ref(c_strm->transport->grp_lock);

    /* Send RTCP SDES */
    if (!c_strm->rtcp_sdes_bye_disabled) {
       pjmedia_stream_common_send_rtcp_sdes(c_strm);
    }

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    /* NAT hole punching by sending KA packet via RTP transport. */
    if (c_strm->use_ka)
        send_keep_alive_packet(c_strm);
#endif

    /* Success! */
    *p_stream = stream;

    PJ_LOG(3, (THIS_FILE, "Text stream %s created", c_strm->port.info.name.ptr));

    return PJ_SUCCESS;

err_cleanup:
    pjmedia_txt_stream_destroy(stream);
    return status;
}

PJ_DEF(pj_status_t) pjmedia_txt_stream_start(pjmedia_txt_stream *stream)
{
    pjmedia_stream_common *c_strm = (pjmedia_stream_common *)stream;
    pj_status_t status;

    pjmedia_stream_common_start(c_strm);
    if (!c_strm->enc->paused || !c_strm->dec->paused) {
        status = pjmedia_clock_start(stream->clock);
    } else {
        status = pjmedia_clock_stop(stream->clock);
    }

    return status;
}

PJ_DEF(pj_status_t)
pjmedia_txt_stream_get_info(const pjmedia_txt_stream *stream,
                            pjmedia_txt_stream_info *info)
{
    PJ_ASSERT_RETURN(stream && info, PJ_EINVAL);

    pj_memcpy(info, &stream->si, sizeof(pjmedia_txt_stream_info));
    return PJ_SUCCESS;
}

static void call_cb(pjmedia_txt_stream *stream, pj_bool_t now)
{
    pjmedia_stream_common *c_strm = &stream->base;

    char frm_type;
    int next_seq;
    char frm_buf[BUFFER_SIZE];
    pj_size_t frm_size = sizeof(frm_buf);
    pj_uint32_t ts;
    int popped_seq;
    char *text_ptr = NULL;

    pj_mutex_lock(c_strm->jb_mutex);

    stream->is_waiting = PJ_FALSE;
    do {
        /* Check if we have a packet with the next sequence number. */
        pjmedia_jbuf_peek_frame(c_strm->jb, 0, NULL, NULL, &frm_type, NULL,
                                NULL, &next_seq);

        /* PJSIP empty frame validation */
        if (frm_type == PJMEDIA_JB_ZERO_EMPTY_FRAME ||
            frm_type == PJMEDIA_JB_ZERO_PREFETCH_FRAME) {
            break;
        }

        /* Pop the frame from the jitter buffer. */
        pjmedia_jbuf_get_frame3(c_strm->jb, frm_buf, &frm_size, &frm_type, NULL,
                                &ts, &popped_seq);

        /* Handle missing frames */
        if (frm_type != PJMEDIA_JB_NORMAL_FRAME) {
            if (frm_type == PJMEDIA_JB_MISSING_FRAME && stream->cb) {
                /* Inject U+FFFD (replacement char) for lost packets */
                pjmedia_txt_stream_data data;
                data.seq = popped_seq;
                data.ts = ts;
                data.text.ptr = "\xEF\xBF\xBD";
                data.text.slen = 3;

                pj_mutex_unlock(c_strm->jb_mutex);
                (*stream->cb)(stream, stream->cb_user_data, &data);
                pj_mutex_lock(c_strm->jb_mutex);
            }
            continue;
        }

        /* Strip the UTF-8 BOM if it exists at the start of the frame */
        text_ptr = frm_buf;
        if (frm_size >= 3 && (unsigned char) text_ptr[0] == 0xEF &&
            (unsigned char) text_ptr[1] == 0xBB &&
            (unsigned char) text_ptr[2] == 0xBF) {
            text_ptr += 3;
            frm_size -= 3;
        }

        /* Prevent zero-length frames from waking up the UI */
        pj_mutex_unlock(c_strm->jb_mutex);
        if (stream->cb && frm_size > 0) {
            pjmedia_txt_stream_data data;

            data.seq = popped_seq;
            data.ts = ts;
            data.text.ptr = text_ptr;
            data.text.slen = frm_size;
            (*stream->cb)(stream, stream->cb_user_data, &data);

        } else if (!stream->cb && frm_size > 0) {
            /* Debug trace if callback isn't set */
            TRACE_((THIS_FILE, "Text frame popped: %.*s", (int) frm_size,
                    text_ptr));
        }
        pj_mutex_lock(c_strm->jb_mutex);

    } while (1);

    pj_mutex_unlock(c_strm->jb_mutex);
}

static pj_status_t decode_red(pjmedia_txt_stream *stream, unsigned pt, int seq,
                              const char *buf, unsigned buflen,
                              unsigned *red_len)
{
    pj_uint32_t hdr[NUM_BUFFERS];
    const char *payload_ptr = NULL;
    const char *data_ptr = NULL;
    int i, level = 0;
    unsigned consumed = 0;
    unsigned length = 0;
    unsigned data_len = 0;
    unsigned primary_len = 0;
    pj_uint16_t block_seq = 0;
    pj_int16_t diff = 0;
    pj_uint8_t b = 0;

    /* Parse headers */
    while (1) {
        if (consumed + 1 > buflen)
            return PJ_ETOOBIG;
        b = (pj_uint8_t) buf[consumed];
        if ((b & 0x7F) != (pj_uint8_t) pt)
            return PJMEDIA_EINVALIDPT;
        if ((b & 0x80) == 0) {
            consumed += 1;
            break;
        }
        if (consumed + 4 > buflen || level >= NUM_BUFFERS)
            return PJ_ETOOBIG;
        pj_memcpy(&hdr[level], &buf[consumed], 4);
        hdr[level] = pj_ntohl(hdr[level]);
        consumed += 4;
        level++;
    }

    /* Parse redundant history blocks */
    payload_ptr = buf + consumed;
    for (i = 0; i < level; i++) {
        length = (hdr[i] & 0x3FF);
        block_seq = (pj_uint16_t) (seq - (level - i));

        data_ptr = payload_ptr;
        data_len = length;

        if (length > 0) {
            if (consumed + length > buflen)
                return PJ_ETOOBIG;

            /* Strip native BOM if it got trapped in history */
            if (data_len >= 3 && (pj_uint8_t) data_ptr[0] == 0xEF &&
                (pj_uint8_t) data_ptr[1] == 0xBB &&
                (pj_uint8_t) data_ptr[2] == 0xBF) {
                data_ptr += 3;
                data_len -= 3;
            }
        }

        /* Inject everything we missed, even length=0 frames! */
        diff = (pj_int16_t) (block_seq - stream->rx_last_seq);
        if (stream->rx_last_seq == -1 || diff > 0) {
            pjmedia_jbuf_put_frame(stream->base.jb, data_ptr, data_len,
                                   block_seq);
            stream->rx_last_seq = block_seq; /* Update tracker */
        }

        if (length > 0) {
            payload_ptr += length;
            consumed += length;
        }
    }

    /* Parse primary block */
    primary_len = buflen - consumed;
    data_ptr = payload_ptr;
    data_len = primary_len;

    if (primary_len > 0) {
        /* Strip native BOM if present */
        if (data_len >= 3 && (pj_uint8_t) data_ptr[0] == 0xEF &&
            (pj_uint8_t) data_ptr[1] == 0xBB &&
            (pj_uint8_t) data_ptr[2] == 0xBF) {
            data_ptr += 3;
            data_len -= 3;
        }
    }

    /* Detect gaps for primary */
    diff = (pj_int16_t) (seq - stream->rx_last_seq);
    if (stream->rx_last_seq == -1 || diff > 0) {
        pjmedia_jbuf_put_frame(stream->base.jb, data_ptr, data_len,
                               (pj_uint16_t) seq);
        stream->rx_last_seq = (pj_uint16_t) seq;
    }

    consumed += primary_len;
    *red_len = consumed;
    return PJ_SUCCESS;
}

static pj_status_t on_stream_rx_rtp(pjmedia_stream_common *c_strm,
                                    const pjmedia_rtp_hdr *hdr,
                                    const void *payload, unsigned payloadlen,
                                    pjmedia_rtp_status seq_st,
                                    pj_bool_t *pkt_discarded)
{
    pjmedia_txt_stream *stream = (pjmedia_txt_stream *) c_strm;
    unsigned consumed = 0;
    pj_uint16_t seq = pj_ntohs(hdr->seq);
    pj_int16_t diff = 0;
    pj_status_t status = PJ_SUCCESS;

    pj_mutex_lock(c_strm->jb_mutex);
    if (seq_st.status.flag.restart) {
        stream->rx_last_seq = -1;
        pjmedia_jbuf_reset(c_strm->jb);
    }

    if (hdr->pt == c_strm->si->rx_red_pt) {
        status = decode_red(stream, c_strm->si->rx_pt, seq,
                            (const char *) payload, payloadlen, &consumed);

        if (status != PJ_SUCCESS) {
            *pkt_discarded = PJ_TRUE;
            pj_mutex_unlock(c_strm->jb_mutex);
            return status;
        }

    } else if (hdr->pt == c_strm->si->rx_pt) {
        /* Fallback: Route T.140 directly to Jitter Buffer */
        diff = (pj_int16_t) (seq - stream->rx_last_seq);
        if (stream->rx_last_seq == -1 || diff > 0) {
            pjmedia_jbuf_put_frame(c_strm->jb, payload, payloadlen, seq);
            stream->rx_last_seq = seq;
        } else {
            *pkt_discarded = PJ_TRUE;
        }
    } else {
        /* Unknown Payload Type */
        status = PJMEDIA_EINVALIDPT;
        *pkt_discarded = PJ_TRUE;
    }

    pj_mutex_unlock(c_strm->jb_mutex);

    /* Trigger the UI callback if we got a packet */
    if (!(*pkt_discarded))
        call_cb(stream, PJ_FALSE);

    return status;
}

static pj_status_t encode_red(pjmedia_txt_stream *stream, unsigned pt,
                              char *buf, int *size,
                              pj_bool_t force_empty_history)
{
    int i;
    unsigned len = 0;
    unsigned hist_len = 0;
    pj_uint8_t *p_hdr = (pj_uint8_t *) buf;
    pj_uint8_t *p_data = NULL;
    pj_uint32_t offset = 0;
    pj_uint32_t off = 0;

    unsigned level = stream->si.tx_red_level;
    if (level >= NUM_BUFFERS)
        level = NUM_BUFFERS - 1;

    /* Write redundant headers */
    for (i = level; i > 0; i--) {
        offset = 0;
        hist_len = 0;

        /* Enforce empty history if requested */
        if (!force_empty_history) {
            offset = stream->tx_buf[0].timestamp - stream->tx_buf[i].timestamp;
            if (offset > 16383)
                continue; /* Skip uninitialized */
            hist_len = stream->tx_buf[i].length;
        }

        if (len + 4 > (unsigned) *size)
            return PJ_ETOOBIG;

        *p_hdr++ = (pj_uint8_t) (0x80 | (pt & 0x7F));
        *p_hdr++ = (pj_uint8_t) ((offset >> 6) & 0xFF);
        *p_hdr++ =
            (pj_uint8_t) (((offset << 2) & 0xFC) | ((hist_len >> 8) & 0x03));
        *p_hdr++ = (pj_uint8_t) (hist_len & 0xFF);
        len += 4;
    }

    /* Primary header */
    if (len + 1 > (unsigned) *size)
        return PJ_ETOOBIG;
    *p_hdr++ = (pj_uint8_t) (pt & 0x7F);
    len += 1;

    /* Payloads */
    p_data = p_hdr;
    for (i = level; i >= 0; i--) {
        unsigned payload_len =
            (force_empty_history && i > 0) ? 0 : stream->tx_buf[i].length;

        if (payload_len == 0)
            continue;

        if (!force_empty_history && i > 0) {
            off = stream->tx_buf[0].timestamp - stream->tx_buf[i].timestamp;
            if (off > 16383)
                continue;
        }

        if (len + payload_len > (unsigned) *size)
            return PJ_ETOOBIG;

        pj_memcpy(p_data, stream->tx_buf[i].buf, payload_len);
        p_data += payload_len;
        len += payload_len;
    }

    *size = len;
    return PJ_SUCCESS;
}

static pj_status_t send_text(pjmedia_txt_stream *stream, unsigned rtp_ts_len)
{
    pjmedia_stream_common *c_strm = &stream->base;
    pjmedia_channel *channel = c_strm->enc;
    pjmedia_rtp_hdr *sent_hdr;
    unsigned pt_to_use = 0;
    void *rtphdr;
    int size, i;
    pj_status_t status = PJ_SUCCESS;
    pj_bool_t is_keepalive = PJ_FALSE;
    pj_bool_t is_first_packet = PJ_FALSE;
    pj_timestamp now;
    void *bom_rtphdr;
    int bom_size = 0;
    pj_uint32_t bom_ts = 0;
    char temp_buf[256];
    unsigned temp_len = 0;

    /* Lock Jitter Buffer mutex to protect stream state during transmission */
    pj_mutex_lock(c_strm->jb_mutex);
    pj_get_timestamp(&now);

    is_first_packet =
        (stream->tx_last_ts.u32.hi == 0 && stream->tx_last_ts.u32.lo == 0);

    /* Calculate RTP timestamp increment based on actual elapsed time */
    if (rtp_ts_len == 0) {
        if (is_first_packet) {
            rtp_ts_len = 20;
        } else {
            rtp_ts_len = pj_elapsed_msec(&stream->tx_last_ts, &now);
        }
        if (rtp_ts_len == 0)
            rtp_ts_len = 1;
    }

    /* Check for Keep-Alive condition (RFC 4103 recommends 10s of idle time) */
    if (stream->is_idle && stream->tx_buf[0].length == 0) {
        if (!is_first_packet &&
            pj_elapsed_msec(&stream->tx_last_ts, &now) >= 10000) {
            is_keepalive = PJ_TRUE;
        } else {
            status = PJ_SUCCESS;
            goto on_return;
        }
    }

    pt_to_use = (stream->si.tx_red_pt) ? stream->si.tx_red_pt : channel->pt;

    /* Protect against application-layer BOMs leaking into the buffer */
    if (stream->tx_buf[0].length >= 3 &&
        (pj_uint8_t) stream->tx_buf[0].buf[0] == 0xEF &&
        (pj_uint8_t) stream->tx_buf[0].buf[1] == 0xBB &&
        (pj_uint8_t) stream->tx_buf[0].buf[2] == 0xBF) {
        stream->tx_buf[0].length -= 3;
        pj_memmove(stream->tx_buf[0].buf, stream->tx_buf[0].buf + 3,
                   stream->tx_buf[0].length);
    }

    /* BOM sent once per session to establish UTF-8 capability */
    if (is_first_packet && stream->tx_buf[0].length > 0) {
        /* M=1 (Marker bit) for the first packet of a talkspurt */
        status = pjmedia_rtp_encode_rtp(&channel->rtp, pt_to_use, 1,
                                        (int) channel->buf_size, rtp_ts_len,
                                        (const void **) &bom_rtphdr, &bom_size);
        if (status == PJ_SUCCESS) {
            pj_memcpy(channel->buf, bom_rtphdr, sizeof(pjmedia_rtp_hdr));
            bom_ts = pj_ntohl(((pjmedia_rtp_hdr *) bom_rtphdr)->ts);

            /* Back up the actual character the user typed */
            temp_len = stream->tx_buf[0].length;
            pj_memcpy(temp_buf, stream->tx_buf[0].buf, temp_len);

            /* Load the BOM into the active slot */
            stream->tx_buf[0].buf[0] = 0xEF;
            stream->tx_buf[0].buf[1] = 0xBB;
            stream->tx_buf[0].buf[2] = 0xBF;
            stream->tx_buf[0].length = 3;
            stream->tx_buf[0].timestamp = bom_ts;

            bom_size = (int) channel->buf_size - sizeof(pjmedia_rtp_hdr);

            /* Encode the BOM */
            if (stream->si.tx_red_pt > 0) {
                /* RED Negotiated: Prime history slots and use RED encoder */
                for (i = 1; i < NUM_BUFFERS; i++) {
                    stream->tx_buf[i].length = 0;
                    stream->tx_buf[i].timestamp = bom_ts;
                }
                status = encode_red(
                    stream, (unsigned) stream->si.tx_pt,
                    ((char *) channel->buf) + sizeof(pjmedia_rtp_hdr),
                    &bom_size, PJ_TRUE);
            } else {
                /* T.140 Negotiated: Send raw UTF-8 BOM bytes */
                if (bom_size < 3) {
                    status = PJ_ETOOBIG;
                } else {
                    pj_memcpy(((char *) channel->buf) + sizeof(pjmedia_rtp_hdr),
                              stream->tx_buf[0].buf, 3);
                    bom_size = 3;
                    status = PJ_SUCCESS;
                }
            }

            if (status == PJ_SUCCESS) {
                pj_mutex_unlock(c_strm->jb_mutex);
                status = pjmedia_transport_send_rtp(
                    c_strm->transport, channel->buf,
                    bom_size + sizeof(pjmedia_rtp_hdr));
                pj_mutex_lock(c_strm->jb_mutex);
                if (status == PJ_SUCCESS) {
                    sent_hdr = (pjmedia_rtp_hdr *) channel->buf;
                    pjmedia_rtcp_tx_rtp(&c_strm->rtcp, bom_size);
                    c_strm->rtcp.stat.rtp_tx_last_ts = pj_ntohl(sent_hdr->ts);
                    c_strm->rtcp.stat.rtp_tx_last_seq = pj_ntohs(sent_hdr->seq);
#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA != 0
                    c_strm->last_frm_ts_sent = c_strm->rtcp.stat.rtp_tx_last_ts;
#endif
                }
            }

            /* Shift the register: BOM becomes history for the next packet */
            for (i = NUM_BUFFERS - 1; i > 0; i--) {
                stream->tx_buf[i] = stream->tx_buf[i - 1];
            }

            /* Restore the user's typed character */
            stream->tx_buf[0].length = temp_len;
            pj_memcpy(stream->tx_buf[0].buf, temp_buf, temp_len);

            is_first_packet = PJ_FALSE;
            stream->is_idle = PJ_FALSE;
            rtp_ts_len = 10; /* TS advance for the actual character packet */
        }
    }

    /* If there's no data, no pending RED, and not a keepalive, we are done */
    if (stream->tx_buf[0].length == 0 && !is_keepalive &&
        stream->tx_nred == 0) {
        status = PJ_SUCCESS;
        goto on_return;
    }

    /* Standard character transmission */
    status = pjmedia_rtp_encode_rtp(&channel->rtp, pt_to_use, 0,
                                    (int) channel->buf_size, rtp_ts_len,
                                    (const void **) &rtphdr, &size);
    if (status != PJ_SUCCESS)
        goto on_return;

    pj_memcpy(channel->buf, rtphdr, sizeof(pjmedia_rtp_hdr));
    stream->tx_buf[0].timestamp = pj_ntohl(((pjmedia_rtp_hdr *) rtphdr)->ts);

    /* Mid-session Talkspurt initialization (e.g., typing after a long pause) */
    if (stream->is_idle && !is_keepalive) {
        stream->is_idle = PJ_FALSE;
        ((pjmedia_rtp_hdr *) channel->buf)->m = 1;
    }

    /* Encode character packet */
    size = (int) channel->buf_size - sizeof(pjmedia_rtp_hdr);
    if (stream->si.tx_red_pt > 0) {
        /* RED Negotiated: Use RFC 2198 wrapper */
        status = encode_red(stream, (unsigned) stream->si.tx_pt,
                            ((char *) channel->buf) + sizeof(pjmedia_rtp_hdr),
                            &size, PJ_FALSE);
        if (status != PJ_SUCCESS)
            goto on_return;
    } else {
        /* T.140 Negotiated: Send raw UTF-8 per RFC 4103 */
        if (stream->tx_buf[0].length > size) {
            status = PJ_ETOOBIG;
            goto on_return;
        }
        pj_memcpy(((char *) channel->buf) + sizeof(pjmedia_rtp_hdr),
                  stream->tx_buf[0].buf, stream->tx_buf[0].length);
        size = stream->tx_buf[0].length;
    }

    /* Linear shift register management */
    if (stream->tx_buf[0].length > 0) {
        /* We just sent a new character, reset the trailing packet counter */
        stream->tx_nred = stream->si.tx_red_level;
        for (i = NUM_BUFFERS - 1; i > 0; i--) {
            stream->tx_buf[i] = stream->tx_buf[i - 1];
        }
        stream->tx_buf[0].length = 0;

    } else if (stream->tx_nred > 0) {
        /* We are sending empty packets to fulfill redundancy requirements */
        stream->tx_nred--;
        if (stream->tx_nred == 0)
            stream->is_idle = PJ_TRUE;
        for (i = NUM_BUFFERS - 1; i > 0; i--) {
            stream->tx_buf[i] = stream->tx_buf[i - 1];
        }
        stream->tx_buf[0].length = 0;

    } else if (is_keepalive) {
        /* Keepalives do not advance the shift register */
        stream->is_idle = PJ_TRUE;
    } else {
        goto on_return;
    }

    /* network transmission */
    pj_mutex_unlock(c_strm->jb_mutex);
    status = pjmedia_transport_send_rtp(c_strm->transport, channel->buf,
                                        size + sizeof(pjmedia_rtp_hdr));
    pj_mutex_lock(c_strm->jb_mutex);

    if (status == PJ_SUCCESS) {
        pj_get_timestamp(&stream->tx_last_ts);

        pjmedia_rtp_hdr *sent_hdr = (pjmedia_rtp_hdr *) channel->buf;
        pjmedia_rtcp_tx_rtp(&c_strm->rtcp, size);
        c_strm->rtcp.stat.rtp_tx_last_ts = pj_ntohl(sent_hdr->ts);
        c_strm->rtcp.stat.rtp_tx_last_seq = pj_ntohs(sent_hdr->seq);
#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA != 0
        c_strm->last_frm_ts_sent = c_strm->rtcp.stat.rtp_tx_last_ts;
#endif
    }

on_return:
    pj_mutex_unlock(c_strm->jb_mutex);
    return status;
}

/**
 * check_tx_rtcp()
 * This function is for transmitting periodic RTCP SR/RR report.
 */
static void check_tx_rtcp(pjmedia_txt_stream *stream)
{
    pjmedia_stream_common *c_strm = &stream->base;
    pj_timestamp now;
    pj_status_t status;

    pj_get_timestamp(&now);
    if (stream->rtcp_last_tx.u64 == 0) {
        stream->rtcp_last_tx = now;
    } else if (pj_elapsed_msec(&stream->rtcp_last_tx, &now) >=
               c_strm->rtcp_interval) {
        status = send_rtcp(c_strm, !c_strm->rtcp_sdes_bye_disabled, PJ_FALSE,
                           PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE);
        if (status == PJ_SUCCESS) {
            stream->rtcp_last_tx = now;
        }
    }
}

/* Clock callback */
static void clock_cb(const pj_timestamp *ts, void *user_data)
{
    pjmedia_txt_stream *stream = (pjmedia_txt_stream *) user_data;
    pj_timestamp now;
    unsigned interval;

    PJ_UNUSED_ARG(ts);

    pj_get_timestamp(&now);
    interval = pj_elapsed_msec(&stream->tx_last_ts, &now);
    /* If the interval is at least the buffering time, or
     * if the next clock callback occurs past the maximum buffering
     * time, send the text now.
     */
    if (interval >= stream->buf_time ||
        interval + stream->buf_time > MAX_BUFFERING_TIME) {
        send_text(stream, interval);
    }

    /* If we are waiting for the packet gap, and the next clock cb
     * occurs past the maximum waiting time, call the rx callback
     * now.
     */
    if (stream->is_waiting) {
        interval = pj_elapsed_msec(&stream->rx_wait_ts, &now);
        if (interval + stream->buf_time > MAX_RX_WAITING_TIME) {
            call_cb(stream, PJ_TRUE);
        }
    }

    /* Check if now is the time to transmit RTCP SR/RR report. */
    check_tx_rtcp(stream);
}

PJ_DEF(pj_status_t)
pjmedia_txt_stream_send_text(pjmedia_txt_stream *stream, const pj_str_t *text)
{
    pjmedia_stream_common *c_strm = &stream->base;
    pj_timestamp now;
    unsigned interval;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    pj_mutex_lock(c_strm->jb_mutex);

    if (stream->tx_buf[0].length + text->slen > BUFFER_SIZE) {
        pj_mutex_unlock(c_strm->jb_mutex);
        return PJ_ETOOMANY;
    }

    pj_memcpy(stream->tx_buf[stream->tx_buf_idx].buf +
                  stream->tx_buf[stream->tx_buf_idx].length,
              text->ptr, text->slen);
    stream->tx_buf[stream->tx_buf_idx].length += (unsigned) text->slen;

    /* Decide if we should send the text immediately or just buffer it. */
    pj_get_timestamp(&now);
    interval = pj_elapsed_msec(&stream->tx_last_ts, &now);
    if (interval >= stream->buf_time) {
        status = send_text(stream, interval);
    }

    pj_mutex_unlock(c_strm->jb_mutex);

    return status;
}

PJ_DEF(pj_status_t)
pjmedia_txt_stream_set_rx_callback(
    pjmedia_txt_stream *stream,
    void (*cb)(pjmedia_txt_stream *, void *user_data,
               const pjmedia_txt_stream_data *data),
    void *user_data, unsigned option)
{
    pjmedia_stream_common *c_strm = (pjmedia_stream_common *) stream;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    PJ_UNUSED_ARG(option);

    pj_mutex_lock(c_strm->jb_mutex);

    stream->cb = cb;
    stream->cb_user_data = user_data;

    pj_mutex_unlock(c_strm->jb_mutex);

    return PJ_SUCCESS;
}

/**
 * Internal function for collecting codec info from the SDP media.
 */
static pj_status_t get_codec_info(pjmedia_txt_stream_info *si, pj_pool_t *pool,
                                  const pjmedia_sdp_media *local_m,
                                  const pjmedia_sdp_media *rem_m)
{
    static const pj_str_t ID_RTPMAP = {"rtpmap", 6};
    static const pj_str_t ID_REDUNDANCY = {"red", 3};
    const pjmedia_sdp_attr *attr;
    int nameless = 0;
    unsigned i, fmti;
    unsigned rem_red_pt = 0;
    unsigned rem_t140_pt = 0;
    unsigned pt = 0;
    pjmedia_sdp_rtpmap r;

    /* Find Remote RED PT */
    for (i = 0; i < rem_m->desc.fmt_count; ++i) {
        attr =
            pjmedia_sdp_media_find_attr(rem_m, &ID_RTPMAP, &rem_m->desc.fmt[i]);
        if (attr && pjmedia_sdp_attr_get_rtpmap(attr, &r) == PJ_SUCCESS) {
            if (pj_strcmp(&r.enc_name, &ID_REDUNDANCY) == 0) {
                rem_red_pt = (unsigned) pj_strtoul(&rem_m->desc.fmt[i]);
                break;
            }
        }
    }

    /* Find Remote T.140 PT (must not be RED) */
    for (i = 0; i < rem_m->desc.fmt_count; ++i) {
        pt = (unsigned) pj_strtoul(&rem_m->desc.fmt[i]);
        if (pt == rem_red_pt)
            continue;
        attr =
            pjmedia_sdp_media_find_attr(rem_m, &ID_RTPMAP, &rem_m->desc.fmt[i]);
        if (attr && pjmedia_sdp_attr_get_rtpmap(attr, &r) == PJ_SUCCESS) {
            rem_t140_pt = pt;
            break;
        }
    }

    si->tx_red_pt = (pj_uint8_t) rem_red_pt;
    si->tx_pt = (pj_uint8_t) rem_t140_pt;

    /* Get Local RX parameters... */
    for (fmti = 0; fmti < local_m->desc.fmt_count; ++fmti) {
        attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP,
                                           &local_m->desc.fmt[fmti]);
        if (attr && pjmedia_sdp_attr_get_rtpmap(attr, &r) == PJ_SUCCESS) {
            if (pj_strcmp(&r.enc_name, &ID_REDUNDANCY) == 0) {
                si->rx_red_pt =
                    (pj_uint8_t) pj_strtoul(&local_m->desc.fmt[fmti]);
            } else {
                si->rx_pt = (pj_uint8_t) pj_strtoul(&local_m->desc.fmt[fmti]);
            }
        }
    }

    /* Parse redundancy level */
    if (rem_red_pt > 0) {
        if (pjmedia_stream_info_parse_fmtp(pool, rem_m, rem_red_pt,
                                           &si->enc_fmtp) == PJ_SUCCESS) {
            nameless = 0;
            for (i = 0; i < si->enc_fmtp.cnt; ++i) {
                if (si->enc_fmtp.param[i].name.slen == 0)
                    nameless++;
            }
            si->tx_red_level = (nameless > 0) ? (nameless - 1) : 0;
        }
    }

    /* Restore downstream format tracking and fmtp parsing */
    si->fmt.type = PJMEDIA_TYPE_TEXT;
    si->fmt.pt = si->tx_pt;
    pj_strset2(&si->fmt.encoding_name, "t140");
    si->fmt.channel_cnt = 1;
    si->fmt.clock_rate = 1000; /* Enforce standard T.140 clock rate */

    /* Parse local fmtp for the decoder */
    pjmedia_stream_info_parse_fmtp(pool, local_m, si->rx_pt, &si->dec_fmtp);

    /* Re-parse remote fmtp for the T.140 encoder (in case RED overwrote it) */
    pjmedia_stream_info_parse_fmtp(pool, rem_m, si->tx_pt, &si->enc_fmtp);

    /* Enforce dynamic PT range validation (PT >= 96) */
    if (si->tx_pt < 96 || si->rx_pt < 96) {
        return PJMEDIA_EINVALIDPT;
    }

    return PJ_SUCCESS;
}

/**
 * Create stream info from SDP media line.
 */
PJ_DEF(pj_status_t)
pjmedia_txt_stream_info_from_sdp(pjmedia_txt_stream_info *si, pj_pool_t *pool,
                                 pjmedia_endpt *endpt,
                                 const pjmedia_sdp_session *local,
                                 const pjmedia_sdp_session *remote,
                                 unsigned stream_idx)
{
    pjmedia_stream_info_common *csi = (pjmedia_stream_info_common *) si;
    const pjmedia_sdp_media *local_m;
    const pjmedia_sdp_media *rem_m;
    pj_bool_t active;
    pj_status_t status;

    PJ_ASSERT_RETURN(si, PJ_EINVAL);

    pj_bzero(si, sizeof(*si));
    status = pjmedia_stream_info_common_from_sdp(csi, pool, endpt, local,
                                                 remote, stream_idx, &active);
    if (status != PJ_SUCCESS || !active)
        return status;

    /* Keep SDP shortcuts */
    local_m = local->media[stream_idx];
    rem_m = remote->media[stream_idx];

    /* Media type must be text */
    if (pjmedia_get_type(&local_m->desc.media) != PJMEDIA_TYPE_TEXT)
        return PJMEDIA_EINVALIMEDIATYPE;

    /* Get codec info */
    status = get_codec_info(si, pool, local_m, rem_m);

    /* Get redundancy info */
    pjmedia_stream_info_common_parse_redundancy(csi, pool, local_m, rem_m);

    return status;
}

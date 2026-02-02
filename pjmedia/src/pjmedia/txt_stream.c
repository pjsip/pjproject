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
#define BUFFER_SIZE             64

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

    /* Unsubscribe from RTCP session events */
    // Currently unused
    //pjmedia_event_unsubscribe(NULL, &stream_event_cb, stream,
    //                          &c_strm->rtcp);

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
        rtcp_setting.clock_rate = info->fmt.clock_rate;
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

    pj_mutex_lock(c_strm->jb_mutex);

    stream->is_waiting = PJ_FALSE;
    do {
        char frm_buf[BUFFER_SIZE];
        pj_size_t frm_size = sizeof(frm_buf);
        pj_uint32_t ts;

        /* Check if we have a packet with the next sequence number. */
        pjmedia_jbuf_peek_frame(c_strm->jb, 0, NULL, NULL, &frm_type, NULL,
                                NULL, &next_seq);

        /* Jitter buffer is empty, just return. */
        if (frm_type == PJMEDIA_JB_ZERO_EMPTY_FRAME)
            break;

        if (!now && stream->rx_last_seq != -1 &&
            next_seq != (pj_uint16_t)(stream->rx_last_seq + 1))
        {
            /* If analysis of a received packet reveals a gap in the sequence,
             * we need to wait for the missing packet(s) to arrive.
             * The waiting time is determined by MAX_RX_WAITING_TIME,
             */
            stream->is_waiting = PJ_TRUE;
            pj_get_timestamp(&stream->rx_wait_ts);
            break;
        }

        pjmedia_jbuf_get_frame3(c_strm->jb, frm_buf, &frm_size, &frm_type,
                                NULL, &ts, &stream->rx_last_seq);

        /* Call callback. */
        pj_mutex_unlock(c_strm->jb_mutex);
        if (stream->cb) {
            pjmedia_txt_stream_data data;

            data.seq = next_seq;
            data.ts = ts;
            data.text.ptr = frm_buf;
            data.text.slen = frm_size;
            (*stream->cb)(stream, stream->cb_user_data, &data);
        } else {
            TRACE_((c_strm->port.info.name.ptr, "Text data %zu: %.*s",
                    frm_size, (int)frm_size, frm_buf));
        }
        pj_mutex_lock(c_strm->jb_mutex);
    } while (1);

    pj_mutex_unlock(c_strm->jb_mutex);
}

static pj_status_t decode_red(pjmedia_txt_stream *stream,
                              unsigned pt, int seq,
                              const char *buf, unsigned buflen,
                              unsigned *red_len)
{
    pjmedia_stream_common *c_strm = &stream->base;
    pjmedia_rtp_add_hdr hdr[NUM_BUFFERS];
    int i, len = 0, level = 0;
    /* Acceptable seq delta for redundancy packets.
     * The value is the same as the one used in rtp and jbuf
     * for determining late packets.
     */
    enum { MAX_DELTA = 100 };

    /* Read the headers. */
    do {
        pjmedia_rtp_add_hdr_short shdr;

        if (len + sizeof(shdr) > buflen)
            return PJ_ETOOBIG;

        pj_memcpy(&shdr, buf, sizeof(shdr));
        if (shdr.pt != (pj_uint8_t)pt) {
            /* Bad PT, the PT is different from the expected media PT. */
            return PJMEDIA_EINVALIDPT;
        }
        if (shdr.f == 0) {
            /* Zero F bit indicating final block. */
            buf += sizeof(shdr);
            len += sizeof(shdr);
            break;
        }

        /* Redundancy level higher than what we can support. */
        if (level >= c_strm->si->rx_red_level || level >= NUM_BUFFERS)
            return PJ_ETOOBIG;
        if (len + sizeof(hdr[level]) > buflen)
            return PJ_ETOOBIG;

        /* Store the headers. */
        pj_memcpy(&hdr[level], buf, sizeof(hdr[level]));
        hdr[level] = pj_ntohl(hdr[level]);
        buf += sizeof(hdr[level]);
        len += sizeof(hdr[level]);

        level++;
    } while (1);

    /* Read the redundant data. */
    for (i = 0; i < level; i++) {
        /* Bit 22-31 is block length. */
        unsigned length = (hdr[i] & 0x3FF);
        int ext_seq;
        pj_uint16_t delta;

        if (length == 0)
            continue;
        if (len + length > buflen)
            return PJ_ETOOBIG;

        ext_seq = (pj_uint16_t)(seq + i - level);
        TRACE_((c_strm->port.info.name.ptr, "Received RTP red, seq: %d, "
                "%.*s (%d bytes)", ext_seq, (int)length, buf, length));

        delta = (pj_uint16_t) (stream->rx_last_seq - ext_seq);
        if (delta < MAX_DELTA) {
            /* Redundant packets that we already have in jbuf, do nothing. */
        } else {
            pjmedia_jbuf_put_frame(c_strm->jb, buf, length, ext_seq);
        }

        buf += length;
        len += length;
    }

    *red_len = len;

    return PJ_SUCCESS;
}

static pj_status_t on_stream_rx_rtp(pjmedia_stream_common *c_strm,
                                    const pjmedia_rtp_hdr *hdr,
                                    const void *payload,
                                    unsigned payloadlen,
                                    pjmedia_rtp_status seq_st,
                                    pj_bool_t *pkt_discarded)
{
    pjmedia_txt_stream *stream = (pjmedia_txt_stream *)c_strm;
    unsigned red_len = 0;
    int seq;
    pj_status_t status;

    pj_mutex_lock(c_strm->jb_mutex);

    if (seq_st.status.flag.restart) {
        stream->rx_last_seq = -1;
        pjmedia_jbuf_reset(c_strm->jb);
        PJ_LOG(4, (c_strm->port.info.name.ptr, "Jitter buffer reset"));
    }

    seq = pj_ntohs(hdr->seq);

    if (hdr->pt == c_strm->si->rx_red_pt) {
        status = decode_red(stream, c_strm->si->rx_pt, seq,
                            (const char *)payload, payloadlen, &red_len);
        if (status != PJ_SUCCESS) {
            *pkt_discarded = PJ_TRUE;
            pj_mutex_unlock(c_strm->jb_mutex);
            return status;
        }
    }

    /* Put the primary data into jbuf. */
    payload = ((pj_uint8_t*)payload) + red_len;
    payloadlen -= red_len;
    if (seq > stream->rx_last_seq) {
        pjmedia_jbuf_put_frame(c_strm->jb, payload, payloadlen, seq);
    }

    pj_mutex_unlock(c_strm->jb_mutex);

    /* Check if we need to call the callback. */
    call_cb(stream, PJ_FALSE);

    return PJ_SUCCESS;
}

static pj_status_t encode_red(unsigned level, unsigned pt,
                              red_buf *rbuf, unsigned rbuf_idx,
                              char *buf, int *size)
{
    int i;
    unsigned len = 0;

    /* Encode RTP additional headers for redundancy. */
    for (i = level; i > 0; i--) {
        pjmedia_rtp_add_hdr_short shdr;
        pjmedia_rtp_add_hdr hdr = 0;
        int past_idx, offset;

        past_idx = (int)rbuf_idx - i;
        if (past_idx < 0) past_idx += NUM_BUFFERS;

        /* 1 means not final. */
        shdr.f = 1;
        shdr.pt = (pj_uint8_t)pt;

        /* Timestamp is an offset, not absolute. */
        offset = rbuf[rbuf_idx].timestamp - rbuf[past_idx].timestamp;
        /* 4.1. Redundant data with timestamp offset > 16383 MUST NOT
         * be included.
         */
        if (offset > 16383) {
            offset = 0;
            rbuf[past_idx].length = 0;
        }
        hdr = offset << 10;

        /* Block length. */
        hdr |= rbuf[past_idx].length;

        if (len + sizeof(hdr) > (unsigned)*size)
            return PJ_ETOOBIG;
        hdr = pj_htonl(hdr);
        pj_memcpy(buf, &hdr, sizeof(hdr));
        pj_memcpy(buf, &shdr, sizeof(shdr));
        buf += sizeof(hdr);
        len += sizeof(hdr);
    }

    if (level > 0) {
        pjmedia_rtp_add_hdr_short hdr;

        /* Last RTP additional header, for the primary data. */
        hdr.f = 0;
        hdr.pt = (pj_uint8_t)pt;
        if (len + sizeof(hdr) > (unsigned)*size)
            return PJ_ETOOBIG;
        pj_memcpy(buf, &hdr, sizeof(hdr));
        buf += sizeof(hdr);
        len += sizeof(hdr);
    }

    /* Encode the redundant data placed in age order (with the most
     * recent put last), and finally, the primary data.
     */
    for (i = level; i >= 0; i--) {
        int idx = (int)rbuf_idx - i;
        if (idx < 0) idx += NUM_BUFFERS;

        if (rbuf[idx].length == 0)
            continue;
        if (len + rbuf[idx].length > (unsigned)*size)
            return PJ_ETOOBIG;
        pj_memcpy(buf, rbuf[idx].buf, rbuf[idx].length);
        buf += rbuf[idx].length;
        len += rbuf[idx].length;
    }

    *size = len;
    return PJ_SUCCESS;
}


static pj_status_t send_text(pjmedia_txt_stream *stream,
                             unsigned rtp_ts_len)
{
    pjmedia_stream_common *c_strm = &stream->base;
    pjmedia_channel *channel = c_strm->enc;
    void *rtphdr;
    int size;
    unsigned pt = 0;
    pj_status_t status = PJ_SUCCESS;

    pj_mutex_lock(c_strm->jb_mutex);

    if (stream->is_idle &&
        stream->tx_buf[stream->tx_buf_idx].length == 0)
    {
        /* Nothing to send. */
        goto on_return;
    }

    pt = (stream->si.tx_red_pt && stream->tx_nred)?
         stream->si.tx_red_pt: channel->pt;

    status = pjmedia_rtp_encode_rtp( &channel->rtp,
                                     pt, 0,
                                     (int)channel->buf_size, rtp_ts_len,
                                     (const void**)&rtphdr,
                                     &size);

    TRACE_((c_strm->port.info.name.ptr, "Sending text with seq %d, %.*s "
            "(%d bytes), pt:%d red:%d",
            pj_ntohs(channel->rtp.out_hdr.seq),
            (int)stream->tx_buf[stream->tx_buf_idx].length,
            stream->tx_buf[stream->tx_buf_idx].buf,
            (int)stream->tx_buf[stream->tx_buf_idx].length,
            pt, stream->tx_nred));

    if (status != PJ_SUCCESS)
        goto on_return;

    /* Copy RTP header to the beginning of packet */
    pj_memcpy(channel->buf, rtphdr, sizeof(pjmedia_rtp_hdr));

    if (stream->is_idle) {
        stream->is_idle = PJ_FALSE;
        /* Set M-bit for the first packet in a session, and the first
         * packet after an idle period.
         */
        ((pjmedia_rtp_hdr *)channel->buf)->m = 1;
    }

    size = channel->buf_size - sizeof(pjmedia_rtp_hdr);
    status = encode_red(stream->tx_nred, channel->pt,
                        stream->tx_buf, stream->tx_buf_idx,
                        ((char*)channel->buf) + sizeof(pjmedia_rtp_hdr),
                        &size);
    if (status != PJ_SUCCESS)
        goto on_return;

    if (stream->tx_buf[stream->tx_buf_idx].length == 0) {
        int i;

        /* We are in idle if:
         * - no new T.140 data is available for transmission, and
         * - there's no more redundancy packet to send.
         */
        stream->is_idle = PJ_TRUE;
        for (i = 0; i < stream->tx_nred - 1; i++) {
            int idx = (int)stream->tx_buf_idx - 1 - i;
            if (idx < 0) idx += NUM_BUFFERS;
            if (stream->tx_buf[idx].length != 0) {
                stream->is_idle = PJ_FALSE;
                break;
            }
        }

        /* If idle, reset redundancy. */
        if (stream->is_idle) stream->tx_nred = 0;
    } else if (stream->tx_nred < stream->si.tx_red_level) {
        /* We have data, increase the number of redundancies. */
        stream->tx_nred++;
    }

    stream->tx_buf_idx = (stream->tx_buf_idx + 1) % NUM_BUFFERS;
    stream->tx_buf[stream->tx_buf_idx].length = 0;

    pj_mutex_unlock(c_strm->jb_mutex);

    /* Send the RTP packet to the transport. */
    status = pjmedia_transport_send_rtp(c_strm->transport, channel->buf,
                                        size + sizeof(pjmedia_rtp_hdr));

    pj_mutex_lock(c_strm->jb_mutex);
    if (status == PJ_SUCCESS) {
        pj_get_timestamp(&stream->tx_last_ts);

        /* Update stat */
        pjmedia_rtcp_tx_rtp(&c_strm->rtcp, (unsigned)size);
        c_strm->rtcp.stat.rtp_tx_last_ts =
            pj_ntohl(c_strm->enc->rtp.out_hdr.ts);
        c_strm->rtcp.stat.rtp_tx_last_seq =
            pj_ntohs(c_strm->enc->rtp.out_hdr.seq);

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
        /* Update time of last sending packet. */
        pj_gettimeofday(&c_strm->last_frm_ts_sent);
#endif

    } else {
        TRACE_((c_strm->port.info.name.ptr, "Failed sending text %d", status));
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

    pj_get_timestamp(&now);
    if (stream->rtcp_last_tx.u64 == 0) {
        stream->rtcp_last_tx = now;
    } else if (pj_elapsed_msec(&stream->rtcp_last_tx, &now) >=
               c_strm->rtcp_interval)
    {
        pj_status_t status;

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
    pjmedia_txt_stream *stream = (pjmedia_txt_stream *)user_data;
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
        interval + stream->buf_time > MAX_BUFFERING_TIME)
    {
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

    if (stream->tx_buf[stream->tx_buf_idx].length + text->slen > BUFFER_SIZE)
    {
        pj_mutex_unlock(c_strm->jb_mutex);
        return PJ_ETOOMANY;
    }

    pj_memcpy(stream->tx_buf[stream->tx_buf_idx].buf +
              stream->tx_buf[stream->tx_buf_idx].length,
              text->ptr, text->slen);
    stream->tx_buf[stream->tx_buf_idx].length += (unsigned)text->slen;

    /* Decide if we should send the text immediately or just buffer it. */
    pj_get_timestamp(&now);
    interval = pj_elapsed_msec(&stream->tx_last_ts, &now);
    if (interval >= stream->buf_time) {
        status = send_text(stream, interval);
    }

    pj_mutex_unlock(c_strm->jb_mutex);

    return status;
}

PJ_DEF(pj_status_t) pjmedia_txt_stream_set_rx_callback(
                        pjmedia_txt_stream *stream,
                        void (*cb)(pjmedia_txt_stream *,
                                   void *user_data,
                                   const pjmedia_txt_stream_data *data),
                        void *user_data,
                        unsigned option)
{
    pjmedia_stream_common *c_strm = (pjmedia_stream_common *)stream;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    PJ_UNUSED_ARG(option);

    pj_mutex_lock(c_strm->jb_mutex);

    stream->cb = cb;
    stream->cb_user_data = user_data;

    pj_mutex_unlock(c_strm->jb_mutex);

    return PJ_SUCCESS;
}

/*
 * Internal function for collecting codec info from the SDP media.
 */
static pj_status_t get_codec_info(pjmedia_txt_stream_info *si,
                                  pj_pool_t *pool,
                                  const pjmedia_sdp_media *local_m,
                                  const pjmedia_sdp_media *rem_m)
{
    static const pj_str_t ID_RTPMAP = { "rtpmap", 6 };
    static const pj_str_t ID_REDUNDANCY = { "red", 3 };
    const pjmedia_sdp_attr *attr;
    pjmedia_sdp_rtpmap *rtpmap;
    unsigned i, fmti, pt = 0;
    pj_status_t status = PJ_SUCCESS;

    /* Find the first pt which is not redundancy */
    for (fmti = 0; fmti < local_m->desc.fmt_count; ++fmti) {
        pjmedia_sdp_rtpmap r;

        if (!pj_isdigit(*local_m->desc.fmt[fmti].ptr))
            return PJMEDIA_EINVALIDPT;

        attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP,
                                           &local_m->desc.fmt[fmti]);
        if (attr == NULL)
            continue;

        status = pjmedia_sdp_attr_get_rtpmap(attr, &r);
        if (status != PJ_SUCCESS)
            continue;

        if (pj_strcmp(&r.enc_name, &ID_REDUNDANCY) != 0)
            break;
    }
    if (fmti >= local_m->desc.fmt_count)
        return PJMEDIA_EINVALIDPT;

    pt = pj_strtoul(&local_m->desc.fmt[fmti]);
    if (pt < 96)
        return PJMEDIA_EINVALIDPT;

    /* Get payload type for receiving direction */
    si->rx_pt = pt;

    /* Get codec info. */
    attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP,
                                       &local_m->desc.fmt[fmti]);
    if (attr == NULL)
        return PJMEDIA_EMISSINGRTPMAP;

    status = pjmedia_sdp_attr_to_rtpmap(pool, attr, &rtpmap);
    if (status != PJ_SUCCESS)
        return status;

    /* Build codec format info: */
    si->fmt.type = si->type;
    si->fmt.pt = pj_strtoul(&local_m->desc.fmt[fmti]);
    si->fmt.encoding_name = rtpmap->enc_name;
    si->fmt.clock_rate = rtpmap->clock_rate;
    si->fmt.channel_cnt = 1;

    /* Determine payload type for outgoing channel, by finding
     * dynamic payload type in remote SDP that matches the answer.
     */
    si->tx_pt = 0xFFFF;
    for (i=0; i<rem_m->desc.fmt_count; ++i) {
        if (pjmedia_sdp_neg_fmt_match(pool,
                                      (pjmedia_sdp_media*)local_m, fmti,
                                      (pjmedia_sdp_media*)rem_m, i, fmti) ==
            PJ_SUCCESS)
        {
            /* Found matched codec. */
            si->tx_pt = pj_strtoul(&rem_m->desc.fmt[i]);
            break;
        }
    }

    if (si->tx_pt == 0xFFFF)
        return PJMEDIA_EMISSINGRTPMAP;

    /* Get remote fmtp for our encoder. */
    pjmedia_stream_info_parse_fmtp(pool, rem_m, si->tx_pt,
                                   &si->enc_fmtp);

    /* Get local fmtp for our decoder. */
    pjmedia_stream_info_parse_fmtp(pool, local_m, si->rx_pt,
                                   &si->dec_fmtp);

    return PJ_SUCCESS;
}

/*
 * Create stream info from SDP media line.
 */
PJ_DEF(pj_status_t) pjmedia_txt_stream_info_from_sdp(
                                           pjmedia_txt_stream_info *si,
                                           pj_pool_t *pool,
                                           pjmedia_endpt *endpt,
                                           const pjmedia_sdp_session *local,
                                           const pjmedia_sdp_session *remote,
                                           unsigned stream_idx)
{
    pjmedia_stream_info_common *csi = (pjmedia_stream_info_common *)si;
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

/* 
 * Copyright (C) 2025 Teluu Inc. (http://www.teluu.com)
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

/* Prototypes. */
/* Specific stream implementation's RX RTP handler. */
static pj_status_t on_stream_rx_rtp(pjmedia_stream_common *c_strm,
                                    const pjmedia_rtp_hdr *hdr,
                                    const void *payload,
                                    unsigned payloadlen,
                                    pjmedia_rtp_status seq_st,
                                    pj_bool_t *pkt_discarded);

/* Specific stream implementation's destroy handler. */
static void on_stream_destroy(void *arg);

#if TRACE_JB

#include <pj/file_io.h>

#define TRACE_JB_INVALID_FD          ((pj_oshandle_t)-1)
#define TRACE_JB_OPENED(s)           (s->trace_jb_fd != TRACE_JB_INVALID_FD)

PJ_INLINE(int) trace_jb_print_timestamp(char **buf, pj_ssize_t len)
{
    pj_time_val now;
    pj_parsed_time ptime;
    char *p = *buf;

    if (len < 14)
        return -1;

    pj_gettimeofday(&now);
    pj_time_decode(&now, &ptime);
    p += pj_utoa_pad(ptime.hour, p, 2, '0');
    *p++ = ':';
    p += pj_utoa_pad(ptime.min, p, 2, '0');
    *p++ = ':';
    p += pj_utoa_pad(ptime.sec, p, 2, '0');
    *p++ = '.';
    p += pj_utoa_pad(ptime.msec, p, 3, '0');
    *p++ = ',';

    *buf = p;

    return 0;
}

PJ_INLINE(int) trace_jb_print_state(pjmedia_stream_common *c_strm,
                                    char **buf, pj_ssize_t len)
{
    char *p = *buf;
    char *endp = *buf + len;
    pjmedia_jb_state state;

    pjmedia_jbuf_get_state(c_strm->jb, &state);

    len = pj_ansi_snprintf(p, endp-p, "%d, %d, %d",
                           state.size, state.burst, state.prefetch);
    if ((len < 0) || (len >= endp-p))
        return -1;

    p += len;
    *buf = p;
    return 0;
}

static void trace_jb_get(pjmedia_stream_common *c_strm, pjmedia_jb_frame_type ft,
                         pj_size_t fsize)
{
    char *p = c_strm->trace_jb_buf;
    char *endp = c_strm->trace_jb_buf + PJ_LOG_MAX_SIZE;
    pj_ssize_t len = 0;
    const char* ft_st;

    if (!TRACE_JB_OPENED(c_strm))
        return;

    /* Print timestamp. */
    if (trace_jb_print_timestamp(&p, endp-p))
        goto on_insuff_buffer;

    /* Print frame type and size */
    switch(ft) {
        case PJMEDIA_JB_MISSING_FRAME:
            ft_st = "missing";
            break;
        case PJMEDIA_JB_NORMAL_FRAME:
            ft_st = "normal";
            break;
        case PJMEDIA_JB_ZERO_PREFETCH_FRAME:
            ft_st = "prefetch";
            break;
        case PJMEDIA_JB_ZERO_EMPTY_FRAME:
            ft_st = "empty";
            break;
        default:
            ft_st = "unknown";
            break;
    }

    /* Print operation, size, frame count, frame type */
    len = pj_ansi_snprintf(p, endp-p, "GET,%zu,1,%s,,,,", fsize, ft_st);
    if ((len < 0) || (len >= endp-p))
        goto on_insuff_buffer;
    p += len;

    /* Print JB state */
    if (trace_jb_print_state(c_strm, &p, endp-p))
        goto on_insuff_buffer;

    /* Print end of line */
    if (endp-p < 2)
        goto on_insuff_buffer;
    *p++ = '\n';

    /* Write and flush */
    len = p - c_strm->trace_jb_buf;
    pj_file_write(c_strm->trace_jb_fd, c_strm->trace_jb_buf, &len);
    pj_file_flush(c_strm->trace_jb_fd);
    return;

on_insuff_buffer:
    pj_assert(!"Trace buffer too small, check PJ_LOG_MAX_SIZE!");
}

static void trace_jb_put(pjmedia_stream_common *c_strm,
                         const pjmedia_rtp_hdr *hdr,
                         unsigned payloadlen, unsigned frame_cnt)
{
    char *p = c_strm->trace_jb_buf;
    char *endp = c_strm->trace_jb_buf + PJ_LOG_MAX_SIZE;
    pj_ssize_t len = 0;

    if (!TRACE_JB_OPENED(c_strm))
        return;

    /* Print timestamp. */
    if (trace_jb_print_timestamp(&p, endp-p))
        goto on_insuff_buffer;

    /* Print operation, size, frame count, RTP info */
    len = pj_ansi_snprintf(p, endp-p,
                           "PUT,%d,%d,,%d,%d,%d,",
                           payloadlen, frame_cnt,
                           pj_ntohs(hdr->seq), pj_ntohl(hdr->ts), hdr->m);
    if ((len < 0) || (len >= endp-p))
        goto on_insuff_buffer;
    p += len;

    /* Print JB state */
    if (trace_jb_print_state(c_strm, &p, endp-p))
        goto on_insuff_buffer;

    /* Print end of line */
    if (endp-p < 2)
        goto on_insuff_buffer;
    *p++ = '\n';

    /* Write and flush */
    len = p - c_strm->trace_jb_buf;
    pj_file_write(c_strm->trace_jb_fd, c_strm->trace_jb_buf, &len);
    pj_file_flush(c_strm->trace_jb_fd);
    return;

on_insuff_buffer:
    pj_assert(!"Trace buffer too small, check PJ_LOG_MAX_SIZE!");
}

#endif /* TRACE_JB */


static pj_status_t send_rtcp(pjmedia_stream_common *c_strm,
                             pj_bool_t with_sdes,
                             pj_bool_t with_bye,
                             pj_bool_t with_xr,
                             pj_bool_t with_fb,
                             pj_bool_t with_fb_nack,
                             pj_bool_t with_fb_pli)
{
    return pjmedia_stream_send_rtcp(c_strm, with_sdes, with_bye,
                                    with_xr, with_fb, with_fb_nack,
                                    with_fb_pli);
}


#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA != 0
/*
 * Send keep-alive packet using non-codec frame.
 */
static void send_keep_alive_packet(pjmedia_stream_common *c_strm)
{
#if PJMEDIA_STREAM_ENABLE_KA == PJMEDIA_STREAM_KA_EMPTY_RTP

    /* Keep-alive packet is empty RTP */
    pj_status_t status;
    void *pkt;
    int pkt_len;

    if (!c_strm->transport)
        return;

    TRC_((c_strm->port.info.name.ptr,
          "Sending keep-alive (RTCP and empty RTP)"));

    /* Send RTP */
    status = pjmedia_rtp_encode_rtp( &c_strm->enc->rtp,
                                     c_strm->enc->pt, 0,
                                     1,
                                     0,
                                     (const void**)&pkt,
                                     &pkt_len);
    pj_assert(status == PJ_SUCCESS);

    pj_memcpy(c_strm->enc->buf, pkt, pkt_len);
    pjmedia_transport_send_rtp(c_strm->transport, c_strm->enc->buf,
                               pkt_len);

    /* Send RTCP */
    send_rtcp(c_strm, PJ_TRUE, PJ_FALSE, PJ_FALSE, PJ_FALSE,
              PJ_FALSE, PJ_FALSE);

    /* Update stats in case the stream is paused */
    c_strm->rtcp.stat.rtp_tx_last_seq = pj_ntohs(c_strm->enc->rtp.out_hdr.seq);

#elif PJMEDIA_STREAM_ENABLE_KA == PJMEDIA_STREAM_KA_USER

    /* Keep-alive packet is defined in PJMEDIA_STREAM_KA_USER_PKT */
    pjmedia_channel *channel = c_strm->enc;
    int pkt_len;
    const pj_str_t str_ka = PJMEDIA_STREAM_KA_USER_PKT;

    TRC_((c_strm->port.info.name.ptr,
          "Sending keep-alive (custom RTP/RTCP packets)"));

    /* Send to RTP port */
    pj_memcpy(c_strm->enc->buf, str_ka.ptr, str_ka.slen);
    pkt_len = str_ka.slen;
    pjmedia_transport_send_rtp(c_strm->transport, c_strm->enc->buf,
                               pkt_len);

    /* Send to RTCP port */
    pjmedia_transport_send_rtcp(c_strm->transport, c_strm->enc->buf,
                                pkt_len);

#else

    PJ_UNUSED_ARG(stream);

#endif
}
#endif  /* defined(PJMEDIA_STREAM_ENABLE_KA) */

/**
 * Publish transport error event.
 */
static void publish_tp_event(pjmedia_event_type event_type,
                             pj_status_t status,
                             pj_bool_t is_rtp,
                             pjmedia_dir dir,
                             pjmedia_stream_common *stream)
{
    pjmedia_event ev;
    pj_timestamp ts_now;

    pj_get_timestamp(&ts_now);
    pj_bzero(&ev.data.med_tp_err, sizeof(ev.data.med_tp_err));

    /* Publish event. */
    pjmedia_event_init(&ev, event_type,
                       &ts_now, stream);
    ev.data.med_tp_err.type = stream->si->type;
    ev.data.med_tp_err.is_rtp = is_rtp;
    ev.data.med_tp_err.dir = dir;
    ev.data.med_tp_err.status = status;

    pjmedia_event_publish(NULL, stream, &ev, 0);
}

/*
 * This callback is called by stream transport on receipt of packets
 * in the RTCP socket.
 */
static void on_rx_rtcp( void *data,
                        void *pkt,
                        pj_ssize_t bytes_read)
{
    pjmedia_stream_common *c_strm = (pjmedia_stream_common *)data;
    pj_status_t status;

    /* Check for errors */
    if (bytes_read < 0) {
        status = (pj_status_t)-bytes_read;
        if (status == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {
            return;
        }
        LOGERR_((c_strm->port.info.name.ptr, status,
                         "Unable to receive RTCP packet"));

        if (status == PJ_ESOCKETSTOP) {
            /* Publish receive error event. */
            publish_tp_event(PJMEDIA_EVENT_MEDIA_TP_ERR, status, PJ_FALSE,
                             PJMEDIA_DIR_DECODING, c_strm);
        }
        return;
    }

    pjmedia_rtcp_rx_rtcp(&c_strm->rtcp, pkt, bytes_read);
}

/*
 * This callback is called by stream transport on receipt of packets
 * in the RTP socket.
 */
static void on_rx_rtp( pjmedia_tp_cb_param *param)
{
#ifdef AUDIO_STREAM
    pjmedia_stream *stream = (pjmedia_stream*) param->user_data;
#endif
    pjmedia_stream_common *c_strm = (pjmedia_stream_common *)
                                    param->user_data;
    void *pkt = param->pkt;
    pj_ssize_t bytes_read = param->size;
    pjmedia_channel *channel = c_strm->dec;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;
    pjmedia_rtp_status seq_st;
    pj_bool_t check_pt;
    pj_status_t status;
    pj_bool_t pkt_discarded = PJ_FALSE;

    /* Check for errors */
    if (bytes_read < 0) {
        status = (pj_status_t)-bytes_read;
        if (status == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {
            return;
        }

        LOGERR_((c_strm->port.info.name.ptr, status,
                 "Unable to receive RTP packet"));

        if (status == PJ_ESOCKETSTOP) {
            /* Publish receive error event. */
            publish_tp_event(PJMEDIA_EVENT_MEDIA_TP_ERR, status, PJ_TRUE,
                             PJMEDIA_DIR_DECODING, c_strm);
        }
        return;
    }

    /* Ignore non-RTP keep-alive packets */
    if (bytes_read < (pj_ssize_t) sizeof(pjmedia_rtp_hdr))
        return;

    /* Update RTP and RTCP session. */
    status = pjmedia_rtp_decode_rtp(&channel->rtp, pkt, (int)bytes_read,
                                    &hdr, &payload, &payloadlen);
    if (status != PJ_SUCCESS) {
        LOGERR_((c_strm->port.info.name.ptr, status, "RTP decode error"));
        c_strm->rtcp.stat.rx.discard++;
        return;
    }

    /* Check if multiplexing is allowed and the payload indicates RTCP. */
    if (c_strm->si->rtcp_mux && hdr->pt >= 64 && hdr->pt <= 95) {
        on_rx_rtcp(c_strm, pkt, bytes_read);
        return;
    }

    /* See if source address of RTP packet is different than the
     * configured address, and check if we need to tell the
     * media transport to switch RTP remote address.
     */
    if (param->src_addr) {
        pj_uint32_t peer_ssrc = channel->rtp.peer_ssrc;
        pj_bool_t badssrc = PJ_FALSE;

        /* Check SSRC. */
        if (!channel->rtp.has_peer_ssrc && peer_ssrc == 0)
            peer_ssrc = pj_ntohl(hdr->ssrc);

        if ((c_strm->si->has_rem_ssrc) && (pj_ntohl(hdr->ssrc) != peer_ssrc)) {
            badssrc = PJ_TRUE;
        }

        if (pj_sockaddr_cmp(&c_strm->rem_rtp_addr, param->src_addr) == 0) {
            /* We're still receiving from rem_rtp_addr. */
            c_strm->rtp_src_cnt = 0;
            c_strm->rem_rtp_flag = badssrc? 2: 1;
        } else {
            c_strm->rtp_src_cnt++;

            if (c_strm->rtp_src_cnt < PJMEDIA_RTP_NAT_PROBATION_CNT) {
                if (c_strm->rem_rtp_flag == 1 ||
                    (c_strm->rem_rtp_flag == 2 && badssrc))
                {
                    /* Only discard if:
                     * - we have ever received packet with good ssrc from
                     *   remote address (rem_rtp_addr), or
                     * - we have ever received packet with bad ssrc from
                     *   remote address and this packet also has bad ssrc.
                     */
                    return;                 
                }
                if (!badssrc && c_strm->rem_rtp_flag != 1)
                {
                    /* Immediately switch if we receive packet with the
                     * correct ssrc AND we never receive packets with
                     * good ssrc from rem_rtp_addr.
                     */
                    param->rem_switch = PJ_TRUE;
                }
            } else {
                /* Switch. We no longer receive packets from rem_rtp_addr. */
                param->rem_switch = PJ_TRUE;
            }

            if (param->rem_switch) {
                /* Set remote RTP address to source address */
                pj_sockaddr_cp(&c_strm->rem_rtp_addr, param->src_addr);

                /* Reset counter and flag */
                c_strm->rtp_src_cnt = 0;
                c_strm->rem_rtp_flag = badssrc? 2: 1;

                /* Update RTCP peer ssrc */
                c_strm->rtcp.peer_ssrc = pj_ntohl(hdr->ssrc);
            }
        }
    }

    /* Add ref counter to avoid premature destroy from callbacks */
    pj_grp_lock_add_ref(c_strm->grp_lock);

    /* Ignore the packet if decoder is paused */
    if (channel->paused)
        goto on_return;

    /* Update RTP session (also checks if RTP session can accept
     * the incoming packet.
     */
    pj_bzero(&seq_st, sizeof(seq_st));
    check_pt = PJMEDIA_STREAM_CHECK_RTP_PT;
#ifdef AUDIO_STREAM
    check_pt = check_pt && hdr->pt != stream->rx_event_pt;
#endif
    pjmedia_rtp_session_update2(&channel->rtp, hdr, &seq_st, check_pt);
#if !PJMEDIA_STREAM_CHECK_RTP_PT
    if (!check_pt && hdr->pt != channel->rtp.out_pt) {
#ifdef AUDIO_STREAM
        if (hdr->pt != stream->rx_event_pt)
#endif
        seq_st.status.flag.badpt = -1;
    }
#endif
    if (seq_st.status.value) {
        TRC_  ((c_strm->port.info.name.ptr,
                "RTP status: badpt=%d, badssrc=%d, dup=%d, "
                "outorder=%d, probation=%d, restart=%d",
                seq_st.status.flag.badpt,
                seq_st.status.flag.badssrc,
                seq_st.status.flag.dup,
                seq_st.status.flag.outorder,
                seq_st.status.flag.probation,
                seq_st.status.flag.restart));

        if (seq_st.status.flag.badpt) {
            PJ_LOG(4,(c_strm->port.info.name.ptr,
                      "Bad RTP pt %d (expecting %d)",
                      hdr->pt, channel->rtp.out_pt));
        }

        if (!c_strm->si->has_rem_ssrc && seq_st.status.flag.badssrc) {
            PJ_LOG(4,(c_strm->port.info.name.ptr,
                      "Changed RTP peer SSRC %d (previously %d)",
                      channel->rtp.peer_ssrc, c_strm->rtcp.peer_ssrc));
            c_strm->rtcp.peer_ssrc = channel->rtp.peer_ssrc;
        }


    }

    /* Skip bad RTP packet */
    if (seq_st.status.flag.bad) {
        pkt_discarded = PJ_TRUE;
        goto on_return;
    }

    /* Ignore if payloadlen is zero */
    if (payloadlen == 0) {
        pkt_discarded = PJ_TRUE;
        goto on_return;
    }

    /* Pass it to specific stream for further processing. */
    on_stream_rx_rtp(c_strm, hdr, payload, payloadlen, seq_st,
                     &pkt_discarded);

on_return:
    /* Update RTCP session */
    if (c_strm->rtcp.peer_ssrc == 0)
        c_strm->rtcp.peer_ssrc = channel->rtp.peer_ssrc;

    pjmedia_rtcp_rx_rtp2(&c_strm->rtcp, pj_ntohs(hdr->seq),
                         pj_ntohl(hdr->ts), payloadlen, pkt_discarded);

    /* RTCP-FB generic NACK */
    if (c_strm->rtcp.received >= 10 && seq_st.diff > 1 &&
        c_strm->send_rtcp_fb_nack && pj_ntohs(hdr->seq) >= seq_st.diff)
    {
        pj_uint16_t nlost, first_seq;

        /* Report only one NACK (last 17 losts) */
        nlost = PJ_MIN(seq_st.diff - 1, 17);
        first_seq = pj_ntohs(hdr->seq) - nlost;

        pj_bzero(&c_strm->rtcp_fb_nack, sizeof(c_strm->rtcp_fb_nack));
        c_strm->rtcp_fb_nack.pid = first_seq;
        while (--nlost) {
            c_strm->rtcp_fb_nack.blp <<= 1;
            c_strm->rtcp_fb_nack.blp |= 1;
        }

        /* Send it immediately */
        status = send_rtcp(c_strm, !c_strm->rtcp_sdes_bye_disabled,
                           PJ_FALSE, PJ_FALSE, PJ_TRUE, PJ_FALSE, PJ_FALSE);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(c_strm->port.info.name.ptr, status,
                      "Error sending RTCP FB generic NACK"));
        } else {
            c_strm->initial_rr = PJ_TRUE;
        }
    }

    /* Send RTCP RR and SDES after we receive some RTP packets */
    if (c_strm->rtcp.received >= 10 && !c_strm->initial_rr) {
        status = send_rtcp(c_strm, !c_strm->rtcp_sdes_bye_disabled,
                           PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_FALSE);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(c_strm->port.info.name.ptr, status,
                     "Error sending initial RTCP RR"));
        } else {
            c_strm->initial_rr = PJ_TRUE;
        }
    }

    pj_grp_lock_dec_ref(c_strm->grp_lock);
}

/* Common stream destroy handler. */
static void on_destroy(void *arg)
{
    pjmedia_stream_common *c_strm = (pjmedia_stream_common *)arg;

    /* This function may be called when stream is partly initialized. */

    /* Call specific stream destroy handler. */
    on_stream_destroy(arg);

    /* Release ref to transport */
    if (c_strm->transport && c_strm->transport->grp_lock)
        pj_grp_lock_dec_ref(c_strm->transport->grp_lock);

    /* Free mutex */
    if (c_strm->jb_mutex) {
        pj_mutex_destroy(c_strm->jb_mutex);
        c_strm->jb_mutex = NULL;
    }

    /* Destroy jitter buffer */
    if (c_strm->jb) {
        pjmedia_jbuf_destroy(c_strm->jb);
        c_strm->jb = NULL;
    }

#if TRACE_JB
    if (TRACE_JB_OPENED(c_strm)) {
        pj_file_close(c_strm->trace_jb_fd);
        c_strm->trace_jb_fd = TRACE_JB_INVALID_FD;
    }
#endif

    PJ_LOG(4,(c_strm->port.info.name.ptr, "Stream destroyed"));
    pj_pool_safe_release(&c_strm->own_pool);
}

/*
 * Copyright (C) 2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/vid_stream.h>
#include <pjmedia/errno.h>
#include <pjmedia/event.h>
#include <pjmedia/jbuf.h>
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/rtcp_fb.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/compat/socket.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/sock_select.h>
#include <pj/string.h>      /* memcpy() */


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)


#define THIS_FILE                       "vid_stream.c"
#define ERRLEVEL                        1
#define LOGERR_(expr)                   PJ_PERROR(4,expr)
#define TRC_(expr)                      PJ_LOG(5,expr)
#define SIGNATURE                       PJMEDIA_SIG_PORT_VID_STREAM

#define TRACE_RC                        0

/* Tracing jitter buffer operations in a stream session to a CSV file.
 * The trace will contain JB operation timestamp, frame info, RTP info, and
 * the JB state right after the operation.
 */
#define TRACE_JB                        0       /* Enable/disable trace.    */
#define TRACE_JB_PATH_PREFIX            ""      /* Optional path/prefix
                                                   for the CSV filename.    */
#if TRACE_JB
#   include <pj/file_io.h>
#   define TRACE_JB_INVALID_FD          ((pj_oshandle_t)-1)
#   define TRACE_JB_OPENED(s)           (s->trace_jb_fd != TRACE_JB_INVALID_FD)
#endif

#ifndef PJMEDIA_VSTREAM_SIZE
#   define PJMEDIA_VSTREAM_SIZE 16000
#endif

#ifndef PJMEDIA_VSTREAM_INC
#   define PJMEDIA_VSTREAM_INC  4000
#endif

/* Due to network MTU limitation, a picture bitstream may be splitted into
 * several chunks for RTP delivery. The chunk number may vary depend on the
 * picture resolution and MTU. This constant specifies the minimum chunk
 * number to be allocated to store a picture bitstream in decoding direction.
 */
#define MIN_CHUNKS_PER_FRM      30

/*  Number of send error before repeat the report. */
#define SEND_ERR_COUNT_TO_REPORT        50

/**
 * Media channel.
 */
typedef struct pjmedia_vid_channel
{
    pjmedia_vid_stream     *stream;         /**< Parent stream.             */
    pjmedia_dir             dir;            /**< Channel direction.         */
    pjmedia_port            port;           /**< Port interface.            */
    unsigned                pt;             /**< Payload type.              */
    pj_bool_t               paused;         /**< Paused?.                   */
    void                   *buf;            /**< Output buffer.             */
    unsigned                buf_size;       /**< Size of output buffer.     */
    pjmedia_rtp_session     rtp;            /**< RTP session.               */
} pjmedia_vid_channel;


/**
 * This structure describes media stream.
 * A media stream is bidirectional media transmission between two endpoints.
 * It consists of two channels, i.e. encoding and decoding channels.
 * A media stream corresponds to a single "m=" line in a SDP session
 * description.
 */
struct pjmedia_vid_stream
{
    pj_pool_t               *own_pool;      /**< Internal pool.             */
    pjmedia_endpt           *endpt;         /**< Media endpoint.            */
    pjmedia_vid_codec_mgr   *codec_mgr;     /**< Codec manager.             */
    pjmedia_vid_stream_info  info;          /**< Stream info.               */
    pj_grp_lock_t           *grp_lock;      /**< Stream lock.               */

    pjmedia_vid_channel     *enc;           /**< Encoding channel.          */
    pjmedia_vid_channel     *dec;           /**< Decoding channel.          */

    pjmedia_dir              dir;           /**< Stream direction.          */
    void                    *user_data;     /**< User data.                 */
    pj_str_t                 name;          /**< Stream name                */
    pj_str_t                 cname;         /**< SDES CNAME                 */

    pjmedia_transport       *transport;     /**< Stream transport.          */

    pjmedia_jbuf            *jb;            /**< Jitter buffer.             */
    char                     jb_last_frm;   /**< Last frame type from jb    */
    unsigned                 jb_last_frm_cnt;/**< Last JB frame type counter*/

    pjmedia_rtcp_session     rtcp;          /**< RTCP for incoming RTP.     */
    pj_timestamp             rtcp_last_tx;  /**< Last RTCP tx time.         */
    pj_timestamp             rtcp_fb_last_tx;/**< Last RTCP-FB tx time.     */
    pj_uint32_t              rtcp_interval; /**< Interval, in msec.         */
    pj_bool_t                initial_rr;    /**< Initial RTCP RR sent       */
    pj_bool_t                rtcp_sdes_bye_disabled;/**< Send RTCP SDES/BYE?*/
    void                    *out_rtcp_pkt;  /**< Outgoing RTCP packet.      */
    unsigned                 out_rtcp_pkt_size;
                                            /**< Outgoing RTCP packet size. */

    unsigned                 dec_max_size;  /**< Size of decoded/raw picture*/
    pjmedia_ratio            dec_max_fps;   /**< Max fps of decoding dir.   */
    pjmedia_frame            dec_frame;     /**< Current decoded frame.     */
    unsigned                 dec_delay_cnt; /**< Decoding delay (in frames).*/
    unsigned                 dec_max_delay; /**< Decoding max delay (in ts).*/
    pjmedia_event            fmt_event;     /**< Buffered fmt_changed event
                                                 to avoid deadlock          */
    pjmedia_event            miss_keyframe_event;
                                            /**< Buffered missing keyframe
                                                 event for delayed republish*/

    unsigned                 frame_size;    /**< Size of encoded base frame.*/
    unsigned                 frame_ts_len;  /**< Frame length in timestamp. */

    unsigned                 rx_frame_cnt;  /**< # of array in rx_frames    */
    pjmedia_frame           *rx_frames;     /**< Temp. buffer for incoming
                                                 frame assembly.            */
    pj_bool_t                force_keyframe;/**< Forced to encode keyframe? */
    unsigned                 num_keyframe;  /**< The number of keyframe needed
                                                 to be sent, e.g: after the
                                                 stream is created. */
    pj_timestamp             last_keyframe_tx;
                                            /**< Timestamp of the last
                                                 keyframe. */


#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    pj_bool_t                use_ka;           /**< Stream keep-alive with non-
                                                    codec-VAD mechanism is
                                                    enabled?                */
    unsigned                 ka_interval;      /**< The keepalive sending 
                                                    interval                */
    pj_time_val              last_frm_ts_sent; /**< Time of last sending
                                                    packet                  */
    unsigned                 start_ka_count;   /**< The number of keep-alive
                                                    to be sent after it is
                                                    created                 */
    unsigned                 start_ka_interval;/**< The keepalive sending
                                                    interval after the stream
                                                    is created              */
    pj_timestamp             last_start_ka_tx; /**< Timestamp of the last
                                                    keepalive sent          */
#endif

#if TRACE_JB
    pj_oshandle_t            trace_jb_fd;   /**< Jitter tracing file handle.*/
    char                    *trace_jb_buf;  /**< Jitter tracing buffer.     */
#endif

    pjmedia_vid_codec       *codec;         /**< Codec instance being used. */
    pj_uint32_t              last_dec_ts;   /**< Last decoded timestamp.    */
    int                      last_dec_seq;  /**< Last decoded sequence.     */
    pj_uint32_t              rtp_tx_err_cnt;/**< The number of RTP
                                                 send() error               */
    pj_uint32_t              rtcp_tx_err_cnt;/**< The number of RTCP
                                                  send() error              */

    pj_timestamp             ts_freq;       /**< Timestamp frequency.       */

    pj_sockaddr              rem_rtp_addr;  /**< Remote RTP address         */
    unsigned                 rem_rtp_flag;  /**< Indicator flag about
                                                 packet from this addr.
                                                 0=no pkt, 1=good ssrc pkts,
                                                 2=bad ssrc pkts            */
    pj_sockaddr              rtp_src_addr;  /**< Actual packet src addr.    */
    unsigned                 rtp_src_cnt;   /**< How many pkt from this addr*/


    /* RTCP Feedback */
    pj_bool_t                send_rtcp_fb_nack;     /**< Send NACK?         */
    int                      pending_rtcp_fb_nack;  /**< Any pending NACK?  */
    int                      rtcp_fb_nack_cap_idx;  /**< RX NACK cap idx.   */
    pjmedia_rtcp_fb_nack     rtcp_fb_nack;          /**< TX NACK state.     */

    pj_bool_t                send_rtcp_fb_pli;      /**< Send PLI?          */
    int                      pending_rtcp_fb_pli;   /**< Any pending PLI?   */
    int                      rtcp_fb_pli_cap_idx;   /**< RX PLI cap idx.    */

#if TRACE_RC
    unsigned                 rc_total_sleep;
    unsigned                 rc_total_pkt;
    unsigned                 rc_total_img;
    pj_timestamp             tx_start;
    pj_timestamp             tx_end;
#endif
};

/* Prototypes */
static pj_status_t decode_frame(pjmedia_vid_stream *stream,
                                pjmedia_frame *frame);


static pj_status_t send_rtcp(pjmedia_vid_stream *stream,
                             pj_bool_t with_sdes,
                             pj_bool_t with_bye,
                             pj_bool_t with_fb_nack,
                             pj_bool_t with_fb_pli);

static void on_rx_rtcp( void *data,
                        void *pkt,
                        pj_ssize_t bytes_read);

static void on_destroy(void *arg);


#if TRACE_JB

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

PJ_INLINE(int) trace_jb_print_state(pjmedia_vid_stream *stream,
                                    char **buf, pj_ssize_t len)
{
    char *p = *buf;
    char *endp = *buf + len;
    pjmedia_jb_state state;

    pjmedia_jbuf_get_state(stream->jb, &state);

    len = pj_ansi_snprintf(p, endp-p, "%d, %d, %d",
                           state.size, state.burst, state.prefetch);
    if ((len < 0) || (len >= endp-p))
        return -1;

    p += len;
    *buf = p;
    return 0;
}

static void trace_jb_get(pjmedia_vid_stream *stream, pjmedia_jb_frame_type ft,
                         pj_size_t fsize)
{
    char *p = stream->trace_jb_buf;
    char *endp = stream->trace_jb_buf + PJ_LOG_MAX_SIZE;
    pj_ssize_t len = 0;
    const char* ft_st;

    if (!TRACE_JB_OPENED(stream))
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
    len = pj_ansi_snprintf(p, endp-p, "GET,%d,1,%s,,,,", fsize, ft_st);
    if ((len < 0) || (len >= endp-p))
        goto on_insuff_buffer;
    p += len;

    /* Print JB state */
    if (trace_jb_print_state(stream, &p, endp-p))
        goto on_insuff_buffer;

    /* Print end of line */
    if (endp-p < 2)
        goto on_insuff_buffer;
    *p++ = '\n';

    /* Write and flush */
    len = p - stream->trace_jb_buf;
    pj_file_write(stream->trace_jb_fd, stream->trace_jb_buf, &len);
    pj_file_flush(stream->trace_jb_fd);
    return;

on_insuff_buffer:
    pj_assert(!"Trace buffer too small, check PJ_LOG_MAX_SIZE!");
}

static void trace_jb_put(pjmedia_vid_stream *stream,
                         const pjmedia_rtp_hdr *hdr,
                         unsigned payloadlen, unsigned frame_cnt)
{
    char *p = stream->trace_jb_buf;
    char *endp = stream->trace_jb_buf + PJ_LOG_MAX_SIZE;
    pj_ssize_t len = 0;

    if (!TRACE_JB_OPENED(stream))
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
    if (trace_jb_print_state(stream, &p, endp-p))
        goto on_insuff_buffer;

    /* Print end of line */
    if (endp-p < 2)
        goto on_insuff_buffer;
    *p++ = '\n';

    /* Write and flush */
    len = p - stream->trace_jb_buf;
    pj_file_write(stream->trace_jb_fd, stream->trace_jb_buf, &len);
    pj_file_flush(stream->trace_jb_fd);
    return;

on_insuff_buffer:
    pj_assert(!"Trace buffer too small, check PJ_LOG_MAX_SIZE!");
}

#endif /* TRACE_JB */

static void dump_port_info(const pjmedia_vid_channel *chan,
                           const char *event_name)
{
    const pjmedia_port_info *pi = &chan->port.info;
    char fourcc_name[5];

    PJ_LOG(4, (pi->name.ptr,
               " %s format %s: %dx%d %s%s %d/%d(~%d)fps",
               (chan->dir==PJMEDIA_DIR_DECODING? "Decoding":"Encoding"),
               event_name,
               pi->fmt.det.vid.size.w, pi->fmt.det.vid.size.h,
               pjmedia_fourcc_name(pi->fmt.id, fourcc_name),
               (chan->dir==PJMEDIA_DIR_ENCODING?"->":"<-"),
               pi->fmt.det.vid.fps.num, pi->fmt.det.vid.fps.denum,
               pi->fmt.det.vid.fps.num/pi->fmt.det.vid.fps.denum));
}

/*
 * Handle events from stream components.
 */
static pj_status_t stream_event_cb(pjmedia_event *event,
                                   void *user_data)
{
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*)user_data;

    if (event->epub == stream->codec) {
        /* This is codec event */
        switch (event->type) {
        case PJMEDIA_EVENT_FMT_CHANGED:
            /* Copy the event to avoid deadlock if we publish the event
             * now. This happens because fmt_event may trigger restart
             * while we're still holding the stream lock.
             */
            pj_memcpy(&stream->fmt_event, event, sizeof(*event));
            return PJ_SUCCESS;

        case PJMEDIA_EVENT_KEYFRAME_MISSING:
            /* Republish this event later from get_frame(). */
            pj_memcpy(&stream->miss_keyframe_event, event, sizeof(*event));

            if (stream->send_rtcp_fb_pli) {
                /* Schedule sending RTCP-FB PLI to encoder, if configured,
                 * also perhaps better to make it redundant, in case the first
                 * packet is lost.
                 */
                stream->pending_rtcp_fb_pli = 2;
            }
            return PJ_SUCCESS;

        default:
            break;
        }
    } else if (event->epub == &stream->rtcp && 
               event->type==PJMEDIA_EVENT_RX_RTCP_FB)
    {
        /* This is RX RTCP-FB event */
        pjmedia_event_rx_rtcp_fb_data *data = 
                    (pjmedia_event_rx_rtcp_fb_data*)&event->data.rx_rtcp_fb;

        /* Check if configured to listen to the RTCP-FB type */
        if (data->cap.type == PJMEDIA_RTCP_FB_NACK) {
            if (data->cap.param.slen == 0 &&
                stream->rtcp_fb_nack_cap_idx >= 0)
            {
                /* Generic NACK */

                /* Update event data capability before republishing */
                data->cap = stream->info.loc_rtcp_fb.caps[
                                        stream->rtcp_fb_nack_cap_idx];
            }
            else if (pj_strcmp2(&data->cap.param, "pli") == 0 &&
                     stream->rtcp_fb_pli_cap_idx >= 0)
            {
                /* PLI */

                /* Tell encoder to generate keyframe */
                pjmedia_vid_stream_send_keyframe(stream);

                /* Update event data capability before republishing */
                data->cap = stream->info.loc_rtcp_fb.caps[
                                        stream->rtcp_fb_pli_cap_idx];

            }
        }
    }

    /* Republish events */
    return pjmedia_event_publish(NULL, stream, event,
                                 PJMEDIA_EVENT_PUBLISH_POST_EVENT);
}


/**
 * Publish transport error event.
 */
static void publish_tp_event(pjmedia_event_type event_type,
                             pj_status_t status,
                             pj_bool_t is_rtp,
                             pjmedia_dir dir,
                             pjmedia_vid_stream *stream)
{
    pjmedia_event ev;
    pj_timestamp ts_now;

    pj_get_timestamp(&ts_now);
    pj_bzero(&ev.data.med_tp_err, sizeof(ev.data.med_tp_err));

    /* Publish event. */
    pjmedia_event_init(&ev, event_type,
                       &ts_now, stream);
    ev.data.med_tp_err.type = PJMEDIA_TYPE_VIDEO;
    ev.data.med_tp_err.is_rtp = is_rtp;
    ev.data.med_tp_err.dir = dir;
    ev.data.med_tp_err.status = status;

    pjmedia_event_publish(NULL, stream, &ev, 0);
}

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA != 0
/*
 * Send keep-alive packet using non-codec frame.
 */
static void send_keep_alive_packet(pjmedia_vid_stream *stream)
{
#if PJMEDIA_STREAM_ENABLE_KA == PJMEDIA_STREAM_KA_EMPTY_RTP

    /* Keep-alive packet is empty RTP */
    pjmedia_vid_channel *channel = stream->enc;
    pj_status_t status;
    void *pkt;
    int pkt_len;

    if (!stream->transport)
        return;

    TRC_((channel->port.info.name.ptr,
          "Sending keep-alive (RTCP and empty RTP)"));

    /* Send RTP */
    status = pjmedia_rtp_encode_rtp( &stream->enc->rtp,
                                     stream->enc->pt, 0,
                                     1,
                                     0,
                                     (const void**)&pkt,
                                     &pkt_len);
    pj_assert(status == PJ_SUCCESS);

    pj_memcpy(stream->enc->buf, pkt, pkt_len);
    pjmedia_transport_send_rtp(stream->transport, stream->enc->buf,
                               pkt_len);

    /* Send RTCP */
    send_rtcp(stream, PJ_TRUE, PJ_FALSE, PJ_FALSE, PJ_FALSE);

    /* Update stats in case the stream is paused */
    stream->rtcp.stat.rtp_tx_last_seq = pj_ntohs(stream->enc->rtp.out_hdr.seq);

#elif PJMEDIA_STREAM_ENABLE_KA == PJMEDIA_STREAM_KA_USER

    /* Keep-alive packet is defined in PJMEDIA_STREAM_KA_USER_PKT */
    pjmedia_vid_channel *channel = stream->enc;
    int pkt_len;
    const pj_str_t str_ka = PJMEDIA_STREAM_KA_USER_PKT;

    TRC_((channel->port.info.name.ptr,
          "Sending keep-alive (custom RTP/RTCP packets)"));

    /* Send to RTP port */
    pj_memcpy(stream->enc->buf, str_ka.ptr, str_ka.slen);
    pkt_len = str_ka.slen;
    pjmedia_transport_send_rtp(stream->transport, stream->enc->buf,
                               pkt_len);

    /* Send to RTCP port */
    pjmedia_transport_send_rtcp(stream->transport, stream->enc->buf,
                                pkt_len);

#else

    PJ_UNUSED_ARG(stream);

#endif
}
#endif  /* defined(PJMEDIA_STREAM_ENABLE_KA) */


static pj_status_t send_rtcp(pjmedia_vid_stream *stream,
                             pj_bool_t with_sdes,
                             pj_bool_t with_bye,
                             pj_bool_t with_fb_nack,
                             pj_bool_t with_fb_pli)
{
    void *sr_rr_pkt;
    pj_uint8_t *pkt;
    int len, max_len;
    pj_status_t status;


    /* To avoid deadlock with media transport, we use the transport's
     * group lock.
     */
    if (stream->transport->grp_lock)
        pj_grp_lock_acquire( stream->transport->grp_lock );

    /* Build RTCP RR/SR packet */
    pjmedia_rtcp_build_rtcp(&stream->rtcp, &sr_rr_pkt, &len);

    if (with_sdes || with_bye || with_fb_nack || with_fb_pli) {
        pkt = (pj_uint8_t*) stream->out_rtcp_pkt;
        pj_memcpy(pkt, sr_rr_pkt, len);
        max_len = stream->out_rtcp_pkt_size;
    } else {
        pkt = (pj_uint8_t*)sr_rr_pkt;
        max_len = len;
    }

    /* Build RTCP SDES packet, forced if also send RTCP-FB */
    with_sdes = with_sdes || with_fb_pli || with_fb_nack;
    if (with_sdes) {
        pjmedia_rtcp_sdes sdes;
        pj_size_t sdes_len;

        pj_bzero(&sdes, sizeof(sdes));
        sdes.cname = stream->cname;
        sdes_len = max_len - len;
        status = pjmedia_rtcp_build_rtcp_sdes(&stream->rtcp, pkt+len,
                                              &sdes_len, &sdes);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(stream->name.ptr, status,
                                     "Error generating RTCP SDES"));
        } else {
            len += (int)sdes_len;
        }
    }

    /* Build RTCP BYE packet */
    if (with_bye) {
        pj_size_t bye_len = max_len - len;
        status = pjmedia_rtcp_build_rtcp_bye(&stream->rtcp, pkt+len,
                                             &bye_len, NULL);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(stream->name.ptr, status,
                                     "Error generating RTCP BYE"));
        } else {
            len += (int)bye_len;
        }
    }

    /* Build RTCP-FB generic NACK packet */
    if (with_fb_nack && stream->rtcp_fb_nack.pid >= 0) {
        pj_size_t fb_len = max_len - len;
        status = pjmedia_rtcp_fb_build_nack(&stream->rtcp, pkt+len, &fb_len,
                                            1, &stream->rtcp_fb_nack);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(stream->name.ptr, status,
                                     "Error generating RTCP-FB NACK"));
        } else {
            len += (int)fb_len;
        }
    }

    /* Build RTCP-FB PLI packet */
    if (with_fb_pli) {
        pj_size_t fb_len = max_len - len;
        status = pjmedia_rtcp_fb_build_pli(&stream->rtcp, pkt+len, &fb_len);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(stream->name.ptr, status,
                                     "Error generating RTCP-FB PLI"));
        } else {
            len += (int)fb_len;
            PJ_LOG(5,(stream->name.ptr, "Sending RTCP-FB PLI packet"));
        }
    }

    /* Send! */
    status = pjmedia_transport_send_rtcp(stream->transport, pkt, len);
    if (status != PJ_SUCCESS) {
        if (stream->rtcp_tx_err_cnt++ == 0) {
            LOGERR_((stream->name.ptr, status, "Error sending RTCP"));
        }
        if (stream->rtcp_tx_err_cnt > SEND_ERR_COUNT_TO_REPORT) {
            stream->rtcp_tx_err_cnt = 0;
        }
    }

    if (stream->transport->grp_lock)
        pj_grp_lock_release( stream->transport->grp_lock );

    return status;
}


/**
 * check_tx_rtcp()
 *
 * This function is can be called by either put_frame() or get_frame(),
 * to transmit periodic RTCP SR/RR report.
 * If 'fb_pli' is set to PJ_TRUE, this will send immediate RTCP-FB PLI.
 */
static void check_tx_rtcp(pjmedia_vid_stream *stream)
{
    pj_timestamp now;
    pj_bool_t early;

    /* Check if early RTCP mode is required (i.e: RTCP-FB) and allowed (i.e:
     * elapsed timestamp from previous RTCP-FB >= PJMEDIA_RTCP_FB_INTERVAL).
     */
    pj_get_timestamp(&now);
    early = ((stream->pending_rtcp_fb_pli || stream->pending_rtcp_fb_nack)
             &&
             (stream->rtcp_fb_last_tx.u64 == 0 ||
              pj_elapsed_msec(&stream->rtcp_fb_last_tx, &now) >=
                                            PJMEDIA_RTCP_FB_INTERVAL));

    /* First check, unless RTCP is 'urgent', just init rtcp_last_tx. */
    if (stream->rtcp_last_tx.u64 == 0 && !early) {
        pj_get_timestamp(&stream->rtcp_last_tx);
        return;
    } 

    /* Build & send RTCP */
    if (early ||
        pj_elapsed_msec(&stream->rtcp_last_tx, &now) >= stream->rtcp_interval)
    {
        pj_status_t status;

        status = send_rtcp(stream, !stream->rtcp_sdes_bye_disabled, PJ_FALSE,
                           stream->pending_rtcp_fb_nack,
                           stream->pending_rtcp_fb_pli);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(stream->name.ptr, status,
                         "Error sending RTCP"));
        }

        stream->rtcp_last_tx = now;

        if (early)
            stream->rtcp_fb_last_tx = now;

        if (stream->pending_rtcp_fb_pli)
            stream->pending_rtcp_fb_pli--;

        if (stream->pending_rtcp_fb_nack)
            stream->pending_rtcp_fb_nack--;
    }
}


#if 0
static void dump_bin(const char *buf, unsigned len)
{
    unsigned i;

    PJ_LOG(3,(THIS_FILE, "begin dump"));
    for (i=0; i<len; ++i) {
        int j;
        char bits[9];
        unsigned val = buf[i] & 0xFF;

        bits[8] = '\0';
        for (j=0; j<8; ++j) {
            if (val & (1 << (7-j)))
                bits[j] = '1';
            else
                bits[j] = '0';
        }

        PJ_LOG(3,(THIS_FILE, "%2d %s [%d]", i, bits, val));
    }
    PJ_LOG(3,(THIS_FILE, "end dump"));
}
#endif


/*
 * This callback is called by stream transport on receipt of packets
 * in the RTP socket.
 */
static void on_rx_rtp( pjmedia_tp_cb_param *param)
{
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*) param->user_data;
    void *pkt = param->pkt;
    pj_ssize_t bytes_read = param->size;
    pjmedia_vid_channel *channel = stream->dec;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;
    pjmedia_rtp_status seq_st;
    pj_status_t status;
    long ts_diff;
    pj_bool_t pkt_discarded = PJ_FALSE;

    /* Check for errors */
    if (bytes_read < 0) {
        status = (pj_status_t)-bytes_read;
        if (status == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {
            return;
        }

        LOGERR_((channel->port.info.name.ptr, status,
                 "Unable to receive RTP packet"));

        if (status == PJ_ESOCKETSTOP) {
            /* Publish receive error event. */
            publish_tp_event(PJMEDIA_EVENT_MEDIA_TP_ERR, status, PJ_TRUE,
                             PJMEDIA_DIR_DECODING, stream);
        }
        return;
    }

    /* Ignore keep-alive packets */
    if (bytes_read < (pj_ssize_t) sizeof(pjmedia_rtp_hdr))
        return;

    /* Update RTP and RTCP session. */
    status = pjmedia_rtp_decode_rtp(&channel->rtp, pkt, (int)bytes_read,
                                    &hdr, &payload, &payloadlen);
    if (status != PJ_SUCCESS) {
        LOGERR_((channel->port.info.name.ptr, status, "RTP decode error"));
        stream->rtcp.stat.rx.discard++;
        return;
    }

    /* Check if multiplexing is allowed and the payload indicates RTCP. */
    if (stream->info.rtcp_mux && hdr->pt >= 64 && hdr->pt <= 95) {
        on_rx_rtcp(stream, pkt, bytes_read);
        return;
    }

    /* Ignore the packet if decoder is paused */
    if (channel->paused)
        goto on_return;

    /* Update RTP session (also checks if RTP session can accept
     * the incoming packet.
     */
    pjmedia_rtp_session_update2(&channel->rtp, hdr, &seq_st,
                                PJMEDIA_VID_STREAM_CHECK_RTP_PT);
#if !PJMEDIA_VID_STREAM_CHECK_RTP_PT
    if (hdr->pt != channel->rtp.out_pt) {
        seq_st.status.flag.badpt = -1;
    }
#endif
    if (seq_st.status.value) {
        TRC_  ((channel->port.info.name.ptr,
                "RTP status: badpt=%d, badssrc=%d, dup=%d, "
                "outorder=%d, probation=%d, restart=%d",
                seq_st.status.flag.badpt,
                seq_st.status.flag.badssrc,
                seq_st.status.flag.dup,
                seq_st.status.flag.outorder,
                seq_st.status.flag.probation,
                seq_st.status.flag.restart));

        if (seq_st.status.flag.badpt) {
            PJ_LOG(4,(channel->port.info.name.ptr,
                      "Bad RTP pt %d (expecting %d)",
                      hdr->pt, channel->rtp.out_pt));
        }

        if (!stream->info.has_rem_ssrc && seq_st.status.flag.badssrc) {
            PJ_LOG(4,(channel->port.info.name.ptr,
                      "Changed RTP peer SSRC %d (previously %d)",
                      channel->rtp.peer_ssrc, stream->rtcp.peer_ssrc));
            stream->rtcp.peer_ssrc = channel->rtp.peer_ssrc;
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

    /* See if source address of RTP packet is different than the
     * configured address, and check if we need to tell the
     * media transport to switch RTP remote address.
     */
    if (param->src_addr) {
        pj_bool_t badssrc = (stream->info.has_rem_ssrc &&
                             seq_st.status.flag.badssrc);

        if (pj_sockaddr_cmp(&stream->rem_rtp_addr, param->src_addr) == 0) {
            /* We're still receiving from rem_rtp_addr. */
            stream->rtp_src_cnt = 0;
            stream->rem_rtp_flag = badssrc? 2: 1;
        } else {
            stream->rtp_src_cnt++;

            if (stream->rtp_src_cnt < PJMEDIA_RTP_NAT_PROBATION_CNT) {
                if (stream->rem_rtp_flag == 1 ||
                    (stream->rem_rtp_flag == 2 && badssrc))
                {
                    /* Only discard if:
                     * - we have ever received packet with good ssrc from
                     *   remote address (rem_rtp_addr), or
                     * - we have ever received packet with bad ssrc from
                     *   remote address and this packet also has bad ssrc.
                     */
                    pkt_discarded = PJ_TRUE;
                    goto on_return;
                }
                if (stream->info.has_rem_ssrc && !seq_st.status.flag.badssrc
                    && stream->rem_rtp_flag != 1)
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
                pj_sockaddr_cp(&stream->rem_rtp_addr, param->src_addr);

                /* Reset counter and flag */
                stream->rtp_src_cnt = 0;
                stream->rem_rtp_flag = badssrc? 2: 1;

                /* Update RTCP peer ssrc */
                stream->rtcp.peer_ssrc = pj_ntohl(hdr->ssrc);
            }
        }
    }

    pj_grp_lock_acquire( stream->grp_lock );

    /* Quickly see if there may be a full picture in the jitter buffer, and
     * decode them if so. More thorough check will be done in decode_frame().
     */
    ts_diff = pj_ntohl(hdr->ts) - stream->dec_frame.timestamp.u32.lo;
    if (ts_diff != 0 || hdr->m) {
        if (PJMEDIA_VID_STREAM_SKIP_PACKETS_TO_REDUCE_LATENCY) {
            /* Always decode whenever we have picture in jb and
             * overwrite already decoded picture if necessary
             */
            pj_size_t old_size = stream->dec_frame.size;

            stream->dec_frame.size = stream->dec_max_size;
            if (decode_frame(stream, &stream->dec_frame) != PJ_SUCCESS) {
                stream->dec_frame.size = old_size;
            }
        } else {
            /* Only decode if we don't already have decoded one,
             * unless the jb is full.
             */
            pj_bool_t can_decode = PJ_FALSE;

            if (pjmedia_jbuf_is_full(stream->jb)) {
                can_decode = PJ_TRUE;
            }
            else if (stream->dec_frame.size == 0) {
                can_decode = PJ_TRUE;
            }
            /* For video, checking for a full jbuf above is not very useful
             * since video jbuf has a rather large capacity (to accommodate
             * many chunks per frame) and thus can typically store frames
             * that are much longer than max latency/delay specified in jb_max
             * setting.
             * So we need to compare the last decoded frame's timestamp with
             * the current timestamp.
             */
            else if (ts_diff > (long)stream->dec_max_delay) {
                can_decode = PJ_TRUE;
            }

            if (can_decode) {
                stream->dec_frame.size = stream->dec_max_size;
                if (decode_frame(stream, &stream->dec_frame) != PJ_SUCCESS) {
                    stream->dec_frame.size = 0;
                }
            }
        }
    }

    /* Put "good" packet to jitter buffer, or reset the jitter buffer
     * when RTP session is restarted.
     */
    if (seq_st.status.flag.restart) {
        status = pjmedia_jbuf_reset(stream->jb);
        PJ_LOG(4,(channel->port.info.name.ptr, "Jitter buffer reset"));
    } else {
        /* Just put the payload into jitter buffer */
        pjmedia_jbuf_put_frame3(stream->jb, payload, payloadlen, 0,
                                pj_ntohs(hdr->seq), pj_ntohl(hdr->ts), NULL);

#if TRACE_JB
        trace_jb_put(stream, hdr, payloadlen, count);
#endif

    }
    pj_grp_lock_release( stream->grp_lock );

    /* Check if we need to send RTCP-FB generic NACK */
    if (stream->send_rtcp_fb_nack && seq_st.diff > 1 &&
        pj_ntohs(hdr->seq) >= seq_st.diff)
    {
        int i;
        pj_bzero(&stream->rtcp_fb_nack, sizeof(stream->rtcp_fb_nack));
        stream->rtcp_fb_nack.pid = pj_ntohs(hdr->seq) - seq_st.diff + 1;
        for (i = 0; i < (seq_st.diff - 1); ++i) {
            stream->rtcp_fb_nack.blp <<= 1;
            stream->rtcp_fb_nack.blp |= 1;
        }
        stream->pending_rtcp_fb_nack = 1;
    }

    /* Check if now is the time to transmit RTCP SR/RR report.
     * We only do this when stream direction is "decoding only" or
     * if the encoder is paused,
     * because otherwise check_tx_rtcp() will be handled by put_frame()
     */
    if (stream->dir == PJMEDIA_DIR_DECODING || stream->enc->paused) {
        check_tx_rtcp(stream);
    }

    if (status != 0) {
        LOGERR_((channel->port.info.name.ptr, status,
                 "Jitter buffer put() error"));
        pkt_discarded = PJ_TRUE;
        goto on_return;
    }

on_return:
    /* Update RTCP session */
    if (stream->rtcp.peer_ssrc == 0)
        stream->rtcp.peer_ssrc = channel->rtp.peer_ssrc;

    pjmedia_rtcp_rx_rtp2(&stream->rtcp, pj_ntohs(hdr->seq),
                         pj_ntohl(hdr->ts), payloadlen, pkt_discarded);

    /* Send RTCP RR and SDES after we receive some RTP packets */
    if (stream->rtcp.received >= 10 && !stream->initial_rr) {
        status = send_rtcp(stream, !stream->rtcp_sdes_bye_disabled,
                           PJ_FALSE, PJ_FALSE, PJ_FALSE);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(stream->name.ptr, status,
                     "Error sending initial RTCP RR"));
        } else {
            stream->initial_rr = PJ_TRUE;
        }
    }
}


/*
 * This callback is called by stream transport on receipt of packets
 * in the RTCP socket.
 */
static void on_rx_rtcp( void *data,
                        void *pkt,
                        pj_ssize_t bytes_read)
{
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*) data;
    pj_status_t status;

    /* Check for errors */
    if (bytes_read < 0) {
        status = (pj_status_t)-bytes_read;
        if (status == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {
            return;
        }
        LOGERR_((stream->cname.ptr, status, "Unable to receive RTCP packet"));
        if (status == PJ_ESOCKETSTOP) {
            /* Publish receive error event. */
            publish_tp_event(PJMEDIA_EVENT_MEDIA_TP_ERR, status, PJ_FALSE,
                             PJMEDIA_DIR_DECODING, stream);
        }
        return;
    }

    pjmedia_rtcp_rx_rtcp(&stream->rtcp, pkt, bytes_read);
}

static pj_status_t put_frame(pjmedia_port *port,
                             pjmedia_frame *frame)
{
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*) port->port_data.pdata;
    pjmedia_vid_channel *channel = stream->enc;
    pj_status_t status = 0;
    pjmedia_frame frame_out;
    unsigned rtp_ts_len;
    void *rtphdr;
    int rtphdrlen;
    pj_bool_t has_more_data = PJ_FALSE;
    pj_size_t total_sent = 0;
    pjmedia_vid_encode_opt enc_opt;
    unsigned pkt_cnt = 0;
    pj_timestamp initial_time;
    pj_timestamp now;
    pj_timestamp null_ts ={{0}};

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA != 0
    /* If the interval since last sending packet is greater than
     * PJMEDIA_STREAM_KA_INTERVAL, send keep-alive packet.
     */
    if (stream->use_ka)
    {
        pj_uint32_t dtx_duration, ka_interval;
        pj_time_val tm_now, tmp;

        pj_gettimeofday(&tm_now);

        tmp = tm_now;
        PJ_TIME_VAL_SUB(tmp, stream->last_frm_ts_sent);
        dtx_duration = PJ_TIME_VAL_MSEC(tmp);

        if (stream->start_ka_count) {
            ka_interval = stream->start_ka_interval;
        }  else {
            ka_interval = stream->ka_interval * 1000;
        }
        if (dtx_duration > ka_interval) {
            send_keep_alive_packet(stream);
            stream->last_frm_ts_sent = tm_now;

            if (stream->start_ka_count)
                stream->start_ka_count--;
        }
    }
#endif
    /* Get frame length in timestamp unit */
    rtp_ts_len = stream->frame_ts_len;

    /* Don't do anything if stream is paused, except updating RTP timestamp */
    if (channel->paused) {
        /* Update RTP session's timestamp. */
        status = pjmedia_rtp_encode_rtp( &channel->rtp, 0, 0, 0, rtp_ts_len,
                                         NULL, NULL);

        /* Update RTCP stats with last RTP timestamp. */
        stream->rtcp.stat.rtp_tx_last_ts =
                                        pj_ntohl(channel->rtp.out_hdr.ts);
        return PJ_SUCCESS;
    }

    /* Empty video frame? Just update RTP timestamp for now */
    if (frame->type==PJMEDIA_FRAME_TYPE_VIDEO && frame->size==0) {
        pjmedia_rtp_encode_rtp(&channel->rtp, channel->pt, 1, 0,
                               rtp_ts_len,  (const void**)&rtphdr,
                               &rtphdrlen);
        return PJ_SUCCESS;
    }

    /* Init frame_out buffer. */
    pj_bzero(&frame_out, sizeof(frame_out));
    frame_out.buf = ((char*)channel->buf) + sizeof(pjmedia_rtp_hdr);

    /* Check if need to send keyframe. */
    pj_get_timestamp(&now);
    if (stream->num_keyframe &&
        (pj_cmp_timestamp(&null_ts, &stream->last_keyframe_tx) != 0))
    {
        unsigned elapse_time;

        elapse_time = pj_elapsed_msec(&stream->last_keyframe_tx, &now);
        if (elapse_time > stream->info.sk_cfg.interval)
        {
            stream->force_keyframe = PJ_TRUE;
            --stream->num_keyframe;
        }
    }

    /* Init encoding option */
    pj_bzero(&enc_opt, sizeof(enc_opt));
    if (stream->force_keyframe &&
        pj_elapsed_msec(&stream->last_keyframe_tx, &now) >=
                        PJMEDIA_VID_STREAM_MIN_KEYFRAME_INTERVAL_MSEC)
    {
        /* Force encoder to generate keyframe */
        enc_opt.force_keyframe = PJ_TRUE;
        stream->force_keyframe = PJ_FALSE;
        TRC_((channel->port.info.name.ptr,
              "Forcing encoder to generate keyframe"));
    }

    /* Encode! */
    status = pjmedia_vid_codec_encode_begin(stream->codec, &enc_opt, frame,
                                            channel->buf_size -
                                               sizeof(pjmedia_rtp_hdr),
                                            &frame_out,
                                            &has_more_data);
    if (status != PJ_SUCCESS) {
        LOGERR_((channel->port.info.name.ptr, status,
                "Codec encode_begin() error"));

        /* Update RTP timestamp */
        pjmedia_rtp_encode_rtp(&channel->rtp, channel->pt, 1, 0,
                               rtp_ts_len,  (const void**)&rtphdr,
                               &rtphdrlen);
        return status;
    }

    pj_get_timestamp(&initial_time);

    if ((frame_out.bit_info & PJMEDIA_VID_FRM_KEYFRAME)
                                                  == PJMEDIA_VID_FRM_KEYFRAME)
    {
        stream->last_keyframe_tx = initial_time;
        TRC_((channel->port.info.name.ptr, "Keyframe generated"));
    }

    /* Loop while we have frame to send */
    for (;;) {
        status = pjmedia_rtp_encode_rtp(&channel->rtp,
                                        channel->pt,
                                        (has_more_data == PJ_FALSE ? 1 : 0),
                                        (int)frame_out.size,
                                        rtp_ts_len,
                                        (const void**)&rtphdr,
                                        &rtphdrlen);
        if (status != PJ_SUCCESS) {
            LOGERR_((channel->port.info.name.ptr, status,
                    "RTP encode_rtp() error"));
            return status;
        }

        /* When the payload length is zero, we should not send anything,
         * but proceed the rest normally.
         */
        if (frame_out.size != 0 && stream->transport) {
            /* Copy RTP header to the beginning of packet */
            pj_memcpy(channel->buf, rtphdr, sizeof(pjmedia_rtp_hdr));

            /* Send the RTP packet to the transport. */
            status = pjmedia_transport_send_rtp(stream->transport,
                                                (char*)channel->buf,
                                                frame_out.size +
                                                    sizeof(pjmedia_rtp_hdr));
            if (status != PJ_SUCCESS) {
                if (stream->rtp_tx_err_cnt++ == 0) {
                    LOGERR_((channel->port.info.name.ptr, status,
                             "Error sending RTP"));
                }
                if (stream->rtp_tx_err_cnt > SEND_ERR_COUNT_TO_REPORT) {
                    stream->rtp_tx_err_cnt = 0;
                }
            }
            pjmedia_rtcp_tx_rtp(&stream->rtcp, (unsigned)frame_out.size);
            total_sent += frame_out.size;
            pkt_cnt++;
        }

        if (!has_more_data)
            break;

        /* Next packets use same timestamp */
        rtp_ts_len = 0;

        frame_out.size = 0;

        /* Encode more! */
        status = pjmedia_vid_codec_encode_more(stream->codec,
                                               channel->buf_size -
                                                   sizeof(pjmedia_rtp_hdr),
                                               &frame_out,
                                               &has_more_data);
        if (status != PJ_SUCCESS) {
            LOGERR_((channel->port.info.name.ptr, status,
                     "Codec encode_more() error"));
            /* Ignore this error (?) */
            break;
        }

        /* Send rate control */
        if (stream->info.rc_cfg.method==PJMEDIA_VID_STREAM_RC_SIMPLE_BLOCKING)
        {
            pj_timestamp next_send_ts, total_send_ts;

            total_send_ts.u64 = total_sent * stream->ts_freq.u64 * 8 /
                                stream->info.rc_cfg.bandwidth;
            next_send_ts = initial_time;
            pj_add_timestamp(&next_send_ts, &total_send_ts);

            pj_get_timestamp(&now);
            if (pj_cmp_timestamp(&now, &next_send_ts) < 0) {
                unsigned ms_sleep;
                ms_sleep = pj_elapsed_msec(&now, &next_send_ts);

                if (ms_sleep > 10)
                    ms_sleep = 10;

                pj_thread_sleep(ms_sleep);
            }
        }
    }

#if TRACE_RC
    /* Trace log for rate control */
    {
        pj_timestamp end_time;
        unsigned total_sleep;

        pj_get_timestamp(&end_time);
        total_sleep = pj_elapsed_msec(&initial_time, &end_time);
        PJ_LOG(5, (stream->name.ptr, "total pkt=%d size=%d sleep=%d",
                   pkt_cnt, total_sent, total_sleep));

        if (stream->tx_start.u64 == 0)
            stream->tx_start = initial_time;
        stream->tx_end = end_time;
        stream->rc_total_pkt += pkt_cnt;
        stream->rc_total_sleep += total_sleep;
        stream->rc_total_img++;
    }
#endif

    /* Check if now is the time to transmit RTCP SR/RR report.
     * We only do this when stream direction is not "decoding only", because
     * when it is, check_tx_rtcp() will be handled by get_frame().
     */
    if (stream->dir != PJMEDIA_DIR_DECODING && stream->transport) {
        check_tx_rtcp(stream);
    }

    /* Do nothing if we have nothing to transmit */
    if (total_sent == 0) {
        return PJ_SUCCESS;
    }

    /* Update stat */
    if (pkt_cnt) {
        stream->rtcp.stat.rtp_tx_last_ts =
                pj_ntohl(stream->enc->rtp.out_hdr.ts);
        stream->rtcp.stat.rtp_tx_last_seq =
                pj_ntohs(stream->enc->rtp.out_hdr.seq);
    }

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    /* Update time of last sending packet. */
    pj_gettimeofday(&stream->last_frm_ts_sent);
#endif

    return PJ_SUCCESS;
}

/* Decode one image from jitter buffer */
static pj_status_t decode_frame(pjmedia_vid_stream *stream,
                                pjmedia_frame *frame)
{
    pjmedia_vid_channel *channel = stream->dec;
    pj_uint32_t last_ts = 0, frm_ts = 0;
    pj_bool_t last_ts_inited = PJ_FALSE;
    int frm_first_seq = 0, frm_last_seq = 0;
    pj_bool_t got_frame = PJ_FALSE;
    unsigned cnt, frm_pkt_cnt = 0, frm_cnt = 0;
    pj_status_t status;

    /* Repeat get payload from the jitter buffer until all payloads with same
     * timestamp are collected.
     */

    /* Check if we got a decodable frame */
    for (cnt=0; ; ) {
        char ptype;
        pj_uint32_t ts;
        int seq;

        /* Peek frame from jitter buffer. */
        pjmedia_jbuf_peek_frame(stream->jb, cnt, NULL, NULL,
                                &ptype, NULL, &ts, &seq);
        if (ptype == PJMEDIA_JB_NORMAL_FRAME) {
            if (stream->last_dec_ts == ts) {
                /* Remove any late packet (the frame has been decoded) */
                pjmedia_jbuf_remove_frame(stream->jb, 1);
                continue;
            }

            if (!last_ts_inited) {
                last_ts = ts;

                /* Init timestamp and first seq of the first frame */
                frm_ts = ts;
                frm_first_seq = seq;
                last_ts_inited = PJ_TRUE;
            }
            if (ts != last_ts) {
                last_ts = ts;
                if (frm_pkt_cnt == 0)
                    frm_pkt_cnt = cnt;

                /* Is it time to decode? Check with minimum delay setting */
                if (++frm_cnt == stream->dec_delay_cnt) {
                    got_frame = PJ_TRUE;
                    break;
                }
            }
        } else if (ptype == PJMEDIA_JB_ZERO_EMPTY_FRAME) {
            /* No more packet in the jitter buffer */
            break;
        }

        ++cnt;
    }

    if (got_frame) {
        unsigned i;

        /* Exclude any MISSING frames in the end of the packets array, as
         * it may be part of the next video frame (late packets).
         */
        for (; frm_pkt_cnt > 1; --frm_pkt_cnt) {
            char ptype;
            pjmedia_jbuf_peek_frame(stream->jb, frm_pkt_cnt, NULL, NULL, &ptype,
                                    NULL, NULL, NULL);
            if (ptype == PJMEDIA_JB_NORMAL_FRAME)
                break;
        }

        /* Check if the packet count for this frame exceeds the limit */
        if (frm_pkt_cnt > stream->rx_frame_cnt) {
            PJ_LOG(1,(channel->port.info.name.ptr,
                      "Discarding %u frames because array is full!",
                      frm_pkt_cnt - stream->rx_frame_cnt));
            pjmedia_jbuf_remove_frame(stream->jb,
                                      frm_pkt_cnt - stream->rx_frame_cnt);
            frm_pkt_cnt = stream->rx_frame_cnt;
        }

        /* Generate frame bitstream from the payload */
        for (i = 0; i < frm_pkt_cnt; ++i) {
            char ptype;

            stream->rx_frames[i].type = PJMEDIA_FRAME_TYPE_VIDEO;
            stream->rx_frames[i].timestamp.u64 = frm_ts;
            stream->rx_frames[i].bit_info = 0;

            /* We use jbuf_peek_frame() as it will returns the pointer of
             * the payload (no buffer and memcpy needed), just as we need.
             */
            pjmedia_jbuf_peek_frame(stream->jb, i,
                                    (const void**)&stream->rx_frames[i].buf,
                                    &stream->rx_frames[i].size, &ptype,
                                    NULL, NULL, &frm_last_seq);

            if (ptype != PJMEDIA_JB_NORMAL_FRAME) {
                /* Packet lost, must set payload to NULL and keep going */
                stream->rx_frames[i].buf = NULL;
                stream->rx_frames[i].size = 0;
                stream->rx_frames[i].type = PJMEDIA_FRAME_TYPE_NONE;
                continue;
            }
        }

        /* Decode */
        status = pjmedia_vid_codec_decode(stream->codec, frm_pkt_cnt,
                                          stream->rx_frames,
                                          (unsigned)frame->size, frame);
        if (status != PJ_SUCCESS) {
            LOGERR_((channel->port.info.name.ptr, status,
                     "codec decode() error"));
            frame->type = PJMEDIA_FRAME_TYPE_NONE;
            frame->size = 0;
        }

        pjmedia_jbuf_remove_frame(stream->jb, frm_pkt_cnt);
    }

    /* Learn remote frame rate after successful decoding */
    if (got_frame && frame->type == PJMEDIA_FRAME_TYPE_VIDEO && frame->size)
    {
        /* Only check remote frame rate when timestamp is not wrapping and
         * sequence is increased by 1.
         */
        if (frm_ts > stream->last_dec_ts &&
            frm_first_seq - stream->last_dec_seq == 1)
        {
            pj_uint32_t ts_diff;
            pjmedia_ratio new_fps;

            ts_diff = frm_ts - stream->last_dec_ts;

            /* Calculate new FPS based on RTP timestamp diff */
            if (stream->info.codec_info.clock_rate % ts_diff == 0) {
                new_fps.num = stream->info.codec_info.clock_rate/ts_diff;
                new_fps.denum = 1;
            } else {
                new_fps.num = stream->info.codec_info.clock_rate;
                new_fps.denum = ts_diff;
            }

            /* Only apply the new FPS when it is >0, <=100, and increasing */
            if (new_fps.num/new_fps.denum <= 100 &&
                new_fps.num/new_fps.denum > 0 &&
                new_fps.num*1.0/new_fps.denum >
                stream->dec_max_fps.num*1.0/stream->dec_max_fps.denum)
            {
                pjmedia_video_format_detail *vfd;
                vfd = pjmedia_format_get_video_format_detail(
                                        &channel->port.info.fmt, PJ_TRUE);

                /* Update FPS in channel & stream info */
                vfd->fps = new_fps;
                stream->info.codec_param->dec_fmt.det.vid.fps = new_fps;

                /* Update the decoding delay */
                {
                    pjmedia_jb_state jb_state;
                    pjmedia_jbuf_get_state(stream->jb, &jb_state);

                    stream->dec_delay_cnt =
                                    ((PJMEDIA_VID_STREAM_DECODE_MIN_DELAY_MSEC *
                                      vfd->fps.num) +
                                     (1000 * vfd->fps.denum) - 1) /
                                    (1000 * vfd->fps.denum);
                    if (stream->dec_delay_cnt < 1)
                        stream->dec_delay_cnt = 1;
                    if (stream->dec_delay_cnt > jb_state.max_count * 4/5)
                        stream->dec_delay_cnt = jb_state.max_count * 4/5;
                }

                /* Publish PJMEDIA_EVENT_FMT_CHANGED event */
                {
                    pjmedia_event *event = &stream->fmt_event;

                    /* Update max fps of decoding dir */
                    stream->dec_max_fps = vfd->fps;

                    /* Use the buffered format changed event:
                     * - just update the framerate if there is pending event,
                     * - otherwise, init the whole event.
                     */
                    if (stream->fmt_event.type != PJMEDIA_EVENT_NONE) {
                        event->data.fmt_changed.new_fmt.det.vid.fps = vfd->fps;
                    } else {
                        pjmedia_event_init(event, PJMEDIA_EVENT_FMT_CHANGED,
                                           &frame->timestamp, &channel->port);
                        event->data.fmt_changed.dir = PJMEDIA_DIR_DECODING;
                        pj_memcpy(&event->data.fmt_changed.new_fmt,
                                  &stream->info.codec_param->dec_fmt,
                                  sizeof(pjmedia_format));
                    }
                }
            }
        }

        /* Update last frame seq and timestamp */
        stream->last_dec_seq = frm_last_seq;
        stream->last_dec_ts = frm_ts;
    }

    return got_frame ? PJ_SUCCESS : PJ_ENOTFOUND;
}


static pj_status_t get_frame(pjmedia_port *port,
                             pjmedia_frame *frame)
{
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*) port->port_data.pdata;
    pjmedia_vid_channel *channel = stream->dec;

    /* Return no frame is channel is paused */
    if (channel->paused) {
        frame->type = PJMEDIA_FRAME_TYPE_NONE;
        frame->size = 0;
        return PJ_SUCCESS;
    }

    /* Report pending events. Do not publish the event while holding the
     * stream lock as that would lead to deadlock. It should be safe to
     * operate on fmt_event without the mutex because format change normally
     * would only occur once during the start of the media.
     */
    if (stream->fmt_event.type != PJMEDIA_EVENT_NONE) {
        pjmedia_event_fmt_changed_data *fmt_chg_data;

        fmt_chg_data = &stream->fmt_event.data.fmt_changed;

        /* Update stream info and decoding channel port info */
        if (fmt_chg_data->dir == PJMEDIA_DIR_DECODING) {
            pjmedia_format_copy(&stream->info.codec_param->dec_fmt,
                                &fmt_chg_data->new_fmt);
            pjmedia_format_copy(&stream->dec->port.info.fmt,
                                &fmt_chg_data->new_fmt);

            /* Override the framerate to be 1.5x higher in the event
             * for the renderer.
             */
            fmt_chg_data->new_fmt.det.vid.fps.num *= 3;
            fmt_chg_data->new_fmt.det.vid.fps.num /= 2;
        } else {
            pjmedia_format_copy(&stream->info.codec_param->enc_fmt,
                                &fmt_chg_data->new_fmt);
            pjmedia_format_copy(&stream->enc->port.info.fmt,
                                &fmt_chg_data->new_fmt);
        }

        dump_port_info(fmt_chg_data->dir==PJMEDIA_DIR_DECODING ?
                        stream->dec : stream->enc,
                       "changed");

        pjmedia_event_publish(NULL, port, &stream->fmt_event,
                              PJMEDIA_EVENT_PUBLISH_POST_EVENT);

        stream->fmt_event.type = PJMEDIA_EVENT_NONE;
    }

    if (stream->miss_keyframe_event.type != PJMEDIA_EVENT_NONE) {
        pjmedia_event_publish(NULL, port, &stream->miss_keyframe_event,
                              PJMEDIA_EVENT_PUBLISH_POST_EVENT);
        stream->miss_keyframe_event.type = PJMEDIA_EVENT_NONE;
    }

    pj_grp_lock_acquire( stream->grp_lock );

    if (stream->dec_frame.size == 0) {
        /* Don't have frame in buffer, try to decode one */
        if (decode_frame(stream, frame) != PJ_SUCCESS) {
            frame->type = PJMEDIA_FRAME_TYPE_NONE;
            frame->size = 0;
        }
    } else {
        if (frame->size < stream->dec_frame.size) {
            PJ_LOG(4,(stream->dec->port.info.name.ptr,
                      "Error: not enough buffer for decoded frame "
                      "(supplied=%d, required=%d)",
                      (int)frame->size, (int)stream->dec_frame.size));
            frame->type = PJMEDIA_FRAME_TYPE_NONE;
            frame->size = 0;
        } else {
            frame->type = stream->dec_frame.type;
            frame->timestamp = stream->dec_frame.timestamp;
            frame->size = stream->dec_frame.size;
            pj_memcpy(frame->buf, stream->dec_frame.buf, frame->size);
        }

        stream->dec_frame.size = 0;
    }

    pj_grp_lock_release( stream->grp_lock );

    return PJ_SUCCESS;
}

/*
 * Create media channel.
 */
static pj_status_t create_channel( pj_pool_t *pool,
                                   pjmedia_vid_stream *stream,
                                   pjmedia_dir dir,
                                   unsigned pt,
                                   const pjmedia_vid_stream_info *info,
                                   pjmedia_vid_channel **p_channel)
{
    enum { M = 32 };
    pjmedia_vid_channel *channel;
    pj_status_t status;
    unsigned min_out_pkt_size;
    pj_str_t name;
    const char *type_name;
    pjmedia_format *fmt;
    char fourcc_name[5];
    pjmedia_port_info *pi;

    pj_assert(info->type == PJMEDIA_TYPE_VIDEO);
    pj_assert(dir == PJMEDIA_DIR_DECODING || dir == PJMEDIA_DIR_ENCODING);

    /* Allocate memory for channel descriptor */
    channel = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_channel);
    PJ_ASSERT_RETURN(channel != NULL, PJ_ENOMEM);

    /* Init vars */
    if (dir==PJMEDIA_DIR_DECODING) {
        type_name = "vstdec";
        fmt = &info->codec_param->dec_fmt;
    } else {
        type_name = "vstenc";
        fmt = &info->codec_param->enc_fmt;
    }
    name.ptr = (char*) pj_pool_alloc(pool, M);
    name.slen = pj_ansi_snprintf(name.ptr, M, "%s%p", type_name, stream);
    pi = &channel->port.info;

    /* Init channel info. */
    channel->stream = stream;
    channel->dir = dir;
    channel->paused = 1;
    channel->pt = pt;

    /* Allocate buffer for outgoing packet. */
    if (dir == PJMEDIA_DIR_ENCODING) {
        channel->buf_size = sizeof(pjmedia_rtp_hdr) + stream->frame_size;

        /* It should big enough to hold (minimally) RTCP SR with an SDES. */
        min_out_pkt_size =  sizeof(pjmedia_rtcp_sr_pkt) +
                            sizeof(pjmedia_rtcp_common) +
                            (4 + (unsigned)stream->cname.slen) +
                            32;

        if (channel->buf_size < min_out_pkt_size)
            channel->buf_size = min_out_pkt_size;

        channel->buf = pj_pool_alloc(pool, channel->buf_size);
        PJ_ASSERT_RETURN(channel->buf != NULL, PJ_ENOMEM);
    }

    /* Create RTP and RTCP sessions: */
    {
        pjmedia_rtp_session_setting settings;

        settings.flags = (pj_uint8_t)((info->rtp_seq_ts_set << 2) |
                                      (info->has_rem_ssrc << 4) | 3);
        settings.default_pt = pt;
        settings.sender_ssrc = info->ssrc;
        settings.peer_ssrc = info->rem_ssrc;
        settings.seq = info->rtp_seq;
        settings.ts = info->rtp_ts;
        status = pjmedia_rtp_session_init2(&channel->rtp, settings);
    }
    if (status != PJ_SUCCESS)
        return status;

    /* Init port. */
    pjmedia_port_info_init2(pi, &name, SIGNATURE, dir, fmt);
    if (dir == PJMEDIA_DIR_DECODING) {
        channel->port.get_frame = &get_frame;
    } else {
        pi->fmt.id = info->codec_param->dec_fmt.id;
        channel->port.put_frame = &put_frame;
    }
    channel->port.port_data.pdata = stream;

    /* Use stream group lock */
    channel->port.grp_lock = stream->grp_lock;

    PJ_LOG(5, (name.ptr,
               "%s channel created %dx%d %s%s%.*s %d/%d(~%d)fps",
               (dir==PJMEDIA_DIR_ENCODING?"Encoding":"Decoding"),
               pi->fmt.det.vid.size.w, pi->fmt.det.vid.size.h,
               pjmedia_fourcc_name(pi->fmt.id, fourcc_name),
               (dir==PJMEDIA_DIR_ENCODING?"->":"<-"),
               (int)info->codec_info.encoding_name.slen,
               info->codec_info.encoding_name.ptr,
               pi->fmt.det.vid.fps.num, pi->fmt.det.vid.fps.denum,
               pi->fmt.det.vid.fps.num/pi->fmt.det.vid.fps.denum));

    /* Done. */
    *p_channel = channel;
    return PJ_SUCCESS;
}


/*
 * Create stream.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_create(
                                        pjmedia_endpt *endpt,
                                        pj_pool_t *pool,
                                        pjmedia_vid_stream_info *info,
                                        pjmedia_transport *tp,
                                        void *user_data,
                                        pjmedia_vid_stream **p_stream)
{
    enum { M = 32 };
    pj_pool_t *own_pool = NULL;
    pjmedia_vid_stream *stream;
    unsigned jb_init, jb_max, jb_min_pre, jb_max_pre;
    int frm_ptime, chunks_per_frm;
    pjmedia_video_format_detail *vfd_enc, *vfd_dec;
    char *p;
    pj_status_t status;
    pjmedia_transport_attach_param att_param;

    if (!pool) {
        own_pool = pjmedia_endpt_create_pool( endpt, "vstrm%p",
                                              PJMEDIA_VSTREAM_SIZE,
                                              PJMEDIA_VSTREAM_INC);
        PJ_ASSERT_RETURN(own_pool != NULL, PJ_ENOMEM);
        pool = own_pool;
    }

    /* Allocate stream */
    stream = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_stream);
    PJ_ASSERT_RETURN(stream != NULL, PJ_ENOMEM);
    stream->own_pool = own_pool;

    /* Get codec manager */
    stream->codec_mgr = pjmedia_vid_codec_mgr_instance();
    PJ_ASSERT_RETURN(stream->codec_mgr, PJMEDIA_CODEC_EFAILED);

    /* Init stream/port name */
    stream->name.ptr = (char*) pj_pool_alloc(pool, M);
    stream->name.slen = pj_ansi_snprintf(stream->name.ptr, M,
                                         "vstrm%p", stream);

    /* Create and initialize codec: */
    status = pjmedia_vid_codec_mgr_alloc_codec(stream->codec_mgr,
                                               &info->codec_info,
                                               &stream->codec);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Get codec param: */
    if (!info->codec_param) {
        pjmedia_vid_codec_param def_param;

        status = pjmedia_vid_codec_mgr_get_default_param(stream->codec_mgr,
                                                         &info->codec_info,
                                                         &def_param);
        if (status != PJ_SUCCESS)
            goto err_cleanup;

        info->codec_param = pjmedia_vid_codec_param_clone(pool, &def_param);
        pj_assert(info->codec_param);
    }

    /* Init codec param and adjust MTU */
    info->codec_param->dir = info->dir;
    info->codec_param->enc_mtu -= (sizeof(pjmedia_rtp_hdr) +
                                   PJMEDIA_STREAM_RESV_PAYLOAD_LEN);
    if (info->codec_param->enc_mtu > PJMEDIA_MAX_MTU)
        info->codec_param->enc_mtu = PJMEDIA_MAX_MTU;

    /* Packet size estimation for decoding direction */
    vfd_enc = pjmedia_format_get_video_format_detail(
                                        &info->codec_param->enc_fmt, PJ_TRUE);
    vfd_dec = pjmedia_format_get_video_format_detail(
                                        &info->codec_param->dec_fmt, PJ_TRUE);

    /* Init stream: */
    stream->endpt = endpt;
    stream->dir = info->dir;
    stream->user_data = user_data;
    stream->rtcp_interval = PJMEDIA_RTCP_INTERVAL + pj_rand()%1000 - 500;
    stream->rtcp_sdes_bye_disabled = info->rtcp_sdes_bye_disabled;

    stream->jb_last_frm = PJMEDIA_JB_NORMAL_FRAME;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    stream->use_ka = info->use_ka;
    stream->ka_interval = info->ka_cfg.ka_interval;
    stream->start_ka_count = info->ka_cfg.start_count;
    stream->start_ka_interval = info->ka_cfg.start_interval;    
#endif
    stream->num_keyframe = info->sk_cfg.count;

    stream->cname = info->cname;
    if (stream->cname.slen == 0) {
        /* Build random RTCP CNAME. CNAME has user@host format */
        stream->cname.ptr = p = (char*) pj_pool_alloc(pool, 20);
        pj_create_random_string(p, 5);
        p += 5;
        *p++ = '@'; *p++ = 'p'; *p++ = 'j';
        pj_create_random_string(p, 6);
        p += 6;
        *p++ = '.'; *p++ = 'o'; *p++ = 'r'; *p++ = 'g';
        stream->cname.slen = p - stream->cname.ptr;
    }

    /* Create group lock */
    status = pj_grp_lock_create_w_handler(pool, NULL, stream, 
                                          &on_destroy,
                                          &stream->grp_lock);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Add ref count of group lock */
    status = pj_grp_lock_add_ref(stream->grp_lock);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Init and open the codec. */
    status = pjmedia_vid_codec_init(stream->codec, pool);
    if (status != PJ_SUCCESS)
        goto err_cleanup;
    status = pjmedia_vid_codec_open(stream->codec, info->codec_param);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Subscribe to codec events */
    pjmedia_event_subscribe(NULL, &stream_event_cb, stream,
                            stream->codec);

    /* Estimate the maximum frame size */
    stream->frame_size = vfd_enc->size.w * vfd_enc->size.h * 4;

#if 0
    stream->frame_size = vfd_enc->max_bps/8 * vfd_enc->fps.denum /
                         vfd_enc->fps.num;

    /* As the maximum frame_size is not represented directly by maximum bps
     * (which includes intra and predicted frames), let's increase the
     * frame size value for safety.
     */
    stream->frame_size <<= 4;
#endif

    /* Validate the frame size */
    if (stream->frame_size == 0 ||
        stream->frame_size > PJMEDIA_MAX_VIDEO_ENC_FRAME_SIZE)
    {
        stream->frame_size = PJMEDIA_MAX_VIDEO_ENC_FRAME_SIZE;
    }

    /* Get frame length in timestamp unit */
    stream->frame_ts_len = info->codec_info.clock_rate *
                           vfd_enc->fps.denum / vfd_enc->fps.num;

    /* Initialize send rate states */
    pj_get_timestamp_freq(&stream->ts_freq);
    if (info->rc_cfg.bandwidth == 0)
        info->rc_cfg.bandwidth = vfd_enc->max_bps;

    /* For simple blocking, need to have bandwidth large enough, otherwise
     * we can slow down the transmission too much
     */
    if (info->rc_cfg.method==PJMEDIA_VID_STREAM_RC_SIMPLE_BLOCKING &&
        info->rc_cfg.bandwidth < vfd_enc->avg_bps * 3)
    {
        info->rc_cfg.bandwidth = vfd_enc->avg_bps * 3;
    }

    /* Override the initial framerate in the decoding direction. This initial
     * value will be used by the renderer to configure its clock, and setting
     * it to a bit higher value can avoid the possibility of high latency
     * caused by clock drift (remote encoder clock runs slightly faster than
     * local renderer clock) or video setup lag. Note that the actual framerate
     * will be continuously calculated based on the incoming RTP timestamps.
     */
    vfd_dec->fps.num = vfd_dec->fps.num * 3 / 2;
    stream->dec_max_fps = vfd_dec->fps;

    /* Create decoder channel */
    status = create_channel( pool, stream, PJMEDIA_DIR_DECODING,
                             info->rx_pt, info, &stream->dec);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Create encoder channel */
    status = create_channel( pool, stream, PJMEDIA_DIR_ENCODING,
                             info->tx_pt, info, &stream->enc);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    /* Create temporary buffer for immediate decoding */
    stream->dec_max_size = vfd_dec->size.w * vfd_dec->size.h * 4;
    stream->dec_frame.buf = pj_pool_alloc(pool, stream->dec_max_size);

    /* Init jitter buffer parameters: */
    frm_ptime       = 1000 * vfd_dec->fps.denum / vfd_dec->fps.num;
    chunks_per_frm  = stream->frame_size / PJMEDIA_MAX_MRU;
    if (chunks_per_frm < MIN_CHUNKS_PER_FRM)
        chunks_per_frm = MIN_CHUNKS_PER_FRM;

    /* JB max count, default 500ms */
    if (info->jb_max >= frm_ptime) {
        jb_max      = info->jb_max * chunks_per_frm / frm_ptime;
        stream->dec_max_delay = info->codec_info.clock_rate * info->jb_max /
                                1000;
    } else {
        jb_max      = 500 * chunks_per_frm / frm_ptime;
        stream->dec_max_delay = info->codec_info.clock_rate * 500 / 1000;
    }

    /* JB min prefetch, default 1 frame */
    if (info->jb_min_pre >= frm_ptime)
        jb_min_pre  = info->jb_min_pre * chunks_per_frm / frm_ptime;
    else
        jb_min_pre  = 1;

    /* JB max prefetch, default 4/5 JB max count */
    if (info->jb_max_pre >= frm_ptime)
        jb_max_pre  = info->jb_max_pre * chunks_per_frm / frm_ptime;
    else
        jb_max_pre  = PJ_MAX(1, jb_max * 4 / 5);

    /* JB init prefetch, default 0 */
    if (info->jb_init >= frm_ptime)
        jb_init  = info->jb_init * chunks_per_frm / frm_ptime;
    else
        jb_init  = 0;

    /* Calculate the decoding delay (in number of frames) based on FPS */
    stream->dec_delay_cnt = ((PJMEDIA_VID_STREAM_DECODE_MIN_DELAY_MSEC *
                              vfd_dec->fps.num) +
                             (1000 * vfd_dec->fps.denum) - 1) /
                            (1000 * vfd_dec->fps.denum);
    if (stream->dec_delay_cnt < 1)
        stream->dec_delay_cnt = 1;
    if (stream->dec_delay_cnt > jb_max * 4/5)
        stream->dec_delay_cnt = jb_max * 4/5;

    /* Allocate array for temporary storage for assembly of incoming
     * frames. Add more just in case.
     */
    stream->rx_frame_cnt = chunks_per_frm * 2;
    stream->rx_frames = pj_pool_calloc(pool, stream->rx_frame_cnt,
                                       sizeof(stream->rx_frames[0]));

    /* Create jitter buffer */
    status = pjmedia_jbuf_create(pool, &stream->dec->port.info.name,
                                 PJMEDIA_MAX_MRU,
                                 1000 * vfd_enc->fps.denum / vfd_enc->fps.num,
                                 jb_max, &stream->jb);
    if (status != PJ_SUCCESS)
        goto err_cleanup;


    /* Set up jitter buffer */
    pjmedia_jbuf_set_adaptive(stream->jb, jb_init, jb_min_pre, jb_max_pre);
    pjmedia_jbuf_set_discard(stream->jb, PJMEDIA_JB_DISCARD_NONE);

    /* Init RTCP session: */
    {
        pjmedia_rtcp_session_setting rtcp_setting;

        pjmedia_rtcp_session_setting_default(&rtcp_setting);
        rtcp_setting.name = stream->name.ptr;
        rtcp_setting.ssrc = info->ssrc;
        rtcp_setting.rtp_ts_base = pj_ntohl(stream->enc->rtp.out_hdr.ts);
        rtcp_setting.clock_rate = info->codec_info.clock_rate;
        rtcp_setting.samples_per_frame = 1;

        pjmedia_rtcp_init2(&stream->rtcp, &rtcp_setting);

        if (info->rtp_seq_ts_set) {
            stream->rtcp.stat.rtp_tx_last_seq = info->rtp_seq;
            stream->rtcp.stat.rtp_tx_last_ts = info->rtp_ts;
        }

        /* Subscribe to RTCP events */
        pjmedia_event_subscribe(NULL, &stream_event_cb, stream,
                                &stream->rtcp);
    }

    /* Allocate outgoing RTCP buffer, should be enough to hold SR/RR, SDES,
     * BYE, Feedback, and XR.
     */
    stream->out_rtcp_pkt_size =  sizeof(pjmedia_rtcp_sr_pkt) +
                                 sizeof(pjmedia_rtcp_common) +
                                 (4 + (unsigned)stream->cname.slen) +
                                 32 + 32;
    if (stream->out_rtcp_pkt_size > PJMEDIA_MAX_MTU)
        stream->out_rtcp_pkt_size = PJMEDIA_MAX_MTU;

    stream->out_rtcp_pkt = pj_pool_alloc(pool, stream->out_rtcp_pkt_size);
    pj_bzero(&att_param, sizeof(att_param));
    att_param.stream = stream;
    att_param.media_type = PJMEDIA_TYPE_VIDEO;
    att_param.user_data = stream;
    pj_sockaddr_cp(&att_param.rem_addr, &info->rem_addr);
    pj_sockaddr_cp(&stream->rem_rtp_addr, &info->rem_addr);
    if (info->rtcp_mux) {
        pj_sockaddr_cp(&att_param.rem_rtcp, &info->rem_addr);
    } else if (pj_sockaddr_has_addr(&info->rem_rtcp.addr)) {
        pj_sockaddr_cp(&att_param.rem_rtcp, &info->rem_rtcp);
    }
    att_param.addr_len = pj_sockaddr_get_len(&info->rem_addr);
    att_param.rtp_cb2 = &on_rx_rtp;
    att_param.rtcp_cb = &on_rx_rtcp;

    /* Only attach transport when stream is ready. */
    status = pjmedia_transport_attach2(tp, &att_param);
    if (status != PJ_SUCCESS)
        goto err_cleanup;

    stream->transport = tp;

    /* Send RTCP SDES */
    if (!stream->rtcp_sdes_bye_disabled) {
        pjmedia_vid_stream_send_rtcp_sdes(stream);
    }

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    /* NAT hole punching by sending KA packet via RTP transport. */
    if (stream->use_ka)
        send_keep_alive_packet(stream);
#endif

#if TRACE_JB
    {
        char trace_name[PJ_MAXPATH];
        pj_ssize_t len;

        pj_ansi_snprintf(trace_name, sizeof(trace_name),
                         TRACE_JB_PATH_PREFIX "%s.csv",
                         channel->port.info.name.ptr);
        status = pj_file_open(pool, trace_name, PJ_O_RDWR,
                              &stream->trace_jb_fd);
        if (status != PJ_SUCCESS) {
            stream->trace_jb_fd = TRACE_JB_INVALID_FD;
            PJ_PERROR(3,(THIS_FILE, status,
                         "Failed creating RTP trace file '%s'",
                         trace_name));
        } else {
            stream->trace_jb_buf = (char*)pj_pool_alloc(pool, PJ_LOG_MAX_SIZE);

            /* Print column header */
            len = pj_ansi_snprintf(stream->trace_jb_buf, PJ_LOG_MAX_SIZE,
                                   "Time, Operation, Size, Frame Count, "
                                   "Frame type, RTP Seq, RTP TS, RTP M, "
                                   "JB size, JB burst level, JB prefetch\n");
            if (len < 1 || len >= PJ_LOG_MAX_SIZE)
                len = PJ_LOG_MAX_SIZE - 1;
            pj_file_write(stream->trace_jb_fd, stream->trace_jb_buf, &len);
            pj_file_flush(stream->trace_jb_fd);
        }
    }
#endif

    /* Save the stream info */
    pj_memcpy(&stream->info, info, sizeof(*info));
    stream->info.codec_param = pjmedia_vid_codec_param_clone(
                                                pool, info->codec_param);
    pjmedia_rtcp_fb_info_dup(pool, &stream->info.loc_rtcp_fb,
                             &info->loc_rtcp_fb);
    pjmedia_rtcp_fb_info_dup(pool, &stream->info.rem_rtcp_fb,
                             &info->rem_rtcp_fb);

    /* Check if we should send RTCP-FB */
    if (stream->info.rem_rtcp_fb.cap_count) {
        pjmedia_rtcp_fb_info *rfi = &stream->info.rem_rtcp_fb;
        unsigned i;

        for (i = 0; i < rfi->cap_count; ++i) {
            if (rfi->caps[i].type == PJMEDIA_RTCP_FB_NACK) {
                if (rfi->caps[i].param.slen == 0) {
                    stream->send_rtcp_fb_nack = PJ_TRUE;
                    PJ_LOG(5,(stream->name.ptr, "Send RTCP-FB generic NACK"));
                } else if (pj_stricmp2(&rfi->caps[i].param, "pli")==0) {
                    stream->send_rtcp_fb_pli = PJ_TRUE;
                    PJ_LOG(5,(stream->name.ptr, "Send RTCP-FB PLI"));
                }
            }
        }
    }

    /* Check if we should process incoming RTCP-FB */
    stream->rtcp_fb_nack_cap_idx = -1;
    stream->rtcp_fb_pli_cap_idx = -1;
    if (stream->info.loc_rtcp_fb.cap_count) {
        pjmedia_rtcp_fb_info *lfi = &stream->info.loc_rtcp_fb;
        unsigned i;

        for (i = 0; i < lfi->cap_count; ++i) {
            if (lfi->caps[i].type == PJMEDIA_RTCP_FB_NACK) {
                if (lfi->caps[i].param.slen == 0) {
                    stream->rtcp_fb_nack_cap_idx = i;
                    PJ_LOG(5,(stream->name.ptr,
                              "Receive RTCP-FB generic NACK"));
                } else if (pj_stricmp2(&lfi->caps[i].param, "pli")==0) {
                    stream->rtcp_fb_pli_cap_idx = i;
                    PJ_LOG(5,(stream->name.ptr, "Receive RTCP-FB PLI"));
                }
            }
        }
    }

    /* Success! */
    *p_stream = stream;

    PJ_LOG(5,(THIS_FILE, "Video stream %s created", stream->name.ptr));

    return PJ_SUCCESS;

err_cleanup:
    pjmedia_vid_stream_destroy(stream);
    return status;
}


/*
 * Destroy stream.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_destroy( pjmedia_vid_stream *stream )
{
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Destroy request on %s..", stream->name.ptr));

    /* Stop the streaming */
    if (stream->enc)
        stream->enc->port.put_frame = NULL;
    if (stream->dec)
        stream->dec->port.get_frame = NULL;

#if TRACE_RC
    {
        unsigned total_time;

        total_time = pj_elapsed_msec(&stream->tx_start, &stream->tx_end);
        PJ_LOG(5, (stream->name.ptr,
                   "RC stat: pkt_cnt=%.2f/image, sleep=%.2fms/s, fps=%.2f",
                   stream->rc_total_pkt*1.0/stream->rc_total_img,
                   stream->rc_total_sleep*1000.0/total_time,
                   stream->rc_total_img*1000.0/total_time));
    }
#endif

    /* Unsubscribe from events */
    if (stream->codec) {
        pjmedia_event_unsubscribe(NULL, &stream_event_cb, stream,
                                  stream->codec);
    }
    pjmedia_event_unsubscribe(NULL, &stream_event_cb, stream, &stream->rtcp);

    /* Send RTCP BYE (also SDES) */
    if (stream->transport && !stream->rtcp_sdes_bye_disabled) {
        send_rtcp(stream, PJ_TRUE, PJ_TRUE, PJ_FALSE, PJ_FALSE);
    }

    /* Detach from transport
     * MUST NOT hold stream mutex while detaching from transport, as
     * it may cause deadlock. See ticket #460 for the details.
     */
    if (stream->transport) {
        pjmedia_transport_detach(stream->transport, stream);
        stream->transport = NULL;
    }

    /* This function may be called when stream is partly initialized,
     * i.e: group lock may not be created yet.
     */
    if (stream->grp_lock) {
        return pj_grp_lock_dec_ref(stream->grp_lock);
    } else {
        on_destroy(stream);
    }

    return PJ_SUCCESS;
}


/*
 * Destroy stream.
 */
static void on_destroy( void *arg )
{
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*)arg;
    pj_assert(stream);

    PJ_LOG(4,(THIS_FILE, "Destroying %s..", stream->name.ptr));

    /* Free codec. */
    if (stream->codec) {
        pjmedia_vid_codec_close(stream->codec);
        pjmedia_vid_codec_mgr_dealloc_codec(stream->codec_mgr, stream->codec);
        stream->codec = NULL;
    }

    /* Free mutex */
    stream->grp_lock = NULL;

    /* Destroy jitter buffer */
    if (stream->jb) {
        pjmedia_jbuf_destroy(stream->jb);
        stream->jb = NULL;
    }

#if TRACE_JB
    if (TRACE_JB_OPENED(stream)) {
        pj_file_close(stream->trace_jb_fd);
        stream->trace_jb_fd = TRACE_JB_INVALID_FD;
    }
#endif

    pj_pool_safe_release(&stream->own_pool);
}


/*
 * Get the port interface.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_get_port(pjmedia_vid_stream *stream,
                                                pjmedia_dir dir,
                                                pjmedia_port **p_port )
{
    PJ_ASSERT_RETURN(dir==PJMEDIA_DIR_ENCODING || dir==PJMEDIA_DIR_DECODING,
                     PJ_EINVAL);

    if (dir == PJMEDIA_DIR_ENCODING)
        *p_port = &stream->enc->port;
    else
        *p_port = &stream->dec->port;

    return PJ_SUCCESS;
}


/*
 * Get the transport object
 */
PJ_DEF(pjmedia_transport*) pjmedia_vid_stream_get_transport(
                                                    pjmedia_vid_stream *st)
{
    return st->transport;
}


/*
 * Get stream statistics.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_get_stat(
                                            const pjmedia_vid_stream *stream,
                                            pjmedia_rtcp_stat *stat)
{
    PJ_ASSERT_RETURN(stream && stat, PJ_EINVAL);

    pj_memcpy(stat, &stream->rtcp.stat, sizeof(pjmedia_rtcp_stat));
    return PJ_SUCCESS;
}


/*
 * Reset the stream statistics in the middle of a stream session.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_reset_stat(pjmedia_vid_stream *stream)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    pjmedia_rtcp_init_stat(&stream->rtcp.stat);

    return PJ_SUCCESS;
}


/*
 * Get jitter buffer state.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_get_stat_jbuf(
                                            const pjmedia_vid_stream *stream,
                                            pjmedia_jb_state *state)
{
    PJ_ASSERT_RETURN(stream && state, PJ_EINVAL);
    return pjmedia_jbuf_get_state(stream->jb, state);
}


/*
 * Get the stream info.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_get_info(
                                            const pjmedia_vid_stream *stream,
                                            pjmedia_vid_stream_info *info)
{
    PJ_ASSERT_RETURN(stream && info, PJ_EINVAL);
    pj_memcpy(info, &stream->info, sizeof(*info));
    return PJ_SUCCESS;
}


/*
 * Start stream.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_start(pjmedia_vid_stream *stream)
{

    PJ_ASSERT_RETURN(stream && stream->enc && stream->dec, PJ_EINVALIDOP);

    if (stream->enc && (stream->dir & PJMEDIA_DIR_ENCODING)) {
        stream->enc->paused = 0;
        //pjmedia_snd_stream_start(stream->enc->snd_stream);
        PJ_LOG(4,(stream->enc->port.info.name.ptr, "Encoder stream started"));
    } else {
        PJ_LOG(4,(stream->enc->port.info.name.ptr, "Encoder stream paused"));
    }

    if (stream->dec && (stream->dir & PJMEDIA_DIR_DECODING)) {
        stream->dec->paused = 0;
        //pjmedia_snd_stream_start(stream->dec->snd_stream);
        PJ_LOG(4,(stream->dec->port.info.name.ptr, "Decoder stream started"));
    } else {
        PJ_LOG(4,(stream->dec->port.info.name.ptr, "Decoder stream paused"));
    }

    return PJ_SUCCESS;
}


/*
 * Modify codec parameter.
 */
PJ_DEF(pj_status_t)
pjmedia_vid_stream_modify_codec_param(pjmedia_vid_stream *stream,
                                      const pjmedia_vid_codec_param *param)
{
    PJ_ASSERT_RETURN(stream && param, PJ_EINVAL);

    return pjmedia_vid_codec_modify(stream->codec, param);
}


/*
 * Check status.
 */
PJ_DEF(pj_bool_t) pjmedia_vid_stream_is_running(pjmedia_vid_stream *stream,
                                                pjmedia_dir dir)
{
    pj_bool_t is_running = PJ_TRUE;

    PJ_ASSERT_RETURN(stream, PJ_FALSE);

    if (dir & PJMEDIA_DIR_ENCODING) {
        is_running &= (stream->enc && !stream->enc->paused);
    }

    if (dir & PJMEDIA_DIR_DECODING) {
        is_running &= (stream->dec && !stream->dec->paused);
    }

    return is_running;
}

/*
 * Pause stream.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_pause(pjmedia_vid_stream *stream,
                                             pjmedia_dir dir)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if ((dir & PJMEDIA_DIR_ENCODING) && stream->enc) {
        stream->enc->paused = 1;
        PJ_LOG(4,(stream->enc->port.info.name.ptr, "Encoder stream paused"));
    }

    if ((dir & PJMEDIA_DIR_DECODING) && stream->dec) {
        stream->dec->paused = 1;

        /* Also reset jitter buffer */
        pj_grp_lock_acquire( stream->grp_lock );
        pjmedia_jbuf_reset(stream->jb);
        pj_grp_lock_release( stream->grp_lock );

        PJ_LOG(4,(stream->dec->port.info.name.ptr, "Decoder stream paused"));
    }

    return PJ_SUCCESS;
}


/*
 * Resume stream
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_resume(pjmedia_vid_stream *stream,
                                              pjmedia_dir dir)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if ((dir & PJMEDIA_DIR_ENCODING) && stream->enc) {
        stream->enc->paused = 0;
        stream->force_keyframe = PJ_TRUE;
        PJ_LOG(4,(stream->enc->port.info.name.ptr, "Encoder stream resumed"));
    }

    if ((dir & PJMEDIA_DIR_DECODING) && stream->dec) {
        stream->dec->paused = 0;
        stream->last_dec_seq = 0;
        PJ_LOG(4,(stream->dec->port.info.name.ptr, "Decoder stream resumed"));
    }

    return PJ_SUCCESS;
}


/*
 * Force stream to send video keyframe.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_send_keyframe(
                                                pjmedia_vid_stream *stream)
{
    pj_timestamp now;

    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if (!pjmedia_vid_stream_is_running(stream, PJMEDIA_DIR_ENCODING))
        return PJ_EINVALIDOP;

    pj_get_timestamp(&now);
    if (pj_elapsed_msec(&stream->last_keyframe_tx, &now) <
                        PJMEDIA_VID_STREAM_MIN_KEYFRAME_INTERVAL_MSEC)
    {
        return PJ_ETOOMANY;
    }

    stream->force_keyframe = PJ_TRUE;

    return PJ_SUCCESS;
}


/*
 * Send RTCP SDES.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_send_rtcp_sdes(
                                                pjmedia_vid_stream *stream)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    return send_rtcp(stream, PJ_TRUE, PJ_FALSE, PJ_FALSE, PJ_FALSE);
}


/*
 * Send RTCP BYE.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_send_rtcp_bye(
                                                pjmedia_vid_stream *stream)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if (stream->enc && stream->transport) {
        return send_rtcp(stream, PJ_TRUE, PJ_TRUE, PJ_FALSE, PJ_FALSE);
    }

    return PJ_SUCCESS;
}


/*
 * Send RTCP PLI.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_send_rtcp_pli(
                                                pjmedia_vid_stream *stream)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if (stream->transport) {
        return send_rtcp(stream, PJ_FALSE, PJ_FALSE, PJ_FALSE, PJ_TRUE);
    }

    return PJ_SUCCESS;
}


/*
 * Initialize the video stream rate control with default settings.
 */
PJ_DEF(void)
pjmedia_vid_stream_rc_config_default(pjmedia_vid_stream_rc_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->method = PJMEDIA_VID_STREAM_RC_SIMPLE_BLOCKING;
}


/*
 * Initialize the video stream send keyframe with default settings.
 */
PJ_DEF(void)
pjmedia_vid_stream_sk_config_default(pjmedia_vid_stream_sk_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->count = PJMEDIA_VID_STREAM_START_KEYFRAME_CNT;
    cfg->interval = PJMEDIA_VID_STREAM_START_KEYFRAME_INTERVAL_MSEC;
}


/**
 * Get RTP session information from video stream.
 */
PJ_DEF(pj_status_t)
pjmedia_vid_stream_get_rtp_session_info(pjmedia_vid_stream *stream,
                                    pjmedia_stream_rtp_sess_info *session_info)
{
    session_info->rx_rtp = &stream->dec->rtp;
    session_info->tx_rtp = &stream->enc->rtp;
    session_info->rtcp = &stream->rtcp;
    return PJ_SUCCESS;
}


#endif /* PJMEDIA_HAS_VIDEO */

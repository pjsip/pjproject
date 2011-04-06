/* $Id$ */
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
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/jbuf.h>
#include <pjmedia/sdp_neg.h>
#include <pjmedia/stream_common.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/compat/socket.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/sock_select.h>
#include <pj/string.h>	    /* memcpy() */


#define THIS_FILE			"vid_stream.c"
#define ERRLEVEL			1
#define LOGERR_(expr)			stream_perror expr
#define TRC_(expr)			PJ_LOG(5,expr)

/* Tracing jitter buffer operations in a stream session to a CSV file.
 * The trace will contain JB operation timestamp, frame info, RTP info, and
 * the JB state right after the operation.
 */
#define TRACE_JB			0	/* Enable/disable trace.    */
#define TRACE_JB_PATH_PREFIX		""	/* Optional path/prefix
						   for the CSV filename.    */
#if TRACE_JB
#   include <pj/file_io.h>
#   define TRACE_JB_INVALID_FD		((pj_oshandle_t)-1)
#   define TRACE_JB_OPENED(s)		(s->trace_jb_fd != TRACE_JB_INVALID_FD)
#endif

#ifndef PJMEDIA_VSTREAM_SIZE
#   define PJMEDIA_VSTREAM_SIZE	1000
#endif

#ifndef PJMEDIA_VSTREAM_INC
#   define PJMEDIA_VSTREAM_INC	1000
#endif


/**
 * Media channel.
 */
typedef struct pjmedia_vid_channel
{
    pjmedia_vid_stream	   *stream;	    /**< Parent stream.		    */
    pjmedia_dir		    dir;	    /**< Channel direction.	    */
    pjmedia_port	    port;	    /**< Port interface.	    */
    unsigned		    pt;		    /**< Payload type.		    */
    pj_bool_t		    paused;	    /**< Paused?.		    */
    void		   *buf;	    /**< Output buffer.		    */
    unsigned		    buf_size;	    /**< Size of output buffer.	    */
    unsigned                buf_len;	    /**< Length of data in buffer.  */
    pjmedia_rtp_session	    rtp;	    /**< RTP session.		    */
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
    pj_pool_t		    *own_pool;      /**< Internal pool.		    */
    pjmedia_endpt	    *endpt;	    /**< Media endpoint.	    */
    pjmedia_vid_codec_mgr   *codec_mgr;	    /**< Codec manager.		    */
    pjmedia_vid_stream_info  info;	    /**< Stream info.		    */

    pjmedia_vid_channel	    *enc;	    /**< Encoding channel.	    */
    pjmedia_vid_channel	    *dec;	    /**< Decoding channel.	    */

    pjmedia_dir		     dir;	    /**< Stream direction.	    */
    void		    *user_data;	    /**< User data.		    */
    pj_str_t		     name;	    /**< Stream name		    */
    pj_str_t		     cname;	    /**< SDES CNAME		    */

    pjmedia_transport	    *transport;	    /**< Stream transport.	    */

    pj_mutex_t		    *jb_mutex;
    pjmedia_jbuf	    *jb;	    /**< Jitter buffer.		    */
    char		     jb_last_frm;   /**< Last frame type from jb    */
    unsigned		     jb_last_frm_cnt;/**< Last JB frame type counter*/

    pjmedia_rtcp_session     rtcp;	    /**< RTCP for incoming RTP.	    */
    pj_uint32_t		     rtcp_last_tx;  /**< RTCP tx time in timestamp  */
    pj_uint32_t		     rtcp_interval; /**< Interval, in timestamp.    */
    pj_bool_t		     initial_rr;    /**< Initial RTCP RR sent	    */

    unsigned		     frame_size;    /**< Size of encoded base frame.*/
    unsigned		     frame_ts_len;  /**< Frame length in timestamp. */

#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
    pj_uint32_t		     rtcp_xr_last_tx;  /**< RTCP XR tx time 
					            in timestamp.           */
    pj_uint32_t		     rtcp_xr_interval; /**< Interval, in timestamp. */
    pj_sockaddr		     rtcp_xr_dest;     /**< Additional remote RTCP XR 
						    dest. If sin_family is 
						    zero, it will be ignored*/
    unsigned		     rtcp_xr_dest_len; /**< Length of RTCP XR dest
					            address		    */
#endif

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    pj_bool_t		     use_ka;	       /**< Stream keep-alive with non-
						    codec-VAD mechanism is
						    enabled?		    */
    pj_timestamp	     last_frm_ts_sent; /**< Timestamp of last sending
					            packet		    */
#endif

#if TRACE_JB
    pj_oshandle_t	     trace_jb_fd;   /**< Jitter tracing file handle.*/
    char		    *trace_jb_buf;  /**< Jitter tracing buffer.	    */
#endif

    pjmedia_vid_codec	    *codec;	    /**< Codec instance being used. */
    pj_uint32_t		     last_dec_ts;    /**< Last decoded timestamp.   */
    int			     last_dec_seq;   /**< Last decoded sequence.    */
};


/*
 * Print error.
 */
static void stream_perror(const char *sender, const char *title,
			  pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(4,(sender, "%s: %s [err:%d]", title, errmsg, status));
}


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


#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA != 0
/*
 * Send keep-alive packet using non-codec frame.
 */
static void send_keep_alive_packet(pjmedia_vid_stream *stream)
{
#if PJMEDIA_STREAM_ENABLE_KA == PJMEDIA_STREAM_KA_EMPTY_RTP

    /* Keep-alive packet is empty RTP */
    pj_status_t status;
    void *pkt;
    int pkt_len;

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
    pjmedia_rtcp_build_rtcp(&stream->rtcp, &pkt, &pkt_len);
    pjmedia_transport_send_rtcp(stream->transport, pkt, pkt_len);

#elif PJMEDIA_STREAM_ENABLE_KA == PJMEDIA_STREAM_KA_USER

    /* Keep-alive packet is defined in PJMEDIA_STREAM_KA_USER_PKT */
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
#endif	/* defined(PJMEDIA_STREAM_ENABLE_KA) */


/**
 * check_tx_rtcp()
 *
 * This function is can be called by either put_frame() or get_frame(),
 * to transmit periodic RTCP SR/RR report.
 */
static void check_tx_rtcp(pjmedia_vid_stream *stream, pj_uint32_t timestamp)
{
    /* Note that timestamp may represent local or remote timestamp, 
     * depending on whether this function is called from put_frame()
     * or get_frame().
     */


    if (stream->rtcp_last_tx == 0) {
	
	stream->rtcp_last_tx = timestamp;

    } else if (timestamp - stream->rtcp_last_tx >= stream->rtcp_interval) {
	
	void *rtcp_pkt;
	int len;

	pjmedia_rtcp_build_rtcp(&stream->rtcp, &rtcp_pkt, &len);

	pjmedia_transport_send_rtcp(stream->transport, rtcp_pkt, len);

	stream->rtcp_last_tx = timestamp;
    }

#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
    if (stream->rtcp.xr_enabled) {

	if (stream->rtcp_xr_last_tx == 0) {
    	
	    stream->rtcp_xr_last_tx = timestamp;

	} else if (timestamp - stream->rtcp_xr_last_tx >= 
		   stream->rtcp_xr_interval)
	{
	    int i;
	    pjmedia_jb_state jb_state;
	    void *rtcp_pkt;
	    int len;

	    /* Update RTCP XR with current JB states */
	    pjmedia_jbuf_get_state(stream->jb, &jb_state);
    	    
	    i = jb_state.avg_delay;
	    pjmedia_rtcp_xr_update_info(&stream->rtcp.xr_session, 
					PJMEDIA_RTCP_XR_INFO_JB_NOM,
					i);

	    i = jb_state.max_delay;
	    pjmedia_rtcp_xr_update_info(&stream->rtcp.xr_session, 
					PJMEDIA_RTCP_XR_INFO_JB_MAX,
					i);

	    /* Build RTCP XR packet */
	    pjmedia_rtcp_build_rtcp_xr(&stream->rtcp.xr_session, 0, 
				       &rtcp_pkt, &len);

	    /* Send the RTCP XR to remote address */
	    pjmedia_transport_send_rtcp(stream->transport, rtcp_pkt, len);

	    /* Send the RTCP XR to third-party destination if specified */
	    if (stream->rtcp_xr_dest_len) {
		pjmedia_transport_send_rtcp2(stream->transport, 
					     &stream->rtcp_xr_dest,
					     stream->rtcp_xr_dest_len, 
					     rtcp_pkt, len);
	    }

	    /* Update last tx RTCP XR */
	    stream->rtcp_xr_last_tx = timestamp;
	}
    }
#endif
}

/* Build RTCP SDES packet */
static unsigned create_rtcp_sdes(pjmedia_vid_stream *stream, pj_uint8_t *pkt,
				 unsigned max_len)
{
    pjmedia_rtcp_common hdr;
    pj_uint8_t *p = pkt;

    /* SDES header */
    hdr.version = 2;
    hdr.p = 0;
    hdr.count = 1;
    hdr.pt = 202;
    hdr.length = 2 + (4+stream->cname.slen+3)/4 - 1;
    if (max_len < (hdr.length << 2)) {
	pj_assert(!"Not enough buffer for SDES packet");
	return 0;
    }
    hdr.length = pj_htons((pj_uint16_t)hdr.length);
    hdr.ssrc = stream->enc->rtp.out_hdr.ssrc;
    pj_memcpy(p, &hdr, sizeof(hdr));
    p += sizeof(hdr);

    /* CNAME item */
    *p++ = 1;
    *p++ = (pj_uint8_t)stream->cname.slen;
    pj_memcpy(p, stream->cname.ptr, stream->cname.slen);
    p += stream->cname.slen;

    /* END */
    *p++ = '\0';
    *p++ = '\0';

    /* Pad to 32bit */
    while ((p-pkt) % 4)
	*p++ = '\0';

    return (p - pkt);
}

/* Build RTCP BYE packet */
static unsigned create_rtcp_bye(pjmedia_vid_stream *stream, pj_uint8_t *pkt,
				unsigned max_len)
{
    pjmedia_rtcp_common hdr;

    /* BYE header */
    hdr.version = 2;
    hdr.p = 0;
    hdr.count = 1;
    hdr.pt = 203;
    hdr.length = 1;
    if (max_len < (hdr.length << 2)) {
	pj_assert(!"Not enough buffer for SDES packet");
	return 0;
    }
    hdr.length = pj_htons((pj_uint16_t)hdr.length);
    hdr.ssrc = stream->enc->rtp.out_hdr.ssrc;
    pj_memcpy(pkt, &hdr, sizeof(hdr));

    return sizeof(hdr);
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
static void on_rx_rtp( void *data, 
		       void *pkt,
                       pj_ssize_t bytes_read)

{
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*) data;
    pjmedia_vid_channel *channel = stream->dec;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;
    pjmedia_rtp_status seq_st;
    pj_status_t status;
    pj_bool_t pkt_discarded = PJ_FALSE;

    /* Check for errors */
    if (bytes_read < 0) {
	LOGERR_((channel->port.info.name.ptr, "RTP recv() error", -bytes_read));
	return;
    }

    /* Ignore keep-alive packets */
    if (bytes_read < (pj_ssize_t) sizeof(pjmedia_rtp_hdr))
	return;

    /* Update RTP and RTCP session. */
    status = pjmedia_rtp_decode_rtp(&channel->rtp, pkt, bytes_read,
				    &hdr, &payload, &payloadlen);
    if (status != PJ_SUCCESS) {
	LOGERR_((channel->port.info.name.ptr, "RTP decode error", status));
	stream->rtcp.stat.rx.discard++;
	return;
    }

    /* Ignore the packet if decoder is paused */
    if (channel->paused)
	goto on_return;

    /* Update RTP session (also checks if RTP session can accept
     * the incoming packet.
     */
    pjmedia_rtp_session_update2(&channel->rtp, hdr, &seq_st, PJ_TRUE);
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

	if (seq_st.status.flag.badssrc) {
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


    /* Put "good" packet to jitter buffer, or reset the jitter buffer
     * when RTP session is restarted.
     */
    pj_mutex_lock( stream->jb_mutex );
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
    pj_mutex_unlock( stream->jb_mutex );


    /* Check if now is the time to transmit RTCP SR/RR report.
     * We only do this when stream direction is "decoding only", 
     * because otherwise check_tx_rtcp() will be handled by put_frame()
     */
    if (stream->dir == PJMEDIA_DIR_DECODING) {
	check_tx_rtcp(stream, pj_ntohl(hdr->ts));
    }

    if (status != 0) {
	LOGERR_((channel->port.info.name.ptr, "Jitter buffer put() error", 
		status));
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
	void *sr_rr_pkt;
	pj_uint8_t *pkt;
	int len;

	/* Build RR or SR */
	pjmedia_rtcp_build_rtcp(&stream->rtcp, &sr_rr_pkt, &len);
	pkt = (pj_uint8_t*) stream->enc->buf;
	pj_memcpy(pkt, sr_rr_pkt, len);
	pkt += len;

	/* Append SDES */
	len = create_rtcp_sdes(stream, (pj_uint8_t*)pkt, 
			       stream->enc->buf_size - len);
	if (len > 0) {
	    pkt += len;
	    len = ((pj_uint8_t*)pkt) - ((pj_uint8_t*)stream->enc->buf);
	    pjmedia_transport_send_rtcp(stream->transport, 
					stream->enc->buf, len);
	}

	stream->initial_rr = PJ_TRUE;
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

    /* Check for errors */
    if (bytes_read < 0) {
	LOGERR_((stream->cname.ptr, "RTCP recv() error", 
		 -bytes_read));
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
    unsigned processed = 0;


#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA != 0
    /* If the interval since last sending packet is greater than
     * PJMEDIA_STREAM_KA_INTERVAL, send keep-alive packet.
     */
    if (stream->use_ka)
    {
	pj_uint32_t dtx_duration;

	dtx_duration = pj_timestamp_diff32(&stream->last_frm_ts_sent, 
					   &frame->timestamp);
	if (dtx_duration >
	    PJMEDIA_STREAM_KA_INTERVAL * channel->port.info.clock_rate)
	{
	    send_keep_alive_packet(stream);
	    stream->last_frm_ts_sent = frame->timestamp;
	}
    }
#endif

    /* Don't do anything if stream is paused */
    if (channel->paused) {
	return PJ_SUCCESS;
    }

    /* Get frame length in timestamp unit */
    rtp_ts_len = stream->frame_ts_len;

    /* Init frame_out buffer. */
    frame_out.buf = ((char*)channel->buf) + sizeof(pjmedia_rtp_hdr);
    frame_out.size = 0;

    /* Encode! */
    status = (*stream->codec->op->encode)(stream->codec, frame, 
				          channel->buf_size - 
				          sizeof(pjmedia_rtp_hdr),
				          &frame_out);
    if (status != PJ_SUCCESS) {
        LOGERR_((channel->port.info.name.ptr, 
	        "Codec encode() error", status));

	/* Update RTP timestamp */
	pjmedia_rtp_encode_rtp(&channel->rtp, channel->pt, 1, 0,
			       rtp_ts_len, &rtphdr, &rtphdrlen);
        return status;
    }


    while (processed < frame_out.size) {
        pj_uint8_t *payload;
        pj_uint8_t *rtp_pkt;
        pj_size_t payload_len;

        /* Generate RTP payload */
        status = (*stream->codec->op->packetize)(
                                           stream->codec,
                                           (pj_uint8_t*)frame_out.buf,
                                           frame_out.size,
                                           &processed,
                                           (const pj_uint8_t**)&payload,
                                           &payload_len);
        if (status != PJ_SUCCESS) {
            LOGERR_((channel->port.info.name.ptr, 
	            "Codec pack() error", status));

	    /* Update RTP timestamp */
	    pjmedia_rtp_encode_rtp(&channel->rtp, channel->pt, 1, 0,
				   rtp_ts_len, &rtphdr, &rtphdrlen);
            return status;
        }

        /* Encapsulate. */
        status = pjmedia_rtp_encode_rtp( &channel->rtp, 
                                         channel->pt,
                                         (processed==frame_out.size?1:0),
				         payload_len,
					 rtp_ts_len, 
				         (const void**)&rtphdr, 
				         &rtphdrlen);

        if (status != PJ_SUCCESS) {
	    LOGERR_((channel->port.info.name.ptr, 
		    "RTP encode_rtp() error", status));
	    return status;
        }

        /* Next packets use same timestamp */
        rtp_ts_len = 0;

        rtp_pkt = payload - sizeof(pjmedia_rtp_hdr);

        /* Copy RTP header to the beginning of packet */
        pj_memcpy(rtp_pkt, rtphdr, sizeof(pjmedia_rtp_hdr));

        /* Send the RTP packet to the transport. */
        pjmedia_transport_send_rtp(stream->transport, rtp_pkt, 
			           payload_len + sizeof(pjmedia_rtp_hdr));
    }

    /* Check if now is the time to transmit RTCP SR/RR report. 
     * We only do this when stream direction is not "decoding only", because
     * when it is, check_tx_rtcp() will be handled by get_frame().
     */
    if (stream->dir != PJMEDIA_DIR_DECODING) {
	check_tx_rtcp(stream, pj_ntohl(channel->rtp.out_hdr.ts));
    }

    /* Do nothing if we have nothing to transmit */
    if (frame_out.size == 0) {
	return PJ_SUCCESS;
    }

    /* Update stat */
    pjmedia_rtcp_tx_rtp(&stream->rtcp, frame_out.size);
    stream->rtcp.stat.rtp_tx_last_ts = pj_ntohl(stream->enc->rtp.out_hdr.ts);
    stream->rtcp.stat.rtp_tx_last_seq = pj_ntohs(stream->enc->rtp.out_hdr.seq);

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    /* Update timestamp of last sending packet. */
    stream->last_frm_ts_sent = frame->timestamp;
#endif

    return PJ_SUCCESS;
}


static pj_status_t get_frame(pjmedia_port *port,
                             pjmedia_frame *frame)
{
    pjmedia_vid_stream *stream = (pjmedia_vid_stream*) port->port_data.pdata;
    pjmedia_vid_channel *channel = stream->dec;
    pjmedia_frame frame_in;
    pj_uint32_t last_ts = 0;
    int frm_first_seq = 0, frm_last_seq = 0;
    pj_status_t status;

    /* Return no frame is channel is paused */
    if (channel->paused) {
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    /* Repeat get payload from the jitter buffer until all payloads with same
     * timestamp are collected (a complete frame unpacketized).
     */
    {
	pj_bool_t got_frame;
	unsigned cnt;

	channel->buf_len = 0;
	got_frame = PJ_FALSE;

	/* Lock jitter buffer mutex first */
	pj_mutex_lock( stream->jb_mutex );

	/* Check if we got a decodable frame */
	for (cnt=0; ; ++cnt) {
	    char ptype;
	    pj_uint32_t ts;
	    int seq;

	    /* Peek frame from jitter buffer. */
	    pjmedia_jbuf_peek_frame(stream->jb, cnt, NULL, NULL,
				    &ptype, NULL, &ts, &seq);
	    if (ptype == PJMEDIA_JB_NORMAL_FRAME) {
		if (last_ts == 0) {
		    last_ts = ts;
		    frm_first_seq = seq;
		}
		if (ts != last_ts) {
		    got_frame = PJ_TRUE;
		    break;
		}
		frm_last_seq = seq;
	    } else if (ptype == PJMEDIA_JB_ZERO_EMPTY_FRAME) {
		/* No more packet in the jitter buffer */
		break;
	    }
	}

	if (got_frame) {
	    unsigned i;

	    /* Generate frame bitstream from the payload */
	    channel->buf_len = 0;
	    for (i = 0; i < cnt; ++i) {
		const pj_uint8_t *p;
		pj_size_t psize;
		char ptype;

		/* We use jbuf_peek_frame() as it will returns the pointer of
		 * the payload (no buffer and memcpy needed), just as we need.
		 */
		pjmedia_jbuf_peek_frame(stream->jb, i, &p,
					&psize, &ptype, NULL, NULL, NULL);

		if (ptype != PJMEDIA_JB_NORMAL_FRAME) {
		    /* Packet lost, must set payload to NULL and keep going */
		    p = NULL;
		    psize = 0;
		}

		status = (*stream->codec->op->unpacketize)(
						stream->codec,
						p, psize,
						(pj_uint8_t*)channel->buf,
						channel->buf_size,
						&channel->buf_len);
		if (status != PJ_SUCCESS) {
		    LOGERR_((channel->port.info.name.ptr, 
			    "Codec unpack() error", status));
		    /* Just ignore this unpack error */
		}
	    }

	    pjmedia_jbuf_remove_frame(stream->jb, cnt);
	}

	/* Unlock jitter buffer mutex. */
	pj_mutex_unlock( stream->jb_mutex );

	if (!got_frame) {
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    frame->size = 0;
	    return PJ_SUCCESS;
	}
    }

    /* Decode */
    frame_in.buf = channel->buf;
    frame_in.size = channel->buf_len;
    frame_in.bit_info = 0;
    frame_in.type = PJMEDIA_FRAME_TYPE_VIDEO;
    frame_in.timestamp.u64 = last_ts;

    status = stream->codec->op->decode(stream->codec, &frame_in,
				       frame->size, frame);
    if (status != PJ_SUCCESS) {
	LOGERR_((port->info.name.ptr, "codec decode() error", 
		 status));
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	frame->size = 0;
    }

    /* Check if the decoder format is changed */
    if (frame->bit_info & PJMEDIA_VID_CODEC_EVENT_FMT_CHANGED) {
	/* Update param from codec */
	stream->codec->op->get_param(stream->codec, stream->info.codec_param);

	/* Update decoding channel port info */
	pjmedia_format_copy(&channel->port.info.fmt,
			    &stream->info.codec_param->dec_fmt);
    }

    /* Learn remote frame rate after successful decoding */
    if (0 && frame->type == PJMEDIA_FRAME_TYPE_VIDEO && frame->size)
    {
	/* Only check remote frame rate when timestamp is not wrapping and
	 * sequence is increased by 1.
	 */
	if (last_ts > stream->last_dec_ts &&
	    frm_first_seq - stream->last_dec_seq == 1)
	{
	    pj_uint32_t ts_diff;
	    pjmedia_video_format_detail *vfd;

	    ts_diff = last_ts - stream->last_dec_ts;
	    vfd = pjmedia_format_get_video_format_detail(
				    &channel->port.info.fmt, PJ_TRUE);
	    if ((int)(stream->info.codec_info.clock_rate / ts_diff) !=
		vfd->fps.num / vfd->fps.denum)
	    {
		/* Frame rate changed, update decoding port info */
		vfd->fps.num = stream->info.codec_info.clock_rate;
		vfd->fps.denum = ts_diff;

		/* Update stream info */
		stream->info.codec_param->dec_fmt.det.vid.fps = vfd->fps;

		/* Set bit_info */
		frame->bit_info |= PJMEDIA_VID_CODEC_EVENT_FMT_CHANGED;

		PJ_LOG(5, (channel->port.info.name.ptr, "Frame rate changed to %.2ffps",
			   (1.0 * vfd->fps.num / vfd->fps.denum)));
	    }
	}

	/* Update last frame seq and timestamp */
	stream->last_dec_seq = frm_last_seq;
	stream->last_dec_ts = last_ts;
    }

#if PJ_LOG_MAX_LEVEL >= 5
    if (frame->bit_info & PJMEDIA_VID_CODEC_EVENT_FMT_CHANGED) {
	pjmedia_port_info *pi = &channel->port.info;

	PJ_LOG(5, (channel->port.info.name.ptr,
		   "Decoding format changed to %dx%d %c%c%c%c %.2ffps",
		   pi->fmt.det.vid.size.w, pi->fmt.det.vid.size.h,
		   ((pi->fmt.id & 0x000000FF) >> 0),
		   ((pi->fmt.id & 0x0000FF00) >> 8),
		   ((pi->fmt.id & 0x00FF0000) >> 16),
		   ((pi->fmt.id & 0xFF000000) >> 24),
		   (1.0*pi->fmt.det.vid.fps.num/pi->fmt.det.vid.fps.denum)));
    }
#endif

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
    channel->buf_size = sizeof(pjmedia_rtp_hdr) + stream->frame_size;

    /* It should big enough to hold (minimally) RTCP SR with an SDES. */
    min_out_pkt_size =  sizeof(pjmedia_rtcp_sr_pkt) +
			sizeof(pjmedia_rtcp_common) +
			(4 + stream->cname.slen) +
			32;

    if (channel->buf_size < min_out_pkt_size)
	channel->buf_size = min_out_pkt_size;

    channel->buf = pj_pool_alloc(pool, channel->buf_size);
    PJ_ASSERT_RETURN(channel->buf != NULL, PJ_ENOMEM);

    /* Create RTP and RTCP sessions: */
    if (info->rtp_seq_ts_set == 0) {
	status = pjmedia_rtp_session_init(&channel->rtp, pt, info->ssrc);
    } else {
	pjmedia_rtp_session_setting settings;

	settings.flags = (pj_uint8_t)((info->rtp_seq_ts_set << 2) | 3);
	settings.default_pt = pt;
	settings.sender_ssrc = info->ssrc;
	settings.seq = info->rtp_seq;
	settings.ts = info->rtp_ts;
	status = pjmedia_rtp_session_init2(&channel->rtp, settings);
    }
    if (status != PJ_SUCCESS)
	return status;

    /* Init port. */
    pjmedia_port_info_init2(pi, &name,
			    PJMEDIA_PORT_SIGNATURE('V', 'C', 'H', 'N'),
			    dir, fmt);
    if (dir == PJMEDIA_DIR_DECODING) {
	channel->port.get_frame = &get_frame;
    } else {
	pi->fmt.id = info->codec_param->dec_fmt.id;
	channel->port.put_frame = &put_frame;
    }

    /* Init port. */
    channel->port.port_data.pdata = stream;

    PJ_LOG(5, (name.ptr, "%s channel created %dx%d %c%c%c%c%s%.*s %.2ffps",
	       (dir==PJMEDIA_DIR_ENCODING?"Encoding":"Decoding"),
	       pi->fmt.det.vid.size.w, pi->fmt.det.vid.size.h,
	       ((pi->fmt.id & 0x000000FF) >> 0),
	       ((pi->fmt.id & 0x0000FF00) >> 8),
	       ((pi->fmt.id & 0x00FF0000) >> 16),
	       ((pi->fmt.id & 0xFF000000) >> 24),
	       (dir==PJMEDIA_DIR_ENCODING?"->":"<-"),
	       info->codec_info.encoding_name.slen,
	       info->codec_info.encoding_name.ptr,
	       (1.0*pi->fmt.det.vid.fps.num/pi->fmt.det.vid.fps.denum)));

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
    unsigned jb_init, jb_max, jb_min_pre, jb_max_pre, len;
    int frm_ptime, chunks_per_frm;
    pjmedia_video_format_detail *vfd_enc;
    char *p;
    pj_status_t status;

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
	return status;


    /* Get codec param: */
    if (!info->codec_param) {
	pjmedia_vid_codec_param def_param;

	status = pjmedia_vid_codec_mgr_get_default_param(stream->codec_mgr, 
						         &info->codec_info,
						         &def_param);
	if (status != PJ_SUCCESS)
	    return status;

	info->codec_param = pjmedia_vid_codec_param_clone(pool, &def_param);
	pj_assert(info->codec_param);
    }

    vfd_enc = pjmedia_format_get_video_format_detail(
					&info->codec_param->enc_fmt, PJ_TRUE);

    /* Init stream: */
    stream->endpt = endpt;
    stream->dir = info->dir;
    stream->user_data = user_data;
    stream->rtcp_interval = (PJMEDIA_RTCP_INTERVAL-500 + (pj_rand()%1000)) *
			    info->codec_info.clock_rate / 1000;

    stream->jb_last_frm = PJMEDIA_JB_NORMAL_FRAME;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    stream->use_ka = info->use_ka;
#endif

    /* Build random RTCP CNAME. CNAME has user@host format */
    stream->cname.ptr = p = (char*) pj_pool_alloc(pool, 20);
    pj_create_random_string(p, 5);
    p += 5;
    *p++ = '@'; *p++ = 'p'; *p++ = 'j';
    pj_create_random_string(p, 6);
    p += 6;
    *p++ = '.'; *p++ = 'o'; *p++ = 'r'; *p++ = 'g';
    stream->cname.slen = p - stream->cname.ptr;


    /* Create mutex to protect jitter buffer: */

    status = pj_mutex_create_simple(pool, NULL, &stream->jb_mutex);
    if (status != PJ_SUCCESS)
	return status;

    /* Init codec param */
    info->codec_param->dir = info->dir;
    info->codec_param->enc_mtu = PJMEDIA_MAX_MTU - sizeof(pjmedia_rtp_hdr);

    /* Init and open the codec. */
    status = stream->codec->op->init(stream->codec, pool);
    if (status != PJ_SUCCESS)
	return status;
    status = stream->codec->op->open(stream->codec, info->codec_param);
    if (status != PJ_SUCCESS)
	return status;

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

    /* Create decoder channel */
    status = create_channel( pool, stream, PJMEDIA_DIR_DECODING, 
			     info->rx_pt, info, &stream->dec);
    if (status != PJ_SUCCESS)
	return status;


    /* Create encoder channel */
    status = create_channel( pool, stream, PJMEDIA_DIR_ENCODING, 
			     info->tx_pt, info, &stream->enc);
    if (status != PJ_SUCCESS)
	return status;

    /* Init jitter buffer parameters: */
    frm_ptime	    = 1000 * vfd_enc->fps.denum / vfd_enc->fps.num;
    chunks_per_frm  = stream->frame_size / PJMEDIA_MAX_MTU;

    /* JB max count, default 500ms */
    if (info->jb_max >= frm_ptime)
	jb_max	    = info->jb_max * chunks_per_frm / frm_ptime;
    else
	jb_max	    = 500 * chunks_per_frm / frm_ptime;

    /* JB min prefetch, default 1 frame */
    if (info->jb_min_pre >= frm_ptime)
	jb_min_pre  = info->jb_min_pre * chunks_per_frm / frm_ptime;
    else
	jb_min_pre  = 1;

    /* JB max prefetch, default 4/5 JB max count */
    if (info->jb_max_pre >= frm_ptime)
	jb_max_pre  = info->jb_max_pre * chunks_per_frm / frm_ptime;
    else
	jb_max_pre  = jb_max * 4 / 5;

    /* JB init prefetch, default 0 */
    if (info->jb_init >= frm_ptime)
	jb_init  = info->jb_init * chunks_per_frm / frm_ptime;
    else
	jb_init  = 0;

    /* Create jitter buffer */
    status = pjmedia_jbuf_create(pool, &stream->dec->port.info.name,
				 PJMEDIA_MAX_MTU, 
				 1000 * vfd_enc->fps.denum / vfd_enc->fps.num,
				 jb_max, &stream->jb);
    if (status != PJ_SUCCESS)
	return status;


    /* Set up jitter buffer */
    pjmedia_jbuf_set_adaptive( stream->jb, jb_init, jb_min_pre, jb_max_pre);
    //pjmedia_jbuf_enable_discard(stream->jb, PJ_FALSE);

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
    }

    /* Only attach transport when stream is ready. */
    status = pjmedia_transport_attach(tp, stream, &info->rem_addr, 
				      &info->rem_rtcp, 
				      pj_sockaddr_get_len(&info->rem_addr), 
                                      &on_rx_rtp, &on_rx_rtcp);
    if (status != PJ_SUCCESS)
	return status;

    stream->transport = tp;

    /* Send RTCP SDES */
    len = create_rtcp_sdes(stream, (pj_uint8_t*)stream->enc->buf, 
			   stream->enc->buf_size);
    if (len != 0) {
	pjmedia_transport_send_rtcp(stream->transport, 
				    stream->enc->buf, len);
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
	    PJ_LOG(3,(THIS_FILE, "Failed creating RTP trace file '%s'", 
		      trace_name));
	} else {
	    stream->trace_jb_buf = (char*)pj_pool_alloc(pool, PJ_LOG_MAX_SIZE);

	    /* Print column header */
	    len = pj_ansi_snprintf(stream->trace_jb_buf, PJ_LOG_MAX_SIZE,
				   "Time, Operation, Size, Frame Count, "
				   "Frame type, RTP Seq, RTP TS, RTP M, "
				   "JB size, JB burst level, JB prefetch\n");
	    pj_file_write(stream->trace_jb_fd, stream->trace_jb_buf, &len);
	    pj_file_flush(stream->trace_jb_fd);
	}
    }
#endif

    /* Save the stream info */
    pj_memcpy(&stream->info, info, sizeof(*info));
    stream->info.codec_param = pjmedia_vid_codec_param_clone(
						pool, info->codec_param);

    /* Success! */
    *p_stream = stream;

    PJ_LOG(5,(THIS_FILE, "Video stream %s created", stream->name.ptr));

    return PJ_SUCCESS;
}


/*
 * Destroy stream.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_destroy( pjmedia_vid_stream *stream )
{
    unsigned len;
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
    /* Send RTCP XR on stream destroy */
    if (stream->rtcp.xr_enabled) {
	int i;
	pjmedia_jb_state jb_state;
	void *rtcp_pkt;
	int len;

	/* Update RTCP XR with current JB states */
	pjmedia_jbuf_get_state(stream->jb, &jb_state);
    	    
	i = jb_state.avg_delay;
	pjmedia_rtcp_xr_update_info(&stream->rtcp.xr_session, 
				    PJMEDIA_RTCP_XR_INFO_JB_NOM,
				    i);

	i = jb_state.max_delay;
	pjmedia_rtcp_xr_update_info(&stream->rtcp.xr_session, 
				    PJMEDIA_RTCP_XR_INFO_JB_MAX,
				    i);

	/* Build RTCP XR packet */
	pjmedia_rtcp_build_rtcp_xr(&stream->rtcp.xr_session, 0, 
				   &rtcp_pkt, &len);

	/* Send the RTCP XR to remote address */
	pjmedia_transport_send_rtcp(stream->transport, rtcp_pkt, len);
	
	/* Send the RTCP XR to third-party destination if specified */
	if (stream->rtcp_xr_dest_len) {
	    pjmedia_transport_send_rtcp2(stream->transport, 
					 &stream->rtcp_xr_dest,
					 stream->rtcp_xr_dest_len, 
					 rtcp_pkt, len);
	}
    }
#endif

    /* Send RTCP BYE */
    if (stream->enc && stream->transport) {
	len = create_rtcp_bye(stream, (pj_uint8_t*)stream->enc->buf,
			      stream->enc->buf_size);
	if (len != 0) {
	    pjmedia_transport_send_rtcp(stream->transport, 
					stream->enc->buf, len);
	}
    }

    /* Detach from transport 
     * MUST NOT hold stream mutex while detaching from transport, as
     * it may cause deadlock. See ticket #460 for the details.
     */
    if (stream->transport) {
	pjmedia_transport_detach(stream->transport, stream);
	stream->transport = NULL;
    }

    /* This function may be called when stream is partly initialized. */
    if (stream->jb_mutex)
	pj_mutex_lock(stream->jb_mutex);


    /* Free codec. */
    if (stream->codec) {
	stream->codec->op->close(stream->codec);
	pjmedia_vid_codec_mgr_dealloc_codec(stream->codec_mgr, stream->codec);
	stream->codec = NULL;
    }

    /* Free mutex */
    
    if (stream->jb_mutex) {
	pj_mutex_destroy(stream->jb_mutex);
	stream->jb_mutex = NULL;
    }

    /* Destroy jitter buffer */
    if (stream->jb)
	pjmedia_jbuf_destroy(stream->jb);

#if TRACE_JB
    if (TRACE_JB_OPENED(stream)) {
	pj_file_close(stream->trace_jb_fd);
	stream->trace_jb_fd = TRACE_JB_INVALID_FD;
    }
#endif

    if (stream->own_pool) {
	pj_pool_t *pool = stream->own_pool;
	stream->own_pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
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


#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
/*
 * Get stream extended statistics.
 */
PJ_DEF(pj_status_t) pjmedia_stream_get_stat_xr(
					    const pjmedia_vid_stream *stream,
					    pjmedia_rtcp_xr_stat *stat)
{
    PJ_ASSERT_RETURN(stream && stat, PJ_EINVAL);

    if (stream->rtcp.xr_enabled) {
	pj_memcpy(stat, &stream->rtcp.xr_session.stat,
		  sizeof(pjmedia_rtcp_xr_stat));
	return PJ_SUCCESS;
    }
    return PJ_ENOTFOUND;
}
#endif

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
	pj_mutex_lock( stream->jb_mutex );
	pjmedia_jbuf_reset(stream->jb);
	pj_mutex_unlock( stream->jb_mutex );

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
	PJ_LOG(4,(stream->enc->port.info.name.ptr, "Encoder stream resumed"));
    }

    if ((dir & PJMEDIA_DIR_DECODING) && stream->dec) {
	stream->dec->paused = 0;
	PJ_LOG(4,(stream->dec->port.info.name.ptr, "Decoder stream resumed"));
    }

    return PJ_SUCCESS;
}


static const pj_str_t ID_VIDEO = { "video", 5};
static const pj_str_t ID_IN = { "IN", 2 };
static const pj_str_t ID_IP4 = { "IP4", 3};
static const pj_str_t ID_IP6 = { "IP6", 3};
static const pj_str_t ID_RTP_AVP = { "RTP/AVP", 7 };
static const pj_str_t ID_RTP_SAVP = { "RTP/SAVP", 8 };
//static const pj_str_t ID_SDP_NAME = { "pjmedia", 7 };
static const pj_str_t ID_RTPMAP = { "rtpmap", 6 };

static const pj_str_t STR_INACTIVE = { "inactive", 8 };
static const pj_str_t STR_SENDRECV = { "sendrecv", 8 };
static const pj_str_t STR_SENDONLY = { "sendonly", 8 };
static const pj_str_t STR_RECVONLY = { "recvonly", 8 };


/*
 * Internal function for collecting codec info and param from the SDP media.
 */
static pj_status_t get_video_codec_info_param(pjmedia_vid_stream_info *si,
					      pj_pool_t *pool,
					      pjmedia_vid_codec_mgr *mgr,
					      const pjmedia_sdp_media *local_m,
					      const pjmedia_sdp_media *rem_m)
{
    unsigned pt = 0;
    const pjmedia_vid_codec_info *p_info;
    pj_status_t status;

    pt = pj_strtoul(&local_m->desc.fmt[0]);

    /* Get codec info. */
    status = pjmedia_vid_codec_mgr_get_codec_info(mgr, pt, &p_info);
    if (status != PJ_SUCCESS)
	return status;

    si->codec_info = *p_info;

    /* Get payload type for receiving direction */
    si->rx_pt = pt;

    /* Get payload type for transmitting direction */
    if (pt < 96) {
	/* For static payload type, pt's are symetric */
	si->tx_pt = pt;

    } else {
	unsigned i;

	/* Determine payload type for outgoing channel, by finding
	 * dynamic payload type in remote SDP that matches the answer.
	 */
	si->tx_pt = 0xFFFF;
	for (i=0; i<rem_m->desc.fmt_count; ++i) {
	    if (pjmedia_sdp_neg_fmt_match(NULL,
					  (pjmedia_sdp_media*)local_m, 0,
					  (pjmedia_sdp_media*)rem_m, i, 0) ==
		PJ_SUCCESS)
	    {
		/* Found matched codec. */
		si->tx_pt = pj_strtoul(&rem_m->desc.fmt[i]);
		break;
	    }
	}

	if (si->tx_pt == 0xFFFF)
	    return PJMEDIA_EMISSINGRTPMAP;
    }

  
    /* Now that we have codec info, get the codec param. */
    si->codec_param = PJ_POOL_ALLOC_T(pool, pjmedia_vid_codec_param);
    status = pjmedia_vid_codec_mgr_get_default_param(mgr, 
						     &si->codec_info,
						     si->codec_param);

    /* Get remote fmtp for our encoder. */
    pjmedia_stream_info_parse_fmtp(pool, rem_m, si->tx_pt,
				   &si->codec_param->enc_fmtp);

    /* Get local fmtp for our decoder. */
    pjmedia_stream_info_parse_fmtp(pool, local_m, si->rx_pt,
				   &si->codec_param->dec_fmtp);

    /* When direction is NONE (it means SDP negotiation has failed) we don't
     * need to return a failure here, as returning failure will cause
     * the whole SDP to be rejected. See ticket #:
     *	http://
     *
     * Thanks Alain Totouom 
     */
    if (status != PJ_SUCCESS && si->dir != PJMEDIA_DIR_NONE)
	return status;

    return PJ_SUCCESS;
}



/*
 * Create stream info from SDP media line.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_info_from_sdp(
					   pjmedia_vid_stream_info *si,
					   pj_pool_t *pool,
					   pjmedia_endpt *endpt,
					   const pjmedia_sdp_session *local,
					   const pjmedia_sdp_session *remote,
					   unsigned stream_idx)
{
    const pjmedia_sdp_attr *attr;
    const pjmedia_sdp_media *local_m;
    const pjmedia_sdp_media *rem_m;
    const pjmedia_sdp_conn *local_conn;
    const pjmedia_sdp_conn *rem_conn;
    int rem_af, local_af;
    pj_sockaddr local_addr;
    pj_status_t status;

    PJ_UNUSED_ARG(endpt);

    /* Validate arguments: */
    PJ_ASSERT_RETURN(pool && si && local && remote, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_idx < local->media_count, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_idx < remote->media_count, PJ_EINVAL);

    /* Keep SDP shortcuts */
    local_m = local->media[stream_idx];
    rem_m = remote->media[stream_idx];

    local_conn = local_m->conn ? local_m->conn : local->conn;
    if (local_conn == NULL)
	return PJMEDIA_SDP_EMISSINGCONN;

    rem_conn = rem_m->conn ? rem_m->conn : remote->conn;
    if (rem_conn == NULL)
	return PJMEDIA_SDP_EMISSINGCONN;

    /* Media type must be video */
    if (pj_stricmp(&local_m->desc.media, &ID_VIDEO) != 0)
	return PJMEDIA_EINVALIMEDIATYPE;


    /* Reset: */

    pj_bzero(si, sizeof(*si));

#if PJMEDIA_HAS_RTCP_XR && PJMEDIA_STREAM_ENABLE_XR
    /* Set default RTCP XR enabled/disabled */
    si->rtcp_xr_enabled = PJ_TRUE;
#endif

    /* Media type: */
    si->type = PJMEDIA_TYPE_VIDEO;

    /* Transport protocol */

    /* At this point, transport type must be compatible, 
     * the transport instance will do more validation later.
     */
    status = pjmedia_sdp_transport_cmp(&rem_m->desc.transport, 
				       &local_m->desc.transport);
    if (status != PJ_SUCCESS)
	return PJMEDIA_SDPNEG_EINVANSTP;

    if (pj_stricmp(&local_m->desc.transport, &ID_RTP_AVP) == 0) {

	si->proto = PJMEDIA_TP_PROTO_RTP_AVP;

    } else if (pj_stricmp(&local_m->desc.transport, &ID_RTP_SAVP) == 0) {

	si->proto = PJMEDIA_TP_PROTO_RTP_SAVP;

    } else {

	si->proto = PJMEDIA_TP_PROTO_UNKNOWN;
	return PJ_SUCCESS;
    }


    /* Check address family in remote SDP */
    rem_af = pj_AF_UNSPEC();
    if (pj_stricmp(&rem_conn->net_type, &ID_IN)==0) {
	if (pj_stricmp(&rem_conn->addr_type, &ID_IP4)==0) {
	    rem_af = pj_AF_INET();
	} else if (pj_stricmp(&rem_conn->addr_type, &ID_IP6)==0) {
	    rem_af = pj_AF_INET6();
	}
    }

    if (rem_af==pj_AF_UNSPEC()) {
	/* Unsupported address family */
	return PJ_EAFNOTSUP;
    }

    /* Set remote address: */
    status = pj_sockaddr_init(rem_af, &si->rem_addr, &rem_conn->addr, 
			      rem_m->desc.port);
    if (status != PJ_SUCCESS) {
	/* Invalid IP address. */
	return PJMEDIA_EINVALIDIP;
    }

    /* Check address family of local info */
    local_af = pj_AF_UNSPEC();
    if (pj_stricmp(&local_conn->net_type, &ID_IN)==0) {
	if (pj_stricmp(&local_conn->addr_type, &ID_IP4)==0) {
	    local_af = pj_AF_INET();
	} else if (pj_stricmp(&local_conn->addr_type, &ID_IP6)==0) {
	    local_af = pj_AF_INET6();
	}
    }

    if (local_af==pj_AF_UNSPEC()) {
	/* Unsupported address family */
	return PJ_SUCCESS;
    }

    /* Set remote address: */
    status = pj_sockaddr_init(local_af, &local_addr, &local_conn->addr, 
			      local_m->desc.port);
    if (status != PJ_SUCCESS) {
	/* Invalid IP address. */
	return PJMEDIA_EINVALIDIP;
    }

    /* Local and remote address family must match */
    if (local_af != rem_af)
	return PJ_EAFNOTSUP;

    /* Media direction: */

    if (local_m->desc.port == 0 || 
	pj_sockaddr_has_addr(&local_addr)==PJ_FALSE ||
	pj_sockaddr_has_addr(&si->rem_addr)==PJ_FALSE ||
	pjmedia_sdp_media_find_attr(local_m, &STR_INACTIVE, NULL)!=NULL)
    {
	/* Inactive stream. */

	si->dir = PJMEDIA_DIR_NONE;

    } else if (pjmedia_sdp_media_find_attr(local_m, &STR_SENDONLY, NULL)!=NULL) {

	/* Send only stream. */

	si->dir = PJMEDIA_DIR_ENCODING;

    } else if (pjmedia_sdp_media_find_attr(local_m, &STR_RECVONLY, NULL)!=NULL) {

	/* Recv only stream. */

	si->dir = PJMEDIA_DIR_DECODING;

    } else {

	/* Send and receive stream. */

	si->dir = PJMEDIA_DIR_ENCODING_DECODING;

    }

    /* No need to do anything else if stream is rejected */
    if (local_m->desc.port == 0) {
	return PJ_SUCCESS;
    }

    /* If "rtcp" attribute is present in the SDP, set the RTCP address
     * from that attribute. Otherwise, calculate from RTP address.
     */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
				  "rtcp", NULL);
    if (attr) {
	pjmedia_sdp_rtcp_attr rtcp;
	status = pjmedia_sdp_attr_get_rtcp(attr, &rtcp);
	if (status == PJ_SUCCESS) {
	    if (rtcp.addr.slen) {
		status = pj_sockaddr_init(rem_af, &si->rem_rtcp, &rtcp.addr,
					  (pj_uint16_t)rtcp.port);
	    } else {
		pj_sockaddr_init(rem_af, &si->rem_rtcp, NULL, 
				 (pj_uint16_t)rtcp.port);
		pj_memcpy(pj_sockaddr_get_addr(&si->rem_rtcp),
		          pj_sockaddr_get_addr(&si->rem_addr),
			  pj_sockaddr_get_addr_len(&si->rem_addr));
	    }
	}
    }
    
    if (!pj_sockaddr_has_addr(&si->rem_rtcp)) {
	int rtcp_port;

	pj_memcpy(&si->rem_rtcp, &si->rem_addr, sizeof(pj_sockaddr));
	rtcp_port = pj_sockaddr_get_port(&si->rem_addr) + 1;
	pj_sockaddr_set_port(&si->rem_rtcp, (pj_uint16_t)rtcp_port);
    }

    /* Get codec info and param */
    status = get_video_codec_info_param(si, pool, NULL, local_m, rem_m);

    /* Leave SSRC to random. */
    si->ssrc = pj_rand();

    /* Set default jitter buffer parameter. */
    si->jb_init = si->jb_max = si->jb_min_pre = si->jb_max_pre = -1;

    return status;
}


/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/stream.h>
#include <pjmedia/errno.h>
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/jbuf.h>
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


#define THIS_FILE			"stream.c"
#define ERRLEVEL			1
#define LOGERR_(expr)			stream_perror expr
#define TRC_(expr)			PJ_LOG(5,expr)

/**
 * Media channel.
 */
struct pjmedia_channel
{
    pjmedia_stream	   *stream;	    /**< Parent stream.		    */
    pjmedia_dir		    dir;	    /**< Channel direction.	    */
    unsigned		    pt;		    /**< Payload type.		    */
    pj_bool_t		    paused;	    /**< Paused?.		    */
    unsigned		    in_pkt_size;    /**< Size of input buffer.	    */
    void		   *in_pkt;	    /**< Input buffer.		    */
    unsigned		    out_pkt_size;   /**< Size of output buffer.	    */
    void		   *out_pkt;	    /**< Output buffer.		    */
    unsigned		    pcm_buf_size;   /**< Size of PCM buffer.	    */
    void		   *pcm_buf;	    /**< PCM buffer.		    */
    pjmedia_rtp_session	    rtp;	    /**< RTP session.		    */
};


struct dtmf
{
    int		    event;
    pj_uint32_t	    start_ts;
};

/**
 * This structure describes media stream.
 * A media stream is bidirectional media transmission between two endpoints.
 * It consists of two channels, i.e. encoding and decoding channels.
 * A media stream corresponds to a single "m=" line in a SDP session
 * description.
 */
struct pjmedia_stream
{
    pjmedia_endpt	    *endpt;	    /**< Media endpoint.	    */
    pjmedia_codec_mgr	    *codec_mgr;	    /**< Codec manager instance.    */

    pjmedia_port	     port;	    /**< Port interface.	    */
    pjmedia_channel	    *enc;	    /**< Encoding channel.	    */
    pjmedia_channel	    *dec;	    /**< Decoding channel.	    */

    pjmedia_dir		     dir;	    /**< Stream direction.	    */
    void		    *user_data;	    /**< User data.		    */

    pjmedia_codec	    *codec;	    /**< Codec instance being used. */
    pj_size_t		     frame_size;    /**< Size of encoded frame.	    */
    pj_mutex_t		    *jb_mutex;
    pjmedia_jbuf	    *jb;	    /**< Jitter buffer.		    */

    pjmedia_sock_info	     skinfo;	    /**< Transport info.	    */
    pj_sockaddr_in	     rem_rtp_addr;  /**< Remote RTP address.	    */
    pj_sockaddr_in	     rem_rtcp_addr; /**< Remote RTCP address.	    */


    pjmedia_rtcp_session     rtcp;	    /**< RTCP for incoming RTP.	    */

    pj_ioqueue_key_t	    *rtp_key;	    /**< RTP ioqueue key.	    */
    pj_ioqueue_op_key_t	     rtp_op_key;    /**< The pending read op key.   */
    pj_sockaddr_in	     rtp_src_addr;  /**< addr of src pkt from remote*/
    unsigned		     rtp_src_cnt;   /**< if different, # of pkt rcv */
    int			     rtp_addrlen;   /**< Address length.	    */

    pj_ioqueue_key_t	    *rtcp_key;	    /**< RTCP ioqueue key.	    */
    pj_ioqueue_op_key_t	     rtcp_op_key;   /**< The pending read op key.   */
    pj_size_t		     rtcp_pkt_size; /**< Size of RTCP packet buf.   */
    char		     rtcp_pkt[512]; /**< RTCP packet buffer.	    */
    pj_uint32_t		     rtcp_last_tx;  /**< RTCP tx time in timestamp  */
    pj_uint32_t		     rtcp_interval; /**< Interval, in timestamp.    */
    int			     rtcp_addrlen;  /**< Address length.	    */

    /* RFC 2833 DTMF transmission queue: */
    int			     tx_event_pt;   /**< Outgoing pt for dtmf.	    */
    int			     tx_dtmf_count; /**< # of digits in tx dtmf buf.*/
    struct dtmf		     tx_dtmf_buf[32];/**< Outgoing dtmf queue.	    */

    /* Incoming DTMF: */
    int			     rx_event_pt;   /**< Incoming pt for dtmf.	    */
    int			     last_dtmf;	    /**< Current digit, or -1.	    */
    pj_uint32_t		     last_dtmf_dur; /**< Start ts for cur digit.    */
    unsigned		     rx_dtmf_count; /**< # of digits in dtmf rx buf.*/
    char		     rx_dtmf_buf[32];/**< Incoming DTMF buffer.	    */
};


/* RFC 2833 digit */
static const char digitmap[16] = { '0', '1', '2', '3', 
				   '4', '5', '6', '7', 
				   '8', '9', '*', '#',
				   'A', 'B', 'C', 'D'};

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


/*
 * play_callback()
 *
 * This callback is called by sound device's player thread when it
 * needs to feed the player with some frames.
 */
static pj_status_t get_frame( pjmedia_port *port, pjmedia_frame *frame)
{
    pjmedia_stream *stream = port->user_data;
    pjmedia_channel *channel = stream->dec;
    
    char frame_type;
    pj_status_t status;
    struct pjmedia_frame frame_in, frame_out;

    /* Return no frame is channel is paused */
    if (channel->paused) {
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    /* Lock jitter buffer mutex */
    pj_mutex_lock( stream->jb_mutex );

    /* Get frame from jitter buffer. */
    status = pjmedia_jbuf_get_frame(stream->jb, channel->out_pkt,
				    &frame_type);

    /* Unlock jitter buffer mutex. */
    pj_mutex_unlock( stream->jb_mutex );

    if (status != PJ_SUCCESS || frame_type == PJMEDIA_JB_ZERO_FRAME ||
	frame_type == PJMEDIA_JB_MISSING_FRAME) 
    {
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    /* Decode */
    frame_in.buf = channel->out_pkt;
    frame_in.size = stream->frame_size;
    frame_in.type = PJMEDIA_FRAME_TYPE_AUDIO;  /* ignored */
    frame_out.buf = channel->pcm_buf;
    status = stream->codec->op->decode( stream->codec, &frame_in,
					channel->pcm_buf_size, &frame_out);
    if (status != 0) {
	LOGERR_((port->info.name.ptr, "codec decode() error", status));

	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    /* Put in sound buffer. */
    if (frame_out.size > frame->size) {
	PJ_LOG(4,(port->info.name.ptr, 
		  "Sound playout buffer truncated %d bytes", 
		  frame_out.size - frame->size));
	frame_out.size = frame->size;
    }

    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = frame_out.size;
    frame->timestamp.u64 = 0;
    pj_memcpy(frame->buf, frame_out.buf, frame_out.size);

    return PJ_SUCCESS;
}


/*
 * Transmit DTMF
 */
static void create_dtmf_payload(pjmedia_stream *stream, 
			  struct pjmedia_frame *frame_out)
{
    pjmedia_rtp_dtmf_event *event;
    struct dtmf *digit = &stream->tx_dtmf_buf[0];
    unsigned duration;
    pj_uint32_t cur_ts;

    pj_assert(sizeof(pjmedia_rtp_dtmf_event) == 4);

    event = frame_out->buf;
    cur_ts = pj_ntohl(stream->enc->rtp.out_hdr.ts);
    duration = cur_ts - digit->start_ts;

    event->event = (pj_uint8_t)digit->event;
    event->e_vol = 10;
    event->duration = pj_htons((pj_uint16_t)duration);

    if (duration >= PJMEDIA_DTMF_DURATION) {
	event->e_vol |= 0x80;

	/* Prepare next digit. */
	pj_mutex_lock(stream->jb_mutex);
	pj_array_erase(stream->tx_dtmf_buf, sizeof(stream->tx_dtmf_buf[0]),
		       stream->tx_dtmf_count, 0);
	--stream->tx_dtmf_count;

	stream->tx_dtmf_buf[0].start_ts = cur_ts;
	pj_mutex_unlock(stream->jb_mutex);

	if (stream->tx_dtmf_count)
	    PJ_LOG(5,(stream->port.info.name.ptr,
		      "Sending DTMF digit id %c", 
		      digitmap[stream->tx_dtmf_buf[0].event]));

    } else if (duration == 0) {
	PJ_LOG(5,(stream->port.info.name.ptr, "Sending DTMF digit id %c", 
		  digitmap[digit->event]));
    }


    frame_out->size = 4;
}


/**
 * check_tx_rtcp()
 *
 * This function is can be called by either put_frame() or get_frame(),
 * to transmit periodic RTCP SR/RR report.
 */
static void check_tx_rtcp(pjmedia_stream *stream, pj_uint32_t timestamp)
{
    /* Note that timestamp may represent local or remote timestamp, 
     * depending on whether this function is called from put_frame()
     * or get_frame().
     */


    if (stream->rtcp_last_tx == 0) {
	
	stream->rtcp_last_tx = timestamp;

    } else if (timestamp - stream->rtcp_last_tx >= stream->rtcp_interval) {
	
	pjmedia_rtcp_pkt *rtcp_pkt;
	pj_ssize_t size;
	int len;
	pj_status_t status;

	pjmedia_rtcp_build_rtcp(&stream->rtcp, &rtcp_pkt, &len);
	size = len;
	status = pj_sock_sendto(stream->skinfo.rtcp_sock, rtcp_pkt, &size, 0,
				&stream->rem_rtcp_addr, 
				sizeof(stream->rem_rtcp_addr));
#if 0
	if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(port->info.name.ptr, "Error sending RTCP: %s [%d]",
				 errmsg, status));
	}
#endif

	stream->rtcp_last_tx = timestamp;
    }

}


/**
 * put_frame()
 *
 * This callback is called by upstream component when it has PCM frame
 * to transmit. This function encodes the PCM frame, pack it into
 * RTP packet, and transmit to peer.
 */
static pj_status_t put_frame( pjmedia_port *port, 
			      const pjmedia_frame *frame )
{
    pjmedia_stream *stream = port->user_data;
    pjmedia_channel *channel = stream->enc;
    pj_status_t status = 0;
    struct pjmedia_frame frame_out;
    int ts_len;
    pj_bool_t has_tx;
    void *rtphdr;
    int rtphdrlen;
    pj_ssize_t sent;


    /* Don't do anything if stream is paused */
    if (channel->paused)
	return PJ_SUCCESS;


    /* Number of samples in the frame */
    ts_len = frame->size / 2;

    /* Init frame_out buffer. */
    frame_out.buf = ((char*)channel->out_pkt) + sizeof(pjmedia_rtp_hdr);

    /* Make compiler happy */
    frame_out.size = 0;

    /* If we have DTMF digits in the queue, transmit the digits. 
     * Otherwise encode the PCM buffer.
     */
    if (stream->tx_dtmf_count) {

	has_tx = PJ_TRUE;
	create_dtmf_payload(stream, &frame_out);

	/* Encapsulate. */
	status = pjmedia_rtp_encode_rtp( &channel->rtp, 
					 stream->tx_event_pt, 0, 
					 frame_out.size, ts_len, 
					 (const void**)&rtphdr, 
					 &rtphdrlen);

    } else if (frame->type != PJMEDIA_FRAME_TYPE_NONE) {
	unsigned max_size;

	has_tx = PJ_TRUE;
	max_size = channel->out_pkt_size - sizeof(pjmedia_rtp_hdr);
	status = stream->codec->op->encode( stream->codec, frame, 
					    max_size, 
					    &frame_out);
	if (status != 0) {
	    LOGERR_((stream->port.info.name.ptr, 
		    "Codec encode() error", status));
	    return status;
	}

	//printf("p"); fflush(stdout);

	/* Encapsulate. */
	status = pjmedia_rtp_encode_rtp( &channel->rtp, 
					 channel->pt, 0, 
					 frame_out.size, ts_len, 
					 (const void**)&rtphdr, 
					 &rtphdrlen);
    } else {

	/* Just update RTP session's timestamp. */
	has_tx = PJ_FALSE;
	status = pjmedia_rtp_encode_rtp( &channel->rtp, 
					 0, 0, 
					 0, ts_len, 
					 (const void**)&rtphdr, 
					 &rtphdrlen);

    }

    if (status != PJ_SUCCESS) {
	LOGERR_((stream->port.info.name.ptr, 
		"RTP encode_rtp() error", status));
	return status;
    }

    /* Check if now is the time to transmit RTCP SR/RR report. 
     * We only do this when stream direction is not "decoding only", because
     * when it is, check_tx_rtcp() will be handled by get_frame().
     */
    if (stream->dir != PJMEDIA_DIR_DECODING) {
	check_tx_rtcp(stream, pj_ntohl(channel->rtp.out_hdr.ts));
    }

    /* Do nothing if we have nothing to transmit */
    if (!has_tx)
	return PJ_SUCCESS;

    if (rtphdrlen != sizeof(pjmedia_rtp_hdr)) {
	/* We don't support RTP with extended header yet. */
	PJ_TODO(SUPPORT_SENDING_RTP_WITH_EXTENDED_HEADER);
	return PJ_SUCCESS;
    }

    pj_memcpy(channel->out_pkt, rtphdr, sizeof(pjmedia_rtp_hdr));

    /* Send. */
    sent = frame_out.size+sizeof(pjmedia_rtp_hdr);
    status = pj_sock_sendto(stream->skinfo.rtp_sock, channel->out_pkt, 
			    &sent, 0, &stream->rem_rtp_addr, 
			    sizeof(stream->rem_rtp_addr));
    if (status != PJ_SUCCESS)
	return status;

    /* Update stat */
    pjmedia_rtcp_tx_rtp(&stream->rtcp, frame_out.size);

    return PJ_SUCCESS;
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
 * Handle incoming DTMF digits.
 */
static void handle_incoming_dtmf( pjmedia_stream *stream, 
				  const void *payload, unsigned payloadlen)
{
    const pjmedia_rtp_dtmf_event *event = payload;

    /* Check compiler packing. */
    pj_assert(sizeof(pjmedia_rtp_dtmf_event)==4);

    /* Must have sufficient length before we proceed. */
    if (payloadlen < sizeof(pjmedia_rtp_dtmf_event))
	return;

    //dump_bin(payload, payloadlen);

    /* Check if this is the same/current digit of the last packet. */
    if (stream->last_dtmf != -1 &&
	event->event == stream->last_dtmf &&
	pj_ntohs(event->duration) >= stream->last_dtmf_dur)
    {
	/* Yes, this is the same event. */
	stream->last_dtmf_dur = pj_ntohs(event->duration);
	return;
    }

    /* Ignore unknown event. */
    if (event->event > 15) {
	PJ_LOG(5,(stream->port.info.name.ptr, 
		  "Ignored RTP pkt with bad DTMF event %d",
    		  event->event));
	return;
    }

    /* New event! */
    PJ_LOG(5,(stream->port.info.name.ptr, "Received DTMF digit %c, vol=%d",
    	      digitmap[event->event],
    	      (event->e_vol & 0x3F)));

    stream->last_dtmf = event->event;
    stream->last_dtmf_dur = pj_ntohs(event->duration);

    /* By convention, we use jitter buffer's mutex to access shared
     * DTMF variables.
     */
    pj_mutex_lock(stream->jb_mutex);
    if (stream->rx_dtmf_count >= PJ_ARRAY_SIZE(stream->rx_dtmf_buf)) {
	/* DTMF digits overflow.  Discard the oldest digit. */
	pj_array_erase(stream->rx_dtmf_buf, sizeof(stream->rx_dtmf_buf[0]),
		       stream->rx_dtmf_count, 0);
	--stream->rx_dtmf_count;
    }
    stream->rx_dtmf_buf[stream->rx_dtmf_count++] = digitmap[event->event];
    pj_mutex_unlock(stream->jb_mutex);
}


/*
 * This callback is called by ioqueue framework on receipt of packets
 * in the RTP socket. 
 */
static void on_rx_rtp( pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read)

{
    pjmedia_stream *stream = pj_ioqueue_get_user_data(key);
    pjmedia_channel *channel = stream->dec;
    pj_status_t status;

    
    PJ_UNUSED_ARG(op_key);


    /*
     * Loop while we have packet.
     */
    do {
	const pjmedia_rtp_hdr *hdr;
	const void *payload;
	unsigned payloadlen;
	pjmedia_rtp_status seq_st;

	/* Go straight to read next packet if bytes_read == 0.
	 */
	if (bytes_read == 0)
	    goto read_next_packet;

	if (bytes_read < 0)
	    goto read_next_packet;

	/* Update RTP and RTCP session. */
	status = pjmedia_rtp_decode_rtp(&channel->rtp, 
					channel->in_pkt, bytes_read, 
					&hdr, &payload, &payloadlen);
	if (status != PJ_SUCCESS) {
	    LOGERR_((stream->port.info.name.ptr, "RTP decode error", status));
	    goto read_next_packet;
	}


	/* Inform RTCP session */
	pjmedia_rtcp_rx_rtp(&stream->rtcp, pj_ntohs(hdr->seq),
			    pj_ntohl(hdr->ts), payloadlen);

	/* Handle incoming DTMF. */
	if (hdr->pt == stream->rx_event_pt) {
	    handle_incoming_dtmf(stream, payload, payloadlen);
	    goto read_next_packet;
	}


	/* Update RTP session (also checks if RTP session can accept
	 * the incoming packet.
	 */
	pjmedia_rtp_session_update(&channel->rtp, hdr, &seq_st);
	if (seq_st.status.value) {
	    TRC_  ((stream->port.info.name.ptr, 
		    "RTP status: badpt=%d, badssrc=%d, dup=%d, "
		    "outorder=%d, probation=%d, restart=%d", 
		    seq_st.status.flag.badpt,
		    seq_st.status.flag.badssrc,
		    seq_st.status.flag.dup,
		    seq_st.status.flag.outorder,
		    seq_st.status.flag.probation,
		    seq_st.status.flag.restart));

	    if (seq_st.status.flag.badpt) {
		PJ_LOG(4,(stream->port.info.name.ptr,
			  "Bad RTP pt %d (expecting %d)",
			  hdr->pt, channel->rtp.out_pt));
	    }
	}

	/* Skip bad RTP packet */
	if (seq_st.status.flag.bad)
	    goto read_next_packet;


	/* See if source address of RTP packet is different than the 
	 * configured address.
	 */
	if ((stream->rem_rtp_addr.sin_addr.s_addr != 
	     stream->rtp_src_addr.sin_addr.s_addr) ||
	    (stream->rem_rtp_addr.sin_port != stream->rtp_src_addr.sin_port))
	{
	    stream->rtp_src_cnt++;

	    if (stream->rtp_src_cnt >= PJMEDIA_RTP_NAT_PROBATION_CNT) {
	    
		stream->rem_rtp_addr = stream->rtp_src_addr;
		stream->rtp_src_cnt = 0;

		PJ_LOG(4,(stream->port.info.name.ptr,
			  "Remote RTP address switched to %s:%d",
			  pj_inet_ntoa(stream->rtp_src_addr.sin_addr),
			  pj_ntohs(stream->rtp_src_addr.sin_port)));
	    }
	}



	/* Put "good" packet to jitter buffer, or reset the jitter buffer
	 * when RTP session is restarted.
	 */
	pj_mutex_lock( stream->jb_mutex );
	if (seq_st.status.flag.restart) {
	    status = pjmedia_jbuf_reset(stream->jb);
	    PJ_LOG(4,(stream->port.info.name.ptr, "Jitter buffer reset"));

	} else {
	    status = pjmedia_jbuf_put_frame(stream->jb, payload, payloadlen,
					    pj_ntohs(hdr->seq));
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
	    LOGERR_((stream->port.info.name.ptr, "Jitter buffer put() error", 
		    status));
	    goto read_next_packet;
	}


read_next_packet:
	bytes_read = channel->in_pkt_size;
	stream->rtp_addrlen = sizeof(stream->rtp_src_addr);
	status = pj_ioqueue_recvfrom( stream->rtp_key,
				      &stream->rtp_op_key,
				      channel->in_pkt, 
				      &bytes_read, 0,
				      &stream->rtp_src_addr, 
				      &stream->rtp_addrlen);

	if (status != PJ_SUCCESS) {
	    bytes_read = -status;
	}

    } while (status == PJ_SUCCESS ||
	     status == PJ_STATUS_FROM_OS(OSERR_ECONNRESET));

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	char errmsg[PJ_ERR_MSG_SIZE];

	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(4,(stream->port.info.name.ptr, 
		  "Error reading RTP packet: %s [status=%d]. "
		  "RTP stream thread quitting!",
		  errmsg, status));
    }
}


/*
 * This callback is called by ioqueue framework on receipt of packets
 * in the RTCP socket. 
 */
static void on_rx_rtcp( pj_ioqueue_key_t *key, 
                        pj_ioqueue_op_key_t *op_key, 
                        pj_ssize_t bytes_read)
{
    pjmedia_stream *stream = pj_ioqueue_get_user_data(key);
    pj_status_t status;

    PJ_UNUSED_ARG(op_key);

    do {
	if (bytes_read > 0) {
	    pjmedia_rtcp_rx_rtcp(&stream->rtcp, stream->rtcp_pkt, 
				 bytes_read);
	}

	bytes_read = stream->rtcp_pkt_size;
	stream->rtcp_addrlen = sizeof(stream->rem_rtcp_addr);
	status = pj_ioqueue_recvfrom( stream->rtcp_key,
				      &stream->rtcp_op_key,
				      stream->rtcp_pkt,
				      &bytes_read, 0,
				      &stream->rem_rtcp_addr,
				      &stream->rtcp_addrlen);

    } while (status == PJ_SUCCESS);

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	char errmsg[PJ_ERR_MSG_SIZE];

	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(4,(stream->port.info.name.ptr, 
		  "Error reading RTCP packet: %s [status=%d]",
		  errmsg, status));
    }

}


/*
 * Create media channel.
 */
static pj_status_t create_channel( pj_pool_t *pool,
				   pjmedia_stream *stream,
				   pjmedia_dir dir,
				   unsigned pt,
				   const pjmedia_stream_info *param,
				   const pjmedia_codec_param *codec_param,
				   pjmedia_channel **p_channel)
{
    pjmedia_channel *channel;
    pj_status_t status;
    
    /* Allocate memory for channel descriptor */

    channel = pj_pool_zalloc(pool, sizeof(pjmedia_channel));
    PJ_ASSERT_RETURN(channel != NULL, PJ_ENOMEM);

    /* Init channel info. */

    channel->stream = stream;
    channel->dir = dir;
    channel->paused = 1;
    channel->pt = pt;

    /* Allocate buffer for incoming packet. */

    channel->in_pkt_size = PJMEDIA_MAX_MTU;
    channel->in_pkt = pj_pool_alloc( pool, channel->in_pkt_size );
    PJ_ASSERT_RETURN(channel->in_pkt != NULL, PJ_ENOMEM);

    
    /* Allocate buffer for outgoing packet. */

    channel->out_pkt_size = sizeof(pjmedia_rtp_hdr) + 
			    codec_param->avg_bps/8 * 
			    PJMEDIA_MAX_FRAME_DURATION_MS / 
			    1000;

    if (channel->out_pkt_size > PJMEDIA_MAX_MTU)
	channel->out_pkt_size = PJMEDIA_MAX_MTU;

    channel->out_pkt = pj_pool_alloc(pool, channel->out_pkt_size);
    PJ_ASSERT_RETURN(channel->out_pkt != NULL, PJ_ENOMEM);


    /* Allocate buffer for decoding to PCM: */

    channel->pcm_buf_size = codec_param->clock_rate * 
			    codec_param->channel_cnt *
			    codec_param->pcm_bits_per_sample / 8 *
			    PJMEDIA_MAX_FRAME_DURATION_MS / 1000;
    channel->pcm_buf = pj_pool_alloc (pool, channel->pcm_buf_size);
    PJ_ASSERT_RETURN(channel->pcm_buf != NULL, PJ_ENOMEM);


    /* Create RTP and RTCP sessions: */

    status = pjmedia_rtp_session_init(&channel->rtp, pt, param->ssrc);
    if (status != PJ_SUCCESS)
	return status;

    /* Done. */
    *p_channel = channel;
    return PJ_SUCCESS;
}


/*
 * Create media stream.
 */
PJ_DEF(pj_status_t) pjmedia_stream_create( pjmedia_endpt *endpt,
					   pj_pool_t *pool,
					   const pjmedia_stream_info *info,
					   void *user_data,
					   pjmedia_stream **p_stream)

{
    pjmedia_stream *stream;
    pjmedia_codec_param codec_param;
    pj_ioqueue_callback ioqueue_cb;
    pj_uint16_t rtcp_port;
    unsigned jbuf_init, jbuf_max;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && info && p_stream, PJ_EINVAL);


    /* Allocate the media stream: */

    stream = pj_pool_zalloc(pool, sizeof(pjmedia_stream));
    PJ_ASSERT_RETURN(stream != NULL, PJ_ENOMEM);

    /* Init stream/port name */
    stream->port.info.name.ptr = pj_pool_alloc(pool, 24);
    pj_ansi_sprintf(stream->port.info.name.ptr,
		    "strm%p", stream);
    stream->port.info.name.slen = pj_ansi_strlen(stream->port.info.name.ptr);

    /* Init port. */
    stream->port.info.signature = ('S'<<3 | 'T'<<2 | 'R'<<1 | 'M');
    stream->port.info.type = PJMEDIA_TYPE_AUDIO;
    stream->port.info.has_info = 1;
    stream->port.info.need_info = 0;
    stream->port.info.pt = info->fmt.pt;
    pj_strdup(pool, &stream->port.info.encoding_name, &info->fmt.encoding_name);
    stream->port.info.clock_rate = info->fmt.clock_rate;
    stream->port.info.channel_count = info->fmt.channel_cnt;
    stream->port.user_data = stream;
    stream->port.put_frame = &put_frame;
    stream->port.get_frame = &get_frame;


    /* Init stream: */
    stream->endpt = endpt;
    stream->codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    stream->dir = info->dir;
    stream->user_data = user_data;
    stream->skinfo = info->sock_info;
    stream->rem_rtp_addr = info->rem_addr;
    rtcp_port = (pj_uint16_t) (pj_ntohs(info->rem_addr.sin_port)+1);
    stream->rem_rtcp_addr = stream->rem_rtp_addr;
    stream->rem_rtcp_addr.sin_port = pj_htons(rtcp_port);
    stream->rtcp_interval = (PJMEDIA_RTCP_INTERVAL + (pj_rand() % 8000)) * 
			    info->fmt.clock_rate / 1000;

    stream->tx_event_pt = info->tx_event_pt ? info->tx_event_pt : -1;
    stream->rx_event_pt = info->rx_event_pt ? info->rx_event_pt : -1;
    stream->last_dtmf = -1;


    /* Create mutex to protect jitter buffer: */

    status = pj_mutex_create_simple(pool, NULL, &stream->jb_mutex);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Create and initialize codec: */

    status = pjmedia_codec_mgr_alloc_codec( stream->codec_mgr,
					    &info->fmt, &stream->codec);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Get default codec param: */

    //status = stream->codec->op->default_attr(stream->codec, &codec_param);
    status = pjmedia_codec_mgr_get_default_param( stream->codec_mgr, 
						  &info->fmt, &codec_param);
    if (status != PJ_SUCCESS)
	goto err_cleanup;

    /* Set additional info. */
    stream->port.info.bits_per_sample = 16;
    stream->port.info.samples_per_frame = info->fmt.clock_rate*codec_param.ptime/1000;
    stream->port.info.bytes_per_frame = codec_param.avg_bps/8 * codec_param.ptime/1000;


    /* Open the codec: */

    status = stream->codec->op->open(stream->codec, &codec_param);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Get the frame size: */

    stream->frame_size = (codec_param.avg_bps / 8) * codec_param.ptime / 1000;


    /* Init RTCP session: */

    pjmedia_rtcp_init(&stream->rtcp, stream->port.info.name.ptr,
		      info->fmt.clock_rate, 
		      stream->port.info.samples_per_frame, 
		      info->ssrc);


    /* Create jitter buffer: */
    if (info->jb_init)
	jbuf_init = info->jb_init;
    else
	jbuf_init = 60 / (stream->port.info.samples_per_frame * 1000 / 
			  info->fmt.clock_rate);

    if (info->jb_max)
	jbuf_max = info->jb_max;
    else
	jbuf_max = 240 / (stream->port.info.samples_per_frame * 1000 / 
			   info->fmt.clock_rate);
    status = pjmedia_jbuf_create(pool, &stream->port.info.name,
				 stream->frame_size, 
				 jbuf_init, jbuf_max,
				 &stream->jb);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Create decoder channel: */

    status = create_channel( pool, stream, PJMEDIA_DIR_DECODING, 
			     info->fmt.pt, info, &codec_param, &stream->dec);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Create encoder channel: */

    status = create_channel( pool, stream, PJMEDIA_DIR_ENCODING, 
			     info->tx_pt, info, &codec_param, &stream->enc);
    if (status != PJ_SUCCESS)
	goto err_cleanup;

    /*  Register RTP socket to ioqueue */
    pj_memset(&ioqueue_cb, 0, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = &on_rx_rtp;

    status = pj_ioqueue_register_sock( pool, 
				       pjmedia_endpt_get_ioqueue(endpt), 
				       stream->skinfo.rtp_sock,
				       stream, &ioqueue_cb, &stream->rtp_key);
    if (status != PJ_SUCCESS)
	goto err_cleanup;

    /* Init pending operation key. */
    pj_ioqueue_op_key_init(&stream->rtp_op_key, sizeof(stream->rtp_op_key));

    /* Bootstrap the first recvfrom() operation. */
    on_rx_rtp( stream->rtp_key, &stream->rtp_op_key, 0);


    /* Register RTCP socket to ioqueue. */
    if (stream->skinfo.rtcp_sock != PJ_INVALID_SOCKET) {
	pj_memset(&ioqueue_cb, 0, sizeof(ioqueue_cb));
	ioqueue_cb.on_read_complete = &on_rx_rtcp;

	status = pj_ioqueue_register_sock( pool, 
					   pjmedia_endpt_get_ioqueue(endpt),
					   stream->skinfo.rtcp_sock,
					   stream, &ioqueue_cb, 
					   &stream->rtcp_key);
	if (status != PJ_SUCCESS)
	    goto err_cleanup;
    }

    /* Init pending operation key. */
    pj_ioqueue_op_key_init(&stream->rtcp_op_key, sizeof(stream->rtcp_op_key));

    stream->rtcp_pkt_size = sizeof(stream->rtcp_pkt);

    /* Bootstrap the first recvfrom() operation. */
    on_rx_rtcp( stream->rtcp_key, &stream->rtcp_op_key, 0);

    /* Success! */
    *p_stream = stream;

    PJ_LOG(5,(THIS_FILE, "Stream %s created", stream->port.info.name.ptr));
    return PJ_SUCCESS;


err_cleanup:
    pjmedia_stream_destroy(stream);
    return status;
}


/*
 * Destroy stream.
 */
PJ_DEF(pj_status_t) pjmedia_stream_destroy( pjmedia_stream *stream )
{

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);


    /* This function may be called when stream is partly initialized. */
    if (stream->jb_mutex)
	pj_mutex_lock(stream->jb_mutex);


    /* Unregister from ioqueue. */
    if (stream->rtp_key) {
	pj_ioqueue_unregister(stream->rtp_key);
	stream->rtp_key = NULL;
    }
    if (stream->rtcp_key) {
	pj_ioqueue_unregister(stream->rtcp_key);
	stream->rtcp_key = NULL;
    }

    /* Free codec. */

    if (stream->codec) {
	stream->codec->op->close(stream->codec);
	pjmedia_codec_mgr_dealloc_codec(stream->codec_mgr, stream->codec);
	stream->codec = NULL;
    }

    /* Free mutex */
    
    if (stream->jb_mutex) {
	pj_mutex_destroy(stream->jb_mutex);
	stream->jb_mutex = NULL;
    }

    return PJ_SUCCESS;
}



/*
 * Get the port interface.
 */
PJ_DEF(pj_status_t) pjmedia_stream_get_port( pjmedia_stream *stream,
					     pjmedia_port **p_port )
{
    *p_port = &stream->port;
    return PJ_SUCCESS;
}


/*
 * Start stream.
 */
PJ_DEF(pj_status_t) pjmedia_stream_start(pjmedia_stream *stream)
{

    PJ_ASSERT_RETURN(stream && stream->enc && stream->dec, PJ_EINVALIDOP);

    if (stream->enc && (stream->dir & PJMEDIA_DIR_ENCODING)) {
	stream->enc->paused = 0;
	//pjmedia_snd_stream_start(stream->enc->snd_stream);
	PJ_LOG(4,(stream->port.info.name.ptr, "Encoder stream started"));
    } else {
	PJ_LOG(4,(stream->port.info.name.ptr, "Encoder stream paused"));
    }

    if (stream->dec && (stream->dir & PJMEDIA_DIR_DECODING)) {
	stream->dec->paused = 0;
	//pjmedia_snd_stream_start(stream->dec->snd_stream);
	PJ_LOG(4,(stream->port.info.name.ptr, "Decoder stream started"));
    } else {
	PJ_LOG(4,(stream->port.info.name.ptr, "Decoder stream paused"));
    }

    return PJ_SUCCESS;
}


/*
 * Get stream statistics.
 */
PJ_DEF(pj_status_t) pjmedia_stream_get_stat( const pjmedia_stream *stream,
					     pjmedia_rtcp_stat *stat)
{
    PJ_ASSERT_RETURN(stream && stat, PJ_EINVAL);

    pj_memcpy(stat, &stream->rtcp.stat, sizeof(pjmedia_rtcp_stat));
    return PJ_SUCCESS;
}


/*
 * Pause stream.
 */
PJ_DEF(pj_status_t) pjmedia_stream_pause( pjmedia_stream *stream,
					  pjmedia_dir dir)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if ((dir & PJMEDIA_DIR_ENCODING) && stream->enc) {
	stream->enc->paused = 1;
	PJ_LOG(4,(stream->port.info.name.ptr, "Encoder stream paused"));
    }

    if ((dir & PJMEDIA_DIR_DECODING) && stream->dec) {
	stream->dec->paused = 1;
	PJ_LOG(4,(stream->port.info.name.ptr, "Decoder stream paused"));
    }

    return PJ_SUCCESS;
}


/*
 * Resume stream
 */
PJ_DEF(pj_status_t) pjmedia_stream_resume( pjmedia_stream *stream,
					   pjmedia_dir dir)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if ((dir & PJMEDIA_DIR_ENCODING) && stream->enc) {
	stream->enc->paused = 1;
	PJ_LOG(4,(stream->port.info.name.ptr, "Encoder stream resumed"));
    }

    if ((dir & PJMEDIA_DIR_DECODING) && stream->dec) {
	stream->dec->paused = 1;
	PJ_LOG(4,(stream->port.info.name.ptr, "Decoder stream resumed"));
    }

    return PJ_SUCCESS;
}

/*
 * Dial DTMF
 */
PJ_DEF(pj_status_t) pjmedia_stream_dial_dtmf( pjmedia_stream *stream,
					      const pj_str_t *digit_char)
{
    pj_status_t status = PJ_SUCCESS;

    /* By convention we use jitter buffer mutex to access DTMF
     * queue.
     */
    PJ_ASSERT_RETURN(stream && digit_char, PJ_EINVAL);

    /* Check that remote can receive DTMF events. */
    if (stream->tx_event_pt < 0) {
	return PJMEDIA_RTP_EREMNORFC2833;
    }
    
    pj_mutex_lock(stream->jb_mutex);
    
    if (stream->tx_dtmf_count+digit_char->slen >=
	PJ_ARRAY_SIZE(stream->tx_dtmf_buf))
    {
	status = PJ_ETOOMANY;
    } else {
	int i;

	/* convert ASCII digits into payload type first, to make sure
	 * that all digits are valid. 
	 */
	for (i=0; i<digit_char->slen; ++i) {
	    unsigned pt;

	    if (digit_char->ptr[i] >= '0' &&
		digit_char->ptr[i] <= '9')
	    {
		pt = digit_char->ptr[i] - '0';
	    } 
	    else if (pj_tolower(digit_char->ptr[i]) >= 'a' &&
		     pj_tolower(digit_char->ptr[i]) <= 'd')
	    {
		pt = pj_tolower(digit_char->ptr[i]) - 'a' + 12;
	    }
	    else if (digit_char->ptr[i] == '*')
	    {
		pt = 10;
	    }
	    else if (digit_char->ptr[i] == '#')
	    {
		pt = 11;
	    }
	    else
	    {
		status = PJMEDIA_RTP_EINDTMF;
		break;
	    }

	    stream->tx_dtmf_buf[stream->tx_dtmf_count+i].event = pt;
	}

	if (status != PJ_SUCCESS)
	    goto on_return;

	/* Init start_ts and end_ts only for the first digit.
	 * Subsequent digits are initialized on the fly.
	 */
	if (stream->tx_dtmf_count ==0) {
	    pj_uint32_t start_ts;

	    start_ts = pj_ntohl(stream->enc->rtp.out_hdr.ts);
	    stream->tx_dtmf_buf[0].start_ts = start_ts;
	}

	/* Increment digit count only if all digits are valid. */
	stream->tx_dtmf_count += digit_char->slen;

    }

on_return:
    pj_mutex_unlock(stream->jb_mutex);

    return status;
}


/*
 * See if we have DTMF digits in the rx buffer.
 */
PJ_DEF(pj_bool_t) pjmedia_stream_check_dtmf(pjmedia_stream *stream)
{
    return stream->rx_dtmf_count != 0;
}


/*
 * Retrieve incoming DTMF digits from the stream's DTMF buffer.
 */
PJ_DEF(pj_status_t) pjmedia_stream_get_dtmf( pjmedia_stream *stream,
					     char *digits,
					     unsigned *size)
{
    PJ_ASSERT_RETURN(stream && digits && size, PJ_EINVAL);

    pj_assert(sizeof(stream->rx_dtmf_buf[0]) == 0);

    /* By convention, we use jitter buffer's mutex to access DTMF
     * digits resources.
     */
    pj_mutex_lock(stream->jb_mutex);

    if (stream->rx_dtmf_count < *size)
	*size = stream->rx_dtmf_count;

    if (*size) {
	pj_memcpy(digits, stream->rx_dtmf_buf, *size);
	stream->rx_dtmf_count -= *size;
	if (stream->rx_dtmf_count) {
	    pj_memmove(stream->rx_dtmf_buf,
		       &stream->rx_dtmf_buf[*size],
		       stream->rx_dtmf_count);
	}
    }

    pj_mutex_unlock(stream->jb_mutex);

    return PJ_SUCCESS;
}

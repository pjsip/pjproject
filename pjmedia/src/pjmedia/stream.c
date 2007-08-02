/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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

#define BYTES_PER_SAMPLE		2

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

    pjmedia_transport	    *transport;	    /**< Stream transport.	    */

    pjmedia_codec	    *codec;	    /**< Codec instance being used. */
    pjmedia_codec_param	     codec_param;   /**< Codec param.		    */
    pj_int16_t		    *enc_buf;	    /**< Encoding buffer, when enc's
						 ptime is different than dec.
						 Otherwise it's NULL.	    */

    unsigned		     enc_samples_per_frame;
    unsigned		     enc_buf_size;  /**< Encoding buffer size, in
						 samples.		    */
    unsigned		     enc_buf_pos;   /**< First position in buf.	    */
    unsigned		     enc_buf_count; /**< Number of samples in the
						 encoding buffer.	    */

    unsigned		     vad_enabled;   /**< VAD enabled in param.	    */
    unsigned		     frame_size;    /**< Size of encoded base frame.*/
    pj_bool_t		     is_streaming;  /**< Currently streaming?. This
						 is used to put RTP marker
						 bit.			    */
    pj_uint32_t		     ts_vad_disabled;/**< TS when VAD was disabled. */
    pj_uint32_t		     tx_duration;   /**< TX duration in timestamp.  */

    pj_mutex_t		    *jb_mutex;
    pjmedia_jbuf	    *jb;	    /**< Jitter buffer.		    */
    char		     jb_last_frm;   /**< Last frame type from jb    */

    pjmedia_rtcp_session     rtcp;	    /**< RTCP for incoming RTP.	    */

    pj_uint32_t		     rtcp_last_tx;  /**< RTCP tx time in timestamp  */
    pj_uint32_t		     rtcp_interval; /**< Interval, in timestamp.    */

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

    /* DTMF callback */
    void		    (*dtmf_cb)(pjmedia_stream*, void*, int);
    void		     *dtmf_cb_user_data;
};


/* RFC 2833 digit */
static const char digitmap[16] = { '0', '1', '2', '3', 
				   '4', '5', '6', '7', 
				   '8', '9', '*', '#',
				   'A', 'B', 'C', 'D'};

/* Zero PCM frame */
#define ZERO_PCM_MAX_SIZE   1920    /* 40ms worth of PCM @ 48KHz */
static pj_int16_t zero_frame[ZERO_PCM_MAX_SIZE];


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
    pjmedia_stream *stream = (pjmedia_stream*) port->port_data.pdata;
    pjmedia_channel *channel = stream->dec;
    unsigned samples_count, samples_per_frame, samples_required;
    pj_int16_t *p_out_samp;
    pj_status_t status;


    /* Return no frame is channel is paused */
    if (channel->paused) {
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    /* Repeat get frame from the jitter buffer and decode the frame
     * until we have enough frames according to codec's ptime.
     */

    /* Lock jitter buffer mutex first */
    pj_mutex_lock( stream->jb_mutex );

    samples_required = stream->port.info.samples_per_frame;
    samples_per_frame = stream->codec_param.info.frm_ptime *
			stream->codec_param.info.clock_rate *
			stream->codec_param.info.channel_cnt / 
			1000;
    p_out_samp = (pj_int16_t*) frame->buf;

    for (samples_count=0; samples_count < samples_required; 
	 samples_count += samples_per_frame) 
    {
	char frame_type;

	/* Get frame from jitter buffer. */
	pjmedia_jbuf_get_frame(stream->jb, channel->out_pkt, &frame_type);
	
	if (frame_type == PJMEDIA_JB_MISSING_FRAME) {
	    
	    /* Activate PLC */
	    if (stream->codec->op->recover && 
		stream->codec_param.setting.plc) 
	    {
		pjmedia_frame frame_out;

		frame_out.buf = p_out_samp + samples_count;
		frame_out.size = frame->size - samples_count*2;
		status = (*stream->codec->op->recover)(stream->codec,
						       frame_out.size,
						       &frame_out);

	    } else {
		status = -1;
	    }

	    if (status != PJ_SUCCESS) {
		/* Either PLC failed or PLC not supported/enabled */
		pjmedia_zero_samples(p_out_samp + samples_count,
				     samples_required - samples_count);
		PJ_LOG(5,(stream->port.info.name.ptr,  "Frame lost!"));

	    } else {
		PJ_LOG(5,(stream->port.info.name.ptr, 
			  "Lost frame recovered"));
	    }
	    
	} else if (frame_type == PJMEDIA_JB_ZERO_EMPTY_FRAME) {

	    /* Jitter buffer is empty. If this is the first "empty" state,
	     * activate PLC to smoothen the fade-out, otherwise zero
	     * the frame. 
	     */
	    if (frame_type != stream->jb_last_frm) {
		pjmedia_jb_state jb_state;

		/* Activate PLC to smoothen the missing frame */
		if (stream->codec->op->recover && 
		    stream->codec_param.setting.plc) 
		{
		    pjmedia_frame frame_out;

		    do {
			frame_out.buf = p_out_samp + samples_count;
			frame_out.size = frame->size - samples_count*2;
			status = (*stream->codec->op->recover)(stream->codec,
							       frame_out.size,
							       &frame_out);
			if (status != PJ_SUCCESS)
			    break;
			samples_count += samples_per_frame;

		    } while (samples_count < samples_required);

		} 

		/* Report the state of jitter buffer */
		pjmedia_jbuf_get_state(stream->jb, &jb_state);
		PJ_LOG(5,(stream->port.info.name.ptr, 
			  "Jitter buffer empty (prefetch=%d)", 
			  jb_state.prefetch));

	    }

	    if (samples_count < samples_required) {
		pjmedia_zero_samples(p_out_samp + samples_count,
				     samples_required - samples_count);
		samples_count = samples_required;
	    }

	    stream->jb_last_frm = frame_type;
	    break;

	} else if (frame_type != PJMEDIA_JB_NORMAL_FRAME) {

	    pjmedia_jb_state jb_state;

	    /* It can only be PJMEDIA_JB_ZERO_PREFETCH frame */
	    pj_assert(frame_type == PJMEDIA_JB_ZERO_PREFETCH_FRAME);

	    /* Get the state of jitter buffer */
	    pjmedia_jbuf_get_state(stream->jb, &jb_state);

	    /* Always activate PLC when it's available.. */
	    if (stream->codec->op->recover && 
		stream->codec_param.setting.plc) 
	    {
		pjmedia_frame frame_out;

		do {
		    frame_out.buf = p_out_samp + samples_count;
		    frame_out.size = frame->size - samples_count*2;
		    status = (*stream->codec->op->recover)(stream->codec,
							   frame_out.size,
							   &frame_out);
		    if (status != PJ_SUCCESS)
			break;
		    samples_count += samples_per_frame;

		} while (samples_count < samples_required);

		if (stream->jb_last_frm != frame_type) {
		    PJ_LOG(5,(stream->port.info.name.ptr, 
			      "Jitter buffer is bufferring with plc (prefetch=%d)",
			      jb_state.prefetch));
		}

	    } 

	    if (samples_count < samples_required) {
		pjmedia_zero_samples(p_out_samp + samples_count,
				     samples_required - samples_count);
		samples_count = samples_required;
		PJ_LOG(5,(stream->port.info.name.ptr, 
			  "Jitter buffer is bufferring (prefetch=%d)..", 
			  jb_state.prefetch));
	    }

	    stream->jb_last_frm = frame_type;
	    break;

	} else {
	    /* Got "NORMAL" frame from jitter buffer */
	    pjmedia_frame frame_in, frame_out;

	    /* Decode */
	    frame_in.buf = channel->out_pkt;
	    frame_in.size = stream->frame_size;
	    frame_in.type = PJMEDIA_FRAME_TYPE_AUDIO;  /* ignored */

	    frame_out.buf = p_out_samp + samples_count;
	    frame_out.size = frame->size - samples_count*BYTES_PER_SAMPLE;
	    status = stream->codec->op->decode( stream->codec, &frame_in,
						frame_out.size, &frame_out);
	    if (status != 0) {
		LOGERR_((port->info.name.ptr, "codec decode() error", 
			 status));

		pjmedia_zero_samples(p_out_samp + samples_count, 
				     samples_per_frame);
	    }
	}

	stream->jb_last_frm = frame_type;
    }


    /* Unlock jitter buffer mutex. */
    pj_mutex_unlock( stream->jb_mutex );

    /* Return PJMEDIA_FRAME_TYPE_NONE if we have no frames at all
     * (it can happen when jitter buffer returns PJMEDIA_JB_ZERO_EMPTY_FRAME).
     */
    if (samples_count == 0) {
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	frame->size = 0;
    } else {
	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame->size = samples_count * BYTES_PER_SAMPLE;
	frame->timestamp.u64 = 0;
    }

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

    event = (pjmedia_rtp_dtmf_event*) frame_out->buf;
    cur_ts = pj_ntohl(stream->enc->rtp.out_hdr.ts);
    duration = cur_ts - digit->start_ts;

    if (duration == 0) {
	PJ_LOG(5,(stream->port.info.name.ptr, "Sending DTMF digit id %c", 
		  digitmap[digit->event]));
	duration = stream->port.info.samples_per_frame;
	digit->start_ts = cur_ts - duration;
    }

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

	if (stream->tx_dtmf_count) {
	    PJ_LOG(5,(stream->port.info.name.ptr,
		      "Sending DTMF digit id %c", 
		      digitmap[stream->tx_dtmf_buf[0].event]));
	}

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
	int len;

	pjmedia_rtcp_build_rtcp(&stream->rtcp, &rtcp_pkt, &len);

	(*stream->transport->op->send_rtcp)(stream->transport, 
					    rtcp_pkt, len);

	stream->rtcp_last_tx = timestamp;
    }

}


/**
 * Rebuffer the frame when encoder and decoder has different ptime
 * (such as when different iLBC modes are used by local and remote)
 */
static void rebuffer(pjmedia_stream *stream,
		     pjmedia_frame *frame)
{
    /* How many samples are needed */
    unsigned count;

    /* Normalize frame */
    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO)
	frame->size = 0;

    /* Remove used frame from the buffer. */
    if (stream->enc_buf_pos) {
	if (stream->enc_buf_count) {
	    pj_memmove(stream->enc_buf,
		       stream->enc_buf + stream->enc_buf_pos,
		       (stream->enc_buf_count << 1));
	}
	stream->enc_buf_pos = 0;
    }

    /* Make sure we have space to store the new frame */
    pj_assert(stream->enc_buf_count + (frame->size >> 1) <
		stream->enc_buf_size);

    /* Append new frame to the buffer */
    if (frame->size) {
	pj_memcpy(stream->enc_buf + stream->enc_buf_count,
		  frame->buf, frame->size);
	stream->enc_buf_count += (frame->size >> 1);
    }

    /* How many samples are needed */
    count = stream->codec_param.info.enc_ptime * 
		stream->port.info.clock_rate / 1000;

    /* See if we have enough samples */
    if (stream->enc_buf_count >= count) {

	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame->buf = stream->enc_buf;
	frame->size = (count << 1);

	stream->enc_buf_pos = count;
	stream->enc_buf_count -= count;

    } else {
	/* We don't have enough samples */
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
    }
}


/**
 * put_frame_imp()
 */
static pj_status_t put_frame_imp( pjmedia_port *port, 
				  const pjmedia_frame *frame )
{
    pjmedia_stream *stream = (pjmedia_stream*) port->port_data.pdata;
    pjmedia_channel *channel = stream->enc;
    pj_status_t status = 0;
    pjmedia_frame frame_out;
    unsigned ts_len, samples_per_frame;
    void *rtphdr;
    int rtphdrlen;


    /* Don't do anything if stream is paused */
    if (channel->paused) {
	stream->enc_buf_pos = stream->enc_buf_count = 0;
	return PJ_SUCCESS;
    }

    /* Number of samples in the frame */
    if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO)
	ts_len = (frame->size >> 1);
    else
	ts_len = 0;

    /* Increment transmit duration */
    stream->tx_duration += ts_len;

    /* Init frame_out buffer. */
    frame_out.buf = ((char*)channel->out_pkt) + sizeof(pjmedia_rtp_hdr);
    frame_out.size = 0;

    /* Calculate number of samples per frame */
    samples_per_frame = stream->enc_samples_per_frame;


    /* If we have DTMF digits in the queue, transmit the digits. 
     * Otherwise encode the PCM buffer.
     */
    if (stream->tx_dtmf_count) {

	create_dtmf_payload(stream, &frame_out);

	/* Encapsulate. */
	status = pjmedia_rtp_encode_rtp( &channel->rtp, 
					 stream->tx_event_pt, 0, 
					 frame_out.size, ts_len, 
					 (const void**)&rtphdr, 
					 &rtphdrlen);

    } else if (frame->type != PJMEDIA_FRAME_TYPE_NONE) {
	unsigned ts, codec_samples_per_frame;

	/* Repeatedly call encode if there are multiple frames to be
	 * sent.
	 */
	codec_samples_per_frame = stream->codec_param.info.enc_ptime *
				  stream->codec_param.info.clock_rate /
				  1000;
	if (codec_samples_per_frame == 0) {
	    codec_samples_per_frame = stream->codec_param.info.frm_ptime *
				      stream->codec_param.info.clock_rate /
				      1000;
	}

	for (ts=0; ts<ts_len; ts += codec_samples_per_frame) {
	    pjmedia_frame tmp_out_frame, tmp_in_frame;
	    unsigned bytes_per_sample, max_size;

	    /* Nb of bytes in PCM sample */
	    bytes_per_sample = stream->codec_param.info.pcm_bits_per_sample/8;

	    /* Split original PCM input frame into base frame size */
	    tmp_in_frame.timestamp.u64 = frame->timestamp.u64 + ts;
	    tmp_in_frame.buf = ((char*)frame->buf) + ts * bytes_per_sample;
	    tmp_in_frame.size = codec_samples_per_frame * bytes_per_sample;
	    tmp_in_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;

	    /* Set output frame position */
	    tmp_out_frame.buf = ((char*)frame_out.buf) + frame_out.size;

	    max_size = channel->out_pkt_size - sizeof(pjmedia_rtp_hdr) -
		       frame_out.size;

	    /* Encode! */
	    status = stream->codec->op->encode( stream->codec, &tmp_in_frame, 
						max_size, &tmp_out_frame);
	    if (status != PJ_SUCCESS) {
		LOGERR_((stream->port.info.name.ptr, 
			"Codec encode() error", status));
		return status;
	    }

	    /* tmp_out_frame.size may be zero for silence frame. */
	    frame_out.size += tmp_out_frame.size;

	    /* Stop processing next PCM frame when encode() returns either 
	     * CNG frame or NULL frame.
	     */
	    if (tmp_out_frame.type!=PJMEDIA_FRAME_TYPE_AUDIO || 
		tmp_out_frame.size==0) 
	    {
		break;
	    }

	}

	/* Encapsulate. */
	status = pjmedia_rtp_encode_rtp( &channel->rtp, 
					 channel->pt, 0, 
					 frame_out.size, ts_len, 
					 (const void**)&rtphdr, 
					 &rtphdrlen);
    } else {

	/* Just update RTP session's timestamp. */
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
    if (frame_out.size == 0) {
	if (stream->is_streaming) {
	    PJ_LOG(5,(stream->port.info.name.ptr,"Starting silence"));
	    stream->is_streaming = PJ_FALSE;
	}

	return PJ_SUCCESS;
    }


    /* Copy RTP header to the beginning of packet */
    pj_memcpy(channel->out_pkt, rtphdr, sizeof(pjmedia_rtp_hdr));


    /* Set RTP marker bit if currently not streaming */
    if (stream->is_streaming == PJ_FALSE) {
	pjmedia_rtp_hdr *rtp = (pjmedia_rtp_hdr*) channel->out_pkt;

	rtp->m = 1;
	PJ_LOG(5,(stream->port.info.name.ptr,"Start talksprut.."));
    }

    stream->is_streaming = PJ_TRUE;

    /* Send the RTP packet to the transport. */
    (*stream->transport->op->send_rtp)(stream->transport,
				       channel->out_pkt, 
				       frame_out.size + 
					    sizeof(pjmedia_rtp_hdr));


    /* Update stat */
    pjmedia_rtcp_tx_rtp(&stream->rtcp, frame_out.size);

    return PJ_SUCCESS;
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
    pjmedia_stream *stream = (pjmedia_stream*) port->port_data.pdata;
    pjmedia_frame tmp_zero_frame;
    unsigned samples_per_frame;

    samples_per_frame = stream->enc_samples_per_frame;

    /* http://www.pjsip.org/trac/ticket/56:
     *  when input is PJMEDIA_FRAME_TYPE_NONE, feed zero PCM frame
     *  instead so that encoder can decide whether or not to transmit
     *  silence frame.
     */
    if (frame->type == PJMEDIA_FRAME_TYPE_NONE &&
	samples_per_frame <= ZERO_PCM_MAX_SIZE) 
    {
	pj_memcpy(&tmp_zero_frame, frame, sizeof(pjmedia_frame));
	frame = &tmp_zero_frame;

	tmp_zero_frame.buf = zero_frame;
	tmp_zero_frame.size = samples_per_frame * 2;
	tmp_zero_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    }

#if 0
    // This is no longer needed because each TYPE_NONE frame will
    // be converted into zero frame above

    /* If VAD is temporarily disabled during creation, feed zero PCM frame
     * to the codec.
     */
    if (stream->vad_enabled != stream->codec_param.setting.vad &&
	stream->vad_enabled != 0 &&
	frame->type == PJMEDIA_FRAME_TYPE_NONE &&
	samples_per_frame <= ZERO_PCM_MAX_SIZE)
    {
	pj_memcpy(&tmp_in_frame, frame, sizeof(pjmedia_frame));
	frame = &tmp_in_frame;

	tmp_in_frame.buf = zero_frame;
	tmp_in_frame.size = samples_per_frame * 2;
	tmp_in_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    }
#endif

    /* If VAD is temporarily disabled during creation, enable it
     * after transmitting for VAD_SUSPEND_SEC seconds.
     */
    if (stream->vad_enabled != stream->codec_param.setting.vad &&
	(stream->tx_duration - stream->ts_vad_disabled) > 
	stream->port.info.clock_rate * PJMEDIA_STREAM_VAD_SUSPEND_MSEC / 1000)
    {
	stream->codec_param.setting.vad = stream->vad_enabled;
	stream->codec->op->modify(stream->codec, &stream->codec_param);
	PJ_LOG(4,(stream->port.info.name.ptr,"VAD re-enabled"));
    }


    /* If encoder has different ptime than decoder, then the frame must
     * be passed through the encoding buffer via rebuffer() function.
     */
    if (stream->enc_buf != NULL) {
	pjmedia_frame tmp_rebuffer_frame;
	pj_status_t status = PJ_SUCCESS;

	/* Copy original frame to temporary frame since we need 
	 * to modify it.
	 */
	pj_memcpy(&tmp_rebuffer_frame, frame, sizeof(pjmedia_frame));

	/* Loop while we have full frame in enc_buffer */
	for (;;) {
	    pj_status_t st;

	    /* Run rebuffer() */
	    rebuffer(stream, &tmp_rebuffer_frame);

	    /* Process this frame */
	    st = put_frame_imp(port, &tmp_rebuffer_frame);
	    if (st != PJ_SUCCESS)
		status = st;

	    /* If we still have full frame in the buffer, re-run
	     * rebuffer() with NULL frame.
	     */
	    if (stream->enc_buf_count >= stream->enc_samples_per_frame) {

		tmp_rebuffer_frame.type = PJMEDIA_FRAME_TYPE_NONE;

	    } else {

		/* Otherwise break */
		break;
	    }
	}

	return status;

    } else {
	return put_frame_imp(port, frame);
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
 * Handle incoming DTMF digits.
 */
static void handle_incoming_dtmf( pjmedia_stream *stream, 
				  const void *payload, unsigned payloadlen)
{
    pjmedia_rtp_dtmf_event *event = (pjmedia_rtp_dtmf_event*) payload;

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

    /* If DTMF callback is installed, call the callback, otherwise keep
     * the DTMF digits in the buffer.
     */
    if (stream->dtmf_cb) {

	stream->dtmf_cb(stream, stream->dtmf_cb_user_data, 
			digitmap[event->event]);

    } else {
	/* By convention, we use jitter buffer's mutex to access shared
	 * DTMF variables.
	 */
	pj_mutex_lock(stream->jb_mutex);
	if (stream->rx_dtmf_count >= PJ_ARRAY_SIZE(stream->rx_dtmf_buf)) {
	    /* DTMF digits overflow.  Discard the oldest digit. */
	    pj_array_erase(stream->rx_dtmf_buf, 
			   sizeof(stream->rx_dtmf_buf[0]),
			   stream->rx_dtmf_count, 0);
	    --stream->rx_dtmf_count;
	}
	stream->rx_dtmf_buf[stream->rx_dtmf_count++] = digitmap[event->event];
	pj_mutex_unlock(stream->jb_mutex);
    }
}


/*
 * This callback is called by stream transport on receipt of packets
 * in the RTP socket. 
 */
static void on_rx_rtp( void *data, 
		       const void *pkt,
                       pj_ssize_t bytes_read)

{
    pjmedia_stream *stream = (pjmedia_stream*) data;
    pjmedia_channel *channel = stream->dec;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;
    pjmedia_rtp_status seq_st;
    pj_status_t status;


    /* Check for errors */
    if (bytes_read < 0) {
	LOGERR_((stream->port.info.name.ptr, "RTP recv() error", -bytes_read));
	return;
    }

    /* Ignore keep-alive packets */
    if (bytes_read < (pj_ssize_t) sizeof(pjmedia_rtp_hdr))
	return;

    /* Update RTP and RTCP session. */
    status = pjmedia_rtp_decode_rtp(&channel->rtp, pkt, bytes_read,
				    &hdr, &payload, &payloadlen);
    if (status != PJ_SUCCESS) {
	LOGERR_((stream->port.info.name.ptr, "RTP decode error", status));
	stream->rtcp.stat.rx.discard++;
	return;
    }


    /* Inform RTCP session */
    pjmedia_rtcp_rx_rtp(&stream->rtcp, pj_ntohs(hdr->seq),
			pj_ntohl(hdr->ts), payloadlen);

    /* Ignore the packet if decoder is paused */
    if (channel->paused)
	return;

    /* Handle incoming DTMF. */
    if (hdr->pt == stream->rx_event_pt) {
	handle_incoming_dtmf(stream, payload, payloadlen);
	return;
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
	return;


    /* Put "good" packet to jitter buffer, or reset the jitter buffer
     * when RTP session is restarted.
     */
    pj_mutex_lock( stream->jb_mutex );
    if (seq_st.status.flag.restart) {
	status = pjmedia_jbuf_reset(stream->jb);
	PJ_LOG(4,(stream->port.info.name.ptr, "Jitter buffer reset"));

    } else {
	/*
	 * Packets may contain more than one frames, while the jitter
	 * buffer can only take one frame per "put" operation. So we need
	 * to ask the codec to "parse" the payload into multiple frames.
	 */
	enum { MAX = 16 };
	pj_timestamp ts;
	unsigned i, count = MAX;
	unsigned samples_per_frame;
	pjmedia_frame frames[MAX];

	/* Get the timestamp of the first sample */
	ts.u64 = pj_ntohl(hdr->ts);

	/* Parse the payload. */
	status = (*stream->codec->op->parse)(stream->codec,
					     (void*)payload,
					     payloadlen,
					     &ts,
					     &count,
					     frames);
	if (status != PJ_SUCCESS) {
	    LOGERR_((stream->port.info.name.ptr, 
		     "Codec parse() error", 
		     status));
	    count = 0;
	}

	/* Put each frame to jitter buffer. */
	samples_per_frame = stream->codec_param.info.frm_ptime * 
			    stream->codec_param.info.clock_rate *
			    stream->codec_param.info.channel_cnt /
			    1000;
			    
	for (i=0; i<count; ++i) {
	    unsigned ext_seq;

	    ext_seq = (unsigned)(frames[i].timestamp.u64 /
				 samples_per_frame);
	    pjmedia_jbuf_put_frame(stream->jb, frames[i].buf, 
				   frames[i].size, ext_seq);

	}
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
	return;
    }
}


/*
 * This callback is called by stream transport on receipt of packets
 * in the RTCP socket. 
 */
static void on_rx_rtcp( void *data,
                        const void *pkt, 
                        pj_ssize_t bytes_read)
{
    pjmedia_stream *stream = (pjmedia_stream*) data;

    /* Check for errors */
    if (bytes_read < 0) {
	LOGERR_((stream->port.info.name.ptr, "RTCP recv() error", 
		 -bytes_read));
	return;
    }

    pjmedia_rtcp_rx_rtcp(&stream->rtcp, pkt, bytes_read);
}


/*
 * Create media channel.
 */
static pj_status_t create_channel( pj_pool_t *pool,
				   pjmedia_stream *stream,
				   pjmedia_dir dir,
				   unsigned pt,
				   const pjmedia_stream_info *param,
				   pjmedia_channel **p_channel)
{
    pjmedia_channel *channel;
    pj_status_t status;
    
    /* Allocate memory for channel descriptor */

    channel = PJ_POOL_ZALLOC_T(pool, pjmedia_channel);
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
			    stream->codec_param.info.avg_bps/8 * 
			    PJMEDIA_MAX_FRAME_DURATION_MS / 
			    1000;

    if (channel->out_pkt_size > PJMEDIA_MAX_MTU)
	channel->out_pkt_size = PJMEDIA_MAX_MTU;

    channel->out_pkt = pj_pool_alloc(pool, channel->out_pkt_size);
    PJ_ASSERT_RETURN(channel->out_pkt != NULL, PJ_ENOMEM);



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
					   pjmedia_transport *tp,
					   void *user_data,
					   pjmedia_stream **p_stream)

{
    enum { M = 32 };
    pjmedia_stream *stream;
    pj_str_t name;
    unsigned jb_init, jb_max, jb_min_pre, jb_max_pre;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && info && p_stream, PJ_EINVAL);


    /* Allocate the media stream: */

    stream = PJ_POOL_ZALLOC_T(pool, pjmedia_stream);
    PJ_ASSERT_RETURN(stream != NULL, PJ_ENOMEM);

    /* Init stream/port name */
    name.ptr = (char*) pj_pool_alloc(pool, M);
    name.slen = pj_ansi_snprintf(name.ptr, M, "strm%p", stream);


    /* Init some port-info. Some parts of the info will be set later
     * once we have more info about the codec.
     */
    pjmedia_port_info_init(&stream->port.info, &name,
			   PJMEDIA_PORT_SIGNATURE('S', 'T', 'R', 'M'),
			   info->fmt.clock_rate, info->fmt.channel_cnt,
			   16, 80);

    /* Init port. */

    pj_strdup(pool, &stream->port.info.encoding_name, &info->fmt.encoding_name);
    stream->port.info.clock_rate = info->fmt.clock_rate;
    stream->port.info.channel_count = info->fmt.channel_cnt;
    stream->port.port_data.pdata = stream;
    stream->port.put_frame = &put_frame;
    stream->port.get_frame = &get_frame;


    /* Init stream: */
    stream->endpt = endpt;
    stream->codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    stream->dir = info->dir;
    stream->user_data = user_data;
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


    /* Get codec param: */
    if (info->param)
	stream->codec_param = *info->param;
    else {
	status = pjmedia_codec_mgr_get_default_param(stream->codec_mgr, 
						     &info->fmt, 
						     &stream->codec_param);
	if (status != PJ_SUCCESS)
	    goto err_cleanup;
    }

    /* Check for invalid frame per packet. */
    if (stream->codec_param.setting.frm_per_pkt < 1)
	stream->codec_param.setting.frm_per_pkt = 1;

    /* Set additional info. */
    stream->port.info.bits_per_sample = 16;
    stream->port.info.samples_per_frame = info->fmt.clock_rate * 
					  stream->codec_param.info.frm_ptime *
					  stream->codec_param.setting.frm_per_pkt /
					  1000;
    stream->port.info.bytes_per_frame = stream->codec_param.info.avg_bps/8 * 
					stream->codec_param.info.frm_ptime *
					stream->codec_param.setting.frm_per_pkt /
					1000;


    /* Open the codec: */

    status = stream->codec->op->open(stream->codec, &stream->codec_param);
    if (status != PJ_SUCCESS)
	goto err_cleanup;

    /* If encoder and decoder's ptime are asymmetric, then we need to
     * create buffer on the encoder side. This could happen for example
     * with iLBC 
     */
    if (stream->codec_param.info.enc_ptime!=0 &&
	stream->codec_param.info.enc_ptime!=stream->codec_param.info.frm_ptime)
    {
	unsigned ptime;

	stream->enc_samples_per_frame = stream->codec_param.info.enc_ptime *
					stream->port.info.clock_rate / 1000;

	/* Set buffer size as twice the largest ptime value between
	 * stream's ptime, encoder ptime, or decoder ptime.
	 */

	ptime = stream->port.info.samples_per_frame * 1000 /
		stream->port.info.clock_rate;

	if (stream->codec_param.info.enc_ptime > ptime)
	    ptime = stream->codec_param.info.enc_ptime;

	if (stream->codec_param.info.frm_ptime > ptime)
	    ptime = stream->codec_param.info.frm_ptime;

	ptime <<= 1;

	/* Allocate buffer */
	stream->enc_buf_size = stream->port.info.clock_rate * ptime / 1000;
	stream->enc_buf = (pj_int16_t*)
			  pj_pool_alloc(pool, stream->enc_buf_size * 2);

    } else {
	stream->enc_samples_per_frame = stream->port.info.samples_per_frame;
    }

    /* Initially disable the VAD in the stream, to help traverse NAT better */
    stream->vad_enabled = stream->codec_param.setting.vad;
    if (stream->vad_enabled) {
	stream->codec_param.setting.vad = 0;
	stream->ts_vad_disabled = 0;
	stream->codec->op->modify(stream->codec, &stream->codec_param);
	PJ_LOG(4,(stream->port.info.name.ptr,"VAD temporarily disabled"));
    }

    /* Get the frame size: */

    stream->frame_size = ((stream->codec_param.info.avg_bps + 7) / 8) * 
			  stream->codec_param.info.frm_ptime / 1000;


    /* Init RTCP session: */

    pjmedia_rtcp_init(&stream->rtcp, stream->port.info.name.ptr,
		      info->fmt.clock_rate, 
		      stream->port.info.samples_per_frame, 
		      info->ssrc);


    /* Init jitter buffer parameters: */
    if (info->jb_max > 0)
	jb_max = info->jb_max;
    else
	jb_max = 360 / stream->codec_param.info.frm_ptime;

    if (info->jb_min_pre >= 0)
	jb_min_pre = info->jb_min_pre;
    else
	jb_min_pre = 60 / stream->codec_param.info.frm_ptime;

    if (info->jb_max_pre > 0)
	jb_max_pre = info->jb_max_pre;
    else
	jb_max_pre = 240 / stream->codec_param.info.frm_ptime;

    if (info->jb_init >= 0)
	jb_init = info->jb_init;
    else
	jb_init = (jb_min_pre + jb_max_pre) / 2;


    /* Create jitter buffer */
    status = pjmedia_jbuf_create(pool, &stream->port.info.name,
				 stream->frame_size, 
				 stream->codec_param.info.frm_ptime,
				 jb_max, &stream->jb);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Set up jitter buffer */
    pjmedia_jbuf_set_adaptive( stream->jb, jb_init, jb_min_pre, jb_max_pre);

    /* Create decoder channel: */

    status = create_channel( pool, stream, PJMEDIA_DIR_DECODING, 
			     info->fmt.pt, info, &stream->dec);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Create encoder channel: */

    status = create_channel( pool, stream, PJMEDIA_DIR_ENCODING, 
			     info->tx_pt, info, &stream->enc);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Only attach transport when stream is ready. */
    status = pjmedia_transport_attach(tp, stream, &info->rem_addr, 
				      &info->rem_rtcp, sizeof(info->rem_addr), 
                                      &on_rx_rtp, &on_rx_rtcp);
    if (status != PJ_SUCCESS)
	goto err_cleanup;

    stream->transport = tp;


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


    /* Detach from transport */
    if (stream->transport) {
	(*stream->transport->op->detach)(stream->transport, stream);
	stream->transport = NULL;
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
 * Get the transport object
 */
PJ_DEF(pjmedia_transport*) pjmedia_stream_get_transport(pjmedia_stream *st)
{
    return st->transport;
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

	/* Also reset jitter buffer */
	pj_mutex_lock( stream->jb_mutex );
	pjmedia_jbuf_reset(stream->jb);
	pj_mutex_unlock( stream->jb_mutex );

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
	stream->enc->paused = 0;
	PJ_LOG(4,(stream->port.info.name.ptr, "Encoder stream resumed"));
    }

    if ((dir & PJMEDIA_DIR_DECODING) && stream->dec) {
	stream->dec->paused = 0;
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
	(long)PJ_ARRAY_SIZE(stream->tx_dtmf_buf))
    {
	status = PJ_ETOOMANY;
    } else {
	int i;

	/* convert ASCII digits into payload type first, to make sure
	 * that all digits are valid. 
	 */
	for (i=0; i<digit_char->slen; ++i) {
	    unsigned pt;
	    int dig = pj_tolower(digit_char->ptr[i]);

	    if (dig >= '0' && dig <= '9')
	    {
		pt = dig - '0';
	    } 
	    else if (dig >= 'a' && dig <= 'd')
	    {
		pt = dig - 'a' + 12;
	    }
	    else if (dig == '*')
	    {
		pt = 10;
	    }
	    else if (dig == '#')
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


/*
 * Set callback to be called upon receiving DTMF digits.
 */
PJ_DEF(pj_status_t) pjmedia_stream_set_dtmf_callback(pjmedia_stream *stream,
				 void (*cb)(pjmedia_stream*, 
					    void *user_data, 
					    int digit), 
				 void *user_data)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    /* By convention, we use jitter buffer's mutex to access DTMF
     * digits resources.
     */
    pj_mutex_lock(stream->jb_mutex);

    stream->dtmf_cb = cb;
    stream->dtmf_cb_user_data = user_data;

    pj_mutex_unlock(stream->jb_mutex);

    return PJ_SUCCESS;
}


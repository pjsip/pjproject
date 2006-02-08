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
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/jbuf.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/string.h>	    /* memcpy() */
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/compat/socket.h>
#include <pj/sock_select.h>
#include <pj/errno.h>
#include <stdlib.h>


#define THIS_FILE			"stream.c"
#define ERRLEVEL			1
#define TRACE_(expr)			PJ_LOG(3,expr)

#define PJMEDIA_MAX_FRAME_DURATION_MS   200
#define PJMEDIA_MAX_BUFFER_SIZE_MS	2000
#define PJMEDIA_MAX_MTU			1500

struct jb_frame
{
    unsigned size;
    void    *buf;
};

#define pj_fifobuf_alloc(fifo,size)	malloc(size)
#define pj_fifobuf_unalloc(fifo,buf)	free(buf)
#define pj_fifobuf_free(fifo, buf)	free(buf)


/**
 * Media channel.
 */
struct pjmedia_channel
{
    pjmedia_stream	   *stream;	    /**< Parent stream.		    */
    pjmedia_dir		    dir;	    /**< Channel direction.	    */
    unsigned		    pt;		    /**< Payload type.		    */
    pj_bool_t		    paused;	    /**< Paused?.		    */
    pj_snd_stream_info	    snd_info;	    /**< Sound stream param.	    */
    pj_snd_stream	   *snd_stream;	    /**< Sound stream.		    */
    unsigned		    in_pkt_size;    /**< Size of input buffer.	    */
    void		   *in_pkt;	    /**< Input buffer.		    */
    unsigned		    out_pkt_size;   /**< Size of output buffer.	    */
    void		   *out_pkt;	    /**< Output buffer.		    */
    unsigned		    pcm_buf_size;   /**< Size of PCM buffer.	    */
    void		   *pcm_buf;	    /**< PCM buffer.		    */
    pj_rtp_session	    rtp;	    /**< RTP session.		    */
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
    pjmedia_channel	    *enc;	    /**< Encoding channel.	    */
    pjmedia_channel	    *dec;	    /**< Decoding channel.	    */

    pjmedia_dir		     dir;	    /**< Stream direction.	    */
    pjmedia_stream_stat	     stat;	    /**< Stream statistics.	    */

    pjmedia_codec_mgr	    *codec_mgr;	    /**< Codec manager instance.    */
    pjmedia_codec	    *codec;	    /**< Codec instance being used. */

    pj_mutex_t		    *jb_mutex;
    pj_jitter_buffer	     jb;	    /**< Jitter buffer.		    */

    pj_sock_t		     rtp_sock;	    /**< RTP socket.		    */
    pj_sock_t		     rtcp_sock;	    /**< RTCP socket.		    */
    pj_sockaddr_in	     dst_addr;	    /**< Destination RTP address.   */

    pj_rtcp_session	     rtcp;	    /**< RTCP for incoming RTP.	    */

    pj_bool_t		     quit_flag;	    /**< To signal thread exit.	    */
    pj_thread_t		    *thread;	    /**< Jitter buffer's thread.    */
};



/*
 * play_callback()
 *
 * This callback is called by sound device's player thread when it
 * needs to feed the player with some frames.
 */
static pj_status_t play_callback(/* in */   void *user_data,
				 /* in */   pj_uint32_t timestamp,
				 /* out */  void *frame,
				 /*inout*/  unsigned size)
{
    pjmedia_channel *channel = user_data;
    pjmedia_stream *stream = channel->stream;
    struct jb_frame *jb_frame;
    void *p;
    pj_uint32_t extseq;
    pj_status_t status;
    struct pjmedia_frame frame_in, frame_out;

    PJ_UNUSED_ARG(timestamp);

    /* Do nothing if we're quitting. */
    if (stream->quit_flag)
	return -1;

    /* Lock jitter buffer mutex */
    pj_mutex_lock( stream->jb_mutex );

    /* Get frame from jitter buffer. */
    status = pj_jb_get(&stream->jb, &extseq, &p);

    /* Unlock jitter buffer mutex. */
    pj_mutex_unlock( stream->jb_mutex );

    jb_frame = p;
    if (status != PJ_SUCCESS || jb_frame == NULL) {
	pj_memset(frame, 0, size);
	return 0;
    }

    /* Decode */
    frame_in.buf = jb_frame->buf;
    frame_in.size = jb_frame->size;
    frame_in.type = PJMEDIA_FRAME_TYPE_AUDIO;  /* ignored */
    frame_out.buf = channel->pcm_buf;
    status = stream->codec->op->decode( stream->codec, &frame_in,
					channel->pcm_buf_size, &frame_out);
    if (status != 0) {
	TRACE_((THIS_FILE, "decode() has return error status %d", status));

	pj_memset(frame, 0, size);
	pj_fifobuf_free (&channel->fifobuf, jb_frame);
	return 0;
    }

    /* Put in sound buffer. */
    if (frame_out.size > size) {
	TRACE_((THIS_FILE, "Sound playout buffer truncated %d bytes", 
		frame_out.size - size));
	frame_out.size = size;
    }

    pj_memcpy(frame, frame_out.buf, size);
    pj_fifobuf_free (&channel->fifobuf, jb_frame);

    return 0;
}


/**
 * rec_callback()
 *
 * This callback is called when the mic device has gathered
 * enough audio samples. We will encode the audio samples and
 * send it to remote.
 */
static pj_status_t rec_callback( /* in */ void *user_data,
			         /* in */ pj_uint32_t timestamp,
			         /* in */ const void *frame,
			         /* in */ unsigned size)
{
    pjmedia_channel *channel = user_data;
    pjmedia_stream *stream = channel->stream;
    pj_status_t status = 0;
    struct pjmedia_frame frame_in, frame_out;
    int ts_len;
    void *rtphdr;
    int rtphdrlen;
    pj_ssize_t sent;


    PJ_UNUSED_ARG(timestamp);

    /* Check if stream is quitting. */
    if (stream->quit_flag)
	return -1;

    /* Encode. */
    frame_in.type = PJMEDIA_TYPE_AUDIO;
    frame_in.buf = (void*)frame;
    frame_in.size = size;
    frame_out.buf = ((char*)channel->out_pkt) + sizeof(pj_rtp_hdr);
    status = stream->codec->op->encode( stream->codec, &frame_in, 
					channel->out_pkt_size - sizeof(pj_rtp_hdr), 
					&frame_out);
    if (status != 0) {
	TRACE_((THIS_FILE, "Codec encode() has returned error status %d", 
		status));
	return status;
    }

    /* Encapsulate. */
    ts_len = size / (channel->snd_info.bits_per_sample / 8);
    status = pj_rtp_encode_rtp( &channel->rtp, 
				channel->pt, 0, 
				frame_out.size, ts_len, 
				(const void**)&rtphdr, &rtphdrlen);
    if (status != 0) {
	TRACE_((THIS_FILE, "RTP encode_rtp() has returned error status %d", 
			   status));
	return status;
    }

    if (rtphdrlen != sizeof(pj_rtp_hdr)) {
	/* We don't support RTP with extended header yet. */
	PJ_TODO(SUPPORT_SENDING_RTP_WITH_EXTENDED_HEADER);
	TRACE_((THIS_FILE, "Unsupported extended RTP header for transmission"));
	return 0;
    }

    pj_memcpy(channel->out_pkt, rtphdr, sizeof(pj_rtp_hdr));

    /* Send. */
    sent = frame_out.size+sizeof(pj_rtp_hdr);
    status = pj_sock_sendto(stream->rtp_sock, channel->out_pkt, &sent, 0, 
			    &stream->dst_addr, sizeof(stream->dst_addr));
    if (status != PJ_SUCCESS)
	return status;

    /* Update stat */
    stream->stat.enc.pkt++;
    stream->stat.enc.bytes += frame_out.size+sizeof(pj_rtp_hdr);

    return 0;
}


/*
 * This thread will poll the socket for incoming packets, and put
 * the packets to jitter buffer.
 */
static int PJ_THREAD_FUNC jitter_buffer_thread (void*arg)
{
    pjmedia_stream *stream = arg;
    pjmedia_channel *channel = stream->dec;

    while (!stream->quit_flag) {
	pj_ssize_t len, size;
	const pj_rtp_hdr *hdr;
	const void *payload;
	unsigned payloadlen;
	int status;
	struct jb_frame *jb_frame;

	/* Wait for packet. */
	pj_fd_set_t fds;
	pj_time_val timeout;

	PJ_FD_ZERO (&fds);
	PJ_FD_SET (stream->rtp_sock, &fds);
	timeout.sec = 0;
	timeout.msec = 1;

	/* Wait with timeout. */
	status = pj_sock_select(stream->rtp_sock, &fds, NULL, NULL, &timeout);
	if (status != 1)
	    continue;

	/* Get packet from socket. */
	len = channel->in_pkt_size;
	status = pj_sock_recv(stream->rtp_sock, channel->in_pkt, &len, 0);
	if (len < 1 || status != PJ_SUCCESS) {
	    if (pj_get_netos_error() == PJ_STATUS_FROM_OS(OSERR_ECONNRESET)) {
		/* On Win2K SP2 (or above) and WinXP, recv() will get 
		 * WSAECONNRESET when the sending side receives ICMP port 
		 * unreachable.
		 */
		continue;
	    }
	    pj_thread_sleep(1);
	    continue;
	}

	if (channel->paused)
	    continue;

	/* Update RTP and RTCP session. */
	status = pj_rtp_decode_rtp(&channel->rtp, channel->in_pkt, len, 
				   &hdr, &payload, &payloadlen);
	if (status != PJ_SUCCESS) {
	    TRACE_((THIS_FILE, "RTP decode_rtp() has returned error status %d",
		    status));
	    continue;
	}

	status = pj_rtp_session_update(&channel->rtp, hdr);
	if (status != 0 && 
	    status != PJMEDIA_RTP_ERR_SESSION_PROBATION && 
	    status != PJMEDIA_RTP_ERR_SESSION_RESTARTED) 
	{
	    TRACE_((THIS_FILE, 
		    "RTP session_update() has returned error status %d", 
		    status));
	    continue;
	}
	pj_rtcp_rx_rtp(&stream->rtcp, pj_ntohs(hdr->seq), pj_ntohl(hdr->ts));

	/* Update stat */
	stream->stat.dec.pkt++;
	stream->stat.dec.bytes += len;

	/* Copy to FIFO buffer. */
	size = payloadlen+sizeof(struct jb_frame);
	jb_frame = pj_fifobuf_alloc (&channel->fifobuf, size);
	if (jb_frame == NULL) {
	    TRACE_((THIS_FILE, "Unable to allocate %d bytes FIFO buffer", 
		    size));
	    continue;
	}

	/* Copy the payload */
	jb_frame->size = payloadlen;
	jb_frame->buf = ((char*)jb_frame) + sizeof(struct jb_frame);
	pj_memcpy (jb_frame->buf, payload, payloadlen);

	/* Put to jitter buffer. */
	pj_mutex_lock( stream->jb_mutex );
	status = pj_jb_put(&stream->jb, pj_ntohs(hdr->seq), jb_frame);
	pj_mutex_unlock( stream->jb_mutex );

	if (status != 0) {
	    pj_fifobuf_unalloc (&channel->fifobuf, jb_frame);
	    
	    TRACE_((THIS_FILE, 
		    "Jitter buffer put() has returned error status %d", 
		    status));
	    continue;
	}
    }

    return 0;
}


/*
 * Create sound stream parameter from codec attributes.
 */
static void init_snd_param( pj_snd_stream_info *snd_param,
			    const pjmedia_codec_param *codec_param)
{
    pj_memset(snd_param, 0, sizeof(*snd_param));

    snd_param->bits_per_sample	 = codec_param->pcm_bits_per_sample;
    snd_param->bytes_per_frame   = 2;
    snd_param->frames_per_packet = codec_param->sample_rate * 
				   codec_param->ptime / 
				   1000;
    snd_param->samples_per_frame = 1;
    snd_param->samples_per_sec   = codec_param->sample_rate;
}


/*
 * Create media channel.
 */
static pj_status_t create_channel( pj_pool_t *pool,
				   pjmedia_stream *stream,
				   pjmedia_dir dir,
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
    channel->pt = param->fmt.pt;

    /* Allocate buffer for incoming packet. */

    channel->in_pkt_size = PJMEDIA_MAX_MTU;
    channel->in_pkt = pj_pool_alloc( pool, channel->in_pkt_size );
    PJ_ASSERT_RETURN(channel->in_pkt != NULL, PJ_ENOMEM);

    
    /* Allocate buffer for outgoing packet. */

    channel->out_pkt_size = sizeof(pj_rtp_hdr) + 
			    codec_param->avg_bps/8 * 
			    PJMEDIA_MAX_FRAME_DURATION_MS / 
			    1000;

    if (channel->out_pkt_size > PJMEDIA_MAX_MTU)
	channel->out_pkt_size = PJMEDIA_MAX_MTU;

    channel->out_pkt = pj_pool_alloc(pool, channel->out_pkt_size);
    PJ_ASSERT_RETURN(channel->out_pkt != NULL, PJ_ENOMEM);


    /* Allocate buffer for decoding to PCM: */

    channel->pcm_buf_size = codec_param->sample_rate * 
			    codec_param->pcm_bits_per_sample / 8 *
			    PJMEDIA_MAX_FRAME_DURATION_MS / 1000;
    channel->pcm_buf = pj_pool_alloc (pool, channel->pcm_buf_size);
    PJ_ASSERT_RETURN(channel->pcm_buf != NULL, PJ_ENOMEM);


    /* Create RTP and RTCP sessions: */

    status = pj_rtp_session_init(&channel->rtp, param->fmt.pt, 
				 param->ssrc);
    if (status != PJ_SUCCESS)
	return status;

    /* Create and initialize sound device */

    init_snd_param(&channel->snd_info, codec_param);

    if (dir == PJMEDIA_DIR_ENCODING)
	channel->snd_stream = pj_snd_open_recorder(-1, &channel->snd_info, 
						   &rec_callback, channel);
    else
	channel->snd_stream = pj_snd_open_player(-1, &channel->snd_info, 
						 &play_callback, channel);

    if (!channel->snd_stream)
	return -1;


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
					   pjmedia_stream **p_stream)

{
    pjmedia_stream *stream;
    pjmedia_codec_param codec_param;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && info && p_stream, PJ_EINVAL);


    /* Allocate the media stream: */

    stream = pj_pool_zalloc(pool, sizeof(pjmedia_stream));
    PJ_ASSERT_RETURN(stream != NULL, PJ_ENOMEM);


    /* Init stream: */
   
    stream->dir = info->dir;
    stream->codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);

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

    status = stream->codec->op->default_attr(stream->codec, &codec_param);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Open the codec: */

    status = stream->codec->op->open(stream->codec, &codec_param);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Init RTCP session: */

    pj_rtcp_init(&stream->rtcp, info->ssrc);


    /* Init jitter buffer: */

    status = pj_jb_init(&stream->jb, pool, 
			info->jb_min, info->jb_max, info->jb_maxcnt);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /*  Create jitter buffer thread: */

    status = pj_thread_create(pool, "decode", 
			      &jitter_buffer_thread, stream,
			      0, 0, &stream->thread);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Create decoder channel: */

    status = create_channel( pool, stream, PJMEDIA_DIR_DECODING, info,
			     &codec_param, &stream->dec);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Create encoder channel: */

    status = create_channel( pool, stream, PJMEDIA_DIR_ENCODING, info,
			     &codec_param, &stream->enc);
    if (status != PJ_SUCCESS)
	goto err_cleanup;


    /* Success! */
    *p_stream = stream;
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

    /* Signal threads to quit. */

    stream->quit_flag = 1;


    /* Close encoding sound stream. */
    
    if (stream->enc && stream->enc->snd_stream) {

	pj_snd_stream_stop(stream->enc->snd_stream);
	pj_snd_stream_close(stream->enc->snd_stream);
	stream->enc->snd_stream = NULL;

    }

    /* Close decoding sound stream. */

    if (stream->dec && stream->dec->snd_stream) {

	pj_snd_stream_stop(stream->dec->snd_stream);
	pj_snd_stream_close(stream->dec->snd_stream);
	stream->dec->snd_stream = NULL;

    }

    /* Wait for jitter buffer thread to quit: */

    if (stream->thread) {
	pj_thread_join(stream->thread);
	pj_thread_destroy(stream->thread);
	stream->thread = NULL;
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
 * Start stream.
 */
PJ_DEF(pj_status_t) pjmedia_stream_start(pjmedia_stream *stream)
{

    PJ_ASSERT_RETURN(stream && stream->enc && stream->dec, PJ_EINVALIDOP);

    if (stream->enc && (stream->dir & PJMEDIA_DIR_ENCODING)) {
	stream->enc->paused = 0;
	pj_snd_stream_start(stream->enc->snd_stream);
    }

    if (stream->dec && (stream->dir & PJMEDIA_DIR_DECODING)) {
	stream->dec->paused = 0;
	pj_snd_stream_start(stream->dec->snd_stream);
    }

    return PJ_SUCCESS;
}


/*
 * Get stream statistics.
 */
PJ_DEF(pj_status_t) pjmedia_stream_get_stat( const pjmedia_stream *stream,
					     pjmedia_stream_stat *stat)
{
    PJ_ASSERT_RETURN(stream && stat, PJ_EINVAL);

    pj_memcpy(stat, &stream->stat, sizeof(pjmedia_stream_stat));

    return PJ_SUCCESS;
}


/*
 * Pause stream.
 */
PJ_DEF(pj_status_t) pjmedia_stream_pause( pjmedia_stream *stream,
					  pjmedia_dir dir)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if ((dir & PJMEDIA_DIR_ENCODING) && stream->enc)
	stream->enc->paused = 1;

    if ((dir & PJMEDIA_DIR_DECODING) && stream->dec)
	stream->dec->paused = 1;

    return PJ_SUCCESS;
}


/*
 * Resume stream
 */
PJ_DEF(pj_status_t) pjmedia_stream_resume( pjmedia_stream *stream,
					   pjmedia_dir dir)
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    if ((dir & PJMEDIA_DIR_ENCODING) && stream->enc)
	stream->enc->paused = 1;

    if ((dir & PJMEDIA_DIR_DECODING) && stream->dec)
	stream->dec->paused = 1;

    return PJ_SUCCESS;
}


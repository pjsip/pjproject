/* $Header: /pjproject/pjmedia/src/pjmedia/stream.c 14    6/24/05 11:14p Bennylp $ */

#include <pjmedia/stream.h>
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/jbuf.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/string.h>	    /* memcpy() */
#include <pj/pool.h>
#include <stdlib.h>

#define THISFILE    "stream.c"
#define ERRLEVEL    1

#define PJ_MAX_FRAME_DURATION_MS    200
#define PJ_MAX_BUFFER_SIZE_MS	    2000
#define PJ_MAX_MTU		    1500

struct jb_frame
{
    unsigned size;
    void    *buf;
};

#define pj_fifobuf_alloc(fifo,size)	malloc(size)
#define pj_fifobuf_unalloc(fifo,buf)	free(buf)
#define pj_fifobuf_free(fifo, buf)	free(buf)

enum stream_state
{
    STREAM_STOPPED,
    STREAM_STARTED,
};

struct pj_media_stream_t
{
    pj_media_dir_t	    dir;
    int			    pt;
    int			    state;
    pj_media_stream_stat    stat;
    pj_media_stream_t	   *peer;
    pj_snd_stream_info	    snd_info;
    pj_snd_stream	   *snd_stream;
    pj_mutex_t		   *mutex;
    unsigned		    in_pkt_size;
    void		   *in_pkt;
    unsigned		    out_pkt_size;
    void		   *out_pkt;
    unsigned		    pcm_buf_size;
    void		   *pcm_buf;
    //pj_fifobuf_t	    fifobuf;
    pj_codec_mgr	   *codec_mgr;
    pj_codec		   *codec;
    pj_rtp_session	    rtp;
    pj_rtcp_session	   *rtcp;
    pj_jitter_buffer	   *jb;
    pj_sock_t		    rtp_sock;
    pj_sock_t		    rtcp_sock;
    pj_sockaddr_in	    dst_addr;
    pj_thread_t		   *transport_thread;
    int			    thread_quit_flag;
};


static pj_status_t play_callback(/* in */   void *user_data,
				 /* in */   pj_uint32_t timestamp,
				 /* out */  void *frame,
				 /*inout*/  unsigned size)
{
    pj_media_stream_t *channel = user_data;
    struct jb_frame *jb_frame;
    void *p;
    pj_uint32_t extseq;
    pj_status_t status;
    struct pj_audio_frame frame_in, frame_out;

    PJ_UNUSED_ARG(timestamp)

    /* Lock mutex */
    pj_mutex_lock (channel->mutex);

    if (!channel->codec) {
	pj_mutex_unlock (channel->mutex);
	return -1;
    }

    /* Get frame from jitter buffer. */
    status = pj_jb_get (channel->jb, &extseq, &p);
    jb_frame = p;
    if (status != 0 || jb_frame == NULL) {
	pj_memset(frame, 0, size);
	pj_mutex_unlock(channel->mutex);
	return 0;
    }

    /* Decode */
    frame_in.buf = jb_frame->buf;
    frame_in.size = jb_frame->size;
    frame_in.type = PJ_AUDIO_FRAME_AUDIO;  /* ignored */
    frame_out.buf = channel->pcm_buf;
    status = channel->codec->op->decode (channel->codec, &frame_in,
					 channel->pcm_buf_size, &frame_out);
    if (status != 0) {
	PJ_LOG(3, (THISFILE, "decode() has return error status %d", 
			  status));

	pj_memset(frame, 0, size);
	pj_fifobuf_free (&channel->fifobuf, jb_frame);
	pj_mutex_unlock(channel->mutex);
	return 0;
    }

    /* Put in sound buffer. */
    if (frame_out.size > size) {
	PJ_LOG(3, (THISFILE, "Sound playout buffer truncated %d bytes", 
			  frame_out.size - size));
	frame_out.size = size;
    }

    pj_memcpy(frame, frame_out.buf, size);

    pj_fifobuf_free (&channel->fifobuf, jb_frame);
    pj_mutex_unlock(channel->mutex);
    return 0;
}

static pj_status_t rec_callback( /* in */ void *user_data,
			         /* in */ pj_uint32_t timestamp,
			         /* in */ const void *frame,
			         /* in */ unsigned size)
{
    pj_media_stream_t *channel = user_data;
    pj_status_t status = 0;
    struct pj_audio_frame frame_in, frame_out;
    int ts_len;
    void *rtphdr;
    int rtphdrlen;
    int sent;
#if 0
    static FILE *fhnd = NULL;
#endif

    PJ_UNUSED_ARG(timestamp)

    /* Start locking channel mutex */
    pj_mutex_lock (channel->mutex);

    if (!channel->codec) {
	status = -1;
	goto on_return;
    }

    /* Encode. */
    frame_in.type = PJ_MEDIA_TYPE_AUDIO;
    frame_in.buf = (void*)frame;
    frame_in.size = size;
    frame_out.buf = ((char*)channel->out_pkt) + sizeof(pj_rtp_hdr);
    status = channel->codec->op->encode (channel->codec, &frame_in, 
					 channel->out_pkt_size - sizeof(pj_rtp_hdr), 
					 &frame_out);
    if (status != 0) {
	PJ_LOG(3,(THISFILE, "Codec encode() has returned error status %d", 
			     status));
	goto on_return;
    }

    /* Encapsulate. */
    ts_len = size / (channel->snd_info.bits_per_sample / 8);
    status = pj_rtp_encode_rtp (&channel->rtp, channel->pt, 0, 
				frame_out.size, ts_len, 
				(const void**)&rtphdr, &rtphdrlen);
    if (status != 0) {
	PJ_LOG(3,(THISFILE, "RTP encode_rtp() has returned error status %d", 
			    status));
	goto on_return;
    }

    if (rtphdrlen != sizeof(pj_rtp_hdr)) {
	/* We don't support RTP with extended header yet. */
	PJ_TODO(SUPPORT_SENDING_RTP_WITH_EXTENDED_HEADER);
	PJ_LOG(3,(THISFILE, "Unsupported extended RTP header for transmission"));
	goto on_return;
    }

    pj_memcpy(channel->out_pkt, rtphdr, sizeof(pj_rtp_hdr));

    /* Send. */
    sent = pj_sock_sendto (channel->rtp_sock, channel->out_pkt, frame_out.size+sizeof(pj_rtp_hdr), 0, 
			   &channel->dst_addr, sizeof(channel->dst_addr));
    if (sent != (int)frame_out.size + (int)sizeof(pj_rtp_hdr))  {
	pj_perror(THISFILE, "Error sending RTP packet to %s:%d", 
		  pj_sockaddr_get_str_addr(&channel->dst_addr),
		  pj_sockaddr_get_port(&channel->dst_addr));
	goto on_return;
    }

    /* Update stat */
    channel->stat.pkt_tx++;
    channel->stat.oct_tx += frame_out.size+sizeof(pj_rtp_hdr);

#if 0
    if (fhnd == NULL) {
	fhnd = fopen("RTP.DAT", "wb");
	if (fhnd) {
	    fwrite (channel->out_pkt, frame_out.size+sizeof(pj_rtp_hdr), 1, fhnd);
	    fclose(fhnd);
	}
    }
#endif

on_return:
    pj_mutex_unlock (channel->mutex);
    return status;
}


static void* PJ_THREAD_FUNC stream_decoder_transport_thread (void*arg)
{
    pj_media_stream_t *channel = arg;

    while (!channel->thread_quit_flag) {
	int len, size;
	const pj_rtp_hdr *hdr;
	const void *payload;
	unsigned payloadlen;
	int status;
	struct jb_frame *jb_frame;

	/* Wait for packet. */
	fd_set fds;
	pj_time_val timeout;

	PJ_FD_ZERO (&fds);
	PJ_FD_SET (channel->rtp_sock, &fds);
	timeout.sec = 0;
	timeout.msec = 100;

	/* Wait with timeout. */
	status = pj_sock_select(channel->rtp_sock, &fds, NULL, NULL, &timeout);
	if (status != 1)
	    continue;

	/* Get packet from socket. */
	len = pj_sock_recv (channel->rtp_sock, channel->in_pkt, channel->in_pkt_size, 0);
	if (len < 1) {
	    if (pj_getlasterror() == PJ_ECONNRESET) {
		/* On Win2K SP2 (or above) and WinXP, recv() will get WSAECONNRESET
		   when the sending side receives ICMP port unreachable.
		 */
		continue;
	    }
	    pj_perror(THISFILE, "Error receiving packet from socket (len=%d)", len);
	    pj_thread_sleep(1);
	    continue;
	}

	if (channel->state != STREAM_STARTED)
	    continue;

	if (channel->thread_quit_flag)
	    break;

	/* Start locking the channel. */
	pj_mutex_lock (channel->mutex);

	/* Update RTP and RTCP session. */
	status = pj_rtp_decode_rtp (&channel->rtp, channel->in_pkt, len, &hdr, &payload, &payloadlen);
	if (status != 0) {
	    pj_mutex_unlock (channel->mutex);
	    PJ_LOG(4,(THISFILE, "RTP decode_rtp() has returned error status %d", status));
	    continue;
	}
	status = pj_rtp_session_update (&channel->rtp, hdr);
	if (status != 0 && status != PJ_RTP_ERR_SESSION_PROBATION && status != PJ_RTP_ERR_SESSION_RESTARTED) {
	    pj_mutex_unlock (channel->mutex);
	    PJ_LOG(4,(THISFILE, "RTP session_update() has returned error status %d", status));
	    continue;
	}
	pj_rtcp_rx_rtp (channel->rtcp, pj_ntohs(hdr->seq), pj_ntohl(hdr->ts));

	/* Update stat */
	channel->stat.pkt_rx++;
	channel->stat.oct_rx += len;

	/* Copy to FIFO buffer. */
	size = payloadlen+sizeof(struct jb_frame);
	jb_frame = pj_fifobuf_alloc (&channel->fifobuf, size);
	if (jb_frame == NULL) {
	    pj_mutex_unlock (channel->mutex);
	    PJ_LOG(4,(THISFILE, "Unable to allocate %d bytes FIFO buffer", size));
	    continue;
	}

	/* Copy the payload */
	jb_frame->size = payloadlen;
	jb_frame->buf = ((char*)jb_frame) + sizeof(struct jb_frame);
	pj_memcpy (jb_frame->buf, payload, payloadlen);

	/* Put to jitter buffer. */
	status = pj_jb_put (channel->jb, pj_ntohs(hdr->seq), jb_frame);
	if (status != 0) {
	    pj_fifobuf_unalloc (&channel->fifobuf, jb_frame);
	    pj_mutex_unlock (channel->mutex);
	    PJ_LOG(4,(THISFILE, "Jitter buffer put() has returned error status %d", status));
	    continue;
	}

	pj_mutex_unlock (channel->mutex);
    }

    return NULL;
}

static void init_snd_param_from_codec_attr (pj_snd_stream_info *param,
					    const pj_codec_attr *attr)
{
    param->bits_per_sample = attr->pcm_bits_per_sample;
    param->bytes_per_frame = 2;
    param->frames_per_packet = attr->sample_rate * attr->ptime / 1000;
    param->samples_per_frame = 1;
    param->samples_per_sec = attr->sample_rate;
}

static pj_media_stream_t *create_channel ( pj_pool_t *pool,
					   pj_media_dir_t dir,
					   pj_media_stream_t *peer,
					   pj_codec_id *codec_id,
					   pj_media_stream_create_param *param)
{
    pj_media_stream_t *channel;
    pj_codec_attr codec_attr;
    void *ptr;
    unsigned size;
    int status;
    
    /* Allocate memory for channel descriptor */
    size = sizeof(pj_media_stream_t);
    channel = pj_pool_calloc(pool, 1, size);
    if (!channel) {
	PJ_LOG(1,(THISFILE, "Unable to allocate %u bytes channel descriptor", 
			 size));
	return NULL;
    }

    channel->dir = dir;
    channel->pt = codec_id->pt;
    channel->peer = peer;
    channel->codec_mgr = pj_med_mgr_get_codec_mgr (param->mediamgr);
    channel->rtp_sock = param->rtp_sock;
    channel->rtcp_sock = param->rtcp_sock;
    channel->dst_addr = *param->remote_addr;
    channel->state = STREAM_STOPPED;

    /* Create mutex for the channel. */
    channel->mutex = pj_mutex_create(pool, NULL, PJ_MUTEX_SIMPLE);
    if (channel->mutex == NULL)
	goto err_cleanup;

    /* Create and initialize codec, only if peer is not present.
       We only use one codec instance for both encoder and decoder.
     */
    if (peer && peer->codec) {
	channel->codec = peer->codec;
	status = channel->codec->factory->op->default_attr(channel->codec->factory, codec_id, 
							   &codec_attr);
	if (status != 0) {
	    goto err_cleanup;
	}

    } else {
	channel->codec = pj_codec_mgr_alloc_codec(channel->codec_mgr, codec_id);
	if (channel->codec == NULL) {
	    goto err_cleanup;
	}

	status = channel->codec->factory->op->default_attr(channel->codec->factory, codec_id, 
							   &codec_attr);
	if (status != 0) {
	    goto err_cleanup;
	}

	codec_attr.pt = codec_id->pt;
	status = channel->codec->op->open(channel->codec, &codec_attr);
	if (status != 0) {
	    goto err_cleanup;
	}
    }

    /* Allocate buffer for incoming packet. */
    channel->in_pkt_size = PJ_MAX_MTU;
    channel->in_pkt = pj_pool_alloc(pool, channel->in_pkt_size);
    if (!channel->in_pkt) {
	PJ_LOG(1, (THISFILE, "Unable to allocate %u bytes incoming packet buffer", 
			  channel->in_pkt_size));
	goto err_cleanup;
    }

    /* Allocate buffer for outgoing packet. */
    channel->out_pkt_size = sizeof(pj_rtp_hdr) + 
			    codec_attr.avg_bps / 8 * PJ_MAX_FRAME_DURATION_MS / 1000;
    if (channel->out_pkt_size > PJ_MAX_MTU)
	channel->out_pkt_size = PJ_MAX_MTU;
    channel->out_pkt = pj_pool_alloc(pool, channel->out_pkt_size);
    if (!channel->out_pkt) {
	PJ_LOG(1, (THISFILE, "Unable to allocate %u bytes encoding buffer", 
			  channel->out_pkt_size));
	goto err_cleanup;
    }

    /* Allocate buffer for decoding to PCM */
    channel->pcm_buf_size = codec_attr.sample_rate * 
			    codec_attr.pcm_bits_per_sample / 8 *
			    PJ_MAX_FRAME_DURATION_MS / 1000;
    channel->pcm_buf = pj_pool_alloc (pool, channel->pcm_buf_size);
    if (!channel->pcm_buf) {
	PJ_LOG(1, (THISFILE, "Unable to allocate %u bytes PCM buffer", 
			  channel->pcm_buf_size));
	goto err_cleanup;
    }

    /* Allocate buffer for frames put in jitter buffer. */
    size = codec_attr.avg_bps / 8 * PJ_MAX_BUFFER_SIZE_MS / 1000;
    ptr = pj_pool_alloc(pool, size);
    if (!ptr) {
	PJ_LOG(1, (THISFILE, "Unable to allocate %u bytes jitter buffer", 
			  channel->pcm_buf_size));
	goto err_cleanup;
    }
    //pj_fifobuf_init (&channel->fifobuf, ptr, size);

    /* Create and initialize sound device */
    init_snd_param_from_codec_attr (&channel->snd_info, &codec_attr);

    if (dir == PJ_MEDIA_DIR_ENCODING)
	channel->snd_stream = pj_snd_open_recorder(-1, &channel->snd_info, 
						   &rec_callback, channel);
    else
	channel->snd_stream = pj_snd_open_player(-1, &channel->snd_info, 
						 &play_callback, channel);

    if (!channel->snd_stream)
	goto err_cleanup;

    /* Create RTP and RTCP sessions. */
    if (pj_rtp_session_init(&channel->rtp, codec_id->pt, param->ssrc) != 0) {
	PJ_LOG(1, (THISFILE, "RTP session initialization error"));
	goto err_cleanup;
    }

    /* For decoder, create RTCP session, jitter buffer, and transport thread. */
    if (dir == PJ_MEDIA_DIR_DECODING) {
	channel->rtcp = pj_pool_calloc(pool, 1, sizeof(pj_rtcp_session));
	if (!channel->rtcp) {
	    PJ_LOG(1, (THISFILE, "Unable to allocate RTCP session"));
	    goto err_cleanup;
	}

	pj_rtcp_init(channel->rtcp, param->ssrc);

	channel->jb = pj_pool_calloc(pool, 1, sizeof(pj_jitter_buffer));
	if (!channel->jb) {
	    PJ_LOG(1, (THISFILE, "Unable to allocate jitter buffer descriptor"));
	    goto err_cleanup;
	}
	if (pj_jb_init(channel->jb, pool, param->jb_min, param->jb_max, param->jb_maxcnt)) {
	    PJ_LOG(1, (THISFILE, "Unable to allocate jitter buffer"));
	    goto err_cleanup;
	}

	channel->transport_thread = pj_thread_create(pool, "decode", 
						     &stream_decoder_transport_thread, channel,
						     0, NULL, 0);
	if (!channel->transport_thread) {
	    pj_perror(THISFILE, "Unable to create transport thread");
	    goto err_cleanup;
	}
    }

    /* Done. */
    return channel;

err_cleanup:
    pj_media_stream_destroy(channel);
    return NULL;
}


PJ_DEF(pj_status_t) pj_media_stream_create (pj_pool_t *pool,
					    pj_media_stream_t **enc_stream,
					    pj_media_stream_t **dec_stream,
					    pj_media_stream_create_param *param)
{
    *dec_stream = *enc_stream = NULL;

    if (param->dir & PJ_MEDIA_DIR_DECODING) {
	*dec_stream = 
	    create_channel(pool, PJ_MEDIA_DIR_DECODING, NULL, param->codec_id, param);
	if (!*dec_stream)
	    return -1;
    }

    if (param->dir & PJ_MEDIA_DIR_ENCODING) {
	*enc_stream = 
	    create_channel(pool, PJ_MEDIA_DIR_ENCODING, *dec_stream, param->codec_id, param);
	if (!*enc_stream) {
	    if (*dec_stream) {
		pj_media_stream_destroy(*dec_stream);
		*dec_stream = NULL;
	    }
	    return -1;
	}

	if (*dec_stream) {
	    (*dec_stream)->peer = *enc_stream;
	}
    }

    return 0;
}

PJ_DEF(pj_status_t) pj_media_stream_start (pj_media_stream_t *channel)
{
    pj_status_t status;

    status = pj_snd_stream_start(channel->snd_stream);

    if (status == 0)
	channel->state = STREAM_STARTED;
    return status;
}

PJ_DEF(pj_status_t)  pj_media_stream_get_stat (const pj_media_stream_t *stream,
					       pj_media_stream_stat *stat)
{
    if (stream->dir == PJ_MEDIA_DIR_ENCODING) {
	pj_memcpy (stat, &stream->stat, sizeof(*stat));
    } else {
	pj_rtcp_pkt *rtcp_pkt;
	int len;

	pj_memset (stat, 0, sizeof(*stat));
	pj_assert (stream->rtcp != 0);
	pj_rtcp_build_rtcp (stream->rtcp, &rtcp_pkt, &len);

	stat->pkt_rx = stream->stat.pkt_rx;
	stat->oct_rx = stream->stat.oct_rx;

	PJ_TODO(SUPPORT_JITTER_CALCULATION_FOR_NON_8KHZ_SAMPLE_RATE)
	stat->jitter = pj_ntohl(rtcp_pkt->rr.jitter) / 8;
	stat->pkt_lost = (rtcp_pkt->rr.total_lost_2 << 16) +
			 (rtcp_pkt->rr.total_lost_1 << 8) +
			 rtcp_pkt->rr.total_lost_0;
    }
    return 0;
}

PJ_DEF(pj_status_t) pj_media_stream_pause (pj_media_stream_t *channel)
{
    PJ_UNUSED_ARG(channel)
    return -1;
}

PJ_DEF(pj_status_t) pj_media_stream_resume (pj_media_stream_t *channel)
{
    PJ_UNUSED_ARG(channel)
    return -1;
}

PJ_DEF(pj_status_t) pj_media_stream_destroy (pj_media_stream_t *channel)
{
    channel->thread_quit_flag = 1;

    pj_mutex_lock (channel->mutex);
    if (channel->peer)
	pj_mutex_lock (channel->peer->mutex);

    if (channel->jb) {
	/* No need to deinitialize jitter buffer. */
    }
    if (channel->transport_thread) {
	pj_thread_join(channel->transport_thread);
	pj_thread_destroy(channel->transport_thread);
	channel->transport_thread = NULL;
    }
    if (channel->snd_stream != NULL) {
	pj_mutex_unlock (channel->mutex);
	pj_snd_stream_stop(channel->snd_stream);
	pj_mutex_lock (channel->mutex);
	pj_snd_stream_close(channel->snd_stream);
	channel->snd_stream = NULL;
    }
    if (channel->codec) {
	channel->codec->op->close(channel->codec);
	pj_codec_mgr_dealloc_codec(channel->codec_mgr, channel->codec);
	channel->codec = NULL;
    }
    if (channel->peer) {
	pj_media_stream_t *peer = channel->peer;
	peer->peer = NULL;
	peer->codec = NULL;
	peer->thread_quit_flag = 1;
	if (peer->transport_thread) {
	    pj_mutex_unlock (peer->mutex);
	    pj_thread_join(peer->transport_thread);
	    pj_mutex_lock (peer->mutex);
	    pj_thread_destroy(peer->transport_thread);
	    peer->transport_thread = NULL;
	}
	if (peer->snd_stream) {
	    pj_mutex_unlock (peer->mutex);
	    pj_snd_stream_stop(peer->snd_stream);
	    pj_mutex_lock (peer->mutex);
	    pj_snd_stream_close(peer->snd_stream);
	    peer->snd_stream = NULL;
	}
    }

    channel->state = STREAM_STOPPED;

    if (channel->peer)
	pj_mutex_unlock (channel->peer->mutex);
    pj_mutex_unlock(channel->mutex);
    pj_mutex_destroy(channel->mutex);

    return 0;
}


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
#include <pjmedia/endpoint.h>
#include <pjmedia/errno.h>
#include <pjmedia/sdp.h>
#include <pj/assert.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>


#define THIS_FILE   "endpoint.c"

static const pj_str_t STR_AUDIO = { "audio", 5};
static const pj_str_t STR_VIDEO = { "video", 5};
static const pj_str_t STR_IN = { "IN", 2 };
static const pj_str_t STR_IP4 = { "IP4", 3};
static const pj_str_t STR_RTP_AVP = { "RTP/AVP", 7 };
static const pj_str_t STR_SDP_NAME = { "pjmedia", 7 };
static const pj_str_t STR_SENDRECV = { "sendrecv", 8 };



/* Flag to indicate whether pjmedia error subsystem has been registered
 * to pjlib.
 */
static int error_subsys_registered;


/**
 * Defined in pjmedia/errno.c
 * 
 * Get error message for the specified error code. Note that this
 * function is only able to decode PJMEDIA specific error code.
 * Application should use pj_strerror(), which should be able to
 * decode all error codes belonging to all subsystems (e.g. pjlib,
 * pjmedia, pjsip, etc).
 *
 * @param status    The error code.
 * @param buffer    The buffer where to put the error message.
 * @param bufsize   Size of the buffer.
 *
 * @return	    The error message as NULL terminated string,
 *                  wrapped with pj_str_t.
 */
PJ_DECL(pj_str_t) pjmedia_strerror( pj_status_t status, char *buffer,
				    pj_size_t bufsize);


/* Worker thread proc. */
static int PJ_THREAD_FUNC worker_proc(void*);


#define MAX_THREADS	16


/** Concrete declaration of media endpoint. */
struct pjmedia_endpt
{
    /** Pool. */
    pj_pool_t		 *pool;

    /** Pool factory. */
    pj_pool_factory	 *pf;

    /** Codec manager. */
    pjmedia_codec_mgr	  codec_mgr;

    /** IOqueue instance. */
    pj_ioqueue_t 	 *ioqueue;

    /** Do we own the ioqueue? */
    pj_bool_t		  own_ioqueue;

    /** Number of threads. */
    unsigned		  thread_cnt;

    /** IOqueue polling thread, if any. */
    pj_thread_t		 *thread[MAX_THREADS];

    /** To signal polling thread to quit. */
    pj_bool_t		  quit_flag;
};

/**
 * Initialize and get the instance of media endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_endpt_create(pj_pool_factory *pf,
					 pj_ioqueue_t *ioqueue,
					 unsigned worker_cnt,
					 pjmedia_endpt **p_endpt)
{
    pj_pool_t *pool;
    pjmedia_endpt *endpt;
    unsigned i;
    pj_status_t status;

    if (!error_subsys_registered) {
	pj_register_strerror(PJMEDIA_ERRNO_START, PJ_ERRNO_SPACE_SIZE, 
			     &pjmedia_strerror);
	error_subsys_registered = 1;
    }

    PJ_ASSERT_RETURN(pf && p_endpt, PJ_EINVAL);

    pool = pj_pool_create(pf, "med-ept", 512, 512, NULL);
    if (!pool)
	return PJ_ENOMEM;

    endpt = pj_pool_zalloc(pool, sizeof(struct pjmedia_endpt));
    endpt->pool = pool;
    endpt->pf = pf;
    endpt->ioqueue = ioqueue;
    endpt->thread_cnt = worker_cnt;

    /* Sound */
    pjmedia_snd_init(pf);

    /* Init codec manager. */
    status = pjmedia_codec_mgr_init(&endpt->codec_mgr);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Create ioqueue if none is specified. */
    if (endpt->ioqueue == NULL) {
	
	endpt->own_ioqueue = PJ_TRUE;

	status = pj_ioqueue_create( endpt->pool, PJ_IOQUEUE_MAX_HANDLES,
				    &endpt->ioqueue);
	if (status != PJ_SUCCESS)
	    goto on_error;

	if (worker_cnt == 0) {
	    PJ_LOG(4,(THIS_FILE, "Warning: no worker thread is created in"  
				 "media endpoint for internal ioqueue"));
	}
    }

    /* Create worker threads if asked. */
    for (i=0; i<worker_cnt; ++i) {
	status = pj_thread_create( endpt->pool, "media", &worker_proc,
				   endpt, 0, 0, &endpt->thread[i]);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }


    *p_endpt = endpt;
    return PJ_SUCCESS;

on_error:

    /* Destroy threads */
    for (i=0; i<endpt->thread_cnt; ++i) {
	if (endpt->thread[i]) {
	    pj_thread_destroy(endpt->thread[i]);
	}
    }

    /* Destroy internal ioqueue */
    if (endpt->ioqueue && endpt->own_ioqueue)
	pj_ioqueue_destroy(endpt->ioqueue);

    pjmedia_snd_deinit();
    pj_pool_release(pool);
    return status;
}

/**
 * Get the codec manager instance.
 */
PJ_DEF(pjmedia_codec_mgr*) pjmedia_endpt_get_codec_mgr(pjmedia_endpt *endpt)
{
    return &endpt->codec_mgr;
}

/**
 * Deinitialize media endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_endpt_destroy (pjmedia_endpt *endpt)
{
    unsigned i;

    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    endpt->quit_flag = 1;

    /* Destroy threads */
    for (i=0; i<endpt->thread_cnt; ++i) {
	if (endpt->thread[i]) {
	    pj_thread_join(endpt->thread[i]);
	    pj_thread_destroy(endpt->thread[i]);
	    endpt->thread[i] = NULL;
	}
    }

    /* Destroy internal ioqueue */
    if (endpt->ioqueue && endpt->own_ioqueue) {
	pj_ioqueue_destroy(endpt->ioqueue);
	endpt->ioqueue = NULL;
    }

    endpt->pf = NULL;

    pjmedia_snd_deinit();
    pj_pool_release (endpt->pool);

    return PJ_SUCCESS;
}


/**
 * Get the ioqueue instance of the media endpoint.
 */
PJ_DEF(pj_ioqueue_t*) pjmedia_endpt_get_ioqueue(pjmedia_endpt *endpt)
{
    PJ_ASSERT_RETURN(endpt, NULL);
    return endpt->ioqueue;
}


/**
 * Worker thread proc.
 */
static int PJ_THREAD_FUNC worker_proc(void *arg)
{
    pjmedia_endpt *endpt = arg;

    while (!endpt->quit_flag) {
	pj_time_val timeout = { 0, 500 };
	pj_ioqueue_poll(endpt->ioqueue, &timeout);
    }

    return 0;
}

/**
 * Create pool.
 */
PJ_DEF(pj_pool_t*) pjmedia_endpt_create_pool( pjmedia_endpt *endpt,
					      const char *name,
					      pj_size_t initial,
					      pj_size_t increment)
{
    pj_assert(endpt != NULL);

    return pj_pool_create(endpt->pf, name, initial, increment, NULL);
}

/**
 * Create a SDP session description that describes the endpoint
 * capability.
 */
PJ_DEF(pj_status_t) pjmedia_endpt_create_sdp( pjmedia_endpt *endpt,
					      pj_pool_t *pool,
					      unsigned stream_cnt,
					      const pjmedia_sock_info sock_info[],
					      pjmedia_sdp_session **p_sdp )
{
    pj_time_val tv;
    unsigned i;
    pjmedia_sdp_session *sdp;
    pjmedia_sdp_media *m;
    pjmedia_sdp_attr *attr;

    /* Sanity check arguments */
    PJ_ASSERT_RETURN(endpt && pool && p_sdp && stream_cnt, PJ_EINVAL);

    /* Check that there are not too many codecs */
    PJ_ASSERT_RETURN(endpt->codec_mgr.codec_cnt <= PJMEDIA_MAX_SDP_FMT,
		     PJ_ETOOMANY);

    /* Create and initialize basic SDP session */
    sdp = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_session));

    pj_gettimeofday(&tv);
    sdp->origin.user = pj_str("-");
    sdp->origin.version = sdp->origin.id = tv.sec + 2208988800UL;
    sdp->origin.net_type = STR_IN;
    sdp->origin.addr_type = STR_IP4;
    sdp->origin.addr = *pj_gethostname();
    sdp->name = STR_SDP_NAME;

    /* Since we only support one media stream at present, put the
     * SDP connection line in the session level.
     */
    sdp->conn = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_conn));
    sdp->conn->net_type = STR_IN;
    sdp->conn->addr_type = STR_IP4;
    pj_strdup2(pool, &sdp->conn->addr, 
	       pj_inet_ntoa(sock_info[0].rtp_addr_name.sin_addr));


    /* SDP time and attributes. */
    sdp->time.start = sdp->time.stop = 0;
    sdp->attr_count = 0;

    /* Create media stream 0: */

    sdp->media_count = 1;
    m = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_media));
    sdp->media[0] = m;

    /* Standard media info: */
    pj_strdup(pool, &m->desc.media, &STR_AUDIO);
    m->desc.port = pj_ntohs(sock_info[0].rtp_addr_name.sin_port);
    m->desc.port_count = 1;
    pj_strdup (pool, &m->desc.transport, &STR_RTP_AVP);

    /* Init media line and attribute list. */
    m->desc.fmt_count = 0;
    m->attr_count = 0;

    /* Add "rtcp" attribute */
#if defined(PJMEDIA_HAS_RTCP_IN_SDP) && PJMEDIA_HAS_RTCP_IN_SDP!=0
    {
	attr = pj_pool_alloc(pool, sizeof(pjmedia_sdp_attr));
	attr->name = pj_str("rtcp");
	attr->value.ptr = pj_pool_alloc(pool, 80);
	attr->value.slen = 
	    pj_ansi_snprintf(attr->value.ptr, 80,
			    ":%u IN IP4 %s",
			    pj_ntohs(sock_info[0].rtcp_addr_name.sin_port),
			    pj_inet_ntoa(sock_info[0].rtcp_addr_name.sin_addr));
	pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
    }
#endif

    /* Add format, rtpmap, and fmtp (when applicable) for each codec */
    for (i=0; i<endpt->codec_mgr.codec_cnt; ++i) {

	pjmedia_codec_info *codec_info;
	pjmedia_sdp_rtpmap rtpmap;
	char tmp_param[3];
	pjmedia_sdp_attr *attr;
	pjmedia_codec_param codec_param;
	pj_str_t *fmt;

	if (endpt->codec_mgr.codec_desc[i].prio == PJMEDIA_CODEC_PRIO_DISABLED)
	    break;

	codec_info = &endpt->codec_mgr.codec_desc[i].info;
	pjmedia_codec_mgr_get_default_param(&endpt->codec_mgr, codec_info,
					    &codec_param);
	fmt = &m->desc.fmt[m->desc.fmt_count++];

	fmt->ptr = pj_pool_alloc(pool, 8);
	fmt->slen = pj_utoa(codec_info->pt, fmt->ptr);

	rtpmap.pt = *fmt;
	rtpmap.clock_rate = codec_info->clock_rate;
	rtpmap.enc_name = codec_info->encoding_name;
	
	/* For audio codecs, rtpmap parameters denotes the number
	 * of channels, which can be omited if the value is 1.
	 */
	if (codec_info->type == PJMEDIA_TYPE_AUDIO &&
	    codec_info->channel_cnt > 1)
	{
	    /* Can only support one digit channel count */
	    pj_assert(codec_info->channel_cnt < 10);

	    tmp_param[0] = '/';
	    tmp_param[1] = (char)('0' + codec_info->channel_cnt);

	    rtpmap.param.ptr = tmp_param;
	    rtpmap.param.slen = 2;

	} else {
	    rtpmap.param.slen = 0;
	}

	pjmedia_sdp_rtpmap_to_attr(pool, &rtpmap, &attr);
	m->attr[m->attr_count++] = attr;

	/* Add fmtp mode where applicable */
	if (codec_param.setting.dec_fmtp_mode != 0) {
	    const pj_str_t fmtp = { "fmtp", 4 };
	    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));

	    attr->name = fmtp;
	    attr->value.ptr = pj_pool_alloc(pool, 32);
	    attr->value.slen = 
		pj_ansi_snprintf( attr->value.ptr, 32,
				  ":%d mode=%d",
				  codec_info->pt,
				  codec_param.setting.dec_fmtp_mode);
	    m->attr[m->attr_count++] = attr;
	}
    }

    /* Add sendrecv attribute. */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = STR_SENDRECV;
    m->attr[m->attr_count++] = attr;

#if defined(PJMEDIA_RTP_PT_TELEPHONE_EVENTS) && \
    PJMEDIA_RTP_PT_TELEPHONE_EVENTS != 0

    /*
     * Add support telephony event
     */
    m->desc.fmt[m->desc.fmt_count++] = 
	pj_str(PJMEDIA_RTP_PT_TELEPHONE_EVENTS_STR);

    /* Add rtpmap. */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("rtpmap");
    attr->value = pj_str(":" PJMEDIA_RTP_PT_TELEPHONE_EVENTS_STR 
			 " telephone-event/8000");
    m->attr[m->attr_count++] = attr;

    /* Add fmtp */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("fmtp");
    attr->value = pj_str(":" PJMEDIA_RTP_PT_TELEPHONE_EVENTS_STR " 0-15");
    m->attr[m->attr_count++] = attr;
#endif

    /* Done */
    *p_sdp = sdp;

    return PJ_SUCCESS;

}



#if PJ_LOG_MAX_LEVEL >= 3
static const char *good_number(char *buf, pj_int32_t val)
{
    if (val < 1000) {
	pj_ansi_sprintf(buf, "%d", val);
    } else if (val < 1000000) {
	pj_ansi_sprintf(buf, "%d.%dK", 
			val / 1000,
			(val % 1000) / 100);
    } else {
	pj_ansi_sprintf(buf, "%d.%02dM", 
			val / 1000000,
			(val % 1000000) / 10000);
    }

    return buf;
}
#endif

PJ_DEF(pj_status_t) pjmedia_endpt_dump(pjmedia_endpt *endpt)
{

#if PJ_LOG_MAX_LEVEL >= 3
    unsigned i, count;
    pjmedia_codec_info codec_info[32];
    unsigned prio[32];

    PJ_LOG(3,(THIS_FILE, "Dumping PJMEDIA capabilities:"));

    count = PJ_ARRAY_SIZE(codec_info);
    if (pjmedia_codec_mgr_enum_codecs(&endpt->codec_mgr, 
				      &count, codec_info, prio) != PJ_SUCCESS)
    {
	PJ_LOG(3,(THIS_FILE, " -error: failed to enum codecs"));
	return PJ_SUCCESS;
    }

    PJ_LOG(3,(THIS_FILE, "  Total number of installed codecs: %d", count));
    for (i=0; i<count; ++i) {
	const char *type;
	pjmedia_codec_param param;
	char bps[32];

	switch (codec_info[i].type) {
	case PJMEDIA_TYPE_AUDIO:
	    type = "Audio"; break;
	case PJMEDIA_TYPE_VIDEO:
	    type = "Video"; break;
	default:
	    type = "Unknown type"; break;
	}

	if (pjmedia_codec_mgr_get_default_param(&endpt->codec_mgr,
						&codec_info[i],
						&param) != PJ_SUCCESS)
	{
	    pj_bzero(&param, sizeof(pjmedia_codec_param));
	}

	PJ_LOG(3,(THIS_FILE, 
		  "   %s codec #%2d: pt=%d (%.*s @%dKHz/%d, %sbps, %dms%s%s%s%s%s)",
		  type, i, codec_info[i].pt,
		  (int)codec_info[i].encoding_name.slen,
		  codec_info[i].encoding_name.ptr,
		  codec_info[i].clock_rate/1000,
		  codec_info[i].channel_cnt,
		  good_number(bps, param.info.avg_bps), 
		  param.info.frm_ptime * param.setting.frm_per_pkt,
		  (param.setting.vad ? " vad" : ""),
		  (param.setting.cng ? " cng" : ""),
		  (param.setting.plc ? " plc" : ""),
		  (param.setting.penh ? " penh" : ""),
		  (prio[i]==PJMEDIA_CODEC_PRIO_DISABLED?" disabled":"")));
    }
#endif

    return PJ_SUCCESS;
}

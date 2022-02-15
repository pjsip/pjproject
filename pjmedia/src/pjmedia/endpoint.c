/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/vid_codec.h>
#include <pjmedia-audiodev/audiodev.h>
#include <pj/assert.h>
#include <pj/ioqueue.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>


#define THIS_FILE   "endpoint.c"

static const pj_str_t STR_IN = { "IN", 2 };
static const pj_str_t STR_IP4 = { "IP4", 3};
static const pj_str_t STR_IP6 = { "IP6", 3};
static const pj_str_t STR_RTP_AVP = { "RTP/AVP", 7 };
static const pj_str_t STR_SDP_NAME = { "pjmedia", 7 };
static const pj_str_t STR_SENDRECV = { "sendrecv", 8 };
static const pj_str_t STR_SENDONLY = { "sendonly", 8 };
static const pj_str_t STR_RECVONLY = { "recvonly", 8 };
static const pj_str_t STR_INACTIVE = { "inactive", 8 };


/* Config to control rtpmap inclusion for static payload types */
pj_bool_t pjmedia_add_rtpmap_for_static_pt = 
	    PJMEDIA_ADD_RTPMAP_FOR_STATIC_PT;

/* Config to control use of RFC3890 TIAS */
pj_bool_t pjmedia_add_bandwidth_tias_in_sdp =
            PJMEDIA_ADD_BANDWIDTH_TIAS_IN_SDP;



/* Worker thread proc. */
static int PJ_THREAD_FUNC worker_proc(void*);


#define MAX_THREADS	16


/* List of media endpoint exit callback. */
typedef struct exit_cb
{
    PJ_DECL_LIST_MEMBER		    (struct exit_cb);
    pjmedia_endpt_exit_callback	    func;
} exit_cb;


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

    /** Is telephone-event enable */
    pj_bool_t		  has_telephone_event;

    /** List of exit callback. */
    exit_cb		  exit_cb_list;
};


PJ_DEF(void)
pjmedia_endpt_create_sdp_param_default(pjmedia_endpt_create_sdp_param *param)
{
    param->dir = PJMEDIA_DIR_ENCODING_DECODING;
}

/**
 * Initialize and get the instance of media endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_endpt_create2(pj_pool_factory *pf,
					  pj_ioqueue_t *ioqueue,
					  unsigned worker_cnt,
					  pjmedia_endpt **p_endpt)
{
    pj_pool_t *pool;
    pjmedia_endpt *endpt;
    unsigned i;
    pj_status_t status;

    status = pj_register_strerror(PJMEDIA_ERRNO_START, PJ_ERRNO_SPACE_SIZE,
				  &pjmedia_strerror);
    pj_assert(status == PJ_SUCCESS);

    PJ_ASSERT_RETURN(pf && p_endpt, PJ_EINVAL);
    PJ_ASSERT_RETURN(worker_cnt <= MAX_THREADS, PJ_EINVAL);

    pool = pj_pool_create(pf, "med-ept", PJMEDIA_POOL_LEN_ENDPT,
						PJMEDIA_POOL_INC_ENDPT, NULL);
    if (!pool)
	return PJ_ENOMEM;

    endpt = PJ_POOL_ZALLOC_T(pool, struct pjmedia_endpt);
    endpt->pool = pool;
    endpt->pf = pf;
    endpt->ioqueue = ioqueue;
    endpt->thread_cnt = worker_cnt;
    endpt->has_telephone_event = PJ_TRUE;

    /* Initialize audio subsystem.
     * To avoid pjmedia's dependendy on pjmedia-audiodev, the initialization
     * (and shutdown) of audio subsystem will be done in the application
     * level instead, when it calls inline functions pjmedia_endpt_create()
     * and pjmedia_endpt_destroy().
     */
    //status = pjmedia_aud_subsys_init(pf);
    //if (status != PJ_SUCCESS)
    //	goto on_error;

    /* Init codec manager. */
    status = pjmedia_codec_mgr_init(&endpt->codec_mgr, endpt->pf);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Initialize exit callback list. */
    pj_list_init(&endpt->exit_cb_list);

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

    pjmedia_codec_mgr_destroy(&endpt->codec_mgr);
    //pjmedia_aud_subsys_shutdown();
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
PJ_DEF(pj_status_t) pjmedia_endpt_destroy2 (pjmedia_endpt *endpt)
{
    exit_cb *ecb;

    pjmedia_endpt_stop_threads(endpt);

    /* Destroy internal ioqueue */
    if (endpt->ioqueue && endpt->own_ioqueue) {
	pj_ioqueue_destroy(endpt->ioqueue);
	endpt->ioqueue = NULL;
    }

    endpt->pf = NULL;

    pjmedia_codec_mgr_destroy(&endpt->codec_mgr);
    //pjmedia_aud_subsys_shutdown();

    /* Call all registered exit callbacks */
    ecb = endpt->exit_cb_list.next;
    while (ecb != &endpt->exit_cb_list) {
	(*ecb->func)(endpt);
	ecb = ecb->next;
    }

    pj_pool_release (endpt->pool);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_endpt_set_flag( pjmedia_endpt *endpt,
					    pjmedia_endpt_flag flag,
					    const void *value)
{
    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    switch (flag) {
    case PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG:
	endpt->has_telephone_event = *(pj_bool_t*)value;
	break;
    default:
	return PJ_EINVAL;
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_endpt_get_flag( pjmedia_endpt *endpt,
					    pjmedia_endpt_flag flag,
					    void *value)
{
    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    switch (flag) {
    case PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG:
	*(pj_bool_t*)value = endpt->has_telephone_event;
	break;
    default:
	return PJ_EINVAL;
    }

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
 * Get the number of worker threads in media endpoint.
 */
PJ_DEF(unsigned) pjmedia_endpt_get_thread_count(pjmedia_endpt *endpt)
{
    PJ_ASSERT_RETURN(endpt, 0);
    return endpt->thread_cnt;
}

/**
 * Get a reference to one of the worker threads of the media endpoint 
 */
PJ_DEF(pj_thread_t*) pjmedia_endpt_get_thread(pjmedia_endpt *endpt, 
					      unsigned index)
{
    PJ_ASSERT_RETURN(endpt, NULL);
    PJ_ASSERT_RETURN(index < endpt->thread_cnt, NULL);

    /* here should be an assert on index >= 0 < endpt->thread_cnt */

    return endpt->thread[index];
}

/**
 * Stop and destroy the worker threads of the media endpoint
 */
PJ_DEF(pj_status_t) pjmedia_endpt_stop_threads(pjmedia_endpt *endpt)
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

    return PJ_SUCCESS;
}

/**
 * Worker thread proc.
 */
static int PJ_THREAD_FUNC worker_proc(void *arg)
{
    pjmedia_endpt *endpt = (pjmedia_endpt*) arg;

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

/* Common initialization for both audio and video SDP media line */
static pj_status_t init_sdp_media(pjmedia_sdp_media *m,
                                  pj_pool_t *pool,
                                  const pj_str_t *media_type,
				  const pjmedia_sock_info *sock_info,
				  pjmedia_dir dir)
{
    char tmp_addr[PJ_INET6_ADDRSTRLEN];
    pjmedia_sdp_attr *attr;
    const pj_sockaddr *addr;

    pj_strdup(pool, &m->desc.media, media_type);

    addr = &sock_info->rtp_addr_name;

    /* Validate address family */
    PJ_ASSERT_RETURN(addr->addr.sa_family == pj_AF_INET() ||
                     addr->addr.sa_family == pj_AF_INET6(),
                     PJ_EAFNOTSUP);

    /* SDP connection line */
    m->conn = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
    m->conn->net_type = STR_IN;
    m->conn->addr_type = (addr->addr.sa_family==pj_AF_INET())? STR_IP4:STR_IP6;
    pj_sockaddr_print(addr, tmp_addr, sizeof(tmp_addr), 0);
    pj_strdup2(pool, &m->conn->addr, tmp_addr);

    /* Port and transport in media description */
    m->desc.port = pj_sockaddr_get_port(addr);
    m->desc.port_count = 1;
    pj_strdup (pool, &m->desc.transport, &STR_RTP_AVP);

    /* Add "rtcp" attribute */
#if defined(PJMEDIA_HAS_RTCP_IN_SDP) && PJMEDIA_HAS_RTCP_IN_SDP!=0
    if (sock_info->rtcp_addr_name.addr.sa_family != 0) {
	attr = pjmedia_sdp_attr_create_rtcp(pool, &sock_info->rtcp_addr_name);
	if (attr)
	    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
    }
#endif

    /* Add direction attribute. */
    if (m->desc.port != 0) {
	attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);
	if (dir == PJMEDIA_DIR_ENCODING) {
	    attr->name = STR_SENDONLY;
	} else if (dir == PJMEDIA_DIR_DECODING) {
	    attr->name = STR_RECVONLY;
	} else if (dir == PJMEDIA_DIR_NONE) {
	    attr->name = STR_INACTIVE;
	} else {
	    attr->name = STR_SENDRECV;
	}
	m->attr[m->attr_count++] = attr;
    }

    return PJ_SUCCESS;
}

/* Create m=audio SDP media line */
PJ_DEF(pj_status_t)
pjmedia_endpt_create_audio_sdp(pjmedia_endpt *endpt,
                               pj_pool_t *pool,
                               const pjmedia_sock_info *si,
                               const pjmedia_endpt_create_sdp_param *options,
                               pjmedia_sdp_media **p_m)
{
    const pj_str_t STR_AUDIO = { "audio", 5 };
    pjmedia_sdp_media *m;
    pjmedia_sdp_attr *attr;
    unsigned i;
    unsigned max_bitrate = 0;
    pj_status_t status;
#if defined(PJMEDIA_RTP_PT_TELEPHONE_EVENTS) && \
	    PJMEDIA_RTP_PT_TELEPHONE_EVENTS != 0
    unsigned televent_num = 0;
    unsigned televent_clockrates[8];
#endif
    unsigned used_pt_num = 0;
    unsigned used_pt[PJMEDIA_MAX_SDP_FMT];
    pjmedia_endpt_create_sdp_param param;

    /* Check that there are not too many codecs */
    PJ_ASSERT_RETURN(endpt->codec_mgr.codec_cnt <= PJMEDIA_MAX_SDP_FMT,
		     PJ_ETOOMANY);

    /* Insert PJMEDIA_RTP_PT_TELEPHONE_EVENTS as used PT */
#if defined(PJMEDIA_RTP_PT_TELEPHONE_EVENTS) && \
	    PJMEDIA_RTP_PT_TELEPHONE_EVENTS != 0
    if (endpt->has_telephone_event) {
	used_pt[used_pt_num++] = PJMEDIA_RTP_PT_TELEPHONE_EVENTS;

#  if PJMEDIA_TELEPHONE_EVENT_ALL_CLOCKRATES==0
	televent_num = 1;
	televent_clockrates[0] = 8000;
#  endif

    }
#endif

    /* Create and init basic SDP media */
    m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
    pjmedia_endpt_create_sdp_param_default(&param);
    status = init_sdp_media(m, pool, &STR_AUDIO, si, options? options->dir:
    			    param.dir);
    if (status != PJ_SUCCESS)
	return status;

    /* Add format, rtpmap, and fmtp (when applicable) for each codec */
    for (i=0; i<endpt->codec_mgr.codec_cnt; ++i) {

	pjmedia_codec_info *codec_info;
	pjmedia_sdp_rtpmap rtpmap;
	char tmp_param[3];
	pjmedia_codec_param codec_param;
	pj_str_t *fmt;
	unsigned pt;

	if (endpt->codec_mgr.codec_desc[i].prio == PJMEDIA_CODEC_PRIO_DISABLED)
	    break;

	codec_info = &endpt->codec_mgr.codec_desc[i].info;
	pjmedia_codec_mgr_get_default_param(&endpt->codec_mgr, codec_info,
					    &codec_param);
	fmt = &m->desc.fmt[m->desc.fmt_count++];
	pt = codec_info->pt;

	/* Rearrange dynamic payload type to make sure it is inside 96-127
	 * range and not being used by other codec/tel-event.
	 */
	if (pt >= 96) {
	    unsigned pt_check = 96;
	    unsigned j = 0;
	    while (j < used_pt_num && pt_check <= 127) {
		if (pt_check==used_pt[j]) {
		    pt_check++;
		    j = 0;
		} else {
		    j++;
		}
	    }
	    if (pt_check > 127) {
		/* No more available PT */
		PJ_LOG(4,(THIS_FILE, "Warning: no available dynamic "
			  "payload type for audio codec"));
		break;
	    }
	    pt = pt_check;
	}

	/* Take a note of used dynamic PT */
	if (pt >= 96)
	    used_pt[used_pt_num++] = pt;

	fmt->ptr = (char*) pj_pool_alloc(pool, 8);
	fmt->slen = pj_utoa(pt, fmt->ptr);

	rtpmap.pt = *fmt;
	rtpmap.enc_name = codec_info->encoding_name;

#if defined(PJMEDIA_HANDLE_G722_MPEG_BUG) && (PJMEDIA_HANDLE_G722_MPEG_BUG != 0)
	if (pt == PJMEDIA_RTP_PT_G722)
	    rtpmap.clock_rate = 8000;
	else
	    rtpmap.clock_rate = codec_info->clock_rate;
#else
	rtpmap.clock_rate = codec_info->clock_rate;
#endif

	/* For audio codecs, rtpmap parameters denotes the number
	 * of channels, which can be omited if the value is 1.
	 */
	if (codec_info->type == PJMEDIA_TYPE_AUDIO &&
	    codec_info->channel_cnt > 1)
	{
	    /* Can only support one digit channel count */
	    pj_assert(codec_info->channel_cnt < 10);

	    tmp_param[0] = (char)('0' + codec_info->channel_cnt);

	    rtpmap.param.ptr = tmp_param;
	    rtpmap.param.slen = 1;

	} else {
	    rtpmap.param.ptr = "";
	    rtpmap.param.slen = 0;
	}

	if (pt >= 96 || pjmedia_add_rtpmap_for_static_pt) {
	    pjmedia_sdp_rtpmap_to_attr(pool, &rtpmap, &attr);
	    m->attr[m->attr_count++] = attr;
	}

	/* Add fmtp params */
	if (codec_param.setting.dec_fmtp.cnt > 0) {
	    enum { MAX_FMTP_STR_LEN = 160 };
	    char buf[MAX_FMTP_STR_LEN];
	    unsigned buf_len = 0, n, ii;
	    pjmedia_codec_fmtp *dec_fmtp = &codec_param.setting.dec_fmtp;

	    /* Print codec PT */
	    n = pj_ansi_snprintf(buf, MAX_FMTP_STR_LEN - buf_len,
				 "%d", pt);
	    buf_len = PJ_MIN(buf_len + n, MAX_FMTP_STR_LEN);

	    for (ii = 0; ii < dec_fmtp->cnt; ++ii) {
		pj_size_t test_len = 2;

		/* Check if buf still available */
		test_len = dec_fmtp->param[ii].val.slen + 
			   dec_fmtp->param[ii].name.slen + 2;
		if (test_len + buf_len >= MAX_FMTP_STR_LEN)
		    return PJ_ETOOBIG;

		/* Print delimiter */
		n = pj_ansi_snprintf(&buf[buf_len], 
				     MAX_FMTP_STR_LEN - buf_len,
				     (ii == 0?" ":";"));
		buf_len = PJ_MIN(buf_len + n, MAX_FMTP_STR_LEN);

		/* Print an fmtp param */
		if (dec_fmtp->param[ii].name.slen)
		    n = pj_ansi_snprintf(&buf[buf_len],
					 MAX_FMTP_STR_LEN - buf_len,
					 "%.*s=%.*s",
					 (int)dec_fmtp->param[ii].name.slen,
					 dec_fmtp->param[ii].name.ptr,
					 (int)dec_fmtp->param[ii].val.slen,
					  dec_fmtp->param[ii].val.ptr);
		else
		    n = pj_ansi_snprintf(&buf[buf_len], 
					 MAX_FMTP_STR_LEN - buf_len,
					 "%.*s", 
					 (int)dec_fmtp->param[ii].val.slen,
					 dec_fmtp->param[ii].val.ptr);
		
		buf_len = PJ_MIN(buf_len + n, MAX_FMTP_STR_LEN);
	    }

	    attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);

	    attr->name = pj_str("fmtp");
	    attr->value = pj_strdup3(pool, buf);
	    m->attr[m->attr_count++] = attr;
	}

	/* Find maximum bitrate in this media */
	if (max_bitrate < codec_param.info.max_bps)
	    max_bitrate = codec_param.info.max_bps;

	/* List clock rate of audio codecs for generating telephone-event */
#if defined(PJMEDIA_RTP_PT_TELEPHONE_EVENTS) && \
	    PJMEDIA_RTP_PT_TELEPHONE_EVENTS != 0 && \
	    PJMEDIA_TELEPHONE_EVENT_ALL_CLOCKRATES != 0
	if (endpt->has_telephone_event) {
	    unsigned j;

	    for (j=0; j<televent_num; ++j) {
		if (televent_clockrates[j] == rtpmap.clock_rate)
		    break;
	    }
	    if (j==televent_num &&
		televent_num<PJ_ARRAY_SIZE(televent_clockrates))
	    {
		/* List this clockrate for tel-event generation */
		televent_clockrates[televent_num++] = rtpmap.clock_rate;
	    }
	}
#endif
    }

#if defined(PJMEDIA_RTP_PT_TELEPHONE_EVENTS) && \
	    PJMEDIA_RTP_PT_TELEPHONE_EVENTS != 0
    /*
     * Add support telephony event
     */
    if (endpt->has_telephone_event) {
	for (i=0; i<televent_num; i++) {
	    char buf[160];
	    unsigned j = 0;
	    unsigned pt;

	    /* Find PT for this tel-event */
	    if (i == 0) {
		/* First telephony-event always uses preconfigured PT
		 * PJMEDIA_RTP_PT_TELEPHONE_EVENTS.
		 */
		pt = PJMEDIA_RTP_PT_TELEPHONE_EVENTS;
	    } else {
		/* Otherwise, find any free PT slot, starting from
		 * (PJMEDIA_RTP_PT_TELEPHONE_EVENTS + 1).
		 */
		pt = PJMEDIA_RTP_PT_TELEPHONE_EVENTS + 1;
		while (j < used_pt_num && pt <= 127) {
		    if (pt == used_pt[j]) {
			pt++;
			j = 0;
		    } else {
			j++;
		    }
		}
		if (pt > 127) {
		    /* Not found? Find more, but now starting from 96 */
		    pt = 96;
		    j = 0;
		    while (j < used_pt_num &&
			   pt < PJMEDIA_RTP_PT_TELEPHONE_EVENTS)
		    {
			if (pt == used_pt[j]) {
			    pt++;
			    j = 0;
			} else {
			    j++;
			}
		    }
		    if (pt >= PJMEDIA_RTP_PT_TELEPHONE_EVENTS) {
			/* No more available PT */
			PJ_LOG(4,(THIS_FILE, "Warning: no available dynamic "
				  "payload type for telephone-event"));
			break;
		    }
		}
	    }
	    used_pt[used_pt_num++] = pt;

	    /* Print tel-event PT */
	    pj_ansi_snprintf(buf, sizeof(buf), "%d", pt);
	    m->desc.fmt[m->desc.fmt_count++] = pj_strdup3(pool, buf);

	    /* Add rtpmap. */
	    attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);
	    attr->name = pj_str("rtpmap");
	    pj_ansi_snprintf(buf, sizeof(buf), "%d telephone-event/%d",
			     pt, televent_clockrates[i]);
	    attr->value = pj_strdup3(pool, buf);
	    m->attr[m->attr_count++] = attr;

	    /* Add fmtp */
	    attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);
	    attr->name = pj_str("fmtp");
#if defined(PJMEDIA_HAS_DTMF_FLASH) && PJMEDIA_HAS_DTMF_FLASH!= 0
	    pj_ansi_snprintf(buf, sizeof(buf), "%d 0-16", pt);
#else
	    pj_ansi_snprintf(buf, sizeof(buf), "%d 0-15", pt);
#endif
	    attr->value = pj_strdup3(pool, buf);
	    m->attr[m->attr_count++] = attr;
	}
    }
#endif

    /* Put bandwidth info in media level using bandwidth modifier "TIAS"
     * (RFC3890).
     */
    if (max_bitrate && pjmedia_add_bandwidth_tias_in_sdp) {
	const pj_str_t STR_BANDW_MODIFIER = { "TIAS", 4 };
	pjmedia_sdp_bandw *b;
	    
	b = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_bandw);
	b->modifier = STR_BANDW_MODIFIER;
	b->value = max_bitrate;
	m->bandw[m->bandw_count++] = b;
    }

    *p_m = m;
    return PJ_SUCCESS;
}


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

/* Create m=video SDP media line */
PJ_DEF(pj_status_t)
pjmedia_endpt_create_video_sdp(pjmedia_endpt *endpt,
                               pj_pool_t *pool,
                               const pjmedia_sock_info *si,
                               const pjmedia_endpt_create_sdp_param *options,
                               pjmedia_sdp_media **p_m)
{


    const pj_str_t STR_VIDEO = { "video", 5 };
    pjmedia_sdp_media *m;
    pjmedia_vid_codec_info codec_info[PJMEDIA_VID_CODEC_MGR_MAX_CODECS];
    unsigned codec_prio[PJMEDIA_VID_CODEC_MGR_MAX_CODECS];
    pjmedia_sdp_attr *attr;
    unsigned cnt, i;
    unsigned max_bitrate = 0;
    pjmedia_endpt_create_sdp_param param;
    pj_status_t status;

    /* Make sure video codec manager is instantiated */
    if (!pjmedia_vid_codec_mgr_instance())
	pjmedia_vid_codec_mgr_create(endpt->pool, NULL);

    /* Create and init basic SDP media */
    pjmedia_endpt_create_sdp_param_default(&param);
    m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
    status = init_sdp_media(m, pool, &STR_VIDEO, si, options? options->dir:
    			    param.dir);
    if (status != PJ_SUCCESS)
	return status;

    cnt = PJ_ARRAY_SIZE(codec_info);
    status = pjmedia_vid_codec_mgr_enum_codecs(NULL, &cnt, 
					       codec_info, codec_prio);

    /* Check that there are not too many codecs */
    PJ_ASSERT_RETURN(0 <= PJMEDIA_MAX_SDP_FMT,
		     PJ_ETOOMANY);

    /* Add format, rtpmap, and fmtp (when applicable) for each codec */
    for (i=0; i<cnt; ++i) {
	pjmedia_sdp_rtpmap rtpmap;
	pjmedia_vid_codec_param codec_param;
	pj_str_t *fmt;
	pjmedia_video_format_detail *vfd;

	pj_bzero(&rtpmap, sizeof(rtpmap));

	if (codec_prio[i] == PJMEDIA_CODEC_PRIO_DISABLED)
	    break;

	if (i > PJMEDIA_MAX_SDP_FMT) {
	    /* Too many codecs, perhaps it is better to tell application by
	     * returning appropriate status code.
	     */
	    PJ_PERROR(3,(THIS_FILE, PJ_ETOOMANY,
			"Skipping some video codecs"));
	    break;
	}

        /* Must support RTP packetization */
        if ((codec_info[i].packings & PJMEDIA_VID_PACKING_PACKETS) == 0)
	{
	    continue;
	}

	pjmedia_vid_codec_mgr_get_default_param(NULL, &codec_info[i],
						&codec_param);

	fmt = &m->desc.fmt[m->desc.fmt_count++];
	fmt->ptr = (char*) pj_pool_alloc(pool, 8);
	fmt->slen = pj_utoa(codec_info[i].pt, fmt->ptr);
	rtpmap.pt = *fmt;

	/* Encoding name */
	rtpmap.enc_name = codec_info[i].encoding_name;

	/* Clock rate */
	rtpmap.clock_rate = codec_info[i].clock_rate;

	if (codec_info[i].pt >= 96 || pjmedia_add_rtpmap_for_static_pt) {
	    pjmedia_sdp_rtpmap_to_attr(pool, &rtpmap, &attr);
	    m->attr[m->attr_count++] = attr;
	}

	/* Add fmtp params */
	if (codec_param.dec_fmtp.cnt > 0) {
	    enum { MAX_FMTP_STR_LEN = 160 };
	    char buf[MAX_FMTP_STR_LEN];
	    unsigned buf_len = 0, n, j;
	    pjmedia_codec_fmtp *dec_fmtp = &codec_param.dec_fmtp;

	    /* Print codec PT */
	    n = pj_ansi_snprintf(buf, MAX_FMTP_STR_LEN - buf_len, 
				 "%d", 
				 codec_info[i].pt);
	    buf_len = PJ_MIN(buf_len + n, MAX_FMTP_STR_LEN);

	    for (j = 0; j < dec_fmtp->cnt; ++j) {
		pj_size_t test_len = 2;

		/* Check if buf still available */
		test_len = dec_fmtp->param[j].val.slen + 
			   dec_fmtp->param[j].name.slen + 2;
		if (test_len + buf_len >= MAX_FMTP_STR_LEN)
		    return PJ_ETOOBIG;

		/* Print delimiter */
		n = pj_ansi_snprintf(&buf[buf_len], 
				     MAX_FMTP_STR_LEN - buf_len,
				     (j == 0?" ":";"));
	    	buf_len = PJ_MIN(buf_len + n, MAX_FMTP_STR_LEN);

		/* Print an fmtp param */
		if (dec_fmtp->param[j].name.slen)
		    n = pj_ansi_snprintf(&buf[buf_len],
					 MAX_FMTP_STR_LEN - buf_len,
					 "%.*s=%.*s",
					 (int)dec_fmtp->param[j].name.slen,
					 dec_fmtp->param[j].name.ptr,
					 (int)dec_fmtp->param[j].val.slen,
					 dec_fmtp->param[j].val.ptr);
		else
		    n = pj_ansi_snprintf(&buf[buf_len], 
					 MAX_FMTP_STR_LEN - buf_len,
					 "%.*s", 
					 (int)dec_fmtp->param[j].val.slen,
					 dec_fmtp->param[j].val.ptr);
		
		buf_len = PJ_MIN(buf_len + n, MAX_FMTP_STR_LEN);
	    }

	    attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);

	    attr->name = pj_str("fmtp");
	    attr->value = pj_strdup3(pool, buf);
	    m->attr[m->attr_count++] = attr;
	}
    
	/* Find maximum bitrate in this media */
	vfd = pjmedia_format_get_video_format_detail(&codec_param.enc_fmt,
						     PJ_TRUE);
	if (vfd && max_bitrate < vfd->max_bps)
	    max_bitrate = vfd->max_bps;
    }

    /* Put bandwidth info in media level using bandwidth modifier "TIAS"
     * (RFC3890).
     */
    if (max_bitrate && pjmedia_add_bandwidth_tias_in_sdp) {
	const pj_str_t STR_BANDW_MODIFIER = { "TIAS", 4 };
	pjmedia_sdp_bandw *b;
	    
	b = PJ_POOL_ALLOC_T(pool, pjmedia_sdp_bandw);
	b->modifier = STR_BANDW_MODIFIER;
	b->value = max_bitrate;
	m->bandw[m->bandw_count++] = b;
    }

    *p_m = m;
    return PJ_SUCCESS;
}

#endif /* PJMEDIA_HAS_VIDEO */


/**
 * Create a "blank" SDP session description. The SDP will contain basic SDP
 * fields such as origin, time, and name, but without any media lines.
 */
PJ_DEF(pj_status_t) pjmedia_endpt_create_base_sdp( pjmedia_endpt *endpt,
						   pj_pool_t *pool,
						   const pj_str_t *sess_name,
						   const pj_sockaddr *origin,
						   pjmedia_sdp_session **p_sdp)
{
    char tmp_addr[PJ_INET6_ADDRSTRLEN];
    pj_time_val tv;
    pjmedia_sdp_session *sdp;

    /* Sanity check arguments */
    PJ_ASSERT_RETURN(endpt && pool && p_sdp, PJ_EINVAL);

    sdp = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_session);

    pj_gettimeofday(&tv);
    sdp->origin.user = pj_str("-");
    sdp->origin.version = sdp->origin.id = tv.sec + 2208988800UL;
    sdp->origin.net_type = STR_IN;

    if (origin->addr.sa_family == pj_AF_INET()) {
 	sdp->origin.addr_type = STR_IP4;
    } else if (origin->addr.sa_family == pj_AF_INET6()) {
 	sdp->origin.addr_type = STR_IP6;
    } else {
 	pj_assert(!"Invalid address family");
 	return PJ_EAFNOTSUP;
    }

    pj_strdup2(pool, &sdp->origin.addr,
 	       pj_sockaddr_print(origin, tmp_addr, sizeof(tmp_addr), 0));

    if (sess_name)
	pj_strdup(pool, &sdp->name, sess_name);
    else
	sdp->name = STR_SDP_NAME;

    /* SDP time and attributes. */
    sdp->time.start = sdp->time.stop = 0;
    sdp->attr_count = 0;

    /* Done */
    *p_sdp = sdp;

    return PJ_SUCCESS;
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
    const pj_sockaddr *addr0;
    pjmedia_sdp_session *sdp;
    pjmedia_sdp_media *m;
    pj_status_t status;

    /* Sanity check arguments */
    PJ_ASSERT_RETURN(endpt && pool && p_sdp && stream_cnt, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_cnt < PJMEDIA_MAX_SDP_MEDIA, PJ_ETOOMANY);

    addr0 = &sock_info[0].rtp_addr_name;

    /* Create and initialize basic SDP session */
    status = pjmedia_endpt_create_base_sdp(endpt, pool, NULL, addr0, &sdp);
    if (status != PJ_SUCCESS)
	return status;

    /* Audio is first, by convention */
    status = pjmedia_endpt_create_audio_sdp(endpt, pool,
                                            &sock_info[0], 0, &m);
    if (status != PJ_SUCCESS)
	return status;
    sdp->media[sdp->media_count++] = m;

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    {
	unsigned i;

	/* The remaining stream, if any, are videos (by convention as well) */
	for (i=1; i<stream_cnt; ++i) {
	    status = pjmedia_endpt_create_video_sdp(endpt, pool,
						    &sock_info[i], 0, &m);
	    if (status != PJ_SUCCESS)
		return status;
	    sdp->media[sdp->media_count++] = m;
	}
    }
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

PJ_DEF(pj_status_t) pjmedia_endpt_atexit( pjmedia_endpt *endpt,
					  pjmedia_endpt_exit_callback func)
{
    exit_cb *new_cb;

    PJ_ASSERT_RETURN(endpt && func, PJ_EINVAL);

    if (endpt->quit_flag)
	return PJ_EINVALIDOP;

    new_cb = PJ_POOL_ZALLOC_T(endpt->pool, exit_cb);
    new_cb->func = func;

    pj_enter_critical_section();
    pj_list_push_back(&endpt->exit_cb_list, new_cb);
    pj_leave_critical_section();

    return PJ_SUCCESS;
}

/* $Id$ */
/* 
 * Copyright (C) 2011-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>

#define THIS_FILE	"pjsua_vid.c"

#if PJSUA_HAS_VIDEO

static void free_vid_win(pjsua_vid_win_id wid);

/*****************************************************************************
 * pjsua video subsystem.
 */
pj_status_t pjsua_vid_subsys_init(void)
{
    unsigned i;
    pj_status_t status;

    status = pjmedia_video_format_mgr_create(pjsua_var.pool, 64, 0, NULL);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status,
		     "Error creating PJMEDIA video format manager"));
	return status;
    }

    status = pjmedia_converter_mgr_create(pjsua_var.pool, NULL);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status,
		     "Error creating PJMEDIA converter manager"));
	return status;
    }

    status = pjmedia_vid_codec_mgr_create(pjsua_var.pool, NULL);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status,
		     "Error creating PJMEDIA video codec manager"));
	return status;
    }

    status = pjmedia_vid_dev_subsys_init(&pjsua_var.cp.factory);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status,
		     "Error creating PJMEDIA video subsystem"));
	return status;
    }

#if PJMEDIA_HAS_VIDEO && PJMEDIA_HAS_FFMPEG_CODEC
    status = pjmedia_codec_ffmpeg_init(NULL, &pjsua_var.cp.factory);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status,
		     "Error initializing ffmpeg library"));
	return status;
    }
#endif

    for (i=0; i<PJSUA_MAX_VID_WINS; ++i) {
	if (pjsua_var.win[i].pool == NULL) {
	    pjsua_var.win[i].pool = pjsua_pool_create("win%p", 512, 512);
	    if (pjsua_var.win[i].pool == NULL)
		return PJ_ENOMEM;
	}
    }

    return PJ_SUCCESS;
}

pj_status_t pjsua_vid_subsys_start(void)
{
    return PJ_SUCCESS;
}

pj_status_t pjsua_vid_subsys_destroy(void)
{
    unsigned i;

    for (i=0; i<PJSUA_MAX_VID_WINS; ++i) {
	if (pjsua_var.win[i].pool) {
	    free_vid_win(i);
	    pj_pool_release(pjsua_var.win[i].pool);
	    pjsua_var.win[i].pool = NULL;
	}
    }

    pjmedia_vid_dev_subsys_shutdown();

#if PJMEDIA_HAS_FFMPEG_CODEC
	    pjmedia_codec_ffmpeg_deinit();
#endif

    return PJ_SUCCESS;
}


/*****************************************************************************
 * Devices.
 */

/*
 * Get the number of video devices installed in the system.
 */
PJ_DEF(unsigned) pjsua_vid_dev_count(void)
{
    return pjmedia_vid_dev_count();
}

/*
 * Retrieve the video device info for the specified device index.
 */
PJ_DEF(pj_status_t) pjsua_vid_dev_get_info(pjmedia_vid_dev_index id,
                                           pjmedia_vid_dev_info *vdi)
{
    return pjmedia_vid_dev_get_info(id, vdi);
}

/*
 * Enum all video devices installed in the system.
 */
PJ_DEF(pj_status_t) pjsua_vid_enum_devs(pjmedia_vid_dev_info info[],
					unsigned *count)
{
    unsigned i, dev_count;

    dev_count = pjmedia_vid_dev_count();

    if (dev_count > *count) dev_count = *count;

    for (i=0; i<dev_count; ++i) {
	pj_status_t status;

	status = pjmedia_vid_dev_get_info(i, &info[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    *count = dev_count;

    return PJ_SUCCESS;
}


/*****************************************************************************
 * Codecs.
 */

/*
 * Enum all supported video codecs in the system.
 */
PJ_DEF(pj_status_t) pjsua_vid_enum_codecs( pjsua_codec_info id[],
					   unsigned *p_count )
{
    pjmedia_vid_codec_info info[32];
    unsigned i, j, count, prio[32];
    pj_status_t status;

    count = PJ_ARRAY_SIZE(info);
    status = pjmedia_vid_codec_mgr_enum_codecs(NULL, &count, info, prio);
    if (status != PJ_SUCCESS) {
	*p_count = 0;
	return status;
    }

    for (i=0, j=0; i<count && j<*p_count; ++i) {
	if (info[i].has_rtp_pack) {
	    pj_bzero(&id[j], sizeof(pjsua_codec_info));

	    pjmedia_vid_codec_info_to_id(&info[i], id[j].buf_, sizeof(id[j].buf_));
	    id[j].codec_id = pj_str(id[j].buf_);
	    id[j].priority = (pj_uint8_t) prio[i];

	    if (id[j].codec_id.slen < sizeof(id[j].buf_)) {
		id[j].desc.ptr = id[j].codec_id.ptr + id[j].codec_id.slen + 1;
		pj_strncpy(&id[j].desc, &info[i].encoding_desc,
			   sizeof(id[j].buf_) - id[j].codec_id.slen - 1);
	    }

	    ++j;
	}
    }

    *p_count = j;

    return PJ_SUCCESS;
}


/*
 * Change video codec priority.
 */
PJ_DEF(pj_status_t) pjsua_vid_codec_set_priority( const pj_str_t *codec_id,
						  pj_uint8_t priority )
{
    const pj_str_t all = { NULL, 0 };

    if (codec_id->slen==1 && *codec_id->ptr=='*')
	codec_id = &all;

    return pjmedia_vid_codec_mgr_set_codec_priority(NULL, codec_id,
						    priority);
}


/*
 * Get video codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_vid_codec_get_param(
					const pj_str_t *codec_id,
					pjmedia_vid_codec_param *param)
{
    const pj_str_t all = { NULL, 0 };
    const pjmedia_vid_codec_info *info;
    unsigned count = 1;
    pj_status_t status;

    if (codec_id->slen==1 && *codec_id->ptr=='*')
	codec_id = &all;

    status = pjmedia_vid_codec_mgr_find_codecs_by_id(NULL, codec_id,
						     &count, &info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    if (count != 1)
	return (count > 1? PJ_ETOOMANY : PJ_ENOTFOUND);

    status = pjmedia_vid_codec_mgr_get_default_param(NULL, info, param);
    return status;
}


/*
 * Set video codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_vid_codec_set_param(
					const pj_str_t *codec_id,
					const pjmedia_vid_codec_param *param)
{
    const pjmedia_vid_codec_info *info[2];
    unsigned count = 2;
    pj_status_t status;

    status = pjmedia_vid_codec_mgr_find_codecs_by_id(NULL, codec_id,
						     &count, info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    /* Codec ID should be specific */
    if (count > 1) {
	pj_assert(!"Codec ID is not specific");
	return PJ_ETOOMANY;
    }

    status = pjmedia_vid_codec_mgr_set_default_param(NULL, pjsua_var.pool,
						     info[0], param);
    return status;
}


/*****************************************************************************
 * Preview
 */

/*
 * Get the preview window handle associated with the capture device, if any.
 */
PJ_DEF(pjsua_vid_win_id) pjsua_vid_preview_get_win(pjmedia_vid_dev_index id)
{
    pjsua_vid_win_id wid = PJSUA_INVALID_ID;
    unsigned i;

    PJSUA_LOCK();

    /* Get real capture ID, if set to PJMEDIA_VID_DEFAULT_CAPTURE_DEV */
    if (id == PJMEDIA_VID_DEFAULT_CAPTURE_DEV) {
	pjmedia_vid_dev_info info;
	pjmedia_vid_dev_get_info(id, &info);
	id = info.id;
    }

    for (i=0; i<PJSUA_MAX_VID_WINS; ++i) {
	pjsua_vid_win *w = &pjsua_var.win[i];
	if (w->type == PJSUA_WND_TYPE_PREVIEW && w->preview_cap_id == id) {
	    wid = i;
	    break;
	}
    }
    PJSUA_UNLOCK();

    return wid;
}


/* Allocate and initialize pjsua video window:
 * - If the type is preview, video capture, tee, and render
 *   will be instantiated.
 * - If the type is stream, only renderer will be created.
 */
static pj_status_t create_vid_win(pjsua_vid_win_type type,
				  const pjmedia_format *fmt,
				  pjmedia_vid_dev_index rend_id,
				  pjmedia_vid_dev_index cap_id,
				  pj_bool_t show,
				  pjsua_vid_win_id *id)
{
    pjsua_vid_win_id wid = PJSUA_INVALID_ID;
    pjsua_vid_win *w = NULL;
    pjmedia_vid_port_param vp_param;
    pjmedia_format fmt_;
    pj_status_t status;
    unsigned i;

    /* If type is preview, check if it exists already */
    if (type == PJSUA_WND_TYPE_PREVIEW) {
	wid = pjsua_vid_preview_get_win(cap_id);
	if (wid != PJSUA_INVALID_ID) {
	    /* Yes, it exists */

	    /* Show window if requested */
	    if (show) {
		pjmedia_vid_dev_stream *rdr;
		pj_bool_t hide = PJ_FALSE;
		
		rdr = pjmedia_vid_port_get_stream(pjsua_var.win[wid].vp_rend);
		pj_assert(rdr);
		status = pjmedia_vid_dev_stream_set_cap(
					rdr,
					PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE,
					&hide);
	    }

	    /* Done */
	    *id = wid;
	    return PJ_SUCCESS;
	}
    }

    /* Allocate window */
    for (i=0; i<PJSUA_MAX_VID_WINS; ++i) {
	w = &pjsua_var.win[i];
	if (w->type == PJSUA_WND_TYPE_NONE) {
	    wid = i;
	    w->type = type;
	    break;
	}
    }
    if (i == PJSUA_MAX_VID_WINS)
	return PJ_ETOOMANY;

    /* Initialize window */
    pjmedia_vid_port_param_default(&vp_param);

    if (w->type == PJSUA_WND_TYPE_PREVIEW) {
	status = pjmedia_vid_dev_default_param(w->pool, cap_id,
					       &vp_param.vidparam);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Normalize capture ID, in case it was set to
	 * PJMEDIA_VID_DEFAULT_CAPTURE_DEV
	 */
	cap_id = vp_param.vidparam.cap_id;

	/* Assign preview capture device ID */
	w->preview_cap_id = cap_id;

	/* Create capture video port */
	vp_param.active = PJ_TRUE;
	vp_param.vidparam.dir = PJMEDIA_DIR_CAPTURE;
	if (fmt)
	    vp_param.vidparam.fmt = *fmt;

	status = pjmedia_vid_port_create(w->pool, &vp_param, &w->vp_cap);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Update format info */
	fmt_ = vp_param.vidparam.fmt;
	fmt = &fmt_;

	/* Create video tee */
	status = pjmedia_vid_tee_create(w->pool, fmt, 2, &w->tee);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Create renderer video port */
    status = pjmedia_vid_dev_default_param(w->pool, rend_id,
					   &vp_param.vidparam);
    if (status != PJ_SUCCESS)
	goto on_error;

    vp_param.active = (w->type == PJSUA_WND_TYPE_STREAM);
    vp_param.vidparam.dir = PJMEDIA_DIR_RENDER;
    vp_param.vidparam.fmt = *fmt;
    vp_param.vidparam.disp_size = fmt->det.vid.size;
    vp_param.vidparam.flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE;
    vp_param.vidparam.window_hide = !show;

    status = pjmedia_vid_port_create(w->pool, &vp_param, &w->vp_rend);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* For preview window, connect capturer & renderer (via tee) */
    if (w->type == PJSUA_WND_TYPE_PREVIEW) {
	pjmedia_port *rend_port;

	status = pjmedia_vid_port_connect(w->vp_cap, w->tee, PJ_FALSE);
	if (status != PJ_SUCCESS)
	    goto on_error;

	rend_port = pjmedia_vid_port_get_passive_port(w->vp_rend);
	status = pjmedia_vid_tee_add_dst_port2(w->tee, 0, rend_port);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Done */
    *id = wid;

    return PJ_SUCCESS;

on_error:
    free_vid_win(wid);
    return status;
}


static void free_vid_win(pjsua_vid_win_id wid)
{
    pjsua_vid_win *w = &pjsua_var.win[wid];
    
    if (w->vp_cap) {
	pjmedia_vid_port_stop(w->vp_cap);
	pjmedia_vid_port_disconnect(w->vp_cap);
	pjmedia_vid_port_destroy(w->vp_cap);
    }
    if (w->vp_rend) {
	pjmedia_vid_port_stop(w->vp_rend);
	pjmedia_vid_port_destroy(w->vp_rend);
    }
    if (w->tee) {
	pjmedia_port_destroy(w->tee);
    }
    pjsua_vid_win_reset(wid);
}


static void inc_vid_win(pjsua_vid_win_id wid)
{
    pjsua_vid_win *w;
    
    pj_assert(wid >= 0 && wid < PJSUA_MAX_VID_WINS);

    w = &pjsua_var.win[wid];
    pj_assert(w->type != PJSUA_WND_TYPE_NONE);
    ++w->ref_cnt;
}

static void dec_vid_win(pjsua_vid_win_id wid)
{
    pjsua_vid_win *w;
    
    pj_assert(wid >= 0 && wid < PJSUA_MAX_VID_WINS);

    w = &pjsua_var.win[wid];
    pj_assert(w->type != PJSUA_WND_TYPE_NONE);
    if (--w->ref_cnt == 0)
	free_vid_win(wid);
}


/* Internal function: update video channel after SDP negotiation */
pj_status_t video_channel_update(pjsua_call_media *call_med,
                                 pj_pool_t *tmp_pool,
			         const pjmedia_sdp_session *local_sdp,
			         const pjmedia_sdp_session *remote_sdp)
{
    pjsua_call *call = call_med->call;
    pjsua_acc  *acc  = &pjsua_var.acc[call->acc_id];
    pjmedia_vid_stream_info the_si, *si = &the_si;
    pjmedia_port *media_port;
    unsigned strm_idx = call_med->idx;
    pj_status_t status;
    
    status = pjmedia_vid_stream_info_from_sdp(si, tmp_pool, pjsua_var.med_endpt,
					      local_sdp, remote_sdp, strm_idx);
    if (status != PJ_SUCCESS)
	return status;

    /* Check if no media is active */
    if (si->dir == PJMEDIA_DIR_NONE) {
	/* Call media state */
	call_med->state = PJSUA_CALL_MEDIA_NONE;

	/* Call media direction */
	call_med->dir = PJMEDIA_DIR_NONE;

    } else {
	pjmedia_transport_info tp_info;

	/* Start/restart media transport */
	status = pjmedia_transport_media_start(call_med->tp,
					       tmp_pool, local_sdp,
					       remote_sdp, strm_idx);
	if (status != PJ_SUCCESS)
	    return status;

	call_med->tp_st = PJSUA_MED_TP_RUNNING;

	/* Get remote SRTP usage policy */
	pjmedia_transport_info_init(&tp_info);
	pjmedia_transport_get_info(call_med->tp, &tp_info);
	if (tp_info.specific_info_cnt > 0) {
	    unsigned i;
	    for (i = 0; i < tp_info.specific_info_cnt; ++i) {
		if (tp_info.spc_info[i].type == PJMEDIA_TRANSPORT_TYPE_SRTP) 
		{
		    pjmedia_srtp_info *srtp_info = 
				(pjmedia_srtp_info*) tp_info.spc_info[i].buffer;

		    call_med->rem_srtp_use = srtp_info->peer_use;
		    break;
		}
	    }
	}

	/* Optionally, application may modify other stream settings here
	 * (such as jitter buffer parameters, codec ptime, etc.)
	 */
	si->jb_init = pjsua_var.media_cfg.jb_init;
	si->jb_min_pre = pjsua_var.media_cfg.jb_min_pre;
	si->jb_max_pre = pjsua_var.media_cfg.jb_max_pre;
	si->jb_max = pjsua_var.media_cfg.jb_max;

	/* Set SSRC */
	si->ssrc = call_med->ssrc;

	/* Set RTP timestamp & sequence, normally these value are intialized
	 * automatically when stream session created, but for some cases (e.g:
	 * call reinvite, call update) timestamp and sequence need to be kept
	 * contigue.
	 */
	si->rtp_ts = call_med->rtp_tx_ts;
	si->rtp_seq = call_med->rtp_tx_seq;
	si->rtp_seq_ts_set = call_med->rtp_tx_seq_ts_set;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
	/* Enable/disable stream keep-alive and NAT hole punch. */
	si->use_ka = pjsua_var.acc[call->acc_id].cfg.use_stream_ka;
#endif

	/* Try to get shared format ID between the capture device and 
	 * the encoder to avoid format conversion in the capture device.
	 */
	if (si->dir & PJMEDIA_DIR_ENCODING) {
	    pjmedia_vid_dev_info dev_info;
	    pjmedia_vid_codec_info *codec_info = &si->codec_info;
	    unsigned i, j;

	    status = pjmedia_vid_dev_get_info(call_med->strm.v.cap_dev,
					      &dev_info);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Find matched format ID */
	    for (i = 0; i < codec_info->dec_fmt_id_cnt; ++i) {
		for (j = 0; j < dev_info.fmt_cnt; ++j) {
		    if (codec_info->dec_fmt_id[i] == 
			(pjmedia_format_id)dev_info.fmt[j].id)
		    {
			/* Apply the matched format ID to the codec */
			si->codec_param->dec_fmt.id = 
						codec_info->dec_fmt_id[i];

			/* Force outer loop to break */
			i = codec_info->dec_fmt_id_cnt;
			break;
		    }
		}
	    }
	}

	/* Create session based on session info. */
	status = pjmedia_vid_stream_create(pjsua_var.med_endpt, NULL, si,
					   call_med->tp, NULL,
					   &call_med->strm.v.stream);
	if (status != PJ_SUCCESS)
	    return status;

	/* Start stream */
	status = pjmedia_vid_stream_start(call_med->strm.v.stream);
	if (status != PJ_SUCCESS)
	    return status;

	/* Setup decoding direction */
	if (si->dir & PJMEDIA_DIR_DECODING)
	{
	    pjsua_vid_win_id wid;
	    pjsua_vid_win *w;

	    status = pjmedia_vid_stream_get_port(call_med->strm.v.stream,
						 PJMEDIA_DIR_DECODING,
						 &media_port);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Create stream video window */
	    status = create_vid_win(PJSUA_WND_TYPE_STREAM,
				    &media_port->info.fmt,
				    call_med->strm.v.rdr_dev,
				    //acc->cfg.vid_rend_dev,
				    PJSUA_INVALID_ID,
				    acc->cfg.vid_in_auto_show,
				    &wid);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Register to video events */
	    pjmedia_event_subscribe(
		    pjmedia_vid_port_get_event_publisher(w->vp_rend),
		    &call_med->esub);

	    w = &pjsua_var.win[wid];
	    
	    /* Connect renderer to stream */
	    status = pjmedia_vid_port_connect(w->vp_rend, media_port,
					      PJ_FALSE);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Start renderer */
	    status = pjmedia_vid_port_start(w->vp_rend);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Done */
	    inc_vid_win(wid);
	    call_med->strm.v.rdr_win_id = wid;
	}

	/* Setup encoding direction */
	if (si->dir & PJMEDIA_DIR_ENCODING && !call->local_hold)
	{
	    pjsua_vid_win *w;
	    pjsua_vid_win_id wid;

	    status = pjmedia_vid_stream_get_port(call_med->strm.v.stream,
						 PJMEDIA_DIR_ENCODING,
						 &media_port);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Create preview video window */
	    status = create_vid_win(PJSUA_WND_TYPE_PREVIEW,
				    &media_port->info.fmt,
				    call_med->strm.v.rdr_dev,
				    call_med->strm.v.cap_dev,
				    //acc->cfg.vid_rend_dev,
				    //acc->cfg.vid_cap_dev,
				    PJ_FALSE,
				    &wid);
	    if (status != PJ_SUCCESS)
		return status;

	    w = &pjsua_var.win[wid];
	    
	    /* Connect stream to capturer (via video window tee) */
	    status = pjmedia_vid_tee_add_dst_port2(w->tee, 0, media_port);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Start renderer */
	    status = pjmedia_vid_port_start(w->vp_rend);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Start capturer */
	    status = pjmedia_vid_port_start(w->vp_cap);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Done */
	    inc_vid_win(wid);
	    call_med->strm.v.cap_win_id = wid;
	}

	/* Call media direction */
	call_med->dir = si->dir;

	/* Call media state */
	if (call->local_hold)
	    call_med->state = PJSUA_CALL_MEDIA_LOCAL_HOLD;
	else if (call_med->dir == PJMEDIA_DIR_DECODING)
	    call_med->state = PJSUA_CALL_MEDIA_REMOTE_HOLD;
	else
	    call_med->state = PJSUA_CALL_MEDIA_ACTIVE;
    }

    /* Print info. */
    {
	char info[80];
	int info_len = 0;
	int len;
	const char *dir;

	switch (si->dir) {
	case PJMEDIA_DIR_NONE:
	    dir = "inactive";
	    break;
	case PJMEDIA_DIR_ENCODING:
	    dir = "sendonly";
	    break;
	case PJMEDIA_DIR_DECODING:
	    dir = "recvonly";
	    break;
	case PJMEDIA_DIR_ENCODING_DECODING:
	    dir = "sendrecv";
	    break;
	default:
	    dir = "unknown";
	    break;
	}
	len = pj_ansi_sprintf( info+info_len,
			       ", stream #%d: %.*s (%s)", strm_idx,
			       (int)si->codec_info.encoding_name.slen,
			       si->codec_info.encoding_name.ptr,
			       dir);
	if (len > 0)
	    info_len += len;
	PJ_LOG(4,(THIS_FILE,"Media updates%s", info));
    }

    if (!acc->cfg.vid_out_auto_transmit && call_med->strm.v.stream) {
	status = pjmedia_vid_stream_pause(call_med->strm.v.stream,
					  PJMEDIA_DIR_ENCODING);
	if (status != PJ_SUCCESS)
	    return status;
    }

    return PJ_SUCCESS;
}


/* Internal function to stop video stream */
void stop_video_stream(pjsua_call_media *call_med)
{
    pjmedia_vid_stream *strm = call_med->strm.v.stream;
    pjmedia_rtcp_stat stat;

    pj_assert(call_med->type == PJMEDIA_TYPE_VIDEO);

    if (!strm)
	return;

    if (call_med->strm.v.cap_win_id != PJSUA_INVALID_ID) {
	pjmedia_port *media_port;
	pjsua_vid_win *w =
		    &pjsua_var.win[call_med->strm.v.cap_win_id];

	pjmedia_vid_stream_get_port(call_med->strm.v.stream,
				    PJMEDIA_DIR_ENCODING,
				    &media_port);
	pj_assert(media_port);

	pjmedia_vid_port_stop(w->vp_cap);
	pjmedia_vid_tee_remove_dst_port(w->tee, media_port);
	pjmedia_vid_port_start(w->vp_cap);

	dec_vid_win(call_med->strm.v.cap_win_id);
    }

    if (call_med->strm.v.rdr_win_id != PJSUA_INVALID_ID) {
	dec_vid_win(call_med->strm.v.rdr_win_id);
    }

    if ((call_med->dir & PJMEDIA_DIR_ENCODING) &&
	(pjmedia_vid_stream_get_stat(strm, &stat) == PJ_SUCCESS))
    {
	/* Save RTP timestamp & sequence, so when media session is
	 * restarted, those values will be restored as the initial
	 * RTP timestamp & sequence of the new media session. So in
	 * the same call session, RTP timestamp and sequence are
	 * guaranteed to be contigue.
	 */
	call_med->rtp_tx_seq_ts_set = 1 | (1 << 1);
	call_med->rtp_tx_seq = stat.rtp_tx_last_seq;
	call_med->rtp_tx_ts = stat.rtp_tx_last_ts;
    }

    pjmedia_vid_stream_destroy(strm);
    call_med->strm.v.stream = NULL;
}


/*
 * Start video preview window for the specified capture device.
 */
PJ_DEF(pj_status_t) pjsua_vid_preview_start(pjmedia_vid_dev_index id,
                                            pjsua_vid_preview_param *prm)
{
    pjsua_vid_win_id wid;
    pjsua_vid_win *w;
    pjmedia_vid_dev_index rend_id;
    pj_status_t status;

    PJSUA_LOCK();

    if (prm) {
	rend_id = prm->rend_id;
    } else {
	rend_id = PJMEDIA_VID_DEFAULT_RENDER_DEV;
    }

    status = create_vid_win(PJSUA_WND_TYPE_PREVIEW, NULL, rend_id, id,
			    PJ_TRUE, &wid);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK();
	return status;
    }

    w = &pjsua_var.win[wid];

    /* Start capturer */
    status = pjmedia_vid_port_start(w->vp_rend);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK();
	return status;
    }

    /* Start renderer */
    status = pjmedia_vid_port_start(w->vp_cap);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK();
	return status;
    }

    inc_vid_win(wid);

    PJSUA_UNLOCK();
    return PJ_SUCCESS;
}

/*
 * Stop video preview.
 */
PJ_DEF(pj_status_t) pjsua_vid_preview_stop(pjmedia_vid_dev_index id)
{
    pjsua_vid_win_id wid = PJSUA_INVALID_ID;

    PJSUA_LOCK();
    wid = pjsua_vid_preview_get_win(id);
    if (wid == PJSUA_INVALID_ID) {
	PJSUA_UNLOCK();
	return PJ_ENOTFOUND;
    }

    dec_vid_win(wid);

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*****************************************************************************
 * Window
 */


/*
 * Enumerates all video windows.
 */
PJ_DEF(pj_status_t) pjsua_vid_enum_wins( pjsua_vid_win_id wids[],
					 unsigned *count)
{
    unsigned i, cnt;

    cnt = 0;

    for (i=0; i<PJSUA_MAX_VID_WINS && cnt <*count; ++i) {
	pjsua_vid_win *w = &pjsua_var.win[i];
	if (w->type != PJSUA_WND_TYPE_NONE)
	    wids[cnt++] = i;
    }

    *count = cnt;

    return PJ_SUCCESS;
}


/*
 * Get window info.
 */
PJ_DEF(pj_status_t) pjsua_vid_win_get_info( pjsua_vid_win_id wid,
                                            pjsua_vid_win_info *wi)
{
    pjsua_vid_win *w;
    pjmedia_vid_dev_stream *s;
    pjmedia_vid_param vparam;
    pj_status_t status;

    PJ_ASSERT_RETURN(wid >= 0 && wid < PJSUA_MAX_VID_WINS && wi, PJ_EINVAL);

    PJSUA_LOCK();
    w = &pjsua_var.win[wid];
    if (w->vp_rend == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    s = pjmedia_vid_port_get_stream(w->vp_rend);
    if (s == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    status = pjmedia_vid_dev_stream_get_param(s, &vparam);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK();
	return status;
    }

    wi->show = !vparam.window_hide;
    wi->pos  = vparam.window_pos;
    wi->size = vparam.disp_size;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}

/*
 * Show or hide window.
 */
PJ_DEF(pj_status_t) pjsua_vid_win_set_show( pjsua_vid_win_id wid,
                                            pj_bool_t show)
{
    pjsua_vid_win *w;
    pjmedia_vid_dev_stream *s;
    pj_bool_t hide;
    pj_status_t status;

    PJ_ASSERT_RETURN(wid >= 0 && wid < PJSUA_MAX_VID_WINS, PJ_EINVAL);

    PJSUA_LOCK();
    w = &pjsua_var.win[wid];
    if (w->vp_rend == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    s = pjmedia_vid_port_get_stream(w->vp_rend);
    if (s == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    hide = !show;
    status = pjmedia_vid_dev_stream_set_cap(s,
			    PJMEDIA_VID_DEV_CAP_OUTPUT_HIDE, &hide);

    PJSUA_UNLOCK();

    return status;
}

/*
 * Set video window position.
 */
PJ_DEF(pj_status_t) pjsua_vid_win_set_pos( pjsua_vid_win_id wid,
                                           const pjmedia_coord *pos)
{
    pjsua_vid_win *w;
    pjmedia_vid_dev_stream *s;
    pj_status_t status;

    PJ_ASSERT_RETURN(wid >= 0 && wid < PJSUA_MAX_VID_WINS && pos, PJ_EINVAL);

    PJSUA_LOCK();
    w = &pjsua_var.win[wid];
    if (w->vp_rend == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    s = pjmedia_vid_port_get_stream(w->vp_rend);
    if (s == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    status = pjmedia_vid_dev_stream_set_cap(s,
			    PJMEDIA_VID_DEV_CAP_OUTPUT_POSITION, pos);

    PJSUA_UNLOCK();

    return status;
}

/*
 * Resize window.
 */
PJ_DEF(pj_status_t) pjsua_vid_win_set_size( pjsua_vid_win_id wid,
                                            const pjmedia_rect_size *size)
{
    pjsua_vid_win *w;
    pjmedia_vid_dev_stream *s;
    pj_status_t status;

    PJ_ASSERT_RETURN(wid >= 0 && wid < PJSUA_MAX_VID_WINS && size, PJ_EINVAL);

    PJSUA_LOCK();
    w = &pjsua_var.win[wid];
    if (w->vp_rend == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    s = pjmedia_vid_port_get_stream(w->vp_rend);
    if (s == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    status = pjmedia_vid_dev_stream_set_cap(s,
			    PJMEDIA_VID_DEV_CAP_OUTPUT_RESIZE, size);

    PJSUA_UNLOCK();

    return status;
}


static void call_get_vid_strm_info(pjsua_call *call,
				   int *first_active,
				   int *first_inactive,
				   unsigned *active_cnt,
				   unsigned *cnt)
{
    unsigned i, var_cnt = 0;

    if (first_active && ++var_cnt)
	*first_active = -1;
    if (first_inactive && ++var_cnt)
	*first_inactive = -1;
    if (active_cnt && ++var_cnt)
	*active_cnt = 0;
    if (cnt && ++var_cnt)
	*cnt = 0;

    for (i = 0; i < call->med_cnt && var_cnt; ++i) {
	if (call->media[i].type == PJMEDIA_TYPE_VIDEO) {
	    if (call->media[i].dir != PJMEDIA_DIR_NONE)
	    {
		if (first_active && *first_active == -1) {
		    *first_active = i;
		    --var_cnt;
		}
		if (active_cnt)
		    ++(*active_cnt);
	    } else if (first_inactive && *first_inactive == -1) {
		*first_inactive = i;
		--var_cnt;
	    }
	    if (cnt)
		++(*cnt);
	}
    }
}


/* Send SDP reoffer. */
static pj_status_t call_reoffer_sdp(pjsua_call_id call_id,
				    const pjmedia_sdp_session *sdp)
{
    pjsua_call *call;
    pjsip_tx_data *tdata;
    pjsip_dialog *dlg;
    pj_status_t status;

    status = acquire_call("call_reoffer_sdp()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    if (call->inv->state != PJSIP_INV_STATE_CONFIRMED) {
	PJ_LOG(3,(THIS_FILE, "Can not re-INVITE call that is not confirmed"));
	pjsip_dlg_dec_lock(dlg);
	return PJSIP_ESESSIONSTATE;
    }

    /* Create re-INVITE with new offer */
    status = pjsip_inv_reinvite( call->inv, NULL, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create re-INVITE", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Send the request */
    status = pjsip_inv_send_msg( call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send re-INVITE", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}

/* Add a new video stream into a call */
static pj_status_t call_add_video(pjsua_call *call,
				  pjmedia_vid_dev_index cap_dev)
{
    pj_pool_t *pool = call->inv->pool_prov;
    pjsua_acc_config *acc_cfg = &pjsua_var.acc[call->acc_id].cfg;
    pjsua_call_media *call_med;
    pjmedia_sdp_session *sdp;
    pjmedia_sdp_media *sdp_m;
    pjmedia_transport_info tpinfo;
    unsigned active_cnt;
    pj_status_t status;

    /* Verify media slot availability */
    if (call->med_cnt == PJSUA_MAX_CALL_MEDIA)
	return PJ_ETOOMANY;

    call_get_vid_strm_info(call, NULL, NULL, &active_cnt, NULL);
    if (active_cnt == acc_cfg->max_video_cnt)
	return PJ_ETOOMANY;

    /* Get active local SDP */
    status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &sdp);
    if (status != PJ_SUCCESS)
	return status;

    /* Initialize call media */
    call_med = &call->media[call->med_cnt++];

    status = pjsua_call_media_init(call_med, PJMEDIA_TYPE_VIDEO,
				   &acc_cfg->rtp_cfg, call->secure_level,
				   NULL);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Override default capture device setting */
    call_med->strm.v.cap_dev = cap_dev;

    /* Init transport media */
    status = pjmedia_transport_media_create(call_med->tp, pool, 0,
					    NULL, call_med->idx);
    if (status != PJ_SUCCESS)
	goto on_error;

    call_med->tp_st = PJSUA_MED_TP_INIT;

    /* Get transport address info */
    pjmedia_transport_info_init(&tpinfo);
    pjmedia_transport_get_info(call_med->tp, &tpinfo);

    /* Create SDP media line */
    status = pjmedia_endpt_create_video_sdp(pjsua_var.med_endpt, pool,
					    &tpinfo.sock_info, 0, &sdp_m);
    if (status != PJ_SUCCESS)
	goto on_error;

    sdp->media[sdp->media_count++] = sdp_m;

    /* Update SDP media line by media transport */
    status = pjmedia_transport_encode_sdp(call_med->tp, pool,
					  sdp, NULL, call_med->idx);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = call_reoffer_sdp(call->index, sdp);
    if (status != PJ_SUCCESS)
	goto on_error;

    return PJ_SUCCESS;

on_error:
    if (call_med->tp) {
	pjmedia_transport_close(call_med->tp);
	call_med->tp = call_med->tp_orig = NULL;
    }

    return status;
}


/* Modify a video stream from a call, i.e: enable/disable */
static pj_status_t call_modify_video(pjsua_call *call,
				     int med_idx,
				     pj_bool_t enable)
{
    pjsua_call_media *call_med;
    pjmedia_sdp_session *sdp;
    pj_status_t status;

    /* Verify and normalize media index */
    if (med_idx == -1) {
	int first_active;
	
	call_get_vid_strm_info(call, &first_active, NULL, NULL, NULL);
	if (first_active == -1)
	    return PJ_ENOTFOUND;

	med_idx = first_active;
    }

    call_med = &call->media[med_idx];

    /* Verify if the stream media type is video */
    if (call_med->type != PJMEDIA_TYPE_VIDEO)
	return PJ_EINVAL;

    /* Verify if the stream already enabled/disabled */
    if (( enable && call_med->dir != PJMEDIA_DIR_NONE) ||
	(!enable && call_med->dir == PJMEDIA_DIR_NONE))
	return PJ_SUCCESS;

    /* Get active local SDP */
    status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &sdp);
    if (status != PJ_SUCCESS)
	return status;

    pj_assert(med_idx < (int)sdp->media_count);

    if (enable) {
	pjsua_acc_config *acc_cfg = &pjsua_var.acc[call->acc_id].cfg;
	pj_pool_t *pool = call->inv->pool_prov;
	pjmedia_sdp_media *sdp_m;
	pjmedia_transport_info tpinfo;

	status = pjsua_call_media_init(call_med, PJMEDIA_TYPE_VIDEO,
				       &acc_cfg->rtp_cfg, call->secure_level,
				       NULL);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Init transport media */
	status = pjmedia_transport_media_create(call_med->tp, pool, 0,
						NULL, call_med->idx);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Get transport address info */
	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call_med->tp, &tpinfo);

	/* Create SDP media line */
	status = pjmedia_endpt_create_video_sdp(pjsua_var.med_endpt, pool,
						&tpinfo.sock_info, 0, &sdp_m);
	if (status != PJ_SUCCESS)
	    goto on_error;

	sdp->media[med_idx] = sdp_m;

	/* Update SDP media line by media transport */
	status = pjmedia_transport_encode_sdp(call_med->tp, pool,
					      sdp, NULL, call_med->idx);
	if (status != PJ_SUCCESS)
	    goto on_error;

on_error:
	if (status != PJ_SUCCESS) {
	    if (call_med->tp) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = call_med->tp_orig = NULL;
	    }
	    return status;
	}
    } else {
	/* Mark media transport to disabled */
	// Don't close this here, as SDP negotiation has not been
	// done and stream may be still active.
	call_med->tp_st = PJSUA_MED_TP_DISABLED;

	/* Disable the stream in SDP by setting port to 0 */
	sdp->media[med_idx]->desc.port = 0;
    }

    status = call_reoffer_sdp(call->index, sdp);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}


/* Change capture device of a video stream in a call */
static pj_status_t call_change_cap_dev(pjsua_call *call,
				       int med_idx,
				       pjmedia_vid_dev_index cap_dev)
{
    pjsua_call_media *call_med;
    pjmedia_vid_dev_info info;
    pjsua_vid_win *w, *new_w = NULL;
    pjsua_vid_win_id wid, new_wid;
    pjmedia_port *media_port;
    pj_status_t status;

    /* Verify and normalize media index */
    if (med_idx == -1) {
	int first_active;
	
	call_get_vid_strm_info(call, &first_active, NULL, NULL, NULL);
	if (first_active == -1)
	    return PJ_ENOTFOUND;

	med_idx = first_active;
    }

    call_med = &call->media[med_idx];

    /* Verify if the stream media type is video */
    if (call_med->type != PJMEDIA_TYPE_VIDEO)
	return PJ_EINVAL;

    /* Verify the capture device */
    status = pjmedia_vid_dev_get_info(cap_dev, &info);
    if (status != PJ_SUCCESS || info.dir != PJMEDIA_DIR_CAPTURE)
	return PJ_EINVAL;

    /* The specified capture device is being used already */
    if (call_med->strm.v.cap_dev == cap_dev)
	return PJ_SUCCESS;

    /* == Apply the new capture device == */

    wid = call_med->strm.v.cap_win_id;
    w = &pjsua_var.win[wid];
    pj_assert(w->type == PJSUA_WND_TYPE_PREVIEW && w->vp_cap);

    status = pjmedia_vid_stream_get_port(call_med->strm.v.stream,
					 PJMEDIA_DIR_ENCODING, &media_port);
    if (status != PJ_SUCCESS)
	return status;
    
    /* = Detach stream port from the old capture device = */
    status = pjmedia_vid_port_disconnect(w->vp_cap);
    if (status != PJ_SUCCESS)
	return status;

    status = pjmedia_vid_tee_remove_dst_port(w->tee, media_port);
    if (status != PJ_SUCCESS) {
	/* Connect back the old capturer */
	pjmedia_vid_port_connect(w->vp_cap, media_port, PJ_FALSE);
	return status;
    }

    /* = Attach stream port to the new capture device = */

    /* Create preview video window */
    status = create_vid_win(PJSUA_WND_TYPE_PREVIEW,
			    &media_port->info.fmt,
			    call_med->strm.v.rdr_dev,
			    cap_dev,
			    PJ_FALSE,
			    &new_wid);
    if (status != PJ_SUCCESS)
	goto on_error;

    inc_vid_win(new_wid);
    new_w = &pjsua_var.win[new_wid];
    
    /* Connect stream to capturer (via video window tee) */
    status = pjmedia_vid_tee_add_dst_port2(new_w->tee, 0, media_port);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Connect capturer to tee */
    status = pjmedia_vid_port_connect(new_w->vp_cap, new_w->tee, PJ_FALSE);
    if (status != PJ_SUCCESS)
	return status;

    /* Start renderer */
    status = pjmedia_vid_port_start(new_w->vp_rend);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Start capturer */
    status = pjmedia_vid_port_start(new_w->vp_cap);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Finally */
    call_med->strm.v.cap_dev = cap_dev;
    call_med->strm.v.cap_win_id = new_wid;
    dec_vid_win(wid);

    return PJ_SUCCESS;

on_error:
    if (new_w) {
	/* Disconnect media port from the new capturer */
	pjmedia_vid_tee_remove_dst_port(new_w->tee, media_port);
	/* Release the new capturer */
	dec_vid_win(new_wid);
    }

    /* Revert back to the old capturer */
    status = pjmedia_vid_tee_add_dst_port2(w->tee, 0, media_port);
    if (status != PJ_SUCCESS)
	return status;

    status = pjmedia_vid_port_connect(w->vp_cap, w->tee, PJ_FALSE);
    if (status != PJ_SUCCESS)
	return status;

    return status;
}


/* Start transmitting video stream in a call */
static pj_status_t call_start_tx_video(pjsua_call *call,
				       int med_idx,
				       pjmedia_vid_dev_index cap_dev)
{
    pjsua_call_media *call_med;
    pj_status_t status;

    /* Verify and normalize media index */
    if (med_idx == -1) {
	int first_active;
	
	call_get_vid_strm_info(call, &first_active, NULL, NULL, NULL);
	if (first_active == -1)
	    return PJ_ENOTFOUND;

	med_idx = first_active;
    }

    call_med = &call->media[med_idx];

    /* Verify if the stream is transmitting video */
    if (call_med->type != PJMEDIA_TYPE_VIDEO || 
	(call_med->dir & PJMEDIA_DIR_ENCODING) == 0)
    {
	return PJ_EINVAL;
    }

    /* Apply the capture device, it may be changed! */
    status = call_change_cap_dev(call, med_idx, cap_dev);
    if (status != PJ_SUCCESS)
	return status;

    /* Start stream in encoding direction */
    status = pjmedia_vid_stream_resume(call_med->strm.v.stream,
				       PJMEDIA_DIR_ENCODING);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}


/* Stop transmitting video stream in a call */
static pj_status_t call_stop_tx_video(pjsua_call *call,
				      int med_idx)
{
    pjsua_call_media *call_med;
    pj_status_t status;

    /* Verify and normalize media index */
    if (med_idx == -1) {
	int first_active;
	
	call_get_vid_strm_info(call, &first_active, NULL, NULL, NULL);
	if (first_active == -1)
	    return PJ_ENOTFOUND;

	med_idx = first_active;
    }

    call_med = &call->media[med_idx];

    /* Verify if the stream is transmitting video */
    if (call_med->type != PJMEDIA_TYPE_VIDEO || 
	(call_med->dir & PJMEDIA_DIR_ENCODING) == 0)
    {
	return PJ_EINVAL;
    }

    /* Pause stream in encoding direction */
    status = pjmedia_vid_stream_pause( call_med->strm.v.stream,
				       PJMEDIA_DIR_ENCODING);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}


/*
 * Start, stop, and/or manipulate video transmission for the specified call.
 */
PJ_DEF(pj_status_t) pjsua_call_set_vid_strm (
				pjsua_call_id call_id,
				pjsua_call_vid_strm_op op,
				const pjsua_call_vid_strm_op_param *param)
{
    pjsua_call *call;
    pjsua_call_vid_strm_op_param param_;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (param) {
	param_ = *param;
    } else {
	param_.med_idx = -1;
	param_.cap_dev = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
    }

    /* If set to PJMEDIA_VID_DEFAULT_CAPTURE_DEV, replace it with
     * account default video capture device.
     */
    if (param_.cap_dev == PJMEDIA_VID_DEFAULT_CAPTURE_DEV) {
	pjsua_acc_config *acc_cfg = &pjsua_var.acc[call->acc_id].cfg;
	param_.cap_dev = acc_cfg->vid_cap_dev;
	
	/* If the account default video capture device is
	 * PJMEDIA_VID_DEFAULT_CAPTURE_DEV, replace it with
	 * global default video capture device.
	 */
	if (param_.cap_dev == PJMEDIA_VID_DEFAULT_CAPTURE_DEV) {
	    pjmedia_vid_dev_info info;
	    pjmedia_vid_dev_get_info(param_.cap_dev, &info);
	    pj_assert(info.dir == PJMEDIA_DIR_CAPTURE);
	    param_.cap_dev = info.id;
	}
    }

    switch (op) {
    case PJSUA_CALL_VID_STRM_ADD:
	status = call_add_video(call, param_.cap_dev);
	break;
    case PJSUA_CALL_VID_STRM_ENABLE:
	status = call_modify_video(call, param_.med_idx, PJ_TRUE);
	break;
    case PJSUA_CALL_VID_STRM_DISABLE:
	status = call_modify_video(call, param_.med_idx, PJ_FALSE);
	break;
    case PJSUA_CALL_VID_STRM_CHANGE_CAP_DEV:
	status = call_change_cap_dev(call, param_.med_idx, param_.cap_dev);
	break;
    case PJSUA_CALL_VID_STRM_START_TRANSMIT:
	status = call_start_tx_video(call, param_.med_idx, param_.cap_dev);
	break;
    case PJSUA_CALL_VID_STRM_STOP_TRANSMIT:
	status = call_stop_tx_video(call, param_.med_idx);
	break;
    default:
	status = PJ_EINVALIDOP;
	break;
    }

    PJSUA_UNLOCK();

    return status;
}


/*
 * Get the media stream index of the default video stream in the call.
 */
PJ_DEF(int) pjsua_call_get_vid_stream_idx(pjsua_call_id call_id)
{
    pjsua_call *call;
    int first_active, first_inactive;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();
    call = &pjsua_var.calls[call_id];
    call_get_vid_strm_info(call, &first_active, &first_inactive, NULL, NULL);
    PJSUA_UNLOCK();

    if (first_active == -1)
	return first_inactive;

    return first_active;
}


#endif /* PJSUA_HAS_VIDEO */


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

static pjsua_vid_win_id alloc_vid_win(pjsua_vid_win_type type)
{
    pjsua_vid_win_id wid = PJSUA_INVALID_ID;
    unsigned i;

    for (i=0; i<PJSUA_MAX_VID_WINS; ++i) {
	pjsua_vid_win *w = &pjsua_var.win[i];
	if (w->type == PJSUA_WND_TYPE_NONE) {
	    wid = i;
	    w->type = type;
	    break;
	}
    }

    return wid;
}

static void free_vid_win(pjsua_vid_win_id wid)
{
    pjsua_vid_win *w = &pjsua_var.win[wid];
    if (w->vp_cap) {
	pjmedia_vid_port_destroy(w->vp_cap);
    }
    if (w->vp_rend) {
	pjmedia_vid_port_destroy(w->vp_rend);
    }
    pjsua_vid_win_reset(wid);
}

/*
 * Start video preview window for the specified capture device.
 */
PJ_DEF(pj_status_t) pjsua_vid_preview_start(pjmedia_vid_dev_index id,
                                            pjsua_vid_preview_param *prm)
{
    pjsua_vid_win_id wid = PJSUA_INVALID_ID;
    pjsua_vid_win *w = NULL;
    pjmedia_vid_port_param cap_param, rend_param;
    pjmedia_port *rend_port;
    const pjmedia_video_format_detail *vfd;
    pj_status_t status;

    PJSUA_LOCK();

    wid = pjsua_vid_preview_get_win(id);
    if (wid != PJSUA_INVALID_ID) {
	/* Preview already started for this device */
	PJSUA_UNLOCK();
	return PJ_SUCCESS;
    }

    wid = alloc_vid_win(PJSUA_WND_TYPE_PREVIEW);
    if (wid != PJSUA_INVALID_ID) {
	pjsua_var.win[wid].preview_cap_id = id;
    }
    if (wid == PJSUA_INVALID_ID) {
	PJSUA_UNLOCK();
	return PJ_ETOOMANY;
    }
    w = &pjsua_var.win[wid];

    /* Create capture video port */
    pjmedia_vid_port_param_default(&cap_param);
    cap_param.active = PJ_TRUE;
    cap_param.vidparam.dir = PJMEDIA_DIR_CAPTURE;
    status = pjmedia_vid_dev_default_param(w->pool, id,
					   &cap_param.vidparam);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pjmedia_vid_port_create(w->pool, &cap_param, &w->vp_cap);
    if (status != PJ_SUCCESS)
	goto on_error;

    vfd = pjmedia_format_get_video_format_detail(&cap_param.vidparam.fmt,
                                                 PJ_TRUE);
    if (vfd == NULL) {
	status = PJ_ENOTFOUND;
	goto on_error;
    }

    /* Create renderer video port */
    pjmedia_vid_port_param_default(&rend_param);
    status = pjmedia_vid_dev_default_param(w->pool,
					   PJMEDIA_VID_DEFAULT_RENDER_DEV,
					   &rend_param.vidparam);
    if (status != PJ_SUCCESS)
	goto on_error;

    rend_param.active = PJ_FALSE;
    rend_param.vidparam.dir = PJMEDIA_DIR_RENDER;
    rend_param.vidparam.fmt = cap_param.vidparam.fmt;
    rend_param.vidparam.disp_size = vfd->size;

    status = pjmedia_vid_port_create(w->pool, &rend_param, &w->vp_rend);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Connect capture dev to renderer */
    rend_port = pjmedia_vid_port_get_passive_port(w->vp_rend);
    status = pjmedia_vid_port_connect(w->vp_cap, rend_port, PJ_FALSE);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Start devices */
    status = pjmedia_vid_port_start(w->vp_rend);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pjmedia_vid_port_start(w->vp_cap);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done */
    PJSUA_UNLOCK();
    return PJ_SUCCESS;

on_error:
    if (wid != PJSUA_INVALID_ID) {
	free_vid_win(wid);
    }

    PJSUA_UNLOCK();
    return status;
}

/*
 * Stop video preview.
 */
PJ_DEF(pj_status_t) pjsua_vid_preview_stop(pjmedia_vid_dev_index id)
{
    pjsua_vid_win_id wid = PJSUA_INVALID_ID;
    pjsua_vid_win *w;
    pj_status_t status;

    PJSUA_LOCK();
    wid = pjsua_vid_preview_get_win(id);
    if (wid == PJSUA_INVALID_ID) {
	PJSUA_UNLOCK();
	return PJ_ENOTFOUND;
    }

    w = &pjsua_var.win[wid];

    status = pjmedia_vid_port_stop(w->vp_cap);
    status = pjmedia_vid_port_stop(w->vp_rend);

    free_vid_win(wid);

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*****************************************************************************
 * Window
 */

/*
 * Get window info.
 */
PJ_DEF(pj_status_t) pjsua_vid_win_get_info( pjsua_vid_win_id wid,
                                            pjsua_vid_win_info *wi)
{
    pjsua_vid_win *w;

    PJ_ASSERT_RETURN(wid >= 0 && wid < PJSUA_MAX_VID_WINS && wi, PJ_EINVAL);

    PJSUA_LOCK();
    w = &pjsua_var.win[wid];
    if (w->vp_rend == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    PJ_TODO(vid_implement_pjsua_vid_win_get_info);
    PJSUA_UNLOCK();

    return PJ_ENOTSUP;
}

/*
 * Show or hide window.
 */
PJ_DEF(pj_status_t) pjsua_vid_win_set_show( pjsua_vid_win_id wid,
                                            pj_bool_t show)
{
    pjsua_vid_win *w;

    PJ_ASSERT_RETURN(wid >= 0 && wid < PJSUA_MAX_VID_WINS, PJ_EINVAL);

    PJSUA_LOCK();
    w = &pjsua_var.win[wid];
    if (w->vp_rend == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    PJ_TODO(vid_implement_pjsua_vid_win_set_show);
    PJSUA_UNLOCK();

    return PJ_ENOTSUP;
}

/*
 * Set video window position.
 */
PJ_DEF(pj_status_t) pjsua_vid_win_set_pos( pjsua_vid_win_id wid,
                                           const pjmedia_coord *pos)
{
    pjsua_vid_win *w;

    PJ_ASSERT_RETURN(wid >= 0 && wid < PJSUA_MAX_VID_WINS && pos, PJ_EINVAL);

    PJSUA_LOCK();
    w = &pjsua_var.win[wid];
    if (w->vp_rend == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    PJ_TODO(vid_implement_pjsua_vid_win_set_pos);
    PJSUA_UNLOCK();

    return PJ_ENOTSUP;
}

/*
 * Resize window.
 */
PJ_DEF(pj_status_t) pjsua_vid_win_set_size( pjsua_vid_win_id wid,
                                            const pjmedia_rect_size *size)
{
    pjsua_vid_win *w;

    PJ_ASSERT_RETURN(wid >= 0 && wid < PJSUA_MAX_VID_WINS && size, PJ_EINVAL);

    PJSUA_LOCK();
    w = &pjsua_var.win[wid];
    if (w->vp_rend == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    PJ_TODO(vid_implement_pjsua_vid_win_set_size);
    PJSUA_UNLOCK();

    return PJ_ENOTSUP;
}



#endif /* PJSUA_HAS_VIDEO */


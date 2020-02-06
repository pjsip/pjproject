/* $Id$ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/vid_port.h>
#include <pjmedia/clock.h>
#include <pjmedia/converter.h>
#include <pjmedia/errno.h>
#include <pjmedia/event.h>
#include <pjmedia/vid_codec.h>
#include <pj/log.h>
#include <pj/pool.h>

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)


#define SIGNATURE	PJMEDIA_SIG_VID_PORT
#define THIS_FILE	"vid_port.c"


/* Enable/disable test of finding closest format algo */
#define ENABLE_TEST_FIND_FMT 0


/**
 * Enable this to trace the format matching process.
 */
#if 0
#  define TRACE_FIND_FMT(args)	    PJ_LOG(5,args)
#else
#  define TRACE_FIND_FMT(args)
#endif

/**
 * We use nearest width and aspect ratio to find match between the requested 
 * format and the supported format. Specify this to determine the array size 
 * of the supported formats with the nearest width. From this array, we will 
 * find the one with lowest diff_ratio. Setting this to 1 will thus skip 
 * the aspect ratio calculation. 
 */
#ifndef PJMEDIA_VID_PORT_MATCH_WIDTH_ARRAY_SIZE
#   define PJMEDIA_VID_PORT_MATCH_WIDTH_ARRAY_SIZE 3
#endif

typedef struct vid_pasv_port vid_pasv_port;

enum role
{
    ROLE_NONE,
    ROLE_ACTIVE,
    ROLE_PASSIVE
};

enum fmt_match
{
    FMT_MATCH,
    FMT_SAME_COLOR_SPACE,
    FMT_DIFF_COLOR_SPACE
};

struct pjmedia_vid_port
{
    pj_pool_t               *pool;
    pj_str_t                 dev_name;
    pjmedia_dir              dir;
//    pjmedia_rect_size        cap_size;
    pjmedia_vid_dev_stream  *strm;
    pjmedia_vid_dev_cb       strm_cb;
    void                    *strm_cb_data;
    enum role                role,
                             stream_role;
    vid_pasv_port           *pasv_port;
    pjmedia_port            *client_port;
    pj_bool_t                destroy_client_port;

    struct {
        pjmedia_converter	*conv;
        void		        *conv_buf;
        pj_size_t		 conv_buf_size;
        pjmedia_conversion_param conv_param;
        unsigned                 usec_ctr;
        unsigned                 usec_src, usec_dst;
    } conv;

    pjmedia_clock           *clock;
    pjmedia_clock_src        clocksrc;

    struct sync_clock_src_t
    {
        pjmedia_clock_src   *sync_clocksrc;
        pj_int32_t           sync_delta;
        unsigned             max_sync_ticks;
        unsigned             nsync_frame;
        unsigned             nsync_progress;
    } sync_clocksrc;

    pjmedia_frame           *frm_buf;
    pj_size_t                frm_buf_size;
    pj_mutex_t              *frm_mutex;
};

struct vid_pasv_port
{
    pjmedia_port	 base;
    pjmedia_vid_port	*vp;
};

struct fmt_prop 
{
    pj_uint32_t id;
    pjmedia_rect_size size;
    pjmedia_ratio fps;
};

static pj_status_t vidstream_cap_cb(pjmedia_vid_dev_stream *stream,
				    void *user_data,
				    pjmedia_frame *frame);
static pj_status_t vidstream_render_cb(pjmedia_vid_dev_stream *stream,
				       void *user_data,
				       pjmedia_frame *frame);
static pj_status_t vidstream_event_cb(pjmedia_event *event,
                                      void *user_data);
static pj_status_t client_port_event_cb(pjmedia_event *event,
                                        void *user_data);

static void enc_clock_cb(const pj_timestamp *ts, void *user_data);
static void dec_clock_cb(const pj_timestamp *ts, void *user_data);

static pj_status_t vid_pasv_port_put_frame(struct pjmedia_port *this_port,
					   pjmedia_frame *frame);

static pj_status_t vid_pasv_port_get_frame(struct pjmedia_port *this_port,
					   pjmedia_frame *frame);


PJ_DEF(void) pjmedia_vid_port_param_default(pjmedia_vid_port_param *prm)
{
    pj_bzero(prm, sizeof(*prm));
    prm->active = PJ_TRUE;
}

static const char *vid_dir_name(pjmedia_dir dir)
{
    switch (dir) {
    case PJMEDIA_DIR_CAPTURE:
	return "capture";
    case PJMEDIA_DIR_RENDER:
	return "render";
    default:
	return "??";
    }
}

static pj_status_t get_vfi(const pjmedia_format *fmt,
			   const pjmedia_video_format_info **p_vfi,
			   pjmedia_video_apply_fmt_param *vafp)
{
    const pjmedia_video_format_info *vfi;

    vfi = pjmedia_get_video_format_info(NULL, fmt->id);
    if (!vfi)
	return PJMEDIA_EBADFMT;

    if (p_vfi) *p_vfi = vfi;

    pj_bzero(vafp, sizeof(*vafp));
    vafp->size = fmt->det.vid.size;
    return vfi->apply_fmt(vfi, vafp);
}

static pj_status_t create_converter(pjmedia_vid_port *vp)
{
    if (vp->conv.conv) {
        pjmedia_converter_destroy(vp->conv.conv);
	vp->conv.conv = NULL;
    }

    /* Instantiate converter if necessary */
    if (vp->conv.conv_param.src.id != vp->conv.conv_param.dst.id ||
	(vp->conv.conv_param.src.det.vid.size.w !=
         vp->conv.conv_param.dst.det.vid.size.w) ||
	(vp->conv.conv_param.src.det.vid.size.h !=
         vp->conv.conv_param.dst.det.vid.size.h))
    {
	pj_status_t status;

	/* Yes, we need converter */
	status = pjmedia_converter_create(NULL, vp->pool, &vp->conv.conv_param,
					  &vp->conv.conv);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, status, "Error creating converter"));
	    return status;
	}
    }

    if (vp->conv.conv ||
        (vp->role==ROLE_ACTIVE && (vp->dir & PJMEDIA_DIR_ENCODING)))
    {
	pj_status_t status;
	pjmedia_video_apply_fmt_param vafp;

	/* Allocate buffer for conversion */
	status = get_vfi(&vp->conv.conv_param.dst, NULL, &vafp);
	if (status != PJ_SUCCESS)
	    return status;

	if (vafp.framebytes > vp->conv.conv_buf_size) {
	    vp->conv.conv_buf = pj_pool_alloc(vp->pool, vafp.framebytes);
	    vp->conv.conv_buf_size = vafp.framebytes;
	}
    }

    vp->conv.usec_ctr = 0;
    vp->conv.usec_src = PJMEDIA_PTIME(&vp->conv.conv_param.src.det.vid.fps);
    vp->conv.usec_dst = PJMEDIA_PTIME(&vp->conv.conv_param.dst.det.vid.fps);

    return PJ_SUCCESS;
}	  

static pj_uint32_t match_format_id(pj_uint32_t req_id,
				   pj_uint32_t sup_id)
{
    const pjmedia_video_format_info *req_fmt_info, *sup_fmt_info;

    if (req_id == sup_id)
	return FMT_MATCH;

    req_fmt_info = pjmedia_get_video_format_info( 
					pjmedia_video_format_mgr_instance(),
					req_id);

    sup_fmt_info = pjmedia_get_video_format_info( 
					pjmedia_video_format_mgr_instance(),
					sup_id);

    if ((req_fmt_info == NULL) || (sup_fmt_info == NULL)) {
	return FMT_DIFF_COLOR_SPACE;
    }

    if (req_fmt_info->color_model == sup_fmt_info->color_model) {
	return FMT_SAME_COLOR_SPACE;
    }

    return FMT_DIFF_COLOR_SPACE;
}

static pj_uint32_t get_match_format_id(pj_uint32_t req_fmt_id,
				       pjmedia_vid_dev_info *di)
{
    unsigned i, match_idx = 0, match_fmt = FMT_DIFF_COLOR_SPACE+1;

    /* Find the matching format. If no exact match is found, find 
     * the supported format with the same color space. If no match is found,
     * use the first supported format on the list.
     */
    for (i = 0; i < di->fmt_cnt; ++i) {
	unsigned tmp_fmt = match_format_id(req_fmt_id, di->fmt[i].id);

	if (match_fmt == FMT_MATCH)
	    return req_fmt_id;

	if (tmp_fmt < match_fmt) {
	    match_idx = i;
	    match_fmt = tmp_fmt;
	}
    }
    return di->fmt[match_idx].id;
}

/**
 * Find the closest supported format from the specific requested format.
 * The algo is to find a supported size with the matching format id, width and
 * lowest diff_ratio.
 * ---
 * For format id matching, the priority is:
 * 1. Find exact match
 * 2. Find format with the same color space
 * 3. Use the first supported format. 
 * ---
 * For ratio matching:
 * Find the lowest difference of the aspect ratio between the requested and
 * the supported format.
 */
static struct fmt_prop find_closest_fmt(pj_uint32_t req_fmt_id,
					pjmedia_rect_size *req_fmt_size,
					pjmedia_ratio *req_fmt_fps,
					pjmedia_vid_dev_info *di)
{
    unsigned i, match_idx = 0;
    pj_uint32_t match_fmt_id;     
    float req_ratio, min_diff_ratio = 0.0;    
    struct fmt_prop ret_prop;
    pj_bool_t found_exact_match = PJ_FALSE;

    #define	GET_DIFF(x, y)	((x) > (y)? (x-y) : (y-x))
    
    /* This will contain the supported format with lowest width difference */
    pjmedia_rect_size nearest_width[PJMEDIA_VID_PORT_MATCH_WIDTH_ARRAY_SIZE];

    /* Initialize the list. */
    for (i=0;i<PJMEDIA_VID_PORT_MATCH_WIDTH_ARRAY_SIZE;++i) {
	nearest_width[i].w = 0xFFFFFFFF;
	nearest_width[i].h = 0;
    }

    /* Get the matching format id. We assume each format will support all 
     * image size. 
     */
    match_fmt_id = get_match_format_id(req_fmt_id, di);
    
    /* Search from the supported format, the smallest diff width. Stop the 
     * search if exact match is found.
     */
    for (i=0;i<di->fmt_cnt;++i) {
	pjmedia_video_format_detail *vfd;
	unsigned diff_width1, diff_width2;

	/* Ignore supported format with different format id. */
	if (di->fmt[i].id != match_fmt_id)
	    continue;

	vfd = pjmedia_format_get_video_format_detail(&di->fmt[i], PJ_TRUE);

	/* Exact match found. */
	if ((vfd->size.w == req_fmt_size->w) && 
	    (vfd->size.h == req_fmt_size->h)) 
	{
	    nearest_width[0] = vfd->size;
	    found_exact_match = PJ_TRUE;
	    break;
	}

	diff_width1 =  GET_DIFF(vfd->size.w, req_fmt_size->w);
	diff_width2 =  GET_DIFF(nearest_width[0].w, req_fmt_size->w);

	/* Fill the nearest width list. */
	if (diff_width1 <= diff_width2) {
	    int k = 1;
	    pjmedia_rect_size tmp_size = vfd->size;

	    while(((k < PJ_ARRAY_SIZE(nearest_width)) &&
		   (GET_DIFF(tmp_size.w, req_fmt_size->w) <
		   (GET_DIFF(nearest_width[k].w, req_fmt_size->w)))))
	    {
		nearest_width[k-1] = nearest_width[k];
		++k;
	    }
	    nearest_width[k-1] = tmp_size;
	}
    }
    /* No need to calculate ratio if exact match is found. */
    if (!found_exact_match) {
	pj_bool_t found_match = PJ_FALSE;

	/* We have the list of supported format with nearest width. Now get the 
	 * best ratio.
	 */
	req_ratio = (float)req_fmt_size->w / (float)req_fmt_size->h;
	for (i=0;i<PJ_ARRAY_SIZE(nearest_width);++i) {
	    float sup_ratio, diff_ratio;

	    if (nearest_width[i].w == 0xFFFFFFFF)
		continue;

	    sup_ratio = (float)nearest_width[i].w / (float)nearest_width[i].h;

	    diff_ratio = GET_DIFF(sup_ratio, req_ratio);

	    if ((!found_match) || (diff_ratio <= min_diff_ratio)) {
		found_match = PJ_TRUE;
		match_idx = i;
		min_diff_ratio = diff_ratio;
	    }
	}
    }
    ret_prop.id = match_fmt_id;
    ret_prop.size = nearest_width[match_idx];
    ret_prop.fps = *req_fmt_fps;
    return ret_prop;
}

#if ENABLE_TEST_FIND_FMT
/**
 * This is to test the algo to find the closest fmt
 */
static void test_find_closest_fmt(pjmedia_vid_dev_info *di)
{  
    unsigned i, j, k;
    char fmt_name[5];

    pjmedia_rect_size find_size[] = {
	{720, 480},
	{352, 288},
	{400, 300},
	{1600, 900},
	{255, 352},
	{500, 500},
    };

    pjmedia_ratio find_fps[] = {
	{1, 1},
	{10, 1},
	{15, 1},
	{30, 1},
    };

    pj_uint32_t find_id[] = {
	PJMEDIA_FORMAT_RGB24,
	PJMEDIA_FORMAT_RGBA,
	PJMEDIA_FORMAT_AYUV,
	PJMEDIA_FORMAT_YUY2,
	PJMEDIA_FORMAT_I420
    };

    TRACE_FIND_FMT((THIS_FILE, "Supported format = "));
    for (i = 0; i < di->fmt_cnt; i++) {
	//pjmedia_video_format_detail *vid_fd = 
	//    pjmedia_format_get_video_format_detail(&di->fmt[i], PJ_TRUE);

	pjmedia_fourcc_name(di->fmt[i].id, fmt_name);

	TRACE_FIND_FMT((THIS_FILE, "id:%s size:%d*%d fps:%d/%d", 
			fmt_name,
			vid_fd->size.w,
			vid_fd->size.h,
			vid_fd->fps.num,
			vid_fd->fps.denum));
    }
    
    for (i = 0; i < PJ_ARRAY_SIZE(find_id); i++) {

	for (j = 0; j < PJ_ARRAY_SIZE(find_fps); j++) {
	
	    for (k = 0; k < PJ_ARRAY_SIZE(find_size); k++) {
		struct fmt_prop match_prop;

		pjmedia_fourcc_name(find_id[i], fmt_name);

		TRACE_FIND_FMT((THIS_FILE, "Trying to find closest match "
				           "id:%s size:%dx%d fps:%d/%d", 
			        fmt_name,
			        find_size[k].w,
			        find_size[k].h,
			        find_fps[j].num,
			        find_fps[j].denum));
		
		match_prop = find_closest_fmt(find_id[i],
					      &find_size[k],
					      &find_fps[j],
					      di);

		if ((match_prop.id == find_id[i]) && 
		    (match_prop.size.w == find_size[k].w) &&
		    (match_prop.size.h == find_size[k].h) &&
		    (match_prop.fps.num / match_prop.fps.denum == 
		     find_fps[j].num * find_fps[j].denum)) 
		{
		    TRACE_FIND_FMT((THIS_FILE, "Exact Match found!!"));
		} else {
		    pjmedia_fourcc_name(match_prop.id, fmt_name);
		    TRACE_FIND_FMT((THIS_FILE, "Closest format = "\
					        "id:%s size:%dx%d fps:%d/%d", 
				    fmt_name,
				    match_prop.size.w,
				    match_prop.size.h, 
				    match_prop.fps.num,
				    match_prop.fps.denum));		    
		}
	    }
	}
    }
}
#endif

PJ_DEF(pj_status_t) pjmedia_vid_port_create( pj_pool_t *pool,
					     const pjmedia_vid_port_param *prm,
					     pjmedia_vid_port **p_vid_port)
{
    pjmedia_vid_port *vp;
    pjmedia_video_format_detail *vfd;
    char fmt_name[5];
    pjmedia_vid_dev_cb vid_cb;
    pj_bool_t need_frame_buf = PJ_FALSE;
    pj_status_t status;
    unsigned ptime_usec;
    pjmedia_vid_dev_param vparam;
    pjmedia_vid_dev_info di;
    char dev_name[sizeof(di.name) + sizeof(di.driver) + 4];

    PJ_ASSERT_RETURN(pool && prm && p_vid_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(prm->vidparam.fmt.type == PJMEDIA_TYPE_VIDEO &&
                     prm->vidparam.dir != PJMEDIA_DIR_NONE &&
                     prm->vidparam.dir != PJMEDIA_DIR_CAPTURE_RENDER,
		     PJ_EINVAL);

    /* Retrieve the video format detail */
    vfd = pjmedia_format_get_video_format_detail(&prm->vidparam.fmt, PJ_TRUE);
    if (!vfd)
	return PJ_EINVAL;

    PJ_ASSERT_RETURN(vfd->fps.num, PJ_EINVAL);

    /* Get device info */
    if (prm->vidparam.dir & PJMEDIA_DIR_CAPTURE)
        status = pjmedia_vid_dev_get_info(prm->vidparam.cap_id, &di);
    else
        status = pjmedia_vid_dev_get_info(prm->vidparam.rend_id, &di);
    if (status != PJ_SUCCESS)
        return status;

    /* Allocate videoport */
    vp = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_port);
    vp->pool = pj_pool_create(pool->factory, "video port", 500, 500, NULL);
    vp->role = prm->active ? ROLE_ACTIVE : ROLE_PASSIVE;
    vp->dir = prm->vidparam.dir;
//    vp->cap_size = vfd->size;

    vparam = prm->vidparam;
    dev_name[0] = '\0';

    pj_ansi_snprintf(dev_name, sizeof(dev_name), "%s [%s]", di.name, di.driver);
    pjmedia_fourcc_name(vparam.fmt.id, fmt_name);
    PJ_LOG(4,(THIS_FILE,
	      "Opening device %s for %s: format=%s, size=%dx%d @%d:%d fps",
	      dev_name,
	      vid_dir_name(prm->vidparam.dir), fmt_name,
	      vfd->size.w, vfd->size.h,
	      vfd->fps.num, vfd->fps.denum));

    if (di.dir == PJMEDIA_DIR_RENDER) {
	/* Find the matching format. If no exact match is found, find 
	 * the supported format with the same color space. If no match is found,
	 * use the first supported format on the list.
	 */
	pj_assert(di.fmt_cnt != 0);
	vparam.fmt.id = get_match_format_id(prm->vidparam.fmt.id, &di);
    } else {
	struct fmt_prop match_prop;

	if (di.fmt_cnt == 0) {
	    status = PJMEDIA_EVID_SYSERR;
	    PJ_PERROR(4,(THIS_FILE, status, "Device has no supported format"));
	    return status;
	}

#if ENABLE_TEST_FIND_FMT
	test_find_closest_fmt(&di);
#endif

	match_prop = find_closest_fmt(prm->vidparam.fmt.id, 
				      &vfd->size,			     
				      &vfd->fps, 
				      &di);

	if ((match_prop.id != prm->vidparam.fmt.id) || 
	    (match_prop.size.w != vfd->size.w) ||
	    (match_prop.size.h != vfd->size.h))
	{
	    vparam.fmt.id = match_prop.id;
	    vparam.fmt.det.vid.size = match_prop.size;
	}
    }

    pj_strdup2_with_null(pool, &vp->dev_name, di.name);
    vp->stream_role = di.has_callback ? ROLE_ACTIVE : ROLE_PASSIVE;

    ptime_usec = PJMEDIA_PTIME(&vfd->fps);
    pjmedia_clock_src_init(&vp->clocksrc, PJMEDIA_TYPE_VIDEO,
                           prm->vidparam.clock_rate, ptime_usec);
    vp->sync_clocksrc.max_sync_ticks = 
        PJMEDIA_CLOCK_SYNC_MAX_RESYNC_DURATION *
        1000 / vp->clocksrc.ptime_usec;

    /* Create the video stream */
    pj_bzero(&vid_cb, sizeof(vid_cb));
    vid_cb.capture_cb = &vidstream_cap_cb;
    vid_cb.render_cb = &vidstream_render_cb;

    status = pjmedia_vid_dev_stream_create(&vparam, &vid_cb, vp,
				           &vp->strm);
    if (status != PJ_SUCCESS)
	goto on_error;

    pjmedia_fourcc_name(vparam.fmt.id, fmt_name);
    PJ_LOG(4,(THIS_FILE,
	      "Device %s opened: format=%s, size=%dx%d @%d:%d fps",
	      dev_name, fmt_name,
	      vparam.fmt.det.vid.size.w, vparam.fmt.det.vid.size.h,
	      vparam.fmt.det.vid.fps.num, vparam.fmt.det.vid.fps.denum));

    /* Subscribe to device's events */
    pjmedia_event_subscribe(NULL, &vidstream_event_cb,
                            vp, vp->strm);

    if (vp->dir & PJMEDIA_DIR_CAPTURE) {
	pjmedia_format_copy(&vp->conv.conv_param.src, &vparam.fmt);
	pjmedia_format_copy(&vp->conv.conv_param.dst, &prm->vidparam.fmt);
    } else {
	pjmedia_format_copy(&vp->conv.conv_param.src, &prm->vidparam.fmt);
	pjmedia_format_copy(&vp->conv.conv_param.dst, &vparam.fmt);
    }

    status = create_converter(vp);
    if (status != PJ_SUCCESS)
	goto on_error;

    if (vp->role==ROLE_ACTIVE &&
        ((vp->dir & PJMEDIA_DIR_ENCODING) || vp->stream_role==ROLE_PASSIVE))
    {
        pjmedia_clock_param param;

	/* Active role is wanted, but our device is passive, so create
	 * master clocks to run the media flow. For encoding direction,
         * we also want to create our own clock since the device's clock
         * may run at a different rate.
	 */
	need_frame_buf = PJ_TRUE;
            
        param.usec_interval = PJMEDIA_PTIME(&vfd->fps);
        param.clock_rate = prm->vidparam.clock_rate;
        status = pjmedia_clock_create2(pool, &param,
                                       PJMEDIA_CLOCK_NO_HIGHEST_PRIO,
                                       (vp->dir & PJMEDIA_DIR_ENCODING) ?
                                       &enc_clock_cb: &dec_clock_cb,
                                       vp, &vp->clock);
        if (status != PJ_SUCCESS)
            goto on_error;

    } else if (vp->role==ROLE_PASSIVE) {
	vid_pasv_port *pp;

	/* Always need to create media port for passive role */
	vp->pasv_port = pp = PJ_POOL_ZALLOC_T(pool, vid_pasv_port);
	pp->vp = vp;
	if (prm->vidparam.dir & PJMEDIA_DIR_CAPTURE)
	    pp->base.get_frame = &vid_pasv_port_get_frame;
	if (prm->vidparam.dir & PJMEDIA_DIR_RENDER)
	    pp->base.put_frame = &vid_pasv_port_put_frame;
	pjmedia_port_info_init2(&pp->base.info, &vp->dev_name,
	                        PJMEDIA_SIG_VID_PORT,
			        prm->vidparam.dir, &prm->vidparam.fmt);

        need_frame_buf = PJ_TRUE;
    }

    if (need_frame_buf) {
	pjmedia_video_apply_fmt_param vafp;

	status = get_vfi(&vp->conv.conv_param.src, NULL, &vafp);
	if (status != PJ_SUCCESS)
	    goto on_error;

        vp->frm_buf = PJ_POOL_ZALLOC_T(pool, pjmedia_frame);
        vp->frm_buf_size = vafp.framebytes;
        vp->frm_buf->buf = pj_pool_zalloc(pool, vafp.framebytes);
        vp->frm_buf->size = vp->frm_buf_size;
        vp->frm_buf->type = PJMEDIA_FRAME_TYPE_NONE;

        status = pj_mutex_create_simple(pool, vp->dev_name.ptr,
                                        &vp->frm_mutex);
        if (status != PJ_SUCCESS)
            goto on_error;
    }

    *p_vid_port = vp;

    return PJ_SUCCESS;

on_error:
    pjmedia_vid_port_destroy(vp);
    return status;
}

PJ_DEF(void) pjmedia_vid_port_set_cb(pjmedia_vid_port *vid_port,
				     const pjmedia_vid_dev_cb *cb,
                                     void *user_data)
{
    pj_assert(vid_port && cb);
    pj_memcpy(&vid_port->strm_cb, cb, sizeof(*cb));
    vid_port->strm_cb_data = user_data;
}

PJ_DEF(pjmedia_vid_dev_stream*)
pjmedia_vid_port_get_stream(pjmedia_vid_port *vp)
{
    PJ_ASSERT_RETURN(vp, NULL);
    return vp->strm;
}


PJ_DEF(pjmedia_port*)
pjmedia_vid_port_get_passive_port(pjmedia_vid_port *vp)
{
    PJ_ASSERT_RETURN(vp && vp->role==ROLE_PASSIVE, NULL);
    return &vp->pasv_port->base;
}


PJ_DEF(pjmedia_clock_src *)
pjmedia_vid_port_get_clock_src( pjmedia_vid_port *vid_port )
{
    PJ_ASSERT_RETURN(vid_port, NULL);
    return &vid_port->clocksrc;
}

PJ_DECL(pj_status_t)
pjmedia_vid_port_set_clock_src( pjmedia_vid_port *vid_port,
                                pjmedia_clock_src *clocksrc)
{
    PJ_ASSERT_RETURN(vid_port && clocksrc, PJ_EINVAL);

    vid_port->sync_clocksrc.sync_clocksrc = clocksrc;
    vid_port->sync_clocksrc.sync_delta =
        pjmedia_clock_src_get_time_msec(&vid_port->clocksrc) -
        pjmedia_clock_src_get_time_msec(clocksrc);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_vid_port_subscribe_event(
						pjmedia_vid_port *vp,
						pjmedia_port *port)
{
    PJ_ASSERT_RETURN(vp && port, PJ_EINVAL);

    /* Subscribe to port's events */
    return pjmedia_event_subscribe(NULL, &client_port_event_cb, vp, port);
}

PJ_DEF(pj_status_t) pjmedia_vid_port_connect(pjmedia_vid_port *vp,
					      pjmedia_port *port,
					      pj_bool_t destroy)
{
    PJ_ASSERT_RETURN(vp && vp->role==ROLE_ACTIVE, PJ_EINVAL);
    vp->destroy_client_port = destroy;
    vp->client_port = port;

    /* Subscribe to client port's events */
    pjmedia_event_subscribe(NULL, &client_port_event_cb, vp,
                            vp->client_port);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_vid_port_disconnect(pjmedia_vid_port *vp)
{
    PJ_ASSERT_RETURN(vp && vp->role==ROLE_ACTIVE, PJ_EINVAL);

    pjmedia_event_unsubscribe(NULL, &client_port_event_cb, vp,
                              vp->client_port);
    vp->client_port = NULL;

    return PJ_SUCCESS;
}


PJ_DEF(pjmedia_port*)
pjmedia_vid_port_get_connected_port(pjmedia_vid_port *vp)
{
    PJ_ASSERT_RETURN(vp && vp->role==ROLE_ACTIVE, NULL);
    return vp->client_port;
}

PJ_DEF(pj_status_t) pjmedia_vid_port_start(pjmedia_vid_port *vp)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(vp, PJ_EINVAL);

    status = pjmedia_vid_dev_stream_start(vp->strm);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Initialize buffer with black color */
    {
        const pjmedia_video_format_info *vfi;
        const pjmedia_format *fmt;
	pjmedia_video_apply_fmt_param vafp;
	pjmedia_frame frame;

	pj_bzero(&frame, sizeof(pjmedia_frame));
	frame.buf = vp->frm_buf->buf;
	frame.size = vp->frm_buf_size;

	fmt = &vp->conv.conv_param.src;
	status = get_vfi(fmt, &vfi, &vafp);
	if (status == PJ_SUCCESS && frame.buf) {
	    frame.type = PJMEDIA_FRAME_TYPE_VIDEO;
	    pj_assert(frame.size >= vafp.framebytes);
	    frame.size = vafp.framebytes;
	    
	    if (vfi->color_model == PJMEDIA_COLOR_MODEL_RGB) {
	    	pj_memset(frame.buf, 0, vafp.framebytes);
	    } else if (fmt->id == PJMEDIA_FORMAT_I420 ||
	  	       fmt->id == PJMEDIA_FORMAT_YV12)
	    {	    	
	    	pj_memset(frame.buf, 16, vafp.plane_bytes[0]);
	    	pj_memset((pj_uint8_t*)frame.buf + vafp.plane_bytes[0],
		      	  0x80, vafp.plane_bytes[1] * 2);
	    }
        }
    }

    if (vp->clock) {
	status = pjmedia_clock_start(vp->clock);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    return PJ_SUCCESS;

on_error:
    pjmedia_vid_port_stop(vp);
    return status;
}

PJ_DEF(pj_bool_t) pjmedia_vid_port_is_running(pjmedia_vid_port *vp)
{
    return pjmedia_vid_dev_stream_is_running(vp->strm);
}

PJ_DEF(pj_status_t) pjmedia_vid_port_stop(pjmedia_vid_port *vp)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(vp, PJ_EINVAL);

    if (vp->clock) {
	status = pjmedia_clock_stop(vp->clock);
    }

    status = pjmedia_vid_dev_stream_stop(vp->strm);

    return status;
}

PJ_DEF(void) pjmedia_vid_port_destroy(pjmedia_vid_port *vp)
{
    PJ_ASSERT_ON_FAIL(vp, return);

    PJ_LOG(4,(THIS_FILE, "Closing %s..", vp->dev_name.ptr));

    if (vp->clock) {
	pjmedia_clock_destroy(vp->clock);
	vp->clock = NULL;
    }
    if (vp->strm) {
        pjmedia_event_unsubscribe(NULL, &vidstream_event_cb, vp, vp->strm);
	pjmedia_vid_dev_stream_destroy(vp->strm);
	vp->strm = NULL;
    }
    if (vp->client_port) {
        pjmedia_event_unsubscribe(NULL, &client_port_event_cb, vp,
                                  vp->client_port);
	if (vp->destroy_client_port)
	    pjmedia_port_destroy(vp->client_port);
	vp->client_port = NULL;
    }
    if (vp->frm_mutex) {
	pj_mutex_destroy(vp->frm_mutex);
	vp->frm_mutex = NULL;
    }
    if (vp->conv.conv) {
        pjmedia_converter_destroy(vp->conv.conv);
        vp->conv.conv = NULL;
    }
    pj_pool_release(vp->pool);
}

/*
static void save_rgb_frame(int width, int height, const pjmedia_frame *frm)
{
    static int counter;
    FILE *pFile;
    char szFilename[32];
    const pj_uint8_t *pFrame = (const pj_uint8_t*)frm->buf;
    int  y;

    if (counter > 10)
	return;

    // Open file
    sprintf(szFilename, "frame%02d.ppm", counter++);
    pFile=fopen(szFilename, "wb");
    if(pFile==NULL)
      return;

    // Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);

    // Write pixel data
    for(y=0; y<height; y++)
      fwrite(pFrame+y*width*3, 1, width*3, pFile);

    // Close file
    fclose(pFile);
}
*/

/* Handle event from vidstream */
static pj_status_t vidstream_event_cb(pjmedia_event *event,
                                      void *user_data)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    
    /* Just republish the event to our client */
    return pjmedia_event_publish(NULL, vp, event, 0);
}

static pj_status_t client_port_event_cb(pjmedia_event *event,
                                        void *user_data)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;

    if (event->type == PJMEDIA_EVENT_FMT_CHANGED) {
        const pjmedia_video_format_detail *vfd;
        const pjmedia_video_format_detail *vfd_cur;
        pjmedia_vid_dev_param vid_param;
        pj_status_t status;
        
        /* Retrieve the current video format detail */
        pjmedia_vid_dev_stream_get_param(vp->strm, &vid_param);
        vfd_cur = pjmedia_format_get_video_format_detail(
		  &vid_param.fmt, PJ_TRUE);
        if (!vfd_cur)
            return PJMEDIA_EVID_BADFORMAT;

        /* Retrieve the new video format detail */
        vfd = pjmedia_format_get_video_format_detail(
                  &event->data.fmt_changed.new_fmt, PJ_TRUE);
        if (!vfd || !vfd->fps.num || !vfd->fps.denum)
            return PJMEDIA_EVID_BADFORMAT;

	/* Ticket #1876: if this is a passive renderer and only frame rate is
	 * changing, simply modify the clock.
	 */
	if (vp->dir == PJMEDIA_DIR_RENDER &&
	    vp->stream_role == ROLE_PASSIVE && vp->role == ROLE_ACTIVE)
	{
	    pj_bool_t fps_only;
	    pjmedia_video_format_detail tmp_vfd;
	    
	    tmp_vfd = *vfd_cur;
	    tmp_vfd.fps = vfd->fps;
	    fps_only = pj_memcmp(vfd, &tmp_vfd, sizeof(*vfd)) == 0;
	    if (fps_only) {
		pjmedia_clock_param clock_param;
		clock_param.usec_interval = PJMEDIA_PTIME(&vfd->fps);
		clock_param.clock_rate = vid_param.clock_rate;
		pjmedia_clock_modify(vp->clock, &clock_param);

		return pjmedia_event_publish(NULL, vp, event,
					     PJMEDIA_EVENT_PUBLISH_POST_EVENT);
	    }
	}

	/* Ticket #1827:
	 * Stopping video port should not be necessary here because
	 * it will also try to stop the clock, from inside the clock's
	 * own thread, so it may get stuck. We just stop the video device
	 * stream instead.
	 * pjmedia_vid_port_stop(vp);
	 */
	pjmedia_vid_dev_stream_stop(vp->strm);
        
	/* Change the destination format to the new format */
	pjmedia_format_copy(&vp->conv.conv_param.src,
			    &event->data.fmt_changed.new_fmt);
	/* Only copy the size here */
	vp->conv.conv_param.dst.det.vid.size =
	    event->data.fmt_changed.new_fmt.det.vid.size;

	status = create_converter(vp);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, status, "Error recreating converter"));
	    return status;
	}

        if (vid_param.fmt.id != vp->conv.conv_param.dst.id ||
            (vid_param.fmt.det.vid.size.h !=
             vp->conv.conv_param.dst.det.vid.size.h) ||
            (vid_param.fmt.det.vid.size.w !=
             vp->conv.conv_param.dst.det.vid.size.w))
        {
            status = pjmedia_vid_dev_stream_set_cap(vp->strm,
                                                PJMEDIA_VID_DEV_CAP_FORMAT,
                                                &vp->conv.conv_param.dst);
            if (status != PJ_SUCCESS) {
		pjmedia_event e;

                PJ_PERROR(3,(THIS_FILE, status,
		    "failure in changing the format of the video device"));
                PJ_LOG(3, (THIS_FILE, "reverting to its original format: %s",
                                      status != PJMEDIA_EVID_ERR ? "success" :
                                      "failure"));

		pjmedia_event_init(&e, PJMEDIA_EVENT_VID_DEV_ERROR, NULL, vp);
		e.data.vid_dev_err.dir = vp->dir;
		e.data.vid_dev_err.status = status;
		e.data.vid_dev_err.id = (vp->dir==PJMEDIA_DIR_ENCODING?
					 vid_param.cap_id : vid_param.rend_id);
		pjmedia_event_publish(NULL, vp, &e,
				      PJMEDIA_EVENT_PUBLISH_POST_EVENT);

                return status;
            }
        }
        
        if (vp->role == ROLE_ACTIVE && vp->stream_role == ROLE_PASSIVE) {
            pjmedia_clock_param clock_param;
            
            /**
             * Initially, frm_buf was allocated the biggest
             * supported size, so we do not need to re-allocate
             * the buffer here.
             */
            /* Adjust the clock */
            clock_param.usec_interval = PJMEDIA_PTIME(&vfd->fps);
            clock_param.clock_rate = vid_param.clock_rate;
            pjmedia_clock_modify(vp->clock, &clock_param);
        }
        
	/* pjmedia_vid_port_start(vp); */
	pjmedia_vid_dev_stream_start(vp->strm);

	/* Update passive port info from the video stream */
	if (vp->role == ROLE_PASSIVE) {
	    pjmedia_format_copy(&vp->pasv_port->base.info.fmt,
				&event->data.fmt_changed.new_fmt);
	}
    }
    
    /* Republish the event, post the event to the event manager
     * to avoid deadlock if vidport is trying to stop the clock.
     */
    return pjmedia_event_publish(NULL, vp, event,
                                 PJMEDIA_EVENT_PUBLISH_POST_EVENT);
}

static pj_status_t convert_frame(pjmedia_vid_port *vp,
                                 pjmedia_frame *src_frame,
                                 pjmedia_frame *dst_frame)
{
    pj_status_t status = PJ_SUCCESS;

    if (vp->conv.conv) {
        if (!dst_frame->buf || dst_frame->size < vp->conv.conv_buf_size) {
            dst_frame->buf  = vp->conv.conv_buf;
	    dst_frame->size = vp->conv.conv_buf_size;
        }
	dst_frame->type	     = src_frame->type;
	dst_frame->timestamp = src_frame->timestamp;
	dst_frame->bit_info  = src_frame->bit_info;
	status = pjmedia_converter_convert(vp->conv.conv,
					   src_frame, dst_frame);
    }
    
    return status;
}

/* Copy frame to buffer. */
static void copy_frame_to_buffer(pjmedia_vid_port *vp,
                                 pjmedia_frame *frame)
{
    pj_mutex_lock(vp->frm_mutex);
    pjmedia_frame_copy(vp->frm_buf, frame);
    pj_mutex_unlock(vp->frm_mutex);
}

/* Get frame from buffer and convert it if necessary. */
static pj_status_t get_frame_from_buffer(pjmedia_vid_port *vp,
                                         pjmedia_frame *frame)
{
    pj_status_t status = PJ_SUCCESS;

    pj_mutex_lock(vp->frm_mutex);
    if (vp->conv.conv)
        status = convert_frame(vp, vp->frm_buf, frame);
    else
        pjmedia_frame_copy(frame, vp->frm_buf);
    pj_mutex_unlock(vp->frm_mutex);
    
    return status;
}

static void enc_clock_cb(const pj_timestamp *ts, void *user_data)
{
    /* We are here because user wants us to be active but the stream is
     * passive. So get a frame from the stream and push it to user.
     */
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    pjmedia_frame frame_;
    pj_status_t status = PJ_SUCCESS;

    pj_assert(vp->role==ROLE_ACTIVE);

    PJ_UNUSED_ARG(ts);

    if (!vp->client_port)
	return;

    if (vp->stream_role == ROLE_PASSIVE) {
        while (vp->conv.usec_ctr < vp->conv.usec_dst) {
            vp->frm_buf->size = vp->frm_buf_size;
            status = pjmedia_vid_dev_stream_get_frame(vp->strm, vp->frm_buf);
            vp->conv.usec_ctr += vp->conv.usec_src;
        }
        vp->conv.usec_ctr -= vp->conv.usec_dst;
        if (status != PJ_SUCCESS)
	    return;
    }

    //save_rgb_frame(vp->cap_size.w, vp->cap_size.h, vp->frm_buf);

    frame_.buf = vp->conv.conv_buf;
    frame_.size = vp->conv.conv_buf_size;
    status = get_frame_from_buffer(vp, &frame_);
    if (status != PJ_SUCCESS)
        return;

    status = pjmedia_port_put_frame(vp->client_port, &frame_);
    if (status != PJ_SUCCESS)
        return;
}

static void dec_clock_cb(const pj_timestamp *ts, void *user_data)
{
    /* We are here because user wants us to be active but the stream is
     * passive. So get a frame from the stream and push it to user.
     */
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    pj_status_t status;
    pjmedia_frame frame;

    pj_assert(vp->role==ROLE_ACTIVE && vp->stream_role==ROLE_PASSIVE);

    PJ_UNUSED_ARG(ts);

    if (!vp->client_port)
	return;

    status = vidstream_render_cb(vp->strm, vp, &frame);
    if (status != PJ_SUCCESS)
        return;
    
    if (frame.size > 0)
	status = pjmedia_vid_dev_stream_put_frame(vp->strm, &frame);
}

static pj_status_t vidstream_cap_cb(pjmedia_vid_dev_stream *stream,
				    void *user_data,
				    pjmedia_frame *frame)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;

    /* We just store the frame in the buffer. For active role, we let
     * video port's clock to push the frame buffer to the user.
     * The decoding counterpart for passive role and active stream is
     * located in vid_pasv_port_put_frame()
     */
    copy_frame_to_buffer(vp, frame);

    /* This is tricky since the frame is still in its original unconverted
     * format, which may not be what the application expects.
     */
    if (vp->strm_cb.capture_cb)
        return (*vp->strm_cb.capture_cb)(stream, vp->strm_cb_data, frame);
    return PJ_SUCCESS;
}

static pj_status_t vidstream_render_cb(pjmedia_vid_dev_stream *stream,
				       void *user_data,
				       pjmedia_frame *frame)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    pj_status_t status = PJ_SUCCESS;
    
    pj_bzero(frame, sizeof(pjmedia_frame));
    if (vp->role==ROLE_ACTIVE) {
        unsigned frame_ts = vp->clocksrc.clock_rate / 1000 *
                            vp->clocksrc.ptime_usec / 1000;

        if (!vp->client_port)
            return status;
        
        if (vp->sync_clocksrc.sync_clocksrc) {
            pjmedia_clock_src *src = vp->sync_clocksrc.sync_clocksrc;
            pj_int32_t diff;
            unsigned nsync_frame;
            
            /* Synchronization */
            /* Calculate the time difference (in ms) with the sync source */
            diff = pjmedia_clock_src_get_time_msec(&vp->clocksrc) -
                   pjmedia_clock_src_get_time_msec(src) -
                   vp->sync_clocksrc.sync_delta;
            
            /* Check whether sync source made a large jump */
            if (diff < 0 && -diff > PJMEDIA_CLOCK_SYNC_MAX_SYNC_MSEC) {
                pjmedia_clock_src_update(&vp->clocksrc, NULL);
                vp->sync_clocksrc.sync_delta = 
                    pjmedia_clock_src_get_time_msec(src) -
                    pjmedia_clock_src_get_time_msec(&vp->clocksrc);
                vp->sync_clocksrc.nsync_frame = 0;
                return status;
            }
            
            /* Calculate the difference (in frames) with the sync source */
            nsync_frame = abs(diff) * 1000 / vp->clocksrc.ptime_usec;
            if (nsync_frame == 0) {
                /* Nothing to sync */
                vp->sync_clocksrc.nsync_frame = 0;
            } else {
                pj_int32_t init_sync_frame = nsync_frame;
                
                /* Check whether it's a new sync or whether we need to reset
                 * the sync
                 */
                if (vp->sync_clocksrc.nsync_frame == 0 ||
                    (vp->sync_clocksrc.nsync_frame > 0 &&
                     nsync_frame > vp->sync_clocksrc.nsync_frame))
                {
                    vp->sync_clocksrc.nsync_frame = nsync_frame;
                    vp->sync_clocksrc.nsync_progress = 0;
                } else {
                    init_sync_frame = vp->sync_clocksrc.nsync_frame;
                }
                
                if (diff >= 0) {
                    unsigned skip_mod;
                    
                    /* We are too fast */
                    if (vp->sync_clocksrc.max_sync_ticks > 0) {
                        skip_mod = init_sync_frame / 
                        vp->sync_clocksrc.max_sync_ticks + 2;
                    } else
                        skip_mod = init_sync_frame + 2;
                    
                    PJ_LOG(5, (THIS_FILE, "synchronization: early by %d ms",
                               diff));
                    /* We'll play a frame every skip_mod-th tick instead of
                     * a complete pause
                     */
                    if (++vp->sync_clocksrc.nsync_progress % skip_mod > 0) {
                        pjmedia_clock_src_update(&vp->clocksrc, NULL);
                        return status;
                    }
                } else {
                    unsigned i, ndrop = init_sync_frame;
                    
                    /* We are too late, drop the frame */
                    if (vp->sync_clocksrc.max_sync_ticks > 0) {
                        ndrop /= vp->sync_clocksrc.max_sync_ticks;
                        ndrop++;
                    }
                    PJ_LOG(5, (THIS_FILE, "synchronization: late, "
                               "dropping %d frame(s)", ndrop));
                    
                    if (ndrop >= nsync_frame) {
                        vp->sync_clocksrc.nsync_frame = 0;
                        ndrop = nsync_frame;
                    } else
                        vp->sync_clocksrc.nsync_progress += ndrop;
                    
                    for (i = 0; i < ndrop; i++) {
                        vp->frm_buf->size = vp->frm_buf_size;
                        status = pjmedia_port_get_frame(vp->client_port,
                                                        vp->frm_buf);
                        if (status != PJ_SUCCESS) {
                            pjmedia_clock_src_update(&vp->clocksrc, NULL);
                            return status;
                        }
                        
                        pj_add_timestamp32(&vp->clocksrc.timestamp,
                                           frame_ts);
                    }
                }
            }
        }
        
        vp->frm_buf->size = vp->frm_buf_size;
        status = pjmedia_port_get_frame(vp->client_port, vp->frm_buf);
        if (status != PJ_SUCCESS) {
            pjmedia_clock_src_update(&vp->clocksrc, NULL);
            return status;
        }
        pj_add_timestamp32(&vp->clocksrc.timestamp, frame_ts);
        pjmedia_clock_src_update(&vp->clocksrc, NULL);

        status = convert_frame(vp, vp->frm_buf, frame);
	if (status != PJ_SUCCESS)
            return status;

	if (!vp->conv.conv)
	    pj_memcpy(frame, vp->frm_buf, sizeof(*frame));
    } else {
        /* The stream is active while we are passive so we need to get the
         * frame from the buffer.
         * The encoding counterpart is located in vid_pasv_port_get_frame()
         */
        get_frame_from_buffer(vp, frame);
    }
    if (vp->strm_cb.render_cb)
        return (*vp->strm_cb.render_cb)(stream, vp->strm_cb_data, frame);
    return PJ_SUCCESS;
}

static pj_status_t vid_pasv_port_put_frame(struct pjmedia_port *this_port,
					   pjmedia_frame *frame)
{
    struct vid_pasv_port *vpp = (struct vid_pasv_port*)this_port;
    pjmedia_vid_port *vp = vpp->vp;

    if (vp->stream_role==ROLE_PASSIVE) {
        /* We are passive and the stream is passive.
         * The encoding counterpart is in vid_pasv_port_get_frame().
         */
        pj_status_t status;
        pjmedia_frame frame_;
        
        pj_bzero(&frame_, sizeof(frame_));
        status = convert_frame(vp, frame, &frame_);
        if (status != PJ_SUCCESS)
            return status;

	return pjmedia_vid_dev_stream_put_frame(vp->strm, (vp->conv.conv?
                                                           &frame_: frame));
    } else {
        /* We are passive while the stream is active so we just store the
         * frame in the buffer.
         * The encoding counterpart is located in vidstream_cap_cb()
         */
        copy_frame_to_buffer(vp, frame);
    }

    return PJ_SUCCESS;
}

static pj_status_t vid_pasv_port_get_frame(struct pjmedia_port *this_port,
					   pjmedia_frame *frame)
{
    struct vid_pasv_port *vpp = (struct vid_pasv_port*)this_port;
    pjmedia_vid_port *vp = vpp->vp;
    pj_status_t status = PJ_SUCCESS;

    if (vp->stream_role==ROLE_PASSIVE) {
        /* We are passive and the stream is passive.
         * The decoding counterpart is in vid_pasv_port_put_frame().
         */
    	pjmedia_frame *get_frm = vp->conv.conv? vp->frm_buf : frame;

    	if (vp->conv.conv)
            get_frm->size = vp->frm_buf_size;

    	status = pjmedia_vid_dev_stream_get_frame(vp->strm, get_frm);
	if (status != PJ_SUCCESS)
	    return status;

        status = convert_frame(vp, vp->frm_buf, frame);
    } else {
        /* The stream is active while we are passive so we need to get the
         * frame from the buffer.
         * The decoding counterpart is located in vidstream_rend_cb()
         */
        get_frame_from_buffer(vp, frame);
    }

    return status;
}


#endif /* PJMEDIA_HAS_VIDEO */

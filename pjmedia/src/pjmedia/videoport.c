/* $Id$ */
/*
 * Copyright (C) 2008-2010 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/videoport.h>
#include <pjmedia/clock.h>
#include <pjmedia/converter.h>
#include <pjmedia/errno.h>
#include <pjmedia/vid_codec.h>
#include <pj/log.h>
#include <pj/pool.h>

#define THIS_FILE	"videoport.c"

typedef struct vid_pasv_port vid_pasv_port;

enum role
{
    ROLE_NONE,
    ROLE_ACTIVE,
    ROLE_PASSIVE
};

struct pjmedia_vid_port
{
    pj_str_t		 dev_name;
    pjmedia_dir		 dir;
    pjmedia_rect_size	 cap_size;
    pjmedia_vid_dev_stream	*strm;
    pjmedia_vid_cb       strm_cb;
    void                *strm_cb_data;
    enum role		 role,
			 stream_role;
    vid_pasv_port	*pasv_port;
    pjmedia_port	*client_port;
    pj_bool_t		 destroy_client_port;

    pjmedia_converter	*cap_conv;
    void		*cap_conv_buf;
    pj_size_t		 cap_conv_buf_size;

    pjmedia_clock	*enc_clock,
		        *dec_clock;

    pjmedia_clock_src    cap_clocksrc,
                         rend_clocksrc;

    struct sync_clock_src_t
    {
        pjmedia_clock_src   *sync_clocksrc;
        pj_int32_t           sync_delta;
        unsigned             max_sync_ticks;
        unsigned             nsync_frame;
        unsigned             nsync_progress;
    } cap_sync_clocksrc, rend_sync_clocksrc;

    pjmedia_frame	*enc_frm_buf,
		        *dec_frm_buf;
    pj_size_t            enc_frm_buf_size,
                         dec_frm_buf_size;

    pj_mutex_t		*enc_frm_mutex,
			*dec_frm_mutex;
};

struct vid_pasv_port
{
    pjmedia_port	 base;
    pjmedia_vid_port	*vp;
};

static pj_status_t vidstream_cap_cb(pjmedia_vid_dev_stream *stream,
				    void *user_data,
				    pjmedia_frame *frame);
static pj_status_t vidstream_render_cb(pjmedia_vid_dev_stream *stream,
				       void *user_data,
				       pjmedia_frame *frame);
static pj_status_t vidstream_event_cb(pjmedia_vid_dev_stream *stream,
			              void *user_data,
                                      pjmedia_vid_event *event);

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
    case PJMEDIA_DIR_CAPTURE_RENDER:
	return "capture and render";
    default:
	return "??";
    }
}

PJ_DEF(pj_status_t) pjmedia_vid_port_create( pj_pool_t *pool,
					     const pjmedia_vid_port_param *prm,
					     pjmedia_vid_port **p_vid_port)
{
    pjmedia_vid_port *vp;
    const pjmedia_video_format_detail *vfd;
    char cap_dev_name[64], rend_dev_name[64];
    pjmedia_vid_cb vid_cb;
    pj_bool_t need_frame_buf = PJ_FALSE;
    pj_status_t status;
    unsigned ptime_usec;
    pjmedia_vid_param vparam;
    pjmedia_conversion_param conv_param;

    PJ_ASSERT_RETURN(pool && prm && p_vid_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(prm->vidparam.fmt.type == PJMEDIA_TYPE_VIDEO &&
                     prm->vidparam.dir != PJMEDIA_DIR_NONE,
		     PJ_EINVAL);

    /* Retrieve the video format detail */
    vfd = pjmedia_format_get_video_format_detail(&prm->vidparam.fmt, PJ_TRUE);
    if (!vfd)
	return PJ_EINVAL;

    PJ_ASSERT_RETURN(vfd->fps.num, PJ_EINVAL);

    /* Allocate videoport */
    vp = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_port);
    vp->role = prm->active ? ROLE_ACTIVE : ROLE_PASSIVE;
    vp->dir = prm->vidparam.dir;
    vp->cap_size = vfd->size;

    vparam = prm->vidparam;
    cap_dev_name[0] = rend_dev_name[0] = '\0';

    if (vp->dir & PJMEDIA_DIR_CAPTURE) {
	pjmedia_vid_dev_info di;
	unsigned i;

	/* Get device info */
	status = pjmedia_vid_dev_get_info(prm->vidparam.cap_id, &di);
	if (status != PJ_SUCCESS)
	    return status;

	pj_ansi_snprintf(cap_dev_name, sizeof(cap_dev_name), "%s [%s]",
	                 di.name, di.driver);

	for (i = 0; i < di.fmt_cnt; ++i) {
	    if (prm->vidparam.fmt.id == di.fmt[i].id)
		break;
	}

	if (i == di.fmt_cnt) {
	    /* The device has no no matching format. Pick one from
	     * the supported formats, and later use converter to
	     * convert it to the required format.
	     */
	    pj_assert(di.fmt_cnt != 0);
	    vparam.fmt.id = di.fmt[0].id;
	}

	pj_strdup2_with_null(pool, &vp->dev_name, di.name);
	vp->stream_role = di.has_callback ? ROLE_ACTIVE : ROLE_PASSIVE;
    }

    if (vp->dir & PJMEDIA_DIR_RENDER) {
	pjmedia_vid_dev_info di;
	unsigned i;

	/* Get device info */
	status = pjmedia_vid_dev_get_info(prm->vidparam.rend_id, &di);
	if (status != PJ_SUCCESS)
	    return status;

	pj_ansi_snprintf(rend_dev_name, sizeof(rend_dev_name), "and %s [%s]",
	                 di.name, di.driver);

	for (i = 0; i < di.fmt_cnt; ++i) {
	    if (prm->vidparam.fmt.id == di.fmt[i].id)
		break;
	}

	if (i == di.fmt_cnt) {
	    /* The device has no no matching format. Pick one from
	     * the supported formats, and later use converter to
	     * convert it to the required format.
	     */
	    pj_assert(di.fmt_cnt != 0);
	    vparam.fmt.id = di.fmt[0].id;
	}

	pj_strdup2_with_null(pool, &vp->dev_name, di.name);
	vp->stream_role = di.has_callback ? ROLE_ACTIVE : ROLE_PASSIVE;
    }

    PJ_LOG(4,(THIS_FILE,
	      "Opening device %s%s for %s: format=%s, size=%dx%d @%d:%d fps",
	      cap_dev_name, rend_dev_name,
	      vid_dir_name(prm->vidparam.dir),
	      pjmedia_get_video_format_info(NULL, vparam.fmt.id)->name,
	      vfd->size.w, vfd->size.h,
	      vfd->fps.num, vfd->fps.denum));

    ptime_usec = PJMEDIA_PTIME(&vfd->fps);
    pjmedia_clock_src_init(&vp->cap_clocksrc, PJMEDIA_TYPE_VIDEO,
                           prm->vidparam.clock_rate, ptime_usec);
    pjmedia_clock_src_init(&vp->rend_clocksrc, PJMEDIA_TYPE_VIDEO,
                           prm->vidparam.clock_rate, ptime_usec);
    vp->cap_sync_clocksrc.max_sync_ticks = 
        PJMEDIA_CLOCK_SYNC_MAX_RESYNC_DURATION *
        1000 / vp->cap_clocksrc.ptime_usec;
    vp->rend_sync_clocksrc.max_sync_ticks = 
        PJMEDIA_CLOCK_SYNC_MAX_RESYNC_DURATION *
        1000 / vp->rend_clocksrc.ptime_usec;

    /* Create the video stream */
    pj_bzero(&vid_cb, sizeof(vid_cb));
    vid_cb.capture_cb = &vidstream_cap_cb;
    vid_cb.render_cb = &vidstream_render_cb;
    vid_cb.on_event_cb = &vidstream_event_cb;

    status = pjmedia_vid_dev_stream_create(&vparam, &vid_cb, vp,
				           &vp->strm);
    if (status != PJ_SUCCESS)
	goto on_error;

    PJ_LOG(4,(THIS_FILE,
	      "Device %s%s opened: format=%s, size=%dx%d @%d:%d fps",
	      cap_dev_name, rend_dev_name,
	      pjmedia_get_video_format_info(NULL, vparam.fmt.id)->name,
	      vparam.fmt.det.vid.size.w, vparam.fmt.det.vid.size.h,
	      vparam.fmt.det.vid.fps.num, vparam.fmt.det.vid.fps.denum));

    /* Instantiate converter if necessary */
    if (vparam.fmt.id != prm->vidparam.fmt.id ||
	vparam.fmt.det.vid.size.w != prm->vidparam.fmt.det.vid.size.w ||
	vparam.fmt.det.vid.size.h != prm->vidparam.fmt.det.vid.size.h /*||
	vparam.fmt.det.vid.fps.num != prm->vidparam.fmt.det.vid.fps.num ||
	vparam.fmt.det.vid.fps.denum != prm->vidparam.fmt.det.vid.fps.denum*/)
    {
	/* Yes, we need converter */
	const pjmedia_video_format_info *vfi;
	pjmedia_video_apply_fmt_param vafp;

	if (vp->dir & PJMEDIA_DIR_CAPTURE) {
	    pjmedia_format_copy(&conv_param.src, &vparam.fmt);
	    pjmedia_format_copy(&conv_param.dst, &prm->vidparam.fmt);

	    status = pjmedia_converter_create(NULL, pool, &conv_param,
					      &vp->cap_conv);
	} else {
	    pjmedia_format_copy(&conv_param.src, &prm->vidparam.fmt);
	    pjmedia_format_copy(&conv_param.dst, &vparam.fmt);

	    status = pjmedia_converter_create(NULL, pool, &conv_param,
					      &vp->cap_conv);
	}

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, status, "Error creating converter"));
	    goto on_error;
	}

	/* Allocate buffer for conversion */
	vfi = pjmedia_get_video_format_info(NULL, conv_param.dst.id);
	if (!vfi)
	    return PJMEDIA_EBADFMT;

	pj_bzero(&vafp, sizeof(vafp));
	vafp.size = conv_param.dst.det.vid.size;
	status = vfi->apply_fmt(vfi, &vafp);
	if (status != PJ_SUCCESS)
	    return PJMEDIA_EBADFMT;

	vp->cap_conv_buf = pj_pool_alloc(pool, vafp.framebytes);
	vp->cap_conv_buf_size = vafp.framebytes;
    }

    if (vp->role==ROLE_ACTIVE && vp->stream_role==ROLE_PASSIVE) {
	/* Active role is wanted, but our device is passive, so create
	 * master clocks to run the media flow.
	 */
	need_frame_buf = PJ_TRUE;

	if (vp->dir & PJMEDIA_DIR_ENCODING) {
            pjmedia_clock_param param;
            
            param.usec_interval = PJMEDIA_PTIME(&vfd->fps);
            param.clock_rate = prm->vidparam.clock_rate;
	    status = pjmedia_clock_create2(pool, &param,
					   PJMEDIA_CLOCK_NO_HIGHEST_PRIO,
					   &enc_clock_cb, vp, &vp->enc_clock);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}

	if (vp->dir & PJMEDIA_DIR_DECODING) {
            pjmedia_clock_param param;
            
            param.usec_interval = PJMEDIA_PTIME(&vfd->fps);
            param.clock_rate = prm->vidparam.clock_rate;
	    status = pjmedia_clock_create2(pool, &param,
					   PJMEDIA_CLOCK_NO_HIGHEST_PRIO,
					   &dec_clock_cb, vp, &vp->dec_clock);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}

    } else if (vp->role==ROLE_PASSIVE) {
	vid_pasv_port *pp;

	/* Always need to create media port for passive role */
	vp->pasv_port = pp = PJ_POOL_ZALLOC_T(pool, vid_pasv_port);
	pp->vp = vp;
	pp->base.get_frame = &vid_pasv_port_get_frame;
	pp->base.put_frame = &vid_pasv_port_put_frame;
	pjmedia_port_info_init2(&pp->base.info, &vp->dev_name,
				PJMEDIA_PORT_SIGNATURE('v', 'i', 'd', 'p'),
			        prm->vidparam.dir, &prm->vidparam.fmt);

	if (vp->stream_role == ROLE_ACTIVE || vp->cap_conv) {
	    need_frame_buf = PJ_TRUE;
	}
    }

    if (need_frame_buf) {
	const pjmedia_video_format_info *vfi;
	pjmedia_video_apply_fmt_param vafp;

	vfi = pjmedia_get_video_format_info(NULL, vp->cap_conv?
					    conv_param.src.id:
					    vparam.fmt.id);
	if (!vfi) {
	    status = PJ_ENOTFOUND;
	    goto on_error;
	}

	pj_bzero(&vafp, sizeof(vafp));
	vafp.size = (vp->cap_conv? conv_param.src.det.vid.size:
		     vparam.fmt.det.vid.size);
	status = vfi->apply_fmt(vfi, &vafp);
	if (status != PJ_SUCCESS)
	    goto on_error;

	if (vp->dir & PJMEDIA_DIR_ENCODING) {
	    vp->enc_frm_buf = PJ_POOL_ZALLOC_T(pool, pjmedia_frame);
	    if (vp->stream_role == ROLE_ACTIVE && vp->cap_conv) {
		vp->enc_frm_buf_size = vp->cap_conv_buf_size;
		vp->enc_frm_buf->buf = vp->cap_conv_buf;
	    } else {
		vp->enc_frm_buf_size = vafp.framebytes;
		vp->enc_frm_buf->buf = pj_pool_alloc(pool, vafp.framebytes);
	    }
	    vp->enc_frm_buf->size = vp->enc_frm_buf_size;
	    vp->enc_frm_buf->type = PJMEDIA_FRAME_TYPE_NONE;

	    status = pj_mutex_create_simple(pool, vp->dev_name.ptr,
					    &vp->enc_frm_mutex);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}

	if (vp->dir & PJMEDIA_DIR_DECODING) {
	    vp->dec_frm_buf = PJ_POOL_ZALLOC_T(pool, pjmedia_frame);
            vp->dec_frm_buf_size = vafp.framebytes;
	    vp->dec_frm_buf->buf = pj_pool_alloc(pool, vafp.framebytes);
	    vp->dec_frm_buf->size = vafp.framebytes;
	    vp->dec_frm_buf->type = PJMEDIA_FRAME_TYPE_NONE;

	    status = pj_mutex_create_simple(pool, vp->dev_name.ptr,
					    &vp->dec_frm_mutex);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    *p_vid_port = vp;

    return PJ_SUCCESS;

on_error:
    pjmedia_vid_port_destroy(vp);
    return status;
}

PJ_DEF(void) pjmedia_vid_port_set_cb(pjmedia_vid_port *vid_port,
				     const pjmedia_vid_cb *cb,
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
pjmedia_vid_port_get_clock_src( pjmedia_vid_port *vid_port,
                                pjmedia_dir dir )
{
    return (dir == PJMEDIA_DIR_CAPTURE? &vid_port->cap_clocksrc:
            &vid_port->rend_clocksrc);
}

PJ_DECL(pj_status_t)
pjmedia_vid_port_set_clock_src( pjmedia_vid_port *vid_port,
                                pjmedia_dir dir,
                                pjmedia_clock_src *clocksrc)
{
    pjmedia_clock_src *vclocksrc;
    struct sync_clock_src_t *sync_src;

    PJ_ASSERT_RETURN(vid_port && clocksrc, PJ_EINVAL);

    vclocksrc = (dir == PJMEDIA_DIR_CAPTURE? &vid_port->cap_clocksrc:
                 &vid_port->rend_clocksrc);
    sync_src = (dir == PJMEDIA_DIR_CAPTURE? &vid_port->cap_sync_clocksrc:
                &vid_port->rend_sync_clocksrc);
    sync_src->sync_clocksrc = clocksrc;
    sync_src->sync_delta = pjmedia_clock_src_get_time_msec(vclocksrc) -
                           pjmedia_clock_src_get_time_msec(clocksrc);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_vid_port_connect(pjmedia_vid_port *vp,
					      pjmedia_port *port,
					      pj_bool_t destroy)
{
    PJ_ASSERT_RETURN(vp && vp->role==ROLE_ACTIVE, PJ_EINVAL);
    vp->destroy_client_port = destroy;
    vp->client_port = port;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_vid_port_disconnect(pjmedia_vid_port *vp)
{
    PJ_ASSERT_RETURN(vp && vp->role==ROLE_ACTIVE, PJ_EINVAL);
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

    if (vp->enc_clock) {
	status = pjmedia_clock_start(vp->enc_clock);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    if (vp->dec_clock) {
	status = pjmedia_clock_start(vp->dec_clock);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    return PJ_SUCCESS;

on_error:
    pjmedia_vid_port_stop(vp);
    return status;
}

PJ_DEF(pj_status_t) pjmedia_vid_port_stop(pjmedia_vid_port *vp)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(vp, PJ_EINVAL);

    status = pjmedia_vid_dev_stream_stop(vp->strm);

    if (vp->enc_clock) {
	status = pjmedia_clock_stop(vp->enc_clock);
    }

    if (vp->dec_clock) {
	status = pjmedia_clock_stop(vp->dec_clock);
    }

    return status;
}

PJ_DEF(void) pjmedia_vid_port_destroy(pjmedia_vid_port *vp)
{
    PJ_ASSERT_ON_FAIL(vp, return);

    PJ_LOG(4,(THIS_FILE, "Closing %s..", vp->dev_name.ptr));

    if (vp->enc_clock) {
	pjmedia_clock_destroy(vp->enc_clock);
	vp->enc_clock = NULL;
    }
    if (vp->dec_clock) {
	pjmedia_clock_destroy(vp->dec_clock);
	vp->dec_clock = NULL;
    }
    if (vp->strm) {
	pjmedia_vid_dev_stream_destroy(vp->strm);
	vp->strm = NULL;
    }
    if (vp->client_port) {
	if (vp->destroy_client_port)
	    pjmedia_port_destroy(vp->client_port);
	vp->client_port = NULL;
    }
    if (vp->enc_frm_mutex) {
	pj_mutex_destroy(vp->enc_frm_mutex);
	vp->enc_frm_mutex = NULL;
    }
    if (vp->dec_frm_mutex) {
	pj_mutex_destroy(vp->dec_frm_mutex);
	vp->dec_frm_mutex = NULL;
    }

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

static pj_status_t detect_fmt_change(pjmedia_vid_port *vp,
                                     pjmedia_frame *frame)
{
    if (frame->bit_info & PJMEDIA_VID_CODEC_EVENT_FMT_CHANGED) {
        const pjmedia_video_format_detail *vfd;
        pjmedia_vid_event pevent;
        pj_status_t status;

        /* Retrieve the video format detail */
        vfd = pjmedia_format_get_video_format_detail(
                  &vp->client_port->info.fmt, PJ_TRUE);
        if (!vfd)
            return PJMEDIA_EVID_BADFORMAT;
        pj_assert(vfd->fps.num);

        status = pjmedia_vid_dev_stream_set_cap(
                     vp->strm,
                     PJMEDIA_VID_DEV_CAP_FORMAT,
                     &vp->client_port->info.fmt);
        if (status != PJ_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "failure in changing the format of the "
                                  "video device"));
            PJ_LOG(3, (THIS_FILE, "reverting to its original format: %s",
                                  status != PJMEDIA_EVID_ERR ? "success" :
                                  "failure"));
            pjmedia_vid_port_stop(vp);
            return status;
        }

        if (vp->stream_role == ROLE_PASSIVE) {
            pjmedia_vid_param vid_param;
            pjmedia_clock_param clock_param;

            /**
             * Initially, dec_frm_buf was allocated the biggest
             * supported size, so we do not need to re-allocate
             * the buffer here.
             */
            /* Adjust the clock */
            pjmedia_vid_dev_stream_get_param(vp->strm, &vid_param);
            clock_param.usec_interval = PJMEDIA_PTIME(&vfd->fps);
            clock_param.clock_rate = vid_param.clock_rate;
            pjmedia_clock_modify(vp->dec_clock, &clock_param);
        }

        /* Notify application of the format change. */
        pevent.event_type = PJMEDIA_EVENT_FMT_CHANGED;
        pj_memcpy(&pevent.event_desc.fmt_change.new_format,
                  &vp->client_port->info.fmt, sizeof(pjmedia_format));
        if (vp->strm_cb.on_event_cb)
            (*vp->strm_cb.on_event_cb)(vp->strm, vp->strm_cb_data, &pevent);
    }

    return PJ_SUCCESS;
}

static void enc_clock_cb(const pj_timestamp *ts, void *user_data)
{
    /* We are here because user wants us to be active but the stream is
     * passive. So get a frame from the stream and push it to user.
     */
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    pjmedia_frame frame;
    pj_status_t status;

    pj_assert(vp->role==ROLE_ACTIVE && vp->stream_role==ROLE_PASSIVE);

    PJ_UNUSED_ARG(ts);

    if (!vp->client_port)
	return;

    vp->enc_frm_buf->size = vp->enc_frm_buf_size;
    status = pjmedia_vid_dev_stream_get_frame(vp->strm, vp->enc_frm_buf);
    if (status != PJ_SUCCESS)
	return;

    //save_rgb_frame(vp->cap_size.w, vp->cap_size.h, vp->enc_frm_buf);

    if (vp->cap_conv) {
	frame.buf  = vp->cap_conv_buf;
	frame.size = vp->cap_conv_buf_size;
	status = pjmedia_converter_convert(vp->cap_conv,
					   vp->enc_frm_buf, &frame);
	if (status != PJ_SUCCESS)
	    return;
    }

    status = pjmedia_port_put_frame(vp->client_port, (vp->cap_conv?
						      &frame:
						      vp->enc_frm_buf));
}

static void dec_clock_cb(const pj_timestamp *ts, void *user_data)
{
    /* We are here because user wants us to be active but the stream is
     * passive. So get a frame from the stream and push it to user.
     */
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    pj_status_t status;
    pjmedia_frame frame;
    unsigned frame_ts = vp->rend_clocksrc.clock_rate / 1000 *
                        vp->rend_clocksrc.ptime_usec / 1000;

    pj_assert(vp->role==ROLE_ACTIVE && vp->stream_role==ROLE_PASSIVE);

    PJ_UNUSED_ARG(ts);

    if (!vp->client_port)
	return;

    if (vp->rend_sync_clocksrc.sync_clocksrc) {
        pjmedia_clock_src *src = vp->rend_sync_clocksrc.sync_clocksrc;
        pj_int32_t diff;
        unsigned nsync_frame;

        /* Synchronization */
        /* Calculate the time difference (in ms) with the sync source */
        diff = pjmedia_clock_src_get_time_msec(&vp->rend_clocksrc) -
               pjmedia_clock_src_get_time_msec(src) -
               vp->rend_sync_clocksrc.sync_delta;

        /* Check whether sync source made a large jump */
        if (diff < 0 && -diff > PJMEDIA_CLOCK_SYNC_MAX_SYNC_MSEC) {
            pjmedia_clock_src_update(&vp->rend_clocksrc, NULL);
            vp->rend_sync_clocksrc.sync_delta = 
                pjmedia_clock_src_get_time_msec(src) -
                pjmedia_clock_src_get_time_msec(&vp->rend_clocksrc);
            vp->rend_sync_clocksrc.nsync_frame = 0;
            return;
        }

        /* Calculate the difference (in frames) with the sync source */
        nsync_frame = abs(diff) * 1000 / vp->rend_clocksrc.ptime_usec;
        if (nsync_frame == 0) {
            /* Nothing to sync */
            vp->rend_sync_clocksrc.nsync_frame = 0;
        } else {
            pj_int32_t init_sync_frame = nsync_frame;

            /* Check whether it's a new sync or whether we need to reset
             * the sync
             */
            if (vp->rend_sync_clocksrc.nsync_frame == 0 ||
                (vp->rend_sync_clocksrc.nsync_frame > 0 &&
                 nsync_frame > vp->rend_sync_clocksrc.nsync_frame))
            {
                vp->rend_sync_clocksrc.nsync_frame = nsync_frame;
                vp->rend_sync_clocksrc.nsync_progress = 0;
            } else {
                init_sync_frame = vp->rend_sync_clocksrc.nsync_frame;
            }

            if (diff >= 0) {
                unsigned skip_mod;

                /* We are too fast */
                if (vp->rend_sync_clocksrc.max_sync_ticks > 0) {
                    skip_mod = init_sync_frame / 
                               vp->rend_sync_clocksrc.max_sync_ticks + 2;
                } else
                    skip_mod = init_sync_frame + 2;

                PJ_LOG(5, (THIS_FILE, "synchronization: early by %d ms",
                           diff));
                /* We'll play a frame every skip_mod-th tick instead of
                 * a complete pause
                 */
                if (++vp->rend_sync_clocksrc.nsync_progress % skip_mod > 0) {
                    pjmedia_clock_src_update(&vp->rend_clocksrc, NULL);
                    return;
                }
            } else {
                unsigned i, ndrop = init_sync_frame;

                /* We are too late, drop the frame */
                if (vp->rend_sync_clocksrc.max_sync_ticks > 0) {
                    ndrop /= vp->rend_sync_clocksrc.max_sync_ticks;
                    ndrop++;
                }
                PJ_LOG(5, (THIS_FILE, "synchronization: late, "
                                      "dropping %d frame(s)", ndrop));

                if (ndrop >= nsync_frame) {
                    vp->rend_sync_clocksrc.nsync_frame = 0;
                    ndrop = nsync_frame;
                } else
                    vp->rend_sync_clocksrc.nsync_progress += ndrop;

                for (i = 0; i < ndrop; i++) {
                    vp->dec_frm_buf->size = vp->dec_frm_buf_size;
                    status = pjmedia_port_get_frame(vp->client_port,
                                                    vp->dec_frm_buf);
                    if (status != PJ_SUCCESS) {
                        pjmedia_clock_src_update(&vp->rend_clocksrc, NULL);
                        return;
                    }

                    status = detect_fmt_change(vp, vp->dec_frm_buf);
                    if (status != PJ_SUCCESS)
                        return;

                    pj_add_timestamp32(&vp->rend_clocksrc.timestamp,
                                       frame_ts);
                }
            }
        }
    }

    vp->dec_frm_buf->size = vp->dec_frm_buf_size;
    status = pjmedia_port_get_frame(vp->client_port, vp->dec_frm_buf);
    if (status != PJ_SUCCESS) {
        pjmedia_clock_src_update(&vp->rend_clocksrc, NULL);
	return;
    }
    pj_add_timestamp32(&vp->rend_clocksrc.timestamp, frame_ts);
    pjmedia_clock_src_update(&vp->rend_clocksrc, NULL);

    status = detect_fmt_change(vp, vp->dec_frm_buf);
    if (status != PJ_SUCCESS)
        return;

    if (vp->cap_conv) {
	frame.buf  = vp->cap_conv_buf;
	frame.size = vp->cap_conv_buf_size;
	status = pjmedia_converter_convert(vp->cap_conv,
					   vp->dec_frm_buf, &frame);
	if (status != PJ_SUCCESS)
	    return;
    }

    status = pjmedia_vid_dev_stream_put_frame(vp->strm, (vp->cap_conv?
							 &frame:
							 vp->dec_frm_buf));
}

static pj_status_t vidstream_cap_cb(pjmedia_vid_dev_stream *stream,
				    void *user_data,
				    pjmedia_frame *frame)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    pjmedia_frame frame_;

    if (vp->cap_conv) {
	pj_status_t status;

	frame_.buf  = vp->cap_conv_buf;
	frame_.size = vp->cap_conv_buf_size;
	status = pjmedia_converter_convert(vp->cap_conv,
					   frame, &frame_);
	if (status != PJ_SUCCESS)
	    return status;
	frame = &frame_;
    }

    if (vp->role==ROLE_ACTIVE) {
        if (vp->client_port)
	    return pjmedia_port_put_frame(vp->client_port, frame);
    } else {
	if (!vp->cap_conv) {
	    pj_mutex_lock(vp->enc_frm_mutex);
	    pjmedia_frame_copy(vp->enc_frm_buf, frame);
	    pj_mutex_unlock(vp->enc_frm_mutex);
	}
    }
    if (vp->strm_cb.capture_cb)
        return (*vp->strm_cb.capture_cb)(stream, vp->strm_cb_data, frame);
    return PJ_SUCCESS;
}

static pj_status_t vidstream_render_cb(pjmedia_vid_dev_stream *stream,
				       void *user_data,
				       pjmedia_frame *frame)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;

    if (vp->role==ROLE_ACTIVE) {
        if (vp->client_port) {
            pj_status_t status;

	    status = pjmedia_port_get_frame(vp->client_port, frame);
            if (status != PJ_SUCCESS)
                return status;

            return detect_fmt_change(vp, frame);
        }
    } else {
	pj_mutex_lock(vp->dec_frm_mutex);
	pjmedia_frame_copy(frame, vp->dec_frm_buf);
	pj_mutex_unlock(vp->dec_frm_mutex);
    }
    if (vp->strm_cb.render_cb)
        return (*vp->strm_cb.render_cb)(stream, vp->strm_cb_data, frame);
    return PJ_SUCCESS;
}

static pj_status_t vidstream_event_cb(pjmedia_vid_dev_stream *stream,
				      void *user_data,
				      pjmedia_vid_event *event)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;

    if (vp->strm_cb.on_event_cb)
        return (*vp->strm_cb.on_event_cb)(stream, vp->strm_cb_data, event);
    return PJ_SUCCESS;
}

static pj_status_t vid_pasv_port_put_frame(struct pjmedia_port *this_port,
					   pjmedia_frame *frame)
{
    struct vid_pasv_port *vpp = (struct vid_pasv_port*)this_port;
    pjmedia_vid_port *vp = vpp->vp;
    pjmedia_frame frame_;
    
    if (vp->cap_conv) {
	pj_status_t status;
	
	frame_.buf  = vp->cap_conv_buf;
	frame_.size = vp->cap_conv_buf_size;
	status = pjmedia_converter_convert(vp->cap_conv,
					   frame, &frame_);
	if (status != PJ_SUCCESS)
	    return status;
	frame = &frame_;
    }    

    if (vp->stream_role==ROLE_PASSIVE) {
	return pjmedia_vid_dev_stream_put_frame(vp->strm, frame);
    } else {
	pj_mutex_lock(vp->dec_frm_mutex);
	pjmedia_frame_copy(vp->dec_frm_buf, frame);
	pj_mutex_unlock(vp->dec_frm_mutex);
    }

    return PJ_SUCCESS;
}

static pj_status_t vid_pasv_port_get_frame(struct pjmedia_port *this_port,
					   pjmedia_frame *frame)
{
    struct vid_pasv_port *vpp = (struct vid_pasv_port*)this_port;
    pjmedia_vid_port *vp = vpp->vp;
    pj_status_t status;

    if (vp->stream_role==ROLE_PASSIVE) {
	if (vp->cap_conv)
	    vp->enc_frm_buf->size = vp->enc_frm_buf_size;
	status = pjmedia_vid_dev_stream_get_frame(vp->strm,
						  (vp->cap_conv?
						   vp->enc_frm_buf:
						   frame));
	if (status != PJ_SUCCESS)
	    return status;

	if (vp->cap_conv) {
	    status = pjmedia_converter_convert(vp->cap_conv,
					       vp->enc_frm_buf, frame);
	    if (status != PJ_SUCCESS)
		return status;
	}	
    } else {
	pj_mutex_lock(vp->enc_frm_mutex);
	pjmedia_frame_copy(frame, vp->enc_frm_buf);
	pj_mutex_unlock(vp->enc_frm_mutex);
    }

    return PJ_SUCCESS;
}


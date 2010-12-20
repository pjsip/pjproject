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
    pjmedia_vid_stream	*strm;
    pjmedia_vid_cb       strm_cb;
    void                *strm_cb_data;
    enum role		 role,
			 stream_role;
    vid_pasv_port	*pasv_port;
    pjmedia_port	*client_port;
    pj_bool_t		 destroy_client_port;

    pjmedia_clock	*enc_clock,
		        *dec_clock;

    pjmedia_frame	*enc_frm_buf,
		        *dec_frm_buf;

    pj_mutex_t		*enc_frm_mutex,
			*dec_frm_mutex;
};

struct vid_pasv_port
{
    pjmedia_port	 base;
    pjmedia_vid_port	*vp;
};

static pj_status_t vidstream_cap_cb(pjmedia_vid_stream *stream,
				    void *user_data,
				    pjmedia_frame *frame);
static pj_status_t vidstream_render_cb(pjmedia_vid_stream *stream,
				       void *user_data,
				       pjmedia_frame *frame);
static pj_status_t vidstream_event_cb(pjmedia_vid_stream *stream,
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
}

PJ_DEF(pj_status_t) pjmedia_vid_port_create( pj_pool_t *pool,
					     const pjmedia_vid_port_param *prm,
					     pjmedia_vid_port **p_vid_port)
{
    pjmedia_vid_port *vp;
    pjmedia_vid_dev_index dev_id = PJMEDIA_VID_INVALID_DEV;
    pjmedia_vid_dev_info di;
    const pjmedia_video_format_detail *vfd;
    pjmedia_vid_cb vid_cb;
    pj_bool_t need_frame_buf = PJ_FALSE;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && prm && p_vid_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(prm->vidparam.fmt.type == PJMEDIA_TYPE_VIDEO,
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

    /* Determine the device id to get the video device info */
    if ((vp->dir & PJMEDIA_DIR_CAPTURE) &&
        prm->vidparam.cap_id != PJMEDIA_VID_INVALID_DEV)
    {
	dev_id = prm->vidparam.cap_id;
    } else if ((vp->dir & PJMEDIA_DIR_RENDER) &&
               prm->vidparam.rend_id != PJMEDIA_VID_INVALID_DEV)
    {
	dev_id = prm->vidparam.rend_id;
    } else
	return PJ_EINVAL;

    /* Get device info */
    status = pjmedia_vid_dev_get_info(dev_id, &di);
    if (status != PJ_SUCCESS)
	return status;

    PJ_LOG(4,(THIS_FILE, "Opening %s..", di.name));

    pj_strdup2_with_null(pool, &vp->dev_name, di.name);
    vp->stream_role = di.has_callback ? ROLE_ACTIVE : ROLE_PASSIVE;

    /* Create the video stream */
    pj_bzero(&vid_cb, sizeof(vid_cb));
    vid_cb.capture_cb = &vidstream_cap_cb;
    vid_cb.render_cb = &vidstream_render_cb;
    vid_cb.on_event_cb = &vidstream_event_cb;

    status = pjmedia_vid_stream_create(&prm->vidparam, &vid_cb, vp,
				       &vp->strm);
    if (status != PJ_SUCCESS)
	goto on_error;

    if (vp->role==ROLE_ACTIVE && vp->stream_role==ROLE_PASSIVE) {
	/* Active role is wanted, but our device is passive, so create
	 * master clocks to run the media flow.
	 */
	need_frame_buf = PJ_TRUE;

	if (vp->dir & PJMEDIA_DIR_ENCODING) {
	    status = pjmedia_clock_create2(pool,
                                           PJMEDIA_PTIME(&vfd->fps),
					   prm->vidparam.clock_rate,
					   PJMEDIA_CLOCK_NO_HIGHEST_PRIO,
					   &enc_clock_cb, vp, &vp->enc_clock);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}

	if (vp->dir & PJMEDIA_DIR_DECODING) {
	    status = pjmedia_clock_create2(pool,
                                           PJMEDIA_PTIME(&vfd->fps),
					   prm->vidparam.clock_rate,
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

	if (vp->stream_role == ROLE_ACTIVE) {
	    need_frame_buf = PJ_TRUE;
	}
    }

    if (need_frame_buf) {
	const pjmedia_video_format_info *vfi;
	pjmedia_video_apply_fmt_param vafp;

	vfi = pjmedia_get_video_format_info(NULL, prm->vidparam.fmt.id);
	if (!vfi) {
	    status = PJ_ENOTFOUND;
	    goto on_error;
	}

	pj_bzero(&vafp, sizeof(vafp));
	vafp.size = vfd->size;
	status = vfi->apply_fmt(vfi, &vafp);
	if (status != PJ_SUCCESS)
	    goto on_error;

	if (vp->dir & PJMEDIA_DIR_ENCODING) {
	    vp->enc_frm_buf = PJ_POOL_ZALLOC_T(pool, pjmedia_frame);
	    vp->enc_frm_buf->buf = pj_pool_alloc(pool, vafp.framebytes);
	    vp->enc_frm_buf->size = vafp.framebytes;
	    vp->enc_frm_buf->type = PJMEDIA_FRAME_TYPE_NONE;

	    status = pj_mutex_create_simple(pool, vp->dev_name.ptr,
					    &vp->enc_frm_mutex);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}

	if (vp->dir & PJMEDIA_DIR_DECODING) {
	    vp->dec_frm_buf = PJ_POOL_ZALLOC_T(pool, pjmedia_frame);
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

PJ_DEF(pjmedia_vid_stream*)
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

    status = pjmedia_vid_stream_start(vp->strm);
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

    status = pjmedia_vid_stream_stop(vp->strm);

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
	pjmedia_vid_stream_destroy(vp->strm);
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

static void enc_clock_cb(const pj_timestamp *ts, void *user_data)
{
    /* We are here because user wants us to be active but the stream is
     * passive. So get a frame from the stream and push it to user.
     */
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    pj_status_t status;

    pj_assert(vp->role==ROLE_ACTIVE && vp->stream_role==ROLE_PASSIVE);

    PJ_UNUSED_ARG(ts);

    if (!vp->client_port)
	return;

    status = pjmedia_vid_stream_get_frame(vp->strm, vp->enc_frm_buf);
    if (status != PJ_SUCCESS)
	return;

    //save_rgb_frame(vp->cap_size.w, vp->cap_size.h, vp->enc_frm_buf);

    status = pjmedia_port_put_frame(vp->client_port, vp->enc_frm_buf);
}

static void dec_clock_cb(const pj_timestamp *ts, void *user_data)
{
    /* We are here because user wants us to be active but the stream is
     * passive. So get a frame from the stream and push it to user.
     */
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;
    pj_status_t status;

    pj_assert(vp->role==ROLE_ACTIVE && vp->stream_role==ROLE_PASSIVE);

    PJ_UNUSED_ARG(ts);

    if (!vp->client_port)
	return;

    status = pjmedia_port_get_frame(vp->client_port, vp->dec_frm_buf);
    if (status != PJ_SUCCESS)
	return;

    status = pjmedia_vid_stream_put_frame(vp->strm, vp->dec_frm_buf);
}

static void copy_frame(pjmedia_frame *dst, const pjmedia_frame *src)
{
    PJ_ASSERT_ON_FAIL(dst->size >= src->size, return);

    pj_memcpy(dst, src, sizeof(*src));
    pj_memcpy(dst->buf, src->buf, src->size);
    dst->size = src->size;
}

static pj_status_t vidstream_cap_cb(pjmedia_vid_stream *stream,
				    void *user_data,
				    pjmedia_frame *frame)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;

    if (vp->role==ROLE_ACTIVE) {
	if (vp->client_port)
	    return pjmedia_port_put_frame(vp->client_port, frame);
    } else {
	pj_mutex_lock(vp->enc_frm_mutex);
	copy_frame(vp->enc_frm_buf, frame);
	pj_mutex_unlock(vp->enc_frm_mutex);
    }
    if (vp->strm_cb.capture_cb)
        return (*vp->strm_cb.capture_cb)(stream, vp->strm_cb_data, frame);
    return PJ_SUCCESS;
}

static pj_status_t vidstream_render_cb(pjmedia_vid_stream *stream,
				       void *user_data,
				       pjmedia_frame *frame)
{
    pjmedia_vid_port *vp = (pjmedia_vid_port*)user_data;

    if (vp->role==ROLE_ACTIVE) {
	if (vp->client_port)
	    return pjmedia_port_get_frame(vp->client_port, frame);
    } else {
	pj_mutex_lock(vp->dec_frm_mutex);
	copy_frame(frame, vp->dec_frm_buf);
	pj_mutex_unlock(vp->dec_frm_mutex);
    }
    if (vp->strm_cb.render_cb)
        return (*vp->strm_cb.render_cb)(stream, vp->strm_cb_data, frame);
    return PJ_SUCCESS;
}

static pj_status_t vidstream_event_cb(pjmedia_vid_stream *stream,
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

    if (vp->stream_role==ROLE_PASSIVE) {
	return pjmedia_vid_stream_put_frame(vp->strm, frame);
    } else {
	pj_mutex_lock(vp->dec_frm_mutex);
	copy_frame(vp->dec_frm_buf, frame);
	pj_mutex_unlock(vp->dec_frm_mutex);
    }

    return PJ_SUCCESS;
}

static pj_status_t vid_pasv_port_get_frame(struct pjmedia_port *this_port,
					   pjmedia_frame *frame)
{
    struct vid_pasv_port *vpp = (struct vid_pasv_port*)this_port;
    pjmedia_vid_port *vp = vpp->vp;

    if (vp->stream_role==ROLE_PASSIVE) {
	return pjmedia_vid_stream_get_frame(vp->strm, frame);
    } else {
	pj_mutex_lock(vp->enc_frm_mutex);
	copy_frame(frame, vp->enc_frm_buf);
	pj_mutex_unlock(vp->enc_frm_mutex);
    }

    return PJ_SUCCESS;
}


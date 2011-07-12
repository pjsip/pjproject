/* $Id$ */
/* 
 * Copyright (C) 2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/vid_tee.h>
#include <pjmedia/errno.h>
#include <pj/array.h>
#include <pj/pool.h>

#define TEE_PORT_NAME	"vid_tee"
#define TEE_PORT_SIGN	PJMEDIA_PORT_SIGNATURE('V', 'T', 'E', 'E')


typedef struct vid_tee_dst_port
{
    pjmedia_port	*dst;
    unsigned		 option;
} vid_tee_dst_port;


typedef struct vid_tee_port
{
    pjmedia_port	 base;
    void		*buf;
    pj_size_t		 buf_size;
    unsigned		 dst_port_maxcnt;
    unsigned		 dst_port_cnt;
    vid_tee_dst_port	*dst_ports;
} vid_tee_port;


static pj_status_t tee_put_frame(pjmedia_port *port, pjmedia_frame *frame);
static pj_status_t tee_get_frame(pjmedia_port *port, pjmedia_frame *frame);
static pj_status_t tee_destroy(pjmedia_port *port);

/*
 * Create a video tee port with the specified source media port.
 */
PJ_DEF(pj_status_t) pjmedia_vid_tee_create( pj_pool_t *pool,
					    const pjmedia_format *fmt,
					    unsigned max_dst_cnt,
					    pjmedia_port **p_vid_tee)
{
    vid_tee_port *tee;
    pj_str_t name_st;
    const pjmedia_video_format_info *vfi;
    pjmedia_video_apply_fmt_param vafp;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && fmt && p_vid_tee, PJ_EINVAL);
    PJ_ASSERT_RETURN(fmt->type == PJMEDIA_TYPE_VIDEO, PJ_EINVAL);

    /* Allocate video tee structure */
    tee = PJ_POOL_ZALLOC_T(pool, vid_tee_port);

    /* Initialize video tee structure */
    tee->dst_port_maxcnt = max_dst_cnt;
    tee->dst_ports = (vid_tee_dst_port*)
                     pj_pool_calloc(pool, max_dst_cnt,
                                    sizeof(vid_tee_dst_port));

    /* Initialize video tee buffer, its size is one frame */
    vfi = pjmedia_get_video_format_info(NULL, fmt->id);
    if (vfi == NULL)
	return PJMEDIA_EBADFMT;

    pj_bzero(&vafp, sizeof(vafp));
    vafp.size = fmt->det.vid.size;
    status = vfi->apply_fmt(vfi, &vafp);
    if (status != PJ_SUCCESS)
	return status;

    tee->buf_size = vafp.framebytes;
    tee->buf = pj_pool_zalloc(pool, tee->buf_size);

    /* Initialize video tee port */
    status = pjmedia_port_info_init2(&tee->base.info,
				     pj_strset2(&name_st, (char*)TEE_PORT_NAME),
				     TEE_PORT_SIGN,
				     PJMEDIA_DIR_ENCODING,
				     fmt);
    if (status != PJ_SUCCESS)
	return status;

    tee->base.get_frame = &tee_get_frame;
    tee->base.put_frame = &tee_put_frame;
    tee->base.on_destroy = &tee_destroy;

    /* Done */
    *p_vid_tee = &tee->base;

    return PJ_SUCCESS;
}


/*
 * Add a destination media port to the video tee.
 */
PJ_DEF(pj_status_t) pjmedia_vid_tee_add_dst_port(pjmedia_port *vid_tee,
						 unsigned option,
						 pjmedia_port *port)
{
    vid_tee_port *tee = (vid_tee_port*)vid_tee;
    pjmedia_video_format_detail *vfd;

    PJ_ASSERT_RETURN(vid_tee && vid_tee->info.signature==TEE_PORT_SIGN,
		     PJ_EINVAL);

    if (tee->dst_port_cnt >= tee->dst_port_maxcnt)
	return PJ_ETOOMANY;

    if (vid_tee->info.fmt.id != port->info.fmt.id)
	return PJMEDIA_EBADFMT;

    vfd = pjmedia_format_get_video_format_detail(&port->info.fmt, PJ_TRUE);
    if (vfd->size.w != vid_tee->info.fmt.det.vid.size.w ||
	vfd->size.h != vid_tee->info.fmt.det.vid.size.h)
    {
	return PJMEDIA_EBADFMT;
    }

    tee->dst_ports[tee->dst_port_cnt].dst = port;
    tee->dst_ports[tee->dst_port_cnt].option = option;
    ++tee->dst_port_cnt;

    return PJ_SUCCESS;
}


/*
 * Remove a destination media port from the video tee.
 */
PJ_DECL(pj_status_t) pjmedia_vid_tee_remove_dst_port(pjmedia_port *vid_tee,
						     pjmedia_port *port)
{
    vid_tee_port *tee = (vid_tee_port*)vid_tee;
    unsigned i;

    PJ_ASSERT_RETURN(vid_tee && vid_tee->info.signature==TEE_PORT_SIGN,
		     PJ_EINVAL);

    for (i = 0; i < tee->dst_port_cnt; ++i) {
	if (tee->dst_ports[i].dst == port) {
	    pj_array_erase(tee->dst_ports, sizeof(tee->dst_ports[0]),
			   tee->dst_port_cnt, i);
	    --tee->dst_port_cnt;
	    return PJ_SUCCESS;
	}
    }

    return PJ_ENOTFOUND;
}


static pj_status_t tee_put_frame(pjmedia_port *port, pjmedia_frame *frame)
{
    vid_tee_port *tee = (vid_tee_port*)port;
    unsigned i;

    for (i = 0; i < tee->dst_port_cnt; ++i) {
	pjmedia_frame frame_ = *frame;

	/* For dst_ports that do in-place processing, we need to duplicate
	 * the data source first.
	 */
	if (tee->dst_ports[i].option & PJMEDIA_VID_TEE_DST_DO_IN_PLACE_PROC) {
	    PJ_ASSERT_RETURN(tee->buf_size <= frame->size, PJ_ETOOBIG);
	    frame_.buf = tee->buf;
	    frame_.size = frame->size;
	    pj_memcpy(frame_.buf, frame->buf, frame->size);
	}

	/* Deliver the data */
	pjmedia_port_put_frame(tee->dst_ports[i].dst, &frame_);
    }

    return PJ_SUCCESS;
}

static pj_status_t tee_get_frame(pjmedia_port *port, pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(port);
    PJ_UNUSED_ARG(frame);

    pj_assert(!"Bug! Tee port get_frame() shouldn't be called.");

    return PJ_EBUG;
}

static pj_status_t tee_destroy(pjmedia_port *port)
{
    vid_tee_port *tee = (vid_tee_port*)port;

    PJ_ASSERT_RETURN(port && port->info.signature==TEE_PORT_SIGN, PJ_EINVAL);

    pj_bzero(tee, sizeof(*tee));

    return PJ_SUCCESS;
}

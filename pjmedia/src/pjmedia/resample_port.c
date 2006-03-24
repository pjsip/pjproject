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
#include <pjmedia/resample.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


#define BYTES_PER_SAMPLE	2

struct resample_port
{
    pjmedia_port	 base;
    pjmedia_resample	*resample_get;
    pjmedia_resample	*resample_put;
    pj_int16_t		*get_buf;
    pj_int16_t		*put_buf;
    unsigned		 downstream_frame_size;
    unsigned		 upstream_frame_size;
};



static pj_status_t resample_put_frame(pjmedia_port *this_port,
				      const pjmedia_frame *frame);
static pj_status_t resample_get_frame(pjmedia_port *this_port, 
				      pjmedia_frame *frame);



PJ_DEF(pj_status_t) pjmedia_resample_port_create( pj_pool_t *pool,
						  pj_bool_t high_quality,
						  pj_bool_t large_filter,
						  unsigned downstream_rate,
						  unsigned upstream_rate,
						  unsigned channel_count,
						  unsigned samples_per_frame,
						  pjmedia_port **p_port )
{
    struct resample_port *rport;
    unsigned upstream_samples_per_frame;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && p_port, PJ_EINVAL);

    upstream_samples_per_frame = (unsigned)(samples_per_frame * 1.0 *
					    upstream_rate / downstream_rate);

    /* Create and initialize port. */
    rport = pj_pool_zalloc(pool, sizeof(struct resample_port));
    PJ_ASSERT_RETURN(rport != NULL, PJ_ENOMEM);

    rport->base.info.bits_per_sample = 16;
    rport->base.info.bytes_per_frame = samples_per_frame * BYTES_PER_SAMPLE;
    rport->base.info.channel_count = channel_count;
    rport->base.info.encoding_name = pj_str("pcm");
    rport->base.info.has_info = 1;
    rport->base.info.name = pj_str("resample");
    rport->base.info.need_info = 0;
    rport->base.info.pt = 0xFF;
    rport->base.info.sample_rate = upstream_rate;
    rport->base.info.samples_per_frame = upstream_samples_per_frame;
    rport->base.info.signature = 0;
    rport->base.info.type = PJMEDIA_TYPE_AUDIO;

    rport->downstream_frame_size = samples_per_frame;
    rport->upstream_frame_size = upstream_samples_per_frame;

    /* Create buffers. 
     * We need separate buffer for get_frame() and put_frame() since
     * both functions may run simultaneously.
     */
    rport->get_buf = pj_pool_alloc(pool, samples_per_frame * BYTES_PER_SAMPLE);
    PJ_ASSERT_RETURN(rport->get_buf, PJ_ENOMEM);

    rport->put_buf = pj_pool_alloc(pool, samples_per_frame * BYTES_PER_SAMPLE);
    PJ_ASSERT_RETURN(rport->put_buf, PJ_ENOMEM);


    /* Create "get_frame" resample */
    status = pjmedia_resample_create( pool, high_quality, large_filter,
				      downstream_rate, upstream_rate,
				      samples_per_frame, &rport->resample_get);
    if (status != PJ_SUCCESS)
	return status;

    /* Create "put_frame" resample */
    status = pjmedia_resample_create( pool, high_quality, large_filter,
				      upstream_rate, downstream_rate,
				      upstream_samples_per_frame,
				      &rport->resample_put);

    /* Set get_frame and put_frame interface */
    rport->base.get_frame = &resample_get_frame;
    rport->base.put_frame = &resample_put_frame;


    /* Done */
    *p_port = &rport->base;

    return PJ_SUCCESS;
}



static pj_status_t resample_put_frame(pjmedia_port *this_port,
				      const pjmedia_frame *frame)
{
    struct resample_port *rport = (struct resample_port*) this_port;
    pjmedia_frame downstream_frame;

    /* Return if we don't have downstream port. */
    if (this_port->downstream_port == NULL) {
	return PJ_SUCCESS;
    }

    if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO) {
	pjmedia_resample_run( rport->resample_put, frame->buf, rport->put_buf);

	downstream_frame.buf = rport->put_buf;
	downstream_frame.size = rport->downstream_frame_size * 
				BYTES_PER_SAMPLE;
    } else {
	downstream_frame.buf = frame->buf;
	downstream_frame.size = frame->size;
    }

    downstream_frame.type = frame->type;
    downstream_frame.timestamp.u64 = frame->timestamp.u64;

    return pjmedia_port_put_frame( this_port->downstream_port, 
				   &downstream_frame );
}



static pj_status_t resample_get_frame(pjmedia_port *this_port, 
				      pjmedia_frame *frame)
{
    struct resample_port *rport = (struct resample_port*) this_port;
    pjmedia_frame downstream_frame;
    pj_status_t status;

    /* Return silence if we don't have downstream port */
    if (this_port->downstream_port == NULL) {
	pj_memset(frame->buf, frame->size, 0);
	return PJ_SUCCESS;
    }

    downstream_frame.buf = rport->get_buf;
    downstream_frame.size = rport->downstream_frame_size * BYTES_PER_SAMPLE;
    downstream_frame.timestamp.u64 = frame->timestamp.u64;
    downstream_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;

    status = pjmedia_port_get_frame( this_port->downstream_port,
				     &downstream_frame);
    if (status != PJ_SUCCESS)
	return status;

    pjmedia_resample_run( rport->resample_get, rport->get_buf, frame->buf);

    frame->size = rport->upstream_frame_size * BYTES_PER_SAMPLE;
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;

    return PJ_SUCCESS;
}



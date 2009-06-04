/* $Id$ */
/* 
 * Copyright (C) 2009 Teluu Inc. (http://www.teluu.com)
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

#include <pjmedia/sync_port.h>
#include <pjmedia/clock.h>
#include <pjmedia/delaybuf.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

#define THIS_FILE   "sync_port.c"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('S', 'Y', 'N', 'C')

typedef struct pjmedia_sync_port
{
    pjmedia_port	 base;
    pjmedia_sync_param	 param;
    pjmedia_port	*dn_port;
    pj_int16_t		*framebuf;
    pjmedia_clock	*clock;
    pjmedia_delay_buf	*delay_buf;
    pjmedia_delay_buf	*delay_buf2;

} pjmedia_sync_port;


/*
 * CALLBACKS
 */

/* Clock callback */
static void clock_cb(const pj_timestamp *ts, void *user_data)
{
    pjmedia_sync_port *port = (pjmedia_sync_port*)user_data;
    pjmedia_frame f;
    pj_status_t status;

    /* call dn_port.put_frame() */
    pj_bzero(&f, sizeof(f));
    f.buf = port->framebuf;
    f.size = port->base.info.samples_per_frame << 1;
    f.type = PJMEDIA_TYPE_AUDIO;
    f.timestamp = *ts;

    status = pjmedia_delay_buf_get(port->delay_buf, (pj_int16_t*)f.buf);
    if (status == PJ_SUCCESS) {
	pjmedia_port_put_frame(port->dn_port, &f);
    }

    /* call dn_port.get_frame() */
    pj_bzero(&f, sizeof(f));
    f.buf = port->framebuf;
    f.size = port->base.info.samples_per_frame << 1;
    f.type = PJMEDIA_TYPE_AUDIO;
    f.timestamp = *ts;

    status = pjmedia_port_get_frame(port->dn_port, &f);
    if (status != PJ_SUCCESS || f.type != PJMEDIA_FRAME_TYPE_AUDIO) {
	pjmedia_zero_samples((pj_int16_t*)f.buf, 
			     port->base.info.samples_per_frame);
    }
    pjmedia_delay_buf_put(port->delay_buf2, (pj_int16_t*)f.buf);
}

/* Get frame (from this port) when port is using own clock. */
static pj_status_t get_frame_clock(pjmedia_port *this_port,
				   pjmedia_frame *frame)
{
    pjmedia_sync_port *port = (pjmedia_sync_port*)this_port;
    pj_status_t status;

    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    /* get frame from delay buf */
    status = pjmedia_delay_buf_get(port->delay_buf2, (pj_int16_t*)frame->buf);
    if (status != PJ_SUCCESS) {
	return status;
    }

    frame->size = this_port->info.samples_per_frame << 1;
    frame->type = PJMEDIA_TYPE_AUDIO;

    return PJ_SUCCESS;
}

/* Get frame (from this port). */
static pj_status_t get_frame(pjmedia_port *this_port,
			     pjmedia_frame *frame)
{
    pjmedia_sync_port *port = (pjmedia_sync_port*)this_port;
    pjmedia_frame f;
    pj_status_t status;

    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    /* get frame */
    status = pjmedia_port_get_frame(port->dn_port, frame);
    if (status != PJ_SUCCESS)
	return status;

    /* put frame from delay buf */
    pj_bzero(&f, sizeof(f));
    f.buf = port->framebuf;
    f.size = this_port->info.samples_per_frame << 1;
    f.type = PJMEDIA_TYPE_AUDIO;
    f.timestamp = frame->timestamp;

    status = pjmedia_delay_buf_get(port->delay_buf, (pj_int16_t*)f.buf);
    if (status != PJ_SUCCESS)
	return status;

    status = pjmedia_port_put_frame(port->dn_port, &f);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}

/* Put frame (to this port). */
static pj_status_t put_frame(pjmedia_port *this_port,
			     const pjmedia_frame *frame)
{
    pjmedia_sync_port *port = (pjmedia_sync_port*)this_port;

    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    /* put frame to delay buf */
    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO) {
	pjmedia_zero_samples((pj_int16_t*)frame->buf, 
			     this_port->info.samples_per_frame);
    }

    return pjmedia_delay_buf_put(port->delay_buf, (pj_int16_t*)frame->buf);
}

/* Destroy the port. */
static pj_status_t on_destroy(pjmedia_port *this_port)
{
    pjmedia_sync_port *port = (pjmedia_sync_port*)this_port;

    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    if (port->clock) {
	pjmedia_clock_stop(port->clock);
	pjmedia_clock_destroy(port->clock);
	port->clock = NULL;
    }

    if (port->delay_buf) {
	pjmedia_delay_buf_destroy(port->delay_buf);
	port->delay_buf = NULL;
    }

    if (port->delay_buf2) {
	pjmedia_delay_buf_destroy(port->delay_buf2);
	port->delay_buf2 = NULL;
    }

    if (port->dn_port && 
	(port->param.options & PJMEDIA_SYNC_DONT_DESTROY_DN) == 0)
    {
	pjmedia_port_destroy(port->dn_port);
    }

    return PJ_SUCCESS;
}


/*
 * API FUNCTIONS
 */

/* Create the sync port. */
PJ_DEF(pj_status_t) pjmedia_sync_port_create(pj_pool_t *pool,
					     pjmedia_port *dn_port,
					     const pjmedia_sync_param *param,
					     pjmedia_port **p_port )
{
    pjmedia_sync_port *sync;
    pj_str_t name;
    unsigned ptime;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && dn_port && p_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(dn_port->info.bits_per_sample==16, PJ_EINVAL);

    sync = PJ_POOL_ZALLOC_T(pool, pjmedia_sync_port);
    sync->framebuf = (pj_int16_t*)
		     pj_pool_zalloc(pool, dn_port->info.samples_per_frame<<1);

    /* Init port */
    if (param)
	sync->param = *param;
    sync->dn_port = dn_port;

    /* Init port info */
    name = pj_str(pool->obj_name);
    pjmedia_port_info_init(&sync->base.info,
			   &name, SIGNATURE,
			   dn_port->info.clock_rate,
			   dn_port->info.channel_count,
			   dn_port->info.bits_per_sample,
			   dn_port->info.samples_per_frame);

    /* Init port op */
    sync->base.get_frame = &get_frame;
    sync->base.put_frame = &put_frame;
    sync->base.on_destroy = &on_destroy;

    /* Create delay buffer to compensate drifts */
    ptime = dn_port->info.samples_per_frame * 1000 / 
	    dn_port->info.channel_count /
	    dn_port->info.clock_rate;
    status = pjmedia_delay_buf_create(pool, name.ptr, 
				      dn_port->info.clock_rate, 
				      dn_port->info.samples_per_frame,
				      dn_port->info.channel_count,
				      PJMEDIA_SOUND_BUFFER_COUNT * ptime,
				      0, 
				      &sync->delay_buf);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Create clock if specified */
    if (sync->param.options & PJMEDIA_SYNC_USE_EXT_CLOCK) {
	status = pjmedia_delay_buf_create(pool, name.ptr, 
					  dn_port->info.clock_rate, 
					  dn_port->info.samples_per_frame,
					  dn_port->info.channel_count,
					  PJMEDIA_SOUND_BUFFER_COUNT * ptime,
					  0, 
					  &sync->delay_buf2);
	if (status != PJ_SUCCESS)
	    goto on_error;

	status = pjmedia_clock_create(pool,
				      dn_port->info.clock_rate, 
				      dn_port->info.channel_count,
				      dn_port->info.samples_per_frame,
				      0,
				      &clock_cb,
				      sync,
				      &sync->clock);
	if (status != PJ_SUCCESS)
	    goto on_error;

	sync->base.get_frame = &get_frame_clock;
    }

    /* Done */
    *p_port = &sync->base;
    return PJ_SUCCESS;

on_error:
    on_destroy(&sync->base);
    return status;
}

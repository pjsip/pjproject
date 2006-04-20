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
#include <pjmedia/master_port.h>
#include <pjmedia/clock.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


struct pjmedia_master_port
{
    unsigned	     options;
    pjmedia_clock   *clock;
    pjmedia_port    *u_port;
    pjmedia_port    *d_port;
    unsigned	     buff_size;
    void	    *buff;
};


static void clock_callback(const pj_timestamp *ts, void *user_data);


/*
 * Create a master port.
 *
 */
PJ_DEF(pj_status_t) pjmedia_master_port_create( pj_pool_t *pool,
						pjmedia_port *u_port,
						pjmedia_port *d_port,
						unsigned options,
						pjmedia_master_port **p_m)
{
    pjmedia_master_port *m;
    unsigned clock_rate;
    unsigned samples_per_frame;
    unsigned bytes_per_frame;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && u_port && d_port && p_m, PJ_EINVAL);


    /* Both ports MUST have the equal ptime */
    PJ_ASSERT_RETURN(u_port->info.sample_rate/u_port->info.samples_per_frame==
		     d_port->info.sample_rate/d_port->info.samples_per_frame,
		     PJMEDIA_ENCSAMPLESPFRAME);


    /* Get clock_rate and samples_per_frame from one of the port. */
    clock_rate = u_port->info.sample_rate;
    samples_per_frame = u_port->info.samples_per_frame;


    /* Get the bytes_per_frame value, to determine the size of the
     * buffer. We take the larger size of the two ports.
     */
    bytes_per_frame = u_port->info.bytes_per_frame;
    if (d_port->info.bytes_per_frame > bytes_per_frame)
	bytes_per_frame = d_port->info.bytes_per_frame;


    /* Create the master port instance */
    m = pj_pool_zalloc(pool, sizeof(pjmedia_master_port));
    m->options = options;
    m->u_port = u_port;
    m->d_port = d_port;

    
    /* Create buffer */
    m->buff_size = bytes_per_frame;
    m->buff = pj_pool_alloc(pool, m->buff_size);
    if (!m->buff)
	return PJ_ENOMEM;


    /* Create media clock */
    status = pjmedia_clock_create(pool, clock_rate, samples_per_frame, 0,
				  &clock_callback, m, &m->clock);
    if (status != PJ_SUCCESS)
	return status;


    /* Done */
    *p_m = m;

    return PJ_SUCCESS;
}


/*
 * Start the media flow.
 */
PJ_DEF(pj_status_t) pjmedia_master_port_start(pjmedia_master_port *m)
{
    PJ_ASSERT_RETURN(m && m->clock, PJ_EINVAL);
    PJ_ASSERT_RETURN(m->u_port && m->d_port, PJ_EINVALIDOP);

    return pjmedia_clock_start(m->clock);
}



/*
 * Stop the media flow.
 */
PJ_DEF(pj_status_t) pjmedia_master_port_stop(pjmedia_master_port *m)
{
    PJ_ASSERT_RETURN(m && m->clock, PJ_EINVAL);
    
    return pjmedia_clock_stop(m->clock);
}


/*
 * Callback to be called for each clock ticks.
 */
static void clock_callback(const pj_timestamp *ts, void *user_data)
{
    pjmedia_master_port *m = user_data;
    pjmedia_frame frame;
    pj_status_t status;


    /* Get frame from upstream port and pass it to downstream port */
    pj_memset(&frame, 0, sizeof(frame));
    frame.buf = m->buff;
    frame.size = m->buff_size;
    frame.timestamp.u64 = ts->u64;

    status = pjmedia_port_get_frame(m->u_port, &frame);
    if (status != PJ_SUCCESS)
	frame.type = PJMEDIA_FRAME_TYPE_NONE;

    status = pjmedia_port_put_frame(m->d_port, &frame);

    /* Get frame from downstream port and pass it to upstream port */
    pj_memset(&frame, 0, sizeof(frame));
    frame.buf = m->buff;
    frame.size = m->buff_size;
    frame.timestamp.u64 = ts->u64;

    status = pjmedia_port_get_frame(m->d_port, &frame);
    if (status != PJ_SUCCESS)
	frame.type = PJMEDIA_FRAME_TYPE_NONE;

    status = pjmedia_port_put_frame(m->u_port, &frame);
}


/*
 * Destroy the master port, and optionally destroy the u_port and 
 * d_port ports.
 */
PJ_DEF(pj_status_t) pjmedia_master_port_destroy(pjmedia_master_port *m)
{
    PJ_ASSERT_RETURN(m, PJ_EINVAL);

    if (m->clock) {
	pjmedia_clock_destroy(m->clock);
	m->clock = NULL;
    }

    if (m->u_port) {
	pjmedia_port_destroy(m->u_port);
	m->u_port = NULL;
    }

    if (m->d_port) {
	pjmedia_port_destroy(m->d_port);
	m->d_port = NULL;
    }

    return PJ_SUCCESS;
}



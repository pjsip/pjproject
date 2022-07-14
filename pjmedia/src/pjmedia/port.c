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
#include <pjmedia/port.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

#define THIS_FILE	"port.c"


/**
 * This is an auxiliary function to initialize port info for
 * ports which deal with PCM audio.
 */
PJ_DEF(pj_status_t) pjmedia_port_info_init( pjmedia_port_info *info,
					    const pj_str_t *name,
					    unsigned signature,
					    unsigned clock_rate,
					    unsigned channel_count,
					    unsigned bits_per_sample,
					    unsigned samples_per_frame)
{
#define USEC_IN_SEC (pj_uint64_t)1000000
    unsigned frame_time_usec, avg_bps;

    PJ_ASSERT_RETURN(clock_rate && channel_count, PJ_EINVAL);

    pj_bzero(info, sizeof(*info));

    info->signature = signature;
    info->dir = PJMEDIA_DIR_ENCODING_DECODING;
    info->name = *name;

    frame_time_usec = (unsigned)(samples_per_frame * USEC_IN_SEC /
				 channel_count / clock_rate);
    avg_bps = clock_rate * channel_count * bits_per_sample;

    pjmedia_format_init_audio(&info->fmt, PJMEDIA_FORMAT_L16, clock_rate,
			      channel_count, bits_per_sample, frame_time_usec,
			      avg_bps, avg_bps);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_port_info_init2( pjmedia_port_info *info,
					     const pj_str_t *name,
					     unsigned signature,
					     pjmedia_dir dir,
					     const pjmedia_format *fmt)
{
    pj_bzero(info, sizeof(*info));
    info->signature = signature;
    info->dir = dir;
    info->name = *name;

    pjmedia_format_copy(&info->fmt, fmt);

    return PJ_SUCCESS;
}

/**
 * Get a clock source from the port.
 */
PJ_DEF(pjmedia_clock_src *) pjmedia_port_get_clock_src( pjmedia_port *port,
                                                        pjmedia_dir dir )
{
    if (port && port->get_clock_src)
	return port->get_clock_src(port, dir);
    else
	return NULL;
}

/**
 * Get a frame from the port (and subsequent downstream ports).
 */
PJ_DEF(pj_status_t) pjmedia_port_get_frame( pjmedia_port *port,
					    pjmedia_frame *frame )
{
    PJ_ASSERT_RETURN(port && frame, PJ_EINVAL);

    if (port->get_frame)
	return port->get_frame(port, frame);
    else {
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_EINVALIDOP;
    }
}


/**
 * Put a frame to the port (and subsequent downstream ports).
 */
PJ_DEF(pj_status_t) pjmedia_port_put_frame( pjmedia_port *port,
					    pjmedia_frame *frame )
{
    PJ_ASSERT_RETURN(port && frame, PJ_EINVAL);

    if (port->put_frame)
	return port->put_frame(port, frame);
    else
	return PJ_EINVALIDOP;
}

/**
 * Destroy port (and subsequent downstream ports)
 */
PJ_DEF(pj_status_t) pjmedia_port_destroy( pjmedia_port *port )
{
    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    if (port->grp_lock) {
	pjmedia_port_dec_ref(port);
	return PJ_SUCCESS;
    }

    if (port->on_destroy) {
	return port->on_destroy(port);
    }

    return PJ_SUCCESS;
}


/* Group lock handler */
static void port_on_destroy(void *arg)
{
    pjmedia_port *port = (pjmedia_port*)arg;
    if (port->on_destroy)
	port->on_destroy(port);
}


/**
 * Create and init group lock.
 */
PJ_DEF(pj_status_t) pjmedia_port_init_grp_lock( pjmedia_port *port,
						pj_pool_t *pool,
						pj_grp_lock_t *glock )
{
    pj_grp_lock_t *grp_lock = glock;
    pj_status_t status;

    PJ_ASSERT_RETURN(port && pool, PJ_EINVAL);
    PJ_ASSERT_RETURN(port->grp_lock == NULL, PJ_EEXISTS);

    /* We need to be caution on ports that do not have the on_destroy()!
     * It is either uninitialized yet or the port does not have one.
     * If the port doesn't have one, we'd expect a possible premature destroy!
     */
    if (port->on_destroy == NULL) {
	PJ_LOG(3,(THIS_FILE, "Media port %s is using group lock but does not "
			     "implement on_destroy()!",
			     port->info.name.ptr));
	pj_assert(!"Port using group lock should implement on_destroy()!");
	return PJ_EINVALIDOP;
    }

    if (!grp_lock) {
	/* Create if not supplied */
	status = pj_grp_lock_create_w_handler(pool, NULL, port,
					      &port_on_destroy,
					      &grp_lock);
    } else {
	/* Just add handler, and use internal group lock pool */
	status = pj_grp_lock_add_handler(grp_lock, NULL, port,
					 &port_on_destroy);
    }

    if (status == PJ_SUCCESS) {
	status = pj_grp_lock_add_ref(grp_lock);
    }

    if (status == PJ_SUCCESS) {
	port->grp_lock = grp_lock;
    } else if (grp_lock && !glock) {
	/* Something wrong, destroy group lock if it is created here */
	pj_grp_lock_destroy(grp_lock);
    }

    return status;
}


/**
 * Increase ref counter of the group lock.
 */
PJ_DEF(pj_status_t) pjmedia_port_add_ref( pjmedia_port *port )
{
    PJ_ASSERT_RETURN(port, PJ_EINVAL);
    PJ_ASSERT_RETURN(port->grp_lock, PJ_EINVALIDOP);

    return pj_grp_lock_add_ref(port->grp_lock);
}


/**
 * Decrease ref counter of the group lock.
 */
PJ_DEF(pj_status_t) pjmedia_port_dec_ref( pjmedia_port *port )
{
    PJ_ASSERT_RETURN(port, PJ_EINVAL);
    PJ_ASSERT_RETURN(port->grp_lock, PJ_EINVALIDOP);

    return pj_grp_lock_dec_ref(port->grp_lock);
}

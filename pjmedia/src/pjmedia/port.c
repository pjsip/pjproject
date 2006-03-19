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
#include <pjmedia/port.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>

#define THIS_FILE	"port.c"


/**
 * Connect two ports.
 */
PJ_DEF(pj_status_t) pjmedia_port_connect( pj_pool_t *pool,
					  pjmedia_port *upstream_port,
					  pjmedia_port *downstream_port)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && upstream_port && downstream_port, PJ_EINVAL);

    /* They both MUST have the same media type. */
    PJ_ASSERT_RETURN(upstream_port->info.type ==
		     downstream_port->info.type, PJMEDIA_ENCTYPE);

    /* They both MUST have the same clock rate. */
    PJ_ASSERT_RETURN(upstream_port->info.sample_rate ==
		     downstream_port->info.sample_rate, PJMEDIA_ENCCLOCKRATE);

    /* They both MUST have the same samples per frame */
    PJ_ASSERT_RETURN(upstream_port->info.samples_per_frame ==
		     downstream_port->info.samples_per_frame, 
		     PJMEDIA_ENCSAMPLESPFRAME);

    /* They both MUST have the same bits per sample */
    PJ_ASSERT_RETURN(upstream_port->info.bits_per_sample ==
		     downstream_port->info.bits_per_sample, 
		     PJMEDIA_ENCBITS);

    /* They both MUST have the same bytes per frame */
    PJ_ASSERT_RETURN(upstream_port->info.bytes_per_frame ==
		     downstream_port->info.bytes_per_frame, 
		     PJMEDIA_ENCBYTES);

    /* Create mutual attachment. */
    status = upstream_port->on_downstream_connect( pool, upstream_port,
						   downstream_port );
    if (status != PJ_SUCCESS)
	return status;

    status = downstream_port->on_upstream_connect( pool, downstream_port,
						   upstream_port );
    if (status != PJ_SUCCESS)
	return status;

    /* Save the attachment. */
    upstream_port->downstream_port = downstream_port;
    downstream_port->upstream_port = upstream_port;
    
    /* Done. */
    return PJ_SUCCESS;
}


/**
 * Disconnect ports.
 */
PJ_DEF(pj_status_t) pjmedia_port_disconnect( pjmedia_port *upstream_port,
					     pjmedia_port *downstream_port)
{
    PJ_ASSERT_RETURN(upstream_port && downstream_port, PJ_EINVAL);

    if (upstream_port->downstream_port == downstream_port)
	upstream_port->downstream_port = NULL;

    if (downstream_port->upstream_port == upstream_port)
	downstream_port->upstream_port = NULL;

    return PJ_SUCCESS;
}


/**
 * Get a frame from the port (and subsequent downstream ports).
 */
PJ_DEF(pj_status_t) pjmedia_port_get_frame( pjmedia_port *port,
					    pjmedia_frame *frame )
{
    PJ_ASSERT_RETURN(port && frame, PJ_EINVAL);
    PJ_ASSERT_RETURN(port->get_frame, PJ_EINVALIDOP);

    return port->get_frame(port, frame);
}



/**
 * Put a frame to the port (and subsequent downstream ports).
 */
PJ_DEF(pj_status_t) pjmedia_port_put_frame( pjmedia_port *port,
					    const pjmedia_frame *frame )
{
    PJ_ASSERT_RETURN(port && frame, PJ_EINVAL);
    PJ_ASSERT_RETURN(port->put_frame, PJ_EINVALIDOP);

    return port->put_frame(port, frame);

}


/**
 * Destroy port (and subsequent downstream ports)
 */
PJ_DEF(pj_status_t) pjmedia_port_destroy( pjmedia_port *port )
{
    pj_status_t status;

    PJ_ASSERT_RETURN(port, PJ_EINVAL);

    /* Recursively call this function again to destroy downstream
     * port first.
     */
    if (port->downstream_port) {
	status = pjmedia_port_destroy(port->downstream_port);
	if (status != PJ_SUCCESS)
	    return status;
	pjmedia_port_disconnect(port, port->downstream_port);
    }

    if (port->on_destroy)
	status = port->on_destroy(port);
    else
	status = PJ_SUCCESS;

    return status;
}





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

    if (port->on_destroy)
	status = port->on_destroy(port);
    else
	status = PJ_SUCCESS;

    return status;
}




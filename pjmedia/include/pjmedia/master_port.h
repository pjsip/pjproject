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
#ifndef __PJMEDIA_MASTER_PORT_H__
#define __PJMEDIA_MASTER_PORT_H__


/**
 * @file master_port.h
 * @brief Master port.
 */
#include <pjmedia/port.h>


PJ_BEGIN_DECL


/**
 * Opaque declaration for master port.
 * A master port has two media ports connected to it, i.e. downstream and
 * upstream ports. The media stream flowing to the downstream port is called
 * encoding or send direction, and media stream flowing to the upstream port
 * is called decoding or receive direction.
 *
 * A master port has a "clock" that periodically passes the media frame from 
 * downstream to upstream ports, and vice versa. In each run, it retrieves 
 * media frame from one side with #pjmedia_port_get_frame(), and passes the 
 * media frame to the other side with #pjmedia_port_put_frame(). In each run,
 * this process is done for twice, i.e. one for each direction.
 */
typedef struct pjmedia_master_port pjmedia_master_port;


/**
 * Create a master port.
 *
 * @param pool		Pool to allocate master port from.
 * @param u_port	Upstream port.
 * @param d_port	Downstream port.
 * @param options	Options flags, bitmask from #pjmedia_master_port_flag.
 * @param p_m		Pointer to receive the master port instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_master_port_create(pj_pool_t *pool,
						pjmedia_port *u_port,
						pjmedia_port *d_port,
						unsigned options,
						pjmedia_master_port **p_m);


/**
 * Start the media flow.
 *
 * @param m		The master port.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_master_port_start(pjmedia_master_port *m);


/**
 * Stop the media flow.
 *
 * @param m		The master port.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_master_port_stop(pjmedia_master_port *m);


/**
 * Change the upstream port. Note that application is responsible to destroy
 * current upstream port (the one that is going to be replaced with the
 * new port).
 *
 * @param m		The master port.
 * @param port		Port to be used for upstream port.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_master_port_set_uport(pjmedia_master_port *m,
						   pjmedia_port *port);


/**
 * Get the upstream port.
 *
 * @param m		The master port.
 *
 * @return		The upstream port.
 */
PJ_DECL(pjmedia_port*) pjmedia_master_port_get_uport(pjmedia_master_port*m);


/**
 * Change the downstream port. Note that application is responsible to destroy
 * current downstream port (the one that is going to be replaced with the
 * new port).
 *
 * @param m		The master port.
 * @param port		Port to be used for downstream port.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_master_port_set_dport(pjmedia_master_port *m,
						   pjmedia_port *port);


/**
 * Get the downstream port.
 *
 * @param m		The master port.
 *
 * @return		The downstream port.
 */
PJ_DECL(pjmedia_port*) pjmedia_master_port_get_dport(pjmedia_master_port*m);


/**
 * Destroy the master port, and optionally destroy the upstream and 
 * downstream ports.
 *
 * @param m		The master port.
 * @param destroy_ports	If non-zero, the function will destroy both
 *			upstream and downstream ports too.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_master_port_destroy(pjmedia_master_port *m,
						 pj_bool_t destroy_ports);



PJ_END_DECL


#endif	/* __PJMEDIA_MASTER_PORT_H__ */


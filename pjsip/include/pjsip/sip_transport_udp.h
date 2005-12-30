/* $Id: $ */
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
#ifndef __PJSIP_TRANSPORT_UDP_H__
#define __PJSIP_TRANSPORT_UDP_H__

#include <pjsip/sip_transport.h>

PJ_BEGIN_DECL

/**
 * Start UDP transport.
 *
 * @param endpt		The SIP endpoint.
 * @param local		Local address to bind.
 * @param a_name	Published address (only the host and port portion is 
 *			used). If this argument is NULL, then the bound address
 *			will be used as the published address.
 * @param async_cnt	Number of simultaneous async operations.
 * @param p_transport	Pointer to receive the transport.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_udp_transport_start(pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       const pjsip_host_port *a_name,
					       unsigned async_cnt,
					       pjsip_transport **p_transport);

/**
 * Attach UDP socket as a new transport and start the transport.
 *
 * @param endpt		The SIP endpoint.
 * @param sock		UDP socket to use.
 * @param a_name	Published address (only the host and port portion is 
 *			used).
 * @param async_cnt	Number of simultaneous async operations.
 * @param p_transport	Pointer to receive the transport.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_udp_transport_attach(pjsip_endpoint *endpt,
						pj_sock_t sock,
						const pjsip_host_port *a_name,
						unsigned async_cnt,
						pjsip_transport **p_transport);


PJ_END_DECL


#endif	/* __PJSIP_TRANSPORT_UDP_H__ */

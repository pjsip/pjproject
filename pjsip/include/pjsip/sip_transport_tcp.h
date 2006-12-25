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
#ifndef __PJSIP_TRANSPORT_TCP_H__
#define __PJSIP_TRANSPORT_TCP_H__

/**
 * @file sip_transport_tcp.h
 * @brief SIP TCP Transport.
 */

#include <pjsip/sip_transport.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSPORT_TCP TCP Transport
 * @ingroup PJSIP_TRANSPORT
 * @brief API to create and register TCP transport.
 * @{
 * The functions below are used to create TCP transport and register 
 * the transport to the framework.
 */

/**
 * Register support for SIP TCP transport by creating TCP listener on
 * the specified address and port. This function will create an
 * instance of SIP TCP transport factory and register it to the
 * transport manager.
 *
 * @param endpt		The SIP endpoint.
 * @param local		Optional local address to bind, or specify the
 *			address to bind the server socket to. Both IP 
 *			interface address and port fields are optional.
 *			If IP interface address is not specified, socket
 *			will be bound to PJ_INADDR_ANY. If port is not
 *			specified, socket will be bound to any port
 *			selected by the operating system.
 * @param async_cnt	Number of simultaneous asynchronous accept()
 *			operations to be supported. It is recommended that
 *			the number here corresponds to the number of
 *			processors in the system (or the number of SIP
 *			worker threads).
 * @param p_factory	Optional pointer to receive the instance of the
 *			SIP TCP transport factory just created.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tcp_transport_start(pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       unsigned async_cnt,
					       pjsip_tpfactory **p_factory);



/**
 * A newer variant of #pjsip_tcp_transport_start(), which allows specifying
 * the published/public address of the TCP transport.
 *
 * @param endpt		The SIP endpoint.
 * @param local		Optional local address to bind, or specify the
 *			address to bind the server socket to. Both IP 
 *			interface address and port fields are optional.
 *			If IP interface address is not specified, socket
 *			will be bound to PJ_INADDR_ANY. If port is not
 *			specified, socket will be bound to any port
 *			selected by the operating system.
 * @param a_name	Optional published address, which is the address to be
 *			advertised as the address of this SIP transport. 
 *			If this argument is NULL, then the bound address
 *			will be used as the published address.
 * @param async_cnt	Number of simultaneous asynchronous accept()
 *			operations to be supported. It is recommended that
 *			the number here corresponds to the number of
 *			processors in the system (or the number of SIP
 *			worker threads).
 * @param p_factory	Optional pointer to receive the instance of the
 *			SIP TCP transport factory just created.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tcp_transport_start2(pjsip_endpoint *endpt,
					        const pj_sockaddr_in *local,
					        const pjsip_host_port *a_name,
					        unsigned async_cnt,
					        pjsip_tpfactory **p_factory);



PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSIP_TRANSPORT_TCP_H__ */

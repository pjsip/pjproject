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
 * Create, register, and start TCP transport.
 *
 * @param endpt		The SIP endpoint.
 * @param local		Local address to bind.
 * @param async_cnt	Number of simultaneous async operations.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tcp_transport_start(pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       unsigned async_cnt);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSIP_TRANSPORT_TCP_H__ */

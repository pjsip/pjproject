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
#ifndef __PJMEDIA_TRANSPORT_UDP_H__
#define __PJMEDIA_TRANSPORT_UDP_H__


/**
 * @file stream_transport_udp.h
 * @brief Stream transport with UDP.
 */

#include <pjmedia/stream.h>


/**
 * Create UDP stream transport.
 */
PJ_DECL(pj_status_t) pjmedia_transport_udp_create(pjmedia_endpt *endpt,
						  const char *name,
						  int port,
						  pjmedia_transport **p_tp);


/**
 * Create UDP stream transport from existing socket info.
 */
PJ_DECL(pj_status_t) pjmedia_transport_udp_attach(pjmedia_endpt *endpt,
						  const char *name,
						  const pjmedia_sock_info *si,
						  pjmedia_transport **p_tp);


/**
 * Close UDP transport.
 */
PJ_DECL(pj_status_t) pjmedia_transport_udp_close(pjmedia_transport *tp);




#endif	/* __PJMEDIA_TRANSPORT_UDP_H__ */



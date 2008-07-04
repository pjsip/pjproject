/* $Id:$ */
/* 
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
#ifndef __PJMEDIA_TRANSPORT_LOOP_H__
#define __PJMEDIA_TRANSPORT_LOOP_H__


/**
 * @file transport_loop.h
 * @brief Loopback transport
 */

#include <pjmedia/stream.h>


/**
 * @defgroup PJMEDIA_TRANSPORT_LOOP Loopback Media Transport
 * @ingroup PJMEDIA_TRANSPORT
 * @brief Loopback transport for testing.
 * @{
 *
 * This is the loopback media transport, where packet sent to this transport
 * will immediately be sent back to the callback.
 */

PJ_BEGIN_DECL


/**
 * Create the loopback transport.
 *
 * @param endpt	    The media endpoint instance.
 * @param p_tp	    Pointer to receive the transport instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_loop_create(pjmedia_endpt *endpt,
						   pjmedia_transport **p_tp);




PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_LOOP_H__ */



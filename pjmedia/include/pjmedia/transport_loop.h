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
 * This is the loopback media transport, where packets sent to this transport
 * will be sent back to the streams attached to this transport. Unlike the
 * other PJMEDIA transports, the loop transport may be attached to multiple
 * streams (in other words, application should specify the same loop transport
 * instance when calling #pjmedia_stream_create()). Any RTP or RTCP packets
 * sent by one stream to this transport by default will be sent back to all 
 * streams that are attached to this transport, including to the stream that
 * sends the packet. Application may individually select which stream to
 * receive packets by calling #pjmedia_transport_loop_disable_rx().
 */

PJ_BEGIN_DECL


/**
 * Settings to be given when creating loopback media transport. Application
 * should call #pjmedia_loop_tp_setting_default() to initialize this
 * structure with its default values.
 */
typedef struct pjmedia_loop_tp_setting
{
    /* Address family, which can be pj_AF_INET() for IPv4 or
     * pj_AF_INET6() for IPv6. Default is IPv4 (pj_AF_INET()).
     */
    int 	af;

    /* Optional local address which will be returned in the transport info.
     * If the string is empty, the address will be the default loopback
     * address (127.0.0.1 or ::1).
     *
     * Note that the address is used for info purpose only and no actual
     * resource will be allocated.
     *
     * Default is empty string.
     */
    pj_str_t 	addr;

    /* The port number for the RTP socket. The RTCP port number will be
     * set to one above RTP port. If zero, it will use the default port
     * number (4000).
     *
     * Note that no actual port will be allocated. Default is 4000.
     */
    int 	port;
    
    /* Setting whether attached streams will receive incoming packets.
     * Application can further customize the setting of a particular setting
     * using the API pjmedia_transport_loop_disable_rx().
     *
     * Default: PJ_FALSE;
     */
    pj_bool_t 	disable_rx;

} pjmedia_loop_tp_setting;


/**
 * Initialize loopback media transport setting with its default values.
 *
 * @param opt	SRTP setting to be initialized.
 */
PJ_DECL(void) pjmedia_loop_tp_setting_default(pjmedia_loop_tp_setting *opt);


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


/**
 * Create the loopback transport.
 *
 * @param endpt	    The media endpoint instance.
 * @param opt       Optional settings. If NULL is given, default
 *		    settings will be used.
 * @param p_tp	    Pointer to receive the transport instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_transport_loop_create2(pjmedia_endpt *endpt,
			       const pjmedia_loop_tp_setting *opt,
			       pjmedia_transport **p_tp);


/**
 * Set the configuration of whether a stream will become the receiver of
 * incoming packets.
 *
 * @param tp	    The transport.
 * @param user	    The stream.
 * @param disabled  PJ_TRUE to disable the receiving of packets, or
 *		    PJ_FALSE to enable it.
 */
PJ_DECL(pj_status_t) pjmedia_transport_loop_disable_rx(pjmedia_transport *tp,
						       void *user,
						       pj_bool_t disabled);


PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_LOOP_H__ */



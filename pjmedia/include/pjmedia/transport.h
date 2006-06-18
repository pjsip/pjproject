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
#ifndef __PJMEDIA_TRANSPORT_H__
#define __PJMEDIA_TRANSPORT_H__


/**
 * @file transport.h Media Transport Interface
 * @brief Transport interface.
 */

#include <pjmedia/types.h>

/**
 * @defgroup PJMEDIA_TRANSPORT Transports
 * @ingroup PJMEDIA
 * @brief Transports.
 * Transport related components.
 */

/**
 * @defgroup PJMEDIA_TRANSPORT_H Network Transport Interface
 * @ingroup PJMEDIA_TRANSPORT
 * @brief PJMEDIA object for sending/receiving media packets over the network
 * @{
 * The media transport (#pjmedia_transport) is the object to send and
 * receive media packets over the network. Currently only media @ref PJMED_STRM
 * are using the transport.
 *
 * Although currently only @ref PJMEDIA_TRANSPORT_UDP is implemented,
 * media transport interface is intended to support any custom transports.
 */

PJ_BEGIN_DECL


/*
 * Forward declaration for media transport.
 */
typedef struct pjmedia_transport pjmedia_transport;


/**
 * This structure describes the operations for the stream transport.
 */
struct pjmedia_transport_op
{
    /**
     * This function is called by the stream when the transport is about
     * to be used by the stream for the first time, and it tells the transport
     * about remote RTP address to send the packet and some callbacks to be 
     * called for incoming packets.
     */
    pj_status_t (*attach)(pjmedia_transport *tp,
			  pjmedia_stream *strm,
			  const pj_sockaddr_t *rem_addr,
			  unsigned addr_len,
			  void (*rtp_cb)(pjmedia_stream*,
					 const void*,
					 pj_ssize_t),
			  void (*rtcp_cb)(pjmedia_stream*,
					  const void*,
					  pj_ssize_t));

    /**
     * This function is called by the stream when the stream is no longer
     * need the transport (normally when the stream is about to be closed).
     */
    void (*detach)(pjmedia_transport *tp,
		   pjmedia_stream *strm);

    /**
     * This function is called by the stream to send RTP packet using the 
     * transport.
     */
    pj_status_t (*send_rtp)(pjmedia_transport *tp,
			    const void *pkt,
			    pj_size_t size);

    /**
     * This function is called by the stream to send RTCP packet using the
     * transport.
     */
    pj_status_t (*send_rtcp)(pjmedia_transport *tp,
			     const void *pkt,
			     pj_size_t size);

    /**
     * This function can be called to destroy this transport.
     */
    pj_status_t (*destroy)(pjmedia_transport *tp);
};


/**
 * @see pjmedia_transport_op.
 */
typedef struct pjmedia_transport_op pjmedia_transport_op;


/**
 * This structure declares stream transport. A stream transport is called
 * by the stream to transmit a packet, and will notify stream when
 * incoming packet is arrived.
 */
struct pjmedia_transport
{
    /** Transport name (for logging purpose). */
    char		  name[PJ_MAX_OBJ_NAME];

    /** Transport's "virtual" function table. */
    pjmedia_transport_op *op;
};


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_H__ */


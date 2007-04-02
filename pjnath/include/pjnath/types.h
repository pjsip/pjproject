/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __PJNATH_TYPES_H__
#define __PJNATH_TYPES_H__

/**
 * @file types.h
 * @brief PJNATH types.
 */

#include <pj/types.h>
#include <pjnath/config.h>

/**
 * @defgroup PJNATH NAT Traversal Helper Library
 * @{
 */

PJ_BEGIN_DECL

/**
 * Initialize pjnath library.
 *
 * @return	    Initialization status.
 */
PJ_DECL(pj_status_t) pjnath_init(void);


/**
 * Display error to the log.
 *
 * @param sender    The sender name.
 * @param title	    Title message.
 * @param status    The error status.
 */
#if PJNATH_ERROR_LEVEL <= PJ_LOG_MAX_LEVEL
PJ_DECL(void) pjnath_perror(const char *sender, const char *title,
			    pj_status_t status);
#else
# define pjnath_perror(sender, title, status)
#endif



PJ_END_DECL

/**
 * @}
 */

/* Doxygen documentation below: */

/**
 * @mainpage PJNATH - Open Source ICE, STUN, and TURN Library
 *
 * \n
 * This is the documentation of PJNATH, an Open Source library providing
 * NAT traversal helper functionalities by using standard based protocols.
 *
 * \n

 * \section PJNATH_STUN STUN Protocol Library
 *
 * Session Traversal Utilities (STUN, or previously known as Simple 
 * Traversal of User Datagram Protocol (UDP) Through Network Address 
 * Translators (NAT)s), is a lightweight protocol that serves as a tool for
 * application protocols in dealing with NAT traversal. It allows a client
 * to determine the IP address and port allocated to them by a NAT and to 
 * keep NAT bindings open.
 * 
 * The PJNATH library provides facilities to support both the core 
 * <B>STUN-bis</B> specification and the <B>TURN</B> usage of STUN, 
 * as well as other STUN usages. Please see #pj_stun_attr_type for 
 * list of STUN attributes supported by this library.
 *
 * 
 * The following are some design principles that have been utilized
 * when implementing the STUN library in PJNATH:
 *
 *  - layered architecture, with \ref PJNATH_STUN_MSG as the lowest
 *    layer and \ref PJNATH_STUN_SESSION as the highest abstraction
 *    layer, to accommodate various usage scenario of the library.
 *
 *  - no transport -- the STUN library is pretty much transport
 *    independent and all sending and receiving functionalities will
 *    have to be implemented by application or higher level
 *    abstraction (such as ICE). This helps facilitating an even
 *    more usage scenarios of the library.
 *
 *  - common functionalities for both STUN client and server
 *    development. All STUN components can be used to develop both
 *    STUN client and STUN server application, and in fact, in ICE,
 *    both STUN client and server functionality exist in a single
 *    ICE session.
 *
 * \n
 *
 * \subsection PJNATH_STUN_ARCH STUN Library Organization
 *
 * \image html stun-arch.jpg "STUN Library Architecture"
 *
 * The STUN library is organized as follows:
 *
 *  - the lowest layer of the library is \ref PJNATH_STUN_MSG. This layer
 *    provides STUN message representation, validation, parsing, and
 *    debugging (dump to log) of STUN messages.
 *
 *  - for client, the next higher layer is \ref PJNATH_STUN_TRANSACTION,
 *    which manages retransmissions of STUN request.
 *
 *  - \ref PJNATH_STUN_AUTH provides mechanism to verify STUN
 *    credential in incoming STUN messages.
 *
 *  - for both client and server, the next higher abstraction is
 *    \ref PJNATH_STUN_SESSION, which provides management of incoming
 *    and outgoing messages and association of STUN credential to
 *    a STUN session.
 *
 * As mentioned previously, all STUN library components are independent
 * of any transports. Application gives incoming packet
 * to the STUN components for processing. and it must supply the STUN 
 * components with callback to send outgoing messages.
 * 
 *
 * \subsection PJNATH_STUN_REF STUN Reference
 *
 * References for STUN:
 *
 *  - <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-behave-rfc3489bis-06.txt">
 *    <B>draft-ietf-behave-rfc3489bis-06</b></A>: Session Traversal 
 *     Utilities for (NAT) (STUN),
 *  - <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-behave-turn-03.txt">
 *    <B>draft-ietf-behave-turn-03</B></A>: Obtaining Relay Addresses 
 *    from Simple Traversal Underneath NAT (STUN)
 *  - Obsoleted: <A HREF="http://www.ietf.org/rfc/rfc3489.txt">RFC 3489</A>.
 *
 * \n
 *
 * \section PJNATH_ICE ICE Implementation
 *
 * Interactive Connectivity Establishment (ICE) is a standard based 
 * methodology for traversing Network Address Translator (NAT), and
 * is described in 
 * <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-mmusic-ice-14.txt">
 * <B>draft-ietf-mmusic-ice-14.txt</B></A> draft. The PJNATH ICE
 * implementation is aimed to provide a usable and generic ICE transports
 * for different types of application, including but not limited to
 * the usage of ICE in SIP/SDP offer/answer.
 * 
 *
 * \subsection PJNATH_ICE_ARCH ICE Library Organization
 * 
 * \image html ice-arch.jpg "ICE Architecture"
 *
 * The ICE library is organized as follows:
 *
 *  - the lowest layer is \ref PJNATH_ICE_SESSION, which provides 
 *    ICE management and negotiation in a transport-independent way.
 *    This layer contains the state machines to perform ICE
 *    negotiation, and provides the most flexibility to control all
 *    aspects of ICE session. This layer normally is only usable for
 *    ICE implementors.
 *
 *  - higher in the hierarchy is \ref PJNATH_ICE_STREAM_TRANSPORT,
 *    which binds ICE with UDP sockets, and provides STUN binding
 *    and relay/TURN allocation for the sockets. This component can
 *    be directly used by application, although normally application
 *    should use the next higher abstraction below since it provides
 *    SDP translations and better integration with other PJ libraries
 *    such as PJSIP and PJMEDIA.
 *
 *  - the highest abstraction is ICE media transport, which maintains
 *    ICE stream transport and provides SDP translations to be used
 *    for SIP offer/answer exchanges.
 *
 * \subsection PJNATH_ICE_REF Reference
 *
 * References for ICE:
 *  - <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-mmusic-ice-15.txt">
 *    <B>draft-ietf-mmusic-ice-15.txt</B></A>: Interactive Connectivity 
 *    Establishment (ICE): A Methodology for Network Address Translator 
 *    (NAT) Traversal for Offer/Answer Protocols
 */

/**
 * @defgroup PJNATH_STUN STUN Library
 * @brief Open source STUN library
 *
 * This module contains implementation of STUN library in PJNATH -
 * the open source NAT helper containing STUN and ICE.
 */

#endif	/* __PJNATH_TYPES_H__ */


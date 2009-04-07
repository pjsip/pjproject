/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
 * This constant describes a number to be used to identify an invalid TURN
 * channel number.
 */
#define PJ_TURN_INVALID_CHANNEL	    0xFFFF


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

@mainpage PJNATH - Open Source ICE, STUN, and TURN Library

\n
This is the documentation of PJNATH, an Open Source library providing
NAT traversal helper functionalities by using standard based protocols
such as STUN, TURN, and ICE.

\n
\n

\section lib_comps Library Components

\subsection comp_stun STUN

Session Traversal Utilities (STUN, or previously known as Simple 
Traversal of User Datagram Protocol (UDP) Through Network Address 
Translators (NAT)s), is a lightweight protocol that serves as a tool for
application protocols in dealing with NAT traversal. It allows a client
to determine the IP address and port allocated to them by a NAT and to 
keep NAT bindings open.

This version of PJNATH implements the following STUN RFC:
- <A HREF="http://www.ietf.org/rfc/rfc5389.txt"><B>RFC 5389</b></A>: 
    Session Traversal Utilities for (NAT) (STUN),


\subsection comp_turn TURN

Traversal Using Relays around NAT (TURN) allows the host to control the
operation of the relay and to exchange packets with its peers using the relay.

This version of PJNATH implements both TCP and UDP client transport and it
complies with the following TURN draft:
 - <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-behave-turn-09.txt">
   <B>draft-ietf-behave-turn-09</B></A>: Obtaining Relay Addresses 
   from Simple Traversal Underneath NAT (STUN)


\subsection comp_ice ICE

Interactive Connectivity Establishment (ICE) is a standard based 
methodology for traversing Network Address Translator (NAT). This
implementation is aimed to provide a usable and generic ICE transports
for different types of application, including but not limited to
the usage of ICE in SIP/SDP offer/answer.


This version of PJNATH implements the following ICE draft:
 - <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-mmusic-ice-19.txt">
   <B>draft-ietf-mmusic-ice-19.txt</B></A> draft. The PJNATH ICE


\subsection comp_natck NAT Classification Utility

The PJNATH library also provides NAT classification utility as 
described in <A HREF="http://www.ietf.org/rfc/rfc3489.txt">RFC 3489</A>.
While the practice to detect the NAT type to assist NAT traversal
has been deprecated in favor of ICE, the information may still be
useful for troubleshooting purposes, hence the utility is provided.


\n
\n

\section lib_org Library Organization

The PJNATH library consists of many components with each providing
specific functionality that may or may not be of the interests of 
applications (or application developers). This section attempts to 
give brief overview on the components provided by PJNATH.

The PJNATH components from the highest layer to the lower layer are
as follows.


\n

\subsection user_comp High-level Transport Objects

PJNATH library provides some high-level objects that may be used
by applications:


\subsubsection stun_sock STUN Transport

The \ref PJNATH_STUN_SOCK provides asynchronous UDP like socket transport
with the additional capability to query the publicly mapped transport
address (using STUN resolution), to refresh the NAT binding, and to
demultiplex internal STUN messages from application data (the 
application data may be a STUN message as well).


\subsubsection turn_sock TURN Client Transport

The \ref PJNATH_TURN_SOCK may be used by the application to send and
receive data via TURN server. For more information please see the
documentation of \ref PJNATH_TURN_SOCK.


\subsubsection ice_strans ICE Stream Transport

The \ref PJNATH_ICE_STREAM_TRANSPORT provides transport interface to
send and receive data through connection that is negotiated
with ICE protocol. The \ref PJNATH_ICE_STREAM_TRANSPORT naturally 
contains both STUN Transport and \ref PJNATH_TURN_SOCK.

The \ref PJNATH_ICE_STREAM_TRANSPORT interface is suitable for both
SIP or non-SIP use. For SIP use, application may prefer to use the
ICE media transport in PJMEDIA instead where it has been integrated
with the SDP offer and answer mechanism.


\subsubsection natck NAT Classification Utility

PJNATH also provides \a PJNATH_NAT_DETECT to assist troubleshooting
of problems related to NAT traversal.



\n


\subsection sessions Transport Independent Sessions Layer

Right below the high level transports objects are the transport
independent sessions. These sessions don't have access to sockets,
so higher level objects (such as transports) must give incoming
packets to the sessions and provide callback to be called by
sessions to send outgoing packets.


\subsubsection ice_sess ICE Session

The \ref PJNATH_ICE_SESSION is used by the \ref PJNATH_ICE_STREAM_TRANSPORT
and contains the actual logic of the ICE negotiation.


\subsubsection turn_sess TURN Session

The \ref PJNATH_TURN_SESSION is used by the \ref PJNATH_TURN_SOCK
and it contains TURN protocol logic. Implementors may implement
other types of TURN client connection (such as TURN TLS client)
by utilizing this session.


\subsubsection stun_sess STUN Session

The \ref PJNATH_STUN_SESSION manages STUN message exchange between
a client and server (or vice versa). It manages \ref PJNATH_STUN_TRANSACTION
for sending or receiving requests and \ref PJNATH_STUN_AUTH for both
both incoming and outgoing STUN messages. 

The \ref PJNATH_STUN_SESSION is naturally used by the \ref PJNATH_TURN_SESSION
and \ref PJNATH_ICE_SESSION


\n

\subsection stun_tsx STUN Transaction Layer

The \ref PJNATH_STUN_TRANSACTION is a thin layer to manage retransmission
of STUN requests.


\n


\subsection stun_msg STUN Messaging Layer

At the very bottom of the PJNATH components is the \ref PJNATH_STUN_MSG
layer. The API contains various representation of STUN messaging components
and it provides API to encode and decode STUN messages.



\n
\n

\section class_dia Class Diagram


The following class diagram shows the interactions between objects in
PJNATH:

\image html UML-class-diagram.png "Class Diagram"
\image latex UML-class-diagram.png "Class Diagram"



\n
\n

\section samples Sample Applications


Some sample applications have been provided with PJNATH, and it's available
under <tt>pjnath/src</tt> directory:

   - <b>pjturn-client</b>: this is a stand-alone, console based TURN client
     application to be used as a demonstration for PJNATH TURN client 
     transport API and for simple testing against TURN server implementations.
     The client supports both UDP and TCP connection to the TURN server.

   - <b>pjturn-srv</b>: this is a simple TURN server to be used for testing
     purposes. It supports both UDP and TCP connections to the clients.


*/

/**
 * @defgroup PJNATH_STUN STUN Library
 * @brief Open source STUN library
 *
 * This module contains implementation of STUN library in PJNATH -
 * the open source NAT helper containing STUN and ICE.
 */

#endif	/* __PJNATH_TYPES_H__ */


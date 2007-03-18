/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#ifndef __PJ_STUN_SERVER_H__
#define __PJ_STUN_SERVER_H__

/*
 * STUN documentation. There is no code here.
 */

/**
 * @defgroup PJNATH_STUN STUN and TURN
 * @ingroup PJNATH

 This is the implementation of STUN/TURN in PJLIB-UTIL library.

 The STUN/TURN implementation in PJLIB-UTIL has the following objectives:
 - standard based (of course)
 - supports both client and server side STUN/TURN services
 - independent (that is, only dependent to pjlib), and general purpose
   enough to be used not only by pjsip applications but also by other 
   types of applications
 - must be able to support ICE.

 The STUN/TURN implementation is based on the following standards:
 - <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-behave-rfc3489bis-05.txt">
   draft-ietf-behave-rfc3489bis-05.txt</A>
 - <A HREF="http://www.ietf.org/internet-drafts/draft-ietf-behave-turn-02.txt">
   draft-ietf-behave-turn-02.txt</A>

 But as STUN standards are currently defined as work in progress at IETF,
 the implementation will be updated as these standards are updated.

  

 @section stun_org_sec Organization

 The implementation consists of the following components.

 @subsection stun_msg_sec Messaging and Parsing

 The lowest layer of the STUN implementation is the @ref PJNATH_STUN_MSG
 component. This part is responsible for encoding and decoding STUN messages.

 This layer only implements message representation and parsing. In particular,
 it does not provide any transport functionalities, therefore it can be used
 by different types of applications.



 @subsection stun_endpt_sec Endpoint

 The @ref PJNATH_STUN_ENDPOINT is used by the library to put together
 common settings for all STUN objects. For example, the STUN endpoint has a
 reference of timer heap to poll all STUN timers, reference to ioqueue to
 poll network events for STUN servers, and some common settings used by
 various STUN objects.


 @subsection stun_clt_tsx_sec Client Transaction

 The @ref PJNATH_STUN_TRANSACTION is used to manage outgoing STUN request,
 for example to retransmit the request and to notify application about the
 completion of the request.

 The @ref PJNATH_STUN_TRANSACTION does not use any networking operations,
 but instead application must supply the transaction with a callback to
 be used by the transaction to send outgoing requests. This way the STUN
 transaction is made more generic and can work with different types of
 networking codes in application.



 @subsection stun_srv_sec Server Components

 The @ref PJNATH_STUN_SERVER is used for:
 - implementing STUN servers, and/or
 - implementing server side STUN handling (for example for ICE).

 */



#endif	/* __PJ_STUN_SERVER_H__ */


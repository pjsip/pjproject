/* $Id */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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
#ifndef __PJLIB_UTIL_ERRNO_H__
#define __PJLIB_UTIL_ERRNO_H__


#include <pj/errno.h>


/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 * This value is 320000.
 */
#define PJLIB_UTIL_ERRNO_START    (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*3)


/************************************************************
 * STUN ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * Unable to resolve STUN server
 */
#define PJLIB_UTIL_ESTUNRESOLVE	    (PJLIB_UTIL_ERRNO_START+1)	/* 320001 */
/**
 * @hideinitializer
 * Unknown STUN message type.
 */
#define PJLIB_UTIL_ESTUNINMSGTYPE   (PJLIB_UTIL_ERRNO_START+2)	/* 320002 */
/**
 * @hideinitializer
 * Invalid STUN message length.
 */
#define PJLIB_UTIL_ESTUNINMSGLEN    (PJLIB_UTIL_ERRNO_START+3)	/* 320003 */
/**
 * @hideinitializer
 * STUN attribute length error.
 */
#define PJLIB_UTIL_ESTUNINATTRLEN   (PJLIB_UTIL_ERRNO_START+4)	/* 320004 */
/**
 * @hideinitializer
 * Invalid STUN attribute type
 */
#define PJLIB_UTIL_ESTUNINATTRTYPE  (PJLIB_UTIL_ERRNO_START+5)	/* 320005 */
/**
 * @hideinitializer
 * Invalid STUN server/socket index
 */
#define PJLIB_UTIL_ESTUNININDEX     (PJLIB_UTIL_ERRNO_START+6)	/* 320006 */
/**
 * @hideinitializer
 * No STUN binding response in the message
 */
#define PJLIB_UTIL_ESTUNNOBINDRES   (PJLIB_UTIL_ERRNO_START+7)	/* 320007 */
/**
 * @hideinitializer
 * Received STUN error attribute
 */
#define PJLIB_UTIL_ESTUNRECVERRATTR (PJLIB_UTIL_ERRNO_START+8)	/* 320008 */
/**
 * @hideinitializer
 * No STUN mapped address attribute
 */
#define PJLIB_UTIL_ESTUNNOMAP       (PJLIB_UTIL_ERRNO_START+9)	/* 320009 */
/**
 * @hideinitializer
 * Received no response from STUN server
 */
#define PJLIB_UTIL_ESTUNNOTRESPOND  (PJLIB_UTIL_ERRNO_START+10)	/* 320010 */
/**
 * @hideinitializer
 * Symetric NAT detected by STUN
 */
#define PJLIB_UTIL_ESTUNSYMMETRIC   (PJLIB_UTIL_ERRNO_START+11)	/* 320011 */



/************************************************************
 * XML ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * General invalid XML message.
 */
#define PJLIB_UTIL_EINXML	    (PJLIB_UTIL_ERRNO_START+20)	/* 320020 */



/************************************************************
 * DNS ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * Outgoing DNS query packet buffer is too small.
 * This error occurs when the user supplied buffer for creating DNS
 * query (#pj_dns_make_query() function) is too small.
 */
#define PJLIB_UTIL_EDNSQRYTOOSMALL  (PJLIB_UTIL_ERRNO_START+40)	/* 320040 */
/**
 * @hideinitializer
 * Invalid packet length in DNS response.
 * This error occurs when the received DNS response packet does not
 * match all the fields length.
 */
#define PJLIB_UTIL_EDNSINSIZE	    (PJLIB_UTIL_ERRNO_START+41)	/* 320041 */
/**
 * @hideinitializer
 * Invalid class in DNS response.
 * This error occurs when the received DNS response contains network
 * class other than IN (Internet).
 */
#define PJLIB_UTIL_EDNSINCLASS	    (PJLIB_UTIL_ERRNO_START+42)	/* 320042 */
/**
 * @hideinitializer
 * Invalid name pointer in DNS response.
 * This error occurs when parsing the compressed names inside DNS
 * response packet, when the name pointer points to an invalid address
 * or the parsing has triggerred too much recursion.
 */
#define PJLIB_UTIL_EDNSINNAMEPTR    (PJLIB_UTIL_ERRNO_START+43)	/* 320043 */



#endif	/* __PJLIB_UTIL_ERRNO_H__ */

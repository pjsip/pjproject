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
 */
#define PJLIB_UTIL_ERRNO_START    (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*3)


/************************************************************
 * STUN ERROR
 ***********************************************************/
/**
 * @hideinitializer
 * Unable to resolve STUN server
 */
#define PJLIB_UTIL_ESTUNRESOLVE	    (PJLIB_UTIL_ERRNO_START+1)
/**
 * @hideinitializer
 * Unknown STUN message type.
 */
#define PJLIB_UTIL_ESTUNINMSGTYPE   (PJLIB_UTIL_ERRNO_START+2)
/**
 * @hideinitializer
 * Invalid STUN message length.
 */
#define PJLIB_UTIL_ESTUNINMSGLEN    (PJLIB_UTIL_ERRNO_START+3)
/**
 * @hideinitializer
 * STUN attribute length error.
 */
#define PJLIB_UTIL_ESTUNINATTRLEN   (PJLIB_UTIL_ERRNO_START+4)
/**
 * @hideinitializer
 * Invalid STUN attribute type
 */
#define PJLIB_UTIL_ESTUNINATTRTYPE  (PJLIB_UTIL_ERRNO_START+5)
/**
 * @hideinitializer
 * Invalid STUN server/socket index
 */
#define PJLIB_UTIL_ESTUNININDEX     (PJLIB_UTIL_ERRNO_START+6)
/**
 * @hideinitializer
 * No STUN binding response in the message
 */
#define PJLIB_UTIL_ESTUNNOBINDRES   (PJLIB_UTIL_ERRNO_START+7)
/**
 * @hideinitializer
 * Received STUN error attribute
 */
#define PJLIB_UTIL_ESTUNRECVERRATTR (PJLIB_UTIL_ERRNO_START+8)
/**
 * @hideinitializer
 * No STUN mapped address attribute
 */
#define PJLIB_UTIL_ESTUNNOMAP       (PJLIB_UTIL_ERRNO_START+9)
/**
 * @hideinitializer
 * Received no response from STUN server
 */
#define PJLIB_UTIL_ESTUNNOTRESPOND  (PJLIB_UTIL_ERRNO_START+10)
/**
 * @hideinitializer
 * Symetric NAT detected by STUN
 */
#define PJLIB_UTIL_ESTUNSYMMETRIC   (PJLIB_UTIL_ERRNO_START+11)



#endif	/* __PJLIB_UTIL_ERRNO_H__ */

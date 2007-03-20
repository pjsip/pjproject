/* $Id$ */
/* 
 * Copyright (C)2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __PJNATH_ERRNO_H__
#define __PJNATH_ERRNO_H__


#include <pj/errno.h>

/**
 * @defgroup PJNATH_ERROR NAT Helper Error Codes
 * @ingroup PJNATH
 * @{
 */

/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 * This value is 370000.
 */
#define PJNATH_ERRNO_START    (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*4)



/************************************************************
 * NEW STUN ERROR
 ***********************************************************/
/* Messaging errors */
#define PJNATH_ESTUNINATTRLEN	    -1
#define PJNATH_ESTUNINMSGLEN	    -1
#define	PJNATH_ESTUNINMSGTYPE	    -1
#define PJNATH_ESTUNFINGERPRINT	    -1
#define PJNATH_ESTUNNOTRESPOND	    -1
#define PJNATH_ESTUNNOXORMAP	    -1

/**
 * @hideinitializer
 * Too many STUN attributes.
 */
#define PJNATH_ESTUNTOOMANYATTR	    (PJNATH_ERRNO_START+110)/* 370110 */
/**
 * @hideinitializer
 * Unknown STUN attribute. This error happens when the decoder encounters
 * mandatory attribute type which it doesn't understand.
 */
#define PJNATH_ESTUNUNKNOWNATTR	    (PJNATH_ERRNO_START+111)/* 370111 */
/**
 * @hideinitializer
 * Invalid STUN socket address length.
 */
#define PJNATH_ESTUNINADDRLEN	    (PJNATH_ERRNO_START+112)/* 370112 */
/**
 * @hideinitializer
 * STUN IPv6 attribute not supported
 */
#define PJNATH_ESTUNIPV6NOTSUPP	    (PJNATH_ERRNO_START+113)/* 370113 */
/**
 * @hideinitializer
 * Expecting STUN response message.
 */
#define PJNATH_ESTUNNOTRESPONSE	    (PJNATH_ERRNO_START+114)/* 370114 */
/**
 * @hideinitializer
 * STUN transaction ID mismatch.
 */
#define PJNATH_ESTUNINVALIDID	    (PJNATH_ERRNO_START+115)/* 370115 */
/**
 * @hideinitializer
 * Unable to find handler for the request.
 */
#define PJNATH_ESTUNNOHANDLER	    (PJNATH_ERRNO_START+116)/* 370116 */
/**
 * @hideinitializer
 * Found non-FINGERPRINT attribute after MESSAGE-INTEGRITY. This is not
 * valid since MESSAGE-INTEGRITY MUST be the last attribute or the
 * attribute right before FINGERPRINT before the message.
 */
#define PJNATH_ESTUNMSGINTPOS	    (PJNATH_ERRNO_START+118)/* 370118 */
/**
 * @hideinitializer
 * Found attribute after FINGERPRINT. This is not valid since FINGERPRINT
 * MUST be the last attribute in the message.
 */
#define PJNATH_ESTUNFINGERPOS	    (PJNATH_ERRNO_START+119)/* 370119 */
/**
 * @hideinitializer
 * Missing STUN USERNAME attribute.
 * When credential is included in the STUN message (MESSAGE-INTEGRITY is
 * present), the USERNAME attribute must be present in the message.
 */
#define PJNATH_ESTUNNOUSERNAME	    (PJNATH_ERRNO_START+120)/* 370120 */
/**
 * @hideinitializer
 * Unknown STUN username/credential.
 */
#define PJNATH_ESTUNUSERNAME	    (PJNATH_ERRNO_START+121)/* 370121 */
/**
 * @hideinitializer
 * Missing/invalidSTUN MESSAGE-INTEGRITY attribute.
 */
#define PJNATH_ESTUNMSGINT	    (PJNATH_ERRNO_START+122)/* 370122 */
/**
 * @hideinitializer
 * Found duplicate STUN attribute.
 */
#define PJNATH_ESTUNDUPATTR	    (PJNATH_ERRNO_START+123)/* 370123 */
/**
 * @hideinitializer
 * Missing STUN REALM attribute.
 */
#define PJNATH_ESTUNNOREALM	    (PJNATH_ERRNO_START+124)/* 370124 */
/**
 * @hideinitializer
 * Missing/stale STUN NONCE attribute value.
 */
#define PJNATH_ESTUNNONCE	    (PJNATH_ERRNO_START+125)/* 370125 */
/**
 * @hideinitializer
 * STUN transaction terminates with failure.
 */
#define PJNATH_ESTUNTSXFAILED	    (PJNATH_ERRNO_START+126)/* 370126 */


//#define PJ_STATUS_FROM_STUN_CODE(code)	(PJNATH_ERRNO_START+code)


/**
 * @hideinitializer
 * No ICE checklist is formed.
 */
#define PJ_EICENOCHECKLIST	    -1



/**
 * @}
 */

#endif	/* __PJNATH_ERRNO_H__ */

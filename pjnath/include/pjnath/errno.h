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

/**
 * @file errno.h
 * @brief PJNATH specific error codes
 */

#include <pj/errno.h>

/**
 * @defgroup PJNATH_ERROR NAT Helper Library Error Codes
 * @brief PJNATH specific error code constants
 * @{
 */

/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 * This value is 370000.
 */
#define PJNATH_ERRNO_START    (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE*4)


/************************************************************
 * STUN MESSAGING ERRORS
 ***********************************************************/

/**
 * Map STUN error code (300-699) into pj_status_t error space.
 */
#define PJ_STATUS_FROM_STUN_CODE(code)	(PJNATH_ERRNO_START+code)

/**
 * @hideinitializer
 * Invalid STUN message length.
 */
#define PJNATH_EINSTUNMSGLEN	    (PJNATH_ERRNO_START+1)  /* 370001 */
/**
 * @hideinitializer
 * Invalid or unexpected STUN message type
 */
#define	PJNATH_EINSTUNMSGTYPE	    (PJNATH_ERRNO_START+2)  /* 370002 */
/**
 * @hideinitializer
 * STUN transaction has timed out
 */
#define PJNATH_ESTUNTIMEDOUT	    (PJNATH_ERRNO_START+3)  /* 370003 */



/**
 * @hideinitializer
 * Too many STUN attributes.
 */
#define PJNATH_ESTUNTOOMANYATTR	    (PJNATH_ERRNO_START+21) /* 370021 */
/**
 * @hideinitializer
 * Invalid STUN attribute length.
 */
#define PJNATH_ESTUNINATTRLEN	    (PJNATH_ERRNO_START+22) /* 370022 */
/**
 * @hideinitializer
 * Found duplicate STUN attribute.
 */
#define PJNATH_ESTUNDUPATTR	    (PJNATH_ERRNO_START+23) /* 370023 */

/**
 * @hideinitializer
 * STUN FINGERPRINT verification failed
 */
#define PJNATH_ESTUNFINGERPRINT	    (PJNATH_ERRNO_START+30) /* 370030 */
/**
 * @hideinitializer
 * Invalid STUN attribute after MESSAGE-INTEGRITY.
 */
#define PJNATH_ESTUNMSGINTPOS	    (PJNATH_ERRNO_START+31) /* 370031 */
/**
 * @hideinitializer
 * Invalid STUN attribute after FINGERPRINT.
 */
#define PJNATH_ESTUNFINGERPOS	    (PJNATH_ERRNO_START+33) /* 370033 */


/**
 * @hideinitializer
 * STUN (XOR-)MAPPED-ADDRESS attribute not found
 */
#define PJNATH_ESTUNNOMAPPEDADDR    (PJNATH_ERRNO_START+40) /* 370040 */
/**
 * @hideinitializer
 * STUN IPv6 attribute not supported
 */
#define PJNATH_ESTUNIPV6NOTSUPP	    (PJNATH_ERRNO_START+41) /* 370041 */




/************************************************************
 * ICE ERROR CODES
 ***********************************************************/

/**
 * @hideinitializer
 * ICE session not available
 */
#define PJNATH_ENOICE		    (PJNATH_ERRNO_START+80) /* 370080 */
/**
 * @hideinitializer
 * ICE check is in progress
 */
#define PJNATH_EICEINPROGRESS	    (PJNATH_ERRNO_START+81) /* 370081 */
/**
 * @hideinitializer
 * All ICE checklists failed
 */
#define PJNATH_EICEFAILED	    (PJNATH_ERRNO_START+82) /* 370082 */
/**
 * @hideinitializer
 * Invalid ICE component ID
 */
#define PJNATH_EICEINCOMPID	    (PJNATH_ERRNO_START+86) /* 370086 */
/**
 * @hideinitializer
 * Invalid ICE candidate ID
 */
#define PJNATH_EICEINCANDID	    (PJNATH_ERRNO_START+87) /* 370087 */
/**
 * @hideinitializer
 * Missing ICE SDP attribute
 */
#define PJNATH_EICEMISSINGSDP	    (PJNATH_ERRNO_START+90) /* 370090 */
/**
 * @hideinitializer
 * Invalid SDP "candidate" attribute
 */
#define PJNATH_EICEINCANDSDP	    (PJNATH_ERRNO_START+91) /* 370091 */



/**
 * @}
 */

#endif	/* __PJNATH_ERRNO_H__ */

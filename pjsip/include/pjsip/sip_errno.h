/* $Id$  */
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
#ifndef __PJSIP_SIP_ERRNO_H__
#define __PJSIP_SIP_ERRNO_H__

#include <pj/errno.h>

PJ_BEGIN_DECL

/*
 * PJSIP error codes occupies 170000 - 219000, and mapped as follows:
 *  - 170100 - 170799: mapped to SIP status code in response msg.
 *  - 171000 - 171999: mapped to errors generated from PJSIP core.
 */

/**
 * Get error message for the specified error code.
 *
 * @param status    The error code.
 * @param buffer    The buffer where to put the error message.
 * @param bufsize   Size of the buffer.
 *
 * @return	    The error message as NULL terminated string,
 *                  wrapped with pj_str_t.
 */
PJ_DECL(pj_str_t) pjsip_strerror( pj_status_t status, char *buffer,
				  pj_size_t bufsize);

/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 */
#define PJSIP_ERRNO_START       (PJ_ERRNO_START_USER)

/**
 * Create error value from SIP status code.
 * @param code      SIP status code.
 * @return          Error code in pj_status_t namespace.
 */
#define PJSIP_ERRNO_FROM_SIP_STATUS(code)   (PJSIP_ERRNO_START+code)

/**
 * Get SIP status code from error value.
 * If conversion to SIP status code is not available, a SIP status code
 * 599 will be returned.
 *
 * @param status    Error code in pj_status_t namespace.
 * @return          SIP status code.
 */
#define PJSIP_ERRNO_TO_SIP_STATUS(status)               \
         ((status>=PJSIP_ERRNO_FROM_SIP_STATUS(100) &&  \
           status<PJSIP_ERRNO_FROM_SIP_STATUS(800)) ?   \
          status-PJSIP_ERRNO_FROM_SIP_STATUS(0) : 599)


/**
 * Start of PJSIP generated error code values.
 */
#define PJSIP_ERRNO_START_PJSIP (PJSIP_ERRNO_START + 1000)

/************************************************************
 * GENERIC SIP ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * SIP object is busy.
 */
#define PJSIP_EBUSY		(PJSIP_ERRNO_START_PJSIP + 1)	/* 171001 */
/**
 * @hideinitializer
 * SIP object with the same type already exists.
 */
#define PJSIP_ETYPEEXISTS	(PJSIP_ERRNO_START_PJSIP + 2)	/* 171002 */


/************************************************************
 * MESSAGING ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * General invalid message error (e.g. syntax error)
 */
#define PJSIP_EINVALIDMSG       (PJSIP_ERRNO_START_PJSIP + 20)	/* 171020 */
/**
 * @hideinitializer
 * Unsupported URL scheme.
 */
#define PJSIP_EINVALIDSCHEME    (PJSIP_ERRNO_START_PJSIP + 21)	/* 171021 */
/**
 * @hideinitializer
 * Message too long. See also PJSIP_ERXOVERFLOW.
 */
#define PJSIP_EMSGTOOLONG	(PJSIP_ERRNO_START_PJSIP + 22)	/* 171022 */
/**
 * @hideinitializer
 * Message not completely received.
 */
#define PJSIP_EPARTIALMSG       (PJSIP_ERRNO_START_PJSIP + 23)	/* 171023 */
/**
 * @hideinitializer
 * Missing Request-URI.
 */
#define PJSIP_EMISSINGREQURI    (PJSIP_ERRNO_START_PJSIP + 24)	/* 171024 */
/**
 * @hideinitializer
 * Missing required header(s).
 */
#define PJSIP_EMISSINGHDR       (PJSIP_ERRNO_START_PJSIP + 25)	/* 171025 */
/**
 * @hideinitializer
 * Missing message body.
 */
#define PJSIP_EMISSINGBODY	(PJSIP_ERRNO_START_PJSIP + 26)	/* 171026 */
/**
 * @hideinitializer
 * Invalid Via header in response (sent-by, etc).
 */
#define PJSIP_EINVALIDVIA	(PJSIP_ERRNO_START_PJSIP + 27)	/* 171027 */
/**
 * @hideinitializer
 * Multiple Via headers in response.
 */
#define PJSIP_EMULTIPLEVIA	(PJSIP_ERRNO_START_PJSIP + 28)	/* 171028 */
/**
 * @hideinitializer
 * Invalid request URI.
 */
#define PJSIP_EINVALIDREQURI	(PJSIP_ERRNO_START_PJSIP + 29)	/* 171029 */
/**
 * @hideinitializer
 * Expecting request message.
 */
#define PJSIP_ENOTREQUESTMSG	(PJSIP_ERRNO_START_PJSIP + 30)	/* 171030 */
/**
 * @hideinitializer
 * Expecting response message.
 */
#define PJSIP_ENOTRESPONSEMSG	(PJSIP_ERRNO_START_PJSIP + 31)	/* 171031 */
/**
 * @hideinitializer
 * Invalid header field.
 */
#define PJSIP_EINVALIDHDR	(PJSIP_ERRNO_START_PJSIP + 32)	/* 171032 */


/************************************************************
 * TRANSPORT ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Unsupported transport type.
 */
#define PJSIP_EUNSUPTRANSPORT	(PJSIP_ERRNO_START_PJSIP + 40)	/* 171040 */
/**
 * @hideinitializer
 * Buffer is being sent, operation still pending.
 */
#define PJSIP_EPENDINGTX	(PJSIP_ERRNO_START_PJSIP + 41)	/* 171041 */
/**
 * @hideinitializer
 * Rx buffer overflow. See also PJSIP_EMSGTOOLONG.
 */
#define PJSIP_ERXOVERFLOW       (PJSIP_ERRNO_START_PJSIP + 42)	/* 171042 */
/**
 * @hideinitializer
 * This is not really an error, it just informs application that
 * transmit data has been deleted on return of pjsip_tx_data_dec_ref().
 */
#define PJSIP_EBUFDESTROYED     (PJSIP_ERRNO_START_PJSIP + 43)	/* 171043 */


/************************************************************
 * TRANSACTION ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Transaction has just been destroyed.
 */
#define PJSIP_ETSXDESTROYED     (PJSIP_ERRNO_START_PJSIP + 60)	/* 171060 */


/************************************************************
 * URI COMPARISON RESULTS
 ***********************************************************/
/**
 * @hideinitializer
 * Scheme mismatch.
 */
#define PJSIP_ECMPSCHEME	(PJSIP_ERRNO_START_PJSIP + 80)	/* 171080 */
/**
 * @hideinitializer
 * User part mismatch.
 */
#define PJSIP_ECMPUSER		(PJSIP_ERRNO_START_PJSIP + 81)	/* 171081 */
/**
 * @hideinitializer
 * Password part mismatch.
 */
#define PJSIP_ECMPPASSWD	(PJSIP_ERRNO_START_PJSIP + 82)	/* 171082 */
/**
 * @hideinitializer
 * Host part mismatch.
 */
#define PJSIP_ECMPHOST		(PJSIP_ERRNO_START_PJSIP + 83)	/* 171083 */
/**
 * @hideinitializer
 * Port part mismatch.
 */
#define PJSIP_ECMPPORT		(PJSIP_ERRNO_START_PJSIP + 84)	/* 171084 */
/**
 * @hideinitializer
 * Transport parameter part mismatch.
 */
#define PJSIP_ECMPTRANSPORTPRM	(PJSIP_ERRNO_START_PJSIP + 85)	/* 171085 */
/**
 * @hideinitializer
 * TTL parameter part mismatch.
 */
#define PJSIP_ECMPTTLPARAM	(PJSIP_ERRNO_START_PJSIP + 86)	/* 171086 */
/**
 * @hideinitializer
 * User parameter part mismatch.
 */
#define PJSIP_ECMPUSERPARAM	(PJSIP_ERRNO_START_PJSIP + 87)	/* 171087 */
/**
 * @hideinitializer
 * Method parameter part mismatch.
 */
#define PJSIP_ECMPMETHODPARAM	(PJSIP_ERRNO_START_PJSIP + 88)	/* 171088 */
/**
 * @hideinitializer
 * Maddr parameter part mismatch.
 */
#define PJSIP_ECMPMADDRPARAM	(PJSIP_ERRNO_START_PJSIP + 89)	/* 171089 */
/**
 * @hideinitializer
 * Parameter part in other_param mismatch.
 */
#define PJSIP_ECMPOTHERPARAM	(PJSIP_ERRNO_START_PJSIP + 90)	/* 171090 */
/**
 * @hideinitializer
 * Parameter part in header_param mismatch.
 */
#define PJSIP_ECMPHEADERPARAM	(PJSIP_ERRNO_START_PJSIP + 91)	/* 171091 */



PJ_END_DECL

#endif	/* __PJSIP_SIP_ERRNO_H__ */

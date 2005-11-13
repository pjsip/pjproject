/* $Id$ 
 */
/* 
 * PJSIP - SIP Stack
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __PJSIP_SIP_ERRNO_H__
#define __PJSIP_SIP_ERRNO_H__

#include <pj/errno.h>

/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 */
#define PJSIP_ERRNO_START       (PJ_ERRNO_START_USER+10000)

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
           status<PJSIP_ERRNO_FROM_SIP_STATUS(999)) ?   \
          status-PJSIP_ERRNO_FROM_SIP_STATUS(0) : 599)


/**
 * Start of PJSIP generated error code values.
 */
#define PJSIP_ERRNO_START_PJSIP (PJSIP_ERRNO_START + 10000)

/**
 * @hideinitializer
 * Missing required header(s).
 */
#define PJSIP_EMISSINGHDR       (PJSIP_ERRNO_START_PJSIP + 1)
/**
 * @hideinitializer
 * Unsupported URL scheme.
 */
#define PJSIP_EINVALIDSCHEME    (PJSIP_ERRNO_START_PJSIP + 2)
/**
 * @hideinitializer
 * Transaction has just been destroyed.
 */
#define PJSIP_ETSXDESTROYED     (PJSIP_ERRNO_START_PJSIP + 3)
/**
 * @hideinitializer
 * Buffer overflow. See also PJSIP_EMSGTOOLONG.
 */
#define PJSIP_EOVERFLOW         (PJSIP_ERRNO_START_PJSIP + 4)
/**
 * @hideinitializer
 * Message not completely received.
 */
#define PJSIP_EPARTIALMSG       (PJSIP_ERRNO_START_PJSIP + 5)
/**
 * @hideinitializer
 * Message too long. See also PJSIP_EOVERFLOW.
 */
#define PJSIP_EMSGTOOLONG	(PJSIP_ERRNO_START_PJSIP + 6)
/**
 * @hideinitializer
 * Buffer is being sent, operation still pending.
 */
#define PJSIP_EPENDINGTX	(PJSIP_ERRNO_START_PJSIP + 7)
/**
 * @hideinitializer
 * Unsupported transport type.
 */
#define PJSIP_EUNSUPTRANSPORT	(PJSIP_ERRNO_START_PJSIP + 8)
/**
 * @hideinitializer
 * Invalid Via header in response (sent-by, etc).
 */
#define PJSIP_EINVALIDVIA	(PJSIP_ERRNO_START_PJSIP + 9)


#endif	/* __PJSIP_SIP_ERRNO_H__ */

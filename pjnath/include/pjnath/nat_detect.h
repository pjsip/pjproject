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
#ifndef __PJNATH_NAT_DETECT_H__
#define __PJNATH_NAT_DETECT_H__

/**
 * @file ice_session.h
 * @brief ICE session management
 */
#include <pjnath/stun_session.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJNATH_NAT_DETECT NAT Classification/Detection Tool
 * @brief NAT Classification/Detection Tool
 * @ingroup PJNATH_ICE
 * @{
 * This module provides one function to perform NAT classification and
 * detection.
 */

/**
 * This enumeration describes the NAT types.
 */
typedef enum pj_stun_nat_type
{
    PJ_STUN_NAT_TYPE_UNKNOWN,
    PJ_STUN_NAT_TYPE_OPEN,
    PJ_STUN_NAT_TYPE_BLOCKED,
    PJ_STUN_NAT_TYPE_SYMMETRIC_UDP,
    PJ_STUN_NAT_TYPE_FULL_CONE,
    PJ_STUN_NAT_TYPE_SYMMETRIC,
    PJ_STUN_NAT_TYPE_RESTRICTED,
    PJ_STUN_NAT_TYPE_PORT_RESTRICTED
} pj_stun_nat_type;


/**
 * This structure contains the result of NAT classification function.
 */
typedef struct pj_stun_nat_detect_result
{
    pj_status_t		 status;
    const char		*status_text;
    pj_stun_nat_type	 nat_type;
    const char		*nat_type_name;
} pj_stun_nat_detect_result;


/**
 * Type of callback to be called when the NAT detection function has
 * completed.
 */
typedef void pj_stun_nat_detect_cb(void *user_data,
				   const pj_stun_nat_detect_result *res);


/**
 * Perform NAT classification function.
 */
PJ_DECL(pj_status_t) pj_stun_detect_nat_type(const pj_sockaddr_in *server,
					     pj_stun_config *stun_cfg,
					     void *user_data,
					     pj_stun_nat_detect_cb *cb);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_NAT_DETECT_H__ */


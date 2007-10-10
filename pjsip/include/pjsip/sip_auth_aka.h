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
#ifndef __PJSIP_AUTH_SIP_AUTH_AKA_H__
#define __PJSIP_AUTH_SIP_AUTH_AKA_H__

/**
 * @file sip_auth_aka.h
 * @brief SIP Digest AKA Authorization Module.
 */

#include <pjsip/sip_auth.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_AUTH_AKA_API Digest AKA Authentication API's
 * @ingroup PJSIP_AUTH_API
 * @brief Digest AKA helper API.
 * @{
 *
 * This module currently exports one function, #pjsip_auth_create_akav1_response(),
 * which can be registered as the callback function in \a ext.aka.cb field
 * of #pjsip_cred_info structure, to calculate the MD5-AKAv1 digest
 * response.
 */


#define PJSIP_AKA_AKLEN		6
#define PJSIP_AKA_AMFLEN	2
#define PJSIP_AKA_AUTNLEN	16
#define PJSIP_AKA_CKLEN		16
#define PJSIP_AKA_IKLEN		16
#define PJSIP_AKA_KLEN		16
#define PJSIP_AKA_OPLEN		16
#define PJSIP_AKA_RANDLEN	16
#define PJSIP_AKA_RESLEN	8
#define PJSIP_AKA_MACLEN	8

/**
 * This function creates MD5 AKAv1 response for the specified challenge
 * in \a chal, based on the information in the credential \a cred.
 * Application may register this function as \a ext.aka.cb field of
 * #pjsip_cred_info structure to make PJSIP automatically call this
 * function to calculate the response digest.
 *
 * @param pool	    Pool to allocate memory.
 * @param chal	    The authentication challenge sent by server in 401
 *		    or 401 response, in either Proxy-Authenticate or
 *		    WWW-Authenticate header.
 * @param cred	    The credential that has been selected by the framework
 *		    to authenticate against the challenge.
 * @param method    The request method.
 * @param auth	    The authentication credential where the digest response
 *		    will be placed to.
 *
 * @return	    PJ_SUCCESS if response has been created successfully.
 */
PJ_DECL(pj_status_t) pjsip_auth_create_akav1(pj_pool_t *pool,
					     const pjsip_digest_challenge*chal,
					     const pjsip_cred_info *cred,
					     const pj_str_t *method,
					     pjsip_digest_credential *auth);


/**
 * @}
 */



PJ_END_DECL


#endif	/* __PJSIP_AUTH_SIP_AUTH_AKA_H__ */


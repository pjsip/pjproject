/* $Id$ */
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
#ifndef __PJSIP_AUTH_SIP_AUTH_MSG_H__
#define __PJSIP_AUTH_SIP_AUTH_MSG_H__

#include <pjsip/sip_msg.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_MSG_AUTHORIZATION Header Field: Authorization and Proxy-Authorization
 * @brief Authorization and Proxy-Authorization header field.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Common credential.
 */
struct pjsip_common_credential
{
    pj_str_t	realm;
};

typedef struct pjsip_common_credential pjsip_common_credential;


/**
 * This structure describe credential used in Authorization and
 * Proxy-Authorization header for digest authentication scheme.
 */
struct pjsip_digest_credential
{
    pj_str_t	realm;
    pj_str_t	username;
    pj_str_t	nonce;
    pj_str_t	uri;
    pj_str_t	response;
    pj_str_t	algorithm;
    pj_str_t	cnonce;
    pj_str_t	opaque;
    pj_str_t	qop;
    pj_str_t	nc;
    pj_str_t	other_param;
};

typedef struct pjsip_digest_credential pjsip_digest_credential;

/**
 * This structure describe credential used in Authorization and
 * Proxy-Authorization header for PGP authentication scheme.
 */
struct pjsip_pgp_credential
{
    pj_str_t	realm;
    pj_str_t	version;
    pj_str_t	signature;
    pj_str_t	signed_by;
    pj_str_t	nonce;
};

typedef struct pjsip_pgp_credential pjsip_pgp_credential;

/**
 * This structure describes SIP Authorization header (and also SIP
 * Proxy-Authorization header).
 */
struct pjsip_authorization_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_authorization_hdr);
    pj_str_t scheme;
    union
    {
	pjsip_common_credential common;
	pjsip_digest_credential digest;
	pjsip_pgp_credential	pgp;
    } credential;
};

typedef struct pjsip_authorization_hdr pjsip_authorization_hdr;

/** SIP Proxy-Authorization header shares the same structure as SIP
    Authorization header.
 */
typedef struct pjsip_authorization_hdr pjsip_proxy_authorization_hdr;

/**
 * Create SIP Authorization header.
 * @param pool Pool where memory will be allocated from.
 * @return SIP Authorization header.
 */
PJ_DECL(pjsip_authorization_hdr*) pjsip_authorization_hdr_create(pj_pool_t *pool);

/**
 * Create SIP Proxy-Authorization header.
 * @param pool Pool where memory will be allocated from.
 * @return SIP Proxy-Authorization header.
 */
PJ_DECL(pjsip_proxy_authorization_hdr*) pjsip_proxy_authorization_hdr_create(pj_pool_t *pool);


/**
 * @}
 */

/**
 * @defgroup PJSIP_WWW_AUTH Header Field: Proxy-Authenticate and WWW-Authenticate
 * @brief Proxy-Authenticate and WWW-Authenticate.
 * @ingroup PJSIP_MSG
 * @{
 */

struct pjsip_common_challenge
{
    pj_str_t	realm;
};

typedef struct pjsip_common_challenge pjsip_common_challenge;

/**
 * This structure describes authentication challenge used in Proxy-Authenticate
 * or WWW-Authenticate for digest authentication scheme.
 */
struct pjsip_digest_challenge
{
    pj_str_t	realm;
    pj_str_t	domain;
    pj_str_t	nonce;
    pj_str_t	opaque;
    int		stale;
    pj_str_t	algorithm;
    pj_str_t	qop;
    pj_str_t	other_param;
};

typedef struct pjsip_digest_challenge pjsip_digest_challenge;

/**
 * This structure describes authentication challenge used in Proxy-Authenticate
 * or WWW-Authenticate for PGP authentication scheme.
 */
struct pjsip_pgp_challenge
{
    pj_str_t	realm;
    pj_str_t	version;
    pj_str_t	micalgorithm;
    pj_str_t	pubalgorithm;
    pj_str_t	nonce;
};

typedef struct pjsip_pgp_challenge pjsip_pgp_challenge;

/**
 * This structure describe SIP WWW-Authenticate header (Proxy-Authenticate
 * header also uses the same structure).
 */
struct pjsip_www_authenticate_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_www_authenticate_hdr);
    pj_str_t	scheme;
    union
    {
	pjsip_common_challenge	common;
	pjsip_digest_challenge	digest;
	pjsip_pgp_challenge	pgp;
    } challenge;
};

typedef struct pjsip_www_authenticate_hdr pjsip_www_authenticate_hdr;
typedef struct pjsip_www_authenticate_hdr pjsip_proxy_authenticate_hdr;


/**
 * Create SIP WWW-Authenticate header.
 * @param pool Pool where memory will be allocated from.
 * @return SIP WWW-Authenticate header.
 */
PJ_DECL(pjsip_www_authenticate_hdr*) pjsip_www_authenticate_hdr_create(pj_pool_t *pool);

/**
 * Create SIP Proxy-Authenticate header.
 * @param pool Pool where memory will be allocated from.
 * @return SIP Proxy-Authenticate header.
 */
PJ_DECL(pjsip_proxy_authenticate_hdr*) pjsip_proxy_authenticate_hdr_create(pj_pool_t *pool);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_AUTH_SIP_AUTH_MSG_H__ */

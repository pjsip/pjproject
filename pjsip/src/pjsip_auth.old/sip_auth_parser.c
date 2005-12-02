/* $Header: /pjproject/pjsip/src/pjsip/sip_auth_parser.c 7     8/31/05 9:05p Bennylp $ */
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
#include <pjsip/sip_auth_parser.h>
#include <pjsip/sip_auth_msg.h>
#include <pjsip/sip_parser.h>
#include <pj/string.h>
#include <pj/except.h>

static pjsip_authorization_hdr*	      parse_hdr_authorization( pj_scanner *scanner, pj_pool_t *pool);
static pjsip_proxy_authorization_hdr* parse_hdr_proxy_authorization( pj_scanner *scanner, pj_pool_t *pool);
static pjsip_www_authenticate_hdr*    parse_hdr_www_authenticate( pj_scanner *scanner, pj_pool_t *pool);
static pjsip_proxy_authenticate_hdr*  parse_hdr_proxy_authenticate( pj_scanner *scanner, pj_pool_t *pool);

static void parse_digest_credential( pj_scanner *scanner, pj_pool_t *pool, pjsip_digest_credential *cred);
static void parse_pgp_credential( pj_scanner *scanner, pj_pool_t *pool, pjsip_pgp_credential *cred);
static void parse_digest_challenge( pj_scanner *scanner, pj_pool_t *pool, pjsip_digest_challenge *chal);
static void parse_pgp_challenge( pj_scanner *scanner, pj_pool_t *pool, pjsip_pgp_challenge *chal);

const pj_str_t	pjsip_USERNAME_STR =	    { "username", 8 },
		pjsip_REALM_STR =	    { "realm", 5},
		pjsip_NONCE_STR =	    { "nonce", 5},
		pjsip_URI_STR =		    { "uri", 3 },
		pjsip_RESPONSE_STR =	    { "response", 8 },
		pjsip_ALGORITHM_STR =	    { "algorithm", 9 },
		pjsip_DOMAIN_STR =	    { "domain", 6 },
		pjsip_STALE_STR =	    { "stale", 5},
		pjsip_QOP_STR =		    { "qop", 3},
		pjsip_CNONCE_STR =	    { "cnonce", 6},
		pjsip_OPAQUE_STR =	    { "opaque", 6},
		pjsip_NC_STR =		    { "nc", 2},
		pjsip_TRUE_STR =	    { "true", 4},
		pjsip_QUOTED_TRUE_STR =	    { "\"true\"", 6},
		pjsip_FALSE_STR =	    { "false", 5},
		pjsip_QUOTED_FALSE_STR =    { "\"false\"", 7},
		pjsip_DIGEST_STR =	    { "Digest", 6},
		pjsip_QUOTED_DIGEST_STR =   { "\"Digest\"", 8},
		pjsip_PGP_STR =		    { "PGP", 3 },
		pjsip_QUOTED_PGP_STR =	    { "\"PGP\"", 5 },
		pjsip_MD5_STR =		    { "md5", 3 },
		pjsip_QUOTED_MD5_STR =	    { "\"md5\"", 5},
		pjsip_AUTH_STR =	    { "auth", 4},
		pjsip_QUOTED_AUTH_STR =	    { "\"auth\"", 6 };


static void parse_digest_credential( pj_scanner *scanner, pj_pool_t *pool, pjsip_digest_credential *cred)
{
    for (;;) {
	pj_str_t name, value;

	pjsip_parse_param_imp(scanner, &name, &value, PJSIP_PARSE_REMOVE_QUOTE);

	if (!pj_stricmp(&name, &pjsip_USERNAME_STR)) {
	    cred->username = value;

	} else if (!pj_stricmp(&name, &pjsip_REALM_STR)) {
	    cred->realm = value;

	} else if (!pj_stricmp(&name, &pjsip_NONCE_STR)) {
	    cred->nonce = value;

	} else if (!pj_stricmp(&name, &pjsip_URI_STR)) {
	    cred->uri = value;

	} else if (!pj_stricmp(&name, &pjsip_RESPONSE_STR)) {
	    cred->response = value;

	} else if (!pj_stricmp(&name, &pjsip_ALGORITHM_STR)) {
	    cred->algorithm = value;

	} else if (!pj_stricmp(&name, &pjsip_CNONCE_STR)) {
	    cred->cnonce = value;

	} else if (!pj_stricmp(&name, &pjsip_OPAQUE_STR)) {
	    cred->opaque = value;

	} else if (!pj_stricmp(&name, &pjsip_QOP_STR)) {
	    cred->qop = value;

	} else if (!pj_stricmp(&name, &pjsip_NC_STR)) {
	    cred->nc = value;

	} else {
	    pjsip_concat_param_imp(&cred->other_param, pool, &name, &value, ',');
	}

	/* Eat comma */
	if (!pj_scan_is_eof(scanner) && *scanner->current == ',')
	    pj_scan_get_char(scanner);
	else
	    break;
    }
}

static void parse_pgp_credential( pj_scanner *scanner, pj_pool_t *pool, pjsip_pgp_credential *cred)
{
    PJ_UNUSED_ARG(scanner)
    PJ_UNUSED_ARG(pool)
    PJ_UNUSED_ARG(cred)

    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
}

static void parse_digest_challenge( pj_scanner *scanner, pj_pool_t *pool, pjsip_digest_challenge *chal)
{
    for (;;) {
	pj_str_t name, value;

	pjsip_parse_param_imp(scanner, &name, &value, PJSIP_PARSE_REMOVE_QUOTE);

	if (!pj_stricmp(&name, &pjsip_REALM_STR)) {
	    chal->realm = value;

	} else if (!pj_stricmp(&name, &pjsip_DOMAIN_STR)) {
	    chal->domain = value;

	} else if (!pj_stricmp(&name, &pjsip_NONCE_STR)) {
	    chal->nonce = value;

	} else if (!pj_stricmp(&name, &pjsip_OPAQUE_STR)) {
	    chal->opaque = value;

	} else if (!pj_stricmp(&name, &pjsip_STALE_STR)) {
	    if (!pj_stricmp(&value, &pjsip_TRUE_STR) || !pj_stricmp(&value, &pjsip_QUOTED_TRUE_STR))
		chal->stale = 1;

	} else if (!pj_stricmp(&name, &pjsip_ALGORITHM_STR)) {
	    chal->algorithm = value;


	} else if (!pj_stricmp(&name, &pjsip_QOP_STR)) {
	    chal->qop = value;

	} else {
	    pjsip_concat_param_imp(&chal->other_param, pool, &name, &value, ',');
	}

	/* Eat comma */
	if (!pj_scan_is_eof(scanner) && *scanner->current == ',')
	    pj_scan_get_char(scanner);
	else
	    break;
    }
}

static void parse_pgp_challenge( pj_scanner *scanner, pj_pool_t *pool, pjsip_pgp_challenge *chal)
{
    PJ_UNUSED_ARG(scanner)
    PJ_UNUSED_ARG(pool)
    PJ_UNUSED_ARG(chal)

    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
}

static void int_parse_hdr_authorization( pj_scanner *scanner, pj_pool_t *pool, 
					 pjsip_authorization_hdr *hdr)
{
    if (*scanner->current == '"') {
	pj_scan_get_quote(scanner, '"', '"', &hdr->scheme);
	hdr->scheme.ptr++;
	hdr->scheme.slen -= 2;
    } else {
	pj_scan_get(scanner, pjsip_TOKEN_SPEC, &hdr->scheme);
    }

    if (!pj_stricmp(&hdr->scheme, &pjsip_DIGEST_STR)) {

	parse_digest_credential(scanner, pool, &hdr->credential.digest);

    } else if (!pj_stricmp(&hdr->scheme, &pjsip_PGP_STR)) {

	parse_pgp_credential( scanner, pool, &hdr->credential.pgp);

    } else {
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
    }

    pjsip_parse_end_hdr_imp( scanner );
}

static void int_parse_hdr_authenticate( pj_scanner *scanner, pj_pool_t *pool, 
					pjsip_www_authenticate_hdr *hdr)
{
    if (*scanner->current == '"') {
	pj_scan_get_quote(scanner, '"', '"', &hdr->scheme);
	hdr->scheme.ptr++;
	hdr->scheme.slen -= 2;
    } else {
	pj_scan_get(scanner, pjsip_TOKEN_SPEC, &hdr->scheme);
    }

    if (!pj_stricmp(&hdr->scheme, &pjsip_DIGEST_STR)) {

	parse_digest_challenge(scanner, pool, &hdr->challenge.digest);

    } else if (!pj_stricmp(&hdr->scheme, &pjsip_PGP_STR)) {

	parse_pgp_challenge(scanner, pool, &hdr->challenge.pgp);

    } else {
	PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
    }

    pjsip_parse_end_hdr_imp( scanner );
}


static pjsip_authorization_hdr *parse_hdr_authorization( pj_scanner *scanner, pj_pool_t *pool)
{
    pjsip_authorization_hdr *hdr = pjsip_authorization_hdr_create(pool);
    int_parse_hdr_authorization(scanner, pool, hdr);
    return hdr;
}

static pjsip_proxy_authorization_hdr *parse_hdr_proxy_authorization( pj_scanner *scanner, pj_pool_t *pool)
{
    pjsip_proxy_authorization_hdr *hdr = pjsip_proxy_authorization_hdr_create(pool);
    int_parse_hdr_authorization(scanner, pool, hdr);
    return hdr;
}

static pjsip_www_authenticate_hdr *parse_hdr_www_authenticate( pj_scanner *scanner, pj_pool_t *pool)
{
    pjsip_www_authenticate_hdr *hdr = pjsip_www_authenticate_hdr_create(pool);
    int_parse_hdr_authenticate(scanner, pool, hdr);
    return hdr;
}

static pjsip_proxy_authenticate_hdr *parse_hdr_proxy_authenticate( pj_scanner *scanner, pj_pool_t *pool)
{
    pjsip_proxy_authenticate_hdr *hdr = pjsip_proxy_authenticate_hdr_create(pool);
    int_parse_hdr_authenticate(scanner, pool, hdr);
    return hdr;
}


PJ_DEF(void) pjsip_auth_init_parser()
{
    pjsip_register_hdr_parser( "Authorization", NULL, (pjsip_parse_hdr_func*) &parse_hdr_authorization);
    pjsip_register_hdr_parser( "Proxy-Authorization", NULL, (pjsip_parse_hdr_func*) &parse_hdr_proxy_authorization);
    pjsip_register_hdr_parser( "WWW-Authenticate", NULL, (pjsip_parse_hdr_func*) &parse_hdr_www_authenticate);
    pjsip_register_hdr_parser( "Proxy-Authenticate", NULL, (pjsip_parse_hdr_func*) &parse_hdr_proxy_authenticate);
}

PJ_DEF(void) pjsip_auth_deinit_parser()
{
}


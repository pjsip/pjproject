/* $Header: /pjproject/pjsip/src/pjsip/sip_auth.c 14    12/02/05 9:05p Bennylp $ */
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
#include <pjsip/sip_auth.h>
#include <pjsip/sip_auth_parser.h>	/* just to get pjsip_DIGEST_STR */
#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pj/md5.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/guid.h>

/* Length of digest string. */
#define MD5STRLEN 32

/* Maximum stack size we use for storing username+realm+password etc. */
#define MAX_TEMP  128

/* A macro just to get rid of type mismatch between char and unsigned char */
#define MD5_APPEND(pms,buf,len)	md5_append(pms, (const unsigned char*)buf, len)

/* Logging. */
#define THIS_FILE   "sip_auth.c"
#if 0
#  define AUTH_TRACE_(expr)  PJ_LOG(3, expr)
#else
#  define AUTH_TRACE_(expr)
#endif

static const char hex[] = "0123456789abcdef";

/* Transform digest to string.
 * output must be at least MD5STRLEN+1 bytes.
 *
 * NOTE: THE OUTPUT STRING IS NOT NULL TERMINATED!
 */
static void digest2str(const unsigned char digest[], char *output)
{
    char *p = output;
    int i;

    for (i = 0; i<16; ++i) {
	int val = digest[i];
	*p++ = hex[val >> 4];
	*p++ = hex[val & 0x0F];
    }
}

/*
 * Create response digest based on the parameters and store the
 * digest ASCII in 'result'. 
 */
static void create_digest( pj_str_t *result,
			   const pj_str_t *nonce,
			   const pj_str_t *nc,
			   const pj_str_t *cnonce,
			   const pj_str_t *qop,
			   const pj_str_t *uri,
			   const pjsip_cred_info *cred_info,
			   const pj_str_t *method)
{
    char ha1[MD5STRLEN];
    char ha2[MD5STRLEN];
    unsigned char digest[16];
    md5_state_t pms;

    pj_assert(result->slen >= MD5STRLEN);

    AUTH_TRACE_((THIS_FILE, "Begin creating digest"));

    if (cred_info->data_type == PJSIP_CRED_DATA_PLAIN_PASSWD) {
	/*** 
	 *** ha1 = MD5(username ":" realm ":" password) 
	 ***/
	md5_init(&pms);
	MD5_APPEND( &pms, cred_info->username.ptr, cred_info->username.slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, cred_info->realm.ptr, cred_info->realm.slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, cred_info->data.ptr, cred_info->data.slen);
	md5_finish(&pms, digest);

	digest2str(digest, ha1);

    } else if (cred_info->data_type == PJSIP_CRED_DATA_DIGEST) {
	pj_assert(cred_info->data.slen == 32);
	pj_memcpy( ha1, cred_info->data.ptr, cred_info->data.slen );
    }

    AUTH_TRACE_((THIS_FILE, "  ha1=%.32s", ha1));

    /***
     *** ha2 = MD5(method ":" req_uri) 
     ***/
    md5_init(&pms);
    MD5_APPEND( &pms, method->ptr, method->slen);
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, uri->ptr, uri->slen);
    md5_finish(&pms, digest);
    digest2str(digest, ha2);

    AUTH_TRACE_((THIS_FILE, "  ha2=%.32s", ha2));

    /***
     *** When qop is not used:
     ***    response = MD5(ha1 ":" nonce ":" ha2) 
     ***
     *** When qop=auth is used:
     ***    response = MD5(ha1 ":" nonce ":" nc ":" cnonce ":" qop ":" ha2)
     ***/
    md5_init(&pms);
    MD5_APPEND( &pms, ha1, MD5STRLEN);
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, nonce->ptr, nonce->slen);
    if (qop && qop->slen != 0) {
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, nc->ptr, nc->slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, cnonce->ptr, cnonce->slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, qop->ptr, qop->slen);
    }
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, ha2, MD5STRLEN);

    /* This is the final response digest. */
    md5_finish(&pms, digest);
    
    /* Convert digest to string and store in chal->response. */
    result->slen = MD5STRLEN;
    digest2str(digest, result->ptr);

    AUTH_TRACE_((THIS_FILE, "  digest=%.32s", result->ptr));
    AUTH_TRACE_((THIS_FILE, "Digest created"));
}

/*
 * Finds out if qop offer contains "auth" token.
 */
static pj_bool_t has_auth_qop( pj_pool_t *pool, const pj_str_t *qop_offer)
{
    pj_str_t qop;
    char *p;

    pj_strdup_with_null( pool, &qop, qop_offer);
    p = qop.ptr;
    while (*p) {
	*p = (char)tolower(*p);
	++p;
    }

    p = qop.ptr;
    while (*p) {
	if (*p=='a' && *(p+1)=='u' && *(p+2)=='t' && *(p+3)=='h') {
	    int e = *(p+4);
	    if (e=='"' || e==',' || e==0)
		return PJ_TRUE;
	    else
		p += 4;
	} else {
	    ++p;
	}
    }

    return PJ_FALSE;
}

/*
 * Generate response digest. 
 * Most of the parameters to generate the digest (i.e. username, realm, uri,
 * and nonce) are expected to be in the credential. Additional parameters (i.e.
 * password and method param) should be supplied in the argument.
 *
 * The resulting digest will be stored in cred->response.
 * The pool is used to allocate 32 bytes to store the digest in cred->response.
 */
static pj_status_t respond_digest( pj_pool_t *pool,
				   pjsip_digest_credential *cred,
				   const pjsip_digest_challenge *chal,
				   const pj_str_t *uri,
				   const pjsip_cred_info *cred_info,
				   const pj_str_t *cnonce,
				   pj_uint32_t nc,
				   const pj_str_t *method)
{
    /* Check algorithm is supported. We only support MD5. */
    if (chal->algorithm.slen && pj_stricmp(&chal->algorithm, &pjsip_MD5_STR))
    {
	PJ_LOG(4,(THIS_FILE, "Unsupported digest algorithm \"%.*s\"",
		  chal->algorithm.slen, chal->algorithm.ptr));
	return -1;
    }

    /* Build digest credential from arguments. */
    pj_strdup(pool, &cred->username, &cred_info->username);
    pj_strdup(pool, &cred->realm, &chal->realm);
    pj_strdup(pool, &cred->nonce, &chal->nonce);
    pj_strdup(pool, &cred->uri, uri);
    cred->algorithm = pjsip_MD5_STR;
    pj_strdup(pool, &cred->opaque, &chal->opaque);
    
    /* Allocate memory. */
    cred->response.ptr = pj_pool_alloc(pool, MD5STRLEN);
    cred->response.slen = MD5STRLEN;

    if (chal->qop.slen == 0) {
	/* Server doesn't require quality of protection. */

	/* Convert digest to string and store in chal->response. */
	create_digest( &cred->response, &cred->nonce, NULL, NULL, NULL,
		       uri, cred_info, method);

    } else if (has_auth_qop(pool, &chal->qop)) {
	/* Server requires quality of protection. 
	 * We respond with selecting "qop=auth" protection.
	 */
	cred->qop = pjsip_AUTH_STR;
	cred->nc.ptr = pj_pool_alloc(pool, 16);
	sprintf(cred->nc.ptr, "%06u", nc);

	if (cnonce && cnonce->slen) {
	    pj_strdup(pool, &cred->cnonce, cnonce);
	} else {
	    pj_str_t dummy_cnonce = { "b39971", 6};
	    pj_strdup(pool, &cred->cnonce, &dummy_cnonce);
	}

	create_digest( &cred->response, &cred->nonce, &cred->nc, cnonce, 
		       &pjsip_AUTH_STR, uri, cred_info, method );

    } else {
	/* Server requires quality protection that we don't support. */
	PJ_LOG(4,(THIS_FILE, "Unsupported qop offer %.*s", 
		  chal->qop.slen, chal->qop.ptr));
	return -1;
    }

    return 0;
}

#if PJSIP_AUTH_QOP_SUPPORT
/*
 * Update authentication session with a challenge.
 */
static void update_digest_session( pj_pool_t *ses_pool, 
				   pjsip_auth_session *auth_sess,
				   const pjsip_www_authenticate_hdr *hdr )
{
    if (hdr->challenge.digest.qop.slen == 0)
	return;

    /* Initialize cnonce and qop if not present. */
    if (auth_sess->cnonce.slen == 0) {
	/* Save the whole challenge */
	auth_sess->last_chal = pjsip_hdr_clone(ses_pool, hdr);

	/* Create cnonce */
	pj_create_unique_string( ses_pool, &auth_sess->cnonce );

	/* Initialize nonce-count */
	auth_sess->nc = 1;

	/* Save realm. */
	pj_assert(auth_sess->realm.slen != 0);
	if (auth_sess->realm.slen == 0) {
	    pj_strdup(ses_pool, &auth_sess->realm, 
		      &hdr->challenge.digest.realm);
	}

    } else {
	/* Update last_nonce and nonce-count */
	if (!pj_strcmp(&hdr->challenge.digest.nonce, 
		       &auth_sess->last_chal->challenge.digest.nonce)) 
	{
	    /* Same nonce, increment nonce-count */
	    ++auth_sess->nc;
	} else {
	    /* Server gives new nonce. */
	    pj_strdup(ses_pool, &auth_sess->last_chal->challenge.digest.nonce,
		      &hdr->challenge.digest.nonce);
	    /* Has the opaque changed? */
	    if (pj_strcmp(&auth_sess->last_chal->challenge.digest.opaque,
			  &hdr->challenge.digest.opaque)) 
	    {
		pj_strdup(ses_pool, 
			  &auth_sess->last_chal->challenge.digest.opaque,
			  &hdr->challenge.digest.opaque);
	    }
	    auth_sess->nc = 1;
	}
    }
}
#endif	/* PJSIP_AUTH_QOP_SUPPORT */


/* Find authentication session in the list. */
static pjsip_auth_session *find_session( pjsip_auth_session *sess_list,
					 const pj_str_t *realm )
{
    pjsip_auth_session *sess = sess_list->next;
    while (sess != sess_list) {
	if (pj_stricmp(&sess->realm, realm) == 0)
	    return sess;
	sess = sess->next;
    }

    return NULL;
}

/* 
 * Create Authorization/Proxy-Authorization response header based on the challege
 * in WWW-Authenticate/Proxy-Authenticate header.
 */
PJ_DEF(pjsip_authorization_hdr*)
pjsip_auth_respond( pj_pool_t *req_pool,
		    const pjsip_www_authenticate_hdr *hdr,
		    const pjsip_uri *uri,
		    const pjsip_cred_info *cred_info,
		    const pjsip_method *method,
		    pj_pool_t *sess_pool,
		    pjsip_auth_session *auth_sess)
{
    pjsip_authorization_hdr *auth;
    char tmp[PJSIP_MAX_URL_SIZE];
    pj_str_t uri_str;
    pj_pool_t *pool;

    pj_assert(hdr != NULL);
    pj_assert(uri != NULL);
    pj_assert(cred_info != NULL);
    pj_assert(method != NULL);

    /* Print URL in the original request. */
    uri_str.ptr = tmp;
    uri_str.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, uri, tmp, sizeof(tmp));
    if (uri_str.slen < 1) {
	pj_assert(!"URL is too long!");
	PJ_LOG(4,(THIS_FILE, "Unable to authorize: URI is too long!"));
	return NULL;
    }

#   if (PJSIP_AUTH_HEADER_CACHING)
    {
	pool = sess_pool;
	PJ_UNUSED_ARG(req_pool);
    }
#   else
    {
	pool = req_pool;
	PJ_UNUSED_ARG(sess_pool);
    }
#   endif

    if (hdr->type == PJSIP_H_WWW_AUTHENTICATE)
	auth = pjsip_authorization_hdr_create(pool);
    else if (hdr->type == PJSIP_H_PROXY_AUTHENTICATE)
	auth = pjsip_proxy_authorization_hdr_create(pool);
    else {
	pj_assert(0);
	return NULL;
    }

    /* Only support digest scheme at the moment. */
    if (!pj_stricmp(&hdr->scheme, &pjsip_DIGEST_STR)) {
	pj_status_t rc;
	pj_str_t *cnonce = NULL;
	pj_uint32_t nc = 1;

	/* Update the session (nonce-count etc) if required. */
#	if PJSIP_AUTH_QOP_SUPPORT
	{
	    if (auth_sess) {
		update_digest_session( sess_pool, auth_sess, hdr );

		cnonce = &auth_sess->cnonce;
		nc = auth_sess->nc;
	    }
	}
#	endif	/* PJSIP_AUTH_QOP_SUPPORT */

	auth->scheme = pjsip_DIGEST_STR;
	rc = respond_digest( pool, &auth->credential.digest,
			     &hdr->challenge.digest, &uri_str, cred_info,
			     cnonce, nc, &method->name);
	if (rc != 0)
	    return NULL;

	/* Set qop type in auth session the first time only. */
	if (hdr->challenge.digest.qop.slen != 0 && auth_sess) {
	    if (auth_sess->qop_value == PJSIP_AUTH_QOP_NONE) {
		pj_str_t *qop_val = &auth->credential.digest.qop;
		if (!pj_strcmp(qop_val, &pjsip_AUTH_STR)) {
		    auth_sess->qop_value = PJSIP_AUTH_QOP_AUTH;
		} else {
		    auth_sess->qop_value = PJSIP_AUTH_QOP_UNKNOWN;
		}
	    }
	}
    } else {
	auth = NULL;
    }

    /* Keep the new authorization header in the cache, only
     * if no qop is not present.
     */
#   if PJSIP_AUTH_HEADER_CACHING
    {
	if (auth && auth_sess && auth_sess->qop_value == PJSIP_AUTH_QOP_NONE) {
	    pjsip_cached_auth_hdr *cached_hdr;

	    /* Delete old header with the same method. */
	    cached_hdr = auth_sess->cached_hdr.next;
	    while (cached_hdr != &auth_sess->cached_hdr) {
		if (pjsip_method_cmp(method, &cached_hdr->method)==0)
		    break;
		cached_hdr = cached_hdr->next;
	    }

	    /* Save the header to the list. */
	    if (cached_hdr != &auth_sess->cached_hdr) {
		cached_hdr->hdr = auth;
	    } else {
		cached_hdr = pj_pool_alloc(pool, sizeof(*cached_hdr));
		pjsip_method_copy( pool, &cached_hdr->method, method);
		cached_hdr->hdr = auth;
		pj_list_insert_before( &auth_sess->cached_hdr, cached_hdr );
	    }
	}
    }
#   endif

    return auth;

}

/* Verify incoming Authorization/Proxy-Authorization header against existing
 * credentials. Will return TRUE if the authorization request matches any of
 * the credential.
 */
PJ_DEF(pj_bool_t) pjsip_auth_verify(const pjsip_authorization_hdr *hdr,
				    const pj_str_t *method,
				    const pjsip_cred_info *cred_info )
{
    if (pj_stricmp(&hdr->scheme, &pjsip_DIGEST_STR) == 0) {
	char digest_buf[MD5STRLEN];
	pj_str_t digest;
	const pjsip_digest_credential *dig = &hdr->credential.digest;

	/* Check that username match. */
	if (pj_strcmp(&dig->username, &cred_info->username) != 0)
	    return PJ_FALSE;

	/* Check that realm match. */
	if (pj_strcmp(&dig->realm, &cred_info->realm) != 0)
	    return PJ_FALSE;

	/* Prepare for our digest calculation. */
	digest.ptr = digest_buf;
	digest.slen = MD5STRLEN;

	/* Create digest for comparison. */
	create_digest(  &digest, 
			&hdr->credential.digest.nonce,
			&hdr->credential.digest.nc, 
			&hdr->credential.digest.cnonce,
			&hdr->credential.digest.qop,
			&hdr->credential.digest.uri,
			cred_info, 
			method );

	return pj_stricmp(&digest, &hdr->credential.digest.response) == 0;

    } else {
	pj_assert(0);
	return PJ_FALSE;
    }
}

/* Find credential to use for the specified realm and scheme. */
PJ_DEF(const pjsip_cred_info*) pjsip_auth_find_cred( unsigned count,
						     const pjsip_cred_info cred[],
						     const pj_str_t *realm,
						     const pj_str_t *scheme)
{
    unsigned i;
    PJ_UNUSED_ARG(scheme)
    for (i=0; i<count; ++i) {
	if (pj_stricmp(&cred[i].realm, realm) == 0)
	    return &cred[i];
    }
    return NULL;
}

#if PJSIP_AUTH_AUTO_SEND_NEXT
static void new_auth_for_req( pjsip_tx_data *tdata,
			      pj_pool_t *sess_pool,
			      pjsip_auth_session *sess,
			      int cred_count,
			      const pjsip_cred_info cred_info[])
{
    const pjsip_cred_info *cred;
    pjsip_authorization_hdr *hauth;

    if (sess->last_chal == NULL)
	return;

    cred = pjsip_auth_find_cred( cred_count, cred_info, &sess->realm,
				 &sess->last_chal->scheme );
    if (!cred)
	return;

    
    hauth = pjsip_auth_respond( tdata->pool, sess->last_chal,
				tdata->msg->line.req.uri,
				cred, &tdata->msg->line.req.method,
				sess_pool, sess);
    if (hauth) {
	pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)hauth);
    }
}
#endif

/* 
 * Initialize new request message with authorization headers.
 * This function will put Authorization/Proxy-Authorization headers to the
 * outgoing request message. If caching is enabled (PJSIP_AUTH_HEADER_CACHING)
 * and the session has previously sent Authorization/Proxy-Authorization header
 * with the same method, then the same Authorization/Proxy-Authorization header
 * will be resent from the cache only if qop is not present. If the stack is 
 * configured to automatically generate next Authorization/Proxy-Authorization
 * headers (PJSIP_AUTH_AUTO_SEND_NEXT flag), then new Authorization/Proxy-
 * Authorization headers are calculated and generated when they are not present
 * in the case or if authorization session has qop.
 *
 * If both PJSIP_AUTH_HEADER_CACHING flag and PJSIP_AUTH_AUTO_SEND_NEXT flag
 * are not set, this function will do nothing. The stack then will only send
 * Authorization/Proxy-Authorization to respond 401/407 response.
 */
PJ_DEF(pj_status_t) pjsip_auth_init_req( pj_pool_t *sess_pool,
					 pjsip_tx_data *tdata,
					 pjsip_auth_session *sess_list,
					 int cred_count, 
					 const pjsip_cred_info cred_info[])
{
    pjsip_auth_session *sess;
    pjsip_method *method = &tdata->msg->line.req.method;

    pj_assert(tdata->msg->type == PJSIP_REQUEST_MSG);

    if (!sess_list)
	return 0;

    sess = sess_list->next;
    while (sess != sess_list) {
	if (sess->qop_value == PJSIP_AUTH_QOP_NONE) {
#	    if (PJSIP_AUTH_HEADER_CACHING)
	    {
		pjsip_cached_auth_hdr *entry = sess->cached_hdr.next;
		while (entry != &sess->cached_hdr) {
		    if (pjsip_method_cmp(&entry->method, method)==0) {
			pjsip_authorization_hdr *hauth;
			hauth = pjsip_hdr_shallow_clone(tdata->pool, entry->hdr);
			pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hauth);
		    } else {
#			if (PJSIP_AUTH_AUTO_SEND_NEXT)
			{
			    new_auth_for_req( tdata, sess_pool, sess, 
					      cred_count, cred_info);
			}
#			else
			{
			    PJ_UNUSED_ARG(sess_pool);
			    PJ_UNUSED_ARG(cred_count);
			    PJ_UNUSED_ARG(cred_info);
			}
#			endif	/* PJSIP_AUTH_AUTO_SEND_NEXT */
		    }
		    entry = entry->next;
		}
	    }
#	    elif (PJSIP_AUTH_AUTO_SEND_NEXT)
	    {
		new_auth_for_req( tdata, sess_pool, sess, 
				  cred_count, cred_info);
	    }
#	    else
	    {
		PJ_UNUSED_ARG(sess_pool);
		PJ_UNUSED_ARG(cred_count);
		PJ_UNUSED_ARG(cred_info);
	    }
#	    endif   /* PJSIP_AUTH_HEADER_CACHING */

	} 
#	if (PJSIP_AUTH_QOP_SUPPORT && PJSIP_AUTH_AUTO_SEND_NEXT)
	else if (sess->qop_value == PJSIP_AUTH_QOP_AUTH) {
	    /* For qop="auth", we have to re-create the authorization header. 
	     */
	    const pjsip_cred_info *cred;
	    pjsip_authorization_hdr *hauth;

	    cred = pjsip_auth_find_cred( cred_count, cred_info, 
					 &sess->realm, 
					 &sess->last_chal->scheme);
	    if (!cred) {
		sess = sess->next;
		continue;
	    }

	    hauth = pjsip_auth_respond( tdata->pool, sess->last_chal, 
					tdata->msg->line.req.uri, 
					cred,
					&tdata->msg->line.req.method,
					sess_pool, sess );
	    if (hauth) {
		pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hauth);
	    }
	}
#	endif	/* PJSIP_AUTH_QOP_SUPPORT && PJSIP_AUTH_AUTO_SEND_NEXT */

	sess = sess->next;
    }
    return 0;
}

/* Process authorization challenge */
static pjsip_authorization_hdr *process_auth( pj_pool_t *req_pool,
					      const pjsip_www_authenticate_hdr *hchal,
					      const pjsip_uri *uri,
					      pjsip_tx_data *tdata,
					      int cred_count,
					      const pjsip_cred_info cred_info[],
					      pj_pool_t *ses_pool,
					      pjsip_auth_session *auth_sess)
{
    const pjsip_cred_info *cred;
    pjsip_authorization_hdr *sent_auth = NULL, *hauth;
    pjsip_hdr *hdr;

    /* See if we have sent authorization header for this realm */
    hdr = tdata->msg->hdr.next;
    while (hdr != &tdata->msg->hdr) {
	if ((hchal->type == PJSIP_H_WWW_AUTHENTICATE &&
	     hdr->type == PJSIP_H_AUTHORIZATION) ||
	    (hchal->type == PJSIP_H_PROXY_AUTHENTICATE &&
	     hdr->type == PJSIP_H_PROXY_AUTHORIZATION))
	{
	    sent_auth = (pjsip_authorization_hdr*) hdr;
	    if (pj_stricmp(&hchal->challenge.common.realm, 
			   &sent_auth->credential.common.realm )==0)
	    {
		break;
	    }
	}
	hdr = hdr->next;
    }

    /* If we have sent, see if server rejected because of stale nonce or
     * other causes.
     */
    if (hdr != &tdata->msg->hdr) {
	if (hchal->challenge.digest.stale == 0) {
	    /* Our credential is rejected. No point in trying to re-supply
	     * the same credential.
	     */
	    PJ_LOG(4, (THIS_FILE, "Authorization failed for %.*s@%.*s",
		       sent_auth->credential.digest.username.slen,
		       sent_auth->credential.digest.username.ptr,
		       sent_auth->credential.digest.realm.slen,
		       sent_auth->credential.digest.realm.ptr));
	    return NULL;
	}

	/* Otherwise remove old, stale authorization header from the mesasge.
	 * We will supply a new one.
	 */
	pj_list_erase(sent_auth);
    }

    /* Find credential to be used for the challenge. */
    cred = pjsip_auth_find_cred( cred_count, cred_info, 
				 &hchal->challenge.common.realm, &hchal->scheme);
    if (!cred) {
	const pj_str_t *realm = &hchal->challenge.common.realm;
	PJ_LOG(4,(THIS_FILE, 
		  "Unable to set auth for %s: can not find credential for %.*s/%.*s",
		  tdata->obj_name, 
		  realm->slen, realm->ptr,
		  hchal->scheme.slen, hchal->scheme.ptr));
	return NULL;
    }

    /* Respond to authorization challenge. */
    hauth = pjsip_auth_respond( req_pool, hchal, uri, cred, 
				&tdata->msg->line.req.method, 
				ses_pool, auth_sess);
    return hauth;
}


/* Reinitialize outgoing request after 401/407 response is received.
 * The purpose of this function is:
 *  - to add a Authorization/Proxy-Authorization header.
 *  - to put the newly created Authorization/Proxy-Authorization header
 *    in cached_list.
 */
PJ_DEF(pjsip_tx_data*) pjsip_auth_reinit_req( pjsip_endpoint *endpt, 
					      pj_pool_t *ses_pool, 
					      pjsip_auth_session *sess_list,
					      int cred_count, 
					      const pjsip_cred_info cred_info[],
					      pjsip_tx_data *tdata, 
					      const pjsip_rx_data *rdata)
{
    const pjsip_hdr *hdr;
    pjsip_via_hdr *via;

    PJ_UNUSED_ARG(endpt)

    pj_assert(rdata->msg->type == PJSIP_RESPONSE_MSG);
    pj_assert(rdata->msg->line.status.code == 401 ||
	      rdata->msg->line.status.code == 407 );

    /*
     * Respond to each authentication challenge.
     */
    hdr = rdata->msg->hdr.next;
    while (hdr != &rdata->msg->hdr) {
	pjsip_auth_session *sess;
	const pjsip_www_authenticate_hdr *hchal;
	pjsip_authorization_hdr *hauth;

	/* Find WWW-Authenticate or Proxy-Authenticate header. */
	while (hdr->type != PJSIP_H_WWW_AUTHENTICATE &&
	       hdr->type != PJSIP_H_PROXY_AUTHENTICATE &&
	       hdr != &rdata->msg->hdr)
	{
	    hdr = hdr->next;
	}
	if (hdr == &rdata->msg->hdr)
	    break;

	hchal = (const pjsip_www_authenticate_hdr*) hdr;

	/* Find authentication session for this realm, create a new one
	 * if not present.
	 */
	sess = find_session(sess_list, &hchal->challenge.common.realm );
	if (!sess) {
	    sess = pj_pool_calloc( ses_pool, 1, sizeof(*sess));
	    pj_strdup( ses_pool, &sess->realm, &hchal->challenge.common.realm);
	    sess->is_proxy = (hchal->type == PJSIP_H_PROXY_AUTHENTICATE);
#	    if (PJSIP_AUTH_HEADER_CACHING)
	    {
		pj_list_init(&sess->cached_hdr);
	    }
#	    endif
	    pj_list_insert_before( sess_list, sess );
	}

	/* Create authorization header for this challenge, and update
	 * authorization session.
	 */
	hauth = process_auth( tdata->pool, hchal, tdata->msg->line.req.uri, 
			      tdata, cred_count, cred_info, ses_pool, sess );
	if (!hauth)
	    return NULL;

	/* Add to the message. */
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hauth);

	/* Process next header. */
	hdr = hdr->next;
    }


    /* Remove branch param in Via header. */
    via = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    if (via)
	via->branch_param.slen = 0;

    /* Increment reference counter. */
    pjsip_tx_data_add_ref(tdata);

    /* Done. */
    return tdata;
}


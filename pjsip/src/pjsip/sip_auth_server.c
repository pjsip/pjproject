/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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

#include <pjsip/sip_auth.h>
#include <pjsip/sip_auth_parser.h>      /* just to get pjsip_DIGEST_STR */
#include <pjsip/sip_auth_msg.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_transport.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/assert.h>

/* Logging. */
#define THIS_FILE   "sip_auth_server.c"
#if 0
#  define AUTH_TRACE_(expr)  PJ_LOG(3, expr)
#else
#  define AUTH_TRACE_(expr)
#endif


/*
 * Initialize server authorization session data structure to serve the 
 * specified realm and to use lookup_func function to look for the credential 
 * info. 
 */
PJ_DEF(pj_status_t) pjsip_auth_srv_init(  pj_pool_t *pool,
                                          pjsip_auth_srv *auth_srv,
                                          const pj_str_t *realm,
                                          pjsip_auth_lookup_cred *lookup,
                                          unsigned options )
{
    PJ_ASSERT_RETURN(pool && auth_srv && realm && lookup, PJ_EINVAL);

    pj_bzero(auth_srv, sizeof(*auth_srv));
    pj_strdup( pool, &auth_srv->realm, realm);
    auth_srv->lookup = lookup;
    auth_srv->is_proxy = (options & PJSIP_AUTH_SRV_IS_PROXY);

    return PJ_SUCCESS;
}

/*
 * Initialize server authorization session data structure to serve the 
 * specified realm and to use lookup_func function to look for the credential 
 * info. 
 */
PJ_DEF(pj_status_t) pjsip_auth_srv_init2(
                                    pj_pool_t *pool,
                                    pjsip_auth_srv *auth_srv,
                                    const pjsip_auth_srv_init_param *param)
{
    PJ_ASSERT_RETURN(pool && auth_srv && param, PJ_EINVAL);

    pj_bzero(auth_srv, sizeof(*auth_srv));
    pj_strdup( pool, &auth_srv->realm, param->realm);
    auth_srv->lookup2 = param->lookup2;
    auth_srv->is_proxy = (param->options & PJSIP_AUTH_SRV_IS_PROXY);

    return PJ_SUCCESS;
}


/* Verify incoming Authorization/Proxy-Authorization header against the 
 * specified credential.
 */
static pj_status_t pjsip_auth_verify( const pjsip_authorization_hdr *hdr,
                                      const pj_str_t *method,
                                      const pjsip_cred_info *cred_info )
{

    if (pj_stricmp(&hdr->scheme, &pjsip_DIGEST_STR) == 0) {
        const pjsip_digest_credential *dig = &hdr->credential.digest;
        const pjsip_auth_algorithm *response_algorithm =
                pjsip_auth_get_algorithm_by_iana_name(&dig->algorithm);
        char digest_buf[PJSIP_AUTH_MAX_DIGEST_BUFFER_LENGTH * 2];
        pj_str_t digest;
        pj_status_t status;
        int result = 0;

        AUTH_TRACE_((THIS_FILE, "Authenticating with realm-digest %.*s-%.*s",
                (int)dig->realm.slen, dig->realm.ptr,
                (int)dig->algorithm.slen, dig->algorithm.ptr));
        /*
         * Check that the response algorithm is supported.
         * Since we sent the challenge it should be but
         * we check anyway in case a client responds with a bad
         * one.
         */
        PJ_ASSERT_RETURN(response_algorithm
                && pjsip_auth_is_algorithm_supported(response_algorithm->algorithm_type),
                PJSIP_EINVALIDALGORITHM);

        /*
         * Cred info type must be plain password or digest and if it's digest,
         * the algorithm must match the algorithm in the header.
         * We don't support the AKAv1-MD5 algorithm as a server.
         */
        PJ_ASSERT_RETURN(PJSIP_CRED_DATA_IS_PASSWD(cred_info)
                || (PJSIP_CRED_DATA_IS_DIGEST(cred_info)
                && cred_info->algorithm_type == response_algorithm->algorithm_type),
                PJSIP_EINVALIDALGORITHM);

        /* Check that username and realm match. 
         * These checks should have been performed before entering this
         * function.
         */
        PJ_ASSERT_RETURN(pj_strcmp(&dig->username, &cred_info->username) == 0,
                         PJ_EINVALIDOP);
        PJ_ASSERT_RETURN(pj_strcmp(&dig->realm, &cred_info->realm) == 0,
                         PJ_EINVALIDOP);

        /* Prepare for our digest calculation. */
        digest.ptr = digest_buf;
        digest.slen = response_algorithm->digest_str_length;

        /* Create digest for comparison. */
        status = pjsip_auth_create_digest2(&digest,
                                 &hdr->credential.digest.nonce,
                                 &hdr->credential.digest.nc, 
                                 &hdr->credential.digest.cnonce,
                                 &hdr->credential.digest.qop,
                                 &hdr->credential.digest.uri,
                                 &cred_info->realm,
                                 cred_info, 
                                 method, response_algorithm->algorithm_type);

        AUTH_TRACE_((THIS_FILE, "Create digest response: %s",
                (status == PJ_SUCCESS ? "success" : "failed")));
        if (status != PJ_SUCCESS)
            return status;

        /* Compare digest. */
        result = pj_stricmp(&digest, &hdr->credential.digest.response);
        AUTH_TRACE_((THIS_FILE, "Digest comparison: %s",
                result ? "failed" : "matched"));

        return (result == 0) ?
               PJ_SUCCESS : PJSIP_EAUTHINVALIDDIGEST;

    } else {
        pj_assert(!"Unsupported authentication scheme");
        return PJSIP_EINVALIDAUTHSCHEME;
    }
}


/*
 * Request the authorization server framework to verify the authorization 
 * information in the specified request in rdata.
 */
PJ_DEF(pj_status_t) pjsip_auth_srv_verify( pjsip_auth_srv *auth_srv,
                                           pjsip_rx_data *rdata,
                                           int *status_code)
{
    pjsip_authorization_hdr *h_auth;
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_hdr_e htype;
    pj_str_t acc_name;
    pjsip_cred_info cred_info;
    pj_status_t status;
    const pjsip_auth_algorithm *algorithm;


    PJ_ASSERT_RETURN(auth_srv && rdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(msg->type == PJSIP_REQUEST_MSG, PJSIP_ENOTREQUESTMSG);

    htype = auth_srv->is_proxy ? PJSIP_H_PROXY_AUTHORIZATION : 
                                 PJSIP_H_AUTHORIZATION;

    /* Initialize status with 200. */
    *status_code = 200;

    /* Find authorization header for our realm. */
    h_auth = (pjsip_authorization_hdr*) pjsip_msg_find_hdr(msg, htype, NULL);
    while (h_auth) {
        if (!pj_stricmp(&h_auth->credential.common.realm, &auth_srv->realm))
            break;

        h_auth = h_auth->next;
        if (h_auth == (void*) &msg->hdr) {
            h_auth = NULL;
            break;
        }

        h_auth=(pjsip_authorization_hdr*)pjsip_msg_find_hdr(msg,htype,h_auth);
    }

    if (!h_auth) {
        *status_code = auth_srv->is_proxy ? 407 : 401;
        return PJSIP_EAUTHNOAUTH;
    }

    /* Check authorization scheme. */
    if (pj_stricmp(&h_auth->scheme, &pjsip_DIGEST_STR) == 0)
        acc_name = h_auth->credential.digest.username;
    else {
        *status_code = auth_srv->is_proxy ? 407 : 401;
        return PJSIP_EINVALIDAUTHSCHEME;
    }

    algorithm = pjsip_auth_get_algorithm_by_iana_name(
            &h_auth->credential.digest.algorithm);

    /* Check that algorithm is supported. */
    if (!algorithm || !pjsip_auth_is_algorithm_supported(algorithm->algorithm_type)) {
        return PJSIP_EINVALIDALGORITHM;
    }

    /* Find the credential information for the account. */
    pj_bzero(&cred_info, sizeof(cred_info));

    if (auth_srv->lookup2) {
        pjsip_auth_lookup_cred_param param;

        pj_bzero(&param, sizeof(param));
        param.realm = auth_srv->realm;
        param.acc_name = acc_name;
        param.rdata = rdata;
        param.auth_hdr = h_auth;
        status = (*auth_srv->lookup2)(rdata->tp_info.pool, &param, &cred_info);
        if (status != PJ_SUCCESS) {
            *status_code = PJSIP_SC_FORBIDDEN;
            return status;
        }
    } else {
        status = (*auth_srv->lookup)(rdata->tp_info.pool, &auth_srv->realm,
                                     &acc_name, &cred_info);
        if (status != PJ_SUCCESS) {
            *status_code = PJSIP_SC_FORBIDDEN;
            return status;
        }
    }

    /*
     * If the data type is DIGEST and an auth algorithm isn't set,
     * default it to MD5.
     */
    if (PJSIP_CRED_DATA_IS_DIGEST(&cred_info) &&
        cred_info.algorithm_type == PJSIP_AUTH_ALGORITHM_NOT_SET) {
        cred_info.algorithm_type = PJSIP_AUTH_ALGORITHM_MD5;
    }

    /* Authenticate with the specified credential. */
    status = pjsip_auth_verify(h_auth, &msg->line.req.method.name, 
                               &cred_info);
    if (status != PJ_SUCCESS) {
        *status_code = PJSIP_SC_FORBIDDEN;
    }
    return status;
}


/*
 * Add authentication challenge headers to the outgoing response in tdata. 
 * Application may specify its customized nonce and opaque for the challenge, 
 * or can leave the value to NULL to make the function fills them in with 
 * random characters.
 */
PJ_DEF(pj_status_t) pjsip_auth_srv_challenge2(  pjsip_auth_srv *auth_srv,
                                               const pj_str_t *qop,
                                               const pj_str_t *nonce,
                                               const pj_str_t *opaque,
                                               pj_bool_t stale,
                                               pjsip_tx_data *tdata,
                                               const pjsip_auth_algorithm_type algorithm_type)
{
    pjsip_www_authenticate_hdr *hdr;
    char nonce_buf[16];
    pj_str_t random;
    const pjsip_auth_algorithm *algorithm =
            pjsip_auth_get_algorithm_by_type(algorithm_type);

    PJ_ASSERT_RETURN( auth_srv && tdata, PJ_EINVAL );

    /* Check that algorithm is supported. */
    if (!algorithm || !pjsip_auth_is_algorithm_supported(algorithm_type)) {
        return PJSIP_EINVALIDALGORITHM;
    }

    random.ptr = nonce_buf;
    random.slen = sizeof(nonce_buf);

    /* Create the header. */
    if (auth_srv->is_proxy)
        hdr = pjsip_proxy_authenticate_hdr_create(tdata->pool);
    else
        hdr = pjsip_www_authenticate_hdr_create(tdata->pool);

    /* Initialize header. 
     * Note: only support digest authentication now.
     */
    hdr->scheme = pjsip_DIGEST_STR;
    pj_strdup(tdata->pool, &hdr->challenge.digest.algorithm, &algorithm->iana_name);
    if (nonce) {
        pj_strdup(tdata->pool, &hdr->challenge.digest.nonce, nonce);
    } else {
        pj_create_random_string(nonce_buf, sizeof(nonce_buf));
        pj_strdup(tdata->pool, &hdr->challenge.digest.nonce, &random);
    }
    if (opaque) {
        pj_strdup(tdata->pool, &hdr->challenge.digest.opaque, opaque);
    } else {
        pj_create_random_string(nonce_buf, sizeof(nonce_buf));
        pj_strdup(tdata->pool, &hdr->challenge.digest.opaque, &random);
    }
    if (qop) {
        pj_strdup(tdata->pool, &hdr->challenge.digest.qop, qop);
    } else {
        hdr->challenge.digest.qop.slen = 0;
    }
    pj_strdup(tdata->pool, &hdr->challenge.digest.realm, &auth_srv->realm);
    hdr->challenge.digest.stale = stale;

    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_auth_srv_challenge(  pjsip_auth_srv *auth_srv,
                                               const pj_str_t *qop,
                                               const pj_str_t *nonce,
                                               const pj_str_t *opaque,
                                               pj_bool_t stale,
                                               pjsip_tx_data *tdata)
{
    return pjsip_auth_srv_challenge2(auth_srv, qop, nonce, opaque,
                                     stale, tdata, PJSIP_AUTH_ALGORITHM_MD5);
}

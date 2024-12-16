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
#include <pjsip/sip_auth_aka.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_util.h>
#include <pjlib-util/md5.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/guid.h>
#include <pj/assert.h>
#include <pj/ctype.h>


#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0 && \
    PJ_SSL_SOCK_IMP==PJ_SSL_SOCK_IMP_OPENSSL
#  include <openssl/opensslv.h>
#  include <openssl/sha.h>
#  include <openssl/evp.h>
#  include <openssl/md5.h>
#  include <openssl/sha.h>

#  if OPENSSL_VERSION_NUMBER < 0x10100000L
#    define EVP_MD_CTX_new() EVP_MD_CTX_create()
#    define EVP_MD_CTX_free(ctx) EVP_MD_CTX_destroy(ctx)
#  endif

#  ifdef _MSC_VER
#    include <openssl/opensslv.h>
#    if OPENSSL_VERSION_NUMBER >= 0x10100000L
#      pragma comment(lib, "libcrypto")
#    else
#      pragma comment(lib, "libeay32")
#      pragma comment(lib, "ssleay32")
#    endif
#  endif

#  define DEFINE_HASH_CONTEXT EVP_MD_CTX* mdctx

#else
#define HAVE_NO_OPENSSL 1
#define MD5_DIGEST_LENGTH (PJSIP_MD5STRLEN / 2)
#define SHA256_DIGEST_LENGTH (PJSIP_SHA256STRLEN / 2)
/* A macro just to get rid of type mismatch between char and unsigned char */
#define MD5_APPEND(pms,buf,len) pj_md5_update(pms, (const pj_uint8_t*)buf, \
                                              (unsigned)len)
#define EVP_MD char
#define EVP_MD_CTX pj_md5_context;
#define DEFINE_HASH_CONTEXT pj_md5_context pmc; pj_md5_context* mdctx = &pmc

#define EVP_get_digestbyname(digest_name) (digest_name)
#define EVP_MD_CTX_new() &pmc
#define EVP_DigestInit_ex(mdctx, md, _unused) (void)md; pj_md5_init(mdctx)
#define EVP_DigestUpdate(mdctx, data, len) MD5_APPEND(mdctx, data, len)
#define EVP_DigestFinal_ex(mdctx, digest, _unused) pj_md5_final(mdctx, digest)
#define EVP_MD_CTX_free(mdctx)
#endif

const pjsip_auth_algorithm pjsip_auth_algorithms[] = {
/*    TYPE                             IANA name            OpenSSL name */
/*      Raw digest byte length  Hex representation length                */
    { PJSIP_AUTH_ALGORITHM_NOT_SET,    {"", 0},             "",
        0,                      0},
    { PJSIP_AUTH_ALGORITHM_MD5,        {"MD5", 3},          "MD5",
        MD5_DIGEST_LENGTH,      MD5_DIGEST_LENGTH * 2},
    { PJSIP_AUTH_ALGORITHM_SHA256,     {"SHA-256", 7},      "SHA256",
        SHA256_DIGEST_LENGTH,   SHA256_DIGEST_LENGTH * 2},
    { PJSIP_AUTH_ALGORITHM_SHA512_256, {"SHA-512-256", 11}, "SHA512-256",
        SHA256_DIGEST_LENGTH,   SHA256_DIGEST_LENGTH * 2},
    { PJSIP_AUTH_ALGORITHM_AKAV1_MD5,  {"AKAv1-MD5", 9},    "",
        MD5_DIGEST_LENGTH,      MD5_DIGEST_LENGTH * 2},
    { PJSIP_AUTH_ALGORITHM_AKAV2_MD5,  {"AKAv2-MD5", 9},    "",
        MD5_DIGEST_LENGTH,      MD5_DIGEST_LENGTH * 2},
    { PJSIP_AUTH_ALGORITHM_COUNT,      {"", 0},             "",
        0,                      0},
};


/* Logging. */
#define THIS_FILE   "sip_auth_client.c"
#if 0
#  define AUTH_TRACE_(expr)  PJ_LOG(3, expr)
#else
#  define AUTH_TRACE_(expr)
#endif


static void dup_bin(pj_pool_t *pool, pj_str_t *dst, const pj_str_t *src)
{
    dst->slen = src->slen;

    if (dst->slen) {
        dst->ptr = (char*) pj_pool_alloc(pool, src->slen);
        pj_memcpy(dst->ptr, src->ptr, src->slen);
    } else {
        dst->ptr = NULL;
    }
}

PJ_DEF(void) pjsip_cred_info_dup(pj_pool_t *pool,
                                 pjsip_cred_info *dst,
                                 const pjsip_cred_info *src)
{
    pj_memcpy(dst, src, sizeof(pjsip_cred_info));

    pj_strdup_with_null(pool, &dst->realm, &src->realm);
    pj_strdup_with_null(pool, &dst->scheme, &src->scheme);
    pj_strdup_with_null(pool, &dst->username, &src->username);
    pj_strdup_with_null(pool, &dst->data, &src->data);
    dst->algorithm_type = src->algorithm_type;

    if (PJSIP_CRED_DATA_IS_AKA(dst)) {
        dup_bin(pool, &dst->ext.aka.k, &src->ext.aka.k);
        dup_bin(pool, &dst->ext.aka.op, &src->ext.aka.op);
        dup_bin(pool, &dst->ext.aka.amf, &src->ext.aka.amf);
    }
}


PJ_DEF(int) pjsip_cred_info_cmp(const pjsip_cred_info *cred1,
                                const pjsip_cred_info *cred2)
{
    int result;

    result = pj_strcmp(&cred1->realm, &cred2->realm);
    if (result) goto on_return;
    result = pj_strcmp(&cred1->scheme, &cred2->scheme);
    if (result) goto on_return;
    result = pj_strcmp(&cred1->username, &cred2->username);
    if (result) goto on_return;
    result = pj_strcmp(&cred1->data, &cred2->data);
    if (result) goto on_return;
    result = (cred1->data_type != cred2->data_type);
    if (result) goto on_return;
    result = cred1->algorithm_type != cred2->algorithm_type;
    if (result) goto on_return;

    if (PJSIP_CRED_DATA_IS_AKA(cred1)) {
        result = pj_strcmp(&cred1->ext.aka.k, &cred2->ext.aka.k);
        if (result) goto on_return;
        result = pj_strcmp(&cred1->ext.aka.op, &cred2->ext.aka.op);
        if (result) goto on_return;
        result = pj_strcmp(&cred1->ext.aka.amf, &cred2->ext.aka.amf);
        if (result) goto on_return;
    }

on_return:
    return result;
}

PJ_DEF(void) pjsip_auth_clt_pref_dup( pj_pool_t *pool,
                                      pjsip_auth_clt_pref *dst,
                                      const pjsip_auth_clt_pref *src)
{
    pj_memcpy(dst, src, sizeof(pjsip_auth_clt_pref));
    pj_strdup_with_null(pool, &dst->algorithm, &src->algorithm);
}


/* Transform digest to string.
 * output must be at least PJSIP_MD5STRLEN+1 bytes.
 *
 * NOTE: THE OUTPUT STRING IS NOT NULL TERMINATED!
 */
static void digestNtoStr(const unsigned char digest[], int n, char *output)
{
    int i;
    for (i = 0; i<n; ++i) {
        pj_val_to_hex_digit(digest[i], output);
        output += 2;
    }
}


/*
 * Create response digest based on the parameters and store the
 * digest ASCII in 'result'.
 */
PJ_DEF(pj_status_t) pjsip_auth_create_digest2( pj_str_t *result,
                                               const pj_str_t *nonce,
                                               const pj_str_t *nc,
                                               const pj_str_t *cnonce,
                                               const pj_str_t *qop,
                                               const pj_str_t *uri,
                                               const pj_str_t *realm,
                                               const pjsip_cred_info *cred_info,
                                               const pj_str_t *method,
                                               const pjsip_auth_algorithm_type algorithm_type)
{
    const pjsip_auth_algorithm *algorithm = NULL;
    unsigned digest_len = 0;
    unsigned digest_strlen = 0;
    char ha1[PJSIP_AUTH_MAX_DIGEST_BUFFER_LENGTH * 2];
    char ha2[PJSIP_AUTH_MAX_DIGEST_BUFFER_LENGTH * 2];
    unsigned char digest[PJSIP_AUTH_MAX_DIGEST_BUFFER_LENGTH];
    unsigned dig_len = 0;
    const EVP_MD* md;
    DEFINE_HASH_CONTEXT;

    PJ_ASSERT_RETURN(result && nonce && uri && realm && cred_info && method, PJ_EINVAL);
    pj_bzero(result->ptr, result->slen);

    algorithm = pjsip_auth_get_algorithm_by_type(algorithm_type);
    if (!algorithm) {
        PJ_LOG(4, (THIS_FILE, "The algorithm_type is invalid"));
        return PJ_ENOTSUP;
    }

    if (!pjsip_auth_is_algorithm_supported(algorithm->algorithm_type)) {
        PJ_LOG(4, (THIS_FILE,
                "The algorithm (%.*s) referenced by algorithm_type is not supported",
                (int)algorithm->iana_name.slen, algorithm->iana_name.ptr));
        return PJ_ENOTSUP;
    }

    if (qop && !(nc && cnonce)) {
        PJ_LOG(4, (THIS_FILE, "nc and cnonce are required if qop is specified"));
        return PJ_EINVAL;
    }

    digest_len = algorithm->digest_length;
    digest_strlen = algorithm->digest_str_length;
    dig_len = digest_len;

    if (result->slen < (pj_ssize_t)digest_strlen) {
        PJ_LOG(4, (THIS_FILE,
                "The length of the result buffer must be at least %d bytes "
                "for algorithm %.*s", digest_strlen,
                (int)algorithm->iana_name.slen, algorithm->iana_name.ptr));
        return PJ_EINVAL;
    }
    result->slen = 0;

    if (!PJSIP_CRED_DATA_IS_PASSWD(cred_info) && !PJSIP_CRED_DATA_IS_DIGEST(cred_info)) {
        PJ_LOG(4, (THIS_FILE,
                "cred_info->data_type must be PJSIP_CRED_DATA_PLAIN_PASSWD "
                "or PJSIP_CRED_DATA_DIGEST"));
        return PJ_EINVAL;
    }

    if (PJSIP_CRED_DATA_IS_DIGEST(cred_info)) {
        pjsip_auth_algorithm_type cred_algorithm_type = cred_info->algorithm_type;

        if (cred_algorithm_type == PJSIP_AUTH_ALGORITHM_NOT_SET) {
            cred_algorithm_type = algorithm_type;
        } else if (cred_algorithm_type != algorithm_type) {
            PJ_LOG(4,(THIS_FILE,
                    "The algorithm specified in the cred_info (%.*s) "
                    "doesn't match the algorithm requested for hashing (%.*s)",
                    (int)pjsip_auth_algorithms[cred_algorithm_type].iana_name.slen,
                    pjsip_auth_algorithms[cred_algorithm_type].iana_name.ptr,
                    (int)pjsip_auth_algorithms[algorithm_type].iana_name.slen,
                    pjsip_auth_algorithms[algorithm_type].iana_name.ptr));
            return PJ_EINVAL;
        }
        PJ_ASSERT_RETURN(cred_info->data.slen >= (pj_ssize_t)digest_strlen,
                         PJ_EINVAL);
    }

    md = EVP_get_digestbyname(algorithm->openssl_name);
    if (md == NULL) {
        /* Shouldn't happen since it was checked above */
        return PJ_ENOTSUP;
    }

    AUTH_TRACE_((THIS_FILE, "Begin creating %.*s digest",
            (int)algorithm->iana_name.slen, algorithm->iana_name.ptr));

    if (PJSIP_CRED_DATA_IS_PASSWD(cred_info))
    {
        AUTH_TRACE_((THIS_FILE, " Using plain text password for %.*s digest",
                (int)algorithm->iana_name.slen, algorithm->iana_name.ptr));
        /***
         *** ha1 = (digest)(username ":" realm ":" password)
         ***/
        mdctx = EVP_MD_CTX_new();

        EVP_DigestInit_ex(mdctx, md, NULL);
        EVP_DigestUpdate(mdctx, cred_info->username.ptr, cred_info->username.slen);
        EVP_DigestUpdate(mdctx, ":", 1);
        EVP_DigestUpdate(mdctx, realm->ptr, realm->slen);
        EVP_DigestUpdate(mdctx, ":", 1);
        EVP_DigestUpdate(mdctx, cred_info->data.ptr, cred_info->data.slen);

        EVP_DigestFinal_ex(mdctx, digest, &dig_len);
        EVP_MD_CTX_free(mdctx);
        digestNtoStr(digest, dig_len, ha1);

    } else {
        AUTH_TRACE_((THIS_FILE, " Using pre computed digest for %.*s digest",
                (int)algorithm->iana_name.slen, algorithm->iana_name.ptr));
        pj_memcpy( ha1, cred_info->data.ptr, cred_info->data.slen );
    }

    AUTH_TRACE_((THIS_FILE, " ha1=%.*s", algorithm->digest_str_length, ha1));

    /***
     *** ha2 = (digest)(method ":" req_uri)
     ***/
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, method->ptr, method->slen);
    EVP_DigestUpdate(mdctx, ":", 1);
    EVP_DigestUpdate(mdctx, uri->ptr, uri->slen);
    EVP_DigestFinal_ex(mdctx, digest, &dig_len);
    EVP_MD_CTX_free(mdctx);
    digestNtoStr(digest, dig_len, ha2);

    AUTH_TRACE_((THIS_FILE, " ha2=%.*s", algorithm->digest_str_length, ha2));

    /***
     *** When qop is not used:
     ***   response = (digest)(ha1 ":" nonce ":" ha2)
     ***
     *** When qop=auth is used:
     ***   response = (digest)(ha1 ":" nonce ":" nc ":" cnonce ":" qop ":" ha2)
     ***/
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, ha1, digest_strlen);
    EVP_DigestUpdate(mdctx, ":", 1);
    EVP_DigestUpdate(mdctx, nonce->ptr, nonce->slen);
    if (qop && qop->slen != 0) {
        EVP_DigestUpdate(mdctx, ":", 1);
        EVP_DigestUpdate(mdctx, nc->ptr, nc->slen);
        EVP_DigestUpdate(mdctx, ":", 1);
        EVP_DigestUpdate(mdctx, cnonce->ptr, cnonce->slen);
        EVP_DigestUpdate(mdctx, ":", 1);
        EVP_DigestUpdate(mdctx, qop->ptr, qop->slen);
    }
    EVP_DigestUpdate(mdctx, ":", 1);
    EVP_DigestUpdate(mdctx, ha2, digest_strlen);

    EVP_DigestFinal_ex(mdctx, digest, &dig_len);
    EVP_MD_CTX_free(mdctx);

    /* Convert digest to string and store in chal->response. */
    result->slen = digest_strlen;
    digestNtoStr(digest, digest_len, result->ptr);

    AUTH_TRACE_((THIS_FILE, "%.*s digest=%.*s",
            (int)algorithm->iana_name.slen, algorithm->iana_name.ptr,
            (int)result->slen, result->ptr));

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_auth_create_digest( pj_str_t *result,
                                              const pj_str_t *nonce,
                                              const pj_str_t *nc,
                                              const pj_str_t *cnonce,
                                              const pj_str_t *qop,
                                              const pj_str_t *uri,
                                              const pj_str_t *realm,
                                              const pjsip_cred_info *cred_info,
                                              const pj_str_t *method)
{
    PJ_ASSERT_RETURN(cred_info, PJ_EINVAL);
    PJ_ASSERT_RETURN(!PJSIP_CRED_DATA_IS_AKA(cred_info), PJ_EINVAL);

    return pjsip_auth_create_digest2(result, nonce, nc, cnonce,
            qop, uri, realm, cred_info, method,
            PJSIP_AUTH_ALGORITHM_MD5);
}


/*
 * Create response SHA-256 digest based on the parameters and store the
 * digest ASCII in 'result'.
 * \deprecated Use pjsip_auth_create_digest2 with
 *     algorithm_type = PJSIP_AUTH_ALGORITHM_SHA256.
 */
PJ_DEF(pj_status_t) pjsip_auth_create_digestSHA256(pj_str_t *result,
                                            const pj_str_t *nonce,
                                            const pj_str_t *nc,
                                            const pj_str_t *cnonce,
                                            const pj_str_t *qop,
                                            const pj_str_t *uri,
                                            const pj_str_t *realm,
                                            const pjsip_cred_info *cred_info,
                                            const pj_str_t *method)
{
    PJ_ASSERT_RETURN(cred_info, PJ_EINVAL);
    PJ_ASSERT_RETURN(!PJSIP_CRED_DATA_IS_AKA(cred_info), PJ_EINVAL);

    return pjsip_auth_create_digest2(result, nonce, nc, cnonce,
            qop, uri, realm, cred_info, method,
            PJSIP_AUTH_ALGORITHM_SHA256);
}


PJ_DEF(const pjsip_auth_algorithm *) pjsip_auth_get_algorithm_by_type(
    pjsip_auth_algorithm_type algorithm_type)
{
    if (algorithm_type > PJSIP_AUTH_ALGORITHM_NOT_SET
        && algorithm_type < PJSIP_AUTH_ALGORITHM_COUNT) {
        return &pjsip_auth_algorithms[algorithm_type];
    }
    return NULL;
}


PJ_DEF(const pjsip_auth_algorithm *) pjsip_auth_get_algorithm_by_iana_name(
    const pj_str_t *iana_name)
{
    int i;

    if (!iana_name) {
        return NULL;
    }

    if (iana_name->slen == 0) {
        return &pjsip_auth_algorithms[PJSIP_AUTH_ALGORITHM_MD5];
    }

#ifdef HAVE_NO_OPENSSL
    i = PJSIP_AUTH_ALGORITHM_MD5;
    if (pj_stricmp(iana_name, &pjsip_auth_algorithms[i].iana_name) == 0) {
        return &pjsip_auth_algorithms[i];
    }
#else
    for (i = PJSIP_AUTH_ALGORITHM_NOT_SET + 1; i < PJSIP_AUTH_ALGORITHM_COUNT; i++) {
        if (pj_stricmp(iana_name, &pjsip_auth_algorithms[i].iana_name) == 0) {
            return &pjsip_auth_algorithms[i];
        }
    }
#endif
    return NULL;
}


PJ_DEF(pj_bool_t) pjsip_auth_is_algorithm_supported(
    pjsip_auth_algorithm_type algorithm_type)
{
    const pjsip_auth_algorithm *algorithm = NULL;

    if (algorithm_type <= PJSIP_AUTH_ALGORITHM_NOT_SET
        || algorithm_type >= PJSIP_AUTH_ALGORITHM_COUNT) {
        return PJ_FALSE;
    }
    algorithm = &pjsip_auth_algorithms[algorithm_type];

    /*
     * If the openssl_name is empty there's no need to check
     * if OpenSSL supports it.
     */
    if (algorithm->openssl_name[0] == '\0') {
        return PJ_TRUE;
    }

#ifdef HAVE_NO_OPENSSL
    return (algorithm_type == PJSIP_AUTH_ALGORITHM_MD5);
#else
    {
        const EVP_MD* md;
        md = EVP_get_digestbyname(algorithm->openssl_name);
        if (md == NULL) {
            return PJ_FALSE;
        }
        return PJ_TRUE;
    }
#endif
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
        *p = (char)pj_tolower(*p);
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
 * The pool is used to allocate enough bytes to store the digest in cred->response.
 */
static pj_status_t respond_digest( pj_pool_t *pool,
                                   pjsip_digest_credential *cred,
                                   const pjsip_digest_challenge *chal,
                                   const pj_str_t *uri,
                                   const pjsip_cred_info *cred_info,
                                   const pj_str_t *cnonce,
                                   pj_uint32_t nc,
                                   const pj_str_t *method,
                                   const pjsip_auth_algorithm_type challenge_algorithm_type)
{
    pj_status_t status = PJ_SUCCESS;

    AUTH_TRACE_((THIS_FILE, "Begin responding to %.*s challenge",
        (int)chal->algorithm.slen, chal->algorithm.ptr));

    /* Build digest credential from arguments. */
    pj_strdup(pool, &cred->username, &cred_info->username);
    pj_strdup(pool, &cred->realm, &chal->realm);
    pj_strdup(pool, &cred->nonce, &chal->nonce);
    pj_strdup(pool, &cred->uri, uri);
    pj_strdup(pool, &cred->algorithm, &chal->algorithm);
    pj_strdup(pool, &cred->opaque, &chal->opaque);

    /* Allocate memory. */
    cred->response.slen = pjsip_auth_algorithms[challenge_algorithm_type].digest_str_length;
    cred->response.ptr = (char*) pj_pool_alloc(pool, cred->response.slen);

    if (chal->qop.slen == 0) {
        /* Server doesn't require quality of protection. */

        if (PJSIP_CRED_DATA_IS_AKA(cred_info)) {
            /* Call application callback to create the response digest */
            return (*cred_info->ext.aka.cb)(pool, chal, cred_info,
                                            method, cred);
        }
        else {
            /* Convert digest to string and store in chal->response. */
            status = pjsip_auth_create_digest2(
                                      &cred->response, &cred->nonce, NULL,
                                      NULL,  NULL, uri, &chal->realm,
                                      cred_info, method, challenge_algorithm_type);
        }

    } else if (has_auth_qop(pool, &chal->qop)) {
        /* Server requires quality of protection.
         * We respond with selecting "qop=auth" protection.
         */
        cred->qop = pjsip_AUTH_STR;
        cred->nc.ptr = (char*) pj_pool_alloc(pool, 16);
        cred->nc.slen = pj_ansi_snprintf(cred->nc.ptr, 16, "%08u", nc);

        if (cnonce && cnonce->slen) {
            pj_strdup(pool, &cred->cnonce, cnonce);
        } else {
            pj_str_t dummy_cnonce = { "b39971", 6};
            pj_strdup(pool, &cred->cnonce, &dummy_cnonce);
        }

        if (PJSIP_CRED_DATA_IS_AKA(cred_info)) {
            /* Call application callback to create the response digest */
            return (*cred_info->ext.aka.cb)(pool, chal, cred_info,
                                            method, cred);
        }
        else {
            /* Convert digest to string and store in chal->response. */
            status = pjsip_auth_create_digest2(
                                      &cred->response, &cred->nonce,
                                      &cred->nc, &cred->cnonce,
                                      &pjsip_AUTH_STR, uri,
                                      &chal->realm, cred_info,
                                      method, challenge_algorithm_type);
        }

    } else {
        /* Server requires quality protection that we don't support. */
        PJ_LOG(4,(THIS_FILE, "Unsupported qop offer %.*s",
                  (int)chal->qop.slen, chal->qop.ptr));
        return PJSIP_EINVALIDQOP;
    }

    return status;
}

#if defined(PJSIP_AUTH_QOP_SUPPORT) && PJSIP_AUTH_QOP_SUPPORT!=0
/*
 * Update authentication session with a challenge.
 */
static void update_digest_session( pjsip_cached_auth *cached_auth,
                                   const pjsip_www_authenticate_hdr *hdr )
{
    if (hdr->challenge.digest.qop.slen == 0) {
#if PJSIP_AUTH_AUTO_SEND_NEXT!=0
        if (!cached_auth->last_chal || pj_stricmp2(&hdr->scheme, "digest")) {
            cached_auth->last_chal = (pjsip_www_authenticate_hdr*)
                                     pjsip_hdr_clone(cached_auth->pool, hdr);
        } else {
            /* Only update if the new challenge is "significantly different"
             * than the one in the cache, to reduce memory usage.
             */
            const pjsip_digest_challenge *d1 =
                        &cached_auth->last_chal->challenge.digest;
            const pjsip_digest_challenge *d2 = &hdr->challenge.digest;

            if (pj_strcmp(&d1->domain, &d2->domain) ||
                pj_strcmp(&d1->realm, &d2->realm) ||
                pj_strcmp(&d1->nonce, &d2->nonce) ||
                pj_strcmp(&d1->opaque, &d2->opaque) ||
                pj_strcmp(&d1->algorithm, &d2->algorithm) ||
                pj_strcmp(&d1->qop, &d2->qop))
            {
                cached_auth->last_chal = (pjsip_www_authenticate_hdr*)
                                       pjsip_hdr_clone(cached_auth->pool, hdr);
            }
        }
#endif
        return;
    }

    /* Initialize cnonce and qop if not present. */
    if (cached_auth->cnonce.slen == 0) {
        /* Save the whole challenge */
        cached_auth->last_chal = (pjsip_www_authenticate_hdr*)
                                 pjsip_hdr_clone(cached_auth->pool, hdr);

        /* Create cnonce */
        pj_create_unique_string( cached_auth->pool, &cached_auth->cnonce );
#if defined(PJSIP_AUTH_CNONCE_USE_DIGITS_ONLY) && \
    PJSIP_AUTH_CNONCE_USE_DIGITS_ONLY!=0
        if (pj_strchr(&cached_auth->cnonce, '-')) {
            /* remove hyphen character. */
            pj_size_t w, r, len = pj_strlen(&cached_auth->cnonce);
            char *s = cached_auth->cnonce.ptr;

            w = r = 0;
            for (; r < len; r++) {
                if (s[r] != '-')
                    s[w++] = s[r];
            }
            s[w] = '\0';
            cached_auth->cnonce.slen = w;
        }
#endif

        /* Initialize nonce-count */
        cached_auth->nc = 1;

        /* Save realm. */
        /* Note: allow empty realm (https://github.com/pjsip/pjproject/issues/1061)
        pj_assert(cached_auth->realm.slen != 0);
        */
        if (cached_auth->realm.slen == 0) {
            pj_strdup(cached_auth->pool, &cached_auth->realm,
                      &hdr->challenge.digest.realm);
        }

    } else {
        /* Update last_nonce and nonce-count */
        if (!pj_strcmp(&hdr->challenge.digest.nonce,
                       &cached_auth->last_chal->challenge.digest.nonce))
        {
            /* Same nonce, increment nonce-count */
            ++cached_auth->nc;
        } else {
            /* Server gives new nonce. */
            pj_strdup(cached_auth->pool, 
                      &cached_auth->last_chal->challenge.digest.nonce,
                      &hdr->challenge.digest.nonce);
            /* Has the opaque changed? */
            if (pj_strcmp(&cached_auth->last_chal->challenge.digest.opaque,
                          &hdr->challenge.digest.opaque))
            {
                pj_strdup(cached_auth->pool,
                          &cached_auth->last_chal->challenge.digest.opaque,
                          &hdr->challenge.digest.opaque);
            }
            cached_auth->nc = 1;
        }
    }
}
#endif  /* PJSIP_AUTH_QOP_SUPPORT */


/* Find cached authentication in the list for the specified realm. */
static pjsip_cached_auth *find_cached_auth( pjsip_auth_clt_sess *sess,
                                            const pj_str_t *realm,
                                            pjsip_auth_algorithm_type algorithm_type)
{
    pjsip_cached_auth *auth = sess->cached_auth.next;
    while (auth != &sess->cached_auth) {
        if (pj_stricmp(&auth->realm, realm) == 0
            && auth->challenge_algorithm_type == algorithm_type)
            return auth;
        auth = auth->next;
    }

    return NULL;
}

/* Find credential to use for the specified realm and auth scheme. */
static const pjsip_cred_info* auth_find_cred( const pjsip_auth_clt_sess *sess,
                                              const pj_str_t *realm,
                                              const pj_str_t *auth_scheme,
                                              const pjsip_auth_algorithm_type algorithm_type)
{
    unsigned i;
    int wildcard = -1;

    PJ_UNUSED_ARG(auth_scheme);

    for (i=0; i<sess->cred_cnt; ++i) {
        switch(sess->cred_info[i].data_type) {
        case PJSIP_CRED_DATA_PLAIN_PASSWD:
            /* PLAIN_PASSWD creds can be used for any algorithm other than AKA */
            if (algorithm_type != PJSIP_AUTH_ALGORITHM_AKAV1_MD5
                    && algorithm_type != PJSIP_AUTH_ALGORITHM_AKAV2_MD5) {
                break;
            }
            continue;
        case PJSIP_CRED_DATA_DIGEST:
            /* Digest creds can only be used if the algorithms match */
            if (sess->cred_info[i].algorithm_type == algorithm_type) {
                break;
            }
            continue;
        case PJSIP_CRED_DATA_EXT_AKA:
            /* AKA creds can only be used for AKA algorithm */
            if (algorithm_type == PJSIP_AUTH_ALGORITHM_AKAV1_MD5
                    || algorithm_type == PJSIP_AUTH_ALGORITHM_AKAV2_MD5) {
                break;
            }
            continue;
        }
        /*
         * We've determined that the credential can be used for the
         * specified algorithm, now check the realm.
         */
        if (pj_stricmp(&sess->cred_info[i].realm, realm) == 0)
            return &sess->cred_info[i];
        else if (sess->cred_info[i].realm.slen == 1 &&
                 sess->cred_info[i].realm.ptr[0] == '*')
        {
            wildcard = i;
        }
    }

    /* No matching realm. See if we have credential with wildcard ('*')
     * as the realm.
     */
    if (wildcard != -1)
        return &sess->cred_info[wildcard];

    /* Nothing is suitable */
    return NULL;
}


/* Init client session. */
PJ_DEF(pj_status_t) pjsip_auth_clt_init(  pjsip_auth_clt_sess *sess,
                                          pjsip_endpoint *endpt,
                                          pj_pool_t *pool,
                                          unsigned options)
{
    PJ_ASSERT_RETURN(sess && endpt && pool && (options==0), PJ_EINVAL);

    sess->pool = pool;
    sess->endpt = endpt;
    sess->cred_cnt = 0;
    sess->cred_info = NULL;
    pj_list_init(&sess->cached_auth);

    return PJ_SUCCESS;
}


/* Deinit client session. */
PJ_DEF(pj_status_t) pjsip_auth_clt_deinit(pjsip_auth_clt_sess *sess)
{
    pjsip_cached_auth *auth;
    
    PJ_ASSERT_RETURN(sess && sess->endpt, PJ_EINVAL);
    
    auth = sess->cached_auth.next;
    while (auth != &sess->cached_auth) {
        pjsip_endpt_release_pool(sess->endpt, auth->pool);
        auth = auth->next;
    }

    return PJ_SUCCESS;
}


/* Clone session. */
PJ_DEF(pj_status_t) pjsip_auth_clt_clone( pj_pool_t *pool,
                                          pjsip_auth_clt_sess *sess,
                                          const pjsip_auth_clt_sess *rhs )
{
    unsigned i;

    PJ_ASSERT_RETURN(pool && sess && rhs, PJ_EINVAL);

    pjsip_auth_clt_init(sess, (pjsip_endpoint*)rhs->endpt, pool, 0);

    sess->cred_cnt = rhs->cred_cnt;
    sess->cred_info = (pjsip_cred_info*)
                      pj_pool_alloc(pool,
                                    sess->cred_cnt*sizeof(pjsip_cred_info));
    for (i=0; i<rhs->cred_cnt; ++i) {
        pj_strdup(pool, &sess->cred_info[i].realm, &rhs->cred_info[i].realm);
        pj_strdup(pool, &sess->cred_info[i].scheme, &rhs->cred_info[i].scheme);
        pj_strdup(pool, &sess->cred_info[i].username,
                  &rhs->cred_info[i].username);
        sess->cred_info[i].data_type = rhs->cred_info[i].data_type;
        pj_strdup(pool, &sess->cred_info[i].data, &rhs->cred_info[i].data);
        if (PJSIP_CRED_DATA_IS_DIGEST(&rhs->cred_info[i])) {
            sess->cred_info[i].algorithm_type = rhs->cred_info[i].algorithm_type;
        }
        if (PJSIP_CRED_DATA_IS_AKA(&rhs->cred_info[i])) {
            pj_strdup(pool, &sess->cred_info[i].ext.aka.k, &rhs->cred_info[i].ext.aka.k);
            pj_strdup(pool, &sess->cred_info[i].ext.aka.op, &rhs->cred_info[i].ext.aka.op);
            pj_strdup(pool, &sess->cred_info[i].ext.aka.amf, &rhs->cred_info[i].ext.aka.amf);
            sess->cred_info[i].ext.aka.cb = rhs->cred_info[i].ext.aka.cb;
        }
    }

    /* TODO note:
     * Cloning the full authentication client is quite a big task.
     * We do only the necessary bits here, i.e. cloning the credentials.
     * The drawback of this basic approach is, a forked dialog will have to
     * re-authenticate itself on the next request because it has lost the
     * cached authentication headers.
     */
    PJ_TODO(FULL_CLONE_OF_AUTH_CLIENT_SESSION);

    return PJ_SUCCESS;
}


/* Set client credentials. */
PJ_DEF(pj_status_t) pjsip_auth_clt_set_credentials( pjsip_auth_clt_sess *sess,
                                                    int cred_cnt,
                                                    const pjsip_cred_info *c)
{
    PJ_ASSERT_RETURN(sess && c, PJ_EINVAL);

    if (cred_cnt == 0) {
        sess->cred_cnt = 0;
    } else {
        int i;
        sess->cred_info = (pjsip_cred_info*)
                          pj_pool_alloc(sess->pool, cred_cnt * sizeof(*c));
        for (i=0; i<cred_cnt; ++i) {
            sess->cred_info[i].data_type = c[i].data_type;

            /* When data_type is PJSIP_CRED_DATA_EXT_AKA,
             * callback must be specified.
             */
            if (PJSIP_CRED_DATA_IS_AKA(&c[i])) {

#if !PJSIP_HAS_DIGEST_AKA_AUTH
                if (!PJSIP_HAS_DIGEST_AKA_AUTH) {
                    pj_assert(!"PJSIP_HAS_DIGEST_AKA_AUTH is not enabled");
                    return PJSIP_EAUTHINAKACRED;
                }
#endif

                /* Callback must be specified */
                PJ_ASSERT_RETURN(c[i].ext.aka.cb != NULL, PJ_EINVAL);

                /* Verify K len */
                PJ_ASSERT_RETURN(c[i].ext.aka.k.slen <= PJSIP_AKA_KLEN,
                                 PJSIP_EAUTHINAKACRED);

                /* Verify OP len */
                PJ_ASSERT_RETURN(c[i].ext.aka.op.slen <= PJSIP_AKA_OPLEN,
                                 PJSIP_EAUTHINAKACRED);

                /* Verify AMF len */
                PJ_ASSERT_RETURN(c[i].ext.aka.amf.slen <= PJSIP_AKA_AMFLEN,
                                 PJSIP_EAUTHINAKACRED);

                sess->cred_info[i].ext.aka.cb = c[i].ext.aka.cb;
                pj_strdup(sess->pool, &sess->cred_info[i].ext.aka.k,
                          &c[i].ext.aka.k);
                pj_strdup(sess->pool, &sess->cred_info[i].ext.aka.op,
                          &c[i].ext.aka.op);
                pj_strdup(sess->pool, &sess->cred_info[i].ext.aka.amf,
                          &c[i].ext.aka.amf);
            }

            pj_strdup(sess->pool, &sess->cred_info[i].scheme, &c[i].scheme);
            pj_strdup(sess->pool, &sess->cred_info[i].realm, &c[i].realm);
            pj_strdup(sess->pool, &sess->cred_info[i].username, &c[i].username);
            pj_strdup(sess->pool, &sess->cred_info[i].data, &c[i].data);
            /*
             * If the data type is DIGEST and an auth algorithm isn't set,
             * default it to MD5.
             */
            if (PJSIP_CRED_DATA_IS_DIGEST(&c[i]) &&
                c[i].algorithm_type == PJSIP_AUTH_ALGORITHM_NOT_SET) {
                sess->cred_info[i].algorithm_type = PJSIP_AUTH_ALGORITHM_MD5;
            } else {
                sess->cred_info[i].algorithm_type = c[i].algorithm_type;
            }
        }
        sess->cred_cnt = cred_cnt;
    }

    return PJ_SUCCESS;
}


/*
 * Set the preference for the client authentication session.
 */
PJ_DEF(pj_status_t) pjsip_auth_clt_set_prefs(pjsip_auth_clt_sess *sess,
                                             const pjsip_auth_clt_pref *p)
{
    PJ_ASSERT_RETURN(sess && p, PJ_EINVAL);

    pj_memcpy(&sess->pref, p, sizeof(*p));
    pj_strdup(sess->pool, &sess->pref.algorithm, &p->algorithm);
    //if (sess->pref.algorithm.slen == 0)
    //  sess->pref.algorithm = pj_str("MD5");

    return PJ_SUCCESS;
}


/*
 * Get the preference for the client authentication session.
 */
PJ_DEF(pj_status_t) pjsip_auth_clt_get_prefs(pjsip_auth_clt_sess *sess,
                                             pjsip_auth_clt_pref *p)
{
    PJ_ASSERT_RETURN(sess && p, PJ_EINVAL);

    pj_memcpy(p, &sess->pref, sizeof(pjsip_auth_clt_pref));
    return PJ_SUCCESS;
}


/*
 * Create Authorization/Proxy-Authorization response header based on the challege
 * in WWW-Authenticate/Proxy-Authenticate header.
 */
static pj_status_t auth_respond( pj_pool_t *req_pool,
                                 const pjsip_www_authenticate_hdr *hdr,
                                 const pjsip_uri *uri,
                                 const pjsip_cred_info *cred_info,
                                 const pjsip_method *method,
                                 pj_pool_t *sess_pool,
                                 pjsip_cached_auth *cached_auth,
                                 pjsip_authorization_hdr **p_h_auth,
                                 const pjsip_auth_algorithm_type challenge_algorithm_type)
{
    pjsip_authorization_hdr *hauth;
    char tmp[PJSIP_MAX_URL_SIZE];
    pj_str_t uri_str;
    pj_pool_t *pool;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(req_pool && hdr && uri && cred_info && method &&
                     sess_pool && cached_auth && p_h_auth, PJ_EINVAL);

    /* Print URL in the original request. */
    uri_str.ptr = tmp;
    uri_str.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, uri, tmp,sizeof(tmp));
    if (uri_str.slen < 1) {
        return PJSIP_EURITOOLONG;
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
        hauth = pjsip_authorization_hdr_create(pool);
    else if (hdr->type == PJSIP_H_PROXY_AUTHENTICATE)
        hauth = pjsip_proxy_authorization_hdr_create(pool);
    else {
        return PJSIP_EINVALIDHDR;
    }

    /* Only support digest scheme at the moment. */
    if (!pj_stricmp(&hdr->scheme, &pjsip_DIGEST_STR)) {
        pj_str_t *cnonce = NULL;
        pj_uint32_t nc = 1;

        /* Update the session (nonce-count etc) if required. */
#       if PJSIP_AUTH_QOP_SUPPORT
        {
            if (cached_auth) {
                update_digest_session( cached_auth, hdr );

                cnonce = &cached_auth->cnonce;
                nc = cached_auth->nc;
            }
        }
#       endif   /* PJSIP_AUTH_QOP_SUPPORT */

        hauth->scheme = pjsip_DIGEST_STR;
        status = respond_digest( pool, &hauth->credential.digest,
                                 &hdr->challenge.digest, &uri_str, cred_info,
                                 cnonce, nc, &method->name, challenge_algorithm_type);
        if (status != PJ_SUCCESS)
            return status;

        /* Set qop type in auth session the first time only. */
        if (hdr->challenge.digest.qop.slen != 0 && cached_auth) {
            if (cached_auth->qop_value == PJSIP_AUTH_QOP_NONE) {
                pj_str_t *qop_val = &hauth->credential.digest.qop;
                if (!pj_strcmp(qop_val, &pjsip_AUTH_STR)) {
                    cached_auth->qop_value = PJSIP_AUTH_QOP_AUTH;
                } else {
                    cached_auth->qop_value = PJSIP_AUTH_QOP_UNKNOWN;
                }
            }
        }
    } else {
        return PJSIP_EINVALIDAUTHSCHEME;
    }

    /* Keep the new authorization header in the cache, only
     * if no qop is not present.
     */
#   if PJSIP_AUTH_HEADER_CACHING
    {
        if (hauth && cached_auth && cached_auth->qop_value == PJSIP_AUTH_QOP_NONE) {
            pjsip_cached_auth_hdr *cached_hdr;

            /* Delete old header with the same method. */
            cached_hdr = cached_auth->cached_hdr.next;
            while (cached_hdr != &cached_auth->cached_hdr) {
                if (pjsip_method_cmp(method, &cached_hdr->method)==0)
                    break;
                cached_hdr = cached_hdr->next;
            }

            /* Save the header to the list. */
            if (cached_hdr != &cached_auth->cached_hdr) {
                cached_hdr->hdr = hauth;
            } else {
                cached_hdr = pj_pool_alloc(pool, sizeof(*cached_hdr));
                pjsip_method_copy( pool, &cached_hdr->method, method);
                cached_hdr->hdr = hauth;
                pj_list_insert_before( &cached_auth->cached_hdr, cached_hdr );
            }
        }

#       if defined(PJSIP_AUTH_AUTO_SEND_NEXT) && PJSIP_AUTH_AUTO_SEND_NEXT!=0
            if (hdr != cached_auth->last_chal) {
                cached_auth->last_chal = pjsip_hdr_clone(sess_pool, hdr);
            }
#       endif
    }
#   endif

    *p_h_auth = hauth;
    return PJ_SUCCESS;

}


#if defined(PJSIP_AUTH_AUTO_SEND_NEXT) && PJSIP_AUTH_AUTO_SEND_NEXT!=0
static pj_status_t new_auth_for_req( pjsip_tx_data *tdata,
                                     pjsip_auth_clt_sess *sess,
                                     pjsip_cached_auth *auth,
                                     pjsip_authorization_hdr **p_h_auth)
{
    const pjsip_cred_info *cred;
    pjsip_authorization_hdr *hauth;
    pj_status_t status;

    PJ_ASSERT_RETURN(tdata && sess && auth, PJ_EINVAL);
    PJ_ASSERT_RETURN(auth->last_chal != NULL, PJSIP_EAUTHNOPREVCHAL);

    cred = auth_find_cred( sess, &auth->realm, &auth->last_chal->scheme,
            auth->challenge_algorithm_type );
    if (!cred)
        return PJSIP_ENOCREDENTIAL;

    status = auth_respond( tdata->pool, auth->last_chal,
                           tdata->msg->line.req.uri,
                           cred, &tdata->msg->line.req.method,
                           sess->pool, auth, &hauth, auth->challenge_algorithm_type);
    if (status != PJ_SUCCESS)
        return status;

    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)hauth);

    if (p_h_auth)
        *p_h_auth = hauth;

    return PJ_SUCCESS;
}
#endif


/* Find credential in list of (Proxy-)Authorization headers */
static pjsip_authorization_hdr* get_header_for_cred_info(
                                const pjsip_hdr *hdr_list,
                                const pjsip_cred_info *cred_info)
{
    pjsip_authorization_hdr *h;

    h = (pjsip_authorization_hdr*)hdr_list->next;
    while (h != (pjsip_authorization_hdr*)hdr_list) {
        /* If the realm doesn't match, just skip */
        if (pj_stricmp(&h->credential.digest.realm, &cred_info->realm) != 0) {
            h = h->next;
            continue;
        }

        switch (cred_info->data_type) {
        case PJSIP_CRED_DATA_PLAIN_PASSWD:
            /*
            * If cred_info->data_type is PLAIN_PASSWD, then we can use the header
            * regardless of algorithm.
            */
            return h;
        case PJSIP_CRED_DATA_DIGEST:
            /*
             * If cred_info->data_type is DIGEST, then we need to check if the
             * algorithms match.
             */
            if (pj_stricmp(&h->credential.digest.algorithm,
                &pjsip_auth_algorithms[cred_info->algorithm_type].iana_name) == 0) {
                return h;
            }
            break;
        case PJSIP_CRED_DATA_EXT_AKA:
            /*
             * If cred_info->data_type is EXT_AKA, then we need to check if the
             * challenge algorithm is AKAv1-MD5 or AKAv2-MD5.
             */
            if (pj_stricmp(&h->credential.digest.algorithm,
                        &pjsip_auth_algorithms[PJSIP_AUTH_ALGORITHM_AKAV1_MD5].iana_name) == 0
                || pj_stricmp(&h->credential.digest.algorithm,
                        &pjsip_auth_algorithms[PJSIP_AUTH_ALGORITHM_AKAV2_MD5].iana_name) == 0) {
                return h;
            }
            break;
        }
        h = h->next;
    }

    return NULL;
}


/* Initialize outgoing request. */
PJ_DEF(pj_status_t) pjsip_auth_clt_init_req( pjsip_auth_clt_sess *sess,
                                             pjsip_tx_data *tdata )
{
    const pjsip_method *method;
    pjsip_cached_auth *auth;
    pjsip_hdr added;

    PJ_ASSERT_RETURN(sess && tdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(sess->pool, PJSIP_ENOTINITIALIZED);
    PJ_ASSERT_RETURN(tdata->msg->type==PJSIP_REQUEST_MSG,
                     PJSIP_ENOTREQUESTMSG);

    /* Init list */
    pj_list_init(&added);

    /* Get the method. */
    method = &tdata->msg->line.req.method;
    PJ_UNUSED_ARG(method); /* Warning about unused var caused by #if below */

    auth = sess->cached_auth.next;
    while (auth != &sess->cached_auth) {
        /* Reset stale counter */
        auth->stale_cnt = 0;

        if (auth->qop_value == PJSIP_AUTH_QOP_NONE) {
#           if defined(PJSIP_AUTH_HEADER_CACHING) && \
               PJSIP_AUTH_HEADER_CACHING!=0
            {
                pjsip_cached_auth_hdr *entry = auth->cached_hdr.next;
                while (entry != &auth->cached_hdr) {
                    if (pjsip_method_cmp(&entry->method, method)==0) {
                        pjsip_authorization_hdr *hauth;
                        hauth = pjsip_hdr_shallow_clone(tdata->pool, entry->hdr);
                        //pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hauth);
                        pj_list_push_back(&added, hauth);
                        break;
                    }
                    entry = entry->next;
                }

#               if defined(PJSIP_AUTH_AUTO_SEND_NEXT) && \
                           PJSIP_AUTH_AUTO_SEND_NEXT!=0
                {
                    if (entry == &auth->cached_hdr)
                        new_auth_for_req( tdata, sess, auth, NULL);
                }
#               endif

            }
#           elif defined(PJSIP_AUTH_AUTO_SEND_NEXT) && \
                 PJSIP_AUTH_AUTO_SEND_NEXT!=0
            {
                new_auth_for_req( tdata, sess, auth, NULL);
            }
#           endif

        }
#       if defined(PJSIP_AUTH_QOP_SUPPORT) && \
           defined(PJSIP_AUTH_AUTO_SEND_NEXT) && \
           (PJSIP_AUTH_QOP_SUPPORT && PJSIP_AUTH_AUTO_SEND_NEXT)
        else if (auth->qop_value == PJSIP_AUTH_QOP_AUTH) {
            /* For qop="auth", we have to re-create the authorization header.
             */
            const pjsip_cred_info *cred;
            pjsip_authorization_hdr *hauth;
            pj_status_t status;

            cred = auth_find_cred(sess, &auth->realm,
                                  &auth->last_chal->scheme,
                                  auth->challenge_algorithm_type);
            if (!cred) {
                auth = auth->next;
                continue;
            }

            status = auth_respond( tdata->pool, auth->last_chal,
                                   tdata->msg->line.req.uri,
                                   cred,
                                   &tdata->msg->line.req.method,
                                   sess->pool, auth, &hauth,
                                   auth->challenge_algorithm_type);
            if (status != PJ_SUCCESS)
                return status;

            //pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hauth);
            pj_list_push_back(&added, hauth);
        }
#       endif   /* PJSIP_AUTH_QOP_SUPPORT && PJSIP_AUTH_AUTO_SEND_NEXT */

        auth = auth->next;
    }

    if (sess->pref.initial_auth == PJ_FALSE) {
        pjsip_hdr *h;

        /* Don't want to send initial empty Authorization header, so
         * just send whatever available in the list (maybe empty).
         */

        h = added.next;
        while (h != &added) {
            pjsip_hdr *next = h->next;
            pjsip_msg_add_hdr(tdata->msg, h);
            h = next;
        }
    } else {
        /* For each realm, add either the cached authorization header
         * or add an empty authorization header.
         */
        unsigned i;
        pj_str_t uri;

        uri.ptr = (char*)pj_pool_alloc(tdata->pool, PJSIP_MAX_URL_SIZE);
        uri.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
                                   tdata->msg->line.req.uri,
                                   uri.ptr, PJSIP_MAX_URL_SIZE);
        if (uri.slen < 1 || uri.slen >= PJSIP_MAX_URL_SIZE)
            return PJSIP_EURITOOLONG;

        for (i=0; i<sess->cred_cnt; ++i) {
            pjsip_cred_info *c = &sess->cred_info[i];
            pjsip_authorization_hdr *h;

            h = get_header_for_cred_info(&added, c);
            if (h) {
                pj_list_erase(h);
                pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)h);
            } else {
                pjsip_authorization_hdr *hs;

                hs = pjsip_authorization_hdr_create(tdata->pool);
                pj_strdup(tdata->pool, &hs->scheme, &c->scheme);
                if (pj_stricmp(&c->scheme, &pjsip_BEARER_STR)==0) {
                        pj_strdup(tdata->pool, &hs->credential.oauth.username,
                                  &c->username);
                        pj_strdup(tdata->pool, &hs->credential.oauth.realm,
                                  &c->realm);
                        pj_strdup(tdata->pool, &hs->credential.oauth.token,
                                  &c->data);
                } else { //if (pj_stricmp(&c->scheme, &pjsip_DIGEST_STR)==0)
                        pj_strdup(tdata->pool, &hs->credential.digest.username,
                                  &c->username);
                        pj_strdup(tdata->pool, &hs->credential.digest.realm,
                                  &c->realm);
                        pj_strdup(tdata->pool,&hs->credential.digest.uri, &uri);

                        if (c->algorithm_type == PJSIP_AUTH_ALGORITHM_NOT_SET) {
                            pj_strdup(tdata->pool, &hs->credential.digest.algorithm,
                                &sess->pref.algorithm);
                        } else {
                            pj_strdup(tdata->pool, &hs->credential.digest.algorithm,
                                &pjsip_auth_algorithms[c->algorithm_type].iana_name);
                        }
                }

                pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hs);
            }
        }
    }

    return PJ_SUCCESS;
}


static void recreate_cached_auth_pool( pjsip_endpoint *endpt, 
                                       pjsip_cached_auth *auth )
{
    pj_pool_t *auth_pool = pjsip_endpt_create_pool(endpt, "auth_cli%p", 1024, 
                                                   1024);

    if (auth->realm.slen) {
        pj_str_t realm;
        pj_strdup(auth_pool, &realm, &auth->realm);
        pj_strassign(&auth->realm, &realm);
    }

    if (auth->cnonce.slen) {
        pj_str_t cnonce;
        pj_strdup(auth_pool, &cnonce, &auth->cnonce);
        pj_strassign(&auth->cnonce, &cnonce);
    }

    if (auth->last_chal) {
        auth->last_chal = (pjsip_www_authenticate_hdr*)
                          pjsip_hdr_clone(auth_pool, auth->last_chal);
    }

    pjsip_endpt_release_pool(endpt, auth->pool);
    auth->pool = auth_pool;
}

/* Process authorization challenge */
static pj_status_t process_auth( pj_pool_t *req_pool,
                                 const pjsip_www_authenticate_hdr *hchal,
                                 const pjsip_uri *uri,
                                 pjsip_tx_data *tdata,
                                 pjsip_auth_clt_sess *sess,
                                 pjsip_cached_auth *cached_auth,
                                 pjsip_authorization_hdr **h_auth,
                                 const pjsip_auth_algorithm_type challenge_algorithm_type)
{
    const pjsip_cred_info *cred;
    pjsip_authorization_hdr *sent_auth = NULL;
    pjsip_hdr *hdr;
    pj_status_t status;

    /* See if we have sent authorization header for this realm (and scheme) */
    hdr = tdata->msg->hdr.next;
    while (hdr != &tdata->msg->hdr) {
        if ((hchal->type == PJSIP_H_WWW_AUTHENTICATE &&
             hdr->type == PJSIP_H_AUTHORIZATION) ||
            (hchal->type == PJSIP_H_PROXY_AUTHENTICATE &&
             hdr->type == PJSIP_H_PROXY_AUTHORIZATION))
        {
            sent_auth = (pjsip_authorization_hdr*) hdr;
            if (pj_stricmp(&hchal->challenge.common.realm,
                           &sent_auth->credential.common.realm)==0 &&
                pj_stricmp(&hchal->scheme, &sent_auth->scheme)==0)
            {
                /* If this authorization has empty response, remove it. */
                if (pj_stricmp(&sent_auth->scheme, &pjsip_DIGEST_STR)==0 &&
                    sent_auth->credential.digest.response.slen == 0)
                {
                    /* This is empty authorization, remove it. */
                    hdr = hdr->next;
                    pj_list_erase(sent_auth);
                    continue;
                } else {
#if defined(PJSIP_AUTH_ALLOW_MULTIPLE_AUTH_HEADER) && \
            PJSIP_AUTH_ALLOW_MULTIPLE_AUTH_HEADER!=0
                    /*
                     * Keep sending additional headers if the the algorithm
                     * is different.
                     * WARNING:  See the comment in sip_config.h regarding
                     * how using this option could be a security risk if
                     * a header with a more secure digest algorithm has already
                     * been sent.
                     */
                    if (pj_stricmp(&sent_auth->scheme, &pjsip_DIGEST_STR)==0 &&
                        pj_stricmp(&sent_auth->credential.digest.algorithm,
                                   &hchal->challenge.digest.algorithm)!=0)
                    {
                        /* Same 'digest' scheme but different algo */
                        hdr = hdr->next;
                        continue;
                    } else
#endif
                    /* Found previous authorization attempt */
                    break;
                }
            }
        }
        hdr = hdr->next;
    }

    /* If we have sent, see if server rejected because of stale nonce or
     * other causes.
     */
    if (hdr != &tdata->msg->hdr) {
        pj_bool_t stale;

        /* Check sent_auth != NULL */
        PJ_ASSERT_RETURN(sent_auth, PJ_EBUG);

        /* Detect "stale" state */
        stale = hchal->challenge.digest.stale;
        if (!stale) {
            /* If stale is false, check is nonce has changed. Some servers
             * (broken ones!) want to change nonce but they fail to set
             * stale to true.
             */
            stale = pj_strcmp(&hchal->challenge.digest.nonce,
                              &sent_auth->credential.digest.nonce);
        }

        if (stale == PJ_FALSE) {
            /* Our credential is rejected. No point in trying to re-supply
             * the same credential.
             */
            PJ_LOG(4, (THIS_FILE, "Authorization failed for %.*s@%.*s: "
                       "server rejected with stale=false",
                       (int)sent_auth->credential.digest.username.slen,
                       sent_auth->credential.digest.username.ptr,
                       (int)sent_auth->credential.digest.realm.slen,
                       sent_auth->credential.digest.realm.ptr));
            return PJSIP_EFAILEDCREDENTIAL;
        }

        cached_auth->stale_cnt++;
        if (cached_auth->stale_cnt >= PJSIP_MAX_STALE_COUNT) {
            /* Our credential is rejected. No point in trying to re-supply
             * the same credential.
             */
            PJ_LOG(4, (THIS_FILE, "Authorization failed for %.*s@%.*s: "
                       "maximum number of stale retries exceeded",
                       (int)sent_auth->credential.digest.username.slen,
                       sent_auth->credential.digest.username.ptr,
                       (int)sent_auth->credential.digest.realm.slen,
                       sent_auth->credential.digest.realm.ptr));
            return PJSIP_EAUTHSTALECOUNT;
        }

        /* Otherwise remove old, stale authorization header from the mesasge.
         * We will supply a new one.
         */
        pj_list_erase(sent_auth);
    }

    /* Find credential to be used for the challenge. */
    cred = auth_find_cred( sess, &hchal->challenge.common.realm,
                           &hchal->scheme, challenge_algorithm_type);
    if (!cred) {
        const pj_str_t *realm = &hchal->challenge.common.realm;
        AUTH_TRACE_((THIS_FILE, "No cred for for %.*s",
            (int)hchal->challenge.digest.algorithm.slen, hchal->challenge.digest.algorithm.ptr));

        PJ_LOG(4,(THIS_FILE,
                  "Unable to set auth for %s: can not find credential for "
                  "%.*s/%.*s %.*s",
                  tdata->obj_name,
                  (int)realm->slen, realm->ptr,
                  (int)hchal->scheme.slen, hchal->scheme.ptr,
                  (int)hchal->challenge.digest.algorithm.slen, hchal->challenge.digest.algorithm.ptr));
        return PJSIP_ENOCREDENTIAL;
    }

    /* Respond to authorization challenge. */
    status = auth_respond( req_pool, hchal, uri, cred,
                           &tdata->msg->line.req.method,
                           sess->pool, cached_auth, h_auth, challenge_algorithm_type);
    return status;
}


/* Reinitialize outgoing request after 401/407 response is received.
 * The purpose of this function is:
 *  - to add a Authorization/Proxy-Authorization header.
 *  - to put the newly created Authorization/Proxy-Authorization header
 *    in cached_list.
 */
PJ_DEF(pj_status_t) pjsip_auth_clt_reinit_req(  pjsip_auth_clt_sess *sess,
                                                const pjsip_rx_data *rdata,
                                                pjsip_tx_data *old_request,
                                                pjsip_tx_data **new_request )
{
    pjsip_tx_data *tdata;
    const pjsip_hdr *hdr;
    unsigned chal_cnt, auth_cnt;
    pjsip_via_hdr *via;
    pj_status_t status;
    pj_status_t last_auth_err;

    PJ_ASSERT_RETURN(sess && rdata && old_request && new_request,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(sess->pool, PJSIP_ENOTINITIALIZED);
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_RESPONSE_MSG,
                     PJSIP_ENOTRESPONSEMSG);
    PJ_ASSERT_RETURN(old_request->msg->type == PJSIP_REQUEST_MSG,
                     PJSIP_ENOTREQUESTMSG);
    PJ_ASSERT_RETURN(rdata->msg_info.msg->line.status.code == 401 ||
                     rdata->msg_info.msg->line.status.code == 407,
                     PJSIP_EINVALIDSTATUS);

    tdata = old_request;
    tdata->auth_retry = PJ_FALSE;

    /*
     * Respond to each authentication challenge.
     */
    hdr = rdata->msg_info.msg->hdr.next;
    chal_cnt = 0;
    auth_cnt = 0;
    last_auth_err = PJSIP_EAUTHNOAUTH;
    while (hdr != &rdata->msg_info.msg->hdr) {
        pjsip_cached_auth *cached_auth;
        const pjsip_www_authenticate_hdr *hchal;
        pjsip_authorization_hdr *hauth;
        const pjsip_auth_algorithm *algorithm;

        /* Find WWW-Authenticate or Proxy-Authenticate header. */
        while (hdr != &rdata->msg_info.msg->hdr &&
               hdr->type != PJSIP_H_WWW_AUTHENTICATE &&
               hdr->type != PJSIP_H_PROXY_AUTHENTICATE)
        {
            hdr = hdr->next;
        }
        if (hdr == &rdata->msg_info.msg->hdr)
            break;

        hchal = (const pjsip_www_authenticate_hdr*)hdr;
        ++chal_cnt;

        /* At the current time, "digest" scheme is the only one supported. */
        if (pj_stricmp(&hchal->scheme, &pjsip_DIGEST_STR) != 0) {
            AUTH_TRACE_((THIS_FILE, "Skipped header for scheme %.*s",
                (int)hchal->scheme.slen, hchal->scheme.ptr));
            last_auth_err = PJSIP_EINVALIDAUTHSCHEME;
            hdr = hdr->next;
            continue;
        }

        algorithm = pjsip_auth_get_algorithm_by_iana_name(&hchal->challenge.digest.algorithm);
        if (!algorithm) {
            AUTH_TRACE_((THIS_FILE, "Skipped header for algorithm %.*s",
                (int)algorithm->iana_name.slen, algorithm->iana_name.ptr));
            last_auth_err = PJSIP_EINVALIDALGORITHM;
            hdr = hdr->next;
            continue;
        }

        /* Find authentication session for this realm, create a new one
         * if not present.
         */
        cached_auth = find_cached_auth(sess, &hchal->challenge.common.realm,
            algorithm->algorithm_type);
        if (!cached_auth) {
            cached_auth = PJ_POOL_ZALLOC_T(sess->pool, pjsip_cached_auth);
            cached_auth->pool = pjsip_endpt_create_pool(sess->endpt,
                                                        "auth_cli%p",
                                                        1024,
                                                        1024);
            pj_strdup(cached_auth->pool, &cached_auth->realm,
                      &hchal->challenge.common.realm);
            cached_auth->is_proxy = (hchal->type == PJSIP_H_PROXY_AUTHENTICATE);
            cached_auth->challenge_algorithm_type = algorithm->algorithm_type;
#           if (PJSIP_AUTH_HEADER_CACHING)
            {
                pj_list_init(&cached_auth->cached_hdr);
            }
#           endif
            pj_list_insert_before(&sess->cached_auth, cached_auth);
        }

        /* Create authorization header for this challenge, and update
         * authorization session.
         */
        status = process_auth(tdata->pool, hchal, tdata->msg->line.req.uri,
                              tdata, sess, cached_auth, &hauth, algorithm->algorithm_type);
        if (status != PJ_SUCCESS) {
            last_auth_err = status;
            AUTH_TRACE_((THIS_FILE, "Skipped header for realm %.*s algorithm %.*s",
                (int)hchal->challenge.common.realm.slen, hchal->challenge.common.realm.ptr,
                (int)algorithm->iana_name.slen, algorithm->iana_name.ptr));

            /* Process next header. */
            hdr = hdr->next;
            continue;
        }

        if (pj_pool_get_used_size(cached_auth->pool) >
            PJSIP_AUTH_CACHED_POOL_MAX_SIZE) 
        {
            recreate_cached_auth_pool(sess->endpt, cached_auth);
        }       

        /* Add to the message. */
        pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hauth);

        /* Process next header. */
        hdr = hdr->next;
        auth_cnt++;
    }

    /* Check if challenge is present */
    if (chal_cnt == 0)
        return PJSIP_EAUTHNOCHAL;

    /* Check if any authorization header has been created */
    if (auth_cnt == 0)
        return last_auth_err;

    /* Remove branch param in Via header. */
    via = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    via->branch_param.slen = 0;

    /* Restore strict route set.
     * See https://github.com/pjsip/pjproject/issues/492
     */
    pjsip_restore_strict_route_set(tdata);

    /* Must invalidate the message! */
    pjsip_tx_data_invalidate_msg(tdata);

    /* Retrying.. */
    tdata->auth_retry = PJ_TRUE;

    /* Increment reference counter. */
    pjsip_tx_data_add_ref(tdata);

    /* Done. */
    *new_request = tdata;
    return PJ_SUCCESS;

}


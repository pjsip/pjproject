/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/ssl_sock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/pool.h>
#include <pj/string.h>

/*
 * Initialize the SSL socket configuration with the default values.
 */
PJ_DEF(void) pj_ssl_sock_param_default(pj_ssl_sock_param *param)
{
    pj_bzero(param, sizeof(*param));

    /* Socket config */
    param->sock_af = PJ_AF_INET;
    param->sock_type = pj_SOCK_STREAM();
    param->async_cnt = 1;
    param->concurrency = -1;
    param->whole_data = PJ_TRUE;

#if (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_GNUTLS)
    /* GnuTLS is allowed to send bigger chunks.*/
    param->send_buffer_size = 65536;

    {
        /* For GnuTLS, TCP_NODELAY is needed to avoid polling delay. */
        static pj_int32_t val = 1;
        param->sockopt_params.cnt = 1;
        param->sockopt_params.options[0].level = pj_SOL_TCP();
        param->sockopt_params.options[0].optname = pj_TCP_NODELAY();
        param->sockopt_params.options[0].optval = &val;
        param->sockopt_params.options[0].optlen = sizeof(pj_int32_t);
    }

#else
    param->send_buffer_size = 8192;
#endif
#if !defined(PJ_SYMBIAN) || PJ_SYMBIAN==0
    param->read_buffer_size = 1500;
#endif
    param->qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    param->qos_ignore_error = PJ_TRUE;

    param->sockopt_ignore_error = PJ_TRUE;
    param->sock_cloexec = PJ_TRUE;
    param->enable_renegotiation = PJ_TRUE;

    /* Security config */
    param->proto = PJ_SSL_SOCK_PROTO_DEFAULT;
}


/*
 * Duplicate SSL socket parameter.
 */
PJ_DEF(void) pj_ssl_sock_param_copy( pj_pool_t *pool, 
                                     pj_ssl_sock_param *dst,
                                     const pj_ssl_sock_param *src)
{
    /* Init secure socket param */
    pj_memcpy(dst, src, sizeof(*dst));
    if (src->ciphers_num > 0) {
        unsigned i;
        dst->ciphers = (pj_ssl_cipher*)
                        pj_pool_calloc(pool, src->ciphers_num, 
                                       sizeof(pj_ssl_cipher));
        for (i = 0; i < src->ciphers_num; ++i)
            dst->ciphers[i] = src->ciphers[i];
    }

    if (src->curves_num > 0) {
        unsigned i;
        dst->curves = (pj_ssl_curve *)pj_pool_calloc(pool, src->curves_num,
                                                     sizeof(pj_ssl_curve));
        for (i = 0; i < src->curves_num; ++i)
            dst->curves[i] = src->curves[i];
    }

    if (src->server_name.slen) {
        /* Server name must be null-terminated */
        pj_strdup_with_null(pool, &dst->server_name, &src->server_name);
    }

    if (src->sigalgs.slen) {
        /* Sigalgs name must be null-terminated */
        pj_strdup_with_null(pool, &dst->sigalgs, &src->sigalgs);
    }

    if (src->entropy_path.slen) {
        /* Path name must be null-terminated */
        pj_strdup_with_null(pool, &dst->entropy_path, &src->entropy_path);
    }

    pj_sockopt_params_clone(pool, &dst->sockopt_params, &src->sockopt_params);
}


PJ_DEF(pj_status_t) pj_ssl_cert_get_verify_status_strings(
                                                pj_uint32_t verify_status, 
                                                const char *error_strings[],
                                                unsigned *count)
{
    unsigned i = 0, shift_idx = 0;
    unsigned unknown = 0;
    pj_uint32_t errs;

    PJ_ASSERT_RETURN(error_strings && count, PJ_EINVAL);

    if (verify_status == PJ_SSL_CERT_ESUCCESS && *count) {
        error_strings[0] = "OK";
        *count = 1;
        return PJ_SUCCESS;
    }

    errs = verify_status;

    while (errs && i < *count) {
        pj_uint32_t err;
        const char *p = NULL;

        if ((errs & 1) == 0) {
            shift_idx++;
            errs >>= 1;
            continue;
        }

        err = (1 << shift_idx);

        switch (err) {
        case PJ_SSL_CERT_EISSUER_NOT_FOUND:
            p = "The issuer certificate cannot be found";
            break;
        case PJ_SSL_CERT_EUNTRUSTED:
            p = "The certificate is untrusted";
            break;
        case PJ_SSL_CERT_EVALIDITY_PERIOD:
            p = "The certificate has expired or not yet valid";
            break;
        case PJ_SSL_CERT_EINVALID_FORMAT:
            p = "One or more fields of the certificate cannot be decoded "
                "due to invalid format";
            break;
        case PJ_SSL_CERT_EISSUER_MISMATCH:
            p = "The issuer info in the certificate does not match to the "
                "(candidate) issuer certificate";
            break;
        case PJ_SSL_CERT_ECRL_FAILURE:
            p = "The CRL certificate cannot be found or cannot be read "
                "properly";
            break;
        case PJ_SSL_CERT_EREVOKED:
            p = "The certificate has been revoked";
            break;
        case PJ_SSL_CERT_EINVALID_PURPOSE:
            p = "The certificate or CA certificate cannot be used for the "
                "specified purpose";
            break;
        case PJ_SSL_CERT_ECHAIN_TOO_LONG:
            p = "The certificate chain length is too long";
            break;
        case PJ_SSL_CERT_EWEAK_SIGNATURE:
            p = "The certificate signature is created using a weak hashing "
                "algorithm";
            break;
        case PJ_SSL_CERT_EIDENTITY_NOT_MATCH:
            p = "The server identity does not match to any identities "
                "specified in the certificate";
            break;
        case PJ_SSL_CERT_EUNKNOWN:
        default:
            unknown++;
            break;
        }
        
        /* Set error string */
        if (p)
            error_strings[i++] = p;

        /* Next */
        shift_idx++;
        errs >>= 1;
    }

    /* Unknown error */
    if (unknown && i < *count)
        error_strings[i++] = "Unknown verification error";

    *count = i;

    return PJ_SUCCESS;
}


/* Parse an IPv4 or IPv6 address literal, returns the address length. */
static unsigned parse_ip(const pj_str_t *s, pj_uint8_t addr[16])
{
    if (s->slen <= 0)
        return 0;
    if (pj_inet_pton(pj_AF_INET(), s, addr) == PJ_SUCCESS)
        return 4;
    if (pj_inet_pton(pj_AF_INET6(), s, addr) == PJ_SUCCESS)
        return 16;
    return 0;
}

static pj_bool_t match_dns_name(const pj_str_t *pattern,
                                const pj_str_t *name,
                                pj_bool_t allow_wildcard)
{
    pj_str_t pat_rest, name_rest;
    char *dot;

    if (pj_stricmp(pattern, name) == 0)
        return PJ_TRUE;

    if (!allow_wildcard || pattern->slen < 3 ||
        pattern->ptr[0] != '*' || pattern->ptr[1] != '.')
    {
        return PJ_FALSE;
    }

    /* Require at least two labels after the wildcard, e.g: reject "*.com" */
    pj_strset(&pat_rest, pattern->ptr + 1, pattern->slen - 1);
    if (!pj_memchr(pat_rest.ptr + 1, '.', pat_rest.slen - 1))
        return PJ_FALSE;

    /* The wildcard matches exactly one non-empty left-most label */
    dot = (char*)pj_memchr(name->ptr, '.', name->slen);
    if (!dot || dot == name->ptr)
        return PJ_FALSE;
    pj_strset(&name_rest, dot, name->slen - (dot - name->ptr));

    return pj_stricmp(&pat_rest, &name_rest) == 0;
}

PJ_DEF(pj_status_t) pj_ssl_cert_verify_name(const pj_ssl_cert_info *ci,
                                            const pj_str_t *name,
                                            unsigned flags)
{
    pj_bool_t allow_wildcard = !(flags & PJ_SSL_CERT_NAME_NO_WILDCARD);
    pj_uint8_t name_ip[16], san_ip[16];
    unsigned name_ip_len, i;

    PJ_ASSERT_RETURN(ci && name, PJ_EINVAL);

    if (name->slen <= 0 || pj_memchr(name->ptr, 0, name->slen))
        return PJ_ENOTFOUND;

    name_ip_len = parse_ip(name, name_ip);

    for (i = 0; i < ci->subj_alt_name.cnt; ++i) {
        const pj_str_t *cert_name = &ci->subj_alt_name.entry[i].name;

        switch (ci->subj_alt_name.entry[i].type) {
        case PJ_SSL_CERT_NAME_DNS:
            if (!name_ip_len &&
                match_dns_name(cert_name, name, allow_wildcard))
            {
                return PJ_SUCCESS;
            }
            break;
        case PJ_SSL_CERT_NAME_IP:
            if (name_ip_len && parse_ip(cert_name, san_ip) == name_ip_len &&
                pj_memcmp(name_ip, san_ip, name_ip_len) == 0)
            {
                return PJ_SUCCESS;
            }
            break;
        case PJ_SSL_CERT_NAME_URI:
            if ((flags & PJ_SSL_CERT_NAME_MATCH_SIP_URI) &&
                (pj_strnicmp2(cert_name, "sip:", 4) == 0 ||
                 pj_strnicmp2(cert_name, "sips:", 5) == 0))
            {
                pj_str_t host;
                char *p = pj_strchr(cert_name, ':') + 1;

                pj_strset(&host, p, cert_name->slen - (p - cert_name->ptr));
                if (pj_stricmp(&host, name) == 0)
                    return PJ_SUCCESS;
            }
            break;
        default:
            break;
        }
    }

    /* The Common Name is only an identity when the certificate carries no
     * SubjectAltName at all, so an entry of a type not matched above, e.g:
     * an email address, suppresses it too. Note that a type that no backend
     * extracts, e.g: an SRVName, leaves the count at zero.
     */
    if ((flags & PJ_SSL_CERT_NAME_MATCH_CN) && ci->subj_alt_name.cnt == 0 &&
        match_dns_name(&ci->subject.cn, name,
                       allow_wildcard && !name_ip_len))
    {
        return PJ_SUCCESS;
    }

    return PJ_ENOTFOUND;
}

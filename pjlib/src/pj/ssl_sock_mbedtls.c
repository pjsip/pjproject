/*
 * Copyright (C) 2025 Teluu Inc. (http://www.teluu.com)
 * Copyright (c) 2025 Arlo Technologies, Inc. (https://www.arlo.com)
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

#include <pj/activesock.h>
#include <pj/assert.h>
#include <pj/compat/socket.h>
#include <pj/errno.h>
#include <pj/file_io.h>
#include <pj/list.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/ssl_sock.h>
#include <pj/string.h>
#include <pj/timer.h>

/* Only build when PJ_HAS_SSL_SOCK is enabled and when the backend is
 * MbedTLS.
 */
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0 && \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_MBEDTLS)

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/oid.h"
#include "mbedtls/platform.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/version.h"

#ifdef MBEDTLS_DEBUG_C
#define MBEDTLS_DEBUG_VERBOSE 1
#endif

#define SSL_SOCK_IMP_USE_CIRC_BUF

#include "ssl_sock_imp_common.h"

#define THIS_FILE "ssl_sock_mbedtls.c"

/*
 * Secure socket structure definition.
 */
typedef struct mbedtls_sock_t {
    pj_ssl_sock_t base;

    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config ssl_config;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt cert;
    mbedtls_pk_context pk_ctx;
} mbedtls_sock_t;

#include "ssl_sock_imp_common.c"

/*
 *******************************************************************
 * Static/internal functions.
 *******************************************************************
 */

/* MbedTLS way of reporting internal operations. */
static void mbedtls_print_logs(void *ctx, int level, const char *file,
                               int line, const char *str)
{
    PJ_UNUSED_ARG(ctx);
    PJ_UNUSED_ARG(level);

    const char* last_slash = strrchr(file, '/');
    const char* file_name = last_slash ? last_slash + 1 : file;

    PJ_LOG(3, (THIS_FILE, "%s:%d: %s", file_name, line, str));
}

/* Convert from MbedTLS error to pj_status_t. */
static pj_status_t ssl_status_from_err(pj_ssl_sock_t *ssock, int err)
{
    PJ_UNUSED_ARG(ssock);

    pj_status_t status;

    switch (err) {
    case 0:
        status = PJ_SUCCESS;
        break;
    case MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL:
    case MBEDTLS_ERR_SSL_ALLOC_FAILED:
        status = PJ_ENOMEM;
        break;
    case MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE:
        status = PJ_ETOOBIG;
        break;
    case MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE:
        status = PJ_ENOTFOUND;
        break;
    case MBEDTLS_ERR_SSL_TIMEOUT:
        status = PJ_ETIMEDOUT;
        break;
    case MBEDTLS_ERR_SSL_INTERNAL_ERROR:
    case MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY:
        status = PJ_EBUG;
        break;
    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
    case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS:
    case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS:
    case MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA:
    case MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET:
        status = PJ_EPENDING;
        break;

    case MBEDTLS_ERR_SSL_UNEXPECTED_RECORD:
    case MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER:
    case MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION:
    case MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE:
        status = PJ_EINVAL;
        break;
    default:
        status = PJ_EUNKNOWN;
        break;
    }

    return status;
}

static int ssl_data_push(void *ctx, const unsigned char *buf, size_t len)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)ctx;

    pj_lock_acquire(ssock->circ_buf_output_mutex);
    if (circ_write(&ssock->circ_buf_output, buf, len) != PJ_SUCCESS) {
        pj_lock_release(ssock->circ_buf_output_mutex);

        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }

    pj_lock_release(ssock->circ_buf_output_mutex);

    return len;
}

static int ssl_data_pull(void *ctx, unsigned char *buf, size_t len)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)ctx;

    pj_lock_acquire(ssock->circ_buf_input_mutex);

    if (circ_empty(&ssock->circ_buf_input)) {
        pj_lock_release(ssock->circ_buf_input_mutex);

        return MBEDTLS_ERR_SSL_WANT_READ;
    }

    pj_size_t circ_buf_size = circ_size(&ssock->circ_buf_input);
    pj_size_t read_size = PJ_MIN(circ_buf_size, len);

    circ_read(&ssock->circ_buf_input, buf, read_size);
    pj_lock_release(ssock->circ_buf_input_mutex);

    return read_size;
}

/* Get Common Name field string from a general name string */
static void cert_get_cn(const pj_str_t *gen_name, pj_str_t *cn)
{
    pj_str_t CN_sign = {"CN=", 3};
    char *p, *q;

    pj_bzero(cn, sizeof(cn));

    p = pj_strstr(gen_name, &CN_sign);
    if (!p)
        return;

    p += 3; /* shift pointer to value part */
    pj_strset(cn, p, gen_name->slen - (p - gen_name->ptr));
    q = pj_strchr(cn, ',');
    if (q)
        cn->slen = q - p;
}

static void cert_get_time(pj_time_val *tv,
                          const mbedtls_x509_time *time,
                          pj_bool_t *gmt)
{
    pj_parsed_time pt;

    pt.day = time->day;
    pt.mon = time->mon - 1;
    pt.year = time->year;

    pt.sec = time->sec;
    pt.min =  time->min;
    pt.hour = time->hour;

    pt.msec = 0;

    // Assume MbedTLS always use GMT
    *gmt = PJ_TRUE;

    pj_time_encode(&pt, tv);
}

static void cert_get_alt_name(const mbedtls_x509_crt *crt,
                              pj_pool_t *pool,
                              pj_ssl_cert_info *ci)
{
    const mbedtls_x509_sequence *cur;
    size_t cnt_alt_name;
    int ret;

    int is_alt_name = mbedtls_x509_crt_has_ext_type(
                                    crt, MBEDTLS_X509_EXT_SUBJECT_ALT_NAME);
    if (!is_alt_name)
        return;

    cnt_alt_name = 0;
    cur = &crt->subject_alt_names;
    while (cur != NULL) {
        cur = cur->next;
        cnt_alt_name++;
    }

    ci->subj_alt_name.entry = pj_pool_calloc(pool, cnt_alt_name,
                                             sizeof(*ci->subj_alt_name.entry));
    if (!ci->subj_alt_name.entry) {
        PJ_LOG(2, (THIS_FILE, "Failed to allocate memory for SubjectAltName"));
        return;
    }

    ci->subj_alt_name.cnt = 0;
    cur = &crt->subject_alt_names;

    while (cur != NULL) {
        mbedtls_x509_subject_alternative_name san;
        pj_ssl_cert_name_type type;
        size_t len;

        memset(&san, 0, sizeof(san));
        ret = mbedtls_x509_parse_subject_alt_name(&cur->buf, &san);
        if (ret != 0) {
            cur = cur->next;
            continue;
        }

        len = san.san.unstructured_name.len;
        switch (san.type) {
        case MBEDTLS_X509_SAN_RFC822_NAME:
            type = PJ_SSL_CERT_NAME_RFC822;
            break;
        case MBEDTLS_X509_SAN_DNS_NAME:
            type = PJ_SSL_CERT_NAME_DNS;
            break;
        case MBEDTLS_X509_SAN_UNIFORM_RESOURCE_IDENTIFIER:
            type = PJ_SSL_CERT_NAME_URI;
            break;
        case MBEDTLS_X509_SAN_IP_ADDRESS:
            type = PJ_SSL_CERT_NAME_IP;
            break;
        default:
            type = PJ_SSL_CERT_NAME_UNKNOWN;
            break;
        }

        if (len && type != PJ_SSL_CERT_NAME_UNKNOWN) {
            ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
            if (type == PJ_SSL_CERT_NAME_IP) {
                char ip_buf[PJ_INET6_ADDRSTRLEN];
                int af = pj_AF_INET();
                if (len == sizeof(pj_in6_addr))
                    af = pj_AF_INET6();
                pj_inet_ntop2(af, san.san.unstructured_name.p,
                              ip_buf, sizeof(ip_buf));
                pj_strdup2(pool,
                           &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
                           ip_buf);
            } else {
                const pj_str_t str = {(char *)san.san.unstructured_name.p, len};
                pj_strdup(pool,
                          &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
                          &str);
            }
            ci->subj_alt_name.cnt++;
        }

        /* So far memory is freed only in the case of directoryName
         * parsing succeeding, as mbedtls_x509_get_name allocates memory.
         */
        mbedtls_x509_free_subject_alt_name(&san);
        cur = cur->next;
    }
}

static void update_cert_info(const mbedtls_x509_crt *crt,
                             pj_pool_t *pool, pj_ssl_cert_info *ci)
{
    int ret;
    char buf[1024];
    size_t bufsize = sizeof(buf);

    pj_bzero(ci, sizeof(pj_ssl_cert_info));

    ci->version = crt->version;

    /* Serial number */
    pj_memcpy(ci->serial_no, crt->serial.p, sizeof(ci->serial_no));

    /* Issuer */
    ret = mbedtls_x509_dn_gets(buf, bufsize, &crt->issuer);
    if (ret < 0) {
        PJ_LOG(2, (THIS_FILE, "Error parsing cert issuer"));
        return;
    }
    pj_strdup2(pool, &ci->issuer.info, buf);
    cert_get_cn(&ci->issuer.info, &ci->issuer.cn);

    /* Subject */
    ret = mbedtls_x509_dn_gets(buf, bufsize, &crt->subject);
    if (ret < 0) {
        PJ_LOG(2, (THIS_FILE, "Error parsing cert subject"));
        return;
    }
    pj_strdup2(pool, &ci->subject.info, buf);
    cert_get_cn(&ci->subject.info, &ci->subject.cn);

    /* Validity period */
    cert_get_time(&ci->validity.start, &crt->valid_from, &ci->validity.gmt);
    cert_get_time(&ci->validity.end, &crt->valid_to, &ci->validity.gmt);

    /* Subject Alternative Name extension */
    cert_get_alt_name(crt, pool, ci);
}

static pj_status_t set_ssl_protocol(pj_ssl_sock_t *ssock)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;
    mbedtls_ssl_protocol_version max_proto;
    mbedtls_ssl_protocol_version min_proto;

    if (ssock->param.proto == PJ_SSL_SOCK_PROTO_DEFAULT) {
        ssock->param.proto = PJ_SSL_SOCK_PROTO_TLS1_2 |
                             PJ_SSL_SOCK_PROTO_TLS1_3;
    }

    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_3) {
        max_proto = MBEDTLS_SSL_VERSION_TLS1_3;
    } else if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_2) {
        max_proto = MBEDTLS_SSL_VERSION_TLS1_2;
    } else {
        PJ_LOG(1, (THIS_FILE, "Unsupported TLS protocol"));
        return PJ_EINVAL;
    }

    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_2) {
        min_proto = MBEDTLS_SSL_VERSION_TLS1_2;
    } else if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_3) {
        min_proto = MBEDTLS_SSL_VERSION_TLS1_3;
    }

    mbedtls_ssl_conf_min_tls_version(&mssock->ssl_config, min_proto);
    mbedtls_ssl_conf_max_tls_version(&mssock->ssl_config, max_proto);

    return PJ_SUCCESS;
}

static int cert_verify_cb(void *data, mbedtls_x509_crt *crt,
                          int depth, uint32_t *flags)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)data;
    const int cert_dump_log_level = 5;

    if (pj_log_get_level() >= cert_dump_log_level) {
        char buf[1024];
        PJ_LOG(5, (THIS_FILE, "Certificate index in chain "
                              "- %d", depth));
        mbedtls_x509_crt_info(buf, sizeof(buf) - 1, "", crt);
        PJ_LOG(5, (THIS_FILE, "Certificate info:\n%s", buf));
    }

    /* Peer cert depth = 0 */
    if (depth > 0)
        return 0;

    if (((*flags) & MBEDTLS_X509_BADCERT_EXPIRED) != 0) {
        PJ_LOG(3, (THIS_FILE, "Server certificate has expired!"));
        ssock->verify_status |= PJ_SSL_CERT_EVALIDITY_PERIOD;
    }

    if (((*flags) & MBEDTLS_X509_BADCERT_REVOKED) != 0) {
        PJ_LOG(3, (THIS_FILE, "Server certificate has been revoked!"));
        ssock->verify_status |= PJ_SSL_CERT_EREVOKED;
    }

    if (((*flags) & MBEDTLS_X509_BADCERT_CN_MISMATCH) != 0) {
        PJ_LOG(3, (THIS_FILE, "CN mismatch!"));
        ssock->verify_status |= PJ_SSL_CERT_EISSUER_MISMATCH;
    }

    if (((*flags) & MBEDTLS_X509_BADCERT_NOT_TRUSTED) != 0) {
        PJ_LOG(3, (THIS_FILE, "Self-signed or not signed by a trusted CA!"));
        ssock->verify_status |= PJ_SSL_CERT_EUNTRUSTED;
    }

    if (((*flags) & MBEDTLS_X509_BADCRL_NOT_TRUSTED) != 0) {
        PJ_LOG(3, (THIS_FILE, "CRL not trusted!"));
        ssock->verify_status |= PJ_SSL_CERT_EUNTRUSTED;
    }

    if (((*flags) & MBEDTLS_X509_BADCRL_EXPIRED) != 0) {
        PJ_LOG(3, (THIS_FILE, "CRL expired!"));
        ssock->verify_status |= PJ_SSL_CERT_EVALIDITY_PERIOD;
    }

    if (((*flags) & MBEDTLS_X509_BADCERT_OTHER) != 0) {
        PJ_LOG(3, (THIS_FILE, "other (unknown) flags!"));
        ssock->verify_status |= PJ_SSL_CERT_EUNKNOWN;
    }

    if ((*flags) == 0) {
        PJ_LOG(5, (THIS_FILE, "Certificate verified without error flags"));
    } else {
        update_cert_info(crt, ssock->pool, &ssock->remote_cert_info);
    }

    return 0;
}

static pj_status_t set_cert(pj_ssl_sock_t *ssock)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;
    mbedtls_x509_crt *cacert_ = NULL;
    int authmode = MBEDTLS_SSL_VERIFY_NONE;
    int ret = 0;
    pj_ssl_cert_t *cert = ssock->cert;

    if (cert == NULL)
        goto on_config;

    if (cert->CA_buf.slen) {
        ret = mbedtls_x509_crt_parse(&mssock->cacert,
                                     (const unsigned char *)cert->CA_buf.ptr,
                                     cert->CA_buf.slen);
        if (ret != 0) {
            PJ_LOG(1, (THIS_FILE, "Failed to CA mbedtls_x509_crt_parse, "
                                  "ret = -0x%04X", -ret));
            goto on_error;
        }
        cacert_ = &mssock->cacert;
    }

    if (cert->cert_buf.slen && cert->privkey_buf.slen) {
        ret = mbedtls_x509_crt_parse(&mssock->cert,
                                     (const unsigned char *)cert->cert_buf.ptr,
                                     cert->cert_buf.slen);
        if (ret != 0) {
            PJ_LOG(1, (THIS_FILE, "Failed to mbedtls_x509_crt_parse, "
                                  "ret = -0x%04X", -ret));
            goto on_error;
        }

        ret = mbedtls_pk_parse_key(&mssock->pk_ctx,
                                   (const unsigned char *)cert->privkey_buf.ptr,
                                   cert->privkey_buf.slen,
                                   (const unsigned char *)cert->privkey_pass.ptr,
                                   cert->privkey_pass.slen,
                                   mbedtls_ctr_drbg_random, &mssock->ctr_drbg);
        if (ret != 0) {
            PJ_LOG(1, (THIS_FILE, "Failed to mbedtls_pk_parse, "
                                  "ret = -0x%04X", -ret));
            goto on_error;
        }

        mbedtls_ssl_conf_own_cert(&mssock->ssl_config,
                                  &mssock->cert,
                                  &mssock->pk_ctx);
    }

#if defined(MBEDTLS_FS_IO)
    if (cert->CA_file.slen) {
        ret = mbedtls_x509_crt_parse_file(&mssock->cacert, cert->CA_file.ptr);
        if (ret != 0) {
            PJ_LOG(1, (THIS_FILE, "Failed to CA mbedtls_x509_crt_parse_file, "
                                  "ret = -0x%04X", -ret));
            goto on_error;
        }
        cacert_ = &mssock->cacert;
    }

    if (cert->CA_path.slen) {
        ret = mbedtls_x509_crt_parse_path(&mssock->cacert, cert->CA_path.ptr);
        if (ret != 0) {
            PJ_LOG(1, (THIS_FILE, "Failed to CA mbedtls_x509_crt_parse_path, "
                                  "ret = -0x%04X", -ret));
            goto on_error;
        }
        cacert_ = &mssock->cacert;
    }

    if (cert->cert_file.slen && cert->privkey_file.slen) {
        ret = mbedtls_x509_crt_parse_file(&mssock->cert, cert->cert_file.ptr);
        if (ret != 0) {
            PJ_LOG(1, (THIS_FILE, "Failed to mbedtls_x509_crt_parse_file, "
                                  "ret = -0x%04X", -ret));
            goto on_error;
        }

        ret = mbedtls_pk_parse_keyfile(&mssock->pk_ctx,
                                       cert->privkey_file.ptr,
                                       cert->privkey_pass.ptr,
                                       mbedtls_ctr_drbg_random,
                                       &mssock->ctr_drbg);
        if (ret != 0) {
            PJ_LOG(1, (THIS_FILE, "Failed to mbedtls_pk_parse_keyfile, "
                                  "ret = -0x%04X", -ret));
            goto on_error;
        }

        mbedtls_ssl_conf_own_cert(&mssock->ssl_config,
                                  &mssock->cert,
                                  &mssock->pk_ctx);
    }
#endif

on_config:
    if (ssock->is_server) {
        if (ssock->param.require_client_cert) {
            authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
        } else {
            authmode = MBEDTLS_SSL_VERIFY_OPTIONAL;
        }
    } else {
        if (cacert_)
            authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
        else
            PJ_LOG(2, (THIS_FILE, "Peer validation is disabled"));
    }

    mbedtls_ssl_conf_verify(&mssock->ssl_config, cert_verify_cb, ssock);
    mbedtls_ssl_conf_ca_chain(&mssock->ssl_config, cacert_, NULL);
    mbedtls_ssl_conf_authmode(&mssock->ssl_config, authmode);

on_error:
    return ssl_status_from_err(ssock, ret);
}

static pj_status_t set_cipher_list(pj_ssl_sock_t *ssock)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;

    /* mbedtls_ssl_conf_ciphersuites requires a 0-terminated
     * list of supported ciphers
     */
    if (ssock->param.ciphers_num > 0) {
        unsigned i;
        pj_ssl_cipher *ciphers;
        ciphers = (pj_ssl_cipher*)
                  pj_pool_calloc(ssock->pool, ssock->param.ciphers_num + 1,
                                 sizeof(pj_ssl_cipher));
        if (!ciphers)
            return PJ_ENOMEM;

        for (i = 0; i < ssock->param.ciphers_num; ++i)
            ciphers[i] = ssock->param.ciphers[i];

        mbedtls_ssl_conf_ciphersuites(&mssock->ssl_config, ciphers);
    }

    return PJ_SUCCESS;
}

/* === SSL socket implementations === */

/* Allocate SSL backend struct */
static pj_ssl_sock_t *ssl_alloc(pj_pool_t *pool)
{
    return (pj_ssl_sock_t *)PJ_POOL_ZALLOC_T(pool, mbedtls_sock_t);
}

/* Create and initialize new SSL context and instance */
static pj_status_t ssl_create(pj_ssl_sock_t *ssock)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;
    const char *pers = "ssl_client";
    pj_status_t status;
    int ret;

    pj_assert(ssock);

    /* Initialize input circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_input, 512);
    if (status != PJ_SUCCESS)
        return status;

    /* Initialize output circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_output, 512);
    if (status != PJ_SUCCESS) {
        return status;
    }

    /* This version string is 18 bytes long, as advised by version.h. */
    char version[18];
    mbedtls_version_get_string_full(version);
    PJ_LOG(4, (THIS_FILE, "Mbed TLS version : %s", version));

#ifdef MBEDTLS_PSA_CRYPTO_C
    ret = psa_crypto_init();
    if (ret != PSA_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Failed to initialize PSA Crypto, "
                              "ret = -0x%04X", ret));
        return PJ_EUNKNOWN;
    }
#endif

    mbedtls_ssl_init(&mssock->ssl_ctx);
    mbedtls_ssl_config_init(&mssock->ssl_config);
    mbedtls_ctr_drbg_init(&mssock->ctr_drbg);
    mbedtls_entropy_init(&mssock->entropy);
    mbedtls_x509_crt_init(&mssock->cacert);
    mbedtls_x509_crt_init(&mssock->cert);
    mbedtls_pk_init(&mssock->pk_ctx);

    const int endpoint =
            ssock->is_server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;
    ret = mbedtls_ssl_config_defaults(&mssock->ssl_config, endpoint,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        PJ_LOG(1, (THIS_FILE, "Failed to mbedtls_ssl_config_defaults, "
                              "ret = -0x%04X", -ret));
        goto out;
    }

    ret = mbedtls_ctr_drbg_seed(&mssock->ctr_drbg, mbedtls_entropy_func,
                                &mssock->entropy,
                                (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        PJ_LOG(1, (THIS_FILE, "Failed to mbedtls_ctr_drbg_seed, "
                              "ret = -0x%04X", -ret));
        goto out;
    }

    mbedtls_ssl_conf_rng(&mssock->ssl_config,
                         mbedtls_ctr_drbg_random,
                         &mssock->ctr_drbg);

    status = set_ssl_protocol(ssock);
    if (status != PJ_SUCCESS)
        return status;

    status = set_cert(ssock);
    if (status != PJ_SUCCESS)
        return status;

    status = set_cipher_list(ssock);
    if (status != PJ_SUCCESS)
        return status;

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_ssl_conf_dbg(&mssock->ssl_config, mbedtls_print_logs, NULL);
    mbedtls_debug_set_threshold(MBEDTLS_DEBUG_VERBOSE);
#endif

    ret = mbedtls_ssl_setup(&mssock->ssl_ctx, &mssock->ssl_config);
    if (ret != 0) {
        PJ_LOG(1, (THIS_FILE, "Failed to mbedtls_ssl_setup, "
                              "ret = -0x%04X", -ret));
        goto out;
    }

    if (!ssock->is_server && ssock->param.server_name.slen) {
        ret = mbedtls_ssl_set_hostname(&mssock->ssl_ctx,
                                       ssock->param.server_name.ptr);
        if (ret != 0) {
            PJ_LOG(1, (THIS_FILE, "Failed to mbedtls_ssl_set_hostname, "
                                  "ret = -0x%04X", -ret));
            goto out;
        }
    }

    mbedtls_ssl_set_bio(&mssock->ssl_ctx, (void *)ssock,
                        (mbedtls_ssl_send_t *)ssl_data_push,
                        (mbedtls_ssl_recv_t *)ssl_data_pull,
                        NULL);

    ssl_ciphers_populate();

    ret = 0;

out:
    return ssl_status_from_err(ssock, ret);
}

/* Destroy MbedTLS credentials and session. */
static void ssl_destroy(pj_ssl_sock_t *ssock)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;

    mbedtls_ssl_free(&mssock->ssl_ctx);
    mbedtls_ssl_config_free(&mssock->ssl_config);
    mbedtls_x509_crt_free(&mssock->cacert);
    mbedtls_ctr_drbg_free(&mssock->ctr_drbg);
    mbedtls_entropy_free(&mssock->entropy);
    mbedtls_x509_crt_free(&mssock->cert);
    mbedtls_pk_free(&mssock->pk_ctx);

#ifdef MBEDTLS_PSA_CRYPTO_C
    mbedtls_psa_crypto_free();
#endif

    /* Destroy circular buffers */
    circ_deinit(&ssock->circ_buf_input);
    circ_deinit(&ssock->circ_buf_output);
}

/* Reset socket state. */
static void ssl_reset_sock_state(pj_ssl_sock_t *ssock)
{
    pj_lock_acquire(ssock->circ_buf_output_mutex);
    ssock->ssl_state = SSL_STATE_NULL;
    pj_lock_release(ssock->circ_buf_output_mutex);

    ssl_close_sockets(ssock);
}

static void ssl_ciphers_populate()
{
    /* Populate once only */
    if (ssl_cipher_num) {
        return;
    }

    const int *list = mbedtls_ssl_list_ciphersuites();

    for (unsigned num = 0;
            ssl_cipher_num < PJ_ARRAY_SIZE(ssl_ciphers) && list[num]; num++) {
        const mbedtls_ssl_ciphersuite_t * const ciphersuite =
                                mbedtls_ssl_ciphersuite_from_id(list[num]);
        if (!ciphersuite)
            continue;
        ssl_ciphers[ssl_cipher_num].name =
                                mbedtls_ssl_ciphersuite_get_name(ciphersuite);
        ssl_ciphers[ssl_cipher_num].id =
                                mbedtls_ssl_ciphersuite_get_id(ciphersuite);
        ssl_cipher_num++;
    }
}

static pj_ssl_cipher ssl_get_cipher(pj_ssl_sock_t *ssock)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;

    int id = mbedtls_ssl_get_ciphersuite_id_from_ssl(&mssock->ssl_ctx);
    if (id != 0) {
        return id;
    } else {
        return PJ_TLS_UNKNOWN_CIPHER;
    }
}

static void ssl_update_certs_info(pj_ssl_sock_t *ssock)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;

    pj_assert(ssock->ssl_state == SSL_STATE_ESTABLISHED);

    /* Get active remote certificate */
    const mbedtls_x509_crt *crt = mbedtls_ssl_get_peer_cert(&mssock->ssl_ctx);
    if (crt) {
        update_cert_info(crt, ssock->pool, &ssock->remote_cert_info);
    }
}

static void ssl_set_state(pj_ssl_sock_t *ssock, pj_bool_t is_server)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(is_server);
}

static void ssl_set_peer_name(pj_ssl_sock_t *ssock)
{
    /* Setting server name is done in ssl_create because ssl_create can handle
     * errors returned from Mbed TLS APIs properly.
     */
    PJ_UNUSED_ARG(ssock);
}

/* Try to perform an asynchronous handshake */
static pj_status_t ssl_do_handshake(pj_ssl_sock_t *ssock)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;
    pj_status_t handshake_status;
    pj_status_t status;
    int ret;

    ret = mbedtls_ssl_handshake(&mssock->ssl_ctx);
    handshake_status = ssl_status_from_err(ssock, ret);

    status = flush_circ_buf_output(ssock, &ssock->handshake_op_key, 0, 0);
    if (status != PJ_SUCCESS) {
        PJ_LOG(2, (THIS_FILE, "Failed to send handshake packets"));
        return status;
    }
    if (handshake_status == PJ_EPENDING)
        return PJ_EPENDING;


    if (handshake_status != PJ_SUCCESS) {
        PJ_LOG(2, (THIS_FILE, "Failed to mbedtls_ssl_handshake, "
                              "ret -0x%04X", -ret));
        return handshake_status;
    }

    ssock->ssl_state = SSL_STATE_ESTABLISHED;

    return PJ_SUCCESS;
}

static pj_status_t ssl_renegotiate(pj_ssl_sock_t *ssock)
{
    PJ_UNUSED_ARG(ssock);

    return PJ_SUCCESS;
}

static pj_status_t ssl_read(pj_ssl_sock_t *ssock, void *data, int *size)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;

    int ret = mbedtls_ssl_read(&mssock->ssl_ctx, data, *size);
    if (ret >= 0) {
        *size = ret;
        return PJ_SUCCESS;
    } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
        *size = 0;
        return PJ_SUCCESS;
    } else {
        return PJ_EUNKNOWN;
    }
}

static pj_status_t ssl_write(pj_ssl_sock_t *ssock, const void *data,
                             pj_ssize_t size, int *nwritten)
{
    mbedtls_sock_t *mssock = (mbedtls_sock_t *)ssock;
    pj_status_t status;
    pj_ssize_t nwritten_ = 0;
    int ret;

    while (nwritten_ < size) {
        ret = mbedtls_ssl_write(&mssock->ssl_ctx,
                                ((const unsigned char *)data) + nwritten_,
                                size - nwritten_);
        if (ret < 0) {
            status = ssl_status_from_err(ssock, ret);

            if (status == PJ_EPENDING)
                continue;

            *nwritten = nwritten_;
            PJ_LOG(2, (THIS_FILE, "Failed to mbedtls_ssl_write, "
                                  "ret -0x%04X", -ret));
            return status;
        }

        nwritten_ += ret;
    }

    *nwritten = nwritten_;
    return PJ_SUCCESS;
}

#endif /* PJ_HAS_SSL_SOCK */

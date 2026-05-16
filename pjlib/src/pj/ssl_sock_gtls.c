/*
 * Copyright (C) 2018-2018 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2014-2017 Savoir-faire Linux.
 * (https://www.savoirfairelinux.com)
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
#include <pj/activesock.h>
#include <pj/compat/socket.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/list.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/timer.h>
#include <pj/file_io.h>

#if GNUTLS_VERSION_NUMBER < 0x030306 && !defined(_MSC_VER)
#   include <dirent.h>
#endif

#include <errno.h>

/* Only build when PJ_HAS_SSL_SOCK and the implementation is GnuTLS. */
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0 && \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_GNUTLS)

#define SSL_SOCK_IMP_USE_CIRC_BUF

#include "ssl_sock_imp_common.h"
#include "ssl_sock_imp_common.c"

#define THIS_FILE               "ssl_sock_gtls.c"

/* Maximum ciphers */
#define MAX_CIPHERS             100

/* Standard trust locations */
#define TRUST_STORE_FILE1 "/etc/ssl/certs/ca-certificates.crt"
#define TRUST_STORE_FILE2 "/etc/ssl/certs/ca-bundle.crt"

/* Debugging output level for GnuTLS only */
#define GNUTLS_LOG_LEVEL 0

/* GnuTLS includes */
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>

#ifdef _MSC_VER
#  pragma comment( lib, "libgnutls")
#endif


/* Secure socket structure definition. */
typedef struct gnutls_sock_t {
    pj_ssl_sock_t         base;

    gnutls_session_t      session;
    gnutls_certificate_credentials_t xcred;

    int                   tls_init_count; /* library initialization counter */
} gnutls_sock_t;

/* Last error reported somehow */
static int tls_last_error;


/*
 *******************************************************************
 * Static/internal functions.
 *******************************************************************
 */

/* Convert from GnuTLS error to pj_status_t. */
static pj_status_t tls_status_from_err(pj_ssl_sock_t *ssock, int err)
{
    pj_status_t status;

    switch (err) {
    case GNUTLS_E_SUCCESS:
        status = PJ_SUCCESS;
        break;
    case GNUTLS_E_MEMORY_ERROR:
        status = PJ_ENOMEM;
        break;
    case GNUTLS_E_LARGE_PACKET:
        status = PJ_ETOOBIG;
        break;
    case GNUTLS_E_NO_CERTIFICATE_FOUND:
        status = PJ_ENOTFOUND;
        break;
    case GNUTLS_E_SESSION_EOF:
        status = PJ_EEOF;
        break;
    case GNUTLS_E_HANDSHAKE_TOO_LARGE:
        status = PJ_ETOOBIG;
        break;
    case GNUTLS_E_EXPIRED:
        status = PJ_EGONE;
        break;
    case GNUTLS_E_TIMEDOUT:
        status = PJ_ETIMEDOUT;
        break;
    case GNUTLS_E_PREMATURE_TERMINATION:
        status = PJ_ECANCELLED;
        break;
    case GNUTLS_E_INTERNAL_ERROR:
    case GNUTLS_E_UNIMPLEMENTED_FEATURE:
        status = PJ_EBUG;
        break;
    case GNUTLS_E_AGAIN:
    case GNUTLS_E_INTERRUPTED:
    case GNUTLS_E_REHANDSHAKE:
        status = PJ_EPENDING;
        break;
    case GNUTLS_E_TOO_MANY_EMPTY_PACKETS:
    case GNUTLS_E_TOO_MANY_HANDSHAKE_PACKETS:
    case GNUTLS_E_RECORD_LIMIT_REACHED:
        status = PJ_ETOOMANY;
        break;
    case GNUTLS_E_UNSUPPORTED_VERSION_PACKET:
    case GNUTLS_E_UNSUPPORTED_SIGNATURE_ALGORITHM:
    case GNUTLS_E_UNSUPPORTED_CERTIFICATE_TYPE:
    case GNUTLS_E_X509_UNSUPPORTED_ATTRIBUTE:
    case GNUTLS_E_X509_UNSUPPORTED_EXTENSION:
    case GNUTLS_E_X509_UNSUPPORTED_CRITICAL_EXTENSION:
        status = PJ_ENOTSUP;
        break;
    case GNUTLS_E_INVALID_SESSION:
    case GNUTLS_E_INVALID_REQUEST:
    case GNUTLS_E_INVALID_PASSWORD:
    case GNUTLS_E_ILLEGAL_PARAMETER:
    case GNUTLS_E_RECEIVED_ILLEGAL_EXTENSION:
    case GNUTLS_E_UNEXPECTED_PACKET:
    case GNUTLS_E_UNEXPECTED_PACKET_LENGTH:
    case GNUTLS_E_UNEXPECTED_HANDSHAKE_PACKET:
    case GNUTLS_E_UNWANTED_ALGORITHM:
    case GNUTLS_E_USER_ERROR:
        status = PJ_EINVAL;
        break;
    default:
        status = PJ_EUNKNOWN;
        break;
    }

    /* Not thread safe */
    tls_last_error = err;
    if (ssock)
        ssock->last_err = err;
    return status;
}


/* Get error string from GnuTLS using tls_last_error */
static pj_str_t tls_strerror(pj_status_t status,
                             char *buf, pj_size_t bufsize)
{
    PJ_UNUSED_ARG(status);
    pj_str_t errstr;
    const char *tmp = gnutls_strerror(tls_last_error);

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)
    if (tmp) {
        pj_ansi_strxcpy(buf, tmp, bufsize);
        errstr = pj_str(buf);
        return errstr;
    }
#endif /* PJ_HAS_ERROR_STRING */

    errstr.ptr = buf;
    errstr.slen = pj_ansi_snprintf(buf, bufsize, "GnuTLS error %d: %s",
                                   tls_last_error, tmp);
    if (errstr.slen < 1 || errstr.slen >= (int) bufsize)
        errstr.slen = bufsize - 1;

    return errstr;
}


/* GnuTLS way of reporting internal operations. */
static void tls_print_logs(int level, const char* msg)
{
    PJ_LOG(3, (THIS_FILE, "GnuTLS [%d]: %s", level, msg));
}


/* Initialize GnuTLS. */
static int tls_init_count;
static void tls_deinit(void);

static pj_status_t tls_init(void)
{
    pj_status_t status;
    int ret;

    if (tls_init_count == 1)
        return PJ_SUCCESS;

    if (tls_init_count == -1) {
        /* Another thread is initializing. Spin briefly. */
        int retry;
        for (retry = 0; retry < 100 && tls_init_count == -1; ++retry)
            pj_thread_sleep(10);
        return (tls_init_count == 1) ? PJ_SUCCESS : PJ_EBUSY;
    }

    tls_init_count = -1;

    /* Register error subsystem */
    status = pj_register_strerror(PJ_ERRNO_START_USER +
                                              PJ_ERRNO_SPACE_SIZE * 6,
                                              PJ_ERRNO_SPACE_SIZE,
                                              &tls_strerror);
    pj_assert(status == PJ_SUCCESS);

    /* Init GnuTLS library */
    ret = gnutls_global_init();
    if (ret < 0) {
        tls_init_count = 0;
        return tls_status_from_err(NULL, ret);
    }

    gnutls_global_set_log_level(GNUTLS_LOG_LEVEL);
    gnutls_global_set_log_function(tls_print_logs);

    /* Init available ciphers */
    if (!ssl_cipher_num) {
        unsigned int i;

        for (i = 0; i<PJ_ARRAY_SIZE(ssl_ciphers); i++) {
            unsigned char id[2];
            const char *suite;
            
            suite = gnutls_cipher_suite_info(i, (unsigned char *)id,
                                             NULL, NULL, NULL, NULL);
            ssl_ciphers[i].id = 0;
            /* usually the array size is bigger than the number of available
             * ciphers anyway, so by checking here we can exit the loop as soon
             * as either all ciphers have been added or the array is full */
            if (suite) {
                ssl_ciphers[i].id = (pj_ssl_cipher)
                    (pj_uint32_t) ((id[0] << 8) | id[1]);
                ssl_ciphers[i].name = suite;
            } else
                break;
        }

        ssl_cipher_num = i;
    }

    /* Schedule cleanup at pj_shutdown() */
    status = pj_atexit(&tls_deinit);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(2, ("gtls", status,
                       "Failed to register GnuTLS cleanup"));
    }

    tls_init_count = 1;
    return PJ_SUCCESS;
}


/* Shutdown GnuTLS */
static void tls_deinit(void)
{
    if (tls_init_count == 1) {
        gnutls_global_deinit();
        tls_init_count = 0;
    }
}


/* Callback invoked every time a certificate has to be validated. */
static int tls_cert_verify_cb(gnutls_session_t session)
{
    pj_ssl_sock_t *ssock;
    unsigned int status;
    int ret;

    /* Get SSL socket instance */
    ssock = (pj_ssl_sock_t *)gnutls_session_get_ptr(session);
    pj_assert(ssock);

    /* Support only x509 format */
    ret = gnutls_certificate_type_get(session) != GNUTLS_CRT_X509;
    if (ret < 0) {
        ssock->verify_status |= PJ_SSL_CERT_EINVALID_FORMAT;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    /* Store verification status */
    ret = gnutls_certificate_verify_peers2(session, &status);
    if (ret < 0) {
        ssock->verify_status |= PJ_SSL_CERT_EUNKNOWN;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }
    /* Always map GnuTLS chain-trust flags into ssock->verify_status so that
     * upper layers (e.g. sip_transport_tls, which sets verify_peer=PJ_FALSE
     * and enforces certificate policy itself) can inspect the result.
     * Aborting the handshake immediately is only done when verify_peer is
     * set at the SSL-socket level. */
    if (status & GNUTLS_CERT_INVALID) {
        if (status & GNUTLS_CERT_SIGNER_NOT_FOUND)
            ssock->verify_status |= PJ_SSL_CERT_EISSUER_NOT_FOUND;
        else if (status & GNUTLS_CERT_EXPIRED ||
                 status & GNUTLS_CERT_NOT_ACTIVATED)
            ssock->verify_status |= PJ_SSL_CERT_EVALIDITY_PERIOD;
        else if (status & GNUTLS_CERT_SIGNER_NOT_CA ||
                 status & GNUTLS_CERT_INSECURE_ALGORITHM)
            ssock->verify_status |= PJ_SSL_CERT_EUNTRUSTED;
        else if (status & GNUTLS_CERT_UNEXPECTED_OWNER ||
                 status & GNUTLS_CERT_MISMATCH)
            ssock->verify_status |= PJ_SSL_CERT_EISSUER_MISMATCH;
        else if (status & GNUTLS_CERT_REVOKED)
            ssock->verify_status |= PJ_SSL_CERT_EREVOKED;
        else
            ssock->verify_status |= PJ_SSL_CERT_EUNKNOWN;

        if (ssock->param.verify_peer)
            return GNUTLS_E_CERTIFICATE_ERROR;
    }

    if (ssock->param.verify_peer) {
        gnutls_x509_crt_t cert;
        unsigned int cert_list_size;
        const gnutls_datum_t *cert_list;
        int ret2;

        ret2 = gnutls_x509_crt_init(&cert);
        if (ret2 < 0)
            goto out;

        cert_list = gnutls_certificate_get_peers(session, &cert_list_size);
        if (cert_list == NULL) {
            ret = GNUTLS_E_NO_CERTIFICATE_FOUND;
            goto out;
        }

        /* TODO: verify whole chain perhaps? */
        ret = gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER);
        if (ret < 0)
            ret = gnutls_x509_crt_import(cert, &cert_list[0],
                                         GNUTLS_X509_FMT_PEM);
        if (ret < 0) {
            ssock->verify_status |= PJ_SSL_CERT_EINVALID_FORMAT;
            goto out;
        }
        ret = gnutls_x509_crt_check_hostname(cert,
                                             ssock->param.server_name.ptr);
        if (ret < 0)
            goto out;

        gnutls_x509_crt_deinit(cert);

        /* notify GnuTLS to continue handshake normally */
        return GNUTLS_E_SUCCESS;

out:
        tls_last_error = ret;
        ssock->verify_status |= PJ_SSL_CERT_EUNKNOWN;
        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    /* verify_peer is false: allow the handshake to complete. Upper layers
     * will read ssock->verify_status to enforce their own policy. */
    return GNUTLS_E_SUCCESS;
}


/* gnutls_handshake() and gnutls_record_send() will call this function to
 * send/write (encrypted) data */
static pj_ssize_t tls_data_push(gnutls_transport_ptr_t ptr,
                             const void *data, size_t len)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)ptr;
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;

    pj_lock_acquire(ssock->ssl_write_buf_mutex);
    if (circ_write(&ssock->ssl_write_buf, data, len) != PJ_SUCCESS) {
        pj_lock_release(ssock->ssl_write_buf_mutex);

        gnutls_transport_set_errno(gssock->session, ENOMEM);
        return -1;
    }

    pj_lock_release(ssock->ssl_write_buf_mutex);

    return len;
}


/* gnutls_handshake() and gnutls_record_recv() will call this function to
 * receive/read (encrypted) data */
static pj_ssize_t tls_data_pull(gnutls_transport_ptr_t ptr,
                             void *data, pj_size_t len)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)ptr;
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;

    pj_lock_acquire(ssock->ssl_read_buf_mutex);

    if (circ_empty(&ssock->ssl_read_buf)) {
        pj_lock_release(ssock->ssl_read_buf_mutex);

        /* Data buffers not yet filled */
        gnutls_transport_set_errno(gssock->session, EAGAIN);
        return -1;
    }

    pj_size_t circ_buf_size = circ_size(&ssock->ssl_read_buf);
    pj_size_t read_size = PJ_MIN(circ_buf_size, len);

    circ_read(&ssock->ssl_read_buf, data, read_size);

    pj_lock_release(ssock->ssl_read_buf_mutex);

    return read_size;
}


/* Append a string to the priority string, only once. */
static pj_status_t tls_str_append_once(pj_str_t *dst, pj_str_t *src)
{
    if (pj_strstr(dst, src) == NULL) {
        /* Check buffer size */
        if (dst->slen + src->slen + 3 > 1024)
            return PJ_ETOOMANY;

        if (dst->slen > 0 && dst->ptr[dst->slen-1] == ':')
            pj_strcat2(dst, "+");
        else
            pj_strcat2(dst, ":+");
        pj_strcat(dst, src);
    }
    return PJ_SUCCESS;
}


/* Generate priority string with user preference order. */
static pj_status_t tls_priorities_set(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    char buf[1024];
    char priority_buf[256];
    pj_str_t cipher_list;
    pj_str_t compression = pj_str("COMP-NULL");
    pj_str_t server = pj_str(":%SERVER_PRECEDENCE");
    int i, j, ret;
    pj_str_t priority;
    const char *err;

    pj_strset(&cipher_list, buf, 0);
    pj_strset(&priority, priority_buf, 0);

    if (ssock->param.proto == PJ_SSL_SOCK_PROTO_DEFAULT) {
        ssock->param.proto = PJ_SSL_SOCK_PROTO_TLS1_2 |
                             PJ_SSL_SOCK_PROTO_TLS1_3;
    }

    /* For each level, enable only the requested protocol */
    pj_strcat2(&priority, "NORMAL:-VERS-ALL:");
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_3) {
        pj_strcat2(&priority, "+VERS-TLS1.3:");
    }
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_2) {
        pj_strcat2(&priority, "+VERS-TLS1.2:");
    }
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_1) {
        pj_strcat2(&priority, "+VERS-TLS1.1:");
    }
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1) {
        pj_strcat2(&priority, "+VERS-TLS1.0:");
    }
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_SSL3) {
        pj_strcat2(&priority, "+VERS-SSL3.0:");
    }
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_SSL2) {
        /* GnuTLS doesn't support SSLv2 */
    }

    pj_strcat(&cipher_list, &priority);
    if (ssock->param.ciphers_num > 0) {
        pj_strcat2(&cipher_list, "-CIPHER-ALL");
    }
    for (i = 0; i < (int)ssock->param.ciphers_num; i++) {
        for (j = 0; ; j++) {
            pj_ssl_cipher c;
            const char *suite;
            unsigned char id[2];
            gnutls_protocol_t proto;
            gnutls_kx_algorithm_t kx;
            gnutls_mac_algorithm_t mac;
            gnutls_cipher_algorithm_t algo;

            suite = gnutls_cipher_suite_info(j, (unsigned char *)id,
                                             &kx, &algo, &mac, &proto);
            if (!suite)
                break;

            c = (pj_ssl_cipher) (pj_uint32_t) ((id[0] << 8) | id[1]);
            if (ssock->param.ciphers[i] == c) {
                char temp[256];
                pj_str_t cipher_entry;

                /* Protocol version */
                /* We shouldn't add protocol support that's not specified
                 * in ssock->param.proto.
                 */
                /*
                pj_strset(&cipher_entry, temp, 0);
                pj_strcat2(&cipher_entry, "VERS-");
                pj_strcat2(&cipher_entry, gnutls_protocol_get_name(proto));
                ret = tls_str_append_once(&cipher_list, &cipher_entry);
                if (ret != PJ_SUCCESS)
                    return ret;
                */

                /* Cipher */
                pj_strset(&cipher_entry, temp, 0);
                pj_strcat2(&cipher_entry, gnutls_cipher_get_name(algo));
                ret = tls_str_append_once(&cipher_list, &cipher_entry);
                if (ret != PJ_SUCCESS)
                    return ret;

                /* Mac */
                pj_strset(&cipher_entry, temp, 0);
                pj_strcat2(&cipher_entry, gnutls_mac_get_name(mac));
                ret = tls_str_append_once(&cipher_list, &cipher_entry);
                if (ret != PJ_SUCCESS)
                    return ret;

                /* Key exchange */
                pj_strset(&cipher_entry, temp, 0);
                pj_strcat2(&cipher_entry, gnutls_kx_get_name(kx));
                ret = tls_str_append_once(&cipher_list, &cipher_entry);
                if (ret != PJ_SUCCESS)
                    return ret;

                /* Compression is always disabled */
                /* Signature is level-default */
                break;
            }
        }
    }

    /* Disable compression, it's a TLS-only extension after all */
    tls_str_append_once(&cipher_list, &compression);

    /* Server will be the one deciding which crypto to use */
    if (ssock->is_server) {
        if (cipher_list.slen + server.slen + 1 > (pj_ssize_t)sizeof(buf))
            return PJ_ETOOMANY;
        else
            pj_strcat(&cipher_list, &server);
    }

    /* End the string and print it */
    if (cipher_list.ptr[cipher_list.slen-1] == ':')
        cipher_list.slen--;
    cipher_list.ptr[cipher_list.slen] = '\0';
    PJ_LOG(5, (ssock->pool->obj_name, "Priority string: %s", cipher_list.ptr));

    /* Set our priority string */
    ret = gnutls_priority_set_direct(gssock->session,
                                        cipher_list.ptr, &err);
    if (ret < 0) {
        tls_last_error = GNUTLS_E_INVALID_REQUEST;
        return PJ_EINVAL;
    }

    return PJ_SUCCESS;
}


/* Load root CA file or load the installed ones. */
static pj_status_t tls_trust_set(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    int ntrusts = 0;
    int err;

    err = gnutls_certificate_set_x509_system_trust(gssock->xcred);
    if (err > 0)
        ntrusts += err;
    err = gnutls_certificate_set_x509_trust_file(gssock->xcred,
                                                 TRUST_STORE_FILE1,
                                                 GNUTLS_X509_FMT_PEM);
    if (err > 0)
        ntrusts += err;

    err = gnutls_certificate_set_x509_trust_file(gssock->xcred,
                                                 TRUST_STORE_FILE2,
                                                 GNUTLS_X509_FMT_PEM);
    if (err > 0)
        ntrusts += err;

    if (ntrusts > 0)
        return PJ_SUCCESS;
    else if (!ntrusts)
        return PJ_ENOTFOUND;
    else
        return PJ_EINVAL;
}

#if GNUTLS_VERSION_NUMBER < 0x030306

#ifdef _POSIX_PATH_MAX
#   define GNUTLS_PATH_MAX _POSIX_PATH_MAX
#else
#   define GNUTLS_PATH_MAX 256
#endif

static int gnutls_certificate_set_x509_trust_dir(
                gnutls_certificate_credentials_t cred,
                const char *dirname, unsigned type)
{
    DIR *dirp;
    struct dirent *d;
    int ret;
    int r = 0;
    char path[GNUTLS_PATH_MAX];
#ifndef _WIN32
    struct dirent e;
#endif

    dirp = opendir(dirname);
    if (dirp != NULL) {
        do {
#ifdef _WIN32
            d = readdir(dirp);
            if (d != NULL) {
#else
            ret = readdir_r(dirp, &e, &d);
            if (ret == 0 && d != NULL
#ifdef _DIRENT_HAVE_D_TYPE
                && (d->d_type == DT_REG || d->d_type == DT_LNK ||
                    d->d_type == DT_UNKNOWN)
#endif
            ) {
#endif
                snprintf(path, sizeof(path), "%s/%s",
                         dirname, d->d_name);

                ret = gnutls_certificate_set_x509_trust_file(cred, path, type);
                if (ret >= 0)
                    r += ret;
            }
        } while (d != NULL);
        closedir(dirp);
    }

    return r;
}

#endif

static pj_ssl_sock_t *ssl_alloc(pj_pool_t *pool)
{
    return (pj_ssl_sock_t *)PJ_POOL_ZALLOC_T(pool, gnutls_sock_t);
}

/* Create and initialize new GnuTLS context and instance */
static pj_status_t ssl_create(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    pj_ssl_cert_t *cert;
    pj_status_t status;
    int ret;

    /* Suppress warnings */
    PJ_UNUSED_ARG(circ_reset);
    PJ_UNUSED_ARG(circ_read_cancel);

    pj_assert(ssock);

    cert = ssock->cert;

    /* Initialize GnuTLS library (idempotent) */
    {
        pj_status_t status = tls_init();
        if (status != PJ_SUCCESS)
            return status;
    }

    /* Start this socket session */
    ret = gnutls_init(&gssock->session, ssock->is_server ? GNUTLS_SERVER
                                                        : GNUTLS_CLIENT);
    if (ret < 0)
        goto out;

    /* Set the ssock object to be retrieved by transport (send/recv) and by
     * user data from this session */
    gnutls_transport_set_ptr(gssock->session,
                             (gnutls_transport_ptr_t) (uintptr_t) ssock);
    gnutls_session_set_ptr(gssock->session,
                           (gnutls_transport_ptr_t) (uintptr_t) ssock);

    /* Initialize input circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->ssl_read_buf, 512);
    if (status != PJ_SUCCESS)
        return status;

    /* Initialize output circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->ssl_write_buf, 512);
    if (status != PJ_SUCCESS)
        return status;

    /* Set the callback that allows GnuTLS to PUSH and PULL data
     * TO and FROM the transport layer */
    gnutls_transport_set_push_function(gssock->session, tls_data_push);
    gnutls_transport_set_pull_function(gssock->session, tls_data_pull);

    /* Determine which cipher suite to support */
    status = tls_priorities_set(ssock);
    if (status != PJ_SUCCESS)
        return status;

    /* Allocate credentials for handshaking and transmission */
    ret = gnutls_certificate_allocate_credentials(&gssock->xcred);
    if (ret < 0)
        goto out;
    gnutls_certificate_set_verify_function(gssock->xcred, tls_cert_verify_cb);

    /* Load system trust file(s) */
    status = tls_trust_set(ssock);
    if (status != PJ_SUCCESS)
        return status;

    /* Load user-provided CA, certificate and key if available */
    if (cert) {
        /* Load CA if one is specified. */
        if (cert->CA_file.slen) {
            ret = gnutls_certificate_set_x509_trust_file(gssock->xcred,
                                                         cert->CA_file.ptr,
                                                         GNUTLS_X509_FMT_PEM);
            if (ret < 0)
                ret = gnutls_certificate_set_x509_trust_file(
                                gssock->xcred,
                                cert->CA_file.ptr,
                                GNUTLS_X509_FMT_DER);
            if (ret < 0)
                goto out;
        }
        if (cert->CA_path.slen) {
            ret = gnutls_certificate_set_x509_trust_dir(gssock->xcred,
                                                         cert->CA_path.ptr,
                                                         GNUTLS_X509_FMT_PEM);
            if (ret < 0)
                ret = gnutls_certificate_set_x509_trust_dir(
                                gssock->xcred,
                                cert->CA_path.ptr,
                                GNUTLS_X509_FMT_DER);
            if (ret < 0)
                goto out;
        }

        /* Load certificate, key and pass if one is specified */
        if (cert->cert_file.slen && cert->privkey_file.slen) {
            const char *prikey_file = cert->privkey_file.ptr;
            const char *prikey_pass = cert->privkey_pass.slen
                                    ? cert->privkey_pass.ptr
                                    : NULL;
            ret = gnutls_certificate_set_x509_key_file2(gssock->xcred,
                                                        cert->cert_file.ptr,
                                                        prikey_file,
                                                        GNUTLS_X509_FMT_PEM,
                                                        prikey_pass,
                                                        0);
            if (ret != GNUTLS_E_SUCCESS)
                ret = gnutls_certificate_set_x509_key_file2(gssock->xcred,
                                                            cert->cert_file.ptr,
                                                            prikey_file,
                                                            GNUTLS_X509_FMT_DER,
                                                            prikey_pass,
                                                            0);
            if (ret < 0)
                goto out;
        }

        if (cert->CA_buf.slen) {
            gnutls_datum_t ca;
            ca.data = (unsigned char*)cert->CA_buf.ptr;
            ca.size = cert->CA_buf.slen;
            ret = gnutls_certificate_set_x509_trust_mem(gssock->xcred,
                                                        &ca,
                                                        GNUTLS_X509_FMT_PEM);
            if (ret < 0)
                ret = gnutls_certificate_set_x509_trust_mem(
                                gssock->xcred, &ca, GNUTLS_X509_FMT_DER);
            if (ret < 0)
                goto out;
        }

        if (cert->cert_buf.slen && cert->privkey_buf.slen) {
            gnutls_datum_t cert_buf;
            gnutls_datum_t privkey_buf;

            cert_buf.data = (unsigned char*)cert->CA_buf.ptr;
            cert_buf.size = cert->CA_buf.slen;
            privkey_buf.data = (unsigned char*)cert->privkey_buf.ptr;
            privkey_buf.size = cert->privkey_buf.slen;

            const char *prikey_pass = cert->privkey_pass.slen
                                    ? cert->privkey_pass.ptr
                                    : NULL;
            ret = gnutls_certificate_set_x509_key_mem2(gssock->xcred,
                                                       &cert_buf,
                                                       &privkey_buf,
                                                       GNUTLS_X509_FMT_PEM,
                                                       prikey_pass,
                                                       0);
            /* Load DER format */
            /*
            if (ret != GNUTLS_E_SUCCESS)
                ret = gnutls_certificate_set_x509_key_mem2(gssock->xcred,
                                                           &cert_buf,
                                                           &privkey_buf,
                                                           GNUTLS_X509_FMT_DER,
                                                           prikey_pass,
                                                           0);
            */                                                           
            if (ret < 0)
                goto out;
        }
    }

    /* Require client certificate if asked */
    if (ssock->is_server && ssock->param.require_client_cert)
        gnutls_certificate_server_set_request(gssock->session,
                                              GNUTLS_CERT_REQUIRE);

    /* Finally set credentials for this session */
    ret = gnutls_credentials_set(gssock->session,
                                 GNUTLS_CRD_CERTIFICATE, gssock->xcred);
    if (ret < 0)
        goto out;

    ret = GNUTLS_E_SUCCESS;
out:
    return tls_status_from_err(ssock, ret);
}


/* Destroy GnuTLS credentials and session. */
static void ssl_destroy(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;

    if (gssock->session) {
        if (ssock->ssl_state == SSL_STATE_ESTABLISHED)
            gnutls_bye(gssock->session, GNUTLS_SHUT_RDWR);
        gnutls_deinit(gssock->session);
        gssock->session = NULL;
    }

    if (gssock->xcred) {
        gnutls_certificate_free_credentials(gssock->xcred);
        gssock->xcred = NULL;
    }

    /* Note: GnuTLS library stays initialized for the process lifetime.
     * tls_deinit() is called via pj_atexit() at pj_shutdown(). */

    /* Destroy circular buffers */
    circ_deinit(&ssock->ssl_read_buf);
    circ_deinit(&ssock->ssl_write_buf);
}


/* Reset socket state. */
static void ssl_reset_sock_state(pj_ssl_sock_t *ssock)
{
    pj_lock_acquire(ssock->ssl_write_buf_mutex);
    ssock->ssl_state = SSL_STATE_NULL;
    pj_lock_release(ssock->ssl_write_buf_mutex);

    ssl_close_sockets(ssock);

    ssock->last_err = tls_last_error = GNUTLS_E_SUCCESS;
}


static void ssl_ciphers_populate(void)
{
     if (!ssl_cipher_num) {
         pj_status_t status = tls_init();
         if (status != PJ_SUCCESS) {
             PJ_PERROR(1, ("gtls", status, "Failed to initialize GnuTLS"));
             return;
         }
         /* GnuTLS stays initialized — no tls_deinit() here */
     }
}


static pj_ssl_cipher ssl_get_cipher(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    int i;
    gnutls_cipher_algorithm_t lookup;
    gnutls_cipher_algorithm_t cipher;

    /* Current cipher */
    cipher = gnutls_cipher_get(gssock->session);
    for (i = 0; ; i++) {
        unsigned char id[2];
        const char *suite;
        
        suite = gnutls_cipher_suite_info(i,(unsigned char *)id, NULL,
                                         &lookup, NULL, NULL);
        if (suite) {
            if (lookup == cipher) {
                return (pj_uint32_t) ((id[0] << 8) | id[1]);
            }
        } else {
            break;
        }
    }

    return PJ_TLS_UNKNOWN_CIPHER;
}


/* Get Common Name field string from a general name string */
static void tls_cert_get_cn(const pj_str_t *gen_name, pj_str_t *cn)
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


/* Get certificate info; in case the certificate info is already populated,
 * this function will check if the contents need updating by inspecting the
 * issuer and the serial number. */
static void tls_cert_get_info(pj_pool_t *pool, pj_ssl_cert_info *ci,
                              gnutls_x509_crt_t cert)
{
    pj_bool_t update_needed;
    char buf[512] = { 0 };
    size_t bufsize = sizeof(buf);
    pj_uint8_t serial_no[64] = { 0 }; /* should be >= sizeof(ci->serial_no) */
    size_t serialsize = sizeof(serial_no);
    size_t len = sizeof(buf);
    int i, ret, seq = 0;
    pj_ssl_cert_name_type type;

    pj_assert(pool && ci && cert);

    /* Get issuer */
    gnutls_x509_crt_get_issuer_dn(cert, buf, &bufsize);

    /* Get serial no */
    gnutls_x509_crt_get_serial(cert, serial_no, &serialsize);

    /* Check if the contents need to be updated */
    update_needed = pj_strcmp2(&ci->issuer.info, buf) ||
                    pj_memcmp(ci->serial_no, serial_no, serialsize);
    if (!update_needed)
        return;

    /* Update cert info */

    pj_bzero(ci, sizeof(pj_ssl_cert_info));

    /* Version */
    ci->version = gnutls_x509_crt_get_version(cert);

    /* Issuer */
    pj_strdup2(pool, &ci->issuer.info, buf);
    tls_cert_get_cn(&ci->issuer.info, &ci->issuer.cn);

    /* Serial number */
    pj_memcpy(ci->serial_no, serial_no, sizeof(ci->serial_no));

    /* Subject */
    bufsize = sizeof(buf);
    gnutls_x509_crt_get_dn(cert, buf, &bufsize);
    pj_strdup2(pool, &ci->subject.info, buf);
    tls_cert_get_cn(&ci->subject.info, &ci->subject.cn);

    /* Validity */
    ci->validity.end.sec = (long)gnutls_x509_crt_get_expiration_time(cert);
    ci->validity.start.sec = (long)gnutls_x509_crt_get_activation_time(cert);
    ci->validity.gmt = 0;

    /* Subject Alternative Name extension */
    if (ci->version >= 3) {
        char out[256] = { 0 };
        /* Get the number of all alternate names so that we can allocate
         * the correct number of bytes in subj_alt_name */
        while (gnutls_x509_crt_get_subject_alt_name(cert, seq, out, &len,
                                                    NULL) !=
               GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
        {
            seq++;
        }

        ci->subj_alt_name.entry = pj_pool_calloc(pool, seq,
                                        sizeof(*ci->subj_alt_name.entry));
        if (!ci->subj_alt_name.entry) {
            tls_last_error = GNUTLS_E_MEMORY_ERROR;
            return;
        }

        /* Now populate the alternative names */
        for (i = 0; i < seq; i++) {
            len = sizeof(out) - 1;
            ret = gnutls_x509_crt_get_subject_alt_name(cert, i, out,
                                                       &len, NULL);

            switch (ret) {
            case GNUTLS_SAN_IPADDRESS:
                type = PJ_SSL_CERT_NAME_IP;
                pj_inet_ntop2(len == sizeof(pj_in6_addr) ? pj_AF_INET6()
                                                         : pj_AF_INET(),
                              out, buf, sizeof(buf));
                break;
            case GNUTLS_SAN_URI:
                type = PJ_SSL_CERT_NAME_URI;
                break;
            case GNUTLS_SAN_RFC822NAME:
                type = PJ_SSL_CERT_NAME_RFC822;
                break;
            case GNUTLS_SAN_DNSNAME:
                type = PJ_SSL_CERT_NAME_DNS;
                break;
            default:
                type = PJ_SSL_CERT_NAME_UNKNOWN;
                break;
            }

            if (len && type != PJ_SSL_CERT_NAME_UNKNOWN) {
                ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
                pj_strdup2(pool,
                        &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
                        type == PJ_SSL_CERT_NAME_IP ? buf : out);
                ci->subj_alt_name.cnt++;
            }
        }
        /* TODO: if no DNS alt. names were found, we could check against
         * the commonName as per RFC3280. */
    }
}

static void tls_cert_get_chain_raw(pj_pool_t *pool, pj_ssl_cert_info *ci,
                                   const gnutls_datum_t *certs,
                                   size_t certs_num)
{
    size_t i=0;
    ci->raw_chain.cert_raw = pj_pool_calloc(pool, certs_num,
                                        sizeof(*ci->raw_chain.cert_raw));
    ci->raw_chain.cnt = certs_num;
    for (i=0; i < certs_num; ++i) {
        const pj_str_t crt_raw = {(char*)certs[i].data,
                                  (pj_ssize_t)certs[i].size};
        pj_strdup(pool, ci->raw_chain.cert_raw+i, &crt_raw);
    }
}

/* Update local & remote certificates info. This function should be
 * called after handshake or renegotiation successfully completed. */
static void ssl_update_certs_info(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    gnutls_x509_crt_t cert = NULL;
    const gnutls_datum_t *us;
    const gnutls_datum_t *certs;
    unsigned int certslen = 0;
    int ret = GNUTLS_CERT_INVALID;

    pj_assert(ssock->ssl_state == SSL_STATE_ESTABLISHED);

    /* Get active local certificate */
    us = gnutls_certificate_get_ours(gssock->session);
    if (!us)
        goto us_out;

    ret = gnutls_x509_crt_init(&cert);
    if (ret < 0)
        goto us_out;
    ret = gnutls_x509_crt_import(cert, us, GNUTLS_X509_FMT_DER);
    if (ret < 0)
        ret = gnutls_x509_crt_import(cert, us, GNUTLS_X509_FMT_PEM);
    if (ret < 0)
        goto us_out;

    tls_cert_get_info(ssock->pool, &ssock->local_cert_info, cert);
    pj_pool_reset(ssock->info_pool);
    tls_cert_get_chain_raw(ssock->info_pool, &ssock->local_cert_info, us, 1);

us_out:
    tls_last_error = ret;
    if (cert)
        gnutls_x509_crt_deinit(cert);
    else
        pj_bzero(&ssock->local_cert_info, sizeof(pj_ssl_cert_info));

    cert = NULL;

    /* Get active remote certificate */
    certs = gnutls_certificate_get_peers(gssock->session, &certslen);
    if (certs == NULL || certslen == 0)
        goto peer_out;

    ret = gnutls_x509_crt_init(&cert);
    if (ret < 0)
        goto peer_out;

    ret = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_PEM);
    if (ret < 0)
        ret = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_DER);
    if (ret < 0)
        goto peer_out;

    tls_cert_get_info(ssock->pool, &ssock->remote_cert_info, cert);
    pj_pool_reset(ssock->info_pool);
    tls_cert_get_chain_raw(ssock->info_pool, &ssock->remote_cert_info, certs,
                           certslen);

peer_out:
    tls_last_error = ret;
    if (cert)
        gnutls_x509_crt_deinit(cert);
    else
        pj_bzero(&ssock->remote_cert_info, sizeof(pj_ssl_cert_info));
}

static void ssl_set_state(pj_ssl_sock_t *ssock, pj_bool_t is_server)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(is_server);
}

static void ssl_set_peer_name(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;

    /* Set server name to connect */
    if (ssock->param.server_name.slen &&
        get_ip_addr_ver(&ssock->param.server_name) == 0)
    {
        int ret;
        /* Server name is null terminated already */
        ret = gnutls_server_name_set(gssock->session, GNUTLS_NAME_DNS,
                                     ssock->param.server_name.ptr,
                                     ssock->param.server_name.slen);
        if (ret < 0) {
            PJ_LOG(3, (ssock->pool->obj_name,
                       "gnutls_server_name_set() failed: %s",
                       gnutls_strerror(ret)));
        }
    }
}

/* Try to perform an asynchronous handshake */
static pj_status_t ssl_do_handshake(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    int ret;
    pj_status_t status;

    /* Perform SSL handshake.
     * Hold write_mutex to prevent concurrent session access from
     * read and write callbacks (ioqueue may fire them in parallel).
     */
    pj_lock_acquire(ssock->write_mutex);
    ret = gnutls_handshake(gssock->session);
    pj_lock_release(ssock->write_mutex);

    if (ret == GNUTLS_E_SUCCESS) {
        /* System are GO */
        ssock->ssl_state = SSL_STATE_ESTABLISHED;
        status = PJ_SUCCESS;
    } else if (!gnutls_error_is_fatal(ret) ||
               ret == GNUTLS_E_GOT_APPLICATION_DATA)
    {
        /* Non fatal error, retry later (busy or again).
         * GNUTLS_E_GOT_APPLICATION_DATA means the peer sent application
         * data while we expected handshake records (common during
         * renegotiation).  The app data sits in GnuTLS's internal record
         * buffer and will be returned by the next gnutls_record_recv().
         * Treat it as non-fatal so the caller retries after draining the
         * buffered application data.
         */
        status = PJ_EPENDING;
    } else {
        /* Fatal error invalidates session, no fallback */
        PJ_LOG(1, ("tls", "gnutls_handshake() error: %s (%d)",
                   gnutls_strerror(ret), ret));
        status = PJ_EINVAL;
    }

    tls_last_error = ret;

    return status;
}

static pj_status_t ssl_read(pj_ssl_sock_t *ssock, void *data, int *size)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    int decrypted_size;

    /* Decrypt received data using GnuTLS (will read our input
     * circular buffer).
     * Hold write_mutex because gnutls_record_recv() may produce
     * handshake data during renegotiation (same as OpenSSL's
     * SSL_read approach).
     */
    pj_lock_acquire(ssock->write_mutex);
    decrypted_size = gnutls_record_recv(gssock->session, data, *size);
    pj_lock_release(ssock->write_mutex);
    *size = 0;
    if (decrypted_size > 0) {
        *size = decrypted_size;
        return PJ_SUCCESS;
    } else if (decrypted_size == 0) {
        /* Nothing more to read */
        return PJ_SUCCESS;
    } else if (decrypted_size == GNUTLS_E_REHANDSHAKE) {
        return PJ_ETRYAGAIN;
    } else if (decrypted_size == GNUTLS_E_AGAIN ||
               decrypted_size == GNUTLS_E_INTERRUPTED ||
               !gnutls_error_is_fatal(decrypted_size))
    {
        /* non-fatal error, let's just continue */
        return PJ_SUCCESS;
    } else {
        return PJ_ECANCELLED;
    }
}

/*
 * Write the plain data to GnuTLS, it will be encrypted by gnutls_record_send()
 * and sent via tls_data_push. Note that re-negotitation may be on progress, so
 * sending data should be delayed until re-negotiation is completed.
 * Caller must hold ssock->write_mutex.
 */
static pj_status_t ssl_write(pj_ssl_sock_t *ssock, const void *data,
                             pj_ssize_t size, int *nwritten)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    int nwritten_;
    pj_ssize_t total_written = 0;

    /* Ask GnuTLS to encrypt our plaintext now. GnuTLS will use the push
     * callback to actually write the encrypted bytes into our output circular
     * buffer. GnuTLS may refuse to "send" everything at once, but since we are
     * not really sending now, we will just call it again now until it succeeds
     * (or fails in a fatal way). */
    while (total_written < size) {
        /* Try encrypting using GnuTLS */
        nwritten_ = gnutls_record_send(gssock->session,
                                      ((char *)data) + total_written,
                                      size - total_written);

        if (nwritten_ > 0) {
            /* Good, some data was encrypted and written */
            total_written += nwritten_;
        } else {
            /* Normally we would have to retry record_send but our internal
             * state has not changed, so we have to ask for more data first.
             * We will just try again later, although this should never happen.
             */
            *nwritten = nwritten_;
            return tls_status_from_err(ssock, nwritten_);
        }
    }

    /* All encrypted data is written to the output circular buffer;
     * now send it on the socket (or notify problem). */
    *nwritten = total_written;
    return PJ_SUCCESS;
}

static pj_status_t ssl_renegotiate(pj_ssl_sock_t *ssock)
{
    gnutls_sock_t *gssock = (gnutls_sock_t *)ssock;
    int status;

    if (!ssock->is_server) {
        /* GnuTLS does not support client-initiated renegotiation.
         * gnutls_rehandshake() is server-only (sends HelloRequest),
         * and gnutls_handshake() on an established session fails.
         */
        return PJ_ENOTSUP;
    }

    /* Server: send HelloRequest to ask client to renegotiate.
     * Hold write_mutex for session access consistency with
     * ssl_do_handshake() and ssl_read().
     */
    pj_lock_acquire(ssock->write_mutex);
    status = gnutls_rehandshake(gssock->session);
    pj_lock_release(ssock->write_mutex);
    if (status == GNUTLS_E_SUCCESS)
        return PJ_SUCCESS;

    return tls_status_from_err(ssock, status);
}

#endif /* PJ_HAS_SSL_SOCK */

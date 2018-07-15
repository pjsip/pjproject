/* $Id$ */
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

#define THIS_FILE               "ssl_sock_gtls.c"

/* Workaround for ticket #985 */
#define DELAYED_CLOSE_TIMEOUT   200

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


/* TLS state enumeration. */
enum tls_connection_state {
    TLS_STATE_NULL,
    TLS_STATE_HANDSHAKING,
    TLS_STATE_ESTABLISHED
};

/* Internal timer types. */
enum timer_id {
    TIMER_NONE,
    TIMER_HANDSHAKE_TIMEOUT,
    TIMER_CLOSE
};

/* Structure of SSL socket read buffer. */
typedef struct read_data_t {
    void *data;
    pj_size_t len;
} read_data_t;

/*
 * Get the offset of pointer to read-buffer of SSL socket from read-buffer
 * of active socket. Note that both SSL socket and active socket employ
 * different but correlated read-buffers (as much as async_cnt for each),
 * and to make it easier/faster to find corresponding SSL socket's read-buffer
 * from known active socket's read-buffer, the pointer of corresponding
 * SSL socket's read-buffer is stored right after the end of active socket's
 * read-buffer.
 */
#define OFFSET_OF_READ_DATA_PTR(ssock, asock_rbuf) \
                                        (read_data_t**) \
                                        ((pj_int8_t *)(asock_rbuf) + \
                                        ssock->param.read_buffer_size)

/* Structure of SSL socket write data. */
typedef struct write_data_t {
    PJ_DECL_LIST_MEMBER(struct write_data_t);
    pj_ioqueue_op_key_t  key;
    pj_size_t            record_len;
    pj_ioqueue_op_key_t *app_key;
    pj_size_t            plain_data_len;
    pj_size_t            data_len;
    unsigned             flags;
    union {
        char             content[1];
        const char      *ptr;
    } data;
} write_data_t;


/* Structure of SSL socket write buffer (circular buffer). */
typedef struct send_buf_t {
    char                *buf;
    pj_size_t            max_len;
    char                *start;
    pj_size_t            len;
} send_buf_t;


/* Circular buffer object */
typedef struct circ_buf_t {
    pj_size_t      cap;    /* maximum number of elements (must be power of 2) */
    pj_size_t      readp;  /* index of oldest element */
    pj_size_t      writep; /* index at which to write new element  */
    pj_size_t      size;   /* number of elements */
    pj_uint8_t    *buf;    /* data buffer */
    pj_pool_t     *pool;   /* where new allocations will take place */
} circ_buf_t;


/* Secure socket structure definition. */
struct pj_ssl_sock_t {
    pj_pool_t            *pool;
    pj_ssl_sock_t        *parent;
    pj_ssl_sock_param     param;
    pj_ssl_sock_param     newsock_param;
    pj_ssl_cert_t        *cert;

    pj_ssl_cert_info      local_cert_info;
    pj_ssl_cert_info      remote_cert_info;

    pj_bool_t             is_server;
    enum tls_connection_state connection_state;
    pj_ioqueue_op_key_t   handshake_op_key;
    pj_timer_entry        timer;
    pj_status_t           verify_status;

    int                   last_err;

    pj_sock_t             sock;
    pj_activesock_t      *asock;

    pj_sockaddr           local_addr;
    pj_sockaddr           rem_addr;
    int                   addr_len;

    pj_bool_t             read_started;
    pj_size_t             read_size;
    pj_uint32_t           read_flags;
    void                **asock_rbuf;
    read_data_t          *ssock_rbuf;

    write_data_t          write_pending;       /* list of pending writes */
    write_data_t          write_pending_empty; /* cache for write_pending */
    pj_bool_t             flushing_write_pend; /* flag of flushing is ongoing */
    send_buf_t            send_buf;
    write_data_t          send_pending; /* list of pending write to network */

    gnutls_session_t      session;
    gnutls_certificate_credentials_t xcred;

    circ_buf_t            circ_buf_input;
    pj_lock_t            *circ_buf_input_mutex;

    circ_buf_t            circ_buf_output;
    pj_lock_t            *circ_buf_output_mutex;

    int                   tls_init_count; /* library initialization counter */
};

/*
 * Certificate/credential structure definition.
 */
struct pj_ssl_cert_t
{
    pj_str_t CA_file;
    pj_str_t CA_path;
    pj_str_t cert_file;
    pj_str_t privkey_file;
    pj_str_t privkey_pass;

    /* Certificate buffer. */
    pj_ssl_cert_buffer CA_buf;
    pj_ssl_cert_buffer cert_buf;
    pj_ssl_cert_buffer privkey_buf;
};


/* GnuTLS available ciphers */
static unsigned tls_available_ciphers;

/* Array of id/names for available ciphers */
static struct tls_ciphers_t {
    pj_ssl_cipher id;
    const char *name;
} tls_ciphers[MAX_CIPHERS];

/* Last error reported somehow */
static int tls_last_error;


/*
 *******************************************************************
 * Circular buffer functions.
 *******************************************************************
 */

static pj_status_t circ_init(pj_pool_factory *factory,
                             circ_buf_t *cb, pj_size_t cap)
{
    cb->cap    = cap;
    cb->readp  = 0;
    cb->writep = 0;
    cb->size   = 0;

    /* Initial pool holding the buffer elements */
    cb->pool = pj_pool_create(factory, "tls-circ%p", cap, cap, NULL);
    if (!cb->pool)
        return PJ_ENOMEM;

    /* Allocate circular buffer */
    cb->buf = pj_pool_alloc(cb->pool, cap);
    if (!cb->buf) {
        pj_pool_release(cb->pool);
        return PJ_ENOMEM;
    }

    return PJ_SUCCESS;
}

static void circ_deinit(circ_buf_t *cb)
{
    if (cb->pool) {
        pj_pool_release(cb->pool);
        cb->pool = NULL;
    }
}

static pj_bool_t circ_empty(const circ_buf_t *cb)
{
    return cb->size == 0;
}

static pj_size_t circ_size(const circ_buf_t *cb)
{
    return cb->size;
}

static pj_size_t circ_avail(const circ_buf_t *cb)
{
    return cb->cap - cb->size;
}

static void circ_read(circ_buf_t *cb, pj_uint8_t *dst, pj_size_t len)
{
    pj_size_t size_after = cb->cap - cb->readp;
    pj_size_t tbc = PJ_MIN(size_after, len);
    pj_size_t rem = len - tbc;

    pj_memcpy(dst, cb->buf + cb->readp, tbc);
    pj_memcpy(dst + tbc, cb->buf, rem);

    cb->readp += len;
    cb->readp &= (cb->cap - 1);

    cb->size -= len;
}

static pj_status_t circ_write(circ_buf_t *cb,
                              const pj_uint8_t *src, pj_size_t len)
{
    /* Overflow condition: resize */
    if (len > circ_avail(cb)) {
        /* Minimum required capacity */
        pj_size_t min_cap = len + cb->size;

        /* Next 32-bit power of two */
        min_cap--;
        min_cap |= min_cap >> 1;
        min_cap |= min_cap >> 2;
        min_cap |= min_cap >> 4;
        min_cap |= min_cap >> 8;
        min_cap |= min_cap >> 16;
        min_cap++;

        /* Create a new pool to hold a bigger buffer, using the same factory */
        pj_pool_t *pool = pj_pool_create(cb->pool->factory, "tls-circ%p",
                                         min_cap, min_cap, NULL);
        if (!pool)
            return PJ_ENOMEM;

        /* Allocate our new buffer */
        pj_uint8_t *buf = pj_pool_alloc(pool, min_cap);
        if (!buf) {
            pj_pool_release(pool);
            return PJ_ENOMEM;
        }

        /* Save old size, which we shall restore after the next read */
        pj_size_t old_size = cb->size;

        /* Copy old data into beginning of new buffer */
        circ_read(cb, buf, cb->size);

        /* Restore old size now */
        cb->size = old_size;

        /* Release the previous pool */
        pj_pool_release(cb->pool);

        /* Update circular buffer members */
        cb->pool = pool;
        cb->buf = buf;
        cb->readp = 0;
        cb->writep = cb->size;
        cb->cap = min_cap;
    }

    pj_size_t size_after = cb->cap - cb->writep;
    pj_size_t tbc = PJ_MIN(size_after, len);
    pj_size_t rem = len - tbc;

    pj_memcpy(cb->buf + cb->writep, src, tbc);
    pj_memcpy(cb->buf, src + tbc, rem);

    cb->writep += len;
    cb->writep &= (cb->cap - 1);

    cb->size += len;

    return PJ_SUCCESS;
}


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
    pj_str_t errstr;
    const char *tmp = gnutls_strerror(tls_last_error);

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)
    if (tmp) {
        pj_ansi_strncpy(buf, tmp, bufsize);
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
static pj_status_t tls_init(void)
{
    /* Register error subsystem */
    pj_status_t status = pj_register_strerror(PJ_ERRNO_START_USER +
                                              PJ_ERRNO_SPACE_SIZE * 6,
                                              PJ_ERRNO_SPACE_SIZE,
                                              &tls_strerror);
    pj_assert(status == PJ_SUCCESS);

    /* Init GnuTLS library */
    int ret = gnutls_global_init();
    if (ret < 0)
        return tls_status_from_err(NULL, ret);

    gnutls_global_set_log_level(GNUTLS_LOG_LEVEL);
    gnutls_global_set_log_function(tls_print_logs);

    /* Init available ciphers */
    if (!tls_available_ciphers) {
        unsigned int i;

        for (i = 0; ; i++) {
            unsigned char id[2];
            const char *suite;
            
            suite = gnutls_cipher_suite_info(i, (unsigned char *)id,
                                             NULL, NULL, NULL, NULL);
            tls_ciphers[i].id = 0;
            /* usually the array size is bigger than the number of available
             * ciphers anyway, so by checking here we can exit the loop as soon
             * as either all ciphers have been added or the array is full */
            if (suite && i < PJ_ARRAY_SIZE(tls_ciphers)) {
                tls_ciphers[i].id = (pj_ssl_cipher)
                    (pj_uint32_t) ((id[0] << 8) | id[1]);
                tls_ciphers[i].name = suite;
            } else
                break;
        }

        tls_available_ciphers = i;
    }

    return PJ_SUCCESS;
}


/* Shutdown GnuTLS */
static void tls_deinit(void)
{
    gnutls_global_deinit();
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
    if (ssock->param.verify_peer) {
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

        return GNUTLS_E_CERTIFICATE_ERROR;
    }

    /* When verification is not requested just return ok here, however
     * applications can still get the verification status. */
        gnutls_x509_crt_t cert;
        unsigned int cert_list_size;
        const gnutls_datum_t *cert_list;
        int ret;

        ret = gnutls_x509_crt_init(&cert);
        if (ret < 0)
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

    return GNUTLS_E_SUCCESS;
}


/* gnutls_handshake() and gnutls_record_send() will call this function to
 * send/write (encrypted) data */
static ssize_t tls_data_push(gnutls_transport_ptr_t ptr,
                             const void *data, size_t len)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)ptr;

    pj_lock_acquire(ssock->circ_buf_output_mutex);
    if (circ_write(&ssock->circ_buf_output, data, len) != PJ_SUCCESS) {
        pj_lock_release(ssock->circ_buf_output_mutex);

        gnutls_transport_set_errno(ssock->session, ENOMEM);
        return -1;
    }

    pj_lock_release(ssock->circ_buf_output_mutex);

    return len;
}


/* gnutls_handshake() and gnutls_record_recv() will call this function to
 * receive/read (encrypted) data */
static ssize_t tls_data_pull(gnutls_transport_ptr_t ptr,
                             void *data, pj_size_t len)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)ptr;

    pj_lock_acquire(ssock->circ_buf_input_mutex);

    if (circ_empty(&ssock->circ_buf_input)) {
        pj_lock_release(ssock->circ_buf_input_mutex);

        /* Data buffers not yet filled */
        gnutls_transport_set_errno(ssock->session, EAGAIN);
        return -1;
    }

    pj_size_t circ_buf_size = circ_size(&ssock->circ_buf_input);
    pj_size_t read_size = PJ_MIN(circ_buf_size, len);

    circ_read(&ssock->circ_buf_input, data, read_size);

    pj_lock_release(ssock->circ_buf_input_mutex);

    return read_size;
}


/* Append a string to the priority string, only once. */
static pj_status_t tls_str_append_once(pj_str_t *dst, pj_str_t *src)
{
    if (pj_strstr(dst, src) == NULL) {
        /* Check buffer size */
        if (dst->slen + src->slen + 3 > 1024)
            return PJ_ETOOMANY;

        pj_strcat2(dst, ":+");
        pj_strcat(dst, src);
    }
    return PJ_SUCCESS;
}


/* Generate priority string with user preference order. */
static pj_status_t tls_priorities_set(pj_ssl_sock_t *ssock)
{
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

    /* For each level, enable only the requested protocol */
    pj_strcat2(&priority, "NORMAL:");
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_2) {
        pj_strcat2(&priority, "+VERS-TLS1.2:");
    }
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_1) {
        pj_strcat2(&priority, "+VERS-TLS1.1:");
    }
    if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1) {
        pj_strcat2(&priority, "+VERS-TLS1.0:");
    }
    pj_strcat2(&priority, "-VERS-SSL3.0:");
    pj_strcat2(&priority, "%LATEST_RECORD_VERSION");

    pj_strcat(&cipher_list, &priority);
    for (i = 0; i < ssock->param.ciphers_num; i++) {
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
                pj_strset(&cipher_entry, temp, 0);
                pj_strcat2(&cipher_entry, "VERS-");
                pj_strcat2(&cipher_entry, gnutls_protocol_get_name(proto));
                ret = tls_str_append_once(&cipher_list, &cipher_entry);
                if (ret != PJ_SUCCESS)
                    return ret;

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
        if (cipher_list.slen + server.slen + 1 > sizeof(buf))
            return PJ_ETOOMANY;
        else
            pj_strcat(&cipher_list, &server);
    }

    /* End the string and print it */
    cipher_list.ptr[cipher_list.slen] = '\0';
    PJ_LOG(5, (ssock->pool->obj_name, "Priority string: %s", cipher_list.ptr));

    /* Set our priority string */
    ret = gnutls_priority_set_direct(ssock->session,
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
    int ntrusts = 0;
    int err;

    err = gnutls_certificate_set_x509_system_trust(ssock->xcred);
    if (err > 0)
        ntrusts += err;
    err = gnutls_certificate_set_x509_trust_file(ssock->xcred,
                                                 TRUST_STORE_FILE1,
                                                 GNUTLS_X509_FMT_PEM);
    if (err > 0)
        ntrusts += err;

    err = gnutls_certificate_set_x509_trust_file(ssock->xcred,
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

/* Create and initialize new GnuTLS context and instance */
static pj_status_t tls_open(pj_ssl_sock_t *ssock)
{
    pj_ssl_cert_t *cert;
    pj_status_t status;
    int ret;

    pj_assert(ssock);

    cert = ssock->cert;

    /* Even if reopening is harmless, having one instance only simplifies
     * deallocating it later on */
    if (!ssock->tls_init_count) {
        ssock->tls_init_count++;
        ret = tls_init();
        if (ret < 0)
            return ret;
    } else
        return PJ_SUCCESS;

    /* Start this socket session */
    ret = gnutls_init(&ssock->session, ssock->is_server ? GNUTLS_SERVER
                                                        : GNUTLS_CLIENT);
    if (ret < 0)
        goto out;

    /* Set the ssock object to be retrieved by transport (send/recv) and by
     * user data from this session */
    gnutls_transport_set_ptr(ssock->session,
                             (gnutls_transport_ptr_t) (uintptr_t) ssock);
    gnutls_session_set_ptr(ssock->session,
                           (gnutls_transport_ptr_t) (uintptr_t) ssock);

    /* Initialize input circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_input, 512);
    if (status != PJ_SUCCESS)
        return status;

    /* Initialize output circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_output, 512);
    if (status != PJ_SUCCESS)
        return status;

    /* Set the callback that allows GnuTLS to PUSH and PULL data
     * TO and FROM the transport layer */
    gnutls_transport_set_push_function(ssock->session, tls_data_push);
    gnutls_transport_set_pull_function(ssock->session, tls_data_pull);

    /* Determine which cipher suite to support */
    status = tls_priorities_set(ssock);
    if (status != PJ_SUCCESS)
        return status;

    /* Allocate credentials for handshaking and transmission */
    ret = gnutls_certificate_allocate_credentials(&ssock->xcred);
    if (ret < 0)
        goto out;
    gnutls_certificate_set_verify_function(ssock->xcred, tls_cert_verify_cb);

    /* Load system trust file(s) */
    status = tls_trust_set(ssock);
    if (status != PJ_SUCCESS)
        return status;

    /* Load user-provided CA, certificate and key if available */
    if (cert) {
        /* Load CA if one is specified. */
        if (cert->CA_file.slen) {
            ret = gnutls_certificate_set_x509_trust_file(ssock->xcred,
                                                         cert->CA_file.ptr,
                                                         GNUTLS_X509_FMT_PEM);
            if (ret < 0)
                ret = gnutls_certificate_set_x509_trust_file(
                		ssock->xcred,
                                cert->CA_file.ptr,
                                GNUTLS_X509_FMT_DER);
            if (ret < 0)
                goto out;
        }
        if (cert->CA_path.slen) {
            ret = gnutls_certificate_set_x509_trust_dir(ssock->xcred,
                                                         cert->CA_path.ptr,
                                                         GNUTLS_X509_FMT_PEM);
            if (ret < 0)
                ret = gnutls_certificate_set_x509_trust_dir(
                		ssock->xcred,
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
            ret = gnutls_certificate_set_x509_key_file2(ssock->xcred,
                                                        cert->cert_file.ptr,
                                                        prikey_file,
                                                        GNUTLS_X509_FMT_PEM,
                                                        prikey_pass,
                                                        0);
            if (ret != GNUTLS_E_SUCCESS)
                ret = gnutls_certificate_set_x509_key_file2(ssock->xcred,
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
            ret = gnutls_certificate_set_x509_trust_mem(ssock->xcred,
                                                        &ca,
                                                        GNUTLS_X509_FMT_PEM);
            if (ret < 0)
                ret = gnutls_certificate_set_x509_trust_mem(
                		ssock->xcred, &ca, GNUTLS_X509_FMT_DER);
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
            ret = gnutls_certificate_set_x509_key_mem2(ssock->xcred,
                                                       &cert_buf,
                                                       &privkey_buf,
                                                       GNUTLS_X509_FMT_PEM,
                                                       prikey_pass,
                                                       0);
            /* Load DER format */
            /*
            if (ret != GNUTLS_E_SUCCESS)
                ret = gnutls_certificate_set_x509_key_mem2(ssock->xcred,
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
        gnutls_certificate_server_set_request(ssock->session,
                                              GNUTLS_CERT_REQUIRE);

    /* Finally set credentials for this session */
    ret = gnutls_credentials_set(ssock->session,
                                 GNUTLS_CRD_CERTIFICATE, ssock->xcred);
    if (ret < 0)
        goto out;

    ret = GNUTLS_E_SUCCESS;
out:
    return tls_status_from_err(ssock, ret);
}


/* Destroy GnuTLS credentials and session. */
static void tls_close(pj_ssl_sock_t *ssock)
{
    if (ssock->session) {
        gnutls_bye(ssock->session, GNUTLS_SHUT_RDWR);
        gnutls_deinit(ssock->session);
        ssock->session = NULL;
    }

    if (ssock->xcred) {
        gnutls_certificate_free_credentials(ssock->xcred);
        ssock->xcred = NULL;
    }

    /* Free GnuTLS library */
    if (ssock->tls_init_count) {
        ssock->tls_init_count--;
        tls_deinit();
    }

    /* Destroy circular buffers */
    circ_deinit(&ssock->circ_buf_input);
    circ_deinit(&ssock->circ_buf_output);
}


/* Reset socket state. */
static void tls_sock_reset(pj_ssl_sock_t *ssock)
{
    ssock->connection_state = TLS_STATE_NULL;

    tls_close(ssock);

    if (ssock->asock) {
        pj_activesock_close(ssock->asock);
        ssock->asock = NULL;
        ssock->sock = PJ_INVALID_SOCKET;
    }
    if (ssock->sock != PJ_INVALID_SOCKET) {
        pj_sock_close(ssock->sock);
        ssock->sock = PJ_INVALID_SOCKET;
    }

    ssock->last_err = tls_last_error = GNUTLS_E_SUCCESS;
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
    ci->validity.end.sec = gnutls_x509_crt_get_expiration_time(cert);
    ci->validity.start.sec = gnutls_x509_crt_get_activation_time(cert);
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
static void tls_cert_update(pj_ssl_sock_t *ssock)
{
    gnutls_x509_crt_t cert = NULL;
    const gnutls_datum_t *us;
    const gnutls_datum_t *certs;
    unsigned int certslen = 0;
    int ret = GNUTLS_CERT_INVALID;

    pj_assert(ssock->connection_state == TLS_STATE_ESTABLISHED);

    /* Get active local certificate */
    us = gnutls_certificate_get_ours(ssock->session);
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
    tls_cert_get_chain_raw(ssock->pool, &ssock->local_cert_info, us, 1);

us_out:
    tls_last_error = ret;
    if (cert)
        gnutls_x509_crt_deinit(cert);
    else
        pj_bzero(&ssock->local_cert_info, sizeof(pj_ssl_cert_info));

    cert = NULL;

    /* Get active remote certificate */
    certs = gnutls_certificate_get_peers(ssock->session, &certslen);
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
    tls_cert_get_chain_raw(ssock->pool, &ssock->remote_cert_info, certs,
    			   certslen);

peer_out:
    tls_last_error = ret;
    if (cert)
        gnutls_x509_crt_deinit(cert);
    else
        pj_bzero(&ssock->remote_cert_info, sizeof(pj_ssl_cert_info));
}


/* When handshake completed:
 * - notify application
 * - if handshake failed, reset SSL state
 * - return PJ_FALSE when SSL socket instance is destroyed by application. */
static pj_bool_t on_handshake_complete(pj_ssl_sock_t *ssock,
                                       pj_status_t status)
{
    pj_bool_t ret = PJ_TRUE;

    /* Cancel handshake timer */
    if (ssock->timer.id == TIMER_HANDSHAKE_TIMEOUT) {
        pj_timer_heap_cancel(ssock->param.timer_heap, &ssock->timer);
        ssock->timer.id = TIMER_NONE;
    }

    /* Update certificates info on successful handshake */
    if (status == PJ_SUCCESS)
        tls_cert_update(ssock);

    /* Accepting */
    if (ssock->is_server) {
        if (status != PJ_SUCCESS) {
            /* Handshake failed in accepting, destroy our self silently. */

            char errmsg[PJ_ERR_MSG_SIZE];
            char buf[PJ_INET6_ADDRSTRLEN + 10];

            pj_strerror(status, errmsg, sizeof(errmsg));
            PJ_LOG(3, (ssock->pool->obj_name,
                       "Handshake failed in accepting %s: %s",
                       pj_sockaddr_print(&ssock->rem_addr, buf,sizeof(buf),3),
                       errmsg));

            /* Workaround for ticket #985 */
#if (defined(PJ_WIN32)&& PJ_WIN32 != 0) || (defined(PJ_WIN64)&& PJ_WIN64 != 0)
            if (ssock->param.timer_heap) {
                pj_time_val interval = {0, DELAYED_CLOSE_TIMEOUT};

                tls_sock_reset(ssock);

                ssock->timer.id = TIMER_CLOSE;
                pj_time_val_normalize(&interval);
                if (pj_timer_heap_schedule(ssock->param.timer_heap,
                                           &ssock->timer, &interval) != 0)
                {
                    ssock->timer.id = TIMER_NONE;
                    pj_ssl_sock_close(ssock);
                }
            } else
#endif /* PJ_WIN32 */
            {
                pj_ssl_sock_close(ssock);
            }

            return PJ_FALSE;
        }
        /* Notify application the newly accepted SSL socket */
        if (ssock->param.cb.on_accept_complete)
            ret = (*ssock->param.cb.on_accept_complete)
                      (ssock->parent, ssock, (pj_sockaddr_t*)&ssock->rem_addr,
                       pj_sockaddr_get_len((pj_sockaddr_t*)&ssock->rem_addr));

    } else { /* Connecting */
        /* On failure, reset SSL socket state first, as app may try to
         * reconnect in the callback. */
        if (status != PJ_SUCCESS) {
            /* Server disconnected us, possibly due to negotiation failure */
            tls_sock_reset(ssock);
        }
        if (ssock->param.cb.on_connect_complete) {

            ret = (*ssock->param.cb.on_connect_complete)(ssock, status);
        }
    }

    return ret;
}

static write_data_t *alloc_send_data(pj_ssl_sock_t *ssock, pj_size_t len)
{
    send_buf_t *send_buf = &ssock->send_buf;
    pj_size_t avail_len, skipped_len = 0;
    char *reg1, *reg2;
    pj_size_t reg1_len, reg2_len;
    write_data_t *p;

    /* Check buffer availability */
    avail_len = send_buf->max_len - send_buf->len;
    if (avail_len < len)
        return NULL;

    /* If buffer empty, reset start pointer and return it */
    if (send_buf->len == 0) {
        send_buf->start = send_buf->buf;
        send_buf->len   = len;
        p = (write_data_t*)send_buf->start;
        goto init_send_data;
    }

    /* Free space may be wrapped/splitted into two regions, so let's
     * analyze them if any region can hold the write data. */
    reg1 = send_buf->start + send_buf->len;
    if (reg1 >= send_buf->buf + send_buf->max_len)
        reg1 -= send_buf->max_len;
        reg1_len = send_buf->max_len - send_buf->len;
    if (reg1 + reg1_len > send_buf->buf + send_buf->max_len) {
        reg1_len = send_buf->buf + send_buf->max_len - reg1;
        reg2 = send_buf->buf;
        reg2_len = send_buf->start - send_buf->buf;
    } else {
        reg2 = NULL;
        reg2_len = 0;
    }

    /* More buffer availability check, note that the write data must be in
     * a contigue buffer. */
    avail_len = PJ_MAX(reg1_len, reg2_len);
    if (avail_len < len)
    return NULL;

    /* Get the data slot */
    if (reg1_len >= len) {
        p = (write_data_t*)reg1;
    } else {
        p = (write_data_t*)reg2;
        skipped_len = reg1_len;
    }

    /* Update buffer length */
    send_buf->len += len + skipped_len;

init_send_data:
    /* Init the new send data */
    pj_bzero(p, sizeof(*p));
    pj_list_init(p);
    pj_list_push_back(&ssock->send_pending, p);

    return p;
}

static void free_send_data(pj_ssl_sock_t *ssock, write_data_t *wdata)
{
    send_buf_t *buf = &ssock->send_buf;
    write_data_t *spl = &ssock->send_pending;

    pj_assert(!pj_list_empty(&ssock->send_pending));

    /* Free slot from the buffer */
    if (spl->next == wdata && spl->prev == wdata) {
    /* This is the only data, reset the buffer */
    buf->start = buf->buf;
    buf->len = 0;
    } else if (spl->next == wdata) {
    /* This is the first data, shift start pointer of the buffer and
     * adjust the buffer length.
     */
    buf->start = (char*)wdata->next;
    if (wdata->next > wdata) {
        buf->len -= ((char*)wdata->next - buf->start);
    } else {
        /* Overlapped */
        pj_size_t right_len, left_len;
        right_len = buf->buf + buf->max_len - (char*)wdata;
        left_len  = (char*)wdata->next - buf->buf;
        buf->len -= (right_len + left_len);
    }
    } else if (spl->prev == wdata) {
    /* This is the last data, just adjust the buffer length */
    if (wdata->prev < wdata) {
        pj_size_t jump_len;
        jump_len = (char*)wdata -
               ((char*)wdata->prev + wdata->prev->record_len);
        buf->len -= (wdata->record_len + jump_len);
    } else {
        /* Overlapped */
        pj_size_t right_len, left_len;
        right_len = buf->buf + buf->max_len -
            ((char*)wdata->prev + wdata->prev->record_len);
        left_len  = (char*)wdata + wdata->record_len - buf->buf;
        buf->len -= (right_len + left_len);
    }
    }
    /* For data in the middle buffer, just do nothing on the buffer. The slot
     * will be freed later when freeing the first/last data. */

    /* Remove the data from send pending list */
    pj_list_erase(wdata);
}

#if 0
/* Just for testing send buffer alloc/free */
#include <pj/rand.h>
pj_status_t pj_ssl_sock_ossl_test_send_buf(pj_pool_t *pool)
{
    enum { MAX_CHUNK_NUM = 20 };
    unsigned chunk_size, chunk_cnt, i;
    write_data_t *wdata[MAX_CHUNK_NUM] = {0};
    pj_time_val now;
    pj_ssl_sock_t *ssock = NULL;
    pj_ssl_sock_param param;
    pj_status_t status;

    pj_gettimeofday(&now);
    pj_srand((unsigned)now.sec);

    pj_ssl_sock_param_default(&param);
    status = pj_ssl_sock_create(pool, &param, &ssock);
    if (status != PJ_SUCCESS) {
        return status;
    }

    if (ssock->send_buf.max_len == 0) {
        ssock->send_buf.buf = (char *)
                              pj_pool_alloc(ssock->pool,
                                            ssock->param.send_buffer_size);
        ssock->send_buf.max_len = ssock->param.send_buffer_size;
        ssock->send_buf.start = ssock->send_buf.buf;
        ssock->send_buf.len = 0;
    }

    chunk_size = ssock->param.send_buffer_size / MAX_CHUNK_NUM / 2;
    chunk_cnt = 0;
    for (i = 0; i < MAX_CHUNK_NUM; i++) {
        wdata[i] = alloc_send_data(ssock, pj_rand() % chunk_size + 321);
        if (wdata[i])
            chunk_cnt++;
        else
            break;
    }

    while (chunk_cnt) {
        i = pj_rand() % MAX_CHUNK_NUM;
        if (wdata[i]) {
            free_send_data(ssock, wdata[i]);
            wdata[i] = NULL;
            chunk_cnt--;
        }
    }

    if (ssock->send_buf.len != 0)
        status = PJ_EBUG;

    pj_ssl_sock_close(ssock);
    return status;
}
#endif

/* Flush write circular buffer to network socket. */
static pj_status_t flush_circ_buf_output(pj_ssl_sock_t *ssock,
                                         pj_ioqueue_op_key_t *send_key,
                                         pj_size_t orig_len, unsigned flags)
{
    pj_ssize_t len;
    write_data_t *wdata;
    pj_size_t needed_len;
    pj_status_t status;

    pj_lock_acquire(ssock->circ_buf_output_mutex);

    /* Check if there is data in the circular buffer, flush it if any */
    if (circ_empty(&ssock->circ_buf_output)) {
        pj_lock_release(ssock->circ_buf_output_mutex);

        return PJ_SUCCESS;
    }

    len = circ_size(&ssock->circ_buf_output);

    /* Calculate buffer size needed, and align it to 8 */
    needed_len = len + sizeof(write_data_t);
    needed_len = ((needed_len + 7) >> 3) << 3;

    /* Allocate buffer for send data */
    wdata = alloc_send_data(ssock, needed_len);
    if (wdata == NULL) {
        pj_lock_release(ssock->circ_buf_output_mutex);
        return PJ_ENOMEM;
    }

    /* Copy the data and set its properties into the send data */
    pj_ioqueue_op_key_init(&wdata->key, sizeof(pj_ioqueue_op_key_t));
    wdata->key.user_data = wdata;
    wdata->app_key = send_key;
    wdata->record_len = needed_len;
    wdata->data_len = len;
    wdata->plain_data_len = orig_len;
    wdata->flags = flags;
    circ_read(&ssock->circ_buf_output, (pj_uint8_t *)&wdata->data, len);

    /* Ticket #1573: Don't hold mutex while calling PJLIB socket send(). */
    pj_lock_release(ssock->circ_buf_output_mutex);

    /* Send it */
    if (ssock->param.sock_type == pj_SOCK_STREAM()) {
        status = pj_activesock_send(ssock->asock, &wdata->key,
                                    wdata->data.content, &len,
                                    flags);
    } else {
        status = pj_activesock_sendto(ssock->asock, &wdata->key,
                                      wdata->data.content, &len,
                                      flags,
                                      (pj_sockaddr_t*)&ssock->rem_addr,
                                      ssock->addr_len);
    }

    if (status != PJ_EPENDING) {
        /* When the sending is not pending, remove the wdata from send
         * pending list. */
        pj_lock_acquire(ssock->circ_buf_output_mutex);
        free_send_data(ssock, wdata);
        pj_lock_release(ssock->circ_buf_output_mutex);
    }

    return status;
}

static void on_timer(pj_timer_heap_t *th, struct pj_timer_entry *te)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)te->user_data;
    int timer_id = te->id;

    te->id = TIMER_NONE;

    PJ_UNUSED_ARG(th);

    switch (timer_id) {
    case TIMER_HANDSHAKE_TIMEOUT:
        PJ_LOG(1, (ssock->pool->obj_name, "TLS timeout after %d.%ds",
                   ssock->param.timeout.sec, ssock->param.timeout.msec));

        on_handshake_complete(ssock, PJ_ETIMEDOUT);
        break;
    case TIMER_CLOSE:
        pj_ssl_sock_close(ssock);
        break;
    default:
        pj_assert(!"Unknown timer");
        break;
    }
}


/* Try to perform an asynchronous handshake */
static pj_status_t tls_try_handshake(pj_ssl_sock_t *ssock)
{
    int ret;
    pj_status_t status;

    /* Perform SSL handshake */
    ret = gnutls_handshake(ssock->session);

    status = flush_circ_buf_output(ssock, &ssock->handshake_op_key, 0, 0);
    if (status != PJ_SUCCESS)
        return status;

    if (ret == GNUTLS_E_SUCCESS) {
        /* System are GO */
        ssock->connection_state = TLS_STATE_ESTABLISHED;
        status = PJ_SUCCESS;
    } else if (!gnutls_error_is_fatal(ret)) {
        /* Non fatal error, retry later (busy or again) */
        status = PJ_EPENDING;
    } else {
        /* Fatal error invalidates session, no fallback */
        status = PJ_EINVAL;
    }

    tls_last_error = ret;

    return status;
}


/*
 *******************************************************************
 * Active socket callbacks.
 *******************************************************************
 */

/* PJ_TRUE asks the socket to read more data, PJ_FALSE takes it off the queue */
static pj_bool_t asock_on_data_read(pj_activesock_t *asock, void *data,
                                    pj_size_t size, pj_status_t status,
                                    pj_size_t *remainder)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)
                           pj_activesock_get_user_data(asock);

    pj_size_t app_remainder = 0;

    if (data && size > 0) {
        /* Push data into input circular buffer (for GnuTLS) */
        pj_lock_acquire(ssock->circ_buf_input_mutex);
        circ_write(&ssock->circ_buf_input, data, size);
        pj_lock_release(ssock->circ_buf_input_mutex);
    }

    /* Check if SSL handshake hasn't finished yet */
    if (ssock->connection_state == TLS_STATE_HANDSHAKING) {
        pj_bool_t ret = PJ_TRUE;

        if (status == PJ_SUCCESS)
            status = tls_try_handshake(ssock);

        /* Not pending is either success or failed */
        if (status != PJ_EPENDING)
            ret = on_handshake_complete(ssock, status);

        return ret;
    }

    /* See if there is any decrypted data for the application */
    if (ssock->read_started) {
        do {
            /* Get read data structure at the end of the data */
            read_data_t *app_read_data =
            	*(OFFSET_OF_READ_DATA_PTR(ssock, data));
            int app_data_size = (int)(ssock->read_size - app_read_data->len);

            /* Decrypt received data using GnuTLS (will read our input
             * circular buffer) */
            int decrypted_size = gnutls_record_recv(ssock->session,
                                	((read_data_t *)app_read_data->data) +
                                         app_read_data->len,
                                         app_data_size);

            if (decrypted_size > 0 || status != PJ_SUCCESS) {
                if (ssock->param.cb.on_data_read) {
                    pj_bool_t ret;
                    app_remainder = 0;

                    if (decrypted_size > 0)
                        app_read_data->len += decrypted_size;

                    ret = (*ssock->param.cb.on_data_read)(ssock,
                                                          app_read_data->data,
                                                          app_read_data->len,
                                                          status,
                                                          &app_remainder);

                    if (!ret) {
                        /* We've been destroyed */
                        return PJ_FALSE;
                    }

                    /* Application may have left some data to be consumed
                     * later as remainder */
                    app_read_data->len = app_remainder;
                }

                /* Active socket signalled connection closed/error, this has
                 * been signalled to the application along with any remaining
                 * buffer. So, let's just reset SSL socket now.  */
                if (status != PJ_SUCCESS) {
                    tls_sock_reset(ssock);
                    return PJ_FALSE;
                }
            } else if (decrypted_size == 0) {
                /* Nothing more to read */

                return PJ_TRUE;
            } else if (decrypted_size == GNUTLS_E_AGAIN ||
                       decrypted_size == GNUTLS_E_INTERRUPTED) {
                return PJ_TRUE;
            } else if (decrypted_size == GNUTLS_E_REHANDSHAKE) {
                /* Seems like we are renegotiating */
                pj_status_t try_handshake_status = tls_try_handshake(ssock);

                /* Not pending is either success or failed */
                if (try_handshake_status != PJ_EPENDING) {
                    if (!on_handshake_complete(ssock, try_handshake_status)) {
                        return PJ_FALSE;
                    }
                }

                if (try_handshake_status != PJ_SUCCESS &&
                    try_handshake_status != PJ_EPENDING) {
                    return PJ_FALSE;
                }
            } else if (!gnutls_error_is_fatal(decrypted_size)) {
                /* non-fatal error, let's just continue */
            } else {
                return PJ_FALSE;
            }
        } while (PJ_TRUE);
    }

    return PJ_TRUE;
}


/* Callback every time new data is available from the active socket */
static pj_bool_t asock_on_data_sent(pj_activesock_t *asock,
                                    pj_ioqueue_op_key_t *send_key,
                                    pj_ssize_t sent)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t *)pj_activesock_get_user_data(asock);

    PJ_UNUSED_ARG(send_key);
    PJ_UNUSED_ARG(sent);

    if (ssock->connection_state == TLS_STATE_HANDSHAKING) {
        /* Initial handshaking */
        pj_status_t status = tls_try_handshake(ssock);

        /* Not pending is either success or failed */
        if (status != PJ_EPENDING)
            return on_handshake_complete(ssock, status);

    } else if (send_key != &ssock->handshake_op_key) {
        /* Some data has been sent, notify application */
        write_data_t *wdata = (write_data_t*)send_key->user_data;
        if (ssock->param.cb.on_data_sent) {
            pj_bool_t ret;
            pj_ssize_t sent_len;

            sent_len = sent > 0 ? wdata->plain_data_len : sent;

            ret = (*ssock->param.cb.on_data_sent)(ssock, wdata->app_key,
                                                  sent_len);
            if (!ret) {
                /* We've been destroyed */
                return PJ_FALSE;
            }
        }

        /* Update write buffer state */
        pj_lock_acquire(ssock->circ_buf_output_mutex);
        free_send_data(ssock, wdata);
        pj_lock_release(ssock->circ_buf_output_mutex);
    } else {
        /* SSL re-negotiation is on-progress, just do nothing */
        /* FIXME: check if this is valid for GnuTLS too */
    }

    return PJ_TRUE;
}


/* Callback every time a new connection has been accepted (server) */
static pj_bool_t asock_on_accept_complete(pj_activesock_t *asock,
                                          pj_sock_t newsock,
                                          const pj_sockaddr_t *src_addr,
                                          int src_addr_len)
{
    pj_ssl_sock_t *ssock_parent = (pj_ssl_sock_t *)
                                  pj_activesock_get_user_data(asock);

    pj_ssl_sock_t *ssock;
    pj_activesock_cb asock_cb;
    pj_activesock_cfg asock_cfg;
    unsigned int i;
    pj_status_t status;

    PJ_UNUSED_ARG(src_addr_len);

    /* Create new SSL socket instance */
    status = pj_ssl_sock_create(ssock_parent->pool,
    				&ssock_parent->newsock_param,
                                &ssock);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Update new SSL socket attributes */
    ssock->sock = newsock;
    ssock->parent = ssock_parent;
    ssock->is_server = PJ_TRUE;
    if (ssock_parent->cert) {
        status = pj_ssl_sock_set_certificate(ssock, ssock->pool,
                                             ssock_parent->cert);
        if (status != PJ_SUCCESS)
            goto on_return;
    }

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(ssock->sock, ssock->param.qos_type,
                                &ssock->param.qos_params, 1,
                                ssock->pool->obj_name, NULL);
    if (status != PJ_SUCCESS && !ssock->param.qos_ignore_error)
        goto on_return;

    /* Update local address */
    ssock->addr_len = src_addr_len;
    status = pj_sock_getsockname(ssock->sock, &ssock->local_addr,
                                 &ssock->addr_len);
    if (status != PJ_SUCCESS) {
        /* This fails on few envs, e.g: win IOCP, just tolerate this and
         * use parent local address instead.
         */
        pj_sockaddr_cp(&ssock->local_addr, &ssock_parent->local_addr);
    }

    /* Set remote address */
    pj_sockaddr_cp(&ssock->rem_addr, src_addr);

    /* Create SSL context */
    status = tls_open(ssock);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Prepare read buffer */
    ssock->asock_rbuf = (void **)pj_pool_calloc(ssock->pool,
                                                ssock->param.async_cnt,
                                                sizeof(void*));
    if (!ssock->asock_rbuf)
        return PJ_ENOMEM;

    for (i = 0; i < ssock->param.async_cnt; ++i) {
        ssock->asock_rbuf[i] = (void *)pj_pool_alloc(
                                            ssock->pool,
                                            ssock->param.read_buffer_size +
                                            sizeof(read_data_t*));
        if (!ssock->asock_rbuf[i])
            return PJ_ENOMEM;
    }

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = PJ_TRUE;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = asock_on_data_read;
    asock_cb.on_data_sent = asock_on_data_sent;

    status = pj_activesock_create(ssock->pool,
                                  ssock->sock,
                                  ssock->param.sock_type,
                                  &asock_cfg,
                                  ssock->param.ioqueue,
                                  &asock_cb,
                                  ssock,
                                  &ssock->asock);

    if (status != PJ_SUCCESS)
        goto on_return;

    /* Start reading */
    status = pj_activesock_start_read2(ssock->asock, ssock->pool,
                                       (unsigned)ssock->param.read_buffer_size,
                                       ssock->asock_rbuf,
                                       PJ_IOQUEUE_ALWAYS_ASYNC);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Prepare write/send state */
    pj_assert(ssock->send_buf.max_len == 0);
    ssock->send_buf.buf = (char *)pj_pool_alloc(ssock->pool,
                                                ssock->param.send_buffer_size);
    if (!ssock->send_buf.buf)
        return PJ_ENOMEM;

    ssock->send_buf.max_len = ssock->param.send_buffer_size;
    ssock->send_buf.start = ssock->send_buf.buf;
    ssock->send_buf.len = 0;

    /* Start handshake timer */
    if (ssock->param.timer_heap &&
        (ssock->param.timeout.sec != 0 || ssock->param.timeout.msec != 0)) {
        pj_assert(ssock->timer.id == TIMER_NONE);
        ssock->timer.id = TIMER_HANDSHAKE_TIMEOUT;
        status = pj_timer_heap_schedule(ssock->param.timer_heap,
                                        &ssock->timer,
                                        &ssock->param.timeout);
        if (status != PJ_SUCCESS)
            ssock->timer.id = TIMER_NONE;
    }

    /* Start SSL handshake */
    ssock->connection_state = TLS_STATE_HANDSHAKING;

    status = tls_try_handshake(ssock);

on_return:
    if (ssock && status != PJ_EPENDING)
        on_handshake_complete(ssock, status);

    /* Must return PJ_TRUE whatever happened, as active socket must
     * continue listening.
     */
    return PJ_TRUE;
}


/* Callback every time a new connection has been completed (client) */
static pj_bool_t asock_on_connect_complete (pj_activesock_t *asock,
                                            pj_status_t status)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)
                           pj_activesock_get_user_data(asock);

    unsigned int i;
    int ret;

    if (status != PJ_SUCCESS)
        goto on_return;

    /* Update local address */
    ssock->addr_len = sizeof(pj_sockaddr);
    status = pj_sock_getsockname(ssock->sock, &ssock->local_addr,
                                 &ssock->addr_len);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Create SSL context */
    status = tls_open(ssock);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Prepare read buffer */
    ssock->asock_rbuf = (void **)pj_pool_calloc(ssock->pool,
                                                ssock->param.async_cnt,
                                                sizeof(void *));
    if (!ssock->asock_rbuf)
        return PJ_ENOMEM;

    for (i = 0; i < ssock->param.async_cnt; ++i) {
        ssock->asock_rbuf[i] = (void *)pj_pool_alloc(
                                            ssock->pool,
                                            ssock->param.read_buffer_size +
                                            sizeof(read_data_t *));
        if (!ssock->asock_rbuf[i])
            return PJ_ENOMEM;
    }

    /* Start read */
    status = pj_activesock_start_read2(ssock->asock, ssock->pool,
                                       (unsigned)ssock->param.read_buffer_size,
                                       ssock->asock_rbuf,
                                       PJ_IOQUEUE_ALWAYS_ASYNC);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Prepare write/send state */
    pj_assert(ssock->send_buf.max_len == 0);
    ssock->send_buf.buf = (char *)pj_pool_alloc(ssock->pool,
                                                ssock->param.send_buffer_size);
    if (!ssock->send_buf.buf)
        return PJ_ENOMEM;

    ssock->send_buf.max_len = ssock->param.send_buffer_size;
    ssock->send_buf.start = ssock->send_buf.buf;
    ssock->send_buf.len = 0;

    /* Set server name to connect */
    if (ssock->param.server_name.slen) {
        /* Server name is null terminated already */
        ret = gnutls_server_name_set(ssock->session, GNUTLS_NAME_DNS,
                                     ssock->param.server_name.ptr,
                                     ssock->param.server_name.slen);
        if (ret < 0) {
            PJ_LOG(3, (ssock->pool->obj_name,
                       "gnutls_server_name_set() failed: %s",
                       gnutls_strerror(ret)));
        }
    }

    /* Start handshake */
    ssock->connection_state = TLS_STATE_HANDSHAKING;

    status = tls_try_handshake(ssock);
    if (status != PJ_EPENDING)
        goto on_return;

    return PJ_TRUE;

on_return:
    return on_handshake_complete(ssock, status);
}

static void tls_ciphers_fill(void)
{
     if (!tls_available_ciphers) {
         tls_init();
         tls_deinit();
     }
}

/*
 *******************************************************************
 * API
 *******************************************************************
 */

/* Load credentials from files. */
PJ_DEF(pj_status_t) pj_ssl_cert_load_from_files(pj_pool_t *pool,
                                                const pj_str_t *CA_file,
                                                const pj_str_t *cert_file,
                                                const pj_str_t *privkey_file,
                                                const pj_str_t *privkey_pass,
                                                pj_ssl_cert_t **p_cert)
{
    return pj_ssl_cert_load_from_files2(pool, CA_file, NULL, cert_file,
                    privkey_file, privkey_pass, p_cert);
}

/* Load credentials from files. */
PJ_DEF(pj_status_t) pj_ssl_cert_load_from_files2(
                        pj_pool_t *pool,
                        const pj_str_t *CA_file,
                        const pj_str_t *CA_path,
                        const pj_str_t *cert_file,
                        const pj_str_t *privkey_file,
                        const pj_str_t *privkey_pass,
                        pj_ssl_cert_t **p_cert)
{
    pj_ssl_cert_t *cert;

    PJ_ASSERT_RETURN(pool && (CA_file || CA_path) && cert_file &&
             privkey_file,
             PJ_EINVAL);

    cert = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    if (CA_file) {
        pj_strdup_with_null(pool, &cert->CA_file, CA_file);
    }
    if (CA_path) {
        pj_strdup_with_null(pool, &cert->CA_path, CA_path);
    }
    pj_strdup_with_null(pool, &cert->cert_file, cert_file);
    pj_strdup_with_null(pool, &cert->privkey_file, privkey_file);
    pj_strdup_with_null(pool, &cert->privkey_pass, privkey_pass);

    *p_cert = cert;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ssl_cert_load_from_buffer(pj_pool_t *pool,
					const pj_ssl_cert_buffer *CA_buf,
					const pj_ssl_cert_buffer *cert_buf,
					const pj_ssl_cert_buffer *privkey_buf,
					const pj_str_t *privkey_pass,
					pj_ssl_cert_t **p_cert)
{
    pj_ssl_cert_t *cert;

    PJ_ASSERT_RETURN(pool && CA_buf && cert_buf && privkey_buf, PJ_EINVAL);

    cert = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    pj_strdup(pool, &cert->CA_buf, CA_buf);
    pj_strdup(pool, &cert->cert_buf, cert_buf);
    pj_strdup(pool, &cert->privkey_buf, privkey_buf);
    pj_strdup_with_null(pool, &cert->privkey_pass, privkey_pass);

    *p_cert = cert;

    return PJ_SUCCESS;
}

/* Store credentials. */
PJ_DEF(pj_status_t) pj_ssl_sock_set_certificate( pj_ssl_sock_t *ssock,
                                                 pj_pool_t *pool,
                                                 const pj_ssl_cert_t *cert)
{
    pj_ssl_cert_t *cert_;

    PJ_ASSERT_RETURN(ssock && pool && cert, PJ_EINVAL);

    cert_ = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    pj_memcpy(cert_, cert, sizeof(cert));
    pj_strdup_with_null(pool, &cert_->CA_file, &cert->CA_file);
    pj_strdup_with_null(pool, &cert_->CA_path, &cert->CA_path);
    pj_strdup_with_null(pool, &cert_->cert_file, &cert->cert_file);
    pj_strdup_with_null(pool, &cert_->privkey_file, &cert->privkey_file);
    pj_strdup_with_null(pool, &cert_->privkey_pass, &cert->privkey_pass);

    pj_strdup(pool, &cert_->CA_buf, &cert->CA_buf);
    pj_strdup(pool, &cert_->cert_buf, &cert->cert_buf);
    pj_strdup(pool, &cert_->privkey_buf, &cert->privkey_buf);

    ssock->cert = cert_;

    return PJ_SUCCESS;
}


/* Get available ciphers. */
PJ_DEF(pj_status_t) pj_ssl_cipher_get_availables(pj_ssl_cipher ciphers[],
                                                 unsigned *cipher_num)
{
    unsigned int i;

    PJ_ASSERT_RETURN(ciphers && cipher_num, PJ_EINVAL);

    tls_ciphers_fill();

    if (!tls_available_ciphers) {
        *cipher_num = 0;
        return PJ_ENOTFOUND;
    }

    *cipher_num = PJ_MIN(*cipher_num, tls_available_ciphers);

    for (i = 0; i < *cipher_num; ++i)
        ciphers[i] = tls_ciphers[i].id;

    return PJ_SUCCESS;
}


/* Get cipher name string. */
PJ_DEF(const char *)pj_ssl_cipher_name(pj_ssl_cipher cipher)
{
    unsigned int i;

    tls_ciphers_fill();

    for (i = 0; i < tls_available_ciphers; ++i) {
        if (cipher == tls_ciphers[i].id)
            return tls_ciphers[i].name;
    }

    return NULL;
}


/* Get cipher identifier. */
PJ_DEF(pj_ssl_cipher) pj_ssl_cipher_id(const char *cipher_name)
{
    unsigned int i;

    tls_ciphers_fill();

    for (i = 0; i < tls_available_ciphers; ++i) {
        if (!pj_ansi_stricmp(tls_ciphers[i].name, cipher_name))
            return tls_ciphers[i].id;
    }

    return PJ_TLS_UNKNOWN_CIPHER;
}


/* Check if the specified cipher is supported by the TLS backend. */
PJ_DEF(pj_bool_t) pj_ssl_cipher_is_supported(pj_ssl_cipher cipher)
{
    unsigned int i;

    tls_ciphers_fill();

    for (i = 0; i < tls_available_ciphers; ++i) {
        if (cipher == tls_ciphers[i].id)
            return PJ_TRUE;
    }

    return PJ_FALSE;
}

/* Create SSL socket instance. */
PJ_DEF(pj_status_t) pj_ssl_sock_create(pj_pool_t *pool,
                                       const pj_ssl_sock_param *param,
                                       pj_ssl_sock_t **p_ssock)
{
    pj_ssl_sock_t *ssock;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && param && p_ssock, PJ_EINVAL);
    PJ_ASSERT_RETURN(param->sock_type == pj_SOCK_STREAM(), PJ_ENOTSUP);

    pool = pj_pool_create(pool->factory, "tls%p", 512, 512, NULL);

    /* Create secure socket */
    ssock = PJ_POOL_ZALLOC_T(pool, pj_ssl_sock_t);
    ssock->pool = pool;
    ssock->sock = PJ_INVALID_SOCKET;
    ssock->connection_state = TLS_STATE_NULL;
    pj_list_init(&ssock->write_pending);
    pj_list_init(&ssock->write_pending_empty);
    pj_list_init(&ssock->send_pending);
    pj_timer_entry_init(&ssock->timer, 0, ssock, &on_timer);
    pj_ioqueue_op_key_init(&ssock->handshake_op_key,
                           sizeof(pj_ioqueue_op_key_t));

    /* Create secure socket mutex */
    status = pj_lock_create_recursive_mutex(pool, pool->obj_name,
                                            &ssock->circ_buf_output_mutex);
    if (status != PJ_SUCCESS)
        return status;

    /* Create input circular buffer mutex */
    status = pj_lock_create_simple_mutex(pool, pool->obj_name,
                                         &ssock->circ_buf_input_mutex);
    if (status != PJ_SUCCESS)
        return status;

    /* Create output circular buffer mutex */
    status = pj_lock_create_simple_mutex(pool, pool->obj_name,
                                         &ssock->circ_buf_output_mutex);
    if (status != PJ_SUCCESS)
        return status;

    /* Init secure socket param */
    ssock->param = *param;
    ssock->param.read_buffer_size = ((ssock->param.read_buffer_size + 7) >> 3)
    				     << 3;

    if (param->ciphers_num > 0) {
        unsigned int i;
        ssock->param.ciphers = (pj_ssl_cipher *)
                               pj_pool_calloc(pool, param->ciphers_num,
                                              sizeof(pj_ssl_cipher));
        if (!ssock->param.ciphers)
            return PJ_ENOMEM;

        for (i = 0; i < param->ciphers_num; ++i)
            ssock->param.ciphers[i] = param->ciphers[i];
    }

    /* Server name must be null-terminated */
    pj_strdup_with_null(pool, &ssock->param.server_name, &param->server_name);

    /* Finally */
    *p_ssock = ssock;

    return PJ_SUCCESS;
}


/*
 * Close the secure socket. This will unregister the socket from the
 * ioqueue and ultimately close the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_close(pj_ssl_sock_t *ssock)
{
    pj_pool_t *pool;

    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);

    if (!ssock->pool)
        return PJ_SUCCESS;

    if (ssock->timer.id != TIMER_NONE) {
        pj_timer_heap_cancel(ssock->param.timer_heap, &ssock->timer);
        ssock->timer.id = TIMER_NONE;
    }

    tls_sock_reset(ssock);

    pj_lock_destroy(ssock->circ_buf_output_mutex);
    pj_lock_destroy(ssock->circ_buf_input_mutex);

    pool = ssock->pool;
    ssock->pool = NULL;
    if (pool)
        pj_pool_release(pool);

    return PJ_SUCCESS;
}


/* Associate arbitrary data with the secure socket. */
PJ_DEF(pj_status_t) pj_ssl_sock_set_user_data(pj_ssl_sock_t *ssock,
                                              void *user_data)
{
    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);

    ssock->param.user_data = user_data;
    return PJ_SUCCESS;
}


/* Retrieve the user data previously associated with this secure socket. */
PJ_DEF(void *)pj_ssl_sock_get_user_data(pj_ssl_sock_t *ssock)
{
    PJ_ASSERT_RETURN(ssock, NULL);

    return ssock->param.user_data;
}


/* Retrieve the local address and port used by specified SSL socket. */
PJ_DEF(pj_status_t) pj_ssl_sock_get_info (pj_ssl_sock_t *ssock,
                                          pj_ssl_sock_info *info)
{
    pj_bzero(info, sizeof(*info));

    /* Established flag */
    info->established = (ssock->connection_state == TLS_STATE_ESTABLISHED);

    /* Protocol */
    info->proto = ssock->param.proto;

    /* Local address */
    pj_sockaddr_cp(&info->local_addr, &ssock->local_addr);

    if (info->established) {
        int i;
        gnutls_cipher_algorithm_t lookup;
        gnutls_cipher_algorithm_t cipher;

        /* Current cipher */
        cipher = gnutls_cipher_get(ssock->session);
        for (i = 0; ; i++) {
            unsigned char id[2];
            const char *suite = gnutls_cipher_suite_info(i,(unsigned char *)id,
                                                         NULL, &lookup, NULL,
                                                         NULL);
            if (suite) {
                if (lookup == cipher) {
                    info->cipher = (pj_uint32_t) ((id[0] << 8) | id[1]);
                    break;
                }
            } else
                break;
        }

        /* Remote address */
        pj_sockaddr_cp(&info->remote_addr, &ssock->rem_addr);

        /* Certificates info */
        info->local_cert_info = &ssock->local_cert_info;
        info->remote_cert_info = &ssock->remote_cert_info;

        /* Verification status */
        info->verify_status = ssock->verify_status;
    }

    /* Last known GnuTLS error code */
    info->last_native_err = ssock->last_err;

    return PJ_SUCCESS;
}


/* Starts read operation on this secure socket. */
PJ_DEF(pj_status_t) pj_ssl_sock_start_read(pj_ssl_sock_t *ssock,
                                           pj_pool_t *pool,
                                           unsigned buff_size,
                                           pj_uint32_t flags)
{
    void **readbuf;
    unsigned int i;

    PJ_ASSERT_RETURN(ssock && pool && buff_size, PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->connection_state == TLS_STATE_ESTABLISHED,
                     PJ_EINVALIDOP);

    readbuf = (void**) pj_pool_calloc(pool, ssock->param.async_cnt,
                                      sizeof(void *));
    if (!readbuf)
        return PJ_ENOMEM;

    for (i = 0; i < ssock->param.async_cnt; ++i) {
        readbuf[i] = pj_pool_alloc(pool, buff_size);
        if (!readbuf[i])
            return PJ_ENOMEM;
    }

    return pj_ssl_sock_start_read2(ssock, pool, buff_size, readbuf, flags);
}


/*
 * Same as #pj_ssl_sock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_read2 (pj_ssl_sock_t *ssock,
                                             pj_pool_t *pool,
                                             unsigned buff_size,
                                             void *readbuf[],
                                             pj_uint32_t flags)
{
    unsigned int i;

    PJ_ASSERT_RETURN(ssock && pool && buff_size && readbuf, PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->connection_state == TLS_STATE_ESTABLISHED,
                     PJ_EINVALIDOP);

    /* Create SSL socket read buffer */
    ssock->ssock_rbuf = (read_data_t*)pj_pool_calloc(pool,
                                                     ssock->param.async_cnt,
                                                     sizeof(read_data_t));
    if (!ssock->ssock_rbuf)
        return PJ_ENOMEM;

    /* Store SSL socket read buffer pointer in the activesock read buffer */
    for (i = 0; i < ssock->param.async_cnt; ++i) {
        read_data_t **p_ssock_rbuf =
                        OFFSET_OF_READ_DATA_PTR(ssock, ssock->asock_rbuf[i]);

        ssock->ssock_rbuf[i].data = readbuf[i];
        ssock->ssock_rbuf[i].len = 0;

        *p_ssock_rbuf = &ssock->ssock_rbuf[i];
    }

    ssock->read_size = buff_size;
    ssock->read_started = PJ_TRUE;
    ssock->read_flags = flags;

    return PJ_SUCCESS;
}


/*
 * Same as pj_ssl_sock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_recvfrom (pj_ssl_sock_t *ssock,
                                                pj_pool_t *pool,
                                                unsigned buff_size,
                                                pj_uint32_t flags)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(buff_size);
    PJ_UNUSED_ARG(flags);

    return PJ_ENOTSUP;
}


/*
 * Same as #pj_ssl_sock_start_recvfrom() except that the recvfrom()
 * operation takes the buffer from the argument rather than creating
 * new ones.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_recvfrom2 (pj_ssl_sock_t *ssock,
                                                 pj_pool_t *pool,
                                                 unsigned buff_size,
                                                 void *readbuf[],
                                                 pj_uint32_t flags)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(buff_size);
    PJ_UNUSED_ARG(readbuf);
    PJ_UNUSED_ARG(flags);

    return PJ_ENOTSUP;
}


/*
 * Write the plain data to GnuTLS, it will be encrypted by gnutls_record_send()
 * and sent via tls_data_push. Note that re-negotitation may be on progress, so
 * sending data should be delayed until re-negotiation is completed.
 */
static pj_status_t tls_write(pj_ssl_sock_t *ssock,
                             pj_ioqueue_op_key_t *send_key,
                             const void *data, pj_ssize_t size, unsigned flags)
{
    pj_status_t status;
    int nwritten;
    pj_ssize_t total_written = 0;

    /* Ask GnuTLS to encrypt our plaintext now. GnuTLS will use the push
     * callback to actually write the encrypted bytes into our output circular
     * buffer. GnuTLS may refuse to "send" everything at once, but since we are
     * not really sending now, we will just call it again now until it succeeds
     * (or fails in a fatal way). */
    while (total_written < size) {
        /* Try encrypting using GnuTLS */
        nwritten = gnutls_record_send(ssock->session,
        			      ((read_data_t *)data) + total_written,
                                      size);

        if (nwritten > 0) {
            /* Good, some data was encrypted and written */
            total_written += nwritten;
        } else {
            /* Normally we would have to retry record_send but our internal
             * state has not changed, so we have to ask for more data first.
             * We will just try again later, although this should never happen.
             */
            return tls_status_from_err(ssock, nwritten);
        }
    }

    /* All encrypted data is written to the output circular buffer;
     * now send it on the socket (or notify problem). */
    if (total_written == size)
        status = flush_circ_buf_output(ssock, send_key, size, flags);
    else
        status = PJ_ENOMEM;

    return status;
}


/* Flush delayed data sending in the write pending list. */
static pj_status_t flush_delayed_send(pj_ssl_sock_t *ssock)
{
    /* Check for another ongoing flush */
    if (ssock->flushing_write_pend) {
        return PJ_EBUSY;
    }

    pj_lock_acquire(ssock->circ_buf_output_mutex);

    /* Again, check for another ongoing flush */
    if (ssock->flushing_write_pend) {
        pj_lock_release(ssock->circ_buf_output_mutex);
        return PJ_EBUSY;
    }

    /* Set ongoing flush flag */
    ssock->flushing_write_pend = PJ_TRUE;

    while (!pj_list_empty(&ssock->write_pending)) {
        write_data_t *wp;
        pj_status_t status;

        wp = ssock->write_pending.next;

        /* Ticket #1573: Don't hold mutex while calling socket send. */
        pj_lock_release(ssock->circ_buf_output_mutex);

        status = tls_write(ssock, &wp->key, wp->data.ptr,
                           wp->plain_data_len, wp->flags);
        if (status != PJ_SUCCESS) {
            /* Reset ongoing flush flag first. */
            ssock->flushing_write_pend = PJ_FALSE;
            return status;
        }

        pj_lock_acquire(ssock->circ_buf_output_mutex);
        pj_list_erase(wp);
        pj_list_push_back(&ssock->write_pending_empty, wp);
    }

    /* Reset ongoing flush flag */
    ssock->flushing_write_pend = PJ_FALSE;

    pj_lock_release(ssock->circ_buf_output_mutex);

    return PJ_SUCCESS;
}


/* Sending is delayed, push back the sending data into pending list. */
static pj_status_t delay_send(pj_ssl_sock_t *ssock,
                              pj_ioqueue_op_key_t *send_key,
                              const void *data, pj_ssize_t size,
                              unsigned flags)
{
    write_data_t *wp;

    pj_lock_acquire(ssock->circ_buf_output_mutex);

    /* Init write pending instance */
    if (!pj_list_empty(&ssock->write_pending_empty)) {
        wp = ssock->write_pending_empty.next;
        pj_list_erase(wp);
    } else {
        wp = PJ_POOL_ZALLOC_T(ssock->pool, write_data_t);
    }

    wp->app_key = send_key;
    wp->plain_data_len = size;
    wp->data.ptr = data;
    wp->flags = flags;

    pj_list_push_back(&ssock->write_pending, wp);

    pj_lock_release(ssock->circ_buf_output_mutex);

    /* Must return PJ_EPENDING */
    return PJ_EPENDING;
}


/**
 * Send data using the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_send(pj_ssl_sock_t *ssock,
                                     pj_ioqueue_op_key_t *send_key,
                                     const void *data, pj_ssize_t *size,
                                     unsigned flags)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(ssock && data && size && (*size > 0), PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->connection_state==TLS_STATE_ESTABLISHED,
                     PJ_EINVALIDOP);

    /* Flush delayed send first. Sending data might be delayed when
     * re-negotiation is on-progress. */
    status = flush_delayed_send(ssock);
    if (status == PJ_EBUSY) {
        /* Re-negotiation or flushing is on progress, delay sending */
        status = delay_send(ssock, send_key, data, *size, flags);
        goto on_return;
    } else if (status != PJ_SUCCESS) {
        goto on_return;
    }

    /* Write data to SSL */
    status = tls_write(ssock, send_key, data, *size, flags);
    if (status == PJ_EBUSY) {
        /* Re-negotiation is on progress, delay sending */
        status = delay_send(ssock, send_key, data, *size, flags);
    }

on_return:
    return status;
}


/**
 * Send datagram using the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_sendto (pj_ssl_sock_t *ssock,
                                        pj_ioqueue_op_key_t *send_key,
                                        const void *data, pj_ssize_t *size,
                                        unsigned flags,
                                        const pj_sockaddr_t *addr, int addr_len)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(send_key);
    PJ_UNUSED_ARG(data);
    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(addr);
    PJ_UNUSED_ARG(addr_len);

    return PJ_ENOTSUP;
}

/**
 * Starts asynchronous socket accept() operations on this secure socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_accept (pj_ssl_sock_t *ssock,
                          pj_pool_t *pool,
                          const pj_sockaddr_t *localaddr,
                          int addr_len)
{
    return pj_ssl_sock_start_accept2(ssock, pool, localaddr, addr_len,
                         &ssock->param);
}

/**
 * Starts asynchronous socket accept() operations on this secure socket.
 */
PJ_DEF(pj_status_t)
pj_ssl_sock_start_accept2 (pj_ssl_sock_t *ssock,
                           pj_pool_t *pool,
                           const pj_sockaddr_t *localaddr,
                           int addr_len,
                           const pj_ssl_sock_param *newsock_param)
{
    pj_activesock_cb asock_cb;
    pj_activesock_cfg asock_cfg;
    pj_status_t status;

    PJ_ASSERT_RETURN(ssock && pool && localaddr && addr_len, PJ_EINVAL);

    /* Verify new socket parameters */
    if (newsock_param->grp_lock != ssock->param.grp_lock ||
        newsock_param->sock_af != ssock->param.sock_af ||
        newsock_param->sock_type != ssock->param.sock_type)
    {
        return PJ_EINVAL;
    }

    /* Create socket */
    status = pj_sock_socket(ssock->param.sock_af, ssock->param.sock_type, 0,
                            &ssock->sock);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Apply SO_REUSEADDR */
    if (ssock->param.reuse_addr) {
        int enabled = 1;
        status = pj_sock_setsockopt(ssock->sock, pj_SOL_SOCKET(),
                                    pj_SO_REUSEADDR(),
                                    &enabled, sizeof(enabled));
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(ssock->pool->obj_name, status,
                         "Warning: error applying SO_REUSEADDR"));
        }
    }

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(ssock->sock, ssock->param.qos_type,
                                &ssock->param.qos_params, 2,
                                ssock->pool->obj_name, NULL);
    if (status != PJ_SUCCESS && !ssock->param.qos_ignore_error)
        goto on_error;

    /* Bind socket */
    status = pj_sock_bind(ssock->sock, localaddr, addr_len);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Start listening to the address */
    status = pj_sock_listen(ssock->sock, PJ_SOMAXCONN);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = PJ_TRUE;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_accept_complete = asock_on_accept_complete;

    status = pj_activesock_create(pool,
                                  ssock->sock,
                                  ssock->param.sock_type,
                                  &asock_cfg,
                                  ssock->param.ioqueue,
                                  &asock_cb,
                                  ssock,
                                  &ssock->asock);

    if (status != PJ_SUCCESS)
        goto on_error;

    /* Start accepting */
    pj_ssl_sock_param_copy(pool, &ssock->newsock_param, newsock_param);
    status = pj_activesock_start_accept(ssock->asock, pool);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Update local address */
    ssock->addr_len = addr_len;
    status = pj_sock_getsockname(ssock->sock, &ssock->local_addr,
                                 &ssock->addr_len);
    if (status != PJ_SUCCESS)
        pj_sockaddr_cp(&ssock->local_addr, localaddr);

    ssock->is_server = PJ_TRUE;

    return PJ_SUCCESS;

on_error:
    tls_sock_reset(ssock);
    return status;
}


/**
 * Starts asynchronous socket connect() operation.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_connect( pj_ssl_sock_t *ssock,
                                               pj_pool_t *pool,
                                               const pj_sockaddr_t *localaddr,
                                               const pj_sockaddr_t *remaddr,
                                               int addr_len)
{
    pj_activesock_cb asock_cb;
    pj_activesock_cfg asock_cfg;
    pj_status_t status;

    PJ_ASSERT_RETURN(ssock && pool && localaddr && remaddr && addr_len,
                     PJ_EINVAL);

    /* Create socket */
    status = pj_sock_socket(ssock->param.sock_af, ssock->param.sock_type, 0,
                            &ssock->sock);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(ssock->sock, ssock->param.qos_type,
                                &ssock->param.qos_params, 2,
                                ssock->pool->obj_name, NULL);
    if (status != PJ_SUCCESS && !ssock->param.qos_ignore_error)
        goto on_error;

    /* Bind socket */
    status = pj_sock_bind(ssock->sock, localaddr, addr_len);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = PJ_TRUE;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_connect_complete = asock_on_connect_complete;
    asock_cb.on_data_read = asock_on_data_read;
    asock_cb.on_data_sent = asock_on_data_sent;

    status = pj_activesock_create(pool,
                                  ssock->sock,
                                  ssock->param.sock_type,
                                  &asock_cfg,
                                  ssock->param.ioqueue,
                                  &asock_cb,
                                  ssock,
                                  &ssock->asock);

    if (status != PJ_SUCCESS)
        goto on_error;

    /* Save remote address */
    pj_sockaddr_cp(&ssock->rem_addr, remaddr);

    /* Start timer */
    if (ssock->param.timer_heap &&
        (ssock->param.timeout.sec != 0 || ssock->param.timeout.msec != 0))
    {
        pj_assert(ssock->timer.id == TIMER_NONE);
        ssock->timer.id = TIMER_HANDSHAKE_TIMEOUT;
        status = pj_timer_heap_schedule(ssock->param.timer_heap,
                                        &ssock->timer,
                                        &ssock->param.timeout);
        if (status != PJ_SUCCESS)
            ssock->timer.id = TIMER_NONE;
    }

    status = pj_activesock_start_connect(ssock->asock, pool, remaddr,
                                         addr_len);

    if (status == PJ_SUCCESS)
        asock_on_connect_complete(ssock->asock, PJ_SUCCESS);
    else if (status != PJ_EPENDING)
        goto on_error;

    /* Update local address */
    ssock->addr_len = addr_len;
    status = pj_sock_getsockname(ssock->sock, &ssock->local_addr,
                                 &ssock->addr_len);
    /* Note that we may not get an IP address here. This can
     * happen for example on Windows, where getsockname()
     * would return 0.0.0.0 if socket has just started the
     * async connect. In this case, just leave the local
     * address with 0.0.0.0 for now; it will be updated
     * once the socket is established.
     */

    /* Update socket state */
    ssock->is_server = PJ_FALSE;

    return PJ_EPENDING;

on_error:
    tls_sock_reset(ssock);
    return status;
}


PJ_DEF(pj_status_t) pj_ssl_sock_renegotiate(pj_ssl_sock_t *ssock)
{
    int status;

    /* Nothing established yet */
    PJ_ASSERT_RETURN(ssock->connection_state == TLS_STATE_ESTABLISHED,
                     PJ_EINVALIDOP);

    /* Cannot renegotiate; we're a client */
    /* FIXME: in fact maybe that's not true */
    PJ_ASSERT_RETURN(!ssock->is_server, PJ_EINVALIDOP);

    /* First call gnutls_rehandshake() to see if this is even possible */
    status = gnutls_rehandshake(ssock->session);

    if (status == GNUTLS_E_SUCCESS) {
        /* Rehandshake is possible, so try a GnuTLS handshake now. The eventual
         * gnutls_record_recv() calls could return a few specific values during
         * this state:
         *
         *   - GNUTLS_E_REHANDSHAKE: rehandshake message processing
         *   - GNUTLS_E_WARNING_ALERT_RECEIVED: client does not wish to
         *                                      renegotiate
         */
        ssock->connection_state = TLS_STATE_HANDSHAKING;
        status = tls_try_handshake(ssock);

        return status;
    } else {
        return tls_status_from_err(ssock, status);
    }
}

#endif /* PJ_HAS_SSL_SOCK */

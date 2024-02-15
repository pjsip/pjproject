/*
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
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
#include <pj/file_access.h>
#include <pj/list.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/timer.h>
#include <pj/unicode.h>

/* Only build when PJ_HAS_SSL_SOCK is enabled and when the backend is
 * Schannel.
 *
 * Note:
 * - Older Windows SDK versions are not supported (some APIs deprecated,
 *   further more they must be missing newer/safer TLS protocol versions).
 */
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0 && \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_SCHANNEL) && \
    defined(_MSC_VER) && _MSC_VER>=1900

#define THIS_FILE "ssl_sock_schannel.c"

/* Sender string in logging */
#define SENDER "Schannel"

/* Debugging */
#define DEBUG_SCHANNEL  0

#if DEBUG_SCHANNEL
#  define LOG_DEBUG(title)                  PJ_LOG(4,(SENDER, title))
#  define LOG_DEBUG1(title, p1)             PJ_LOG(4,(SENDER, title, p1))
#  define LOG_DEBUG2(title, p1, p2)         PJ_LOG(4,(SENDER, title, p1, p2))
#  define LOG_DEBUG_ERR(title, sec_status)  log_sec_err(4, title, sec_status)
#else
#  define LOG_DEBUG(s)
#  define LOG_DEBUG1(title, p1)
#  define LOG_DEBUG2(title, p1, p2)
#  define LOG_DEBUG_ERR(title, sec_status)
#endif


/* For using SSPI */
#define SECURITY_WIN32

/* For using SCH_CREDENTIALS */
#define SCHANNEL_USE_BLACKLISTS
//#define UNICODE_STRING
//#define PUNICODE_STRING
//#include <Ntdef.h>        // error: many redefinitions
//#include <SubAuth.h>
#include <Winternl.h>

#include <security.h>
#include <schannel.h>

#pragma comment (lib, "secur32.lib")
#pragma comment (lib, "shlwapi.lib")
#pragma comment (lib, "Crypt32.lib") // for creating & manipulating certs


/* SSL sock implementation API */
#define SSL_SOCK_IMP_USE_CIRC_BUF
#include "ssl_sock_imp_common.h"

#define MIN_READ_BUF_CAP    (1024*8)
#define MIN_WRITE_BUF_CAP   (1024*8)


/*
 * Schannel global vars.
 */
static struct sch_ssl_t
{
    pj_caching_pool   cp;
    pj_pool_t        *pool;
    unsigned long     init_cnt;
} sch_ssl;


/*
 * Secure socket structure definition.
 */
typedef struct sch_ssl_sock_t
{
    pj_ssl_sock_t         base;

    CredHandle            cred_handle;
    CtxtHandle            ctx_handle;
    SecPkgContext_StreamSizes strm_sizes;
    pj_uint8_t           *write_buf;
    pj_uint8_t           *read_buf;
    circ_buf_t            decrypted_buf;
} sch_ssl_sock_t;


#include "ssl_sock_imp_common.c"


/* === Helper functions === */

static void sch_deinit(void)
{
    if (sch_ssl.pool) {
        pj_pool_secure_release(&sch_ssl.pool);
        pj_caching_pool_destroy(&sch_ssl.cp);
    }

    pj_bzero(&sch_ssl, sizeof(sch_ssl));
}

static void sch_inc()
{
    if (++sch_ssl.init_cnt == 1 && sch_ssl.pool == NULL) {
        pj_caching_pool_init(&sch_ssl.cp, NULL, 0);
        sch_ssl.pool = pj_pool_create(&sch_ssl.cp.factory, "sch%p",
                                      512, 512, NULL);
        if (pj_atexit(&sch_deinit) != PJ_SUCCESS) {
            PJ_LOG(1,(SENDER, "Failed to register atexit() for Schannel."));
        }
    }
}

static void sch_dec()
{
    pj_assert(sch_ssl.init_cnt > 0);
    --sch_ssl.init_cnt;
}

/* Print Schannel error to log */
void log_sec_err(int log_level, const char* title, SECURITY_STATUS ss)
{
    char *str;
    DWORD len;
    len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                         FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                         NULL, ss, 0, (LPTSTR)&str, 0, NULL);
    /* Trim new line chars */
    while (str[len-1] == '\r' || str[len-1] == '\n') str[--len] = 0;

    switch (log_level) {
    case 1:
        PJ_LOG(1, (SENDER, "%s: 0x%x-%s", title, ss, str));
        break;
    case 2:
        PJ_LOG(2, (SENDER, "%s: 0x%x-%s", title, ss, str));
        break;
    case 3:
        PJ_LOG(3, (SENDER, "%s: 0x%x-%s", title, ss, str));
        break;
    case 4:
        PJ_LOG(4, (SENDER, "%s: 0x%x-%s", title, ss, str));
        break;
    case 5:
        PJ_LOG(5, (SENDER, "%s: 0x%x-%s", title, ss, str));
        break;
    default:
        PJ_LOG(6, (SENDER, "%s: 0x%x-%s", title, ss, str));
        break;
    }

    LocalFree(str);
}


/* === SSL socket implementations === */

/* Allocate SSL backend struct */
static pj_ssl_sock_t *ssl_alloc(pj_pool_t *pool)
{
    sch_ssl_sock_t *sch_ssock = NULL;

    sch_inc();

    sch_ssock = (sch_ssl_sock_t*) PJ_POOL_ZALLOC_T(pool, sch_ssl_sock_t);
    if (!sch_ssock)
        return NULL;

    SecInvalidateHandle(&sch_ssock->cred_handle);
    SecInvalidateHandle(&sch_ssock->ctx_handle);

    return &sch_ssock->base;
}

/* Create and initialize new SSL context and instance */
static pj_status_t ssl_create(pj_ssl_sock_t *ssock)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;
    pj_ssize_t read_cap;
    pj_status_t status = PJ_SUCCESS;

    read_cap = PJ_MAX(MIN_READ_BUF_CAP, ssock->param.read_buffer_size);

    /* Allocate read buffer */
    sch_ssock->read_buf =
        (pj_uint8_t*)pj_pool_zalloc(ssock->pool, read_cap);
    if (!sch_ssock->read_buf) {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Initialize input circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_input,
                       read_cap);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Initialize output circular buffer */
    status = circ_init(ssock->pool->factory, &ssock->circ_buf_output,
                       PJ_MAX(MIN_WRITE_BUF_CAP,
                              ssock->param.send_buffer_size));
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Initialize decrypted circular buffer */
    status = circ_init(ssock->pool->factory, &sch_ssock->decrypted_buf,
                       read_cap);
    if (status != PJ_SUCCESS)
        goto on_return;

on_return:
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (SENDER, status, "Error allocating SSL"));
    }

    return status;
}


/* Destroy SSL context and instance */
static void ssl_destroy(pj_ssl_sock_t* ssock)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;

    /* Destroy circular buffers */
    circ_deinit(&ssock->circ_buf_input);
    circ_deinit(&ssock->circ_buf_output);
    circ_deinit(&sch_ssock->decrypted_buf);

    sch_dec();
}

/* Reset SSL socket state */
static void ssl_reset_sock_state(pj_ssl_sock_t* ssock)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;
    SECURITY_STATUS ss;

    LOG_DEBUG("SSL reset");

    pj_lock_acquire(ssock->write_mutex);

    /* Signal shutdown only when SSL connection has been established */
    if (ssock->ssl_state == SSL_STATE_ESTABLISHED &&
        SecIsValidHandle(&sch_ssock->ctx_handle))
    {
        DWORD type = SCHANNEL_SHUTDOWN;

        SecBuffer buf_in[1] = { {0} };
        buf_in[0].BufferType = SECBUFFER_TOKEN;
        buf_in[0].pvBuffer = &type;
        buf_in[0].cbBuffer = sizeof(type);
        SecBufferDesc buf_desc_in = { 0 };
        buf_desc_in.ulVersion = SECBUFFER_VERSION;
        buf_desc_in.cBuffers = ARRAYSIZE(buf_in);
        buf_desc_in.pBuffers = buf_in;
        ApplyControlToken(&sch_ssock->ctx_handle, &buf_desc_in);

        SecBuffer buf_out[1] = { {0} };
        buf_out[0].BufferType = SECBUFFER_TOKEN;
        SecBufferDesc buf_desc_out = { 0 };
        buf_desc_out.ulVersion = SECBUFFER_VERSION;
        buf_desc_out.cBuffers = ARRAYSIZE(buf_out);
        buf_desc_out.pBuffers = buf_out;

        DWORD flags =
            ISC_REQ_ALLOCATE_MEMORY |
            ISC_REQ_CONFIDENTIALITY |
            ISC_REQ_REPLAY_DETECT |
            ISC_REQ_SEQUENCE_DETECT |
            ISC_REQ_STREAM;
        ss = InitializeSecurityContext(&sch_ssock->cred_handle,
                                       &sch_ssock->ctx_handle,
                                       NULL,
                                       flags, 0, 0,
                                       &buf_desc_out, 0, NULL,
                                       &buf_desc_out,
                                       &flags, NULL);
        if (ss == SEC_E_OK) {
            /* May need to send TLS shutdown packets */
            if (buf_out->cbBuffer > 0 && buf_out[0].pvBuffer) {
                pj_status_t status;

                status = circ_write(&ssock->circ_buf_output,
                                    buf_out[0].pvBuffer,
                                    buf_out[0].cbBuffer);
                FreeContextBuffer(buf_out[0].pvBuffer);
                if (status != PJ_SUCCESS) {
                    PJ_PERROR(1, (SENDER, status,
                        "Failed queueing handshake packets"));
                } else {
                    flush_circ_buf_output(ssock, &ssock->shutdown_op_key,
                                          0, 0);
                }
            }
        } else {
            log_sec_err(1, "Error in shutting down SSL", ss);
        }
    }

    ssock->ssl_state = SSL_STATE_NULL;

    if (SecIsValidHandle(&sch_ssock->ctx_handle)) {
        DeleteSecurityContext(&sch_ssock->ctx_handle);
        SecInvalidateHandle(&sch_ssock->ctx_handle);
    }

    if (SecIsValidHandle(&sch_ssock->cred_handle)) {
        FreeCredentialsHandle(&sch_ssock->cred_handle);
        SecInvalidateHandle(&sch_ssock->cred_handle);
    }
    circ_reset(&ssock->circ_buf_input);
    circ_reset(&ssock->circ_buf_output);
    circ_reset(&sch_ssock->decrypted_buf);

    pj_lock_release(ssock->write_mutex);

    ssl_close_sockets(ssock);
}

/* Ciphers and certs */
static void ssl_ciphers_populate()
{
    PJ_TODO(implement_this);
}

static pj_ssl_cipher ssl_get_cipher(pj_ssl_sock_t *ssock)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;
    SecPkgContext_CipherInfo ci;
    SECURITY_STATUS ss;

    if (!SecIsValidHandle(&sch_ssock->ctx_handle)) {
        return PJ_TLS_UNKNOWN_CIPHER;
    }

    ss = QueryContextAttributes(&sch_ssock->ctx_handle,
                                SECPKG_ATTR_CIPHER_INFO,
                                &ci);
    if (ss == SEC_E_OK) {
        pj_ssl_cipher c = (pj_ssl_cipher)ci.dwCipherSuite;

        /* Add cipher to cipher list, if not yet */
        if (ssl_cipher_num < PJ_SSL_SOCK_MAX_CIPHERS &&
            !pj_ssl_cipher_name(c))
        {
            char tmp_buf[SZ_ALG_MAX_SIZE];
            pj_str_t tmp_st;

            pj_unicode_to_ansi(ci.szCipherSuite, SZ_ALG_MAX_SIZE,
                               tmp_buf, sizeof(tmp_buf));
            pj_strdup2_with_null(sch_ssl.pool, &tmp_st, tmp_buf);

            ssl_ciphers[ssl_cipher_num].id = c;
            ssl_ciphers[ssl_cipher_num].name = tmp_st.ptr;
            ++ssl_cipher_num;
        }
        return (pj_ssl_cipher)ci.dwCipherSuite;
    }

    return PJ_TLS_UNKNOWN_CIPHER;
}

static void ssl_update_certs_info(pj_ssl_sock_t *ssock)
{
    PJ_TODO(implement_this);
}

/* SSL session functions */
static void ssl_set_state(pj_ssl_sock_t* ssock, pj_bool_t is_server)
{
    /* Nothing to do */
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(is_server);
}

static void ssl_set_peer_name(pj_ssl_sock_t* ssock)
{
    /* Nothing to do */
    PJ_UNUSED_ARG(ssock);
}


static PCCERT_CONTEXT create_self_signed_cert()
{
    CERT_CONTEXT const* cert_context = NULL;
    CERT_EXTENSIONS cert_extensions = { 0 };

    BYTE cert_name_buffer[16];
    CERT_NAME_BLOB cert_name;
    cert_name.pbData = cert_name_buffer;
    cert_name.cbData = ARRAYSIZE(cert_name_buffer);

    if (!CertStrToName(X509_ASN_ENCODING, "", CERT_X500_NAME_STR, NULL,
                       cert_name.pbData, &cert_name.cbData, NULL))
    {
        return NULL;
    }

    cert_context = CertCreateSelfSignCertificate(
                                    0, &cert_name, 0, NULL,
                                    NULL, NULL, NULL, &cert_extensions);

    return cert_context;
}


static pj_status_t ssl_do_handshake(pj_ssl_sock_t* ssock)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;
    pj_size_t data_in_size = 0;
    pj_uint8_t* data_in = NULL;
    SECURITY_STATUS ss;
    pj_status_t status = PJ_EPENDING;
    pj_status_t status2;

    pj_lock_acquire(ssock->write_mutex);

    /* Create credential handle, if not yet */
    if (!SecIsValidHandle(&sch_ssock->cred_handle)) {
        SCH_CREDENTIALS creds = { 0 };

        creds.dwVersion = SCH_CREDENTIALS_VERSION;
        creds.dwFlags = SCH_USE_STRONG_CRYPTO;

        if (ssock->is_server) {
            PCCERT_CONTEXT cert_context = create_self_signed_cert();
            creds.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
            creds.cCreds = 1;
            creds.paCred = &cert_context;
        } else {
            creds.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION |
                             // SCH_CRED_AUTO_CRED_VALIDATION |
                             SCH_CRED_NO_DEFAULT_CREDS;
        }

        ss = AcquireCredentialsHandle(NULL, UNISP_NAME,
                ssock->is_server? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND,
                NULL, &creds, NULL, NULL,
                &sch_ssock->cred_handle, NULL);
        if (ss < 0) {
            log_sec_err(1, "Failed AcquireCredentialsHandle()", ss);
            status = PJ_EUNKNOWN;
            goto on_return;
        }
    }

    /* Start handshake iteration */

    pj_lock_acquire(ssock->circ_buf_input_mutex);

    if (!circ_empty(&ssock->circ_buf_input)) {
        data_in = sch_ssock->read_buf;
        data_in_size = PJ_MIN(MIN_READ_BUF_CAP,
                              circ_size(&ssock->circ_buf_input));
        circ_read(&ssock->circ_buf_input, data_in, data_in_size);
    }

    SecBuffer buf_in[2] = { {0} };
    buf_in[0].BufferType = SECBUFFER_TOKEN;
    buf_in[0].pvBuffer = data_in;
    buf_in[0].cbBuffer = (unsigned long)data_in_size;
    buf_in[1].BufferType = SECBUFFER_EMPTY;
    SecBufferDesc buf_desc_in = { 0 };
    buf_desc_in.ulVersion = SECBUFFER_VERSION;
    buf_desc_in.cBuffers = ARRAYSIZE(buf_in);
    buf_desc_in.pBuffers = buf_in;

    SecBuffer buf_out[1] = { {0} };
    buf_out[0].BufferType = SECBUFFER_TOKEN;
    SecBufferDesc buf_desc_out = { 0 };
    buf_desc_out.ulVersion = SECBUFFER_VERSION;
    buf_desc_out.cBuffers = ARRAYSIZE(buf_out);
    buf_desc_out.pBuffers = buf_out;

    /* As client */
    if (!ssock->is_server) {
        DWORD flags =
            ISC_REQ_USE_SUPPLIED_CREDS |
            ISC_REQ_ALLOCATE_MEMORY |
            ISC_REQ_CONFIDENTIALITY |
            ISC_REQ_REPLAY_DETECT |
            ISC_REQ_SEQUENCE_DETECT |
            ISC_REQ_STREAM;

        ss = InitializeSecurityContext(
                    &sch_ssock->cred_handle,
                    SecIsValidHandle(&sch_ssock->ctx_handle)?
                        &sch_ssock->ctx_handle : NULL,
                    (SEC_CHAR*)ssock->param.server_name.ptr,
                    flags, 0, 0,
                    data_in_size? &buf_desc_in : NULL,
                    0,
                    SecIsValidHandle(&sch_ssock->ctx_handle)?
                        NULL : &sch_ssock->ctx_handle,
                    &buf_desc_out, &flags, NULL);
    }

    /* As server */
    else {
        DWORD flags =
            ASC_REQ_ALLOCATE_MEMORY |
            ASC_REQ_CONFIDENTIALITY |
            ASC_REQ_REPLAY_DETECT |
            ASC_REQ_SEQUENCE_DETECT |
            ASC_REQ_STREAM;

        ss = AcceptSecurityContext(
                    &sch_ssock->cred_handle,
                    SecIsValidHandle(&sch_ssock->ctx_handle) ?
                        &sch_ssock->ctx_handle : NULL,
                    data_in_size ? &buf_desc_in : NULL,
                    flags, 0,
                    SecIsValidHandle(&sch_ssock->ctx_handle) ?
                        NULL : &sch_ssock->ctx_handle,
                    &buf_desc_out, &flags, NULL);
    }

    /* Check for any unprocessed input data, put it back to buffer */
    if (buf_in[1].BufferType==SECBUFFER_EXTRA && buf_in[1].cbBuffer>0) {
        circ_read_cancel(&ssock->circ_buf_input, buf_in[1].cbBuffer);
    }

    if (ss == SEC_E_OK) {
        /* Handshake completed! */
        ssock->ssl_state = SSL_STATE_ESTABLISHED;
        status = PJ_SUCCESS;
        PJ_LOG(3, (SENDER, "TLS handshake completed!"));

        /* Get stream sizes */
        ss = QueryContextAttributes(&sch_ssock->ctx_handle,
                                    SECPKG_ATTR_STREAM_SIZES,
                                    &sch_ssock->strm_sizes);
        if (ss != SEC_E_OK) {
            log_sec_err(1, "Failed querying stream sizes", ss);
            ssl_reset_sock_state(ssock);
            status = PJ_EUNKNOWN;
        }

        /* Allocate SSL write buf, if not yet */
        if (!sch_ssock->write_buf) {
            pj_size_t size;

            sch_ssock->strm_sizes.cbMaximumMessage =
                PJ_MIN(sch_ssock->strm_sizes.cbMaximumMessage,
                       (unsigned long)ssock->param.send_buffer_size);
            sch_ssock->strm_sizes.cbMaximumMessage =
                PJ_MAX(MIN_WRITE_BUF_CAP,
                       sch_ssock->strm_sizes.cbMaximumMessage);

            size = (pj_size_t)sch_ssock->strm_sizes.cbHeader +
                   sch_ssock->strm_sizes.cbMaximumMessage +
                   sch_ssock->strm_sizes.cbTrailer;
            sch_ssock->write_buf = (pj_uint8_t*)
                                    pj_pool_zalloc(ssock->pool, size);
        }
    }

    else if (ss == SEC_I_COMPLETE_NEEDED ||
             ss == SEC_I_COMPLETE_AND_CONTINUE)
    {
        /* Perhaps CompleteAuthToken() is unnecessary for Schannel, but
         * the sample code seems to call it.
         */
        LOG_DEBUG_ERR("Handshake progress", ss);
        ss = CompleteAuthToken(&sch_ssock->ctx_handle, &buf_desc_out);
        if (ss != SEC_E_OK) {
            log_sec_err(1, "Handshake error in CompleteAuthToken()", ss);
            status = PJ_EUNKNOWN;
        }
    }

    else if (ss == SEC_I_CONTINUE_NEEDED)
    {
        LOG_DEBUG_ERR("Handshake progress", ss);
    }

    else if (ss == SEC_E_INCOMPLETE_MESSAGE)
    {
        LOG_DEBUG_ERR("Handshake progress", ss);

        /* Put back the incomplete message */
        circ_read_cancel(&ssock->circ_buf_input, data_in_size);
    }

    else {
        /* Handshake failed */
        log_sec_err(1, "Handshake failed!", ss);
        status = PJ_EUNKNOWN;
    }

    pj_lock_release(ssock->circ_buf_input_mutex);

    if (buf_out[0].cbBuffer > 0 && buf_out[0].pvBuffer) {
        /* Queue output data to send */
        status2 = circ_write(&ssock->circ_buf_output, buf_out[0].pvBuffer,
                             buf_out[0].cbBuffer);
        FreeContextBuffer(buf_out[0].pvBuffer);
        if (status2 != PJ_SUCCESS) {
            PJ_PERROR(1,(SENDER, status2,
                         "Failed queueing handshake packets"));
            status = status2;
        }
    }

    /* Send handshake packets to wire */
    status2 = flush_circ_buf_output(ssock, &ssock->handshake_op_key, 0, 0);
    if (status2 != PJ_SUCCESS && status2 != PJ_EPENDING) {
        PJ_PERROR(1,(SENDER, status2, "Failed sending handshake packets"));
        status = status2;
    }

on_return:
    if (status != PJ_SUCCESS && status != PJ_EPENDING)
        ssl_reset_sock_state(ssock);

    pj_lock_release(ssock->write_mutex);

    return status;
}

static pj_status_t ssl_renegotiate(pj_ssl_sock_t *ssock)
{
    PJ_TODO(implement_this);
    return PJ_ENOTSUP;
}

static int find_sec_buffer(const SecBuffer* buf, int buf_cnt,
                           unsigned long sec_type)
{
    for (int i = 0; i < buf_cnt; ++i)
        if (buf[i].BufferType == sec_type)
            return i;
    return -1;
}

static pj_status_t ssl_read(pj_ssl_sock_t* ssock, void* data, int* size)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;
    pj_size_t size_ = 0;
    pj_uint8_t* data_ = NULL;
    SECURITY_STATUS ss;
    pj_status_t status = PJ_SUCCESS;
    int i, need = *size, requested = *size;

    /* Avoid compile warning of unused debugging var */
    PJ_UNUSED_ARG(requested);

    pj_lock_acquire(ssock->circ_buf_input_mutex);

    /* Try read from the decrypted buffer */
    size_ = circ_size(&sch_ssock->decrypted_buf);
    if (need <= size_) {
        /* Got all from the decrypted buffer */
        circ_read(&sch_ssock->decrypted_buf, data, need);
        *size = need;
        LOG_DEBUG1("Read %d: returned all from decrypted buffer.", requested);
        pj_lock_release(ssock->circ_buf_input_mutex);
        return PJ_SUCCESS;
    }

    /* Get all data of the decrypted buffer, then decrypt more */
    LOG_DEBUG2("Read %d: %d from decrypted buffer..", requested, size_);
    circ_read(&sch_ssock->decrypted_buf, data, size_);
    *size = (int)size_;
    need  -= (int)size_;

    /* Decrypt data of network input buffer */
    if (!circ_empty(&ssock->circ_buf_input)) {
        data_ = sch_ssock->read_buf;
        size_ = PJ_MIN(MIN_READ_BUF_CAP, circ_size(&ssock->circ_buf_input));
        circ_read(&ssock->circ_buf_input, data_, size_);
    } else {
        LOG_DEBUG2("Read %d: no data to decrypt, returned %d.",
                   requested, *size);
        pj_lock_release(ssock->circ_buf_input_mutex);
        return PJ_SUCCESS;
    }

    SecBuffer buf[4] = { {0} };
    buf[0].BufferType = SECBUFFER_DATA;
    buf[0].pvBuffer = data_;
    buf[0].cbBuffer = (unsigned long)size_;
    buf[1].BufferType = SECBUFFER_EMPTY;
    buf[2].BufferType = SECBUFFER_EMPTY;
    buf[3].BufferType = SECBUFFER_EMPTY;
    SecBufferDesc buf_desc = { 0 };
    buf_desc.ulVersion = SECBUFFER_VERSION;
    buf_desc.cBuffers = ARRAYSIZE(buf);
    buf_desc.pBuffers = buf;

    ss = DecryptMessage(&sch_ssock->ctx_handle, &buf_desc, 0, NULL);

    /* Check for any unprocessed input data, put it back to buffer */
    i = find_sec_buffer(buf, ARRAYSIZE(buf), SECBUFFER_EXTRA);
    if (i >= 0) {
        circ_read_cancel(&ssock->circ_buf_input, buf[i].cbBuffer);
    }

    if (ss == SEC_E_OK) {
        i = find_sec_buffer(buf, ARRAYSIZE(buf), SECBUFFER_DATA);
        if (i >= 0) {
            pj_uint8_t *p = buf[i].pvBuffer;
            pj_size_t len = buf[i].cbBuffer;
            if (need <= len) {
                /* All requested fulfilled, may have excess */
                pj_memcpy((pj_uint8_t*)data + *size, p, need);
                *size += need;
                len -= need;
                p += need;

                /* Store any excess to the decrypted buffer */
                if (len)
                    circ_write(&sch_ssock->decrypted_buf, p, len);

                LOG_DEBUG2("Read %d: after decrypt, excess=%d",
                           requested, len);
            } else {
                /* Not enough, gave everyting */
                pj_memcpy((pj_uint8_t*)data + *size, p, len);
                *size += (int)len;
                LOG_DEBUG2("Read %d: after decrypt, only got %d",
                           requested, len);
            }
        }
    }

    else if (ss == SEC_E_INCOMPLETE_MESSAGE)
    {
        /* Put back the incomplete message */
        circ_read_cancel(&ssock->circ_buf_input, size_);
    }

    else if (ss == SEC_I_RENEGOTIATE) {
        PJ_LOG(3, (SENDER, "Remote signals renegotiation"));

        if (SecIsValidHandle(&sch_ssock->ctx_handle)) {
            DeleteSecurityContext(&sch_ssock->ctx_handle);
            SecInvalidateHandle(&sch_ssock->ctx_handle);
        }

        ssock->ssl_state = SSL_STATE_HANDSHAKING;
        status = PJ_EEOF;

        /* Any unprocessed data should have been returned via buffer type
         * SECBUFFER_EXTRA above (docs seems to say so).
         */
        //circ_read_cancel(&ssock->circ_buf_input, size_);
    }

    else if (ss == SEC_I_CONTEXT_EXPIRED)
    {
        PJ_LOG(3, (SENDER, "TLS connection closed"));
        ssock->ssl_state = SSL_STATE_ERROR;
    }

    else {
        log_sec_err(1, "Decrypt error", ss);
        status = PJ_EUNKNOWN;
    }

    pj_lock_release(ssock->circ_buf_input_mutex);

    /* Reset SSL if it is not renegotiating */
    if (status != PJ_SUCCESS && ssock->ssl_state != SSL_STATE_HANDSHAKING)
        ssl_reset_sock_state(ssock);

    LOG_DEBUG2("Read %d: returned=%d.", requested, *size);
    return status;
}


static pj_status_t ssl_write(pj_ssl_sock_t* ssock, const void* data,
                             pj_ssize_t size, int* nwritten)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;
    pj_ssize_t total = 0;
    pj_status_t status = PJ_SUCCESS;

    pj_lock_acquire(ssock->write_mutex);

    while (total < size) {
        SECURITY_STATUS ss;
        pj_uint8_t *p_header, *p_data, *p_trailer;
        pj_ssize_t write_len, out_size;

        write_len = PJ_MIN(size-total, sch_ssock->strm_sizes.cbMaximumMessage);
        p_header  = sch_ssock->write_buf;
        p_data    = p_header + sch_ssock->strm_sizes.cbHeader;
        p_trailer = p_data + write_len;

        pj_memcpy(p_data, (pj_uint8_t*)data + total, write_len);

        SecBuffer buf[4] = { {0} };
        buf[0].BufferType = SECBUFFER_STREAM_HEADER;
        buf[0].pvBuffer = p_header;
        buf[0].cbBuffer = sch_ssock->strm_sizes.cbHeader;
        buf[1].BufferType = SECBUFFER_DATA;
        buf[1].pvBuffer = p_data;
        buf[1].cbBuffer = (unsigned long)write_len;
        buf[2].BufferType = SECBUFFER_STREAM_TRAILER;
        buf[2].pvBuffer = p_trailer;
        buf[2].cbBuffer = sch_ssock->strm_sizes.cbTrailer;
        buf[3].BufferType = SECBUFFER_EMPTY;

        SecBufferDesc buf_desc = { 0 };
        buf_desc.ulVersion = SECBUFFER_VERSION;
        buf_desc.cBuffers = ARRAYSIZE(buf);
        buf_desc.pBuffers = buf;

        ss = EncryptMessage(&sch_ssock->ctx_handle, 0, &buf_desc, 0);

        if (ss != SEC_E_OK) {
            log_sec_err(1, "Encrypt error", ss);
            status = (ss==SEC_E_CONTEXT_EXPIRED)? PJ_EEOF : PJ_EUNKNOWN;
            break;
        }

        out_size = (pj_ssize_t)buf[0].cbBuffer + buf[1].cbBuffer +
                               buf[2].cbBuffer;
        status = circ_write(&ssock->circ_buf_output, sch_ssock->write_buf,
                            out_size);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1, (SENDER, status,
                          "Failed queueing outgoing packets"));
            break;
        }

        total += write_len;
    }

    pj_lock_release(ssock->write_mutex);

    *nwritten = (int)total;

    return status;
}


#endif  /* PJ_HAS_SSL_SOCK */

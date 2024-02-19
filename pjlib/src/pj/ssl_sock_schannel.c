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
#define SENDER    "ssl_schannel"

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

#include <Bcrypt.h>    // for enumerating ciphers
//#include <Ncrypt.h>    // for enumerating ciphers
//#include <Sslprovider.h>    // for enumerating ciphers


#pragma comment (lib, "secur32.lib")
#pragma comment (lib, "shlwapi.lib")
#pragma comment (lib, "Crypt32.lib") // for creating & manipulating certs
#pragma comment (lib, "Bcrypt.lib")  // for enumerating ciphers
//#pragma comment (lib, "Ncrypt.lib")  // for enumerating ciphers


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
    PCCERT_CONTEXT    self_signed_cert;
} sch_ssl;


/*
 * Secure socket structure definition.
 */
typedef struct sch_ssl_sock_t
{
    pj_ssl_sock_t         base;

    CredHandle            cred_handle;
    CtxtHandle            ctx_handle;
    PCCERT_CONTEXT        cert_ctx;
    SecPkgContext_StreamSizes strm_sizes;

    pj_uint8_t           *write_buf;
    pj_size_t             write_buf_cap;
    pj_uint8_t           *read_buf;
    pj_size_t             read_buf_cap;
    circ_buf_t            decrypted_buf;
} sch_ssl_sock_t;


#include "ssl_sock_imp_common.c"


/* === Helper functions === */

#define PJ_SSL_ERRNO_START              (PJ_ERRNO_START_USER + \
                                         PJ_ERRNO_SPACE_SIZE*6)

static pj_status_t sec_err_to_pj(SECURITY_STATUS ss)
{
    DWORD err = ss & 0xFFFF;

    /* Make sure it does not exceed PJ_ERRNO_SPACE_SIZE */
    if (err >= PJ_ERRNO_SPACE_SIZE)
        return PJ_EUNKNOWN;

    return PJ_SSL_ERRNO_START + err;
}

static SECURITY_STATUS sec_err_from_pj(pj_status_t status)
{
    /* Make sure it is within SSL error space */
    if (status < PJ_SSL_ERRNO_START ||
        status >= PJ_SSL_ERRNO_START + PJ_ERRNO_SPACE_SIZE)
    {
        return ERROR_INVALID_DATA;
    }

    return 0x80090000 + (status - PJ_SSL_ERRNO_START);
}

/* Print Schannel error to log */
static void log_sec_err(int log_level, const char* title, SECURITY_STATUS ss)
{
    char *str = NULL;
    DWORD len;
    len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                         FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                         NULL, ss, 0, (LPSTR)&str, 0, NULL);
    /* Trim new line chars */
    while (len > 0 && (str[len-1] == '\r' || str[len-1] == '\n'))
        str[--len] = 0;

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

static pj_str_t sch_err_print(pj_status_t e, char *msg, pj_size_t max)
{
    DWORD len;
    SECURITY_STATUS ss = sec_err_from_pj(e);
    pj_str_t pjstr = {0};

    len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                         NULL, ss, 0, (LPSTR)msg, (DWORD)max, NULL);

    /* Trim new line chars */
    while (len > 0 && (msg[len-1] == '\r' || msg[len-1] == '\n'))
        msg[--len] = 0;

    pj_strset(&pjstr, msg, len);

    return pjstr;
}

static void sch_deinit(void)
{
    if (sch_ssl.self_signed_cert)
        CertFreeCertificateContext(sch_ssl.self_signed_cert);

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

        pj_register_strerror(PJ_SSL_ERRNO_START, PJ_ERRNO_SPACE_SIZE,
                             &sch_err_print);
    }
}

static void sch_dec()
{
    pj_assert(sch_ssl.init_cnt > 0);
    --sch_ssl.init_cnt;
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
    pj_pool_factory* pf = ssock->pool->factory;
    pj_pool_t* pool = ssock->pool;
    pj_ssize_t read_cap, write_cap;
    pj_status_t status = PJ_SUCCESS;

    read_cap  = PJ_MAX(MIN_READ_BUF_CAP,  ssock->param.read_buffer_size);
    write_cap = PJ_MAX(MIN_WRITE_BUF_CAP, ssock->param.send_buffer_size);

    /* Allocate read buffer */
    sch_ssock->read_buf_cap = read_cap;
    sch_ssock->read_buf = (pj_uint8_t*)pj_pool_zalloc(pool, read_cap);
    if (!sch_ssock->read_buf) {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Allocate write buffer */
    sch_ssock->write_buf_cap = write_cap;
    sch_ssock->write_buf = (pj_uint8_t*)pj_pool_zalloc(pool, write_cap);
    if (!sch_ssock->write_buf) {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Initialize input circular buffer */
    status = circ_init(pf, &ssock->circ_buf_input, read_cap);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Initialize output circular buffer */
    status = circ_init(pf, &ssock->circ_buf_output, write_cap);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Initialize decrypted circular buffer */
    status = circ_init(pf, &sch_ssock->decrypted_buf, read_cap);
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

    /* Free certificate */
    if (sch_ssock->cert_ctx) {
        CertFreeCertificateContext(sch_ssock->cert_ctx);
        sch_ssock->cert_ctx = NULL;
    }

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
        buf_out[0].pvBuffer = sch_ssock->write_buf;
        buf_out[0].cbBuffer = (ULONG)sch_ssock->write_buf_cap;
        SecBufferDesc buf_desc_out = { 0 };
        buf_desc_out.ulVersion = SECBUFFER_VERSION;
        buf_desc_out.cBuffers = ARRAYSIZE(buf_out);
        buf_desc_out.pBuffers = buf_out;

        DWORD flags =
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
                if (status != PJ_SUCCESS) {
                    PJ_PERROR(1, (SENDER, status,
                        "Failed to queuehandshake packets"));
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
    PCRYPT_CONTEXT_FUNCTIONS fn = NULL;
    ULONG size = 0;
    NTSTATUS s;

    /* Populate once only */
    if (ssl_cipher_num)
        return;

    s = BCryptEnumContextFunctions(CRYPT_LOCAL, L"SSL",
                                   NCRYPT_SCHANNEL_INTERFACE,
                                   &size, &fn);
    if (s < 0) {
        PJ_LOG(1,(SENDER, "Error in enumerating ciphers (code=0x%x)", s));
        return;
    }

    for (ULONG i = 0; i < fn->cFunctions; i++) {
        char tmp_buf[SZ_ALG_MAX_SIZE];
        pj_str_t tmp_st;

        pj_unicode_to_ansi(fn->rgpszFunctions[i], SZ_ALG_MAX_SIZE,
                           tmp_buf, sizeof(tmp_buf));
        pj_strdup2_with_null(sch_ssl.pool, &tmp_st, tmp_buf);

        /* Unfortunately we do not get the ID here.
         * Let's just set ID to (0x8000000 + i) for now.
         */
        ssl_ciphers[ssl_cipher_num].id = 0x8000000 + i;
        ssl_ciphers[ssl_cipher_num].name = tmp_st.ptr;
        ++ssl_cipher_num;
    }

    BCryptFreeBuffer(fn);
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

        /* Check if this is in the cipher list */
        if (ssl_cipher_num < PJ_SSL_SOCK_MAX_CIPHERS &&
            !pj_ssl_cipher_name(c))
        {
            char tmp_buf[SZ_ALG_MAX_SIZE+1];
            unsigned i;

            pj_unicode_to_ansi(ci.szCipherSuite, SZ_ALG_MAX_SIZE,
                               tmp_buf, sizeof(tmp_buf));

            /* If cipher is actually in the list and:
             * - if ID is 0, update it, or
             * - if ID does not match, return ID from our list.
             */
            for (i = 0; i < ssl_cipher_num; ++i) {
                if (!pj_ansi_stricmp(ssl_ciphers[i].name, tmp_buf)) {
                    if (ssl_ciphers[i].id == 0)
                        ssl_ciphers[i].id = c;
                    else
                        c = ssl_ciphers[i].id;
                    break;
                }
            }

            /* Add to cipher list if not found */
            if (i == ssl_cipher_num) {
                pj_str_t tmp_st;
                pj_strdup2_with_null(sch_ssl.pool, &tmp_st, tmp_buf);

                ssl_ciphers[ssl_cipher_num].id = c;
                ssl_ciphers[ssl_cipher_num].name = tmp_st.ptr;
                ++ssl_cipher_num;
            }
        }
        return c;
    }

    return PJ_TLS_UNKNOWN_CIPHER;
}

static pj_status_t blob_to_str(DWORD enc_type, CERT_NAME_BLOB* blob,
                               DWORD flag, char *buf, unsigned buf_len)
{
    DWORD ret;
    ret = CertNameToStrA(enc_type, blob, flag, buf, buf_len);
    if (ret < 0) {
        PJ_LOG(3,(SENDER, "Failed to convert cert blob to string"));
        return PJ_ETOOSMALL;
    }
    return PJ_SUCCESS;
}


static pj_status_t file_time_to_time_val(const FILETIME* file_time,
                                         pj_time_val* time_val)
{
    FILETIME local_file_time;
    SYSTEMTIME localTime;
    pj_parsed_time pt;

    if (!FileTimeToLocalFileTime(file_time, &local_file_time))
        return PJ_RETURN_OS_ERROR(GetLastError());

    if (!FileTimeToSystemTime(&local_file_time, &localTime))
        return PJ_RETURN_OS_ERROR(GetLastError());

    pj_bzero(&pt, sizeof(pt));
    pt.year = localTime.wYear;
    pt.mon = localTime.wMonth - 1;
    pt.day = localTime.wDay;
    pt.wday = localTime.wDayOfWeek;

    pt.hour = localTime.wHour;
    pt.min = localTime.wMinute;
    pt.sec = localTime.wSecond;
    pt.msec = localTime.wMilliseconds;

    return pj_time_encode(&pt, time_val);
}


static void cert_parse_info(pj_pool_t* pool, pj_ssl_cert_info* ci,
                            const CERT_CONTEXT *cert)
{
    PCERT_INFO cert_info = cert->pCertInfo;
    char buf[512];
    pj_uint8_t serial_no[20];
    unsigned serial_size;
    pj_bool_t update_needed;
    pj_status_t status;

    /* Get issuer & serial no first */
    status = blob_to_str(cert->dwCertEncodingType, &cert_info->Issuer,
                         CERT_SIMPLE_NAME_STR,
                         buf, sizeof(buf));

    serial_size = PJ_MIN(cert_info->SerialNumber.cbData, sizeof(serial_no));
    pj_memcpy(&serial_no, cert_info->SerialNumber.pbData, serial_size);

    /* Check if the contents need to be updated */
    update_needed = status == PJ_SUCCESS &&
                    (pj_strcmp2(&ci->issuer.info, buf) ||
                     pj_memcmp(ci->serial_no, serial_no, serial_size));
    if (!update_needed)
        return;

    /* Update info */

    pj_bzero(ci, sizeof(pj_ssl_cert_info));

    /* Version */
    ci->version = cert_info->dwVersion;

    /* Issuer */
    pj_strdup2(pool, &ci->issuer.info, buf);
    status = blob_to_str(cert->dwCertEncodingType, &cert_info->Issuer,
                         CERT_X500_NAME_STR | CERT_NAME_STR_NO_PLUS_FLAG,
                         buf, sizeof(buf));
    if (status == PJ_SUCCESS)
        pj_strdup2(pool, &ci->issuer.cn, buf);

    /* Serial number */
    pj_memcpy(ci->serial_no, serial_no, serial_size);

    /* Subject */
    status = blob_to_str(cert->dwCertEncodingType, &cert_info->Subject,
                         CERT_SIMPLE_NAME_STR,
                         buf, sizeof(buf));
    if (status == PJ_SUCCESS)
        pj_strdup2(pool, &ci->subject.info, buf);

    status = blob_to_str(cert->dwCertEncodingType, &cert_info->Subject,
                         CERT_X500_NAME_STR | CERT_NAME_STR_NO_PLUS_FLAG,
                         buf, sizeof(buf));
    if (status == PJ_SUCCESS)
        pj_strdup2(pool, &ci->subject.cn, buf);

    /* Validity */
    file_time_to_time_val(&cert_info->NotAfter, &ci->validity.end);
    file_time_to_time_val(&cert_info->NotBefore, &ci->validity.start);
    ci->validity.gmt = 0;

    /* Subject Alternative Name extension */
    while (1) {
        PCERT_EXTENSION ext = CertFindExtension(szOID_SUBJECT_ALT_NAME2,
                                                cert_info->cExtension,
                                                cert_info->rgExtension);
        if (!ext)
            break;

        CERT_ALT_NAME_INFO* alt_name_info = NULL;
        DWORD alt_name_info_size = 0;
        BOOL rv;
        rv = CryptDecodeObjectEx(cert->dwCertEncodingType,
                                 szOID_SUBJECT_ALT_NAME2,
                                 ext->Value.pbData,
                                 ext->Value.cbData,
                                 CRYPT_DECODE_ALLOC_FLAG |
                                    CRYPT_DECODE_NOCOPY_FLAG,
                                 NULL,
                                 &alt_name_info,
                                 &alt_name_info_size);
        if (!rv)
            break;

        ci->subj_alt_name.entry = pj_pool_calloc(
                                        pool, alt_name_info->cAltEntry,
                                        sizeof(*ci->subj_alt_name.entry));
        if (!ci->subj_alt_name.entry) {
            PJ_LOG(3,(SENDER, "Failed to allocate memory for SubjectAltName"));
            LocalFree(alt_name_info);
            break;
        }

        for (unsigned i = 0; i < alt_name_info->cAltEntry; ++i) {
            CERT_ALT_NAME_ENTRY *ane = &alt_name_info->rgAltEntry[i];
            pj_ssl_cert_name_type type;
            unsigned len = 0;

            switch (ane->dwAltNameChoice) {
            case CERT_ALT_NAME_DNS_NAME:
                type = PJ_SSL_CERT_NAME_DNS;
                len = pj_unicode_to_ansi(ane->pwszDNSName, sizeof(buf),
                                         buf, sizeof(buf)) != NULL;
                break;
            case CERT_ALT_NAME_IP_ADDRESS:
                type = PJ_SSL_CERT_NAME_IP;
                pj_inet_ntop2(ane->IPAddress.cbData == sizeof(pj_in6_addr)?
                                    pj_AF_INET6() : pj_AF_INET(),
                              ane->IPAddress.pbData, buf, sizeof(buf));
                break;
            case CERT_ALT_NAME_URL:
                type = PJ_SSL_CERT_NAME_URI;
                len = pj_unicode_to_ansi(ane->pwszDNSName, sizeof(buf),
                                         buf, sizeof(buf)) != NULL;
                break;
            case CERT_ALT_NAME_RFC822_NAME:
                type = PJ_SSL_CERT_NAME_RFC822;
                len = pj_unicode_to_ansi(ane->pwszDNSName, sizeof(buf),
                                         buf, sizeof(buf)) != NULL;
                break;
            default:
                type = PJ_SSL_CERT_NAME_UNKNOWN;
                break;
            }

            if (len && type != PJ_SSL_CERT_NAME_UNKNOWN) {
                ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
                pj_strdup2(pool,
                        &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
                        buf);
                ci->subj_alt_name.cnt++;
            }
        }

        /* Done parsing Subject Alt Name */
        LocalFree(alt_name_info);
        break;
    }
}

static void ssl_update_certs_info(pj_ssl_sock_t *ssock)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;
    CERT_CONTEXT *cert_ctx = NULL;
    SECURITY_STATUS ss;

    if (!SecIsValidHandle(&sch_ssock->ctx_handle)) {
        return;
    }

    ss = QueryContextAttributes(&sch_ssock->ctx_handle,
                                SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                                &cert_ctx);

    if (ss != SEC_E_OK || !cert_ctx) {
        if (!ssock->is_server)
            log_sec_err(1, "Failed to retrieve remote certificate", ss);
    } else {
        cert_parse_info(ssock->pool, &ssock->remote_cert_info, cert_ctx);
        CertFreeCertificateContext(cert_ctx);
    }

    ss = QueryContextAttributes(&sch_ssock->ctx_handle,
                                SECPKG_ATTR_LOCAL_CERT_CONTEXT,
                                &cert_ctx);

    if (ss != SEC_E_OK || !cert_ctx) {
        if (ssock->is_server)
            log_sec_err(3, "Failed to retrieve local certificate", ss);
    }
    else {
        cert_parse_info(ssock->pool, &ssock->local_cert_info, cert_ctx);
        CertFreeCertificateContext(cert_ctx);
    }
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
    if (!sch_ssl.self_signed_cert) {
        CERT_EXTENSIONS cert_ext = { 0 };
        BYTE cert_name_buffer[64];
        CERT_NAME_BLOB cert_name;
        cert_name.pbData = cert_name_buffer;
        cert_name.cbData = ARRAYSIZE(cert_name_buffer);

        if (!CertStrToNameA(X509_ASN_ENCODING, "CN=lab.pjsip.org",
                            CERT_X500_NAME_STR, NULL,
                            cert_name.pbData, &cert_name.cbData, NULL))
        {
            return NULL;
        }

        sch_ssl.self_signed_cert = CertCreateSelfSignCertificate(
                                        0, &cert_name, 0, NULL,
                                        NULL, NULL, NULL,
                                        &cert_ext);
    }

    /* May return NULL on any failure */
    return CertDuplicateCertificateContext(sch_ssl.self_signed_cert);
}

static PCCERT_CONTEXT find_cert_in_stores(const pj_str_t *subject)
{
    HCERTSTORE store = NULL;
    PCCERT_CONTEXT cert = NULL;

    /* Find in Current User store */
    store = CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING, 0,
                          CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
    if (store) {
        cert = CertFindCertificateInStore(store, X509_ASN_ENCODING,
                        0, CERT_FIND_SUBJECT_STR_A, subject->ptr, NULL);
        CertCloseStore(store, 0);
        if (cert)
            return cert;
    } else {
        log_sec_err(1, "Error opening current user cert store.",
                    GetLastError());
    }

    /* Find in Local Machine store */
    store = CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING, 0,
                          CERT_SYSTEM_STORE_LOCAL_MACHINE, L"MY");
    if (store) {
        cert = CertFindCertificateInStore(store, X509_ASN_ENCODING,
                        0, CERT_FIND_SUBJECT_STR_A, subject->ptr, NULL);
        CertCloseStore(store, 0);
        if (cert)
            return cert;
    } else {
        log_sec_err(1, "Error opening local machine cert store.",
                    GetLastError());
    }

    PJ_LOG(1,(SENDER, "Cannot find certificate with specified subject: %.*s",
              subject->slen, subject->ptr));
    return NULL;
}


static pj_status_t init_creds(pj_ssl_sock_t* ssock)
{
    sch_ssl_sock_t* sch_ssock = (sch_ssl_sock_t*)ssock;
    SCH_CREDENTIALS creds = { 0 };
    unsigned param_cnt = 0;
    TLS_PARAMETERS params[1] = {{0}};
    SECURITY_STATUS ss;

    creds.dwVersion = SCH_CREDENTIALS_VERSION;
    creds.dwFlags = SCH_USE_STRONG_CRYPTO;

    /* Setup Protocol version */
    if (ssock->param.proto != PJ_SSL_SOCK_PROTO_DEFAULT &&
        ssock->param.proto != PJ_SSL_SOCK_PROTO_ALL)
    {
        DWORD tmp = 0;
        if (ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_3) {
            tmp |= ssock->is_server ? SP_PROT_TLS1_3_SERVER :
                                      SP_PROT_TLS1_3_CLIENT;
        }
        if ((ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_2) == 0) {
            tmp |= ssock->is_server ? SP_PROT_TLS1_2_SERVER :
                                      SP_PROT_TLS1_2_CLIENT;
        }
        if ((ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_1) == 0) {
            tmp |= ssock->is_server ? SP_PROT_TLS1_1_SERVER :
                                      SP_PROT_TLS1_1_CLIENT;
        }
        if ((ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1) == 0) {
            tmp |= ssock->is_server ? SP_PROT_TLS1_0_SERVER :
                                      SP_PROT_TLS1_0_CLIENT;
        }
        if ((ssock->param.proto & PJ_SSL_SOCK_PROTO_SSL3) == 0) {
            tmp |= ssock->is_server ? SP_PROT_SSL3_SERVER :
                                      SP_PROT_SSL3_CLIENT;
        }
        if ((ssock->param.proto & PJ_SSL_SOCK_PROTO_SSL2) == 0) {
            tmp |= ssock->is_server ? SP_PROT_SSL2_SERVER :
                                      SP_PROT_SSL2_CLIENT;
        }
        if (tmp) {
            param_cnt = 1;
            params[0].grbitDisabledProtocols = SP_PROT_ALL & ~tmp;
        }
    }

    /* Setup the TLS certificate */
    if (ssock->param.cert_subject.slen && !sch_ssock->cert_ctx) {
        /* Search certificate from stores */
        sch_ssock->cert_ctx =
            find_cert_in_stores(&ssock->param.cert_subject);
    }

    if (ssock->is_server) {
        if (!ssock->param.cert_subject.slen) {
            /* No certificate subject specified, use self-signed cert */
            sch_ssock->cert_ctx = create_self_signed_cert();

            // -- test code --
            //pj_str_t test = {"test.pjsip.org", 14};
            //sch_ssock->cert_ctx =
            //find_cert_in_stores(&test);
        }
    } else {
        creds.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS;
    }

    /* Verification */
    if (!ssock->is_server) {
        if (ssock->param.verify_peer)
            creds.dwFlags |= SCH_CRED_AUTO_CRED_VALIDATION;
        else
            creds.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
    }

    if (sch_ssock->cert_ctx) {
        creds.cCreds = 1;
        creds.paCred = &sch_ssock->cert_ctx;
    }

    /* Now init the credentials */
    if (param_cnt) {
        creds.cTlsParameters = param_cnt;
        creds.pTlsParameters = params;
    }

    ss = AcquireCredentialsHandle(NULL, UNISP_NAME,
                                  ssock->is_server? SECPKG_CRED_INBOUND :
                                                    SECPKG_CRED_OUTBOUND,
                                  NULL, &creds, NULL, NULL,
                                  &sch_ssock->cred_handle, NULL);
    if (ss < 0) {
        log_sec_err(1, "Failed in AcquireCredentialsHandle()", ss);
        return sec_err_to_pj(ss);
    }

    return PJ_SUCCESS;
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
        status2 = init_creds(ssock);
        if (status2 != PJ_SUCCESS) {
            status = status2;
            goto on_return;
        }
    }

    /* Start handshake iteration */

    pj_lock_acquire(ssock->circ_buf_input_mutex);

    if (!circ_empty(&ssock->circ_buf_input)) {
        data_in = sch_ssock->read_buf;
        data_in_size = PJ_MIN(sch_ssock->read_buf_cap,
                              circ_size(&ssock->circ_buf_input));
        circ_read(&ssock->circ_buf_input, data_in, data_in_size);
    }

    SecBuffer buf_in[2] = { {0} };
    buf_in[0].BufferType = SECBUFFER_TOKEN;
    buf_in[0].pvBuffer = data_in;
    buf_in[0].cbBuffer = (ULONG)data_in_size;
    buf_in[1].BufferType = SECBUFFER_EMPTY;
    SecBufferDesc buf_desc_in = { 0 };
    buf_desc_in.ulVersion = SECBUFFER_VERSION;
    buf_desc_in.cBuffers = ARRAYSIZE(buf_in);
    buf_desc_in.pBuffers = buf_in;

    SecBuffer buf_out[1] = { {0} };
    buf_out[0].BufferType = SECBUFFER_TOKEN;
    buf_out[0].pvBuffer = sch_ssock->write_buf;
    buf_out[0].cbBuffer = (ULONG)sch_ssock->write_buf_cap;
    SecBufferDesc buf_desc_out = { 0 };
    buf_desc_out.ulVersion = SECBUFFER_VERSION;
    buf_desc_out.cBuffers = ARRAYSIZE(buf_out);
    buf_desc_out.pBuffers = buf_out;

    /* As client */
    if (!ssock->is_server) {
        DWORD flags =
            ISC_REQ_USE_SUPPLIED_CREDS |
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
            ASC_REQ_CONFIDENTIALITY |
            ASC_REQ_REPLAY_DETECT |
            ASC_REQ_SEQUENCE_DETECT |
            ASC_REQ_STREAM;

        if (ssock->param.require_client_cert)
            flags |= ASC_REQ_MUTUAL_AUTH;

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
            log_sec_err(1, "Failed to query stream sizes", ss);
            ssl_reset_sock_state(ssock);
            status = sec_err_to_pj(ss);
        }

        /* Adjust maximum message size to our allocated buffer size */
        if (!sch_ssock->write_buf) {
            pj_size_t max_msg = sch_ssock->write_buf_cap -
                                sch_ssock->strm_sizes.cbHeader -
                                sch_ssock->strm_sizes.cbTrailer;

            sch_ssock->strm_sizes.cbMaximumMessage =
                            PJ_MIN((ULONG)max_msg,
                                   sch_ssock->strm_sizes.cbMaximumMessage);
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
            status = sec_err_to_pj(ss);
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
        status = sec_err_to_pj(ss);
    }

    pj_lock_release(ssock->circ_buf_input_mutex);

    if ((ss == SEC_E_OK || ss == SEC_I_CONTINUE_NEEDED) &&
        buf_out[0].cbBuffer > 0 && buf_out[0].pvBuffer)
    {
        /* Queue output data to send */
        status2 = circ_write(&ssock->circ_buf_output, buf_out[0].pvBuffer,
                             buf_out[0].cbBuffer);
        if (status2 != PJ_SUCCESS) {
            PJ_PERROR(1,(SENDER, status2,
                         "Failed to queue handshake packets"));
            status = status2;
        }
    }

    /* Send handshake packets to wire */
    status2 = flush_circ_buf_output(ssock, &ssock->handshake_op_key, 0, 0);
    if (status2 != PJ_SUCCESS && status2 != PJ_EPENDING) {
        PJ_PERROR(1,(SENDER, status2, "Failed to send handshake packets"));
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
        size_ = PJ_MIN(sch_ssock->read_buf_cap,
                       circ_size(&ssock->circ_buf_input));
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
    buf[0].cbBuffer = (ULONG)size_;
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

                /* Store any excess in the decrypted buffer */
                if (len)
                    circ_write(&sch_ssock->decrypted_buf, p, len);

                LOG_DEBUG2("Read %d: after decrypt, excess=%d",
                           requested, len);
            } else {
                /* Not enough, just give everything */
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
        status = sec_err_to_pj(ss);
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
        buf[1].cbBuffer = (ULONG)write_len;
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
            status = (ss==SEC_E_CONTEXT_EXPIRED)? PJ_EEOF : sec_err_to_pj(ss);
            break;
        }

        out_size = (pj_ssize_t)buf[0].cbBuffer + buf[1].cbBuffer +
                               buf[2].cbBuffer;
        status = circ_write(&ssock->circ_buf_output, sch_ssock->write_buf,
                            out_size);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1, (SENDER, status,
                          "Failed to queue outgoing packets"));
            break;
        }

        total += write_len;
    }

    pj_lock_release(ssock->write_mutex);

    *nwritten = (int)total;

    return status;
}


#endif  /* PJ_HAS_SSL_SOCK */

/* $Id$ */
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

/* Only build when PJ_HAS_SSL_SOCK is enabled and when the backend is
 * OpenSSL.
 */
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0 && \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL)

#include "ssl_sock_imp_common.c"

#define THIS_FILE		"ssl_sock_ossl.c"

/* 
 * Include OpenSSL headers 
 */
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#if !defined(OPENSSL_NO_DH)
#   include <openssl/dh.h>
#endif

#include <openssl/rand.h>
#include <openssl/opensslconf.h>
#include <openssl/opensslv.h>

#if defined(LIBRESSL_VERSION_NUMBER)
#	define USING_LIBRESSL 1
#else
#	define USING_LIBRESSL 0
#endif

#if !USING_LIBRESSL && !defined(OPENSSL_NO_EC) \
	&& OPENSSL_VERSION_NUMBER >= 0x1000200fL

#   include <openssl/obj_mac.h>

static const unsigned nid_cid_map[] = {
    NID_sect163k1,              /* sect163k1 (1) */
    NID_sect163r1,              /* sect163r1 (2) */
    NID_sect163r2,              /* sect163r2 (3) */
    NID_sect193r1,              /* sect193r1 (4) */
    NID_sect193r2,              /* sect193r2 (5) */
    NID_sect233k1,              /* sect233k1 (6) */
    NID_sect233r1,              /* sect233r1 (7) */
    NID_sect239k1,              /* sect239k1 (8) */
    NID_sect283k1,              /* sect283k1 (9) */
    NID_sect283r1,              /* sect283r1 (10) */
    NID_sect409k1,              /* sect409k1 (11) */
    NID_sect409r1,              /* sect409r1 (12) */
    NID_sect571k1,              /* sect571k1 (13) */
    NID_sect571r1,              /* sect571r1 (14) */
    NID_secp160k1,              /* secp160k1 (15) */
    NID_secp160r1,              /* secp160r1 (16) */
    NID_secp160r2,              /* secp160r2 (17) */
    NID_secp192k1,              /* secp192k1 (18) */
    NID_X9_62_prime192v1,       /* secp192r1 (19) */
    NID_secp224k1,              /* secp224k1 (20) */
    NID_secp224r1,              /* secp224r1 (21) */
    NID_secp256k1,              /* secp256k1 (22) */
    NID_X9_62_prime256v1,       /* secp256r1 (23) */
    NID_secp384r1,              /* secp384r1 (24) */
    NID_secp521r1,              /* secp521r1 (25) */
    NID_brainpoolP256r1,        /* brainpoolP256r1 (26) */
    NID_brainpoolP384r1,        /* brainpoolP384r1 (27) */
    NID_brainpoolP512r1         /* brainpoolP512r1 (28) */
};

static unsigned get_cid_from_nid(unsigned nid)
{
    unsigned i, cid = 0;
    for (i=0; i<PJ_ARRAY_SIZE(nid_cid_map); ++i) {
	if (nid == nid_cid_map[i]) {
	    cid = i+1;
	    break;
	}
    }
    return cid;
}

static unsigned get_nid_from_cid(unsigned cid)
{
    if ((cid == 0) || (cid > PJ_ARRAY_SIZE(nid_cid_map)))
	return 0;

    return nid_cid_map[cid-1];
}

#endif


#if !USING_LIBRESSL && OPENSSL_VERSION_NUMBER >= 0x10100000L
#  define OPENSSL_NO_SSL2	    /* seems to be removed in 1.1.0 */
#  define M_ASN1_STRING_data(x)	    ASN1_STRING_get0_data(x)
#  define M_ASN1_STRING_length(x)   ASN1_STRING_length(x)
#  if defined(OPENSSL_API_COMPAT) && OPENSSL_API_COMPAT >= 0x10100000L
#     define X509_get_notBefore(x)  X509_get0_notBefore(x)
#     define X509_get_notAfter(x)   X509_get0_notAfter(x)
#  endif
#else
#  define SSL_CIPHER_get_id(c)	    (c)->id
#  define SSL_set_session(ssl, s)   (ssl)->session = (s)
#endif


#ifdef _MSC_VER
#  if OPENSSL_VERSION_NUMBER >= 0x10100000L
#    pragma comment(lib, "libcrypto")
#    pragma comment(lib, "libssl")
#    pragma comment(lib, "crypt32")
#  else
#    pragma comment(lib, "libeay32")
#    pragma comment(lib, "ssleay32")
#  endif
#endif


#if defined(PJ_WIN32) && PJ_WIN32 != 0 || \
    defined(PJ_WIN64) && PJ_WIN64 != 0
#  ifdef _MSC_VER
#    define strerror_r(err,buf,len) strerror_s(buf,len,err)
#  else
#    define strerror_r(err,buf,len) pj_ansi_strncpy(buf,strerror(err),len)
#  endif
#endif


/* Suppress compile warning of OpenSSL deprecation (OpenSSL is deprecated
 * since MacOSX 10.7).
 */
#if defined(PJ_DARWINOS) && PJ_DARWINOS==1
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif


/*
 * Secure socket structure definition.
 */
typedef struct ossl_sock_t
{
    pj_ssl_sock_t  	  base;

    SSL_CTX		 *ossl_ctx;
    SSL			 *ossl_ssl;
    BIO			 *ossl_rbio;
    BIO			 *ossl_wbio;
} ossl_sock_t;

/**
 * Mapping from OpenSSL error codes to pjlib error space.
 */

#define PJ_SSL_ERRNO_START		(PJ_ERRNO_START_USER + \
					 PJ_ERRNO_SPACE_SIZE*6)

#define PJ_SSL_ERRNO_SPACE_SIZE		PJ_ERRNO_SPACE_SIZE

/* Expected maximum value of reason component in OpenSSL error code */
#define MAX_OSSL_ERR_REASON		1200


static char *SSLErrorString (int err)
{
    switch (err) {
    case SSL_ERROR_NONE:
	return "SSL_ERROR_NONE";
    case SSL_ERROR_ZERO_RETURN:
	return "SSL_ERROR_ZERO_RETURN";
    case SSL_ERROR_WANT_READ:
	return "SSL_ERROR_WANT_READ";
    case SSL_ERROR_WANT_WRITE:
	return "SSL_ERROR_WANT_WRITE";
    case SSL_ERROR_WANT_CONNECT:
	return "SSL_ERROR_WANT_CONNECT";
    case SSL_ERROR_WANT_ACCEPT:
	return "SSL_ERROR_WANT_ACCEPT";
    case SSL_ERROR_WANT_X509_LOOKUP:
	return "SSL_ERROR_WANT_X509_LOOKUP";
    case SSL_ERROR_SYSCALL:
	return "SSL_ERROR_SYSCALL";
    case SSL_ERROR_SSL:
	return "SSL_ERROR_SSL";
    default:
	return "SSL_ERROR_UNKNOWN";
    }
}

#define ERROR_LOG(msg, err, ssock) \
{ \
    char buf[PJ_INET6_ADDRSTRLEN+10]; \
    PJ_LOG(2,("SSL", "%s (%s): Level: %d err: <%lu> <%s-%s-%s> len: %d " \
	   "peer: %s", \
	   msg, action, level, err, \
	   (ERR_lib_error_string(err)? ERR_lib_error_string(err): "???"), \
	   (ERR_func_error_string(err)? ERR_func_error_string(err):"???"),\
	   (ERR_reason_error_string(err)? \
	    ERR_reason_error_string(err): "???"), len, \
	   (ssock && pj_sockaddr_has_addr(&ssock->rem_addr)? \
	    pj_sockaddr_print(&ssock->rem_addr, buf, sizeof(buf), 3):"???")));\
}

static void SSLLogErrors(char * action, int ret, int ssl_err, int len, 
			 pj_ssl_sock_t *ssock)
{
    char *ssl_err_str = SSLErrorString(ssl_err);

    if (!action) {
	action = "UNKNOWN";
    }

    switch (ssl_err) {
    case SSL_ERROR_SYSCALL:
    {
	unsigned long err2 = ERR_get_error();
	if (err2) {
	    int level = 0;
	    while (err2) {
	        ERROR_LOG("SSL_ERROR_SYSCALL", err2, ssock);
		level++;
		err2 = ERR_get_error();
	    }
	} else if (ret == 0) {
	    /* An EOF was observed that violates the protocol */

	    /* The TLS/SSL handshake was not successful but was shut down
	     * controlled and by the specifications of the TLS/SSL protocol.
	     */
	} else if (ret == -1) {
	    /* BIO error - look for more info in errno... */
	    char errStr[250] = "";
	    strerror_r(errno, errStr, sizeof(errStr));
	    /* for now - continue logging these if they occur.... */
	    PJ_LOG(4,("SSL", "BIO error, SSL_ERROR_SYSCALL (%s): "
	    		     "errno: <%d> <%s> len: %d",
		      	     action, errno, errStr, len));
	} else {
	    /* ret!=0 & ret!=-1 & nothing on error stack - is this valid??? */
	    PJ_LOG(2,("SSL", "SSL_ERROR_SYSCALL (%s) ret: %d len: %d",
		      action, ret, len));
	}
	break;
    }
    case SSL_ERROR_SSL:
    {
	unsigned long err2 = ERR_get_error();
	int level = 0;

	while (err2) {
	    ERROR_LOG("SSL_ERROR_SSL", err2, ssock);
	    level++;
	    err2 = ERR_get_error();
	}
	break;
    }
    default:
	PJ_LOG(2,("SSL", "%lu [%s] (%s) ret: %d len: %d",
		  ssl_err, ssl_err_str, action, ret, len));
	break;
    }
}


static pj_status_t GET_STATUS_FROM_SSL_ERR(unsigned long err)
{
    pj_status_t status;

    /* OpenSSL error range is much wider than PJLIB errno space, so
     * if it exceeds the space, only the error reason will be kept.
     * Note that the last native error will be kept as is and can be
     * retrieved via SSL socket info.
     */
    status = ERR_GET_LIB(err)*MAX_OSSL_ERR_REASON + ERR_GET_REASON(err);
    if (status > PJ_SSL_ERRNO_SPACE_SIZE)
	status = ERR_GET_REASON(err);

    status += PJ_SSL_ERRNO_START;
    return status;
}

/* err contains ERR_get_error() status */
static pj_status_t STATUS_FROM_SSL_ERR(char *action, pj_ssl_sock_t *ssock,
				       unsigned long err)
{
    int level = 0;
    int len = 0; //dummy

    ERROR_LOG("STATUS_FROM_SSL_ERR", err, ssock);
    level++;

    /* General SSL error, dig more from OpenSSL error queue */
    if (err == SSL_ERROR_SSL) {
	err = ERR_get_error();
	ERROR_LOG("STATUS_FROM_SSL_ERR", err, ssock);
    }

    if (ssock)
	ssock->last_err = err;
    return GET_STATUS_FROM_SSL_ERR(err);
}

/* err contains SSL_get_error() status */
static pj_status_t STATUS_FROM_SSL_ERR2(char *action, pj_ssl_sock_t *ssock,
					int ret, int err, int len)
{
    unsigned long ssl_err = err;

    if (err == SSL_ERROR_SSL) {
	ssl_err = ERR_peek_error();
    }

    /* Dig for more from OpenSSL error queue */
    SSLLogErrors(action, ret, err, len, ssock);

    if (ssock)
	ssock->last_err = ssl_err;
    return GET_STATUS_FROM_SSL_ERR(ssl_err);
}

static pj_status_t GET_SSL_STATUS(pj_ssl_sock_t *ssock)
{
    return STATUS_FROM_SSL_ERR("status", ssock, ERR_get_error());
}


/*
 * Get error string of OpenSSL.
 */
static pj_str_t ssl_strerror(pj_status_t status, 
			     char *buf, pj_size_t bufsize)
{
    pj_str_t errstr;
    unsigned long ssl_err = status;

    if (ssl_err) {
	unsigned long l, r;
	ssl_err -= PJ_SSL_ERRNO_START;
	l = ssl_err / MAX_OSSL_ERR_REASON;
	r = ssl_err % MAX_OSSL_ERR_REASON;
	ssl_err = ERR_PACK(l, 0, r);
    }

#if defined(PJ_HAS_ERROR_STRING) && (PJ_HAS_ERROR_STRING != 0)

    {
	const char *tmp = NULL;
	tmp = ERR_reason_error_string(ssl_err);
	if (tmp) {
	    pj_ansi_strncpy(buf, tmp, bufsize);
	    errstr = pj_str(buf);
	    return errstr;
	}
    }

#endif	/* PJ_HAS_ERROR_STRING */

    errstr.ptr = buf;
    errstr.slen = pj_ansi_snprintf(buf, bufsize, 
				   "Unknown OpenSSL error %lu",
				   ssl_err);
    if (errstr.slen < 1 || errstr.slen >= (int)bufsize)
	errstr.slen = bufsize - 1;
    return errstr;
}

/* Additional ciphers recognized by SSL_set_cipher_list()
   but not returned from SSL_get_ciphers().
   NOTE: ids are designed to not conflict with those from
         SSL_get_cipher() which get masked to the lower 24
         bits before use. 
*/
static const struct ssl_ciphers_t ADDITIONAL_CIPHERS[] = {
        {0xFF000000, "DEFAULT"},
        {0xFF000001, "@SECLEVEL=1"},
        {0xFF000002, "@SECLEVEL=2"},
        {0xFF000003, "@SECLEVEL=3"},
        {0xFF000004, "@SECLEVEL=4"},
        {0xFF000005, "@SECLEVEL=5"}
};
static const unsigned int ADDITIONAL_CIPHER_COUNT = 
    sizeof (ADDITIONAL_CIPHERS) / sizeof (ADDITIONAL_CIPHERS[0]);

/*
 *******************************************************************
 * I/O functions.
 *******************************************************************
 */

static pj_bool_t io_empty(pj_ssl_sock_t *ssock, circ_buf_t *cb)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;

    PJ_UNUSED_ARG(cb);

    return !BIO_pending(ossock->ossl_wbio);
}

static pj_size_t io_size(pj_ssl_sock_t *ssock, circ_buf_t *cb)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    char *data;

    PJ_UNUSED_ARG(cb);

    return BIO_get_mem_data(ossock->ossl_wbio, &data);
}

static void io_read(pj_ssl_sock_t *ssock, circ_buf_t *cb,
		    pj_uint8_t *dst, pj_size_t len)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    char *data;

    PJ_UNUSED_ARG(cb);

    BIO_get_mem_data(ossock->ossl_wbio, &data);
    pj_memcpy(dst, data, len);

    /* Reset write BIO */
    (void)BIO_reset(ossock->ossl_wbio);
}

static pj_status_t io_write(pj_ssl_sock_t *ssock, circ_buf_t *cb,
                            const pj_uint8_t *src, pj_size_t len)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    int nwritten;

    nwritten = BIO_write(ossock->ossl_rbio, src, (int)len);
    return (nwritten < (int)len)? GET_SSL_STATUS(cb->owner): PJ_SUCCESS;
}

/*
 *******************************************************************
 */

/* OpenSSL library initialization counter */
static int openssl_init_count;

/* OpenSSL application data index */
static int sslsock_idx;

#if defined(PJ_SSL_SOCK_OSSL_USE_THREAD_CB) && \
    PJ_SSL_SOCK_OSSL_USE_THREAD_CB != 0 && OPENSSL_VERSION_NUMBER < 0x10100000L

/* Thread lock pool.*/
static pj_caching_pool 	 cp;
static pj_pool_t 	*lock_pool;

/* OpenSSL locking list. */
static pj_lock_t **ossl_locks;

/* OpenSSL number locks. */
static unsigned ossl_num_locks;

#if     OPENSSL_VERSION_NUMBER >= 0x10000000
static void ossl_set_thread_id(CRYPTO_THREADID *id)
{
    CRYPTO_THREADID_set_numeric(id,
                     (unsigned long)pj_thread_get_os_handle(pj_thread_this()));
}

#else

static unsigned long ossl_thread_id(void)
{
    return ((unsigned long)pj_thread_get_os_handle(pj_thread_this()));
}
#endif

static void ossl_lock(int mode, int id, const char *file, int line)
{
    PJ_UNUSED_ARG(file);
    PJ_UNUSED_ARG(line);

    if (openssl_init_count == 0)
        return;

    if (mode & CRYPTO_LOCK) {
        if (ossl_locks[id]) {
            //PJ_LOG(6, (THIS_FILE, "Lock File (%s) Line(%d)", file, line));
            pj_lock_acquire(ossl_locks[id]);
        }
    } else {
        if (ossl_locks[id]) {
            //PJ_LOG(6, (THIS_FILE, "Unlock File (%s) Line(%d)", file, line));
            pj_lock_release(ossl_locks[id]);
        }
    }
}

static void release_thread_cb(void)
{
    unsigned i = 0;

#if     OPENSSL_VERSION_NUMBER >= 0x10000000
    CRYPTO_THREADID_set_callback(NULL);
#else
    CRYPTO_set_id_callback(NULL);
#endif
    CRYPTO_set_locking_callback(NULL);

    for (; i < ossl_num_locks; ++i) {
        if (ossl_locks[i]) {
            pj_lock_destroy(ossl_locks[i]);
            ossl_locks[i] = NULL;
        }
    }
    if (lock_pool) {
        pj_pool_release(lock_pool);
        lock_pool = NULL;
        pj_caching_pool_destroy(&cp);
    }
    ossl_locks = NULL;
    ossl_num_locks = 0;
}

static pj_status_t init_ossl_lock()
{
    pj_status_t status = PJ_SUCCESS;

    pj_caching_pool_init(&cp, NULL, 0);

    lock_pool = pj_pool_create(&cp.factory,
                               "ossl-lock",
                               64,
                               64,
                               NULL);

    if (!lock_pool) {
        status = PJ_ENOMEM;
        PJ_PERROR(1, (THIS_FILE, status,"Fail creating OpenSSL lock pool"));
        pj_caching_pool_destroy(&cp);
        return status;
    }

    ossl_num_locks = CRYPTO_num_locks();
    ossl_locks = (pj_lock_t **)pj_pool_calloc(lock_pool,
                                              ossl_num_locks,
                                              sizeof(pj_lock_t*));

    if (ossl_locks) {
        unsigned i = 0;
        for (; (i < ossl_num_locks) && (status == PJ_SUCCESS); ++i) {
            status = pj_lock_create_simple_mutex(lock_pool, "ossl_lock%p",
                                                 &ossl_locks[i]);
        }
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1, (THIS_FILE, status,
                          "Fail creating mutex for OpenSSL lock"));
            release_thread_cb();
            return status;
        }

#if     OPENSSL_VERSION_NUMBER >= 0x10000000
        CRYPTO_THREADID_set_callback(ossl_set_thread_id);
#else
        CRYPTO_set_id_callback(ossl_thread_id);
#endif
        CRYPTO_set_locking_callback(ossl_lock);
        status = pj_atexit(&release_thread_cb);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1, (THIS_FILE, status, "Warning! Unable to set OpenSSL "
                          "lock thread callback unrelease method."));
        }
    } else {
        status = PJ_ENOMEM;
        PJ_PERROR(1, (THIS_FILE, status,"Fail creating OpenSSL locks"));
        release_thread_cb();
    }
    return status;
}

#endif

/* Initialize OpenSSL */
static pj_status_t init_openssl(void)
{
    pj_status_t status;

    if (openssl_init_count)
	return PJ_SUCCESS;

    openssl_init_count = 1;

    /* Register error subsystem */
    status = pj_register_strerror(PJ_SSL_ERRNO_START, 
				  PJ_SSL_ERRNO_SPACE_SIZE, 
				  &ssl_strerror);
    pj_assert(status == PJ_SUCCESS);

    /* Init OpenSSL lib */
#if USING_LIBRESSL || OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
    SSL_load_error_strings();
#else
    OPENSSL_init_ssl(0, NULL);
#endif
#if OPENSSL_VERSION_NUMBER < 0x009080ffL
    /* This is now synonym of SSL_library_init() */
    OpenSSL_add_all_algorithms();
#endif

    /* Init available ciphers */
    if (ssl_cipher_num == 0 || ssl_curves_num == 0) {
	SSL_METHOD *meth = NULL;
	SSL_CTX *ctx;
	SSL *ssl;
	STACK_OF(SSL_CIPHER) *sk_cipher;
	SSL_SESSION *ssl_sess;
	unsigned i, n;
	int nid;
	const char *cname;

#if (USING_LIBRESSL && LIBRESSL_VERSION_NUMBER < 0x2020100fL)\
    || OPENSSL_VERSION_NUMBER < 0x10100000L

	meth = (SSL_METHOD*)SSLv23_server_method();
	if (!meth)
	    meth = (SSL_METHOD*)TLSv1_server_method();
#ifndef OPENSSL_NO_SSL3_METHOD
	if (!meth)
	    meth = (SSL_METHOD*)SSLv3_server_method();
#endif
#ifndef OPENSSL_NO_SSL2
	if (!meth)
	    meth = (SSL_METHOD*)SSLv2_server_method();
#endif

#else
	/* Specific version methods are deprecated in 1.1.0 */
	meth = (SSL_METHOD*)TLS_method();
#endif

	pj_assert(meth);

	ctx=SSL_CTX_new(meth);
	SSL_CTX_set_cipher_list(ctx, "ALL:COMPLEMENTOFALL");

	ssl = SSL_new(ctx);

	sk_cipher = SSL_get_ciphers(ssl);

	n = sk_SSL_CIPHER_num(sk_cipher);
	if (n > PJ_ARRAY_SIZE(ssl_ciphers) - ADDITIONAL_CIPHER_COUNT)
	    n = PJ_ARRAY_SIZE(ssl_ciphers) - ADDITIONAL_CIPHER_COUNT;

	for (i = 0; i < n; ++i) {
	    const SSL_CIPHER *c;
	    c = sk_SSL_CIPHER_value(sk_cipher,i);
	    ssl_ciphers[i].id = (pj_ssl_cipher)
				    (pj_uint32_t)SSL_CIPHER_get_id(c) &
				    0x00FFFFFF;
	    ssl_ciphers[i].name = SSL_CIPHER_get_name(c);
	}

	/* Add cipher aliases not returned from SSL_get_ciphers() */
	for (i = 0; i < ADDITIONAL_CIPHER_COUNT; ++i) {
	    ssl_ciphers[n++] = ADDITIONAL_CIPHERS[i];
	}
	ssl_cipher_num = n;

	ssl_sess = SSL_SESSION_new();
	SSL_set_session(ssl, ssl_sess);

#if !USING_LIBRESSL && !defined(OPENSSL_NO_EC) \
    && OPENSSL_VERSION_NUMBER >= 0x1000200fL
#if OPENSSL_VERSION_NUMBER >= 0x1010100fL
	ssl_curves_num = EC_get_builtin_curves(NULL, 0);
#else
	ssl_curves_num = SSL_get_shared_curve(ssl,-1);

	if (ssl_curves_num > PJ_ARRAY_SIZE(ssl_curves))
	    ssl_curves_num = PJ_ARRAY_SIZE(ssl_curves);
#endif

	if( ssl_curves_num > 0 ) {
#if OPENSSL_VERSION_NUMBER >= 0x1010100fL
	    EC_builtin_curve * curves = NULL;

	    curves = OPENSSL_malloc((int)sizeof(*curves) * ssl_curves_num);
	    if (!EC_get_builtin_curves(curves, ssl_curves_num)) {
		OPENSSL_free(curves);
		curves = NULL;
		ssl_curves_num = 0;
	    }

	    n = ssl_curves_num;
	    ssl_curves_num = 0;

	    for (i = 0; i < n; i++) {
		nid = curves[i].nid;

		if ( 0 != get_cid_from_nid(nid) ) {
		    cname = OBJ_nid2sn(nid);

		    if (!cname)
			cname = OBJ_nid2sn(nid);

		    if (cname) {
			ssl_curves[ssl_curves_num].id = get_cid_from_nid(nid);
			ssl_curves[ssl_curves_num].name = cname;

			ssl_curves_num++;

			if (ssl_curves_num >= PJ_SSL_SOCK_MAX_CURVES )
			    break;
		    }
		}
	    }

	    if(curves)
		OPENSSL_free(curves);
#else
	for (i = 0; i < ssl_curves_num; i++) {
	    nid = SSL_get_shared_curve(ssl, i);

	    if (nid & TLSEXT_nid_unknown) {
		cname = "curve unknown";
		nid &= 0xFFFF;
	    } else {
		cname = EC_curve_nid2nist(nid);
		if (!cname)
		    cname = OBJ_nid2sn(nid);
	    }

	    ssl_curves[i].id   = get_cid_from_nid(nid);
	    ssl_curves[i].name = cname;
	}
#endif

	}
#else
	PJ_UNUSED_ARG(nid);
	PJ_UNUSED_ARG(cname);
	ssl_curves_num = 0;
#endif

	SSL_free(ssl);

	/* On OpenSSL 1.1.1, omitting SSL_SESSION_free() will cause 
	 * memory leak (e.g: as reported by Address Sanitizer). But using
	 * SSL_SESSION_free() may cause crash (due to double free?) on 1.0.x.
	 * As OpenSSL docs specifies to not calling SSL_SESSION_free() after
	 * SSL_free(), perhaps it is safer to obey this, the leak amount seems
	 * to be relatively small (<500 bytes) and should occur once only in
	 * the library lifetime.
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
	SSL_SESSION_free(ssl_sess);
#endif
	 */

	SSL_CTX_free(ctx);
    }

    /* Create OpenSSL application data index for SSL socket */
    sslsock_idx = SSL_get_ex_new_index(0, "SSL socket", NULL, NULL, NULL);
    if (sslsock_idx == -1) {
	status = STATUS_FROM_SSL_ERR2("Init", NULL, -1, ERR_get_error(), 0);
	PJ_LOG(1,(THIS_FILE,
	       "Fatal error: failed to get application data index for "
	       "SSL socket"));
	return status;
    }

#if defined(PJ_SSL_SOCK_OSSL_USE_THREAD_CB) && \
    PJ_SSL_SOCK_OSSL_USE_THREAD_CB != 0 && OPENSSL_VERSION_NUMBER < 0x10100000L

    status = init_ossl_lock();
    if (status != PJ_SUCCESS)
        return status;
#endif

    return status;
}

/* Shutdown OpenSSL */
static void shutdown_openssl(void)
{
    PJ_UNUSED_ARG(openssl_init_count);
}

/* SSL password callback. */
static int password_cb(char *buf, int num, int rwflag, void *user_data)
{
    pj_ssl_cert_t *cert = (pj_ssl_cert_t*) user_data;

    PJ_UNUSED_ARG(rwflag);

    if(num < cert->privkey_pass.slen)
	return 0;
    
    pj_memcpy(buf, cert->privkey_pass.ptr, cert->privkey_pass.slen);
    return (int)cert->privkey_pass.slen;
}


/* SSL certificate verification result callback.
 * Note that this callback seems to be always called from library worker
 * thread, e.g: active socket on_read_complete callback, which should have
 * already been equipped with race condition avoidance mechanism (should not
 * be destroyed while callback is being invoked).
 */
static int verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
    pj_ssl_sock_t *ssock = NULL;
    SSL *ossl_ssl = NULL;
    int err;

    /* Get SSL instance */
    ossl_ssl = X509_STORE_CTX_get_ex_data(x509_ctx, 
				    SSL_get_ex_data_X509_STORE_CTX_idx());
    if (!ossl_ssl) {
	PJ_LOG(1,(THIS_FILE,
		  "SSL verification callback failed to get SSL instance"));
	goto on_return;
    }

    /* Get SSL socket instance */
    ssock = SSL_get_ex_data(ossl_ssl, sslsock_idx);
    if (!ssock) {
	/* SSL socket may have been destroyed */
	PJ_LOG(1,(THIS_FILE,
		  "SSL verification callback failed to get SSL socket "
		  "instance (sslsock_idx=%d).", sslsock_idx));
	goto on_return;
    }

    /* Store verification status */
    err = X509_STORE_CTX_get_error(x509_ctx);
    switch (err) {
    case X509_V_OK:
	break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
	ssock->verify_status |= PJ_SSL_CERT_EISSUER_NOT_FOUND;
	break;

    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
	ssock->verify_status |= PJ_SSL_CERT_EINVALID_FORMAT;
	break;

    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_HAS_EXPIRED:
	ssock->verify_status |= PJ_SSL_CERT_EVALIDITY_PERIOD;
	break;

    case X509_V_ERR_UNABLE_TO_GET_CRL:
    case X509_V_ERR_CRL_NOT_YET_VALID:
    case X509_V_ERR_CRL_HAS_EXPIRED:
    case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
    case X509_V_ERR_CRL_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
    case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
	ssock->verify_status |= PJ_SSL_CERT_ECRL_FAILURE;
	break;	

    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	ssock->verify_status |= PJ_SSL_CERT_EUNTRUSTED;
	break;	

    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
    case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
    case X509_V_ERR_AKID_SKID_MISMATCH:
    case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
    case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
	ssock->verify_status |= PJ_SSL_CERT_EISSUER_MISMATCH;
	break;

    case X509_V_ERR_CERT_REVOKED:
	ssock->verify_status |= PJ_SSL_CERT_EREVOKED;
	break;	

    case X509_V_ERR_INVALID_PURPOSE:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_INVALID_CA:
	ssock->verify_status |= PJ_SSL_CERT_EINVALID_PURPOSE;
	break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG: /* not really used */
    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
	ssock->verify_status |= PJ_SSL_CERT_ECHAIN_TOO_LONG;
	break;

    /* Unknown errors */
    case X509_V_ERR_OUT_OF_MEM:
    default:
	ssock->verify_status |= PJ_SSL_CERT_EUNKNOWN;
	break;
    }

    /* When verification is not requested just return ok here, however
     * application can still get the verification status.
     */
    if (PJ_FALSE == ssock->param.verify_peer)
	preverify_ok = 1;

on_return:
    return preverify_ok;
}

/* Setting SSL sock cipher list */
static pj_status_t set_cipher_list(pj_ssl_sock_t *ssock);
/* Setting SSL sock curves list */
static pj_status_t set_curves_list(pj_ssl_sock_t *ssock);
/* Setting sigalgs list */
static pj_status_t set_sigalgs(pj_ssl_sock_t *ssock);
/* Setting entropy for rng */
static void set_entropy(pj_ssl_sock_t *ssock);


static pj_ssl_sock_t *ssl_alloc(pj_pool_t *pool)
{
    return (pj_ssl_sock_t *)PJ_POOL_ZALLOC_T(pool, ossl_sock_t);
}

static int xname_cmp(const X509_NAME * const *a, const X509_NAME * const *b) {
  return X509_NAME_cmp(*a, *b);
}

/* Create and initialize new SSL context and instance */
static pj_status_t ssl_create(pj_ssl_sock_t *ssock)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
#if !defined(OPENSSL_NO_DH)
    BIO *bio;
    DH *dh;
    long options;
#endif
    SSL_METHOD *ssl_method = NULL;
    SSL_CTX *ctx;
    pj_uint32_t ssl_opt = 0;
    pj_ssl_cert_t *cert;
    int mode, rc;
    pj_status_t status;
        
    pj_assert(ssock);

    cert = ssock->cert;

    /* Make sure OpenSSL library has been initialized */
    init_openssl();

    set_entropy(ssock);

    if (ssock->param.proto == PJ_SSL_SOCK_PROTO_DEFAULT)
	ssock->param.proto = PJ_SSL_SOCK_PROTO_SSL23;

    /* Determine SSL method to use */
    /* Specific version methods are deprecated since 1.1.0 */
#if (USING_LIBRESSL && LIBRESSL_VERSION_NUMBER < 0x2020100fL)\
    || OPENSSL_VERSION_NUMBER < 0x10100000L
    switch (ssock->param.proto) {
    case PJ_SSL_SOCK_PROTO_TLS1:
	ssl_method = (SSL_METHOD*)TLSv1_method();
	break;
#ifndef OPENSSL_NO_SSL2
    case PJ_SSL_SOCK_PROTO_SSL2:
	ssl_method = (SSL_METHOD*)SSLv2_method();
	break;
#endif
#ifndef OPENSSL_NO_SSL3_METHOD
    case PJ_SSL_SOCK_PROTO_SSL3:
	ssl_method = (SSL_METHOD*)SSLv3_method();
#endif
	break;
    }
#endif

    if (!ssl_method) {
#if (USING_LIBRESSL && LIBRESSL_VERSION_NUMBER < 0x2020100fL)\
    || OPENSSL_VERSION_NUMBER < 0x10100000L
	ssl_method = (SSL_METHOD*)SSLv23_method();
#else
	ssl_method = (SSL_METHOD*)TLS_method();
#endif

#ifdef SSL_OP_NO_SSLv2
	/** Check if SSLv2 is enabled */
	ssl_opt |= ((ssock->param.proto & PJ_SSL_SOCK_PROTO_SSL2)==0)?
		    SSL_OP_NO_SSLv2:0;
#endif

#ifdef SSL_OP_NO_SSLv3
	/** Check if SSLv3 is enabled */
	ssl_opt |= ((ssock->param.proto & PJ_SSL_SOCK_PROTO_SSL3)==0)?
		    SSL_OP_NO_SSLv3:0;
#endif

#ifdef SSL_OP_NO_TLSv1
	/** Check if TLSv1 is enabled */
	ssl_opt |= ((ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1)==0)?
		    SSL_OP_NO_TLSv1:0;
#endif

#ifdef SSL_OP_NO_TLSv1_1
	/** Check if TLSv1_1 is enabled */
	ssl_opt |= ((ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_1)==0)?
		    SSL_OP_NO_TLSv1_1:0;
#endif

#ifdef SSL_OP_NO_TLSv1_2
	/** Check if TLSv1_2 is enabled */
	ssl_opt |= ((ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_2)==0)?
		    SSL_OP_NO_TLSv1_2:0;
#endif

#ifdef SSL_OP_NO_TLSv1_3
	/** Check if TLSv1_3 is enabled */
	ssl_opt |= ((ssock->param.proto & PJ_SSL_SOCK_PROTO_TLS1_3)==0)?
		    SSL_OP_NO_TLSv1_3:0;
#endif

    }

    /* Create SSL context */
    ctx = SSL_CTX_new(ssl_method);
    if (ctx == NULL) {
	return GET_SSL_STATUS(ssock);
    }
    ossock->ossl_ctx = ctx;

    if (ssl_opt)
	SSL_CTX_set_options(ctx, ssl_opt);

    /* Set cipher list */
    status = set_cipher_list(ssock);
    if (status != PJ_SUCCESS)
        return status;

    /* Apply credentials */
    if (cert) {
	/* Load CA list if one is specified. */
	if (cert->CA_file.slen || cert->CA_path.slen) {

	    rc = SSL_CTX_load_verify_locations(
			ctx,
			cert->CA_file.slen == 0 ? NULL : cert->CA_file.ptr,
			cert->CA_path.slen == 0 ? NULL : cert->CA_path.ptr);

	    if (rc != 1) {
		status = GET_SSL_STATUS(ssock);
		if (cert->CA_file.slen) {
		    PJ_PERROR(1,(ssock->pool->obj_name, status,
				 "Error loading CA list file '%s'",
				 cert->CA_file.ptr));
		}
		if (cert->CA_path.slen) {
		    PJ_PERROR(1,(ssock->pool->obj_name, status,
				 "Error loading CA path '%s'",
				 cert->CA_path.ptr));
		}
		SSL_CTX_free(ctx);
		return status;
	    } else {
		PJ_LOG(4,(ssock->pool->obj_name,
			  "CA certificates loaded from '%s%s%s'",
			  cert->CA_file.ptr,
			  ((cert->CA_file.slen && cert->CA_path.slen)?
				" + ":""),
			  cert->CA_path.ptr));
	    }
	}
    
	/* Set password callback */
	if (cert->privkey_pass.slen) {
	    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
	    SSL_CTX_set_default_passwd_cb_userdata(ctx, cert);
	}


	/* Load certificate if one is specified */
	if (cert->cert_file.slen) {

	    /* Load certificate chain from file into ctx */
	    rc = SSL_CTX_use_certificate_chain_file(ctx, cert->cert_file.ptr);

	    if(rc != 1) {
		status = GET_SSL_STATUS(ssock);
		PJ_PERROR(1,(ssock->pool->obj_name, status,
			     "Error loading certificate chain file '%s'",
			     cert->cert_file.ptr));
		SSL_CTX_free(ctx);
		return status;
	    } else {
		PJ_LOG(4,(ssock->pool->obj_name,
			  "Certificate chain loaded from '%s'",
			  cert->cert_file.ptr));
	    }
	}


	/* Load private key if one is specified */
	if (cert->privkey_file.slen) {
	    /* Adds the first private key found in file to ctx */
	    rc = SSL_CTX_use_PrivateKey_file(ctx, cert->privkey_file.ptr, 
					     SSL_FILETYPE_PEM);

	    if(rc != 1) {
		status = GET_SSL_STATUS(ssock);
		PJ_PERROR(1,(ssock->pool->obj_name, status,
			     "Error adding private key from '%s'",
			     cert->privkey_file.ptr));
		SSL_CTX_free(ctx);
		return status;
	    } else {
		PJ_LOG(4,(ssock->pool->obj_name,
			  "Private key loaded from '%s'",
			  cert->privkey_file.ptr));
	    }

#if !defined(OPENSSL_NO_DH)
	    if (ssock->is_server) {
		bio = BIO_new_file(cert->privkey_file.ptr, "r");
		if (bio != NULL) {
		    dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
		    if (dh != NULL) {
			if (SSL_CTX_set_tmp_dh(ctx, dh)) {
			    options = SSL_OP_CIPHER_SERVER_PREFERENCE |
    #if !defined(OPENSSL_NO_ECDH) && OPENSSL_VERSION_NUMBER >= 0x10000000L
				      SSL_OP_SINGLE_ECDH_USE |
    #endif
				      SSL_OP_SINGLE_DH_USE;
			    options = SSL_CTX_set_options(ctx, options);
			    PJ_LOG(4,(ssock->pool->obj_name, "SSL DH "
				     "initialized, PFS cipher-suites enabled"));
			}
			DH_free(dh);
		    }
		    BIO_free(bio);
		}
	    }
#endif
	}

	/* Load from buffer. */
	if (cert->cert_buf.slen) {
	    BIO *cbio;
	    X509 *xcert = NULL;
	    
	    cbio = BIO_new_mem_buf((void*)cert->cert_buf.ptr,
				   cert->cert_buf.slen);
	    if (cbio != NULL) {
		xcert = PEM_read_bio_X509(cbio, NULL, 0, NULL);
		if (xcert != NULL) {
		    rc = SSL_CTX_use_certificate(ctx, xcert);
		    if (rc != 1) {
			status = GET_SSL_STATUS(ssock);
			PJ_PERROR(1,(ssock->pool->obj_name, status,
			      "Error loading chain certificate from buffer"));
			X509_free(xcert);
			BIO_free(cbio);
			SSL_CTX_free(ctx);
			return status;
		    } else {
			PJ_LOG(4,(ssock->pool->obj_name,
				  "Certificate chain loaded from buffer"));
		    }
		    X509_free(xcert);
		}
		BIO_free(cbio);
	    }	    
	}

	if (cert->CA_buf.slen) {
	    BIO *cbio = BIO_new_mem_buf((void*)cert->CA_buf.ptr,
					cert->CA_buf.slen);
	    X509_STORE *cts = SSL_CTX_get_cert_store(ctx);

	    if (cbio && cts) {
		STACK_OF(X509_INFO) *inf = PEM_X509_INFO_read_bio(cbio, NULL, 
								  NULL, NULL);

		if (inf != NULL) {
		    int i = 0, cnt = 0;
		    for (; i < sk_X509_INFO_num(inf); i++) {
			X509_INFO *itmp = sk_X509_INFO_value(inf, i);
			if (!itmp->x509)
			    continue;

			rc = X509_STORE_add_cert(cts, itmp->x509);
			if (rc == 1) {
			    ++cnt;
			} else {
#if PJ_LOG_MAX_LEVEL >= 4
			    char buf[256];
			    PJ_LOG(4,(ssock->pool->obj_name,
				      "Error adding CA cert: %s",
				      X509_NAME_oneline(
					X509_get_subject_name(itmp->x509),
					buf, sizeof(buf))));
#endif
			}
		    }
		    PJ_LOG(4,(ssock->pool->obj_name,
			      "CA certificates loaded from buffer (cnt=%d)",
			      cnt));
		}
		sk_X509_INFO_pop_free(inf, X509_INFO_free);
		BIO_free(cbio);
	    }
	}

	if (cert->privkey_buf.slen) {
	    BIO *kbio;	    
	    EVP_PKEY *pkey = NULL;

	    kbio = BIO_new_mem_buf((void*)cert->privkey_buf.ptr,
				   cert->privkey_buf.slen);
	    if (kbio != NULL) {
		pkey = PEM_read_bio_PrivateKey(kbio, NULL, &password_cb,
					       cert);
		if (pkey) {
		    rc = SSL_CTX_use_PrivateKey(ctx, pkey);
		    if (rc != 1) {
			status = GET_SSL_STATUS(ssock);
			PJ_PERROR(1,(ssock->pool->obj_name, status,
				     "Error adding private key from buffer"));
			EVP_PKEY_free(pkey);
			BIO_free(kbio);
			SSL_CTX_free(ctx);
			return status;
		    } else {
			PJ_LOG(4,(ssock->pool->obj_name,
				  "Private key loaded from buffer"));
		    }
		    EVP_PKEY_free(pkey);
		} else {
		    PJ_LOG(1,(ssock->pool->obj_name,
			      "Error reading private key from buffer"));
		}

		if (ssock->is_server) {
		    dh = PEM_read_bio_DHparams(kbio, NULL, NULL, NULL);
		    if (dh != NULL) {
			if (SSL_CTX_set_tmp_dh(ctx, dh)) {
			    options = SSL_OP_CIPHER_SERVER_PREFERENCE |
    #if !defined(OPENSSL_NO_ECDH) && OPENSSL_VERSION_NUMBER >= 0x10000000L
				      SSL_OP_SINGLE_ECDH_USE |
    #endif
				      SSL_OP_SINGLE_DH_USE;
			    options = SSL_CTX_set_options(ctx, options);
			    PJ_LOG(4,(ssock->pool->obj_name, "SSL DH "
				     "initialized, PFS cipher-suites enabled"));
			}
			DH_free(dh);
		    }
		}
		BIO_free(kbio);
	    }	    
	}
    }

    if (ssock->is_server) {
	char *p = NULL;

	/* If certificate file name contains "_rsa.", let's check if there are
	 * ecc and dsa certificates too.
	 */
	if (cert && cert->cert_file.slen) {
	    const pj_str_t RSA = {"_rsa.", 5};
	    p = pj_strstr(&cert->cert_file, &RSA);
	    if (p) p++; /* Skip underscore */
	}
	if (p) {
	    /* Certificate type string length must be exactly 3 */
	    enum { CERT_TYPE_LEN = 3 };
	    const char* cert_types[] = { "ecc", "dsa" };
	    char *cf = cert->cert_file.ptr;
	    int i;

	    /* Check and load ECC & DSA certificates & private keys */
	    for (i = 0; i < PJ_ARRAY_SIZE(cert_types); ++i) {
		int err;

		pj_memcpy(p, cert_types[i], CERT_TYPE_LEN);
		if (!pj_file_exists(cf))
		    continue;

		err = SSL_CTX_use_certificate_chain_file(ctx, cf);
		if (err == 1)
		    err = SSL_CTX_use_PrivateKey_file(ctx, cf,
						      SSL_FILETYPE_PEM);
		if (err == 1) {
		    PJ_LOG(4,(ssock->pool->obj_name,
			      "Additional certificate '%s' loaded.", cf));
		} else {
		    PJ_PERROR(1,(ssock->pool->obj_name, GET_SSL_STATUS(ssock),
				 "Error loading certificate file '%s'", cf));
		    ERR_clear_error();
		}
	    }

	    /* Put back original name */
	    pj_memcpy(p, "rsa", CERT_TYPE_LEN);
	}

    #ifndef SSL_CTRL_SET_ECDH_AUTO
	#define SSL_CTRL_SET_ECDH_AUTO 94
    #endif

	/* SSL_CTX_set_ecdh_auto(ctx,on) requires OpenSSL 1.0.2 which wraps: */
	if (SSL_CTX_ctrl(ctx, SSL_CTRL_SET_ECDH_AUTO, 1, NULL)) {
	    PJ_LOG(4,(ssock->pool->obj_name, "SSL ECDH initialized "
		      "(automatic), faster PFS ciphers enabled"));
    #if !defined(OPENSSL_NO_ECDH) && OPENSSL_VERSION_NUMBER >= 0x10000000L && \
	OPENSSL_VERSION_NUMBER < 0x10100000L
	} else {
	    /* enables AES-128 ciphers, to get AES-256 use NID_secp384r1 */
	    EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	    if (ecdh != NULL) {
		if (SSL_CTX_set_tmp_ecdh(ctx, ecdh)) {
		    PJ_LOG(4,(ssock->pool->obj_name, "SSL ECDH initialized "
			      "(secp256r1), faster PFS cipher-suites enabled"));
		}
		EC_KEY_free(ecdh);
	    }
    #endif
	}
    } else {
	X509_STORE *pkix_validation_store = SSL_CTX_get_cert_store(ctx);
	if (NULL != pkix_validation_store) {
#if defined(X509_V_FLAG_TRUSTED_FIRST)
	    X509_STORE_set_flags(pkix_validation_store, 
				 X509_V_FLAG_TRUSTED_FIRST);
#endif
#if defined(X509_V_FLAG_PARTIAL_CHAIN)
	    X509_STORE_set_flags(pkix_validation_store, 
				 X509_V_FLAG_PARTIAL_CHAIN);
#endif
	}
    }

    /* Add certificate authorities for clients from CA.
     * Needed for certificate request during handshake.
     */
    if (cert && ssock->is_server) {
        STACK_OF(X509_NAME) *ca_dn = NULL;

        if (cert->CA_file.slen > 0) {
            ca_dn = SSL_load_client_CA_file(cert->CA_file.ptr);
        } else if (cert->CA_buf.slen > 0) {
            X509      *x  = NULL;
            X509_NAME *xn = NULL;
            STACK_OF(X509_NAME) *sk = NULL;
            BIO *new_bio = BIO_new_mem_buf((void*)cert->CA_buf.ptr,
					   cert->CA_buf.slen);

            sk = sk_X509_NAME_new(xname_cmp);

            if (sk != NULL && new_bio != NULL) {
                for (;;) {
                    if (PEM_read_bio_X509(new_bio, &x, NULL, NULL) == NULL)
                        break;

                    if ((xn = X509_get_subject_name(x)) == NULL)
                        break;

                    if ((xn = X509_NAME_dup(xn)) == NULL )
                        break;

                    if (sk_X509_NAME_find(sk, xn) >= 0) {
                        X509_NAME_free(xn);
                    } else {
                        sk_X509_NAME_push(sk, xn);
                    }
                    X509_free(x);
                    x = NULL;
                }
            }
            if (sk != NULL)
            	ca_dn = sk;
            if (new_bio != NULL)
                BIO_free(new_bio);
        }

	if (ca_dn != NULL) {
	    SSL_CTX_set_client_CA_list(ctx, ca_dn);
	    PJ_LOG(4,(ssock->pool->obj_name,
		      "CA certificates loaded from %s",
		      (cert->CA_file.slen?cert->CA_file.ptr:"buffer")));
	} else {
	    PJ_LOG(1,(ssock->pool->obj_name,
		      "Error reading CA certificates from %s",
		      (cert->CA_file.slen?cert->CA_file.ptr:"buffer")));
	}
    }

    /* Early sensitive data cleanup after OpenSSL context setup. However,
     * this cannot be done for listener sockets, as the data will still
     * be needed by accepted sockets.
     */
    if (cert && (!ssock->is_server || ssock->parent)) {
	pj_ssl_cert_wipe_keys(cert);	
    }

    /* Create SSL instance */
    ossock->ossl_ssl = SSL_new(ossock->ossl_ctx);
    if (ossock->ossl_ssl == NULL) {
	return GET_SSL_STATUS(ssock);
    }

    /* Set SSL sock as application data of SSL instance */
    SSL_set_ex_data(ossock->ossl_ssl, sslsock_idx, ssock);

    /* SSL verification options */
    mode = SSL_VERIFY_PEER;
    if (ssock->is_server && ssock->param.require_client_cert)
	mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;

    SSL_set_verify(ossock->ossl_ssl, mode, &verify_cb);

    /* Set curve list */
    status = set_curves_list(ssock);
    if (status != PJ_SUCCESS)
	return status;

    /* Set sigalg list */
    status = set_sigalgs(ssock);
    if (status != PJ_SUCCESS)
	return status;

    /* Setup SSL BIOs */
    ossock->ossl_rbio = BIO_new(BIO_s_mem());
    ossock->ossl_wbio = BIO_new(BIO_s_mem());
    (void)BIO_set_close(ossock->ossl_rbio, BIO_CLOSE);
    (void)BIO_set_close(ossock->ossl_wbio, BIO_CLOSE);
    SSL_set_bio(ossock->ossl_ssl, ossock->ossl_rbio, ossock->ossl_wbio);

    return PJ_SUCCESS;
}


/* Destroy SSL context and instance */
static void ssl_destroy(pj_ssl_sock_t *ssock)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;

    /* Destroy SSL instance */
    if (ossock->ossl_ssl) {
	SSL_free(ossock->ossl_ssl); /* this will also close BIOs */
	ossock->ossl_ssl = NULL;
    }

    /* Destroy SSL context */
    if (ossock->ossl_ctx) {
	SSL_CTX_free(ossock->ossl_ctx);
	ossock->ossl_ctx = NULL;
    }

    /* Potentially shutdown OpenSSL library if this is the last
     * context exists.
     */
    shutdown_openssl();
}


/* Reset SSL socket state */
static void ssl_reset_sock_state(pj_ssl_sock_t *ssock)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;

    /* Detach from SSL instance */
    if (ossock->ossl_ssl) {
	SSL_set_ex_data(ossock->ossl_ssl, sslsock_idx, NULL);
    }

    /**
     * Avoid calling SSL_shutdown() if handshake wasn't completed.
     * OpenSSL 1.0.2f complains if SSL_shutdown() is called during an
     * SSL handshake, while previous versions always return 0.
     */
    if (ossock->ossl_ssl && SSL_in_init(ossock->ossl_ssl) == 0) {
	int ret = SSL_shutdown(ossock->ossl_ssl);
	if (ret == 0) {
	    /* Flush data to send close notify. */
	    flush_circ_buf_output(ssock, &ssock->shutdown_op_key, 0, 0);
	}
    }

    pj_lock_acquire(ssock->write_mutex);
    ssock->ssl_state = SSL_STATE_NULL;
    pj_lock_release(ssock->write_mutex);

    ssl_close_sockets(ssock);

    /* Upon error, OpenSSL may leave any error description in the thread 
     * error queue, which sometime may cause next call to SSL API returning
     * false error alarm, e.g: in Linux, SSL_CTX_use_certificate_chain_file()
     * returning false error after a handshake error (in different SSL_CTX!).
     * For now, just clear thread error queue here.
     */
    ERR_clear_error();
}


static void ssl_ciphers_populate()
{
    if (ssl_cipher_num == 0 || ssl_curves_num == 0) {
	init_openssl();
	shutdown_openssl();
    }
}

static pj_ssl_cipher ssl_get_cipher(pj_ssl_sock_t *ssock)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    const SSL_CIPHER *cipher;

    /* Current cipher */
    cipher = SSL_get_current_cipher(ossock->ossl_ssl);
    if (cipher) {
	return (SSL_CIPHER_get_id(cipher) & 0x00FFFFFF);
    } else {
	return PJ_TLS_UNKNOWN_CIPHER;
    }
}

/* Generate cipher list with user preference order in OpenSSL format */
static pj_status_t set_cipher_list(pj_ssl_sock_t *ssock)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    pj_pool_t *tmp_pool = NULL;
    char *buf = NULL;
    enum { BUF_SIZE = 8192 };
    pj_str_t cipher_list;
    unsigned i, j;
    int ret;

    if (ssock->param.ciphers_num == 0) {
	ret = SSL_CTX_set_cipher_list(ossock->ossl_ctx, PJ_SSL_SOCK_OSSL_CIPHERS);
    	if (ret < 1) {
	    return GET_SSL_STATUS(ssock);
    	}    
	
	return PJ_SUCCESS;
    }

    /* Create temporary pool. */
    tmp_pool = pj_pool_create(ssock->pool->factory, "ciphpool", BUF_SIZE, 
			      BUF_SIZE/2 , NULL);
    if (!tmp_pool)
	return PJ_ENOMEM;

    buf = (char *)pj_pool_zalloc(tmp_pool, BUF_SIZE);

    pj_strset(&cipher_list, buf, 0);

    /* Generate user specified cipher list in OpenSSL format */
    for (i = 0; i < ssock->param.ciphers_num; ++i) {
	for (j = 0; j < ssl_cipher_num; ++j) {
	    if (ssock->param.ciphers[i] == ssl_ciphers[j].id)
	    {
		const char *c_name = ssl_ciphers[j].name;

		/* Check buffer size */
		if (cipher_list.slen + pj_ansi_strlen(c_name) + 2 >
		    BUF_SIZE)
		{
		    pj_assert(!"Insufficient temporary buffer for cipher");
		    return PJ_ETOOMANY;
		}

		/* Add colon separator */
		if (cipher_list.slen)
		    pj_strcat2(&cipher_list, ":");

		/* Add the cipher */
		pj_strcat2(&cipher_list, c_name);
		break;
	    }
	}	
    }

    /* Put NULL termination in the generated cipher list */
    cipher_list.ptr[cipher_list.slen] = '\0';

    /* Finally, set chosen cipher list */
    ret = SSL_CTX_set_cipher_list(ossock->ossl_ctx, buf);
    if (ret < 1) {
	pj_pool_release(tmp_pool);
	return GET_SSL_STATUS(ssock);
    }

    pj_pool_release(tmp_pool);
    return PJ_SUCCESS;
}

static pj_status_t set_curves_list(pj_ssl_sock_t *ssock)
{
#if !USING_LIBRESSL && !defined(OPENSSL_NO_EC) \
    && OPENSSL_VERSION_NUMBER >= 0x1000200fL
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    int ret;
    int curves[PJ_SSL_SOCK_MAX_CURVES];
    unsigned cnt;

    if (ssock->param.curves_num == 0)
	return PJ_SUCCESS;

    for (cnt = 0; cnt < ssock->param.curves_num; cnt++) {
	curves[cnt] = get_nid_from_cid(ssock->param.curves[cnt]);
    }

    if( SSL_is_server(ossock->ossl_ssl) ) {
	ret = SSL_set1_curves(ossock->ossl_ssl, curves,
			      ssock->param.curves_num);
	if (ret < 1)
	    return GET_SSL_STATUS(ssock);
    } else {
	ret = SSL_CTX_set1_curves(ossock->ossl_ctx, curves,
				  ssock->param.curves_num);
	if (ret < 1)
	    return GET_SSL_STATUS(ssock);
    }
#else
    PJ_UNUSED_ARG(ssock);
#endif
    return PJ_SUCCESS;
}

static pj_status_t set_sigalgs(pj_ssl_sock_t *ssock)
{
#if !USING_LIBRESSL && OPENSSL_VERSION_NUMBER >= 0x1000200fL
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    int ret;

    if (ssock->param.sigalgs.ptr && ssock->param.sigalgs.slen) {
	if (ssock->is_server) {
	    ret = SSL_set1_client_sigalgs_list(ossock->ossl_ssl,
	    				       ssock->param.sigalgs.ptr);
	} else {
	    ret = SSL_set1_sigalgs_list(ossock->ossl_ssl,
	    				ssock->param.sigalgs.ptr);
	}

	if (ret < 1)
	    return GET_SSL_STATUS(ssock);
    }
#else
    PJ_UNUSED_ARG(ssock);
#endif
    return PJ_SUCCESS;
}

static void set_entropy(pj_ssl_sock_t *ssock)
{
    int ret = 0;

    switch (ssock->param.entropy_type) {
#ifndef OPENSSL_NO_EGD
	case PJ_SSL_ENTROPY_EGD:
	    ret = RAND_egd(ssock->param.entropy_path.ptr);
	    break;
#endif
	case PJ_SSL_ENTROPY_RANDOM:
	    ret = RAND_load_file("/dev/random",255);
	    break;
	case PJ_SSL_ENTROPY_URANDOM:
	    ret = RAND_load_file("/dev/urandom",255);
	    break;
	case PJ_SSL_ENTROPY_FILE:
	    ret = RAND_load_file(ssock->param.entropy_path.ptr,255);
	    break;
	case PJ_SSL_ENTROPY_NONE:
	default:
	    break;
    }

    if (ret < 0) {
	PJ_LOG(3, (ssock->pool->obj_name,
		   "SSL failed to reseed with entropy type %d "
		   "[native err=%d]",
		   ssock->param.entropy_type, ret));
    }
}

/* Parse OpenSSL ASN1_TIME to pj_time_val and GMT info */
static pj_bool_t parse_ossl_asn1_time(pj_time_val *tv, pj_bool_t *gmt,
				      const ASN1_TIME *tm)
{
    unsigned long parts[7] = {0};
    char *p, *end;
    unsigned len;
    pj_bool_t utc;
    pj_parsed_time pt;
    int i;

    utc = tm->type == V_ASN1_UTCTIME;
    p = (char*)tm->data;
    len = tm->length;
    end = p + len - 1;

    /* GMT */
    *gmt = (*end == 'Z');

    /* parse parts */
    for (i = 0; i < 7 && p < end; ++i) {
	pj_str_t st;

	if (i==0 && !utc) {
	    /* 4 digits year part for non-UTC time format */
	    st.slen = 4;
	} else if (i==6) {
	    /* fraction of seconds */
	    if (*p == '.') ++p;
	    st.slen = end - p + 1;
	} else {
	    /* other parts always 2 digits length */
	    st.slen = 2;
	}
	st.ptr = p;

	parts[i] = pj_strtoul(&st);
	p += st.slen;
    }

    /* encode parts to pj_time_val */
    pt.year = parts[0];
    if (utc)
	pt.year += (pt.year < 50)? 2000:1900;
    pt.mon = parts[1] - 1;
    pt.day = parts[2];
    pt.hour = parts[3];
    pt.min = parts[4];
    pt.sec = parts[5];
    pt.msec = parts[6];

    pj_time_encode(&pt, tv);

    return PJ_TRUE;
}


/* Get Common Name field string from a general name string */
static void get_cn_from_gen_name(const pj_str_t *gen_name, pj_str_t *cn)
{
    pj_str_t CN_sign = {"/CN=", 4};
    char *p, *q;

    pj_bzero(cn, sizeof(pj_str_t));

    if (!gen_name->slen)
	return;

    p = pj_strstr(gen_name, &CN_sign);
    if (!p)
	return;

    p += 4; /* shift pointer to value part */
    pj_strset(cn, p, gen_name->slen - (p - gen_name->ptr));
    q = pj_strchr(cn, '/');
    if (q)
	cn->slen = q - p;
}


/* Get certificate info from OpenSSL X509, in case the certificate info
 * hal already populated, this function will check if the contents need 
 * to be updated by inspecting the issuer and the serial number.
 */
static void get_cert_info(pj_pool_t *pool, pj_ssl_cert_info *ci, X509 *x,
			  pj_bool_t get_pem)
{
    pj_bool_t update_needed;
    char buf[512];
    pj_uint8_t serial_no[64] = {0}; /* should be >= sizeof(ci->serial_no) */
    const pj_uint8_t *q;
    unsigned len;
    GENERAL_NAMES *names = NULL;

    pj_assert(pool && ci && x);

    /* Get issuer */
    X509_NAME_oneline(X509_get_issuer_name(x), buf, sizeof(buf));

    /* Get serial no */
    q = (const pj_uint8_t*) M_ASN1_STRING_data(X509_get_serialNumber(x));
    len = M_ASN1_STRING_length(X509_get_serialNumber(x));
    if (len > sizeof(ci->serial_no)) 
	len = sizeof(ci->serial_no);
    pj_memcpy(serial_no + sizeof(ci->serial_no) - len, q, len);

    /* Check if the contents need to be updated. */
    update_needed = pj_strcmp2(&ci->issuer.info, buf) || 
	            pj_memcmp(ci->serial_no, serial_no, sizeof(ci->serial_no));
    if (!update_needed)
	return;

    /* Update cert info */

    pj_bzero(ci, sizeof(pj_ssl_cert_info));

    /* Version */
    ci->version = X509_get_version(x) + 1;

    /* Issuer */
    pj_strdup2(pool, &ci->issuer.info, buf);
    get_cn_from_gen_name(&ci->issuer.info, &ci->issuer.cn);

    /* Serial number */
    pj_memcpy(ci->serial_no, serial_no, sizeof(ci->serial_no));

    /* Subject */
    pj_strdup2(pool, &ci->subject.info, 
	       X509_NAME_oneline(X509_get_subject_name(x),
				 buf, sizeof(buf)));
    get_cn_from_gen_name(&ci->subject.info, &ci->subject.cn);

    /* Validity */
    parse_ossl_asn1_time(&ci->validity.start, &ci->validity.gmt,
			 X509_get_notBefore(x));
    parse_ossl_asn1_time(&ci->validity.end, &ci->validity.gmt,
			 X509_get_notAfter(x));

    /* Subject Alternative Name extension */
    if (ci->version >= 3) {
	names = (GENERAL_NAMES*) X509_get_ext_d2i(x, NID_subject_alt_name,
						  NULL, NULL);
    }
    if (names) {
        unsigned i, cnt;

        cnt = sk_GENERAL_NAME_num(names);
	ci->subj_alt_name.entry = pj_pool_calloc(pool, cnt, 
					    sizeof(*ci->subj_alt_name.entry));

        for (i = 0; i < cnt; ++i) {
	    unsigned char *p = 0;
	    pj_ssl_cert_name_type type = PJ_SSL_CERT_NAME_UNKNOWN;
            const GENERAL_NAME *name;
	    
	    name = sk_GENERAL_NAME_value(names, i);

            switch (name->type) {
                case GEN_EMAIL:
                    len = ASN1_STRING_to_UTF8(&p, name->d.ia5);
		    type = PJ_SSL_CERT_NAME_RFC822;
                    break;
                case GEN_DNS:
                    len = ASN1_STRING_to_UTF8(&p, name->d.ia5);
		    type = PJ_SSL_CERT_NAME_DNS;
                    break;
                case GEN_URI:
                    len = ASN1_STRING_to_UTF8(&p, name->d.ia5);
		    type = PJ_SSL_CERT_NAME_URI;
                    break;
                case GEN_IPADD:
		    p = (unsigned char*)M_ASN1_STRING_data(name->d.ip);
		    len = M_ASN1_STRING_length(name->d.ip);
		    type = PJ_SSL_CERT_NAME_IP;
                    break;
		default:
		    break;
            }

	    if (p && len && type != PJ_SSL_CERT_NAME_UNKNOWN) {
		ci->subj_alt_name.entry[ci->subj_alt_name.cnt].type = type;
		if (type == PJ_SSL_CERT_NAME_IP) {
		    int af = pj_AF_INET();
		    if (len == sizeof(pj_in6_addr)) af = pj_AF_INET6();
		    pj_inet_ntop2(af, p, buf, sizeof(buf));
		    pj_strdup2(pool, 
		          &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name,
		          buf);
		} else {
		    pj_strdup2(pool, 
			  &ci->subj_alt_name.entry[ci->subj_alt_name.cnt].name, 
			  (char*)p);
		    OPENSSL_free(p);
		}
		ci->subj_alt_name.cnt++;
	    }
        }
        GENERAL_NAMES_free(names);
        names = NULL;
    }

    if (get_pem) {
	/* Update raw Certificate info in PEM format. */
	BIO *bio;	
	BUF_MEM *ptr;
	
	bio = BIO_new(BIO_s_mem());
	if (!PEM_write_bio_X509(bio, x)) {
	    PJ_LOG(3,(THIS_FILE, "Error retrieving raw certificate info"));
	    ci->raw.ptr = NULL;
	    ci->raw.slen = 0;
	} else {
	    BIO_write(bio, "\0", 1);
	    BIO_get_mem_ptr(bio, &ptr);
	    pj_strdup2(pool, &ci->raw, ptr->data);	
	}	
	BIO_free(bio);	    
    }	 
}

/* Update remote certificates chain info. This function should be
 * called after handshake or renegotiation successfully completed.
 */
static void ssl_update_remote_cert_chain_info(pj_pool_t *pool,
					      pj_ssl_cert_info *ci,
					      STACK_OF(X509) *chain,
					      pj_bool_t get_pem)
{
    int i;

    /* For now, get_pem has to be PJ_TRUE */
    pj_assert(get_pem);
    PJ_UNUSED_ARG(get_pem);

    ci->raw_chain.cert_raw = (pj_str_t *)pj_pool_calloc(pool,
       				    			sk_X509_num(chain),
       				    			sizeof(pj_str_t));
    ci->raw_chain.cnt = sk_X509_num(chain);

    for (i = 0; i < sk_X509_num(chain); i++) {
        BIO *bio;
        BUF_MEM *ptr;
	X509 *x = sk_X509_value(chain, i);

        bio = BIO_new(BIO_s_mem());
        
        if (!PEM_write_bio_X509(bio, x)) {
            PJ_LOG(3, (THIS_FILE, "Error retrieving raw certificate info"));
            ci->raw_chain.cert_raw[i].ptr  = NULL;
            ci->raw_chain.cert_raw[i].slen = 0;
        } else {
            BIO_write(bio, "\0", 1);
            BIO_get_mem_ptr(bio, &ptr);
            pj_strdup2(pool, &ci->raw_chain.cert_raw[i], ptr->data );
        }
        
        BIO_free(bio);
    }
}

/* Update local & remote certificates info. This function should be
 * called after handshake or renegotiation successfully completed.
 */
static void ssl_update_certs_info(pj_ssl_sock_t *ssock)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    X509 *x;
    STACK_OF(X509) *chain;

    pj_assert(ssock->ssl_state == SSL_STATE_ESTABLISHED);

    /* Active local certificate */
    x = SSL_get_certificate(ossock->ossl_ssl);
    if (x) {
	get_cert_info(ssock->pool, &ssock->local_cert_info, x, PJ_FALSE);
	/* Don't free local's X509! */
    } else {
	pj_bzero(&ssock->local_cert_info, sizeof(pj_ssl_cert_info));
    }

    /* Active remote certificate */
    x = SSL_get_peer_certificate(ossock->ossl_ssl);
    if (x) {
	get_cert_info(ssock->pool, &ssock->remote_cert_info, x, PJ_TRUE);
	/* Free peer's X509 */
	X509_free(x);
    } else {
	pj_bzero(&ssock->remote_cert_info, sizeof(pj_ssl_cert_info));
    }

    chain = SSL_get_peer_cert_chain(ossock->ossl_ssl);
    if (chain) {
	pj_pool_reset(ssock->info_pool);
	ssl_update_remote_cert_chain_info(ssock->info_pool,
       					  &ssock->remote_cert_info,
       					  chain, PJ_TRUE);
    } else {
	ssock->remote_cert_info.raw_chain.cnt = 0;
    }
}


/* Flush write BIO to network socket. Note that any access to write BIO
 * MUST be serialized, so mutex protection must cover any call to OpenSSL
 * API (that possibly generate data for write BIO) along with the call to
 * this function (flushing all data in write BIO generated by above 
 * OpenSSL API call).
 */
static pj_status_t flush_circ_buf_output(pj_ssl_sock_t *ssock,
                                         pj_ioqueue_op_key_t *send_key,
                                         pj_size_t orig_len, unsigned flags);


static void ssl_set_state(pj_ssl_sock_t *ssock, pj_bool_t is_server)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    if (is_server) {
        SSL_set_accept_state(ossock->ossl_ssl);
    } else {
	SSL_set_connect_state(ossock->ossl_ssl);
    }
}


static void ssl_set_peer_name(pj_ssl_sock_t *ssock)
{
#ifdef SSL_set_tlsext_host_name
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;

    /* Set server name to connect */
    if (ssock->param.server_name.slen &&
        get_ip_addr_ver(&ssock->param.server_name) == 0)
    {
	/* Server name is null terminated already */
	if (!SSL_set_tlsext_host_name(ossock->ossl_ssl, 
				      ssock->param.server_name.ptr))
	{
	    char err_str[PJ_ERR_MSG_SIZE];

	    ERR_error_string_n(ERR_get_error(), err_str, sizeof(err_str));
	    PJ_LOG(3,(ssock->pool->obj_name, "SSL_set_tlsext_host_name() "
		"failed: %s", err_str));
	}
    }
#endif
}


/* Asynchronouse handshake */
static pj_status_t ssl_do_handshake(pj_ssl_sock_t *ssock)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    pj_status_t status;
    int err;

    /* Perform SSL handshake */
    pj_lock_acquire(ssock->write_mutex);
    err = SSL_do_handshake(ossock->ossl_ssl);
    pj_lock_release(ssock->write_mutex);

    /* SSL_do_handshake() may put some pending data into SSL write BIO, 
     * flush it if any.
     */
    status = flush_circ_buf_output(ssock, &ssock->handshake_op_key, 0, 0);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	return status;
    }

    if (err < 0) {
	int err2 = SSL_get_error(ossock->ossl_ssl, err);
	if (err2 != SSL_ERROR_NONE && err2 != SSL_ERROR_WANT_READ)
	{
	    /* Handshake fails */
	    status = STATUS_FROM_SSL_ERR2("Handshake", ssock, err, err2, 0);
	    return status;
	}
    }

    /* Check if handshake has been completed */
    if (SSL_is_init_finished(ossock->ossl_ssl)) {
	ssock->ssl_state = SSL_STATE_ESTABLISHED;
	return PJ_SUCCESS;
    }

    return PJ_EPENDING;
}


static pj_status_t ssl_read(pj_ssl_sock_t *ssock, void *data, int *size)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    int size_ = *size;
    int len = size_;

    /* SSL_read() may write some data to write buffer when re-negotiation
     * is on progress, so let's protect it with write mutex.
     */
    pj_lock_acquire(ssock->write_mutex);
    *size = size_ = SSL_read(ossock->ossl_ssl, data, size_);
    pj_lock_release(ssock->write_mutex);

    if (size_ <= 0) {
	pj_status_t status;
	int err = SSL_get_error(ossock->ossl_ssl, size_);

	/* SSL might just return SSL_ERROR_WANT_READ in 
	 * re-negotiation.
	 */
	if (err != SSL_ERROR_NONE && err != SSL_ERROR_WANT_READ) {
	    if (err == SSL_ERROR_SYSCALL && size_ == -1 &&
		ERR_peek_error() == 0 && errno == 0)
	    {
		status = STATUS_FROM_SSL_ERR2("Read", ssock, size_,
					      err, len);
		PJ_LOG(4,("SSL", "SSL_read() = -1, with "
				 "SSL_ERROR_SYSCALL, no SSL error, "
				 "and errno = 0 - skip BIO error"));
		/* Ignore these errors */
	    } else {
		/* Reset SSL socket state, then return PJ_FALSE */
		status = STATUS_FROM_SSL_ERR2("Read", ssock, size_,
		        		      err, len);
		ssl_reset_sock_state(ssock);
		return status;
	    }
	}
	
	/* Need renegotiation */
	return PJ_EEOF;
    }

    return PJ_SUCCESS;
}


/* Write plain data to SSL and flush write BIO. */
static pj_status_t ssl_write(pj_ssl_sock_t *ssock, const void *data,
			     pj_ssize_t size, int *nwritten)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    pj_status_t status = PJ_SUCCESS;

    *nwritten = SSL_write(ossock->ossl_ssl, data, (int)size);
    if (*nwritten <= 0) {
	/* SSL failed to process the data, it may just that re-negotiation
	 * is on progress.
	 */
	int err;
	err = SSL_get_error(ossock->ossl_ssl, *nwritten);
	if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_NONE) {
	    status = PJ_EEOF;
	} else {
	    /* Some problem occured */
	    status = STATUS_FROM_SSL_ERR2("Write", ssock, *nwritten,
	    				  err, size);
	}
    } else if (*nwritten < size) {
	/* nwritten < size, shouldn't happen, unless write BIO cannot hold 
	 * the whole secured data, perhaps because of insufficient memory.
	 */
	status = PJ_ENOMEM;
    }

    return status;
}


static pj_status_t ssl_renegotiate(pj_ssl_sock_t *ssock)
{
    ossl_sock_t *ossock = (ossl_sock_t *)ssock;
    pj_status_t status = PJ_SUCCESS;
    int ret;

    if (SSL_renegotiate_pending(ossock->ossl_ssl))
	return PJ_EPENDING;

    ret = SSL_renegotiate(ossock->ossl_ssl);
    if (ret <= 0) {
	status = GET_SSL_STATUS(ssock);
    }
    
    return status;
}


/* Put back deprecation warning setting */
#if defined(PJ_DARWINOS) && PJ_DARWINOS==1
#  pragma GCC diagnostic pop
#endif


#endif  /* PJ_HAS_SSL_SOCK */


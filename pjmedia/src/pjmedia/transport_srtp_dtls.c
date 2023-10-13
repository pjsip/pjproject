/*
 * Copyright (C) 2017 Teluu Inc. (http://www.teluu.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <pjmedia/clock.h>
#include <pjmedia/sdp.h>
#include <pjmedia/transport_ice.h>
#include <pj/errno.h>
#include <pj/rand.h>
#include <pj/ssl_sock.h>

/* 
 * Include OpenSSL headers
 */
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#if OPENSSL_VERSION_NUMBER >= 0x10100000L && \
    defined(OPENSSL_API_COMPAT) && OPENSSL_API_COMPAT >= 0x10100000L
#  define X509_get_notBefore(x)     X509_getm_notBefore(x)
#  define X509_get_notAfter(x)      X509_getm_notAfter(x)
#endif

/* Set to 1 to enable DTLS-SRTP debugging */
#define DTLS_DEBUG  0

#define NUM_CHANNEL 2

enum {
    RTP_CHANNEL = 0,
    RTCP_CHANNEL = 1
};

#define CHANNEL_TO_STRING(idx) (idx == RTP_CHANNEL? "RTP channel": \
                                "RTCP channel")

/* DTLS-SRTP transport op */
static pj_status_t dtls_media_create  (pjmedia_transport *tp,
                                       pj_pool_t *sdp_pool,
                                       unsigned options,
                                       const pjmedia_sdp_session *sdp_remote,
                                       unsigned media_index);
static pj_status_t dtls_encode_sdp    (pjmedia_transport *tp,
                                       pj_pool_t *sdp_pool,
                                       pjmedia_sdp_session *sdp_local,
                                       const pjmedia_sdp_session *sdp_remote,
                                       unsigned media_index);
static pj_status_t dtls_media_start   (pjmedia_transport *tp,
                                       pj_pool_t *tmp_pool,
                                       const pjmedia_sdp_session *sdp_local,
                                       const pjmedia_sdp_session *sdp_remote,
                                       unsigned media_index);
static pj_status_t dtls_media_stop    (pjmedia_transport *tp);
static pj_status_t dtls_destroy       (pjmedia_transport *tp);
static pj_status_t dtls_on_recv_rtp   (pjmedia_transport *tp,
                                       const void *pkt,
                                       pj_size_t size);
static pj_status_t dtls_on_recv_rtcp  (pjmedia_transport *tp,
                                       const void *pkt,
                                       pj_size_t size);

static void on_ice_complete2(pjmedia_transport *tp,
                             pj_ice_strans_op op,
                             pj_status_t status,
                             void *user_data);

static void dtls_on_destroy(void *arg);


static pjmedia_transport_op dtls_op =
{
    NULL,
    NULL,
    NULL,
    &dtls_on_recv_rtp,      // originally send_rtp()
    &dtls_on_recv_rtcp,     // originally send_rtcp()
    NULL,
    &dtls_media_create,
    &dtls_encode_sdp,
    &dtls_media_start,
    &dtls_media_stop,
    NULL,
    &dtls_destroy,
    NULL,
};


typedef enum dtls_setup
{ 
    DTLS_SETUP_UNKNOWN,
    DTLS_SETUP_ACTPASS,
    DTLS_SETUP_ACTIVE,
    DTLS_SETUP_PASSIVE
} dtls_setup;

typedef struct dtls_srtp dtls_srtp;

typedef struct dtls_srtp_channel
{
    dtls_srtp           *dtls_srtp;
    unsigned             channel;
} dtls_srtp_channel;

typedef struct dtls_srtp
{
    pjmedia_transport    base;
    pj_pool_t           *pool;
    transport_srtp      *srtp;

    dtls_setup           setup;
    unsigned long        last_err;
    pj_bool_t            use_ice;
    dtls_srtp_channel    channel[NUM_CHANNEL];
    pj_bool_t            nego_started[NUM_CHANNEL];
    pj_bool_t            nego_completed[NUM_CHANNEL];
    pj_str_t             rem_fingerprint;   /* Remote fingerprint in SDP    */
    pj_status_t          rem_fprint_status; /* Fingerprint verif. status    */
    pj_sockaddr          rem_addr;          /* Remote address (from SDP/RTP)*/
    pj_sockaddr          rem_rtcp;          /* Remote RTCP address (SDP)    */
    pj_bool_t            pending_start;     /* media_start() invoked but DTLS
                                               nego not done yet, so start
                                               the SRTP once the nego done  */
    pj_bool_t            is_destroying;     /* DTLS being destroyed?        */
    pj_bool_t            got_keys;          /* DTLS nego done & keys ready  */
    pjmedia_srtp_crypto  tx_crypto[NUM_CHANNEL];
    pjmedia_srtp_crypto  rx_crypto[NUM_CHANNEL];

    char                 buf[NUM_CHANNEL][PJMEDIA_MAX_MTU];
    pjmedia_clock       *clock[NUM_CHANNEL];/* Timer workaround for retrans */

    SSL_CTX             *ossl_ctx[NUM_CHANNEL];
    SSL                 *ossl_ssl[NUM_CHANNEL];
    BIO                 *ossl_rbio[NUM_CHANNEL];
    BIO                 *ossl_wbio[NUM_CHANNEL];
    pj_lock_t           *ossl_lock;
} dtls_srtp;


static const pj_str_t ID_TP_DTLS_SRTP = { "UDP/TLS/RTP/SAVP", 16 };
static const pj_str_t ID_SETUP        = { "setup", 5 };
static const pj_str_t ID_ACTPASS      = { "actpass", 7 };
static const pj_str_t ID_ACTIVE       = { "active", 6 };
static const pj_str_t ID_PASSIVE      = { "passive", 7 };
static const pj_str_t ID_FINGERPRINT  = { "fingerprint", 11 };

/* Map of OpenSSL-pjmedia SRTP cryptos. Currently OpenSSL seems to
 * support few cryptos only (based on ssl/d1_srtp.c of OpenSSL 1.1.0c).
 */
#define OPENSSL_PROFILE_NUM 4

static char* ossl_profiles[OPENSSL_PROFILE_NUM] =
{
     "SRTP_AES128_CM_SHA1_80",
     "SRTP_AES128_CM_SHA1_32",
     "SRTP_AEAD_AES_256_GCM",
     "SRTP_AEAD_AES_128_GCM"
};
static char* pj_profiles[OPENSSL_PROFILE_NUM] =
{
    "AES_CM_128_HMAC_SHA1_80",
    "AES_CM_128_HMAC_SHA1_32",
    "AEAD_AES_256_GCM",
    "AEAD_AES_128_GCM"
};

/* This will store the valid OpenSSL profiles which is mapped from 
 * OpenSSL-pjmedia SRTP cryptos.
 */
static char *valid_pj_profiles_list[OPENSSL_PROFILE_NUM];
static char *valid_ossl_profiles_list[OPENSSL_PROFILE_NUM];
static unsigned valid_profiles_cnt;


/* Certificate & private key */
static X509     *dtls_cert;
static EVP_PKEY *dtls_priv_key;
static pj_status_t ssl_generate_cert(X509 **p_cert, EVP_PKEY **p_priv_key);

static pj_status_t dtls_init()
{
    /* Make sure OpenSSL library has been initialized */
    {
        pj_ssl_cipher ciphers[1];
        unsigned cipher_num = 1;
        pj_ssl_cipher_get_availables(ciphers, &cipher_num);
    }

    /* Generate cert if not yet */
    if (!dtls_cert) {
        pj_status_t status;
        status = ssl_generate_cert(&dtls_cert, &dtls_priv_key);
        if (status != PJ_SUCCESS) {
            pj_perror(4, "DTLS-SRTP", status,
                      "Failed generating DTLS certificate");
            return status;
        }
    }

    if (valid_profiles_cnt == 0) {
        unsigned n, j;
        int rc;
        char *p, *end, buf[OPENSSL_PROFILE_NUM*25];

        /* Create DTLS context */
        SSL_CTX *ctx = SSL_CTX_new(DTLS_method());
        if (ctx == NULL) {
            return PJ_ENOMEM;
        }

        p = buf;
        end = buf + sizeof(buf);
        for (j=0; j<PJ_ARRAY_SIZE(ossl_profiles); ++j) {
            rc = SSL_CTX_set_tlsext_use_srtp(ctx, ossl_profiles[j]);
            if (rc == 0) {
                valid_pj_profiles_list[valid_profiles_cnt] =
                    pj_profiles[j];
                valid_ossl_profiles_list[valid_profiles_cnt++] =
                    ossl_profiles[j];

                n = pj_ansi_snprintf(p, end - p, ":%s", pj_profiles[j]);
                p += n;
            }
        }
        SSL_CTX_free(ctx);

        if (valid_profiles_cnt > 0) {
            PJ_LOG(4,("DTLS-SRTP", "%s profile is supported", buf));
        } else {
            PJ_PERROR(4, ("DTLS-SRTP", PJMEDIA_SRTP_DTLS_ENOPROFILE,
                          "Error getting SRTP profile"));

            return PJMEDIA_SRTP_DTLS_ENOPROFILE;
        }
    }

    return PJ_SUCCESS;
}

static void dtls_deinit()
{
    if (dtls_cert) {
        X509_free(dtls_cert);
        dtls_cert = NULL;

        EVP_PKEY_free(dtls_priv_key);
        dtls_priv_key = NULL;
    }

    valid_profiles_cnt = 0;
}


/* Create DTLS-SRTP keying instance */
static pj_status_t dtls_create(transport_srtp *srtp,
                               pjmedia_transport **p_keying)
{
    dtls_srtp *ds;
    pj_pool_t *pool;
    pj_status_t status;

    pool = pj_pool_create(srtp->pool->factory, "dtls%p",
                          2000, 256, NULL);
    ds = PJ_POOL_ZALLOC_T(pool, dtls_srtp);
    ds->pool = pool;

    pj_ansi_strxcpy(ds->base.name, pool->obj_name, PJ_MAX_OBJ_NAME);
    ds->base.type = (pjmedia_transport_type)PJMEDIA_SRTP_KEYING_DTLS_SRTP;
    ds->base.op = &dtls_op;
    ds->base.user_data = srtp;
    ds->srtp = srtp;

    /* Setup group lock handler for destroy and callback synchronization */
    if (srtp->base.grp_lock) {
        pj_grp_lock_t *grp_lock = srtp->base.grp_lock;

        ds->base.grp_lock = grp_lock;
        pj_grp_lock_add_ref(grp_lock);
        pj_grp_lock_add_handler(grp_lock, pool, ds, &dtls_on_destroy);
    } else {
        status = pj_lock_create_recursive_mutex(ds->pool, "dtls_ssl_lock%p",
                                             &ds->ossl_lock);
        if (status != PJ_SUCCESS)
            return status;
    }

    *p_keying = &ds->base;
    PJ_LOG(5,(srtp->pool->obj_name, "SRTP keying DTLS-SRTP created"));
    return PJ_SUCCESS;
}


/* Lock/unlock for DTLS states access protection */

static void DTLS_LOCK(dtls_srtp *ds) {
    if (ds->base.grp_lock)
        pj_grp_lock_acquire(ds->base.grp_lock);
    else
        pj_lock_acquire(ds->ossl_lock);
}

static pj_status_t DTLS_TRY_LOCK(dtls_srtp *ds) {
    if (ds->base.grp_lock)
        return pj_grp_lock_tryacquire(ds->base.grp_lock);
    else
        return pj_lock_tryacquire(ds->ossl_lock);
}

static void DTLS_UNLOCK(dtls_srtp *ds) {
    if (ds->base.grp_lock)
        pj_grp_lock_release(ds->base.grp_lock);
    else
        pj_lock_release(ds->ossl_lock);
}


/**
 * Mapping from OpenSSL error codes to pjlib error space.
 */
#define PJ_SSL_ERRNO_START              (PJ_ERRNO_START_USER + \
                                         PJ_ERRNO_SPACE_SIZE*6)

#define PJ_SSL_ERRNO_SPACE_SIZE         PJ_ERRNO_SPACE_SIZE

/* Expected maximum value of reason component in OpenSSL error code */
#define MAX_OSSL_ERR_REASON             1200

static pj_status_t STATUS_FROM_SSL_ERR(dtls_srtp *ds,
                                       unsigned long err)
{
    pj_status_t status;

    /* General SSL error, dig more from OpenSSL error queue */
    if (err == SSL_ERROR_SSL)
        err = ERR_get_error();

    /* OpenSSL error range is much wider than PJLIB errno space, so
     * if it exceeds the space, only the error reason will be kept.
     * Note that the last native error will be kept as is and can be
     * retrieved via SSL socket info.
     */
    status = ERR_GET_LIB(err)*MAX_OSSL_ERR_REASON + ERR_GET_REASON(err);
    if (status > PJ_SSL_ERRNO_SPACE_SIZE)
        status = ERR_GET_REASON(err);

    if (status != PJ_SUCCESS)
        status += PJ_SSL_ERRNO_START;

    ds->last_err = err;
    return status;
}


static pj_status_t GET_SSL_STATUS(dtls_srtp *ds)
{
    return STATUS_FROM_SSL_ERR(ds, ERR_get_error());
}


/* SSL cert verification callback. */
static int verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
    PJ_UNUSED_ARG(preverify_ok);
    PJ_UNUSED_ARG(x509_ctx);
    /* Just skip it for now (as usually it's a self-signed cert) */
    return 1;
}

/* Get fingerprint from TLS cert, output is formatted for SDP a=fingerprint,
 * e.g: "SHA-256 XX:XX:XX...". If is_sha256 is true, SHA-256 hash algo will
 * be used, otherwise it is SHA-1.
 */
static pj_status_t ssl_get_fingerprint(X509 *cert, pj_bool_t is_sha256,
                                       char *buf, pj_size_t *buf_len)
{
    unsigned int len, st_out_len, i;
    unsigned char tmp[EVP_MAX_MD_SIZE];
    char *p, *end=buf+*buf_len;

    if (!X509_digest(cert, (is_sha256?EVP_sha256():EVP_sha1()), tmp, &len))
        return PJ_EUNKNOWN;

    st_out_len =  len*3 + (is_sha256? 7 : 5);
    if (*buf_len < st_out_len + 1)
        return PJ_ETOOSMALL;

    /* Format fingerprint to "SHA-256 XX:XX:XX..." */
    p = buf;
    p += pj_ansi_snprintf(p, end-p, "SHA-%s %.2X", 
                          (is_sha256?"256":"1"), tmp[0]);
    for (i=1; i<len; ++i)
        p += pj_ansi_snprintf(p, end-p, ":%.2X", tmp[i]);

    *buf_len = st_out_len;

    return PJ_SUCCESS;
}

/* Generate self-signed cert */
static pj_status_t ssl_generate_cert(X509 **p_cert, EVP_PKEY **p_priv_key)
{
    BIGNUM *bne = NULL;
    RSA *rsa_key = NULL;
    X509_NAME *cert_name = NULL;
    X509 *cert = NULL;
    EVP_PKEY *priv_key = NULL;

    /* Create big number */
    bne = BN_new();
    if (!bne) goto on_error;
    if (!BN_set_word(bne, RSA_F4)) goto on_error;

    /* Generate RSA key */
    rsa_key = RSA_new();
    if (!rsa_key) goto on_error;
    if (!RSA_generate_key_ex(rsa_key, 2048, bne, NULL)) goto on_error;

    /* Create private key */
    priv_key = EVP_PKEY_new();
    if (!priv_key) goto on_error;
    if (!EVP_PKEY_assign_RSA(priv_key, rsa_key)) goto on_error;
    rsa_key = NULL;

    /* Create certificate */
    cert = X509_new();
    if (!cert) goto on_error;

    /* Set version to 3 (2 = x509v3) */
    X509_set_version(cert, 2);

    /* Set serial number */
    ASN1_INTEGER_set(X509_get_serialNumber(cert), pj_rand());

    /* Set valid period */
    X509_gmtime_adj(X509_get_notBefore(cert), -60*60*24);
    X509_gmtime_adj(X509_get_notAfter(cert), 60*60*24*365);

    /* Set subject name */
    cert_name = X509_get_subject_name(cert);
    if (!cert_name) goto on_error;
    if (!X509_NAME_add_entry_by_txt(cert_name, "CN", MBSTRING_ASC,
                                    (const unsigned char*)"pjmedia.pjsip.org",
                                    -1, -1, 0)) goto on_error;

    /* Set the issuer name (to subject name as this is self-signed cert) */
    if (!X509_set_issuer_name(cert, cert_name)) goto on_error;

    /* Set the public key */
    if (!X509_set_pubkey(cert, priv_key)) goto on_error;

    /* Sign with the private key */
    if (!X509_sign(cert, priv_key, EVP_sha1())) goto on_error;

    /* Free big number */
    BN_free(bne);

    *p_cert = cert;
    *p_priv_key = priv_key;
    return PJ_SUCCESS;

on_error:
    if (bne) BN_free(bne);
    if (rsa_key && !priv_key) RSA_free(rsa_key);
    if (priv_key) EVP_PKEY_free(priv_key);
    if (cert) X509_free(cert);
    return PJ_EUNKNOWN;
}

/* Create and initialize new SSL context and instance */
static pj_status_t ssl_create(dtls_srtp *ds, unsigned idx)
{
    SSL_CTX *ctx;
    unsigned i;
    int mode, rc;

    /* Check if it is already instantiated */
    if (ds->ossl_ssl[idx])
        return PJ_SUCCESS;

    /* Create DTLS context */
    ctx = SSL_CTX_new(DTLS_method());
    if (ctx == NULL) {
        return GET_SSL_STATUS(ds);
    }

    if (valid_profiles_cnt == 0) {
        SSL_CTX_free(ctx);
        return PJMEDIA_SRTP_DTLS_ENOPROFILE;
    }

    /* Set crypto */
    if (1) {
        char *p, *end, buf[PJ_ARRAY_SIZE(ossl_profiles)*25];
        unsigned n;

        p = buf;
        end = buf + sizeof(buf);
        for (i=0; i<ds->srtp->setting.crypto_count && p < end; ++i) {
            pjmedia_srtp_crypto *crypto = &ds->srtp->setting.crypto[i];
            unsigned j;
            for (j=0; j < valid_profiles_cnt; ++j) {
                if (!pj_ansi_strcmp(crypto->name.ptr,
                                    valid_pj_profiles_list[j]))
                {
                    n = pj_ansi_snprintf(p, end-p, ":%s",
                                         valid_ossl_profiles_list[j]);
                    p += n;
                    break;
                }
            }

        }
        rc = SSL_CTX_set_tlsext_use_srtp(ctx, buf+1);
        PJ_LOG(4,(ds->base.name, "Setting crypto [%s], errcode=%d", buf, rc));
        if (rc != 0) {
            SSL_CTX_free(ctx);
            return GET_SSL_STATUS(ds);
        }
    }

    /* Set ciphers */
    SSL_CTX_set_cipher_list(ctx, PJMEDIA_SRTP_DTLS_OSSL_CIPHERS);

    /* Set cert & private key */
    rc = SSL_CTX_use_certificate(ctx, dtls_cert);
    pj_assert(rc);
    rc = SSL_CTX_use_PrivateKey(ctx, dtls_priv_key);
    pj_assert(rc);
    rc = SSL_CTX_check_private_key(ctx);
    pj_assert(rc);

    /* Create SSL instance */
    ds->ossl_ctx[idx] = ctx;
    ds->ossl_ssl[idx] = SSL_new(ds->ossl_ctx[idx]);
    if (ds->ossl_ssl[idx] == NULL) {
        SSL_CTX_free(ctx);
        return GET_SSL_STATUS(ds);
    }

    /* Set MTU */
#ifdef DTLS_CTRL_SET_LINK_MTU
    if (!SSL_ctrl(ds->ossl_ssl[idx], DTLS_CTRL_SET_LINK_MTU, PJMEDIA_MAX_MTU,
                  NULL))
    {
        PJ_LOG(4, (ds->base.name,
                  "Ignored failure in setting MTU to %d (too small?)",
                  PJMEDIA_MAX_MTU));
    }
#endif

    /* SSL verification options, must be mutual auth */
    mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    SSL_set_verify(ds->ossl_ssl[idx], mode, &verify_cb);

    /* Setup SSL BIOs */
    ds->ossl_rbio[idx] = BIO_new(BIO_s_mem());
    ds->ossl_wbio[idx] = BIO_new(BIO_s_mem());
    (void)BIO_set_close(ds->ossl_rbio[idx], BIO_CLOSE);
    (void)BIO_set_close(ds->ossl_wbio[idx], BIO_CLOSE);
    SSL_set_bio(ds->ossl_ssl[idx], ds->ossl_rbio[idx], ds->ossl_wbio[idx]);

    return PJ_SUCCESS;
}


/* Destroy SSL context and instance */
static void ssl_destroy(dtls_srtp *ds, unsigned idx)
{
    DTLS_LOCK(ds);

    /* Destroy SSL instance */
    if (ds->ossl_ssl[idx]) {
        /**
         * Avoid calling SSL_shutdown() if handshake wasn't completed.
         * OpenSSL 1.0.2f complains if SSL_shutdown() is called during an
         * SSL handshake, while previous versions always return 0.       
         */
        if (SSL_in_init(ds->ossl_ssl[idx]) == 0) {
            SSL_shutdown(ds->ossl_ssl[idx]);
        }
        SSL_free(ds->ossl_ssl[idx]); /* this will also close BIOs */
        ds->ossl_ssl[idx] = NULL;
        /* thus reset the BIOs as well */
        ds->ossl_rbio[idx] = NULL;
        ds->ossl_wbio[idx] = NULL;
    }

    /* Destroy SSL context */
    if (ds->ossl_ctx[idx]) {
        SSL_CTX_free(ds->ossl_ctx[idx]);
        ds->ossl_ctx[idx] = NULL;
    }

    DTLS_UNLOCK(ds);
}

static pj_status_t ssl_get_srtp_material(dtls_srtp *ds, unsigned idx)
{
    unsigned char material[SRTP_MAX_KEY_LEN * 2];
    SRTP_PROTECTION_PROFILE *profile;
    int rc, i, crypto_idx = -1;
    pjmedia_srtp_crypto *tx, *rx;
    pj_status_t status = PJ_SUCCESS;

    DTLS_LOCK(ds);

    if (!ds->ossl_ssl[idx]) {
        status = PJ_EGONE;
        goto on_return;
    }

    /* Get selected crypto-suite */
    profile = SSL_get_selected_srtp_profile(ds->ossl_ssl[idx]);
    if (!profile) {
        status = PJMEDIA_SRTP_DTLS_ENOCRYPTO;
        goto on_return;
    }

    tx = &ds->tx_crypto[idx];
    rx = &ds->rx_crypto[idx];
    pj_bzero(tx, sizeof(*tx));
    pj_bzero(rx, sizeof(*rx));
    for (i=0; i<(int)PJ_ARRAY_SIZE(ossl_profiles); ++i) {
        if (pj_ansi_stricmp(profile->name, ossl_profiles[i])==0) {
            pj_strset2(&tx->name, pj_profiles[i]);
            pj_strset2(&rx->name, pj_profiles[i]);
            crypto_idx = get_crypto_idx(&tx->name);
            break;
        }
    }
    if (crypto_idx == -1) {
        status = PJMEDIA_SRTP_ENOTSUPCRYPTO;
        goto on_return;
    }

    /* Get keying material from DTLS nego. There seems to be no info about
     * material length returned by SSL_export_keying_material()?
     */
    rc = SSL_export_keying_material(ds->ossl_ssl[idx], material,
                                    sizeof(material), "EXTRACTOR-dtls_srtp",
                                    19, NULL, 0, 0);
    if (rc == 0) {
        status = PJMEDIA_SRTP_EINKEYLEN;
        goto on_return;
    }

    /* Parse SRTP master key & salt from keying material */
    {
        char *p = (char*)material;
        char *k1, *k2;
        crypto_suite *cs = &crypto_suites[crypto_idx];
        unsigned key_len, salt_len;

        key_len = cs->cipher_key_len - cs->cipher_salt_len;
        salt_len = cs->cipher_salt_len;

        tx->key.ptr = (char*)pj_pool_alloc(ds->pool, key_len+salt_len);
        tx->key.slen = key_len+salt_len;
        rx->key.ptr = (char*)pj_pool_alloc(ds->pool, key_len+salt_len);
        rx->key.slen = key_len+salt_len;
        if (ds->setup == DTLS_SETUP_ACTIVE) {
            k1 = tx->key.ptr;
            k2 = rx->key.ptr;
        } else {
            k1 = rx->key.ptr;
            k2 = tx->key.ptr;
        }
        pj_memcpy(k1, p, key_len); p += key_len;
        pj_memcpy(k2, p, key_len); p += key_len;
        pj_memcpy(k1+key_len, p, salt_len); p += salt_len;
        pj_memcpy(k2+key_len, p, salt_len);
        ds->got_keys = PJ_TRUE;
    }

on_return:
    DTLS_UNLOCK(ds);
    return status;
}

/* Match remote fingerprint: SDP vs actual */
static pj_status_t ssl_match_fingerprint(dtls_srtp *ds, unsigned idx)
{
    X509 *rem_cert;
    pj_bool_t is_sha256;
    char buf[128];
    pj_size_t buf_len = sizeof(buf);
    pj_status_t status;

    /* Check hash algo, currently we only support SHA-256 & SHA-1 */
    if (!pj_strnicmp2(&ds->rem_fingerprint, "SHA-256 ", 8))
        is_sha256 = PJ_TRUE;
    else if (!pj_strnicmp2(&ds->rem_fingerprint, "SHA-1 ", 6))
        is_sha256 = PJ_FALSE;
    else {
        PJ_LOG(4,(ds->base.name, "Hash algo specified in remote SDP for "
                  "its DTLS certificate fingerprint is not supported"));
        return PJ_ENOTSUP;
    }

    DTLS_LOCK(ds);
    if (!ds->ossl_ssl[idx]) {
        DTLS_UNLOCK(ds);
        return PJ_EGONE;
    }

    /* Get remote cert & calculate the hash */
    rem_cert = SSL_get_peer_certificate(ds->ossl_ssl[idx]);

    DTLS_UNLOCK(ds);

    if (!rem_cert)
        return PJMEDIA_SRTP_DTLS_EPEERNOCERT;

    status = ssl_get_fingerprint(rem_cert, is_sha256, buf, &buf_len);
    X509_free(rem_cert);
    if (status != PJ_SUCCESS)
        return status;

    /* Do they match? */
    if (pj_stricmp2(&ds->rem_fingerprint, buf))
        return PJMEDIA_SRTP_DTLS_EFPNOTMATCH;

    return PJ_SUCCESS;
}


/* Send data to network */
static pj_status_t send_raw(dtls_srtp *ds, unsigned idx, const void *buf,
                            pj_size_t len)
{
#if DTLS_DEBUG
    PJ_LOG(2,(ds->base.name, "DTLS-SRTP %s sending %lu bytes",
                             CHANNEL_TO_STRING(idx), len));
#endif

    return (idx == RTP_CHANNEL?
            pjmedia_transport_send_rtp(ds->srtp->member_tp, buf, len):
            pjmedia_transport_send_rtcp(ds->srtp->member_tp, buf, len));
}


/* Start socket if member transport is UDP */
static pj_status_t udp_member_transport_media_start(dtls_srtp *ds)
{
    pjmedia_transport_info info;
    pj_status_t status;

    if (!ds->srtp->member_tp)
        return PJ_SUCCESS;

    pjmedia_transport_info_init(&info);
    status = pjmedia_transport_get_info(ds->srtp->member_tp, &info);
    if (status != PJ_SUCCESS)
        return status;

    if (info.specific_info_cnt == 1 &&
        info.spc_info[0].type == PJMEDIA_TRANSPORT_TYPE_UDP)
    {
        return pjmedia_transport_media_start(ds->srtp->member_tp, 0, 0, 0, 0);
    }

    return PJ_SUCCESS;
}


/* Flush write BIO */
static pj_status_t ssl_flush_wbio(dtls_srtp *ds, unsigned idx)
{
    pj_size_t len;
    pj_status_t status = PJ_SUCCESS;

    DTLS_LOCK(ds);

    if (!ds->ossl_wbio[idx]) {
        DTLS_UNLOCK(ds);
        return PJ_EGONE;
    }

    /* Check whether there is data to send */
    if (BIO_ctrl_pending(ds->ossl_wbio[idx]) > 0) {
        /* Yes, get and send it */
        len = BIO_read(ds->ossl_wbio[idx], ds->buf[idx], sizeof(ds->buf));
        if (len > 0) {
            DTLS_UNLOCK(ds);

            status = send_raw(ds, idx, ds->buf[idx], len);
            if (status != PJ_SUCCESS) {
#if DTLS_DEBUG
                pj_perror(2, ds->base.name, status, "Send error");
#endif
                /* This error should be recoverable, remote will retransmit
                 * its packet when not receiving from us.
                 */
            }
            DTLS_LOCK(ds);
        }
    }

    if (!ds->ossl_ssl[idx]) {
        DTLS_UNLOCK(ds);
        return PJ_EGONE;
    }

    /* Just return if handshake completion procedure (key parsing, fingerprint
     * verification, etc) has been done or handshake is still in progress.
     */
    if (ds->nego_completed[idx] || !SSL_is_init_finished(ds->ossl_ssl[idx])) {
        DTLS_UNLOCK(ds);
        return PJ_SUCCESS;
    }

    /* Yes, SSL handshake is done! */
    ds->nego_completed[idx] = PJ_TRUE;
    PJ_LOG(2,(ds->base.name, "DTLS-SRTP negotiation for %s completed!",
                             CHANNEL_TO_STRING(idx)));

    DTLS_UNLOCK(ds);

    /* Stop the retransmission clock. Note that the clock may not be stopped
     * if this function is called from clock thread context. We'll try again
     * later in socket context.
     */
    if (ds->clock[idx])
        pjmedia_clock_stop(ds->clock[idx]);

    /* Get SRTP key material */
    status = ssl_get_srtp_material(ds, idx);
    if (status != PJ_SUCCESS) {
        pj_perror(4, ds->base.name, status,
                  "Failed to get SRTP material");
        goto on_return;
    }

    /* Verify remote fingerprint if we've already got one from SDP */
    if (ds->rem_fingerprint.slen && ds->rem_fprint_status == PJ_EPENDING) {
        ds->rem_fprint_status = status = ssl_match_fingerprint(ds, idx);
        if (status != PJ_SUCCESS) {
            pj_perror(4, ds->base.name, status,
                      "Fingerprint specified in remote SDP doesn't match "
                      "to actual remote certificate fingerprint!");
            goto on_return;
        }
    }

    /* If media_start() has been called, start SRTP now */
    if (ds->pending_start && idx == RTP_CHANNEL) {
        ds->pending_start = PJ_FALSE;
        ds->srtp->keying_pending_cnt--;

        /* Copy negotiated policy to SRTP */
        ds->srtp->srtp_ctx.tx_policy_neg = ds->tx_crypto[idx];
        ds->srtp->srtp_ctx.rx_policy_neg = ds->rx_crypto[idx];

        status = start_srtp(ds->srtp);
        if (status != PJ_SUCCESS)
            pj_perror(4, ds->base.name, status, "Failed starting SRTP");
    } else if (idx == RTCP_CHANNEL) {
        pjmedia_srtp_setting setting;

        pjmedia_srtp_setting_default (&setting);

        /* Copy negotiated policy to SRTP */
        ds->srtp->srtp_rtcp.tx_policy_neg = ds->tx_crypto[idx];
        ds->srtp->srtp_rtcp.rx_policy_neg = ds->rx_crypto[idx];

        status = create_srtp_ctx(ds->srtp, &ds->srtp->srtp_rtcp,
                                 &setting, &ds->srtp->srtp_rtcp.tx_policy_neg,
                                 &ds->srtp->srtp_rtcp.rx_policy_neg);
        if (status != PJ_SUCCESS)
            pj_perror(4, ds->base.name, status, "Failed creating SRTP RTCP");
    }

on_return:
    if (idx == RTP_CHANNEL && ds->srtp->setting.cb.on_srtp_nego_complete) {
        (*ds->srtp->setting.cb.on_srtp_nego_complete)
                                            (&ds->srtp->base, status);
    }

    return status;
}


static void clock_cb(const pj_timestamp *ts, void *user_data)
{
    dtls_srtp_channel *ds_ch = (dtls_srtp_channel*)user_data;
    dtls_srtp *ds = ds_ch->dtls_srtp;
    unsigned idx = ds_ch->channel;
    pj_status_t status;

    PJ_UNUSED_ARG(ts);

    while (1) {
        /* Check if we should quit before trying to acquire the lock. */
        if (ds->nego_completed[idx])
            return;

        /* To avoid deadlock, we must use TRY_LOCK here. */
        status = DTLS_TRY_LOCK(ds);
        if (status == PJ_SUCCESS)
            break;

        /* Acquiring lock failed, check if we have been signaled to quit. */
        if (ds->nego_completed[idx])
            return;

        pj_thread_sleep(20);
    }


    if (!ds->ossl_ssl[idx]) {
        DTLS_UNLOCK(ds);
        return;
    }

    if (DTLSv1_handle_timeout(ds->ossl_ssl[idx]) > 0) {
        DTLS_UNLOCK(ds);
        ssl_flush_wbio(ds, idx);
    } else {
        DTLS_UNLOCK(ds);
    }
}


/* Asynchronous handshake */
static pj_status_t ssl_handshake_channel(dtls_srtp *ds, unsigned idx)
{
    pj_status_t status;
    int err;

    DTLS_LOCK(ds);

    /* Init DTLS (if not yet) */
    status = ssl_create(ds, idx);
    if (status != PJ_SUCCESS) {
        DTLS_UNLOCK(ds);
        return status;
    }

    /* Check if handshake has been initiated or even completed */
    if (ds->nego_started[idx] || SSL_is_init_finished(ds->ossl_ssl[idx])) {
        DTLS_UNLOCK(ds);
        return PJ_SUCCESS;
    }

    /* Perform SSL handshake */
    if (ds->setup == DTLS_SETUP_ACTIVE) {
        SSL_set_connect_state(ds->ossl_ssl[idx]);
    } else {
        SSL_set_accept_state(ds->ossl_ssl[idx]);
    }
    err = SSL_do_handshake(ds->ossl_ssl[idx]);
    if (err < 0) {
        err = SSL_get_error(ds->ossl_ssl[idx], err);

        DTLS_UNLOCK(ds);

        if (err == SSL_ERROR_WANT_READ) {
            status = ssl_flush_wbio(ds, idx);
            if (status != PJ_SUCCESS)
                goto on_return;
        } else if (err != SSL_ERROR_NONE) {
            /* Handshake fails */
            status = STATUS_FROM_SSL_ERR(ds, err);
            pj_perror(2, ds->base.name, status, "SSL_do_handshake() error");
            goto on_return;
        }
    } else {
        DTLS_UNLOCK(ds);
    }

    /* Create and start clock @4Hz for retransmission */
    if (!ds->clock[idx]) {
        ds->channel[idx].dtls_srtp = ds;
        ds->channel[idx].channel = idx;
        status = pjmedia_clock_create(ds->pool, 4, 1, 1,
                                      PJMEDIA_CLOCK_NO_HIGHEST_PRIO, clock_cb,
                                      &ds->channel[idx], &ds->clock[idx]);
        if (status != PJ_SUCCESS)
            goto on_return;
    }    
    status = pjmedia_clock_start(ds->clock[idx]);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Finally, DTLS nego started! */
    ds->nego_started[idx] = PJ_TRUE;
    PJ_LOG(4,(ds->base.name, "DTLS-SRTP %s negotiation initiated as %s",
              CHANNEL_TO_STRING(idx),
              (ds->setup==DTLS_SETUP_ACTIVE? "client":"server")));

on_return:
    if (status != PJ_SUCCESS) {
        ds->nego_completed[idx] = PJ_TRUE;
        if (ds->clock[idx])
            pjmedia_clock_stop(ds->clock[idx]);
    }
    return status;
}

static pj_status_t ssl_handshake(dtls_srtp *ds)
{
    pj_status_t status;

    status = ssl_handshake_channel(ds, RTP_CHANNEL);
    if (status != PJ_SUCCESS)
        return status;

    if (!ds->srtp->use_rtcp_mux)
        status = ssl_handshake_channel(ds, RTCP_CHANNEL);

    return status;
}

/* Parse a=setup & a=fingerprint in remote SDP to update DTLS-SRTP states
 * 'setup' and 'rem_fingerprint'.
 * TODO: check those attributes in a=acap too?
 */
static pj_status_t parse_setup_finger_attr(dtls_srtp *ds,
                                           pj_bool_t rem_as_offerer,
                                           const pjmedia_sdp_session *sdp,
                                           unsigned media_index)
{
    pjmedia_sdp_media *m;
    pjmedia_sdp_attr *a;

    m = sdp->media[media_index];

    /* Parse a=setup */
    a = pjmedia_sdp_media_find_attr(m, &ID_SETUP, NULL);
    if (!a)
        a = pjmedia_sdp_attr_find(sdp->attr_count,
                                  sdp->attr, &ID_SETUP, NULL);
    if (!a)
        return PJMEDIA_SRTP_ESDPAMBIGUEANS;

    if (pj_stristr(&a->value, &ID_PASSIVE) ||
        (rem_as_offerer && pj_stristr(&a->value, &ID_ACTPASS)))
    {
        /* Remote offers/answers 'passive' (or offers 'actpass'), so we are
         * the client.
         */
        ds->setup = DTLS_SETUP_ACTIVE;
    } else if (pj_stristr(&a->value, &ID_ACTIVE)) {
        /* Remote offers/answers 'active' so we are the server. */
        ds->setup = DTLS_SETUP_PASSIVE;
    } else {
        /* Unknown value set in remote a=setup */
        return PJMEDIA_SRTP_ESDPAMBIGUEANS;
    }

    /* Parse a=fingerprint */
    a = pjmedia_sdp_media_find_attr(m, &ID_FINGERPRINT, NULL);
    if (!a)
        a = pjmedia_sdp_attr_find(sdp->attr_count,
                                  sdp->attr, &ID_FINGERPRINT,
                                  NULL);
    if (!a) {
        /* No fingerprint attribute in remote SDP */
        return PJMEDIA_SRTP_DTLS_ENOFPRINT;
    } else {
        pj_str_t rem_fp = a->value;
        pj_strtrim(&rem_fp);
        if (pj_stricmp(&ds->rem_fingerprint, &rem_fp))
            pj_strdup(ds->pool, &ds->rem_fingerprint, &rem_fp);
    }

    return PJ_SUCCESS;
}

static pj_status_t get_rem_addrs(dtls_srtp *ds,
                                 const pjmedia_sdp_session *sdp_remote,
                                 unsigned media_index,
                                 pj_sockaddr *rem_rtp,
                                 pj_sockaddr *rem_rtcp,
                                 pj_bool_t *rtcp_mux)
{
    pjmedia_sdp_media *m_rem = sdp_remote->media[media_index];
    pjmedia_sdp_conn *conn;
    pjmedia_sdp_attr *a;
    int af = pj_AF_UNSPEC();
    pj_bool_t use_ice_info = PJ_FALSE;

    /* Init RTP & RTCP address */
    pj_bzero(rem_rtp, sizeof(*rem_rtp));
    pj_bzero(rem_rtcp, sizeof(*rem_rtcp));

    /* If underlying transport is ICE, get remote addresses from ICE */
    if (ds->use_ice) {
        pjmedia_transport_info info;
        pjmedia_ice_transport_info *ice_info;

        pjmedia_transport_info_init(&info);
        pjmedia_transport_get_info(ds->srtp->member_tp, &info);
        ice_info = (pjmedia_ice_transport_info*)
                   pjmedia_transport_info_get_spc_info(
                                    &info, PJMEDIA_TRANSPORT_TYPE_ICE);
        if (ice_info) {
            *rem_rtp = ice_info->comp[0].rcand_addr;
            if (ice_info->comp_cnt > 1)
                *rem_rtcp = ice_info->comp[1].rcand_addr;

            use_ice_info = PJ_TRUE;
        }
    }

    /* Get remote addresses from SDP */
    if (!use_ice_info) {

        /* Get RTP address */
        conn = m_rem->conn ? m_rem->conn : sdp_remote->conn;
        if (pj_stricmp2(&conn->net_type, "IN")==0) {
            if (pj_stricmp2(&conn->addr_type, "IP4")==0) {
                af = pj_AF_INET();
            } else if (pj_stricmp2(&conn->addr_type, "IP6")==0) {
                af = pj_AF_INET6();
            }
        }
        if (af != pj_AF_UNSPEC()) {
            pj_sockaddr_init(af, rem_rtp, &conn->addr,
                             m_rem->desc.port);
        } else {
            return PJ_EAFNOTSUP;
        }

        /* Get RTCP address. If "rtcp" attribute is present in the SDP,
         * set the RTCP address from that attribute. Otherwise, calculate
         * from RTP address.
         */
        a = pjmedia_sdp_attr_find2(m_rem->attr_count, m_rem->attr,
                                   "rtcp", NULL);
        if (a) {
            pjmedia_sdp_rtcp_attr rtcp;
            pj_status_t status;
            status = pjmedia_sdp_attr_get_rtcp(a, &rtcp);
            if (status == PJ_SUCCESS) {
                if (rtcp.addr.slen) {
                    pj_sockaddr_init(af, rem_rtcp, &rtcp.addr,
                                     (pj_uint16_t)rtcp.port);
                } else {
                    pj_sockaddr_init(af, rem_rtcp, NULL,
                                     (pj_uint16_t)rtcp.port);
                    pj_memcpy(pj_sockaddr_get_addr(rem_rtcp),
                              pj_sockaddr_get_addr(rem_rtp),
                              pj_sockaddr_get_addr_len(rem_rtp));
                }
            }
        }
        if (!pj_sockaddr_has_addr(rem_rtcp)) {
            int rtcp_port;
            pj_memcpy(rem_rtcp, rem_rtp, sizeof(pj_sockaddr));
            rtcp_port = pj_sockaddr_get_port(rem_rtp) + 1;
            pj_sockaddr_set_port(rem_rtcp, (pj_uint16_t)rtcp_port);
        }
    }

    /* Check if remote indicates the desire to use rtcp-mux in its SDP. */
    if (rtcp_mux) {
        a = pjmedia_sdp_attr_find2(m_rem->attr_count, m_rem->attr,
                                   "rtcp-mux", NULL);
        *rtcp_mux = (a? PJ_TRUE: PJ_FALSE);
    }

    return PJ_SUCCESS;
}

/* Check if an incoming packet is a DTLS packet (rfc5764 section 5.1.2) */
#define IS_DTLS_PKT(pkt, pkt_len) (*(char*)pkt > 19 && *(char*)pkt < 64)


/* Received packet (SSL handshake) from socket */
static pj_status_t ssl_on_recv_packet(dtls_srtp *ds, unsigned idx,
                                      const void *data, pj_size_t len)
{
    char tmp[128];
    pj_size_t nwritten;

    DTLS_LOCK(ds);

    if (!ds->ossl_rbio[idx]) {
        DTLS_UNLOCK(ds);
        return PJ_EGONE;
    }

    nwritten = BIO_write(ds->ossl_rbio[idx], data, (int)len);
    if (nwritten < len) {
        /* Error? */
        pj_status_t status;
        status = GET_SSL_STATUS(ds);
#if DTLS_DEBUG
        pj_perror(2, ds->base.name, status, "BIO_write() error");
#endif
        DTLS_UNLOCK(ds);
        return status;
    }

    if (!ds->ossl_ssl[idx]) {
        DTLS_UNLOCK(ds);
        return PJ_EGONE;
    }

    /* Consume (and ignore) the packet */
    while (1) {
        int rc = SSL_read(ds->ossl_ssl[idx], tmp, sizeof(tmp));
        if (rc <= 0) {
#if DTLS_DEBUG
            pj_status_t status = GET_SSL_STATUS(ds);
            if (status != PJ_SUCCESS)
                pj_perror(2, ds->base.name, status, "SSL_read() error");
#endif
            break;
        }
    }

    DTLS_UNLOCK(ds);

    /* Flush anything pending in the write BIO */
    return ssl_flush_wbio(ds, idx);
}


static void on_ice_complete2(pjmedia_transport *tp,
                             pj_ice_strans_op op,
                             pj_status_t status,
                             void *user_data)
{
    dtls_srtp *ds = (dtls_srtp*)user_data;
    pj_assert(ds);

    PJ_UNUSED_ARG(tp);

    if (op == PJ_ICE_STRANS_OP_NEGOTIATION && status == PJ_SUCCESS &&
        ds->setup == DTLS_SETUP_ACTIVE)
    {
        pj_status_t tmp_st;
        tmp_st = ssl_handshake(ds);
        if (tmp_st != PJ_SUCCESS)
            pj_perror(4, ds->base.name, tmp_st, "Failed starting DTLS nego");
    }
}


/* *************************************
 *
 * DTLS-SRTP transport keying operations
 *
 * *************************************/

static pj_status_t dtls_on_recv(pjmedia_transport *tp, unsigned idx,     
                                const void *pkt, pj_size_t size)
{
    dtls_srtp *ds = (dtls_srtp*)tp;

    DTLS_LOCK(ds);

    /* Destroy the retransmission clock if handshake has been completed. */
    if (ds->clock[idx] && ds->nego_completed[idx]) {
        pjmedia_clock_destroy(ds->clock[idx]);
        ds->clock[idx] = NULL;
    }

    if (size < 1 || !IS_DTLS_PKT(pkt, size) || ds->is_destroying) {
        DTLS_UNLOCK(ds);
        return PJ_EIGNORED;
    }

#if DTLS_DEBUG
    PJ_LOG(2,(ds->base.name, "DTLS-SRTP %s receiving %lu bytes",
                             CHANNEL_TO_STRING(idx), size));
#endif

    /* This is DTLS packet, let's process it. Note that if DTLS nego has
     * been completed, this may be a retransmission (e.g: remote didn't
     * receive our last handshake packet) or just a stray.
     */

    /* Check remote address info, reattach member tp if changed */
    if (!ds->use_ice && !ds->nego_completed[idx]) {
        pjmedia_transport_info info;
        pj_bool_t reattach_tp = PJ_FALSE;

        pjmedia_transport_get_info(ds->srtp->member_tp, &info);

        if (idx == RTP_CHANNEL &&
            pj_sockaddr_cmp(&ds->rem_addr, &info.src_rtp_name))
        {
            pj_sockaddr_cp(&ds->rem_addr, &info.src_rtp_name);
            reattach_tp = PJ_TRUE;
        } else if (idx == RTCP_CHANNEL && !ds->srtp->use_rtcp_mux &&
                   pj_sockaddr_has_addr(&info.src_rtcp_name) &&
                   pj_sockaddr_cmp(&ds->rem_rtcp, &info.src_rtcp_name))
        {
            pj_sockaddr_cp(&ds->rem_rtcp, &info.src_rtcp_name);
            reattach_tp = PJ_TRUE;
        }

        if (reattach_tp) {
            pjmedia_transport_attach_param ap;
            pj_status_t status;

            /* Attach member transport */
            pj_bzero(&ap, sizeof(ap));
            ap.user_data = ds->srtp;
            if (pj_sockaddr_has_addr(&ds->rem_addr)) {
                pj_sockaddr_cp(&ap.rem_addr, &ds->rem_addr);
            } else {
                pj_sockaddr_init(pj_AF_INET(), &ap.rem_addr, 0, 0);
            }
            if (ds->srtp->use_rtcp_mux) {
                /* Using RTP & RTCP multiplexing */
                pj_sockaddr_cp(&ap.rem_rtcp, &ap.rem_addr);
            } else if (pj_sockaddr_has_addr(&ds->rem_rtcp)) {
                pj_sockaddr_cp(&ap.rem_rtcp, &ds->rem_rtcp);
            } else if (pj_sockaddr_has_addr(&ds->rem_addr)) {
                pj_sockaddr_cp(&ap.rem_rtcp, &ds->rem_addr);
                pj_sockaddr_set_port(&ap.rem_rtcp,
                                     pj_sockaddr_get_port(&ap.rem_rtcp) + 1);
            } else {
                pj_sockaddr_init(pj_AF_INET(), &ap.rem_rtcp, 0, 0);
            }
            ap.addr_len = pj_sockaddr_get_len(&ap.rem_addr);
            status = pjmedia_transport_attach2(&ds->srtp->base, &ap);
            if (status != PJ_SUCCESS) {
                DTLS_UNLOCK(ds);
                return status;
            }

#if DTLS_DEBUG
            {
                char addr[PJ_INET6_ADDRSTRLEN];
                char addr2[PJ_INET6_ADDRSTRLEN];
                PJ_LOG(2,(ds->base.name, "Re-attached transport to update "
                          "remote addr=%s remote rtcp=%s",
                          pj_sockaddr_print(&ap.rem_addr, addr,
                                            sizeof(addr), 3),
                          pj_sockaddr_print(&ap.rem_rtcp, addr2,
                                            sizeof(addr2), 3)));
            }
#endif
        }
    }

    /* If our setup is ACTPASS, incoming packet may be a client hello,
     * so let's update setup to PASSIVE and initiate DTLS handshake.
     */
    if (!ds->nego_started[idx] &&
        (ds->setup == DTLS_SETUP_ACTPASS || ds->setup == DTLS_SETUP_PASSIVE))
    {
        pj_status_t status;
        ds->setup = DTLS_SETUP_PASSIVE;
        status = ssl_handshake_channel(ds, idx);
        if (status != PJ_SUCCESS) {
            DTLS_UNLOCK(ds);
            return status;
        }
    }

    DTLS_UNLOCK(ds);

    /* Send it to OpenSSL */
    ssl_on_recv_packet(ds, idx, pkt, size);

    return PJ_SUCCESS;
}

/*
 * This callback is called by SRTP transport when incoming rtp is received.
 * Originally this is send_rtp() op.
 */
static pj_status_t dtls_on_recv_rtp( pjmedia_transport *tp,
                                     const void *pkt,
                                     pj_size_t size)
{
    return dtls_on_recv(tp, RTP_CHANNEL, pkt, size);
}

/*
 * This callback is called by SRTP transport when incoming rtcp is received.
 * Originally this is send_rtcp() op.
 */
static pj_status_t dtls_on_recv_rtcp(pjmedia_transport *tp,
                                     const void *pkt,
                                     pj_size_t size)
{
    return dtls_on_recv(tp, RTCP_CHANNEL, pkt, size);
}

static pj_status_t dtls_media_create( pjmedia_transport *tp,
                                      pj_pool_t *sdp_pool,
                                      unsigned options,
                                      const pjmedia_sdp_session *sdp_remote,
                                      unsigned media_index)
{
    dtls_srtp *ds = (dtls_srtp*) tp;
    pj_status_t status = PJ_SUCCESS;

#if DTLS_DEBUG
    PJ_LOG(2,(ds->base.name, "dtls_media_create()"));
#endif

    PJ_UNUSED_ARG(sdp_pool);
    PJ_UNUSED_ARG(options);

    if (ds->srtp->offerer_side) {
        /* As offerer: do nothing. */
    } else {
        /* As answerer:
         *    Check for DTLS-SRTP support in remote SDP. Detect remote
         *    support of DTLS-SRTP by inspecting remote SDP offer for
         *    SDP a=fingerprint attribute. And currently we only support
         *    RTP/AVP transports.
         */
        pjmedia_sdp_media *m_rem = sdp_remote->media[media_index];
        pjmedia_sdp_attr *attr_fp;
        pj_uint32_t rem_proto = 0;

        /* Find SDP a=fingerprint line. */
        attr_fp = pjmedia_sdp_media_find_attr(m_rem, &ID_FINGERPRINT, NULL);
        if (!attr_fp)
            attr_fp = pjmedia_sdp_attr_find(sdp_remote->attr_count,
                                            sdp_remote->attr, &ID_FINGERPRINT,
                                            NULL);

        /* Get media transport proto */
        rem_proto = pjmedia_sdp_transport_get_proto(&m_rem->desc.transport);
        if (!PJMEDIA_TP_PROTO_HAS_FLAG(rem_proto, PJMEDIA_TP_PROTO_RTP_AVP) ||
            !attr_fp)
        {
            /* Remote doesn't signal DTLS-SRTP */
            status = PJMEDIA_SRTP_ESDPINTRANSPORT;
            goto on_return;
        }

        /* Check for a=fingerprint in remote SDP. */
        switch (ds->srtp->setting.use) {
            case PJMEDIA_SRTP_DISABLED:
                status = PJMEDIA_SRTP_ESDPINTRANSPORT;
                goto on_return;
                break;
            case PJMEDIA_SRTP_OPTIONAL:
                break;
            case PJMEDIA_SRTP_MANDATORY:
                break;
        }
    }

    /* Set remote cert fingerprint verification status to PJ_EPENDING */
    ds->rem_fprint_status = PJ_EPENDING;

on_return:
#if DTLS_DEBUG
    if (status != PJ_SUCCESS) {
        pj_perror(4, ds->base.name, status, "dtls_media_create() failed");
    }
#endif
    return status;
}

static void dtls_media_stop_channel(dtls_srtp *ds, unsigned idx)
{
    ds->nego_completed[idx] = PJ_TRUE;
    if (ds->clock[idx])
        pjmedia_clock_stop(ds->clock[idx]);

    /* Reset DTLS state */
    ssl_destroy(ds, idx);
    ds->nego_started[idx] = PJ_FALSE;
    ds->nego_completed[idx] = PJ_FALSE;
}

static pj_status_t dtls_encode_sdp( pjmedia_transport *tp,
                                    pj_pool_t *sdp_pool,
                                    pjmedia_sdp_session *sdp_local,
                                    const pjmedia_sdp_session *sdp_remote,
                                    unsigned media_index)
{
    dtls_srtp *ds = (dtls_srtp *)tp;
    pjmedia_sdp_media *m_loc;
    pjmedia_sdp_attr *a;
    pj_bool_t use_ice = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;

#if DTLS_DEBUG
    PJ_LOG(2,(ds->base.name, "dtls_encode_sdp()"));
#endif

    PJ_UNUSED_ARG(sdp_pool);

    m_loc = sdp_local->media[media_index];
    if (ds->srtp->offerer_side) {
        /* As offerer */

        /* Add attribute a=setup if none (rfc5763 section 5) */
        a = pjmedia_sdp_media_find_attr(m_loc, &ID_SETUP, NULL);
        if (!a)
            a = pjmedia_sdp_attr_find(sdp_local->attr_count,
                                      sdp_local->attr, &ID_SETUP, NULL);
        if (!a) {
            pj_str_t val;

            if (ds->setup == DTLS_SETUP_UNKNOWN)
                ds->setup = DTLS_SETUP_ACTPASS;
            
            if (ds->setup == DTLS_SETUP_ACTIVE)
                val = ID_ACTIVE;
            else if (ds->setup == DTLS_SETUP_PASSIVE)
                val = ID_PASSIVE;
            else
                val = ID_ACTPASS;
            a = pjmedia_sdp_attr_create(ds->pool, ID_SETUP.ptr, &val);
            pjmedia_sdp_media_add_attr(m_loc, a);
        }
    } else {
        /* As answerer */
        dtls_setup last_setup = ds->setup;
        pj_str_t last_rem_fp = ds->rem_fingerprint;
        pj_bool_t rem_addr_changed = PJ_FALSE;

        /* Parse a=setup and a=fingerprint */
        status = parse_setup_finger_attr(ds, PJ_TRUE, sdp_remote,
                                         media_index);
        if (status != PJ_SUCCESS)
            goto on_return;

        /* Add attribute a=setup:active/passive if we are client/server. */
        a = pjmedia_sdp_attr_create(ds->pool, ID_SETUP.ptr,
                    (ds->setup==DTLS_SETUP_ACTIVE? &ID_ACTIVE:&ID_PASSIVE));
        pjmedia_sdp_media_add_attr(m_loc, a);

        if (last_setup != DTLS_SETUP_UNKNOWN) {
            pj_sockaddr rem_rtp;
            pj_sockaddr rem_rtcp;
            pj_bool_t use_rtcp_mux;

            status = get_rem_addrs(ds, sdp_remote, media_index, &rem_rtp,
                                   &rem_rtcp, &use_rtcp_mux);
            if (status == PJ_SUCCESS) {
                if (use_rtcp_mux) {
                    /* Remote indicates it wants to use rtcp-mux */
                    pjmedia_transport_info info;

                    pjmedia_transport_info_init(&info);
                    pjmedia_transport_get_info(ds->srtp->member_tp, &info);
                    if (pj_sockaddr_cmp(&info.sock_info.rtp_addr_name,
                        &info.sock_info.rtcp_addr_name))
                    {
                        /* But we do not wish to use rtcp mux */
                        use_rtcp_mux = PJ_FALSE;
                    }
                }
                if (pj_sockaddr_has_addr(&ds->rem_addr) &&
                    pj_sockaddr_has_addr(&rem_rtp) &&
                    (pj_sockaddr_cmp(&ds->rem_addr, &rem_rtp) ||
                     (!use_rtcp_mux &&
                      pj_sockaddr_has_addr(&ds->rem_rtcp) &&
                      pj_sockaddr_has_addr(&rem_rtcp) &&
                      pj_sockaddr_cmp(&ds->rem_rtcp, &rem_rtcp))))
                {
                    rem_addr_changed = PJ_TRUE;
                }
            }
        }

        /* Check if remote signals DTLS re-nego by changing its
         * setup/fingerprint in SDP or media transport address in SDP.
         */
        if ((last_setup != DTLS_SETUP_UNKNOWN && last_setup != ds->setup) ||
            (last_rem_fp.slen &&
             pj_memcmp(&last_rem_fp, &ds->rem_fingerprint, sizeof(pj_str_t)))||
            (rem_addr_changed))
        {
            dtls_media_stop_channel(ds, RTP_CHANNEL);
            dtls_media_stop_channel(ds, RTCP_CHANNEL);
            ds->got_keys = PJ_FALSE;
            ds->rem_fprint_status = PJ_EPENDING;
        }
    }

    /* Set media transport to UDP/TLS/RTP/SAVP if we are the offerer,
     * otherwise just match it to the offer (currently we only accept
     * UDP/TLS/RTP/SAVP in remote offer though).
     */
    if (ds->srtp->offerer_side) {
        m_loc->desc.transport = ID_TP_DTLS_SRTP;
    } else {
        m_loc->desc.transport = 
                            sdp_remote->media[media_index]->desc.transport;
    }

    /* Add a=fingerprint attribute, fingerprint of our TLS certificate */
    {
        char buf[128];
        pj_size_t buf_len = sizeof(buf);
        pj_str_t fp;

        status = ssl_get_fingerprint(dtls_cert, PJ_TRUE, buf, &buf_len);
        if (status != PJ_SUCCESS)
            goto on_return;

        pj_strset(&fp, buf, buf_len);
        a = pjmedia_sdp_attr_create(ds->pool, ID_FINGERPRINT.ptr, &fp);
        pjmedia_sdp_media_add_attr(m_loc, a);
    }

    if (ds->nego_completed[RTP_CHANNEL]) {
        /* This is subsequent SDP offer/answer and no DTLS re-nego has been
         * signalled.
         */
        goto on_return;
    }

    /* Attach member transport, so we can receive DTLS init (if our setup
     * is PASSIVE/ACTPASS) or send DTLS init (if our setup is ACTIVE).
     */
    {
        pjmedia_transport_attach_param ap;
        pjmedia_transport_info info;

        pj_bzero(&ap, sizeof(ap));
        ap.user_data = ds->srtp;
        pjmedia_transport_get_info(ds->srtp->member_tp, &info);

        if (sdp_remote) {
            get_rem_addrs(ds, sdp_remote, media_index, &ds->rem_addr,
                          &ds->rem_rtcp, NULL);
        }

        if (pj_sockaddr_has_addr(&ds->rem_addr)) {
            pj_sockaddr_cp(&ap.rem_addr, &ds->rem_addr);
        } else if (pj_sockaddr_has_addr(&info.sock_info.rtp_addr_name)) {
            pj_sockaddr_cp(&ap.rem_addr, &info.sock_info.rtp_addr_name);
        } else {
            pj_sockaddr_init(pj_AF_INET(), &ap.rem_addr, 0, 0);
        }

        if (pj_sockaddr_cmp(&info.sock_info.rtp_addr_name,
                            &info.sock_info.rtcp_addr_name) == 0)
        {
            /* Using RTP & RTCP multiplexing */
            pj_sockaddr_cp(&ap.rem_rtcp, &ap.rem_addr);
        } else if (pj_sockaddr_has_addr(&ds->rem_rtcp)) {
            pj_sockaddr_cp(&ap.rem_rtcp, &ds->rem_rtcp);
        } else if (pj_sockaddr_has_addr(&info.sock_info.rtcp_addr_name)) {
            pj_sockaddr_cp(&ap.rem_rtcp, &info.sock_info.rtcp_addr_name);
        } else {
            pj_sockaddr_init(pj_AF_INET(), &ap.rem_rtcp, 0, 0);
        }

        ap.addr_len = pj_sockaddr_get_len(&ap.rem_addr);
        status = pjmedia_transport_attach2(&ds->srtp->base, &ap);
        if (status != PJ_SUCCESS)
            goto on_return;

        /* Start member transport if it is UDP, so we can receive packet
         * (see also #2097).
         */
        udp_member_transport_media_start(ds);

#if DTLS_DEBUG
        {
            char addr[PJ_INET6_ADDRSTRLEN];
            char addr2[PJ_INET6_ADDRSTRLEN];
            PJ_LOG(2,(ds->base.name, "Attached transport, remote addr=%s "
                                     "remote rtcp=%s",
                      pj_sockaddr_print(&ap.rem_addr, addr2, sizeof(addr2), 3),
                      pj_sockaddr_print(&ap.rem_rtcp, addr, sizeof(addr), 3)));
        }
#endif
    }

    /* If our setup is ACTIVE and member transport is not ICE,
     * start DTLS nego.
     */
    if (ds->setup == DTLS_SETUP_ACTIVE) {
        pjmedia_transport_info info;
        pjmedia_ice_transport_info *ice_info;

        pjmedia_transport_info_init(&info);
        pjmedia_transport_get_info(ds->srtp->member_tp, &info);
        ice_info = (pjmedia_ice_transport_info*)
                   pjmedia_transport_info_get_spc_info(
                                    &info, PJMEDIA_TRANSPORT_TYPE_ICE);
        use_ice = ice_info && ice_info->comp_cnt;
        if (!use_ice) {
            /* Start SSL nego */
            status = ssl_handshake(ds);
            if (status != PJ_SUCCESS)
                goto on_return;
        }
    }

on_return:
#if DTLS_DEBUG
    if (status != PJ_SUCCESS) {
        pj_perror(4, ds->base.name, status, "dtls_encode_sdp() failed");
    }
#endif
    return status;
}


static pj_status_t dtls_media_start( pjmedia_transport *tp,
                                     pj_pool_t *tmp_pool,
                                     const pjmedia_sdp_session *sdp_local,
                                     const pjmedia_sdp_session *sdp_remote,
                                     unsigned media_index)
{
    dtls_srtp *ds = (dtls_srtp *)tp;
    pj_ice_strans_state ice_state;
    pj_bool_t use_rtcp_mux = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;
    struct transport_srtp *srtp = (struct transport_srtp*)tp->user_data;

#if DTLS_DEBUG
    PJ_LOG(2,(ds->base.name, "dtls_media_start()"));
#endif

    PJ_UNUSED_ARG(tmp_pool);
    PJ_UNUSED_ARG(sdp_local);

    if (ds->srtp->offerer_side) {
        /* As offerer */
        dtls_setup last_setup = ds->setup;
        pj_str_t last_rem_fp = ds->rem_fingerprint;

        /* Parse a=setup and a=fingerprint */
        status = parse_setup_finger_attr(ds, PJ_FALSE, sdp_remote,
                                         media_index);
        if (status != PJ_SUCCESS)
            goto on_return;

        /* Check if remote signals DTLS re-nego by changing its
         * setup/fingerprint in SDP.
         */
        if ((last_setup != DTLS_SETUP_ACTPASS && last_setup != ds->setup) ||
            (last_rem_fp.slen &&
             pj_memcmp(&last_rem_fp, &ds->rem_fingerprint, sizeof(pj_str_t))))
        {
            dtls_media_stop_channel(ds, RTP_CHANNEL);
            dtls_media_stop_channel(ds, RTCP_CHANNEL);
            ds->got_keys = PJ_FALSE;
            ds->rem_fprint_status = PJ_EPENDING;
        }
    } else {
        /* As answerer */
        
        /* Nothing to do? */
    }

    /* Check and update ICE and rtcp-mux status */
    {
        pjmedia_transport_info info;
        pjmedia_ice_transport_info *ice_info;

        pjmedia_transport_info_init(&info);
        pjmedia_transport_get_info(ds->srtp->member_tp, &info);
        if (pj_sockaddr_cmp(&info.sock_info.rtp_addr_name,
                            &info.sock_info.rtcp_addr_name) == 0)
        {
            ds->srtp->use_rtcp_mux = use_rtcp_mux = PJ_TRUE;
        }
        ice_info = (pjmedia_ice_transport_info*)
                   pjmedia_transport_info_get_spc_info(
                                    &info, PJMEDIA_TRANSPORT_TYPE_ICE);
        ds->use_ice = ice_info && ice_info->active;
        ice_state = ds->use_ice? ice_info->sess_state : 0;

        /* Update remote RTP & RTCP addresses */
        get_rem_addrs(ds, sdp_remote, media_index, &ds->rem_addr,
                      &ds->rem_rtcp, NULL);
    }

    /* Check if the background DTLS nego has completed */
    if (ds->got_keys) { 
        unsigned idx = RTP_CHANNEL;

        ds->srtp->srtp_ctx.tx_policy_neg = ds->tx_crypto[idx];
        ds->srtp->srtp_ctx.rx_policy_neg = ds->rx_crypto[idx];

        /* Verify remote fingerprint (if available) */
        if (ds->rem_fingerprint.slen && ds->rem_fprint_status == PJ_EPENDING)
        {
            ds->rem_fprint_status = ssl_match_fingerprint(ds, idx);
            if (ds->rem_fprint_status != PJ_SUCCESS) {
                pj_perror(4, ds->base.name, ds->rem_fprint_status,
                          "Fingerprint specified in remote SDP doesn't match "
                          "to actual remote certificate fingerprint!");
                return ds->rem_fprint_status;
            }
        }

        return PJ_SUCCESS;
    } 

    /* SRTP key is not ready, SRTP start is pending */
    ds->srtp->keying_pending_cnt++;
    ds->pending_start = PJ_TRUE;

    srtp->peer_use = PJMEDIA_SRTP_MANDATORY;

    /* If our DTLS setup is ACTIVE:
     * - start DTLS nego after ICE nego, or
     * - start it now if there is no ICE.
     */
    if (ds->setup == DTLS_SETUP_ACTIVE) {
        if (ds->use_ice && ice_state < PJ_ICE_STRANS_STATE_RUNNING)  {
            /* Register ourselves to listen to ICE notifications */
            pjmedia_ice_cb ice_cb;
            pj_bzero(&ice_cb, sizeof(ice_cb));
            ice_cb.on_ice_complete2 = &on_ice_complete2;
            pjmedia_ice_add_ice_cb(ds->srtp->member_tp, &ice_cb, ds);
        } else {
            /* This can happen when we are SDP offerer and remote wants
             * PASSIVE DTLS role.
             */
            pjmedia_transport_attach_param ap;
            pj_bzero(&ap, sizeof(ap));
            ap.user_data = ds->srtp;

            /* Attach ourselves to member transport for DTLS nego. */
            if (pj_sockaddr_has_addr(&ds->rem_addr))
                pj_sockaddr_cp(&ap.rem_addr, &ds->rem_addr);
            else
                pj_sockaddr_init(pj_AF_INET(), &ap.rem_addr, 0, 0);

            if (use_rtcp_mux) {
                /* Using RTP & RTCP multiplexing */
                pj_sockaddr_cp(&ap.rem_rtcp, &ds->rem_addr);
            } else if (pj_sockaddr_has_addr(&ds->rem_rtcp)) {
                pj_sockaddr_cp(&ap.rem_rtcp, &ds->rem_rtcp);
            } else if (pj_sockaddr_has_addr(&ds->rem_addr)) {
                pj_sockaddr_cp(&ap.rem_rtcp, &ds->rem_addr);
                pj_sockaddr_set_port(&ap.rem_rtcp,
                                     pj_sockaddr_get_port(&ap.rem_rtcp) + 1);
            } else {
                pj_sockaddr_init(pj_AF_INET(), &ap.rem_rtcp, 0, 0);
            }

            ap.addr_len = pj_sockaddr_get_len(&ap.rem_addr);
            status = pjmedia_transport_attach2(&ds->srtp->base, &ap);
            if (status != PJ_SUCCESS)
                goto on_return;
#if DTLS_DEBUG
            {
                char addr[PJ_INET6_ADDRSTRLEN];
                char addr2[PJ_INET6_ADDRSTRLEN];
                PJ_LOG(2,(ds->base.name, "Attached transport, "
                          "remote addr=%s remote rtcp=%s",
                          pj_sockaddr_print(&ap.rem_addr, addr,
                          sizeof(addr), 3),
                          pj_sockaddr_print(&ap.rem_rtcp, addr2,
                          sizeof(addr2), 3)));
            }
#endif
            
            status = ssl_handshake(ds);
            if (status != PJ_SUCCESS)
                goto on_return;
        }
    }

on_return:
#if DTLS_DEBUG
    if (status != PJ_SUCCESS) {
        pj_perror(4, ds->base.name, status, "dtls_media_start() failed");
    }
#endif
    return status;
}

static pj_status_t dtls_media_stop(pjmedia_transport *tp)
{
    dtls_srtp *ds = (dtls_srtp *)tp;

#if DTLS_DEBUG
    PJ_LOG(2,(ds->base.name, "dtls_media_stop()"));
#endif

    dtls_media_stop_channel(ds, RTP_CHANNEL);
    dtls_media_stop_channel(ds, RTCP_CHANNEL);

    ds->setup = DTLS_SETUP_UNKNOWN;
    ds->use_ice = PJ_FALSE;
    ds->got_keys = PJ_FALSE;
    ds->rem_fingerprint.slen = 0;
    ds->rem_fprint_status = PJ_EPENDING;

    return PJ_SUCCESS;
}

static void dtls_destroy_channel(dtls_srtp *ds, unsigned idx)
{
    if (ds->clock[idx]) {
        ds->nego_completed[idx] = PJ_TRUE;
        pjmedia_clock_destroy(ds->clock[idx]);
        ds->clock[idx] = NULL;
    }
    ssl_destroy(ds, idx);
}

static void dtls_on_destroy(void *arg) {
    dtls_srtp *ds = (dtls_srtp *)arg;

    if (ds->ossl_lock)
        pj_lock_destroy(ds->ossl_lock);

    pj_pool_safe_release(&ds->pool);
}

static pj_status_t dtls_destroy(pjmedia_transport *tp)
{
    dtls_srtp *ds = (dtls_srtp *)tp;

#if DTLS_DEBUG
    PJ_LOG(2,(ds->base.name, "dtls_destroy()"));
#endif

    ds->is_destroying = PJ_TRUE;

    DTLS_LOCK(ds);

    dtls_destroy_channel(ds, RTP_CHANNEL);
    dtls_destroy_channel(ds, RTCP_CHANNEL);

    DTLS_UNLOCK(ds);

    if (ds->base.grp_lock) {
        pj_grp_lock_dec_ref(ds->base.grp_lock);
    } else {
        dtls_on_destroy(tp);
    }

    return PJ_SUCCESS;
}


/* Get fingerprint of local DTLS-SRTP certificate. */
PJ_DEF(pj_status_t) pjmedia_transport_srtp_dtls_get_fingerprint(
                                pjmedia_transport *tp,
                                const char *hash,
                                char *buf, pj_size_t *len)
{
    PJ_ASSERT_RETURN(dtls_cert, PJ_EINVALIDOP);
    PJ_ASSERT_RETURN(tp && hash && buf && len, PJ_EINVAL);
    PJ_ASSERT_RETURN(pj_ansi_strcmp(hash, "SHA-256")==0 ||
                     pj_ansi_strcmp(hash, "SHA-1")==0, PJ_EINVAL);
    PJ_UNUSED_ARG(tp);

    return ssl_get_fingerprint(dtls_cert,
                               pj_ansi_strcmp(hash, "SHA-256")==0,
                               buf, len);
}


/* Manually start DTLS-SRTP negotiation (without SDP offer/answer) */
PJ_DEF(pj_status_t) pjmedia_transport_srtp_dtls_start_nego(
                                pjmedia_transport *tp,
                                const pjmedia_srtp_dtls_nego_param *param)
{
    transport_srtp *srtp = (transport_srtp*)tp;
    dtls_srtp *ds = NULL;
    unsigned j;
    pjmedia_transport_attach_param ap;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && param, PJ_EINVAL);
    PJ_ASSERT_RETURN(pj_sockaddr_has_addr(&param->rem_addr), PJ_EINVAL);

    /* Find DTLS keying and destroy any other keying. */
    for (j = 0; j < srtp->all_keying_cnt; ++j) {
        if (srtp->all_keying[j]->op == &dtls_op)
            ds = (dtls_srtp*)srtp->all_keying[j];
        else
            pjmedia_transport_close(srtp->all_keying[j]);
    }

    /* DTLS-SRTP is not enabled */
    if (!ds)
        return PJ_ENOTSUP;

    /* Set SRTP keying to DTLS-SRTP only */
    srtp->keying_cnt = 1;
    srtp->keying[0] = &ds->base;
    srtp->keying_pending_cnt = 0;

    /* Apply param to DTLS-SRTP internal states */
    pj_strdup(ds->pool, &ds->rem_fingerprint, &param->rem_fingerprint);
    ds->rem_fprint_status = PJ_EPENDING;
    ds->rem_addr = param->rem_addr;
    ds->rem_rtcp = param->rem_rtcp;
    ds->setup = param->is_role_active? DTLS_SETUP_ACTIVE:DTLS_SETUP_PASSIVE;

    /* Pending start SRTP */
    ds->pending_start = PJ_TRUE;
    srtp->keying_pending_cnt++;

    /* Attach member transport, so we can send/receive DTLS init packets */
    pj_bzero(&ap, sizeof(ap));
    ap.user_data = ds->srtp;
    pj_sockaddr_cp(&ap.rem_addr, &ds->rem_addr);
    pj_sockaddr_cp(&ap.rem_rtcp, &ds->rem_rtcp);
    if (pj_sockaddr_cmp(&ds->rem_addr, &ds->rem_rtcp) == 0)
        ds->srtp->use_rtcp_mux = PJ_TRUE;
    ap.addr_len = pj_sockaddr_get_len(&ap.rem_addr);
    status = pjmedia_transport_attach2(&ds->srtp->base, &ap);
    if (status != PJ_SUCCESS)
        goto on_return;

#if DTLS_DEBUG
    {
        char addr[PJ_INET6_ADDRSTRLEN];
        char addr2[PJ_INET6_ADDRSTRLEN];
        PJ_LOG(2,(ds->base.name, "Attached transport, remote addr=%s "
                                 "remote rtcp=%s",
                  pj_sockaddr_print(&ap.rem_addr, addr, sizeof(addr), 3),
                  pj_sockaddr_print(&ap.rem_addr, addr2, sizeof(addr2), 3)));
    }
#endif

    /* Start DTLS handshake */
    pj_bzero(&srtp->srtp_ctx.rx_policy_neg,
             sizeof(srtp->srtp_ctx.rx_policy_neg));
    pj_bzero(&srtp->srtp_ctx.tx_policy_neg,
             sizeof(srtp->srtp_ctx.tx_policy_neg));
    status = ssl_handshake(ds);
    if (status != PJ_SUCCESS)
        goto on_return;

on_return:
    if (status != PJ_SUCCESS) {
        ssl_destroy(ds, RTP_CHANNEL);
        ssl_destroy(ds, RTCP_CHANNEL);
    }
    return status;
}

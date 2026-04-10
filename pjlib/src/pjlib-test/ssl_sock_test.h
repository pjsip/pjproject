/*
 * Shared definitions for SSL socket tests.
 */
#ifndef __SSL_SOCK_TEST_H__
#define __SSL_SOCK_TEST_H__

#include "test.h"
#include <pjlib.h>

#define CERT_DIR                    "../build/"
#if (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_DARWIN) || \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_APPLE)
#   define CERT_CA_FILE             CERT_DIR "cacert.der"
#else
#   define CERT_CA_FILE             CERT_DIR "cacert.pem"
#endif
#define CERT_FILE                   CERT_DIR "cacert.pem"
#define CERT_PRIVKEY_FILE           CERT_DIR "privkey.pem"
#define CERT_PRIVKEY_PASS           "privkeypass"

#define TEST_LOAD_FROM_FILES        1


#if INCLUDE_SSLSOCK_TEST

/*
 * Load server certificate. Returns PJ_SUCCESS, or PJ_SUCCESS with
 * cert==NULL if backend doesn't support file-based certs (Schannel).
 */
PJ_INLINE(pj_status_t) ssl_test_load_cert(pj_pool_t *pool,
                                                  const char *caller,
                                                  pj_ssl_cert_t **p_cert)
{
#if TEST_LOAD_FROM_FILES
    pj_str_t ca = pj_str((char *)CERT_CA_FILE);
    pj_str_t crt = pj_str((char *)CERT_FILE);
    pj_str_t key = pj_str((char *)CERT_PRIVKEY_FILE);
    pj_str_t pass = pj_str((char *)CERT_PRIVKEY_PASS);
    pj_status_t status;

    status = pj_ssl_cert_load_from_files(pool, &ca, &crt, &key,
                                         &pass, p_cert);
    if (status == PJ_ENOTSUP) {
        PJ_LOG(3, ("", "...%s: cert load not supported, skipping",
                   caller));
        *p_cert = NULL;
        return PJ_SUCCESS;
    }
    if (status != PJ_SUCCESS) {
        app_perror(caller, status);
        return status;
    }
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(caller);
    *p_cert = NULL;
    return PJ_SUCCESS;
#endif
}

/*
 * Setup SSL server socket: create, load cert, start accept on 127.0.0.1
 * with random port. Returns listen address in *listen_addr.
 */
PJ_INLINE(pj_status_t) ssl_test_create_server(
                                    pj_pool_t *pool,
                                    pj_ssl_sock_param *param,
                                    const char *caller,
                                    pj_ssl_sock_t **p_ssock,
                                    pj_sockaddr *listen_addr)
{
    pj_ssl_cert_t *cert = NULL;
    pj_ssl_sock_info info;
    pj_status_t status;

    status = pj_ssl_sock_create(pool, param, p_ssock);
    if (status != PJ_SUCCESS) {
        app_perror(caller, status);
        return status;
    }

    status = ssl_test_load_cert(pool, caller, &cert);
    if (status != PJ_SUCCESS)
        return status;

    if (cert) {
        status = pj_ssl_sock_set_certificate(*p_ssock, pool, cert);
        if (status != PJ_SUCCESS) {
            app_perror(caller, status);
            return status;
        }
    }

    status = pj_ssl_sock_start_accept(*p_ssock, pool, listen_addr,
                                      pj_sockaddr_get_len(listen_addr));
    if (status != PJ_SUCCESS) {
        app_perror(caller, status);
        return status;
    }

    pj_ssl_sock_get_info(*p_ssock, &info);
    *listen_addr = info.local_addr;
    return PJ_SUCCESS;
}

#endif /* INCLUDE_SSLSOCK_TEST */

#endif /* __SSL_SOCK_TEST_H__ */

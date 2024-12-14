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
#ifndef __PJSIP_AUTH_SIP_AUTH_H__
#define __PJSIP_AUTH_SIP_AUTH_H__

/**
 * @file pjsip_auth.h
 * @brief SIP Authorization Module.
 */

#include <pjsip/sip_config.h>
#include <pjsip/sip_auth_msg.h>

PJ_BEGIN_DECL

/**
 * @addtogroup PJSIP_AUTH
 * @ingroup PJSIP_CORE
 * @brief Client and server side authentication framework.
 */

/**
 * @defgroup PJSIP_AUTH_API Authentication API's
 * @ingroup PJSIP_AUTH
 * @brief Structures and functions to perform authentication.
 * @{
 */

/**
 * Length of digest MD5 string.
 * \deprecated Use #pjsip_auth_algorithm::digest_str_length instead.
 */
#define PJSIP_MD5STRLEN         32

/**
 * Length of digest SHA256 string.
 * \deprecated Use #pjsip_auth_algorithm::digest_str_length instead.
 */
#define PJSIP_SHA256STRLEN      64

/**
 * The length of the buffer needed to contain the largest
 * supported algorithm's digest.
 */
#define PJSIP_AUTH_MAX_DIGEST_BUFFER_LENGTH      64

/**
 * Digest Algorithm Types.
 * \warning These entries must remain in order with
 * no gaps and with _NOT_SET = 0 and _COUNT as the last entry.
 *
 * The MD5, SHA-256, and SHA-512/256 algorithms are described
 * in RFC 7616 and RFC 8760.
 * The AKA algorithms are described in RFC 3310 and RFC 4169
 * and 3GPP TS 33.203.
 */
typedef enum pjsip_auth_algorithm_type
{
    PJSIP_AUTH_ALGORITHM_NOT_SET = 0,  /**< Algorithm not set.          */
    PJSIP_AUTH_ALGORITHM_MD5,          /**< MD5 algorithm.              */
    PJSIP_AUTH_ALGORITHM_SHA256,       /**< SHA-256 algorithm.          */
    PJSIP_AUTH_ALGORITHM_SHA512_256,   /**< SHA-512/256 algorithm       */
    PJSIP_AUTH_ALGORITHM_AKAV1_MD5,    /**< AKA v1 with MD5 algorithm.  */
    PJSIP_AUTH_ALGORITHM_AKAV2_MD5,    /**< AKA v2 with MD5 algorithm.  */
    PJSIP_AUTH_ALGORITHM_COUNT,        /**< Number of algorithms.       */
} pjsip_auth_algorithm_type;


/**
 * Authentication Digest Algorithm
 *
 * This structure describes a digest algorithm used in
 * SIP authentication.
 *
 */
typedef struct pjsip_auth_algorithm
{
    pjsip_auth_algorithm_type algorithm_type; /**< Digest algorithm type     */
    pj_str_t iana_name;                       /**< IANA/RFC name used in
                                                   SIP headers               */
    const char *openssl_name;                 /**< The name used by OpenSSL's
                                                   EVP_get_digestbyname()    */
    unsigned digest_length;                   /**< Length of the raw digest
                                                   in bytes                  */
    unsigned digest_str_length;               /**< Length of the digest HEX
                                                   representation            */
} pjsip_auth_algorithm;


/** Type of data in the credential information in #pjsip_cred_info. */
typedef enum pjsip_cred_data_type
{
    PJSIP_CRED_DATA_PLAIN_PASSWD=0, /**< Plain text password.           */
    PJSIP_CRED_DATA_DIGEST      =1, /**< Hashed digest.                 */

    PJSIP_CRED_DATA_EXT_AKA     =16 /**< Extended AKA info is available */

} pjsip_cred_data_type;

#define PJSIP_CRED_DATA_PASSWD_MASK         0x000F
#define PJSIP_CRED_DATA_EXT_MASK            0x00F0

#define PJSIP_CRED_DATA_IS_AKA(cred) (((cred)->data_type & PJSIP_CRED_DATA_EXT_MASK) == PJSIP_CRED_DATA_EXT_AKA)
#define PJSIP_CRED_DATA_IS_PASSWD(cred) (((cred)->data_type & PJSIP_CRED_DATA_PASSWD_MASK) == PJSIP_CRED_DATA_PLAIN_PASSWD)
#define PJSIP_CRED_DATA_IS_DIGEST(cred) (((cred)->data_type & PJSIP_CRED_DATA_PASSWD_MASK) == PJSIP_CRED_DATA_DIGEST)

/** Authentication's quality of protection (qop) type. */
typedef enum pjsip_auth_qop_type
{
    PJSIP_AUTH_QOP_NONE,            /**< No quality of protection. */
    PJSIP_AUTH_QOP_AUTH,            /**< Authentication. */
    PJSIP_AUTH_QOP_AUTH_INT,        /**< Authentication with integrity protection. */
    PJSIP_AUTH_QOP_UNKNOWN          /**< Unknown protection. */
} pjsip_auth_qop_type;


/**
 * Type of callback function to create authentication response.
 * Application can specify this callback in \a cb field of the credential info
 * (#pjsip_cred_info) and specifying PJSIP_CRED_DATA_DIGEST_CALLBACK as 
 * \a data_type. When this function is called, most of the fields in the 
 * \a auth authentication response will have been filled by the framework. 
 * Application normally should just need to calculate the response digest 
 * of the authentication response.
 *
 * @param pool      Pool to allocate memory from if application needs to.
 * @param chal      The authentication challenge sent by server in 401
 *                  or 401 response, in either Proxy-Authenticate or
 *                  WWW-Authenticate header.
 * @param cred      The credential that has been selected by the framework
 *                  to authenticate against the challenge.
 * @param auth      The authentication response which application needs to
 *                  calculate the response digest.
 *
 * @return          Application may return non-PJ_SUCCESS to abort the
 *                  authentication process. When this happens, the 
 *                  framework will return failure to the original function
 *                  that requested authentication.
 */
typedef pj_status_t (*pjsip_cred_cb)(pj_pool_t *pool,
                                     const pjsip_digest_challenge *chal,
                                     const pjsip_cred_info *cred,
                                     const pj_str_t *method,
                                     pjsip_digest_credential *auth);


/** 
 * This structure describes credential information. 
 * A credential information is a static, persistent information that identifies
 * credentials required to authorize to a specific realm.
 *
 * Note that since PJSIP 0.7.0.1, it is possible to make a credential that is
 * valid for any realms, by setting the realm to star/wildcard character,
 * i.e. realm = pj_str("*");.
 *
 * You should always fill this structure with zeros using PJ_POOL_ZALLOC_T()
 * or pj_bzero() before setting any fields.
 */
struct pjsip_cred_info
{
    pj_str_t    realm;          /**< Realm. Use "*" to make a credential that
                                     can be used to authenticate against any
                                     challenges.                            */
    pj_str_t    scheme;         /**< Scheme (e.g. "digest").                */
    pj_str_t    username;       /**< User name.                             */
    int         data_type;      /**< Type of data \ref pjsip_cred_data_type */
    pj_str_t    data;           /**< The data, which can be a plaintext 
                                     password or a hashed digest.           */
    /**
     * If the data_type is #PJSIP_CRED_DATA_DIGEST and the digest algorithm
     * used is not MD5 (the default), then this field MUST be set to the
     * appropriate digest algorithm type.
     */
    pjsip_auth_algorithm_type algorithm_type;    /**< Digest algorithm type */

    /** Extended data */
    union {
        /** Digest AKA credential information. Note that when AKA credential
         *  is being used, the \a data field of this #pjsip_cred_info is
         *  not used, but it still must be initialized to an empty string.
         * Please see \ref PJSIP_AUTH_AKA_API for more information.
         */
        struct {
            pj_str_t      k;    /**< Permanent subscriber key.          */
            pj_str_t      op;   /**< Operator variant key.              */
            pj_str_t      amf;  /**< Authentication Management Field    */
            pjsip_cred_cb cb;   /**< Callback to create AKA digest.     */
        } aka;

    } ext;
};

/**
 * This structure describes cached value of previously sent Authorization
 * or Proxy-Authorization header. The authentication framework keeps a list
 * of this structure and will resend the same header to the same server
 * as long as the method, uri, and nonce stays the same.
 */
typedef struct pjsip_cached_auth_hdr
{
    /** Standard list member */
    PJ_DECL_LIST_MEMBER(struct pjsip_cached_auth_hdr);

    pjsip_method             method;    /**< To quickly see the method. */
    pjsip_authorization_hdr *hdr;       /**< The cached header.         */

} pjsip_cached_auth_hdr;


/**
 * This structure describes authentication information for the specified
 * realm. Each instance of this structure describes authentication "session"
 * between this endpoint and remote server. This "session" information is
 * usefull to keep information that persists for more than one challenge,
 * such as nonce-count and cnonce value.
 *
 * Other than that, this structure also keeps the last authorization headers
 * that have been sent in the cache list.
 */
typedef struct pjsip_cached_auth
{
    /** Standard list member */
    PJ_DECL_LIST_MEMBER(struct pjsip_cached_auth);

    pj_pool_t                   *pool;      /**< Pool for cached auth       */
    pj_str_t                     realm;     /**< Realm.                     */
    pj_bool_t                    is_proxy;  /**< Server type (401/407)      */
    pjsip_auth_qop_type          qop_value; /**< qop required by server.    */
    unsigned                     stale_cnt; /**< Number of stale retry.     */
#if PJSIP_AUTH_QOP_SUPPORT
    pj_uint32_t                  nc;        /**< Nonce count.               */
    pj_str_t                     cnonce;    /**< Cnonce value.              */
#endif
    pjsip_www_authenticate_hdr  *last_chal; /**< Last challenge seen.       */
#if PJSIP_AUTH_HEADER_CACHING
    pjsip_cached_auth_hdr        cached_hdr;/**< List of cached header for
                                                 each method.               */
#endif
    pjsip_auth_algorithm_type    challenge_algorithm_type; /**< Challenge
                                                                algorithm   */
} pjsip_cached_auth;


/**
 * This structure describes client authentication session preference.
 * The preference can be set by calling #pjsip_auth_clt_set_prefs().
 */
typedef struct pjsip_auth_clt_pref
{
    /**
     * If this flag is set, the authentication client framework will
     * send an empty Authorization header in each initial request.
     * Default is no.
     */
    pj_bool_t   initial_auth;

    /**
     * Specify the algorithm to use when empty Authorization header 
     * is to be sent for each initial request (see above)
     */
    pj_str_t    algorithm;

} pjsip_auth_clt_pref;


/**
 * Get a pjsip_auth_algorithm structure by type.
 *
 * @param algorithm_type  The algorithm type
 *
 * @return                A pointer to a pjsip_auth_algorithm structure
 *                        or NULL if not found.
 */
PJ_DECL(const pjsip_auth_algorithm *) pjsip_auth_get_algorithm_by_type(
        pjsip_auth_algorithm_type algorithm_type);


/**
 * Get a pjsip_auth_algorithm by IANA name.
 *
 * @param iana_name  The IANA name (MD5, SHA-256, SHA-512-256)
 *
 * @return                A pointer to a pjsip_auth_algorithm structure
 *                        or NULL if not found.
 */
PJ_DECL(const pjsip_auth_algorithm *) pjsip_auth_get_algorithm_by_iana_name(
        const pj_str_t *iana_name);


/**
 * Check if a digest algorithm is supported.
 * Algorithms that require support from OpenSSL will be checked
 * at runtime to determine if they are actually available in
 * the current version of OpenSSL.
 *
 * @param algorithm_type  The algorithm type
 *
 * @return                PJ_TRUE if the algorithm is supported,
 *                        PJ_FALSE otherwise.
 */
PJ_DECL(pj_bool_t) pjsip_auth_is_algorithm_supported(
        pjsip_auth_algorithm_type algorithm_type);


/**
 * Duplicate a client authentication preference setting.
 *
 * @param pool      The memory pool.
 * @param dst       Destination client authentication preference.
 * @param src       Source client authentication preference.
 */
PJ_DECL(void) pjsip_auth_clt_pref_dup(pj_pool_t *pool,
                                      pjsip_auth_clt_pref *dst,
                                      const pjsip_auth_clt_pref *src);


/**
 * This structure describes client authentication sessions. It keeps
 * all the information needed to authorize the client against all downstream 
 * servers.
 */
typedef struct pjsip_auth_clt_sess
{
    pj_pool_t           *pool;          /**< Pool to use.                   */
    pjsip_endpoint      *endpt;         /**< Endpoint where this belongs.   */
    pjsip_auth_clt_pref  pref;          /**< Preference/options.            */
    unsigned             cred_cnt;      /**< Number of credentials.         */
    pjsip_cred_info     *cred_info;     /**< Array of credential information*/
    pjsip_cached_auth    cached_auth;   /**< Cached authorization info.     */

} pjsip_auth_clt_sess;


/**
 * Duplicate a credential info.
 *
 * @param pool      The memory pool.
 * @param dst       Destination credential.
 * @param src       Source credential.
 */
PJ_DECL(void) pjsip_cred_info_dup(pj_pool_t *pool,
                                  pjsip_cred_info *dst,
                                  const pjsip_cred_info *src);

/**
 * Compare two credential infos.
 *
 * @param cred1     The credential info to compare.
 * @param cred2     The credential info to compare.
 *
 * @return          0 if both credentials are equal.
 */
PJ_DECL(int) pjsip_cred_info_cmp(const pjsip_cred_info *cred1,
                                 const pjsip_cred_info *cred2);


/**
 * Type of function to lookup credential for the specified name.
 *
 * \note If pjsip_cred_info::data_type is set to PJSIP_CRED_DATA_DIGEST and
 * pjsip_cred_info::algorithm_type is left unset (0), algorithm_type will
 * default to #PJSIP_AUTH_ALGORITHM_MD5.
 *
 * @param pool          Pool to initialize the credential info.
 * @param realm         Realm to find the account.
 * @param acc_name      Account name to look for.
 * @param cred_info     The structure to put the credential when it's found.
 *
 * @return              The function MUST return PJ_SUCCESS when it found
 *                      a correct credential for the specified account and
 *                      realm. Otherwise it may return PJSIP_EAUTHACCNOTFOUND
 *                      or PJSIP_EAUTHACCDISABLED.
 */
typedef pj_status_t pjsip_auth_lookup_cred( pj_pool_t *pool,
                                            const pj_str_t *realm,
                                            const pj_str_t *acc_name,
                                            pjsip_cred_info *cred_info );


/**
 * This structure describes input param for credential lookup.
 */
typedef struct pjsip_auth_lookup_cred_param
{
    pj_str_t realm;                     /**< Realm to find the account.      */
    pj_str_t acc_name;                  /**< Account name to look for.       */
    pjsip_rx_data *rdata;               /**< Incoming request to be
                                             authenticated.                  */
    pjsip_authorization_hdr *auth_hdr;  /**< Authorization header to be
                                             authenticated.                  */
} pjsip_auth_lookup_cred_param;


/**
 * Type of function to lookup credential for the specified name.
 *
 * \note If pjsip_cred_info::data_type is set to PJSIP_CRED_DATA_DIGEST and
 * pjsip_cred_info::algorithm_type is left unset (0), algorithm_type will
 * default to #PJSIP_AUTH_ALGORITHM_MD5.
 *
 * @param pool          Pool to initialize the credential info.
 * @param param         The input param for credential lookup.
 * @param cred_info     The structure to put the credential when it's found.
 *
 * @return              The function MUST return PJ_SUCCESS when it found
 *                      a correct credential for the specified account and
 *                      realm. Otherwise it may return PJSIP_EAUTHACCNOTFOUND
 *                      or PJSIP_EAUTHACCDISABLED.
 */
typedef pj_status_t pjsip_auth_lookup_cred2(
                                pj_pool_t *pool,
                                const pjsip_auth_lookup_cred_param *param,
                                pjsip_cred_info *cred_info );


/** Flag to specify that server is a proxy. */
#define PJSIP_AUTH_SRV_IS_PROXY     1

/**
 * This structure describes server authentication information.
 */
typedef struct pjsip_auth_srv
{
    pj_str_t                 realm;     /**< Realm to serve.                */
    pj_bool_t                is_proxy;  /**< Will issue 407 instead of 401  */
    pjsip_auth_lookup_cred  *lookup;    /**< Lookup function.               */
    pjsip_auth_lookup_cred2 *lookup2;   /**< Lookup function with additional
                                             info in its input param.       */
} pjsip_auth_srv;


/**
 * Initialize client authentication session data structure, and set the 
 * session to use pool for its subsequent memory allocation. The argument 
 * options should be set to zero for this PJSIP version.
 *
 * @param sess          The client authentication session.
 * @param endpt         Endpoint where this session belongs.
 * @param pool          Pool to use.
 * @param options       Must be zero.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_init( pjsip_auth_clt_sess *sess,
                                          pjsip_endpoint *endpt,
                                          pj_pool_t *pool, 
                                          unsigned options);


/**
 * Deinitialize client authentication session data structure.
 *
 * @param sess          The client authentication session.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_deinit( pjsip_auth_clt_sess *sess);


/**
 * Clone client initialization session. 
 *
 * @param pool          Pool to use.
 * @param sess          Structure to put the duplicated session.
 * @param rhs           The client session to be cloned.
 *
 * @return              PJ_SUCCESS on success;
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_clone( pj_pool_t *pool,
                                           pjsip_auth_clt_sess *sess,
                                           const pjsip_auth_clt_sess *rhs);

/**
 * Set the credentials to be used during the session. This will duplicate 
 * the specified credentials using client authentication's pool.
 *
 * \note If pjsip_cred_info::data_type is set to PJSIP_CRED_DATA_DIGEST and
 * pjsip_cred_info::algorithm_type is left unset (0), algorithm_type will
 * default to #PJSIP_AUTH_ALGORITHM_MD5.
 *
 * @param sess          The client authentication session.
 * @param cred_cnt      Number of credentials.
 * @param c             Array of credentials.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_set_credentials( pjsip_auth_clt_sess *sess,
                                                     int cred_cnt,
                                                     const pjsip_cred_info *c);


/**
 * Set the preference for the client authentication session.
 *
 * @param sess          The client authentication session.
 * @param p             Preference.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_set_prefs(pjsip_auth_clt_sess *sess,
                                              const pjsip_auth_clt_pref *p);


/**
 * Get the preference for the client authentication session.
 *
 * @param sess          The client authentication session.
 * @param p             Pointer to receive the preference.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_get_prefs(pjsip_auth_clt_sess *sess,
                                              pjsip_auth_clt_pref *p);

/**
 * Initialize new request message with authorization headers.
 * This function will put Authorization/Proxy-Authorization headers to the
 * outgoing request message. If caching is enabled (PJSIP_AUTH_HEADER_CACHING)
 * and the session has previously sent Authorization/Proxy-Authorization header
 * with the same method, then the same Authorization/Proxy-Authorization header
 * will be resent from the cache only if qop is not present. If the stack is 
 * configured to automatically generate next Authorization/Proxy-Authorization
 * headers (PJSIP_AUTH_AUTO_SEND_NEXT flag), then new Authorization/Proxy-
 * Authorization headers are calculated and generated when they are not present
 * in the case or if authorization session has qop.
 *
 * If both PJSIP_AUTH_HEADER_CACHING flag and PJSIP_AUTH_AUTO_SEND_NEXT flag
 * are not set, this function will do nothing. The stack then will only send
 * Authorization/Proxy-Authorization to respond 401/407 response.
 *
 * @param sess          The client authentication session.
 * @param tdata         The request message to be initialized.
 *
 * @return              PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_init_req( pjsip_auth_clt_sess *sess,
                                              pjsip_tx_data *tdata );


/**
 * Call this function when a transaction failed with 401 or 407 response.
 * This function will reinitialize the original request message with the
 * authentication challenge found in the response message, and add the
 * new authorization header in the authorization cache.
 *
 * Note that upon return the reference counter of the new transmit data
 * will be set to 1.
 *
 * @param sess          The client authentication session.
 * @param rdata         The response message containing 401/407 status.
 * @param old_request   The original request message, which will be re-
 *                      created with authorization info.
 * @param new_request   Pointer to receive new request message which
 *                      will contain all required authorization headers.
 *
 * @return              PJ_SUCCESS if new request can be successfully
 *                      created to respond all the authentication
 *                      challenges.
 */
PJ_DECL(pj_status_t) pjsip_auth_clt_reinit_req( pjsip_auth_clt_sess *sess,
                                                const pjsip_rx_data *rdata,
                                                pjsip_tx_data *old_request,
                                                pjsip_tx_data **new_request );

/**
 * Initialize server authorization session data structure to serve the 
 * specified realm and to use lookup_func function to look for the credential 
 * info. 
 *
 * @param pool          Pool used to initialize the authentication server.
 * @param auth_srv      The authentication server structure.
 * @param realm         Realm to be served by the server.
 * @param lookup        Account lookup function.
 * @param options       Options, bitmask of:
 *                      - PJSIP_AUTH_SRV_IS_PROXY: to specify that the server
 *                        will authorize clients as a proxy server (instead of
 *                        as UAS), which means that Proxy-Authenticate will 
 *                        be used instead of WWW-Authenticate.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_srv_init( pj_pool_t *pool,
                                          pjsip_auth_srv *auth_srv,
                                          const pj_str_t *realm,
                                          pjsip_auth_lookup_cred *lookup,
                                          unsigned options );


/**
 * This structure describes initialization settings of server authorization
 * session.
 */
typedef struct pjsip_auth_srv_init_param
{
    /**
     * Realm to be served by the server.
     */
    const pj_str_t              *realm;

    /**
     * Account lookup function.
     */
    pjsip_auth_lookup_cred2     *lookup2;

    /**
     * Options, bitmask of:
     * - PJSIP_AUTH_SRV_IS_PROXY: to specify that the server will authorize
     *   clients as a proxy server (instead of as UAS), which means that
     *   Proxy-Authenticate will be used instead of WWW-Authenticate.
     */
    unsigned                     options;

} pjsip_auth_srv_init_param;


/**
 * Initialize server authorization session data structure to serve the 
 * specified realm and to use lookup_func function to look for the credential
 * info. 
 *
 * @param pool          Pool used to initialize the authentication server.
 * @param auth_srv      The authentication server structure.
 * @param param         The initialization param.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_srv_init2(
                                    pj_pool_t *pool,
                                    pjsip_auth_srv *auth_srv,
                                    const pjsip_auth_srv_init_param *param);

/**
 * Request the authorization server framework to verify the authorization 
 * information in the specified request in rdata.
 *
 * @param auth_srv      The server authentication structure.
 * @param rdata         Incoming request to be authenticated.
 * @param status_code   When not null, it will be filled with suitable 
 *                      status code to be sent to the client.
 *
 * @return              PJ_SUCCESS if request is successfully authenticated.
 *                      Otherwise the function may return one of the
 *                      following error codes:
 *                      - PJSIP_EAUTHNOAUTH
 *                      - PJSIP_EINVALIDAUTHSCHEME
 *                      - PJSIP_EAUTHACCNOTFOUND
 *                      - PJSIP_EAUTHACCDISABLED
 *                      - PJSIP_EAUTHINVALIDREALM
 *                      - PJSIP_EAUTHINVALIDDIGEST
 */
PJ_DECL(pj_status_t) pjsip_auth_srv_verify( pjsip_auth_srv *auth_srv,
                                            pjsip_rx_data *rdata,
                                            int *status_code );


/**
 * Add authentication challenge headers to the outgoing response in tdata. 
 * Application may specify its customized nonce and opaque for the challenge, 
 * or can leave the value to NULL to make the function fills them in with 
 * random characters.  The digest algorithm defaults to MD5.  If you need
 * to specify a different algorithm, use #pjsip_auth_srv_challenge2.
 *
 * @param auth_srv      The server authentication structure.
 * @param qop           Optional qop value.
 * @param nonce         Optional nonce value.
 * @param opaque        Optional opaque value.
 * @param stale         Stale indication.
 * @param tdata         The outgoing response message. The response must have
 *                      401 or 407 response code.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_srv_challenge( pjsip_auth_srv *auth_srv,
                                               const pj_str_t *qop,
                                               const pj_str_t *nonce,
                                               const pj_str_t *opaque,
                                               pj_bool_t stale,
                                               pjsip_tx_data *tdata);

/**
 * Add authentication challenge headers to the outgoing response in tdata.
 * Application may specify its customized nonce and opaque for the challenge,
 * or can leave the value to NULL to make the function fills them in with
 * random characters.
 * Application must specify the algorithm to use.
 *
 * @param auth_srv       The server authentication structure.
 * @param qop            Optional qop value.
 * @param nonce          Optional nonce value.
 * @param opaque         Optional opaque value.
 * @param stale          Stale indication.
 * @param tdata          The outgoing response message. The response must have
 *                       401 or 407 response code.
 * @param algorithm_type One of the #pjsip_auth_algorithm_type values.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_srv_challenge2(pjsip_auth_srv *auth_srv,
                                               const pj_str_t *qop,
                                               const pj_str_t *nonce,
                                               const pj_str_t *opaque,
                                               pj_bool_t stale,
                                               pjsip_tx_data *tdata,
                                               const pjsip_auth_algorithm_type algorithm_type);

/**
 * Helper function to create a digest out of the specified
 * parameters.
 *
 * \deprecated Use #pjsip_auth_create_digest2 with
 * algorithm_type = #PJSIP_AUTH_ALGORITHM_MD5.
 *
 * \warning Because of ambiguities in the API, this function
 * should only be used for backward compatibility with the
 * MD5 digest algorithm. New code should use
 * #pjsip_auth_create_digest2
 *
 * pjsip_cred_info::data_type must be #PJSIP_CRED_DATA_PLAIN_PASSWD
 * or #PJSIP_CRED_DATA_DIGEST.
 *
 * \note If pjsip_cred_info::data_type is set to PJSIP_CRED_DATA_DIGEST and
 * pjsip_cred_info::algorithm_type is left unset (0), algorithm_type will
 * default to #PJSIP_AUTH_ALGORITHM_MD5.
 *
 * @param result        String to store the response digest. This string
 *                      must have been preallocated by caller with the 
 *                      buffer at least PJSIP_MD5STRLEN (32 bytes) in size.
 * @param nonce         Optional nonce.
 * @param nc            Nonce count.
 * @param cnonce        Optional cnonce.
 * @param qop           Optional qop.
 * @param uri           URI.
 * @param realm         Realm.
 * @param cred_info     Credential info.
 * @param method        SIP method.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_create_digest(pj_str_t *result,
                                              const pj_str_t *nonce,
                                              const pj_str_t *nc,
                                              const pj_str_t *cnonce,
                                              const pj_str_t *qop,
                                              const pj_str_t *uri,
                                              const pj_str_t *realm,
                                              const pjsip_cred_info *cred_info,
                                              const pj_str_t *method);

/**
 * Helper function to create SHA-256 digest out of the specified 
 * parameters.
 *
 * \deprecated Use #pjsip_auth_create_digest2 with
 * algorithm_type = #PJSIP_AUTH_ALGORITHM_SHA256.
 *
 * \warning Because of ambiguities in the API, this function
 * should only be used for backward compatibility with the
 * SHA256 digest algorithm. New code should use
 * #pjsip_auth_create_digest2
 *
 * pjsip_cred_info::data_type must be #PJSIP_CRED_DATA_PLAIN_PASSWD
 * or #PJSIP_CRED_DATA_DIGEST.
 *
 * \note If pjsip_cred_info::data_type is set to PJSIP_CRED_DATA_DIGEST and
 * pjsip_cred_info::algorithm_type is left unset (0), algorithm_type will
 * default to #PJSIP_AUTH_ALGORITHM_SHA256.
 *
 * @param result        String to store the response digest. This string
 *                      must have been preallocated by caller with the 
 *                      buffer at least PJSIP_SHA256STRLEN (64 bytes) in size.
 * @param nonce         Optional nonce.
 * @param nc            Nonce count.
 * @param cnonce        Optional cnonce.
 * @param qop           Optional qop.
 * @param uri           URI.
 * @param realm         Realm.
 * @param cred_info     Credential info.
 * @param method        SIP method.
 *
 * @return              PJ_SUCCESS on success. 
 */
PJ_DECL(pj_status_t) pjsip_auth_create_digestSHA256(pj_str_t* result,
                                            const pj_str_t* nonce,
                                            const pj_str_t* nc,
                                            const pj_str_t* cnonce,
                                            const pj_str_t* qop,
                                            const pj_str_t* uri,
                                            const pj_str_t* realm,
                                            const pjsip_cred_info* cred_info,
                                            const pj_str_t* method);

/**
 * Helper function to create a digest out of the specified
 * parameters.
 *
 * pjsip_cred_info::data_type must be #PJSIP_CRED_DATA_PLAIN_PASSWD
 * or #PJSIP_CRED_DATA_DIGEST.
 *
 * If pjsip_cred_info::data_type is #PJSIP_CRED_DATA_PLAIN_PASSWORD,
 * pjsip_cred_info::username + ":" + realm + ":" + pjsip_cred_info::data
 * will be hashed using the algorithm_type specified by the last
 * parameter passed to this function to create the "ha1" hash.
 *
 * If pjsip_cred_info::data_type is #PJSIP_CRED_DATA_DIGEST,
 * pjsip_cred_info::data must contain the value of
 * username + ":" + realm + ":" + password
 * pre-hashed with the algorithm specifed by pjsip_cred_info::algorithm_type
 * and will be used as the "ha1" hash directly.  In this case
 * pjsip_cred_info::algorithm_type MUST match the algorithm_type
 * passed as the last parameter to this function.
 *
 * @param result         String to store the response digest. This string
 *                       must have been preallocated by the caller with the
 *                       buffer at least as large as the digest_str_length
 *                       member of the appropriate pjsip_auth_algorithm.
 * @param nonce          Optional nonce.
 * @param nc             Nonce count.
 * @param cnonce         Optional cnonce.
 * @param qop            Optional qop.
 * @param uri            URI.
 * @param realm          Realm.
 * @param cred_info      Credential info.
 * @param method         SIP method.
 * @param algorithm_type The hash algorithm to use.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_auth_create_digest2(pj_str_t *result,
                                 const pj_str_t *nonce,
                                 const pj_str_t *nc,
                                 const pj_str_t *cnonce,
                                 const pj_str_t *qop,
                                 const pj_str_t *uri,
                                 const pj_str_t *realm,
                                 const pjsip_cred_info *cred_info,
                                 const pj_str_t *method,
                                 const pjsip_auth_algorithm_type algorithm_type);

/**
 * @}
 */



PJ_END_DECL


#endif  /* __PJSIP_AUTH_SIP_AUTH_H__ */


/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
 * @defgroup PJSIP_AUTH_API Authorization API's
 * @ingroup PJSIP_AUTH
 * @{
 */

 /** Type of data in the credential information. */
typedef enum pjsip_cred_data_type
{
    PJSIP_CRED_DATA_PLAIN_PASSWD,   /**< Plain text password.	*/
    PJSIP_CRED_DATA_DIGEST,	    /**< Hashed digest.		*/
} pjsip_cred_data_type;

/** Authentication's quality of protection (qop) type. */
typedef enum pjsip_auth_qop_type
{
    PJSIP_AUTH_QOP_NONE,	    /**< No quality of protection. */
    PJSIP_AUTH_QOP_AUTH,	    /**< Authentication. */
    PJSIP_AUTH_QOP_AUTH_INT,	    /**< Authentication with integrity protection. */
    PJSIP_AUTH_QOP_UNKNOWN,	    /**< Unknown protection. */
} pjsip_auth_qop_type;


/** 
 * This structure describes credential information. 
 * A credential information is a static, persistent information that identifies
 * username and password required to authorize to a specific realm.
 */
struct pjsip_cred_info
{
    pj_str_t    realm;		/**< Realm.	    */
    pj_str_t	scheme;		/**< Scheme.	    */
    pj_str_t	username;	/**< User name.	    */
    int		data_type;	/**< Type of data.  */
    pj_str_t	data;		/**< The data, which can be a plaintext 
				     password or a hashed digest. */
};

/**
 * This structure describes cached value of previously sent Authorization
 * or Proxy-Authorization header. The authentication framework keeps a list
 * of this structure and will resend the same header to the same server
 * as long as the method, uri, and nonce stays the same.
 */
typedef struct pjsip_cached_auth_hdr
{
    PJ_DECL_LIST_MEMBER(struct pjsip_cached_auth_hdr);

    pjsip_method	     method;
    pjsip_authorization_hdr *hdr;

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
typedef struct pjsip_auth_session
{
    PJ_DECL_LIST_MEMBER(struct pjsip_auth_session);

    pj_str_t			 realm;
    pj_bool_t			 is_proxy;
    pjsip_auth_qop_type		 qop_value;
#if PJSIP_AUTH_QOP_SUPPORT
    pj_uint32_t			 nc;
    pj_str_t			 cnonce;
#endif
#if PJSIP_AUTH_AUTO_SEND_NEXT
    pjsip_www_authenticate_hdr	*last_chal;
#endif
#if PJSIP_AUTH_HEADER_CACHING
    pjsip_cached_auth_hdr	 cached_hdr;
#endif

} pjsip_auth_session;


/**
 * Create authorization header for the specified credential.
 * Application calls this function to create Authorization or Proxy-Authorization
 * header after receiving WWW-Authenticate or Proxy-Authenticate challenge
 * (normally in 401/407 response).
 * If authorization session argument is specified, this function will update
 * the session with the updated information if required (e.g. to update
 * nonce-count when qop is "auth" or "auth-int"). This function will also
 * save the authorization header in the session's cached header list.
 *
 * @param req_pool	Pool to allocate new header for the request.
 * @param hdr		The WWW-Authenticate or Proxy-Authenticate found in 
 *			the response.
 * @param uri		The URI for which authorization is targeted to.
 * @param cred_info	The credential to be used for authentication.
 * @param method	The method.
 * @param sess_pool	Session pool to update session or to allocate message
 *			in the cache. May be NULL if auth_sess is NULL.
 * @param auth_sess	If not NULL, this specifies the specific authentication
 *			session to be used or updated.
 *
 * @return		The Authorization header, which can be typecasted to 
 *			Proxy-Authorization.
 */
PJ_DECL(pjsip_authorization_hdr*) pjsip_auth_respond( 
					 pj_pool_t *req_pool,
					 const pjsip_www_authenticate_hdr *hdr,
					 const pjsip_uri *uri,
					 const pjsip_cred_info *cred_info,
					 const pjsip_method *method,
					 pj_pool_t *sess_pool,
					 pjsip_auth_session *auth_sess);

/**
 * Verify digest in the authorization request.
 *
 * @param hdr		The incoming Authorization/Proxy-Authorization header.
 * @param method	The method.
 * @param password	The plaintext password to verify.
 *
 * @return		Non-zero if authorization succeed.
 */
PJ_DECL(pj_bool_t) pjsip_auth_verify(	const pjsip_authorization_hdr *hdr,
					const pj_str_t *method,
					const pjsip_cred_info *cred_info );


/**
 * This function can be used to find credential information which matches
 * the specified realm.
 *
 * @param count		Number of credentials in the parameter.
 * @param cred		The array of credentials.
 * @param realm		Realm to search.
 * @param scheme	Authentication scheme.
 *
 * @return		The credential which matches the specified realm.
 */
PJ_DECL(const pjsip_cred_info*) pjsip_auth_find_cred( unsigned count,
						      const pjsip_cred_info cred[],
						      const pj_str_t *realm,
						      const pj_str_t *scheme );


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
 * @param sess_pool	Session level pool, where memory will be allocated from
 *			for data that persists across requests (e.g. caching).
 * @param tdata		The request message to be initialized.
 * @param sess_list	List of authorization sessions that have been recorded.
 * @param cred_count	Number of credentials.
 * @param cred_info	Array of credentials.
 *
 * @return		Zero if successfull.
 */
PJ_DECL(pj_status_t) pjsip_auth_init_req( pj_pool_t *sess_pool,
					  pjsip_tx_data *tdata,
					  pjsip_auth_session *sess_list,
					  int cred_count, 
					  const pjsip_cred_info cred_info[]);

/**
 * Call this function when a transaction failed with 401 or 407 response.
 * This function will reinitialize the original request message with the
 * authentication challenge found in the response message, and add the
 * new authorization header in the authorization cache.
 *
 * Note that upon return the reference counter of the transmit data
 * will be incremented.
 *
 * @param endpt		Endpoint.
 * @param pool		The pool to allocate memory for new cred_info.
 * @param cached_list	Cached authorization headers.
 * @param cred_count	Number of credentials.
 * @param cred_info	Array of credentials to use.
 * @param tdata		The original request message, which normally can be
 *			retrieved from tsx->last_tx.
 * @param rdata		The response message containing 401/407 status.
 *
 * @return		New transmit data buffer, or NULL if the dialog
 *			can not respond to the authorization challenge.
 */
PJ_DECL(pjsip_tx_data*) 
pjsip_auth_reinit_req( pjsip_endpoint *endpt,
		       pj_pool_t *ses_pool,
		       pjsip_auth_session *sess_list,
		       int cred_count, const pjsip_cred_info cred_info[],
		       pjsip_tx_data *tdata, const pjsip_rx_data *rdata);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_AUTH_SIP_AUTH_H__ */


/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#ifndef __PJ_STUN_AUTH_H__
#define __PJ_STUN_AUTH_H__

/**
 * @file stun_auth.h
 * @brief STUN authentication.
 */

#include <pjlib-util/stun_msg.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJLIB_UTIL_STUN_AUTH STUN Authentication
 * @ingroup PJLIB_UTIL_STUN
 * @{
 */

/**
 * Type of authentication data in the credential.
 */
typedef enum pj_stun_auth_cred_type
{
    /**
     * The credential data contains a static credential to be matched 
     * against the credential in the message. A static credential can be 
     * used as both client side or server side authentication.
     */
    PJ_STUN_AUTH_CRED_STATIC,

    /**
     * The credential data contains callbacks to be called to verify the 
     * credential in the message. A dynamic credential is suitable when 
     * performing server side authentication where server does not know
     * in advance the identity of the user requesting authentication.
     */
    PJ_STUN_AUTH_CRED_DYNAMIC

} pj_stun_auth_cred_type;


/**
 * This structure contains the descriptions needed to perform server side
 * authentication. Depending on the \a type set in the structure, application
 * may specify a static username/password combination, or to have callbacks
 * called by the function to authenticate the credential dynamically.
 */
typedef struct pj_stun_auth_cred
{
    /**
     * The type of authentication information in this structure.
     */
    pj_stun_auth_cred_type	type;

    /**
     * This union contains the authentication data.
     */
    union 
    {
	/**
	 * This structure contains static data for performing authentication.
	 * A non-empty realm indicates whether short term or long term
	 * credential is used.
	 */
	struct
	{
	    /** 
	     * If not-empty, it indicates that this is a long term credential.
	     */
	    pj_str_t	  realm;

	    /** 
	     * The username of the credential.
	     */
	    pj_str_t	  username;

	    /**
	     * Data type to indicate the type of password in the \a data field.
	     * Value zero indicates that the data contains a plaintext
	     * password.
	     */
	    int		  data_type;

	    /** 
	     * The data, which depends depends on the value of \a data_type
	     * field. When \a data_type is zero, this field will contain the
	     * plaintext password.
	     */
	    pj_str_t	  data;

	    /** 
	     * Optional NONCE.
	     */
	    pj_str_t	  nonce;

	} static_cred;

	/**
	 * This structure contains callback to be called by the framework
	 * to authenticate the incoming message.
	 */
	struct
	{
	    /**
	     * User data which will be passed back to callback functions.
	     */
	    void *user_data;

	    /**
	     * This callback is called by pj_stun_verify_credential() when
	     * server needs to challenge the request with 401 response.
	     *
	     * @param user_data	The user data as specified in the credential.
	     * @param pool	Pool to allocate memory.
	     * @param realm	On return, the function should fill in with
	     *			realm if application wants to use long term
	     *			credential. Otherwise application should set
	     *			empty string for the realm.
	     * @param nonce	On return, if application wants to use long
	     *			term credential, it MUST fill in the nonce
	     *			with some value. Otherwise  if short term 
	     *			credential is wanted, it MAY set this value.
	     *			If short term credential is wanted and the
	     *			application doesn't want to include NONCE,
	     *			then it must set this to empty string.
	     *
	     * @return		The callback should return PJ_SUCCESS, or
	     *			otherwise response message will not be 
	     *			created.
	     */
	    pj_status_t (*get_auth)(void *user_data,
				    pj_pool_t *pool,
				    pj_str_t *realm,
				    pj_str_t *nonce);

	    /**
	     * Get the password for the specified username. This function 
	     * is also used to check whether the username is valid.
	     *
	     * @param user_data	The user data as specified in the credential.
	     * @param realm	The realm as specified in the message.
	     * @param username	The username as specified in the message.
	     * @param pool	Pool to allocate memory when necessary.
	     * @param data_type On return, application should fill up this
	     *			argument with the type of data (which should
	     *			be zero if data is a plaintext password).
	     * @param data	On return, application should fill up this
	     *			argument with the password according to
	     *			data_type.
	     *
	     * @return		The callback should return PJ_SUCCESS if
	     *			username has been successfully verified
	     *			and password was obtained. If non-PJ_SUCCESS
	     *			is returned, it is assumed that the
	     *			username is not valid.
	     */
	    pj_status_t	(*get_password)(void *user_data, 
				        const pj_str_t *realm,
				        const pj_str_t *username,
					pj_pool_t *pool,
					int *data_type,
					pj_str_t *data);

	    /**
	     * This callback will be called to verify that the NONCE given
	     * in the message can be accepted. If this callback returns
	     * PJ_FALSE, 438 (Stale Nonce) response will be created.
	     *
	     * @param user_data	The user data as specified in the credential.
	     * @param realm	The realm as specified in the message.
	     * @param username	The username as specified in the message.
	     * @param nonce	The nonce to be verified.
	     *
	     * @return		The callback MUST return non-zero if the 
	     *			NONCE can be accepted.
	     */
	    pj_bool_t	(*verify_nonce)(void *user_data,
					const pj_str_t *realm,
					const pj_str_t *username,
					const pj_str_t *nonce);

	} dyn_cred;

    } data;

} pj_stun_auth_cred;


/**
 * Duplicate authentication credential.
 *
 * @param pool		Pool to be used to allocate memory.
 * @param dst		Destination credential.
 * @param src		Source credential.
 */
PJ_DECL(void) pj_stun_auth_cred_dup(pj_pool_t *pool,
				      pj_stun_auth_cred *dst,
				      const pj_stun_auth_cred *src);


/**
 * Verify credential in the STUN message. Note that before calling this
 * function, application must have checked that the message contains
 * PJ_STUN_ATTR_MESSAGE_INTEGRITY attribute by calling pj_stun_msg_find_attr()
 * function, because this function will reject the message with 401 error
 * if it doesn't contain PJ_STUN_ATTR_MESSAGE_INTEGRITY attribute.
 *
 * @param pkt		The original packet which has been parsed into
 *			the message. This packet MUST NOT have been modified
 *			after the parsing.
 * @param pkt_len	The length of the packet.
 * @param msg		The parsed message to be verified.
 * @param cred		Pointer to credential to be used to authenticate
 *			the message.
 * @param pool		If response is to be created, then memory will
 *			be allocated from this pool.
 * @param p_response	Optional pointer to receive the response message
 *			then the credential in the request fails to
 *			authenticate.
 *
 * @return		PJ_SUCCESS if credential is verified successfully.
 *			If the verification fails and \a p_response is not
 *			NULL, an appropriate response will be returned in
 *			\a p_response.
 */
PJ_DECL(pj_status_t) pj_stun_verify_credential(const pj_uint8_t *pkt,
					       unsigned pkt_len,
					       const pj_stun_msg *msg,
					       pj_stun_auth_cred *cred,
					       pj_pool_t *pool,
					       pj_stun_msg **p_response);




/**
 * @}
 */


/* Calculate HMAC-SHA1 key for long term credential, by getting
 * MD5 digest of username, realm, and password. 
 */
void pj_stun_calc_md5_key(pj_uint8_t digest[16],
			  const pj_str_t *realm,
			  const pj_str_t *username,
			  const pj_str_t *passwd);


PJ_END_DECL


#endif	/* __PJ_STUN_AUTH_H__ */


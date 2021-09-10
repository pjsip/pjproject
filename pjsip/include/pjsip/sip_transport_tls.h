/* $Id$ */
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
#ifndef __PJSIP_TRANSPORT_TLS_H__
#define __PJSIP_TRANSPORT_TLS_H__

/**
 * @file sip_transport_tls.h
 * @brief SIP TLS Transport.
 */

#include <pjsip/sip_transport.h>
#include <pj/pool.h>
#include <pj/ssl_sock.h>
#include <pj/string.h>
#include <pj/sock_qos.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSPORT_TLS TLS Transport
 * @ingroup PJSIP_TRANSPORT
 * @brief API to create and register TLS transport.
 * @{
 * The functions below are used to create TLS transport and register 
 * the transport to the framework.
 */

/**
 * The default SSL method to be used by PJSIP.
 * Default is PJSIP_TLSV1_METHOD
 */
#ifndef PJSIP_SSL_DEFAULT_METHOD
#   define PJSIP_SSL_DEFAULT_METHOD	PJSIP_TLSV1_METHOD
#endif


/** SSL protocol method constants. */
typedef enum pjsip_ssl_method
{
    PJSIP_SSL_UNSPECIFIED_METHOD = 0,	/**< Default protocol method.	*/    
    PJSIP_SSLV2_METHOD		 = 20,	/**< Use SSLv2 method.		*/
    PJSIP_SSLV3_METHOD		 = 30,	/**< Use SSLv3 method.		*/
    PJSIP_TLSV1_METHOD		 = 31,	/**< Use TLSv1 method.		*/
    PJSIP_TLSV1_1_METHOD	 = 32,	/**< Use TLSv1_1 method.	*/
    PJSIP_TLSV1_2_METHOD	 = 33,	/**< Use TLSv1_2 method.	*/
    PJSIP_TLSV1_3_METHOD	 = 34,	/**< Use TLSv1_3 method.	*/
    PJSIP_SSLV23_METHOD		 = 23,	/**< Use SSLv23 method.		*/
} pjsip_ssl_method;

/**
 * The default enabled SSL proto to be used.
 * Default is all protocol above TLSv1 (TLSv1 & TLS v1.1 & TLS v1.2).
 */
#ifndef PJSIP_SSL_DEFAULT_PROTO
#   define PJSIP_SSL_DEFAULT_PROTO  (PJ_SSL_SOCK_PROTO_TLS1 | \
				     PJ_SSL_SOCK_PROTO_TLS1_1 | \
				     PJ_SSL_SOCK_PROTO_TLS1_2)
#endif


/**
 * This structure describe the parameter passed from #on_accept_fail_cb().
 */
typedef struct pjsip_tls_on_accept_fail_param {
    /**
     * Local address of the fail accept operation of the TLS listener.
     */
    const pj_sockaddr_t *local_addr;

    /**
     * Remote address of the fail accept operation of the TLS listener.
     */
    const pj_sockaddr_t *remote_addr;

    /**
     * Error status of the fail accept operation of the TLS listener.
     */
    pj_status_t status;

    /**
     * Last error code returned by native SSL backend. Note that this may be
     * zero, if the failure is not SSL related (e.g: accept rejection).
     */
    pj_status_t last_native_err;

} pjsip_tls_on_accept_fail_param;


/**
 * TLS transport settings.
 */
typedef struct pjsip_tls_setting
{
    /**
     * Certificate of Authority (CA) list file.
     */
    pj_str_t	ca_list_file;

    /**
     * Certificate of Authority (CA) list directory path.
     */
    pj_str_t	ca_list_path;

    /**
     * Public endpoint certificate file, which will be used as client-
     * side  certificate for outgoing TLS connection, and server-side
     * certificate for incoming TLS connection.
     */
    pj_str_t	cert_file;

    /**
     * Optional private key of the endpoint certificate to be used.
     */
    pj_str_t	privkey_file;

    /**
     * Certificate of Authority (CA) buffer. If ca_list_file, ca_list_path,
     * cert_file or privkey_file are set, this setting will be ignored.
     */
    pj_ssl_cert_buffer ca_buf;

    /**
     * Public endpoint certificate buffer, which will be used as client-
     * side  certificate for outgoing TLS connection, and server-side
     * certificate for incoming TLS connection. If ca_list_file, ca_list_path,
     * cert_file or privkey_file are set, this setting will be ignored.
     */
    pj_ssl_cert_buffer cert_buf;

    /**
     * Optional private key buffer of the endpoint certificate to be used. 
     * If ca_list_file, ca_list_path, cert_file or privkey_file are set, 
     * this setting will be ignored.
     */
    pj_ssl_cert_buffer privkey_buf;

    /**
     * Password to open private key.
     */
    pj_str_t	password;

    /**
     * TLS protocol method from #pjsip_ssl_method. In the future, this field
     * might be deprecated in favor of <b>proto</b> field. For now, this field 
     * is only applicable only when <b>proto</b> field is set to zero.
     *
     * Default is PJSIP_SSL_UNSPECIFIED_METHOD (0), which in turn will
     * use PJSIP_SSL_DEFAULT_METHOD, which default value is PJSIP_TLSV1_METHOD.
     */
    pjsip_ssl_method	method;

    /**
     * TLS protocol type from #pj_ssl_sock_proto. Use this field to enable 
     * specific protocol type. Use bitwise OR operation to combine the protocol 
     * type.
     *
     * Default is PJSIP_SSL_DEFAULT_PROTO.
     */
    pj_uint32_t	proto;

    /**
     * Number of ciphers contained in the specified cipher preference. 
     * If this is set to zero, then default cipher list of the backend 
     * will be used.
     *
     * Default: 0 (zero).
     */
    unsigned ciphers_num;

    /**
     * Ciphers and order preference. The #pj_ssl_cipher_get_availables()
     * can be used to check the available ciphers supported by backend.
     */
    pj_ssl_cipher *ciphers;

    /**
     * Number of curves contained in the specified curve preference.
     * If this is set to zero, then default curve list of the backend
     * will be used.
     *
     * Default: 0 (zero).
     */
    unsigned curves_num;

    /**
     * Curves and order preference. The #pj_ssl_curve_get_availables()
     * can be used to check the available curves supported by backend.
     */
    pj_ssl_curve *curves;

    /**
     * The supported signature algorithms. Set the sigalgs string
     * using this form:
     * "<DIGEST>+<ALGORITHM>:<DIGEST>+<ALGORITHM>"
     * Digests are: "RSA", "DSA" or "ECDSA"
     * Algorithms are: "MD5", "SHA1", "SHA224", "SHA256", "SHA384", "SHA512"
     * Example: "ECDSA+SHA256:RSA+SHA256"
     */
    pj_str_t	sigalgs;

    /**
     * Reseed random number generator.
     * For type #PJ_SSL_ENTROPY_FILE, parameter \a entropy_path
     * must be set to a file.
     * For type #PJ_SSL_ENTROPY_EGD, parameter \a entropy_path
     * must be set to a socket.
     *
     * Default value is PJ_SSL_ENTROPY_NONE.
    */
    pj_ssl_entropy_t	entropy_type;

    /**
     * When using a file/socket for entropy #PJ_SSL_ENTROPY_EGD or
     * #PJ_SSL_ENTROPY_FILE, \a entropy_path must contain the path
     * to entropy socket/file.
     *
     * Default value is an empty string.
     */
    pj_str_t		entropy_path;

    /**
     * Specifies TLS transport behavior on the server TLS certificate 
     * verification result:
     * - If \a verify_server is disabled (set to PJ_FALSE), TLS transport 
     *   will just notify the application via #pjsip_tp_state_callback with
     *   state PJSIP_TP_STATE_CONNECTED regardless TLS verification result.
     * - If \a verify_server is enabled (set to PJ_TRUE), TLS transport 
     *   will be shutdown and application will be notified with state
     *   PJSIP_TP_STATE_DISCONNECTED whenever there is any TLS verification
     *   error, otherwise PJSIP_TP_STATE_CONNECTED will be notified.
     *
     * In any cases, application can inspect #pjsip_tls_state_info in the
     * callback to see the verification detail.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t	verify_server;

    /**
     * Specifies TLS transport behavior on the client TLS certificate 
     * verification result:
     * - If \a verify_client is disabled (set to PJ_FALSE), TLS transport 
     *   will just notify the application via #pjsip_tp_state_callback with
     *   state PJSIP_TP_STATE_CONNECTED regardless TLS verification result.
     * - If \a verify_client is enabled (set to PJ_TRUE), TLS transport 
     *   will be shutdown and application will be notified with state
     *   PJSIP_TP_STATE_DISCONNECTED whenever there is any TLS verification
     *   error, otherwise PJSIP_TP_STATE_CONNECTED will be notified.
     *
     * In any cases, application can inspect #pjsip_tls_state_info in the
     * callback to see the verification detail.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t	verify_client;

    /**
     * When acting as server (incoming TLS connections), reject inocming
     * connection if client doesn't supply a TLS certificate.
     *
     * This setting corresponds to SSL_VERIFY_FAIL_IF_NO_PEER_CERT flag.
     * Default value is PJ_FALSE.
     */
    pj_bool_t	require_client_cert;

    /**
     * TLS negotiation timeout to be applied for both outgoing and
     * incoming connection. If both sec and msec member is set to zero,
     * the SSL negotiation doesn't have a timeout.
     */
    pj_time_val	timeout;

    /**
     * Should SO_REUSEADDR be used for the listener socket.
     * Default value is PJSIP_TLS_TRANSPORT_REUSEADDR.
     */
    pj_bool_t reuse_addr;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qos_param fields since this is more portable.
     *
     * Default value is PJ_QOS_TYPE_BEST_EFFORT.
     */
    pj_qos_type qos_type;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qos_type field and may not be
     * supported on all platforms.
     *
     * By default all settings in this structure are disabled.
     */
    pj_qos_params qos_params;

    /**
     * Specify if the transport should ignore any errors when setting the QoS
     * traffic type/parameters.
     *
     * Default: PJ_TRUE
     */
    pj_bool_t qos_ignore_error;

    /**
     * Specify options to be set on the transport. 
     *
     * By default there is no options.
     * 
     */
    pj_sockopt_params sockopt_params;

    /**
     * Specify if the transport should ignore any errors when setting the 
     * sockopt parameters.
     *
     * Default: PJ_TRUE
     * 
     */
    pj_bool_t sockopt_ignore_error;

    /**
     * Callback to be called when a accept operation of the TLS listener fails.
     *
     * @param param         The parameter to the callback.
     */
    void(*on_accept_fail_cb)(const pjsip_tls_on_accept_fail_param *param);

} pjsip_tls_setting;


/**
 * This structure defines TLS transport extended info in <tt>ext_info</tt>
 * field of #pjsip_transport_state_info for the transport state notification
 * callback #pjsip_tp_state_callback.
 */
typedef struct pjsip_tls_state_info
{
    /**
     * SSL socket info.
     */
    pj_ssl_sock_info	*ssl_sock_info;

} pjsip_tls_state_info;


/**
 * Initialize TLS setting with default values.
 *
 * @param tls_opt   The TLS setting to be initialized.
 */
PJ_INLINE(void) pjsip_tls_setting_default(pjsip_tls_setting *tls_opt)
{
    pj_memset(tls_opt, 0, sizeof(*tls_opt));
    tls_opt->reuse_addr = PJSIP_TLS_TRANSPORT_REUSEADDR;
    tls_opt->qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    tls_opt->qos_ignore_error = PJ_TRUE;
    tls_opt->sockopt_ignore_error = PJ_TRUE;
    tls_opt->proto = PJSIP_SSL_DEFAULT_PROTO;
}


/**
 * Copy TLS setting.
 *
 * @param pool	    The pool to duplicate strings etc.
 * @param dst	    Destination structure.
 * @param src	    Source structure.
 */
PJ_INLINE(void) pjsip_tls_setting_copy(pj_pool_t *pool,
				       pjsip_tls_setting *dst,
				       const pjsip_tls_setting *src)
{
    pj_memcpy(dst, src, sizeof(*dst));
    pj_strdup_with_null(pool, &dst->ca_list_file, &src->ca_list_file);
    pj_strdup_with_null(pool, &dst->ca_list_path, &src->ca_list_path);
    pj_strdup_with_null(pool, &dst->cert_file, &src->cert_file);
    pj_strdup_with_null(pool, &dst->privkey_file, &src->privkey_file);
    pj_strdup_with_null(pool, &dst->password, &src->password);
    pj_strdup_with_null(pool, &dst->sigalgs, &src->sigalgs);
    pj_strdup_with_null(pool, &dst->entropy_path, &src->entropy_path);

    pj_strdup(pool, &dst->ca_buf, &src->ca_buf);
    pj_strdup(pool, &dst->cert_buf, &src->cert_buf);
    pj_strdup(pool, &dst->privkey_buf, &src->privkey_buf);

    if (src->ciphers_num) {
	unsigned i;
	dst->ciphers = (pj_ssl_cipher*) pj_pool_calloc(pool, src->ciphers_num,
						       sizeof(pj_ssl_cipher));
	for (i=0; i<src->ciphers_num; ++i)
	    dst->ciphers[i] = src->ciphers[i];
    }

    if (src->curves_num) {
	unsigned i;
	dst->curves = (pj_ssl_curve*) pj_pool_calloc(pool, src->curves_num,
						     sizeof(pj_ssl_curve));
	for (i=0; i<src->curves_num; ++i)
	    dst->curves[i] = src->curves[i];
    }
}


/**
 * Wipe out certificates and keys in the TLS setting buffer.
 *
 * @param opt	    TLS setting.
 */
PJ_DECL(void) pjsip_tls_setting_wipe_keys(pjsip_tls_setting *opt);


/**
 * Register support for SIP TLS transport by creating TLS listener on
 * the specified address and port. This function will create an
 * instance of SIP TLS transport factory and register it to the
 * transport manager.
 *
 * See also #pjsip_tls_transport_start2() which supports IPv6.
 *
 * @param endpt		The SIP endpoint.
 * @param opt		Optional TLS settings.
 * @param local		Optional local address to bind, or specify the
 *			address to bind the server socket to. Both IP 
 *			interface address and port fields are optional.
 *			If IP interface address is not specified, socket
 *			will be bound to PJ_INADDR_ANY. If port is not
 *			specified, socket will be bound to any port
 *			selected by the operating system.
 * @param a_name	Optional published address, which is the address to be
 *			advertised as the address of this SIP transport. 
 *			If this argument is NULL, then the bound address
 *			will be used as the published address.
 * @param async_cnt	Number of simultaneous asynchronous accept()
 *			operations to be supported. It is recommended that
 *			the number here corresponds to the number of
 *			processors in the system (or the number of SIP
 *			worker threads).
 * @param p_factory	Optional pointer to receive the instance of the
 *			SIP TLS transport factory just created.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tls_transport_start(pjsip_endpoint *endpt,
					       const pjsip_tls_setting *opt,
					       const pj_sockaddr_in *local,
					       const pjsip_host_port *a_name,
					       unsigned async_cnt,
					       pjsip_tpfactory **p_factory);

/**
 * Variant of #pjsip_tls_transport_start() that supports IPv6. To instantiate
 * IPv6 listener, set the address family of the "local" argument to IPv6
 * (the host and port part may be left unspecified if not desired, i.e. by
 * filling them with zeroes).
 *
 * @param endpt		The SIP endpoint.
 * @param opt		Optional TLS settings.
 * @param local		Optional local address to bind, or specify the
 *			address to bind the server socket to. Both IP
 *			interface address and port fields are optional.
 *			If IP interface address is not specified, socket
 *			will be bound to any address. If port is not
 *			specified, socket will be bound to any port
 *			selected by the operating system.
 * @param a_name	Optional published address, which is the address to be
 *			advertised as the address of this SIP transport.
 *			If this argument is NULL, then the bound address
 *			will be used as the published address.
 * @param async_cnt	Number of simultaneous asynchronous accept()
 *			operations to be supported. It is recommended that
 *			the number here corresponds to the number of
 *			processors in the system (or the number of SIP
 *			worker threads).
 * @param p_factory	Optional pointer to receive the instance of the
 *			SIP TLS transport factory just created.
 *
 * @return		PJ_SUCCESS when the transport has been successfully
 *			started and registered to transport manager, or
 *			the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tls_transport_start2(pjsip_endpoint *endpt,
						const pjsip_tls_setting *opt,
						const pj_sockaddr *local,
						const pjsip_host_port *a_name,
						unsigned async_cnt,
						pjsip_tpfactory **p_factory);

/**
 * Start the TLS listener, if the listener is not started yet. This is useful
 * to start the listener manually, if listener was not started when
 * PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER is set to 0.
 *
 * @param factory	The SIP TLS transport factory.
 *
 * @param local		The address where the listener should be bound to.
 *			Both IP interface address and port fields are optional.
 *			If IP interface address is not specified, socket
 *			will be bound to PJ_INADDR_ANY. If port is not
 *			specified, socket will be bound to any port
 *			selected by the operating system.
 *
 * @param a_name	The published address for the listener.
 *			If this argument is NULL, then the bound address will
 *			be used as the published address.
 *
 * @return		PJ_SUCCESS when the listener has been successfully
 *			started.
 */
PJ_DECL(pj_status_t) pjsip_tls_transport_lis_start(pjsip_tpfactory *factory,
						const pj_sockaddr *local,
						const pjsip_host_port *a_name);


/**
 * Restart the TLS listener. This will close the listener socket and recreate
 * the socket based on the config used when starting the transport.
 *
 * @param factory	The SIP TLS transport factory.
 *
 * @param local		The address where the listener should be bound to.
 *			Both IP interface address and port fields are optional.
 *			If IP interface address is not specified, socket
 *			will be bound to PJ_INADDR_ANY. If port is not
 *			specified, socket will be bound to any port
 *			selected by the operating system.
 *
 * @param a_name	The published address for the listener.
 *			If this argument is NULL, then the bound address will
 *			be used as the published address.
 *
 * @return		PJ_SUCCESS when the listener has been successfully
 *			restarted.
 *
 */
PJ_DECL(pj_status_t) pjsip_tls_transport_restart(pjsip_tpfactory *factory,
						const pj_sockaddr *local,
						const pjsip_host_port *a_name);

PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSIP_TRANSPORT_TLS_H__ */

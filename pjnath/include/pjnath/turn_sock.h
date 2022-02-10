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
#ifndef __PJNATH_TURN_SOCK_H__
#define __PJNATH_TURN_SOCK_H__

/**
 * @file turn_sock.h
 * @brief TURN relay using UDP client as transport protocol
 */
#include <pjnath/turn_session.h>
#include <pj/sock_qos.h>
#include <pj/ssl_sock.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
@addtogroup PJNATH_TURN_SOCK
@{

This is a ready to use object for relaying application data via a TURN server,
by managing all the operations in \ref turn_op_sec.

\section turnsock_using_sec Using TURN transport

This object provides a thin wrapper to the \ref PJNATH_TURN_SESSION, hence the
API is very much the same (apart from the obvious difference in the names).
Please see \ref PJNATH_TURN_SESSION for the documentation on how to use the
session.

\section turnsock_samples_sec Samples

The \ref turn_client_sample is a sample application to use the
\ref PJNATH_TURN_SOCK.

Also see <b>\ref samples_page</b> for other samples.

 */


/** 
 * Opaque declaration for TURN client.
 */
typedef struct pj_turn_sock pj_turn_sock;

/**
 * This structure contains callbacks that will be called by the TURN
 * transport.
 */
typedef struct pj_turn_sock_cb
{
    /**
     * Notification when incoming data has been received from the remote
     * peer via the TURN server. The data reported in this callback will
     * be the exact data as sent by the peer (e.g. the TURN encapsulation
     * such as Data Indication or ChannelData will be removed before this
     * function is called).
     *
     * @param turn_sock	    The TURN client transport.
     * @param data	    The data as received from the peer.    
     * @param data_len	    Length of the data.
     * @param peer_addr	    The peer address.
     * @param addr_len	    The length of the peer address.
     */
    void (*on_rx_data)(pj_turn_sock *turn_sock,
		       void *pkt,
		       unsigned pkt_len,
		       const pj_sockaddr_t *peer_addr,
		       unsigned addr_len);

    /**
     * Notifification when asynchronous send operation has completed.
     *
     * @param turn_sock	    The TURN transport.
     * @param sent	    If value is positive non-zero it indicates the
     *			    number of data sent. When the value is negative,
     *			    it contains the error code which can be retrieved
     *			    by negating the value (i.e. status=-sent).
     *
     * @return		    Application should normally return PJ_TRUE to let
     *			    the TURN transport continue its operation. However
     *			    it must return PJ_FALSE if it has destroyed the
     *			    TURN transport in this callback.
     */
    pj_bool_t (*on_data_sent)(pj_turn_sock *sock,
			      pj_ssize_t sent);

    /**
     * Notification when TURN session state has changed. Application should
     * implement this callback to monitor the progress of the TURN session.
     *
     * @param turn_sock	    The TURN client transport.
     * @param old_state	    Previous state.
     * @param new_state	    Current state.
     */
    void (*on_state)(pj_turn_sock *turn_sock, 
		     pj_turn_state_t old_state,
		     pj_turn_state_t new_state);

    /**
     * Notification when TURN client received a ConnectionAttempt Indication
     * from the TURN server, which indicates that peer initiates a TCP
     * connection to allocated slot in the TURN server. Application should
     * implement this callback if it uses RFC 6062 (TURN TCP allocations),
     * otherwise TURN client will automatically accept it.
     *
     * If application accepts the peer connection attempt (i.e: by returning
     * PJ_SUCCESS or not implementing this callback), the TURN socket will
     * initiate a new connection to the TURN server and send ConnectionBind
     * request, and eventually will notify application via
     * on_connection_status callback, if implemented.
     *
     * @param turn_sock	    The TURN client transport.
     * @param conn_id	    The connection ID assigned by TURN server.
     * @param peer_addr	    Peer address that tried to connect to the
     *			    TURN server.
     * @param addr_len	    Length of the peer address.
     *
     * @return		    The callback must return PJ_SUCCESS to accept
     *			    the connection attempt.
     */
    pj_status_t (*on_connection_attempt)(pj_turn_sock *turn_sock,
					 pj_uint32_t conn_id,
					 const pj_sockaddr_t *peer_addr,
					 unsigned addr_len);

    /**
     * Notification for initiated TCP data connection to peer (RFC 6062),
     * for example after peer connection attempt is accepted.
     *
     * @param turn_sock	    The TURN client transport.
     * @param status	    The status code.
     * @param conn_id	    The connection ID.
     * @param peer_addr	    Peer address.
     * @param addr_len	    Length of the peer address.
     */
    void (*on_connection_status)(pj_turn_sock *turn_sock,
				 pj_status_t status,
				 pj_uint32_t conn_id,
				 const pj_sockaddr_t *peer_addr,
				 unsigned addr_len);

} pj_turn_sock_cb;


/**
 * The default enabled SSL proto to be used.
 * Default is all protocol above TLSv1 (TLSv1 & TLS v1.1 & TLS v1.2).
 */
#ifndef PJ_TURN_TLS_DEFAULT_PROTO
#   define PJ_TURN_TLS_DEFAULT_PROTO  (PJ_SSL_SOCK_PROTO_TLS1 | \
				       PJ_SSL_SOCK_PROTO_TLS1_1 | \
				       PJ_SSL_SOCK_PROTO_TLS1_2)
#endif

/**
 * TLS transport settings.
 */
typedef struct pj_turn_sock_tls_cfg
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
     * side  certificate for outgoing TLS connection.
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
     * The ssl socket parameter.
     * These fields are used by TURN TLS:
     * - proto
     * - ciphers_num
     * - ciphers
     * - curves_num
     * - curves
     * - sigalgs
     * - entropy_type
     * - entropy_path
     * - timeout
     * - sockopt_params
     * - sockopt_ignore_error
     */
    pj_ssl_sock_param ssock_param;

} pj_turn_sock_tls_cfg;

/**
 * Initialize TLS setting with default values.
 *
 * @param tls_cfg   The TLS setting to be initialized.
 */
 PJ_DECL(void) pj_turn_sock_tls_cfg_default(pj_turn_sock_tls_cfg *tls_cfg);

/**
 * Duplicate TLS setting.
 *
 * @param pool	    The pool to duplicate strings etc.
 * @param dst	    Destination structure.
 * @param src	    Source structure.
 */
 PJ_DECL(void) pj_turn_sock_tls_cfg_dup(pj_pool_t *pool,
					pj_turn_sock_tls_cfg *dst,
					const pj_turn_sock_tls_cfg *src);

/**
 * Wipe out certificates and keys in the TLS setting.
 *
 * @param tls_cfg   The TLS setting.
 */
PJ_DECL(void) pj_turn_sock_tls_cfg_wipe_keys(pj_turn_sock_tls_cfg *tls_cfg);


/**
 * This structure describes options that can be specified when creating
 * the TURN socket. Application should call #pj_turn_sock_cfg_default()
 * to initialize this structure with its default values before using it.
 */
typedef struct pj_turn_sock_cfg
{
    /**
     * The group lock to be used by the STUN socket. If NULL, the STUN socket
     * will create one internally.
     *
     * Default: NULL
     */
    pj_grp_lock_t *grp_lock;

    /**
     * Packet buffer size.
     *
     * Default value is PJ_TURN_MAX_PKT_LEN.
     */
    unsigned max_pkt_size;

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
     * By default all settings in this structure are not set.
     */
    pj_qos_params qos_params;

    /**
     * Specify if STUN socket should ignore any errors when setting the QoS
     * traffic type/parameters.
     *
     * Default: PJ_TRUE
     */
    pj_bool_t qos_ignore_error;

    /**
     * Specify the interface where the socket should be bound to. If the
     * address is zero, socket will be bound to INADDR_ANY. If the address
     * is non-zero, socket will be bound to this address only. If the port is
     * set to zero, the socket will bind at any port (chosen by the OS).
     */
    pj_sockaddr bound_addr;

    /**
     * Specify the port range for TURN socket binding, relative to the start
     * port number specified in \a bound_addr. Note that this setting is only
     * applicable when the start port number is non zero.
     *
     * Default value is zero.
     */
    pj_uint16_t	port_range;

    /**
     * Specify target value for socket receive buffer size. It will be
     * applied using setsockopt(). When it fails to set the specified size,
     * it will try with lower value until the highest possible has been
     * successfully set.
     *
     * Default: 0 (OS default)
     */
    unsigned so_rcvbuf_size;

    /**
     * Specify target value for socket send buffer size. It will be
     * applied using setsockopt(). When it fails to set the specified size,
     * it will try with lower value until the highest possible has been
     * successfully set.
     *
     * Default: 0 (OS default)
     */
    unsigned so_sndbuf_size;

    /**
     * This specifies TLS settings for TLS transport. It is only be used
     * when this TLS is used to connect to the TURN server.
     */
    pj_turn_sock_tls_cfg tls_cfg;

} pj_turn_sock_cfg;


/**
 * Initialize pj_turn_sock_cfg structure with default values.
 */
PJ_DECL(void) pj_turn_sock_cfg_default(pj_turn_sock_cfg *cfg);


/**
 * Create a TURN transport instance with the specified address family and
 * connection type. Once TURN transport instance is created, application
 * must call pj_turn_sock_alloc() to allocate a relay address in the TURN
 * server.
 *
 * @param cfg		The STUN configuration which contains among other
 *			things the ioqueue and timer heap instance for
 *			the operation of this transport.
 * @param af		Address family of the client connection. Currently
 *			pj_AF_INET() and pj_AF_INET6() are supported.
 * @param conn_type	Connection type to the TURN server. Both TCP and
 *			UDP are supported.
 * @param cb		Callback to receive events from the TURN transport.
 * @param setting	Optional settings to be specified to the transport.
 *			If this parameter is NULL, default values will be
 *			used.
 * @param user_data	Arbitrary application data to be associated with
 *			this transport.
 * @param p_turn_sock	Pointer to receive the created instance of the
 *			TURN transport.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_sock_create(pj_stun_config *cfg,
					 int af,
					 pj_turn_tp_type conn_type,
					 const pj_turn_sock_cb *cb,
					 const pj_turn_sock_cfg *setting,
					 void *user_data,
					 pj_turn_sock **p_turn_sock);

/**
 * Destroy the TURN transport instance. This will gracefully close the
 * connection between the client and the TURN server. Although this
 * function will return immediately, the TURN socket deletion may continue
 * in the background and the application may still get state changes
 * notifications from this transport.
 *
 * @param turn_sock	The TURN transport instance.
 */
PJ_DECL(void) pj_turn_sock_destroy(pj_turn_sock *turn_sock);


/**
 * Associate a user data with this TURN transport. The user data may then
 * be retrieved later with #pj_turn_sock_get_user_data().
 *
 * @param turn_sock	The TURN transport instance.
 * @param user_data	Arbitrary data.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_sock_set_user_data(pj_turn_sock *turn_sock,
					        void *user_data);

/**
 * Retrieve the previously assigned user data associated with this TURN
 * transport.
 *
 * @param turn_sock	The TURN transport instance.
 *
 * @return		The user/application data.
 */
PJ_DECL(void*) pj_turn_sock_get_user_data(pj_turn_sock *turn_sock);


/**
 * Get the group lock for this TURN transport.
 *
 * @param turn_sock	The TURN transport instance.
 *
 * @return	        The group lock.
 */
PJ_DECL(pj_grp_lock_t *) pj_turn_sock_get_grp_lock(pj_turn_sock *turn_sock);


/**
 * Get the TURN transport info. The transport info contains, among other
 * things, the allocated relay address.
 *
 * @param turn_sock	The TURN transport instance.
 * @param info		Pointer to be filled with TURN transport info.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_sock_get_info(pj_turn_sock *turn_sock,
					   pj_turn_session_info *info);

/**
 * Acquire the internal mutex of the TURN transport. Application may need
 * to call this function to synchronize access to other objects alongside 
 * the TURN transport, to avoid deadlock.
 *
 * @param turn_sock	The TURN transport instance.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_sock_lock(pj_turn_sock *turn_sock);


/**
 * Release the internal mutex previously held with pj_turn_sock_lock().
 *
 * @param turn_sock	The TURN transport instance.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_sock_unlock(pj_turn_sock *turn_sock);


/**
 * Set STUN message logging for this TURN session. 
 * See #pj_stun_session_set_log().
 *
 * @param turn_sock	The TURN transport instance.
 * @param flags		Bitmask combination of #pj_stun_sess_msg_log_flag
 */
PJ_DECL(void) pj_turn_sock_set_log(pj_turn_sock *turn_sock,
				   unsigned flags);

/**
 * Configure the SOFTWARE name to be sent in all STUN requests by the
 * TURN session.
 *
 * @param turn_sock	The TURN transport instance.
 * @param sw	    Software name string. If this argument is NULL or
 *		    empty, the session will not include SOFTWARE attribute
 *		    in STUN requests and responses.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_turn_sock_set_software_name(pj_turn_sock *turn_sock,
						    const pj_str_t *sw);


/**
 * Allocate a relay address/resource in the TURN server. This function
 * will resolve the TURN server using DNS SRV (if desired) and send TURN
 * \a Allocate request using the specified credential to allocate a relay
 * address in the server. This function completes asynchronously, and
 * application will be notified when the allocation process has been
 * successful in the \a on_state() callback when the state is set to
 * PJ_TURN_STATE_READY. If the allocation fails, the state will be set
 * to PJ_TURN_STATE_DEALLOCATING or greater.
 *
 * @param turn_sock	The TURN transport instance.
 * @param domain	The domain, hostname, or IP address of the TURN
 *			server. When this parameter contains domain name,
 *			the \a resolver parameter must be set to activate
 *			DNS SRV resolution.
 * @param default_port	The default TURN port number to use when DNS SRV
 *			resolution is not used. If DNS SRV resolution is
 *			used, the server port number will be set from the
 *			DNS SRV records.
 * @param resolver	If this parameter is not NULL, then the \a domain
 *			parameter will be first resolved with DNS SRV and
 *			then fallback to using DNS A/AAAA resolution when
 *			DNS SRV resolution fails. If this parameter is
 *			NULL, the \a domain parameter will be resolved as
 *			hostname.
 * @param cred		The STUN credential to be used for the TURN server.
 * @param param		Optional TURN allocation parameter.
 *
 * @return		PJ_SUCCESS if the operation has been successfully
 *			queued, or the appropriate error code on failure.
 *			When this function returns PJ_SUCCESS, the final
 *			result of the allocation process will be notified
 *			to application in \a on_state() callback.
 *			
 */
PJ_DECL(pj_status_t) pj_turn_sock_alloc(pj_turn_sock *turn_sock,
				        const pj_str_t *domain,
				        int default_port,
				        pj_dns_resolver *resolver,
				        const pj_stun_auth_cred *cred,
				        const pj_turn_alloc_param *param);

/**
 * Create or renew permission in the TURN server for the specified peer IP
 * addresses. Application must install permission for a particular (peer)
 * IP address before it sends any data to that IP address, or otherwise
 * the TURN server will drop the data.
 *
 * @param turn_sock	The TURN transport instance.
 * @param addr_cnt	Number of IP addresses.
 * @param addr		Array of peer IP addresses. Only the address family
 *			and IP address portion of the socket address matter.
 * @param options	Specify 1 to let the TURN client session automatically
 *			renew the permission later when they are about to
 *			expire.
 *
 * @return		PJ_SUCCESS if the operation has been successfully
 *			issued, or the appropriate error code. Note that
 *			the operation itself will complete asynchronously.
 */
PJ_DECL(pj_status_t) pj_turn_sock_set_perm(pj_turn_sock *turn_sock,
					   unsigned addr_cnt,
					   const pj_sockaddr addr[],
					   unsigned options);

/**
 * Send a data to the specified peer address via the TURN relay. This 
 * function will encapsulate the data as STUN Send Indication or TURN
 * ChannelData packet and send the message to the TURN server. The TURN
 * server then will send the data to the peer.
 *
 * The allocation (pj_turn_sock_alloc()) must have been successfully
 * created before application can relay any data.
 *
 * @param turn_sock	The TURN transport instance.
 * @param pkt		The data/packet to be sent to peer.
 * @param pkt_len	Length of the data.
 * @param peer_addr	The remote peer address (the ultimate destination
 *			of the data, and not the TURN server address).
 * @param addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS if data has been sent immediately, or
 *			PJ_EPENDING if data cannot be sent immediately. In
 *			this case the \a on_data_sent() callback will be
 *			called when data is actually sent. Any other return
 *			value indicates error condition.
 */ 
PJ_DECL(pj_status_t) pj_turn_sock_sendto(pj_turn_sock *turn_sock,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_sockaddr_t *peer_addr,
					unsigned addr_len);

/**
 * Optionally establish channel binding for the specified a peer address.
 * This function will assign a unique channel number for the peer address
 * and request channel binding to the TURN server for this address. When
 * a channel has been bound to a peer, the TURN transport and TURN server
 * will exchange data using ChannelData encapsulation format, which has
 * lower bandwidth overhead than Send Indication (the default format used
 * when peer address is not bound to a channel).
 *
 * @param turn_sock	The TURN transport instance.
 * @param peer		The remote peer address.
 * @param addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_sock_bind_channel(pj_turn_sock *turn_sock,
					       const pj_sockaddr_t *peer,
					       unsigned addr_len);
/**
 * Initiate connection to the specified peer using Connect request.
 * Application must call this function when it uses RFC 6062 (TURN TCP
 * allocations) to initiate a data connection to a peer. The connection status
 * will be notified via on_connection_status callback.
 *
 * According to RFC 6062, the TURN transport instance must be created with
 * connection type are set to PJ_TURN_TP_TCP, application must send TCP
 * Allocate request (with pj_turn_session_alloc()，set TURN allocation
 * parameter peer_conn_type to PJ_TURN_TP_TCP) before calling this function.
 *
 *
 * @param turn_sock	The TURN transport instance.
 * @param peer		The remote peer address.
 * @param addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_sock_connect(pj_turn_sock *turn_sock,
					  const pj_sockaddr_t *peer,
					  unsigned addr_len);
/**
 * Close previous TCP data connection for the specified peer.
 * According to RFC 6062, when the client wishes to terminate its relayed
 * connection to the peer, it closes the data connection to the server.
 *
 * @param turn_sock	The TURN transport instance.
 * @param peer		The remote peer address.
 * @param addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_sock_disconnect(pj_turn_sock *turn_sock,
					   const pj_sockaddr_t *peer,
					   unsigned addr_len);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_TURN_SOCK_H__ */


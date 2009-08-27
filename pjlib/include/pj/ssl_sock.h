/* $Id$ */
/* 
 * Copyright (C) 2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_SSL_SOCK_H__
#define __PJ_SSL_SOCK_H__

/**
 * @file ssl_sock.h
 * @brief Secure socket
 */

#include <pj/ioqueue.h>
#include <pj/sock.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJ_SSL_SOCK Secure socket I/O
 * @brief Secure socket provides security on socket operation using standard
 * security protocols such as SSL and TLS.
 * @ingroup PJ_IO
 * @{
 *
 * Secure socket wraps normal socket and applies security features, i.e: 
 * privacy and data integrity, on the socket traffic, using standard security
 * protocols such as SSL and TLS.
 *
 * Secure socket employs active socket operations, which is similar to (and
 * described more detail) in \ref PJ_ACTIVESOCK.
 */

/**
 * Opaque declaration of certificate or endpoint credentials. This may contains
 * certificate, private key, and trusted Certificate Authorities lists.
 */
typedef struct pj_ssl_cert_t pj_ssl_cert_t;

/**
 * This opaque structure describes the secure socket.
 */
typedef struct pj_ssl_sock_t pj_ssl_sock_t;

/**
 * This structure contains the callbacks to be called by the secure socket.
 */
typedef struct pj_ssl_sock_cb
{
    /**
     * This callback is called when a data arrives as the result of
     * pj_ssl_sock_start_read().
     *
     * @param ssock	The secure socket.
     * @param data	The buffer containing the new data, if any. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument may be NULL.
     * @param size	The length of data in the buffer.
     * @param status	The status of the read operation. This may contain
     *			non-PJ_SUCCESS for example when the TCP connection
     *			has been closed. In this case, the buffer may
     *			contain left over data from previous callback which
     *			the application may want to process.
     * @param remainder	If application wishes to leave some data in the 
     *			buffer (common for TCP applications), it should 
     *			move the remainder data to the front part of the 
     *			buffer and set the remainder length here. The value
     *			of this parameter will be ignored for datagram
     *			sockets.
     *
     * @return		PJ_TRUE if further read is desired, and PJ_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the secure socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_read)(pj_ssl_sock_t *ssock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder);
    /**
     * This callback is called when a packet arrives as the result of
     * pj_ssl_sock_start_recvfrom().
     *
     * @param ssock	The secure socket.
     * @param data	The buffer containing the packet, if any. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument will be set to NULL.
     * @param size	The length of packet in the buffer. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument will be set to zero.
     * @param src_addr	Source address of the packet.
     * @param addr_len	Length of the source address.
     * @param status	This contains
     *
     * @return		PJ_TRUE if further read is desired, and PJ_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the secure socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_recvfrom)(pj_ssl_sock_t *ssock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status);

    /**
     * This callback is called when data has been sent.
     *
     * @param ssock	The secure socket.
     * @param send_key	Key associated with the send operation.
     * @param sent	If value is positive non-zero it indicates the
     *			number of data sent. When the value is negative,
     *			it contains the error code which can be retrieved
     *			by negating the value (i.e. status=-sent).
     *
     * @return		Application may destroy the secure socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_sent)(pj_ssl_sock_t *ssock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent);

    /**
     * This callback is called when new connection arrives as the result
     * of pj_ssl_sock_start_accept().
     *
     * @param ssock	The secure socket.
     * @param newsock	The new incoming secure socket.
     * @param src_addr	The source address of the connection.
     * @param addr_len	Length of the source address.
     *
     * @return		PJ_TRUE if further accept() is desired, and PJ_FALSE
     *			when application no longer wants to accept incoming
     *			connection. Application may destroy the secure socket
     *			in the callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_accept_complete)(pj_ssl_sock_t *ssock,
				    pj_ssl_sock_t *newsock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len);

    /**
     * This callback is called when pending connect operation has been
     * completed.
     *
     * @param ssock	The secure socket.
     * @param status	The connection result. If connection has been
     *			successfully established, the status will contain
     *			PJ_SUCCESS.
     *
     * @return		Application may destroy the secure socket in the
     *			callback and return PJ_FALSE here. 
     */
    pj_bool_t (*on_connect_complete)(pj_ssl_sock_t *ssock,
				     pj_status_t status);

} pj_ssl_sock_cb;


/** 
 * Enumeration of secure socket protocol types.
 */
typedef enum pj_ssl_sock_proto
{
    PJ_SSL_SOCK_PROTO_DEFAULT,	    /**< Default protocol of backend.	*/
    PJ_SSL_SOCK_PROTO_TLS1,	    /**< TLSv1.0 protocol.		*/
    PJ_SSL_SOCK_PROTO_SSL2,	    /**< SSLv2.0 protocol.		*/
    PJ_SSL_SOCK_PROTO_SSL3,	    /**< SSLv3.0 protocol.		*/
    PJ_SSL_SOCK_PROTO_SSL23,	    /**< SSLv3.0 but can roll back to 
					 SSLv2.0.			*/
    PJ_SSL_SOCK_PROTO_DTLS1	    /**< DTLSv1.0 protocol.		*/
} pj_ssl_sock_proto;


/**
 * Definition of secure socket info structure.
 */
typedef struct pj_ssl_sock_info 
{
    /**
     * Describes whether secure socket connection is established, i.e: TLS/SSL 
     * handshaking has been done successfully.
     */
    pj_bool_t established;
    /**
     * Describes secure socket protocol being used.
     */
    pj_ssl_sock_proto proto;
    /**
     * Describes cipher suite being used, this can be known only when 
     * connection is established.
     */
    pj_str_t cipher;
    /**
     * Describes local address.
     */
    pj_sockaddr local_addr;
    /**
     * Describes remote address.
     */
    pj_sockaddr remote_addr;
   
} pj_ssl_sock_info;

/**
 * Definition of secure socket creation parameters.
 */
typedef struct pj_ssl_sock_param
{
    /**
     * Specifies socket address family, either pj_AF_INET() and pj_AF_INET6().
     *
     * Default is pj_AF_INET().
     */
    int sock_af;

    /**
     * Specify socket type, either pj_SOCK_DGRAM() or pj_SOCK_STREAM().
     *
     * Default is pj_SOCK_STREAM().
     */
    int sock_type;

    /**
     * Specify the ioqueue to use. Secure socket uses the ioqueue to perform
     * active socket operations, see \ref PJ_ACTIVESOCK for more detail.
     */
    pj_ioqueue_t *ioqueue;

    /**
     * Specify secure socket callbacks, see #pj_ssl_sock_cb.
     */
    pj_ssl_sock_cb cb;

    /**
     * Specify secure socket user data.
     */
    void *user_data;

    /**
     * Specify security protocol to use, see #pj_ssl_sock_proto.
     *
     * Default is PJ_SSL_SOCK_PROTO_DEFAULT.
     */
    pj_ssl_sock_proto proto;

    /**
     * Number of concurrent asynchronous operations that is to be supported
     * by the secure socket. This value only affects socket receive and
     * accept operations -- the secure socket will issue one or more 
     * asynchronous read and accept operations based on the value of this
     * field. Setting this field to more than one will allow more than one
     * incoming data or incoming connections to be processed simultaneously
     * on multiprocessor systems, when the ioqueue is polled by more than
     * one threads.
     *
     * The default value is 1.
     */
    unsigned async_cnt;

    /**
     * The ioqueue concurrency to be forced on the socket when it is 
     * registered to the ioqueue. See #pj_ioqueue_set_concurrency() for more
     * info about ioqueue concurrency.
     *
     * When this value is -1, the concurrency setting will not be forced for
     * this socket, and the socket will inherit the concurrency setting of 
     * the ioqueue. When this value is zero, the secure socket will disable
     * concurrency for the socket. When this value is +1, the secure socket
     * will enable concurrency for the socket.
     *
     * The default value is -1.
     */
    int concurrency;

    /**
     * If this option is specified, the secure socket will make sure that
     * asynchronous send operation with stream oriented socket will only
     * call the callback after all data has been sent. This means that the
     * secure socket will automatically resend the remaining data until
     * all data has been sent.
     *
     * Please note that when this option is specified, it is possible that
     * error is reported after partial data has been sent. Also setting
     * this will disable the ioqueue concurrency for the socket.
     *
     * Default value is 1.
     */
    pj_bool_t whole_data;

    /**
     * Specify buffer size for delayed send operation. This setting is only
     * applied for some platforms that restrict more than one outstanding 
     * send operation at a time, e.g: Symbian. So delaying/buffering send 
     * mechanism is used to allow application to send data anytime without 
     * worrying about current outstanding send operations.
     *  
     * Default value is 0, except for Symbian 8192 bytes.
     */
    pj_size_t send_buffer_size;

    /**
     * Cipher list string. If empty, then default cipher list of the backend 
     * will be used.
     */
    pj_str_t ciphers;

    /**
     * Security negotiation timeout. If this is set to zero (both sec and 
     * msec), the negotiation doesn't have a timeout.
     *
     * Default value is zero.
     */
    pj_time_val	timeout;

    /**
     * Specify whether endpoint should verify peer certificate.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t verify_peer;
    
    /**
     * When secure socket is acting as server (handles incoming connection),
     * it will require the client to provide certificate.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t require_client_cert;

    /**
     * When secure socket is acting as client (perform outgoing connection)
     * and it needs to verify server name (e.g: host or domain name) by
     * matching it to the name specified in the server certificate. This 
     * setting is useful when the server is hosting multiple domains for
     * the same listening socket.
     *
     * Default value is zero/not-set.
     */
    pj_str_t servername;
    
} pj_ssl_sock_param;


/**
 * Initialize the secure socket parameters for its creation with 
 * the default values.
 *
 * @param param		The parameter to be initialized.
 */
PJ_DECL(void) pj_ssl_sock_param_default(pj_ssl_sock_param *param);


/**
 * Create secure socket instance.
 *
 * @param pool		The pool for allocating secure socket instance.
 * @param param		The secure socket parameter, see #pj_ssl_sock_param.
 * @param p_ssock	Pointer to secure socket instance to be created.
 *
 * @return		PJ_SUCCESS when successful.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_create(pj_pool_t *pool,
					const pj_ssl_sock_param *param,
					pj_ssl_sock_t **p_ssock);


/**
 * Set secure socket certificate or credentials. Credentials may include 
 * certificate, private key and trusted Certification Authorities list. 
 * Normally, server socket must provide certificate (and private key).
 * Socket client may also need to provide certificate in case requested
 * by the server.
 *
 * @param ssock		The secure socket instance.
 * @param pool		The pool.
 * @param cert		The endpoint certificate/credentials, see
 *			#pj_ssl_cert_t.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_set_certificate(
					    pj_ssl_sock_t *ssock,
					    pj_pool_t *pool,
					    const pj_ssl_cert_t *cert);


/**
 * Close and destroy the secure socket.
 *
 * @param ssock		The secure socket.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_close(pj_ssl_sock_t *ssock);


/**
 * Associate arbitrary data with the secure socket. Application may
 * inspect this data in the callbacks and associate it with higher
 * level processing.
 *
 * @param ssock		The secure socket.
 * @param user_data	The user data to be associated with the secure
 *			socket.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_set_user_data(pj_ssl_sock_t *ssock,
					       void *user_data);

/**
 * Retrieve the user data previously associated with this secure
 * socket.
 *
 * @param ssock		The secure socket.
 *
 * @return		The user data.
 */
PJ_DECL(void*) pj_ssl_sock_get_user_data(pj_ssl_sock_t *ssock);


/**
 * Retrieve the local address and port used by specified secure socket.
 *
 * @param ssock		The secure socket.
 * @param info		The info buffer to be set, see #pj_ssl_sock_info.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_get_info(pj_ssl_sock_t *ssock,
					  pj_ssl_sock_info *info);


/**
 * Starts read operation on this secure socket. This function will create
 * \a async_cnt number of buffers (the \a async_cnt parameter was given
 * in \a pj_ssl_sock_create() function) where each buffer is \a buff_size
 * long. The buffers are allocated from the specified \a pool. Once the 
 * buffers are created, it then issues \a async_cnt number of asynchronous
 * \a recv() operations to the socket and returns back to caller. Incoming
 * data on the socket will be reported back to application via the 
 * \a on_data_read() callback.
 *
 * Application only needs to call this function once to initiate read
 * operations. Further read operations will be done automatically by the
 * secure socket when \a on_data_read() callback returns non-zero. 
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param flags		Flags to be given to pj_ioqueue_recv().
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_read(pj_ssl_sock_t *ssock,
					    pj_pool_t *pool,
					    unsigned buff_size,
					    pj_uint32_t flags);

/**
 * Same as #pj_ssl_sock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param readbuf	Array of packet buffers, each has buff_size size.
 * @param flags		Flags to be given to pj_ioqueue_recv().
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_read2(pj_ssl_sock_t *ssock,
					     pj_pool_t *pool,
					     unsigned buff_size,
					     void *readbuf[],
					     pj_uint32_t flags);

/**
 * Same as pj_ssl_sock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param flags		Flags to be given to pj_ioqueue_recvfrom().
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_recvfrom(pj_ssl_sock_t *ssock,
						pj_pool_t *pool,
						unsigned buff_size,
						pj_uint32_t flags);

/**
 * Same as #pj_ssl_sock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate buffers for incoming data.
 * @param buff_size	The size of each buffer, in bytes.
 * @param readbuf	Array of packet buffers, each has buff_size size.
 * @param flags		Flags to be given to pj_ioqueue_recvfrom().
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_recvfrom2(pj_ssl_sock_t *ssock,
						 pj_pool_t *pool,
						 unsigned buff_size,
						 void *readbuf[],
						 pj_uint32_t flags);

/**
 * Send data using the socket.
 *
 * @param ssock		The secure socket.
 * @param send_key	The operation key to send the data, which is useful
 *			if application wants to submit multiple pending
 *			send operations and want to track which exact data 
 *			has been sent in the \a on_data_sent() callback.
 * @param data		The data to be sent. This data must remain valid
 *			until the data has been sent.
 * @param size		The size of the data.
 * @param flags		Flags to be given to pj_ioqueue_send().
 *
 *
 * @return		PJ_SUCCESS if data has been sent immediately, or
 *			PJ_EPENDING if data cannot be sent immediately. In
 *			this case the \a on_data_sent() callback will be
 *			called when data is actually sent. Any other return
 *			value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_send(pj_ssl_sock_t *ssock,
				      pj_ioqueue_op_key_t *send_key,
				      const void *data,
				      pj_ssize_t *size,
				      unsigned flags);

/**
 * Send datagram using the socket.
 *
 * @param ssock		The secure socket.
 * @param send_key	The operation key to send the data, which is useful
 *			if application wants to submit multiple pending
 *			send operations and want to track which exact data 
 *			has been sent in the \a on_data_sent() callback.
 * @param data		The data to be sent. This data must remain valid
 *			until the data has been sent.
 * @param size		The size of the data.
 * @param flags		Flags to be given to pj_ioqueue_send().
 * @param addr		The destination address.
 * @param addr_len	Length of buffer containing destination address.
 *
 * @return		PJ_SUCCESS if data has been sent immediately, or
 *			PJ_EPENDING if data cannot be sent immediately. In
 *			this case the \a on_data_sent() callback will be
 *			called when data is actually sent. Any other return
 *			value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_sendto(pj_ssl_sock_t *ssock,
					pj_ioqueue_op_key_t *send_key,
					const void *data,
					pj_ssize_t *size,
					unsigned flags,
					const pj_sockaddr_t *addr,
					int addr_len);


/**
 * Starts asynchronous socket accept() operations on this secure socket. 
 * This function will issue \a async_cnt number of asynchronous \a accept() 
 * operations to the socket and returns back to caller. Incoming
 * connection on the socket will be reported back to application via the
 * \a on_accept_complete() callback.
 *
 * Application only needs to call this function once to initiate accept()
 * operations. Further accept() operations will be done automatically by 
 * the secure socket when \a on_accept_complete() callback returns non-zero.
 *
 * @param ssock		The secure socket.
 * @param pool		Pool used to allocate some internal data for the
 *			operation.
 * @param localaddr	Local address to bind on.
 * @param addr_len	Length of buffer containing local address.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_accept(pj_ssl_sock_t *ssock,
					      pj_pool_t *pool,
					      const pj_sockaddr_t *local_addr,
					      int addr_len);


/**
 * Starts asynchronous socket connect() operation and SSL/TLS handshaking 
 * for this socket. Once the connection is done (either successfully or not),
 * the \a on_connect_complete() callback will be called.
 *
 * @param ssock		The secure socket.
 * @param pool		The pool to allocate some internal data for the
 *			operation.
 * @param localaddr	Local address.
 * @param remaddr	Remote address.
 * @param addr_len	Length of buffer containing above addresses.
 *
 * @return		PJ_SUCCESS if connection can be established immediately
 *			or PJ_EPENDING if connection cannot be established 
 *			immediately. In this case the \a on_connect_complete()
 *			callback will be called when connection is complete. 
 *			Any other return value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_ssl_sock_start_connect(pj_ssl_sock_t *ssock,
					       pj_pool_t *pool,
					       const pj_sockaddr_t *localaddr,
					       const pj_sockaddr_t *remaddr,
					       int addr_len);


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJ_SSL_SOCK_H__ */

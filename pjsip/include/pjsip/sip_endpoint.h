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
#ifndef __PJSIP_SIP_ENDPOINT_H__
#define __PJSIP_SIP_ENDPOINT_H__

/**
 * @file sip_endpoint.h
 * @brief SIP Endpoint.
 */

#include <pjsip/sip_transport.h>
#include <pjsip/sip_resolve.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP SIP Stack Core
 * Implementation of core SIP protocol stack processing.
 */

/**
 * @defgroup PJSIP_ENDPT SIP Endpoint
 * @ingroup PJSIP
 * @brief
 * Representation of SIP node instance.
 *
 * SIP Endpoint instance (pjsip_endpoint) can be viewed as the master/owner of
 * all SIP objects in an application. It performs the following roles:
 *  - it manages the allocation/deallocation of memory pools for all objects.
 *  - it manages listeners and transports, and how they are used by 
 *    transactions.
 *  - it owns transaction hash table.
 *  - it receives incoming messages from transport layer and automatically
 *    dispatches them to the correct transaction (or create a new one).
 *  - it has a single instance of timer management (timer heap).
 *  - it manages modules, which is the primary means of extending the library.
 *  - it provides single polling function for all objects and distributes 
 *    events.
 *  - it provides SIP policy such as which outbound proxy to use for all
 *    outgoing SIP request messages.
 *  - it automatically handles incoming requests which can not be handled by
 *    existing modules (such as when incoming request has unsupported method).
 *  - and so on..
 *
 * Theoritically application can have multiple instances of SIP endpoint, 
 * although it's not clear why application may want to do it.
 *
 * @{
 */

/**
 * Create an instance of SIP endpoint from the specified pool factory.
 * The pool factory reference then will be kept by the endpoint, so that 
 * future memory allocations by SIP components will be taken from the same
 * pool factory.
 *
 * @param pf	        Pool factory that will be used for the lifetime of 
 *                      endpoint.
 * @param name          Optional name to be specified for the endpoint.
 *                      If this parameter is NULL, then the name will use
 *                      local host name.
 * @param endpt         Pointer to receive endpoint instance.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create(pj_pool_factory *pf,
                                        pjsip_endpoint **endpt);

/**
 * Destroy endpoint instance. Application must make sure that all pending
 * transactions have been terminated properly, because this function does not
 * check for the presence of pending transactions.
 *
 * @param endpt		The SIP endpoint to be destroyed.
 */
PJ_DECL(void) pjsip_endpt_destroy(pjsip_endpoint *endpt);

/**
 * Get endpoint name.
 *
 * @param endpt         The SIP endpoint instance.
 *
 * @return              Endpoint name, as was registered during endpoint
 *                      creation. The string is NULL terminated.
 */
PJ_DECL(const pj_str_t*) pjsip_endpt_name(const pjsip_endpoint *endpt);

/**
 * Poll for events. Application must call this function periodically to ensure
 * that all events from both transports and timer heap are handled in timely
 * manner.  This function, like all other endpoint functions, is thread safe, 
 * and application may have more than one thread concurrently calling this function.
 *
 * @param endpt		The endpoint.
 * @param max_timeout	Maximum time to wait for events, or NULL to wait forever
 *			until event is received.
 */
PJ_DECL(void) pjsip_endpt_handle_events( pjsip_endpoint *endpt, 
					 const pj_time_val *max_timeout);

/**
 * Dump endpoint status to the log. This will print the status to the log
 * with log level 3.
 *
 * @param endpt		The endpoint.
 * @param detail	If non zero, then it will dump a detailed output.
 *			BEWARE that this option may crash the system because
 *			it tries to access all memory pools.
 */
PJ_DECL(void) pjsip_endpt_dump( pjsip_endpoint *endpt, pj_bool_t detail );

/**
 * Create pool from the endpoint. All SIP components should allocate their
 * memory pool by calling this function, to make sure that the pools are
 * allocated from the same pool factory. This function, like all other endpoint
 * functions, is thread safe.
 *
 * @param endpt		The SIP endpoint.
 * @param pool_name	Name to be assigned to the pool.
 * @param initial	The initial size of the pool.
 * @param increment	The resize size.
 * @return		Memory pool, or NULL on failure.
 *
 * @see pj_pool_create
 */
PJ_DECL(pj_pool_t*) pjsip_endpt_create_pool( pjsip_endpoint *endpt,
					     const char *pool_name,
					     pj_size_t initial,
					     pj_size_t increment );

/**
 * Return back pool to endpoint to be released back to the pool factory.
 * This function, like all other endpoint functions, is thread safe.
 *
 * @param endpt	    The endpoint.
 * @param pool	    The pool to be destroyed.
 */
PJ_DECL(void) pjsip_endpt_destroy_pool( pjsip_endpoint *endpt,
					pj_pool_t *pool );

/**
 * Schedule timer to endpoint's timer heap. Application must poll the endpoint
 * periodically (by calling #pjsip_endpt_handle_events) to ensure that the
 * timer events are handled in timely manner. When the timeout for the timer
 * has elapsed, the callback specified in the entry argument will be called.
 * This function, like all other endpoint functions, is thread safe.
 *
 * @param endpt	    The endpoint.
 * @param entry	    The timer entry.
 * @param delay	    The relative delay of the timer.
 * @return	    PJ_OK (zero) if successfull.
 */
PJ_DECL(pj_status_t) pjsip_endpt_schedule_timer( pjsip_endpoint *endpt,
						 pj_timer_entry *entry,
						 const pj_time_val *delay );

/**
 * Cancel the previously registered timer.
 * This function, like all other endpoint functions, is thread safe.
 *
 * @param endpt	    The endpoint.
 * @param entry	    The timer entry previously registered.
 */
PJ_DECL(void) pjsip_endpt_cancel_timer( pjsip_endpoint *endpt, 
					pj_timer_entry *entry );

/**
 * Create a new transaction. After creating the transaction, application MUST
 * initialize the transaction as either UAC or UAS (by calling
 * #pjsip_tsx_init_uac or #pjsip_tsx_init_uas), then must register the 
 * transaction to endpoint with #pjsip_endpt_register_tsx.
 * This function, like all other endpoint functions, is thread safe.
 *
 * @param endpt	    The SIP endpoint.
 * @param p_tsx	    Pointer to receive the transaction.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_tsx(pjsip_endpoint *endpt,
					    pjsip_transaction **p_tsx);

/**
 * Register the transaction to the endpoint's transaction table.
 * Before the transaction is registered, it must have been initialized as
 * either UAS or UAC by calling #pjsip_tsx_init_uac or #pjsip_tsx_init_uas.
 * This function, like all other endpoint functions, is thread safe.
 *
 * @param endpt	    The SIP endpoint.
 * @param tsx	    The transaction.
 */
PJ_DECL(void) pjsip_endpt_register_tsx( pjsip_endpoint *endpt,
					pjsip_transaction *tsx);

/**
 * Forcefull destroy the transaction.
 * The only time where application needs to call this function is when the
 * transaction fails to initialize in #pjsip_tsx_init_uac or
 * #pjsip_tsx_init_uas. For other cases. the transaction will be destroyed
 * automaticly by endpoint.
 *
 * @param endpt	    The endpoint.
 * @param tsx	    The transaction to destroy.
 */
PJ_DECL(void) pjsip_endpt_destroy_tsx( pjsip_endpoint *endpt,
				      pjsip_transaction *tsx);

/**
 * Create a new transmit data buffer.
 * This function, like all other endpoint functions, is thread safe.
 *
 * @param endpt	    The endpoint.
 * @param p_tdata    Pointer to receive transmit data buffer.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_tdata( pjsip_endpoint *endpt,
					       pjsip_tx_data **p_tdata);

/**
 * Asynchronously resolve a SIP target host or domain according to rule 
 * specified in RFC 3263 (Locating SIP Servers). When the resolving operation
 * has completed, the callback will be called.
 *
 * Note: at the moment we don't have implementation of RFC 3263 yet!
 *
 * @param resolver  The resolver engine.
 * @param pool	    The pool to allocate resolver job.
 * @param target    The target specification to be resolved.
 * @param token	    A user defined token to be passed back to callback function.
 * @param cb	    The callback function.
 */
PJ_DECL(void) pjsip_endpt_resolve( pjsip_endpoint *endpt,
				   pj_pool_t *pool,
				   pjsip_host_port *target,
				   void *token,
				   pjsip_resolver_callback *cb);

/**
 * Find a SIP transport suitable for sending SIP message to the specified
 * address. This function will complete asynchronously when the transport is
 * ready (for example, when TCP socket is connected), and when it completes,
 * the callback will be called with the status of the operation.
 *
 * @see pjsip_transport_get
 */
PJ_DECL(void) pjsip_endpt_get_transport( pjsip_endpoint *endpt,
					 pj_pool_t *pool,
					 pjsip_transport_type_e type,
					 const pj_sockaddr_in *remote,
					 void *token,
					 pjsip_transport_completion_callback *cb);

/**
 * Create listener a new transport listener. A listener is transport object
 * that is capable of receiving SIP messages. For UDP listener, normally
 * application should use #pjsip_endpt_create_udp_listener instead if the 
 * application has already created the socket.
 * This function, like all other endpoint functions, is thread safe.
 *
 * @param endpt	    The endpoint instance.
 * @param type	    Transport type (eg. UDP, TCP, etc.)
 * @param addr	    The bound address of the transport.
 * @param addr_name The address to be advertised in SIP messages. For example,
 *		    the bound address can be 0.0.0.0, but the advertised address
 *		    normally will be the IP address of the host.
 *
 * @return	    Zero if listener is created successfully.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_listener( pjsip_endpoint *endpt,
						  pjsip_transport_type_e type,
						  pj_sockaddr_in *addr,
						  const pj_sockaddr_in *addr_name);

/**
 * Create UDP listener. For UDP, normally the application would create the
 * socket by itself (for STUN purpose), then it can register the socket as
 * listener by calling this function.
 * This function, like all other endpoint functions, is thread safe.
 *
 * @param endpt	    The endpoint instance.
 * @param sock	    The socket handle.
 * @param addr_name The address to be advertised in SIP message. If the socket
 *		    has been resolved with STUN, then application may specify 
 *		    the mapped address in this parameter.
 *
 * @return	    Zero if listener is created successfully.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_udp_listener( pjsip_endpoint *endpt,
						      pj_sock_t sock,
						      const pj_sockaddr_in *addr_name);

/**
 * Get additional headers to be put in outgoing request message. 
 * This function is normally called by transaction layer when sending outgoing
 * requests.
 * 
 * @param endpt	    The endpoint.
 *
 * @return	    List of additional headers to be put in outgoing requests.
 */
PJ_DECL(const pjsip_hdr*) pjsip_endpt_get_request_headers(pjsip_endpoint *endpt);

/**
 * Get "Allow" header from endpoint. The endpoint builds the "Allow" header
 * from the list of methods supported by modules.
 *
 * @param endpt	    The endpoint.
 *
 * @return	    "Allow" header, or NULL if endpoint doesn't have "Allow" header.
 */
PJ_DECL(const pjsip_allow_hdr*) pjsip_endpt_get_allow_hdr( pjsip_endpoint *endpt );


/**
 * Find transaction in endpoint's transaction table by the transaction's key.
 * This function normally is only used by modules. The key for a transaction
 * can be created by calling #pjsip_tsx_create_key.
 *
 * @param endpt	    The endpoint instance.
 * @param key	    Transaction key, as created with #pjsip_tsx_create_key.
 *
 * @return	    The transaction, or NULL if it's not found.
 */
PJ_DECL(pjsip_transaction*) pjsip_endpt_find_tsx( pjsip_endpoint *endpt,
					          const pj_str_t *key );

/**
 * Set list of SIP proxies to be visited for all outbound request messages.
 * Application can call this function to specify how outgoing request messages
 * should be routed. For example, if outgoing requests should go through an
 * outbound proxy, then application can specify the URL of the proxy when
 * calling this function. More than one proxy can be specified, and the
 * order of which proxy is specified when calling this function specifies
 * the order of which proxy will be visited first by the request messages.
 *
 * @param endpt	    The endpoint instance.
 * @param url_cnt   Number of proxies/URLs in the array.
 * @param url	    Array of proxy URL, which specifies the order of which
 *		    proxy will be visited first (e.g. url[0] will be visited
 *		    before url[1]).
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_set_proxies( pjsip_endpoint *endpt,
					      int url_cnt, const pj_str_t url[]);

/**
 * Get the list of "Route" header that are configured for this endpoint.
 * The "Route" header specifies how outbound request messages will be sent,
 * and is built when application sets the outbound proxy.
 *
 * @param endpt	    The endpoint instance.
 *
 * @return	    List of "Route" header.
 */
PJ_DECL(const pjsip_route_hdr*) pjsip_endpt_get_routing( pjsip_endpoint *endpt );

/**
 * Log an error.
 */
PJ_DECL(void) pjsip_endpt_log_error( pjsip_endpoint *endpt,
				     const char *sender,
                                     pj_status_t error_code,
                                     const char *format,
                                     ... );

#define PJSIP_ENDPT_LOG_ERROR(expr)   \
            pjsip_endpt_log_error expr

#define PJSIP_ENDPT_TRACE(tracing,expr) \
            do {                        \
                if ((tracing))          \
                    PJ_LOG(4,expr);     \
            } while (0)

/**
 * @}
 */

/*
 * Internal functions.
 */
/*
 * Receive transaction events from transactions and put in the event queue
 * to be processed later.
 */
void pjsip_endpt_send_tsx_event( pjsip_endpoint *endpt, pjsip_event *evt );

PJ_END_DECL

#endif	/* __PJSIP_SIP_ENDPOINT_H__ */


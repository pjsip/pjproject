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
#ifndef __PJSIP_SIP_ENDPOINT_H__
#define __PJSIP_SIP_ENDPOINT_H__

/**
 * @file sip_endpoint.h
 * @brief SIP Endpoint.
 */

#include <pjsip/sip_transport.h>
#include <pjsip/sip_resolve.h>

/**
 * @defgroup PJSIP_CORE_CORE At the Very Core
 * @ingroup PJSIP_CORE
 * @brief The very core of PJSIP.
 */

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_ENDPT Endpoint
 * @ingroup PJSIP_CORE_CORE
 * @brief The master, owner of all objects
 *
 * SIP Endpoint instance (pjsip_endpoint) can be viewed as the master/owner of
 * all SIP objects in an application. It performs the following roles:
 *  - it manages the allocation/deallocation of memory pools for all objects.
 *  - it manages listeners and transports, and how they are used by 
 *    transactions.
 *  - it receives incoming messages from transport layer and automatically
 *    dispatches them to the correct transaction (or create a new one).
 *  - it has a single instance of timer management (timer heap).
 *  - it manages modules, which is the primary means of extending the library.
 *  - it provides single polling function for all objects and distributes 
 *    events.
 *  - it automatically handles incoming requests which can not be handled by
 *    existing modules (such as when incoming request has unsupported method).
 *  - and so on..
 *
 * Application should only instantiate one SIP endpoint instance for every
 * process.
 *
 * @{
 */


/**
 * Type of callback to register to pjsip_endpt_atexit().
 */
typedef void (*pjsip_endpt_exit_callback)(pjsip_endpoint *endpt);


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
					const char *name,
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
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_handle_events( pjsip_endpoint *endpt, 
					        const pj_time_val *max_timeout);


/**
 * Handle events with additional info about number of events that
 * have been handled.
 *
 * @param endpt		The endpoint.
 * @param max_timeout	Maximum time to wait for events, or NULL to wait forever
 *			until event is received.
 * @param count		Optional argument to receive the number of events that
 *			have been handled by the function.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_handle_events2(pjsip_endpoint *endpt,
					        const pj_time_val *max_timeout,
					        unsigned *count);

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
#if PJ_TIMER_DEBUG
#define pjsip_endpt_schedule_timer(ept,ent,d) \
			pjsip_endpt_schedule_timer_dbg(ept, ent, d, \
			                               __FILE__, __LINE__)

PJ_DECL(pj_status_t) pjsip_endpt_schedule_timer_dbg(pjsip_endpoint *endpt,
						    pj_timer_entry *entry,
						    const pj_time_val *delay,
						    const char *src_file,
						    int src_line);
#else
PJ_DECL(pj_status_t) pjsip_endpt_schedule_timer( pjsip_endpoint *endpt,
						 pj_timer_entry *entry,
						 const pj_time_val *delay );
#endif

/**
 * Schedule timer to endpoint's timer heap with group lock. Application must
 * poll the endpoint periodically (by calling #pjsip_endpt_handle_events) to
 * ensure that the timer events are handled in timely manner. When the
 * timeout for the timer has elapsed, the callback specified in the entry
 * argument will be called. This function, like all other endpoint functions,
 * is thread safe.
 *
 * @param endpt	    The endpoint.
 * @param entry	    The timer entry.
 * @param delay	    The relative delay of the timer.
 * @param id_val    The value to be set to the "id" field of the timer entry
 * 		    once the timer is scheduled.
 * @param grp_lock  The group lock.
 * @return	    PJ_OK (zero) if successfull.
 */
#if PJ_TIMER_DEBUG
#define pjsip_endpt_schedule_timer_w_grp_lock(ept,ent,d,id,gl) \
		pjsip_endpt_schedule_timer_w_grp_lock_dbg(ept,ent,d,id,gl,\
							  __FILE__, __LINE__)

PJ_DECL(pj_status_t) pjsip_endpt_schedule_timer_w_grp_lock_dbg(
						    pjsip_endpoint *endpt,
						    pj_timer_entry *entry,
						    const pj_time_val *delay,
						    int id_val,
						    pj_grp_lock_t *grp_lock,
						    const char *src_file,
						    int src_line);
#else
PJ_DECL(pj_status_t) pjsip_endpt_schedule_timer_w_grp_lock(
						 pjsip_endpoint *endpt,
						 pj_timer_entry *entry,
						 const pj_time_val *delay,
						 int id_val,
						 pj_grp_lock_t *grp_lock );
#endif

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
 * Get the timer heap instance of the SIP endpoint.
 *
 * @param endpt	    The endpoint.
 *
 * @return	    The timer heap instance.
 */
PJ_DECL(pj_timer_heap_t*) pjsip_endpt_get_timer_heap(pjsip_endpoint *endpt);


/**
 * Register new module to the endpoint.
 * The endpoint will then call the load and start function in the module to 
 * properly initialize the module, and assign a unique module ID for the 
 * module.
 *
 * @param endpt		The endpoint.
 * @param module	The module to be registered.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_register_module( pjsip_endpoint *endpt,
						  pjsip_module *module );

/**
 * Unregister a module from the endpoint.
 * The endpoint will then call the stop and unload function in the module to 
 * properly shutdown the module.
 *
 * @param endpt		The endpoint.
 * @param module	The module to be registered.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_unregister_module( pjsip_endpoint *endpt,
						    pjsip_module *module );

/**
 * This describes additional parameters to pjsip_endpt_process_rx_data()
 * function. Application MUST call pjsip_process_rdata_param_default() to
 * initialize this structure.
 */
typedef struct pjsip_process_rdata_param
{
    /**
     * Specify the minimum priority number of the modules that are allowed
     * to process the message. Default is zero to allow all modules to
     * process the message.
     */
    unsigned start_prio;

    /**
     * Specify the pointer of the module where processing will start.
     * The default is NULL, meaning processing will start from the start
     * of the module list.
     */
    void *start_mod;

    /**
     * Set to N, then processing will start at Nth module after start
     * module (where start module can be an explicit module as specified
     * by \a start_mod or the start of module list when \a start_mod is
     * NULL). For example, if set to 1, then processing will start from
     * the next module after start module. Default is zero.
     */
    unsigned idx_after_start;

    /**
     * Print nothing to log. Default is PJ_FALSE.
     */
    pj_bool_t silent;

} pjsip_process_rdata_param;

/**
 * Initialize with default.
 *
 * @param p	The param.
 */
PJ_DECL(void) pjsip_process_rdata_param_default(pjsip_process_rdata_param *p);

/**
 * Manually distribute the specified pjsip_rx_data to registered modules.
 * Normally application does not need to call this function because received
 * messages will be given to endpoint automatically by transports.
 *
 * Application can use this function when it has postponed the processing of
 * an incoming message, for example to perform long operations such as
 * database operation or to consult other servers to decide what to do with
 * the message. In this case, application clones the original rdata, return
 * from the callback, and perform the long operation. Upon completing the
 * long operation, it resumes pjsip's module processing by calling this
 * function, and then free the cloned rdata.
 *
 * @param endpt		The endpoint instance.
 * @param rdata		The rdata to be distributed.
 * @param p		Optional pointer to param to specify from which module
 * 			the processing should start.
 * @param p_handled	Optional pointer to receive last return value of
 * 			module's \a on_rx_request() or \a on_rx_response()
 * 			callback.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_process_rx_data(pjsip_endpoint *endpt,
                                                 pjsip_rx_data *rdata,
                                                 pjsip_process_rdata_param *p,
                                                 pj_bool_t *p_handled);

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
PJ_DECL(void) pjsip_endpt_release_pool( pjsip_endpoint *endpt,
					pj_pool_t *pool );

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
 * Register the transaction to the endpoint's transaction table.
 * This function should only be used internally by the stack.
 *
 * @param endpt	    The SIP endpoint.
 * @param tsx	    The transaction.
 */
PJ_DECL(void) pjsip_endpt_register_tsx( pjsip_endpoint *endpt,
					pjsip_transaction *tsx);

/**
 * Forcefull destroy the transaction. This function should only be used
 * internally by the stack.
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
 * Create the DNS resolver instance. Application creates the DNS
 * resolver instance, set the nameserver to be used by the DNS
 * resolver, then set the DNS resolver to be used by the endpoint
 * by calling #pjsip_endpt_set_resolver().
 *
 * @param endpt		The SIP endpoint instance.
 * @param p_resv	Pointer to receive the DNS resolver instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_resolver(pjsip_endpoint *endpt,
						 pj_dns_resolver **p_resv);

/**
 * Set DNS resolver to be used by the SIP resolver. Application can set
 * the resolver instance to NULL to disable DNS resolution (perhaps
 * temporarily). When DNS resolver is disabled, the endpoint will resolve
 * hostnames with the normal pj_gethostbyname() function.
 *
 * @param endpt		The SIP endpoint instance.
 * @param resv		The resolver instance to be used by the SIP
 *			endpoint.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_set_resolver(pjsip_endpoint *endpt,
					      pj_dns_resolver *resv);

/**
 * Set the DNS external resolver implementation to use in the SIP resolver. 
 *
 * Note that naturally when implementing its own resolver, application would not
 * need the internal resolver, hence this function will also destroy the 
 * PJLIB-UTIL DNS resolver if any (e.g: set using #pjsip_endpt_set_resolver()). 
 * Application that needs it, still be able create its own instance. 
 *
 * @param endpt       The SIP resolver engine.
 * @param ext_res   The external resolver implementation callback. This argument
 *		    can be NULL to reset the whole external implementation. 
 *		    However, it is prohibited to reset individual callback.
 * 
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_set_ext_resolver(pjsip_endpoint *endpt,
						  pjsip_ext_resolver *ext_res);

/**
 * Get the DNS resolver being used by the SIP resolver.
 *
 * @param endpt		The SIP endpoint instance.
 *
 * @return		The DNS resolver instance currently being used
 *			by the SIP endpoint.
 */
PJ_DECL(pj_dns_resolver*) pjsip_endpt_get_resolver(pjsip_endpoint *endpt);

/**
 * Asynchronously resolve a SIP target host or domain according to rule 
 * specified in RFC 3263 (Locating SIP Servers). When the resolving operation
 * has completed, the callback will be called.
 *
 * @param endpt	    The endpoint instance.
 * @param pool	    The pool to allocate resolver job.
 * @param target    The target specification to be resolved.
 * @param token	    A user defined token to be passed back to callback function.
 * @param cb	    The callback function.
 */
PJ_DECL(void) pjsip_endpt_resolve( pjsip_endpoint *endpt,
				   pj_pool_t *pool,
				   pjsip_host_info *target,
				   void *token,
				   pjsip_resolver_callback *cb);

/**
 * Get transport manager instance.
 *
 * @param endpt	    The endpoint.
 *
 * @return	    Transport manager instance.
 */
PJ_DECL(pjsip_tpmgr*) pjsip_endpt_get_tpmgr(pjsip_endpoint *endpt);

/**
 * Get ioqueue instance.
 *
 * @param endpt	    The endpoint.
 *
 * @return	    The ioqueue.
 */
PJ_DECL(pj_ioqueue_t*) pjsip_endpt_get_ioqueue(pjsip_endpoint *endpt);

/**
 * Find a SIP transport suitable for sending SIP message to the specified
 * address. If transport selector ("sel") is set, then the function will
 * check if the transport selected is suitable to send requests to the
 * specified address.
 *
 * @see pjsip_tpmgr_acquire_transport
 *
 * @param endpt	    The SIP endpoint instance.
 * @param type	    The type of transport to be acquired.
 * @param remote    The remote address to send message to.
 * @param addr_len  Length of the remote address.
 * @param sel	    Optional pointer to transport selector instance which is
 *		    used to find explicit transport, if required.
 * @param p_tp	    Pointer to receive the transport instance, if one is found.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) 
pjsip_endpt_acquire_transport( pjsip_endpoint *endpt,
			       pjsip_transport_type_e type,
			       const pj_sockaddr_t *remote,
			       int addr_len,
			       const pjsip_tpselector *sel,
			       pjsip_transport **p_tp);


/**
 * Find a SIP transport suitable for sending SIP message to the specified
 * address by also considering the outgoing SIP message data. If transport 
 * selector ("sel") is set, then the function will check if the transport 
 * selected is suitable to send requests to the specified address.
 *
 * @see pjsip_tpmgr_acquire_transport
 *
 * @param endpt	    The SIP endpoint instance.
 * @param type	    The type of transport to be acquired.
 * @param remote    The remote address to send message to.
 * @param addr_len  Length of the remote address.
 * @param sel	    Optional pointer to transport selector instance which is
 *		    used to find explicit transport, if required.
 * @param tdata	    Optional pointer to SIP message data to be sent.
 * @param p_tp	    Pointer to receive the transport instance, if one is found.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) 
pjsip_endpt_acquire_transport2(pjsip_endpoint *endpt,
			       pjsip_transport_type_e type,
			       const pj_sockaddr_t *remote,
			       int addr_len,
			       const pjsip_tpselector *sel,
			       pjsip_tx_data *tdata,
			       pjsip_transport **p_tp);


/*****************************************************************************
 *
 * Capabilities Management
 *
 * Modules may implement new capabilities to the stack. These capabilities
 * are indicated by the appropriate SIP header fields, such as Accept,
 * Accept-Encoding, Accept-Language, Allow, Supported, etc.
 *
 * When a module provides new capabilities to the stack, it registers these
 * capabilities to the endpoint by supplying new tags (strings) to the
 * appropriate header fields. Application (or other modules) can then query
 * these header fields to get the list of supported capabilities, and may
 * include these headers in the outgoing message.
 *****************************************************************************
 */

/**
 * Get the value of the specified capability header field.
 *
 * @param endpt	    The endpoint.
 * @param htype	    The header type to be retrieved, which value may be:
 *		    - PJSIP_H_ACCEPT
 *		    - PJSIP_H_ALLOW
 *		    - PJSIP_H_SUPPORTED
 * @param hname	    If htype specifies PJSIP_H_OTHER, then the header name
 *		    must be supplied in this argument. Otherwise the value
 *		    must be set to NULL.
 *
 * @return	    The appropriate header, or NULL if the header is not
 *		    available.
 */
PJ_DECL(const pjsip_hdr*) pjsip_endpt_get_capability( pjsip_endpoint *endpt,
						      int htype,
						      const pj_str_t *hname);


/**
 * Check if we have the specified capability.
 *
 * @param endpt	    The endpoint.
 * @param htype	    The header type to be retrieved, which value may be:
 *		    - PJSIP_H_ACCEPT
 *		    - PJSIP_H_ALLOW
 *		    - PJSIP_H_SUPPORTED
 * @param hname	    If htype specifies PJSIP_H_OTHER, then the header name
 *		    must be supplied in this argument. Otherwise the value
 *		    must be set to NULL.
 * @param token	    The capability token to check. For example, if \a htype
 *		    is PJSIP_H_ALLOW, then \a token specifies the method
 *		    names; if \a htype is PJSIP_H_SUPPORTED, then \a token
 *		    specifies the extension names such as "100rel".
 *
 * @return	    PJ_TRUE if the specified capability is supported,
 *		    otherwise PJ_FALSE..
 */
PJ_DECL(pj_bool_t) pjsip_endpt_has_capability( pjsip_endpoint *endpt,
					       int htype,
					       const pj_str_t *hname,
					       const pj_str_t *token);


/**
 * Add or register new capabilities as indicated by the tags to the
 * appropriate header fields in the endpoint.
 *
 * @param endpt	    The endpoint.
 * @param mod	    The module which registers the capability.
 * @param htype	    The header type to be set, which value may be:
 *		    - PJSIP_H_ACCEPT
 *		    - PJSIP_H_ALLOW
 *		    - PJSIP_H_SUPPORTED
 * @param hname	    If htype specifies PJSIP_H_OTHER, then the header name
 *		    must be supplied in this argument. Otherwise the value
 *		    must be set to NULL.
 * @param count	    The number of tags in the array. The value must not
 *		    be greater than PJSIP_GENERIC_ARRAY_MAX_COUNT.
 * @param tags	    Array of tags describing the capabilities or extensions
 *		    to be added to the appropriate header.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_add_capability( pjsip_endpoint *endpt,
						 pjsip_module *mod,
						 int htype,
						 const pj_str_t *hname,
						 unsigned count,
						 const pj_str_t tags[]);

/**
 * Get list of additional headers to be put in outgoing request message.
 * Currently only Max-Forwards are defined.
 *
 * @param e	    The endpoint.
 *
 * @return	    List of headers.
 */
PJ_DECL(const pjsip_hdr*) pjsip_endpt_get_request_headers(pjsip_endpoint *e);


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
 * Register cleanup function to be called by SIP endpoint when 
 * #pjsip_endpt_destroy() is called.  Note that application should not
 * use or access any endpoint resource (such as pool, ioqueue, timer heap)
 * from within the callback as such resource may have been released when
 * the callback function is invoked.
 *
 * @param endpt		The SIP endpoint.
 * @param func		The function to be registered.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_atexit(pjsip_endpoint *endpt,
					pjsip_endpt_exit_callback func);


/**
 * @}
 */


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


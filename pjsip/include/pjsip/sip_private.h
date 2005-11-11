/* $Id$
 */
#ifndef __PJSIP_SIP_PRIVATE_H__
#define __PJSIP_SIP_PRIVATE_H__

/**
 * @file sip_private.h
 * @brief Private structures and functions for PJSIP Library.
 */ 

#include <pjsip/sip_types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_PRIVATE Private structures and functions (PJSIP internals)
 * @ingroup PJSIP
 * @{
 */


/** 
 * Create a new transport manager.
 * @param pool The pool
 * @param endpt The endpoint
 * @param cb Callback to be called to receive messages from transport.
 */
PJ_DECL(pj_status_t) pjsip_transport_mgr_create( pj_pool_t *pool,
						 pjsip_endpoint *endpt,
						 void (*cb)(pjsip_endpoint *,
							    pjsip_rx_data *),
						 pjsip_transport_mgr **);


/**
 * Destroy transport manager and release all transports.
 * @param mgr Transport manager to be destroyed.
 */
PJ_DECL(pj_status_t) pjsip_transport_mgr_destroy( pjsip_transport_mgr *mgr );

/**
 * Poll for transport events.
 * Incoming messages will be parsed by the transport manager, and the callback
 * will be called for each of this message.
 * @param endpt The endpoint.
 * @param timeout Timeout value, or NULL to wait forever.
 */
PJ_DECL(int) pjsip_transport_mgr_handle_events( pjsip_transport_mgr *mgr,
					        const pj_time_val *timeout );

/**
 * Get the pointer to the first transport iterator.
 * @param mgr The transport manager.
 * @param it  The iterator used for iterating the hash element.
 * @return the iterator to the first transport, or NULL.
 */
PJ_DECL(pj_hash_iterator_t*) pjsip_transport_first( pjsip_transport_mgr *mgr,
						    pj_hash_iterator_t *it );


/**
 * Get the next transport iterator.
 * @param itr the iterator to the transport.
 * @return the iterator pointed to the next transport, or NULL.
 */
PJ_DECL(pj_hash_iterator_t*) pjsip_transport_next( pjsip_transport_mgr *mgr,
						   pj_hash_iterator_t *itr );

/**
 * Get the value of transport iterator.
 * @param mgr the transport manager.
 * @param itr the transport iterator.
 * @return the transport associated with the iterator.
 */
PJ_DECL(pjsip_transport_t*) pjsip_transport_this( pjsip_transport_mgr *mgr,
						  pj_hash_iterator_t *itr );

/** 
 * @}
 */

PJ_END_DECL

#endif /* __PJSIP_PRIVATE_I_H__ */


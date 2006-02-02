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
#ifndef __PJMEDIA_SDP_NEG_H__
#define __PJMEDIA_SDP_NEG_H__


/**
 * @defgroup PJSDP SDP Library
 */
/**
 * @file sdp_neg.h
 * @brief SDP negotiator header file.
 */
/**
 * @defgroup PJ_SDP_NEG SDP Negotiator.
 * @ingroup PJSDP
 * @{
 *.
 */

#include <pjmedia/types.h>

PJ_BEGIN_DECL

/**
 * This enumeration describes SDP negotiation state.
 */
enum pjmedia_sdp_neg_state
{
    /** 
     * This is the state of SDP negoator before it is initialized. 
     */
    PJMEDIA_SDP_NEG_STATE_NULL,

    /** 
     * This state occurs when SDP negotiator has sent our offer to remote and
     * it is waiting for answer.
     */
    PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER,

    /**
     * This state occurs when an offer (either local or remote) has been 
     * provided with answer. The SDP negotiator is ready to negotiate both
     * session descriptors.
     */
    PJMEDIA_SDP_NEG_STATE_WAIT_NEGO,

    /**
     * This state occurs when SDP negotiation has completed, either 
     * successfully or not.
     */
    PJMEDIA_SDP_NEG_STATE_DONE,
};

/* Negotiator state:
 *
 *                                                 reinit_local_offer()
 *                                                 modify_local_offer()
 *     create_w_local_offer()     +-------------+  tx_local_offer()
 *     /------------------------->| LOCAL_OFFER |<----------------------\
 *    |                           +-------------+                        |
 *    |                                  |                               |
 *    |               rx_remote_answer() |                               |
 *    |                                  V                               |
 * +--+---+                         +-----------+     negotiate()     +------+
 * + NULL |------------------------>| WAIT_NEGO |-------------------->| DONE |
 * +------+ create_w_remote_offer() +-----------+                     +------+
 *                                       A                               |
 *                                       |         rx_remote_offer()     |
 *                                        \-----------------------------/
 */ 

/**
 * Create the SDP negotiator with local offer. The SDP negotiator then
 * will move to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER state, where it waits
 * until it receives answer from remote. When SDP answer from remote is
 * received, application should call #pjmedia_sdp_neg_rx_remote_answer().
 *
 * After calling this function, application should send the local SDP offer
 * to remote party and wait for SDP answer.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_create_w_local_offer( pj_pool_t *pool,
				      const pjmedia_sdp_session *local,
				      pjmedia_sdp_neg **p_neg);

/**
 * Initialize the SDP negotiator with both local and remote offer. 
 * Application normally calls this function when it receives initial offer
 * from remote. Application must also provide initial local offer when
 * calling this function. After this function is called, the SDP negotiator
 * state will move to PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and the negotiation
 * function can be called.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_create_w_remote_offer(pj_pool_t *pool,
				      const pjmedia_sdp_session *local,
				      const pjmedia_sdp_session *remote,
				      pjmedia_sdp_neg **p_neg);

/**
 * Get SDP negotiator state.
 */
PJ_DECL(pjmedia_sdp_neg_state)
pjmedia_sdp_neg_get_state( pjmedia_sdp_neg *neg );

/**
 * Get the currently active local SDP. Application can only call this
 * function after negotiation has been done, or otherwise there won't be
 * active SDPs. Calling this function will not change the state of the 
 * negotiator.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_get_local( pjmedia_sdp_neg *neg,
			   const pjmedia_sdp_session **local);

/**
 * Get the currently active remote SDP. Application can only call this
 * function after negotiation has been done, or otherwise there won't be
 * active SDPs. Calling this function will not change the state of the 
 * negotiator.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_get_remote( pjmedia_sdp_neg *neg,
			    const pjmedia_sdp_session **remote);


/**
 * Completely replaces local offer with new SDP. After calling
 * This function can only be called in state PJMEDIA_SDP_NEG_STATE_DONE.
 * this function, application can send the modified offer to remote.
 * The negotiator state will move to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER,
 * where it waits for SDP answer from remote.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_modify_local_offer( pj_pool_t *pool,
				    pjmedia_sdp_neg *neg,
				    const pjmedia_sdp_session *local);

/**
 * Negotiate local and remote answer. Before calling this function, the
 * SDP negotiator must be in PJMEDIA_SDP_NEG_STATE_WAIT_NEGO state.
 * After calling this function, the negotiator state will move to
 * PJMEDIA_SDP_NEG_STATE_DONE.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_neg_negotiate( pj_pool_t *pool,
					        pjmedia_sdp_neg *neg,
						pj_bool_t allow_asym);


/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_DONE state.
 * Application calls this function to retrieve currently active
 * local SDP to be sent to remote. The negotiator state will then move
 * to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER, where it waits for SDP answer
 * from remote.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_tx_local_offer( pj_pool_t *pool,
			        pjmedia_sdp_neg *neg,
				const pjmedia_sdp_session **offer);

/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_DONE state. 
 * Application calls this function when it receives SDP offer from remote.
 * After this function is called, the negotiator state will move to 
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and application can call the
 * negotiation function #pjmedia_sdp_neg_negotiate().
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_rx_remote_offer( pj_pool_t *pool,
				 pjmedia_sdp_neg *neg,
				 const pjmedia_sdp_session *remote);


/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER
 * state. Application calls this function when it receives SDP answer
 * from remote. After this function is called, the negotiator state will
 * move to PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and application can call the
 * negotiation function #pjmedia_sdp_neg_negotiate().
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_rx_remote_answer( pj_pool_t *pool,
				  pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session *remote);



PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_SDP_NEG_H__ */


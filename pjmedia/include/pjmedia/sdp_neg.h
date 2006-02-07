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
 * This enumeration describes SDP negotiation state. The negotiator state
 * is illustrated in the following diagram.
 * 
 * <pre>
 *                                              reinit_local_offer()
 *                                              modify_local_offer()
 *     create_w_local_offer()  +-------------+  send_local_offer()
 *     ----------------------->| LOCAL_OFFER |<-----------------------
 *    |                        +-------------+                        |
 *    |                               |                               |
 *    |           set_remote_answer() |                               |
 *    |                               V                               |
 * +--+---+                     +-----------+     negotiate()     +------+
 * | NULL |                     | WAIT_NEGO |-------------------->| DONE |
 * +------+                     +-----------+                     +------+
 *    |                               A                               |
 *    |            set_local_answer() |                               |
 *    |                               |                               |
 *    |                        +--------------+   set_remote_offer()  |
 *     ----------------------->| REMOTE_OFFER |<----------------------
 *     create_w_remote_offer() +--------------+
 *
 * </pre>
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
     * This state occurs when SDP negotiator has received offer from remote
     * and currently waiting for local answer.
     */
    PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER,

    /**
     * This state occurs when an offer (either local or remote) has been 
     * provided with answer. The SDP negotiator is ready to negotiate both
     * session descriptors. Application can call #pjmedia_sdp_neg_negotiate()
     * immediately to begin negotiation process.
     */
    PJMEDIA_SDP_NEG_STATE_WAIT_NEGO,

    /**
     * This state occurs when SDP negotiation has completed, either 
     * successfully or not.
     */
    PJMEDIA_SDP_NEG_STATE_DONE,
};

/**
 * Create the SDP negotiator with local offer. The SDP negotiator then
 * will move to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER state, where it waits
 * until it receives answer from remote. When SDP answer from remote is
 * received, application should call #pjmedia_sdp_neg_set_remote_answer().
 *
 * After calling this function, application should send the local SDP offer
 * to remote party using higher layer signaling protocol (e.g. SIP) and 
 * wait for SDP answer.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param local		The initial local capability.
 * @param p_neg		Pointer to receive the negotiator instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_create_w_local_offer( pj_pool_t *pool,
				      const pjmedia_sdp_session *local,
				      pjmedia_sdp_neg **p_neg);

/**
 * Initialize the SDP negotiator with remote offer, and optionally
 * specify the initial local capability, if known. Application normally 
 * calls this function when it receives initial offer
 * from remote. 
 *
 * If local media capability is specified, this capability will be set as
 * initial local capability of the negotiator, and after this function is
 * called, the SDP negotiator state will move to state
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and the negotiation function can be 
 * called. 
 *
 * If local SDP is not specified, the negotiator will not have initial local
 * capability, and after this function is called the negotiator state will 
 * move to PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER state. Application MUST supply
 * local answer later with #pjmedia_sdp_neg_set_local_answer(), before
 * calling the negotiation function.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param initial	Optional initial local capability.
 * @param remote	The remote offer.
 * @param p_neg		Pointer to receive the negotiator instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_create_w_remote_offer(pj_pool_t *pool,
				      const pjmedia_sdp_session *initial,
				      const pjmedia_sdp_session *remote,
				      pjmedia_sdp_neg **p_neg);

/**
 * Get SDP negotiator state.
 *
 * @param neg		The SDP negotiator instance.
 *
 * @return		The negotiator state.
 */
PJ_DECL(pjmedia_sdp_neg_state)
pjmedia_sdp_neg_get_state( pjmedia_sdp_neg *neg );

/**
 * Get the currently active local SDP. Application can only call this
 * function after negotiation has been done, or otherwise there won't be
 * active SDPs. Calling this function will not change the state of the 
 * negotiator.
 *
 * @param neg		The SDP negotiator instance.
 * @param local		Pointer to receive the local active SDP.
 *
 * @return		PJ_SUCCESS if local active SDP is present.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_get_active_local( pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session **local);

/**
 * Get the currently active remote SDP. Application can only call this
 * function after negotiation has been done, or otherwise there won't be
 * active SDPs. Calling this function will not change the state of the 
 * negotiator.
 *
 * @param neg		The SDP negotiator instance.
 * @param remote	Pointer to receive the remote active SDP.
 *
 * @return		PJ_SUCCESS if remote active SDP is present.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_get_active_remote( pjmedia_sdp_neg *neg,
				   const pjmedia_sdp_session **remote);

/**
 * Get the current remote SDP offer or answer. Application can only 
 * call this function in state PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER or
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, or otherwise there won't be remote 
 * SDP offer/answer. Calling this  function will not change the state 
 * of the negotiator.
 *
 * @param neg		The SDP negotiator instance.
 * @param remote	Pointer to receive the current remote offer or
 *			answer.
 *
 * @return		PJ_SUCCESS if the negotiator currently has
 *			remote offer or answer.
 */
PJ_DECL(pj_status_t)
pjmedia_sdp_neg_get_neg_remote( pjmedia_sdp_neg *neg,
				const pjmedia_sdp_session **remote);


/**
 * Get the current local SDP offer or answer. Application can only 
 * call this function in state PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER or
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, or otherwise there won't be local 
 * SDP offer/answer. Calling this function will not change the state 
 * of the negotiator.
 *
 * @param neg		The SDP negotiator instance.
 * @param local		Pointer to receive the current local offer or
 *			answer.
 *
 * @return		PJ_SUCCESS if the negotiator currently has
 *			local offer or answer.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_get_neg_local( pjmedia_sdp_neg *neg,
			       const pjmedia_sdp_session **local);

/**
 * Completely replaces local offer with new SDP. After calling
 * This function can only be called in state PJMEDIA_SDP_NEG_STATE_DONE.
 * this function, application can send the modified offer to remote.
 * The negotiator state will move to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER,
 * where it waits for SDP answer from remote.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param local		The new local SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_modify_local_offer( pj_pool_t *pool,
				    pjmedia_sdp_neg *neg,
				    const pjmedia_sdp_session *local);

/**
 * Negotiate local and remote answer. Before calling this function, the
 * SDP negotiator must be in PJMEDIA_SDP_NEG_STATE_WAIT_NEGO state.
 * After calling this function, the negotiator state will move to
 * PJMEDIA_SDP_NEG_STATE_DONE regardless whether the negotiation has
 * been successfull or not.
 *
 * If the negotiation succeeds (i.e. the return value is PJ_SUCCESS),
 * the active local and remote SDP will be replaced with the new SDP
 * from the negotiation process.
 *
 * If the negotiation fails, the active local and remote SDP will not
 * change.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param allow_asym	Should be zero.
 *
 * @return		PJ_SUCCESS when there is at least one media
 *			is actuve common in both offer and answer, or 
 *			failure code when negotiation has failed.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_neg_negotiate( pj_pool_t *pool,
					        pjmedia_sdp_neg *neg,
						pj_bool_t allow_asym);


/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_DONE state.
 * Application calls this function to retrieve currently active
 * local SDP to be sent to remote. The negotiator state will then move
 * to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER, where it waits for SDP answer
 * from remote. When SDP answer has been received from remote, application
 * must call #pjmedia_sdp_neg_set_remote_answer().
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param offer		Pointer to receive active local SDP to be
 *			offered to remote.
 *
 * @return		PJ_SUCCESS if local offer can be created.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_send_local_offer( pj_pool_t *pool,
			          pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session **offer);

/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER
 * state, i.e. after application calls #pjmedia_sdp_neg_send_local_offer()
 * function. Application calls this function when it receives SDP answer
 * from remote. After this function is called, the negotiator state will
 * move to PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and application can call the
 * negotiation function #pjmedia_sdp_neg_negotiate().
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param remote	The remote answer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_set_remote_answer( pj_pool_t *pool,
				   pjmedia_sdp_neg *neg,
				   const pjmedia_sdp_session *remote);



/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_DONE state. 
 * Application calls this function when it receives SDP offer from remote.
 * After this function is called, the negotiator state will move to 
 * PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER, and application MUST call the
 * #pjmedia_sdp_neg_set_local_answer() to set local answer before it can
 * call the negotiation function.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param remote	The remote offer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_set_remote_offer( pj_pool_t *pool,
				  pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session *remote);



/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER
 * state, i.e. after application calls #pjmedia_sdp_neg_set_remote_offer()
 * function. After this function is called, the negotiator state will
 * move to PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and application can call the
 * negotiation function #pjmedia_sdp_neg_negotiate().
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param local		Optional local answer. If negotiator has initial
 *			local capability, application can specify NULL on
 *			this argument; in this case, the negotiator will
 *			create answer by by negotiating remote offer with
 *			initial local capability. If negotiator doesn't have
 *			initial local capability, application MUST specify
 *			local answer here.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_set_local_answer( pj_pool_t *pool,
				  pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session *local);




PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_SDP_NEG_H__ */


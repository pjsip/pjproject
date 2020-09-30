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
#ifndef __PJMEDIA_TRANSPORT_ICE_H__
#define __PJMEDIA_TRANSPORT_ICE_H__


/**
 * @file transport_ice.h
 * @brief ICE capable media transport.
 */

#include <pjmedia/stream.h>
#include <pjnath/ice_strans.h>


/**
 * @defgroup PJMEDIA_TRANSPORT_ICE ICE Media Transport 
 * @ingroup PJMEDIA_TRANSPORT
 * @brief Interactive Connectivity Establishment (ICE) transport
 * @{
 *
 * This describes the implementation of media transport using
 * Interactive Connectivity Establishment (ICE) protocol.
 */

PJ_BEGIN_DECL


/**
 * Structure containing callbacks to receive ICE notifications.
 */
typedef struct pjmedia_ice_cb
{
    /**
     * This callback will be called when ICE negotiation completes.
     *
     * @param tp	PJMEDIA ICE transport.
     * @param op	The operation
     * @param status	Operation status.
     */
    void    (*on_ice_complete)(pjmedia_transport *tp,
			       pj_ice_strans_op op,
			       pj_status_t status);

    /**
     * This callback will be called when ICE negotiation completes, with
     * application user data. Note that if both callbacks are implemented,
     * only this callback will be invoked.
     *
     * @param tp	PJMEDIA ICE transport.
     * @param op	The operation
     * @param status	Operation status.
     * @param user_data	User data for this callback.
     */
    void    (*on_ice_complete2)(pjmedia_transport *tp,
			        pj_ice_strans_op op,
			        pj_status_t status,
				void *user_data);

    /**
     * Callback to report a new ICE local candidate, e.g: after successful
     * STUN Binding, after a successful TURN allocation. Only new candidates
     * whose type is server reflexive or relayed will be notified via this
     * callback. This callback also indicates end-of-candidate via parameter
     * 'last'.
     *
     * Trickle ICE can use this callback to convey the new candidate
     * to remote agent and monitor end-of-candidate indication.
     *
     * @param tp	PJMEDIA ICE transport.
     * @param cand	The new local candidate, can be NULL when the last
     *			local candidate initialization failed/timeout.
     * @param last	PJ_TRUE if this is the last of local candidate.
     */
    void    (*on_new_candidate)(pjmedia_transport *tp,
				const pj_ice_sess_cand *cand,
				pj_bool_t last);

} pjmedia_ice_cb;


/**
 * This structure specifies ICE transport specific info. This structure
 * will be filled in media transport specific info.
 */
typedef struct pjmedia_ice_transport_info
{
    /**
     * Specifies whether ICE is used, i.e. SDP offer and answer indicates
     * that both parties support ICE and ICE should be used for the session.
     */
    pj_bool_t active;

    /**
     * ICE sesion state.
     */
    pj_ice_strans_state sess_state;

    /**
     * Session role.
     */
    pj_ice_sess_role role;

    /**
     * Number of components in the component array. Before ICE negotiation
     * is complete, the number represents the number of components of the
     * local agent. After ICE negotiation has been completed successfully,
     * the number represents the number of common components between local
     * and remote agents.
     */
    unsigned comp_cnt;

    /**
     * Array of ICE components. Typically the first element denotes RTP and
     * second element denotes RTCP.
     */
    struct
    {
	/**
	 * Local candidate type.
	 */
	pj_ice_cand_type    lcand_type;

	/**
	 * The local address.
	 */
	pj_sockaddr	    lcand_addr;

	/**
	 * Remote candidate type.
	 */
	pj_ice_cand_type    rcand_type;

	/**
	 * Remote address.
	 */
	pj_sockaddr	    rcand_addr;

    } comp[2];

} pjmedia_ice_transport_info;


/**
 * Options that can be specified when creating ICE transport.
 */
enum pjmedia_transport_ice_options
{
    /**
     * Normally when remote doesn't use ICE, the ICE transport will 
     * continuously check the source address of incoming packets to see 
     * if it is different than the configured remote address, and switch 
     * the remote address to the source address of the packet if they 
     * are different after several packets are received.
     * Specifying this option will disable this feature.
     */
    PJMEDIA_ICE_NO_SRC_ADDR_CHECKING = 1
};


/**
 * Create the Interactive Connectivity Establishment (ICE) media transport
 * using the specified configuration. When STUN or TURN (or both) is used,
 * the creation operation will complete asynchronously, when STUN resolution
 * and TURN allocation completes. When the initialization completes, the
 * \a on_ice_complete() complete will be called with \a op parameter equal
 * to PJ_ICE_STRANS_OP_INIT.
 *
 * In addition, this transport will also notify the application about the
 * result of ICE negotiation, also in \a on_ice_complete() callback. In this
 * case the callback will be called with \a op parameter equal to
 * PJ_ICE_STRANS_OP_NEGOTIATION.
 *
 * Other than this, application should use the \ref PJMEDIA_TRANSPORT API
 * to manipulate this media transport.
 *
 * @param endpt		The media endpoint.
 * @param name		Optional name to identify this ICE media transport
 *			for logging purposes.
 * @param comp_cnt	Number of components to be created.
 * @param cfg		Pointer to configuration settings.
 * @param cb		Optional structure containing ICE specific callbacks.
 * @param p_tp		Pointer to receive the media transport instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_create(pjmedia_endpt *endpt,
					const char *name,
					unsigned comp_cnt,
					const pj_ice_strans_cfg *cfg,
					const pjmedia_ice_cb *cb,
					pjmedia_transport **p_tp);


/**
 * The same as #pjmedia_ice_create() with additional \a options param.
 *
 * @param endpt		The media endpoint.
 * @param name		Optional name to identify this ICE media transport
 *			for logging purposes.
 * @param comp_cnt	Number of components to be created.
 * @param cfg		Pointer to configuration settings.
 * @param cb		Optional structure containing ICE specific callbacks.
 * @param options	Options, see #pjmedia_transport_ice_options.
 * @param p_tp		Pointer to receive the media transport instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_create2(pjmedia_endpt *endpt,
					 const char *name,
					 unsigned comp_cnt,
					 const pj_ice_strans_cfg *cfg,
					 const pjmedia_ice_cb *cb,
					 unsigned options,
					 pjmedia_transport **p_tp);

/**
 * The same as #pjmedia_ice_create2() with additional \a user_data param.
 *
 * @param endpt		The media endpoint.
 * @param name		Optional name to identify this ICE media transport
 *			for logging purposes.
 * @param comp_cnt	Number of components to be created.
 * @param cfg		Pointer to configuration settings.
 * @param cb		Optional structure containing ICE specific callbacks.
 * @param options	Options, see #pjmedia_transport_ice_options.
 * @param user_data	User data to be attached to the transport.
 * @param p_tp		Pointer to receive the media transport instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_create3(pjmedia_endpt *endpt,
					 const char *name,
					 unsigned comp_cnt,
					 const pj_ice_strans_cfg *cfg,
					 const pjmedia_ice_cb *cb,
					 unsigned options,
					 void *user_data,
					 pjmedia_transport **p_tp);

/**
 * Get the group lock for the ICE media transport.
 *
 * @param tp	        The ICE media transport.
 *
 * @return		The group lock.
 */
PJ_DECL(pj_grp_lock_t *) pjmedia_ice_get_grp_lock(pjmedia_transport *tp);


/**
 * Add application to receive ICE notifications from the specified ICE media
 * transport.
 *
 * @param tp	        The ICE media transport.
 * @param cb	        The ICE specific callbacks.
 * @param user_data     Optional application user data.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_add_ice_cb(pjmedia_transport *tp,
					    const pjmedia_ice_cb *cb,
					    void *user_data);


/**
 * Remove application to stop receiving ICE notifications from the specified
 * ICE media transport.
 *
 * @param tp	        The ICE media transport.
 * @param cb	        The ICE specific callbacks.
 * @param user_data     Optional application user data. The same user data
 *			passed to pjmedia_ice_add_ice_cb(), this is for
 *			validation purpose.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_remove_ice_cb(pjmedia_transport *tp,
					       const pjmedia_ice_cb *cb,
					       void *user_data);


/**
 * Check if trickle support is signalled in the specified SDP. This function
 * will check trickle indication in the media level first, if not found, it
 * will check in the session level.
 *
 * @param sdp		The SDP.
 * @param med_idx	The media index to be checked.
 *
 * @return		PJ_TRUE if trickle ICE indication is found.
 */
PJ_DECL(pj_bool_t) pjmedia_ice_sdp_has_trickle(const pjmedia_sdp_session *sdp,
					       unsigned med_idx);


/**
 * Update check list after new local or remote ICE candidates are added,
 * or signal ICE session that trickling is done. Application typically would
 * call this function after finding (and conveying) new local ICE candidates
 * to remote, after receiving remote ICE candidates, or after receiving
 * end-of-candidates indication.
 *
 * After check list is updated, ICE connectivity check will automatically
 * start if check list has any candidate pair.
 *
 * This function is only applicable when trickle ICE is not disabled and
 * after ICE connectivity checks are started, i.e: after
 * pjmedia_transport_media_start() has been invoked.
 *
 * @param tp		The ICE media transport.
 * @param rem_ufrag	Remote ufrag, as seen in the SDP received from
 *			the remote agent.
 * @param rem_passwd	Remote password, as seen in the SDP received from
 *			the remote agent.
 * @param rcand_cnt	Number of new remote candidates in the array.
 * @param rcand		New remote candidates array.
 * @param trickle_done	Flag to indicate end of trickling, set to PJ_TRUE
 *			after all local candidates have been gathered and
 *			after receiving end-of-candidate indication from
 *			remote.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_trickle_update(
					    pjmedia_transport *tp,
					    const pj_str_t *rem_ufrag,
					    const pj_str_t *rem_passwd,
					    unsigned rcand_cnt,
					    const pj_ice_sess_cand rcand[],
					    pj_bool_t trickle_done);


/**
 * Decode trickle ICE info from the specified SDP.
 *
 * @param sdp		The SDP containing trickle ICE info.
 * @param media_index	The media index.
 * @param mid		Output, media ID.
 * @param ufrag		Output, ufrag.
 * @param passwd	Output, password.
 * @param cand_cnt	On input, maximum number of candidate array.
 *			On output, the number of candidates.
 * @param cand		Output, the candidates.
 * @param end_of_cand	Output, end of candidate indication.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_trickle_decode_sdp(
					    const pjmedia_sdp_session *sdp,
					    unsigned media_index,
					    pj_str_t *mid,
					    pj_str_t *ufrag,
					    pj_str_t *passwd,
					    unsigned *cand_cnt,
					    pj_ice_sess_cand cand[],
					    pj_bool_t *end_of_cand);


/**
 * Encode trickle ICE info into the specified SDP. This function may generate
 * the following SDP attributes:
 * - Media ID, "a=mid", currently this is the media_index.
 * - ICE ufrag & password, "a=ice-ufrag" & "a=ice-pwd".
 * - Trickle ICE support indication, "a=ice-options:trickle".
 * - ICE candidates, "a=candidate".
 * - End of candidate indication, "a=end-of-candidates".
 *
 * @param sdp_pool	The memory pool for generating SDP attributes.
 * @param sdp		The SDP to be updated.
 * @param media_index	The media index.
 * @param ufrag		The ufrag, optional.
 * @param passwd	The password, optional.
 * @param cand_cnt	The number of local candidates, can be zero.
 * @param cand		The local candidates.
 * @param end_of_cand	End of candidate indication.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_trickle_encode_sdp(
					    pj_pool_t *sdp_pool,
					    pjmedia_sdp_session *sdp,
					    unsigned media_index,
					    const pj_str_t *ufrag,
					    const pj_str_t *passwd,
					    unsigned cand_cnt,
					    const pj_ice_sess_cand cand[],
					    pj_bool_t end_of_cand);


PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_ICE_H__ */



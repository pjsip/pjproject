/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __pjmedia_ice_H__
#define __pjmedia_ice_H__


/**
 * @file transport_ice.h
 * @brief ICE capable media transport.
 */

#include <pjmedia/stream.h>
#include <pjnath/ice_strans.h>


/**
 * @defgroup PJMEDIA_TRANSPORT_ICE ICE Capable media transport 
 * @ingroup PJMEDIA_TRANSPORT
 * @brief Implementation of media transport with ICE.
 * @{
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
     * @param status	ICE negotiation result, PJ_SUCCESS on success.
     */
    void    (*on_ice_complete)(pjmedia_transport *tp,
			       pj_status_t status);

} pjmedia_ice_cb;

/**
 * Create the media transport.
 *
 * @param endpt		The media endpoint.
 * @param name		Optional name to identify this ICE media transport
 *			for logging purposes.
 * @param comp_cnt	Number of components to be created.
 * @param stun_cfg	Pointer to STUN configuration settings.
 * @param cb		Optional callbacks.
 * @param p_tp		Pointer to receive the media transport instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_create(pjmedia_endpt *endpt,
					const char *name,
					unsigned comp_cnt,
					pj_stun_config *stun_cfg,
					const pjmedia_ice_cb *cb,
					pjmedia_transport **p_tp);

/**
 * Destroy the media transport.
 *
 * @param tp		The media transport.
 *
 * @return		PJ_SUCCESS.
 */
PJ_DECL(pj_status_t) pjmedia_ice_destroy(pjmedia_transport *tp);


/**
 * Start the initialization process of this media transport. This function
 * will gather the transport addresses to be registered to ICE session as
 * candidates. If STUN is configured, this will start the STUN Binding or
 * Allocate request to get the STUN server reflexive or relayed address.
 * This function will return immediately, and application should poll the
 * STUN completion status by calling #pjmedia_ice_get_init_status().
 *
 * @param tp		The media transport.
 * @param options	Options, see pj_ice_strans_option in PJNATH 
 *			documentation.
 * @param start_addr	Local address where socket will be bound to. This
 *			address will be used as follows:
 *			- if the value is NULL, then socket will be bound
 *			  to any available port.
 *			- if the value is not NULL, then if the port number
 *			  is not zero, it will used as the starting port 
 *			  where the socket will be bound to. If bind() to
 *			  this port fails, this function will try to bind
 *			  to port+2, repeatedly until it succeeded.
 *			  If application doesn't want this function to 
 *			  retry binding the socket to other port, it can
 *			  specify PJ_ICE_ST_OPT_NO_PORT_RETRY option.
 *			- if the value is not NULL, then if the address
 *			  is not INADDR_ANY, this function will bind the
 *			  socket to this particular interface only, and
 *			  no other host candidates will be added for this
 *			  socket.
 * @param stun_srv	Address of the STUN server, or NULL if STUN server
 *			reflexive mapping is not to be used.
 * @param turn_srv	Address of the TURN server, or NULL if TURN relay
 *			is not to be used.
 *
 * @return		PJ_SUCCESS when the initialization process has started
 *			successfully, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_start_init(pjmedia_transport *tp,
					    unsigned options,
					    const pj_sockaddr_in *start_addr,
					    const pj_sockaddr_in *stun_srv,
					    const pj_sockaddr_in *turn_srv);

/**
 * Poll the initialization status of this media transport.
 *
 * @param tp		The media transport.
 *
 * @return		PJ_SUCCESS if all candidates have been resolved
 *			successfully, PJ_EPENDING if transport resolution
 *			is still in progress, or other status on failure.
 */
PJ_DECL(pj_status_t) pjmedia_ice_get_init_status(pjmedia_transport *tp);


/**
 * Get the ICE stream transport component for the specified component ID.
 *
 * @param tp		The media transport.
 * @param comp_id	The component ID.
 * @param comp		The structure which will be filled with the
 *			component.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_get_comp(pjmedia_transport *tp,
					  unsigned comp_id,
					  pj_ice_strans_comp *comp);

/**
 * Initialize the ICE session.
 *
 * @param tp		The media transport.
 * @param role		ICE role.
 * @param local_ufrag	Optional local username fragment.
 * @param local_passwd	Optional local password.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.

 */
PJ_DECL(pj_status_t) pjmedia_ice_init_ice(pjmedia_transport *tp,
					  pj_ice_sess_role role,
					  const pj_str_t *local_ufrag,
					  const pj_str_t *local_passwd);

/**
 * Modify the SDP to add ICE specific SDP attributes before sending
 * the SDP to remote host.
 *
 * @param tp		The media transport.
 * @param pool		Pool to allocate memory for the SDP elements.
 * @param sdp		The SDP descriptor to be modified.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_modify_sdp(pjmedia_transport *tp,
					    pj_pool_t *pool,
					    pjmedia_sdp_session *sdp);

/**
 * Start ICE connectivity checks.
 *
 * This function will pair the local and remote candidates to create 
 * check list. Once the check list is created and sorted based on the
 * priority, ICE periodic checks will be started. This function will 
 * return immediately, and application will be notified about the 
 * connectivity check status in the callback.
 *
 * @param tp		The media transport.
 * @param pool		Memory pool to parse the SDP.
 * @param rem_sdp	The SDP received from remote agent.
 * @param media_index	The media index (in SDP) to process.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_start_ice(pjmedia_transport *tp,
					   pj_pool_t *pool,
					   const pjmedia_sdp_session *rem_sdp,
					   unsigned media_index);

/**
 * Stop the ICE session (typically when the call is terminated). Application
 * may restart the ICE session again by calling #pjmedia_ice_init_ice(),
 * for example to use this media transport for the next call.
 *
 * @param tp		The media transport.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_stop_ice(pjmedia_transport *tp);


/**
 * Simulate packet lost in the specified direction (for testing purposes).
 * When enabled, the transport will randomly drop packets to the specified
 * direction.
 *
 * @param tp	    The ICE media transport.
 * @param dir	    Media direction to which packets will be randomly dropped.
 * @param pct_lost  Percent lost (0-100). Set to zero to disable packet
 *		    lost simulation.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_ice_simulate_lost(pjmedia_transport *tp,
					       pjmedia_dir dir,
					       unsigned pct_lost);




PJ_END_DECL


/**
 * @}
 */


#endif	/* __pjmedia_ice_H__ */



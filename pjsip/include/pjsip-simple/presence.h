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
#ifndef __PJSIP_SIMPLE_PRESENCE_H__
#define __PJSIP_SIMPLE_PRESENCE_H__

/**
 * @file presence.h
 * @brief SIP Extension for Presence (RFC 3856)
 */
#include <pjsip-simple/evsub.h>
#include <pjsip-simple/pidf.h>
#include <pjsip-simple/xpidf.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_SIMPLE_PRES SIP Extension for Presence (RFC 3856)
 * @ingroup PJSIP_SIMPLE
 * @{
 *
 * This module contains the implementation of SIP Presence Extension as 
 * described in RFC 3856. It uses the SIP Event Notification framework
 * (evsub.h) and extends the framework by implementing "presence"
 * event package.
 */



/**
 * Initialize the presence module and register it as endpoint module and
 * package to the event subscription module.
 *
 * @param endpt		The endpoint instance.
 * @param mod_evsub	The event subscription module instance.
 *
 * @return		PJ_SUCCESS if the module is successfully 
 *			initialized and registered to both endpoint
 *			and the event subscription module.
 */
PJ_DECL(pj_status_t) pjsip_pres_init_module(pjsip_endpoint *endpt,
					    pjsip_module *mod_evsub);


/**
 * Get the presence module instance.
 *
 * @return		The presence module instance.
 */
PJ_DECL(pjsip_module*) pjsip_pres_instance(void);


#define PJSIP_PRES_STATUS_MAX_INFO  8

/**
 * This structure describes presence status of a presentity.
 */
struct pjsip_pres_status
{
    unsigned		info_cnt;	/**< Number of info in the status.  */
    struct {

	pj_bool_t	basic_open;	/**< Basic status/availability.	    */
	pj_str_t	id;		/**< Tuple id.			    */
	pj_str_t	contact;	/**< Optional contact address.	    */

    } info[PJSIP_PRES_STATUS_MAX_INFO];	/**< Array of info.		    */

    pj_bool_t		_is_valid;	/**< Internal flag.		    */
};


/**
 * @see pjsip_pres_status
 */
typedef struct pjsip_pres_status pjsip_pres_status;


/**
 * Create presence client subscription session.
 *
 * @param dlg		The underlying dialog to use.
 * @param user_cb	Pointer to callbacks to receive presence subscription
 *			events.
 * @param p_evsub	Pointer to receive the presence subscription
 *			session.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_create_uac( pjsip_dialog *dlg,
					    const pjsip_evsub_user *user_cb,
					    pjsip_evsub **p_evsub );


/**
 * Create presence server subscription session.
 *
 * @param dlg		The underlying dialog to use.
 * @param user_cb	Pointer to callbacks to receive presence subscription
 *			events.
 * @param rdata		The incoming SUBSCRIBE request that creates the event 
 *			subscription.
 * @param p_evsub	Pointer to receive the presence subscription
 *			session.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_create_uas( pjsip_dialog *dlg,
					    const pjsip_evsub_user *user_cb,
					    pjsip_rx_data *rdata,
					    pjsip_evsub **p_evsub );


/**
 * Call this function to create request to initiate presence subscription, to 
 * refresh subcription, or to request subscription termination.
 *
 * @param sub		Client subscription instance.
 * @param expires	Subscription expiration. If the value is set to zero,
 *			this will request unsubscription.
 * @param p_tdata	Pointer to receive the request.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_initiate( pjsip_evsub *sub,
					  pj_int32_t expires,
					  pjsip_tx_data **p_tdata);



/**
 * Accept the incoming subscription request by sending 2xx response to
 * incoming SUBSCRIBE request.
 *
 * @param sub		Server subscription instance.
 * @param rdata		The incoming subscription request message.
 * @param st_code	Status code, which MUST be final response.
 * @param hdr_list	Optional list of headers to be added in the response.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_accept( pjsip_evsub *sub,
					pjsip_rx_data *rdata,
				        int st_code,
					const pjsip_hdr *hdr_list );




/**
 * For notifier, create NOTIFY request to subscriber, and set the state 
 * of the subscription. Application MUST set the presence status to the
 * appropriate state (by calling #pjsip_pres_set_status()) before calling
 * this function.
 *
 * @param sub		The server subscription (notifier) instance.
 * @param state		New state to set.
 * @param state_str	The state string name, if state contains value other
 *			than active, pending, or terminated. Otherwise this
 *			argument is ignored.
 * @param reason	Specify reason if new state is terminated, otherwise
 *			put NULL.
 * @param p_tdata	Pointer to receive the request.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_notify( pjsip_evsub *sub,
					pjsip_evsub_state state,
					const pj_str_t *state_str,
					const pj_str_t *reason,
					pjsip_tx_data **p_tdata);


/**
 * Create NOTIFY request to reflect current subscription status.
 *
 * @param sub		Server subscription object.
 * @param p_tdata	Pointer to receive request.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_current_notify( pjsip_evsub *sub,
					        pjsip_tx_data **p_tdata );



/**
 * Send request.
 *
 * @param sub		The subscription object.
 * @param tdata		Request message to be sent.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_send_request( pjsip_evsub *sub,
					      pjsip_tx_data *tdata );


/**
 * Get the presence status. Client normally would call this function
 * after receiving NOTIFY request from server.
 *
 * @param sub		The client or server subscription.
 * @param status	The structure to receive presence status.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_get_status( pjsip_evsub *sub,
					    pjsip_pres_status *status );


/**
 * Set the presence status. This operation is only valid for server
 * subscription. After calling this function, application would need to
 * send NOTIFY request to client.
 *
 * @param sub		The server subscription.
 * @param status	Status to be set.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_pres_set_status( pjsip_evsub *sub,
					    const pjsip_pres_status *status );


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_PRESENCE_H__ */

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
#include <pjsip_simple/event_notify.h>
#include <pjsip_simple/pidf.h>
#include <pjsip_simple/xpidf.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_SIMPLE_PRES SIP Extension for Presence (RFC 3856)
 * @ingroup PJSIP_SIMPLE
 * @{
 *
 * This module contains the implementation of SIP Presence Extension as 
 * described in RFC 3856. It uses the SIP Event Notification framework
 * (event_notify.h) and extends the framework by implementing "presence"
 * event package.
 */

/**
 * Presence message body type.
 */
typedef enum pjsip_pres_type
{
    PJSIP_PRES_TYPE_PIDF,
    PJSIP_PRES_TYPE_XPIDF,
} pjsip_pres_type;

/**
 * This structure describe a presentity, for both subscriber and notifier.
 */
typedef struct pjsip_presentity
{
    pjsip_event_sub *sub;	    /**< Event subscribtion record.	*/
    pjsip_pres_type  pres_type;	    /**< Presentity type.		*/
    pjsip_msg_body  *uas_body;	    /**< Message body (UAS only).	*/
    union {
	pjpidf_pres *pidf;
	pjxpidf_pres *xpidf;
    }		     uas_data;	    /**< UAS data.			*/
    pj_str_t	     timestamp;	    /**< Time of last update.		*/
    void	    *user_data;	    /**< Application data.		*/
} pjsip_presentity;


/**
 * This structure describe callback that is registered to receive notification
 * from the presence module.
 */
typedef struct pjsip_presence_cb
{
    /**
     * This callback is first called when the module receives incoming 
     * SUBSCRIBE request to determine whether application wants to accept
     * the request. If it does, then on_presence_request will be called.
     *
     * @param rdata	The received message.
     * @return		Application should return 2xx to accept the request,
     *			or failure status (>=300) to reject the request.
     */
    void (*accept_presence)(pjsip_rx_data *rdata, int *status);

    /**
     * This callback is called when the module receive the first presence
     * subscription request.
     *
     * @param pres	The presence descriptor.
     * @param rdata	The incoming request.
     * @param timeout	Timeout to be set for incoming request. Otherwise
     *			app can just leave this and accept the default.
     */
    void (*on_received_request)(pjsip_presentity *pres, pjsip_rx_data *rdata,
				int *timeout);

    /**
     * This callback is called when the module received subscription refresh
     * request.
     *
     * @param pres	The presence descriptor.
     * @param rdata	The incoming request.
     */
    void (*on_received_refresh)(pjsip_presentity *pres, pjsip_rx_data *rdata);

    /**
     * This callback is called when the module receives incoming NOTIFY
     * request.
     *
     * @param pres	The presence descriptor.
     * @param open	The latest status of the presentity.
     */
    void (*on_received_update)(pjsip_presentity *pres, pj_bool_t open);

    /**
     * This callback is called when the subscription has terminated.
     *
     * @param sub	The subscription instance.
     * @param reason	The termination reason.
     */
    void (*on_terminated)(pjsip_presentity *pres, const pj_str_t *reason);

} pjsip_presence_cb;


/**
 * Initialize the presence module and register callback.
 *
 * @param cb		Callback structure.
 */
PJ_DECL(void) pjsip_presence_init(const pjsip_presence_cb *cb);


/**
 * Create to presence subscription of a presentity URL.
 *
 * @param endpt		Endpoint instance.
 * @param local_url	Local URL.
 * @param remote_url	Remote URL which the presence is being subscribed.
 * @param expires	The expiration.
 * @param user_data	User data to attach to presence subscription.
 *
 * @return		The presence structure if successfull, or NULL if
 *			failed.
 */
PJ_DECL(pjsip_presentity*) pjsip_presence_create( pjsip_endpoint *endpt,
						  const pj_str_t *local_url,
						  const pj_str_t *remote_url,
						  int expires,
						  void *user_data );

/**
 * Set credentials to be used by this presentity for outgoing requests.
 *
 * @param pres		Presentity instance.
 * @param count		Number of credentials in the array.
 * @param cred		Array of credentials.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_presence_set_credentials( pjsip_presentity *pres,
						     int count,
						     const pjsip_cred_info cred[]);

/**
 * Set route set for outgoing requests.
 *
 * @param pres		Presentity instance.
 * @param route_set	List of route headers.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_presence_set_route_set( pjsip_presentity *pres,
						   const pjsip_route_hdr *hdr );

/**
 * Send SUBSCRIBE request for the specified presentity.
 *
 * @param pres		The presentity instance.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_presence_subscribe( pjsip_presentity *pres );

/**
 * Ceased the presence subscription.
 *
 * @param pres		The presence structure.
 * 
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_presence_unsubscribe( pjsip_presentity *pres );

/**
 * Notify subscriber about change in local status.
 *
 * @param pres		The presence structure.
 * @param state		Set the state of the subscription.
 * @param open		Set the presence status (open or closed).
 *
 * @return		Zero if a NOTIFY request can be sent.
 */
PJ_DECL(pj_status_t) pjsip_presence_notify( pjsip_presentity *pres,
					    pjsip_event_sub_state state,
					    pj_bool_t open );

/**
 * Destroy presence structure and the underlying subscription.
 *
 * @param pres		The presence structure.
 *
 * @return		Zero if the subscription was destroyed, or one if
 *			the subscription can not be destroyed immediately
 *			and will be destroyed later, or -1 if failed.
 */
PJ_DECL(pj_status_t) pjsip_presence_destroy( pjsip_presentity *pres );


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_PRESENCE_H__ */

/* $Header: /pjproject/pjsip/src/pjsip_simple/event_notify.h 7     8/31/05 9:05p Bennylp $ */
#ifndef __PJSIP_SIMPLE_EVENT_NOTIFY_H__
#define __PJSIP_SIMPLE_EVENT_NOTIFY_H__

/**
 * @file event_notify.h
 * @brief SIP Specific Event Notification Extension (RFC 3265)
 */

#include <pjsip/sip_types.h>
#include <pjsip/sip_auth.h>
#include <pjsip_simple/event_notify_msg.h>
#include <pj/timer.h>

/**
 * @defgroup PJSIP_EVENT_NOT SIP Event Notification (RFC 3265) Module
 * @ingroup PJSIP_SIMPLE
 * @{
 *
 * This module provides the implementation of SIP Extension for SIP Specific
 * Event Notification (RFC 3265). It extends PJSIP by supporting SUBSCRIBE and
 * NOTIFY methods.
 *
 * This module itself is extensible; new event packages can be registered to
 * this module to handle specific extensions (such as presence).
 */

PJ_BEGIN_DECL

typedef struct pjsip_event_sub_cb pjsip_event_sub_cb;
typedef struct pjsip_event_sub pjsip_event_sub;

/**
 * This enumeration describes subscription state as described in the RFC 3265.
 * The standard specifies that extensions may define additional states. In the
 * case where the state is not known, the subscription state will be set to
 * PJSIP_EVENT_SUB_STATE_UNKNOWN, and the token will be kept in state_str
 * member of the susbcription structure.
 */
typedef enum pjsip_event_sub_state
{
    /** State is NULL. */
    PJSIP_EVENT_SUB_STATE_NULL,

    /** Subscription is active. */
    PJSIP_EVENT_SUB_STATE_ACTIVE,

    /** Subscription is pending. */
    PJSIP_EVENT_SUB_STATE_PENDING,

    /** Subscription is terminated. */
    PJSIP_EVENT_SUB_STATE_TERMINATED,

    /** Subscription state can not be determined. Application can query
     *  the state information in state_str member.
     */
    PJSIP_EVENT_SUB_STATE_UNKNOWN,

} pjsip_event_sub_state;

/**
 * This structure describes notification to be called when incoming SUBSCRIBE
 * request is received. The module will call the callback registered by package
 * that matches the event description in the incoming SUBSCRIBE.
 */
typedef struct pjsip_event_sub_pkg_cb
{
    /**
     * This callback is called to first enquery the package whether it wants
     * to accept incoming SUBSCRIBE request. If it does, then on_subscribe
     * will be called.
     *
     * @param rdata	The incoming request.
     * @param status	The status code to be returned back to subscriber.
     */
    void (*on_query_subscribe)(pjsip_rx_data *rdata, int *status);

    /**
     * This callback is called when the module receives incoming SUBSCRIBE
     * request.
     *
     * @param sub	The subscription instance.
     * @param rdata	The received buffer.
     * @param cb	Callback to be registered to the subscription instance.
     * @param expires	The expiration to be set.
     */
    void (*on_subscribe)(pjsip_event_sub *sub, pjsip_rx_data *rdata,
			 pjsip_event_sub_cb **cb, int *expires);

} pjsip_event_sub_pkg_cb;

/**
 * This structure describes callback that is registered by application or
 * package to receive notifications about a subscription.
 */
struct pjsip_event_sub_cb
{
    /**
     * This callback is used by both subscriber and notifier. It is called 
     * when the subscription has been terminated.
     *
     * @param sub	The subscription instance.
     * @param reason	The termination reason.
     */
    void (*on_sub_terminated)(pjsip_event_sub *sub, const pj_str_t *reason);

    /**
     * This callback is called when we received SUBSCRIBE request to refresh
     * the subscription.
     *
     * @param sub	The subscription instance.
     * @param rdata	The received SUBSCRIBE request.
     */
    void (*on_received_refresh)(pjsip_event_sub *sub, pjsip_rx_data *rdata);

    /**
     * This callback is called when the module receives final response on
     * previously sent SUBSCRIBE request.
     *
     * @param sub	The subscription instance.
     * @param event	The event.
     */
    void (*on_received_sub_response)(pjsip_event_sub *sub, pjsip_event *event);

    /**
     * This callback is called when the module receives incoming NOTIFY
     * request.
     *
     * @param sub	The subscription instance.
     * @param rdata	The received data.
     */
    void (*on_received_notify)(pjsip_event_sub *sub, pjsip_rx_data *rdata);

    /**
     * This callback is called when the module receives final response to
     * previously sent NOTIFY request.
     *
     * @param sub	The subscription instance.
     * @param event	The event.
     */
    void (*on_received_notify_response)(pjsip_event_sub *sub, pjsip_event *event);

};

/**
 * This structure describes an event subscription record. The structure is used
 * to represent both subscriber and notifier.
 */
struct pjsip_event_sub
{
    pj_pool_t		*pool;		    /**< Pool. */
    pjsip_endpoint	*endpt;		    /**< Endpoint. */
    pjsip_event_sub_cb	 cb;		    /**< Callback. */
    pj_mutex_t		*mutex;		    /**< Mutex. */
    pjsip_role_e	 role;		    /**< Role (UAC=subscriber, UAS=notifier) */
    pjsip_event_sub_state state;	    /**< Subscription state. */
    pj_str_t		 state_str;	    /**< String describing the state. */
    pjsip_from_hdr	*from;		    /**< Cached local info (From) */
    pjsip_to_hdr	*to;		    /**< Cached remote info (To) */
    pjsip_contact_hdr	*contact;	    /**< Cached local contact. */
    pjsip_cid_hdr	*call_id;	    /**< Cached Call-ID */
    int			 cseq;		    /**< Outgoing CSeq */
    pjsip_event_hdr	*event;		    /**< Event description. */
    pjsip_expires_hdr	*uac_expires;	    /**< Cached Expires header (UAC only). */
    pjsip_accept_hdr	*local_accept;	    /**< Local Accept header. */
    pjsip_route_hdr	 route_set;	    /**< Route-set. */

    pj_str_t		 key;		    /**< Key in the hash table. */
    void		*user_data;	    /**< Application data. */
    int			 default_interval;  /**< Refresh interval. */
    pj_timer_entry	 timer;		    /**< Internal timer. */
    pj_time_val		 expiry_time;	    /**< Time when subscription expires. */
    int			 pending_tsx;	    /**< Number of pending transactions. */
    pj_bool_t		 delete_flag;	    /**< Pending deletion flag. */

    pjsip_auth_session	 auth_sess;	    /**< Authorization sessions.	*/
    unsigned		 cred_cnt;	    /**< Number of credentials.		*/
    pjsip_cred_info	*cred_info;	    /**< Array of credentials.		*/
};




/**
 * Initialize the module and get the instance of the module to be registered to
 * endpoint.
 *
 * @return		The module instance.
 */
PJ_DECL(pjsip_module*) pjsip_event_sub_get_module(void);


/**
 * Register event package.
 *
 * @param event		The event identification for the package.
 * @param accept_cnt	Number of strings in Accept array.
 * @param accept	Array of Accept value.
 * @param cb		Callback to receive incoming SUBSCRIBE for the package.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_event_sub_register_pkg( const pj_str_t *event_name,
						   int accept_cnt,
						   const pj_str_t accept[],
						   const pjsip_event_sub_pkg_cb *cb );


/**
 * Create initial subscription instance (client).
 *
 * @param endpt		The endpoint.
 * @param from		URL to put in From header.
 * @param to		The target resource.
 * @param event		Event package.
 * @param expires	Expiration time.
 * @param accept	Accept specification.
 * @param user_data	Application data to attach to this subscription.
 *
 * @return		New client subscription instance.
 */
PJ_DECL(pjsip_event_sub*) pjsip_event_sub_create( pjsip_endpoint *endpt,
						  const pj_str_t *from,
						  const pj_str_t *to,
						  const pj_str_t *event,
						  int expires,
						  int accept_cnt,
						  const pj_str_t accept[],
						  void *user_data,
						  const pjsip_event_sub_cb *cb);

/**
 * Set credentials to be used for outgoing request messages.
 *
 * @param sub		Subscription instance.
 * @param count		Number of credentials.
 * @param cred		Array of credential info.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_event_sub_set_credentials( pjsip_event_sub *sub,
						      int count,
						      const pjsip_cred_info cred[]);

/**
 * Set route set for outgoing requests.
 *
 * @param sub		Subscription instance.
 * @param route_set	List of route headers.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_event_sub_set_route_set( pjsip_event_sub *sub,
						    const pjsip_route_hdr *route_set );


/**
 * Send SUBSCRIBE request.
 *
 * @param sub		Subscription instance.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_event_sub_subscribe( pjsip_event_sub *sub );

/**
 * Terminate subscription (client). This will send unsubscription request to
 * notifier.
 *
 * @param sub		Client subscription instance.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_event_sub_unsubscribe( pjsip_event_sub *sub );


/**
 * For notifier, send NOTIFY request to subscriber, and set the state of
 * the subscription.
 *
 * @param sub		The server subscription (notifier) instance.
 * @param state		New state to set.
 * @param reason	Specify reason if new state is terminated, otherwise
 *			put NULL.
 * @param type		Description of content type.
 * @param body		Text body to send with the NOTIFY, or NULL if the
 *			NOTIFY request should not contain any message body.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_event_sub_notify( pjsip_event_sub *sub,
					     pjsip_event_sub_state state,
					     const pj_str_t *reason,
					     pjsip_msg_body *body);

/**
 * Destroy subscription instance.
 *
 * @param sub		The client or server subscription instance.
 *
 * @return		Zero on success, one if the subscription will be
 *			deleted automatically later, or -1 on error.
 */
PJ_DECL(pj_status_t) pjsip_event_sub_destroy(pjsip_event_sub *sub);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSIP_SIMPLE_EVENT_NOTIFY_H__ */

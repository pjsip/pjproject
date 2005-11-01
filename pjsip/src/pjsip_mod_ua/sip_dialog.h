/* $Header: /pjproject/pjsip/src/pjsip_mod_ua/sip_dialog.h 13    8/31/05 9:05p Bennylp $ */
#ifndef __PJSIP_DIALOG_H__
#define __PJSIP_DIALOG_H__

/**
 * @file dialog.h
 * @brief SIP Dialog abstraction
 */

#include <pjsip/sip_msg.h>
#include <pjsip/sip_auth.h>
#include <pj/sock.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSUA_DIALOG SIP Dialog
 * @ingroup PJSUA
 * @{
 * \brief
 *   This file contains SIP dialog, a higher level abstraction of SIP session.
 *
 * \par Overview
 * A SIP dialog is an abstraction of communication session between two user 
 * agents that persist for some time. The dialog facilitates sequencing of 
 * messages between the user agents and proper routing of requests between both
 * of them. The dialog represents a context in which to interpret SIP messages.
 * However method independent User Agent processing for requests and responses 
 * outside of a dialog exists, hence a dialog is not necessary for message 
 * processing.
 *
 * A dialog is identified at each User Agent with a dialog Id, which consists 
 * of a Call-Id value, a local tag and a remote tag. 
 *
 * A dialog contains certain pieces of data needed for further message 
 * transmissions within the dialog. This data consists of:
 *  - Dialog Id - used to identify the dialog.
 *  - Local sequence number - used to order requests from the UA to its peer.
 *  - Remote sequence number - used to order requests from its peer to the UA.
 *  - Local URI - the address of the local party.
 *  - Remote URI - the address of the remote party.
 *  - Remote target - the address from the Contact header field of the request 
 *    or response or refresh request or response.
 *  - "secure" boolean - determines if the dialog is secure.
 *  - Route set - an ordered list of URIs. The route set is the list of servers
 *    that need to be traversed to send a request to the peer. 
 *  - Authentication info - array of authentication credentials to be used
 *    by the dialog to authenticate to proxies and servers.
 *
 * \par Manipulating Dialog
 * Application should use functions declared in this file to do something with 
 * the dialog. Among other things, application can:
 *   - create outgoing dialog (#pjsip_dlg_init()).
 *   - sends outgoing invitation (#pjsip_dlg_invite()).
 *   - sends response (provisional and final) to incoming invitation
 *     (#pjsip_dlg_answer())
 *   - disconnect dialog (#pjsip_dlg_disconnect()).
 *   - send other request (#pjsip_dlg_create_request() and #pjsip_dlg_send_msg())
 *
 * \par Getting Dialog's Notification
 * Dialog emits notification about various things that's happening to it (e.g.
 * a message is received, dialog state has changed, etc.). Normally it is in
 * the interest of the application to capture these notifications, by
 * supplying the function to be called when the event occurs in #pjsip_dlg_callback
 * structure, and register this structure to user agent by calling 
 * #pjsip_ua_set_dialog_callback().
 *
 * \par Incoming Invitation
 * Upon receiving a new incoming invitation, user agent will automatically create
 * a new dialog, and inform application via \b pjsip_dlg_callback.
 */

/** Forward declaration for user agent structure. */
typedef struct pjsip_user_agent pjsip_user_agent;

/** Forward declaration for dialog structure. */
typedef struct pjsip_dlg pjsip_dlg;

/**
 * \brief Type of events that are reported by the dialog to the application callback
 * function.
 */
typedef enum pjsip_dlg_event_e
{
    /** Dialog state has changed. */
    PJSIP_DIALOG_EVENT_STATE_CHANGED,

    /** Any mid-call messages (reinvitation, message, etc.). */
    PJSIP_DIALOG_EVENT_MID_CALL_REQUEST,

    /** Other events (low level events). */
    PJSIP_DIALOG_EVENT_OTHER,

} pjsip_dlg_event_e;


/** 
 * \brief Structure registered by applications to receive dialog notifications 
 * from the User Agent. 
 *
 * Applications registers this structure to get notifications from the User Agent
 * about dialog state changes and other events. Application can set any of
 * the callback function to NULL if it doesn't want to handle the notification,
 * however, setting some callbacks to NULL probably will cause some undesired
 * result (such as setting \b on_incoming to NULL will cause the creation of
 * a lot of dialogs with no owner).
 */
struct pjsip_dlg_callback
{
    /**
     * This is a low level, uninterpreted callback that is called by framework 
     * for all kinds of events, such as transaction events, dialog events, etc.
     * @param dlg	The dialog.
     * @param dlg_event The type of dialog event.
     * @param event	The event descriptor.
     */
    void (*on_all_events)(pjsip_dlg *dlg, pjsip_dlg_event_e dlg_event, 
			  pjsip_event *event );

    /**
     * This is a low level callback that is called by the framework when the
     * underlying transaction is about to send outgoing message. This callback
     * is provided to allow application to modify the message before it is 
     * transmitted. 
     * @param dlg	The dialog.
     * @param tsx	The transaction that transmits the message.
     * @param tdata	The transmission data, which contains the message.
     * @param retransmission The number of times this message has been sent.
     *			Zero indicates the message is about to be sent the first time,
     *			one indicates this is the first retransmission, etc.
     */
    void (*on_before_tx)(pjsip_dlg *dlg, pjsip_transaction *tsx,
			 pjsip_tx_data *tdata, pj_bool_t retransmission);

    /**
     * This is a low level callback that is called by the framework when the dialog
     * has sent a message. Note that a receive of retransmission will not trigger
     * this callback since retransmission is handled internally by transaction.
     * @param dlg	The dialog.
     * @param tsx	The transaction that transmits the message.
     * @param tdata	The transmission data, which contains the message.
     */
    void (*on_tx_msg)(pjsip_dlg *dlg, pjsip_transaction *tsx, 
		      pjsip_tx_data *tdata);

    /**
     * This is a low level callback that is called by the framework when the
     * dialog has received a message. Note that a receipt of retransmission 
     * will not trigger this callback since retransmission is handled internally
     * by transaction.
     * @param dlg	The dialog.
     * @param tsx	The transaction that receives the message.
     * @param rdata	The receive data, which contains the message.
     */
    void (*on_rx_msg)(pjsip_dlg *dlg, pjsip_transaction *tsx, 
		      pjsip_rx_data *rdata);

    /**
     * This callback is called by the framework when the user agent
     * instance receives an incoming INVITE message.
     * @param dlg	The new dialog that's just created to handle the incoming call.
     * @param tsx	The INVITE transaction that's just created.
     * @param rdata	The receive data, which contains the INVITE message.
     */
    void (*on_incoming)(pjsip_dlg *dlg, pjsip_transaction *tsx,
		        pjsip_rx_data *rdata);

    /**
     * This callback is called by the framework when the dialog is sending
     * the first outgoing INVITE message.
     * @param dlg	The dialog.
     * @param tsx	The INVITE transaction.
     * @param tdata	The transmit data, which contains the INVITE message.
     */
    void (*on_calling)(pjsip_dlg *dlg, pjsip_transaction *tsx,
		       pjsip_tx_data *tdata);

    /**
     * This callback is called by the framework when the initial INVITE
     * transaction has sent/received provisional response.
     * @param dlg	The dialog.
     * @param tsx	The transaction.
     * @param event	The event, which src_type will always be either
     *			PJSIP_EVENT_RX_MSG or PJSIP_EVENT_TX_MSG. The provisional
     *			response message itself will be in either \b rdata or \b tdata.
     * @see pjsip_event.
     */
    void (*on_provisional)(pjsip_dlg *dlg, pjsip_transaction *tsx,
			   pjsip_event *event);

    /**
     * This callback is called for both UAS and UAC dialog when 200 response
     * to INVITE is sent or received.
     * @param dlg	The dialog.
     * @param event	The event, which src_type can only be either 
     *			PJSIP_EVENT_TX_MSG or PJSIP_EVENT_RX_MSG.
     * @see pjsip_event
     */
    void (*on_connecting)(pjsip_dlg *dlg, pjsip_event *event);

    /**
     * This callback is called for both UAS and UAC when an ACK request is
     * sent or received by the dialog.
     * @param dlg	The dialog.
     * @param event	The event, which src_type can only be either 
     *			PJSIP_EVENT_TX_MSG or PJSIP_EVENT_RX_MSG.
     * @see pjsip_event
     */
    void (*on_established)(pjsip_dlg *dlg, pjsip_event *event);

    /**
     * This callback is called when the dialog is disconnected, i.e. upon
     * sending/receiving non-200 response to INVITE, sending/receiving
     * CANCEL to initial INVITE, and sending/receiving BYE.
     *
     * @param dlg	The dialog.
     * @param event	The event.
     * @see pjsip_event
     */
    void (*on_disconnected)(pjsip_dlg *dlg, pjsip_event *event);

    /**
     * This callback is called when the dialog is about to be destroyed.
     * @param dlg The dialog.
     */
    void (*on_terminated)(pjsip_dlg *dlg);

    /**
     * This callback will be called when the dialog receives mid call events
     * such as re-invitation or incoming pager.
     *
     * @param dlg	The dialog.
     * @param event	The event.
     */
    void (*on_mid_call_events)(pjsip_dlg *dlg, pjsip_event *event);

};  /* struct pjsip_dlg_callback */



/**
 * Dialog state.
 */
typedef enum pjsip_dlg_state_e
{
    /** 
     * State NULL is after the dialog is instantiated but before any
     * initialization is done. 
     */
    PJSIP_DIALOG_STATE_NULL,

    /**
     * State INCOMING is after the (callee) dialog has been initialized with
     * the incoming request, but before any responses is sent by the dialog.
     */
    PJSIP_DIALOG_STATE_INCOMING,

    /**
     * State CALLING is after the (caller) dialog has sent outgoing invitation
     * but before any responses are received.
     */
    PJSIP_DIALOG_STATE_CALLING,

    /**
     * State PROCEEDING is after the dialog sent/received provisional 
     * responses, but before final response is sent/received.
     */
    PJSIP_DIALOG_STATE_PROCEEDING,

    /**
     * State CONNECTING is after the dialog has sent/received final response
     * to the invitation, but before acknowledgement is sent.
     */
    PJSIP_DIALOG_STATE_CONNECTING,

    /**
     * State ESTABLISHED occurs after the invitation has been accepted and
     * acknowledged.
     */
    PJSIP_DIALOG_STATE_ESTABLISHED,

    /**
     * State DISCONNECTED occurs after either party successfully disconnect
     * the session.
     */
    PJSIP_DIALOG_STATE_DISCONNECTED,

    /**
     * State TERMINATE occurs when the dialog is ready to be destroyed.
     */
    PJSIP_DIALOG_STATE_TERMINATED,

} pjsip_dlg_state_e;


/**
 * Get the dialog string state.
 *
 * @param state	    Dialog state.
 * @return	    The string describing the state.
 */
const char *pjsip_dlg_state_str(pjsip_dlg_state_e state);

/**
 * This structure is used to describe dialog's participants, which in this
 * case is local party (i.e. us) and remote party.
 */
typedef struct pjsip_dlg_party
{
    pjsip_uri		*target;    /**< Target URL.			*/
    pjsip_fromto_hdr	*info;	    /**< URL in From/To header.		*/
    pj_str_t		 tag;	    /**< Tag.				*/
    pjsip_contact_hdr	*contact;   /**< URL in Contact.		*/
    pj_sockaddr_in	 addr;	    /**< The current transport address. */
    int			 cseq;	    /**< Sequence number counter.	*/
} pjsip_dlg_party;


/**
 * This structure describes the dialog structure.
 */
struct pjsip_dlg
{
    PJ_DECL_LIST_MEMBER(struct pjsip_dlg)

    char	        obj_name[PJ_MAX_OBJ_NAME];  /**< Log identification.	*/

    pjsip_user_agent   *ua;			    /**< User agent instance.	*/
    pj_pool_t	       *pool;			    /**< Dialog's pool.		*/
    pjsip_dlg_state_e   state;			    /**< Dialog's call state.	*/
    pjsip_role_e	role;			    /**< Dialog's role.		*/
    pj_mutex_t	       *mutex;			    /**< Dialog's mutex.	*/

    pjsip_dlg_party     local;			    /**< Local party info.	*/
    pjsip_dlg_party     remote;			    /**< Remote party info.	*/

    pjsip_cid_hdr      *call_id;		    /**< Call-ID		*/
    pj_bool_t	        secure;			    /**< Use secure transport?	*/

    pjsip_route_hdr     route_set;		    /**< Dialog's route set.	*/
    pjsip_transaction  *invite_tsx;		    /**< Current INVITE transaction. */
    int			pending_tsx_count;	    /**< Total pending tsx count. */

    int			cred_count;		    /**< Number of credentials. */
    pjsip_cred_info    *cred_info;		    /**< Array of credentials.	*/

    pjsip_auth_session	auth_sess;		    /**< List of auth session. */

    pjsip_msg_body     *body;

    void	       *user_data;		    /**< Application's data.	*/

    int  (*handle_tsx_event)(struct pjsip_dlg *,    /**< Internal state handler.*/
			     pjsip_transaction *,
			     pjsip_event *);
};


/**
 * Initialize dialog with local and remote info. This function is normally
 * called after application creates the dialog with #pjsip_ua_create_dialog
 * for UAC dialogs.
 *
 * This function will initialize local and remote info from the URL, generate
 * a globally unique Call-ID, initialize CSeq, and initialize other dialog's
 * internal attributes.
 *
 * @param dlg		The dialog to initialize.
 * @param local_info	URI/name address to be used as local info 
 *			(From and Contact headers).
 * @param remote_info	URI/name address to be used as remote info (To header).
 * @param target	URI for initial remote's target, or NULL to set the
 *			initial target the same as remote_info.
 *
 * @return		zero on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_init( pjsip_dlg *dlg,
				     const pj_str_t *local_info,
				     const pj_str_t *remote_info,
				     const pj_str_t *target);


/**
 * Set authentication credentials to be used by this dialog.
 *
 * If authentication credentials are set for the dialog, the dialog will try to
 * perform authentication automatically using the credentials supplied, and 
 * also cache the last Authorization or Proxy-Authorization headers for next 
 * requests.
 * 
 * If none of the credentials are suitable or accepted by remote, then
 * the dialog will just pass the authorization failure response back to
 * application.
 *
 * @param dlg		The dialog.
 * @param count		Number of credentials in the array.
 * @param cred		Array of credentials.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_set_credentials( pjsip_dlg *dlg,
					        int count,
						const pjsip_cred_info cred[]);

/**
 * Override local contact details.
 *
 * Call this function to change the contact details to be advertised in Contact
 * header. Application normally need to call this function for incoming calls
 * before answering the call with 200/OK, because for an incoming dialogs, the
 * initial local contact info are generated from the To header, which is 
 * normally not the appropriate one.
 *
 * @param dlg		The dialog.
 * @param contact	The contact to use.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_set_contact( pjsip_dlg *dlg,
					    const pj_str_t *contact );


/**
 * Set initial route set to be used by the dialog. This initial route set
 * governs where and how the initial INVITE request will be routed. This
 * initial route set will be overwritten with the route set found in the
 * 2xx response of INVITE.
 *
 * Application only needs to call this function if it wants to have custom
 * route for individual dialogs. If only a single route for all dialogs is
 * needed, then application can set the global route by calling function
 * #pjsip_endpt_set_proxies().
 *
 * @param dlg		The dialog.
 * @param route_set	The route set list.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_set_route_set( pjsip_dlg *dlg,
					      const pjsip_route_hdr *route_set );


/**
 * Variation of #pjsip_dlg_set_route_set where the headers will be used
 * as it is (i.e. without cloned).
 *
 * @param dlg		The dialog.
 * @param route_set	The route set list.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_set_route_set_np( pjsip_dlg *dlg,
						 pjsip_route_hdr *route_set);

/**
 * Create initial outgoing INVITE message.
 *
 * This function is just a simple wrapper to #pjsip_dlg_create_request(),
 * so it follows the same rule there. In addition, this function also adds
 * \b Allow header to the outgoing request.
 *
 * After the message is successfully created, application must call
 * #pjsip_dlg_send_msg() to actually send the message and update the dialog's
 * state. Note that upon return the reference counter of the transmit data
 * will be set to one.
 *
 * @param dlg		The dialog.
 *
 * @return		The dialog transmit data, or NULL.
 */
PJ_DECL(pjsip_tx_data*) pjsip_dlg_invite( pjsip_dlg *dlg );


/**
 * Answer incoming dialog invitation, with either provisional responses
 * or a final response. Application can only call this function when there's
 * a pending invitation to be answered.
 *
 * After the message is successfully created, application must call
 * #pjsip_dlg_send_msg() to actually send the message and update the dialog's
 * state. Note that upon return the reference counter of the transmit data
 * will be set to one.
 *
 * @param dlg	The dialog.
 * @param code	The response code, which can be:
 *		- 100-199 Provisional response (application can issue multiple 
 *                        provisional responses).
 *		- 200-299 To answer the invitation (normally status code 200
 *                        is sent).
 *              - 300-699 To reject the invitation.
 * @return	Transmit data if successfull.
 */
PJ_DECL(pjsip_tx_data*) pjsip_dlg_answer( pjsip_dlg *dlg, int code );


/**
 * High level function to create message to disconnect dialog. Depending 
 * on dialog's  state, this function will either create CANCEL, final response,
 * or BYE message. A status code must be supplied, which will be set if dialog 
 * will be transmitting a final response to INVITE.
 *
 * After the message is successfully created, application must call
 * #pjsip_dlg_send_msg to actually send the message and update the dialog's
 * state. Note that upon return the reference counter of the transmit data
 * will be set to one.
 *
 * @param dlg		The dialog.
 * @param status_code	The status code for disconnection.
 * @return		Transmit data if successfull.
 */
PJ_DECL(pjsip_tx_data*) pjsip_dlg_disconnect( pjsip_dlg *dlg, int status_code);

/**
 * Create CANCEL message to cancel pending outgoing dialog invitation. 
 * Normally application should call #pjsip_dlg_disconnect() instead, because
 * that function will create the correct message regardless of the state of 
 * the dialog. 
 *
 * Application can call this function at anytime after it issues outgoing 
 * invitation and before receiving final response. However, there's no 
 * guarantee that the invitation will be successfully cancelled, since the 
 * CANCEL request and the final response can pass over in the wire. So the 
 * application must prepare to have the dialog connected even after the 
 * dialog is cancelled.
 *
 * The final state of the dialog will be reported in the dialog callback.
 * If the CANCEL request succeeded, then the dialog will be disconnected with
 * status code \a PJSIP_SC_REQUEST_TERMINATED.
 *
 * After the message is successfully created, application must call
 * #pjsip_dlg_send_msg() to actually send the message and update the dialog's
 * state. 
 *
 * Upon return of this function, the reference counter of the transmit data
 * will be set to one.
 *
 * @param dlg		The dialog.
 * @return		The dialog transmit data containing the CANCEL message,
 *			or NULL.
 */
PJ_DECL(pjsip_tx_data*) pjsip_dlg_cancel( pjsip_dlg *dlg );


/**
 * Create BYE message. Application shouldn't normally need to use this function,
 * but rather it's preferable to use #pjsip_dlg_disconnect() instead because
 * that function will work to disconnect the session no matter what the state
 * is.
 *
 * After the message is successfully created, application must call
 * #pjsip_dlg_send_msg() to actually send the message and update the dialog's
 * state. Note that upon return the reference counter of the transmit data
 * will be set to one.
 *
 * @param dlg		The dialog.
 * @return		The BYE message or NULL.
 */
PJ_DECL(pjsip_tx_data*) pjsip_dlg_bye( pjsip_dlg *dlg );

/**
 * This function is called by application to create new outgoing request
 * message for this dialog. After the request is created, application can
 * modify the message (such adding headers), and eventually send the request
 * by calling #pjsip_dlg_send_msg().
 *
 * This function will initialize the request message with dialog's properties
 * as follows:
 *  - the request line is initialized with the method and the target is
 *    initialized from current remote target.
 *  - \b From, \b To, \b Contact, and \b Call-Id headers will be added.
 *  - An initial \b CSeq header will be provided (although the value will be
 *    verified again when the message is actually sent with #pjsip_dlg_send_msg().
 *  - \b Route headers will be added from dialog's route set.
 *  - Authentication headers (\b Authorization or \b Proxy-Authorization) will
 *    be added from dialog's authorization cache.
 *
 * Note that upon return the reference counter of the transmit data
 * will be set to one. When the message is sent, #pjsip_dlg_send_msg() will
 * decrement the reference counter, and when the reference counter reach zero,
 * the message will be deleted.
 *
 * @param dlg		The dialog.
 * @param method	The request method.
 * @param cseq		Specify CSeq, or -1 to let the dialog specify CSeq.
 *
 * @return		Transmit data for the new request.
 *
 * @see pjsip_dlg_send_msg()
 */
PJ_DECL(pjsip_tx_data*) pjsip_dlg_create_request( pjsip_dlg *dlg,
						  const pjsip_method *method,
						  int cseq);


/**
 * This function can be called by application to send outgoing message (request
 * or response) to remote party. Note that after calling this function, the
 * transmit data will be deleted regardless of the return status. To prevent
 * deletion, application must increase the reference count, but then it will
 * be responsible to delete this transmit data itself (by decreasing the
 * reference count).
 *
 * @param dlg		The dialog.
 * @param tdata		The transmit data, which contains the request message.
 * @return		zero on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_send_msg( pjsip_dlg *dlg,
					 pjsip_tx_data *tdata );


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_DIALOG_H__ */


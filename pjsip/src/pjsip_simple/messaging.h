/* $Header: /pjproject/pjsip/src/pjsip_simple/messaging.h 6     8/31/05 9:05p Bennylp $ */
#ifndef __PJSIP_SIMPLE_MESSAGING_H__
#define __PJSIP_SIMPLE_MESSAGING_H__

/**
 * @file messaging.h
 * @brief Instant Messaging Extension (RFC 3428)
 */

#include <pjsip/sip_msg.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_MESSAGING SIP Instant Messaging (RFC 3428) Module
 * @ingroup PJSIP_SIMPLE
 * @{
 *
 * This module provides the implementation of SIP Extension for Instant
 * Messaging (RFC 3428). It extends PJSIP by supporting MESSAGE method.
 *
 * The RFC 3428 doesn't provide any means of dialog for the purpose of sending/
 * receiving instant messaging. IM with SIP is basicly sessionless, which means
 * that there is absolutely no correlation between IM messages sent or received
 * by a host. Any correlation between IM messages is only perceivable by
 * user, phsychologically.
 *
 * However, the RFC doesn't prohibit sending IM within a dialog (presumably
 * using the same Call-ID and CSeq namespace), although it prohibits creating
 * a dialog specificly for creating IM session.
 *
 * The implementation here is modeled to support both ways of sending IM msgs,
 * i.e. sending IM message individually and sending IM message within a dialog.
 * Although IM message can be associated with a dialog, this implementation of
 * IM module is completely independent of the User Agent library in PJSIP. Yes,
 * that's what is called modularity, and it demonstrates the clearness 
 * of PJSIP design (the last sentence is of course marketing crap :)).
 *
 * To send an IM message as part of dialog, application would first create the
 * message using #pjsip_messaging_create_msg, using dialog's Call-ID, CSeq,
 * From, and To header, then send the message using #pjsip_dlg_send_msg instead
 * of #pjsip_messaging_send_msg. 
 *
 * To send IM messages individually, application has two options. The first is
 * to create the request with #pjsip_messaging_create_msg then send it with
 * #pjsip_messaging_send_msg. But this way, application has to pre-construct
 * From and To header first, which is not too convenient.
 *
 * The second option (to send IM messages not associated with a dialog) is to
 * first create an 'IM session'. An 'IM session' here is not a SIP dialog, as
 * it doesn't have Contact header etc. An 'IM session' here is just a local
 * state to cache most of IM headers, for convenience and optimization. Appl 
 * creates an IM session with #pjsip_messaging_create_session, and destroy
 * the session with #pjsip_messaging_destroy_session. To create an outgoing
 * IM message, application would call #pjsip_messaging_session_create_msg,
 * and to send the message it would use #pjsip_messaging_send_msg.
 *
 * Message authorization is handled by application, as usual, by inserting a 
 * proper WWW-Authenticate or Proxy-Authenticate header before sending the 
 * message.
 *
 * And the last bit, to handle incoing IM messages.
 *
 * To handle incoming IM messages, application would register a global callback
 * to be called when incoming messages arrive, by registering with function
 * #pjsip_messaging_set_incoming_callback. This will be the global callback
 * for all incoming IM messages. Although the message was sent as part of
 * a dialog, it would still come here. And as long as the request has proper
 * identification (Call-ID, From/To tag), the dialog will be aware of the
 * request and update it's state (remote CSeq) accordingly.
 */



/**
 * Typedef for callback to be called when outgoing message has been sent
 * and a final response has been received.
 */
typedef void (*pjsip_messaging_cb)(void *token, int status_code);

/**
 * Typedef for callback to receive incoming message.
 *
 * @param rdata	    Incoming message data.
 *
 * @return	    The status code to be returned back to original sender.
 *		    Application must return a final status code upon returning
 *		    from this function, or otherwise the stack will respond
 *		    with error.
 */
typedef int (*pjsip_on_new_msg_cb)(pjsip_rx_data *rdata);

/**
 * Opaque data type for instant messaging session.
 */
typedef struct pjsip_messaging_session pjsip_messaging_session;

/**
 * Get the messaging module.
 *
 * @return SIP module.
 */
PJ_DECL(pjsip_module*) pjsip_messaging_get_module();

/**
 * Set the global callback to be called when incoming message is received.
 *
 * @param cb	    The callback to be called when incoming message is received.
 *
 * @return	    The previous callback.
 */
PJ_DECL(pjsip_on_new_msg_cb) 
pjsip_messaging_set_incoming_callback(pjsip_on_new_msg_cb cb);


/**
 * Create an instant message transmit data buffer using the specified arguments.
 * The returned transmit data buffers will have it's reference counter set
 * to 1, and when application send the buffer, the send function will decrement
 * the reference counter. When the reference counter reach zero, the buffer
 * will be deleted. As long as the function does not increment the buffer's 
 * reference counter between creating and sending the request,  the buffer 
 * will always be deleted and no memory leak will occur.
 *
 * @param endpt	    Endpoint instance.
 * @param target    Target URL.
 * @param from	    The "From" header, which content will be copied to request.
 *		    If the "From" header doesn't have a tag parameter, the
 *		    function will generate one.
 * @param to	    The "To" header, which content will be copied to request.
 * @param call_id   Optionally specify Call-ID, or put NULL to make this
 *		    function generate a unique Call-ID automatically.
 * @param cseq	    Optionally specify CSeq, or put -1 to make this function
 *		    generate a random CSeq.
 * @param text	    Optionally specify "text/plain" message body, or put NULL 
 *		    if application wants to put body other than "text/plain"
 *		    manually.
 *
 * @return	    SIP transmit data buffer, which reference count has been
 *		    set to 1.
 */
PJ_DECL(pjsip_tx_data*) 
pjsip_messaging_create_msg_from_hdr(pjsip_endpoint *endpt, 
				    const pjsip_uri *target,
				    const pjsip_from_hdr *from,
				    const pjsip_to_hdr *to, 
				    const pjsip_cid_hdr *call_id,
				    int cseq, 
				    const pj_str_t *text);

/**
 * Create instant message, by specifying URL string for both From and To header.
 *
 * @param endpt	    Endpoint instance.
 * @param target    Target URL.
 * @param from	    URL of the sender.
 * @param to	    URL of the recipient.
 * @param call_id   Optionally specify Call-ID, or put NULL to make this
 *		    function generate a unique Call-ID automatically.
 * @param cseq	    Optionally specify CSeq, or put -1 to make this function
 *		    generate a random CSeq.
 * @param text	    Optionally specify "text/plain" message body, or put NULL 
 *		    if application wants to put body other than "text/plain"
 *		    manually.
 *
 * @return	    SIP transmit data buffer, which reference count has been
 *		    set to 1.
 */
PJ_DECL(pjsip_tx_data*) pjsip_messaging_create_msg( pjsip_endpoint *endpt, 
						    const pj_str_t *target,
						    const pj_str_t *from,
						    const pj_str_t *to, 
						    const pj_str_t *call_id,
						    int cseq, 
						    const pj_str_t *text);

/**
 * Send the instant message transmit buffer and attach a callback to be called
 * when the request has received a final response. This function will decrement
 * the transmit buffer's reference counter, and when the reference counter
 * reach zero, the buffer will be deleted. As long as the function does not
 * increment the buffer's reference counter between creating the request and
 * calling this function, the buffer will always be deleted regardless whether
 * the sending was failed or succeeded.
 *
 * @param endpt	    Endpoint instance.
 * @param tdata	    Transmit data buffer.
 * @param token	    Token to be associated with the SIP transaction which sends
 *		    this request.
 * @param cb	    The callback to be called when the SIP request has received
 *		    a final response from destination.
 *
 * @return	    Zero if the transaction was started successfully. Note that
 *		    this doesn't mean the request has been received successfully
 *		    by remote recipient.
 */
PJ_DECL(pj_status_t) pjsip_messaging_send_msg( pjsip_endpoint *endpt, 
					       pjsip_tx_data *tdata, 
					       void *token, 
					       pjsip_messaging_cb cb );

/**
 * Create an instant messaging session, which can conveniently be used to send
 * multiple instant messages to the same recipient.
 *
 * @param endpt	    Endpoint instance.
 * @param from	    URI of sender. The function will add a unique tag parameter
 *		    to this URI in the From header.
 * @param to	    URI of recipient.
 *
 * @return	    Messaging session.
 */
PJ_DECL(pjsip_messaging_session*) 
pjsip_messaging_create_session( pjsip_endpoint *endpt, 
			        const pj_str_t *from,
			        const pj_str_t *to );

/**
 * Create an instant message using instant messaging session, and optionally
 * attach a text message.
 *
 * @param ses	    The instant messaging session.
 * @param text	    Optional "text/plain" message to be attached as the
 *		    message body. If this parameter is NULL, then no message
 *		    body will be created, and application can attach any
 *		    type of message body before the request is sent.
 *
 * @return	    SIP transmit data buffer, which reference counter has been
 *		    set to 1.
 */
PJ_DECL(pjsip_tx_data*)
pjsip_messaging_session_create_msg( pjsip_messaging_session *ses, 
				    const pj_str_t *text );

/**
 * Destroy an instant messaging session.
 *
 * @param ses	    The instant messaging session.
 *
 * @return	    Zero on successfull.
 */
PJ_DECL(pj_status_t)
pjsip_messaging_destroy_session( pjsip_messaging_session *ses );

/**
 * @}
 */

PJ_END_DECL

#endif

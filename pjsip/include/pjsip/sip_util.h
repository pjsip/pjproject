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
#ifndef __PJSIP_SIP_MISC_H__
#define __PJSIP_SIP_MISC_H__

#include <pjsip/sip_msg.h>
#include <pjsip/sip_resolve.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_ENDPT_STATELESS Message Creation and Stateless Operations
 * @ingroup PJSIP_CORE_CORE
 * @brief Utilities to create various messages and base function to send messages.
 * @{
 */

/**
 * Create an independent request message. This can be used to build any
 * request outside a dialog, such as OPTIONS, MESSAGE, etc. To create a request
 * inside a dialog, application should use #pjsip_dlg_create_request.
 *
 * This function adds the following headers in the request:
 *  - From, To, Call-ID, and CSeq,
 *  - Contact header, if contact is specified.
 *  - A blank Via header.
 *  - Additional request headers (such as Max-Forwards) which are copied
 *    from endpoint configuration.
 *
 * In addition, the function adds a unique tag in the From header.
 *
 * Once a transmit data is created, the reference counter is initialized to 1.
 *
 * @param endpt	    Endpoint instance.
 * @param method    SIP Method.
 * @param target    Target URI.
 * @param from	    URL to put in From header.
 * @param to	    URL to put in To header.
 * @param contact   URL to put in Contact header.
 * @param call_id   Optional Call-ID (put NULL to generate unique Call-ID).
 * @param cseq	    Optional CSeq (put -1 to generate random CSeq).
 * @param text	    Optional text body (put NULL to omit body).
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_request( pjsip_endpoint *endpt, 
						 const pjsip_method *method,
						 const pj_str_t *target,
						 const pj_str_t *from,
						 const pj_str_t *to, 
						 const pj_str_t *contact,
						 const pj_str_t *call_id,
						 int cseq, 
						 const pj_str_t *text,
						 pjsip_tx_data **p_tdata);

/**
 * Create an independent request message from the specified headers. This
 * function will clone the headers and put them in the request.
 *
 * This function adds the following headers in the request:
 *  - From, To, Call-ID, and CSeq,
 *  - Contact header, if contact is specified.
 *  - A blank Via header.
 *  - Additional request headers (such as Max-Forwards) which are copied
 *    from endpoint configuration.
 *
 * In addition, the function adds a unique tag in the From header.
 *
 * Once a transmit data is created, the reference counter is initialized to 1.
 *
 * @param endpt	    Endpoint instance.
 * @param method    SIP Method.
 * @param target    Target URI.
 * @param from	    URL to put in From header.
 * @param to	    URL to put in To header.
 * @param contact   URL to put in Contact header.
 * @param call_id   Optional Call-ID (put NULL to generate unique Call-ID).
 * @param cseq	    Optional CSeq (put -1 to generate random CSeq).
 * @param text	    Optional text body (put NULL to omit body).
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t)
pjsip_endpt_create_request_from_hdr( pjsip_endpoint *endpt,
				     const pjsip_method *method,
				     const pjsip_uri *target,
				     const pjsip_from_hdr *from,
				     const pjsip_to_hdr *to,
				     const pjsip_contact_hdr *contact,
				     const pjsip_cid_hdr *call_id,
				     int cseq,
				     const pj_str_t *text,
				     pjsip_tx_data **p_tdata);

/**
 * Construct a minimal response message for the received request. This function
 * will construct all the Via, Record-Route, Call-ID, From, To, CSeq, and 
 * Call-ID headers from the request.
 *
 * Note: the txdata reference counter is set to ZERO!.
 *
 * @param endpt	    The endpoint.
 * @param rdata	    The request receive data.
 * @param st_code   Status code to be put in the response.
 * @param st_text   Optional status text, or NULL to get the default text.
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_response( pjsip_endpoint *endpt,
						  const pjsip_rx_data *rdata,
						  int st_code,
						  const pj_str_t *st_text,
						  pjsip_tx_data **p_tdata);

/**
 * Construct a full ACK request for the received non-2xx final response.
 * This utility function is normally called by the transaction to construct
 * an ACK request to 3xx-6xx final response.
 * The generation of ACK message for 2xx final response is different than
 * this one.
 * 
 * @param endpt	    The endpoint.
 * @param tdata	    This contains the original INVITE request
 * @param rdata	    The final response.
 * @param ack	    The ACK request created.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_ack( pjsip_endpoint *endpt,
					     const pjsip_tx_data *tdata,
					     const pjsip_rx_data *rdata,
					     pjsip_tx_data **ack);


/**
 * Construct CANCEL request for the previously sent request.
 *
 * @param endpt	    The endpoint.
 * @param tdata	    The transmit buffer for the request being cancelled.
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_cancel( pjsip_endpoint *endpt,
						const pjsip_tx_data *tdata,
						pjsip_tx_data **p_tdata);


/**
 * Find which destination to be used to send the request message, based
 * on the request URI and Route headers in the message. The procedure
 * used here follows the guidelines on sending the request in RFC 3261
 * chapter 8.1.2.
 *
 * Note there was a change in the behavior of this function since version
 * 0.5.10.2. Previously this function may modify the request when strict
 * route is present (to update request URI and route-set). This is no
 * longer the case now, and this process is done in separate function
 * (see #pjsip_process_route_set()).
 *
 * @param tdata	    The transmit data containing the request message.
 * @param dest_info On return, it contains information about destination
 *		    host to contact, along with the preferable transport
 *		    type, if any. Caller will then normally proceed with
 *		    resolving this host with server resolution procedure
 *		    described in RFC 3263.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 *
 * @see pjsip_process_route_set
 */
PJ_DECL(pj_status_t) pjsip_get_request_dest(const pjsip_tx_data *tdata,
					    pjsip_host_info *dest_info );


/**
 * Process route-set found in the request and calculate destination to be
 * used to send the request message, based on the request URI and Route 
 * headers in the message. The procedure used here follows the guidelines
 * on sending the request in RFC 3261 chapter 8.1.2.
 *
 * This function may modify the message (request line and Route headers),
 * if the Route information specifies strict routing and the request
 * URI in the message is different than the calculated target URI. In that
 * case, the target URI will be put as the request URI of the request and
 * current request URI will be put as the last entry of the Route headers.
 *
 * @param tdata	    The transmit data containing the request message.
 * @param dest_info On return, it contains information about destination
 *		    host to contact, along with the preferable transport
 *		    type, if any. Caller will then normally proceed with
 *		    resolving this host with server resolution procedure
 *		    described in RFC 3263.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 *
 * @see pjsip_get_request_addr
 */
PJ_DECL(pj_status_t) pjsip_process_route_set(pjsip_tx_data *tdata,
					     pjsip_host_info *dest_info );


/**
 * This structure holds the state of outgoing stateless request.
 */
typedef struct pjsip_send_state
{
    /** Application token, which was specified when the function
     *  #pjsip_endpt_send_request_stateless() is called.
     */
    void *token;

    /** Endpoint instance. 
     */
    pjsip_endpoint *endpt;

    /** Transmit data buffer being sent. 
     */
    pjsip_tx_data *tdata;

    /** Server addresses resolved. 
     */
    pjsip_server_addresses   addr;

    /** Current server address being tried. 
     */
    unsigned cur_addr;

    /** Current transport being used. 
     */
    pjsip_transport *cur_transport;

    /** The application callback which was specified when the function
     *  #pjsip_endpt_send_request_stateless() was called.
     */
    void (*app_cb)(struct pjsip_send_state*,
		   pj_ssize_t sent,
		   pj_bool_t *cont);
} pjsip_send_state;

/**
 * Send outgoing request statelessly The function will take care of which 
 * destination and transport to use based on the information in the message,
 * taking care of URI in the request line and Route header.
 *
 * This function is different than #pjsip_transport_send() in that this 
 * function adds/modify the Via header as necessary.
 *
 * @param endpt	    The endpoint instance.
 * @param tdata	    The transmit data to be sent.
 * @param token	    Arbitrary token to be given back on the callback.
 * @param cb	    Optional callback to notify transmission status (also
 *		    gives chance for application to discontinue retrying
 *		    sending to alternate address).
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) 
pjsip_endpt_send_request_stateless( pjsip_endpoint *endpt,
				    pjsip_tx_data *tdata,
				    void *token,
				    void (*cb)(pjsip_send_state*,
					       pj_ssize_t sent,
					       pj_bool_t *cont));

/**
 * This structure describes destination information to send response.
 * It is initialized by calling #pjsip_get_response_addr().
 *
 * If the response message should be sent using transport from which
 * the request was received, then transport, addr, and addr_len fields
 * are initialized.
 *
 * The dst_host field is also initialized. It should be used when server
 * fails to send the response using the transport from which the request
 * was received, or when the transport is NULL, which means server
 * must send the response to this address (this situation occurs when
 * maddr parameter is set, or when rport param is not set in the request).
 */
typedef struct pjsip_response_addr
{
    pjsip_transport *transport;	/**< Immediate transport to be used. */
    pj_sockaddr	     addr;	/**< Immediate address to send to.   */
    int		     addr_len;	/**< Address length.		     */
    pjsip_host_info  dst_host;	/**< Destination host to contact.    */
} pjsip_response_addr;

/**
 * Determine which address (and transport) to use to send response message
 * based on the received request. This function follows the specification
 * in section 18.2.2 of RFC 3261 and RFC 3581 for calculating the destination
 * address and transport.
 *
 * The information about destination to send the response will be returned
 * in res_addr argument. Please see #pjsip_response_addr for more info.
 *
 * @param pool	    The pool.
 * @param rdata	    The incoming request received by the server.
 * @param res_addr  On return, it will be initialized with information about
 *		    destination address and transport to send the response.
 *
 * @return	    zero (PJ_OK) if successfull.
 */
PJ_DECL(pj_status_t) pjsip_get_response_addr(pj_pool_t *pool,
					     pjsip_rx_data *rdata,
					     pjsip_response_addr *res_addr);

/**
 * Send response in tdata statelessly. The function will take care of which 
 * response destination and transport to use based on the information in the 
 * Via header (such as the presence of rport, symmetric transport, etc.).
 *
 * This function will create a new ephemeral transport if no existing 
 * transports can be used to send the message to the destination. The ephemeral
 * transport will be destroyed after some period if it is not used to send any 
 * more messages.
 *
 * The behavior of this function complies with section 18.2.2 of RFC 3261
 * and RFC 3581.
 *
 * @param endpt	    The endpoint instance.
 * @param res_addr  The information about the address and transport to send
 *		    the response to. Application can get this information
 *		    by calling #pjsip_get_response_addr().
 * @param tdata	    The response message to be sent.
 * @param token	    Token to be passed back when the callback is called.
 * @param cb	    Optional callback to notify the transmission status
 *		    to application, and to inform whether next address or
 *		    transport will be tried.
 * 
 * @return	    PJ_SUCCESS if response has been successfully created and
 *		    sent to transport layer, or a non-zero error code. 
 *		    However, even when it returns PJ_SUCCESS, there is no 
 *		    guarantee that the response has been successfully sent.
 */
PJ_DECL(pj_status_t) pjsip_endpt_send_response( pjsip_endpoint *endpt,
					        pjsip_response_addr *res_addr,
					        pjsip_tx_data *tdata,
						void *token,
						void (*cb)(pjsip_send_state*,
							   pj_ssize_t sent,
							   pj_bool_t *cont));

/**
 * This is a convenient function which wraps #pjsip_get_response_addr() and
 * #pjsip_endpt_send_response() in a single function.
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The original request to be responded.
 * @param tdata	    The response message to be sent.
 * @param token	    Token to be passed back when the callback is called.
 * @param cb	    Optional callback to notify the transmission status
 *		    to application, and to inform whether next address or
 *		    transport will be tried.
 * 
 * @return	    PJ_SUCCESS if response has been successfully created and
 *		    sent to transport layer, or a non-zero error code. 
 *		    However, even when it returns PJ_SUCCESS, there is no 
 *		    guarantee that the response has been successfully sent.
 */
PJ_DECL(pj_status_t) pjsip_endpt_send_response2(pjsip_endpoint *endpt,
					        pjsip_rx_data *rdata,
					        pjsip_tx_data *tdata,
						void *token,
						void (*cb)(pjsip_send_state*,
							   pj_ssize_t sent,
							   pj_bool_t *cont));

/**
 * This composite function sends response message statelessly to an incoming
 * request message. Internally it calls #pjsip_endpt_create_response() and
 * #pjsip_endpt_send_response().
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The incoming request message.
 * @param st_code   Status code of the response.
 * @param st_text   Optional status text of the response.
 * @param hdr_list  Optional header list to be added to the response.
 * @param body	    Optional message body to be added to the response.
 *
 * @return	    PJ_SUCCESS if response message has successfully been
 *		    sent.
 */
PJ_DECL(pj_status_t) pjsip_endpt_respond_stateless(pjsip_endpoint *endpt,
						   pjsip_rx_data *rdata,
						   int st_code,
						   const pj_str_t *st_text,
						   const pjsip_hdr *hdr_list,
						   const pjsip_msg_body *body);
						    
/**
 * @}
 */

/**
 * @defgroup PJSIP_TRANSACT_UTIL Stateful Operations
 * @ingroup PJSIP_TRANSACT
 * @brief Utility function to send requests/responses statefully.
 * @{
 */

/**
 * This composite function creates and sends response statefully for the
 * incoming request.
 *
 * @param endpt	    The endpoint instance.
 * @param tsx_user  The module to be registered as transaction user.
 * @param rdata	    The incoming request message.
 * @param st_code   Status code of the response.
 * @param st_text   Optional status text of the response.
 * @param hdr_list  Optional header list to be added to the response.
 * @param body	    Optional message body to be added to the response.
 * @param p_tsx	    Optional pointer to receive the transaction which was
 *		    created to send the response.
 *
 * @return	    PJ_SUCCESS if response message has successfully been
 *		    created.
 */
PJ_DECL(pj_status_t) pjsip_endpt_respond( pjsip_endpoint *endpt,
					  pjsip_module *tsx_user,
					  pjsip_rx_data *rdata,
					  int st_code,
					  const pj_str_t *st_text,
					  const pjsip_hdr *hdr_list,
					  const pjsip_msg_body *body,
					  pjsip_transaction **p_tsx );

/**
 * Send outgoing request and initiate UAC transaction for the request.
 * This is an auxiliary function to be used by application to send arbitrary
 * requests outside a dialog. To send a request within a dialog, application
 * should use #pjsip_dlg_send_request instead.
 *
 * @param endpt	    The endpoint instance.
 * @param tdata	    The transmit data to be sent.
 * @param timeout   Optional timeout for final response to be received, or -1 
 *		    if the transaction should not have a timeout restriction.
 *		    The value is in miliseconds.
 * @param token	    Optional token to be associated with the transaction, and 
 *		    to be passed to the callback.
 * @param cb	    Optional callback to be called when the transaction has
 *		    received a final response. The callback will be called with
 *		    the previously registered token and the event that triggers
 *		    the completion of the transaction.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_send_request( pjsip_endpoint *endpt,
					       pjsip_tx_data *tdata,
					       pj_int32_t timeout,
					       void *token,
					       void (*cb)(void*,pjsip_event*));

/**
 * @}
 */

/**
 * @defgroup PJSIP_PROXY_CORE Core Proxy Layer
 * @ingroup PJSIP
 * @brief Core proxy operations
 * @{
 */

/**
 * Create new request message to be forwarded upstream to new destination URI 
 * in uri. The new request is a full/deep clone of the request received in 
 * rdata, unless if other copy mechanism is specified in the options. 
 * The branch parameter, if not NULL, will be used as the branch-param in 
 * the Via header. If it is NULL, then a unique branch parameter will be used.
 *
 * Note: this function DOES NOT perform Route information preprocessing as
 *	  described in RFC 3261 Section 16.4. Application must take care of
 *	  removing/updating the Route headers according of the rules as
 *	  described in that section.
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The incoming request message.
 * @param uri	    The URI where the request will be forwarded to.
 * @param branch    Optional branch parameter. Application may specify its
 *		    own branch, for example if it wishes to perform loop
 *		    detection. If the branch parameter is not specified,
 *		    this function will generate its own by calling 
 *		    #pjsip_calculate_branch_id() function.
 * @param options   Optional option flags when duplicating the message.
 * @param tdata	    The result.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_request_fwd(pjsip_endpoint *endpt,
						    pjsip_rx_data *rdata, 
						    const pjsip_uri *uri,
						    const pj_str_t *branch,
						    unsigned options,
						    pjsip_tx_data **tdata);



/**
 * Create new response message to be forwarded downstream by the proxy from 
 * the response message found in rdata. Note that this function practically 
 * will clone the response as is, i.e. without checking the validity of the 
 * response or removing top most Via header. This function will perform 
 * full/deep clone of the response, unless other copy mechanism is used in 
 * the options.
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The incoming response message.
 * @param options   Optional option flags when duplicate the message.
 * @param tdata	    The result
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_response_fwd( pjsip_endpoint *endpt,
						      pjsip_rx_data *rdata, 
						      unsigned options,
						      pjsip_tx_data **tdata);



/**
 * Create a globally unique branch parameter based on the information in 
 * the incoming request message, for the purpose of creating a new request
 * for forwarding. This is the default implementation used by 
 * #pjsip_endpt_create_request_fwd() function if the branch parameter is
 * not specified.
 *
 * The default implementation here will just create an MD5 hash of the
 * top-most Via.
 *
 * Note that the returned string was allocated from rdata's pool.
 *
 * @param rdata	    The incoming request message.
 *
 * @return	    Unique branch-ID string.
 */
PJ_DECL(pj_str_t) pjsip_calculate_branch_id( pjsip_rx_data *rdata );


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_MISC_H__ */


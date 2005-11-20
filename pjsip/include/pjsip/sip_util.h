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
#ifndef __PJSIP_SIP_MISC_H__
#define __PJSIP_SIP_MISC_H__

#include <pjsip/sip_msg.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_ENDPT SIP Endpoint
 * @ingroup PJSIP
 * @{
 */

/**
 * Create an independent request message. This can be used to build any
 * request outside a dialog, such as OPTIONS, MESSAGE, etc. To create a request
 * inside a dialog, application should use #pjsip_dlg_create_request.
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
 * function will shallow clone the headers and put them in the request.
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
 * Send outgoing request and initiate UAC transaction for the request.
 * This is an auxiliary function to be used by application to send arbitrary
 * requests outside a dialog. To send a request within a dialog, application
 * should use #pjsip_dlg_send_msg instead.
 *
 * @param endpt	    The endpoint instance.
 * @param tdata	    The transmit data to be sent.
 * @param timeout   Optional timeout for final response to be received, or -1 
 *		    if the transaction should not have a timeout restriction.
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
					       int timeout,
					       void *token,
					       void (*cb)(void*,pjsip_event*));

/**
 * Construct a minimal response message for the received request. This function
 * will construct all the Via, Record-Route, Call-ID, From, To, CSeq, and 
 * Call-ID headers from the request.
 *
 * Note: the txdata reference counter is set to ZERO!.
 *
 * @param endpt	    The endpoint.
 * @param rdata	    The request receive data.
 * @param code	    Status code to be put in the response.
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_response( pjsip_endpoint *endpt,
						  const pjsip_rx_data *rdata,
						  int code,
						  pjsip_tx_data **p_tdata);

/**
 * Construct a full ACK request for the received non-2xx final response.
 * This utility function is normally called by the transaction to construct
 * an ACK request to 3xx-6xx final response.
 * The generation of ACK message for 2xx final response is different than
 * this one.
 * 
 * @param endpt	    The endpoint.
 * @param tdata	    On input, this contains the original INVITE request, and on
 *		    output, it contains the ACK message.
 * @param rdata	    The final response message.
 */
PJ_DECL(void) pjsip_endpt_create_ack( pjsip_endpoint *endpt,
				      pjsip_tx_data *tdata,
				      const pjsip_rx_data *rdata );


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
						pjsip_tx_data *tdata,
						pjsip_tx_data **p_tdata);


/**
 * Get the address parameters (host, port, flag, TTL, etc) to send the
 * response.
 *
 * @param pool	    The pool.
 * @param tr	    The transport where the request was received.
 * @param via	    The top-most Via header of the request.
 * @param addr	    The send address concluded from the calculation.
 *
 * @return	    zero (PJ_OK) if successfull.
 */
PJ_DECL(pj_status_t) pjsip_get_response_addr(pj_pool_t *pool,
					     const pjsip_transport *tr,
					     const pjsip_via_hdr *via,
					     pjsip_host_port *addr);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_MISC_H__ */


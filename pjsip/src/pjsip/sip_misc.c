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
#include <pjsip/sip_misc.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_module.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <pj/pool.h>
#include <pj/except.h>
#include <pj/rand.h>
#include <pj/assert.h>
#include <pj/errno.h>

#define THIS_FILE    "endpoint"

static const char *event_str[] = 
{
    "UNIDENTIFIED",
    "TIMER",
    "TX_MSG",
    "RX_MSG",
    "TRANSPORT_ERROR",
    "TSX_STATE",
    "RX_2XX_RESPONSE",
    "RX_ACK",
    "DISCARD_MSG",
    "USER",
    "BEFORE_TX",
};

static pj_str_t str_TEXT = { "text", 4},
		str_PLAIN = { "plain", 5 };
static int aux_mod_id;

struct aux_tsx_data
{
    void *token;
    void (*cb)(void*,pjsip_event*);
};

static pj_status_t aux_tsx_init( pjsip_endpoint *endpt,
				 struct pjsip_module *mod, pj_uint32_t id )
{
    PJ_UNUSED_ARG(endpt);
    PJ_UNUSED_ARG(mod);

    aux_mod_id = id;
    return 0;
}

static void aux_tsx_handler( struct pjsip_module *mod, pjsip_event *event )
{
    pjsip_transaction *tsx;
    struct aux_tsx_data *tsx_data;

    PJ_UNUSED_ARG(mod);

    if (event->type != PJSIP_EVENT_TSX_STATE)
	return;

    pj_assert(event->body.tsx_state.tsx != NULL);
    tsx = event->body.tsx_state.tsx;
    if (tsx == NULL)
	return;
    if (tsx->module_data[aux_mod_id] == NULL)
	return;
    if (tsx->status_code < 200)
	return;

    /* Call the callback, if any, and prevent the callback to be called again
     * by clearing the transaction's module_data.
     */
    tsx_data = tsx->module_data[aux_mod_id];
    tsx->module_data[aux_mod_id] = NULL;

    if (tsx_data->cb) {
	(*tsx_data->cb)(tsx_data->token, event);
    }
}

pjsip_module aux_tsx_module = 
{
    { "Aux-Tsx", 7},	    /* Name.		*/
    0,			    /* Flag		*/
    128,		    /* Priority		*/
    NULL,		    /* Arbitrary data.	*/
    0,			    /* Number of methods supported (none). */
    { 0 },		    /* Array of methods (none) */
    &aux_tsx_init,	    /* init_module()	*/
    NULL,		    /* start_module()	*/
    NULL,		    /* deinit_module()	*/
    &aux_tsx_handler,	    /* tsx_handler()	*/
};

PJ_DEF(pj_status_t) pjsip_endpt_send_request(  pjsip_endpoint *endpt,
					       pjsip_tx_data *tdata,
					       int timeout,
					       void *token,
					       void (*cb)(void*,pjsip_event*))
{
    pjsip_transaction *tsx;
    struct aux_tsx_data *tsx_data;
    pj_status_t status;

    status = pjsip_endpt_create_tsx(endpt, &tsx);
    if (!tsx) {
	pjsip_tx_data_dec_ref(tdata);
	return -1;
    }

    tsx_data = pj_pool_alloc(tsx->pool, sizeof(struct aux_tsx_data));
    tsx_data->token = token;
    tsx_data->cb = cb;
    tsx->module_data[aux_mod_id] = tsx_data;

    if (pjsip_tsx_init_uac(tsx, tdata) != 0) {
	pjsip_endpt_destroy_tsx(endpt, tsx);
	pjsip_tx_data_dec_ref(tdata);
	return -1;
    }

    pjsip_endpt_register_tsx(endpt, tsx);
    pjsip_tx_data_invalidate_msg(tdata);
    pjsip_tsx_on_tx_msg(tsx, tdata);
    pjsip_tx_data_dec_ref(tdata);
    return 0;
}

/*
 * Initialize transmit data (msg) with the headers and optional body.
 * This will just put the headers in the message as it is. Be carefull
 * when calling this function because once a header is put in a message, 
 * it CAN NOT be put in other message until the first message is deleted, 
 * because the way the header is put in the list.
 * That's why the session will shallow_clone it's headers before calling
 * this function.
 */
static void init_request_throw( pjsip_endpoint *endpt,
                                pjsip_tx_data *tdata, 
				pjsip_method *method,
				pjsip_uri *param_target,
				pjsip_from_hdr *param_from,
				pjsip_to_hdr *param_to, 
				pjsip_contact_hdr *param_contact,
				pjsip_cid_hdr *param_call_id,
				pjsip_cseq_hdr *param_cseq, 
				const pj_str_t *param_text)
{
    pjsip_msg *msg;
    pjsip_msg_body *body;
    const pjsip_hdr *endpt_hdr;

    /* Create the message. */
    msg = tdata->msg = pjsip_msg_create(tdata->pool, PJSIP_REQUEST_MSG);

    /* Init request URI. */
    pj_memcpy(&msg->line.req.method, method, sizeof(*method));
    msg->line.req.uri = param_target;

    /* Add additional request headers from endpoint. */
    endpt_hdr = pjsip_endpt_get_request_headers(endpt)->next;
    while (endpt_hdr != pjsip_endpt_get_request_headers(endpt)) {
	pjsip_hdr *hdr = pjsip_hdr_shallow_clone(tdata->pool, endpt_hdr);
	pjsip_msg_add_hdr( tdata->msg, hdr );
	endpt_hdr = endpt_hdr->next;
    }

    /* Add From header. */
    if (param_from->tag.slen == 0)
	pj_create_unique_string(tdata->pool, &param_from->tag);
    pjsip_msg_add_hdr(msg, (void*)param_from);

    /* Add To header. */
    pjsip_msg_add_hdr(msg, (void*)param_to);

    /* Add Contact header. */
    if (param_contact) {
	pjsip_msg_add_hdr(msg, (void*)param_contact);
    }

    /* Add Call-ID header. */
    pjsip_msg_add_hdr(msg, (void*)param_call_id);

    /* Add CSeq header. */
    pjsip_msg_add_hdr(msg, (void*)param_cseq);

    /* Create message body. */
    if (param_text) {
	body = pj_pool_calloc(tdata->pool, 1, sizeof(pjsip_msg_body));
	body->content_type.type = str_TEXT;
	body->content_type.subtype = str_PLAIN;
	body->data = pj_pool_alloc(tdata->pool, param_text->slen );
	pj_memcpy(body->data, param_text->ptr, param_text->slen);
	body->len = param_text->slen;
	body->print_body = &pjsip_print_text_body;
	msg->body = body;
    }
}

/*
 * Create arbitrary request.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_request(  pjsip_endpoint *endpt, 
						 const pjsip_method *method,
						 const pj_str_t *param_target,
						 const pj_str_t *param_from,
						 const pj_str_t *param_to, 
						 const pj_str_t *param_contact,
						 const pj_str_t *param_call_id,
						 int param_cseq, 
						 const pj_str_t *param_text,
						 pjsip_tx_data **p_tdata)
{
    pjsip_uri *target;
    pjsip_tx_data *tdata;
    pjsip_from_hdr *from;
    pjsip_to_hdr *to;
    pjsip_contact_hdr *contact;
    pjsip_cseq_hdr *cseq = NULL;    /* = NULL, warning in VC6 */
    pjsip_cid_hdr *call_id;
    pj_str_t tmp;
    pj_status_t status;
    PJ_USE_EXCEPTION;

    PJ_LOG(5,(THIS_FILE, "Entering pjsip_endpt_create_request()"));

    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Init reference counter to 1. */
    pjsip_tx_data_add_ref(tdata);

    PJ_TRY {
	/* Request target. */
	pj_strdup_with_null(tdata->pool, &tmp, param_target);
	target = pjsip_parse_uri( tdata->pool, tmp.ptr, tmp.slen, 0);
	if (target == NULL) {
	    PJ_LOG(4,(THIS_FILE, "Error creating request: invalid target %s", 
		      tmp.ptr));
	    goto on_error;
	}

	/* From */
	from = pjsip_from_hdr_create(tdata->pool);
	pj_strdup_with_null(tdata->pool, &tmp, param_from);
	from->uri = pjsip_parse_uri( tdata->pool, tmp.ptr, tmp.slen, 
				     PJSIP_PARSE_URI_AS_NAMEADDR);
	if (from->uri == NULL) {
	    PJ_LOG(4,(THIS_FILE, "Error creating request: invalid 'From' URI '%s'",
				tmp.ptr));
	    goto on_error;
	}
	pj_create_unique_string(tdata->pool, &from->tag);

	/* To */
	to = pjsip_to_hdr_create(tdata->pool);
	pj_strdup_with_null(tdata->pool, &tmp, param_to);
	to->uri = pjsip_parse_uri( tdata->pool, tmp.ptr, tmp.slen, 
				   PJSIP_PARSE_URI_AS_NAMEADDR);
	if (to->uri == NULL) {
	    PJ_LOG(4,(THIS_FILE, "Error creating request: invalid 'To' URI '%s'",
				tmp.ptr));
	    goto on_error;
	}

	/* Contact. */
	if (param_contact) {
	    contact = pjsip_contact_hdr_create(tdata->pool);
	    pj_strdup_with_null(tdata->pool, &tmp, param_contact);
	    contact->uri = pjsip_parse_uri( tdata->pool, tmp.ptr, tmp.slen,
					    PJSIP_PARSE_URI_AS_NAMEADDR);
	    if (contact->uri == NULL) {
		PJ_LOG(4,(THIS_FILE, 
			  "Error creating request: invalid 'Contact' URI '%s'",
			  tmp.ptr));
		goto on_error;
	    }
	} else {
	    contact = NULL;
	}

	/* Call-ID */
	call_id = pjsip_cid_hdr_create(tdata->pool);
	if (param_call_id != NULL && param_call_id->slen)
	    pj_strdup(tdata->pool, &call_id->id, param_call_id);
	else
	    pj_create_unique_string(tdata->pool, &call_id->id);

	/* CSeq */
	cseq = pjsip_cseq_hdr_create(tdata->pool);
	if (param_cseq >= 0)
	    cseq->cseq = param_cseq;
	else
	    cseq->cseq = pj_rand() & 0xFFFF;

	/* Method */
	pjsip_method_copy(tdata->pool, &cseq->method, method);

	/* Create the request. */
	init_request_throw( endpt, tdata, &cseq->method, target, from, to, 
                            contact, call_id, cseq, param_text);
    }
    PJ_DEFAULT {
	status = PJ_ENOMEM;
	goto on_error;
    }
    PJ_END

    PJ_LOG(4,(THIS_FILE, "Request %s (%d %.*s) created.", 
		        tdata->obj_name, 
			cseq->cseq, 
			cseq->method.name.slen,
			cseq->method.name.ptr));

    *p_tdata = tdata;
    return PJ_SUCCESS;

on_error:
    pjsip_tx_data_dec_ref(tdata);
    return status;
}

PJ_DEF(pj_status_t)
pjsip_endpt_create_request_from_hdr( pjsip_endpoint *endpt,
				     const pjsip_method *method,
				     const pjsip_uri *param_target,
				     const pjsip_from_hdr *param_from,
				     const pjsip_to_hdr *param_to,
				     const pjsip_contact_hdr *param_contact,
				     const pjsip_cid_hdr *param_call_id,
				     int param_cseq,
				     const pj_str_t *param_text,
				     pjsip_tx_data **p_tdata)
{
    pjsip_uri *target;
    pjsip_tx_data *tdata;
    pjsip_from_hdr *from;
    pjsip_to_hdr *to;
    pjsip_contact_hdr *contact;
    pjsip_cid_hdr *call_id;
    pjsip_cseq_hdr *cseq = NULL; /* The NULL because warning in VC6 */
    pj_status_t status;
    PJ_USE_EXCEPTION;

    PJ_LOG(5,(THIS_FILE, "Entering pjsip_endpt_create_request_from_hdr()"));

    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    pjsip_tx_data_add_ref(tdata);

    PJ_TRY {
	target = pjsip_uri_clone(tdata->pool, param_target);
	from = pjsip_hdr_shallow_clone(tdata->pool, param_from);
	pjsip_fromto_set_from(from);
	to = pjsip_hdr_shallow_clone(tdata->pool, param_to);
	pjsip_fromto_set_to(to);
	if (param_contact)
	    contact = pjsip_hdr_shallow_clone(tdata->pool, param_contact);
	else
	    contact = NULL;
	call_id = pjsip_hdr_shallow_clone(tdata->pool, param_call_id);
	cseq = pjsip_cseq_hdr_create(tdata->pool);
	if (param_cseq >= 0)
	    cseq->cseq = param_cseq;
	else
	    cseq->cseq = pj_rand() % 0xFFFF;
	pjsip_method_copy(tdata->pool, &cseq->method, method);

	init_request_throw(endpt, tdata, &cseq->method, target, from, to, 
                           contact, call_id, cseq, param_text);
    }
    PJ_DEFAULT {
	status = PJ_ENOMEM;
	goto on_error;
    }
    PJ_END;

    PJ_LOG(4,(THIS_FILE, "Request %s (%d %.*s) created.", 
			tdata->obj_name, 
			cseq->cseq, 
			cseq->method.name.slen,
			cseq->method.name.ptr));

    *p_tdata = tdata;
    return PJ_SUCCESS;

on_error:
    pjsip_tx_data_dec_ref(tdata);
    return status;
}

/*
 * Construct a minimal response message for the received request.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_response( pjsip_endpoint *endpt,
						 const pjsip_rx_data *rdata,
						 int code,
						 pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg, *req_msg;
    pjsip_hdr *hdr;
    pjsip_via_hdr *via;
    pjsip_rr_hdr *rr;
    pj_status_t status;

    /* rdata must be a request message. */
    req_msg = rdata->msg;
    pj_assert(req_msg->type == PJSIP_REQUEST_MSG);

    /* Log this action. */
    PJ_LOG(5,(THIS_FILE, "pjsip_endpt_create_response(rdata=%p, code=%d)", 
		         rdata, code));

    /* Create a new transmit buffer. */
    status = pjsip_endpt_create_tdata( endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create new response message. */
    tdata->msg = msg = pjsip_msg_create(tdata->pool, PJSIP_RESPONSE_MSG);

    /* Set status code and reason text. */
    msg->line.status.code = code;
    msg->line.status.reason = *pjsip_get_status_text(code);

    /* Set TX data attributes. */
    tdata->rx_timestamp = rdata->timestamp;

    /* Copy all the via headers, in order. */
    via = rdata->via;
    while (via) {
	pjsip_msg_add_hdr( msg, pjsip_hdr_clone(tdata->pool, via));
	via = via->next;
	if (via != (void*)&req_msg->hdr)
	    via = pjsip_msg_find_hdr(req_msg, PJSIP_H_VIA, via);
	else
	    break;
    }

    /* Copy all Record-Route headers, in order. */
    rr = pjsip_msg_find_hdr(req_msg, PJSIP_H_RECORD_ROUTE, NULL);
    while (rr) {
	pjsip_msg_add_hdr(msg, pjsip_hdr_clone(tdata->pool, rr));
	rr = rr->next;
	if (rr != (void*)&req_msg->hdr)
	    rr = pjsip_msg_find_hdr(req_msg, PJSIP_H_RECORD_ROUTE, rr);
	else
	    break;
    }

    /* Copy Call-ID header. */
    hdr = pjsip_msg_find_hdr( req_msg, PJSIP_H_CALL_ID, NULL);
    pjsip_msg_add_hdr(msg, pjsip_hdr_clone(tdata->pool, hdr));

    /* Copy From header. */
    hdr = pjsip_hdr_clone(tdata->pool, rdata->from);
    pjsip_msg_add_hdr( msg, hdr);

    /* Copy To header. */
    hdr = pjsip_hdr_clone(tdata->pool, rdata->to);
    pjsip_msg_add_hdr( msg, hdr);

    /* Copy CSeq header. */
    hdr = pjsip_hdr_clone(tdata->pool, rdata->cseq);
    pjsip_msg_add_hdr( msg, hdr);

    /* All done. */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


/*
 * Construct ACK for 3xx-6xx final response (according to chapter 17.1.1 of
 * RFC3261). Note that the generation of ACK for 2xx response is different,
 * and one must not use this function to generate such ACK.
 */
PJ_DEF(void) pjsip_endpt_create_ack(pjsip_endpoint *endpt,
				    pjsip_tx_data *tdata,
				    const pjsip_rx_data *rdata )
{
    pjsip_msg *ack_msg, *invite_msg;
    pjsip_to_hdr *to;
    pjsip_from_hdr *from;
    pjsip_cseq_hdr *cseq;
    pjsip_hdr *hdr;

    /* Make compiler happy. */
    PJ_UNUSED_ARG(endpt);

    /* rdata must be a final response. */
    pj_assert(rdata->msg->type==PJSIP_RESPONSE_MSG &&
	      rdata->msg->line.status.code >= 300);

    /* Log this action. */
    PJ_LOG(5,(THIS_FILE, "pjsip_endpt_create_ack(rdata=%p)", rdata));

    /* Create new request message. */
    ack_msg = pjsip_msg_create(tdata->pool, PJSIP_REQUEST_MSG);
    pjsip_method_set( &ack_msg->line.req.method, PJSIP_ACK_METHOD );

    /* The original INVITE message. */
    invite_msg = tdata->msg;

    /* Copy Request-Uri from the original INVITE. */
    ack_msg->line.req.uri = invite_msg->line.req.uri;
    
    /* Copy Call-ID from the original INVITE */
    hdr = pjsip_msg_find_remove_hdr( invite_msg, PJSIP_H_CALL_ID, NULL);
    pjsip_msg_add_hdr( ack_msg, hdr );

    /* Copy From header from the original INVITE. */
    from = (pjsip_from_hdr*)pjsip_msg_find_remove_hdr(invite_msg, 
						      PJSIP_H_FROM, NULL);
    pjsip_msg_add_hdr( ack_msg, (pjsip_hdr*)from );

    /* Copy To header from the original INVITE. */
    to = (pjsip_to_hdr*)pjsip_msg_find_remove_hdr( invite_msg, 
						   PJSIP_H_TO, NULL);
    pj_strdup(tdata->pool, &to->tag, &rdata->to->tag);
    pjsip_msg_add_hdr( ack_msg, (pjsip_hdr*)to );

    /* Must contain single Via, just as the original INVITE. */
    hdr = pjsip_msg_find_remove_hdr( invite_msg, PJSIP_H_VIA, NULL);
    pjsip_msg_insert_first_hdr( ack_msg, hdr );

    /* Must have the same CSeq value as the original INVITE, but method 
     * changed to ACK 
     */
    cseq = (pjsip_cseq_hdr*) pjsip_msg_find_remove_hdr( invite_msg, 
							PJSIP_H_CSEQ, NULL);
    pjsip_method_set( &cseq->method, PJSIP_ACK_METHOD );
    pjsip_msg_add_hdr( ack_msg, (pjsip_hdr*) cseq );

    /* If the original INVITE has Route headers, those header fields MUST 
     * appear in the ACK.
     */
    hdr = pjsip_msg_find_remove_hdr( invite_msg, PJSIP_H_ROUTE, NULL);
    while (hdr != NULL) {
	pjsip_msg_add_hdr( ack_msg, hdr );
	hdr = pjsip_msg_find_remove_hdr( invite_msg, PJSIP_H_ROUTE, NULL);
    }

    /* Set the message in the "tdata" to point to the ACK message. */
    tdata->msg = ack_msg;

    /* Reset transmit packet buffer, to force 're-printing' of message. */
    tdata->buf.cur = tdata->buf.start;

    /* We're done.
     * "tdata" parameter now contains the ACK message.
     */
}


/*
 * Construct CANCEL request for the previously sent request, according to
 * chapter 9.1 of RFC3261.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_cancel( pjsip_endpoint *endpt,
					       pjsip_tx_data *req_tdata,
					       pjsip_tx_data **p_tdata)
{
    pjsip_msg *req_msg;	/* the original request. */
    pjsip_tx_data *cancel_tdata;
    pjsip_msg *cancel_msg;
    pjsip_hdr *hdr;
    pjsip_cseq_hdr *req_cseq, *cseq;
    pjsip_uri *req_uri;
    pj_status_t status;

    /* Log this action. */
    PJ_LOG(5,(THIS_FILE, "pjsip_endpt_create_cancel(tdata=%p)", req_tdata));

    /* Get the original request. */
    req_msg = req_tdata->msg;

    /* The transmit buffer must INVITE request. */
    PJ_ASSERT_RETURN(req_msg->type == PJSIP_REQUEST_MSG &&
		     req_msg->line.req.method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVAL);

    /* Create new transmit buffer. */
    status = pjsip_endpt_create_tdata( endpt, &cancel_tdata);
    if (status != PJ_SUCCESS) {
	return status;
    }

    /* Create CANCEL request message. */
    cancel_msg = pjsip_msg_create(cancel_tdata->pool, PJSIP_REQUEST_MSG);
    cancel_tdata->msg = cancel_msg;

    /* Request-URI, Call-ID, From, To, and the numeric part of the CSeq are
     * copied from the original request.
     */
    /* Set request line. */
    pjsip_method_set(&cancel_msg->line.req.method, PJSIP_CANCEL_METHOD);
    req_uri = req_msg->line.req.uri;
    cancel_msg->line.req.uri = pjsip_uri_clone(cancel_tdata->pool, req_uri);

    /* Copy Call-ID */
    hdr = pjsip_msg_find_hdr(req_msg, PJSIP_H_CALL_ID, NULL);
    pjsip_msg_add_hdr(cancel_msg, pjsip_hdr_clone(cancel_tdata->pool, hdr));

    /* Copy From header. */
    hdr = pjsip_msg_find_hdr(req_msg, PJSIP_H_FROM, NULL);
    pjsip_msg_add_hdr(cancel_msg, pjsip_hdr_clone(cancel_tdata->pool, hdr));

    /* Copy To header. */
    hdr = pjsip_msg_find_hdr(req_msg, PJSIP_H_TO, NULL);
    pjsip_msg_add_hdr(cancel_msg, pjsip_hdr_clone(cancel_tdata->pool, hdr));

    /* Create new CSeq with equal number, but method set to CANCEL. */
    req_cseq = (pjsip_cseq_hdr*) pjsip_msg_find_hdr(req_msg, PJSIP_H_CSEQ, NULL);
    cseq = pjsip_cseq_hdr_create(cancel_tdata->pool);
    cseq->cseq = req_cseq->cseq;
    pjsip_method_set(&cseq->method, PJSIP_CANCEL_METHOD);
    pjsip_msg_add_hdr(cancel_msg, (pjsip_hdr*)cseq);

    /* Must only have single Via which matches the top-most Via in the 
     * request being cancelled. 
     */
    hdr = pjsip_msg_find_hdr(req_msg, PJSIP_H_VIA, NULL);
    pjsip_msg_insert_first_hdr(cancel_msg, 
			       pjsip_hdr_clone(cancel_tdata->pool, hdr));

    /* If the original request has Route header, the CANCEL request must also
     * has exactly the same.
     * Copy "Route" header from the request.
     */
    hdr = pjsip_msg_find_hdr(req_msg, PJSIP_H_ROUTE, NULL);
    while (hdr != NULL) {
	pjsip_msg_add_hdr(cancel_msg, pjsip_hdr_clone(cancel_tdata->pool, hdr));
	hdr = hdr->next;
	if (hdr != &cancel_msg->hdr)
	    hdr = pjsip_msg_find_hdr(req_msg, PJSIP_H_ROUTE, hdr);
	else
	    break;
    }

    /* Done.
     * Return the transmit buffer containing the CANCEL request.
     */
    *p_tdata = cancel_tdata;
    return PJ_SUCCESS;
}

/* Get the address parameters (host, port, flag, TTL, etc) to send the
 * response.
 */
PJ_DEF(pj_status_t) pjsip_get_response_addr(pj_pool_t *pool,
					    const pjsip_transport_t *req_transport,
					    const pjsip_via_hdr *via,
					    pjsip_host_port *send_addr)
{
    /* Determine the destination address (section 18.2.2):
     * - for TCP, SCTP, or TLS, send the response using the transport where
     *   the request was received.
     * - if maddr parameter is present, send to this address using the port
     *   in sent-by or 5060. If multicast is used, the TTL in the Via must
     *   be used, or 1 if ttl parameter is not present.
     * - otherwise if received parameter is present, set to this address.
     * - otherwise send to the address in sent-by.
     */
    send_addr->flag = pjsip_transport_get_flag(req_transport);
    send_addr->type = pjsip_transport_get_type(req_transport);

    if (PJSIP_TRANSPORT_IS_RELIABLE(req_transport)) {
	const pj_sockaddr_in *remote_addr;
	remote_addr = pjsip_transport_get_remote_addr(req_transport);
	pj_strdup2(pool, &send_addr->host, 
		   pj_inet_ntoa(remote_addr->sin_addr));
	send_addr->port = pj_sockaddr_in_get_port(remote_addr);

    } else {
	/* Set the host part */
	if (via->maddr_param.slen) {
	    pj_strdup(pool, &send_addr->host, &via->maddr_param);
	} else if (via->recvd_param.slen) {
	    pj_strdup(pool, &send_addr->host, &via->recvd_param);
	} else {
	    pj_strdup(pool, &send_addr->host, &via->sent_by.host);
	}

	/* Set the port */
	send_addr->port = via->sent_by.port;
    }

    return PJ_SUCCESS;
}

/*
 * Get the event string from the event ID.
 */
PJ_DEF(const char *) pjsip_event_str(pjsip_event_id_e e)
{
    return event_str[e];
}


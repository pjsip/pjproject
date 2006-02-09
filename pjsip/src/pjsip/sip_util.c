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
#include <pjsip/sip_util.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_errno.h>
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
    "USER",
};

static pj_str_t str_TEXT = { "text", 4},
		str_PLAIN = { "plain", 5 };

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
    pjsip_via_hdr *via;
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

    /* Add a blank Via header in the front of the message. */
    via = pjsip_via_hdr_create(tdata->pool);
    via->rport_param = 0;
    pjsip_msg_insert_first_hdr(msg, (void*)via);

    /* Add header params as request headers */
    if (PJSIP_URI_SCHEME_IS_SIP(param_target) || 
	PJSIP_URI_SCHEME_IS_SIPS(param_target)) 
    {
	pjsip_sip_uri *uri = (pjsip_sip_uri*) pjsip_uri_get_uri(param_target);
	pjsip_param *hparam;

	hparam = uri->header_param.next;
	while (hparam != &uri->header_param) {
	    pjsip_generic_string_hdr *hdr;

	    hdr = pjsip_generic_string_hdr_create(tdata->pool, 
						  &hparam->name,
						  &hparam->value);
	    pjsip_msg_add_hdr(msg, (pjsip_hdr*)hdr);
	    hparam = hparam->next;
	}
    }

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

    PJ_LOG(5,(THIS_FILE, "%s created.", 
			 pjsip_tx_data_get_info(tdata)));

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
	    status = PJSIP_EINVALIDREQURI;
	    goto on_error;
	}

	/* From */
	from = pjsip_from_hdr_create(tdata->pool);
	pj_strdup_with_null(tdata->pool, &tmp, param_from);
	from->uri = pjsip_parse_uri( tdata->pool, tmp.ptr, tmp.slen, 
				     PJSIP_PARSE_URI_AS_NAMEADDR);
	if (from->uri == NULL) {
	    status = PJSIP_EINVALIDHDR;
	    goto on_error;
	}
	pj_create_unique_string(tdata->pool, &from->tag);

	/* To */
	to = pjsip_to_hdr_create(tdata->pool);
	pj_strdup_with_null(tdata->pool, &tmp, param_to);
	to->uri = pjsip_parse_uri( tdata->pool, tmp.ptr, tmp.slen, 
				   PJSIP_PARSE_URI_AS_NAMEADDR);
	if (to->uri == NULL) {
	    status = PJSIP_EINVALIDHDR;
	    goto on_error;
	}

	/* Contact. */
	if (param_contact) {
	    contact = pjsip_contact_hdr_create(tdata->pool);
	    pj_strdup_with_null(tdata->pool, &tmp, param_contact);
	    contact->uri = pjsip_parse_uri( tdata->pool, tmp.ptr, tmp.slen,
					    PJSIP_PARSE_URI_AS_NAMEADDR);
	    if (contact->uri == NULL) {
		status = PJSIP_EINVALIDHDR;
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
    PJ_CATCH_ANY {
	status = PJ_ENOMEM;
	goto on_error;
    }
    PJ_END

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

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && method && param_target && param_from &&
		     param_to && p_tdata, PJ_EINVAL);

    /* Create new transmit data. */
    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Set initial reference counter to 1. */
    pjsip_tx_data_add_ref(tdata);

    PJ_TRY {
	/* Duplicate target URI and headers. */
	target = pjsip_uri_clone(tdata->pool, param_target);
	from = pjsip_hdr_clone(tdata->pool, param_from);
	pjsip_fromto_hdr_set_from(from);
	to = pjsip_hdr_clone(tdata->pool, param_to);
	pjsip_fromto_hdr_set_to(to);
	if (param_contact)
	    contact = pjsip_hdr_clone(tdata->pool, param_contact);
	else
	    contact = NULL;
	call_id = pjsip_hdr_clone(tdata->pool, param_call_id);
	cseq = pjsip_cseq_hdr_create(tdata->pool);
	if (param_cseq >= 0)
	    cseq->cseq = param_cseq;
	else
	    cseq->cseq = pj_rand() % 0xFFFF;
	pjsip_method_copy(tdata->pool, &cseq->method, method);

	/* Copy headers to the request. */
	init_request_throw(endpt, tdata, &cseq->method, target, from, to, 
                           contact, call_id, cseq, param_text);
    }
    PJ_CATCH_ANY {
	status = PJ_ENOMEM;
	goto on_error;
    }
    PJ_END;

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
						 int st_code,
						 const pj_str_t *st_text,
						 pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg, *req_msg;
    pjsip_hdr *hdr;
    pjsip_via_hdr *via;
    pjsip_rr_hdr *rr;
    pj_status_t status;

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && rdata && p_tdata, PJ_EINVAL);

    /* Check status code. */
    PJ_ASSERT_RETURN(st_code >= 100 && st_code <= 699, PJ_EINVAL);

    /* rdata must be a request message. */
    req_msg = rdata->msg_info.msg;
    pj_assert(req_msg->type == PJSIP_REQUEST_MSG);

    /* Create a new transmit buffer. */
    status = pjsip_endpt_create_tdata( endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Set initial reference count to 1. */
    pjsip_tx_data_add_ref(tdata);

    /* Create new response message. */
    tdata->msg = msg = pjsip_msg_create(tdata->pool, PJSIP_RESPONSE_MSG);

    /* Set status code and reason text. */
    msg->line.status.code = st_code;
    if (st_text)
	pj_strdup(tdata->pool, &msg->line.status.reason, st_text);
    else
	msg->line.status.reason = *pjsip_get_status_text(st_code);

    /* Set TX data attributes. */
    tdata->rx_timestamp = rdata->pkt_info.timestamp;

    /* Copy all the via headers, in order. */
    via = rdata->msg_info.via;
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
    hdr = pjsip_hdr_clone(tdata->pool, rdata->msg_info.from);
    pjsip_msg_add_hdr( msg, hdr);

    /* Copy To header. */
    hdr = pjsip_hdr_clone(tdata->pool, rdata->msg_info.to);
    pjsip_msg_add_hdr( msg, hdr);

    /* Copy CSeq header. */
    hdr = pjsip_hdr_clone(tdata->pool, rdata->msg_info.cseq);
    pjsip_msg_add_hdr( msg, hdr);

    /* All done. */
    *p_tdata = tdata;

    PJ_LOG(5,(THIS_FILE, "%s created", pjsip_tx_data_get_info(tdata)));
    return PJ_SUCCESS;
}


/*
 * Construct ACK for 3xx-6xx final response (according to chapter 17.1.1 of
 * RFC3261). Note that the generation of ACK for 2xx response is different,
 * and one must not use this function to generate such ACK.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_ack( pjsip_endpoint *endpt,
					    const pjsip_tx_data *tdata,
					    const pjsip_rx_data *rdata,
					    pjsip_tx_data **ack_tdata)
{
    pjsip_tx_data *ack = NULL;
    const pjsip_msg *invite_msg;
    const pjsip_from_hdr *from_hdr;
    const pjsip_to_hdr *to_hdr;
    const pjsip_cid_hdr *cid_hdr;
    const pjsip_cseq_hdr *cseq_hdr;
    const pjsip_hdr *hdr;
    pjsip_hdr *via;
    pjsip_to_hdr *to;
    pj_status_t status;

    /* rdata must be a non-2xx final response. */
    pj_assert(rdata->msg_info.msg->type==PJSIP_RESPONSE_MSG &&
	      rdata->msg_info.msg->line.status.code >= 300);

    /* Initialize return value to NULL. */
    *ack_tdata = NULL;

    /* The original INVITE message. */
    invite_msg = tdata->msg;

    /* Get the headers from original INVITE request. */
#   define FIND_HDR(m,HNAME) pjsip_msg_find_hdr(m, PJSIP_H_##HNAME, NULL)

    from_hdr = (const pjsip_from_hdr*) FIND_HDR(invite_msg, FROM);
    PJ_ASSERT_ON_FAIL(from_hdr != NULL, goto on_missing_hdr);

    to_hdr = (const pjsip_to_hdr*) FIND_HDR(invite_msg, TO);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

    cid_hdr = (const pjsip_cid_hdr*) FIND_HDR(invite_msg, CALL_ID);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

    cseq_hdr = (const pjsip_cseq_hdr*) FIND_HDR(invite_msg, CSEQ);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

#   undef FIND_HDR

    /* Create new request message from the headers. */
    status = pjsip_endpt_create_request_from_hdr(endpt, 
						 &pjsip_ack_method,
						 tdata->msg->line.req.uri,
						 from_hdr, to_hdr,
						 NULL, cid_hdr,
						 cseq_hdr->cseq, NULL,
						 &ack);

    if (status != PJ_SUCCESS)
	return status;

    /* Update tag in To header with the one from the response (if any). */
    to = (pjsip_to_hdr*) pjsip_msg_find_hdr(ack->msg, PJSIP_H_TO, NULL);
    pj_strdup(ack->pool, &to->tag, &rdata->msg_info.to->tag);


    /* Clear Via headers in the new request. */
    while ((via=pjsip_msg_find_hdr(ack->msg, PJSIP_H_VIA, NULL)) != NULL)
	pj_list_erase(via);

    /* Must contain single Via, just as the original INVITE. */
    hdr = pjsip_msg_find_hdr( invite_msg, PJSIP_H_VIA, NULL);
    pjsip_msg_insert_first_hdr( ack->msg, pjsip_hdr_clone(ack->pool,hdr) );

    /* If the original INVITE has Route headers, those header fields MUST 
     * appear in the ACK.
     */
    hdr = pjsip_msg_find_hdr( invite_msg, PJSIP_H_ROUTE, NULL);
    while (hdr != NULL) {
	pjsip_msg_add_hdr( ack->msg, pjsip_hdr_clone(ack->pool, hdr) );
	hdr = hdr->next;
	if (hdr == &invite_msg->hdr)
	    break;
	hdr = pjsip_msg_find_hdr( invite_msg, PJSIP_H_ROUTE, hdr);
    }

    /* We're done.
     * "tdata" parameter now contains the ACK message.
     */
    *ack_tdata = ack;
    return PJ_SUCCESS;

on_missing_hdr:
    if (ack)
	pjsip_tx_data_dec_ref(ack);
    return PJSIP_EMISSINGHDR;
}


/*
 * Construct CANCEL request for the previously sent request, according to
 * chapter 9.1 of RFC3261.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_cancel( pjsip_endpoint *endpt,
					       const pjsip_tx_data *req_tdata,
					       pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *cancel_tdata = NULL;
    const pjsip_from_hdr *from_hdr;
    const pjsip_to_hdr *to_hdr;
    const pjsip_cid_hdr *cid_hdr;
    const pjsip_cseq_hdr *cseq_hdr;
    const pjsip_hdr *hdr;
    pjsip_hdr *via;
    pj_status_t status;

    /* The transmit buffer must INVITE request. */
    PJ_ASSERT_RETURN(req_tdata->msg->type == PJSIP_REQUEST_MSG &&
		     req_tdata->msg->line.req.method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVAL);

    /* Get the headers from original INVITE request. */
#   define FIND_HDR(m,HNAME) pjsip_msg_find_hdr(m, PJSIP_H_##HNAME, NULL)

    from_hdr = (const pjsip_from_hdr*) FIND_HDR(req_tdata->msg, FROM);
    PJ_ASSERT_ON_FAIL(from_hdr != NULL, goto on_missing_hdr);

    to_hdr = (const pjsip_to_hdr*) FIND_HDR(req_tdata->msg, TO);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

    cid_hdr = (const pjsip_cid_hdr*) FIND_HDR(req_tdata->msg, CALL_ID);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

    cseq_hdr = (const pjsip_cseq_hdr*) FIND_HDR(req_tdata->msg, CSEQ);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

#   undef FIND_HDR

    /* Create new request message from the headers. */
    status = pjsip_endpt_create_request_from_hdr(endpt, 
						 &pjsip_cancel_method,
						 req_tdata->msg->line.req.uri,
						 from_hdr, to_hdr,
						 NULL, cid_hdr,
						 cseq_hdr->cseq, NULL,
						 &cancel_tdata);

    if (status != PJ_SUCCESS)
	return status;

    /* Clear Via headers in the new request. */
    while ((via=pjsip_msg_find_hdr(cancel_tdata->msg, PJSIP_H_VIA, NULL)) != NULL)
	pj_list_erase(via);


    /* Must only have single Via which matches the top-most Via in the 
     * request being cancelled. 
     */
    hdr = pjsip_msg_find_hdr(req_tdata->msg, PJSIP_H_VIA, NULL);
    if (hdr) {
	pjsip_msg_insert_first_hdr(cancel_tdata->msg, 
				   pjsip_hdr_clone(cancel_tdata->pool, hdr));
    }

    /* If the original request has Route header, the CANCEL request must also
     * has exactly the same.
     * Copy "Route" header from the request.
     */
    hdr = pjsip_msg_find_hdr(req_tdata->msg, PJSIP_H_ROUTE, NULL);
    while (hdr != NULL) {
	pjsip_msg_add_hdr(cancel_tdata->msg, 
			  pjsip_hdr_clone(cancel_tdata->pool, hdr));
	hdr = hdr->next;
	if (hdr != &cancel_tdata->msg->hdr)
	    hdr = pjsip_msg_find_hdr(cancel_tdata->msg, PJSIP_H_ROUTE, hdr);
	else
	    break;
    }

    /* Done.
     * Return the transmit buffer containing the CANCEL request.
     */
    *p_tdata = cancel_tdata;
    return PJ_SUCCESS;

on_missing_hdr:
    if (cancel_tdata)
	pjsip_tx_data_dec_ref(cancel_tdata);
    return PJSIP_EMISSINGHDR;
}


/*
 * Find which destination to be used to send the request message, based
 * on the request URI and Route headers in the message. The procedure
 * used here follows the guidelines on sending the request in RFC 3261
 * chapter 8.1.2.
 */
PJ_DEF(pj_status_t) pjsip_get_request_addr( pjsip_tx_data *tdata,
					    pjsip_host_info *dest_info )
{
    const pjsip_uri *new_request_uri, *target_uri;
    const pjsip_name_addr *topmost_route_uri;
    pjsip_route_hdr *first_route_hdr, *last_route_hdr;
    
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_REQUEST_MSG, 
		     PJSIP_ENOTREQUESTMSG);
    PJ_ASSERT_RETURN(dest_info != NULL, PJ_EINVAL);

    /* Get the first "Route" header from the message. If the message doesn't
     * have any "Route" headers but the endpoint has, then copy the "Route"
     * headers from the endpoint first.
     */
    last_route_hdr = first_route_hdr = 
	pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, NULL);
    if (first_route_hdr) {
	topmost_route_uri = &first_route_hdr->name_addr;
	while (last_route_hdr->next != (void*)&tdata->msg->hdr) {
	    pjsip_route_hdr *hdr;
	    hdr = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, 
                                     last_route_hdr->next);
	    if (!hdr)
		break;
	    last_route_hdr = hdr;
	}
    } else {
	topmost_route_uri = NULL;
    }

    /* If Route headers exist, and the first element indicates loose-route,
     * the URI is taken from the Request-URI, and we keep all existing Route
     * headers intact.
     * If Route headers exist, and the first element DOESN'T indicate loose
     * route, the URI is taken from the first Route header, and remove the
     * first Route header from the message.
     * Otherwise if there's no Route headers, the URI is taken from the
     * Request-URI.
     */
    if (topmost_route_uri) {
	pj_bool_t has_lr_param;

	if (PJSIP_URI_SCHEME_IS_SIP(topmost_route_uri) ||
	    PJSIP_URI_SCHEME_IS_SIPS(topmost_route_uri))
	{
	    const pjsip_sip_uri *url = 
		pjsip_uri_get_uri((void*)topmost_route_uri);
	    has_lr_param = url->lr_param;
	} else {
	    has_lr_param = 0;
	}

	if (has_lr_param) {
	    new_request_uri = tdata->msg->line.req.uri;
	    /* We shouldn't need to delete topmost Route if it has lr param.
	     * But seems like it breaks some proxy implementation, so we
	     * delete it anyway.
	     */
	    /*
	    pj_list_erase(first_route_hdr);
	    if (first_route_hdr == last_route_hdr)
		last_route_hdr = NULL;
	    */
	} else {
	    new_request_uri = pjsip_uri_get_uri((void*)topmost_route_uri);
	    pj_list_erase(first_route_hdr);
	    if (first_route_hdr == last_route_hdr)
		last_route_hdr = NULL;
	}

	target_uri = (pjsip_uri*)topmost_route_uri;

    } else {
	target_uri = new_request_uri = tdata->msg->line.req.uri;
    }

    /* The target URI must be a SIP/SIPS URL so we can resolve it's address.
     * Otherwise we're in trouble (i.e. there's no host part in tel: URL).
     */
    pj_memset(dest_info, 0, sizeof(*dest_info));

    if (PJSIP_URI_SCHEME_IS_SIPS(target_uri)) {
	pjsip_uri *uri = (pjsip_uri*) target_uri;
	const pjsip_sip_uri *url=(const pjsip_sip_uri*)pjsip_uri_get_uri(uri);
	dest_info->flag |= (PJSIP_TRANSPORT_SECURE | PJSIP_TRANSPORT_RELIABLE);
	pj_strdup(tdata->pool, &dest_info->addr.host, &url->host);
        dest_info->addr.port = url->port;
	dest_info->type = 
            pjsip_transport_get_type_from_name(&url->transport_param);

    } else if (PJSIP_URI_SCHEME_IS_SIP(target_uri)) {
	pjsip_uri *uri = (pjsip_uri*) target_uri;
	const pjsip_sip_uri *url=(const pjsip_sip_uri*)pjsip_uri_get_uri(uri);
	pj_strdup(tdata->pool, &dest_info->addr.host, &url->host);
	dest_info->addr.port = url->port;
	dest_info->type = 
            pjsip_transport_get_type_from_name(&url->transport_param);
	dest_info->flag = 
	    pjsip_transport_get_flag_from_type(dest_info->type);
    } else {
        pj_assert(!"Unsupported URI scheme!");
	PJ_TODO(SUPPORT_REQUEST_ADDR_RESOLUTION_FOR_TEL_URI);
	return PJSIP_EINVALIDSCHEME;
    }

    /* If target URI is different than request URI, replace 
     * request URI add put the original URI in the last Route header.
     */
    if (new_request_uri && new_request_uri!=tdata->msg->line.req.uri) {
	pjsip_route_hdr *route = pjsip_route_hdr_create(tdata->pool);
	route->name_addr.uri = tdata->msg->line.req.uri;
	if (last_route_hdr)
	    pj_list_insert_after(last_route_hdr, route);
	else
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)route);
	tdata->msg->line.req.uri = (pjsip_uri*)new_request_uri;
    }

    /* Success. */
    return PJ_SUCCESS;  
}


/* Transport callback for sending stateless request. 
 * This is one of the most bizzare function in pjsip, so
 * good luck if you happen to debug this function!!
 */
static void stateless_send_transport_cb( void *token,
					 pjsip_tx_data *tdata,
					 pj_ssize_t sent )
{
    pjsip_send_state *stateless_data = token;

    PJ_UNUSED_ARG(tdata);
    pj_assert(tdata == stateless_data->tdata);

    for (;;) {
	pj_status_t status;
	pj_bool_t cont;

	pj_sockaddr_t *cur_addr;
	pjsip_transport_type_e cur_addr_type;
	int cur_addr_len;

	pjsip_via_hdr *via;

	if (sent == -PJ_EPENDING) {
	    /* This is the initial process.
	     * When the process started, this function will be called by
	     * stateless_send_resolver_callback() with sent argument set to
	     * -PJ_EPENDING.
	     */
	    cont = PJ_TRUE;
	} else {
	    /* There are two conditions here:
	     * (1) Message is sent (i.e. sent > 0),
	     * (2) Failure (i.e. sent <= 0)
	     */
	    cont = (sent > 0) ? PJ_FALSE :
		   (stateless_data->cur_addr<stateless_data->addr.count-1);
	    if (stateless_data->app_cb) {
		(*stateless_data->app_cb)(stateless_data, sent, &cont);
	    } else {
		/* Doesn't have application callback.
		 * Terminate the process.
		 */
		cont = PJ_FALSE;
	    }
	}

	/* Finished with this transport. */
	if (stateless_data->cur_transport) {
	    pjsip_transport_dec_ref(stateless_data->cur_transport);
	    stateless_data->cur_transport = NULL;
	}

	/* Done if application doesn't want to continue. */
	if (sent > 0 || !cont) {
	    pjsip_tx_data_dec_ref(tdata);
	    return;
	}

	/* Try next address, if any, and only when this is not the 
	 * first invocation. 
	 */
	if (sent != -PJ_EPENDING) {
	    stateless_data->cur_addr++;
	}

	/* Have next address? */
	if (stateless_data->cur_addr >= stateless_data->addr.count) {
	    /* This only happens when a rather buggy application has
	     * sent 'cont' to PJ_TRUE when the initial value was PJ_FALSE.
	     * In this case just stop the processing; we don't need to
	     * call the callback again as application has been informed
	     * before.
	     */
	    pjsip_tx_data_dec_ref(tdata);
	    return;
	}

	/* Keep current server address information handy. */
	cur_addr = &stateless_data->addr.entry[stateless_data->cur_addr].addr;
	cur_addr_type = stateless_data->addr.entry[stateless_data->cur_addr].type;
	cur_addr_len = stateless_data->addr.entry[stateless_data->cur_addr].addr_len;

	/* Acquire transport. */
	status = pjsip_endpt_acquire_transport( stateless_data->endpt,
						cur_addr_type,
						cur_addr,
						cur_addr_len,
						&stateless_data->cur_transport);
	if (status != PJ_SUCCESS) {
	    sent = -status;
	    continue;
	}

	/* Modify Via header. */
	via = (pjsip_via_hdr*) pjsip_msg_find_hdr( tdata->msg,
						   PJSIP_H_VIA, NULL);
	if (!via) {
	    /* Shouldn't happen if request was created with PJSIP API! 
	     * But we handle the case anyway for robustness.
	     */
	    pj_assert(!"Via header not found!");
	    via = pjsip_via_hdr_create(tdata->pool);
	    pjsip_msg_insert_first_hdr(tdata->msg, (pjsip_hdr*)via);
	}

	if (via->branch_param.slen == 0) {
	    pj_str_t tmp;
	    via->branch_param.ptr = pj_pool_alloc(tdata->pool,
						  PJSIP_MAX_BRANCH_LEN);
	    via->branch_param.slen = PJSIP_MAX_BRANCH_LEN;
	    pj_memcpy(via->branch_param.ptr, PJSIP_RFC3261_BRANCH_ID,
		      PJSIP_RFC3261_BRANCH_LEN);
	    tmp.ptr = via->branch_param.ptr + PJSIP_RFC3261_BRANCH_LEN;
	    pj_generate_unique_string(&tmp);
	}

	via->transport = pj_str(stateless_data->cur_transport->type_name);
	via->sent_by = stateless_data->cur_transport->local_name;
	via->rport_param = 0;

	/* Send message using this transport. */
	status = pjsip_transport_send( stateless_data->cur_transport,
				       tdata,
				       cur_addr,
				       cur_addr_len,
				       stateless_data,
				       &stateless_send_transport_cb);
	if (status == PJ_SUCCESS) {
	    /* Recursively call this function. */
	    sent = tdata->buf.cur - tdata->buf.start;
	    stateless_send_transport_cb( stateless_data, tdata, sent );
	    return;
	} else if (status == PJ_EPENDING) {
	    /* This callback will be called later. */
	    return;
	} else {
	    /* Recursively call this function. */
	    sent = -status;
	    stateless_send_transport_cb( stateless_data, tdata, sent );
	    return;
	}
    }

}

/* Resolver callback for sending stateless request. */
static void 
stateless_send_resolver_callback( pj_status_t status,
				  void *token,
				  const struct pjsip_server_addresses *addr)
{
    pjsip_send_state *stateless_data = token;

    /* Fail on server resolution. */
    if (status != PJ_SUCCESS) {
	if (stateless_data->app_cb) {
	    pj_bool_t cont = PJ_FALSE;
	    (*stateless_data->app_cb)(stateless_data, -status, &cont);
	}
	pjsip_tx_data_dec_ref(stateless_data->tdata);
	return;
    }

    /* Copy server addresses */
    pj_memcpy( &stateless_data->addr, addr, sizeof(pjsip_server_addresses));

    /* Process the addresses. */
    stateless_send_transport_cb( stateless_data, stateless_data->tdata,
				 -PJ_EPENDING);
}

/*
 * Send stateless request.
 * The sending process consists of several stages:
 *  - determine which host to contact (#pjsip_get_request_addr).
 *  - resolve the host (#pjsip_endpt_resolve)
 *  - establish transport (#pjsip_endpt_acquire_transport)
 *  - send the message (#pjsip_transport_send)
 */
PJ_DEF(pj_status_t) 
pjsip_endpt_send_request_stateless(pjsip_endpoint *endpt, 
				   pjsip_tx_data *tdata,
				   void *token,
				   void (*cb)(pjsip_send_state*,
					      pj_ssize_t sent,
					      pj_bool_t *cont))
{
    pjsip_host_info dest_info;
    pjsip_send_state *stateless_data;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && tdata, PJ_EINVAL);

    /* Get destination name to contact. */
    status = pjsip_get_request_addr(tdata, &dest_info);
    if (status != PJ_SUCCESS)
	return status;

    /* Keep stateless data. */
    stateless_data = pj_pool_zalloc(tdata->pool, sizeof(pjsip_send_state));
    stateless_data->token = token;
    stateless_data->endpt = endpt;
    stateless_data->tdata = tdata;
    stateless_data->app_cb = cb;

    /* Resolve destination host.
     * The processing then resumed when the resolving callback is called.
     */
    pjsip_endpt_resolve( endpt, tdata->pool, &dest_info, stateless_data,
			 &stateless_send_resolver_callback);
    return PJ_SUCCESS;
}

/*
 * Determine which address (and transport) to use to send response message
 * based on the received request. This function follows the specification
 * in section 18.2.2 of RFC 3261 and RFC 3581 for calculating the destination
 * address and transport.
 */
PJ_DEF(pj_status_t) pjsip_get_response_addr( pj_pool_t *pool,
					     pjsip_rx_data *rdata,
					     pjsip_response_addr *res_addr )
{
    pjsip_transport *src_transport = rdata->tp_info.transport;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && rdata && res_addr, PJ_EINVAL);

    /* All requests must have "received" parameter.
     * This must always be done in transport layer.
     */
    pj_assert(rdata->msg_info.via->recvd_param.slen != 0);

    /* Do the calculation based on RFC 3261 Section 18.2.2 and RFC 3581 */

    if (PJSIP_TRANSPORT_IS_RELIABLE(src_transport)) {
	/* For reliable protocol such as TCP or SCTP, or TLS over those, the
	 * response MUST be sent using the existing connection to the source
	 * of the original request that created the transaction, if that 
	 * connection is still open. 
	 * If that connection is no longer open, the server SHOULD open a 
	 * connection to the IP address in the received parameter, if present,
	 * using the port in the sent-by value, or the default port for that 
	 * transport, if no port is specified. 
	 * If that connection attempt fails, the server SHOULD use the 
	 * procedures in [4] for servers in order to determine the IP address
	 * and port to open the connection and send the response to.
	 */
	res_addr->transport = rdata->tp_info.transport;
	pj_memcpy(&res_addr->addr, &rdata->pkt_info.src_addr,
		  rdata->pkt_info.src_addr_len);
	res_addr->addr_len = rdata->pkt_info.src_addr_len;
	res_addr->dst_host.type = src_transport->key.type;
	res_addr->dst_host.flag = src_transport->flag;
	pj_strdup( pool, &res_addr->dst_host.addr.host, 
		   &rdata->msg_info.via->recvd_param);
	res_addr->dst_host.addr.port = rdata->msg_info.via->sent_by.port;
	if (res_addr->dst_host.addr.port == 0) {
	    res_addr->dst_host.addr.port = 
		pjsip_transport_get_default_port_for_type(res_addr->dst_host.type);
	}

    } else if (rdata->msg_info.via->maddr_param.slen) {
	/* Otherwise, if the Via header field value contains a maddr parameter,
	 * the response MUST be forwarded to the address listed there, using 
	 * the port indicated in sent-by, or port 5060 if none is present. 
	 * If the address is a multicast address, the response SHOULD be sent 
	 * using the TTL indicated in the ttl parameter, or with a TTL of 1 if
	 * that parameter is not present. 
	 */
	res_addr->transport = NULL;
	res_addr->dst_host.type = src_transport->key.type;
	res_addr->dst_host.flag = src_transport->flag;
	pj_strdup( pool, &res_addr->dst_host.addr.host, 
		   &rdata->msg_info.via->maddr_param);
	res_addr->dst_host.addr.port = rdata->msg_info.via->sent_by.port;
	if (res_addr->dst_host.addr.port == 0)
	    res_addr->dst_host.addr.port = 5060;

    } else if (rdata->msg_info.via->rport_param >= 0) {
	/* There is both a "received" parameter and an "rport" parameter, 
	 * the response MUST be sent to the IP address listed in the "received"
	 * parameter, and the port in the "rport" parameter. 
	 * The response MUST be sent from the same address and port that the 
	 * corresponding request was received on.
	 */
	res_addr->transport = rdata->tp_info.transport;
	pj_memcpy(&res_addr->addr, &rdata->pkt_info.src_addr,
		  rdata->pkt_info.src_addr_len);
	res_addr->addr_len = rdata->pkt_info.src_addr_len;
	res_addr->dst_host.type = src_transport->key.type;
	res_addr->dst_host.flag = src_transport->flag;
	pj_strdup( pool, &res_addr->dst_host.addr.host, 
		   &rdata->msg_info.via->recvd_param);
	res_addr->dst_host.addr.port = rdata->msg_info.via->sent_by.port;
	if (res_addr->dst_host.addr.port == 0) {
	    res_addr->dst_host.addr.port = 
		pjsip_transport_get_default_port_for_type(res_addr->dst_host.type);
	}

    } else {
	res_addr->transport = NULL;
	res_addr->dst_host.type = src_transport->key.type;
	res_addr->dst_host.flag = src_transport->flag;
	pj_strdup( pool, &res_addr->dst_host.addr.host, 
		   &rdata->msg_info.via->recvd_param);
	res_addr->dst_host.addr.port = rdata->msg_info.via->sent_by.port;
	if (res_addr->dst_host.addr.port == 0) {
	    res_addr->dst_host.addr.port = 
		pjsip_transport_get_default_port_for_type(res_addr->dst_host.type);
	}
    }

    return PJ_SUCCESS;
}

/*
 * Callback called by transport during send_response.
 */
static void send_response_transport_cb(void *token, pjsip_tx_data *tdata,
				       pj_ssize_t sent)
{
    pjsip_send_state *send_state = token;
    pj_bool_t cont = PJ_FALSE;

    /* Call callback, if any. */
    if (send_state->app_cb)
	(*send_state->app_cb)(send_state, sent, &cont);

    /* Decrement transport reference counter. */
    pjsip_transport_dec_ref(send_state->cur_transport);

    /* Decrement transmit data ref counter. */
    pjsip_tx_data_dec_ref(tdata);
}

/*
 * Resolver calback during send_response.
 */
static void send_response_resolver_cb( pj_status_t status, void *token,
				       const pjsip_server_addresses *addr )
{
    pjsip_send_state *send_state = token;

    if (status != PJ_SUCCESS) {
	if (send_state->app_cb) {
	    pj_bool_t cont = PJ_FALSE;
	    (*send_state->app_cb)(send_state, -status, &cont);
	}
	pjsip_tx_data_dec_ref(send_state->tdata);
	return;
    }

    /* Only handle the first address resolved. */

    /* Acquire transport. */
    status = pjsip_endpt_acquire_transport( send_state->endpt, 
					    addr->entry[0].type,
					    &addr->entry[0].addr,
					    addr->entry[0].addr_len,
					    &send_state->cur_transport);
    if (status != PJ_SUCCESS) {
	if (send_state->app_cb) {
	    pj_bool_t cont = PJ_FALSE;
	    (*send_state->app_cb)(send_state, -status, &cont);
	}
	pjsip_tx_data_dec_ref(send_state->tdata);
	return;
    }

    /* Update address in send_state. */
    send_state->addr = *addr;

    /* Send response using the transoprt. */
    status = pjsip_transport_send( send_state->cur_transport, 
				   send_state->tdata,
				   &addr->entry[0].addr,
				   addr->entry[0].addr_len,
				   send_state,
				   &send_response_transport_cb);
    if (status == PJ_SUCCESS) {
	pj_ssize_t sent = send_state->tdata->buf.cur - 
			  send_state->tdata->buf.start;
	send_response_transport_cb(send_state, send_state->tdata, sent);

    } else if (status == PJ_EPENDING) {
	/* Transport callback will be called later. */
    } else {
	send_response_transport_cb(send_state, send_state->tdata, -status);
    }
}

/*
 * Send response.
 */
PJ_DEF(pj_status_t) pjsip_endpt_send_response( pjsip_endpoint *endpt,
					       pjsip_response_addr *res_addr,
					       pjsip_tx_data *tdata,
					       void *token,
					       void (*cb)(pjsip_send_state*,
							  pj_ssize_t sent,
							  pj_bool_t *cont))
{
    /* Determine which transports and addresses to send the response,
     * based on Section 18.2.2 of RFC 3261.
     */
    pjsip_send_state *send_state;
    pj_status_t status;

    /* Create structure to keep the sending state. */
    send_state = pj_pool_zalloc(tdata->pool, sizeof(pjsip_send_state));
    send_state->endpt = endpt;
    send_state->tdata = tdata;
    send_state->token = token;
    send_state->app_cb = cb;

    if (res_addr->transport != NULL) {
	send_state->cur_transport = res_addr->transport;
	pjsip_transport_add_ref(send_state->cur_transport);

	status = pjsip_transport_send( send_state->cur_transport, tdata, 
				       &res_addr->addr,
				       res_addr->addr_len,
				       send_state,
				       &send_response_transport_cb );
	if (status == PJ_SUCCESS) {
	    pj_ssize_t sent = tdata->buf.cur - tdata->buf.start;
	    send_response_transport_cb(send_state, tdata, sent);
	    return PJ_SUCCESS;
	} else if (status == PJ_EPENDING) {
	    /* Callback will be called later. */
	    return PJ_SUCCESS;
	} else {
	    pjsip_transport_dec_ref(send_state->cur_transport);
	    return status;
	}
    } else {
	pjsip_endpt_resolve(endpt, tdata->pool, &res_addr->dst_host, 
			    send_state, &send_response_resolver_cb);
	return PJ_SUCCESS;
    }
}

/*
 * Send response
 */
PJ_DEF(pj_status_t) pjsip_endpt_respond_stateless( pjsip_endpoint *endpt,
						   pjsip_rx_data *rdata,
						   int st_code,
						   const pj_str_t *st_text,
						   const pjsip_hdr *hdr_list,
						   const pjsip_msg_body *body)
{
    pj_status_t status;
    pjsip_response_addr res_addr;
    pjsip_tx_data *tdata;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(endpt && rdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* Check that no UAS transaction has been created for this request. 
     * If UAS transaction has been created for this request, application
     * MUST send the response statefully using that transaction.
     */
    PJ_ASSERT_RETURN(pjsip_rdata_get_tsx(rdata)==NULL, PJ_EINVALIDOP);

    /* Create response message */
    status = pjsip_endpt_create_response( endpt, rdata, st_code, st_text, 
					  &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add the message headers, if any */
    if (hdr_list) {
	const pjsip_hdr *hdr = hdr_list->next;
	while (hdr != hdr_list) {
	    pjsip_msg_add_hdr( tdata->msg, pjsip_hdr_clone(tdata->pool, hdr) );
	    hdr = hdr->next;
	}
    }

    /* Add the message body, if any. */
    if (body) {
	tdata->msg->body = pj_pool_alloc(tdata->pool, sizeof(pjsip_msg_body));
	status = pjsip_msg_body_clone( tdata->pool, tdata->msg->body, body );
	if (status != PJ_SUCCESS) {
	    pjsip_tx_data_dec_ref(tdata);
	    return status;
	}
    }

    /* Get where to send request. */
    status = pjsip_get_response_addr( tdata->pool, rdata, &res_addr );
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    /* Send! */
    status = pjsip_endpt_send_response( endpt, &res_addr, tdata, NULL, NULL );

    return status;
}


/*
 * Send response statefully.
 */
PJ_DEF(pj_status_t) pjsip_endpt_respond(  pjsip_endpoint *endpt,
					  pjsip_module *tsx_user,
					  pjsip_rx_data *rdata,
					  int st_code,
					  const pj_str_t *st_text,
					  const pjsip_hdr *hdr_list,
					  const pjsip_msg_body *body,
					  pjsip_transaction **p_tsx )
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;

    /* Validate arguments. */
    PJ_ASSERT_RETURN(endpt && rdata, PJ_EINVAL);

    if (p_tsx) *p_tsx = NULL;

    /* Create response message */
    status = pjsip_endpt_create_response( endpt, rdata, st_code, st_text, 
					  &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add the message headers, if any */
    if (hdr_list) {
	const pjsip_hdr *hdr = hdr_list->next;
	while (hdr != hdr_list) {
	    pjsip_msg_add_hdr( tdata->msg, pjsip_hdr_clone(tdata->pool, hdr) );
	    hdr = hdr->next;
	}
    }

    /* Add the message body, if any. */
    if (body) {
	tdata->msg->body = pj_pool_alloc(tdata->pool, sizeof(pjsip_msg_body));
	status = pjsip_msg_body_clone( tdata->pool, tdata->msg->body, body );
	if (status != PJ_SUCCESS) {
	    pjsip_tx_data_dec_ref(tdata);
	    return status;
	}
    }

    /* Create UAS transaction. */
    status = pjsip_tsx_create_uas(tsx_user, rdata, &tsx);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    /* Feed the request to the transaction. */
    pjsip_tsx_recv_msg(tsx, rdata);

    /* Send the message. */
    status = pjsip_tsx_send_msg(tsx, tdata);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
    } else {
	*p_tsx = tsx;
    }

    return status;
}


/*
 * Get the event string from the event ID.
 */
PJ_DEF(const char *) pjsip_event_str(pjsip_event_id_e e)
{
    return event_str[e];
}


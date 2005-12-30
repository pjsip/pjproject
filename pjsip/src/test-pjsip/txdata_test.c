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

#include "test.h"
#include <pjsip_core.h>
#include <pjlib.h>

#define HFIND(msg,h,H) ((pjsip_##h##_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_##H, NULL))

/*
 * This tests various core message creation functions. 
 */
int txdata_test(void)
{
    pj_status_t status;
    pj_str_t target, from, to, contact, body;
    pjsip_rx_data dummy_rdata;
    pjsip_tx_data *invite, *invite2, *cancel, *response, *ack;

    /* Create INVITE request. */
    target = pj_str("tel:+1");
    from = pj_str("tel:+0");
    to = pj_str("tel:+1");
    contact = pj_str("Bob <sip:+0@example.com;user=phone>");
    body = pj_str("Hello world!");

    status = pjsip_endpt_create_request( endpt, &pjsip_invite_method, &target,
					 &from, &to, &contact, NULL, 10, &body,
					 &invite);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create request", status);
	return -10;
    }

    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(invite) != 0) {
	PJ_LOG(3,("", "   error: buffer must be invalid"));
	return -14;
    }
    /* Reference counter must be set to 1. */
    if (pj_atomic_get(invite->ref_cnt) != 1) {
	PJ_LOG(3,("", "   error: invalid reference counter"));
	return -15;
    }
    /* Check message type. */
    if (invite->msg->type != PJSIP_REQUEST_MSG)
	return -16;
    /* Check method. */
    if (invite->msg->line.req.method.id != PJSIP_INVITE_METHOD)
	return -17;

    /* Check that mandatory headers are present. */
    if (HFIND(invite->msg, from, FROM) == 0)
	return -20;
    if (HFIND(invite->msg, to, TO) == 0)
	return -21;
    if (HFIND(invite->msg, contact, CONTACT) == 0)
	return -22;
    if (HFIND(invite->msg, cid, CALL_ID) == 0)
	return -23;
    if (HFIND(invite->msg, cseq, CSEQ) == 0)
	return -24;
    do {
	pjsip_via_hdr *via = HFIND(invite->msg, via, VIA);
	if (via == NULL)
	    return -25;
	/* Branch param must be empty. */
	if (via->branch_param.slen != 0)
	    return -26;
    } while (0);
    if (invite->msg->body == NULL)
	return -28;

    /* Create another INVITE request from first request. */
    status = pjsip_endpt_create_request_from_hdr( endpt, &pjsip_invite_method,
						  invite->msg->line.req.uri,
						  HFIND(invite->msg,from,FROM),
						  HFIND(invite->msg,to,TO),
						  HFIND(invite->msg,contact,CONTACT),
						  HFIND(invite->msg,cid,CALL_ID),
						  10, &body, &invite2);
    if (status != PJ_SUCCESS) {
	app_perror("   error: create second request failed", status);
	return -30;
    }
    
    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(invite2) != 0) {
	PJ_LOG(3,("", "   error: buffer must be invalid"));
	return -34;
    }
    /* Reference counter must be set to 1. */
    if (pj_atomic_get(invite2->ref_cnt) != 1) {
	PJ_LOG(3,("", "   error: invalid reference counter"));
	return -35;
    }
    /* Check message type. */
    if (invite2->msg->type != PJSIP_REQUEST_MSG)
	return -36;
    /* Check method. */
    if (invite2->msg->line.req.method.id != PJSIP_INVITE_METHOD)
	return -37;

    /* Check that mandatory headers are again present. */
    if (HFIND(invite2->msg, from, FROM) == 0)
	return -40;
    if (HFIND(invite2->msg, to, TO) == 0)
	return -41;
    if (HFIND(invite2->msg, contact, CONTACT) == 0)
	return -42;
    if (HFIND(invite2->msg, cid, CALL_ID) == 0)
	return -43;
    if (HFIND(invite2->msg, cseq, CSEQ) == 0)
	return -44;
    if (HFIND(invite2->msg, via, VIA) == 0)
	return -45;
    /*
    if (HFIND(invite2->msg, ctype, CONTENT_TYPE) == 0)
	return -46;
    if (HFIND(invite2->msg, clen, CONTENT_LENGTH) == 0)
	return -47;
    */
    if (invite2->msg->body == NULL)
	return -48;

    /* Done checking invite2. We can delete this. */
    if (pjsip_tx_data_dec_ref(invite2) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,("", "   error: request buffer not destroyed!"));
	return -49;
    }

    /* Initialize dummy rdata (to simulate receiving a request) 
     * We should never do this in real application, as there are many
     * many more fields need to be initialized!!
     */
    dummy_rdata.msg_info.call_id = (HFIND(invite->msg, cid, CALL_ID))->id;
    dummy_rdata.msg_info.clen = NULL;
    dummy_rdata.msg_info.cseq = HFIND(invite->msg, cseq, CSEQ);
    dummy_rdata.msg_info.ctype = NULL;
    dummy_rdata.msg_info.from = HFIND(invite->msg, from, FROM);
    dummy_rdata.msg_info.max_fwd = NULL;
    dummy_rdata.msg_info.msg = invite->msg;
    dummy_rdata.msg_info.record_route = NULL;
    dummy_rdata.msg_info.require = NULL;
    dummy_rdata.msg_info.route = NULL;
    dummy_rdata.msg_info.to = HFIND(invite->msg, to, TO);
    dummy_rdata.msg_info.via = HFIND(invite->msg, via, VIA);

    /* Create a response message for the request. */
    status = pjsip_endpt_create_response( endpt, &dummy_rdata, 301, NULL, 
					  &response);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create response", status);
	return -50;
    }
    
    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(response) != 0) {
	PJ_LOG(3,("", "   error: buffer must be invalid"));
	return -54;
    }
    /* Check reference counter. */
    if (pj_atomic_get(response->ref_cnt) != 1) {
	PJ_LOG(3,("", "   error: invalid ref count in response"));
	return -55;
    }
    /* Check message type. */
    if (response->msg->type != PJSIP_RESPONSE_MSG)
	return -56;
    /* Check correct status is set. */
    if (response->msg->line.status.code != 301)
	return -57;

    /* Check that mandatory headers are again present. */
    if (HFIND(response->msg, from, FROM) == 0)
	return -60;
    if (HFIND(response->msg, to, TO) == 0)
	return -61;
    /*
    if (HFIND(response->msg, contact, CONTACT) == 0)
	return -62;
     */
    if (HFIND(response->msg, cid, CALL_ID) == 0)
	return -63;
    if (HFIND(response->msg, cseq, CSEQ) == 0)
	return -64;
    if (HFIND(response->msg, via, VIA) == 0)
	return -65;

    /* This response message will be used later when creating ACK */

    /* Create CANCEL request for the original request. */
    status = pjsip_endpt_create_cancel( endpt, invite, &cancel);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create CANCEL request", status);
	return -80;
    }

    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(cancel) != 0) {
	PJ_LOG(3,("", "   error: buffer must be invalid"));
	return -84;
    }
    /* Check reference counter. */
    if (pj_atomic_get(cancel->ref_cnt) != 1) {
	PJ_LOG(3,("", "   error: invalid ref count in CANCEL request"));
	return -85;
    }
    /* Check message type. */
    if (cancel->msg->type != PJSIP_REQUEST_MSG)
	return -86;
    /* Check method. */
    if (cancel->msg->line.req.method.id != PJSIP_CANCEL_METHOD)
	return -87;

    /* Check that mandatory headers are again present. */
    if (HFIND(cancel->msg, from, FROM) == 0)
	return -90;
    if (HFIND(cancel->msg, to, TO) == 0)
	return -91;
    /*
    if (HFIND(cancel->msg, contact, CONTACT) == 0)
	return -92;
    */
    if (HFIND(cancel->msg, cid, CALL_ID) == 0)
	return -93;
    if (HFIND(cancel->msg, cseq, CSEQ) == 0)
	return -94;
    if (HFIND(cancel->msg, via, VIA) == 0)
	return -95;

    /* Done checking CANCEL request. */
    if (pjsip_tx_data_dec_ref(cancel) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,("", "   error: response buffer not destroyed!"));
	return -99;
    }

    /* Modify dummy_rdata to simulate receiving response. */
    pj_memset(&dummy_rdata, 0, sizeof(dummy_rdata));
    dummy_rdata.msg_info.msg = response->msg;
    dummy_rdata.msg_info.to = HFIND(response->msg, to, TO);

    /* Create ACK request */
    status = pjsip_endpt_create_ack( endpt, invite, &dummy_rdata, &ack );
    if (status != PJ_SUCCESS) {
	PJ_LOG(3,("", "   error: unable to create ACK"));
	return -100;
    }
    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(ack) != 0) {
	PJ_LOG(3,("", "   error: buffer must be invalid"));
	return -104;
    }
    /* Check reference counter. */
    if (pj_atomic_get(ack->ref_cnt) != 1) {
	PJ_LOG(3,("", "   error: invalid ref count in ACK request"));
	return -105;
    }
    /* Check message type. */
    if (ack->msg->type != PJSIP_REQUEST_MSG)
	return -106;
    /* Check method. */
    if (ack->msg->line.req.method.id != PJSIP_ACK_METHOD)
	return -107;
    /* Check Request-URI is present. */
    if (ack->msg->line.req.uri == NULL)
	return -108;

    /* Check that mandatory headers are again present. */
    if (HFIND(ack->msg, from, FROM) == 0)
	return -110;
    if (HFIND(ack->msg, to, TO) == 0)
	return -111;
    if (HFIND(ack->msg, cid, CALL_ID) == 0)
	return -112;
    if (HFIND(ack->msg, cseq, CSEQ) == 0)
	return -113;
    if (HFIND(ack->msg, via, VIA) == 0)
	return -114;
    if (ack->msg->body != NULL)
	return -115;

    /* Done checking invite message. */
    if (pjsip_tx_data_dec_ref(invite) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,("", "   error: response buffer not destroyed!"));
	return -120;
    }

    /* Done checking response message. */
    if (pjsip_tx_data_dec_ref(response) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,("", "   error: response buffer not destroyed!"));
	return -130;
    }

    /* Done checking ack message. */
    if (pjsip_tx_data_dec_ref(ack) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,("", "   error: response buffer not destroyed!"));
	return -140;
    }

    /* Done. */
    return 0;
}


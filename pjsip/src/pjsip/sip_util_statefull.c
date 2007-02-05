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
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

struct tsx_data
{
    pj_time_val	    delay;
    pj_timer_entry  timeout_timer;

    void *token;
    void (*cb)(void*, pjsip_event*);
};

static void mod_util_on_tsx_state(pjsip_transaction*, pjsip_event*);

/* This module will be registered in pjsip_endpt.c */

pjsip_module mod_stateful_util = 
{
    NULL, NULL,			    /* prev, next.			*/
    { "mod-stateful-util", 17 },    /* Name.				*/
    -1,				    /* Id				*/
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority				*/
    NULL,			    /* load()				*/
    NULL,			    /* start()				*/
    NULL,			    /* stop()				*/
    NULL,			    /* unload()				*/
    NULL,			    /* on_rx_request()			*/
    NULL,			    /* on_rx_response()			*/
    NULL,			    /* on_tx_request.			*/
    NULL,			    /* on_tx_response()			*/
    &mod_util_on_tsx_state,	    /* on_tsx_state()			*/
};

static void mod_util_on_tsx_state(pjsip_transaction *tsx, pjsip_event *event)
{
    struct tsx_data *tsx_data;

    if (event->type != PJSIP_EVENT_TSX_STATE)
	return;

    tsx_data = tsx->mod_data[mod_stateful_util.id];
    if (tsx_data == NULL)
	return;

    if (tsx->status_code < 200)
	return;

    /* Cancel timer if any */
    if (tsx_data->timeout_timer.id != 0) {
	tsx_data->timeout_timer.id = 0;
	pjsip_endpt_cancel_timer(tsx->endpt, &tsx_data->timeout_timer);
    }

    /* Call the callback, if any, and prevent the callback to be called again
     * by clearing the transaction's module_data.
     */
    tsx->mod_data[mod_stateful_util.id] = NULL;

    if (tsx_data->cb) {
	(*tsx_data->cb)(tsx_data->token, event);
    }
}


static void mod_util_on_timeout(pj_timer_heap_t *th, pj_timer_entry *te)
{
    pjsip_transaction *tsx = (pjsip_transaction*) te->user_data;
    struct tsx_data *tsx_data;

    PJ_UNUSED_ARG(th);

    tsx_data = tsx->mod_data[mod_stateful_util.id];
    if (tsx_data == NULL) {
	pj_assert(!"Shouldn't happen");
	return;
    }

    tsx_data->timeout_timer.id = 0;

    PJ_LOG(4,(tsx->obj_name, "Transaction timed out by user timer (%d.%d sec)",
	      (int)tsx_data->delay.sec, (int)tsx_data->delay.msec));

    /* Terminate the transaction. This will call mod_util_on_tsx_state() */
    pjsip_tsx_terminate(tsx, PJSIP_SC_TSX_TIMEOUT);
}


PJ_DEF(pj_status_t) pjsip_endpt_send_request(  pjsip_endpoint *endpt,
					       pjsip_tx_data *tdata,
					       pj_int32_t timeout,
					       void *token,
					       void (*cb)(void*,pjsip_event*))
{
    pjsip_transaction *tsx;
    struct tsx_data *tsx_data;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && tdata && (timeout==-1 || timeout>0), PJ_EINVAL);

    /* Check that transaction layer module is registered to endpoint */
    PJ_ASSERT_RETURN(mod_stateful_util.id != -1, PJ_EINVALIDOP);


    status = pjsip_tsx_create_uac(&mod_stateful_util, tdata, &tsx);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    tsx_data = pj_pool_zalloc(tsx->pool, sizeof(struct tsx_data));
    tsx_data->token = token;
    tsx_data->cb = cb;

    if (timeout >= 0) {
	tsx_data->delay.sec = 0;
	tsx_data->delay.msec = timeout;
	pj_time_val_normalize(&tsx_data->delay);

	tsx_data->timeout_timer.id = PJ_TRUE;
	tsx_data->timeout_timer.user_data = tsx;
	tsx_data->timeout_timer.cb = &mod_util_on_timeout;
	
	status = pjsip_endpt_schedule_timer(endpt, &tsx_data->timeout_timer, 
					    &tsx_data->delay);
	if (status != PJ_SUCCESS) {
	    pjsip_tsx_terminate(tsx, PJSIP_SC_INTERNAL_SERVER_ERROR);
	    pjsip_tx_data_dec_ref(tdata);
	    return status;
	}
    }

    tsx->mod_data[mod_stateful_util.id] = tsx_data;

    status = pjsip_tsx_send_msg(tsx, NULL);
    if (status != PJ_SUCCESS) {
	if (tsx_data->timeout_timer.id != 0) {
	    pjsip_endpt_cancel_timer(endpt, &tsx_data->timeout_timer);
	    tsx_data->timeout_timer.id = PJ_FALSE;
	}
	pjsip_tx_data_dec_ref(tdata);
    }

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
	tdata->msg->body = pjsip_msg_body_clone( tdata->pool, body );
	if (tdata->msg->body == NULL) {
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
    } else if (p_tsx) {
	*p_tsx = tsx;
    }

    return status;
}



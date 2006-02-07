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
#include <pj/pool.h>
#include <pj/assert.h>

struct tsx_data
{
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
    NULL,			    /* User data.			*/
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

    /* Call the callback, if any, and prevent the callback to be called again
     * by clearing the transaction's module_data.
     */
    tsx->mod_data[mod_stateful_util.id] = NULL;

    if (tsx_data->cb) {
	(*tsx_data->cb)(tsx_data->token, event);
    }
}


PJ_DEF(pj_status_t) pjsip_endpt_send_request(  pjsip_endpoint *endpt,
					       pjsip_tx_data *tdata,
					       int timeout,
					       void *token,
					       void (*cb)(void*,pjsip_event*))
{
    pjsip_transaction *tsx;
    struct tsx_data *tsx_data;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && tdata && (timeout==-1 || timeout>0), PJ_EINVAL);

    status = pjsip_tsx_create_uac(&mod_stateful_util, tdata, &tsx);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    tsx_data = pj_pool_alloc(tsx->pool, sizeof(struct tsx_data));
    tsx_data->token = token;
    tsx_data->cb = cb;
    tsx->mod_data[mod_stateful_util.id] = tsx_data;

    PJ_TODO(IMPLEMENT_TIMEOUT_FOR_SEND_REQUEST);

    return pjsip_tsx_send_msg(tsx, NULL);
}


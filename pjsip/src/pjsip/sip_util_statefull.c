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
#include <pj/pool.h>

struct aux_tsx_data
{
    void *token;
    void (*cb)(void*,pjsip_event*);
};

static void aux_tsx_handler( pjsip_transaction *tsx, pjsip_event *event );

pjsip_module aux_tsx_module = 
{
    NULL, NULL,				/* prev and next	*/
    { "Aux-Tsx", 7},			/* Name.		*/
    -1,					/* Id		*/
    PJSIP_MOD_PRIORITY_APPLICATION-1,   /* Priority		*/
    NULL,				/* User data.	*/
    0,					/* Number of methods supported (=0). */
    { 0 },				/* Array of methods (none) */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    NULL,				/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    &aux_tsx_handler,			/* tsx_handler()	*/
};

static void aux_tsx_handler( pjsip_transaction *tsx, pjsip_event *event )
{
    struct aux_tsx_data *tsx_data;

    if (event->type != PJSIP_EVENT_TSX_STATE)
	return;
    if (tsx->module_data[aux_tsx_module.id] == NULL)
	return;
    if (tsx->status_code < 200)
	return;

    /* Call the callback, if any, and prevent the callback to be called again
     * by clearing the transaction's module_data.
     */
    tsx_data = tsx->module_data[aux_tsx_module.id];
    tsx->module_data[aux_tsx_module.id] = NULL;

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
    tsx->module_data[aux_tsx_module.id] = tsx_data;

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


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
#include "pjsip_perf.h"


/*
 * This file handles OPTIONS generator and incoming OPTIONS requests.
 */
#define THIS_FILE   "handler_options.c"


/****************************************************************************
 *
 * INCOMING OPTIONS HANDLER
 *
 ****************************************************************************
 */


static pj_bool_t mod_options_on_rx_request(pjsip_rx_data *rdata);


/* The module instance. */
static pjsip_module mod_perf_options = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-perf-options", 16 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &mod_options_on_rx_request,		/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};

static pj_bool_t mod_options_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;

    if (msg->line.req.method.id == PJSIP_OPTIONS_METHOD) {

	if (settings.stateless) {
	    pjsip_endpt_respond_stateless( settings.endpt, rdata, 200, NULL, 
					   NULL, NULL);
	} else {

	    pjsip_endpt_respond( settings.endpt, NULL, rdata, 200, NULL,
				 NULL, NULL, NULL);
	}

	return PJ_TRUE;
    }

    return PJ_FALSE;
}


/****************************************************************************
 *
 * OUTGOING OPTIONS GENERATOR.
 *
 ****************************************************************************
 */

struct callback_data
{
    void     *test_data;
    void    (*completion_cb)(void*,pj_bool_t);
};

static void options_callback(void *token, const pjsip_event *e)
{
    struct callback_data *cb_data = token;

    if (e->type == PJSIP_EVENT_TSX_STATE) {
	(*cb_data->completion_cb)(cb_data->test_data,
				  e->body.tsx_state.tsx->status_code/100==2);
    }
}

pj_status_t options_spawn_test(const pj_str_t *target,
			       const pj_str_t *from,
			       const pj_str_t *to,
			       unsigned cred_cnt,
			       const pjsip_cred_info cred[],
			       const pjsip_route_hdr *route_set,
			       void *test_data,
			       void (*completion_cb)(void*,pj_bool_t))
{
    pj_status_t status;
    struct callback_data *cb_data;
    pjsip_tx_data *tdata;

    PJ_LOG(5,(THIS_FILE,"Sending OPTIONS request.."));

    PJ_UNUSED_ARG(route_set);
    PJ_UNUSED_ARG(cred_cnt);
    PJ_UNUSED_ARG(cred);

    status = pjsip_endpt_create_request( settings.endpt, 
					 &pjsip_options_method,
					 target,
					 from,
					 to,
					 NULL, NULL, -1, NULL,
					 &tdata);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create request", status);
	return status;
    }

    cb_data = pj_pool_alloc(tdata->pool, sizeof(struct callback_data));
    cb_data->test_data = test_data;
    cb_data->completion_cb = completion_cb;

    status = pjsip_endpt_send_request( settings.endpt, tdata, -1,
				       cb_data, &options_callback);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to send request", status);
	return status;
    }

    return PJ_SUCCESS;
}


pj_status_t options_handler_init(void)
{
    return pjsip_endpt_register_module(settings.endpt, &mod_perf_options);
}



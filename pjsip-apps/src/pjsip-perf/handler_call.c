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
 * This file handles call generation and incoming calls.
 */
#define THIS_FILE   "handler_call.c"

/*
 * Dummy SDP.
 */
static pjmedia_sdp_session *local_sdp;


#define TIMER_ID    1234

/* Call data, to be attached to invite session. */
struct call_data
{
    pjsip_inv_session	*inv;
    pj_bool_t		 confirmed;
    pj_timer_entry	 bye_timer;
    void		*test_data;
    void	       (*completion_cb)(void*,pj_bool_t);
};


/****************************************************************************
 *
 * INCOMING CALL HANDLER
 *
 ****************************************************************************
 */


static pj_bool_t mod_call_on_rx_request(pjsip_rx_data *rdata);

/* The module instance. */
static pjsip_module mod_call = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-perf-call", 13 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &mod_call_on_rx_request,		/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/
};



/*
 * Handle incoming requests.
 * Because this module is registered to the INVITE module too, this
 * callback may be called for requests inside a dialog.
 */
static pj_bool_t mod_call_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_dialog *dlg;
    pjsip_inv_session *inv;
    pjsip_tx_data *response;
    struct call_data *call_data;
    unsigned options;
    pj_status_t status;


    /* Don't want to handle anything but INVITE */
    if (msg->line.req.method.id != PJSIP_INVITE_METHOD)
	return PJ_FALSE;

    /* Don't want to handle request that's already associated with
     * existing dialog or transaction.
     */
    if (pjsip_rdata_get_dlg(rdata) || pjsip_rdata_get_tsx(rdata))
	return PJ_FALSE;


    /* Verify that we can handle the request. */
    options = 0;
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
				      settings.endpt, &response);
    if (status != PJ_SUCCESS) {

	/*
	 * No we can't handle the incoming INVITE request.
	 */

	if (response) {
	    pjsip_response_addr res_addr;

	    pjsip_get_response_addr(response->pool, rdata, &res_addr);
	    pjsip_endpt_send_response(settings.endpt, &res_addr, response, 
				      NULL, NULL);

	} else {

	    /* Respond with 500 (Internal Server Error) */
	    pjsip_endpt_respond_stateless(settings.endpt, rdata, 500, NULL,
					  NULL, NULL);
	}

	return PJ_TRUE;
    } 

    /*
     * Yes we can handle the incoming INVITE request.
     */

    /* Create dialog. */
    status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, NULL, &dlg);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_respond(dlg, rdata, 500, NULL, NULL, NULL);
	return PJ_TRUE;
    }

    /* Create invite session: */
    status = pjsip_inv_create_uas( dlg, rdata, local_sdp, 0, &inv);
    if (status != PJ_SUCCESS) {

	pjsip_dlg_respond(dlg, rdata, 500, NULL, NULL, NULL);

	// TODO: Need to delete dialog
	return PJ_TRUE;
    }

    /* Create and associate call data. */
    call_data = pj_pool_zalloc(inv->pool, sizeof(struct call_data));
    call_data->inv = inv;
    call_data->bye_timer.user_data = call_data;
    inv->mod_data[mod_call.id] = call_data;

    /* Answer with 200 straight away. */
    status = pjsip_inv_initial_answer(inv, rdata,  200, 
				      NULL, NULL, &response);
    if (status != PJ_SUCCESS) {
	
	app_perror(THIS_FILE, "Unable to create 200 response", status);

	pjsip_dlg_respond(dlg, rdata, 500, NULL, NULL, NULL);

	// TODO: Need to delete dialog

    } else {
	status = pjsip_inv_send_msg(inv, response);
	if (status != PJ_SUCCESS)
	    app_perror(THIS_FILE, "Unable to send 100 response", status);
    }


    return PJ_TRUE;
}


/****************************************************************************
 *
 * OUTGOING CALL GENERATOR
 *
 ****************************************************************************
 */

/**
 * Make outgoing call.
 */
pj_status_t call_spawn_test( const pj_str_t *target,
			     const pj_str_t *from,
			     const pj_str_t *to,
			     unsigned cred_cnt,
			     const pjsip_cred_info cred[],
			     const pjsip_route_hdr *route_set,
			     void *test_data,
			     void (*completion_cb)(void*,pj_bool_t))
{
    pjsip_dialog *dlg;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata;
    struct call_data *call_data;
    pj_status_t status;

    /* Create outgoing dialog: */
    status = pjsip_dlg_create_uac( pjsip_ua_instance(), 
				   from, NULL, 
				   to, target,
				   &dlg);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Dialog creation failed", status);
	return status;
    }

    /* Create the INVITE session: */
    status = pjsip_inv_create_uac( dlg, local_sdp, 0, &inv);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Invite session creation failed", status);
	goto on_error;
    }


    /* Set dialog Route-Set: */
    if (route_set)
	pjsip_dlg_set_route_set(dlg, route_set);


    /* Set credentials: */
    pjsip_auth_clt_set_credentials( &dlg->auth_sess, cred_cnt,  cred);


    /* Create initial INVITE: */
    status = pjsip_inv_invite(inv, &tdata);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create initial INVITE request", 
		   status);
	goto on_error;
    }


    /* Create and associate our call data */
    call_data = pj_pool_zalloc(inv->pool, sizeof(struct call_data));
    call_data->inv = inv;
    call_data->test_data = test_data;
    call_data->bye_timer.user_data = call_data;
    call_data->completion_cb = completion_cb;

    inv->mod_data[mod_call.id] = call_data;


    /* Send initial INVITE: */
    status = pjsip_inv_send_msg(inv, tdata);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to send initial INVITE request", 
		    status);
	goto on_error;
    }


    return PJ_SUCCESS;


on_error:
    PJ_TODO(DESTROY_DIALOG_ON_FAIL);
    return status;
}


/* Timer callback to send BYE. */
static void bye_callback( pj_timer_heap_t *ht, pj_timer_entry *e)
{
    struct call_data *call_data = e->user_data;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_UNUSED_ARG(ht);
    PJ_UNUSED_ARG(e);

    e->id = 0;

    status = pjsip_inv_end_session(call_data->inv, PJSIP_SC_REQUEST_TIMEOUT, 
				   NULL, &tdata);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create BYE", status);
	return;
    }

    status = pjsip_inv_send_msg(call_data->inv, tdata);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to send BYE", status);
	return;
    }

}

/*
 * This callback receives notification from invite session when the
 * session state has changed.
 */
static void call_on_state_changed( pjsip_inv_session *inv, pjsip_event *e)
{
    struct call_data *call_data;

    PJ_UNUSED_ARG(e);

    call_data = inv->mod_data[mod_call.id];
    if (call_data == NULL)
	return;

    /* Once call has been confirmed, schedule timer to terminate the call. */
    if (inv->state == PJSIP_INV_STATE_CONFIRMED) {

	pj_time_val interval;

	call_data->confirmed = PJ_TRUE;

	/* For UAC, schedule time to send BYE.
	 * For UAS, schedule time to disconnect INVITE, just in case BYE
	 * is not received.
	 */
	if (inv->role == PJSIP_ROLE_UAC)
	    interval.sec = settings.duration, interval.msec = 0;
	else
	    interval.sec = settings.duration+5, interval.msec = 0;

	call_data->bye_timer.id = TIMER_ID;
	call_data->bye_timer.cb = &bye_callback;
	pjsip_endpt_schedule_timer(settings.endpt, &call_data->bye_timer,
				   &interval);

    }
    /* If call has been terminated, cancel our timer, if any.
     * And call tester's callback.
     */
    else if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

	/* Cancel timer, if any. */
	if (call_data->bye_timer.id == TIMER_ID) {
	    call_data->bye_timer.id = 0;
	    pjsip_endpt_cancel_timer(settings.endpt, &call_data->bye_timer);
	}

	/* Detach call data from the invite session. */
	inv->mod_data[mod_call.id] = NULL;

	/* Call tester callback. */
	if (call_data->completion_cb) {
	    (*call_data->completion_cb)(call_data->test_data,
					call_data->confirmed);
	}
    }
}


/*
 * This callback is called by invite session framework when UAC session
 * has forked.
 */
static void call_on_forked( pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);

    PJ_TODO(HANDLE_FORKED_DIALOG);
}



/****************************************************************************
 *
 * INITIALIZATION
 *
 ****************************************************************************
 */

pj_status_t call_handler_init(void)
{
    pjsip_inv_callback inv_cb;
    pjmedia_sock_info skinfo;
    pj_status_t status;

    /* Register incoming call handler. */
    status = pjsip_endpt_register_module(settings.endpt, &mod_call);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to register call handler", 
		    status);
	return status;
    }

    /* Invite session callback: */
    pj_memset(&inv_cb, 0, sizeof(inv_cb));
    inv_cb.on_state_changed = &call_on_state_changed;
    inv_cb.on_new_session = &call_on_forked;

    /* Initialize invite session module: */
    status = pjsip_inv_usage_init(settings.endpt, &inv_cb);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to initialize INVITE session module", 
		    status);
	return status;
    }

    /* Create dummy SDP. */
    pj_memset(&skinfo, 0, sizeof(skinfo));
    pj_sockaddr_in_init(&skinfo.rtp_addr_name, pj_gethostname(), 4000);
    pj_sockaddr_in_init(&skinfo.rtcp_addr_name, pj_gethostname(), 4001);

    status = pjmedia_endpt_create_sdp( settings.med_endpt, settings.pool,
				       1, &skinfo, &local_sdp);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to generate local SDP", 
		    status);
	return status;
    }

    return PJ_SUCCESS;
}



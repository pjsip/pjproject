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
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>


#define THIS_FILE		"pjsua_call.c"


/* This callback receives notification from invite session when the
 * session state has changed.
 */
static void pjsua_call_on_state_changed(pjsip_inv_session *inv, 
					pjsip_event *e);

/* This callback is called by invite session framework when UAC session
 * has forked.
 */
static void pjsua_call_on_forked( pjsip_inv_session *inv, 
				  pjsip_event *e);

/*
 * Callback to be called when SDP offer/answer negotiation has just completed
 * in the session. This function will start/update media if negotiation
 * has succeeded.
 */
static void pjsua_call_on_media_update(pjsip_inv_session *inv,
				       pj_status_t status);

/*
 * Called when session received new offer.
 */
static void pjsua_call_on_rx_offer(pjsip_inv_session *inv,
				   const pjmedia_sdp_session *offer);

/*
 * This callback is called when transaction state has changed in INVITE
 * session. We use this to trap:
 *  - incoming REFER request.
 *  - incoming MESSAGE request.
 */
static void pjsua_call_on_tsx_state_changed(pjsip_inv_session *inv,
					    pjsip_transaction *tsx,
					    pjsip_event *e);


/* Destroy the call's media */
static pj_status_t call_destroy_media(int call_id);

/* Create inactive SDP for call hold. */
static pj_status_t create_inactive_sdp(pjsua_call *call,
				       pjmedia_sdp_session **p_answer);


/*
 * Reset call descriptor.
 */
static void reset_call(pjsua_call_id id)
{
    pjsua_call *call = &pjsua_var.calls[id];

    call->index = id;
    call->inv = NULL;
    call->user_data = NULL;
    call->session = NULL;
    call->xfer_sub = NULL;
    call->last_code = 0;
    call->conf_slot = PJSUA_INVALID_ID;
    call->last_text.ptr = call->last_text_buf_;
    call->last_text.slen = 0;
}


/*
 * Init call subsystem.
 */
pj_status_t pjsua_call_subsys_init(const pjsua_config *cfg)
{
    pjsip_inv_callback inv_cb;
    unsigned i;
    const pj_str_t str_norefersub = { "norefersub", 10 };
    pj_status_t status;

    /* Init calls array. */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.calls); ++i)
	reset_call(i);

    /* Copy config */
    pjsua_config_dup(pjsua_var.pool, &pjsua_var.ua_cfg, cfg);

    /* Initialize invite session callback. */
    pj_bzero(&inv_cb, sizeof(inv_cb));
    inv_cb.on_state_changed = &pjsua_call_on_state_changed;
    inv_cb.on_new_session = &pjsua_call_on_forked;
    inv_cb.on_media_update = &pjsua_call_on_media_update;
    inv_cb.on_rx_offer = &pjsua_call_on_rx_offer;
    inv_cb.on_tsx_state_changed = &pjsua_call_on_tsx_state_changed;


    /* Initialize invite session module: */
    status = pjsip_inv_usage_init(pjsua_var.endpt, &inv_cb);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Add "norefersub" in Supported header */
    pjsip_endpt_add_capability(pjsua_var.endpt, NULL, PJSIP_H_SUPPORTED,
			       NULL, 1, &str_norefersub);

    return status;
}


/*
 * Start call subsystem.
 */
pj_status_t pjsua_call_subsys_start(void)
{
    /* Nothing to do */
    return PJ_SUCCESS;
}


/*
 * Get maximum number of calls configured in pjsua.
 */
PJ_DEF(unsigned) pjsua_call_get_max_count(void)
{
    return pjsua_var.ua_cfg.max_calls;
}


/*
 * Get number of currently active calls.
 */
PJ_DEF(unsigned) pjsua_call_get_count(void)
{
    return pjsua_var.call_cnt;
}


/*
 * Enum calls.
 */
PJ_DEF(pj_status_t) pjsua_enum_calls( pjsua_call_id ids[],
				      unsigned *count)
{
    unsigned i, c;

    PJ_ASSERT_RETURN(ids && *count, PJ_EINVAL);

    PJSUA_LOCK();

    for (i=0, c=0; c<*count && i<pjsua_var.ua_cfg.max_calls; ++i) {
	if (!pjsua_var.calls[i].inv)
	    continue;
	ids[c] = i;
	++c;
    }

    *count = c;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Make outgoing call to the specified URI using the specified account.
 */
PJ_DEF(pj_status_t) pjsua_call_make_call( pjsua_acc_id acc_id,
					  const pj_str_t *dest_uri,
					  unsigned options,
					  void *user_data,
					  const pjsua_msg_data *msg_data,
					  pjsua_call_id *p_call_id)
{
    pjsip_dialog *dlg = NULL;
    pjmedia_sdp_session *offer;
    pjsip_inv_session *inv = NULL;
    pjsua_acc *acc;
    pjsua_call *call;
    unsigned call_id;
    pj_str_t contact;
    pjsip_tx_data *tdata;
    pj_status_t status;


    /* Check that account is valid */
    PJ_ASSERT_RETURN(acc_id>=0 || acc_id<PJ_ARRAY_SIZE(pjsua_var.acc), 
		     PJ_EINVAL);

    /* Options must be zero for now */
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    PJSUA_LOCK();

    acc = &pjsua_var.acc[acc_id];
    if (!acc->valid) {
	pjsua_perror(THIS_FILE, "Unable to make call because account "
		     "is not valid", PJ_EINVALIDOP);
	PJSUA_UNLOCK();
	return PJ_EINVALIDOP;
    }

    /* Find free call slot. */
    for (call_id=0; call_id<pjsua_var.ua_cfg.max_calls; ++call_id) {
	if (pjsua_var.calls[call_id].inv == NULL)
	    break;
    }

    if (call_id == pjsua_var.ua_cfg.max_calls) {
	pjsua_perror(THIS_FILE, "Error making file", PJ_ETOOMANY);
	PJSUA_UNLOCK();
	return PJ_ETOOMANY;
    }

    call = &pjsua_var.calls[call_id];

    /* Mark call start time. */
    pj_gettimeofday(&call->start_time);

    /* Reset first response time */
    call->res_time.sec = 0;

    /* Create suitable Contact header */
    status = pjsua_acc_create_uac_contact(pjsua_var.pool, &contact,
					  acc_id, dest_uri);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to generate Contact header", status);
	PJSUA_UNLOCK();
	return status;
    }

    /* Create outgoing dialog: */
    status = pjsip_dlg_create_uac( pjsip_ua_instance(), 
				   &acc->cfg.id, &contact,
				   dest_uri, dest_uri, &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Dialog creation failed", status);
	PJSUA_UNLOCK();
	return status;
    }

    /* Get media capability from media endpoint: */

    status = pjmedia_endpt_create_sdp( pjsua_var.med_endpt, dlg->pool, 1, 
				       &call->skinfo, &offer);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "pjmedia unable to create SDP", status);
	goto on_error;
    }

    /* Create the INVITE session: */

    status = pjsip_inv_create_uac( dlg, offer, 0, &inv);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Invite session creation failed", status);
	goto on_error;
    }


    /* Create and associate our data in the session. */

    call->inv = inv;

    dlg->mod_data[pjsua_var.mod.id] = call;
    inv->mod_data[pjsua_var.mod.id] = call;

    /* Attach user data */
    call->user_data = user_data;

    /* Set dialog Route-Set: */
    if (!pj_list_empty(&acc->route_set))
	pjsip_dlg_set_route_set(dlg, &acc->route_set);


    /* Set credentials: */
    if (acc->cred_cnt) {
	pjsip_auth_clt_set_credentials( &dlg->auth_sess, 
					acc->cred_cnt, acc->cred);
    }


    /* Create initial INVITE: */

    status = pjsip_inv_invite(inv, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create initial INVITE request", 
		     status);
	goto on_error;
    }


    /* Add additional headers etc */

    pjsua_process_msg_data( tdata, msg_data);

    /* Send initial INVITE: */

    status = pjsip_inv_send_msg(inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send initial INVITE request", 
		     status);

	/* Upon failure to send first request, both dialog and invite 
	 * session would have been cleared.
	 */
	inv = NULL;
	dlg = NULL;
	goto on_error;
    }

    /* Done. */

    ++pjsua_var.call_cnt;

    if (p_call_id)
	*p_call_id = call_id;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;


on_error:
    if (inv != NULL) {
	pjsip_inv_terminate(inv, PJSIP_SC_OK, PJ_FALSE);
    } else if (dlg) {
	pjsip_dlg_terminate(dlg);
    }

    if (call_id != -1) {
	reset_call(call_id);
    }

    PJSUA_UNLOCK();
    return status;
}


/**
 * Handle incoming INVITE request.
 * Called by pjsua_core.c
 */
pj_bool_t pjsua_call_on_incoming(pjsip_rx_data *rdata)
{
    pj_str_t contact;
    pjsip_dialog *dlg = pjsip_rdata_get_dlg(rdata);
    pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_tx_data *response = NULL;
    unsigned options = 0;
    pjsip_inv_session *inv = NULL;
    int acc_id;
    pjsua_call *call;
    int call_id = -1;
    pjmedia_sdp_session *answer;
    pj_status_t status;

    /* Don't want to handle anything but INVITE */
    if (msg->line.req.method.id != PJSIP_INVITE_METHOD)
	return PJ_FALSE;

    /* Don't want to handle anything that's already associated with
     * existing dialog or transaction.
     */
    if (dlg || tsx)
	return PJ_FALSE;


    /* Verify that we can handle the request. */
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
				      pjsua_var.endpt, &response);
    if (status != PJ_SUCCESS) {

	/*
	 * No we can't handle the incoming INVITE request.
	 */

	if (response) {
	    pjsip_response_addr res_addr;

	    pjsip_get_response_addr(response->pool, rdata, &res_addr);
	    pjsip_endpt_send_response(pjsua_var.endpt, &res_addr, response, 
				      NULL, NULL);

	} else {

	    /* Respond with 500 (Internal Server Error) */
	    pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 500, NULL,
					  NULL, NULL);
	}

	return PJ_TRUE;
    } 


    /*
     * Yes we can handle the incoming INVITE request.
     */

    /* Find free call slot. */
    for (call_id=0; call_id<(int)pjsua_var.ua_cfg.max_calls; ++call_id) {
	if (pjsua_var.calls[call_id].inv == NULL)
	    break;
    }

    if (call_id == (int)pjsua_var.ua_cfg.max_calls) {
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 
				      PJSIP_SC_BUSY_HERE, NULL,
				      NULL, NULL);
	PJ_LOG(2,(THIS_FILE, 
		  "Unable to accept incoming call (too many calls)"));
	return PJ_TRUE;
    }

    /* Clear call descriptor */
    reset_call(call_id);

    call = &pjsua_var.calls[call_id];

    /* Mark call start time. */
    pj_gettimeofday(&call->start_time);

    /* Get media capability from media endpoint: */
    status = pjmedia_endpt_create_sdp( pjsua_var.med_endpt, 
				       rdata->tp_info.pool, 1,
				       &call->skinfo, &answer );
    if (status != PJ_SUCCESS) {
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 500, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    /* 
     * Get which account is most likely to be associated with this incoming
     * call. We need the account to find which contact URI to put for
     * the call.
     */
    acc_id = pjsua_acc_find_for_incoming(rdata);

    /* Get suitable Contact header */
    status = pjsua_acc_create_uas_contact(rdata->tp_info.pool, &contact,
					  acc_id, rdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to generate Contact header", status);
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 500, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    /* Create dialog: */
    status = pjsip_dlg_create_uas( pjsip_ua_instance(), rdata,
				   &contact, &dlg);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 500, NULL,
				      NULL, NULL);

	return PJ_TRUE;
    }

    /* Set credentials */
    if (pjsua_var.acc[acc_id].cred_cnt) {
	pjsip_auth_clt_set_credentials(&dlg->auth_sess, 
				       pjsua_var.acc[acc_id].cred_cnt,
				       pjsua_var.acc[acc_id].cred);
    }

    /* Create invite session: */
    status = pjsip_inv_create_uas( dlg, rdata, answer, 0, &inv);
    if (status != PJ_SUCCESS) {
	pjsip_hdr hdr_list;
	pjsip_warning_hdr *w;

	w = pjsip_warning_hdr_create_from_status(dlg->pool, 
						 pjsip_endpt_name(pjsua_var.endpt),
						 status);
	pj_list_init(&hdr_list);
	pj_list_push_back(&hdr_list, w);

	pjsip_dlg_respond(dlg, rdata, 500, NULL, &hdr_list, NULL);

	/* Can't terminate dialog because transaction is in progress.
	pjsip_dlg_terminate(dlg);
	 */
	return PJ_TRUE;
    }


    /* Create and attach pjsua_var data to the dialog: */
    call->inv = inv;

    dlg->mod_data[pjsua_var.mod.id] = call;
    inv->mod_data[pjsua_var.mod.id] = call;


    /* Must answer with some response to initial INVITE.
     */
    status = pjsip_inv_initial_answer(inv, rdata, 
				      100, NULL, NULL, &response);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send answer to incoming INVITE", 
		     status);

	pjsip_dlg_respond(dlg, rdata, 500, NULL, NULL, NULL);
	pjsip_inv_terminate(inv, 500, PJ_FALSE);
	return PJ_TRUE;

    } else {
	status = pjsip_inv_send_msg(inv, response);
	if (status != PJ_SUCCESS)
	    pjsua_perror(THIS_FILE, "Unable to send 100 response", status);
    }

    ++pjsua_var.call_cnt;


    /* Notify application */
    if (pjsua_var.ua_cfg.cb.on_incoming_call)
	pjsua_var.ua_cfg.cb.on_incoming_call(acc_id, call_id, rdata);

    /* This INVITE request has been handled. */
    return PJ_TRUE;
}



/*
 * Check if the specified call has active INVITE session and the INVITE
 * session has not been disconnected.
 */
PJ_DEF(pj_bool_t) pjsua_call_is_active(pjsua_call_id call_id)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);
    return pjsua_var.calls[call_id].inv != NULL &&
	   pjsua_var.calls[call_id].inv->state != PJSIP_INV_STATE_DISCONNECTED;
}


/*
 * Check if call has an active media session.
 */
PJ_DEF(pj_bool_t) pjsua_call_has_media(pjsua_call_id call_id)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls, 
		     PJ_EINVAL);
    return pjsua_var.calls[call_id].session != NULL;
}


/*
 * Get the conference port identification associated with the call.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_call_get_conf_port(pjsua_call_id call_id)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls, 
		     PJ_EINVAL);
    return pjsua_var.calls[call_id].conf_slot;
}


/*
 * Obtain detail information about the specified call.
 */
PJ_DEF(pj_status_t) pjsua_call_get_info( pjsua_call_id call_id,
					 pjsua_call_info *info)
{
    pjsua_call *call;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    pj_bzero(info, sizeof(*info));

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (call->inv == NULL) {
	PJSUA_UNLOCK();
	return PJ_SUCCESS;
    }

    pjsip_dlg_inc_lock(call->inv->dlg);


    /* id and role */
    info->id = call_id;
    info->role = call->inv->role;

    /* local info */
    info->local_info.ptr = info->buf_.local_info;
    pj_strncpy(&info->local_info, &call->inv->dlg->local.info_str,
	       sizeof(info->buf_.local_info));

    /* local contact */
    info->local_contact.ptr = info->buf_.local_contact;
    info->local_contact.slen = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
					       call->inv->dlg->local.contact->uri,
					       info->local_contact.ptr,
					       sizeof(info->buf_.local_contact));

    /* remote info */
    info->remote_info.ptr = info->buf_.remote_info;
    pj_strncpy(&info->remote_info, &call->inv->dlg->remote.info_str,
	       sizeof(info->buf_.remote_info));

    /* remote contact */
    if (call->inv->dlg->remote.contact) {
	int len;
	info->remote_contact.ptr = info->buf_.remote_contact;
	len = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
			      call->inv->dlg->remote.contact->uri,
			      info->remote_contact.ptr,
			      sizeof(info->buf_.remote_contact));
	if (len < 0) len = 0;
	info->remote_contact.slen = len;
    } else {
	info->remote_contact.slen = 0;
    }

    /* call id */
    info->call_id.ptr = info->buf_.call_id;
    pj_strncpy(&info->call_id, &call->inv->dlg->call_id->id,
	       sizeof(info->buf_.call_id));

    /* state, state_text */
    info->state = call->inv->state;
    info->state_text = pj_str((char*)pjsip_inv_state_name(info->state));

    /* If call is disconnected, set the last_status from the cause code */
    if (call->inv->state >= PJSIP_INV_STATE_DISCONNECTED) {
	/* last_status, last_status_text */
	info->last_status = call->inv->cause;

	info->last_status_text.ptr = info->buf_.last_status_text;
	pj_strncpy(&info->last_status_text, &call->inv->cause_text,
		   sizeof(info->buf_.last_status_text));
    } else {
	/* last_status, last_status_text */
	info->last_status = call->last_code;

	info->last_status_text.ptr = info->buf_.last_status_text;
	pj_strncpy(&info->last_status_text, &call->last_text,
		   sizeof(info->buf_.last_status_text));
    }
    
    /* media status and dir */
    info->media_status = call->media_st;
    info->media_dir = call->media_dir;


    /* conference slot number */
    info->conf_slot = call->conf_slot;

    /* calculate duration */
    if (info->state >= PJSIP_INV_STATE_DISCONNECTED) {

	info->total_duration = call->dis_time;
	PJ_TIME_VAL_SUB(info->total_duration, call->start_time);

	if (call->conn_time.sec) {
	    info->connect_duration = call->dis_time;
	    PJ_TIME_VAL_SUB(info->connect_duration, call->conn_time);
	}

    } else if (info->state == PJSIP_INV_STATE_CONFIRMED) {

	pj_gettimeofday(&info->total_duration);
	PJ_TIME_VAL_SUB(info->total_duration, call->start_time);

	pj_gettimeofday(&info->connect_duration);
	PJ_TIME_VAL_SUB(info->connect_duration, call->conn_time);

    } else {
	pj_gettimeofday(&info->total_duration);
	PJ_TIME_VAL_SUB(info->total_duration, call->start_time);
    }

    pjsip_dlg_dec_lock(call->inv->dlg);
    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Attach application specific data to the call.
 */
PJ_DEF(pj_status_t) pjsua_call_set_user_data( pjsua_call_id call_id,
					      void *user_data)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);
    pjsua_var.calls[call_id].user_data = user_data;

    return PJ_SUCCESS;
}


/*
 * Get user data attached to the call.
 */
PJ_DEF(void*) pjsua_call_get_user_data(pjsua_call_id call_id)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     NULL);
    return pjsua_var.calls[call_id].user_data;
}


/*
 * Send response to incoming INVITE request.
 */
PJ_DEF(pj_status_t) pjsua_call_answer( pjsua_call_id call_id, 
				       unsigned code,
				       const pj_str_t *reason,
				       const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (call->inv == NULL) {
	PJ_LOG(3,(THIS_FILE, "Call %d already disconnected", call_id));
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONTERMINATED;
    }

    if (call->res_time.sec == 0)
	pj_gettimeofday(&call->res_time);

    /* Create response message */
    status = pjsip_inv_answer(call->inv, code, reason, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating response", 
		     status);
	PJSUA_UNLOCK();
	return status;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the message */
    status = pjsip_inv_send_msg(call->inv, tdata);
    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error sending response", 
		     status);

    PJSUA_UNLOCK();

    return status;
}


/*
 * Hangup call by using method that is appropriate according to the
 * call state.
 */
PJ_DEF(pj_status_t) pjsua_call_hangup(pjsua_call_id call_id,
				      unsigned code,
				      const pj_str_t *reason,
				      const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pj_status_t status;
    pjsip_tx_data *tdata;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (!call->inv) {
	PJ_LOG(3,(THIS_FILE,"Invalid call or call has been disconnected"));
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    if (code==0) {
	if (call->inv->state == PJSIP_INV_STATE_CONFIRMED)
	    code = PJSIP_SC_OK;
	else if (call->inv->role == PJSIP_ROLE_UAS)
	    code = PJSIP_SC_DECLINE;
	else
	    code = PJSIP_SC_REQUEST_TERMINATED;
    }

    status = pjsip_inv_end_session(call->inv, code, reason, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Failed to create end session message", 
		     status);
	PJSUA_UNLOCK();
	return status;
    }

    /* pjsip_inv_end_session may return PJ_SUCCESS with NULL 
     * as p_tdata when INVITE transaction has not been answered
     * with any provisional responses.
     */
    if (tdata == NULL) {
	PJSUA_UNLOCK();
	return PJ_SUCCESS;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the message */
    status = pjsip_inv_send_msg(call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Failed to send end session message", 
		     status);
	PJSUA_UNLOCK();
	return status;
    }

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Put the specified call on hold.
 */
PJ_DEF(pj_status_t) pjsua_call_set_hold(pjsua_call_id call_id,
					const pjsua_msg_data *msg_data)
{
    pjmedia_sdp_session *sdp;
    pjsua_call *call;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];
    
    if (!call->inv) {
	PJ_LOG(3,(THIS_FILE,"Call has been disconnected"));
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONTERMINATED;
    }

    if (call->inv->state != PJSIP_INV_STATE_CONFIRMED) {
	PJ_LOG(3,(THIS_FILE, "Can not hold call that is not confirmed"));
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONSTATE;
    }

    status = create_inactive_sdp(call, &sdp);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK();
	return status;
    }

    /* Create re-INVITE with new offer */
    status = pjsip_inv_reinvite( call->inv, NULL, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create re-INVITE", status);
	PJSUA_UNLOCK();
	return status;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the request */
    status = pjsip_inv_send_msg( call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send re-INVITE", status);
	PJSUA_UNLOCK();
	return status;
    }

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Send re-INVITE (to release hold).
 */
PJ_DEF(pj_status_t) pjsua_call_reinvite( pjsua_call_id call_id,
					 pj_bool_t unhold,
					 const pjsua_msg_data *msg_data)
{
    pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pjsua_call *call;
    pj_status_t status;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (!call->inv) {
	PJ_LOG(3,(THIS_FILE,"Call has been disconnected"));
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONTERMINATED;
    }


    if (call->inv->state != PJSIP_INV_STATE_CONFIRMED) {
	PJ_LOG(3,(THIS_FILE, "Can not re-INVITE call that is not confirmed"));
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONSTATE;
    }

    /* Create SDP */
    PJ_UNUSED_ARG(unhold);
    PJ_TODO(create_active_inactive_sdp_based_on_unhold_arg);
    status = pjmedia_endpt_create_sdp( pjsua_var.med_endpt, call->inv->pool, 
				       1, &call->skinfo, &sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to get SDP from media endpoint", 
		     status);
	PJSUA_UNLOCK();
	return status;
    }

    /* Create re-INVITE with new offer */
    status = pjsip_inv_reinvite( call->inv, NULL, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create re-INVITE", status);
	PJSUA_UNLOCK();
	return status;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the request */
    status = pjsip_inv_send_msg( call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send re-INVITE", status);
	PJSUA_UNLOCK();
	return status;
    }

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Initiate call transfer to the specified address.
 */
PJ_DEF(pj_status_t) pjsua_call_xfer( pjsua_call_id call_id, 
				     const pj_str_t *dest,
				     const pjsua_msg_data *msg_data)
{
    pjsip_evsub *sub;
    pjsip_tx_data *tdata;
    pjsua_call *call;
    pj_status_t status;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);
    
    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (!call->inv) {
	PJ_LOG(3,(THIS_FILE,"Call has been disconnected"));
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONTERMINATED;
    }
   
    /* Create xfer client subscription.
     * We're not interested in knowing the transfer result, so we
     * put NULL as the callback.
     */
    status = pjsip_xfer_create_uac(call->inv->dlg, NULL, &sub);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create xfer", status);
	PJSUA_UNLOCK();
	return status;
    }

    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate(sub, dest, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create REFER request", status);
	PJSUA_UNLOCK();
	return status;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send. */
    status = pjsip_xfer_send_request(sub, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send REFER request", status);
	PJSUA_UNLOCK();
	return status;
    }

    /* For simplicity (that's what this program is intended to be!), 
     * leave the original invite session as it is. More advanced application
     * may want to hold the INVITE, or terminate the invite, or whatever.
     */

    PJSUA_UNLOCK();

    return PJ_SUCCESS;

}


/*
 * Send DTMF digits to remote using RFC 2833 payload formats.
 */
PJ_DEF(pj_status_t) pjsua_call_dial_dtmf( pjsua_call_id call_id, 
					  const pj_str_t *digits)
{
    pjsua_call *call;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);
    
    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (!call->session) {
	PJ_LOG(3,(THIS_FILE, "Media is not established yet!"));
	PJSUA_UNLOCK();
	return PJ_EINVALIDOP;
    }

    status = pjmedia_session_dial_dtmf( call->session, 0, digits);

    PJSUA_UNLOCK();

    return status;
}


/**
 * Send instant messaging inside INVITE session.
 */
PJ_DEF(pj_status_t) pjsua_call_send_im( pjsua_call_id call_id, 
					const pj_str_t *mime_type,
					const pj_str_t *content,
					const pjsua_msg_data *msg_data,
					void *user_data)
{
    pjsua_call *call;
    const pj_str_t mime_text_plain = pj_str("text/plain");
    pjsip_media_type ctype;
    pjsua_im_data *im_data;
    pjsip_tx_data *tdata;
    pj_status_t status;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (!call->inv) {
	PJ_LOG(3,(THIS_FILE,"Call has been disconnected"));
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONTERMINATED;
    }

    /* Lock dialog. */
    pjsip_dlg_inc_lock(call->inv->dlg);

    /* Set default media type if none is specified */
    if (mime_type == NULL) {
	mime_type = &mime_text_plain;
    }

    /* Create request message. */
    status = pjsip_dlg_create_request( call->inv->dlg, &pjsip_message_method,
				       -1, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create MESSAGE request", status);
	goto on_return;
    }

    /* Add accept header. */
    pjsip_msg_add_hdr( tdata->msg, 
		       (pjsip_hdr*)pjsua_im_create_accept(tdata->pool));

    /* Parse MIME type */
    pjsua_parse_media_type(tdata->pool, mime_type, &ctype);

    /* Create "text/plain" message body. */
    tdata->msg->body = pjsip_msg_body_create( tdata->pool, &ctype.type,
					      &ctype.subtype, content);
    if (tdata->msg->body == NULL) {
	pjsua_perror(THIS_FILE, "Unable to create msg body", PJ_ENOMEM);
	pjsip_tx_data_dec_ref(tdata);
	goto on_return;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Create IM data and attach to the request. */
    im_data = pj_pool_zalloc(tdata->pool, sizeof(*im_data));
    im_data->acc_id = call->acc_id;
    im_data->call_id = call_id;
    im_data->to = call->inv->dlg->remote.info_str;
    pj_strdup_with_null(tdata->pool, &im_data->body, content);
    im_data->user_data = user_data;


    /* Send the request. */
    status = pjsip_dlg_send_request( call->inv->dlg, tdata, 
				     pjsua_var.mod.id, im_data);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send MESSAGE request", status);
	goto on_return;
    }

on_return:
    pjsip_dlg_dec_lock(call->inv->dlg);
    PJSUA_UNLOCK();
    return status;
}


/*
 * Send IM typing indication inside INVITE session.
 */
PJ_DEF(pj_status_t) pjsua_call_send_typing_ind( pjsua_call_id call_id, 
						pj_bool_t is_typing,
						const pjsua_msg_data*msg_data)
{
    pjsua_call *call;
    pjsip_tx_data *tdata;
    pj_status_t status;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (!call->inv) {
	PJ_LOG(3,(THIS_FILE,"Call has been disconnected"));
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONTERMINATED;
    }

    /* Lock dialog. */
    pjsip_dlg_inc_lock(call->inv->dlg);
    
    /* Create request message. */
    status = pjsip_dlg_create_request( call->inv->dlg, &pjsip_message_method,
				       -1, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create MESSAGE request", status);
	goto on_return;
    }

    /* Create "application/im-iscomposing+xml" msg body. */
    tdata->msg->body = pjsip_iscomposing_create_body(tdata->pool, is_typing,
						     NULL, NULL, -1);

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the request. */
    status = pjsip_dlg_send_request( call->inv->dlg, tdata, -1, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send MESSAGE request", status);
	goto on_return;
    }

on_return:
    pjsip_dlg_dec_lock(call->inv->dlg);
    PJSUA_UNLOCK();
    return status;
}


/*
 * Terminate all calls.
 */
PJ_DEF(void) pjsua_call_hangup_all(void)
{
    unsigned i;

    PJSUA_LOCK();

    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	if (pjsua_var.calls[i].inv)
	    pjsua_call_hangup(i, 0, NULL, NULL);
    }

    PJSUA_UNLOCK();
}


static const char *good_number(char *buf, pj_int32_t val)
{
    if (val < 1000) {
	pj_ansi_sprintf(buf, "%d", val);
    } else if (val < 1000000) {
	pj_ansi_sprintf(buf, "%d.%dK", 
			val / 1000,
			(val % 1000) / 100);
    } else {
	pj_ansi_sprintf(buf, "%d.%02dM", 
			val / 1000000,
			(val % 1000000) / 10000);
    }

    return buf;
}


/* Dump media session */
static void dump_media_session(const char *indent, 
			       char *buf, unsigned maxlen,
			       pjmedia_session *session)
{
    unsigned i;
    char *p = buf, *end = buf+maxlen;
    int len;
    pjmedia_session_info info;

    pjmedia_session_get_info(session, &info);

    for (i=0; i<info.stream_cnt; ++i) {
	pjmedia_rtcp_stat stat;
	const char *rem_addr;
	int rem_port;
	const char *dir;
	char last_update[64];
	char packets[32], bytes[32], ipbytes[32], avg_bps[32];
	pj_time_val media_duration, now;

	pjmedia_session_get_stream_stat(session, i, &stat);
	rem_addr = pj_inet_ntoa(info.stream_info[i].rem_addr.sin_addr);
	rem_port = pj_ntohs(info.stream_info[i].rem_addr.sin_port);

	if (info.stream_info[i].dir == PJMEDIA_DIR_ENCODING)
	    dir = "sendonly";
	else if (info.stream_info[i].dir == PJMEDIA_DIR_DECODING)
	    dir = "recvonly";
	else if (info.stream_info[i].dir == PJMEDIA_DIR_ENCODING_DECODING)
	    dir = "sendrecv";
	else
	    dir = "inactive";

	
	len = pj_ansi_snprintf(buf, end-p, 
		  "%s  #%d %.*s @%dKHz, %s, peer=%s:%d",
		  indent, i,
		  (int)info.stream_info[i].fmt.encoding_name.slen,
		  info.stream_info[i].fmt.encoding_name.ptr,
		  info.stream_info[i].fmt.clock_rate / 1000,
		  dir,
		  rem_addr, rem_port);
	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';

	if (stat.rx.update_cnt == 0)
	    strcpy(last_update, "never");
	else {
	    pj_gettimeofday(&now);
	    PJ_TIME_VAL_SUB(now, stat.rx.update);
	    sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		    now.sec / 3600,
		    (now.sec % 3600) / 60,
		    now.sec % 60,
		    now.msec);
	}

	pj_gettimeofday(&media_duration);
	PJ_TIME_VAL_SUB(media_duration, stat.start);
	if (PJ_TIME_VAL_MSEC(media_duration) == 0)
	    media_duration.msec = 1;

	len = pj_ansi_snprintf(p, end-p,
	       "%s     RX pt=%d, stat last update: %s\n"
	       "%s        total %spkt %sB (%sB +IP hdr) @avg=%sbps\n"
	       "%s        pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)\n"
	       "%s              (msec)    min     avg     max     last\n"
	       "%s        loss period: %7.3f %7.3f %7.3f %7.3f\n"
	       "%s        jitter     : %7.3f %7.3f %7.3f %7.3f%s",
	       indent, info.stream_info[i].fmt.pt,
	       last_update,
	       indent,
	       good_number(packets, stat.rx.pkt),
	       good_number(bytes, stat.rx.bytes),
	       good_number(ipbytes, stat.rx.bytes + stat.rx.pkt * 32),
	       good_number(avg_bps, stat.rx.bytes * 8 * 1000 / PJ_TIME_VAL_MSEC(media_duration)),
	       indent,
	       stat.rx.loss,
	       stat.rx.loss * 100.0 / (stat.rx.pkt + stat.rx.loss),
	       stat.rx.dup, 
	       stat.rx.dup * 100.0 / (stat.rx.pkt + stat.rx.loss),
	       stat.rx.reorder, 
	       stat.rx.reorder * 100.0 / (stat.rx.pkt + stat.rx.loss),
	       indent, indent,
	       stat.rx.loss_period.min / 1000.0, 
	       stat.rx.loss_period.avg / 1000.0, 
	       stat.rx.loss_period.max / 1000.0,
	       stat.rx.loss_period.last / 1000.0,
	       indent,
	       stat.rx.jitter.min / 1000.0,
	       stat.rx.jitter.avg / 1000.0,
	       stat.rx.jitter.max / 1000.0,
	       stat.rx.jitter.last / 1000.0,
	       ""
	       );

	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';
	
	if (stat.tx.update_cnt == 0)
	    strcpy(last_update, "never");
	else {
	    pj_gettimeofday(&now);
	    PJ_TIME_VAL_SUB(now, stat.tx.update);
	    sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		    now.sec / 3600,
		    (now.sec % 3600) / 60,
		    now.sec % 60,
		    now.msec);
	}

	len = pj_ansi_snprintf(p, end-p,
	       "%s     TX pt=%d, ptime=%dms, stat last update: %s\n"
	       "%s        total %spkt %sB (%sB +IP hdr) @avg %sbps\n"
	       "%s        pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)\n"
	       "%s              (msec)    min     avg     max     last\n"
	       "%s        loss period: %7.3f %7.3f %7.3f %7.3f\n"
	       "%s        jitter     : %7.3f %7.3f %7.3f %7.3f%s",
	       indent,
	       info.stream_info[i].tx_pt,
	       info.stream_info[i].param->info.frm_ptime *
		info.stream_info[i].param->setting.frm_per_pkt,
	       last_update,

	       indent,
	       good_number(packets, stat.tx.pkt),
	       good_number(bytes, stat.tx.bytes),
	       good_number(ipbytes, stat.tx.bytes + stat.tx.pkt * 32),
	       good_number(avg_bps, stat.tx.bytes * 8 * 1000 / PJ_TIME_VAL_MSEC(media_duration)),

	       indent,
	       stat.tx.loss,
	       stat.tx.loss * 100.0 / (stat.tx.pkt + stat.tx.loss),
	       stat.tx.dup, 
	       stat.tx.dup * 100.0 / (stat.tx.pkt + stat.tx.loss),
	       stat.tx.reorder, 
	       stat.tx.reorder * 100.0 / (stat.tx.pkt + stat.tx.loss),

	       indent, indent,
	       stat.tx.loss_period.min / 1000.0, 
	       stat.tx.loss_period.avg / 1000.0, 
	       stat.tx.loss_period.max / 1000.0,
	       stat.tx.loss_period.last / 1000.0,
	       indent,
	       stat.tx.jitter.min / 1000.0,
	       stat.tx.jitter.avg / 1000.0,
	       stat.tx.jitter.max / 1000.0,
	       stat.tx.jitter.last / 1000.0,
	       ""
	       );

	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';

	len = pj_ansi_snprintf(p, end-p,
	       "%s    RTT msec       : %7.3f %7.3f %7.3f %7.3f", 
	       indent,
	       stat.rtt.min / 1000.0,
	       stat.rtt.avg / 1000.0,
	       stat.rtt.max / 1000.0,
	       stat.rtt.last / 1000.0
	       );
	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';
    }
}


/* Print call info */
static void print_call(const char *title,
		       int call_id, 
		       char *buf, pj_size_t size)
{
    int len;
    pjsip_inv_session *inv = pjsua_var.calls[call_id].inv;
    pjsip_dialog *dlg = inv->dlg;
    char userinfo[128];

    /* Dump invite sesion info. */

    len = pjsip_hdr_print_on(dlg->remote.info, userinfo, sizeof(userinfo));
    if (len < 1)
	pj_ansi_strcpy(userinfo, "<--uri too long-->");
    else
	userinfo[len] = '\0';
    
    len = pj_ansi_snprintf(buf, size, "%s[%s] %s",
			   title,
			   pjsip_inv_state_name(inv->state),
			   userinfo);
    if (len < 1 || len >= (int)size) {
	pj_ansi_strcpy(buf, "<--uri too long-->");
	len = 18;
    } else
	buf[len] = '\0';
}


/*
 * Dump call and media statistics to string.
 */
PJ_DEF(pj_status_t) pjsua_call_dump( pjsua_call_id call_id, 
				     pj_bool_t with_media, 
				     char *buffer, 
				     unsigned maxlen,
				     const char *indent)
{
    pjsua_call *call;
    pj_time_val duration, res_delay, con_delay;
    char tmp[128];
    char *p, *end;
    int len;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    *buffer = '\0';
    p = buffer;
    end = buffer + maxlen;
    len = 0;

    if (call->inv == NULL) {
	PJSUA_UNLOCK();
	return PJ_EINVALIDOP;
    }

    print_call(indent, call_id, tmp, sizeof(tmp));
    
    len = pj_ansi_strlen(tmp);
    pj_ansi_strcpy(buffer, tmp);

    p += len;
    *p++ = '\r';
    *p++ = '\n';

    /* Calculate call duration */
    if (call->inv->state >= PJSIP_INV_STATE_CONFIRMED) {
	pj_gettimeofday(&duration);
	PJ_TIME_VAL_SUB(duration, call->conn_time);
	con_delay = call->conn_time;
	PJ_TIME_VAL_SUB(con_delay, call->start_time);
    } else {
	duration.sec = duration.msec = 0;
	con_delay.sec = con_delay.msec = 0;
    }

    /* Calculate first response delay */
    if (call->inv->state >= PJSIP_INV_STATE_EARLY) {
	res_delay = call->res_time;
	PJ_TIME_VAL_SUB(res_delay, call->start_time);
    } else {
	res_delay.sec = res_delay.msec = 0;
    }

    /* Print duration */
    len = pj_ansi_snprintf(p, end-p, 
		           "%s  Call time: %02dh:%02dm:%02ds, "
		           "1st res in %d ms, conn in %dms",
			   indent,
		           (int)(duration.sec / 3600),
		           (int)((duration.sec % 3600)/60),
		           (int)(duration.sec % 60),
		           (int)PJ_TIME_VAL_MSEC(res_delay), 
		           (int)PJ_TIME_VAL_MSEC(con_delay));
    
    if (len > 0 && len < end-p) {
	p += len;
	*p++ = '\n';
	*p = '\0';
    }

    /* Dump session statistics */
    if (with_media && call->session)
	dump_media_session(indent, p, end-p, call->session);

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Destroy the call's media
 */
static pj_status_t call_destroy_media(int call_id)
{
    pjsua_call *call = &pjsua_var.calls[call_id];

    if (call->conf_slot != PJSUA_INVALID_ID) {
	pjmedia_conf_remove_port(pjsua_var.mconf, call->conf_slot);
	call->conf_slot = PJSUA_INVALID_ID;
    }

    if (call->session) {
	/* Destroy session (this will also close RTP/RTCP sockets). */
	pjmedia_session_destroy(call->session);
	call->session = NULL;

	PJ_LOG(4,(THIS_FILE, "Media session for call %d is destroyed", 
			     call_id));

    }

    call->media_st = PJSUA_CALL_MEDIA_NONE;

    return PJ_SUCCESS;
}


/*
 * This callback receives notification from invite session when the
 * session state has changed.
 */
static void pjsua_call_on_state_changed(pjsip_inv_session *inv, 
					pjsip_event *e)
{
    pjsua_call *call;

    PJSUA_LOCK();

    call = inv->dlg->mod_data[pjsua_var.mod.id];

    if (!call) {
	PJSUA_UNLOCK();
	return;
    }


    /* Get call times */
    switch (inv->state) {
	case PJSIP_INV_STATE_EARLY:
	case PJSIP_INV_STATE_CONNECTING:
	    if (call->res_time.sec == 0)
		pj_gettimeofday(&call->res_time);
	    call->last_code = e->body.tsx_state.tsx->status_code;
	    pj_strncpy(&call->last_text, 
		       &e->body.tsx_state.tsx->status_text,
		       sizeof(call->last_text_buf_));
	    break;
	case PJSIP_INV_STATE_CONFIRMED:
	    pj_gettimeofday(&call->conn_time);
	    break;
	case PJSIP_INV_STATE_DISCONNECTED:
	    pj_gettimeofday(&call->dis_time);
	    if (call->res_time.sec == 0)
		pj_gettimeofday(&call->res_time);
	    if (e->body.tsx_state.tsx->status_code > call->last_code) {
		call->last_code = e->body.tsx_state.tsx->status_code;
		pj_strncpy(&call->last_text, 
			   &e->body.tsx_state.tsx->status_text,
			   sizeof(call->last_text_buf_));
	    }
	    break;
	default:
	    call->last_code = e->body.tsx_state.tsx->status_code;
	    pj_strncpy(&call->last_text, 
		       &e->body.tsx_state.tsx->status_text,
		       sizeof(call->last_text_buf_));
	    break;
    }

    /* If this is an outgoing INVITE that was created because of
     * REFER/transfer, send NOTIFY to transferer.
     */
    if (call->xfer_sub && e->type==PJSIP_EVENT_TSX_STATE)  {
	int st_code = -1;
	pjsip_evsub_state ev_state = PJSIP_EVSUB_STATE_ACTIVE;
	

	switch (call->inv->state) {
	case PJSIP_INV_STATE_NULL:
	case PJSIP_INV_STATE_CALLING:
	    /* Do nothing */
	    break;

	case PJSIP_INV_STATE_EARLY:
	case PJSIP_INV_STATE_CONNECTING:
	    st_code = e->body.tsx_state.tsx->status_code;
	    ev_state = PJSIP_EVSUB_STATE_ACTIVE;
	    break;

	case PJSIP_INV_STATE_CONFIRMED:
	    /* When state is confirmed, send the final 200/OK and terminate
	     * subscription.
	     */
	    st_code = e->body.tsx_state.tsx->status_code;
	    ev_state = PJSIP_EVSUB_STATE_TERMINATED;
	    break;

	case PJSIP_INV_STATE_DISCONNECTED:
	    st_code = e->body.tsx_state.tsx->status_code;
	    ev_state = PJSIP_EVSUB_STATE_TERMINATED;
	    break;

	case PJSIP_INV_STATE_INCOMING:
	    /* Nothing to do. Just to keep gcc from complaining about
	     * unused enums.
	     */
	    break;
	}

	if (st_code != -1) {
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    status = pjsip_xfer_notify( call->xfer_sub,
					ev_state, st_code,
					NULL, &tdata);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to create NOTIFY", status);
	    } else {
		status = pjsip_xfer_send_request(call->xfer_sub, tdata);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Unable to send NOTIFY", status);
		}
	    }
	}
    }


    if (pjsua_var.ua_cfg.cb.on_call_state)
	(*pjsua_var.ua_cfg.cb.on_call_state)(call->index, e);

    /* call->inv may be NULL now */

    /* Destroy media session when invite session is disconnected. */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

	pj_assert(call != NULL);

	if (call)
	    call_destroy_media(call->index);

	/* Free call */
	call->inv = NULL;
	--pjsua_var.call_cnt;
    }

    PJSUA_UNLOCK();
}

/*
 * This callback is called by invite session framework when UAC session
 * has forked.
 */
static void pjsua_call_on_forked( pjsip_inv_session *inv, 
				  pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);

    PJ_TODO(HANDLE_FORKED_DIALOG);
}


/*
 * Disconnect call upon error.
 */
static void call_disconnect( pjsip_inv_session *inv, 
			     int code )
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    status = pjsip_inv_end_session(inv, code, NULL, &tdata);
    if (status == PJ_SUCCESS)
	pjsip_inv_send_msg(inv, tdata);
}

/*
 * Callback to be called when SDP offer/answer negotiation has just completed
 * in the session. This function will start/update media if negotiation
 * has succeeded.
 */
static void pjsua_call_on_media_update(pjsip_inv_session *inv,
				       pj_status_t status)
{
    int prev_media_st = 0;
    pjsua_call *call;
    pjmedia_session_info sess_info;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;
    pjmedia_port *media_port;
    pj_str_t port_name;
    char tmp[PJSIP_MAX_URL_SIZE];

    PJSUA_LOCK();

    call = inv->dlg->mod_data[pjsua_var.mod.id];

    if (status != PJ_SUCCESS) {

	pjsua_perror(THIS_FILE, "SDP negotiation has failed", status);

	/* Disconnect call if we're not in the middle of initializing an
	 * UAS dialog and if this is not a re-INVITE 
	 */
	if (inv->state != PJSIP_INV_STATE_NULL &&
	    inv->state != PJSIP_INV_STATE_CONFIRMED) 
	{
	    //call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	}

	PJSUA_UNLOCK();
	return;
    }

    /* Destroy existing media session, if any. */

    if (call) {
	prev_media_st = call->media_st;
	call_destroy_media(call->index);
    }

    /* Get local and remote SDP */

    status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &local_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to retrieve currently active local SDP", 
		     status);
	//call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	PJSUA_UNLOCK();
	return;
    }


    status = pjmedia_sdp_neg_get_active_remote(call->inv->neg, &remote_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to retrieve currently active remote SDP", 
		     status);
	//call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	PJSUA_UNLOCK();
	return;
    }

    /* Create media session info based on SDP parameters. 
     * We only support one stream per session at the moment
     */    
    status = pjmedia_session_info_from_sdp( call->inv->dlg->pool, 
					    pjsua_var.med_endpt, 
					    1,&sess_info, 
					    local_sdp, remote_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create media session", 
		     status);
	call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	PJSUA_UNLOCK();
	return;
    }

    /* Check if media is put on-hold */
    if (sess_info.stream_cnt == 0 || 
	sess_info.stream_info[0].dir == PJMEDIA_DIR_NONE)
    {

	/* Determine who puts the call on-hold */
	if (prev_media_st == PJSUA_CALL_MEDIA_ACTIVE) {
	    if (pjmedia_sdp_neg_was_answer_remote(call->inv->neg)) {
		/* It was local who offer hold */
		call->media_st = PJSUA_CALL_MEDIA_LOCAL_HOLD;
	    } else {
		call->media_st = PJSUA_CALL_MEDIA_REMOTE_HOLD;
	    }
	}

	call->media_dir = PJMEDIA_DIR_NONE;

    } else {

	/* Override ptime, if this option is specified. */
	if (pjsua_var.media_cfg.ptime != 0) {
	    sess_info.stream_info[0].param->setting.frm_per_pkt = (pj_uint8_t)
		(pjsua_var.media_cfg.ptime / sess_info.stream_info[0].param->info.frm_ptime);
	    if (sess_info.stream_info[0].param->setting.frm_per_pkt == 0)
		sess_info.stream_info[0].param->setting.frm_per_pkt = 1;
	}

	/* Disable VAD, if this option is specified. */
	if (pjsua_var.media_cfg.no_vad) {
	    sess_info.stream_info[0].param->setting.vad = 0;
	}


	/* Optionally, application may modify other stream settings here
	 * (such as jitter buffer parameters, codec ptime, etc.)
	 */

	/* Create session based on session info. */
	status = pjmedia_session_create( pjsua_var.med_endpt, &sess_info,
					 &call->med_tp,
					 call, &call->session );
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create media session", 
			 status);
	    //call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	    PJSUA_UNLOCK();
	    return;
	}


	/* Get the port interface of the first stream in the session.
	 * We need the port interface to add to the conference bridge.
	 */
	pjmedia_session_get_port(call->session, 0, &media_port);


	/*
	 * Add the call to conference bridge.
	 */
	port_name.ptr = tmp;
	port_name.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
					 call->inv->dlg->remote.info->uri,
					 tmp, sizeof(tmp));
	if (port_name.slen < 1) {
	    port_name = pj_str("call");
	}
	status = pjmedia_conf_add_port( pjsua_var.mconf, call->inv->pool,
					media_port, 
					&port_name,
					(unsigned*)&call->conf_slot);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create conference slot", 
			 status);
	    call_destroy_media(call->index);
	    //call_disconnect(inv, PJSIP_SC_INTERNAL_SERVER_ERROR);
	    PJSUA_UNLOCK();
	    return;
	}

	/* Call's media state is active */
	call->media_st = PJSUA_CALL_MEDIA_ACTIVE;
	call->media_dir = sess_info.stream_info[0].dir;
    }

    /* Print info. */
    {
	char info[80];
	int info_len = 0;
	unsigned i;

	for (i=0; i<sess_info.stream_cnt; ++i) {
	    int len;
	    const char *dir;
	    pjmedia_stream_info *strm_info = &sess_info.stream_info[i];

	    switch (strm_info->dir) {
	    case PJMEDIA_DIR_NONE:
		dir = "inactive";
		break;
	    case PJMEDIA_DIR_ENCODING:
		dir = "sendonly";
		break;
	    case PJMEDIA_DIR_DECODING:
		dir = "recvonly";
		break;
	    case PJMEDIA_DIR_ENCODING_DECODING:
		dir = "sendrecv";
		break;
	    default:
		dir = "unknown";
		break;
	    }
	    len = pj_ansi_sprintf( info+info_len,
				   ", stream #%d: %.*s (%s)", i,
				   (int)strm_info->fmt.encoding_name.slen,
				   strm_info->fmt.encoding_name.ptr,
				   dir);
	    if (len > 0)
		info_len += len;
	}
	PJ_LOG(4,(THIS_FILE,"Media updates%s", info));
    }

    /* Call application callback, if any */
    if (pjsua_var.ua_cfg.cb.on_call_media_state)
	pjsua_var.ua_cfg.cb.on_call_media_state(call->index);


    PJSUA_UNLOCK();
}


/*
 * Create inactive SDP for call hold.
 */
static pj_status_t create_inactive_sdp(pjsua_call *call,
				       pjmedia_sdp_session **p_answer)
{
    pj_status_t status;
    pjmedia_sdp_conn *conn;
    pjmedia_sdp_attr *attr;
    pjmedia_sdp_session *sdp;

    /* Create new offer */
    status = pjmedia_endpt_create_sdp(pjsua_var.med_endpt, pjsua_var.pool, 1,
				      &call->skinfo, &sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	return status;
    }

    /* Get SDP media connection line */
    conn = sdp->media[0]->conn;
    if (!conn)
	conn = sdp->conn;

    /* Modify address */
    conn->addr = pj_str("0.0.0.0");

    /* Remove existing directions attributes */
    pjmedia_sdp_media_remove_all_attr(sdp->media[0], "sendrecv");
    pjmedia_sdp_media_remove_all_attr(sdp->media[0], "sendonly");
    pjmedia_sdp_media_remove_all_attr(sdp->media[0], "recvonly");
    pjmedia_sdp_media_remove_all_attr(sdp->media[0], "inactive");

    /* Add inactive attribute */
    attr = pjmedia_sdp_attr_create(pjsua_var.pool, "inactive", NULL);
    pjmedia_sdp_media_add_attr(sdp->media[0], attr);

    *p_answer = sdp;

    return status;
}


/*
 * Called when session received new offer.
 */
static void pjsua_call_on_rx_offer(pjsip_inv_session *inv,
				   const pjmedia_sdp_session *offer)
{
    const char *remote_state;
    pjsua_call *call;
    pjmedia_sdp_conn *conn;
    pjmedia_sdp_session *answer;
    pj_bool_t is_remote_active;
    pj_status_t status;

    PJSUA_LOCK();

    call = inv->dlg->mod_data[pjsua_var.mod.id];

    /*
     * See if remote is offering active media (i.e. not on-hold)
     */
    is_remote_active = PJ_TRUE;

    conn = offer->media[0]->conn;
    if (!conn)
	conn = offer->conn;

    if (pj_strcmp2(&conn->addr, "0.0.0.0")==0 ||
	pj_strcmp2(&conn->addr, "0")==0)
    {
	is_remote_active = PJ_FALSE;

    } 
    else if (pjmedia_sdp_media_find_attr2(offer->media[0], "inactive", NULL))
    {
	is_remote_active = PJ_FALSE;
    }

    remote_state = (is_remote_active ? "active" : "inactive");

    /* Supply candidate answer */
    if (call->media_st == PJSUA_CALL_MEDIA_LOCAL_HOLD || !is_remote_active) {
	PJ_LOG(4,(THIS_FILE, 
		  "Call %d: RX new media offer, creating inactive SDP "
		  "(media in offer is %s)", call->index, remote_state));
	status = create_inactive_sdp( call, &answer );
    } else {
	PJ_LOG(4,(THIS_FILE, "Call %d: received updated media offer",
		  call->index));
	status = pjmedia_endpt_create_sdp( pjsua_var.med_endpt, 
					   call->inv->pool, 1,
					   &call->skinfo, &answer);	
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	PJSUA_UNLOCK();
	return;
    }

    status = pjsip_inv_set_sdp_answer(call->inv, answer);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to set answer", status);
	PJSUA_UNLOCK();
	return;
    }

    PJSUA_UNLOCK();
}


/*
 * Callback called by event framework when the xfer subscription state
 * has changed.
 */
static void xfer_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
    
    PJ_UNUSED_ARG(event);

    /*
     * We're only interested when subscription is terminated, to 
     * clear the xfer_sub member of the inv_data.
     */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	pjsua_call *call;

	call = pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
	if (!call)
	    return;

	pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
	call->xfer_sub = NULL;

	PJ_LOG(3,(THIS_FILE, "Xfer subscription terminated"));
    }
}


/*
 * Follow transfer (REFER) request.
 */
static void on_call_transfered( pjsip_inv_session *inv,
			        pjsip_rx_data *rdata )
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjsua_call *existing_call;
    int new_call;
    const pj_str_t str_refer_to = { "Refer-To", 8};
    const pj_str_t str_refer_sub = { "Refer-Sub", 9 };
    pjsip_generic_string_hdr *refer_to;
    pjsip_generic_string_hdr *refer_sub;
    pj_bool_t no_refer_sub = PJ_FALSE;
    char *uri;
    pj_str_t tmp;
    pjsip_status_code code;
    pjsip_evsub *sub;

    existing_call = inv->dlg->mod_data[pjsua_var.mod.id];

    /* Find the Refer-To header */
    refer_to = (pjsip_generic_string_hdr*)
	pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_to, NULL);

    if (refer_to == NULL) {
	/* Invalid Request.
	 * No Refer-To header!
	 */
	PJ_LOG(4,(THIS_FILE, "Received REFER without Refer-To header!"));
	pjsip_dlg_respond( inv->dlg, rdata, 400, NULL, NULL, NULL);
	return;
    }

    /* Find optional Refer-Sub header */
    refer_sub = (pjsip_generic_string_hdr*)
	pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_sub, NULL);

    if (refer_sub) {
	if (!pj_strnicmp2(&refer_sub->hvalue, "true", 4)==0)
	    no_refer_sub = PJ_TRUE;
    }


    /* Notify callback */
    code = PJSIP_SC_OK;
    if (pjsua_var.ua_cfg.cb.on_call_transfered)
	(*pjsua_var.ua_cfg.cb.on_call_transfered)(existing_call->index,
						  &refer_to->hvalue, &code);

    if (code < 200)
	code = 200;
    if (code >= 300) {
	/* Application rejects call transfer request */
	pjsip_dlg_respond( inv->dlg, rdata, code, NULL, NULL, NULL);
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Call to %.*s is being transfered to %.*s",
	      (int)inv->dlg->remote.info_str.slen,
	      inv->dlg->remote.info_str.ptr,
	      (int)refer_to->hvalue.slen, 
	      refer_to->hvalue.ptr));

    if (no_refer_sub) {
	/*
	 * Always answer with 200.
	 */
	pjsip_tx_data *tdata;
	const pj_str_t str_false = { "false", 5};
	pjsip_hdr *hdr;

	status = pjsip_dlg_create_response(inv->dlg, rdata, 200, NULL, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create 200 response to REFER",
			 status);
	    return;
	}

	/* Add Refer-Sub header */
	hdr = (pjsip_hdr*) 
	       pjsip_generic_string_hdr_create(tdata->pool, &str_refer_sub,
					      &str_false);
	pjsip_msg_add_hdr(tdata->msg, hdr);


	/* Send answer */
	status = pjsip_dlg_send_response(inv->dlg, pjsip_rdata_get_tsx(rdata),
					 tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create 200 response to REFER",
			 status);
	    return;
	}

	/* Don't have subscription */
	sub = NULL;

    } else {
	struct pjsip_evsub_user xfer_cb;
	pjsip_hdr hdr_list;

	/* Init callback */
	pj_bzero(&xfer_cb, sizeof(xfer_cb));
	xfer_cb.on_evsub_state = &xfer_on_evsub_state;

	/* Init additional header list to be sent with REFER response */
	pj_list_init(&hdr_list);

	/* Create transferee event subscription */
	status = pjsip_xfer_create_uas( inv->dlg, &xfer_cb, rdata, &sub);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create xfer uas", status);
	    pjsip_dlg_respond( inv->dlg, rdata, 500, NULL, NULL, NULL);
	    return;
	}

	/* If there's Refer-Sub header and the value is "true", send back
	 * Refer-Sub in the response with value "true" too.
	 */
	if (refer_sub) {
	    const pj_str_t str_true = { "true", 4 };
	    pjsip_hdr *hdr;

	    hdr = (pjsip_hdr*) 
		   pjsip_generic_string_hdr_create(inv->dlg->pool, 
						   &str_refer_sub,
						   &str_true);
	    pj_list_push_back(&hdr_list, hdr);

	}

	/* Accept the REFER request, send 200 (OK). */
	pjsip_xfer_accept(sub, rdata, code, &hdr_list);

	/* Create initial NOTIFY request */
	status = pjsip_xfer_notify( sub, PJSIP_EVSUB_STATE_ACTIVE,
				    100, NULL, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create NOTIFY to REFER", 
			 status);
	    return;
	}

	/* Send initial NOTIFY request */
	status = pjsip_xfer_send_request( sub, tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to send NOTIFY to REFER", status);
	    return;
	}
    }

    /* We're cheating here.
     * We need to get a null terminated string from a pj_str_t.
     * So grab the pointer from the hvalue and NULL terminate it, knowing
     * that the NULL position will be occupied by a newline. 
     */
    uri = refer_to->hvalue.ptr;
    uri[refer_to->hvalue.slen] = '\0';

    /* Now make the outgoing call. */
    tmp = pj_str(uri);
    status = pjsua_call_make_call(existing_call->acc_id, &tmp, 0,
				  existing_call->user_data, NULL, 
				  &new_call);
    if (status != PJ_SUCCESS) {

	/* Notify xferer about the error (if we have subscription) */
	if (sub) {
	    status = pjsip_xfer_notify(sub, PJSIP_EVSUB_STATE_TERMINATED,
				       500, NULL, &tdata);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to create NOTIFY to REFER", 
			      status);
		return;
	    }
	    status = pjsip_xfer_send_request(sub, tdata);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to send NOTIFY to REFER", 
			      status);
		return;
	    }
	}
	return;
    }

    if (sub) {
	/* Put the server subscription in inv_data.
	 * Subsequent state changed in pjsua_inv_on_state_changed() will be
	 * reported back to the server subscription.
	 */
	pjsua_var.calls[new_call].xfer_sub = sub;

	/* Put the invite_data in the subscription. */
	pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, 
				 &pjsua_var.calls[new_call]);
    }
}



/*
 * This callback is called when transaction state has changed in INVITE
 * session. We use this to trap:
 *  - incoming REFER request.
 *  - incoming MESSAGE request.
 */
static void pjsua_call_on_tsx_state_changed(pjsip_inv_session *inv,
					    pjsip_transaction *tsx,
					    pjsip_event *e)
{
    pjsua_call *call = inv->dlg->mod_data[pjsua_var.mod.id];

    PJSUA_LOCK();

    if (tsx->role==PJSIP_ROLE_UAS &&
	tsx->state==PJSIP_TSX_STATE_TRYING &&
	pjsip_method_cmp(&tsx->method, &pjsip_refer_method)==0)
    {
	/*
	 * Incoming REFER request.
	 */
	on_call_transfered(call->inv, e->body.tsx_state.src.rdata);

    }
    else if (tsx->role==PJSIP_ROLE_UAS &&
	     tsx->state==PJSIP_TSX_STATE_TRYING &&
	     pjsip_method_cmp(&tsx->method, &pjsip_message_method)==0)
    {
	/*
	 * Incoming MESSAGE request!
	 */
	pjsip_rx_data *rdata;
	pjsip_msg *msg;
	pjsip_accept_hdr *accept_hdr;
	pj_status_t status;

	rdata = e->body.tsx_state.src.rdata;
	msg = rdata->msg_info.msg;

	/* Request MUST have message body, with Content-Type equal to
	 * "text/plain".
	 */
	if (pjsua_im_accept_pager(rdata, &accept_hdr) == PJ_FALSE) {

	    pjsip_hdr hdr_list;

	    pj_list_init(&hdr_list);
	    pj_list_push_back(&hdr_list, accept_hdr);

	    pjsip_dlg_respond( inv->dlg, rdata, PJSIP_SC_NOT_ACCEPTABLE_HERE, 
			       NULL, &hdr_list, NULL );
	    PJSUA_UNLOCK();
	    return;
	}

	/* Respond with 200 first, so that remote doesn't retransmit in case
	 * the UI takes too long to process the message. 
	 */
	status = pjsip_dlg_respond( inv->dlg, rdata, 200, NULL, NULL, NULL);

	/* Process MESSAGE request */
	pjsua_im_process_pager(call->index, &inv->dlg->remote.info_str,
			       &inv->dlg->local.info_str, rdata);

    }
    else if (tsx->role == PJSIP_ROLE_UAC &&
	     pjsip_method_cmp(&tsx->method, &pjsip_message_method)==0)
    {
	/* Handle outgoing pager status */
	if (tsx->status_code >= 200) {
	    pjsua_im_data *im_data;

	    im_data = tsx->mod_data[pjsua_var.mod.id];
	    /* im_data can be NULL if this is typing indication */

	    if (im_data && pjsua_var.ua_cfg.cb.on_pager_status) {
		pjsua_var.ua_cfg.cb.on_pager_status(im_data->call_id,
						    &im_data->to,
						    &im_data->body,
						    im_data->user_data,
						    tsx->status_code,
						    &tsx->status_text);
	    }
	}
    }


    PJSUA_UNLOCK();
}

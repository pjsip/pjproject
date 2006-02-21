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
#include "pjsua.h"
#include <pj/log.h>


/*
 * pjsua_inv.c
 *
 * Invite session specific functionalities.
 */

#define THIS_FILE   "pjsua_inv.c"


/**
 * Make outgoing call.
 */
pj_status_t pjsua_invite(const char *cstr_dest_uri,
			 pjsip_inv_session **p_inv)
{
    pj_str_t dest_uri;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *offer;
    pjsip_inv_session *inv;
    struct pjsua_inv_data *inv_data;
    pjsip_tx_data *tdata;
    int med_sk_index = 0;
    pj_status_t status;

    /* Convert cstr_dest_uri to dest_uri */
    
    dest_uri = pj_str((char*)cstr_dest_uri);

    /* Find free socket. */
    for (med_sk_index=0; med_sk_index<PJSUA_MAX_CALLS; ++med_sk_index) {
	if (!pjsua.med_sock_use[med_sk_index])
	    break;
    }

    if (med_sk_index == PJSUA_MAX_CALLS) {
	PJ_LOG(3,(THIS_FILE, "Error: too many calls!"));
	return PJ_ETOOMANY;
    }

    pjsua.med_sock_use[med_sk_index] = 1;

    /* Create outgoing dialog: */

    status = pjsip_dlg_create_uac( pjsip_ua_instance(), &pjsua.local_uri,
				   &pjsua.contact_uri, &dest_uri, &dest_uri,
				   &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Dialog creation failed", status);
	return status;
    }

    /* Get media capability from media endpoint: */

    status = pjmedia_endpt_create_sdp( pjsua.med_endpt, dlg->pool,
				       1, &pjsua.med_sock_info[med_sk_index], 
				       &offer);
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

    inv_data = pj_pool_zalloc( dlg->pool, sizeof(struct pjsua_inv_data));
    inv_data->inv = inv;
    inv_data->call_slot = med_sk_index;
    dlg->mod_data[pjsua.mod.id] = inv_data;
    inv->mod_data[pjsua.mod.id] = inv_data;


    /* Set dialog Route-Set: */

    if (!pj_list_empty(&pjsua.route_set))
	pjsip_dlg_set_route_set(dlg, &pjsua.route_set);


    /* Set credentials: */

    pjsip_auth_clt_set_credentials( &dlg->auth_sess, pjsua.cred_count, 
				    pjsua.cred_info);


    /* Create initial INVITE: */

    status = pjsip_inv_invite(inv, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create initial INVITE request", 
		     status);
	goto on_error;
    }


    /* Send initial INVITE: */

    status = pjsip_inv_send_msg(inv, tdata, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send initial INVITE request", 
		     status);
	goto on_error;
    }

    /* Add invite session to the list. */
    
    pj_list_push_back(&pjsua.inv_list, inv_data);


    /* Done. */

    *p_inv = inv;

    return PJ_SUCCESS;


on_error:

    PJ_TODO(DESTROY_DIALOG_ON_FAIL);
    pjsua.med_sock_use[med_sk_index] = 0;
    return status;
}


/**
 * Handle incoming INVITE request.
 */
pj_bool_t pjsua_inv_on_incoming(pjsip_rx_data *rdata)
{
    pjsip_dialog *dlg = pjsip_rdata_get_dlg(rdata);
    pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);
    pjsip_msg *msg = rdata->msg_info.msg;

    /*
     * Handle incoming INVITE outside dialog.
     */
    if (dlg == NULL && tsx == NULL &&
	msg->line.req.method.id == PJSIP_INVITE_METHOD)
    {
	pj_status_t status;
	pjsip_tx_data *response = NULL;
	unsigned options = 0;

	/* Verify that we can handle the request. */
	status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
					  pjsua.endpt, &response);
	if (status != PJ_SUCCESS) {

	    /*
	     * No we can't handle the incoming INVITE request.
	     */

	    if (response) {
		pjsip_response_addr res_addr;

		pjsip_get_response_addr(response->pool, rdata, &res_addr);
		pjsip_endpt_send_response(pjsua.endpt, &res_addr, response, 
					  NULL, NULL);

	    } else {

		/* Respond with 500 (Internal Server Error) */
		pjsip_endpt_respond_stateless(pjsua.endpt, rdata, 500, NULL,
					      NULL, NULL);
	    }

	} else {
	    /*
	     * Yes we can handle the incoming INVITE request.
	     */
	    pjsip_inv_session *inv;
	    struct pjsua_inv_data *inv_data;
	    pjmedia_sdp_session *answer;
	    int med_sk_index;


	    /* Find free socket. */
	    for (med_sk_index=0; med_sk_index<PJSUA_MAX_CALLS; ++med_sk_index) {
		if (!pjsua.med_sock_use[med_sk_index])
		    break;
	    }

	    if (med_sk_index == PJSUA_MAX_CALLS) {
		PJ_LOG(3,(THIS_FILE, "Error: too many calls!"));
		return PJ_TRUE;
	    }


	    pjsua.med_sock_use[med_sk_index] = 1;

	    /* Get media capability from media endpoint: */

	    status = pjmedia_endpt_create_sdp( pjsua.med_endpt, rdata->tp_info.pool,
					       1, &pjsua.med_sock_info[med_sk_index], 
					       &answer );
	    if (status != PJ_SUCCESS) {

		pjsip_endpt_respond_stateless(pjsua.endpt, rdata, 500, NULL,
					      NULL, NULL);
		pjsua.med_sock_use[med_sk_index] = 0;
		return PJ_TRUE;
	    }

	    /* Create dialog: */

	    status = pjsip_dlg_create_uas( pjsip_ua_instance(), rdata,
					   &pjsua.contact_uri, &dlg);
	    if (status != PJ_SUCCESS) {
		pjsua.med_sock_use[med_sk_index] = 0;
		return PJ_TRUE;
	    }


	    /* Create invite session: */

	    status = pjsip_inv_create_uas( dlg, rdata, answer, 0, &inv);
	    if (status != PJ_SUCCESS) {

		status = pjsip_dlg_create_response( dlg, rdata, 500, NULL,
						    &response);
		if (status == PJ_SUCCESS)
		    status = pjsip_dlg_send_response(dlg, 
						     pjsip_rdata_get_tsx(rdata),
						     response);
		pjsua.med_sock_use[med_sk_index] = 0;
		return PJ_TRUE;

	    }


	    /* Create and attach pjsua data to the dialog: */

	    inv_data = pj_pool_zalloc(dlg->pool, sizeof(struct pjsua_inv_data));
	    inv_data->inv = inv;
	    inv_data->call_slot = inv_data->call_slot = med_sk_index;
	    dlg->mod_data[pjsua.mod.id] = inv_data;
	    inv->mod_data[pjsua.mod.id] = inv_data;

	    pj_list_push_back(&pjsua.inv_list, inv_data);


	    /* Answer with 100 (using the dialog, not invite): */

	    status = pjsip_dlg_create_response(dlg, rdata, 100, NULL, &response);
	    if (status == PJ_SUCCESS)
		status = pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), response);
	}

	/* This INVITE request has been handled. */
	return PJ_TRUE;
    }

    return PJ_FALSE;
}


/*
 * This callback receives notification from invite session when the
 * session state has changed.
 */
void pjsua_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{

    /* Destroy media session when invite session is disconnected. */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
	struct pjsua_inv_data *inv_data;

	inv_data = inv->dlg->mod_data[pjsua.mod.id];

	pj_assert(inv_data != NULL);

	if (inv_data && inv_data->session) {
	    pjmedia_conf_remove_port(pjsua.mconf, inv_data->conf_slot);
	    pjmedia_session_destroy(inv_data->session);
	    pjsua.med_sock_use[inv_data->call_slot] = 0;
	    inv_data->session = NULL;

	    PJ_LOG(3,(THIS_FILE,"Media session is destroyed"));
	}

	if (inv_data) {

	    pj_list_erase(inv_data);

	}
    }

    pjsua_ui_inv_on_state_changed(inv, e);
}


/*
 * This callback is called by invite session framework when UAC session
 * has forked.
 */
void pjsua_inv_on_new_session(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);

    PJ_TODO(HANDLE_FORKED_DIALOG);
}


/*
 * Callback to be called when SDP offer/answer negotiation has just completed
 * in the session. This function will start/update media if negotiation
 * has succeeded.
 */
void pjsua_inv_on_media_update(pjsip_inv_session *inv, pj_status_t status)
{
    struct pjsua_inv_data *inv_data;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;

    if (status != PJ_SUCCESS) {

	pjsua_perror(THIS_FILE, "SDP negotiation has failed", status);
	return;

    }

    /* Destroy existing media session, if any. */

    inv_data = inv->dlg->mod_data[pjsua.mod.id];
    if (inv_data && inv_data->session) {
	pjmedia_conf_remove_port(pjsua.mconf, inv_data->conf_slot);
	pjmedia_session_destroy(inv_data->session);
	pjsua.med_sock_use[inv_data->call_slot] = 0;
	inv_data->session = NULL;
    }

    /* Get local and remote SDP */

    status = pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to retrieve currently active local SDP", 
		     status);
	return;
    }


    status = pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to retrieve currently active remote SDP", 
		     status);
	return;
    }

    /* Create new media session. 
     * The media session is active immediately.
     */

    if (!pjsua.null_audio) {
	pjmedia_port *media_port;
	pj_str_t port_name;
	char tmp[PJSIP_MAX_URL_SIZE];

	status = pjmedia_session_create( pjsua.med_endpt, 1, 
					 &pjsua.med_sock_info[inv_data->call_slot],
					 local_sdp, remote_sdp, 
					 &inv_data->session );
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create media session", 
			 status);
	    return;
	}

	pjmedia_session_get_port(inv_data->session, 0, &media_port);

	port_name.ptr = tmp;
	port_name.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
					 inv_data->inv->dlg->remote.info->uri,
					 tmp, sizeof(tmp));
	if (port_name.slen < 1) {
	    port_name = pj_str("call");
	}
	status = pjmedia_conf_add_port( pjsua.mconf, inv->pool,
					media_port, 
					&port_name,
					&inv_data->conf_slot);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create conference slot", 
			 status);
	    pjmedia_session_destroy(inv_data->session);
	    inv_data->session = NULL;
	    return;
	}

	pjmedia_conf_connect_port( pjsua.mconf, 0, inv_data->conf_slot);
	pjmedia_conf_connect_port( pjsua.mconf, inv_data->conf_slot, 0);

	PJ_LOG(3,(THIS_FILE,"Media has been started successfully"));
    }
}


/*
 * Terminate all calls.
 */
void pjsua_inv_shutdown()
{
    struct pjsua_inv_data *inv_data, *next;

    inv_data = pjsua.inv_list.next;
    while (inv_data != &pjsua.inv_list) {
	pjsip_tx_data *tdata;

	next = inv_data->next;

	if (pjsip_inv_end_session(inv_data->inv, 410, NULL, &tdata)==0)
	    pjsip_inv_send_msg(inv_data->inv, tdata, NULL);

	inv_data = next;
    }
}


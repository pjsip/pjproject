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
#include "pjsua_imp.h"

/*
 * pjsua_pres.c
 *
 * Presence related stuffs.
 */

#define THIS_FILE   "pjsua_pres.c"



/* **************************************************************************
 * THE FOLLOWING PART HANDLES SERVER SUBSCRIPTION
 * **************************************************************************
 */

/* Proto */
static pj_bool_t pres_on_rx_request(pjsip_rx_data *rdata);

/* The module instance. */
static pjsip_module mod_pjsua_pres = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-pres", 14 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &pres_on_rx_request,		/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};


/* Callback called when *server* subscription state has changed. */
static void pres_evsub_on_srv_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsua_srv_pres *uapres = pjsip_evsub_get_mod_data(sub, pjsua.mod.id);

    PJ_UNUSED_ARG(event);

    if (uapres) {
	PJ_LOG(3,(THIS_FILE, "Server subscription to %s is %s",
		  uapres->remote, pjsip_evsub_get_state_name(sub)));

	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	    pjsip_evsub_set_mod_data(sub, pjsua.mod.id, NULL);
	    pj_list_erase(uapres);
	}
    }
}

/* This is called when request is received. 
 * We need to check for incoming SUBSCRIBE request.
 */
static pj_bool_t pres_on_rx_request(pjsip_rx_data *rdata)
{
    int acc_index;
    pjsua_acc_config *acc_config;
    pjsip_method *req_method = &rdata->msg_info.msg->line.req.method;
    pjsua_srv_pres *uapres;
    pjsip_evsub *sub;
    pjsip_evsub_user pres_cb;
    pjsip_tx_data *tdata;
    pjsip_pres_status pres_status;
    pjsip_dialog *dlg;
    pj_status_t status;

    if (pjsip_method_cmp(req_method, &pjsip_subscribe_method) != 0)
	return PJ_FALSE;

    /* Incoming SUBSCRIBE: */

    /* Find which account for the incoming request. */
    acc_index = pjsua_find_account_for_incoming(rdata);
    acc_config = &pjsua.config.acc_config[acc_index];

    /* Create UAS dialog: */
    status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, 
				  &acc_config->contact,
				  &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to create UAS dialog for subscription", 
		     status);
	return PJ_FALSE;
    }

    /* Init callback: */
    pj_memset(&pres_cb, 0, sizeof(pres_cb));
    pres_cb.on_evsub_state = &pres_evsub_on_srv_state;

    /* Create server presence subscription: */
    status = pjsip_pres_create_uas( dlg, &pres_cb, rdata, &sub);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_terminate(dlg);
	pjsua_perror(THIS_FILE, "Unable to create server subscription", 
		     status);
	return PJ_FALSE;
    }

    /* Attach our data to the subscription: */
    uapres = pj_pool_alloc(dlg->pool, sizeof(pjsua_srv_pres));
    uapres->sub = sub;
    uapres->remote = pj_pool_alloc(dlg->pool, PJSIP_MAX_URL_SIZE);
    status = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, dlg->remote.info->uri,
			     uapres->remote, PJSIP_MAX_URL_SIZE);
    if (status < 1)
	pj_ansi_strcpy(uapres->remote, "<-- url is too long-->");
    else
	uapres->remote[status] = '\0';

    pjsip_evsub_set_mod_data(sub, pjsua.mod.id, uapres);

    /* Add server subscription to the list: */
    pj_list_push_back(&pjsua.acc[acc_index].pres_srv_list, uapres);


    /* Create and send 200 (OK) to the SUBSCRIBE request: */
    status = pjsip_pres_accept(sub, rdata, 200, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to accept presence subscription", 
		     status);
	pj_list_erase(uapres);
	pjsip_pres_terminate(sub, PJ_FALSE);
	return PJ_FALSE;
    }


    /* Set our online status: */
    pj_memset(&pres_status, 0, sizeof(pres_status));
    pres_status.info_cnt = 1;
    pres_status.info[0].basic_open = pjsua.acc[acc_index].online_status;
    //Both pjsua.local_uri and pjsua.contact_uri are enclosed in "<" and ">"
    //causing XML parsing to fail.
    //pres_status.info[0].contact = pjsua.local_uri;

    pjsip_pres_set_status(sub, &pres_status);

    /* Create and send the first NOTIFY to active subscription: */
    status = pjsip_pres_notify( sub, PJSIP_EVSUB_STATE_ACTIVE, NULL,
			        NULL, &tdata);
    if (status == PJ_SUCCESS)
	status = pjsip_pres_send_request( sub, tdata);

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create/send NOTIFY", 
		     status);
	pj_list_erase(uapres);
	pjsip_pres_terminate(sub, PJ_FALSE);
	return PJ_FALSE;
    }


    /* Done: */

    return PJ_TRUE;
}


/* Refresh subscription (e.g. when our online status has changed) */
static void refresh_server_subscription(int acc_index)
{
    pjsua_srv_pres *uapres;

    uapres = pjsua.acc[acc_index].pres_srv_list.next;

    while (uapres != &pjsua.acc[acc_index].pres_srv_list) {
	
	pjsip_pres_status pres_status;
	pjsip_tx_data *tdata;

	pjsip_pres_get_status(uapres->sub, &pres_status);
	if (pres_status.info[0].basic_open != pjsua.acc[acc_index].online_status) {
	    pres_status.info[0].basic_open = pjsua.acc[acc_index].online_status;
	    pjsip_pres_set_status(uapres->sub, &pres_status);

	    if (pjsua.quit_flag) {
		pj_str_t reason = { "noresource", 10 };
		if (pjsip_pres_notify(uapres->sub, 
				      PJSIP_EVSUB_STATE_TERMINATED, NULL,
				      &reason, &tdata)==PJ_SUCCESS)
		{
		    pjsip_pres_send_request(uapres->sub, tdata);
		}
	    } else {
		if (pjsip_pres_current_notify(uapres->sub, &tdata)==PJ_SUCCESS)
		    pjsip_pres_send_request(uapres->sub, tdata);
	    }
	}

	uapres = uapres->next;
    }
}



/* **************************************************************************
 * THE FOLLOWING PART HANDLES CLIENT SUBSCRIPTION
 * **************************************************************************
 */

/* Callback called when *client* subscription state has changed. */
static void pjsua_evsub_on_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsua_buddy *buddy;

    PJ_UNUSED_ARG(event);

    buddy = pjsip_evsub_get_mod_data(sub, pjsua.mod.id);
    if (buddy) {
	PJ_LOG(3,(THIS_FILE, 
		  "Presence subscription to %.*s is %s",
		  (int)pjsua.config.buddy_uri[buddy->index].slen,
		  pjsua.config.buddy_uri[buddy->index].ptr, 
		  pjsip_evsub_get_state_name(sub)));

	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	    buddy->sub = NULL;
	    buddy->status.info_cnt = 0;
	    pjsip_evsub_set_mod_data(sub, pjsua.mod.id, NULL);
	}

	/* Call callback */
	if (pjsua.cb.on_buddy_state)
	    (*pjsua.cb.on_buddy_state)(buddy->index);
    }
}

/* Callback called when we receive NOTIFY */
static void pjsua_evsub_on_rx_notify(pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body)
{
    pjsua_buddy *buddy;

    buddy = pjsip_evsub_get_mod_data(sub, pjsua.mod.id);
    if (buddy) {
	/* Update our info. */
	pjsip_pres_get_status(sub, &buddy->status);
    }

    /* The default is to send 200 response to NOTIFY.
     * Just leave it there..
     */
    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(p_st_code);
    PJ_UNUSED_ARG(p_st_text);
    PJ_UNUSED_ARG(res_hdr);
    PJ_UNUSED_ARG(p_body);
}


/* Event subscription callback. */
static pjsip_evsub_user pres_callback = 
{
    &pjsua_evsub_on_state,  

    NULL,   /* on_tsx_state: don't care about transaction state. */

    NULL,   /* on_rx_refresh: don't care about SUBSCRIBE refresh, unless 
	     * we want to authenticate 
	     */

    &pjsua_evsub_on_rx_notify,

    NULL,   /* on_client_refresh: Use default behaviour, which is to 
	     * refresh client subscription. */

    NULL,   /* on_server_timeout: Use default behaviour, which is to send 
	     * NOTIFY to terminate. 
	     */
};


/* It does what it says.. */
static void subscribe_buddy_presence(unsigned index)
{
    int acc_index;
    pjsua_acc_config *acc_config;
    pjsip_dialog *dlg;
    pjsip_tx_data *tdata;
    pj_status_t status;

    acc_index = pjsua.buddies[index].acc_index;
    acc_config = &pjsua.config.acc_config[acc_index];

    status = pjsip_dlg_create_uac( pjsip_ua_instance(), 
				   &acc_config->id,
				   &acc_config->contact,
				   &pjsua.config.buddy_uri[index],
				   NULL, &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create dialog", 
		     status);
	return;
    }

    if (acc_config->cred_count) {
	pjsip_auth_clt_set_credentials( &dlg->auth_sess, 
					acc_config->cred_count,
					acc_config->cred_info);
    }

    status = pjsip_pres_create_uac( dlg, &pres_callback, 
				    &pjsua.buddies[index].sub);
    if (status != PJ_SUCCESS) {
	pjsua.buddies[index].sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to create presence client", 
		     status);
	pjsip_dlg_terminate(dlg);
	return;
    }

    pjsip_evsub_set_mod_data(pjsua.buddies[index].sub, pjsua.mod.id,
			     &pjsua.buddies[index]);

    status = pjsip_pres_initiate(pjsua.buddies[index].sub, -1, &tdata);
    if (status != PJ_SUCCESS) {
	pjsip_pres_terminate(pjsua.buddies[index].sub, PJ_FALSE);
	pjsua.buddies[index].sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to create initial SUBSCRIBE", 
		     status);
	return;
    }

    status = pjsip_pres_send_request(pjsua.buddies[index].sub, tdata);
    if (status != PJ_SUCCESS) {
	pjsip_pres_terminate(pjsua.buddies[index].sub, PJ_FALSE);
	pjsua.buddies[index].sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to send initial SUBSCRIBE", 
		     status);
	return;
    }
}


/* It does what it says... */
static void unsubscribe_buddy_presence(unsigned index)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    if (pjsua.buddies[index].sub == NULL)
	return;

    if (pjsip_evsub_get_state(pjsua.buddies[index].sub) == 
	PJSIP_EVSUB_STATE_TERMINATED)
    {
	pjsua.buddies[index].sub = NULL;
	return;
    }

    status = pjsip_pres_initiate( pjsua.buddies[index].sub, 0, &tdata);
    if (status == PJ_SUCCESS)
	status = pjsip_pres_send_request( pjsua.buddies[index].sub, tdata );

    if (status != PJ_SUCCESS) {

	pjsip_pres_terminate(pjsua.buddies[index].sub, PJ_FALSE);
	pjsua.buddies[index].sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to unsubscribe presence", 
		     status);
    }
}


/* It does what it says.. */
static void refresh_client_subscription(void)
{
    unsigned i;

    for (i=0; i<pjsua.config.buddy_cnt; ++i) {

	if (pjsua.buddies[i].monitor && !pjsua.buddies[i].sub) {
	    subscribe_buddy_presence(i);

	} else if (!pjsua.buddies[i].monitor && pjsua.buddies[i].sub) {
	    unsubscribe_buddy_presence(i);

	}
    }
}


/*
 * Init presence
 */
pj_status_t pjsua_pres_init()
{
    pj_status_t status;

    status = pjsip_endpt_register_module( pjsua.endpt, &mod_pjsua_pres);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to register pjsua presence module", 
		     status);
    }

    return status;
}

/*
 * Get buddy count.
 */
PJ_DEF(unsigned) pjsua_get_buddy_count(void)
{
    return pjsua.config.buddy_cnt;
}


/**
 * Get buddy info.
 */
PJ_DEF(pj_status_t) pjsua_buddy_get_info(unsigned index,
					 pjsua_buddy_info *info)
{
    pjsua_buddy *buddy;

    PJ_ASSERT_RETURN(index < pjsua.config.buddy_cnt, PJ_EINVAL);

    pj_memset(info, 0, sizeof(pjsua_buddy_info));

    buddy = &pjsua.buddies[index];
    info->index = buddy->index;
    info->is_valid = pjsua.config.buddy_uri[index].slen;
    if (!info->is_valid)
	return PJ_SUCCESS;

    info->name = buddy->name;
    info->display_name = buddy->display;
    info->host = buddy->host;
    info->port = buddy->port;
    info->uri = pjsua.config.buddy_uri[index];
    
    if (buddy->sub == NULL || buddy->status.info_cnt==0) {
	info->status = PJSUA_BUDDY_STATUS_UNKNOWN;
	info->status_text = pj_str("?");
    } else if (pjsua.buddies[index].status.info[0].basic_open) {
	info->status = PJSUA_BUDDY_STATUS_ONLINE;
	info->status_text = pj_str("Online");
    } else {
	info->status = PJSUA_BUDDY_STATUS_OFFLINE;
	info->status_text = pj_str("Offline");
    }

    return PJ_SUCCESS;
}


/**
 * Add new buddy.
 */
PJ_DEF(pj_status_t) pjsua_buddy_add( const pj_str_t *uri,
				     int *buddy_index)
{
    pjsip_name_addr *url;
    pjsip_sip_uri *sip_uri;
    int index;
    pj_str_t tmp;

    PJ_ASSERT_RETURN(pjsua.config.buddy_cnt <= PJ_ARRAY_SIZE(pjsua.config.buddy_uri),
		     PJ_ETOOMANY);

    index = pjsua.config.buddy_cnt;

    /* Get name and display name for buddy */
    pj_strdup_with_null(pjsua.pool, &tmp, uri);
    url = (pjsip_name_addr*)pjsip_parse_uri(pjsua.pool, tmp.ptr, tmp.slen,
					    PJSIP_PARSE_URI_AS_NAMEADDR);

    if (url == NULL)
	return PJSIP_EINVALIDURI;

    /* Save URI */
    pjsua.config.buddy_uri[index] = tmp;

    sip_uri = (pjsip_sip_uri*) url->uri;
    pjsua.buddies[index].name = sip_uri->user;
    pjsua.buddies[index].display = url->display;
    pjsua.buddies[index].host = sip_uri->host;
    pjsua.buddies[index].port = sip_uri->port;
    if (pjsua.buddies[index].port == 0)
	pjsua.buddies[index].port = 5060;

    /* Find account for outgoing preence subscription */
    pjsua.buddies[index].acc_index = 
	pjsua_find_account_for_outgoing(&pjsua.config.buddy_uri[index]);

    if (buddy_index)
	*buddy_index = index;

    pjsua.config.buddy_cnt++;

    return PJ_SUCCESS;
}



PJ_DEF(pj_status_t) pjsua_buddy_subscribe_pres( unsigned index,
						pj_bool_t monitor)
{
    pjsua_buddy *buddy;

    PJ_ASSERT_RETURN(index < pjsua.config.buddy_cnt, PJ_EINVAL);

    buddy = &pjsua.buddies[index];
    buddy->monitor = monitor;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_acc_set_online_status( unsigned acc_index,
						 pj_bool_t is_online)
{
    PJ_ASSERT_RETURN(acc_index < pjsua.config.acc_cnt, PJ_EINVAL);
    pjsua.acc[acc_index].online_status = is_online;
    return PJ_SUCCESS;
}


/*
 * Refresh presence
 */
PJ_DEF(void) pjsua_pres_refresh(int acc_index)
{
    refresh_client_subscription();
    refresh_server_subscription(acc_index);
}


/*
 * Shutdown presence.
 */
void pjsua_pres_shutdown(void)
{
    unsigned acc_index;
    unsigned i;

    for (acc_index=0; acc_index<(int)pjsua.config.acc_cnt; ++acc_index) {
	pjsua.acc[acc_index].online_status = 0;
    }

    for (i=0; i<pjsua.config.buddy_cnt; ++i) {
	pjsua.buddies[i].monitor = 0;
    }

    for (acc_index=0; acc_index<(int)pjsua.config.acc_cnt; ++acc_index) {
	pjsua_pres_refresh(acc_index);
    }
}

/*
 * Dump presence status.
 */
void pjsua_pres_dump(pj_bool_t detail)
{
    unsigned acc_index;
    unsigned i;


    /*
     * When no detail is required, just dump number of server and client
     * subscriptions.
     */
    if (detail == PJ_FALSE) {
	
	int count = 0;

	for (acc_index=0; acc_index < (int)pjsua.config.acc_cnt; ++acc_index) {

	    if (!pj_list_empty(&pjsua.acc[acc_index].pres_srv_list)) {
		struct pjsua_srv_pres *uapres;

		uapres = pjsua.acc[acc_index].pres_srv_list.next;
		while (uapres != &pjsua.acc[acc_index].pres_srv_list) {
		    ++count;
		    uapres = uapres->next;
		}
	    }
	}

	PJ_LOG(3,(THIS_FILE, "Number of server/UAS subscriptions: %d", 
		  count));

	count = 0;

	for (i=0; i<pjsua.config.buddy_cnt; ++i) {
	    if (pjsua.buddies[i].sub) {
		++count;
	    }
	}

	PJ_LOG(3,(THIS_FILE, "Number of client/UAC subscriptions: %d", 
		  count));
	return;
    }
    

    /*
     * Dumping all server (UAS) subscriptions
     */
    PJ_LOG(3,(THIS_FILE, "Dumping pjsua server subscriptions:"));

    for (acc_index=0; acc_index < (int)pjsua.config.acc_cnt; ++acc_index) {

	PJ_LOG(3,(THIS_FILE, "  %.*s",
		  (int)pjsua.config.acc_config[acc_index].id.slen,
		  pjsua.config.acc_config[acc_index].id.ptr));

	if (pj_list_empty(&pjsua.acc[acc_index].pres_srv_list)) {

	    PJ_LOG(3,(THIS_FILE, "  - none - "));

	} else {
	    struct pjsua_srv_pres *uapres;

	    uapres = pjsua.acc[acc_index].pres_srv_list.next;
	    while (uapres != &pjsua.acc[acc_index].pres_srv_list) {
	    
		PJ_LOG(3,(THIS_FILE, "    %10s %s",
			  pjsip_evsub_get_state_name(uapres->sub),
			  uapres->remote));

		uapres = uapres->next;
	    }
	}
    }

    /*
     * Dumping all client (UAC) subscriptions
     */
    PJ_LOG(3,(THIS_FILE, "Dumping pjsua client subscriptions:"));

    if (pjsua.config.buddy_cnt == 0) {

	PJ_LOG(3,(THIS_FILE, "  - no buddy list - "));

    } else {
	for (i=0; i<pjsua.config.buddy_cnt; ++i) {

	    if (pjsua.buddies[i].sub) {
		PJ_LOG(3,(THIS_FILE, "  %10s %.*s",
			  pjsip_evsub_get_state_name(pjsua.buddies[i].sub),
			  (int)pjsua.config.buddy_uri[i].slen,
			  pjsua.config.buddy_uri[i].ptr));
	    } else {
		PJ_LOG(3,(THIS_FILE, "  %10s %.*s",
			  "(null)",
			  (int)pjsua.config.buddy_uri[i].slen,
			  pjsua.config.buddy_uri[i].ptr));
	    }
	}
    }
}


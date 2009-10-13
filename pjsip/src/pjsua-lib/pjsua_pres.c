/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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


#define THIS_FILE   "pjsua_pres.c"


static void subscribe_buddy_presence(unsigned index);


/*
 * Find buddy.
 */
static pjsua_buddy_id pjsua_find_buddy(const pjsip_uri *uri)
{
    const pjsip_sip_uri *sip_uri;
    unsigned i;

    uri = (const pjsip_uri*) pjsip_uri_get_uri((pjsip_uri*)uri);

    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri))
	return PJSUA_INVALID_ID;

    sip_uri = (const pjsip_sip_uri*) uri;

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.buddy); ++i) {
	const pjsua_buddy *b = &pjsua_var.buddy[i];

	if (!pjsua_buddy_is_valid(i))
	    continue;

	if (pj_stricmp(&sip_uri->user, &b->name)==0 &&
	    pj_stricmp(&sip_uri->host, &b->host)==0 &&
	    (sip_uri->port==(int)b->port || (sip_uri->port==0 && b->port==5060)))
	{
	    /* Match */
	    return i;
	}
    }

    return PJSUA_INVALID_ID;
}


/*
 * Get total number of buddies.
 */
PJ_DEF(unsigned) pjsua_get_buddy_count(void)
{
    return pjsua_var.buddy_cnt;
}


/*
 * Find buddy.
 */
PJ_DEF(pjsua_buddy_id) pjsua_buddy_find(const pj_str_t *uri_str)
{
    pj_str_t input;
    pj_pool_t *pool;
    pjsip_uri *uri;
    pjsua_buddy_id buddy_id;

    pool = pjsua_pool_create("buddyfind", 512, 512);
    pj_strdup_with_null(pool, &input, uri_str);

    uri = pjsip_parse_uri(pool, input.ptr, input.slen, 0);
    if (!uri)
	buddy_id = PJSUA_INVALID_ID;
    else
	buddy_id = pjsua_find_buddy(uri);

    pj_pool_release(pool);

    return buddy_id;
}


/*
 * Check if buddy ID is valid.
 */
PJ_DEF(pj_bool_t) pjsua_buddy_is_valid(pjsua_buddy_id buddy_id)
{
    return buddy_id>=0 && buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var.buddy) &&
	   pjsua_var.buddy[buddy_id].uri.slen != 0;
}


/*
 * Enum buddy IDs.
 */
PJ_DEF(pj_status_t) pjsua_enum_buddies( pjsua_buddy_id ids[],
					unsigned *count)
{
    unsigned i, c;

    PJ_ASSERT_RETURN(ids && count, PJ_EINVAL);

    PJSUA_LOCK();

    for (i=0, c=0; c<*count && i<PJ_ARRAY_SIZE(pjsua_var.buddy); ++i) {
	if (!pjsua_var.buddy[i].uri.slen)
	    continue;
	ids[c] = i;
	++c;
    }

    *count = c;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Get detailed buddy info.
 */
PJ_DEF(pj_status_t) pjsua_buddy_get_info( pjsua_buddy_id buddy_id,
					  pjsua_buddy_info *info)
{
    unsigned total=0;
    pjsua_buddy *buddy;

    PJ_ASSERT_RETURN(buddy_id>=0 && 
		       buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var.buddy), 
		     PJ_EINVAL);

    PJSUA_LOCK();

    pj_bzero(info, sizeof(pjsua_buddy_info));

    buddy = &pjsua_var.buddy[buddy_id];
    info->id = buddy->index;
    if (pjsua_var.buddy[buddy_id].uri.slen == 0) {
	PJSUA_UNLOCK();
	return PJ_SUCCESS;
    }

    /* uri */
    info->uri.ptr = info->buf_ + total;
    pj_strncpy(&info->uri, &buddy->uri, sizeof(info->buf_)-total);
    total += info->uri.slen;

    /* contact */
    info->contact.ptr = info->buf_ + total;
    pj_strncpy(&info->contact, &buddy->contact, sizeof(info->buf_)-total);
    total += info->contact.slen;

    /* Presence status */
    pj_memcpy(&info->pres_status, &buddy->status, sizeof(pjsip_pres_status));

    /* status and status text */    
    if (buddy->sub == NULL || buddy->status.info_cnt==0) {
	info->status = PJSUA_BUDDY_STATUS_UNKNOWN;
	info->status_text = pj_str("?");
    } else if (pjsua_var.buddy[buddy_id].status.info[0].basic_open) {
	info->status = PJSUA_BUDDY_STATUS_ONLINE;

	/* copy RPID information */
	info->rpid = buddy->status.info[0].rpid;

	if (info->rpid.note.slen)
	    info->status_text = info->rpid.note;
	else
	    info->status_text = pj_str("Online");

    } else {
	info->status = PJSUA_BUDDY_STATUS_OFFLINE;
	info->status_text = pj_str("Offline");
    }

    /* monitor pres */
    info->monitor_pres = buddy->monitor;

    /* subscription state and termination reason */
    if (buddy->sub) {
	info->sub_state = pjsip_evsub_get_state(buddy->sub);
	info->sub_state_name = pjsip_evsub_get_state_name(buddy->sub);
	if (info->sub_state == PJSIP_EVSUB_STATE_TERMINATED &&
	    total < sizeof(info->buf_)) 
	{
	    info->sub_term_reason.ptr = info->buf_ + total;
	    pj_strncpy(&info->sub_term_reason,
		       pjsip_evsub_get_termination_reason(buddy->sub),
		       sizeof(info->buf_) - total);
	    total += info->sub_term_reason.slen;
	} else {
	    info->sub_term_reason = pj_str("");
	}
    } else if (total < sizeof(info->buf_)) {
	info->sub_state_name = "NULL";
	info->sub_term_reason.ptr = info->buf_ + total;
	pj_strncpy(&info->sub_term_reason, &buddy->term_reason,
		   sizeof(info->buf_) - total);
	total += info->sub_term_reason.slen;
    } else {
	info->sub_state_name = "NULL";
	info->sub_term_reason = pj_str("");
    }

    PJSUA_UNLOCK();
    return PJ_SUCCESS;
}

/*
 * Set the user data associated with the buddy object.
 */
PJ_DEF(pj_status_t) pjsua_buddy_set_user_data( pjsua_buddy_id buddy_id,
					       void *user_data)
{
    PJ_ASSERT_RETURN(buddy_id>=0 && 
		       buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var.buddy),
		     PJ_EINVAL);

    PJSUA_LOCK();

    pjsua_var.buddy[buddy_id].user_data = user_data;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Get the user data associated with the budy object.
 */
PJ_DEF(void*) pjsua_buddy_get_user_data(pjsua_buddy_id buddy_id)
{
    void *user_data;

    PJ_ASSERT_RETURN(buddy_id>=0 && 
		       buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var.buddy),
		     NULL);

    PJSUA_LOCK();

    user_data = pjsua_var.buddy[buddy_id].user_data;

    PJSUA_UNLOCK();

    return user_data;
}


/*
 * Reset buddy descriptor.
 */
static void reset_buddy(pjsua_buddy_id id)
{
    pj_pool_t *pool = pjsua_var.buddy[id].pool;
    pj_bzero(&pjsua_var.buddy[id], sizeof(pjsua_var.buddy[id]));
    pjsua_var.buddy[id].pool = pool;
    pjsua_var.buddy[id].index = id;
}


/*
 * Add new buddy.
 */
PJ_DEF(pj_status_t) pjsua_buddy_add( const pjsua_buddy_config *cfg,
				     pjsua_buddy_id *p_buddy_id)
{
    pjsip_name_addr *url;
    pjsua_buddy *buddy;
    pjsip_sip_uri *sip_uri;
    int index;
    pj_str_t tmp;

    PJ_ASSERT_RETURN(pjsua_var.buddy_cnt <= 
			PJ_ARRAY_SIZE(pjsua_var.buddy),
		     PJ_ETOOMANY);

    PJSUA_LOCK();

    /* Find empty slot */
    for (index=0; index<(int)PJ_ARRAY_SIZE(pjsua_var.buddy); ++index) {
	if (pjsua_var.buddy[index].uri.slen == 0)
	    break;
    }

    /* Expect to find an empty slot */
    if (index == PJ_ARRAY_SIZE(pjsua_var.buddy)) {
	PJSUA_UNLOCK();
	/* This shouldn't happen */
	pj_assert(!"index < PJ_ARRAY_SIZE(pjsua_var.buddy)");
	return PJ_ETOOMANY;
    }

    buddy = &pjsua_var.buddy[index];

    /* Create pool for this buddy */
    if (buddy->pool) {
	pj_pool_reset(buddy->pool);
    } else {
	char name[PJ_MAX_OBJ_NAME];
	pj_ansi_snprintf(name, sizeof(name), "buddy%03d", index);
	buddy->pool = pjsua_pool_create(name, 512, 256);
    }

    /* Init buffers for presence subscription status */
    buddy->term_reason.ptr = (char*) 
			     pj_pool_alloc(buddy->pool, 
					   PJSUA_BUDDY_SUB_TERM_REASON_LEN);

    /* Get name and display name for buddy */
    pj_strdup_with_null(buddy->pool, &tmp, &cfg->uri);
    url = (pjsip_name_addr*)pjsip_parse_uri(buddy->pool, tmp.ptr, tmp.slen,
					    PJSIP_PARSE_URI_AS_NAMEADDR);

    if (url == NULL) {
	pjsua_perror(THIS_FILE, "Unable to add buddy", PJSIP_EINVALIDURI);
	pj_pool_release(buddy->pool);
	buddy->pool = NULL;
	PJSUA_UNLOCK();
	return PJSIP_EINVALIDURI;
    }

    /* Only support SIP schemes */
    if (!PJSIP_URI_SCHEME_IS_SIP(url) && !PJSIP_URI_SCHEME_IS_SIPS(url)) {
	pj_pool_release(buddy->pool);
	buddy->pool = NULL;
	PJSUA_UNLOCK();
	return PJSIP_EINVALIDSCHEME;
    }

    /* Reset buddy, to make sure everything is cleared with default
     * values
     */
    reset_buddy(index);

    /* Save URI */
    pjsua_var.buddy[index].uri = tmp;

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(url->uri);
    pjsua_var.buddy[index].name = sip_uri->user;
    pjsua_var.buddy[index].display = url->display;
    pjsua_var.buddy[index].host = sip_uri->host;
    pjsua_var.buddy[index].port = sip_uri->port;
    pjsua_var.buddy[index].monitor = cfg->subscribe;
    if (pjsua_var.buddy[index].port == 0)
	pjsua_var.buddy[index].port = 5060;

    /* Save user data */
    pjsua_var.buddy[index].user_data = (void*)cfg->user_data;

    if (p_buddy_id)
	*p_buddy_id = index;

    pjsua_var.buddy_cnt++;

    PJSUA_UNLOCK();

    pjsua_buddy_subscribe_pres(index, cfg->subscribe);

    return PJ_SUCCESS;
}


/*
 * Delete buddy.
 */
PJ_DEF(pj_status_t) pjsua_buddy_del(pjsua_buddy_id buddy_id)
{
    PJ_ASSERT_RETURN(buddy_id>=0 && 
			buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var.buddy),
		     PJ_EINVAL);

    if (pjsua_var.buddy[buddy_id].uri.slen == 0) {
	return PJ_SUCCESS;
    }

    /* Unsubscribe presence */
    pjsua_buddy_subscribe_pres(buddy_id, PJ_FALSE);

    PJSUA_LOCK();

    /* Not interested with further events for this buddy */
    if (pjsua_var.buddy[buddy_id].sub) {
	pjsip_evsub_set_mod_data(pjsua_var.buddy[buddy_id].sub, 
				 pjsua_var.mod.id, NULL);
    }

    /* Remove buddy */
    pjsua_var.buddy[buddy_id].uri.slen = 0;
    pjsua_var.buddy_cnt--;

    /* Reset buddy struct */
    reset_buddy(buddy_id);

    PJSUA_UNLOCK();
    return PJ_SUCCESS;
}


/*
 * Enable/disable buddy's presence monitoring.
 */
PJ_DEF(pj_status_t) pjsua_buddy_subscribe_pres( pjsua_buddy_id buddy_id,
						pj_bool_t subscribe)
{
    pjsua_buddy *buddy;

    PJ_ASSERT_RETURN(buddy_id>=0 && 
			buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var.buddy),
		     PJ_EINVAL);

    PJSUA_LOCK();

    buddy = &pjsua_var.buddy[buddy_id];
    buddy->monitor = subscribe;

    PJSUA_UNLOCK();

    pjsua_pres_refresh();

    return PJ_SUCCESS;
}


/*
 * Update buddy's presence.
 */
PJ_DEF(pj_status_t) pjsua_buddy_update_pres(pjsua_buddy_id buddy_id)
{
    pjsua_buddy *buddy;

    PJ_ASSERT_RETURN(buddy_id>=0 && 
			buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var.buddy),
		     PJ_EINVAL);

    PJSUA_LOCK();

    buddy = &pjsua_var.buddy[buddy_id];

    /* Return error if buddy's presence monitoring is not enabled */
    if (!buddy->monitor) {
	PJSUA_UNLOCK();
	return PJ_EINVALIDOP;
    }

    /* Ignore if presence is already active for the buddy */
    if (buddy->sub) {
	PJSUA_UNLOCK();
	return PJ_SUCCESS;
    }

    /* Initiate presence subscription */
    subscribe_buddy_presence(buddy_id);

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Dump presence subscriptions to log file.
 */
PJ_DEF(void) pjsua_pres_dump(pj_bool_t verbose)
{
    unsigned acc_id;
    unsigned i;

    
    PJSUA_LOCK();

    /*
     * When no detail is required, just dump number of server and client
     * subscriptions.
     */
    if (verbose == PJ_FALSE) {
	
	int count = 0;

	for (acc_id=0; acc_id<PJ_ARRAY_SIZE(pjsua_var.acc); ++acc_id) {

	    if (!pjsua_var.acc[acc_id].valid)
		continue;

	    if (!pj_list_empty(&pjsua_var.acc[acc_id].pres_srv_list)) {
		struct pjsua_srv_pres *uapres;

		uapres = pjsua_var.acc[acc_id].pres_srv_list.next;
		while (uapres != &pjsua_var.acc[acc_id].pres_srv_list) {
		    ++count;
		    uapres = uapres->next;
		}
	    }
	}

	PJ_LOG(3,(THIS_FILE, "Number of server/UAS subscriptions: %d", 
		  count));

	count = 0;

	for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.buddy); ++i) {
	    if (pjsua_var.buddy[i].uri.slen == 0)
		continue;
	    if (pjsua_var.buddy[i].sub) {
		++count;
	    }
	}

	PJ_LOG(3,(THIS_FILE, "Number of client/UAC subscriptions: %d", 
		  count));
	PJSUA_UNLOCK();
	return;
    }
    

    /*
     * Dumping all server (UAS) subscriptions
     */
    PJ_LOG(3,(THIS_FILE, "Dumping pjsua server subscriptions:"));

    for (acc_id=0; acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc); ++acc_id) {

	if (!pjsua_var.acc[acc_id].valid)
	    continue;

	PJ_LOG(3,(THIS_FILE, "  %.*s",
		  (int)pjsua_var.acc[acc_id].cfg.id.slen,
		  pjsua_var.acc[acc_id].cfg.id.ptr));

	if (pj_list_empty(&pjsua_var.acc[acc_id].pres_srv_list)) {

	    PJ_LOG(3,(THIS_FILE, "  - none - "));

	} else {
	    struct pjsua_srv_pres *uapres;

	    uapres = pjsua_var.acc[acc_id].pres_srv_list.next;
	    while (uapres != &pjsua_var.acc[acc_id].pres_srv_list) {
	    
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

    if (pjsua_var.buddy_cnt == 0) {

	PJ_LOG(3,(THIS_FILE, "  - no buddy list - "));

    } else {
	for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.buddy); ++i) {

	    if (pjsua_var.buddy[i].uri.slen == 0)
		continue;

	    if (pjsua_var.buddy[i].sub) {
		PJ_LOG(3,(THIS_FILE, "  %10s %.*s",
			  pjsip_evsub_get_state_name(pjsua_var.buddy[i].sub),
			  (int)pjsua_var.buddy[i].uri.slen,
			  pjsua_var.buddy[i].uri.ptr));
	    } else {
		PJ_LOG(3,(THIS_FILE, "  %10s %.*s",
			  "(null)",
			  (int)pjsua_var.buddy[i].uri.slen,
			  pjsua_var.buddy[i].uri.ptr));
	    }
	}
    }

    PJSUA_UNLOCK();
}


/***************************************************************************
 * Server subscription.
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
    pjsua_srv_pres *uapres;

    PJ_UNUSED_ARG(event);

    PJSUA_LOCK();

    uapres = (pjsua_srv_pres*) pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
    if (uapres) {
	pjsip_evsub_state state;

	PJ_LOG(4,(THIS_FILE, "Server subscription to %s is %s",
		  uapres->remote, pjsip_evsub_get_state_name(sub)));

	state = pjsip_evsub_get_state(sub);

	if (pjsua_var.ua_cfg.cb.on_srv_subscribe_state) {
	    pj_str_t from;

	    from = uapres->dlg->remote.info_str;
	    (*pjsua_var.ua_cfg.cb.on_srv_subscribe_state)(uapres->acc_id, 
							  uapres, &from,
							  state, event);
	}

	if (state == PJSIP_EVSUB_STATE_TERMINATED) {
	    pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
	    pj_list_erase(uapres);
	}
    }

    PJSUA_UNLOCK();
}

/* This is called when request is received. 
 * We need to check for incoming SUBSCRIBE request.
 */
static pj_bool_t pres_on_rx_request(pjsip_rx_data *rdata)
{
    int acc_id;
    pjsua_acc *acc;
    pj_str_t contact;
    pjsip_method *req_method = &rdata->msg_info.msg->line.req.method;
    pjsua_srv_pres *uapres;
    pjsip_evsub *sub;
    pjsip_evsub_user pres_cb;
    pjsip_dialog *dlg;
    pjsip_status_code st_code;
    pj_str_t reason;
    pjsip_expires_hdr *expires_hdr;
    pjsua_msg_data msg_data;
    pj_status_t status;

    if (pjsip_method_cmp(req_method, pjsip_get_subscribe_method()) != 0)
	return PJ_FALSE;

    /* Incoming SUBSCRIBE: */

    PJSUA_LOCK();

    /* Find which account for the incoming request. */
    acc_id = pjsua_acc_find_for_incoming(rdata);
    acc = &pjsua_var.acc[acc_id];

    PJ_LOG(4,(THIS_FILE, "Creating server subscription, using account %d",
	      acc_id));
    
    /* Create suitable Contact header */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	status = pjsua_acc_create_uas_contact(rdata->tp_info.pool, &contact,
					      acc_id, rdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", 
			 status);
	    PJSUA_UNLOCK();
	    pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 400, NULL,
					  NULL, NULL);
	    return PJ_TRUE;
	}
    }

    /* Create UAS dialog: */
    status = pjsip_dlg_create_uas(pjsip_ua_instance(), rdata, 
				  &contact, &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to create UAS dialog for subscription", 
		     status);
	PJSUA_UNLOCK();
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 400, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    /* Set credentials and preference. */
    pjsip_auth_clt_set_credentials(&dlg->auth_sess, acc->cred_cnt, acc->cred);
    pjsip_auth_clt_set_prefs(&dlg->auth_sess, &acc->cfg.auth_pref);

    /* Init callback: */
    pj_bzero(&pres_cb, sizeof(pres_cb));
    pres_cb.on_evsub_state = &pres_evsub_on_srv_state;

    /* Create server presence subscription: */
    status = pjsip_pres_create_uas( dlg, &pres_cb, rdata, &sub);
    if (status != PJ_SUCCESS) {
	int code = PJSIP_ERRNO_TO_SIP_STATUS(status);
	pjsip_tx_data *tdata;

	pjsua_perror(THIS_FILE, "Unable to create server subscription", 
		     status);

	if (code==599 || code > 699 || code < 300) {
	    code = 400;
	}

	status = pjsip_dlg_create_response(dlg, rdata, code, NULL, &tdata);
	if (status == PJ_SUCCESS) {
	    status = pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata),
					     tdata);
	}

	PJSUA_UNLOCK();
	return PJ_TRUE;
    }

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(dlg, &tp_sel);
    }

    /* Attach our data to the subscription: */
    uapres = PJ_POOL_ALLOC_T(dlg->pool, pjsua_srv_pres);
    uapres->sub = sub;
    uapres->remote = (char*) pj_pool_alloc(dlg->pool, PJSIP_MAX_URL_SIZE);
    uapres->acc_id = acc_id;
    uapres->dlg = dlg;
    status = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, dlg->remote.info->uri,
			     uapres->remote, PJSIP_MAX_URL_SIZE);
    if (status < 1)
	pj_ansi_strcpy(uapres->remote, "<-- url is too long-->");
    else
	uapres->remote[status] = '\0';

    pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, uapres);

    /* Add server subscription to the list: */
    pj_list_push_back(&pjsua_var.acc[acc_id].pres_srv_list, uapres);


    /* Capture the value of Expires header. */
    expires_hdr = (pjsip_expires_hdr*)
    		  pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_EXPIRES,
				     NULL);
    if (expires_hdr)
	uapres->expires = expires_hdr->ivalue;
    else
	uapres->expires = -1;

    st_code = (pjsip_status_code)200;
    reason = pj_str("OK");
    pjsua_msg_data_init(&msg_data);

    /* Notify application callback, if any */
    if (pjsua_var.ua_cfg.cb.on_incoming_subscribe) {
	pjsua_buddy_id buddy_id;

	buddy_id = pjsua_find_buddy(rdata->msg_info.from->uri);

	(*pjsua_var.ua_cfg.cb.on_incoming_subscribe)(acc_id, uapres, buddy_id,
						     &dlg->remote.info_str, 
						     rdata, &st_code, &reason,
						     &msg_data);
    }

    /* Handle rejection case */
    if (st_code >= 300) {
	pjsip_tx_data *tdata;

	/* Create response */
	status = pjsip_dlg_create_response(dlg, rdata, st_code, 
					   &reason, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating response",  status);
	    pj_list_erase(uapres);
	    pjsip_pres_terminate(sub, PJ_FALSE);
	    PJSUA_UNLOCK();
	    return PJ_FALSE;
	}

	/* Add header list, if any */
	pjsua_process_msg_data(tdata, &msg_data);

	/* Send the response */
	status = pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata),
					 tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error sending response",  status);
	    /* This is not fatal */
	}

	/* Terminate presence subscription */
	pj_list_erase(uapres);
	pjsip_pres_terminate(sub, PJ_FALSE);
	PJSUA_UNLOCK();
	return PJ_TRUE;
    }

    /* Create and send 2xx response to the SUBSCRIBE request: */
    status = pjsip_pres_accept(sub, rdata, st_code, &msg_data.hdr_list);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to accept presence subscription", 
		     status);
	pj_list_erase(uapres);
	pjsip_pres_terminate(sub, PJ_FALSE);
	PJSUA_UNLOCK();
	return PJ_FALSE;
    }

    /* If code is 200, send NOTIFY now */
    if (st_code == 200) {
	pjsua_pres_notify(acc_id, uapres, PJSIP_EVSUB_STATE_ACTIVE, 
			  NULL, NULL, PJ_TRUE, &msg_data);
    }

    /* Done: */

    PJSUA_UNLOCK();

    return PJ_TRUE;
}


/*
 * Send NOTIFY.
 */
PJ_DEF(pj_status_t) pjsua_pres_notify( pjsua_acc_id acc_id,
				       pjsua_srv_pres *srv_pres,
				       pjsip_evsub_state ev_state,
				       const pj_str_t *state_str,
				       const pj_str_t *reason,
				       pj_bool_t with_body,
				       const pjsua_msg_data *msg_data)
{
    pjsua_acc *acc;
    pjsip_pres_status pres_status;
    pjsua_buddy_id buddy_id;
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Check parameters */
    PJ_ASSERT_RETURN(acc_id!=-1 && srv_pres, PJ_EINVAL);

    /* Check that account ID is valid */
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);
    /* Check that account is valid */
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    PJSUA_LOCK();

    acc = &pjsua_var.acc[acc_id];

    /* Check that the server presence subscription is still valid */
    if (pj_list_find_node(&acc->pres_srv_list, srv_pres) == NULL) {
	/* Subscription has been terminated */
	PJSUA_UNLOCK();
	return PJ_EINVALIDOP;
    }

    /* Set our online status: */
    pj_bzero(&pres_status, sizeof(pres_status));
    pres_status.info_cnt = 1;
    pres_status.info[0].basic_open = acc->online_status;
    pres_status.info[0].id = acc->cfg.pidf_tuple_id;
    //Both pjsua_var.local_uri and pjsua_var.contact_uri are enclosed in "<" and ">"
    //causing XML parsing to fail.
    //pres_status.info[0].contact = pjsua_var.local_uri;
    /* add RPID information */
    pj_memcpy(&pres_status.info[0].rpid, &acc->rpid, 
	      sizeof(pjrpid_element));

    pjsip_pres_set_status(srv_pres->sub, &pres_status);

    /* Check expires value. If it's zero, send our presense state but
     * set subscription state to TERMINATED.
     */
    if (srv_pres->expires == 0)
	ev_state = PJSIP_EVSUB_STATE_TERMINATED;

    /* Create and send the NOTIFY to active subscription: */
    status = pjsip_pres_notify(srv_pres->sub, ev_state, state_str, 
			       reason, &tdata);
    if (status == PJ_SUCCESS) {
	/* Force removal of message body if msg_body==FALSE */
	if (!with_body) {
	    tdata->msg->body = NULL;
	}
	pjsua_process_msg_data(tdata, msg_data);
	status = pjsip_pres_send_request( srv_pres->sub, tdata);
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create/send NOTIFY", 
		     status);
	pj_list_erase(srv_pres);
	pjsip_pres_terminate(srv_pres->sub, PJ_FALSE);
	PJSUA_UNLOCK();
	return status;
    }


    /* Subscribe to buddy's presence if we're not subscribed */
    buddy_id = pjsua_find_buddy(srv_pres->dlg->remote.info->uri);
    if (buddy_id != PJSUA_INVALID_ID) {
	pjsua_buddy *b = &pjsua_var.buddy[buddy_id];
	if (b->monitor && b->sub == NULL) {
	    PJ_LOG(4,(THIS_FILE, "Received SUBSCRIBE from buddy %d, "
		      "activating outgoing subscription", buddy_id));
	    subscribe_buddy_presence(buddy_id);
	}
    }

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Client presence publication callback.
 */
static void publish_cb(struct pjsip_publishc_cbparam *param)
{
    pjsua_acc *acc = (pjsua_acc*) param->token;

    if (param->code/100 != 2 || param->status != PJ_SUCCESS) {

	pjsip_publishc_destroy(param->pubc);
	acc->publish_sess = NULL;

	if (param->status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(param->status, errmsg, sizeof(errmsg));
	    PJ_LOG(1,(THIS_FILE, 
		      "Client publication (PUBLISH) failed, status=%d, msg=%s",
		       param->status, errmsg));
	} else if (param->code == 412) {
	    /* 412 (Conditional Request Failed)
	     * The PUBLISH refresh has failed, retry with new one.
	     */
	    pjsua_pres_init_publish_acc(acc->index);
	    
	} else {
	    PJ_LOG(1,(THIS_FILE, 
		      "Client publication (PUBLISH) failed (%d/%.*s)",
		       param->code, (int)param->reason.slen,
		       param->reason.ptr));
	}

    } else {
	if (param->expiration < 1) {
	    /* Could happen if server "forgot" to include Expires header
	     * in the response. We will not renew, so destroy the pubc.
	     */
	    pjsip_publishc_destroy(param->pubc);
	    acc->publish_sess = NULL;
	}
    }
}


/*
 * Send PUBLISH request.
 */
static pj_status_t send_publish(int acc_id, pj_bool_t active)
{
    pjsua_acc_config *acc_cfg = &pjsua_var.acc[acc_id].cfg;
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pjsip_pres_status pres_status;
    pjsip_tx_data *tdata;
    pj_status_t status;


    /* Create PUBLISH request */
    if (active) {
	char *bpos;
	pj_str_t entity;

	status = pjsip_publishc_publish(acc->publish_sess, PJ_TRUE, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating PUBLISH request", status);
	    goto on_error;
	}

	/* Set our online status: */
	pj_bzero(&pres_status, sizeof(pres_status));
	pres_status.info_cnt = 1;
	pres_status.info[0].basic_open = acc->online_status;
	pres_status.info[0].id = acc->cfg.pidf_tuple_id;
	/* .. including RPID information */
	pj_memcpy(&pres_status.info[0].rpid, &acc->rpid, 
		  sizeof(pjrpid_element));

	/* Be careful not to send PIDF with presence entity ID containing
	 * "<" character.
	 */
	if ((bpos=pj_strchr(&acc_cfg->id, '<')) != NULL) {
	    char *epos = pj_strchr(&acc_cfg->id, '>');
	    if (epos - bpos < 2) {
		pj_assert(!"Unexpected invalid URI");
		status = PJSIP_EINVALIDURI;
		goto on_error;
	    }
	    entity.ptr = bpos+1;
	    entity.slen = epos - bpos - 1;
	} else {
	    entity = acc_cfg->id;
	}

	/* Create and add PIDF message body */
	status = pjsip_pres_create_pidf(tdata->pool, &pres_status,
					&entity, &tdata->msg->body);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating PIDF for PUBLISH request",
			 status);
	    pjsip_tx_data_dec_ref(tdata);
	    goto on_error;
	}
    } else {
	status = pjsip_publishc_unpublish(acc->publish_sess, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating PUBLISH request", status);
	    goto on_error;
	}
    }

    /* Add headers etc */
    pjsua_process_msg_data(tdata, NULL);

    /* Send the PUBLISH request */
    status = pjsip_publishc_send(acc->publish_sess, tdata);
    if (status == PJ_EPENDING) {
	PJ_LOG(3,(THIS_FILE, "Previous request is in progress, "
		  "PUBLISH request is queued"));
    } else if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error sending PUBLISH request", status);
	goto on_error;
    }

    acc->publish_state = acc->online_status;
    return PJ_SUCCESS;

on_error:
    if (acc->publish_sess) {
	pjsip_publishc_destroy(acc->publish_sess);
	acc->publish_sess = NULL;
    }
    return status;
}


/* Create client publish session */
pj_status_t pjsua_pres_init_publish_acc(int acc_id)
{
    const pj_str_t STR_PRESENCE = { "presence", 8 };
    pjsua_acc_config *acc_cfg = &pjsua_var.acc[acc_id].cfg;
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pj_status_t status;

    /* Create and init client publication session */
    if (acc_cfg->publish_enabled) {

	/* Create client publication */
	status = pjsip_publishc_create(pjsua_var.endpt, &acc_cfg->publish_opt, 
				       acc, &publish_cb,
				       &acc->publish_sess);
	if (status != PJ_SUCCESS) {
	    acc->publish_sess = NULL;
	    return status;
	}

	/* Initialize client publication */
	status = pjsip_publishc_init(acc->publish_sess, &STR_PRESENCE,
				     &acc_cfg->id, &acc_cfg->id,
				     &acc_cfg->id, 
				     PJSUA_PUBLISH_EXPIRATION);
	if (status != PJ_SUCCESS) {
	    acc->publish_sess = NULL;
	    return status;
	}

	/* Add credential for authentication */
	if (acc->cred_cnt) {
	    pjsip_publishc_set_credentials(acc->publish_sess, acc->cred_cnt, 
					   acc->cred);
	}

	/* Set route-set */
	pjsip_publishc_set_route_set(acc->publish_sess, &acc->route_set);

	/* Send initial PUBLISH request */
	if (acc->online_status != 0) {
	    status = send_publish(acc_id, PJ_TRUE);
	    if (status != PJ_SUCCESS)
		return status;
	}

    } else {
	acc->publish_sess = NULL;
    }

    return PJ_SUCCESS;
}


/* Init presence for account */
pj_status_t pjsua_pres_init_acc(int acc_id)
{
    pjsua_acc *acc = &pjsua_var.acc[acc_id];

    /* Init presence subscription */
    pj_list_init(&acc->pres_srv_list);

    return PJ_SUCCESS;
}


/* Terminate server subscription for the account */
void pjsua_pres_delete_acc(int acc_id)
{
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pjsua_acc_config *acc_cfg = &pjsua_var.acc[acc_id].cfg;
    pjsua_srv_pres *uapres;

    uapres = pjsua_var.acc[acc_id].pres_srv_list.next;

    /* Notify all subscribers that we're no longer available */
    while (uapres != &acc->pres_srv_list) {
	
	pjsip_pres_status pres_status;
	pj_str_t reason = { "noresource", 10 };
	pjsua_srv_pres *next;
	pjsip_tx_data *tdata;

	next = uapres->next;

	pjsip_pres_get_status(uapres->sub, &pres_status);
	
	pres_status.info[0].basic_open = pjsua_var.acc[acc_id].online_status;
	pjsip_pres_set_status(uapres->sub, &pres_status);

	if (pjsip_pres_notify(uapres->sub, 
			      PJSIP_EVSUB_STATE_TERMINATED, NULL,
			      &reason, &tdata)==PJ_SUCCESS)
	{
	    pjsip_pres_send_request(uapres->sub, tdata);
	}

	uapres = next;
    }

    /* Clear server presence subscription list because account might be reused
     * later. */
    pj_list_init(&acc->pres_srv_list);

    /* Terminate presence publication, if any */
    if (acc->publish_sess) {
	acc->online_status = PJ_FALSE;
	send_publish(acc_id, PJ_FALSE);
	/* By ticket #364, don't destroy the session yet (let the callback
	   destroy it)
	if (acc->publish_sess) {
	    pjsip_publishc_destroy(acc->publish_sess);
	    acc->publish_sess = NULL;
	}
	*/
	acc_cfg->publish_enabled = PJ_FALSE;
    }
}


/* Update server subscription (e.g. when our online status has changed) */
void pjsua_pres_update_acc(int acc_id, pj_bool_t force)
{
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pjsua_acc_config *acc_cfg = &pjsua_var.acc[acc_id].cfg;
    pjsua_srv_pres *uapres;

    uapres = pjsua_var.acc[acc_id].pres_srv_list.next;

    while (uapres != &acc->pres_srv_list) {
	
	pjsip_pres_status pres_status;
	pjsip_tx_data *tdata;

	pjsip_pres_get_status(uapres->sub, &pres_status);

	/* Only send NOTIFY once subscription is active. Some subscriptions
	 * may still be in NULL (when app is adding a new buddy while in the
	 * on_incoming_subscribe() callback) or PENDING (when user approval is
	 * being requested) state and we don't send NOTIFY to these subs until
	 * the user accepted the request.
	 */
	if (pjsip_evsub_get_state(uapres->sub)==PJSIP_EVSUB_STATE_ACTIVE &&
	    (force || pres_status.info[0].basic_open != acc->online_status)) 
	{

	    pres_status.info[0].basic_open = acc->online_status;
	    pj_memcpy(&pres_status.info[0].rpid, &acc->rpid, 
		      sizeof(pjrpid_element));

	    pjsip_pres_set_status(uapres->sub, &pres_status);

	    if (pjsip_pres_current_notify(uapres->sub, &tdata)==PJ_SUCCESS) {
		pjsua_process_msg_data(tdata, NULL);
		pjsip_pres_send_request(uapres->sub, tdata);
	    }
	}

	uapres = uapres->next;
    }

    /* Send PUBLISH if required. We only do this when we have a PUBLISH
     * session. If we don't have a PUBLISH session, then it could be
     * that we're waiting until registration has completed before we
     * send the first PUBLISH. 
     */
    if (acc_cfg->publish_enabled && acc->publish_sess) {
	if (force || acc->publish_state != acc->online_status) {
	    send_publish(acc_id, PJ_TRUE);
	}
    }
}



/***************************************************************************
 * Client subscription.
 */

/* Callback called when *client* subscription state has changed. */
static void pjsua_evsub_on_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsua_buddy *buddy;

    PJ_UNUSED_ARG(event);

    PJSUA_LOCK();

    buddy = (pjsua_buddy*) pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
    if (buddy) {
	PJ_LOG(4,(THIS_FILE, 
		  "Presence subscription to %.*s is %s",
		  (int)pjsua_var.buddy[buddy->index].uri.slen,
		  pjsua_var.buddy[buddy->index].uri.ptr, 
		  pjsip_evsub_get_state_name(sub)));

	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	    if (buddy->term_reason.ptr == NULL) {
		buddy->term_reason.ptr = (char*) 
					 pj_pool_alloc(buddy->pool,
					   PJSUA_BUDDY_SUB_TERM_REASON_LEN);
	    }
	    pj_strncpy(&buddy->term_reason, 
		       pjsip_evsub_get_termination_reason(sub), 
		       PJSUA_BUDDY_SUB_TERM_REASON_LEN);
	} else {
	    buddy->term_reason.slen = 0;
	}

	/* Call callback */
	if (pjsua_var.ua_cfg.cb.on_buddy_state)
	    (*pjsua_var.ua_cfg.cb.on_buddy_state)(buddy->index);

	/* Clear subscription */
	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	    buddy->sub = NULL;
	    buddy->status.info_cnt = 0;
	    pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
	}
    }

    PJSUA_UNLOCK();
}


/* Callback when transaction state has changed. */
static void pjsua_evsub_on_tsx_state(pjsip_evsub *sub, 
				     pjsip_transaction *tsx,
				     pjsip_event *event)
{
    pjsua_buddy *buddy;
    pjsip_contact_hdr *contact_hdr;

    PJSUA_LOCK();

    buddy = (pjsua_buddy*) pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
    if (!buddy) {
	PJSUA_UNLOCK();
	return;
    }

    /* We only use this to update buddy's Contact, when it's not
     * set.
     */
    if (buddy->contact.slen != 0) {
	/* Contact already set */
	PJSUA_UNLOCK();
	return;
    }
    
    /* Only care about 2xx response to outgoing SUBSCRIBE */
    if (tsx->status_code/100 != 2 ||
	tsx->role != PJSIP_UAC_ROLE ||
	event->type != PJSIP_EVENT_RX_MSG || 
	pjsip_method_cmp(&tsx->method, pjsip_get_subscribe_method())!=0)
    {
	PJSUA_UNLOCK();
	return;
    }

    /* Find contact header. */
    contact_hdr = (pjsip_contact_hdr*)
		  pjsip_msg_find_hdr(event->body.rx_msg.rdata->msg_info.msg,
				     PJSIP_H_CONTACT, NULL);
    if (!contact_hdr) {
	PJSUA_UNLOCK();
	return;
    }

    buddy->contact.ptr = (char*)
			 pj_pool_alloc(buddy->pool, PJSIP_MAX_URL_SIZE);
    buddy->contact.slen = pjsip_uri_print( PJSIP_URI_IN_CONTACT_HDR,
					   contact_hdr->uri,
					   buddy->contact.ptr, 
					   PJSIP_MAX_URL_SIZE);
    if (buddy->contact.slen < 0)
	buddy->contact.slen = 0;

    PJSUA_UNLOCK();
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

    PJSUA_LOCK();

    buddy = (pjsua_buddy*) pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
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

    PJSUA_UNLOCK();
}


/* Event subscription callback. */
static pjsip_evsub_user pres_callback = 
{
    &pjsua_evsub_on_state,  
    &pjsua_evsub_on_tsx_state,

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
    pj_pool_t *tmp_pool = NULL;
    pjsua_buddy *buddy;
    int acc_id;
    pjsua_acc *acc;
    pj_str_t contact;
    pjsip_tx_data *tdata;
    pj_status_t status;

    buddy = &pjsua_var.buddy[index];
    acc_id = pjsua_acc_find_for_outgoing(&buddy->uri);

    acc = &pjsua_var.acc[acc_id];

    PJ_LOG(4,(THIS_FILE, "Using account %d for buddy %d subscription",
			 acc_id, index));

    /* Generate suitable Contact header unless one is already set in
     * the account
     */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	tmp_pool = pjsua_pool_create("tmpbuddy", 512, 256);

	status = pjsua_acc_create_uac_contact(tmp_pool, &contact,
					      acc_id, &buddy->uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", 
		         status);
	    pj_pool_release(tmp_pool);
	    return;
	}
    }

    /* Create UAC dialog */
    status = pjsip_dlg_create_uac( pjsip_ua_instance(), 
				   &acc->cfg.id,
				   &contact,
				   &buddy->uri,
				   NULL, &buddy->dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create dialog", 
		     status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    /* Increment the dialog's lock otherwise when presence session creation
     * fails the dialog will be destroyed prematurely.
     */
    pjsip_dlg_inc_lock(buddy->dlg);

    status = pjsip_pres_create_uac( buddy->dlg, &pres_callback, 
				    PJSIP_EVSUB_NO_EVENT_ID, &buddy->sub);
    if (status != PJ_SUCCESS) {
	pjsua_var.buddy[index].sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to create presence client", 
		     status);
	/* This should destroy the dialog since there's no session
	 * referencing it
	 */
	pjsip_dlg_dec_lock(buddy->dlg);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(buddy->dlg, &tp_sel);
    }

    /* Set route-set */
    if (!pj_list_empty(&acc->route_set)) {
	pjsip_dlg_set_route_set(buddy->dlg, &acc->route_set);
    }

    /* Set credentials */
    if (acc->cred_cnt) {
	pjsip_auth_clt_set_credentials( &buddy->dlg->auth_sess, 
					acc->cred_cnt, acc->cred);
    }

    /* Set authentication preference */
    pjsip_auth_clt_set_prefs(&buddy->dlg->auth_sess, &acc->cfg.auth_pref);

    pjsip_evsub_set_mod_data(buddy->sub, pjsua_var.mod.id, buddy);

    status = pjsip_pres_initiate(buddy->sub, -1, &tdata);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(buddy->dlg);
	if (buddy->sub) {
	    pjsip_pres_terminate(buddy->sub, PJ_FALSE);
	}
	buddy->sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to create initial SUBSCRIBE", 
		     status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    pjsua_process_msg_data(tdata, NULL);

    status = pjsip_pres_send_request(buddy->sub, tdata);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(buddy->dlg);
	if (buddy->sub) {
	    pjsip_pres_terminate(buddy->sub, PJ_FALSE);
	}
	buddy->sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to send initial SUBSCRIBE", 
		     status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    pjsip_dlg_dec_lock(buddy->dlg);
    if (tmp_pool) pj_pool_release(tmp_pool);
}


/* It does what it says... */
static void unsubscribe_buddy_presence(unsigned index)
{
    pjsua_buddy *buddy;
    pjsip_tx_data *tdata;
    pj_status_t status;

    buddy = &pjsua_var.buddy[index];

    if (buddy->sub == NULL)
	return;

    if (pjsip_evsub_get_state(buddy->sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	pjsua_var.buddy[index].sub = NULL;
	return;
    }

    status = pjsip_pres_initiate( buddy->sub, 0, &tdata);
    if (status == PJ_SUCCESS) {
	pjsua_process_msg_data(tdata, NULL);
	status = pjsip_pres_send_request( buddy->sub, tdata );
    }

    if (status != PJ_SUCCESS && buddy->sub) {
	pjsip_pres_terminate(buddy->sub, PJ_FALSE);
	buddy->sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to unsubscribe presence", 
		     status);
    }
}


/* Lock all buddies */
#define LOCK_BUDDIES	unsigned cnt_ = 0; \
			pjsip_dialog *dlg_list_[PJSUA_MAX_BUDDIES]; \
			unsigned i_; \
			for (i_=0; i_<PJ_ARRAY_SIZE(pjsua_var.buddy);++i_) { \
			    if (pjsua_var.buddy[i_].sub) { \
				dlg_list_[cnt_++] = pjsua_var.buddy[i_].dlg; \
				pjsip_dlg_inc_lock(pjsua_var.buddy[i_].dlg); \
			    } \
			} \
			PJSUA_LOCK();

/* Unlock all buddies */
#define UNLOCK_BUDDIES	PJSUA_UNLOCK(); \
			for (i_=0; i_<cnt_; ++i_) { \
			    pjsip_dlg_dec_lock(dlg_list_[i_]); \
			}
			


/* It does what it says.. */
static void refresh_client_subscriptions(void)
{
    unsigned i;

    LOCK_BUDDIES;

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.buddy); ++i) {

	if (!pjsua_var.buddy[i].uri.slen)
	    continue;

	if (pjsua_var.buddy[i].monitor && !pjsua_var.buddy[i].sub) {
	    subscribe_buddy_presence(i);

	} else if (!pjsua_var.buddy[i].monitor && pjsua_var.buddy[i].sub) {
	    unsubscribe_buddy_presence(i);

	}
    }

    UNLOCK_BUDDIES;
}

/* Timer callback to re-create client subscription */
static void pres_timer_cb(pj_timer_heap_t *th,
			  pj_timer_entry *entry)
{
    unsigned i;
    pj_time_val delay = { PJSUA_PRES_TIMER, 0 };

    /* Retry failed PUBLISH requests */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	pjsua_acc *acc = &pjsua_var.acc[i];
	if (acc->cfg.publish_enabled && acc->publish_sess==NULL)
	    pjsua_pres_init_publish_acc(acc->index);
    }

    entry->id = PJ_FALSE;
    refresh_client_subscriptions();

    pjsip_endpt_schedule_timer(pjsua_var.endpt, entry, &delay);
    entry->id = PJ_TRUE;

    PJ_UNUSED_ARG(th);
}


/*
 * Init presence
 */
pj_status_t pjsua_pres_init()
{
    unsigned i;
    pj_status_t status;

    status = pjsip_endpt_register_module( pjsua_var.endpt, &mod_pjsua_pres);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to register pjsua presence module", 
		     status);
    }

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.buddy); ++i) {
	reset_buddy(i);
    }

    return status;
}


/*
 * Start presence subsystem.
 */
pj_status_t pjsua_pres_start(void)
{
    /* Start presence timer to re-subscribe to buddy's presence when
     * subscription has failed.
     */
    if (pjsua_var.pres_timer.id == PJ_FALSE) {
	pj_time_val pres_interval = {PJSUA_PRES_TIMER, 0};

	pjsua_var.pres_timer.cb = &pres_timer_cb;
	pjsip_endpt_schedule_timer(pjsua_var.endpt, &pjsua_var.pres_timer,
				   &pres_interval);
	pjsua_var.pres_timer.id = PJ_TRUE;
    }

    return PJ_SUCCESS;
}


/*
 * Refresh presence subscriptions
 */
void pjsua_pres_refresh()
{
    unsigned i;

    refresh_client_subscriptions();

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	if (pjsua_var.acc[i].valid)
	    pjsua_pres_update_acc(i, PJ_FALSE);
    }
}


/*
 * Shutdown presence.
 */
void pjsua_pres_shutdown(void)
{
    unsigned i;

    if (pjsua_var.pres_timer.id != 0) {
	pjsip_endpt_cancel_timer(pjsua_var.endpt, &pjsua_var.pres_timer);
	pjsua_var.pres_timer.id = PJ_FALSE;
    }

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	if (!pjsua_var.acc[i].valid)
	    continue;
	pjsua_pres_delete_acc(i);
    }

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.buddy); ++i) {
	pjsua_var.buddy[i].monitor = 0;
    }

    pjsua_pres_refresh();
}

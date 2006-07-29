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


#define THIS_FILE		"pjsua_acc.c"


/*
 * Get number of current accounts.
 */
PJ_DEF(unsigned) pjsua_acc_get_count(void)
{
    return pjsua_var.acc_cnt;
}


/*
 * Check if the specified account ID is valid.
 */
PJ_DEF(pj_bool_t) pjsua_acc_is_valid(pjsua_acc_id acc_id)
{
    return acc_id>=0 && acc_id<PJ_ARRAY_SIZE(pjsua_var.acc) &&
	   pjsua_var.acc[acc_id].valid;
}


/*
 * Copy account configuration.
 */
static void copy_acc_config(pj_pool_t *pool,
			    pjsua_acc_config *dst,
			    const pjsua_acc_config *src)
{
    unsigned i;

    pj_memcpy(dst, src, sizeof(pjsua_acc_config));

    pj_strdup_with_null(pool, &dst->id, &src->id);
    pj_strdup_with_null(pool, &dst->reg_uri, &src->reg_uri);
    pj_strdup_with_null(pool, &dst->force_contact, &src->force_contact);

    dst->proxy_cnt = src->proxy_cnt;
    for (i=0; i<src->proxy_cnt; ++i)
	pj_strdup_with_null(pool, &dst->proxy[i], &src->proxy[i]);

    dst->reg_timeout = src->reg_timeout;
    dst->cred_count = src->cred_count;

    for (i=0; i<src->cred_count; ++i) {
	pjsip_cred_dup(pool, &dst->cred_info[i], &src->cred_info[i]);
    }
}


/*
 * Initialize a new account (after configuration is set).
 */
static pj_status_t initialize_acc(unsigned acc_id)
{
    pjsua_acc_config *acc_cfg = &pjsua_var.acc[acc_id].cfg;
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pjsip_name_addr *name_addr;
    pjsip_sip_uri *sip_uri, *sip_reg_uri;
    unsigned i;

    /* Need to parse local_uri to get the elements: */

    name_addr = (pjsip_name_addr*)
		    pjsip_parse_uri(pjsua_var.pool, acc_cfg->id.ptr,
				    acc_cfg->id.slen, 
				    PJSIP_PARSE_URI_AS_NAMEADDR);
    if (name_addr == NULL) {
	pjsua_perror(THIS_FILE, "Invalid local URI", 
		     PJSIP_EINVALIDURI);
	return PJSIP_EINVALIDURI;
    }

    /* Local URI MUST be a SIP or SIPS: */

    if (!PJSIP_URI_SCHEME_IS_SIP(name_addr) && 
	!PJSIP_URI_SCHEME_IS_SIPS(name_addr)) 
    {
	pjsua_perror(THIS_FILE, "Invalid local URI", 
		     PJSIP_EINVALIDSCHEME);
	return PJSIP_EINVALIDSCHEME;
    }


    /* Get the SIP URI object: */
    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(name_addr);


    /* Parse registrar URI, if any */
    if (acc_cfg->reg_uri.slen) {
	pjsip_uri *reg_uri;

	reg_uri = pjsip_parse_uri(pjsua_var.pool, acc_cfg->reg_uri.ptr,
				  acc_cfg->reg_uri.slen, 0);
	if (reg_uri == NULL) {
	    pjsua_perror(THIS_FILE, "Invalid registrar URI", 
			 PJSIP_EINVALIDURI);
	    return PJSIP_EINVALIDURI;
	}

	/* Registrar URI MUST be a SIP or SIPS: */
	if (!PJSIP_URI_SCHEME_IS_SIP(reg_uri) && 
	    !PJSIP_URI_SCHEME_IS_SIPS(reg_uri)) 
	{
	    pjsua_perror(THIS_FILE, "Invalid registar URI", 
			 PJSIP_EINVALIDSCHEME);
	    return PJSIP_EINVALIDSCHEME;
	}

	sip_reg_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(reg_uri);

    } else {
	sip_reg_uri = NULL;
    }


    /* Save the user and domain part. These will be used when finding an 
     * account for incoming requests.
     */
    acc->display = name_addr->display;
    acc->user_part = sip_uri->user;
    acc->srv_domain = sip_uri->host;
    acc->srv_port = 0;

    if (sip_reg_uri) {
	acc->srv_port = sip_reg_uri->port;
    }

    /* Create Contact header if not present. */
    //if (acc_cfg->contact.slen == 0) {
    //	acc_cfg->contact = acc_cfg->id;
    //}

    /* Build account route-set from outbound proxies and route set from 
     * account configuration.
     */
    pj_list_init(&acc->route_set);

    for (i=0; i<pjsua_var.ua_cfg.outbound_proxy_cnt; ++i) {
    	pj_str_t hname = { "Route", 5};
	pjsip_route_hdr *r;
	pj_str_t tmp;

	pj_strdup_with_null(pjsua_var.pool, &tmp, 
			    &pjsua_var.ua_cfg.outbound_proxy[i]);
	r = pjsip_parse_hdr(pjsua_var.pool, &hname, tmp.ptr, tmp.slen, NULL);
	if (r == NULL) {
	    pjsua_perror(THIS_FILE, "Invalid outbound proxy URI",
			 PJSIP_EINVALIDURI);
	    return PJSIP_EINVALIDURI;
	}
	pj_list_push_back(&acc->route_set, r);
    }

    for (i=0; i<acc_cfg->proxy_cnt; ++i) {
    	pj_str_t hname = { "Route", 5};
	pjsip_route_hdr *r;
	pj_str_t tmp;

	pj_strdup_with_null(pjsua_var.pool, &tmp, &acc_cfg->proxy[i]);
	r = pjsip_parse_hdr(pjsua_var.pool, &hname, tmp.ptr, tmp.slen, NULL);
	if (r == NULL) {
	    pjsua_perror(THIS_FILE, "Invalid URI in account route set",
			 PJ_EINVAL);
	    return PJ_EINVAL;
	}
	pj_list_push_back(&acc->route_set, r);
    }

    
    /* Concatenate credentials from account config and global config */
    acc->cred_cnt = 0;
    for (i=0; i<acc_cfg->cred_count; ++i) {
	acc->cred[acc->cred_cnt++] = acc_cfg->cred_info[i];
    }
    for (i=0; i<pjsua_var.ua_cfg.cred_count && 
	      acc->cred_cnt < PJ_ARRAY_SIZE(acc->cred); ++i)
    {
	acc->cred[acc->cred_cnt++] = pjsua_var.ua_cfg.cred_info[i];
    }

    /* Init presence subscription */
    pj_list_init(&acc->pres_srv_list);

    /* Mark account as valid */
    pjsua_var.acc[acc_id].valid = PJ_TRUE;


    return PJ_SUCCESS;
}


/*
 * Add a new account to pjsua.
 */
PJ_DEF(pj_status_t) pjsua_acc_add( const pjsua_acc_config *cfg,
				   pj_bool_t is_default,
				   pjsua_acc_id *p_acc_id)
{
    unsigned id;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_var.acc_cnt < PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_ETOOMANY);

    /* Must have a transport */
    PJ_TODO(associate_acc_with_transport);
    PJ_ASSERT_RETURN(pjsua_var.tpdata[0].data.ptr != NULL, PJ_EINVALIDOP);

    PJSUA_LOCK();

    /* Find empty account id. */
    for (id=0; id < PJ_ARRAY_SIZE(pjsua_var.acc); ++id) {
	if (pjsua_var.acc[id].valid == PJ_FALSE)
	    break;
    }

    /* Expect to find a slot */
    PJ_ASSERT_ON_FAIL(	id < PJ_ARRAY_SIZE(pjsua_var.acc), 
			{PJSUA_UNLOCK(); return PJ_EBUG;});

    /* Copy config */
    copy_acc_config(pjsua_var.pool, &pjsua_var.acc[id].cfg, cfg);
    
    /* Normalize registration timeout */
    if (pjsua_var.acc[id].cfg.reg_uri.slen &&
	pjsua_var.acc[id].cfg.reg_timeout == 0)
    {
	pjsua_var.acc[id].cfg.reg_timeout = PJSUA_REG_INTERVAL;
    }

    status = initialize_acc(id);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error adding account", status);
	PJSUA_UNLOCK();
	return status;
    }

    if (is_default)
	pjsua_var.default_acc = id;

    if (p_acc_id)
	*p_acc_id = id;

    pjsua_var.acc_cnt++;

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Account %.*s added with id %d",
	      (int)cfg->id.slen, cfg->id.ptr, id));

    /* If accounts has registration enabled, start registration */
    if (pjsua_var.acc[id].cfg.reg_uri.slen)
	pjsua_acc_set_registration(id, PJ_TRUE);


    return PJ_SUCCESS;
}


/*
 * Add local account
 */
PJ_DEF(pj_status_t) pjsua_acc_add_local( pjsua_transport_id tid,
					 pj_bool_t is_default,
					 pjsua_acc_id *p_acc_id)
{
    pjsua_acc_config cfg;
    struct transport_data *t = &pjsua_var.tpdata[tid];
    char uri[PJSIP_MAX_URL_SIZE];

    /* ID must be valid */
    PJ_ASSERT_RETURN(tid>=0 && tid<PJ_ARRAY_SIZE(pjsua_var.tpdata), PJ_EINVAL);

    /* Transport must be valid */
    PJ_ASSERT_RETURN(t->data.ptr != NULL, PJ_EINVAL);
    
    pjsua_acc_config_default(&cfg);

    /* Build URI for the account */
    pj_ansi_snprintf(uri, PJSIP_MAX_URL_SIZE,
		     "<sip:%.*s:%d;transport=%s>", 
		     (int)t->local_name.host.slen,
		     t->local_name.host.ptr,
		     t->local_name.port,
		     pjsip_transport_get_type_name(t->type));

    cfg.id = pj_str(uri);
    
    return pjsua_acc_add(&cfg, is_default, p_acc_id);
}


/*
 * Delete account.
 */
PJ_DEF(pj_status_t) pjsua_acc_del(pjsua_acc_id acc_id)
{
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    PJSUA_LOCK();

    /* Delete registration */
    if (pjsua_var.acc[acc_id].regc != NULL) 
	pjsua_acc_set_registration(acc_id, PJ_FALSE);

    /* Delete server presence subscription */
    pjsua_pres_delete_acc(acc_id);

    /* Invalidate */
    pjsua_var.acc[acc_id].valid = PJ_FALSE;

    PJ_TODO(may_need_to_scan_calls);

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Account id %d deleted", acc_id));

    return PJ_SUCCESS;
}


/*
 * Modify account information.
 */
PJ_DEF(pj_status_t) pjsua_acc_modify( pjsua_acc_id acc_id,
				      const pjsua_acc_config *cfg)
{
    PJ_TODO(pjsua_acc_modify);
    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(cfg);
    return PJ_EINVALIDOP;
}


/*
 * Modify account's presence status to be advertised to remote/presence
 * subscribers.
 */
PJ_DEF(pj_status_t) pjsua_acc_set_online_status( pjsua_acc_id acc_id,
						 pj_bool_t is_online)
{
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    pjsua_var.acc[acc_id].online_status = is_online;
    pjsua_pres_refresh();
    return PJ_SUCCESS;
}


/*
 * This callback is called by pjsip_regc when outgoing register
 * request has completed.
 */
static void regc_cb(struct pjsip_regc_cbparam *param)
{

    pjsua_acc *acc = param->token;

    PJSUA_LOCK();

    /*
     * Print registration status.
     */
    if (param->status!=PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "SIP registration error", 
		     param->status);
	pjsip_regc_destroy(acc->regc);
	acc->regc = NULL;
	
    } else if (param->code < 0 || param->code >= 300) {
	PJ_LOG(2, (THIS_FILE, "SIP registration failed, status=%d (%.*s)", 
		   param->code, 
		   (int)param->reason.slen, param->reason.ptr));
	pjsip_regc_destroy(acc->regc);
	acc->regc = NULL;

    } else if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {

	if (param->expiration < 1) {
	    pjsip_regc_destroy(acc->regc);
	    acc->regc = NULL;
	    PJ_LOG(3,(THIS_FILE, "%s: unregistration success",
		      pjsua_var.acc[acc->index].cfg.id.ptr));
	} else {
	    PJ_LOG(3, (THIS_FILE, 
		       "%s: registration success, status=%d (%.*s), "
		       "will re-register in %d seconds", 
		       pjsua_var.acc[acc->index].cfg.id.ptr,
		       param->code,
		       (int)param->reason.slen, param->reason.ptr,
		       param->expiration));
	}

    } else {
	PJ_LOG(4, (THIS_FILE, "SIP registration updated status=%d", param->code));
    }

    acc->reg_last_err = param->status;
    acc->reg_last_code = param->code;

    if (pjsua_var.ua_cfg.cb.on_reg_state)
	(*pjsua_var.ua_cfg.cb.on_reg_state)(acc->index);

    PJSUA_UNLOCK();
}


/*
 * Initialize client registration.
 */
static pj_status_t pjsua_regc_init(int acc_id)
{
    pjsua_acc *acc;
    pj_str_t contact;
    pj_status_t status;

    acc = &pjsua_var.acc[acc_id];

    if (acc->cfg.reg_uri.slen == 0) {
	PJ_LOG(3,(THIS_FILE, "Registrar URI is not specified"));
	return PJ_SUCCESS;
    }

    /* initialize SIP registration if registrar is configured */

    status = pjsip_regc_create( pjsua_var.endpt, 
				acc, &regc_cb, &acc->regc);

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create client registration", 
		     status);
	return status;
    }

    status = pjsua_acc_create_uac_contact( pjsua_var.pool, &contact,
					   acc_id, &acc->cfg.reg_uri);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to generate suitable Contact header"
				" for registration", 
		     status);
	return status;
    }

    status = pjsip_regc_init( acc->regc,
			      &acc->cfg.reg_uri, 
			      &acc->cfg.id, 
			      &acc->cfg.id,
			      1, &contact, 
			      acc->cfg.reg_timeout);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Client registration initialization error", 
		     status);
	return status;
    }

    /* Set credentials
     */
    if (acc->cred_cnt) {
	pjsip_regc_set_credentials( acc->regc, acc->cred_cnt, acc->cred);
    }

    /* Set route-set
     */
    if (!pj_list_empty(&acc->route_set)) {
	pjsip_regc_set_route_set( acc->regc, &acc->route_set );
    }

    return PJ_SUCCESS;
}


/*
 * Update registration or perform unregistration. 
 */
PJ_DEF(pj_status_t) pjsua_acc_set_registration( pjsua_acc_id acc_id, 
						pj_bool_t renew)
{
    pj_status_t status = 0;
    pjsip_tx_data *tdata = 0;

    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    PJSUA_LOCK();

    if (renew) {
	if (pjsua_var.acc[acc_id].regc == NULL) {
	    // Need route set.
	    status = pjsua_regc_init(acc_id);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to create registration", 
			     status);
		goto on_return;
	    }
	}
	if (!pjsua_var.acc[acc_id].regc) {
	    status = PJ_EINVALIDOP;
	    goto on_return;
	}

	status = pjsip_regc_register(pjsua_var.acc[acc_id].regc, 1, 
				     &tdata);

    } else {
	if (pjsua_var.acc[acc_id].regc == NULL) {
	    PJ_LOG(3,(THIS_FILE, "Currently not registered"));
	    status = PJ_EINVALIDOP;
	    goto on_return;
	}
	status = pjsip_regc_unregister(pjsua_var.acc[acc_id].regc, &tdata);
    }

    if (status == PJ_SUCCESS) {
	pjsua_process_msg_data(tdata, NULL);
	status = pjsip_regc_send( pjsua_var.acc[acc_id].regc, tdata );
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create/send REGISTER", 
		     status);
    } else {
	PJ_LOG(3,(THIS_FILE, "%s sent",
	         (renew? "Registration" : "Unregistration")));
    }

on_return:
    PJSUA_UNLOCK();
    return status;
}


/*
 * Get account information.
 */
PJ_DEF(pj_status_t) pjsua_acc_get_info( pjsua_acc_id acc_id,
					pjsua_acc_info *info)
{
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pjsua_acc_config *acc_cfg = &pjsua_var.acc[acc_id].cfg;

    PJ_ASSERT_RETURN(info != NULL, PJ_EINVAL);
    
    pj_bzero(info, sizeof(pjsua_acc_info));

    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc), 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    PJSUA_LOCK();
    
    if (pjsua_var.acc[acc_id].valid == PJ_FALSE) {
	PJSUA_UNLOCK();
	return PJ_EINVALIDOP;
    }

    info->id = acc_id;
    info->is_default = (pjsua_var.default_acc == acc_id);
    info->acc_uri = acc_cfg->id;
    info->has_registration = (acc->cfg.reg_uri.slen > 0);
    info->online_status = acc->online_status;
    
    if (acc->reg_last_err) {
	info->status = acc->reg_last_err;
	pj_strerror(acc->reg_last_err, info->buf_, sizeof(info->buf_));
	info->status_text = pj_str(info->buf_);
    } else if (acc->reg_last_code) {
	if (info->has_registration) {
	    info->status = acc->reg_last_code;
	    info->status_text = *pjsip_get_status_text(acc->reg_last_code);
	} else {
	    info->status = 0;
	    info->status_text = pj_str("not registered");
	}
    } else if (acc->cfg.reg_uri.slen) {
	info->status = 100;
	info->status_text = pj_str("In Progress");
    } else {
	info->status = 0;
	info->status_text = pj_str("does not register");
    }
    
    if (acc->regc) {
	pjsip_regc_info regc_info;
	pjsip_regc_get_info(acc->regc, &regc_info);
	info->expires = regc_info.next_reg;
    } else {
	info->expires = -1;
    }

    PJSUA_UNLOCK();

    return PJ_SUCCESS;

}


/*
 * Enum accounts all account ids.
 */
PJ_DEF(pj_status_t) pjsua_enum_accs(pjsua_acc_id ids[],
				    unsigned *count )
{
    unsigned i, c;

    PJ_ASSERT_RETURN(ids && *count, PJ_EINVAL);

    PJSUA_LOCK();

    for (i=0, c=0; c<*count && i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	if (!pjsua_var.acc[i].valid)
	    continue;
	ids[c] = i;
	++c;
    }

    *count = c;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Enum accounts info.
 */
PJ_DEF(pj_status_t) pjsua_acc_enum_info( pjsua_acc_info info[],
					 unsigned *count )
{
    unsigned i, c;

    PJ_ASSERT_RETURN(info && *count, PJ_EINVAL);

    PJSUA_LOCK();

    for (i=0, c=0; c<*count && i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	if (!pjsua_var.acc[i].valid)
	    continue;

	pjsua_acc_get_info(i, &info[c]);
	++c;
    }

    *count = c;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * This is an internal function to find the most appropriate account to
 * used to reach to the specified URL.
 */
PJ_DEF(pjsua_acc_id) pjsua_acc_find_for_outgoing(const pj_str_t *url)
{
    pj_str_t tmp;
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;
    unsigned acc_id;

    PJSUA_LOCK();

    PJ_TODO(dont_use_pjsua_pool);

    pj_strdup_with_null(pjsua_var.pool, &tmp, url);

    uri = pjsip_parse_uri(pjsua_var.pool, tmp.ptr, tmp.slen, 0);
    if (!uri) {
	acc_id = pjsua_var.default_acc;
	goto on_return;
    }

    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri)) 
    {
	/* Return the first account with proxy */
	for (acc_id=0; acc_id<PJ_ARRAY_SIZE(pjsua_var.acc); ++acc_id) {
	    if (!pjsua_var.acc[acc_id].valid)
		continue;
	    if (!pj_list_empty(&pjsua_var.acc[acc_id].route_set))
		break;
	}

	if (acc_id != PJ_ARRAY_SIZE(pjsua_var.acc)) {
	    /* Found rather matching account */
	    goto on_return;
	}

	/* Not found, use default account */
	acc_id = pjsua_var.default_acc;
	goto on_return;
    }

    sip_uri = pjsip_uri_get_uri(uri);

    /* Find matching domain AND port */
    for (acc_id=0; acc_id<PJ_ARRAY_SIZE(pjsua_var.acc); ++acc_id) {
	if (!pjsua_var.acc[acc_id].valid)
	    continue;
	if (pj_stricmp(&pjsua_var.acc[acc_id].srv_domain, &sip_uri->host)==0 &&
	    pjsua_var.acc[acc_id].srv_port == sip_uri->port)
	    break;
    }

    /* If no match, try to match the domain part only */
    if (acc_id == PJ_ARRAY_SIZE(pjsua_var.acc)) {
	/* Just use default account */
	for (acc_id=0; acc_id<PJ_ARRAY_SIZE(pjsua_var.acc); ++acc_id) {
	    if (!pjsua_var.acc[acc_id].valid)
		continue;
	    if (pj_stricmp(&pjsua_var.acc[acc_id].srv_domain, &sip_uri->host)==0)
		break;
	}
    }

    if (acc_id == PJ_ARRAY_SIZE(pjsua_var.acc)) {
	/* Just use default account */
	acc_id = pjsua_var.default_acc;
    }

on_return:
    PJSUA_UNLOCK();

    return acc_id;
}


/*
 * This is an internal function to find the most appropriate account to be
 * used to handle incoming calls.
 */
PJ_DEF(pjsua_acc_id) pjsua_acc_find_for_incoming(pjsip_rx_data *rdata)
{
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;
    unsigned acc_id;

    uri = rdata->msg_info.to->uri;

    /* Just return default account if To URI is not SIP: */
    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri)) 
    {
	return pjsua_var.default_acc;
    }


    PJSUA_LOCK();

    sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);

    /* Find account which has matching username and domain. */
    for (acc_id=0; acc_id < pjsua_var.acc_cnt; ++acc_id) {

	pjsua_acc *acc = &pjsua_var.acc[acc_id];

	if (pj_stricmp(&acc->user_part, &sip_uri->user)==0 &&
	    pj_stricmp(&acc->srv_domain, &sip_uri->host)==0) 
	{
	    /* Match ! */
	    PJSUA_UNLOCK();
	    return acc_id;
	}
    }

    /* No matching, try match domain part only. */
    for (acc_id=0; acc_id < pjsua_var.acc_cnt; ++acc_id) {

	pjsua_acc *acc = &pjsua_var.acc[acc_id];

	if (pj_stricmp(&acc->srv_domain, &sip_uri->host)==0) {
	    /* Match ! */
	    PJSUA_UNLOCK();
	    return acc_id;
	}
    }

    /* Still no match, use default account */
    PJSUA_UNLOCK();
    return pjsua_var.default_acc;
}


PJ_DEF(pj_status_t) pjsua_acc_create_uac_contact( pj_pool_t *pool,
						  pj_str_t *contact,
						  pjsua_acc_id acc_id,
						  const pj_str_t *suri)
{
    pjsua_acc *acc;
    pjsip_sip_uri *sip_uri;
    pj_status_t status;
    pjsip_transport_type_e tp_type = PJSIP_TRANSPORT_UNSPECIFIED;
    pj_str_t local_addr;
    unsigned flag;
    int secure;
    int local_port;
    
    acc = &pjsua_var.acc[acc_id];

    /* If route-set is configured for the account, then URI is the 
     * first entry of the route-set.
     */
    if (!pj_list_empty(&acc->route_set)) {
	sip_uri = (pjsip_sip_uri*) acc->route_set.next->name_addr.uri;
    } else {
	pj_str_t tmp;
	pjsip_uri *uri;

	pj_strdup_with_null(pool, &tmp, suri);

	uri = pjsip_parse_uri(pool, tmp.ptr, tmp.slen, 0);
	if (uri == NULL)
	    return PJSIP_EINVALIDURI;

	/* For non-SIP scheme, route set should be configured */
	if (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri))
	    return PJSIP_EINVALIDREQURI;

	sip_uri = (pjsip_sip_uri*)uri;
    }

    /* Get transport type of the URI */
    if (PJSIP_URI_SCHEME_IS_SIPS(sip_uri))
	tp_type = PJSIP_TRANSPORT_TLS;
    else if (sip_uri->transport_param.slen == 0) {
	tp_type = PJSIP_TRANSPORT_UDP;
    } else
	tp_type = pjsip_transport_get_type_from_name(&sip_uri->transport_param);
    
    if (tp_type == PJSIP_TRANSPORT_UNSPECIFIED)
	return PJSIP_EUNSUPTRANSPORT;

    flag = pjsip_transport_get_flag_from_type(tp_type);
    secure = (flag & PJSIP_TRANSPORT_SECURE) != 0;

    /* Get local address suitable to send request from */
    status = pjsip_tpmgr_find_local_addr(pjsip_endpt_get_tpmgr(pjsua_var.endpt),
					 pool, tp_type, &local_addr, &local_port);
    if (status != PJ_SUCCESS)
	return status;

    /* Create the contact header */
    contact->ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
    contact->slen = pj_ansi_snprintf(contact->ptr, PJSIP_MAX_URL_SIZE,
				     "%.*s%s<%s:%.*s%s%.*s:%d;transport=%s>",
				     (int)acc->display.slen,
				     acc->display.ptr,
				     (acc->display.slen?" " : ""),
				     (secure ? "sips" : "sip"),
				     (int)acc->user_part.slen,
				     acc->user_part.ptr,
				     (acc->user_part.slen?"@":""),
				     (int)local_addr.slen,
				     local_addr.ptr,
				     local_port,
				     pjsip_transport_get_type_name(tp_type));

    return PJ_SUCCESS;
}



PJ_DEF(pj_status_t) pjsua_acc_create_uas_contact( pj_pool_t *pool,
						  pj_str_t *contact,
						  pjsua_acc_id acc_id,
						  pjsip_rx_data *rdata )
{
    /* 
     *  Section 12.1.1, paragraph about using SIPS URI in Contact.
     *  If the request that initiated the dialog contained a SIPS URI 
     *  in the Request-URI or in the top Record-Route header field value, 
     *  if there was any, or the Contact header field if there was no 
     *  Record-Route header field, the Contact header field in the response
     *  MUST be a SIPS URI.
     */
    pjsua_acc *acc;
    pjsip_sip_uri *sip_uri;
    pj_status_t status;
    pjsip_transport_type_e tp_type = PJSIP_TRANSPORT_UNSPECIFIED;
    pj_str_t local_addr;
    unsigned flag;
    int secure;
    int local_port;
    
    acc = &pjsua_var.acc[acc_id];

    /* If Record-Route is present, then URI is the top Record-Route. */
    if (rdata->msg_info.record_route) {
	sip_uri = (pjsip_sip_uri*) rdata->msg_info.record_route->name_addr.uri;
    } else {
	pjsip_contact_hdr *h_contact;
	pjsip_uri *uri = NULL;

	/* Otherwise URI is Contact URI */
	h_contact = pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT,
				       NULL);
	if (h_contact)
	    uri = pjsip_uri_get_uri(h_contact->uri);
	

	/* Or if Contact URI is not present, take the remote URI from
	 * the From URI.
	 */
	if (uri == NULL)
	    uri = pjsip_uri_get_uri(rdata->msg_info.from->uri);


	/* Can only do sip/sips scheme at present. */
	if (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri))
	    return PJSIP_EINVALIDREQURI;

	sip_uri = (pjsip_sip_uri*)uri;
    }

    /* Get transport type of the URI */
    if (PJSIP_URI_SCHEME_IS_SIPS(sip_uri))
	tp_type = PJSIP_TRANSPORT_TLS;
    else if (sip_uri->transport_param.slen == 0) {
	tp_type = PJSIP_TRANSPORT_UDP;
    } else
	tp_type = pjsip_transport_get_type_from_name(&sip_uri->transport_param);
    
    if (tp_type == PJSIP_TRANSPORT_UNSPECIFIED)
	return PJSIP_EUNSUPTRANSPORT;

    flag = pjsip_transport_get_flag_from_type(tp_type);
    secure = (flag & PJSIP_TRANSPORT_SECURE) != 0;

    /* Get local address suitable to send request from */
    status = pjsip_tpmgr_find_local_addr(pjsip_endpt_get_tpmgr(pjsua_var.endpt),
					 pool, tp_type, &local_addr, &local_port);
    if (status != PJ_SUCCESS)
	return status;

    /* Create the contact header */
    contact->ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
    contact->slen = pj_ansi_snprintf(contact->ptr, PJSIP_MAX_URL_SIZE,
				     "%.*s%s<%s:%.*s%s%.*s:%d;transport=%s>",
				     (int)acc->display.slen,
				     acc->display.ptr,
				     (acc->display.slen?" " : ""),
				     (secure ? "sips" : "sip"),
				     (int)acc->user_part.slen,
				     acc->user_part.ptr,
				     (acc->user_part.slen?"@":""),
				     (int)local_addr.slen,
				     local_addr.ptr,
				     local_port,
				     pjsip_transport_get_type_name(tp_type));

    return PJ_SUCCESS;
}




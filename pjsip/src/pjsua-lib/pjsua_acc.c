/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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


#define THIS_FILE		"pjsua_acc.c"

enum
{
    OUTBOUND_UNKNOWN,	// status unknown
    OUTBOUND_WANTED,	// initiated in registration
    OUTBOUND_ACTIVE,	// got positive response from server
    OUTBOUND_NA		// not wanted or got negative response from server
};


static int get_ip_addr_ver(const pj_str_t *host);
static void schedule_reregistration(pjsua_acc *acc);
static void keep_alive_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te);

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
    return acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc) &&
	   pjsua_var.acc[acc_id].valid;
}


/*
 * Set default account
 */
PJ_DEF(pj_status_t) pjsua_acc_set_default(pjsua_acc_id acc_id)
{
    pjsua_var.default_acc = acc_id;
    return PJ_SUCCESS;
}


/*
 * Get default account.
 */
PJ_DEF(pjsua_acc_id) pjsua_acc_get_default(void)
{
    return pjsua_var.default_acc;
}


/*
 * Copy account configuration.
 */
PJ_DEF(void) pjsua_acc_config_dup( pj_pool_t *pool,
				   pjsua_acc_config *dst,
				   const pjsua_acc_config *src)
{
    unsigned i;

    pj_memcpy(dst, src, sizeof(pjsua_acc_config));

    pj_strdup_with_null(pool, &dst->id, &src->id);
    pj_strdup_with_null(pool, &dst->reg_uri, &src->reg_uri);
    pj_strdup_with_null(pool, &dst->force_contact, &src->force_contact);
    pj_strdup_with_null(pool, &dst->reg_contact_params,
			&src->reg_contact_params);
    pj_strdup_with_null(pool, &dst->reg_contact_uri_params,
			&src->reg_contact_uri_params);
    pj_strdup_with_null(pool, &dst->contact_params, &src->contact_params);
    pj_strdup_with_null(pool, &dst->contact_uri_params,
                        &src->contact_uri_params);
    pj_strdup_with_null(pool, &dst->pidf_tuple_id, &src->pidf_tuple_id);
    pj_strdup_with_null(pool, &dst->rfc5626_instance_id,
                        &src->rfc5626_instance_id);
    pj_strdup_with_null(pool, &dst->rfc5626_reg_id, &src->rfc5626_reg_id);

    dst->proxy_cnt = src->proxy_cnt;
    for (i=0; i<src->proxy_cnt; ++i)
	pj_strdup_with_null(pool, &dst->proxy[i], &src->proxy[i]);

    dst->reg_timeout = src->reg_timeout;
    dst->reg_delay_before_refresh = src->reg_delay_before_refresh;
    dst->cred_count = src->cred_count;

    for (i=0; i<src->cred_count; ++i) {
	pjsip_cred_dup(pool, &dst->cred_info[i], &src->cred_info[i]);
    }

    pj_list_init(&dst->reg_hdr_list);
    if (!pj_list_empty(&src->reg_hdr_list)) {
	const pjsip_hdr *hdr;

	hdr = src->reg_hdr_list.next;
	while (hdr != &src->reg_hdr_list) {
	    pj_list_push_back(&dst->reg_hdr_list, pjsip_hdr_clone(pool, hdr));
	    hdr = hdr->next;
	}
    }

    pj_list_init(&dst->sub_hdr_list);
    if (!pj_list_empty(&src->sub_hdr_list)) {
	const pjsip_hdr *hdr;

	hdr = src->sub_hdr_list.next;
	while (hdr != &src->sub_hdr_list) {
	    pj_list_push_back(&dst->sub_hdr_list, pjsip_hdr_clone(pool, hdr));
	    hdr = hdr->next;
	}
    }

    pjsip_auth_clt_pref_dup(pool, &dst->auth_pref, &src->auth_pref);

    pjsua_transport_config_dup(pool, &dst->rtp_cfg, &src->rtp_cfg);

    pjsua_ice_config_dup(pool, &dst->ice_cfg, &src->ice_cfg);
    pjsua_turn_config_dup(pool, &dst->turn_cfg, &src->turn_cfg);

    pjsua_srtp_opt_dup(pool, &dst->srtp_opt, &src->srtp_opt, PJ_FALSE);

    pj_strdup(pool, &dst->ka_data, &src->ka_data);

    pjmedia_rtcp_fb_setting_dup(pool, &dst->rtcp_fb_cfg, &src->rtcp_fb_cfg);
}

/*
 * Calculate CRC of proxy list.
 */
static pj_uint32_t calc_proxy_crc(const pj_str_t proxy[], pj_size_t cnt)
{
    pj_crc32_context ctx;
    unsigned i;
    
    pj_crc32_init(&ctx);
    for (i=0; i<cnt; ++i) {
	pj_crc32_update(&ctx, (pj_uint8_t*)proxy[i].ptr, proxy[i].slen);
    }

    return pj_crc32_final(&ctx);
}

/*
 * Initialize outbound settings.
 */
static void init_outbound_setting(pjsua_acc *acc)
{
    pjsua_acc_config *acc_cfg = &acc->cfg;
    if (acc_cfg->rfc5626_instance_id.slen == 0) {
	const pj_str_t *hostname;
	pj_uint32_t hval;
	pj_size_t pos;
	char instprm[] = ";+sip.instance=\"<urn:uuid:00000000-0000-0000-0000-0000CCDDEEFF>\"";

	hostname = pj_gethostname();
	pos = pj_ansi_strlen(instprm) - 10;
	hval = pj_hash_calc(0, hostname->ptr, (unsigned)hostname->slen);
	pj_val_to_hex_digit(((char*)&hval)[0], instprm + pos + 0);
	pj_val_to_hex_digit(((char*)&hval)[1], instprm + pos + 2);
	pj_val_to_hex_digit(((char*)&hval)[2], instprm + pos + 4);
	pj_val_to_hex_digit(((char*)&hval)[3], instprm + pos + 6);

	pj_strdup2(acc->pool, &acc->rfc5626_instprm, instprm);
    } else {
	const char *prmname = ";+sip.instance=\"";
	pj_size_t len;

	len = pj_ansi_strlen(prmname) + acc_cfg->rfc5626_instance_id.slen + 1;
	acc->rfc5626_instprm.ptr = (char*)pj_pool_alloc(acc->pool, len + 1);
	pj_ansi_snprintf(acc->rfc5626_instprm.ptr, len + 1,
			 "%s%.*s\"",
			 prmname,
			 (int)acc_cfg->rfc5626_instance_id.slen,
			 acc_cfg->rfc5626_instance_id.ptr);
	acc->rfc5626_instprm.slen = len;
    }

    if (acc_cfg->rfc5626_reg_id.slen == 0) {
	acc->rfc5626_regprm = pj_str(";reg-id=1");
    } else {
	const char *prmname = ";reg-id=";
	pj_size_t len;

	len = pj_ansi_strlen(prmname) + acc_cfg->rfc5626_reg_id.slen;
	acc->rfc5626_regprm.ptr = (char*)pj_pool_alloc(acc->pool, len + 1);
	pj_ansi_snprintf(acc->rfc5626_regprm.ptr, len + 1,
			 "%s%.*s\"",
			 prmname,
			 (int)acc_cfg->rfc5626_reg_id.slen,
			 acc_cfg->rfc5626_reg_id.ptr);
	acc->rfc5626_regprm.slen = len;
    }

    acc->rfc5626_status = OUTBOUND_WANTED;
}

/*
 * Initialize a new account (after configuration is set).
 */
static pj_status_t initialize_acc(unsigned acc_id)
{
    pjsua_acc_config *acc_cfg = &pjsua_var.acc[acc_id].cfg;
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pjsip_name_addr *name_addr;
    pjsip_sip_uri *sip_reg_uri;
    pj_status_t status;
    unsigned i;

    /* Need to parse local_uri to get the elements: */

    name_addr = (pjsip_name_addr*)
		    pjsip_parse_uri(acc->pool, acc_cfg->id.ptr,
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
	acc->display = name_addr->display;
	acc->user_part = name_addr->display;
	acc->srv_domain = pj_str("");
	acc->srv_port = 0;
    } else {
	pjsip_sip_uri *sip_uri;

	/* Get the SIP URI object: */
	sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(name_addr);

	/* Save the user and domain part. These will be used when finding an
	 * account for incoming requests.
	 */
	acc->display = name_addr->display;
	acc->user_part = sip_uri->user;
	acc->srv_domain = sip_uri->host;
	acc->srv_port = 0;

	/* Escape user part (ticket #2010) */
	if (acc->user_part.slen) {
	    const pjsip_parser_const_t *pconst;
	    char buf[PJSIP_MAX_URL_SIZE];
	    pj_str_t user_part;

	    pconst = pjsip_parser_const();
	    pj_strset(&user_part, buf, sizeof(buf));
	    pj_strncpy_escape(&user_part, &sip_uri->user, sizeof(buf),
			      &pconst->pjsip_USER_SPEC_LENIENT);
	    if (user_part.slen > acc->user_part.slen)
		pj_strdup(acc->pool, &acc->user_part, &user_part);
	}
    }
    acc->is_sips = PJSIP_URI_SCHEME_IS_SIPS(name_addr);


    /* Parse registrar URI, if any */
    if (acc_cfg->reg_uri.slen) {
	pjsip_uri *reg_uri;

	reg_uri = pjsip_parse_uri(acc->pool, acc_cfg->reg_uri.ptr,
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

    if (!pj_list_empty(&pjsua_var.outbound_proxy)) {
	pjsip_route_hdr *r;

	r = pjsua_var.outbound_proxy.next;
	while (r != &pjsua_var.outbound_proxy) {
	    pj_list_push_back(&acc->route_set,
			      pjsip_hdr_shallow_clone(acc->pool, r));
	    r = r->next;
	}
    }

    for (i=0; i<acc_cfg->proxy_cnt; ++i) {
    	pj_str_t hname = { "Route", 5};
	pjsip_route_hdr *r;
	pj_str_t tmp;

	pj_strdup_with_null(acc->pool, &tmp, &acc_cfg->proxy[i]);
	r = (pjsip_route_hdr*)
	    pjsip_parse_hdr(acc->pool, &hname, tmp.ptr, tmp.slen, NULL);
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

    /* If account's ICE and TURN customization is not set, then
     * initialize it with the settings from the global media config.
     */
    if (acc->cfg.ice_cfg_use == PJSUA_ICE_CONFIG_USE_DEFAULT) {
	pjsua_ice_config_from_media_config(NULL, &acc->cfg.ice_cfg,
	                                &pjsua_var.media_cfg);
    }
    if (acc->cfg.turn_cfg_use == PJSUA_TURN_CONFIG_USE_DEFAULT) {
	pjsua_turn_config_from_media_config(NULL, &acc->cfg.turn_cfg,
	                                    &pjsua_var.media_cfg);
    }

    /* If ICE is enabled, add "+sip.ice" media feature tag in account's
     * contact params.
     */
#if PJSUA_ADD_ICE_TAGS
    if (acc_cfg->ice_cfg.enable_ice) {
	pj_ssize_t new_len;
	pj_str_t new_prm;

	new_len = acc_cfg->contact_params.slen + 10;
	new_prm.ptr = (char*)pj_pool_alloc(acc->pool, new_len);
	pj_strcpy(&new_prm, &acc_cfg->contact_params);
	pj_strcat2(&new_prm, ";+sip.ice");
	acc_cfg->contact_params = new_prm;
    }
#endif

    status = pjsua_pres_init_acc(acc_id);
    if (status != PJ_SUCCESS)
	return status;

    /* If SIP outbound is enabled, generate instance and reg ID if they are
     * not specified
     */
    if (acc_cfg->use_rfc5626) {
	init_outbound_setting(acc);
    }

    /* Mark account as valid */
    pjsua_var.acc[acc_id].valid = PJ_TRUE;

    /* Insert account ID into account ID array, sorted by priority */
    for (i=0; i<pjsua_var.acc_cnt; ++i) {
	if ( pjsua_var.acc[pjsua_var.acc_ids[i]].cfg.priority <
	     pjsua_var.acc[acc_id].cfg.priority)
	{
	    break;
	}
    }
    pj_array_insert(pjsua_var.acc_ids, sizeof(pjsua_var.acc_ids[0]),
		    pjsua_var.acc_cnt, i, &acc_id);

    if (acc_cfg->transport_id != PJSUA_INVALID_ID)
	acc->tp_type = pjsua_var.tpdata[acc_cfg->transport_id].type;

    acc->ip_change_op = PJSUA_IP_CHANGE_OP_NULL;

    return PJ_SUCCESS;
}


/*
 * Add a new account to pjsua.
 */
PJ_DEF(pj_status_t) pjsua_acc_add( const pjsua_acc_config *cfg,
				   pj_bool_t is_default,
				   pjsua_acc_id *p_acc_id)
{
    pjsua_acc *acc;
    unsigned i, id;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(cfg, PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc_cnt < PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_ETOOMANY);

    /* Must have a transport */
    PJ_ASSERT_RETURN(pjsua_var.tpdata[0].data.ptr != NULL, PJ_EINVALIDOP);

    PJ_LOG(4,(THIS_FILE, "Adding account: id=%.*s",
	      (int)cfg->id.slen, cfg->id.ptr));
    pj_log_push_indent();

    PJSUA_LOCK();

    /* Find empty account id. */
    for (id=0; id < PJ_ARRAY_SIZE(pjsua_var.acc); ++id) {
	if (pjsua_var.acc[id].valid == PJ_FALSE)
	    break;
    }

    /* Expect to find a slot */
    PJ_ASSERT_ON_FAIL(	id < PJ_ARRAY_SIZE(pjsua_var.acc), 
			{PJSUA_UNLOCK(); return PJ_EBUG;});

    acc = &pjsua_var.acc[id];

    /* Create pool for this account. */
    if (acc->pool)
	pj_pool_reset(acc->pool);
    else
	acc->pool = pjsua_pool_create("acc%p", PJSUA_POOL_LEN_ACC,
                                  PJSUA_POOL_INC_ACC);

    /* Copy config */
    pjsua_acc_config_dup(acc->pool, &pjsua_var.acc[id].cfg, cfg);
    
    /* Normalize registration timeout and refresh delay */
    if (pjsua_var.acc[id].cfg.reg_uri.slen) {
        if (pjsua_var.acc[id].cfg.reg_timeout == 0) {
            pjsua_var.acc[id].cfg.reg_timeout = PJSUA_REG_INTERVAL;
        }
        if (pjsua_var.acc[id].cfg.reg_delay_before_refresh == 0) {
            pjsua_var.acc[id].cfg.reg_delay_before_refresh =
                PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH;
        }
    }

    /* Check the route URI's and force loose route if required */
    for (i=0; i<acc->cfg.proxy_cnt; ++i) {
	status = normalize_route_uri(acc->pool, &acc->cfg.proxy[i]);
	if (status != PJ_SUCCESS) {
	    PJSUA_UNLOCK();
	    pj_log_pop_indent();
	    return status;
	}
    }

    /* Get CRC of account proxy setting */
    acc->local_route_crc = calc_proxy_crc(acc->cfg.proxy, acc->cfg.proxy_cnt);

    /* Get CRC of global outbound proxy setting */
    acc->global_route_crc=calc_proxy_crc(pjsua_var.ua_cfg.outbound_proxy,
					 pjsua_var.ua_cfg.outbound_proxy_cnt);

    status = initialize_acc(id);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error adding account", status);
	pj_pool_release(acc->pool);
	acc->pool = NULL;
	PJSUA_UNLOCK();
	pj_log_pop_indent();
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
    if (pjsua_var.acc[id].cfg.reg_uri.slen) {
	if (pjsua_var.acc[id].cfg.register_on_acc_add)
            pjsua_acc_set_registration(id, PJ_TRUE);
    } else {
	/* Otherwise subscribe to MWI, if it's enabled */
	if (pjsua_var.acc[id].cfg.mwi_enabled)
	    pjsua_start_mwi(id, PJ_TRUE);

	/* Start publish too */
	if (acc->cfg.publish_enabled)
	    pjsua_pres_init_publish_acc(id);
    }

    pj_log_pop_indent();
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
    pjsua_transport_data *t = &pjsua_var.tpdata[tid];
    char transport_param[32];
    char uri[PJSIP_MAX_URL_SIZE];
    char addr_buf[PJ_INET6_ADDRSTRLEN+10];
    pjsua_acc_id acc_id;
    pj_status_t status;

    /* ID must be valid */
    PJ_ASSERT_RETURN(tid>=0 && tid<(int)PJ_ARRAY_SIZE(pjsua_var.tpdata), 
		     PJ_EINVAL);

    /* Transport must be valid */
    PJ_ASSERT_RETURN(t->data.ptr != NULL, PJ_EINVAL);
    
    pjsua_acc_config_default(&cfg);

    /* Lower the priority of local account */
    --cfg.priority;

    /* Don't add transport parameter if it's UDP */
    if (t->type!=PJSIP_TRANSPORT_UDP && t->type!=PJSIP_TRANSPORT_UDP6) {
	pj_ansi_snprintf(transport_param, sizeof(transport_param),
		         ";transport=%s",
			 pjsip_transport_get_type_name(t->type));
    } else {
	transport_param[0] = '\0';
    }

    /* Build URI for the account */
    pj_ansi_snprintf(uri, PJSIP_MAX_URL_SIZE,		     
		     "<sip:%s%s>", 
		     pj_addr_str_print(&t->local_name.host, t->local_name.port, 
				       addr_buf, sizeof(addr_buf), 1),
		     transport_param);

    cfg.id = pj_str(uri);
    cfg.transport_id = tid;
    
    status = pjsua_acc_add(&cfg, is_default, &acc_id);
    if (status == PJ_SUCCESS) {
	pjsua_var.acc[acc_id].tp_type = t->type;
	if (p_acc_id)
	    *p_acc_id = acc_id;
    }

    return status;
}


/*
 * Set arbitrary data to be associated with the account.
 */
PJ_DEF(pj_status_t) pjsua_acc_set_user_data(pjsua_acc_id acc_id,
					    void *user_data)
{
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    PJSUA_LOCK();

    pjsua_var.acc[acc_id].cfg.user_data = user_data;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Retrieve arbitrary data associated with the account.
 */
PJ_DEF(void*) pjsua_acc_get_user_data(pjsua_acc_id acc_id)
{
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     NULL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, NULL);

    return pjsua_var.acc[acc_id].cfg.user_data;
}


/*
 * Delete account.
 */
PJ_DEF(pj_status_t) pjsua_acc_del(pjsua_acc_id acc_id)
{
    pjsua_acc *acc;
    unsigned i;

    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    PJ_LOG(4,(THIS_FILE, "Deleting account %d..", acc_id));
    pj_log_push_indent();

    PJSUA_LOCK();

    acc = &pjsua_var.acc[acc_id];

    /* Cancel keep-alive timer, if any */
    if (acc->ka_timer.id) {
	pjsip_endpt_cancel_timer(pjsua_var.endpt, &acc->ka_timer);
	acc->ka_timer.id = PJ_FALSE;
    }
    if (acc->ka_transport) {
	pjsip_transport_dec_ref(acc->ka_transport);
	acc->ka_transport = NULL;
    }

    /* Cancel any re-registration timer */
    if (acc->auto_rereg.timer.id) {
	acc->auto_rereg.timer.id = PJ_FALSE;
	pjsua_cancel_timer(&acc->auto_rereg.timer);
    }

    /* Delete registration */
    if (acc->regc != NULL) {
	pjsua_acc_set_registration(acc_id, PJ_FALSE);
	if (acc->regc) {
	    pjsip_regc_destroy(acc->regc);
	}
	acc->regc = NULL;
    }

    /* Terminate mwi subscription */
    if (acc->cfg.mwi_enabled) {
        acc->cfg.mwi_enabled = PJ_FALSE;
        pjsua_start_mwi(acc_id, PJ_FALSE);
    }

    /* Delete server presence subscription */
    pjsua_pres_delete_acc(acc_id, 0);

    /* Release account pool */
    if (acc->pool) {
	pj_pool_release(acc->pool);
	acc->pool = NULL;
    }

    /* Invalidate */
    acc->valid = PJ_FALSE;
    acc->contact.slen = 0;
    acc->reg_mapped_addr.slen = 0;
    acc->rfc5626_status = OUTBOUND_UNKNOWN;
    pj_bzero(&acc->via_addr, sizeof(acc->via_addr));
    acc->via_tp = NULL;
    acc->next_rtp_port = 0;
    acc->ip_change_op = PJSUA_IP_CHANGE_OP_NULL;

    /* Remove from array */
    for (i=0; i<pjsua_var.acc_cnt; ++i) {
	if (pjsua_var.acc_ids[i] == acc_id)
	    break;
    }
    if (i != pjsua_var.acc_cnt) {
	pj_array_erase(pjsua_var.acc_ids, sizeof(pjsua_var.acc_ids[0]),
		       pjsua_var.acc_cnt, i);
	--pjsua_var.acc_cnt;
    }

    /* Leave the calls intact, as I don't think calls need to
     * access account once it's created
     */

    /* Update default account */
    if (pjsua_var.default_acc == acc_id)
	pjsua_var.default_acc = 0;

#if PJ_HAS_SSL_SOCK
    pj_turn_sock_tls_cfg_wipe_keys(&acc->cfg.turn_cfg.turn_tls_setting);
#endif

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Account id %d deleted", acc_id));

    pj_log_pop_indent();
    return PJ_SUCCESS;
}


/* Get config */
PJ_DEF(pj_status_t) pjsua_acc_get_config(pjsua_acc_id acc_id,
                                         pj_pool_t *pool,
                                         pjsua_acc_config *acc_cfg)
{
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc)
                     && pjsua_var.acc[acc_id].valid, PJ_EINVAL);
    //this now would not work due to corrupt header list
    //pj_memcpy(acc_cfg, &pjsua_var.acc[acc_id].cfg, sizeof(*acc_cfg));
    pjsua_acc_config_dup(pool, acc_cfg, &pjsua_var.acc[acc_id].cfg);
    return PJ_SUCCESS;
}

/* Compare two SIP headers. Return zero if equal */
static int pjsip_hdr_cmp(const pjsip_hdr *h1, const pjsip_hdr *h2)
{
    char buf1[PJSIP_MAX_URL_SIZE];
    char buf2[PJSIP_MAX_URL_SIZE];
    pj_str_t p1, p2;

    p1.ptr = buf1;
    p1.slen = 0;
    p2.ptr = buf2;
    p2.slen = 0;

    p1.slen = pjsip_hdr_print_on((void*)h1, buf1, sizeof(buf1));
    if (p1.slen < 0)
	p1.slen = 0;
    p2.slen = pjsip_hdr_print_on((void*)h2, buf2, sizeof(buf2));
    if (p2.slen < 0)
	p2.slen = 0;

    return pj_strcmp(&p1, &p2);
}

/* Update SIP header list from another list. Return PJ_TRUE if
 * the list has been updated */
static pj_bool_t update_hdr_list(pj_pool_t *pool, pjsip_hdr *dst,
                                 const pjsip_hdr *src)
{
    pjsip_hdr *dst_i;
    const pjsip_hdr *src_i;
    pj_bool_t changed = PJ_FALSE;

    /* Remove header that's no longer needed */
    for (dst_i = dst->next; dst_i != dst; ) {
	for (src_i = src->next; src_i != src; src_i = src_i->next) {
	    if (pjsip_hdr_cmp(dst_i, src_i) == 0)
		break;
	}
	if (src_i == src) {
	    pjsip_hdr *next = dst_i->next;
	    pj_list_erase(dst_i);
	    changed = PJ_TRUE;
	    dst_i = next;
	} else {
	    dst_i = dst_i->next;
	}
    }

    /* Add new header */
    for (src_i = src->next; src_i != src; src_i = src_i->next) {
	for (dst_i = dst->next; dst_i != dst; dst_i = dst_i->next) {
	    if (pjsip_hdr_cmp(dst_i, src_i) == 0)
		break;
	}
	if (dst_i == dst) {
	    dst_i = pjsip_hdr_clone(pool, src_i);
	    pj_list_push_back(dst, dst_i);
	    changed = PJ_TRUE;
	}
    }

    return changed;
}

/*
 * Modify account information.
 */
PJ_DEF(pj_status_t) pjsua_acc_modify( pjsua_acc_id acc_id,
				      const pjsua_acc_config *cfg)
{
    pjsua_acc *acc;
    pjsip_name_addr *id_name_addr = NULL;
    pjsip_sip_uri *id_sip_uri = NULL;
    pjsip_sip_uri *reg_sip_uri = NULL;
    pj_uint32_t local_route_crc, global_route_crc;
    pjsip_route_hdr global_route;
    pjsip_route_hdr local_route;
    pj_str_t acc_proxy[PJSUA_ACC_MAX_PROXIES];
    pj_bool_t update_reg = PJ_FALSE;
    pj_bool_t unreg_first = PJ_FALSE;
    pj_bool_t update_mwi = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Modifying account %d", acc_id));
    pj_log_push_indent();

    PJSUA_LOCK();

    acc = &pjsua_var.acc[acc_id];
    if (!acc->valid) {
	status = PJ_EINVAL;
	goto on_return;
    }

    /* == Validate first == */

    /* Account id */
    if (pj_strcmp(&acc->cfg.id, &cfg->id)) {
	/* Need to parse id to get the elements: */
	id_name_addr = (pjsip_name_addr*)
			pjsip_parse_uri(acc->pool, cfg->id.ptr, cfg->id.slen,
					PJSIP_PARSE_URI_AS_NAMEADDR);
	if (id_name_addr == NULL) {
	    status = PJSIP_EINVALIDURI;
	    pjsua_perror(THIS_FILE, "Invalid local URI", status);
	    goto on_return;
	}

	/* URI MUST be a SIP or SIPS: */
	if (!PJSIP_URI_SCHEME_IS_SIP(id_name_addr) && 
	    !PJSIP_URI_SCHEME_IS_SIPS(id_name_addr)) 
	{
	    status = PJSIP_EINVALIDSCHEME;
	    pjsua_perror(THIS_FILE, "Invalid local URI", status);
	    goto on_return;
	}

	/* Get the SIP URI object: */
	id_sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(id_name_addr);
    }

    /* Registrar URI */
    if (pj_strcmp(&acc->cfg.reg_uri, &cfg->reg_uri) && cfg->reg_uri.slen) {
	pjsip_uri *reg_uri;

	/* Need to parse reg_uri to get the elements: */
	reg_uri = pjsip_parse_uri(acc->pool, cfg->reg_uri.ptr, 
				  cfg->reg_uri.slen, 0);
	if (reg_uri == NULL) {
	    status = PJSIP_EINVALIDURI;
	    pjsua_perror(THIS_FILE, "Invalid registrar URI", status);
	    goto on_return;
	}

	/* Registrar URI MUST be a SIP or SIPS: */
	if (!PJSIP_URI_SCHEME_IS_SIP(reg_uri) && 
	    !PJSIP_URI_SCHEME_IS_SIPS(reg_uri)) 
	{
	    status = PJSIP_EINVALIDSCHEME;
	    pjsua_perror(THIS_FILE, "Invalid registar URI", status);
	    goto on_return;
	}

	reg_sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(reg_uri);
    }

    /* REGISTER header list */
    if (update_hdr_list(acc->pool, &acc->cfg.reg_hdr_list, &cfg->reg_hdr_list)) {
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* SUBSCRIBE header list */
    update_hdr_list(acc->pool, &acc->cfg.sub_hdr_list, &cfg->sub_hdr_list);

    /* Global outbound proxy */
    global_route_crc = calc_proxy_crc(pjsua_var.ua_cfg.outbound_proxy, 
				      pjsua_var.ua_cfg.outbound_proxy_cnt);
    if (global_route_crc != acc->global_route_crc) {
	pjsip_route_hdr *r;

	/* Copy from global outbound proxies */
	pj_list_init(&global_route);
	r = pjsua_var.outbound_proxy.next;
	while (r != &pjsua_var.outbound_proxy) {
	    pj_list_push_back(&global_route,
		              pjsip_hdr_shallow_clone(acc->pool, r));
	    r = r->next;
	}
    }

    /* Account proxy */
    local_route_crc = calc_proxy_crc(cfg->proxy, cfg->proxy_cnt);
    if (local_route_crc != acc->local_route_crc) {
	pjsip_route_hdr *r;
	unsigned i;

	/* Validate the local route and save it to temporary var */
	pj_list_init(&local_route);
	for (i=0; i<cfg->proxy_cnt; ++i) {
	    pj_str_t hname = { "Route", 5};

	    pj_strdup_with_null(acc->pool, &acc_proxy[i], &cfg->proxy[i]);
	    status = normalize_route_uri(acc->pool, &acc_proxy[i]);
	    if (status != PJ_SUCCESS)
		goto on_return;
	    r = (pjsip_route_hdr*)
		pjsip_parse_hdr(acc->pool, &hname, acc_proxy[i].ptr, 
				acc_proxy[i].slen, NULL);
	    if (r == NULL) {
		status = PJSIP_EINVALIDURI;
		pjsua_perror(THIS_FILE, "Invalid URI in account route set",
			     status);
		goto on_return;
	    }

	    pj_list_push_back(&local_route, r);
	}

	/* Recalculate the CRC again after route URI normalization */
	local_route_crc = calc_proxy_crc(acc_proxy, cfg->proxy_cnt);
    }


    /* == Apply the new config == */

    /* Account ID. */
    if (id_name_addr && id_sip_uri) {
	pj_strdup_with_null(acc->pool, &acc->cfg.id, &cfg->id);
	pj_strdup_with_null(acc->pool, &acc->display, &id_name_addr->display);
	pj_strdup_with_null(acc->pool, &acc->user_part, &id_sip_uri->user);
	pj_strdup_with_null(acc->pool, &acc->srv_domain, &id_sip_uri->host);
	acc->srv_port = 0;
	acc->is_sips = PJSIP_URI_SCHEME_IS_SIPS(id_name_addr);
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* User data */
    acc->cfg.user_data = cfg->user_data;

    /* Priority */
    if (acc->cfg.priority != cfg->priority) {
	unsigned i;

	acc->cfg.priority = cfg->priority;
	
	/* Resort accounts priority */
	for (i=0; i<pjsua_var.acc_cnt; ++i) {
	    if (pjsua_var.acc_ids[i] == acc_id)
		break;
	}
	pj_assert(i < pjsua_var.acc_cnt);
	pj_array_erase(pjsua_var.acc_ids, sizeof(acc_id),
		       pjsua_var.acc_cnt, i);
	for (i=0; i<pjsua_var.acc_cnt; ++i) {
	    if (pjsua_var.acc[pjsua_var.acc_ids[i]].cfg.priority <
		acc->cfg.priority)
	    {
		break;
	    }
	}
	pj_array_insert(pjsua_var.acc_ids, sizeof(acc_id),
			pjsua_var.acc_cnt, i, &acc_id);
    }

    /* MWI */
    if (acc->cfg.mwi_enabled != cfg->mwi_enabled) {
	acc->cfg.mwi_enabled = cfg->mwi_enabled;
	update_mwi = PJ_TRUE;
    }
    if (acc->cfg.mwi_expires != cfg->mwi_expires && cfg->mwi_expires > 0) {
	acc->cfg.mwi_expires = cfg->mwi_expires;
	update_mwi = PJ_TRUE;
    }

    /* PIDF tuple ID */
    if (pj_strcmp(&acc->cfg.pidf_tuple_id, &cfg->pidf_tuple_id))
	pj_strdup_with_null(acc->pool, &acc->cfg.pidf_tuple_id,
			    &cfg->pidf_tuple_id);

    /* Publish */
    acc->cfg.publish_opt = cfg->publish_opt;
    acc->cfg.unpublish_max_wait_time_msec = cfg->unpublish_max_wait_time_msec;
    if (acc->cfg.publish_enabled != cfg->publish_enabled) {
	acc->cfg.publish_enabled = cfg->publish_enabled;
	if (!acc->cfg.publish_enabled)
	    pjsua_pres_unpublish(acc, 0);
	else
	    update_reg = PJ_TRUE;
    }

    /* Force contact URI */
    if (pj_strcmp(&acc->cfg.force_contact, &cfg->force_contact)) {
	pj_strdup_with_null(acc->pool, &acc->cfg.force_contact,
			    &cfg->force_contact);
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Register contact params */
    if (pj_strcmp(&acc->cfg.reg_contact_params, &cfg->reg_contact_params)) {
	pj_strdup_with_null(acc->pool, &acc->cfg.reg_contact_params,
			    &cfg->reg_contact_params);
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Register contact URI params */
    if (pj_strcmp(&acc->cfg.reg_contact_uri_params,
		  &cfg->reg_contact_uri_params))
    {
	pj_strdup_with_null(acc->pool, &acc->cfg.reg_contact_uri_params,
			    &cfg->reg_contact_uri_params);
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Contact param */
    if (pj_strcmp(&acc->cfg.contact_params, &cfg->contact_params)) {
	pj_strdup_with_null(acc->pool, &acc->cfg.contact_params,
			    &cfg->contact_params);
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Contact URI params */
    if (pj_strcmp(&acc->cfg.contact_uri_params, &cfg->contact_uri_params)) {
	pj_strdup_with_null(acc->pool, &acc->cfg.contact_uri_params,
			    &cfg->contact_uri_params);
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Reliable provisional response */
    acc->cfg.require_100rel = cfg->require_100rel;

    /* Session timer */
    acc->cfg.use_timer = cfg->use_timer;
    acc->cfg.timer_setting = cfg->timer_setting;

    /* Transport */
    if (acc->cfg.transport_id != cfg->transport_id) {
	acc->cfg.transport_id = cfg->transport_id;

	if (acc->cfg.transport_id != PJSUA_INVALID_ID)
	    acc->tp_type = pjsua_var.tpdata[acc->cfg.transport_id].type;
	else
	    acc->tp_type = PJSIP_TRANSPORT_UNSPECIFIED;

	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Update keep-alive */
    if (acc->cfg.ka_interval != cfg->ka_interval ||
	pj_strcmp(&acc->cfg.ka_data, &cfg->ka_data))
    {
	pjsip_transport *ka_transport = acc->ka_transport;

	if (acc->ka_timer.id) {
	    pjsip_endpt_cancel_timer(pjsua_var.endpt, &acc->ka_timer);
	    acc->ka_timer.id = PJ_FALSE;
	}
	if (acc->ka_transport) {
	    pjsip_transport_dec_ref(acc->ka_transport);
	    acc->ka_transport = NULL;
	}

	acc->cfg.ka_interval = cfg->ka_interval;

	if (cfg->ka_interval) {
	    if (ka_transport) {
		/* Keep-alive has been running so we can just restart it */
		pj_time_val delay;

		pjsip_transport_add_ref(ka_transport);
		acc->ka_transport = ka_transport;

		acc->ka_timer.cb = &keep_alive_timer_cb;
		acc->ka_timer.user_data = (void*)acc;

		delay.sec = acc->cfg.ka_interval;
		delay.msec = 0;
		status = pjsua_schedule_timer(&acc->ka_timer, &delay);
		if (status == PJ_SUCCESS) {
		    acc->ka_timer.id = PJ_TRUE;
		} else {
		    pjsip_transport_dec_ref(ka_transport);
		    acc->ka_transport = NULL;
		    pjsua_perror(THIS_FILE, "Error starting keep-alive timer",
		                 status);
		}

	    } else {
		/* Keep-alive has not been running, we need to (re)register
		 * first.
		 */
		update_reg = PJ_TRUE;
	    }
	}
    }

    if (pj_strcmp(&acc->cfg.ka_data, &cfg->ka_data))
	pj_strdup(acc->pool, &acc->cfg.ka_data, &cfg->ka_data);
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    acc->cfg.use_srtp = cfg->use_srtp;
    acc->cfg.srtp_secure_signaling = cfg->srtp_secure_signaling;
    acc->cfg.srtp_optional_dup_offer = cfg->srtp_optional_dup_offer;    
#endif

#if defined(PJMEDIA_STREAM_ENABLE_KA) && (PJMEDIA_STREAM_ENABLE_KA != 0)
    acc->cfg.use_stream_ka = cfg->use_stream_ka;
#endif

    /* Use of proxy */
    if (acc->cfg.reg_use_proxy != cfg->reg_use_proxy) {
        acc->cfg.reg_use_proxy = cfg->reg_use_proxy;
        update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Global outbound proxy */
    if (global_route_crc != acc->global_route_crc) {
	unsigned i;
	pj_size_t rcnt;

	/* Remove the outbound proxies from the route set */
	rcnt = pj_list_size(&acc->route_set);
	for (i=0; i < rcnt - acc->cfg.proxy_cnt; ++i) {
	    pjsip_route_hdr *r = acc->route_set.next;
	    pj_list_erase(r);
	}

	/* Insert the outbound proxies to the beginning of route set */
	pj_list_merge_first(&acc->route_set, &global_route);

	/* Update global route CRC */
	acc->global_route_crc = global_route_crc;

	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Account proxy */
    if (local_route_crc != acc->local_route_crc) {
	unsigned i;

	/* Remove the current account proxies from the route set */
	for (i=0; i < acc->cfg.proxy_cnt; ++i) {
	    pjsip_route_hdr *r = acc->route_set.prev;
	    pj_list_erase(r);
	}

	/* Insert new proxy setting to the route set */
	pj_list_merge_last(&acc->route_set, &local_route);

	/* Update the proxy setting */
	acc->cfg.proxy_cnt = cfg->proxy_cnt;
	for (i = 0; i < cfg->proxy_cnt; ++i)
	    acc->cfg.proxy[i] = acc_proxy[i];

	/* Update local route CRC */
	acc->local_route_crc = local_route_crc;

	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Credential info */
    {
	unsigned i;
	pj_bool_t cred_changed = PJ_FALSE;

	/* Selective update credential info. */
	for (i = 0; i < cfg->cred_count; ++i) {
	    unsigned j;
	    pjsip_cred_info ci;

	    /* Find if this credential is already listed */
	    for (j = i; j < acc->cfg.cred_count; ++j) {
		if (pjsip_cred_info_cmp(&acc->cfg.cred_info[j], 
					&cfg->cred_info[i]) == 0)
		{
		    /* Found, but different index/position, swap */
		    if (j != i) {
			ci = acc->cfg.cred_info[i];
			acc->cfg.cred_info[i] = acc->cfg.cred_info[j];
			acc->cfg.cred_info[j] = ci;
		    }
		    break;
		}
	    }

	    /* Not found, insert this */
	    if (j == acc->cfg.cred_count) {
		cred_changed = PJ_TRUE;

		/* If account credential is full, discard the last one. */
		if (acc->cfg.cred_count == PJ_ARRAY_SIZE(acc->cfg.cred_info)) {
    		    pj_array_erase(acc->cfg.cred_info, sizeof(pjsip_cred_info),
				   acc->cfg.cred_count, acc->cfg.cred_count-1);
		    acc->cfg.cred_count--;
		}

		/* Insert this */
		pjsip_cred_info_dup(acc->pool, &ci, &cfg->cred_info[i]);
		pj_array_insert(acc->cfg.cred_info, sizeof(pjsip_cred_info),
				acc->cfg.cred_count, i, &ci);
	    }
	}
	acc->cfg.cred_count = cfg->cred_count;

	/* Concatenate credentials from account config and global config */
	acc->cred_cnt = 0;
	for (i=0; i<acc->cfg.cred_count; ++i) {
	    acc->cred[acc->cred_cnt++] = acc->cfg.cred_info[i];
	}
	for (i=0; i<pjsua_var.ua_cfg.cred_count && 
		  acc->cred_cnt < PJ_ARRAY_SIZE(acc->cred); ++i)
	{
	    acc->cred[acc->cred_cnt++] = pjsua_var.ua_cfg.cred_info[i];
	}

	if (cred_changed) {
	    update_reg = PJ_TRUE;
	    unreg_first = PJ_TRUE;
	}
    }

    /* Authentication preference */
    acc->cfg.auth_pref.initial_auth = cfg->auth_pref.initial_auth;
    if (pj_strcmp(&acc->cfg.auth_pref.algorithm, &cfg->auth_pref.algorithm)) {
	pj_strdup_with_null(acc->pool, &acc->cfg.auth_pref.algorithm, 
			    &cfg->auth_pref.algorithm);
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Registration */
    if (acc->cfg.reg_timeout != cfg->reg_timeout) {
	acc->cfg.reg_timeout = cfg->reg_timeout;
	if (acc->regc != NULL)
	    pjsip_regc_update_expires(acc->regc, acc->cfg.reg_timeout);

	update_reg = PJ_TRUE;
    }
    acc->cfg.unreg_timeout = cfg->unreg_timeout;
    acc->cfg.allow_contact_rewrite = cfg->allow_contact_rewrite;
    acc->cfg.reg_retry_interval = cfg->reg_retry_interval;
    acc->cfg.reg_first_retry_interval = cfg->reg_first_retry_interval;
    acc->cfg.reg_retry_random_interval = cfg->reg_retry_random_interval;    
    acc->cfg.drop_calls_on_reg_fail = cfg->drop_calls_on_reg_fail;
    acc->cfg.register_on_acc_add = cfg->register_on_acc_add;
    if (acc->cfg.reg_delay_before_refresh != cfg->reg_delay_before_refresh) {
        acc->cfg.reg_delay_before_refresh = cfg->reg_delay_before_refresh;
	if (acc->regc != NULL)
	    pjsip_regc_set_delay_before_refresh(acc->regc,
						cfg->reg_delay_before_refresh);
    }

    /* Allow via rewrite */
    if (acc->cfg.allow_via_rewrite != cfg->allow_via_rewrite) {
        if (acc->regc != NULL) {
            if (cfg->allow_via_rewrite) {
                pjsip_regc_set_via_sent_by(acc->regc, &acc->via_addr,
                                           acc->via_tp);
            } else
                pjsip_regc_set_via_sent_by(acc->regc, NULL, NULL);
        }
        if (acc->publish_sess != NULL) {
            if (cfg->allow_via_rewrite) {
                pjsip_publishc_set_via_sent_by(acc->publish_sess,
                                               &acc->via_addr, acc->via_tp);
            } else
                pjsip_publishc_set_via_sent_by(acc->publish_sess, NULL, NULL);
        }
        acc->cfg.allow_via_rewrite = cfg->allow_via_rewrite;
    }

    /* Normalize registration timeout and refresh delay */
    if (acc->cfg.reg_uri.slen ) {
        if (acc->cfg.reg_timeout == 0) {
            acc->cfg.reg_timeout = PJSUA_REG_INTERVAL;
        }
        if (acc->cfg.reg_delay_before_refresh == 0) {
	    acc->cfg.reg_delay_before_refresh =
                PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH;
        }
    }

    /* Registrar URI */
    if (pj_strcmp(&acc->cfg.reg_uri, &cfg->reg_uri)) {
	if (cfg->reg_uri.slen) {
	    pj_strdup_with_null(acc->pool, &acc->cfg.reg_uri, &cfg->reg_uri);
	    if (reg_sip_uri)
		acc->srv_port = reg_sip_uri->port;
	} 
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* SIP outbound setting */
    if (acc->cfg.use_rfc5626 != cfg->use_rfc5626 ||
	pj_strcmp(&acc->cfg.rfc5626_instance_id, &cfg->rfc5626_instance_id) ||
	pj_strcmp(&acc->cfg.rfc5626_reg_id, &cfg->rfc5626_reg_id))
    {	
	if (acc->cfg.use_rfc5626 != cfg->use_rfc5626)
	    acc->cfg.use_rfc5626 = cfg->use_rfc5626;

	if (pj_strcmp(&acc->cfg.rfc5626_instance_id, 
		      &cfg->rfc5626_instance_id)) 
	{
	    pj_strdup_with_null(acc->pool, &acc->cfg.rfc5626_instance_id,
				&cfg->rfc5626_instance_id);
	}
	if (pj_strcmp(&acc->cfg.rfc5626_reg_id, &cfg->rfc5626_reg_id)) {
	    pj_strdup_with_null(acc->pool, &acc->cfg.rfc5626_reg_id,
				&cfg->rfc5626_reg_id);
	}
	init_outbound_setting(acc);
	update_reg = PJ_TRUE;
	unreg_first = PJ_TRUE;
    }

    /* Video settings */
    acc->cfg.vid_in_auto_show = cfg->vid_in_auto_show;
    acc->cfg.vid_out_auto_transmit = cfg->vid_out_auto_transmit;
    acc->cfg.vid_wnd_flags = cfg->vid_wnd_flags;
    acc->cfg.vid_cap_dev = cfg->vid_cap_dev;
    acc->cfg.vid_rend_dev = cfg->vid_rend_dev;
    acc->cfg.vid_stream_rc_cfg = cfg->vid_stream_rc_cfg;

    /* Media settings */
    if (pj_stricmp(&acc->cfg.rtp_cfg.public_addr, &cfg->rtp_cfg.public_addr) ||
	pj_stricmp(&acc->cfg.rtp_cfg.bound_addr, &cfg->rtp_cfg.bound_addr))
    {
	pjsua_transport_config_dup(acc->pool, &acc->cfg.rtp_cfg,
				   &cfg->rtp_cfg);
    } else {
    	pj_str_t p_addr = acc->cfg.rtp_cfg.public_addr;
    	pj_str_t b_addr = acc->cfg.rtp_cfg.bound_addr;
    	
	/* ..to save memory by not using the pool */
	acc->cfg.rtp_cfg =  cfg->rtp_cfg;
	acc->cfg.rtp_cfg.public_addr = p_addr;
	acc->cfg.rtp_cfg.bound_addr = b_addr;
    }

    acc->cfg.nat64_opt = cfg->nat64_opt;
    acc->cfg.ipv6_media_use = cfg->ipv6_media_use;
    acc->cfg.enable_rtcp_mux = cfg->enable_rtcp_mux;
    acc->cfg.lock_codec = cfg->lock_codec;

    /* STUN and Media customization */
    if (acc->cfg.sip_stun_use != cfg->sip_stun_use) {
	acc->cfg.sip_stun_use = cfg->sip_stun_use;
	update_reg = PJ_TRUE;
    }
    acc->cfg.media_stun_use = cfg->media_stun_use;

    /* ICE settings */
    acc->cfg.ice_cfg_use = cfg->ice_cfg_use;
    switch (acc->cfg.ice_cfg_use) {
    case PJSUA_ICE_CONFIG_USE_DEFAULT:
	/* Copy ICE settings from media settings so that we don't need to
	 * check the media config if we look for ICE config.
	 */
	pjsua_ice_config_from_media_config(NULL, &acc->cfg.ice_cfg,
	                                &pjsua_var.media_cfg);
	break;
    case PJSUA_ICE_CONFIG_USE_CUSTOM:
	pjsua_ice_config_dup(acc->pool, &acc->cfg.ice_cfg, &cfg->ice_cfg);
	break;
    }

    /* TURN settings */
    acc->cfg.turn_cfg_use = cfg->turn_cfg_use;
    switch (acc->cfg.turn_cfg_use) {
    case PJSUA_TURN_CONFIG_USE_DEFAULT:
	/* Copy TURN settings from media settings so that we don't need to
	 * check the media config if we look for TURN config.
	 */
	pjsua_turn_config_from_media_config(NULL, &acc->cfg.turn_cfg,
	                                    &pjsua_var.media_cfg);
	break;
    case PJSUA_TURN_CONFIG_USE_CUSTOM:
	pjsua_turn_config_dup(acc->pool, &acc->cfg.turn_cfg,
	                      &cfg->turn_cfg);
	break;
    }

    acc->cfg.use_srtp = cfg->use_srtp;

    /* Call hold type */
    acc->cfg.call_hold_type = cfg->call_hold_type;

    /* Unregister first */
    if (unreg_first) {
	if (acc->regc) {
	    status = pjsua_acc_set_registration(acc->index, PJ_FALSE);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Ignored failure in unregistering the "
			     "old account setting in modifying account",
			     status);
		/* Not really sure if we should return error */
		status = PJ_SUCCESS;
	    }
	}
	if (acc->regc != NULL) {
	    pjsip_regc_destroy(acc->regc);
	    acc->regc = NULL;
	    acc->contact.slen = 0;
	    acc->reg_mapped_addr.slen = 0;
	    acc->rfc5626_status = OUTBOUND_UNKNOWN;
	}
	
	if (!cfg->reg_uri.slen) {
	    /* Reg URI still needed, delay unset after sending unregister. */
	    pj_bzero(&acc->cfg.reg_uri, sizeof(acc->cfg.reg_uri));
	}
    }

    /* Update registration */
    if (update_reg) {
	/* If accounts has registration enabled, start registration */
	if (acc->cfg.reg_uri.slen) {
	    status = pjsua_acc_set_registration(acc->index, PJ_TRUE);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Failed to register with new account "
			     "setting in modifying account", status);
		goto on_return;
	    }
	}
    }

    /* Update MWI subscription */
    if (update_mwi) {
	status = pjsua_start_mwi(acc_id, PJ_TRUE);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Failed in starting MWI subscription for "
			 "new account setting in modifying account", status);
	}
    }

    /* IP Change config */
    acc->cfg.ip_change_cfg.shutdown_tp = cfg->ip_change_cfg.shutdown_tp;
    acc->cfg.ip_change_cfg.hangup_calls = cfg->ip_change_cfg.hangup_calls;    
    acc->cfg.ip_change_cfg.reinvite_flags = cfg->ip_change_cfg.reinvite_flags;

    /* SRTP setting */
    pjsua_srtp_opt_dup(acc->pool, &acc->cfg.srtp_opt, &cfg->srtp_opt,
    		       PJ_TRUE);

    /* RTCP-FB config */
    pjmedia_rtcp_fb_setting_dup(acc->pool, &acc->cfg.rtcp_fb_cfg,
				&cfg->rtcp_fb_cfg);

on_return:
    PJSUA_UNLOCK();
    pj_log_pop_indent();
    return status;
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

    PJ_LOG(4,(THIS_FILE, "Acc %d: setting online status to %d..",
	      acc_id, is_online));
    pj_log_push_indent();

    pjsua_var.acc[acc_id].online_status = is_online;
    pj_bzero(&pjsua_var.acc[acc_id].rpid, sizeof(pjrpid_element));
    pjsua_pres_update_acc(acc_id, PJ_FALSE);

    pj_log_pop_indent();
    return PJ_SUCCESS;
}


/* 
 * Set online status with extended information 
 */
PJ_DEF(pj_status_t) pjsua_acc_set_online_status2( pjsua_acc_id acc_id,
						  pj_bool_t is_online,
						  const pjrpid_element *pr)
{
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    PJ_LOG(4,(THIS_FILE, "Acc %d: setting online status to %d..",
    	      acc_id, is_online));
    pj_log_push_indent();

    PJSUA_LOCK();
    pjsua_var.acc[acc_id].online_status = is_online;
    pjrpid_element_dup(pjsua_var.acc[acc_id].pool, &pjsua_var.acc[acc_id].rpid, pr);
    PJSUA_UNLOCK();

    pjsua_pres_update_acc(acc_id, PJ_TRUE);
    pj_log_pop_indent();

    return PJ_SUCCESS;
}

/* Create reg_contact, adding SIP outbound params and other REGISTER specific
 * Contact params, i.e: reg_contact_params, reg_contact_uri_params.
 */
static void update_regc_contact(pjsua_acc *acc)
{
    pjsua_acc_config *acc_cfg = &acc->cfg;
    pj_bool_t need_outbound = PJ_FALSE;
    const pj_str_t tcp_param = pj_str(";transport=tcp");
    const pj_str_t tls_param = pj_str(";transport=tls");

    if (!acc_cfg->use_rfc5626)
	goto done;

    /* Check if outbound has been requested and rejected */
    if (acc->rfc5626_status == OUTBOUND_NA)
	goto done;

    if (pj_stristr(&acc->contact, &tcp_param)==NULL &&
	pj_stristr(&acc->contact, &tls_param)==NULL)
    {
	/* Currently we can only do SIP outbound for TCP
	 * and TLS.
	 */
	goto done;
    }

    /* looks like we can use outbound */
    need_outbound = PJ_TRUE;

done:
    {
	pj_ssize_t len;
	pj_str_t reg_contact;

	acc->rfc5626_status = OUTBOUND_WANTED;
	len = acc->contact.slen +
	      acc->cfg.contact_params.slen +
	      acc->cfg.reg_contact_params.slen +
	      acc->cfg.reg_contact_uri_params.slen +
	      (need_outbound?
	       (acc->rfc5626_instprm.slen + acc->rfc5626_regprm.slen): 0);
	if (len > acc->contact.slen) {
	    reg_contact.ptr = (char*) pj_pool_alloc(acc->pool, len);

	    pj_strcpy(&reg_contact, &acc->contact);
	
	    /* Contact URI params */
	    if (acc->cfg.reg_contact_uri_params.slen) {
		pj_pool_t *pool;
		pjsip_contact_hdr *contact_hdr;
		pjsip_sip_uri *uri;
		pj_str_t uri_param = acc->cfg.reg_contact_uri_params;
		const pj_str_t STR_CONTACT = { "Contact", 7 };
		char tmp_uri[PJSIP_MAX_URL_SIZE];
		pj_ssize_t tmp_len;

		/* Get the URI string */
		pool = pjsua_pool_create("tmp", 512, 512);
		contact_hdr = (pjsip_contact_hdr*)
			      pjsip_parse_hdr(pool, &STR_CONTACT,
					      reg_contact.ptr,
					      reg_contact.slen, NULL);
		pj_assert(contact_hdr != NULL);
		uri = (pjsip_sip_uri*) contact_hdr->uri;
		pj_assert(uri != NULL);
		uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri);
		tmp_len = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
					  uri, tmp_uri,
					  sizeof(tmp_uri));
		pj_assert(tmp_len > 0);
		pj_pool_release(pool);

		/* Regenerate Contact */
		reg_contact.slen = pj_ansi_snprintf(
					    reg_contact.ptr, len,
					    "<%.*s%.*s>%.*s",
					    (int)tmp_len, tmp_uri,
					    (int)uri_param.slen, uri_param.ptr,
					    (int)acc->cfg.contact_params.slen,
					    acc->cfg.contact_params.ptr);
		pj_assert(reg_contact.slen > 0);
	    }

	    /* Outbound */
    	    if (need_outbound) {
    	    	acc->rfc5626_status = OUTBOUND_WANTED;

	    	/* Need to use outbound, append the contact with
	    	 * +sip.instance and reg-id parameters.
	     	 */
	    	pj_strcat(&reg_contact, &acc->rfc5626_regprm);
	    	pj_strcat(&reg_contact, &acc->rfc5626_instprm);
	    } else {
	    	acc->rfc5626_status = OUTBOUND_NA;
	    }

	    /* Contact params */
	    pj_strcat(&reg_contact, &acc->cfg.reg_contact_params);
	    
	    acc->reg_contact = reg_contact;

	    PJ_LOG(4,(THIS_FILE,
		      "Contact for acc %d updated: %.*s",
		      acc->index,
		      (int)acc->reg_contact.slen,
		      acc->reg_contact.ptr));

	} else {
	     /* Outbound is not needed/wanted for the account and there's
	      * no custom registration Contact params. acc->reg_contact
	      * is set to the same as acc->contact.
	      */
	     acc->reg_contact = acc->contact;
	     acc->rfc5626_status = OUTBOUND_NA;
	}
    }
}

/* Check if IP is private IP address */
static pj_bool_t is_private_ip(const pj_str_t *addr)
{
    const pj_str_t private_net[] = 
    {
	{ "10.", 3 },
	{ "127.", 4 },
	{ "172.16.", 7 }, { "172.17.", 7 }, { "172.18.", 7 }, { "172.19.", 7 },
        { "172.20.", 7 }, { "172.21.", 7 }, { "172.22.", 7 }, { "172.23.", 7 },
        { "172.24.", 7 }, { "172.25.", 7 }, { "172.26.", 7 }, { "172.27.", 7 },
        { "172.28.", 7 }, { "172.29.", 7 }, { "172.30.", 7 }, { "172.31.", 7 },
	{ "192.168.", 8 }
    };
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(private_net); ++i) {
	if (pj_strncmp(addr, &private_net[i], private_net[i].slen)==0)
	    return PJ_TRUE;
    }

    return PJ_FALSE;
}

/* Update NAT address from the REGISTER response */
static pj_bool_t acc_check_nat_addr(pjsua_acc *acc,
                                    int contact_rewrite_method,
				    struct pjsip_regc_cbparam *param)
{
    pjsip_transport *tp;
    const pj_str_t *via_addr;
    pj_pool_t *pool;
    int rport;
    pjsip_sip_uri *uri;
    pjsip_via_hdr *via;
    pj_sockaddr contact_addr;
    pj_sockaddr recv_addr = {{0}};
    pj_status_t status;
    pj_bool_t matched;
    pj_str_t srv_ip;
    pjsip_contact_hdr *contact_hdr;
    char host_addr_buf[PJ_INET6_ADDRSTRLEN+10];
    char via_addr_buf[PJ_INET6_ADDRSTRLEN+10];
    const pj_str_t STR_CONTACT = { "Contact", 7 };

    tp = param->rdata->tp_info.transport;

    /* Get the received and rport info */
    via = param->rdata->msg_info.via;
    if (via->rport_param < 1) {
	/* Remote doesn't support rport */
	rport = via->sent_by.port;
	if (rport==0) {
	    pjsip_transport_type_e tp_type;
	    tp_type = (pjsip_transport_type_e) tp->key.type;
	    rport = pjsip_transport_get_default_port_for_type(tp_type);
	}
    } else
	rport = via->rport_param;

    if (via->recvd_param.slen != 0)
        via_addr = &via->recvd_param;
    else
        via_addr = &via->sent_by.host;

    /* If allow_via_rewrite is enabled, we save the Via "received" address
     * from the response, if either of the following condition is met:
     *  - the Via "received" address differs from saved one (or we haven't
     *    saved any yet)
     *  - transport is different
     *  - only the port has changed, AND either the received address is
     *    public IP or allow_contact_rewrite is 2
     */
    if (acc->cfg.allow_via_rewrite &&
        (pj_strcmp(&acc->via_addr.host, via_addr) || acc->via_tp != tp ||
         (acc->via_addr.port != rport &&
           (!is_private_ip(via_addr) || acc->cfg.allow_contact_rewrite == 2))))
    {
        if (pj_strcmp(&acc->via_addr.host, via_addr))
            pj_strdup(acc->pool, &acc->via_addr.host, via_addr);
        acc->via_addr.port = rport;
        acc->via_tp = tp;
        pjsip_regc_set_via_sent_by(acc->regc, &acc->via_addr, acc->via_tp);
        if (acc->publish_sess != NULL) {
                pjsip_publishc_set_via_sent_by(acc->publish_sess,
                                               &acc->via_addr, acc->via_tp);
        }
    }

    /* Save mapped address if needed */
    if (acc->cfg.allow_sdp_nat_rewrite &&
	pj_strcmp(&acc->reg_mapped_addr, via_addr))
    {
	pj_strdup(acc->pool, &acc->reg_mapped_addr, via_addr);
    }

    /* Only update if account is configured to auto-update */
    if (acc->cfg.allow_contact_rewrite == PJ_FALSE)
	return PJ_FALSE;

    /* If SIP outbound is active, no need to update */
    if (acc->rfc5626_status == OUTBOUND_ACTIVE) {
	PJ_LOG(4,(THIS_FILE, "Acc %d has SIP outbound active, no need to "
			     "update registration Contact", acc->index));
	return PJ_FALSE;
    }

#if 0
    // Always update
    // See http://lists.pjsip.org/pipermail/pjsip_lists.pjsip.org/2008-March/002178.html

    /* For UDP, only update if STUN is enabled (for now).
     * For TCP/TLS, always check.
     */
    if ((tp->key.type == PJSIP_TRANSPORT_UDP &&
	 (pjsua_var.ua_cfg.stun_domain.slen != 0 ||
	 (pjsua_var.ua_cfg.stun_host.slen != 0))  ||
	(tp->key.type == PJSIP_TRANSPORT_TCP) ||
	(tp->key.type == PJSIP_TRANSPORT_TLS))
    {
	/* Yes we will check */
    } else {
	return PJ_FALSE;
    }
#endif

    /* Compare received and rport with the URI in our registration */
    pool = pjsua_pool_create("tmp", 512, 512);
    contact_hdr = (pjsip_contact_hdr*)
		  pjsip_parse_hdr(pool, &STR_CONTACT, acc->contact.ptr, 
				  acc->contact.slen, NULL);
    pj_assert(contact_hdr != NULL);
    uri = (pjsip_sip_uri*) contact_hdr->uri;
    pj_assert(uri != NULL);
    uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri);

    if (uri->port == 0) {
	pjsip_transport_type_e tp_type;
	tp_type = (pjsip_transport_type_e) tp->key.type;
	uri->port = pjsip_transport_get_default_port_for_type(tp_type);
    }

    /* Convert IP address strings into sockaddr for comparison.
     * (http://trac.pjsip.org/repos/ticket/863)
     */
    status = pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &uri->host, 
			       &contact_addr);
    if (status == PJ_SUCCESS)
	status = pj_sockaddr_parse(pj_AF_UNSPEC(), 0, via_addr, 
				   &recv_addr);
    if (status == PJ_SUCCESS) {
	/* Compare the addresses as sockaddr according to the ticket above,
	 * but only if they have the same family (ipv4 vs ipv4, or
	 * ipv6 vs ipv6).
	 * Checking for the same address family is currently disabled,
	 * since it can be useful in cases such as when on NAT64,
	 * in order to get the IPv4-mapped address from IPv6.
	 */
	matched = //(contact_addr.addr.sa_family != recv_addr.addr.sa_family)||
	          (uri->port == rport &&
		   pj_sockaddr_cmp(&contact_addr, &recv_addr)==0);
    } else {
	/* Compare the addresses as string, as before */
	matched = (uri->port == rport &&
		   pj_stricmp(&uri->host, via_addr)==0);
    }

    if (matched) {
	/* Address doesn't change */
	pj_pool_release(pool);
	return PJ_FALSE;
    }

    /* Get server IP */
    srv_ip = pj_str(param->rdata->pkt_info.src_name);

    /* At this point we've detected that the address as seen by registrar.
     * has changed.
     */

    /* Do not switch if both Contact and server's IP address are
     * public but response contains private IP. A NAT in the middle
     * might have messed up with the SIP packets. See:
     * http://trac.pjsip.org/repos/ticket/643
     *
     * This exception can be disabled by setting allow_contact_rewrite
     * to 2. In this case, the switch will always be done whenever there
     * is difference in the IP address in the response.
     */
    if (acc->cfg.allow_contact_rewrite != 2 && !is_private_ip(&uri->host) &&
	!is_private_ip(&srv_ip) && is_private_ip(via_addr))
    {
	/* Don't switch */
	pj_pool_release(pool);
	return PJ_FALSE;
    }

    /* Also don't switch if only the port number part is different, and
     * the Via received address is private.
     * See http://trac.pjsip.org/repos/ticket/864
     */
    if (acc->cfg.allow_contact_rewrite != 2 &&
	pj_sockaddr_cmp(&contact_addr, &recv_addr)==0 &&
	is_private_ip(via_addr))
    {
	/* Don't switch */
	pj_pool_release(pool);
	return PJ_FALSE;
    }
    pj_addr_str_print(&uri->host, uri->port, host_addr_buf, 
		      sizeof(host_addr_buf), 1);
    pj_addr_str_print(via_addr, rport, via_addr_buf, 
		      sizeof(via_addr_buf), 1);
    PJ_LOG(3,(THIS_FILE, "IP address change detected for account %d "
			 "(%s --> %s). Updating registration "
			 "(using method %d)",
			 acc->index, host_addr_buf, via_addr_buf,
			 contact_rewrite_method));

    pj_assert(contact_rewrite_method == PJSUA_CONTACT_REWRITE_UNREGISTER ||
	      contact_rewrite_method == PJSUA_CONTACT_REWRITE_NO_UNREG ||
              contact_rewrite_method == PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE);

    if (contact_rewrite_method == PJSUA_CONTACT_REWRITE_UNREGISTER) {
	/* Unregister current contact */
	pjsua_acc_set_registration(acc->index, PJ_FALSE);
	if (acc->regc != NULL) {
	    pjsip_regc_destroy(acc->regc);
	    acc->regc = NULL;
	    acc->contact.slen = 0;
	}
    }

    /*
     * Build new Contact header
     */
    {
	const char *ob = ";ob";
	char *tmp;
	const char *beginquote, *endquote;
	char transport_param[32];
	int len;
	pj_bool_t secure;

	secure = pjsip_transport_get_flag_from_type(tp->key.type) &
		 PJSIP_TRANSPORT_SECURE;
  	
        /* Enclose IPv6 address in square brackets */
        if (tp->key.type & PJSIP_TRANSPORT_IPV6) {
            beginquote = "[";
            endquote = "]";
        } else {
            beginquote = endquote = "";
        }

	/* Don't add transport parameter if it's UDP */
	if (tp->key.type != PJSIP_TRANSPORT_UDP &&
		tp->key.type != PJSIP_TRANSPORT_UDP6)
	{
	    pj_ansi_snprintf(transport_param, sizeof(transport_param),
			     ";transport=%s",
			     pjsip_transport_get_type_name(
				     (pjsip_transport_type_e)tp->key.type));
	} else {
	    transport_param[0] = '\0';
	}

	tmp = (char*) pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
	len = pj_ansi_snprintf(tmp, PJSIP_MAX_URL_SIZE,
			       "<%s:%.*s%s%s%.*s%s:%d%s%.*s%s>%.*s",
			       ((secure && acc->is_sips)? "sips" : "sip"),
			       (int)acc->user_part.slen,
			       acc->user_part.ptr,
			       (acc->user_part.slen? "@" : ""),
			       beginquote,
			       (int)via_addr->slen,
			       via_addr->ptr,
			       endquote,
			       rport,
			       transport_param,
			       (int)acc->cfg.contact_uri_params.slen,
			       acc->cfg.contact_uri_params.ptr,
			       (acc->cfg.use_rfc5626? ob: ""),
			       (int)acc->cfg.contact_params.slen,
			       acc->cfg.contact_params.ptr);
	if (len < 1 || len >= PJSIP_MAX_URL_SIZE) {
	    PJ_LOG(1,(THIS_FILE, "URI too long"));
	    pj_pool_release(pool);
	    return PJ_FALSE;
	}
	pj_strdup2_with_null(acc->pool, &acc->contact, tmp);

	update_regc_contact(acc);

	/* Always update, by http://trac.pjsip.org/repos/ticket/864. */
        /* Since the Via address will now be overwritten to the correct
         * address by https://trac.pjsip.org/repos/ticket/1537, we do
         * not need to update the transport address.
         */
        /*
	pj_strdup_with_null(tp->pool, &tp->local_name.host, via_addr);
	tp->local_name.port = rport;
         */

    }

    if (contact_rewrite_method == PJSUA_CONTACT_REWRITE_NO_UNREG &&
        acc->regc != NULL)
    {
	pjsip_regc_update_contact(acc->regc, 1, &acc->reg_contact);
    }

    /* Perform new registration */
    if (contact_rewrite_method < PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE) {
        pjsua_acc_set_registration(acc->index, PJ_TRUE);
    }

    pj_pool_release(pool);

    return PJ_TRUE;
}

/* Check and update Service-Route header */
static void update_service_route(pjsua_acc *acc, pjsip_rx_data *rdata)
{
    pjsip_generic_string_hdr *hsr = NULL;
    pjsip_route_hdr *hr, *h;
    const pj_str_t HNAME = { "Service-Route", 13 };
    const pj_str_t HROUTE = { "Route", 5 };
    pjsip_uri *uri[PJSUA_ACC_MAX_PROXIES];
    unsigned i, uri_cnt = 0;
    pj_size_t rcnt;

    /* Find and parse Service-Route headers */
    for (;;) {
	char saved;
	int parsed_len;

	/* Find Service-Route header */
	hsr = (pjsip_generic_string_hdr*)
	      pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &HNAME, hsr);
	if (!hsr)
	    break;

	/* Parse as Route header since the syntax is similar. This may
	 * return more than one headers.
	 */
	saved = hsr->hvalue.ptr[hsr->hvalue.slen];
	hsr->hvalue.ptr[hsr->hvalue.slen] = '\0';
	hr = (pjsip_route_hdr*)
	     pjsip_parse_hdr(rdata->tp_info.pool, &HROUTE, hsr->hvalue.ptr,
			     hsr->hvalue.slen, &parsed_len);
	hsr->hvalue.ptr[hsr->hvalue.slen] = saved;

	if (hr == NULL) {
	    /* Error */
	    PJ_LOG(1,(THIS_FILE, "Error parsing Service-Route header"));
	    return;
	}

	/* Save each URI in the result */
	h = hr;
	do {
	    if (!PJSIP_URI_SCHEME_IS_SIP(h->name_addr.uri) &&
		!PJSIP_URI_SCHEME_IS_SIPS(h->name_addr.uri))
	    {
		PJ_LOG(1,(THIS_FILE,"Error: non SIP URI in Service-Route: %.*s",
			  (int)hsr->hvalue.slen, hsr->hvalue.ptr));
		return;
	    }

	    uri[uri_cnt++] = h->name_addr.uri;
	    h = h->next;
	} while (h != hr && uri_cnt != PJ_ARRAY_SIZE(uri));

	if (h != hr) {
	    PJ_LOG(1,(THIS_FILE, "Error: too many Service-Route headers"));
	    return;
	}

	/* Prepare to find next Service-Route header */
	hsr = hsr->next;
	if ((void*)hsr == (void*)&rdata->msg_info.msg->hdr)
	    break;
    }

    if (uri_cnt == 0)
	return;

    /* 
     * Update account's route set 
     */
    
    /* First remove all routes which are not the outbound proxies */
    rcnt = pj_list_size(&acc->route_set);
    if (rcnt != pjsua_var.ua_cfg.outbound_proxy_cnt + acc->cfg.proxy_cnt) {
	for (i=pjsua_var.ua_cfg.outbound_proxy_cnt + acc->cfg.proxy_cnt, 
		hr=acc->route_set.prev; 
	     i<rcnt; 
	     ++i)
	 {
	    pjsip_route_hdr *prev = hr->prev;
	    pj_list_erase(hr);
	    hr = prev;
	 }
    }

    /* Then append the Service-Route URIs */
    for (i=0; i<uri_cnt; ++i) {
	hr = pjsip_route_hdr_create(acc->pool);
	hr->name_addr.uri = (pjsip_uri*)pjsip_uri_clone(acc->pool, uri[i]);
	pj_list_push_back(&acc->route_set, hr);
    }

    /* Done */

    PJ_LOG(4,(THIS_FILE, "Service-Route updated for acc %d with %d URI(s)",
	      acc->index, uri_cnt));
}

/* Keep alive timer callback */
static void keep_alive_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
    pjsua_acc *acc;
    pjsip_tpselector tp_sel;
    pj_time_val delay;
    char addrtxt[PJ_INET6_ADDRSTRLEN];
    pj_status_t status;
    unsigned ka_timer;
    unsigned lower_bound;

    PJ_UNUSED_ARG(th);

    PJSUA_LOCK();

    te->id = PJ_FALSE;

    acc = (pjsua_acc*) te->user_data;

    /* Check if the account is still active. It might have just been deleted
     * while the keep-alive timer was about to be called (race condition).
     */
    if (acc->ka_transport == NULL)
	goto on_return;

    /* Select the transport to send the packet */
    pj_bzero(&tp_sel, sizeof(tp_sel));
    tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
    tp_sel.u.transport = acc->ka_transport;

    PJ_LOG(5,(THIS_FILE, 
	      "Sending %d bytes keep-alive packet for acc %d to %s",
	      acc->cfg.ka_data.slen, acc->index,
	      pj_sockaddr_print(&acc->ka_target, addrtxt, sizeof(addrtxt),3)));

    /* Send raw packet */
    status = pjsip_tpmgr_send_raw(pjsip_endpt_get_tpmgr(pjsua_var.endpt),
				  acc->ka_transport->key.type, &tp_sel,
				  NULL, acc->cfg.ka_data.ptr, 
				  acc->cfg.ka_data.slen, 
				  &acc->ka_target, acc->ka_target_len,
				  NULL, NULL);

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	pjsua_perror(THIS_FILE, "Error sending keep-alive packet", status);
    }

    /* Check just in case keep-alive has been disabled. This shouldn't happen
     * though as when ka_interval is changed this timer should have been
     * cancelled.
     *
     * Also check if Flow Timer (rfc5626) is not set.
     */
    if (acc->cfg.ka_interval == 0 && acc->rfc5626_flowtmr == 0)
	goto on_return;

    ka_timer = acc->rfc5626_flowtmr ? acc->rfc5626_flowtmr :
				      acc->cfg.ka_interval;

    lower_bound = (unsigned)((float)ka_timer * 0.8f);
    delay.sec = pj_rand() % (ka_timer - lower_bound) + lower_bound;
    delay.msec = 0;

    /* Reschedule next timer */
    status = pjsip_endpt_schedule_timer(pjsua_var.endpt, te, &delay);
    if (status == PJ_SUCCESS) {
	te->id = PJ_TRUE;
    } else {
	pjsua_perror(THIS_FILE, "Error starting keep-alive timer", status);
    }

on_return:
    PJSUA_UNLOCK();
}


/* Update keep-alive for the account */
static void update_keep_alive(pjsua_acc *acc, pj_bool_t start,
			      struct pjsip_regc_cbparam *param)
{
    /* In all cases, stop keep-alive timer if it's running. */
    if (acc->ka_timer.id) {
	pjsip_endpt_cancel_timer(pjsua_var.endpt, &acc->ka_timer);
	acc->ka_timer.id = PJ_FALSE;

	if (acc->ka_transport) {
	    pjsip_transport_dec_ref(acc->ka_transport);
	    acc->ka_transport = NULL;
	}
    }

    if (start) {
	pj_time_val delay;
	pj_status_t status;
	pjsip_generic_string_hdr *hsr = NULL;
	unsigned ka_timer;
	unsigned lower_bound;

	static const pj_str_t STR_FLOW_TIMER  = { "Flow-Timer", 10 };

	hsr = (pjsip_generic_string_hdr*)
	      pjsip_msg_find_hdr_by_name(param->rdata->msg_info.msg,
					 &STR_FLOW_TIMER, hsr);

	if (hsr) {	    
	    acc->rfc5626_flowtmr = pj_strtoul(&hsr->hvalue);
	}

	/* Only do keep-alive if:
	 *  - REGISTER response contain Flow-Timer header, otherwise
	 *  - ka_interval is not zero in the account, and
	 *  - transport is UDP.
	 *
	 * Previously we only enabled keep-alive when STUN is enabled, since
	 * we thought that keep-alive is only needed in Internet situation.
	 * But it has been discovered that Windows Firewall on WinXP also
	 * needs to be kept-alive, otherwise incoming packets will be dropped.
	 * So because of this, now keep-alive is always enabled for UDP,
	 * regardless of whether STUN is enabled or not.
	 *
	 * Note that this applies only for UDP. For TCP/TLS, the keep-alive
	 * is done by the transport layer.
	 */
	if (/*pjsua_var.stun_srv.ipv4.sin_family == 0 ||*/
	    ((acc->cfg.ka_interval == 0) && (acc->rfc5626_flowtmr == 0)) ||
	    (!hsr && ((param->rdata->tp_info.transport->key.type &
		       ~PJSIP_TRANSPORT_IPV6) != PJSIP_TRANSPORT_UDP)))
	{
	    /* Keep alive is not necessary */
	    return;
	}

	/* Save transport and destination address. */
	acc->ka_transport = param->rdata->tp_info.transport;
	pjsip_transport_add_ref(acc->ka_transport);

	/* https://trac.pjsip.org/repos/ticket/1607:
	 * Calculate the destination address from the original request. Some
	 * (broken) servers send the response using different source address
	 * than the one that receives the request, which is forbidden by RFC
	 * 3581.
	 */
	{
	    pjsip_transaction *tsx;
	    pjsip_tx_data *req;

	    tsx = pjsip_rdata_get_tsx(param->rdata);
	    PJ_ASSERT_ON_FAIL(tsx, return);

	    req = tsx->last_tx;

	    pj_memcpy(&acc->ka_target, &req->tp_info.dst_addr,
	              req->tp_info.dst_addr_len);
	    acc->ka_target_len = req->tp_info.dst_addr_len;
	}

	/* Setup and start the timer */
	acc->ka_timer.cb = &keep_alive_timer_cb;
	acc->ka_timer.user_data = (void*)acc;

	ka_timer = acc->rfc5626_flowtmr ? acc->rfc5626_flowtmr :
					  acc->cfg.ka_interval;

	lower_bound = (unsigned)((float)ka_timer * 0.8f);
	delay.sec = pj_rand() % (ka_timer - lower_bound) + lower_bound;
	delay.msec = 0;
	status = pjsip_endpt_schedule_timer(pjsua_var.endpt, &acc->ka_timer, 
					    &delay);
	if (status == PJ_SUCCESS) {
	    char addr[PJ_INET6_ADDRSTRLEN+10];
	    pj_str_t input_str = pj_str(param->rdata->pkt_info.src_name);
	    acc->ka_timer.id = PJ_TRUE;

	    pj_addr_str_print(&input_str, param->rdata->pkt_info.src_port, 
			      addr, sizeof(addr), 1);
	    PJ_LOG(4,(THIS_FILE, "Keep-alive timer started for acc %d, "
				 "destination:%s, interval:%ds",
				 acc->index, addr, delay.sec));
	} else {
	    acc->ka_timer.id = PJ_FALSE;
	    pjsip_transport_dec_ref(acc->ka_transport);
	    acc->ka_transport = NULL;
	    pjsua_perror(THIS_FILE, "Error starting keep-alive timer", status);
	}
    }
}


/* Update the status of SIP outbound registration request */
static void update_rfc5626_status(pjsua_acc *acc, pjsip_rx_data *rdata)
{
    pjsip_require_hdr *hreq;
    const pj_str_t STR_OUTBOUND = {"outbound", 8};
    unsigned i;

    if (acc->rfc5626_status == OUTBOUND_UNKNOWN) {
	goto on_return;
    }

    hreq = rdata->msg_info.require;
    if (!hreq) {
	acc->rfc5626_status = OUTBOUND_NA;
	goto on_return;
    }

    for (i=0; i<hreq->count; ++i) {
	if (pj_stricmp(&hreq->values[i], &STR_OUTBOUND)==0) {
	    acc->rfc5626_status = OUTBOUND_ACTIVE;
	    goto on_return;
	}
    }

    /* Server does not support outbound */
    acc->rfc5626_status = OUTBOUND_NA;

on_return:
    if (acc->rfc5626_status != OUTBOUND_ACTIVE) {
	acc->reg_contact = acc->contact;
    }
    PJ_LOG(4,(THIS_FILE, "SIP outbound status for acc %d is %s",
			 acc->index, (acc->rfc5626_status==OUTBOUND_ACTIVE?
					 "active": "not active")));
}

static void regc_tsx_cb(struct pjsip_regc_tsx_cb_param *param)
{
    pjsua_acc *acc = (pjsua_acc*) param->cbparam.token;

    PJSUA_LOCK();

    if (param->cbparam.regc != acc->regc) {
        PJSUA_UNLOCK();
	return;
    }

    pj_log_push_indent();

    /* Check if we should do NAT bound address check for contact rewrite.
     * Note that '!contact_rewritten' check here is to avoid overriding
     * the current contact generated from last 2xx.
     */
    if (!acc->contact_rewritten &&
	(acc->cfg.contact_rewrite_method &
         PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE) ==
        PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE &&
        param->cbparam.code >= 400 &&
        param->cbparam.rdata)
    {
        if (acc_check_nat_addr(acc, PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE,
                               &param->cbparam))
        {
            param->contact_cnt = 1;
            param->contact[0] = acc->reg_contact;

	    /* Don't set 'contact_rewritten' to PJ_TRUE here to allow
	     * further check of NAT bound address in 2xx response.
	     */
        }
    }

    PJSUA_UNLOCK();
    pj_log_pop_indent();
}

/*
 * Timer callback to handle call on IP change process.
 */
static void handle_call_on_ip_change_cb(void *user_data)
{
    pjsua_acc *acc = (pjsua_acc*)user_data;
    pjsua_acc_handle_call_on_ip_change(acc);
}

/*
 * This callback is called by pjsip_regc when outgoing register
 * request has completed.
 */
static void regc_cb(struct pjsip_regc_cbparam *param)
{

    pjsua_acc *acc = (pjsua_acc*) param->token;

    PJSUA_LOCK();

    if (param->regc != acc->regc) {
        PJSUA_UNLOCK();
	return;
    }

    pj_log_push_indent();

    /*
     * Print registration status.
     */
    if (param->status!=PJ_SUCCESS) {
    	pj_status_t status;

	pjsua_perror(THIS_FILE, "SIP registration error", 
		     param->status);

	if (param->status == PJSIP_EBUSY) {
	    pj_log_pop_indent();
            PJSUA_UNLOCK();
	    return;
	}

	/* This callback is called without holding the registration's lock,
	 * so there can be a race condition with another registration
	 * process. Therefore, we must not forcefully try to destroy
	 * the registration here.
	 */
	status = pjsip_regc_destroy2(acc->regc, PJ_FALSE);
	if (status == PJ_SUCCESS) {
	    acc->regc = NULL;
	    acc->contact.slen = 0;
	    acc->reg_mapped_addr.slen = 0;
	    acc->rfc5626_status = OUTBOUND_UNKNOWN;
	    acc->rfc5626_flowtmr = 0;
	
	    /* Stop keep-alive timer if any. */
	    update_keep_alive(acc, PJ_FALSE, NULL);
	} else {
	    /* Another registration is in progress. */
	    pj_assert(status == PJ_EBUSY);
	    pjsua_perror(THIS_FILE, "Deleting registration failed", 
		     	 status);	    
	}

    } else if (param->code < 0 || param->code >= 300) {
	PJ_LOG(2, (THIS_FILE, "SIP registration failed, status=%d (%.*s)", 
		   param->code, 
		   (int)param->reason.slen, param->reason.ptr));
	pjsip_regc_destroy(acc->regc);
	acc->regc = NULL;
	acc->contact.slen = 0;
	acc->reg_mapped_addr.slen = 0;
	acc->rfc5626_status = OUTBOUND_UNKNOWN;
	acc->rfc5626_flowtmr = 0;

	/* Stop keep-alive timer if any. */
	update_keep_alive(acc, PJ_FALSE, NULL);

    } else if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {

	/* Update auto registration flag */
	acc->auto_rereg.active = PJ_FALSE;
	acc->auto_rereg.attempt_cnt = 0;

	if (param->expiration < 1) {
	    pjsip_regc_destroy(acc->regc);
	    acc->regc = NULL;
	    acc->contact.slen = 0;
	    acc->reg_mapped_addr.slen = 0;
	    acc->rfc5626_status = OUTBOUND_UNKNOWN;
	    acc->rfc5626_flowtmr = 0;

	    /* Reset pointer to registration transport */
	    //acc->auto_rereg.reg_tp = NULL;

	    /* Stop keep-alive timer if any. */
	    update_keep_alive(acc, PJ_FALSE, NULL);

	    PJ_LOG(3,(THIS_FILE, "%s: unregistration success",
		      pjsua_var.acc[acc->index].cfg.id.ptr));

	} else {	    
	    /* Check and update SIP outbound status first, since the result
	     * will determine if we should update re-registration
	     */
	    update_rfc5626_status(acc, param->rdata);

	    /* Check NAT bound address if it hasn't been done before */
            if (!acc->contact_rewritten &&
		acc_check_nat_addr(acc, (acc->cfg.contact_rewrite_method & 3),
                                   param))
            {
		PJSUA_UNLOCK();
		pj_log_pop_indent();

		/* Avoid another check of NAT bound address */
		acc->contact_rewritten = PJ_TRUE;
		return;
	    }

	    /* Check and update Service-Route header */
	    update_service_route(acc, param->rdata);

#if         PJSUA_REG_AUTO_REG_REFRESH

            PJ_LOG(3, (THIS_FILE,
                        "%s: registration success, status=%d (%.*s), "
                        "will re-register in %d seconds",
                        pjsua_var.acc[acc->index].cfg.id.ptr,
                        param->code,
                        (int)param->reason.slen, param->reason.ptr,
                        param->expiration));

#else

            PJ_LOG(3, (THIS_FILE,
                        "%s: registration success, status=%d (%.*s), "
                        "auto re-register disabled",
                        pjsua_var.acc[acc->index].cfg.id.ptr,
                        param->code,
                        (int)param->reason.slen, param->reason.ptr));

#endif
	    /* Start keep-alive timer if necessary. */
	    update_keep_alive(acc, PJ_TRUE, param);

	    /* Send initial PUBLISH if it is enabled */
	    if (acc->cfg.publish_enabled && acc->publish_sess==NULL)
		pjsua_pres_init_publish_acc(acc->index);

	    /* Subscribe to MWI, if it's enabled */
	    if (acc->cfg.mwi_enabled)
		pjsua_start_mwi(acc->index, PJ_FALSE);

	}
    } else {
	PJ_LOG(4, (THIS_FILE, "SIP registration updated status=%d", param->code));
    }

    acc->reg_last_err = param->status;
    acc->reg_last_code = param->code;

    /* Reaching this point means no contact rewrite, so reset the flag */
    acc->contact_rewritten = PJ_FALSE;

    /* Check if we need to auto retry registration. Basically, registration
     * failure codes triggering auto-retry are those of temporal failures
     * considered to be recoverable in relatively short term.
     */
    if (acc->cfg.reg_retry_interval && 
	(param->code == PJSIP_SC_REQUEST_TIMEOUT ||
	 param->code == PJSIP_SC_INTERNAL_SERVER_ERROR ||
	 param->code == PJSIP_SC_BAD_GATEWAY ||
	 param->code == PJSIP_SC_SERVICE_UNAVAILABLE ||
	 param->code == PJSIP_SC_SERVER_TIMEOUT ||
	 param->code == PJSIP_SC_TEMPORARILY_UNAVAILABLE ||
	 PJSIP_IS_STATUS_IN_CLASS(param->code, 600))) /* Global failure */
    {
	schedule_reregistration(acc);
    }

    /* Call the registration status callback */

    if (pjsua_var.ua_cfg.cb.on_reg_state) {
	(*pjsua_var.ua_cfg.cb.on_reg_state)(acc->index);
    }

    if (pjsua_var.ua_cfg.cb.on_reg_state2) {
	pjsua_reg_info reg_info;
	pjsip_regc_info rinfo;

	pjsip_regc_get_info(param->regc, &rinfo);
	reg_info.cbparam = param;
	reg_info.regc = param->regc;
	reg_info.renew = !param->is_unreg;
	(*pjsua_var.ua_cfg.cb.on_reg_state2)(acc->index, &reg_info);
    }

    if (acc->ip_change_op == PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT) {
	if (pjsua_var.ua_cfg.cb.on_ip_change_progress) {
	    pjsua_ip_change_op_info ip_chg_info;
	    pjsip_regc_info rinfo;

	    pj_bzero(&ip_chg_info, sizeof(ip_chg_info));
	    pjsip_regc_get_info(param->regc, &rinfo);
	    ip_chg_info.acc_update_contact.acc_id = acc->index;
	    ip_chg_info.acc_update_contact.code = param->code;
	    ip_chg_info.acc_update_contact.is_register = !param->is_unreg;
	    (*pjsua_var.ua_cfg.cb.on_ip_change_progress)(acc->ip_change_op,
							 param->status,
							 &ip_chg_info);
	}

	if (PJSIP_IS_STATUS_IN_CLASS(param->code, 200)) {
	    if (param->expiration < 1) {
		pj_status_t status;
		/* Send re-register. */
		PJ_LOG(3, (THIS_FILE, "%.*s: send registration triggered by IP"
			   " change", pjsua_var.acc[acc->index].cfg.id.slen,
			   pjsua_var.acc[acc->index].cfg.id.ptr));

		status = pjsua_acc_set_registration(acc->index, PJ_TRUE);
		if ((status != PJ_SUCCESS) &&
		    pjsua_var.ua_cfg.cb.on_ip_change_progress)
		{
		    pjsua_ip_change_op_info ip_chg_info;

		    pj_bzero(&ip_chg_info, sizeof(ip_chg_info));
		    ip_chg_info.acc_update_contact.acc_id = acc->index;
		    ip_chg_info.acc_update_contact.is_register = PJ_TRUE;
		    (*pjsua_var.ua_cfg.cb.on_ip_change_progress)(
							    acc->ip_change_op,
							    status,
							    &ip_chg_info);

		    pjsua_acc_end_ip_change(acc);
		}
	    } else {
                /* Avoid deadlock issue when sending BYE or Re-INVITE.  */
		pjsua_schedule_timer2(&handle_call_on_ip_change_cb,
				      (void*)acc, 0);
	    }
	} else {
	    pjsua_acc_end_ip_change(acc);
	}
    }

    PJSUA_UNLOCK();
    pj_log_pop_indent();
}


/*
 * Initialize client registration.
 */
static pj_status_t pjsua_regc_init(int acc_id)
{
    pjsua_acc *acc;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_acc_is_valid(acc_id), PJ_EINVAL);
    acc = &pjsua_var.acc[acc_id];

    if (acc->cfg.reg_uri.slen == 0) {
	PJ_LOG(3,(THIS_FILE, "Registrar URI is not specified"));
	return PJ_SUCCESS;
    }

    /* Destroy existing session, if any */
    if (acc->regc) {
	pjsip_regc_destroy(acc->regc);
	acc->regc = NULL;
	acc->contact.slen = 0;
	acc->reg_mapped_addr.slen = 0;
	acc->rfc5626_status = OUTBOUND_UNKNOWN;
    }

    /* initialize SIP registration if registrar is configured */

    status = pjsip_regc_create( pjsua_var.endpt, 
				acc, &regc_cb, &acc->regc);

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create client registration", 
		     status);
	return status;
    }

    pool = pjsua_pool_create("tmpregc", 512, 512);

    if (acc->contact.slen == 0) {
	pj_str_t tmp_contact;

	status = pjsua_acc_create_uac_contact( pool, &tmp_contact,
					       acc_id, &acc->cfg.reg_uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate suitable Contact header"
				    " for registration", 
			 status);
	    pjsip_regc_destroy(acc->regc);
	    pj_pool_release(pool);
	    acc->regc = NULL;
	    return status;
	}

	pj_strdup_with_null(acc->pool, &acc->contact, &tmp_contact);
	update_regc_contact(acc);
    }

    status = pjsip_regc_init( acc->regc,
			      &acc->cfg.reg_uri, 
			      &acc->cfg.id, 
			      &acc->cfg.id,
			      1, &acc->reg_contact,
			      acc->cfg.reg_timeout);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Client registration initialization error", 
		     status);
	pjsip_regc_destroy(acc->regc);
	pj_pool_release(pool);
	acc->regc = NULL;
	acc->contact.slen = 0;
	acc->reg_mapped_addr.slen = 0;
	acc->rfc5626_status = OUTBOUND_UNKNOWN;

	return status;
    }

    pjsip_regc_set_reg_tsx_cb(acc->regc, regc_tsx_cb);

    /* If account is locked to specific transport, then set transport to
     * the client registration.
     */
    if (pjsua_var.acc[acc_id].cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(pjsua_var.acc[acc_id].cfg.transport_id, &tp_sel);
	pjsip_regc_set_transport(acc->regc, &tp_sel);
    }


    /* Set credentials
     */
    if (acc->cred_cnt) {
	pjsip_regc_set_credentials( acc->regc, acc->cred_cnt, acc->cred);
    }

    /* Set delay before registration refresh */
    pjsip_regc_set_delay_before_refresh(acc->regc,
                                        acc->cfg.reg_delay_before_refresh);

    /* Set authentication preference */
    pjsip_regc_set_prefs(acc->regc, &acc->cfg.auth_pref);

    /* Set route-set
     */
    if (acc->cfg.reg_use_proxy) {
	pjsip_route_hdr route_set;
	const pjsip_route_hdr *r;

	pj_list_init(&route_set);

	if (acc->cfg.reg_use_proxy & PJSUA_REG_USE_OUTBOUND_PROXY) {
	    r = pjsua_var.outbound_proxy.next;
	    while (r != &pjsua_var.outbound_proxy) {
		pj_list_push_back(&route_set, pjsip_hdr_shallow_clone(pool, r));
		r = r->next;
	    }
	}

	if (acc->cfg.reg_use_proxy & PJSUA_REG_USE_ACC_PROXY &&
	    acc->cfg.proxy_cnt)
	{
	    int cnt = acc->cfg.proxy_cnt;
	    pjsip_route_hdr *pos = route_set.prev;
	    int i;

	    r = acc->route_set.prev;
	    for (i=0; i<cnt; ++i) {
		pj_list_push_front(pos, pjsip_hdr_shallow_clone(pool, r));
		r = r->prev;
	    }
	}

	if (!pj_list_empty(&route_set))
	    pjsip_regc_set_route_set( acc->regc, &route_set );
    }

    /* Add custom request headers specified in the account config */
    pjsip_regc_add_headers(acc->regc, &acc->cfg.reg_hdr_list);

    /* Add other request headers. */
    if (pjsua_var.ua_cfg.user_agent.slen) {
	pjsip_hdr hdr_list;
	const pj_str_t STR_USER_AGENT = { "User-Agent", 10 };
	pjsip_generic_string_hdr *h;

	pj_list_init(&hdr_list);

	h = pjsip_generic_string_hdr_create(pool, &STR_USER_AGENT, 
					    &pjsua_var.ua_cfg.user_agent);
	pj_list_push_back(&hdr_list, (pjsip_hdr*)h);

	pjsip_regc_add_headers(acc->regc, &hdr_list);
    }

    /* If SIP outbound is used, add "Supported: outbound, path header" */
    if (acc->rfc5626_status == OUTBOUND_WANTED ||
	acc->rfc5626_status == OUTBOUND_ACTIVE)
    {
	pjsip_hdr hdr_list;
	pjsip_supported_hdr *hsup;

	pj_list_init(&hdr_list);
	hsup = pjsip_supported_hdr_create(pool);
	pj_list_push_back(&hdr_list, hsup);

	hsup->count = 2;
	hsup->values[0] = pj_str("outbound");
	hsup->values[1] = pj_str("path");

	pjsip_regc_add_headers(acc->regc, &hdr_list);
    }

    pj_pool_release(pool);

    return PJ_SUCCESS;
}

pj_bool_t pjsua_sip_acc_is_using_ipv6(pjsua_acc_id acc_id)
{
    pjsua_acc *acc = &pjsua_var.acc[acc_id];

    return (acc->tp_type & PJSIP_TRANSPORT_IPV6) == PJSIP_TRANSPORT_IPV6;
}

pj_bool_t pjsua_sip_acc_is_using_stun(pjsua_acc_id acc_id)
{
    pjsua_acc *acc = &pjsua_var.acc[acc_id];

    return acc->cfg.sip_stun_use != PJSUA_STUN_USE_DISABLED &&
	   pjsua_var.ua_cfg.stun_srv_cnt != 0;
}

pj_bool_t pjsua_media_acc_is_using_stun(pjsua_acc_id acc_id)
{
    pjsua_acc *acc = &pjsua_var.acc[acc_id];

    return acc->cfg.media_stun_use != PJSUA_STUN_USE_DISABLED &&
	   pjsua_var.ua_cfg.stun_srv_cnt != 0;
}

/*
 * Update registration or perform unregistration. 
 */
PJ_DEF(pj_status_t) pjsua_acc_set_registration( pjsua_acc_id acc_id, 
						pj_bool_t renew)
{
    pjsua_acc *acc;
    pj_status_t status = 0;
    pjsip_tx_data *tdata = 0;

    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.acc[acc_id].valid, PJ_EINVALIDOP);

    PJ_LOG(4,(THIS_FILE, "Acc %d: setting %sregistration..",
	      acc_id, (renew? "" : "un")));
    pj_log_push_indent();

    PJSUA_LOCK();

    acc = &pjsua_var.acc[acc_id];

    /* Cancel any re-registration timer */
    if (pjsua_var.acc[acc_id].auto_rereg.timer.id) {
	pjsua_var.acc[acc_id].auto_rereg.timer.id = PJ_FALSE;
	pjsua_cancel_timer(&pjsua_var.acc[acc_id].auto_rereg.timer);
    }

    /* Reset pointer to registration transport */
    // Do not reset this here, as if currently there is another registration
    // on progress, this registration will fail but transport pointer will
    // become NULL which will prevent transport to be destroyed immediately
    // after disconnected (which may cause iOS app getting killed (see #1482).
    //pjsua_var.acc[acc_id].auto_rereg.reg_tp = NULL;

    if (renew) {
	if (pjsua_var.acc[acc_id].regc == NULL) {
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

	status = pjsip_regc_register(pjsua_var.acc[acc_id].regc,
                                     PJSUA_REG_AUTO_REG_REFRESH,
				     &tdata);

	if (0 && status == PJ_SUCCESS && pjsua_var.acc[acc_id].cred_cnt) {
	    pjsip_authorization_hdr *h;
	    char *uri;
	    int d;

	    uri = (char*) pj_pool_alloc(tdata->pool, acc->cfg.reg_uri.slen+10);
	    d = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, tdata->msg->line.req.uri,
				uri, acc->cfg.reg_uri.slen+10);
	    pj_assert(d > 0);
	    PJ_UNUSED_ARG(d);

	    h = pjsip_authorization_hdr_create(tdata->pool);
	    h->scheme = pjsip_DIGEST_STR;
	    h->credential.digest.username = acc->cred[0].username;
	    h->credential.digest.realm = acc->srv_domain;
	    h->credential.digest.uri = pj_str(uri);
	    h->credential.digest.algorithm = pjsip_MD5_STR;

	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)h);
	}

    } else {
	if (pjsua_var.acc[acc_id].regc == NULL) {
	    PJ_LOG(3,(THIS_FILE, "Currently not registered"));
	    status = PJ_EINVALIDOP;
	    goto on_return;
	}

	pjsua_pres_unpublish(&pjsua_var.acc[acc_id], 0);

	status = pjsip_regc_unregister(pjsua_var.acc[acc_id].regc, &tdata);
    }

    if (status == PJ_SUCCESS) {
        pjsip_regc *regc = pjsua_var.acc[acc_id].regc;

        if (pjsua_var.acc[acc_id].cfg.allow_via_rewrite &&
            pjsua_var.acc[acc_id].via_addr.host.slen > 0)
        {
            pjsip_regc_set_via_sent_by(pjsua_var.acc[acc_id].regc,
                                       &pjsua_var.acc[acc_id].via_addr,
                                       pjsua_var.acc[acc_id].via_tp);
        } else if (!pjsua_sip_acc_is_using_stun(acc_id)) {
            /* Choose local interface to use in Via if acc is not using
             * STUN
             */
            pjsua_acc_get_uac_addr(acc_id, tdata->pool,
	                           &acc->cfg.reg_uri,
	                           &tdata->via_addr,
	                           NULL, NULL,
	                           &tdata->via_tp);
        }

	/* Increment ref counter and release PJSUA lock here, to avoid
	 * deadlock while making sure that regc won't be destroyed.
	 */
	pjsip_regc_add_ref(regc);
	PJSUA_UNLOCK();
	
	//pjsua_process_msg_data(tdata, NULL);
	status = pjsip_regc_send( regc, tdata );
	
	PJSUA_LOCK();
	if (pjsip_regc_dec_ref(regc) == PJ_EGONE) {
	    /* regc has been deleted. */
	    goto on_return;
	}
    }

    /* Update pointer to registration transport */
    if (status == PJ_SUCCESS) {
        /* Variable auto_rereg.reg_tp is currently unused since it may differ
         * with the transport used by regc (for example, when a resolver is
         * employed). A more reliable way is to query the regc directly
         * when needed.
         */
	//pjsip_regc_info reg_info;

	//pjsip_regc_get_info(pjsua_var.acc[acc_id].regc, &reg_info);
	//pjsua_var.acc[acc_id].auto_rereg.reg_tp = reg_info.transport;

        if (pjsua_var.ua_cfg.cb.on_reg_started) {
            (*pjsua_var.ua_cfg.cb.on_reg_started)(acc_id, renew);
        }
	if (pjsua_var.ua_cfg.cb.on_reg_started2) {
	    pjsua_reg_info rinfo;

	    rinfo.cbparam = NULL;
	    rinfo.regc = pjsua_var.acc[acc_id].regc;
	    rinfo.renew = renew;
            (*pjsua_var.ua_cfg.cb.on_reg_started2)(acc_id, &rinfo);
        }
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create/send REGISTER", 
		     status);
    } else {
	PJ_LOG(4,(THIS_FILE, "Acc %d: %s sent", acc_id,
	         (renew? "Registration" : "Unregistration")));
    }

on_return:
    PJSUA_UNLOCK();
    pj_log_pop_indent();
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
    PJ_ASSERT_RETURN(pjsua_acc_is_valid(acc_id), PJ_EINVAL);
    
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
    pj_memcpy(&info->rpid, &acc->rpid, sizeof(pjrpid_element));
    if (info->rpid.note.slen)
	info->online_status_text = info->rpid.note;
    else if (info->online_status)
	info->online_status_text = pj_str("Online");
    else
	info->online_status_text = pj_str("Offline");

    if (acc->reg_last_code) {
	if (info->has_registration) {
	    info->status = (pjsip_status_code) acc->reg_last_code;
	    info->status_text = *pjsip_get_status_text(acc->reg_last_code);
            if (acc->reg_last_err)
	        info->reg_last_err = acc->reg_last_err;
	} else {
	    info->status = (pjsip_status_code) 0;
	    info->status_text = pj_str("not registered");
	}
    } else if (acc->cfg.reg_uri.slen) {
	info->status = PJSIP_SC_TRYING;
	info->status_text = pj_str("In Progress");
    } else {
	info->status = (pjsip_status_code) 0;
	info->status_text = pj_str("does not register");
    }
    
    if (acc->regc) {
	pjsip_regc_info regc_info;
	pjsip_regc_get_info(acc->regc, &regc_info);
	info->expires = regc_info.next_reg;
    } else {
	info->expires = PJSIP_EXPIRES_NOT_SPECIFIED;
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
    pj_pool_t *tmp_pool;
    unsigned i;

    PJSUA_LOCK();

    tmp_pool = pjsua_pool_create("tmpacc10", 256, 256);

    pj_strdup_with_null(tmp_pool, &tmp, url);

    uri = pjsip_parse_uri(tmp_pool, tmp.ptr, tmp.slen, 0);
    if (!uri) {
	pj_pool_release(tmp_pool);
	PJSUA_UNLOCK();
	return pjsua_var.default_acc;
    }

    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri)) 
    {
	/* Return the first account with proxy */
	for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	    if (!pjsua_var.acc[i].valid)
		continue;
	    if (!pj_list_empty(&pjsua_var.acc[i].route_set))
		break;
	}

	if (i != PJ_ARRAY_SIZE(pjsua_var.acc)) {
	    /* Found rather matching account */
	    pj_pool_release(tmp_pool);
	    PJSUA_UNLOCK();
	    return i;
	}

	/* Not found, use default account */
	pj_pool_release(tmp_pool);
	PJSUA_UNLOCK();
	return pjsua_var.default_acc;
    }

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri);

    /* Find matching domain AND port */
    for (i=0; i<pjsua_var.acc_cnt; ++i) {
	unsigned acc_id = pjsua_var.acc_ids[i];
	if (pj_stricmp(&pjsua_var.acc[acc_id].srv_domain, &sip_uri->host)==0 &&
	    pjsua_var.acc[acc_id].srv_port == sip_uri->port)
	{
	    pj_pool_release(tmp_pool);
	    PJSUA_UNLOCK();
	    return acc_id;
	}
    }

    /* If no match, try to match the domain part only */
    for (i=0; i<pjsua_var.acc_cnt; ++i) {
	unsigned acc_id = pjsua_var.acc_ids[i];
	if (pj_stricmp(&pjsua_var.acc[acc_id].srv_domain, &sip_uri->host)==0)
	{
	    pj_pool_release(tmp_pool);
	    PJSUA_UNLOCK();
	    return acc_id;
	}
    }


    /* Still no match, just use default account */
    pj_pool_release(tmp_pool);
    PJSUA_UNLOCK();
    return pjsua_var.default_acc;
}


/*
 * This is an internal function to find the most appropriate account to be
 * used to handle incoming calls.
 */
PJ_DEF(pjsua_acc_id) pjsua_acc_find_for_incoming(pjsip_rx_data *rdata)
{
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;
    pjsua_acc_id id = PJSUA_INVALID_ID;
    int max_score;
    unsigned i;

    if (pjsua_var.acc_cnt == 0) {
	PJ_LOG(2, (THIS_FILE, "No available account to handle %s",
		  pjsip_rx_data_get_info(rdata)));

	return PJSUA_INVALID_ID;
    }

    uri = rdata->msg_info.to->uri;

    PJSUA_LOCK();

    /* Use Req URI if To URI is not SIP */
    if (!PJSIP_URI_SCHEME_IS_SIP(uri) &&
	!PJSIP_URI_SCHEME_IS_SIPS(uri))
    {
	if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG)
	    uri = rdata->msg_info.msg->line.req.uri;
	else
	    goto on_return;
    }

    /* Just return default account if both To and Req URI are not SIP: */
    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri)) 
    {
	goto on_return;
    }

    sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);

    /* Select account by weighted score. Matching priority order is:
     * transport type (matched or not set), domain part, and user part.
     * Note that the transport type has higher priority as unmatched
     * transport type may cause failure in sending response.
     */
    max_score = 0;
    for (i=0; i < pjsua_var.acc_cnt; ++i) {
	unsigned acc_id = pjsua_var.acc_ids[i];
	pjsua_acc *acc = &pjsua_var.acc[acc_id];
	int score = 0;

	if (!acc->valid)
	    continue;

	/* Match transport type */
	if (acc->tp_type == rdata->tp_info.transport->key.type ||
	    acc->tp_type == PJSIP_TRANSPORT_UNSPECIFIED)
	{
	    score |= 4;
	}

	/* Match domain */
	if (pj_stricmp(&acc->srv_domain, &sip_uri->host)==0) {
	    score |= 2;
	}

	/* Match username */
	if (pj_stricmp(&acc->user_part, &sip_uri->user)==0) {
	    score |= 1;
	}

	if (score > max_score) {
	    id = acc_id;
	    max_score = score;
	}
    }

on_return:
    PJSUA_UNLOCK();

    /* Still no match, use default account */
    if (id == PJSUA_INVALID_ID)
	id = pjsua_var.default_acc;

    /* Invoke account find callback */
    if (pjsua_var.ua_cfg.cb.on_acc_find_for_incoming)
	(*pjsua_var.ua_cfg.cb.on_acc_find_for_incoming)(rdata, &id);

    /* Verify if the specified account id is valid */
    if (!pjsua_acc_is_valid(id))
	id = pjsua_var.default_acc;

    return id;
}


/*
 * Create arbitrary requests for this account. 
 */
PJ_DEF(pj_status_t) pjsua_acc_create_request(pjsua_acc_id acc_id,
					     const pjsip_method *method,
					     const pj_str_t *target,
					     pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsua_acc *acc;
    pjsip_route_hdr *r;
    pj_status_t status;

    PJ_ASSERT_RETURN(method && target && p_tdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_acc_is_valid(acc_id), PJ_EINVAL);

    acc = &pjsua_var.acc[acc_id];

    status = pjsip_endpt_create_request(pjsua_var.endpt, method, target, 
					&acc->cfg.id, target,
					NULL, NULL, -1, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create request", status);
	return status;
    }

    /* Copy routeset */
    r = acc->route_set.next;
    while (r != &acc->route_set) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*)pjsip_hdr_clone(tdata->pool, r));
	r = r->next;
    }

    /* If account is locked to specific transport, then set that transport to
     * the transmit data.
     */
    if (pjsua_var.acc[acc_id].cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);
	pjsip_tx_data_set_transport(tdata, &tp_sel);
    }

    /* If via_addr is set, use this address for the Via header. */
    if (pjsua_var.acc[acc_id].cfg.allow_via_rewrite &&
        pjsua_var.acc[acc_id].via_addr.host.slen > 0)
    {
        tdata->via_addr = pjsua_var.acc[acc_id].via_addr;
        tdata->via_tp = pjsua_var.acc[acc_id].via_tp;
    } else if (!pjsua_sip_acc_is_using_stun(acc_id)) {
        /* Choose local interface to use in Via if acc is not using
         * STUN
         */
        pjsua_acc_get_uac_addr(acc_id, tdata->pool,
	                       target,
	                       &tdata->via_addr,
	                       NULL, NULL,
	                       &tdata->via_tp);
    }

    /* Done */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}

/*
 * Internal:
 *  determine if an address is a valid IP address, and if it is,
 *  return the IP version (4 or 6).
 */
static int get_ip_addr_ver(const pj_str_t *host)
{
    pj_in_addr dummy;
    pj_in6_addr dummy6;

    /* First check if this is an IPv4 address */
    if (pj_inet_pton(pj_AF_INET(), host, &dummy) == PJ_SUCCESS)
	return 4;

    /* Then check if this is an IPv6 address */
    if (pj_inet_pton(pj_AF_INET6(), host, &dummy6) == PJ_SUCCESS)
	return 6;

    /* Not an IP address */
    return 0;
}

/* Get local transport address suitable to be used for Via or Contact address
 * to send request to the specified destination URI.
 */
pj_status_t pjsua_acc_get_uac_addr(pjsua_acc_id acc_id,
				   pj_pool_t *pool,
				   const pj_str_t *dst_uri,
				   pjsip_host_port *addr,
				   pjsip_transport_type_e *p_tp_type,
				   int *secure,
				   const void **p_tp)
{
    pjsua_acc *acc;
    pjsip_sip_uri *sip_uri;
    pj_status_t status;
    pjsip_transport_type_e tp_type = PJSIP_TRANSPORT_UNSPECIFIED;
    unsigned flag;
    pjsip_tpselector tp_sel;
    pjsip_tpmgr *tpmgr;
    pjsip_tpmgr_fla2_param tfla2_prm;
    pj_bool_t update_addr = PJ_TRUE;

    PJ_ASSERT_RETURN(pjsua_acc_is_valid(acc_id), PJ_EINVAL);
    acc = &pjsua_var.acc[acc_id];

    /* If route-set is configured for the account, then URI is the
     * first entry of the route-set.
     */
    if (!pj_list_empty(&acc->route_set)) {
	sip_uri = (pjsip_sip_uri*)
		  pjsip_uri_get_uri(acc->route_set.next->name_addr.uri);
    } else {
	pj_str_t tmp;
	pjsip_uri *uri;

	pj_strdup_with_null(pool, &tmp, dst_uri);

	uri = pjsip_parse_uri(pool, tmp.ptr, tmp.slen, 0);
	if (uri == NULL)
	    return PJSIP_EINVALIDURI;

	/* For non-SIP scheme, route set should be configured */
	if (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri))
	    return PJSIP_ENOROUTESET;

	sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);
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

    /* If destination URI specifies IPv6 or account is configured to use IPv6,
     * then set transport type to use IPv6 as well.
     */
    if (pj_strchr(&sip_uri->host, ':') || pjsua_sip_acc_is_using_ipv6(acc_id))
	tp_type = (pjsip_transport_type_e)(((int)tp_type) |
	 	  PJSIP_TRANSPORT_IPV6);

    flag = pjsip_transport_get_flag_from_type(tp_type);

    /* Init transport selector. */
    pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);

    /* Get local address suitable to send request from */
    pjsip_tpmgr_fla2_param_default(&tfla2_prm);
    tfla2_prm.tp_type = tp_type;
    tfla2_prm.tp_sel = &tp_sel;
    tfla2_prm.dst_host = sip_uri->host;
    tfla2_prm.local_if = (!pjsua_sip_acc_is_using_stun(acc_id) ||
	                  (flag & PJSIP_TRANSPORT_RELIABLE));

    tpmgr = pjsip_endpt_get_tpmgr(pjsua_var.endpt);
    status = pjsip_tpmgr_find_local_addr2(tpmgr, pool, &tfla2_prm);
    if (status != PJ_SUCCESS)
	return status;

    /* Set this as default return value. This may be changed below. */
    addr->host = tfla2_prm.ret_addr;
    addr->port = tfla2_prm.ret_port;

    /* If we are behind NAT64, use the Contact and Via address from
     * the UDP6 transport, which should be obtained from STUN.
     */
    if (acc->cfg.nat64_opt != PJSUA_NAT64_DISABLED) {
        pjsip_tpmgr_fla2_param tfla2_prm2 = tfla2_prm;
        
        tfla2_prm2.tp_type = PJSIP_TRANSPORT_UDP6;
        tfla2_prm2.tp_sel = NULL;
        tfla2_prm2.local_if = (!pjsua_sip_acc_is_using_stun(acc_id));
        status = pjsip_tpmgr_find_local_addr2(tpmgr, pool, &tfla2_prm2);
    	if (status == PJ_SUCCESS) {
    	    update_addr = PJ_FALSE;
	    addr->host = tfla2_prm2.ret_addr;
	    pj_strdup(acc->pool, &acc->via_addr.host, &addr->host);
	    acc->via_addr.port = addr->port;
	    acc->via_tp = (pjsip_transport *)tfla2_prm.ret_tp;
	}
    } else
    /* For UDP transport, check if we need to overwrite the address
     * with its bound address.
     */
    if ((flag & PJSIP_TRANSPORT_DATAGRAM) && tfla2_prm.local_if &&
    	tfla2_prm.ret_tp)
    {
    	int i;

    	for (i = 0; i < sizeof(pjsua_var.tpdata); i++) {
    	    if (tfla2_prm.ret_tp==(const void *)pjsua_var.tpdata[i].data.tp) {
    	    	if (pjsua_var.tpdata[i].has_bound_addr) {
		    pj_strdup(pool, &addr->host,
		    	      &pjsua_var.tpdata[i].data.tp->local_name.host);
	    	    addr->port = (pj_uint16_t)
	    	    		 pjsua_var.tpdata[i].data.tp->local_name.port;
    	    	}
    	    	break;
    	    }
    	}
    }

    /* For TCP/TLS, acc may request to specify source port */
    if (acc->cfg.contact_use_src_port) {
	pjsip_host_info dinfo;
	pjsip_transport *tp = NULL;
	pj_addrinfo ai;
	pj_bool_t log_written = PJ_FALSE;

	status = pjsip_get_dest_info((pjsip_uri*)sip_uri, NULL,
				     pool, &dinfo);

	if (status==PJ_SUCCESS && (dinfo.flag & PJSIP_TRANSPORT_RELIABLE)==0) {
	    /* Not TCP or TLS. No need to do this */
	    status = PJ_EINVALIDOP;
	    log_written = PJ_TRUE;
	}

	if (status==PJ_SUCCESS &&
	    get_ip_addr_ver(&dinfo.addr.host)==0 &&
	    pjsua_var.ua_cfg.nameserver_count)
	{
	    /* If nameserver is configured, PJSIP will resolve destinations
	     * by their DNS SRV record first. On the other hand, we will
	     * resolve destination with DNS A record via pj_getaddrinfo().
	     * They may yield different IP addresses, hence causing different
	     * TCP/TLS connection to be created and hence different source
	     * address.
	     */
	    PJ_LOG(4,(THIS_FILE, "Warning: cannot use source TCP/TLS socket"
		      " address for Contact when nameserver is configured."));
	    status = PJ_ENOTSUP;
	    log_written = PJ_TRUE;
	}

	if (status == PJ_SUCCESS) {
	    unsigned cnt=1;
	    int af = pj_AF_UNSPEC();

	    if (pjsua_sip_acc_is_using_ipv6(acc_id) ||
		(dinfo.type & PJSIP_TRANSPORT_IPV6))
	    {
		af = pj_AF_INET6();
	    }
	    status = pj_getaddrinfo(af, &dinfo.addr.host, &cnt, &ai);
	    if (cnt == 0) {
		status = PJ_ENOTSUP;
	    } else if ((dinfo.type & PJSIP_TRANSPORT_IPV6)==0 &&
			ai.ai_addr.addr.sa_family == pj_AF_INET6())
	    {
		/* Destination is a hostname and account is not bound to IPv6,
		 * but hostname resolution reveals that it has IPv6 address,
		 * so let's use IPv6 transport type.
		 */
		dinfo.type |= PJSIP_TRANSPORT_IPV6;
		tp_type |= PJSIP_TRANSPORT_IPV6;
	    }
	}

	if (status == PJ_SUCCESS) {
	    pjsip_tx_data tdata;
	    int addr_len = pj_sockaddr_get_len(&ai.ai_addr);
	    pj_uint16_t port = (pj_uint16_t)dinfo.addr.port;

	    /* Create a dummy tdata to inform remote host name to transport */
	    pj_bzero(&tdata, sizeof(tdata));
	    pj_strdup(pool, &tdata.dest_info.name, &dinfo.addr.host);

	    if (port==0) {
		port = (dinfo.flag & PJSIP_TRANSPORT_SECURE) ? 5061 : 5060;
	    }
	    pj_sockaddr_set_port(&ai.ai_addr, port);
	    status = pjsip_endpt_acquire_transport2(pjsua_var.endpt,
						    dinfo.type,
						    &ai.ai_addr,
						    addr_len,
						    &tp_sel,
						    &tdata, &tp);
	}

	if (status == PJ_SUCCESS && (tp->local_name.port == 0 ||
				     tp->local_name.host.slen==0 ||
				     *tp->local_name.host.ptr=='0'))
	{
	    /* Trap zero port or "0.0.0.0" address. */
	    /* The TCP/TLS transport is still connecting and unfortunately
	     * this OS doesn't report the bound local address in this state.
	     */
	    PJ_LOG(4,(THIS_FILE, "Unable to get transport local port "
		      "for Contact address (OS doesn't support)"));
	    status = PJ_ENOTSUP;
	    log_written = PJ_TRUE;
	}

	if (status == PJ_SUCCESS) {
	    /* Got the local transport address, don't update if
	     * we are on NAT64 and already obtained the address
	     * from STUN above.
	     */
	    if (update_addr)
	        pj_strdup(pool, &addr->host, &tp->local_name.host);
	    addr->port = tp->local_name.port;
	}

	if (tp) {
	    /* Here the transport's ref counter WILL reach zero. But the
	     * transport will NOT get destroyed because it should have an
	     * idle timer.
	     */
	    pjsip_transport_dec_ref(tp);
	    tp = NULL;
	}

	if (status != PJ_SUCCESS && !log_written) {
	    PJ_PERROR(4,(THIS_FILE, status, "Unable to use source local "
		         "TCP socket address for Contact"));
	}
	status = PJ_SUCCESS;
    }

    if (p_tp_type)
	*p_tp_type = tp_type;

    if (secure) {
	*secure = (flag & PJSIP_TRANSPORT_SECURE) != 0;
    }

    if (p_tp)
	*p_tp = tfla2_prm.ret_tp;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_acc_create_uac_contact( pj_pool_t *pool,
						  pj_str_t *contact,
						  pjsua_acc_id acc_id,
						  const pj_str_t *suri)
{
    pjsua_acc *acc;
    pj_status_t status;
    pjsip_transport_type_e tp_type;
    pjsip_host_port addr;
    int secure;
    const char *beginquote, *endquote;
    char transport_param[32];
    const char *ob = ";ob";

    
    PJ_ASSERT_RETURN(pjsua_acc_is_valid(acc_id), PJ_EINVAL);
    acc = &pjsua_var.acc[acc_id];

    /* If force_contact is configured, then use use it */
    if (acc->cfg.force_contact.slen) {
	*contact = acc->cfg.force_contact;
	return PJ_SUCCESS;
    }

    status = pjsua_acc_get_uac_addr(acc_id, pool, suri, &addr,
                                    &tp_type, &secure, NULL);
    if (status != PJ_SUCCESS)
	return status;

    /* Enclose IPv6 address in square brackets */
    if (tp_type & PJSIP_TRANSPORT_IPV6) {
	beginquote = "[";
	endquote = "]";
    } else {
	beginquote = endquote = "";
    }

    /* Don't add transport parameter if it's UDP */
    if (tp_type!=PJSIP_TRANSPORT_UDP && tp_type!=PJSIP_TRANSPORT_UDP6) {
	pj_ansi_snprintf(transport_param, sizeof(transport_param),
		         ";transport=%s",
			 pjsip_transport_get_type_name(tp_type));
    } else {
	transport_param[0] = '\0';
    }


    /* Create the contact header */
    contact->ptr = (char*)pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
    contact->slen = pj_ansi_snprintf(contact->ptr, PJSIP_MAX_URL_SIZE,
				     "%s%.*s%s<%s:%.*s%s%s%.*s%s:%d%s%.*s%s>%.*s",
				     (acc->display.slen?"\"" : ""),
				     (int)acc->display.slen,
				     acc->display.ptr,
				     (acc->display.slen?"\" " : ""),
				     ((secure && acc->is_sips)? "sips" : "sip"),
				     (int)acc->user_part.slen,
				     acc->user_part.ptr,
				     (acc->user_part.slen?"@":""),
				     beginquote,
				     (int)addr.host.slen,
				     addr.host.ptr,
				     endquote,
				     addr.port,
				     transport_param,
				     (int)acc->cfg.contact_uri_params.slen,
				     acc->cfg.contact_uri_params.ptr,
				     (acc->cfg.use_rfc5626? ob: ""),
				     (int)acc->cfg.contact_params.slen,
				     acc->cfg.contact_params.ptr);
    if (contact->slen < 1 || contact->slen >= (int)PJSIP_MAX_URL_SIZE)
	return PJ_ETOOSMALL;
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
    pjsip_tpselector tp_sel;
    pjsip_tpmgr *tpmgr;
    pjsip_tpmgr_fla2_param tfla2_prm;
    unsigned flag;
    int secure;
    int local_port;
    const char *beginquote, *endquote;
    char transport_param[32];
    
    PJ_ASSERT_RETURN(pjsua_acc_is_valid(acc_id), PJ_EINVAL);
    acc = &pjsua_var.acc[acc_id];

    /* If force_contact is configured, then use use it */
    if (acc->cfg.force_contact.slen) {
	*contact = acc->cfg.force_contact;
	return PJ_SUCCESS;
    }

    /* If Record-Route is present, then URI is the top Record-Route. */
    if (rdata->msg_info.record_route) {
	sip_uri = (pjsip_sip_uri*) 
		pjsip_uri_get_uri(rdata->msg_info.record_route->name_addr.uri);
    } else {
	pjsip_hdr *pos = NULL;
	pjsip_contact_hdr *h_contact;
	pjsip_uri *uri = NULL;

	/* Otherwise URI is Contact URI.
	 * Iterate the Contact URI until we find sip: or sips: scheme.
	 */
	do {
	    h_contact = (pjsip_contact_hdr*)
			pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT,
					   pos);
	    if (h_contact) {
		if (h_contact->uri)
		    uri = (pjsip_uri*) pjsip_uri_get_uri(h_contact->uri);
		else
		    uri = NULL;
		if (!uri || (!PJSIP_URI_SCHEME_IS_SIP(uri) &&
		             !PJSIP_URI_SCHEME_IS_SIPS(uri)))
		{
		    pos = (pjsip_hdr*)h_contact->next;
		    if (pos == &rdata->msg_info.msg->hdr)
			h_contact = NULL;
		} else {
		    break;
		}
	    }
	} while (h_contact);
	

	/* Or if Contact URI is not present, take the remote URI from
	 * the From URI.
	 */
	if (uri == NULL)
	    uri = (pjsip_uri*) pjsip_uri_get_uri(rdata->msg_info.from->uri);


	/* Can only do sip/sips scheme at present. */
	if (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri))
	    return PJSIP_EINVALIDREQURI;

	sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);
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

    /* If destination URI specifies IPv6 or account is configured to use IPv6
     * or the transport being used to receive data is an IPv6 transport,
     * then set transport type to use IPv6 as well.
     */
    if (pj_strchr(&sip_uri->host, ':') ||
	pjsua_sip_acc_is_using_ipv6(acc_id) ||
	(rdata->tp_info.transport->key.type & PJSIP_TRANSPORT_IPV6))
    {
	tp_type = (pjsip_transport_type_e)
		  (((int)tp_type) | PJSIP_TRANSPORT_IPV6);
    }

    flag = pjsip_transport_get_flag_from_type(tp_type);
    secure = (flag & PJSIP_TRANSPORT_SECURE) != 0;

    /* Init transport selector. */
    pjsua_init_tpselector(pjsua_var.acc[acc_id].cfg.transport_id, &tp_sel);

    /* Get local address suitable to send request from */
    pjsip_tpmgr_fla2_param_default(&tfla2_prm);
    tfla2_prm.tp_type = tp_type;
    tfla2_prm.tp_sel = &tp_sel;
    tfla2_prm.dst_host = sip_uri->host;
    tfla2_prm.local_if = (!pjsua_sip_acc_is_using_stun(acc_id) ||
	                  (flag & PJSIP_TRANSPORT_RELIABLE));

    tpmgr = pjsip_endpt_get_tpmgr(pjsua_var.endpt);
    status = pjsip_tpmgr_find_local_addr2(tpmgr, pool, &tfla2_prm);
    if (status != PJ_SUCCESS)
	return status;

    local_addr = tfla2_prm.ret_addr;
    local_port = tfla2_prm.ret_port;


    /* Enclose IPv6 address in square brackets */
    if (tp_type & PJSIP_TRANSPORT_IPV6) {
	beginquote = "[";
	endquote = "]";
    } else {
	beginquote = endquote = "";
    }

    /* Don't add transport parameter if it's UDP */
    if (tp_type!=PJSIP_TRANSPORT_UDP && tp_type!=PJSIP_TRANSPORT_UDP6) {
	pj_ansi_snprintf(transport_param, sizeof(transport_param),
		         ";transport=%s",
			 pjsip_transport_get_type_name(tp_type));
    } else {
	transport_param[0] = '\0';
    }


    /* Create the contact header */
    contact->ptr = (char*) pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
    contact->slen = pj_ansi_snprintf(contact->ptr, PJSIP_MAX_URL_SIZE,
				     "%s%.*s%s<%s:%.*s%s%s%.*s%s:%d%s%.*s>%.*s",
				     (acc->display.slen?"\"" : ""),
				     (int)acc->display.slen,
				     acc->display.ptr,
				     (acc->display.slen?"\" " : ""),
				     ((secure && acc->is_sips)? "sips" : "sip"),
				     (int)acc->user_part.slen,
				     acc->user_part.ptr,
				     (acc->user_part.slen?"@":""),
				     beginquote,
				     (int)local_addr.slen,
				     local_addr.ptr,
				     endquote,
				     local_port,
				     transport_param,
				     (int)acc->cfg.contact_uri_params.slen,
				     acc->cfg.contact_uri_params.ptr,
				     (int)acc->cfg.contact_params.slen,
				     acc->cfg.contact_params.ptr);
    if (contact->slen < 1 || contact->slen >= (int)PJSIP_MAX_URL_SIZE)
	return PJ_ETOOSMALL;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_acc_set_transport( pjsua_acc_id acc_id,
					     pjsua_transport_id tp_id)
{
    pjsua_acc *acc;

    PJ_ASSERT_RETURN(pjsua_acc_is_valid(acc_id), PJ_EINVAL);
    acc = &pjsua_var.acc[acc_id];

    PJ_ASSERT_RETURN(tp_id >= 0 && tp_id < (int)PJ_ARRAY_SIZE(pjsua_var.tpdata),
		     PJ_EINVAL);
    
    acc->cfg.transport_id = tp_id;
    acc->tp_type = pjsua_var.tpdata[tp_id].type;

    return PJ_SUCCESS;
}


/* Auto re-registration timeout callback */
static void auto_rereg_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
    pjsua_acc *acc;
    pj_status_t status;

    PJ_UNUSED_ARG(th);
    acc = (pjsua_acc*) te->user_data;
    pj_assert(acc);

    PJSUA_LOCK();

    /* Check if the reregistration timer is still valid, e.g: while waiting
     * timeout timer application might have deleted the account or disabled
     * the auto-reregistration.
     */
    if (!acc->valid || !acc->auto_rereg.active || 
	acc->cfg.reg_retry_interval == 0)
    {
	goto on_return;
    }

    /* Start re-registration */
    acc->auto_rereg.attempt_cnt++;

    /* Generate new contact as the current contact may use a disconnected
     * transport. Only do this when outbound is not active and contact is not
     * rewritten (where the contact address may really be used by server to
     * contact the UA).
     */
    if (acc->rfc5626_status != OUTBOUND_ACTIVE && !acc->contact_rewritten) {
	pj_str_t tmp_contact;
	pj_pool_t *pool;

	pool = pjsua_pool_create("tmpregc", 512, 512);

	status = pjsua_acc_create_uac_contact(pool, &tmp_contact, acc->index,
					      &acc->cfg.reg_uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE,
			 "Unable to generate suitable Contact header"
			 " for re-registration", status);
	    pj_pool_release(pool);
	    schedule_reregistration(acc);
	    goto on_return;
	}

	if (pj_strcmp(&tmp_contact, &acc->contact)) {
	    if (acc->contact.slen < tmp_contact.slen) {
		pj_strdup_with_null(acc->pool, &acc->contact, &tmp_contact);
	    } else {
		pj_strcpy(&acc->contact, &tmp_contact);
	    }
	    update_regc_contact(acc);
	    if (acc->regc)
		pjsip_regc_update_contact(acc->regc, 1, &acc->reg_contact);
	}
	pj_pool_release(pool);
    }

    status = pjsua_acc_set_registration(acc->index, PJ_TRUE);
    if (status != PJ_SUCCESS)
	schedule_reregistration(acc);

on_return:
    PJSUA_UNLOCK();
}


/* Schedule reregistration for specified account. Note that the first 
 * re-registration after a registration failure will be done immediately.
 * Also note that this function should be called within PJSUA mutex.
 */
static void schedule_reregistration(pjsua_acc *acc)
{
    pj_time_val delay;

    pj_assert(acc);

    /* Validate the account and re-registration feature status */
    if (!acc->valid || acc->cfg.reg_retry_interval == 0) {
	return;
    }

    /* If configured, disconnect calls of this account after the first
     * reregistration attempt failed.
     */
    if (acc->cfg.drop_calls_on_reg_fail && acc->auto_rereg.attempt_cnt >= 1)
    {
	unsigned i, cnt;

	for (i = 0, cnt = 0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	    if (pjsua_var.calls[i].acc_id == acc->index) {
		pjsua_call_hangup(i, 0, NULL, NULL);
		++cnt;
	    }
	}

	if (cnt) {
	    PJ_LOG(3, (THIS_FILE, "Disconnecting %d call(s) of account #%d "
				  "after reregistration attempt failed",
				  cnt, acc->index));
	}
    }

    /* Cancel any re-registration timer */
    if (acc->auto_rereg.timer.id) {
	acc->auto_rereg.timer.id = PJ_FALSE;
	pjsua_cancel_timer(&acc->auto_rereg.timer);
    }

    /* Update re-registration flag */
    acc->auto_rereg.active = PJ_TRUE;

    /* Set up timer for reregistration */
    acc->auto_rereg.timer.cb = &auto_rereg_timer_cb;
    acc->auto_rereg.timer.user_data = acc;

    /* Reregistration attempt. The first attempt will be done immediately. */
    delay.sec = acc->auto_rereg.attempt_cnt? acc->cfg.reg_retry_interval :
					     acc->cfg.reg_first_retry_interval;
    delay.msec = 0;

    /* Randomize interval by +/- reg_retry_random_interval, if configured */
    if (acc->cfg.reg_retry_random_interval) {
	long rand_ms = acc->cfg.reg_retry_random_interval * 1000;
	if (delay.sec >= (long)acc->cfg.reg_retry_random_interval) {
	    delay.msec = -rand_ms + (pj_rand() % (rand_ms * 2));
	} else {
	    delay.sec = 0;
	    delay.msec = (pj_rand() % (delay.sec * 1000 + rand_ms));
	}
    }
    pj_time_val_normalize(&delay);

    PJ_LOG(4,(THIS_FILE,
	      "Scheduling re-registration retry for acc %d in %u seconds..",
	      acc->index, delay.sec));

    acc->auto_rereg.timer.id = PJ_TRUE;
    if (pjsua_schedule_timer(&acc->auto_rereg.timer, &delay) != PJ_SUCCESS)
	acc->auto_rereg.timer.id = PJ_FALSE;
}


/* Internal function to perform auto-reregistration on transport 
 * connection/disconnection events.
 */
void pjsua_acc_on_tp_state_changed(pjsip_transport *tp,
				   pjsip_transport_state state,
				   const pjsip_transport_state_info *info)
{
    unsigned i;

    PJ_UNUSED_ARG(info);

    /* Only care for transport disconnection events */
    if (state != PJSIP_TP_STATE_DISCONNECTED)
	return;

    PJ_LOG(4,(THIS_FILE, "Disconnected notification for transport %s",
	      tp->obj_name));
    pj_log_push_indent();

    /* Shutdown this transport, to make sure that the transport manager 
     * will create a new transport for reconnection.
     */
    pjsip_transport_shutdown(tp);

    PJSUA_LOCK();

    /* Enumerate accounts using this transport and perform actions
     * based on the transport state.
     */
    for (i = 0; i < PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	pjsua_acc *acc = &pjsua_var.acc[i];

	/* Skip if this account is not valid. */
	if (!acc->valid)
	    continue;

	/* Reset Account's via transport and via address */
	if (acc->via_tp == (void*)tp) {
	    pj_bzero(&acc->via_addr, sizeof(acc->via_addr));
	    acc->via_tp = NULL;

	    /* Also reset regc's Via addr */
	    if (acc->regc)
		pjsip_regc_set_via_sent_by(acc->regc, NULL, NULL);
	}

	/* Release transport immediately if regc is using it
	 * See https://trac.pjsip.org/repos/ticket/1481
	 */
	if (acc->regc) {
	    pjsip_regc_info reg_info;

	    pjsip_regc_get_info(acc->regc, &reg_info);
	    if (reg_info.transport != tp)
	        continue;

	    pjsip_regc_release_transport(pjsua_var.acc[i].regc);

	    if (pjsua_var.acc[i].ip_change_op ==
					    PJSUA_IP_CHANGE_OP_ACC_SHUTDOWN_TP)
	    {
		/* Before progressing to next step, report here. */
		if (pjsua_var.ua_cfg.cb.on_ip_change_progress) {
		    pjsua_ip_change_op_info ch_info;

		    pj_bzero(&ch_info, sizeof(ch_info));
		    ch_info.acc_shutdown_tp.acc_id = acc->index;

		    pjsua_var.ua_cfg.cb.on_ip_change_progress(
							 acc->ip_change_op,
							 PJ_SUCCESS,
							 &ch_info);
		}

		if (acc->cfg.allow_contact_rewrite) {
		    pjsua_acc_update_contact_on_ip_change(acc);
		} else {
		    pjsua_acc_handle_call_on_ip_change(acc);
		}
	    } else if (acc->cfg.reg_retry_interval) {
		/* Schedule reregistration for this account */
	        schedule_reregistration(acc);
	    }
	}
    }

    PJSUA_UNLOCK();
    pj_log_pop_indent();
}


/*
 * Internal function to update contact on ip change process.
 */
pj_status_t pjsua_acc_update_contact_on_ip_change(pjsua_acc *acc)
{
    pj_status_t status;
    pj_bool_t need_unreg = ((acc->cfg.contact_rewrite_method &
			     PJSUA_CONTACT_REWRITE_UNREGISTER) != 0);

    acc->ip_change_op = PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT;

    PJ_LOG(3, (THIS_FILE, "%.*s: send %sregistration triggered "
	       "by IP change", acc->cfg.id.slen,
	       acc->cfg.id.ptr, (need_unreg ? "un-" : "")));

    status = pjsua_acc_set_registration(acc->index, !need_unreg);

    if ((status != PJ_SUCCESS) && (pjsua_var.ua_cfg.cb.on_ip_change_progress)
	&& (acc->ip_change_op == PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT))
    {
	/* If update contact fails, notification might already been triggered
	 * from registration callback.
	 */
	pjsua_ip_change_op_info info;

	pj_bzero(&info, sizeof(info));
	info.acc_update_contact.acc_id = acc->index;
	info.acc_update_contact.is_register = !need_unreg;

	pjsua_var.ua_cfg.cb.on_ip_change_progress(acc->ip_change_op,
						  status,
						  &info);

	pjsua_acc_end_ip_change(acc);
    }
    return status;
}


/*
 * Internal function to handle call on ip change process.
 */
pj_status_t pjsua_acc_handle_call_on_ip_change(pjsua_acc *acc)
{
    pj_status_t status = PJ_SUCCESS;
    unsigned i = 0;

    PJSUA_LOCK();
    if (acc->cfg.ip_change_cfg.hangup_calls ||
	acc->cfg.ip_change_cfg.reinvite_flags)
    {
	for (i = 0; i < (int)pjsua_var.ua_cfg.max_calls; ++i) {
	    pjsua_call_info call_info;
	    pjsua_call_get_info(i, &call_info);

	    if (pjsua_var.calls[i].acc_id != acc->index)
	    {
		continue;
	    }

	    if ((acc->cfg.ip_change_cfg.hangup_calls) &&
		(call_info.state >= PJSIP_INV_STATE_EARLY))
	    {
		acc->ip_change_op = PJSUA_IP_CHANGE_OP_ACC_HANGUP_CALLS;
		PJ_LOG(3, (THIS_FILE, "call to %.*s: hangup "
			   "triggered by IP change",
			   (int)call_info.remote_info.slen,
			   call_info.remote_info.ptr));

		status = pjsua_call_hangup(i, PJSIP_SC_GONE, NULL, NULL);

		if (pjsua_var.ua_cfg.cb.on_ip_change_progress) {
		    pjsua_ip_change_op_info info;

		    pj_bzero(&info, sizeof(info));
		    info.acc_hangup_calls.acc_id = acc->index;
		    info.acc_hangup_calls.call_id = call_info.id;

		    pjsua_var.ua_cfg.cb.on_ip_change_progress(
							     acc->ip_change_op,
							     status,
							     &info);
		}
	    } else if ((acc->cfg.ip_change_cfg.reinvite_flags) &&
		(call_info.state == PJSIP_INV_STATE_CONFIRMED))
	    {
		acc->ip_change_op = PJSUA_IP_CHANGE_OP_ACC_REINVITE_CALLS;

		pjsua_call_cleanup_flag(&call_info.setting);
		call_info.setting.flag |=
					 acc->cfg.ip_change_cfg.reinvite_flags;

		PJ_LOG(3, (THIS_FILE, "call to %.*s: send "
			   "re-INVITE with flags 0x%x triggered "
			   "by IP change (IP change flag: 0x%x)",
			   call_info.remote_info.slen,
			   call_info.remote_info.ptr,
			   call_info.setting.flag,
			   acc->cfg.ip_change_cfg.reinvite_flags));

		status = pjsua_call_reinvite(i, call_info.setting.flag, NULL);

		if (pjsua_var.ua_cfg.cb.on_ip_change_progress) {
		    pjsua_ip_change_op_info info;

		    pj_bzero(&info, sizeof(info));
		    info.acc_reinvite_calls.acc_id = acc->index;
		    info.acc_reinvite_calls.call_id = call_info.id;

		    pjsua_var.ua_cfg.cb.on_ip_change_progress(
							     acc->ip_change_op,
							     status,
							     &info);
		}

	    }
	}
    }
    pjsua_acc_end_ip_change(acc);
    PJSUA_UNLOCK();
    return status;
}

void pjsua_acc_end_ip_change(pjsua_acc *acc)
{
    int i = 0;
    pj_bool_t all_done = PJ_TRUE;

    PJSUA_LOCK();
    if (acc && acc->ip_change_op < PJSUA_IP_CHANGE_OP_COMPLETED) {
	PJ_LOG(3, (THIS_FILE, "IP address change handling for acc %d "
		   "completed", acc->index));
	acc->ip_change_op = PJSUA_IP_CHANGE_OP_COMPLETED;
	if (pjsua_var.acc_cnt) {
	    for (; i < (int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
		if (pjsua_var.acc[i].valid &&
		    pjsua_var.acc[i].ip_change_op !=
						  PJSUA_IP_CHANGE_OP_COMPLETED)
		{
		    all_done = PJ_FALSE;
		    break;
		}
	    }
	}
    }
    if (all_done && pjsua_var.ua_cfg.cb.on_ip_change_progress) {
	PJ_LOG(3, (THIS_FILE, "IP address change handling completed"));
	pjsua_var.ua_cfg.cb.on_ip_change_progress(
					    PJSUA_IP_CHANGE_OP_COMPLETED,
					    PJ_SUCCESS,
					    NULL);
    }
    PJSUA_UNLOCK();
}

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


#define THIS_FILE   "pjsua_core.c"

#define DEFAULT_RTP_PORT	4000


/* Internal prototypes */
static void resolve_stun_entry(pjsua_stun_resolve *sess);


/* PJSUA application instance. */
struct pjsua_data pjsua_var;


PJ_DEF(struct pjsua_data*) pjsua_get_var(void)
{
    return &pjsua_var;
}


/* Display error */
PJ_DEF(void) pjsua_perror( const char *sender, const char *title, 
			   pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(sender, "%s: %s [status=%d]", title, errmsg, status));
}


static void init_data()
{
    unsigned i;

    pj_bzero(&pjsua_var, sizeof(pjsua_var));

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i)
	pjsua_var.acc[i].index = i;
    
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.tpdata); ++i)
	pjsua_var.tpdata[i].index = i;

    pjsua_var.stun_status = PJ_EUNKNOWN;
    pjsua_var.nat_status = PJ_EPENDING;
    pj_list_init(&pjsua_var.stun_res);
    pj_list_init(&pjsua_var.outbound_proxy);

    pjsua_config_default(&pjsua_var.ua_cfg);

    for (i=0; i<PJSUA_MAX_VID_WINS; ++i) {
	pjsua_vid_win_reset(i);
    }
}


PJ_DEF(void) pjsua_logging_config_default(pjsua_logging_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->msg_logging = PJ_TRUE;
    cfg->level = 5;
    cfg->console_level = 4;
    cfg->decor = PJ_LOG_HAS_SENDER | PJ_LOG_HAS_TIME | 
		 PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_NEWLINE |
		 PJ_LOG_HAS_SPACE | PJ_LOG_HAS_THREAD_SWC |
		 PJ_LOG_HAS_INDENT;
#if (defined(PJ_WIN32) && PJ_WIN32 != 0) || (defined(PJ_WIN64) && PJ_WIN64 != 0)
    cfg->decor |= PJ_LOG_HAS_COLOR;
#endif
}

PJ_DEF(void) pjsua_logging_config_dup(pj_pool_t *pool,
				      pjsua_logging_config *dst,
				      const pjsua_logging_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));
    pj_strdup_with_null(pool, &dst->log_filename, &src->log_filename);
}

PJ_DEF(void) pjsua_config_default(pjsua_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->max_calls = ((PJSUA_MAX_CALLS) < 4) ? (PJSUA_MAX_CALLS) : 4;
    cfg->thread_cnt = PJSUA_SEPARATE_WORKER_FOR_TIMER? 2 : 1;
    cfg->nat_type_in_sdp = 1;
    cfg->stun_ignore_failure = PJ_TRUE;
    cfg->force_lr = PJ_TRUE;
    cfg->enable_unsolicited_mwi = PJ_TRUE;
    cfg->use_srtp = PJSUA_DEFAULT_USE_SRTP;
    cfg->srtp_secure_signaling = PJSUA_DEFAULT_SRTP_SECURE_SIGNALING;
    cfg->hangup_forked_call = PJ_TRUE;

    cfg->use_timer = PJSUA_SIP_TIMER_OPTIONAL;
    pjsip_timer_setting_default(&cfg->timer_setting);
    pjsua_srtp_opt_default(&cfg->srtp_opt);
}

PJ_DEF(void) pjsua_config_dup(pj_pool_t *pool,
			      pjsua_config *dst,
			      const pjsua_config *src)
{
    unsigned i;

    pj_memcpy(dst, src, sizeof(*src));

    for (i=0; i<src->outbound_proxy_cnt; ++i) {
	pj_strdup_with_null(pool, &dst->outbound_proxy[i],
			    &src->outbound_proxy[i]);
    }

    for (i=0; i<src->cred_count; ++i) {
	pjsip_cred_dup(pool, &dst->cred_info[i], &src->cred_info[i]);
    }

    pj_strdup_with_null(pool, &dst->user_agent, &src->user_agent);
    pj_strdup_with_null(pool, &dst->stun_domain, &src->stun_domain);
    pj_strdup_with_null(pool, &dst->stun_host, &src->stun_host);

    for (i=0; i<src->stun_srv_cnt; ++i) {
	pj_strdup_with_null(pool, &dst->stun_srv[i], &src->stun_srv[i]);
    }

    pjsua_srtp_opt_dup(pool, &dst->srtp_opt, &src->srtp_opt, PJ_FALSE);
}

PJ_DEF(void) pjsua_msg_data_init(pjsua_msg_data *msg_data)
{
    pj_bzero(msg_data, sizeof(*msg_data));
    pj_list_init(&msg_data->hdr_list);
    pjsip_media_type_init(&msg_data->multipart_ctype, NULL, NULL);
    pj_list_init(&msg_data->multipart_parts);
}

PJ_DEF(pjsua_msg_data*) pjsua_msg_data_clone(pj_pool_t *pool,
                                             const pjsua_msg_data *rhs)
{
    pjsua_msg_data *msg_data;
    const pjsip_hdr *hdr;
    const pjsip_multipart_part *mpart;

    PJ_ASSERT_RETURN(pool && rhs, NULL);

    msg_data = PJ_POOL_ZALLOC_T(pool, pjsua_msg_data);
    PJ_ASSERT_RETURN(msg_data != NULL, NULL);

    pj_strdup(pool, &msg_data->target_uri, &rhs->target_uri);

    pj_list_init(&msg_data->hdr_list);
    hdr = rhs->hdr_list.next;
    while (hdr != &rhs->hdr_list) {
	pj_list_push_back(&msg_data->hdr_list, pjsip_hdr_clone(pool, hdr));
	hdr = hdr->next;
    }

    pj_strdup(pool, &msg_data->content_type, &rhs->content_type);
    pj_strdup(pool, &msg_data->msg_body, &rhs->msg_body);

    pjsip_media_type_cp(pool, &msg_data->multipart_ctype,
                        &rhs->multipart_ctype);

    pj_list_init(&msg_data->multipart_parts);
    mpart = rhs->multipart_parts.next;
    while (mpart != &rhs->multipart_parts) {
	pj_list_push_back(&msg_data->multipart_parts,
                          pjsip_multipart_clone_part(pool, mpart));
	mpart = mpart->next;
    }

    return msg_data;
}

PJ_DEF(void) pjsua_transport_config_default(pjsua_transport_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    pjsip_tls_setting_default(&cfg->tls_setting);
}

PJ_DEF(void) pjsua_transport_config_dup(pj_pool_t *pool,
					pjsua_transport_config *dst,
					const pjsua_transport_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));
    pj_strdup(pool, &dst->public_addr, &src->public_addr);
    pj_strdup(pool, &dst->bound_addr, &src->bound_addr);
}

PJ_DEF(void) pjsua_ice_config_from_media_config( pj_pool_t *pool,
                                           pjsua_ice_config *dst,
                                           const pjsua_media_config *src)
{
    PJ_UNUSED_ARG(pool);

    dst->enable_ice = src->enable_ice;
    dst->ice_max_host_cands = src->ice_max_host_cands;
    dst->ice_opt = src->ice_opt;
    dst->ice_no_rtcp = src->ice_no_rtcp;
    dst->ice_always_update = src->ice_always_update;
}

PJ_DEF(void) pjsua_ice_config_dup( pj_pool_t *pool,
                                pjsua_ice_config *dst,
                                const pjsua_ice_config *src)
{
    PJ_UNUSED_ARG(pool);
    pj_memcpy(dst, src, sizeof(*src));
}

PJ_DEF(void) pjsua_turn_config_from_media_config(pj_pool_t *pool,
                                                 pjsua_turn_config *dst,
                                                 const pjsua_media_config *src)
{
    dst->enable_turn = src->enable_turn;
    dst->turn_conn_type = src->turn_conn_type;
    if (pool == NULL) {
	dst->turn_server = src->turn_server;
	dst->turn_auth_cred = src->turn_auth_cred;

#if PJ_HAS_SSL_SOCK
	pj_memcpy(&dst->turn_tls_setting, &src->turn_tls_setting,
		  sizeof(src->turn_tls_setting));
#endif
    } else {
	if (pj_stricmp(&dst->turn_server, &src->turn_server))
	    pj_strdup(pool, &dst->turn_server, &src->turn_server);
	pj_stun_auth_cred_dup(pool, &dst->turn_auth_cred,
	                      &src->turn_auth_cred);

#if PJ_HAS_SSL_SOCK
	pj_turn_sock_tls_cfg_dup(pool, &dst->turn_tls_setting,
				 &src->turn_tls_setting);
#endif
    }
}

PJ_DEF(void) pjsua_turn_config_dup(pj_pool_t *pool,
                                   pjsua_turn_config *dst,
                                   const pjsua_turn_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));
    if (pool) {
	pj_strdup(pool, &dst->turn_server, &src->turn_server);
	pj_stun_auth_cred_dup(pool, &dst->turn_auth_cred,
	                      &src->turn_auth_cred);

#if PJ_HAS_SSL_SOCK
	pj_turn_sock_tls_cfg_dup(pool, &dst->turn_tls_setting,
				 &src->turn_tls_setting);
#endif
    }
}


PJ_DEF(void) pjsua_srtp_opt_default(pjsua_srtp_opt *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
}


PJ_DEF(void) pjsua_srtp_opt_dup( pj_pool_t *pool, pjsua_srtp_opt *dst,
                                 const pjsua_srtp_opt *src,
                                 pj_bool_t check_str)
{
    pjsua_srtp_opt backup_dst;
    
    if (check_str) pj_memcpy(&backup_dst, dst, sizeof(*dst));
    pj_memcpy(dst, src, sizeof(*src));

    if (pool) {
    	unsigned i;
    	
    	for (i = 0; i < src->crypto_count; i++) {
    	    if (!check_str ||
    	    	pj_stricmp(&backup_dst.crypto[i].key, &src->crypto[i].key))
    	    {
	    	pj_strdup(pool, &dst->crypto[i].key, &src->crypto[i].key);
	    } else {
	    	/* If strings are identical, use the old string to
	    	 * avoid wasting memory.
	    	 */
	    	dst->crypto[i].key = backup_dst.crypto[i].key;
	    }
    	    if (!check_str ||
    	    	pj_stricmp(&backup_dst.crypto[i].name, &src->crypto[i].name))
    	    {
	    	pj_strdup(pool, &dst->crypto[i].name, &src->crypto[i].name);
	    } else {
	    	/* If strings are identical, use the old string to
	    	 * avoid wasting memory.
	    	 */
	    	dst->crypto[i].name = backup_dst.crypto[i].name;
	    }
	}
    }
}


PJ_DEF(void) pjsua_acc_config_default(pjsua_acc_config *cfg)
{
    pjsua_media_config med_cfg;

    pj_bzero(cfg, sizeof(*cfg));

    cfg->reg_timeout = PJSUA_REG_INTERVAL;
    cfg->reg_delay_before_refresh = PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH;
    cfg->unreg_timeout = PJSUA_UNREG_TIMEOUT;
    pjsip_publishc_opt_default(&cfg->publish_opt);
    cfg->unpublish_max_wait_time_msec = PJSUA_UNPUBLISH_MAX_WAIT_TIME_MSEC;
    cfg->transport_id = PJSUA_INVALID_ID;
    cfg->allow_contact_rewrite = PJ_TRUE;
    cfg->allow_via_rewrite = PJ_TRUE;
    cfg->require_100rel = pjsua_var.ua_cfg.require_100rel;
    cfg->use_timer = pjsua_var.ua_cfg.use_timer;
    cfg->timer_setting = pjsua_var.ua_cfg.timer_setting;
    cfg->lock_codec = 1;
    cfg->ka_interval = 15;
    cfg->ka_data = pj_str("\r\n");
    cfg->vid_cap_dev = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
    cfg->vid_rend_dev = PJMEDIA_VID_DEFAULT_RENDER_DEV;
#if PJMEDIA_HAS_VIDEO
    pjmedia_vid_stream_rc_config_default(&cfg->vid_stream_rc_cfg);
    pjmedia_vid_stream_sk_config_default(&cfg->vid_stream_sk_cfg);
#endif
    pjsua_transport_config_default(&cfg->rtp_cfg);
    cfg->rtp_cfg.port = DEFAULT_RTP_PORT;
    pjmedia_rtcp_fb_setting_default(&cfg->rtcp_fb_cfg);

    pjsua_media_config_default(&med_cfg);
    pjsua_ice_config_from_media_config(NULL, &cfg->ice_cfg, &med_cfg);
    pjsua_turn_config_from_media_config(NULL, &cfg->turn_cfg, &med_cfg);

    cfg->use_srtp = pjsua_var.ua_cfg.use_srtp;
    cfg->srtp_secure_signaling = pjsua_var.ua_cfg.srtp_secure_signaling;
    cfg->srtp_optional_dup_offer = pjsua_var.ua_cfg.srtp_optional_dup_offer;
    cfg->srtp_opt = pjsua_var.ua_cfg.srtp_opt;
    cfg->reg_retry_interval = PJSUA_REG_RETRY_INTERVAL;
    cfg->reg_retry_random_interval = 10;
    cfg->contact_rewrite_method = PJSUA_CONTACT_REWRITE_METHOD;
    cfg->contact_use_src_port = PJ_TRUE;
    cfg->use_rfc5626 = PJ_TRUE;
    cfg->reg_use_proxy = PJSUA_REG_USE_OUTBOUND_PROXY |
			 PJSUA_REG_USE_ACC_PROXY;
#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    cfg->use_stream_ka = (PJMEDIA_STREAM_ENABLE_KA != 0);
#endif
    pj_list_init(&cfg->reg_hdr_list);
    pj_list_init(&cfg->sub_hdr_list);
    cfg->call_hold_type = PJSUA_CALL_HOLD_TYPE_DEFAULT;
    cfg->register_on_acc_add = PJ_TRUE;
    cfg->mwi_expires = PJSIP_MWI_DEFAULT_EXPIRES;

    cfg->media_stun_use = PJSUA_STUN_RETRY_ON_FAILURE;
    cfg->ip_change_cfg.shutdown_tp = PJ_TRUE;
    cfg->ip_change_cfg.hangup_calls = PJ_FALSE;
    cfg->ip_change_cfg.reinvite_flags = PJSUA_CALL_REINIT_MEDIA |
					PJSUA_CALL_UPDATE_CONTACT |
					PJSUA_CALL_UPDATE_VIA;
}

PJ_DEF(void) pjsua_buddy_config_default(pjsua_buddy_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
}

PJ_DEF(void) pjsua_media_config_default(pjsua_media_config *cfg)
{
    const pj_sys_info *si = pj_get_sys_info();
    pj_str_t dev_model = {"iPhone5", 7};
    
    pj_bzero(cfg, sizeof(*cfg));

    cfg->clock_rate = PJSUA_DEFAULT_CLOCK_RATE;
    /* It is reported that there may be some media server resampling problem
     * with iPhone 5 devices running iOS 7, so we set the sound device's
     * clock rate to 44100 to avoid resampling.
     */
    if (pj_stristr(&si->machine, &dev_model) &&
        ((si->os_ver & 0xFF000000) >> 24) >= 7)
    {
        cfg->snd_clock_rate = 44100;
    } else {
        cfg->snd_clock_rate = 0;
    }
    cfg->channel_count = 1;
    cfg->audio_frame_ptime = PJSUA_DEFAULT_AUDIO_FRAME_PTIME;
    cfg->max_media_ports = PJSUA_MAX_CONF_PORTS;
    cfg->has_ioqueue = PJ_TRUE;
    cfg->thread_cnt = 1;
    cfg->quality = PJSUA_DEFAULT_CODEC_QUALITY;
    cfg->ilbc_mode = PJSUA_DEFAULT_ILBC_MODE;
    cfg->ec_tail_len = PJSUA_DEFAULT_EC_TAIL_LEN;
    cfg->snd_rec_latency = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    cfg->snd_play_latency = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
    cfg->jb_init = cfg->jb_min_pre = cfg->jb_max_pre = cfg->jb_max = -1;
    cfg->snd_auto_close_time = 1;

    cfg->ice_max_host_cands = -1;
    cfg->ice_always_update = PJ_TRUE;
    pj_ice_sess_options_default(&cfg->ice_opt);

    cfg->turn_conn_type = PJ_TURN_TP_UDP;
#if PJ_HAS_SSL_SOCK
    pj_turn_sock_tls_cfg_default(&cfg->turn_tls_setting);
#endif
    cfg->vid_preview_enable_native = PJ_TRUE;
}

/*****************************************************************************
 * This is a very simple PJSIP module, whose sole purpose is to display
 * incoming and outgoing messages to log. This module will have priority
 * higher than transport layer, which means:
 *
 *  - incoming messages will come to this module first before reaching
 *    transaction layer.
 *
 *  - outgoing messages will come to this module last, after the message
 *    has been 'printed' to contiguous buffer by transport layer and
 *    appropriate transport instance has been decided for this message.
 *
 */

/* Notification on incoming messages */
static pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
{
    char addr[PJ_INET6_ADDRSTRLEN+10];
    pj_str_t input_str = pj_str(rdata->pkt_info.src_name);

    PJ_LOG(4,(THIS_FILE, "RX %d bytes %s from %s %s:\n"
			 "%.*s\n"
			 "--end msg--",
			 rdata->msg_info.len,
			 pjsip_rx_data_get_info(rdata),
			 rdata->tp_info.transport->type_name,	      
			 pj_addr_str_print(&input_str, 
					   rdata->pkt_info.src_port, 
					   addr,
					   sizeof(addr), 
					   1),
			 (int)rdata->msg_info.len,
			 rdata->msg_info.msg_buf));
    
    /* Always return false, otherwise messages will not get processed! */
    return PJ_FALSE;
}

/* Notification on outgoing messages */
static pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
{
    char addr[PJ_INET6_ADDRSTRLEN+10];
    pj_str_t input_str = pj_str(tdata->tp_info.dst_name);
    
    /* Important note:
     *	tp_info field is only valid after outgoing messages has passed
     *	transport layer. So don't try to access tp_info when the module
     *	has lower priority than transport layer.
     */
    PJ_LOG(4,(THIS_FILE, "TX %d bytes %s to %s %s:\n"
			 "%.*s\n"
			 "--end msg--",
			 (tdata->buf.cur - tdata->buf.start),
			 pjsip_tx_data_get_info(tdata),
			 tdata->tp_info.transport->type_name,
			 pj_addr_str_print(&input_str, 
					   tdata->tp_info.dst_port, 
					   addr,
					   sizeof(addr), 
					   1),
			 (int)(tdata->buf.cur - tdata->buf.start),
			 tdata->buf.start));


    /* Always return success, otherwise message will not get sent! */
    return PJ_SUCCESS;
}

/* The module instance. */
static pjsip_module pjsua_msg_logger = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-log", 13 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &logging_on_rx_msg,			/* on_rx_request()	*/
    &logging_on_rx_msg,			/* on_rx_response()	*/
    &logging_on_tx_msg,			/* on_tx_request.	*/
    &logging_on_tx_msg,			/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};


/*****************************************************************************
 * Another simple module to handle incoming OPTIONS request
 */

/* Notification on incoming request */
static pj_bool_t options_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;
    pjsip_response_addr res_addr;
    const pjsip_hdr *cap_hdr;
    pj_status_t status;

    /* Only want to handle OPTIONS requests */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
			 pjsip_get_options_method()) != 0)
    {
	return PJ_FALSE;
    }

    /* Don't want to handle if shutdown is in progress */
    if (pjsua_var.thread_quit_flag) {
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 
				      PJSIP_SC_TEMPORARILY_UNAVAILABLE, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    /* Create basic response. */
    status = pjsip_endpt_create_response(pjsua_var.endpt, rdata, 200, NULL, 
					 &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create OPTIONS response", status);
	return PJ_TRUE;
    }

    /* Add Allow header */
    cap_hdr = pjsip_endpt_get_capability(pjsua_var.endpt, PJSIP_H_ALLOW, NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Accept header */
    cap_hdr = pjsip_endpt_get_capability(pjsua_var.endpt, PJSIP_H_ACCEPT, NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Supported header */
    cap_hdr = pjsip_endpt_get_capability(pjsua_var.endpt, PJSIP_H_SUPPORTED, NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Allow-Events header from the evsub module */
    cap_hdr = pjsip_evsub_get_allow_events_hdr(NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add User-Agent header */
    if (pjsua_var.ua_cfg.user_agent.slen) {
	const pj_str_t USER_AGENT = { "User-Agent", 10};
	pjsip_hdr *h;

	h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool,
							 &USER_AGENT,
							 &pjsua_var.ua_cfg.user_agent);
	pjsip_msg_add_hdr(tdata->msg, h);
    }

    /* Get media socket info, make sure transport is ready */
#if DISABLED_FOR_TICKET_1185
    if (pjsua_var.calls[0].med_tp) {
	pjmedia_transport_info tpinfo;
	pjmedia_sdp_session *sdp;

	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(pjsua_var.calls[0].med_tp, &tpinfo);

	/* Add SDP body, using call0's RTP address */
	status = pjmedia_endpt_create_sdp(pjsua_var.med_endpt, tdata->pool, 1,
					  &tpinfo.sock_info, &sdp);
	if (status == PJ_SUCCESS) {
	    pjsip_create_sdp_body(tdata->pool, sdp, &tdata->msg->body);
	}
    }
#endif

    /* Send response */
    pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
    status = pjsip_endpt_send_response(pjsua_var.endpt, &res_addr, tdata, NULL, NULL);
    if (status != PJ_SUCCESS)
	pjsip_tx_data_dec_ref(tdata);

    return PJ_TRUE;
}


/* The module instance. */
static pjsip_module pjsua_options_handler = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-options", 17 },	/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &options_on_rx_request,		/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};


/*****************************************************************************
 * These two functions are the main callbacks registered to PJSIP stack
 * to receive SIP request and response messages that are outside any
 * dialogs and any transactions.
 */

/*
 * Handler for receiving incoming requests.
 *
 * This handler serves multiple purposes:
 *  - it receives requests outside dialogs.
 *  - it receives requests inside dialogs, when the requests are
 *    unhandled by other dialog usages. Example of these
 *    requests are: MESSAGE.
 */
static pj_bool_t mod_pjsua_on_rx_request(pjsip_rx_data *rdata)
{
    pj_bool_t processed = PJ_FALSE;

    PJSUA_LOCK();

    if (rdata->msg_info.msg->line.req.method.id == PJSIP_INVITE_METHOD) {

	processed = pjsua_call_on_incoming(rdata);
    }

    PJSUA_UNLOCK();

    return processed;
}


/*
 * Handler for receiving incoming responses.
 *
 * This handler serves multiple purposes:
 *  - it receives strayed responses (i.e. outside any dialog and
 *    outside any transactions).
 *  - it receives responses coming to a transaction, when pjsua
 *    module is set as transaction user for the transaction.
 *  - it receives responses inside a dialog, when these responses
 *    are unhandled by other dialog usages.
 */
static pj_bool_t mod_pjsua_on_rx_response(pjsip_rx_data *rdata)
{
    PJ_UNUSED_ARG(rdata);
    return PJ_FALSE;
}


/*****************************************************************************
 * Logging.
 */

/* Log callback */
static void log_writer(int level, const char *buffer, int len)
{
    /* Write to file, stdout or application callback. */

    if (pjsua_var.log_file) {
	pj_ssize_t size = len;
	pj_file_write(pjsua_var.log_file, buffer, &size);
	/* This will slow things down considerably! Don't do it!
	 pj_file_flush(pjsua_var.log_file);
	*/
    }

    if (level <= (int)pjsua_var.log_cfg.console_level) {
	if (pjsua_var.log_cfg.cb)
	    (*pjsua_var.log_cfg.cb)(level, buffer, len);
	else
	    pj_log_write(level, buffer, len);
    }
}


/*
 * Application can call this function at any time (after pjsua_create(), of
 * course) to change logging settings.
 */
PJ_DEF(pj_status_t) pjsua_reconfigure_logging(const pjsua_logging_config *cfg)
{
    pj_status_t status;

    /* Save config. */
    pjsua_logging_config_dup(pjsua_var.pool, &pjsua_var.log_cfg, cfg);

    /* Redirect log function to ours */
    pj_log_set_log_func( &log_writer );

    /* Set decor */
    pj_log_set_decor(pjsua_var.log_cfg.decor);

    /* Set log level */
    pj_log_set_level(pjsua_var.log_cfg.level);

    /* Close existing file, if any */
    if (pjsua_var.log_file) {
	pj_file_close(pjsua_var.log_file);
	pjsua_var.log_file = NULL;
    }

    /* If output log file is desired, create the file: */
    if (pjsua_var.log_cfg.log_filename.slen) {
	unsigned flags = PJ_O_WRONLY;
	flags |= pjsua_var.log_cfg.log_file_flags;
	status = pj_file_open(pjsua_var.pool, 
			      pjsua_var.log_cfg.log_filename.ptr,
			      flags, 
			      &pjsua_var.log_file);

	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating log file", status);
	    return status;
	}
    }

    /* Unregister msg logging if it's previously registered */
    if (pjsua_msg_logger.id >= 0) {
	pjsip_endpt_unregister_module(pjsua_var.endpt, &pjsua_msg_logger);
	pjsua_msg_logger.id = -1;
    }

    /* Enable SIP message logging */
    if (pjsua_var.log_cfg.msg_logging)
	pjsip_endpt_register_module(pjsua_var.endpt, &pjsua_msg_logger);

    return PJ_SUCCESS;
}


/*****************************************************************************
 * PJSUA Base API.
 */

/* Worker thread function. */
static int worker_thread(void *arg)
{
    enum { TIMEOUT = 10 };

    PJ_UNUSED_ARG(arg);

    while (!pjsua_var.thread_quit_flag) {
	int count;

	count = pjsua_handle_events(TIMEOUT);
	if (count < 0)
	    pj_thread_sleep(TIMEOUT);
    }

    return 0;
}

#if PJSUA_SEPARATE_WORKER_FOR_TIMER

/* Timer heap worker thread function. */
static int worker_thread_timer(void *arg)
{
    pj_timer_heap_t *th;

    PJ_UNUSED_ARG(arg);

    th = pjsip_endpt_get_timer_heap(pjsua_var.endpt);
    while (!pjsua_var.thread_quit_flag) {
	pj_time_val timeout = {0, 0};
	int c;

	c = pj_timer_heap_poll(th, &timeout);
	if (c == 0) {
	    /* Sleep if no event */
	    enum { MAX_SLEEP_MS = 100 };
	    if (PJ_TIME_VAL_MSEC(timeout) < MAX_SLEEP_MS)
		pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
	    else
		pj_thread_sleep(MAX_SLEEP_MS);
	}
    }
    return 0;
}

/* Ioqueue worker thread function. */
static int worker_thread_ioqueue(void *arg)
{
    pj_ioqueue_t *ioq;

    PJ_UNUSED_ARG(arg);

    ioq = pjsip_endpt_get_ioqueue(pjsua_var.endpt);
    while (!pjsua_var.thread_quit_flag) {
	pj_time_val timeout = {0, 100};
	pj_ioqueue_poll(ioq, &timeout);
    }
    return 0;
}

#endif

PJ_DEF(void) pjsua_stop_worker_threads(void)
{
    unsigned i;

    pjsua_var.thread_quit_flag = 1;

    /* Wait worker threads to quit: */
    for (i=0; i<(int)pjsua_var.ua_cfg.thread_cnt; ++i) {
    	if (pjsua_var.thread[i]) {
    	    pj_status_t status;
    	    status = pj_thread_join(pjsua_var.thread[i]);
    	    if (status != PJ_SUCCESS) {
    		PJ_PERROR(4,(THIS_FILE, status, "Error joining worker thread"));
    		pj_thread_sleep(1000);
    	    }
    	    pj_thread_destroy(pjsua_var.thread[i]);
    	    pjsua_var.thread[i] = NULL;
    	}
    }
}

/* Init random seed */
static void init_random_seed(void)
{
    pj_sockaddr addr;
    const pj_str_t *hostname;
    pj_uint32_t pid;
    pj_time_val t;
    unsigned seed=0;

    /* Add hostname */
    hostname = pj_gethostname();
    seed = pj_hash_calc(seed, hostname->ptr, (int)hostname->slen);

    /* Add primary IP address */
    if (pj_gethostip(pj_AF_INET(), &addr)==PJ_SUCCESS)
	seed = pj_hash_calc(seed, &addr.ipv4.sin_addr, 4);

    /* Get timeofday */
    pj_gettimeofday(&t);
    seed = pj_hash_calc(seed, &t, sizeof(t));

    /* Add PID */
    pid = pj_getpid();
    seed = pj_hash_calc(seed, &pid, sizeof(pid));

    /* Init random seed */
    pj_srand(seed);
}

/*
 * Instantiate pjsua application.
 */
PJ_DEF(pj_status_t) pjsua_create(void)
{
    pj_status_t status;

    /* Init pjsua data */
    init_data();

    /* Set default logging settings */
    pjsua_logging_config_default(&pjsua_var.log_cfg);

    /* Init PJLIB: */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    pj_log_push_indent();

    /* Init random seed */
    init_random_seed();

    /* Init PJLIB-UTIL: */
    status = pjlib_util_init();
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	pjsua_perror(THIS_FILE, "Failed in initializing pjlib-util", status);
	pj_shutdown();
	return status;
    }

    /* Init PJNATH */
    status = pjnath_init();
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	pjsua_perror(THIS_FILE, "Failed in initializing pjnath", status);
	pj_shutdown();
	return status;
    }

    /* Set default sound device ID */
    pjsua_var.cap_dev = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
    pjsua_var.play_dev = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

    /* Set default video device ID */
    pjsua_var.vcap_dev = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
    pjsua_var.vrdr_dev = PJMEDIA_VID_DEFAULT_RENDER_DEV;

    /* Init caching pool. */
    pj_caching_pool_init(&pjsua_var.cp, NULL, 0);

    /* Create memory pools for application and internal use. */
    pjsua_var.pool = pjsua_pool_create("pjsua", 1000, 1000);
    pjsua_var.timer_pool = pjsua_pool_create("pjsua_timer", 500, 500);
    if (pjsua_var.pool == NULL || pjsua_var.timer_pool == NULL) {
	pj_log_pop_indent();
	status = PJ_ENOMEM;
	pjsua_perror(THIS_FILE, "Unable to create pjsua/timer pool", status);
	pj_shutdown();
	return status;
    }
    
    /* Create mutex */
    status = pj_mutex_create_recursive(pjsua_var.pool, "pjsua", 
				       &pjsua_var.mutex);
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	pjsua_perror(THIS_FILE, "Unable to create mutex", status);
	pjsua_destroy();
	return status;
    }

    /* Must create SIP endpoint to initialize SIP parser. The parser
     * is needed for example when application needs to call pjsua_verify_url().
     */
    status = pjsip_endpt_create(&pjsua_var.cp.factory, 
				pj_gethostname()->ptr, 
				&pjsua_var.endpt);
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	pjsua_perror(THIS_FILE, "Unable to create endpoint", status);
	pjsua_destroy();
	return status;
    }

    /* Init timer entry and event list */
    pj_list_init(&pjsua_var.timer_list);
    pj_list_init(&pjsua_var.event_list);

    /* Create timer mutex */
    status = pj_mutex_create_recursive(pjsua_var.pool, "pjsua_timer", 
				       &pjsua_var.timer_mutex);
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	pjsua_perror(THIS_FILE, "Unable to create mutex", status);
	pjsua_destroy();
	return status;
    }

    pjsua_set_state(PJSUA_STATE_CREATED);
    pj_log_pop_indent();
    return PJ_SUCCESS;
}


/*
 * Initialize pjsua with the specified settings. All the settings are 
 * optional, and the default values will be used when the config is not
 * specified.
 */
PJ_DEF(pj_status_t) pjsua_init( const pjsua_config *ua_cfg,
				const pjsua_logging_config *log_cfg,
				const pjsua_media_config *media_cfg)
{
    pjsua_config	 default_cfg;
    pjsua_media_config	 default_media_cfg;
    const pj_str_t	 STR_OPTIONS = { "OPTIONS", 7 };
    pjsip_ua_init_param  ua_init_param;
    unsigned i;
    pj_status_t status;

    pj_log_push_indent();

    /* Create default configurations when the config is not supplied */

    if (ua_cfg == NULL) {
	pjsua_config_default(&default_cfg);
	ua_cfg = &default_cfg;
    }

    if (media_cfg == NULL) {
	pjsua_media_config_default(&default_media_cfg);
	media_cfg = &default_media_cfg;
    }

    /* Initialize logging first so that info/errors can be captured */
    if (log_cfg) {
	status = pjsua_reconfigure_logging(log_cfg);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT != 0
    if (!(pj_get_sys_info()->flags & PJ_SYS_HAS_IOS_BG)) {
	PJ_LOG(5, (THIS_FILE, "Device does not support "
			      "background mode"));
	pj_activesock_enable_iphone_os_bg(PJ_FALSE);
    }
#endif

    /* If nameserver is configured, create DNS resolver instance and
     * set it to be used by SIP resolver.
     */
    if (ua_cfg->nameserver_count) {
#if PJSIP_HAS_RESOLVER
	unsigned ii;

	/* Create DNS resolver */
	status = pjsip_endpt_create_resolver(pjsua_var.endpt, 
					     &pjsua_var.resolver);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating resolver", status);
	    goto on_error;
	}

	/* Configure nameserver for the DNS resolver */
	status = pj_dns_resolver_set_ns(pjsua_var.resolver, 
					ua_cfg->nameserver_count,
					ua_cfg->nameserver, NULL);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error setting nameserver", status);
	    goto on_error;
	}

	/* Set this DNS resolver to be used by the SIP resolver */
	status = pjsip_endpt_set_resolver(pjsua_var.endpt, pjsua_var.resolver);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error setting DNS resolver", status);
	    goto on_error;
	}

	/* Print nameservers */
	for (ii=0; ii<ua_cfg->nameserver_count; ++ii) {
	    PJ_LOG(4,(THIS_FILE, "Nameserver %.*s added",
		      (int)ua_cfg->nameserver[ii].slen,
		      ua_cfg->nameserver[ii].ptr));
	}
#else
	PJ_LOG(2,(THIS_FILE, 
		  "DNS resolver is disabled (PJSIP_HAS_RESOLVER==0)"));
#endif
    }

    /* Init SIP UA: */

    /* Initialize transaction layer: */
    status = pjsip_tsx_layer_init_module(pjsua_var.endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Initialize UA layer module: */
    pj_bzero(&ua_init_param, sizeof(ua_init_param));
    if (ua_cfg->hangup_forked_call) {
	ua_init_param.on_dlg_forked = &on_dlg_forked;
    }
    status = pjsip_ua_init_module( pjsua_var.endpt, &ua_init_param);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Initialize Replaces support. */
    status = pjsip_replaces_init_module( pjsua_var.endpt );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize 100rel support */
    status = pjsip_100rel_init_module(pjsua_var.endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize session timer support */
    status = pjsip_timer_init_module(pjsua_var.endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize and register PJSUA application module. */
    {
	const pjsip_module mod_initializer = 
	{
	NULL, NULL,		    /* prev, next.			*/
	{ "mod-pjsua", 9 },	    /* Name.				*/
	-1,			    /* Id				*/
	PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority			*/
	NULL,			    /* load()				*/
	NULL,			    /* start()				*/
	NULL,			    /* stop()				*/
	NULL,			    /* unload()				*/
	&mod_pjsua_on_rx_request,   /* on_rx_request()			*/
	&mod_pjsua_on_rx_response,  /* on_rx_response()			*/
	NULL,			    /* on_tx_request.			*/
	NULL,			    /* on_tx_response()			*/
	NULL,			    /* on_tsx_state()			*/
	};

	pjsua_var.mod = mod_initializer;

	status = pjsip_endpt_register_module(pjsua_var.endpt, &pjsua_var.mod);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    }

    /* Parse outbound proxies */
    for (i=0; i<ua_cfg->outbound_proxy_cnt; ++i) {
	pj_str_t tmp;
    	pj_str_t hname = { "Route", 5};
	pjsip_route_hdr *r;

	pj_strdup_with_null(pjsua_var.pool, &tmp, &ua_cfg->outbound_proxy[i]);

	r = (pjsip_route_hdr*)
	    pjsip_parse_hdr(pjsua_var.pool, &hname, tmp.ptr,
			    (unsigned)tmp.slen, NULL);
	if (r == NULL) {
	    pjsua_perror(THIS_FILE, "Invalid outbound proxy URI",
			 PJSIP_EINVALIDURI);
	    status = PJSIP_EINVALIDURI;
	    goto on_error;
	}

	if (pjsua_var.ua_cfg.force_lr) {
	    pjsip_sip_uri *sip_url;
	    if (!PJSIP_URI_SCHEME_IS_SIP(r->name_addr.uri) &&
		!PJSIP_URI_SCHEME_IS_SIPS(r->name_addr.uri))
	    {
		status = PJSIP_EINVALIDSCHEME;
		goto on_error;
	    }
	    sip_url = (pjsip_sip_uri*)r->name_addr.uri;
	    sip_url->lr_param = 1;
	}

	pj_list_push_back(&pjsua_var.outbound_proxy, r);
    }
    

    /* Initialize PJSUA call subsystem: */
    status = pjsua_call_subsys_init(ua_cfg);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Convert deprecated STUN settings */
    if (pjsua_var.ua_cfg.stun_srv_cnt==0) {
	if (pjsua_var.ua_cfg.stun_domain.slen) {
	    pjsua_var.ua_cfg.stun_srv[pjsua_var.ua_cfg.stun_srv_cnt++] = 
		pjsua_var.ua_cfg.stun_domain;
	}
	if (pjsua_var.ua_cfg.stun_host.slen) {
	    pjsua_var.ua_cfg.stun_srv[pjsua_var.ua_cfg.stun_srv_cnt++] = 
		pjsua_var.ua_cfg.stun_host;
	}
    }

    /* Start resolving STUN server */
    status = resolve_stun_server(PJ_FALSE, PJ_FALSE, 0);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	goto on_error;
    }

    /* Initialize PJSUA media subsystem */
    status = pjsua_media_subsys_init(media_cfg);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Init core SIMPLE module : */
    status = pjsip_evsub_init_module(pjsua_var.endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Init presence module: */
    status = pjsip_pres_init_module( pjsua_var.endpt, pjsip_evsub_instance());
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize MWI support */
    status = pjsip_mwi_init_module(pjsua_var.endpt, pjsip_evsub_instance());

    /* Init PUBLISH module */
    pjsip_publishc_init_module(pjsua_var.endpt);

    /* Init xfer/REFER module */
    status = pjsip_xfer_init_module( pjsua_var.endpt );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Init pjsua presence handler: */
    status = pjsua_pres_init();
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init out-of-dialog MESSAGE request handler. */
    status = pjsua_im_init();
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register OPTIONS handler */
    pjsip_endpt_register_module(pjsua_var.endpt, &pjsua_options_handler);

    /* Add OPTIONS in Allow header */
    pjsip_endpt_add_capability(pjsua_var.endpt, NULL, PJSIP_H_ALLOW,
			       NULL, 1, &STR_OPTIONS);

    /* Start worker thread if needed. */
    if (pjsua_var.ua_cfg.thread_cnt) {
	unsigned ii;

	if (pjsua_var.ua_cfg.thread_cnt > PJ_ARRAY_SIZE(pjsua_var.thread))
	    pjsua_var.ua_cfg.thread_cnt = PJ_ARRAY_SIZE(pjsua_var.thread);

#if PJSUA_SEPARATE_WORKER_FOR_TIMER
	if (pjsua_var.ua_cfg.thread_cnt < 2)
	    pjsua_var.ua_cfg.thread_cnt = 2;
#endif

	for (ii=0; ii<pjsua_var.ua_cfg.thread_cnt; ++ii) {
	    char tname[16];
	    
	    pj_ansi_snprintf(tname, sizeof(tname), "pjsua_%d", ii);

#if PJSUA_SEPARATE_WORKER_FOR_TIMER
	    if (ii == 0) {
		status = pj_thread_create(pjsua_var.pool, tname,
					  &worker_thread_timer,
					  NULL, 0, 0, &pjsua_var.thread[ii]);
	    } else {
		status = pj_thread_create(pjsua_var.pool, tname,
					  &worker_thread_ioqueue,
					  NULL, 0, 0, &pjsua_var.thread[ii]);
	    }
#else
	    status = pj_thread_create(pjsua_var.pool, tname, &worker_thread,
				      NULL, 0, 0, &pjsua_var.thread[ii]);
#endif
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
	PJ_LOG(4,(THIS_FILE, "%d SIP worker threads created", 
		  pjsua_var.ua_cfg.thread_cnt));
    } else {
	PJ_LOG(4,(THIS_FILE, "No SIP worker threads created"));
    }

    /* Done! */

    PJ_LOG(3,(THIS_FILE, "pjsua version %s for %s initialized", 
			 pj_get_version(), pj_get_sys_info()->info.ptr));

    pjsua_set_state(PJSUA_STATE_INIT);
    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    pj_log_pop_indent();
    return status;
}


/* Sleep with polling */
static void busy_sleep(unsigned msec)
{
    pj_time_val timeout, now;

    pj_gettickcount(&timeout);
    timeout.msec += msec;
    pj_time_val_normalize(&timeout);

    do {
	int i;
	i = msec / 10;
	while (pjsua_handle_events(10) > 0 && i > 0)
	    --i;
	pj_gettickcount(&now);
    } while (PJ_TIME_VAL_LT(now, timeout));
}

static void stun_resolve_add_ref(pjsua_stun_resolve *sess)
{
    ++sess->ref_cnt;
}


static void release_stun_session(pjsua_stun_resolve *sess)
{
    PJSUA_LOCK();
    pj_list_erase(sess);
    PJSUA_UNLOCK();

    pj_assert(sess->stun_sock==NULL);
    pj_pool_release(sess->pool);
}

static void destroy_stun_resolve_cb(pj_timer_heap_t *t, pj_timer_entry *e)
{
    pjsua_stun_resolve *sess = (pjsua_stun_resolve*)e->user_data;
    PJ_UNUSED_ARG(t);

    release_stun_session(sess);
}


static void destroy_stun_resolve(pjsua_stun_resolve *sess, pj_bool_t forced)
{
    pj_time_val timeout = {0, 0};

    if (sess->destroy_flag)
	return;

    sess->destroy_flag = PJ_TRUE;
    if (sess->stun_sock) {
        pj_stun_sock_destroy(sess->stun_sock);
        sess->stun_sock = NULL;
    }

    if (pjsua_var.stun_status == PJ_EUNKNOWN ||
    	pjsua_var.stun_status == PJ_EPENDING)
    {
        pjsua_var.stun_status = PJNATH_ESTUNDESTROYED;
    }

    if (forced) {
	release_stun_session(sess);
    } else {
	/* Schedule session clean up, it needs PJSUA lock and locking it here
	 * may cause deadlock as this function may be called by STUN socket
	 * while holding STUN socket lock, while application may wait for STUN
	 * resolution while holding PJSUA lock.
	 */
	pj_timer_entry_init(&sess->timer, 0, (void*)sess,
			    &destroy_stun_resolve_cb);
	pjsua_schedule_timer(&sess->timer, &timeout);
    }
}

static void stun_resolve_dec_ref(pjsua_stun_resolve *sess)
{
    int ref_cnt = --sess->ref_cnt;
    /* If the STUN resolution session is blocking, only the waiting thread
     * is allowed to destroy the session, otherwise it may cause deadlock.
     */
    if ((ref_cnt > 0) ||
	(sess->blocking && (sess->waiter != pj_thread_this()))) 
    {
	return;
    }

    destroy_stun_resolve(sess, PJ_FALSE);
}


/* This is the internal function to be called when STUN resolution
 * session (pj_stun_resolve) has completed.
 */
static void stun_resolve_complete(pjsua_stun_resolve *sess)
{
    pj_stun_resolve_result result;

    if (sess->has_result)
	goto on_return;

    pj_bzero(&result, sizeof(result));
    result.token = sess->token;
    result.status = sess->status;
    result.name = sess->srv[sess->idx];
    result.index = sess->idx;
    pj_memcpy(&result.addr, &sess->addr, sizeof(result.addr));
    sess->has_result = PJ_TRUE;

    if (result.status == PJ_SUCCESS) {
	char addr[PJ_INET6_ADDRSTRLEN+10];
	pj_sockaddr_print(&result.addr, addr, sizeof(addr), 3);
	PJ_LOG(4,(THIS_FILE, 
		  "STUN resolution success, using %.*s, address is %s",
		  (int)sess->srv[sess->idx].slen,
		  sess->srv[sess->idx].ptr,
		  addr));
    } else {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(result.status, errmsg, sizeof(errmsg));
	PJ_LOG(1,(THIS_FILE, "STUN resolution failed: %s", errmsg));
    }

    sess->cb(&result);

on_return:
    if (!sess->blocking) {
	stun_resolve_dec_ref(sess);
    }
}

/* This is the callback called by the STUN socket (pj_stun_sock)
 * to report it's state. We use this as part of testing the
 * STUN server.
 */
static pj_bool_t test_stun_on_status(pj_stun_sock *stun_sock, 
				     pj_stun_sock_op op,
				     pj_status_t status)
{
    pjsua_stun_resolve *sess;

    sess = (pjsua_stun_resolve*) pj_stun_sock_get_user_data(stun_sock);
    pj_assert(stun_sock == sess->stun_sock);

    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));

	PJ_LOG(4,(THIS_FILE, "STUN resolution for %.*s failed: %s",
		  (int)sess->srv[sess->idx].slen,
		  sess->srv[sess->idx].ptr, errmsg));

	if (op == PJ_STUN_SOCK_BINDING_OP && !sess->async_wait) {
	    /* Just return here, we will destroy the STUN socket and
	     * continue the STUN resolution later in resolve_stun_entry().
	     * For more details, please refer to ticket #1962.
	     */
            return PJ_FALSE;
	}

	pj_stun_sock_destroy(stun_sock);
	sess->stun_sock = NULL;

	stun_resolve_add_ref(sess);

	if (pjsua_var.ua_cfg.stun_try_ipv6 && sess->af == pj_AF_INET()) {
	    sess->af = pj_AF_INET6();
	} else {
	    ++sess->idx;
	    sess->af = pj_AF_INET();
	    if (sess->idx >= sess->count)
                sess->status = status;
        }

	resolve_stun_entry(sess);

	stun_resolve_dec_ref(sess);

	return PJ_FALSE;

    } else if (op == PJ_STUN_SOCK_BINDING_OP) {
	pj_stun_sock_info ssi;

	pj_stun_sock_get_info(stun_sock, &ssi);
	pj_memcpy(&sess->addr, &ssi.srv_addr, sizeof(sess->addr));

	stun_resolve_add_ref(sess);

	sess->status = PJ_SUCCESS;
	pj_stun_sock_destroy(stun_sock);
	sess->stun_sock = NULL;

	stun_resolve_complete(sess);

	stun_resolve_dec_ref(sess);

	return PJ_FALSE;

    } else
	return PJ_TRUE;
    
}

/* This is an internal function to resolve and test current
 * server entry in pj_stun_resolve session. It is called by
 * pjsua_resolve_stun_servers() and test_stun_on_status() above
 */
static void resolve_stun_entry(pjsua_stun_resolve *sess)
{
    pj_status_t status = PJ_EUNKNOWN;

    /* Loop while we have entry to try */
    for (; sess->idx < sess->count;
    	 (pjsua_var.ua_cfg.stun_try_ipv6 && sess->af == pj_AF_INET())?
	 sess->af = pj_AF_INET6(): (++sess->idx, sess->af = pj_AF_INET()))
    {
	int af;
	char target[64];
	pj_str_t hostpart;
	pj_uint16_t port;
	pj_stun_sock_cb stun_sock_cb;
	
	pj_assert(sess->idx < sess->count);

	if (pjsua_var.ua_cfg.stun_try_ipv6 &&
	    pjsua_var.stun_opt != PJSUA_NAT64_DISABLED &&
	    sess->af == pj_AF_INET())
	{
	    /* Skip IPv4 STUN resolution if NAT64 is not disabled. */
	    PJ_LOG(4,(THIS_FILE, "Skipping IPv4 resolution of STUN server "
	    			 "%s (%d of %d)", target,
	    			 sess->idx+1, sess->count));	    
	    continue;
	}

	pj_ansi_snprintf(target, sizeof(target), "%.*s",
			 (int)sess->srv[sess->idx].slen,
			 sess->srv[sess->idx].ptr);

	/* Parse the server entry into host:port */
	status = pj_sockaddr_parse2(pj_AF_UNSPEC(), 0, &sess->srv[sess->idx],
				    &hostpart, &port, &af);
	if (status != PJ_SUCCESS) {
    	    PJ_LOG(2,(THIS_FILE, "Invalid STUN server entry %s", target));
	    continue;
	}
	
	/* Use default port if not specified */
	if (port == 0)
	    port = PJ_STUN_PORT;

	pj_assert(sess->stun_sock == NULL);

	PJ_LOG(4,(THIS_FILE, "Trying STUN server %s %s (%d of %d)..",
		  target, (sess->af == pj_AF_INET()? "IPv4": "IPv6"),
		  sess->idx+1, sess->count));

	/* Use STUN_sock to test this entry */
	pj_bzero(&stun_sock_cb, sizeof(stun_sock_cb));
	stun_sock_cb.on_status = &test_stun_on_status;
	sess->async_wait = PJ_FALSE;
	status = pj_stun_sock_create(&pjsua_var.stun_cfg, "stunresolve",
				     sess->af, &stun_sock_cb,
				     NULL, sess, &sess->stun_sock);
	if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(THIS_FILE, 
		     "Error creating STUN socket for %s: %s",
		     target, errmsg));

	    continue;
	}

	status = pj_stun_sock_start(sess->stun_sock, &hostpart, port,
				    pjsua_var.resolver);
	if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(THIS_FILE, 
		     "Error starting STUN socket for %s: %s",
		     target, errmsg));

	    if (sess->stun_sock) {
		pj_stun_sock_destroy(sess->stun_sock);
		sess->stun_sock = NULL;
	    }
	    continue;
	}

	/* Done for now, testing will resume/complete asynchronously in
	 * stun_sock_cb()
	 */
	sess->async_wait = PJ_TRUE;
	return;
    }

    if (sess->idx >= sess->count) {
	/* No more entries to try */
	stun_resolve_add_ref(sess);
	pj_assert(status != PJ_SUCCESS || sess->status != PJ_EPENDING);
        if (sess->status == PJ_EPENDING)
            sess->status = status;
	stun_resolve_complete(sess);
	stun_resolve_dec_ref(sess);
    }
}


/*
 * Update STUN servers.
 */
PJ_DEF(pj_status_t) pjsua_update_stun_servers(unsigned count, pj_str_t srv[],
					      pj_bool_t wait)
{
    unsigned i;
    pj_status_t status;

    PJ_ASSERT_RETURN(count && srv, PJ_EINVAL);
    
    PJSUA_LOCK();

    pjsua_var.ua_cfg.stun_srv_cnt = count;
    for (i = 0; i < count; i++) {
        if (pj_strcmp(&pjsua_var.ua_cfg.stun_srv[i], &srv[i]))
            pj_strdup(pjsua_var.pool, &pjsua_var.ua_cfg.stun_srv[i], &srv[i]);
    }
    pjsua_var.stun_status = PJ_EUNKNOWN;

    PJSUA_UNLOCK();
    
    status = resolve_stun_server(wait, PJ_FALSE, 0);
    if (wait == PJ_FALSE && status == PJ_EPENDING)
        status = PJ_SUCCESS;

    return status;
}


/*
 * Resolve STUN server.
 */
PJ_DEF(pj_status_t) pjsua_resolve_stun_servers( unsigned count,
						pj_str_t srv[],
						pj_bool_t wait,
						void *token,
						pj_stun_resolve_cb cb)
{
    pj_pool_t *pool;
    pjsua_stun_resolve *sess;
    pj_status_t status;
    unsigned i, max_wait_ms;
    pj_timestamp start, now;

    PJ_ASSERT_RETURN(count && srv && cb, PJ_EINVAL);

    pool = pjsua_pool_create("stunres", 256, 256);
    if (!pool)
	return PJ_ENOMEM;

    sess = PJ_POOL_ZALLOC_T(pool, pjsua_stun_resolve);
    sess->pool = pool;
    sess->token = token;
    sess->cb = cb;
    sess->count = count;
    sess->blocking = wait;
    sess->waiter = pj_thread_this();
    sess->status = PJ_EPENDING;
    sess->af = pj_AF_INET();
    stun_resolve_add_ref(sess);
    sess->srv = (pj_str_t*) pj_pool_calloc(pool, count, sizeof(pj_str_t));
    for (i=0; i<count; ++i) {
	pj_strdup(pool, &sess->srv[i], &srv[i]);
    }

    PJSUA_LOCK();
    pj_list_push_back(&pjsua_var.stun_res, sess);
    PJSUA_UNLOCK();

    resolve_stun_entry(sess);

    if (!wait)
	return PJ_SUCCESS;

    /* Should limit the wait time to avoid deadlock. For example,
     * if app holds dlg/tsx lock, pjsua worker thread will block on
     * any dlg/tsx state change.
     */
    max_wait_ms = count * pjsua_var.stun_cfg.rto_msec * (1 << 7);
    pj_get_timestamp(&start);
    
    while ((sess->status == PJ_EPENDING) && (!sess->destroy_flag)) {
        /* If there is no worker thread or
         * the function is called from the only worker thread,
         * we have to handle the events here.
         */
        if (pjsua_var.thread[0] == NULL ||
            (pj_thread_this() == pjsua_var.thread[0] &&
             pjsua_var.ua_cfg.thread_cnt == 1))
            {
            pjsua_handle_events(50);
        } else {
            pj_thread_sleep(20);
        }

	pj_get_timestamp(&now);
	if (pj_elapsed_msec(&start, &now) > max_wait_ms)
	    sess->status = PJ_ETIMEDOUT;
    }

    status = sess->status;
    stun_resolve_dec_ref(sess);

    return status;
}

/*
 * Cancel pending STUN resolution.
 */
PJ_DEF(pj_status_t) pjsua_cancel_stun_resolution( void *token,
						  pj_bool_t notify_cb)
{
    pjsua_stun_resolve *sess;
    unsigned cancelled_count = 0;

    PJSUA_LOCK();
    sess = pjsua_var.stun_res.next;
    while (sess != &pjsua_var.stun_res) {
	pjsua_stun_resolve *next = sess->next;

	if (sess->token == token) {
	    sess->has_result = PJ_TRUE;
	    sess->status = PJ_ECANCELLED;
	    if (notify_cb) {
		pj_stun_resolve_result result;

		pj_bzero(&result, sizeof(result));
		result.token = token;
		result.status = PJ_ECANCELLED;

		sess->cb(&result);
	    }	    
	    ++cancelled_count;
	}

	sess = next;
    }
    PJSUA_UNLOCK();

    return cancelled_count ? PJ_SUCCESS : PJ_ENOTFOUND;
}

static void internal_stun_resolve_cb(const pj_stun_resolve_result *result)
{
    pjsua_var.stun_status = result->status;
    if ((result->status == PJ_SUCCESS) && (pjsua_var.ua_cfg.stun_srv_cnt>0)) {
	pj_memcpy(&pjsua_var.stun_srv, &result->addr, sizeof(result->addr));
	pjsua_var.stun_srv_idx = result->index;

	/* Perform NAT type detection if not yet */
	if (pjsua_var.nat_type == PJ_STUN_NAT_TYPE_UNKNOWN &&
	    !pjsua_var.nat_in_progress &&
	    pjsua_var.ua_cfg.nat_type_in_sdp)
	{
	    pjsua_detect_nat_type();
	}
    }
    
    if (pjsua_var.ua_cfg.cb.on_stun_resolution_complete)
    	(*pjsua_var.ua_cfg.cb.on_stun_resolution_complete)(result);
}

/*
 * Resolve STUN server.
 */
pj_status_t resolve_stun_server(pj_bool_t wait, pj_bool_t retry_if_cur_error,
				unsigned options)
{
    pjsua_var.stun_opt = options;

    /* Retry resolving if currently the STUN status is error */
    if (pjsua_var.stun_status != PJ_EPENDING &&
	pjsua_var.stun_status != PJ_SUCCESS &&
	retry_if_cur_error)
    {
	pjsua_var.stun_status = PJ_EUNKNOWN;
    }

    if (pjsua_var.stun_status == PJ_EUNKNOWN) {
	pj_status_t status;

	/* Initialize STUN configuration */
	pj_stun_config_init(&pjsua_var.stun_cfg, &pjsua_var.cp.factory, 0,
			    pjsip_endpt_get_ioqueue(pjsua_var.endpt),
			    pjsip_endpt_get_timer_heap(pjsua_var.endpt));

	/* Start STUN server resolution */
	if (pjsua_var.ua_cfg.stun_srv_cnt) {
	    pjsua_var.stun_status = PJ_EPENDING;
	    status = pjsua_resolve_stun_servers(pjsua_var.ua_cfg.stun_srv_cnt,
						pjsua_var.ua_cfg.stun_srv,
						wait, NULL,
						&internal_stun_resolve_cb);
	    if (wait || status != PJ_SUCCESS) {
		pjsua_var.stun_status = status;
	    }
	} else {
	    pjsua_var.stun_status = PJ_SUCCESS;
	}

    } else if (pjsua_var.stun_status == PJ_EPENDING) {
	/* STUN server resolution has been started, wait for the
	 * result.
	 */
	if (wait) {
	    unsigned max_wait_ms;
	    pj_timestamp start, now;

	    /* Should limit the wait time to avoid deadlock. For example,
	     * if app holds dlg/tsx lock, pjsua worker thread will block on
	     * any dlg/tsx state change.
	     */
	    max_wait_ms = pjsua_var.ua_cfg.stun_srv_cnt *
			  pjsua_var.stun_cfg.rto_msec * (1 << 7);
	    pj_get_timestamp(&start);

	    while (pjsua_var.stun_status == PJ_EPENDING) {		
                /* If there is no worker thread or
                 * the function is called from the only worker thread,
                 * we have to handle the events here.
                 */
		if (pjsua_var.thread[0] == NULL ||
                    (pj_thread_this() == pjsua_var.thread[0] &&
                     pjsua_var.ua_cfg.thread_cnt == 1))
                {
		    pjsua_handle_events(10);
                } else {
		    pj_thread_sleep(10);
                }

		pj_get_timestamp(&now);
		if (pj_elapsed_msec(&start, &now) > max_wait_ms)
		    return PJ_ETIMEDOUT;
	    }
	}
    }

    if (pjsua_var.stun_status != PJ_EPENDING &&
	pjsua_var.stun_status != PJ_SUCCESS &&
	pjsua_var.ua_cfg.stun_ignore_failure)
    {
	PJ_LOG(2,(THIS_FILE, 
		  "Ignoring STUN resolution failure (by setting)"));
	//pjsua_var.stun_status = PJ_SUCCESS;
	return PJ_SUCCESS;
    }

    return pjsua_var.stun_status;
}

/*
 * Destroy pjsua.
 */
PJ_DEF(pj_status_t) pjsua_destroy2(unsigned flags)
{
    int i;  /* Must be signed */

    if (pjsua_var.endpt) {
	PJ_LOG(4,(THIS_FILE, "Shutting down, flags=%d...", flags));
    }

    if (pjsua_var.state > PJSUA_STATE_NULL &&
	pjsua_var.state < PJSUA_STATE_CLOSING)
    {
	pjsua_set_state(PJSUA_STATE_CLOSING);
    }

    /* Signal threads to quit: */
    pjsua_stop_worker_threads();
    
    if (pjsua_var.endpt) {
	unsigned max_wait;

	pj_log_push_indent();

	/* Terminate all calls. */
	if ((flags & PJSUA_DESTROY_NO_TX_MSG) == 0) {
	    pjsua_call_hangup_all();
	}

	/* Deinit media channel of all calls (see #1717) */
	for (i=0; i<(int)pjsua_var.ua_cfg.max_calls; ++i) {
	    /* TODO: check if we're not allowed to send to network in the
	     *       "flags", and if so do not do TURN allocation...
	     */
	    pjsua_media_channel_deinit(i);
	}

	/* Set all accounts to offline */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	    if (!pjsua_var.acc[i].valid)
		continue;
	    pjsua_var.acc[i].online_status = PJ_FALSE;
	    pj_bzero(&pjsua_var.acc[i].rpid, sizeof(pjrpid_element));
	}

	/* Terminate all presence subscriptions. */
	pjsua_pres_shutdown(flags);

	/* Wait for sometime until all publish client sessions are done
	 * (ticket #364)
	 */
	/* First stage, get the maximum wait time */
	max_wait = 100;
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	    if (!pjsua_var.acc[i].valid)
		continue;
	    if (pjsua_var.acc[i].cfg.unpublish_max_wait_time_msec > max_wait)
		max_wait = pjsua_var.acc[i].cfg.unpublish_max_wait_time_msec;
	}
	
	/* No waiting if RX is disabled */
	if (flags & PJSUA_DESTROY_NO_RX_MSG) {
	    max_wait = 0;
	}

	/* Second stage, wait for unpublications to complete */
	for (i=0; i<(int)(max_wait/50); ++i) {
	    unsigned j;
	    for (j=0; j<PJ_ARRAY_SIZE(pjsua_var.acc); ++j) {
		if (!pjsua_var.acc[j].valid)
		    continue;

		if (pjsua_var.acc[j].publish_sess)
		    break;
	    }
	    if (j != PJ_ARRAY_SIZE(pjsua_var.acc))
		busy_sleep(50);
	    else
		break;
	}

	/* Third stage, forcefully destroy unfinished unpublications */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	    if (pjsua_var.acc[i].publish_sess) {
		pjsip_publishc_destroy(pjsua_var.acc[i].publish_sess);
		pjsua_var.acc[i].publish_sess = NULL;
	    }
	}

	/* Unregister all accounts */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	    if (!pjsua_var.acc[i].valid)
		continue;

	    if (pjsua_var.acc[i].regc && (flags & PJSUA_DESTROY_NO_TX_MSG)==0)
	    {
		pjsua_acc_set_registration(i, PJ_FALSE);
	    }
#if PJ_HAS_SSL_SOCK
	    pj_turn_sock_tls_cfg_wipe_keys(
			      &pjsua_var.acc[i].cfg.turn_cfg.turn_tls_setting);
#endif
	}

	/* Wait until all unregistrations are done (ticket #364) */
	/* First stage, get the maximum wait time */
	max_wait = 100;
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	    if (!pjsua_var.acc[i].valid)
		continue;
	    if (pjsua_var.acc[i].cfg.unreg_timeout > max_wait)
		max_wait = pjsua_var.acc[i].cfg.unreg_timeout;
	}
	
	/* No waiting if RX is disabled */
	if (flags & PJSUA_DESTROY_NO_RX_MSG) {
	    max_wait = 0;
	}

	/* Second stage, wait for unregistrations to complete */
	for (i=0; i<(int)(max_wait/50); ++i) {
	    unsigned j;
	    for (j=0; j<PJ_ARRAY_SIZE(pjsua_var.acc); ++j) {
		if (!pjsua_var.acc[j].valid)
		    continue;

		if (pjsua_var.acc[j].regc)
		    break;
	    }
	    if (j != PJ_ARRAY_SIZE(pjsua_var.acc))
		busy_sleep(50);
	    else
		break;
	}
	/* Note variable 'i' is used below */

	/* Wait for some time to allow unregistration and ICE/TURN
	 * transports shutdown to complete: 
	 */
	if (i < 20 && (flags & PJSUA_DESTROY_NO_RX_MSG) == 0) {
	    busy_sleep(1000 - i*50);
	}

	PJ_LOG(4,(THIS_FILE, "Destroying..."));
	
	/* Terminate any pending STUN resolution */
	if (!pj_list_empty(&pjsua_var.stun_res)) {
	    pjsua_stun_resolve *sess = pjsua_var.stun_res.next;
	    while (sess != &pjsua_var.stun_res) {
		pjsua_stun_resolve *next = sess->next;
		destroy_stun_resolve(sess, PJ_TRUE);
		sess = next;
	    }
	}

	/* Destroy media (to shutdown media endpoint, etc) */
	pjsua_media_subsys_destroy(flags);

	/* Must destroy endpoint first before destroying pools in
	 * buddies or accounts, since shutting down transaction layer
	 * may emit events which trigger some buddy or account callbacks
	 * to be called.
	 */
	pjsip_endpt_destroy(pjsua_var.endpt);
	pjsua_var.endpt = NULL;

	/* Destroy pool in the buddy object */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var.buddy); ++i) {
	    if (pjsua_var.buddy[i].pool) {
		pj_pool_release(pjsua_var.buddy[i].pool);
		pjsua_var.buddy[i].pool = NULL;
	    }
	}

	/* Destroy accounts */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	    if (pjsua_var.acc[i].pool) {
		pj_pool_release(pjsua_var.acc[i].pool);
		pjsua_var.acc[i].pool = NULL;
	    }
	}
    }

    /* Destroy mutex */
    if (pjsua_var.mutex) {
	pj_mutex_destroy(pjsua_var.mutex);
	pjsua_var.mutex = NULL;
    }
    
    if (pjsua_var.timer_mutex) {
        pj_mutex_destroy(pjsua_var.timer_mutex);
        pjsua_var.timer_mutex = NULL;
    }

    /* Destroy pools and pool factory. */
    if (pjsua_var.timer_pool) {
	pj_pool_release(pjsua_var.timer_pool);
	pjsua_var.timer_pool = NULL;
    }
    if (pjsua_var.pool) {
	pj_pool_release(pjsua_var.pool);
	pjsua_var.pool = NULL;
	pj_caching_pool_destroy(&pjsua_var.cp);

	pjsua_set_state(PJSUA_STATE_NULL);

	PJ_LOG(4,(THIS_FILE, "PJSUA destroyed..."));

	/* End logging */
	if (pjsua_var.log_file) {
	    pj_file_close(pjsua_var.log_file);
	    pjsua_var.log_file = NULL;
	}

	pj_log_pop_indent();

	/* Shutdown PJLIB */
	pj_shutdown();
    }

    /* Clear pjsua_var */
    pj_bzero(&pjsua_var, sizeof(pjsua_var));

    /* Done. */
    return PJ_SUCCESS;
}

void pjsua_set_state(pjsua_state new_state)
{
    const char *state_name[] = {
        "NULL",
        "CREATED",
        "INIT",
        "STARTING",
        "RUNNING",
        "CLOSING"
    };
    pjsua_state old_state = pjsua_var.state;

    pjsua_var.state = new_state;
    PJ_LOG(4,(THIS_FILE, "PJSUA state changed: %s --> %s",
	      state_name[old_state], state_name[new_state]));
}

/* Get state */
PJ_DEF(pjsua_state) pjsua_get_state(void)
{
    return pjsua_var.state;
}

PJ_DEF(pj_status_t) pjsua_destroy(void)
{
    return pjsua_destroy2(0);
}


/**
 * Application is recommended to call this function after all initialization
 * is done, so that the library can do additional checking set up
 * additional 
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DEF(pj_status_t) pjsua_start(void)
{
    pj_status_t status;

    pjsua_set_state(PJSUA_STATE_STARTING);
    pj_log_push_indent();

    status = pjsua_call_subsys_start();
    if (status != PJ_SUCCESS)
	goto on_return;

    status = pjsua_media_subsys_start();
    if (status != PJ_SUCCESS)
	goto on_return;

    status = pjsua_pres_start();
    if (status != PJ_SUCCESS)
	goto on_return;

    pjsua_set_state(PJSUA_STATE_RUNNING);

on_return:
    pj_log_pop_indent();
    return status;
}


/**
 * Poll pjsua for events, and if necessary block the caller thread for
 * the specified maximum interval (in miliseconds).
 */
PJ_DEF(int) pjsua_handle_events(unsigned msec_timeout)
{
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0

    return pj_symbianos_poll(-1, msec_timeout);

#else

    unsigned count = 0;
    pj_time_val tv;
    pj_status_t status;

    tv.sec = 0;
    tv.msec = msec_timeout;
    pj_time_val_normalize(&tv);

    status = pjsip_endpt_handle_events2(pjsua_var.endpt, &tv, &count);

    if (status != PJ_SUCCESS)
	return -status;

    return count;
    
#endif
}


/*
 * Create memory pool.
 */
PJ_DEF(pj_pool_t*) pjsua_pool_create( const char *name, pj_size_t init_size,
				      pj_size_t increment)
{
    /* Pool factory is thread safe, no need to lock */
    return pj_pool_create(&pjsua_var.cp.factory, name, init_size, increment, 
			  NULL);
}


/*
 * Internal function to get SIP endpoint instance of pjsua, which is
 * needed for example to register module, create transports, etc.
 * Probably is only valid after #pjsua_init() is called.
 */
PJ_DEF(pjsip_endpoint*) pjsua_get_pjsip_endpt(void)
{
    return pjsua_var.endpt;
}

/*
 * Internal function to get media endpoint instance.
 * Only valid after #pjsua_init() is called.
 */
PJ_DEF(pjmedia_endpt*) pjsua_get_pjmedia_endpt(void)
{
    return pjsua_var.med_endpt;
}

/*
 * Internal function to get PJSUA pool factory.
 */
PJ_DEF(pj_pool_factory*) pjsua_get_pool_factory(void)
{
    return &pjsua_var.cp.factory;
}

/*****************************************************************************
 * PJSUA SIP Transport API.
 */

/*
 * Tools to get address string.
 */
static const char *addr_string(const pj_sockaddr_t *addr)
{
    static char str[128];
    str[0] = '\0';
    pj_inet_ntop(((const pj_sockaddr*)addr)->addr.sa_family, 
		 pj_sockaddr_get_addr(addr),
		 str, sizeof(str));
    return str;
}

void pjsua_acc_on_tp_state_changed(pjsip_transport *tp,
				   pjsip_transport_state state,
				   const pjsip_transport_state_info *info);

/* Callback to receive transport state notifications */
static void on_tp_state_callback(pjsip_transport *tp,
				 pjsip_transport_state state,
				 const pjsip_transport_state_info *info)
{
    if (pjsua_var.ua_cfg.cb.on_transport_state) {
	(*pjsua_var.ua_cfg.cb.on_transport_state)(tp, state, info);
    }
    if (pjsua_var.old_tp_cb) {
	(*pjsua_var.old_tp_cb)(tp, state, info);
    }
    pjsua_acc_on_tp_state_changed(tp, state, info);
}

/* Set transport state callback */
static void set_tp_state_cb()
{
    pjsip_tp_state_callback tpcb;
    pjsip_tpmgr *tpmgr;

    tpmgr = pjsip_endpt_get_tpmgr(pjsua_var.endpt);
    tpcb = pjsip_tpmgr_get_state_cb(tpmgr);

    if (tpcb != &on_tp_state_callback) {
	pjsua_var.old_tp_cb = tpcb;
	pjsip_tpmgr_set_state_cb(tpmgr, &on_tp_state_callback);
    }
}

/*
 * Create and initialize SIP socket (and possibly resolve public
 * address via STUN, depending on config).
 */
static pj_status_t create_sip_udp_sock(int af,
				       const pjsua_transport_config *cfg,
				       pj_sock_t *p_sock,
				       pj_sockaddr *p_pub_addr)
{
    char stun_ip_addr[PJ_INET6_ADDRSTRLEN];
    unsigned port = cfg->port;
    pj_str_t stun_srv;
    pj_sock_t sock;
    pj_sockaddr bind_addr;
    pj_status_t status;

    /* Make sure STUN server resolution has completed */
    status = resolve_stun_server(PJ_TRUE, PJ_TRUE, 0);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	return status;
    }

    /* Initialize bound address */
    if (cfg->bound_addr.slen) {
	status = pj_sockaddr_init(af, &bind_addr, &cfg->bound_addr, 
				  (pj_uint16_t)port);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, 
			 "Unable to resolve transport bound address", 
			 status);
	    return status;
	}
    } else {
	pj_sockaddr_init(af, &bind_addr, NULL, (pj_uint16_t)port);
    }

    /* Create socket */
    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &sock);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "socket() error", status);
	return status;
    }

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(sock, cfg->qos_type, 
				&cfg->qos_params, 
				2, THIS_FILE, "SIP UDP socket");

    /* Apply sockopt, if specified */
    if (cfg->sockopt_params.cnt)
	status = pj_sock_setsockopt_params(sock, &cfg->sockopt_params);

    /* Bind socket */
    status = pj_sock_bind(sock, &bind_addr, pj_sockaddr_get_len(&bind_addr));
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "bind() error", status);
	pj_sock_close(sock);
	return status;
    }

    /* If port is zero, get the bound port */
    if (port == 0) {
	pj_sockaddr bound_addr;
	int namelen = sizeof(bound_addr);
	status = pj_sock_getsockname(sock, &bound_addr, &namelen);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "getsockname() error", status);
	    pj_sock_close(sock);
	    return status;
	}

	port = pj_sockaddr_get_port(&bound_addr);
    }

    if (pjsua_var.stun_srv.addr.sa_family != 0) {
    	pj_sockaddr_print(&pjsua_var.stun_srv,
    		     	  stun_ip_addr, sizeof(stun_ip_addr), 0);
	stun_srv = pj_str(stun_ip_addr);
    } else {
	stun_srv.slen = 0;
    }

    /* Get the published address, either by STUN or by resolving
     * the name of local host.
     */
    if (pj_sockaddr_has_addr(p_pub_addr)) {
	/*
	 * Public address is already specified, no need to resolve the 
	 * address, only set the port.
	 */
	if (pj_sockaddr_get_port(p_pub_addr) == 0)
	    pj_sockaddr_set_port(p_pub_addr, (pj_uint16_t)port);

    } else if (stun_srv.slen &&
               (af == pj_AF_INET() || pjsua_var.ua_cfg.stun_try_ipv6))
    {
	pjstun_setting stun_opt;

	/*
	 * STUN is specified, resolve the address with STUN.
	 * Currently, this is only to get IPv4 mapped address
	 * (does IPv6 still need a mapped address?).
	 */
	pj_bzero(&stun_opt, sizeof(stun_opt));
	stun_opt.use_stun2 = pjsua_var.ua_cfg.stun_map_use_stun2;
	stun_opt.af = pjsua_var.stun_srv.addr.sa_family;
	stun_opt.srv1  = stun_opt.srv2  = stun_srv;
	stun_opt.port1 = stun_opt.port2 = 
			 pj_sockaddr_get_port(&pjsua_var.stun_srv);
	status = pjstun_get_mapped_addr2(&pjsua_var.cp.factory, &stun_opt,
					 1, &sock, &p_pub_addr->ipv4);
	if (status != PJ_SUCCESS) {
	    /* Failed getting mapped address via STUN */
	    pjsua_perror(THIS_FILE, "Error contacting STUN server", status);
	    
	    /* Return error if configured to not ignore STUN failure */
	    if (!pjsua_var.ua_cfg.stun_ignore_failure) {
		pj_sock_close(sock);
		return status;
	    }

	    /* Otherwise, just use host IP */
	    pj_sockaddr_init(af, p_pub_addr, NULL, (pj_uint16_t)port);
	    status = pj_gethostip(af, p_pub_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to get local host IP", status);
		pj_sock_close(sock);
		return status;
	    }
	}

    } else {

	pj_bzero(p_pub_addr, sizeof(pj_sockaddr));

	if (pj_sockaddr_has_addr(&bind_addr)) {
	    pj_sockaddr_copy_addr(p_pub_addr, &bind_addr);
	} else {
	    status = pj_gethostip(af, p_pub_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to get local host IP", status);
		pj_sock_close(sock);
		return status;
	    }
	}

	p_pub_addr->addr.sa_family = (pj_uint16_t)af;
	pj_sockaddr_set_port(p_pub_addr, (pj_uint16_t)port);

	if (stun_srv.slen && af != pj_AF_INET()) {
	    /* STUN is specified, but it is not IPv4, just print warning */
	    PJ_PERROR(2, (THIS_FILE, PJ_EAFNOTSUP,
		          "Cannot use STUN for SIP UDP socket %s:%d",
		          addr_string(p_pub_addr),
		          (int)pj_sockaddr_get_port(p_pub_addr)));
	}

    }

    *p_sock = sock;

    PJ_LOG(4,(THIS_FILE, "SIP UDP socket reachable at %s:%d",
	      addr_string(p_pub_addr),
	      (int)pj_sockaddr_get_port(p_pub_addr)));

    return PJ_SUCCESS;
}


/*
 * Create SIP transport.
 */
PJ_DEF(pj_status_t) pjsua_transport_create( pjsip_transport_type_e type,
					    const pjsua_transport_config *cfg,
					    pjsua_transport_id *p_id)
{
    pjsip_transport *tp;
    unsigned id;
    pj_status_t status;

    PJSUA_LOCK();

    /* Find empty transport slot */
    for (id=0; id < PJ_ARRAY_SIZE(pjsua_var.tpdata); ++id) {
	if (pjsua_var.tpdata[id].data.ptr == NULL)
	    break;
    }

    if (id == PJ_ARRAY_SIZE(pjsua_var.tpdata)) {
	status = PJ_ETOOMANY;
	pjsua_perror(THIS_FILE, "Error creating transport", status);
	goto on_return;
    }

    /* Create the transport */
    if (type==PJSIP_TRANSPORT_UDP || type==PJSIP_TRANSPORT_UDP6) {
	/*
	 * Create UDP transport (IPv4 or IPv6).
	 */
	pjsua_transport_config config;
	char hostbuf[PJ_INET6_ADDRSTRLEN];
	pj_sock_t sock = PJ_INVALID_SOCKET;
	pj_sockaddr pub_addr;
	pjsip_host_port addr_name;

	/* Supply default config if it's not specified */
	if (cfg == NULL) {
	    pjsua_transport_config_default(&config);
	    cfg = &config;
	}

	/* Initialize the public address from the config, if any */
	pj_sockaddr_init(pjsip_transport_type_get_af(type), &pub_addr, 
			 NULL, (pj_uint16_t)cfg->port);
	if (cfg->public_addr.slen) {
	    status = pj_sockaddr_set_str_addr(pjsip_transport_type_get_af(type),
					      &pub_addr, &cfg->public_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, 
			     "Unable to resolve transport public address", 
			     status);
		goto on_return;
	    }
	}

	/* Create the socket and possibly resolve the address with STUN 
	 * (only when public address is not specified).
	 */
	status = create_sip_udp_sock(pjsip_transport_type_get_af(type),
				     cfg, &sock, &pub_addr);
	if (status != PJ_SUCCESS)
	    goto on_return;

	pj_ansi_strcpy(hostbuf, addr_string(&pub_addr));
	addr_name.host = pj_str(hostbuf);
	addr_name.port = pj_sockaddr_get_port(&pub_addr);

	/* Create UDP transport */
	status = pjsip_udp_transport_attach2(pjsua_var.endpt, type, sock,
					     &addr_name, 1, &tp);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating SIP UDP transport", 
			 status);
	    pj_sock_close(sock);
	    goto on_return;
	}


	/* Save the transport */
	pjsua_var.tpdata[id].type = type;
	pjsua_var.tpdata[id].local_name = tp->local_name;
	pjsua_var.tpdata[id].data.tp = tp;
	if (cfg->bound_addr.slen)
	    pjsua_var.tpdata[id].has_bound_addr = PJ_TRUE;

#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0

    } else if (type == PJSIP_TRANSPORT_TCP || type == PJSIP_TRANSPORT_TCP6) {
	/*
	 * Create TCP transport.
	 */
	pjsua_transport_config config;
	pjsip_tpfactory *tcp;
	pjsip_tcp_transport_cfg tcp_cfg;
	int af;

	af = (type==PJSIP_TRANSPORT_TCP6) ? pj_AF_INET6() : pj_AF_INET();
	pjsip_tcp_transport_cfg_default(&tcp_cfg, af);

	/* Supply default config if it's not specified */
	if (cfg == NULL) {
	    pjsua_transport_config_default(&config);
	    cfg = &config;
	}

	/* Configure bind address */
	if (cfg->port)
	    pj_sockaddr_set_port(&tcp_cfg.bind_addr, (pj_uint16_t)cfg->port);

	if (cfg->bound_addr.slen) {
	    status = pj_sockaddr_set_str_addr(tcp_cfg.af, 
					      &tcp_cfg.bind_addr,
					      &cfg->bound_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, 
			     "Unable to resolve transport bound address", 
			     status);
		goto on_return;
	    }
	}

	/* Set published name */
	if (cfg->public_addr.slen)
	    tcp_cfg.addr_name.host = cfg->public_addr;

	/* Copy the QoS settings */
	tcp_cfg.qos_type = cfg->qos_type;
	pj_memcpy(&tcp_cfg.qos_params, &cfg->qos_params, 
		  sizeof(cfg->qos_params));

	/* Copy the sockopt */
	pj_memcpy(&tcp_cfg.sockopt_params, &cfg->sockopt_params,
		  sizeof(tcp_cfg.sockopt_params));

	/* Create the TCP transport */
	status = pjsip_tcp_transport_start3(pjsua_var.endpt, &tcp_cfg, &tcp);

	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating SIP TCP listener", 
			 status);
	    goto on_return;
	}

	/* Save the transport */
	pjsua_var.tpdata[id].type = type;
	pjsua_var.tpdata[id].local_name = tcp->addr_name;
	pjsua_var.tpdata[id].data.factory = tcp;

#endif	/* PJ_HAS_TCP */

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
    } else if (type == PJSIP_TRANSPORT_TLS || type == PJSIP_TRANSPORT_TLS6) {
	/*
	 * Create TLS transport.
	 */
	pjsua_transport_config config;
	pjsip_host_port a_name;
	pjsip_tpfactory *tls;
	pj_sockaddr local_addr;
	int af;

	/* Supply default config if it's not specified */
	if (cfg == NULL) {
	    pjsua_transport_config_default(&config);
	    config.port = 5061;
	    cfg = &config;
	}

	/* Init local address */
	af = (type==PJSIP_TRANSPORT_TLS) ? pj_AF_INET() : pj_AF_INET6();
	pj_sockaddr_init(af, &local_addr, NULL, 0);

	if (cfg->port)
	    pj_sockaddr_set_port(&local_addr, (pj_uint16_t)cfg->port);

	if (cfg->bound_addr.slen) {
	    status = pj_sockaddr_set_str_addr(af, &local_addr,
	                                      &cfg->bound_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, 
			     "Unable to resolve transport bound address", 
			     status);
		goto on_return;
	    }
	}

	/* Init published name */
	pj_bzero(&a_name, sizeof(pjsip_host_port));
	if (cfg->public_addr.slen)
	    a_name.host = cfg->public_addr;

	status = pjsip_tls_transport_start2(pjsua_var.endpt, &cfg->tls_setting,
					    &local_addr, &a_name, 1, &tls);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating SIP TLS listener", 
			 status);
	    goto on_return;
	}

	/* Save the transport */
	pjsua_var.tpdata[id].type = type;
	pjsua_var.tpdata[id].local_name = tls->addr_name;
	pjsua_var.tpdata[id].data.factory = tls;
#endif

    } else {
	status = PJSIP_EUNSUPTRANSPORT;
	pjsua_perror(THIS_FILE, "Error creating transport", status);
	goto on_return;
    }

    /* Set transport state callback */
    set_tp_state_cb();

    /* Return the ID */
    if (p_id) *p_id = id;

    status = PJ_SUCCESS;

on_return:

    PJSUA_UNLOCK();

    return status;
}


/*
 * Register transport that has been created by application.
 */
PJ_DEF(pj_status_t) pjsua_transport_register( pjsip_transport *tp,
					      pjsua_transport_id *p_id)
{
    unsigned id;

    PJSUA_LOCK();

    /* Find empty transport slot */
    for (id=0; id < PJ_ARRAY_SIZE(pjsua_var.tpdata); ++id) {
	if (pjsua_var.tpdata[id].data.ptr == NULL)
	    break;
    }

    if (id == PJ_ARRAY_SIZE(pjsua_var.tpdata)) {
	pjsua_perror(THIS_FILE, "Error creating transport", PJ_ETOOMANY);
	PJSUA_UNLOCK();
	return PJ_ETOOMANY;
    }

    /* Save the transport */
    pjsua_var.tpdata[id].type = (pjsip_transport_type_e) tp->key.type;
    pjsua_var.tpdata[id].local_name = tp->local_name;
    pjsua_var.tpdata[id].data.tp = tp;

    /* Set transport state callback */
    set_tp_state_cb();

    /* Return the ID */
    if (p_id) *p_id = id;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Register transport factory that has been created by application.
 */
PJ_DEF(pj_status_t) pjsua_tpfactory_register( pjsip_tpfactory *tf,
					      pjsua_transport_id *p_id)
{
    unsigned id;

    PJSUA_LOCK();

    /* Find empty transport slot */
    for (id=0; id < PJ_ARRAY_SIZE(pjsua_var.tpdata); ++id) {
	if (pjsua_var.tpdata[id].data.ptr == NULL)
	    break;
    }

    if (id == PJ_ARRAY_SIZE(pjsua_var.tpdata)) {
	pjsua_perror(THIS_FILE, "Error creating transport", PJ_ETOOMANY);
	PJSUA_UNLOCK();
	return PJ_ETOOMANY;
    }

    /* Save the transport */
    pjsua_var.tpdata[id].type = (pjsip_transport_type_e) tf->type;
    pjsua_var.tpdata[id].local_name = tf->addr_name;
    pjsua_var.tpdata[id].data.factory = tf;

    /* Set transport state callback */
    set_tp_state_cb();

    /* Return the ID */
    if (p_id) *p_id = id;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Enumerate all transports currently created in the system.
 */
PJ_DEF(pj_status_t) pjsua_enum_transports( pjsua_transport_id id[],
					   unsigned *p_count )
{
    unsigned i, count;

    PJSUA_LOCK();

    for (i=0, count=0; i<PJ_ARRAY_SIZE(pjsua_var.tpdata) && count<*p_count; 
	 ++i) 
    {
	if (!pjsua_var.tpdata[i].data.ptr)
	    continue;

	id[count++] = i;
    }

    *p_count = count;

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*
 * Get information about transports.
 */
PJ_DEF(pj_status_t) pjsua_transport_get_info( pjsua_transport_id id,
					      pjsua_transport_info *info)
{
    pjsua_transport_data *t = &pjsua_var.tpdata[id];
    pj_status_t status;

    pj_bzero(info, sizeof(*info));

    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.tpdata), 
		     PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var.tpdata[id].data.ptr != NULL, PJ_EINVAL);

    PJSUA_LOCK();

    if ((t->type & ~PJSIP_TRANSPORT_IPV6) == PJSIP_TRANSPORT_UDP) {

	pjsip_transport *tp = t->data.tp;

	if (tp == NULL) {
	    PJSUA_UNLOCK();
	    return PJ_EINVALIDOP;
	}
    
	info->id = id;
	info->type = (pjsip_transport_type_e) tp->key.type;
	info->type_name = pj_str(tp->type_name);
	info->info = pj_str(tp->info);
	info->flag = tp->flag;
	info->addr_len = tp->addr_len;
	info->local_addr = tp->local_addr;
	info->local_name = tp->local_name;
	info->usage_count = pj_atomic_get(tp->ref_cnt);

	status = PJ_SUCCESS;

    } else if ((t->type & ~PJSIP_TRANSPORT_IPV6) == PJSIP_TRANSPORT_TCP ||
	       (t->type & ~PJSIP_TRANSPORT_IPV6) == PJSIP_TRANSPORT_TLS)
    {

	pjsip_tpfactory *factory = t->data.factory;

	if (factory == NULL) {
	    PJSUA_UNLOCK();
	    return PJ_EINVALIDOP;
	}
    
	info->id = id;
	info->type = t->type;
	info->type_name = pj_str(factory->type_name);
	info->info = pj_str(factory->info);
	info->flag = factory->flag;
	info->addr_len = sizeof(factory->local_addr);
	info->local_addr = factory->local_addr;
	info->local_name = factory->addr_name;
	info->usage_count = 0;

	status = PJ_SUCCESS;

    } else {
	pj_assert(!"Unsupported transport");
	status = PJ_EINVALIDOP;
    }


    PJSUA_UNLOCK();

    return status;
}


/*
 * Disable a transport or re-enable it.
 */
PJ_DEF(pj_status_t) pjsua_transport_set_enable( pjsua_transport_id id,
						pj_bool_t enabled)
{
    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.tpdata), 
		     PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var.tpdata[id].data.ptr != NULL, PJ_EINVAL);


    /* To be done!! */
    PJ_TODO(pjsua_transport_set_enable);
    PJ_UNUSED_ARG(enabled);

    return PJ_EINVALIDOP;
}


/*
 * Close the transport.
 */
PJ_DEF(pj_status_t) pjsua_transport_close( pjsua_transport_id id,
					   pj_bool_t force )
{
    pj_status_t status;
    pjsip_transport_type_e tp_type;

    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.tpdata), 
		     PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var.tpdata[id].data.ptr != NULL, PJ_EINVAL);

    tp_type = pjsua_var.tpdata[id].type & ~PJSIP_TRANSPORT_IPV6;

    /* Note: destroy() may not work if there are objects still referencing
     *	     the transport.
     */
    if (force) {
	switch (tp_type) {
	case PJSIP_TRANSPORT_UDP:
	    status = pjsip_transport_shutdown(pjsua_var.tpdata[id].data.tp);
	    if (status  != PJ_SUCCESS)
		return status;
	    status = pjsip_transport_destroy(pjsua_var.tpdata[id].data.tp);
	    if (status != PJ_SUCCESS)
		return status;
	    break;

	case PJSIP_TRANSPORT_TLS:
	case PJSIP_TRANSPORT_TCP:
	    /* This will close the TCP listener, but existing TCP/TLS
	     * connections (if any) will still linger 
	     */
	    status = (*pjsua_var.tpdata[id].data.factory->destroy)
			(pjsua_var.tpdata[id].data.factory);
	    if (status != PJ_SUCCESS)
		return status;

	    break;

	default:
	    return PJ_EINVAL;
	}
	
    } else {
	/* If force is not specified, transports will be closed at their
	 * convenient time. However this will leak PJSUA-API transport
	 * descriptors as PJSUA-API wouldn't know when exactly the
	 * transport is closed thus it can't cleanup PJSUA transport
	 * descriptor.
	 */
	switch (tp_type) {
	case PJSIP_TRANSPORT_UDP:
	    return pjsip_transport_shutdown(pjsua_var.tpdata[id].data.tp);
	case PJSIP_TRANSPORT_TLS:
	case PJSIP_TRANSPORT_TCP:
	    return (*pjsua_var.tpdata[id].data.factory->destroy)
			(pjsua_var.tpdata[id].data.factory);
	default:
	    return PJ_EINVAL;
	}
    }

    /* Cleanup pjsua data when force is applied */
    if (force) {
	pjsua_var.tpdata[id].type = PJSIP_TRANSPORT_UNSPECIFIED;
	pjsua_var.tpdata[id].data.ptr = NULL;
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_transport_lis_start(pjsua_transport_id id,
					     const pjsua_transport_config *cfg)
{
    pj_status_t status = PJ_SUCCESS;
    pjsip_transport_type_e tp_type;

    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.tpdata), 
		     PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var.tpdata[id].data.ptr != NULL, PJ_EINVAL);

    tp_type = pjsua_var.tpdata[id].type & ~PJSIP_TRANSPORT_IPV6;
 
    if ((tp_type == PJSIP_TRANSPORT_TLS) || (tp_type == PJSIP_TRANSPORT_TCP)) {
	pj_sockaddr bind_addr;
	pjsip_host_port addr_name;
	pjsip_tpfactory *factory = pjsua_var.tpdata[id].data.factory;
	
        int af = pjsip_transport_type_get_af(factory->type);

	if (cfg->port)
	    pj_sockaddr_set_port(&bind_addr, (pj_uint16_t)cfg->port);

	if (cfg->bound_addr.slen) {
	    status = pj_sockaddr_set_str_addr(af, 
					      &bind_addr,
					      &cfg->bound_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, 
			     "Unable to resolve transport bound address", 
			     status);
		return status;
	    }
	}

	/* Set published name */
	if (cfg->public_addr.slen)
	    addr_name.host = cfg->public_addr;

	if (tp_type == PJSIP_TRANSPORT_TCP) {
	    status = pjsip_tcp_transport_lis_start(factory, &bind_addr,
						   &addr_name);
	}
#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
	else {
	    status = pjsip_tls_transport_lis_start(factory, &bind_addr,
						   &addr_name);	
	}
#endif	
    } else if (tp_type == PJSIP_TRANSPORT_UDP) {
	status = PJ_SUCCESS;
    } else {
	status = PJ_EINVAL;
    }
    return status;
}


/*
 * Add additional headers etc in msg_data specified by application
 * when sending requests.
 */
void pjsua_process_msg_data(pjsip_tx_data *tdata,
			    const pjsua_msg_data *msg_data)
{
    pj_bool_t allow_body;
    const pjsip_hdr *hdr;

    /* Always add User-Agent */
    if (pjsua_var.ua_cfg.user_agent.slen && 
	tdata->msg->type == PJSIP_REQUEST_MSG) 
    {
	const pj_str_t STR_USER_AGENT = { "User-Agent", 10 };
	pjsip_hdr *h;
	h = (pjsip_hdr*)pjsip_generic_string_hdr_create(tdata->pool, 
							&STR_USER_AGENT, 
							&pjsua_var.ua_cfg.user_agent);
	pjsip_msg_add_hdr(tdata->msg, h);
    }

    if (!msg_data)
	return;

    hdr = msg_data->hdr_list.next;
    while (hdr && hdr != &msg_data->hdr_list) {
	pjsip_hdr *new_hdr;

	new_hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, hdr);
	pjsip_msg_add_hdr(tdata->msg, new_hdr);

	hdr = hdr->next;
    }

    allow_body = (tdata->msg->body == NULL);

    if (allow_body && msg_data->content_type.slen && msg_data->msg_body.slen) {
	pjsip_media_type ctype;
	pjsip_msg_body *body;	

	pjsua_parse_media_type(tdata->pool, &msg_data->content_type, &ctype);
	body = pjsip_msg_body_create(tdata->pool, &ctype.type, &ctype.subtype,
				     &msg_data->msg_body);
	tdata->msg->body = body;
    }

    /* Multipart */
    if (!pj_list_empty(&msg_data->multipart_parts) &&
	msg_data->multipart_ctype.type.slen)
    {
	pjsip_msg_body *bodies;
	pjsip_multipart_part *part;
	pj_str_t *boundary = NULL;

	bodies = pjsip_multipart_create(tdata->pool,
				        &msg_data->multipart_ctype,
				        boundary);
	part = msg_data->multipart_parts.next;
	while (part != &msg_data->multipart_parts) {
	    pjsip_multipart_part *part_copy;

	    part_copy = pjsip_multipart_clone_part(tdata->pool, part);
	    pjsip_multipart_add_part(tdata->pool, bodies, part_copy);
	    part = part->next;
	}

	if (tdata->msg->body) {
	    part = pjsip_multipart_create_part(tdata->pool);
	    part->body = tdata->msg->body;
	    pjsip_multipart_add_part(tdata->pool, bodies, part);

	    tdata->msg->body = NULL;
	}

	tdata->msg->body = bodies;
    }
}


/*
 * Add route_set to outgoing requests
 */
void pjsua_set_msg_route_set( pjsip_tx_data *tdata,
			      const pjsip_route_hdr *route_set )
{
    const pjsip_route_hdr *r;

    r = route_set->next;
    while (r != route_set) {
	pjsip_route_hdr *new_r;

	new_r = (pjsip_route_hdr*) pjsip_hdr_clone(tdata->pool, r);
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)new_r);

	r = r->next;
    }
}


/*
 * Simple version of MIME type parsing (it doesn't support parameters)
 */
void pjsua_parse_media_type( pj_pool_t *pool,
			     const pj_str_t *mime,
			     pjsip_media_type *media_type)
{
    pj_str_t tmp;
    char *pos;

    pj_bzero(media_type, sizeof(*media_type));

    pj_strdup_with_null(pool, &tmp, mime);

    pos = pj_strchr(&tmp, '/');
    if (pos) {
	media_type->type.ptr = tmp.ptr; 
	media_type->type.slen = (pos-tmp.ptr);
	media_type->subtype.ptr = pos+1; 
	media_type->subtype.slen = tmp.ptr+tmp.slen-pos-1;
    } else {
	media_type->type = tmp;
    }
}


/*
 * Internal function to init transport selector from transport id.
 */
void pjsua_init_tpselector(pjsua_transport_id tp_id,
			   pjsip_tpselector *sel)
{
    pjsua_transport_data *tpdata;
    unsigned flag;

    pj_bzero(sel, sizeof(*sel));
    if (tp_id == PJSUA_INVALID_ID)
	return;

    pj_assert(tp_id >= 0 && tp_id < (int)PJ_ARRAY_SIZE(pjsua_var.tpdata));
    tpdata = &pjsua_var.tpdata[tp_id];

    flag = pjsip_transport_get_flag_from_type(tpdata->type);

    if (flag & PJSIP_TRANSPORT_DATAGRAM) {
	sel->type = PJSIP_TPSELECTOR_TRANSPORT;
	sel->u.transport = tpdata->data.tp;
    } else {
	sel->type = PJSIP_TPSELECTOR_LISTENER;
	sel->u.listener = tpdata->data.factory;
    }
}


PJ_DEF(void) pjsua_ip_change_param_default(pjsua_ip_change_param *param)
{
    pj_bzero(param, sizeof(*param));
    param->restart_listener = PJ_TRUE;
    param->restart_lis_delay = PJSUA_TRANSPORT_RESTART_DELAY_TIME;
}


/* Callback upon NAT detection completion */
static void nat_detect_cb(void *user_data, 
			  const pj_stun_nat_detect_result *res)
{
    PJ_UNUSED_ARG(user_data);

    pjsua_var.nat_in_progress = PJ_FALSE;
    pjsua_var.nat_status = res->status;
    pjsua_var.nat_type = res->nat_type;

    if (pjsua_var.ua_cfg.cb.on_nat_detect) {
	(*pjsua_var.ua_cfg.cb.on_nat_detect)(res);
    }
}


/*
 * Detect NAT type.
 */
PJ_DEF(pj_status_t) pjsua_detect_nat_type()
{
    pj_status_t status;

    if (pjsua_var.nat_in_progress)
	return PJ_SUCCESS;

    /* Make sure STUN server resolution has completed */
    status = resolve_stun_server(PJ_TRUE, PJ_TRUE, 0);
    if (status != PJ_SUCCESS) {
	pjsua_var.nat_status = status;
	pjsua_var.nat_type = PJ_STUN_NAT_TYPE_ERR_UNKNOWN;
	return status;
    }

    /* Make sure we have STUN */
    if (pjsua_var.stun_srv.addr.sa_family == 0) {
	pjsua_var.nat_status = PJNATH_ESTUNINSERVER;
	return PJNATH_ESTUNINSERVER;
    }

    status = pj_stun_detect_nat_type2(&pjsua_var.stun_srv, 
				      &pjsua_var.stun_cfg, 
				      NULL, &nat_detect_cb);

    if (status != PJ_SUCCESS) {
	pjsua_var.nat_status = status;
	pjsua_var.nat_type = PJ_STUN_NAT_TYPE_ERR_UNKNOWN;
	return status;
    }

    pjsua_var.nat_in_progress = PJ_TRUE;

    return PJ_SUCCESS;
}


/*
 * Get NAT type.
 */
PJ_DEF(pj_status_t) pjsua_get_nat_type(pj_stun_nat_type *type)
{
    *type = pjsua_var.nat_type;
    return pjsua_var.nat_status;
}

/*
 * Verify that valid url is given.
 */
PJ_DEF(pj_status_t) pjsua_verify_url(const char *c_url)
{
    pjsip_uri *p;
    pj_pool_t *pool;
    char *url;
    pj_size_t len = (c_url ? pj_ansi_strlen(c_url) : 0);

    if (!len) return PJSIP_EINVALIDURI;

    pool = pj_pool_create(&pjsua_var.cp.factory, "check%p", 1024, 0, NULL);
    if (!pool) return PJ_ENOMEM;

    url = (char*) pj_pool_alloc(pool, len+1);
    pj_ansi_strcpy(url, c_url);

    p = pjsip_parse_uri(pool, url, len, 0);

    pj_pool_release(pool);
    return p ? 0 : PJSIP_EINVALIDURI;
}

/*
 * Verify that valid SIP url is given.
 */
PJ_DEF(pj_status_t) pjsua_verify_sip_url(const char *c_url)
{
    pjsip_uri *p;
    pj_pool_t *pool;
    char *url;
    pj_size_t len = (c_url ? pj_ansi_strlen(c_url) : 0);

    if (!len) return PJSIP_EINVALIDURI;

    pool = pj_pool_create(&pjsua_var.cp.factory, "check%p", 1024, 0, NULL);
    if (!pool) return PJ_ENOMEM;

    url = (char*) pj_pool_alloc(pool, len+1);
    pj_ansi_strcpy(url, c_url);

    p = pjsip_parse_uri(pool, url, len, 0);
    if (!p || (pj_stricmp2(pjsip_uri_get_scheme(p), "sip") != 0 &&
	       pj_stricmp2(pjsip_uri_get_scheme(p), "sips") != 0))
    {
	p = NULL;
    }

    pj_pool_release(pool);
    return p ? 0 : PJSIP_EINVALIDURI;
}

/*
 * Schedule a timer entry. 
 */
#if PJ_TIMER_DEBUG
PJ_DEF(pj_status_t) pjsua_schedule_timer_dbg( pj_timer_entry *entry,
                                              const pj_time_val *delay,
                                              const char *src_file,
                                              int src_line)
{
    return pjsip_endpt_schedule_timer_dbg(pjsua_var.endpt, entry, delay,
                                          src_file, src_line);
}
#else
PJ_DEF(pj_status_t) pjsua_schedule_timer( pj_timer_entry *entry,
					  const pj_time_val *delay)
{
    return pjsip_endpt_schedule_timer(pjsua_var.endpt, entry, delay);
}
#endif

/* Timer callback */
static void timer_cb( pj_timer_heap_t *th,
		      pj_timer_entry *entry)
{
    pjsua_timer_list *tmr = (pjsua_timer_list *)entry->user_data;
    void (*cb)(void *user_data) = tmr->cb;
    void *user_data = tmr->user_data;

    PJ_UNUSED_ARG(th);

    pj_mutex_lock(pjsua_var.timer_mutex);
    pj_list_push_back(&pjsua_var.timer_list, tmr);
    pj_mutex_unlock(pjsua_var.timer_mutex);

    if (cb)
        (*cb)(user_data);
}

/*
 * Schedule a timer callback. 
 */
#if PJ_TIMER_DEBUG
PJ_DEF(pj_status_t) pjsua_schedule_timer2_dbg( void (*cb)(void *user_data),
                                               void *user_data,
                                               unsigned msec_delay,
                                               const char *src_file,
                                               int src_line)
#else
PJ_DEF(pj_status_t) pjsua_schedule_timer2( void (*cb)(void *user_data),
                                           void *user_data,
                                           unsigned msec_delay)
#endif
{
    pjsua_timer_list *tmr = NULL;
    pj_status_t status;
    pj_time_val delay;

    pj_mutex_lock(pjsua_var.timer_mutex);

    if (pj_list_empty(&pjsua_var.timer_list)) {
        tmr = PJ_POOL_ALLOC_T(pjsua_var.timer_pool, pjsua_timer_list);
    } else {
        tmr = pjsua_var.timer_list.next;
        pj_list_erase(tmr);
    }
    pj_timer_entry_init(&tmr->entry, 0, tmr, timer_cb);
    tmr->cb = cb;
    tmr->user_data = user_data;
    delay.sec = 0;
    delay.msec = msec_delay;

#if PJ_TIMER_DEBUG
    status = pjsip_endpt_schedule_timer_dbg(pjsua_var.endpt, &tmr->entry,
                                            &delay, src_file, src_line);
#else
    status = pjsip_endpt_schedule_timer(pjsua_var.endpt, &tmr->entry, &delay);
#endif
    if (status != PJ_SUCCESS) {
        pj_list_push_back(&pjsua_var.timer_list, tmr);
    }

    pj_mutex_unlock(pjsua_var.timer_mutex);

    return status;
}

/*
 * Cancel the previously scheduled timer.
 *
 */
PJ_DEF(void) pjsua_cancel_timer(pj_timer_entry *entry)
{
    pjsip_endpt_cancel_timer(pjsua_var.endpt, entry);
}

/** 
 * Normalize route URI (check for ";lr" and append one if it doesn't
 * exist and pjsua_config.force_lr is set.
 */
pj_status_t normalize_route_uri(pj_pool_t *pool, pj_str_t *uri)
{
    pj_str_t tmp_uri;
    pj_pool_t *tmp_pool;
    pjsip_uri *uri_obj;
    pjsip_sip_uri *sip_uri;

    tmp_pool = pjsua_pool_create("tmplr%p", 512, 512);
    if (!tmp_pool)
	return PJ_ENOMEM;

    pj_strdup_with_null(tmp_pool, &tmp_uri, uri);

    uri_obj = pjsip_parse_uri(tmp_pool, tmp_uri.ptr, tmp_uri.slen, 0);
    if (!uri_obj) {
	PJ_LOG(1,(THIS_FILE, "Invalid route URI: %.*s", 
		  (int)uri->slen, uri->ptr));
	pj_pool_release(tmp_pool);
	return PJSIP_EINVALIDURI;
    }

    if (!PJSIP_URI_SCHEME_IS_SIP(uri_obj) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri_obj))
    {
	PJ_LOG(1,(THIS_FILE, "Route URI must be SIP URI: %.*s", 
		  (int)uri->slen, uri->ptr));
	pj_pool_release(tmp_pool);
	return PJSIP_EINVALIDSCHEME;
    }

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri_obj);

    /* Done if force_lr is disabled or if lr parameter is present */
    if (!pjsua_var.ua_cfg.force_lr || sip_uri->lr_param) {
	pj_pool_release(tmp_pool);
	return PJ_SUCCESS;
    }

    /* Set lr param */
    sip_uri->lr_param = 1;

    /* Print the URI */
    tmp_uri.ptr = (char*) pj_pool_alloc(tmp_pool, PJSIP_MAX_URL_SIZE);
    tmp_uri.slen = pjsip_uri_print(PJSIP_URI_IN_ROUTING_HDR, uri_obj, 
				   tmp_uri.ptr, PJSIP_MAX_URL_SIZE);
    if (tmp_uri.slen < 1) {
	PJ_LOG(1,(THIS_FILE, "Route URI is too long: %.*s", 
		  (int)uri->slen, uri->ptr));
	pj_pool_release(tmp_pool);
	return PJSIP_EURITOOLONG;
    }

    /* Clone the URI */
    pj_strdup_with_null(pool, uri, &tmp_uri);

    pj_pool_release(tmp_pool);
    return PJ_SUCCESS;
}

/*
 * This is a utility function to dump the stack states to log, using
 * verbosity level 3.
 */
PJ_DEF(void) pjsua_dump(pj_bool_t detail)
{
    unsigned old_decor;
    unsigned i;

    PJ_LOG(3,(THIS_FILE, "Start dumping application states:"));

    old_decor = pj_log_get_decor();
    pj_log_set_decor(old_decor & (PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));

    if (detail)
	pj_dump_config();

    pjsip_endpt_dump(pjsua_get_pjsip_endpt(), detail);

    pjmedia_endpt_dump(pjsua_get_pjmedia_endpt());

    PJ_LOG(3,(THIS_FILE, "Dumping media transports:"));
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	pjsua_acc_config *acc_cfg;
	pjmedia_transport *tp[PJSUA_MAX_CALL_MEDIA*2];
	unsigned tp_cnt = 0;
	unsigned j;

	/* Collect media transports in this call */
	for (j = 0; j < call->med_cnt; ++j) {
	    if (call->media[j].tp != NULL)
		tp[tp_cnt++] = call->media[j].tp;
	}
	for (j = 0; j < call->med_prov_cnt; ++j) {
	    pjmedia_transport *med_tp = call->media_prov[j].tp;
	    if (med_tp) {
		unsigned k;
		pj_bool_t used = PJ_FALSE;
		for (k = 0; k < tp_cnt; ++k) {
		    if (med_tp == tp[k]) {
			used = PJ_TRUE;
			break;
		    }
		}
		if (!used)
		    tp[tp_cnt++] = med_tp;
	    }
	}

	acc_cfg = &pjsua_var.acc[call->acc_id].cfg;

	/* Dump the media transports in this call */
	for (j = 0; j < tp_cnt; ++j) {
	    pjmedia_transport_info tpinfo;
	    char addr_buf[80];

	    pjmedia_transport_info_init(&tpinfo);
	    pjmedia_transport_get_info(tp[j], &tpinfo);
	    PJ_LOG(3,(THIS_FILE, " %s: %s",
		      (acc_cfg->ice_cfg.enable_ice ? "ICE" : "UDP"),
		      pj_sockaddr_print(&tpinfo.sock_info.rtp_addr_name,
					addr_buf,
					sizeof(addr_buf), 3)));
	}
    }

    pjsip_tsx_layer_dump(detail);
    pjsip_ua_dump(detail);

// Dumping complete call states may require a 'large' buffer 
// (about 3KB per call session, including RTCP XR).
#if 0
    /* Dump all invite sessions: */
    PJ_LOG(3,(THIS_FILE, "Dumping invite sessions:"));

    if (pjsua_call_get_count() == 0) {

	PJ_LOG(3,(THIS_FILE, "  - no sessions -"));

    } else {
	unsigned i;

	for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	    if (pjsua_call_is_active(i)) {
		/* Tricky logging, since call states log string tends to be 
		 * longer than PJ_LOG_MAX_SIZE.
		 */
		char buf[1024 * 3];
		unsigned call_dump_len;
		unsigned part_len;
		unsigned part_idx;
		unsigned log_decor;

		pjsua_call_dump(i, detail, buf, sizeof(buf), "  ");
		call_dump_len = strlen(buf);

		log_decor = pj_log_get_decor();
		pj_log_set_decor(log_decor & ~(PJ_LOG_HAS_NEWLINE | 
					       PJ_LOG_HAS_CR));
		PJ_LOG(3,(THIS_FILE, "\n"));
		pj_log_set_decor(0);

		part_idx = 0;
		part_len = PJ_LOG_MAX_SIZE-80;
		while (part_idx < call_dump_len) {
		    char p_orig, *p;

		    p = &buf[part_idx];
		    if (part_idx + part_len > call_dump_len)
			part_len = call_dump_len - part_idx;
		    p_orig = p[part_len];
		    p[part_len] = '\0';
		    PJ_LOG(3,(THIS_FILE, "%s", p));
		    p[part_len] = p_orig;
		    part_idx += part_len;
		}
		pj_log_set_decor(log_decor);
	    }
	}
    }
#endif

    /* Dump presence status */
    pjsua_pres_dump(detail);

    pj_log_set_decor(old_decor);
    PJ_LOG(3,(THIS_FILE, "Dump complete"));
}


/* Forward declaration. */
static void restart_listener_cb(void *user_data);


static pj_status_t handle_ip_change_on_acc()
{
    int i = 0;
    pj_status_t status = PJ_SUCCESS;
    pj_bool_t acc_done[PJSUA_MAX_ACC];

    PJSUA_LOCK();

    if (pjsua_var.acc_cnt == 0) {
	PJ_LOG(3, (THIS_FILE,
		   "No account is set, IP change handling will stop"));
	pjsua_acc_end_ip_change(NULL);
	PJSUA_UNLOCK();
	return status;
    }

    /* Reset ip_change_active flag. */
    for (; i < (int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	pjsua_var.acc[i].ip_change_op = PJSUA_IP_CHANGE_OP_NULL;
	acc_done[i] = PJ_FALSE;
    }

    for (i = 0; i < (int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	pj_bool_t shutdown_transport = PJ_FALSE;
	pjsip_regc_info regc_info;
	char acc_id[PJSUA_MAX_ACC * 4];
	pjsua_acc *acc = &pjsua_var.acc[i];
	pjsip_transport *transport = NULL;
	pjsua_acc_id shut_acc_ids[PJSUA_MAX_ACC];
	unsigned shut_acc_cnt = 0;

	if (!acc->valid || (acc_done[i]))
	    continue;

	if (acc->regc) {
	    int j = 0;
	    pj_status_t found_restart_tp_fail = PJ_FALSE;

	    pjsip_regc_get_info(acc->regc, &regc_info);

	    /* Check if transport restart listener succeed. */
	    for (; j < PJ_ARRAY_SIZE(pjsua_var.tpdata); ++j) {
		if (pjsua_var.tpdata[j].data.ptr != NULL &&
		  pjsua_var.tpdata[j].restart_status != PJ_SUCCESS &&
		  pjsua_var.tpdata[j].type == regc_info.transport->key.type)
		{
		    if ((pjsua_var.tpdata[j].data.factory
					   == regc_info.transport->factory) ||
			(pjsua_var.tpdata[j].data.tp
					       == regc_info.transport))
		    {
			found_restart_tp_fail = PJ_TRUE;
			break;
		    }
		}
	    }

	    if (found_restart_tp_fail) {
		if (acc->ka_timer.id) {
		    pjsip_endpt_cancel_timer(pjsua_var.endpt, &acc->ka_timer);
		    acc->ka_timer.id = PJ_FALSE;

		    if (acc->ka_transport) {
			pjsip_transport_dec_ref(acc->ka_transport);
			acc->ka_transport = NULL;
		    }
		}
		pjsua_acc_end_ip_change(acc);
		continue;
	    }

	    if ((regc_info.transport) &&
		((regc_info.transport->flag & PJSIP_TRANSPORT_DATAGRAM) == 0))
	    {
		transport = regc_info.transport;
		shutdown_transport = acc->cfg.ip_change_cfg.shutdown_tp;
		shut_acc_ids[shut_acc_cnt++] = acc->index;
	    }
	} else if (acc->cfg.reg_uri.slen &&
		   acc->reg_last_code != PJSIP_SC_OK &&
		   acc->reg_last_code != PJSIP_SC_REQUEST_TIMEOUT &&
		   acc->reg_last_code != PJSIP_SC_INTERNAL_SERVER_ERROR &&
		   acc->reg_last_code != PJSIP_SC_BAD_GATEWAY &&
		   acc->reg_last_code != PJSIP_SC_SERVICE_UNAVAILABLE &&
		   acc->reg_last_code != PJSIP_SC_SERVER_TIMEOUT &&
		   acc->reg_last_code != PJSIP_SC_TEMPORARILY_UNAVAILABLE)
	{
	    PJ_LOG(3, (THIS_FILE, "Permanent registration failure, "
		       "IP change handling will stop for acc %d", acc->index));

	    pjsua_acc_end_ip_change(acc);
	    continue;
	}
	pj_ansi_snprintf(acc_id, sizeof(acc_id), "#%d", i);

	if (transport) {
	    unsigned j = i + 1;

	    /* Find other account that uses the same transport. */
	    for (; j < (int)PJ_ARRAY_SIZE(pjsua_var.acc); ++j) {
		pjsip_regc_info tmp_regc_info;
		pjsua_acc *next_acc = &pjsua_var.acc[j];

		if (!next_acc->valid || !next_acc->regc ||
		    (next_acc->ip_change_op > PJSUA_IP_CHANGE_OP_NULL))
		{
		    continue;
		}

		pjsip_regc_get_info(next_acc->regc, &tmp_regc_info);
		if (transport == tmp_regc_info.transport) {
                    char tmp_buf[4];

                    pj_ansi_snprintf(tmp_buf, sizeof(tmp_buf), " #%d", j);
                    if (pj_ansi_strlen(acc_id) + pj_ansi_strlen(tmp_buf) <
                        sizeof(acc_id))
                    {
                        pj_ansi_strcat(acc_id, tmp_buf);
                    }

		    shut_acc_ids[shut_acc_cnt++] = j;
		    if (!shutdown_transport) {
			shutdown_transport =
				    next_acc->cfg.ip_change_cfg.shutdown_tp;
		    }
		}
	    }
	}

	if (shutdown_transport) {
	    unsigned j;
	    /* Shutdown the transport. */
	    PJ_LOG(3, (THIS_FILE, "Shutdown transport %s used by account %s "
		       "triggered by IP change", transport->obj_name, acc_id));

	    for (j = 0; j < shut_acc_cnt; ++j) {
		pjsua_acc *tmp_acc = &pjsua_var.acc[shut_acc_ids[j]];
		tmp_acc->ip_change_op = PJSUA_IP_CHANGE_OP_ACC_SHUTDOWN_TP;
		acc_done[shut_acc_ids[j]] = PJ_TRUE;
	    }

	    status = pjsip_transport_shutdown2(transport, PJ_TRUE);
	} else {
	    acc_done[i] = PJ_TRUE;
	    if (acc->cfg.allow_contact_rewrite && acc->cfg.reg_uri.slen) {
		status = pjsua_acc_update_contact_on_ip_change(acc);
	    } else {
		status = pjsua_acc_handle_call_on_ip_change(acc);
	    }
	}
    }
    PJSUA_UNLOCK();
    return status;
}


static pj_status_t restart_listener(pjsua_transport_id id,
				    unsigned restart_lis_delay)
{
    pj_sockaddr bind_addr;
    pjsua_transport_info tp_info;
    pj_status_t status;

    pjsua_transport_get_info(id, &tp_info);
    pj_sockaddr_init(pjsip_transport_type_get_af(tp_info.type),
		     &bind_addr,
		     NULL,
		     pj_sockaddr_get_port(&tp_info.local_addr));

    switch (tp_info.type) {
    case PJSIP_TRANSPORT_UDP:
    case PJSIP_TRANSPORT_UDP6:
	status = pjsip_udp_transport_restart2(
				       pjsua_var.tpdata[id].data.tp,
				       PJSIP_UDP_TRANSPORT_DESTROY_SOCKET,
				       PJ_INVALID_SOCKET,
				       &bind_addr,
				       NULL);
	break;

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
    case PJSIP_TRANSPORT_TLS:
    case PJSIP_TRANSPORT_TLS6:
	status = pjsip_tls_transport_restart(
					pjsua_var.tpdata[id].data.factory,
					&bind_addr,
					NULL);
	break;
#endif
    case PJSIP_TRANSPORT_TCP:
    case PJSIP_TRANSPORT_TCP6:
	status = pjsip_tcp_transport_restart(
					pjsua_var.tpdata[id].data.factory,
					&bind_addr,
					NULL);
	break;

    default:
	status = PJ_EINVAL;
    }

    PJ_PERROR(3,(THIS_FILE, status, "Listener %.*s restart",
		 tp_info.info.slen, tp_info.info.ptr));

    if (status != PJ_SUCCESS && (restart_lis_delay > 0)) {
	/* Try restarting again, with delay. */
	pjsua_schedule_timer2(&restart_listener_cb,
			      (void*)(pj_size_t)id,
			      restart_lis_delay);

	PJ_LOG(3,(THIS_FILE, "Retry listener %.*s restart in %d ms",
		  tp_info.info.slen, tp_info.info.ptr, restart_lis_delay));

	status = PJ_SUCCESS;
    } else {
	int i = 0;
	pj_bool_t all_done = PJ_TRUE;

	pjsua_var.tpdata[id].is_restarting = PJ_FALSE;
	pjsua_var.tpdata[id].restart_status = status;
	if (pjsua_var.ua_cfg.cb.on_ip_change_progress) {
	    pjsua_ip_change_op_info info;

	    pj_bzero(&info, sizeof(info));
	    info.lis_restart.transport_id = id;
	    pjsua_var.ua_cfg.cb.on_ip_change_progress(
						PJSUA_IP_CHANGE_OP_RESTART_LIS,
						status,
						&info);
	}

	/* Move forward if all listener has been restarted. */
	for (; i < PJ_ARRAY_SIZE(pjsua_var.tpdata); ++i) {
	    if (pjsua_var.tpdata[i].data.ptr != NULL &&
		pjsua_var.tpdata[i].is_restarting)
	    {
		all_done = PJ_FALSE;
		break;
	    }
	}
	if (all_done)
	    status = handle_ip_change_on_acc();
    }
    return status;
}


static void restart_listener_cb(void *user_data)
{
    pjsua_transport_id transport_id = (pjsua_transport_id)(pj_size_t)user_data;
    restart_listener(transport_id, 0);
}


PJ_DEF(pj_status_t) pjsua_handle_ip_change(const pjsua_ip_change_param *param)
{
    pj_status_t status = PJ_SUCCESS;
    int i = 0;

    PJ_ASSERT_RETURN(param, PJ_EINVAL);

    for (; i < (int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	if (pjsua_var.acc[i].valid &&
	    pjsua_var.acc[i].ip_change_op != PJSUA_IP_CHANGE_OP_NULL &&
	    pjsua_var.acc[i].ip_change_op != PJSUA_IP_CHANGE_OP_COMPLETED)
	{
	    PJ_LOG(2, (THIS_FILE,
		     "Previous IP address change handling still in progress"));
	}
    }

    PJ_LOG(3, (THIS_FILE, "Start handling IP address change"));
    if (param->restart_listener) {
	PJSUA_LOCK();
	/* Restart listener/transport, handle_ip_change_on_acc() will
	 * be called after listener restart is completed successfully.
	 */
	for (i = 0; i < PJ_ARRAY_SIZE(pjsua_var.tpdata); ++i) {
	    if (pjsua_var.tpdata[i].data.ptr != NULL) {
		pjsua_var.tpdata[i].is_restarting = PJ_TRUE;
		pjsua_var.tpdata[i].restart_status = PJ_EUNKNOWN;
	    }
	}
	for (i = 0; i < PJ_ARRAY_SIZE(pjsua_var.tpdata); ++i) {
	    if (pjsua_var.tpdata[i].data.ptr != NULL) {
		status = restart_listener(i, param->restart_lis_delay);
	    }
	}
        PJSUA_UNLOCK();
    } else {
	for (i = 0; i < PJ_ARRAY_SIZE(pjsua_var.tpdata); ++i) {
	    if (pjsua_var.tpdata[i].data.ptr != NULL) {
		pjsua_var.tpdata[i].restart_status = PJ_SUCCESS;
	    }
	}
	status = handle_ip_change_on_acc();
    }

    return status;
}

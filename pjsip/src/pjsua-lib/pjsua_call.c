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


#define THIS_FILE		"pjsua_call.c"


/* Retry interval of sending re-INVITE for locking a codec when remote
 * SDP answer contains multiple codec, in milliseconds.
 */
#define LOCK_CODEC_RETRY_INTERVAL   200

/*
 * Max UPDATE/re-INVITE retry to lock codec
 */
#define LOCK_CODEC_MAX_RETRY	     5

/* Determine whether we should restart ICE upon receiving a re-INVITE
 * with no SDP.
 */
#define RESTART_ICE_ON_REINVITE      1

/* Retry interval of trying to hangup a call. */
#define CALL_HANGUP_RETRY_INTERVAL   5000

/* Max number of hangup retries. */
#define CALL_HANGUP_MAX_RETRY	     4

/*
 * The INFO method.
 */
const pjsip_method pjsip_info_method =
{
    PJSIP_OTHER_METHOD,
    { "INFO", 4 }
};

/* UPDATE method */
static const pjsip_method pjsip_update_method =
{
    PJSIP_OTHER_METHOD,
    { "UPDATE", 6 }
};

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
				struct pjsip_inv_on_rx_offer_cb_param *param);

/*
 * Called when receiving re-INVITE.
 */
static pj_status_t pjsua_call_on_rx_reinvite(pjsip_inv_session *inv,
    		                  	     const pjmedia_sdp_session *offer,
                                  	     pjsip_rx_data *rdata);

/*
 * Called to generate new offer.
 */
static void pjsua_call_on_create_offer(pjsip_inv_session *inv,
				       pjmedia_sdp_session **offer);

/*
 * This callback is called when transaction state has changed in INVITE
 * session. We use this to trap:
 *  - incoming REFER request.
 *  - incoming MESSAGE request.
 */
static void pjsua_call_on_tsx_state_changed(pjsip_inv_session *inv,
					    pjsip_transaction *tsx,
					    pjsip_event *e);

/*
 * Redirection handler.
 */
static pjsip_redirect_op pjsua_call_on_redirected(pjsip_inv_session *inv,
						  const pjsip_uri *target,
						  const pjsip_event *e);


/* Create SDP for call hold. */
static pj_status_t create_sdp_of_call_hold(pjsua_call *call,
					   pjmedia_sdp_session **p_sdp);

/*
 * Callback called by event framework when the xfer subscription state
 * has changed.
 */
static void xfer_client_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);
static void xfer_server_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);

/* Timer callback to send re-INVITE/UPDATE to lock codec or ICE update */
static void reinv_timer_cb(pj_timer_heap_t *th, pj_timer_entry *entry);

/* Timer callback to hangup the call */
static void hangup_timer_cb(pj_timer_heap_t *th, pj_timer_entry *entry);

/* Check and send reinvite for lock codec and ICE update */
static pj_status_t process_pending_reinvite(pjsua_call *call);

/* Timer callbacks for trickle ICE */
static void trickle_ice_send_sip_info(pj_timer_heap_t *th,
				      struct pj_timer_entry *te);
static void trickle_ice_retrans_18x(pj_timer_heap_t *th,
				    struct pj_timer_entry *te);

/* End call session */
static pj_status_t call_inv_end_session(pjsua_call *call,
					unsigned code,
				        const pj_str_t *reason,
				        const pjsua_msg_data *msg_data);

/*
 * Reset call descriptor.
 */
static void reset_call(pjsua_call_id id)
{
    pjsua_call *call = &pjsua_var.calls[id];
    unsigned i;

    if (call->incoming_data) {
	pjsip_rx_data_free_cloned(call->incoming_data);
	call->incoming_data = NULL;
    }
    pj_bzero(call, sizeof(*call));
    call->index = id;
    call->last_text.ptr = call->last_text_buf_;
    call->cname.ptr = call->cname_buf;
    call->cname.slen = sizeof(call->cname_buf);
    for (i=0; i<PJ_ARRAY_SIZE(call->media); ++i) {
	pjsua_call_media *call_med = &call->media[i];
	call_med->ssrc = pj_rand();
	call_med->strm.a.conf_slot = PJSUA_INVALID_ID;
	call_med->strm.v.cap_win_id = PJSUA_INVALID_ID;
	call_med->strm.v.rdr_win_id = PJSUA_INVALID_ID;
	call_med->strm.v.strm_dec_slot = PJSUA_INVALID_ID;
	call_med->strm.v.strm_enc_slot = PJSUA_INVALID_ID;
	call_med->call = call;
	call_med->idx = i;
	call_med->tp_auto_del = PJ_TRUE;
    }
    pjsua_call_setting_default(&call->opt);
    pj_timer_entry_init(&call->reinv_timer, PJ_FALSE,
			(void*)(pj_size_t)id, &reinv_timer_cb);
    pj_bzero(&call->trickle_ice, sizeof(call->trickle_ice));
    pj_timer_entry_init(&call->trickle_ice.timer, 0, call,
			&trickle_ice_send_sip_info);
}

/* Get DTMF method type name */
static const char* get_dtmf_method_name(int type)
{
    switch (type) {
	case PJSUA_DTMF_METHOD_RFC2833:   
	    return "RFC2833";
	case PJSUA_DTMF_METHOD_SIP_INFO:  
	    return "SIP INFO";
    }
    return "(Unknown)";
}

/*
 * Init call subsystem.
 */
pj_status_t pjsua_call_subsys_init(const pjsua_config *cfg)
{
    pjsip_inv_callback inv_cb;
    unsigned i;
    const pj_str_t str_norefersub = { "norefersub", 10 };
    const pj_str_t str_trickle_ice = { "trickle-ice", 11 };
    pj_status_t status;

    /* Init calls array. */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.calls); ++i)
	reset_call(i);

    /* Copy config */
    pjsua_config_dup(pjsua_var.pool, &pjsua_var.ua_cfg, cfg);

    /* Verify settings */
    if (pjsua_var.ua_cfg.max_calls >= PJSUA_MAX_CALLS) {
	pjsua_var.ua_cfg.max_calls = PJSUA_MAX_CALLS;
    }

    /* Check the route URI's and force loose route if required */
    for (i=0; i<pjsua_var.ua_cfg.outbound_proxy_cnt; ++i) {
	status = normalize_route_uri(pjsua_var.pool,
				     &pjsua_var.ua_cfg.outbound_proxy[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Initialize invite session callback. */
    pj_bzero(&inv_cb, sizeof(inv_cb));
    inv_cb.on_state_changed = &pjsua_call_on_state_changed;
    inv_cb.on_new_session = &pjsua_call_on_forked;
    inv_cb.on_media_update = &pjsua_call_on_media_update;
    inv_cb.on_rx_offer2 = &pjsua_call_on_rx_offer;
    inv_cb.on_create_offer = &pjsua_call_on_create_offer;
    inv_cb.on_tsx_state_changed = &pjsua_call_on_tsx_state_changed;
    inv_cb.on_redirected = &pjsua_call_on_redirected;
    if (pjsua_var.ua_cfg.cb.on_call_rx_reinvite) {
    	inv_cb.on_rx_reinvite = &pjsua_call_on_rx_reinvite;
    }

    /* Initialize invite session module: */
    status = pjsip_inv_usage_init(pjsua_var.endpt, &inv_cb);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Add "norefersub" in Supported header */
    pjsip_endpt_add_capability(pjsua_var.endpt, NULL, PJSIP_H_SUPPORTED,
			       NULL, 1, &str_norefersub);

    /* Add "INFO" in Allow header, for DTMF and video key frame request. */
    pjsip_endpt_add_capability(pjsua_var.endpt, NULL, PJSIP_H_ALLOW,
			       NULL, 1, &pjsip_info_method.name);

    /* Add "trickle-ice" in Supported header */
    pjsip_endpt_add_capability(pjsua_var.endpt, NULL, PJSIP_H_SUPPORTED,
			       NULL, 1, &str_trickle_ice);

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


/* Allocate one call id */
static pjsua_call_id alloc_call_id(void)
{
    pjsua_call_id cid;

#if 1
    /* New algorithm: round-robin */
    if (pjsua_var.next_call_id >= (int)pjsua_var.ua_cfg.max_calls ||
	pjsua_var.next_call_id < 0)
    {
	pjsua_var.next_call_id = 0;
    }

    for (cid=pjsua_var.next_call_id;
	 cid<(int)pjsua_var.ua_cfg.max_calls;
	 ++cid)
    {
	if (pjsua_var.calls[cid].inv == NULL &&
            pjsua_var.calls[cid].async_call.dlg == NULL)
        {
	    ++pjsua_var.next_call_id;
	    return cid;
	}
    }

    for (cid=0; cid < pjsua_var.next_call_id; ++cid) {
	if (pjsua_var.calls[cid].inv == NULL &&
            pjsua_var.calls[cid].async_call.dlg == NULL)
        {
	    ++pjsua_var.next_call_id;
	    return cid;
	}
    }

#else
    /* Old algorithm */
    for (cid=0; cid<(int)pjsua_var.ua_cfg.max_calls; ++cid) {
	if (pjsua_var.calls[cid].inv == NULL)
	    return cid;
    }
#endif

    return PJSUA_INVALID_ID;
}

/* Get signaling secure level.
 * Return:
 *  0: if signaling is not secure
 *  1: if TLS transport is used for immediate hop
 *  2: if end-to-end signaling is secure.
 */
static int get_secure_level(pjsua_acc_id acc_id, const pj_str_t *dst_uri)
{
    const pj_str_t tls = pj_str(";transport=tls");
    const pj_str_t sips = pj_str("sips:");
    pjsua_acc *acc = &pjsua_var.acc[acc_id];

    if (pj_stristr(dst_uri, &sips))
	return 2;

    if (!pj_list_empty(&acc->route_set)) {
	pjsip_route_hdr *r = acc->route_set.next;
	pjsip_uri *uri = r->name_addr.uri;
	pjsip_sip_uri *sip_uri;

	sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);
	if (pj_stricmp2(&sip_uri->transport_param, "tls")==0)
	    return 1;

    } else {
	if (pj_stristr(dst_uri, &tls))
	    return 1;
    }

    return 0;
}

/*
static int call_get_secure_level(pjsua_call *call)
{
    if (call->inv->dlg->secure)
	return 2;

    if (!pj_list_empty(&call->inv->dlg->route_set)) {
	pjsip_route_hdr *r = call->inv->dlg->route_set.next;
	pjsip_uri *uri = r->name_addr.uri;
	pjsip_sip_uri *sip_uri;

	sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);
	if (pj_stricmp2(&sip_uri->transport_param, "tls")==0)
	    return 1;

    } else {
	pjsip_sip_uri *sip_uri;

	if (PJSIP_URI_SCHEME_IS_SIPS(call->inv->dlg->target))
	    return 2;
	if (!PJSIP_URI_SCHEME_IS_SIP(call->inv->dlg->target))
	    return 0;

	sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(call->inv->dlg->target);
	if (pj_stricmp2(&sip_uri->transport_param, "tls")==0)
	    return 1;
    }

    return 0;
}
*/

/* Outgoing call callback when media transport creation is completed. */
static pj_status_t
on_make_call_med_tp_complete(pjsua_call_id call_id,
                             const pjsua_med_tp_state_info *info)
{
    pjmedia_sdp_session *offer = NULL;
    pjsip_inv_session *inv = NULL;
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjsua_acc *acc = &pjsua_var.acc[call->acc_id];
    pjsip_dialog *dlg = call->async_call.dlg;
    unsigned options = 0;
    pjsip_tx_data *tdata;
    pj_bool_t cb_called = PJ_FALSE;
    pj_status_t status = (info? info->status: PJ_SUCCESS);

    PJSUA_LOCK();

    /* Increment the dialog's lock otherwise when invite session creation
     * fails the dialog will be destroyed prematurely.
     */
    pjsip_dlg_inc_lock(dlg);

    /* Decrement dialog session. */
    pjsip_dlg_dec_session(dlg, &pjsua_var.mod);

    if (status != PJ_SUCCESS) {
	pj_str_t err_str;
	pj_ssize_t title_len;

	call->last_code = PJSIP_SC_TEMPORARILY_UNAVAILABLE;
	pj_strcpy2(&call->last_text, "Media init error: ");

	title_len = call->last_text.slen;
	err_str = pj_strerror(status, call->last_text_buf_ + title_len,
	                      sizeof(call->last_text_buf_) - title_len);
	call->last_text.slen += err_str.slen;

	pjsua_perror(THIS_FILE, "Error initializing media channel", status);
	goto on_error;
    }

    /* pjsua_media_channel_deinit() has been called or
     * call has been hung up.
     */
    if (call->async_call.med_ch_deinit ||
        call->async_call.call_var.out_call.hangup)
    {
        PJ_LOG(4,(THIS_FILE, "Call has been hung up or media channel has "
                             "been deinitialized"));
        goto on_error;
    }

    /* Create offer */
    if ((call->opt.flag & PJSUA_CALL_NO_SDP_OFFER) == 0) {
        status = pjsua_media_channel_create_sdp(call->index, dlg->pool, NULL,
                                                &offer, NULL);
        if (status != PJ_SUCCESS) {
            pjsua_perror(THIS_FILE, "Error initializing media channel", status);
            goto on_error;
        }
    }

    /* Create the INVITE session: */
    options |= PJSIP_INV_SUPPORT_100REL;
    if (acc->cfg.require_100rel == PJSUA_100REL_MANDATORY)
	options |= PJSIP_INV_REQUIRE_100REL;
    if (acc->cfg.use_timer != PJSUA_SIP_TIMER_INACTIVE) {
	options |= PJSIP_INV_SUPPORT_TIMER;
	if (acc->cfg.use_timer == PJSUA_SIP_TIMER_REQUIRED)
	    options |= PJSIP_INV_REQUIRE_TIMER;
	else if (acc->cfg.use_timer == PJSUA_SIP_TIMER_ALWAYS)
	    options |= PJSIP_INV_ALWAYS_USE_TIMER;
    }
    if (acc->cfg.ice_cfg.enable_ice &&
	acc->cfg.ice_cfg.ice_opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED)
    {
	options |= PJSIP_INV_SUPPORT_TRICKLE_ICE;
    }

    status = pjsip_inv_create_uac( dlg, offer, options, &inv);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Invite session creation failed", status);
	goto on_error;
    }

    /* Init Session Timers */
    status = pjsip_timer_init_session(inv, &acc->cfg.timer_setting);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Session Timer init failed", status);
	goto on_error;
    }

    /* Create and associate our data in the session. */
    call->inv = inv;

    dlg->mod_data[pjsua_var.mod.id] = call;
    inv->mod_data[pjsua_var.mod.id] = call;

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(dlg, &tp_sel);
    }

    /* Set dialog Route-Set: */
    if (!pj_list_empty(&acc->route_set))
	pjsip_dlg_set_route_set(dlg, &acc->route_set);


    /* Set credentials: */
    if (acc->cred_cnt) {
	pjsip_auth_clt_set_credentials( &dlg->auth_sess,
					acc->cred_cnt, acc->cred);
    }

    /* Set authentication preference */
    pjsip_auth_clt_set_prefs(&dlg->auth_sess, &acc->cfg.auth_pref);

    /* Create initial INVITE: */

    status = pjsip_inv_invite(inv, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create initial INVITE request",
		     status);
	goto on_error;
    }


    /* Add additional headers etc */

    pjsua_process_msg_data( tdata,
                            call->async_call.call_var.out_call.msg_data);

    /* Must increment call counter now */
    ++pjsua_var.call_cnt;

    /* Send initial INVITE: */

    status = pjsip_inv_send_msg(inv, tdata);
    if (status != PJ_SUCCESS) {
	cb_called = PJ_TRUE;

	/* Upon failure to send first request, the invite
	 * session would have been cleared.
	 */
	call->inv = inv = NULL;
	goto on_error;
    }

    /* Done. */
    call->med_ch_cb = NULL;

    pjsip_dlg_dec_lock(dlg);
    PJSUA_UNLOCK();

    return PJ_SUCCESS;

on_error:
    if (inv == NULL && call_id != -1 && !cb_called &&
    	!call->hanging_up &&
	pjsua_var.ua_cfg.cb.on_call_state)
    {
	/* Use user event rather than NULL to avoid crash in
	 * unsuspecting app.
	 */
	pjsip_event user_event;
	PJSIP_EVENT_INIT_USER(user_event, 0, 0, 0, 0);

        (*pjsua_var.ua_cfg.cb.on_call_state)(call_id, &user_event);
    }

    if (dlg) {
	/* This may destroy the dialog */
	pjsip_dlg_dec_lock(dlg);
	call->async_call.dlg = NULL;
    }

    if (inv != NULL) {
	pjsip_inv_terminate(inv, PJSIP_SC_OK, PJ_FALSE);
	call->inv = NULL;
    }

    if (call_id != -1) {
	pjsua_media_channel_deinit(call_id);
	reset_call(call_id);
    }

    call->med_ch_cb = NULL;

    pjsua_check_snd_dev_idle();

    PJSUA_UNLOCK();
    return status;
}


/*
 * Cleanup call setting flag to avoid one time flags, such as
 * PJSUA_CALL_UNHOLD, PJSUA_CALL_UPDATE_CONTACT, or
 * PJSUA_CALL_NO_SDP_OFFER, to be sticky (ticket #1793).
 */
void pjsua_call_cleanup_flag(pjsua_call_setting *opt)
{
    opt->flag &= ~(PJSUA_CALL_UNHOLD | PJSUA_CALL_UPDATE_CONTACT |
		   PJSUA_CALL_NO_SDP_OFFER | PJSUA_CALL_REINIT_MEDIA |
		   PJSUA_CALL_UPDATE_VIA | PJSUA_CALL_SET_MEDIA_DIR);
}


/*
 * Initialize call settings based on account ID.
 */
PJ_DEF(void) pjsua_call_setting_default(pjsua_call_setting *opt)
{
    unsigned i;

    pj_assert(opt);

    pj_bzero(opt, sizeof(*opt));
    opt->flag = PJSUA_CALL_INCLUDE_DISABLED_MEDIA;
    opt->aud_cnt = 1;

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    opt->vid_cnt = 1;
    opt->req_keyframe_method = PJSUA_VID_REQ_KEYFRAME_SIP_INFO |
			       PJSUA_VID_REQ_KEYFRAME_RTCP_PLI;
#endif

    for (i = 0; i < PJMEDIA_MAX_SDP_MEDIA; i++) {
    	opt->media_dir[i] = PJMEDIA_DIR_ENCODING_DECODING;
    }
}

/* 
 * Initialize pjsua_call_send_dtmf_param default values. 
 */
PJ_DEF(void) pjsua_call_send_dtmf_param_default(
					     pjsua_call_send_dtmf_param *param)
{
    pj_bzero(param, sizeof(*param));
    param->duration = PJSUA_CALL_SEND_DTMF_DURATION_DEFAULT;
}

static pj_status_t apply_call_setting(pjsua_call *call,
				      const pjsua_call_setting *opt,
				      const pjmedia_sdp_session *rem_sdp)
{
    pj_assert(call);

    if (!opt) {
	pjsua_call_cleanup_flag(&call->opt);
    } else {
    	call->opt = *opt;
    }

#if !PJMEDIA_HAS_VIDEO
    pj_assert(call->opt.vid_cnt == 0);
#endif

    if (call->opt.flag & PJSUA_CALL_REINIT_MEDIA) {
    	PJ_LOG(4, (THIS_FILE, "PJSUA_CALL_REINIT_MEDIA"));
    	pjsua_media_channel_deinit(call->index);
    }

    /* If call is established or media channel hasn't been initialized,
     * reinit media channel.
     */
    if ((call->inv && call->inv->state == PJSIP_INV_STATE_CONNECTING &&
         call->med_cnt == 0) ||
        (call->inv && call->inv->state == PJSIP_INV_STATE_CONFIRMED) ||
        (call->opt.flag & PJSUA_CALL_REINIT_MEDIA))
    {
	pjsip_role_e role = rem_sdp? PJSIP_ROLE_UAS : PJSIP_ROLE_UAC;
	pj_status_t status;

	status = pjsua_media_channel_init(call->index, role,
					  call->secure_level,
					  call->inv->pool_prov,
					  rem_sdp, NULL,
					  PJ_FALSE, NULL);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error re-initializing media channel",
			 status);
	    return status;
	}
    }

    return PJ_SUCCESS;
}

static void dlg_set_via(pjsip_dialog *dlg, pjsua_acc *acc)
{
    if (acc->cfg.allow_via_rewrite && acc->via_addr.host.slen > 0) {
        pjsip_dlg_set_via_sent_by(dlg, &acc->via_addr, acc->via_tp);
    } else if (!pjsua_sip_acc_is_using_stun(acc->index)) {
   	/* Choose local interface to use in Via if acc is not using
   	 * STUN. See https://trac.pjsip.org/repos/ticket/1804
   	 */
   	pjsip_host_port via_addr;
   	const void *via_tp;

   	if (pjsua_acc_get_uac_addr(acc->index, dlg->pool, &acc->cfg.id,
   				   &via_addr, NULL, NULL,
   				   &via_tp) == PJ_SUCCESS)
   	{
   	    pjsip_dlg_set_via_sent_by(dlg, &via_addr,
   	                              (pjsip_transport*)via_tp);
   	}
    }
}


static pj_status_t dlg_set_target(pjsip_dialog *dlg, const pj_str_t *target)
{
    pjsip_uri *target_uri;
    pj_str_t tmp;
    pj_status_t status;

    /* Parse target & verify */
    pj_strdup_with_null(dlg->pool, &tmp, target);
    target_uri = pjsip_parse_uri(dlg->pool, tmp.ptr, tmp.slen, 0);
    if (!target_uri) {
	return PJSIP_EINVALIDURI;
    }
    if (!PJSIP_URI_SCHEME_IS_SIP(target_uri) &&
	!PJSIP_URI_SCHEME_IS_SIPS(target_uri))
    {
	return PJSIP_EINVALIDSCHEME;
    }

    /* Add the new target */
    status = pjsip_target_set_add_uri(&dlg->target_set, dlg->pool,
				      target_uri, 0);
    if (status != PJ_SUCCESS)
	return status;

    /* Set it as current target */
    status = pjsip_target_set_set_current(&dlg->target_set,
			    pjsip_target_set_get_next(&dlg->target_set));
    if (status != PJ_SUCCESS)
	return status;

    /* Update dialog target URI */
    dlg->target = target_uri;

    return PJ_SUCCESS;
}


/* Get account contact for call and update dialog transport */
void call_update_contact(pjsua_call *call, pj_str_t **new_contact)
{
    pjsip_tpselector tp_sel;
    pjsua_acc *acc = &pjsua_var.acc[call->acc_id];

    if (acc->cfg.force_contact.slen)
	*new_contact = &acc->cfg.force_contact;
    else if (acc->contact.slen)
	*new_contact = &acc->contact;
    else {
	/* Non-registering account */
	pjsip_dialog *dlg = call->inv->dlg;
	pj_str_t tmp_contact;
	pj_status_t status;

	status = pjsua_acc_create_uac_contact(dlg->pool,
					      &tmp_contact,
					      acc->index,
					      &dlg->remote.info_str);
	if (status == PJ_SUCCESS) {
	    *new_contact = PJ_POOL_ZALLOC_T(dlg->pool, pj_str_t);
	    **new_contact = tmp_contact;
	} else {
	    PJ_PERROR(3,(THIS_FILE, status,
			 "Call %d: failed creating contact "
			 "for contact update", call->index));
	}
    }


    /* When contact is changed, the account transport may have been
     * changed too, so let's update the dialog's transport too.
     */
    pjsua_init_tpselector(acc->cfg.transport_id, &tp_sel);
    pjsip_dlg_set_transport(call->inv->dlg, &tp_sel);
}



/*
 * Make outgoing call to the specified URI using the specified account.
 */
PJ_DEF(pj_status_t) pjsua_call_make_call(pjsua_acc_id acc_id,
					 const pj_str_t *dest_uri,
					 const pjsua_call_setting *opt,
					 void *user_data,
					 const pjsua_msg_data *msg_data,
					 pjsua_call_id *p_call_id)
{
    pj_pool_t *tmp_pool = NULL;
    pjsip_dialog *dlg = NULL;
    pjsua_acc *acc;
    pjsua_call *call = NULL;
    int call_id = -1;
    pj_str_t contact;
    pj_status_t status;

    /* Check that account is valid */
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var.acc),
		     PJ_EINVAL);

    /* Check arguments */
    PJ_ASSERT_RETURN(dest_uri, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Making call with acc #%d to %.*s", acc_id,
	      (int)dest_uri->slen, dest_uri->ptr));

    pj_log_push_indent();

    PJSUA_LOCK();

    acc = &pjsua_var.acc[acc_id];
    if (!acc->valid) {
	pjsua_perror(THIS_FILE, "Unable to make call because account "
		     "is not valid", PJ_EINVALIDOP);
	status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Find free call slot. */
    call_id = alloc_call_id();

    if (call_id == PJSUA_INVALID_ID) {
	pjsua_perror(THIS_FILE, "Error making call", PJ_ETOOMANY);
	status = PJ_ETOOMANY;
	goto on_error;
    }

    /* Clear call descriptor */
    reset_call(call_id);

    call = &pjsua_var.calls[call_id];

    /* Associate session with account */
    call->acc_id = acc_id;
    call->call_hold_type = acc->cfg.call_hold_type;

    /* Generate per-session RTCP CNAME, according to RFC 7022. */
    pj_create_random_string(call->cname_buf, call->cname.slen);

    /* Apply call setting */
    status = apply_call_setting(call, opt, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Failed to apply call setting", status);
	goto on_error;
    }
    
    /* Create sound port if none is instantiated, to check if sound device
     * can be used. But only do this with the conference bridge, as with
     * audio switchboard (i.e. APS-Direct), we can only open the sound
     * device once the correct format has been known
     */
    if (!pjsua_var.is_mswitch && pjsua_var.snd_port==NULL &&
	pjsua_var.null_snd==NULL && !pjsua_var.no_snd && call->opt.aud_cnt > 0)
    {
	status = pjsua_set_snd_dev(pjsua_var.cap_dev, pjsua_var.play_dev);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Create temporary pool */
    tmp_pool = pjsua_pool_create("tmpcall10", 512, 256);

    /* Verify that destination URI is valid before calling
     * pjsua_acc_create_uac_contact, or otherwise there
     * a misleading "Invalid Contact URI" error will be printed
     * when pjsua_acc_create_uac_contact() fails.
     */
    if (1) {
	pjsip_uri *uri;
	pj_str_t dup;

	pj_strdup_with_null(tmp_pool, &dup, dest_uri);
	uri = pjsip_parse_uri(tmp_pool, dup.ptr, dup.slen, 0);

	if (uri == NULL) {
	    pjsua_perror(THIS_FILE, "Unable to make call",
			 PJSIP_EINVALIDREQURI);
	    status = PJSIP_EINVALIDREQURI;
	    goto on_error;
	}
    }

    /* Mark call start time. */
    pj_gettimeofday(&call->start_time);

    /* Reset first response time */
    call->res_time.sec = 0;

    /* Create suitable Contact header unless a Contact header has been
     * set in the account.
     */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	status = pjsua_acc_create_uac_contact(tmp_pool, &contact,
					      acc_id, dest_uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header",
			 status);
	    goto on_error;
	}
    }

    /* Create outgoing dialog: */
    status = pjsip_dlg_create_uac( pjsip_ua_instance(),
				   &acc->cfg.id, &contact,
				   dest_uri,
                                   (msg_data && msg_data->target_uri.slen?
                                    &msg_data->target_uri: dest_uri),
                                   &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Dialog creation failed", status);
	goto on_error;
    }

    /* Increment the dialog's lock otherwise when invite session creation
     * fails the dialog will be destroyed prematurely.
     */
    pjsip_dlg_inc_lock(dlg);

    dlg_set_via(dlg, acc);

    /* Calculate call's secure level */
    call->secure_level = get_secure_level(acc_id, dest_uri);

    /* Attach user data */
    call->user_data = user_data;

    /* Store variables required for the callback after the async
     * media transport creation is completed.
     */
    if (msg_data) {
	call->async_call.call_var.out_call.msg_data = pjsua_msg_data_clone(
                                                          dlg->pool, msg_data);
    }
    call->async_call.dlg = dlg;

    /* Temporarily increment dialog session. Without this, dialog will be
     * prematurely destroyed if dec_lock() is called on the dialog before
     * the invite session is created.
     */
    pjsip_dlg_inc_session(dlg, &pjsua_var.mod);

    if ((call->opt.flag & PJSUA_CALL_NO_SDP_OFFER) == 0) {
        /* Init media channel */
        status = pjsua_media_channel_init(call->index, PJSIP_ROLE_UAC,
                                          call->secure_level, dlg->pool,
                                          NULL, NULL, PJ_TRUE,
                                          &on_make_call_med_tp_complete);
    }
    if (status == PJ_SUCCESS) {
        status = on_make_call_med_tp_complete(call->index, NULL);
        if (status != PJ_SUCCESS)
	    goto on_error;
    } else if (status != PJ_EPENDING) {
	pjsua_perror(THIS_FILE, "Error initializing media channel", status);
        pjsip_dlg_dec_session(dlg, &pjsua_var.mod);
	goto on_error;
    }

    /* Done. */

    if (p_call_id)
	*p_call_id = call_id;

    pjsip_dlg_dec_lock(dlg);
    pj_pool_release(tmp_pool);
    PJSUA_UNLOCK();

    pj_log_pop_indent();

    return PJ_SUCCESS;


on_error:
    if (dlg && call) {
	/* This may destroy the dialog */
	pjsip_dlg_dec_lock(dlg);
	call->async_call.dlg = NULL;
    }

    if (call_id != -1) {
	pjsua_media_channel_deinit(call_id);
	reset_call(call_id);
    }

    pjsua_check_snd_dev_idle();

    if (tmp_pool)
	pj_pool_release(tmp_pool);
    PJSUA_UNLOCK();

    pj_log_pop_indent();
    return status;
}


/* Get the NAT type information in remote's SDP */
static void update_remote_nat_type(pjsua_call *call,
				   const pjmedia_sdp_session *sdp)
{
    const pjmedia_sdp_attr *xnat;

    xnat = pjmedia_sdp_attr_find2(sdp->attr_count, sdp->attr, "X-nat", NULL);
    if (xnat) {
	call->rem_nat_type = (pj_stun_nat_type) (xnat->value.ptr[0] - '0');
    } else {
	call->rem_nat_type = PJ_STUN_NAT_TYPE_UNKNOWN;
    }

    PJ_LOG(5,(THIS_FILE, "Call %d: remote NAT type is %d (%s)", call->index,
	      call->rem_nat_type, pj_stun_get_nat_name(call->rem_nat_type)));
}


static pj_status_t process_incoming_call_replace(pjsua_call *call,
						 pjsip_dialog *replaced_dlg)
{
    pjsip_inv_session *replaced_inv;
    struct pjsua_call *replaced_call;
    pjsip_tx_data *tdata = NULL;
    pj_status_t status = PJ_SUCCESS;

    /* Get the invite session in the dialog */
    replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);

    /* Get the replaced call instance */
    replaced_call = (pjsua_call*) replaced_dlg->mod_data[pjsua_var.mod.id];

    /* Notify application */
    if (!replaced_call->hanging_up && pjsua_var.ua_cfg.cb.on_call_replaced)
	pjsua_var.ua_cfg.cb.on_call_replaced(replaced_call->index,
					     call->index);

    if (replaced_call->inv->state <= PJSIP_INV_STATE_EARLY &&
	replaced_call->inv->role != PJSIP_ROLE_UAC)
    {
	if (replaced_call->last_code > 100 && replaced_call->last_code < 200)
	{
	    pjsip_status_code code = replaced_call->last_code;
	    pj_str_t *text = &replaced_call->last_text;

    	    PJ_LOG(4,(THIS_FILE, "Answering replacement call %d with %d/%.*s",
				 call->index, code, text->slen, text->ptr));

	    /* Answer the new call with last response in the replaced call */
	    status = pjsip_inv_answer(call->inv, code, text, NULL, &tdata);
	}
    } else {
    	PJ_LOG(4,(THIS_FILE, "Answering replacement call %d with 200/OK",
			     call->index));

	/* Answer the new call with 200 response */
	status = pjsip_inv_answer(call->inv, 200, NULL, NULL, &tdata);
    }

    if (status == PJ_SUCCESS && tdata)
	status = pjsip_inv_send_msg(call->inv, tdata);

    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error answering session", status);

    /* Note that inv may be invalid if 200/OK has caused error in
     * starting the media.
     */

    PJ_LOG(4,(THIS_FILE, "Disconnecting replaced call %d",
			 replaced_call->index));

    /* Disconnect replaced invite session */
    status = pjsip_inv_end_session(replaced_inv, PJSIP_SC_GONE, NULL,
				   &tdata);
    if (status == PJ_SUCCESS && tdata)
	status = pjsip_inv_send_msg(replaced_inv, tdata);

    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error terminating session", status);

    return status;
}


static void process_pending_call_answer(pjsua_call *call)
{
    struct call_answer *answer, *next;

    /* No initial answer yet, this function should be called again later */
    if (!call->inv->last_answer)
	return;

    answer = call->async_call.call_var.inc_call.answers.next;
    while (answer != &call->async_call.call_var.inc_call.answers) {
        next = answer->next;
	pjsua_call_answer2(call->index, answer->opt, answer->code,
			   answer->reason, answer->msg_data);

        /* Call might have been disconnected if application is answering
         * with 200/OK and the media failed to start.
         * See pjsua_call_answer() below.
         */
        if (!call->inv || !call->inv->pool_prov)
            break;

        pj_list_erase(answer);
        answer = next;
    }
}

static pj_status_t process_pending_call_hangup(pjsua_call *call)
{
    pjsip_dialog *dlg = NULL;
    pj_status_t status;

    PJ_LOG(4,(THIS_FILE, "Call %d processing pending hangup: code=%d..",
    			 call->index, call->last_code));
    pj_log_push_indent();

    status = acquire_call("pending_hangup()", call->index, &call, &dlg);
    if (status != PJ_SUCCESS) {
    	PJ_LOG(3, (THIS_FILE, "Call %d failed to process pending hangup",
    			      call->index));
	goto on_return;
    }

    pjsua_media_channel_deinit(call->index);
    pjsua_check_snd_dev_idle();

    if (call->inv)
	call_inv_end_session(call, call->last_code, &call->last_text, NULL);

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}

pj_status_t create_temp_sdp(pj_pool_t *pool,
			    const pjmedia_sdp_session *rem_sdp,
    			    pjmedia_sdp_session **p_sdp)
{
    const pj_str_t STR_AUDIO = { "audio", 5 };
    const pj_str_t STR_VIDEO = { "video", 5 };
    const pj_str_t STR_IP6 = { "IP6", 3};

    pjmedia_sdp_session *sdp;
    pj_sockaddr origin;
    pj_uint16_t tmp_port = 50123;
    pj_status_t status = PJ_SUCCESS;
    pj_str_t tmp_st;
    unsigned i = 0;
    pj_bool_t sess_use_ipv4 = PJ_TRUE;

    /* Get one address to use in the origin field */
    pj_sockaddr_init(PJ_AF_INET, &origin, pj_strset2(&tmp_st, "127.0.0.1"), 0);

    /* Create the base (blank) SDP */
    status = pjmedia_endpt_create_base_sdp(pjsua_var.med_endpt, pool, NULL,
                                           &origin, &sdp);
    if (status != PJ_SUCCESS)
	return status;

    if (rem_sdp->conn && pj_stricmp(&rem_sdp->conn->addr_type, &STR_IP6)==0) {
	sess_use_ipv4 = PJ_FALSE;
    }

    for (; i< rem_sdp->media_count ; ++i) {
	pjmedia_sdp_media *m = NULL;
	pjmedia_sock_info sock_info;
	pj_bool_t med_use_ipv4 = sess_use_ipv4;

	if (rem_sdp->media[i]->conn && 
	    pj_stricmp(&rem_sdp->media[i]->conn->addr_type, &STR_IP6) == 0) 
	{
	    med_use_ipv4 = PJ_FALSE;
	}

	pj_sockaddr_init(med_use_ipv4?PJ_AF_INET:PJ_AF_INET6, 
			 &sock_info.rtp_addr_name, 
			 med_use_ipv4?pj_strset2(&tmp_st, "127.0.0.1"):
				      pj_strset2(&tmp_st, "::1"), 
			 rem_sdp->media[i]->desc.port? (tmp_port++):0);

	pj_sockaddr_init(med_use_ipv4?PJ_AF_INET:PJ_AF_INET6, 
			 &sock_info.rtcp_addr_name, 
			 med_use_ipv4?pj_strset2(&tmp_st, "127.0.0.1"):
				      pj_strset2(&tmp_st, "::1"), 
			 rem_sdp->media[i]->desc.port? (tmp_port++):0);

	if (pj_stricmp(&rem_sdp->media[i]->desc.media, &STR_AUDIO)==0) {
	    m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
	    status = pjmedia_endpt_create_audio_sdp(pjsua_var.med_endpt,
						    pool, &sock_info, 0, &m);

	    if (status != PJ_SUCCESS)
		return status;
	
	} else if (pj_stricmp(&rem_sdp->media[i]->desc.media, &STR_VIDEO)==0) {
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
	    m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
	    status = pjmedia_endpt_create_video_sdp(pjsua_var.med_endpt, pool,
						    &sock_info, 0, &m);
	    if (status != PJ_SUCCESS)
		return status;
#else	    
	    m = pjmedia_sdp_media_clone_deactivate(pool, rem_sdp->media[i]);
#endif	    
	} else {
	    m = pjmedia_sdp_media_clone_deactivate(pool, rem_sdp->media[i]);
	}
	if (status != PJ_SUCCESS)
	    return status;

	/* Add connection line, if none */
	if (m->conn == NULL && sdp->conn == NULL) {
	    m->conn = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
	    m->conn->net_type = pj_str("IN");
	    if (med_use_ipv4) {
		m->conn->addr_type = pj_str("IP4");
		m->conn->addr = pj_str("127.0.0.1");
	    } else {
		m->conn->addr_type = pj_str("IP6");
		m->conn->addr = pj_str("::1");
	    }
	}

	/* Disable media if it has zero format/codec */
	if (m->desc.fmt_count == 0) {
	    m->desc.fmt[m->desc.fmt_count++] = pj_str("0");
	    pjmedia_sdp_media_deactivate(pool, m);
	}

	sdp->media[sdp->media_count++] = m;
    }	   

    *p_sdp = sdp;
    return PJ_SUCCESS;
}

static pj_status_t verify_request(const pjsua_call *call,
				  pjsip_rx_data *rdata,
				  pj_bool_t use_tmp_sdp,
				  int *sip_err_code,
				  pjsip_tx_data **response)
{
    const pjmedia_sdp_session *offer = NULL;
    pjmedia_sdp_session *answer;    
    int err_code = 0;
    pj_status_t status;
    
    /* Get remote SDP offer (if any). */
    if (call->inv->neg)
    {
	pjmedia_sdp_neg_get_neg_remote(call->inv->neg, &offer);
    }

    if (use_tmp_sdp) {
	if (offer == NULL)
	    return PJ_SUCCESS;

	/* Create temporary SDP to check for codec support and capability 
	 * to handle the required SIP extensions.
	 */
	status = create_temp_sdp(call->inv->pool_prov, offer, &answer);

	if (status != PJ_SUCCESS) {
	    err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
	    pjsua_perror(THIS_FILE, "Error creating SDP answer", status);
	}
    } else {
	status = pjsua_media_channel_create_sdp(call->index,
						call->async_call.dlg->pool,
						offer, &answer, sip_err_code);

	if (status != PJ_SUCCESS) {
	    err_code = *sip_err_code;
	    pjsua_perror(THIS_FILE, "Error creating SDP answer", status);
	} else {
	    status = pjsip_inv_set_local_sdp(call->inv, answer);
	    if (status != PJ_SUCCESS) {
		err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;		
		pjsua_perror(THIS_FILE, "Error setting local SDP", status);
	    }
	}
    }

    if (status == PJ_SUCCESS) {
	unsigned options = 0;

	/* Verify that we can handle the request. */
	status = pjsip_inv_verify_request3(rdata,
					   call->inv->pool_prov, &options, 
					   offer, answer, NULL, 
					   pjsua_var.endpt, response);
	if (status != PJ_SUCCESS) {
	    /*
	     * No we can't handle the incoming INVITE request.
	     */
	    pjsua_perror(THIS_FILE, "Request verification failed", status);

	    if (response)
		err_code = (*response)->msg->line.status.code;
	    else
		err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;		
	}
    }

    if (sip_err_code && status != PJ_SUCCESS)
	*sip_err_code = err_code? err_code:PJSIP_ERRNO_TO_SIP_STATUS(status);

    return status;
}

/* Incoming call callback when media transport creation is completed. */
static pj_status_t
on_incoming_call_med_tp_complete2(pjsua_call_id call_id,
				  const pjsua_med_tp_state_info *info,
				  pjsip_rx_data *rdata,
				  int *sip_err_code,
				  pjsip_tx_data **tdata)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjsip_dialog *dlg = call->async_call.dlg;    
    pj_status_t status = (info? info->status: PJ_SUCCESS);
    int err_code = (info? info->sip_err_code: 0);
    pjsip_tx_data *response = NULL;

    PJSUA_LOCK();

    /* Increment the dialog's lock to prevent it to be destroyed prematurely,
     * such as in case of transport error.
     */
    pjsip_dlg_inc_lock(dlg);

    /* Decrement dialog session. */
    pjsip_dlg_dec_session(dlg, &pjsua_var.mod);    

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing media channel", status);
        goto on_return;
    }

    /* pjsua_media_channel_deinit() has been called. */
    if (call->async_call.med_ch_deinit) {
        pjsua_media_channel_deinit(call->index);
        call->med_ch_cb = NULL;
        pjsip_dlg_dec_lock(dlg);
        PJSUA_UNLOCK();
        return PJ_SUCCESS;
    }

    status = verify_request(call, rdata, PJ_FALSE, &err_code, &response);

on_return:
    if (status != PJ_SUCCESS) {
	if (err_code == 0)
	    err_code = PJSIP_ERRNO_TO_SIP_STATUS(status);

	if (sip_err_code)
	    *sip_err_code = err_code;

        /* If the callback is called from pjsua_call_on_incoming(), the
         * invite's state is PJSIP_INV_STATE_NULL, so the invite session
         * will be terminated later, otherwise we end the session here.
         */
        if (call->inv->state > PJSIP_INV_STATE_NULL) {            
            pj_status_t status_ = PJ_SUCCESS;

	    if (response == NULL) {
		status_ = pjsip_inv_end_session(call->inv, err_code, NULL,
						&response);
	    }

	    if (status_ == PJ_SUCCESS && response)
	        status_ = pjsip_inv_send_msg(call->inv, response);
	}
        pjsua_media_channel_deinit(call->index);
    }

    /* Set the callback to NULL to indicate that the async operation
     * has completed.
     */
    call->med_ch_cb = NULL;

    /* Finish any pending process */
    if (status == PJ_SUCCESS) {
	if (call->async_call.call_var.inc_call.replaced_dlg) {
	    /* Process pending call replace */
	    pjsip_dialog *replaced_dlg =
			call->async_call.call_var.inc_call.replaced_dlg;
	    process_incoming_call_replace(call, replaced_dlg);
	} else {
	    /* Process pending call answers */
	    process_pending_call_answer(call);
	}
    }
    
    pjsip_dlg_dec_lock(dlg);

    if (tdata)
	*tdata = response;
    
    PJSUA_UNLOCK();
    return status;
}

static pj_status_t
on_incoming_call_med_tp_complete(pjsua_call_id call_id,
				 const pjsua_med_tp_state_info *info)
{
    return on_incoming_call_med_tp_complete2(call_id, info, NULL, NULL, NULL);
}


/**
 * Handle incoming INVITE request.
 * Called by pjsua_core.c
 */
pj_bool_t pjsua_call_on_incoming(pjsip_rx_data *rdata)
{
    pj_str_t contact;
    pjsip_dialog *dlg = pjsip_rdata_get_dlg(rdata);
    pjsip_dialog *replaced_dlg = NULL;
    pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_tx_data *response = NULL;
    unsigned options = 0;
    pjsip_inv_session *inv = NULL;
    int acc_id;
    pjsua_call *call = NULL;
    int call_id = -1;
    int sip_err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
    pjmedia_sdp_session *offer=NULL;
    pj_bool_t should_dec_dlg = PJ_FALSE;
    pj_status_t status;

    /* Don't want to handle anything but INVITE */
    if (msg->line.req.method.id != PJSIP_INVITE_METHOD)
	return PJ_FALSE;

    /* Don't want to handle anything that's already associated with
     * existing dialog or transaction.
     */
    if (dlg || tsx)
	return PJ_FALSE;

    /* Don't want to accept the call if shutdown is in progress */
    if (pjsua_var.thread_quit_flag) {
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata,
				      PJSIP_SC_TEMPORARILY_UNAVAILABLE, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    PJ_LOG(4,(THIS_FILE, "Incoming %s", rdata->msg_info.info));
    pj_log_push_indent();

    PJSUA_LOCK();

    /* Find free call slot. */
    call_id = alloc_call_id();

    if (call_id == PJSUA_INVALID_ID) {
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata,
				      PJSIP_SC_BUSY_HERE, NULL,
				      NULL, NULL);
	PJ_LOG(2,(THIS_FILE,
		  "Unable to accept incoming call (too many calls)"));
	goto on_return;
    }

    /* Clear call descriptor */
    reset_call(call_id);

    call = &pjsua_var.calls[call_id];

    /* Generate per-session RTCP CNAME, according to RFC 7022. */
    pj_create_random_string(call->cname_buf, call->cname.slen);

    /* Mark call start time. */
    pj_gettimeofday(&call->start_time);

    /* Check INVITE request for Replaces header. If Replaces header is
     * present, the function will make sure that we can handle the request.
     */
    status = pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE,
					   &response);
    if (status != PJ_SUCCESS) {
	/*
	 * Something wrong with the Replaces header.
	 */
	if (response) {
	    pjsip_response_addr res_addr;

	    pjsip_get_response_addr(response->pool, rdata, &res_addr);
	    status = pjsip_endpt_send_response(pjsua_var.endpt, &res_addr, response,
				      NULL, NULL);
	    if (status != PJ_SUCCESS) pjsip_tx_data_dec_ref(response);
	} else {

	    /* Respond with 500 (Internal Server Error) */
	    pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 500, NULL,
					  NULL, NULL);
	}

	goto on_return;
    }

    /* If this INVITE request contains Replaces header, notify application
     * about the request so that application can do subsequent checking
     * if it wants to.
     */
    if (replaced_dlg != NULL &&
	(pjsua_var.ua_cfg.cb.on_call_replace_request ||
	 pjsua_var.ua_cfg.cb.on_call_replace_request2))
    {
	pjsua_call *replaced_call;
	int st_code = 200;
	pj_str_t st_text = { "OK", 2 };

	/* Get the replaced call instance */
	replaced_call = (pjsua_call*) replaced_dlg->mod_data[pjsua_var.mod.id];

	/* Copy call setting from the replaced call */
	call->opt = replaced_call->opt;
	pjsua_call_cleanup_flag(&call->opt);

	/* Notify application */
	if (!replaced_call->hanging_up &&
	    pjsua_var.ua_cfg.cb.on_call_replace_request)
	{
	    pjsua_var.ua_cfg.cb.on_call_replace_request(replaced_call->index,
							rdata,
							&st_code, &st_text);
	}

	if (!replaced_call->hanging_up &&
	    pjsua_var.ua_cfg.cb.on_call_replace_request2)
	{
	    pjsua_var.ua_cfg.cb.on_call_replace_request2(replaced_call->index,
							 rdata,
							 &st_code, &st_text,
							 &call->opt);
	}

	/* Must specify final response */
	PJ_ASSERT_ON_FAIL(st_code >= 200, st_code = 200);

	/* Check if application rejects this request. */
	if (st_code >= 300) {

	    if (st_text.slen == 2)
		st_text = *pjsip_get_status_text(st_code);

	    pjsip_endpt_respond(pjsua_var.endpt, NULL, rdata,
				st_code, &st_text, NULL, NULL, NULL);
	    goto on_return;
	}
    }

    if (!replaced_dlg) {
	/* Clone rdata. */
	pjsip_rx_data_clone(rdata, 0, &call->incoming_data);
    }

    /*
     * Get which account is most likely to be associated with this incoming
     * call. We need the account to find which contact URI to put for
     * the call.
     */
    acc_id = call->acc_id = pjsua_acc_find_for_incoming(rdata);
    if (acc_id == PJSUA_INVALID_ID) {
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata,
				      PJSIP_SC_TEMPORARILY_UNAVAILABLE, NULL,
				      NULL, NULL);

	PJ_LOG(2,(THIS_FILE,
		  "Unable to accept incoming call (no available account)"));

	goto on_return;
    }
    call->call_hold_type = pjsua_var.acc[acc_id].cfg.call_hold_type;

    /* Get call's secure level */
    if (PJSIP_URI_SCHEME_IS_SIPS(rdata->msg_info.msg->line.req.uri))
	call->secure_level = 2;
    else if (PJSIP_TRANSPORT_IS_SECURE(rdata->tp_info.transport))
	call->secure_level = 1;
    else
	call->secure_level = 0;

    /* Parse SDP from incoming request */
    if (rdata->msg_info.msg->body) {
	pjsip_rdata_sdp_info *sdp_info;

	sdp_info = pjsip_rdata_get_sdp_info(rdata);
	offer = sdp_info->sdp;

	status = sdp_info->sdp_err;
	if (status==PJ_SUCCESS && sdp_info->sdp==NULL && 
	    !PJSIP_INV_ACCEPT_UNKNOWN_BODY)
	{
	    if (sdp_info->body.ptr == NULL) {
		status = PJSIP_ERRNO_FROM_SIP_STATUS(
					       PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	    } else {
		status = PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE);
	    }
	}

	if (status != PJ_SUCCESS) {
	    pjsip_hdr hdr_list;

	    /* Check if body really contains SDP. */
	    if (sdp_info->body.ptr == NULL) {
		/* Couldn't find "application/sdp" */
		pjsip_accept_hdr *acc;		

		pjsua_perror(THIS_FILE, "Unknown Content-Type in incoming "\
		             "INVITE", status);

		/* Add Accept header to response */
		acc = pjsip_accept_hdr_create(rdata->tp_info.pool);
		PJ_ASSERT_RETURN(acc, PJ_ENOMEM);
		acc->values[acc->count++] = pj_str("application/sdp");
		pj_list_init(&hdr_list);
		pj_list_push_back(&hdr_list, acc);

		pjsip_endpt_respond(pjsua_var.endpt, NULL, rdata, 
				    PJSIP_SC_UNSUPPORTED_MEDIA_TYPE,
				    NULL, &hdr_list, NULL, NULL);
	    } else {
		const pj_str_t reason = pj_str("Bad SDP");
		pjsip_warning_hdr *w;

		pjsua_perror(THIS_FILE, "Bad SDP in incoming INVITE",
			     status);

		w = pjsip_warning_hdr_create_from_status(rdata->tp_info.pool,
					      pjsip_endpt_name(pjsua_var.endpt),
					      status);
		pj_list_init(&hdr_list);
		pj_list_push_back(&hdr_list, w);

		pjsip_endpt_respond(pjsua_var.endpt, NULL, rdata, 400,
				    &reason, &hdr_list, NULL, NULL);
	    }
	    goto on_return;
	}

	/* Do quick checks on SDP before passing it to transports. More elabore
	 * checks will be done in pjsip_inv_verify_request2() below.
	 */
	if ((offer) && (offer->media_count==0)) {
	    const pj_str_t reason = pj_str("Missing media in SDP");
	    pjsip_endpt_respond(pjsua_var.endpt, NULL, rdata, 400, &reason,
				NULL, NULL, NULL);
	    goto on_return;
	}

    } else {
	offer = NULL;
    }

    /* Verify that we can handle the request. */
    options |= PJSIP_INV_SUPPORT_100REL;
    options |= PJSIP_INV_SUPPORT_TIMER;
    if (pjsua_var.acc[acc_id].cfg.require_100rel == PJSUA_100REL_MANDATORY)
	options |= PJSIP_INV_REQUIRE_100REL;
    if (pjsua_var.acc[acc_id].cfg.ice_cfg.enable_ice) {
	options |= PJSIP_INV_SUPPORT_ICE;
	if (pjsua_var.acc[acc_id].cfg.ice_cfg.ice_opt.trickle !=
	    PJ_ICE_SESS_TRICKLE_DISABLED)
	{
	    options |= PJSIP_INV_SUPPORT_TRICKLE_ICE;
	}
    }
    if (pjsua_var.acc[acc_id].cfg.use_timer == PJSUA_SIP_TIMER_REQUIRED)
	options |= PJSIP_INV_REQUIRE_TIMER;
    else if (pjsua_var.acc[acc_id].cfg.use_timer == PJSUA_SIP_TIMER_ALWAYS)
	options |= PJSIP_INV_ALWAYS_USE_TIMER;

    status = pjsip_inv_verify_request2(rdata, &options, offer, NULL, NULL,
				       pjsua_var.endpt, &response);
    if (status != PJ_SUCCESS) {

	/*
	 * No we can't handle the incoming INVITE request.
	 */
	if (response) {
	    pjsip_response_addr res_addr;

	    pjsip_get_response_addr(response->pool, rdata, &res_addr);
	    status = pjsip_endpt_send_response(pjsua_var.endpt, &res_addr, response,
				                           NULL, NULL);
	    if (status != PJ_SUCCESS) pjsip_tx_data_dec_ref(response);

	} else {
	    /* Respond with 500 (Internal Server Error) */
	    pjsip_endpt_respond(pjsua_var.endpt, NULL, rdata, 500, NULL,
				NULL, NULL, NULL);
	}

	goto on_return;
    }

    /* Get suitable Contact header */
    if (pjsua_var.acc[acc_id].contact.slen) {
	contact = pjsua_var.acc[acc_id].contact;
    } else {
	status = pjsua_acc_create_uas_contact(rdata->tp_info.pool, &contact,
					      acc_id, rdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header",
			 status);
	    pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 500, NULL,
					  NULL, NULL);
	    goto on_return;
	}
    }

    /* Create dialog: */
    status = pjsip_dlg_create_uas_and_inc_lock( pjsip_ua_instance(), rdata,
				   		&contact, &dlg);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_respond_stateless(pjsua_var.endpt, rdata, 500, NULL,
				      NULL, NULL);
	goto on_return;
    }

    if (pjsua_var.acc[acc_id].cfg.allow_via_rewrite &&
        pjsua_var.acc[acc_id].via_addr.host.slen > 0)
    {
        pjsip_dlg_set_via_sent_by(dlg, &pjsua_var.acc[acc_id].via_addr,
                                  pjsua_var.acc[acc_id].via_tp);
    } else if (!pjsua_sip_acc_is_using_stun(acc_id)) {
	/* Choose local interface to use in Via if acc is not using
	 * STUN. See https://trac.pjsip.org/repos/ticket/1804
	 */
	char target_buf[PJSIP_MAX_URL_SIZE];
	pj_str_t target;
	pjsip_host_port via_addr;
	const void *via_tp;

	target.ptr = target_buf;
	target.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
	                              dlg->target,
	                              target_buf, sizeof(target_buf));
	if (target.slen < 0) target.slen = 0;

	if (pjsua_acc_get_uac_addr(acc_id, dlg->pool, &target,
				   &via_addr, NULL, NULL,
				   &via_tp) == PJ_SUCCESS)
	{
	    pjsip_dlg_set_via_sent_by(dlg, &via_addr,
				      (pjsip_transport*)via_tp);
	}
    }

    /* Set credentials */
    if (pjsua_var.acc[acc_id].cred_cnt) {
	pjsip_auth_clt_set_credentials(&dlg->auth_sess,
				       pjsua_var.acc[acc_id].cred_cnt,
				       pjsua_var.acc[acc_id].cred);
    }

    /* Set preference */
    pjsip_auth_clt_set_prefs(&dlg->auth_sess,
			     &pjsua_var.acc[acc_id].cfg.auth_pref);

    /* Disable Session Timers if not prefered and the incoming INVITE request
     * did not require it.
     */
    if (pjsua_var.acc[acc_id].cfg.use_timer == PJSUA_SIP_TIMER_INACTIVE &&
	(options & PJSIP_INV_REQUIRE_TIMER) == 0)
    {
	options &= ~(PJSIP_INV_SUPPORT_TIMER);
    }

    /* If 100rel is optional and UAC supports it, use it. */
    if ((options & PJSIP_INV_REQUIRE_100REL)==0 &&
	pjsua_var.acc[acc_id].cfg.require_100rel == PJSUA_100REL_OPTIONAL)
    {
	const pj_str_t token = { "100rel", 6};
	pjsip_dialog_cap_status cap_status;

	cap_status = pjsip_dlg_remote_has_cap(dlg, PJSIP_H_SUPPORTED, NULL,
	                                      &token);
	if (cap_status == PJSIP_DIALOG_CAP_SUPPORTED)
	    options |= PJSIP_INV_REQUIRE_100REL;
    }

    /* Create invite session: */
    status = pjsip_inv_create_uas( dlg, rdata, NULL, options, &inv);
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
	goto on_return;
    }

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (pjsua_var.acc[acc_id].cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(pjsua_var.acc[acc_id].cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(dlg, &tp_sel);
    }

    /* Create and attach pjsua_var data to the dialog */
    call->inv = inv;

    /* Store variables required for the callback after the async
     * media transport creation is completed.
     */
    call->async_call.dlg = dlg;
    pj_list_init(&call->async_call.call_var.inc_call.answers);

    pjsip_dlg_inc_session(dlg, &pjsua_var.mod);
    should_dec_dlg = PJ_TRUE;

    /* Init media channel, only when there is offer or call replace request.
     * For incoming call without SDP offer, media channel init will be done
     * in pjsua_call_answer(), see ticket #1526.
     */
    if (offer || replaced_dlg) {

	/* This is only for initial verification, it will check the SDP for
	 * codec support and the capability to handle the required
	 * SIP extensions.
	 */
	status = verify_request(call, rdata, PJ_TRUE, &sip_err_code, 
				&response);

	if (status != PJ_SUCCESS) {
	    pjsip_dlg_inc_lock(dlg);

	    if (response) {
		pjsip_dlg_send_response(dlg, call->inv->invite_tsx, response);
	    } else {
		pjsip_dlg_respond(dlg, rdata, sip_err_code, NULL, NULL, NULL);
	    }
		
	    if (call->inv && call->inv->dlg) {
		pjsip_inv_terminate(call->inv, sip_err_code, PJ_FALSE);
	    }
	    pjsip_dlg_dec_lock(dlg);

	    call->inv = NULL;
	    call->async_call.dlg = NULL;
	    goto on_return;
	}
	status = pjsua_media_channel_init(call->index, PJSIP_ROLE_UAS,
					  call->secure_level,
					  rdata->tp_info.pool,
					  offer,
					  &sip_err_code, PJ_TRUE,
					  &on_incoming_call_med_tp_complete);
	if (status == PJ_EPENDING) {
	    /* on_incoming_call_med_tp_complete() will call
	     * pjsip_dlg_dec_session().
	     */
	    should_dec_dlg = PJ_FALSE;
	} else  if (status == PJ_SUCCESS) {
	    /* on_incoming_call_med_tp_complete2() will call
	     * pjsip_dlg_dec_session().
	     */
	    should_dec_dlg = PJ_FALSE;

	    status = on_incoming_call_med_tp_complete2(call_id, NULL, 
						       rdata, &sip_err_code, 
						       &response);
	    if (status != PJ_SUCCESS) {		
		/* Since the call invite's state is still PJSIP_INV_STATE_NULL,
		 * the invite session was not ended in
		 * on_incoming_call_med_tp_complete(), so we need to send
		 * a response message and terminate the invite here.
		 */
		pjsip_dlg_inc_lock(dlg);

		if (response) {
		    pjsip_dlg_send_response(dlg, call->inv->invite_tsx, 
					    response);

		} else {
		    pjsip_dlg_respond(dlg, rdata, sip_err_code, NULL, NULL, 
				      NULL);
		}		

		if (call->inv && call->inv->dlg) {
		    pjsip_inv_terminate(call->inv, sip_err_code, PJ_FALSE);
		}
		pjsip_dlg_dec_lock(dlg);

		call->inv = NULL;
		call->async_call.dlg = NULL;
		goto on_return;
	    }
	} else if (status != PJ_EPENDING) {
	    pjsua_perror(THIS_FILE, "Error initializing media channel", status);
	    
	    pjsip_dlg_inc_lock(dlg);
	    pjsip_dlg_respond(dlg, rdata, sip_err_code, NULL, NULL, NULL);
	    if (call->inv && call->inv->dlg) {
		pjsip_inv_terminate(call->inv, sip_err_code, PJ_FALSE);
	    }
	    pjsip_dlg_dec_lock(dlg);

	    call->inv = NULL;
	    call->async_call.dlg = NULL;
	    goto on_return;
	}
    }

    /* Create answer */
/*
    status = pjsua_media_channel_create_sdp(call->index, rdata->tp_info.pool,
					    offer, &answer, &sip_err_code);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating SDP answer", status);
	pjsip_endpt_respond(pjsua_var.endpt, NULL, rdata,
			    sip_err_code, NULL, NULL, NULL, NULL);
	goto on_return;
    }
*/

    /* Init Session Timers */
    status = pjsip_timer_init_session(inv,
				    &pjsua_var.acc[acc_id].cfg.timer_setting);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Session Timer init failed", status);
        pjsip_dlg_respond(dlg, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL, NULL);
	pjsip_inv_terminate(inv, PJSIP_SC_INTERNAL_SERVER_ERROR, PJ_FALSE);

	pjsua_media_channel_deinit(call->index);
	call->inv = NULL;
	call->async_call.dlg = NULL;

	goto on_return;
    }

    /* Update NAT type of remote endpoint, only when there is SDP in
     * incoming INVITE!
     */
    if (pjsua_var.ua_cfg.nat_type_in_sdp && inv->neg &&
	pjmedia_sdp_neg_get_state(inv->neg) > PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER)
    {
	const pjmedia_sdp_session *remote_sdp;

	if (pjmedia_sdp_neg_get_neg_remote(inv->neg, &remote_sdp)==PJ_SUCCESS)
	    update_remote_nat_type(call, remote_sdp);
    }

    /* Must answer with some response to initial INVITE. We'll do this before
     * attaching the call to the invite session/dialog, so that the application
     * will not get notification about this event (on another scenario, it is
     * also possible that inv_send_msg() fails and causes the invite session to
     * be disconnected. If we have the call attached at this time, this will
     * cause the disconnection callback to be called before on_incoming_call()
     * callback is called, which is not right).
     */
    status = pjsip_inv_initial_answer(inv, rdata,
				      100, NULL, NULL, &response);
    if (status != PJ_SUCCESS) {
	if (response == NULL) {
	    pjsua_perror(THIS_FILE, "Unable to send answer to incoming INVITE",
			 status);
	    pjsip_dlg_respond(dlg, rdata, 500, NULL, NULL, NULL);
	    pjsip_inv_terminate(inv, 500, PJ_FALSE);
	} else {
	    pjsip_inv_send_msg(inv, response);
	    pjsip_inv_terminate(inv, response->msg->line.status.code,
				PJ_FALSE);
	}
	pjsua_media_channel_deinit(call->index);
	call->inv = NULL;
	call->async_call.dlg = NULL;
	goto on_return;

    } else {
#if !PJSUA_DISABLE_AUTO_SEND_100
	status = pjsip_inv_send_msg(inv, response);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to send 100 response", status);
	    pjsua_media_channel_deinit(call->index);
	    call->inv = NULL;
	    call->async_call.dlg = NULL;
	    goto on_return;
	}
#endif
    }

    /* Only do this after sending 100/Trying (really! see the long comment
     * above)
     */
    if (dlg->mod_data[pjsua_var.mod.id] == NULL) {
	/* In PJSUA2, on_incoming_call() may be called from 
	 * on_media_transport_created() hence this might already set
	 * to allow notification about fail events via on_call_state() and
	 * on_call_tsx_state().
	 */
	dlg->mod_data[pjsua_var.mod.id] = call;
	inv->mod_data[pjsua_var.mod.id] = call;
	++pjsua_var.call_cnt;
    }

    /* Check if this request should replace existing call */
    if (replaced_dlg) {
	/* Process call replace. If the media channel init has been completed,
	 * just process now, otherwise, just queue the replaced dialog so
	 * it will be processed once the media channel async init is finished
	 * successfully.
	 */
	if (call->med_ch_cb == NULL) {
	    process_incoming_call_replace(call, replaced_dlg);
	} else {
	    call->async_call.call_var.inc_call.replaced_dlg = replaced_dlg;
	}
    } else {
	/* Notify application if on_incoming_call() is overriden,
	 * otherwise hangup the call with 480
	 */
	if (pjsua_var.ua_cfg.cb.on_incoming_call) {
	    pjsua_var.ua_cfg.cb.on_incoming_call(acc_id, call_id, rdata);

            /* Notes:
             * - the call might be reset when it's rejected or hangup
             * by application from the callback.
	     * - onIncomingCall() may be simulated by onCreateMediaTransport()
	     * when media init is done synchrounously (see #1916). And if app
	     * happens to answer/hangup the call from the callback, the 
	     * answer/hangup should have been delayed (see #1923), 
	     * so let's process the answer/hangup now.
	     */
	    if (call->async_call.call_var.inc_call.hangup) {
		process_pending_call_hangup(call);
	    } else if (call->med_ch_cb == NULL && call->inv) {
		process_pending_call_answer(call);
	    }
	} else {
	    pjsua_call_hangup(call_id, PJSIP_SC_TEMPORARILY_UNAVAILABLE,
			      NULL, NULL);
	}
    }


    /* This INVITE request has been handled. */
on_return:
    if (dlg) {
	if (should_dec_dlg)
	    pjsip_dlg_dec_session(dlg, &pjsua_var.mod);

        pjsip_dlg_dec_lock(dlg);
    }

    if (call && call->incoming_data) {
	pjsip_rx_data_free_cloned(call->incoming_data);
	call->incoming_data = NULL;
    }
    
    pj_log_pop_indent();
    PJSUA_UNLOCK();
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
    return !pjsua_var.calls[call_id].hanging_up &&
    	   pjsua_var.calls[call_id].inv != NULL &&
	   pjsua_var.calls[call_id].inv->state != PJSIP_INV_STATE_DISCONNECTED;
}


/* Acquire lock to the specified call_id */
pj_status_t acquire_call(const char *title,
				pjsua_call_id call_id,
				pjsua_call **p_call,
				pjsip_dialog **p_dlg)
{
    unsigned retry;
    pjsua_call *call = NULL;
    pj_bool_t has_pjsua_lock = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;
    pj_time_val time_start, timeout;
    pjsip_dialog *dlg = NULL;

    pj_gettimeofday(&time_start);
    timeout.sec = 0;
    timeout.msec = PJSUA_ACQUIRE_CALL_TIMEOUT;
    pj_time_val_normalize(&timeout);

    for (retry=0; ; ++retry) {

        if (retry % 10 == 9) {
            pj_time_val dtime;

            pj_gettimeofday(&dtime);
            PJ_TIME_VAL_SUB(dtime, time_start);
            if (!PJ_TIME_VAL_LT(dtime, timeout))
                break;
        }

	has_pjsua_lock = PJ_FALSE;

	status = PJSUA_TRY_LOCK();
	if (status != PJ_SUCCESS) {
	    pj_thread_sleep(retry/10);
	    continue;
	}

	has_pjsua_lock = PJ_TRUE;
	call = &pjsua_var.calls[call_id];
        if (call->inv)
            dlg = call->inv->dlg;
        else
            dlg = call->async_call.dlg;

	if (dlg == NULL) {
	    PJSUA_UNLOCK();
	    PJ_LOG(3,(THIS_FILE, "Invalid call_id %d in %s", call_id, title));
	    return PJSIP_ESESSIONTERMINATED;
	}

	status = pjsip_dlg_try_inc_lock(dlg);
	if (status != PJ_SUCCESS) {
	    PJSUA_UNLOCK();
	    pj_thread_sleep(retry/10);
	    continue;
	}

	PJSUA_UNLOCK();

	break;
    }

    if (status != PJ_SUCCESS) {
	if (has_pjsua_lock == PJ_FALSE)
	    PJ_LOG(1,(THIS_FILE, "Timed-out trying to acquire PJSUA mutex "
				 "(possibly system has deadlocked) in %s",
				 title));
	else
	    PJ_LOG(1,(THIS_FILE, "Timed-out trying to acquire dialog mutex "
				 "(possibly system has deadlocked) in %s",
				 title));
	return PJ_ETIMEDOUT;
    }

    *p_call = call;
    *p_dlg = dlg;

    return PJ_SUCCESS;
}


/*
 * Obtain detail information about the specified call.
 */
PJ_DEF(pj_status_t) pjsua_call_get_info( pjsua_call_id call_id,
					 pjsua_call_info *info)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    unsigned mi;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    pj_bzero(info, sizeof(*info));

    /* Use PJSUA_LOCK() instead of acquire_call():
     *  https://trac.pjsip.org/repos/ticket/1371
     */
    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];
    dlg = (call->inv ? call->inv->dlg : call->async_call.dlg);
    if (!dlg) {
	PJSUA_UNLOCK();
	return PJSIP_ESESSIONTERMINATED;
    }

    /* id and role */
    info->id = call_id;
    info->role = dlg->role;
    info->acc_id = call->acc_id;

    /* local info */
    info->local_info.ptr = info->buf_.local_info;
    pj_strncpy(&info->local_info, &dlg->local.info_str,
	       sizeof(info->buf_.local_info));

    /* local contact */
    info->local_contact.ptr = info->buf_.local_contact;
    info->local_contact.slen = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
					       dlg->local.contact->uri,
					       info->local_contact.ptr,
					       sizeof(info->buf_.local_contact));
    if (info->local_contact.slen < 0)
	info->local_contact.slen = 0;

    /* remote info */
    info->remote_info.ptr = info->buf_.remote_info;
    pj_strncpy(&info->remote_info, &dlg->remote.info_str,
	       sizeof(info->buf_.remote_info));

    /* remote contact */
    if (dlg->remote.contact) {
	int len;
	info->remote_contact.ptr = info->buf_.remote_contact;
	len = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
			      dlg->remote.contact->uri,
			      info->remote_contact.ptr,
			      sizeof(info->buf_.remote_contact));
	if (len < 0) len = 0;
	info->remote_contact.slen = len;
    } else {
	info->remote_contact.slen = 0;
    }

    /* call id */
    info->call_id.ptr = info->buf_.call_id;
    pj_strncpy(&info->call_id, &dlg->call_id->id,
	       sizeof(info->buf_.call_id));

    /* call setting */
    pj_memcpy(&info->setting, &call->opt, sizeof(call->opt));

    /* state, state_text */
    if (call->hanging_up) {
        info->state = PJSIP_INV_STATE_DISCONNECTED;
    } else if (call->inv) {
        info->state = call->inv->state;
        if (call->inv->role == PJSIP_ROLE_UAS &&
            info->state == PJSIP_INV_STATE_NULL)
        {
            info->state = PJSIP_INV_STATE_INCOMING;
        }
    } else if (call->async_call.dlg && call->last_code==0) {
        info->state = PJSIP_INV_STATE_NULL;
    } else {
        info->state = PJSIP_INV_STATE_DISCONNECTED;
    }
    info->state_text = pj_str((char*)pjsip_inv_state_name(info->state));

    /* If call is disconnected, set the last_status from the cause code */
    if (call->inv && call->inv->state >= PJSIP_INV_STATE_DISCONNECTED) {
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

    /* Audio & video count offered by remote */
    info->rem_offerer   = call->rem_offerer;
    if (call->rem_offerer) {
	info->rem_aud_cnt = call->rem_aud_cnt;
	info->rem_vid_cnt = call->rem_vid_cnt;
    }

    /* Build array of active media info */
    info->media_cnt = 0;
    for (mi=0; mi < call->med_cnt &&
	       info->media_cnt < PJ_ARRAY_SIZE(info->media); ++mi)
    {
	pjsua_call_media *call_med = &call->media[mi];

	info->media[info->media_cnt].index = mi;
	info->media[info->media_cnt].status = call_med->state;
	info->media[info->media_cnt].dir = call_med->dir;
	info->media[info->media_cnt].type = call_med->type;

	if (call_med->type == PJMEDIA_TYPE_AUDIO) {
	    info->media[info->media_cnt].stream.aud.conf_slot =
						call_med->strm.a.conf_slot;
	} else if (call_med->type == PJMEDIA_TYPE_VIDEO) {
	    pjmedia_vid_dev_index cap_dev = PJMEDIA_VID_INVALID_DEV;

	    info->media[info->media_cnt].stream.vid.win_in =
						call_med->strm.v.rdr_win_id;

	    info->media[info->media_cnt].stream.vid.dec_slot =
						call_med->strm.v.strm_dec_slot;
	    info->media[info->media_cnt].stream.vid.enc_slot =
						call_med->strm.v.strm_enc_slot;

	    if (call_med->strm.v.cap_win_id != PJSUA_INVALID_ID) {
		cap_dev = call_med->strm.v.cap_dev;
	    }
	    info->media[info->media_cnt].stream.vid.cap_dev = cap_dev;
	} else {
	    continue;
	}
	++info->media_cnt;
    }

    if (call->audio_idx != -1) {
	info->media_status = call->media[call->audio_idx].state;
	info->media_dir = call->media[call->audio_idx].dir;
	info->conf_slot = call->media[call->audio_idx].strm.a.conf_slot;
    }

    /* Build array of provisional media info */
    info->prov_media_cnt = 0;
    for (mi=0; mi < call->med_prov_cnt &&
	       info->prov_media_cnt < PJ_ARRAY_SIZE(info->prov_media); ++mi)
    {
	pjsua_call_media *call_med = &call->media_prov[mi];

	info->prov_media[info->prov_media_cnt].index = mi;
	info->prov_media[info->prov_media_cnt].status = call_med->state;
	info->prov_media[info->prov_media_cnt].dir = call_med->dir;
	info->prov_media[info->prov_media_cnt].type = call_med->type;
	if (call_med->type == PJMEDIA_TYPE_AUDIO) {
	    info->prov_media[info->prov_media_cnt].stream.aud.conf_slot =
						call_med->strm.a.conf_slot;
	} else if (call_med->type == PJMEDIA_TYPE_VIDEO) {
	    pjmedia_vid_dev_index cap_dev = PJMEDIA_VID_INVALID_DEV;

	    info->prov_media[info->prov_media_cnt].stream.vid.win_in =
						call_med->strm.v.rdr_win_id;

	    if (call_med->strm.v.cap_win_id != PJSUA_INVALID_ID) {
		cap_dev = call_med->strm.v.cap_dev;
	    }
	    info->prov_media[info->prov_media_cnt].stream.vid.cap_dev=cap_dev;
	} else {
	    continue;
	}
	++info->prov_media_cnt;
    }

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

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}

/*
 * Check if call remote peer support the specified capability.
 */
PJ_DEF(pjsip_dialog_cap_status) pjsua_call_remote_has_cap(
						    pjsua_call_id call_id,
						    int htype,
						    const pj_str_t *hname,
						    const pj_str_t *token)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_status_t status;
    pjsip_dialog_cap_status cap_status;

    status = acquire_call("pjsua_call_peer_has_cap()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return PJSIP_DIALOG_CAP_UNKNOWN;

    cap_status = pjsip_dlg_remote_has_cap(dlg, htype, hname, token);

    pjsip_dlg_dec_lock(dlg);

    return cap_status;
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
 * Get remote's NAT type.
 */
PJ_DEF(pj_status_t) pjsua_call_get_rem_nat_type(pjsua_call_id call_id,
						pj_stun_nat_type *p_type)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(p_type != NULL, PJ_EINVAL);

    *p_type = pjsua_var.calls[call_id].rem_nat_type;
    return PJ_SUCCESS;
}


/*
 * Get media transport info for the specified media index.
 */
PJ_DEF(pj_status_t)
pjsua_call_get_med_transport_info(pjsua_call_id call_id,
                                  unsigned med_idx,
                                  pjmedia_transport_info *t)
{
    pjsua_call *call;
    pjsua_call_media *call_med;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(t, PJ_EINVAL);

    PJSUA_LOCK();

    call = &pjsua_var.calls[call_id];

    if (med_idx >= call->med_cnt) {
	PJSUA_UNLOCK();
	return PJ_EINVAL;
    }

    call_med = &call->media[med_idx];

    pjmedia_transport_info_init(t);
    status = pjmedia_transport_get_info(call_med->tp, t);

    PJSUA_UNLOCK();
    return status;
}


/* Media channel init callback for pjsua_call_answer(). */
static pj_status_t
on_answer_call_med_tp_complete(pjsua_call_id call_id,
                               const pjsua_med_tp_state_info *info)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjmedia_sdp_session *sdp;
    int sip_err_code = (info? info->sip_err_code: 0);
    pj_status_t status = (info? info->status: PJ_SUCCESS);

    PJSUA_LOCK();

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing media channel", status);
        goto on_return;
    }

    /* pjsua_media_channel_deinit() has been called. */
    if (call->async_call.med_ch_deinit) {
        pjsua_media_channel_deinit(call->index);
        call->med_ch_cb = NULL;
        PJSUA_UNLOCK();
        return PJ_SUCCESS;
    }

    status = pjsua_media_channel_create_sdp(call_id,
                                            call->async_call.dlg->pool,
					    NULL, &sdp, &sip_err_code);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating SDP answer", status);
        goto on_return;
    }

    status = pjsip_inv_set_local_sdp(call->inv, sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error setting local SDP", status);
        sip_err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;
        goto on_return;
    }

on_return:
    if (status != PJ_SUCCESS) {
        /* If the callback is called from pjsua_call_on_incoming(), the
         * invite's state is PJSIP_INV_STATE_NULL, so the invite session
         * will be terminated later, otherwise we end the session here.
         */
        if (call->inv->state > PJSIP_INV_STATE_NULL) {
            pjsip_tx_data *tdata;
            pj_status_t status_;

	    if (sip_err_code == 0)
		sip_err_code = PJSIP_ERRNO_TO_SIP_STATUS(status);

	    status_ = pjsip_inv_end_session(call->inv, sip_err_code, NULL,
                                            &tdata);
	    if (status_ == PJ_SUCCESS && tdata)
	        status_ = pjsip_inv_send_msg(call->inv, tdata);
        }

        pjsua_media_channel_deinit(call->index);
    }

    /* Set the callback to NULL to indicate that the async operation
     * has completed.
     */
    call->med_ch_cb = NULL;

    /* Finish any pending process */
    if (status == PJ_SUCCESS) {
	/* Process pending call answers */
	process_pending_call_answer(call);
    }

    PJSUA_UNLOCK();
    return status;
}


/*
 * Send response to incoming INVITE request.
 */
PJ_DEF(pj_status_t) pjsua_call_answer( pjsua_call_id call_id,
				       unsigned code,
				       const pj_str_t *reason,
				       const pjsua_msg_data *msg_data)
{
    return pjsua_call_answer2(call_id, NULL, code, reason, msg_data);
}


/*
 * Send response to incoming INVITE request.
 */
PJ_DEF(pj_status_t) pjsua_call_answer2(pjsua_call_id call_id,
				       const pjsua_call_setting *opt,
				       unsigned code,
				       const pj_str_t *reason,
				       const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Answering call %d: code=%d", call_id, code));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_answer()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    if (!call->inv->invite_tsx ||
    	call->inv->invite_tsx->state >= PJSIP_TSX_STATE_COMPLETED)
    {
	PJ_LOG(3,(THIS_FILE, "Unable to answer call (no incoming INVITE or "
			     "already answered)"));
	status = PJ_EINVALIDOP;
	goto on_return;
    }

    /* Apply call setting, only if status code is 1xx or 2xx. */
    if (opt && code < 300) {
	/* Check if it has not been set previously or it is different to
	 * the previous one.
	 */
	if (!call->opt_inited) {
	    call->opt_inited = PJ_TRUE;
	    apply_call_setting(call, opt, NULL);
	} else if (pj_memcmp(opt, &call->opt, sizeof(*opt)) != 0) {
	    /* Warn application about call setting inconsistency */
	    PJ_LOG(2,(THIS_FILE, "The call setting changes is ignored."));
	}
    }

    PJSUA_LOCK();

    /* Ticket #1526: When the incoming call contains no SDP offer, the media
     * channel may have not been initialized at this stage. The media channel
     * will be initialized here (along with SDP local offer generation) when
     * the following conditions are met:
     * - no pending media channel init
     * - local SDP has not been generated
     * - call setting has just been set, or SDP offer needs to be sent, i.e:
     *   answer code 183 or 2xx is issued
     */
    if (!call->med_ch_cb &&
	(call->opt_inited || (code==183 || code/100==2)) &&
	(!call->inv->neg ||
	 pjmedia_sdp_neg_get_state(call->inv->neg) ==
		PJMEDIA_SDP_NEG_STATE_NULL))
    {
	/* Mark call setting as initialized as it is just about to be used
	 * for initializing the media channel.
	 */
	call->opt_inited = PJ_TRUE;

	status = pjsua_media_channel_init(call->index, PJSIP_ROLE_UAC,
					  call->secure_level,
					  dlg->pool,
					  NULL, NULL, PJ_TRUE,
					  &on_answer_call_med_tp_complete);
	if (status == PJ_SUCCESS) {
	    status = on_answer_call_med_tp_complete(call->index, NULL);
	    if (status != PJ_SUCCESS) {
		PJSUA_UNLOCK();
		goto on_return;
	    }
	} else if (status != PJ_EPENDING) {
	    PJSUA_UNLOCK();
	    pjsua_perror(THIS_FILE, "Error initializing media channel", status);
	    goto on_return;
	}
    }

    /* If media transport creation is not yet completed, we will answer
     * the call in the media transport creation callback instead.
     * Or if initial answer is not sent yet, we will answer the call after
     * initial answer is sent (see #1923).
     */
    if (call->med_ch_cb || !call->inv->last_answer) {
        struct call_answer *answer;

        PJ_LOG(4,(THIS_FILE, "Pending answering call %d upon completion "
                             "of media transport", call_id));

        answer = PJ_POOL_ZALLOC_T(call->inv->pool_prov, struct call_answer);
        answer->code = code;
	if (opt) {
	    answer->opt = PJ_POOL_ZALLOC_T(call->inv->pool_prov,
					   pjsua_call_setting);
	    *answer->opt = *opt;
	}
        if (reason) {
	    answer->reason = PJ_POOL_ZALLOC_T(call->inv->pool_prov, pj_str_t);
            pj_strdup(call->inv->pool_prov, answer->reason, reason);
        }
        if (msg_data) {
            answer->msg_data = pjsua_msg_data_clone(call->inv->pool_prov,
                                                    msg_data);
        }
        pj_list_push_back(&call->async_call.call_var.inc_call.answers,
                          answer);

        PJSUA_UNLOCK();
        if (dlg) pjsip_dlg_dec_lock(dlg);
        pj_log_pop_indent();
        return status;
    }

    PJSUA_UNLOCK();

    if (call->res_time.sec == 0)
	pj_gettimeofday(&call->res_time);

    if (reason && reason->slen == 0)
	reason = NULL;

    /* Create response message */
    status = pjsip_inv_answer(call->inv, code, reason, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating response",
		     status);
	goto on_return;
    }

    /* Call might have been disconnected if application is answering with
     * 200/OK and the media failed to start.
     */
    if (call->inv == NULL)
	goto on_return;

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the message */
    status = pjsip_inv_send_msg(call->inv, tdata);
    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error sending response",
		     status);

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}


/*
 * Send response to incoming INVITE request.
 */
PJ_DEF(pj_status_t)
pjsua_call_answer_with_sdp(pjsua_call_id call_id,
			   const pjmedia_sdp_session *sdp, 
			   const pjsua_call_setting *opt,
			   unsigned code,
			   const pj_str_t *reason,
			   const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call("pjsua_call_answer_with_sdp()",
    			  call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    status = pjsip_inv_set_sdp_answer(call->inv, sdp);

    pjsip_dlg_dec_lock(dlg);
    
    if (status != PJ_SUCCESS)
    	return status;
    
    return pjsua_call_answer2(call_id, opt, code, reason, msg_data);
}


static pj_status_t call_inv_end_session(pjsua_call *call,
					unsigned code,
				        const pj_str_t *reason,
				        const pjsua_msg_data *msg_data)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    if (code==0) {
	if (call->inv->state == PJSIP_INV_STATE_CONFIRMED)
	    code = PJSIP_SC_OK;
	else if (call->inv->role == PJSIP_ROLE_UAS)
	    code = PJSIP_SC_DECLINE;
	else
	    code = PJSIP_SC_REQUEST_TERMINATED;
    }

    /* Stop hangup timer, if it is active. */
    if (call->hangup_timer.id) {
	pjsua_cancel_timer(&call->hangup_timer);
	call->hangup_timer.id = PJ_FALSE;
    }

    status = pjsip_inv_end_session(call->inv, code, reason, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE,
		     "Failed to create end session message",
		     status);
	goto on_return;
    }

    /* pjsip_inv_end_session may return PJ_SUCCESS with NULL
     * as p_tdata when INVITE transaction has not been answered
     * with any provisional responses.
     */
    if (tdata == NULL) {
	goto on_return;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the message */
    status = pjsip_inv_send_msg(call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE,
		     "Failed to send end session message",
		     status);
	goto on_return;
    }
    
on_return:
    /* Failure in pjsip_inv_send_msg() can cause
     * pjsua_call_on_state_changed() to be called and call to be reset,
     * so we need to check for call->inv as well.
     */
    if (status != PJ_SUCCESS && call->inv) {
    	pj_time_val delay;

    	/* Schedule a retry */
    	if (call->hangup_retry >= CALL_HANGUP_MAX_RETRY) {
    	    /* Forcefully terminate the invite session. */
	    PJ_LOG(1,(THIS_FILE,"Call %d: failed to hangup after %d retries, "
				"terminating the session forcefully now!",
				call->index, call->hangup_retry));
    	    pjsip_inv_terminate(call->inv, call->hangup_code, PJ_TRUE);
    	    return PJ_SUCCESS;
    	}
    	    
    	if (call->hangup_retry == 0) {
    	    pj_timer_entry_init(&call->hangup_timer, PJ_FALSE,
				(void*)call, &hangup_timer_cb);

    	    call->hangup_code = code;
    	    if (reason) {
    	    	pj_strdup(call->inv->pool_prov, &call->hangup_reason,
    	    	    	  reason);
    	    }
    	    if (msg_data) {
    	     	call->hangup_msg_data = pjsua_msg_data_clone(
    	     				    call->inv->pool_prov,
    	     				    msg_data);
    	    }
    	}

	delay.sec = 0;
    	delay.msec = CALL_HANGUP_RETRY_INTERVAL;
    	pj_time_val_normalize(&delay);
    	call->hangup_timer.id = PJ_TRUE;
    	pjsua_schedule_timer(&call->hangup_timer, &delay);
    	call->hangup_retry++;

       	PJ_LOG(4, (THIS_FILE, "Will retry call %d hangup in %d msec",
                              call->index, CALL_HANGUP_RETRY_INTERVAL));
    }

    return PJ_SUCCESS;
}

/* Timer callback to hangup call */
static void hangup_timer_cb(pj_timer_heap_t *th, pj_timer_entry *entry)
{
    pjsua_call* call = (pjsua_call *)entry->user_data;
    pjsip_dialog *dlg;
    pj_status_t status;

    PJ_UNUSED_ARG(th);

    pj_log_push_indent();

    status = acquire_call("hangup_timer_cb()", call->index, &call, &dlg);
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	return;
    }

    call->hangup_timer.id = PJ_FALSE;
    call_inv_end_session(call, call->hangup_code, &call->hangup_reason,
    			 call->hangup_msg_data);

    pjsip_dlg_dec_lock(dlg);

    pj_log_pop_indent();
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
    pjsip_dialog *dlg = NULL;
    pj_status_t status;

    if (call_id<0 || call_id>=(int)pjsua_var.ua_cfg.max_calls) {
	PJ_LOG(1,(THIS_FILE, "pjsua_call_hangup(): invalid call id %d",
			     call_id));
    }

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Call %d hanging up: code=%d..", call_id, code));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_hangup()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    if (!call->hanging_up) {
    	pj_bool_t delay_hangup = PJ_FALSE;
	pjsip_event user_event;

	pj_gettimeofday(&call->dis_time);
	if (call->res_time.sec == 0)
	    pj_gettimeofday(&call->res_time);

    	if (code==0) {
	    if (call->inv && call->inv->state == PJSIP_INV_STATE_CONFIRMED)
	        code = PJSIP_SC_OK;
	    else if (call->inv && call->inv->role == PJSIP_ROLE_UAS)
	    	code = PJSIP_SC_DECLINE;
	    else
	    	code = PJSIP_SC_REQUEST_TERMINATED;
    	}
    	
	call->last_code = code;
	pj_strncpy(&call->last_text,
	    	   pjsip_get_status_text(call->last_code),
		   sizeof(call->last_text_buf_));

    	/* Stop reinvite timer, if it is active. */
    	if (call->reinv_timer.id) {
	    pjsua_cancel_timer(&call->reinv_timer);
	    call->reinv_timer.id = PJ_FALSE;
    	}

    	/* If media transport creation is not yet completed, we will continue
    	 * from the media transport creation callback instead.
         */
    	if ((call->med_ch_cb && !call->inv) ||
	    ((call->inv != NULL) &&
	     (call->inv->state == PJSIP_INV_STATE_NULL)))
    	{
    	    delay_hangup = PJ_TRUE;
            PJ_LOG(4,(THIS_FILE, "Will continue call %d hangup upon "
                             	 "completion of media transport", call_id));

	    if (call->inv && call->inv->role == PJSIP_ROLE_UAS)
	    	call->async_call.call_var.inc_call.hangup = PJ_TRUE;
	    else
	    	call->async_call.call_var.out_call.hangup = PJ_TRUE;

            if (reason) {
            	pj_strncpy(&call->last_text, reason,
		       	   sizeof(call->last_text_buf_));
            }

	    call->hanging_up = PJ_TRUE;
    	} else {
    	    /* Destroy media session. */
    	    pjsua_media_channel_deinit(call_id);
	    call->hanging_up = PJ_TRUE;
	    pjsua_check_snd_dev_idle();
	}

    	/* Call callback which will report DISCONNECTED state.
    	 * Use user event rather than NULL to avoid crash in
	 * unsuspecting app.
	 */
	PJSIP_EVENT_INIT_USER(user_event, 0, 0, 0, 0);
    	if (pjsua_var.ua_cfg.cb.on_call_state) {
	    (*pjsua_var.ua_cfg.cb.on_call_state)(call->index,
	    					 &user_event);
	}

	if (call->inv && !delay_hangup) {
	    call_inv_end_session(call, code, reason, msg_data);
	}
    } else {
	/* Already requested and on progress */
        PJ_LOG(4,(THIS_FILE, "Call %d hangup request ignored as "
			     "it is on progress", call_id));
    }

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}


/*
 * Accept or reject redirection.
 */
PJ_DEF(pj_status_t) pjsua_call_process_redirect( pjsua_call_id call_id,
						 pjsip_redirect_op cmd)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call("pjsua_call_process_redirect()", call_id,
			  &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    status = pjsip_inv_process_redirect(call->inv, cmd, NULL);

    pjsip_dlg_dec_lock(dlg);

    return status;
}


/*
 * Put the specified call on hold.
 */
PJ_DEF(pj_status_t) pjsua_call_set_hold(pjsua_call_id call_id,
					const pjsua_msg_data *msg_data)
{
    return pjsua_call_set_hold2(call_id, 0, msg_data);
}

PJ_DEF(pj_status_t) pjsua_call_set_hold2(pjsua_call_id call_id,
                                         unsigned options,
					 const pjsua_msg_data *msg_data)
{
    pjmedia_sdp_session *sdp;
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pjsip_tx_data *tdata;
    pj_str_t *new_contact = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Putting call %d on hold", call_id));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_set_hold()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    if (call->inv->state != PJSIP_INV_STATE_CONFIRMED) {
	PJ_LOG(3,(THIS_FILE, "Can not hold call that is not confirmed"));
	status = PJSIP_ESESSIONSTATE;
	goto on_return;
    }

    /* We may need to re-initialize media before creating SDP */
    if (call->med_prov_cnt == 0) {
    	status = apply_call_setting(call, &call->opt, NULL);
    	if (status != PJ_SUCCESS)
	    goto on_return;
    }

    status = create_sdp_of_call_hold(call, &sdp);
    if (status != PJ_SUCCESS)
	goto on_return;

    if ((options & PJSUA_CALL_UPDATE_CONTACT) &&
	pjsua_acc_is_valid(call->acc_id))
    {
	call_update_contact(call, &new_contact);
    }

    if ((options & PJSUA_CALL_UPDATE_VIA) &&
	pjsua_acc_is_valid(call->acc_id))
    {
    	dlg_set_via(call->inv->dlg, &pjsua_var.acc[call->acc_id]);
    }

    if ((call->opt.flag & PJSUA_CALL_UPDATE_TARGET) &&
	msg_data && msg_data->target_uri.slen)
    {
	status = dlg_set_target(dlg, &msg_data->target_uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to set new target", status);
	    goto on_return;
	}
    }

    /* Create re-INVITE with new offer */
    status = pjsip_inv_reinvite( call->inv, new_contact, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create re-INVITE", status);
	goto on_return;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Record the tx_data to keep track the operation */
    call->hold_msg = (void*) tdata;

    /* Send the request */
    status = pjsip_inv_send_msg( call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send re-INVITE", status);
	call->hold_msg = NULL;
	goto on_return;
    }

    /* Set flag that local put the call on hold */
    call->local_hold = PJ_TRUE;

    /* Clear unhold flag */
    call->opt.flag &= ~PJSUA_CALL_UNHOLD;

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}


/*
 * Send re-INVITE (to release hold).
 */
PJ_DEF(pj_status_t) pjsua_call_reinvite( pjsua_call_id call_id,
                                         unsigned options,
					 const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pj_status_t status;

    status = acquire_call("pjsua_call_reinvite()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    if (options != call->opt.flag)
	call->opt.flag = options;

    status = pjsua_call_reinvite2(call_id, &call->opt, msg_data);

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Send re-INVITE (to release hold).
 */
PJ_DEF(pj_status_t) pjsua_call_reinvite2(pjsua_call_id call_id,
                                         const pjsua_call_setting *opt,
					 const pjsua_msg_data *msg_data)
{
    pjmedia_sdp_session *sdp = NULL;
    pj_str_t *new_contact = NULL;
    pjsip_tx_data *tdata;
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pj_status_t status;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Sending re-INVITE on call %d", call_id));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_reinvite2()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    if (pjsua_call_media_is_changing(call)) {
	PJ_LOG(1,(THIS_FILE, "Unable to reinvite" ERR_MEDIA_CHANGING));
	status = PJ_EINVALIDOP;
	goto on_return;
    }

    if (call->inv->state != PJSIP_INV_STATE_CONFIRMED) {
	PJ_LOG(3,(THIS_FILE, "Can not re-INVITE call that is not confirmed"));
	status = PJSIP_ESESSIONSTATE;
	goto on_return;
    }

    status = apply_call_setting(call, opt, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Failed to apply call setting", status);
	goto on_return;
    }

    /* Create SDP */
    if (call->local_hold && (call->opt.flag & PJSUA_CALL_UNHOLD)==0) {
	status = create_sdp_of_call_hold(call, &sdp);
    } else if ((call->opt.flag & PJSUA_CALL_NO_SDP_OFFER) == 0) {
	status = pjsua_media_channel_create_sdp(call->index,
						call->inv->pool_prov,
						NULL, &sdp, NULL);
    }
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to get SDP from media endpoint",
		     status);
	goto on_return;
    }

    if ((call->opt.flag & PJSUA_CALL_UPDATE_CONTACT) &&
	    pjsua_acc_is_valid(call->acc_id))
    {
	call_update_contact(call, &new_contact);
    }

    if ((call->opt.flag & PJSUA_CALL_UPDATE_VIA) &&
	pjsua_acc_is_valid(call->acc_id))
    {
    	dlg_set_via(call->inv->dlg, &pjsua_var.acc[call->acc_id]);
    }

    if ((call->opt.flag & PJSUA_CALL_UPDATE_TARGET) &&
	msg_data && msg_data->target_uri.slen)
    {
	status = dlg_set_target(dlg, &msg_data->target_uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to set new target", status);
	    goto on_return;
	}
    }

    /* Create re-INVITE with new offer */
    status = pjsip_inv_reinvite( call->inv, new_contact, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create re-INVITE", status);
	goto on_return;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the request */
    call->med_update_success = PJ_FALSE;
    status = pjsip_inv_send_msg( call->inv, tdata);
    if (status == PJ_SUCCESS &&
        ((call->opt.flag & PJSUA_CALL_UNHOLD) &&
         (call->opt.flag & PJSUA_CALL_NO_SDP_OFFER) == 0))
    {
    	call->local_hold = PJ_FALSE;
    } else if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send re-INVITE", status);
	goto on_return;
    }

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}


/*
 * Send UPDATE request.
 */
PJ_DEF(pj_status_t) pjsua_call_update( pjsua_call_id call_id,
				       unsigned options,
				       const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pj_status_t status;

    status = acquire_call("pjsua_call_update()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    if (options != call->opt.flag)
	call->opt.flag = options;

    status = pjsua_call_update2(call_id, &call->opt, msg_data);

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Send UPDATE request.
 */
PJ_DEF(pj_status_t) pjsua_call_update2(pjsua_call_id call_id,
				       const pjsua_call_setting *opt,
				       const pjsua_msg_data *msg_data)
{
    pjmedia_sdp_session *sdp = NULL;
    pj_str_t *new_contact = NULL;
    pjsip_tx_data *tdata;
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Sending UPDATE on call %d", call_id));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_update2()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Don't check media changing if UPDATE is sent without SDP */
    if (pjsua_call_media_is_changing(call) &&
	(opt && opt->flag & PJSUA_CALL_NO_SDP_OFFER) == 0)
    {
	PJ_LOG(1,(THIS_FILE, "Unable to send UPDATE" ERR_MEDIA_CHANGING));
	status = PJ_EINVALIDOP;
	goto on_return;
    }

    status = apply_call_setting(call, opt, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Failed to apply call setting", status);
	goto on_return;
    }

    /* Create SDP */
    if (call->local_hold && (call->opt.flag & PJSUA_CALL_UNHOLD)==0) {
	status = create_sdp_of_call_hold(call, &sdp);
    } else if ((call->opt.flag & PJSUA_CALL_NO_SDP_OFFER) == 0) {
	status = pjsua_media_channel_create_sdp(call->index,
						call->inv->pool_prov,
						NULL, &sdp, NULL);
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to get SDP from media endpoint",
		     status);
	goto on_return;
    }

    if ((call->opt.flag & PJSUA_CALL_UPDATE_CONTACT) &&
	    pjsua_acc_is_valid(call->acc_id))
    {
	call_update_contact(call, &new_contact);
    }

    if ((call->opt.flag & PJSUA_CALL_UPDATE_VIA) &&
	pjsua_acc_is_valid(call->acc_id))
    {
	dlg_set_via(call->inv->dlg, &pjsua_var.acc[call->acc_id]);
    }

    if ((call->opt.flag & PJSUA_CALL_UPDATE_TARGET) &&
	msg_data && msg_data->target_uri.slen)
    {
	status = dlg_set_target(dlg, &msg_data->target_uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to set new target", status);
	    goto on_return;
	}
    }

    /* Create UPDATE with new offer */
    status = pjsip_inv_update(call->inv, new_contact, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create UPDATE request", status);
	goto on_return;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the request */
    call->med_update_success = PJ_FALSE;
    status = pjsip_inv_send_msg( call->inv, tdata);
    if (status == PJ_SUCCESS &&
        ((call->opt.flag & PJSUA_CALL_UNHOLD) &&
         (call->opt.flag & PJSUA_CALL_NO_SDP_OFFER) == 0))
    {
    	call->local_hold = PJ_FALSE;
    } else if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send UPDATE request", status);
	goto on_return;
    }

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
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
    pjsip_dialog *dlg = NULL;
    pjsip_generic_string_hdr *gs_hdr;
    const pj_str_t str_ref_by = { "Referred-By", 11 };
    struct pjsip_evsub_user xfer_cb;
    pj_status_t status;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls &&
                     dest, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Transferring call %d to %.*s", call_id,
			 (int)dest->slen, dest->ptr));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_xfer()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Create xfer client subscription. */
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &xfer_client_on_evsub_state;

    status = pjsip_xfer_create_uac(call->inv->dlg, &xfer_cb, &sub);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create xfer", status);
	goto on_return;
    }

    /* Associate this call with the client subscription */
    pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, call);

    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate(sub, dest, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create REFER request", status);
	goto on_return;
    }

    /* Add Referred-By header */
    gs_hdr = pjsip_generic_string_hdr_create(tdata->pool, &str_ref_by,
					     &dlg->local.info_str);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)gs_hdr);


    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send. */
    status = pjsip_xfer_send_request(sub, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send REFER request", status);
	goto on_return;
    }

    /* For simplicity (that's what this program is intended to be!),
     * leave the original invite session as it is. More advanced application
     * may want to hold the INVITE, or terminate the invite, or whatever.
     */
on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;

}


/*
 * Initiate attended call transfer to the specified address.
 */
PJ_DEF(pj_status_t) pjsua_call_xfer_replaces( pjsua_call_id call_id,
					      pjsua_call_id dest_call_id,
					      unsigned options,
					      const pjsua_msg_data *msg_data)
{
    pjsua_call *dest_call;
    pjsip_dialog *dest_dlg;
    char str_dest_buf[PJSIP_MAX_URL_SIZE*2];
    pj_str_t str_dest;
    int len;
    char call_id_dest_buf[PJSIP_MAX_URL_SIZE * 2];
    int call_id_len;
    pjsip_uri *uri;
    pj_status_t status;
    const pjsip_parser_const_t *pconst;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(dest_call_id>=0 &&
		      dest_call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Transferring call %d replacing with call %d",
			 call_id, dest_call_id));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_xfer_replaces()", dest_call_id,
			  &dest_call, &dest_dlg);
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	return status;
    }

    /*
     * Create REFER destination URI with Replaces field.
     */

    /* Make sure we have sufficient buffer's length */
    PJ_ASSERT_ON_FAIL(dest_dlg->remote.info_str.slen +
		      dest_dlg->call_id->id.slen +
		      dest_dlg->remote.info->tag.slen +
		      dest_dlg->local.info->tag.slen + 32
		      < (long)sizeof(str_dest_buf),
		      { status=PJSIP_EURITOOLONG; goto on_error; });

    /* Print URI */
    str_dest_buf[0] = '<';
    str_dest.slen = 1;

    uri = (pjsip_uri*) pjsip_uri_get_uri(dest_dlg->remote.info->uri);
    len = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, uri,
		          str_dest_buf+1, sizeof(str_dest_buf)-1);
    if (len < 0) {
	status = PJSIP_EURITOOLONG;
	goto on_error;
    }

    str_dest.slen += len;
	
    /* This uses the the same scanner definition used for SIP parsing 
     * to escape the call-id in the refer.
     *
     * A common pattern for call-ids is: name@domain. The '@' character, 
     * when used in a URL parameter, throws off many SIP parsers. 
     * URL escape it based off of the allowed characters for header values.
    */
    pconst = pjsip_parser_const();	
    call_id_len = (int)pj_strncpy2_escape(call_id_dest_buf, &dest_dlg->call_id->id,
     					  PJ_ARRAY_SIZE(call_id_dest_buf),
     					  &pconst->pjsip_HDR_CHAR_SPEC);
    if (call_id_len < 0) {
    	status = PJSIP_EURITOOLONG;
    	goto on_error;
    }

    /* Build the URI */
    len = pj_ansi_snprintf(str_dest_buf + str_dest.slen,
			   sizeof(str_dest_buf) - str_dest.slen,
			   "?%s"
			   "Replaces=%.*s"
			   "%%3Bto-tag%%3D%.*s"
			   "%%3Bfrom-tag%%3D%.*s>",
			   ((options&PJSUA_XFER_NO_REQUIRE_REPLACES) ?
			    "" : "Require=replaces&"),
			   call_id_len,
			   call_id_dest_buf,
			   (int)dest_dlg->remote.info->tag.slen,
			   dest_dlg->remote.info->tag.ptr,
			   (int)dest_dlg->local.info->tag.slen,
			   dest_dlg->local.info->tag.ptr);

    PJ_ASSERT_ON_FAIL(len > 0 && len <= (int)sizeof(str_dest_buf)-str_dest.slen,
		      { status=PJSIP_EURITOOLONG; goto on_error; });

    str_dest.ptr = str_dest_buf;
    str_dest.slen += len;

    pjsip_dlg_dec_lock(dest_dlg);

    status = pjsua_call_xfer(call_id, &str_dest, msg_data);

    pj_log_pop_indent();
    return status;

on_error:
    if (dest_dlg) pjsip_dlg_dec_lock(dest_dlg);
    pj_log_pop_indent();
    return status;
}

/*
 * Send DTMF digits to remote.
 */
PJ_DEF(pj_status_t) pjsua_call_send_dtmf(pjsua_call_id call_id,
 				       const pjsua_call_send_dtmf_param *param)
{
    pj_status_t status = PJ_EINVAL;    

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls &&
		     param, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Call %d sending DTMF %.*s using %s method",
    		       call_id, (int)param->digits.slen, param->digits.ptr,
		       get_dtmf_method_name(param->method)));

    if (param->method == PJSUA_DTMF_METHOD_RFC2833) {
	status = pjsua_call_dial_dtmf(call_id, &param->digits);
    } else if (param->method == PJSUA_DTMF_METHOD_SIP_INFO) {
	const pj_str_t SIP_INFO = pj_str("INFO");
	int i;

	for (i = 0; i < param->digits.slen; ++i) {
	    char body[80];
	    pjsua_msg_data msg_data_;

	    pjsua_msg_data_init(&msg_data_);
	    msg_data_.content_type = pj_str("application/dtmf-relay");

	    pj_ansi_snprintf(body, sizeof(body),
			     "Signal=%c\r\n"
			     "Duration=%d",
			     param->digits.ptr[i], param->duration);
	    msg_data_.msg_body = pj_str(body);

	    status = pjsua_call_send_request(call_id, &SIP_INFO, &msg_data_);
	}
    }

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
    pjsip_dialog *dlg = NULL;
    const pj_str_t mime_text_plain = pj_str("text/plain");
    pjsip_media_type ctype;
    pjsua_im_data *im_data;
    pjsip_tx_data *tdata;
    pj_bool_t content_in_msg_data;
    pj_status_t status;

    content_in_msg_data = msg_data && (msg_data->msg_body.slen ||
				       msg_data->multipart_ctype.type.slen);

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);
    
    /* Message body must be specified. */
    PJ_ASSERT_RETURN(content || content_in_msg_data, PJ_EINVAL);

    if (content) {
	PJ_LOG(4,(THIS_FILE, "Call %d sending %d bytes MESSAGE..",
        		      call_id, (int)content->slen));
    } else {
	PJ_LOG(4,(THIS_FILE, "Call %d sending MESSAGE..",
        		      call_id));
    }

    pj_log_push_indent();

    status = acquire_call("pjsua_call_send_im()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

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

    /* Add message body, if content is set */
    if (content) {
	/* Set default media type if none is specified */
	if (mime_type == NULL) {
	    mime_type = &mime_text_plain;
	}

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
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Create IM data and attach to the request. */
    im_data = PJ_POOL_ZALLOC_T(tdata->pool, pjsua_im_data);
    im_data->acc_id = call->acc_id;
    im_data->call_id = call_id;
    im_data->to = call->inv->dlg->remote.info_str;
    if (content)
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
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
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
    pjsip_dialog *dlg = NULL;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Call %d sending typing indication..",
            	          call_id));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_send_typing_ind", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

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
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}


/*
 * Send arbitrary request.
 */
PJ_DEF(pj_status_t) pjsua_call_send_request(pjsua_call_id call_id,
					    const pj_str_t *method_str,
					    const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg = NULL;
    pjsip_method method;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls,
		     PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Call %d sending %.*s request..",
            	          call_id, (int)method_str->slen, method_str->ptr));
    pj_log_push_indent();

    status = acquire_call("pjsua_call_send_request", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Init method */
    pjsip_method_init_np(&method, (pj_str_t*)method_str);

    /* Create request message. */
    status = pjsip_dlg_create_request( call->inv->dlg, &method, -1, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create request", status);
	goto on_return;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( tdata, msg_data);

    /* Send the request. */
    status = pjsip_dlg_send_request( call->inv->dlg, tdata, -1, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send request", status);
	goto on_return;
    }

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}


/*
 * Terminate all calls.
 */
PJ_DEF(void) pjsua_call_hangup_all(void)
{
    unsigned i;

    PJ_LOG(4,(THIS_FILE, "Hangup all calls.."));
    pj_log_push_indent();

    // This may deadlock, see https://trac.pjsip.org/repos/ticket/1305
    //PJSUA_LOCK();

    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	if (pjsua_var.calls[i].inv)
	    pjsua_call_hangup(i, 0, NULL, NULL);
    }

    //PJSUA_UNLOCK();
    pj_log_pop_indent();
}


/* Timer callback to send re-INVITE/UPDATE to lock codec or ICE update */
static void reinv_timer_cb(pj_timer_heap_t *th, pj_timer_entry *entry)
{
    pjsua_call_id call_id = (pjsua_call_id)(pj_size_t)entry->user_data;
    pjsip_dialog *dlg;
    pjsua_call *call;
    pj_status_t status;

    PJ_UNUSED_ARG(th);

    pjsua_var.calls[call_id].reinv_timer.id = PJ_FALSE;

    pj_log_push_indent();

    status = acquire_call("reinv_timer_cb()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	return;
    }

    process_pending_reinvite(call);

    pjsip_dlg_dec_lock(dlg);

    pj_log_pop_indent();
}


/* Check if the specified format can be skipped in counting codecs */
static pj_bool_t is_non_av_fmt(const pjmedia_sdp_media *m,
			       const pj_str_t *fmt)
{
    const pj_str_t STR_TEL = {"telephone-event", 15};
    unsigned pt;

    pt = pj_strtoul(fmt);

    /* Check for comfort noise */
    if (pt == PJMEDIA_RTP_PT_CN)
	return PJ_TRUE;

    /* Dynamic PT, check the format name */
    if (pt >= 96) {
	pjmedia_sdp_attr *a;
	pjmedia_sdp_rtpmap rtpmap;

	/* Get the format name */
	a = pjmedia_sdp_attr_find2(m->attr_count, m->attr, "rtpmap", fmt);
	if (a && pjmedia_sdp_attr_get_rtpmap(a, &rtpmap)==PJ_SUCCESS) {
	    /* Check for telephone-event */
	    if (pj_stricmp(&rtpmap.enc_name, &STR_TEL)==0)
		return PJ_TRUE;
	} else {
	    /* Invalid SDP, should not reach here */
	    pj_assert(!"SDP should have been validated!");
	    return PJ_TRUE;
	}
    }

    return PJ_FALSE;
}


/* Schedule check for the need of re-INVITE/UPDATE after media update, cases:
 * - lock codec if remote answerer has given us more than one codecs
 * - update ICE default transport address if it has changed after ICE
 *   connectivity check.
 */
void pjsua_call_schedule_reinvite_check(pjsua_call *call, unsigned delay_ms)
{
    pj_time_val delay;

    /* Stop reinvite timer, if it is active */
    if (call->reinv_timer.id)
	pjsua_cancel_timer(&call->reinv_timer);

    delay.sec = 0;
    delay.msec = delay_ms;
    pj_time_val_normalize(&delay);
    call->reinv_timer.id = PJ_TRUE;
    pjsua_schedule_timer(&call->reinv_timer, &delay);
}


/* Check if lock codec is needed */
static pj_bool_t check_lock_codec(pjsua_call *call)
{
    const pjmedia_sdp_session *local_sdp, *remote_sdp;
    pj_bool_t has_mult_fmt = PJ_FALSE;
    unsigned i;
    pj_status_t status;

    /* Check if lock codec is disabled */
    if (!pjsua_var.acc[call->acc_id].cfg.lock_codec)
	return PJ_FALSE;

    /* Check lock codec retry count */
    if (call->lock_codec.retry_cnt >= LOCK_CODEC_MAX_RETRY)
        return PJ_FALSE;

    /* Check if we are the answerer, we shouldn't need to lock codec */
    if (!call->inv->neg || !pjmedia_sdp_neg_was_answer_remote(call->inv->neg))
        return PJ_FALSE;

    /* Check if remote answerer has given us more than one codecs. */
    status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &local_sdp);
    if (status != PJ_SUCCESS)
	return PJ_FALSE;
    status = pjmedia_sdp_neg_get_active_remote(call->inv->neg, &remote_sdp);
    if (status != PJ_SUCCESS)
	return PJ_FALSE;

    for (i = 0; i < call->med_cnt && !has_mult_fmt; ++i) {
	pjsua_call_media *call_med = &call->media[i];
	const pjmedia_sdp_media *rem_m, *loc_m;
	unsigned codec_cnt = 0;
	unsigned j;

	/* Skip this if the media is inactive or error */
	if (call_med->state == PJSUA_CALL_MEDIA_NONE ||
	    call_med->state == PJSUA_CALL_MEDIA_ERROR ||
	    call_med->dir == PJMEDIA_DIR_NONE)
	{
	    continue;
	}

	/* Remote may answer with less media lines. */
	if (i >= remote_sdp->media_count)
	    continue;

	rem_m = remote_sdp->media[i];
	loc_m = local_sdp->media[i];

	/* Verify that media must be active. */
	pj_assert(loc_m->desc.port && rem_m->desc.port);
	PJ_UNUSED_ARG(loc_m);

	/* Count the formats in the answer. */
	for (j=0; j<rem_m->desc.fmt_count && codec_cnt <= 1; ++j) {
	    if (!is_non_av_fmt(rem_m, &rem_m->desc.fmt[j]) && ++codec_cnt > 1)
		has_mult_fmt = PJ_TRUE;
	}
    }

    /* Reset retry count when remote answer has one codec */
    if (!has_mult_fmt)
	call->lock_codec.retry_cnt = 0;

    return has_mult_fmt;
}

/* Check if ICE setup is complete and if it needs to send reinvite */
static pj_bool_t check_ice_complete(pjsua_call *call, pj_bool_t *need_reinv)
{
    pj_bool_t ice_need_reinv = PJ_FALSE;
    pj_bool_t ice_complete = PJ_TRUE;
    unsigned i;

    /* Check if ICE setup is complete and if it needs reinvite */
    for (i = 0; i < call->med_cnt; ++i) {
	pjsua_call_media *call_med = &call->media[i];
	pjmedia_transport_info tpinfo;
	pjmedia_ice_transport_info *ice_info;

	if (call_med->tp_st == PJSUA_MED_TP_NULL ||
	    call_med->tp_st == PJSUA_MED_TP_DISABLED ||
	    call_med->state == PJSUA_CALL_MEDIA_ERROR)
	{
	    continue;
	}

	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call_med->tp, &tpinfo);
	ice_info = (pjmedia_ice_transport_info*)
		   pjmedia_transport_info_get_spc_info(
					&tpinfo, PJMEDIA_TRANSPORT_TYPE_ICE);

	/* Check if ICE is active */
	if (!ice_info || !ice_info->active)
	    continue;

	/* Check if ICE setup not completed yet */
	if (ice_info->sess_state < PJ_ICE_STRANS_STATE_RUNNING)	{
	    ice_complete = PJ_FALSE;
	    break;
	}

	/* Check if ICE needs to send reinvite */
	if (!ice_need_reinv &&
	    ice_info->sess_state == PJ_ICE_STRANS_STATE_RUNNING &&
	    ice_info->role == PJ_ICE_SESS_ROLE_CONTROLLING)
	{
	    pjsua_ice_config *cfg=&pjsua_var.acc[call->acc_id].cfg.ice_cfg;
	    if ((cfg->ice_always_update && !call->reinv_ice_sent) ||
		pj_sockaddr_cmp(&tpinfo.sock_info.rtp_addr_name,
				&call_med->rtp_addr))
	    {
		ice_need_reinv = PJ_TRUE;
	    }
	}
    }

    if (ice_complete && need_reinv)
	*need_reinv = ice_need_reinv;

    return ice_complete;
}

/* Check and send reinvite for lock codec and ICE update */
static pj_status_t process_pending_reinvite(pjsua_call *call)
{
    const pj_str_t ST_UPDATE = {"UPDATE", 6};
    pj_pool_t *pool = call->inv->pool_prov;
    pjsip_inv_session *inv = call->inv;
    pj_bool_t ice_need_reinv;
    pj_bool_t ice_completed;
    pj_bool_t need_lock_codec;
    pj_bool_t rem_can_update;
    pjmedia_sdp_session *new_offer;
    pjsip_tx_data *tdata;
    unsigned i;
    pj_status_t status;

    /* Verify if another SDP negotiation is in progress, e.g: session timer
     * or another re-INVITE.
     */
    if (inv==NULL || inv->neg==NULL ||
	pjmedia_sdp_neg_get_state(inv->neg)!=PJMEDIA_SDP_NEG_STATE_DONE)
    {
	return PJMEDIA_SDPNEG_EINSTATE;
    }

    /* Don't do this if call is disconnecting! */
    if (inv->state > PJSIP_INV_STATE_CONFIRMED || inv->cause >= 200)
    {
	return PJ_EINVALIDOP;
    }

    /* Delay this when the SDP negotiation done in call state EARLY and
     * remote does not support UPDATE method.
     */
    if (inv->state == PJSIP_INV_STATE_EARLY &&
	pjsip_dlg_remote_has_cap(inv->dlg, PJSIP_H_ALLOW, NULL, &ST_UPDATE)!=
	PJSIP_DIALOG_CAP_SUPPORTED)
    {
        call->reinv_pending = PJ_TRUE;
        return PJ_EPENDING;
    }

    /* Check if ICE setup is complete and if it needs reinvite */
    ice_completed = check_ice_complete(call, &ice_need_reinv);
    if (!ice_completed)
	return PJ_EPENDING;

    /* Check if we need to lock codec */
    need_lock_codec = check_lock_codec(call);

    /* Check if reinvite is really needed */
    if (!need_lock_codec && !ice_need_reinv)
	return PJ_SUCCESS;


    /* Okay! So we need to send re-INVITE/UPDATE */

    /* Check if remote support UPDATE */
    rem_can_update = pjsip_dlg_remote_has_cap(inv->dlg, PJSIP_H_ALLOW, NULL,
					      &ST_UPDATE) ==
						PJSIP_DIALOG_CAP_SUPPORTED;

    /* Logging stuff */
    {
	const char *ST_ICE_UPDATE = "ICE transport address after "
				    "ICE negotiation";
	const char *ST_LOCK_CODEC = "media session to use only one codec";
	PJ_LOG(4,(THIS_FILE, "Call %d sending %s for updating %s%s%s",
		  call->index,
		  (rem_can_update? "UPDATE" : "re-INVITE"),
		  (ice_need_reinv? ST_ICE_UPDATE : ST_LOCK_CODEC),
		  (ice_need_reinv && need_lock_codec? " and " : ""),
		  (ice_need_reinv && need_lock_codec? ST_LOCK_CODEC : "")
		  ));
    }

    /* Clear reinit media flag. Should we also cleanup other flags here? */
    call->opt.flag &= ~PJSUA_CALL_REINIT_MEDIA;

    /* Generate SDP re-offer */
    status = pjsua_media_channel_create_sdp(call->index, pool, NULL,
					    &new_offer, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	return status;
    }

    /* Update the new offer so it contains only a codec. Note that
     * SDP nego has removed unmatched codecs from the offer and the codec
     * order in the offer has been matched to the answer, so we'll override
     * the codecs in the just generated SDP with the ones from the active
     * local SDP and leave just one codec for the next SDP re-offer.
     */
    if (need_lock_codec) {
	const pjmedia_sdp_session *ref_sdp;

	/* Get local active SDP as reference */
	status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &ref_sdp);
	if (status != PJ_SUCCESS)
	    return status;

	/* Verify media count. Note that remote may add/remove media line
	 * in the answer. When answer has less media, it must have been
	 * handled by pjsua_media_channel_update() as disabled media.
	 * When answer has more media, it must have been ignored (treated
	 * as non-exist) anywhere. Local media count should not be updated
	 * at this point, as modifying media count operation (i.e: reinvite,
	 * update, vid_set_strm) is currently blocking, protected with
	 * dialog mutex, and eventually reset SDP nego state to LOCAL OFFER.
	 */
	if (call->med_cnt != ref_sdp->media_count ||
	    ref_sdp->media_count != new_offer->media_count)
	{
	    /* Anyway, just in case, let's just return error */
	    return PJMEDIA_SDPNEG_EINSTATE;
	}

	for (i = 0; i < call->med_cnt; ++i) {
	    unsigned j, codec_cnt = 0;
	    const pjmedia_sdp_media *ref_m = ref_sdp->media[i];
	    pjmedia_sdp_media *m = new_offer->media[i];
	    pjsua_call_media *call_med = &call->media[i];

	    /* Verify if media is deactivated */
	    if (call_med->state == PJSUA_CALL_MEDIA_NONE ||
		call_med->state == PJSUA_CALL_MEDIA_ERROR ||
		call_med->dir == PJMEDIA_DIR_NONE)
	    {
		continue;
	    }

	    /* Reset formats */
	    m->desc.fmt_count = 0;
	    pjmedia_sdp_attr_remove_all(&m->attr_count, m->attr, "rtpmap");
	    pjmedia_sdp_attr_remove_all(&m->attr_count, m->attr, "fmtp");

	    /* Copy only the first format + any non-AV formats from
	     * the active local SDP.
	     */
	    for (j = 0; j < ref_m->desc.fmt_count; ++j) {
		const pj_str_t *fmt = &ref_m->desc.fmt[j];

		if (is_non_av_fmt(ref_m, fmt) || (++codec_cnt == 1)) {
		    pjmedia_sdp_attr *a;

		    m->desc.fmt[m->desc.fmt_count++] = *fmt;
		    a = pjmedia_sdp_attr_find2(ref_m->attr_count, ref_m->attr,
					       "rtpmap", fmt);
		    if (a) {
		        pjmedia_sdp_attr_add(&m->attr_count, m->attr,
		    			     pjmedia_sdp_attr_clone(pool, a));
		    }
		    a = pjmedia_sdp_attr_find2(ref_m->attr_count, ref_m->attr,
					       "fmtp", fmt);
		    if (a) {
		        pjmedia_sdp_attr_add(&m->attr_count, m->attr,
		        		     pjmedia_sdp_attr_clone(pool, a));
		    }
		}
	    }
	}
    }

    /* Put back original direction and "c=0.0.0.0" line */
    {
	const pjmedia_sdp_session *cur_sdp;

	/* Get local active SDP */
	status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &cur_sdp);
	if (status != PJ_SUCCESS)
	    return status;

	/* Make sure media count has not been changed */
	if (call->med_cnt != cur_sdp->media_count)
	    return PJMEDIA_SDPNEG_EINSTATE;

	for (i = 0; i < call->med_cnt; ++i) {
	    const pjmedia_sdp_media *m = cur_sdp->media[i];
	    pjmedia_sdp_media *new_m = new_offer->media[i];
	    pjsua_call_media *call_med = &call->media[i];
	    pjmedia_sdp_attr *a = NULL;

	    /* Update direction to the current dir */
	    pjmedia_sdp_media_remove_all_attr(new_m, "sendrecv");
	    pjmedia_sdp_media_remove_all_attr(new_m, "sendonly");
	    pjmedia_sdp_media_remove_all_attr(new_m, "recvonly");
	    pjmedia_sdp_media_remove_all_attr(new_m, "inactive");

	    if (call_med->dir == PJMEDIA_DIR_ENCODING_DECODING) {
		a = pjmedia_sdp_attr_create(pool, "sendrecv", NULL);
	    } else if (call_med->dir == PJMEDIA_DIR_ENCODING) {
		a = pjmedia_sdp_attr_create(pool, "sendonly", NULL);
	    } else if (call_med->dir == PJMEDIA_DIR_DECODING) {
		a = pjmedia_sdp_attr_create(pool, "recvonly", NULL);
	    } else {
		const pjmedia_sdp_conn *conn;
		a = pjmedia_sdp_attr_create(pool, "inactive", NULL);

		/* Also check if the original c= line address is zero */
		conn = m->conn;
		if (!conn)
		    conn = cur_sdp->conn;
		if (pj_strcmp2(&conn->addr, "0.0.0.0")==0 ||
		    pj_strcmp2(&conn->addr, "0")==0)
		{
		    if (!new_m->conn) {
			new_m->conn = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
		    }

		    if (pj_strcmp2(&new_m->conn->addr, "0.0.0.0")) {
			new_m->conn->net_type = pj_str("IN");
			new_m->conn->addr_type = pj_str("IP4");
			new_m->conn->addr = pj_str("0.0.0.0");
		    }
		}
	    }

	    pj_assert(a);
	    pjmedia_sdp_media_add_attr(new_m, a);
	}
    }


    if (rem_can_update) {
	status = pjsip_inv_update(inv, NULL, new_offer, &tdata);
    } else {
	status = pjsip_inv_reinvite(inv, NULL, new_offer, &tdata);
    }

    if (status==PJ_EINVALIDOP &&
	++call->lock_codec.retry_cnt < LOCK_CODEC_MAX_RETRY)
    {
	/* Ups, let's reschedule again */
	pjsua_call_schedule_reinvite_check(call, LOCK_CODEC_RETRY_INTERVAL);
	return PJ_SUCCESS;
    } else if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating UPDATE/re-INVITE",
		     status);
	return status;
    }

    /* Send the UPDATE/re-INVITE request */
    status = pjsip_inv_send_msg(inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error sending UPDATE/re-INVITE",
		     status);
	return status;
    }

    /* Update flags */
    if (ice_need_reinv)
	call->reinv_ice_sent = PJ_TRUE;
    if (need_lock_codec)
	++call->lock_codec.retry_cnt;

    return PJ_SUCCESS;
}


static void trickle_ice_retrans_18x(pj_timer_heap_t *th,
				    struct pj_timer_entry *te)
{
    pjsua_call *call = (pjsua_call*)te->user_data;
    pjsip_tx_data *tdata = NULL;
    pj_time_val delay;

    PJ_UNUSED_ARG(th);

    /* If trickling has been started or dialog has been established on
     * both sides, stop 18x retransmission.
     */
    if (call->trickle_ice.trickling >= PJSUA_OP_STATE_RUNNING ||
	call->trickle_ice.remote_dlg_est)
    {
	return;
    }

    /* Make sure last tdata is 18x response */
    if (call->inv->invite_tsx)
	tdata = call->inv->invite_tsx->last_tx;
    if (!tdata || tdata->msg->type != PJSIP_RESPONSE_MSG ||
	tdata->msg->line.status.code/10 != 18)
    {
	return;
    }

    /* Retransmit 18x */
    ++call->trickle_ice.retrans18x_count;
    PJ_LOG(4,(THIS_FILE,
	      "Call %d: ICE trickle retransmitting 18x (retrans #%d)",
	      call->index, call->trickle_ice.retrans18x_count));

    pjsip_tx_data_add_ref(tdata);
    pjsip_tsx_retransmit_no_state(call->inv->invite_tsx, tdata);

    /* Schedule next retransmission */
    if (call->trickle_ice.retrans18x_count < 6) {
	pj_uint32_t tmp;
	tmp = (1 << call->trickle_ice.retrans18x_count) * pjsip_cfg()->tsx.t1;
	delay.sec = 0;
	delay.msec = tmp;
	pj_time_val_normalize(&delay);
    } else {
	delay.sec = 1;
	delay.msec = 500;
    }
    pjsua_schedule_timer(te, &delay);
}


static void trickle_ice_recv_sip_info(pjsua_call *call, pjsip_rx_data *rdata)
{
    pjsip_media_type med_type;
    pjsip_rdata_sdp_info *sdp_info;
    pj_status_t status;
    unsigned i, j, med_cnt;
    pj_bool_t use_med_prov;

    pjsip_media_type_init2(&med_type, "application", "trickle-ice-sdpfrag");

    /* Parse the SDP */
    sdp_info = pjsip_rdata_get_sdp_info2(rdata, &med_type);
    if (!sdp_info->sdp) {
	pj_status_t err = sdp_info->body.ptr? sdp_info->sdp_err:PJ_ENOTFOUND;
	pjsua_perror(THIS_FILE, "Failed to parse trickle ICE SDP in "
				"incoming INFO", err);
	return;
    }

    PJSUA_LOCK();

    /* Retrieve the candidates from the SDP */
    use_med_prov = call->med_prov_cnt > call->med_cnt;
    med_cnt = use_med_prov? call->med_prov_cnt : call->med_cnt;
    for (i = 0; i < sdp_info->sdp->media_count; ++i) {
	pjmedia_transport *tp = NULL;
	pj_str_t mid, ufrag, pwd;
	unsigned cand_cnt = PJ_ICE_ST_MAX_CAND;
	pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
	pj_bool_t end_of_cand;

	status = pjmedia_ice_trickle_decode_sdp(sdp_info->sdp, i, &mid,
						&ufrag, &pwd,
						&cand_cnt, cand,
						&end_of_cand);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Failed to retrive ICE candidates from "
				    "SDP in incoming INFO", status);
	    continue;
	}

	for (j = 0; j < med_cnt; ++j) {
	    pjsua_call_media *cm = use_med_prov? &call->media_prov[j] :
						 &call->media[j];
	    tp = cm->tp_orig;

	    if (tp && tp->type == PJMEDIA_TRANSPORT_TYPE_ICE &&
		pj_strcmp(&cm->rem_mid, &mid) == 0)
	    {
		break;
	    }
	}

	if (j == med_cnt) {
	    pjsua_perror(THIS_FILE, "Cannot add remote candidates from SDP in "
			 "incoming INFO because media ID (SDP a=mid) is not "
			 "recognized",
			 PJ_EIGNORED);
	    continue;
	}

	/* Update ICE checklist */
	status = pjmedia_ice_trickle_update(tp, &ufrag, &pwd, cand_cnt, cand,
					    end_of_cand);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Failed to update ICE checklist from "
				    "incoming INFO", status);
	}
    }

    PJSUA_UNLOCK();
}


static void trickle_ice_send_sip_info(pj_timer_heap_t *th,
				      struct pj_timer_entry *te)
{
    pjsua_call *call = (pjsua_call*)te->user_data;
    pj_pool_t *tmp_pool = NULL;
    pj_bool_t all_end_of_cand, use_med_prov;
    pjmedia_sdp_session *sdp;
    unsigned i, med_cnt;
    pjsua_msg_data msg_data;
    pjsip_generic_string_hdr hdr1, hdr2;
    pj_status_t status = PJ_SUCCESS;
    pj_bool_t forced, need_send = PJ_FALSE;
    pj_sockaddr orig_addr;

    pj_str_t SIP_INFO		= {"INFO", 4};
    pj_str_t CONTENT_DISP_STR	= {"Content-Disposition", 19};
    pj_str_t INFO_PKG_STR	= {"Info-Package", 12};
    pj_str_t TRICKLE_ICE_STR	= {"trickle-ice", 11};

    PJ_UNUSED_ARG(th);

    PJSUA_LOCK();

    /* Check provisional media or active media to use */
    use_med_prov = call->med_prov_cnt > call->med_cnt;
    med_cnt = use_med_prov? call->med_prov_cnt : call->med_cnt;

    /* Check if any pending INFO already */
    if (call->trickle_ice.pending_info)
	goto on_return;

    /* Check if any new candidate, if not forced */
    forced = (te->id == 2);
    if (!forced) {
	for (i = 0; i < med_cnt; ++i) {
	    pjsua_call_media *cm = use_med_prov? &call->media_prov[i] :
						 &call->media[i];
	    pjmedia_transport *tp = cm->tp_orig;

	    if (!tp || tp->type != PJMEDIA_TRANSPORT_TYPE_ICE)
		continue;

	    if (pjmedia_ice_trickle_has_new_cand(tp))
		break;
	}

	/* No new local candidate */
	if (i == med_cnt)
	    goto on_return;
    }

    PJ_LOG(4,(THIS_FILE, "Call %d: ICE trickle sending SIP INFO%s",
	      call->index, (forced? " (forced)":"")));

    /* Create temporary pool */
    tmp_pool = pjsua_pool_create("tmp_ice", 128, 128);

    /* Create empty SDP */
    pj_sockaddr_init(pj_AF_INET(), &orig_addr, NULL, 0);
    status = pjmedia_endpt_create_base_sdp(pjsua_var.med_endpt, tmp_pool,
					   NULL, &orig_addr, &sdp);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Generate SDP for SIP INFO */
    all_end_of_cand = PJ_TRUE;
    for (i = 0; i < med_cnt; ++i) {
	pjsua_call_media *cm = use_med_prov? &call->media_prov[i] :
					     &call->media[i];
	pjmedia_transport *tp = cm->tp_orig;
	pj_bool_t end_of_cand = PJ_FALSE;

	if (!tp || tp->type != PJMEDIA_TRANSPORT_TYPE_ICE)
	    continue;

	status = pjmedia_ice_trickle_send_local_cand(tp, tmp_pool, sdp,
						     &end_of_cand);
	if (status != PJ_SUCCESS || !end_of_cand)
	    all_end_of_cand = PJ_FALSE;

	need_send |= (status==PJ_SUCCESS);
    }

    if (!need_send)
	goto on_return;

    /* Generate and send SIP INFO */
    pjsua_msg_data_init(&msg_data);

    pjsip_generic_string_hdr_init2(&hdr1, &INFO_PKG_STR, &TRICKLE_ICE_STR);
    pj_list_push_back(&msg_data.hdr_list, &hdr1);
    pjsip_generic_string_hdr_init2(&hdr2, &CONTENT_DISP_STR, &INFO_PKG_STR);
    pj_list_push_back(&msg_data.hdr_list, &hdr2);

    msg_data.content_type = pj_str("application/trickle-ice-sdpfrag");
    msg_data.msg_body.ptr = pj_pool_alloc(tmp_pool, PJSIP_MAX_PKT_LEN);
    msg_data.msg_body.slen = pjmedia_sdp_print(sdp, msg_data.msg_body.ptr,
					       PJSIP_MAX_PKT_LEN);
    if (msg_data.msg_body.slen == -1) {
	PJ_LOG(3,(THIS_FILE,
		  "Warning! Call %d: ICE trickle failed to print SDP for "
		  "SIP INFO due to insufficient buffer", call->index));
	goto on_return;
    }

    status = pjsua_call_send_request(call->index, &SIP_INFO, &msg_data);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Set flag for pending SIP INFO */
    call->trickle_ice.pending_info = PJ_TRUE;

    /* Stop trickling if local candidate gathering for all media is done */
    if (all_end_of_cand) {
	PJ_LOG(4,(THIS_FILE, "Call %d: ICE trickle stopped trickling "
			     "as local candidate gathering completed",
			     call->index));
	call->trickle_ice.trickling = PJSUA_OP_STATE_DONE;
    }

    /* Update ICE checklist after conveying local candidates. */
    for (i = 0; i < med_cnt; ++i) {
	pjsua_call_media *cm = use_med_prov? &call->media_prov[i] :
					     &call->media[i];
	pjmedia_transport *tp = cm->tp_orig;
	if (!tp || tp->type != PJMEDIA_TRANSPORT_TYPE_ICE)
	    continue;

	pjmedia_ice_trickle_update(tp, NULL, NULL, 0, NULL, PJ_FALSE);
    }

on_return:
    if (tmp_pool)
	pj_pool_release(tmp_pool);

    /* Reschedule if we are trickling */
    if (call->trickle_ice.trickling == PJSUA_OP_STATE_RUNNING) {
	pj_time_val delay = {0, PJSUA_TRICKLE_ICE_NEW_CAND_CHECK_INTERVAL};

	/* Reset forced mode after successfully sending forced SIP INFO */
	te->id = (status==PJ_SUCCESS? 0 : 2);

	pj_time_val_normalize(&delay);
	pjsua_schedule_timer(te, &delay);
    }

    PJSUA_UNLOCK();
}


/* Before sending INFO can be started, UA needs to confirm these:
 * 1. dialog is established (perhaps early) at both sides,
 * 2. trickle ICE is supported by peer.
 *
 * This function needs to be called when:
 * - UAS sending 18x, to start 18x retrans
 * - UAC receiving 18x, to forcefully send SIP INFO & start trickling
 * - UAS receiving INFO, to cease 18x retrans & start trickling
 * - UAS receiving PRACK, to start trickling
 * - UAC/UAS receiving remote SDP (and check for trickle ICE support),
 *   to start trickling.
 */
void pjsua_ice_check_start_trickling(pjsua_call *call,
				     pj_bool_t forceful,
				     pjsip_event *e)
{
    pjsip_inv_session *inv = call->inv;

    /* Make sure trickling/sending-INFO has not been started */
    if (!forceful && call->trickle_ice.trickling >= PJSUA_OP_STATE_RUNNING)
	return;

    /* Make sure trickle ICE is enabled */
    if (!call->trickle_ice.enabled)
	return;

    /* Make sure the dialog state is established */
    if (!inv || inv->dlg->state != PJSIP_DIALOG_STATE_ESTABLISHED)
	return;

    /* First, make sure remote dialog is also established. */
    if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
	/* Set flag indicating remote dialog is established */
	call->trickle_ice.remote_dlg_est = PJ_TRUE;
    } else if (inv->state > PJSIP_INV_STATE_CONFIRMED) {
	/* Call is terminating/terminated (just trying to be safe) */
	call->trickle_ice.remote_dlg_est = PJ_FALSE;
    } else if (!call->trickle_ice.remote_dlg_est && e) {
	/* Call is being initialized */
	pjsip_msg *msg = NULL;
	pjsip_rx_data *rdata = NULL;
	pjsip_tx_data *tdata = NULL;
	pj_bool_t has_100rel = (inv->options & PJSIP_INV_REQUIRE_100REL);
	pj_timer_entry *te = &call->trickle_ice.timer;

	if (e->type == PJSIP_EVENT_TSX_STATE &&
	    e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
	{
	    rdata = e->body.tsx_state.src.rdata;
	} else if (e->type == PJSIP_EVENT_TSX_STATE &&
		   e->body.tsx_state.type == PJSIP_EVENT_TX_MSG)
	{
	    tdata = e->body.tsx_state.src.tdata;
	} else {
	    return;
	}

	/* UAC must have received 18x at this point, so dialog must have been
	 * established at the remote side.
	 */
	if (inv->role == PJSIP_ROLE_UAC) {
	    /* UAC needs to send SIP INFO when receiving 18x and 100rel is not
	     * active.
	     * Note that 18x may not have SDP (so we don't know if remote
	     * supports trickle ICE), but we should send INFO anyway, as the
	     * draft allows start trickling without answer.
	     */
	    if (!has_100rel && rdata &&
		rdata->msg_info.msg->type == PJSIP_RESPONSE_MSG &&
		rdata->msg_info.msg->line.status.code/10 == 18)
	    {
		pjsip_rdata_sdp_info *sdp_info;
		sdp_info = pjsip_rdata_get_sdp_info(rdata);
		if (sdp_info->sdp) {
		    unsigned i;
		    for (i = 0; i < sdp_info->sdp->media_count; ++i) {
			if (pjmedia_ice_sdp_has_trickle(sdp_info->sdp, i)) {
			    call->trickle_ice.remote_sup = PJ_TRUE;
			    break;
			}
		    }
		} else {
		    /* Start sending SIP INFO forcefully */
		    forceful = PJ_TRUE;
		}

		if (forceful || call->trickle_ice.remote_sup) {
		    PJ_LOG(4,(THIS_FILE,
			      "Call %d: ICE trickle started after UAC "
			      "receiving 18x (with%s SDP)",
			      call->index, sdp_info->sdp?"":"out"));
		}
	    }
	}

	/* But if we are the UAS, we need to wait for SIP PRACK or INFO to
	 * confirm dialog state at remote. And while waiting, 18x needs to be
	 * retransmitted.
	 */
	else {

	    if (tdata && e->body.tsx_state.tsx == inv->invite_tsx &&
		call->trickle_ice.retrans18x_count == 0)
	    {
		/* Ignite 18x retransmission */
		msg = tdata->msg;
		if (msg->type == PJSIP_RESPONSE_MSG &&
		    msg->line.status.code/10 == 18)
		{
		    pj_time_val delay;
		    delay.sec = pjsip_cfg()->tsx.t1 / 1000;
		    delay.msec = pjsip_cfg()->tsx.t1 % 1000;
		    pj_assert(!pj_timer_entry_running(te));
		    te->cb = &trickle_ice_retrans_18x;
		    pjsua_schedule_timer(te, &delay);

		    PJ_LOG(4,(THIS_FILE,
			      "Call %d: ICE trickle start retransmitting 18x",
			      call->index));
		}
		return;
	    }

	    /* Check for incoming PRACK or INFO to stop 18x retransmission */
	    if (!rdata)
		return;

	    msg = rdata->msg_info.msg;
	    if (has_100rel) {
		/* With 100rel, has received PRACK? */
		if (msg->type != PJSIP_REQUEST_MSG ||
		    pjsip_method_cmp(&msg->line.req.method,
				     pjsip_get_prack_method()))
		{
		    return;
		}
	    } else {
		pj_str_t INFO_PKG_STR = {"Info-Package", 12};
		pjsip_generic_string_hdr *hdr;

		/* Without 100rel, has received INFO? */
		if (msg->type != PJSIP_REQUEST_MSG ||
		    pjsip_method_cmp(&msg->line.req.method,
				     &pjsip_info_method))
		{
		    return;
		}

		/* With Info-Package header containing 'trickle-ice' */
		hdr = (pjsip_generic_string_hdr*)
		      pjsip_msg_find_hdr_by_name(msg, &INFO_PKG_STR, NULL);
		if (!hdr || pj_strcmp2(&hdr->hvalue, "trickle-ice"))
		    return;

		/* Set the flag indicating remote supports trickle ICE */
		call->trickle_ice.remote_sup = PJ_TRUE;
	    }
	    PJ_LOG(4,(THIS_FILE,
		      "Call %d: ICE trickle stop retransmitting 18x after "
		      "receiving %s",
		      call->index, (has_100rel?"PRACK":"INFO")));
	}

	/* Set flag indicating remote dialog is established.
	 * Any 18x retransmission should be ceased automatically.
	 */
	call->trickle_ice.remote_dlg_est = PJ_TRUE;
    }

    /* Check if ICE trickling can be started */
    if (!forceful &&
	(!call->trickle_ice.remote_dlg_est || !call->trickle_ice.remote_sup))
    {
	return;
    }

    /* Let's start trickling (or sending SIP INFO) */
    if (forceful || call->trickle_ice.trickling < PJSUA_OP_STATE_RUNNING)
    {
	pj_timer_entry *te = &call->trickle_ice.timer;
	pj_time_val delay = {0,0};

	if (call->trickle_ice.trickling < PJSUA_OP_STATE_RUNNING)
	    call->trickle_ice.trickling = PJSUA_OP_STATE_RUNNING;

	pjsua_cancel_timer(te);
	te->id = forceful? 2 : 0;
	te->cb = &trickle_ice_send_sip_info;
	pjsua_schedule_timer(te, &delay);

	PJ_LOG(4,(THIS_FILE,
		  "Call %d: ICE trickle start trickling",
		  call->index));
    }
}


/*
 * This callback receives notification from invite session when the
 * session state has changed.
 */
static void pjsua_call_on_state_changed(pjsip_inv_session *inv,
					pjsip_event *e)
{
    pjsua_call *call;
    unsigned num_locks = 0;

    pj_log_push_indent();

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];

    if (!call) {
	pj_log_pop_indent();
	return;
    }


    /* Get call times */
    switch (inv->state) {
	case PJSIP_INV_STATE_EARLY:
	case PJSIP_INV_STATE_CONNECTING:
	    if (call->res_time.sec == 0)
		pj_gettimeofday(&call->res_time);
	    call->last_code = (pjsip_status_code)
	    		      e->body.tsx_state.tsx->status_code;
	    pj_strncpy(&call->last_text,
		       &e->body.tsx_state.tsx->status_text,
		       sizeof(call->last_text_buf_));
	    break;
	case PJSIP_INV_STATE_CONFIRMED:
	    if (call->hanging_up) {
	    	/* This can happen if there is a crossover between
	    	 * our CANCEL request and the remote's 200 response.
	    	 * So we send BYE here.
	    	 */
	    	call_inv_end_session(call, 200, NULL, NULL);
	    	return;
	    }
	    pj_gettimeofday(&call->conn_time);

	    if (call->trickle_ice.enabled) {
		call->trickle_ice.remote_dlg_est = PJ_TRUE;
		pjsua_ice_check_start_trickling(call, PJ_FALSE, NULL);
	    }

            /* See if auto reinvite was pended as media update was done in the
             * EARLY state and remote does not support UPDATE.
             */
            if (call->reinv_pending) {
		call->reinv_pending = PJ_FALSE;
		pjsua_call_schedule_reinvite_check(call, 0);
	    }
	    break;
	case PJSIP_INV_STATE_DISCONNECTED:
	    pj_gettimeofday(&call->dis_time);
	    if (call->res_time.sec == 0)
		pj_gettimeofday(&call->res_time);
	    if (e->type == PJSIP_EVENT_TSX_STATE &&
		e->body.tsx_state.tsx->status_code > call->last_code)
	    {
		call->last_code = (pjsip_status_code)
				  e->body.tsx_state.tsx->status_code;
		pj_strncpy(&call->last_text,
			   &e->body.tsx_state.tsx->status_text,
			   sizeof(call->last_text_buf_));
	    } else {
		call->last_code = PJSIP_SC_REQUEST_TERMINATED;
		pj_strncpy(&call->last_text,
			   pjsip_get_status_text(call->last_code),
			   sizeof(call->last_text_buf_));
	    }

	    /* Stop reinvite timer, if it is active */
	    if (call->reinv_timer.id) {
		pjsua_cancel_timer(&call->reinv_timer);
		call->reinv_timer.id = PJ_FALSE;
	    }
	    break;
	default:
	    call->last_code = (pjsip_status_code)
	    		      e->body.tsx_state.tsx->status_code;
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
	    if (call->inv->state == PJSIP_INV_STATE_CONNECTING)
		ev_state = PJSIP_EVSUB_STATE_TERMINATED;
	    else
		ev_state = PJSIP_EVSUB_STATE_ACTIVE;
	    break;

	case PJSIP_INV_STATE_CONFIRMED:
#if 0
/* We don't need this, as we've terminated the subscription in
 * CONNECTING state.
 */
	    /* When state is confirmed, send the final 200/OK and terminate
	     * subscription.
	     */
	    st_code = e->body.tsx_state.tsx->status_code;
	    ev_state = PJSIP_EVSUB_STATE_TERMINATED;
#endif
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

    /* Destroy media session when invite session is disconnected. */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
	PJSUA_LOCK();

	if (!call->hanging_up)
	    pjsua_media_channel_deinit(call->index);
	
	PJSUA_UNLOCK();
    }

    /* Release locks before calling callbacks, to avoid deadlock. */
    while (PJSUA_LOCK_IS_LOCKED()) {
    	num_locks++;
    	PJSUA_UNLOCK();
    }

    /* Ticket #1627: Invoke on_call_tsx_state() when call is disconnected. */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED &&
	e->type == PJSIP_EVENT_TSX_STATE &&
	!call->hanging_up && call->inv &&
	pjsua_var.ua_cfg.cb.on_call_tsx_state)
    {
	(*pjsua_var.ua_cfg.cb.on_call_tsx_state)(call->index,
						 e->body.tsx_state.tsx, e);
    }

    if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_call_state)
	(*pjsua_var.ua_cfg.cb.on_call_state)(call->index, e);

    /* Re-acquire the locks. */
    for (;num_locks > 0; num_locks--)
    	PJSUA_LOCK();

    /* call->inv may be NULL now */

    /* Finally, free call when invite session is disconnected. */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

	PJSUA_LOCK();

	/* Free call */
	call->inv = NULL;

	pj_assert(pjsua_var.call_cnt > 0);
	--pjsua_var.call_cnt;

	/* Reset call */
	reset_call(call->index);

	pjsua_check_snd_dev_idle();

	PJSUA_UNLOCK();
    }
    pj_log_pop_indent();
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
 * Callback from UA layer when forked dialog response is received.
 */
pjsip_dialog* on_dlg_forked(pjsip_dialog *dlg, pjsip_rx_data *res)
{
    if (dlg->uac_has_2xx &&
	res->msg_info.cseq->method.id == PJSIP_INVITE_METHOD &&
	pjsip_rdata_get_tsx(res) == NULL &&
	res->msg_info.msg->line.status.code/100 == 2)
    {
	pjsip_dialog *forked_dlg;
	pjsip_tx_data *bye;
	pj_status_t status;

	/* Create forked dialog */
	status = pjsip_dlg_fork(dlg, res, &forked_dlg);
	if (status != PJ_SUCCESS)
	    return NULL;

	pjsip_dlg_inc_lock(forked_dlg);

	/* Disconnect the call */
	status = pjsip_dlg_create_request(forked_dlg, pjsip_get_bye_method(),
					  -1, &bye);
	if (status == PJ_SUCCESS) {
	    status = pjsip_dlg_send_request(forked_dlg, bye, -1, NULL);
	}

	pjsip_dlg_dec_lock(forked_dlg);

	if (status != PJ_SUCCESS) {
	    return NULL;
	}

	return forked_dlg;

    } else {
	return dlg;
    }
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
    if (status != PJ_SUCCESS || !tdata)
	return;

#if DISABLED_FOR_TICKET_1185
    pjsua_call *call;

    /* Add SDP in 488 status */
    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];

    if (call && call->tp && tdata->msg->type==PJSIP_RESPONSE_MSG &&
	code==PJSIP_SC_NOT_ACCEPTABLE_HERE)
    {
	pjmedia_sdp_session *local_sdp;
	pjmedia_transport_info ti;

	pjmedia_transport_info_init(&ti);
	pjmedia_transport_get_info(call->med_tp, &ti);
	status = pjmedia_endpt_create_sdp(pjsua_var.med_endpt, tdata->pool,
					  1, &ti.sock_info, &local_sdp);
	if (status == PJ_SUCCESS) {
	    pjsip_create_sdp_body(tdata->pool, local_sdp,
				  &tdata->msg->body);
	}
    }
#endif

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
    pjsua_call *call;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;
    //const pj_str_t st_update = {"UPDATE", 6};

    pj_log_push_indent();

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];

    if (call->hanging_up)
        goto on_return;

    if (status != PJ_SUCCESS) {

	pjsua_perror(THIS_FILE, "SDP negotiation has failed", status);

	/* Revert back provisional media. */
	pjsua_media_prov_revert(call->index);

	/* Do not deinitialize media since this may be a re-INVITE or
	 * UPDATE (which in this case the media should not get affected
	 * by the failed re-INVITE/UPDATE). The media will be shutdown
	 * when call is disconnected anyway.
	 */
	/* Stop/destroy media, if any */
	/*pjsua_media_channel_deinit(call->index);*/

	/* Disconnect call if we're not in the middle of initializing an
	 * UAS dialog and if this is not a re-INVITE
	 */
	if (inv->state != PJSIP_INV_STATE_NULL &&
	    inv->state != PJSIP_INV_STATE_CONFIRMED)
	{
	    call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	}

	goto on_return;
    }


    /* Get local and remote SDP */
    status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &local_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE,
		     "Unable to retrieve currently active local SDP",
		     status);
	//call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	goto on_return;
    }

    status = pjmedia_sdp_neg_get_active_remote(call->inv->neg, &remote_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE,
		     "Unable to retrieve currently active remote SDP",
		     status);
	//call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	goto on_return;
    }

    call->med_update_success = (status == PJ_SUCCESS);

    /* Trickle ICE tasks:
     * - Check remote SDP for trickle ICE support & start sending SIP INFO.
     */
    {
	unsigned i;
	for (i = 0; i < remote_sdp->media_count; ++i) {
	    if (pjmedia_ice_sdp_has_trickle(remote_sdp, i))
		break;
	}
	call->trickle_ice.remote_sup = (i < remote_sdp->media_count);
	if (call->trickle_ice.remote_sup)
	    pjsua_ice_check_start_trickling(call, PJ_FALSE, NULL);
    }

    /* Update remote's NAT type */
    if (pjsua_var.ua_cfg.nat_type_in_sdp) {
	update_remote_nat_type(call, remote_sdp);
    }

    /* Update media channel with the new SDP */
    status = pjsua_media_channel_update(call->index, local_sdp, remote_sdp);

    /* If this is not the initial INVITE, don't disconnect call due to
     * no media after SDP negotiation.
     */
    if (status == PJMEDIA_SDPNEG_ENOMEDIA &&
	call->inv->state == PJSIP_INV_STATE_CONFIRMED)
    {
	status = PJ_SUCCESS;
    }

    /* Disconnect call after failure in media channel update */
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create media session",
		     status);
	call_disconnect(inv, PJSIP_SC_NOT_ACCEPTABLE_HERE);
	/* No need to deinitialize; media will be shutdown when call
	 * state is disconnected anyway.
	 */
	/*pjsua_media_channel_deinit(call->index);*/
	goto on_return;
    }

    /* Ticket #476: make sure only one codec is specified in the answer. */
    pjsua_call_schedule_reinvite_check(call, 0);

    /* Call application callback, if any */
    if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_call_media_state)
	pjsua_var.ua_cfg.cb.on_call_media_state(call->index);

on_return:
    pj_log_pop_indent();
}


/* Modify SDP for call hold. */
static pj_status_t modify_sdp_of_call_hold(pjsua_call *call,
					   pj_pool_t *pool,
					   pjmedia_sdp_session *sdp,
					   pj_bool_t as_answerer)
{
    unsigned mi;

    /* Call-hold is done by set the media direction to 'sendonly'
     * (PJMEDIA_DIR_ENCODING), except when current media direction is
     * 'inactive' (PJMEDIA_DIR_NONE).
     * (See RFC 3264 Section 8.4 and RFC 4317 Section 3.1)
     */
    /* http://trac.pjsip.org/repos/ticket/880
       if (call->dir != PJMEDIA_DIR_ENCODING) {
     */
    /* https://trac.pjsip.org/repos/ticket/1142:
     *  configuration to use c=0.0.0.0 for call hold.
     */

    for (mi=0; mi<sdp->media_count; ++mi) {
	pjmedia_sdp_media *m = sdp->media[mi];

	if (call->call_hold_type == PJSUA_CALL_HOLD_TYPE_RFC2543) {
	    pjmedia_sdp_conn *conn;
	    pjmedia_sdp_attr *attr;

	    /* Get SDP media connection line */
	    conn = m->conn;
	    if (!conn)
		conn = sdp->conn;

	    /* Modify address */
	    conn->addr = pj_str("0.0.0.0");

	    /* Remove existing directions attributes */
	    pjmedia_sdp_media_remove_all_attr(m, "sendrecv");
	    pjmedia_sdp_media_remove_all_attr(m, "sendonly");
	    pjmedia_sdp_media_remove_all_attr(m, "recvonly");
	    pjmedia_sdp_media_remove_all_attr(m, "inactive");

	    /* Add inactive attribute */
	    attr = pjmedia_sdp_attr_create(pool, "inactive", NULL);
	    pjmedia_sdp_media_add_attr(m, attr);


	} else {
	    pjmedia_sdp_attr *attr;

	    /* Remove existing directions attributes */
	    pjmedia_sdp_media_remove_all_attr(m, "sendrecv");
	    pjmedia_sdp_media_remove_all_attr(m, "sendonly");
	    pjmedia_sdp_media_remove_all_attr(m, "recvonly");
	    pjmedia_sdp_media_remove_all_attr(m, "inactive");

	    /* When as answerer, just simply set dir to "sendonly", note that
	     * if the offer uses "sendonly" or "inactive", the SDP negotiator
	     * will change our answer dir to "inactive".
	     */
	    if (as_answerer || (call->media[mi].dir & PJMEDIA_DIR_ENCODING)) {
		/* Add sendonly attribute */
		attr = pjmedia_sdp_attr_create(pool, "sendonly", NULL);
		pjmedia_sdp_media_add_attr(m, attr);
	    } else {
		/* Add inactive attribute */
		attr = pjmedia_sdp_attr_create(pool, "inactive", NULL);
		pjmedia_sdp_media_add_attr(m, attr);
	    }
	}
    }

    return PJ_SUCCESS;
}

/* Create SDP for call hold. */
static pj_status_t create_sdp_of_call_hold(pjsua_call *call,
					   pjmedia_sdp_session **p_sdp)
{
    pj_status_t status;
    pj_pool_t *pool;
    pjmedia_sdp_session *sdp;

    /* Use call's provisional pool */
    pool = call->inv->pool_prov;

    /* Create new offer */
    status = pjsua_media_channel_create_sdp(call->index, pool, NULL, &sdp,
					    NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	return status;
    }

    status = modify_sdp_of_call_hold(call, pool, sdp, PJ_FALSE);
    if (status != PJ_SUCCESS)
	return status;

    *p_sdp = sdp;

    return PJ_SUCCESS;
}

/*
 * Called when session received new offer.
 */
static void pjsua_call_on_rx_offer(pjsip_inv_session *inv,
				struct pjsip_inv_on_rx_offer_cb_param *param)
{
    pjsua_call *call;
    pjmedia_sdp_session *answer;
    unsigned i;
    pj_status_t status;
    const pjmedia_sdp_session *offer = param->offer;
    pjsua_call_setting opt;
    pj_bool_t async = PJ_FALSE;

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];
    if (call->hanging_up)
     	return;

    /* Supply candidate answer */
    PJ_LOG(4,(THIS_FILE, "Call %d: received updated media offer",
	      call->index));

    pj_log_push_indent();

    if (pjsua_call_media_is_changing(call)) {
	PJ_LOG(1,(THIS_FILE, "Unable to process offer" ERR_MEDIA_CHANGING));
	goto on_return;
    }

    pjsua_call_cleanup_flag(&call->opt);
    opt = call->opt;

    if (pjsua_var.ua_cfg.cb.on_call_rx_reinvite &&
        param->rdata->msg_info.msg->type == PJSIP_REQUEST_MSG &&
        param->rdata->msg_info.msg->line.req.method.id == PJSIP_INVITE_METHOD)
    {
        pjsip_status_code code = PJSIP_SC_OK;

    	/* If on_call_rx_reinvite() callback is implemented,
    	 * call it first.
    	 */
	(*pjsua_var.ua_cfg.cb.on_call_rx_reinvite)(
						call->index, offer,
						(pjsip_rx_data *)param->rdata,
						NULL, &async, &code, &opt);
	if (async) {
    	    pjsip_tx_data *response;

    	    status = pjsip_inv_initial_answer(inv,
    	    				      (pjsip_rx_data *)param->rdata,
				      	      100, NULL, NULL, &response);
    	    if (status != PJ_SUCCESS) {
		PJ_PERROR(3, (THIS_FILE, status,
			      "Failed to create initial answer"));
    	    	goto on_return;
    	    }

	    status = pjsip_inv_send_msg(inv, response);
    	    if (status != PJ_SUCCESS) {
		PJ_PERROR(3, (THIS_FILE, status,
			      "Failed to send initial answer")); 
    	    	goto on_return;
    	    }

	    PJ_LOG(4,(THIS_FILE, "App will manually answer the re-INVITE "
	    			 "on call %d", call->index));
	}
	if (code != PJSIP_SC_OK) {
	    PJ_LOG(4,(THIS_FILE, "Rejecting re-INVITE updated media offer "
	    			 "on call %d", call->index));
	    goto on_return;
	}

	call->opt = opt;
    }

    if (pjsua_var.ua_cfg.cb.on_call_rx_offer && !async) {
	pjsip_status_code code = PJSIP_SC_OK;

	(*pjsua_var.ua_cfg.cb.on_call_rx_offer)(call->index, offer, NULL,
						&code, &opt);

	if (code != PJSIP_SC_OK) {
	    PJ_LOG(4,(THIS_FILE, "Rejecting updated media offer on call %d",
		      call->index));
	    goto on_return;
	}

	call->opt = opt;
    }

    /* Re-init media for the new remote offer before creating SDP */
    status = apply_call_setting(call, &call->opt, offer);
    if (status != PJ_SUCCESS)
	goto on_return;

    status = pjsua_media_channel_create_sdp(call->index,
					    call->inv->pool_prov,
					    offer, &answer, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	goto on_return;
    }

    if (async) {
	call->rx_reinv_async = async;
	goto on_return;
    }

    /* Validate media count in the generated answer */
    pj_assert(answer->media_count == offer->media_count);

    /* Check if offer's conn address is zero */
    for (i = 0; i < answer->media_count; ++i) {
	pjmedia_sdp_conn *conn;

	conn = offer->media[i]->conn;
	if (!conn)
	    conn = offer->conn;

	if (pj_strcmp2(&conn->addr, "0.0.0.0")==0 ||
	    pj_strcmp2(&conn->addr, "0")==0)
	{
	    pjmedia_sdp_conn *a_conn = answer->media[i]->conn;

	    /* Modify answer address */
	    if (a_conn) {
		a_conn->addr = pj_str("0.0.0.0");
	    } else if (answer->conn == NULL ||
		       pj_strcmp2(&answer->conn->addr, "0.0.0.0") != 0)
	    {
		a_conn = PJ_POOL_ZALLOC_T(call->inv->pool_prov,
					  pjmedia_sdp_conn);
		a_conn->net_type = pj_str("IN");
		a_conn->addr_type = pj_str("IP4");
		a_conn->addr = pj_str("0.0.0.0");
		answer->media[i]->conn = a_conn;
	    }
	}
    }

    /* Check if call is on-hold */
    if (call->local_hold) {
	modify_sdp_of_call_hold(call, call->inv->pool_prov, answer, PJ_TRUE);
    }

    status = pjsip_inv_set_sdp_answer(call->inv, answer);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to set answer", status);
	goto on_return;
    }

on_return:
    pj_log_pop_indent();
}


/*
 * Called when receiving re-INVITE.
 */
static pj_status_t pjsua_call_on_rx_reinvite(pjsip_inv_session *inv,
    		                  	     const pjmedia_sdp_session *offer,
                                  	     pjsip_rx_data *rdata)
{
    pjsua_call *call;
    pj_bool_t async;

    PJ_UNUSED_ARG(offer);
    PJ_UNUSED_ARG(rdata);

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];
    async = call->rx_reinv_async;
    call->rx_reinv_async = PJ_FALSE;

    return (async? PJ_SUCCESS: !PJ_SUCCESS);
}


/*
 * Called to generate new offer.
 */
static void pjsua_call_on_create_offer(pjsip_inv_session *inv,
				       pjmedia_sdp_session **offer)
{
    pjsua_call *call;
    pj_status_t status;
    unsigned mi;

    pj_log_push_indent();

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];
    if (call->hanging_up || pjsua_call_media_is_changing(call)) {
	*offer = NULL;
	PJ_LOG(1,(THIS_FILE, "Unable to create offer%s",
 		  call->hanging_up? ", call hanging up":
 		  ERR_MEDIA_CHANGING));
	goto on_return;
    }
    
#if RESTART_ICE_ON_REINVITE

    /* Ticket #1783, RFC 5245 section 12.5:
     * If an agent receives a mid-dialog re-INVITE that contains no offer,
     * it MUST restart ICE for each media stream and go through the process
     * of gathering new candidates.
     */
    for (mi=0; mi<call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];
	pjmedia_transport_info tpinfo;
	pjmedia_ice_transport_info *ice_info;

        /* Check if the media is using ICE */
        pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call_med->tp, &tpinfo);
	ice_info = (pjmedia_ice_transport_info*)
                    pjmedia_transport_info_get_spc_info(
                        &tpinfo, PJMEDIA_TRANSPORT_TYPE_ICE);
        if (!ice_info)
	    continue;

        /* Stop and re-init ICE stream transport.
         * According to RFC 5245 section 9.1.1.1, during ICE restart,
         * media can continue to be sent to the previously validated pair.
         */
        pjmedia_transport_media_stop(call_med->tp);
        pjmedia_transport_media_create(call_med->tp, call->inv->pool_prov,
                                       (call_med->enable_rtcp_mux?
            			    	PJMEDIA_TPMED_RTCP_MUX: 0),
            			       NULL, mi);

        PJ_LOG(4, (THIS_FILE, "Restarting ICE for media %d", mi));
    }
#endif

    pjsua_call_cleanup_flag(&call->opt);

    if (pjsua_var.ua_cfg.cb.on_call_tx_offer) {
	(*pjsua_var.ua_cfg.cb.on_call_tx_offer)(call->index, NULL,
						&call->opt);
    }

    /* We may need to re-initialize media before creating SDP */
    if (call->med_prov_cnt == 0 || pjsua_var.ua_cfg.cb.on_call_tx_offer) {
    	status = apply_call_setting(call, &call->opt, NULL);
    	if (status != PJ_SUCCESS)
	    goto on_return;
    }

    /* See if we've put call on hold. */
    if (call->local_hold) {
	PJ_LOG(4,(THIS_FILE,
		  "Call %d: call is on-hold locally, creating call-hold SDP ",
		  call->index));
	status = create_sdp_of_call_hold( call, offer );
    } else {
	PJ_LOG(4,(THIS_FILE, "Call %d: asked to send a new offer",
		  call->index));

	status = pjsua_media_channel_create_sdp(call->index,
						call->inv->pool_prov,
					        NULL, offer, NULL);
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	goto on_return;
    }

on_return:
    pj_log_pop_indent();
}


/*
 * Callback called by event framework when the xfer subscription state
 * has changed.
 */
static void xfer_client_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{

    PJ_UNUSED_ARG(event);

    pj_log_push_indent();

    /*
     * When subscription is accepted (got 200/OK to REFER), check if
     * subscription suppressed.
     */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACCEPTED) {

	pjsip_rx_data *rdata;
	pjsip_generic_string_hdr *refer_sub;
	const pj_str_t REFER_SUB = { "Refer-Sub", 9 };
	pjsua_call *call;

	call = (pjsua_call*) pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);

	/* Must be receipt of response message */
	pj_assert(event->type == PJSIP_EVENT_TSX_STATE &&
		  event->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
	rdata = event->body.tsx_state.src.rdata;

	/* Find Refer-Sub header */
	refer_sub = (pjsip_generic_string_hdr*)
		    pjsip_msg_find_hdr_by_name(rdata->msg_info.msg,
					       &REFER_SUB, NULL);

	/* Check if subscription is suppressed */
	if (refer_sub && pj_stricmp2(&refer_sub->hvalue, "false")==0) {
	    /* Since no subscription is desired, assume that call has been
	     * transferred successfully.
	     */
	    if (call && !call->hanging_up &&
	        pjsua_var.ua_cfg.cb.on_call_transfer_status)
	    {
		const pj_str_t ACCEPTED = { "Accepted", 8 };
		pj_bool_t cont = PJ_FALSE;
		(*pjsua_var.ua_cfg.cb.on_call_transfer_status)(call->index,
							       200,
							       &ACCEPTED,
							       PJ_TRUE,
							       &cont);
	    }

	    /* Yes, subscription is suppressed.
	     * Terminate our subscription now.
	     */
	    PJ_LOG(4,(THIS_FILE, "Xfer subscription suppressed, terminating "
				 "event subcription..."));
	    pjsip_evsub_terminate(sub, PJ_TRUE);

	} else {
	    /* Notify application about call transfer progress.
	     * Initially notify with 100/Accepted status.
	     */
	    if (call && !call->hanging_up &&
	        pjsua_var.ua_cfg.cb.on_call_transfer_status)
	    {
		const pj_str_t ACCEPTED = { "Accepted", 8 };
		pj_bool_t cont = PJ_FALSE;
		(*pjsua_var.ua_cfg.cb.on_call_transfer_status)(call->index,
							       100,
							       &ACCEPTED,
							       PJ_FALSE,
							       &cont);
	    }
	}
    }
    /*
     * On incoming NOTIFY or an error response, notify application about call 
     * transfer progress.
     */
    else if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACTIVE ||
	     pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED)
    {
	pjsua_call *call;
	pjsip_msg *msg;
	pjsip_msg_body *body;
	pjsip_status_line status_line;
	pj_bool_t is_last;
	pj_bool_t cont;
	pj_status_t status;

	call = (pjsua_call*) pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);

	/* When subscription is terminated, clear the xfer_sub member of
	 * the inv_data.
	 */
	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	    pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
	    PJ_LOG(4,(THIS_FILE, "Xfer client subscription terminated"));

	}

	if (!call || call->hanging_up || !event ||
	    !pjsua_var.ua_cfg.cb.on_call_transfer_status)
	{
	    /* Application is not interested with call progress status */
	    goto on_return;
	}
	
	if (event->type == PJSIP_EVENT_TSX_STATE &&
	    event->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
	{
	    pjsip_rx_data *rdata;

	    rdata = event->body.tsx_state.src.rdata;
	    msg = rdata->msg_info.msg;

	    /* This better be a NOTIFY request */
	    if (pjsip_method_cmp(&msg->line.req.method, 
				 pjsip_get_notify_method()) == 0) 
	    {
		/* Check if there's body */
		body = msg->body;
		if (!body) {
		    PJ_LOG(2, (THIS_FILE,
			       "Warning: received NOTIFY without message "
			       "body"));
		    goto on_return;
		}

		/* Check for appropriate content */
		if (pj_stricmp2(&body->content_type.type, "message") != 0 ||
		    pj_stricmp2(&body->content_type.subtype, "sipfrag") != 0)
		{
		    PJ_LOG(2, (THIS_FILE,
			       "Warning: received NOTIFY with non "
			       "message/sipfrag content"));
		    goto on_return;
		}

		/* Try to parse the content */
		status = pjsip_parse_status_line((char*)body->data, body->len,
						 &status_line);
		if (status != PJ_SUCCESS) {
		    PJ_LOG(2, (THIS_FILE,
			       "Warning: received NOTIFY with invalid "
			       "message/sipfrag content"));
		    goto on_return;
		}
	    } else {
		status_line.code = msg->line.status.code;
		status_line.reason = msg->line.status.reason;
	    }
	} else {
	    status_line.code = 500;
	    status_line.reason = *pjsip_get_status_text(500);
	}

	/* Notify application */
	is_last = (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED);
	cont = !is_last;
	(*pjsua_var.ua_cfg.cb.on_call_transfer_status)(call->index,
						       status_line.code,
						       &status_line.reason,
						       is_last, &cont);

	if (!cont) {
	    pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
	}

	/* If the call transfer has completed but the subscription is
	 * not terminated, terminate it now.
	 */
	if (status_line.code/100 == 2 && !is_last) {
	    pjsip_tx_data *tdata;

	    status = pjsip_evsub_initiate(sub, pjsip_get_subscribe_method(),
					  0, &tdata);
	    if (status == PJ_SUCCESS)
		status = pjsip_evsub_send_request(sub, tdata);
	}
    }

on_return:
    pj_log_pop_indent();
}


/*
 * Callback called by event framework when the xfer subscription state
 * has changed.
 */
static void xfer_server_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
    PJ_UNUSED_ARG(event);

    pj_log_push_indent();

    /*
     * When subscription is terminated, clear the xfer_sub member of
     * the inv_data.
     */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	pjsua_call *call;

	call = (pjsua_call*) pjsip_evsub_get_mod_data(sub, pjsua_var.mod.id);
	if (!call)
	    goto on_return;

	pjsip_evsub_set_mod_data(sub, pjsua_var.mod.id, NULL);
	call->xfer_sub = NULL;

	PJ_LOG(4,(THIS_FILE, "Xfer server subscription terminated"));
    }

on_return:
    pj_log_pop_indent();
}


/*
 * Follow transfer (REFER) request.
 */
static void on_call_transferred( pjsip_inv_session *inv,
			        pjsip_rx_data *rdata )
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjsua_call *existing_call;
    int new_call;
    const pj_str_t str_refer_to = { "Refer-To", 8};
    const pj_str_t str_refer_sub = { "Refer-Sub", 9 };
    const pj_str_t str_ref_by = { "Referred-By", 11 };
    pjsip_generic_string_hdr *refer_to;
    pjsip_generic_string_hdr *refer_sub;
    pjsip_hdr *ref_by_hdr;
    pj_bool_t no_refer_sub = PJ_FALSE;
    char *uri;
    pjsua_msg_data msg_data;
    pj_str_t tmp;
    pjsip_status_code code;
    pjsip_evsub *sub;
    pjsua_call_setting call_opt;

    pj_log_push_indent();

    existing_call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];
    if (existing_call->hanging_up) {
	pjsip_dlg_respond( inv->dlg, rdata, 487, NULL, NULL, NULL);
    	goto on_return;
    }

    /* Find the Refer-To header */
    refer_to = (pjsip_generic_string_hdr*)
	pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_to, NULL);

    if (refer_to == NULL) {
	/* Invalid Request.
	 * No Refer-To header!
	 */
	PJ_LOG(4,(THIS_FILE, "Received REFER without Refer-To header!"));
	pjsip_dlg_respond( inv->dlg, rdata, 400, NULL, NULL, NULL);
	goto on_return;
    }

    /* Find optional Refer-Sub header */
    refer_sub = (pjsip_generic_string_hdr*)
	pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_sub, NULL);

    if (refer_sub) {
	if (pj_strnicmp2(&refer_sub->hvalue, "true", 4)!=0)
	    no_refer_sub = PJ_TRUE;
    }

    /* Find optional Referred-By header (to be copied onto outgoing INVITE
     * request.
     */
    ref_by_hdr = (pjsip_hdr*)
		 pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_ref_by,
					    NULL);

    /* Notify callback */
    code = PJSIP_SC_ACCEPTED;
    if (pjsua_var.ua_cfg.cb.on_call_transfer_request) {
	(*pjsua_var.ua_cfg.cb.on_call_transfer_request)(existing_call->index,
							&refer_to->hvalue,
							&code);
    }

    pjsua_call_cleanup_flag(&existing_call->opt);
    call_opt = existing_call->opt;
    if (pjsua_var.ua_cfg.cb.on_call_transfer_request2) {
	(*pjsua_var.ua_cfg.cb.on_call_transfer_request2)(existing_call->index,
							 &refer_to->hvalue,
							 &code,
							 &call_opt);
    }

    if (code < 200)
	code = PJSIP_SC_ACCEPTED;
    if (code >= 300) {
	/* Application rejects call transfer request */
	pjsip_dlg_respond( inv->dlg, rdata, code, NULL, NULL, NULL);
	goto on_return;
    }

    PJ_LOG(3,(THIS_FILE, "Call to %.*s is being transferred to %.*s",
	      (int)inv->dlg->remote.info_str.slen,
	      inv->dlg->remote.info_str.ptr,
	      (int)refer_to->hvalue.slen,
	      refer_to->hvalue.ptr));

    if (no_refer_sub) {
	/*
	 * Always answer with 2xx.
	 */
	pjsip_tx_data *tdata2;
	const pj_str_t str_false = { "false", 5};
	pjsip_hdr *hdr;

	status = pjsip_dlg_create_response(inv->dlg, rdata, code, NULL,
					   &tdata2);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create 2xx response to REFER",
			 status);
	    goto on_return;
	}

	/* Add Refer-Sub header */
	hdr = (pjsip_hdr*)
	       pjsip_generic_string_hdr_create(tdata2->pool, &str_refer_sub,
					      &str_false);
	pjsip_msg_add_hdr(tdata2->msg, hdr);


	/* Send answer */
	status = pjsip_dlg_send_response(inv->dlg, pjsip_rdata_get_tsx(rdata),
					 tdata2);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create 2xx response to REFER",
			 status);
	    goto on_return;
	}

	/* Don't have subscription */
	sub = NULL;

    } else {
	struct pjsip_evsub_user xfer_cb;
	pjsip_hdr hdr_list;

	/* Init callback */
	pj_bzero(&xfer_cb, sizeof(xfer_cb));
	xfer_cb.on_evsub_state = &xfer_server_on_evsub_state;

	/* Init additional header list to be sent with REFER response */
	pj_list_init(&hdr_list);

	/* Create transferee event subscription */
	status = pjsip_xfer_create_uas( inv->dlg, &xfer_cb, rdata, &sub);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create xfer uas", status);
	    pjsip_dlg_respond( inv->dlg, rdata, 500, NULL, NULL, NULL);
	    goto on_return;
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

	/* Accept the REFER request, send 2xx. */
	pjsip_xfer_accept(sub, rdata, code, &hdr_list);

	/* Create initial NOTIFY request */
	status = pjsip_xfer_notify( sub, PJSIP_EVSUB_STATE_ACTIVE,
				    100, NULL, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create NOTIFY to REFER",
			 status);
	    goto on_return;
	}

	/* Send initial NOTIFY request */
	status = pjsip_xfer_send_request( sub, tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to send NOTIFY to REFER", status);
	    goto on_return;
	}
    }

    /* We're cheating here.
     * We need to get a null terminated string from a pj_str_t.
     * So grab the pointer from the hvalue and NULL terminate it, knowing
     * that the NULL position will be occupied by a newline.
     */
    uri = refer_to->hvalue.ptr;
    uri[refer_to->hvalue.slen] = '\0';

    /* Init msg_data */
    pjsua_msg_data_init(&msg_data);

    /* If Referred-By header is present in the REFER request, copy this
     * to the outgoing INVITE request.
     */
    if (ref_by_hdr != NULL) {
	pjsip_hdr *dup = (pjsip_hdr*)
			 pjsip_hdr_clone(rdata->tp_info.pool, ref_by_hdr);
	pj_list_push_back(&msg_data.hdr_list, dup);
    }

    /* Now make the outgoing call. */
    tmp = pj_str(uri);
    status = pjsua_call_make_call(existing_call->acc_id, &tmp, &call_opt,
				  existing_call->user_data, &msg_data,
				  &new_call);
    if (status != PJ_SUCCESS) {

	/* Notify xferer about the error (if we have subscription) */
	if (sub) {
	    status = pjsip_xfer_notify(sub, PJSIP_EVSUB_STATE_TERMINATED,
				       500, NULL, &tdata);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to create NOTIFY to REFER",
			      status);
		goto on_return;
	    }
	    status = pjsip_xfer_send_request(sub, tdata);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to send NOTIFY to REFER",
			      status);
		goto on_return;
	    }
	}
	goto on_return;
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

on_return:
    pj_log_pop_indent();
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
    /* Incoming INFO request for media control, DTMF, trickle ICE, etc. */
    const pj_str_t STR_APPLICATION	 = { "application", 11};
    const pj_str_t STR_MEDIA_CONTROL_XML = { "media_control+xml", 17 };
    const pj_str_t STR_DTMF_RELAY	 = { "dtmf-relay", 10 };
    const pj_str_t STR_TRICKLE_ICE_SDP	 = { "trickle-ice-sdpfrag", 19 };

    pjsua_call *call;

    pj_log_push_indent();

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];

    if (call == NULL)
	goto on_return;

    if (call->inv == NULL || call->hanging_up) {
	/* Call has been disconnected. */
	goto on_return;
    }

    /* https://trac.pjsip.org/repos/ticket/1452:
     *    If a request is retried due to 401/407 challenge, don't process the
     *    transaction first but wait until we've retried it.
     */
    if (tsx->role == PJSIP_ROLE_UAC &&
	(tsx->status_code==401 || tsx->status_code==407) &&
	tsx->last_tx && tsx->last_tx->auth_retry)
    {
	goto on_return;
    }

    /* Notify application callback first */
    if (pjsua_var.ua_cfg.cb.on_call_tsx_state) {
	(*pjsua_var.ua_cfg.cb.on_call_tsx_state)(call->index, tsx, e);
    }

    if (tsx->role==PJSIP_ROLE_UAS &&
	tsx->state==PJSIP_TSX_STATE_TRYING &&
	pjsip_method_cmp(&tsx->method, pjsip_get_refer_method())==0)
    {
	/*
	 * Incoming REFER request.
	 */
	on_call_transferred(call->inv, e->body.tsx_state.src.rdata);

    }
    else if (tsx->role==PJSIP_ROLE_UAS &&
	     tsx->state==PJSIP_TSX_STATE_TRYING &&
	     pjsip_method_cmp(&tsx->method, &pjsip_message_method)==0)
    {
	/*
	 * Incoming MESSAGE request!
	 */
	pjsip_rx_data *rdata;
	pjsip_accept_hdr *accept_hdr;

	rdata = e->body.tsx_state.src.rdata;

	/* Request MUST have message body, with Content-Type equal to
	 * "text/plain".
	 */
	if (pjsua_im_accept_pager(rdata, &accept_hdr) == PJ_FALSE) {

	    pjsip_hdr hdr_list;

	    pj_list_init(&hdr_list);
	    pj_list_push_back(&hdr_list, accept_hdr);

	    pjsip_dlg_respond( inv->dlg, rdata, PJSIP_SC_NOT_ACCEPTABLE_HERE,
			       NULL, &hdr_list, NULL );
	    goto on_return;
	}

	/* Respond with 200 first, so that remote doesn't retransmit in case
	 * the UI takes too long to process the message.
	 */
	pjsip_dlg_respond( inv->dlg, rdata, 200, NULL, NULL, NULL);

	/* Process MESSAGE request */
	pjsua_im_process_pager(call->index, &inv->dlg->remote.info_str,
			       &inv->dlg->local.info_str, rdata);

    }
    else if (e->type == PJSIP_EVENT_TSX_STATE &&
            tsx->role == PJSIP_ROLE_UAC &&
            pjsip_method_cmp(&tsx->method, &pjsip_message_method)==0 &&
            (tsx->state == PJSIP_TSX_STATE_COMPLETED ||
            (tsx->state == PJSIP_TSX_STATE_TERMINATED &&
            e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED)))
    {
        /* Handle outgoing pager status */
        if (tsx->status_code >= 200) {
            pjsua_im_data *im_data;

            im_data = (pjsua_im_data*) tsx->mod_data[pjsua_var.mod.id];
            /* im_data can be NULL if this is typing indication */

            if (im_data) {
                pj_str_t im_body = im_data->body;
                if (im_body.slen==0) {
                    pjsip_msg_body *body = tsx->last_tx->msg->body;
                    pj_strset(&im_body, body->data, body->len);
                }

                if (pjsua_var.ua_cfg.cb.on_pager_status) {
                        pjsua_var.ua_cfg.cb.on_pager_status(im_data->call_id,
                                                            &im_data->to,
                                                            &im_body,
                                                            im_data->user_data,
                                                            (pjsip_status_code)
                                                            tsx->status_code,
                                                            &tsx->status_text);
                }

                if (pjsua_var.ua_cfg.cb.on_pager_status2) {
                    pjsip_rx_data* rdata;

                    if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
                    rdata = e->body.tsx_state.src.rdata;
                    else
                    rdata = NULL;

                    pjsua_var.ua_cfg.cb.on_pager_status2(im_data->call_id,
                                                        &im_data->to,
                                                        &im_body,
                                                        im_data->user_data,
                                                        (pjsip_status_code)
                                                            tsx->status_code,
                                                        &tsx->status_text,
                                                        tsx->last_tx,
                                                        rdata, im_data->acc_id);
                }
            }
        }
    } else if (tsx->role == PJSIP_ROLE_UAC &&
               pjsip_method_cmp(&tsx->method, pjsip_get_invite_method())==0 &&
               tsx->state >= PJSIP_TSX_STATE_COMPLETED &&
	       e->body.tsx_state.prev_state < PJSIP_TSX_STATE_COMPLETED &&
               (!PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 300) &&
                tsx->status_code!=401 && tsx->status_code!=407 &&
                tsx->status_code!=422))
    {
        if (tsx->status_code/100 == 2) {
            /* If we have sent CANCEL and the original INVITE returns a 2xx,
             * we then send BYE.
             */
            if (call->hanging_up) {
                PJ_LOG(3,(THIS_FILE, "Unsuccessful in cancelling the original "
	        	  "INVITE for call %d due to %d response, sending BYE "
	        	  "instead", call->index, tsx->status_code));
		call_disconnect(call->inv, PJSIP_SC_OK);
            }
        } else {
            /* Monitor the status of call hold/unhold request */
            if (tsx->last_tx == (pjsip_tx_data*)call->hold_msg) {
	        /* Outgoing call hold failed */
	        call->local_hold = PJ_FALSE;
	        PJ_LOG(3,(THIS_FILE, "Error putting call %d on hold "
	        	  "(reason=%d)", call->index, tsx->status_code));
            } else if (call->opt.flag & PJSUA_CALL_UNHOLD) {
	        /* Call unhold failed */
            	call->local_hold = PJ_TRUE;
	    	PJ_LOG(3,(THIS_FILE, "Error releasing hold on call %d "
	    		  "(reason=%d)", call->index, tsx->status_code));
	    }   
        }
        
        if (tsx->last_tx == (pjsip_tx_data*)call->hold_msg) {
            call->hold_msg = NULL;
        }
        
        if (tsx->last_tx->msg->body &&
            (tsx->status_code/100 != 2 || !call->med_update_success))
        {
            /* Either we get non-2xx or media update failed,
             * revert back provisional media.
             */
	    pjsua_media_prov_revert(call->index);
        }
    } else if (tsx->role == PJSIP_ROLE_UAC &&
               pjsip_method_cmp(&tsx->method, &pjsip_update_method)==0 &&
               tsx->state >= PJSIP_TSX_STATE_COMPLETED &&
	       e->body.tsx_state.prev_state < PJSIP_TSX_STATE_COMPLETED &&
               (!PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 300) &&
                tsx->status_code!=401 && tsx->status_code!=407 &&
                tsx->status_code!=422))
    {
        if (tsx->last_tx->msg->body &&
            (tsx->status_code/100 != 2 || !call->med_update_success))
        {
            /* Either we get non-2xx or media update failed,
             * revert back provisional media.
             */
	    pjsua_media_prov_revert(call->index);
        }
    } else if (tsx->role==PJSIP_ROLE_UAS &&
	       tsx->state==PJSIP_TSX_STATE_TRYING &&
	       pjsip_method_cmp(&tsx->method, &pjsip_info_method)==0)
    {
	pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
	pjsip_msg_body *body = rdata->msg_info.msg->body;

	/* Check Media Control content in the INFO message */
	if (body && body->len &&
	    pj_stricmp(&body->content_type.type, &STR_APPLICATION)==0 &&
	    pj_stricmp(&body->content_type.subtype, &STR_MEDIA_CONTROL_XML)==0)
	{
	    pjsip_tx_data *tdata;
	    pj_str_t control_st;
	    pj_status_t status;

	    /* Apply and answer the INFO request */
	    pj_strset(&control_st, (char*)body->data, body->len);
	    status = pjsua_media_apply_xml_control(call->index, &control_st);
	    if (status == PJ_SUCCESS) {
		status = pjsip_endpt_create_response(tsx->endpt, rdata,
						     200, NULL, &tdata);
		if (status == PJ_SUCCESS)
		    status = pjsip_tsx_send_msg(tsx, tdata);
	    } else {
		status = pjsip_endpt_create_response(tsx->endpt, rdata,
						     400, NULL, &tdata);
		if (status == PJ_SUCCESS)
		    status = pjsip_tsx_send_msg(tsx, tdata);
	    }
	}

	/* Check DTMF content in the INFO message */
	else if (body && body->len &&
		 pj_stricmp(&body->content_type.type, &STR_APPLICATION)==0 &&
		 pj_stricmp(&body->content_type.subtype, &STR_DTMF_RELAY)==0)
	{
	    pjsip_tx_data *tdata;
	    pj_status_t status;
	    pj_bool_t is_handled = PJ_FALSE;

	    if (pjsua_var.ua_cfg.cb.on_dtmf_digit2 ||
                pjsua_var.ua_cfg.cb.on_dtmf_event)
            {
		pjsua_dtmf_info info = {0};
		pj_str_t delim, token, input;
		pj_ssize_t found_idx;

		delim = pj_str("\r\n");
		input = pj_str(rdata->msg_info.msg->body->data);
		found_idx = pj_strtok(&input, &delim, &token, 0);
		if (found_idx != input.slen) {
		    /* Get signal/digit */
		    const pj_str_t STR_SIGNAL = { "Signal", 6 };
		    const pj_str_t STR_DURATION = { "Duration", 8 };
		    char *val;
		    pj_ssize_t count_equal_sign;

		    val = pj_strstr(&input, &STR_SIGNAL);
		    if (val) {
			char* p = val + STR_SIGNAL.slen;
			count_equal_sign = 0;
			while ((p - input.ptr < input.slen) && (*p == ' ' || *p == '=')) {
			    if(*p == '=')
				count_equal_sign++;
			    ++p;
			}

			if (count_equal_sign == 1 && (p - input.ptr < input.slen)) {
			    info.digit = *p;
			    is_handled = PJ_TRUE;
			} else {
			    PJ_LOG(2, (THIS_FILE, "Invalid dtmf-relay format"));
			}

			/* Get duration */
			input.ptr += token.slen + 2;
			input.slen -= (token.slen + 2);

			val = pj_strstr(&input, &STR_DURATION);
			if (val && is_handled) {
			    pj_str_t val_str;
			    char* ptr = val + STR_DURATION.slen;
			    count_equal_sign = 0;
			    while ((ptr - input.ptr < input.slen) &&
                                   (*ptr == ' ' || *ptr == '='))
                            {
				if (*ptr == '=')
				    count_equal_sign++;
			        ++ptr;
			    }

			    if ((count_equal_sign == 1) &&
                                (ptr - input.ptr < input.slen))
                            {
			        val_str.ptr = ptr;
			        val_str.slen = input.slen - (ptr - input.ptr);
			        info.duration = pj_strtoul(&val_str);
			    } else {
                                info.duration = PJSUA_UNKNOWN_DTMF_DURATION;
				is_handled = PJ_FALSE;
				PJ_LOG(2, (THIS_FILE,
                                           "Invalid dtmf-relay format"));
			    }
			}

			if (is_handled) {
			    info.method = PJSUA_DTMF_METHOD_SIP_INFO;
                            if (pjsua_var.ua_cfg.cb.on_dtmf_event) {
		                pjsua_dtmf_event evt;
                                pj_timestamp begin_of_time, timestamp;
                                /* Use the current instant as the events start
                                 * time.
                                 */
                                begin_of_time.u64 = 0;
                                pj_get_timestamp(&timestamp);
                                evt.method = info.method;
                                evt.timestamp = pj_elapsed_msec(&begin_of_time,
                                                                &timestamp);
                                evt.digit = info.digit;
                                evt.duration = info.duration;
                                /* There is only one message indicating the full
                                 * duration of the digit.
                                 */
                                evt.flags = PJMEDIA_STREAM_DTMF_IS_END;
                                (*pjsua_var.ua_cfg.cb.on_dtmf_event)(call->index,
                                                                     &evt);
                            } else {
			        (*pjsua_var.ua_cfg.cb.on_dtmf_digit2)(call->index,
							              &info);
                            }

			    status = pjsip_endpt_create_response(tsx->endpt, rdata,
				200, NULL, &tdata);
			    if (status == PJ_SUCCESS)
				status = pjsip_tsx_send_msg(tsx, tdata);
			}
		    }
		}
	    } 
	    
	    if (!is_handled) {
		status = pjsip_endpt_create_response(tsx->endpt, rdata,
						     400, NULL, &tdata);
		if (status == PJ_SUCCESS)
		    status = pjsip_tsx_send_msg(tsx, tdata);
	    }
	}

	/* Check Trickle ICE content in the INFO message */
	else if (body && body->len &&
		 pj_stricmp(&body->content_type.type, &STR_APPLICATION)==0 &&
		 pj_stricmp(&body->content_type.subtype,
			    &STR_TRICKLE_ICE_SDP)==0)
	{
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    /* Trickle ICE tasks:
	     * - UAS receiving INFO, cease 18x retrans & start trickling
	     */
	    if (call->trickle_ice.enabled) {
		pjsua_ice_check_start_trickling(call, PJ_FALSE, e);

		/* Process the SIP INFO content */
		trickle_ice_recv_sip_info(call, rdata);

		/* Send 200 response, regardless */
		status = pjsip_endpt_create_response(tsx->endpt, rdata,
						     200, NULL, &tdata);
	    } else {
		/* Trickle ICE not enabled, send 400 response */
		status = pjsip_endpt_create_response(tsx->endpt, rdata,
						     400, NULL, &tdata);
	    }
	    if (status == PJ_SUCCESS)
		status = pjsip_tsx_send_msg(tsx, tdata);
	}

    } else if (tsx->role == PJSIP_ROLE_UAC && 
	       pjsip_method_cmp(&tsx->method, &pjsip_info_method)==0 &&
	       (tsx->state == PJSIP_TSX_STATE_COMPLETED ||
	       (tsx->state == PJSIP_TSX_STATE_TERMINATED &&
	        e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED)))
    {
	pjsip_msg_body *body = NULL;
	
	if (e->body.tsx_state.type == PJSIP_EVENT_TX_MSG)
	    body = e->body.tsx_state.src.tdata->msg->body;
	else
	    body = e->body.tsx_state.tsx->last_tx->msg->body;

	/* Check DTMF content in the INFO message */
	if (body && body->len &&
	    pj_stricmp(&body->content_type.type, &STR_APPLICATION)==0 &&
	    pj_stricmp(&body->content_type.subtype, &STR_DTMF_RELAY)==0)
	{
	    /* Status of outgoing INFO request */
	    if (tsx->status_code >= 200 && tsx->status_code < 300) {
		PJ_LOG(4,(THIS_FILE, 
			  "Call %d: DTMF sent successfully with INFO",
			  call->index));
	    } else if (tsx->status_code >= 300) {
		PJ_LOG(4,(THIS_FILE, 
			  "Call %d: Failed to send DTMF with INFO: %d/%.*s",
			  call->index,
		          tsx->status_code,
			  (int)tsx->status_text.slen,
			  tsx->status_text.ptr));
	    }
	}

	/* Check Trickle ICE content in the INFO message */
	else if (body && body->len &&
		 pj_stricmp(&body->content_type.type, &STR_APPLICATION)==0 &&
		 pj_stricmp(&body->content_type.subtype,
			    &STR_TRICKLE_ICE_SDP)==0)
	{
	    /* Reset pending SIP INFO for Trickle ICE */
	    call->trickle_ice.pending_info = PJ_FALSE;
	}
    } else if (inv->state < PJSIP_INV_STATE_CONFIRMED &&
	       pjsip_method_cmp(&tsx->method, pjsip_get_invite_method())==0 &&
	       tsx->state == PJSIP_TSX_STATE_PROCEEDING &&
	       tsx->status_code/10 == 18)
    {
	/* Trickle ICE tasks:
	 * - UAS sending 18x, start 18x retrans
	 * - UAC receiving 18x, forcefully send SIP INFO & start trickling
	 */
	pj_bool_t force = call->trickle_ice.trickling<PJSUA_OP_STATE_RUNNING;
	pjsua_ice_check_start_trickling(call, force, e);
    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       pjsip_method_cmp(&tsx->method, pjsip_get_prack_method())==0 &&
	       tsx->state==PJSIP_TSX_STATE_TRYING)
    {
	/* Trickle ICE tasks:
	 * - UAS receiving PRACK, start trickling
	 */
	pjsua_ice_check_start_trickling(call, PJ_FALSE, e);
    }

on_return:
    pj_log_pop_indent();
}


/* Redirection handler */
static pjsip_redirect_op pjsua_call_on_redirected(pjsip_inv_session *inv,
						  const pjsip_uri *target,
						  const pjsip_event *e)
{
    pjsua_call *call = (pjsua_call*) inv->dlg->mod_data[pjsua_var.mod.id];
    pjsip_redirect_op op;

    pj_log_push_indent();

    if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_call_redirected) {
	op = (*pjsua_var.ua_cfg.cb.on_call_redirected)(call->index,
							 target, e);
    } else {
	if (!call->hanging_up) {
	    PJ_LOG(4,(THIS_FILE, "Unhandled redirection for call %d "
		      "(callback not implemented by application). "
		      "Disconnecting call.",
		      call->index));
	}
	op = PJSIP_REDIRECT_STOP;
    }

    pj_log_pop_indent();

    return op;
}


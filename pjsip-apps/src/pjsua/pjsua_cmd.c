/* $Id: pjsua_cmd.c 4347 2013-02-13 10:19:25Z nanang $ */
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

#include "pjsua_cmd.h"

#define THIS_FILE	"pjsua_cmd.c"

#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
#   define SOME_BUF_SIZE	(1024 * 10)
#else
#   define SOME_BUF_SIZE	(1024 * 3)
#endif

static char some_buf[SOME_BUF_SIZE];

/** Variable definition **/
int		    stdout_refresh = -1;
pj_bool_t	    stdout_refresh_quit = PJ_FALSE;
pjsua_call_id	    current_call = PJSUA_INVALID_ID;
pjsua_app_config    app_config;
pjsua_call_setting  call_opt;
pjsua_msg_data	    msg_data;

PJ_DEF(int) my_atoi(const char *cs)
{
    pj_str_t s;

    pj_cstr(&s, cs);
    if (cs[0] == '-') {
	s.ptr++, s.slen--;
	return 0 - (int)pj_strtoul(&s);
    } else if (cs[0] == '+') {
	s.ptr++, s.slen--;
	return pj_strtoul(&s);
    } else {
	return pj_strtoul(&s);
    }
}

/*
 * Find next call when current call is disconnected or when user
 * press ']'
 */
PJ_DEF(pj_bool_t) find_next_call()
{
    int i, max;

    max = pjsua_call_get_max_count();
    for (i=current_call+1; i<max; ++i) {
	if (pjsua_call_is_active(i)) {
	    current_call = i;
	    return PJ_TRUE;
	}
    }

    for (i=0; i<current_call; ++i) {
	if (pjsua_call_is_active(i)) {
	    current_call = i;
	    return PJ_TRUE;
	}
    }

    current_call = PJSUA_INVALID_ID;
    return PJ_FALSE;
}

PJ_DEF(pj_bool_t) find_prev_call()
{
    int i, max;

    max = pjsua_call_get_max_count();
    for (i=current_call-1; i>=0; --i) {
	if (pjsua_call_is_active(i)) {
	    current_call = i;
	    return PJ_TRUE;
	}
    }

    for (i=max-1; i>current_call; --i) {
	if (pjsua_call_is_active(i)) {
	    current_call = i;
	    return PJ_TRUE;
	}
    }

    current_call = PJSUA_INVALID_ID;
    return PJ_FALSE;
}

/*
 * Send arbitrary request to remote host
 */
PJ_DEF(void) send_request(char *cstr_method, const pj_str_t *dst_uri)
{
    pj_str_t str_method;
    pjsip_method method;
    pjsip_tx_data *tdata;
    pjsip_endpoint *endpt;
    pj_status_t status;

    endpt = pjsua_get_pjsip_endpt();

    str_method = pj_str(cstr_method);
    pjsip_method_init_np(&method, &str_method);

    status = pjsua_acc_create_request(current_acc, &method, dst_uri, &tdata);

    status = pjsip_endpt_send_request(endpt, tdata, -1, NULL, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send request", status);
	return;
    }
}

/*
 * Print log of call states. Since call states may be too long for logger,
 * printing it is a bit tricky, it should be printed part by part as long 
 * as the logger can accept.
 */
PJ_DEF(void) log_call_dump(int call_id) 
{
    unsigned call_dump_len;
    unsigned part_len;
    unsigned part_idx;
    unsigned log_decor;

    pjsua_call_dump(call_id, PJ_TRUE, some_buf, sizeof(some_buf), "  ");
    call_dump_len = strlen(some_buf);

    log_decor = pj_log_get_decor();
    pj_log_set_decor(log_decor & ~(PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));
    PJ_LOG(3,(THIS_FILE, "\n"));
    pj_log_set_decor(0);

    part_idx = 0;
    part_len = PJ_LOG_MAX_SIZE-80;
    while (part_idx < call_dump_len) {
	char p_orig, *p;

	p = &some_buf[part_idx];
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

/*
 * Save account settings
 */
static void write_account_settings(int acc_index, pj_str_t *result)
{
    unsigned i;
    char line[128];
    pjsua_acc_config *acc_cfg = &app_config.acc_cfg[acc_index];

    
    pj_ansi_sprintf(line, "\n#\n# Account %d:\n#\n", acc_index);
    pj_strcat2(result, line);


    /* Identity */
    if (acc_cfg->id.slen) {
	pj_ansi_sprintf(line, "--id %.*s\n", 
			(int)acc_cfg->id.slen, 
			acc_cfg->id.ptr);
	pj_strcat2(result, line);
    }

    /* Registrar server */
    if (acc_cfg->reg_uri.slen) {
	pj_ansi_sprintf(line, "--registrar %.*s\n",
			      (int)acc_cfg->reg_uri.slen,
			      acc_cfg->reg_uri.ptr);
	pj_strcat2(result, line);

	pj_ansi_sprintf(line, "--reg-timeout %u\n",
			      acc_cfg->reg_timeout);
	pj_strcat2(result, line);
    }

    /* Contact */
    if (acc_cfg->force_contact.slen) {
	pj_ansi_sprintf(line, "--contact %.*s\n", 
			(int)acc_cfg->force_contact.slen, 
			acc_cfg->force_contact.ptr);
	pj_strcat2(result, line);
    }

    /* Contact header parameters */
    if (acc_cfg->contact_params.slen) {
	pj_ansi_sprintf(line, "--contact-params %.*s\n", 
			(int)acc_cfg->contact_params.slen, 
			acc_cfg->contact_params.ptr);
	pj_strcat2(result, line);
    }

    /* Contact URI parameters */
    if (acc_cfg->contact_uri_params.slen) {
	pj_ansi_sprintf(line, "--contact-uri-params %.*s\n", 
			(int)acc_cfg->contact_uri_params.slen, 
			acc_cfg->contact_uri_params.ptr);
	pj_strcat2(result, line);
    }

    /*  */
    if (acc_cfg->allow_contact_rewrite!=1)
    {
	pj_ansi_sprintf(line, "--auto-update-nat %i\n",
			(int)acc_cfg->allow_contact_rewrite);
	pj_strcat2(result, line);
    }

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* SRTP */
    if (acc_cfg->use_srtp) {
	int use_srtp = (int)acc_cfg->use_srtp;
	if (use_srtp == PJMEDIA_SRTP_OPTIONAL && 
	    acc_cfg->srtp_optional_dup_offer)
	{
	    use_srtp = 3;
	}
	pj_ansi_sprintf(line, "--use-srtp %i\n", use_srtp);
	pj_strcat2(result, line);
    }
    if (acc_cfg->srtp_secure_signaling != 
	PJSUA_DEFAULT_SRTP_SECURE_SIGNALING) 
    {
	pj_ansi_sprintf(line, "--srtp-secure %d\n",
			acc_cfg->srtp_secure_signaling);
	pj_strcat2(result, line);
    }
#endif

    /* Proxy */
    for (i=0; i<acc_cfg->proxy_cnt; ++i) {
	pj_ansi_sprintf(line, "--proxy %.*s\n",
			      (int)acc_cfg->proxy[i].slen,
			      acc_cfg->proxy[i].ptr);
	pj_strcat2(result, line);
    }

    /* Credentials */
    for (i=0; i<acc_cfg->cred_count; ++i) {
	if (acc_cfg->cred_info[i].realm.slen) {
	    pj_ansi_sprintf(line, "--realm %.*s\n",
				  (int)acc_cfg->cred_info[i].realm.slen,
				  acc_cfg->cred_info[i].realm.ptr);
	    pj_strcat2(result, line);
	}

	if (acc_cfg->cred_info[i].username.slen) {
	    pj_ansi_sprintf(line, "--username %.*s\n",
				  (int)acc_cfg->cred_info[i].username.slen,
				  acc_cfg->cred_info[i].username.ptr);
	    pj_strcat2(result, line);
	}

	if (acc_cfg->cred_info[i].data.slen) {
	    pj_ansi_sprintf(line, "--password %.*s\n",
				  (int)acc_cfg->cred_info[i].data.slen,
				  acc_cfg->cred_info[i].data.ptr);
	    pj_strcat2(result, line);
	}

	if (i != acc_cfg->cred_count - 1)
	    pj_strcat2(result, "--next-cred\n");
    }

    /* reg-use-proxy */
    if (acc_cfg->reg_use_proxy != 3) {
	pj_ansi_sprintf(line, "--reg-use-proxy %d\n",
			      acc_cfg->reg_use_proxy);
	pj_strcat2(result, line);
    }

    /* rereg-delay */
    if (acc_cfg->reg_retry_interval != PJSUA_REG_RETRY_INTERVAL) {
	pj_ansi_sprintf(line, "--rereg-delay %d\n",
		              acc_cfg->reg_retry_interval);
	pj_strcat2(result, line);
    }

    /* 100rel extension */
    if (acc_cfg->require_100rel) {
	pj_strcat2(result, "--use-100rel\n");
    }

    /* Session Timer extension */
    if (acc_cfg->use_timer) {
	pj_ansi_sprintf(line, "--use-timer %d\n",
			      acc_cfg->use_timer);
	pj_strcat2(result, line);
    }
    if (acc_cfg->timer_setting.min_se != 90) {
	pj_ansi_sprintf(line, "--timer-min-se %d\n",
			      acc_cfg->timer_setting.min_se);
	pj_strcat2(result, line);
    }
    if (acc_cfg->timer_setting.sess_expires != PJSIP_SESS_TIMER_DEF_SE) {
	pj_ansi_sprintf(line, "--timer-se %d\n",
			      acc_cfg->timer_setting.sess_expires);
	pj_strcat2(result, line);
    }

    /* Publish */
    if (acc_cfg->publish_enabled)
	pj_strcat2(result, "--publish\n");

    /* MWI */
    if (acc_cfg->mwi_enabled)
	pj_strcat2(result, "--mwi\n");

    if (acc_cfg->sip_stun_use != PJSUA_STUN_USE_DEFAULT ||
	acc_cfg->media_stun_use != PJSUA_STUN_USE_DEFAULT)
    {
	pj_strcat2(result, "--disable-stun\n");
    }

    /* Media Transport*/
    if (acc_cfg->ice_cfg.enable_ice)
	pj_strcat2(result, "--use-ice\n");

    if (acc_cfg->ice_cfg.ice_opt.aggressive == PJ_FALSE)
	pj_strcat2(result, "--ice-regular\n");

    if (acc_cfg->turn_cfg.enable_turn)
	pj_strcat2(result, "--use-turn\n");

    if (acc_cfg->ice_cfg.ice_max_host_cands >= 0) {
	pj_ansi_sprintf(line, "--ice_max_host_cands %d\n",
	                acc_cfg->ice_cfg.ice_max_host_cands);
	pj_strcat2(result, line);
    }

    if (acc_cfg->ice_cfg.ice_no_rtcp)
	pj_strcat2(result, "--ice-no-rtcp\n");

    if (acc_cfg->turn_cfg.turn_server.slen) {
	pj_ansi_sprintf(line, "--turn-srv %.*s\n",
			(int)acc_cfg->turn_cfg.turn_server.slen,
			acc_cfg->turn_cfg.turn_server.ptr);
	pj_strcat2(result, line);
    }

    if (acc_cfg->turn_cfg.turn_conn_type == PJ_TURN_TP_TCP)
	pj_strcat2(result, "--turn-tcp\n");

    if (acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.username.slen) {
	pj_ansi_sprintf(line, "--turn-user %.*s\n",
			(int)acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.username.slen,
			acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.username.ptr);
	pj_strcat2(result, line);
    }

    if (acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.data.slen) {
	pj_ansi_sprintf(line, "--turn-passwd %.*s\n",
			(int)acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.data.slen,
			acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.data.ptr);
	pj_strcat2(result, line);
    }
}

/*
 * Write settings.
 */
PJ_DEF(int) write_settings(pjsua_app_config *config, char *buf, pj_size_t max)
{
    unsigned acc_index;
    unsigned i;
    pj_str_t cfg;
    char line[128];
    extern pj_bool_t pjsip_use_compact_form;

    PJ_UNUSED_ARG(max);

    cfg.ptr = buf;
    cfg.slen = 0;

    /* Logging. */
    pj_strcat2(&cfg, "#\n# Logging options:\n#\n");
    pj_ansi_sprintf(line, "--log-level %d\n",
		    config->log_cfg.level);
    pj_strcat2(&cfg, line);

    pj_ansi_sprintf(line, "--app-log-level %d\n",
		    config->log_cfg.console_level);
    pj_strcat2(&cfg, line);

    if (config->log_cfg.log_filename.slen) {
	pj_ansi_sprintf(line, "--log-file %.*s\n",
			(int)config->log_cfg.log_filename.slen,
			config->log_cfg.log_filename.ptr);
	pj_strcat2(&cfg, line);
    }

    if (config->log_cfg.log_file_flags & PJ_O_APPEND) {
	pj_strcat2(&cfg, "--log-append\n");
    }

    /* Save account settings. */
    for (acc_index=0; acc_index < config->acc_cnt; ++acc_index) {
	
	write_account_settings(acc_index, &cfg);

	if (acc_index < config->acc_cnt-1)
	    pj_strcat2(&cfg, "--next-account\n");
    }

    pj_strcat2(&cfg, "\n#\n# Network settings:\n#\n");

    /* Nameservers */
    for (i=0; i<config->cfg.nameserver_count; ++i) {
	pj_ansi_sprintf(line, "--nameserver %.*s\n",
			      (int)config->cfg.nameserver[i].slen,
			      config->cfg.nameserver[i].ptr);
	pj_strcat2(&cfg, line);
    }

    /* Outbound proxy */
    for (i=0; i<config->cfg.outbound_proxy_cnt; ++i) {
	pj_ansi_sprintf(line, "--outbound %.*s\n",
			      (int)config->cfg.outbound_proxy[i].slen,
			      config->cfg.outbound_proxy[i].ptr);
	pj_strcat2(&cfg, line);
    }

    /* Transport options */
    if (config->ipv6) {
	pj_strcat2(&cfg, "--ipv6\n");
    }
    if (config->enable_qos) {
	pj_strcat2(&cfg, "--set-qos\n");
    }

    /* UDP Transport. */
    pj_ansi_sprintf(line, "--local-port %d\n", config->udp_cfg.port);
    pj_strcat2(&cfg, line);

    /* IP address, if any. */
    if (config->udp_cfg.public_addr.slen) {
	pj_ansi_sprintf(line, "--ip-addr %.*s\n", 
			(int)config->udp_cfg.public_addr.slen,
			config->udp_cfg.public_addr.ptr);
	pj_strcat2(&cfg, line);
    }

    /* Bound IP address, if any. */
    if (config->udp_cfg.bound_addr.slen) {
	pj_ansi_sprintf(line, "--bound-addr %.*s\n", 
			(int)config->udp_cfg.bound_addr.slen,
			config->udp_cfg.bound_addr.ptr);
	pj_strcat2(&cfg, line);
    }

    /* No TCP ? */
    if (config->no_tcp) {
	pj_strcat2(&cfg, "--no-tcp\n");
    }

    /* No UDP ? */
    if (config->no_udp) {
	pj_strcat2(&cfg, "--no-udp\n");
    }

    /* STUN */
    for (i=0; i<config->cfg.stun_srv_cnt; ++i) {
	pj_ansi_sprintf(line, "--stun-srv %.*s\n",
			(int)config->cfg.stun_srv[i].slen, 
			config->cfg.stun_srv[i].ptr);
	pj_strcat2(&cfg, line);
    }

#if defined(PJSIP_HAS_TLS_TRANSPORT) && (PJSIP_HAS_TLS_TRANSPORT != 0)
    /* TLS */
    if (config->use_tls)
	pj_strcat2(&cfg, "--use-tls\n");
    if (config->udp_cfg.tls_setting.ca_list_file.slen) {
	pj_ansi_sprintf(line, "--tls-ca-file %.*s\n",
			(int)config->udp_cfg.tls_setting.ca_list_file.slen, 
			config->udp_cfg.tls_setting.ca_list_file.ptr);
	pj_strcat2(&cfg, line);
    }
    if (config->udp_cfg.tls_setting.cert_file.slen) {
	pj_ansi_sprintf(line, "--tls-cert-file %.*s\n",
			(int)config->udp_cfg.tls_setting.cert_file.slen, 
			config->udp_cfg.tls_setting.cert_file.ptr);
	pj_strcat2(&cfg, line);
    }
    if (config->udp_cfg.tls_setting.privkey_file.slen) {
	pj_ansi_sprintf(line, "--tls-privkey-file %.*s\n",
			(int)config->udp_cfg.tls_setting.privkey_file.slen, 
			config->udp_cfg.tls_setting.privkey_file.ptr);
	pj_strcat2(&cfg, line);
    }

    if (config->udp_cfg.tls_setting.password.slen) {
	pj_ansi_sprintf(line, "--tls-password %.*s\n",
			(int)config->udp_cfg.tls_setting.password.slen, 
			config->udp_cfg.tls_setting.password.ptr);
	pj_strcat2(&cfg, line);
    }

    if (config->udp_cfg.tls_setting.verify_server)
	pj_strcat2(&cfg, "--tls-verify-server\n");

    if (config->udp_cfg.tls_setting.verify_client)
	pj_strcat2(&cfg, "--tls-verify-client\n");

    if (config->udp_cfg.tls_setting.timeout.sec) {
	pj_ansi_sprintf(line, "--tls-neg-timeout %d\n",
			(int)config->udp_cfg.tls_setting.timeout.sec);
	pj_strcat2(&cfg, line);
    }

    for (i=0; i<config->udp_cfg.tls_setting.ciphers_num; ++i) {
	pj_ansi_sprintf(line, "--tls-cipher 0x%06X # %s\n",
			config->udp_cfg.tls_setting.ciphers[i],
			pj_ssl_cipher_name(config->udp_cfg.tls_setting.ciphers[i]));
	pj_strcat2(&cfg, line);
    }
#endif

    pj_strcat2(&cfg, "\n#\n# Media settings:\n#\n");

    /* Video & extra audio */
    for (i=0; i<config->vid.vid_cnt; ++i) {
	pj_strcat2(&cfg, "--video\n");
    }
    for (i=1; i<config->aud_cnt; ++i) {
	pj_strcat2(&cfg, "--extra-audio\n");
    }

    /* SRTP */
#if PJMEDIA_HAS_SRTP
    if (app_config.cfg.use_srtp != PJSUA_DEFAULT_USE_SRTP) {
	int use_srtp = (int)app_config.cfg.use_srtp;
	if (use_srtp == PJMEDIA_SRTP_OPTIONAL && 
	    app_config.cfg.srtp_optional_dup_offer)
	{
	    use_srtp = 3;
	}
	pj_ansi_sprintf(line, "--use-srtp %d\n", use_srtp);
	pj_strcat2(&cfg, line);
    }
    if (app_config.cfg.srtp_secure_signaling != 
	PJSUA_DEFAULT_SRTP_SECURE_SIGNALING) 
    {
	pj_ansi_sprintf(line, "--srtp-secure %d\n",
			app_config.cfg.srtp_secure_signaling);
	pj_strcat2(&cfg, line);
    }
#endif

    /* Media */
    if (config->null_audio)
	pj_strcat2(&cfg, "--null-audio\n");
    if (config->auto_play)
	pj_strcat2(&cfg, "--auto-play\n");
    if (config->auto_loop)
	pj_strcat2(&cfg, "--auto-loop\n");
    if (config->auto_conf)
	pj_strcat2(&cfg, "--auto-conf\n");
    for (i=0; i<config->wav_count; ++i) {
	pj_ansi_sprintf(line, "--play-file %s\n",
			config->wav_files[i].ptr);
	pj_strcat2(&cfg, line);
    }
    for (i=0; i<config->tone_count; ++i) {
	pj_ansi_sprintf(line, "--play-tone %d,%d,%d,%d\n",
			config->tones[i].freq1, config->tones[i].freq2, 
			config->tones[i].on_msec, config->tones[i].off_msec);
	pj_strcat2(&cfg, line);
    }
    if (config->rec_file.slen) {
	pj_ansi_sprintf(line, "--rec-file %s\n",
			config->rec_file.ptr);
	pj_strcat2(&cfg, line);
    }
    if (config->auto_rec)
	pj_strcat2(&cfg, "--auto-rec\n");
    if (config->capture_dev != PJSUA_INVALID_ID) {
	pj_ansi_sprintf(line, "--capture-dev %d\n", config->capture_dev);
	pj_strcat2(&cfg, line);
    }
    if (config->playback_dev != PJSUA_INVALID_ID) {
	pj_ansi_sprintf(line, "--playback-dev %d\n", config->playback_dev);
	pj_strcat2(&cfg, line);
    }
    if (config->media_cfg.snd_auto_close_time != -1) {
	pj_ansi_sprintf(line, "--snd-auto-close %d\n", 
			config->media_cfg.snd_auto_close_time);
	pj_strcat2(&cfg, line);
    }
    if (config->no_tones) {
	pj_strcat2(&cfg, "--no-tones\n");
    }
    if (config->media_cfg.jb_max != -1) {
	pj_ansi_sprintf(line, "--jb-max-size %d\n", 
			config->media_cfg.jb_max);
	pj_strcat2(&cfg, line);
    }

    /* Sound device latency */
    if (config->capture_lat != PJMEDIA_SND_DEFAULT_REC_LATENCY) {
	pj_ansi_sprintf(line, "--capture-lat %d\n", config->capture_lat);
	pj_strcat2(&cfg, line);
    }
    if (config->playback_lat != PJMEDIA_SND_DEFAULT_PLAY_LATENCY) {
	pj_ansi_sprintf(line, "--playback-lat %d\n", config->playback_lat);
	pj_strcat2(&cfg, line);
    }

    /* Media clock rate. */
    if (config->media_cfg.clock_rate != PJSUA_DEFAULT_CLOCK_RATE) {
	pj_ansi_sprintf(line, "--clock-rate %d\n",
			config->media_cfg.clock_rate);
	pj_strcat2(&cfg, line);
    } else {
	pj_ansi_sprintf(line, "#using default --clock-rate %d\n",
			config->media_cfg.clock_rate);
	pj_strcat2(&cfg, line);
    }

    if (config->media_cfg.snd_clock_rate && 
	config->media_cfg.snd_clock_rate != config->media_cfg.clock_rate) 
    {
	pj_ansi_sprintf(line, "--snd-clock-rate %d\n",
			config->media_cfg.snd_clock_rate);
	pj_strcat2(&cfg, line);
    }

    /* Stereo mode. */
    if (config->media_cfg.channel_count == 2) {
	pj_ansi_sprintf(line, "--stereo\n");
	pj_strcat2(&cfg, line);
    }

    /* quality */
    if (config->media_cfg.quality != PJSUA_DEFAULT_CODEC_QUALITY) {
	pj_ansi_sprintf(line, "--quality %d\n",
			config->media_cfg.quality);
	pj_strcat2(&cfg, line);
    } else {
	pj_ansi_sprintf(line, "#using default --quality %d\n",
			config->media_cfg.quality);
	pj_strcat2(&cfg, line);
    }

    if (config->vid.vcapture_dev != PJMEDIA_VID_DEFAULT_CAPTURE_DEV) {
	pj_ansi_sprintf(line, "--vcapture-dev %d\n", config->vid.vcapture_dev);
	pj_strcat2(&cfg, line);
    }
    if (config->vid.vrender_dev != PJMEDIA_VID_DEFAULT_RENDER_DEV) {
	pj_ansi_sprintf(line, "--vrender-dev %d\n", config->vid.vrender_dev);
	pj_strcat2(&cfg, line);
    }
    for (i=0; i<config->avi_cnt; ++i) {
	pj_ansi_sprintf(line, "--play-avi %s\n", config->avi[i].path.ptr);
	pj_strcat2(&cfg, line);
    }
    if (config->avi_auto_play) {
	pj_ansi_sprintf(line, "--auto-play-avi\n");
	pj_strcat2(&cfg, line);
    }

    /* ptime */
    if (config->media_cfg.ptime) {
	pj_ansi_sprintf(line, "--ptime %d\n",
			config->media_cfg.ptime);
	pj_strcat2(&cfg, line);
    }

    /* no-vad */
    if (config->media_cfg.no_vad) {
	pj_strcat2(&cfg, "--no-vad\n");
    }

    /* ec-tail */
    if (config->media_cfg.ec_tail_len != PJSUA_DEFAULT_EC_TAIL_LEN) {
	pj_ansi_sprintf(line, "--ec-tail %d\n",
			config->media_cfg.ec_tail_len);
	pj_strcat2(&cfg, line);
    } else {
	pj_ansi_sprintf(line, "#using default --ec-tail %d\n",
			config->media_cfg.ec_tail_len);
	pj_strcat2(&cfg, line);
    }

    /* ec-opt */
    if (config->media_cfg.ec_options != 0) {
	pj_ansi_sprintf(line, "--ec-opt %d\n",
			config->media_cfg.ec_options);
	pj_strcat2(&cfg, line);
    } 

    /* ilbc-mode */
    if (config->media_cfg.ilbc_mode != PJSUA_DEFAULT_ILBC_MODE) {
	pj_ansi_sprintf(line, "--ilbc-mode %d\n",
			config->media_cfg.ilbc_mode);
	pj_strcat2(&cfg, line);
    } else {
	pj_ansi_sprintf(line, "#using default --ilbc-mode %d\n",
			config->media_cfg.ilbc_mode);
	pj_strcat2(&cfg, line);
    }

    /* RTP drop */
    if (config->media_cfg.tx_drop_pct) {
	pj_ansi_sprintf(line, "--tx-drop-pct %d\n",
			config->media_cfg.tx_drop_pct);
	pj_strcat2(&cfg, line);

    }
    if (config->media_cfg.rx_drop_pct) {
	pj_ansi_sprintf(line, "--rx-drop-pct %d\n",
			config->media_cfg.rx_drop_pct);
	pj_strcat2(&cfg, line);

    }

    /* Start RTP port. */
    pj_ansi_sprintf(line, "--rtp-port %d\n",
		    config->rtp_cfg.port);
    pj_strcat2(&cfg, line);

    /* Disable codec */
    for (i=0; i<config->codec_dis_cnt; ++i) {
	pj_ansi_sprintf(line, "--dis-codec %s\n",
		    config->codec_dis[i].ptr);
	pj_strcat2(&cfg, line);
    }
    /* Add codec. */
    for (i=0; i<config->codec_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-codec %s\n",
		    config->codec_arg[i].ptr);
	pj_strcat2(&cfg, line);
    }

    pj_strcat2(&cfg, "\n#\n# User agent:\n#\n");

    /* Auto-answer. */
    if (config->auto_answer != 0) {
	pj_ansi_sprintf(line, "--auto-answer %d\n",
			config->auto_answer);
	pj_strcat2(&cfg, line);
    }

    /* accept-redirect */
    if (config->redir_op != PJSIP_REDIRECT_ACCEPT_REPLACE) {
	pj_ansi_sprintf(line, "--accept-redirect %d\n",
			config->redir_op);
	pj_strcat2(&cfg, line);
    }

    /* Max calls. */
    pj_ansi_sprintf(line, "--max-calls %d\n",
		    config->cfg.max_calls);
    pj_strcat2(&cfg, line);

    /* Uas-duration. */
    if (config->duration != NO_LIMIT_DURATION) {
	pj_ansi_sprintf(line, "--duration %d\n",
			config->duration);
	pj_strcat2(&cfg, line);
    }

    /* norefersub ? */
    if (config->no_refersub) {
	pj_strcat2(&cfg, "--norefersub\n");
    }

    if (pjsip_use_compact_form)
    {
	pj_strcat2(&cfg, "--use-compact-form\n");
    }

    if (!config->cfg.force_lr) {
	pj_strcat2(&cfg, "--no-force-lr\n");
    }

    pj_strcat2(&cfg, "\n#\n# Buddies:\n#\n");

    /* Add buddies. */
    for (i=0; i<config->buddy_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-buddy %.*s\n",
			      (int)config->buddy_cfg[i].uri.slen,
			      config->buddy_cfg[i].uri.ptr);
	pj_strcat2(&cfg, line);
    }

    /* SIP extensions. */
    pj_strcat2(&cfg, "\n#\n# SIP extensions:\n#\n");
    /* 100rel extension */
    if (config->cfg.require_100rel) {
	pj_strcat2(&cfg, "--use-100rel\n");
    }
    /* Session Timer extension */
    if (config->cfg.use_timer) {
	pj_ansi_sprintf(line, "--use-timer %d\n",
			      config->cfg.use_timer);
	pj_strcat2(&cfg, line);
    }
    if (config->cfg.timer_setting.min_se != 90) {
	pj_ansi_sprintf(line, "--timer-min-se %d\n",
			      config->cfg.timer_setting.min_se);
	pj_strcat2(&cfg, line);
    }
    if (config->cfg.timer_setting.sess_expires != PJSIP_SESS_TIMER_DEF_SE) {
	pj_ansi_sprintf(line, "--timer-se %d\n",
			      config->cfg.timer_setting.sess_expires);
	pj_strcat2(&cfg, line);
    }

    *(cfg.ptr + cfg.slen) = '\0';
    return cfg.slen;
}

#ifdef PJSUA_HAS_VIDEO
PJ_DEF(void) app_config_init_video(pjsua_acc_config *acc_cfg)
{
    acc_cfg->vid_in_auto_show = app_config.vid.in_auto_show;
    acc_cfg->vid_out_auto_transmit = app_config.vid.out_auto_transmit;
    /* Note that normally GUI application will prefer a borderless
     * window.
     */
    acc_cfg->vid_wnd_flags = PJMEDIA_VID_DEV_WND_BORDER |
                             PJMEDIA_VID_DEV_WND_RESIZABLE;
    acc_cfg->vid_cap_dev = app_config.vid.vcapture_dev;
    acc_cfg->vid_rend_dev = app_config.vid.vrender_dev;

    if (app_config.avi_auto_play &&
	app_config.avi_def_idx != PJSUA_INVALID_ID &&
	app_config.avi[app_config.avi_def_idx].dev_id != PJMEDIA_VID_INVALID_DEV)
    {
	acc_cfg->vid_cap_dev = app_config.avi[app_config.avi_def_idx].dev_id;
    }
}
#else
PJ_DEF(void) app_config_init_video(pjsua_acc_config *acc_cfg)
{
    PJ_UNUSED_ARG(acc_cfg);
}
#endif

#ifdef HAVE_MULTIPART_TEST
  /*
   * Enable multipart in msg_data and add a dummy body into the
   * multipart bodies.
   */
  PJ_DEF(void) add_multipart(pjsua_msg_data *msg_data)
  {
      static pjsip_multipart_part *alt_part;

      if (!alt_part) {
	  pj_str_t type, subtype, content;

	  alt_part = pjsip_multipart_create_part(app_config.pool);

	  type = pj_str("text");
	  subtype = pj_str("plain");
	  content = pj_str("Sample text body of a multipart bodies");
	  alt_part->body = pjsip_msg_body_create(app_config.pool, &type,
						 &subtype, &content);
      }

      msg_data->multipart_ctype.type = pj_str("multipart");
      msg_data->multipart_ctype.subtype = pj_str("mixed");
      pj_list_push_back(&msg_data->multipart_parts, alt_part);
  }
#endif

/* arrange windows. arg:
 *   -1:    arrange all windows
 *   != -1: arrange only this window id
 */
PJ_DEF(void) arrange_window(pjsua_vid_win_id wid)
{
#if PJSUA_HAS_VIDEO
    pjmedia_coord pos;
    int i, last;

    pos.x = 0;
    pos.y = 10;
    last = (wid == PJSUA_INVALID_ID) ? PJSUA_MAX_VID_WINS : wid;

    for (i=0; i<last; ++i) {
	pjsua_vid_win_info wi;
	pj_status_t status;

	status = pjsua_vid_win_get_info(i, &wi);
	if (status != PJ_SUCCESS)
	    continue;

	if (wid == PJSUA_INVALID_ID)
	    pjsua_vid_win_set_pos(i, &pos);

	if (wi.show)
	    pos.y += wi.size.h;
    }

    if (wid != PJSUA_INVALID_ID)
	pjsua_vid_win_set_pos(wid, &pos);
#else
    PJ_UNUSED_ARG(wid);
#endif
}


#if PJSUA_HAS_VIDEO
PJ_DEF(void) vid_print_dev(int id, const pjmedia_vid_dev_info *vdi,
                          const char *title)
{
    char capnames[120];
    char formats[120];
    const char *dirname;
    unsigned i;

    if (vdi->dir == PJMEDIA_DIR_CAPTURE_RENDER) {
	dirname = "capture, render";
    } else if (vdi->dir == PJMEDIA_DIR_CAPTURE) {
	dirname = "capture";
    } else {
	dirname = "render";
    }


    capnames[0] = '\0';
    for (i=0; i<sizeof(int)*8 && (1 << i) < PJMEDIA_VID_DEV_CAP_MAX; ++i) {
	if (vdi->caps & (1 << i)) {
	    const char *capname = pjmedia_vid_dev_cap_name(1 << i, NULL);
	    if (capname) {
		if (*capnames)
		    strcat(capnames, ", ");
		strncat(capnames, capname,
		        sizeof(capnames)-strlen(capnames)-1);
	    }
	}
    }

    formats[0] = '\0';
    for (i=0; i<vdi->fmt_cnt; ++i) {
	const pjmedia_video_format_info *vfi =
		pjmedia_get_video_format_info(NULL, vdi->fmt[i].id);
	if (vfi) {
	    if (*formats)
		strcat(formats, ", ");
	    strncat(formats, vfi->name, sizeof(formats)-strlen(formats)-1);
	}
    }

    PJ_LOG(3,(THIS_FILE, "%3d %s [%s][%s] %s", id, vdi->name, vdi->driver,
	      dirname, title));
    PJ_LOG(3,(THIS_FILE, "    Supported capabilities: %s", capnames));
    PJ_LOG(3,(THIS_FILE, "    Supported formats: %s", formats));
}

PJ_DEF(void) vid_list_devs()
{
    unsigned i, count;
    pjmedia_vid_dev_info vdi;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "Video device list:"));
    count = pjsua_vid_dev_count();
    if (count == 0) {
	PJ_LOG(3,(THIS_FILE, " - no device detected -"));
	return;
    } else {
	PJ_LOG(3,(THIS_FILE, "%d device(s) detected:", count));
    }

    status = pjsua_vid_dev_get_info(PJMEDIA_VID_DEFAULT_RENDER_DEV, &vdi);
    if (status == PJ_SUCCESS)
	vid_print_dev(PJMEDIA_VID_DEFAULT_RENDER_DEV, &vdi,
	              "(default renderer device)");

    status = pjsua_vid_dev_get_info(PJMEDIA_VID_DEFAULT_CAPTURE_DEV, &vdi);
    if (status == PJ_SUCCESS)
	vid_print_dev(PJMEDIA_VID_DEFAULT_CAPTURE_DEV, &vdi,
	              "(default capture device)");

    for (i=0; i<count; ++i) {
	status = pjsua_vid_dev_get_info(i, &vdi);
	if (status == PJ_SUCCESS)
	    vid_print_dev(i, &vdi, "");
    }
}

PJ_DEF(void) app_config_show_video(int acc_id, const pjsua_acc_config *acc_cfg)
{
    PJ_LOG(3,(THIS_FILE,
	      "Account %d:\n"
	      "  RX auto show:     %d\n"
	      "  TX auto transmit: %d\n"
	      "  Capture dev:      %d\n"
	      "  Render dev:       %d",
	      acc_id,
	      acc_cfg->vid_in_auto_show,
	      acc_cfg->vid_out_auto_transmit,
	      acc_cfg->vid_cap_dev,
	      acc_cfg->vid_rend_dev));
}


#endif
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


#define THIS_FILE		"pjsua_media.c"

#define DEFAULT_RTP_PORT	4000

#define NULL_SND_DEV_ID		-99

#ifndef PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT
#   define PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT	0
#endif


/* Next RTP port to be used */
static pj_uint16_t next_rtp_port;

/* Open sound dev */
static pj_status_t open_snd_dev(pjmedia_snd_port_param *param);
/* Close existing sound device */
static void close_snd_dev(void);
/* Create audio device param */
static pj_status_t create_aud_param(pjmedia_aud_param *param,
				    pjmedia_aud_dev_index capture_dev,
				    pjmedia_aud_dev_index playback_dev,
				    unsigned clock_rate,
				    unsigned channel_count,
				    unsigned samples_per_frame,
				    unsigned bits_per_sample);


static void pjsua_media_config_dup(pj_pool_t *pool,
				   pjsua_media_config *dst,
				   const pjsua_media_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));
    pj_strdup(pool, &dst->turn_server, &src->turn_server);
    pj_stun_auth_cred_dup(pool, &dst->turn_auth_cred, &src->turn_auth_cred);
}


/**
 * Init media subsystems.
 */
pj_status_t pjsua_media_subsys_init(const pjsua_media_config *cfg)
{
    pj_str_t codec_id = {NULL, 0};
    unsigned opt;
    pjmedia_audio_codec_config codec_cfg;
    pj_status_t status;

    /* To suppress warning about unused var when all codecs are disabled */
    PJ_UNUSED_ARG(codec_id);

    pj_log_push_indent();

    /* Specify which audio device settings are save-able */
    pjsua_var.aud_svmask = 0xFFFFFFFF;
    /* These are not-settable */
    pjsua_var.aud_svmask &= ~(PJMEDIA_AUD_DEV_CAP_EXT_FORMAT |
			      PJMEDIA_AUD_DEV_CAP_INPUT_SIGNAL_METER |
			      PJMEDIA_AUD_DEV_CAP_OUTPUT_SIGNAL_METER);
    /* EC settings use different API */
    pjsua_var.aud_svmask &= ~(PJMEDIA_AUD_DEV_CAP_EC |
			      PJMEDIA_AUD_DEV_CAP_EC_TAIL);

    /* Copy configuration */
    pjsua_media_config_dup(pjsua_var.pool, &pjsua_var.media_cfg, cfg);

    /* Normalize configuration */
    if (pjsua_var.media_cfg.snd_clock_rate == 0) {
	pjsua_var.media_cfg.snd_clock_rate = pjsua_var.media_cfg.clock_rate;
    }

    if (pjsua_var.media_cfg.has_ioqueue &&
	pjsua_var.media_cfg.thread_cnt == 0)
    {
	pjsua_var.media_cfg.thread_cnt = 1;
    }

    if (pjsua_var.media_cfg.max_media_ports < pjsua_var.ua_cfg.max_calls) {
	pjsua_var.media_cfg.max_media_ports = pjsua_var.ua_cfg.max_calls + 2;
    }

    /* Create media endpoint. */
    status = pjmedia_endpt_create(&pjsua_var.cp.factory, 
				  pjsua_var.media_cfg.has_ioqueue? NULL :
				     pjsip_endpt_get_ioqueue(pjsua_var.endpt),
				  pjsua_var.media_cfg.thread_cnt,
				  &pjsua_var.med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Media stack initialization has returned error", 
		     status);
	goto on_error;
    }

    /*
     * Register all codecs
     */
    pjmedia_audio_codec_config_default(&codec_cfg);
    codec_cfg.speex.quality = pjsua_var.media_cfg.quality;
    codec_cfg.speex.complexity = -1;
    codec_cfg.ilbc.mode = pjsua_var.media_cfg.ilbc_mode;

#if PJMEDIA_HAS_PASSTHROUGH_CODECS
    /* Register passthrough codecs */
    {
	unsigned aud_idx;
	unsigned ext_fmt_cnt = 0;
	pjmedia_format ext_fmts[32];

	/* List extended formats supported by audio devices */
	for (aud_idx = 0; aud_idx < pjmedia_aud_dev_count(); ++aud_idx) {
	    pjmedia_aud_dev_info aud_info;
	    unsigned i;
	    
	    status = pjmedia_aud_dev_get_info(aud_idx, &aud_info);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Error querying audio device info",
			     status);
		goto on_error;
	    }
	    
	    /* Collect extended formats supported by this audio device */
	    for (i = 0; i < aud_info.ext_fmt_cnt; ++i) {
		unsigned j;
		pj_bool_t is_listed = PJ_FALSE;

		/* See if this extended format is already in the list */
		for (j = 0; j < ext_fmt_cnt && !is_listed; ++j) {
		    if (ext_fmts[j].id == aud_info.ext_fmt[i].id &&
			ext_fmts[j].bitrate == aud_info.ext_fmt[i].bitrate)
		    {
			is_listed = PJ_TRUE;
		    }
		}
		
		/* Put this format into the list, if it is not in the list */
		if (!is_listed)
		    ext_fmts[ext_fmt_cnt++] = aud_info.ext_fmt[i];

		pj_assert(ext_fmt_cnt <= PJ_ARRAY_SIZE(ext_fmts));
	    }
	}

	/* Init the passthrough codec with supported formats only */
	codec_cfg.passthrough.setting.fmt_cnt = ext_fmt_cnt;
	codec_cfg.passthrough.setting.fmts = ext_fmts;
	codec_cfg.passthrough.setting.ilbc_mode = cfg->ilbc_mode;
    }
#endif /* PJMEDIA_HAS_PASSTHROUGH_CODECS */

    /* Register all codecs */
    status = pjmedia_codec_register_audio_codecs(pjsua_var.med_endpt,
                                                 &codec_cfg);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status, "Error registering codecs"));
	goto on_error;
    }

    /* Set speex/16000 to higher priority*/
    codec_id = pj_str("speex/16000");
    pjmedia_codec_mgr_set_codec_priority(
	pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
	&codec_id, PJMEDIA_CODEC_PRIO_NORMAL+2);

    /* Set speex/8000 to next higher priority*/
    codec_id = pj_str("speex/8000");
    pjmedia_codec_mgr_set_codec_priority(
	pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
	&codec_id, PJMEDIA_CODEC_PRIO_NORMAL+1);

    /* Disable ALL L16 codecs */
    codec_id = pj_str("L16");
    pjmedia_codec_mgr_set_codec_priority( 
	pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
	&codec_id, PJMEDIA_CODEC_PRIO_DISABLED);


    /* Save additional conference bridge parameters for future
     * reference.
     */
    pjsua_var.mconf_cfg.channel_count = pjsua_var.media_cfg.channel_count;
    pjsua_var.mconf_cfg.bits_per_sample = 16;
    pjsua_var.mconf_cfg.samples_per_frame = pjsua_var.media_cfg.clock_rate * 
					    pjsua_var.mconf_cfg.channel_count *
					    pjsua_var.media_cfg.audio_frame_ptime / 
					    1000;

    /* Init options for conference bridge. */
    opt = PJMEDIA_CONF_NO_DEVICE;
    if (pjsua_var.media_cfg.quality >= 3 &&
	pjsua_var.media_cfg.quality <= 4)
    {
	opt |= PJMEDIA_CONF_SMALL_FILTER;
    }
    else if (pjsua_var.media_cfg.quality < 3) {
	opt |= PJMEDIA_CONF_USE_LINEAR;
    }
	
    /* Init conference bridge. */
    status = pjmedia_conf_create(pjsua_var.pool, 
				 pjsua_var.media_cfg.max_media_ports,
				 pjsua_var.media_cfg.clock_rate, 
				 pjsua_var.mconf_cfg.channel_count,
				 pjsua_var.mconf_cfg.samples_per_frame, 
				 pjsua_var.mconf_cfg.bits_per_sample, 
				 opt, &pjsua_var.mconf);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating conference bridge", 
		     status);
	goto on_error;
    }

    /* Are we using the audio switchboard (a.k.a APS-Direct)? */
    pjsua_var.is_mswitch = pjmedia_conf_get_master_port(pjsua_var.mconf)
			    ->info.signature == PJMEDIA_CONF_SWITCH_SIGNATURE;

    /* Create null port just in case user wants to use null sound. */
    status = pjmedia_null_port_create(pjsua_var.pool, 
				      pjsua_var.media_cfg.clock_rate,
				      pjsua_var.mconf_cfg.channel_count,
				      pjsua_var.mconf_cfg.samples_per_frame,
				      pjsua_var.mconf_cfg.bits_per_sample,
				      &pjsua_var.null_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* Initialize SRTP library. */
    status = pjmedia_srtp_init_lib();
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing SRTP library", 
		     status);
	goto on_error;
    }
#endif

    /* Video */
#if PJMEDIA_HAS_VIDEO
    status = pjsua_vid_subsys_init();
    if (status != PJ_SUCCESS)
	goto on_error;
#endif

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    pj_log_pop_indent();
    return status;
}


/* Check if sound device is idle. */
static void check_snd_dev_idle()
{
    unsigned call_cnt;

    /* Get the call count, we shouldn't close the sound device when there is
     * any calls active.
     */
    call_cnt = pjsua_call_get_count();

    /* When this function is called from pjsua_media_channel_deinit() upon
     * disconnecting call, actually the call count hasn't been updated/
     * decreased. So we put additional check here, if there is only one
     * call and it's in DISCONNECTED state, there is actually no active
     * call.
     */
    if (call_cnt == 1) {
	pjsua_call_id call_id;
	pj_status_t status;

	status = pjsua_enum_calls(&call_id, &call_cnt);
	if (status == PJ_SUCCESS && call_cnt > 0 &&
	    !pjsua_call_is_active(call_id))
	{
	    call_cnt = 0;
	}
    }

    /* Activate sound device auto-close timer if sound device is idle.
     * It is idle when there is no port connection in the bridge and
     * there is no active call.
     *
     * Note: this block is now valid if no snd dev is used because of #1299
     */
    if ((pjsua_var.snd_port!=NULL || pjsua_var.null_snd!=NULL ||
	    pjsua_var.no_snd) &&
	pjsua_var.snd_idle_timer.id == PJ_FALSE &&
	pjmedia_conf_get_connect_count(pjsua_var.mconf) == 0 &&
	call_cnt == 0 &&
	pjsua_var.media_cfg.snd_auto_close_time >= 0)
    {
	pj_time_val delay;

	delay.msec = 0;
	delay.sec = pjsua_var.media_cfg.snd_auto_close_time;

	pjsua_var.snd_idle_timer.id = PJ_TRUE;
	pjsip_endpt_schedule_timer(pjsua_var.endpt, &pjsua_var.snd_idle_timer,
				   &delay);
    }
}


/* Timer callback to close sound device */
static void close_snd_timer_cb( pj_timer_heap_t *th,
				pj_timer_entry *entry)
{
    PJ_UNUSED_ARG(th);

    PJSUA_LOCK();
    if (entry->id) {
	PJ_LOG(4,(THIS_FILE,"Closing sound device after idle for %d seconds",
		  pjsua_var.media_cfg.snd_auto_close_time));

	entry->id = PJ_FALSE;

	close_snd_dev();
    }
    PJSUA_UNLOCK();
}


/*
 * Start pjsua media subsystem.
 */
pj_status_t pjsua_media_subsys_start(void)
{
    pj_status_t status;

    pj_log_push_indent();

#if DISABLED_FOR_TICKET_1185
    /* Create media for calls, if none is specified */
    if (pjsua_var.calls[0].media[0].tp == NULL) {
	pjsua_transport_config transport_cfg;

	/* Create default transport config */
	pjsua_transport_config_default(&transport_cfg);
	transport_cfg.port = DEFAULT_RTP_PORT;

	status = pjsua_media_transports_create(&transport_cfg);
	if (status != PJ_SUCCESS) {
	    pj_log_pop_indent();
	    return status;
	}
    }
#endif

    pj_timer_entry_init(&pjsua_var.snd_idle_timer, PJ_FALSE, NULL,
			&close_snd_timer_cb);

    /* Video */
#if PJMEDIA_HAS_VIDEO
    status = pjsua_vid_subsys_start();
    if (status != PJ_SUCCESS) {
	pj_log_pop_indent();
	return status;
    }
#endif

    /* Perform NAT detection */
    status = pjsua_detect_nat_type();
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status, "NAT type detection failed"));
    }

    pj_log_pop_indent();
    return PJ_SUCCESS;
}


/*
 * Destroy pjsua media subsystem.
 */
pj_status_t pjsua_media_subsys_destroy(void)
{
    unsigned i;

    PJ_LOG(4,(THIS_FILE, "Shutting down media.."));
    pj_log_push_indent();

    close_snd_dev();

    if (pjsua_var.mconf) {
	pjmedia_conf_destroy(pjsua_var.mconf);
	pjsua_var.mconf = NULL;
    }

    if (pjsua_var.null_port) {
	pjmedia_port_destroy(pjsua_var.null_port);
	pjsua_var.null_port = NULL;
    }

    /* Destroy file players */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.player); ++i) {
	if (pjsua_var.player[i].port) {
	    pjmedia_port_destroy(pjsua_var.player[i].port);
	    pjsua_var.player[i].port = NULL;
	}
    }

    /* Destroy file recorders */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.recorder); ++i) {
	if (pjsua_var.recorder[i].port) {
	    pjmedia_port_destroy(pjsua_var.recorder[i].port);
	    pjsua_var.recorder[i].port = NULL;
	}
    }

    /* Close media transports */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	unsigned strm_idx;
	pjsua_call *call = &pjsua_var.calls[i];
	for (strm_idx=0; strm_idx<call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];
	    if (call_med->tp_st != PJSUA_MED_TP_IDLE) {
		pjsua_media_channel_deinit(i);
	    }
	    if (call_med->tp && call_med->tp_auto_del) {
		pjmedia_transport_close(call_med->tp);
	    }
	    call_med->tp = NULL;
	}
    }

    /* Destroy media endpoint. */
    if (pjsua_var.med_endpt) {

#	if PJMEDIA_HAS_VIDEO
	    pjsua_vid_subsys_destroy();
#	endif

	pjmedia_endpt_destroy(pjsua_var.med_endpt);
	pjsua_var.med_endpt = NULL;

	/* Deinitialize sound subsystem */
	// Not necessary, as pjmedia_snd_deinit() should have been called
	// in pjmedia_endpt_destroy().
	//pjmedia_snd_deinit();
    }

    /* Reset RTP port */
    next_rtp_port = 0;

    pj_log_pop_indent();

    return PJ_SUCCESS;
}

/*
 * Create RTP and RTCP socket pair, and possibly resolve their public
 * address via STUN.
 */
static pj_status_t create_rtp_rtcp_sock(const pjsua_transport_config *cfg,
					pjmedia_sock_info *skinfo)
{
    enum {
	RTP_RETRY = 100
    };
    int i;
    pj_sockaddr_in bound_addr;
    pj_sockaddr_in mapped_addr[2];
    pj_status_t status = PJ_SUCCESS;
    char addr_buf[PJ_INET6_ADDRSTRLEN+2];
    pj_sock_t sock[2];

    /* Make sure STUN server resolution has completed */
    status = resolve_stun_server(PJ_TRUE);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	return status;
    }

    if (next_rtp_port == 0)
	next_rtp_port = (pj_uint16_t)cfg->port;

    if (next_rtp_port == 0)
	next_rtp_port = (pj_uint16_t)40000;

    for (i=0; i<2; ++i)
	sock[i] = PJ_INVALID_SOCKET;

    bound_addr.sin_addr.s_addr = PJ_INADDR_ANY;
    if (cfg->bound_addr.slen) {
	status = pj_sockaddr_in_set_str_addr(&bound_addr, &cfg->bound_addr);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to resolve transport bind address",
			 status);
	    return status;
	}
    }

    /* Loop retry to bind RTP and RTCP sockets. */
    for (i=0; i<RTP_RETRY; ++i, next_rtp_port += 2) {

	/* Create RTP socket. */
	status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock[0]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    return status;
	}

	/* Apply QoS to RTP socket, if specified */
	status = pj_sock_apply_qos2(sock[0], cfg->qos_type,
				    &cfg->qos_params,
				    2, THIS_FILE, "RTP socket");

	/* Bind RTP socket */
	status=pj_sock_bind_in(sock[0], pj_ntohl(bound_addr.sin_addr.s_addr),
			       next_rtp_port);
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[0]);
	    sock[0] = PJ_INVALID_SOCKET;
	    continue;
	}

	/* Create RTCP socket. */
	status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock[1]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    pj_sock_close(sock[0]);
	    return status;
	}

	/* Apply QoS to RTCP socket, if specified */
	status = pj_sock_apply_qos2(sock[1], cfg->qos_type,
				    &cfg->qos_params,
				    2, THIS_FILE, "RTCP socket");

	/* Bind RTCP socket */
	status=pj_sock_bind_in(sock[1], pj_ntohl(bound_addr.sin_addr.s_addr),
			       (pj_uint16_t)(next_rtp_port+1));
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[0]);
	    sock[0] = PJ_INVALID_SOCKET;

	    pj_sock_close(sock[1]);
	    sock[1] = PJ_INVALID_SOCKET;
	    continue;
	}

	/*
	 * If we're configured to use STUN, then find out the mapped address,
	 * and make sure that the mapped RTCP port is adjacent with the RTP.
	 */
	if (pjsua_var.stun_srv.addr.sa_family != 0) {
	    char ip_addr[32];
	    pj_str_t stun_srv;

	    pj_ansi_strcpy(ip_addr,
			   pj_inet_ntoa(pjsua_var.stun_srv.ipv4.sin_addr));
	    stun_srv = pj_str(ip_addr);

	    status=pjstun_get_mapped_addr(&pjsua_var.cp.factory, 2, sock,
					   &stun_srv, pj_ntohs(pjsua_var.stun_srv.ipv4.sin_port),
					   &stun_srv, pj_ntohs(pjsua_var.stun_srv.ipv4.sin_port),
					   mapped_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "STUN resolve error", status);
		goto on_error;
	    }

#if PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT
	    if (pj_ntohs(mapped_addr[1].sin_port) ==
		pj_ntohs(mapped_addr[0].sin_port)+1)
	    {
		/* Success! */
		break;
	    }

	    pj_sock_close(sock[0]);
	    sock[0] = PJ_INVALID_SOCKET;

	    pj_sock_close(sock[1]);
	    sock[1] = PJ_INVALID_SOCKET;
#else
	    if (pj_ntohs(mapped_addr[1].sin_port) !=
		pj_ntohs(mapped_addr[0].sin_port)+1)
	    {
		PJ_LOG(4,(THIS_FILE,
			  "Note: STUN mapped RTCP port %d is not adjacent"
			  " to RTP port %d",
			  pj_ntohs(mapped_addr[1].sin_port),
			  pj_ntohs(mapped_addr[0].sin_port)));
	    }
	    /* Success! */
	    break;
#endif

	} else if (cfg->public_addr.slen) {

	    status = pj_sockaddr_in_init(&mapped_addr[0], &cfg->public_addr,
					 (pj_uint16_t)next_rtp_port);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    status = pj_sockaddr_in_init(&mapped_addr[1], &cfg->public_addr,
					 (pj_uint16_t)(next_rtp_port+1));
	    if (status != PJ_SUCCESS)
		goto on_error;

	    break;

	} else {

	    if (bound_addr.sin_addr.s_addr == 0) {
		pj_sockaddr addr;

		/* Get local IP address. */
		status = pj_gethostip(pj_AF_INET(), &addr);
		if (status != PJ_SUCCESS)
		    goto on_error;

		bound_addr.sin_addr.s_addr = addr.ipv4.sin_addr.s_addr;
	    }

	    for (i=0; i<2; ++i) {
		pj_sockaddr_in_init(&mapped_addr[i], NULL, 0);
		mapped_addr[i].sin_addr.s_addr = bound_addr.sin_addr.s_addr;
	    }

	    mapped_addr[0].sin_port=pj_htons((pj_uint16_t)next_rtp_port);
	    mapped_addr[1].sin_port=pj_htons((pj_uint16_t)(next_rtp_port+1));
	    break;
	}
    }

    if (sock[0] == PJ_INVALID_SOCKET) {
	PJ_LOG(1,(THIS_FILE,
		  "Unable to find appropriate RTP/RTCP ports combination"));
	goto on_error;
    }


    skinfo->rtp_sock = sock[0];
    pj_memcpy(&skinfo->rtp_addr_name,
	      &mapped_addr[0], sizeof(pj_sockaddr_in));

    skinfo->rtcp_sock = sock[1];
    pj_memcpy(&skinfo->rtcp_addr_name,
	      &mapped_addr[1], sizeof(pj_sockaddr_in));

    PJ_LOG(4,(THIS_FILE, "RTP socket reachable at %s",
	      pj_sockaddr_print(&skinfo->rtp_addr_name, addr_buf,
				sizeof(addr_buf), 3)));
    PJ_LOG(4,(THIS_FILE, "RTCP socket reachable at %s",
	      pj_sockaddr_print(&skinfo->rtcp_addr_name, addr_buf,
				sizeof(addr_buf), 3)));

    next_rtp_port += 2;
    return PJ_SUCCESS;

on_error:
    for (i=0; i<2; ++i) {
	if (sock[i] != PJ_INVALID_SOCKET)
	    pj_sock_close(sock[i]);
    }
    return status;
}

/* Create normal UDP media transports */
static pj_status_t create_udp_media_transport(const pjsua_transport_config *cfg,
					      pjsua_call_media *call_med)
{
    pjmedia_sock_info skinfo;
    pj_status_t status;

    status = create_rtp_rtcp_sock(cfg, &skinfo);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create RTP/RTCP socket",
		     status);
	goto on_error;
    }

    status = pjmedia_transport_udp_attach(pjsua_var.med_endpt, NULL,
					  &skinfo, 0, &call_med->tp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create media transport",
		     status);
	goto on_error;
    }

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_ENCODING,
				    pjsua_var.media_cfg.tx_drop_pct);

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_DECODING,
				    pjsua_var.media_cfg.rx_drop_pct);

    call_med->tp_ready = PJ_SUCCESS;

    return PJ_SUCCESS;

on_error:
    if (call_med->tp)
	pjmedia_transport_close(call_med->tp);

    return status;
}

#if DISABLED_FOR_TICKET_1185
/* Create normal UDP media transports */
static pj_status_t create_udp_media_transports(pjsua_transport_config *cfg)
{
    unsigned i;
    pj_status_t status;

    for (i=0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    status = create_udp_media_transport(cfg, &call_med->tp);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    return PJ_SUCCESS;

on_error:
    for (i=0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    if (call_med->tp) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = NULL;
	    }
	}
    }
    return status;
}
#endif

/* This callback is called when ICE negotiation completes */
static void on_ice_complete(pjmedia_transport *tp, 
			    pj_ice_strans_op op,
			    pj_status_t result)
{
    pjsua_call_media *call_med = (pjsua_call_media*)tp->user_data;

    if (!call_med)
	return;

    switch (op) {
    case PJ_ICE_STRANS_OP_INIT:
        call_med->tp_ready = result;
        if (call_med->med_create_cb)
            (*call_med->med_create_cb)(call_med, result,
                                       call_med->call->secure_level, NULL);
	break;
    case PJ_ICE_STRANS_OP_NEGOTIATION:
	if (result != PJ_SUCCESS) {
	    call_med->state = PJSUA_CALL_MEDIA_ERROR;
	    call_med->dir = PJMEDIA_DIR_NONE;

	    if (call_med->call && pjsua_var.ua_cfg.cb.on_call_media_state) {
		pjsua_var.ua_cfg.cb.on_call_media_state(call_med->call->index);
	    }
	} else if (call_med->call) {
	    /* Send UPDATE if default transport address is different than
	     * what was advertised (ticket #881)
	     */
	    pjmedia_transport_info tpinfo;
	    pjmedia_ice_transport_info *ii = NULL;
	    unsigned i;

	    pjmedia_transport_info_init(&tpinfo);
	    pjmedia_transport_get_info(tp, &tpinfo);
	    for (i=0; i<tpinfo.specific_info_cnt; ++i) {
		if (tpinfo.spc_info[i].type==PJMEDIA_TRANSPORT_TYPE_ICE) {
		    ii = (pjmedia_ice_transport_info*)
			 tpinfo.spc_info[i].buffer;
		    break;
		}
	    }

	    if (ii && ii->role==PJ_ICE_SESS_ROLE_CONTROLLING &&
		pj_sockaddr_cmp(&tpinfo.sock_info.rtp_addr_name,
				&call_med->rtp_addr))
	    {
		pj_bool_t use_update;
		const pj_str_t STR_UPDATE = { "UPDATE", 6 };
		pjsip_dialog_cap_status support_update;
		pjsip_dialog *dlg;

		dlg = call_med->call->inv->dlg;
		support_update = pjsip_dlg_remote_has_cap(dlg, PJSIP_H_ALLOW,
							  NULL, &STR_UPDATE);
		use_update = (support_update == PJSIP_DIALOG_CAP_SUPPORTED);

		PJ_LOG(4,(THIS_FILE, 
		          "ICE default transport address has changed for "
			  "call %d, sending %s", call_med->call->index,
			  (use_update ? "UPDATE" : "re-INVITE")));

		if (use_update)
		    pjsua_call_update(call_med->call->index, 0, NULL);
		else
		    pjsua_call_reinvite(call_med->call->index, 0, NULL);
	    }
	}
	break;
    case PJ_ICE_STRANS_OP_KEEP_ALIVE:
	if (result != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, result,
		         "ICE keep alive failure for transport %d:%d",
		         call_med->call->index, call_med->idx));
	}
        if (pjsua_var.ua_cfg.cb.on_call_media_transport_state) {
            pjsua_med_tp_state_info info;

            pj_bzero(&info, sizeof(info));
            info.med_idx = call_med->idx;
            info.state = call_med->tp_st;
            info.status = result;
            info.ext_info = &op;
	    (*pjsua_var.ua_cfg.cb.on_call_media_transport_state)(
                call_med->call->index, &info);
        }
	if (pjsua_var.ua_cfg.cb.on_ice_transport_error) {
	    pjsua_call_id id = call_med->call->index;
	    (*pjsua_var.ua_cfg.cb.on_ice_transport_error)(id, op, result,
							  NULL);
	}
	break;
    }
}


/* Parse "HOST:PORT" format */
static pj_status_t parse_host_port(const pj_str_t *host_port,
				   pj_str_t *host, pj_uint16_t *port)
{
    pj_str_t str_port;

    str_port.ptr = pj_strchr(host_port, ':');
    if (str_port.ptr != NULL) {
	int iport;

	host->ptr = host_port->ptr;
	host->slen = (str_port.ptr - host->ptr);
	str_port.ptr++;
	str_port.slen = host_port->slen - host->slen - 1;
	iport = (int)pj_strtoul(&str_port);
	if (iport < 1 || iport > 65535)
	    return PJ_EINVAL;
	*port = (pj_uint16_t)iport;
    } else {
	*host = *host_port;
	*port = 0;
    }

    return PJ_SUCCESS;
}

/* Create ICE media transports (when ice is enabled) */
static pj_status_t create_ice_media_transport(
				const pjsua_transport_config *cfg,
				pjsua_call_media *call_med,
                                pj_bool_t async)
{
    char stunip[PJ_INET6_ADDRSTRLEN];
    pj_ice_strans_cfg ice_cfg;
    pjmedia_ice_cb ice_cb;
    char name[32];
    unsigned comp_cnt;
    pj_status_t status;

    /* Make sure STUN server resolution has completed */
    status = resolve_stun_server(PJ_TRUE);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	return status;
    }

    /* Create ICE stream transport configuration */
    pj_ice_strans_cfg_default(&ice_cfg);
    pj_stun_config_init(&ice_cfg.stun_cfg, &pjsua_var.cp.factory, 0,
		        pjsip_endpt_get_ioqueue(pjsua_var.endpt),
			pjsip_endpt_get_timer_heap(pjsua_var.endpt));
    
    ice_cfg.af = pj_AF_INET();
    ice_cfg.resolver = pjsua_var.resolver;
    
    ice_cfg.opt = pjsua_var.media_cfg.ice_opt;

    /* Configure STUN settings */
    if (pj_sockaddr_has_addr(&pjsua_var.stun_srv)) {
	pj_sockaddr_print(&pjsua_var.stun_srv, stunip, sizeof(stunip), 0);
	ice_cfg.stun.server = pj_str(stunip);
	ice_cfg.stun.port = pj_sockaddr_get_port(&pjsua_var.stun_srv);
    }
    if (pjsua_var.media_cfg.ice_max_host_cands >= 0)
	ice_cfg.stun.max_host_cands = pjsua_var.media_cfg.ice_max_host_cands;

    /* Copy QoS setting to STUN setting */
    ice_cfg.stun.cfg.qos_type = cfg->qos_type;
    pj_memcpy(&ice_cfg.stun.cfg.qos_params, &cfg->qos_params,
	      sizeof(cfg->qos_params));

    /* Configure TURN settings */
    if (pjsua_var.media_cfg.enable_turn) {
	status = parse_host_port(&pjsua_var.media_cfg.turn_server,
				 &ice_cfg.turn.server,
				 &ice_cfg.turn.port);
	if (status != PJ_SUCCESS || ice_cfg.turn.server.slen == 0) {
	    PJ_LOG(1,(THIS_FILE, "Invalid TURN server setting"));
	    return PJ_EINVAL;
	}
	if (ice_cfg.turn.port == 0)
	    ice_cfg.turn.port = 3479;
	ice_cfg.turn.conn_type = pjsua_var.media_cfg.turn_conn_type;
	pj_memcpy(&ice_cfg.turn.auth_cred, 
		  &pjsua_var.media_cfg.turn_auth_cred,
		  sizeof(ice_cfg.turn.auth_cred));

	/* Copy QoS setting to TURN setting */
	ice_cfg.turn.cfg.qos_type = cfg->qos_type;
	pj_memcpy(&ice_cfg.turn.cfg.qos_params, &cfg->qos_params,
		  sizeof(cfg->qos_params));
    }

    pj_bzero(&ice_cb, sizeof(pjmedia_ice_cb));
    ice_cb.on_ice_complete = &on_ice_complete;
    pj_ansi_snprintf(name, sizeof(name), "icetp%02d", call_med->idx);
    call_med->tp_ready = PJ_EPENDING;

    comp_cnt = 1;
    if (PJMEDIA_ADVERTISE_RTCP && !pjsua_var.media_cfg.ice_no_rtcp)
	++comp_cnt;

    status = pjmedia_ice_create3(pjsua_var.med_endpt, name, comp_cnt,
				 &ice_cfg, &ice_cb, 0, call_med,
				 &call_med->tp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create ICE media transport",
		     status);
	goto on_error;
    }

    /* Wait until transport is initialized, or time out */
    if (!async) {
        PJSUA_UNLOCK();
        while (call_med->tp_ready == PJ_EPENDING) {
	    pjsua_handle_events(100);
        }
        PJSUA_LOCK();
    }

    if (async && call_med->tp_ready == PJ_EPENDING) {
        return PJ_EPENDING;
    } else if (call_med->tp_ready != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing ICE media transport",
		     call_med->tp_ready);
	status = call_med->tp_ready;
	goto on_error;
    }

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_ENCODING,
				    pjsua_var.media_cfg.tx_drop_pct);

    pjmedia_transport_simulate_lost(call_med->tp, PJMEDIA_DIR_DECODING,
				    pjsua_var.media_cfg.rx_drop_pct);

    return PJ_SUCCESS;

on_error:
    if (call_med->tp != NULL) {
	pjmedia_transport_close(call_med->tp);
	call_med->tp = NULL;
    }

    return status;
}

#if DISABLED_FOR_TICKET_1185
/* Create ICE media transports (when ice is enabled) */
static pj_status_t create_ice_media_transports(pjsua_transport_config *cfg)
{
    unsigned i;
    pj_status_t status;

    for (i=0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    status = create_ice_media_transport(cfg, call_med);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    return PJ_SUCCESS;

on_error:
    for (i=0; i < pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    if (call_med->tp) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = NULL;
	    }
	}
    }
    return status;
}
#endif

#if DISABLED_FOR_TICKET_1185
/*
 * Create media transports for all the calls. This function creates
 * one UDP media transport for each call.
 */
PJ_DEF(pj_status_t) pjsua_media_transports_create(
			const pjsua_transport_config *app_cfg)
{
    pjsua_transport_config cfg;
    unsigned i;
    pj_status_t status;


    /* Make sure pjsua_init() has been called */
    PJ_ASSERT_RETURN(pjsua_var.ua_cfg.max_calls>0, PJ_EINVALIDOP);

    PJSUA_LOCK();

    /* Delete existing media transports */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    if (call_med->tp && call_med->tp_auto_del) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = NULL;
		call_med->tp_orig = NULL;
	    }
	}
    }

    /* Copy config */
    pjsua_transport_config_dup(pjsua_var.pool, &cfg, app_cfg);

    /* Create the transports */
    if (pjsua_var.media_cfg.enable_ice) {
	status = create_ice_media_transports(&cfg);
    } else {
	status = create_udp_media_transports(&cfg);
    }

    /* Set media transport auto_delete to True */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    call_med->tp_auto_del = PJ_TRUE;
	}
    }

    PJSUA_UNLOCK();

    return status;
}

/*
 * Attach application's created media transports.
 */
PJ_DEF(pj_status_t) pjsua_media_transports_attach(pjsua_media_transport tp[],
						  unsigned count,
						  pj_bool_t auto_delete)
{
    unsigned i;

    PJ_ASSERT_RETURN(tp && count==pjsua_var.ua_cfg.max_calls, PJ_EINVAL);

    /* Assign the media transports */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var.calls[i];
	unsigned strm_idx;

	for (strm_idx=0; strm_idx < call->med_cnt; ++strm_idx) {
	    pjsua_call_media *call_med = &call->media[strm_idx];

	    if (call_med->tp && call_med->tp_auto_del) {
		pjmedia_transport_close(call_med->tp);
		call_med->tp = NULL;
		call_med->tp_orig = NULL;
	    }
	}

	PJ_TODO(remove_pjsua_media_transports_attach);

	call->media[0].tp = tp[i].transport;
	call->media[0].tp_auto_del = auto_delete;
    }

    return PJ_SUCCESS;
}
#endif

/* Go through the list of media in the SDP, find acceptable media, and
 * sort them based on the "quality" of the media, and store the indexes
 * in the specified array. Media with the best quality will be listed
 * first in the array. The quality factors considered currently is
 * encryption.
 */
static void sort_media(const pjmedia_sdp_session *sdp,
		       const pj_str_t *type,
		       pjmedia_srtp_use	use_srtp,
		       pj_uint8_t midx[],
		       unsigned *p_count)
{
    unsigned i;
    unsigned count = 0;
    int score[PJSUA_MAX_CALL_MEDIA];

    pj_assert(*p_count >= PJSUA_MAX_CALL_MEDIA);

    *p_count = 0;
    for (i=0; i<PJSUA_MAX_CALL_MEDIA; ++i)
	score[i] = 1;

    /* Score each media */
    for (i=0; i<sdp->media_count && count<PJSUA_MAX_CALL_MEDIA; ++i) {
	const pjmedia_sdp_media *m = sdp->media[i];
	const pjmedia_sdp_conn *c;

	/* Skip different media */
	if (pj_stricmp(&m->desc.media, type) != 0) {
	    score[count++] = -22000;
	    continue;
	}

	c = m->conn? m->conn : sdp->conn;

	/* Supported transports */
	if (pj_stricmp2(&m->desc.transport, "RTP/SAVP")==0) {
	    switch (use_srtp) {
	    case PJMEDIA_SRTP_MANDATORY:
	    case PJMEDIA_SRTP_OPTIONAL:
		++score[i];
		break;
	    case PJMEDIA_SRTP_DISABLED:
		//--score[i];
		score[i] -= 5;
		break;
	    }
	} else if (pj_stricmp2(&m->desc.transport, "RTP/AVP")==0) {
	    switch (use_srtp) {
	    case PJMEDIA_SRTP_MANDATORY:
		//--score[i];
		score[i] -= 5;
		break;
	    case PJMEDIA_SRTP_OPTIONAL:
		/* No change in score */
		break;
	    case PJMEDIA_SRTP_DISABLED:
		++score[i];
		break;
	    }
	} else {
	    score[i] -= 10;
	}

	/* Is media disabled? */
	if (m->desc.port == 0)
	    score[i] -= 10;

	/* Is media inactive? */
	if (pjmedia_sdp_media_find_attr2(m, "inactive", NULL) ||
	    pj_strcmp2(&c->addr, "0.0.0.0") == 0)
	{
	    //score[i] -= 10;
	    score[i] -= 1;
	}

	++count;
    }

    /* Created sorted list based on quality */
    for (i=0; i<count; ++i) {
	unsigned j;
	int best = 0;

	for (j=1; j<count; ++j) {
	    if (score[j] > score[best])
		best = j;
	}
	/* Don't put media with negative score, that media is unacceptable
	 * for us.
	 */
	if (score[best] >= 0) {
	    midx[*p_count] = (pj_uint8_t)best;
	    (*p_count)++;
	}

	score[best] = -22000;

    }
}

/* Callback to receive media events */
static pj_status_t call_media_on_event(pjmedia_event_subscription *esub,
                                       pjmedia_event *event)
{
    pjsua_call_media *call_med = (pjsua_call_media*)esub->user_data;
    pjsua_call *call = call_med->call;

    if (pjsua_var.ua_cfg.cb.on_call_media_event && call) {
	++event->proc_cnt;
	(*pjsua_var.ua_cfg.cb.on_call_media_event)(call->index,
						   call_med->idx, event);
    }

    return PJ_SUCCESS;
}

/* Set media transport state and notify the application via the callback. */
void set_media_tp_state(pjsua_call_media *call_med,
                        pjsua_med_tp_st tp_st)
{
    if (pjsua_var.ua_cfg.cb.on_call_media_transport_state &&
        call_med->tp_st != tp_st)
    {
        pjsua_med_tp_state_info info;

        pj_bzero(&info, sizeof(info));
        info.med_idx = call_med->idx;
        info.state = tp_st;
        info.status = call_med->tp_ready;
	(*pjsua_var.ua_cfg.cb.on_call_media_transport_state)(
            call_med->call->index, &info);
    }

    call_med->tp_st = tp_st;
}

/* Callback to resume pjsua_call_media_init() after media transport
 * creation is completed.
 */
static pj_status_t call_media_init_cb(pjsua_call_media *call_med,
                                      pj_status_t status,
                                      int security_level,
                                      int *sip_err_code)
{
    pjsua_acc *acc = &pjsua_var.acc[call_med->call->acc_id];
    int err_code = 0;

    if (status != PJ_SUCCESS)
        goto on_error;

    if (call_med->tp_st == PJSUA_MED_TP_CREATING)
        set_media_tp_state(call_med, PJSUA_MED_TP_IDLE);

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* This function may be called when SRTP transport already exists
     * (e.g: in re-invite, update), don't need to destroy/re-create.
     */
    if (!call_med->tp_orig || call_med->tp == call_med->tp_orig) {
	pjmedia_srtp_setting srtp_opt;
	pjmedia_transport *srtp = NULL;

	/* Check if SRTP requires secure signaling */
	if (acc->cfg.use_srtp != PJMEDIA_SRTP_DISABLED) {
	    if (security_level < acc->cfg.srtp_secure_signaling) {
		err_code = PJSIP_SC_NOT_ACCEPTABLE;
		status = PJSIP_ESESSIONINSECURE;
		goto on_error;
	    }
	}

	/* Always create SRTP adapter */
	pjmedia_srtp_setting_default(&srtp_opt);
	srtp_opt.close_member_tp = PJ_TRUE;
	/* If media session has been ever established, let's use remote's
	 * preference in SRTP usage policy, especially when it is stricter.
	 */
	if (call_med->rem_srtp_use > acc->cfg.use_srtp)
	    srtp_opt.use = call_med->rem_srtp_use;
	else
	    srtp_opt.use = acc->cfg.use_srtp;

	status = pjmedia_transport_srtp_create(pjsua_var.med_endpt,
					       call_med->tp,
					       &srtp_opt, &srtp);
	if (status != PJ_SUCCESS) {
	    err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
	    goto on_error;
	}

	/* Set SRTP as current media transport */
	call_med->tp_orig = call_med->tp;
	call_med->tp = srtp;
    }
#else
    call_med->tp_orig = call_med->tp;
    PJ_UNUSED_ARG(security_level);
#endif

on_error:
    if (status != PJ_SUCCESS && call_med->tp) {
	pjmedia_transport_close(call_med->tp);
	call_med->tp = NULL;
    }

    if (sip_err_code)
        *sip_err_code = err_code;

    if (call_med->med_init_cb) {
        pjsua_med_tp_state_info info;

        pj_bzero(&info, sizeof(info));
        info.status = status;
        info.state = call_med->tp_st;
        info.med_idx = call_med->idx;
        info.sip_err_code = err_code;
        (*call_med->med_init_cb)(call_med->call->index, &info);
    }

    return status;
}

/* Initialize the media line */
pj_status_t pjsua_call_media_init(pjsua_call_media *call_med,
                                  pjmedia_type type,
				  const pjsua_transport_config *tcfg,
				  int security_level,
				  int *sip_err_code,
                                  pj_bool_t async,
                                  pjsua_med_tp_state_cb cb)
{
    pjsua_acc *acc = &pjsua_var.acc[call_med->call->acc_id];
    pj_status_t status = PJ_SUCCESS;

    /*
     * Note: this function may be called when the media already exists
     * (e.g. in reinvites, updates, etc.)
     */
    call_med->type = type;

    /* Create the media transport for initial call. */
    if (call_med->tp == NULL) {
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
	/* While in initial call, set default video devices */
	if (type == PJMEDIA_TYPE_VIDEO) {
	    call_med->strm.v.rdr_dev = acc->cfg.vid_rend_dev;
	    call_med->strm.v.cap_dev = acc->cfg.vid_cap_dev;
	    if (call_med->strm.v.rdr_dev == PJMEDIA_VID_DEFAULT_RENDER_DEV) {
		pjmedia_vid_dev_info info;
		pjmedia_vid_dev_get_info(call_med->strm.v.rdr_dev, &info);
		call_med->strm.v.rdr_dev = info.id;
	    }
	    if (call_med->strm.v.cap_dev == PJMEDIA_VID_DEFAULT_CAPTURE_DEV) {
		pjmedia_vid_dev_info info;
		pjmedia_vid_dev_get_info(call_med->strm.v.cap_dev, &info);
		call_med->strm.v.cap_dev = info.id;
	    }

	    /* Init event subscribtion */
	    pjmedia_event_subscription_init(&call_med->esub_rend, &call_media_on_event,
					    call_med);
	    pjmedia_event_subscription_init(&call_med->esub_cap, &call_media_on_event,
					    call_med);
	}
#endif

        set_media_tp_state(call_med, PJSUA_MED_TP_CREATING);

        if (async) {
            call_med->med_create_cb = &call_media_init_cb;
            call_med->med_init_cb = cb;
        }

	if (pjsua_var.media_cfg.enable_ice) {
	    status = create_ice_media_transport(tcfg, call_med, async);
	    if (async && status == PJ_SUCCESS) {
		/* Callback has been called. */
		call_med->med_init_cb = NULL;
		/* We cannot return PJ_SUCCESS here since we already call
		 * the callback.
		 */
		return PJ_EPENDING;
	    } else if (async && status == PJ_EPENDING) {
	        /* We will resume call media initialization in the
	         * on_ice_complete() callback.
	         */
	        return PJ_EPENDING;
	    }
	} else {
	    status = create_udp_media_transport(tcfg, call_med);
	}

        if (status != PJ_SUCCESS) {
	    PJ_PERROR(1,(THIS_FILE, status, "Error creating media transport"));
	    return status;
	}

        /* Media transport creation completed immediately, so 
         * we don't need to call the callback.
         */
        call_med->med_init_cb = NULL;

    } else if (call_med->tp_st == PJSUA_MED_TP_DISABLED) {
	/* Media is being reenabled. */
	set_media_tp_state(call_med, PJSUA_MED_TP_INIT);
    }

    return call_media_init_cb(call_med, status, security_level,
                              sip_err_code);
}

/* Callback to resume pjsua_media_channel_init() after media transport
 * initialization is completed.
 */
static pj_status_t media_channel_init_cb(pjsua_call_id call_id,
                                         const pjsua_med_tp_state_info *info)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    pj_status_t status = (info? info->status : PJ_SUCCESS);
    unsigned mi;

    if (info) {
        pj_mutex_lock(call->med_ch_mutex);

        /* Set the callback to NULL to indicate that the async operation
         * has completed.
         */
        call->media[info->med_idx].med_init_cb = NULL;

        /* In case of failure, save the information to be returned
         * by the last media transport to finish.
         */
        if (info->status != PJ_SUCCESS)
            pj_memcpy(&call->med_ch_info, info, sizeof(info));

        /* Check whether all the call's medias have finished calling their
         * callbacks.
         */
        for (mi=0; mi < call->med_cnt; ++mi) {
            pjsua_call_media *call_med = &call->media[mi];

            if (call_med->med_init_cb ||
                call_med->tp_st == PJSUA_MED_TP_NULL)
            {
                pj_mutex_unlock(call->med_ch_mutex);
                return PJ_SUCCESS;
            }

            if (call_med->tp_ready != PJ_SUCCESS)
                status = call_med->tp_ready;
        }

        /* OK, we are called by the last media transport finished. */
        pj_mutex_unlock(call->med_ch_mutex);
    }

    if (call->med_ch_mutex) {
        pj_mutex_destroy(call->med_ch_mutex);
        call->med_ch_mutex = NULL;
    }

    if (status != PJ_SUCCESS) {
        pjsua_media_channel_deinit(call_id);
        goto on_error;
    }

    /* Tell the media transport of a new offer/answer session */
    for (mi=0; mi < call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];

	/* Note: tp may be NULL if this media line is disabled */
	if (call_med->tp && call_med->tp_st == PJSUA_MED_TP_IDLE) {
            pj_pool_t *tmp_pool = call->async_call.pool_prov;
            
            if (!tmp_pool) {
                tmp_pool = (call->inv? call->inv->pool_prov:
                            call->async_call.dlg->pool);
            }

	    status = pjmedia_transport_media_create(
                         call_med->tp, tmp_pool,
                         0, call->async_call.rem_sdp, mi);
	    if (status != PJ_SUCCESS) {
                call->med_ch_info.status = status;
                call->med_ch_info.med_idx = mi;
                call->med_ch_info.state = call_med->tp_st;
                call->med_ch_info.sip_err_code = PJSIP_SC_NOT_ACCEPTABLE;
		pjsua_media_channel_deinit(call_id);
		goto on_error;
	    }

	    set_media_tp_state(call_med, PJSUA_MED_TP_INIT);
	}
    }

    call->med_ch_info.status = PJ_SUCCESS;

on_error:
    if (call->med_ch_cb)
        (*call->med_ch_cb)(call->index, &call->med_ch_info);

    return status;
}

pj_status_t pjsua_media_channel_init(pjsua_call_id call_id,
				     pjsip_role_e role,
				     int security_level,
				     pj_pool_t *tmp_pool,
				     const pjmedia_sdp_session *rem_sdp,
				     int *sip_err_code,
                                     pj_bool_t async,
                                     pjsua_med_tp_state_cb cb)
{
    const pj_str_t STR_AUDIO = { "audio", 5 };
    const pj_str_t STR_VIDEO = { "video", 5 };
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjsua_acc *acc = &pjsua_var.acc[call->acc_id];
    pj_uint8_t maudidx[PJSUA_MAX_CALL_MEDIA];
    unsigned maudcnt = PJ_ARRAY_SIZE(maudidx);
    pj_uint8_t mvididx[PJSUA_MAX_CALL_MEDIA];
    unsigned mvidcnt = PJ_ARRAY_SIZE(mvididx);
    pjmedia_type media_types[PJSUA_MAX_CALL_MEDIA];
    unsigned mi;
    pj_bool_t pending_med_tp = PJ_FALSE;
    pj_status_t status;

    PJ_UNUSED_ARG(role);

    /*
     * Note: this function may be called when the media already exists
     * (e.g. in reinvites, updates, etc).
     */

    if (pjsua_get_state() != PJSUA_STATE_RUNNING)
	return PJ_EBUSY;

    if (async) {
        pj_pool_t *tmppool = (call->inv? call->inv->pool_prov:
                              call->async_call.dlg->pool);

        status = pj_mutex_create_simple(tmppool, NULL, &call->med_ch_mutex);
        if (status != PJ_SUCCESS)
            return status;
    }

    PJ_LOG(4,(THIS_FILE, "Call %d: initializing media..", call_id));
    pj_log_push_indent();

#if DISABLED_FOR_TICKET_1185
    /* Return error if media transport has not been created yet
     * (e.g. application is starting)
     */
    for (i=0; i<call->med_cnt; ++i) {
	if (call->media[i].tp == NULL) {
	    status = PJ_EBUSY;
	    goto on_error;
	}
    }
#endif

    if (rem_sdp) {
	sort_media(rem_sdp, &STR_AUDIO, acc->cfg.use_srtp,
		   maudidx, &maudcnt);
	// Don't apply media count limitation until SDP negotiation is done.
	//if (maudcnt > acc->cfg.max_audio_cnt)
	//    maudcnt = acc->cfg.max_audio_cnt;

	if (maudcnt==0) {
	    /* Expecting audio in the offer */
	    if (sip_err_code) *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;
	    pjsua_media_channel_deinit(call_id);
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE_HERE);
	    goto on_error;
	}

#if PJMEDIA_HAS_VIDEO
	sort_media(rem_sdp, &STR_VIDEO, acc->cfg.use_srtp,
		   mvididx, &mvidcnt);
	// Don't apply media count limitation until SDP negotiation is done.
	//if (mvidcnt > acc->cfg.max_video_cnt)
	    //mvidcnt = acc->cfg.max_video_cnt;
#else
	mvidcnt = 0;
#endif

	/* Update media count only when remote add any media, this media count
	 * must never decrease.
	 */
	if (call->med_cnt < rem_sdp->media_count)
	    call->med_cnt = PJ_MIN(rem_sdp->media_count, PJSUA_MAX_CALL_MEDIA);

    } else {
	maudcnt = acc->cfg.max_audio_cnt;
	for (mi=0; mi<maudcnt; ++mi) {
	    maudidx[mi] = (pj_uint8_t)mi;
	    media_types[mi] = PJMEDIA_TYPE_AUDIO;
	}
#if PJMEDIA_HAS_VIDEO
	mvidcnt = acc->cfg.max_video_cnt;
	for (mi=0; mi<mvidcnt; ++mi) {
	    media_types[maudcnt + mi] = PJMEDIA_TYPE_VIDEO;
	}
#else
	mvidcnt = 0;
#endif	
	call->med_cnt = maudcnt + mvidcnt;
    }

    if (call->med_cnt == 0) {
	/* Expecting at least one media */
	if (sip_err_code) *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;
	pjsua_media_channel_deinit(call_id);
	status = PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE_HERE);
	goto on_error;
    }

    if (async) {
        call->med_ch_cb = cb;
        if (rem_sdp) {
            call->async_call.rem_sdp =
                pjmedia_sdp_session_clone(call->inv->pool_prov, rem_sdp);
        }
    }

    call->async_call.pool_prov = tmp_pool;

    /* Initialize each media line */
    for (mi=0; mi < call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];
	pj_bool_t enabled = PJ_FALSE;
	pjmedia_type media_type = PJMEDIA_TYPE_NONE;

	if (rem_sdp) {
	    if (mi >= rem_sdp->media_count) {
		/* Media has been removed in remote re-offer */
		media_type = call_med->type;
	    } else if (!pj_stricmp(&rem_sdp->media[mi]->desc.media, &STR_AUDIO)) {
		media_type = PJMEDIA_TYPE_AUDIO;
		if (pj_memchr(maudidx, mi, maudcnt * sizeof(maudidx[0]))) {
		    enabled = PJ_TRUE;
		}
	    }
	    else if (!pj_stricmp(&rem_sdp->media[mi]->desc.media, &STR_VIDEO)) {
		media_type = PJMEDIA_TYPE_VIDEO;
		if (pj_memchr(mvididx, mi, mvidcnt * sizeof(mvididx[0]))) {
		    enabled = PJ_TRUE;
		}
	    }

	} else {
	    enabled = PJ_TRUE;
	    media_type = media_types[mi];
	}

	if (enabled) {
	    status = pjsua_call_media_init(call_med, media_type,
	                                   &acc->cfg.rtp_cfg,
					   security_level, sip_err_code,
                                           async,
                                           (async? &media_channel_init_cb:
                                            NULL));
            if (status == PJ_EPENDING) {
                pending_med_tp = PJ_TRUE;
            } else if (status != PJ_SUCCESS) {
                if (pending_med_tp) {
                    /* Save failure information. */
                    call_med->tp_ready = status;
                    pj_bzero(&call->med_ch_info, sizeof(call->med_ch_info));
                    call->med_ch_info.status = status;
                    call->med_ch_info.state = call_med->tp_st;
                    call->med_ch_info.med_idx = call_med->idx;
                    if (sip_err_code)
                        call->med_ch_info.sip_err_code = *sip_err_code;

                    /* We will return failure in the callback later. */
                    return PJ_EPENDING;
                }

                pjsua_media_channel_deinit(call_id);
		goto on_error;
	    }
	} else {
	    /* By convention, the media is disabled if transport is NULL 
	     * or transport state is PJSUA_MED_TP_DISABLED.
	     */
	    if (call_med->tp) {
		// Don't close transport here, as SDP negotiation has not been
		// done and stream may be still active.
		//pjmedia_transport_close(call_med->tp);
		//call_med->tp = NULL;
		pj_assert(call_med->tp_st == PJSUA_MED_TP_INIT || 
			  call_med->tp_st == PJSUA_MED_TP_RUNNING);
		set_media_tp_state(call_med, PJSUA_MED_TP_DISABLED);
	    }

	    /* Put media type just for info */
	    call_med->type = media_type;
	}
    }

    call->audio_idx = maudidx[0];

    PJ_LOG(4,(THIS_FILE, "Media index %d selected for audio call %d",
	      call->audio_idx, call->index));

    if (pending_med_tp) {
        /* We shouldn't use temporary pool anymore. */
        call->async_call.pool_prov = NULL;
        /* We have a pending media transport initialization. */
        pj_log_pop_indent();
        return PJ_EPENDING;
    }

    /* Media transport initialization completed immediately, so 
     * we don't need to call the callback.
     */
    call->med_ch_cb = NULL;

    status = media_channel_init_cb(call_id, NULL);
    if (status != PJ_SUCCESS && sip_err_code)
        *sip_err_code = call->med_ch_info.sip_err_code;

    pj_log_pop_indent();
    return status;

on_error:
    if (call->med_ch_mutex) {
        pj_mutex_destroy(call->med_ch_mutex);
        call->med_ch_mutex = NULL;
    }

    pj_log_pop_indent();
    return status;
}

pj_status_t pjsua_media_channel_create_sdp(pjsua_call_id call_id, 
					   pj_pool_t *pool,
					   const pjmedia_sdp_session *rem_sdp,
					   pjmedia_sdp_session **p_sdp,
					   int *sip_err_code)
{
    enum { MAX_MEDIA = PJSUA_MAX_CALL_MEDIA };
    pjmedia_sdp_session *sdp;
    pj_sockaddr origin;
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjmedia_sdp_neg_state sdp_neg_state = PJMEDIA_SDP_NEG_STATE_NULL;
    unsigned mi;
    pj_status_t status;

    if (pjsua_get_state() != PJSUA_STATE_RUNNING)
	return PJ_EBUSY;

    if (rem_sdp) {
	/* If this is a re-offer, let's re-initialize media as remote may
	 * add or remove media
	 */
	if (call->inv && call->inv->state == PJSIP_INV_STATE_CONFIRMED) {
	    status = pjsua_media_channel_init(call_id, PJSIP_ROLE_UAS,
					      call->secure_level, pool,
					      rem_sdp, sip_err_code,
                                              PJ_FALSE, NULL);
	    if (status != PJ_SUCCESS)
		return status;
	}

#if 0
	pjsua_acc *acc = &pjsua_var.acc[call->acc_id];
	pj_uint8_t maudidx[PJSUA_MAX_CALL_MEDIA];
	unsigned maudcnt = PJ_ARRAY_SIZE(maudidx);

	sort_media(rem_sdp, &STR_AUDIO, acc->cfg.use_srtp,
	           maudidx, &maudcnt);

	if (maudcnt==0) {
	    /* Expecting audio in the offer */
	    if (sip_err_code) *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;
	    pjsua_media_channel_deinit(call_id);
	    return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE_HERE);
	}

	call->audio_idx = maudidx[0];
#endif
    } else {
	/* Audio is first in our offer, by convention */
	// The audio_idx should not be changed here, as this function may be
	// called in generating re-offer and the current active audio index
	// can be anywhere.
	//call->audio_idx = 0;
    }

#if 0
    // Since r3512, old-style hold should have got transport, created by 
    // pjsua_media_channel_init() in initial offer/answer or remote reoffer.
    /* Create media if it's not created. This could happen when call is
     * currently on-hold (with the old style hold)
     */
    if (call->media[call->audio_idx].tp == NULL) {
	pjsip_role_e role;
	role = (rem_sdp ? PJSIP_ROLE_UAS : PJSIP_ROLE_UAC);
	status = pjsua_media_channel_init(call_id, role, call->secure_level, 
					  pool, rem_sdp, sip_err_code);
	if (status != PJ_SUCCESS)
	    return status;
    }
#endif

    /* Get SDP negotiator state */
    if (call->inv && call->inv->neg)
	sdp_neg_state = pjmedia_sdp_neg_get_state(call->inv->neg);

    /* Get one address to use in the origin field */
    pj_bzero(&origin, sizeof(origin));
    for (mi=0; mi<call->med_cnt; ++mi) {
	pjmedia_transport_info tpinfo;

	if (call->media[mi].tp == NULL)
	    continue;

	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call->media[mi].tp, &tpinfo);
	pj_sockaddr_cp(&origin, &tpinfo.sock_info.rtp_addr_name);
	break;
    }

    /* Create the base (blank) SDP */
    status = pjmedia_endpt_create_base_sdp(pjsua_var.med_endpt, pool, NULL,
                                           &origin, &sdp);
    if (status != PJ_SUCCESS)
	return status;

    /* Process each media line */
    for (mi=0; mi<call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];
	pjmedia_sdp_media *m = NULL;
	pjmedia_transport_info tpinfo;

	if (rem_sdp && mi >= rem_sdp->media_count) {
	    /* Remote might have removed some media lines. */
	    break;
	}

	if (call_med->tp == NULL || call_med->tp_st == PJSUA_MED_TP_DISABLED)
	{
	    /*
	     * This media is disabled. Just create a valid SDP with zero
	     * port.
	     */
	    if (rem_sdp) {
		/* Just clone the remote media and deactivate it */
		m = pjmedia_sdp_media_clone_deactivate(pool,
						       rem_sdp->media[mi]);
	    } else {
		m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
		m->desc.transport = pj_str("RTP/AVP");
		m->desc.fmt_count = 1;
		m->conn = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
		m->conn->net_type = pj_str("IN");
		m->conn->addr_type = pj_str("IP4");
		m->conn->addr = pj_str("127.0.0.1");

		switch (call_med->type) {
		case PJMEDIA_TYPE_AUDIO:
		    m->desc.media = pj_str("audio");
		    m->desc.fmt[0] = pj_str("0");
		    break;
		case PJMEDIA_TYPE_VIDEO:
		    m->desc.media = pj_str("video");
		    m->desc.fmt[0] = pj_str("31");
		    break;
		default:
		    if (rem_sdp) {
			pj_strdup(pool, &m->desc.media,
				  &rem_sdp->media[mi]->desc.media);
			pj_strdup(pool, &m->desc.fmt[0],
				  &rem_sdp->media[mi]->desc.fmt[0]);
		    } else {
			pj_assert(!"Invalid call_med media type");
			return PJ_EBUG;
		    }
		}
	    }

	    sdp->media[sdp->media_count++] = m;
	    continue;
	}

	/* Get transport address info */
	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call_med->tp, &tpinfo);

	/* Ask pjmedia endpoint to create SDP media line */
	switch (call_med->type) {
	case PJMEDIA_TYPE_AUDIO:
	    status = pjmedia_endpt_create_audio_sdp(pjsua_var.med_endpt, pool,
                                                    &tpinfo.sock_info, 0, &m);
	    break;
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
	case PJMEDIA_TYPE_VIDEO:
	    status = pjmedia_endpt_create_video_sdp(pjsua_var.med_endpt, pool,
	                                            &tpinfo.sock_info, 0, &m);
	    break;
#endif
	default:
	    pj_assert(!"Invalid call_med media type");
	    return PJ_EBUG;
	}

	if (status != PJ_SUCCESS)
	    return status;

	sdp->media[sdp->media_count++] = m;

	/* Give to transport */
	status = pjmedia_transport_encode_sdp(call_med->tp, pool,
					      sdp, rem_sdp, mi);
	if (status != PJ_SUCCESS) {
	    if (sip_err_code) *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE;
	    return status;
	}

	/* Copy c= line of the first media to session level,
	 * if there's none.
	 */
	if (sdp->conn == NULL) {
	    sdp->conn = pjmedia_sdp_conn_clone(pool, m->conn);
	}
    }

    /* Add NAT info in the SDP */
    if (pjsua_var.ua_cfg.nat_type_in_sdp) {
	pjmedia_sdp_attr *a;
	pj_str_t value;
	char nat_info[80];

	value.ptr = nat_info;
	if (pjsua_var.ua_cfg.nat_type_in_sdp == 1) {
	    value.slen = pj_ansi_snprintf(nat_info, sizeof(nat_info),
					  "%d", pjsua_var.nat_type);
	} else {
	    const char *type_name = pj_stun_get_nat_name(pjsua_var.nat_type);
	    value.slen = pj_ansi_snprintf(nat_info, sizeof(nat_info),
					  "%d %s",
					  pjsua_var.nat_type,
					  type_name);
	}

	a = pjmedia_sdp_attr_create(pool, "X-nat", &value);

	pjmedia_sdp_attr_add(&sdp->attr_count, sdp->attr, a);

    }


#if DISABLED_FOR_TICKET_1185 && defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* Check if SRTP is in optional mode and configured to use duplicated
     * media, i.e: secured and unsecured version, in the SDP offer.
     */
    if (!rem_sdp &&
	pjsua_var.acc[call->acc_id].cfg.use_srtp == PJMEDIA_SRTP_OPTIONAL &&
	pjsua_var.acc[call->acc_id].cfg.srtp_optional_dup_offer)
    {
	unsigned i;

	for (i = 0; i < sdp->media_count; ++i) {
	    pjmedia_sdp_media *m = sdp->media[i];

	    /* Check if this media is unsecured but has SDP "crypto"
	     * attribute.
	     */
	    if (pj_stricmp2(&m->desc.transport, "RTP/AVP") == 0 &&
		pjmedia_sdp_media_find_attr2(m, "crypto", NULL) != NULL)
	    {
		if (i == (unsigned)call->audio_idx &&
		    sdp_neg_state == PJMEDIA_SDP_NEG_STATE_DONE)
		{
		    /* This is a session update, and peer has chosen the
		     * unsecured version, so let's make this unsecured too.
		     */
		    pjmedia_sdp_media_remove_all_attr(m, "crypto");
		} else {
		    /* This is new offer, duplicate media so we'll have
		     * secured (with "RTP/SAVP" transport) and and unsecured
		     * versions.
		     */
		    pjmedia_sdp_media *new_m;

		    /* Duplicate this media and apply secured transport */
		    new_m = pjmedia_sdp_media_clone(pool, m);
		    pj_strdup2(pool, &new_m->desc.transport, "RTP/SAVP");

		    /* Remove the "crypto" attribute in the unsecured media */
		    pjmedia_sdp_media_remove_all_attr(m, "crypto");

		    /* Insert the new media before the unsecured media */
		    if (sdp->media_count < PJMEDIA_MAX_SDP_MEDIA) {
			pj_array_insert(sdp->media, sizeof(new_m),
					sdp->media_count, i, &new_m);
			++sdp->media_count;
			++i;
		    }
		}
	    }
	}
    }
#endif

    *p_sdp = sdp;
    return PJ_SUCCESS;
}


static void stop_media_session(pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    unsigned mi;

    pj_log_push_indent();

    for (mi=0; mi<call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];

	if (call_med->type == PJMEDIA_TYPE_AUDIO) {
	    pjmedia_stream *strm = call_med->strm.a.stream;
	    pjmedia_rtcp_stat stat;

	    if (strm) {
		if (call_med->strm.a.conf_slot != PJSUA_INVALID_ID) {
		    if (pjsua_var.mconf) {
			pjsua_conf_remove_port(call_med->strm.a.conf_slot);
		    }
		    call_med->strm.a.conf_slot = PJSUA_INVALID_ID;
		}

		if ((call_med->dir & PJMEDIA_DIR_ENCODING) &&
		    (pjmedia_stream_get_stat(strm, &stat) == PJ_SUCCESS))
		{
		    /* Save RTP timestamp & sequence, so when media session is
		     * restarted, those values will be restored as the initial
		     * RTP timestamp & sequence of the new media session. So in
		     * the same call session, RTP timestamp and sequence are
		     * guaranteed to be contigue.
		     */
		    call_med->rtp_tx_seq_ts_set = 1 | (1 << 1);
		    call_med->rtp_tx_seq = stat.rtp_tx_last_seq;
		    call_med->rtp_tx_ts = stat.rtp_tx_last_ts;
		}

		if (pjsua_var.ua_cfg.cb.on_stream_destroyed) {
		    pjsua_var.ua_cfg.cb.on_stream_destroyed(call_id, strm, mi);
		}

		pjmedia_stream_destroy(strm);
		call_med->strm.a.stream = NULL;
	    }
	}

#if PJMEDIA_HAS_VIDEO
	else if (call_med->type == PJMEDIA_TYPE_VIDEO) {
	    stop_video_stream(call_med);
	}
#endif

	PJ_LOG(4,(THIS_FILE, "Media session call%02d:%d is destroyed",
			     call_id, mi));
	call_med->state = PJSUA_CALL_MEDIA_NONE;
    }

    pj_log_pop_indent();
}

pj_status_t pjsua_media_channel_deinit(pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    unsigned mi;

    PJSUA_LOCK();
    for (mi=0; mi<call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];

        if (call_med->tp_st == PJSUA_MED_TP_CREATING) {
            /* We will do the deinitialization after media transport
             * creation is completed.
             */
            call->async_call.med_ch_deinit = PJ_TRUE;
            PJSUA_UNLOCK();
            return PJ_SUCCESS;
        }
    }
    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Call %d: deinitializing media..", call_id));
    pj_log_push_indent();

    stop_media_session(call_id);

    for (mi=0; mi<call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];

	if (call_med->tp_st > PJSUA_MED_TP_IDLE) {
	    pjmedia_transport_media_stop(call_med->tp);
	    set_media_tp_state(call_med, PJSUA_MED_TP_IDLE);
	}

	//if (call_med->tp_orig && call_med->tp &&
	//	call_med->tp != call_med->tp_orig)
	//{
	//    pjmedia_transport_close(call_med->tp);
	//    call_med->tp = call_med->tp_orig;
	//}
	if (call_med->tp) {
	    pjmedia_transport_close(call_med->tp);
	    call_med->tp = call_med->tp_orig = NULL;
	}
    }

    check_snd_dev_idle();
    pj_log_pop_indent();

    return PJ_SUCCESS;
}


/*
 * DTMF callback from the stream.
 */
static void dtmf_callback(pjmedia_stream *strm, void *user_data,
			  int digit)
{
    PJ_UNUSED_ARG(strm);

    pj_log_push_indent();

    /* For discussions about call mutex protection related to this 
     * callback, please see ticket #460:
     *	http://trac.pjsip.org/repos/ticket/460#comment:4
     */
    if (pjsua_var.ua_cfg.cb.on_dtmf_digit) {
	pjsua_call_id call_id;

	call_id = (pjsua_call_id)(long)user_data;
	pjsua_var.ua_cfg.cb.on_dtmf_digit(call_id, digit);
    }

    pj_log_pop_indent();
}


static pj_status_t audio_channel_update(pjsua_call_media *call_med,
                                        pj_pool_t *tmp_pool,
				        const pjmedia_sdp_session *local_sdp,
				        const pjmedia_sdp_session *remote_sdp)
{
    pjsua_call *call = call_med->call;
    pjmedia_stream_info the_si, *si = &the_si;
    pjmedia_port *media_port;
    unsigned strm_idx = call_med->idx;
    pj_status_t status;

    PJ_LOG(4,(THIS_FILE,"Audio channel update.."));
    pj_log_push_indent();
    
    status = pjmedia_stream_info_from_sdp(si, tmp_pool, pjsua_var.med_endpt,
                                          local_sdp, remote_sdp, strm_idx);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Check if no media is active */
    if (si->dir == PJMEDIA_DIR_NONE) {
	/* Call media state */
	call_med->state = PJSUA_CALL_MEDIA_NONE;

	/* Call media direction */
	call_med->dir = PJMEDIA_DIR_NONE;

    } else {
	pjmedia_transport_info tp_info;

	/* Start/restart media transport */
	status = pjmedia_transport_media_start(call_med->tp,
					       tmp_pool, local_sdp,
					       remote_sdp, strm_idx);
	if (status != PJ_SUCCESS)
	    goto on_return;

	set_media_tp_state(call_med, PJSUA_MED_TP_RUNNING);

	/* Get remote SRTP usage policy */
	pjmedia_transport_info_init(&tp_info);
	pjmedia_transport_get_info(call_med->tp, &tp_info);
	if (tp_info.specific_info_cnt > 0) {
	    unsigned i;
	    for (i = 0; i < tp_info.specific_info_cnt; ++i) {
		if (tp_info.spc_info[i].type == PJMEDIA_TRANSPORT_TYPE_SRTP) 
		{
		    pjmedia_srtp_info *srtp_info = 
				(pjmedia_srtp_info*) tp_info.spc_info[i].buffer;

		    call_med->rem_srtp_use = srtp_info->peer_use;
		    break;
		}
	    }
	}

	/* Override ptime, if this option is specified. */
	if (pjsua_var.media_cfg.ptime != 0) {
	    si->param->setting.frm_per_pkt = (pj_uint8_t)
		(pjsua_var.media_cfg.ptime / si->param->info.frm_ptime);
	    if (si->param->setting.frm_per_pkt == 0)
		si->param->setting.frm_per_pkt = 1;
	}

	/* Disable VAD, if this option is specified. */
	if (pjsua_var.media_cfg.no_vad) {
	    si->param->setting.vad = 0;
	}


	/* Optionally, application may modify other stream settings here
	 * (such as jitter buffer parameters, codec ptime, etc.)
	 */
	si->jb_init = pjsua_var.media_cfg.jb_init;
	si->jb_min_pre = pjsua_var.media_cfg.jb_min_pre;
	si->jb_max_pre = pjsua_var.media_cfg.jb_max_pre;
	si->jb_max = pjsua_var.media_cfg.jb_max;

	/* Set SSRC */
	si->ssrc = call_med->ssrc;

	/* Set RTP timestamp & sequence, normally these value are intialized
	 * automatically when stream session created, but for some cases (e.g:
	 * call reinvite, call update) timestamp and sequence need to be kept
	 * contigue.
	 */
	si->rtp_ts = call_med->rtp_tx_ts;
	si->rtp_seq = call_med->rtp_tx_seq;
	si->rtp_seq_ts_set = call_med->rtp_tx_seq_ts_set;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
	/* Enable/disable stream keep-alive and NAT hole punch. */
	si->use_ka = pjsua_var.acc[call->acc_id].cfg.use_stream_ka;
#endif

	/* Create session based on session info. */
	status = pjmedia_stream_create(pjsua_var.med_endpt, NULL, si,
				       call_med->tp, NULL,
				       &call_med->strm.a.stream);
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}

	/* Start stream */
	status = pjmedia_stream_start(call_med->strm.a.stream);
	if (status != PJ_SUCCESS) {
	    goto on_return;
	}

	/* If DTMF callback is installed by application, install our
	 * callback to the session.
	 */
	if (pjsua_var.ua_cfg.cb.on_dtmf_digit) {
	    pjmedia_stream_set_dtmf_callback(call_med->strm.a.stream,
					     &dtmf_callback,
					     (void*)(long)(call->index));
	}

	/* Get the port interface of the first stream in the session.
	 * We need the port interface to add to the conference bridge.
	 */
	pjmedia_stream_get_port(call_med->strm.a.stream, &media_port);

	/* Notify application about stream creation.
	 * Note: application may modify media_port to point to different
	 * media port
	 */
	if (pjsua_var.ua_cfg.cb.on_stream_created) {
	    pjsua_var.ua_cfg.cb.on_stream_created(call->index,
						  call_med->strm.a.stream,
						  strm_idx, &media_port);
	}

	/*
	 * Add the call to conference bridge.
	 */
	{
	    char tmp[PJSIP_MAX_URL_SIZE];
	    pj_str_t port_name;

	    port_name.ptr = tmp;
	    port_name.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
					     call->inv->dlg->remote.info->uri,
					     tmp, sizeof(tmp));
	    if (port_name.slen < 1) {
		port_name = pj_str("call");
	    }
	    status = pjmedia_conf_add_port( pjsua_var.mconf, 
					    call->inv->pool_prov,
					    media_port, 
					    &port_name,
					    (unsigned*)
					    &call_med->strm.a.conf_slot);
	    if (status != PJ_SUCCESS) {
		goto on_return;
	    }
	}

	/* Call media direction */
	call_med->dir = si->dir;

	/* Call media state */
	if (call->local_hold)
	    call_med->state = PJSUA_CALL_MEDIA_LOCAL_HOLD;
	else if (call_med->dir == PJMEDIA_DIR_DECODING)
	    call_med->state = PJSUA_CALL_MEDIA_REMOTE_HOLD;
	else
	    call_med->state = PJSUA_CALL_MEDIA_ACTIVE;
    }

    /* Print info. */
    {
	char info[80];
	int info_len = 0;
	int len;
	const char *dir;

	switch (si->dir) {
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
			       ", stream #%d: %.*s (%s)", strm_idx,
			       (int)si->fmt.encoding_name.slen,
			       si->fmt.encoding_name.ptr,
			       dir);
	if (len > 0)
	    info_len += len;
	PJ_LOG(4,(THIS_FILE,"Audio updated%s", info));
    }

on_return:
    pj_log_pop_indent();
    return status;
}

pj_status_t pjsua_media_channel_update(pjsua_call_id call_id,
				       const pjmedia_sdp_session *local_sdp,
				       const pjmedia_sdp_session *remote_sdp)
{
    pjsua_call *call = &pjsua_var.calls[call_id];
    pjsua_acc *acc = &pjsua_var.acc[call->acc_id];
    pj_pool_t *tmp_pool = call->inv->pool_prov;
    unsigned mi;
    pj_bool_t got_media = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;

    const pj_str_t STR_AUDIO = { "audio", 5 };
    const pj_str_t STR_VIDEO = { "video", 5 };
    pj_uint8_t maudidx[PJSUA_MAX_CALL_MEDIA];
    unsigned maudcnt = PJ_ARRAY_SIZE(maudidx);
    pj_uint8_t mvididx[PJSUA_MAX_CALL_MEDIA];
    unsigned mvidcnt = PJ_ARRAY_SIZE(mvididx);
    pj_bool_t need_renego_sdp = PJ_FALSE;

    if (pjsua_get_state() != PJSUA_STATE_RUNNING)
	return PJ_EBUSY;

    PJ_LOG(4,(THIS_FILE, "Call %d: updating media..", call_id));
    pj_log_push_indent();

    /* Destroy existing media session, if any. */
    stop_media_session(call->index);

    /* Call media count must be at least equal to SDP media. Note that
     * it may not be equal when remote removed any SDP media line.
     */
    pj_assert(call->med_cnt >= local_sdp->media_count);

    /* Reset audio_idx first */
    call->audio_idx = -1;

    /* Apply maximum audio/video count of the account */
    sort_media(local_sdp, &STR_AUDIO, acc->cfg.use_srtp,
	       maudidx, &maudcnt);
#if PJMEDIA_HAS_VIDEO
    sort_media(local_sdp, &STR_VIDEO, acc->cfg.use_srtp,
	       mvididx, &mvidcnt);
#else
    PJ_UNUSED_ARG(STR_VIDEO);
    mvidcnt = 0;
#endif
    if (maudcnt > acc->cfg.max_audio_cnt || mvidcnt > acc->cfg.max_video_cnt)
    {
	pjmedia_sdp_session *local_sdp2;

	maudcnt = PJ_MIN(maudcnt, acc->cfg.max_audio_cnt);
	mvidcnt = PJ_MIN(mvidcnt, acc->cfg.max_video_cnt);
	local_sdp2 = pjmedia_sdp_session_clone(tmp_pool, local_sdp);

	for (mi=0; mi < local_sdp2->media_count; ++mi) {
	    pjmedia_sdp_media *m = local_sdp2->media[mi];

	    if (m->desc.port == 0 ||
		pj_memchr(maudidx, mi, maudcnt*sizeof(maudidx[0])) ||
		pj_memchr(mvididx, mi, mvidcnt*sizeof(mvididx[0])))
	    {
		continue;
	    }
	    
	    /* Deactivate this media */
	    pjmedia_sdp_media_deactivate(tmp_pool, m);
	}

	local_sdp = local_sdp2;
	need_renego_sdp = PJ_TRUE;
    }

    /* Process each media stream */
    for (mi=0; mi < call->med_cnt; ++mi) {
	pjsua_call_media *call_med = &call->media[mi];

	if (mi >= local_sdp->media_count ||
	    mi >= remote_sdp->media_count)
	{
	    /* This may happen when remote removed any SDP media lines in
	     * its re-offer.
	     */
	    continue;
#if 0
	    /* Something is wrong */
	    PJ_LOG(1,(THIS_FILE, "Error updating media for call %d: "
		      "invalid media index %d in SDP", call_id, mi));
	    status = PJMEDIA_SDP_EINSDP;
	    goto on_error;
#endif
	}

	switch (call_med->type) {
	case PJMEDIA_TYPE_AUDIO:
	    status = audio_channel_update(call_med, tmp_pool,
	                                  local_sdp, remote_sdp);
	    if (call->audio_idx==-1 && status==PJ_SUCCESS &&
		call_med->strm.a.stream)
	    {
		call->audio_idx = mi;
	    }
	    break;
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
	case PJMEDIA_TYPE_VIDEO:
	    status = video_channel_update(call_med, tmp_pool,
	                                  local_sdp, remote_sdp);
	    break;
#endif
	default:
	    status = PJMEDIA_EINVALIMEDIATYPE;
	    break;
	}

	/* Close the transport of deactivated media, need this here as media
	 * can be deactivated by the SDP negotiation and the max media count
	 * (account) setting.
	 */
	if (local_sdp->media[mi]->desc.port==0 && call_med->tp) {
	    pjmedia_transport_close(call_med->tp);
	    call_med->tp = call_med->tp_orig = NULL;
	    set_media_tp_state(call_med, PJSUA_MED_TP_IDLE);
	}

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(1,(THIS_FILE, status, "Error updating media call%02d:%d",
		         call_id, mi));
	} else {
	    got_media = PJ_TRUE;
	}
    }

    /* Perform SDP re-negotiation if needed. */
    if (got_media && need_renego_sdp) {
	pjmedia_sdp_neg *neg = call->inv->neg;

	/* This should only happen when we are the answerer. */
	PJ_ASSERT_RETURN(neg && !pjmedia_sdp_neg_was_answer_remote(neg),
			 PJMEDIA_SDPNEG_EINSTATE);
	
	status = pjmedia_sdp_neg_set_remote_offer(tmp_pool, neg, remote_sdp);
	if (status != PJ_SUCCESS)
	    goto on_error;

	status = pjmedia_sdp_neg_set_local_answer(tmp_pool, neg, local_sdp);
	if (status != PJ_SUCCESS)
	    goto on_error;

	status = pjmedia_sdp_neg_negotiate(tmp_pool, neg, 0);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    pj_log_pop_indent();
    return (got_media? PJ_SUCCESS : PJMEDIA_SDPNEG_ENOMEDIA);

on_error:
    pj_log_pop_indent();
    return status;
}

/*
 * Get maxinum number of conference ports.
 */
PJ_DEF(unsigned) pjsua_conf_get_max_ports(void)
{
    return pjsua_var.media_cfg.max_media_ports;
}


/*
 * Get current number of active ports in the bridge.
 */
PJ_DEF(unsigned) pjsua_conf_get_active_ports(void)
{
    unsigned ports[PJSUA_MAX_CONF_PORTS];
    unsigned count = PJ_ARRAY_SIZE(ports);
    pj_status_t status;

    status = pjmedia_conf_enum_ports(pjsua_var.mconf, ports, &count);
    if (status != PJ_SUCCESS)
	count = 0;

    return count;
}


/*
 * Enumerate all conference ports.
 */
PJ_DEF(pj_status_t) pjsua_enum_conf_ports(pjsua_conf_port_id id[],
					  unsigned *count)
{
    return pjmedia_conf_enum_ports(pjsua_var.mconf, (unsigned*)id, count);
}


/*
 * Get information about the specified conference port
 */
PJ_DEF(pj_status_t) pjsua_conf_get_port_info( pjsua_conf_port_id id,
					      pjsua_conf_port_info *info)
{
    pjmedia_conf_port_info cinfo;
    unsigned i;
    pj_status_t status;

    status = pjmedia_conf_get_port_info( pjsua_var.mconf, id, &cinfo);
    if (status != PJ_SUCCESS)
	return status;

    pj_bzero(info, sizeof(*info));
    info->slot_id = id;
    info->name = cinfo.name;
    info->clock_rate = cinfo.clock_rate;
    info->channel_count = cinfo.channel_count;
    info->samples_per_frame = cinfo.samples_per_frame;
    info->bits_per_sample = cinfo.bits_per_sample;

    /* Build array of listeners */
    info->listener_cnt = cinfo.listener_cnt;
    for (i=0; i<cinfo.listener_cnt; ++i) {
	info->listeners[i] = cinfo.listener_slots[i];
    }

    return PJ_SUCCESS;
}


/*
 * Add arbitrary media port to PJSUA's conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_conf_add_port( pj_pool_t *pool,
					 pjmedia_port *port,
					 pjsua_conf_port_id *p_id)
{
    pj_status_t status;

    status = pjmedia_conf_add_port(pjsua_var.mconf, pool,
				   port, NULL, (unsigned*)p_id);
    if (status != PJ_SUCCESS) {
	if (p_id)
	    *p_id = PJSUA_INVALID_ID;
    }

    return status;
}


/*
 * Remove arbitrary slot from the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_conf_remove_port(pjsua_conf_port_id id)
{
    pj_status_t status;

    status = pjmedia_conf_remove_port(pjsua_var.mconf, (unsigned)id);
    check_snd_dev_idle();

    return status;
}


/*
 * Establish unidirectional media flow from souce to sink. 
 */
PJ_DEF(pj_status_t) pjsua_conf_connect( pjsua_conf_port_id source,
					pjsua_conf_port_id sink)
{
    pj_status_t status = PJ_SUCCESS;

    PJ_LOG(4,(THIS_FILE, "%s connect: %d --> %d",
	      (pjsua_var.is_mswitch ? "Switch" : "Conf"),
	      source, sink));
    pj_log_push_indent();

    /* If sound device idle timer is active, cancel it first. */
    PJSUA_LOCK();
    if (pjsua_var.snd_idle_timer.id) {
	pjsip_endpt_cancel_timer(pjsua_var.endpt, &pjsua_var.snd_idle_timer);
	pjsua_var.snd_idle_timer.id = PJ_FALSE;
    }
    PJSUA_UNLOCK();


    /* For audio switchboard (i.e. APS-Direct):
     * Check if sound device need to be reopened, i.e: its attributes
     * (format, clock rate, channel count) must match to peer's. 
     * Note that sound device can be reopened only if it doesn't have
     * any connection.
     */
    if (pjsua_var.is_mswitch) {
	pjmedia_conf_port_info port0_info;
	pjmedia_conf_port_info peer_info;
	unsigned peer_id;
	pj_bool_t need_reopen = PJ_FALSE;

	peer_id = (source!=0)? source : sink;
	status = pjmedia_conf_get_port_info(pjsua_var.mconf, peer_id, 
					    &peer_info);
	pj_assert(status == PJ_SUCCESS);

	status = pjmedia_conf_get_port_info(pjsua_var.mconf, 0, &port0_info);
	pj_assert(status == PJ_SUCCESS);

	/* Check if sound device is instantiated. */
	need_reopen = (pjsua_var.snd_port==NULL && pjsua_var.null_snd==NULL && 
		      !pjsua_var.no_snd);

	/* Check if sound device need to reopen because it needs to modify 
	 * settings to match its peer. Sound device must be idle in this case 
	 * though.
	 */
	if (!need_reopen && 
	    port0_info.listener_cnt==0 && port0_info.transmitter_cnt==0) 
	{
	    need_reopen = (peer_info.format.id != port0_info.format.id ||
			   peer_info.format.det.aud.avg_bps !=
				   port0_info.format.det.aud.avg_bps ||
			   peer_info.clock_rate != port0_info.clock_rate ||
			   peer_info.channel_count!=port0_info.channel_count);
	}

	if (need_reopen) {
	    if (pjsua_var.cap_dev != NULL_SND_DEV_ID) {
		pjmedia_snd_port_param param;

		/* Create parameter based on peer info */
		status = create_aud_param(&param.base, pjsua_var.cap_dev, 
					  pjsua_var.play_dev,
					  peer_info.clock_rate,
					  peer_info.channel_count,
					  peer_info.samples_per_frame,
					  peer_info.bits_per_sample);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error opening sound device",
				 status);
		    goto on_return;
		}

		/* And peer format */
		if (peer_info.format.id != PJMEDIA_FORMAT_PCM) {
		    param.base.flags |= PJMEDIA_AUD_DEV_CAP_EXT_FORMAT;
		    param.base.ext_fmt = peer_info.format;
		}

		param.options = 0;
		status = open_snd_dev(&param);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error opening sound device",
				 status);
		    goto on_return;
		}
	    } else {
		/* Null-audio */
		status = pjsua_set_snd_dev(pjsua_var.cap_dev,
					   pjsua_var.play_dev);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error opening sound device",
				 status);
		    goto on_return;
		}
	    }
	} else if (pjsua_var.no_snd) {
	    if (!pjsua_var.snd_is_on) {
		pjsua_var.snd_is_on = PJ_TRUE;
	    	/* Notify app */
	    	if (pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
	    	    (*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(1);
	    	}
	    }
	}

    } else {
	/* The bridge version */

	/* Create sound port if none is instantiated */
	if (pjsua_var.snd_port==NULL && pjsua_var.null_snd==NULL && 
	    !pjsua_var.no_snd) 
	{
	    pj_status_t status;

	    status = pjsua_set_snd_dev(pjsua_var.cap_dev, pjsua_var.play_dev);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Error opening sound device", status);
		goto on_return;
	    }
	} else if (pjsua_var.no_snd && !pjsua_var.snd_is_on) {
	    pjsua_var.snd_is_on = PJ_TRUE;
	    /* Notify app */
	    if (pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
		(*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(1);
	    }
	}
    }

    status = pjmedia_conf_connect_port(pjsua_var.mconf, source, sink, 0);

on_return:
    pj_log_pop_indent();
    return status;
}


/*
 * Disconnect media flow from the source to destination port.
 */
PJ_DEF(pj_status_t) pjsua_conf_disconnect( pjsua_conf_port_id source,
					   pjsua_conf_port_id sink)
{
    pj_status_t status;

    PJ_LOG(4,(THIS_FILE, "%s disconnect: %d -x- %d",
	      (pjsua_var.is_mswitch ? "Switch" : "Conf"),
	      source, sink));
    pj_log_push_indent();

    status = pjmedia_conf_disconnect_port(pjsua_var.mconf, source, sink);
    check_snd_dev_idle();

    pj_log_pop_indent();
    return status;
}


/*
 * Adjust the signal level to be transmitted from the bridge to the 
 * specified port by making it louder or quieter.
 */
PJ_DEF(pj_status_t) pjsua_conf_adjust_tx_level(pjsua_conf_port_id slot,
					       float level)
{
    return pjmedia_conf_adjust_tx_level(pjsua_var.mconf, slot,
					(int)((level-1) * 128));
}

/*
 * Adjust the signal level to be received from the specified port (to
 * the bridge) by making it louder or quieter.
 */
PJ_DEF(pj_status_t) pjsua_conf_adjust_rx_level(pjsua_conf_port_id slot,
					       float level)
{
    return pjmedia_conf_adjust_rx_level(pjsua_var.mconf, slot,
					(int)((level-1) * 128));
}


/*
 * Get last signal level transmitted to or received from the specified port.
 */
PJ_DEF(pj_status_t) pjsua_conf_get_signal_level(pjsua_conf_port_id slot,
						unsigned *tx_level,
						unsigned *rx_level)
{
    return pjmedia_conf_get_signal_level(pjsua_var.mconf, slot, 
					 tx_level, rx_level);
}

/*****************************************************************************
 * File player.
 */

static char* get_basename(const char *path, unsigned len)
{
    char *p = ((char*)path) + len;

    if (len==0)
	return p;

    for (--p; p!=path && *p!='/' && *p!='\\'; ) --p;

    return (p==path) ? p : p+1;
}


/*
 * Create a file player, and automatically connect this player to
 * the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_player_create( const pj_str_t *filename,
					 unsigned options,
					 pjsua_player_id *p_id)
{
    unsigned slot, file_id;
    char path[PJ_MAXPATH];
    pj_pool_t *pool = NULL;
    pjmedia_port *port;
    pj_status_t status = PJ_SUCCESS;

    if (pjsua_var.player_cnt >= PJ_ARRAY_SIZE(pjsua_var.player))
	return PJ_ETOOMANY;

    PJ_LOG(4,(THIS_FILE, "Creating file player: %.*s..",
	      (int)filename->slen, filename->ptr));
    pj_log_push_indent();

    PJSUA_LOCK();

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.player); ++file_id) {
	if (pjsua_var.player[file_id].port == NULL)
	    break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.player)) {
	/* This is unexpected */
	pj_assert(0);
	status = PJ_EBUG;
	goto on_error;
    }

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';

    pool = pjsua_pool_create(get_basename(path, filename->slen), 1000, 1000);
    if (!pool) {
	status = PJ_ENOMEM;
	goto on_error;
    }

    status = pjmedia_wav_player_port_create(
				    pool, path,
				    pjsua_var.mconf_cfg.samples_per_frame *
				    1000 / pjsua_var.media_cfg.channel_count /
				    pjsua_var.media_cfg.clock_rate, 
				    options, 0, &port);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to open file for playback", status);
	goto on_error;
    }

    status = pjmedia_conf_add_port(pjsua_var.mconf, pool, 
				   port, filename, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	pjsua_perror(THIS_FILE, "Unable to add file to conference bridge", 
		     status);
	goto on_error;
    }

    pjsua_var.player[file_id].type = 0;
    pjsua_var.player[file_id].pool = pool;
    pjsua_var.player[file_id].port = port;
    pjsua_var.player[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var.player_cnt;

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Player created, id=%d, slot=%d", file_id, slot));

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    PJSUA_UNLOCK();
    if (pool) pj_pool_release(pool);
    pj_log_pop_indent();
    return status;
}


/*
 * Create a file playlist media port, and automatically add the port
 * to the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_playlist_create( const pj_str_t file_names[],
					   unsigned file_count,
					   const pj_str_t *label,
					   unsigned options,
					   pjsua_player_id *p_id)
{
    unsigned slot, file_id, ptime;
    pj_pool_t *pool = NULL;
    pjmedia_port *port;
    pj_status_t status = PJ_SUCCESS;

    if (pjsua_var.player_cnt >= PJ_ARRAY_SIZE(pjsua_var.player))
	return PJ_ETOOMANY;

    PJ_LOG(4,(THIS_FILE, "Creating playlist with %d file(s)..", file_count));
    pj_log_push_indent();

    PJSUA_LOCK();

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.player); ++file_id) {
	if (pjsua_var.player[file_id].port == NULL)
	    break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.player)) {
	/* This is unexpected */
	pj_assert(0);
	status = PJ_EBUG;
	goto on_error;
    }


    ptime = pjsua_var.mconf_cfg.samples_per_frame * 1000 / 
	    pjsua_var.media_cfg.clock_rate;

    pool = pjsua_pool_create("playlist", 1000, 1000);
    if (!pool) {
	status = PJ_ENOMEM;
	goto on_error;
    }

    status = pjmedia_wav_playlist_create(pool, label, 
					 file_names, file_count,
					 ptime, options, 0, &port);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create playlist", status);
	goto on_error;
    }

    status = pjmedia_conf_add_port(pjsua_var.mconf, pool, 
				   port, &port->info.name, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	pjsua_perror(THIS_FILE, "Unable to add port", status);
	goto on_error;
    }

    pjsua_var.player[file_id].type = 1;
    pjsua_var.player[file_id].pool = pool;
    pjsua_var.player[file_id].port = port;
    pjsua_var.player[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var.player_cnt;

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Playlist created, id=%d, slot=%d", file_id, slot));

    pj_log_pop_indent();

    return PJ_SUCCESS;

on_error:
    PJSUA_UNLOCK();
    if (pool) pj_pool_release(pool);
    pj_log_pop_indent();

    return status;
}


/*
 * Get conference port ID associated with player.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_player_get_conf_port(pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);

    return pjsua_var.player[id].slot;
}

/*
 * Get the media port for the player.
 */
PJ_DEF(pj_status_t) pjsua_player_get_port( pjsua_player_id id,
					   pjmedia_port **p_port)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(p_port != NULL, PJ_EINVAL);
    
    *p_port = pjsua_var.player[id].port;

    return PJ_SUCCESS;
}

/*
 * Set playback position.
 */
PJ_DEF(pj_status_t) pjsua_player_set_pos( pjsua_player_id id,
					  pj_uint32_t samples)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].type == 0, PJ_EINVAL);

    return pjmedia_wav_player_port_set_pos(pjsua_var.player[id].port, samples);
}


/*
 * Close the file, remove the player from the bridge, and free
 * resources associated with the file player.
 */
PJ_DEF(pj_status_t) pjsua_player_destroy(pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var.player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Destroying player %d..", id));
    pj_log_push_indent();

    PJSUA_LOCK();

    if (pjsua_var.player[id].port) {
	pjsua_conf_remove_port(pjsua_var.player[id].slot);
	pjmedia_port_destroy(pjsua_var.player[id].port);
	pjsua_var.player[id].port = NULL;
	pjsua_var.player[id].slot = 0xFFFF;
	pj_pool_release(pjsua_var.player[id].pool);
	pjsua_var.player[id].pool = NULL;
	pjsua_var.player_cnt--;
    }

    PJSUA_UNLOCK();
    pj_log_pop_indent();

    return PJ_SUCCESS;
}


/*****************************************************************************
 * File recorder.
 */

/*
 * Create a file recorder, and automatically connect this recorder to
 * the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_recorder_create( const pj_str_t *filename,
					   unsigned enc_type,
					   void *enc_param,
					   pj_ssize_t max_size,
					   unsigned options,
					   pjsua_recorder_id *p_id)
{
    enum Format
    {
	FMT_UNKNOWN,
	FMT_WAV,
	FMT_MP3,
    };
    unsigned slot, file_id;
    char path[PJ_MAXPATH];
    pj_str_t ext;
    int file_format;
    pj_pool_t *pool = NULL;
    pjmedia_port *port;
    pj_status_t status = PJ_SUCCESS;

    /* Filename must present */
    PJ_ASSERT_RETURN(filename != NULL, PJ_EINVAL);

    /* Don't support max_size at present */
    PJ_ASSERT_RETURN(max_size == 0 || max_size == -1, PJ_EINVAL);

    /* Don't support encoding type at present */
    PJ_ASSERT_RETURN(enc_type == 0, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Creating recorder %.*s..",
	      (int)filename->slen, filename->ptr));
    pj_log_push_indent();

    if (pjsua_var.rec_cnt >= PJ_ARRAY_SIZE(pjsua_var.recorder)) {
	pj_log_pop_indent();
	return PJ_ETOOMANY;
    }

    /* Determine the file format */
    ext.ptr = filename->ptr + filename->slen - 4;
    ext.slen = 4;

    if (pj_stricmp2(&ext, ".wav") == 0)
	file_format = FMT_WAV;
    else if (pj_stricmp2(&ext, ".mp3") == 0)
	file_format = FMT_MP3;
    else {
	PJ_LOG(1,(THIS_FILE, "pjsua_recorder_create() error: unable to "
			     "determine file format for %.*s",
			     (int)filename->slen, filename->ptr));
	pj_log_pop_indent();
	return PJ_ENOTSUP;
    }

    PJSUA_LOCK();

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.recorder); ++file_id) {
	if (pjsua_var.recorder[file_id].port == NULL)
	    break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.recorder)) {
	/* This is unexpected */
	pj_assert(0);
	status = PJ_EBUG;
	goto on_return;
    }

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';

    pool = pjsua_pool_create(get_basename(path, filename->slen), 1000, 1000);
    if (!pool) {
	status = PJ_ENOMEM;
	goto on_return;
    }

    if (file_format == FMT_WAV) {
	status = pjmedia_wav_writer_port_create(pool, path, 
						pjsua_var.media_cfg.clock_rate, 
						pjsua_var.mconf_cfg.channel_count,
						pjsua_var.mconf_cfg.samples_per_frame,
						pjsua_var.mconf_cfg.bits_per_sample, 
						options, 0, &port);
    } else {
	PJ_UNUSED_ARG(enc_param);
	port = NULL;
	status = PJ_ENOTSUP;
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to open file for recording", status);
	goto on_return;
    }

    status = pjmedia_conf_add_port(pjsua_var.mconf, pool, 
				   port, filename, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	goto on_return;
    }

    pjsua_var.recorder[file_id].port = port;
    pjsua_var.recorder[file_id].slot = slot;
    pjsua_var.recorder[file_id].pool = pool;

    if (p_id) *p_id = file_id;

    ++pjsua_var.rec_cnt;

    PJSUA_UNLOCK();

    PJ_LOG(4,(THIS_FILE, "Recorder created, id=%d, slot=%d", file_id, slot));

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_return:
    PJSUA_UNLOCK();
    if (pool) pj_pool_release(pool);
    pj_log_pop_indent();
    return status;
}


/*
 * Get conference port associated with recorder.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_recorder_get_conf_port(pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.recorder), 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.recorder[id].port != NULL, PJ_EINVAL);

    return pjsua_var.recorder[id].slot;
}

/*
 * Get the media port for the recorder.
 */
PJ_DEF(pj_status_t) pjsua_recorder_get_port( pjsua_recorder_id id,
					     pjmedia_port **p_port)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.recorder), 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.recorder[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(p_port != NULL, PJ_EINVAL);

    *p_port = pjsua_var.recorder[id].port;
    return PJ_SUCCESS;
}

/*
 * Destroy recorder (this will complete recording).
 */
PJ_DEF(pj_status_t) pjsua_recorder_destroy(pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var.recorder), 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.recorder[id].port != NULL, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Destroying recorder %d..", id));
    pj_log_push_indent();

    PJSUA_LOCK();

    if (pjsua_var.recorder[id].port) {
	pjsua_conf_remove_port(pjsua_var.recorder[id].slot);
	pjmedia_port_destroy(pjsua_var.recorder[id].port);
	pjsua_var.recorder[id].port = NULL;
	pjsua_var.recorder[id].slot = 0xFFFF;
	pj_pool_release(pjsua_var.recorder[id].pool);
	pjsua_var.recorder[id].pool = NULL;
	pjsua_var.rec_cnt--;
    }

    PJSUA_UNLOCK();
    pj_log_pop_indent();

    return PJ_SUCCESS;
}


/*****************************************************************************
 * Sound devices.
 */

/*
 * Enum sound devices.
 */

PJ_DEF(pj_status_t) pjsua_enum_aud_devs( pjmedia_aud_dev_info info[],
					 unsigned *count)
{
    unsigned i, dev_count;

    dev_count = pjmedia_aud_dev_count();
    
    if (dev_count > *count) dev_count = *count;

    for (i=0; i<dev_count; ++i) {
	pj_status_t status;

	status = pjmedia_aud_dev_get_info(i, &info[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    *count = dev_count;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_enum_snd_devs( pjmedia_snd_dev_info info[],
					 unsigned *count)
{
    unsigned i, dev_count;

    dev_count = pjmedia_aud_dev_count();
    
    if (dev_count > *count) dev_count = *count;
    pj_bzero(info, dev_count * sizeof(pjmedia_snd_dev_info));

    for (i=0; i<dev_count; ++i) {
	pjmedia_aud_dev_info ai;
	pj_status_t status;

	status = pjmedia_aud_dev_get_info(i, &ai);
	if (status != PJ_SUCCESS)
	    return status;

	strncpy(info[i].name, ai.name, sizeof(info[i].name));
	info[i].name[sizeof(info[i].name)-1] = '\0';
	info[i].input_count = ai.input_count;
	info[i].output_count = ai.output_count;
	info[i].default_samples_per_sec = ai.default_samples_per_sec;
    }

    *count = dev_count;

    return PJ_SUCCESS;
}

/* Create audio device parameter to open the device */
static pj_status_t create_aud_param(pjmedia_aud_param *param,
				    pjmedia_aud_dev_index capture_dev,
				    pjmedia_aud_dev_index playback_dev,
				    unsigned clock_rate,
				    unsigned channel_count,
				    unsigned samples_per_frame,
				    unsigned bits_per_sample)
{
    pj_status_t status;

    /* Normalize device ID with new convention about default device ID */
    if (playback_dev == PJMEDIA_AUD_DEFAULT_CAPTURE_DEV)
	playback_dev = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

    /* Create default parameters for the device */
    status = pjmedia_aud_dev_default_param(capture_dev, param);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error retrieving default audio "
				"device parameters", status);
	return status;
    }
    param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param->rec_id = capture_dev;
    param->play_id = playback_dev;
    param->clock_rate = clock_rate;
    param->channel_count = channel_count;
    param->samples_per_frame = samples_per_frame;
    param->bits_per_sample = bits_per_sample;

    /* Update the setting with user preference */
#define update_param(cap, field)    \
	if (pjsua_var.aud_param.flags & cap) { \
	    param->flags |= cap; \
	    param->field = pjsua_var.aud_param.field; \
	}
    update_param( PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, input_vol);
    update_param( PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, output_vol);
    update_param( PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE, input_route);
    update_param( PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE, output_route);
#undef update_param

    /* Latency settings */
    param->flags |= (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY | 
		     PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY);
    param->input_latency_ms = pjsua_var.media_cfg.snd_rec_latency;
    param->output_latency_ms = pjsua_var.media_cfg.snd_play_latency;

    /* EC settings */
    if (pjsua_var.media_cfg.ec_tail_len) {
	param->flags |= (PJMEDIA_AUD_DEV_CAP_EC | PJMEDIA_AUD_DEV_CAP_EC_TAIL);
	param->ec_enabled = PJ_TRUE;
	param->ec_tail_ms = pjsua_var.media_cfg.ec_tail_len;
    } else {
	param->flags &= ~(PJMEDIA_AUD_DEV_CAP_EC|PJMEDIA_AUD_DEV_CAP_EC_TAIL);
    }

    return PJ_SUCCESS;
}

/* Internal: the first time the audio device is opened (during app
 *   startup), retrieve the audio settings such as volume level
 *   so that aud_get_settings() will work.
 */
static pj_status_t update_initial_aud_param()
{
    pjmedia_aud_stream *strm;
    pjmedia_aud_param param;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_var.snd_port != NULL, PJ_EBUG);

    strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);

    status = pjmedia_aud_stream_get_param(strm, &param);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error audio stream "
				"device parameters", status);
	return status;
    }

#define update_saved_param(cap, field)  \
	if (param.flags & cap) { \
	    pjsua_var.aud_param.flags |= cap; \
	    pjsua_var.aud_param.field = param.field; \
	}

    update_saved_param(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, input_vol);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, output_vol);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE, input_route);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE, output_route);
#undef update_saved_param

    return PJ_SUCCESS;
}

/* Get format name */
static const char *get_fmt_name(pj_uint32_t id)
{
    static char name[8];

    if (id == PJMEDIA_FORMAT_L16)
	return "PCM";
    pj_memcpy(name, &id, 4);
    name[4] = '\0';
    return name;
}

/* Open sound device with the setting. */
static pj_status_t open_snd_dev(pjmedia_snd_port_param *param)
{
    pjmedia_port *conf_port;
    pj_status_t status;

    PJ_ASSERT_RETURN(param, PJ_EINVAL);

    /* Check if NULL sound device is used */
    if (NULL_SND_DEV_ID==param->base.rec_id ||
	NULL_SND_DEV_ID==param->base.play_id)
    {
	return pjsua_set_null_snd_dev();
    }

    /* Close existing sound port */
    close_snd_dev();

    /* Notify app */
    if (pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
	(*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(1);
    }

    /* Create memory pool for sound device. */
    pjsua_var.snd_pool = pjsua_pool_create("pjsua_snd", 4000, 4000);
    PJ_ASSERT_RETURN(pjsua_var.snd_pool, PJ_ENOMEM);


    PJ_LOG(4,(THIS_FILE, "Opening sound device %s@%d/%d/%dms",
	      get_fmt_name(param->base.ext_fmt.id),
	      param->base.clock_rate, param->base.channel_count,
	      param->base.samples_per_frame / param->base.channel_count *
	      1000 / param->base.clock_rate));
    pj_log_push_indent();

    status = pjmedia_snd_port_create2( pjsua_var.snd_pool, 
				       param, &pjsua_var.snd_port);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Get the port0 of the conference bridge. */
    conf_port = pjmedia_conf_get_master_port(pjsua_var.mconf);
    pj_assert(conf_port != NULL);

    /* For conference bridge, resample if necessary if the bridge's
     * clock rate is different than the sound device's clock rate.
     */
    if (!pjsua_var.is_mswitch &&
	param->base.ext_fmt.id == PJMEDIA_FORMAT_PCM &&
	PJMEDIA_PIA_SRATE(&conf_port->info) != param->base.clock_rate)
    {
	pjmedia_port *resample_port;
	unsigned resample_opt = 0;

	if (pjsua_var.media_cfg.quality >= 3 &&
	    pjsua_var.media_cfg.quality <= 4)
	{
	    resample_opt |= PJMEDIA_RESAMPLE_USE_SMALL_FILTER;
	}
	else if (pjsua_var.media_cfg.quality < 3) {
	    resample_opt |= PJMEDIA_RESAMPLE_USE_LINEAR;
	}
	
	status = pjmedia_resample_port_create(pjsua_var.snd_pool, 
					      conf_port,
					      param->base.clock_rate,
					      resample_opt, 
					      &resample_port);
	if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4, (THIS_FILE, 
		       "Error creating resample port: %s", 
		       errmsg));
	    close_snd_dev();
	    goto on_error;
	} 
	    
	conf_port = resample_port;
    }

    /* Otherwise for audio switchboard, the switch's port0 setting is
     * derived from the sound device setting, so update the setting.
     */
    if (pjsua_var.is_mswitch) {
	pj_memcpy(&conf_port->info.fmt, &param->base.ext_fmt,
		  sizeof(conf_port->info.fmt));
	conf_port->info.fmt.det.aud.clock_rate = param->base.clock_rate;
	conf_port->info.fmt.det.aud.frame_time_usec = param->base.samples_per_frame*
						      1000000 /
						      param->base.clock_rate;
	conf_port->info.fmt.det.aud.channel_count = param->base.channel_count;
	conf_port->info.fmt.det.aud.bits_per_sample = 16;
    }


    /* Connect sound port to the bridge */
    status = pjmedia_snd_port_connect(pjsua_var.snd_port, 	 
				      conf_port ); 	 
    if (status != PJ_SUCCESS) { 	 
	pjsua_perror(THIS_FILE, "Unable to connect conference port to "
			        "sound device", status); 	 
	pjmedia_snd_port_destroy(pjsua_var.snd_port); 	 
	pjsua_var.snd_port = NULL; 	 
	goto on_error;
    }

    /* Save the device IDs */
    pjsua_var.cap_dev = param->base.rec_id;
    pjsua_var.play_dev = param->base.play_id;

    /* Update sound device name. */
    {
	pjmedia_aud_dev_info rec_info;
	pjmedia_aud_stream *strm;
	pjmedia_aud_param si;
        pj_str_t tmp;

	strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);
	status = pjmedia_aud_stream_get_param(strm, &si);
	if (status == PJ_SUCCESS)
	    status = pjmedia_aud_dev_get_info(si.rec_id, &rec_info);

	if (status==PJ_SUCCESS) {
	    if (param->base.clock_rate != pjsua_var.media_cfg.clock_rate) {
		char tmp_buf[128];
		int tmp_buf_len = sizeof(tmp_buf);

		tmp_buf_len = pj_ansi_snprintf(tmp_buf, sizeof(tmp_buf)-1, 
					       "%s (%dKHz)",
					       rec_info.name, 
					       param->base.clock_rate/1000);
		pj_strset(&tmp, tmp_buf, tmp_buf_len);
		pjmedia_conf_set_port0_name(pjsua_var.mconf, &tmp); 
	    } else {
		pjmedia_conf_set_port0_name(pjsua_var.mconf, 
					    pj_cstr(&tmp, rec_info.name));
	    }
	}

	/* Any error is not major, let it through */
	status = PJ_SUCCESS;
    }

    /* If this is the first time the audio device is open, retrieve some
     * settings from the device (such as volume settings) so that the
     * pjsua_snd_get_setting() work.
     */
    if (pjsua_var.aud_open_cnt == 0) {
	update_initial_aud_param();
	++pjsua_var.aud_open_cnt;
    }

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    pj_log_pop_indent();
    return status;
}


/* Close existing sound device */
static void close_snd_dev(void)
{
    pj_log_push_indent();

    /* Notify app */
    if (pjsua_var.snd_is_on && pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
	(*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(0);
    }

    /* Close sound device */
    if (pjsua_var.snd_port) {
	pjmedia_aud_dev_info cap_info, play_info;
	pjmedia_aud_stream *strm;
	pjmedia_aud_param param;

	strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);
	pjmedia_aud_stream_get_param(strm, &param);

	if (pjmedia_aud_dev_get_info(param.rec_id, &cap_info) != PJ_SUCCESS)
	    cap_info.name[0] = '\0';
	if (pjmedia_aud_dev_get_info(param.play_id, &play_info) != PJ_SUCCESS)
	    play_info.name[0] = '\0';

	PJ_LOG(4,(THIS_FILE, "Closing %s sound playback device and "
			     "%s sound capture device",
			     play_info.name, cap_info.name));

	pjmedia_snd_port_disconnect(pjsua_var.snd_port);
	pjmedia_snd_port_destroy(pjsua_var.snd_port);
	pjsua_var.snd_port = NULL;
    }

    /* Close null sound device */
    if (pjsua_var.null_snd) {
	PJ_LOG(4,(THIS_FILE, "Closing null sound device.."));
	pjmedia_master_port_destroy(pjsua_var.null_snd, PJ_FALSE);
	pjsua_var.null_snd = NULL;
    }

    if (pjsua_var.snd_pool) 
	pj_pool_release(pjsua_var.snd_pool);
    pjsua_var.snd_pool = NULL;
    pjsua_var.snd_is_on = PJ_FALSE;

    pj_log_pop_indent();
}


/*
 * Select or change sound device. Application may call this function at
 * any time to replace current sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_snd_dev( int capture_dev,
				       int playback_dev)
{
    unsigned alt_cr_cnt = 1;
    unsigned alt_cr[] = {0, 44100, 48000, 32000, 16000, 8000};
    unsigned i;
    pj_status_t status = -1;

    PJ_LOG(4,(THIS_FILE, "Set sound device: capture=%d, playback=%d",
	      capture_dev, playback_dev));
    pj_log_push_indent();

    /* Null-sound */
    if (capture_dev==NULL_SND_DEV_ID && playback_dev==NULL_SND_DEV_ID) {
	status = pjsua_set_null_snd_dev();
	pj_log_pop_indent();
	return status;
    }

    /* Set default clock rate */
    alt_cr[0] = pjsua_var.media_cfg.snd_clock_rate;
    if (alt_cr[0] == 0)
	alt_cr[0] = pjsua_var.media_cfg.clock_rate;

    /* Allow retrying of different clock rate if we're using conference 
     * bridge (meaning audio format is always PCM), otherwise lock on
     * to one clock rate.
     */
    if (pjsua_var.is_mswitch) {
	alt_cr_cnt = 1;
    } else {
	alt_cr_cnt = PJ_ARRAY_SIZE(alt_cr);
    }

    /* Attempts to open the sound device with different clock rates */
    for (i=0; i<alt_cr_cnt; ++i) {
	pjmedia_snd_port_param param;
	unsigned samples_per_frame;

	/* Create the default audio param */
	samples_per_frame = alt_cr[i] *
			    pjsua_var.media_cfg.audio_frame_ptime *
			    pjsua_var.media_cfg.channel_count / 1000;
	status = create_aud_param(&param.base, capture_dev, playback_dev, 
				  alt_cr[i], pjsua_var.media_cfg.channel_count,
				  samples_per_frame, 16);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Open! */
	param.options = 0;
	status = open_snd_dev(&param);
	if (status == PJ_SUCCESS)
	    break;
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to open sound device", status);
	goto on_error;
    }

    pjsua_var.no_snd = PJ_FALSE;
    pjsua_var.snd_is_on = PJ_TRUE;

    pj_log_pop_indent();
    return PJ_SUCCESS;

on_error:
    pj_log_pop_indent();
    return status;
}


/*
 * Get currently active sound devices. If sound devices has not been created
 * (for example when pjsua_start() is not called), it is possible that
 * the function returns PJ_SUCCESS with -1 as device IDs.
 */
PJ_DEF(pj_status_t) pjsua_get_snd_dev(int *capture_dev,
				      int *playback_dev)
{
    if (capture_dev) {
	*capture_dev = pjsua_var.cap_dev;
    }
    if (playback_dev) {
	*playback_dev = pjsua_var.play_dev;
    }

    return PJ_SUCCESS;
}


/*
 * Use null sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_null_snd_dev(void)
{
    pjmedia_port *conf_port;
    pj_status_t status;

    PJ_LOG(4,(THIS_FILE, "Setting null sound device.."));
    pj_log_push_indent();


    /* Close existing sound device */
    close_snd_dev();

    /* Notify app */
    if (pjsua_var.ua_cfg.cb.on_snd_dev_operation) {
	(*pjsua_var.ua_cfg.cb.on_snd_dev_operation)(1);
    }

    /* Create memory pool for sound device. */
    pjsua_var.snd_pool = pjsua_pool_create("pjsua_snd", 4000, 4000);
    PJ_ASSERT_RETURN(pjsua_var.snd_pool, PJ_ENOMEM);

    PJ_LOG(4,(THIS_FILE, "Opening null sound device.."));

    /* Get the port0 of the conference bridge. */
    conf_port = pjmedia_conf_get_master_port(pjsua_var.mconf);
    pj_assert(conf_port != NULL);

    /* Create master port, connecting port0 of the conference bridge to
     * a null port.
     */
    status = pjmedia_master_port_create(pjsua_var.snd_pool, pjsua_var.null_port,
					conf_port, 0, &pjsua_var.null_snd);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create null sound device",
		     status);
	pj_log_pop_indent();
	return status;
    }

    /* Start the master port */
    status = pjmedia_master_port_start(pjsua_var.null_snd);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    pjsua_var.cap_dev = NULL_SND_DEV_ID;
    pjsua_var.play_dev = NULL_SND_DEV_ID;

    pjsua_var.no_snd = PJ_FALSE;
    pjsua_var.snd_is_on = PJ_TRUE;

    pj_log_pop_indent();
    return PJ_SUCCESS;
}



/*
 * Use no device!
 */
PJ_DEF(pjmedia_port*) pjsua_set_no_snd_dev(void)
{
    /* Close existing sound device */
    close_snd_dev();

    pjsua_var.no_snd = PJ_TRUE;
    return pjmedia_conf_get_master_port(pjsua_var.mconf);
}


/*
 * Configure the AEC settings of the sound port.
 */
PJ_DEF(pj_status_t) pjsua_set_ec(unsigned tail_ms, unsigned options)
{
    pjsua_var.media_cfg.ec_tail_len = tail_ms;

    if (pjsua_var.snd_port)
	return pjmedia_snd_port_set_ec( pjsua_var.snd_port, pjsua_var.pool,
					tail_ms, options);
    
    return PJ_SUCCESS;
}


/*
 * Get current AEC tail length.
 */
PJ_DEF(pj_status_t) pjsua_get_ec_tail(unsigned *p_tail_ms)
{
    *p_tail_ms = pjsua_var.media_cfg.ec_tail_len;
    return PJ_SUCCESS;
}


/*
 * Check whether the sound device is currently active.
 */
PJ_DEF(pj_bool_t) pjsua_snd_is_active(void)
{
    return pjsua_var.snd_port != NULL;
}


/*
 * Configure sound device setting to the sound device being used. 
 */
PJ_DEF(pj_status_t) pjsua_snd_set_setting( pjmedia_aud_dev_cap cap,
					   const void *pval,
					   pj_bool_t keep)
{
    pj_status_t status;

    /* Check if we are allowed to set the cap */
    if ((cap & pjsua_var.aud_svmask) == 0) {
	return PJMEDIA_EAUD_INVCAP;
    }

    /* If sound is active, set it immediately */
    if (pjsua_snd_is_active()) {
	pjmedia_aud_stream *strm;
	
	strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);
	status = pjmedia_aud_stream_set_cap(strm, cap, pval);
    } else {
	status = PJ_SUCCESS;
    }

    if (status != PJ_SUCCESS)
	return status;

    /* Save in internal param for later device open */
    if (keep) {
	status = pjmedia_aud_param_set_cap(&pjsua_var.aud_param,
					   cap, pval);
    }

    return status;
}

/*
 * Retrieve a sound device setting.
 */
PJ_DEF(pj_status_t) pjsua_snd_get_setting( pjmedia_aud_dev_cap cap,
					   void *pval)
{
    /* If sound device has never been opened before, open it to 
     * retrieve the initial setting from the device (e.g. audio
     * volume)
     */
    if (pjsua_var.aud_open_cnt==0) {
	PJ_LOG(4,(THIS_FILE, "Opening sound device to get initial settings"));
	pjsua_set_snd_dev(pjsua_var.cap_dev, pjsua_var.play_dev);
	close_snd_dev();
    }

    if (pjsua_snd_is_active()) {
	/* Sound is active, retrieve from device directly */
	pjmedia_aud_stream *strm;
	
	strm = pjmedia_snd_port_get_snd_stream(pjsua_var.snd_port);
	return pjmedia_aud_stream_get_cap(strm, cap, pval);
    } else {
	/* Otherwise retrieve from internal param */
	return pjmedia_aud_param_get_cap(&pjsua_var.aud_param,
					 cap, pval);
    }
}


/*****************************************************************************
 * Codecs.
 */

/*
 * Enum all supported codecs in the system.
 */
PJ_DEF(pj_status_t) pjsua_enum_codecs( pjsua_codec_info id[],
				       unsigned *p_count )
{
    pjmedia_codec_mgr *codec_mgr;
    pjmedia_codec_info info[32];
    unsigned i, count, prio[32];
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);
    count = PJ_ARRAY_SIZE(info);
    status = pjmedia_codec_mgr_enum_codecs( codec_mgr, &count, info, prio);
    if (status != PJ_SUCCESS) {
	*p_count = 0;
	return status;
    }

    if (count > *p_count) count = *p_count;

    for (i=0; i<count; ++i) {
	pj_bzero(&id[i], sizeof(pjsua_codec_info));

	pjmedia_codec_info_to_id(&info[i], id[i].buf_, sizeof(id[i].buf_));
	id[i].codec_id = pj_str(id[i].buf_);
	id[i].priority = (pj_uint8_t) prio[i];
    }

    *p_count = count;

    return PJ_SUCCESS;
}


/*
 * Change codec priority.
 */
PJ_DEF(pj_status_t) pjsua_codec_set_priority( const pj_str_t *codec_id,
					      pj_uint8_t priority )
{
    const pj_str_t all = { NULL, 0 };
    pjmedia_codec_mgr *codec_mgr;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

    if (codec_id->slen==1 && *codec_id->ptr=='*')
	codec_id = &all;

    return pjmedia_codec_mgr_set_codec_priority(codec_mgr, codec_id, 
					        priority);
}


/*
 * Get codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_codec_get_param( const pj_str_t *codec_id,
					   pjmedia_codec_param *param )
{
    const pj_str_t all = { NULL, 0 };
    const pjmedia_codec_info *info;
    pjmedia_codec_mgr *codec_mgr;
    unsigned count = 1;
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

    if (codec_id->slen==1 && *codec_id->ptr=='*')
	codec_id = &all;

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, codec_id,
						 &count, &info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    if (count != 1)
	return (count > 1? PJ_ETOOMANY : PJ_ENOTFOUND);

    status = pjmedia_codec_mgr_get_default_param( codec_mgr, info, param);
    return status;
}


/*
 * Set codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_codec_set_param( const pj_str_t *codec_id,
					   const pjmedia_codec_param *param)
{
    const pjmedia_codec_info *info[2];
    pjmedia_codec_mgr *codec_mgr;
    unsigned count = 2;
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, codec_id,
						 &count, info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    /* Codec ID should be specific, except for G.722.1 */
    if (count > 1 && 
	pj_strnicmp2(codec_id, "G7221/16", 8) != 0 &&
	pj_strnicmp2(codec_id, "G7221/32", 8) != 0)
    {
	pj_assert(!"Codec ID is not specific");
	return PJ_ETOOMANY;
    }

    status = pjmedia_codec_mgr_set_default_param(codec_mgr, info[0], param);
    return status;
}



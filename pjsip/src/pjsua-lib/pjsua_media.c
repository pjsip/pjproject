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


#define THIS_FILE		"pjsua_media.c"

#define PTIME		    10
#define FPS		    (1000/PTIME)
#define DEFAULT_RTP_PORT    4000


/* Close existing sound device */
static void close_snd_dev(void);


/**
 * Init media subsystems.
 */
pj_status_t pjsua_media_subsys_init(const pjsua_media_config *cfg)
{
    pj_str_t codec_id;
    unsigned opt;
    pj_status_t status;

    /* Copy configuration */
    pj_memcpy(&pjsua_var.media_cfg, cfg, sizeof(*cfg));

    /* Normalize configuration */
#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    pjsua_var.media_cfg.clock_rate = 44100;
#endif

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
	return status;
    }

    /* Register all codecs */
#if PJMEDIA_HAS_SPEEX_CODEC
    /* Register speex. */
    status = pjmedia_codec_speex_init(pjsua_var.med_endpt, 
				      PJMEDIA_SPEEX_NO_UWB,
				      pjsua_var.media_cfg.quality, 
				      pjsua_var.media_cfg.quality);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing Speex codec",
		     status);
	return status;
    }
#endif /* PJMEDIA_HAS_SPEEX_CODEC */

#if PJMEDIA_HAS_GSM_CODEC
    /* Register GSM */
    status = pjmedia_codec_gsm_init(pjsua_var.med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing GSM codec",
		     status);
	return status;
    }
#endif /* PJMEDIA_HAS_GSM_CODEC */

#if PJMEDIA_HAS_G711_CODEC
    /* Register PCMA and PCMU */
    status = pjmedia_codec_g711_init(pjsua_var.med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing G711 codec",
		     status);
	return status;
    }
#endif	/* PJMEDIA_HAS_G711_CODEC */

#if PJMEDIA_HAS_L16_CODEC
    /* Register L16 family codecs, but disable all */
    status = pjmedia_codec_l16_init(pjsua_var.med_endpt, 0);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing L16 codecs",
		     status);
	return status;
    }

    /* Disable ALL L16 codecs */
    codec_id = pj_str("L16");
    pjmedia_codec_mgr_set_codec_priority( 
	pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
	&codec_id, PJMEDIA_CODEC_PRIO_DISABLED);

#endif	/* PJMEDIA_HAS_L16_CODEC */


    /* Save additional conference bridge parameters for future
     * reference.
     */
    pjsua_var.mconf_cfg.samples_per_frame = pjsua_var.media_cfg.clock_rate * 
					    PTIME / 1000;
    pjsua_var.mconf_cfg.channel_count = 1;
    pjsua_var.mconf_cfg.bits_per_sample = 16;

    /* Init options for conference bridge. */
    opt = PJMEDIA_CONF_NO_DEVICE;
    if (pjsua_var.media_cfg.quality >= 3 &&
	pjsua_var.media_cfg.quality <= 7)
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
	pjsua_perror(THIS_FILE, 
		     "Media stack initialization has returned error", 
		     status);
	return status;
    }

    /* Create null port just in case user wants to use null sound. */
    status = pjmedia_null_port_create(pjsua_var.pool, 
				      pjsua_var.media_cfg.clock_rate,
				      pjsua_var.mconf_cfg.channel_count,
				      pjsua_var.mconf_cfg.samples_per_frame,
				      pjsua_var.mconf_cfg.bits_per_sample,
				      &pjsua_var.null_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

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
    static pj_uint16_t rtp_port;
    pj_sockaddr_in mapped_addr[2];
    pj_status_t status = PJ_SUCCESS;
    pj_sock_t sock[2];

    if (rtp_port == 0)
	rtp_port = (pj_uint16_t)cfg->port;

    for (i=0; i<2; ++i)
	sock[i] = PJ_INVALID_SOCKET;


    /* Loop retry to bind RTP and RTCP sockets. */
    for (i=0; i<RTP_RETRY; ++i, rtp_port += 2) {

	/* Create and bind RTP socket. */
	status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock[0]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    return status;
	}

	status = pj_sock_bind_in(sock[0], cfg->ip_addr.s_addr, rtp_port);
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[0]); 
	    sock[0] = PJ_INVALID_SOCKET;
	    continue;
	}

	/* Create and bind RTCP socket. */
	status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock[1]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    pj_sock_close(sock[0]);
	    return status;
	}

	status = pj_sock_bind_in(sock[1], cfg->ip_addr.s_addr, 
				 (pj_uint16_t)(rtp_port+1));
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
	if (cfg->stun_config.stun_srv1.slen) {
	    status=pj_stun_get_mapped_addr(&pjsua_var.cp.factory, 2, sock,
					   &cfg->stun_config.stun_srv1, 
					   cfg->stun_config.stun_port1,
					   &cfg->stun_config.stun_srv2, 
					   cfg->stun_config.stun_port2,
					   mapped_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "STUN resolve error", status);
		goto on_error;
	    }

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

	} else {
	    const pj_str_t *hostname;
	    pj_sockaddr_in addr;

	    /* Get local IP address. */
	    hostname = pj_gethostname();

	    pj_memset( &addr, 0, sizeof(addr));
	    addr.sin_family = PJ_AF_INET;
	    status = pj_sockaddr_in_set_str_addr( &addr, hostname);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unresolvable local hostname", 
			     status);
		goto on_error;
	    }

	    for (i=0; i<2; ++i)
		pj_memcpy(&mapped_addr[i], &addr, sizeof(addr));

	    mapped_addr[0].sin_port=pj_htons((pj_uint16_t)rtp_port);
	    mapped_addr[1].sin_port=pj_htons((pj_uint16_t)(rtp_port+1));
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

    PJ_LOG(4,(THIS_FILE, "RTP socket reachable at %s:%d",
	      pj_inet_ntoa(skinfo->rtp_addr_name.sin_addr), 
	      pj_ntohs(skinfo->rtp_addr_name.sin_port)));
    PJ_LOG(4,(THIS_FILE, "RTCP socket reachable at %s:%d",
	      pj_inet_ntoa(skinfo->rtcp_addr_name.sin_addr), 
	      pj_ntohs(skinfo->rtcp_addr_name.sin_port)));

    rtp_port += 2;
    return PJ_SUCCESS;

on_error:
    for (i=0; i<2; ++i) {
	if (sock[i] != PJ_INVALID_SOCKET)
	    pj_sock_close(sock[i]);
    }
    return status;
}


/*
 * Start pjsua media subsystem.
 */
pj_status_t pjsua_media_subsys_start(void)
{
    pj_status_t status;

    /* Create media for calls, if none is specified */
    if (pjsua_var.calls[0].med_tp == NULL) {
	pjsua_transport_config transport_cfg;

	/* Create default transport config */
	pjsua_transport_config_default(&transport_cfg);
	transport_cfg.port = DEFAULT_RTP_PORT;

	status = pjsua_media_transports_create(&transport_cfg);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Create sound port if none is created yet */
    if (pjsua_var.snd_port==NULL && pjsua_var.null_snd==NULL) {
	status = pjsua_set_snd_dev(pjsua_var.cap_dev, pjsua_var.play_dev);
	if (status != PJ_SUCCESS) {
	    /* Error opening sound device, use null device */
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(THIS_FILE, 
		      "Error opening default sound device (%s (status=%d)). "
		      "Will use NULL device instead",
		      errmsg, status));
	    
	    status = pjsua_set_null_snd_dev();
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Error opening NULL sound device",
			     status);
		return status;
	    }
	}
    }

    return PJ_SUCCESS;
}


/*
 * Destroy pjsua media subsystem.
 */
pj_status_t pjsua_media_subsys_destroy(void)
{
    unsigned i;

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
    for (i=0; i<(int)pjsua_var.ua_cfg.max_calls; ++i) {
	if (pjsua_var.calls[i].med_tp) {
	    (*pjsua_var.calls[i].med_tp->op->destroy)(pjsua_var.calls[i].med_tp);
	    pjsua_var.calls[i].med_tp = NULL;
	}
    }

    /* Destroy media endpoint. */
    if (pjsua_var.med_endpt) {

	/* Shutdown all codecs: */
#	if PJMEDIA_HAS_SPEEX_CODEC
	    pjmedia_codec_speex_deinit();
#	endif /* PJMEDIA_HAS_SPEEX_CODEC */

#	if PJMEDIA_HAS_GSM_CODEC
	    pjmedia_codec_gsm_deinit();
#	endif /* PJMEDIA_HAS_GSM_CODEC */

#	if PJMEDIA_HAS_G711_CODEC
	    pjmedia_codec_g711_deinit();
#	endif	/* PJMEDIA_HAS_G711_CODEC */

#	if PJMEDIA_HAS_L16_CODEC
	    pjmedia_codec_l16_deinit();
#	endif	/* PJMEDIA_HAS_L16_CODEC */


	pjmedia_endpt_destroy(pjsua_var.med_endpt);
	pjsua_var.med_endpt = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Create UDP media transports for all the calls. This function creates
 * one UDP media transport for each call.
 */
PJ_DEF(pj_status_t) 
pjsua_media_transports_create(const pjsua_transport_config *app_cfg)
{
    pjsua_transport_config cfg;
    unsigned i;
    pj_status_t status;


    /* Make sure pjsua_init() has been called */
    PJ_ASSERT_RETURN(pjsua_var.ua_cfg.max_calls>0, PJ_EINVALIDOP);

    PJSUA_LOCK();

    /* Delete existing media transports */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	if (pjsua_var.calls[i].med_tp != NULL) {
	    pjsua_var.calls[i].med_tp->op->destroy(pjsua_var.calls[i].med_tp);
	    pjsua_var.calls[i].med_tp = NULL;
	}
    }

    /* Copy config */
    pj_memcpy(&cfg, app_cfg, sizeof(*app_cfg));
    pjsua_normalize_stun_config(&cfg.stun_config);

    /* Create each media transport */
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {

	status = create_rtp_rtcp_sock(&cfg, &pjsua_var.calls[i].skinfo);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create RTP/RTCP socket",
		         status);
	    goto on_error;
	}
	status = pjmedia_transport_udp_attach(pjsua_var.med_endpt, NULL,
					      &pjsua_var.calls[i].skinfo, 0,
					      &pjsua_var.calls[i].med_tp);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create media transport",
		         status);
	    goto on_error;
	}
    }

    PJSUA_UNLOCK();

    return PJ_SUCCESS;

on_error:
    for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	if (pjsua_var.calls[i].med_tp != NULL) {
	    pjsua_var.calls[i].med_tp->op->destroy(pjsua_var.calls[i].med_tp);
	    pjsua_var.calls[i].med_tp = NULL;
	}
    }

    PJSUA_UNLOCK();

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
    unsigned ports[256];
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

    pj_memset(info, 0, sizeof(*info));
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
 * Establish unidirectional media flow from souce to sink. 
 */
PJ_DEF(pj_status_t) pjsua_conf_connect( pjsua_conf_port_id source,
					pjsua_conf_port_id sink)
{
    return pjmedia_conf_connect_port(pjsua_var.mconf, source, sink, 0);
}


/*
 * Disconnect media flow from the source to destination port.
 */
PJ_DEF(pj_status_t) pjsua_conf_disconnect( pjsua_conf_port_id source,
					   pjsua_conf_port_id sink)
{
    return pjmedia_conf_disconnect_port(pjsua_var.mconf, source, sink);
}


/*****************************************************************************
 * File player.
 */

/*
 * Create a file player, and automatically connect this player to
 * the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_player_create( const pj_str_t *filename,
					 unsigned options,
					 pjsua_player_id *p_id)
{
    unsigned slot, file_id;
    char path[128];
    pjmedia_port *port;
    pj_status_t status;

    if (pjsua_var.player_cnt >= PJ_ARRAY_SIZE(pjsua_var.player))
	return PJ_ETOOMANY;

    PJSUA_LOCK();

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.player); ++file_id) {
	if (pjsua_var.player[file_id].port == NULL)
	    break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.player)) {
	/* This is unexpected */
	PJSUA_UNLOCK();
	pj_assert(0);
	return PJ_EBUG;
    }

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';
    status = pjmedia_wav_player_port_create(pjsua_var.pool, path,
					    pjsua_var.mconf_cfg.samples_per_frame *
					      1000 / pjsua_var.media_cfg.clock_rate, 
					    0, 0, &port);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK();
	pjsua_perror(THIS_FILE, "Unable to open file for playback", status);
	return status;
    }

    status = pjmedia_conf_add_port(pjsua_var.mconf, pjsua_var.pool, 
				   port, filename, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	PJSUA_UNLOCK();
	return status;
    }

    pjsua_var.player[file_id].port = port;
    pjsua_var.player[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var.player_cnt;

    PJSUA_UNLOCK();
    return PJ_SUCCESS;
}


/*
 * Get conference port ID associated with player.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_player_get_conf_port(pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<PJ_ARRAY_SIZE(pjsua_var.player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);

    return pjsua_var.player[id].slot;
}


/*
 * Set playback position.
 */
PJ_DEF(pj_status_t) pjsua_player_set_pos( pjsua_player_id id,
					  pj_uint32_t samples)
{
    PJ_ASSERT_RETURN(id>=0 && id<PJ_ARRAY_SIZE(pjsua_var.player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);

    return pjmedia_wav_player_port_set_pos(pjsua_var.player[id].port, samples);
}


/*
 * Close the file, remove the player from the bridge, and free
 * resources associated with the file player.
 */
PJ_DEF(pj_status_t) pjsua_player_destroy(pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<PJ_ARRAY_SIZE(pjsua_var.player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.player[id].port != NULL, PJ_EINVAL);

    PJSUA_LOCK();

    if (pjsua_var.player[id].port) {
	pjmedia_conf_remove_port(pjsua_var.mconf, 
				 pjsua_var.player[id].slot);
	pjmedia_port_destroy(pjsua_var.player[id].port);
	pjsua_var.player[id].port = NULL;
	pjsua_var.player[id].slot = 0xFFFF;
	pjsua_var.player_cnt--;
    }

    PJSUA_UNLOCK();

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
					   unsigned file_format,
					   const pj_str_t *encoding,
					   pj_ssize_t max_size,
					   unsigned options,
					   pjsua_recorder_id *p_id)
{
    unsigned slot, file_id;
    char path[128];
    pjmedia_port *port;
    pj_status_t status;

    if (pjsua_var.rec_cnt >= PJ_ARRAY_SIZE(pjsua_var.recorder))
	return PJ_ETOOMANY;

    PJSUA_LOCK();

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var.recorder); ++file_id) {
	if (pjsua_var.recorder[file_id].port == NULL)
	    break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var.recorder)) {
	/* This is unexpected */
	PJSUA_UNLOCK();
	pj_assert(0);
	return PJ_EBUG;
    }

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';
    status = pjmedia_wav_writer_port_create(pjsua_var.pool, path, 
					    pjsua_var.media_cfg.clock_rate, 
					    pjsua_var.mconf_cfg.channel_count,
					    pjsua_var.mconf_cfg.samples_per_frame,
					    pjsua_var.mconf_cfg.bits_per_sample, 
					    0, 0, &port);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK();
	pjsua_perror(THIS_FILE, "Unable to open file for recording", status);
	return status;
    }

    status = pjmedia_conf_add_port(pjsua_var.mconf, pjsua_var.pool, 
				   port, filename, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	PJSUA_UNLOCK();
	return status;
    }

    pjsua_var.recorder[file_id].port = port;
    pjsua_var.recorder[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var.rec_cnt;

    PJSUA_UNLOCK();
    return PJ_SUCCESS;
}


/*
 * Get conference port associated with recorder.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_recorder_get_conf_port(pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<PJ_ARRAY_SIZE(pjsua_var.recorder), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.recorder[id].port != NULL, PJ_EINVAL);

    return pjsua_var.recorder[id].slot;
}


/*
 * Destroy recorder (this will complete recording).
 */
PJ_DEF(pj_status_t) pjsua_recorder_destroy(pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<PJ_ARRAY_SIZE(pjsua_var.recorder), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var.recorder[id].port != NULL, PJ_EINVAL);

    PJSUA_LOCK();

    if (pjsua_var.recorder[id].port) {
	pjmedia_conf_remove_port(pjsua_var.mconf, 
				 pjsua_var.recorder[id].slot);
	pjmedia_port_destroy(pjsua_var.recorder[id].port);
	pjsua_var.recorder[id].port = NULL;
	pjsua_var.recorder[id].slot = 0xFFFF;
	pjsua_var.rec_cnt--;
    }

    PJSUA_UNLOCK();

    return PJ_SUCCESS;
}


/*****************************************************************************
 * Sound devices.
 */

/*
 * Enum sound devices.
 */
PJ_DEF(pj_status_t) pjsua_enum_snd_devs( pjmedia_snd_dev_info info[],
					 unsigned *count)
{
    unsigned i, dev_count;

    dev_count = pjmedia_snd_get_dev_count();
    
    if (dev_count > *count) dev_count = *count;

    for (i=0; i<dev_count; ++i) {
	const pjmedia_snd_dev_info *ci;

	ci = pjmedia_snd_get_dev_info(i);
	pj_memcpy(&info[i], ci, sizeof(*ci));
    }

    *count = dev_count;

    return PJ_SUCCESS;
}


/* Close existing sound device */
static void close_snd_dev(void)
{
    /* Close sound device */
    if (pjsua_var.snd_port) {
	const pjmedia_snd_dev_info *cap_info, *play_info;

	cap_info = pjmedia_snd_get_dev_info(pjsua_var.cap_dev);
	play_info = pjmedia_snd_get_dev_info(pjsua_var.play_dev);

	PJ_LOG(4,(THIS_FILE, "Closing %s sound playback device and "
			     "%s sound capture device",
			     play_info->name, cap_info->name));

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
}

/*
 * Select or change sound device. Application may call this function at
 * any time to replace current sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_snd_dev( int capture_dev,
				       int playback_dev)
{
    pjmedia_port *conf_port;
    const pjmedia_snd_dev_info *cap_info, *play_info;
    pj_status_t status;

    /* Close existing sound port */
    close_snd_dev();


    cap_info = pjmedia_snd_get_dev_info(capture_dev);
    play_info = pjmedia_snd_get_dev_info(playback_dev);

    PJ_LOG(4,(THIS_FILE, "Opening %s sound playback device and "
			 "%s sound capture device..",
			 play_info->name, cap_info->name));

    /* Create the sound device. Sound port will start immediately. */
    status = pjmedia_snd_port_create(pjsua_var.pool, capture_dev,
				     playback_dev, 
				     pjsua_var.media_cfg.clock_rate, 1,
				     pjsua_var.media_cfg.clock_rate/FPS,
				     16, 0, &pjsua_var.snd_port);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to open sound device", status);
	return status;
    }

    /* Get the port0 of the conference bridge. */
    conf_port = pjmedia_conf_get_master_port(pjsua_var.mconf);
    pj_assert(conf_port != NULL);

    /* Connect to the conference port */
    status = pjmedia_snd_port_connect(pjsua_var.snd_port, conf_port);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to connect conference port to "
				"sound device", status);
	pjmedia_snd_port_destroy(pjsua_var.snd_port);
	pjsua_var.snd_port = NULL;
	return status;
    }

    /* Save the device IDs */
    pjsua_var.cap_dev = capture_dev;
    pjsua_var.play_dev = playback_dev;

    return PJ_SUCCESS;
}


/*
 * Use null sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_null_snd_dev(void)
{
    pjmedia_port *conf_port;
    pj_status_t status;

    /* Close existing sound device */
    close_snd_dev();

    PJ_LOG(4,(THIS_FILE, "Opening null sound device.."));

    /* Get the port0 of the conference bridge. */
    conf_port = pjmedia_conf_get_master_port(pjsua_var.mconf);
    pj_assert(conf_port != NULL);

    /* Create master port, connecting port0 of the conference bridge to
     * a null port.
     */
    status = pjmedia_master_port_create(pjsua_var.pool, pjsua_var.null_port,
					conf_port, 0, &pjsua_var.null_snd);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create null sound device",
		     status);
	return status;
    }

    /* Start the master port */
    status = pjmedia_master_port_start(pjsua_var.null_snd);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    return PJ_SUCCESS;
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
    pjmedia_codec_mgr *codec_mgr;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

    return pjmedia_codec_mgr_set_codec_priority(codec_mgr, codec_id, 
					        priority);
}


/*
 * Get codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_codec_get_param( const pj_str_t *codec_id,
					   pjmedia_codec_param *param )
{
    const pjmedia_codec_info *info;
    pjmedia_codec_mgr *codec_mgr;
    unsigned count = 1;
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, codec_id,
						 &count, &info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    if (count != 1)
	return PJ_ENOTFOUND;

    status = pjmedia_codec_mgr_get_default_param( codec_mgr, info, param);
    return status;
}


/*
 * Set codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_codec_set_param( const pj_str_t *id,
					   const pjmedia_codec_param *param)
{
    PJ_TODO(set_codec_param);
    return PJ_SUCCESS;
}



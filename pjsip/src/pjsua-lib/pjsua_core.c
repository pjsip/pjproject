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
 * pjsua_core.c
 *
 * Core application functionalities.
 */

#define THIS_FILE   "pjsua_core.c"


/* 
 * Global variable.
 */
struct pjsua pjsua;


/* 
 * Default local URI, if none is specified in cmd-line 
 */
#define PJSUA_LOCAL_URI	    "<sip:user@127.0.0.1>"



/*
 * Init default application parameters.
 */
PJ_DEF(void) pjsua_default_config(pjsua_config *cfg)
{
    unsigned i;

    pj_memset(cfg, 0, sizeof(pjsua_config));

    cfg->thread_cnt = 1;
    cfg->media_has_ioqueue = 1;
    cfg->media_thread_cnt = 1;
    cfg->udp_port = 5060;
    cfg->start_rtp_port = 4000;
    cfg->max_calls = 4;
    cfg->conf_ports = 0;

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
    pjsua.clock_rate = 44100;
#endif

    cfg->complexity = 10;
    cfg->quality = 10;
    
    cfg->auto_answer = 100;
    cfg->uas_duration = 3600;

    /* Default logging settings: */
    cfg->log_level = 5;
    cfg->app_log_level = 4;
    cfg->log_decor =  PJ_LOG_HAS_SENDER | PJ_LOG_HAS_TIME | 
		      PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_NEWLINE;


    /* Also init logging settings in pjsua.config, because log
     * may be written before pjsua_init() is called.
     */
    pjsua.config.log_level = 5;
    pjsua.config.app_log_level = 4;


    /* Init accounts: */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.acc); ++i) {
	cfg->acc_config[i].reg_timeout = 55;
    }

}


#define strncpy_with_null(dst,src,len)	\
do { \
    strncpy(dst, src, len); \
    dst[len-1] = '\0'; \
} while (0)



PJ_DEF(pj_status_t) pjsua_test_config( const pjsua_config *cfg,
				       char *errmsg,
				       int len)
{
    unsigned i;

    /* If UDP port is zero, then sip_host and sip_port must be specified */
    if (cfg->udp_port == 0) {
	if (cfg->sip_host.slen==0 || cfg->sip_port==0) {
	    strncpy_with_null(errmsg, 
			      "sip_host and sip_port must be specified",
			      len);
	    return -1;
	}
    }

    if (cfg->max_calls < 1) {
	strncpy_with_null(errmsg, 
			  "max_calls needs to be at least 1",
			  len);
	return -1;
    }

    /* STUN */
    if (cfg->stun_srv1.slen || cfg->stun_port1 || cfg->stun_port2 || 
	cfg->stun_srv2.slen) 
    {
	if (cfg->stun_port1 == 0) {
	    strncpy_with_null(errmsg, "stun_port1 required", len);
	    return -1;
	}
	if (cfg->stun_srv1.slen == 0) {
	    strncpy_with_null(errmsg, "stun_srv1 required", len);
	    return -1;
	}
	if (cfg->stun_port2 == 0) {
	    strncpy_with_null(errmsg, "stun_port2 required", len);
	    return -1;
	}
	if (cfg->stun_srv2.slen == 0) {
	    strncpy_with_null(errmsg, "stun_srv2 required", len);
	    return -1;
	}
    }

    /* Verify accounts */
    for (i=0; i<cfg->acc_cnt; ++i) {
	const pjsua_acc_config *acc_cfg = &cfg->acc_config[i];
	unsigned j;

	if (acc_cfg->id.slen == 0) {
	    strncpy_with_null(errmsg, "missing account ID", len);
	    return -1;
	}

	if (acc_cfg->id.slen == 0) {
	    strncpy_with_null(errmsg, "missing registrar URI", len);
	    return -1;
	}

	if (acc_cfg->reg_timeout == 0) {
	    strncpy_with_null(errmsg, "missing registration timeout", len);
	    return -1;
	}


	for (j=0; j<acc_cfg->cred_count; ++j) {

	    if (acc_cfg->cred_info[j].scheme.slen == 0) {
		strncpy_with_null(errmsg, "missing auth scheme in account", 
				  len);
		return -1;
	    }

	    if (acc_cfg->cred_info[j].realm.slen == 0) {
		strncpy_with_null(errmsg, "missing realm in account", len);
		return -1;
	    }

	    if (acc_cfg->cred_info[j].username.slen == 0) {
		strncpy_with_null(errmsg, "missing username in account", len);
		return -1;
	    }

	}
    }

    return PJ_SUCCESS;
}


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

    if (rdata->msg_info.msg->line.req.method.id == PJSIP_INVITE_METHOD) {

	return pjsua_call_on_incoming(rdata);
    }

    return PJ_FALSE;
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


static int PJ_THREAD_FUNC pjsua_poll(void *arg)
{
    pj_status_t last_err = 0;

    PJ_UNUSED_ARG(arg);

    do {
	pj_time_val timeout = { 0, 10 };
	pj_status_t status;
	
	status = pjsip_endpt_handle_events (pjsua.endpt, &timeout);
	if (status != PJ_SUCCESS && status != last_err) {
	    last_err = status;
	    pjsua_perror(THIS_FILE, "handle_events() returned error", status);
	}
    } while (!pjsua.quit_flag);

    return 0;
}

/**
 * Poll pjsua.
 */
PJ_DECL(int) pjsua_handle_events(unsigned msec_timeout)
{
    unsigned count = 0;
    pj_time_val tv;
    pj_status_t status;

    tv.sec = 0;
    tv.msec = msec_timeout;
    pj_time_val_normalize(&tv);

    status = pjsip_endpt_handle_events2(pjsua.endpt, &tv, &count);
    if (status != PJ_SUCCESS)
	return -status;

    return count;
}


#define pjsua_has_stun()    (pjsua.config.stun_port1 && \
			     pjsua.config.stun_port2)


/*
 * Create and initialize SIP socket (and possibly resolve public
 * address via STUN, depending on config).
 */
static pj_status_t create_sip_udp_sock(int port,
				       pj_sock_t *p_sock,
				       pj_sockaddr_in *p_pub_addr)
{
    pj_sock_t sock;
    pj_status_t status;

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "socket() error", status);
	return status;
    }

    status = pj_sock_bind_in(sock, 0, (pj_uint16_t)port);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "bind() error", status);
	pj_sock_close(sock);
	return status;
    }

    if (pjsua_has_stun()) {
	status = pj_stun_get_mapped_addr(&pjsua.cp.factory, 1, &sock,
				         &pjsua.config.stun_srv1, 
					 pjsua.config.stun_port1,
					 &pjsua.config.stun_srv2, 
					 pjsua.config.stun_port2,
				         p_pub_addr);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "STUN resolve error", status);
	    pj_sock_close(sock);
	    return status;
	}

    } else {

	const pj_str_t *hostname = pj_gethostname();
	struct pj_hostent he;

	status = pj_gethostbyname(hostname, &he);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to resolve local host", status);
	    pj_sock_close(sock);
	    return status;
	}

	pj_memset(p_pub_addr, 0, sizeof(pj_sockaddr_in));
	p_pub_addr->sin_family = PJ_AF_INET;
	p_pub_addr->sin_port = pj_htons((pj_uint16_t)port);
	p_pub_addr->sin_addr = *(pj_in_addr*)he.h_addr;
    }

    *p_sock = sock;
    return PJ_SUCCESS;
}


/* 
 * Create RTP and RTCP socket pair, and possibly resolve their public
 * address via STUN.
 */
static pj_status_t create_rtp_rtcp_sock(pjmedia_sock_info *skinfo)
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
	rtp_port = (pj_uint16_t)pjsua.config.start_rtp_port;

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

	status = pj_sock_bind_in(sock[0], 0, rtp_port);
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

	status = pj_sock_bind_in(sock[1], 0, (pj_uint16_t)(rtp_port+1));
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
	if (pjsua_has_stun()) {
	    status=pj_stun_get_mapped_addr(&pjsua.cp.factory, 2, sock,
					   &pjsua.config.stun_srv1, 
					   pjsua.config.stun_port1,
					   &pjsua.config.stun_srv2, 
					   pjsua.config.stun_port2,
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



/**
 * Create pjsua application.
 * This initializes pjlib/pjlib-util, and creates memory pool factory to
 * be used by application.
 */
PJ_DEF(pj_status_t) pjsua_create(void)
{
    pj_status_t status;

    /* Init PJLIB: */

    status = pj_init();
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "pj_init() error", status);
	return status;
    }

    /* Init PJLIB-UTIL: */

    status = pjlib_util_init();
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "pjlib_util_init() error", status);
	return status;
    }

    /* Init memory pool: */

    /* Init caching pool. */
    pj_caching_pool_init(&pjsua.cp, &pj_pool_factory_default_policy, 0);

    /* Create memory pool for application. */
    pjsua.pool = pj_pool_create(&pjsua.cp.factory, "pjsua", 4000, 4000, NULL);

    /* Must create endpoint to initialize SIP parser. */
    /* Create global endpoint: */

    status = pjsip_endpt_create(&pjsua.cp.factory, 
				pj_gethostname()->ptr, 
				&pjsua.endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create SIP endpoint", status);
	return status;
    }

    /* Must create media endpoint too */
    status = pjmedia_endpt_create(&pjsua.cp.factory, 
				  pjsua.config.media_has_ioqueue? NULL :
				       pjsip_endpt_get_ioqueue(pjsua.endpt), 
				  pjsua.config.media_thread_cnt,
				  &pjsua.med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Media stack initialization has returned error", 
		     status);
	return status;
    }


    return PJ_SUCCESS;
}



/*
 * Init media.
 */
static pj_status_t init_media(void)
{
    int i;
    unsigned options;
    pj_str_t codec_id;
    pj_status_t status;

    /* Register all codecs */
#if PJMEDIA_HAS_SPEEX_CODEC
    /* Register speex. */
    status = pjmedia_codec_speex_init(pjsua.med_endpt, 
				      PJMEDIA_SPEEX_NO_UWB,
				      pjsua.config.quality, 
				      pjsua.config.complexity );
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing Speex codec",
		     status);
	return status;
    }

    /* Set "speex/16000/1" to have highest priority */
    codec_id = pj_str("speex/16000/1");
    pjmedia_codec_mgr_set_codec_priority( 
	pjmedia_endpt_get_codec_mgr(pjsua.med_endpt),
	&codec_id, 
	PJMEDIA_CODEC_PRIO_HIGHEST);

#endif /* PJMEDIA_HAS_SPEEX_CODEC */

#if PJMEDIA_HAS_GSM_CODEC
    /* Register GSM */
    status = pjmedia_codec_gsm_init(pjsua.med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing GSM codec",
		     status);
	return status;
    }
#endif /* PJMEDIA_HAS_GSM_CODEC */

#if PJMEDIA_HAS_G711_CODEC
    /* Register PCMA and PCMU */
    status = pjmedia_codec_g711_init(pjsua.med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing G711 codec",
		     status);
	return status;
    }
#endif	/* PJMEDIA_HAS_G711_CODEC */

#if PJMEDIA_HAS_L16_CODEC
    /* Register L16 family codecs, but disable all */
    status = pjmedia_codec_l16_init(pjsua.med_endpt, 0);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing L16 codecs",
		     status);
	return status;
    }

    /* Disable ALL L16 codecs */
    codec_id = pj_str("L16");
    pjmedia_codec_mgr_set_codec_priority( 
	pjmedia_endpt_get_codec_mgr(pjsua.med_endpt),
	&codec_id, 
	PJMEDIA_CODEC_PRIO_DISABLED);

#endif	/* PJMEDIA_HAS_L16_CODEC */


    /* Enable those codecs that user put with "--add-codec", and move
     * the priority to top
     */
    for (i=0; i<(int)pjsua.config.codec_cnt; ++i) {
	pjmedia_codec_mgr_set_codec_priority( 
	    pjmedia_endpt_get_codec_mgr(pjsua.med_endpt),
	    &pjsua.config.codec_arg[i], 
	    PJMEDIA_CODEC_PRIO_HIGHEST);
    }


    /* Init options for conference bridge. */
    options = PJMEDIA_CONF_NO_DEVICE;

    /* Calculate maximum number of ports, if it's not specified */
    if (pjsua.config.conf_ports == 0) {
	pjsua.config.conf_ports = 3 * pjsua.config.max_calls;
    }

    /* Init conference bridge. */
    pjsua.clock_rate = pjsua.config.clock_rate ? pjsua.config.clock_rate : 16000;
    pjsua.samples_per_frame = pjsua.clock_rate * 10 / 1000;
    status = pjmedia_conf_create(pjsua.pool, 
				 pjsua.config.conf_ports, 
				 pjsua.clock_rate, 
				 1, /* mono */
				 pjsua.samples_per_frame, 
				 16, 
				 options,
				 &pjsua.mconf);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Media stack initialization has returned error", 
		     status);
	return status;
    }

    if (pjsua.config.null_audio == PJ_FALSE) {
	pjmedia_port *conf_port;

	/* Create sound device port */
	status = pjmedia_snd_port_create(pjsua.pool, 
					 pjsua.config.snd_capture_id,
					 pjsua.config.snd_player_id, 
					 pjsua.clock_rate, 1 /* mono */,
					 pjsua.samples_per_frame, 16,
					 0, &pjsua.snd_port);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create sound device", status);
	    return status;
	}

	/* Get the port interface of the conference bridge */
	conf_port = pjmedia_conf_get_master_port(pjsua.mconf);

	/* Connect conference port interface to sound port */
	pjmedia_snd_port_connect( pjsua.snd_port, conf_port);

    } else {
	pjmedia_port *null_port, *conf_port;

	/* Create NULL port */
	status = pjmedia_null_port_create(pjsua.pool, pjsua.clock_rate,
					  1, pjsua.samples_per_frame, 16,
					  &null_port);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create NULL port", status);
	    return status;
	}

	/* Get the port interface of the conference bridge */
	conf_port = pjmedia_conf_get_master_port(pjsua.mconf);

	/* Create master port to control conference bridge's clock */
	status = pjmedia_master_port_create(pjsua.pool, null_port, conf_port,
					    0, &pjsua.master_port);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create master port", status);
	    return status;
	}
    }

    /* Create WAV file player if required: */

    if (pjsua.config.wav_file.slen) {

	status = pjsua_player_create(&pjsua.config.wav_file, NULL);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create file player", 
			 status);
	    return status;
	}
    }


    return PJ_SUCCESS;
}


/*
 * Copy account configuration.
 */
static void copy_acc_config(pj_pool_t *pool,
			    pjsua_acc_config *dst_acc,
			    const pjsua_acc_config *src_acc)
{
    unsigned j;

    pj_strdup_with_null(pool, &dst_acc->id, &src_acc->id);
    pj_strdup_with_null(pool, &dst_acc->reg_uri, &src_acc->reg_uri);
    pj_strdup_with_null(pool, &dst_acc->contact, &src_acc->contact);
    pj_strdup_with_null(pool, &dst_acc->proxy, &src_acc->proxy);

    for (j=0; j<src_acc->cred_count; ++j) {
	pj_strdup_with_null(pool, &dst_acc->cred_info[j].realm, 
			    &src_acc->cred_info[j].realm);
	pj_strdup_with_null(pool, &dst_acc->cred_info[j].scheme, 
			    &src_acc->cred_info[j].scheme);
	pj_strdup_with_null(pool, &dst_acc->cred_info[j].username, 
			    &src_acc->cred_info[j].username);
	pj_strdup_with_null(pool, &dst_acc->cred_info[j].data, 
			    &src_acc->cred_info[j].data);
    }
}


/*
 * Copy configuration.
 */
static void copy_config(pj_pool_t *pool, pjsua_config *dst, 
			const pjsua_config *src)
{
    unsigned i;

    /* Plain memcpy */
    pj_memcpy(dst, src, sizeof(pjsua_config));

    /* Duplicate strings */
    pj_strdup_with_null(pool, &dst->sip_host, &src->sip_host);
    pj_strdup_with_null(pool, &dst->stun_srv1, &src->stun_srv1);
    pj_strdup_with_null(pool, &dst->stun_srv2, &src->stun_srv2);
    pj_strdup_with_null(pool, &dst->wav_file, &src->wav_file);
    
    for (i=0; i<src->codec_cnt; ++i) {
	pj_strdup_with_null(pool, &dst->codec_arg[i], &src->codec_arg[i]);
    }

    pj_strdup_with_null(pool, &dst->outbound_proxy, &src->outbound_proxy);
    pj_strdup_with_null(pool, &dst->uri_to_call, &src->uri_to_call);

    for (i=0; i<src->acc_cnt; ++i) {
	pjsua_acc_config *dst_acc = &dst->acc_config[i];
	const pjsua_acc_config *src_acc = &src->acc_config[i];
	copy_acc_config(pool, dst_acc, src_acc);
    }

    pj_strdup_with_null(pool, &dst->log_filename, &src->log_filename);

    for (i=0; i<src->buddy_cnt; ++i) {
	pj_strdup_with_null(pool, &dst->buddy_uri[i], &src->buddy_uri[i]);
    }
}


/*****************************************************************************
 * Console application custom logging:
 */


static void log_writer(int level, const char *buffer, int len)
{
    /* Write to both stdout and file. */

    if (level <= (int)pjsua.config.app_log_level)
	pj_log_write(level, buffer, len);

    if (pjsua.log_file) {
	fwrite(buffer, len, 1, pjsua.log_file);
	fflush(pjsua.log_file);
    }
}


static pj_status_t logging_init()
{
    /* Redirect log function to ours */

    pj_log_set_log_func( &log_writer );

    /* If output log file is desired, create the file: */

    if (pjsua.config.log_filename.slen) {
	pjsua.log_file = fopen(pjsua.config.log_filename.ptr, "wt");
	if (pjsua.log_file == NULL) {
	    PJ_LOG(1,(THIS_FILE, "Unable to open log file %s", 
		      pjsua.config.log_filename.ptr));   
	    return -1;
	}
    }

    return PJ_SUCCESS;
}


static void logging_shutdown(void)
{
    /* Close logging file, if any: */

    if (pjsua.log_file) {
	fclose(pjsua.log_file);
	pjsua.log_file = NULL;
    }
}


/*
 * Initialize pjsua application.
 * This will initialize all libraries, create endpoint instance, and register
 * pjsip modules.
 */
PJ_DECL(pj_status_t) pjsua_init(const pjsua_config *cfg,
				const pjsua_callback *cb)
{
    char errmsg[80];
    unsigned i;
    pj_status_t status;


    /* Init accounts: */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.acc); ++i) {
	pjsua.acc[i].index = i;
	pjsua.acc[i].online_status = PJ_TRUE;
	pj_list_init(&pjsua.acc[i].route_set);
	pj_list_init(&pjsua.acc[i].pres_srv_list);
    }

    /* Init call array: */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.calls); ++i) {
	pjsua.calls[i].index = i;
	pjsua.calls[i].refresh_tm._timer_id = -1;
	pjsua.calls[i].hangup_tm._timer_id = -1;
	pjsua.calls[i].conf_slot = 0;
    }

    /* Init buddies array */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.buddies); ++i) {
	pjsua.buddies[i].index = i;
    }

    /* Copy configuration */
    copy_config(pjsua.pool, &pjsua.config, cfg);

    /* Copy callback */
    pj_memcpy(&pjsua.cb, cb, sizeof(pjsua_callback));

    /* Test configuration */
    if (pjsua_test_config(&pjsua.config, errmsg, sizeof(errmsg))) {
	PJ_LOG(1,(THIS_FILE, "Error in configuration: %s", errmsg));
	status = -1;
	goto on_error;
    }


    /* Init PJLIB logging: */

    pj_log_set_level(pjsua.config.log_level);
    pj_log_set_decor(pjsua.config.log_decor);

    status = logging_init();
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create SIP UDP socket */
    if (pjsua.config.udp_port) {

	status = create_sip_udp_sock( pjsua.config.udp_port,
				      &pjsua.sip_sock,
				      &pjsua.sip_sock_name);
	if (status != PJ_SUCCESS)
	    goto on_error;
    
	pj_strdup2_with_null(pjsua.pool, &pjsua.config.sip_host,
			     pj_inet_ntoa(pjsua.sip_sock_name.sin_addr));
	pjsua.config.sip_port = pj_ntohs(pjsua.sip_sock_name.sin_port);

    } else {

	/* Check that SIP host and port is configured */
	if (cfg->sip_host.slen == 0 || cfg->sip_port == 0) {
	    PJ_LOG(1,(THIS_FILE, 
		      "Error: sip_host and sip_port must be specified"));
	    status = PJ_EINVAL;
	    goto on_error;
	}

	pjsua.sip_sock = PJ_INVALID_SOCKET;
    }


    /* Init media endpoint */
    status = init_media();
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Init RTP sockets, only when UDP transport is enabled */
    for (i=0; pjsua.config.start_rtp_port && i<pjsua.config.max_calls; ++i) {
	status = create_rtp_rtcp_sock(&pjsua.calls[i].skinfo);
	if (status != PJ_SUCCESS) {
	    unsigned j;
	    for (j=0; j<i; ++j) {
		if (pjsua.calls[i].med_tp)
		    pjsua.calls[i].med_tp->op->destroy(pjsua.calls[i].med_tp);
	    }
	    goto on_error;
	}
	status = pjmedia_transport_udp_attach(pjsua.med_endpt, NULL,
					      &pjsua.calls[i].skinfo, 0,
					      &pjsua.calls[i].med_tp);
    }

    /* Init PJSIP : */

    /* Initialize transaction layer: */

    status = pjsip_tsx_layer_init_module(pjsua.endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Transaction layer initialization error", 
		     status);
	goto on_error;
    }

    /* Initialize UA layer module: */

    status = pjsip_ua_init_module( pjsua.endpt, NULL );
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "UA layer initialization error", status);
	goto on_error;
    }

    /* Initialize and register pjsua's application module: */

    {
	pjsip_module my_mod = 
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

	pjsua.mod = my_mod;

	status = pjsip_endpt_register_module(pjsua.endpt, &pjsua.mod);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to register pjsua module", 
			 status);
	    goto on_error;
	}
    }

    /* Initialize invite session module: */

    status = pjsua_call_init();
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Invite usage initialization error", 
		     status);
	goto on_error;
    }

    /* Init core SIMPLE module : */

    pjsip_evsub_init_module(pjsua.endpt);

    /* Init presence module: */

    pjsip_pres_init_module( pjsua.endpt, pjsip_evsub_instance());

    /* Init xfer/REFER module */

    pjsip_xfer_init_module( pjsua.endpt );

    /* Init pjsua presence handler: */

    pjsua_pres_init();

    /* Init out-of-dialog MESSAGE request handler. */

    pjsua_im_init();


    /* Done. */
    return PJ_SUCCESS;

on_error:
    pjsua_destroy();
    return status;
}


/*
 * Find account for incoming request.
 */
int pjsua_find_account_for_incoming(pjsip_rx_data *rdata)
{
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;
    unsigned acc_index;

    uri = rdata->msg_info.to->uri;

    /* Just return last account if To URI is not SIP: */
    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri)) 
    {
	return pjsua.config.acc_cnt;
    }


    sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);

    /* Find account which has matching username and domain. */
    for (acc_index=0; acc_index < pjsua.config.acc_cnt; ++acc_index) {

	pjsua_acc *acc = &pjsua.acc[acc_index];

	if (pj_stricmp(&acc->user_part, &sip_uri->user)==0 &&
	    pj_stricmp(&acc->host_part, &sip_uri->host)==0) 
	{
	    /* Match ! */
	    return acc_index;
	}
    }

    /* No matching, try match domain part only. */
    for (acc_index=0; acc_index < pjsua.config.acc_cnt; ++acc_index) {

	pjsua_acc *acc = &pjsua.acc[acc_index];

	if (pj_stricmp(&acc->host_part, &sip_uri->host)==0) {
	    /* Match ! */
	    return acc_index;
	}
    }

    /* Still no match, just return last account */
    return pjsua.config.acc_cnt;
}


/*
 * Find account for outgoing request.
 */
int pjsua_find_account_for_outgoing(const pj_str_t *url)
{
    PJ_UNUSED_ARG(url);

    /* Just use account #0 */
    return 0;
}


/*
 * Init account
 */
static pj_status_t init_acc(unsigned acc_index)
{
    pjsua_acc_config *acc_cfg = &pjsua.config.acc_config[acc_index];
    pjsua_acc *acc = &pjsua.acc[acc_index];
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;

    /* Need to parse local_uri to get the elements: */

    uri = pjsip_parse_uri(pjsua.pool, acc_cfg->id.ptr,
			  acc_cfg->id.slen, 0);
    if (uri == NULL) {
	pjsua_perror(THIS_FILE, "Invalid local URI", 
		     PJSIP_EINVALIDURI);
	return PJSIP_EINVALIDURI;
    }

    /* Local URI MUST be a SIP or SIPS: */

    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri)) 
    {
	pjsua_perror(THIS_FILE, "Invalid local URI", 
		     PJSIP_EINVALIDSCHEME);
	return PJSIP_EINVALIDSCHEME;
    }


    /* Get the SIP URI object: */

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri);

    acc->user_part = sip_uri->user;
    acc->host_part = sip_uri->host;

    /* Build Contact header */

    if (acc_cfg->contact.slen == 0)  {
	char contact[128];
	const char *addr;
	int port;
	int len;

	addr = pjsua.config.sip_host.ptr;
	port = pjsua.config.sip_port;

	/* The local Contact is the username@ip-addr, where
	 *  - username is taken from the local URI,
	 *  - ip-addr in UDP transport's address name (which may have been
	 *    resolved from STUN.
	 */
	
	/* Build temporary contact string. */

	if (sip_uri->user.slen) {

	    /* With the user part. */
	    len = pj_ansi_snprintf(contact, sizeof(contact),
			      "<sip:%.*s@%s:%d>",
			      (int)sip_uri->user.slen,
			      sip_uri->user.ptr,
			      addr, port);
	} else {

	    /* Without user part */

	    len = pj_ansi_snprintf(contact, sizeof(contact),
			      "<sip:%s:%d>",
			      addr, port);
	}

	if (len < 1 || len >= sizeof(contact)) {
	    pjsua_perror(THIS_FILE, "Invalid Contact", PJSIP_EURITOOLONG);
	    return PJSIP_EURITOOLONG;
	}

	/* Duplicate Contact uri. */

	pj_strdup2(pjsua.pool, &acc_cfg->contact, contact);

    }


    /* Build route-set for this account */
    if (pjsua.config.outbound_proxy.slen) {
	pj_str_t hname = { "Route", 5};
	pjsip_route_hdr *r;
	pj_str_t tmp;

	pj_strdup_with_null(pjsua.pool, &tmp, &pjsua.config.outbound_proxy);
	r = pjsip_parse_hdr(pjsua.pool, &hname, tmp.ptr, tmp.slen, NULL);
	pj_list_push_back(&acc->route_set, r);
    }

    if (acc_cfg->proxy.slen) {
	pj_str_t hname = { "Route", 5};
	pjsip_route_hdr *r;
	pj_str_t tmp;

	pj_strdup_with_null(pjsua.pool, &tmp, &acc_cfg->proxy);
	r = pjsip_parse_hdr(pjsua.pool, &hname, tmp.ptr, tmp.slen, NULL);
	pj_list_push_back(&acc->route_set, r);
    }

    return PJ_SUCCESS;
}

/*
 * Add a new account.
 */
PJ_DEF(pj_status_t) pjsua_acc_add( const pjsua_acc_config *cfg,
				   int *acc_index)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua.config.acc_cnt<PJ_ARRAY_SIZE(pjsua.config.acc_config),
		     PJ_ETOOMANY);

    copy_acc_config(pjsua.pool, &pjsua.config.acc_config[pjsua.config.acc_cnt], cfg);
    
    status = init_acc(pjsua.config.acc_cnt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error adding account", status);
	return status;
    }

    if (acc_index)
	*acc_index = pjsua.config.acc_cnt;

    pjsua.config.acc_cnt++;

    return PJ_SUCCESS;
}



/*
 * Start pjsua stack.
 * This will start the registration process, if registration is configured.
 */
PJ_DEF(pj_status_t) pjsua_start(void)
{
    int i;  /* Must be signed */
    unsigned count;
    pj_status_t status = PJ_SUCCESS;


    /* Add UDP transport: */
    if (pjsua.sip_sock > 0) {

	/* Init the published name for the transport.
         * Depending whether STUN is used, this may be the STUN mapped
	 * address, or socket's bound address.
	 */
	pjsip_host_port addr_name;

	addr_name.host = pjsua.config.sip_host;
	addr_name.port = pjsua.config.sip_port;

	/* Create UDP transport from previously created UDP socket: */

	status = pjsip_udp_transport_attach( pjsua.endpt, pjsua.sip_sock,
					     &addr_name, 1, 
					     NULL);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to start UDP transport", 
			 status);
	    goto on_error;
	}
    }

    /* Initialize all unused accounts with default id and contact.
     */
    {
	char buf[80];
	pj_str_t tmp, id;

	tmp.ptr = buf;
	tmp.slen = pj_ansi_sprintf(tmp.ptr, "<sip:%s:%d>", 
				   pjsua.config.sip_host.ptr,
				   pjsua.config.sip_port);
	pj_strdup_with_null( pjsua.pool, &id, &tmp);

	for (i=pjsua.config.acc_cnt; i<PJ_ARRAY_SIZE(pjsua.config.acc_config);
	     ++i)
	{
	    pjsua_acc_config *acc_cfg = 
		&pjsua.config.acc_config[pjsua.config.acc_cnt];

	    acc_cfg->id = id;
	    acc_cfg->contact = id;
	}
    }
    

    /* Initialize accounts: */
    for (i=0; i<(int)pjsua.config.acc_cnt; ++i) {
	status = init_acc(i);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error initializing account", status);
	    goto on_error;
	}
    }



    /* Create worker thread(s), if required: */

    for (i=0; i<(int)pjsua.config.thread_cnt; ++i) {
	status = pj_thread_create( pjsua.pool, "pjsua", &pjsua_poll,
				   NULL, 0, 0, &pjsua.threads[i]);
	if (status != PJ_SUCCESS) {
	    pjsua.quit_flag = 1;
	    for (--i; i>=0; --i) {
		pj_thread_join(pjsua.threads[i]);
		pj_thread_destroy(pjsua.threads[i]);
	    }
	    goto on_error;
	}
    }

    /* Start registration: */

    /* Create client registration session: */
    for (i=0; i<(int)pjsua.config.acc_cnt; ++i) {
	status = pjsua_regc_init(i);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Perform registration, if required. */
	if (pjsua.acc[i].regc) {
	    pjsua_acc_set_registration(i, PJ_TRUE);
	}
    }


    /* Re-init buddies */
    count = pjsua.config.buddy_cnt;
    pjsua.config.buddy_cnt = 0;
    for (i=0; i<(int)count; ++i) {
	pj_str_t uri = pjsua.config.buddy_uri[i];
	pjsua_buddy_add(&uri, NULL);
    }



    PJ_LOG(3,(THIS_FILE, "PJSUA version %s started", PJ_VERSION));
    return PJ_SUCCESS;

on_error:
    pjsua_destroy();
    return status;
}


/* Sleep with polling */
static void busy_sleep(unsigned msec)
{
    pj_time_val timeout, now;

    pj_gettimeofday(&timeout);
    timeout.msec += msec;
    pj_time_val_normalize(&timeout);

    do {
	pjsua_poll(NULL);
	pj_gettimeofday(&now);
    } while (PJ_TIME_VAL_LT(now, timeout));
}

/**
 * Get maxinum number of conference ports.
 */
PJ_DEF(unsigned) pjsua_conf_max_ports(void)
{
    return pjsua.config.conf_ports;
}


/**
 * Enum all conference ports.
 */
PJ_DEF(pj_status_t) pjsua_conf_enum_ports( unsigned *count,
					   pjmedia_conf_port_info info[])
{
    return pjmedia_conf_get_ports_info(pjsua.mconf, count, info);
}




/**
 * Connect conference port.
 */
PJ_DEF(pj_status_t) pjsua_conf_connect( unsigned src_port,
					unsigned dst_port)
{
    return pjmedia_conf_connect_port(pjsua.mconf, src_port, dst_port, 0);
}


/**
 * Connect conference port connection.
 */
PJ_DEF(pj_status_t) pjsua_conf_disconnect( unsigned src_port,
					   unsigned dst_port)
{
    return pjmedia_conf_disconnect_port(pjsua.mconf, src_port, dst_port);
}



/**
 * Create a file player.
 */
PJ_DEF(pj_status_t) pjsua_player_create( const pj_str_t *filename,
					 pjsua_player_id *id)
{
    unsigned slot;
    char path[128];
    pjmedia_port *port;
    pj_status_t status;

    if (pjsua.player_cnt >= PJ_ARRAY_SIZE(pjsua.player))
	return PJ_ETOOMANY;

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';
    status = pjmedia_wav_player_port_create(pjsua.pool, path,
					    pjsua.samples_per_frame *
					       1000 / pjsua.clock_rate,
					    0, 0, NULL,
					    &port);
    if (status != PJ_SUCCESS)
	return status;

    status = pjmedia_conf_add_port(pjsua.mconf, pjsua.pool, 
				   port, filename, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	return status;
    }

    pjsua.player[pjsua.player_cnt].port = port;
    pjsua.player[pjsua.player_cnt].slot = slot;

    if (*id)
	*id = pjsua.player_cnt;

    ++pjsua.player_cnt;

    return PJ_SUCCESS;
}


/**
 * Get conference port associated with player.
 */
PJ_DEF(int) pjsua_player_get_conf_port(pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id < PJ_ARRAY_SIZE(pjsua.player), PJ_EINVAL);
    return pjsua.player[id].slot;
}


/**
 * Re-wind playback.
 */
PJ_DEF(pj_status_t) pjsua_player_set_pos(pjsua_player_id id,
					 pj_uint32_t samples)
{
    PJ_ASSERT_RETURN(id>=0 && id < PJ_ARRAY_SIZE(pjsua.player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua.player[id].port != NULL, PJ_EINVALIDOP);

    return pjmedia_wav_player_port_set_pos(pjsua.player[id].port, samples);
}


/**
 * Get conference port associated with player.
 */
PJ_DEF(pj_status_t) pjsua_player_destroy(pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id < PJ_ARRAY_SIZE(pjsua.player), PJ_EINVAL);

    if (pjsua.player[id].port) {
	pjmedia_port_destroy(pjsua.player[id].port);
	pjsua.player[id].port = NULL;
	pjsua.player[id].slot = 0xFFFF;
	pjsua.player_cnt--;
    }

    return PJ_SUCCESS;
}


/**
 * Create a file recorder.
 */
PJ_DEF(pj_status_t) pjsua_recorder_create( const pj_str_t *filename,
					   pjsua_recorder_id *id)
{
    unsigned slot;
    char path[128];
    pjmedia_port *port;
    pj_status_t status;

    if (pjsua.recorder_cnt >= PJ_ARRAY_SIZE(pjsua.recorder))
	return PJ_ETOOMANY;

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';
    status = pjmedia_wav_writer_port_create(pjsua.pool, path,
					    pjsua.clock_rate, 1,
					    pjsua.samples_per_frame,
					    16, 0, 0, NULL,
					    &port);
    if (status != PJ_SUCCESS)
	return status;

    status = pjmedia_conf_add_port(pjsua.mconf, pjsua.pool, 
				   port, filename, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	return status;
    }

    pjsua.recorder[pjsua.recorder_cnt].port = port;
    pjsua.recorder[pjsua.recorder_cnt].slot = slot;

    if (*id)
	*id = pjsua.recorder_cnt;

    ++pjsua.recorder_cnt;

    return PJ_SUCCESS;
}


/**
 * Get conference port associated with recorder.
 */
PJ_DEF(int) pjsua_recorder_get_conf_port(pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id < PJ_ARRAY_SIZE(pjsua.recorder), PJ_EINVAL);
    return pjsua.recorder[id].slot;
}


/**
 * Destroy recorder (will complete recording).
 */
PJ_DEF(pj_status_t) pjsua_recorder_destroy(pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id < PJ_ARRAY_SIZE(pjsua.recorder), PJ_EINVAL);

    if (pjsua.recorder[id].port) {
	pjmedia_port_destroy(pjsua.recorder[id].port);
	pjsua.recorder[id].port = NULL;
	pjsua.recorder[id].slot = 0xFFFF;
	pjsua.recorder_cnt--;
    }

    return PJ_SUCCESS;
}

/**
 * Enum sound devices.
 */
PJ_DEF(pj_status_t) pjsua_enum_snd_devices( unsigned *count,
					    pjmedia_snd_dev_info info[])
{
    int i, dev_count;

    dev_count = pjmedia_snd_get_dev_count();
    if (dev_count > (int)*count)
	dev_count = *count;

    for (i=0; i<dev_count; ++i) {
	const pjmedia_snd_dev_info *dev_info;
	dev_info = pjmedia_snd_get_dev_info(i);
	pj_memcpy(&info[i], dev_info, sizeof(pjmedia_snd_dev_info));
    }

    *count = dev_count;
    return PJ_SUCCESS;
}


/**
 * Select or change sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_snd_dev( int snd_capture_id,
				       int snd_player_id)
{
    pjsua.config.snd_capture_id = snd_capture_id;
    pjsua.config.snd_player_id = snd_player_id;
    return PJ_SUCCESS;
}


/*
 * Destroy pjsua.
 */
PJ_DEF(pj_status_t) pjsua_destroy(void)
{
    int i;  /* Must be signed */

    /* Signal threads to quit: */
    pjsua.quit_flag = 1;

    /* Wait worker threads to quit: */
    for (i=0; i<(int)pjsua.config.thread_cnt; ++i) {
	
	if (pjsua.threads[i]) {
	    pj_thread_join(pjsua.threads[i]);
	    pj_thread_destroy(pjsua.threads[i]);
	    pjsua.threads[i] = NULL;
	}
    }

    
    if (pjsua.endpt) {
	/* Terminate all calls. */
	pjsua_call_hangup_all();

	/* Terminate all presence subscriptions. */
	pjsua_pres_shutdown();

	/* Unregister, if required: */
	for (i=0; i<(int)pjsua.config.acc_cnt; ++i) {
	    if (pjsua.acc[i].regc) {
		pjsua_acc_set_registration(i, PJ_FALSE);
	    }
	}

	/* Wait for some time to allow unregistration to complete: */
	PJ_LOG(4,(THIS_FILE, "Shutting down..."));
	busy_sleep(1000);
    }

    /* If we have master port, destroying master port will recursively
     * destroy conference bridge, otherwise must destroy it manually. 
     */
    if (pjsua.master_port) {
	pjmedia_master_port_destroy(pjsua.master_port);
	pjsua.master_port = NULL;
    } else {
	if (pjsua.snd_port) {
	    pjmedia_snd_port_destroy(pjsua.snd_port);
	    pjsua.snd_port = NULL;
	}
	if (pjsua.mconf) {
	    pjmedia_conf_destroy(pjsua.mconf);
	    pjsua.mconf = NULL;
	}
    }

    /* Destroy file players */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.player); ++i) {
	if (pjsua.player[i].port) {
	    pjmedia_port_destroy(pjsua.player[i].port);
	    pjsua.player[i].port = NULL;
	}
    }


    /* Destroy file recorders */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.recorder); ++i) {
	if (pjsua.recorder[i].port) {
	    pjmedia_port_destroy(pjsua.recorder[i].port);
	    pjsua.recorder[i].port = NULL;
	}
    }


    /* Close transports */
    for (i=0; i<(int)pjsua.config.max_calls; ++i) {
	if (pjsua.calls[i].med_tp) {
	    (*pjsua.calls[i].med_tp->op->destroy)(pjsua.calls[i].med_tp);
	    pjsua.calls[i].med_tp = NULL;
	}
    }

    /* Destroy media endpoint. */
    if (pjsua.med_endpt) {

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


	pjmedia_endpt_destroy(pjsua.med_endpt);
	pjsua.med_endpt = NULL;
    }

    /* Destroy endpoint. */
    if (pjsua.endpt) {
	pjsip_endpt_destroy(pjsua.endpt);
	pjsua.endpt = NULL;
    }

    /* Destroy caching pool. */
    pj_caching_pool_destroy(&pjsua.cp);


    PJ_LOG(4,(THIS_FILE, "PJSUA destroyed..."));

    /* End logging */
    logging_shutdown();

    /* Done. */

    return PJ_SUCCESS;
}


/**
 * Get SIP endpoint instance.
 * Only valid after pjsua_init().
 */
PJ_DEF(pjsip_endpoint*) pjsua_get_pjsip_endpt(void)
{
    return pjsua.endpt;
}

/**
 * Get media endpoint instance.
 * Only valid after pjsua_init().
 */
PJ_DEF(pjmedia_endpt*) pjsua_get_pjmedia_endpt(void)
{
    return pjsua.med_endpt;
}


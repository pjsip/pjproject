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
void pjsua_default(void)
{
    unsigned i;


    /* Normally need another thread for console application, because main 
     * thread will be blocked in fgets().
     */
    pjsua.thread_cnt = 1;


    /* Default transport settings: */
    pjsua.sip_port = 5060;


    /* Default we start RTP at port 4000 */
    pjsua.start_rtp_port = 4000;


    /* Default logging settings: */
    pjsua.log_level = 5;
    pjsua.app_log_level = 4;
    pjsua.log_decor = PJ_LOG_HAS_SENDER | PJ_LOG_HAS_TIME | 
		      PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_NEWLINE;


    /* Default call settings. */
    pjsua.uas_refresh = -1;
    pjsua.uas_duration = -1;

    /* Default: do not use STUN: */
    pjsua.stun_port1 = pjsua.stun_port2 = 0;

    /* Default for media: */
    pjsua.clock_rate = 8000;
    pjsua.complexity = -1;
    pjsua.quality = 4;


    /* Init accounts: */
    pjsua.acc_cnt = 1;
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.acc); ++i) {
	pjsua.acc[i].index = i;
	pjsua.acc[i].local_uri = pj_str(PJSUA_LOCAL_URI);
	pjsua.acc[i].reg_timeout = 55;
	pjsua.acc[i].online_status = PJ_TRUE;
	pj_list_init(&pjsua.acc[i].route_set);
	pj_list_init(&pjsua.acc[i].pres_srv_list);
    }

    /* Init call array: */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.calls); ++i) {
	pjsua.calls[i].index = i;
	pjsua.calls[i].refresh_tm._timer_id = -1;
	pjsua.calls[i].hangup_tm._timer_id = -1;
    }

    /* Default max nb of calls. */
    pjsua.max_calls = 4;

    /* Init server presence subscription list: */
    

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


/* 
 * Initialize sockets and optionally get the public address via STUN. 
 */
static pj_status_t init_sockets(pj_bool_t sip,
				pjmedia_sock_info *skinfo)
{
    enum { 
	RTP_RETRY = 100
    };
    enum {
	SIP_SOCK,
	RTP_SOCK,
	RTCP_SOCK,
    };
    int i;
    static pj_uint16_t rtp_port;
    pj_sock_t sock[3];
    pj_sockaddr_in mapped_addr[3];
    pj_status_t status = PJ_SUCCESS;

    if (rtp_port == 0)
	rtp_port = (pj_uint16_t)pjsua.start_rtp_port;

    for (i=0; i<3; ++i)
	sock[i] = PJ_INVALID_SOCKET;

    /* Create and bind SIP UDP socket. */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock[SIP_SOCK]);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "socket() error", status);
	goto on_error;
    }

    if (sip) {
	status = pj_sock_bind_in(sock[SIP_SOCK], 0, pjsua.sip_port);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "bind() error", status);
	    goto on_error;
	}
    } else {
	status = pj_sock_bind_in(sock[SIP_SOCK], 0, 0);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "bind() error", status);
	    goto on_error;
	}
    }


    /* Loop retry to bind RTP and RTCP sockets. */
    for (i=0; i<RTP_RETRY; ++i, rtp_port += 2) {

	/* Create and bind RTP socket. */
	status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock[RTP_SOCK]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    goto on_error;
	}

	status = pj_sock_bind_in(sock[RTP_SOCK], 0, rtp_port);
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[RTP_SOCK]); 
	    sock[RTP_SOCK] = PJ_INVALID_SOCKET;
	    continue;
	}

	/* Create and bind RTCP socket. */
	status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock[RTCP_SOCK]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    goto on_error;
	}

	status = pj_sock_bind_in(sock[RTCP_SOCK], 0, (pj_uint16_t)(rtp_port+1));
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[RTP_SOCK]); 
	    sock[RTP_SOCK] = PJ_INVALID_SOCKET;

	    pj_sock_close(sock[RTCP_SOCK]); 
	    sock[RTCP_SOCK] = PJ_INVALID_SOCKET;
	    continue;
	}

	/*
	 * If we're configured to use STUN, then find out the mapped address,
	 * and make sure that the mapped RTCP port is adjacent with the RTP.
	 */
	if (pjsua.stun_port1 == 0) {
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

	    for (i=0; i<3; ++i)
		pj_memcpy(&mapped_addr[i], &addr, sizeof(addr));

	    if (sip) {
		mapped_addr[SIP_SOCK].sin_port = 
		    pj_htons((pj_uint16_t)pjsua.sip_port);
	    }
	    mapped_addr[RTP_SOCK].sin_port=pj_htons((pj_uint16_t)rtp_port);
	    mapped_addr[RTCP_SOCK].sin_port=pj_htons((pj_uint16_t)(rtp_port+1));
	    break;

	} else {
	    status=pj_stun_get_mapped_addr(&pjsua.cp.factory, 3, sock,
					   &pjsua.stun_srv1, pjsua.stun_port1,
					   &pjsua.stun_srv2, pjsua.stun_port2,
					   mapped_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "STUN error", status);
		goto on_error;
	    }

	    if (pj_ntohs(mapped_addr[2].sin_port) == 
		pj_ntohs(mapped_addr[1].sin_port)+1)
	    {
		break;
	    }

	    pj_sock_close(sock[RTP_SOCK]); 
	    sock[RTP_SOCK] = PJ_INVALID_SOCKET;

	    pj_sock_close(sock[RTCP_SOCK]); 
	    sock[RTCP_SOCK] = PJ_INVALID_SOCKET;
	}
    }

    if (sock[RTP_SOCK] == PJ_INVALID_SOCKET) {
	PJ_LOG(1,(THIS_FILE, 
		  "Unable to find appropriate RTP/RTCP ports combination"));
	goto on_error;
    }

    if (sip) {
	pjsua.sip_sock = sock[SIP_SOCK];
	pj_memcpy(&pjsua.sip_sock_name, 
		  &mapped_addr[SIP_SOCK], 
		  sizeof(pj_sockaddr_in));
    } else {
	pj_sock_close(sock[0]);
    }

    skinfo->rtp_sock = sock[RTP_SOCK];
    pj_memcpy(&skinfo->rtp_addr_name, 
	      &mapped_addr[RTP_SOCK], sizeof(pj_sockaddr_in));

    skinfo->rtcp_sock = sock[RTCP_SOCK];
    pj_memcpy(&skinfo->rtcp_addr_name, 
	      &mapped_addr[RTCP_SOCK], sizeof(pj_sockaddr_in));

    if (sip) {
	PJ_LOG(4,(THIS_FILE, "SIP UDP socket reachable at %s:%d",
		  pj_inet_ntoa(pjsua.sip_sock_name.sin_addr), 
		  pj_ntohs(pjsua.sip_sock_name.sin_port)));
    }
    PJ_LOG(4,(THIS_FILE, "RTP socket reachable at %s:%d",
	      pj_inet_ntoa(skinfo->rtp_addr_name.sin_addr), 
	      pj_ntohs(skinfo->rtp_addr_name.sin_port)));
    PJ_LOG(4,(THIS_FILE, "RTCP UDP socket reachable at %s:%d",
	      pj_inet_ntoa(skinfo->rtcp_addr_name.sin_addr), 
	      pj_ntohs(skinfo->rtcp_addr_name.sin_port)));

    rtp_port += 2;
    return PJ_SUCCESS;

on_error:
    for (i=0; i<3; ++i) {
	if (sip && i==0)
	    continue;
	if (sock[i] != PJ_INVALID_SOCKET)
	    pj_sock_close(sock[i]);
    }
    return status;
}



/* 
 * Initialize stack. 
 */
static pj_status_t init_stack(void)
{
    pj_status_t status;

    /* Create global endpoint: */

    {
	const pj_str_t *hostname;
	const char *endpt_name;

	/* Endpoint MUST be assigned a globally unique name.
	 * The name will be used as the hostname in Warning header.
	 */

	/* For this implementation, we'll use hostname for simplicity */
	hostname = pj_gethostname();
	endpt_name = hostname->ptr;

	/* Create the endpoint: */

	status = pjsip_endpt_create(&pjsua.cp.factory, endpt_name, 
				    &pjsua.endpt);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create SIP endpoint", status);
	    return status;
	}
    }


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

    /* Done */

    return PJ_SUCCESS;


on_error:
    pjsip_endpt_destroy(pjsua.endpt);
    pjsua.endpt = NULL;
    return status;
}


static int PJ_THREAD_FUNC pjsua_poll(void *arg)
{
    pj_status_t last_err = 0;

    PJ_UNUSED_ARG(arg);

    do {
	pj_time_val timeout = { 0, 10 };
	pj_status_t status;
	
	status = pjsip_endpt_handle_events (pjsua.endpt, &timeout);
	if (status != last_err) {
	    last_err = status;
	    pjsua_perror(THIS_FILE, "handle_events() returned error", status);
	}
    } while (!pjsua.quit_flag);

    return 0;
}

/*
 * Initialize pjsua application.
 * This will initialize all libraries, create endpoint instance, and register
 * pjsip modules.
 */
pj_status_t pjsua_init(void)
{
    pj_status_t status;

    /* Init PJLIB logging: */

    pj_log_set_level(pjsua.log_level);
    pj_log_set_decor(pjsua.log_decor);


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


    /* Init PJSIP : */

    status = init_stack();
    if (status != PJ_SUCCESS) {
	pj_caching_pool_destroy(&pjsua.cp);
	pjsua_perror(THIS_FILE, "Stack initialization has returned error", 
		     status);
	return status;
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


    /* Init media endpoint: */

    status = pjmedia_endpt_create(&pjsua.cp.factory, &pjsua.med_endpt);
    if (status != PJ_SUCCESS) {
	pj_caching_pool_destroy(&pjsua.cp);
	pjsua_perror(THIS_FILE, 
		     "Media stack initialization has returned error", 
		     status);
	return status;
    }

    /* Done. */
    return PJ_SUCCESS;
}


/*
 * Find account for incoming request.
 */
int pjsua_find_account_for_incoming(pjsip_rx_data *rdata)
{
    pjsip_uri *uri;
    pjsip_sip_uri *sip_uri;
    int acc_index;

    uri = rdata->msg_info.to->uri;

    /* Just return account #0 if To URI is not SIP: */
    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri)) 
    {
	return 0;
    }


    sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);

    /* Find account which has matching username and domain. */
    for (acc_index=0; acc_index < pjsua.acc_cnt; ++acc_index) {

	pjsua_acc *acc = &pjsua.acc[acc_index];

	if (pj_stricmp(&acc->user_part, &sip_uri->user)==0 &&
	    pj_stricmp(&acc->host_part, &sip_uri->host)==0) 
	{
	    /* Match ! */
	    return acc_index;
	}
    }

    /* No matching, try match domain part only. */
    for (acc_index=0; acc_index < pjsua.acc_cnt; ++acc_index) {

	pjsua_acc *acc = &pjsua.acc[acc_index];

	if (pj_stricmp(&acc->host_part, &sip_uri->host)==0) {
	    /* Match ! */
	    return acc_index;
	}
    }

    /* Still no match, just return account #0 */
    return 0;
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
 * Init media.
 */
static pj_status_t init_media(void)
{
    unsigned options;
    pj_status_t status;

    /* If user doesn't specify any codecs, register all of them. */
    if (pjsua.codec_cnt == 0) {

	unsigned option = PJMEDIA_SPEEX_NO_WB | PJMEDIA_SPEEX_NO_UWB;

	/* Register speex. */
	if (pjsua.clock_rate >= 16000)
	    option &= ~(PJMEDIA_SPEEX_NO_WB);
	if (pjsua.clock_rate >= 32000)
	    option &= ~(PJMEDIA_SPEEX_NO_UWB);

	status = pjmedia_codec_speex_init(pjsua.med_endpt, option, 
					  pjsua.quality, pjsua.complexity );
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error initializing Speex codec",
		         status);
	    return status;
	}

	pjsua.codec_arg[pjsua.codec_cnt] = pj_str("speex");
	pjsua.codec_deinit[pjsua.codec_cnt] = &pjmedia_codec_speex_deinit;
	pjsua.codec_cnt++;

	/* Register GSM */
	status = pjmedia_codec_gsm_init(pjsua.med_endpt);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error initializing GSM codec",
		         status);
	    return status;
	}

	pjsua.codec_arg[pjsua.codec_cnt] = pj_str("gsm");
	pjsua.codec_deinit[pjsua.codec_cnt] = &pjmedia_codec_gsm_deinit;
	pjsua.codec_cnt++;

	/* Register PCMA and PCMU */
	status = pjmedia_codec_g711_init(pjsua.med_endpt);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error initializing G711 codec",
		         status);
	    return status;
	}

	pjsua.codec_arg[pjsua.codec_cnt] = pj_str("pcmu");
	pjsua.codec_deinit[pjsua.codec_cnt] = &pjmedia_codec_g711_deinit;
	pjsua.codec_cnt++;
	pjsua.codec_arg[pjsua.codec_cnt] = pj_str("pcma");
	pjsua.codec_deinit[pjsua.codec_cnt] = &pjmedia_codec_g711_deinit;
	pjsua.codec_cnt++;

    } else {

	/* If user specifies the exact codec to be used, then create only
	 * those codecs.
	 */
	int i;

	for (i=0; i<pjsua.codec_cnt; ++i) {
	
	    /* Is it speex? */
	    if (!pj_stricmp2(&pjsua.codec_arg[i], "speex")) {

		unsigned option = PJMEDIA_SPEEX_NO_WB | PJMEDIA_SPEEX_NO_UWB;

		/* Register speex. */
		if (pjsua.clock_rate >= 16000)
		    option &= ~(PJMEDIA_SPEEX_NO_WB);
		if (pjsua.clock_rate >= 32000)
		    option &= ~(PJMEDIA_SPEEX_NO_UWB);

		status = pjmedia_codec_speex_init(pjsua.med_endpt, option,
						  -1, -1);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error initializing Speex codec",
			         status);
		    return status;
		}

		pjsua.codec_deinit[i] = &pjmedia_codec_speex_deinit;
	    }
	    /* Is it gsm? */
	    else if (!pj_stricmp2(&pjsua.codec_arg[i], "gsm")) {

		status = pjmedia_codec_gsm_init(pjsua.med_endpt);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error initializing GSM codec",
			         status);
		    return status;
		}

		pjsua.codec_deinit[i] = &pjmedia_codec_gsm_deinit;

	    }
	    /* Is it pcma/pcmu? */
	    else if (!pj_stricmp2(&pjsua.codec_arg[i], "pcmu") ||
		     !pj_stricmp2(&pjsua.codec_arg[i], "pcma"))
	    {

		status = pjmedia_codec_g711_init(pjsua.med_endpt);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error initializing G711 codec",
			         status);
		    return status;
		}

		pjsua.codec_deinit[i] = &pjmedia_codec_g711_deinit;

	    }
	    /* Don't know about this codec... */
	    else {

		PJ_LOG(1,(THIS_FILE, "Error: unsupported codecs %s",
			  pjsua.codec_arg[i].ptr));
		return PJMEDIA_CODEC_EUNSUP;
	    }
	}
    }

    /* Init options for conference bridge. */
    options = 0;
    if (pjsua.no_mic)
	options |= PJMEDIA_CONF_NO_MIC;

    /* Init conference bridge. */

    status = pjmedia_conf_create(pjsua.pool, 
				 pjsua.max_calls+PJSUA_CONF_MORE_PORTS, 
				 pjsua.clock_rate, 
				 pjsua.clock_rate * 20 / 1000, 16, 
				 options,
				 &pjsua.mconf);
    if (status != PJ_SUCCESS) {
	pj_caching_pool_destroy(&pjsua.cp);
	pjsua_perror(THIS_FILE, 
		     "Media stack initialization has returned error", 
		     status);
	return status;
    }

    /* Add NULL port to the bridge. */
    status = pjmedia_null_port_create( pjsua.pool, pjsua.clock_rate, 
				       pjsua.clock_rate * 20 / 1000, 16,
				       &pjsua.null_port);
    pjmedia_conf_add_port( pjsua.mconf, pjsua.pool, pjsua.null_port, 
			   &pjsua.null_port->info.name, NULL );

    /* Create WAV file player if required: */

    if (pjsua.wav_file) {
	pj_str_t port_name;

	/* Create the file player port. */
	status = pjmedia_file_player_port_create( pjsua.pool, pjsua.wav_file,
						  0, -1, NULL, 
						  &pjsua.file_port);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, 
			 "Error playing media file", 
			 status);
	    return status;
	}

	/* Add port to conference bridge: */
	status = pjmedia_conf_add_port(pjsua.mconf, pjsua.pool, 
				       pjsua.file_port, 
				       pj_cstr(&port_name, pjsua.wav_file),
				       &pjsua.wav_slot);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, 
			 "Unable to add file player to conference bridge", 
			 status);
	    return status;
	}
    }


    return PJ_SUCCESS;
}


/*
 * Start pjsua stack.
 * This will start the registration process, if registration is configured.
 */
pj_status_t pjsua_start(void)
{
    int i;  /* Must be signed */
    pjsip_transport *udp_transport;
    pj_status_t status = PJ_SUCCESS;

    /*
     * Init media subsystem (codecs, conference bridge, et all).
     */
    status = init_media();
    if (status != PJ_SUCCESS)
	return status;

    /* Init sockets (STUN etc): */
    for (i=0; i<(int)pjsua.max_calls; ++i) {
	status = init_sockets(i==0, &pjsua.calls[i].skinfo);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "init_sockets() has returned error", 
			 status);
	    --i;
	    if (i >= 0)
		pj_sock_close(pjsua.sip_sock);
	    while (i >= 0) {
		pj_sock_close(pjsua.calls[i].skinfo.rtp_sock);
		pj_sock_close(pjsua.calls[i].skinfo.rtcp_sock);
	    }
	    return status;
	}
    }

    /* Add UDP transport: */

    {
	/* Init the published name for the transport.
         * Depending whether STUN is used, this may be the STUN mapped
	 * address, or socket's bound address.
	 */
	pjsip_host_port addr_name;

	addr_name.host.ptr = pj_inet_ntoa(pjsua.sip_sock_name.sin_addr);
	addr_name.host.slen = pj_ansi_strlen(addr_name.host.ptr);
	addr_name.port = pj_ntohs(pjsua.sip_sock_name.sin_port);

	/* Create UDP transport from previously created UDP socket: */

	status = pjsip_udp_transport_attach( pjsua.endpt, pjsua.sip_sock,
					     &addr_name, 1, 
					     &udp_transport);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to start UDP transport", 
			 status);
	    return status;
	}
    }

    /* Initialize Contact URI, if one is not specified: */
    for (i=0; i<pjsua.acc_cnt; ++i) {

	pjsip_uri *uri;
	pjsip_sip_uri *sip_uri;

	/* Need to parse local_uri to get the elements: */

	uri = pjsip_parse_uri(pjsua.pool, pjsua.acc[i].local_uri.ptr,
			      pjsua.acc[i].local_uri.slen, 0);
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

	pjsua.acc[i].user_part = sip_uri->user;
	pjsua.acc[i].host_part = sip_uri->host;

	if (pjsua.acc[i].contact_uri.slen == 0 && 
	    pjsua.acc[i].local_uri.slen) 
	{
	    char contact[128];
	    int len;

	    /* The local Contact is the username@ip-addr, where
	     *  - username is taken from the local URI,
	     *  - ip-addr in UDP transport's address name (which may have been
	     *    resolved from STUN.
	     */
	    
	    /* Build temporary contact string. */

	    if (sip_uri->user.slen) {

		/* With the user part. */
		len = pj_ansi_snprintf(contact, sizeof(contact),
				  "<sip:%.*s@%.*s:%d>",
				  (int)sip_uri->user.slen,
				  sip_uri->user.ptr,
				  (int)udp_transport->local_name.host.slen,
				  udp_transport->local_name.host.ptr,
				  udp_transport->local_name.port);
	    } else {

		/* Without user part */

		len = pj_ansi_snprintf(contact, sizeof(contact),
				  "<sip:%.*s:%d>",
				  (int)udp_transport->local_name.host.slen,
				  udp_transport->local_name.host.ptr,
				  udp_transport->local_name.port);
	    }

	    if (len < 1 || len >= sizeof(contact)) {
		pjsua_perror(THIS_FILE, "Invalid Contact", PJSIP_EURITOOLONG);
		return PJSIP_EURITOOLONG;
	    }

	    /* Duplicate Contact uri. */

	    pj_strdup2(pjsua.pool, &pjsua.acc[i].contact_uri, contact);

	}
    }

    /* If outbound_proxy is specified, put it in the route_set: */

    if (pjsua.outbound_proxy.slen) {

	pjsip_route_hdr *route;
	const pj_str_t hname = { "Route", 5 };
	int parsed_len;

	route = pjsip_parse_hdr( pjsua.pool, &hname, 
				 pjsua.outbound_proxy.ptr, 
				 pjsua.outbound_proxy.slen,
				   &parsed_len);
	if (route == NULL) {
	    pjsua_perror(THIS_FILE, "Invalid outbound proxy URL", 
			 PJSIP_EINVALIDURI);
	    return PJSIP_EINVALIDURI;
	}

	for (i=0; i<pjsua.acc_cnt; ++i) {
	    pj_list_push_front(&pjsua.acc[i].route_set, route);
	}
    }


    /* Create worker thread(s), if required: */

    for (i=0; i<pjsua.thread_cnt; ++i) {
	status = pj_thread_create( pjsua.pool, "pjsua", &pjsua_poll,
				   NULL, 0, 0, &pjsua.threads[i]);
	if (status != PJ_SUCCESS) {
	    pjsua.quit_flag = 1;
	    for (--i; i>=0; --i) {
		pj_thread_join(pjsua.threads[i]);
		pj_thread_destroy(pjsua.threads[i]);
	    }
	    return status;
	}
    }

    /* Start registration: */

    /* Create client registration session: */
    for (i=0; i<pjsua.acc_cnt; ++i) {
	status = pjsua_regc_init(i);
	if (status != PJ_SUCCESS)
	    return status;

	/* Perform registration, if required. */
	if (pjsua.acc[i].regc) {
	    pjsua_regc_update(i, 1);
	}
    }


    /* Find account for outgoing preence subscription */
    for (i=0; i<pjsua.buddy_cnt; ++i) {
	pjsua.buddies[i].acc_index = 
	    pjsua_find_account_for_outgoing(&pjsua.buddies[i].uri);
    }


    PJ_LOG(3,(THIS_FILE, "PJSUA version %s started", PJ_VERSION));
    return PJ_SUCCESS;
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

/*
 * Destroy pjsua.
 */
pj_status_t pjsua_destroy(void)
{
    int i;  /* Must be signed */

    /* Signal threads to quit: */
    pjsua.quit_flag = 1;

    /* Terminate all calls. */
    pjsua_call_hangup_all();

    /* Terminate all presence subscriptions. */
    pjsua_pres_shutdown();

    /* Unregister, if required: */
    for (i=0; i<pjsua.acc_cnt; ++i) {
	if (pjsua.acc[i].regc) {
	    pjsua_regc_update(i, 0);
	}
    }

    /* Wait worker threads to quit: */
    for (i=0; i<pjsua.thread_cnt; ++i) {
	
	if (pjsua.threads[i]) {
	    pj_thread_join(pjsua.threads[i]);
	    pj_thread_destroy(pjsua.threads[i]);
	    pjsua.threads[i] = NULL;
	}
    }


    /* Wait for some time to allow unregistration to complete: */
    PJ_LOG(4,(THIS_FILE, "Shutting down..."));
    busy_sleep(1000);

    /* Destroy conference bridge. */
    if (pjsua.mconf)
	pjmedia_conf_destroy(pjsua.mconf);

    /* Destroy file port */
    pjmedia_port_destroy(pjsua.file_port);

    /* Destroy null port. */
    pjmedia_port_destroy(pjsua.null_port);


    /* Destroy sound framework: 
     * (this should be done in pjmedia_shutdown())
     */
    pj_snd_deinit();

    /* Shutdown all codecs: */
    for (i = pjsua.codec_cnt-1; i >= 0; --i) {
	(*pjsua.codec_deinit[i])();
    }

    /* Destroy media endpoint. */

    pjmedia_endpt_destroy(pjsua.med_endpt);

    /* Destroy endpoint. */

    pjsip_endpt_destroy(pjsua.endpt);
    pjsua.endpt = NULL;

    /* Destroy caching pool. */

    pj_caching_pool_destroy(&pjsua.cp);


    /* Done. */

    return PJ_SUCCESS;
}


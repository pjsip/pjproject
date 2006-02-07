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
#include "pjsua.h"


#define THIS_FILE   "pjsua.c"

struct pjsua_t pjsua;

#define PJSUA_LOCAL_URI	    "<sip:user@127.0.0.1>"

static char *PJSUA_DUMMY_SDP_OFFER = 
	    "v=0\r\n"
	    "o=offer 2890844526 2890844526 IN IP4 127.0.0.1\r\n"
	    "s= \r\n"
	    "c=IN IP4 127.0.0.1\r\n"
	    "t=0 0\r\n"
	    "m=audio 49170 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n";

static char *PJSUA_DUMMY_SDP_ANSWER = 
	    "v=0\r\n"
	    "o=answer 2890844730 2890844730 IN IP4 127.0.0.1\r\n"
	    "s= \r\n"
	    "c=IN IP4 127.0.0.1\r\n"
	    "t=0 0\r\n"
	    "m=audio 49920 RTP/AVP 0\r\n"
	    "a=rtpmap:0 PCMU/8000\r\n";

/*
 * Init default application parameters.
 */
void pjsua_default(void)
{

    /* Normally need another thread for console application, because main 
     * thread will be blocked in fgets().
     */
    pjsua.thread_cnt = 1;


    /* Default transport settings: */

    pjsua.sip_port = 5060;


    /* Default logging settings: */

    pjsua.log_level = 5;
    pjsua.app_log_level = 4;
    pjsua.log_decor = PJ_LOG_HAS_SENDER | PJ_LOG_HAS_TIME | 
		      PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_NEWLINE;

    /* Default: do not use STUN: */

    pjsua.stun_port1 = pjsua.stun_port2 = 0;

    /* Default URIs: */

    pjsua.local_uri = pj_str(PJSUA_LOCAL_URI);
}


/*
 * Display error message for the specified error code.
 */
void pjsua_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(1,(THIS_FILE, "%s: %s", title, errmsg));
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
    PJ_UNUSED_ARG(rdata);
    PJ_TODO(IMPLEMENT_UAS);
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
    PJ_TODO(IMPLEMENT_UAS);
    return PJ_FALSE;
}


/*
 * This callback receives notification from invite session when the
 * session state has changed.
 */
static void pjsua_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
    pjsua_ui_inv_on_state_changed(inv, e);
}


/*
 * This callback is called by invite session framework when UAC session
 * has forked.
 */
static void pjsua_inv_on_new_session(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);

    PJ_TODO(HANDLE_FORKED_DIALOG);
}

/* 
 * Initialize sockets and optionally get the public address via STUN. 
 */
static pj_status_t init_sockets()
{
    enum { 
	RTP_START_PORT = 4000,
	RTP_RANDOM_START = 2,
	RTP_RETRY = 10 
    };
    enum {
	SIP_SOCK,
	RTP_SOCK,
	RTCP_SOCK,
    };
    int i;
    pj_uint16_t rtp_port;
    pj_sock_t sock[3];
    pj_sockaddr_in mapped_addr[3];
    pj_status_t status;

    for (i=0; i<3; ++i)
	sock[i] = PJ_INVALID_SOCKET;

    /* Create and bind SIP UDP socket. */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock[SIP_SOCK]);
    if (status != PJ_SUCCESS) {
	pjsua_perror("socket() error", status);
	goto on_error;
    }
    
    status = pj_sock_bind_in(sock[SIP_SOCK], 0, pjsua.sip_port);
    if (status != PJ_SUCCESS) {
	pjsua_perror("bind() error", status);
	goto on_error;
    }

    /* Initialize start of RTP port to try. */
    rtp_port = (pj_uint16_t)(RTP_START_PORT + (pj_rand() % RTP_RANDOM_START) / 2);

    /* Loop retry to bind RTP and RTCP sockets. */
    for (i=0; i<RTP_RETRY; ++i, rtp_port += 2) {

	/* Create and bind RTP socket. */
	status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock[RTP_SOCK]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror("socket() error", status);
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
	    pjsua_perror("socket() error", status);
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
		pjsua_perror("Unresolvable local hostname", status);
		goto on_error;
	    }

	    for (i=0; i<3; ++i)
		pj_memcpy(&mapped_addr[i], &addr, sizeof(addr));

	    mapped_addr[SIP_SOCK].sin_port = pj_htons((pj_uint16_t)pjsua.sip_port);
	    mapped_addr[RTP_SOCK].sin_port = pj_htons((pj_uint16_t)rtp_port);
	    mapped_addr[RTCP_SOCK].sin_port = pj_htons((pj_uint16_t)(rtp_port+1));
	    break;
	} else {
	    status = pj_stun_get_mapped_addr( &pjsua.cp.factory, 3, sock,
					      &pjsua.stun_srv1, pjsua.stun_port1,
					      &pjsua.stun_srv2, pjsua.stun_port2,
					      mapped_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror("STUN error", status);
		goto on_error;
	    }

	    if (pj_ntohs(mapped_addr[2].sin_port) == pj_ntohs(mapped_addr[1].sin_port)+1)
		break;

	    pj_sock_close(sock[RTP_SOCK]); sock[RTP_SOCK] = PJ_INVALID_SOCKET;
	    pj_sock_close(sock[RTCP_SOCK]); sock[RTCP_SOCK] = PJ_INVALID_SOCKET;
	}
    }

    if (sock[RTP_SOCK] == PJ_INVALID_SOCKET) {
	PJ_LOG(1,(THIS_FILE, "Unable to find appropriate RTP/RTCP ports combination"));
	goto on_error;
    }

    pjsua.sip_sock = sock[SIP_SOCK];
    pj_memcpy(&pjsua.sip_sock_name, &mapped_addr[SIP_SOCK], sizeof(pj_sockaddr_in));
    pjsua.rtp_sock = sock[RTP_SOCK];
    pj_memcpy(&pjsua.rtp_sock_name, &mapped_addr[RTP_SOCK], sizeof(pj_sockaddr_in));
    pjsua.rtcp_sock = sock[RTCP_SOCK];
    pj_memcpy(&pjsua.rtcp_sock_name, &mapped_addr[RTCP_SOCK], sizeof(pj_sockaddr_in));

    PJ_LOG(4,(THIS_FILE, "SIP UDP socket reachable at %s:%d",
	      pj_inet_ntoa(pjsua.sip_sock_name.sin_addr), 
	      pj_ntohs(pjsua.sip_sock_name.sin_port)));
    PJ_LOG(4,(THIS_FILE, "RTP socket reachable at %s:%d",
	      pj_inet_ntoa(pjsua.rtp_sock_name.sin_addr), 
	      pj_ntohs(pjsua.rtp_sock_name.sin_port)));
    PJ_LOG(4,(THIS_FILE, "RTCP UDP socket reachable at %s:%d",
	      pj_inet_ntoa(pjsua.rtcp_sock_name.sin_addr), 
	      pj_ntohs(pjsua.rtcp_sock_name.sin_port)));

    return PJ_SUCCESS;

on_error:
    for (i=0; i<3; ++i) {
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
	    pjsua_perror("Unable to create SIP endpoint", status);
	    return status;
	}
    }


    /* Initialize transaction layer: */

    status = pjsip_tsx_layer_init(pjsua.endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror("Transaction layer initialization error", status);
	goto on_error;
    }

    /* Initialize UA layer module: */

    status = pjsip_ua_init( pjsua.endpt, NULL );
    if (status != PJ_SUCCESS) {
	pjsua_perror("UA layer initialization error", status);
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
	NULL,			    /* User data.			*/
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
	    pjsua_perror("Unable to register pjsua module", status);
	    goto on_error;
	}
    }

    /* Initialize invite session module: */

    {
	
	/* Initialize invite session callback. */
	pjsip_inv_callback inv_cb;

	pj_memset(&inv_cb, 0, sizeof(inv_cb));
	inv_cb.on_state_changed = &pjsua_inv_on_state_changed;
	inv_cb.on_new_session = &pjsua_inv_on_new_session;

	/* Initialize invite session module: */
	status = pjsip_inv_usage_init(pjsua.endpt, &pjsua.mod, &inv_cb);
	if (status != PJ_SUCCESS) {
	    pjsua_perror("Invite usage initialization error", status);
	    goto on_error;
	}

    }


    /* Done */

    return PJ_SUCCESS;


on_error:
    pjsip_endpt_destroy(pjsua.endpt);
    pjsua.endpt = NULL;
    return status;
}


static int PJ_THREAD_FUNC pjsua_worker_thread(void *arg)
{
    PJ_UNUSED_ARG(arg);

    while (!pjsua.quit_flag) {
	pj_time_val timeout = { 0, 10 };
	pjsip_endpt_handle_events (pjsua.endpt, &timeout);
    }

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
	pjsua_perror("pj_init() error", status);
	return status;
    }

    /* Init memory pool: */

    /* Init caching pool. */
    pj_caching_pool_init(&pjsua.cp, &pj_pool_factory_default_policy, 0);

    /* Create memory pool for application. */
    pjsua.pool = pj_pool_create(&pjsua.cp.factory, "pjsua", 4000, 4000, NULL);


    /* Init PJSIP and all the modules: */

    status = init_stack();
    if (status != PJ_SUCCESS) {
	pj_caching_pool_destroy(&pjsua.cp);
	pjsua_perror("Stack initialization has returned error", status);
	return status;
    }

    /* Done. */
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
    pj_status_t status;

    /* Init sockets (STUN etc): */

    status = init_sockets();
    if (status != PJ_SUCCESS) {
	pj_caching_pool_destroy(&pjsua.cp);
	pjsua_perror("init_sockets() has returned error", status);
	return status;
    }


    /* Add UDP transport: */

    {
	/* Init the published name for the transport.
         * Depending whether STUN is used, this may be the STUN mapped
	 * address, or socket's bound address.
	 */
	pjsip_host_port addr_name;

	addr_name.host.ptr = pj_inet_ntoa(pjsua.sip_sock_name.sin_addr);
	addr_name.host.slen = pj_native_strlen(addr_name.host.ptr);
	addr_name.port = pj_ntohs(pjsua.sip_sock_name.sin_port);

	/* Create UDP transport from previously created UDP socket: */

	status = pjsip_udp_transport_attach( pjsua.endpt, pjsua.sip_sock,
					     &addr_name, 1, 
					     &udp_transport);
	if (status != PJ_SUCCESS) {
	    pjsua_perror("Unable to start UDP transport", status);
	    return status;
	}
    }

    /* Initialize Contact URI, if one is not specified: */

    if (pjsua.contact_uri.slen == 0 && pjsua.local_uri.slen) {

	pjsip_uri *uri;
	pjsip_sip_uri *sip_uri;
	char contact[128];
	int len;

	/* The local Contact is the username@ip-addr, where
	 *  - username is taken from the local URI,
	 *  - ip-addr in UDP transport's address name (which may have been
	 *    resolved from STUN.
	 */
	
	/* Need to parse local_uri to get the elements: */

	uri = pjsip_parse_uri(pjsua.pool, pjsua.local_uri.ptr, 
			      pjsua.local_uri.slen, 0);
	if (uri == NULL) {
	    pjsua_perror("Invalid local URI", PJSIP_EINVALIDURI);
	    return PJSIP_EINVALIDURI;
	}


	/* Local URI MUST be a SIP or SIPS: */

	if (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri)) {
	    pjsua_perror("Invalid local URI", PJSIP_EINVALIDSCHEME);
	    return PJSIP_EINVALIDSCHEME;
	}


	/* Get the SIP URI object: */

	sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri);

	
	/* Build temporary contact string. */

	if (sip_uri->user.slen) {

	    /* With the user part. */
	    len = pj_snprintf(contact, sizeof(contact),
			      "<sip:%.*s@%.*s:%d>",
			      sip_uri->user.slen,
			      sip_uri->user.ptr,
			      udp_transport->local_name.host.slen,
			      udp_transport->local_name.host.ptr,
			      udp_transport->local_name.port);
	} else {

	    /* Without user part */

	    len = pj_snprintf(contact, sizeof(contact),
			      "<sip:%.*s:%d>",
			      udp_transport->local_name.host.slen,
			      udp_transport->local_name.host.ptr,
			      udp_transport->local_name.port);
	}

	if (len < 1 || len >= sizeof(contact)) {
	    pjsua_perror("Invalid Contact", PJSIP_EURITOOLONG);
	    return PJSIP_EURITOOLONG;
	}

	/* Duplicate Contact uri. */

	pj_strdup2(pjsua.pool, &pjsua.contact_uri, contact);

    }

    /* Initialize global route_set: */

    PJ_TODO(INIT_GLOBAL_ROUTE_SET);


    /* Create worker thread(s), if required: */

    for (i=0; i<pjsua.thread_cnt; ++i) {
	status = pj_thread_create( pjsua.pool, "pjsua", &pjsua_worker_thread,
				   NULL, 0, 0, &pjsua.threads[i]);
	if (status != PJ_SUCCESS) {
	    pjsua.quit_flag = 1;
	    for (--i; i>=0; --i) {
		pj_thread_join(pjsua.threads[i]);
		pj_thread_destroy(pjsua.threads[i]);
	    }
	    pj_caching_pool_destroy(&pjsua.cp);
	    return status;
	}
    }

    /* Start registration: */

    /* Create client registration session: */

    status = pjsua_regc_init();
    if (status != PJ_SUCCESS)
	return status;

    /* Perform registration, if required. */
    if (pjsua.regc) {
	pjsua_regc_update(1);
    }



    return PJ_SUCCESS;
}


/*
 * Destroy pjsua.
 */
pj_status_t pjsua_destroy(void)
{
    int i;

    /* Unregister, if required: */
    if (pjsua.regc) {

	pjsua_regc_update(0);

	/* Wait for some time to allow unregistration to complete: */

	pj_thread_sleep(500);
    }

    /* Signal threads to quit: */

    pjsua.quit_flag = 1;

    /* Wait worker threads to quit: */

    for (i=0; i<pjsua.thread_cnt; ++i) {
	
	pj_thread_join(pjsua.threads[i]);
	pj_thread_destroy(pjsua.threads[i]);
    }

    /* Destroy endpoint. */
    pjsip_endpt_destroy(pjsua.endpt);
    pjsua.endpt = NULL;

    /* Destroy caching pool. */
    pj_caching_pool_destroy(&pjsua.cp);


    /* Done. */

    return PJ_SUCCESS;
}


/**
 * Make outgoing call.
 */
pj_status_t pjsua_invite(const char *cstr_dest_uri,
			 pjsip_inv_session **p_inv)
{
    pj_str_t dest_uri;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *offer;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Convert cstr_dest_uri to dest_uri */
    
    dest_uri = pj_str((char*)cstr_dest_uri);

    /* Create outgoing dialog: */

    status = pjsip_dlg_create_uac( pjsip_ua_instance(), &pjsua.local_uri,
				   &pjsua.contact_uri, &dest_uri, &dest_uri,
				   &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror("Dialog creation failed", status);
	return status;
    }

    /* Create dummy SDP for offer: */

    status = pjmedia_sdp_parse(dlg->pool, PJSUA_DUMMY_SDP_OFFER,
			       pj_native_strlen(PJSUA_DUMMY_SDP_OFFER),
			       &offer);
    if (status != PJ_SUCCESS) {
	pjsua_perror("Dummy SDP offer parsing failed", status);
	goto on_error;
    }

    /* Create the INVITE session: */

    status = pjsip_inv_create_uac( dlg, offer, 0, &inv);
    if (status != PJ_SUCCESS) {
	pjsua_perror("Invite session creation failed", status);
	goto on_error;
    }


    /* Set dialog Route-Set: */

    PJ_TODO(INIT_DIALOG_ROUTE_SET);

    /* Set credentials: */

    pjsip_auth_clt_set_credentials( &dlg->auth_sess, pjsua.cred_count, 
				    pjsua.cred_info);


    /* Create initial INVITE: */

    status = pjsip_inv_invite(inv, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror("Unable to create initial INVITE request", status);
	goto on_error;
    }


    /* Send initial INVITE: */

    status = pjsip_inv_send_msg(inv, tdata, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror("Unable to send initial INVITE request", status);
	goto on_error;
    }


    /* Done. */

    *p_inv = inv;

    return PJ_SUCCESS;


on_error:

    PJ_TODO(DESTROY_DIALOG_ON_FAIL);
    return status;
}


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


/* Include all headers. */
#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjlib-util.h>
#include <pjlib.h>

#include <stdlib.h>

#define THIS_FILE	"siprtp.c"
#define MAX_CALLS	1024
#define RTP_START_PORT	44100


/* A bidirectional media stream */
struct media_stream
{
    /* Static: */
    pj_uint16_t		 port;		    /* RTP port (RTCP is +1)	*/

    /* Current stream info: */
    pjmedia_stream_info	 si;		    /* Current stream info.	*/

    /* More info: */
    unsigned		 clock_rate;	    /* clock rate		*/
    unsigned		 samples_per_frame; /* samples per frame	*/
    unsigned		 bytes_per_frame;   /* frame size.		*/

    /* Sockets: */
    pj_sock_t		 rtp_sock;	    /* RTP socket.		*/
    pj_sock_t		 rtcp_sock;	    /* RTCP socket.		*/

    /* RTP session: */
    pjmedia_rtp_session	 out_sess;	    /* outgoing RTP session	*/
    pjmedia_rtp_session	 in_sess;	    /* incoming RTP session	*/

    /* RTCP stats: */
    pjmedia_rtcp_session rtcp;		    /* incoming RTCP session.	*/
    pjmedia_rtcp_pkt	 rem_rtcp;	    /* received RTCP stat.	*/

    /* Thread: */
    pj_bool_t		 thread_quit_flag;  /* worker thread quit flag	*/
    pj_thread_t		*thread;	    /* RTP/RTCP worker thread	*/
};


struct call
{
    unsigned		 index;
    pjsip_inv_session	*inv;
    unsigned		 media_count;
    struct media_stream	 media[2];
};


static struct app
{
    unsigned		 max_calls;
    unsigned		 thread_count;
    int			 sip_port;
    int			 rtp_start_port;
    char		*local_addr;
    pj_str_t		 local_uri;
    pj_str_t		 local_contact;

    pj_str_t		 uri_to_call;

    pj_caching_pool	 cp;
    pj_pool_t		*pool;

    pjsip_endpoint	*sip_endpt;
    pj_bool_t		 thread_quit;
    pj_thread_t		*thread[1];

    pjmedia_endpt	*med_endpt;
    struct call		 call[MAX_CALLS];
} app;



/*
 * Prototypes:
 */

/* Callback to be called when SDP negotiation is done in the call: */
static void call_on_media_update( pjsip_inv_session *inv,
				  pj_status_t status);

/* Callback to be called when invite session's state has changed: */
static void call_on_state_changed( pjsip_inv_session *inv, 
				   pjsip_event *e);

/* Callback to be called when dialog has forked: */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);

/* Callback to be called to handle incoming requests outside dialogs: */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata );

/* Worker thread prototype */
static int worker_thread(void *arg);

/* Create SDP for call */
static pj_status_t create_sdp( pj_pool_t *pool,
			       struct call *call,
			       pjmedia_sdp_session **p_sdp);

/* Destroy the call's media */
static void destroy_call_media(unsigned call_index);

/* Display error */
static void app_perror(const char *sender, const char *title, 
		       pj_status_t status);


/* This is a PJSIP module to be registered by application to handle
 * incoming requests outside any dialogs/transactions. The main purpose
 * here is to handle incoming INVITE request message, where we will
 * create a dialog and INVITE session for it.
 */
static pjsip_module mod_siprtp =
{
    NULL, NULL,			    /* prev, next.		*/
    { "mod-siprtpapp", 13 },	    /* Name.			*/
    -1,				    /* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
    NULL,			    /* load()			*/
    NULL,			    /* start()			*/
    NULL,			    /* stop()			*/
    NULL,			    /* unload()			*/
    &on_rx_request,		    /* on_rx_request()		*/
    NULL,			    /* on_rx_response()		*/
    NULL,			    /* on_tx_request.		*/
    NULL,			    /* on_tx_response()		*/
    NULL,			    /* on_tsx_state()		*/
};


/*
 * Init SIP stack
 */
static pj_status_t init_sip()
{
    unsigned i;
    pj_status_t status;

    /* init PJLIB-UTIL: */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&app.cp, &pj_pool_factory_default_policy, 0);

    /* Create application pool for misc. */
    app.pool = pj_pool_create(&app.cp.factory, "app", 1000, 1000, NULL);

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

	status = pjsip_endpt_create(&app.cp.factory, endpt_name, 
				    &app.sip_endpt);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    }


    /* Add UDP transport. */
    {
	pj_sockaddr_in addr;

	addr.sin_family = PJ_AF_INET;
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons((pj_uint16_t)app.sip_port);

	status = pjsip_udp_transport_start( app.sip_endpt, &addr, NULL, 
					    1, NULL);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* 
     * Init transaction layer.
     * This will create/initialize transaction hash tables etc.
     */
    status = pjsip_tsx_layer_init_module(app.sip_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /*  Initialize UA layer. */
    status = pjsip_ua_init_module( app.sip_endpt, NULL );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /*  Init invite session module. */
    {
	pjsip_inv_callback inv_cb;

	/* Init the callback for INVITE session: */
	pj_memset(&inv_cb, 0, sizeof(inv_cb));
	inv_cb.on_state_changed = &call_on_state_changed;
	inv_cb.on_new_session = &call_on_forked;
	inv_cb.on_media_update = &call_on_media_update;

	/* Initialize invite session module:  */
	status = pjsip_inv_usage_init(app.sip_endpt, &inv_cb);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Register our module to receive incoming requests. */
    status = pjsip_endpt_register_module( app.sip_endpt, &mod_siprtp);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Start worker threads */
    for (i=0; i<app.thread_count; ++i) {
	pj_thread_create( app.pool, "app", &worker_thread, NULL,
			  0, 0, &app.thread[i]);
    }

    /* Done */
    return PJ_SUCCESS;
}


/*
 * Destroy SIP
 */
static void destroy_sip()
{
    unsigned i;

    app.thread_quit = 1;
    for (i=0; i<app.thread_count; ++i) {
	if (app.thread[i]) {
	    pj_thread_join(app.thread[i]);
	    pj_thread_destroy(app.thread[i]);
	    app.thread[i] = NULL;
	}
    }

    if (app.sip_endpt) {
	pjsip_endpt_destroy(app.sip_endpt);
	app.sip_endpt = NULL;
    }

    if (app.pool) {
	pj_pool_release(app.pool);
	app.pool = NULL;
	pj_caching_pool_destroy(&app.cp);
    }
}


/*
 * Init media stack.
 */
static pj_status_t init_media()
{
    pj_ioqueue_t *ioqueue;
    unsigned	i, count;
    pj_uint16_t	rtp_port;
    pj_str_t	temp;
    pj_sockaddr_in  addr;
    pj_status_t	status;


    /* Get the ioqueue from the SIP endpoint */
    ioqueue = pjsip_endpt_get_ioqueue(app.sip_endpt);


    /* Initialize media endpoint so that at least error subsystem is properly
     * initialized.
     */
    status = pjmedia_endpt_create(&app.cp.factory, ioqueue, 1, 
				  &app.med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Determine address to bind socket */
    pj_memset(&addr, 0, sizeof(addr));
    addr.sin_family = PJ_AF_INET;
    i = pj_inet_aton(pj_cstr(&temp, app.local_addr), &addr.sin_addr);
    if (i == 0) {
	PJ_LOG(3,(THIS_FILE, 
		  "Error: invalid local address %s (expecting IP)",
		  app.local_addr));
	return -1;
    }


    /* RTP port counter */
    rtp_port = (pj_uint16_t)(app.rtp_start_port & 0xFFFE);


    /* Init media sockets. */
    for (i=0, count=0; i<app.max_calls; ++i, ++count) {

	int retry;

	app.call[i].index = i;

	/* Repeat binding media socket to next port when fails to bind
	 * to current port number.
	 */
	retry = 0;
	do {
	    struct media_stream *m = &app.call[i].media[0];

	    ++retry;
	    rtp_port += 2;
	    m->port = rtp_port;

	    /* Create and bind RTP socket */
	    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0,
				    &m->rtp_sock);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    addr.sin_port = pj_htons(rtp_port);
	    status = pj_sock_bind(m->rtp_sock, &addr, sizeof(addr));
	    if (status != PJ_SUCCESS) {
		pj_sock_close(m->rtp_sock), m->rtp_sock=0;
		continue;
	    }


	    /* Create and bind RTCP socket */
	    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0,
				    &m->rtcp_sock);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    addr.sin_port = pj_htons((pj_uint16_t)(rtp_port+1));
	    status = pj_sock_bind(m->rtcp_sock, &addr, sizeof(addr));
	    if (status != PJ_SUCCESS) {
		pj_sock_close(m->rtp_sock), m->rtp_sock=0;
		pj_sock_close(m->rtcp_sock), m->rtcp_sock=0;
		continue;
	    }

	} while (status != PJ_SUCCESS && retry < 100);

	if (status != PJ_SUCCESS)
	    goto on_error;

    }

    /* Done */
    return PJ_SUCCESS;

on_error:
    for (i=0; i<count; ++i) {
	struct media_stream *m = &app.call[i].media[0];

	pj_sock_close(m->rtp_sock), m->rtp_sock=0;
	pj_sock_close(m->rtcp_sock), m->rtcp_sock=0;
    }

    return status;
}


/*
 * Destroy media.
 */
static void destroy_media()
{
    unsigned i;

    for (i=0; i<app.max_calls; ++i) {
	struct media_stream *m = &app.call[i].media[0];

	if (m->rtp_sock)
	    pj_sock_close(m->rtp_sock), m->rtp_sock = 0;

	if (m->rtcp_sock)
	    pj_sock_close(m->rtcp_sock), m->rtcp_sock = 0;
    }

    if (app.med_endpt) {
	pjmedia_endpt_destroy(app.med_endpt);
	app.med_endpt = NULL;
    }
}


/*
 * Make outgoing call.
 */
static pj_status_t make_call(const pj_str_t *dst_uri)
{
    unsigned i;
    struct call *call;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pj_status_t status;


    /* Find unused call slot */
    for (i=0; i<app.max_calls; ++i) {
	if (app.call[i].inv == NULL)
	    break;
    }

    if (i == app.max_calls)
	return PJ_ETOOMANY;

    call = &app.call[i];

    /* Create UAC dialog */
    status = pjsip_dlg_create_uac( pjsip_ua_instance(), 
				   &app.local_uri,	/* local URI	    */
				   &app.local_contact,	/* local Contact    */
				   dst_uri,		/* remote URI	    */
				   dst_uri,		/* remote target    */
				   &dlg);		/* dialog	    */
    if (status != PJ_SUCCESS)
	return status;

    /* Create SDP */
    create_sdp( dlg->pool, call, &sdp);

    /* Create the INVITE session. */
    status = pjsip_inv_create_uac( dlg, sdp, 0, &call->inv);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_terminate(dlg);
	return status;
    }


    /* Attach call data to invite session */
    call->inv->mod_data[mod_siprtp.id] = call;


    /* Create initial INVITE request.
     * This INVITE request will contain a perfectly good request and 
     * an SDP body as well.
     */
    status = pjsip_inv_invite(call->inv, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Send initial INVITE request. 
     * From now on, the invite session's state will be reported to us
     * via the invite session callbacks.
     */
    status = pjsip_inv_send_msg(call->inv, tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    return PJ_SUCCESS;
}


/*
 * Receive incoming call
 */
static void process_incoming_call(pjsip_rx_data *rdata)
{
    unsigned i;
    struct call *call;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Find free call slot */
    for (i=0; i<app.max_calls; ++i) {
	if (app.call[i].inv == NULL)
	    break;
    }

    if (i == app.max_calls) {
	const pj_str_t reason = pj_str("Too many calls");
	pjsip_endpt_respond_stateless( app.sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return;
    }

    call = &app.call[i];

    /* Create UAS dialog */
    status = pjsip_dlg_create_uas( pjsip_ua_instance(), rdata,
				   &app.local_contact, &dlg);
    if (status != PJ_SUCCESS) {
	const pj_str_t reason = pj_str("Unable to create dialog");
	pjsip_endpt_respond_stateless( app.sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return;
    }

    /* Create SDP */
    create_sdp( dlg->pool, call, &sdp);

    /* Create UAS invite session */
    status = pjsip_inv_create_uas( dlg, rdata, sdp, 0, &call->inv);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_terminate(dlg);
	return;
    }
    
    /* Attach call data to invite session */
    call->inv->mod_data[mod_siprtp.id] = call;

    /* Create 200 response .*/
    status = pjsip_inv_initial_answer(call->inv, rdata, 200, 
				      NULL, NULL, &tdata);
    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);

    /* Send the 200 response. */  
    status = pjsip_inv_send_msg(call->inv, tdata); 
    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);


    /* Done */
}


/* Callback to be called when dialog has forked: */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);

    PJ_TODO( HANDLE_FORKING );
}


/* Callback to be called to handle incoming requests outside dialogs: */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata )
{
    /* Respond (statelessly) any non-INVITE requests with 500  */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
	pj_str_t reason = pj_str("Unsupported Operation");
	pjsip_endpt_respond_stateless( app.sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;
    }

    /* Handle incoming INVITE */
    process_incoming_call(rdata);

    /* Done */
    return PJ_TRUE;
}


/* Callback to be called when invite session's state has changed: */
static void call_on_state_changed( pjsip_inv_session *inv, 
				   pjsip_event *e)
{
    PJ_UNUSED_ARG(e);

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
	struct call *call = inv->mod_data[mod_siprtp.id];

	if (!call)
	    return;

	call->inv = NULL;
	inv->mod_data[mod_siprtp.id] = NULL;

	destroy_call_media(call->index);
    }
}


/* Utility */
static void app_perror(const char *sender, const char *title, 
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(3,(sender, "%s: %s [status=%d]", title, errmsg, status));
}


/* Worker thread */
static int worker_thread(void *arg)
{
    PJ_UNUSED_ARG(arg);

    while (!app.thread_quit) {
	pj_time_val timeout = {0, 10};
	pjsip_endpt_handle_events(app.sip_endpt, &timeout);
    }

    return 0;
}


/* Usage */
static const char *USAGE = 
"Usage:							\n"
"   siprtp [options]        => to start in server mode	\n"
"   siprtp [options] URL    => to start in client mode	\n"
"\n"
"where options are:		\n"
"   --count=N,       -c     Set number of calls to create (default:1) \n"
"   --port=PORT      -p     Set local SIP port (default: 5060)	 \n"
"   --rtp-port=PORT  -r     Set start of RTP port (default: 4000)     \n"
"   --ip-addr=IP     -i     Set local IP address to use (otherwise it will\n"
"                           try to determine local IP address from hostname)\n"
;


/* Init application options */
static pj_status_t init_options(int argc, char *argv[])
{
    static char ip_addr[32];
    static char local_uri[64];

    struct pj_getopt_option long_options[] = {
	{ "count",	1, 0, 'c' },
	{ "port",	1, 0, 'p' },
	{ "rtp-port",	1, 0, 'r' },
	{ "ip-addr",	1, 0, 'i' },
	{ NULL, 0, 0, 0 },
    };
    int c;
    int option_index;

    /* Get local IP address for the default IP address */
    {
	const pj_str_t *hostname;
	pj_sockaddr_in tmp_addr;
	char *addr;

	hostname = pj_gethostname();
	pj_sockaddr_in_init(&tmp_addr, hostname, 0);
	addr = pj_inet_ntoa(tmp_addr.sin_addr);
	pj_ansi_strcpy(ip_addr, addr);
    }

    /* Init default */
    app.max_calls = 1;
    app.thread_count = 1;
    app.sip_port = 5060;
    app.rtp_start_port = 4000;
    app.local_addr = ip_addr;

    /* Parse options */
    pj_optind = 0;
    while((c=pj_getopt_long(argc,argv, "c:p:r:i:", 
			    long_options, &option_index))!=-1) 
    {
	switch (c) {
	case 'c':
	    app.max_calls = atoi(pj_optarg);
	    if (app.max_calls < 0 || app.max_calls > MAX_CALLS) {
		PJ_LOG(3,(THIS_FILE, "Invalid max calls value %s", pj_optarg));
		return 1;
	    }
	    break;
	case 'p':
	    app.sip_port = atoi(pj_optarg);
	    break;
	case 'r':
	    app.rtp_start_port = atoi(pj_optarg);
	    break;
	case 'i':
	    app.local_addr = pj_optarg;
	    break;
	default:
	    puts(USAGE);
	    return 1;
	}
    }

    /* Check if URL is specified */
    if (pj_optind < argc)
	app.uri_to_call = pj_str(argv[pj_optind]);

    /* Build local URI and contact */
    pj_ansi_sprintf( local_uri, "sip:%s:%d", app.local_addr, app.sip_port);
    app.local_uri = pj_str(local_uri);
    app.local_contact = app.local_uri;


    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * MEDIA STUFFS
 */

/*
 * Create SDP session for a call.
 */
static pj_status_t create_sdp( pj_pool_t *pool,
			       struct call *call,
			       pjmedia_sdp_session **p_sdp)
{
    pj_time_val tv;
    pjmedia_sdp_session *sdp;
    pjmedia_sdp_media *m;
    pjmedia_sdp_attr *attr;
    struct media_stream *audio = &call->media[0];

    PJ_ASSERT_RETURN(pool && p_sdp, PJ_EINVAL);


    /* Create and initialize basic SDP session */
    sdp = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_session));

    pj_gettimeofday(&tv);
    sdp->origin.user = pj_str("pjsip-siprtp");
    sdp->origin.version = sdp->origin.id = tv.sec + 2208988800UL;
    sdp->origin.net_type = pj_str("IN");
    sdp->origin.addr_type = pj_str("IP4");
    sdp->origin.addr = *pj_gethostname();
    sdp->name = pj_str("pjsip");

    /* Since we only support one media stream at present, put the
     * SDP connection line in the session level.
     */
    sdp->conn = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_conn));
    sdp->conn->net_type = pj_str("IN");
    sdp->conn->addr_type = pj_str("IP4");
    sdp->conn->addr = pj_str(app.local_addr);


    /* SDP time and attributes. */
    sdp->time.start = sdp->time.stop = 0;
    sdp->attr_count = 0;

    /* Create media stream 0: */

    sdp->media_count = 1;
    m = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_media));
    sdp->media[0] = m;

    /* Standard media info: */
    m->desc.media = pj_str("audio");
    m->desc.port = audio->port;
    m->desc.port_count = 1;
    m->desc.transport = pj_str("RTP/AVP");

    /* Add format and rtpmap for each codec. */
    m->desc.fmt_count = 1;
    m->attr_count = 0;

    {
	pjmedia_sdp_rtpmap rtpmap;
	pjmedia_sdp_attr *attr;

	PJ_TODO(PARAMETERIZE_CODEC);

	m->desc.fmt[0] = pj_str("0");
	rtpmap.pt = pj_str("0");
	rtpmap.clock_rate = 8000;
	rtpmap.enc_name = pj_str("pcmu");
	rtpmap.param.slen = 0;

	pjmedia_sdp_rtpmap_to_attr(pool, &rtpmap, &attr);
	m->attr[m->attr_count++] = attr;
    }

    /* Add sendrecv attribute. */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("sendrecv");
    m->attr[m->attr_count++] = attr;

#if 1
    /*
     * Add support telephony event
     */
    m->desc.fmt[m->desc.fmt_count++] = pj_str("101");
    /* Add rtpmap. */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("rtpmap");
    attr->value = pj_str(":101 telephone-event/8000");
    m->attr[m->attr_count++] = attr;
    /* Add fmtp */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("fmtp");
    attr->value = pj_str(":101 0-15");
    m->attr[m->attr_count++] = attr;
#endif

    /* Done */
    *p_sdp = sdp;

    return PJ_SUCCESS;
}


/* Media thread */
static int media_thread(void *arg)
{
    struct media_stream *strm = arg;
    char packet[1500];
    pj_time_val next_rtp, next_rtcp;

    pj_gettimeofday(&next_rtp);
    next_rtp.msec += strm->samples_per_frame * 1000 / strm->clock_rate;
    pj_time_val_normalize(&next_rtp);

    next_rtcp = next_rtp;
    next_rtcp.sec += 5;


    while (!strm->thread_quit_flag) {
	pj_fd_set_t set;
	pj_time_val now, lesser, timeout;
	int rc;

	/* Determine how long to sleep */
	if (PJ_TIME_VAL_LT(next_rtp, next_rtcp))
	    lesser = next_rtp;
	else
	    lesser = next_rtcp;

	pj_gettimeofday(&now);
	if (PJ_TIME_VAL_LTE(lesser, now))
	    timeout.sec = timeout.msec = 0;
	else {
	    timeout = lesser;
	    PJ_TIME_VAL_SUB(timeout, now);
	}

	PJ_FD_ZERO(&set);
	PJ_FD_SET(strm->rtp_sock, &set);
	PJ_FD_SET(strm->rtcp_sock, &set);

	rc = pj_sock_select(FD_SETSIZE, &set, NULL, NULL, &timeout);

	if (PJ_FD_ISSET(strm->rtp_sock, &set)) {

	    /*
	     * Process incoming RTP packet.
	     */
	    pj_status_t status;
	    pj_ssize_t size;
	    const pjmedia_rtp_hdr *hdr;
	    const void *payload;
	    unsigned payload_len;

	    size = sizeof(packet);
	    status = pj_sock_recv(strm->rtp_sock, packet, &size, 0);
	    if (status != PJ_SUCCESS) {
		app_perror(THIS_FILE, "RTP recv() error", status);
		continue;
	    }

	    /* Decode RTP packet. */
	    status = pjmedia_rtp_decode_rtp(&strm->in_sess, 
					    packet, size, 
					    &hdr, 
					    &payload, &payload_len);
	    if (status != PJ_SUCCESS) {
		app_perror(THIS_FILE, "RTP decode error", status);
		continue;
	    }

	    /* Update RTP session */
	    status = pjmedia_rtp_session_update(&strm->in_sess, hdr);
	    if (status != PJ_SUCCESS && 
		status != PJMEDIA_RTP_ESESSPROBATION && 
		status != PJMEDIA_RTP_ESESSRESTART) 
	    {
		app_perror(THIS_FILE, "RTP update error", status);
		PJ_LOG(3,(THIS_FILE,"RTP packet detail: pt=%d, seq=%d",
			  hdr->pt, pj_ntohs(hdr->seq)));
		continue;
	    }

	    /* Update the RTCP session. */
	    pjmedia_rtcp_rx_rtp(&strm->rtcp, pj_ntohs(hdr->seq),
				pj_ntohl(hdr->ts));

	} else if (PJ_FD_ISSET(strm->rtcp_sock, &set)) {

	    /*
	     * Process incoming RTCP
	     */
	    pj_status_t status;
	    pj_ssize_t size;

	    size = sizeof(packet);
	    status = pj_sock_recv( strm->rtcp_sock, packet, &size, 0);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error receiving RTCP packet", status);
	    else {
		if (size > sizeof(strm->rem_rtcp))
		    PJ_LOG(3,(THIS_FILE, "Error: RTCP packet too large"));
		else
		    pj_memcpy(&strm->rem_rtcp, packet, size);
	    }
	}


	pj_gettimeofday(&now);

	if (PJ_TIME_VAL_LTE(next_rtp, now)) {
	    /*
	     * Time to send RTP packet.
	     */
	    pj_status_t status;
	    const pjmedia_rtp_hdr *hdr;
	    pj_ssize_t size;
	    int hdrlen;

	    /* Format RTP header */
	    status = pjmedia_rtp_encode_rtp( &strm->out_sess, strm->si.tx_pt,
					     0, /* marker bit */
					     strm->bytes_per_frame, 
					     strm->samples_per_frame,
					     &hdr, &hdrlen);
	    if (status == PJ_SUCCESS) {

		/* Copy RTP header to packet */
		pj_memcpy(packet, hdr, hdrlen);

		/* Zero the payload */
		pj_memset(packet+hdrlen, 0, strm->bytes_per_frame);

		/* Send RTP packet */
		size = hdrlen + strm->bytes_per_frame;
		status = pj_sock_sendto( strm->rtp_sock, packet, &size, 0,
					 &strm->si.rem_addr, 
					 sizeof(strm->si.rem_addr));

		if (status != PJ_SUCCESS)
		    app_perror(THIS_FILE, "Error sending RTP packet", status);

	    }

	    /* Update RTCP SR */
	    pjmedia_rtcp_tx_rtp( &strm->rtcp, (pj_uint16_t)strm->bytes_per_frame);

	    /* Schedule next send */
	    next_rtp.msec += strm->samples_per_frame * 1000 / strm->clock_rate;
	    pj_time_val_normalize(&next_rtp);
	}


	if (PJ_TIME_VAL_LTE(next_rtcp, now)) {
	    /*
	     * Time to send RTCP packet.
	     */
	    pjmedia_rtcp_pkt *rtcp_pkt;
	    int rtcp_len;
	    pj_sockaddr_in rem_addr;
	    pj_ssize_t size;
	    int port;
	    pj_status_t status;

	    /* Build RTCP packet */
	    pjmedia_rtcp_build_rtcp(&strm->rtcp, &rtcp_pkt, &rtcp_len);

    
	    /* Calculate address based on RTP address */
	    rem_addr = strm->si.rem_addr;
	    port = pj_ntohs(strm->si.rem_addr.sin_port) + 1;
	    rem_addr.sin_port = pj_htons((pj_uint16_t)port);

	    /* Send packet */
	    size = rtcp_len;
	    status = pj_sock_sendto(strm->rtcp_sock, rtcp_pkt, &size, 0,
				    &rem_addr, sizeof(rem_addr));
	    if (status != PJ_SUCCESS) {
		app_perror(THIS_FILE, "Error sending RTCP packet", status);
	    }
	    

	    next_rtcp.sec += 5;
	}

    }

    return 0;
}


/* Callback to be called when SDP negotiation is done in the call: */
static void call_on_media_update( pjsip_inv_session *inv,
				  pj_status_t status)
{
    struct call *call;
    pj_pool_t *pool;
    struct media_stream *audio;
    pjmedia_sdp_session *local_sdp, *remote_sdp;


    call = inv->mod_data[mod_siprtp.id];
    pool = inv->dlg->pool;
    audio = &call->media[0];

    /* If this is a mid-call media update, then destroy existing media */
    if (audio->thread != NULL)
	destroy_call_media(call->index);


    /* Do nothing if media negotiation has failed */
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "SDP negotiation failed", status);
	return;
    }

    
    /* Capture stream definition from the SDP */
    pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);
    pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);

    status = pjmedia_stream_info_from_sdp(&audio->si, inv->pool, app.med_endpt,
					  local_sdp, remote_sdp, 0);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error creating stream info from SDP", status);
	return;
    }


    audio->clock_rate = audio->si.fmt.sample_rate;
    audio->samples_per_frame = audio->clock_rate * 20 / 1000;
    audio->bytes_per_frame = 160;
    PJ_TODO(TAKE_CODEC_INFO_FROM_ARGUMENT);


    pjmedia_rtp_session_init(&audio->out_sess, audio->si.tx_pt, 
			     (pj_uint32_t)audio);
    pjmedia_rtp_session_init(&audio->in_sess, audio->si.fmt.pt, 0);
    pjmedia_rtcp_init(&audio->rtcp, 0);

    /* Start media thread. */
    audio->thread_quit_flag = 0;
    status = pj_thread_create( inv->pool, "media", &media_thread, audio,
			       0, 0, &audio->thread);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error creating media thread", status);
    }
}



/* Destroy call's media */
static void destroy_call_media(unsigned call_index)
{
    struct media_stream *audio = &app.call[call_index].media[0];

    if (audio->thread) {
	audio->thread_quit_flag = 1;
	pj_thread_join(audio->thread);
	pj_thread_destroy(audio->thread);
	audio->thread = NULL;
	audio->thread_quit_flag = 0;
    }
}


/////////////////////////////////////////////////////////////////////////////
/*
 * USER INTERFACE STUFFS
 */

static const char *good_number(char *buf, pj_int32_t val)
{
    if (val < 1000) {
	pj_ansi_sprintf(buf, "%d", val);
    } else if (val < 1000000) {
	pj_ansi_sprintf(buf, "%d.%dK", 
			val / 1000,
			(val % 1000) / 100);
    } else {
	pj_ansi_sprintf(buf, "%d.%02dM", 
			val / 1000000,
			(val % 1000000) / 10000);
    }

    return buf;
}


static void print_call(int call_index)
{
    int len;
    pjsip_inv_session *inv = app.call[call_index].inv;
    pjsip_dialog *dlg = inv->dlg;
    struct media_stream *audio = &app.call[call_index].media[0];
    char userinfo[128];
    char packets[16];

    /* Dump invite sesion info. */

    len = pjsip_hdr_print_on(dlg->remote.info, userinfo, sizeof(userinfo));
    if (len < 1)
	pj_ansi_strcpy(userinfo, "<--uri too long-->");
    else
	userinfo[len] = '\0';
    
    printf("Call #%d: %s\n", call_index, pjsip_inv_state_name(inv->state));
    printf("   %s\n", userinfo);

    if (app.call[call_index].media[0].thread == NULL) {
	return;
    }

    printf("   Stream #0: audio %.*s@%dHz, %d bytes/sec\n",
	   (int)audio->si.fmt.encoding_name.slen,
	   audio->si.fmt.encoding_name.ptr,
	   audio->clock_rate,
	   audio->bytes_per_frame * audio->clock_rate / audio->samples_per_frame);
    printf("              RX pkt=%s, fraction lost=%5.2f%%, jitter=%dms\n",
	   good_number(packets, audio->rtcp.received),
	   audio->rtcp.rtcp_pkt.rr.fract_lost/255.0,
	   pj_ntohl(audio->rtcp.rtcp_pkt.rr.jitter) * 1000 / audio->clock_rate);
    printf("              TX pkt=%s, fraction lost=%5.2f%%, jitter=%dms\n",
	   good_number(packets, pj_ntohl(audio->rtcp.rtcp_pkt.sr.sender_pcount)),
	   audio->rem_rtcp.rr.fract_lost/255.0,
	   pj_ntohl(audio->rem_rtcp.rr.jitter) * 1000 / audio->clock_rate);
}


static void list_calls()
{
    unsigned i;
    puts("List all calls:");
    for (i=0; i<app.max_calls; ++i) {
	if (!app.call[i].inv)
	    continue;
	print_call(i);
    }
}

static void hangup_call(unsigned index)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    if (app.call[index].inv == NULL)
	return;

    status = pjsip_inv_end_session(app.call[index].inv, 603, NULL, &tdata);
    if (status==PJ_SUCCESS && tdata!=NULL)
	pjsip_inv_send_msg(app.call[index].inv, tdata);
}

static void hangup_all_calls()
{
    unsigned i;
    for (i=0; i<app.max_calls; ++i) {
	if (!app.call[i].inv)
	    continue;
	hangup_call(i);
    }
}

static pj_bool_t simple_input(const char *title, char *buf, pj_size_t len)
{
    char *p;

    printf("%s (empty to cancel): ", title); fflush(stdout);
    fgets(buf, len, stdin);

    /* Remove trailing newlines. */
    for (p=buf; ; ++p) {
	if (*p=='\r' || *p=='\n') *p='\0';
	else if (!*p) break;
    }

    if (!*buf)
	return PJ_FALSE;
    
    return PJ_TRUE;
}


static const char *MENU =
"\n"
"Enter menu character:\n"
"  l    List all calls\n"
"  h    Hangup a call\n"
"  H    Hangup all calls\n"
"  q    Quit\n"
"\n";


/* Main screen menu */
static void console_main()
{
    char input1[10];
    unsigned i;

    for (;;) {
	printf(">>> "); fflush(stdout);
	fgets(input1, sizeof(input1), stdin);

	switch (input1[0]) {
	case 'l':
	    list_calls();
	    break;

	case 'h':
	    if (!simple_input("Call number to hangup", input1, sizeof(input1)))
		break;

	    i = atoi(input1);
	    hangup_call(i);
	    break;

	case 'H':
	    hangup_all_calls();
	    break;

	case 'q':
	    goto on_exit;

	default:
	    printf("%s", MENU);
	    break;
	}

	fflush(stdout);
    }

on_exit:
    ;
}


/* Notification on incoming messages */
static pj_bool_t console_on_rx_msg(pjsip_rx_data *rdata)
{
    PJ_LOG(4,(THIS_FILE, "RX %d bytes %s from %s:%d:\n"
			 "%s\n"
			 "--end msg--",
			 rdata->msg_info.len,
			 pjsip_rx_data_get_info(rdata),
			 rdata->pkt_info.src_name,
			 rdata->pkt_info.src_port,
			 rdata->msg_info.msg_buf));
    
    /* Always return false, otherwise messages will not get processed! */
    return PJ_FALSE;
}

/* Notification on outgoing messages */
static pj_status_t console_on_tx_msg(pjsip_tx_data *tdata)
{
    
    /* Important note:
     *	tp_info field is only valid after outgoing messages has passed
     *	transport layer. So don't try to access tp_info when the module
     *	has lower priority than transport layer.
     */

    PJ_LOG(4,(THIS_FILE, "TX %d bytes %s to %s:%d:\n"
			 "%s\n"
			 "--end msg--",
			 (tdata->buf.cur - tdata->buf.start),
			 pjsip_tx_data_get_info(tdata),
			 tdata->tp_info.dst_name,
			 tdata->tp_info.dst_port,
			 tdata->buf.start));

    /* Always return success, otherwise message will not get sent! */
    return PJ_SUCCESS;
}

/* The module instance. */
static pjsip_module msg_logger = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-siprtp-log", 14 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &console_on_rx_msg,			/* on_rx_request()	*/
    &console_on_rx_msg,			/* on_rx_response()	*/
    &console_on_tx_msg,			/* on_tx_request.	*/
    &console_on_tx_msg,			/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};




/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_status_t status;

    status = pj_init();
    if (status != PJ_SUCCESS)
	return 1;

    status = init_options(argc, argv);
    if (status != PJ_SUCCESS)
	return 1;

    status = init_sip();
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Initialization has failed", status);
	destroy_sip();
	return 1;
    }

    pjsip_endpt_register_module(app.sip_endpt, &msg_logger);

    status = init_media();
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Media initialization failed", status);
	destroy_sip();
	return 1;
    }

    if (app.uri_to_call.slen) {
	unsigned i;

	for (i=0; i<app.max_calls; ++i) {
	    status = make_call(&app.uri_to_call);
	    if (status != PJ_SUCCESS) {
		app_perror(THIS_FILE, "Error making call", status);
		break;
	    }
	}
    }
   
    console_main();


    destroy_media();
    destroy_sip();

    return 0;
}


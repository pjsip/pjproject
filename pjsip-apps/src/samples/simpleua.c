
/* Include all PJSIP core headers. */
#include <pjsip.h>

/* Include all PJMEDIA headers. */
#include <pjmedia.h>

/* Include all PJMEDIA-CODEC headers. */
#include <pjmedia-codec.h>

/* Include all PJSIP-UA headers */
#include <pjsip_ua.h>

/* Include all PJSIP-SIMPLE headers */
#include <pjsip_simple.h>

/* Include all PJLIB-UTIL headers. */
#include <pjlib-util.h>

/* Include all PJLIB headers. */
#include <pjlib.h>


#define THIS_FILE   "simpleua.c"


/*
 * Static variables.
 */
static pj_bool_t g_complete;

/* Global endpoint instance. */
static pjsip_endpoint *g_endpt;

/* Global caching pool factory. */
static pj_caching_pool cp;

/* Global media endpoint. */
static pjmedia_endpt	    *g_med_endpt;
static pjmedia_sock_info     g_med_skinfo;

/* Call variables. */
static pjsip_inv_session    *g_inv;
static pjmedia_session	    *g_med_session;
static pjmedia_snd_port	    *g_snd_player;
static pjmedia_snd_port	    *g_snd_rec;


/*
 * Prototypes.
 */
static void call_on_media_update( pjsip_inv_session *inv,
				  pj_status_t status);
static void call_on_state_changed( pjsip_inv_session *inv, 
				   pjsip_event *e);
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);
static pj_bool_t on_rx_request( pjsip_rx_data *rdata );


/* Module to receive incoming requests (e.g. INVITE). */
static pjsip_module mod_simpleua =
{
    NULL, NULL,			    /* prev, next.		*/
    { "mod-simpleua", 12 },	    /* Name.			*/
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
 * Show error.
 */
static int app_perror( const char *sender, const char *title, 
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(1,(sender, "%s: %s [code=%d]", title, errmsg, status));
    return 1;
}


/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_status_t status;

    /* Init PJLIB */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Init PJLIB-UTIL: */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Init memory pool: */

    /* Init caching pool. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

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

	status = pjsip_endpt_create(&cp.factory, endpt_name, 
				    &g_endpt);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* 
     * Add UDP transport. 
     */
    {
	pj_sockaddr_in addr;

	addr.sin_family = PJ_AF_INET;
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons(5060);

	status = pjsip_udp_transport_start( g_endpt, &addr, NULL, 1, NULL);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to start UDP transport", status);
	    return 1;
	}
    }


    /* 
     * Init transaction layer. 
     */
    status = pjsip_tsx_layer_init_module(g_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* 
     * Initialize UA layer module: 
     */
    status = pjsip_ua_init_module( g_endpt, NULL );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /*
     * Init invite session module.
     */
    {
	pjsip_inv_callback inv_cb;

	/* Init the callback for INVITE session: */
	pj_memset(&inv_cb, 0, sizeof(inv_cb));
	inv_cb.on_state_changed = &call_on_state_changed;
	inv_cb.on_new_session = &call_on_forked;
	inv_cb.on_media_update = &call_on_media_update;

	/* Initialize invite session module:  */
	status = pjsip_inv_usage_init(g_endpt, &inv_cb);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }


    /*
     * Register module to receive incoming requests.
     */
    status = pjsip_endpt_register_module( g_endpt, &mod_simpleua);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Init media endpoint: 
     */
    status = pjmedia_endpt_create(&cp.factory, &g_med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* 
     * Add PCMA/PCMU codec to the media endpoint. 
     */
    status = pjmedia_codec_g711_init(g_med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* 
     * Initialize RTP socket info for the media.
     */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &g_med_skinfo.rtp_sock);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    
    pj_sockaddr_in_init( &g_med_skinfo.rtp_addr_name, 
			 pjsip_endpt_name(g_endpt), 4000);

    status = pj_sock_bind(g_med_skinfo.rtp_sock, &g_med_skinfo.rtp_addr_name,
			  sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, 
		    "Unable to bind RTP socket", 
		    status);
	return 1;
    }


    /* For simplicity, ignore RTCP socket. */
    g_med_skinfo.rtcp_sock = PJ_INVALID_SOCKET;
    g_med_skinfo.rtcp_addr_name = g_med_skinfo.rtp_addr_name;

    
    /*
     * If URL is specified, then make call immediately.
     */
    if (argc > 1) {
	char temp[80];
	pj_str_t dst_uri = pj_str(argv[1]);
	pj_str_t local_uri;
	pjsip_dialog *dlg;
	pjmedia_sdp_session *local_sdp;
	pjsip_tx_data *tdata;

	pj_ansi_sprintf(temp, "sip:simpleuac@%s", pjsip_endpt_name(g_endpt)->ptr);
	local_uri = pj_str(temp);

	/* Create UAC dialog */
	status = pjsip_dlg_create_uac( pjsip_ua_instance(), 
				       &local_uri,  /* local URI */
				       NULL,	    /* local Contact */
				       &dst_uri,    /* remote URI */
				       &dst_uri,    /* remote target */
				       &dlg);	    /* dialog */
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to create UAC dialog", status);
	    return 1;
	}

	/* Get media capability from media endpoint: */
	status = pjmedia_endpt_create_sdp( g_med_endpt, dlg->pool, 1, 
					   &g_med_skinfo, &local_sdp);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


	/* Create the INVITE session: */
	status = pjsip_inv_create_uac( dlg, local_sdp, 0, &g_inv);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


	/* Create initial INVITE request: */
	status = pjsip_inv_invite(g_inv, &tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	/* Send initial INVITE request: */
	status = pjsip_inv_send_msg(g_inv, tdata);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    } else {
	PJ_LOG(3,(THIS_FILE, "Ready to accept incoming calls..."));
    }


    /* Loop until one call is completed */
    for (;!g_complete;) {
	pj_time_val timeout = {0, 10};
	pjsip_endpt_handle_events(g_endpt, &timeout);
    }

    return 0;
}



/*
 * Callback when INVITE session state has changed.
 */
static void call_on_state_changed( pjsip_inv_session *inv, 
				   pjsip_event *e)
{
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

	PJ_LOG(3,(THIS_FILE, "Call DISCONNECTED [reason=%d (%s)]", 
		  inv->cause,
		  pjsip_get_status_text(inv->cause)->ptr));

	PJ_LOG(3,(THIS_FILE, "One call completed, application quitting..."));
	g_complete = 1;

    } else {

	PJ_LOG(3,(THIS_FILE, "Call state changed to %s", 
		  pjsip_inv_state_name(inv->state)));

    }
}

/*
 * Callback when media negotiation has completed.
 */
static void call_on_media_update( pjsip_inv_session *inv,
				  pj_status_t status)
{
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;
    pjmedia_port *media_port;

    if (status != PJ_SUCCESS) {

	app_perror(THIS_FILE, "SDP negotiation has failed", status);

	/* Here we should disconnect call if we're not in the middle 
	 * of initializing an UAS dialog and if this is not a re-INVITE.
	 */
	return;
    }

    /* Get local and remote SDP */

    status = pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);

    status = pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);

    /* Create new media session. 
     * The media session is active immediately.
     */
    status = pjmedia_session_create( g_med_endpt, 1, 
				     &g_med_skinfo,
				     local_sdp, remote_sdp, 
				     NULL, &g_med_session );
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to create media session", status);
	return;
    }

    /* Get the port interface of the first stream in the session. */
    pjmedia_session_get_port(g_med_session, 0, &media_port);

    /* Create a sound Player device and connect the media port to the
     * sound device.
     */
    status = pjmedia_snd_port_create_player( 
		    inv->pool,				/* pool		    */
		    -1,					/* sound dev id	    */
		    media_port->info.sample_rate,	/* clock rate	    */
		    media_port->info.channel_count,	/* channel count    */
		    media_port->info.samples_per_frame, /* samples per frame*/
		    media_port->info.bits_per_sample,   /* bits per sample  */
		    0,					/* options	    */
		    &g_snd_player);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to create sound player", status);
	PJ_LOG(3,(THIS_FILE, "%d %d %d %d",
	    	    media_port->info.sample_rate,	/* clock rate	    */
		    media_port->info.channel_count,	/* channel count    */
		    media_port->info.samples_per_frame, /* samples per frame*/
		    media_port->info.bits_per_sample    /* bits per sample  */
	    ));
	return;
    }

    status = pjmedia_snd_port_connect(g_snd_player, media_port);


    /* Create a sound recorder device and connect the media port to the
     * sound device.
     */
    status = pjmedia_snd_port_create_rec( 
		    inv->pool,				/* pool		    */
		    -1,					/* sound dev id	    */
		    media_port->info.sample_rate,	/* clock rate	    */
		    media_port->info.channel_count,	/* channel count    */
		    media_port->info.samples_per_frame, /* samples per frame*/
		    media_port->info.bits_per_sample,   /* bits per sample  */
		    0,					/* options	    */
		    &g_snd_rec);
    if (status != PJ_SUCCESS) {
	app_perror( THIS_FILE, "Unable to create sound recorder", status);
	return;
    }

    status = pjmedia_snd_port_connect(g_snd_rec, media_port);

    /* Done with media. */
}


/*
 * This callback is called when dialog has forked
 */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
}

/*
 * Callback when incoming requests outside any transactions and any
 * dialogs are received
 */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata )
{
    pjsip_dialog *dlg;
    pjmedia_sdp_session *local_sdp;
    pjsip_tx_data *tdata;
    unsigned options = 0;
    pj_status_t status;

    /* 
     * Respond (statelessly) any non-INVITE requests with 500 
     */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {

	pj_str_t reason = pj_str("Simple UA unable to handle this request");

	pjsip_endpt_respond_stateless( g_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;
    }

    /*
     * Reject INVITE if we already have an INVITE session in progress.
     */
    if (g_inv) {

	pj_str_t reason = pj_str("Another call is in progress");

	pjsip_endpt_respond_stateless( g_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;

    }

    /* Verify that we can handle the request. */
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
				      g_endpt, NULL);
    if (status != PJ_SUCCESS) {

	pj_str_t reason = pj_str("Sorry Simple UA can not handle this INVITE");

	pjsip_endpt_respond_stateless( g_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;
    } 

    /*
     * Create UAS dialog.
     */
    status = pjsip_dlg_create_uas( pjsip_ua_instance(), 
				   rdata,
				   NULL, /* contact */
				   &dlg);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_respond_stateless(g_endpt, rdata, 500, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    /* 
     * Get media capability from media endpoint: 
     */

    status = pjmedia_endpt_create_sdp( g_med_endpt, rdata->tp_info.pool, 1,
				       &g_med_skinfo, 
				       &local_sdp);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    /* 
     * Create invite session: 
     */
    status = pjsip_inv_create_uas( dlg, rdata, local_sdp, 0, &g_inv);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    /*
     * Initially send 180 response.
     */
    status = pjsip_inv_initial_answer(g_inv, rdata, 
				      180, 
				      NULL, NULL, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    /*
     * Send the 180 response.
     */  
    status = pjsip_inv_send_msg(g_inv, tdata); 
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    /*
     * Now send 200 response.
     */
    status = pjsip_inv_answer( g_inv, 
			       200, NULL,	/* st_code and st_text */
			       NULL,		/* SDP already specified */
			       &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    /*
     * Send the 200 response.
     */
    status = pjsip_inv_send_msg(g_inv, tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    return PJ_TRUE;
}

 

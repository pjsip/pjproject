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


#define THIS_FILE   "pjsua_core.c"


/* PJSUA application instance. */
struct pjsua_data pjsua_var;


/* Display error */
PJ_DEF(void) pjsua_perror( const char *sender, const char *title, 
			   pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(3,(sender, "%s: %s [status=%d]", title, errmsg, status));
}


static void init_data()
{
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.acc); ++i)
	pjsua_var.acc[i].index = i;
    
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var.tpdata); ++i)
	pjsua_var.tpdata[i].index = i;
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
    PJ_LOG(4,(THIS_FILE, "RX %d bytes %s from %s %s:%d:\n"
			 "%.*s\n"
			 "--end msg--",
			 rdata->msg_info.len,
			 pjsip_rx_data_get_info(rdata),
			 rdata->tp_info.transport->type_name,
			 rdata->pkt_info.src_name,
			 rdata->pkt_info.src_port,
			 (int)rdata->msg_info.len,
			 rdata->msg_info.msg_buf));
    
    /* Always return false, otherwise messages will not get processed! */
    return PJ_FALSE;
}

/* Notification on outgoing messages */
static pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
{
    
    /* Important note:
     *	tp_info field is only valid after outgoing messages has passed
     *	transport layer. So don't try to access tp_info when the module
     *	has lower priority than transport layer.
     */

    PJ_LOG(4,(THIS_FILE, "TX %d bytes %s to %s %s:%d:\n"
			 "%.*s\n"
			 "--end msg--",
			 (tdata->buf.cur - tdata->buf.start),
			 pjsip_tx_data_get_info(tdata),
			 tdata->tp_info.transport->type_name,
			 tdata->tp_info.dst_name,
			 tdata->tp_info.dst_port,
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
    pjmedia_sdp_session *sdp;
    const pjsip_hdr *cap_hdr;
    pj_status_t status;

    /* Only want to handle OPTIONS requests */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
			 &pjsip_options_method) != 0)
    {
	return PJ_FALSE;
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
	pjsip_msg_add_hdr(tdata->msg, pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Accept header */
    cap_hdr = pjsip_endpt_get_capability(pjsua_var.endpt, PJSIP_H_ACCEPT, NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Supported header */
    cap_hdr = pjsip_endpt_get_capability(pjsua_var.endpt, PJSIP_H_SUPPORTED, NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Allow-Events header from the evsub module */
    cap_hdr = pjsip_evsub_get_allow_events_hdr(NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, pjsip_hdr_clone(tdata->pool, cap_hdr));
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

    /* Add SDP body, using call0's RTP address */
    status = pjmedia_endpt_create_sdp(pjsua_var.med_endpt, tdata->pool, 1,
				      &pjsua_var.calls[0].skinfo, &sdp);
    if (status == PJ_SUCCESS) {
	pjsip_create_sdp_body(tdata->pool, sdp, &tdata->msg->body);
    }

    /* Send response statelessly */
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

    /* Close existing file, if any */
    if (pjsua_var.log_file) {
	pj_file_close(pjsua_var.log_file);
	pjsua_var.log_file = NULL;
    }

    /* If output log file is desired, create the file: */
    if (pjsua_var.log_cfg.log_filename.slen) {

	status = pj_file_open(pjsua_var.pool, 
			      pjsua_var.log_cfg.log_filename.ptr,
			      PJ_O_WRONLY, 
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


    /* Init PJLIB-UTIL: */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Set default sound device ID */
    pjsua_var.cap_dev = pjsua_var.play_dev = -1;

    /* Init caching pool. */
    pj_caching_pool_init(&pjsua_var.cp, &pj_pool_factory_default_policy, 0);

    /* Create memory pool for application. */
    pjsua_var.pool = pjsua_pool_create("pjsua", 4000, 4000);
    
    PJ_ASSERT_RETURN(pjsua_var.pool, PJ_ENOMEM);

    /* Create mutex */
    status = pj_mutex_create_recursive(pjsua_var.pool, "pjsua", 
				       &pjsua_var.mutex);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create mutex", status);
	return status;
    }

    /* Must create SIP endpoint to initialize SIP parser. The parser
     * is needed for example when application needs to call pjsua_verify_url().
     */
    status = pjsip_endpt_create(&pjsua_var.cp.factory, 
				pj_gethostname()->ptr, 
				&pjsua_var.endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


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
    pj_status_t status;


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
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    }

    /* If nameserver is configured, create DNS resolver instance and
     * set it to be used by SIP resolver.
     */
    if (ua_cfg->nameserver_count) {
#if PJSIP_HAS_RESOLVER
	pj_dns_resolver *resv;
	unsigned i;

	/* Create DNS resolver */
	status = pjsip_endpt_create_resolver(pjsua_var.endpt, &resv);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

	/* Configure nameserver for the DNS resolver */
	status = pj_dns_resolver_set_ns(resv, ua_cfg->nameserver_count,
					ua_cfg->nameserver, NULL);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error setting nameserver", status);
	    return status;
	}

	/* Set this DNS resolver to be used by the SIP resolver */
	status = pjsip_endpt_set_resolver(pjsua_var.endpt, resv);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error setting DNS resolver", status);
	    return status;
	}

	/* Print nameservers */
	for (i=0; i<ua_cfg->nameserver_count; ++i) {
	    PJ_LOG(4,(THIS_FILE, "Nameserver %.*s added",
		      (int)ua_cfg->nameserver[i].slen,
		      ua_cfg->nameserver[i].ptr));
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
    status = pjsip_ua_init_module( pjsua_var.endpt, NULL );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Initialize Replaces support. */
    status = pjsip_replaces_init_module( pjsua_var.endpt );
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

    

    /* Initialize PJSUA call subsystem: */
    status = pjsua_call_subsys_init(ua_cfg);
    if (status != PJ_SUCCESS)
	goto on_error;


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
	unsigned i;

	if (pjsua_var.ua_cfg.thread_cnt > PJ_ARRAY_SIZE(pjsua_var.thread))
	    pjsua_var.ua_cfg.thread_cnt = PJ_ARRAY_SIZE(pjsua_var.thread);

	for (i=0; i<pjsua_var.ua_cfg.thread_cnt; ++i) {
	    status = pj_thread_create(pjsua_var.pool, "pjsua", &worker_thread,
				      NULL, 0, 0, &pjsua_var.thread[i]);
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
			 PJ_VERSION, PJ_OS_NAME));

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
	while (pjsua_handle_events(10) > 0)
	    ;
	pj_gettimeofday(&now);
    } while (PJ_TIME_VAL_LT(now, timeout));
}

/*
 * Destroy pjsua.
 */
PJ_DEF(pj_status_t) pjsua_destroy(void)
{
    int i;  /* Must be signed */

    /* Signal threads to quit: */
    pjsua_var.thread_quit_flag = 1;

    /* Wait worker threads to quit: */
    for (i=0; i<(int)pjsua_var.ua_cfg.thread_cnt; ++i) {
	if (pjsua_var.thread[i]) {
	    pj_thread_join(pjsua_var.thread[i]);
	    pj_thread_destroy(pjsua_var.thread[i]);
	    pjsua_var.thread[i] = NULL;
	}
    }
    
    if (pjsua_var.endpt) {
	/* Terminate all calls. */
	pjsua_call_hangup_all();

	/* Terminate all presence subscriptions. */
	pjsua_pres_shutdown();

	/* Unregister, if required: */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var.acc); ++i) {
	    if (!pjsua_var.acc[i].valid)
		continue;

	    if (pjsua_var.acc[i].regc) {
		pjsua_acc_set_registration(i, PJ_FALSE);
	    }
	}

	/* Wait for some time to allow unregistration to complete: */
	PJ_LOG(4,(THIS_FILE, "Shutting down..."));
	busy_sleep(1000);
    }

    /* Destroy media */
    pjsua_media_subsys_destroy();

    /* Destroy endpoint. */
    if (pjsua_var.endpt) {
	pjsip_endpt_destroy(pjsua_var.endpt);
	pjsua_var.endpt = NULL;
    }

    /* Destroy mutex */
    if (pjsua_var.mutex) {
	pj_mutex_destroy(pjsua_var.mutex);
	pjsua_var.mutex = NULL;
    }

    /* Destroy pool and pool factory. */
    if (pjsua_var.pool) {
	pj_pool_release(pjsua_var.pool);
	pjsua_var.pool = NULL;
	pj_caching_pool_destroy(&pjsua_var.cp);
    }


    PJ_LOG(4,(THIS_FILE, "PJSUA destroyed..."));

    /* End logging */
    if (pjsua_var.log_file) {
	pj_file_close(pjsua_var.log_file);
	pjsua_var.log_file = NULL;
    }

    /* Shutdown PJLIB */
    pj_shutdown();

    /* Clear pjsua_var */
    pj_bzero(&pjsua_var, sizeof(pjsua_var));

    /* Done. */
    return PJ_SUCCESS;
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

    status = pjsua_call_subsys_start();
    if (status != PJ_SUCCESS)
	return status;

    status = pjsua_media_subsys_start();
    if (status != PJ_SUCCESS)
	return status;

    status = pjsua_pres_start();
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}


/**
 * Poll pjsua for events, and if necessary block the caller thread for
 * the specified maximum interval (in miliseconds).
 */
PJ_DEF(int) pjsua_handle_events(unsigned msec_timeout)
{
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
 * Create and initialize SIP socket (and possibly resolve public
 * address via STUN, depending on config).
 */
static pj_status_t create_sip_udp_sock(pj_in_addr bound_addr,
				       int port,
				       pj_bool_t use_stun,
				       const pjsua_stun_config *stun_param,
				       pj_sock_t *p_sock,
				       pj_sockaddr_in *p_pub_addr)
{
    pjsua_stun_config stun;
    pj_sock_t sock;
    pj_status_t status;

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "socket() error", status);
	return status;
    }

    status = pj_sock_bind_in(sock, bound_addr.s_addr, (pj_uint16_t)port);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "bind() error", status);
	pj_sock_close(sock);
	return status;
    }

    /* Copy and normalize STUN param */
    if (use_stun) {
	pj_memcpy(&stun, stun_param, sizeof(*stun_param));
	pjsua_normalize_stun_config(&stun);
    } else {
	pj_bzero(&stun, sizeof(pjsua_stun_config));
    }

    /* Get the published address, either by STUN or by resolving
     * the name of local host.
     */
    if (stun.stun_srv1.slen) {
	/*
	 * STUN is specified, resolve the address with STUN.
	 */
	status = pj_stun_get_mapped_addr(&pjsua_var.cp.factory, 1, &sock,
				         &stun.stun_srv1, 
					  stun.stun_port1,
					 &stun.stun_srv2, 
					  stun.stun_port2,
				          p_pub_addr);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error resolving with STUN", status);
	    pj_sock_close(sock);
	    return status;
	}

    } else if (p_pub_addr->sin_addr.s_addr != 0) {
	/*
	 * Public address is already specified, no need to resolve the 
	 * address, only set the port.
	 */
	/* Do nothing */

    } else {

	pj_bzero(p_pub_addr, sizeof(pj_sockaddr_in));

	status = pj_gethostip(&p_pub_addr->sin_addr);
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock);
	    return status;
	}

	p_pub_addr->sin_family = PJ_AF_INET;
	p_pub_addr->sin_port = pj_htons((pj_uint16_t)port);
    }

    *p_sock = sock;

    PJ_LOG(4,(THIS_FILE, "SIP UDP socket reachable at %s:%d",
	      pj_inet_ntoa(p_pub_addr->sin_addr),
	      (int)pj_ntohs(p_pub_addr->sin_port)));

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
    if (type == PJSIP_TRANSPORT_UDP) {
	/*
	 * Create UDP transport.
	 */
	pjsua_transport_config config;
	pj_sock_t sock = PJ_INVALID_SOCKET;
	pj_sockaddr_in bound_addr;
	pj_sockaddr_in pub_addr;
	pjsip_host_port addr_name;

	/* Supply default config if it's not specified */
	if (cfg == NULL) {
	    pjsua_transport_config_default(&config);
	    cfg = &config;
	}

	/* Initialize bound address, if any */
	bound_addr.sin_addr.s_addr = PJ_INADDR_ANY;
	if (cfg->bound_addr.slen) {
	    status = pj_sockaddr_in_set_str_addr(&bound_addr,&cfg->bound_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, 
			     "Unable to resolve transport bound address", 
			     status);
		goto on_return;
	    }
	}

	/* Initialize the public address from the config, if any */
	pj_sockaddr_in_init(&pub_addr, NULL, (pj_uint16_t)cfg->port);
	if (cfg->public_addr.slen) {
	    status = pj_sockaddr_in_set_str_addr(&pub_addr, &cfg->public_addr);
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
	status = create_sip_udp_sock(bound_addr.sin_addr, cfg->port, 
				     cfg->use_stun, &cfg->stun_config, 
				     &sock, &pub_addr);
	if (status != PJ_SUCCESS)
	    goto on_return;

	addr_name.host = pj_str(pj_inet_ntoa(pub_addr.sin_addr));
	addr_name.port = pj_ntohs(pub_addr.sin_port);

	/* Create UDP transport */
	status = pjsip_udp_transport_attach( pjsua_var.endpt, sock,
					     &addr_name, 1, 
					     &tp);
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

    } else if (type == PJSIP_TRANSPORT_TCP) {
	/*
	 * Create TCP transport.
	 */
	pjsua_transport_config config;
	pjsip_host_port a_name;
	pjsip_tpfactory *tcp;
	pj_sockaddr_in local_addr;

	/* Supply default config if it's not specified */
	if (cfg == NULL) {
	    pjsua_transport_config_default(&config);
	    cfg = &config;
	}

	/* Init local address */
	pj_sockaddr_in_init(&local_addr, 0, 0);

	if (cfg->port)
	    local_addr.sin_port = pj_htons((pj_uint16_t)cfg->port);

	if (cfg->bound_addr.slen) {
	    status = pj_sockaddr_in_set_str_addr(&local_addr,&cfg->bound_addr);
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

	/* Create the TCP transport */
	status = pjsip_tcp_transport_start2(pjsua_var.endpt, &local_addr, 
					    &a_name, 1, &tcp);

	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating SIP TCP listener", 
			 status);
	    goto on_return;
	}

	/* Save the transport */
	pjsua_var.tpdata[id].type = type;
	pjsua_var.tpdata[id].local_name = tcp->addr_name;
	pjsua_var.tpdata[id].data.factory = tcp;

    } else {
	status = PJSIP_EUNSUPTRANSPORT;
	pjsua_perror(THIS_FILE, "Error creating transport", status);
	goto on_return;
    }


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
    pjsua_var.tpdata[id].type = tp->key.type;
    pjsua_var.tpdata[id].local_name = tp->local_name;
    pjsua_var.tpdata[id].data.tp = tp;

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
    struct transport_data *t = &pjsua_var.tpdata[id];
    pj_status_t status;

    pj_bzero(info, sizeof(*info));

    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<PJ_ARRAY_SIZE(pjsua_var.tpdata), PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var.tpdata[id].data.ptr != NULL, PJ_EINVAL);

    PJSUA_LOCK();

    if (pjsua_var.tpdata[id].type == PJSIP_TRANSPORT_UDP) {

	pjsip_transport *tp = t->data.tp;

	if (tp == NULL) {
	    PJSUA_UNLOCK();
	    return PJ_EINVALIDOP;
	}
    
	info->id = id;
	info->type = tp->key.type;
	info->type_name = pj_str(tp->type_name);
	info->info = pj_str(tp->info);
	info->flag = tp->flag;
	info->addr_len = tp->addr_len;
	info->local_addr = tp->local_addr;
	info->local_name = tp->local_name;
	info->usage_count = pj_atomic_get(tp->ref_cnt);

	status = PJ_SUCCESS;

    } else if (pjsua_var.tpdata[id].type == PJSIP_TRANSPORT_TCP) {

	pjsip_tpfactory *factory = t->data.factory;

	if (factory == NULL) {
	    PJSUA_UNLOCK();
	    return PJ_EINVALIDOP;
	}
    
	info->id = id;
	info->type = t->type;
	info->type_name = pj_str("TCP");
	info->info = pj_str("TCP transport");
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
    PJ_ASSERT_RETURN(id>=0 && id<PJ_ARRAY_SIZE(pjsua_var.tpdata), PJ_EINVAL);

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
    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<PJ_ARRAY_SIZE(pjsua_var.tpdata), PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var.tpdata[id].data.ptr != NULL, PJ_EINVAL);

    /* Note: destroy() may not work if there are objects still referencing
     *	     the transport.
     */
    if (force) {
	switch (pjsua_var.tpdata[id].type) {
	case PJSIP_TRANSPORT_UDP:
	    return pjsip_transport_destroy(pjsua_var.tpdata[id].data.tp);
	case PJSIP_TRANSPORT_TCP:
	    break;
	default:
	    break;
	}
	
    } else {
	switch (pjsua_var.tpdata[id].type) {
	case PJSIP_TRANSPORT_UDP:
	    return pjsip_transport_shutdown(pjsua_var.tpdata[id].data.tp);
	case PJSIP_TRANSPORT_TCP:
	    return (*pjsua_var.tpdata[id].data.factory->destroy)
			(pjsua_var.tpdata[id].data.factory);
	default:
	    break;
	}
    }

    /* Unreachable */
    pj_assert(!"Unknown transport");
    return PJ_EINVALIDOP;
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

	new_hdr = pjsip_hdr_clone(tdata->pool, hdr);
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

	new_r = pjsip_hdr_clone(tdata->pool, r);
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
 * Verify that valid SIP url is given.
 */
PJ_DEF(pj_status_t) pjsua_verify_sip_url(const char *c_url)
{
    pjsip_uri *p;
    pj_pool_t *pool;
    char *url;
    int len = (c_url ? pj_ansi_strlen(c_url) : 0);

    if (!len) return -1;

    pool = pj_pool_create(&pjsua_var.cp.factory, "check%p", 1024, 0, NULL);
    if (!pool) return -1;

    url = pj_pool_alloc(pool, len+1);
    pj_ansi_strcpy(url, c_url);

    p = pjsip_parse_uri(pool, url, len, 0);
    if (!p || pj_stricmp2(pjsip_uri_get_scheme(p), "sip") != 0)
	p = NULL;

    pj_pool_release(pool);
    return p ? 0 : -1;
}

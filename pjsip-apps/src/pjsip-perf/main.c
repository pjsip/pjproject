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
#include "pjsip_perf.h"
#include <stdlib.h>		/* atoi */

#define THIS_FILE   "main.c"

pjsip_perf_settings settings;

/* Show error message. */
void app_perror(const char *sender, const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(3,(sender, "%s: %s [code=%d]", title, errmsg, status));
}


/* Init default settings. */
static void init_settings(void)
{
    pj_status_t status;

    settings.stateless = 0;
    settings.start_rate = 10;
    settings.max_capacity = 64;
    settings.duration = 0;
    settings.thread_cnt = 1;
    settings.local_port = 5060;
    settings.log_level = 3;
    settings.app_log_level = 3;

    pjsip_method_set(&settings.method, PJSIP_OPTIONS_METHOD);

    pj_init();

    /* Create caching pool. */
    pj_caching_pool_init(&settings.cp, &pj_pool_factory_default_policy, 
			 4 * 1024 * 1024);

    /* Create application pool. */
    settings.pool = pj_pool_create(&settings.cp.factory, "pjsip-perf", 1024,
				   1024, NULL);

    /* Create endpoint. */
    status = pjsip_endpt_create(&settings.cp.factory, NULL, &settings.endpt);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create endpoint", status);
	return;
    }

}

/* Poll function. */
static int PJ_THREAD_FUNC poll_pjsip(void *arg)
{
    pj_status_t last_err = 0;

    PJ_UNUSED_ARG(arg);

    do {
	pj_time_val timeout = { 0, 10 };
	pj_status_t status;
	
	status = pjsip_endpt_handle_events (settings.endpt, &timeout);
	if (status != last_err) {
	    last_err = status;
	    app_perror(THIS_FILE, "handle_events() returned error", status);
	}
    } while (!settings.quit_flag);

    return 0;
}

/*****************************************************************************
 * This is a simple module to count and log messages 
 */
static void on_rx_msg(pjsip_rx_data *rdata)
{
    PJ_LOG(5,(THIS_FILE, "RX %d bytes %s from %s:%d:\n"
			 "%s\n"
			 "--end msg--",
			 rdata->msg_info.len,
			 pjsip_rx_data_get_info(rdata),
			 rdata->pkt_info.src_name,
			 rdata->pkt_info.src_port,
			 rdata->msg_info.msg_buf));
}

static void on_tx_msg(pjsip_tx_data *tdata)
{
    PJ_LOG(5,(THIS_FILE, "TX %d bytes %s to %s:%d:\n"
			 "%s\n"
			 "--end msg--",
			 (tdata->buf.cur - tdata->buf.start),
			 pjsip_tx_data_get_info(tdata),
			 tdata->tp_info.dst_name,
			 tdata->tp_info.dst_port,
			 tdata->buf.start));
}

static pj_bool_t mod_counter_on_rx_request(pjsip_rx_data *rdata)
{
    settings.rx_req++;
    on_rx_msg(rdata);
    return PJ_FALSE;
}

static pj_bool_t mod_counter_on_rx_response(pjsip_rx_data *rdata)
{
    settings.rx_res++;
    on_rx_msg(rdata);
    return PJ_FALSE;
}

static pj_status_t mod_counter_on_tx_request(pjsip_tx_data *tdata)
{
    settings.tx_req++;
    on_tx_msg(tdata);
    return PJ_SUCCESS;
}

static pj_status_t mod_counter_on_tx_response(pjsip_tx_data *tdata)
{
    settings.tx_res++;
    on_tx_msg(tdata);
    return PJ_SUCCESS;
}

static pjsip_module mod_counter = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-counter", 11 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &mod_counter_on_rx_request,		/* on_rx_request()	*/
    &mod_counter_on_rx_response,	/* on_rx_response()	*/
    &mod_counter_on_tx_request,		/* on_tx_request.	*/
    &mod_counter_on_tx_response,	/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};


/*****************************************************************************
 * Console application custom logging:
 */


static FILE *log_file;
static void app_log_writer(int level, const char *buffer, int len)
{
    /* Write to both stdout and file. */
    if (level <= settings.app_log_level)
	pj_log_write(level, buffer, len);

    if (log_file) {
	fwrite(buffer, len, 1, log_file);
	fflush(log_file);
    }
}


static pj_status_t app_logging_init(void)
{
    /* Redirect log function to ours */

    pj_log_set_log_func( &app_log_writer );

    /* If output log file is desired, create the file: */

    if (settings.log_file) {
	log_file = fopen(settings.log_file, "wt");
	if (log_file == NULL) {
	    PJ_LOG(1,(THIS_FILE, "Unable to open log file %s", 
		      settings.log_file));   
	    return -1;
	}
    }

    return PJ_SUCCESS;
}


void app_logging_shutdown(void)
{
    /* Close logging file, if any: */
    if (log_file) {
	fclose(log_file);
	log_file = NULL;
    }
}


/* Initialize */
static pj_status_t initialize(void)
{
    pj_sockaddr_in addr;
    int i;
    pj_status_t status;

    /* Init logging */
    if (app_logging_init() != PJ_SUCCESS)
	return -1;

    /* Create UDP transport. */
    pj_memset(&addr, 0, sizeof(addr));
    addr.sin_family = PJ_AF_INET;
    addr.sin_port = pj_htons((pj_uint16_t)settings.local_port);
    status = pjsip_udp_transport_start(settings.endpt, &addr, NULL, 
				       settings.thread_cnt, NULL);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to start UDP transport", status);
	return status;
    }


    /* Initialize transaction layer: */
    status = pjsip_tsx_layer_init_module(settings.endpt);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Transaction layer initialization error", 
		   status);
	return status;
    }

    /* Initialize UA layer module: */
    pjsip_ua_init_module( settings.endpt, NULL );

    /* Init core SIMPLE module : */
    pjsip_evsub_init_module(settings.endpt);

    /* Init presence module: */
    pjsip_pres_init_module( settings.endpt, pjsip_evsub_instance());

    /* Init xfer/REFER module */
    pjsip_xfer_init_module( settings.endpt );

    /* Init multimedia endpoint. */
    status = pjmedia_endpt_create(&settings.cp.factory, 
				  pjsip_endpt_get_ioqueue(settings.endpt), 0,
				  &settings.med_endpt);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create media endpoint", 
		   status);
	return status;
    }

    /* Init OPTIONS test handler */
    status = options_handler_init();
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create OPTIONS handler", status);
	return status;
    }

    /* Init call test handler */
    status = call_handler_init();
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to initialize call handler", status);
	return status;
    }


    /* Register message counter module. */
    status = pjsip_endpt_register_module(settings.endpt, &mod_counter);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to register module", status);
	return status;
    }

    /* Start worker thread. */
    for (i=0; i<settings.thread_cnt; ++i) {
	status = pj_thread_create(settings.pool, "pjsip-perf", &poll_pjsip,
				  NULL, 0, 0, &settings.thread[i]);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to create thread", status);
	    return status;
	}
    }

    pj_log_set_level(settings.log_level);
    return PJ_SUCCESS;
}


/* Shutdown */
static void shutdown(void)
{
    int i;

    /* Signal and wait worker thread to quit. */
    settings.quit_flag = 1;

    for (i=0; i<settings.thread_cnt; ++i) {
	pj_thread_join(settings.thread[i]);
	pj_thread_destroy(settings.thread[i]);
    }

    pjsip_endpt_destroy(settings.endpt);
    pj_caching_pool_destroy(&settings.cp);
}


/* Verify that valid SIP url is given. */
pj_status_t verify_sip_url(const char *c_url)
{
    pjsip_uri *p;
    pj_pool_t *pool;
    char *url;
    int len = (c_url ? pj_ansi_strlen(c_url) : 0);

    if (!len) return -1;

    pool = pj_pool_create(&settings.cp.factory, "check%p", 1024, 0, NULL);
    if (!pool) return -1;

    url = pj_pool_alloc(pool, len+1);
    pj_ansi_strcpy(url, c_url);

    p = pjsip_parse_uri(pool, url, len, 0);
    if (!p || pj_stricmp2(pjsip_uri_get_scheme(p), "sip") != 0)
	p = NULL;

    pj_pool_release(pool);
    return p ? 0 : -1;
}



/* Usage */
static void usage(void)
{
    puts("Usage:");
    puts("  pjsip-perf [options] [target]");
    puts("where");
    puts("  target		Optional default target URL");
    puts("");
    puts("General options:");
    puts("  --help              Display this help screen");
    puts("  --version           Display version info");
    puts("");
    puts("Logging options:");
    puts("  --log-level=N       Set log verbosity (default=3)");
    puts("  --app-log-level=N   Set screen log verbosity (default=3)");
    puts("  --log-file=FILE     Save log to FILE");
    puts("");
    puts("SIP options:");
    puts("  --local-port=N      SIP local port");
    puts("  --stateless		Handle incoming request statelessly if possible");
    puts("  --thread-cnt=N	Number of worker threads (default=1)");
    puts("");
    puts("Rate control:");
    puts("  --start-rate=N      Start rate in tasks per seconds (default=1)");
    puts("");
    puts("Capacity control:");
    puts("  --max-capacity=N    Maximum outstanding sessions (default=64)");
    puts("");
    puts("Duration control:");
    puts("  --duration=secs     Sessions duration (default=0)");
    puts("");
}


/* Read options. */
static pj_status_t parse_options(int argc, char *argv[])
{
    enum {
	OPT_HELP,
	OPT_VERSION,
	OPT_LOG_LEVEL,
	OPT_APP_LOG_LEVEL,
	OPT_LOG_FILE,
	OPT_LOCAL_PORT,
	OPT_STATELESS,
	OPT_THREAD_CNT,
	OPT_START_RATE,
	OPT_MAX_CAPACITY,
	OPT_DURATION
    };
    struct pj_getopt_option long_opts[] = {
	{ "help",	    0, 0, OPT_HELP},
	{ "version",	    0, 0, OPT_VERSION},
	{ "log-level",	    1, 0, OPT_LOG_LEVEL},
	{ "app-log-level",  1, 0, OPT_APP_LOG_LEVEL},
	{ "log-file",	    1, 0, OPT_LOG_FILE},
	{ "local-port",	    1, 0, OPT_LOCAL_PORT},
	{ "stateless",	    0, 0, OPT_STATELESS},
	{ "thread-cnt",	    1, 0, OPT_THREAD_CNT},
	{ "start-rate",	    1, 0, OPT_START_RATE},
	{ "max-capacity",   1, 0, OPT_MAX_CAPACITY},
	{ "duration",	    1, 0, OPT_DURATION},
	{ NULL, 0, 0, 0}
    };
    int c, option_index;

    pj_optind = 0;
    while ((c=pj_getopt_long(argc, argv, "", long_opts, &option_index)) != -1) {
	switch (c) {

	case OPT_HELP:
	    usage();
	    return PJ_EINVAL;

	case OPT_VERSION:
	    pj_dump_config();
	    return PJ_EINVAL;

	case OPT_LOG_LEVEL:
	    settings.log_level = atoi(pj_optarg);
	    break;

	case OPT_APP_LOG_LEVEL:
	    settings.app_log_level = atoi(pj_optarg);
	    break;

	case OPT_LOG_FILE:
	    settings.log_file = pj_optarg;
	    break;

	case OPT_LOCAL_PORT:
	    settings.local_port = atoi(pj_optarg);
	    if (settings.local_port < 1 || settings.local_port > 65535) {
		PJ_LOG(1,(THIS_FILE,"Invalid --local-port %s", pj_optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_STATELESS:
	    settings.stateless = 1;
	    break;

	case OPT_THREAD_CNT:
	    settings.thread_cnt = atoi(pj_optarg);
	    if (settings.thread_cnt < 1 || 
		settings.thread_cnt > PJ_ARRAY_SIZE(settings.thread)) 
	    {
		PJ_LOG(1,(THIS_FILE,"Invalid --thread-cnt %s", pj_optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_START_RATE:
	    settings.start_rate = atoi(pj_optarg);
	    if (settings.start_rate < 1 || settings.start_rate > 1000000) {
		PJ_LOG(1,(THIS_FILE,"Invalid --start-rate %s", pj_optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_MAX_CAPACITY:
	    settings.max_capacity = atoi(pj_optarg);
	    if (settings.max_capacity < 1 || settings.max_capacity > 65000) {
		PJ_LOG(1,(THIS_FILE,
			  "Invalid --max-capacity %s (range=1-65000)", 
			  pj_optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_DURATION:
	    settings.duration = atoi(pj_optarg);
	    if (settings.duration < 0 || settings.duration > 1000000) {
		PJ_LOG(1,(THIS_FILE,"Invalid --duration %s", pj_optarg));
		return PJ_EINVAL;
	    }
	    break;

	}
    }

    if (pj_optind != argc) {
	if (verify_sip_url(argv[pj_optind]) != PJ_SUCCESS) {
	    PJ_LOG(3,(THIS_FILE, "Invalid SIP URL %s", argv[pj_optind]));
	    return PJ_EINVAL;
	}
	
	settings.target = pj_str(argv[pj_optind]);
	++pj_optind;
    }

    if (pj_optind != argc) {
	printf("Error: unknown options %s\n", argv[pj_optind]);
	return PJ_EINVAL;
    }

    return PJ_SUCCESS;
}


static void spawn_batch( pj_timer_heap_t *timer_heap,
			 struct pj_timer_entry *entry );

/* Completion callback. */
static void completion_cb(void *token, pj_bool_t success)
{
    batch *batch = token;

    if (success)
	batch->success++;
    else
	batch->failed++;

    if (batch->success+batch->failed == batch->rate) {
	pj_time_val elapsed, sess_elapsed;
	unsigned msec;

	pj_gettimeofday(&batch->end_time);
	elapsed = sess_elapsed = batch->end_time;

	/* Batch time. */
	PJ_TIME_VAL_SUB(elapsed, batch->start_time);
	msec = PJ_TIME_VAL_MSEC(elapsed);
	if (msec == 0) msec = 1;

	/* Session time */
	PJ_TIME_VAL_SUB(sess_elapsed, settings.session->start_time);

	/* Spawn time */
	PJ_TIME_VAL_SUB(batch->spawned_time, batch->start_time);

	if (batch->failed) {
	    PJ_LOG(2,(THIS_FILE, 
		      "%02d:%02d:%02d: %d tasks in %d.%ds (%d tasks/sec), "
		      "spawn=time=%d.%d, FAILED=%d",
		      (sess_elapsed.sec / 3600),
		      (sess_elapsed.sec % 3600) / 60,
		      (sess_elapsed.sec % 60),
		      batch->rate,
		      elapsed.sec, elapsed.msec,
		      batch->rate * 1000 / msec,
		      batch->spawned_time.sec,
		      batch->spawned_time.msec,
		      batch->failed));
	} else {
	    PJ_LOG(3,(THIS_FILE, 
		      "%02d:%02d:%02d: %d tasks in %d.%ds (%d tasks/sec), "
		      "spawn=time=%d.%d",
		      (sess_elapsed.sec / 3600),
		      (sess_elapsed.sec % 3600) / 60,
		      (sess_elapsed.sec % 60),
		      batch->rate,
		      elapsed.sec, elapsed.msec,
		      batch->rate * 1000 / msec,
		      batch->spawned_time.sec,
		      batch->spawned_time.msec));
	}

	if (!settings.session->stopping) {
	    pj_time_val interval;

	    if (msec >= 1000)
		interval.sec = interval.msec = 0;
	    else
		interval.sec = 0, interval.msec = 1000-msec;

	    settings.timer.cb = &spawn_batch;
	    pjsip_endpt_schedule_timer( settings.endpt, &settings.timer, &interval);
	} else {
	    PJ_LOG(3,(THIS_FILE, "%.*s test session completed",
		      (int)settings.session->method.name.slen,
		      settings.session->method.name.ptr));
	    pj_pool_release(settings.session->pool);
	    settings.session = NULL;
	}
    }
}

/* Spawn new batch. */
static void spawn_batch( pj_timer_heap_t *timer_heap,
			 struct pj_timer_entry *entry )
{
    session *sess = settings.session;
    batch *batch;
    pj_status_t status = PJ_SUCCESS;
    pjsip_cred_info cred_info[1];
    pj_time_val elapsed;

    unsigned i;

    PJ_UNUSED_ARG(timer_heap);
    PJ_UNUSED_ARG(entry);

    if (!pj_list_empty(&sess->free_list)) {
	batch = sess->free_list.next;
	pj_list_erase(batch);
    } else {
	batch = pj_pool_alloc(sess->pool, sizeof(struct batch));
    }

    pj_gettimeofday(&batch->start_time);
    batch->rate = settings.cur_rate;
    batch->started = 0;
    batch->success = 0;
    batch->failed = 0;
    pj_gettimeofday(&batch->start_time);
    batch->spawned_time = batch->start_time;

    pj_list_push_back(&sess->active_list, batch);

    for (i=0; i<batch->rate; ++i) {
	pj_str_t from = { "sip:user@127.0.0.1", 18};

	if (sess->method.id == PJSIP_OPTIONS_METHOD) {
	    status = options_spawn_test(&settings.target, &from, 
					&settings.target,
					0, cred_info, NULL, batch, 
					&completion_cb);
	} else if (sess->method.id == PJSIP_INVITE_METHOD) {
	    status = call_spawn_test( &settings.target, &from, 
				      &settings.target,
				      0, cred_info, NULL, batch, 
				      &completion_cb);
	}
	if (status != PJ_SUCCESS)
	    break;

	batch->started++;

	elapsed.sec = elapsed.msec = 0;
	pjsip_endpt_handle_events(settings.endpt, &elapsed);
    }

    pj_gettimeofday(&batch->spawned_time);

    /// 
#if 0
    elapsed = batch->spawned_time;
    PJ_TIME_VAL_SUB(elapsed, batch->start_time);
    PJ_LOG(2,(THIS_FILE, "%d requests sent in %d ms", batch->started,
			 PJ_TIME_VAL_MSEC(elapsed)));
#endif

    sess->total_created += batch->started;
    
    batch = sess->active_list.next;
    sess->outstanding = 0;
    while (batch != &sess->active_list) {
	sess->outstanding += (batch->started - batch->success - batch->failed);

	if (batch->started == batch->success + batch->failed) {
	    struct batch *next = batch->next;
	    pj_list_erase(batch);
	    pj_list_push_back(&sess->free_list, batch);
	    batch = next;
	} else {
	    batch = batch->next;
	}
    }
}


/* Start new session */
static void start_session(pj_bool_t auto_repeat)
{
    pj_time_val interval = { 0, 0 };
    pj_pool_t *pool;
    session *sess;

    pool = pjsip_endpt_create_pool(settings.endpt, "session", 4000, 4000);
    if (!pool) {
	app_perror(THIS_FILE, "Unable to create pool", PJ_ENOMEM);
	return;
    }

    sess = pj_pool_zalloc(pool, sizeof(session));
    sess->pool = pool;
    sess->stopping = auto_repeat ? 0 : 1;
    sess->method = settings.method;

    pj_list_init(&sess->active_list);
    pj_list_init(&sess->free_list);
    pj_gettimeofday(&sess->start_time);

    settings.session = sess;

    settings.timer.cb = &spawn_batch;
    pjsip_endpt_schedule_timer( settings.endpt, &settings.timer, &interval);
}


/* Dump state */
static void dump(pj_bool_t detail)
{
    pjsip_endpt_dump(settings.endpt, detail);
    pjsip_tsx_layer_dump(detail);
    pjsip_ua_dump(detail);
}


/* help screen */
static void help_screen(void)
{
    puts  ("+============================================================================+");
    printf("| Current mode: %-10s    Current rate: %-5d     Call Capacity: %-7d |\n",
	    settings.method.name.ptr, settings.cur_rate, settings.max_capacity);
    printf("|                                                     Call Duration: %-7d |\n",
	    settings.duration);

    printf("| Total tx requests:  %-7u rx requests:  %-7u                          |\n",
	   settings.tx_req, settings.rx_req);
    printf("|       tx responses: %-7u rx responses: %-7u                          |\n",
	   settings.tx_res, settings.rx_res);
    puts  ("+--------------------------------------+-------------------------------------+");
    puts  ("|          Test Settings               |           Misc Commands:            |");
    puts  ("|                                      |                                     |");
    puts  ("|   m  Change mode                     |                                     |");
    puts  ("| + -  Increment/decrement rate by 10  |   d   Dump status                   |");
    puts  ("| * /  Increment/decrement rate by 100 |   dd  Dump detailed (e.g. tables)   |");
    puts  ("+--------------------------------------+-------------------------------------+");
    puts  ("|                              Test Commands                                 |");
    puts  ("|                                                                            |");
    puts  ("|  s   Start single test batch             c  Clear counters                 |");
    puts  ("| sc   Start continuous test               x  Stop continuous tests          |");
    puts  ("+----------------------------------------------------------------------------+");
    puts  ("|  q:  Quit                                                                  |");
    puts  ("+============================================================================+");
    puts  ("");

}

/*
 * Input simple string
 */
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

/* Main input loop */
static void test_main(void)
{
    char menuin[10];
    char input[80];

    settings.cur_rate = settings.start_rate;

    help_screen();

    for (;;) {
	printf(">>>> "); fflush(stdout);

	fgets(menuin, sizeof(menuin), stdin);

	switch (menuin[0]) {
	case 's':
	    if (settings.session != NULL) {
		PJ_LOG(3,(THIS_FILE,"Error: another session is in progress"));
	    } else if (settings.target.slen == 0) {
		PJ_LOG(3,(THIS_FILE,"Error: target URL is not configured"));
	    } else {
		start_session(menuin[1]=='c');
	    }
	    break;

	case 'x':
	    if (settings.session) {
		settings.session->stopping = 1;
		puts("Stopping sessions...");
	    } else {
		PJ_LOG(3,(THIS_FILE,"Error: no sessions"));
	    }
	    break;

	case 'c':
	    /* Clear counters */
	    settings.rx_req = settings.rx_res = settings.tx_req = 
		settings.tx_res = 0;
	    puts("Counters cleared");
	    break;

	case 'm':
	    if (!simple_input("Change method [OPTIONS,INVITE]", input, sizeof(input)))
		continue;

	    if (pj_ansi_stricmp(input, "OPTIONS")==0)
		pjsip_method_set(&settings.method, PJSIP_OPTIONS_METHOD);
	    else if (pj_ansi_stricmp(input, "INVITE")==0)
		pjsip_method_set(&settings.method, PJSIP_INVITE_METHOD);
	    else {
		puts("Error: invalid method");
	    }
	    break;

	case 'd':
	    dump(menuin[1]=='d');
	    break;

	case '+':
	    settings.cur_rate += 10;
	    PJ_LOG(3,(THIS_FILE, "Rate is now %d", settings.cur_rate));
	    break;

	case '-':
	    if (settings.cur_rate > 10) {
		settings.cur_rate -= 10;
		PJ_LOG(3,(THIS_FILE, "Rate is now %d", settings.cur_rate));
	    }
	    break;

	case '*':
	    settings.cur_rate += 100;
	    PJ_LOG(3,(THIS_FILE, "Rate is now %d", settings.cur_rate));
	    break;

	case '/':
	    if (settings.cur_rate > 100) {
		settings.cur_rate -= 100;
		PJ_LOG(3,(THIS_FILE, "Rate is now %d", settings.cur_rate));
	    }
	    break;

	case 'q':
	    return;

	default:
	    help_screen();
	    break;

	}
    }
}


/* main() */
int main(int argc, char *argv[])
{
    pj_status_t status;

    init_settings();

    status = parse_options(argc, argv);
    if (status != PJ_SUCCESS)
	return 1;

    status = initialize();
    if (status != PJ_SUCCESS)
	return 1;


    test_main();

    shutdown();

    return 0;
}


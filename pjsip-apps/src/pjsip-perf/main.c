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
#include <pjsua-lib/getopt.h>
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

    settings.stateless = 1;
    settings.start_rate = 10;
    settings.max_capacity = 64;
    settings.duration = 0;
    settings.thread_cnt = 1;
    settings.local_port = 5060;

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

/* Initialize */
static pj_status_t initialize(void)
{
    pj_sockaddr_in addr;
    int i;
    pj_status_t status;

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
    status = pjmedia_endpt_create(&settings.cp.factory, &settings.med_endpt);
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


    /* Start worker thread. */
    for (i=0; i<settings.thread_cnt; ++i) {
	status = pj_thread_create(settings.pool, "pjsip-perf", &poll_pjsip,
				  NULL, 0, 0, &settings.thread[i]);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to create thread", status);
	    return status;
	}
    }

    pj_log_set_level(3);
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
	OPT_LOCAL_PORT,
	OPT_STATELESS,
	OPT_THREAD_CNT,
	OPT_START_RATE,
	OPT_MAX_CAPACITY,
	OPT_DURATION
    };
    struct option long_opts[] = {
	{ "help",	    0, 0, OPT_HELP},
	{ "version",	    0, 0, OPT_VERSION},
	{ "local-port",	    1, 0, OPT_LOCAL_PORT},
	{ "stateless",	    0, 0, OPT_STATELESS},
	{ "thread-cnt",	    1, 0, OPT_THREAD_CNT},
	{ "start-rate",	    1, 0, OPT_START_RATE},
	{ "max-capacity",   1, 0, OPT_MAX_CAPACITY},
	{ "duration",	    1, 0, OPT_DURATION},
	{ NULL, 0, 0, 0}
    };
    int c, option_index;

    optind = 0;
    while ((c=getopt_long(argc, argv, "", long_opts, &option_index)) != -1) {
	switch (c) {

	case OPT_HELP:
	    usage();
	    return PJ_EINVAL;

	case OPT_VERSION:
	    pj_dump_config();
	    return PJ_EINVAL;

	case OPT_LOCAL_PORT:
	    settings.local_port = atoi(optarg);
	    if (settings.local_port < 1 || settings.local_port > 65535) {
		PJ_LOG(1,(THIS_FILE,"Invalid --local-port %s", optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_STATELESS:
	    settings.stateless = 1;
	    break;

	case OPT_THREAD_CNT:
	    settings.thread_cnt = atoi(optarg);
	    if (settings.thread_cnt < 1 || 
		settings.thread_cnt > PJ_ARRAY_SIZE(settings.thread)) 
	    {
		PJ_LOG(1,(THIS_FILE,"Invalid --thread-cnt %s", optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_START_RATE:
	    settings.start_rate = atoi(optarg);
	    if (settings.start_rate < 1 || settings.start_rate > 1000000) {
		PJ_LOG(1,(THIS_FILE,"Invalid --start-rate %s", optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_MAX_CAPACITY:
	    settings.max_capacity = atoi(optarg);
	    if (settings.max_capacity < 1 || settings.max_capacity > 65000) {
		PJ_LOG(1,(THIS_FILE,
			  "Invalid --max-capacity %s (range=1-65000)", 
			  optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_DURATION:
	    settings.duration = atoi(optarg);
	    if (settings.duration < 0 || settings.duration > 1000000) {
		PJ_LOG(1,(THIS_FILE,"Invalid --duration %s", optarg));
		return PJ_EINVAL;
	    }
	    break;

	}
    }

    if (optind != argc) {
	if (verify_sip_url(argv[optind]) != PJ_SUCCESS) {
	    PJ_LOG(3,(THIS_FILE, "Invalid SIP URL %s", argv[optind]));
	    return PJ_EINVAL;
	}
	
	settings.target = pj_str(argv[optind]);
	++optind;
    }

    if (optind != argc) {
	printf("Error: unknown options %s\n", argv[optind]);
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

	PJ_TIME_VAL_SUB(elapsed, batch->start_time);
	PJ_TIME_VAL_SUB(sess_elapsed, settings.session->start_time);
	msec = PJ_TIME_VAL_MSEC(elapsed);
	if (msec == 0) msec = 1;

	PJ_LOG(3,(THIS_FILE, "%02d:%02d:%02d: %d tasks in %d.%ds (%d tasks/sec)",
			     (sess_elapsed.sec / 3600),
			     (sess_elapsed.sec % 3600) / 60,
			     (sess_elapsed.sec % 60),
			     batch->rate,
			     elapsed.sec, elapsed.msec,
			     batch->rate * 1000 / msec));

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
    pj_time_val now, spawn_time, sess_time;

    unsigned i;

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
    }

    pj_gettimeofday(&now);
    spawn_time = sess_time = now;
    PJ_TIME_VAL_SUB(spawn_time, batch->start_time);
    PJ_TIME_VAL_SUB(sess_time, sess->start_time);

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
    pj_time_val interval = { 1, 0 };
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

    spawn_batch(NULL, NULL);
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

    puts  ("+--------------------------------------+-------------------------------------+");
    puts  ("|          Test Settings               |           Misc Commands:            |");
    puts  ("|                                      |                                     |");
    puts  ("|   m  Change mode                     |                                     |");
    puts  ("| + -  Increment/decrement rate by 10  |   d   Dump status                   |");
    puts  ("| * /  Increment/decrement rate by 100 |   d1  Dump detailed (e.g. tables)   |");
    puts  ("+--------------------------------------+-------------------------------------+");
    puts  ("|                              Test Commands                                 |");
    puts  ("|                                                                            |");
    puts  ("|  s   Start single test batch                                               |");
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
	    } else {
		PJ_LOG(3,(THIS_FILE,"Error: no sessions"));
	    }
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
	    dump(menuin[1]=='1');
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


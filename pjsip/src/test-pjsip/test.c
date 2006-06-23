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


#include "test.h"
#include <pjlib.h>
#include <pjsip.h>

#define THIS_FILE   "test.c"

#define DO_TEST(test)	do { \
			    PJ_LOG(3, (THIS_FILE, "Running %s...", #test));  \
			    rc = test; \
			    PJ_LOG(3, (THIS_FILE,  \
				       "%s(%d)",  \
				       (rc ? "..ERROR" : "..success"), rc)); \
			    if (rc!=0) goto on_return; \
			} while (0)



pjsip_endpoint *endpt;
int log_level = 3;

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[256];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(3,(THIS_FILE, "%s: [pj_status_t=%d] %s", msg, rc, errbuf));

}

void flush_events(unsigned duration)
{
    pj_time_val stop_time;

    pj_gettimeofday(&stop_time);
    stop_time.msec += duration;
    pj_time_val_normalize(&stop_time);

    /* Process all events for the specified duration. */
    for (;;) {
	pj_time_val timeout = {0, 1}, now;

	pjsip_endpt_handle_events(endpt, &timeout);

	pj_gettimeofday(&now);
	if (PJ_TIME_VAL_GTE(now, stop_time))
	    break;
    }
}

pj_status_t register_static_modules(pj_size_t *count, pjsip_module **modules)
{
    *count = 0;
    return PJ_SUCCESS;
}

int test_main(void)
{
    pj_status_t rc;
    pj_caching_pool caching_pool;
    const char *filename;
    int line;

    pj_log_set_level(log_level);
    /*
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME | 
                     PJ_LOG_HAS_MICRO_SEC);
     */

    if ((rc=pj_init()) != PJ_SUCCESS) {
	app_perror("pj_init", rc);
	return rc;
    }

    pj_dump_config();

    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0 );

    rc = pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);
    if (rc != PJ_SUCCESS) {
	app_perror("pjsip_endpt_create", rc);
	pj_caching_pool_destroy(&caching_pool);
	return rc;
    }

    PJ_LOG(3,(THIS_FILE,""));

    /* Init logger module. */
    init_msg_logger();
    msg_logger_set_enabled(1);

    /* Start transaction layer module. */
    rc = pjsip_tsx_layer_init_module(endpt);
    if (rc != PJ_SUCCESS) {
	app_perror("   Error initializing transaction module", rc);
	goto on_return;
    }

    /* Create loop transport. */
    rc = pjsip_loop_start(endpt, NULL);
    if (rc != PJ_SUCCESS) {
	app_perror("   error: unable to create datagram loop transport", 
		   rc);
	goto on_return;
    }

    DO_TEST(uri_test());
    DO_TEST(msg_test());
    DO_TEST(msg_err_test());
    DO_TEST(txdata_test());
    DO_TEST(transport_udp_test());
    DO_TEST(transport_loop_test());
    DO_TEST(tsx_basic_test());
    DO_TEST(tsx_uac_test());
    DO_TEST(tsx_uas_test());

on_return:

    pjsip_endpt_destroy(endpt);
    pj_caching_pool_destroy(&caching_pool);

    PJ_LOG(3,(THIS_FILE, ""));
 
    pj_thread_get_stack_info(pj_thread_this(), &filename, &line);
    PJ_LOG(3,(THIS_FILE, "Stack max usage: %u, deepest: %s:%u", 
	              pj_thread_get_stack_max_usage(pj_thread_this()),
		      filename, line));
    if (rc == 0)
	PJ_LOG(3,(THIS_FILE, "Looks like everything is okay!.."));
    else
	PJ_LOG(3,(THIS_FILE, "Test completed with error(s)"));

    return rc;
}


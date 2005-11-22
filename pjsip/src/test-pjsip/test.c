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
#include <pjsip_core.h>

#define DO_TEST(test)	do { \
			    PJ_LOG(3, ("test", "Running %s...", #test));  \
			    rc = test; \
			    PJ_LOG(3, ("test",  \
				       "%s(%d)",  \
				       (rc ? "..ERROR" : "..success"), rc)); \
			    if (rc!=0) goto on_return; \
			} while (0)



pjsip_endpoint *endpt;

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[256];

    PJ_CHECK_STACK();

    pjsip_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(3,("test", "%s: [pj_status_t=%d] %s", msg, rc, errbuf));

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

    pj_log_set_level(3);
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME | 
                     PJ_LOG_HAS_MICRO_SEC);

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

    PJ_LOG(3,("",""));

    DO_TEST(uri_test());
    //DO_TEST(msg_test());

on_return:

    pjsip_endpt_destroy(endpt);
    pj_caching_pool_destroy(&caching_pool);

    PJ_LOG(3,("test", ""));
 
    pj_thread_get_stack_info(pj_thread_this(), &filename, &line);
    PJ_LOG(3,("test", "Stack max usage: %u, deepest: %s:%u", 
	              pj_thread_get_stack_max_usage(pj_thread_this()),
		      filename, line));
    if (rc == 0)
	PJ_LOG(3,("test", "Looks like everything is okay!.."));
    else
	PJ_LOG(3,("test", "Test completed with error(s)"));

    return 0;
}


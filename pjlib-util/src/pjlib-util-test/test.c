/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util.h>

#define THIS_FILE   "test.c"

pj_pool_factory *mem;
struct test_app_t test_app = {
    .param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME |
                       PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_INDENT,
};

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[256];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(1,("test", "%s: [pj_status_t=%d] %s", msg, rc, errbuf));
}

static int test_inner(int argc, char *argv[])
{
    pj_caching_pool caching_pool;

    mem = &caching_pool.factory;

    pj_log_set_level(3);
    pj_log_set_decor(test_app.param_log_decor);

    PJ_TEST_SUCCESS(pj_init(), NULL, { return 1; })
    PJ_TEST_SUCCESS(pjlib_util_init(), NULL, { return 2; });
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0 );
    
    if (ut_app_init1(&test_app.ut_app, mem) != PJ_SUCCESS)
        return 1;

    pj_dump_config();

#if INCLUDE_XML_TEST
    UT_ADD_TEST(&test_app.ut_app, xml_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_JSON_TEST
    UT_ADD_TEST(&test_app.ut_app, json_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_ENCRYPTION_TEST
    UT_ADD_TEST(&test_app.ut_app, encryption_test, PJ_TEST_PARALLEL);
#   if WITH_BENCHMARK
    UT_ADD_TEST(&test_app.ut_app, encryption_benchmark, PJ_TEST_PARALLEL);
#   endif
#endif

#if INCLUDE_STUN_TEST
    UT_ADD_TEST(&test_app.ut_app, stun_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_RESOLVER_TEST
    UT_ADD_TEST(&test_app.ut_app, resolver_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_HTTP_CLIENT_TEST
    UT_ADD_TEST(&test_app.ut_app, http_client_test, PJ_TEST_PARALLEL);
#endif


    if (ut_run_tests(&test_app.ut_app, "pjlib-util tests", argc, argv)) {
        ut_app_destroy(&test_app.ut_app);
        return 1;
    }

    ut_app_destroy(&test_app.ut_app);
    return 0;
}

int test_main(int argc, char *argv[])
{
    PJ_USE_EXCEPTION;

    PJ_TRY {
        return test_inner(argc, argv);
    }
    PJ_CATCH_ANY {
        int id = PJ_GET_EXCEPTION();
        PJ_LOG(3,("test", "FATAL: unhandled exception id %d (%s)", 
                  id, pj_exception_id_name(id)));
    }
    PJ_END;

    return -1;
}


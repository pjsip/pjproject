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

#define THIS_FILE   "test.c"

#ifdef _MSC_VER
#  pragma warning(disable:4127)

  /* Disable ioqueue stress test on MSVC2005, due to compile errors on
   * structure field assignments.
   */
#  if _MSC_VER <= 1400
#    undef INCLUDE_IOQUEUE_STRESS_TEST
#    define INCLUDE_IOQUEUE_STRESS_TEST 0
#  endif

#endif

#define DO_TEST(test)   do { \
                            PJ_LOG(3, (THIS_FILE, "Running %s...", #test));  \
                            rc = test; \
                            PJ_LOG(3, (THIS_FILE,  \
                                       "%s(%d)",  \
                                       (rc ? "..ERROR" : "..success"), rc)); \
                            if (rc!=0) goto on_return; \
                        } while (0)


pj_pool_factory *mem;

int param_echo_sock_type;
const char *param_echo_server = ECHO_SERVER_ADDRESS;
int param_echo_port = ECHO_SERVER_START_PORT;
int param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME |
                      PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_INDENT;
pj_bool_t param_ci_mode = PJ_FALSE;  /* GH CI mode: more lenient tests */
pj_test_select_tests param_unittest_logging_policy = PJ_TEST_FAILED_TESTS;
int param_unittest_nthreads = -1;


int null_func()
{
    return 0;
}

static pj_bool_t test_included(const char *name, int argc, char *argv[])
{
    if (argc <= 1)
        return PJ_TRUE;

    ++argv;
    while (*argv) {
        if (pj_ansi_strcmp(name, *argv)==0)
            return PJ_TRUE;
        ++argv;
    }
    return PJ_FALSE;
}

static pj_test_case *init_test_case( int (*test_func)(void), const char *obj_name,
                                     unsigned flags, pj_test_case *tc,
                                     char *log_buf, unsigned log_buf_size)
{
    flags |= PJ_TEST_FUNC_NO_ARG;
    pj_test_case_init(tc, obj_name, flags, (int (*)(void*))test_func, NULL,
                      log_buf, log_buf_size, NULL);
    return tc;
}

static int essential_tests(int argc, char *argv[])
{
    pj_test_suite suite;
    pj_test_runner runner;
    pj_test_stat stat;
    enum { 
        MAX_TESTS = 12,
        LOG_BUF_SIZE = 256,
    };
    char log_bufs[MAX_TESTS][LOG_BUF_SIZE];
    pj_test_case test_cases[MAX_TESTS];
    int ntests = 0;

    /* Test the unit-testing framework first, outside unit-test! */
    PJ_LOG(3,(THIS_FILE, "Testing the unit-test framework (basic)"));
    if (unittest_basic_test())
        return 1;

    /* Now that the basic unit-testing framework has been tested, 
     * perform essential tests using basic unit-testing framework.
     */
    pj_test_suite_init(&suite);

#define ADD_TEST(test_func, flags) \
    if (ntests < MAX_TESTS) { \
        const char *test_name = #test_func; \
        if (test_included(test_name, argc, argv)) { \
            pj_test_case *tc = init_test_case( &test_func, test_name, flags, \
                                               &test_cases[ntests], \
                                               log_bufs[ntests], \
                                               LOG_BUF_SIZE); \
            pj_test_suite_add_case( &suite, tc); \
            ++ntests; \
        } \
    } else { \
        PJ_LOG(1,(THIS_FILE, "Too many tests for adding %s", #test_func)); \
    }


#if INCLUDE_ERRNO_TEST
    ADD_TEST( errno_test, 0);
#endif

#if INCLUDE_EXCEPTION_TEST
    ADD_TEST( exception_test, 0);
#endif

#if INCLUDE_OS_TEST
    ADD_TEST( os_test, 0);
#endif

#if INCLUDE_LIST_TEST
    ADD_TEST( list_test, 0);
#endif

#if INCLUDE_POOL_TEST
    ADD_TEST( pool_test, 0);
#endif

#if INCLUDE_STRING_TEST
    ADD_TEST( string_test, 0);
#endif

#if INCLUDE_FIFOBUF_TEST
    ADD_TEST( fifobuf_test, 0);
#endif

#if INCLUDE_MUTEX_TEST
    ADD_TEST( mutex_test, 0);
#endif

#if INCLUDE_THREAD_TEST
    ADD_TEST( thread_test, 0);
#endif

#undef ADD_TEST

    PJ_LOG(3,(THIS_FILE, "Performing %d essential tests", ntests));
    pj_test_init_basic_runner(&runner);
    pj_test_run(&runner, &suite);
    pj_test_get_stat(&suite, &stat);
    pj_test_display_stat(&stat, "essential tests", THIS_FILE);
    pj_test_display_log_messages(&suite, param_unittest_logging_policy);

    if (stat.nfailed)
        return 1;

    /* Now that the essential components have been tested, test the
     * multithreaded unit-testing framework.
     */
    PJ_LOG(3,(THIS_FILE, "Testing the unit-test framework (multithread)"));
    if (unittest_test())
        return 1;
    
    return 0;
}

static int features_tests(int argc, char *argv[])
{
    pj_test_suite suite;
    pj_test_runner *runner;
    pj_test_text_runner_param prm;
    pj_test_stat stat;
    pj_pool_t *pool;
    enum { 
        MAX_TESTS = 24,
        LOG_BUF_SIZE = 1000,
    };
    pj_test_case test_cases[MAX_TESTS];
    int ntests = 0;
    pj_status_t status;

    pool = pj_pool_create(mem, "test.c", 4000, 4000, NULL);
    if (!pool) {
        PJ_LOG(1,(THIS_FILE, "Pool creation error"));
        return 1;
    }
    pj_test_suite_init(&suite);

#define ADD_TEST(test_func, flags) \
    if (ntests < MAX_TESTS) { \
        const char *test_name = #test_func; \
        if (test_included(test_name, argc, argv)) { \
            char *log_buf = (char*)pj_pool_alloc(pool, LOG_BUF_SIZE); \
            pj_test_case *tc = init_test_case( &test_func, test_name, flags, \
                                               &test_cases[ntests], \
                                               log_buf, LOG_BUF_SIZE); \
            pj_test_suite_add_case( &suite, tc); \
            ++ntests; \
        } \
    } else { \
        PJ_LOG(1,(THIS_FILE, "Too many tests for adding %s", #test_func)); \
    }

#if INCLUDE_RAND_TEST
    ADD_TEST( rand_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_POOL_PERF_TEST
    ADD_TEST( pool_perf_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_RBTREE_TEST
    ADD_TEST( rbtree_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_HASH_TEST
    ADD_TEST( hash_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_TIMESTAMP_TEST
    ADD_TEST( timestamp_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_ATOMIC_TEST
    ADD_TEST( atomic_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_TIMER_TEST
    ADD_TEST( timer_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SLEEP_TEST
    ADD_TEST( sleep_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SOCK_TEST
    ADD_TEST( sock_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SOCK_PERF_TEST
    ADD_TEST( sock_perf_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SELECT_TEST
    ADD_TEST( select_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_UDP_IOQUEUE_TEST
    ADD_TEST( udp_ioqueue_test, PJ_TEST_PARALLEL);
#endif

#if PJ_HAS_TCP && INCLUDE_TCP_IOQUEUE_TEST
    ADD_TEST( tcp_ioqueue_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_IOQUEUE_UNREG_TEST
    ADD_TEST( udp_ioqueue_unreg_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_IOQUEUE_STRESS_TEST
    ADD_TEST( ioqueue_stress_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_IOQUEUE_PERF_TEST
    ADD_TEST( ioqueue_perf_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_ACTIVESOCK_TEST
    ADD_TEST( activesock_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_FILE_TEST
    ADD_TEST( file_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SSLSOCK_TEST
    ADD_TEST( ssl_sock_test, PJ_TEST_PARALLEL);
#endif


#undef ADD_TEST

    pj_test_text_runner_param_default(&prm);
    if (param_unittest_nthreads >= 0)
        prm.nthreads = param_unittest_nthreads;
    status = pj_test_create_text_runner(pool, &prm, &runner);
    if (status != PJ_SUCCESS) {
        app_perror("Error creating text runner", status);
        return 1;
    }

    PJ_LOG(3,(THIS_FILE,
              "Performing %d features tests with %d worker thread%s", 
              ntests, prm.nthreads, prm.nthreads>1?"s":""));
    pj_test_run(runner, &suite);
    pj_test_runner_destroy(runner);
    pj_test_get_stat(&suite, &stat);
    pj_test_display_stat(&stat, "features tests", THIS_FILE);
    pj_test_display_log_messages(&suite, param_unittest_logging_policy);
    pj_pool_release(pool);
    
    return stat.nfailed ? 1 : 0;
}

int test_inner(int argc, char *argv[])
{
    pj_caching_pool caching_pool;
    const char *filename;
    int line;
    int rc = 0;

    mem = &caching_pool.factory;

    pj_log_set_level(3);
    pj_log_set_decor(param_log_decor);

    rc = pj_init();
    if (rc != 0) {
        app_perror("pj_init() error!!", rc);
        return rc;
    }

    pj_dump_config();
    pj_caching_pool_init( &caching_pool, NULL, 0 );

    if (param_ci_mode)
        PJ_LOG(3,(THIS_FILE, "Using ci-mode"));

    rc = essential_tests(argc, argv);
    if (rc)
        return rc;

    rc = features_tests(argc, argv);
    if (rc)
        return rc;

#if INCLUDE_ECHO_SERVER
    //echo_server();
    //echo_srv_sync();
    udp_echo_srv_ioqueue();

#elif INCLUDE_ECHO_CLIENT
    if (param_echo_sock_type == 0)
        param_echo_sock_type = pj_SOCK_DGRAM();

    echo_client( param_echo_sock_type,
                 param_echo_server,
                 param_echo_port);
#endif

    goto on_return;

on_return:

    pj_caching_pool_destroy( &caching_pool );

    PJ_LOG(3,(THIS_FILE, " "));

    pj_thread_get_stack_info(pj_thread_this(), &filename, &line);
    PJ_LOG(3,(THIS_FILE, "Stack max usage: %u, deepest: %s:%u",
                      pj_thread_get_stack_max_usage(pj_thread_this()),
                      filename, line));
    if (rc == 0)
        PJ_LOG(3,(THIS_FILE, "Looks like everything is okay!.."));
    else
        PJ_LOG(3,(THIS_FILE, "Test completed with error(s)"));

    pj_shutdown();

    return rc;
}

#include <pj/sock.h>

int test_main(int argc, char *argv[])
{
    PJ_USE_EXCEPTION;

    PJ_TRY {
        return test_inner(argc, argv);
    }
    PJ_CATCH_ANY {
        int id = PJ_GET_EXCEPTION();
        PJ_LOG(3,(THIS_FILE, "FATAL: unhandled exception id %d (%s)",
                  id, pj_exception_id_name(id)));
    }
    PJ_END;

    return -1;
}

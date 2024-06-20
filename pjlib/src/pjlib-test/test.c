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

pj_pool_factory *mem;

struct test_app_t test_app = {
    .param_echo_sock_type = 0,
    .param_echo_server    = ECHO_SERVER_ADDRESS,
    .param_echo_port      = ECHO_SERVER_START_PORT,
    .param_log_decor      = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME |
                            PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_INDENT,
    .param_ci_mode        = PJ_FALSE,
};

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

static void list_tests(const pj_test_suite *suite, const char *title)
{
    unsigned d = pj_log_get_decor();
    const pj_test_case *tc;

    pj_log_set_decor(d ^ PJ_LOG_HAS_NEWLINE);
    PJ_LOG(3,(THIS_FILE, "%ld %s:", pj_list_size(&suite->tests), title));
    pj_log_set_decor(0);
    for (tc=suite->tests.next; tc!=&suite->tests; tc=tc->next) {
        PJ_LOG(3,(THIS_FILE, " %s", tc->obj_name));
    }
    PJ_LOG(3,(THIS_FILE, "\n"));
    pj_log_set_decor(d);
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

    /* Test the unit-testing framework first, outside unit-test! 
     * Only perform the test if user is not requesting specific test.
     */
    if (argc==1) {
        pj_dump_config();

        PJ_LOG(3,(THIS_FILE, "Testing the unit-test framework (basic)"));
        if (unittest_basic_test())
            return 1;
    }

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

    if (test_app.ut_app.prm_list_test) {
        list_tests(&suite, "essential tests");
        return 0;
    }

    if (ntests > 0) {
        pj_test_runner_param runner_prm;
        pj_test_runner_param_default(&runner_prm);
        runner_prm.stop_on_error = test_app.ut_app.prm_stop_on_error;

        PJ_LOG(3,(THIS_FILE, "Performing %d essential tests", ntests));
        pj_test_init_basic_runner(&runner, &runner_prm);
        pj_test_run(&runner, &suite);
        pj_test_get_stat(&suite, &stat);
        pj_test_display_stat(&stat, "essential tests", THIS_FILE);
        pj_test_display_log_messages(&suite, 
                                     test_app.ut_app.prm_logging_policy);

        if (stat.nfailed)
            return 1;
    }

    /* Now that the essential components have been tested, test the
     * multithreaded unit-testing framework.
     */
    if (argc==1) {
        PJ_LOG(3,(THIS_FILE, "Testing the unit-test test scheduling"));
        if (unittest_parallel_test())
            return 1;

        PJ_LOG(3,(THIS_FILE, "Testing the unit-test framework (multithread)"));
        if (unittest_test())
            return 1;
    }

    return 0;
}

static int features_tests(int argc, char *argv[])
{
    if (ut_app_init1(&test_app.ut_app, mem) != PJ_SUCCESS)
        return 1;

#if INCLUDE_RAND_TEST
    UT_ADD_TEST(&test_app.ut_app, rand_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_POOL_PERF_TEST
    UT_ADD_TEST(&test_app.ut_app, pool_perf_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_RBTREE_TEST
    UT_ADD_TEST(&test_app.ut_app, rbtree_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_HASH_TEST
    UT_ADD_TEST(&test_app.ut_app, hash_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_TIMESTAMP_TEST
    UT_ADD_TEST(&test_app.ut_app, timestamp_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_ATOMIC_TEST
    UT_ADD_TEST(&test_app.ut_app, atomic_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_TIMER_TEST
    UT_ADD_TEST(&test_app.ut_app, timer_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SLEEP_TEST
    UT_ADD_TEST(&test_app.ut_app, sleep_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_FILE_TEST
    UT_ADD_TEST(&test_app.ut_app, file_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SOCK_TEST
    UT_ADD_TEST(&test_app.ut_app, sock_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SOCK_PERF_TEST
    UT_ADD_TEST(&test_app.ut_app, sock_perf_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SELECT_TEST
    UT_ADD_TEST(&test_app.ut_app, select_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_UDP_IOQUEUE_TEST
    UT_ADD_TEST(&test_app.ut_app, udp_ioqueue_test, PJ_TEST_PARALLEL);
#endif

#if PJ_HAS_TCP && INCLUDE_TCP_IOQUEUE_TEST
    UT_ADD_TEST(&test_app.ut_app, tcp_ioqueue_test, PJ_TEST_PARALLEL);
#endif

    /* Consistently encountered retcode 520 on Windows virtual machine
       with 8 vcpu and 16GB RAM when multithread unit test is used,
       with the following logs:
    17:50:57.761 .tcp (multithreads)
    17:50:58.254 ...pj_ioqueue_send() error: Object is busy (PJ_EBUSY)
    17:50:58.264 ..test failed (retcode=520)
    17:50:58.264 .tcp (multithreads, sequenced, concur=0)
    17:51:06.084 .tcp (multithreads, sequenced, concur=1)
    17:51:06.484 ...pj_ioqueue_send() error: Object is busy (PJ_EBUSY)
    17:51:06.486 ..test failed (retcode=520)

    I suspect it's because the ioq stress test also uses a lot of threads
    and couldn't keep up with processing the data.
    Therefore we'll disable parallelism on Windows for this test. [blp]
    */
#if INCLUDE_IOQUEUE_STRESS_TEST
#  if defined(PJ_WIN32) && PJ_WIN32!=0
    UT_ADD_TEST(&test_app.ut_app, ioqueue_stress_test, 0);
#  else
    UT_ADD_TEST(&test_app.ut_app, ioqueue_stress_test, PJ_TEST_PARALLEL);
#  endif
#endif

#if INCLUDE_IOQUEUE_UNREG_TEST
    UT_ADD_TEST(&test_app.ut_app, udp_ioqueue_unreg_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_IOQUEUE_PERF_TEST
    UT_ADD_TEST(&test_app.ut_app, ioqueue_perf_test0, PJ_TEST_PARALLEL);
    UT_ADD_TEST(&test_app.ut_app, ioqueue_perf_test1, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_ACTIVESOCK_TEST
    UT_ADD_TEST(&test_app.ut_app, activesock_test, PJ_TEST_PARALLEL);
#endif

#if INCLUDE_SSLSOCK_TEST
    UT_ADD_TEST(&test_app.ut_app, ssl_sock_test, PJ_TEST_PARALLEL);
#endif


#undef ADD_TEST

    if (ut_run_tests(&test_app.ut_app, "features tests", argc, argv)) {
        ut_app_destroy(&test_app.ut_app);
        return 1;
    }

    ut_app_destroy(&test_app.ut_app);
    return 0;
}

int test_inner(int argc, char *argv[])
{
    pj_caching_pool caching_pool;
    const char *filename;
    int line;
    int rc = 0;

    mem = &caching_pool.factory;

    pj_log_set_level(3);
    pj_log_set_decor(test_app.param_log_decor);

    rc = pj_init();
    if (rc != 0) {
        app_perror("pj_init() error!!", rc);
        return rc;
    }

    pj_caching_pool_init( &caching_pool, NULL, 0 );

    if (test_app.param_ci_mode)
        PJ_LOG(3,(THIS_FILE, "Using ci-mode"));

    if (!test_app.param_skip_essentials) {
        rc = essential_tests(argc, argv);
        if (rc)
            goto on_return;
    }

    rc = features_tests(argc, argv);
    if (rc)
        goto on_return;

#if INCLUDE_ECHO_SERVER
    //echo_server();
    //echo_srv_sync();
    udp_echo_srv_ioqueue();

#elif INCLUDE_ECHO_CLIENT
    if (test_app.param_echo_sock_type == 0)
        test_app.param_echo_sock_type = pj_SOCK_DGRAM();

    echo_client( test_app.param_echo_sock_type,
                 test_app.param_echo_server,
                 test_app.param_echo_port);
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
        PJ_LOG(3,(THIS_FILE, "**Test completed with error(s)**"));

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

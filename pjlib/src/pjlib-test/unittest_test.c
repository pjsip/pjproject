/* 
 * Copyright (C) 2008-2024 Teluu Inc. (http://www.teluu.com)
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


/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_unittest_test;

#if INCLUDE_UNITTEST_TEST

#define THIS_FILE   "unittest_test.c"

static char log_buffer[1024], *log_buffer_ptr = log_buffer;
static pj_log_func *old_log_func;

static void reset_log_buffer()
{
    log_buffer_ptr = log_buffer;
    *log_buffer_ptr = '\0';
}

#if 0
static void print_log_buffer(char *title)
{
    printf("------ log buffer %s: ------\n", title);
    printf("%s", log_buffer);
    printf("%s\n", "------ end buffer: ------");
}
#else
#define print_log_buffer(title)
#endif

/* This log callback appends the log to the log_buffer */
static void log_callback(int level, const char *data, int len)
{
    unsigned max_len = (log_buffer+sizeof(log_buffer))-log_buffer_ptr-1;

    /* make sure len is correct */
    len = strlen(data);
    if (len > max_len)
        len = max_len;

    pj_ansi_strxcpy(log_buffer_ptr, data, max_len);
    log_buffer_ptr += len;
}

static void start_capture_log()
{
    old_log_func = pj_log_get_log_func();
    pj_log_set_log_func(&log_callback);
    reset_log_buffer();
}

static void end_capture_log()
{
    pj_log_set_log_func(old_log_func);
}

static int test_true(int is_true, const char *reason)
{
    PJ_TEST_TRUE(is_true, reason, return -100);
    return 0;
}

static int test_eq(int value0, int value1, const char *reason)
{
    PJ_TEST_EQ(value0, value1, reason, return -200);
    return 0;
}

static int test_success(pj_status_t status, const char *reason)
{
    PJ_TEST_SUCCESS(status, reason, return -300);
    return 0;
}

/*
 * Test various assertion tests
 */
static int assertion_tests()
{
    int ret;

    /* Unary PJ_TEST_TRUE successful */
    ret = test_true(1, NULL);
    if (ret != 0) return ret;

    /* PJ_TEST_TRUE successful, reason is specified (but not used) */
    ret = test_true(1, "logic error");
    if (ret != 0) return ret;

    /* PJ_TEST_TRUE fails, without reason */
    reset_log_buffer();
    ret = test_true(0, NULL);
    if (ret != -100) return -110;

    /* Check log message, should be something like:
       09:47:09.692 Test "is_true" fails in unittest_test.c:44
     */
    if (pj_ansi_strlen(log_buffer) < 10) return -120;
    if (!pj_ansi_strstr(log_buffer, "is_true")) return -121;
    if (!pj_ansi_strstr(log_buffer, THIS_FILE)) return -122;

    /* PJ_TEST_TRUE fails, reason is specified */
    reset_log_buffer();
    ret = test_true(0, "logic error");
    if (ret != -100) return -110;

    /* Check log message, should be something like:
       09:47:37.145 Test "is_true" fails in unittest_test.c:44 (logic error)
     */
    if (pj_ansi_strlen(log_buffer) < 10) return -130;
    if (!pj_ansi_strstr(log_buffer, "is_true")) return -131;
    if (!pj_ansi_strstr(log_buffer, THIS_FILE)) return -132;
    if (!pj_ansi_strstr(log_buffer, " (logic error)")) return -132;

    /* Binary PJ_TEST_EQ successful */
    ret = test_eq(999, 999, NULL);
    if (ret != 0) return ret;

    ret = test_eq(999, 999, "not used");
    if (ret != 0) return ret;

    /* Binary comparison PJ_TEST_EQ fails, reason not given */
    reset_log_buffer();
    ret = test_eq(998, 999, NULL);
    if (ret != -200) return -210;
    
    /* Check log message, should be something like:
       09:47:56.315 Test "value0" (998) == "value1" (999) fails in unittest_test.c:50
     */
    if (pj_ansi_strlen(log_buffer) < 10) return -220;
    if (!pj_ansi_strstr(log_buffer, "value0")) return -221;
    if (!pj_ansi_strstr(log_buffer, "value1")) return -222;
    if (!pj_ansi_strstr(log_buffer, "998")) return -223;
    if (!pj_ansi_strstr(log_buffer, "999")) return -224;
    if (!pj_ansi_strstr(log_buffer, THIS_FILE)) return -225;

    /* Binary comparison PJ_TEST_EQ fails, reason is given */
    reset_log_buffer();
    ret = test_eq(998, 999, "values are different");
    if (ret != -200) return -250;

    /* Check log message, should be something like:
       09:51:37.866 Test "value0" (998) == "value1" (999) fails in unittest_test.c:50 (values are different)
     */
    if (pj_ansi_strlen(log_buffer) < 10) return -260;
    if (!pj_ansi_strstr(log_buffer, "value0")) return -261;
    if (!pj_ansi_strstr(log_buffer, "value1")) return -262;
    if (!pj_ansi_strstr(log_buffer, "998")) return -263;
    if (!pj_ansi_strstr(log_buffer, "999")) return -264;
    if (!pj_ansi_strstr(log_buffer, THIS_FILE)) return -265;
    if (!pj_ansi_strstr(log_buffer, " (values are different)")) return -266;

    /* PJ_TEST_SUCCESS successful scenario */
    ret = test_success(PJ_SUCCESS, NULL);
    if (ret != 0) return ret;

    /* PJ_TEST_SUCCESS successful, reason is specified (but not used) */
    ret = test_success(PJ_SUCCESS, "logic error");
    if (ret != 0) return ret;

    /* PJ_TEST_SUCCESS fails, without reason */
    reset_log_buffer();
    ret = test_success(PJ_EPENDING, NULL);
    if (ret != -300) return -310;

    /* Check log message, should be something like:
       09:52:22.654 "status" fails in unittest_test.c:56, status=70002 (Pending operation (PJ_EPENDING))
     */
    if (pj_ansi_strlen(log_buffer) < 10) return -320;
    if (!pj_ansi_strstr(log_buffer, "Pending operation")) return -321;
    if (!pj_ansi_strstr(log_buffer, THIS_FILE)) return -322;

    /* PJ_TEST_SUCCESS fails, reason given */
    reset_log_buffer();
    ret = test_success(PJ_EPENDING, "should be immediate");
    if (ret != -300) return -350;

    /* Check log message, should be something like:
       09:52:49.717 "status" fails in unittest_test.c:56, status=70002 (Pending operation (PJ_EPENDING)) (should be immediate)
     */
    if (pj_ansi_strlen(log_buffer) < 10) return -350;
    if (!pj_ansi_strstr(log_buffer, "Pending operation")) return -351;
    if (!pj_ansi_strstr(log_buffer, THIS_FILE)) return -352;
    if (!pj_ansi_strstr(log_buffer, " (should be immediate)")) return -353;

    return 0;
}


enum test_flags
{
    /* value 0-31 is reserved for test id */
    TEST_LOG_DETAIL   = 32,
    TEST_LOG_INFO     = 64,
    TEST_LOG_ALL      = (TEST_LOG_DETAIL | TEST_LOG_INFO),

    TEST_RETURN_ERROR = 256,
};

/** Dummy test */
static int func_to_test(void *arg)
{
    unsigned flags = (unsigned)(long)arg;
    unsigned test_id = (flags & 31);
    
    /* Note that for simplicity, make the length of log messages the same
     * (otherwise freeing one oldest log may not be enough to fit in the
     * later log)
     */
    if (flags & TEST_LOG_DETAIL) {
        PJ_LOG(4,(THIS_FILE, "Entering func_to_test(%d).....", test_id));
    }

    if (flags & TEST_LOG_INFO) {
        PJ_LOG(3,(THIS_FILE, "Performing func_to_test(%d)...", test_id));
    }

    if (flags & TEST_RETURN_ERROR) {
        PJ_LOG(1,(THIS_FILE, "Some error in func_to_test(%d)", test_id));
    }

    /* Simulate some work and additional sleep to ensure tests
     * completes in correct order
     */
    pj_thread_sleep(100+test_id*100);

    return (flags & TEST_RETURN_ERROR) ? -123 : 0;
}

enum { 
    /* approx len of each log msg in func_to_test() 
     * Note that logging adds decor e.g. time, plus overhead in fifobuf.
     */
    MSG_LEN = 45 + 4 + sizeof(pj_test_log_item),
};

/**
 * Simple demonstration on how to use the unittest framework.
 * Here we use the unittest framework to test the unittest framework itself.
 * We test both the basic and text runner.
 */
static int usage_test(pj_pool_t *pool, pj_bool_t basic, pj_bool_t parallel,
                      unsigned log_size)
{
    enum {
        TEST_CASE_LOG_SIZE = 256,
    };
    char test_title[80];
    pj_test_suite suite;
    unsigned flags;
    char buffer0[TEST_CASE_LOG_SIZE], buffer1[TEST_CASE_LOG_SIZE];
    pj_test_case test_case0, test_case1;
    pj_test_runner *runner;
    pj_test_runner basic_runner;
    pj_test_stat stat;

    /* to differentiate different invocations of this function */
    pj_ansi_snprintf(test_title, sizeof(test_title),
                     "basic=%d, parallel=%d, log_size=%d", 
                     basic, parallel, log_size);
    //PJ_LOG(3,(THIS_FILE, "Unittest usage_test: %s", test_title));

    PJ_TEST_LTE(log_size, TEST_CASE_LOG_SIZE, test_title, return -1);

    /* Init test suite */
    pj_test_suite_init(&suite);

    if (basic)
        flags = 0;
    else
        flags = parallel? PJ_TEST_PARALLEL : 0;

    /* Add test case 0. This test case writes some logs and returns
     * success.
     */
    
    pj_test_case_init(&test_case0, "successful test", flags, &func_to_test,
                      (void*)(long)(0+TEST_LOG_ALL),
                      buffer0, log_size, NULL);
    pj_test_suite_add_case(&suite, &test_case0);

    /* Add test case 1. This test case simulates error. It writes
     * error to log and returns non-zero error.
     */
    pj_test_case_init(&test_case1, "failure test", flags, &func_to_test, 
                      (void*)(long)(1+TEST_LOG_ALL+TEST_RETURN_ERROR),
                      buffer1, log_size, NULL);
    pj_test_suite_add_case(&suite, &test_case1);

    /* Create runner */
    if (basic) {
        runner = &basic_runner;
        pj_test_init_basic_runner(runner, NULL);
    } else {
        pj_test_runner_param prm;
        pj_test_runner_param_default(&prm);
        prm.nthreads = 4; /* more threads than we need, for testing */
        PJ_TEST_SUCCESS(pj_test_create_text_runner(pool, &prm, &runner), 
                        test_title, return -10);
    }

    /* Run runner */
    pj_test_run(runner, &suite);

    /* Runner can be safely destroyed now */
    pj_test_runner_destroy(runner);

    /* test the statistics returned by pj_test_get_stat() */
    pj_bzero(&stat, sizeof(stat));
    pj_test_get_stat(&suite, &stat);
    PJ_TEST_EQ(stat.ntests, 2, test_title, return -100);
    PJ_TEST_EQ(stat.nruns, 2, test_title, return -110);
    PJ_TEST_EQ(stat.nfailed, 1, test_title, return -120);
    PJ_TEST_EQ(stat.failed_names[0], test_case1.obj_name, test_title, return -130);
    PJ_TEST_EQ(strcmp(stat.failed_names[0], "failure test"), 0, 
               test_title, return -135);
    PJ_TEST_GTE( PJ_TIME_VAL_MSEC(stat.duration), 200, test_title, return -140);

    /* test logging.
     * Since gave the test cases buffer to store log messages, we can dump
     * the logs now and check the contents.
     */
    start_capture_log();
    /* Dumping all test logs. both test 0 and 1 must be present */
    pj_test_display_log_messages(&suite, PJ_TEST_ALL_TESTS);
    print_log_buffer(test_title);
    end_capture_log();
    if (log_size >= MSG_LEN) {
        /* We should only have space for the last log */
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Some error in func_to_test(1)"),
                         test_title, return -201);
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Performing func_to_test(0)"),
                         test_title, return -202);
    } else if (log_size < 10) {
        /* buffer is too small, writing log will be rejected */
        PJ_TEST_EQ(strstr(log_buffer, "Some error"), NULL,
                   test_title, return -203);
    } else {
        /* buffer is small, message will be truncated */
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Some error in"),
                         test_title, return -204);
    }

    if (log_size >= 2*MSG_LEN) {
        /* We should have space for two last log messages */
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Performing func_to_test(1)"),
                         test_title, return -205);
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Entering func_to_test(0)"),
                         test_title, return -206);
    }
    if (log_size >= 3*MSG_LEN) {
        /* We should have space for three last log messages */
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Entering func_to_test(1)"),
                         test_title, return -207);
    }

    /* Dumping only failed test. Only test 1 must be present */
    start_capture_log();
    pj_test_display_log_messages(&suite, PJ_TEST_FAILED_TESTS);
    print_log_buffer(test_title);
    end_capture_log();
    if (log_size >= MSG_LEN) {
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Some error in func_to_test(1)"),
                         test_title, return -211);
    } else if (log_size < 10) {
        /* buffer is too small, writing log will be rejected */
        PJ_TEST_EQ(strstr(log_buffer, "Some error"), NULL,
                   test_title, return -212);
    } else {
        /* buffer is small, message will be truncated */
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Some error in"),
                         test_title, return -213);
    }
    if (log_size >= 2*MSG_LEN) {
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Performing func_to_test(1)"),
                         test_title, return -214);
    }
    if (log_size >= 3*MSG_LEN) {
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Entering func_to_test(1)"),
                         test_title, return -215);
    }
    PJ_TEST_EQ(strstr(log_buffer, "Entering func_to_test(0)"), NULL,
               test_title, return -216);

    /* Dumping only successful test. Only test 0 must be present */
    start_capture_log();
    pj_test_display_log_messages(&suite, PJ_TEST_SUCCESSFUL_TESTS);
    print_log_buffer(test_title);
    end_capture_log();
    if (log_size >= MSG_LEN) {
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Performing func_to_test(0)"),
                         test_title, return -221);
    }
    if (log_size >= 2*MSG_LEN) {
        PJ_TEST_NOT_NULL(strstr(log_buffer, "Entering func_to_test(0)"),
                         test_title, return -222);
    }
    PJ_TEST_EQ(strstr(log_buffer, "Entering func_to_test(1)"), NULL,
               test_title, return -223);

    return 0;
}

static int log_msg_sizes[] = { 
    1*MSG_LEN, /* log buffer enough for 1 message */
    2*MSG_LEN, /* log buffer enough for 2 messages */
    3*MSG_LEN, /* log buffer enough for 3 message */
    0,         /* no log buffer */
    64,        /* log will be truncated */
};

int unittest_basic_test(void)
{
    int ret, log_level = pj_log_get_level();
    unsigned j;

    if (pj_test_is_under_test()) {
        PJ_LOG(1,(THIS_FILE, "Cannot run unittest_test under unit-test!"));
        return -1;
    }

    /* We wants to get detailed logging */
    pj_log_set_level(4);

    start_capture_log();
    ret = assertion_tests();
    end_capture_log();
    if (ret) goto on_return;

    for (j=0; j<PJ_ARRAY_SIZE(log_msg_sizes); ++j) {
        pj_bool_t parallel;
        for (parallel=0; parallel<2; ++parallel) {
            ret = usage_test(NULL, PJ_TRUE, parallel, log_msg_sizes[j]);
            if (ret) goto on_return;
        }
    }

on_return:
    pj_log_set_level(log_level);
    return ret;
}

int unittest_test(void)
{
    int ret, log_level = pj_log_get_level();
    unsigned j;

    if (pj_test_is_under_test()) {
        PJ_LOG(1,(THIS_FILE, "Cannot run unittest_test under unit-test!"));
        return -1;
    }

    /* We wants to get detailed logging */
    pj_log_set_level(4);

    for (j=0; j<PJ_ARRAY_SIZE(log_msg_sizes); ++j) {
        pj_bool_t parallel;
        for (parallel=0; parallel<2; ++parallel) {
            pj_pool_t *pool = pj_pool_create( mem, NULL, 4000, 4000, NULL);

            ret = usage_test(pool, PJ_FALSE, parallel, log_msg_sizes[j]);

            pj_pool_release(pool);

            if (ret) goto on_return;
        }
    }

on_return:
    pj_log_set_level(log_level);
    return ret;
}

#endif  /* INCLUDE_UNITTEST_TEST */

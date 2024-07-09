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
#ifndef __PJ_UNITTEST_H__
#define __PJ_UNITTEST_H__

/**
 * @file testing.h
 * @brief PJLIB unit testing framework
 */
/**
 * @defgroup PJ_UNITTEST Unit testing framework
 * @ingroup PJ_MISC
 * @{
 */
#include <pj/fifobuf.h>
#include <pj/list.h>
#include <pj/log.h>
#include <pj/pool.h>

PJ_BEGIN_DECL

/*
 * These various PJ_TEST_XXX macros can be used in any programs without
 * having to use the unit-test framework.
 */

/**
 * Check that an expression is non-zero. If the check fails, informative error
 * message will be displayed, and the code in err_action will be executed.
 * 
 * @param expr          The expression to check
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_NON_ZERO(expr, err_reason, err_action)  \
            { \
                if ((expr)==0) { \
                    const char *tmp_reason_ = err_reason; \
                    const char *sep0_ = (tmp_reason_ ? " (": ""); \
                    const char *sep1_ = (tmp_reason_ ? ")": ""); \
                    if (!tmp_reason_) tmp_reason_=""; \
                    PJ_LOG(1,(THIS_FILE, "Test \"%s\" != 0 fails in " \
                                         "%s:%d%s%s%s", \
                              #expr, THIS_FILE,__LINE__,sep0_, \
                              tmp_reason_,sep1_));\
                    err_action; \
                } \
            }

/**
 * Generic check for binary operation. If the check fails, informative error
 * message will be displayed, and the code in err_action will be executed.
 * 
 * @param expr0         First expression
 * @param op            The operator
 * @param expr1         Second expression
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_BINARY_OP(expr0, op, expr1, err_reason, err_action) \
            { \
                long tmp_value0_ = (long)(expr0); \
                long tmp_value1_ = (long)(expr1); \
                if (!(tmp_value0_ op tmp_value1_)) { \
                    const char *tmp_reason_ = err_reason; \
                    const char *sep0_ = (tmp_reason_ ? " (": ""); \
                    const char *sep1_ = (tmp_reason_ ? ")": ""); \
                    if (!tmp_reason_) tmp_reason_=""; \
                    PJ_LOG(1,(THIS_FILE, "Test \"%s\" (value=%ld) " #op \
                              " \"%s\" (value=%ld) fails in %s:%d%s%s%s", \
                              #expr0, tmp_value0_, #expr1, tmp_value1_, \
                              THIS_FILE, __LINE__, \
                              sep0_, tmp_reason_, sep1_)); \
                    err_action; \
                } \
            }

/**
 * Generic check for (PJ) string comparison operation. If the check fails,
 * informative error  message will be displayed, and the code in err_action
 * will be executed.
 * 
 * @param str_op        The string operation (e.g. pj_strcmp)
 * @param ps0           Pointer to first string
 * @param ps1           Pointer to second string
 * @param res_op        Operator to compare result (e.g. ==)
 * @param res           Expected return value of str_op(&s0, &s1)
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_STR_OP(str_op, ps0, ps1, res_op, res, err_reason, err_action) \
            { \
                int result__ = str_op(ps0, ps1); \
                if (!(result__ res_op res)) { \
                    const char *fn_name = #str_op; \
                    const char *tmp_reason_ = err_reason; \
                    const char *sep0_ = (tmp_reason_ ? " (": ""); \
                    const char *sep1_ = (tmp_reason_ ? ")": ""); \
                    if (!tmp_reason_) tmp_reason_=""; \
                    PJ_LOG(1,(THIS_FILE, "Test %s(\"%.*s\", \"%.*s\")%s%d" \
                              " fails (%s result=%d) in %s:%d%s%s%s", \
                              fn_name, (int)ps0->slen, ps0->ptr, \
                              (int)ps1->slen, ps1->ptr, #res_op, res, \
                              fn_name, result__, \
                              THIS_FILE, __LINE__, \
                              sep0_, tmp_reason_, sep1_)); \
                    err_action; \
                } \
            }

/**
 * Check that an expression is PJ_SUCCESS. If the check fails, error message
 * explaining the error code will be displayed, and the code in err_action
 * will be executed.
 * 
 * @param expr          The expression to check
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_SUCCESS(expr, err_reason, err_action) \
            { \
                pj_status_t tmp_status_ = (expr); \
                if (tmp_status_ != PJ_SUCCESS) { \
                    char errbuf[80]; \
                    const char *tmp_reason_ = err_reason; \
                    const char *sep0_ = (tmp_reason_ ? " (": ""); \
                    const char *sep1_ = (tmp_reason_ ? ")": ""); \
                    if (!tmp_reason_) tmp_reason_=""; \
                    pj_strerror(tmp_status_, errbuf, sizeof(errbuf)); \
                    PJ_LOG(1,(THIS_FILE, "\"%s\" fails in %s:%d, " \
                              "status=%d (%s)%s%s%s", \
                              #expr, THIS_FILE, __LINE__, tmp_status_,errbuf,\
                              sep0_, tmp_reason_, sep1_)); \
                    err_action; \
                } \
            }

/**
 * Alias for PJ_TEST_NON_ZERO()
 */
#define PJ_TEST_TRUE(expr, err_reason, err_action)  \
            PJ_TEST_NON_ZERO(expr, err_reason, err_action)

/**
 * Alias for PJ_TEST_NON_ZERO()
 */
#define PJ_TEST_NOT_NULL(expr, err_reason, err_action)  \
            PJ_TEST_NON_ZERO(expr, err_reason, err_action)

/**
 * Check that expr0 equals expr1.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param expr0         First expression
 * @param expr1         Second expression
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_EQ(expr0, expr1, err_reason, err_action)  \
            PJ_TEST_BINARY_OP(expr0, ==, expr1, err_reason, err_action)

/**
 * Check that expr0 does not equal expr1.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param expr0         First expression
 * @param expr1         Second expression
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_NEQ(expr0, expr1, err_reason, err_action)  \
            PJ_TEST_BINARY_OP(expr0, !=, expr1, err_reason, err_action)

/**
 * Check that expr0 is less than expr1.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param expr0         First expression
 * @param expr1         Second expression
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_LT(expr0, expr1, err_reason, err_action)  \
            PJ_TEST_BINARY_OP(expr0, <, expr1, err_reason, err_action)

/**
 * Check that expr0 is less than or equal to expr1.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param expr0         First expression
 * @param expr1         Second expression
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_LTE(expr0, expr1, err_reason, err_action)  \
            PJ_TEST_BINARY_OP(expr0, <=, expr1, err_reason, err_action)

/**
 * Check that expr0 is greater than expr1.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param expr0         First expression
 * @param expr1         Second expression
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_GT(expr0, expr1, err_reason, err_action)  \
            PJ_TEST_BINARY_OP(expr0, >, expr1, err_reason, err_action)

/**
 * Check that expr0 is greater than or equal to expr1.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param expr0         First expression
 * @param expr1         Second expression
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_GTE(expr0, expr1, err_reason, err_action)  \
            PJ_TEST_BINARY_OP(expr0, >=, expr1, err_reason, err_action)


/**
 * Check string comparison result.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param ps0           Pointer to first string
 * @param ps1           Pointer to second string
 * @param res_op        Operator to compare result (e.g. ==, <, >)
 * @param exp_result    Expected result (e.g. zero for equal string)
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_STRCMP(ps0, ps1, res_op, exp_result, err_reason, err_action) \
            PJ_TEST_STR_OP(pj_strcmp, ps0, ps1, res_op, exp_result, \
                           err_reason, err_action)

/**
 * Check case-insensitive string comparison result.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param ps0           Pointer to first string
 * @param ps1           Pointer to second string
 * @param res_op        Operator to compare result (e.g. ==, <, >)
 * @param exp_result    Expected result (e.g. zero for equal)
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_STRICMP(ps0, ps1, res_op, exp_result, err_reason, err_action) \
            PJ_TEST_STR_OP(pj_stricmp, ps0, ps1, res_op, exp_result, \
                           err_reason, err_action)

/**
 * Check that two strings are equal.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param ps0           Pointer to first string
 * @param ps1           Pointer to second string
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_STREQ(ps0, ps1, err_reason, err_action)  \
            PJ_TEST_STRCMP(ps0, ps1, ==, 0, err_reason, err_action)

/**
 * Check that two strings are not equal.
 * If the check fails, informative error message will be displayed and
 * the code in err_action will be executed.
 * 
 * @param ps0           Pointer to first string
 * @param ps1           Pointer to second string
 * @param err_reason    NULL or extra text to display when the check fails
 * @param err_action    Action to perform when the check fails
 */
#define PJ_TEST_STRNEQ(ps0, ps1, err_reason, err_action)  \
            PJ_TEST_STRCMP(ps0, ps1, !=, 0, err_reason, err_action)


/**
 * Bitwise constants that can be used in test case flags (see
 * pj_test_case_init()).
 */
typedef enum pj_test_case_flag
{
    /** 
     * Do not allow other test cases to run while this test case is running.
     * Note this only makes sense for test runners that support worker
     * threads. Basic runner will always run test cases serially.
     */
    PJ_TEST_EXCLUSIVE = 1,

    /** 
     * Specify that the test function must be called without argument.
     * This is mainly for backward compatibility with existing PJ test
     * functions which take no argument.
     */
    PJ_TEST_FUNC_NO_ARG = 2,

    /**
     * Write the original log at the time it is called instead of pooling
     * the logs to be printed after all tests finish.
     */
    PJ_TEST_LOG_NO_CACHE = 4,

    /**
     * Keep this test case in front of the list when shuffling the test
     * cases.
     */
    PJ_TEST_KEEP_FIRST = 8,

    /**
     * Keep this test case in last in the list when shuffling the test
     * cases.
     */
    PJ_TEST_KEEP_LAST = 16,

} pj_test_case_flag;


/**
 * An internal structure to represent one logging item that is saved
 * inside pj_test_case.
 */
typedef struct pj_test_log_item
{
    PJ_DECL_LIST_MEMBER(struct pj_test_log_item);

    /** level */
    int level;

    /** len */
    int len;

    /** The log message. The actual buffer is longer. */
    char msg[1];

} pj_test_log_item;


/** Forward declaration of test runner */
typedef struct pj_test_runner pj_test_runner;


/**
 * Additional parameters for creating test case. Use
 * pj_test_case_param_default() to initialize this structure.
 */
typedef struct pj_test_case_param
{
    /**
     * Custom log level for this test case, to filter out logs that are more
     * detail than this level. Default is 6, meaning it will accept all log
     * levels.
     */
    int log_level;

} pj_test_case_param;


/**
 * A test case is unit-test object to perform test against a user defined
 * function.
 */
typedef struct pj_test_case
{
    PJ_DECL_LIST_MEMBER(struct pj_test_case);

    /** Test name */
    char obj_name[PJ_MAX_OBJ_NAME];

    /** 
     * The test function to be called to perform the test. By convention, the
     * function must return zero for the test to be considered successful,
     * non-zero on failure, and MUST NEVER return PJ_EPENDING, otherwise the
     * return value will be silently changed to -12345.
     */
    int (*test_func)(void*);

    /** Argument to be passed to the test function */
    void *arg;

    /** Flags, combination of pj_test_flag constants  */
    unsigned flags;

    /** Circular buffer for logging */
    pj_fifobuf_t fb;

    /** Parameters */
    pj_test_case_param prm;

    /** 
     * The return value of the test function. Zero indicates success. Initially
     * the value is PJ_EPENDING before the test is run.
     */
    int result;

    /** List of saved logging messages */
    pj_test_log_item logs;

    /** Pointer to the runner running this test case */
    pj_test_runner *runner;

    /** Start time */
    pj_timestamp start_time;

    /** End time */
    pj_timestamp end_time;

} pj_test_case;


/**
 * Test suite is a collection of test cases.
 */
typedef struct pj_test_suite
{
    /** List of tests */
    pj_test_case tests;

    /** Start time */
    pj_timestamp start_time;

    /** End time */
    pj_timestamp end_time;

} pj_test_suite;


/**
 * Test statistics. Collect the statistics after the test runner finishes
 * with pj_test_get_stat().
 */
typedef struct pj_test_stat
{
    /** Total duration */
    pj_time_val duration;

    /** Total number of tests in the test suite */
    unsigned ntests;

    /** Number of tests run */
    unsigned nruns;
    
    /** Number of failed tests */
    unsigned nfailed;

    /**
     * Array of failed test names. Be careful that the number elements are
     * fixed, hence it may not be able to store all failed test names (in case
     * nfailed is more than the capacity, not all failed test names will be
     * stored, hence be careful in the loop).
     */
    const char *failed_names[32];

} pj_test_stat;


/**
 * Test runner parameters. Use pj_test_runner_param_default() to initialize
 * this structure.
 */
typedef struct pj_test_runner_param
{
    /** Stop the test on error (default: false) */
    pj_bool_t stop_on_error;

    /** Number of worker threads. Set to zero to disable parallel testings.
     * Only applicable to test text runner. Default is 1 if multithreading
     * is available.
     */
    unsigned nthreads;

    /**
     * 0: only display test name and result after test completion (default)
     * 1: display test name test when starting and finishing a test
     */
    unsigned verbosity;

} pj_test_runner_param;


/** 
 * This structure represents a test runner. Currently there are two types
 * of test runners, the basic runner and text runner. The basic runner is the
 * simplest test runner that can be used without pool and threads, and can be
 * created with pj_test_init_basic_runner(). The text runner is more powerful
 * since it supports worker threads, and it is mostly suitable for console
 * based environments. It is created with pj_test_create_text_runner().
 */
struct pj_test_runner
{
    /** Parameters */
    pj_test_runner_param prm;

    /** The test suite being run */
    pj_test_suite *suite;

    /** Saving the original log writer */
    pj_log_func   *orig_log_writer;

    /** Number of tests */
    unsigned ntests;

    /** Number of completed tests */
    unsigned nruns;

    /** Stopping */
    pj_bool_t stopping;

    /** main method */
    void (*main)(pj_test_runner*);

    /** callback when test case completes. Default is to write to log */
    void (*on_test_complete)(pj_test_runner*, pj_test_case*);

    /** destroy method */
    void (*destroy)(pj_test_runner*);
};

/** Option to select tests (e.g. in pj_test_dump_log_messages()) */
typedef enum pj_test_select_tests
{
    /** Select no test*/
    PJ_TEST_NO_TEST = 0,

    /** Select only failed tests */
    PJ_TEST_FAILED_TESTS = 1,

    /** Select only successful tests */
    PJ_TEST_SUCCESSFUL_TESTS = 2,

    /** Select all tests*/
    PJ_TEST_ALL_TESTS = 3,

    /** No header/footer separator */
    PJ_TEST_NO_HEADER_FOOTER = 4,
    
} pj_test_select_tests;


/**
 * Initialize test suite.
 * 
 * @param suite         The test suite
 */
PJ_DECL(void) pj_test_suite_init(pj_test_suite *suite);

/**
 * Initialize pj_test_case_param with default values. If app only uses
 * default values in params, alternatively it doesn't need to use param
 * at all and just specify NULL in pj_test_case_init().
 * 
 * @param prm       The parameter.
 */
PJ_DECL(void) pj_test_case_param_default(pj_test_case_param *prm);

/**
 * Initialize test case.
 * 
 * @param tc            The test case
 * @param obj_name      Name that will appear as test name/title
 * @param flags         Bitwise of pj_test_case_flag to control threading,
 *                      function calling, logging, etc.
 * @param test_func     The test function to be called to perform the test.
 *                      By convention, the function must return zero for the
 *                      test to be considered successful, non-zero on failure,
 *                      and MUST NEVER return PJ_EPENDING, otherwise the
 *                      return value will be silently changed to -12345.
 * @param arg           Argument to give to the test function
 * @param fifobuf_buf   Buffer for saving the logs, if required.
 * @param buf_size      Size of the buffer for saving the logs.
 * @param prm           Optional additional settings for the test case or NULL
 */
PJ_DECL(void) pj_test_case_init(pj_test_case *tc,
                                const char *obj_name, 
                                unsigned flags,
                                int (*test_func)(void*),
                                void *arg,
                                void *fifobuf_buf,
                                unsigned buf_size,
                                const pj_test_case_param *prm);

/**
 * Add test case to test suite. A test case can only be added to one suite.
 * 
 * @param suite         The test suite
 * @param tc            The test case
 */
PJ_DECL(void) pj_test_suite_add_case(pj_test_suite *suite, pj_test_case *tc);

/**
 * Shuffle the tests.
 * 
 * @param suite         The test suite
 * @param seed          Optional random seed to use, only if the value is
 *                      greater than or equal to zero. It is recommended
 *                      to set this value to make the test reproducible.
 */
PJ_DECL(void) pj_test_suite_shuffle(pj_test_suite *suite, int seed);

/**
 * Initialize parameters with reasonable default values. This usually means
 * using one worker thread if threading is enabled, and zero worker thread
 * (i.e. only use the main thread) otherwise.
 * 
 * @param prm           Test runner parameter
 */
PJ_DECL(void) pj_test_runner_param_default(pj_test_runner_param *prm);

/**
 * Initialize a basic test runner. A basic runner can be declared in the stack
 * and it does not require pool nor multithreading.
 * 
 * @param runner        The runner.
 * @param prm           Runner params, or NULL to accept default values.
 */
PJ_DECL(void) pj_test_init_basic_runner(pj_test_runner *runner,
                                        const pj_test_runner_param *prm);

/**
 * Create console based test runner.
 * 
 * @param pool          The pool to use to allocate memory
 * @param prm           Test runner parameter, or NULL for default values.
 * @param p_runner      Pointer to receive the text runner
 * 
 * @return PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_test_create_text_runner(
                            pj_pool_t *pool, 
                            const pj_test_runner_param *prm,
                            pj_test_runner **p_runner);

/**
 * Run test suite with the specified runner.
 * 
 * @param runner        The test runner
 * @param suite         The test suite
 */
PJ_DECL(void) pj_test_run(pj_test_runner *runner, 
                          pj_test_suite *suite);

/**
 * This is a crude test to detect if thread is currently running under
 * a test. It is mainly used to prevent nested unit testing.
 * 
 * @return PJ_TRUE if we are currently running in the context of a test case
 *         being run.
 */
PJ_DECL(pj_bool_t) pj_test_is_under_test(void);

/**
 * Get the test statistics after the run completes. The test suite and
 * test cases instances must be kept alive in order to get and access the
 * statistics or log messages.
 * 
 * @param suite         The test suite
 * @param stat          The test statistics result.
 */
PJ_DECL(void) pj_test_get_stat(const pj_test_suite *suite, pj_test_stat *stat);

/**
 * Display statistics to the log.
 * 
 * @param stat          The test statistics result.
 */
PJ_DECL(void) pj_test_display_stat(const pj_test_stat *stat, 
                                   const char *test_name,
                                   const char *log_sender);

/**
 * Display previously saved log messages in the test cases to logging.
 * Note that log messages emited during test case's run are only saved
 * when fifobuf of the test case is configured with a suitable buffer.
 * Also note that the test suite and test cases instances must be kept alive
 * in order to get and access the statistics or log messages.
 * 
 * @param suite         The test suite
 * @param flags         Select which test logs to display by choosing
 *                      from pj_test_select_tests.
 */
PJ_DECL(void) pj_test_display_log_messages(const pj_test_suite *suite,
                                           unsigned flags);

/**
 * Destroy the runner. Runner may be destroyed right after it is run,
 * but the test suite and test cases instances must be kept alive in order
 * to get the statistics or log messages.
 * 
 * @param runner        The test runner.
 */
PJ_DECL(void) pj_test_runner_destroy(pj_test_runner *runner);


/**
 * Macro to control how long worker thread should sleep waiting for next
 * ready test.
 */
#ifndef PJ_TEST_THREAD_WAIT_MSEC
#  define PJ_TEST_THREAD_WAIT_MSEC  100
#endif

PJ_END_DECL

/**
 * @}   // PJ_UNITTEST group
 */

#endif /* __PJ_UNITTEST_H__ */


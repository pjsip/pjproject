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
#include <pj/unittest.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/fifobuf.h>
#include <pj/os.h>
#include <pj/string.h>

#define THIS_FILE       "unittest.c"
#define INVALID_TLS_ID  -1

static long tls_id = INVALID_TLS_ID;
static pj_test_case *tc_main_thread;

/* Forward decls. */
static void unittest_log_callback(int level, const char *data, int len);
static int get_completion_line( const pj_test_case *tc, const char *end_line,
                                char *log_buf, unsigned buf_size);

/* atexit() callback to free TLS */
static void unittest_shutdown(void)
{
    if (tls_id != INVALID_TLS_ID) {
        pj_thread_local_free(tls_id);
        tls_id = INVALID_TLS_ID;
    }
}

/* initialize unittest subsystem. can be called many times. */
static pj_status_t unittest_init(void)
{
#if PJ_HAS_THREADS
    if (tls_id == INVALID_TLS_ID) {
        pj_status_t status;
        status = pj_thread_local_alloc(&tls_id);
        if (status != PJ_SUCCESS) {
            tls_id = INVALID_TLS_ID;
            return status;
        }

        pj_atexit(&unittest_shutdown);
    }
#endif
    return PJ_SUCCESS;
}

/* Initialize param with default values */
PJ_DEF(void) pj_test_case_param_default( pj_test_case_param *prm)
{
    pj_bzero(prm, sizeof(*prm));
    prm->log_level = 6;
}

/* Initialize test case */
PJ_DEF(void) pj_test_case_init( pj_test_case *tc,
                                const char *obj_name, 
                                unsigned flags,
                                int (*test_func)(void*),
                                void *arg,
                                void *fifobuf_buf,
                                unsigned buf_size,
                                const pj_test_case_param *prm)
{
    pj_bzero(tc, sizeof(*tc));

    /* Parameters */
    if (prm) {
        pj_memcpy(&tc->prm, prm, sizeof(*prm));
    } else {
        pj_test_case_param_default(&tc->prm);
    }
    pj_ansi_strxcpy(tc->obj_name, obj_name, sizeof(tc->obj_name));
    tc->flags = flags;
    tc->test_func = test_func;
    tc->arg = arg;
    pj_fifobuf_init(&tc->fb, fifobuf_buf, buf_size);

    /* Run-time state */
    tc->result = PJ_EPENDING;
    pj_list_init(&tc->logs);
}

/* Init test suite */
PJ_DEF(void) pj_test_suite_init(pj_test_suite *suite)
{
    pj_bzero(suite, sizeof(*suite));
    pj_list_init(&suite->tests);
}

/* Add test case */
PJ_DEF(void) pj_test_suite_add_case(pj_test_suite *suite, pj_test_case *tc)
{
    pj_list_push_back(&suite->tests, tc);
}

/* Initialize text runner param with default values */
PJ_DEF(void) pj_test_text_runner_param_default(
                pj_test_text_runner_param *prm)
{
    pj_bzero(prm, sizeof(*prm));
#if PJ_HAS_THREADS
    prm->nthreads = 1;
#endif
}

/* Main API to start running a test runner */
PJ_DEF(void) pj_test_run(pj_test_runner *runner, pj_test_suite *suite)
{
    unsigned index, total;
    pj_test_case *tc;

    /* Redirect logging to our custom callback */
    runner->orig_log_writer = pj_log_get_log_func();
    pj_log_set_log_func(&unittest_log_callback);

    /* Initialize suite and test cases */
    runner->suite = suite;
    total = pj_list_size(&suite->tests);
    for (tc=suite->tests.next, index=0; tc!=&suite->tests; 
         tc=tc->next, ++index) 
    {
        tc->result = PJ_EPENDING;
        tc->runner = runner;
        tc->index = index;
        tc->total = total;
    }

    /* Call the run method to perform runner specific loop */
    pj_get_timestamp(&suite->start_time);
    runner->main(runner);
    pj_get_timestamp(&suite->end_time);

    /* Restore logging */
    pj_log_set_log_func(runner->orig_log_writer);
}

/* Calculate statistics */
PJ_DEF(void) pj_test_get_stat( const pj_test_suite *suite, pj_test_stat *stat)
{
    const pj_test_case *tc;

    pj_bzero(stat, sizeof(*stat));
    stat->duration = pj_elapsed_time(&suite->start_time, &suite->end_time);
    stat->ntests = pj_list_size(&suite->tests);

    for (tc=suite->tests.next; tc!=&suite->tests; tc=tc->next) {
        if (tc->result != PJ_EPENDING) {
            stat->nruns++;
            if (tc->result != PJ_SUCCESS) {
                if (stat->nfailed < PJ_ARRAY_SIZE(stat->failed_names)) {
                    stat->failed_names[stat->nfailed] = tc->obj_name;
                }
                stat->nfailed++;
            }
        }
    }
}

/* Dump previously saved log messages */
PJ_DEF(void) pj_test_dump_log_messages( const pj_test_suite *suite,
                                        pj_test_select_tests which)
{
    const pj_test_case *tc = suite->tests.next;
    pj_log_func *log_writer = pj_log_get_log_func();
    char test_line[80];

    while (tc != &suite->tests) {
        const pj_test_log_item *log_item = tc->logs.next;

        if ((tc->result == PJ_EPENDING) ||
            (which==PJ_TEST_FAILED_TESTS && tc->result==0) ||
            (which==PJ_TEST_SUCCESSFUL_TESTS && tc->result!=0))
        {
            /* Test doesn't meet criteria */
            tc = tc->next;
            continue;
        }

        if (log_item != &tc->logs) {
            get_completion_line(tc, " logs:", test_line, sizeof(test_line));
            log_writer(3, test_line, pj_ansi_strlen(test_line));

            do {
                log_writer(log_item->level, log_item->msg, log_item->len);
                log_item = log_item->next;
            } while (log_item != &tc->logs);
        }
        tc = tc->next;
    }
}

/* Destroy runner */
PJ_DEF(void) pj_test_runner_destroy(pj_test_runner *runner)
{
    runner->destroy(runner);
}

/**************************** Common for runners ****************************/

/* Set the current test case being run by a thread. The logging callback
 * needs this info.
 */
static void set_current_test_case(pj_test_case *tc)
{
    if (tls_id == INVALID_TLS_ID)
        tc_main_thread = tc;
    else
        pj_thread_local_set(tls_id, tc);
}


/* Get the current test case being run by a thread. The logging callback
 * needs this info.
 */
static pj_test_case *get_current_test_case()
{
    if (tls_id == INVALID_TLS_ID)
        return tc_main_thread;
    else
        return (pj_test_case*) pj_thread_local_get(tls_id);
}

/* Logging callback */
static void unittest_log_callback(int level, const char *data, int len)
{
    pj_test_case *tc = get_current_test_case();
    unsigned req_size, free_size;
    pj_bool_t truncated;
    pj_test_log_item *log_item;

    PJ_ASSERT_ON_FAIL(len > 0, return);
    PJ_ASSERT_ON_FAIL(tc, return);

    /* Filter out unwanted log */
    if (level > tc->prm.log_level)
        return;

    /* If the test case wants to display the original log as they are called,
     * then write it using the original logging writer now.
     */
    if (tc->flags & PJ_TEST_ORIGINAL_LOG) {
        tc->runner->orig_log_writer(level, data, len);
        return;
    }

    /* If fifobuf is not configured on this test case, there's nothing
     * we can do. We assume tester doesn't want logging.
     */
    if (pj_fifobuf_capacity(&tc->fb)==0)
        return;
    
    /* Required size is the message length plus sizeof(pj_test_log_item).
     * This should be enough to save the message INCLUDING the null
     * character (because of msg[1] in pj_test_log_item)
     */
    req_size = len + sizeof(pj_test_log_item);

    /* Free the buffer until it's enough to save the message. */
    while ((free_size = pj_fifobuf_available_size(&tc->fb)) < req_size &&
           !pj_list_empty(&tc->logs)) 
    {
        pj_test_log_item *first = tc->logs.next;

        /* Free the oldest */
        pj_list_erase(first);
        pj_fifobuf_free(&tc->fb, first);
    }

    if (free_size < sizeof(pj_test_log_item) + 10) {
        /* Tester has set the fifobuf's size too small */
        return;
    }

    if (free_size < req_size) {
        /* Truncate message */
        len = free_size - sizeof(pj_test_log_item);
        req_size = free_size;
        truncated = PJ_TRUE;
    } else {
        truncated = PJ_FALSE;
    }

    log_item = (pj_test_log_item*)pj_fifobuf_alloc(&tc->fb, req_size);
    PJ_ASSERT_ON_FAIL(log_item, return);
    log_item->level = level;
    log_item->len = len;
    pj_memcpy(log_item->msg, data, len+1);
    if (truncated)
        log_item->msg[len-1] = '\n';
    pj_list_push_back(&tc->logs, log_item);
}

/* Create test case completion line, i.e. the one that looks like:
 *  [2/24] pool_test             [OK]
 */
static int get_completion_line( const pj_test_case *tc, const char *end_line,
                                char *log_buf, unsigned buf_size)
{
    char res_buf[20];
    int log_len;

    if (tc->result==0) {
        pj_ansi_strxcpy(res_buf, "OK", sizeof(res_buf));
    } else if (tc->result==PJ_EPENDING) {
        pj_ansi_strxcpy(res_buf, "pending", sizeof(res_buf));
    } else {
        pj_ansi_snprintf(res_buf, sizeof(res_buf), "Err: %d", tc->result);    
    }

    log_len = pj_ansi_snprintf(log_buf, buf_size, "[%d/%d] %-32s [%s]%s\n",
                               tc->index+1, tc->total, tc->obj_name, res_buf,
                               end_line);

    if (log_len < 1 || log_len >= sizeof(log_buf))
        log_len = pj_ansi_strlen(log_buf);
        
    return log_len;
}

/* Default runner's callback when a test case completes. This must be
 * reentrant from multiple threads
 */
static void on_test_complete(pj_test_runner *runner, pj_test_case *tc)
{
    char line[80];
    int len;

    len = get_completion_line(tc, "", line, sizeof(line));
    tc->runner->orig_log_writer(3, line, len);
}

/* This is the main function to run a single test case. It may
 * be used by the basic runner, which has no threads (=no TLS),
 * no fifobuf, no pool, or by multiple threads.
 */
static void run_test_case(pj_test_runner *runner, pj_test_case *tc)
{
    /* Set the test case being worked on by this thread */
    set_current_test_case(tc);

    /* Call the test case's function */
    if (tc->flags & PJ_TEST_FUNC_NO_ARG) {
        /* Function without argument */
        typedef int (*func_t)(void);
        func_t func = (func_t)tc->test_func;
        tc->result = func();
    } else {
        tc->result = tc->test_func(tc->arg);
    }

    if (tc->result == PJ_EPENDING)
        tc->result = -12345;

    runner->on_test_complete(runner, tc);

    /* Reset the test case being worked on by this thread */
    set_current_test_case(NULL);
}

/******************************* Basic Runner *******************************/

/* This is the "main()" function for basic runner. It just runs the tests
 * sequentially
 */
static void basic_runner_main(pj_test_runner *runner)
{
    pj_test_case *tc;
    for (tc = runner->suite->tests.next; 
         tc != &runner->suite->tests; 
         tc = tc->next)
    {
        run_test_case(runner, tc);
    }
}

/* Destroy for basic runner */
static void basic_runner_destroy(pj_test_runner *runner)
{
    /* Nothing to do for basic runner */
}

/* Initialize a basic runner. */
PJ_DEF(void) pj_test_init_basic_runner(pj_test_runner *runner)
{
    pj_bzero(runner, sizeof(*runner));
    runner->main = &basic_runner_main;
    runner->destroy = &basic_runner_destroy;
    runner->on_test_complete = &on_test_complete;
}

/******************************* Text Runner *******************************/

typedef struct text_runner_t
{
    pj_test_runner              base;
    pj_test_text_runner_param   prm;

    pj_test_case               *cur_case;
    pj_mutex_t                 *mutex;
    pj_thread_t               **threads;
} text_runner_t;

/* This is called by thread(s) to get the next test case to run.
 * The rule is:
 *  1. if current test case (tc) doesn't have PJ_TEST_PARALLEL flag, then we
 *     cannot get next tc until that tc completes
 *  2. if tc has PJ_TEST_PARALLEL flag, free to return next tc, until we
 *     meet tc with no PJ_TEST_PARALLEL flag, in this case that tc is
 *     returned and we'll have rule 1.
 * 
 *  Returns:
 *   - PJ_SUCCESS if we successfully returns next tc
 *   - PJ_EPENDING if there is tc left but we must wait for completion of
 *     non-parallel tc
 *   - PJ_ENOTFOUND if there is no further tests, which in this case the
 *     thread can exit.
 *   - other error is not anticipated, and will cause thread to exit.
 */
static pj_status_t text_runner_get_next_test_case(text_runner_t *runner,
                                                  pj_test_case **p_test_case)
{
    pj_status_t status;

    *p_test_case = NULL;
    pj_mutex_lock(runner->mutex);

    if (runner->cur_case == NULL) {
        if (pj_list_empty(&runner->base.suite->tests)) {
            status = PJ_ENOTFOUND;
            goto on_return;
        }
        runner->cur_case = *p_test_case = runner->base.suite->tests.next;
        status = PJ_SUCCESS;
    } else if (runner->cur_case == &runner->base.suite->tests) {
        /* All done */
        status = PJ_ENOTFOUND;
    } else {
        if (runner->cur_case->result == PJ_EPENDING) {
            /* Test is still running. */
            if (runner->cur_case->flags & PJ_TEST_PARALLEL) {
                /* Allow other test to run */
                runner->cur_case = runner->cur_case->next;
                if (runner->cur_case == &runner->base.suite->tests) {
                    status = PJ_ENOTFOUND;
                } else {
                    *p_test_case = runner->cur_case;
                    status = PJ_SUCCESS;
                }
            } else {
                if (runner->cur_case->next == &runner->base.suite->tests) {
                    /* The current case is the last one. The calling thread
                     * can quit now.
                     */
                    status = PJ_ENOTFOUND;
                } else {
                    /* Current test case does not allow parallel run */
                    status = PJ_EPENDING;
                }
            }
        } else {
            /* Current test is done, get next text */
            runner->cur_case = runner->cur_case->next;
            if (runner->cur_case == &runner->base.suite->tests) {
                status = PJ_ENOTFOUND;
            } else {
                *p_test_case = runner->cur_case;
                status = PJ_SUCCESS;
            }
        }
    }

on_return:
    pj_mutex_unlock(runner->mutex);
    return status;
}

/* Thread loop */
static int text_runner_thread_proc(void *arg)
{
    text_runner_t *runner = (text_runner_t*)arg;

    for (;;) {
        pj_test_case *tc;
        pj_status_t status;

        status = text_runner_get_next_test_case(runner, &tc);
        if (status==PJ_SUCCESS) {
            run_test_case(&runner->base, tc);
        } else if (status==PJ_EPENDING) {
            /* Yeah sleep, but the "correct" solution is probably an order of
             * magnitute more complicated, so this is good I think.
             */
            pj_thread_sleep(250);
        } else {
            break;
        }
    }
    return 0;
}

/* This is the "main()" function for text runner. */
static void text_runner_main(pj_test_runner *base)
{
    text_runner_t *runner = (text_runner_t*)base;
    unsigned i;

    for (i=0; i<runner->prm.nthreads; ++i) {
        pj_thread_resume(runner->threads[i]);
    }

    /* The main thread behaves like another worker thread */
    text_runner_thread_proc(base);

    for (i=0; i<runner->prm.nthreads; ++i) {
        pj_thread_join(runner->threads[i]);
    }
}

/* text runner destructor */
static void text_runner_destroy(pj_test_runner *base)
{
    text_runner_t *runner = (text_runner_t*)base;
    unsigned i;

    for (i=0; i<runner->prm.nthreads; ++i) {
        pj_thread_destroy(runner->threads[i]);
    }
    if (runner->mutex)
        pj_mutex_destroy(runner->mutex);

}

/* Create text runner */
PJ_DEF(pj_status_t) pj_test_create_text_runner(
                            pj_pool_t *pool, 
                            const pj_test_text_runner_param *prm,
                            pj_test_runner **p_runner)
{
    text_runner_t *runner;
    unsigned i;
    pj_status_t status;

    *p_runner = NULL;

    status = unittest_init();
    if (status != PJ_SUCCESS)
        return status;

    runner = PJ_POOL_ZALLOC_T(pool, text_runner_t);
    runner->base.main = text_runner_main;
    runner->base.destroy = text_runner_destroy;
    runner->base.on_test_complete = &on_test_complete;

    status = pj_mutex_create(pool, "unittest%p", PJ_MUTEX_RECURSE,
                             &runner->mutex);
    if (status != PJ_SUCCESS)
        goto on_error;

    if (prm) {
        pj_memcpy(&runner->prm, prm, sizeof(*prm));
    } else {
        pj_test_text_runner_param_default(&runner->prm);
    }
    runner->prm.nthreads = 0;
    runner->threads = (pj_thread_t**) pj_pool_calloc(pool, prm->nthreads,
                                                     sizeof(pj_thread_t*));
    for (i=0; i<prm->nthreads; ++i) {
        status = pj_thread_create(pool, "unittest%p", 
                                  text_runner_thread_proc, runner,
                                  0, PJ_THREAD_SUSPENDED, 
                                  &runner->threads[i]);
        if (status != PJ_SUCCESS)
            goto on_error;
        runner->prm.nthreads++;
    }

    *p_runner = (pj_test_runner*)runner;    
    return PJ_SUCCESS;

on_error:
    text_runner_destroy(&runner->base);
    return status;
}


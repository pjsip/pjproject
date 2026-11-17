/*
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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

/**
 * \page page_pjlib_thread_test Test: Thread Test
 *
 * This file contains \a thread_test() definition.
 *
 * \section thread_test_scope_sec Scope of Test
 * This tests:
 *  - whether PJ_THREAD_SUSPENDED flag works.
 *  - whether multithreading works.
 *  - whether thread timeslicing works, and threads have equal
 *    time-slice proportion.
 *  - whether thread-local storage is private to each thread.
 *
 * APIs tested:
 *  - pj_thread_create()
 *  - pj_thread_register()
 *  - pj_thread_this()
 *  - pj_thread_get_name()
 *  - pj_thread_destroy()
 *  - pj_thread_resume()
 *  - pj_thread_sleep()
 *  - pj_thread_join()
 *  - pj_thread_destroy()
 *
 *
 * This file is <b>pjlib-test/thread.c</b>
 *
 * \include pjlib-test/thread.c
 */
#if INCLUDE_THREAD_TEST

#include <pjlib.h>

#define THIS_FILE   "thread_test"

typedef unsigned long counter_t;
#define counter_fmt "%lu"
#if PJTEST_TIME_PASSES_WHEN_BUSY
static volatile int quit_flag=0;
static volatile int unregister_test_flag=0;

#if 0
#   define TRACE__(args)        PJ_LOG(3,args)
#else
#   define TRACE__(args)
#endif


/*
 * The thread's entry point.
 *
 * Each of the thread mainly will just execute the loop which
 * increments a variable.
 */
static int thread_proc(void *data)
{
    /* Test that pj_thread_register() works. */
    pj_thread_desc desc;
    pj_thread_t *this_thread, *initial_thread_desc;
    unsigned id;
    pj_status_t rc;
    counter_t *pcounter = (counter_t *)data;

    id = *pcounter;
    PJ_UNUSED_ARG(id); /* Warning about unused var if TRACE__ is disabled */
    TRACE__((THIS_FILE, "     thread %d running..", id));

    if (!pj_thread_is_registered()) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_is_registered() returns FALSE!"));
        return -1;
    }

    initial_thread_desc = pj_thread_this();
    if (initial_thread_desc == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: initial_thread_desc = pj_thread_this() returns NULL!"));
        return -1;
    }

    pj_bzero(desc, sizeof(desc));

    //error in test logic: re-registering an already registered thread
    rc = pj_thread_register("thread", desc, &this_thread);
    if (rc != PJ_SUCCESS) {
        app_perror("...error in pj_thread_register", rc);
        return rc;
    }

    /* Test that pj_thread_this() works */
    this_thread = pj_thread_this();
    if (this_thread == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_this() returns NULL!"));
        return -1;
    }

    /* Test that pj_thread_get_name() works */
    if (pj_thread_get_name(this_thread) == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_get_name() returns NULL!"));
        return -1;
    }

    /* reregister on the initial thread descriptor to check 
     * reregistered descriptor is joinable
     */
    for (; unregister_test_flag; --unregister_test_flag) {
        rc = pj_thread_unregister();
        if (rc != PJ_SUCCESS) {
            app_perror("...error in pj_thread_unregister", rc);
            unregister_test_flag = 0;
            return rc;
        }

        rc = pj_thread_attach("thread", 
                              (void*)initial_thread_desc, 
                              &this_thread);
        if (rc != PJ_SUCCESS) {
            app_perror("...error in pj_thread_attach", rc);
            unregister_test_flag = 0;
            return rc;
        }
    }

    /* Main loop */
    for (;!quit_flag;) {
        (*pcounter)++;
        //Must sleep if platform doesn't do time-slicing.
        //2024-12-18: always sleep since otherwise test may occasionaly throw
        //            error on Linux (bennylp)
        pj_thread_sleep(0);
    }

    TRACE__((THIS_FILE, "     thread %d quitting..", id));
    return PJ_SUCCESS;
}

/*
 * simple_thread()
 */
static int simple_thread(const char *title, unsigned flags)
{
    pj_pool_t *pool;
    pj_thread_t *thread;
    pj_status_t rc;
    counter_t counter = 0;
    enum { MAX_REREGISTER = 10 };

    PJ_LOG(3,(THIS_FILE, "..%s", title));

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
        return -1000;

    quit_flag = 0;

    unregister_test_flag = flags ? 0 : MAX_REREGISTER;

    TRACE__((THIS_FILE, "    Creating thread 0.."));
    rc = pj_thread_create(pool, "thread", (pj_thread_proc*)&thread_proc,
                          &counter,
                          PJ_THREAD_DEFAULT_STACK_SIZE,
                          flags,
                          &thread);

    if (rc != PJ_SUCCESS) {
        app_perror("...error: unable to create thread", rc);
        return -1010;
    }

    TRACE__((THIS_FILE, "    Main thread waiting.."));
    pj_thread_sleep(1500);
    TRACE__((THIS_FILE, "    Main thread resuming.."));

    if (flags & PJ_THREAD_SUSPENDED) {

        /* Check that counter is still zero */
        if (counter != 0) {
            PJ_LOG(3,(THIS_FILE, "...error: thread is not suspended"));
            return -1015;
        }

        rc = pj_thread_resume(thread);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: resume thread error", rc);
            return -1020;
        }
    }

    PJ_LOG(3,(THIS_FILE, "..waiting for thread to quit.."));

    pj_thread_sleep(1500);

    quit_flag = 1;
    while (unregister_test_flag) {
        pj_thread_sleep(0);
    }

    rc = pj_thread_join(thread);
    if (rc != PJ_SUCCESS) {
        app_perror("...error: unable to join thread", rc);
        return -1030;
    }

    pj_pool_release(pool);

    if (counter == 0) {
        PJ_LOG(3,(THIS_FILE, "...error: thread is not running"));
        return -1025;
    }

    PJ_LOG(3,(THIS_FILE, "...%s success", title));
    return PJ_SUCCESS;
}


/*
 * timeslice_test()
 */
static int timeslice_test(void)
{
    enum { NUM_THREADS = 4 };
    pj_pool_t *pool;
    counter_t counter[NUM_THREADS], lowest, highest, diff;
    pj_thread_t *thread[NUM_THREADS];
    unsigned i;
    pj_status_t rc;

    quit_flag = 0;
    unregister_test_flag = 0;

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
        return -10;

    PJ_LOG(3,(THIS_FILE, "..timeslice testing with %d threads", NUM_THREADS));

    /* Create all threads in suspended mode. */
    for (i=0; i<NUM_THREADS; ++i) {
        counter[i] = i;
        rc = pj_thread_create(pool, "thread", (pj_thread_proc*)&thread_proc,
                              &counter[i],
                              PJ_THREAD_DEFAULT_STACK_SIZE,
                              PJ_THREAD_SUSPENDED,
                              &thread[i]);
        if (rc!=PJ_SUCCESS) {
            app_perror("...ERROR in pj_thread_create()", rc);
            return -20;
        }
    }

    /* Sleep for 1 second.
     * The purpose of this is to test whether all threads are suspended.
     */
    TRACE__((THIS_FILE, "    Main thread waiting.."));
    pj_thread_sleep(1000);
    TRACE__((THIS_FILE, "    Main thread resuming.."));

    /* Check that all counters are still zero. */
    for (i=0; i<NUM_THREADS; ++i) {
        if (counter[i] > i) {
            PJ_LOG(3,(THIS_FILE, "....ERROR! Thread %d-th is not suspended!",
                      i));
            return -30;
        }
    }

    /* Now resume all threads. */
    for (i=0; i<NUM_THREADS; ++i) {
        TRACE__((THIS_FILE, "    Resuming thread %d [%p]..", i, thread[i]));
        rc = pj_thread_resume(thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...ERROR in pj_thread_resume()", rc);
            return -40;
        }
    }

    /* Main thread sleeps for some time to allow threads to run.
     * The longer we sleep, the more accurate the calculation will be,
     * but it'll make user waits for longer for the test to finish.
     */
    TRACE__((THIS_FILE, "    Main thread waiting (5s).."));
    pj_thread_sleep(5000);
    TRACE__((THIS_FILE, "    Main thread resuming.."));

    /* Signal all threads to quit. */
    quit_flag = 1;

    /* Wait until all threads quit, then destroy. */
    for (i=0; i<NUM_THREADS; ++i) {
        TRACE__((THIS_FILE, "    Main thread joining thread %d [%p]..",
                            i, thread[i]));
        rc = pj_thread_join(thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...ERROR in pj_thread_join()", rc);
            return -50;
        }
        TRACE__((THIS_FILE, "    Destroying thread %d [%p]..", i, thread[i]));
        rc = pj_thread_destroy(thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...ERROR in pj_thread_destroy()", rc);
            return -60;
        }
    }

    TRACE__((THIS_FILE, "    Main thread calculating time slices.."));

    /* Now examine the value of the counters.
     * Check that all threads had equal proportion of processing.
     */
    lowest = highest = counter[0];
    /* Check that all threads are running. */
    if (counter[0] == 0) {
        PJ_LOG(3,(THIS_FILE, "....ERROR! Thread 0 was not running!"));
        return -70;
    }
    for (i=1; i<NUM_THREADS; ++i) {

        /* Check that all threads are running. */
        if (counter[i] == i) {
            PJ_LOG(3,(THIS_FILE, "....ERROR! Thread %d-th was not running!",
                      i));
            return -75;
        }

        if (counter[i] < lowest)
            lowest = counter[i];
        if (counter[i] > highest)
            highest = counter[i];
    }

    /* The difference between lowest and higest should be lower than 50%.
     */
    diff = (highest-lowest)*100 / ((highest+lowest)/2);
    if ( diff >= 50) {
        PJ_LOG(3,(THIS_FILE,
                  "...ERROR: thread didn't have equal timeslice!"));
        PJ_LOG(3,(THIS_FILE,
                  ".....lowest counter=" counter_fmt
                  ", highest counter=" counter_fmt
                  ", diff=" counter_fmt "%%",
                  lowest, highest, diff));
        return -80;
    } else {
        PJ_LOG(3,(THIS_FILE,
                  "...info: timeslice diff between lowest & highest=" counter_fmt "%%",
                  diff));
    }

    pj_pool_release(pool);
    return 0;
}

#endif  /* PJTEST_TIME_PASSES_WHEN_BUSY */

/*
 * Thread-local storage. Each thread must see only its own values, in
 * every slot, while the other threads run; a slot freed and allocated
 * again must read NULL in every thread; and allocation must fail cleanly
 * when the slots run out.
 */
enum { TLS_SLOTS = 4, TLS_THREADS = 4, TLS_ROUNDS = 10, TLS_MAX_ALLOC = 64,
       TLS_MAIN = TLS_THREADS };

#define TLS_VAL(id,k)   ((void*)(pj_ssize_t)(((id)+1)*100 + (k)+1))

static struct tls_test
{
    long            slot[TLS_SLOTS];
    long            new_slot;           /* slot[0] freed and reallocated */
    volatile int    stage;              /* 2: new_slot allocated         */
    volatile int    done[TLS_THREADS];  /* thread finished its rounds    */
    int             err[TLS_THREADS];
} tls_test;

/* Wait up to ten seconds for *flag to become nonzero. */
static int tls_wait(volatile int *flag)
{
    int i;
    for (i=0; i<1000 && !*flag; ++i)
        pj_thread_sleep(10);
    return *flag ? 0 : -1;
}

static int tls_thread_proc(void *arg)
{
    struct tls_test *t = &tls_test;
    int id = (int)(pj_ssize_t)arg;
    int k, round, errors = 0;

    for (k=0; k<TLS_SLOTS; ++k) {
        if (pj_thread_local_get(t->slot[k]) != NULL)
            ++errors;
        pj_thread_local_set(t->slot[k], TLS_VAL(id, k));
    }
    for (round=0; round<TLS_ROUNDS; ++round) {
        pj_thread_sleep(10);
        for (k=0; k<TLS_SLOTS; ++k) {
            if (pj_thread_local_get(t->slot[k]) != TLS_VAL(id, k))
                ++errors;
        }
    }
    t->done[id] = 1;

    if (tls_wait(&t->stage) != 0)
        ++errors;
    if (pj_thread_local_get(t->new_slot) != NULL)
        ++errors;
    pj_thread_local_set(t->new_slot, TLS_VAL(id, 0));
    if (pj_thread_local_get(t->new_slot) != TLS_VAL(id, 0))
        ++errors;
    for (k=1; k<TLS_SLOTS; ++k) {
        if (pj_thread_local_get(t->slot[k]) != TLS_VAL(id, k))
            ++errors;
    }

    t->err[id] = errors;
    return 0;
}

/* Every slot in the list must read NULL here, then hold what we set. */
static int tls_list_thread_proc(void *arg)
{
    long *slots = (long*)arg;
    int i, errors = 0;

    for (i=0; slots[i] >= 0; ++i) {
        if (pj_thread_local_get(slots[i]) != NULL)
            ++errors;
        pj_thread_local_set(slots[i], TLS_VAL(0, i));
        if (pj_thread_local_get(slots[i]) != TLS_VAL(0, i))
            ++errors;
    }
    return errors;
}

static int thread_local_test(void)
{
    struct tls_test *t = &tls_test;
    pj_pool_t *pool;
    pj_thread_t *threads[TLS_THREADS], *thread;
    long slots[TLS_MAX_ALLOC+1];
    int i, k, n_slots, errors;
    pj_status_t rc;

    PJ_LOG(3,(THIS_FILE, "..thread-local storage test"));

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, return -70);

    /* Slots start NULL and read back what main stores. */
    pj_bzero(t, sizeof(*t));
    for (k=0; k<TLS_SLOTS; ++k) {
        PJ_TEST_SUCCESS(pj_thread_local_alloc(&t->slot[k]), NULL,
                        return -71);
        PJ_TEST_EQ(pj_thread_local_get(t->slot[k]), NULL, NULL, return -72);
        PJ_TEST_SUCCESS(pj_thread_local_set(t->slot[k], TLS_VAL(TLS_MAIN, k)),
                        NULL, return -73);
        PJ_TEST_EQ(pj_thread_local_get(t->slot[k]), TLS_VAL(TLS_MAIN, k),
                   NULL, return -74);
    }

    /* Threads run concurrently, each with its own values in each slot. */
    for (i=0; i<TLS_THREADS; ++i) {
        rc = pj_thread_create(pool, "tls%p", &tls_thread_proc,
                              (void*)(pj_ssize_t)i,
                              PJ_THREAD_DEFAULT_STACK_SIZE, 0, &threads[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: creating thread", rc);
            t->stage = 2;
            while (--i >= 0) {
                pj_thread_join(threads[i]);
                pj_thread_destroy(threads[i]);
            }
            return -75;
        }
    }
    for (i=0; i<TLS_THREADS; ++i) {
        PJ_TEST_EQ(tls_wait(&t->done[i]), 0, "thread did not finish",
                   {t->stage = 2; return -76;});
    }
    for (k=0; k<TLS_SLOTS; ++k) {
        PJ_TEST_EQ(pj_thread_local_get(t->slot[k]), TLS_VAL(TLS_MAIN, k),
                   "a thread's value leaked to main", {t->stage=2; return -77;});
    }

    /* A slot freed and allocated again reads NULL in every live thread. */
    pj_thread_local_free(t->slot[0]);
    PJ_TEST_SUCCESS(pj_thread_local_alloc(&t->new_slot), NULL,
                    {t->stage = 2; return -78;});
    PJ_TEST_EQ(pj_thread_local_get(t->new_slot), NULL,
               "reallocated slot not NULL in main", {t->stage=2; return -79;});
    t->stage = 2;

    errors = 0;
    for (i=0; i<TLS_THREADS; ++i) {
        pj_thread_join(threads[i]);
        pj_thread_destroy(threads[i]);
        errors += t->err[i];
    }
    PJ_TEST_EQ(errors, 0, "wrong values seen in threads", return -80);
    PJ_TEST_EQ(pj_thread_local_get(t->new_slot), NULL,
               "a thread's value leaked to main", return -81);
    for (k=1; k<TLS_SLOTS; ++k) {
        PJ_TEST_EQ(pj_thread_local_get(t->slot[k]), TLS_VAL(TLS_MAIN, k),
                   "a thread's value leaked to main", return -82);
    }
    pj_thread_local_free(t->new_slot);
    for (k=1; k<TLS_SLOTS; ++k)
        pj_thread_local_free(t->slot[k]);

    /* Allocate until the slots run out or we have plenty; every slot must
     * still be independent, and freeing them must make room again.
     */
    rc = PJ_SUCCESS;
    for (n_slots=0; n_slots<TLS_MAX_ALLOC; ++n_slots) {
        rc = pj_thread_local_alloc(&slots[n_slots]);
        if (rc != PJ_SUCCESS)
            break;
        pj_thread_local_set(slots[n_slots], TLS_VAL(TLS_MAIN, n_slots));
    }
    slots[n_slots] = -1;
    PJ_LOG(3,(THIS_FILE, "....%d slots allocated%s", n_slots,
              rc==PJ_SUCCESS ? "" : " before allocation failed"));
    PJ_TEST_TRUE(n_slots >= TLS_SLOTS+1, "too few slots", return -83);

    rc = pj_thread_create(pool, "tls%p", &tls_list_thread_proc, slots,
                          PJ_THREAD_DEFAULT_STACK_SIZE, 0, &thread);
    PJ_TEST_SUCCESS(rc, NULL, return -84);
    pj_thread_join(thread);
    pj_thread_destroy(thread);

    for (i=0; i<n_slots; ++i) {
        PJ_TEST_EQ(pj_thread_local_get(slots[i]), TLS_VAL(TLS_MAIN, i),
                   "slots are not independent", return -85);
        pj_thread_local_free(slots[i]);
    }
    PJ_TEST_SUCCESS(pj_thread_local_alloc(&slots[0]),
                    "no slot free after freeing them all", return -86);
    PJ_TEST_EQ(pj_thread_local_get(slots[0]), NULL, NULL, return -87);
    pj_thread_local_free(slots[0]);

    pj_pool_release(pool);
    return 0;
}

int thread_test(void)
{
    int rc;

    rc = thread_local_test();
    if (rc != 0)
        return rc;

#if PJTEST_TIME_PASSES_WHEN_BUSY
    rc = simple_thread("simple thread test", 0);
    if (rc != PJ_SUCCESS)
        return rc;

    rc = simple_thread("suspended thread test", PJ_THREAD_SUSPENDED);
    if (rc != PJ_SUCCESS)
        return rc;

    rc = timeslice_test();
    if (rc != PJ_SUCCESS)
        return rc;
#else
    PJ_LOG(3,(THIS_FILE, "..spinning thread tests skipped on this target"));
#endif

    return rc;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled.
 */
int dummy_thread_test;
#endif  /* INCLUDE_THREAD_TEST */



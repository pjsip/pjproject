/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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

/**
 * @file grp_lock.c
 * @brief Group lock tests — specifically targeting unchain race conditions
 *        between concurrent acquire/release/tryacquire and unchain operations.
 */
#include "test.h"
#include <pjlib.h>

#if INCLUDE_GRP_LOCK_TEST && PJ_HAS_THREADS

#define THIS_FILE   "grp_lock.c"

/*=========================================================================
 * Basic chaining/unchaining test (no threading).
 *=========================================================================*/
static int basic_chain_unchain_test(pj_pool_t *pool)
{
    pj_grp_lock_t *grp_lock;
    pj_grp_lock_t *ext_grp_lock;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "...basic chain/unchain test"));

    /* Create two group locks */
    status = pj_grp_lock_create(pool, NULL, &grp_lock);
    if (status != PJ_SUCCESS) return -100;

    status = pj_grp_lock_create(pool, NULL, &ext_grp_lock);
    if (status != PJ_SUCCESS) return -110;

    pj_grp_lock_add_ref(grp_lock);
    pj_grp_lock_add_ref(ext_grp_lock);

    /* Chain ext_grp_lock into grp_lock */
    status = pj_grp_lock_chain_lock(grp_lock,
                                    (pj_lock_t *)ext_grp_lock, 0);
    if (status != PJ_SUCCESS) return -120;

    /* Acquire group lock — should also acquire ext lock */
    pj_grp_lock_acquire(grp_lock);

    /* Release group lock */
    pj_grp_lock_release(grp_lock);

    /* Acquire again and unchain while holding */
    pj_grp_lock_acquire(grp_lock);

    status = pj_grp_lock_unchain_lock(grp_lock,
                                      (pj_lock_t *)ext_grp_lock);
    if (status != PJ_SUCCESS) return -130;

    pj_grp_lock_release(grp_lock);

    /* Acquire/release after unchain — should work without ext lock */
    pj_grp_lock_acquire(grp_lock);
    pj_grp_lock_release(grp_lock);

    /* Clean up */
    pj_grp_lock_dec_ref(ext_grp_lock);
    pj_grp_lock_dec_ref(grp_lock);

    return 0;
}

/*=========================================================================
 * Race condition stress tests.
 *
 * Strategy: multiple threads concurrently acquire/release/tryacquire a
 * group lock while another thread unchains an external lock from it.
 * We use a semaphore to synchronize the start so threads are as likely
 * as possible to hit the race window.
 *=========================================================================*/

/* Shared state for all race tests */
typedef struct race_test_state
{
    pj_pool_t       *pool;
    pj_grp_lock_t   *grp_lock;      /* The group lock under test */
    pj_grp_lock_t   *ext_grp_lock;  /* External lock to chain/unchain */

    pj_sem_t        *start_sem;      /* Fired to start all threads */
    pj_atomic_t     *started;        /* Count of threads that started */

    int              n_threads;      /* Number of worker threads */
    int              n_iterations;   /* Iterations per thread */
    int              n_rounds;       /* Chain/unchain rounds */
    volatile pj_bool_t stopping;     /* Shutdown flag */
} race_test_state;

static void race_state_init(race_test_state *state, pj_pool_t *pool,
                            int n_threads, int n_iterations, int n_rounds)
{
    pj_status_t status;

    pj_bzero(state, sizeof(*state));
    state->pool = pool;
    state->n_threads = n_threads;
    state->n_iterations = n_iterations;
    state->n_rounds = n_rounds;

    status = pj_grp_lock_create(pool, NULL, &state->grp_lock);
    pj_assert(status == PJ_SUCCESS);
    pj_grp_lock_add_ref(state->grp_lock);

    status = pj_grp_lock_create(pool, NULL, &state->ext_grp_lock);
    pj_assert(status == PJ_SUCCESS);
    pj_grp_lock_add_ref(state->ext_grp_lock);

    status = pj_sem_create(pool, "race", 0, n_threads + 1, &state->start_sem);
    pj_assert(status == PJ_SUCCESS);

    status = pj_atomic_create(pool, 0, &state->started);
    pj_assert(status == PJ_SUCCESS);
}

static void race_state_destroy(race_test_state *state)
{
    if (state->started)
        pj_atomic_destroy(state->started);
    if (state->start_sem)
        pj_sem_destroy(state->start_sem);
    if (state->ext_grp_lock)
        pj_grp_lock_dec_ref(state->ext_grp_lock);
    if (state->grp_lock)
        pj_grp_lock_dec_ref(state->grp_lock);
}

/*-------------------------------------------------------------------------
 * Worker thread: repeatedly acquire/release the group lock.
 *-------------------------------------------------------------------------*/
static int acquire_release_worker(void *arg)
{
    race_test_state *state = (race_test_state *)arg;
    int i;

    /* Signal ready and wait for start */
    pj_atomic_inc(state->started);
    pj_sem_wait(state->start_sem);

    for (i = 0; i < state->n_iterations && !state->stopping; ++i) {
        pj_grp_lock_acquire(state->grp_lock);
        /* Brief hold to increase overlap probability */
        pj_thread_sleep(0);
        pj_grp_lock_release(state->grp_lock);
    }

    return 0;
}

/*-------------------------------------------------------------------------
 * Worker thread: repeatedly tryacquire/release the group lock.
 *-------------------------------------------------------------------------*/
static int tryacquire_release_worker(void *arg)
{
    race_test_state *state = (race_test_state *)arg;
    int i;

    pj_atomic_inc(state->started);
    pj_sem_wait(state->start_sem);

    for (i = 0; i < state->n_iterations && !state->stopping; ++i) {
        pj_status_t status = pj_grp_lock_tryacquire(state->grp_lock);
        if (status == PJ_SUCCESS) {
            pj_thread_sleep(0);
            pj_grp_lock_release(state->grp_lock);
        }
    }

    return 0;
}

/*-------------------------------------------------------------------------
 * Worker thread: mixed acquire + tryacquire + release.
 *-------------------------------------------------------------------------*/
static int mixed_worker(void *arg)
{
    race_test_state *state = (race_test_state *)arg;
    int i;

    pj_atomic_inc(state->started);
    pj_sem_wait(state->start_sem);

    for (i = 0; i < state->n_iterations && !state->stopping; ++i) {
        if (i % 3 == 0) {
            pj_status_t status;
            status = pj_grp_lock_tryacquire(state->grp_lock);
            if (status == PJ_SUCCESS) {
                pj_thread_sleep(0);
                pj_grp_lock_release(state->grp_lock);
            }
        } else {
            pj_grp_lock_acquire(state->grp_lock);
            pj_thread_sleep(0);
            pj_grp_lock_release(state->grp_lock);
        }
    }

    return 0;
}

/*-------------------------------------------------------------------------
 * Driver: chain, start threads, unchain, join threads, repeat.
 *
 * Each round:
 *   1. Chain the external lock into the group lock.
 *   2. Start N worker threads that acquire/release/tryacquire.
 *   3. Wait for all threads to start, then fire the start semaphore.
 *   4. Unchain the external lock while threads are running.
 *   5. Wait for all threads to finish.
 *   6. Verify the group lock and external lock are in a clean state.
 *-------------------------------------------------------------------------*/

#define MAX_RACE_THREADS 8

static int run_race_test(race_test_state *state,
                         int (*worker_func)(void *),
                         const char *test_name)
{
    int round;

    PJ_LOG(3,(THIS_FILE, "...%s (%d rounds, %d threads, %d iterations)",
              test_name, state->n_rounds, state->n_threads,
              state->n_iterations));

    for (round = 0; round < state->n_rounds; ++round) {
        pj_thread_t *threads[MAX_RACE_THREADS];
        pj_status_t status;
        int i, n_created = 0;

        pj_assert(state->n_threads <= MAX_RACE_THREADS);

        /* Reset per-round state */
        pj_atomic_set(state->started, 0);
        state->stopping = PJ_FALSE;

        /* Chain the external lock */
        pj_grp_lock_add_ref(state->ext_grp_lock);
        status = pj_grp_lock_chain_lock(state->grp_lock,
                                        (pj_lock_t *)state->ext_grp_lock,
                                        0);
        if (status != PJ_SUCCESS) {
            pj_grp_lock_dec_ref(state->ext_grp_lock);
            PJ_LOG(1,(THIS_FILE, "chain_lock failed, round %d", round));
            return -200;
        }

        /* Spawn worker threads */
        for (i = 0; i < state->n_threads; ++i) {
            char name[16];
            pj_ansi_snprintf(name, sizeof(name), "race%d", i);
            status = pj_thread_create(state->pool, name,
                                      worker_func, state,
                                      0, 0, &threads[i]);
            if (status != PJ_SUCCESS) {
                PJ_LOG(1,(THIS_FILE, "thread_create failed"));
                n_created = i;
                state->stopping = PJ_TRUE;
                for (i = 0; i < n_created; ++i)
                    pj_sem_post(state->start_sem);
                for (i = 0; i < n_created; ++i) {
                    pj_thread_join(threads[i]);
                    pj_thread_destroy(threads[i]);
                }
                pj_grp_lock_unchain_lock(state->grp_lock,
                                         (pj_lock_t *)state->ext_grp_lock);
                pj_grp_lock_dec_ref(state->ext_grp_lock);
                return -210;
            }
        }

        /* Wait for all threads to be ready */
        while (pj_atomic_get(state->started) < state->n_threads)
            pj_thread_sleep(1);

        /* Fire! All threads start simultaneously. */
        for (i = 0; i < state->n_threads; ++i)
            pj_sem_post(state->start_sem);

        /* Brief pause to let threads get into acquire/release loops */
        pj_thread_sleep(1);

        /* Unchain while threads are running — this is the race target */
        pj_grp_lock_unchain_lock(state->grp_lock,
                                 (pj_lock_t *)state->ext_grp_lock);
        pj_grp_lock_dec_ref(state->ext_grp_lock);

        /* Wait for all threads to finish */
        for (i = 0; i < state->n_threads; ++i) {
            pj_thread_join(threads[i]);
            pj_thread_destroy(threads[i]);
        }

        /* Verify: group lock should be acquirable (not stuck) */
        pj_grp_lock_acquire(state->grp_lock);
        pj_grp_lock_release(state->grp_lock);

        /* Verify: external lock should be acquirable (not stuck) */
        pj_grp_lock_acquire(state->ext_grp_lock);
        pj_grp_lock_release(state->ext_grp_lock);
    }

    return 0;
}

/*=========================================================================
 * Test 1: acquire vs. unchain (Race 1 — forward traversal, blocking)
 *=========================================================================*/
static int acquire_vs_unchain_test(pj_pool_t *pool)
{
    race_test_state state;
    int rc;

    race_state_init(&state, pool, 4, 200, 50);
    rc = run_race_test(&state, &acquire_release_worker,
                       "acquire vs unchain");
    race_state_destroy(&state);
    return rc;
}

/*=========================================================================
 * Test 2: release vs. unchain (Race 2 — backward traversal)
 *
 * Same as test 1 — acquire_release_worker exercises both acquire (forward)
 * and release (backward). We use more threads to increase scheduling
 * pressure and the chance of preemption during release traversal.
 *=========================================================================*/
static int release_vs_unchain_test(pj_pool_t *pool)
{
    race_test_state state;
    int rc;

    race_state_init(&state, pool, 6, 200, 50);
    rc = run_race_test(&state, &acquire_release_worker,
                       "release vs unchain");
    race_state_destroy(&state);
    return rc;
}

/*=========================================================================
 * Test 3: tryacquire vs. unchain (Race 3 — forward + rollback)
 *=========================================================================*/
static int tryacquire_vs_unchain_test(pj_pool_t *pool)
{
    race_test_state state;
    int rc;

    race_state_init(&state, pool, 4, 200, 50);
    rc = run_race_test(&state, &tryacquire_release_worker,
                       "tryacquire vs unchain");
    race_state_destroy(&state);
    return rc;
}

/*=========================================================================
 * Test 4: mixed operations vs. unchain
 *
 * Multiple threads do a mix of acquire, tryacquire, and release while
 * another path unchains. This tests all race windows simultaneously.
 *=========================================================================*/
static int mixed_vs_unchain_test(pj_pool_t *pool)
{
    race_test_state state;
    int rc;

    race_state_init(&state, pool, 6, 200, 50);
    rc = run_race_test(&state, &mixed_worker,
                       "mixed ops vs unchain");
    race_state_destroy(&state);
    return rc;
}

/*=========================================================================
 * Test 5: multiple chained locks — unchain one while threads are active.
 *
 * Chain two external locks (at different priorities) into the group lock.
 * Unchain only one while threads are running. Verify the remaining chain
 * still works correctly.
 *=========================================================================*/
static int multi_chain_unchain_test(pj_pool_t *pool)
{
    race_test_state state;
    pj_grp_lock_t *ext2;
    pj_status_t status;
    int round;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE,
              "...multi-chain unchain test (50 rounds, 4 threads)"));

    race_state_init(&state, pool, 4, 200, 1);

    /* Create a second external lock */
    status = pj_grp_lock_create(pool, NULL, &ext2);
    if (status != PJ_SUCCESS) return -500;
    pj_grp_lock_add_ref(ext2);

    for (round = 0; round < 50 && rc == 0; ++round) {
        pj_thread_t *threads[MAX_RACE_THREADS];
        int i, n_created = 0;

        pj_atomic_set(state.started, 0);
        state.stopping = PJ_FALSE;

        /* Chain two external locks at different priorities */
        pj_grp_lock_add_ref(state.ext_grp_lock);
        status = pj_grp_lock_chain_lock(state.grp_lock,
                                        (pj_lock_t *)state.ext_grp_lock,
                                        -1);
        if (status != PJ_SUCCESS) { rc = -510; break; }

        pj_grp_lock_add_ref(ext2);
        status = pj_grp_lock_chain_lock(state.grp_lock,
                                        (pj_lock_t *)ext2, -2);
        if (status != PJ_SUCCESS) { rc = -520; break; }

        /* Spawn threads */
        for (i = 0; i < state.n_threads; ++i) {
            char name[16];
            pj_ansi_snprintf(name, sizeof(name), "mc%d", i);
            status = pj_thread_create(state.pool, name,
                                      &mixed_worker, &state,
                                      0, 0, &threads[i]);
            if (status != PJ_SUCCESS) {
                n_created = i;
                state.stopping = PJ_TRUE;
                for (i = 0; i < n_created; ++i)
                    pj_sem_post(state.start_sem);
                for (i = 0; i < n_created; ++i) {
                    pj_thread_join(threads[i]);
                    pj_thread_destroy(threads[i]);
                }
                rc = -530;
                break;
            }
        }
        if (rc != 0) break;

        while (pj_atomic_get(state.started) < state.n_threads)
            pj_thread_sleep(1);

        for (i = 0; i < state.n_threads; ++i)
            pj_sem_post(state.start_sem);

        pj_thread_sleep(1);

        /* Unchain only the first external lock */
        pj_grp_lock_unchain_lock(state.grp_lock,
                                 (pj_lock_t *)state.ext_grp_lock);
        pj_grp_lock_dec_ref(state.ext_grp_lock);

        for (i = 0; i < state.n_threads; ++i) {
            pj_thread_join(threads[i]);
            pj_thread_destroy(threads[i]);
        }

        /* Verify all locks are in clean state */
        pj_grp_lock_acquire(state.grp_lock);
        pj_grp_lock_release(state.grp_lock);

        pj_grp_lock_acquire(state.ext_grp_lock);
        pj_grp_lock_release(state.ext_grp_lock);

        pj_grp_lock_acquire(ext2);
        pj_grp_lock_release(ext2);

        /* Unchain the second external lock for next round */
        pj_grp_lock_unchain_lock(state.grp_lock, (pj_lock_t *)ext2);
        pj_grp_lock_dec_ref(ext2);
    }

    pj_grp_lock_dec_ref(ext2);
    race_state_destroy(&state);
    return rc;
}

/*=========================================================================
 * Test 6: repeated chain/unchain cycles (no concurrency).
 *
 * Verify that chaining and unchaining the same lock multiple times
 * doesn't corrupt the group lock state.
 *=========================================================================*/
static int repeated_chain_unchain_test(pj_pool_t *pool)
{
    pj_grp_lock_t *grp_lock;
    pj_grp_lock_t *ext_grp_lock;
    pj_status_t status;
    int i;

    PJ_LOG(3,(THIS_FILE, "...repeated chain/unchain cycles"));

    status = pj_grp_lock_create(pool, NULL, &grp_lock);
    if (status != PJ_SUCCESS) return -600;
    pj_grp_lock_add_ref(grp_lock);

    status = pj_grp_lock_create(pool, NULL, &ext_grp_lock);
    if (status != PJ_SUCCESS) return -610;
    pj_grp_lock_add_ref(ext_grp_lock);

    for (i = 0; i < 100; ++i) {
        /* Chain */
        pj_grp_lock_add_ref(ext_grp_lock);
        pj_grp_lock_chain_lock(grp_lock,
                                (pj_lock_t *)ext_grp_lock, 0);

        /* Acquire/release with chain active */
        pj_grp_lock_acquire(grp_lock);
        pj_grp_lock_release(grp_lock);

        /* Unchain */
        pj_grp_lock_unchain_lock(grp_lock,
                                  (pj_lock_t *)ext_grp_lock);
        pj_grp_lock_dec_ref(ext_grp_lock);

        /* Acquire/release after unchain */
        pj_grp_lock_acquire(grp_lock);
        pj_grp_lock_release(grp_lock);
    }

    pj_grp_lock_dec_ref(ext_grp_lock);
    pj_grp_lock_dec_ref(grp_lock);

    return 0;
}

/*=========================================================================
 * Test entry point
 *=========================================================================*/
int grp_lock_test(void)
{
    pj_pool_t *pool;
    int rc;

    pool = pj_pool_create(mem, "grplock", 4000, 4000, NULL);
    if (!pool)
        return -10;

    rc = basic_chain_unchain_test(pool);
    if (rc != 0) goto on_return;

    rc = repeated_chain_unchain_test(pool);
    if (rc != 0) goto on_return;

    rc = acquire_vs_unchain_test(pool);
    if (rc != 0) goto on_return;

    rc = release_vs_unchain_test(pool);
    if (rc != 0) goto on_return;

    rc = tryacquire_vs_unchain_test(pool);
    if (rc != 0) goto on_return;

    rc = mixed_vs_unchain_test(pool);
    if (rc != 0) goto on_return;

    rc = multi_chain_unchain_test(pool);
    if (rc != 0) goto on_return;

on_return:
    pj_pool_release(pool);
    return rc;
}

#else
int dummy_grp_lock_test;
#endif  /* INCLUDE_GRP_LOCK_TEST && PJ_HAS_THREADS */

/* $Id$ */
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

/**
 * \page page_pjlib_timer_test Test: Timer
 *
 * This file provides implementation of \b timer_test(). It tests the
 * functionality of the timer heap.
 *
 *
 * This file is <b>pjlib-test/timer.c</b>
 *
 * \include pjlib-test/timer.c
 */


#if INCLUDE_TIMER_TEST

#include <pjlib.h>

#define LOOP		16
#define MIN_COUNT	250
#define MAX_COUNT	(LOOP * MIN_COUNT)
#define MIN_DELAY	2
#define	D		(MAX_COUNT / 32000)
#define DELAY		(D < MIN_DELAY ? MIN_DELAY : D)
#define THIS_FILE	"timer_test"


static void timer_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    PJ_UNUSED_ARG(ht);
    PJ_UNUSED_ARG(e);
}

static int test_timer_heap(void)
{
    int i, j;
    pj_timer_entry *entry;
    pj_pool_t *pool;
    pj_timer_heap_t *timer;
    pj_time_val delay;
    pj_status_t status;
    int err=0;
    pj_size_t size;
    unsigned count;

    PJ_LOG(3,("test", "...Basic test"));

    size = pj_timer_heap_mem_size(MAX_COUNT)+MAX_COUNT*sizeof(pj_timer_entry);
    pool = pj_pool_create( mem, NULL, size, 4000, NULL);
    if (!pool) {
	PJ_LOG(3,("test", "...error: unable to create pool of %u bytes",
		  size));
	return -10;
    }

    entry = (pj_timer_entry*)pj_pool_calloc(pool, MAX_COUNT, sizeof(*entry));
    if (!entry)
	return -20;

    for (i=0; i<MAX_COUNT; ++i) {
	entry[i].cb = &timer_callback;
    }
    status = pj_timer_heap_create(pool, MAX_COUNT, &timer);
    if (status != PJ_SUCCESS) {
        app_perror("...error: unable to create timer heap", status);
	return -30;
    }

    count = MIN_COUNT;
    for (i=0; i<LOOP; ++i) {
	int early = 0;
	int done=0;
	int cancelled=0;
	int rc;
	pj_timestamp t1, t2, t_sched, t_cancel, t_poll;
	pj_time_val now, expire;

	pj_gettimeofday(&now);
	pj_srand(now.sec);
	t_sched.u32.lo = t_cancel.u32.lo = t_poll.u32.lo = 0;

	// Register timers
	for (j=0; j<(int)count; ++j) {
	    delay.sec = pj_rand() % DELAY;
	    delay.msec = pj_rand() % 1000;

	    // Schedule timer
	    pj_get_timestamp(&t1);
	    rc = pj_timer_heap_schedule(timer, &entry[j], &delay);
	    if (rc != 0)
		return -40;
	    pj_get_timestamp(&t2);

	    t_sched.u32.lo += (t2.u32.lo - t1.u32.lo);

	    // Poll timers.
	    pj_get_timestamp(&t1);
	    rc = pj_timer_heap_poll(timer, NULL);
	    pj_get_timestamp(&t2);
	    if (rc > 0) {
		t_poll.u32.lo += (t2.u32.lo - t1.u32.lo);
		early += rc;
	    }
	}

	// Set the time where all timers should finish
	pj_gettimeofday(&expire);
	delay.sec = DELAY; 
	delay.msec = 0;
	PJ_TIME_VAL_ADD(expire, delay);

	// Wait unfil all timers finish, cancel some of them.
	do {
	    int index = pj_rand() % count;
	    pj_get_timestamp(&t1);
	    rc = pj_timer_heap_cancel(timer, &entry[index]);
	    pj_get_timestamp(&t2);
	    if (rc > 0) {
		cancelled += rc;
		t_cancel.u32.lo += (t2.u32.lo - t1.u32.lo);
	    }

	    pj_gettimeofday(&now);

	    pj_get_timestamp(&t1);
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0
	    /* On Symbian, we must use OS poll (Active Scheduler poll) since 
	     * timer is implemented using Active Object.
	     */
	    rc = 0;
	    while (pj_symbianos_poll(-1, 0))
		++rc;
#else
	    rc = pj_timer_heap_poll(timer, NULL);
#endif
	    pj_get_timestamp(&t2);
	    if (rc > 0) {
		done += rc;
		t_poll.u32.lo += (t2.u32.lo - t1.u32.lo);
	    }

	} while (PJ_TIME_VAL_LTE(now, expire)&&pj_timer_heap_count(timer) > 0);

	if (pj_timer_heap_count(timer)) {
	    PJ_LOG(3, (THIS_FILE, "ERROR: %d timers left", 
		       pj_timer_heap_count(timer)));
	    ++err;
	}
	t_sched.u32.lo /= count; 
	t_cancel.u32.lo /= count;
	t_poll.u32.lo /= count;
	PJ_LOG(4, (THIS_FILE, 
	        "...ok (count:%d, early:%d, cancelled:%d, "
		"sched:%d, cancel:%d poll:%d)", 
		count, early, cancelled, t_sched.u32.lo, t_cancel.u32.lo,
		t_poll.u32.lo));

	count = count * 2;
	if (count > MAX_COUNT)
	    break;
    }

    pj_pool_release(pool);
    return err;
}


/***************
 * Stress test *
 ***************
 * Test scenario (if RANDOMIZED_TEST is 0):
 * 1. Create and schedule a number of timer entries.
 * 2. Start threads for polling (simulating normal worker thread).
 *    Each expired entry will try to cancel and re-schedule itself
 *    from within the callback.
 * 3. Start threads for cancelling random entries. Each successfully
 *    cancelled entry will be re-scheduled after some random delay.
 *
 * Test scenario (if RANDOMIZED_TEST is 1):
 * 1. Create and schedule a number of timer entries.
 * 2. Start threads which will, based on a configurable probability
 *    setting, randomly perform timer scheduling, cancelling, or
 *    polling (simulating normal worker thread).
 * This test is considered a failure if:
 * - It triggers assertion/crash.
 * - There's an error message in the log, which indicates a potential
 *   bug in the implementation (note that race message is ok).
 */
#define RANDOMIZED_TEST 1
#define SIMULATE_CRASH	PJ_TIMER_USE_COPY

#if RANDOMIZED_TEST
    #define ST_STRESS_THREAD_COUNT	    20
    #define ST_POLL_THREAD_COUNT	    0
    #define ST_CANCEL_THREAD_COUNT	    0
#else
    #define ST_STRESS_THREAD_COUNT	    0
    #define ST_POLL_THREAD_COUNT	    10
    #define ST_CANCEL_THREAD_COUNT	    10
#endif

#define ST_ENTRY_COUNT		    10000
#define ST_DURATION		    30000
#define ST_ENTRY_MAX_TIMEOUT_MS	    ST_DURATION/10

/* Number of group lock, may be zero, shared by timer entries, group lock
 * can be useful to evaluate poll vs cancel race condition scenario, i.e:
 * each group lock must have ref count==1 at the end of the test, otherwise
 * assertion will raise.
 */
#define ST_ENTRY_GROUP_LOCK_COUNT   1

#define BT_ENTRY_COUNT 100000
#define BT_ENTRY_SHOW_START 100
#define BT_ENTRY_SHOW_MULT 10
#define BT_REPEAT_RANDOM_TEST 4
#define BT_REPEAT_INC_TEST 4

struct thread_param
{
    pj_timer_heap_t *timer;
    pj_bool_t stopping;
    pj_timer_entry *entries;
    pj_atomic_t **status;
    pj_atomic_t *n_sched, *n_cancel, *n_poll;
    pj_grp_lock_t **grp_locks;
    int err;

    pj_atomic_t *idx;
    struct {
	pj_bool_t is_poll;
	unsigned cnt;
    } stat[ST_POLL_THREAD_COUNT + ST_CANCEL_THREAD_COUNT + 1];
    /* Plus one here to avoid compile warning of zero-sized array */
};

static pj_status_t st_schedule_entry(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    pj_time_val delay = {0};
    pj_grp_lock_t *grp_lock = NULL;
    pj_status_t status;
    struct thread_param *tparam = (struct thread_param *)e->user_data;

    if (ST_ENTRY_GROUP_LOCK_COUNT && pj_rand() % 10) {
	/* About 90% of entries should have group lock */
	grp_lock = tparam->grp_locks[pj_rand() % ST_ENTRY_GROUP_LOCK_COUNT];
    }

    delay.msec = pj_rand() % ST_ENTRY_MAX_TIMEOUT_MS;
    pj_time_val_normalize(&delay);
    status = pj_timer_heap_schedule_w_grp_lock(ht, e, &delay, 1, grp_lock);
    return status;
}

static void dummy_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    PJ_UNUSED_ARG(ht);
    PJ_LOG(4,("test", "dummy callback called %p %p", e, e->user_data));
}

static void st_entry_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    struct thread_param *tparam = (struct thread_param *)e->user_data;

#if RANDOMIZED_TEST
    /* Make sure the flag has been set. */
    while (pj_atomic_get(tparam->status[e - tparam->entries]) != 1)
    	pj_thread_sleep(10);
    pj_atomic_set(tparam->status[e - tparam->entries], 0);
#endif

    /* try to cancel this */
    pj_timer_heap_cancel_if_active(ht, e, 10);
    
    /* busy doing something */
    pj_thread_sleep(pj_rand() % 50);

    /* reschedule entry */
    if (!ST_STRESS_THREAD_COUNT)
    	st_schedule_entry(ht, e);
}

/* Randomized stress worker thread function. */
static int stress_worker(void *arg)
{
    /* Enumeration of possible task. */
    enum {
    	SCHEDULING = 0,
    	CANCELLING = 1,
    	POLLING = 2,
    	NOTHING = 3
    };
    /* Probability of a certain task being chosen.
     * The first number indicates the probability of the first task,
     * the second number for the second task, and so on.
     */
    int prob[3] = {75, 15, 5};
    struct thread_param *tparam = (struct thread_param*)arg;
    int t_idx, i;

    t_idx = pj_atomic_inc_and_get(tparam->idx);
    PJ_LOG(4,("test", "...thread #%d (random) started", t_idx));
    while (!tparam->stopping) {
    	int job, task;
	int idx, count;
    	pj_status_t prev_status, status;

    	/* Randomly choose which task to do */
    	job = pj_rand() % 100;
    	if (job < prob[0]) task = SCHEDULING;
    	else if (job < (prob[0] + prob[1])) task = CANCELLING;
    	else if (job < (prob[0] + prob[1] + prob[2])) task = POLLING;
    	else task = NOTHING;
    
    	idx = pj_rand() % ST_ENTRY_COUNT;
    	prev_status = pj_atomic_get(tparam->status[idx]);
    	if (task == SCHEDULING) {
    	    if (prev_status != 0) continue;
    	    status = st_schedule_entry(tparam->timer, &tparam->entries[idx]);
    	    if (prev_status == 0 && status != PJ_SUCCESS) {
    	        /* To make sure the flag has been set. */
    	        pj_thread_sleep(20);
    	        if (pj_atomic_get(tparam->status[idx]) == 1) {
    	    	    /* Race condition with another scheduling. */
    	    	    PJ_LOG(3,("test", "race schedule-schedule %d: %p",
    	    	   	              idx, &tparam->entries[idx]));
    	        } else {
    	            if (tparam->err != 0) tparam->err = -210;
	    	    PJ_LOG(3,("test", "error: failed to schedule entry %d: %p",
	    	   		      idx, &tparam->entries[idx]));
	    	}
    	    } else if (prev_status == 1 && status == PJ_SUCCESS) {
    	    	/* Race condition with another cancellation or
    	    	 * timer poll.
    	    	 */
    	    	pj_thread_sleep(20);
    	    	PJ_LOG(3,("test", "race schedule-cancel/poll %d: %p",
    	    	   	          idx, &tparam->entries[idx]));
    	    }
    	    if (status == PJ_SUCCESS) {
    	    	pj_atomic_set(tparam->status[idx], 1);
    	    	pj_atomic_inc(tparam->n_sched);
    	    }
    	} else if (task == CANCELLING) {
	    count = pj_timer_heap_cancel_if_active(tparam->timer,
	    					   &tparam->entries[idx], 10);
    	    if (prev_status == 0 && count > 0) {
    	        /* To make sure the flag has been set. */
    	        pj_thread_sleep(20);
    	        if (pj_atomic_get(tparam->status[idx]) == 1) {
    	    	    /* Race condition with scheduling. */
    	    	    PJ_LOG(3,("test", "race cancel-schedule %d: %p",
    	    	   	              idx, &tparam->entries[idx]));
    	        } else {
    	            if (tparam->err != 0) tparam->err = -220;
	    	    PJ_LOG(3,("test", "error: cancelling invalid entry %d: %p",
	    	   		      idx, &tparam->entries[idx]));
	    	}
    	    } else if (prev_status == 1 && count == 0) {
    	        /* To make sure the flag has been cleared. */
    	        pj_thread_sleep(20);
    	    	if (pj_atomic_get(tparam->status[idx]) == 0) {
    	    	    /* Race condition with polling. */
    	    	    PJ_LOG(3,("test", "race cancel-poll %d: %p",
    	    	   	              idx, &tparam->entries[idx]));
    	    	} else {
    	            if (tparam->err != 0) tparam->err = -230;
    	    	    PJ_LOG(3,("test", "error: failed to cancel entry %d: %p",
    	    		   	      idx, &tparam->entries[idx]));
    	    	}
    	    }
    	    if (count > 0) {
    	        /* Make sure the flag has been set. */
    		while (pj_atomic_get(tparam->status[idx]) != 1)
    		    pj_thread_sleep(10);
    	    	pj_atomic_set(tparam->status[idx], 0);
    	    	pj_atomic_inc(tparam->n_cancel);
    	    }
    	} else if (task == POLLING) {
	    count = pj_timer_heap_poll(tparam->timer, NULL);
	    for (i = 0; i < count; i++) {
	        pj_atomic_inc_and_get(tparam->n_poll);
	    }
	} else {
	    pj_thread_sleep(10);
	}
    }
    PJ_LOG(4,("test", "...thread #%d (poll) stopped", t_idx));

    return 0;
}

/* Poll worker thread function. */
static int poll_worker(void *arg)
{
    struct thread_param *tparam = (struct thread_param*)arg;
    int idx;

    idx = pj_atomic_inc_and_get(tparam->idx);
    tparam->stat[idx].is_poll = PJ_TRUE;

    PJ_LOG(4,("test", "...thread #%d (poll) started", idx));
    while (!tparam->stopping) {
	unsigned count;
	count = pj_timer_heap_poll(tparam->timer, NULL);
	if (count > 0) {
	    /* Count expired entries */
	    PJ_LOG(5,("test", "...thread #%d called %d entries",
		      idx, count));
	    tparam->stat[idx].cnt += count;
	} else {
	    pj_thread_sleep(10);
	}
    }
    PJ_LOG(4,("test", "...thread #%d (poll) stopped", idx));

    return 0;
}

/* Cancel worker thread function. */
static int cancel_worker(void *arg)
{
    struct thread_param *tparam = (struct thread_param*)arg;
    int idx;

    idx = pj_atomic_inc_and_get(tparam->idx);
    tparam->stat[idx].is_poll = PJ_FALSE;

    PJ_LOG(4,("test", "...thread #%d (cancel) started", idx));
    while (!tparam->stopping) {
	int count;
	pj_timer_entry *e = &tparam->entries[pj_rand() % ST_ENTRY_COUNT];

	count = pj_timer_heap_cancel_if_active(tparam->timer, e, 2);
	if (count > 0) {
	    /* Count cancelled entries */
	    PJ_LOG(5,("test", "...thread #%d cancelled %d entries",
		      idx, count));
	    tparam->stat[idx].cnt += count;

	    /* Reschedule entry after some delay */
	    pj_thread_sleep(pj_rand() % 100);
	    st_schedule_entry(tparam->timer, e);
	}
    }
    PJ_LOG(4,("test", "...thread #%d (cancel) stopped", idx));

    return 0;
}

static int timer_stress_test(void)
{
    unsigned count = 0, n_sched = 0, n_cancel = 0, n_poll = 0;
    int i;
    pj_timer_entry *entries = NULL;
    pj_atomic_t	**entries_status = NULL;
    pj_grp_lock_t **grp_locks = NULL;
    pj_pool_t *pool;
    pj_timer_heap_t *timer = NULL;
    pj_lock_t *timer_lock;
    pj_status_t status;
    int err=0;
    pj_thread_t **stress_threads = NULL;
    pj_thread_t **poll_threads = NULL;
    pj_thread_t **cancel_threads = NULL;
    struct thread_param tparam = {0};
    pj_time_val now;
#if SIMULATE_CRASH
    pj_timer_entry *entry;
    pj_pool_t *tmp_pool;
    pj_time_val delay = {0};
#endif

    PJ_LOG(3,("test", "...Stress test"));

    pj_gettimeofday(&now);
    pj_srand(now.sec);

    pool = pj_pool_create( mem, NULL, 128, 128, NULL);
    if (!pool) {
	PJ_LOG(3,("test", "...error: unable to create pool"));
	err = -10;
	goto on_return;
    }

    /* Create timer heap.
     * Initially we only create a fraction of what's required,
     * to test the timer heap growth algorithm.
     */
    status = pj_timer_heap_create(pool, ST_ENTRY_COUNT/64, &timer);
    if (status != PJ_SUCCESS) {
        app_perror("...error: unable to create timer heap", status);
	err = -20;
	goto on_return;
    }

    /* Set recursive lock for the timer heap. */
    status = pj_lock_create_recursive_mutex( pool, "lock", &timer_lock);
    if (status != PJ_SUCCESS) {
        app_perror("...error: unable to create lock", status);
	err = -30;
	goto on_return;
    }
    pj_timer_heap_set_lock(timer, timer_lock, PJ_TRUE);

    /* Create group locks for the timer entry. */
    if (ST_ENTRY_GROUP_LOCK_COUNT) {
	grp_locks = (pj_grp_lock_t**)
		    pj_pool_calloc(pool, ST_ENTRY_GROUP_LOCK_COUNT,
				   sizeof(pj_grp_lock_t*));
    	tparam.grp_locks = grp_locks;
    }
    for (i=0; i<ST_ENTRY_GROUP_LOCK_COUNT; ++i) {    
	status = pj_grp_lock_create(pool, NULL, &grp_locks[i]);
	if (status != PJ_SUCCESS) {
	    app_perror("...error: unable to create group lock", status);
	    err = -40;
	    goto on_return;
	}
	pj_grp_lock_add_ref(grp_locks[i]);
    }

    /* Create and schedule timer entries */
    entries = (pj_timer_entry*)pj_pool_calloc(pool, ST_ENTRY_COUNT,
					      sizeof(*entries));
    if (!entries) {
	err = -50;
	goto on_return;
    }
    entries_status = (pj_atomic_t**)pj_pool_calloc(pool, ST_ENTRY_COUNT,
					      	   sizeof(*entries_status));
    if (!entries_status) {
	err = -55;
	goto on_return;
    }
    
    for (i=0; i<ST_ENTRY_COUNT; ++i) {
	pj_timer_entry_init(&entries[i], 0, &tparam, &st_entry_callback);

	status = pj_atomic_create(pool, -1, &entries_status[i]);
	if (status != PJ_SUCCESS) {
	    err = -60;
	    goto on_return;
	}
	pj_atomic_set(entries_status[i], 0);

	/* For randomized test, we schedule the entry inside the thread */
	if (!ST_STRESS_THREAD_COUNT) {
	    status = st_schedule_entry(timer, &entries[i]);
	    if (status != PJ_SUCCESS) {
	        app_perror("...error: unable to schedule entry", status);
	        err = -60;
	        goto on_return;
	    }
	}
    }

    tparam.stopping = PJ_FALSE;
    tparam.timer = timer;
    tparam.entries = entries;
    tparam.status = entries_status;
    status = pj_atomic_create(pool, -1, &tparam.idx);
    if (status != PJ_SUCCESS) {
	app_perror("...error: unable to create atomic", status);
	err = -70;
	goto on_return;
    }
    status = pj_atomic_create(pool, -1, &tparam.n_sched);
    pj_assert (status == PJ_SUCCESS);
    pj_atomic_set(tparam.n_sched, 0);
    status = pj_atomic_create(pool, -1, &tparam.n_cancel);
    pj_assert (status == PJ_SUCCESS);
    pj_atomic_set(tparam.n_cancel, 0);
    status = pj_atomic_create(pool, -1, &tparam.n_poll);
    pj_assert (status == PJ_SUCCESS);
    pj_atomic_set(tparam.n_poll, 0);

    /* Start stress worker threads */
    if (ST_STRESS_THREAD_COUNT) {
	stress_threads = (pj_thread_t**)
		        pj_pool_calloc(pool, ST_STRESS_THREAD_COUNT,
				       sizeof(pj_thread_t*));
    }
    for (i=0; i<ST_STRESS_THREAD_COUNT; ++i) {
	status = pj_thread_create( pool, "poll", &stress_worker, &tparam,
				   0, 0, &stress_threads[i]);
	if (status != PJ_SUCCESS) {
	    app_perror("...error: unable to create stress thread", status);
	    err = -75;
	    goto on_return;
	}
    }

    /* Start poll worker threads */
    if (ST_POLL_THREAD_COUNT) {
	poll_threads = (pj_thread_t**)
		        pj_pool_calloc(pool, ST_POLL_THREAD_COUNT,
				       sizeof(pj_thread_t*));
    }
    for (i=0; i<ST_POLL_THREAD_COUNT; ++i) {
	status = pj_thread_create( pool, "poll", &poll_worker, &tparam,
				   0, 0, &poll_threads[i]);
	if (status != PJ_SUCCESS) {
	    app_perror("...error: unable to create poll thread", status);
	    err = -80;
	    goto on_return;
	}
    }

    /* Start cancel worker threads */
    if (ST_CANCEL_THREAD_COUNT) {
	cancel_threads = (pj_thread_t**)
		          pj_pool_calloc(pool, ST_CANCEL_THREAD_COUNT,
				         sizeof(pj_thread_t*));
    }
    for (i=0; i<ST_CANCEL_THREAD_COUNT; ++i) {
	status = pj_thread_create( pool, "cancel", &cancel_worker, &tparam,
				   0, 0, &cancel_threads[i]);
	if (status != PJ_SUCCESS) {
	    app_perror("...error: unable to create cancel thread", status);
	    err = -90;
	    goto on_return;
	}
    }

#if SIMULATE_CRASH
    tmp_pool = pj_pool_create( mem, NULL, 4096, 128, NULL);
    pj_assert(tmp_pool);
    entry = (pj_timer_entry*)pj_pool_calloc(tmp_pool, 1, sizeof(*entry));
    pj_assert(entry);
    pj_timer_entry_init(entry, 0, &tparam, &dummy_callback);
    delay.sec = 6;
    status = pj_timer_heap_schedule(timer, entry, &delay);
    pj_assert(status == PJ_SUCCESS);
    pj_thread_sleep(1000);
    PJ_LOG(3,("test", "...Releasing timer entry %p without cancelling it",
    		      entry));
    pj_pool_secure_release(&tmp_pool);
    //pj_pool_release(tmp_pool);
    //pj_memset(tmp_pool, 128, 4096);
#endif

    /* Wait */
    pj_thread_sleep(ST_DURATION);

on_return:
    
    PJ_LOG(3,("test", "...Cleaning up resources"));
    tparam.stopping = PJ_TRUE;
    
    for (i=0; i<ST_STRESS_THREAD_COUNT; ++i) {
	if (!stress_threads[i])
	    continue;
	pj_thread_join(stress_threads[i]);
	pj_thread_destroy(stress_threads[i]);
    }

    for (i=0; i<ST_POLL_THREAD_COUNT; ++i) {
	if (!poll_threads[i])
	    continue;
	pj_thread_join(poll_threads[i]);
	pj_thread_destroy(poll_threads[i]);
    }
    
    for (i=0; i<ST_CANCEL_THREAD_COUNT; ++i) {
	if (!cancel_threads[i])
	    continue;
	pj_thread_join(cancel_threads[i]);
	pj_thread_destroy(cancel_threads[i]);
    }
    
    for (i=0; i<ST_POLL_THREAD_COUNT+ST_CANCEL_THREAD_COUNT; ++i) {
	PJ_LOG(3,("test", "...Thread #%d (%s) executed %d entries",
		  i, (tparam.stat[i].is_poll? "poll":"cancel"),
		  tparam.stat[i].cnt));
    }

    for (i=0; i<ST_ENTRY_COUNT; ++i) {
	count += pj_timer_heap_cancel_if_active(timer, &entries[i], 10);
	if (entries_status)
	    pj_atomic_destroy(entries_status[i]);
    }

    for (i=0; i<ST_ENTRY_GROUP_LOCK_COUNT; ++i) {
	/* Ref count must be equal to 1 */
	if (pj_grp_lock_get_ref(grp_locks[i]) != 1) {
	    pj_assert(!"Group lock ref count must be equal to 1");
	    if (!err) err = -100;
	}
	pj_grp_lock_dec_ref(grp_locks[i]);
    }

    if (timer)
	pj_timer_heap_destroy(timer);

    PJ_LOG(3,("test", "Total memory of timer heap: %d",
    		      pj_timer_heap_mem_size(ST_ENTRY_COUNT)));

    if (tparam.idx)
	pj_atomic_destroy(tparam.idx);
    if (tparam.n_sched) {
        n_sched = pj_atomic_get(tparam.n_sched);
        PJ_LOG(3,("test", "Total number of scheduled entries: %d", n_sched));
	pj_atomic_destroy(tparam.n_sched);
    }
    if (tparam.n_cancel) {
        n_cancel = pj_atomic_get(tparam.n_cancel);
        PJ_LOG(3,("test", "Total number of cancelled entries: %d", n_cancel));
	pj_atomic_destroy(tparam.n_cancel);
    }
    if (tparam.n_poll) {
        n_poll = pj_atomic_get(tparam.n_poll);
        PJ_LOG(3,("test", "Total number of polled entries: %d", n_poll));
	pj_atomic_destroy(tparam.n_poll);
    }
    PJ_LOG(3,("test", "Number of remaining active entries: %d", count));
    if (n_sched) {
        pj_bool_t match = PJ_TRUE;

#if SIMULATE_CRASH
	n_sched++;
#endif
        if (n_sched != (n_cancel + n_poll + count)) {
            if (tparam.err != 0) tparam.err = -250;
            match = PJ_FALSE;
        }
    	PJ_LOG(3,("test", "Scheduled = cancelled + polled + remaining?: %s",
    			  (match? "yes": "no")));
    }

    pj_pool_safe_release(&pool);

    return (err? err: tparam.err);
}

static int get_random_delay()
{
    return pj_rand() % BT_ENTRY_COUNT;
}

static int get_next_delay(int delay)
{
    return ++delay;
}

typedef enum BENCH_TEST_TYPE {
    RANDOM_SCH = 0,
    RANDOM_CAN = 1,
    INCREMENT_SCH = 2,
    INCREMENT_CAN = 3
} BENCH_TEST_TYPE;

static char *get_test_name(BENCH_TEST_TYPE test_type) {
    switch (test_type) {
    case RANDOM_SCH:
    case INCREMENT_SCH:
	return "schedule";
    case RANDOM_CAN:
    case INCREMENT_CAN:
	return "cancel";
    }
    return "undefined";
}

static void *get_format_num(unsigned n, char *out)
{
    int c;
    char buf[64];
    char *p;

    pj_ansi_snprintf(buf, 64, "%d", n);
    c = 2 - pj_ansi_strlen(buf) % 3;
    for (p = buf; *p != 0; ++p) {
       *out++ = *p;
       if (c == 1) {
           *out++ = ',';
       }
       c = (c + 1) % 3;
    }
    *--out = 0;
    return out;
}

static void print_bench(BENCH_TEST_TYPE test_type, pj_timestamp time_freq,
			pj_timestamp time_start, int start_idx, int end_idx)
{
    char start_idx_str[64];
    char end_idx_str[64];
    char num_req_str[64];
    unsigned num_req;
    pj_timestamp t2;

    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &time_start);

    num_req = (unsigned)(time_freq.u64 * (end_idx-start_idx) / t2.u64);
    if (test_type == RANDOM_CAN || test_type == INCREMENT_CAN) {
	start_idx = BT_ENTRY_COUNT - start_idx;
	end_idx = BT_ENTRY_COUNT - end_idx;
    }
    get_format_num(start_idx, start_idx_str);
    get_format_num(end_idx, end_idx_str);
    get_format_num(num_req, num_req_str);

    PJ_LOG(3, (THIS_FILE, "    Entries %s-%s: %s %s ent/sec",
	       start_idx_str, end_idx_str, get_test_name(test_type),
	       num_req_str));
}

static int bench_test(pj_timer_heap_t *timer,
		      pj_timer_entry *entries,
		      pj_timestamp freq,
		      BENCH_TEST_TYPE test_type)
{
    pj_timestamp t1;
    unsigned mult = BT_ENTRY_SHOW_START;
    int i, j;

    pj_get_timestamp(&t1);
    /*Schedule random entry.*/
    for (i=0, j=0; j < BT_ENTRY_COUNT; ++j) {
	pj_time_val delay = { 0 };
	pj_status_t status;

	if (test_type == RANDOM_SCH || test_type == INCREMENT_SCH) {
	    if (test_type == RANDOM_SCH)
		delay.msec = get_random_delay();
	    else
		delay.msec = get_next_delay(delay.msec);

	    pj_timer_entry_init(&entries[j], 0, NULL, &dummy_callback);

	    status = pj_timer_heap_schedule(timer, &entries[j], &delay);
	    if (status != PJ_SUCCESS) {
		app_perror("...error: unable to schedule timer entry", status);
		return -50;
	    }
	} else if (test_type == RANDOM_CAN || test_type == INCREMENT_CAN) {
	    unsigned num_ent = pj_timer_heap_cancel(timer, &entries[j]);
	    if (num_ent == 0) {
		PJ_LOG(3, ("test", "...error: unable to cancel timer entry"));
		return -60;
	    }
	} else {
	    return -70;
	}

	if (j && (j % mult) == 0) {
	    print_bench(test_type, freq, t1, i, j);

	    i = j+1;
	    pj_get_timestamp(&t1);
	    mult *= BT_ENTRY_SHOW_MULT;
	}
    }
    if (j > 0 && ((j-1) % mult != 0)) {
	print_bench(test_type, freq, t1, i, j);
    }
    return 0;
}

static int timer_bench_test(void)
{
    pj_pool_t *pool = NULL;
    pj_timer_heap_t *timer = NULL;
    pj_status_t status;
    int err=0;
    pj_timer_entry *entries = NULL;
    pj_timestamp freq;
    int i;

    PJ_LOG(3,("test", "...Benchmark test"));

    status = pj_get_timestamp_freq(&freq);
    if (status != PJ_SUCCESS) {
	PJ_LOG(3,("test", "...error: unable to get timestamp freq"));
	err = -10;
	goto on_return;
    }

    pool = pj_pool_create( mem, NULL, 128, 128, NULL);
    if (!pool) {
	PJ_LOG(3,("test", "...error: unable to create pool"));
	err = -20;
	goto on_return;
    }

    /* Create timer heap.*/
    status = pj_timer_heap_create(pool, BT_ENTRY_COUNT/64, &timer);
    if (status != PJ_SUCCESS) {
        app_perror("...error: unable to create timer heap", status);
	err = -30;
	goto on_return;
    }

    /* Create and schedule timer entries */
    entries = (pj_timer_entry*)pj_pool_calloc(pool, BT_ENTRY_COUNT,
					      sizeof(*entries));
    if (!entries) {
	err = -40;
	goto on_return;
    }

    PJ_LOG(3,("test", "....random scheduling/cancelling test.."));
    for (i = 0; i < BT_REPEAT_RANDOM_TEST; ++i) {
	PJ_LOG(3,("test", "    test %d of %d..", i+1, BT_REPEAT_RANDOM_TEST));
	err = bench_test(timer, entries, freq, RANDOM_SCH);
	if (err < 0)
	    goto on_return;

	err = bench_test(timer, entries, freq, RANDOM_CAN);
	if (err < 0)
	    goto on_return;
    }

    PJ_LOG(3,("test", "....increment scheduling/cancelling test.."));
    for (i = 0; i < BT_REPEAT_INC_TEST; ++i) {
	PJ_LOG(3,("test", "    test %d of %d..", i+1, BT_REPEAT_INC_TEST));
	err = bench_test(timer, entries, freq, INCREMENT_SCH);
	if (err < 0)
	    goto on_return;

	err = bench_test(timer, entries, freq, INCREMENT_CAN);
	if (err < 0)
	    goto on_return;
    }
 on_return:
    PJ_LOG(3,("test", "...Cleaning up resources"));
    if (pool)
	pj_pool_safe_release(&pool);
    return err;
}

int timer_test()
{
    int rc;

    rc = test_timer_heap();
    if (rc != 0)
	return rc;

    rc = timer_stress_test();
    if (rc != 0)
	return rc;

#if WITH_BENCHMARK
    rc = timer_bench_test();
    if (rc != 0)
	return rc;
#endif

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_timer_test;
#endif	/* INCLUDE_TIMER_TEST */



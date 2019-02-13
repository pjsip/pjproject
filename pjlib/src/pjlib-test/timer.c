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
 * Test scenario:
 * 1. Create and schedule a number of timer entries.
 * 2. Start threads for polling (simulating normal worker thread).
 *    Each expired entry will try to cancel and re-schedule itself
 *    from within the callback.
 * 3. Start threads for cancelling random entries. Each successfully
 *    cancelled entry will be re-scheduled after some random delay.
 */
#define ST_POLL_THREAD_COUNT	    10
#define ST_CANCEL_THREAD_COUNT	    10

#define ST_ENTRY_COUNT		    1000
#define ST_ENTRY_MAX_TIMEOUT_MS	    100

/* Number of group lock, may be zero, shared by timer entries, group lock
 * can be useful to evaluate poll vs cancel race condition scenario, i.e:
 * each group lock must have ref count==1 at the end of the test, otherwise
 * assertion will raise.
 */
#define ST_ENTRY_GROUP_LOCK_COUNT   1


struct thread_param
{
    pj_timer_heap_t *timer;
    pj_bool_t stopping;
    pj_timer_entry *entries;

    pj_atomic_t *idx;
    struct {
	pj_bool_t is_poll;
	unsigned cnt;
    } stat[ST_POLL_THREAD_COUNT + ST_CANCEL_THREAD_COUNT];
};

static pj_status_t st_schedule_entry(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    pj_time_val delay = {0};
    pj_grp_lock_t *grp_lock = (pj_grp_lock_t*)e->user_data;
    pj_status_t status;

    delay.msec = pj_rand() % ST_ENTRY_MAX_TIMEOUT_MS;
    pj_time_val_normalize(&delay);
    status = pj_timer_heap_schedule_w_grp_lock(ht, e, &delay, 1, grp_lock);
    return status;
}

static void st_entry_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    /* try to cancel this */
    pj_timer_heap_cancel_if_active(ht, e, 10);
    
    /* busy doing something */
    pj_thread_sleep(pj_rand() % 50);

    /* reschedule entry */
    st_schedule_entry(ht, e);
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
    int i;
    pj_timer_entry *entries = NULL;
    pj_grp_lock_t **grp_locks = NULL;
    pj_pool_t *pool;
    pj_timer_heap_t *timer = NULL;
    pj_lock_t *timer_lock;
    pj_status_t status;
    int err=0;
    pj_thread_t **poll_threads = NULL;
    pj_thread_t **cancel_threads = NULL;
    struct thread_param tparam = {0};
    pj_time_val now;

    PJ_LOG(3,("test", "...Stress test"));

    pj_gettimeofday(&now);
    pj_srand(now.sec);

    pool = pj_pool_create( mem, NULL, 128, 128, NULL);
    if (!pool) {
	PJ_LOG(3,("test", "...error: unable to create pool"));
	err = -10;
	goto on_return;
    }

    /* Create timer heap */
    status = pj_timer_heap_create(pool, ST_ENTRY_COUNT, &timer);
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

    for (i=0; i<ST_ENTRY_COUNT; ++i) {
	pj_grp_lock_t *grp_lock = NULL;

	if (ST_ENTRY_GROUP_LOCK_COUNT && pj_rand() % 10) {
	    /* About 90% of entries should have group lock */
	    grp_lock = grp_locks[pj_rand() % ST_ENTRY_GROUP_LOCK_COUNT];
	}

	pj_timer_entry_init(&entries[i], 0, grp_lock, &st_entry_callback);
	status = st_schedule_entry(timer, &entries[i]);
	if (status != PJ_SUCCESS) {
	    app_perror("...error: unable to schedule entry", status);
	    err = -60;
	    goto on_return;
	}
    }

    tparam.stopping = PJ_FALSE;
    tparam.timer = timer;
    tparam.entries = entries;
    status = pj_atomic_create(pool, -1, &tparam.idx);
    if (status != PJ_SUCCESS) {
	app_perror("...error: unable to create atomic", status);
	err = -70;
	goto on_return;
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

    /* Wait 30s */
    pj_thread_sleep(30*1000);


on_return:
    
    PJ_LOG(3,("test", "...Cleaning up resources"));
    tparam.stopping = PJ_TRUE;
    
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
	pj_timer_heap_cancel_if_active(timer, &entries[i], 10);
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

    if (tparam.idx)
	pj_atomic_destroy(tparam.idx);

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

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_timer_test;
#endif	/* INCLUDE_TIMER_TEST */



/* $Header: /pjproject/pjlib/src/test/timer_test.cpp 4     5/12/05 9:53p Bennylp $ */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "libpj_test.h"
#include <pj/timer.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <stdio.h>	// puts(), printf()
#include <stdlib.h>	// srand(), rand()
#include <time.h>	// time()

#define LOOP		256
#define MIN_COUNT	1024
#define MAX_COUNT	(LOOP * MIN_COUNT)
#define DELAY		(MAX_COUNT / 32000)

static void timer_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    PJ_UNUSED_ARG(ht);
    PJ_UNUSED_ARG(e);
}

static int test()
{
    int i, j;
    pj_timer_entry *entry;
    pj_pool_t *pool;
    pj_timer_heap_t *timer;
    pj_time_val delay;
    int err=0;
    unsigned count;

    pool = (*mem->create_pool)( mem, NULL, MAX_COUNT*PJ_TIMER_ENTRY_SIZE + PJ_TIMER_HEAP_SIZE + PJ_POOL_SIZE, 0, NULL);
    entry = (pj_timer_entry*)pj_pool_calloc(pool, MAX_COUNT, sizeof(*entry));
    for (i=0; i<MAX_COUNT; ++i) {
	entry[i].cb = &timer_callback;
    }
    timer = pj_timer_heap_create(pool, MAX_COUNT, 0);

    puts("");
    count = MIN_COUNT;
    for (i=0; i<LOOP; ++i) {
	int early = 0;
	int done=0;
	int cancelled=0;
	int rc;
	pj_hr_timestamp t1, t2, t_sched, t_cancel, t_poll;

	//printf("...test %d of %d.. ", i, LOOP);
	printf("...");

	srand(time(NULL));
	t_sched.u32.lo = t_cancel.u32.lo = t_poll.u32.lo = 0;

	// Register timers
	for (j=0; j<(int)count; ++j) {
	    delay.sec = rand() % DELAY;
	    delay.msec = rand() % 1000;

	    // Schedule timer
	    pj_hr_gettimestamp(&t1);
	    pj_timer_heap_schedule(timer, &entry[j], &delay);
	    pj_hr_gettimestamp(&t2);

	    t_sched.u32.lo += (t2.u32.lo - t1.u32.lo);

	    // Poll timers.
	    pj_hr_gettimestamp(&t1);
	    rc = pj_timer_heap_poll(timer, NULL);
	    pj_hr_gettimestamp(&t2);
	    if (rc > 0) {
		t_poll.u32.lo += (t2.u32.lo - t1.u32.lo);
		early += rc;
	    }
	}

	// Set the time where all timers should finish
	pj_time_val expire, now;
	pj_gettimeofday(&expire);
	delay.sec = DELAY; 
	delay.msec = 0;
	PJ_TIME_VAL_ADD(expire, delay);

	// Wait unfil all timers finish, cancel some of them.
	do {
	    int index = rand() % count;
	    pj_hr_gettimestamp(&t1);
	    rc = pj_timer_heap_cancel(timer, &entry[index]);
	    pj_hr_gettimestamp(&t2);
	    if (rc > 0) {
		cancelled += rc;
		t_cancel.u32.lo += (t2.u32.lo - t1.u32.lo);
	    }

	    pj_gettimeofday(&now);

	    pj_hr_gettimestamp(&t1);
	    rc = pj_timer_heap_poll(timer, NULL);
	    pj_hr_gettimestamp(&t2);
	    if (rc > 0) {
		done += rc;
		t_poll.u32.lo += (t2.u32.lo - t1.u32.lo);
	    }

	} while (PJ_TIME_VAL_LTE(now, expire) && pj_timer_heap_count(timer) > 0);

	if (pj_timer_heap_count(timer)) {
	    printf("ERROR: %d timers left\n", pj_timer_heap_count(timer));
	    ++err;
	}
	t_sched.u32.lo /= count; 
	t_cancel.u32.lo /= count;
	t_poll.u32.lo /= count;
	printf("ok (count:%d, early:%d, cancelled:%d, sched:%d, cancel:%d poll:%d)\n", 
		count, early, cancelled, t_sched.u32.lo, t_cancel.u32.lo,
		t_poll.u32.lo);

	count = count * 2;
	if (count > MAX_COUNT)
	    break;
    }

    pj_pool_release(pool);
    return err;
}


int timer_test()
{
    return test();
}


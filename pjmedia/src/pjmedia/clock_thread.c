/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/clock.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/os.h>
#include <pj/pool.h>


/*
 * Implementation of media clock with OS thread.
 */


struct pjmedia_clock
{
    pj_timestamp	     freq;
    pj_timestamp	     interval;
    pj_timestamp	     next_tick;
    pj_timestamp	     timestamp;
    unsigned		     samples_per_frame;
    unsigned		     options;
    pjmedia_clock_callback  *cb;
    void		    *user_data;
    pj_thread_t		    *thread;
    pj_bool_t		     running;
    pj_bool_t		     quitting;
};


static int clock_thread(void *arg);


/*
 * Create media clock.
 */
PJ_DEF(pj_status_t) pjmedia_clock_create( pj_pool_t *pool,
					  unsigned clock_rate,
					  unsigned samples_per_frame,
					  unsigned options,
					  pjmedia_clock_callback *cb,
					  void *user_data,
					  pjmedia_clock **p_clock)
{
    pjmedia_clock *clock;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && clock_rate && samples_per_frame && p_clock,
		     PJ_EINVAL);

    clock = pj_pool_alloc(pool, sizeof(pjmedia_clock));

    
    status = pj_get_timestamp_freq(&clock->freq);
    if (status != PJ_SUCCESS)
	return status;

    clock->interval.u64 = samples_per_frame * clock->freq.u64 / clock_rate;
    clock->next_tick.u64 = 0;
    clock->timestamp.u64 = 0;
    clock->samples_per_frame = samples_per_frame;
    clock->options = options;
    clock->cb = cb;
    clock->user_data = user_data;
    clock->thread = NULL;
    clock->running = PJ_FALSE;
    clock->quitting = PJ_FALSE;
    
    status = pj_thread_create(pool, "clock", &clock_thread, clock,
			      0, 0, &clock->thread);
    if (status != PJ_SUCCESS)
	return status;


    *p_clock = clock;

    return PJ_SUCCESS;
}


/*
 * Start the clock. 
 */
PJ_DEF(pj_status_t) pjmedia_clock_start(pjmedia_clock *clock)
{
    pj_timestamp now;
    pj_status_t status;

    PJ_ASSERT_RETURN(clock != NULL, PJ_EINVAL);

    status = pj_get_timestamp(&now);
    if (status != PJ_SUCCESS)
	return status;

    clock->next_tick.u64 = now.u64 + clock->interval.u64;
    clock->running = PJ_TRUE;

    return status;
}


/*
 * Stop the clock. 
 */
PJ_DEF(pj_status_t) pjmedia_clock_stop(pjmedia_clock *clock)
{
    PJ_ASSERT_RETURN(clock != NULL, PJ_EINVAL);

    clock->running = PJ_FALSE;

    return PJ_SUCCESS;
}


/*
 * Poll the clock. 
 */
PJ_DEF(pj_bool_t) pjmedia_clock_wait( pjmedia_clock *clock,
				      pj_bool_t wait,
				      pj_timestamp *ts)
{
    pj_timestamp now;
    pj_status_t status;

    PJ_ASSERT_RETURN(clock != NULL, PJ_FALSE);
    PJ_ASSERT_RETURN((clock->options & PJMEDIA_CLOCK_NO_ASYNC) != 0,
		     PJ_FALSE);
    PJ_ASSERT_RETURN(clock->running, PJ_FALSE);

    status = pj_get_timestamp(&now);
    if (status != PJ_SUCCESS)
	return PJ_FALSE;

    /* Wait for the next tick to happen */
    if (now.u64 < clock->next_tick.u64) {
	unsigned msec;

	if (!wait)
	    return PJ_FALSE;

	msec = pj_elapsed_msec(&now, &clock->next_tick);
	pj_thread_sleep(msec);
    }

    /* Call callback, if any */
    if (clock->cb)
	(*clock->cb)(&clock->timestamp, clock->user_data);

    /* Report timestamp to caller */
    if (ts)
	ts->u64 = clock->timestamp.u64;

    /* Increment timestamp */
    clock->timestamp.u64 += clock->samples_per_frame;

    /* Calculate next tick */
    clock->next_tick.u64 += clock->interval.u64;

    /* Done */
    return PJ_TRUE;
}


/*
 * Clock thread
 */
static int clock_thread(void *arg)
{
    pj_timestamp now;
    pjmedia_clock *clock = arg;

    /* Get the first tick */
    pj_get_timestamp(&clock->next_tick);
    clock->next_tick.u64 += clock->interval.u64;


    while (!clock->quitting) {

	pj_get_timestamp(&now);

	/* Wait for the next tick to happen */
	if (now.u64 < clock->next_tick.u64) {
	    unsigned msec;
	    msec = pj_elapsed_msec(&now, &clock->next_tick);
	    pj_thread_sleep(msec);
	}

	/* Skip if not running */
	if (!clock->running)
	    continue;

	/* Call callback, if any */
	if (clock->cb)
	    (*clock->cb)(&clock->timestamp, clock->user_data);

	/* Increment timestamp */
	clock->timestamp.u64 += clock->samples_per_frame;

	/* Calculate next tick */
	clock->next_tick.u64 += clock->interval.u64;


    }

    return 0;
}


/*
 * Destroy the clock. 
 */
PJ_DEF(pj_status_t) pjmedia_clock_destroy(pjmedia_clock *clock)
{
    PJ_ASSERT_RETURN(clock != NULL, PJ_EINVAL);

    clock->running = PJ_FALSE;
    clock->quitting = PJ_TRUE;

    if (clock->thread) {
	pj_thread_join(clock->thread);
	pj_thread_destroy(clock->thread);
	clock->thread = NULL;
    }

    return PJ_SUCCESS;
}






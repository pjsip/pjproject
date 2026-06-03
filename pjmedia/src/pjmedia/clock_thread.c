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
#include <pjmedia/clock.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/compat/high_precision.h>

/* API: Init clock source */
PJ_DEF(pj_status_t) pjmedia_clock_src_init( pjmedia_clock_src *clocksrc,
                                            pjmedia_type media_type,
                                            unsigned clock_rate,
                                            unsigned ptime_usec )
{
    PJ_ASSERT_RETURN(clocksrc, PJ_EINVAL);

    clocksrc->media_type = media_type;
    clocksrc->clock_rate = clock_rate;
    clocksrc->ptime_usec = ptime_usec;
    pj_set_timestamp32(&clocksrc->timestamp, 0, 0);
    pj_get_timestamp(&clocksrc->last_update);

    return PJ_SUCCESS;
}

/* API: Update clock source */
PJ_DEF(pj_status_t) pjmedia_clock_src_update( pjmedia_clock_src *clocksrc,
                                              const pj_timestamp *timestamp )
{
    PJ_ASSERT_RETURN(clocksrc, PJ_EINVAL);

    if (timestamp)
        pj_memcpy(&clocksrc->timestamp, timestamp, sizeof(pj_timestamp));
    pj_get_timestamp(&clocksrc->last_update);

    return PJ_SUCCESS;
}

/* API: Get clock source's current timestamp */
PJ_DEF(pj_status_t)
pjmedia_clock_src_get_current_timestamp( const pjmedia_clock_src *clocksrc,
                                         pj_timestamp *timestamp)
{
    pj_timestamp now;
    unsigned elapsed_ms;
    
    PJ_ASSERT_RETURN(clocksrc && timestamp, PJ_EINVAL);

    pj_get_timestamp(&now);
    elapsed_ms = pj_elapsed_msec(&clocksrc->last_update, &now);
    pj_memcpy(timestamp, &clocksrc->timestamp, sizeof(pj_timestamp));
    pj_add_timestamp32(timestamp, elapsed_ms * clocksrc->clock_rate / 1000);

    return PJ_SUCCESS;
}

/* API: Get clock source's time (in ms) */
PJ_DEF(pj_uint32_t)
pjmedia_clock_src_get_time_msec( const pjmedia_clock_src *clocksrc )
{
    pj_timestamp ts;

    if (pjmedia_clock_src_get_current_timestamp(clocksrc, &ts) != PJ_SUCCESS)
        return 0;

#if PJ_HAS_INT64
    if (ts.u64 > PJ_UINT64(0x3FFFFFFFFFFFFF))
        return (pj_uint32_t)(ts.u64 / clocksrc->clock_rate * 1000);
    else
        return (pj_uint32_t)(ts.u64 * 1000 / clocksrc->clock_rate);
#elif PJ_HAS_FLOATING_POINT
    return (pj_uint32_t)((1.0 * ts.u32.hi * 0xFFFFFFFFUL + ts.u32.lo)
                         * 1000.0 / clocksrc->clock_rate);
#else
    if (ts.u32.lo > 0x3FFFFFUL)
        return (pj_uint32_t)(0xFFFFFFFFUL / clocksrc->clock_rate * ts.u32.hi 
                             * 1000UL + ts.u32.lo / clocksrc->clock_rate *
                             1000UL);
    else
        return (pj_uint32_t)(0xFFFFFFFFUL / clocksrc->clock_rate * ts.u32.hi 
                             * 1000UL + ts.u32.lo * 1000UL /
                             clocksrc->clock_rate);
#endif
}


/*
 * Implementation of media clock with OS thread.
 */

struct pjmedia_clock
{
    pj_pool_t               *pool;
    pj_timestamp             freq;
    pj_timestamp             interval;
    pj_timestamp             next_tick;
    pj_timestamp             timestamp;
    unsigned                 timestamp_inc;
    unsigned                 options;
    pj_uint64_t              max_jump;
    pjmedia_clock_callback  *cb;
    void                    *user_data;
    pj_thread_t             *thread;
    pj_bool_t                running;
    pj_bool_t                quitting;
    pj_lock_t               *lock;
    /* Serializes pjmedia_clock_stop() and pjmedia_clock_destroy()
     * across concurrent callers. Cannot be the same as `lock` above,
     * because `lock` is held by the clock thread inside the callback
     * (see clock_thread()), and stop/destroy must be allowed to run
     * while the thread is mid-callback. */
    pj_mutex_t              *destroy_lock;
};


static int clock_thread(void *arg);

#define MAX_JUMP_MSEC   500
#define USEC_IN_SEC     (pj_uint64_t)1000000

/*
 * Create media clock.
 */
PJ_DEF(pj_status_t) pjmedia_clock_create( pj_pool_t *pool,
                                          unsigned clock_rate,
                                          unsigned channel_count,
                                          unsigned samples_per_frame,
                                          unsigned options,
                                          pjmedia_clock_callback *cb,
                                          void *user_data,
                                          pjmedia_clock **p_clock)
{
    pjmedia_clock_param param;

    param.usec_interval = (unsigned)(samples_per_frame * USEC_IN_SEC /
                                     channel_count / clock_rate);
    param.clock_rate = clock_rate;
    return pjmedia_clock_create2(pool, &param, options, cb,
                                 user_data, p_clock);
}

PJ_DEF(pj_status_t) pjmedia_clock_create2(pj_pool_t *pool,
                                          const pjmedia_clock_param *param,
                                          unsigned options,
                                          pjmedia_clock_callback *cb,
                                          void *user_data,
                                          pjmedia_clock **p_clock)
{
    pjmedia_clock *clock;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && param->usec_interval && param->clock_rate &&
                     p_clock, PJ_EINVAL);

    clock = PJ_POOL_ALLOC_T(pool, pjmedia_clock);
    clock->pool = pj_pool_create(pool->factory, "clock%p", 512, 512, NULL);

    status = pj_get_timestamp_freq(&clock->freq);
    if (status != PJ_SUCCESS)
        return status;

    clock->interval.u64 = param->usec_interval * clock->freq.u64 /
                          USEC_IN_SEC;
    clock->next_tick.u64 = 0;
    clock->timestamp.u64 = 0;
    clock->max_jump = MAX_JUMP_MSEC * clock->freq.u64 / 1000;
    clock->timestamp_inc = (unsigned)(param->usec_interval *
                                      param->clock_rate /
                                      (unsigned)USEC_IN_SEC);
    clock->options = options;
    clock->cb = cb;
    clock->user_data = user_data;
    clock->thread = NULL;
    clock->running = PJ_FALSE;
    clock->quitting = PJ_FALSE;
    clock->destroy_lock = NULL;

    /* I don't think we need a mutex, so we'll use null. */
    status = pj_lock_create_null_mutex(pool, "clock", &clock->lock);
    if (status != PJ_SUCCESS)
        return status;

    /* But we *do* need a real mutex to serialize stop/destroy: under
     * stress, multiple threads can land in pjmedia_clock_stop() on the
     * same clock concurrently (e.g. cbar_stream_stop reached from both
     * vid_port handle_format_change and free_vid_win). Without this,
     * two pthread_join() calls race the same descriptor — POSIX
     * rejects the second one with EINVAL, but on Windows the
     * underlying WaitForSingleObject() permits multiple waiters and
     * *both* return success, leading to double pj_thread_destroy()
     * and double pj_pool_reset().
     *
     * Allocated from clock->pool — not the caller's pool — so the
     * lock and the memory it guards share one lifetime and are torn
     * down together in pjmedia_clock_destroy() by a single owner.
     * pj_mutex_destroy(destroy_lock) runs immediately before
     * pj_pool_safe_release(&clock->pool) there. This relies on the
     * caller not issuing concurrent destroy on the same handle —
     * which the higher-layer is_destroying flag + dec_vid_win under
     * PJSUA_LOCK guarantees for the pjsua video path. */
    status = pj_mutex_create_recursive(clock->pool, "clockdestroy",
                                       &clock->destroy_lock);
    if (status != PJ_SUCCESS) {
        pj_lock_destroy(clock->lock);
        clock->lock = NULL;
        pj_pool_safe_release(&clock->pool);
        return status;
    }

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

    if (clock->running)
        return PJ_SUCCESS;

    status = pj_get_timestamp(&now);
    if (status != PJ_SUCCESS)
        return status;

    clock->next_tick.u64 = now.u64 + clock->interval.u64;
    clock->running = PJ_TRUE;
    clock->quitting = PJ_FALSE;

    if ((clock->options & PJMEDIA_CLOCK_NO_ASYNC) == 0) {
        if (clock->thread) {
            /* This is probably the leftover thread that failed to
             * be cleaned up during the last clock stoppage.
             */
            pj_thread_destroy(clock->thread);
            clock->thread = NULL;
        }

        status = pj_thread_create(clock->pool, "clock", &clock_thread, clock,
                                  0, 0, &clock->thread);
        if (status != PJ_SUCCESS) {
            clock->running = PJ_FALSE;
            return status;
        }
    }

    return PJ_SUCCESS;
}


/*
 * Stop the clock. 
 */
PJ_DEF(pj_status_t) pjmedia_clock_stop(pjmedia_clock *clock)
{
    pj_status_t ret = PJ_SUCCESS;

    PJ_ASSERT_RETURN(clock != NULL, PJ_EINVAL);

    /* Serialize in-flight stop/destroy calls within the clock's
     * lifetime. The caller contract in clock.h bars stop/destroy
     * after a successful destroy (which tears down this lock). */
    pj_mutex_lock(clock->destroy_lock);

    clock->running = PJ_FALSE;
    clock->quitting = PJ_TRUE;

    if (clock->thread) {
        pj_status_t status = pj_thread_join(clock->thread);
        if (status == PJ_SUCCESS) {
            pj_thread_destroy(clock->thread);
            clock->thread = NULL;
            pj_pool_reset(clock->pool);
        } else if (status == PJ_ECANCELLED) {
            /* We are called from the clock thread itself; we cannot
             * join ourselves. Leave clock->quitting set so the thread
             * will exit on the next loop iteration. The caller must
             * NOT release the clock's owning pool — the thread is
             * still alive. */
            ret = PJ_EBUSY;
        } else {
            /* Any other OS error from pj_thread_join(): clock->thread
             * is still alive and the descriptor was not freed.
             * Propagate the error so the caller doesn't release the
             * owning pool out from under the running thread. */
            ret = status;
        }
    }

    pj_mutex_unlock(clock->destroy_lock);
    return ret;
}


/*
 * Update the clock. 
 */
PJ_DEF(pj_status_t) pjmedia_clock_modify(pjmedia_clock *clock,
                                         const pjmedia_clock_param *param)
{
    clock->interval.u64 = param->usec_interval * clock->freq.u64 /
                          USEC_IN_SEC;
    clock->timestamp_inc = (unsigned)(param->usec_interval *
                                      param->clock_rate /
                                      (unsigned)USEC_IN_SEC);

    return PJ_SUCCESS;
}


/* Calculate next tick */
PJ_INLINE(void) clock_calc_next_tick(pjmedia_clock *clock,
                                     pj_timestamp *now)
{
    if (clock->next_tick.u64+clock->max_jump < now->u64) {
        /* Timestamp has made large jump, adjust next_tick */
        clock->next_tick.u64 = now->u64;
    }
    clock->next_tick.u64 += clock->interval.u64;

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
    clock->timestamp.u64 += clock->timestamp_inc;

    /* Calculate next tick */
    clock_calc_next_tick(clock, &now);

    /* Done */
    return PJ_TRUE;
}


/*
 * Clock thread
 */
static int clock_thread(void *arg)
{
    pj_timestamp now;
    pjmedia_clock *clock = (pjmedia_clock*) arg;

    /* Set thread priority to maximum unless not wanted. */
    if ((clock->options & PJMEDIA_CLOCK_NO_HIGHEST_PRIO) == 0) {
        pj_thread_t *this_thread = pj_thread_this();
        int max = pj_thread_get_prio_max(this_thread);
        if (max > 0) {
            pj_status_t status;

            status = pj_thread_set_prio(this_thread, max);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(5, (pj_thread_get_name(this_thread), status,
                          "Unable to increase thread priority to %d", max));
            }
        }
    }

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
        if (!clock->running) {
            /* Calculate next tick */
            clock_calc_next_tick(clock, &now);
            continue;
        }

        pj_lock_acquire(clock->lock);

        /* Call callback, if any */
        if (clock->cb)
            (*clock->cb)(&clock->timestamp, clock->user_data);

        /* Best effort way to detect if we've been destroyed in the callback */
        if (clock->quitting) {
            pj_lock_release(clock->lock);
            break;
        }

        /* Increment timestamp */
        clock->timestamp.u64 += clock->timestamp_inc;

        /* Calculate next tick */
        clock_calc_next_tick(clock, &now);

        pj_lock_release(clock->lock);
    }

    return 0;
}


/*
 * Destroy the clock. 
 */
PJ_DEF(pj_status_t) pjmedia_clock_destroy(pjmedia_clock *clock)
{
    pj_status_t ret = PJ_SUCCESS;

    PJ_ASSERT_RETURN(clock != NULL, PJ_EINVAL);

    /* Serialize against concurrent stop/destroy. See the same
     * pattern in pjmedia_clock_stop() above. */
    pj_mutex_lock(clock->destroy_lock);

    clock->running = PJ_FALSE;
    clock->quitting = PJ_TRUE;

    if (clock->thread) {
        pj_status_t status = pj_thread_join(clock->thread);
        if (status == PJ_SUCCESS) {
            pj_thread_destroy(clock->thread);
            clock->thread = NULL;
        } else {
            /* PJ_ECANCELLED (self-join) or another OS error: the
             * clock thread is still alive and may still touch
             * clock->lock and clock->pool from inside the callback
             * loop. Tearing them down here would be a UAF. Skip the
             * cleanup, propagate the error, and let the caller
             * (which still owns the backing pool) retry once the
             * thread has actually exited. */
            ret = (status == PJ_ECANCELLED) ? PJ_EBUSY : status;
            pj_mutex_unlock(clock->destroy_lock);
            return ret;
        }
    }

    if (clock->lock) {
        pj_lock_destroy(clock->lock);
        clock->lock = NULL;
    }

    /* destroy_lock memory lives in clock->pool, so tear it down here
     * before pool release. Caller must not race stop/destroy past
     * this point. */
    pj_mutex_unlock(clock->destroy_lock);
    pj_mutex_destroy(clock->destroy_lock);
    clock->destroy_lock = NULL;

    pj_pool_safe_release(&clock->pool);

    return ret;
}

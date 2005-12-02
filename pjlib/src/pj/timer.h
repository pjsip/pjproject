/* $Header: /pjproject/pjlib/src/pj/timer.h 5     5/03/05 9:07a Bennylp $ */
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
/* (C)1993-2003 Douglas C. Schmidt
 *
 * This file is originaly from ACE library by Doug Schmidt
 * ACE(TM), TAO(TM) and CIAO(TM) are copyrighted by Douglas C. Schmidt and his research 
 * group at Washington University, University of California, Irvine, and Vanderbilt 
 * University Copyright (c) 1993-2003, all rights reserved.
 *
 */

#ifndef __PJ_TIMER_H__
#define __PJ_TIMER_H__

/**
 * @file timer.h
 * @brief Timer Heap
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_TIMER Timer Heap Management.
 * @ingroup PJ_MISC
 * @brief
 * The timer scheduling implementation here is based on ACE library's 
 * ACE_Timer_Heap, with only little modification to suit our library's style
 * (I even left most of the comments in the original source).
 *
 * To quote the original quote in ACE_Timer_Heap_T class:
 *
 *      This implementation uses a heap-based callout queue of
 *      absolute times.  Therefore, in the average and worst case,
 *      scheduling, canceling, and expiring timers is O(log N) (where
 *      N is the total number of timers).  In addition, we can also
 *      preallocate as many \a ACE_Timer_Nodes as there are slots in
 *      the heap.  This allows us to completely remove the need for
 *      dynamic memory allocation, which is important for real-time
 *      systems.
 * @{
 */


/**
 * The type for internal timer ID.
 */
typedef int pj_timer_id_t;


struct pj_timer_entry;

/**
 * The type of callback function to be called by timer scheduler when a timer
 * has expired.
 */
typedef void pj_timer_heap_callback(pj_timer_heap_t *timer_heap,
				    struct pj_timer_entry *entry);


/**
 * This structure represents an entry to the timer.
 */
struct pj_timer_entry
{
    /** Arbitrary ID assigned by the user/owner of this entry. */
    int id;

    /** User data to be associated with this entry. */
    void *user_data;
    
    /** Callback to be called when the timer expires. */
    pj_timer_heap_callback *cb;

    /** Internal unique timer ID, which is assigned by the timer heap. 
	Application should not touch this ID.
     */
    pj_timer_id_t _timer_id;

    /** The future time when the timer expires, which the value is updated
     *  by timer heap when the timer is scheduled.
     */
    pj_time_val _timer_value;
};


/**
 * Guidance on how much memory is required per timer item.
 */
#define PJ_TIMER_ENTRY_SIZE	(2 * sizeof(void*) + sizeof(pj_timer_entry))

/**
 * Guidance on how much memory is required for the timer heap.
 */
#define PJ_TIMER_HEAP_SIZE	(8 * sizeof(void*) + 2 * PJ_TIMER_ENTRY_SIZE)

/**
 * Flag to indicate that thread synchronization is NOT needed for the timer heap.
 */
#define PJ_TIMER_HEAP_SYNCHRONIZE	(0)
#define PJ_TIMER_HEAP_NO_SYNCHRONIZE	(1)

/**
 * Create a timer heap.
 * @param pool the pool where allocations in the timer heap will be allocated.
 *             The timer heap will dynamicly allocate more storate from the
 *	       pool if the number of timer entries registered is more than
 *	       the size originally requested when calling this function.
 * @param count the maximum number of timer entries to be supported initially.
 *		If the application registers more entries during runtime,
 *		then the timer heap will resize.
 * @param flag Creation flag, currently only PJ_TIMER_HEAP_NO_SYNCHRONIZE is
 *	       recognized..
 * @return the timer heap, or NULL.
 */
PJ_DECL(pj_timer_heap_t*) pj_timer_heap_create( pj_pool_t *pool,
					        pj_size_t count,
						unsigned flag );

/**
 * Schedule a timer entry which will expire AFTER the specified delay.
 * @param ht the timer heap.
 * @param entry the entry to be registered. 
 * @param delay the interval to expire.
 * @return zero on success, or -1 on error.
 */
PJ_DECL(pj_status_t) pj_timer_heap_schedule( pj_timer_heap_t *ht,
					     pj_timer_entry *entry, 
					     const pj_time_val *delay);

/**
 * Cancel a previously registered timer.
 * @param ht the timer heap.
 * @param entry the entry to be cancelled.
 * @return the number of timer cancelled, which should be one if the
 *         entry has really been registered, or zero if no timer was
 *         cancelled.
 */
PJ_DECL(int) pj_timer_heap_cancel( pj_timer_heap_t *ht,
				   pj_timer_entry *entry);

/**
 * Get the number of timer entries.
 * @param ht the timer heap.
 * @return the number of timer entries.
 */
PJ_DECL(pj_size_t) pj_timer_heap_count( pj_timer_heap_t *ht );

/**
 * Get the earliest time registered in the timer heap.
 * @param ht the timer heap.
 * @param timeval the time deadline of the earliest timer entry.
 */
PJ_DECL(void) pj_timer_heap_earliest_time( pj_timer_heap_t *ht, 
					   pj_time_val *timeval);

/**
 * Poll the timer heap, check for expired timers and call the callback for
 * each of the expired timers.
 * @param ht the timer heap.
 * @param next_delay If this parameter is not NULL, it will be filled up with
 *		     the time delay until the next timer elapsed, or -1 in
 *		     the sec part if no entry exist.
 * @return the number of timers expired.
 */
PJ_DECL(int) pj_timer_heap_poll( pj_timer_heap_t *ht, pj_time_val *next_delay);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJ_TIMER_H__ */


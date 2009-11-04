/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#include <pj/timer.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/lock.h>

#include "os_symbian.h"


#define DEFAULT_MAX_TIMED_OUT_PER_POLL  (64)

// Maximum number of miliseconds that RTimer.At() supports
#define MAX_RTIMER_INTERVAL		2147


/**
 * The implementation of timer heap.
 */
struct pj_timer_heap_t
{
    /** Maximum size of the heap. */
    pj_size_t max_size;

    /** Current size of the heap. */
    pj_size_t cur_size;

    /** Max timed out entries to process per poll. */
    unsigned max_entries_per_poll;
};


//////////////////////////////////////////////////////////////////////////////
/**
 * Active object for each timer entry.
 */
class CPjTimerEntry : public CActive 
{
public:
    static CPjTimerEntry* NewL(	pj_timer_heap_t *timer_heap,
    				pj_timer_entry *entry,
    				const pj_time_val *delay);
    
    ~CPjTimerEntry();
    
    virtual void RunL();
    virtual void DoCancel();

private:	
    pj_timer_heap_t *timer_heap_;
    pj_timer_entry  *entry_;
    RTimer	     rtimer_;
    pj_uint32_t	     interval_left_;
    
    CPjTimerEntry(pj_timer_heap_t *timer_heap, pj_timer_entry *entry);
    void ConstructL(const pj_time_val *delay);
    void Schedule();
};


CPjTimerEntry::CPjTimerEntry(pj_timer_heap_t *timer_heap,
			     pj_timer_entry *entry)
: CActive(PJ_SYMBIAN_TIMER_PRIORITY), timer_heap_(timer_heap), entry_(entry),
  interval_left_(0)
{
}

CPjTimerEntry::~CPjTimerEntry() 
{
    Cancel();
    rtimer_.Close();
}

void CPjTimerEntry::Schedule()
{
    pj_int32_t interval;
    
    if (interval_left_ > MAX_RTIMER_INTERVAL) {
	interval = MAX_RTIMER_INTERVAL;
    } else {
	interval = interval_left_;
    }
    
    interval_left_ -= interval;
    rtimer_.After(iStatus, interval * 1000);
    SetActive();
}

void CPjTimerEntry::ConstructL(const pj_time_val *delay) 
{
    rtimer_.CreateLocal();
    CActiveScheduler::Add(this);
    
    interval_left_ = PJ_TIME_VAL_MSEC(*delay);
    Schedule();
}

CPjTimerEntry* CPjTimerEntry::NewL(pj_timer_heap_t *timer_heap,
				   pj_timer_entry *entry,
				   const pj_time_val *delay) 
{
    CPjTimerEntry *self = new CPjTimerEntry(timer_heap, entry);
    CleanupStack::PushL(self);
    self->ConstructL(delay);
    CleanupStack::Pop(self);

    return self;
}

void CPjTimerEntry::RunL() 
{
    if (interval_left_ > 0) {
	Schedule();
	return;
    }
    
    --timer_heap_->cur_size;
    entry_->_timer_id = NULL;
    entry_->cb(timer_heap_, entry_);
    
    // Finger's crossed!
    delete this;
}

void CPjTimerEntry::DoCancel() 
{
     rtimer_.Cancel();
}


//////////////////////////////////////////////////////////////////////////////


/*
 * Calculate memory size required to create a timer heap.
 */
PJ_DEF(pj_size_t) pj_timer_heap_mem_size(pj_size_t count)
{
    return /* size of the timer heap itself: */
           sizeof(pj_timer_heap_t) + 
           /* size of each entry: */
           (count+2) * (sizeof(pj_timer_entry*)+sizeof(pj_timer_id_t)) +
           /* lock, pool etc: */
           132;
}

/*
 * Create a new timer heap.
 */
PJ_DEF(pj_status_t) pj_timer_heap_create( pj_pool_t *pool,
					  pj_size_t size,
                                          pj_timer_heap_t **p_heap)
{
    pj_timer_heap_t *ht;

    PJ_ASSERT_RETURN(pool && p_heap, PJ_EINVAL);

    *p_heap = NULL;

    /* Allocate timer heap data structure from the pool */
    ht = PJ_POOL_ALLOC_T(pool, pj_timer_heap_t);
    if (!ht)
        return PJ_ENOMEM;

    /* Initialize timer heap sizes */
    ht->max_size = size;
    ht->cur_size = 0;
    ht->max_entries_per_poll = DEFAULT_MAX_TIMED_OUT_PER_POLL;

    *p_heap = ht;
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_timer_heap_destroy( pj_timer_heap_t *ht )
{
    PJ_UNUSED_ARG(ht);
}

PJ_DEF(void) pj_timer_heap_set_lock(  pj_timer_heap_t *ht,
                                      pj_lock_t *lock,
                                      pj_bool_t auto_del )
{
    PJ_UNUSED_ARG(ht);
    if (auto_del)
    	pj_lock_destroy(lock);
}


PJ_DEF(unsigned) pj_timer_heap_set_max_timed_out_per_poll(pj_timer_heap_t *ht,
                                                          unsigned count )
{
    unsigned old_count = ht->max_entries_per_poll;
    ht->max_entries_per_poll = count;
    return old_count;
}

PJ_DEF(pj_timer_entry*) pj_timer_entry_init( pj_timer_entry *entry,
                                             int id,
                                             void *user_data,
                                             pj_timer_heap_callback *cb )
{
    pj_assert(entry && cb);

    entry->_timer_id = NULL;
    entry->id = id;
    entry->user_data = user_data;
    entry->cb = cb;

    return entry;
}

PJ_DEF(pj_status_t) pj_timer_heap_schedule( pj_timer_heap_t *ht,
					    pj_timer_entry *entry, 
					    const pj_time_val *delay)
{
    CPjTimerEntry *timerObj;
    
    PJ_ASSERT_RETURN(ht && entry && delay, PJ_EINVAL);
    PJ_ASSERT_RETURN(entry->cb != NULL, PJ_EINVAL);

    /* Prevent same entry from being scheduled more than once */
    PJ_ASSERT_RETURN(entry->_timer_id == NULL, PJ_EINVALIDOP);

    timerObj = CPjTimerEntry::NewL(ht, entry, delay);
    entry->_timer_id = (void*) timerObj;
    
    ++ht->cur_size;
    return PJ_SUCCESS;
}

PJ_DEF(int) pj_timer_heap_cancel( pj_timer_heap_t *ht,
				  pj_timer_entry *entry)
{
    PJ_ASSERT_RETURN(ht && entry, PJ_EINVAL);
    
    if (entry->_timer_id != NULL) {
    	CPjTimerEntry *timerObj = (CPjTimerEntry*) entry->_timer_id;
    	timerObj->Cancel();
    	delete timerObj;
    	entry->_timer_id = NULL;
    	--ht->cur_size;
    	return 1;
    } else {
    	return 0;
    }
}

PJ_DEF(unsigned) pj_timer_heap_poll( pj_timer_heap_t *ht, 
                                     pj_time_val *next_delay )
{
    /* Polling is not necessary on Symbian, since all async activities
     * are registered to active scheduler.
     */
    PJ_UNUSED_ARG(ht);
    if (next_delay) {
    	next_delay->sec = 1;
    	next_delay->msec = 0;
    }
    return 0;
}

PJ_DEF(pj_size_t) pj_timer_heap_count( pj_timer_heap_t *ht )
{
    PJ_ASSERT_RETURN(ht, 0);

    return ht->cur_size;
}

PJ_DEF(pj_status_t) pj_timer_heap_earliest_time( pj_timer_heap_t * ht,
					         pj_time_val *timeval)
{
    /* We don't support this! */
    PJ_UNUSED_ARG(ht);
    
    timeval->sec = 1;
    timeval->msec = 0;
    
    return PJ_SUCCESS;
}


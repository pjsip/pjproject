/*
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
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
#include <pj/assert.h>
#include <pj/atomic_queue.h>
#include <pj/errno.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <atomic>

#if 0
#   define TRACE_(arg) PJ_LOG(4,arg)
#else
#   define TRACE_(expr)
#endif
class AtomicQueue {
public:
    /**
     * Constructor
     */
    AtomicQueue(unsigned maxItemCnt, unsigned itemSize, const char* name = ""):
                maxItemCnt_(maxItemCnt),
                itemSize_(itemSize),
                ptrWrite(0),
                ptrRead(0),
                buffer(NULL),
                name_(name)
    {
        buffer = new char[maxItemCnt_ * itemSize_];

        /* Surpress warning when debugging log is disabled */
        PJ_UNUSED_ARG(name_);

        TRACE_((name_, "Created AtomicQueue: maxItemCnt=%d itemSize=%d",
                maxItemCnt_, itemSize_));
    }

    /**
     * Destructor
     */
    ~AtomicQueue()
    {
        delete [] buffer;
    }

    /**
     * Get a item from the head of the queue
     */
    bool get(void* item)
    {
        if (ptrRead == ptrWrite)
            return false;

        unsigned cur_ptr = ptrRead;
        void *p = &buffer[cur_ptr * itemSize_];
        pj_memcpy(item, p, itemSize_);
        inc_ptrRead_if_not_yet(cur_ptr);

        TRACE_((name_, "GET: ptrRead=%d ptrWrite=%d\n",
               ptrRead.load(), ptrWrite.load()));

        return true;
    }

    /**
     * Put a item to the back of the queue
     */
    void put(void* item)
    {
        unsigned cur_ptr = ptrWrite;
        void *p = &buffer[cur_ptr * itemSize_];
        pj_memcpy(p, item, itemSize_);
        unsigned next_ptr = inc_ptrWrite(cur_ptr);

        /* Increment read pointer if next write is overlapping
         * (next_ptr == read ptr)
         */
        unsigned next_read_ptr = (next_ptr == maxItemCnt_-1)? 0 : (next_ptr+1);
        ptrRead.compare_exchange_strong(next_ptr, next_read_ptr);

        TRACE_((name_, "PUT: ptrRead=%d ptrWrite=%d\n",
               ptrRead.load(), ptrWrite.load()));
    }

private:
    unsigned maxItemCnt_;
    unsigned itemSize_;
    std::atomic<unsigned> ptrWrite;
    std::atomic<unsigned> ptrRead;
    char *buffer;
    const char *name_;

    /* Increment read pointer, only if producer not incemented it already.
     * Producer may increment the read pointer if the write pointer is
     * right before the read pointer (buffer almost full).
     */
    bool inc_ptrRead_if_not_yet(unsigned old_ptr)
    {
        unsigned new_ptr = (old_ptr == maxItemCnt_-1)? 0 : (old_ptr+1);
        return ptrRead.compare_exchange_strong(old_ptr, new_ptr);
    }

    /* Increment write pointer */
    unsigned inc_ptrWrite(unsigned old_ptr)
    {
        unsigned new_ptr = (old_ptr == maxItemCnt_-1)? 0 : (old_ptr+1);
        if (ptrWrite.compare_exchange_strong(old_ptr, new_ptr))
            return new_ptr;

        /* Should never happen */
        pj_assert(!"There is more than one producer!");
        return old_ptr;
    }

    AtomicQueue():maxItemCnt_(0), itemSize_(0), ptrWrite(0),
                  ptrRead(0), buffer(NULL), name_("")
    {}
};

struct pj_atomic_queue_t
{
    AtomicQueue    *aQ;
};

PJ_DEF(pj_status_t) pj_atomic_queue_create(pj_pool_t *pool,
                                           unsigned max_item_cnt,
                                           unsigned item_size,
                                           const char *name,
                                           pj_atomic_queue_t **atomic_queue)
{
    pj_atomic_queue_t *aqueue;

    PJ_ASSERT_RETURN(pool, PJ_EINVAL);
    aqueue = PJ_POOL_ZALLOC_T(pool, pj_atomic_queue_t);
    aqueue->aQ = new AtomicQueue(max_item_cnt, item_size, name);
    *atomic_queue = aqueue;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_atomic_queue_destroy(pj_atomic_queue_t *atomic_queue)
{
    PJ_ASSERT_RETURN(atomic_queue && atomic_queue->aQ, PJ_EINVAL);
    delete atomic_queue->aQ;
    atomic_queue->aQ = NULL;
    return PJ_SUCCESS;
}

/**
 * For producer, there is write pointer 'ptrWrite' that will be incremented
 * every time a item is queued to the back of the queue.
 */
PJ_DEF(pj_status_t) pj_atomic_queue_put(pj_atomic_queue_t *atomic_queue,
                                        void *item)
{
    PJ_ASSERT_RETURN(atomic_queue && atomic_queue->aQ && item, PJ_EINVAL);
    atomic_queue->aQ->put(item);
    return PJ_SUCCESS;
}

/**
 * For consumer, there is read pointer 'ptrRead' that will be incremented
 * every time a item is fetched from the head of the queue, only if the
 * pointer is not modified by producer (in case of queue full).
 */
PJ_DEF(pj_status_t) pj_atomic_queue_get(pj_atomic_queue_t *atomic_queue,
                                        void *item)
{
    PJ_ASSERT_RETURN(atomic_queue && atomic_queue->aQ && item, PJ_EINVAL);
    if (atomic_queue->aQ->get(item))
        return PJ_SUCCESS;
    else
        return PJ_ENOTFOUND;
}

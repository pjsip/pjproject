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
#ifndef __ATOMIC_QUEUE_HPP__
#define __ATOMIC_QUEUE_HPP__

#include <atomic>

/* Atomic queue (ring buffer) for single consumer & single producer.
 *
 * Producer invokes 'put(item)' to put an item to the back of the queue and
 * consumer invokes 'get(item)' to get an item from the head of the queue.
 *
 * For producer, there is write pointer 'ptrWrite' that will be incremented
 * every time a item is queued to the back of the queue. If the queue is
 * almost full (the write pointer is right before the read pointer) the
 * producer will forcefully discard the oldest item in the head of the
 * queue by incrementing read pointer.
 *
 * For consumer, there is read pointer 'ptrRead' that will be incremented
 * every time a item is fetched from the head of the queue, only if the
 * pointer is not modified by producer (in case of queue full).
 */
class AtomicQueue {
public:
    /**
     * Constructor
     */
    AtomicQueue(unsigned max_item_cnt, unsigned item_size,
                const char* name_);

    /**
     * Destructor
     */
    ~AtomicQueue();

    /**
     * Get a item from the head of the queue
     */
    bool get(void* item);

    /**
     * Put a item to the back of the queue
     */
    void put(void* item);

private:
    unsigned maxItemCnt;
    unsigned itemSize;
    std::atomic<unsigned> ptrWrite;
    std::atomic<unsigned> ptrRead;
    char *buffer;
    const char *name;

    /* Increment read pointer, only if producer not incemented it already.
     * Producer may increment the read pointer if the write pointer is
     * right before the read pointer (buffer almost full).
     */
    bool inc_ptr_read_if_not_yet(unsigned old_ptr);

    /* Increment write pointer */
    unsigned inc_ptr_write(unsigned old_ptr);

    AtomicQueue() {}
};

#endif

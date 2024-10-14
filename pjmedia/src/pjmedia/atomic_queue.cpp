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
#include <pj/string.h>
#include <pjmedia/atomic_queue.hpp>

#if defined(PJ_ANDROID) && PJ_ANDROID != 0

#include <android/log.h>

AtomicQueue::AtomicQueue(unsigned max_item_cnt, unsigned item_size,
                         const char* name_= "") :
                         maxItemCnt(max_item_cnt), itemSize(item_size),
                         ptrWrite(0), ptrRead(0),
                         buffer(NULL), name(name_)
{
    buffer = new char[maxItemCnt * itemSize];

    /* Surpress warning when debugging log is disabled */
    PJ_UNUSED_ARG(name);

    // __android_log_print(ANDROID_LOG_INFO, name,
    //                 "Created AtomicQueue: max_item_cnt=%d item_size=%d\n",
    //                 max_item_cnt, item_size);
}

AtomicQueue::~AtomicQueue() {
    delete [] buffer;
}

/* Get a item from the head of the queue */
bool AtomicQueue::get(void* item) {
    if (ptrRead == ptrWrite)
        return false;

    unsigned cur_ptr = ptrRead;
    void *p = &buffer[cur_ptr * itemSize];
    pj_memcpy(item, p, itemSize);
    inc_ptr_read_if_not_yet(cur_ptr);

    // __android_log_print(ANDROID_LOG_INFO, name,
    //                    "GET: ptrRead=%d ptrWrite=%d\n",
    //                    ptrRead.load(), ptrWrite.load());
    return true;
}

/* Put a item to the back of the queue */
void AtomicQueue::put(void* item) {
    unsigned cur_ptr = ptrWrite;
    void *p = &buffer[cur_ptr * itemSize];
    pj_memcpy(p, item, itemSize);
    unsigned next_ptr = inc_ptr_write(cur_ptr);

    /* Increment read pointer if next write is overlapping
     * (next_ptr == read ptr)
     */
    unsigned next_read_ptr = (next_ptr == maxItemCnt-1)? 0 : (next_ptr+1);
    ptrRead.compare_exchange_strong(next_ptr, next_read_ptr);

    // __android_log_print(ANDROID_LOG_INFO, name,
    //                    "PUT: ptrRead=%d ptrWrite=%d\n",
    //                    ptrRead.load(), ptrWrite.load());
}

bool AtomicQueue::inc_ptr_read_if_not_yet(unsigned old_ptr) {
    unsigned new_ptr = (old_ptr == maxItemCnt-1)? 0 : (old_ptr+1);
    return ptrRead.compare_exchange_strong(old_ptr, new_ptr);
}

/* Increment write pointer */
unsigned AtomicQueue::inc_ptr_write(unsigned old_ptr) {
    unsigned new_ptr = (old_ptr == maxItemCnt-1)? 0 : (old_ptr+1);
    if (ptrWrite.compare_exchange_strong(old_ptr, new_ptr))
        return new_ptr;

    /* Should never happen */
    pj_assert(!"There is more than one producer!");
    return old_ptr;
}

#endif

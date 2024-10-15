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
#include <pj/string.h>

#if defined(PJ_ANDROID) && PJ_ANDROID != 0

//#include <android/log.h>

pj_atomic_queue::pj_atomic_queue(unsigned max_item_cnt, unsigned item_size,
                                 const char* name = "") :
                                 max_item_cnt_(max_item_cnt),
                                 item_size_(item_size),
                                 ptr_write(0),
                                 ptr_read(0),
                                 buffer(NULL),
                                 name_(name)
{
    buffer = new char[max_item_cnt_ * item_size_];

    /* Surpress warning when debugging log is disabled */
    PJ_UNUSED_ARG(name_);

    // __android_log_print(ANDROID_LOG_INFO, name,
    //                 "Created AtomicQueue: max_item_cnt=%d item_size=%d\n",
    //                 max_item_cnt_, item_size_);
}

pj_atomic_queue::~pj_atomic_queue() {
    delete [] buffer;
}

/* Get a item from the head of the queue */
bool pj_atomic_queue::get(void* item) {
    if (ptr_read == ptr_write)
        return false;

    unsigned cur_ptr = ptr_read;
    void *p = &buffer[cur_ptr * item_size_];
    pj_memcpy(item, p, item_size_);
    inc_ptr_read_if_not_yet(cur_ptr);

    // __android_log_print(ANDROID_LOG_INFO, name,
    //                    "GET: ptr_read=%d ptr_write=%d\n",
    //                    ptr_read.load(), ptr_write.load());
    return true;
}

/* Put a item to the back of the queue */
void pj_atomic_queue::put(void* item) {
    unsigned cur_ptr = ptr_write;
    void *p = &buffer[cur_ptr * item_size_];
    pj_memcpy(p, item, item_size_);
    unsigned next_ptr = inc_ptr_write(cur_ptr);

    /* Increment read pointer if next write is overlapping
     * (next_ptr == read ptr)
     */
    unsigned next_read_ptr = (next_ptr == max_item_cnt_-1)? 0 : (next_ptr+1);
    ptr_read.compare_exchange_strong(next_ptr, next_read_ptr);

    // __android_log_print(ANDROID_LOG_INFO, name_,
    //                    "PUT: ptr_read=%d ptr_write=%d\n",
    //                    ptr_read.load(), ptr_write.load());
}

bool pj_atomic_queue::inc_ptr_read_if_not_yet(unsigned old_ptr) {
    unsigned new_ptr = (old_ptr == max_item_cnt_-1)? 0 : (old_ptr+1);
    return ptr_read.compare_exchange_strong(old_ptr, new_ptr);
}

/* Increment write pointer */
unsigned pj_atomic_queue::inc_ptr_write(unsigned old_ptr) {
    unsigned new_ptr = (old_ptr == max_item_cnt_-1)? 0 : (old_ptr+1);
    if (ptr_write.compare_exchange_strong(old_ptr, new_ptr))
        return new_ptr;

    /* Should never happen */
    pj_assert(!"There is more than one producer!");
    return old_ptr;
}

#endif

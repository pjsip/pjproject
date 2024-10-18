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
#ifndef __PJ_ATOMIC_QUEUE_H__
#define __PJ_ATOMIC_QUEUE_H__

/**
 * @file atomic_queue.h
 * @brief Atomic Queue operations
 * @{
 *
 * Atomic queue for a single consumer and producer.
 * This cyclic queue employs a ring buffer for storage, maintaining read/write
 * pointers without locks. It’s designed for one producer and one consumer,
 * as having multiple producers/consumers could lead to conflicts with the
 * read/write pointers.
 * The producer uses #pj_atomic_queue_put() to add an item to the back
 * of the queue, while the consumer uses #pj_atomic_queue_get()
 * to retrieve an item from the front.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * Create a new Atomic Queue for single consumer and single producer case.
 *
 * @param pool          The pool to allocate the atomic queue structure.
 * @param max_item_cnt  The maximum number of items that can be stored.
 * @param item_size     The size of each item.
 * @param name          The name of the queue.
 * @param atomic_queue  Pointer to hold the newly created Atomic Queue.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_atomic_queue_create(pj_pool_t *pool,
                                            unsigned max_item_cnt,
                                            unsigned item_size,
                                            const char *name,
                                            pj_atomic_queue_t **atomic_queue);

/**
 * Destroy the Atomic Queue.
 *
 * @param atomic_queue  The Atomic Queue to be destroyed.
 *
 * @return              PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pj_atomic_queue_destroy(pj_atomic_queue_t *atomic_queue);

/**
 * Put an item to the back of the queue. If the queue is almost full
 * (the write pointer is right before the read pointer) the producer will
 * forcefully discard the oldest item in the head of the queue by incrementing
 * the read pointer.
 *
 * @param atomic_queue  The Atomic Queue.
 * @param item          The pointer to the data to store.
 *
 * @return              PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pj_atomic_queue_put(pj_atomic_queue_t *atomic_queue,
                                         void *item);

/**
 * Get an item from the head of the queue.
 *
 * @param atomic_queue  The Atomic Queue.
 * @param item          The pointer to data to get the data.
 *
 * @return              PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pj_atomic_queue_get(pj_atomic_queue_t *atomic_queue,
                                         void *item);

/**
 * @}
 */

PJ_END_DECL

#endif

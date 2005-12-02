/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/pool.c,v 1.1 2005/12/02 20:02:30 nn Exp $ */
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

#include <pj/pool.h>

#include <pj/log.h>
#include <pj/except.h>
#include <stdlib.h>

/* Include inline definitions when inlining is disabled. */
#if !PJ_FUNCTIONS_ARE_INLINED
#  include "pool_i.h"
#endif


static pj_pool_block *pj_pool_create_block( pj_pool_t *pool, pj_size_t size)
{
    pj_pool_block *block;

    PJ_LOG(4, (pool->obj_name, "create_block(sz=%d), cur.cap=%d, cur.used=%d", 
				size, pool->capacity, pool->used_size));

    // Request memory from allocator.
    block = (pj_pool_block*) (*pool->factory->policy.block_alloc)(pool->factory, size);
    if (block == NULL) {
	(*pool->callback)(pool, size);
	return NULL;
    }

    // Add capacity.
    pool->capacity += size;
    pool->used_size += sizeof(pj_pool_block);

    // Set block attribytes.
    block->cur = block->buf = ((unsigned char*)block) + sizeof(pj_pool_block);
    block->end = ((unsigned char*)block) + size;

    // Insert in the front of the list.
    pj_list_insert_after(&pool->block_list, block);

    PJ_LOG(5, (pool->obj_name, " block created, buffer=%p to %p", block->buf, block->end));

    return block;
}

PJ_DEF(void*) pj_pool_allocate_find(pj_pool_t *pool, unsigned size)
{
    pj_pool_block *block = pool->block_list.next;
    void *p;
    unsigned block_size;

    while (block != &pool->block_list) {
	p = pj_pool_alloc_from_block(pool, block, size);
	if (p != NULL)
	    return p;
	block = block->next;
    }

    /*
    if (pool->increment_size < size + sizeof(pj_pool_block)) {
	PJ_LOG(2, (pool->obj_name, "Not enough increment to allocate %d bytes "
		   "from pool (used=%d, cap=%d)",
		   size, pool->used_size, pool->capacity));
	(*pool->callback)(pool);
	return NULL;
    }
    */
    /* Normalize size. */
    if (size & 0x03) {
	size &= ~0x03;
	size += 4;
    }

    /* Add block header overhead. */
    block_size = size + sizeof(pj_pool_block);

    if (block_size < pool->increment_size)
	block_size = pool->increment_size;

    PJ_LOG(4, (pool->obj_name, "Resizing pool by %d bytes (used=%d, cap=%d)",
	       block_size, pool->used_size, pool->capacity));

    block = pj_pool_create_block(pool, block_size);
    if (!block)
	return NULL;

    p = pj_pool_alloc_from_block(pool, block, size);
#ifndef NDEBUG
    if (p == NULL) {
	p = p;
    }
#endif
    pj_assert(p != NULL);
    return p;
}

PJ_DEF(void) pj_pool_init_int(  pj_pool_t *pool, 
				const char *name,
				pj_size_t increment_size,
				pj_pool_callback *callback)
{
    pj_pool_block *block;

    pool->increment_size = increment_size;
    pool->callback = callback;
    pool->used_size = sizeof(*pool);
    block = pool->block_list.next;
    while (block != &pool->block_list) {
	pool->used_size += sizeof(pj_pool_block);
	block = block->next;
    }

    if (name) {
	if (strchr(name, '%') != NULL) {
	    sprintf(pool->obj_name, name, pool);
	} else {
	    strncpy(pool->obj_name, name, PJ_MAX_OBJ_NAME);
	}
    } else {
	pool->obj_name[0] = '\0';
    }
}

PJ_DEF(pj_pool_t*) pj_pool_create_int( pj_pool_factory *f, const char *name,
				       pj_size_t initial_size, 
				       pj_size_t increment_size,
				       pj_pool_callback *callback)
{
    pj_pool_t *pool;
    pj_pool_block *block;
    unsigned char *buffer;

    buffer = (*f->policy.block_alloc)(f, initial_size);
    if (!buffer)
	return NULL;

    // Set pool administrative data.
    pool = (pj_pool_t*)buffer;
    memset(pool, 0, sizeof(*pool));

    pj_list_init(&pool->block_list);
    pool->factory = f;

    // Create the first block from the memory.
    block = (pj_pool_block*) (buffer + sizeof(*pool));
    block->cur = block->buf = ((unsigned char*)block) + sizeof(pj_pool_block);
    block->end = buffer + initial_size;
    pj_list_insert_after(&pool->block_list, block);

    pj_pool_init_int(pool, name, increment_size, callback);

    // Pool initial capacity and used size
    pool->capacity = initial_size;

    PJ_LOG(4, (pool->obj_name, "pool created, size=%u", pool->capacity));
    return pool;
}

static void reset_pool(pj_pool_t *pool)
{
    pj_pool_block *block;

    block = pool->block_list.prev;
    if (block == &pool->block_list)
	return;

    /* Skip the first block because it is occupying the same memory
       as the pool itself.
    */
    block = block->prev;
    
    while (block != &pool->block_list) {
	pj_pool_block *prev = block->prev;
	pj_list_erase(block);
	(*pool->factory->policy.block_free)(pool->factory, block, 
					    block->end - (unsigned char*)block);
	block = prev;
    }

    block = pool->block_list.next;
    block->cur = block->buf;
    pool->capacity = block->end - (unsigned char*)pool;
    pool->used_size = 0;
}

PJ_DEF(void) pj_pool_reset(pj_pool_t *pool)
{
    PJ_LOG(5, (pool->obj_name, "reset(): cap=%d, used=%d(%d%%)", 
	       pool->capacity, pool->used_size, pool->used_size*100/pool->capacity));

    reset_pool(pool);
}


PJ_DEF(void) pj_pool_destroy_int(pj_pool_t *pool)
{
    pj_size_t initial_size;

    PJ_LOG(4, (pool->obj_name, "destroy(): cap=%d, used=%d(%d%%), block0=%p-%p", 
	       pool->capacity, pool->used_size, pool->used_size*100/pool->capacity,
	       ((pj_pool_block*)pool->block_list.next)->buf, 
	       ((pj_pool_block*)pool->block_list.next)->end));

    reset_pool(pool);
    initial_size = ((pj_pool_block*)pool->block_list.next)->end - 
		   (unsigned char*)pool;
    (*pool->factory->policy.block_free)(pool->factory, pool, initial_size);
}



/*
PJ_DEF(char*) pj_pool_strdup(pj_pool_t *pool, const char *str)
{
    int size;
    char *buf;

    if (!str)
	return NULL;

    size = strlen(str)+1;
    buf = pj_pool_alloc(pool, size);
    if (!buf)
	return NULL;

    return (char*)memcpy( buf, str, size+1 );
}
*/

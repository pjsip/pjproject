/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/pool_caching.c,v 1.1 2005/12/02 20:02:30 nn Exp $ */
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
#include <pj/string.h>
#include <stdlib.h> /* malloc, free */


static pj_pool_t* cpool_create_pool(pj_pool_factory *pf, 
				    const char *name,
				    pj_size_t initial_size, 
				    pj_size_t increment_sz,
				    pj_pool_callback *callback);
static void cpool_release_pool(pj_pool_factory *pf, pj_pool_t *pool);
static void cpool_dump_status(pj_pool_factory *factory, pj_bool_t detail );

static pj_size_t pool_sizes[PJ_CACHING_POOL_ARRAY_SIZE] = 
{
    256, 512, 1024, 2048, 4096, 8192, 12288, 16384, 
    20480, 24576, 28672, 32768, 40960, 49152, 57344, 65536
};


PJ_DEF(void) pj_caching_pool_init( pj_caching_pool *cp, 
				   const pj_pool_factory_policy *policy,
				   pj_size_t max_capacity)
{
    int i;

    pj_memset(cp, 0, sizeof(*cp));
    
    cp->max_capacity = max_capacity;
    pj_list_init(&cp->used_list);
    for (i=0; i<PJ_CACHING_POOL_ARRAY_SIZE; ++i)
	pj_list_init(&cp->free_list[i]);

    pj_memcpy(&cp->factory.policy, policy, sizeof(pj_pool_factory_policy));
    cp->factory.create_pool = &cpool_create_pool;
    cp->factory.release_pool = &cpool_release_pool;
    cp->factory.dump_status = &cpool_dump_status;
}

PJ_DEF(void) pj_caching_pool_destroy( pj_caching_pool *cp )
{
    int i;
    pj_pool_t *pool;

    /* Delete all pool in free list */
    for (i=0; i < PJ_CACHING_POOL_ARRAY_SIZE; ++i) {
	pj_pool_t *pool = cp->free_list[i].next;
	pj_pool_t *next;
	for (; pool != (void*)&cp->free_list[i]; pool = next) {
	    next = pool->next;
	    pj_list_erase(pool);
	    pj_pool_destroy_int(pool);
	}
    }

    /* Delete all pools in used list */
    pool = cp->used_list.next;
    while (pool != (pj_pool_t*) &cp->used_list) {
	pj_pool_t *next = pool->next;
	pj_list_erase(pool);
	pj_pool_destroy_int(pool);
	pool = next;
    }
}

static pj_pool_t* cpool_create_pool(pj_pool_factory *pf, 
					      const char *name, 
					      pj_size_t initial_size, 
					      pj_size_t increment_sz, 
					      pj_pool_callback *callback)
{
    pj_caching_pool *cp = (pj_caching_pool*)pf;
    pj_pool_t *pool;
    int idx;

    /* Use pool factory's policy when callback is NULL */
    if (callback == NULL) {
	callback = pf->policy.callback;
    }

    /* Search the suitable size for the pool. 
     * We'll just do linear search to the size array, as the array size itself
     * is only a few elements. Binary search I suspect will be less efficient
     * for this purpose.
     */
    for (idx=0; 
	 idx < PJ_CACHING_POOL_ARRAY_SIZE && pool_sizes[idx] < initial_size; 
	 ++idx)
	;

    /* Check whether there's a pool in the list. */
    if (idx==PJ_CACHING_POOL_ARRAY_SIZE || pj_list_empty(&cp->free_list[idx])) {
	/* No pool is available. */
	/* Set minimum size. */
	if (idx < PJ_CACHING_POOL_ARRAY_SIZE)
	    initial_size =  pool_sizes[idx];

	/* Create new pool */
	pool = pj_pool_create_int(&cp->factory, name, initial_size, 
				  increment_sz, callback);
	if (!pool)
	    return NULL;

    } else {
	/* Get one pool from the list. */
	pool = cp->free_list[idx].next;
	pj_list_erase(pool);

	/* Initialize the pool. */
	pj_pool_init_int(pool, name, increment_sz, callback);

	/* Update pool manager's free capacity. */
	cp->capacity -= pj_pool_get_capacity(pool);

	PJ_LOG(5, (pool->obj_name, "pool reused, size=%u", pool->capacity));
    }

    /* Put in used list. */
    pj_list_insert_before( &cp->used_list, pool );

    /* Increment used count. */
    ++cp->used_count;
    return pool;
}

static void cpool_release_pool( pj_pool_factory *pf, pj_pool_t *pool)
{
    pj_caching_pool *cp = (pj_caching_pool*)pf;
    int i;

    /* Erase from the used list. */
    pj_list_erase(pool);

    /* Decrement used count. */
    --cp->used_count;

    /* Destroy the pool if the size is greater than our size or if the total
     * capacity in our recycle list (plus the size of the pool) exceeds 
     * maximum capacity.
   . */
    if (pool->capacity > pool_sizes[PJ_CACHING_POOL_ARRAY_SIZE-1] ||
	cp->capacity + pool->capacity > cp->max_capacity)
    {
	pj_pool_destroy_int(pool);
	return;
    }

    /* Reset pool. */
    PJ_LOG(4, (pool->obj_name, "recycle(): cap=%d, used=%d(%d%%)", 
	       pool->capacity, pool->used_size, pool->used_size*100/pool->capacity));
    pj_pool_reset(pool);

    /*
     * Otherwise put the pool in our recycle list.
     */
    for (i=0; i < PJ_CACHING_POOL_ARRAY_SIZE && pool_sizes[i] != pool->capacity; ++i)
	;

    pj_assert( i != PJ_CACHING_POOL_ARRAY_SIZE );
    if (i == PJ_CACHING_POOL_ARRAY_SIZE) {
	/* Something has gone wrong with the pool. */
	pj_pool_destroy_int(pool);
	return;
    }

    pj_list_insert_after(&cp->free_list[i], pool);
    cp->capacity += pool->capacity;
}

static void cpool_dump_status(pj_pool_factory *factory, pj_bool_t detail )
{
#if PJ_LOG_MAX_LEVEL >= 3
    pj_caching_pool *cp = (pj_caching_pool*)factory;
    PJ_LOG(3,("cachpool", " Dumping caching pool:"));
    PJ_LOG(3,("cachpool", "   Capacity=%u, max_capacity=%u, used_cnt=%u", \
			     cp->capacity, cp->max_capacity, cp->used_count));
    if (detail) {
	pj_pool_t *pool = cp->used_list.next;
	pj_uint32_t total_used = 0, total_capacity = 0;
        PJ_LOG(3,("cachpool", "  Dumping all active pools:"));
	while (pool != (void*)&cp->used_list) {
	    PJ_LOG(3,("cachpool", "   %12s: %8d of %8d (%d%%) used", pool->obj_name, 
				  pool->used_size, pool->capacity,
				  pool->used_size*100/pool->capacity));
	    total_used += pool->used_size;
	    total_capacity += pool->capacity;
	    pool = pool->next;
	}
	PJ_LOG(3,("cachpool", "  Total %9d of %9d (%d %%) used!",
			      total_used, total_capacity,
			      total_used * 100 / total_capacity));
    }
#endif
}

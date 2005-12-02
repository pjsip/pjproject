/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/pool_dbg_win32.c,v 1.1 2005/12/02 20:02:30 nn Exp $ */
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

/* Only if we ARE debugging memory allocations. */
#if PJ_POOL_DEBUG

#include <pj/list.h>
#include <pj/log.h>

#include <stdlib.h>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct memory_entry
{
    PJ_DECL_LIST_MEMBER(struct memory_entry)
    void *ptr;
    char *file;
    int   line;
} memory_entry;

struct pj_pool_t
{
    char	    obj_name[32];
    HANDLE	    hHeap;
    memory_entry    first;
    pj_size_t	    initial_size;
    pj_size_t	    increment;
    pj_size_t	    used_size;
    char	   *file;
    int		    line;
};

PJ_DEF(void) pj_pool_set_functions( void *(*malloc_func)(pj_size_t),
				     void (*free_func)(void *ptr, pj_size_t))
{
    /* Ignored. */
    PJ_UNUSED_ARG(malloc_func)
    PJ_UNUSED_ARG(free_func)
}

PJ_DEF(pj_pool_t*) pj_pool_create_dbg( const char *name,
				       pj_size_t initial_size, 
				       pj_size_t increment_size,
				       pj_pool_callback *callback,
				       char *file, int line)
{
    pj_pool_t *pool;
    HANDLE hHeap;

    PJ_UNUSED_ARG(callback)

    /* Create Win32 heap for the pool. */
    hHeap = HeapCreate(HEAP_GENERATE_EXCEPTIONS|HEAP_NO_SERIALIZE,
		       initial_size, 0);
    if (!hHeap) {
	return NULL;
    }


    /* Create and initialize the pool structure. */
    pool = HeapAlloc(hHeap, HEAP_GENERATE_EXCEPTIONS|HEAP_NO_SERIALIZE, 
		     sizeof(*pool));
    memset(pool, 0, sizeof(*pool));
    pool->file = file;
    pool->line = line;
    pool->hHeap = hHeap;
    pool->initial_size = initial_size;
    pool->increment = increment_size;
    pool->used_size = 0;

    /* Set name. */
    if (name) {
	if (strchr(name, '%') != NULL) {
	    sprintf(pool->obj_name, name, pool);
	} else {
	    strncpy(pool->obj_name, name, PJ_MAX_OBJ_NAME);
	}
    } else {
	pool->obj_name[0] = '\0';
    }

    /* List pool's entry. */
    pj_list_init(&pool->first);

    PJ_LOG(3,(pool->obj_name, "Pool created"));
    return pool;
}

PJ_DEF(void) pj_pool_destroy( pj_pool_t *pool )
{
    memory_entry *entry;

    PJ_LOG(3,(pool->obj_name, "Destoying pool, init_size=%u, used=%u",
			      pool->initial_size, pool->used_size));

    if (!HeapValidate( pool->hHeap, HEAP_NO_SERIALIZE, pool)) {
	PJ_LOG(2,(pool->obj_name, "Corrupted pool structure, allocated in %s:%d", 
		  pool->file, pool->line));
    }

    /* Validate all memory entries in the pool. */
    for (entry=pool->first.next; entry != &pool->first; entry = entry->next) {
	if (!HeapValidate( pool->hHeap, HEAP_NO_SERIALIZE, entry)) {
	    PJ_LOG(2,(pool->obj_name, "Corrupted pool entry, allocated in %s:%d", 
		      entry->file, entry->line));
	}

	if (!HeapValidate( pool->hHeap, HEAP_NO_SERIALIZE, entry->ptr)) {
	    PJ_LOG(2,(pool->obj_name, "Corrupted pool memory, allocated in %s:%d", 
		      entry->file, entry->line));
	}
    }

    /* Destroy heap. */
    HeapDestroy(pool->hHeap);
}

PJ_DEF(void) pj_pool_reset( pj_pool_t *pool )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(pool)
}

PJ_DEF(pj_size_t) pj_pool_get_capacity( pj_pool_t *pool )
{
    PJ_UNUSED_ARG(pool)
    return 0;
}

PJ_DEF(pj_size_t) pj_pool_get_used_size( pj_pool_t *pool )
{
    PJ_UNUSED_ARG(pool)
    return 0;
}

PJ_DEF(pj_size_t) pj_pool_get_request_count( pj_pool_t *pool )
{
    PJ_UNUSED_ARG(pool)
    return 0;
}

PJ_DEF(void*) pj_pool_alloc_dbg( pj_pool_t *pool, pj_size_t size, 
				 char *file, int line)
{
    memory_entry *entry;
    int entry_size;

    entry_size = sizeof(*entry);
    entry = HeapAlloc(pool->hHeap, HEAP_GENERATE_EXCEPTIONS|HEAP_NO_SERIALIZE,
		      entry_size);
    entry->file = file;
    entry->line = line;
    entry->ptr = HeapAlloc(pool->hHeap, HEAP_GENERATE_EXCEPTIONS|HEAP_NO_SERIALIZE,
			   size);
    pj_list_insert_before( &pool->first, entry);

    pool->used_size += size;
    return entry->ptr;
}

PJ_DEF(void*) pj_pool_calloc_dbg( pj_pool_t *pool, pj_size_t count, pj_size_t elem,
				  char *file, int line)
{
    void *ptr;
    ptr = pj_pool_alloc_dbg(pool, count*elem, file, line);
    memset(ptr, 0, count*elem);
    return ptr;
}


PJ_DEF(void) pj_pool_pool_init( pj_pool_pool_t *pool_pool, 
				 pj_size_t max_capacity)
{
    PJ_UNUSED_ARG(pool_pool)
    PJ_UNUSED_ARG(max_capacity)
}

PJ_DEF(void) pj_pool_pool_destroy( pj_pool_pool_t *pool_pool )
{
    PJ_UNUSED_ARG(pool_pool)
}

PJ_DEF(pj_pool_t*) pj_pool_pool_create_pool( pj_pool_pool_t *pool_pool,
					      const char *name,
					      pj_size_t initial_size, 
					      pj_size_t increment_size,
					      pj_pool_callback *callback)
{
    PJ_UNUSED_ARG(pool_pool)
    return pj_pool_create(name, initial_size, increment_size, callback);
}

PJ_DEF(void) pj_pool_pool_release_pool( pj_pool_pool_t *pool_pool,
					pj_pool_t *pool )
{
    PJ_UNUSED_ARG(pool_pool)
    pj_pool_destroy(pool);
}


#endif	/* PJ_POOL_DEBUG */

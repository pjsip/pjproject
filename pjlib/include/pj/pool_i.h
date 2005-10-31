/* $Header: /pjproject-0.3/pjlib/include/pj/pool_i.h 6     10/14/05 12:26a Bennylp $ */


#include <pj/string.h>

PJ_DECL(void*) pj_pool_allocate_find(pj_pool_t *pool, unsigned size);

PJ_IDEF(pj_size_t) pj_pool_get_capacity( pj_pool_t *pool )
{
    return pool->capacity;
}

PJ_IDEF(pj_size_t) pj_pool_get_used_size( pj_pool_t *pool )
{
    return pool->used_size;
}

PJ_IDEF(void*) pj_pool_alloc_from_block( pj_pool_t *pool,
					 pj_pool_block *block, pj_size_t size )
{
    /* The operation below is valid for size==0. 
     * When size==0, the function will return the pointer to the pool
     * memory address, but no memory will be allocated.
     */
    if (size & (PJ_POOL_ALIGNMENT-1)) {
	size &= ~(PJ_POOL_ALIGNMENT-1);
	size += PJ_POOL_ALIGNMENT;
    }
    if ((unsigned)(block->end - block->cur) >= size) {
	void *ptr = block->cur;
	block->cur += size;
	pool->used_size += size;
	return ptr;
    }
    return NULL;
}

PJ_IDEF(void*) pj_pool_alloc( pj_pool_t *pool, pj_size_t size)
{
    pj_pool_block *block = pool->block_list.next;
    void *ptr = pj_pool_alloc_from_block(pool, block, size);
    if (!ptr)
	ptr = pj_pool_allocate_find(pool, size);
    return ptr;
}


PJ_IDEF(void*) pj_pool_calloc( pj_pool_t *pool, pj_size_t count, pj_size_t size)
{
    void *buf = pj_pool_alloc( pool, size*count);
    if (buf)
	pj_memset(buf, 0, size * count);
    return buf;
}

PJ_IDEF(const char *) pj_pool_getobjname( const pj_pool_t *pool )
{
    return pool->obj_name;
}

PJ_IDEF(pj_pool_t*) pj_pool_create( pj_pool_factory *f, 
				    const char *name,
				    pj_size_t initial_size, 
				    pj_size_t increment_size,
				    pj_pool_callback *callback)
{
    return (*f->create_pool)(f, name, initial_size, increment_size, callback);
}

PJ_IDEF(void) pj_pool_release( pj_pool_t *pool )
{
    (*pool->factory->release_pool)(pool->factory, pool);
}


/* $Id$
 *
 */
#include <pj/pool.h>
#include <pj/except.h>
#include <pj/os.h>


static void *default_block_alloc(pj_pool_factory *factory, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(factory);

    return kmalloc(size, GFP_ATOMIC);
}

static void default_block_free(pj_pool_factory *factory, 
			       void *mem, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(size);

    kfree(mem);
}

static void default_pool_callback(pj_pool_t *pool, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(size);

    PJ_THROW(PJ_NO_MEMORY_EXCEPTION);
}

pj_pool_factory_policy pj_pool_factory_default_policy = 
{
    &default_block_alloc,
    &default_block_free,
    &default_pool_callback,
    0
};


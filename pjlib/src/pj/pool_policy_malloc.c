/* $Header: /pjproject-0.3/pjlib/src/pj/pool_policy_malloc.c 2     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/pool_policy_malloc.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     9/21/05 1:37p Bennylp
 * Renamed from pool_policy.c
 * 
 * 3     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#include <pj/pool.h>
#include <pj/except.h>
#include <pj/os.h>
#include <pj/compat/malloc.h>

/*
 * This file contains pool default policy definition and implementation.
 */


static void *default_block_alloc(pj_pool_factory *factory, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(size);

    return malloc(size);
}

static void default_block_free(pj_pool_factory *factory, void *mem, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(size);

    free(mem);
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

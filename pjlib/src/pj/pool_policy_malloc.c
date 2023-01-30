/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pj/pool.h>
#include <pj/except.h>
#include <pj/os.h>
#include <pj/compat/malloc.h>

#if !PJ_HAS_POOL_ALT_API

/*
 * This file contains pool default policy definition and implementation.
 */
#include "pool_signature.h"


static void *default_block_alloc(pj_pool_factory *factory, pj_size_t size)
{
    void *p;

    PJ_CHECK_STACK();

    if (factory->on_block_alloc) {
        int rc;
        rc = factory->on_block_alloc(factory, size);
        if (!rc)
            return NULL;
    }

    p = malloc(size+(SIG_SIZE << 1));

    if (p == NULL) {
        if (factory->on_block_free) 
            factory->on_block_free(factory, size);
    } else {
        /* Apply signature when PJ_SAFE_POOL is set. It will move
         * "p" pointer forward.
         */
        APPLY_SIG(p, size);
    }

    return p;
}

static void default_block_free(pj_pool_factory *factory, void *mem, 
                               pj_size_t size)
{
    PJ_CHECK_STACK();

    if (factory->on_block_free) 
        factory->on_block_free(factory, size);

    /* Check and remove signature when PJ_SAFE_POOL is set. It will
     * move "mem" pointer backward.
     */
    REMOVE_SIG(mem, size);

    /* Note that when PJ_SAFE_POOL is set, the actual size of the block
     * is size + SIG_SIZE*2.
     */

    free(mem);
}

static void default_pool_callback(pj_pool_t *pool, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(size);

    PJ_THROW(PJ_NO_MEMORY_EXCEPTION);
}

PJ_DEF_DATA(pj_pool_factory_policy) pj_pool_factory_default_policy =
{
    &default_block_alloc,
    &default_block_free,
    &default_pool_callback,
    0
};

PJ_DEF(const pj_pool_factory_policy*) pj_pool_factory_get_default_policy(void)
{
    return &pj_pool_factory_default_policy;
}


#endif  /* PJ_HAS_POOL_ALT_API */

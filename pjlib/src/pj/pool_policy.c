/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/pool_policy.c,v 1.1 2005/12/02 20:02:30 nn Exp $ */
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
#include <pj/except.h>

/*
 * This file contains pool default policy definition and implementation.
 */
#include <stdlib.h>

static void *default_block_alloc(pj_pool_factory *factory, pj_size_t size)
{
    PJ_UNUSED_ARG(factory)
    PJ_UNUSED_ARG(size)

    return malloc(size);
}

static void default_block_free(pj_pool_factory *factory, void *mem, pj_size_t size)
{
    PJ_UNUSED_ARG(factory)
    PJ_UNUSED_ARG(size)

    free(mem);
}

static void default_pool_callback(pj_pool_t *pool, pj_size_t size)
{
    PJ_UNUSED_ARG(pool)
    PJ_UNUSED_ARG(size)

    PJ_THROW(PJ_NO_MEMORY_EXCEPTION);
}

pj_pool_factory_policy pj_pool_factory_default_policy = 
{
    &default_block_alloc,
    &default_block_free,
    &default_pool_callback,
    0
};

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
#include "test.h"
#include <pjlib.h>

/**
 * \page page_pjlib_atomic_test Test: Atomic Variable
 *
 * This file provides implementation of \b atomic_test(). It tests the
 * functionality of the atomic variable API.
 *
 * \section atomic_test_sec Scope of the Test
 *
 * API tested:
 *  - pj_atomic_create()
 *  - pj_atomic_get()
 *  - pj_atomic_inc()
 *  - pj_atomic_dec()
 *  - pj_atomic_set()
 *  - pj_atomic_destroy()
 *
 *
 * This file is <b>pjlib-test/atomic.c</b>
 *
 * \include pjlib-test/atomic.c
 */


#if INCLUDE_ATOMIC_TEST

#define THIS_FILE   "atomic.c"
#define ERR(r__)    { rc=r__; goto on_return; }

int atomic_test(void)
{
    pj_pool_t *pool;
    pj_atomic_t *atomic_var;
    int rc=0;

    PJ_TEST_NOT_NULL( (pool=pj_pool_create(mem, NULL, 4096, 0, NULL)),
                      NULL, ERR(-10));
    /* create() */
    PJ_TEST_SUCCESS( pj_atomic_create(pool, 111, &atomic_var), NULL, ERR(-20));

    /* get: check the value. */
    PJ_TEST_EQ( pj_atomic_get(atomic_var), 111, NULL, ERR(-30));

    /* increment. */
    pj_atomic_inc(atomic_var);
    PJ_TEST_EQ( pj_atomic_get(atomic_var), 112, NULL, ERR(-40));

    /* decrement. */
    pj_atomic_dec(atomic_var);
    PJ_TEST_EQ( pj_atomic_get(atomic_var), 111, NULL, ERR(-50));

    /* set */
    pj_atomic_set(atomic_var, 211);
    PJ_TEST_EQ( pj_atomic_get(atomic_var), 211, NULL, ERR(-60));

    /* add */
    pj_atomic_add(atomic_var, 10);
    PJ_TEST_EQ( pj_atomic_get(atomic_var), 221, NULL, ERR(-60));

    /* check the value again. */
    PJ_TEST_EQ( pj_atomic_get(atomic_var), 221, NULL, ERR(-70));

    /* destroy */
    PJ_TEST_SUCCESS( pj_atomic_destroy(atomic_var), NULL, ERR(-80));

on_return:
    pj_pool_release(pool);
    return rc;
}


#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_atomic_test;
#endif  /* INCLUDE_ATOMIC_TEST */


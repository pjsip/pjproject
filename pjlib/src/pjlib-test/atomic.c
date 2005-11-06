/* $Id$
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

int atomic_test(void)
{
    pj_pool_t *pool;
    pj_atomic_t *atomic_var;
    pj_status_t rc;

    pool = pj_pool_create(mem, NULL, 4096, 0, NULL);
    if (!pool)
        return -10;

    /* create() */
    rc = pj_atomic_create(pool, 111, &atomic_var);
    if (rc != 0) {
        return -20;
    }

    /* get: check the value. */
    if (pj_atomic_get(atomic_var) != 111)
        return -30;

    /* increment. */
    pj_atomic_inc(atomic_var);
    if (pj_atomic_get(atomic_var) != 112)
        return -40;

    /* decrement. */
    pj_atomic_dec(atomic_var);
    if (pj_atomic_get(atomic_var) != 111)
        return -50;

    /* set */
    pj_atomic_set(atomic_var, 211);
    if (pj_atomic_get(atomic_var) != 211)
        return -60;

    /* add */
    pj_atomic_add(atomic_var, 10);
    if (pj_atomic_get(atomic_var) != 221)
        return -60;

    /* check the value again. */
    if (pj_atomic_get(atomic_var) != 221)
        return -70;

    /* destroy */
    rc = pj_atomic_destroy(atomic_var);
    if (rc != 0)
        return -80;

    pj_pool_release(pool);

    return 0;
}


#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_atomic_test;
#endif  /* INCLUDE_ATOMIC_TEST */


/* $Header: /pjproject-0.3/pjlib/src/pjlib-test/pool.c 2     10/14/05 12:26a Bennylp $
 */
/*
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/pool.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 */
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/log.h>
#include "test.h"

/**
 * \page page_pjlib_pool_test Test: Pool
 *
 * This file provides implementation of \b pool_test(). It tests the
 * functionality of the memory pool.
 *
 *
 * This file is <b>pjlib-test/pool.c</b>
 *
 * \include pjlib-test/pool.c
 */


#if INCLUDE_POOL_TEST

#define SIZE	4096

/* Normally we should throw exception when memory alloc fails.
 * Here we do nothing so that the flow will go back to original caller,
 * which will test the result using NULL comparison. Normally caller will
 * catch the exception instead of checking for NULLs.
 */
static void null_callback(pj_pool_t *pool, pj_size_t size)
{
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(size);
}

#define GET_FREE(p)	(pj_pool_get_capacity(p)-pj_pool_get_used_size(p))

/* Test that the capacity and used size reported by the pool is correct. 
 */
static int capacity_test(void)
{
    pj_pool_t *pool = pj_pool_create(mem, NULL, SIZE, 0, &null_callback);
    pj_size_t freesize;

    PJ_LOG(3,("test", "...capacity_test()"));

    if (!pool)
	return -200;

    freesize = GET_FREE(pool);

    if (pj_pool_alloc(pool, freesize) == NULL) {
	PJ_LOG(3,("test", "...error: wrong freesize %u reported",
			  freesize));
	pj_pool_release(pool);
	return -210;
    }

    pj_pool_release(pool);
    return 0;
}

/* Test function to drain the pool's space. 
 */
static int drain_test(pj_size_t size, pj_size_t increment)
{
    pj_pool_t *pool = pj_pool_create(mem, NULL, size, increment, 
				     &null_callback);
    pj_size_t freesize;
    void *p;
    int status = 0;
    
    PJ_LOG(3,("test", "...drain_test(%d,%d)", size, increment));

    if (!pool)
	return -10;

    /* Get free size */
    freesize = GET_FREE(pool);
    if (freesize < 1) {
    	status=-15; 
	goto on_error;
    }

    /* Drain the pool until there's nothing left. */
    while (freesize > 0) {
	int size;

	if (freesize > 255)
	    size = ((pj_rand() & 0x000000FF) + 4) & ~0x03L;
	else
	    size = freesize;

	p = pj_pool_alloc(pool, size);
	if (!p) {
	    status=-20; goto on_error;
	}

	freesize -= size;
    }

    /* Check that capacity is zero. */
    if (GET_FREE(pool) != 0) {
	PJ_LOG(3,("test", "....error: returned free=%u (expecting 0)",
		  GET_FREE(pool)));
	status=-30; goto on_error;
    }

    /* Try to allocate once more */
    p = pj_pool_alloc(pool, 257);
    if (!p) {
	status=-40; goto on_error;
    }

    /* Check that capacity is NOT zero. */
    if (GET_FREE(pool) == 0) {
	status=-50; goto on_error;
    }


on_error:
    pj_pool_release(pool);
    return status;
}

int pool_test(void)
{
    enum { LOOP = 2 };
    int loop;
    int rc;

    rc = capacity_test();
    if (rc) return rc;

    for (loop=0; loop<LOOP; ++loop) {
	/* Test that the pool should grow automaticly. */
	rc = drain_test(SIZE, SIZE);
	if (rc != 0) return rc;

	/* Test situation where pool is not allowed to grow. 
 	 * We expect the test to return correct error.
	 */
	rc = drain_test(SIZE, 0);
	if (rc != -40) return rc;
    }

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_pool_test;
#endif	/* INCLUDE_POOL_TEST */


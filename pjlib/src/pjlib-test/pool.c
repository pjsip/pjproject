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
#include <pj/pool_buf.h>
#include <pj/rand.h>
#include <pj/log.h>
#include <pj/except.h>
#include <pj/unittest.h>
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

#define THIS_FILE   "pool.c"
#define SIZE        4096

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

#define GET_FREE(p)     (pj_pool_get_capacity(p)-pj_pool_get_used_size(p))

/* Test that the capacity and used size reported by the pool is correct. 
 */
static int capacity_test(void)
{
    pj_pool_t *pool = pj_pool_create(mem, NULL, SIZE, 0, &null_callback);
    pj_size_t freesize;

    PJ_LOG(3,("test", "...capacity_test()"));

    PJ_TEST_NOT_NULL(pool, NULL, return -200);

    freesize = GET_FREE(pool);
    PJ_TEST_NOT_NULL(pj_pool_alloc(pool, freesize), "wrong freesize reported",
                     { pj_pool_release(pool); return -210; });

    pj_pool_release(pool);

    return 0;
}

/* Test that the alignment works. */
static int pool_alignment_test(void)
{
    pj_pool_t *pool, *pool2;
    void *ptr;
    enum { MEMSIZE = 64, LOOP = 100, POOL_ALIGNMENT_TEST = 4*PJ_POOL_ALIGNMENT };
    unsigned i;
    int rc = 0;

    PJ_LOG(3,("test", "...alignment test"));

    /* Test pj_pool_allocate_find() when alignment is so big */
    pool = pj_pool_aligned_create(mem, NULL,
                                  50 + sizeof(pj_pool_t) + sizeof(pj_pool_block),
                                  50, 4,
                                  &null_callback);
    ptr = pj_pool_aligned_alloc(pool, 256, 1);
    pj_pool_release(pool);
    PJ_TEST_NOT_NULL(ptr, NULL, return -300);

    pool = pj_pool_create(mem, NULL, PJ_POOL_SIZE+MEMSIZE, MEMSIZE, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, return -304);

    pool2 = pj_pool_aligned_create(mem, NULL, PJ_POOL_SIZE + MEMSIZE, MEMSIZE,
                                   POOL_ALIGNMENT_TEST, NULL);
    PJ_TEST_NOT_NULL(pool2, NULL, { pj_pool_release(pool); return -307; });

#define IS_ALIGNED(p)   ((((unsigned long)(pj_ssize_t)p) & \
                           (PJ_POOL_ALIGNMENT-1)) == 0)
#define IS_ALIGNED2(p)   ((((unsigned long)(pj_ssize_t)p) & \
                           (POOL_ALIGNMENT_TEST-1)) == 0)

    for (i=0; i<LOOP; ++i) {
        /* Test first allocation */
        if (i % 2)
        {
            ptr = pj_pool_aligned_alloc(pool, POOL_ALIGNMENT_TEST, 1);
            PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-310; goto on_return; });

            ptr = pj_pool_aligned_alloc(pool2, PJ_POOL_ALIGNMENT, 1);
            PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-315; goto on_return; });
        }
        else
        {
            ptr = pj_pool_alloc(pool, 1);
            PJ_TEST_TRUE(IS_ALIGNED(ptr), NULL, { rc=-320; goto on_return; });

            ptr = pj_pool_alloc(pool2, 1);
            PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-325; goto on_return; });
        }

        /* Test subsequent allocation */
        ptr = pj_pool_alloc(pool, 1);
        PJ_TEST_TRUE(IS_ALIGNED(ptr), NULL, { rc=-330; goto on_return; });

        ptr = pj_pool_aligned_alloc(pool, POOL_ALIGNMENT_TEST, 1);
        PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-335; goto on_return; });

        ptr = pj_pool_alloc(pool2, 1);
        PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-340; goto on_return; });

        ptr = pj_pool_aligned_alloc(pool2, PJ_POOL_ALIGNMENT, 1);
        PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-345; goto on_return; });


        /* Test allocation after new block is created */
        ptr = pj_pool_alloc(pool, MEMSIZE*2+1);
        PJ_TEST_TRUE(IS_ALIGNED(ptr), NULL, { rc=-350; goto on_return; });

        ptr = pj_pool_aligned_alloc(pool, POOL_ALIGNMENT_TEST, MEMSIZE*2+1);
        PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-355; goto on_return; });
        
        ptr = pj_pool_alloc(pool2, MEMSIZE*2+1);
        PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-360; goto on_return; });
        
        ptr = pj_pool_aligned_alloc(pool2, PJ_POOL_ALIGNMENT, MEMSIZE*2+1);
        PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc=-365; goto on_return; });

        /* Reset the pool */
        pj_pool_reset(pool);
        pj_pool_reset(pool2);

    }

    /* Done */
on_return:
    pj_pool_release(pool);
    pj_pool_release(pool2);

    return rc;
}

/* Test that the alignment works for pool on buf. */
static int pool_buf_alignment_test(void)
{
    pj_pool_t *pool;
    char buf[512];
    void *ptr;
    enum { LOOP = 100, POOL_ALIGNMENT_TEST = 4*PJ_POOL_ALIGNMENT};
    unsigned i;
    int rc = 0;

    PJ_LOG(3,("test", "...pool_buf alignment test"));

    pool = pj_pool_create_on_buf(NULL, buf, sizeof(buf));
    PJ_TEST_NOT_NULL(pool, NULL, return -400);

    for (i=0; i<LOOP; ++i) {
        /* Test first allocation */
        if (i % 2) {
            ptr = pj_pool_aligned_alloc(pool, POOL_ALIGNMENT_TEST, 1);
            PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc = -410; goto on_return; });
        } else {
            ptr = pj_pool_alloc(pool, 1);
            PJ_TEST_TRUE(IS_ALIGNED(ptr), NULL, { rc = -415; goto on_return; });
        }

        /* Test subsequent allocation */
        ptr = pj_pool_alloc(pool, 1);
        PJ_TEST_TRUE(IS_ALIGNED(ptr), NULL, { rc = -420; goto on_return; });

        ptr = pj_pool_aligned_alloc(pool, POOL_ALIGNMENT_TEST, 1);
        PJ_TEST_TRUE(IS_ALIGNED2(ptr), NULL, { rc = -425; goto on_return; });

        /* Reset the pool */
        pj_pool_reset(pool);
    }

    /* Done */
on_return:
    pj_pool_release(pool);

    return rc;
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
    
    PJ_LOG(3,("test", "...drain_test(%lu,%lu)", (unsigned long)size,
              (unsigned long)increment));
    PJ_TEST_NOT_NULL(pool, NULL, return -10);
    

    /* Get free size */
    freesize = GET_FREE(pool);
    PJ_TEST_GTE(freesize, 1, NULL, { status=-15; goto on_error;});

    /* Drain the pool until there's nothing left. */
    while (freesize > 0) {
        int size2;

        if (freesize > 255)
            size2 = ((pj_rand() & 0x000000FF) + PJ_POOL_ALIGNMENT) & 
                   ~(PJ_POOL_ALIGNMENT - 1);
        else
            size2 = (int)freesize;

        p = pj_pool_alloc(pool, size2);
        PJ_TEST_NOT_NULL(p, NULL, { status=-20; goto on_error;});

        freesize -= size2;
    }

    /* Check that capacity is zero. */
    PJ_TEST_EQ(GET_FREE(pool), 0, NULL, { status=-30; goto on_error;});

    /* Try to allocate once more */
    p = pj_pool_alloc(pool, 257);
    PJ_TEST_NOT_NULL(p, NULL, { status=-40; goto on_error;});

    /* Check that capacity is NOT zero. */
    PJ_TEST_NEQ(GET_FREE(pool), 0, NULL, { status=-50; goto on_error;});


on_error:
    pj_pool_release(pool);
    return status;
}

/* Test the buffer based pool */
static int pool_buf_test(void)
{
    enum { STATIC_BUF_SIZE = 40 };
    /* 16 is the internal struct in pool_buf */
    static char buf[ STATIC_BUF_SIZE + sizeof(pj_pool_t) + 
                     sizeof(pj_pool_block) + 2 * PJ_POOL_ALIGNMENT];
    pj_pool_t *pool;
    void *p;
    PJ_USE_EXCEPTION;

    PJ_LOG(3,("test", "...pool_buf test"));

    pool = pj_pool_create_on_buf("no name", buf, sizeof(buf));
    PJ_TEST_NOT_NULL(pool, NULL, return -70);

    /* Drain the pool */
    PJ_TRY {
        PJ_TEST_NOT_NULL(pj_pool_alloc(pool, STATIC_BUF_SIZE/2), NULL, return -75);
        PJ_TEST_NOT_NULL(pj_pool_alloc(pool, STATIC_BUF_SIZE/2), NULL, return -76);
    }
    PJ_CATCH_ANY {
        PJ_TEST_TRUE(PJ_FALSE, "Caught exception", return -77);
    }
    PJ_END;

    /* On the next alloc, exception should be thrown */
    PJ_TRY {
        p = pj_pool_alloc(pool, STATIC_BUF_SIZE);
        PJ_TEST_TRUE(p==NULL, NULL, return -78);
    }
    PJ_CATCH_ANY {
        /* This is the expected result */
    }
    PJ_END;

    /* Done */
    return 0;
}


int pool_test(void)
{
    enum { LOOP = 2 };
    int rc;

#if PJ_HAS_POOL_ALT_API == 0
    rc = capacity_test();
    if (rc) return rc;
#endif  //PJ_HAS_POOL_ALT_API == 0

    rc = pool_alignment_test();
    if (rc) return rc;

    rc = pool_buf_alignment_test();
    if (rc) return rc;

#if PJ_HAS_POOL_ALT_API == 0
    int loop;
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

    rc = pool_buf_test();
    if (rc != 0)
        return rc;
#endif  //PJ_HAS_POOL_ALT_API == 0


    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_pool_test;
#endif  /* INCLUDE_POOL_TEST */


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


/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_fifobuf_test;

#if INCLUDE_FIFOBUF_TEST

#include <pjlib.h>

#define THIS_FILE   "fifobuf.c"

enum {
    SZ = sizeof(unsigned),
};

static int fifobuf_size_test()
{
    enum { SIZE = 32 };
    char before[8];
    char buffer[SIZE];
    char after[8];
    char zero[8];
    void *p0, *p1;
    pj_fifobuf_t fifo;

    pj_bzero(before, sizeof(before));
    pj_bzero(after, sizeof(after));
    pj_bzero(zero, sizeof(zero));

    pj_fifobuf_init (&fifo, buffer, sizeof(buffer));

    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, NULL, return -11);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, NULL, return -12);

    p0 = pj_fifobuf_alloc(&fifo, 16);
    p1 = pj_fifobuf_alloc(&fifo, 4);
    PJ_UNUSED_ARG(p1);

    pj_fifobuf_free(&fifo, p0);

    PJ_TEST_EQ( pj_fifobuf_available_size(&fifo), 16, NULL, return -14);
    PJ_TEST_EQ( pj_memcmp(before, zero, sizeof(zero)), 0, NULL, return -18);
    PJ_TEST_EQ( pj_memcmp(after, zero, sizeof(zero)), 0, NULL, return -19);
    return 0;
}

static int fifobuf_rolling_test()
{
    enum {
        REPEAT=2048,
        N=100,
        MIN_SIZE = sizeof(pj_list),
        MAX_SIZE = 64,
        SIZE=(MIN_SIZE+MAX_SIZE)/2*N,
    };
    pj_list chunks;
    char buffer[SIZE];
    pj_fifobuf_t fifo;
    unsigned rep;

    pj_fifobuf_init(&fifo, buffer, sizeof(buffer));
    pj_list_init(&chunks);

    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, NULL, return -300);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, NULL, return -310);

    pj_srand(0);

    /* Repeat the test */
    for (rep=0; rep<REPEAT; rep++) {
        pj_list *chunk;
        int i, n;

        /* Allocate random number of chunks */
        n = N/2 + (pj_rand() % (N/2)) - pj_list_size(&chunks);
        for (i=0; i<n; ++i) {
            unsigned size = MIN_SIZE + (pj_rand() % (MAX_SIZE-MIN_SIZE));
            chunk = (pj_list*)pj_fifobuf_alloc(&fifo, size);
            if (!chunk)
                break;
            pj_memset(chunk, 0x44, size);
            pj_list_push_back(&chunks, chunk);
        }

        PJ_TEST_GTE(pj_list_size(&chunks), N/2, NULL, return -330);

        /* Free cunks, leave some chunks for the next repeat */
        n = N/4 + (pj_rand() % (N/4));
        while (pj_list_size(&chunks) > n) {
            chunk = chunks.next;
            pj_list_erase(chunk);
            pj_fifobuf_free(&fifo, chunk);
        }
    }

    while (pj_list_size(&chunks)) {
        pj_list *chunk = chunks.next;
        pj_list_erase(chunk);
        pj_fifobuf_free(&fifo, chunk);
    }

    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, NULL, return -350);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, NULL, return -360);

    return 0;
}

static int fifobuf_misc_test()
{
    enum { SIZE = 1024, MAX_ENTRIES = 128, 
           MIN_SIZE = 4, MAX_SIZE = 64,
           LOOP=10000 };
    pj_pool_t *pool;
    pj_fifobuf_t fifo;
    pj_size_t available = SIZE;
    void *entries[MAX_ENTRIES];
    void *buffer;
    int i;

    pool = pj_pool_create(mem, NULL, SIZE+256, 0, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, return -10);

    buffer = pj_pool_alloc(pool, SIZE);
    PJ_TEST_NOT_NULL(buffer, NULL, return -20);

    pj_fifobuf_init (&fifo, buffer, SIZE);
    
    /* Capacity and maximum alloc */
    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, NULL, return -21);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, NULL, return -22);

    entries[0] = pj_fifobuf_alloc(&fifo, SIZE-SZ);
    PJ_TEST_NOT_NULL(entries[0], NULL, return -23);

    pj_fifobuf_free(&fifo, entries[0]);
    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, NULL, return -21);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, NULL, return -22);

    /* Alignment test */
    for (i=0; i<30; ++i) {
        entries[i] = pj_fifobuf_alloc(&fifo, i+1);
        PJ_TEST_NOT_NULL(entries[i], NULL, return -50);
        //fifobuf is no longer aligned.
        //PJ_TEST_EQ(((pj_size_t)entries[i]) % sizeof(unsigned), 0, -60, 
        //           "alignment error");
    }
    for (i=0; i<30; ++i) {
        PJ_TEST_SUCCESS( pj_fifobuf_free(&fifo, entries[i]), NULL, return -70);
    }

    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, NULL, return -31);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, NULL, return -32);

    /* alloc() and free() */
    for (i=0; i<LOOP*MAX_ENTRIES; ++i) {
        int size;
        int c, f;
        c = i%2;
        f = (i+1)%2;
        do {
            size = MIN_SIZE+(pj_rand() % MAX_SIZE);
            entries[c] = pj_fifobuf_alloc (&fifo, size);
        } while (entries[c] == 0);
        if ( i!=0) {
            pj_fifobuf_free(&fifo, entries[f]);
        }
    }
    if (entries[(i+1)%2])
        pj_fifobuf_free(&fifo, entries[(i+1)%2]);

    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, NULL, return -42);

    /* Fifobuf must be empty (==not used) */
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, NULL, return -62);

    pj_pool_release(pool);
    return 0;
}

int fifobuf_test()
{
    int rc;

    rc = fifobuf_size_test();
    if (rc != 0)
        return rc;

    rc = fifobuf_rolling_test();
    if (rc)
        return rc;

    rc = fifobuf_misc_test();
    if (rc)
        return rc;
    
    return 0;
}

#endif  /* INCLUDE_FIFOBUF_TEST */



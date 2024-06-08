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

static int fifobuf_size_test()
{
    enum { SIZE = 32, SZ=sizeof(unsigned) };
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

    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, -11, NULL);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, -12, NULL);

    p0 = pj_fifobuf_alloc(&fifo, 16);
    p1 = pj_fifobuf_alloc(&fifo, 4);
    PJ_UNUSED_ARG(p1);

    pj_fifobuf_free(&fifo, p0);

    PJ_TEST_EQ( pj_fifobuf_available_size(&fifo), 16, -14, NULL);
    PJ_TEST_EQ( pj_memcmp(before, zero, sizeof(zero)), 0, -18, NULL);
    PJ_TEST_EQ( pj_memcmp(after, zero, sizeof(zero)), 0, -19, NULL);
    return 0;
}

int fifobuf_test()
{
    enum { SIZE = 1024, MAX_ENTRIES = 128, 
           MIN_SIZE = 4, MAX_SIZE = 64, SZ = sizeof(unsigned),
           LOOP=10000 };
    pj_pool_t *pool;
    pj_fifobuf_t fifo;
    pj_size_t available = SIZE;
    void *entries[MAX_ENTRIES];
    void *buffer;
    int i;

    i = fifobuf_size_test();
    if (i != 0)
        return i;

    pool = pj_pool_create(mem, NULL, SIZE+256, 0, NULL);
    PJ_TEST_NOT_NULL(pool, -10, NULL);

    buffer = pj_pool_alloc(pool, SIZE);
    PJ_TEST_NOT_NULL(buffer, -20, NULL);

    pj_fifobuf_init (&fifo, buffer, SIZE);
    
    /* Capacity and maximum alloc */
    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, -21, NULL);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, -22, NULL);

    entries[0] = pj_fifobuf_alloc(&fifo, SIZE-SZ);
    PJ_TEST_NOT_NULL(entries[0], -23, NULL);

    pj_fifobuf_free(&fifo, entries[0]);
    PJ_TEST_EQ(pj_fifobuf_capacity(&fifo), SIZE-SZ, -21, NULL);
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, -22, NULL);

    /* Alignment test */
    for (i=0; i<30; ++i) {
        entries[i] = pj_fifobuf_alloc(&fifo, i+1);
        PJ_TEST_NOT_NULL(entries[i], -50, NULL);
        //fifobuf is no longer aligned.
        //PJ_TEST_EQ(((pj_size_t)entries[i]) % sizeof(unsigned), 0, -60, 
        //           "alignment error");
    }
    for (i=0; i<30; ++i) {
        PJ_TEST_SUCCESS( pj_fifobuf_free(&fifo, entries[i]), -70, NULL);
    }

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

    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, -1, NULL);

    /* alloc() and unalloc() */
    entries[0] = pj_fifobuf_alloc (&fifo, MIN_SIZE);
    if (!entries[0]) return -1;
    for (i=0; i<LOOP*MAX_ENTRIES; ++i) {
        int size = MIN_SIZE+(pj_rand() % MAX_SIZE);
        entries[1] = pj_fifobuf_alloc (&fifo, size);
        if (entries[1])
            pj_fifobuf_unalloc(&fifo, entries[1]);
    }
    pj_fifobuf_unalloc(&fifo, entries[0]);
    PJ_TEST_GTE(pj_fifobuf_available_size(&fifo), SIZE-SZ, -2, NULL);

    /* Allocate as much as possible, then free them all. */
    for (i=0; i<LOOP; ++i) {
        int count, j;
        available = pj_fifobuf_available_size(&fifo);
        for (count=0; available>MIN_SIZE+SZ+MAX_SIZE/2 && count < MAX_ENTRIES;) {
            int size = MIN_SIZE+(pj_rand() % MAX_SIZE);
            entries[count] = pj_fifobuf_alloc (&fifo, size);
            if (entries[count]) {
                available = pj_fifobuf_available_size(&fifo);
                ++count;
            }
        }
        for (j=0; j<count; ++j) {
            pj_fifobuf_free (&fifo, entries[j]);
        }
    }

    /* Fifobuf must be empty (==not used) */
    PJ_TEST_EQ(pj_fifobuf_available_size(&fifo), SIZE-SZ, -3, NULL);

    pj_pool_release(pool);
    return 0;
}

#endif  /* INCLUDE_FIFOBUF_TEST */



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
#include <pj/hash.h>
#include <pj/rand.h>
#include <pj/log.h>
#include <pj/pool.h>
#include "test.h"

#if INCLUDE_HASH_TEST

#define HASH_COUNT  31
#define THIS_FILE   "hash_test.c"


static int hash_test_with_key(pj_pool_t *pool, unsigned char key)
{
    pj_hash_table_t *ht;
    unsigned value = 0x12345;
    pj_hash_iterator_t it_buf, *it;
    unsigned *entry;

    PJ_TEST_NOT_NULL( (ht=pj_hash_create(pool, HASH_COUNT)), NULL, return -10);

    pj_hash_set(pool, ht, &key, sizeof(key), 0, &value);

    PJ_TEST_NOT_NULL((entry=(unsigned*)pj_hash_get(ht,&key,sizeof(key),NULL)),
                     NULL, return -20);
    PJ_TEST_EQ( *entry, value, NULL, return -25);
    PJ_TEST_EQ(pj_hash_count(ht), 1, NULL, return -30);
    PJ_TEST_NOT_NULL((it=pj_hash_first(ht, &it_buf)), NULL, return -40);
    PJ_TEST_NOT_NULL((entry=(unsigned*)pj_hash_this(ht, it)), NULL,
                     return -50);
    PJ_TEST_EQ(*entry, value, NULL, return -60);
    PJ_TEST_EQ(pj_hash_next(ht, it), NULL, NULL, return -70);

    /* Erase item */

    pj_hash_set(NULL, ht, &key, sizeof(key), 0, NULL);

    PJ_TEST_EQ(pj_hash_get(ht, &key, sizeof(key), NULL), NULL, NULL,
               return -80);
    PJ_TEST_EQ(pj_hash_count(ht), 0, NULL, return -90);
    PJ_TEST_EQ(pj_hash_first(ht, &it_buf), NULL, NULL, return -100);
    return 0;
}


static int hash_collision_test(pj_pool_t *pool)
{
    enum {
        COUNT = HASH_COUNT * 4
    };
    pj_hash_table_t *ht;
    pj_hash_iterator_t it_buf, *it;
    unsigned char *values;
    unsigned i;

    PJ_TEST_NOT_NULL((ht=pj_hash_create(pool, HASH_COUNT)), NULL, return -200);

    values = (unsigned char*) pj_pool_alloc(pool, COUNT);

    for (i=0; i<COUNT; ++i) {
        values[i] = (unsigned char)i;
        pj_hash_set(pool, ht, &i, sizeof(i), 0, &values[i]);
    }

    PJ_TEST_EQ(pj_hash_count(ht), COUNT, NULL, return -210);

    for (i=0; i<COUNT; ++i) {
        unsigned char *entry;
        entry = (unsigned char*) pj_hash_get(ht, &i, sizeof(i), NULL);
        PJ_TEST_NOT_NULL(entry, NULL, return -220);
        PJ_TEST_EQ(*entry, values[i], NULL, return -230);
    }

    i = 0;
    it = pj_hash_first(ht, &it_buf);
    while (it) {
        ++i;
        it = pj_hash_next(ht, it);
    }

    PJ_TEST_EQ(i, COUNT, NULL, return -240);
    return 0;
}


/*
 * Hash table test.
 */
int hash_test(void)
{
    pj_pool_t *pool = pj_pool_create(mem, "hash", 512, 512, NULL);
    int rc;
    unsigned i;

    /* Test to fill in each row in the table */
    for (i=0; i<=HASH_COUNT; ++i) {
        rc = hash_test_with_key(pool, (unsigned char)i);
        if (rc != 0) {
            pj_pool_release(pool);
            return rc;
        }
    }

    /* Collision test */
    rc = hash_collision_test(pool);
    if (rc != 0) {
        pj_pool_release(pool);
        return rc;
    }

    pj_pool_release(pool);
    return 0;
}

#endif  /* INCLUDE_HASH_TEST */


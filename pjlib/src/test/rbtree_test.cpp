/* $Header: /pjproject/pjlib/src/test/rbtree_test.cpp 4     5/12/05 9:53p Bennylp $ */
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
#include "libpj_test.h"
#include <pj/rbtree.h>
#include <pj/types.h>
#include <pj/string.h>
#include <pj/hash.h>
#include <pj/pool.h>
#include <pj/os.h>
#include <stdlib.h>
#include <stdio.h>

#define LOOP	    256
#define MIN_COUNT   1024
#define MAX_COUNT   (LOOP * MIN_COUNT)
#define STRSIZE	    16

struct node_key
{
    pj_uint32_t hash;
    char str[STRSIZE];
};

static int compare_node(const node_key *k1, const node_key *k2)
{
    if (k1->hash == k2->hash) {
	return strcmp(k1->str, k2->str);
    } else {
	return k1->hash	< k2->hash ? -1 : 1;
    }
}

void randomize_string(char *str, int len)
{
    for (int i=0; i<len-1; ++i)
	str[i] = (char)('a' + rand() % 26);
    str[len-1] = '\0';
}

static int test()
{
    pj_rbtree rb;
    node_key *key;
    pj_rbtree_node *node;
    pj_pool_t *pool;
    int err=0;

    pj_rbtree_init(&rb, (pj_rbtree_comp*)&compare_node);
    pool = (*mem->create_pool)( mem, "pool", 
				MAX_COUNT*(sizeof(*key)+PJ_RBTREE_NODE_SIZE) + PJ_RBTREE_SIZE + PJ_POOL_SIZE, 
				0, NULL);

    key = (node_key *)pj_pool_alloc(pool, MAX_COUNT*sizeof(*key));
    node = (pj_rbtree_node*)pj_pool_alloc(pool, MAX_COUNT*sizeof(*node));

    puts("");
    int count = MIN_COUNT;
    for (int i=0; i<LOOP; ++i) {
	int j;
	pj_hr_timestamp t1, t2, t_setup, t_insert, t_search, t_erase;

	pj_assert(rb.size == 0);

	t_setup.u32.lo = t_insert.u32.lo = t_search.u32.lo = t_erase.u32.lo = 0;

	for (j=0; j<count; j++) {
	    randomize_string(key[j].str, STRSIZE);

	    pj_hr_gettimestamp(&t1);
	    node[j].key = &key[j];
	    node[j].user_data = key[j].str;
	    key[j].hash = pj_hash_calc(0, key[j].str, PJ_HASH_KEY_STRING);
	    pj_hr_gettimestamp(&t2);
	    t_setup.u32.lo += (t2.u32.lo - t1.u32.lo);

	    pj_hr_gettimestamp(&t1);
	    pj_rbtree_insert(&rb, &node[j]);
	    pj_hr_gettimestamp(&t2);
	    t_insert.u32.lo += (t2.u32.lo - t1.u32.lo);
	}

	pj_assert(rb.size == (unsigned)count);

	// Iterate key, make sure they're sorted.
	pj_rbtree_node *prev, *it;
	prev = NULL;
	it = pj_rbtree_first(&rb);
	while (it) {
	    if (prev) {
		if (compare_node((node_key*)prev->key, (node_key*)it->key) >= 0) {
		    ++err;
		    printf("Error: %s >= %s\n", (char*)prev->user_data, (char*)it->user_data);
		}
	    }
	    prev = it;
	    it = pj_rbtree_next(&rb, it);
	}

	// Search.
	for (j=0; j<count; j++) {
	    pj_hr_gettimestamp(&t1);
	    it = pj_rbtree_find(&rb, &key[j]);
	    pj_hr_gettimestamp(&t2);
	    t_search.u32.lo += (t2.u32.lo - t1.u32.lo);

	    pj_assert(it != NULL);
	    if (it == NULL)
		++err;
	}

	// Erase node.
	for (j=0; j<count; j++) {
	    pj_hr_gettimestamp(&t1);
	    it = pj_rbtree_erase(&rb, &node[j]);
	    pj_hr_gettimestamp(&t2);
	    t_erase.u32.lo += (t2.u32.lo - t1.u32.lo);
	}

	printf("...count:%d, setup:%d, insert:%d, search:%d, erase:%d\n",
		count,
		t_setup.u32.lo / count, t_insert.u32.lo / count,
		t_search.u32.lo / count, t_erase.u32.lo / count);

	count = 2 * count;
	if (count > MAX_COUNT)
	    break;
    }

    pj_pool_release(pool);
    return err;
}


int rbtree_test()
{
    return test();
}


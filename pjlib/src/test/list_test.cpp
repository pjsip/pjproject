/* $Header: /pjproject/pjlib/src/test/list_test.cpp 2     2/24/05 10:34a Bennylp $
 */
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
#include <pj/types.h>
#include <pj/list.h>

typedef struct list_node
{
    PJ_DECL_LIST_MEMBER(struct list_node)
    int value;
} list_node;

static int compare_node(void *value, const pj_list_type *nd)
{
    list_node *node = (list_node*)nd;
    return ((int)value == node->value) ? 0 : -1;
}

#define PJ_SIGNED_ARRAY_SIZE(a)	((int)PJ_ARRAY_SIZE(a))

int list_test()
{
    list_node nodes[4];    // must be even number of nodes
    list_node list;
    list_node list2;
    list_node *p;
    int i; // don't change to unsigned!

    //
    // Test insert_before().
    //
    list.value = (unsigned)-1;
    pj_list_init(&list);
    for (i=0; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i) {
	nodes[i].value = i;
	pj_list_insert_before(&list, &nodes[i]);
    }
    // check.
    for (i=0, p=list.next; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next) {
	pj_assert(p->value == i);
	if (p->value != i) {
	    return -1;
	}
    }

    //
    // Test insert_after()
    //
    pj_list_init(&list);
    for (i=PJ_SIGNED_ARRAY_SIZE(nodes)-1; i>=0; --i) {
	pj_list_insert_after(&list, &nodes[i]);
    }
    // check.
    for (i=0, p=list.next; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next) {
	pj_assert(p->value == i);
	if (p->value != i) {
	    return -1;
	}
    }

    //
    // Test merge_last()
    //
    // Init lists
    pj_list_init(&list);
    pj_list_init(&list2);
    for (i=0; i<PJ_SIGNED_ARRAY_SIZE(nodes)/2; ++i) {
	pj_list_insert_before(&list, &nodes[i]);
    }
    for (i=PJ_SIGNED_ARRAY_SIZE(nodes)/2; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i) {
	pj_list_insert_before(&list2, &nodes[i]);
    }
    // merge
    pj_list_merge_last(&list, &list2);
    // check.
    for (i=0, p=list.next; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next) {
	pj_assert(p->value == i);
	if (p->value != i) {
	    return -1;
	}
    }
    // check list is empty
    pj_assert( pj_list_empty(&list2) );
    if (!pj_list_empty(&list2)) {
	return -1;
    }

    // 
    // Check merge_first()
    //
    pj_list_init(&list);
    pj_list_init(&list2);
    for (i=0; i<PJ_SIGNED_ARRAY_SIZE(nodes)/2; ++i) {
	pj_list_insert_before(&list, &nodes[i]);
    }
    for (i=PJ_SIGNED_ARRAY_SIZE(nodes)/2; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i) {
	pj_list_insert_before(&list2, &nodes[i]);
    }
    // merge
    pj_list_merge_first(&list2, &list);
    // check (list2).
    for (i=0, p=list2.next; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next) {
	pj_assert(p->value == i);
	if (p->value != i) {
	    return -1;
	}
    }
    // check list is empty
    pj_assert( pj_list_empty(&list) );
    if (!pj_list_empty(&list)) {
	return -1;
    }

    //
    // Test insert_nodes_before()
    //
    // init list
    pj_list_init(&list);
    for (i=0; i<PJ_SIGNED_ARRAY_SIZE(nodes)/2; ++i) {
	pj_list_insert_before(&list, &nodes[i]);
    }
    // chain remaining nodes
    pj_list_init(&nodes[PJ_SIGNED_ARRAY_SIZE(nodes)/2]);
    for (i=PJ_SIGNED_ARRAY_SIZE(nodes)/2+1; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i) {
	pj_list_insert_before(&nodes[PJ_SIGNED_ARRAY_SIZE(nodes)/2], &nodes[i]);
    }
    // insert nodes
    pj_list_insert_nodes_before(&list, &nodes[PJ_SIGNED_ARRAY_SIZE(nodes)/2]);
    // check
    for (i=0, p=list.next; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next) {
	pj_assert(p->value == i);
	if (p->value != i) {
	    return -1;
	}
    }

    // erase test.
    pj_list_init(&list);
    for (i=0; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i) {
	nodes[i].value = i;
	pj_list_insert_before(&list, &nodes[i]);
    }
    for (i=PJ_SIGNED_ARRAY_SIZE(nodes)-1; i>=0; --i) {
	pj_list_erase(&nodes[i]);
	int j;
	for (j=0, p=list.next; j<i; ++j, p=p->next) {
	    pj_assert(p->value == j);
	    if (p->value != j) {
		return -1;
	    }
	}
    }

    // find and search
    pj_list_init(&list);
    for (i=0; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i) {
	nodes[i].value = i;
	pj_list_insert_before(&list, &nodes[i]);
    }
    for (i=0; i<PJ_SIGNED_ARRAY_SIZE(nodes); ++i) {
	p = (list_node*) pj_list_find_node(&list, &nodes[i]);
	pj_assert( p == &nodes[i] );
	if (p != &nodes[i]) {
	    return -1;
	}
	p = (list_node*) pj_list_search(&list, (void*)i, &compare_node);
	pj_assert( p == &nodes[i] );
	if (p != &nodes[i]) {
	    return -1;
	}
    }
    return 0;
}

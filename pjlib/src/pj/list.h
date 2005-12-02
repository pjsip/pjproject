/* $Header: /pjproject/pjlib/src/pj/list.h 6     8/24/05 10:27a Bennylp $ */
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

#ifndef __PJ_LIST_H__
#define __PJ_LIST_H__

/**
 * @file list.h
 * @brief Linked List data structure.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_DS Data Structure.
 * @ingroup PJ
 */
/**
 * @defgroup PJ_LIST Linked List
 * @ingroup PJ_DS
 * @{
 *
 * List in PJLIB is implemented as doubly-linked list, and it won't require
 * dynamic memory allocation (just as all PJLIB data structures). The list here
 * should be viewed more like a low level C list instead of high level C++ list
 * (which normally are easier to use but require dynamic memory allocations),
 * therefore all caveats with C list apply here too (such as you can NOT put
 * a node in more than one lists).
 *
 * It's best to describe the list using a simple example below.
 *
 * Sample:
 \verbatim
  #include <pj/list.h>
  #include <stdio.h>

  struct my_node
  {
    // This must be the first member declared in the struct!
    PJ_DECL_LIST_MEMBER(struct my_node)
    int value;
  };
 
  int main()
  {
     struct my_node nodes[10];
     struct my_node list;
     struct my_node iterator;
     int i;
 
     // Initialize the list as empty.
     pj_list_init(&list);
 
     // Insert nodes.
     for (i=0; i<10; ++i) {
        nodes[i].value = i;
        pj_list_insert_before(&list, &nodes[i]);
     }
 
     // Iterate list nodes.
     iterator = list.next;
     while (iterator != &list) {
        printf("value = %d\n", iterator->value);
        iterator = iterator->next;
     }

     // Erase all nodes.
     for (i=0; i<10; ++i) {
	pj_list_erase(&nodes[i]);
     }

     // List must be empty by now.
     pj_assert( pj_list_empty(&list) );

     return 0;
  };
 \endverbatim
 *
 */


/**
 * Use this macro in the start of the structure declaration to declare that
 * the structure can be used in the linked list operation.
 */
#define PJ_DECL_LIST_MEMBER(type)  type *prev, *next;


/**
 * This structure describes generic list node and list. The owner of this list
 * must initialize the 'value' member to an appropriate value (typically the
 * owner itself).
 */
struct pj_list
{
    PJ_DECL_LIST_MEMBER(void)
};


/**
 * Initialize the list.
 * Initially, the list will have no member, and function pj_list_empty() will
 * always return nonzero (which indicates TRUE) for the newly initialized 
 * list.
 *
 * @param node The list head.
 */
PJ_IDECL(void) pj_list_init(pj_list_type * node);


/**
 * Check that the list is empty.
 *
 * @param node	The list head.
 *
 * @return Non-zero if the list is not-empty, or zero if it is empty.
 *
 */
PJ_IDECL(int) pj_list_empty(const pj_list_type * node);


/**
 * Insert the node to the list before the specified element position.
 *
 * @param pos	The element to which the node will be inserted before. 
 * @param node	The element to be inserted.
 *
 * @return void.
 */
PJ_IDECL(void)	pj_list_insert_before(pj_list_type *pos, pj_list_type *node);


/**
 * Remove elements from the source list, and insert them to the destination
 * list. The elements of the source list will occupy the
 * front elements of the target list. Note that the node pointed by \a list2
 * itself is not considered as a node, but rather as the list descriptor, so
 * it will not be inserted to the \a list1. The elements to be inserted starts
 * at \a list2->next. If \a list2 is to be included in the operation, use
 * \a pj_list_insert_nodes_before.
 *
 * @param list1	The destination list.
 * @param list2	The source list.
 *
 * @return void.
 */
PJ_IDECL(void) pj_list_merge_first(pj_list_type *list1, pj_list_type *list2);


/**
 * Inserts all nodes in \a nodes to the target list.
 *
 * @param lst	    The target list.
 * @param nodes	    Nodes list.
 */
PJ_IDECL(void) pj_list_insert_nodes_before(pj_list_type *lst,
					   pj_list_type *nodes);

/**
 * Insert all nodes in \a nodes to the target list.
 *
 * @param lst	    The target list.
 * @param nodes	    Nodes list.
 */
PJ_IDECL(void) pj_list_insert_nodes_after(pj_list_type *lst,
					  pj_list_type *nodes);

/**
 * Insert a node to the list after the specified element position.
 *
 * @param pos	    The element in the list which will precede the inserted 
 *		    element.
 * @param node	    The element to be inserted after the position element.
 *
 * @return void.
 */
PJ_IDECL(void) pj_list_insert_after(pj_list_type *pos, pj_list_type *node);


/**
 * Remove elements from the second list argument, and insert them to the list 
 * in the first argument. The elements from the second list will be appended
 * to the first list. Note that the node pointed by \a list2
 * itself is not considered as a node, but rather as the list descriptor, so
 * it will not be inserted to the \a list1. The elements to be inserted starts
 * at \a list2->next. If \a list2 is to be included in the operation, use
 * \a pj_list_insert_nodes_before.
 *
 * @param list1	    The element in the list which will precede the inserted 
 *		    element.
 * @param list2	    The element in the list to be inserted.
 *
 * @return void.
 */
PJ_IDECL(void) pj_list_merge_last( pj_list_type *list1, pj_list_type *list2);


/**
 * Erase the node from the list it currently belongs.
 *
 * @param node	    The element to be erased.
 */
PJ_IDECL(void) pj_list_erase(pj_list_type *node);


/**
 * Find node in the list.
 *
 * @param list	    The list head.
 * @param node	    The node element to be searched.
 *
 * @return The node itself if it is found in the list, or NULL if it is not 
 *         found in the list.
 */
PJ_IDECL(pj_list_type*) pj_list_find_node(pj_list_type *list, 
					  pj_list_type *node);


/**
 * Search the list for the specified value, using the specified comparison
 * function. This function iterates on nodes in the list, started with the
 * first node, and call the user supplied comparison function until the
 * comparison function returns ZERO.
 *
 * @param list	    The list head.
 * @param value	    The user defined value to be passed in the comparison 
 *		    function
 * @param comp	    The comparison function, which should return ZERO to 
 *		    indicate that the searched value is found.
 *
 * @return The first node that matched, or NULL if it is not found.
 */
PJ_IDECL(pj_list_type*) pj_list_search(pj_list_type *list, void *value,
				       int (*comp)(void *value, 
						   const pj_list_type *node)
				       );


/**
 * @}
 */

#if PJ_FUNCTIONS_ARE_INLINED
#  include "list_i.h"
#endif

PJ_END_DECL

#endif	/* __PJ_LIST_H__ */


/* $Id$
 */
#include <pj/list.h>
#include <pj/assert.h>
#include <pj/log.h>

/**
 * \page page_pjlib_samples_list_c Example: List Manipulation
 *
 * Below is sample program to demonstrate how to manipulate linked list.
 *
 * \includelineno pjlib-samples/list.c
 */

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
    struct my_node *it;
    int i;
    
    // Initialize the list as empty.
    pj_list_init(&list);
    
    // Insert nodes.
    for (i=0; i<10; ++i) {
        nodes[i].value = i;
        pj_list_insert_before(&list, &nodes[i]);
    }
    
    // Iterate list nodes.
    it = list.next;
    while (it != &list) {
        PJ_LOG(3,("list", "value = %d", it->value));
        it = it->next;
    }
    
    // Erase all nodes.
    for (i=0; i<10; ++i) {
        pj_list_erase(&nodes[i]);
    }
    
    // List must be empty by now.
    pj_assert( pj_list_empty(&list) );
    
    return 0;
};

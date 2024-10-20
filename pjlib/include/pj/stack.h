#ifndef __PJ_STACK_H__
#define __PJ_STACK_H__

/**
 * @file stack.h
 * @brief Single Linked List data structure.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/*
 * @defgroup PJ_DS Data Structure.
 */

/**
 * @defgroup PJ_STACK Single Linked List with FILO in/out policy
 * @ingroup PJ_DS
 * @{
 *
 * Stack in PJLIB is single-linked list with First In Last Out logic. 
 * Stack is thread safe. Common PJLIB stack implementation uses internal locking mechanism so is thread-safe.
 * Implementation for Windows platform uses locking free Windows embeded single linked list implementation.
 * Windows single linked list implementation requires aligned data, both stack item and stack itself should 
 * be aligned by 8 (for x86) or 16 (for x64) byte. We recomend set PJ_POOL_ALIGNMENT macro to corresponding value.
 * winnt.h define MEMORY_ALLOCATION_ALIGNMENT macro for this purpose.
 * Stack won't require dynamic memory allocation (just as all PJLIB data structures). The stack here
 * should be viewed more like a low level C stack instead of high level C++ stack
 * (which normally are easier to use but require dynamic memory allocations),
 * therefore all caveats with C stack apply here too (such as you can NOT put
 * a node in more than one stacks).
 *
 * \section pj_stack_example_sec Examples
 *
 * See below for examples on how to manipulate stack:
 *  - @ref page_pjlib_stack_test
 */


#define IS_ALIGNED_PTR(PTR,ALIGNMENT) (((pj_ssize_t)(void*)(PTR) % (ALIGNMENT)) == 0)


/**
 * Use this macro in the start of the structure declaration to declare that
 * the structure can be used in the stack operation. This macro simply
 * declares additional member @a next to the structure.
 * @hideinitializer
 */
#define PJ_DECL_STACK_MEMBER(type)                       \
                                   /** Stack @a next. */ \
                                   type *next 


/**
 * Create the stack: allocate memory, allocate and initialize OS resources.
 * Initially, the stack will have no member, and function pj_stack_pop() will
 * always return NULL (which indicates there are no any items in the stack currently) for the newly initialized 
 * stack.
 *
 * @param pool Pool to allocate memory from.
 * @param stack The stack head.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stack_create(pj_pool_t *pool, pj_stack_type **stack);

/**
 * Free OS resources allocated by pj_stack_create().
 *
 * @param stack	    The target stack.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stack_destroy(pj_stack_type *stack);


/**
 * Insert (push) the node to the front of the stack as atomic (thread safe) operation.
 *
 * @param stack	The stack. 
 * @param node	The element to be inserted.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stack_push(pj_stack_type *stack, pj_stack_t *node);


/**
 * Extract (pop) element from the front of the stack (removing it from the stack) as atomic (thread safe) operation.
 *
 * @param stack	    The target stack.
 *
 * @return NULL if the stack is empty, or else pointer to element extracted from stack.
 */
PJ_DECL(pj_stack_t*) pj_stack_pop(pj_stack_type *stack);

/**
 * Traverse the stack and get it's elements quantity. 
 * The return value of pj_stack_size should not be relied upon in multithreaded applications 
 * because the item count can be changed at any time by another thread.
 * For Windows platform returns the number of entries in the stack modulo 65535. For example, 
 * if the specified stack contains 65536 entries, pj_stack_size returns zero.
 *
 * @param stack	    The target stack.
 *
 * @return	    Number of elements.
 */
PJ_DEF(pj_size_t) pj_stack_size(/*const*/ pj_stack_type *stack);


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJ_STACK_H__ */


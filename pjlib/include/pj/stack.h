#ifndef __PJ_STACK_H__
#define __PJ_STACK_H__

/**
 * @file stack.h
 * @brief Single Linked List data structure.
 */

#include <pj/types.h>

#if defined(PJ_STACK_IMPLEMENTATION) && PJ_STACK_IMPLEMENTATION==PJ_STACK_WIN32

/* the Windows platform stack implementation. */

/* 
 * PJ_STACK_ALIGN_PREFIX, PJ_STACK_ALIGN_SUFFIX is a readable syntax to declare
 * platform default alignment for the stack item (see example below).
 */

#   define PJ_STACK_ALIGN_PREFIX PJ_ALIGN_DATA_PREFIX(PJ_POOL_ALIGNMENT)
#   define PJ_STACK_ALIGN_SUFFIX PJ_ALIGN_DATA_SUFFIX(PJ_POOL_ALIGNMENT)

#else  // PJ_STACK_IMPLEMENTATION != PJ_STACK_WIN32

/* compilation of crossplatform stack implementation */

#   define PJ_STACK_ALIGN_PREFIX
#   define PJ_STACK_ALIGN_SUFFIX

#endif // PJ_STACK_IMPLEMENTATION

#include <pj/stack.h>


PJ_BEGIN_DECL

/*
 * @defgroup PJ_DS Data Structure.
 */

/**
 * @defgroup PJ_STACK Single Linked List with LIFO in/out policy
 * @ingroup PJ_DS
 * @{
 *
 * Stack in PJLIB is single-linked list with First In Last Out logic. 
 * Stack is thread safe. Common PJLIB stack implementation uses internal locking mechanism so is thread-safe.
 * Implementation for Windows platform uses locking free Windows embeded single linked list implementation.
 * The performance of pj_stack implementation for Windows platform is 2-5x higher than cross-platform.
 * 
 * By default pjlib compile and link os independent "cross-platform" implementation. 
 * To select implementation you may optionaly define PJ_STACK_IMPLEMENTATION as PJ_STACK_WIN32 
 * or PJ_STACK_OS_INDEPENDENT. The last option is default.
 * 
 * To use the implementation on the Windows platform, some prerequisites must be met:
 * - this should be compiling for Windows platform
 * - add #define PJ_STACK_IMPLEMENTATION           PJ_STACK_WIN32 
 *      to your config_site.h
 * 
 * Windows single linked list implementation requires aligned data, both stack item and stack itself should 
 * be aligned by 8 (for x86) or 16 (for x64) byte. 
 * pjsip build system define PJ_POOL_ALIGNMENT macro to corresponding value.
 * winnt.h define MEMORY_ALLOCATION_ALIGNMENT macro for this purpose.
 * To use this macro in build system we recomend (this is optional) to add #include <windows.h> 
 * to your config_site.h.
 * You may redefine PJ_POOL_ALIGNMENT in your config_site.h but to use PJ_STACK_WIN32 implementation 
 * PJ_POOL_ALIGNMENTshould not be less then MEMORY_ALLOCATION_ALIGNMENT
 * 
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
 * Use PJ_DECL_STACK_MEMBER macro in the start of the structure declaration to
 * declare that the structure can be used in the stack operation. This macro
 * simply declares additional member @a next to the structure.
 * 
 * The full declaration of stack item should contain alignment macro 
 * and may look like this:
 * 
 * typedef struct PJ_STACK_ALIGN_PREFIX stack_node {
 *   PJ_DECL_STACK_MEMBER(struct stack_node);
 *   ...
 *   your data here
 *   ...
 * } PJ_STACK_ALIGN_SUFFIX stack_node;
* 
 * @hideinitializer
 */
#define PJ_DECL_STACK_MEMBER(type)                       \
                                   /** Stack @a next. */ \
                                   type *next 


/**
 * Create the stack: allocate memory, allocate and initialize OS resources.
 * Initially, the stack will have no member, and function pj_stack_pop() will
 * always return NULL for the newly initialized stack
 * (which indicates there are no any items in the stack currently).
 *
 * @param pool Pool to allocate memory from.
 * @param stack The stack head.
 *
 * @return    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stack_create(pj_pool_t *pool, pj_stack_type **stack);

/**
 * Free OS resources allocated by pj_stack_create().
 *
 * @param stack     The target stack.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stack_destroy(pj_stack_type *stack);


/**
 * Insert (push) the node to the front of the stack as atomic (thread safe) operation.
 *
 * @param stack The stack. 
 * @param node  The element to be inserted.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stack_push(pj_stack_type *stack, pj_stack_t *node);


/**
 * Extract (pop) element from the front of the stack (removing it from the stack) as atomic (thread safe) operation.
 *
 * @param stack     The target stack.
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
 * @param stack     The target stack.
 *
 * @return          Number of elements.
 */
PJ_DEF(pj_size_t) pj_stack_size(/*const*/ pj_stack_type *stack);


/**
 * @}
 */

PJ_END_DECL

#endif  /* __PJ_STACK_H__ */


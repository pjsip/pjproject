/* 
 * Copyright (C) 2008-2025 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_ATOMIC_SLIST_H__
#define __PJ_ATOMIC_SLIST_H__

/* 2022-2025 Leonid Goltsblat <lgoltsblat@gmail.com> */

/**
 * @file atomic_slist.h
 * @brief Single Linked List data structure.
 */

#include <pj/types.h>

#if defined(PJ_ATOMIC_SLIST_IMPLEMENTATION) && PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32

/* the Windows platform atomic_slist implementation. */

/** 
 * PJ_ATOMIC_SLIST_ALIGN_PREFIX, PJ_ATOMIC_SLIST_ALIGN_SUFFIX is a readable syntax to declare
 * platform default alignment for the atomic_slist item (see example below).
 */

#   if defined(MEMORY_ALLOCATION_ALIGNMENT)
#       define PJ_ATOMIC_SLIST_ALIGNMENT MEMORY_ALLOCATION_ALIGNMENT
#   elif defined(_WIN64) || defined(_M_ALPHA)
#       define PJ_ATOMIC_SLIST_ALIGNMENT 16
#   else
#       define PJ_ATOMIC_SLIST_ALIGNMENT 8
#   endif

#   define PJ_ATOMIC_SLIST_ALIGN_PREFIX     PJ_ALIGN_DATA_PREFIX(PJ_ATOMIC_SLIST_ALIGNMENT)
#   define PJ_ATOMIC_SLIST_ALIGN_SUFFIX     PJ_ALIGN_DATA_SUFFIX(PJ_ATOMIC_SLIST_ALIGNMENT)

#else  // PJ_ATOMIC_SLIST_IMPLEMENTATION != PJ_ATOMIC_SLIST_WIN32

/* compilation of crossplatform atomic_slist implementation */

#   define PJ_ATOMIC_SLIST_ALIGNMENT        0

#   define PJ_ATOMIC_SLIST_ALIGN_PREFIX
#   define PJ_ATOMIC_SLIST_ALIGN_SUFFIX

#endif // PJ_ATOMIC_SLIST_IMPLEMENTATION


PJ_BEGIN_DECL

/*
 * @defgroup PJ_DS Data Structure.
 */

/**
 * @defgroup PJ_ATOMIC_SLIST Single Linked List with LIFO in/out policy
 * @ingroup PJ_DS
 * @{
 *
 * Atomic slist in PJLIB is single-linked list with First In Last Out logic. 
 * Atomic slist is thread safe. Common PJLIB slist implementation uses internal
 * locking mechanism so is thread-safe.
 * Implementation for Windows platform uses locking free Windows embeded single
 * linked list implementation. The performance of pj_atomic_slist implementation 
 * for Windows platform is considerably higher than cross-platform.
 * 
 * By default pjlib compile and link os independent "cross-platform" (generic) 
 * implementation. To select implementation you may optionaly define 
 * PJ_ATOMIC_SLIST_IMPLEMENTATION as PJ_ATOMIC_SLIST_WIN32 or
 * PJ_ATOMIC_SLIST_GENERIC.
 * The last option is default for all platforms except Windows
 * where the default is PJ_ATOMIC_SLIST_WIN32.
 * 
 * Windows single linked list implementation (PJ_ATOMIC_SLIST_WIN32)
 * requires aligned data, both slist item and slist itself 
 * should be aligned by 8 (for x86) or 16 (for x64) byte.
 * winnt.h define MEMORY_ALLOCATION_ALIGNMENT macro for this purpose.
 * For the same purpose pjsip defines PJ_ATOMIC_SLIST_ALIGNMENT macro
 * calculating value based on curent platform.
 * To use MEMORY_ALLOCATION_ALIGNMENT macro in the build system as the value
 * of PJ_ATOMIC_SLIST_ALIGNMENT macro we recomend (this is optional) to add
 * #include <windows.h> to your config_site.h.
 * For other implementation PJ_ATOMIC_SLIST_ALIGNMENT macro is defined as 0, 
 * which causes pj_pool_aligned_alloc() to use the default pool alignment.
 * 
 * To allocate slist element (node) or array of slist nodes application
 * should use call pj_pool_aligned_alloc() with PJ_ATOMIC_SLIST_ALIGNMENT
 * as the value of an alignment parameter.
 * To hide these implementation details from the application,
 * the API provides a pj_atomic_slist_calloc() function that internally 
 * handles the implementation-specific alignment.
 * However application developer should declare slist nodes types properly
 * aligned. The macros PJ_ATOMIC_SLIST_ALIGN_PREFIX and 
 * PJ_ATOMIC_SLIST_ALIGN_SUFFIX are provided for this purpose (see below).
 * 
 * Atomic slist won't require dynamic memory allocation (just as all PJLIB 
 * data structures). The slist here should be viewed more like a low level C 
 * slist instead of high level C++ slist (which normally are easier to use but
 * requires dynamic memory allocations), therefore all caveats with C slist 
 * apply here too (such as you can NOT put a node in more than one slists).
 *
 * \section pj_atomic_slist_example_sec Examples
 *
 * See below for examples on how to manipulate slist:
 *  - @ref page_pjlib_atomic_slist_test
 */


/**
 * Use PJ_DECL_ATOMIC_SLIST_MEMBER macro in the start of the structure 
 * declaration to declare that the structure can be used in the slist
 *  operation. This macro simply declares additional member @a next to 
 * the structure.
 * 
 * The full declaration of slist item should contain alignment macro 
 * and may look like this:
 * 
 * typedef struct PJ_ATOMIC_SLIST_ALIGN_PREFIX slist_node {
 *   PJ_DECL_ATOMIC_SLIST_MEMBER(struct slist_node);
 *   ...
 *   your data here
 *   ...
 * } PJ_ATOMIC_SLIST_ALIGN_SUFFIX slist_node;
* 
 * @hideinitializer
 */
#if defined(PJ_ATOMIC_SLIST_IMPLEMENTATION) && PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32
#   define PJ_DECL_ATOMIC_SLIST_MEMBER(type)                       \
                                       /** Slist @a next. */ \
                                       SLIST_ENTRY next 
#else  // PJ_ATOMIC_SLIST_IMPLEMENTATION != PJ_ATOMIC_SLIST_WIN32
#   define PJ_DECL_ATOMIC_SLIST_MEMBER(type)                       \
                                       /** Slist @a next. */ \
                                       type *next 
#endif // PJ_ATOMIC_SLIST_IMPLEMENTATION


/**
 * Create the slist: allocate memory, allocate and initialize OS resources.
 * Initially, the slist will have no member, 
 * and function pj_atomic_slist_pop() will always return NULL
 * for the newly initialized slist (which indicates there are no any items 
 * in the slist currently).
 *
 * @param pool      Pool to allocate memory from.
 * @param slist     The slist head.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_atomic_slist_create(pj_pool_t *pool, 
                                            pj_atomic_slist **slist);

/**
 * Free OS resources allocated by pj_atomic_slist_create().
 *
 * @param slist     The target slist.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_atomic_slist_destroy(pj_atomic_slist *slist);


/**
 * Insert (push) the node to the front of the slist
 * as atomic (thread safe) operation.
 *
 * @param slist     The slist. 
 * @param node      The element to be inserted.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_atomic_slist_push(pj_atomic_slist *slist, 
                                          pj_atomic_slist_node_t *node);


/**
 * Extract (pop) element from the front of the slist 
 * (removing it from the slist) as atomic (thread safe) operation.
 *
 * @param slist     The target slist.
 *
 * @return          NULL if the slist is empty, 
 *                  or else pointer to element extracted from slist.
 */
PJ_DECL(pj_atomic_slist_node_t*) pj_atomic_slist_pop(pj_atomic_slist *slist);

/**
 * Traverse the slist and get it's elements quantity. 
 * The return value of pj_atomic_slist_size should not be relied upon
 * in multithreaded applications because the item count can be changed
 * at any time by another thread.
 * For Windows platform returns the number of entries in the slist 
 * modulo 65535. For example, if the specified slist contains 65536 entries,
 * pj_atomic_slist_size returns zero.
 *
 * @param slist     The target slist.
 *
 * @return          Number of elements.
 */
PJ_DECL(pj_size_t) pj_atomic_slist_size(/*const*/ pj_atomic_slist *slist);

/**
 * Allocate storage for slist nodes array or single node from the pool 
 * and initialize it to zero.
 * This is simple wrapper arround pj_pool_aligned_alloc() or pj_pool_calloc()
 * that internally handles the slist implementation-specific alignment.
 *
 * @param pool      the pool.
 * @param count     the number of slist elements in the array.
 * @param elem      the size of individual slist element.
 *
 * @return          Pointer to the allocated memory.
 */
PJ_DECL(void*) pj_atomic_slist_calloc(pj_pool_t *pool, pj_size_t count, pj_size_t elem);

/**
 * @}
 */

PJ_END_DECL

#endif  /* __PJ_ATOMIC_SLIST_H__ */


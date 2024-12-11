/* 
* Copyright (C) 2008-2024 Teluu Inc. (http://www.teluu.com)
* Copyright (C) 2022-2024 Leonid Goltsblat <lgoltsblat@gmail.com>
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
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/errno.h>

/*
 * stack_win32.c
 *
 * This file contains the stack implementation for Windows platform.
 *
 * If all prerequisites are met, this file will be included in stack.c,
 * which is the cross-platform stack implementation.
 * 
 * This file should generally NOT be compiled as standalone source.
 */

#ifndef PJ_WIN32
#error stack_win32.c should be compiled only for Windows platform
#endif  // PJ_WIN32

#include <windows.h>

#include <pj/stack.h>

#ifndef THIS_FILE
#   define THIS_FILE    "stack_win32.c"
#endif // !THIS_FILE


#if PJ_POOL_ALIGNMENT < MEMORY_ALLOCATION_ALIGNMENT
#error pj_stack implementation for Windows platform required PJ_POOL_ALIGNMENT \
macro set to value not less then MEMORY_ALLOCATION_ALIGNMENT. \
(The MEMORY_ALLOCATION_ALIGNMENT macro which is 16 on the x64 platform and 8 on the x86 platform \
is the platform default alignment for the Windows platform and is set in winnt.h.) \
Please add the following line to your config_site.h file: #define PJ_POOL_ALIGNMENT  MEMORY_ALLOCATION_ALIGNMENT \
for this you may also need to #include <windows.h> into config_site.h
#endif

//#pragma message("compilation of Windows platform stack implementation")

//uncomment next line to check alignment in runtime
//#define PJ_STACK_ALIGN_DEBUG


struct pj_stack_type
{
    SLIST_HEADER head;
};


PJ_DEF(pj_status_t) pj_stack_create(pj_pool_t *pool, pj_stack_type **stack)
{

    PJ_ASSERT_RETURN(pool && stack, PJ_EINVAL);

    pj_stack_type* p_stack = PJ_POOL_ALLOC_T(pool, pj_stack_type);
    if (!p_stack)
        return PJ_ENOMEM;
    
#ifdef PJ_STACK_ALIGN_DEBUG
    PJ_ASSERT_ON_FAIL(IS_ALIGNED_PTR(&p_stack->head, MEMORY_ALLOCATION_ALIGNMENT), {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_stack implementation requires alignment not less than %d (stc%p).", MEMORY_ALLOCATION_ALIGNMENT, p_stack));
            return PJ_EINVALIDOP;
        });
#endif //PJ_STACK_ALIGN_DEBUG

    InitializeSListHead(&p_stack->head);

    *stack = p_stack;
    PJ_LOG(6, (THIS_FILE, "Stack created stc%p", p_stack));
    return PJ_SUCCESS;

}


PJ_DEF(pj_status_t) pj_stack_destroy(pj_stack_type *stack)
{
    PJ_ASSERT_RETURN(stack, PJ_EINVAL);
    //nothing to do here
    PJ_LOG(6, (THIS_FILE, "Stack destroyed stc%p", stack));
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stack_push(pj_stack_type *stack, pj_stack_t *node)
{
#ifdef PJ_STACK_ALIGN_DEBUG
    PJ_ASSERT_RETURN(node && stack, PJ_EINVAL);

    PJ_ASSERT_ON_FAIL(  IS_ALIGNED_PTR(&stack->head, MEMORY_ALLOCATION_ALIGNMENT) && 
                        IS_ALIGNED_PTR(node, MEMORY_ALLOCATION_ALIGNMENT), 
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_stack implementation requires alignment not less than %d (stc%p).", MEMORY_ALLOCATION_ALIGNMENT, stack));
            return PJ_EINVALIDOP;
        });
#endif //PJ_STACK_ALIGN_DEBUG

    /*pFirstEntry =*/ InterlockedPushEntrySList(&stack->head, node);
    return PJ_SUCCESS;
}


PJ_DEF(pj_stack_t*) pj_stack_pop(pj_stack_type *stack)
{
#ifdef PJ_STACK_ALIGN_DEBUG
    PJ_ASSERT_RETURN(stack, NULL);
    PJ_ASSERT_ON_FAIL(IS_ALIGNED_PTR(&stack->head, MEMORY_ALLOCATION_ALIGNMENT),
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_stack implementation requires alignment not less than %d (stc%p).", MEMORY_ALLOCATION_ALIGNMENT, stack));
            return NULL;
        });
#endif //PJ_STACK_ALIGN_DEBUG

    return InterlockedPopEntrySList(&stack->head);
}

/**
 * Get the stack's elements quantity.
 * The return value of pj_stack_size should not be relied upon in multithreaded applications
 * because the item count can be changed at any time by another thread.
 * For Windows platform returns the number of entries in the stack modulo 65535. For example,
 * if the specified stack contains 65536 entries, pj_stack_size returns zero.
 *
 * @param stack	    The target stack.
 *
 * @return	    Number of elements.
 */
PJ_DEF(pj_size_t) pj_stack_size(/*const*/ pj_stack_type *stack)
{
#ifdef PJ_STACK_ALIGN_DEBUG
    PJ_ASSERT_RETURN(stack, 0);
    PJ_ASSERT_ON_FAIL(IS_ALIGNED_PTR(&stack->head, MEMORY_ALLOCATION_ALIGNMENT),
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_stack implementation requires alignment not less than %d (stc%p).", MEMORY_ALLOCATION_ALIGNMENT, stack));
            return 0;
        });
#endif //PJ_STACK_ALIGN_DEBUG

    return QueryDepthSList(&stack->head);
}

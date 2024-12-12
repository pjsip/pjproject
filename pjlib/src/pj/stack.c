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

#include <pj/stack.h>

#if defined(PJ_STACK_IMPLEMENTATION) && PJ_STACK_IMPLEMENTATION==PJ_STACK_WIN32

    /* Include the Windows platform stack implementation. */
#   include "stack_win32.c"

#else  // PJ_STACK_IMPLEMENTATION != PJ_STACK_WIN32

//#pragma message("compilation of cross-platform stack implementation")

#define THIS_FILE       "stack.c"

/**
 * This structure describes generic stack node and stack. The owner of this stack
 * must initialize the 'value' member to an appropriate value (typically the
 * owner itself).
 */
typedef struct pj_stack_item_t
{
    PJ_DECL_STACK_MEMBER(void);
} PJ_ATTR_MAY_ALIAS pj_stack_item_t; /* may_alias avoids warning with gcc-4.4 -Wall -O2 */


struct pj_stack_type
{
    pj_stack_item_t  head;
    pj_mutex_t      *mutex;
};

PJ_DEF(pj_status_t) pj_stack_create(pj_pool_t *pool, pj_stack_type **stack)
{
    pj_stack_type   *p_stack;
    pj_status_t      rc;

    PJ_ASSERT_RETURN(pool && stack, PJ_EINVAL);

    p_stack = PJ_POOL_ZALLOC_T(pool, pj_stack_type);
    if (!p_stack)
        return PJ_ENOMEM;

    char                name[PJ_MAX_OBJ_NAME];
    /* Set name. */
    pj_ansi_snprintf(name, PJ_MAX_OBJ_NAME, "stc%p", p_stack);

    rc = pj_mutex_create_simple(pool, name, &p_stack->mutex);
    if (rc != PJ_SUCCESS)
        return rc;


    p_stack->head.next = &p_stack->head;
    *stack = p_stack;

    PJ_LOG(6, (THIS_FILE, "Stack created stc%p", p_stack));
    return PJ_SUCCESS;

}


PJ_DEF(pj_status_t) pj_stack_destroy(pj_stack_type *stack)
{
    PJ_ASSERT_RETURN(stack, PJ_EINVAL);
    pj_status_t rc;
    rc = pj_mutex_destroy(stack->mutex);
    if (rc == PJ_SUCCESS)
    {
        PJ_LOG(6, (THIS_FILE, "Stack destroyed stc%p", stack));
    }
    return rc;
}


PJ_DEF(pj_status_t) pj_stack_push(pj_stack_type *stack, pj_stack_t *node)
{
    PJ_ASSERT_RETURN(node && stack, PJ_EINVAL);
    pj_status_t status;
    if ((status = pj_mutex_lock(stack->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_stack_push", status, "Error locking mutex for stack stc%p", stack));
        return status;
    }
    ((pj_stack_item_t*)node)->next = stack->head.next;
    stack->head.next = node;
    if ((status = pj_mutex_unlock(stack->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_stack_push", status, "Error unlocking mutex for stack stc%p", stack));
    }
    return status;
}


PJ_DEF(pj_stack_t*) pj_stack_pop(pj_stack_type *stack)
{
    PJ_ASSERT_RETURN(stack, NULL);
    pj_status_t status;
    if ((status = pj_mutex_lock(stack->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_stack_pop", status, "Error locking mutex for stack stc%p", stack));
        return NULL;
    }

    pj_stack_item_t *node = stack->head.next;
    if (node != &stack->head) {
        stack->head.next = node->next;
        node->next = NULL;
    }
    else
        node = NULL;

    if ((status = pj_mutex_unlock(stack->mutex)) != PJ_SUCCESS)
        PJ_PERROR(1, ("pj_stack_pop", status, "Error unlocking mutex for stack stc%p", stack));

    return node;
}


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
PJ_DEF(pj_size_t) pj_stack_size(/*const*/ pj_stack_type *stack)
{
    PJ_ASSERT_RETURN(stack, 0);
    pj_status_t status;
    if ((status = pj_mutex_lock(stack->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_stack_size", status, "Error locking mutex for stack stc%p", stack));
        return 0;
    }

    const pj_stack_item_t *node = stack->head.next;
    pj_size_t count = 0;

    while (node != node->next) {
        ++count;
        node = node->next;
    }

    if ((status = pj_mutex_unlock(stack->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_stack_size", status, "Error unlocking mutex for stack stc%p", stack));
    }

    return count;
}
#endif // PJ_STACK_IMPLEMENTATION

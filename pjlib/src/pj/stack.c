#include <pj/os.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/errno.h>

#ifdef PJ_WIN32
#error stack.c should NOT be compiled for Windows platform (compile stack_win32.c instead)
#endif  // PJ_WIN32

#include <pj/stack.h>

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

    pj_stack_item_t* node = stack->head.next;
    stack->head.next = node->next;

    if ((status = pj_mutex_unlock(stack->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_stack_pop", status, "Error unlocking mutex for stack stc%p", stack));
    }
    if (node != node->next)
    {
        node->next = NULL;
        return node;
    }
    else
        return NULL;
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

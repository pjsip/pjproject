#include <pj/os.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/errno.h>

#ifndef PJ_WIN32
#error stack_win32.c should be compiled înly for Windows platform
#endif  // PJ_WIN32

#include <windows.h>

#include <pj/stack.h>

#define THIS_FILE	"stack_win32.c"

#if PJ_POOL_ALIGNMENT < MEMORY_ALLOCATION_ALIGNMENT
#error pj_stack implementation for Windows platform required PJ_POOL_ALIGNMENT macro set to value not less then MEMORY_ALLOCATION_ALIGNMENT \
    (platform dependent constant declared in winnt.h) \
    please add next line to your config_site.h file: #define PJ_POOL_ALIGNMENT    MEMORY_ALLOCATION_ALIGNMENT\
    for this you may also need to #include <windows.h> into config_site.h
#endif

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
    
    PJ_ASSERT_ON_FAIL(IS_ALIGNED_PTR(&p_stack->head, MEMORY_ALLOCATION_ALIGNMENT), {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_stack implementation requires alignment not less than %d (stc%p).", MEMORY_ALLOCATION_ALIGNMENT, p_stack));
            return PJ_EINVALIDOP;
        });

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
    PJ_ASSERT_RETURN(node && stack, PJ_EINVAL);

    PJ_ASSERT_ON_FAIL(  IS_ALIGNED_PTR(&stack->head, MEMORY_ALLOCATION_ALIGNMENT) && 
                        IS_ALIGNED_PTR(node, MEMORY_ALLOCATION_ALIGNMENT), 
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_stack implementation requires alignment not less than %d (stc%p).", MEMORY_ALLOCATION_ALIGNMENT, stack));
            return PJ_EINVALIDOP;
        });

    /*pFirstEntry =*/ InterlockedPushEntrySList(&stack->head, node);
    return PJ_SUCCESS;
}


PJ_DEF(pj_stack_t*) pj_stack_pop(pj_stack_type *stack)
{
    PJ_ASSERT_RETURN(stack, NULL);
    PJ_ASSERT_ON_FAIL(IS_ALIGNED_PTR(&stack->head, MEMORY_ALLOCATION_ALIGNMENT),
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_stack implementation requires alignment not less than %d (stc%p).", MEMORY_ALLOCATION_ALIGNMENT, stack));
            return NULL;
        });

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
    PJ_ASSERT_RETURN(stack, 0);
    PJ_ASSERT_ON_FAIL(IS_ALIGNED_PTR(&stack->head, MEMORY_ALLOCATION_ALIGNMENT),
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_stack implementation requires alignment not less than %d (stc%p).", MEMORY_ALLOCATION_ALIGNMENT, stack));
            return 0;
        });

    return QueryDepthSList(&stack->head);
}

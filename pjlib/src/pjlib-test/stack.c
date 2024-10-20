#include "test.h"

 /**
  * \page page_pjlib_stack_test Test: Stack
  *
  * This file provides implementation of \b stack_test(). It tests the
  * functionality of the stack API.
  *
  * \section stack_test_sec Scope of the Test
  *
  * API tested:
  *  - pj_stack_create()
  *  - pj_stack_push()
  *  - pj_stack_pop()
  *  - pj_stack_destroy()
  *
  *
  * This file is <b>pjlib-test/stack.c</b>
  *
  * \include pjlib-test/stack.c
  */

#if INCLUDE_STACK_TEST

#include <pjlib.h>

#ifdef PJ_WIN32

#if !defined(MEMORY_ALLOCATION_ALIGNMENT)
#include <windows.h>
#endif  //MEMORY_ALLOCATION_ALIGNMENT

#endif  // PJ_WIN32

typedef struct PJ_SYS_ALIGN_PREFIX stack_node {
    PJ_DECL_STACK_MEMBER(struct stack_node);
    int value;
} PJ_SYS_ALIGN_SUFFIX stack_node;

int stack_test()
{
    pj_stack_type* stack = NULL;
    const int sz = 15;
    stack_node* nodes;
    stack_node* p;
    int i; // don't change to unsigned!

    pj_pool_t* pool = NULL;;
    pj_status_t status;
    int rc;

    pool = pj_pool_create(mem, NULL, 4096, 0, NULL);
    if (!pool)
    {
        rc = -10;
        goto error;
    }

    nodes = pj_pool_calloc(pool, sz, sizeof(stack_node));
    if (!nodes)
    {
        rc = -15;
        goto error;
    }

#ifdef PJ_WIN32

#if  defined(_MSC_VER)
#pragma warning(push)                                                  
#pragma warning(disable:4324)   // structure padded due to align()
#endif  // defined(_MSC_VER)

    if (TYPE_ALIGNMENT(stack_node) < MEMORY_ALLOCATION_ALIGNMENT)
    {
        //#error alignment of array's items used as stack node under Windows platform should not be less than MEMORY_ALLOCATION_ALIGNMENT
        rc = -3;
        goto error;
    }

#if  defined(_MSC_VER)
#pragma warning(pop)
#endif  // defined(_MSC_VER)

    for (i = 0; i < sz; ++i) {
        if (!IS_ALIGNED_PTR(&nodes[i], MEMORY_ALLOCATION_ALIGNMENT))
        {
            rc = -7;
            goto error;
        }
    }

#endif  // PJ_WIN32

    //const char* stack_test = "stack_test";
    // test stack_create()
    if ((status = pj_stack_create(pool, /*stack_test,*/ &stack)) != PJ_SUCCESS)
    {
        rc = -20;
        goto error;
    }

    // created stack is empty
    if (pj_stack_pop(stack))
    {
        rc = -25;
        goto error;
    }

    // Test push().
    for (i = 0; i < sz; ++i) {
        nodes[i].value = i;
        if ((status= pj_stack_push(stack, &nodes[i])) != PJ_SUCCESS)
        {
            rc = -30;
            goto error;
        }
    }

    // test pop().
    while((p = pj_stack_pop(stack)) != NULL) {
        --i;
        pj_assert(p->value == i); //FILO !
        if (p->value != i || p != nodes + i) {  
            {
                rc = -40;
                goto error;
            }
        }
    }
    if (i)
    {
        rc = -50;
        goto error;
    }
    rc = 0;
error:
    if (stack != NULL && pj_stack_destroy(stack) != PJ_SUCCESS && !rc)
    {
        rc = -55;
    }

    if (pool)
        pj_pool_release(pool);

    return rc;
}

#else
  /* To prevent warning about "translation unit is empty"
   * when this test is disabled.
   */
int dummy_stack_test;
#endif	/* INCLUDE_STACK_TEST */



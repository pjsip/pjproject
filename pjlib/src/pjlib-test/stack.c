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
  * MACROS tested:
  *  - PJ_STACK_ALIGN_PREFIX
  *  - PJ_STACK_ALIGN_SUFFIX
  *
  * This file is <b>pjlib-test/stack.c</b>
  *
  * \include pjlib-test/stack.c
  */

#if INCLUDE_STACK_TEST

#include <pjlib.h>

#if defined(PJ_WIN32) && PJ_STACK_IMPLEMENTATION==PJ_STACK_WIN32 && !defined(MEMORY_ALLOCATION_ALIGNMENT)

#   include <windows.h>

#endif

#define THIS_FILE       "stack.c"
#define MAX_RESERVED    16
#define MAX_SLOTS       100

#define MAX_THREADS     2
#define MAX_REPEATS     100000

#define TRACE(log)      PJ_LOG(3,log)

typedef struct PJ_STACK_ALIGN_PREFIX stack_node {
    PJ_DECL_STACK_MEMBER(struct stack_node);
    int value;
} PJ_STACK_ALIGN_SUFFIX stack_node;


/* any useful data; 
 * here simply pj_thread_t* - thread owned this slot 
 * alignment of array's items used as stack node under Windows platform should not be less than MEMORY_ALLOCATION_ALIGNMENT
 */
typedef struct PJ_STACK_ALIGN_PREFIX slot_data
{
    PJ_DECL_STACK_MEMBER(struct slot_data);

    pj_thread_t     *owner;                             /* this member simulates "payload" - user data stored in this slot */
} PJ_STACK_ALIGN_SUFFIX slot_data;

typedef struct stack_test_desc stack_test_desc;
typedef struct stack_test_desc {
    struct {
        const char          *title;
        int                  n_threads;                 /* number of worker threads */
        unsigned             repeat;                    /* number of successfull slot reservation on each concurrent thread*/

    } cfg;

    struct {
        pj_pool_t           *pool;
        pj_stack_type       *empty_slot_stack;          /**< Empty slots stack. In current implemetation each stack item store pointer to slot*/
        slot_data            slots[MAX_SLOTS];          /**< Array of useful information "slots" (file players, for example).*/
        int                  retcode;                   /* test retcode. non-zero will abort. */
    } state;
}   stack_test_desc;

static int stack_stress_test_init(stack_test_desc* test);
static int stack_stress_test_destroy(stack_test_desc* test);

static int stack_stress_test(stack_test_desc* test);
static int worker_thread(void* p);

static stack_test_desc tests[] = {
    {
        .cfg.title = "stack (single thread)",
        .cfg.n_threads = 0,
        .cfg.repeat = MAX_REPEATS
    },
    {
        .cfg.title = "stack (multi threads)",
        .cfg.n_threads = MAX_THREADS,
        .cfg.repeat = MAX_REPEATS
    }
};

int stack_test()
{
    pj_stack_type   *stack = NULL;
    const int        sz = 15;
    stack_node      *nodes;
    stack_node      *p;
    int i; // don't change to unsigned!

    pj_pool_t       *pool = NULL;;
    pj_status_t      status;
    int              rc;

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

#if defined(PJ_WIN32) && PJ_STACK_IMPLEMENTATION==PJ_STACK_WIN32

    /* Here we check our alignment macros PJ_STACK_ALIGN_PREFIX, PJ_STACK_ALIGN_SUFFIX */

#   if  defined(_MSC_VER)
#       pragma warning(push)                                                  
#       pragma warning(disable:4324)   // structure padded due to align()
#   endif  // defined(_MSC_VER)

    if (TYPE_ALIGNMENT(stack_node) < MEMORY_ALLOCATION_ALIGNMENT)
    {
        //#error alignment of array's items used as stack node under Windows platform should not be less than MEMORY_ALLOCATION_ALIGNMENT
        rc = -3;
        goto error;
    }

#   if  defined(_MSC_VER)
#       pragma warning(pop)
#   endif  // defined(_MSC_VER)

    for (i = 0; i < sz; ++i) {
        if (!IS_ALIGNED_PTR(&nodes[i], MEMORY_ALLOCATION_ALIGNMENT))
        {
            rc = -7;
            goto error;
        }
    }

#endif  // PJ_STACK_IMPLEMENTATION==PJ_STACK_WIN32

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
            rc = -40;
            goto error;
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

    for (i = 0; !rc && i < PJ_ARRAY_SIZE(tests); ++i) {
        tests[i].state.pool = pool;
        rc = stack_stress_test(&tests[i]);
    }

    if (pool)
        pj_pool_release(pool);

    return rc;
}

/*
 * This test illustrates:
 * 1) a multi-threaded use case for the pj_stack API (pj_stack is thread safe)
 * 2) a useful idea: reserving an empty slot in a large array without having to lock the entire array
 */
static int stack_stress_test(stack_test_desc* test) {
    unsigned i;
    pj_status_t status;
    pj_timestamp t1, t2;
    int rc;

    TRACE((THIS_FILE, "%s", test->cfg.title));
    int ident = pj_log_get_indent();    /* worker_thread change ident on this thread */
    pj_log_push_indent();

    rc = stack_stress_test_init(test);
    if (rc)
        return rc;

    pj_get_timestamp(&t1);

    if (test->cfg.n_threads == 0)
        worker_thread(test);
    else {
        unsigned n_threads = test->cfg.n_threads;
        pj_thread_t* threads[MAX_THREADS];

        for (i = 0; i < n_threads; ++i) {
            status = pj_thread_create(test->state.pool, "stack_stress_test",
                &worker_thread, test,
                0, PJ_THREAD_SUSPENDED,
                &threads[i]);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(1, (THIS_FILE, status, "Unable to create thread"));
                return -70;
            }
        }

        for (i = 0; i < n_threads; ++i) {
            status = pj_thread_resume(threads[i]);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(1, (THIS_FILE, status, "Unable to resume thread"));
                return -75;
            }
        }

        worker_thread(test);

        for (i = 0; i < n_threads; ++i) {
            pj_thread_join(threads[i]);
            pj_thread_destroy(threads[i]);
        }
    }

    pj_get_timestamp(&t2);

    rc = stack_stress_test_destroy(test);
    if (rc)
        return rc;

    TRACE((THIS_FILE, "%s time: %d ms", test->cfg.title, pj_elapsed_msec(&t1, &t2)));

    pj_log_set_indent(ident); /* restore ident changed by worker_thread() instead of pj_log_pop_indent() */
    return test->state.retcode;

}

static int stack_stress_test_init(stack_test_desc* test) {
    pj_status_t status;
    status = pj_stack_create(test->state.pool, &test->state.empty_slot_stack);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Unable to create stack"));
        return -60;
    }
    slot_data* p;
    for (p = test->state.slots + PJ_ARRAY_SIZE(test->state.slots) - 1; p > test->state.slots - 1; --p) {
        if ((status = pj_stack_push(test->state.empty_slot_stack, p)) != PJ_SUCCESS) {
            PJ_PERROR(1, (THIS_FILE, status, "Unable to init stack"));
            return -65;
        }
    }
    return 0;
}

static int stack_stress_test_destroy(stack_test_desc* test) {
    pj_status_t status;
    status = pj_stack_destroy(test->state.empty_slot_stack);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Unable to destroy stack"));
        return -80;
    }
    return 0;
}

/* worker thread */
static int worker_thread(void* p) {
    stack_test_desc* test = (stack_test_desc*)p;

    unsigned    n_events = 0;
    unsigned    reserved_slots[MAX_RESERVED];
    unsigned    reserved_count = 0;
    unsigned    slot_id;

    pj_bzero(reserved_slots, sizeof(reserved_slots));

    /* log indent is not propagated to other threads,
     * so we set it explicitly here
     */
    pj_log_set_indent(3);

    while (test->state.retcode == 0 && n_events < test->cfg.repeat) {
        slot_data* slot = pj_stack_pop(test->state.empty_slot_stack);
        if (slot != NULL) {                         /* we have got an empty slot */
            if (slot->owner != NULL) {
                PJ_LOG(1, (THIS_FILE, "Reserved slot is not empty"));
                test->state.retcode = -90;
                break;
            }
            else {
                slot->owner = pj_thread_this();     /* slot reserved successfully */
                slot_id = slot - test->state.slots;
                reserved_slots[reserved_count++] = slot_id;
                ++n_events;
            }
        }
        if (slot == NULL ||                         /* no empty slots at all or */
            reserved_count >= MAX_RESERVED ||       /* this thread has reserved the maximum number of slots allowed */
            n_events >= test->cfg.repeat) {         /* or test completed */
            while (reserved_count) {                /* clear slots reserved here */
                slot_id = reserved_slots[--reserved_count];
                slot = &test->state.slots[slot_id];
                if (slot->owner != pj_thread_this()) {
                    PJ_LOG(1, (THIS_FILE, "Anothed thread has reserved this thread's slot"));
                    test->state.retcode = -85;
                }
                else if (slot->owner == NULL) {
                    PJ_LOG(1, (THIS_FILE, "Anothed thread has freed up this thread's slot"));
                    test->state.retcode = -95;
                } 
                else {
                    slot->owner = NULL;                                /* free up slot before returning */
                    pj_stack_push(test->state.empty_slot_stack, slot); /* slot returned to empty slot's stack */
                }
            }
        }

    }

    TRACE((THIS_FILE, "thread exiting, n_events=%d", n_events));
    return 0;
}


#else
  /* To prevent warning about "translation unit is empty"
   * when this test is disabled.
   */
int dummy_stack_test;
#endif	/* INCLUDE_STACK_TEST */



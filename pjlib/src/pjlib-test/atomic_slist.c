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

/* 2022-2025 Leonid Goltsblat <lgoltsblat@gmail.com> */

#include "test.h"

 /**
  * \page page_pjlib_atomic_slist_test Test: Slist
  *
  * This file provides implementation of \b atomic_slist_test() and
  * \b atomic_slist_mt_test(). It tests the functionality and perfomance
  * of the atomic_slist API.
  *
  * \section slist_test_sec Scope of the Test
  *
  * API tested:
  *  - pj_atomic_slist_create()
  *  - pj_atomic_slist_push()
  *  - pj_atomic_slist_pop()
  *  - pj_atomic_slist_destroy()
  *  - pj_atomic_slist_calloc()
  *
  * MACROS tested:
  *  - PJ_ATOMIC_SLIST_ALIGN_PREFIX
  *  - PJ_ATOMIC_SLIST_ALIGN_SUFFIX
  *
  * This file is <b>pjlib-test/atomic_slist.c</b>
  *
  * \include pjlib-test/atomic_slist.c
  */

#if INCLUDE_ATOMIC_SLIST_TEST

#include <pjlib.h>

#if defined(PJ_WIN32) && PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32

#ifndef MEMORY_ALLOCATION_ALIGNMENT
#   include <windows.h>
#endif // !MEMORY_ALLOCATION_ALIGNMENT

#ifndef PJ_IS_ALIGNED
#   define PJ_IS_ALIGNED(PTR, ALIGNMENT)       (!((pj_ssize_t)(PTR) & ((ALIGNMENT)-1)))
#endif  // !PJ_IS_ALIGNED

#endif

#define THIS_FILE           "atomic_slist.c"
#define MAX_RESERVED        16
#define MAX_SLOTS           100

#define MAX_WORKER_THREADS  3
#define MAX_REPEATS         100000

#define TRACE(log)          PJ_LOG(3,log)

typedef struct PJ_ATOMIC_SLIST_ALIGN_PREFIX slist_node {
    PJ_DECL_ATOMIC_SLIST_MEMBER(struct slist_node);
    int     value;
    char    unaligner;  /* to unalign slist_node. 
                         * Without PJ_ATOMIC_SLIST_ALIGN_* 
                         * sizeof(slist_node)%MEMORY_ALLOCATION_ALIGNMENT != 0
                         */
} PJ_ATOMIC_SLIST_ALIGN_SUFFIX slist_node;


/* any useful data; 
 * here simply pj_thread_t* - thread owned this slot 
 * alignment of array's items used as slist node under Windows platform should not be less than MEMORY_ALLOCATION_ALIGNMENT
 */
typedef struct PJ_ATOMIC_SLIST_ALIGN_PREFIX slot_data
{
    PJ_DECL_ATOMIC_SLIST_MEMBER(struct slot_data);

    /* this member simulates "payload" - user data stored in this slot */
    pj_thread_t     *owner;                             
} PJ_ATOMIC_SLIST_ALIGN_SUFFIX slot_data;

typedef struct slist_test_desc {
    struct {
        const char          *title;
        int                  n_threads;                 /* number of worker threads */
        unsigned             repeat;                    /* number of successfull slot reservation on each concurrent thread*/
    } cfg;

    struct {
        pj_pool_t           *pool;
        pj_atomic_slist     *empty_slot_slist;  /**< Empty slots slist. */

        /* to unalign slots[].*/
        char                 unaligner2;        
        /* 
         * In Visual Studio we will see the following warning here:
         * warning C4324: '<unnamed-tag>': structure was padded due to alignment specifier
         * 
         * The compiler ensures that the elements of the slots[] array are 
         * aligned if slist_test_desc allocated statically. 
         * Otherwise (dynamically allocated from the pool), the application 
         * must request correctly aligned memory for slist_test_desc.
         */
        slot_data            slots[MAX_SLOTS];  /**< Array of useful information
                                                 * "slots" (file players, for example)*/

        int                  retcode;           /* test retcode. non-zero will abort. */
    } state;
}   slist_test_desc;

static int slist_stress_test_init(slist_test_desc* test);
static int slist_stress_test_destroy(slist_test_desc* test);

static int slist_stress_test(slist_test_desc* test);
static int worker_thread(void* p);

static slist_test_desc tests[] = {
    {
        .cfg.title = "slist (single thread)",
        .cfg.n_threads = 0,
        .cfg.repeat = MAX_REPEATS
    },
    {
        .cfg.title = "slist (multi threads)",
        .cfg.n_threads = MAX_WORKER_THREADS,
        .cfg.repeat = MAX_REPEATS
    }
};

int atomic_slist_test()
{
    pj_atomic_slist *slist = NULL;
    const int        sz = 15;
    slist_node      *nodes;
    slist_node      *p;
    int              i; // don't change to unsigned!

    pj_pool_t       *pool = NULL;;
    int              rc = 0;

    pool = pj_pool_create(mem, NULL, 4096, 0, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, { rc=-10; goto error; });

    nodes = pj_atomic_slist_calloc(pool, sz, sizeof(slist_node));
    PJ_TEST_NOT_NULL(nodes, NULL, { rc=-15; goto error; });

#if defined(PJ_WIN32) && PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32

    /* Here we check our alignment macros PJ_ATOMIC_SLIST_ALIGN_PREFIX, PJ_ATOMIC_SLIST_ALIGN_SUFFIX */

#   if  defined(_MSC_VER)
#       pragma warning(push)                                                  
#       pragma warning(disable:4324)   // structure padded due to align()
#   endif  // defined(_MSC_VER)

    PJ_TEST_GTE(TYPE_ALIGNMENT(slist_node), MEMORY_ALLOCATION_ALIGNMENT, NULL, { rc=-3; goto error; });

#   if  defined(_MSC_VER)
#       pragma warning(pop)
#   endif  // defined(_MSC_VER)

    for (i = 0; i < sz; ++i) 
        PJ_TEST_TRUE(PJ_IS_ALIGNED(&nodes[i], MEMORY_ALLOCATION_ALIGNMENT), NULL, { rc=-7; goto error; });

#endif  // PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32

    // test slist_create()
    PJ_TEST_SUCCESS(pj_atomic_slist_create(pool, &slist), NULL, { rc=-20; goto error; });

    // newly created slist is empty
    PJ_TEST_EQ(pj_atomic_slist_pop(slist), NULL, NULL, { rc=-25; goto error; });

    // Test push().
    for (i = 0; i < sz; ++i) {
        nodes[i].value = i;
        PJ_TEST_SUCCESS(pj_atomic_slist_push(slist, &nodes[i]), NULL, { rc=-30; goto error; });
    }

    // test pop().
    while((p = pj_atomic_slist_pop(slist)) != NULL) {
        --i;
        pj_assert(p->value == i); //FILO !
        PJ_TEST_TRUE(p->value == i && p == nodes+i, NULL, { rc=-40; goto error; });
    }
    PJ_TEST_EQ(i, 0, NULL, { rc=-50; goto error; });

error:
    if (slist != NULL)
        PJ_TEST_SUCCESS(pj_atomic_slist_destroy(slist), NULL, if (!rc) rc = -55);

    if (pool)
        pj_pool_release(pool);

    return rc;
}

int atomic_slist_mt_test()
{
    pj_pool_t       *pool = NULL;;
    int              rc = 0;
    int              i;

    pool = pj_pool_create(mem, NULL, 4096, 0, NULL);
    PJ_TEST_NOT_NULL(pool, NULL, return -100);

    for (i = 0; !rc && i < PJ_ARRAY_SIZE(tests); ++i) {
        tests[i].state.pool = pool;
        rc = slist_stress_test(&tests[i]);
    }

    if (pool)
        pj_pool_release(pool);

    return rc;
}

/*
 * This test illustrates:
 * 1) a multi-threaded use case for the pj_atomic_slist API (pj_atomic_slist is thread safe)
 * 2) a useful idea: reserving an empty slot in a large array without having to lock the entire array
 */
static int slist_stress_test(slist_test_desc* test) {
    unsigned i;
    pj_timestamp t1, t2;
    int rc;

    TRACE((THIS_FILE, "%s", test->cfg.title));
    int ident = pj_log_get_indent();    /* worker_thread change ident on this thread */
    pj_log_push_indent();

    rc = slist_stress_test_init(test);
    if (rc)
        return rc;

    pj_get_timestamp(&t1);

    if (test->cfg.n_threads == 0)
        worker_thread(test);
    else {
        unsigned n_threads = test->cfg.n_threads;
        pj_thread_t* threads[MAX_WORKER_THREADS];

        for (i = 0; i < n_threads; ++i)
            PJ_TEST_SUCCESS((pj_thread_create(test->state.pool, "slist_stress_test",
                                              &worker_thread, test,
                                              0, PJ_THREAD_SUSPENDED,
                                              &threads[i])), 
                            "Unable to create thread", return -105);

        for (i = 0; i < n_threads; ++i)
            PJ_TEST_SUCCESS(pj_thread_resume(threads[i]), "Unable to resume thread", return -110);

        worker_thread(test);

        for (i = 0; i < n_threads; ++i) {
            pj_thread_join(threads[i]);
            pj_thread_destroy(threads[i]);
        }
    }

    pj_get_timestamp(&t2);

    rc = slist_stress_test_destroy(test);
    if (rc)
        return rc;

    TRACE((THIS_FILE, "%s time: %d ms (total), %d nanosec/op", test->cfg.title, 
           pj_elapsed_msec(&t1, &t2), 
           pj_elapsed_nanosec(&t1, &t2) / ((test->cfg.n_threads+1)*test->cfg.repeat)));

    pj_log_set_indent(ident); /* restore ident changed by worker_thread() instead of pj_log_pop_indent() */
    return test->state.retcode;

}

static int slist_stress_test_init(slist_test_desc* test) {
    slot_data* p;
    PJ_TEST_SUCCESS(pj_atomic_slist_create(test->state.pool, &test->state.empty_slot_slist), "Unable to create slist", return -115);

    for (p = test->state.slots + PJ_ARRAY_SIZE(test->state.slots) - 1; p > test->state.slots - 1; --p)
        PJ_TEST_SUCCESS(pj_atomic_slist_push(test->state.empty_slot_slist, p), "Unable to init slist", return -120);

    return 0;
}

static int slist_stress_test_destroy(slist_test_desc* test) {
    PJ_TEST_SUCCESS(pj_atomic_slist_destroy(test->state.empty_slot_slist), "Unable to destroy slist", return -125);
    return 0;
}

/* worker thread */
static int worker_thread(void* p) {
    slist_test_desc* test = (slist_test_desc*)p;

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
        slot_data* slot = pj_atomic_slist_pop(test->state.empty_slot_slist);
        if (slot != NULL) {                         /* we have got an empty slot */
            PJ_TEST_EQ(slot->owner, NULL, "Reserved slot is not empty", {
                            test->state.retcode = -130;
                            break;
                       });
            slot->owner = pj_thread_this();     /* slot reserved successfully */
            slot_id = slot - test->state.slots;
            reserved_slots[reserved_count++] = slot_id;
            ++n_events;
        }
        if (slot == NULL ||                         /* no empty slots at all or */
            reserved_count >= MAX_RESERVED ||       /* this thread has reserved the maximum number of slots allowed */
            n_events >= test->cfg.repeat) {         /* or test completed */
            while (reserved_count) {                /* clear slots reserved here */
                slot_id = reserved_slots[--reserved_count];
                slot = &test->state.slots[slot_id];
                if (slot->owner == pj_thread_this()) {
                    slot->owner = NULL;                                       /* free up slot before returning */
                    pj_atomic_slist_push(test->state.empty_slot_slist, slot); /* slot returned to empty slot's slist */
                } else if (slot->owner == NULL) {
                    PJ_LOG(1, (THIS_FILE, "Anothed thread has freed up this thread's slot"));
                    test->state.retcode = -135;
                }  else {
                    PJ_LOG(1, (THIS_FILE, "Anothed thread has reserved this thread's slot"));
                    test->state.retcode = -140;
                }
            }
        }

    }

    TRACE((THIS_FILE, "thread exiting %s, n_events=%d", test->cfg.title, n_events));
    return 0;
}


#else
  /* To prevent warning about "translation unit is empty"
   * when this test is disabled.
   */
int dummy_slist_test;
#endif	/* INCLUDE_ATOMIC_SLIST_TEST */



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

#include <pj/os.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/errno.h>

#define THIS_FILE       "atomic_slist.c"

#if defined(PJ_ATOMIC_SLIST_IMPLEMENTATION) && PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32

#ifdef PJ_WIN32
#   include <windows.h>
#else
#   error atomic slist PJ_ATOMIC_SLIST_WIN32 implementation should be compiled only for Windows platform
#endif  // PJ_WIN32

//uncomment next line to check alignment in runtime
#define PJ_ATOMIC_SLIST_ALIGN_DEBUG


#ifndef PJ_IS_ALIGNED
#   define PJ_IS_ALIGNED(PTR, ALIGNMENT)       (!((pj_ssize_t)(PTR) & ((ALIGNMENT)-1)))
#endif  //PJ_IS_ALIGNED

#endif //defined(PJ_ATOMIC_SLIST_IMPLEMENTATION) && PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32

#include <pj/atomic_slist.h>

#if defined(PJ_ATOMIC_SLIST_IMPLEMENTATION) && PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_GENERIC

//#pragma message("compilation of cross-platform slist implementation")

/**
 * This structure describes generic slist node.
 */
typedef struct pj_atomic_slist_node
{
    PJ_DECL_ATOMIC_SLIST_MEMBER(struct pj_atomic_slist_node);
} PJ_ATTR_MAY_ALIAS pj_atomic_slist_node; /* may_alias avoids warning with gcc-4.4 -Wall -O2 */


struct pj_atomic_slist
{
    pj_atomic_slist_node    head;
    pj_mutex_t             *mutex;
};

PJ_DEF(pj_status_t) pj_atomic_slist_create(pj_pool_t *pool, pj_atomic_slist **slist)
{
    pj_atomic_slist *p_slist;
    pj_status_t      rc;

    PJ_ASSERT_RETURN(pool && slist, PJ_EINVAL);

    p_slist = PJ_POOL_ZALLOC_T(pool, pj_atomic_slist);
    if (!p_slist)
        return PJ_ENOMEM;

    char                name[PJ_MAX_OBJ_NAME];
    /* Set name. */
    pj_ansi_snprintf(name, PJ_MAX_OBJ_NAME, "slst%p", p_slist);

    rc = pj_mutex_create_simple(pool, name, &p_slist->mutex);
    if (rc != PJ_SUCCESS)
        return rc;


    p_slist->head.next = &p_slist->head;
    *slist = p_slist;

    PJ_LOG(6, (THIS_FILE, "Atomic slist created slst%p", p_slist));
    return PJ_SUCCESS;

}


PJ_DEF(pj_status_t) pj_atomic_slist_destroy(pj_atomic_slist *slist)
{
    PJ_ASSERT_RETURN(slist, PJ_EINVAL);
    pj_status_t rc;
    rc = pj_mutex_destroy(slist->mutex);
    if (rc == PJ_SUCCESS)
    {
        PJ_LOG(6, (THIS_FILE, "Atomic slist destroyed slst%p", slist));
    }
    return rc;
}


PJ_DEF(pj_status_t) pj_atomic_slist_push(pj_atomic_slist *slist, pj_atomic_slist_node_t *node)
{
    PJ_ASSERT_RETURN(node && slist, PJ_EINVAL);
    pj_status_t status;
    if ((status = pj_mutex_lock(slist->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_atomic_slist_push", status, "Error locking mutex for slist slst%p", slist));
        return status;
    }
    ((pj_atomic_slist_node*)node)->next = slist->head.next;
    slist->head.next = node;
    if ((status = pj_mutex_unlock(slist->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_atomic_slist_push", status, "Error unlocking mutex for slist slst%p", slist));
    }
    return status;
}


PJ_DEF(pj_atomic_slist_node_t*) pj_atomic_slist_pop(pj_atomic_slist *slist)
{
    PJ_ASSERT_RETURN(slist, NULL);
    pj_status_t status;
    if ((status = pj_mutex_lock(slist->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_atomic_slist_pop", status, "Error locking mutex for slist slst%p", slist));
        return NULL;
    }

    pj_atomic_slist_node *node = slist->head.next;
    if (node != &slist->head) {
        slist->head.next = node->next;
        node->next = NULL;
    }
    else
        node = NULL;

    if ((status = pj_mutex_unlock(slist->mutex)) != PJ_SUCCESS)
        PJ_PERROR(1, ("pj_atomic_slist_pop", status, "Error unlocking mutex for slist slst%p", slist));

    return node;
}


/**
 * Traverse the slist and get it's elements quantity.
 * The return value of pj_atomic_slist_size should not be relied upon in multithreaded applications
 * because the item count can be changed at any time by another thread.
 */
PJ_DEF(pj_size_t) pj_atomic_slist_size(/*const*/ pj_atomic_slist *slist)
{
    const pj_atomic_slist_node *node;
    pj_size_t count;
    pj_status_t status;

    PJ_ASSERT_RETURN(slist, 0);

    if ((status = pj_mutex_lock(slist->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_atomic_slist_size", status, "Error locking mutex for slist slst%p", slist));
        return 0;
    }

    for (node = slist->head.next, count = 0; node != node->next; node = node->next)
        ++count;

    if ((status = pj_mutex_unlock(slist->mutex)) != PJ_SUCCESS)
    {
        PJ_PERROR(1, ("pj_atomic_slist_size", status, "Error unlocking mutex for slist slst%p", slist));
    }

    return count;
}

PJ_DEF(void*) pj_atomic_slist_calloc(pj_pool_t *pool, pj_size_t count, pj_size_t elem)
{
    return pj_pool_calloc(pool, count, elem);
}


#elif defined(PJ_ATOMIC_SLIST_IMPLEMENTATION) && PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32

//#pragma message("compilation of Windows platform slist implementation")


struct pj_atomic_slist {
    SLIST_HEADER head;
};


PJ_DEF(pj_status_t) pj_atomic_slist_create(pj_pool_t *pool, pj_atomic_slist **slist)
{

    PJ_ASSERT_RETURN(pool && slist, PJ_EINVAL);

    pj_atomic_slist *p_slist = pj_pool_aligned_alloc(pool, MEMORY_ALLOCATION_ALIGNMENT, sizeof(pj_atomic_slist));
    if (!p_slist)
        return PJ_ENOMEM;

#ifdef PJ_ATOMIC_SLIST_ALIGN_DEBUG
    PJ_ASSERT_ON_FAIL(PJ_IS_ALIGNED(&p_slist->head, MEMORY_ALLOCATION_ALIGNMENT), {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_atomic_slist implementation requires alignment not less than %d (slst%p).", MEMORY_ALLOCATION_ALIGNMENT, p_slist));
            return PJ_EINVALIDOP;
        });
#endif //PJ_ATOMIC_SLIST_ALIGN_DEBUG

    InitializeSListHead(&p_slist->head);

    *slist = p_slist;
    PJ_LOG(6, (THIS_FILE, "Atomic slist created slst%p", p_slist));
    return PJ_SUCCESS;

}


PJ_DEF(pj_status_t) pj_atomic_slist_destroy(pj_atomic_slist *slist)
{
    PJ_ASSERT_RETURN(slist, PJ_EINVAL);
    //nothing to do here
    PJ_LOG(6, (THIS_FILE, "Atomic slist destroyed slst%p", slist));
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_atomic_slist_push(pj_atomic_slist *slist, pj_atomic_slist_node_t *node)
{
#ifdef PJ_ATOMIC_SLIST_ALIGN_DEBUG
    PJ_ASSERT_RETURN(node && slist, PJ_EINVAL);

    PJ_ASSERT_ON_FAIL(PJ_IS_ALIGNED(&slist->head, MEMORY_ALLOCATION_ALIGNMENT) &&
        PJ_IS_ALIGNED(node, MEMORY_ALLOCATION_ALIGNMENT),
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_atomic_slist implementation requires alignment not less than %d (slst%p).", MEMORY_ALLOCATION_ALIGNMENT, slist));
            return PJ_EINVALIDOP;
        });
#endif //PJ_ATOMIC_SLIST_ALIGN_DEBUG

    /*pFirstEntry =*/ InterlockedPushEntrySList(&slist->head, node);
    return PJ_SUCCESS;
}


PJ_DEF(pj_atomic_slist_node_t*) pj_atomic_slist_pop(pj_atomic_slist *slist)
{
#ifdef PJ_ATOMIC_SLIST_ALIGN_DEBUG
    PJ_ASSERT_RETURN(slist, NULL);
    PJ_ASSERT_ON_FAIL(PJ_IS_ALIGNED(&slist->head, MEMORY_ALLOCATION_ALIGNMENT),
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_atomic_slist implementation requires alignment not less than %d (slst%p).", MEMORY_ALLOCATION_ALIGNMENT, slist));
            return NULL;
        });
#endif //PJ_ATOMIC_SLIST_ALIGN_DEBUG

    return InterlockedPopEntrySList(&slist->head);
}

/**
 * Get the slist's elements quantity.
 * The return value of pj_atomic_slist_size should not be relied upon in multithreaded applications
 * because the item count can be changed at any time by another thread.
 * For Windows platform returns the number of entries in the slist modulo 65535. For example,
 * if the specified slist contains 65536 entries, pj_atomic_slist_size returns zero.
 */
PJ_DEF(pj_size_t) pj_atomic_slist_size(/*const*/ pj_atomic_slist *slist)
{
#ifdef PJ_ATOMIC_SLIST_ALIGN_DEBUG
    PJ_ASSERT_RETURN(slist, 0);
    PJ_ASSERT_ON_FAIL(PJ_IS_ALIGNED(&slist->head, MEMORY_ALLOCATION_ALIGNMENT),
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_atomic_slist implementation requires alignment not less than %d (slst%p).", MEMORY_ALLOCATION_ALIGNMENT, slist));
            return 0;
        });
#endif //PJ_ATOMIC_SLIST_ALIGN_DEBUG

    return QueryDepthSList(&slist->head);
}

PJ_DEF(void*) pj_atomic_slist_calloc(pj_pool_t *pool, pj_size_t count, pj_size_t elem)
{
#ifdef PJ_ATOMIC_SLIST_ALIGN_DEBUG
    PJ_ASSERT_ON_FAIL(PJ_IS_ALIGNED(elem, MEMORY_ALLOCATION_ALIGNMENT),
        {
            PJ_LOG(1, (THIS_FILE, "Windows platform's pj_atomic_slist implementation requires alignment not less than %d.", MEMORY_ALLOCATION_ALIGNMENT));
            return 0;
        });
#endif //PJ_ATOMIC_SLIST_ALIGN_DEBUG

    void *p = pj_pool_aligned_alloc(pool, MEMORY_ALLOCATION_ALIGNMENT, elem*count);
    if (p)
        pj_bzero(p, elem*count);
    return p;
}

#endif // PJ_ATOMIC_SLIST_IMPLEMENTATION==PJ_ATOMIC_SLIST_WIN32

/*
 * Copyright (C) 2022 Teluu Inc. (http://www.teluu.com)
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
/*
 * ioqueue_kqueue.c
 *
 * This is the implementation of IOQueue framework using kqueue on macos/BSD
 */

#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/list.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>

#include <sys/event.h>

#define os_kqueue_open kqueue
#define os_kqueue_close close
#define os_kqueue_ctl kevent
#define os_kqueue_wait kevent

#define THIS_FILE "ioq_kqueue"

//#define TRACE_(expr) PJ_LOG(3,expr)
#define TRACE_(expr)

/*
 * Include common ioqueue abstraction.
 */
#include "ioqueue_common_abs.h"

/*
 * This describes each key.
 */
struct pj_ioqueue_key_t {
    DECLARE_COMMON_KEY
};

struct queue {
    pj_ioqueue_key_t *key;
    enum ioqueue_event_type event_type;
};

/*
 * This describes the I/O queue.
 */
struct pj_ioqueue_t {
    DECLARE_COMMON_IOQUEUE unsigned max, count;
    pj_ioqueue_key_t active_list;
    int kfd; // kqueue fd

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    pj_mutex_t *ref_cnt_mutex;
    pj_ioqueue_key_t closing_list;
    pj_ioqueue_key_t free_list;
#endif
};

/* Include implementation for common abstraction after we declare
 * pj_ioqueue_key_t and pj_ioqueue_t.
 */
#include "ioqueue_common_abs.c"

#if PJ_IOQUEUE_HAS_SAFE_UNREG
/* Scan closing keys to be put to free list again */
static void scan_closing_keys(pj_ioqueue_t *ioqueue);
#endif

/*
 * pj_ioqueue_name()
 */
PJ_DEF(const char *) pj_ioqueue_name(void)
{
    return "kqueue";
}

/*
 * pj_ioqueue_create()
 *
 * Create ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_create( pj_pool_t *pool,
                                       pj_size_t max_fd,
                                       pj_ioqueue_t **p_ioqueue)
{
    return pj_ioqueue_create2(pool, max_fd, NULL, p_ioqueue);
}

/*
 * pj_ioqueue_create2()
 *
 * Create ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_create2(pj_pool_t *pool,
                                       pj_size_t max_fd,
                                       const pj_ioqueue_cfg *cfg,
                                       pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioqueue;
    pj_status_t rc;
    pj_lock_t *lock;
    pj_size_t i;

    /* Check that arguments are valid. */
    PJ_ASSERT_RETURN(pool != NULL && p_ioqueue != NULL && max_fd > 0,
                     PJ_EINVAL);

    /* Check that size of pj_ioqueue_op_key_t is sufficient */
    PJ_ASSERT_RETURN(sizeof(pj_ioqueue_op_key_t) - sizeof(void *) >=
                         sizeof(union operation_key),
                     PJ_EBUG);

    ioqueue = pj_pool_alloc(pool, sizeof(pj_ioqueue_t));

    ioqueue_init(ioqueue);

    if (cfg)
        pj_memcpy(&ioqueue->cfg, cfg, sizeof(*cfg));
    else
        pj_ioqueue_cfg_default(&ioqueue->cfg);
    ioqueue->max = max_fd;
    ioqueue->count = 0;
    pj_list_init(&ioqueue->active_list);

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    /* When safe unregistration is used (the default), we pre-create
     * all keys and put them in the free list.
     */

    /* Mutex to protect key's reference counter
     * We don't want to use key's mutex or ioqueue's mutex because
     * that would create deadlock situation in some cases.
     */
    rc = pj_mutex_create_simple(pool, NULL, &ioqueue->ref_cnt_mutex);
    if (rc != PJ_SUCCESS)
        return rc;

    /* Init key list */
    pj_list_init(&ioqueue->free_list);
    pj_list_init(&ioqueue->closing_list);

    /* Pre-create all keys according to max_fd */
    for (i = 0; i < max_fd; ++i) {
        pj_ioqueue_key_t *key;

        key = PJ_POOL_ALLOC_T(pool, pj_ioqueue_key_t);
        key->ref_count = 0;
        rc = pj_lock_create_recursive_mutex(pool, NULL, &key->lock);
        if (rc != PJ_SUCCESS) {
            key = ioqueue->free_list.next;
            while (key != &ioqueue->free_list) {
                pj_lock_destroy(key->lock);
                key = key->next;
            }
            pj_mutex_destroy(ioqueue->ref_cnt_mutex);
            return rc;
        }

        pj_list_push_back(&ioqueue->free_list, key);
    }
#endif

    rc = pj_lock_create_simple_mutex(pool, "ioq%p", &lock);
    if (rc != PJ_SUCCESS)
        return rc;

    rc = pj_ioqueue_set_lock(ioqueue, lock, PJ_TRUE);
    if (rc != PJ_SUCCESS)
        return rc;

    /* create kqueue */
    ioqueue->kfd = os_kqueue_open();
    if (ioqueue->kfd == -1) {
        pj_lock_acquire(ioqueue->lock);
        ioqueue_destroy(ioqueue);
        return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
    }

    /* set close-on-exec flag */
    pj_set_cloexec_flag(ioqueue->kfd);

    PJ_LOG(4,
           ("pjlib", "%s I/O Queue created (%p)", pj_ioqueue_name(), ioqueue));

    *p_ioqueue = ioqueue;
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_destroy()
 *
 * Destroy ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_destroy(pj_ioqueue_t *ioqueue)
{
    pj_ioqueue_key_t *key;

    PJ_ASSERT_RETURN(ioqueue, PJ_EINVAL);
    PJ_ASSERT_RETURN(ioqueue->kfd > 0, PJ_EINVALIDOP);

    pj_lock_acquire(ioqueue->lock);
    os_kqueue_close(ioqueue->kfd);
    ioqueue->kfd = -1;

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    /* Destroy reference counters */
    key = ioqueue->active_list.next;
    while (key != &ioqueue->active_list) {
        pj_lock_destroy(key->lock);
        key = key->next;
    }

    key = ioqueue->closing_list.next;
    while (key != &ioqueue->closing_list) {
        pj_lock_destroy(key->lock);
        key = key->next;
    }

    key = ioqueue->free_list.next;
    while (key != &ioqueue->free_list) {
        pj_lock_destroy(key->lock);
        key = key->next;
    }

    pj_mutex_destroy(ioqueue->ref_cnt_mutex);
#endif
    return ioqueue_destroy(ioqueue);
}

/*
 * pj_ioqueue_register_sock()
 *
 * Register a socket to ioqueue.
 */
PJ_DEF(pj_status_t)
pj_ioqueue_register_sock2(pj_pool_t *pool, pj_ioqueue_t *ioqueue,
                          pj_sock_t sock, pj_grp_lock_t *grp_lock,
                          void *user_data, const pj_ioqueue_callback *cb,
                          pj_ioqueue_key_t **p_key)
{
    pj_ioqueue_key_t *key = NULL;
    pj_uint32_t value;
    int status;
    pj_status_t rc = PJ_SUCCESS;
    struct kevent events[2];

    PJ_ASSERT_RETURN(
        pool && ioqueue && sock != PJ_INVALID_SOCKET && cb && p_key, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

    if (ioqueue->count >= ioqueue->max) {
        rc = PJ_ETOOMANY;
        TRACE_((THIS_FILE, "pj_ioqueue_register_sock error: too many files"));
        goto on_return;
    }

    /* If safe unregistration (PJ_IOQUEUE_HAS_SAFE_UNREG) is used, get
     * the key from the free list. Otherwise allocate a new one.
     */
#if PJ_IOQUEUE_HAS_SAFE_UNREG

    /* Scan closing_keys first to let them come back to free_list */
    scan_closing_keys(ioqueue);

    pj_assert(!pj_list_empty(&ioqueue->free_list));
    if (pj_list_empty(&ioqueue->free_list)) {
        rc = PJ_ETOOMANY;
        goto on_return;
    }

    key = ioqueue->free_list.next;
    pj_list_erase(key);
#else
    /* Create key. */
    key = (pj_ioqueue_key_t *)pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));
#endif

    rc = ioqueue_init_key(pool, ioqueue, key, sock, grp_lock, user_data, cb);
    if (rc != PJ_SUCCESS) {
        key = NULL;
        goto on_return;
    }

    /* Initialize kevent structure, ADD read/write, default disable write */
    EV_SET(&events[0], key->fd, EVFILT_READ, EV_ADD, 0, 0, key);
    EV_SET(&events[1], key->fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, key);

    /* add event to kqueue */
    status = os_kqueue_ctl(ioqueue->kfd, events, 2, NULL, 0, NULL);
    if (status == -1) {
        rc = pj_get_os_error();
        pj_lock_destroy(key->lock);
        key = NULL;
        TRACE_((THIS_FILE, "pj_ioqueue_register_sock error: kevent rc=%d",
                status));
        goto on_return;
    }

    /* Set socket to nonblocking. */
    value = 1;
    if (ioctl(sock, FIONBIO, &value)) {
        rc = pj_get_netos_error();
        goto on_return;
    }

    /* Put in active list. */
    pj_list_insert_before(&ioqueue->active_list, key);
    ++ioqueue->count;

on_return:
    if (rc != PJ_SUCCESS) {
        if (key && key->grp_lock)
            pj_grp_lock_dec_ref_dbg(key->grp_lock, "ioqueue", 0);
    }
    *p_key = key;
    pj_lock_release(ioqueue->lock);

    return rc;
}

PJ_DEF(pj_status_t)
pj_ioqueue_register_sock(pj_pool_t *pool, pj_ioqueue_t *ioqueue, pj_sock_t sock,
                         void *user_data, const pj_ioqueue_callback *cb,
                         pj_ioqueue_key_t **p_key)
{
    return pj_ioqueue_register_sock2(pool, ioqueue, sock, NULL, user_data, cb,
                                     p_key);
}

#if PJ_IOQUEUE_HAS_SAFE_UNREG
/* Increment key's reference counter */
static void increment_counter(pj_ioqueue_key_t *key)
{
    pj_mutex_lock(key->ioqueue->ref_cnt_mutex);
    ++key->ref_count;
    pj_mutex_unlock(key->ioqueue->ref_cnt_mutex);
}

/* Decrement the key's reference counter, and when the counter reach zero,
 * destroy the key.
 *
 * Note: MUST NOT CALL THIS FUNCTION WHILE HOLDING ioqueue's LOCK.
 */
static void decrement_counter(pj_ioqueue_key_t *key)
{
    pj_lock_acquire(key->ioqueue->lock);
    pj_mutex_lock(key->ioqueue->ref_cnt_mutex);
    --key->ref_count;
    if (key->ref_count == 0) {

        pj_assert(key->closing == 1);
        pj_gettickcount(&key->free_time);
        key->free_time.msec += PJ_IOQUEUE_KEY_FREE_DELAY;
        pj_time_val_normalize(&key->free_time);

        pj_list_erase(key);
        pj_list_push_back(&key->ioqueue->closing_list, key);
    }
    pj_mutex_unlock(key->ioqueue->ref_cnt_mutex);
    pj_lock_release(key->ioqueue->lock);
}
#endif

/*
 * pj_ioqueue_unregister()
 *
 * Unregister handle from ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister(pj_ioqueue_key_t *key)
{
    pj_ioqueue_t *ioqueue;
    int status;
    struct kevent events[2];

    PJ_ASSERT_RETURN(key != NULL, PJ_EINVAL);

    ioqueue = key->ioqueue;

    /* Lock the key to make sure no callback is simultaneously modifying
     * the key. We need to lock the key before ioqueue here to prevent
     * deadlock.
     */
    pj_ioqueue_lock_key(key);

    /* Best effort to avoid double key-unregistration */
    if (IS_CLOSING(key)) {
        pj_ioqueue_unlock_key(key);
        return PJ_SUCCESS;
    }

    /* Also lock ioqueue */
    pj_lock_acquire(ioqueue->lock);

    /* Avoid "negative" ioqueue count */
    if (ioqueue->count > 0) {
        --ioqueue->count;
    } else {
        /* If this happens, very likely there is double unregistration
         * of a key.
         */
        pj_assert(!"Bad ioqueue count in key unregistration!");
        PJ_LOG(1, (THIS_FILE, "Bad ioqueue count in key unregistration!"));
    }

#if !PJ_IOQUEUE_HAS_SAFE_UNREG
    pj_list_erase(key);
#endif

    /* Initialize kevent structure, DELETE read/write */
    EV_SET(&events[0], key->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&events[1], key->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    /* delete event from kqueue */
    status = os_kqueue_ctl(ioqueue->kfd, events, 2, NULL, 0, NULL);
    if (status == -1) {
        pj_status_t rc = pj_get_os_error();
        pj_lock_release(ioqueue->lock);
        pj_ioqueue_unlock_key(key);
        return rc;
    }

    /* Destroy the key. */
    pj_sock_close(key->fd);

    pj_lock_release(ioqueue->lock);

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    /* Mark key is closing. */
    key->closing = 1;

    /* Decrement counter. */
    decrement_counter(key);

    /* Done. */
    if (key->grp_lock) {
        /* just dec_ref and unlock. we will set grp_lock to NULL
         * elsewhere */
        pj_grp_lock_t *grp_lock = key->grp_lock;
        // Don't set grp_lock to NULL otherwise the other thread
        // will crash. Just leave it as dangling pointer, but this
        // should be safe
        // key->grp_lock = NULL;
        pj_grp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
        pj_grp_lock_release(grp_lock);
    } else {
        pj_ioqueue_unlock_key(key);
    }
#else
    if (key->grp_lock) {
        /* set grp_lock to NULL and unlock */
        pj_grp_lock_t *grp_lock = key->grp_lock;
        // Don't set grp_lock to NULL otherwise the other thread
        // will crash. Just leave it as dangling pointer, but this
        // should be safe
        // key->grp_lock = NULL;
        pj_grp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
        pj_grp_lock_release(grp_lock);
    } else {
        pj_ioqueue_unlock_key(key);
    }

    pj_lock_destroy(key->lock);
#endif

    return PJ_SUCCESS;
}

/* ioqueue_remove_from_set()
 * This function is called from ioqueue_dispatch_event() to instruct
 * the ioqueue to remove the specified descriptor from ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_remove_from_set2(pj_ioqueue_t *ioqueue,
                                     pj_ioqueue_key_t *key,
                                     unsigned event_types)
{
    struct kevent event;
    if (event_types & READABLE_EVENT) {
        /*
        if (!key_has_pending_read(key) && !key_has_pending_accept(key)) {
            EV_SET(&event, key->fd, EVFILT_READ, EV_DISABLE, 0, 0, key);
            os_kqueue_ctl(ioqueue->kfd, &event, 1, NULL, 0, NULL);
        }
        */
    }
    if (event_types & WRITEABLE_EVENT) {
        if (!key_has_pending_write(key) && !key_has_pending_connect(key)) {
            EV_SET(&event, key->fd, EVFILT_WRITE, EV_DISABLE, 0, 0, key);
            os_kqueue_ctl(ioqueue->kfd, &event, 1, NULL, 0, NULL);
        }
    }
}

static void ioqueue_remove_from_set( pj_ioqueue_t *ioqueue,
                                     pj_ioqueue_key_t *key,
                                     enum ioqueue_event_type event_type )
{
    ioqueue_remove_from_set2(ioqueue, key, event_type);
}

/*
 * ioqueue_add_to_set()
 * This function is called from pj_ioqueue_recv(), pj_ioqueue_send() etc
 * to instruct the ioqueue to add the specified handle to ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_add_to_set2(pj_ioqueue_t *ioqueue, pj_ioqueue_key_t *key,
                                unsigned event_types)
{
    struct kevent event;

    if (event_types & READABLE_EVENT) {
        /*
        EV_SET(&event, key->fd, EVFILT_READ, EV_ENABLE, 0, 0, key);
        os_kqueue_ctl(ioqueue->kfd, &event, 1, NULL, 0, NULL);
        */
    }

    if (event_types & WRITEABLE_EVENT) {
        EV_SET(&event, key->fd, EVFILT_WRITE, EV_ENABLE, 0, 0, key);
        os_kqueue_ctl(ioqueue->kfd, &event, 1, NULL, 0, NULL);
    }
}

static void ioqueue_add_to_set( pj_ioqueue_t *ioqueue,
                                pj_ioqueue_key_t *key,
                                enum ioqueue_event_type event_type )
{
    ioqueue_add_to_set2(ioqueue, key, event_type);
}

#if PJ_IOQUEUE_HAS_SAFE_UNREG
/* Scan closing keys to be put to free list again */
static void scan_closing_keys(pj_ioqueue_t *ioqueue)
{
    pj_time_val now;
    pj_ioqueue_key_t *h;

    pj_gettickcount(&now);
    h = ioqueue->closing_list.next;
    while (h != &ioqueue->closing_list) {
        pj_ioqueue_key_t *next = h->next;

        pj_assert(h->closing != 0);

        if (PJ_TIME_VAL_GTE(now, h->free_time)) {
            pj_list_erase(h);
            // Don't set grp_lock to NULL otherwise the other thread
            // will crash. Just leave it as dangling pointer, but this
            // should be safe
            // h->grp_lock = NULL;
            pj_list_push_back(&ioqueue->free_list, h);
        }
        h = next;
    }
}
#endif

/*
 * pj_ioqueue_poll()
 *
 */
PJ_DEF(int) pj_ioqueue_poll(pj_ioqueue_t *ioqueue, const pj_time_val *timeout)
{
    int i, count, event_cnt, processed_cnt;
    struct timespec xtimeout = {1, 0};
    enum
    {
        MAX_EVENTS = PJ_IOQUEUE_MAX_CAND_EVENTS
    };
    struct kevent events[MAX_EVENTS];
    struct queue queue[MAX_EVENTS];
    pj_timestamp t1, t2;

    PJ_CHECK_STACK();

    if (timeout) {
        xtimeout.tv_sec = timeout->sec;
        xtimeout.tv_nsec = timeout->msec * 1000000;
    }

    TRACE_((THIS_FILE, "start kqueue wait, msec=%d",
            xtimeout.tv_sec * 1000 + xtimeout.tv_nsec / 1000000));

    pj_get_timestamp(&t1);
    count =
        os_kqueue_wait(ioqueue->kfd, NULL, 0, events, MAX_EVENTS, &xtimeout);
    pj_get_timestamp(&t2);
    if (count == 0) {
#if PJ_IOQUEUE_HAS_SAFE_UNREG
        /* Check the closing keys only when there's no activity and when there
         * are pending closing keys.
         */
        if (count == 0 && !pj_list_empty(&ioqueue->closing_list)) {
            pj_lock_acquire(ioqueue->lock);
            scan_closing_keys(ioqueue);
            pj_lock_release(ioqueue->lock);
        }
#endif
        TRACE_((THIS_FILE, "kqueue wait timed out"));
        return count;
    } else if (count < 0) {
        TRACE_((THIS_FILE, "kqueue wait error"));
        return -pj_get_netos_error();
    }

    /* Lock ioqueue. */
    pj_lock_acquire(ioqueue->lock);

    for (event_cnt = 0, i = 0; i < count; ++i) {
        pj_ioqueue_key_t *h = (pj_ioqueue_key_t *)events[i].udata;

        TRACE_((THIS_FILE, "event %d: events=%d", i, events[i].filter));

        /*
         * Check readability.
         */
        if ((events[i].filter & EVFILT_READ) &&
            (key_has_pending_read(h) || key_has_pending_accept(h)) &&
            !IS_CLOSING(h)) {

#if PJ_IOQUEUE_HAS_SAFE_UNREG
            increment_counter(h);
#endif
            queue[event_cnt].key = h;
            queue[event_cnt].event_type = READABLE_EVENT;
            ++event_cnt;
            continue;
        }

        /*
         * Check for writeability.
         */
        if ((events[i].filter & EVFILT_WRITE) &&
            (key_has_pending_write(h) || key_has_pending_connect(h)) &&
            !IS_CLOSING(h)) {

#if PJ_IOQUEUE_HAS_SAFE_UNREG
            increment_counter(h);
#endif
            queue[event_cnt].key = h;
            queue[event_cnt].event_type = WRITEABLE_EVENT;
            ++event_cnt;
            continue;
        }
    }

    for (i = 0; i < event_cnt; ++i) {
        if (queue[i].key->grp_lock)
            pj_grp_lock_add_ref_dbg(queue[i].key->grp_lock, "ioqueue", 0);
    }

    pj_lock_release(ioqueue->lock);

    processed_cnt = 0;

    /* Now process the events. */
    for (i = 0; i < event_cnt; ++i) {

        /* Just do not exceed PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL */
        if (processed_cnt < PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL) {
            switch (queue[i].event_type) {
            case READABLE_EVENT:
                if (ioqueue_dispatch_read_event(ioqueue, queue[i].key))
                    ++processed_cnt;
                break;
            case WRITEABLE_EVENT:
                if (ioqueue_dispatch_write_event(ioqueue, queue[i].key))
                    ++processed_cnt;
                break;
            case EXCEPTION_EVENT:
                if (ioqueue_dispatch_exception_event(ioqueue, queue[i].key))
                    ++processed_cnt;
                break;
            case NO_EVENT:
                pj_assert(!"Invalid event!");
                break;
            }
        }
#if PJ_IOQUEUE_HAS_SAFE_UNREG
        decrement_counter(queue[i].key);
#endif

        if (queue[i].key->grp_lock)
            pj_grp_lock_dec_ref_dbg(queue[i].key->grp_lock, "ioqueue", 0);
    }

    if (!event_cnt) {
        /* We need to sleep in order to avoid busy polling.
         * Limit the duration of the sleep, as doing pj_thread_sleep() for
         * a long time is very inefficient. The main objective here is just
         * to avoid busy loop.
         */
        long msec = xtimeout.tv_sec * 1000 + xtimeout.tv_nsec / 1000000;
        long delay = msec - pj_elapsed_usec(&t1, &t2)/1000;
        if (delay > 10) delay = 10;
        if (delay > 0)
            pj_thread_sleep(delay);
    }

    TRACE_((THIS_FILE, "     poll: count=%d events=%d processed=%d", count,
            event_cnt, processed_cnt));

    return processed_cnt;
}

PJ_DEF(pj_oshandle_t) pj_ioqueue_get_os_handle( pj_ioqueue_t *ioqueue )
{
    return ioqueue ? (pj_oshandle_t)&ioqueue->kfd : NULL;
}

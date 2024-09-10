/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
 * ioqueue_epoll.c
 *
 * This is the implementation of IOQueue framework using /dev/epoll
 * API in _both_ Linux user-mode and kernel-mode.
 */

#include <pj/ioqueue.h>
#include <pj/os.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/list.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/sock.h>
#include <pj/compat/socket.h>
#include <pj/rand.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>

#define epoll_data              data.ptr
#define epoll_data_type         void*
#define ioctl_val_type          unsigned long
#define getsockopt_val_ptr      int*
#define os_getsockopt           getsockopt
#define os_ioctl                ioctl
#define os_read                 read
#define os_close                close
#define os_epoll_create         epoll_create1
#define os_epoll_ctl            epoll_ctl
#define os_epoll_wait           epoll_wait


#define THIS_FILE   "ioq_epoll"

//#define TRACE_(expr) PJ_LOG(3,expr)
#define TRACE_(expr)

/* Enable this during development to warn against stray events.
 * But don't enable this during production, for performance reason.
 */
//#define TRACE_WARN(expr)      PJ_LOG(2,expr)
#define TRACE_WARN(expr)


#ifndef EPOLLEXCLUSIVE
#  define EPOLLEXCLUSIVE        (1U << 28)
#endif

enum { IO_MASK = EPOLLIN | EPOLLOUT | EPOLLERR };

static char ioq_name[32];

/*
 * Include common ioqueue abstraction.
 */
#include "ioqueue_common_abs.h"

/*
 * This describes each key.
 */
struct pj_ioqueue_key_t
{
    DECLARE_COMMON_KEY
    struct epoll_event ev;
};

struct queue
{
    pj_ioqueue_key_t        *key;
    enum ioqueue_event_type  event_type;
};

/*
 * This describes the I/O queue.
 */
struct pj_ioqueue_t
{
    DECLARE_COMMON_IOQUEUE

    unsigned            max, count;
    //pj_ioqueue_key_t  hlist;
    pj_ioqueue_key_t    active_list;    
    int                 epfd;
    //struct epoll_event *events;
    //struct queue       *queue;

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    pj_mutex_t         *ref_cnt_mutex;
    pj_ioqueue_key_t    closing_list;
    pj_ioqueue_key_t    free_list;
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


/* EPOLLEXCLUSIVE or EPOLLONESHOT is reported to cause perm handshake error
 * on OpenSSL 1.0.2, so let's disable this when using OpenSSL older than
 * version 1.1.0.
 */
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0 && \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL)

#  include <openssl/opensslv.h>
#  if OPENSSL_VERSION_NUMBER < 0x10100000L
#    define DISABLE_EXCLUSIVE_ONESHOT  "problem with OpenSSL ver <= 1.0.2"
#  endif

#endif


/*
 *  Run-time detection of epoll exclusive/oneshot on the machine.
 */
static unsigned detect_epoll_support()
{
    static int epoll_support = -1;
    struct epoll_event ev;
    int rc;
    int epfd = -1, evfd = -1, disable_exclusive = 0;
    int tmp_epoll_support = 0, support_exclusive = 0, support_oneshot = 0;

    /* Note: this function should be thread-safe */
    if (epoll_support != -1)
        return epoll_support;

#ifdef DISABLE_EXCLUSIVE_ONESHOT
    PJ_LOG(3,(THIS_FILE, "epoll EXCLUSIVE/ONESHOT support disabled, reason: %s",
              DISABLE_EXCLUSIVE_ONESHOT));
    epoll_support = 0;
    return epoll_support;
#endif

    epfd = os_epoll_create(EPOLL_CLOEXEC);
    if (epfd < 0)
        goto on_error;

    evfd = eventfd(0, EFD_CLOEXEC);
    if (evfd < 0)
        goto on_error;

    /*
     * Choose events that should cause an error on EPOLLEXCLUSIVE enabled
     * kernels - specifically the combination of EPOLLONESHOT and
     * EPOLLEXCLUSIVE
     */
    pj_bzero(&ev, sizeof(ev));
    ev.events = EPOLLIN | EPOLLEXCLUSIVE | EPOLLONESHOT;
    rc = epoll_ctl(epfd, EPOLL_CTL_ADD, evfd, &ev);
    if (rc == 0) {
        /* The kernel has accepted our invalid request. Assume it probably
         * doesn't know about/support EPOLLEXCLUSIVE. But we assume that
         * it may still be able to support EPOLLONESHOT, since this was
         * added very long time ago.
         */
        disable_exclusive = 1;
        rc = epoll_ctl(epfd, EPOLL_CTL_DEL, evfd, &ev);
        if (rc != 0)
            goto on_error;

    } else if (errno != EINVAL) {
        /* Unexpected error */
        goto on_error;
    }

    /* Check EPOLLEXCLUSIVE support */
    if (!disable_exclusive) {
        ev.events = EPOLLIN | EPOLLEXCLUSIVE;
        rc = epoll_ctl(epfd, EPOLL_CTL_ADD, evfd, &ev);
        if (rc == 0) {
            support_exclusive = 1;
            rc = epoll_ctl(epfd, EPOLL_CTL_DEL, evfd, &ev);
            if (rc != 0)
                goto on_error;
        }
    }

    /* Check EPOLLONESHOT support */
    ev.events = EPOLLIN | EPOLLONESHOT;
    rc = epoll_ctl(epfd, EPOLL_CTL_ADD, evfd, &ev);
    if (rc == 0) {
        support_oneshot = 1;
        rc = epoll_ctl(epfd, EPOLL_CTL_DEL, evfd, &ev);
        if (rc != 0)
            goto on_error;
    }

    tmp_epoll_support = 0;
    if (support_exclusive && !disable_exclusive)
        tmp_epoll_support |= PJ_IOQUEUE_EPOLL_EXCLUSIVE;
    if (support_oneshot)
        tmp_epoll_support |= PJ_IOQUEUE_EPOLL_ONESHOT;

    pj_ansi_snprintf(ioq_name, sizeof(ioq_name), "epoll[0x%x]",
                     tmp_epoll_support);
    epoll_support = tmp_epoll_support;

    if (epfd > 0)
        os_close(epfd);
    if (evfd > 0)
        os_close(evfd);

    return epoll_support;

on_error:
    rc = PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    PJ_PERROR(2,(THIS_FILE, rc, "detect_epoll_support() error"));
    if (epfd >= 0)
        os_close(epfd);
    if (evfd >= 0)
        os_close(evfd);
    return 0;
}

/*
 * pj_ioqueue_name()
 */
PJ_DEF(const char*) pj_ioqueue_name(void)
{
    detect_epoll_support();
    return ioq_name;
}


/*
 * pj_ioqueue_create()
 *
 * Create epoll ioqueue.
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
 * Create epoll ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_create2(pj_pool_t *pool,
                                       pj_size_t max_fd,
                                       const pj_ioqueue_cfg *cfg,
                                       pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioqueue;
    pj_status_t rc;
    pj_lock_t *lock;
    const unsigned type_mask = PJ_IOQUEUE_EPOLL_EXCLUSIVE |
                               PJ_IOQUEUE_EPOLL_ONESHOT;
    unsigned epoll_support, valid_types;
    pj_size_t i;

    /* Check that arguments are valid. */
    PJ_ASSERT_RETURN(pool != NULL && p_ioqueue != NULL && 
                     max_fd > 0, PJ_EINVAL);

    /* Check that size of pj_ioqueue_op_key_t is sufficient */
    PJ_ASSERT_RETURN(sizeof(pj_ioqueue_op_key_t)-sizeof(void*) >=
                     sizeof(union operation_key), PJ_EBUG);

    ioqueue = pj_pool_alloc(pool, sizeof(pj_ioqueue_t));

    ioqueue_init(ioqueue);

    if (cfg)
        pj_memcpy(&ioqueue->cfg, cfg, sizeof(*cfg));
    else {
        pj_ioqueue_cfg_default(&ioqueue->cfg);
        cfg = &ioqueue->cfg;
    }
    ioqueue->max = max_fd;
    ioqueue->count = 0;
    pj_list_init(&ioqueue->active_list);

    /* Adjust/validate epoll type according to supported epoll types.
     */
    epoll_support = detect_epoll_support();
    valid_types = epoll_support & cfg->epoll_flags;
    /* Note that epoll_flags may be used to specify options other than
     * epoll types in the future, hence be careful when clearing the
     * bits (only bits related to epoll types should be cleared)
     */
    ioqueue->cfg.epoll_flags &= ~type_mask;
    if (valid_types & PJ_IOQUEUE_EPOLL_EXCLUSIVE) {
        ioqueue->cfg.epoll_flags |= PJ_IOQUEUE_EPOLL_EXCLUSIVE;
    } else if (valid_types & PJ_IOQUEUE_EPOLL_ONESHOT) {
        ioqueue->cfg.epoll_flags |= PJ_IOQUEUE_EPOLL_ONESHOT;
    } else if ((cfg->epoll_flags & type_mask) == 0) {
        /* user has disabled both EXCLUSIVE and ONESHOT */
    } else {
        /* the requested epoll type is not available */
        return PJ_EINVAL;
    }

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
    for ( i=0; i<max_fd; ++i) {
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

    ioqueue->epfd = os_epoll_create(EPOLL_CLOEXEC);
    if (ioqueue->epfd < 0) {
        pj_lock_acquire(ioqueue->lock);
        ioqueue_destroy(ioqueue);
        return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
    }

    /*ioqueue->events = pj_pool_calloc(pool, max_fd, sizeof(struct epoll_event));
    PJ_ASSERT_RETURN(ioqueue->events != NULL, PJ_ENOMEM);

    ioqueue->queue = pj_pool_calloc(pool, max_fd, sizeof(struct queue));
    PJ_ASSERT_RETURN(ioqueue->queue != NULL, PJ_ENOMEM);
   */

    PJ_LOG(4, ("pjlib", "epoll I/O Queue created (flags:0x%x, ptr=%p)",
               ioqueue->cfg.epoll_flags, ioqueue));

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
    PJ_ASSERT_RETURN(ioqueue->epfd > 0, PJ_EINVALIDOP);

    pj_lock_acquire(ioqueue->lock);
    os_close(ioqueue->epfd);
    ioqueue->epfd = 0;

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
PJ_DEF(pj_status_t) pj_ioqueue_register_sock2(pj_pool_t *pool,
                                              pj_ioqueue_t *ioqueue,
                                              pj_sock_t sock,
                                              pj_grp_lock_t *grp_lock,
                                              void *user_data,
                                              const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key)
{
    pj_ioqueue_key_t *key = NULL;
    pj_uint32_t value;
    int rc;
    pj_status_t status = PJ_SUCCESS;
    
    PJ_ASSERT_RETURN(pool && ioqueue && sock != PJ_INVALID_SOCKET &&
                     cb && p_key, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

    if (ioqueue->count >= ioqueue->max) {
        status = PJ_ETOOMANY;
        TRACE_((THIS_FILE, "pj_ioqueue_register_sock error: too many files"));
        goto on_return;
    }

    /* Set socket to nonblocking. */
    value = 1;
    if ((rc=os_ioctl(sock, FIONBIO, (ioctl_val_type)&value))) {
        TRACE_((THIS_FILE, "pj_ioqueue_register_sock error: ioctl rc=%d", 
                rc));
        status = pj_get_netos_error();
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
        status = PJ_ETOOMANY;
        goto on_return;
    }

    key = ioqueue->free_list.next;
    pj_list_erase(key);
#else
    /* Create key. */
    key = (pj_ioqueue_key_t*)pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));
#endif

    status = ioqueue_init_key(pool, ioqueue, key, sock, grp_lock, user_data, cb);
    if (status != PJ_SUCCESS) {
        key = NULL;
        goto on_return;
    }
    pj_bzero(&key->ev, sizeof(key->ev));
    key->ev.epoll_data = (epoll_data_type)key;
    key->ev.events = 0;
    if (ioqueue->cfg.epoll_flags & PJ_IOQUEUE_EPOLL_EXCLUSIVE)
        key->ev.events |= EPOLLEXCLUSIVE;
    else if (ioqueue->cfg.epoll_flags & PJ_IOQUEUE_EPOLL_ONESHOT)
        key->ev.events |= EPOLLONESHOT;

    /* Create key's mutex */
 /*   rc = pj_mutex_create_recursive(pool, NULL, &key->mutex);
    if (rc != PJ_SUCCESS) {
        key = NULL;
        goto on_return;
    }
*/
    /* os_epoll_ctl. */
    rc = os_epoll_ctl(ioqueue->epfd, EPOLL_CTL_ADD, sock, &key->ev);
    if (rc < 0) {
        status = pj_get_os_error();
        pj_lock_destroy(key->lock);
        key = NULL;
        PJ_PERROR(1,(THIS_FILE, status, "epol_ctl(ADD) error"));
        goto on_return;
    }
    
    /* Register */
    pj_list_insert_before(&ioqueue->active_list, key);
    ++ioqueue->count;

    //TRACE_((THIS_FILE, "socket registered, count=%d", ioqueue->count));

on_return:
    if (status != PJ_SUCCESS) {
        if (key && key->grp_lock)
            pj_grp_lock_dec_ref_dbg(key->grp_lock, "ioqueue", 0);
    }
    *p_key = key;
    pj_lock_release(ioqueue->lock);
    
    return status;
}

PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
                                              pj_ioqueue_t *ioqueue,
                                              pj_sock_t sock,
                                              void *user_data,
                                              const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key)
{
    return pj_ioqueue_register_sock2(pool, ioqueue, sock, NULL, user_data,
                                     cb, p_key);
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
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key)
{
    pj_ioqueue_t *ioqueue;
    int status;
    
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
        PJ_LOG(1,(THIS_FILE, "Bad ioqueue count in key unregistration!"));
    }

#if !PJ_IOQUEUE_HAS_SAFE_UNREG
    pj_list_erase(key);
#endif

    /* Note: although event argument is ignored on EPOLL_CTL_DEL, we still
     * need to clear the IO flags to be safe (in case another thread is run
     * after we exit this function).
     */
    key->ev.events &= ~IO_MASK;
    status = os_epoll_ctl( ioqueue->epfd, EPOLL_CTL_DEL, key->fd, &key->ev);
    if (status != 0) {
        status = pj_get_os_error();
        PJ_PERROR(2, (THIS_FILE, status,
                      "Ignoring pj_ioqueue_unregister error: os_epoll_ctl"));
        /* From epoll doc: "Closing a file descriptor cause it to be
         * removed from all epoll interest lists". So we should just
         * proceed instead of returning failure here.
         */
        // pj_lock_release(ioqueue->lock);
        // pj_ioqueue_unlock_key(key);
        // return rc;
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
        //key->grp_lock = NULL;
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
        //key->grp_lock = NULL;
        pj_grp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
        pj_grp_lock_release(grp_lock);
    } else {
        pj_ioqueue_unlock_key(key);
    }

    pj_lock_destroy(key->lock);
#endif

    return PJ_SUCCESS;
}


static void update_epoll_event_set(pj_ioqueue_t *ioqueue,
                                   pj_ioqueue_key_t *key,
                                   pj_uint32_t events)
{
    int rc;
    /* From epoll_ctl(2):
     * EPOLLEXCLUSIVE may be used only in an EPOLL_CTL_ADD operation;
     * attempts to employ it with EPOLL_CTL_MOD yield an error.
     */
    if (key->ev.events & EPOLLEXCLUSIVE) {
        rc = os_epoll_ctl(ioqueue->epfd, EPOLL_CTL_DEL, key->fd, &key->ev);
        key->ev.events = events;
        rc = os_epoll_ctl(ioqueue->epfd, EPOLL_CTL_ADD, key->fd, &key->ev);
    } else {
        key->ev.events = events;
        rc = os_epoll_ctl(ioqueue->epfd, EPOLL_CTL_MOD, key->fd, &key->ev);
    }

    if (rc != 0) {
        pj_status_t status = pj_get_os_error();
        PJ_PERROR(1,(THIS_FILE, status,
                     "epol_ctl(MOD) error (events=0x%x)", events));
    }
}


/* ioqueue_remove_from_set()
 * This function is called from ioqueue_dispatch_event() to instruct
 * the ioqueue to remove the specified descriptor from ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_remove_from_set( pj_ioqueue_t *ioqueue,
                                     pj_ioqueue_key_t *key,
                                     enum ioqueue_event_type event_type )
{
    ioqueue_remove_from_set2(ioqueue, key, event_type);
}

static void ioqueue_remove_from_set2(pj_ioqueue_t *ioqueue,
                                     pj_ioqueue_key_t *key, 
                                     unsigned event_types)
{
    pj_uint32_t events = key->ev.events;

    if (event_types & READABLE_EVENT)
        events &= ~EPOLLIN;
    if (event_types & WRITEABLE_EVENT)
        events &= ~EPOLLOUT;

    /* Note that although EPOLLERR is removed, epoll will still report
     * EPOLLERR events to us and there is no way to disable it. But we
     * still remove it from "events" anyway to make our interest correct
     * in our own record.
     */
    if (event_types & EXCEPTION_EVENT)
        events &= ~EPOLLERR;

    if (events != key->ev.events)
        update_epoll_event_set(ioqueue, key, events);
}

/*
 * ioqueue_add_to_set()
 * This function is called from pj_ioqueue_recv(), pj_ioqueue_send() etc
 * to instruct the ioqueue to add the specified handle to ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_add_to_set( pj_ioqueue_t *ioqueue,
                                pj_ioqueue_key_t *key,
                                enum ioqueue_event_type event_type )
{
    ioqueue_add_to_set2(ioqueue, key, event_type);
}

static void ioqueue_add_to_set2(pj_ioqueue_t *ioqueue,
                                pj_ioqueue_key_t *key,
                                unsigned event_types )
{
    pj_uint32_t events = key->ev.events;

    if (event_types & READABLE_EVENT)
        events |= EPOLLIN;
    if (event_types & WRITEABLE_EVENT)
        events |= EPOLLOUT;
    if (event_types & EXCEPTION_EVENT)
        events |= EPOLLERR;

    if (events != key->ev.events)
        update_epoll_event_set(ioqueue, key, events);
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
            //h->grp_lock = NULL;
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
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioqueue, const pj_time_val *timeout)
{
    int i, count, event_cnt, processed_cnt;
    int msec;
    //struct epoll_event *events = ioqueue->events;
    //struct queue *queue = ioqueue->queue;
    enum { MAX_EVENTS = PJ_IOQUEUE_MAX_CAND_EVENTS };
    struct epoll_event events[MAX_EVENTS];
    struct queue queue[MAX_EVENTS];
    pj_timestamp t1, t2;
    
    PJ_CHECK_STACK();

    msec = timeout ? PJ_TIME_VAL_MSEC(*timeout) : 9000;

    TRACE_((THIS_FILE, "start os_epoll_wait, msec=%d", msec));
    pj_get_timestamp(&t1);
 
    //count = os_epoll_wait( ioqueue->epfd, events, ioqueue->max, msec);
    count = os_epoll_wait( ioqueue->epfd, events, MAX_EVENTS, msec);
    if (count == 0) {
#if PJ_IOQUEUE_HAS_SAFE_UNREG
    /* Check the closing keys only when there's no activity and when there are
     * pending closing keys.
     */
    if (count == 0 && !pj_list_empty(&ioqueue->closing_list)) {
        pj_lock_acquire(ioqueue->lock);
        scan_closing_keys(ioqueue);
        pj_lock_release(ioqueue->lock);
    }
#endif
        TRACE_((THIS_FILE, "  os_epoll_wait timed out"));
        return count;
    }
    else if (count < 0) {
        TRACE_((THIS_FILE, "  os_epoll_wait error"));
        return -pj_get_netos_error();
    }

    pj_get_timestamp(&t2);
    TRACE_((THIS_FILE, "  os_epoll_wait returns %d, time=%d usec",
                       count, pj_elapsed_usec(&t1, &t2)));

    /* Lock ioqueue. */
    pj_lock_acquire(ioqueue->lock);

    for (event_cnt=0, i=0; i<count; ++i) {
        pj_ioqueue_key_t *h = (pj_ioqueue_key_t*)(epoll_data_type)
                                events[i].epoll_data;

        TRACE_((THIS_FILE, "     event %d: events=%x", i, events[i].events));

        /*
         * Check readability.
         */
        if ((events[i].events & EPOLLIN) && 
            (key_has_pending_read(h) || key_has_pending_accept(h)) && !IS_CLOSING(h) ) {

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
        if ((events[i].events & EPOLLOUT) && key_has_pending_write(h) && !IS_CLOSING(h)) {

#if PJ_IOQUEUE_HAS_SAFE_UNREG
            increment_counter(h);
#endif
            queue[event_cnt].key = h;
            queue[event_cnt].event_type = WRITEABLE_EVENT;
            ++event_cnt;
            continue;
        }

#if PJ_HAS_TCP
        /*
         * Check for completion of connect() operation.
         */
        if ((events[i].events & EPOLLOUT) && (h->connecting) && !IS_CLOSING(h)) {

#if PJ_IOQUEUE_HAS_SAFE_UNREG
            increment_counter(h);
#endif
            queue[event_cnt].key = h;
            queue[event_cnt].event_type = WRITEABLE_EVENT;
            ++event_cnt;
            continue;
        }
#endif /* PJ_HAS_TCP */

        /*
         * Check for error condition.
         */
        if ((events[i].events & EPOLLERR) && !IS_CLOSING(h)) {
            /*
             * We need to handle this exception event.  If it's related to us
             * connecting, report it as such.  If not, just report it as a
             * read event and the higher layers will handle it.
             */
            if (h->connecting) {
#if PJ_IOQUEUE_HAS_SAFE_UNREG
                increment_counter(h);
#endif
                queue[event_cnt].key = h;
                queue[event_cnt].event_type = EXCEPTION_EVENT;
                ++event_cnt;
            } else if (key_has_pending_read(h) || key_has_pending_accept(h)) {
#if PJ_IOQUEUE_HAS_SAFE_UNREG
                increment_counter(h);
#endif
                queue[event_cnt].key = h;
                queue[event_cnt].event_type = READABLE_EVENT;
                ++event_cnt;
            }
            continue;
        }

        if (ioqueue->cfg.epoll_flags & PJ_IOQUEUE_EPOLL_ONESHOT) {
            /* We are not processing this event, but we still need to rearm
             * to receive future events.
             */
            if (!IS_CLOSING(h))
                update_epoll_event_set(ioqueue, h, h->ev.events);
        }

        /* There are some innocent cases where this might happen:
         * 1. TCP outgoing connection failure. Although app has handled the
         *    failure event, epoll will keep reporting EPOLLHUP until the
         *    socket is unregistered.
         * 2. Thread A and B are woken up for a single EPOLLIN, (or when
         *    EPOLLEXCLUSIVE is used, two packets arrives almost simultaneously,
         *    only single pending recv() was submitted. This also causes both
         *    threads to be woken up. This scenario can be observed in ioq_reg.c
         *    test)
         *    - Thread A is processing the event
         *    - Thread B waits for ioqueue lock
         *    - Thread A releases the ioqueue lock, acquires key lock, remove
         *      pending recv() from list, release key lock (because concurrency
         *      is enabled).
         *    - Thread B resumes, it finds no pending recv() op.
         * 3. Other scenarios involving TCP in normal operation using plain or
         *    EPOLLEXCLUSIVE (didn't happen with ONESHOT), for both EPOLLIN
         *    and EPOLLOUT events. Not sure what, but it happens only
         *    sporadically, so probably it's fine. This is reproducible with
         *    "tcp (multithreads)" test in ioq_stress_test.c.
         */
        TRACE_WARN((THIS_FILE, "     UNHANDLED event %d: events=0x%x, h=%p",
                    i, events[i].events, h));
    }
    for (i=0; i<event_cnt; ++i) {
        if (queue[i].key->grp_lock)
            pj_grp_lock_add_ref_dbg(queue[i].key->grp_lock, "ioqueue", 0);
    }

    PJ_RACE_ME(5);

    pj_lock_release(ioqueue->lock);

    PJ_RACE_ME(5);

    processed_cnt = 0;

    /* Now process the events. */
    for (i=0; i<event_cnt; ++i) {
        /* Just do not exceed PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL */
        if (processed_cnt < PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL) {
            pj_bool_t event_done = PJ_FALSE;
            switch (queue[i].event_type) {
            case READABLE_EVENT:
                event_done = ioqueue_dispatch_read_event(ioqueue,queue[i].key);

                break;
            case WRITEABLE_EVENT:
                event_done = ioqueue_dispatch_write_event(ioqueue,
                                                          queue[i].key);

                break;
            case EXCEPTION_EVENT:
                event_done = ioqueue_dispatch_exception_event(ioqueue,
                                                              queue[i].key);
                break;
            case NO_EVENT:
                pj_assert(!"Invalid event!");
                break;
            }
            if (event_done) {
                ++processed_cnt;
            }
        }

        /* Re-arm ONESHOT as long as there are pending requests. This is
         * necessary to deal with this case:
         * - thread A and B are calling ioqueue_recv()
         * - packet arrives, thread A is processing
         * - but thread A doesn't call ioqueue_recv() again
         * - if we don't rearm here, thread B will never get the event.
         *
         * On the other hand, if thread A calls ioqueue_recv() again above,
         * this will result in double epoll_ctl() calls. This should be okay,
         * albeit inefficient. We err on the safe side.
         */
        if ((ioqueue->cfg.epoll_flags & PJ_IOQUEUE_EPOLL_ONESHOT) &&
            (queue[i].key->ev.events & IO_MASK))
        {
            pj_ioqueue_lock_key(queue[i].key);
            update_epoll_event_set(ioqueue, queue[i].key,
                                   queue[i].key->ev.events);
            pj_ioqueue_unlock_key(queue[i].key);
        }

#if PJ_IOQUEUE_HAS_SAFE_UNREG
        decrement_counter(queue[i].key);
#endif

        if (queue[i].key->grp_lock)
            pj_grp_lock_dec_ref_dbg(queue[i].key->grp_lock,
                                    "ioqueue", 0);
    }

    /* Special case:
     * When epoll returns > 0 but event_cnt, the number of events
     * we want to process, is zero.
     * There are several possibilities this can happen, see "UNHANDLED event"
     * log message above.
     */
    if (count > 0 && !event_cnt && msec > 0) {
        /* We need to sleep in order to avoid busy polling, such
         * as in the case of the thread that doesn't process
         * the event as explained above.
         * Limit the duration of the sleep, as doing pj_thread_sleep() for
         * a long time is very inefficient. The main objective here is just
         * to avoid busy loop.
         */
        int delay = msec - pj_elapsed_usec(&t1, &t2)/1000;
        if (delay > 10) delay = 10;
        if (delay > 0)
            pj_thread_sleep(delay);
    }

    TRACE_((THIS_FILE, "     poll: count=%d events=%d processed=%d",
                       count, event_cnt, processed_cnt));

    pj_get_timestamp(&t1);
    TRACE_((THIS_FILE, "ioqueue_poll() returns %d, time=%d usec",
                       processed_cnt, pj_elapsed_usec(&t2, &t1)));

    return processed_cnt;
}

PJ_DEF(pj_oshandle_t) pj_ioqueue_get_os_handle( pj_ioqueue_t *ioqueue )
{
    return ioqueue ? (pj_oshandle_t)&ioqueue->epfd : NULL;
}

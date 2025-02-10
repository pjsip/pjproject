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
#include <pj/ioqueue.h>
#include <pj/os.h>
#include <pj/lock.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/sock.h>
#include <pj/array.h>
#include <pj/log.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/compat/socket.h>

#define THIS_FILE "ioq_winnt"

/* Only build when the backend is Windows I/O Completion Ports. */
#if PJ_IOQUEUE_IMP == PJ_IOQUEUE_IMP_IOCP


#define THIS_FILE "ioq_winnt"

#if defined(PJ_HAS_WINSOCK2_H) && PJ_HAS_WINSOCK2_H != 0
#  include <winsock2.h>
#elif defined(PJ_HAS_WINSOCK_H) && PJ_HAS_WINSOCK_H != 0
#  include <winsock.h>
#endif

#if defined(PJ_HAS_MSWSOCK_H) && PJ_HAS_MSWSOCK_H != 0
#  include <mswsock.h>
#endif

/* For GetAcceptExSockaddrs() on MSVC2005 */
#pragma comment(lib, "mswsock.lib")

#if 0
#  define TRACE(args) PJ_LOG(3,args)
#else
#  define TRACE(args)
#endif


/* The address specified in AcceptEx() must be 16 more than the size of
 * SOCKADDR (source: MSDN).
 */
#define ACCEPT_ADDR_LEN     (sizeof(pj_sockaddr_in)+16)


/* Timeout for cancelling pending operations in ioqueue destroy.
 * Upon ioqueue destroy, all keys must be unregistered and all pending
 * operations must be cancelled. As cancelling ops is asynchronous,
 * IOCP destroy may need to wait for the maximum time specified here.
 */
#define TIMEOUT_CANCEL_OP   5000


typedef struct generic_overlapped
{
    WSAOVERLAPPED          overlapped;
    pj_ioqueue_operation_e operation;
} generic_overlapped;

/*
 * OVERLAPPPED structure for send and receive.
 */
typedef struct ioqueue_overlapped
{
    WSAOVERLAPPED          overlapped;
    pj_ioqueue_operation_e operation;
    WSABUF                 wsabuf;
    pj_sockaddr_in         dummy_addr;
    int                    dummy_addrlen;
} ioqueue_overlapped;

#if PJ_HAS_TCP
/*
 * OVERLAP structure for accept.
 */
typedef struct ioqueue_accept_rec
{
    WSAOVERLAPPED           overlapped;
    pj_ioqueue_operation_e  operation;
    pj_sock_t               newsock;
    pj_sock_t              *newsock_ptr;
    int                    *addrlen;
    void                   *remote;
    void                   *local;
    char                    accept_buf[2 * ACCEPT_ADDR_LEN];
} ioqueue_accept_rec;
#endif

/*
 * Structure to hold operation key.
 */
union operation_key
{
    generic_overlapped      generic;
    ioqueue_overlapped      overlapped;
#if PJ_HAS_TCP
    ioqueue_accept_rec      accept;
#endif
};

/*
 * Pending operation.
 * As cancellation of IOCP operation is asynchronous, we cannot use the
 * operation key provided by app (pj_ioqueue_op_key_t.internal__).
 */
struct pending_op
{
    PJ_DECL_LIST_MEMBER(struct pending_op);
    union operation_key pending_key;
    pj_ioqueue_op_key_t *app_op_key;
};

/* Type of handle in the key. */
enum handle_type
{
    HND_IS_UNKNOWN,
    HND_IS_FILE,
    HND_IS_SOCKET,
};

enum { POST_QUIT_LEN = 0xFFFFDEADUL };

/*
 * Structure for individual socket.
 */
struct pj_ioqueue_key_t
{
    PJ_DECL_LIST_MEMBER(struct pj_ioqueue_key_t);

    pj_pool_t          *pool;
    pj_ioqueue_t       *ioqueue;
    HANDLE              hnd;
    void               *user_data;
    enum handle_type    hnd_type;
    pj_ioqueue_callback cb;
    pj_bool_t           allow_concurrent;
    pj_grp_lock_t      *grp_lock;

#if PJ_HAS_TCP
    int                 connecting;
#endif

    pj_bool_t           closing;
    struct pending_op   pending_list;
    struct pending_op   free_pending_list;
};

/*
 * IO Queue structure.
 */
struct pj_ioqueue_t
{
    pj_pool_t        *pool;
    pj_ioqueue_cfg    cfg;
    HANDLE            iocp;
    pj_lock_t        *lock;
    pj_bool_t         auto_delete_lock;
    pj_bool_t         default_concurrency;
    pj_size_t         max_fd;

    pj_ioqueue_key_t  active_list;
    pj_ioqueue_key_t  free_list;

    /* These are to keep track of connecting sockets */
#if PJ_HAS_TCP
    unsigned          event_count;
    HANDLE            event_pool[MAXIMUM_WAIT_OBJECTS+1];
    unsigned          connecting_count;
    HANDLE            connecting_handles[MAXIMUM_WAIT_OBJECTS+1];
    pj_ioqueue_key_t *connecting_keys[MAXIMUM_WAIT_OBJECTS+1];
#endif
};


/* Dynamic resolution of CancelIoEx().
 * (because older SDKs do not have CancelIoEx()?)
 */
typedef BOOL(WINAPI* FnCancelIoEx)(HANDLE hFile, LPOVERLAPPED lpOverlapped);
static FnCancelIoEx fnCancelIoEx = NULL;

#define OPKEY_OPERATION(op_key) ((union operation_key*)op_key)->generic.operation

/* Prototypes of internal functions */
static void key_on_destroy(void* data);
static void increment_counter(pj_ioqueue_key_t* key);
static void decrement_counter(pj_ioqueue_key_t* key);


#define PENDING_OP_POS(op_key) (PJ_ARRAY_SIZE(op_key->internal__) - 1)

static struct pending_op* get_pending_op(pj_ioqueue_op_key_t *op_key)
{
    return (struct pending_op*)
           (op_key->internal__[PENDING_OP_POS(op_key)]);
}


#if PJ_HAS_TCP
/*
 * Process the socket when the overlapped accept() completed.
 */
static void ioqueue_on_accept_complete(pj_ioqueue_key_t *key,
                                       ioqueue_accept_rec *accept_overlapped)
{
    struct sockaddr *local;
    struct sockaddr *remote;
    int locallen, remotelen;
    pj_status_t status;

    PJ_CHECK_STACK();

    /* On WinXP or later, use SO_UPDATE_ACCEPT_CONTEXT so that socket 
     * addresses can be obtained with getsockname() and getpeername().
     */
    status = setsockopt(accept_overlapped->newsock, SOL_SOCKET,
                        SO_UPDATE_ACCEPT_CONTEXT, 
                        (char*)&key->hnd, 
                        sizeof(SOCKET));
    (void)status;
    /* SO_UPDATE_ACCEPT_CONTEXT is for WinXP or later.
     * So ignore the error status.
     */

    /* Operation complete immediately. */
    if (accept_overlapped->addrlen) {
        GetAcceptExSockaddrs( accept_overlapped->accept_buf,
                              0, 
                              ACCEPT_ADDR_LEN,
                              ACCEPT_ADDR_LEN,
                              &local,
                              &locallen,
                              &remote,
                              &remotelen);
        if (*accept_overlapped->addrlen >= locallen) {
            if (accept_overlapped->local)
                pj_memcpy(accept_overlapped->local, local, locallen);
            if (accept_overlapped->remote)
                pj_memcpy(accept_overlapped->remote, remote, locallen);
        } else {
            if (accept_overlapped->local)
                pj_bzero(accept_overlapped->local, 
                         *accept_overlapped->addrlen);
            if (accept_overlapped->remote)
                pj_bzero(accept_overlapped->remote, 
                         *accept_overlapped->addrlen);
        }

        *accept_overlapped->addrlen = locallen;
    }
    if (accept_overlapped->newsock_ptr)
        *accept_overlapped->newsock_ptr = accept_overlapped->newsock;
    accept_overlapped->operation = 0;
}

static void erase_connecting_socket( pj_ioqueue_t *ioqueue, unsigned pos)
{
    pj_ioqueue_key_t *key = ioqueue->connecting_keys[pos];
    HANDLE hEvent = ioqueue->connecting_handles[pos];

    /* Remove key from array of connecting handles. */
    pj_array_erase(ioqueue->connecting_keys, sizeof(key),
                   ioqueue->connecting_count, pos);
    pj_array_erase(ioqueue->connecting_handles, sizeof(HANDLE),
                   ioqueue->connecting_count, pos);
    --ioqueue->connecting_count;

    /* Disassociate the socket from the event. */
    WSAEventSelect((pj_sock_t)key->hnd, hEvent, 0);

    /* Put event object to pool. */
    if (ioqueue->event_count < MAXIMUM_WAIT_OBJECTS) {
        ioqueue->event_pool[ioqueue->event_count++] = hEvent;
    } else {
        /* Shouldn't happen. There should be no more pending connections
         * than max. 
         */
        pj_assert(0);
        CloseHandle(hEvent);
    }

}

/*
 * Poll for the completion of non-blocking connect().
 * If there's a completion, the function return the key of the completed
 * socket, and 'result' argument contains the connect() result. If connect()
 * succeeded, 'result' will have value zero, otherwise will have the error
 * code.
 */
static int check_connecting( pj_ioqueue_t *ioqueue )
{
    if (ioqueue->connecting_count) {
        int i, count;
        struct 
        {
            pj_ioqueue_key_t *key;
            pj_status_t       status;
        } events[PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL-1];

        pj_lock_acquire(ioqueue->lock);
        for (count=0; count<PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL-1; ++count) {
            DWORD result;

            result = WaitForMultipleObjects(ioqueue->connecting_count,
                                            ioqueue->connecting_handles,
                                            FALSE, 0);
            if (result >= WAIT_OBJECT_0 && 
                result < WAIT_OBJECT_0+ioqueue->connecting_count) 
            {
                WSANETWORKEVENTS net_events;

                /* Got completed connect(). */
                unsigned pos = result - WAIT_OBJECT_0;
                events[count].key = ioqueue->connecting_keys[pos];

                /* See whether connect has succeeded. */
                WSAEnumNetworkEvents((pj_sock_t)events[count].key->hnd, 
                                     ioqueue->connecting_handles[pos], 
                                     &net_events);
                events[count].status = 
                    PJ_STATUS_FROM_OS(net_events.iErrorCode[FD_CONNECT_BIT]);

                /* Erase socket from pending connect. */
                erase_connecting_socket(ioqueue, pos);
            } else {
                /* No more events */
                break;
            }
        }
        pj_lock_release(ioqueue->lock);

        /* Call callbacks. */
        for (i=0; i<count; ++i) {
            if (events[i].key->cb.on_connect_complete) {
                events[i].key->cb.on_connect_complete(events[i].key, 
                                                      events[i].status);
            }
        }

        return count;
    }

    return 0;
    
}
#endif

/*
 * pj_ioqueue_name()
 */
PJ_DEF(const char*) pj_ioqueue_name(void)
{
    return "iocp";
}

PJ_DEF(void) pj_ioqueue_cfg_default(pj_ioqueue_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->epoll_flags = PJ_IOQUEUE_DEFAULT_EPOLL_FLAGS;
    cfg->default_concurrency = PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY;
}

PJ_DEF(pj_status_t) pj_ioqueue_clear_key( pj_ioqueue_key_t *key )
{
    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    pj_ioqueue_lock_key(key);

    key->connecting = 0;

    pj_ioqueue_unlock_key(key);

    return PJ_SUCCESS;

}

/*
 * pj_ioqueue_create()
 */
PJ_DEF(pj_status_t) pj_ioqueue_create( pj_pool_t *pool,
                                       pj_size_t max_fd,
                                       pj_ioqueue_t **p_ioqueue)
{
    return pj_ioqueue_create2(pool, max_fd, NULL, p_ioqueue);
}

/*
 * pj_ioqueue_create2()
 */
PJ_DEF(pj_status_t) pj_ioqueue_create2(pj_pool_t *pool,
                                       pj_size_t max_fd,
                                       const pj_ioqueue_cfg *cfg,
                                       pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioqueue;
    pj_size_t i;
    pj_status_t rc;

    PJ_UNUSED_ARG(max_fd);
    PJ_ASSERT_RETURN(pool && p_ioqueue, PJ_EINVAL);

    rc = sizeof(union operation_key);

    /* Check that sizeof(pj_ioqueue_op_key_t) makes sense.
     * IOCP operations require some buffers (WSAOVERLAPPED, etc) which is
     * represented by operation_key. The pj_ioqueue_op_key_t also holds
     * three important pointers: activesock_data, user_data, and
     * app supplied op-key (at .internal__[31]), so pj_ioqueue_op_key_t size
     * must cover all above.
     */
    PJ_ASSERT_RETURN(sizeof(pj_ioqueue_op_key_t)-3*sizeof(void*) >= 
                     sizeof(union operation_key), PJ_EBUG);

    if (!fnCancelIoEx) {
        fnCancelIoEx = (FnCancelIoEx)
                       GetProcAddress(GetModuleHandle(PJ_T("Kernel32.dll")),
                                      "CancelIoEx");
        if (!fnCancelIoEx) {
            rc = PJ_RETURN_OS_ERROR(GetLastError());
            PJ_PERROR(1, (THIS_FILE, rc,
                          "Failed in getting address of CancelIoEx()"));
            return rc;
        }
    }

    /* Create IOCP */
    ioqueue = pj_pool_zalloc(pool, sizeof(*ioqueue));
    ioqueue->pool = pool;
    if (cfg)
        pj_memcpy(&ioqueue->cfg, cfg, sizeof(*cfg));
    else
        pj_ioqueue_cfg_default(&ioqueue->cfg);

    ioqueue->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (ioqueue->iocp == NULL)
        return PJ_RETURN_OS_ERROR(GetLastError());

    /* Create IOCP mutex */
    rc = pj_lock_create_recursive_mutex(pool, NULL, &ioqueue->lock);
    if (rc != PJ_SUCCESS) {
        CloseHandle(ioqueue->iocp);
        return rc;
    }

    ioqueue->auto_delete_lock = PJ_TRUE;
    ioqueue->default_concurrency = PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY;

    /*
     * Create and initialize key pools.
     */
    pj_list_init(&ioqueue->active_list);
    pj_list_init(&ioqueue->free_list);

    /* Preallocate keys according to max_fd setting, and put them
     * in free_list.
     */
    for (i=0; i<max_fd; ++i) {
        pj_ioqueue_key_t *key;

        key = pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));

        /* Initialize pending op lists */
        pj_list_init(&key->pending_list);
        pj_list_init(&key->free_pending_list);

        pj_list_push_back(&ioqueue->free_list, key);
    }
    ioqueue->max_fd = max_fd;

    *p_ioqueue = ioqueue;

    PJ_LOG(4, (THIS_FILE, "WinNT IOCP I/O Queue created (%p)", ioqueue));
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_destroy()
 */
PJ_DEF(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioqueue )
{
#if PJ_HAS_TCP
    unsigned i;
#endif
    pj_ioqueue_key_t *key, *next;
    pj_time_val stop;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(ioqueue, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

#if PJ_HAS_TCP
    /* Destroy events in the pool */
    for (i=0; i<ioqueue->event_count; ++i) {
        CloseHandle(ioqueue->event_pool[i]);
    }
    ioqueue->event_count = 0;
#endif

    /* Destroy active keys */
    key = ioqueue->active_list.next;
    while (key != &ioqueue->active_list) {
        next = key->next;
        pj_ioqueue_unregister(key);
        key = next;
    }

    pj_lock_release(ioqueue->lock);

    /* Wait cancelling pending ops. */
    pj_gettickcount(&stop);
    stop.msec += TIMEOUT_CANCEL_OP;
    pj_time_val_normalize(&stop);

    while (1) {
        pj_time_val timeout = {0, 100};
        pj_size_t pending_key_cnt;

        pending_key_cnt = ioqueue->max_fd - pj_list_size(&ioqueue->free_list);
        if (!pending_key_cnt)
            break;

        pj_ioqueue_poll(ioqueue, &timeout);

        pj_gettickcount(&timeout);
        if (PJ_TIME_VAL_GTE(timeout, stop)) {
            PJ_LOG(3, (THIS_FILE, "Warning, IOCP destroy timeout in waiting "
                       "for cancelling ops, after %dms, pending keys=%d",
                       TIMEOUT_CANCEL_OP, (int)pending_key_cnt));
            break;
        }
    }

    if (CloseHandle(ioqueue->iocp) != TRUE)
        return PJ_RETURN_OS_ERROR(GetLastError());

    if (ioqueue->auto_delete_lock)
        pj_lock_destroy(ioqueue->lock);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ioqueue_set_default_concurrency(pj_ioqueue_t *ioqueue,
                                                       pj_bool_t allow)
{
    PJ_ASSERT_RETURN(ioqueue != NULL, PJ_EINVAL);
    ioqueue->default_concurrency = allow;
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_set_lock()
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_lock( pj_ioqueue_t *ioqueue, 
                                         pj_lock_t *lock,
                                         pj_bool_t auto_delete )
{
    PJ_ASSERT_RETURN(ioqueue && lock, PJ_EINVAL);

    if (ioqueue->auto_delete_lock) {
        pj_lock_destroy(ioqueue->lock);
    }

    ioqueue->lock = lock;
    ioqueue->auto_delete_lock = auto_delete;

    return PJ_SUCCESS;
}


/*
 * pj_ioqueue_register_sock2()
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock2(pj_pool_t *pool,
                                              pj_ioqueue_t *ioqueue,
                                              pj_sock_t sock,
                                              pj_grp_lock_t *grp_lock,
                                              void *user_data,
                                              const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **key )
{
    HANDLE hioq;
    pj_ioqueue_key_t *rec;
    u_long value;
    int rc;

    PJ_ASSERT_RETURN(pool && ioqueue && cb && key, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

    /* Verify that there is a free key */
    if (pj_list_empty(&ioqueue->free_list)) {
        pj_lock_release(ioqueue->lock);
        return PJ_ETOOMANY;
    }

    /* Get the key record from the free list. */
    rec = ioqueue->free_list.next;

    /* Create pool for this key */
    rec->pool = pj_pool_create(ioqueue->pool->factory, "key%p",
                               512, 512, NULL);
    if (!rec->pool) {
        pj_lock_release(ioqueue->lock);
        return PJ_ENOMEM;
    }

    /* Move key from free list to active list */
    pj_list_erase(rec);
    pj_list_push_back(&ioqueue->active_list, rec);

    pj_lock_release(ioqueue->lock);


    rec->closing = 0;
    
    /* Build the key for this socket. */
    rec->ioqueue = ioqueue;
    rec->hnd = (HANDLE)sock;
    rec->hnd_type = HND_IS_SOCKET;
    rec->user_data = user_data;
    pj_memcpy(&rec->cb, cb, sizeof(pj_ioqueue_callback));

    /* Set concurrency for this handle */
    rc = pj_ioqueue_set_concurrency(rec, ioqueue->default_concurrency);
    if (rc != PJ_SUCCESS)
        return rc;

#if PJ_HAS_TCP
    rec->connecting = 0;
#endif

    /* Set socket to nonblocking. */
    value = 1;
    rc = ioctlsocket(sock, FIONBIO, &value);
    if (rc != 0)
        return PJ_RETURN_OS_ERROR(WSAGetLastError());

    /* Associate with IOCP */
    hioq = CreateIoCompletionPort((HANDLE)sock, ioqueue->iocp,
                                  (ULONG_PTR)rec, 0);
    if (!hioq)
        return PJ_RETURN_OS_ERROR(GetLastError());

    /* Create group lock if not specified */
    if (!grp_lock) {
        pj_status_t status;
        status = pj_grp_lock_create_w_handler(rec->pool, NULL, rec,
                                              &key_on_destroy, &grp_lock);
        if (status != PJ_SUCCESS) {
            key_on_destroy(rec);
            return status;
        }
    }
    rec->grp_lock = grp_lock;

    /* Set initial reference count to 1 */
    increment_counter(rec);

    TRACE((THIS_FILE, "REG key %p", rec));

    /* Finally */
    *key = rec;
    return PJ_SUCCESS;
}


/*
 * pj_ioqueue_register_sock()
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
                                              pj_ioqueue_t *ioqueue,
                                              pj_sock_t sock,
                                              void *user_data,
                                              const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **key )
{
    return pj_ioqueue_register_sock2(pool, ioqueue, sock, NULL, user_data, cb,
                                     key);
}


/*
 * pj_ioqueue_get_user_data()
 */
PJ_DEF(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key )
{
    PJ_ASSERT_RETURN(key, NULL);
    return key->user_data;
}

/*
 * pj_ioqueue_set_user_data()
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_user_data( pj_ioqueue_key_t *key,
                                              void *user_data,
                                              void **old_data )
{
    PJ_ASSERT_RETURN(key, PJ_EINVAL);
    
    if (old_data)
        *old_data = key->user_data;

    key->user_data = user_data;
    return PJ_SUCCESS;
}


static void key_on_destroy(void *data) {
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)data;
    pj_ioqueue_t* ioqueue = key->ioqueue;

    /* Reset pool & keys */
    key->grp_lock = NULL;
    pj_pool_safe_release(&key->pool);

    /* Reset free pending lists */
    pj_assert(pj_list_empty(&key->pending_list));
    pj_list_init(&key->free_pending_list);

    /* Return key to free list */
    pj_lock_acquire(ioqueue->lock);
    pj_list_erase(key);
    pj_list_push_back(&ioqueue->free_list, key);

    TRACE((THIS_FILE, "FREE key %p", key));

    pj_lock_release(ioqueue->lock);
}


/* Increment the key's reference counter. */
static void increment_counter(pj_ioqueue_key_t* key)
{
    pj_grp_lock_add_ref_dbg(key->grp_lock, "ioqueue", 0);
}


/* Decrement the key's reference counter, and when the counter reach zero,
 * destroy the key.
 */
static void decrement_counter(pj_ioqueue_key_t *key)
{
    pj_grp_lock_dec_ref_dbg(key->grp_lock, "ioqueue", 0);
}

static struct pending_op *alloc_pending_op(pj_ioqueue_key_t *key,
                                           pj_ioqueue_op_key_t *op_key,
                                           void *buf,
                                           pj_ssize_t len)
{
    struct pending_op *op = NULL;
    int ref_cnt;

    pj_assert(key && op_key);

    /* Get pending op from free op list, or create a new one if none */
    pj_ioqueue_lock_key(key);
    ref_cnt = pj_grp_lock_get_ref(key->grp_lock);

    if (pj_list_empty(&key->free_pending_list)) {
        op = PJ_POOL_ZALLOC_T(key->pool, struct pending_op);
        if (!op) {
            pj_ioqueue_unlock_key(key);
            return NULL;
        }
        pj_list_init(op);
    } else {
        op = key->free_pending_list.next;
        pj_list_erase(op);
    }
    pj_list_push_back(&key->pending_list, op);
    increment_counter(key);
    pj_ioqueue_unlock_key(key);

    /* Init the pending op */
    op->app_op_key = op_key;
    op->pending_key.overlapped.wsabuf.buf = (CHAR*)buf;
    op->pending_key.overlapped.wsabuf.len = (ULONG)len;

    /* Link app op key to pending-op */
    op_key->internal__[PENDING_OP_POS(op_key)] = op;

    TRACE((THIS_FILE, "ALLOC   op key %p (cnt=%d) op %p", key, ref_cnt, op));

    return op;
}

static void release_pending_op(pj_ioqueue_key_t *key, struct pending_op *op)
{
    int ref_cnt;

    pj_assert(key && op);
    pj_ioqueue_lock_key(key);
    pj_list_erase(op);
    pj_list_push_back(&key->free_pending_list, op);
    decrement_counter(key);
    ref_cnt = pj_grp_lock_get_ref(key->grp_lock);
    pj_ioqueue_unlock_key(key);

    TRACE((THIS_FILE, "RELEASE op key %p (cnt=%d) op %p", key, ref_cnt, op));
}

static pj_status_t cancel_all_pending_op(pj_ioqueue_key_t *key)
{
    BOOL rc = fnCancelIoEx(key->hnd, NULL);

    if (rc == 0) {
        DWORD dwError = WSAGetLastError();
        if (dwError != ERROR_NOT_FOUND) {
            TRACE((THIS_FILE, "CANCEL key %p error %d", key, dwError));
            return PJ_RETURN_OS_ERROR(dwError);
        }
    }

    TRACE((THIS_FILE, "CANCEL key %p success", key));
    return PJ_SUCCESS;
}

/*
 * Poll the I/O Completion Port, execute callback, 
 * and return the key and bytes transferred of the last operation.
 */
static pj_bool_t poll_iocp( HANDLE hIocp, DWORD dwTimeout, 
                            pj_ssize_t *p_bytes, pj_ioqueue_key_t **p_key )
{
    DWORD dwBytesTransferred;
    ULONG_PTR dwKey;
    generic_overlapped *pOv;
    pj_ioqueue_key_t *key;
    pj_ssize_t size_status = -1;
    BOOL rcGetQueued;
    struct pending_op *op = NULL;
    pj_ioqueue_op_key_t *op_key = NULL;

    /* Poll for completion status. */
    rcGetQueued = GetQueuedCompletionStatus(hIocp, &dwBytesTransferred,
                                            &dwKey, (OVERLAPPED**)&pOv, 
                                            dwTimeout);
    if (!rcGetQueued && pOv) {
        PJ_PERROR(4, (THIS_FILE, PJ_STATUS_FROM_OS(GetLastError()),
                      "GetQueuedCompletionStatus() error dwKey:%p, pOv:%p",
                      (void *)dwKey, pOv));
    }

    /* The return value is:
     * - nonzero if event was dequeued.
     * - zero and pOv==NULL if no event was dequeued.
     * - zero and pOv!=NULL if event for failed I/O was dequeued.
     */
    if (pOv) {
        pj_bool_t has_lock;
        pj_ioqueue_operation_e operation = pOv->operation;

        /* Event was dequeued for either successfull or failed I/O */
        key = (pj_ioqueue_key_t*)dwKey;
        size_status = dwBytesTransferred;

        /* Report to caller regardless */
        if (p_bytes)
            *p_bytes = size_status;
        if (p_key)
            *p_key = key;

        switch(operation)
        {
            case PJ_IOQUEUE_OP_RECV:
            case PJ_IOQUEUE_OP_RECV_FROM:
            case PJ_IOQUEUE_OP_SEND:
            case PJ_IOQUEUE_OP_SEND_TO:
            case PJ_IOQUEUE_OP_ACCEPT:
                op = (struct pending_op*)
                     ((char*)pOv - offsetof(struct pending_op, pending_key));
                op_key = op->app_op_key;
                break;
            default:
                /* Invalid operation, just release op & ignore */
                pj_assert(0);
                op = (struct pending_op*)
                    ((char*)pOv - offsetof(struct pending_op, pending_key));
                release_pending_op(key, op);
                return PJ_TRUE;
        }

        /* We shouldn't call callbacks if key is quitting. */
        if (key->closing) {
            release_pending_op(key, op);
            return PJ_TRUE;
        }

        /* If concurrency is disabled, lock the key 
         * (and save the lock status to local var since app may change
         * concurrency setting while in the callback) */
        if (key->allow_concurrent == PJ_FALSE) {
            pj_ioqueue_lock_key(key);
            has_lock = PJ_TRUE;
        } else {
            has_lock = PJ_FALSE;
        }

        /* Now that we get the lock, check again that key is not closing */
        if (key->closing) {
            if (has_lock) {
                pj_ioqueue_unlock_key(key);
            }
            release_pending_op(key, op);
            return PJ_TRUE;
        }

        /* Increment reference counter to prevent this key from being
         * deleted
         */
        increment_counter(key);

        /* Carry out the callback */
        switch (operation) {
        case PJ_IOQUEUE_OP_READ:
        case PJ_IOQUEUE_OP_RECV:
        case PJ_IOQUEUE_OP_RECV_FROM:
            //pOv->operation = 0;
            OPKEY_OPERATION(op_key) = 0;
            if (key->cb.on_read_complete)
                key->cb.on_read_complete(key, op_key, size_status);
            break;
        case PJ_IOQUEUE_OP_WRITE:
        case PJ_IOQUEUE_OP_SEND:
        case PJ_IOQUEUE_OP_SEND_TO:
            //pOv->operation = 0;
            OPKEY_OPERATION(op_key) = 0;
            if (key->cb.on_write_complete)
                key->cb.on_write_complete(key, op_key, size_status);
            break;
#if PJ_HAS_TCP
        case PJ_IOQUEUE_OP_ACCEPT:
            /* special case for accept. */
            OPKEY_OPERATION(op_key) = 0;
            ioqueue_on_accept_complete(key, (ioqueue_accept_rec*)pOv);
            if (key->cb.on_accept_complete) {
                ioqueue_accept_rec *accept_rec = (ioqueue_accept_rec*)pOv;
                pj_status_t status = PJ_SUCCESS;
                pj_sock_t newsock;

                newsock = accept_rec->newsock;
                accept_rec->newsock = PJ_INVALID_SOCKET;

                if (newsock == PJ_INVALID_SOCKET) {
                    int dwError = WSAGetLastError();
                    if (dwError == 0) dwError = OSERR_ENOTCONN;
                    status = PJ_RETURN_OS_ERROR(dwError);
                }

                key->cb.on_accept_complete(key, op_key, newsock, status);
            }
            break;
        case PJ_IOQUEUE_OP_CONNECT:
#endif
        case PJ_IOQUEUE_OP_NONE:
            pj_assert(0);
            break;
        }

        if (has_lock)
            pj_ioqueue_unlock_key(key);

        release_pending_op(key, op);
        decrement_counter(key);

        return PJ_TRUE;
    }

    /* No event was queued. */
    return PJ_FALSE;
}

/*
 * pj_ioqueue_unregister()
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key )
{
    //unsigned i;
    //pj_bool_t has_lock;
    enum { RETRY = 10 };

    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    /* Best effort to avoid double key-unregistration */
    if (!key->grp_lock || key->closing)
        return PJ_SUCCESS;

#if PJ_HAS_TCP
    if (key->connecting) {
        unsigned pos;
        pj_ioqueue_t *ioqueue;

        ioqueue = key->ioqueue;

        /* Erase from connecting_handles */
        pj_lock_acquire(ioqueue->lock);
        for (pos=0; pos < ioqueue->connecting_count; ++pos) {
            if (ioqueue->connecting_keys[pos] == key) {
                erase_connecting_socket(ioqueue, pos);
                break;
            }
        }
        key->connecting = 0;
        pj_lock_release(ioqueue->lock);
    }
#endif

    /* Mark key as closing before closing handle. */
    key->closing = 1;

    /* If concurrency is disabled, wait until the key has finished
     * processing the callback
     */
    //if (key->allow_concurrent == PJ_FALSE) {
    //    pj_ioqueue_lock_key(key);
    //    has_lock = PJ_TRUE;
    //} else {
    //    has_lock = PJ_FALSE;
    //}

    /* Cancel all pending I/O operations (asynchronously) */
    cancel_all_pending_op(key);

    /* Close handle (the only way to disassociate handle from IOCP). 
     * We also need to close handle to make sure that no further events
     * will come to the handle.
     */
    /* Update 2008/07/18 (https://github.com/pjsip/pjproject/issues/575):
     *  - It seems that CloseHandle() in itself does not actually close
     *    the socket (i.e. it will still appear in "netstat" output). Also
     *    if we only use CloseHandle(), an "Invalid Handle" exception will
     *    be raised in WSACleanup().
     *  - MSDN documentation says that CloseHandle() must be called after 
     *    closesocket() call (see
     *    http://msdn.microsoft.com/en-us/library/ms724211(VS.85).aspx).
     *    But turns out that this will raise "Invalid Handle" exception
     *    in debug mode.
     *  So because of this, we replaced CloseHandle() with closesocket()
     *  instead. These was tested on WinXP SP2.
     */
    //CloseHandle(key->hnd);
    pj_sock_close((pj_sock_t)key->hnd);

    /* Reset callbacks */
    key->cb.on_accept_complete = NULL;
    key->cb.on_connect_complete = NULL;
    key->cb.on_read_complete = NULL;
    key->cb.on_write_complete = NULL;

    /* Even after handle is closed, I suspect that IOCP may still try to
     * do something with the handle, causing memory corruption when pool
     * debugging is enabled.
     *
     * Forcing context switch seems to have fixed that, but this is quite
     * an ugly solution..
     *
     * Update 2008/02/13:
     *  This should not happen if concurrency is disallowed for the key.
     *  So at least application has a solution for this (i.e. by disallowing
     *  concurrency in the key).
     * 
     * Update 2025/01/20:
     *  Any pending ops will be cancelled asynchronously, so key resources
     *  will be released later from the group lock handler after all
     *  pending ops are cancelled.
     */
    //This will loop forever if unregistration is done on the callback.
    //Doing this with RETRY I think should solve the IOCP setting the 
    //socket signalled, without causing the deadlock.
    //while (pj_atomic_get(key->ref_count) != 1)
    //  pj_thread_sleep(0);
    //for (i=0; pj_atomic_get(key->ref_count) != 1 && i<RETRY; ++i)
    //    pj_thread_sleep(0);

    //if (has_lock)
    //    pj_ioqueue_unlock_key(key);

    TRACE((THIS_FILE, "UNREG key %p ref cnt %d",
                      key, pj_grp_lock_get_ref(key->grp_lock)));

    /* Decrement reference counter to destroy the key.
     * If the key has pending op, it will be destroyed only after the op is
     * cancelled (asynchronously).
     */
    decrement_counter(key);

    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_poll()
 *
 * Poll for events.
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioqueue, const pj_time_val *timeout)
{
    DWORD dwMsec;
#if PJ_HAS_TCP
    int connect_count = 0;
#endif
    int event_count = 0;

    PJ_ASSERT_RETURN(ioqueue, -PJ_EINVAL);

    /* Calculate miliseconds timeout for GetQueuedCompletionStatus */
    dwMsec = timeout ? timeout->sec*1000 + timeout->msec : INFINITE;

    /* Poll for completion status. */
    event_count = poll_iocp(ioqueue->iocp, dwMsec, NULL, NULL);

#if PJ_HAS_TCP
    /* Check the connecting array, only when there's no activity. */
    if (event_count == 0) {
        connect_count = check_connecting(ioqueue);
        if (connect_count > 0)
            event_count += connect_count;
    }
#endif

    /* Return number of events. */
    return event_count;
}

/*
 * pj_ioqueue_recv()
 *
 * Initiate overlapped WSARecv() operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recv(  pj_ioqueue_key_t *key,
                                      pj_ioqueue_op_key_t *op_key,
                                      void *buffer,
                                      pj_ssize_t *length,
                                      pj_uint32_t flags )
{
    /*
     * Ideally we should just call pj_ioqueue_recvfrom() with NULL addr and
     * addrlen here. But unfortunately it generates EINVAL... :-(
     *  -bennylp
     */
    int rc;
    DWORD bytesRead;
    DWORD dwFlags = 0;
    union operation_key *op_key_rec;
    struct pending_op *op;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && op_key && buffer && length, PJ_EINVAL);

    /* Check key is not closing */
    if (key->closing)
        return PJ_ECANCELLED;

    op_key_rec = (union operation_key*)op_key->internal__;
    op_key_rec->overlapped.wsabuf.buf = buffer;
    op_key_rec->overlapped.wsabuf.len = (ULONG)*length;

    dwFlags = flags;
    
    /* Try non-overlapped received first to see if data is
     * immediately available.
     */
    if ((flags & PJ_IOQUEUE_ALWAYS_ASYNC) == 0) {
        rc = WSARecv((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
                     &bytesRead, &dwFlags, NULL, NULL);
        if (rc == 0) {
            *length = bytesRead;
            return PJ_SUCCESS;
        } else {
            DWORD dwError = WSAGetLastError();
            if (dwError != WSAEWOULDBLOCK) {
                *length = -1;
                return PJ_RETURN_OS_ERROR(dwError);
            }
        }
    }

    op = alloc_pending_op(key, op_key, buffer, *length);
    if (!op)
        return PJ_ENOMEM;

    op_key_rec = &op->pending_key;

    dwFlags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No immediate data available.
     * Register overlapped Recv() operation.
     */
    pj_bzero( &op_key_rec->overlapped.overlapped, 
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = PJ_IOQUEUE_OP_RECV;
    OPKEY_OPERATION(op_key) = PJ_IOQUEUE_OP_RECV;

    rc = WSARecv((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1, 
                  &bytesRead, &dwFlags, 
                  &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
        DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING) {
            *length = -1;
            release_pending_op(key, op);
            return PJ_STATUS_FROM_OS(dwStatus);
        }
    }

    /* Pending operation has been scheduled. */
    return PJ_EPENDING;
}

/*
 * pj_ioqueue_recvfrom()
 *
 * Initiate overlapped RecvFrom() operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key,
                                         void *buffer,
                                         pj_ssize_t *length,
                                         pj_uint32_t flags,
                                         pj_sockaddr_t *addr,
                                         int *addrlen)
{
    int rc;
    DWORD bytesRead;
    DWORD dwFlags = 0;
    union operation_key *op_key_rec;
    struct pending_op *op;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && op_key && buffer, PJ_EINVAL);

    /* Check key is not closing */
    if (key->closing)
        return PJ_ECANCELLED;

    op_key_rec = (union operation_key*)op_key->internal__;
    op_key_rec->overlapped.wsabuf.buf = buffer;
    op_key_rec->overlapped.wsabuf.len = (ULONG)*length;

    dwFlags = flags;
    
    /* Try non-overlapped received first to see if data is
     * immediately available.
     */
    if ((flags & PJ_IOQUEUE_ALWAYS_ASYNC) == 0) {
        rc = WSARecvFrom((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
                         &bytesRead, &dwFlags, addr, addrlen, NULL, NULL);
        if (rc == 0) {
            *length = bytesRead;
            return PJ_SUCCESS;
        } else {
            DWORD dwError = WSAGetLastError();
            if (dwError != WSAEWOULDBLOCK) {
                *length = -1;
                return PJ_RETURN_OS_ERROR(dwError);
            }
        }
    }

    op = alloc_pending_op(key, op_key, buffer, *length);
    if (!op)
        return PJ_ENOMEM;

    op_key_rec = &op->pending_key;

    dwFlags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No immediate data available.
     * Register overlapped Recv() operation.
     */
    pj_bzero( &op_key_rec->overlapped.overlapped, 
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = PJ_IOQUEUE_OP_RECV;
    OPKEY_OPERATION(op_key) = PJ_IOQUEUE_OP_RECV;

    rc = WSARecvFrom((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1, 
                     &bytesRead, &dwFlags, addr, addrlen,
                     &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
        DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING) {
            *length = -1;
            release_pending_op(key, op);
            return PJ_STATUS_FROM_OS(dwStatus);
        }
    } 

    /* Pending operation has been scheduled. */
    return PJ_EPENDING;
}

/*
 * pj_ioqueue_send()
 *
 * Initiate overlapped Send operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_send(  pj_ioqueue_key_t *key,
                                      pj_ioqueue_op_key_t *op_key,
                                      const void *data,
                                      pj_ssize_t *length,
                                      pj_uint32_t flags )
{
    return pj_ioqueue_sendto(key, op_key, data, length, flags, NULL, 0);
}


/*
 * pj_ioqueue_sendto()
 *
 * Initiate overlapped SendTo operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
                                       const void *data,
                                       pj_ssize_t *length,
                                       pj_uint32_t flags,
                                       const pj_sockaddr_t *addr,
                                       int addrlen)
{
    int rc;
    DWORD bytesWritten;
    DWORD dwFlags;
    union operation_key *op_key_rec;
    struct pending_op *op;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && op_key && data, PJ_EINVAL);

    /* Check key is not closing */
    if (key->closing)
        return PJ_ECANCELLED;

    op_key_rec = (union operation_key*)op_key->internal__;

    /*
     * First try blocking write.
     */
    op_key_rec->overlapped.wsabuf.buf = (void*)data;
    op_key_rec->overlapped.wsabuf.len = (ULONG)*length;

    dwFlags = flags;

    if ((flags & PJ_IOQUEUE_ALWAYS_ASYNC) == 0) {
        rc = WSASendTo((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
                       &bytesWritten, dwFlags, addr, addrlen,
                       NULL, NULL);
        if (rc == 0) {
            *length = bytesWritten;
            return PJ_SUCCESS;
        } else {
            DWORD dwStatus = WSAGetLastError();
            if (dwStatus != WSAEWOULDBLOCK) {
                *length = -1;
                return PJ_RETURN_OS_ERROR(dwStatus);
            }
        }
    }

    op = alloc_pending_op(key, op_key, (void *)data, *length);
    if (!op)
        return PJ_ENOMEM;

    op_key_rec = &op->pending_key;

    dwFlags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /*
     * Data can't be sent immediately.
     * Schedule asynchronous WSASend().
     */
    pj_bzero( &op_key_rec->overlapped.overlapped, 
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = PJ_IOQUEUE_OP_SEND;
    OPKEY_OPERATION(op_key) = PJ_IOQUEUE_OP_SEND;

    rc = WSASendTo((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
                   &bytesWritten,  dwFlags, addr, addrlen,
                   &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
        DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING) {
            release_pending_op(key, op);
            return PJ_STATUS_FROM_OS(dwStatus);
        }
    }

    /* Asynchronous operation successfully submitted. */
    return PJ_EPENDING;
}

#if PJ_HAS_TCP

/*
 * pj_ioqueue_accept()
 *
 * Initiate overlapped accept() operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_accept( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
                                       pj_sock_t *new_sock,
                                       pj_sockaddr_t *local,
                                       pj_sockaddr_t *remote,
                                       int *addrlen)
{
    BOOL rc;
    DWORD bytesReceived;
    pj_status_t status;
    union operation_key *op_key_rec;
    SOCKET sock;
    struct pending_op *op;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && op_key && new_sock, PJ_EINVAL);

    /* Check key is not closing */
    if (key->closing)
        return PJ_ECANCELLED;

    /*
     * See if there is a new connection immediately available.
     */
    sock = WSAAccept((SOCKET)key->hnd, remote, addrlen, NULL, 0);
    if (sock != INVALID_SOCKET) {
        /* Yes! New socket is available! */
        if (local && addrlen) {
            int status_;

            /* On WinXP or later, use SO_UPDATE_ACCEPT_CONTEXT so that socket 
             * addresses can be obtained with getsockname() and getpeername().
             */
            status_ = setsockopt(sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                                 (char*)&key->hnd, sizeof(SOCKET));
            /* SO_UPDATE_ACCEPT_CONTEXT is for WinXP or later.
             * So ignore the error status.
             */

            status_ = getsockname(sock, local, addrlen);
            if (status_ != 0) {
                DWORD dwError = WSAGetLastError();
                closesocket(sock);
                return PJ_RETURN_OS_ERROR(dwError);
            }
        }

        *new_sock = sock;
        return PJ_SUCCESS;

    } else {
        DWORD dwError = WSAGetLastError();
        if (dwError != WSAEWOULDBLOCK) {
            return PJ_RETURN_OS_ERROR(dwError);
        }
    }

    /*
     * No connection is immediately available.
     * Must schedule an asynchronous operation.
     */
    op = alloc_pending_op(key, op_key, NULL, 0);
    if (!op)
        return PJ_ENOMEM;

    op_key_rec = &op->pending_key;

    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, 
                            &op_key_rec->accept.newsock);
    if (status != PJ_SUCCESS) {
        release_pending_op(key, op);
        return status;
    }

    OPKEY_OPERATION(op_key) = PJ_IOQUEUE_OP_ACCEPT;
    op_key_rec->accept.operation = PJ_IOQUEUE_OP_ACCEPT;
    op_key_rec->accept.addrlen = addrlen;
    op_key_rec->accept.local = local;
    op_key_rec->accept.remote = remote;
    op_key_rec->accept.newsock_ptr = new_sock;
    pj_bzero( &op_key_rec->accept.overlapped, 
              sizeof(op_key_rec->accept.overlapped));

    rc = AcceptEx( (SOCKET)key->hnd, (SOCKET)op_key_rec->accept.newsock,
                   op_key_rec->accept.accept_buf,
                   0, ACCEPT_ADDR_LEN, ACCEPT_ADDR_LEN,
                   &bytesReceived,
                   &op_key_rec->accept.overlapped );

    if (rc == TRUE) {
        ioqueue_on_accept_complete(key, &op_key_rec->accept);
        release_pending_op(key, op);
        return PJ_SUCCESS;
    } else {
        DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING) {
            release_pending_op(key, op);
            return PJ_STATUS_FROM_OS(dwStatus);
        }
    }

    /* Asynchronous Accept() has been submitted. */
    return PJ_EPENDING;
}


/*
 * pj_ioqueue_connect()
 *
 * Initiate overlapped connect() operation (well, it's non-blocking actually,
 * since there's no overlapped version of connect()).
 */
PJ_DEF(pj_status_t) pj_ioqueue_connect( pj_ioqueue_key_t *key,
                                        const pj_sockaddr_t *addr,
                                        int addrlen )
{
    HANDLE hEvent;
    pj_ioqueue_t *ioqueue;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && addr && addrlen, PJ_EINVAL);

    /* Check key is not closing */
    if (key->closing)
        return PJ_ECANCELLED;

    /* Initiate connect() */
    if (connect((pj_sock_t)key->hnd, addr, addrlen) != 0) {
        DWORD dwStatus;
        dwStatus = WSAGetLastError();
        if (dwStatus != WSAEWOULDBLOCK) {
            return PJ_RETURN_OS_ERROR(dwStatus);
        }
    } else {
        /* Connect has completed immediately! */
        return PJ_SUCCESS;
    }

    ioqueue = key->ioqueue;

    /* Add to the array of connecting socket to be polled */
    pj_lock_acquire(ioqueue->lock);

    if (ioqueue->connecting_count >= MAXIMUM_WAIT_OBJECTS) {
        pj_lock_release(ioqueue->lock);
        return PJ_ETOOMANYCONN;
    }

    /* Get or create event object. */
    if (ioqueue->event_count) {
        hEvent = ioqueue->event_pool[ioqueue->event_count - 1];
        --ioqueue->event_count;
    } else {
        hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (hEvent == NULL) {
            DWORD dwStatus = GetLastError();
            pj_lock_release(ioqueue->lock);
            return PJ_STATUS_FROM_OS(dwStatus);
        }
    }

    /* Mark key as connecting.
     * We can't use array index since key can be removed dynamically. 
     */
    key->connecting = 1;

    /* Associate socket events to the event object. */
    if (WSAEventSelect((pj_sock_t)key->hnd, hEvent, FD_CONNECT) != 0) {
        CloseHandle(hEvent);
        pj_lock_release(ioqueue->lock);
        return PJ_RETURN_OS_ERROR(WSAGetLastError());
    }

    /* Add to array. */
    ioqueue->connecting_keys[ ioqueue->connecting_count ] = key;
    ioqueue->connecting_handles[ ioqueue->connecting_count ] = hEvent;
    ioqueue->connecting_count++;

    pj_lock_release(ioqueue->lock);

    return PJ_EPENDING;
}
#endif  /* #if PJ_HAS_TCP */


PJ_DEF(void) pj_ioqueue_op_key_init( pj_ioqueue_op_key_t *op_key,
                                     pj_size_t size )
{
    pj_bzero(op_key, size);
}

PJ_DEF(pj_bool_t) pj_ioqueue_is_pending( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key )
{
    struct generic_overlapped* op_rec;

    PJ_UNUSED_ARG(key);

    /* Instead of using GetOverlappedResult(), simply checking the operation
     * status should be fine.
     */
    op_rec = (struct generic_overlapped*)op_key;
    return op_rec->operation != 0;

#if 0
    BOOL rc;
    DWORD bytesTransferred;
    struct pending_op *op;

    op = get_pending_op(op_key);
    if (!op)
        return PJ_FALSE;

    rc = GetOverlappedResult( key->hnd, (LPOVERLAPPED)&op->pending_key,
                              &bytesTransferred, FALSE );

    if (rc == FALSE) {
        return GetLastError()==ERROR_IO_INCOMPLETE;
    }

    return FALSE;
#endif
}


PJ_DEF(pj_status_t) pj_ioqueue_post_completion( pj_ioqueue_key_t *key,
                                                pj_ioqueue_op_key_t *op_key,
                                                pj_ssize_t bytes_status )
{
    BOOL rc;
    struct pending_op* op;

    op = get_pending_op(op_key);
    if (!op)
        return PJ_EINVAL;

    rc = PostQueuedCompletionStatus(key->ioqueue->iocp, (DWORD)bytes_status,
                                    (ULONG_PTR)key,
                                    (OVERLAPPED*)&op->pending_key );
    if (rc == FALSE) {
        return PJ_RETURN_OS_ERROR(GetLastError());
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_set_concurrency(pj_ioqueue_key_t *key,
                                               pj_bool_t allow)
{
    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    /* PJ_IOQUEUE_HAS_SAFE_UNREG must be enabled if concurrency is
     * disabled.
     */
    PJ_ASSERT_RETURN(allow || PJ_IOQUEUE_HAS_SAFE_UNREG, PJ_EINVAL);

    key->allow_concurrent = allow;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_lock_key(pj_ioqueue_key_t *key)
{
    PJ_ASSERT_RETURN(key && key->grp_lock, PJ_EINVAL);
    return pj_grp_lock_acquire(key->grp_lock);
}

PJ_DEF(pj_status_t) pj_ioqueue_unlock_key(pj_ioqueue_key_t *key)
{
    PJ_ASSERT_RETURN(key && key->grp_lock, PJ_EINVAL);
    return pj_grp_lock_release(key->grp_lock);
}

PJ_DEF(pj_oshandle_t) pj_ioqueue_get_os_handle( pj_ioqueue_t *ioqueue )
{
    return ioqueue ? (pj_oshandle_t)ioqueue->iocp : NULL;
}


#endif /* PJ_IOQUEUE_IMP == PJ_IOQUEUE_IMP_IOCP */

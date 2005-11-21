/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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


#if defined(PJ_HAS_WINSOCK2_H) && PJ_HAS_WINSOCK2_H != 0
#  include <winsock2.h>
#elif defined(PJ_HAS_WINSOCK_H) && PJ_HAS_WINSOCK_H != 0
#  include <winsock.h>
#endif

#if defined(PJ_HAS_MSWSOCK_H) && PJ_HAS_MSWSOCK_H != 0
#  include <mswsock.h>
#endif


/* The address specified in AcceptEx() must be 16 more than the size of
 * SOCKADDR (source: MSDN).
 */
#define ACCEPT_ADDR_LEN	    (sizeof(pj_sockaddr_in)+16)

typedef struct generic_overlapped
{
    WSAOVERLAPPED	   overlapped;
    pj_ioqueue_operation_e operation;
} generic_overlapped;

/*
 * OVERLAPPPED structure for send and receive.
 */
typedef struct ioqueue_overlapped
{
    WSAOVERLAPPED	   overlapped;
    pj_ioqueue_operation_e operation;
    WSABUF		   wsabuf;
    pj_sockaddr_in         dummy_addr;
    int                    dummy_addrlen;
} ioqueue_overlapped;

#if PJ_HAS_TCP
/*
 * OVERLAP structure for accept.
 */
typedef struct ioqueue_accept_rec
{
    WSAOVERLAPPED	    overlapped;
    pj_ioqueue_operation_e  operation;
    pj_sock_t		    newsock;
    pj_sock_t		   *newsock_ptr;
    int			   *addrlen;
    void		   *remote;
    void		   *local;
    char		    accept_buf[2 * ACCEPT_ADDR_LEN];
} ioqueue_accept_rec;
#endif

/*
 * Structure to hold pending operation key.
 */
union operation_key
{
    generic_overlapped      generic;
    ioqueue_overlapped      overlapped;
#if PJ_HAS_TCP
    ioqueue_accept_rec      accept;
#endif
};

/* Type of handle in the key. */
enum handle_type
{
    HND_IS_UNKNOWN,
    HND_IS_FILE,
    HND_IS_SOCKET,
};

/*
 * Structure for individual socket.
 */
struct pj_ioqueue_key_t
{
    pj_ioqueue_t       *ioqueue;
    HANDLE		hnd;
    void	       *user_data;
    enum handle_type    hnd_type;
#if PJ_HAS_TCP
    int			connecting;
#endif
    pj_ioqueue_callback	cb;
};

/*
 * IO Queue structure.
 */
struct pj_ioqueue_t
{
    HANDLE	      iocp;
    pj_lock_t        *lock;
    pj_bool_t         auto_delete_lock;
    unsigned	      event_count;
    HANDLE	      event_pool[MAXIMUM_WAIT_OBJECTS+1];
#if PJ_HAS_TCP
    unsigned	      connecting_count;
    HANDLE	      connecting_handles[MAXIMUM_WAIT_OBJECTS+1];
    pj_ioqueue_key_t *connecting_keys[MAXIMUM_WAIT_OBJECTS+1];
#endif
};


#if PJ_HAS_TCP
/*
 * Process the socket when the overlapped accept() completed.
 */
static void ioqueue_on_accept_complete(ioqueue_accept_rec *accept_overlapped)
{
    struct sockaddr *local;
    struct sockaddr *remote;
    int locallen, remotelen;

    PJ_CHECK_STACK();

    /* Operation complete immediately. */
    GetAcceptExSockaddrs( accept_overlapped->accept_buf,
			  0, 
			  ACCEPT_ADDR_LEN,
			  ACCEPT_ADDR_LEN,
			  &local,
			  &locallen,
			  &remote,
			  &remotelen);
    if (*accept_overlapped->addrlen > locallen) {
        pj_memcpy(accept_overlapped->local, local, locallen);
        pj_memcpy(accept_overlapped->remote, remote, locallen);
    } else {
        pj_memset(accept_overlapped->local, 0, *accept_overlapped->addrlen);
        pj_memset(accept_overlapped->remote, 0, *accept_overlapped->addrlen);
    }
    *accept_overlapped->addrlen = locallen;
    if (accept_overlapped->newsock_ptr)
        *accept_overlapped->newsock_ptr = accept_overlapped->newsock;
    accept_overlapped->operation = 0;
    accept_overlapped->newsock = PJ_INVALID_SOCKET;
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
static pj_ioqueue_key_t *check_connecting( pj_ioqueue_t *ioqueue, 
					   pj_ssize_t *connect_err )
{
    pj_ioqueue_key_t *key = NULL;

    if (ioqueue->connecting_count) {
	DWORD result;

	pj_lock_acquire(ioqueue->lock);
	result = WaitForMultipleObjects(ioqueue->connecting_count,
					ioqueue->connecting_handles,
					FALSE, 0);
	if (result >= WAIT_OBJECT_0 && 
	    result < WAIT_OBJECT_0+ioqueue->connecting_count) 
	{
	    WSANETWORKEVENTS net_events;

	    /* Got completed connect(). */
	    unsigned pos = result - WAIT_OBJECT_0;
	    key = ioqueue->connecting_keys[pos];

	    /* See whether connect has succeeded. */
	    WSAEnumNetworkEvents((pj_sock_t)key->hnd, 
				 ioqueue->connecting_handles[pos], 
				 &net_events);
	    *connect_err = 
                PJ_STATUS_FROM_OS(net_events.iErrorCode[FD_CONNECT_BIT]);

	    /* Erase socket from pending connect. */
	    erase_connecting_socket(ioqueue, pos);
	}
	pj_lock_release(ioqueue->lock);
    }
    return key;
}
#endif

/*
 * pj_ioqueue_name()
 */
PJ_DEF(const char*) pj_ioqueue_name(void)
{
    return "iocp";
}

/*
 * pj_ioqueue_create()
 */
PJ_DEF(pj_status_t) pj_ioqueue_create( pj_pool_t *pool, 
				       pj_size_t max_fd,
				       pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioqueue;
    pj_status_t rc;

    PJ_UNUSED_ARG(max_fd);
    PJ_ASSERT_RETURN(pool && p_ioqueue, PJ_EINVAL);

    rc = sizeof(union operation_key);

    /* Check that sizeof(pj_ioqueue_op_key_t) makes sense. */
    PJ_ASSERT_RETURN(sizeof(pj_ioqueue_op_key_t)-sizeof(void*) >= 
                     sizeof(union operation_key), PJ_EBUG);

    ioqueue = pj_pool_zalloc(pool, sizeof(*ioqueue));
    ioqueue->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (ioqueue->iocp == NULL)
	return PJ_RETURN_OS_ERROR(GetLastError());

    rc = pj_lock_create_simple_mutex(pool, NULL, &ioqueue->lock);
    if (rc != PJ_SUCCESS) {
	CloseHandle(ioqueue->iocp);
	return rc;
    }

    ioqueue->auto_delete_lock = PJ_TRUE;

    *p_ioqueue = ioqueue;

    PJ_LOG(4, ("pjlib", "WinNT IOCP I/O Queue created (%p)", ioqueue));
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_destroy()
 */
PJ_DEF(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioqueue )
{
    unsigned i;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(ioqueue, PJ_EINVAL);

    /* Destroy events in the pool */
    for (i=0; i<ioqueue->event_count; ++i) {
	CloseHandle(ioqueue->event_pool[i]);
    }
    ioqueue->event_count = 0;

    if (CloseHandle(ioqueue->iocp) != TRUE)
	return PJ_RETURN_OS_ERROR(GetLastError());

    if (ioqueue->auto_delete_lock)
        pj_lock_destroy(ioqueue->lock);

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
 * pj_ioqueue_register_sock()
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					      pj_ioqueue_t *ioqueue,
					      pj_sock_t sock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
					      pj_ioqueue_key_t **key )
{
    HANDLE hioq;
    pj_ioqueue_key_t *rec;
    u_long value;
    int rc;

    PJ_ASSERT_RETURN(pool && ioqueue && cb && key, PJ_EINVAL);

    /* Build the key for this socket. */
    rec = pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));
    rec->ioqueue = ioqueue;
    rec->hnd = (HANDLE)sock;
    rec->hnd_type = HND_IS_SOCKET;
    rec->user_data = user_data;
    pj_memcpy(&rec->cb, cb, sizeof(pj_ioqueue_callback));

    /* Set socket to nonblocking. */
    value = 1;
    rc = ioctlsocket(sock, FIONBIO, &value);
    if (rc != 0) {
        return PJ_RETURN_OS_ERROR(WSAGetLastError());
    }

    /* Associate with IOCP */
    hioq = CreateIoCompletionPort((HANDLE)sock, ioqueue->iocp, (DWORD)rec, 0);
    if (!hioq) {
	return PJ_RETURN_OS_ERROR(GetLastError());
    }

    *key = rec;
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_unregister()
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key )
{
    PJ_ASSERT_RETURN(key, PJ_EINVAL);

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
    if (key->hnd_type == HND_IS_FILE) {
        CloseHandle(key->hnd);
    }
    return PJ_SUCCESS;
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

/*
 * pj_ioqueue_poll()
 *
 * Poll for events.
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioqueue, const pj_time_val *timeout)
{
    DWORD dwMsec, dwBytesTransfered, dwKey;
    generic_overlapped *pOv;
    pj_ioqueue_key_t *key;
    pj_ssize_t size_status;
    BOOL rc;

    PJ_ASSERT_RETURN(ioqueue, -PJ_EINVAL);

    /* Check the connecting array. */
#if PJ_HAS_TCP
    key = check_connecting(ioqueue, &size_status);
    if (key != NULL) {
	key->cb.on_connect_complete(key, (int)size_status);
	return 1;
    }
#endif

    /* Calculate miliseconds timeout for GetQueuedCompletionStatus */
    dwMsec = timeout ? timeout->sec*1000 + timeout->msec : INFINITE;

    /* Poll for completion status. */
    rc = GetQueuedCompletionStatus(ioqueue->iocp, &dwBytesTransfered, &dwKey,
				   (OVERLAPPED**)&pOv, dwMsec);

    /* The return value is:
     * - nonzero if event was dequeued.
     * - zero and pOv==NULL if no event was dequeued.
     * - zero and pOv!=NULL if event for failed I/O was dequeued.
     */
    if (pOv) {
	/* Event was dequeued for either successfull or failed I/O */
	key = (pj_ioqueue_key_t*)dwKey;
	size_status = dwBytesTransfered;
	switch (pOv->operation) {
	case PJ_IOQUEUE_OP_READ:
	case PJ_IOQUEUE_OP_RECV:
	case PJ_IOQUEUE_OP_RECV_FROM:
            pOv->operation = 0;
            if (key->cb.on_read_complete)
	        key->cb.on_read_complete(key, (pj_ioqueue_op_key_t*)pOv, 
                                         size_status);
	    break;
	case PJ_IOQUEUE_OP_WRITE:
	case PJ_IOQUEUE_OP_SEND:
	case PJ_IOQUEUE_OP_SEND_TO:
            pOv->operation = 0;
            if (key->cb.on_write_complete)
	        key->cb.on_write_complete(key, (pj_ioqueue_op_key_t*)pOv, 
                                                size_status);
	    break;
#if PJ_HAS_TCP
	case PJ_IOQUEUE_OP_ACCEPT:
	    /* special case for accept. */
	    ioqueue_on_accept_complete((ioqueue_accept_rec*)pOv);
            if (key->cb.on_accept_complete) {
                ioqueue_accept_rec *accept_rec = (ioqueue_accept_rec*)pOv;
	        key->cb.on_accept_complete(key, 
                                           (pj_ioqueue_op_key_t*)pOv, 
                                           accept_rec->newsock,
                                           PJ_SUCCESS);
            }
	    break;
	case PJ_IOQUEUE_OP_CONNECT:
#endif
	case PJ_IOQUEUE_OP_NONE:
	    pj_assert(0);
	    break;
	}
	return 1;
    }

    if (GetLastError()==WAIT_TIMEOUT) {
	/* Check the connecting array (again). */
#if PJ_HAS_TCP
	key = check_connecting(ioqueue, &size_status);
	if (key != NULL) {
	    key->cb.on_connect_complete(key, (int)size_status);
	    return 1;
	}
#endif
	return 0;
    }
    return -1;
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

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && op_key && buffer && length, PJ_EINVAL);

    op_key_rec = (union operation_key*)op_key->internal__;
    op_key_rec->overlapped.wsabuf.buf = buffer;
    op_key_rec->overlapped.wsabuf.len = *length;

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

    dwFlags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No immediate data available.
     * Register overlapped Recv() operation.
     */
    pj_memset(&op_key_rec->overlapped.overlapped, 0,
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = PJ_IOQUEUE_OP_RECV;

    rc = WSARecv((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1, 
                  &bytesRead, &dwFlags, 
		  &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING) {
            *length = -1;
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

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && op_key && buffer, PJ_EINVAL);

    op_key_rec = (union operation_key*)op_key->internal__;
    op_key_rec->overlapped.wsabuf.buf = buffer;
    op_key_rec->overlapped.wsabuf.len = *length;

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

    dwFlags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No immediate data available.
     * Register overlapped Recv() operation.
     */
    pj_memset(&op_key_rec->overlapped.overlapped, 0,
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = PJ_IOQUEUE_OP_RECV;

    rc = WSARecvFrom((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1, 
                     &bytesRead, &dwFlags, addr, addrlen,
		     &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING) {
            *length = -1;
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

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && op_key && data, PJ_EINVAL);
    
    op_key_rec = (union operation_key*)op_key->internal__;

    /*
     * First try blocking write.
     */
    op_key_rec->overlapped.wsabuf.buf = (void*)data;
    op_key_rec->overlapped.wsabuf.len = *length;

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

    dwFlags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /*
     * Data can't be sent immediately.
     * Schedule asynchronous WSASend().
     */
    pj_memset(&op_key_rec->overlapped.overlapped, 0,
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = PJ_IOQUEUE_OP_SEND;

    rc = WSASendTo((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
                   &bytesWritten,  dwFlags, addr, addrlen,
		   &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING)
            return PJ_STATUS_FROM_OS(dwStatus);
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

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(key && op_key && new_sock, PJ_EINVAL);

    /*
     * See if there is a new connection immediately available.
     */
    sock = WSAAccept((SOCKET)key->hnd, remote, addrlen, NULL, 0);
    if (sock != INVALID_SOCKET) {
        /* Yes! New socket is available! */
        int status;

        status = getsockname(sock, local, addrlen);
        if (status != 0) {
            DWORD dwError = WSAGetLastError();
            closesocket(sock);
            return PJ_RETURN_OS_ERROR(dwError);
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
    op_key_rec = (union operation_key*)op_key->internal__;
    
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, 
                            &op_key_rec->accept.newsock);
    if (status != PJ_SUCCESS)
	return status;

    /* On WinXP or later, use SO_UPDATE_ACCEPT_CONTEXT so that socket 
     * addresses can be obtained with getsockname() and getpeername().
     */
    status = setsockopt(op_key_rec->accept.newsock, SOL_SOCKET,
                        SO_UPDATE_ACCEPT_CONTEXT, 
                        (char*)&key->hnd, sizeof(SOCKET));
    /* SO_UPDATE_ACCEPT_CONTEXT is for WinXP or later.
     * So ignore the error status.
     */

    op_key_rec->accept.operation = PJ_IOQUEUE_OP_ACCEPT;
    op_key_rec->accept.addrlen = addrlen;
    op_key_rec->accept.local = local;
    op_key_rec->accept.remote = remote;
    op_key_rec->accept.newsock_ptr = new_sock;
    pj_memset(&op_key_rec->accept.overlapped, 0, 
	      sizeof(op_key_rec->accept.overlapped));

    rc = AcceptEx( (SOCKET)key->hnd, (SOCKET)op_key_rec->accept.newsock,
		   op_key_rec->accept.accept_buf,
		   0, ACCEPT_ADDR_LEN, ACCEPT_ADDR_LEN,
		   &bytesReceived,
		   &op_key_rec->accept.overlapped );

    if (rc == TRUE) {
	ioqueue_on_accept_complete(&op_key_rec->accept);
	return PJ_SUCCESS;
    } else {
	DWORD dwStatus = WSAGetLastError();
	if (dwStatus!=WSA_IO_PENDING)
            return PJ_STATUS_FROM_OS(dwStatus);
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
#endif	/* #if PJ_HAS_TCP */


PJ_DEF(void) pj_ioqueue_op_key_init( pj_ioqueue_op_key_t *op_key,
				     pj_size_t size )
{
    pj_memset(op_key, 0, size);
}

PJ_DEF(pj_bool_t) pj_ioqueue_is_pending( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key )
{
    BOOL rc;
    DWORD bytesTransfered;

    rc = GetOverlappedResult( key->hnd, (LPOVERLAPPED)op_key,
                              &bytesTransfered, FALSE );

    if (rc == FALSE) {
        return GetLastError()==ERROR_IO_INCOMPLETE;
    }

    return FALSE;
}


PJ_DEF(pj_status_t) pj_ioqueue_post_completion( pj_ioqueue_key_t *key,
                                                pj_ioqueue_op_key_t *op_key,
                                                pj_ssize_t bytes_status )
{
    BOOL rc;

    rc = PostQueuedCompletionStatus(key->ioqueue->iocp, bytes_status,
                                    (long)key, (OVERLAPPED*)op_key );
    if (rc == FALSE) {
        return PJ_RETURN_OS_ERROR(GetLastError());
    }

    return PJ_SUCCESS;
}


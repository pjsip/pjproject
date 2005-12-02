/* $Header: /pjproject/pjlib/src/pj/ioqueue_winnt.c 7     5/24/05 12:16a Bennylp $ */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pj/ioqueue.h>

#if defined(PJ_IOQUEUE_USE_WIN32_IOCP) && PJ_IOQUEUE_USE_WIN32_IOCP!=0

#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/sock.h>
#include <pj/array.h>
#include <pj/log.h>
#include <mswsock.h>

#define ACCEPT_ADDR_LEN	    (sizeof(pj_sockaddr_in)+20)

/*
 * OVERLAP structure for send and receive.
 */
typedef struct ioqueue_overlapped
{
    WSAOVERLAPPED	   overlapped;
    pj_ioqueue_operation_e operation;
    WSABUF		   wsabuf;
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
 * Structure for individual socket.
 */
struct pj_ioqueue_key_t
{
    HANDLE		hnd;
    void	       *user_data;
    ioqueue_overlapped	recv_overlapped;
    ioqueue_overlapped	send_overlapped;
#if PJ_HAS_TCP
    int			connecting;
    ioqueue_accept_rec	accept_overlapped;
#endif
    pj_ioqueue_callback	cb;
};

/*
 * IO Queue structure.
 */
struct pj_ioqueue_t
{
    HANDLE	      iocp;
    pj_mutex_t	     *mutex;
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

    /* Operation complete immediately. */
    GetAcceptExSockaddrs( accept_overlapped->accept_buf,
			  0, 
			  ACCEPT_ADDR_LEN,
			  ACCEPT_ADDR_LEN,
			  &local,
			  &locallen,
			  &remote,
			  &remotelen);
    pj_memcpy(accept_overlapped->local, local, locallen);
    pj_memcpy(accept_overlapped->remote, remote, locallen);
    *accept_overlapped->addrlen = locallen;
    *accept_overlapped->newsock_ptr = accept_overlapped->newsock;
    accept_overlapped->operation = 0;
    accept_overlapped->newsock = PJ_INVALID_SOCKET;
}

static void erase_connecting_socket( pj_ioqueue_t *ioqueue, unsigned pos)
{
    pj_ioqueue_key_t *key = ioqueue->connecting_keys[pos];
    HANDLE hEvent = ioqueue->connecting_handles[pos];
    unsigned long optval;

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

    /* Set socket to blocking again. */
    optval = 0;
    if (ioctlsocket((pj_sock_t)key->hnd, FIONBIO, &optval) != 0) {
	DWORD dwStatus;
	dwStatus = WSAGetLastError();
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

	pj_mutex_lock(ioqueue->mutex);
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
	    *connect_err = net_events.iErrorCode[FD_CONNECT_BIT];

	    /* Erase socket from pending connect. */
	    erase_connecting_socket(ioqueue, pos);
	}
	pj_mutex_unlock(ioqueue->mutex);
    }
    return key;
}
#endif


PJ_DEF(pj_ioqueue_t*) pj_ioqueue_create( pj_pool_t *pool, 
					 pj_size_t max_fd )
{
    pj_ioqueue_t *ioq;

    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(max_fd);

    ioq = pj_pool_calloc(pool, 1, sizeof(*ioq));
    ioq->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
    if (ioq->iocp == NULL) {
	return NULL;
    }
    ioq->mutex = pj_mutex_create(pool, NULL, 0);
    if (ioq->mutex == NULL) {
	CloseHandle(ioq->iocp);
	return NULL;
    }

    PJ_LOG(4, ("pjlib", "WinNT IOCP I/O Queue created (%p)", ioq));
    return ioq;
}

PJ_DEF(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioque )
{
    unsigned i;

    /* Destroy events in the pool */
    for (i=0; i<ioque->event_count; ++i) {
	CloseHandle(ioque->event_pool[i]);
    }
    ioque->event_count = 0;

    return CloseHandle(ioque->iocp) ? 0 : -1;
}

PJ_DEF(pj_ioqueue_key_t*) pj_ioqueue_register( pj_pool_t *pool,
					       pj_ioqueue_t *ioque,
					       pj_oshandle_t hnd,
					       void *user_data,
					       const pj_ioqueue_callback *cb)
{
    HANDLE hioq;
    pj_ioqueue_key_t *rec;

    rec = pj_pool_calloc(pool, 1, sizeof(pj_ioqueue_key_t));
    rec->hnd = hnd;
    rec->user_data = user_data;
    pj_memcpy(&rec->cb, cb, sizeof(pj_ioqueue_callback));
#if PJ_HAS_TCP
    rec->accept_overlapped.newsock = PJ_INVALID_SOCKET;
#endif
    hioq = CreateIoCompletionPort(hnd, ioque->iocp, (DWORD)rec, 2);
    if (!hioq) {
	return NULL;
    }
    return rec;
}



PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_t *ioque,
					   pj_ioqueue_key_t *key )
{
#if PJ_HAS_TCP
    if (key->connecting) {
	unsigned pos;

	/* Erase from connecting_handles */
	pj_mutex_lock(ioque->mutex);
	for (pos=0; pos < ioque->connecting_count; ++pos) {
	    if (ioque->connecting_keys[pos] == key) {
		erase_connecting_socket(ioque, pos);
		closesocket(*key->accept_overlapped.newsock_ptr);
		break;
	    }
	}
	pj_mutex_unlock(ioque->mutex);
	key->connecting = 0;
    }
#endif
    return 0;
}

PJ_DEF(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key )
{
    return key->user_data;
}

/*
 * Poll for events.
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioque, const pj_time_val *timeout)
{
    DWORD dwMsec, dwBytesTransfered, dwKey;
    ioqueue_overlapped *ov;
    pj_ioqueue_key_t *key;
    pj_ssize_t size_status;
    BOOL rc;

    /* Check the connecting array. */
#if PJ_HAS_TCP
    key = check_connecting(ioque, &size_status);
    if (key != NULL) {
	key->cb.on_connect_complete(key, (int)size_status);
	return 1;
    }
#endif

    /* Calculate miliseconds timeout for GetQueuedCompletionStatus */
    dwMsec = timeout ? timeout->sec*1000 + timeout->msec : INFINITE;

    /* Poll for completion status. */
    rc = GetQueuedCompletionStatus(ioque->iocp, &dwBytesTransfered, &dwKey,
				   (OVERLAPPED**)&ov, dwMsec);

    /* The return value is:
     * - nonzero if event was dequeued.
     * - zero and ov==NULL if no event was dequeued.
     * - zero and ov!=NULL if event for failed I/O was dequeued.
     */
    if (ov) {
	/* Event was dequeued for either successfull or failed I/O */
	key = (pj_ioqueue_key_t*)dwKey;
	size_status = dwBytesTransfered;
	switch (ov->operation) {
	case PJ_IOQUEUE_OP_READ:
	case PJ_IOQUEUE_OP_RECV_FROM:
	    key->cb.on_read_complete(key, size_status);
	    break;
	case PJ_IOQUEUE_OP_WRITE:
	case PJ_IOQUEUE_OP_SEND_TO:
	    key->cb.on_write_complete(key, size_status);
	    break;
#if PJ_HAS_TCP
	case PJ_IOQUEUE_OP_ACCEPT:
	    /* special case for accept. */
	    ioqueue_on_accept_complete((ioqueue_accept_rec*)ov);
	    key->cb.on_accept_complete(key, 0);
	    break;
#endif
	}
	return 1;
    }

    if (GetLastError()==WAIT_TIMEOUT) {
	/* Check the connecting array. */
#if PJ_HAS_TCP
	key = check_connecting(ioque, &size_status);
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
 * Initiate overlapped ReadFile operation.
 */
PJ_DEF(int) pj_ioqueue_read( pj_ioqueue_t *ioque,
			     pj_ioqueue_key_t *key,
			     void *buffer,
			     pj_size_t buflen)
{
    //BOOL rc;
    int rc;
    DWORD bytesRead;
    DWORD dwFlags = 0;

    PJ_UNUSED_ARG(ioque);

    pj_memset(&key->recv_overlapped, 0, sizeof(key->recv_overlapped));
    key->recv_overlapped.operation = PJ_IOQUEUE_OP_READ;

#if 0
    rc = ReadFile(key->hnd, buffer, buflen, &bytesRead, 
		  &key->recv_overlapped.overlapped);
    if (rc == FALSE) {
	DWORD dwStatus = GetLastError();
	return dwStatus==ERROR_IO_PENDING ? PJ_IOQUEUE_PENDING : -1;
    } else {
	/*
	 * This is workaround to a probable bug in Win2000 (probably NT as well).
	 * Even if 'rc' is zero, which indicates operation has completed,
	 * GetQueuedCompletionStatus still will return the key.
	 * So as work around, we always return PJ_IOQUEUE_PENDING here.
	 */
	//return bytesRead;
	return PJ_IOQUEUE_PENDING;
    }
#endif
    key->recv_overlapped.wsabuf.buf = buffer;
    key->recv_overlapped.wsabuf.len = buflen;
    rc = WSARecv((SOCKET)key->hnd, &key->recv_overlapped.wsabuf, 1, &bytesRead, &dwFlags,
		 &key->recv_overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
	return dwStatus==WSA_IO_PENDING ? PJ_IOQUEUE_PENDING : -1;
    } else {
	// Must always return pending status.
	// See comments on pj_ioqueue_read
	//return bytesRead;
	return PJ_IOQUEUE_PENDING;
    }
}

/*
 * Initiate overlapped RecvFrom operation.
 */
PJ_DEF(int) pj_ioqueue_recvfrom( pj_ioqueue_t *ioque,
				 pj_ioqueue_key_t *key,
				 void *buffer,
				 pj_size_t buflen,
				 pj_sockaddr_t *addr,
				 int *addrlen)
{
    BOOL rc;
    DWORD bytesRead;
    DWORD dwFlags;

    PJ_UNUSED_ARG(ioque);

    pj_memset(&key->recv_overlapped, 0, sizeof(key->recv_overlapped));
    key->recv_overlapped.operation = PJ_IOQUEUE_OP_RECV_FROM;
    key->recv_overlapped.wsabuf.buf = buffer;
    key->recv_overlapped.wsabuf.len = buflen;
    dwFlags = 0;
    rc = WSARecvFrom((SOCKET)key->hnd, &key->recv_overlapped.wsabuf, 1, 
		     &bytesRead, &dwFlags, 
		     addr, addrlen,
		     &key->recv_overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
	return dwStatus==WSA_IO_PENDING ? PJ_IOQUEUE_PENDING : -1;
    } else {
	// Must always return pending status.
	// See comments on pj_ioqueue_read
	//return bytesRead;
	return PJ_IOQUEUE_PENDING;
    }
}

/*
 * Initiate overlapped WriteFile operation.
 */
PJ_DEF(int) pj_ioqueue_write( pj_ioqueue_t *ioque,
			      pj_ioqueue_key_t *key,
			      const void *data,
			      pj_size_t datalen)
{
    //BOOL rc;
    int rc;
    DWORD bytesWritten;

    PJ_UNUSED_ARG(ioque);

    pj_memset(&key->send_overlapped, 0, sizeof(key->send_overlapped));
    key->send_overlapped.operation = PJ_IOQUEUE_OP_WRITE;
    /*
    rc = WriteFile(key->hnd, data, datalen, &bytesWritten, 
		   &key->send_overlapped.overlapped);
    
    if (rc == FALSE) {
	DWORD dwStatus = GetLastError();
	return dwStatus==ERROR_IO_PENDING ? PJ_IOQUEUE_PENDING : -1;
    } else {
	// Must always return pending status.
	// See comments on pj_ioqueue_read
	// return bytesWritten;
	return PJ_IOQUEUE_PENDING;
    }
    */
    key->send_overlapped.wsabuf.buf = (void*)data;
    key->send_overlapped.wsabuf.len = datalen;
    rc = WSASend((SOCKET)key->hnd, &key->send_overlapped.wsabuf, 1, &bytesWritten, 0,
		 &key->send_overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
	return dwStatus==WSA_IO_PENDING ? PJ_IOQUEUE_PENDING : -1;
    } else {
	// Must always return pending status.
	// See comments on pj_ioqueue_read
	//return bytesRead;
	return PJ_IOQUEUE_PENDING;
    }}

/*
 * Initiate overlapped SendTo operation.
 */
PJ_DEF(int) pj_ioqueue_sendto( pj_ioqueue_t *ioque,
			       pj_ioqueue_key_t *key,
			       const void *data,
			       pj_size_t datalen,
			       const pj_sockaddr_t *addr,
			       int addrlen)
{
    BOOL rc;
    DWORD bytesSent;
    DWORD dwFlags;

    PJ_UNUSED_ARG(ioque);

    pj_memset(&key->send_overlapped, 0, sizeof(key->send_overlapped));
    key->send_overlapped.operation = PJ_IOQUEUE_OP_SEND_TO;
    key->send_overlapped.wsabuf.buf = (char*)data;
    key->send_overlapped.wsabuf.len = datalen;
    dwFlags = 0;
    rc = WSASendTo((SOCKET)key->hnd, &key->send_overlapped.wsabuf, 1, 
		   &bytesSent, dwFlags, addr, 
		   addrlen, &key->send_overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
	return dwStatus==WSA_IO_PENDING ? PJ_IOQUEUE_PENDING : -1;
    } else {
	// Must always return pending status.
	// See comments on pj_ioqueue_read
	// return bytesSent;
	return PJ_IOQUEUE_PENDING;
    }
}

#if PJ_HAS_TCP

/*
 * Initiate overlapped accept() operation.
 */
PJ_DEF(int) pj_ioqueue_accept( pj_ioqueue_t *ioqueue,
			       pj_ioqueue_key_t *key,
			       pj_sock_t *new_sock,
			       pj_sockaddr_t *local,
			       pj_sockaddr_t *remote,
			       int *addrlen)
{
    BOOL rc;
    DWORD bytesReceived;

    PJ_UNUSED_ARG(ioqueue)

    pj_assert(key->accept_overlapped.operation == 0);

    if (key->accept_overlapped.newsock == PJ_INVALID_SOCKET) {
	pj_sock_t sock;
	sock = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, PJ_SOCK_ASYNC);
	if (sock == PJ_INVALID_SOCKET) {
	    return -1;
	}
	key->accept_overlapped.newsock = sock;
    }
    key->accept_overlapped.operation = PJ_IOQUEUE_OP_ACCEPT;
    key->accept_overlapped.addrlen = addrlen;
    key->accept_overlapped.local = local;
    key->accept_overlapped.remote = remote;
    key->accept_overlapped.newsock_ptr = new_sock;
    pj_memset(&key->accept_overlapped.overlapped, 0, 
	      sizeof(key->accept_overlapped.overlapped));

    rc = AcceptEx( (SOCKET)key->hnd, (SOCKET)key->accept_overlapped.newsock,
		   key->accept_overlapped.accept_buf,
		   0, ACCEPT_ADDR_LEN, ACCEPT_ADDR_LEN,
		   &bytesReceived,
		   &key->accept_overlapped.overlapped);

    if (rc == TRUE) {
	ioqueue_on_accept_complete(&key->accept_overlapped);
	key->cb.on_accept_complete(key, 0);
	return 0;
    } else {
	DWORD dwStatus = WSAGetLastError();
	return dwStatus==WSA_IO_PENDING ? PJ_IOQUEUE_PENDING : -1;
    }
}


/*
 * Initiate overlapped connect() operation (well, it's non-blocking actually,
 * since there's no overlapped version of connect()).
 */
PJ_DEF(pj_status_t) pj_ioqueue_connect( pj_ioqueue_t *ioqueue,
					pj_ioqueue_key_t *key,
					const pj_sockaddr_t *addr,
					int addrlen )
{
    unsigned long optval = 1;
    HANDLE hEvent;

    /* Set socket to non-blocking. */
    if (ioctlsocket((pj_sock_t)key->hnd, PJ_FIONBIO, &optval) != PJ_OK) {
	DWORD dwStatus;
	dwStatus = WSAGetLastError();
	return -1;
    }

    /* Initiate connect() */
    if (connect((pj_sock_t)key->hnd, addr, addrlen) != 0) {
	DWORD dwStatus;
	dwStatus = WSAGetLastError();
	if (dwStatus != WSAEWOULDBLOCK) {
	    /* Permanent error */
	    return -1;
	} else {
	    /* Pending operation. This is what we're looking for. */
	}
    } else {
	/* Connect has completed immediately! */
	/* Restore to blocking mode. */
	optval = 0;
	if (ioctlsocket((pj_sock_t)key->hnd, FIONBIO, &optval) != PJ_OK) {
	    return -1;
	}

	key->cb.on_connect_complete(key, 0);
	return PJ_OK;
    }

    /* Add to the array of connecting socket to be polled */
    pj_mutex_lock(ioqueue->mutex);

    if (ioqueue->connecting_count >= MAXIMUM_WAIT_OBJECTS) {
	pj_mutex_unlock(ioqueue->mutex);
	return -1;
    }

    /* Get or create event object. */
    if (ioqueue->event_count) {
	hEvent = ioqueue->event_pool[ioqueue->event_count - 1];
	--ioqueue->event_count;
    } else {
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL) {
	    pj_mutex_unlock(ioqueue->mutex);
	    return -1;
	}
    }

    /* Mark key as connecting.
     * We can't use array index since key can be removed dynamically. 
     */
    key->connecting = 1;

    /* Associate socket events to the event object. */
    if (WSAEventSelect((pj_sock_t)key->hnd, hEvent, FD_CONNECT) != 0) {
	CloseHandle(hEvent);
	pj_mutex_unlock(ioqueue->mutex);
	return -1;
    }

    /* Add to array. */
    ioqueue->connecting_keys[ ioqueue->connecting_count ] = key;
    ioqueue->connecting_handles[ ioqueue->connecting_count ] = hEvent;
    ioqueue->connecting_count++;

    pj_mutex_unlock(ioqueue->mutex);

    return PJ_IOQUEUE_PENDING;
}
#endif	/* #if PJ_HAS_TCP */

#endif	/* #if PJ_IOQUEUE_USE_WIN32_IOCP */


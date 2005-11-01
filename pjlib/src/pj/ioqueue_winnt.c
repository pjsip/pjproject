/* $Id$
 *
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
    pj_memcpy(accept_overlapped->local, local, locallen);
    pj_memcpy(accept_overlapped->remote, remote, locallen);
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
	    *connect_err = net_events.iErrorCode[FD_CONNECT_BIT];

	    /* Erase socket from pending connect. */
	    erase_connecting_socket(ioqueue, pos);
	}
	pj_lock_release(ioqueue->lock);
    }
    return key;
}
#endif


PJ_DEF(pj_status_t) pj_ioqueue_create( pj_pool_t *pool, 
				       pj_size_t max_fd,
				       int max_threads,
				       pj_ioqueue_t **ioqueue)
{
    pj_ioqueue_t *ioq;
    pj_status_t rc;

    PJ_UNUSED_ARG(max_fd);
    PJ_ASSERT_RETURN(pool && ioqueue, PJ_EINVAL);

    ioq = pj_pool_zalloc(pool, sizeof(*ioq));
    ioq->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, max_threads);
    if (ioq->iocp == NULL)
	return PJ_RETURN_OS_ERROR(GetLastError());

    rc = pj_lock_create_simple_mutex(pool, NULL, &ioq->lock);
    if (rc != PJ_SUCCESS) {
	CloseHandle(ioq->iocp);
	return rc;
    }

    ioq->auto_delete_lock = PJ_TRUE;

    *ioqueue = ioq;

    PJ_LOG(4, ("pjlib", "WinNT IOCP I/O Queue created (%p)", ioq));
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioque )
{
    unsigned i;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(ioque, PJ_EINVAL);

    /* Destroy events in the pool */
    for (i=0; i<ioque->event_count; ++i) {
	CloseHandle(ioque->event_pool[i]);
    }
    ioque->event_count = 0;

    if (ioque->auto_delete_lock)
        pj_lock_destroy(ioque->lock);

    if (CloseHandle(ioque->iocp) == TRUE)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(GetLastError());
}

PJ_DEF(pj_status_t) pj_ioqueue_set_lock( pj_ioqueue_t *ioque, 
					 pj_lock_t *lock,
					 pj_bool_t auto_delete )
{
    PJ_ASSERT_RETURN(ioque && lock, PJ_EINVAL);

    if (ioque->auto_delete_lock) {
        pj_lock_destroy(ioque->lock);
    }

    ioque->lock = lock;
    ioque->auto_delete_lock = auto_delete;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					      pj_ioqueue_t *ioque,
					      pj_sock_t hnd,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
					      pj_ioqueue_key_t **key )
{
    HANDLE hioq;
    pj_ioqueue_key_t *rec;

    PJ_ASSERT_RETURN(pool && ioque && cb && key, PJ_EINVAL);

    rec = pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));
    rec->hnd = (HANDLE)hnd;
    rec->user_data = user_data;
    pj_memcpy(&rec->cb, cb, sizeof(pj_ioqueue_callback));
#if PJ_HAS_TCP
    rec->accept_overlapped.newsock = PJ_INVALID_SOCKET;
#endif
    hioq = CreateIoCompletionPort((HANDLE)hnd, ioque->iocp, (DWORD)rec, 0);
    if (!hioq) {
	return PJ_RETURN_OS_ERROR(GetLastError());
    }

    *key = rec;
    return PJ_SUCCESS;
}



PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_t *ioque,
					   pj_ioqueue_key_t *key )
{
    PJ_ASSERT_RETURN(ioque && key, PJ_EINVAL);

#if PJ_HAS_TCP
    if (key->connecting) {
	unsigned pos;

	/* Erase from connecting_handles */
	pj_lock_acquire(ioque->lock);
	for (pos=0; pos < ioque->connecting_count; ++pos) {
	    if (ioque->connecting_keys[pos] == key) {
		erase_connecting_socket(ioque, pos);
                if (key->accept_overlapped.newsock_ptr) {
                    /* ??? shouldn't it be newsock instead of newsock_ptr??? */
		    closesocket(*key->accept_overlapped.newsock_ptr);
                }
		break;
	    }
	}
	pj_lock_release(ioque->lock);
	key->connecting = 0;
    }
#endif
    return PJ_SUCCESS;
}

PJ_DEF(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key )
{
    PJ_ASSERT_RETURN(key, NULL);
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

    PJ_ASSERT_RETURN(ioque, -PJ_EINVAL);

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
	case PJ_IOQUEUE_OP_RECV:
	case PJ_IOQUEUE_OP_RECV_FROM:
            key->recv_overlapped.operation = 0;
            if (key->cb.on_read_complete)
	        key->cb.on_read_complete(key, size_status);
	    break;
	case PJ_IOQUEUE_OP_WRITE:
	case PJ_IOQUEUE_OP_SEND:
	case PJ_IOQUEUE_OP_SEND_TO:
            key->send_overlapped.operation = 0;
            if (key->cb.on_write_complete)
	        key->cb.on_write_complete(key, size_status);
	    break;
#if PJ_HAS_TCP
	case PJ_IOQUEUE_OP_ACCEPT:
	    /* special case for accept. */
	    ioqueue_on_accept_complete((ioqueue_accept_rec*)ov);
            if (key->cb.on_accept_complete)
	        key->cb.on_accept_complete(key, key->accept_overlapped.newsock,
                                           0);
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
 * pj_ioqueue_read()
 *
 * Initiate overlapped ReadFile operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_read( pj_ioqueue_t *ioque,
				     pj_ioqueue_key_t *key,
				     void *buffer,
				     pj_size_t buflen)
{
    BOOL rc;
    DWORD bytesRead;

    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ioque);

    if (key->recv_overlapped.operation != PJ_IOQUEUE_OP_NONE) {
        pj_assert(!"Operation already pending for this descriptor");
        return PJ_EBUSY;
    }

    pj_memset(&key->recv_overlapped, 0, sizeof(key->recv_overlapped));
    key->recv_overlapped.operation = PJ_IOQUEUE_OP_READ;

    rc = ReadFile(key->hnd, buffer, buflen, &bytesRead, 
		  &key->recv_overlapped.overlapped);
    if (rc == FALSE) {
	DWORD dwStatus = GetLastError();
	if (dwStatus==ERROR_IO_PENDING)
            return PJ_EPENDING;
        else
            return PJ_STATUS_FROM_OS(dwStatus);
    } else {
	/*
	 * This is workaround to a probable bug in Win2000 (probably NT too).
	 * Even if 'rc' is TRUE, which indicates operation has completed,
	 * GetQueuedCompletionStatus still will return the key.
	 * So as work around, we always return PJ_EPENDING here.
	 */
	return PJ_EPENDING;
    }
}

/*
 * pj_ioqueue_recv()
 *
 * Initiate overlapped WSARecv() operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recv(  pj_ioqueue_t *ioque,
				      pj_ioqueue_key_t *key,
				      void *buffer,
				      pj_size_t buflen,
				      unsigned flags )
{
    int rc;
    DWORD bytesRead;
    DWORD dwFlags = 0;

    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ioque);

    if (key->recv_overlapped.operation != PJ_IOQUEUE_OP_NONE) {
        pj_assert(!"Operation already pending for this socket");
        return PJ_EBUSY;
    }

    pj_memset(&key->recv_overlapped, 0, sizeof(key->recv_overlapped));
    key->recv_overlapped.operation = PJ_IOQUEUE_OP_READ;

    key->recv_overlapped.wsabuf.buf = buffer;
    key->recv_overlapped.wsabuf.len = buflen;

    dwFlags = flags;

    rc = WSARecv((SOCKET)key->hnd, &key->recv_overlapped.wsabuf, 1, 
                 &bytesRead, &dwFlags,
		 &key->recv_overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
	if (dwStatus==WSA_IO_PENDING)
            return PJ_EPENDING;
        else
            return PJ_STATUS_FROM_OS(dwStatus);
    } else {
	/* Must always return pending status.
	 * See comments on pj_ioqueue_read
	 * return bytesRead;
         */
	return PJ_EPENDING;
    }
}

/*
 * pj_ioqueue_recvfrom()
 *
 * Initiate overlapped RecvFrom() operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_t *ioque,
					 pj_ioqueue_key_t *key,
					 void *buffer,
					 pj_size_t buflen,
                                         unsigned flags,
					 pj_sockaddr_t *addr,
					 int *addrlen)
{
    BOOL rc;
    DWORD bytesRead;
    DWORD dwFlags;

    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ioque);

    if (key->recv_overlapped.operation != PJ_IOQUEUE_OP_NONE) {
        pj_assert(!"Operation already pending for this socket");
        return PJ_EBUSY;
    }

    pj_memset(&key->recv_overlapped, 0, sizeof(key->recv_overlapped));
    key->recv_overlapped.operation = PJ_IOQUEUE_OP_RECV_FROM;
    key->recv_overlapped.wsabuf.buf = buffer;
    key->recv_overlapped.wsabuf.len = buflen;
    dwFlags = flags;
    rc = WSARecvFrom((SOCKET)key->hnd, &key->recv_overlapped.wsabuf, 1, 
		     &bytesRead, &dwFlags, 
		     addr, addrlen,
		     &key->recv_overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
	if (dwStatus==WSA_IO_PENDING)
            return PJ_EPENDING;
        else
            return PJ_STATUS_FROM_OS(dwStatus);
    } else {
	/* Must always return pending status.
	 * See comments on pj_ioqueue_read
	 * return bytesRead;
         */
	return PJ_EPENDING;
    }
}

/*
 * pj_ioqueue_write()
 *
 * Initiate overlapped WriteFile() operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_write( pj_ioqueue_t *ioque,
				      pj_ioqueue_key_t *key,
				      const void *data,
				      pj_size_t datalen)
{
    BOOL rc;
    DWORD bytesWritten;

    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ioque);

    if (key->send_overlapped.operation != PJ_IOQUEUE_OP_NONE) {
        pj_assert(!"Operation already pending for this descriptor");
        return PJ_EBUSY;
    }

    pj_memset(&key->send_overlapped, 0, sizeof(key->send_overlapped));
    key->send_overlapped.operation = PJ_IOQUEUE_OP_WRITE;
    rc = WriteFile(key->hnd, data, datalen, &bytesWritten, 
		   &key->send_overlapped.overlapped);
    
    if (rc == FALSE) {
	DWORD dwStatus = GetLastError();
	if (dwStatus==ERROR_IO_PENDING)
            return PJ_EPENDING;
        else
            return PJ_STATUS_FROM_OS(dwStatus);
    } else {
	/* Must always return pending status.
	 * See comments on pj_ioqueue_read
	 * return bytesWritten;
         */
	return PJ_EPENDING;
    }
}


/*
 * pj_ioqueue_send()
 *
 * Initiate overlapped Send operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_send(  pj_ioqueue_t *ioque,
				      pj_ioqueue_key_t *key,
				      const void *data,
				      pj_size_t datalen,
				      unsigned flags )
{
    int rc;
    DWORD bytesWritten;
    DWORD dwFlags;

    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ioque);

    if (key->send_overlapped.operation != PJ_IOQUEUE_OP_NONE) {
        pj_assert(!"Operation already pending for this socket");
        return PJ_EBUSY;
    }

    pj_memset(&key->send_overlapped, 0, sizeof(key->send_overlapped));
    key->send_overlapped.operation = PJ_IOQUEUE_OP_WRITE;
    key->send_overlapped.wsabuf.buf = (void*)data;
    key->send_overlapped.wsabuf.len = datalen;
    dwFlags = flags;
    rc = WSASend((SOCKET)key->hnd, &key->send_overlapped.wsabuf, 1,
                 &bytesWritten,  dwFlags,
		 &key->send_overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
        if (dwStatus==WSA_IO_PENDING)
            return PJ_EPENDING;
        else
            return PJ_STATUS_FROM_OS(dwStatus);
    } else {
	/* Must always return pending status.
	 * See comments on pj_ioqueue_read
	 * return bytesRead;
         */
	return PJ_EPENDING;
    }
}


/*
 * pj_ioqueue_sendto()
 *
 * Initiate overlapped SendTo operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_t *ioque,
				       pj_ioqueue_key_t *key,
				       const void *data,
				       pj_size_t datalen,
                                       unsigned flags,
				       const pj_sockaddr_t *addr,
				       int addrlen)
{
    BOOL rc;
    DWORD bytesSent;
    DWORD dwFlags;

    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ioque);

    if (key->send_overlapped.operation != PJ_IOQUEUE_OP_NONE) {
        pj_assert(!"Operation already pending for this socket");
        return PJ_EBUSY;
    }

    pj_memset(&key->send_overlapped, 0, sizeof(key->send_overlapped));
    key->send_overlapped.operation = PJ_IOQUEUE_OP_SEND_TO;
    key->send_overlapped.wsabuf.buf = (char*)data;
    key->send_overlapped.wsabuf.len = datalen;
    dwFlags = flags;
    rc = WSASendTo((SOCKET)key->hnd, &key->send_overlapped.wsabuf, 1, 
		   &bytesSent, dwFlags, addr, 
		   addrlen, &key->send_overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
	if (dwStatus==WSA_IO_PENDING)
            return PJ_EPENDING;
        else
            return PJ_STATUS_FROM_OS(dwStatus);
    } else {
	// Must always return pending status.
	// See comments on pj_ioqueue_read
	// return bytesSent;
	return PJ_EPENDING;
    }
}

#if PJ_HAS_TCP

/*
 * pj_ioqueue_accept()
 *
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
    pj_status_t status;

    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ioqueue);

    if (key->accept_overlapped.operation != PJ_IOQUEUE_OP_NONE) {
        pj_assert(!"Operation already pending for this socket");
        return PJ_EBUSY;
    }

    if (key->accept_overlapped.newsock == PJ_INVALID_SOCKET) {
	pj_sock_t sock;
	status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, &sock);
	if (status != PJ_SUCCESS)
	    return status;

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
        if (key->cb.on_accept_complete)
	    key->cb.on_accept_complete(key, key->accept_overlapped.newsock, 0);
	return PJ_SUCCESS;
    } else {
	DWORD dwStatus = WSAGetLastError();
	if (dwStatus==WSA_IO_PENDING)
            return PJ_EPENDING;
        else
            return PJ_STATUS_FROM_OS(dwStatus);
    }
}


/*
 * pj_ioqueue_connect()
 *
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

    PJ_CHECK_STACK();

    /* Set socket to non-blocking. */
    if (ioctlsocket((pj_sock_t)key->hnd, FIONBIO, &optval) != 0) {
	return PJ_RETURN_OS_ERROR(WSAGetLastError());
    }

    /* Initiate connect() */
    if (connect((pj_sock_t)key->hnd, addr, addrlen) != 0) {
	DWORD dwStatus;
	dwStatus = WSAGetLastError();
	if (dwStatus != WSAEWOULDBLOCK) {
	    /* Permanent error */
	    return PJ_RETURN_OS_ERROR(dwStatus);
	} else {
	    /* Pending operation. This is what we're looking for. */
	}
    } else {
	/* Connect has completed immediately! */
	/* Restore to blocking mode. */
	optval = 0;
	if (ioctlsocket((pj_sock_t)key->hnd, FIONBIO, &optval) != 0) {
	    return PJ_RETURN_OS_ERROR(WSAGetLastError());
	}

	key->cb.on_connect_complete(key, 0);
	return PJ_SUCCESS;
    }

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


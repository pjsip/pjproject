/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/ioqueue_select.c,v 1.1 2005/12/02 20:02:29 nn Exp $ */
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
#include <pj/os.h>
#include <pj/log.h>
#include <pj/list.h>
#include <pj/pool.h>
#include <pj/string.h>

#ifdef PJ_LINUX
# include <errno.h>
#endif

/*
 * This is the top level ifdef, which will enable/disable compilation of this file
 * depending on whether PJ_IOQUEUE_USE_SELECT macro is set!
 */
#if defined(PJ_IOQUEUE_USE_SELECT) && PJ_IOQUEUE_USE_SELECT!=0


#define FD_SETSIZE  PJ_IOQUEUE_MAX_HANDLES
#define THIS_FILE   "ioq_select"

#include <pj/sock.h>

#define PJ_IOQUEUE_IS_READ_OP(op)   \
	((op & PJ_IOQUEUE_OP_READ)  || (op & PJ_IOQUEUE_OP_RECV_FROM))
#define PJ_IOQUEUE_IS_WRITE_OP(op)  \
	((op & PJ_IOQUEUE_OP_WRITE) || (op & PJ_IOQUEUE_OP_SEND_TO))

#if PJ_HAS_TCP
#  define PJ_IOQUEUE_IS_ACCEPT_OP(op)	(op & PJ_IOQUEUE_OP_ACCEPT)
#  define PJ_IOQUEUE_IS_CONNECT_OP(op)	(op & PJ_IOQUEUE_OP_CONNECT)
#else
#  define PJ_IOQUEUE_IS_ACCEPT_OP(op)	0
#  define PJ_IOQUEUE_IS_CONNECT_OP(op)	0
#endif

struct pj_ioqueue_key_t
{
    PJ_DECL_LIST_MEMBER(struct pj_ioqueue_key_t)
    pj_sock_t		    fd;
    pj_ioqueue_operation_e  op;
    void		   *user_data;
    pj_ioqueue_callback	    cb;

    void		   *rd_buf;
    pj_size_t		    rd_buflen;
    void		   *wr_buf;
    pj_size_t		    wr_buflen;

    pj_sockaddr_t	   *rmt_addr;
    int			   *rmt_addrlen;

    pj_sockaddr_t	   *local_addr;
    int			   *local_addrlen;

    pj_sock_t		   *accept_fd;
};

struct pj_ioqueue_t
{
    pj_mutex_t	     *mutex;
    unsigned		max, count;
    pj_ioqueue_key_t	hlist;
    pj_fdset_t		rfdset;
    pj_fdset_t		wfdset;
#if PJ_HAS_TCP
    pj_fdset_t		xfdset;
#endif
};

PJ_DEF(pj_ioqueue_t*) pj_ioqueue_create(pj_pool_t *pool, pj_size_t max_fd)
{
    pj_ioqueue_t *ioque;

    if (max_fd > PJ_IOQUEUE_MAX_HANDLES) {
	PJ_LOG(1,("ioqueue", "max_fd too large! Can't create ioqueue."));
	return NULL;
    }

    ioque = pj_pool_alloc(pool, sizeof(pj_ioqueue_t));
    ioque->max = max_fd;
    ioque->count = 0;
    PJ_FD_ZERO(&ioque->rfdset);
    PJ_FD_ZERO(&ioque->wfdset);
#if PJ_HAS_TCP
    PJ_FD_ZERO(&ioque->xfdset);
#endif
    pj_list_init(&ioque->hlist);

    ioque->mutex = pj_mutex_create(pool, "ioq%p", PJ_MUTEX_SIMPLE);
    if (!ioque->mutex) {
	PJ_LOG(1,("ioqueue", "Mutex creation failed!"));
	return NULL;
    }

    PJ_LOG(4, ("pjlib", "select() I/O Queue created (%p)", ioque));
    return ioque;
}

PJ_DEF(pj_status_t) pj_ioqueue_destroy(pj_ioqueue_t *ioque)
{
    pj_mutex_destroy(ioque->mutex);
    return 0;
}

PJ_DEF(pj_ioqueue_key_t*) pj_ioqueue_register( pj_pool_t *pool,
					       pj_ioqueue_t *ioque,
					       pj_oshandle_t sock,
					       void *user_data,
					       const pj_ioqueue_callback *cb)
{
    pj_ioqueue_key_t *key = NULL;
    pj_uint32_t value;
    
    pj_mutex_lock(ioque->mutex);

    if (ioque->count >= ioque->max)
	goto on_return;

    /* Set socket to nonblocking. */
    value = 1;
    if (pj_sock_ioctl((pj_sock_t)sock, PJ_FIONBIO, &value)) {
	PJ_PERROR(("ioqueue", "Error setting FIONBIO"));
	goto on_return;
    }

    /* Create key. */
    key = (pj_ioqueue_key_t*)pj_pool_calloc(pool, 1, sizeof(pj_ioqueue_key_t));
    key->fd = (pj_sock_t)sock;
    key->user_data = user_data;

    /* Save callback. */
    pj_memcpy(&key->cb, cb, sizeof(pj_ioqueue_callback));

    /* Register */
    pj_list_insert_before(&ioque->hlist, key);
    ++ioque->count;

on_return:
    pj_mutex_unlock(ioque->mutex);
    return key;
}

PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_t *ioque,
					   pj_ioqueue_key_t *key)
{
    pj_mutex_lock(ioque->mutex);

    pj_assert(ioque->count > 0);
    --ioque->count;
    pj_list_erase(key);
    PJ_FD_CLR(key->fd, &ioque->rfdset);
    PJ_FD_CLR(key->fd, &ioque->wfdset);
#if PJ_HAS_TCP
    PJ_FD_CLR(key->fd, &ioque->xfdset);
#endif

    pj_mutex_unlock(ioque->mutex);
    return 0;
}

PJ_DEF(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key )
{
    return key->user_data;
}

PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioque, const pj_time_val *timeout)
{
    pj_fdset_t rfdset, wfdset, xfdset;
    int rc;
    pj_ioqueue_key_t *h;
    
    /* Copy ioqueue's fd_set to local variables. */
    pj_mutex_lock(ioque->mutex);

    rfdset = ioque->rfdset;
    wfdset = ioque->wfdset;
#if PJ_HAS_TCP
    xfdset = ioque->xfdset;
#else
    PJ_FD_ZERO(&xfdset);
#endif

    /* Unlock ioqueue before select(). */
    pj_mutex_unlock(ioque->mutex);

    rc = pj_sock_select(FD_SETSIZE, &rfdset, &wfdset, &xfdset, timeout);
    
    if (rc <= 0)
	return rc;

    /* Lock ioqueue again before scanning for signalled sockets. */
    pj_mutex_lock(ioque->mutex);

#if PJ_HAS_TCP
    /* Scan for exception socket */
    h = ioque->hlist.next;
    for ( ; h!=&ioque->hlist; h = h->next) {
	if ((h->op & PJ_IOQUEUE_OP_CONNECT) && PJ_FD_ISSET(h->fd, &xfdset))
	    break;
    }
    if (h != &ioque->hlist) {
	/* 'connect()' should be the only operation. */
	pj_assert((h->op == PJ_IOQUEUE_OP_CONNECT));

	/* Clear operation. */
	h->op &= ~(PJ_IOQUEUE_OP_CONNECT);
	PJ_FD_CLR(h->fd, &ioque->wfdset);
	PJ_FD_CLR(h->fd, &ioque->xfdset);

	/* Unlock I/O queue before calling callback. */
	pj_mutex_unlock(ioque->mutex);

	/* Call callback. */
	(*h->cb.on_connect_complete)(h, -1);
	return 1;
    }
#endif	/* PJ_HAS_TCP */

    /* Scan for writable socket  */
    h = ioque->hlist.next;
    for ( ; h!=&ioque->hlist; h = h->next) {
	if ((PJ_IOQUEUE_IS_WRITE_OP(h->op) || PJ_IOQUEUE_IS_CONNECT_OP(h->op)) && PJ_FD_ISSET(h->fd, &wfdset))
	    break;
    }
    if (h != &ioque->hlist) {
	pj_assert(PJ_IOQUEUE_IS_WRITE_OP(h->op) || PJ_IOQUEUE_IS_CONNECT_OP(h->op));

#if PJ_HAS_TCP
	if ((h->op & PJ_IOQUEUE_OP_CONNECT)) {
	    /* Completion of connect() operation */
	    pj_ssize_t bytes_transfered;

#if defined(PJ_LINUX)
	    /* from connect(2): 
		* On Linux, use getsockopt to read the SO_ERROR option at
		* level SOL_SOCKET to determine whether connect() completed
		* successfully (if SO_ERROR is zero).
		*/
	    int value;
	    socklen_t vallen = sizeof(value);
	    int rc = getsockopt(h->fd, SOL_SOCKET, SO_ERROR, &value, &vallen);
	    if (rc != 0) {
		/* Argh!! What to do now??? 
		    * Just indicate that the socket is connected. The
		    * application will get error as soon as it tries to use
		    * the socket to send/receive.
		    */
		PJ_PERROR(("ioqueue", "Unable to determine connect() status"));
		bytes_transfered = 0;
	    } else {
		bytes_transfered = value;
	    }
#elif defined(PJ_WIN32)
	    bytes_transfered = 0; /* success */
#else
#  error "Got to check this one!"
#endif

	    /* Clear operation. */
	    h->op &= (~PJ_IOQUEUE_OP_CONNECT);
	    PJ_FD_CLR(h->fd, &ioque->wfdset);
	    PJ_FD_CLR(h->fd, &ioque->xfdset);

	    /* Unlock mutex before calling callback. */
	    pj_mutex_unlock(ioque->mutex);

	    /* Call callback. */
	    (*h->cb.on_connect_complete)(h, bytes_transfered);

	    return 1;

	} else 
#endif /* PJ_HAS_TCP */
	{
	    /* Completion of write(), send(), or sendto() operation. */

	    /* Clear operation. */
	    h->op &= ~(PJ_IOQUEUE_OP_WRITE | PJ_IOQUEUE_OP_SEND_TO);
	    PJ_FD_CLR(h->fd, &ioque->wfdset);

	    /* Unlock mutex before calling callback. */
	    pj_mutex_unlock(ioque->mutex);

	    /* Call callback. */
	    /* All data must have been sent? */
	    (*h->cb.on_write_complete)(h, h->wr_buflen);

	    return 1;
	}

	/* Unreached. */
    }

    /* Scan for readable socket. */
    h = ioque->hlist.next;
    for ( ; h!=&ioque->hlist; h = h->next) {
	if ((PJ_IOQUEUE_IS_READ_OP(h->op) || PJ_IOQUEUE_IS_ACCEPT_OP(h->op)) && 
	    PJ_FD_ISSET(h->fd, &rfdset))
	    break;
    }
    if (h != &ioque->hlist) {
	pj_assert(PJ_IOQUEUE_IS_READ_OP(h->op) || PJ_IOQUEUE_IS_ACCEPT_OP(h->op));
#	if PJ_HAS_TCP
	if ((h->op & PJ_IOQUEUE_OP_ACCEPT)) {
	    /* accept() must be the only operation specified on server socket */
	    pj_assert(h->op == PJ_IOQUEUE_OP_ACCEPT);

	    *h->accept_fd = pj_sock_accept(h->fd, h->rmt_addr, h->rmt_addrlen);
	    if (*h->accept_fd == PJ_INVALID_SOCKET) {
		rc = -1;
	    } else if (h->local_addr) {
		rc = pj_sock_getsockname(*h->accept_fd, h->local_addr, h->local_addrlen);
	    } else {
		rc = 0;
	    }

	    h->op &= ~(PJ_IOQUEUE_OP_ACCEPT);
	    PJ_FD_CLR(h->fd, &ioque->rfdset);

	    /* Unlock mutex before calling callback. */
	    pj_mutex_unlock(ioque->mutex);

	    /* Call callback. */
	    (*h->cb.on_accept_complete)(h, rc);

	    return 1;

	} else 
#	endif
	if ((h->op & PJ_IOQUEUE_OP_RECV_FROM)) {
	    rc = pj_sock_recvfrom(h->fd, h->rd_buf, h->rd_buflen, 0,
				    h->rmt_addr, h->rmt_addrlen);
	} else {
	    rc = pj_sock_recv(h->fd, h->rd_buf, h->rd_buflen, 0);
	}
	
	if (rc < 0) {
	    pj_status_t sock_err = -1;
#	    if defined(_WIN32)
	    /* On Win32, for UDP, WSAECONNRESET on the receive side 
	     * indicates that previous sending has triggered ICMP Port 
	     * Unreachable message.
	     * But we wouldn't know at this point which one of previous 
	     * key that has triggered the error, since UDP socket can
	     * be shared!
	     * So we'll just ignore it!
	     */

	    sock_err = pj_sock_getlasterror();
	    if (sock_err == PJ_ECONNRESET) {
		pj_mutex_unlock(ioque->mutex);
		PJ_LOG(4,(THIS_FILE, "Received ICMP port unreachable on key=%p (ignored)!", h));
		return 0;
	    } 
#	    endif

	    PJ_LOG(4, (THIS_FILE, "socket recv error on key %p, rc=%d, err=%d", h, rc, sock_err));
	}

	h->op &= ~(PJ_IOQUEUE_OP_READ | PJ_IOQUEUE_OP_RECV_FROM);
	PJ_FD_CLR(h->fd, &ioque->rfdset);

	/* Unlock mutex before callback. */
	pj_mutex_unlock(ioque->mutex);

	/* Call callback. */
	(*h->cb.on_read_complete)(h, rc);
	return 1;
    }

    /* Shouldn't happen. */
    /* For strange reason on WinXP select() can return 1 while there is no
     * fd_set signaled. */
    /* pj_assert(0); */

    rc = 0;

    pj_mutex_unlock(ioque->mutex);
    return rc;
}

PJ_DEF(int) pj_ioqueue_read( pj_ioqueue_t *ioque,
			     pj_ioqueue_key_t *key,
			     void *buffer,
			     pj_size_t buflen)
{
    pj_mutex_lock(ioque->mutex);

    key->op |= PJ_IOQUEUE_OP_READ;
    key->rd_buf = buffer;
    key->rd_buflen = buflen;
    PJ_FD_SET(key->fd, &ioque->rfdset);

    pj_mutex_unlock(ioque->mutex);
    return PJ_IOQUEUE_PENDING;
}

PJ_DEF(int) pj_ioqueue_recvfrom( pj_ioqueue_t *ioque,
				 pj_ioqueue_key_t *key,
				 void *buffer,
				 pj_size_t buflen,
				 pj_sockaddr_t *addr,
				 int *addrlen)
{
    pj_mutex_lock(ioque->mutex);

    key->op |= PJ_IOQUEUE_OP_RECV_FROM;
    key->rd_buf = buffer;
    key->rd_buflen = buflen;
    key->rmt_addr = addr;
    key->rmt_addrlen = addrlen;
    PJ_FD_SET(key->fd, &ioque->rfdset);

    pj_mutex_unlock(ioque->mutex);
    return PJ_IOQUEUE_PENDING;
}

PJ_DEF(int) pj_ioqueue_write( pj_ioqueue_t *ioque,
			      pj_ioqueue_key_t *key,
			      const void *data,
			      pj_size_t datalen)
{
    if (pj_sock_send(key->fd, data, datalen, 0) != (pj_ssize_t)datalen)
	return -1;

    pj_mutex_lock(ioque->mutex);

    key->op |= PJ_IOQUEUE_OP_WRITE;
    key->wr_buf = NULL;
    key->wr_buflen = datalen;
    PJ_FD_SET(key->fd, &ioque->wfdset);

    pj_mutex_unlock(ioque->mutex);

    return PJ_IOQUEUE_PENDING;
}

PJ_DEF(int) pj_ioqueue_sendto( pj_ioqueue_t *ioque,
			       pj_ioqueue_key_t *key,
			       const void *data,
			       pj_size_t datalen,
			       const pj_sockaddr_t *addr,
			       int addrlen)
{
    if (pj_sock_sendto(key->fd, data, datalen, 0, addr, addrlen) != (pj_ssize_t)datalen)
	return -1;

    pj_mutex_lock(ioque->mutex);

    key->op |= PJ_IOQUEUE_OP_SEND_TO;
    key->wr_buf = NULL;
    key->wr_buflen = datalen;
    PJ_FD_SET(key->fd, &ioque->wfdset);

    pj_mutex_unlock(ioque->mutex);
    return PJ_IOQUEUE_PENDING;
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
    /* check parameters. All must be specified! */
    pj_assert(ioqueue && key && new_sock && local && remote && addrlen);

    /* Server socket must have no other operation! */
    pj_assert(key->op == 0);
    
    pj_mutex_lock(ioqueue->mutex);

    key->op = PJ_IOQUEUE_OP_ACCEPT;
    key->accept_fd = new_sock;
    key->rmt_addr = remote;
    key->rmt_addrlen = addrlen;
    key->local_addr = local;
    key->local_addrlen = addrlen;   /* use same addr. as rmt_addrlen */

    PJ_FD_SET(key->fd, &ioqueue->rfdset);

    pj_mutex_unlock(ioqueue->mutex);
    return PJ_IOQUEUE_PENDING;
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
    int status;
    
    /* check parameters. All must be specified! */
    pj_assert(ioqueue && key && addr && addrlen);

    /* Connecting socket must have no other operation! */
    pj_assert(key->op == 0);
    
    status = pj_sock_connect(key->fd, addr, addrlen);
    if (status == 0) {
	/* Connected! */
	return 0;
    } else {
	pj_status_t oserr = pj_getlasterror();
	if (oserr == PJ_EINPROGRESS || oserr == PJ_EWOULDBLOCK) {
	    /* Pending! */
	    pj_mutex_lock(ioqueue->mutex);
	    key->op = PJ_IOQUEUE_OP_CONNECT;
	    PJ_FD_SET(key->fd, &ioqueue->wfdset);
	    PJ_FD_SET(key->fd, &ioqueue->xfdset);
	    pj_mutex_unlock(ioqueue->mutex);
	    return PJ_IOQUEUE_PENDING;
	} else {
	    /* Error! */
	    return oserr;
	}
    }
}
#endif	/* PJ_HAS_TCP */

#endif	/* #if PJ_IOQUEUE_USE_SELECT */


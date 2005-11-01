/* $Header: /pjproject-0.3/pjlib/src/pj/ioqueue_select.c 15    10/29/05 10:27p Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/ioqueue_select.c $
 * 
 * 15    10/29/05 10:27p Bennylp
 * Fixed misc warnings.
 * 
 * 14    10/29/05 11:31a Bennylp
 * Changed accept and lock.
 * 
 * 13    10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 12    9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 11    9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */

/*
 * sock_select.c
 *
 * This is the implementation of IOQueue using pj_sock_select().
 * It runs anywhere where pj_sock_select() is available (currently
 * Win32, Linux, Linux kernel, etc.).
 */

#include <pj/ioqueue.h>
#include <pj/os.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/list.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/sock.h>
#include <pj/compat/socket.h>
#include <pj/sock_select.h>
#include <pj/errno.h>

/*
 * ISSUES with ioqueue_select()
 *
 * EAGAIN/EWOULDBLOCK error in recv():
 *  - when multiple threads are working with the ioqueue, application
 *    may receive EAGAIN or EWOULDBLOCK in the receive callback.
 *    This error happens because more than one thread is watching for
 *    the same descriptor set, so when all of them call recv() or recvfrom()
 *    simultaneously, only one will succeed and the rest will get the error.
 *
 */
#define THIS_FILE   "ioq_select"

#define PJ_IOQUEUE_IS_READ_OP(op)   ((op & PJ_IOQUEUE_OP_READ) || \
                                     (op & PJ_IOQUEUE_OP_RECV) || \
                                     (op & PJ_IOQUEUE_OP_RECV_FROM))
#define PJ_IOQUEUE_IS_WRITE_OP(op)  ((op & PJ_IOQUEUE_OP_WRITE) || \
                                     (op & PJ_IOQUEUE_OP_SEND) || \
                                     (op & PJ_IOQUEUE_OP_SEND_TO))


#if PJ_HAS_TCP
#  define PJ_IOQUEUE_IS_ACCEPT_OP(op)	(op & PJ_IOQUEUE_OP_ACCEPT)
#  define PJ_IOQUEUE_IS_CONNECT_OP(op)	(op & PJ_IOQUEUE_OP_CONNECT)
#else
#  define PJ_IOQUEUE_IS_ACCEPT_OP(op)	0
#  define PJ_IOQUEUE_IS_CONNECT_OP(op)	0
#endif

/*
 * During debugging build, VALIDATE_FD_SET is set.
 * This will check the validity of the fd_sets.
 */
#if defined(PJ_DEBUG) && PJ_DEBUG != 0
#  define VALIDATE_FD_SET		1
#else
#  define VALIDATE_FD_SET		0
#endif

/*
 * This describes each key.
 */
struct pj_ioqueue_key_t
{
    PJ_DECL_LIST_MEMBER(struct pj_ioqueue_key_t)
    pj_sock_t		    fd;
    pj_ioqueue_operation_e  op;
    void		   *user_data;
    pj_ioqueue_callback	    cb;

    void		   *rd_buf;
    unsigned                rd_flags;
    pj_size_t		    rd_buflen;
    void		   *wr_buf;
    pj_size_t		    wr_buflen;

    pj_sockaddr_t	   *rmt_addr;
    int			   *rmt_addrlen;

    pj_sockaddr_t	   *local_addr;
    int			   *local_addrlen;

    pj_sock_t		   *accept_fd;
};

/*
 * This describes the I/O queue itself.
 */
struct pj_ioqueue_t
{
    pj_lock_t          *lock;
    pj_bool_t           auto_delete_lock;
    unsigned		max, count;
    pj_ioqueue_key_t	hlist;
    pj_fd_set_t		rfdset;
    pj_fd_set_t		wfdset;
#if PJ_HAS_TCP
    pj_fd_set_t		xfdset;
#endif
};

/*
 * pj_ioqueue_create()
 *
 * Create select ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_create( pj_pool_t *pool, 
                                       pj_size_t max_fd,
                                       int max_threads,
                                       pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioque;
    pj_status_t rc;

    PJ_UNUSED_ARG(max_threads);

    if (max_fd > PJ_IOQUEUE_MAX_HANDLES) {
        pj_assert(!"max_fd too large");
	return PJ_EINVAL;
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

    rc = pj_lock_create_recursive_mutex(pool, "ioq%p", &ioque->lock);
    if (rc != PJ_SUCCESS)
	return rc;

    ioque->auto_delete_lock = PJ_TRUE;

    PJ_LOG(4, ("pjlib", "select() I/O Queue created (%p)", ioque));

    *p_ioqueue = ioque;
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_destroy()
 *
 * Destroy ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_destroy(pj_ioqueue_t *ioque)
{
    pj_status_t rc = PJ_SUCCESS;

    PJ_ASSERT_RETURN(ioque, PJ_EINVAL);

    if (ioque->auto_delete_lock)
        rc = pj_lock_destroy(ioque->lock);

    return rc;
}


/*
 * pj_ioqueue_set_lock()
 */
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


/*
 * pj_ioqueue_register_sock()
 *
 * Register a handle to ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					      pj_ioqueue_t *ioque,
					      pj_sock_t sock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key)
{
    pj_ioqueue_key_t *key = NULL;
    pj_uint32_t value;
    pj_status_t rc = PJ_SUCCESS;
    
    PJ_ASSERT_RETURN(pool && ioque && sock != PJ_INVALID_SOCKET &&
                     cb && p_key, PJ_EINVAL);

    pj_lock_acquire(ioque->lock);

    if (ioque->count >= ioque->max) {
        rc = PJ_ETOOMANY;
	goto on_return;
    }

    /* Set socket to nonblocking. */
    value = 1;
#ifdef PJ_WIN32
    if (ioctlsocket(sock, FIONBIO, (unsigned long*)&value)) {
#else
    if (ioctl(sock, FIONBIO, &value)) {
#endif
        rc = pj_get_netos_error();
	goto on_return;
    }

    /* Create key. */
    key = (pj_ioqueue_key_t*)pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));
    key->fd = sock;
    key->user_data = user_data;

    /* Save callback. */
    pj_memcpy(&key->cb, cb, sizeof(pj_ioqueue_callback));

    /* Register */
    pj_list_insert_before(&ioque->hlist, key);
    ++ioque->count;

on_return:
    *p_key = key;
    pj_lock_release(ioque->lock);
    
    return rc;
}

/*
 * pj_ioqueue_unregister()
 *
 * Unregister handle from ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_t *ioque,
					   pj_ioqueue_key_t *key)
{
    PJ_ASSERT_RETURN(ioque && key, PJ_EINVAL);

    pj_lock_acquire(ioque->lock);

    pj_assert(ioque->count > 0);
    --ioque->count;
    pj_list_erase(key);
    PJ_FD_CLR(key->fd, &ioque->rfdset);
    PJ_FD_CLR(key->fd, &ioque->wfdset);
#if PJ_HAS_TCP
    PJ_FD_CLR(key->fd, &ioque->xfdset);
#endif

    pj_lock_release(ioque->lock);
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_get_user_data()
 *
 * Obtain value associated with a key.
 */
PJ_DEF(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key )
{
    PJ_ASSERT_RETURN(key != NULL, NULL);
    return key->user_data;
}


/* This supposed to check whether the fd_set values are consistent
 * with the operation currently set in each key.
 */
#if VALIDATE_FD_SET
static void validate_sets(const pj_ioqueue_t *ioque,
			  const pj_fd_set_t *rfdset,
			  const pj_fd_set_t *wfdset,
			  const pj_fd_set_t *xfdset)
{
    pj_ioqueue_key_t *key;

    key = ioque->hlist.next;
    while (key != &ioque->hlist) {
	if ((key->op & PJ_IOQUEUE_OP_READ) 
	    || (key->op & PJ_IOQUEUE_OP_RECV)
	    || (key->op & PJ_IOQUEUE_OP_RECV_FROM)
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	    || (key->op & PJ_IOQUEUE_OP_ACCEPT)
#endif
	    ) 
	{
	    pj_assert(PJ_FD_ISSET(key->fd, rfdset));
	} 
	else {
	    pj_assert(PJ_FD_ISSET(key->fd, rfdset) == 0);
	}
	if ((key->op & PJ_IOQUEUE_OP_WRITE)
	    || (key->op & PJ_IOQUEUE_OP_SEND)
	    || (key->op & PJ_IOQUEUE_OP_SEND_TO)
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	    || (key->op & PJ_IOQUEUE_OP_CONNECT)
#endif
	   )
	{
	    pj_assert(PJ_FD_ISSET(key->fd, wfdset));
	}
	else {
	    pj_assert(PJ_FD_ISSET(key->fd, wfdset) == 0);
	}
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	if (key->op & PJ_IOQUEUE_OP_CONNECT)
	{
	    pj_assert(PJ_FD_ISSET(key->fd, xfdset));
	}
	else {
	    pj_assert(PJ_FD_ISSET(key->fd, xfdset) == 0);
	}
#endif /* PJ_HAS_TCP */

	key = key->next;
    }
}
#endif	/* VALIDATE_FD_SET */


/*
 * pj_ioqueue_poll()
 *
 * Few things worth written:
 *
 *  - we used to do only one callback called per poll, but it didn't go
 *    very well. The reason is because on some situation, the write 
 *    callback gets called all the time, thus doesn't give the read
 *    callback to get called. This happens, for example, when user
 *    submit write operation inside the write callback.
 *    As the result, we changed the behaviour so that now multiple
 *    callbacks are called in a single poll. It should be fast too,
 *    just that we need to be carefull with the ioqueue data structs.
 *
 *  - to guarantee preemptiveness etc, the poll function must strictly
 *    work on fd_set copy of the ioqueue (not the original one).
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioque, const pj_time_val *timeout)
{
    pj_fd_set_t rfdset, wfdset, xfdset;
    int count;
    pj_ioqueue_key_t *h;

    /* Lock ioqueue before making fd_set copies */
    pj_lock_acquire(ioque->lock);

    if (PJ_FD_COUNT(&ioque->rfdset)==0 &&
        PJ_FD_COUNT(&ioque->wfdset)==0 &&
        PJ_FD_COUNT(&ioque->xfdset)==0)
    {
        pj_lock_release(ioque->lock);
        if (timeout)
            pj_thread_sleep(PJ_TIME_VAL_MSEC(*timeout));
        return 0;
    }

    /* Copy ioqueue's pj_fd_set_t to local variables. */
    pj_memcpy(&rfdset, &ioque->rfdset, sizeof(pj_fd_set_t));
    pj_memcpy(&wfdset, &ioque->wfdset, sizeof(pj_fd_set_t));
#if PJ_HAS_TCP
    pj_memcpy(&xfdset, &ioque->xfdset, sizeof(pj_fd_set_t));
#else
    PJ_FD_ZERO(&xfdset);
#endif

#if VALIDATE_FD_SET
    validate_sets(ioque, &rfdset, &wfdset, &xfdset);
#endif

    /* Unlock ioqueue before select(). */
    pj_lock_release(ioque->lock);

    count = pj_sock_select(FD_SETSIZE, &rfdset, &wfdset, &xfdset, timeout);
    
    if (count <= 0)
	return count;

    /* Lock ioqueue again before scanning for signalled sockets. */
    pj_lock_acquire(ioque->lock);

#if PJ_HAS_TCP
    /* Scan for exception socket */
    h = ioque->hlist.next;
do_except_scan:
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
        PJ_FD_CLR(h->fd, &wfdset);
        PJ_FD_CLR(h->fd, &xfdset);

	/* Call callback. */
        if (h->cb.on_connect_complete)
	    (*h->cb.on_connect_complete)(h, -1);

        /* Re-scan exception list. */
        goto do_except_scan;
    }
#endif	/* PJ_HAS_TCP */

    /* Scan for readable socket. */
    h = ioque->hlist.next;
do_readable_scan:
    for ( ; h!=&ioque->hlist; h = h->next) {
	if ((PJ_IOQUEUE_IS_READ_OP(h->op) || PJ_IOQUEUE_IS_ACCEPT_OP(h->op)) && 
	    PJ_FD_ISSET(h->fd, &rfdset))
        {
	    break;
        }
    }
    if (h != &ioque->hlist) {
        pj_status_t rc;

	pj_assert(PJ_IOQUEUE_IS_READ_OP(h->op) ||
		  PJ_IOQUEUE_IS_ACCEPT_OP(h->op));
	
#	if PJ_HAS_TCP
	if ((h->op & PJ_IOQUEUE_OP_ACCEPT)) {
	    /* accept() must be the only operation specified on server socket */
	    pj_assert(h->op == PJ_IOQUEUE_OP_ACCEPT);

	    rc=pj_sock_accept(h->fd, h->accept_fd, h->rmt_addr, h->rmt_addrlen);
	    if (rc==0 && h->local_addr) {
		rc = pj_sock_getsockname(*h->accept_fd, h->local_addr, 
					 h->local_addrlen);
	    }

	    h->op &= ~(PJ_IOQUEUE_OP_ACCEPT);
	    PJ_FD_CLR(h->fd, &ioque->rfdset);

	    /* Call callback. */
            if (h->cb.on_accept_complete)
	        (*h->cb.on_accept_complete)(h, *h->accept_fd, rc);

            /* Re-scan readable sockets. */
            goto do_readable_scan;
        } 
        else {
#	endif
            pj_ssize_t bytes_read = h->rd_buflen;

	    if ((h->op & PJ_IOQUEUE_OP_RECV_FROM)) {
	        rc = pj_sock_recvfrom(h->fd, h->rd_buf, &bytes_read, 0,
				      h->rmt_addr, h->rmt_addrlen);
	    } else if ((h->op & PJ_IOQUEUE_OP_RECV)) {
	        rc = pj_sock_recv(h->fd, h->rd_buf, &bytes_read, 0);
            } else {
                /*
                 * User has specified pj_ioqueue_read().
                 * On Win32, we should do ReadFile(). But because we got
                 * here because of select() anyway, user must have put a
                 * socket descriptor on h->fd, which in this case we can
                 * just call pj_sock_recv() instead of ReadFile().
                 * On Unix, user may put a file in h->fd, so we'll have
                 * to call read() here.
                 * This may not compile on systems which doesn't have 
                 * read(). That's why we only specify PJ_LINUX here so
                 * that error is easier to catch.
                 */
#	        if defined(PJ_WIN32) && PJ_WIN32 != 0
                rc = pj_sock_recv(h->fd, h->rd_buf, &bytes_read, 0);
#               elif (defined(PJ_LINUX) && PJ_LINUX != 0) || \
		     (defined(PJ_SUNOS) && PJ_SUNOS != 0)
                bytes_read = read(h->fd, h->rd_buf, bytes_read);
                rc = (bytes_read >= 0) ? PJ_SUCCESS : pj_get_os_error();
#		elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL != 0
                bytes_read = sys_read(h->fd, h->rd_buf, bytes_read);
                rc = (bytes_read >= 0) ? PJ_SUCCESS : -bytes_read;
#               else
#               error "Implement read() for this platform!"
#               endif
            }
	    
	    if (rc != PJ_SUCCESS) {
#	        if defined(PJ_WIN32) && PJ_WIN32 != 0
	        /* On Win32, for UDP, WSAECONNRESET on the receive side 
	         * indicates that previous sending has triggered ICMP Port 
	         * Unreachable message.
	         * But we wouldn't know at this point which one of previous 
	         * key that has triggered the error, since UDP socket can
	         * be shared!
	         * So we'll just ignore it!
	         */

	        if (rc == PJ_STATUS_FROM_OS(WSAECONNRESET)) {
		    PJ_LOG(4,(THIS_FILE, 
                              "Ignored ICMP port unreach. on key=%p", h));
	        }
#	        endif

                /* In any case we would report this to caller. */
                bytes_read = -rc;
	    }

	    h->op &= ~(PJ_IOQUEUE_OP_READ | PJ_IOQUEUE_OP_RECV | 
                       PJ_IOQUEUE_OP_RECV_FROM);
	    PJ_FD_CLR(h->fd, &ioque->rfdset);
            PJ_FD_CLR(h->fd, &rfdset);

	    /* Call callback. */
            if (h->cb.on_read_complete)
	        (*h->cb.on_read_complete)(h, bytes_read);

            /* Re-scan readable sockets. */
            goto do_readable_scan;

        }
    }

    /* Scan for writable socket  */
    h = ioque->hlist.next;
do_writable_scan:
    for ( ; h!=&ioque->hlist; h = h->next) {
	if ((PJ_IOQUEUE_IS_WRITE_OP(h->op) || PJ_IOQUEUE_IS_CONNECT_OP(h->op)) 
	    && PJ_FD_ISSET(h->fd, &wfdset))
        {
	    break;
        }
    }
    if (h != &ioque->hlist) {
	pj_assert(PJ_IOQUEUE_IS_WRITE_OP(h->op) || 
		  PJ_IOQUEUE_IS_CONNECT_OP(h->op));

#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0
	if ((h->op & PJ_IOQUEUE_OP_CONNECT)) {
	    /* Completion of connect() operation */
	    pj_ssize_t bytes_transfered;

#if (defined(PJ_LINUX) && PJ_LINUX!=0) || \
    (defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL!=0)
	    /* from connect(2): 
		* On Linux, use getsockopt to read the SO_ERROR option at
		* level SOL_SOCKET to determine whether connect() completed
		* successfully (if SO_ERROR is zero).
		*/
	    int value;
	    socklen_t vallen = sizeof(value);
	    int gs_rc = getsockopt(h->fd, SOL_SOCKET, SO_ERROR, 
                                   &value, &vallen);
	    if (gs_rc != 0) {
		/* Argh!! What to do now??? 
		 * Just indicate that the socket is connected. The
		 * application will get error as soon as it tries to use
		 * the socket to send/receive.
		 */
		bytes_transfered = 0;
	    } else {
                bytes_transfered = value;
	    }
#elif defined(PJ_WIN32) && PJ_WIN32!=0
	    bytes_transfered = 0; /* success */
#else
	    /* Excellent information in D.J. Bernstein page:
	     * http://cr.yp.to/docs/connect.html
	     *
	     * Seems like the most portable way of detecting connect()
	     * failure is to call getpeername(). If socket is connected,
	     * getpeername() will return 0. If the socket is not connected,
	     * it will return ENOTCONN, and read(fd, &ch, 1) will produce
	     * the right errno through error slippage. This is a combination
	     * of suggestions from Douglas C. Schmidt and Ken Keys.
	     */
	    int gp_rc;
	    struct sockaddr_in addr;
	    socklen_t addrlen = sizeof(addr);

	    gp_rc = getpeername(h->fd, (struct sockaddr*)&addr, &addrlen);
	    bytes_transfered = gp_rc;
#endif

	    /* Clear operation. */
	    h->op &= (~PJ_IOQUEUE_OP_CONNECT);
	    PJ_FD_CLR(h->fd, &ioque->wfdset);
	    PJ_FD_CLR(h->fd, &ioque->xfdset);

	    /* Call callback. */
            if (h->cb.on_connect_complete)
	        (*h->cb.on_connect_complete)(h, bytes_transfered);

            /* Re-scan writable sockets. */
            goto do_writable_scan;

	} else 
#endif /* PJ_HAS_TCP */
	{
	    /* Completion of write(), send(), or sendto() operation. */

	    /* Clear operation. */
	    h->op &= ~(PJ_IOQUEUE_OP_WRITE | PJ_IOQUEUE_OP_SEND | 
                       PJ_IOQUEUE_OP_SEND_TO);
	    PJ_FD_CLR(h->fd, &ioque->wfdset);
            PJ_FD_CLR(h->fd, &wfdset);

	    /* Call callback. */
	    /* All data must have been sent? */
            if (h->cb.on_write_complete)
	        (*h->cb.on_write_complete)(h, h->wr_buflen);

            /* Re-scan writable sockets. */
            goto do_writable_scan;
	}
    }

    /* Shouldn't happen. */
    /* For strange reason on WinXP select() can return 1 while there is no
     * pj_fd_set_t signaled. */
    /* pj_assert(0); */

    //count = 0;

    pj_lock_release(ioque->lock);
    return count;
}

/*
 * pj_ioqueue_read()
 *
 * Start asynchronous read from the descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_read( pj_ioqueue_t *ioque,
			             pj_ioqueue_key_t *key,
			             void *buffer,
			             pj_size_t buflen)
{
    PJ_ASSERT_RETURN(ioque && key && buffer, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* For consistency with other ioqueue implementation, we would reject 
     * if descriptor has already been submitted for reading before.
     */
    PJ_ASSERT_RETURN(((key->op & PJ_IOQUEUE_OP_READ) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_RECV) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_RECV_FROM) == 0),
                     PJ_EBUSY);

    pj_lock_acquire(ioque->lock);

    key->op |= PJ_IOQUEUE_OP_READ;
    key->rd_flags = 0;
    key->rd_buf = buffer;
    key->rd_buflen = buflen;
    PJ_FD_SET(key->fd, &ioque->rfdset);

    pj_lock_release(ioque->lock);
    return PJ_EPENDING;
}


/*
 * pj_ioqueue_recv()
 *
 * Start asynchronous recv() from the socket.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recv(  pj_ioqueue_t *ioque,
				      pj_ioqueue_key_t *key,
				      void *buffer,
				      pj_size_t buflen,
				      unsigned flags )
{
    PJ_ASSERT_RETURN(ioque && key && buffer, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* For consistency with other ioqueue implementation, we would reject 
     * if descriptor has already been submitted for reading before.
     */
    PJ_ASSERT_RETURN(((key->op & PJ_IOQUEUE_OP_READ) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_RECV) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_RECV_FROM) == 0),
                     PJ_EBUSY);

    pj_lock_acquire(ioque->lock);

    key->op |= PJ_IOQUEUE_OP_RECV;
    key->rd_buf = buffer;
    key->rd_buflen = buflen;
    key->rd_flags = flags;
    PJ_FD_SET(key->fd, &ioque->rfdset);

    pj_lock_release(ioque->lock);
    return PJ_EPENDING;
}

/*
 * pj_ioqueue_recvfrom()
 *
 * Start asynchronous recvfrom() from the socket.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_t *ioque,
				         pj_ioqueue_key_t *key,
				         void *buffer,
				         pj_size_t buflen,
                                         unsigned flags,
				         pj_sockaddr_t *addr,
				         int *addrlen)
{
    PJ_ASSERT_RETURN(ioque && key && buffer, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* For consistency with other ioqueue implementation, we would reject 
     * if descriptor has already been submitted for reading before.
     */
    PJ_ASSERT_RETURN(((key->op & PJ_IOQUEUE_OP_READ) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_RECV) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_RECV_FROM) == 0),
                     PJ_EBUSY);

    pj_lock_acquire(ioque->lock);

    key->op |= PJ_IOQUEUE_OP_RECV_FROM;
    key->rd_buf = buffer;
    key->rd_buflen = buflen;
    key->rd_flags = flags;
    key->rmt_addr = addr;
    key->rmt_addrlen = addrlen;
    PJ_FD_SET(key->fd, &ioque->rfdset);

    pj_lock_release(ioque->lock);
    return PJ_EPENDING;
}

/*
 * pj_ioqueue_write()
 *
 * Start asynchronous write() to the descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_write( pj_ioqueue_t *ioque,
			              pj_ioqueue_key_t *key,
			              const void *data,
			              pj_size_t datalen)
{
    pj_status_t rc;
    pj_ssize_t sent;

    PJ_ASSERT_RETURN(ioque && key && data, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* For consistency with other ioqueue implementation, we would reject 
     * if descriptor has already been submitted for writing before.
     */
    PJ_ASSERT_RETURN(((key->op & PJ_IOQUEUE_OP_WRITE) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_SEND) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_SEND_TO) == 0),
                     PJ_EBUSY);

    sent = datalen;
    /* sent would be -1 after pj_sock_send() if it returns error. */
    rc = pj_sock_send(key->fd, data, &sent, 0);
    if (rc != PJ_SUCCESS && rc != PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {
        return rc;
    }

    pj_lock_acquire(ioque->lock);

    key->op |= PJ_IOQUEUE_OP_WRITE;
    key->wr_buf = NULL;
    key->wr_buflen = datalen;
    PJ_FD_SET(key->fd, &ioque->wfdset);

    pj_lock_release(ioque->lock);

    return PJ_EPENDING;
}

/*
 * pj_ioqueue_send()
 *
 * Start asynchronous send() to the descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_send( pj_ioqueue_t *ioque,
			             pj_ioqueue_key_t *key,
			             const void *data,
			             pj_size_t datalen,
                                     unsigned flags)
{
    pj_status_t rc;
    pj_ssize_t sent;

    PJ_ASSERT_RETURN(ioque && key && data, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* For consistency with other ioqueue implementation, we would reject 
     * if descriptor has already been submitted for writing before.
     */
    PJ_ASSERT_RETURN(((key->op & PJ_IOQUEUE_OP_WRITE) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_SEND) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_SEND_TO) == 0),
                     PJ_EBUSY);

    sent = datalen;
    /* sent would be -1 after pj_sock_send() if it returns error. */
    rc = pj_sock_send(key->fd, data, &sent, flags);
    if (rc != PJ_SUCCESS && rc != PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) {
        return rc;
    }

    pj_lock_acquire(ioque->lock);

    key->op |= PJ_IOQUEUE_OP_SEND;
    key->wr_buf = NULL;
    key->wr_buflen = datalen;
    PJ_FD_SET(key->fd, &ioque->wfdset);

    pj_lock_release(ioque->lock);

    return PJ_EPENDING;
}


/*
 * pj_ioqueue_sendto()
 *
 * Start asynchronous write() to the descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_t *ioque,
			               pj_ioqueue_key_t *key,
			               const void *data,
			               pj_size_t datalen,
                                       unsigned flags,
			               const pj_sockaddr_t *addr,
			               int addrlen)
{
    pj_status_t rc;
    pj_ssize_t sent;

    PJ_ASSERT_RETURN(ioque && key && data, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* For consistency with other ioqueue implementation, we would reject 
     * if descriptor has already been submitted for writing before.
     */
    PJ_ASSERT_RETURN(((key->op & PJ_IOQUEUE_OP_WRITE) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_SEND) == 0 &&
                      (key->op & PJ_IOQUEUE_OP_SEND_TO) == 0),
                     PJ_EBUSY);

    sent = datalen;
    /* sent would be -1 after pj_sock_sendto() if it returns error. */
    rc = pj_sock_sendto(key->fd, data, &sent, flags, addr, addrlen);
    if (rc != PJ_SUCCESS && rc != PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK))  {
        return rc;
    }

    pj_lock_acquire(ioque->lock);

    key->op |= PJ_IOQUEUE_OP_SEND_TO;
    key->wr_buf = NULL;
    key->wr_buflen = datalen;
    PJ_FD_SET(key->fd, &ioque->wfdset);

    pj_lock_release(ioque->lock);
    return PJ_EPENDING;
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
    pj_assert(ioqueue && key && new_sock);

    /* Server socket must have no other operation! */
    pj_assert(key->op == 0);
    
    pj_lock_acquire(ioqueue->lock);

    key->op = PJ_IOQUEUE_OP_ACCEPT;
    key->accept_fd = new_sock;
    key->rmt_addr = remote;
    key->rmt_addrlen = addrlen;
    key->local_addr = local;
    key->local_addrlen = addrlen;   /* use same addr. as rmt_addrlen */

    PJ_FD_SET(key->fd, &ioqueue->rfdset);

    pj_lock_release(ioqueue->lock);
    return PJ_EPENDING;
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
    pj_status_t rc;
    
    /* check parameters. All must be specified! */
    PJ_ASSERT_RETURN(ioqueue && key && addr && addrlen, PJ_EINVAL);

    /* Connecting socket must have no other operation! */
    PJ_ASSERT_RETURN(key->op == 0, PJ_EBUSY);
    
    rc = pj_sock_connect(key->fd, addr, addrlen);
    if (rc == PJ_SUCCESS) {
	/* Connected! */
	return PJ_SUCCESS;
    } else {
	if (rc == PJ_STATUS_FROM_OS(OSERR_EINPROGRESS) || 
            rc == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) 
        {
	    /* Pending! */
	    pj_lock_acquire(ioqueue->lock);
	    key->op = PJ_IOQUEUE_OP_CONNECT;
	    PJ_FD_SET(key->fd, &ioqueue->wfdset);
	    PJ_FD_SET(key->fd, &ioqueue->xfdset);
	    pj_lock_release(ioqueue->lock);
	    return PJ_EPENDING;
	} else {
	    /* Error! */
	    return rc;
	}
    }
}
#endif	/* PJ_HAS_TCP */


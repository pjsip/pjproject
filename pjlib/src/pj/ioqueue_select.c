/* $Id$
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

/*
 * The select ioqueue relies on socket functions (pj_sock_xxx()) to return
 * the correct error code.
 */
#if PJ_RETURN_OS_ERROR(100) != PJ_STATUS_FROM_OS(100)
#   error "Error reporting must be enabled for this function to work!"
#endif

/**
 * Get the number of descriptors in the set. This is defined in sock_select.c
 * This function will only return the number of sockets set from PJ_FD_SET
 * operation. When the set is modified by other means (such as by select()),
 * the count will not be reflected here.
 *
 * That's why don't export this function in the header file, to avoid
 * misunderstanding.
 *
 * @param fdsetp    The descriptor set.
 *
 * @return          Number of descriptors in the set.
 */
PJ_DECL(pj_size_t) PJ_FD_COUNT(const pj_fd_set_t *fdsetp);




/*
 * During debugging build, VALIDATE_FD_SET is set.
 * This will check the validity of the fd_sets.
 */
#if defined(PJ_DEBUG) && PJ_DEBUG != 0
#  define VALIDATE_FD_SET		1
#else
#  define VALIDATE_FD_SET		0
#endif

struct generic_operation
{
    PJ_DECL_LIST_MEMBER(struct generic_operation);
    pj_ioqueue_operation_e  op;
};

struct read_operation
{
    PJ_DECL_LIST_MEMBER(struct read_operation);
    pj_ioqueue_operation_e  op;

    void		   *buf;
    pj_size_t		    size;
    unsigned                flags;
    pj_sockaddr_t	   *rmt_addr;
    int			   *rmt_addrlen;
};

struct write_operation
{
    PJ_DECL_LIST_MEMBER(struct write_operation);
    pj_ioqueue_operation_e  op;

    char		   *buf;
    pj_size_t		    size;
    pj_ssize_t              written;
    unsigned                flags;
    pj_sockaddr_in	    rmt_addr;
    int			    rmt_addrlen;
};

#if PJ_HAS_TCP
struct accept_operation
{
    PJ_DECL_LIST_MEMBER(struct accept_operation);
    pj_ioqueue_operation_e  op;

    pj_sock_t              *accept_fd;
    pj_sockaddr_t	   *local_addr;
    pj_sockaddr_t	   *rmt_addr;
    int			   *addrlen;
};
#endif

union operation_key
{
    struct generic_operation generic;
    struct read_operation    read;
    struct write_operation   write;
#if PJ_HAS_TCP
    struct accept_operation  accept;
#endif
};

/*
 * This describes each key.
 */
struct pj_ioqueue_key_t
{
    PJ_DECL_LIST_MEMBER(struct pj_ioqueue_key_t);
    pj_ioqueue_t           *ioqueue;
    pj_sock_t		    fd;
    void		   *user_data;
    pj_ioqueue_callback	    cb;
    int                     connecting;
    struct read_operation   read_list;
    struct write_operation  write_list;
#if PJ_HAS_TCP
    struct accept_operation accept_list;
#endif
};

/*
 * This describes the I/O queue itself.
 */
struct pj_ioqueue_t
{
    pj_lock_t          *lock;
    pj_bool_t           auto_delete_lock;
    unsigned		max, count;
    pj_ioqueue_key_t	key_list;
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
                                       pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioqueue;
    pj_status_t rc;

    /* Check that arguments are valid. */
    PJ_ASSERT_RETURN(pool != NULL && p_ioqueue != NULL && 
                     max_fd > 0 && max_fd <= PJ_IOQUEUE_MAX_HANDLES, 
                     PJ_EINVAL);

    /* Check that size of pj_ioqueue_op_key_t is sufficient */
    PJ_ASSERT_RETURN(sizeof(pj_ioqueue_op_key_t)-sizeof(void*) >=
                     sizeof(union operation_key), PJ_EBUG);

    ioqueue = pj_pool_alloc(pool, sizeof(pj_ioqueue_t));
    ioqueue->max = max_fd;
    ioqueue->count = 0;
    PJ_FD_ZERO(&ioqueue->rfdset);
    PJ_FD_ZERO(&ioqueue->wfdset);
#if PJ_HAS_TCP
    PJ_FD_ZERO(&ioqueue->xfdset);
#endif
    pj_list_init(&ioqueue->key_list);

    rc = pj_lock_create_recursive_mutex(pool, "ioq%p", &ioqueue->lock);
    if (rc != PJ_SUCCESS)
	return rc;

    ioqueue->auto_delete_lock = PJ_TRUE;

    PJ_LOG(4, ("pjlib", "select() I/O Queue created (%p)", ioqueue));

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
    pj_status_t rc = PJ_SUCCESS;

    PJ_ASSERT_RETURN(ioqueue, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

    if (ioqueue->auto_delete_lock)
        rc = pj_lock_destroy(ioqueue->lock);

    return rc;
}


/*
 * pj_ioqueue_register_sock()
 *
 * Register a handle to ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					      pj_ioqueue_t *ioqueue,
					      pj_sock_t sock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key)
{
    pj_ioqueue_key_t *key = NULL;
    pj_uint32_t value;
    pj_status_t rc = PJ_SUCCESS;
    
    PJ_ASSERT_RETURN(pool && ioqueue && sock != PJ_INVALID_SOCKET &&
                     cb && p_key, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

    if (ioqueue->count >= ioqueue->max) {
        rc = PJ_ETOOMANY;
	goto on_return;
    }

    /* Set socket to nonblocking. */
    value = 1;
#ifdef PJ_WIN32
    if (ioctlsocket(sock, FIONBIO, (u_long*)&value)) {
#else
    if (ioctl(sock, FIONBIO, &value)) {
#endif
        rc = pj_get_netos_error();
	goto on_return;
    }

    /* Create key. */
    key = (pj_ioqueue_key_t*)pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));
    key->ioqueue = ioqueue;
    key->fd = sock;
    key->user_data = user_data;
    pj_list_init(&key->read_list);
    pj_list_init(&key->write_list);
#if PJ_HAS_TCP
    pj_list_init(&key->accept_list);
#endif

    /* Save callback. */
    pj_memcpy(&key->cb, cb, sizeof(pj_ioqueue_callback));

    /* Register */
    pj_list_insert_before(&ioqueue->key_list, key);
    ++ioqueue->count;

on_return:
    /* On error, socket may be left in non-blocking mode. */
    *p_key = key;
    pj_lock_release(ioqueue->lock);
    
    return rc;
}

/*
 * pj_ioqueue_unregister()
 *
 * Unregister handle from ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key)
{
    pj_ioqueue_t *ioqueue;

    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    ioqueue = key->ioqueue;

    pj_lock_acquire(ioqueue->lock);

    pj_assert(ioqueue->count > 0);
    --ioqueue->count;
    pj_list_erase(key);
    PJ_FD_CLR(key->fd, &ioqueue->rfdset);
    PJ_FD_CLR(key->fd, &ioqueue->wfdset);
#if PJ_HAS_TCP
    PJ_FD_CLR(key->fd, &ioqueue->xfdset);
#endif

    pj_lock_release(ioqueue->lock);
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


/*
 * pj_ioqueue_set_user_data()
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_user_data( pj_ioqueue_key_t *key,
                                              void *user_data,
                                              void **old_data)
{
    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    if (old_data)
        *old_data = key->user_data;
    key->user_data = user_data;

    return PJ_SUCCESS;
}


/* This supposed to check whether the fd_set values are consistent
 * with the operation currently set in each key.
 */
#if VALIDATE_FD_SET
static void validate_sets(const pj_ioqueue_t *ioqueue,
			  const pj_fd_set_t *rfdset,
			  const pj_fd_set_t *wfdset,
			  const pj_fd_set_t *xfdset)
{
    pj_ioqueue_key_t *key;

    key = ioqueue->key_list.next;
    while (key != &ioqueue->key_list) {
	if (!pj_list_empty(&key->read_list)
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	    || !pj_list_empty(&key->accept_list)
#endif
	    ) 
	{
	    pj_assert(PJ_FD_ISSET(key->fd, rfdset));
	} 
	else {
	    pj_assert(PJ_FD_ISSET(key->fd, rfdset) == 0);
	}
	if (!pj_list_empty(&key->write_list)
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	    || key->connecting
#endif
	   )
	{
	    pj_assert(PJ_FD_ISSET(key->fd, wfdset));
	}
	else {
	    pj_assert(PJ_FD_ISSET(key->fd, wfdset) == 0);
	}
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	if (key->connecting)
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
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioqueue, const pj_time_val *timeout)
{
    pj_fd_set_t rfdset, wfdset, xfdset;
    int count;
    pj_ioqueue_key_t *h;

    PJ_ASSERT_RETURN(ioqueue, PJ_EINVAL);

    /* Lock ioqueue before making fd_set copies */
    pj_lock_acquire(ioqueue->lock);

    /* We will only do select() when there are sockets to be polled.
     * Otherwise select() will return error.
     */
    if (PJ_FD_COUNT(&ioqueue->rfdset)==0 &&
        PJ_FD_COUNT(&ioqueue->wfdset)==0 &&
        PJ_FD_COUNT(&ioqueue->xfdset)==0)
    {
        pj_lock_release(ioqueue->lock);
        if (timeout)
            pj_thread_sleep(PJ_TIME_VAL_MSEC(*timeout));
        return 0;
    }

    /* Copy ioqueue's pj_fd_set_t to local variables. */
    pj_memcpy(&rfdset, &ioqueue->rfdset, sizeof(pj_fd_set_t));
    pj_memcpy(&wfdset, &ioqueue->wfdset, sizeof(pj_fd_set_t));
#if PJ_HAS_TCP
    pj_memcpy(&xfdset, &ioqueue->xfdset, sizeof(pj_fd_set_t));
#else
    PJ_FD_ZERO(&xfdset);
#endif

#if VALIDATE_FD_SET
    validate_sets(ioqueue, &rfdset, &wfdset, &xfdset);
#endif

    /* Unlock ioqueue before select(). */
    pj_lock_release(ioqueue->lock);

    count = pj_sock_select(FD_SETSIZE, &rfdset, &wfdset, &xfdset, timeout);
    
    if (count <= 0)
	return count;

    /* Lock ioqueue again before scanning for signalled sockets. 
     * We must strictly use recursive mutex since application may invoke
     * the ioqueue again inside the callback.
     */
    pj_lock_acquire(ioqueue->lock);

    /* Scan for writable sockets first to handle piggy-back data
     * coming with accept().
     */
    h = ioqueue->key_list.next;
do_writable_scan:
    for ( ; h!=&ioqueue->key_list; h = h->next) {
	if ( (!pj_list_empty(&h->write_list) || h->connecting)
	     && PJ_FD_ISSET(h->fd, &wfdset))
        {
	    break;
        }
    }
    if (h != &ioqueue->key_list) {
	pj_assert(!pj_list_empty(&h->write_list) || h->connecting);

#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0
	if (h->connecting) {
	    /* Completion of connect() operation */
	    pj_ssize_t bytes_transfered;

#if (defined(PJ_HAS_SO_ERROR) && PJ_HAS_SO_ERROR!=0)
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
	    h->connecting = 0;
	    PJ_FD_CLR(h->fd, &ioqueue->wfdset);
	    PJ_FD_CLR(h->fd, &ioqueue->xfdset);

	    /* Call callback. */
            if (h->cb.on_connect_complete)
	        (*h->cb.on_connect_complete)(h, bytes_transfered);

            /* Re-scan writable sockets. */
            goto do_writable_scan;

	} else 
#endif /* PJ_HAS_TCP */
	{
	    /* Socket is writable. */
            struct write_operation *write_op;
            pj_ssize_t sent;
            pj_status_t send_rc;

            /* Get the first in the queue. */
            write_op = h->write_list.next;

            /* Send the data. */
            sent = write_op->size - write_op->written;
            if (write_op->op == PJ_IOQUEUE_OP_SEND) {
                send_rc = pj_sock_send(h->fd, write_op->buf+write_op->written,
                                       &sent, write_op->flags);
            } else if (write_op->op == PJ_IOQUEUE_OP_SEND_TO) {
                send_rc = pj_sock_sendto(h->fd, 
                                         write_op->buf+write_op->written,
                                         &sent, write_op->flags,
                                         &write_op->rmt_addr, 
                                         write_op->rmt_addrlen);
            } else {
                pj_assert(!"Invalid operation type!");
                send_rc = PJ_EBUG;
            }

            if (send_rc == PJ_SUCCESS) {
                write_op->written += sent;
            } else {
                pj_assert(send_rc > 0);
                write_op->written = -send_rc;
            }

            /* In any case we don't need to process this descriptor again. */
            PJ_FD_CLR(h->fd, &wfdset);

            /* Are we finished with this buffer? */
            if (send_rc!=PJ_SUCCESS || 
                write_op->written == (pj_ssize_t)write_op->size) 
            {
                pj_list_erase(write_op);

                /* Clear operation if there's no more data to send. */
                if (pj_list_empty(&h->write_list))
                    PJ_FD_CLR(h->fd, &ioqueue->wfdset);

	        /* Call callback. */
                if (h->cb.on_write_complete) {
	            (*h->cb.on_write_complete)(h, 
                                               (pj_ioqueue_op_key_t*)write_op,
                                               write_op->written);
                }
            }
	    
            /* Re-scan writable sockets. */
            goto do_writable_scan;
	}
    }

    /* Scan for readable socket. */
    h = ioqueue->key_list.next;
do_readable_scan:
    for ( ; h!=&ioqueue->key_list; h = h->next) {
	if ((!pj_list_empty(&h->read_list) 
#if PJ_HAS_TCP
             || !pj_list_empty(&h->accept_list)
#endif
            ) && PJ_FD_ISSET(h->fd, &rfdset))
        {
	    break;
        }
    }
    if (h != &ioqueue->key_list) {
        pj_status_t rc;

#if PJ_HAS_TCP
	pj_assert(!pj_list_empty(&h->read_list) || 
                  !pj_list_empty(&h->accept_list));
#else
        pj_assert(!pj_list_empty(&h->read_list));
#endif
	
#	if PJ_HAS_TCP
	if (!pj_list_empty(&h->accept_list)) {

            struct accept_operation *accept_op;
	    
            /* Get one accept operation from the list. */
	    accept_op = h->accept_list.next;
            pj_list_erase(accept_op);

	    rc=pj_sock_accept(h->fd, accept_op->accept_fd, 
                              accept_op->rmt_addr, accept_op->addrlen);
	    if (rc==PJ_SUCCESS && accept_op->local_addr) {
		rc = pj_sock_getsockname(*accept_op->accept_fd, 
                                         accept_op->local_addr,
					 accept_op->addrlen);
	    }

	    /* Clear bit in fdset if there is no more pending accept */
            if (pj_list_empty(&h->accept_list))
	        PJ_FD_CLR(h->fd, &ioqueue->rfdset);

	    /* Call callback. */
            if (h->cb.on_accept_complete)
	        (*h->cb.on_accept_complete)(h, (pj_ioqueue_op_key_t*)accept_op,
                                            *accept_op->accept_fd, rc);

            /* Re-scan readable sockets. */
            goto do_readable_scan;
        }
        else {
#	endif
            struct read_operation *read_op;
            pj_ssize_t bytes_read;

            pj_assert(!pj_list_empty(&h->read_list));

            /* Get one pending read operation from the list. */
            read_op = h->read_list.next;
            pj_list_erase(read_op);

            bytes_read = read_op->size;

	    if ((read_op->op == PJ_IOQUEUE_OP_RECV_FROM)) {
	        rc = pj_sock_recvfrom(h->fd, read_op->buf, &bytes_read, 0,
				      read_op->rmt_addr, 
                                      read_op->rmt_addrlen);
	    } else if ((read_op->op == PJ_IOQUEUE_OP_RECV)) {
	        rc = pj_sock_recv(h->fd, read_op->buf, &bytes_read, 0);
            } else {
                pj_assert(read_op->op == PJ_IOQUEUE_OP_READ);
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
                rc = pj_sock_recv(h->fd, read_op->buf, &bytes_read, 0);
                //rc = ReadFile((HANDLE)h->fd, read_op->buf, read_op->size,
                //              &bytes_read, NULL);
#               elif (defined(PJ_HAS_UNISTD_H) && PJ_HAS_UNISTD_H != 0)
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
		    //PJ_LOG(4,(THIS_FILE, 
                    //          "Ignored ICMP port unreach. on key=%p", h));
	        }
#	        endif

                /* In any case we would report this to caller. */
                bytes_read = -rc;
	    }

            /* Clear fdset if there is no pending read. */
            if (pj_list_empty(&h->read_list))
	        PJ_FD_CLR(h->fd, &ioqueue->rfdset);

            /* In any case clear from temporary set. */
            PJ_FD_CLR(h->fd, &rfdset);

	    /* Call callback. */
            if (h->cb.on_read_complete)
	        (*h->cb.on_read_complete)(h, (pj_ioqueue_op_key_t*)read_op,
                                          bytes_read);

            /* Re-scan readable sockets. */
            goto do_readable_scan;

        }
    }

#if PJ_HAS_TCP
    /* Scan for exception socket for TCP connection error. */
    h = ioqueue->key_list.next;
do_except_scan:
    for ( ; h!=&ioqueue->key_list; h = h->next) {
	if (h->connecting && PJ_FD_ISSET(h->fd, &xfdset))
	    break;
    }
    if (h != &ioqueue->key_list) {

	pj_assert(h->connecting);

	/* Clear operation. */
	h->connecting = 0;
	PJ_FD_CLR(h->fd, &ioqueue->wfdset);
	PJ_FD_CLR(h->fd, &ioqueue->xfdset);
        PJ_FD_CLR(h->fd, &wfdset);
        PJ_FD_CLR(h->fd, &xfdset);

	/* Call callback. */
        if (h->cb.on_connect_complete)
	    (*h->cb.on_connect_complete)(h, -1);

        /* Re-scan exception list. */
        goto do_except_scan;
    }
#endif	/* PJ_HAS_TCP */

    /* Shouldn't happen. */
    /* For strange reason on WinXP select() can return 1 while there is no
     * pj_fd_set_t signaled. */
    /* pj_assert(0); */

    //count = 0;

    pj_lock_release(ioqueue->lock);
    return count;
}

/*
 * pj_ioqueue_recv()
 *
 * Start asynchronous recv() from the socket.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recv(  pj_ioqueue_key_t *key,
                                      pj_ioqueue_op_key_t *op_key,
				      void *buffer,
				      pj_ssize_t *length,
				      unsigned flags )
{
    pj_status_t status;
    pj_ssize_t size;
    struct read_operation *read_op;
    pj_ioqueue_t *ioqueue;

    PJ_ASSERT_RETURN(key && op_key && buffer && length, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* Try to see if there's data immediately available. 
     */
    size = *length;
    status = pj_sock_recv(key->fd, buffer, &size, flags);
    if (status == PJ_SUCCESS) {
        /* Yes! Data is available! */
        *length = size;
        return PJ_SUCCESS;
    } else {
        /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
         * the error to caller.
         */
        if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL))
            return status;
    }

    /*
     * No data is immediately available.
     * Must schedule asynchronous operation to the ioqueue.
     */
    ioqueue = key->ioqueue;
    pj_lock_acquire(ioqueue->lock);

    read_op = (struct read_operation*)op_key;

    read_op->op = PJ_IOQUEUE_OP_RECV;
    read_op->buf = buffer;
    read_op->size = *length;
    read_op->flags = flags;

    pj_list_insert_before(&key->read_list, read_op);
    PJ_FD_SET(key->fd, &ioqueue->rfdset);

    pj_lock_release(ioqueue->lock);
    return PJ_EPENDING;
}

/*
 * pj_ioqueue_recvfrom()
 *
 * Start asynchronous recvfrom() from the socket.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key,
				         void *buffer,
				         pj_ssize_t *length,
                                         unsigned flags,
				         pj_sockaddr_t *addr,
				         int *addrlen)
{
    pj_status_t status;
    pj_ssize_t size;
    struct read_operation *read_op;
    pj_ioqueue_t *ioqueue;

    PJ_ASSERT_RETURN(key && op_key && buffer && length, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* Try to see if there's data immediately available. 
     */
    size = *length;
    status = pj_sock_recvfrom(key->fd, buffer, &size, flags,
                              addr, addrlen);
    if (status == PJ_SUCCESS) {
        /* Yes! Data is available! */
        *length = size;
        return PJ_SUCCESS;
    } else {
        /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
         * the error to caller.
         */
        if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL))
            return status;
    }

    /*
     * No data is immediately available.
     * Must schedule asynchronous operation to the ioqueue.
     */
    ioqueue = key->ioqueue;
    pj_lock_acquire(ioqueue->lock);

    read_op = (struct read_operation*)op_key;

    read_op->op = PJ_IOQUEUE_OP_RECV_FROM;
    read_op->buf = buffer;
    read_op->size = *length;
    read_op->flags = flags;
    read_op->rmt_addr = addr;
    read_op->rmt_addrlen = addrlen;

    pj_list_insert_before(&key->read_list, read_op);
    PJ_FD_SET(key->fd, &ioqueue->rfdset);

    pj_lock_release(ioqueue->lock);
    return PJ_EPENDING;
}

/*
 * pj_ioqueue_send()
 *
 * Start asynchronous send() to the descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_send( pj_ioqueue_key_t *key,
                                     pj_ioqueue_op_key_t *op_key,
			             const void *data,
			             pj_ssize_t *length,
                                     unsigned flags)
{
    pj_ioqueue_t *ioqueue;
    struct write_operation *write_op;
    pj_status_t status;
    pj_ssize_t sent;

    PJ_ASSERT_RETURN(key && op_key && data && length, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* Fast track:
     *   Try to send data immediately, only if there's no pending write!
     * Note:
     *  We are speculating that the list is empty here without properly
     *  acquiring ioqueue's mutex first. This is intentional, to maximize
     *  performance via parallelism.
     *
     *  This should be safe, because:
     *      - by convention, we require caller to make sure that the
     *        key is not unregistered while other threads are invoking
     *        an operation on the same key.
     *      - pj_list_empty() is safe to be invoked by multiple threads,
     *        even when other threads are modifying the list.
     */
    if (pj_list_empty(&key->write_list)) {
        /*
         * See if data can be sent immediately.
         */
        sent = *length;
        status = pj_sock_send(key->fd, data, &sent, flags);
        if (status == PJ_SUCCESS) {
            /* Success! */
            *length = sent;
            return PJ_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL)) {
                return status;
            }
        }
    }

    /*
     * Schedule asynchronous send.
     */
    ioqueue = key->ioqueue;
    pj_lock_acquire(ioqueue->lock);

    write_op = (struct write_operation*)op_key;
    write_op->op = PJ_IOQUEUE_OP_SEND;
    write_op->buf = NULL;
    write_op->size = *length;
    write_op->written = 0;
    write_op->flags = flags;
    
    pj_list_insert_before(&key->write_list, write_op);
    PJ_FD_SET(key->fd, &ioqueue->wfdset);

    pj_lock_release(ioqueue->lock);

    return PJ_EPENDING;
}


/*
 * pj_ioqueue_sendto()
 *
 * Start asynchronous write() to the descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
			               const void *data,
			               pj_ssize_t *length,
                                       unsigned flags,
			               const pj_sockaddr_t *addr,
			               int addrlen)
{
    pj_ioqueue_t *ioqueue;
    struct write_operation *write_op;
    pj_status_t status;
    pj_ssize_t sent;

    PJ_ASSERT_RETURN(key && op_key && data && length, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* Fast track:
     *   Try to send data immediately, only if there's no pending write!
     * Note:
     *  We are speculating that the list is empty here without properly
     *  acquiring ioqueue's mutex first. This is intentional, to maximize
     *  performance via parallelism.
     *
     *  This should be safe, because:
     *      - by convention, we require caller to make sure that the
     *        key is not unregistered while other threads are invoking
     *        an operation on the same key.
     *      - pj_list_empty() is safe to be invoked by multiple threads,
     *        even when other threads are modifying the list.
     */
    if (pj_list_empty(&key->write_list)) {
        /*
         * See if data can be sent immediately.
         */
        sent = *length;
        status = pj_sock_sendto(key->fd, data, &sent, flags, addr, addrlen);
        if (status == PJ_SUCCESS) {
            /* Success! */
            *length = sent;
            return PJ_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL)) {
                return status;
            }
        }
    }

    /*
     * Check that address storage can hold the address parameter.
     */
    PJ_ASSERT_RETURN(addrlen <= sizeof(pj_sockaddr_in), PJ_EBUG);

    /*
     * Schedule asynchronous send.
     */
    ioqueue = key->ioqueue;
    pj_lock_acquire(ioqueue->lock);

    write_op = (struct write_operation*)op_key;
    write_op->op = PJ_IOQUEUE_OP_SEND_TO;
    write_op->buf = NULL;
    write_op->size = *length;
    write_op->written = 0;
    write_op->flags = flags;
    pj_memcpy(&write_op->rmt_addr, addr, addrlen);
    write_op->rmt_addrlen = addrlen;
    
    pj_list_insert_before(&key->write_list, write_op);
    PJ_FD_SET(key->fd, &ioqueue->wfdset);

    pj_lock_release(ioqueue->lock);

    return PJ_EPENDING;
}

#if PJ_HAS_TCP
/*
 * Initiate overlapped accept() operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_accept( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
			               pj_sock_t *new_sock,
			               pj_sockaddr_t *local,
			               pj_sockaddr_t *remote,
			               int *addrlen)
{
    pj_ioqueue_t *ioqueue;
    struct accept_operation *accept_op;
    pj_status_t status;

    /* check parameters. All must be specified! */
    PJ_ASSERT_RETURN(key && op_key && new_sock, PJ_EINVAL);

    /* Fast track:
     *  See if there's new connection available immediately.
     */
    if (pj_list_empty(&key->accept_list)) {
        status = pj_sock_accept(key->fd, new_sock, remote, addrlen);
        if (status == PJ_SUCCESS) {
            /* Yes! New connection is available! */
            if (local && addrlen) {
                status = pj_sock_getsockname(*new_sock, local, addrlen);
                if (status != PJ_SUCCESS) {
                    pj_sock_close(*new_sock);
                    *new_sock = PJ_INVALID_SOCKET;
                    return status;
                }
            }
            return PJ_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL)) {
                return status;
            }
        }
    }

    /*
     * No connection is available immediately.
     * Schedule accept() operation to be completed when there is incoming
     * connection available.
     */
    ioqueue = key->ioqueue;
    accept_op = (struct accept_operation*)op_key;

    pj_lock_acquire(ioqueue->lock);

    accept_op->op = PJ_IOQUEUE_OP_ACCEPT;
    accept_op->accept_fd = new_sock;
    accept_op->rmt_addr = remote;
    accept_op->addrlen= addrlen;
    accept_op->local_addr = local;

    pj_list_insert_before(&key->accept_list, accept_op);
    PJ_FD_SET(key->fd, &ioqueue->rfdset);

    pj_lock_release(ioqueue->lock);

    return PJ_EPENDING;
}

/*
 * Initiate overlapped connect() operation (well, it's non-blocking actually,
 * since there's no overlapped version of connect()).
 */
PJ_DEF(pj_status_t) pj_ioqueue_connect( pj_ioqueue_key_t *key,
					const pj_sockaddr_t *addr,
					int addrlen )
{
    pj_ioqueue_t *ioqueue;
    pj_status_t status;
    
    /* check parameters. All must be specified! */
    PJ_ASSERT_RETURN(key && addr && addrlen, PJ_EINVAL);

    /* Check if socket has not been marked for connecting */
    if (key->connecting != 0)
        return PJ_EPENDING;
    
    status = pj_sock_connect(key->fd, addr, addrlen);
    if (status == PJ_SUCCESS) {
	/* Connected! */
	return PJ_SUCCESS;
    } else {
	if (status == PJ_STATUS_FROM_OS(PJ_BLOCKING_CONNECT_ERROR_VAL)) {
	    /* Pending! */
            ioqueue = key->ioqueue;
	    pj_lock_acquire(ioqueue->lock);
	    key->connecting = PJ_TRUE;
	    PJ_FD_SET(key->fd, &ioqueue->wfdset);
	    PJ_FD_SET(key->fd, &ioqueue->xfdset);
	    pj_lock_release(ioqueue->lock);
	    return PJ_EPENDING;
	} else {
	    /* Error! */
	    return status;
	}
    }
}
#endif	/* PJ_HAS_TCP */


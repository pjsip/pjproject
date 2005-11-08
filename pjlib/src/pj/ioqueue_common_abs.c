/* $Id$ */

/*
 * ioqueue_common_abs.c
 *
 * This contains common functionalities to emulate proactor pattern with
 * various event dispatching mechanisms (e.g. select, epoll).
 *
 * This file will be included by the appropriate ioqueue implementation.
 * This file is NOT supposed to be compiled as stand-alone source.
 */

static void ioqueue_init( pj_ioqueue_t *ioqueue )
{
    ioqueue->lock = NULL;
    ioqueue->auto_delete_lock = 0;
}

static pj_status_t ioqueue_destroy(pj_ioqueue_t *ioqueue)
{
    if (ioqueue->auto_delete_lock && ioqueue->lock ) {
	pj_lock_release(ioqueue->lock);
        return pj_lock_destroy(ioqueue->lock);
    } else
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

    if (ioqueue->auto_delete_lock && ioqueue->lock) {
        pj_lock_destroy(ioqueue->lock);
    }

    ioqueue->lock = lock;
    ioqueue->auto_delete_lock = auto_delete;

    return PJ_SUCCESS;
}

static pj_status_t ioqueue_init_key( pj_pool_t *pool,
                                     pj_ioqueue_t *ioqueue,
                                     pj_ioqueue_key_t *key,
                                     pj_sock_t sock,
                                     void *user_data,
                                     const pj_ioqueue_callback *cb)
{
    pj_status_t rc;
    int optlen;

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

    /* Get socket type. When socket type is datagram, some optimization
     * will be performed during send to allow parallel send operations.
     */
    optlen = sizeof(key->fd_type);
    rc = pj_sock_getsockopt(sock, PJ_SOL_SOCKET, PJ_SO_TYPE,
                            &key->fd_type, &optlen);
    if (rc != PJ_SUCCESS)
        key->fd_type = PJ_SOCK_STREAM;

    /* Create mutex for the key. */
    rc = pj_mutex_create_simple(pool, NULL, &key->mutex);
    
    return rc;
}

static void ioqueue_destroy_key( pj_ioqueue_key_t *key )
{
    pj_mutex_destroy(key->mutex);
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

PJ_INLINE(int) key_has_pending_write(pj_ioqueue_key_t *key)
{
    return !pj_list_empty(&key->write_list);
}

PJ_INLINE(int) key_has_pending_read(pj_ioqueue_key_t *key)
{
    return !pj_list_empty(&key->read_list);
}

PJ_INLINE(int) key_has_pending_accept(pj_ioqueue_key_t *key)
{
#if PJ_HAS_TCP
    return !pj_list_empty(&key->accept_list);
#else
    return 0;
#endif
}

PJ_INLINE(int) key_has_pending_connect(pj_ioqueue_key_t *key)
{
    return key->connecting;
}


/*
 * ioqueue_dispatch_event()
 *
 * Report occurence of an event in the key to be processed by the
 * framework.
 */
void ioqueue_dispatch_write_event(pj_ioqueue_t *ioqueue, pj_ioqueue_key_t *h)
{
    /* Lock the key. */
    pj_mutex_lock(h->mutex);

#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0
    if (h->connecting) {
	/* Completion of connect() operation */
	pj_ssize_t bytes_transfered;

	/* Clear operation. */
	h->connecting = 0;

        ioqueue_remove_from_set(ioqueue, h->fd, WRITEABLE_EVENT);
        ioqueue_remove_from_set(ioqueue, h->fd, EXCEPTION_EVENT);

        /* Unlock; from this point we don't need to hold key's mutex. */
        pj_mutex_unlock(h->mutex);

#if (defined(PJ_HAS_SO_ERROR) && PJ_HAS_SO_ERROR!=0)
	/* from connect(2): 
	 * On Linux, use getsockopt to read the SO_ERROR option at
	 * level SOL_SOCKET to determine whether connect() completed
	 * successfully (if SO_ERROR is zero).
	 */
	{
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

	/* Call callback. */
        if (h->cb.on_connect_complete)
	    (*h->cb.on_connect_complete)(h, bytes_transfered);

        /* Done. */

    } else 
#endif /* PJ_HAS_TCP */
    if (key_has_pending_write(h)) {
	/* Socket is writable. */
        struct write_operation *write_op;
        pj_ssize_t sent;
        pj_status_t send_rc;

        /* Get the first in the queue. */
        write_op = h->write_list.next;

        /* For datagrams, we can remove the write_op from the list
         * so that send() can work in parallel.
         */
        if (h->fd_type == PJ_SOCK_DGRAM) {
            pj_list_erase(write_op);
            if (pj_list_empty(&h->write_list))
                ioqueue_remove_from_set(ioqueue, h->fd, WRITEABLE_EVENT);

            pj_mutex_unlock(h->mutex);
        }

        /* Send the data. 
         * Unfortunately we must do this while holding key's mutex, thus
         * preventing parallel write on a single key.. :-((
         */
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

        /* Are we finished with this buffer? */
        if (send_rc!=PJ_SUCCESS || 
            write_op->written == (pj_ssize_t)write_op->size ||
            h->fd_type == PJ_SOCK_DGRAM) 
        {
            if (h->fd_type != PJ_SOCK_DGRAM) {
                /* Write completion of the whole stream. */
                pj_list_erase(write_op);

                /* Clear operation if there's no more data to send. */
                if (pj_list_empty(&h->write_list))
                    ioqueue_remove_from_set(ioqueue, h->fd, WRITEABLE_EVENT);

                /* No need to hold mutex anymore */
                pj_mutex_unlock(h->mutex);
            }

	    /* Call callback. */
            if (h->cb.on_write_complete) {
	        (*h->cb.on_write_complete)(h, 
                                           (pj_ioqueue_op_key_t*)write_op,
                                           write_op->written);
            }

        } else {
            pj_mutex_unlock(h->mutex);
        }

        /* Done. */
    } else {
        /*
         * This is normal; execution may fall here when multiple threads
         * are signalled for the same event, but only one thread eventually
         * able to process the event.
         */
        pj_mutex_unlock(h->mutex);
    }
}

void ioqueue_dispatch_read_event( pj_ioqueue_t *ioqueue, pj_ioqueue_key_t *h )
{
    pj_status_t rc;

    /* Lock the key. */
    pj_mutex_lock(h->mutex);

#   if PJ_HAS_TCP
    if (!pj_list_empty(&h->accept_list)) {

        struct accept_operation *accept_op;
	
        /* Get one accept operation from the list. */
	accept_op = h->accept_list.next;
        pj_list_erase(accept_op);

	/* Clear bit in fdset if there is no more pending accept */
        if (pj_list_empty(&h->accept_list))
            ioqueue_remove_from_set(ioqueue, h->fd, READABLE_EVENT);

        /* Unlock; from this point we don't need to hold key's mutex. */
        pj_mutex_unlock(h->mutex);

	rc=pj_sock_accept(h->fd, accept_op->accept_fd, 
                          accept_op->rmt_addr, accept_op->addrlen);
	if (rc==PJ_SUCCESS && accept_op->local_addr) {
	    rc = pj_sock_getsockname(*accept_op->accept_fd, 
                                     accept_op->local_addr,
				     accept_op->addrlen);
	}

	/* Call callback. */
        if (h->cb.on_accept_complete) {
	    (*h->cb.on_accept_complete)(h, 
                                        (pj_ioqueue_op_key_t*)accept_op,
                                        *accept_op->accept_fd, rc);
	}

    }
    else
#   endif
    if (key_has_pending_read(h)) {
        struct read_operation *read_op;
        pj_ssize_t bytes_read;

        /* Get one pending read operation from the list. */
        read_op = h->read_list.next;
        pj_list_erase(read_op);

        /* Clear fdset if there is no pending read. */
        if (pj_list_empty(&h->read_list))
            ioqueue_remove_from_set(ioqueue, h->fd, READABLE_EVENT);

        /* Unlock; from this point we don't need to hold key's mutex. */
        pj_mutex_unlock(h->mutex);

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
#	    if defined(PJ_WIN32) && PJ_WIN32 != 0
                rc = pj_sock_recv(h->fd, read_op->buf, &bytes_read, 0);
                //rc = ReadFile((HANDLE)h->fd, read_op->buf, read_op->size,
                //              &bytes_read, NULL);
#           elif (defined(PJ_HAS_UNISTD_H) && PJ_HAS_UNISTD_H != 0)
                bytes_read = read(h->fd, read_op->buf, bytes_read);
                rc = (bytes_read >= 0) ? PJ_SUCCESS : pj_get_os_error();
#	    elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL != 0
                bytes_read = sys_read(h->fd, read_op->buf, bytes_read);
                rc = (bytes_read >= 0) ? PJ_SUCCESS : -bytes_read;
#           else
#               error "Implement read() for this platform!"
#           endif
        }
	
	if (rc != PJ_SUCCESS) {
#	    if defined(PJ_WIN32) && PJ_WIN32 != 0
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
#	    endif

            /* In any case we would report this to caller. */
            bytes_read = -rc;
	}

	/* Call callback. */
        if (h->cb.on_read_complete) {
	    (*h->cb.on_read_complete)(h, 
                                      (pj_ioqueue_op_key_t*)read_op,
                                      bytes_read);
        }

    } else {
        /*
         * This is normal; execution may fall here when multiple threads
         * are signalled for the same event, but only one thread eventually
         * able to process the event.
         */
        pj_mutex_unlock(h->mutex);
    }
}


void ioqueue_dispatch_exception_event( pj_ioqueue_t *ioqueue, 
                                       pj_ioqueue_key_t *h )
{
    pj_mutex_lock(h->mutex);

    if (!h->connecting) {
        /* It is possible that more than one thread was woken up, thus
         * the remaining thread will see h->connecting as zero because
         * it has been processed by other thread.
         */
        pj_mutex_unlock(h->mutex);
        return;
    }

    /* Clear operation. */
    h->connecting = 0;

    pj_mutex_unlock(h->mutex);

    ioqueue_remove_from_set(ioqueue, h->fd, WRITEABLE_EVENT);
    ioqueue_remove_from_set(ioqueue, h->fd, EXCEPTION_EVENT);

    /* Call callback. */
    if (h->cb.on_connect_complete)
	(*h->cb.on_connect_complete)(h, -1);
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
    read_op = (struct read_operation*)op_key;

    read_op->op = PJ_IOQUEUE_OP_RECV;
    read_op->buf = buffer;
    read_op->size = *length;
    read_op->flags = flags;

    pj_mutex_lock(key->mutex);
    pj_list_insert_before(&key->read_list, read_op);
    ioqueue_add_to_set(key->ioqueue, key->fd, READABLE_EVENT);
    pj_mutex_unlock(key->mutex);

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
    read_op = (struct read_operation*)op_key;

    read_op->op = PJ_IOQUEUE_OP_RECV_FROM;
    read_op->buf = buffer;
    read_op->size = *length;
    read_op->flags = flags;
    read_op->rmt_addr = addr;
    read_op->rmt_addrlen = addrlen;

    pj_mutex_lock(key->mutex);
    pj_list_insert_before(&key->read_list, read_op);
    ioqueue_add_to_set(key->ioqueue, key->fd, READABLE_EVENT);
    pj_mutex_unlock(key->mutex);

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
    write_op = (struct write_operation*)op_key;
    write_op->op = PJ_IOQUEUE_OP_SEND;
    write_op->buf = (void*)data;
    write_op->size = *length;
    write_op->written = 0;
    write_op->flags = flags;
    
    pj_mutex_lock(key->mutex);
    pj_list_insert_before(&key->write_list, write_op);
    ioqueue_add_to_set(key->ioqueue, key->fd, WRITEABLE_EVENT);
    pj_mutex_unlock(key->mutex);

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
    write_op = (struct write_operation*)op_key;
    write_op->op = PJ_IOQUEUE_OP_SEND_TO;
    write_op->buf = (void*)data;
    write_op->size = *length;
    write_op->written = 0;
    write_op->flags = flags;
    pj_memcpy(&write_op->rmt_addr, addr, addrlen);
    write_op->rmt_addrlen = addrlen;
    
    pj_mutex_lock(key->mutex);
    pj_list_insert_before(&key->write_list, write_op);
    ioqueue_add_to_set(key->ioqueue, key->fd, WRITEABLE_EVENT);
    pj_mutex_unlock(key->mutex);

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
    accept_op = (struct accept_operation*)op_key;

    accept_op->op = PJ_IOQUEUE_OP_ACCEPT;
    accept_op->accept_fd = new_sock;
    accept_op->rmt_addr = remote;
    accept_op->addrlen= addrlen;
    accept_op->local_addr = local;

    pj_mutex_lock(key->mutex);
    pj_list_insert_before(&key->accept_list, accept_op);
    ioqueue_add_to_set(key->ioqueue, key->fd, READABLE_EVENT);
    pj_mutex_unlock(key->mutex);

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
            pj_mutex_lock(key->mutex);
	    key->connecting = PJ_TRUE;
            ioqueue_add_to_set(key->ioqueue, key->fd, WRITEABLE_EVENT);
            ioqueue_add_to_set(key->ioqueue, key->fd, EXCEPTION_EVENT);
            pj_mutex_unlock(key->mutex);
	    return PJ_EPENDING;
	} else {
	    /* Error! */
	    return status;
	}
    }
}
#endif	/* PJ_HAS_TCP */


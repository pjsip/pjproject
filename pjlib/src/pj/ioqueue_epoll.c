/* $Header: /pjproject-0.3/pjlib/src/pj/ioqueue_epoll.c 4     10/29/05 10:27p Bennylp $ */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pj/ioqueue_epoll.c $
 * 
 * 4     10/29/05 10:27p Bennylp
 * Fixed misc warnings.
 * 
 * 3     10/29/05 11:49a Bennylp
 * Fixed warnings.
 * 
 * 2     10/29/05 11:31a Bennylp
 * Changed accept and lock.
 * 
 * 1     10/17/05 10:49p Bennylp
 * Created.
 * 
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

#if !defined(PJ_LINUX_KERNEL) || PJ_LINUX_KERNEL==0
    /*
     * Linux user mode
     */
#   include <sys/epoll.h>
#   include <errno.h>
#   include <unistd.h>

#   define epoll_data		data.ptr
#   define epoll_data_type	void*
#   define ioctl_val_type	unsigned long*
#   define getsockopt_val_ptr	int*
#   define os_getsockopt	getsockopt
#   define os_ioctl		ioctl
#   define os_read		read
#   define os_close		close
#   define os_epoll_create	epoll_create
#   define os_epoll_ctl		epoll_ctl
#   define os_epoll_wait	epoll_wait
#else
    /*
     * Linux kernel mode.
     */
#   include <linux/config.h>
#   include <linux/version.h>
#   if defined(MODVERSIONS)
#	include <linux/modversions.h>
#   endif
#   include <linux/kernel.h>
#   include <linux/poll.h>
#   include <linux/eventpoll.h>
#   include <linux/syscalls.h>
#   include <linux/errno.h>
#   include <linux/unistd.h>
#   include <asm/ioctls.h>
    enum EPOLL_EVENTS
    {
	EPOLLIN = 0x001,
	EPOLLOUT = 0x004,
	EPOLLERR = 0x008,
    };
#   define os_epoll_create		sys_epoll_create
    static int os_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
    {
	long rc;
	mm_segment_t oldfs = get_fs();
	set_fs(KERNEL_DS);
	rc = sys_epoll_ctl(epfd, op, fd, event);
	set_fs(oldfs);
	if (rc) {
	    errno = -rc;
	    return -1;
	} else {
	    return 0;
	}
    }
    static int os_epoll_wait(int epfd, struct epoll_event *events,
			  int maxevents, int timeout)
    {
	int count;
	mm_segment_t oldfs = get_fs();
	set_fs(KERNEL_DS);
	count = sys_epoll_wait(epfd, events, maxevents, timeout);
	set_fs(oldfs);
	return count;
    }
#   define os_close		sys_close
#   define os_getsockopt	pj_sock_getsockopt
    static int os_read(int fd, void *buf, size_t len)
    {
	long rc;
	mm_segment_t oldfs = get_fs();
	set_fs(KERNEL_DS);
	rc = sys_read(fd, buf, len);
	set_fs(oldfs);
	if (rc) {
	    errno = -rc;
	    return -1;
	} else {
	    return 0;
	}
    }
#   define socklen_t		unsigned
#   define ioctl_val_type	unsigned long
    int ioctl(int fd, int opt, ioctl_val_type value);
    static int os_ioctl(int fd, int opt, ioctl_val_type value)
    {
	int rc;
        mm_segment_t oldfs = get_fs();
	set_fs(KERNEL_DS);
	rc = ioctl(fd, opt, value);
	set_fs(oldfs);
	if (rc < 0) {
	    errno = -rc;
	    return rc;
	} else
	    return rc;
    }
#   define getsockopt_val_ptr	char*

#   define epoll_data		data
#   define epoll_data_type	__u32
#endif

#define THIS_FILE   "ioq_epoll"

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


//#define TRACE_(expr) PJ_LOG(3,expr)
#define TRACE_(expr)


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
 * This describes the I/O queue.
 */
struct pj_ioqueue_t
{
    pj_lock_t          *lock;
    pj_bool_t           auto_delete_lock;
    unsigned		max, count;
    pj_ioqueue_key_t	hlist;
    int			epfd;
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
    pj_list_init(&ioque->hlist);

    rc = pj_lock_create_recursive_mutex(pool, "ioq%p", &ioque->lock);
    if (rc != PJ_SUCCESS)
	return rc;

    ioque->auto_delete_lock = PJ_TRUE;
    ioque->epfd = os_epoll_create(max_fd);
    if (ioque->epfd < 0) {
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
    }
    
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
    PJ_ASSERT_RETURN(ioque, PJ_EINVAL);
    PJ_ASSERT_RETURN(ioque->epfd > 0, PJ_EINVALIDOP);

    pj_lock_acquire(ioque->lock);
    os_close(ioque->epfd);
    ioque->epfd = 0;
    if (ioque->auto_delete_lock)
        pj_lock_destroy(ioque->lock);
    
    return PJ_SUCCESS;
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
 * Register a socket to ioqueue.
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
    struct epoll_event ev;
    int status;
    pj_status_t rc = PJ_SUCCESS;
    
    PJ_ASSERT_RETURN(pool && ioque && sock != PJ_INVALID_SOCKET &&
                     cb && p_key, PJ_EINVAL);

    pj_lock_acquire(ioque->lock);

    if (ioque->count >= ioque->max) {
        rc = PJ_ETOOMANY;
	TRACE_((THIS_FILE, "pj_ioqueue_register_sock error: too many files"));
	goto on_return;
    }

    /* Set socket to nonblocking. */
    value = 1;
    if ((rc=os_ioctl(sock, FIONBIO, (ioctl_val_type)&value))) {
	TRACE_((THIS_FILE, "pj_ioqueue_register_sock error: ioctl rc=%d", 
                rc));
        rc = pj_get_netos_error();
	goto on_return;
    }

    /* Create key. */
    key = (pj_ioqueue_key_t*)pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));
    key->fd = sock;
    key->user_data = user_data;
    pj_memcpy(&key->cb, cb, sizeof(pj_ioqueue_callback));

    /* os_epoll_ctl. */
    ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;
    ev.epoll_data = (epoll_data_type)key;
    status = os_epoll_ctl(ioque->epfd, EPOLL_CTL_ADD, sock, &ev);
    if (status < 0) {
	rc = pj_get_os_error();
	TRACE_((THIS_FILE, 
                "pj_ioqueue_register_sock error: os_epoll_ctl rc=%d", 
                status));
	goto on_return;
    }
    
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
    struct epoll_event ev;
    int status;
    
    PJ_ASSERT_RETURN(ioque && key, PJ_EINVAL);

    pj_lock_acquire(ioque->lock);

    pj_assert(ioque->count > 0);
    --ioque->count;
    pj_list_erase(key);

    ev.events = 0;
    ev.epoll_data = (epoll_data_type)key;
    status = os_epoll_ctl( ioque->epfd, EPOLL_CTL_DEL, key->fd, &ev);
    if (status != 0) {
	pj_status_t rc = pj_get_os_error();
	pj_lock_release(ioque->lock);
	return rc;
    }

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


/*
 * pj_ioqueue_poll()
 *
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioque, const pj_time_val *timeout)
{
    int i, count, processed;
    struct epoll_event events[16];
    int msec;
    
    PJ_CHECK_STACK();

    msec = timeout ? PJ_TIME_VAL_MSEC(*timeout) : 9000;
    
    count = os_epoll_wait( ioque->epfd, events, PJ_ARRAY_SIZE(events), msec);
    if (count <= 0)
	return count;

    /* Lock ioqueue. */
    pj_lock_acquire(ioque->lock);

    processed = 0;

    for (i=0; i<count; ++i) {
	pj_ioqueue_key_t *h = (pj_ioqueue_key_t*)(epoll_data_type)
				events[i].epoll_data;
	pj_status_t rc;

	/*
	 * Check for completion of read operations.
	 */
	if ((events[i].events & EPOLLIN) && (PJ_IOQUEUE_IS_READ_OP(h->op))) {
	    pj_ssize_t bytes_read = h->rd_buflen;

	    if ((h->op & PJ_IOQUEUE_OP_RECV_FROM)) {
	        rc = pj_sock_recvfrom( h->fd, h->rd_buf, &bytes_read, 0,
				       h->rmt_addr, h->rmt_addrlen);
	    } else if ((h->op & PJ_IOQUEUE_OP_RECV)) {
	        rc = pj_sock_recv(h->fd, h->rd_buf, &bytes_read, 0);
	    } else {
		bytes_read = os_read( h->fd, h->rd_buf, bytes_read);
		rc = (bytes_read >= 0) ? PJ_SUCCESS : pj_get_os_error();
	    }
	    
	    if (rc != PJ_SUCCESS) {
	        bytes_read = -rc;
	    }

	    h->op &= ~(PJ_IOQUEUE_OP_READ | PJ_IOQUEUE_OP_RECV | 
		       PJ_IOQUEUE_OP_RECV_FROM);

	    /* Call callback. */
	    (*h->cb.on_read_complete)(h, bytes_read);

	    ++processed;
	}
	/*
	 * Check for completion of accept() operation.
	 */
	else if ((events[i].events & EPOLLIN) &&
		 (h->op & PJ_IOQUEUE_OP_ACCEPT)) 
	{
	    /* accept() must be the only operation specified on 
	     * server socket 
	     */
	    pj_assert( h->op == PJ_IOQUEUE_OP_ACCEPT);

	    rc = pj_sock_accept( h->fd, h->accept_fd, 
			         h->rmt_addr, h->rmt_addrlen);
	    if (rc==PJ_SUCCESS && h->local_addr) {
		rc = pj_sock_getsockname(*h->accept_fd, h->local_addr, 
				          h->local_addrlen);
	    }

	    h->op &= ~(PJ_IOQUEUE_OP_ACCEPT);

	    /* Call callback. */
	    (*h->cb.on_accept_complete)(h, *h->accept_fd, rc);
	    
	    ++processed;
	}

	/*
	 * Check for completion of write operations.
	 */
	if ((events[i].events & EPOLLOUT) && PJ_IOQUEUE_IS_WRITE_OP(h->op)) {
	    /* Completion of write(), send(), or sendto() operation. */

	    /* Clear operation. */
	    h->op &= ~(PJ_IOQUEUE_OP_WRITE | PJ_IOQUEUE_OP_SEND | 
                       PJ_IOQUEUE_OP_SEND_TO);

	    /* Call callback. */
	    /* All data must have been sent? */
	    (*h->cb.on_write_complete)(h, h->wr_buflen);

	    ++processed;
	}
#if PJ_HAS_TCP
	/*
	 * Check for completion of connect() operation.
	 */
	else if ((events[i].events & EPOLLOUT) && 
		 (h->op & PJ_IOQUEUE_OP_CONNECT)) 
	{
	    /* Completion of connect() operation */
	    pj_ssize_t bytes_transfered;

	    /* from connect(2): 
		* On Linux, use getsockopt to read the SO_ERROR option at
		* level SOL_SOCKET to determine whether connect() completed
		* successfully (if SO_ERROR is zero).
		*/
	    int value;
	    socklen_t vallen = sizeof(value);
	    int gs_rc = os_getsockopt(h->fd, SOL_SOCKET, SO_ERROR, 
                                      (getsockopt_val_ptr)&value, &vallen);
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

	    /* Clear operation. */
	    h->op &= (~PJ_IOQUEUE_OP_CONNECT);

	    /* Call callback. */
	    (*h->cb.on_connect_complete)(h, bytes_transfered);

	    ++processed;
	}
#endif /* PJ_HAS_TCP */
	
	/*
	 * Check for error condition.
	 */
	if (events[i].events & EPOLLERR) {
	    if (h->op & PJ_IOQUEUE_OP_CONNECT) {
		h->op &= ~PJ_IOQUEUE_OP_CONNECT;

		/* Call callback. */
		(*h->cb.on_connect_complete)(h, -1);

		++processed;
	    }
	}
    }
    
    pj_lock_release(ioque->lock);

    return processed;
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
	    pj_lock_release(ioqueue->lock);
	    return PJ_EPENDING;
	} else {
	    /* Error! */
	    return rc;
	}
    }
}
#endif	/* PJ_HAS_TCP */


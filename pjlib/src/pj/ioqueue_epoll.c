/* $Id$
 */
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
#   define ioctl_val_type	unsigned long
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

//#define TRACE_(expr) PJ_LOG(3,expr)
#define TRACE_(expr)

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
};

/*
 * This describes the I/O queue.
 */
struct pj_ioqueue_t
{
    DECLARE_COMMON_IOQUEUE

    unsigned		max, count;
    pj_ioqueue_key_t	hlist;
    int			epfd;
};

/* Include implementation for common abstraction after we declare
 * pj_ioqueue_key_t and pj_ioqueue_t.
 */
#include "ioqueue_common_abs.c"

/*
 * pj_ioqueue_name()
 */
PJ_DEF(const char*) pj_ioqueue_name(void)
{
#if defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL!=0
	return "epoll-kernel";
#else
	return "epoll";
#endif
}

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
    pj_lock_t *lock;

    /* Check that arguments are valid. */
    PJ_ASSERT_RETURN(pool != NULL && p_ioqueue != NULL && 
                     max_fd > 0, PJ_EINVAL);

    /* Check that size of pj_ioqueue_op_key_t is sufficient */
    PJ_ASSERT_RETURN(sizeof(pj_ioqueue_op_key_t)-sizeof(void*) >=
                     sizeof(union operation_key), PJ_EBUG);

    ioqueue = pj_pool_alloc(pool, sizeof(pj_ioqueue_t));

    ioqueue_init(ioqueue);

    ioqueue->max = max_fd;
    ioqueue->count = 0;
    pj_list_init(&ioqueue->hlist);

    rc = pj_lock_create_simple_mutex(pool, "ioq%p", &lock);
    if (rc != PJ_SUCCESS)
	return rc;

    rc = pj_ioqueue_set_lock(ioqueue, lock, PJ_TRUE);
    if (rc != PJ_SUCCESS)
        return rc;

    ioqueue->epfd = os_epoll_create(max_fd);
    if (ioqueue->epfd < 0) {
	ioqueue_destroy(ioqueue);
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
    }
    
    PJ_LOG(4, ("pjlib", "epoll I/O Queue created (%p)", ioqueue));

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
    PJ_ASSERT_RETURN(ioqueue, PJ_EINVAL);
    PJ_ASSERT_RETURN(ioqueue->epfd > 0, PJ_EINVALIDOP);

    pj_lock_acquire(ioqueue->lock);
    os_close(ioqueue->epfd);
    ioqueue->epfd = 0;
    return ioqueue_destroy(ioqueue);
}

/*
 * pj_ioqueue_register_sock()
 *
 * Register a socket to ioqueue.
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
    struct epoll_event ev;
    int status;
    pj_status_t rc = PJ_SUCCESS;
    
    PJ_ASSERT_RETURN(pool && ioqueue && sock != PJ_INVALID_SOCKET &&
                     cb && p_key, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

    if (ioqueue->count >= ioqueue->max) {
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
    rc = ioqueue_init_key(pool, ioqueue, key, sock, user_data, cb);
    if (rc != PJ_SUCCESS) {
	key = NULL;
	goto on_return;
    }

    /* os_epoll_ctl. */
    ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;
    ev.epoll_data = (epoll_data_type)key;
    status = os_epoll_ctl(ioqueue->epfd, EPOLL_CTL_ADD, sock, &ev);
    if (status < 0) {
	rc = pj_get_os_error();
	key = NULL;
	TRACE_((THIS_FILE, 
                "pj_ioqueue_register_sock error: os_epoll_ctl rc=%d", 
                status));
	goto on_return;
    }
    
    /* Register */
    pj_list_insert_before(&ioqueue->hlist, key);
    ++ioqueue->count;

on_return:
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
    struct epoll_event ev;
    int status;
    
    PJ_ASSERT_RETURN(key != NULL, PJ_EINVAL);

    ioqueue = key->ioqueue;
    pj_lock_acquire(ioqueue->lock);

    pj_assert(ioqueue->count > 0);
    --ioqueue->count;
    pj_list_erase(key);

    ev.events = 0;
    ev.epoll_data = (epoll_data_type)key;
    status = os_epoll_ctl( ioqueue->epfd, EPOLL_CTL_DEL, key->fd, &ev);
    if (status != 0) {
	pj_status_t rc = pj_get_os_error();
	pj_lock_release(ioqueue->lock);
	return rc;
    }

    pj_lock_release(ioqueue->lock);

    /* Destroy the key. */
    ioqueue_destroy_key(key);

    return PJ_SUCCESS;
}

/* ioqueue_remove_from_set()
 * This function is called from ioqueue_dispatch_event() to instruct
 * the ioqueue to remove the specified descriptor from ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_remove_from_set( pj_ioqueue_t *ioqueue,
                                     pj_sock_t fd, 
                                     enum ioqueue_event_type event_type)
{
}

/*
 * ioqueue_add_to_set()
 * This function is called from pj_ioqueue_recv(), pj_ioqueue_send() etc
 * to instruct the ioqueue to add the specified handle to ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_add_to_set( pj_ioqueue_t *ioqueue,
                                pj_sock_t fd,
                                enum ioqueue_event_type event_type )
{
}

/*
 * pj_ioqueue_poll()
 *
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioqueue, const pj_time_val *timeout)
{
    int i, count, processed;
    struct epoll_event events[PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL];
    int msec;
    struct queue {
	pj_ioqueue_key_t	*key;
	enum ioqueue_event_type	 event_type;
    } queue[PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL];
    
    PJ_CHECK_STACK();

    msec = timeout ? PJ_TIME_VAL_MSEC(*timeout) : 9000;
    
    count = os_epoll_wait( ioqueue->epfd, events, PJ_ARRAY_SIZE(events), msec);
    if (count <= 0)
	return count;

    /* Lock ioqueue. */
    pj_lock_acquire(ioqueue->lock);

    for (processed=0, i=0; i<count; ++i) {
	pj_ioqueue_key_t *h = (pj_ioqueue_key_t*)(epoll_data_type)
				events[i].epoll_data;

	/*
	 * Check readability.
	 */
	if ((events[i].events & EPOLLIN) && 
	    (key_has_pending_read(h) || key_has_pending_accept(h))) {
	    queue[processed].key = h;
	    queue[processed].event_type = READABLE_EVENT;
	    ++processed;
	}

	/*
	 * Check for writeability.
	 */
	if ((events[i].events & EPOLLOUT) && key_has_pending_write(h)) {
	    queue[processed].key = h;
	    queue[processed].event_type = WRITEABLE_EVENT;
	    ++processed;
	}

#if PJ_HAS_TCP
	/*
	 * Check for completion of connect() operation.
	 */
	if ((events[i].events & EPOLLOUT) && (h->connecting)) {
	    queue[processed].key = h;
	    queue[processed].event_type = WRITEABLE_EVENT;
	    ++processed;
	}
#endif /* PJ_HAS_TCP */
	
	/*
	 * Check for error condition.
	 */
	if (events[i].events & EPOLLERR && (h->connecting)) {
	    queue[processed].key = h;
	    queue[processed].event_type = EXCEPTION_EVENT;
	    ++processed;
	}
    }
    pj_lock_release(ioqueue->lock);

    /* Now process the events. */
    for (i=0; i<processed; ++i) {
	switch (queue[i].event_type) {
        case READABLE_EVENT:
            ioqueue_dispatch_read_event(ioqueue, queue[i].key);
            break;
        case WRITEABLE_EVENT:
            ioqueue_dispatch_write_event(ioqueue, queue[i].key);
            break;
        case EXCEPTION_EVENT:
            ioqueue_dispatch_exception_event(ioqueue, queue[i].key);
            break;
        case NO_EVENT:
            pj_assert(!"Invalid event!");
            break;
        }
    }

    return processed;
}


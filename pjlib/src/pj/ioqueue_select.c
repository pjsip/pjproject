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
 * Include declaration from common abstraction.
 */
#include "ioqueue_common_abs.h"

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
/*
#if defined(PJ_DEBUG) && PJ_DEBUG != 0
#  define VALIDATE_FD_SET		1
#else
#  define VALIDATE_FD_SET		0
#endif
*/
#define VALIDATE_FD_SET     0

/*
 * This describes each key.
 */
struct pj_ioqueue_key_t
{
    DECLARE_COMMON_KEY
};

/*
 * This describes the I/O queue itself.
 */
struct pj_ioqueue_t
{
    DECLARE_COMMON_IOQUEUE

    unsigned		max, count;
    pj_ioqueue_key_t	key_list;
    pj_fd_set_t		rfdset;
    pj_fd_set_t		wfdset;
#if PJ_HAS_TCP
    pj_fd_set_t		xfdset;
#endif
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
    return "select";
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
    pj_lock_t *lock;
    pj_status_t rc;

    /* Check that arguments are valid. */
    PJ_ASSERT_RETURN(pool != NULL && p_ioqueue != NULL && 
                     max_fd > 0 && max_fd <= PJ_IOQUEUE_MAX_HANDLES, 
                     PJ_EINVAL);

    /* Check that size of pj_ioqueue_op_key_t is sufficient */
    PJ_ASSERT_RETURN(sizeof(pj_ioqueue_op_key_t)-sizeof(void*) >=
                     sizeof(union operation_key), PJ_EBUG);

    ioqueue = pj_pool_alloc(pool, sizeof(pj_ioqueue_t));

    ioqueue_init(ioqueue);

    ioqueue->max = max_fd;
    ioqueue->count = 0;
    PJ_FD_ZERO(&ioqueue->rfdset);
    PJ_FD_ZERO(&ioqueue->wfdset);
#if PJ_HAS_TCP
    PJ_FD_ZERO(&ioqueue->xfdset);
#endif
    pj_list_init(&ioqueue->key_list);

    rc = pj_lock_create_simple_mutex(pool, "ioq%p", &lock);
    if (rc != PJ_SUCCESS)
	return rc;

    rc = pj_ioqueue_set_lock(ioqueue, lock, PJ_TRUE);
    if (rc != PJ_SUCCESS)
        return rc;

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
    PJ_ASSERT_RETURN(ioqueue, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);
    return ioqueue_destroy(ioqueue);
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
    rc = ioqueue_init_key(pool, ioqueue, key, sock, user_data, cb);
    if (rc != PJ_SUCCESS) {
	key = NULL;
	goto on_return;
    }

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

    /* ioqueue_destroy may try to acquire key's mutex.
     * Since normally the order of locking is to lock key's mutex first
     * then ioqueue's mutex, ioqueue_destroy may deadlock unless we
     * release ioqueue's mutex first.
     */
    pj_lock_release(ioqueue->lock);

    /* Destroy the key. */
    ioqueue_destroy_key(key);

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

    /*
     * This basicly would not work anymore.
     * We need to lock key before performing the check, but we can't do
     * so because we're holding ioqueue mutex. If we acquire key's mutex
     * now, the will cause deadlock.
     */
    pj_assert(0);

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


/* ioqueue_remove_from_set()
 * This function is called from ioqueue_dispatch_event() to instruct
 * the ioqueue to remove the specified descriptor from ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_remove_from_set( pj_ioqueue_t *ioqueue,
                                     pj_sock_t fd, 
                                     enum ioqueue_event_type event_type)
{
    pj_lock_acquire(ioqueue->lock);

    if (event_type == READABLE_EVENT)
        PJ_FD_CLR((pj_sock_t)fd, &ioqueue->rfdset);
    else if (event_type == WRITEABLE_EVENT)
        PJ_FD_CLR((pj_sock_t)fd, &ioqueue->wfdset);
    else if (event_type == EXCEPTION_EVENT)
        PJ_FD_CLR((pj_sock_t)fd, &ioqueue->xfdset);
    else
        pj_assert(0);

    pj_lock_release(ioqueue->lock);
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
    pj_lock_acquire(ioqueue->lock);

    if (event_type == READABLE_EVENT)
        PJ_FD_SET((pj_sock_t)fd, &ioqueue->rfdset);
    else if (event_type == WRITEABLE_EVENT)
        PJ_FD_SET((pj_sock_t)fd, &ioqueue->wfdset);
    else if (event_type == EXCEPTION_EVENT)
        PJ_FD_SET((pj_sock_t)fd, &ioqueue->xfdset);
    else
        pj_assert(0);

    pj_lock_release(ioqueue->lock);
}

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
    int count, counter;
    pj_ioqueue_key_t *h;
    struct event
    {
        pj_ioqueue_key_t	*key;
        enum ioqueue_event_type  event_type;
    } event[PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL];

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
    else if (count > PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL)
        count = PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL;

    /* Scan descriptor sets for event and add the events in the event
     * array to be processed later in this function. We do this so that
     * events can be processed in parallel without holding ioqueue lock.
     */
    pj_lock_acquire(ioqueue->lock);

    counter = 0;

    /* Scan for writable sockets first to handle piggy-back data
     * coming with accept().
     */
    h = ioqueue->key_list.next;
    for ( ; h!=&ioqueue->key_list && counter<count; h = h->next) {
	if ( (key_has_pending_write(h) || key_has_pending_connect(h))
	     && PJ_FD_ISSET(h->fd, &wfdset))
        {
            event[counter].key = h;
            event[counter].event_type = WRITEABLE_EVENT;
            ++counter;
        }

        /* Scan for readable socket. */
	if ((key_has_pending_read(h) || key_has_pending_accept(h))
            && PJ_FD_ISSET(h->fd, &rfdset))
        {
            event[counter].key = h;
            event[counter].event_type = READABLE_EVENT;
            ++counter;
	}

#if PJ_HAS_TCP
        if (key_has_pending_connect(h) && PJ_FD_ISSET(h->fd, &xfdset)) {
            event[counter].key = h;
            event[counter].event_type = EXCEPTION_EVENT;
            ++counter;
        }
#endif
    }

    pj_lock_release(ioqueue->lock);

    count = counter;

    /* Now process all events. The dispatch functions will take care
     * of locking in each of the key
     */
    for (counter=0; counter<count; ++counter) {
        switch (event[counter].event_type) {
        case READABLE_EVENT:
            ioqueue_dispatch_read_event(ioqueue, event[counter].key);
            break;
        case WRITEABLE_EVENT:
            ioqueue_dispatch_write_event(ioqueue, event[counter].key);
            break;
        case EXCEPTION_EVENT:
            ioqueue_dispatch_exception_event(ioqueue, event[counter].key);
            break;
        case NO_EVENT:
            pj_assert(!"Invalid event!");
            break;
        }
    }

    return count;
}


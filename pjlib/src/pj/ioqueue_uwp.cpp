/* $Id$ */
/* 
 * Copyright (C) 2016 Teluu Inc. (http://www.teluu.com)
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
/*
#include <pj/ioqueue.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/list.h>
#include <pj/lock.h>
#include <pj/pool.h>
#include <pj/string.h>

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
#include <pj/sock_qos.h>
#include <pj/errno.h>
#include <pj/rand.h>
*/

#include <pj/ioqueue.h>
#include <pj/sock.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/lock.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/unicode.h>
#include <pj/compat/socket.h>

#include <ppltasks.h>
#include <string>

#include "sock_uwp.h"


/*
 * IO Queue structure.
 */
struct pj_ioqueue_t
{
    int		     dummy;
};


/*
 * IO Queue key structure.
 */
struct pj_ioqueue_key_t
{
    pj_sock_t		     sock;
    void		    *user_data;
    pj_ioqueue_callback	     cb;
};


static void on_read(PjUwpSocket *s, int bytes_read)
{
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)s->user_data;
    if (key->cb.on_read_complete) {
	(*key->cb.on_read_complete)(key, (pj_ioqueue_op_key_t*)s->read_userdata, bytes_read);
    }
    s->read_userdata = NULL;
}

static void on_write(PjUwpSocket *s, int bytes_sent)
{
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)s->user_data;
    if (key->cb.on_write_complete) {
	(*key->cb.on_write_complete)(key, (pj_ioqueue_op_key_t*)s->write_userdata, bytes_sent);
    }
    s->write_userdata = NULL;
}

static void on_accept(PjUwpSocket *s, pj_status_t status)
{
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)s->user_data;
    if (key->cb.on_accept_complete) {
	if (status == PJ_SUCCESS) {
	    pj_sock_t new_sock;
	    pj_sockaddr addr;
	    int addrlen;
	    status = pj_sock_accept(key->sock, &new_sock, &addr, &addrlen);
	    (*key->cb.on_accept_complete)(key, (pj_ioqueue_op_key_t*)s->accept_userdata, new_sock, status);
	} else {
	    (*key->cb.on_accept_complete)(key, (pj_ioqueue_op_key_t*)s->accept_userdata, NULL, status);
	}
    }
    s->accept_userdata = NULL;
}

static void on_connect(PjUwpSocket *s, pj_status_t status)
{
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)s->user_data;
    if (key->cb.on_connect_complete) {
	(*key->cb.on_connect_complete)(key, status);
    }
}


/*
 * Return the name of the ioqueue implementation.
 */
PJ_DEF(const char*) pj_ioqueue_name(void)
{
    return "ioqueue-uwp";
}


/*
 * Create a new I/O Queue framework.
 */
PJ_DEF(pj_status_t) pj_ioqueue_create(	pj_pool_t *pool, 
					pj_size_t max_fd,
					pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioq;

    PJ_UNUSED_ARG(max_fd);

    ioq = PJ_POOL_ZALLOC_T(pool, pj_ioqueue_t);
    *p_ioqueue = ioq;
    return PJ_SUCCESS;
}


/*
 * Destroy the I/O queue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioq )
{
    PJ_UNUSED_ARG(ioq);
    return PJ_SUCCESS;
}


/*
 * Set the lock object to be used by the I/O Queue. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_lock( pj_ioqueue_t *ioq, 
					 pj_lock_t *lock,
					 pj_bool_t auto_delete )
{
    /* Don't really need lock for now */
    PJ_UNUSED_ARG(ioq);
    
    if (auto_delete) {
	pj_lock_destroy(lock);
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_set_default_concurrency(pj_ioqueue_t *ioqueue,
						       pj_bool_t allow)
{
    /* Not supported, just return PJ_SUCCESS silently */
    PJ_UNUSED_ARG(ioqueue);
    PJ_UNUSED_ARG(allow);
    return PJ_SUCCESS;
}

/*
 * Register a socket to the I/O queue framework. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					      pj_ioqueue_t *ioq,
					      pj_sock_t sock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key )
{
    PJ_UNUSED_ARG(ioq);

    pj_ioqueue_key_t *key;

    key = PJ_POOL_ZALLOC_T(pool, pj_ioqueue_key_t);
    key->sock = sock;
    key->user_data = user_data;    
    pj_memcpy(&key->cb, cb, sizeof(pj_ioqueue_callback));

    PjUwpSocket *s = (PjUwpSocket*)sock;
    s->is_blocking = PJ_FALSE;
    s->user_data = key;
    s->on_read = &on_read;
    s->on_write = &on_write;
    s->on_accept = &on_accept;
    s->on_connect = &on_connect;

    *p_key = key;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_register_sock2(pj_pool_t *pool,
					      pj_ioqueue_t *ioqueue,
					      pj_sock_t sock,
					      pj_grp_lock_t *grp_lock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key)
{
    PJ_UNUSED_ARG(grp_lock);

    return pj_ioqueue_register_sock(pool, ioqueue, sock, user_data, cb, p_key);
}

/*
 * Unregister from the I/O Queue framework. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key )
{
    if (key == NULL || key->sock == NULL)
	return PJ_SUCCESS;

    if (key->sock)
	pj_sock_close(key->sock);
    key->sock = NULL;

    return PJ_SUCCESS;
}


/*
 * Get user data associated with an ioqueue key.
 */
PJ_DEF(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key )
{
    return key->user_data;
}


/*
 * Set or change the user data to be associated with the file descriptor or
 * handle or socket descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_user_data( pj_ioqueue_key_t *key,
                                              void *user_data,
                                              void **old_data)
{
    if (old_data)
	*old_data = key->user_data;
    key->user_data= user_data;

    return PJ_SUCCESS;
}


/*
 * Initialize operation key.
 */
PJ_DEF(void) pj_ioqueue_op_key_init( pj_ioqueue_op_key_t *op_key,
				     pj_size_t size )
{
    pj_bzero(op_key, size);
}


/*
 * Check if operation is pending on the specified operation key.
 */
PJ_DEF(pj_bool_t) pj_ioqueue_is_pending( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key )
{
    PJ_UNUSED_ARG(key);
    PJ_UNUSED_ARG(op_key);
    return PJ_FALSE;
}


/*
 * Post completion status to the specified operation key and call the
 * appropriate callback. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_post_completion( pj_ioqueue_key_t *key,
                                                pj_ioqueue_op_key_t *op_key,
                                                pj_ssize_t bytes_status )
{
    PJ_UNUSED_ARG(key);
    PJ_UNUSED_ARG(op_key);
    PJ_UNUSED_ARG(bytes_status);
    return PJ_ENOTSUP;
}


#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
/**
 * Instruct I/O Queue to accept incoming connection on the specified 
 * listening socket.
 */
PJ_DEF(pj_status_t) pj_ioqueue_accept( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
				       pj_sock_t *new_sock,
				       pj_sockaddr_t *local,
				       pj_sockaddr_t *remote,
				       int *addrlen )
{
    PJ_UNUSED_ARG(new_sock);
    PJ_UNUSED_ARG(local);
    PJ_UNUSED_ARG(remote);
    PJ_UNUSED_ARG(addrlen);

    PjUwpSocket *s = (PjUwpSocket*)key->sock;
    s->accept_userdata = op_key;
    return pj_sock_listen(key->sock, 0);
}


/*
 * Initiate non-blocking socket connect.
 */
PJ_DEF(pj_status_t) pj_ioqueue_connect( pj_ioqueue_key_t *key,
					const pj_sockaddr_t *addr,
					int addrlen )
{
    return pj_sock_connect(key->sock, addr, addrlen);
}


#endif	/* PJ_HAS_TCP */

/*
 * Poll the I/O Queue for completed events.
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioq,
			     const pj_time_val *timeout)
{
    /* Polling is not necessary on uwp, since all async activities
     * are registered to active scheduler.
     */
    PJ_UNUSED_ARG(ioq);
    PJ_UNUSED_ARG(timeout);
    return 0;
}


/*
 * Instruct the I/O Queue to read from the specified handle.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recv( pj_ioqueue_key_t *key,
                                     pj_ioqueue_op_key_t *op_key,
				     void *buffer,
				     pj_ssize_t *length,
				     pj_uint32_t flags )
{
    PjUwpSocket *s = (PjUwpSocket*)key->sock;
    s->read_userdata = op_key;
    return pj_sock_recv(key->sock, buffer, length, flags);
}


/*
 * This function behaves similarly as #pj_ioqueue_recv(), except that it is
 * normally called for socket, and the remote address will also be returned
 * along with the data.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key,
					 void *buffer,
					 pj_ssize_t *length,
                                         pj_uint32_t flags,
					 pj_sockaddr_t *addr,
					 int *addrlen)
{
    PjUwpSocket *s = (PjUwpSocket*)key->sock;
    s->read_userdata = op_key;
    return pj_sock_recvfrom(key->sock, buffer, length, flags, addr, addrlen);
}


/*
 * Instruct the I/O Queue to write to the handle.
 */
PJ_DEF(pj_status_t) pj_ioqueue_send( pj_ioqueue_key_t *key,
                                     pj_ioqueue_op_key_t *op_key,
				     const void *data,
				     pj_ssize_t *length,
				     pj_uint32_t flags )
{
    PjUwpSocket *s = (PjUwpSocket*)key->sock;
    s->write_userdata = op_key;
    return pj_sock_send(key->sock, data, length, flags);
}


/*
 * Instruct the I/O Queue to write to the handle.
 */
PJ_DEF(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
				       const void *data,
				       pj_ssize_t *length,
                                       pj_uint32_t flags,
				       const pj_sockaddr_t *addr,
				       int addrlen)
{
    PjUwpSocket *s = (PjUwpSocket*)key->sock;
    s->write_userdata = op_key;
    return pj_sock_sendto(key->sock, data, length, flags, addr, addrlen);
}

PJ_DEF(pj_status_t) pj_ioqueue_set_concurrency(pj_ioqueue_key_t *key,
					       pj_bool_t allow)
{
	/* Not supported, just return PJ_SUCCESS silently */
	PJ_UNUSED_ARG(key);
	PJ_UNUSED_ARG(allow);
	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_lock_key(pj_ioqueue_key_t *key)
{
	/* Not supported, just return PJ_SUCCESS silently */
	PJ_UNUSED_ARG(key);
	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_unlock_key(pj_ioqueue_key_t *key)
{
	/* Not supported, just return PJ_SUCCESS silently */
	PJ_UNUSED_ARG(key);
	return PJ_SUCCESS;
}

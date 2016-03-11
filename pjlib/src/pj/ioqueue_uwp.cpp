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
#include <pj/ioqueue.h>
#include <pj/errno.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>

#include <ppltasks.h>
#include <string>

#define THIS_FILE   "ioq_uwp"

#include "sock_uwp.h"
#include "ioqueue_common_abs.h"

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
    pj_thread_desc   thread_desc[16];
    unsigned	     thread_cnt;
};


#include "ioqueue_common_abs.c"

static void ioqueue_remove_from_set( pj_ioqueue_t *ioqueue,
				     pj_ioqueue_key_t *key, 
				     enum ioqueue_event_type event_type)
{
    PJ_UNUSED_ARG(ioqueue);
    PJ_UNUSED_ARG(key);
    PJ_UNUSED_ARG(event_type);
}


static void start_next_read(pj_ioqueue_key_t *key)
{
    if (key_has_pending_read(key)) {
	PjUwpSocket *s = (PjUwpSocket*)key->fd;
	struct read_operation *op;
	op = (struct read_operation*)key->read_list.next;

	if (op->op == PJ_IOQUEUE_OP_RECV)
	    s->Recv(NULL, (pj_ssize_t*)&op->size);
	else
	    s->RecvFrom(NULL, (pj_ssize_t*)&op->size, NULL);
    }
}


static void start_next_write(pj_ioqueue_key_t *key)
{
    if (key_has_pending_write(key)) {
	PjUwpSocket *s = (PjUwpSocket*)key->fd;
	struct write_operation *op;
	op = (struct write_operation*)key->write_list.next;

	if (op->op == PJ_IOQUEUE_OP_SEND)
	    s->Send(op->buf, (pj_ssize_t*)&op->size);
	else
	    s->SendTo(op->buf, (pj_ssize_t*)&op->size, &op->rmt_addr);
    }
}


static void ioqueue_add_to_set( pj_ioqueue_t *ioqueue,
				pj_ioqueue_key_t *key,
				enum ioqueue_event_type event_type )
{
    PJ_UNUSED_ARG(ioqueue);

    if (event_type == READABLE_EVENT) {
	/* This is either recv, recvfrom, or accept, do nothing on accept */
	start_next_read(key);
    } else if (event_type == WRITEABLE_EVENT) {
	/* This is either send, sendto, or connect, do nothing on connect */
	//start_next_write(key);
    }
}


static void check_thread(pj_ioqueue_t *ioq) {
    if (pj_thread_is_registered())
	return;

    pj_thread_t *t;
    char tmp[16];
    pj_ansi_snprintf(tmp, sizeof(tmp), "UwpThread%02d", ioq->thread_cnt);
    pj_thread_register(tmp, ioq->thread_desc[ioq->thread_cnt++], &t);
    pj_assert(ioq->thread_cnt < PJ_ARRAY_SIZE(ioq->thread_desc));
    ioq->thread_cnt %= PJ_ARRAY_SIZE(ioq->thread_desc);
}

static void on_read(PjUwpSocket *s, int bytes_read)
{
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)s->GetUserData();
    pj_ioqueue_t *ioq = key->ioqueue;
    check_thread(ioq);

    ioqueue_dispatch_read_event(key->ioqueue, key);
    
    if (bytes_read > 0)
	start_next_read(key);
}

static void on_write(PjUwpSocket *s, int bytes_sent)
{
    PJ_UNUSED_ARG(bytes_sent);
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)s->GetUserData();
    pj_ioqueue_t *ioq = key->ioqueue;
    check_thread(ioq);

    ioqueue_dispatch_write_event(key->ioqueue, key);

    //start_next_write(key);
}

static void on_accept(PjUwpSocket *s)
{
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)s->GetUserData();
    pj_ioqueue_t *ioq = key->ioqueue;
    check_thread(ioq);

    ioqueue_dispatch_read_event(key->ioqueue, key);
}

static void on_connect(PjUwpSocket *s, pj_status_t status)
{
    PJ_UNUSED_ARG(status);
    pj_ioqueue_key_t *key = (pj_ioqueue_key_t*)s->GetUserData();
    pj_ioqueue_t *ioq = key->ioqueue;
    check_thread(ioq);

    ioqueue_dispatch_write_event(key->ioqueue, key);
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
    pj_lock_t *lock;
    pj_status_t rc;

    PJ_UNUSED_ARG(max_fd);

    ioq = PJ_POOL_ZALLOC_T(pool, pj_ioqueue_t);

    /* Create and init ioqueue mutex */
    rc = pj_lock_create_null_mutex(pool, "ioq%p", &lock);
    if (rc != PJ_SUCCESS)
	return rc;

    rc = pj_ioqueue_set_lock(ioq, lock, PJ_TRUE);
    if (rc != PJ_SUCCESS)
	return rc;

    PJ_LOG(4, ("pjlib", "select() I/O Queue created (%p)", ioq));

    *p_ioqueue = ioq;
    return PJ_SUCCESS;
}


/*
 * Destroy the I/O queue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioq )
{
    return ioqueue_destroy(ioq);
}


/*
 * Register a socket to the I/O queue framework. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					      pj_ioqueue_t *ioqueue,
					      pj_sock_t sock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key )
{
    return pj_ioqueue_register_sock2(pool, ioqueue, sock, NULL, user_data,
				     cb, p_key);
}

PJ_DEF(pj_status_t) pj_ioqueue_register_sock2(pj_pool_t *pool,
					      pj_ioqueue_t *ioqueue,
					      pj_sock_t sock,
					      pj_grp_lock_t *grp_lock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key)
{
    PjUwpSocketCallback uwp_cb =
			    { &on_read, &on_write, &on_accept, &on_connect };
    pj_ioqueue_key_t *key;
    pj_status_t rc;

    pj_lock_acquire(ioqueue->lock);

    key = PJ_POOL_ZALLOC_T(pool, pj_ioqueue_key_t);
    rc = ioqueue_init_key(pool, ioqueue, key, sock, grp_lock, user_data, cb);
    if (rc != PJ_SUCCESS) {
	key = NULL;
	goto on_return;
    }

    /* Create ioqueue key lock, if not yet */
    if (!key->lock) {
	rc = pj_lock_create_simple_mutex(pool, NULL, &key->lock);
	if (rc != PJ_SUCCESS) {
	    key = NULL;
	    goto on_return;
	}
    }

    PjUwpSocket *s = (PjUwpSocket*)sock;
    s->SetNonBlocking(&uwp_cb, key);

on_return:
    if (rc != PJ_SUCCESS) {
	if (key && key->grp_lock)
	    pj_grp_lock_dec_ref_dbg(key->grp_lock, "ioqueue", 0);
    }
    *p_key = key;
    pj_lock_release(ioqueue->lock);

    return rc;

}

/*
 * Unregister from the I/O Queue framework. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key )
{
    pj_ioqueue_t *ioqueue;

    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    ioqueue = key->ioqueue;

    /* Lock the key to make sure no callback is simultaneously modifying
     * the key. We need to lock the key before ioqueue here to prevent
     * deadlock.
     */
    pj_ioqueue_lock_key(key);

    /* Also lock ioqueue */
    pj_lock_acquire(ioqueue->lock);

    /* Close socket. */
    pj_sock_close(key->fd);

    /* Clear callback */
    key->cb.on_accept_complete = NULL;
    key->cb.on_connect_complete = NULL;
    key->cb.on_read_complete = NULL;
    key->cb.on_write_complete = NULL;

    pj_lock_release(ioqueue->lock);

    if (key->grp_lock) {
	pj_grp_lock_t *grp_lock = key->grp_lock;
	pj_grp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
	pj_grp_lock_release(grp_lock);
    } else {
	pj_ioqueue_unlock_key(key);
    }

    pj_lock_destroy(key->lock);

    return PJ_SUCCESS;
}


/*
 * Poll the I/O Queue for completed events.
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioq,
			     const pj_time_val *timeout)
{
    /* Polling is not necessary on UWP, since each socket handles
     * its events already.
     */
    PJ_UNUSED_ARG(ioq);

    pj_thread_sleep(PJ_TIME_VAL_MSEC(*timeout));

    return 0;
}


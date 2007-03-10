/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#include "server.h"

struct worker
{
    pj_ioqueue_op_key_t	read_key;
    unsigned		index;
    pj_uint8_t		readbuf[4000];
    pj_sockaddr		src_addr;
    int			src_addr_len;
};

struct pj_stun_usage
{
    pj_pool_t		*pool;
    pj_stun_server	*srv;
    pj_mutex_t		*mutex;
    pj_stun_usage_cb	 cb;
    int			 type;
    pj_sock_t		 sock;
    pj_ioqueue_key_t	*key;
    unsigned		 worker_cnt;
    struct worker	*worker;

    pj_ioqueue_op_key_t	*send_key;
    unsigned		 send_count, send_index;

    pj_bool_t		 quitting;
    void		*user_data;
};


static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);

/*
 * Create STUN usage.
 */
PJ_DEF(pj_status_t) pj_stun_usage_create( pj_stun_server *srv,
					  const char *name,
					  const pj_stun_usage_cb *cb,
					  int family,
					  int type,
					  int protocol,
					  const pj_sockaddr_t *local_addr,
					  int addr_len,
					  pj_stun_usage **p_usage)
{
    pj_stun_server_info *si;
    pj_pool_t *pool;
    pj_stun_usage *usage;
    pj_ioqueue_callback ioqueue_cb;
    unsigned i;
    pj_status_t status;

    si = pj_stun_server_get_info(srv);

    pool = pj_pool_create(si->pf, name, 4000, 4000, NULL);
    usage = PJ_POOL_ZALLOC_T(pool, pj_stun_usage);
    usage->pool = pool;
    usage->srv = srv;

    status = pj_mutex_create_simple(pool, name, &usage->mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    usage->type = type;
    status = pj_sock_socket(family, type, protocol, &usage->sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_sock_bind(usage->sock, local_addr, addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_bzero(&ioqueue_cb, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = &on_read_complete;
    status = pj_ioqueue_register_sock(usage->pool, si->ioqueue, usage->sock,
				      usage, &ioqueue_cb, &usage->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    usage->worker_cnt = si->thread_cnt;
    usage->worker = pj_pool_calloc(pool, si->thread_cnt, 
				   sizeof(struct worker));
    for (i=0; i<si->thread_cnt; ++i) {
	pj_ioqueue_op_key_init(&usage->worker[i].read_key, 
			       sizeof(usage->worker[i].read_key));
	usage->worker[i].index = i;
    }

    usage->send_count = usage->worker_cnt * 2;
    usage->send_key = pj_pool_calloc(pool, usage->send_count, 
				     sizeof(pj_ioqueue_op_key_t));
    for (i=0; i<usage->send_count; ++i) {
	pj_ioqueue_op_key_init(&usage->send_key[i], 
			       sizeof(usage->send_key[i]));
    }

    for (i=0; i<si->thread_cnt; ++i) {
	pj_ssize_t size;

	size = sizeof(usage->worker[i].readbuf);
	usage->worker[i].src_addr_len = sizeof(usage->worker[i].src_addr);
	status = pj_ioqueue_recvfrom(usage->key, &usage->worker[i].read_key,
				     usage->worker[i].readbuf, &size, 
				     PJ_IOQUEUE_ALWAYS_ASYNC,
				     &usage->worker[i].src_addr,
				     &usage->worker[i].src_addr_len);
	if (status != PJ_EPENDING)
	    goto on_error;
    }

    pj_stun_server_register_usage(srv, usage);

    /* Only after everything has been initialized we copy the callback,
     * to prevent callback from being called when we encounter error
     * during initialiation (decendant would not expect this).
     */
    pj_memcpy(&usage->cb, cb, sizeof(*cb));

    *p_usage = usage;
    return PJ_SUCCESS;

on_error:
    pj_stun_usage_destroy(usage);
    return status;
}


/**
 * Destroy usage.
 */
PJ_DEF(pj_status_t) pj_stun_usage_destroy(pj_stun_usage *usage)
{
    pj_stun_server_unregister_usage(usage->srv, usage);
    if (usage->cb.on_destroy)
	(*usage->cb.on_destroy)(usage);

    if (usage->key) {
	pj_ioqueue_unregister(usage->key);
	usage->key = NULL;
	usage->sock = PJ_INVALID_SOCKET;
    } else if (usage->sock != 0 && usage->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(usage->sock);
	usage->sock = PJ_INVALID_SOCKET;
    }

    if (usage->mutex) {
	pj_mutex_destroy(usage->mutex);
	usage->mutex = NULL;
    }

    if (usage->pool) {
	pj_pool_t *pool = usage->pool;
	usage->pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}


/**
 * Set user data.
 */
PJ_DEF(pj_status_t) pj_stun_usage_set_user_data( pj_stun_usage *usage,
						 void *user_data)
{
    usage->user_data = user_data;
    return PJ_SUCCESS;
}

/**
 * Get user data.
 */
PJ_DEF(void*) pj_stun_usage_get_user_data(pj_stun_usage *usage)
{
    return usage->user_data;
}


/**
 * Send with the usage.
 */
PJ_DEF(pj_status_t) pj_stun_usage_sendto( pj_stun_usage *usage,
					  const void *pkt,
					  pj_size_t pkt_size,
					  unsigned flags,
					  const pj_sockaddr_t *dst_addr,
					  unsigned addr_len)
{
    pj_ssize_t size = pkt_size;
    unsigned i, count = usage->send_count, index;

    pj_mutex_lock(usage->mutex);
    for (i=0, ++usage->send_index; i<count; ++i, ++usage->send_index) {
	if (usage->send_index >= usage->send_count)
	    usage->send_index = 0;

	if (pj_ioqueue_is_pending(usage->key, &usage->send_key[usage->send_index])==0) {
	    break;
	}
    }

    if (i==count) {
	pj_mutex_unlock(usage->mutex);
	return PJ_EBUSY;
    }

    index = usage->send_index;
    pj_mutex_unlock(usage->mutex);

    return pj_ioqueue_sendto(usage->key, &usage->send_key[index], 
			     pkt, &size, flags,
			     dst_addr, addr_len);
}


static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    enum { MAX_LOOP = 10 };
    pj_stun_usage *usage = pj_ioqueue_get_user_data(key);
    struct worker *worker = (struct worker*) op_key;
    unsigned count;
    pj_status_t status;
    
    for (count=0; !usage->quitting; ++count) {
	unsigned flags;

	if (bytes_read > 0) {
	    (*usage->cb.on_rx_data)(usage, worker->readbuf, bytes_read,
				    &worker->src_addr, worker->src_addr_len);
	} else if (bytes_read < 0) {
	    pj_stun_perror(usage->pool->obj_name, "recv() error", -bytes_read);
	}

	if (usage->quitting)
	    break;

	bytes_read = sizeof(worker->readbuf);
	flags = (count >= MAX_LOOP) ? PJ_IOQUEUE_ALWAYS_ASYNC : 0;
	worker->src_addr_len = sizeof(worker->src_addr);
	status = pj_ioqueue_recvfrom(usage->key, &worker->read_key,
				     worker->readbuf, &bytes_read, flags,
				     &worker->src_addr, &worker->src_addr_len);
	if (status == PJ_EPENDING)
	    break;
    }
}


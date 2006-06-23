/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjsip/sip_transport_tcp.h>
#include <pjsip/sip_endpoint.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/lock.h>
#include <pj/pool.h>
#include <pj/string.h>


#define MAX_ASYNC_CNT	16
#define POOL_INIT	4000
#define POOL_INC	4000


struct tcp_listener;

struct pending_accept
{
    pj_ioqueue_op_key_t	 op_key;
    struct tcp_listener	*listener;
    pj_sock_t		 new_sock;
    int			 addr_len;
    pj_sockaddr_in	 local_addr;
    pj_sockaddr_in	 remote_addr;
};

struct tcp_listener
{
    pjsip_tpfactory	     factory;
    pj_bool_t		     active;
    pjsip_tpmgr		    *tpmgr;
    pj_sock_t		     sock;
    pj_ioqueue_key_t	    *key;
    unsigned		     async_cnt;
    struct pending_accept    accept_op[MAX_ASYNC_CNT];
};


struct tcp_transport
{
    pjsip_transport	     base;
    pj_sock_t		     sock;
};


/*
 * This callback is called when #pj_ioqueue_accept completes.
 */
static void on_accept_complete(	pj_ioqueue_key_t *key, 
				pj_ioqueue_op_key_t *op_key, 
				pj_sock_t sock, 
				pj_status_t status);

static pj_status_t destroy_listener(struct tcp_listener *listener);

static pj_status_t create_transport(pjsip_tpfactory *factory,
				    pjsip_tpmgr *mgr,
				    pjsip_endpoint *endpt,
				    const pj_sockaddr *rem_addr,
				    int addr_len,
				    pjsip_transport **transport);

PJ_DEF(pj_status_t) pjsip_tcp_transport_start( pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       unsigned async_cnt)
{
    pj_pool_t *pool;
    struct tcp_listener *listener;
    pj_ioqueue_callback listener_cb;
    unsigned i;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && local && async_cnt, PJ_EINVAL);


    pool = pjsip_endpt_create_pool(endpt, "tcplis", POOL_INIT, POOL_INC);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);


    listener = pj_pool_zalloc(pool, sizeof(struct tcp_listener));
    listener->factory.pool = pool;
    listener->factory.type = PJSIP_TRANSPORT_TCP;
    pj_ansi_strcpy(listener->factory.type_name, "tcp");
    listener->factory.flag = 
	pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_TCP);
    listener->sock = PJ_INVALID_SOCKET;

    status = pj_lock_create_recursive_mutex(pool, "tcplis", 
					    &listener->factory.lock);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create and bind socket */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, &listener->sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_memcpy(&listener->factory.local_addr, local, sizeof(pj_sockaddr_in));
    status = pj_sock_bind(listener->sock, local, sizeof(*local));
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register socket to ioqeuue */
    pj_memset(&listener_cb, 0, sizeof(listener_cb));
    listener_cb.on_accept_complete = &on_accept_complete;
    status = pj_ioqueue_register_sock(pool, pjsip_endpt_get_ioqueue(endpt),
				      listener->sock, listener,
				      &listener_cb, &listener->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Start pending accept() operation */
    if (async_cnt > MAX_ASYNC_CNT) async_cnt = MAX_ASYNC_CNT;
    listener->async_cnt = async_cnt;

    for (i=0; i<async_cnt; ++i) {
	pj_ioqueue_op_key_init(&listener->accept_op[i].op_key, 
				sizeof(listener->accept_op[i].op_key));
	listener->accept_op[i].listener = listener;

	status = pj_ioqueue_accept(listener->key, 
				   &listener->accept_op[i].op_key,
				   &listener->accept_op[i].new_sock,
				   &listener->accept_op[i].local_addr,
				   &listener->accept_op[i].remote_addr,
				   &listener->accept_op[i].addr_len);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Register to transport manager */
    listener->tpmgr = pjsip_endpt_get_tpmgr(endpt);
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
					    &listener->factory);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done! */
    listener->active = PJ_TRUE;

    return PJ_SUCCESS;

on_error:
    destroy_listener(listener);
    return status;
}




static pj_status_t destroy_listener(struct tcp_listener *listener)
{
    if (listener->active) {
	pjsip_tpmgr_unregister_tpfactory(listener->tpmgr, &listener->factory);
	listener->active = PJ_FALSE;
    }

    if (listener->key) {
	pj_ioqueue_unregister(listener->key);
	listener->key = NULL;
	listener->sock = PJ_INVALID_SOCKET;
    }

    if (listener->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(listener->sock);
	listener->sock = PJ_INVALID_SOCKET;
    }

    if (listener->factory.lock) {
	pj_lock_destroy(listener->factory.lock);
	listener->factory.lock = NULL;
    }

    if (listener->factory.pool) {
	pj_pool_release(listener->factory.pool);
	listener->factory.pool = NULL;
    }

    return PJ_SUCCESS;
}


static void on_accept_complete(	pj_ioqueue_key_t *key, 
				pj_ioqueue_op_key_t *op_key, 
				pj_sock_t sock, 
				pj_status_t status)
{
}


static pj_status_t create_transport(pjsip_tpfactory *factory,
				    pjsip_tpmgr *mgr,
				    pjsip_endpoint *endpt,
				    const pj_sockaddr *rem_addr,
				    int addr_len,
				    pjsip_transport **transport)
{
}


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
#include <pjsip/sip_errno.h>
#include <pj/compat/socket.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/ioqueue.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


#define THIS_FILE	"sip_transport_tcp.c"

#define MAX_ASYNC_CNT	16
#define POOL_LIS_INIT	4000
#define POOL_LIS_INC	4001
#define POOL_TP_INIT	4000
#define POOL_TP_INC	4002


struct tcp_listener;
struct tcp_transport;


/*
 * This structure is "descendant" of pj_ioqueue_op_key_t, and it is used to
 * track pending/asynchronous accept() operation. TCP transport may have
 * more than one pending accept() operations, depending on the value of
 * async_cnt.
 */
struct pending_accept
{
    pj_ioqueue_op_key_t	     op_key;
    struct tcp_listener	    *listener;
    unsigned		     index;
    pj_pool_t		    *pool;
    pj_sock_t		     new_sock;
    int			     addr_len;
    pj_sockaddr_in	     local_addr;
    pj_sockaddr_in	     remote_addr;
};


/*
 * This is the TCP listener, which is a "descendant" of pjsip_tpfactory (the
 * SIP transport factory).
 */
struct tcp_listener
{
    pjsip_tpfactory	     factory;
    pj_bool_t		     is_registered;
    pjsip_endpoint	    *endpt;
    pjsip_tpmgr		    *tpmgr;
    pj_sock_t		     sock;
    pj_ioqueue_key_t	    *key;
    unsigned		     async_cnt;
    struct pending_accept   *accept_op[MAX_ASYNC_CNT];
};


/*
 * This structure is used to keep delayed transmit operation in a list.
 * A delayed transmission occurs when application sends tx_data when
 * the TCP connect/establishment is still in progress. These delayed
 * transmission will be "flushed" once the socket is connected (either
 * successfully or with errors).
 */
struct delayed_tdata
{
    PJ_DECL_LIST_MEMBER(struct delayed_tdata);
    pjsip_tx_data_op_key    *tdata_op_key;
};


/*
 * This structure describes the TCP transport, and it's descendant of
 * pjsip_transport.
 */
struct tcp_transport
{
    pjsip_transport	     base;
    pj_bool_t		     is_server;
    struct tcp_listener	    *listener;
    pj_bool_t		     is_registered;
    pj_bool_t		     is_closing;
    pj_status_t		     close_reason;
    pj_sock_t		     sock;
    pj_ioqueue_key_t	    *key;
    pj_bool_t		     has_pending_connect;


    /* TCP transport can only have  one rdata!
     * Otherwise chunks of incoming PDU may be received on different
     * buffer.
     */
    pjsip_rx_data	     rdata;

    /* Pending transmission list. */
    struct delayed_tdata     delayed_list;
};


/****************************************************************************
 * PROTOTYPES
 */

/* This callback is called when pending accept() operation completes. */
static void on_accept_complete(	pj_ioqueue_key_t *key, 
				pj_ioqueue_op_key_t *op_key, 
				pj_sock_t sock, 
				pj_status_t status);

/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory);

/* This callback is called by transport manager to create transport */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
					pjsip_tpmgr *mgr,
					pjsip_endpoint *endpt,
					const pj_sockaddr *rem_addr,
					int addr_len,
					pjsip_transport **transport);

/* Common function to create and initialize transport */
static pj_status_t tcp_create(struct tcp_listener *listener,
			      pj_pool_t *pool,
			      pj_sock_t sock, pj_bool_t is_server,
			      const pj_sockaddr_in *local,
			      const pj_sockaddr_in *remote,
			      struct tcp_transport **p_tcp);


static void tcp_perror(const char *sender, const char *title,
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(1,(sender, "%s: %s [code=%d]", title, errmsg, status));
}


static void sockaddr_to_host_port( pj_pool_t *pool,
				   pjsip_host_port *host_port,
				   const pj_sockaddr_in *addr )
{
    enum { M = 48 };
    host_port->host.ptr = pj_pool_alloc(pool, M);
    host_port->host.slen = pj_ansi_snprintf( host_port->host.ptr, M, "%s", 
					    pj_inet_ntoa(addr->sin_addr));
    host_port->port = pj_ntohs(addr->sin_port);
}



/****************************************************************************
 * The TCP listener/transport factory.
 */

/*
 * This is the public API to create, initialize, register, and start the
 * TCP listener.
 */
PJ_DEF(pj_status_t) pjsip_tcp_transport_start2(pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       const pjsip_host_port *a_name,
					       unsigned async_cnt,
					       pjsip_tpfactory **p_factory)
{
    pj_pool_t *pool;
    struct tcp_listener *listener;
    pj_ioqueue_callback listener_cb;
    pj_sockaddr_in *listener_addr;
    int addr_len;
    unsigned i;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && async_cnt, PJ_EINVAL);

    /* Verify that address given in a_name (if any) is valid */
    if (a_name && a_name->host.slen) {
	pj_sockaddr_in tmp;

	status = pj_sockaddr_in_init(&tmp, &a_name->host, 
				     (pj_uint16_t)a_name->port);
	if (status != PJ_SUCCESS || tmp.sin_addr.s_addr == PJ_INADDR_ANY ||
	    tmp.sin_addr.s_addr == PJ_INADDR_NONE)
	{
	    /* Invalid address */
	    return PJ_EINVAL;
	}
    }

    pool = pjsip_endpt_create_pool(endpt, "tcplis", POOL_LIS_INIT, 
				   POOL_LIS_INC);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);


    listener = pj_pool_zalloc(pool, sizeof(struct tcp_listener));
    listener->factory.pool = pool;
    listener->factory.type = PJSIP_TRANSPORT_TCP;
    listener->factory.type_name = "tcp";
    listener->factory.flag = 
	pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_TCP);
    listener->sock = PJ_INVALID_SOCKET;

    pj_ansi_strcpy(listener->factory.obj_name, "tcplis");

    status = pj_lock_create_recursive_mutex(pool, "tcplis", 
					    &listener->factory.lock);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create and bind socket */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, &listener->sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    listener_addr = (pj_sockaddr_in*)&listener->factory.local_addr;
    if (local) {
	pj_memcpy(listener_addr, local, sizeof(pj_sockaddr_in));
    } else {
	pj_sockaddr_in_init(listener_addr, NULL, 0);
    }

    status = pj_sock_bind(listener->sock, listener_addr, 
			  sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Retrieve the bound address */
    addr_len = sizeof(pj_sockaddr_in);
    status = pj_sock_getsockname(listener->sock, listener_addr, &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* If published host/IP is specified, then use that address as the
     * listener advertised address.
     */
    if (a_name && a_name->host.slen) {
	/* Copy the address */
	listener->factory.addr_name = *a_name;
	pj_strdup(listener->factory.pool, &listener->factory.addr_name.host, 
		  &a_name->host);
	listener->factory.addr_name.port = a_name->port;

    } else {
	/* No published address is given, use the bound address */

	/* If the address returns 0.0.0.0, use the default
	 * interface address as the transport's address.
	 */
	if (listener_addr->sin_addr.s_addr == 0) {
	    pj_in_addr hostip;

	    status = pj_gethostip(&hostip);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    listener_addr->sin_addr = hostip;
	}

	/* Save the address name */
	sockaddr_to_host_port(listener->factory.pool, 
			      &listener->factory.addr_name, listener_addr);
    }

    /* If port is zero, get the bound port */
    if (listener->factory.addr_name.port == 0) {
	listener->factory.addr_name.port = pj_ntohs(listener_addr->sin_port);
    }

    pj_ansi_snprintf(listener->factory.obj_name, 
		     sizeof(listener->factory.obj_name),
		     "tcplis:%d",  listener->factory.addr_name.port);


    /* Start listening to the address */
    status = pj_sock_listen(listener->sock, PJSIP_TCP_TRANSPORT_BACKLOG);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Register socket to ioqeuue */
    pj_bzero(&listener_cb, sizeof(listener_cb));
    listener_cb.on_accept_complete = &on_accept_complete;
    status = pj_ioqueue_register_sock(pool, pjsip_endpt_get_ioqueue(endpt),
				      listener->sock, listener,
				      &listener_cb, &listener->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register to transport manager */
    listener->endpt = endpt;
    listener->tpmgr = pjsip_endpt_get_tpmgr(endpt);
    listener->factory.create_transport = lis_create_transport;
    listener->factory.destroy = lis_destroy;
    listener->is_registered = PJ_TRUE;
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
					    &listener->factory);
    if (status != PJ_SUCCESS) {
	listener->is_registered = PJ_FALSE;
	goto on_error;
    }


    /* Start pending accept() operations */
    if (async_cnt > MAX_ASYNC_CNT) async_cnt = MAX_ASYNC_CNT;
    listener->async_cnt = async_cnt;

    for (i=0; i<async_cnt; ++i) {
	pj_pool_t *pool;

	pool = pjsip_endpt_create_pool(endpt, "tcps%p", POOL_TP_INIT, 
				       POOL_TP_INIT);
	if (!pool) {
	    status = PJ_ENOMEM;
	    goto on_error;
	}

	listener->accept_op[i] = pj_pool_zalloc(pool, 
						sizeof(struct pending_accept));
	pj_ioqueue_op_key_init(&listener->accept_op[i]->op_key, 
				sizeof(listener->accept_op[i]->op_key));
	listener->accept_op[i]->pool = pool;
	listener->accept_op[i]->listener = listener;
	listener->accept_op[i]->index = i;

	on_accept_complete(listener->key, &listener->accept_op[i]->op_key,
			   listener->sock, PJ_EPENDING);
    }

    PJ_LOG(4,(listener->factory.obj_name, 
	     "SIP TCP listener ready for incoming connections at %.*s:%d",
	     (int)listener->factory.addr_name.host.slen,
	     listener->factory.addr_name.host.ptr,
	     listener->factory.addr_name.port));

    /* Return the pointer to user */
    if (p_factory) *p_factory = &listener->factory;

    return PJ_SUCCESS;

on_error:
    lis_destroy(&listener->factory);
    return status;
}


/*
 * This is the public API to create, initialize, register, and start the
 * TCP listener.
 */
PJ_DEF(pj_status_t) pjsip_tcp_transport_start( pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       unsigned async_cnt,
					       pjsip_tpfactory **p_factory)
{
    return pjsip_tcp_transport_start2(endpt, local, NULL, async_cnt, p_factory);
}


/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory)
{
    struct tcp_listener *listener = (struct tcp_listener *)factory;
    unsigned i;

    if (listener->is_registered) {
	pjsip_tpmgr_unregister_tpfactory(listener->tpmgr, &listener->factory);
	listener->is_registered = PJ_FALSE;
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

    for (i=0; i<PJ_ARRAY_SIZE(listener->accept_op); ++i) {
	if (listener->accept_op[i] && listener->accept_op[i]->pool) {
	    pj_pool_t *pool = listener->accept_op[i]->pool;
	    listener->accept_op[i]->pool = NULL;
	    pj_pool_release(pool);
	}
    }

    if (listener->factory.pool) {
	pj_pool_t *pool = listener->factory.pool;

	PJ_LOG(4,(listener->factory.obj_name,  "SIP TCP listener destroyed"));

	listener->factory.pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}


/***************************************************************************/
/*
 * TCP Transport
 */

/*
 * Prototypes.
 */
/* Called by transport manager to send message */
static pj_status_t tcp_send_msg(pjsip_transport *transport, 
				pjsip_tx_data *tdata,
				const pj_sockaddr_t *rem_addr,
				int addr_len,
				void *token,
				void (*callback)(pjsip_transport *transport,
						 void *token, 
						 pj_ssize_t sent_bytes));

/* Called by transport manager to shutdown */
static pj_status_t tcp_shutdown(pjsip_transport *transport);

/* Called by transport manager to destroy transport */
static pj_status_t tcp_destroy_transport(pjsip_transport *transport);

/* Utility to destroy transport */
static pj_status_t tcp_destroy(pjsip_transport *transport,
			       pj_status_t reason);

/* Callback from ioqueue on incoming packet */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);

/* Callback from ioqueue when packet is sent */
static void on_write_complete(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key, 
                              pj_ssize_t bytes_sent);

/* Callback from ioqueue when connect completes */
static void on_connect_complete(pj_ioqueue_key_t *key, 
                                pj_status_t status);


/*
 * Common function to create TCP transport, called when pending accept() and
 * pending connect() complete.
 */
static pj_status_t tcp_create( struct tcp_listener *listener,
			       pj_pool_t *pool,
			       pj_sock_t sock, pj_bool_t is_server,
			       const pj_sockaddr_in *local,
			       const pj_sockaddr_in *remote,
			       struct tcp_transport **p_tcp)
{
    struct tcp_transport *tcp;
    pj_ioqueue_t *ioqueue;
    pj_ioqueue_callback tcp_callback;
    pj_status_t status;
    

    PJ_ASSERT_RETURN(sock != PJ_INVALID_SOCKET, PJ_EINVAL);


    if (pool == NULL) {
	pool = pjsip_endpt_create_pool(listener->endpt, "tcp",
				       POOL_TP_INIT, POOL_TP_INC);
	PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);
    }    

    /*
     * Create and initialize basic transport structure.
     */
    tcp = pj_pool_zalloc(pool, sizeof(*tcp));
    tcp->sock = sock;
    tcp->is_server = is_server;
    tcp->listener = listener;
    pj_list_init(&tcp->delayed_list);
    tcp->base.pool = pool;

    pj_ansi_snprintf(tcp->base.obj_name, PJ_MAX_OBJ_NAME, 
		     (is_server ? "tcps%p" :"tcpc%p"), tcp);

    status = pj_atomic_create(pool, 0, &tcp->base.ref_cnt);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    status = pj_lock_create_recursive_mutex(pool, "tcp", &tcp->base.lock);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    tcp->base.key.type = PJSIP_TRANSPORT_TCP;
    pj_memcpy(&tcp->base.key.rem_addr, remote, sizeof(pj_sockaddr_in));
    tcp->base.type_name = "tcp";
    tcp->base.flag = pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_TCP);

    tcp->base.info = pj_pool_alloc(pool, 64);
    pj_ansi_snprintf(tcp->base.info, 64, "TCP to %s:%d",
		     pj_inet_ntoa(remote->sin_addr), 
		     (int)pj_ntohs(remote->sin_port));

    tcp->base.addr_len = sizeof(pj_sockaddr_in);
    pj_memcpy(&tcp->base.local_addr, local, sizeof(pj_sockaddr_in));
    sockaddr_to_host_port(pool, &tcp->base.local_name, local);
    sockaddr_to_host_port(pool, &tcp->base.remote_name, remote);

    tcp->base.endpt = listener->endpt;
    tcp->base.tpmgr = listener->tpmgr;
    tcp->base.send_msg = &tcp_send_msg;
    tcp->base.do_shutdown = &tcp_shutdown;
    tcp->base.destroy = &tcp_destroy_transport;


    /* Register socket to ioqueue */
    pj_bzero(&tcp_callback, sizeof(pj_ioqueue_callback));
    tcp_callback.on_read_complete = &on_read_complete;
    tcp_callback.on_write_complete = &on_write_complete;
    tcp_callback.on_connect_complete = &on_connect_complete;

    ioqueue = pjsip_endpt_get_ioqueue(listener->endpt);
    status = pj_ioqueue_register_sock(pool, ioqueue, sock, 
				      tcp, &tcp_callback, &tcp->key);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Register transport to transport manager */
    status = pjsip_transport_register(listener->tpmgr, &tcp->base);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    tcp->is_registered = PJ_TRUE;

    /* Done setting up basic transport. */
    *p_tcp = tcp;

    PJ_LOG(4,(tcp->base.obj_name, "TCP %s transport created",
	      (tcp->is_server ? "server" : "client")));

    return PJ_SUCCESS;

on_error:
    tcp_destroy(&tcp->base, status);
    return status;
}


/* Flush all delayed transmision once the socket is connected. */
static void tcp_flush_pending_tx(struct tcp_transport *tcp)
{
    pj_lock_acquire(tcp->base.lock);
    while (!pj_list_empty(&tcp->delayed_list)) {
	struct delayed_tdata *pending_tx;
	pjsip_tx_data *tdata;
	pj_ioqueue_op_key_t *op_key;
	pj_ssize_t size;
	pj_status_t status;

	pending_tx = tcp->delayed_list.next;
	pj_list_erase(pending_tx);

	tdata = pending_tx->tdata_op_key->tdata;
	op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	/* send to ioqueue! */
	size = tdata->buf.cur - tdata->buf.start;
	status = pj_ioqueue_send(tcp->key, op_key,
				 tdata->buf.start, &size, 0);

	if (status != PJ_EPENDING) {
	    on_write_complete(tcp->key, op_key, size);
	}

    }
    pj_lock_release(tcp->base.lock);
}


/* Called by transport manager to destroy transport */
static pj_status_t tcp_destroy_transport(pjsip_transport *transport)
{
    struct tcp_transport *tcp = (struct tcp_transport*)transport;

    /* Transport would have been unregistered by now since this callback
     * is called by transport manager.
     */
    tcp->is_registered = PJ_FALSE;

    return tcp_destroy(transport, tcp->close_reason);
}


/* Destroy TCP transport */
static pj_status_t tcp_destroy(pjsip_transport *transport, 
			       pj_status_t reason)
{
    struct tcp_transport *tcp = (struct tcp_transport*)transport;

    if (tcp->close_reason == 0)
	tcp->close_reason = reason;

    if (tcp->is_registered) {
	tcp->is_registered = PJ_FALSE;
	pjsip_transport_destroy(transport);

	/* pjsip_transport_destroy will recursively call this function
	 * again.
	 */
	return PJ_SUCCESS;
    }

    /* Mark transport as closing */
    tcp->is_closing = PJ_TRUE;

    /* Cancel all delayed transmits */
    while (!pj_list_empty(&tcp->delayed_list)) {
	struct delayed_tdata *pending_tx;
	pj_ioqueue_op_key_t *op_key;

	pending_tx = tcp->delayed_list.next;
	pj_list_erase(pending_tx);

	op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	on_write_complete(tcp->key, op_key, -reason);
    }

    if (tcp->rdata.tp_info.pool) {
	pj_pool_release(tcp->rdata.tp_info.pool);
	tcp->rdata.tp_info.pool = NULL;
    }

    if (tcp->key) {
	pj_ioqueue_unregister(tcp->key);
	tcp->key = NULL;
	tcp->sock = PJ_INVALID_SOCKET;
    }

    if (tcp->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(tcp->sock);
	tcp->sock = PJ_INVALID_SOCKET;
    }

    if (tcp->base.lock) {
	pj_lock_destroy(tcp->base.lock);
	tcp->base.lock = NULL;
    }

    if (tcp->base.ref_cnt) {
	pj_atomic_destroy(tcp->base.ref_cnt);
	tcp->base.ref_cnt = NULL;
    }

    if (tcp->base.pool) {
	pj_pool_t *pool;

	if (reason != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(reason, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(tcp->base.obj_name, 
		      "TCP transport destroyed with reason %d: %s", 
		      reason, errmsg));

	} else {

	    PJ_LOG(4,(tcp->base.obj_name, 
		      "TCP transport destroyed normally"));

	}

	pool = tcp->base.pool;
	tcp->base.pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}


/*
 * This utility function creates receive data buffers and start
 * asynchronous recv() operations from the socket. It is called after
 * accept() or connect() operation complete.
 */
static pj_status_t tcp_start_read(struct tcp_transport *tcp)
{
    pj_pool_t *pool;
    pj_ssize_t size;
    pj_sockaddr_in *rem_addr;
    pj_status_t status;

    /* Init rdata */
    pool = pjsip_endpt_create_pool(tcp->listener->endpt,
				   "rtd%p",
				   PJSIP_POOL_RDATA_LEN,
				   PJSIP_POOL_RDATA_INC);
    if (!pool) {
	tcp_perror(tcp->base.obj_name, "Unable to create pool", PJ_ENOMEM);
	return PJ_ENOMEM;
    }

    tcp->rdata.tp_info.pool = pool;

    tcp->rdata.tp_info.transport = &tcp->base;
    tcp->rdata.tp_info.tp_data = tcp;
    tcp->rdata.tp_info.op_key.rdata = &tcp->rdata;
    pj_ioqueue_op_key_init(&tcp->rdata.tp_info.op_key.op_key, 
			   sizeof(pj_ioqueue_op_key_t));

    tcp->rdata.pkt_info.src_addr = tcp->base.key.rem_addr;
    tcp->rdata.pkt_info.src_addr_len = sizeof(pj_sockaddr_in);
    rem_addr = (pj_sockaddr_in*) &tcp->base.key.rem_addr;
    pj_ansi_strcpy(tcp->rdata.pkt_info.src_name,
		   pj_inet_ntoa(rem_addr->sin_addr));
    tcp->rdata.pkt_info.src_port = pj_ntohs(rem_addr->sin_port);

    size = sizeof(tcp->rdata.pkt_info.packet);
    status = pj_ioqueue_recv(tcp->key, &tcp->rdata.tp_info.op_key.op_key,
			     tcp->rdata.pkt_info.packet, &size,
			     PJ_IOQUEUE_ALWAYS_ASYNC);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	PJ_LOG(4, (tcp->base.obj_name, "ioqueue recv() error, status=%d", 
		   status));
	return status;
    }

    return PJ_SUCCESS;
}


/* This callback is called by transport manager for the TCP factory
 * to create outgoing transport to the specified destination.
 */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
					pjsip_tpmgr *mgr,
					pjsip_endpoint *endpt,
					const pj_sockaddr *rem_addr,
					int addr_len,
					pjsip_transport **p_transport)
{
    struct tcp_listener *listener;
    struct tcp_transport *tcp;
    pj_sock_t sock;
    pj_sockaddr_in local_addr;
    pj_status_t status;

    /* Sanity checks */
    PJ_ASSERT_RETURN(factory && mgr && endpt && rem_addr &&
		     addr_len && p_transport, PJ_EINVAL);

    /* Check that address is a sockaddr_in */
    PJ_ASSERT_RETURN(rem_addr->sa_family == PJ_AF_INET &&
		     addr_len == sizeof(pj_sockaddr_in), PJ_EINVAL);


    listener = (struct tcp_listener*)factory;

    
    /* Create socket */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Bind to any port */
    status = pj_sock_bind_in(sock, 0, 0);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    /* Get the local port */
    addr_len = sizeof(pj_sockaddr_in);
    status = pj_sock_getsockname(sock, &local_addr, &addr_len);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    /* Initially set the address from the listener's address */
    local_addr.sin_addr.s_addr = 
	((pj_sockaddr_in*)&listener->factory.local_addr)->sin_addr.s_addr;

    /* Create the transport descriptor */
    status = tcp_create(listener, NULL, sock, PJ_FALSE, &local_addr, 
			(pj_sockaddr_in*)rem_addr, &tcp);
    if (status != PJ_SUCCESS)
	return status;


    /* Start asynchronous connect() operation */
    tcp->has_pending_connect = PJ_TRUE;
    status = pj_ioqueue_connect(tcp->key, rem_addr, sizeof(pj_sockaddr_in));
    if (status == PJ_SUCCESS) {
	tcp->has_pending_connect = PJ_FALSE;
    } else if (status != PJ_EPENDING) {
	tcp_destroy(&tcp->base, status);
	return status;
    }

    /* Update (again) local address, just in case local address currently
     * set is different now that asynchronous connect() is started.
     */
    addr_len = sizeof(pj_sockaddr_in);
    if (pj_sock_getsockname(tcp->sock, &local_addr, &addr_len)==PJ_SUCCESS) {
	pj_sockaddr_in *tp_addr = (pj_sockaddr_in*)&tcp->base.local_addr;

	/* Some systems (like old Win32 perhaps) may not set local address
	 * properly before socket is fully connected.
	 */
	if (tp_addr->sin_addr.s_addr != local_addr.sin_addr.s_addr &&
	    local_addr.sin_addr.s_addr != 0) 
	{
	    tp_addr->sin_addr.s_addr = local_addr.sin_addr.s_addr;
	    tp_addr->sin_port = local_addr.sin_port;
	    sockaddr_to_host_port(tcp->base.pool, &tcp->base.local_name,
				  &local_addr);
	}
    }

    if (tcp->has_pending_connect) {
	PJ_LOG(4,(tcp->base.obj_name, 
		  "TCP transport %.*s:%d is connecting to %.*s:%d...",
		  (int)tcp->base.local_name.host.slen,
		  tcp->base.local_name.host.ptr,
		  tcp->base.local_name.port,
		  (int)tcp->base.remote_name.host.slen,
		  tcp->base.remote_name.host.ptr,
		  tcp->base.remote_name.port));
    }

    /* Done */
    *p_transport = &tcp->base;

    return PJ_SUCCESS;
}


/*
 * This callback is called by ioqueue when pending accept() operation has
 * completed.
 */
static void on_accept_complete(	pj_ioqueue_key_t *key, 
				pj_ioqueue_op_key_t *op_key, 
				pj_sock_t sock, 
				pj_status_t status)
{
    struct tcp_listener *listener;
    struct tcp_transport *tcp;
    struct pending_accept *accept_op;
    int err_cnt = 0;

    listener = pj_ioqueue_get_user_data(key);
    accept_op = (struct pending_accept*) op_key;

    /*
     * Loop while there is immediate connection or when there is error.
     */
    do {
	if (status == PJ_EPENDING) {
	    /*
	     * This can only happen when this function is called during
	     * initialization to kick off asynchronous accept().
	     */

	} else if (status != PJ_SUCCESS) {

	    /*
	     * Error in accept().
	     */
	    tcp_perror(listener->factory.obj_name, "Error in accept()", 
		       status);

	    /*
	     * Prevent endless accept() error loop by limiting the
	     * number of consecutive errors. Once the number of errors
	     * is equal to maximum, we treat this as permanent error, and
	     * we stop the accept() operation.
	     */
	    ++err_cnt;
	    if (err_cnt >= 10) {
		PJ_LOG(1, (listener->factory.obj_name, 
			   "Too many errors, listener stopping"));
	    }

	} else {
	    pj_pool_t *pool;
	    struct pending_accept *new_op;

	    if (sock == PJ_INVALID_SOCKET) {
		sock = accept_op->new_sock;
	    }

	    if (sock == PJ_INVALID_SOCKET) {
		pj_assert(!"Should not happen. status should be error");
		goto next_accept;
	    }

	    PJ_LOG(4,(listener->factory.obj_name, 
		      "TCP listener %.*s:%d: got incoming TCP connection "
		      "from %s:%d, sock=%d",
		      (int)listener->factory.addr_name.host.slen,
		      listener->factory.addr_name.host.ptr,
		      listener->factory.addr_name.port,
		      pj_inet_ntoa(accept_op->remote_addr.sin_addr),
		      pj_ntohs(accept_op->remote_addr.sin_port),
		      sock));

	    /* Create new accept_opt */
	    pool = pjsip_endpt_create_pool(listener->endpt, "tcps%p", 
					   POOL_TP_INIT, POOL_TP_INC);
	    new_op = pj_pool_zalloc(pool, sizeof(struct pending_accept));
	    new_op->pool = pool;
	    new_op->listener = listener;
	    new_op->index = accept_op->index;
	    pj_ioqueue_op_key_init(&new_op->op_key, sizeof(new_op->op_key));
	    listener->accept_op[accept_op->index] = new_op;

	    /* 
	     * Incoming connections!
	     * Create TCP transport for the new socket.
	     */
	    status = tcp_create( listener, accept_op->pool, sock, PJ_TRUE,
				 &accept_op->local_addr, 
				 &accept_op->remote_addr, &tcp);
	    if (status == PJ_SUCCESS) {
		status = tcp_start_read(tcp);
		if (status != PJ_SUCCESS) {
		    PJ_LOG(3,(tcp->base.obj_name, "New transport cancelled"));
		    tcp_destroy(&tcp->base, status);
		}
	    }

	    accept_op = new_op;
	}

next_accept:
	/*
	 * Start the next asynchronous accept() operation.
	 */
	accept_op->addr_len = sizeof(pj_sockaddr_in);
	accept_op->new_sock = PJ_INVALID_SOCKET;

	status = pj_ioqueue_accept(listener->key, 
				   &accept_op->op_key,
				   &accept_op->new_sock,
				   &accept_op->local_addr,
				   &accept_op->remote_addr,
				   &accept_op->addr_len);

	/*
	 * Loop while we have immediate connection or when there is error.
	 */

    } while (status != PJ_EPENDING);
}


/* 
 * Callback from ioqueue when packet is sent.
 */
static void on_write_complete(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key, 
                              pj_ssize_t bytes_sent)
{
    struct tcp_transport *tcp = pj_ioqueue_get_user_data(key);
    pjsip_tx_data_op_key *tdata_op_key = (pjsip_tx_data_op_key*)op_key;

    tdata_op_key->tdata = NULL;

    /* Check for error/closure */
    if (bytes_sent <= 0) {
	pj_status_t status;

	PJ_LOG(5,(tcp->base.obj_name, "TCP send() error, sent=%d", 
		  bytes_sent));

	status = (bytes_sent == 0) ? PJ_RETURN_OS_ERROR(OSERR_ENOTCONN) :
				     -bytes_sent;
	if (tcp->close_reason==PJ_SUCCESS) tcp->close_reason = status;
	pjsip_transport_shutdown(&tcp->base);
    }

    if (tdata_op_key->callback) {
	/*
	 * Notify sip_transport.c that packet has been sent.
	 */
	tdata_op_key->callback(&tcp->base, tdata_op_key->token, bytes_sent);
    }
}


/* 
 * This callback is called by transport manager to send SIP message 
 */
static pj_status_t tcp_send_msg(pjsip_transport *transport, 
				pjsip_tx_data *tdata,
				const pj_sockaddr_t *rem_addr,
				int addr_len,
				void *token,
				void (*callback)(pjsip_transport *transport,
						 void *token, 
						 pj_ssize_t sent_bytes))
{
    struct tcp_transport *tcp = (struct tcp_transport*)transport;
    pj_ssize_t size;
    pj_bool_t delayed = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;

    /* Sanity check */
    PJ_ASSERT_RETURN(transport && tdata, PJ_EINVAL);

    /* Check that there's no pending operation associated with the tdata */
    PJ_ASSERT_RETURN(tdata->op_key.tdata == NULL, PJSIP_EPENDINGTX);
    
    /* Check the address is supported */
    PJ_ASSERT_RETURN(rem_addr && addr_len==sizeof(pj_sockaddr_in), PJ_EINVAL);



    /* Init op key. */
    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;

    /* If asynchronous connect() has not completed yet, just put the
     * transmit data in the pending transmission list since we can not
     * use the socket yet.
     */
    if (tcp->has_pending_connect) {

	/*
	 * Looks like connect() is still in progress. Check again (this time
	 * with holding the lock) to be sure.
	 */
	pj_lock_acquire(tcp->base.lock);

	if (tcp->has_pending_connect) {
	    struct delayed_tdata *delayed_tdata;

	    /*
	     * connect() is still in progress. Put the transmit data to
	     * the delayed list.
	     */
	    delayed_tdata = pj_pool_alloc(tdata->pool, 
					  sizeof(*delayed_tdata));
	    delayed_tdata->tdata_op_key = &tdata->op_key;

	    pj_list_push_back(&tcp->delayed_list, delayed_tdata);
	    status = PJ_EPENDING;

	    /* Prevent pj_ioqueue_send() to be called below */
	    delayed = PJ_TRUE;
	}

	pj_lock_release(tcp->base.lock);
    } 
    
    if (!delayed) {
	/*
	 * Transport is ready to go. Send the packet to ioqueue to be
	 * sent asynchronously.
	 */
	size = tdata->buf.cur - tdata->buf.start;
	status = pj_ioqueue_send(tcp->key, 
				 (pj_ioqueue_op_key_t*)&tdata->op_key,
				 tdata->buf.start, &size, 0);

	if (status != PJ_EPENDING) {
	    /* Not pending (could be immediate success or error) */
	    tdata->op_key.tdata = NULL;

	    /* Shutdown transport on closure/errors */
	    if (size <= 0) {

		PJ_LOG(5,(tcp->base.obj_name, "TCP send() error, sent=%d", 
			  size));

		if (status == PJ_SUCCESS) 
		    status = PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);
		if (tcp->close_reason==PJ_SUCCESS) tcp->close_reason = status;
		pjsip_transport_shutdown(&tcp->base);
	    }
	}
    }

    return status;
}


/* 
 * This callback is called by transport manager to shutdown transport.
 * This normally is only used by UDP transport.
 */
static pj_status_t tcp_shutdown(pjsip_transport *transport)
{

    PJ_UNUSED_ARG(transport);

    /* Nothing to do for TCP */
    return PJ_SUCCESS;
}


/* 
 * Callback from ioqueue that an incoming data is received from the socket.
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    enum { MAX_IMMEDIATE_PACKET = 10 };
    pjsip_rx_data_op_key *rdata_op_key = (pjsip_rx_data_op_key*) op_key;
    pjsip_rx_data *rdata = rdata_op_key->rdata;
    struct tcp_transport *tcp = 
	(struct tcp_transport*)rdata->tp_info.transport;
    int i;
    pj_status_t status;

    /* Don't do anything if transport is closing. */
    if (tcp->is_closing) {
	tcp->is_closing++;
	return;
    }

    /*
     * The idea of the loop is to process immediate data received by
     * pj_ioqueue_recv(), as long as i < MAX_IMMEDIATE_PACKET. When
     * i is >= MAX_IMMEDIATE_PACKET, we force the recv() operation to
     * complete asynchronously, to allow other sockets to get their data.
     */
    for (i=0;; ++i) {
	pj_uint32_t flags;

	/* Houston, we have packet! Report the packet to transport manager
	 * to be parsed.
	 */
	if (bytes_read > 0) {
	    pj_size_t size_eaten;

	    /* Init pkt_info part. */
	    rdata->pkt_info.len += bytes_read;
	    rdata->pkt_info.zero = 0;
	    pj_gettimeofday(&rdata->pkt_info.timestamp);

	    /* Report to transport manager.
	     * The transport manager will tell us how many bytes of the packet
	     * have been processed (as valid SIP message).
	     */
	    size_eaten = 
		pjsip_tpmgr_receive_packet(rdata->tp_info.transport->tpmgr, 
					   rdata);

	    pj_assert(size_eaten <= (pj_size_t)rdata->pkt_info.len);

	    /* Move unprocessed data to the front of the buffer */
	    if (size_eaten>0 && size_eaten<(pj_size_t)rdata->pkt_info.len) {
		pj_memmove(rdata->pkt_info.packet,
			   rdata->pkt_info.packet + size_eaten,
			   rdata->pkt_info.len - size_eaten);
	    }
	    
	    rdata->pkt_info.len -= size_eaten;

	} else if (bytes_read == 0) {

	    /* Transport is closed */
	    PJ_LOG(4,(tcp->base.obj_name, "TCP connection closed"));
	    
	    /* We can not destroy the transport since high level objects may
	     * still keep reference to this transport. So we can only 
	     * instruct transport manager to gracefully start the shutdown
	     * procedure for this transport.
	     */
	    if (tcp->close_reason==PJ_SUCCESS) 
		tcp->close_reason = PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);
	    pjsip_transport_shutdown(&tcp->base);

	    return;

	//} else if (bytes_read < 0)  {
	} else if (-bytes_read != PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK) &&
		   -bytes_read != PJ_STATUS_FROM_OS(OSERR_EINPROGRESS) && 
		   -bytes_read != PJ_STATUS_FROM_OS(OSERR_ECONNRESET)) 
	{

	    /* Socket error. */
	    PJ_LOG(4,(tcp->base.obj_name, "TCP connection reset"));

	    /* We can not destroy the transport since high level objects may
	     * still keep reference to this transport. So we can only 
	     * instruct transport manager to gracefully start the shutdown
	     * procedure for this transport.
	     */
	    if (tcp->close_reason==PJ_SUCCESS) tcp->close_reason = -bytes_read;
	    pjsip_transport_shutdown(&tcp->base);

	    return;
	}

	if (i >= MAX_IMMEDIATE_PACKET) {
	    /* Receive quota reached. Force ioqueue_recv() to 
	     * return PJ_EPENDING 
	     */
	    flags = PJ_IOQUEUE_ALWAYS_ASYNC;
	} else {
	    flags = 0;
	}

	/* Reset pool. */
	pj_pool_reset(rdata->tp_info.pool);

	/* Read next packet. */
	bytes_read = sizeof(rdata->pkt_info.packet) - rdata->pkt_info.len;
	rdata->pkt_info.src_addr_len = sizeof(rdata->pkt_info.src_addr);
	status = pj_ioqueue_recv(key, op_key, 
				 rdata->pkt_info.packet+rdata->pkt_info.len,
				 &bytes_read, flags);

	if (status == PJ_SUCCESS) {

	    /* Continue loop. */
	    pj_assert(i < MAX_IMMEDIATE_PACKET);

	} else if (status == PJ_EPENDING) {
	    break;

	} else {
	    /* Socket error */
	    PJ_LOG(4,(tcp->base.obj_name, "TCP connection reset"));

	    /* We can not destroy the transport since high level objects may
	     * still keep reference to this transport. So we can only 
	     * instruct transport manager to gracefully start the shutdown
	     * procedure for this transport.
	     */
	    if (tcp->close_reason==PJ_SUCCESS) tcp->close_reason = status;
	    pjsip_transport_shutdown(&tcp->base);

	    return;
	}
    }
}


/* 
 * Callback from ioqueue when asynchronous connect() operation completes.
 */
static void on_connect_complete(pj_ioqueue_key_t *key, 
                                pj_status_t status)
{
    struct tcp_transport *tcp;
    pj_sockaddr_in addr;
    int addrlen;

    tcp = pj_ioqueue_get_user_data(key);

    /* Mark that pending connect() operation has completed. */
    tcp->has_pending_connect = PJ_FALSE;

    /* Check connect() status */
    if (status != PJ_SUCCESS) {

	tcp_perror(tcp->base.obj_name, "TCP connect() error", status);

	/* Cancel all delayed transmits */
	while (!pj_list_empty(&tcp->delayed_list)) {
	    struct delayed_tdata *pending_tx;
	    pj_ioqueue_op_key_t *op_key;

	    pending_tx = tcp->delayed_list.next;
	    pj_list_erase(pending_tx);

	    op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	    on_write_complete(tcp->key, op_key, -status);
	}

	/* We can not destroy the transport since high level objects may
	 * still keep reference to this transport. So we can only 
	 * instruct transport manager to gracefully start the shutdown
	 * procedure for this transport.
	 */
	if (tcp->close_reason==PJ_SUCCESS) tcp->close_reason = status;
	pjsip_transport_shutdown(&tcp->base);
	return;
    }

    PJ_LOG(4,(tcp->base.obj_name, 
	      "TCP transport %.*s:%d is connected to %.*s:%d",
	      (int)tcp->base.local_name.host.slen,
	      tcp->base.local_name.host.ptr,
	      tcp->base.local_name.port,
	      (int)tcp->base.remote_name.host.slen,
	      tcp->base.remote_name.host.ptr,
	      tcp->base.remote_name.port));


    /* Update (again) local address, just in case local address currently
     * set is different now that the socket is connected (could happen
     * on some systems, like old Win32 probably?).
     */
    addrlen = sizeof(pj_sockaddr_in);
    if (pj_sock_getsockname(tcp->sock, &addr, &addrlen)==PJ_SUCCESS) {
	pj_sockaddr_in *tp_addr = (pj_sockaddr_in*)&tcp->base.local_addr;

	if (tp_addr->sin_addr.s_addr != addr.sin_addr.s_addr) {
	    tp_addr->sin_addr.s_addr = addr.sin_addr.s_addr;
	    tp_addr->sin_port = addr.sin_port;
	    sockaddr_to_host_port(tcp->base.pool, &tcp->base.local_name,
				  tp_addr);
	}
    }

    /* Start pending read */
    status = tcp_start_read(tcp);
    if (status != PJ_SUCCESS) {
	/* We can not destroy the transport since high level objects may
	 * still keep reference to this transport. So we can only 
	 * instruct transport manager to gracefully start the shutdown
	 * procedure for this transport.
	 */
	if (tcp->close_reason==PJ_SUCCESS) tcp->close_reason = status;
	pjsip_transport_shutdown(&tcp->base);
	return;
    }

    /* Flush all pending send operations */
    tcp_flush_pending_tx(tcp);
}


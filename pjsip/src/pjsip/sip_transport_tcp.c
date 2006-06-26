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
#define POOL_LIS_INC	4000
#define POOL_TP_INIT	4000
#define POOL_TP_INC	4000


struct tcp_listener;
struct tcp_transport;


struct pending_accept
{
    pj_ioqueue_op_key_t	     op_key;
    struct tcp_listener	    *listener;
    pj_sock_t		     new_sock;
    int			     addr_len;
    pj_sockaddr_in	     local_addr;
    pj_sockaddr_in	     remote_addr;
};

struct pending_connect
{
    pj_ioqueue_op_key_t	     op_key;
    struct tcp_transport    *transport;
};


struct tcp_listener
{
    pjsip_tpfactory	     factory;
    char		     name[PJ_MAX_OBJ_NAME];
    pj_bool_t		     active;
    pjsip_endpoint	    *endpt;
    pjsip_tpmgr		    *tpmgr;
    pj_sock_t		     sock;
    pj_ioqueue_key_t	    *key;
    unsigned		     async_cnt;
    struct pending_accept    accept_op[MAX_ASYNC_CNT];
};


struct pending_tdata
{
    PJ_DECL_LIST_MEMBER(struct pending_tdata);
    pjsip_tx_data_op_key    *tdata_op_key;
};


struct tcp_transport
{
    pjsip_transport	     base;
    struct tcp_listener	    *listener;
    pj_bool_t		     is_registered;
    pj_bool_t		     is_closing;
    pj_sock_t		     sock;
    pj_ioqueue_key_t	    *key;
    pj_bool_t		     has_pending_connect;
    struct pending_connect   connect_op;


    /* TCP transport can only have  one rdata!
     * Otherwise chunks of incoming PDU may be received on different
     * buffer.
     */
    pjsip_rx_data	     rdata;

    /* Pending transmission list. */
    struct pending_tdata     tx_list;
};


/*
 * This callback is called when #pj_ioqueue_accept completes.
 */
static void on_accept_complete(	pj_ioqueue_key_t *key, 
				pj_ioqueue_op_key_t *op_key, 
				pj_sock_t sock, 
				pj_status_t status);

static pj_status_t lis_destroy(struct tcp_listener *listener);
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
					pjsip_tpmgr *mgr,
					pjsip_endpoint *endpt,
					const pj_sockaddr *rem_addr,
					int addr_len,
					pjsip_transport **transport);


static pj_status_t create_tcp_transport(struct tcp_listener *listener,
					pj_sock_t sock,
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


    pool = pjsip_endpt_create_pool(endpt, "tcplis", POOL_LIS_INIT, 
				   POOL_LIS_INC);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);


    listener = pj_pool_zalloc(pool, sizeof(struct tcp_listener));
    pj_ansi_sprintf(listener->name, "tcp:%d", (int)pj_ntohs(local->sin_port));
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
	if (status != PJ_SUCCESS && status != PJ_EPENDING)
	    goto on_error;
    }

    /* Register to transport manager */
    listener->endpt = endpt;
    listener->tpmgr = pjsip_endpt_get_tpmgr(endpt);
    listener->factory.create_transport = lis_create_transport;
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
					    &listener->factory);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done! */
    listener->active = PJ_TRUE;

    PJ_LOG(4,(listener->name, 
	     "SIP TCP transport listening for incoming connections at %s:%d",
	     pj_inet_ntoa(local->sin_addr), (int)pj_ntohs(local->sin_port)));

    return PJ_SUCCESS;

on_error:
    lis_destroy(listener);
    return status;
}


static pj_status_t lis_destroy(struct tcp_listener *listener)
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
	PJ_LOG(4,(listener->name,  "SIP TCP transport destroyed"));
	pj_pool_release(listener->factory.pool);
	listener->factory.pool = NULL;
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

/* Called by transport manager to destroy */
static pj_status_t tcp_destroy(pjsip_transport *transport);

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


static void sockaddr_to_host_port( pj_pool_t *pool,
				   pjsip_host_port *host_port,
				   const pj_sockaddr_in *addr )
{
    host_port->host.ptr = pj_pool_alloc(pool, 48);
    host_port->host.slen = pj_ansi_sprintf( host_port->host.ptr, "%s", 
					    pj_inet_ntoa(addr->sin_addr));
    host_port->port = pj_ntohs(addr->sin_port);
}


/*
 * Utilities to create TCP transport.
 */
static pj_status_t create_tcp_transport(struct tcp_listener *listener,
					pj_sock_t sock,
					const pj_sockaddr_in *local,
					const pj_sockaddr_in *remote,
					struct tcp_transport **p_tcp)
{
    struct tcp_transport *tcp;
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_ioqueue_callback tcp_callback;
    pj_status_t status;
    
    pool = pjsip_endpt_create_pool(listener->endpt, "tcp", 
				   POOL_TP_INIT, POOL_TP_INC);
    
    /*
     * Create and initialize basic transport structure.
     */
    tcp = pj_pool_zalloc(pool, sizeof(*tcp));
    tcp->sock = sock;
    tcp->listener = listener;
    pj_list_init(&tcp->tx_list);


    pj_ansi_snprintf(tcp->base.obj_name, PJ_MAX_OBJ_NAME, "tcp%p", tcp);
    tcp->base.pool = pool;

    status = pj_atomic_create(pool, 0, &tcp->base.ref_cnt);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_lock_create_recursive_mutex(pool, "tcp", &tcp->base.lock);
    if (status != PJ_SUCCESS)
	goto on_error;

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
    tcp->base.destroy = &tcp_destroy;


    /* Register socket to ioqueue */
    pj_memset(&tcp_callback, 0, sizeof(pj_ioqueue_callback));
    tcp_callback.on_read_complete = &on_read_complete;
    tcp_callback.on_write_complete = &on_write_complete;
    tcp_callback.on_connect_complete = &on_connect_complete;

    ioqueue = pjsip_endpt_get_ioqueue(listener->endpt);
    status = pj_ioqueue_register_sock(pool, ioqueue, sock, 
				      tcp, &tcp_callback, &tcp->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register transport to transport manager */
    status = pjsip_transport_register(listener->tpmgr, &tcp->base);
    if (status != PJ_SUCCESS)
	goto on_error;

    tcp->is_registered = PJ_TRUE;

    /* Done setting up basic transport. */
    *p_tcp = tcp;

on_error:
    tcp_destroy(&tcp->base);
    return status;
}


/* Flush all pending send operations */
static tcp_flush_pending_tx(struct tcp_transport *tcp)
{
    pj_lock_acquire(tcp->base.lock);
    while (!pj_list_empty(&tcp->tx_list)) {
	struct pending_tdata *pending_tx;
	pjsip_tx_data *tdata;
	pj_ioqueue_op_key_t *op_key;
	pj_ssize_t size;
	pj_status_t status;

	pending_tx = tcp->tx_list.next;
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



/* Destroy TCP transport */
static pj_status_t tcp_destroy(pjsip_transport *transport)
{
    struct tcp_transport *tcp = (struct tcp_transport*)transport;

    /* Cancel all pending transmits */
    while (!pj_list_empty(&tcp->tx_list)) {
	struct pending_tdata *pending_tx;
	pj_ioqueue_op_key_t *op_key;

	pending_tx = tcp->tx_list.next;
	pj_list_erase(pending_tx);

	op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	on_write_complete(tcp->key, op_key, 
			  -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN));
    }

    if (tcp->is_registered) {
	pjsip_transport_destroy(transport);
	tcp->is_registered = PJ_FALSE;
    }

    if (tcp->rdata.tp_info.pool) {
	pj_pool_release(tcp->rdata.tp_info.pool);
	tcp->rdata.tp_info.pool = NULL;
    }

    if (tcp->key) {
	pj_ioqueue_unregister(tcp->key);
	tcp->key = NULL;
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
	PJ_LOG(4,(tcp->base.obj_name, "TCP transport destroyed"));
	pj_pool_release(tcp->base.pool);
	tcp->base.pool = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * This utility function creates receive data buffers and start
 * asynchronous recv() operations from the socket.
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
    if (status != PJ_SUCCESS) {
	tcp_perror(tcp->base.obj_name, "ioqueue recv() error", status);
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
    status = create_tcp_transport(listener, sock, &local_addr, 
				  (pj_sockaddr_in*)rem_addr, &tcp);
    if (status != PJ_SUCCESS)
	return status;
        
    /* Start asynchronous connect() operation */
    tcp->has_pending_connect = PJ_TRUE;
    pj_ioqueue_op_key_init(&tcp->connect_op.op_key, 
			   sizeof(tcp->connect_op.op_key));
    tcp->connect_op.transport = tcp;
    status = pj_ioqueue_connect(tcp->key, rem_addr, sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS) {
	tcp_destroy(&tcp->base);
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

    do {
	if (status != PJ_SUCCESS) {
	    tcp_perror(listener->name, "Error in accept()", status);

	    ++err_cnt;
	    if (err_cnt >= 5) {
		PJ_LOG(1, (listener->name, 
			   "Too many errors, listener stopping"));
	    }

	    goto start_next_accept;
	}

	status = create_tcp_transport( listener, sock, 
				       &accept_op->local_addr, 
				       &accept_op->remote_addr, &tcp);
	if (status == PJ_SUCCESS) {
	    status = tcp_start_read(tcp);
	    if (status != PJ_SUCCESS) {
		PJ_LOG(3,(tcp->base.obj_name, "New transport cancelled"));
		tcp_destroy(&tcp->base);
	    }
	}

start_next_accept:

	status = pj_ioqueue_accept(listener->key, 
				   &accept_op->op_key,
				   &accept_op->new_sock,
				   &accept_op->local_addr,
				   &accept_op->remote_addr,
				   &accept_op->addr_len);

    } while (status != PJ_EPENDING);
}


/* Callback from ioqueue when packet is sent */
static void on_write_complete(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key, 
                              pj_ssize_t bytes_sent)
{
    struct tcp_transport *tp = pj_ioqueue_get_user_data(key);
    pjsip_tx_data_op_key *tdata_op_key = (pjsip_tx_data_op_key*)op_key;

    tdata_op_key->tdata = NULL;

    if (tdata_op_key->callback) {
	tdata_op_key->callback(&tp->base, tdata_op_key->token, bytes_sent);
    }
}


/* This callback is called by transport manager to send SIP message */
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
    pj_status_t status;

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
     * transmit data in the pending transmission list.
     */
    pj_lock_acquire(tcp->base.lock);

    if (tcp->has_pending_connect) {
	struct pending_tdata *pending_tdata;

	/* Pust to list */
	pending_tdata = pj_pool_alloc(tdata->pool, sizeof(*pending_tdata));
	pending_tdata->tdata_op_key = &tdata->op_key;

	pj_list_push_back(&tcp->tx_list, pending_tdata);
	status = PJ_EPENDING;

    } else {
	/* send to ioqueue! */
	size = tdata->buf.cur - tdata->buf.start;
	status = pj_ioqueue_send(tcp->key, 
				 (pj_ioqueue_op_key_t*)&tdata->op_key,
				 tdata->buf.start, &size, 0);

	if (status != PJ_EPENDING)
	    tdata->op_key.tdata = NULL;
    }

    pj_lock_release(tcp->base.lock);

    return status;
}


/* This callback is called by transport manager to shutdown transport */
static pj_status_t tcp_shutdown(pjsip_transport *transport)
{

    PJ_UNUSED_ARG(transport);

    /* Nothing to do for TCP */
    return PJ_SUCCESS;
}


/* Callback from ioqueue on incoming packet */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    enum { MAX_IMMEDIATE_PACKET = 10 };
    pjsip_rx_data_op_key *rdata_op_key = (pjsip_rx_data_op_key*) op_key;
    pjsip_rx_data *rdata = rdata_op_key->rdata;
    struct tcp_transport *tp = (struct tcp_transport*)rdata->tp_info.transport;
    int i;
    pj_status_t status;

    /* Don't do anything if transport is closing. */
    if (tp->is_closing) {
	tp->is_closing++;
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

	/* Report the packet to transport manager. */
	if (bytes_read > 0) {
	    pj_size_t size_eaten;

	    /* Init pkt_info part. */
	    rdata->pkt_info.len += bytes_read;
	    rdata->pkt_info.zero = 0;
	    pj_gettimeofday(&rdata->pkt_info.timestamp);

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
	    PJ_LOG(4,(tp->base.obj_name, "tcp connection closed"));
	    tcp_destroy(&tp->base);
	    return;

	} else if (bytes_read < 0)  {

	    /* Report error to endpoint. */
	    PJSIP_ENDPT_LOG_ERROR((rdata->tp_info.transport->endpt,
				   rdata->tp_info.transport->obj_name,
				   -bytes_read, "tcp recv() error"));

	    /* Transport error, close transport */
	    tcp_destroy(&tp->base);
	    return;
	}

	if (i >= MAX_IMMEDIATE_PACKET) {
	    /* Force ioqueue_recv() to return PJ_EPENDING */
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
	    /* Report error to endpoint */
	    PJSIP_ENDPT_LOG_ERROR((rdata->tp_info.transport->endpt,
				   rdata->tp_info.transport->obj_name,
				   status, "tcp recv() error"));

	    /* Transport error, close transport */
	    tcp_destroy(&tp->base);
	    return;
	}
    }
}


/* Callback from ioqueue when connect completes */
static void on_connect_complete(pj_ioqueue_key_t *key, 
                                pj_status_t status)
{
    struct pending_connect *connect_op = (struct pending_connect *)key;
    struct tcp_transport *tcp = connect_op->transport;
    pj_sockaddr_in addr;
    int addrlen;

    /* Mark that pending connect() operation has completed. */
    tcp->has_pending_connect = PJ_FALSE;

    /* Check connect() status */
    if (status != PJ_SUCCESS) {
	tcp_perror(tcp->base.obj_name, "TCP connect() error", status);
	tcp_destroy(&tcp->base);
	return;
    }

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
	tcp_destroy(&tcp->base);
	return;
    }

    /* Flush all pending send operations */
    tcp_flush_pending_tx(tcp);
}


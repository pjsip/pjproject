/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pj/activesock.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>

/* Only declare the API if PJ_HAS_TCP is true */
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0


#define THIS_FILE	"sip_transport_tcp.c"

#define MAX_ASYNC_CNT	16
#define POOL_LIS_INIT	512
#define POOL_LIS_INC	512
#define POOL_TP_INIT	512
#define POOL_TP_INC	512

struct tcp_listener;
struct tcp_transport;


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
    pj_activesock_t	    *asock;
    pj_sockaddr		     bound_addr;
    pj_qos_type		     qos_type;
    pj_qos_params	     qos_params;
    pj_sockopt_params	     sockopt_params;
    pj_bool_t		     reuse_addr;        
    unsigned		     async_cnt;    
    unsigned		     initial_timeout;

    /* Group lock to be used by TCP listener and ioqueue key */
    pj_grp_lock_t	    *grp_lock;
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
    pj_time_val              timeout;
};


/*
 * This structure describes the TCP transport, and it's descendant of
 * pjsip_transport.
 */
struct tcp_transport
{
    pjsip_transport	     base;
    pj_bool_t		     is_server;

    /* Do not save listener instance in the transport, because
     * listener might be destroyed during transport's lifetime.
     * See http://trac.pjsip.org/repos/ticket/491
    struct tcp_listener	    *listener;
     */

    pj_bool_t		     is_registered;
    pj_bool_t		     is_closing;
    pj_status_t		     close_reason;
    pj_sock_t		     sock;
    pj_activesock_t	    *asock;
    pj_bool_t		     has_pending_connect;

    /* Keep-alive timer. */
    pj_timer_entry	     ka_timer;
    pj_time_val		     last_activity;
    pjsip_tx_data_op_key     ka_op_key;
    pj_str_t		     ka_pkt;

    /* TCP transport can only have  one rdata!
     * Otherwise chunks of incoming PDU may be received on different
     * buffer.
     */
    pjsip_rx_data	     rdata;

    /* Pending transmission list. */
    struct delayed_tdata     delayed_list;

    /* Group lock to be used by TCP transport and ioqueue key */
    pj_grp_lock_t	    *grp_lock;

    /* Initial timer. */
    pj_timer_entry	     initial_timer;
};


/****************************************************************************
 * PROTOTYPES
 */

/* This callback is called when pending accept() operation completes. */
static pj_bool_t on_accept_complete(pj_activesock_t *asock,
				    pj_sock_t newsock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len);

/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory);

/* Clean up listener resources (group lock handler) */
static void lis_on_destroy(void *arg);

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
			      const pj_sockaddr *local,
			      const pj_sockaddr *remote,
			      struct tcp_transport **p_tcp);


static void tcp_perror(const char *sender, const char *title,
		       pj_status_t status)
{
    PJ_PERROR(3,(sender, status, "%s: [code=%d]", title, status));
}


static void sockaddr_to_host_port( pj_pool_t *pool,
				   pjsip_host_port *host_port,
				   const pj_sockaddr *addr )
{
    host_port->host.ptr = (char*) pj_pool_alloc(pool, PJ_INET6_ADDRSTRLEN+4);
    pj_sockaddr_print(addr, host_port->host.ptr, PJ_INET6_ADDRSTRLEN+4, 0);
    host_port->host.slen = pj_ansi_strlen(host_port->host.ptr);
    host_port->port = pj_sockaddr_get_port(addr);
}


static void tcp_init_shutdown(struct tcp_transport *tcp, pj_status_t status)
{
    pjsip_tp_state_callback state_cb;

    if (tcp->close_reason == PJ_SUCCESS)
	tcp->close_reason = status;

    if (tcp->base.is_shutdown || tcp->base.is_destroying)
	return;

    /* Prevent immediate transport destroy by application, as transport
     * state notification callback may be stacked and transport instance
     * must remain valid at any point in the callback.
     */
    pjsip_transport_add_ref(&tcp->base);

    /* Notify application of transport disconnected state */
    state_cb = pjsip_tpmgr_get_state_cb(tcp->base.tpmgr);
    if (state_cb) {
	pjsip_transport_state_info state_info;

	pj_bzero(&state_info, sizeof(state_info));
	state_info.status = tcp->close_reason;
	(*state_cb)(&tcp->base, PJSIP_TP_STATE_DISCONNECTED, &state_info);
    }

    /* check again */
    if (tcp->base.is_shutdown || tcp->base.is_destroying) {
        pjsip_transport_dec_ref(&tcp->base);
	return;
    }

    /* We can not destroy the transport since high level objects may
     * still keep reference to this transport. So we can only 
     * instruct transport manager to gracefully start the shutdown
     * procedure for this transport.
     */
    pjsip_transport_shutdown(&tcp->base);

    /* Now, it is ok to destroy the transport. */
    pjsip_transport_dec_ref(&tcp->base);
}


/*
 * Initialize pjsip_tcp_transport_cfg structure with default values.
 */
PJ_DEF(void) pjsip_tcp_transport_cfg_default(pjsip_tcp_transport_cfg *cfg,
					     int af)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->af = af;
    pj_sockaddr_init(cfg->af, &cfg->bind_addr, NULL, 0);
    cfg->async_cnt = 1;
    cfg->reuse_addr = PJSIP_TCP_TRANSPORT_REUSEADDR;
    cfg->initial_timeout = PJSIP_TCP_INITIAL_TIMEOUT;
}


/****************************************************************************
 * The TCP listener/transport factory.
 */

static void update_bound_addr(struct tcp_listener *listener,
			      const pj_sockaddr *local)
{
    pj_sockaddr *listener_addr = &listener->factory.local_addr;
    int af = pjsip_transport_type_get_af(listener->factory.type);

    /* Bind address may be different than factory.local_addr because
     * factory.local_addr will be resolved.
     */
    if (local) {
	pj_sockaddr_cp(&listener->bound_addr, local);
    }
    else {
	pj_sockaddr_init(af, &listener->bound_addr, NULL, 0);
    }
    pj_sockaddr_cp(listener_addr, &listener->bound_addr);
}

static pj_status_t update_factory_addr(struct tcp_listener *listener,				       
				       const pjsip_host_port *addr_name)
{
    pj_status_t status = PJ_SUCCESS;    
    pj_sockaddr *listener_addr = &listener->factory.local_addr;    

    /* If published host/IP is specified, then use that address as the
     * listener advertised address.
     */
    if (addr_name && addr_name->host.slen) {
	pj_sockaddr tmp;
	int af = pjsip_transport_type_get_af(listener->factory.type);

	/* Verify that address given in a_name (if any) is valid */
	status = pj_sockaddr_init(af, &tmp, &addr_name->host,
				  (pj_uint16_t)addr_name->port);
	if (status != PJ_SUCCESS || !pj_sockaddr_has_addr(&tmp) ||
	    (af == pj_AF_INET() && tmp.ipv4.sin_addr.s_addr == PJ_INADDR_NONE))
	{
	    /* Invalid address */
	    return PJ_EINVAL;
	}

	/* Copy the address */
	listener->factory.addr_name = *addr_name;
	pj_strdup(listener->factory.pool, &listener->factory.addr_name.host,
		  &addr_name->host);
	listener->factory.addr_name.port = addr_name->port;

    } else {
	/* No published address is given, use the bound address */

	/* If the address returns 0.0.0.0, use the default
	 * interface address as the transport's address.
	 */
	if (!pj_sockaddr_has_addr(listener_addr)) {
	    pj_sockaddr hostip;

	    status = pj_gethostip(listener->bound_addr.addr.sa_family,
				  &hostip);
	    if (status != PJ_SUCCESS)
		return status;

	    pj_sockaddr_copy_addr(listener_addr, &hostip);
	}

	/* Save the address name */
	sockaddr_to_host_port(listener->factory.pool,
			      &listener->factory.addr_name,
			      listener_addr);
    }

    /* If port is zero, get the bound port */
    if (listener->factory.addr_name.port == 0) {
	listener->factory.addr_name.port = pj_sockaddr_get_port(listener_addr);
    }

    pj_ansi_snprintf(listener->factory.obj_name,
		     sizeof(listener->factory.obj_name),
		     "tcptp:%d", listener->factory.addr_name.port);
    return status;
}

static void update_transport_info(struct tcp_listener *listener)
{    
    enum { INFO_LEN = 100 };
    char local_addr[PJ_INET6_ADDRSTRLEN + 10];
    char pub_addr[PJ_INET6_ADDRSTRLEN + 10];
    int len;
    pj_sockaddr *listener_addr = &listener->factory.local_addr;

    /* Set transport info. */
    if (listener->factory.info == NULL) {
	listener->factory.info = (char*)pj_pool_alloc(listener->factory.pool,
						      INFO_LEN);
    }
    pj_sockaddr_print(listener_addr, local_addr, sizeof(local_addr), 3);
    pj_addr_str_print(&listener->factory.addr_name.host, 
		      listener->factory.addr_name.port, pub_addr, 
		      sizeof(pub_addr), 1);
    len = pj_ansi_snprintf(
	    listener->factory.info, INFO_LEN, "tcp %s [published as %s]",
	    local_addr, pub_addr);
    PJ_CHECK_TRUNC_STR(len, listener->factory.info, INFO_LEN);

    if (listener->asock) {	
	char addr[PJ_INET6_ADDRSTRLEN+10];

	PJ_LOG(4, (listener->factory.obj_name,
		   "SIP TCP listener ready for incoming connections at %s",
		   pj_addr_str_print(&listener->factory.addr_name.host,
				     listener->factory.addr_name.port, addr,
				     sizeof(addr), 1)));
    } else {
	PJ_LOG(4, (listener->factory.obj_name, "SIP TCP is ready "
	       "(client only)"));
    }
}

/*
 * This is the public API to create, initialize, register, and start the
 * TCP listener.
 */
PJ_DEF(pj_status_t) pjsip_tcp_transport_start3(
					pjsip_endpoint *endpt,
					const pjsip_tcp_transport_cfg *cfg,
					pjsip_tpfactory **p_factory
					)
{        
    pj_pool_t *pool;    
    struct tcp_listener *listener;        
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && cfg->async_cnt, PJ_EINVAL);

    pool = pjsip_endpt_create_pool(endpt, "tcptp", POOL_LIS_INIT, 
				   POOL_LIS_INC);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);


    listener = PJ_POOL_ZALLOC_T(pool, struct tcp_listener);
    listener->factory.pool = pool;
    listener->factory.type = cfg->af==pj_AF_INET() ? PJSIP_TRANSPORT_TCP :
						     PJSIP_TRANSPORT_TCP6;
    listener->factory.type_name = (char*)
			 pjsip_transport_get_type_name(listener->factory.type);
    listener->factory.flag = 
		    pjsip_transport_get_flag_from_type(listener->factory.type);
    listener->qos_type = cfg->qos_type;
    listener->reuse_addr = cfg->reuse_addr;
    listener->async_cnt = cfg->async_cnt;
    listener->initial_timeout = cfg->initial_timeout;
    pj_memcpy(&listener->qos_params, &cfg->qos_params,
	      sizeof(cfg->qos_params));
    pj_memcpy(&listener->sockopt_params, &cfg->sockopt_params,
	      sizeof(cfg->sockopt_params));

    pj_ansi_strcpy(listener->factory.obj_name, "tcptp");
    if (listener->factory.type==PJSIP_TRANSPORT_TCP6)
	pj_ansi_strcat(listener->factory.obj_name, "6");

    status = pj_lock_create_recursive_mutex(pool, listener->factory.obj_name,
					    &listener->factory.lock);
    if (status != PJ_SUCCESS)
	goto on_error;    

    /* Create group lock */
    status = pj_grp_lock_create(pool, NULL, &listener->grp_lock);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_grp_lock_add_ref(listener->grp_lock);
    pj_grp_lock_add_handler(listener->grp_lock, pool, listener,
			    &lis_on_destroy);

    /* Register to transport manager */
    listener->endpt = endpt;
    listener->tpmgr = pjsip_endpt_get_tpmgr(endpt);
    listener->factory.create_transport = lis_create_transport;
    listener->factory.destroy = lis_destroy;    

#if !(defined(PJSIP_TCP_TRANSPORT_DONT_CREATE_LISTENER) && \
              PJSIP_TCP_TRANSPORT_DONT_CREATE_LISTENER != 0)
    /* Start listener. */
    status = pjsip_tcp_transport_lis_start(&listener->factory, &cfg->bind_addr, 
					   &cfg->addr_name);
    if (status != PJ_SUCCESS)
	goto on_error;

#else
    update_bound_addr(listener, &cfg->bind_addr);
    status = update_factory_addr(listener, &cfg->addr_name);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Set transport info. */
    update_transport_info(listener);
#endif

    listener->is_registered = PJ_TRUE;
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
					    &listener->factory);
    if (status != PJ_SUCCESS) {
	listener->is_registered = PJ_FALSE;
	goto on_error;
    }

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
PJ_DEF(pj_status_t) pjsip_tcp_transport_start2(pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       const pjsip_host_port *a_name,
					       unsigned async_cnt,
					       pjsip_tpfactory **p_factory)
{
    pjsip_tcp_transport_cfg cfg;

    pjsip_tcp_transport_cfg_default(&cfg, pj_AF_INET());

    if (local)
	pj_sockaddr_cp(&cfg.bind_addr, local);
    else
	pj_sockaddr_init(cfg.af, &cfg.bind_addr, NULL, 0);

    if (a_name)
	pj_memcpy(&cfg.addr_name, a_name, sizeof(*a_name));

    if (async_cnt)
	cfg.async_cnt = async_cnt;

    return pjsip_tcp_transport_start3(endpt, &cfg, p_factory);
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


/* Clean up listener resources */
static void lis_on_destroy(void *arg)
{
    struct tcp_listener *listener = (struct tcp_listener *)arg;

    if (listener->factory.lock) {
	pj_lock_destroy(listener->factory.lock);
	listener->factory.lock = NULL;
    }

    if (listener->factory.pool) {
	PJ_LOG(4,(listener->factory.obj_name,  "SIP TCP transport destroyed"));
	pj_pool_safe_release(&listener->factory.pool);
    }
}

/* This will close the listener. */
static void lis_close(struct tcp_listener *listener)
{
    if (listener->is_registered) {
	pjsip_tpmgr_unregister_tpfactory(listener->tpmgr, &listener->factory);
	listener->is_registered = PJ_FALSE;
    }

    if (listener->asock) {
	pj_activesock_close(listener->asock);
	listener->asock = NULL;
    }
}

/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory)
{
    struct tcp_listener *listener = (struct tcp_listener *)factory;

    lis_close(listener);

    if (listener->grp_lock) {
	pj_grp_lock_t *grp_lock = listener->grp_lock;
	listener->grp_lock = NULL;
	pj_grp_lock_dec_ref(grp_lock);
	/* Listener may have been deleted at this point */
    } else {
	lis_on_destroy(listener);
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
				pjsip_transport_callback callback);

/* Called by transport manager to shutdown */
static pj_status_t tcp_shutdown(pjsip_transport *transport);

/* Called by transport manager to destroy transport */
static pj_status_t tcp_destroy_transport(pjsip_transport *transport);

/* Utility to destroy transport */
static pj_status_t tcp_destroy(pjsip_transport *transport,
			       pj_status_t reason);

/* Callback on incoming data */
static pj_bool_t on_data_read(pj_activesock_t *asock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder);

/* Callback when packet is sent */
static pj_bool_t on_data_sent(pj_activesock_t *asock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent);

/* Callback when connect completes */
static pj_bool_t on_connect_complete(pj_activesock_t *asock,
				     pj_status_t status);

/* TCP keep-alive timer callback */
static void tcp_keep_alive_timer(pj_timer_heap_t *th, pj_timer_entry *e);

/* TCP initial timer callback */
static void tcp_initial_timer(pj_timer_heap_t *th, pj_timer_entry *e);

/* Clean up TCP resources */
static void tcp_on_destroy(void *arg);

/*
 * Common function to create TCP transport, called when pending accept() and
 * pending connect() complete.
 */
static pj_status_t tcp_create( struct tcp_listener *listener,
			       pj_pool_t *pool,
			       pj_sock_t sock, pj_bool_t is_server,
			       const pj_sockaddr *local,
			       const pj_sockaddr *remote,
			       struct tcp_transport **p_tcp)
{
    struct tcp_transport *tcp;
    pj_ioqueue_t *ioqueue;
    pj_activesock_cfg asock_cfg;
    pj_activesock_cb tcp_callback;
    const pj_str_t ka_pkt = PJSIP_TCP_KEEP_ALIVE_DATA;
    char print_addr[PJ_INET6_ADDRSTRLEN+10];
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
    tcp = PJ_POOL_ZALLOC_T(pool, struct tcp_transport);
    tcp->is_server = is_server;
    tcp->sock = sock;
    /*tcp->listener = listener;*/
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

    tcp->base.key.type = listener->factory.type;
    pj_sockaddr_cp(&tcp->base.key.rem_addr, remote);
    tcp->base.type_name = (char*)pjsip_transport_get_type_name(
				(pjsip_transport_type_e)tcp->base.key.type);
    tcp->base.flag = pjsip_transport_get_flag_from_type(
				(pjsip_transport_type_e)tcp->base.key.type);

    tcp->base.info = (char*) pj_pool_alloc(pool, 64);
    pj_ansi_snprintf(tcp->base.info, 64, "%s to %s",
                     tcp->base.type_name,
                     pj_sockaddr_print(remote, print_addr,
                                       sizeof(print_addr), 3));

    tcp->base.addr_len = pj_sockaddr_get_len(remote);
    pj_sockaddr_cp(&tcp->base.local_addr, local);
    sockaddr_to_host_port(pool, &tcp->base.local_name, local);
    sockaddr_to_host_port(pool, &tcp->base.remote_name, remote);
    tcp->base.dir = is_server? PJSIP_TP_DIR_INCOMING : PJSIP_TP_DIR_OUTGOING;

    tcp->base.endpt = listener->endpt;
    tcp->base.tpmgr = listener->tpmgr;
    tcp->base.send_msg = &tcp_send_msg;
    tcp->base.do_shutdown = &tcp_shutdown;
    tcp->base.destroy = &tcp_destroy_transport;
    tcp->base.factory = &listener->factory;

    /* Create group lock */
    status = pj_grp_lock_create_w_handler(pool, NULL, tcp, &tcp_on_destroy,
    					  &tcp->grp_lock);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_grp_lock_add_ref(tcp->grp_lock);

    tcp->base.grp_lock = tcp->grp_lock;

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = 1;
    asock_cfg.grp_lock = tcp->grp_lock;

    pj_bzero(&tcp_callback, sizeof(tcp_callback));
    tcp_callback.on_data_read = &on_data_read;
    tcp_callback.on_data_sent = &on_data_sent;
    tcp_callback.on_connect_complete = &on_connect_complete;

    ioqueue = pjsip_endpt_get_ioqueue(listener->endpt);
    status = pj_activesock_create(pool, sock, pj_SOCK_STREAM(), &asock_cfg,
				  ioqueue, &tcp_callback, tcp, &tcp->asock);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Register transport to transport manager */
    status = pjsip_transport_register(listener->tpmgr, &tcp->base);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    tcp->is_registered = PJ_TRUE;

    /* Initialize keep-alive timer */
    tcp->ka_timer.user_data = (void*)tcp;
    tcp->ka_timer.cb = &tcp_keep_alive_timer;
    pj_ioqueue_op_key_init(&tcp->ka_op_key.key, sizeof(pj_ioqueue_op_key_t));
    pj_strdup(tcp->base.pool, &tcp->ka_pkt, &ka_pkt);

    /* Initialize initial timer. */
    if (is_server && listener->initial_timeout) {
	pj_time_val delay = { 0 };

	tcp->initial_timer.user_data = (void*)tcp;
	tcp->initial_timer.cb = &tcp_initial_timer;
	
	delay.sec = listener->initial_timeout;
	pjsip_endpt_schedule_timer(listener->endpt, 
				    &tcp->initial_timer, 
				    &delay);
	tcp->initial_timer.id = PJ_TRUE;
    }

    /* Done setting up basic transport. */
    *p_tcp = tcp;

    PJ_LOG(4,(tcp->base.obj_name, "TCP %s transport created",
	      (tcp->is_server ? "server" : "client")));

    return PJ_SUCCESS;

on_error:
    if (tcp->grp_lock && pj_grp_lock_get_ref(tcp->grp_lock))
	tcp_destroy(&tcp->base, status);
    else
    	tcp_on_destroy(tcp);

    return status;
}


/* Flush all delayed transmision once the socket is connected. */
static void tcp_flush_pending_tx(struct tcp_transport *tcp)
{
    pj_time_val now;

    pj_gettickcount(&now);
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

        if (pending_tx->timeout.sec > 0 &&
            PJ_TIME_VAL_GT(now, pending_tx->timeout))
        {
            continue;
        }

	/* send! */
	size = tdata->buf.cur - tdata->buf.start;
	status = pj_activesock_send(tcp->asock, op_key, tdata->buf.start, 
				    &size, 0);
	if (status != PJ_EPENDING) {
            pj_lock_release(tcp->base.lock);
	    on_data_sent(tcp->asock, op_key, size);
            pj_lock_acquire(tcp->base.lock);
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

    /* Stop keep-alive timer. */
    if (tcp->ka_timer.id) {
	pjsip_endpt_cancel_timer(tcp->base.endpt, &tcp->ka_timer);
	tcp->ka_timer.id = PJ_FALSE;
    }

    /* Stop initial timer. */
    if (tcp->initial_timer.id) {
	pjsip_endpt_cancel_timer(tcp->base.endpt, &tcp->initial_timer);
	tcp->initial_timer.id = PJ_FALSE;
    }

    /* Cancel all delayed transmits */
    while (!pj_list_empty(&tcp->delayed_list)) {
	struct delayed_tdata *pending_tx;
	pj_ioqueue_op_key_t *op_key;

	pending_tx = tcp->delayed_list.next;
	pj_list_erase(pending_tx);

	op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

	on_data_sent(tcp->asock, op_key, -reason);
    }

    if (tcp->asock) {
	pj_activesock_close(tcp->asock);
	tcp->asock = NULL;
	tcp->sock = PJ_INVALID_SOCKET;
    } else if (tcp->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(tcp->sock);
	tcp->sock = PJ_INVALID_SOCKET;
    }

    if (tcp->grp_lock) {
	pj_grp_lock_t *grp_lock = tcp->grp_lock;
	tcp->grp_lock = NULL;
	pj_grp_lock_dec_ref(grp_lock);
	/* Transport may have been deleted at this point */
    }

    return PJ_SUCCESS;
}

/* Clean up TCP resources */
static void tcp_on_destroy(void *arg)
{
    struct tcp_transport *tcp = (struct tcp_transport*)arg;

    if (tcp->base.lock) {
	pj_lock_destroy(tcp->base.lock);
	tcp->base.lock = NULL;
    }

    if (tcp->base.ref_cnt) {
	pj_atomic_destroy(tcp->base.ref_cnt);
	tcp->base.ref_cnt = NULL;
    }

    if (tcp->rdata.tp_info.pool) {
	pj_pool_release(tcp->rdata.tp_info.pool);
	tcp->rdata.tp_info.pool = NULL;
    }

    if (tcp->base.pool) {
	pj_pool_t *pool;

	if (tcp->close_reason != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(tcp->close_reason, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(tcp->base.obj_name, 
		      "TCP transport destroyed with reason %d: %s", 
		      tcp->close_reason, errmsg));

	} else {

	    PJ_LOG(4,(tcp->base.obj_name, 
		      "TCP transport destroyed normally"));

	}

	pool = tcp->base.pool;
	tcp->base.pool = NULL;
	pj_pool_release(pool);
    }
}

/*
 * This utility function creates receive data buffers and start
 * asynchronous recv() operations from the socket. It is called after
 * accept() or connect() operation complete.
 */
static pj_status_t tcp_start_read(struct tcp_transport *tcp)
{
    pj_pool_t *pool;
    pj_uint32_t size;
    pj_sockaddr *rem_addr;
    void *readbuf[1];
    pj_status_t status;

    /* Init rdata */
    pool = pjsip_endpt_create_pool(tcp->base.endpt,
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
    tcp->rdata.pkt_info.src_addr_len = sizeof(tcp->rdata.pkt_info.src_addr);
    rem_addr = &tcp->base.key.rem_addr;
    pj_sockaddr_print(rem_addr, tcp->rdata.pkt_info.src_name,
                      sizeof(tcp->rdata.pkt_info.src_name), 0);
    tcp->rdata.pkt_info.src_port = pj_sockaddr_get_port(rem_addr);

    size = sizeof(tcp->rdata.pkt_info.packet);
    readbuf[0] = tcp->rdata.pkt_info.packet;
    status = pj_activesock_start_read2(tcp->asock, tcp->base.pool, size,
				       readbuf, 0);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	PJ_PERROR(4, (tcp->base.obj_name, status,
		      "pj_activesock_start_read() error"));
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
    pj_sockaddr local_addr;
    pj_status_t status;

    /* Sanity checks */
    PJ_ASSERT_RETURN(factory && mgr && endpt && rem_addr &&
		     addr_len && p_transport, PJ_EINVAL);

    /* Check that address is a sockaddr_in or sockaddr_in6*/
    PJ_ASSERT_RETURN((rem_addr->addr.sa_family == pj_AF_INET() &&
		      addr_len == sizeof(pj_sockaddr_in)) ||
		     (rem_addr->addr.sa_family == pj_AF_INET6() &&
		      addr_len == sizeof(pj_sockaddr_in6)), PJ_EINVAL);


    listener = (struct tcp_listener*)factory;

    /* Create socket */
    status = pj_sock_socket(rem_addr->addr.sa_family, pj_SOCK_STREAM(),
                            0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(sock, listener->qos_type, 
				&listener->qos_params, 
				2, listener->factory.obj_name, 
				"outgoing SIP TCP socket");

    /* Apply socket options, if specified */
    if (listener->sockopt_params.cnt)
	status = pj_sock_setsockopt_params(sock, &listener->sockopt_params);


    /* Bind to listener's address and any port */
    pj_bzero(&local_addr, sizeof(local_addr));
    pj_sockaddr_cp(&local_addr, &listener->bound_addr);
    pj_sockaddr_set_port(&local_addr, 0);

    status = pj_sock_bind(sock, &local_addr,
                          pj_sockaddr_get_len(&local_addr));
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    /* Get the local port */
    addr_len = sizeof(local_addr);
    status = pj_sock_getsockname(sock, &local_addr, &addr_len);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    /* Initially set the address from the listener's address */
    if (!pj_sockaddr_has_addr(&local_addr)) {
	pj_sockaddr_copy_addr(&local_addr, &listener->factory.local_addr);
    }

    /* Create the transport descriptor */
    status = tcp_create(listener, NULL, sock, PJ_FALSE, &local_addr, 
			rem_addr, &tcp);
    if (status != PJ_SUCCESS)
	return status;


    /* Start asynchronous connect() operation */
    tcp->has_pending_connect = PJ_TRUE;
    status = pj_activesock_start_connect(tcp->asock, tcp->base.pool, rem_addr,
					 addr_len);
    if (status == PJ_SUCCESS) {
	on_connect_complete(tcp->asock, PJ_SUCCESS);
    } else if (status != PJ_EPENDING) {
	tcp_destroy(&tcp->base, status);
	return status;
    }

    if (tcp->has_pending_connect) {
	char local_addr_buf[PJ_INET6_ADDRSTRLEN+10];
	char remote_addr_buf[PJ_INET6_ADDRSTRLEN+10];

	/* Update (again) local address, just in case local address currently
	 * set is different now that asynchronous connect() is started.
	 */
	addr_len = sizeof(local_addr);
	if (pj_sock_getsockname(sock, &local_addr, &addr_len)==PJ_SUCCESS) {
	    pj_sockaddr *tp_addr = &tcp->base.local_addr;

	    /* Some systems (like old Win32 perhaps) may not set local address
	     * properly before socket is fully connected.
	     */
	    if (pj_sockaddr_cmp(tp_addr, &local_addr) &&
                pj_sockaddr_has_addr(&local_addr) &&
		pj_sockaddr_get_port(&local_addr) != 0)
	    {
		pj_sockaddr_cp(tp_addr, &local_addr);
		sockaddr_to_host_port(tcp->base.pool, &tcp->base.local_name,
				      &local_addr);
	    }
	}
	
	PJ_LOG(4,(tcp->base.obj_name, 
		  "TCP transport %s is connecting to %s...",
		  pj_addr_str_print(&tcp->base.local_name.host, 
				    tcp->base.local_name.port, 
				    local_addr_buf, sizeof(local_addr_buf), 1),
		  pj_addr_str_print(&tcp->base.remote_name.host, 
			        tcp->base.remote_name.port, 
				remote_addr_buf, sizeof(remote_addr_buf), 1)));
    }

    /* Done */
    *p_transport = &tcp->base;

    return PJ_SUCCESS;
}


/*
 * This callback is called by active socket when pending accept() operation
 * has completed.
 */
static pj_bool_t on_accept_complete(pj_activesock_t *asock,
				    pj_sock_t sock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len)
{
    struct tcp_listener *listener;
    struct tcp_transport *tcp;
    char addr[PJ_INET6_ADDRSTRLEN+10];
    pjsip_tp_state_callback state_cb;
    pj_sockaddr tmp_src_addr, tmp_dst_addr;
    int addr_len;
    pj_status_t status;
    char addr_buf[PJ_INET6_ADDRSTRLEN+10];    

    PJ_UNUSED_ARG(src_addr_len);

    listener = (struct tcp_listener*) pj_activesock_get_user_data(asock);

    PJ_ASSERT_RETURN(sock != PJ_INVALID_SOCKET, PJ_TRUE);

    if (!listener->is_registered)
	return PJ_FALSE;

    PJ_LOG(4,(listener->factory.obj_name, 
	      "TCP listener %s: got incoming TCP connection "
	      "from %s, sock=%d",
	      pj_addr_str_print(&listener->factory.addr_name.host, 
				listener->factory.addr_name.port, addr_buf, 
				sizeof(addr_buf), 1),
	      pj_sockaddr_print(src_addr, addr, sizeof(addr), 3),
	      sock));

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(sock, listener->qos_type, 
				&listener->qos_params, 
				2, listener->factory.obj_name, 
				"incoming SIP TCP socket");

    /* Apply socket options, if specified */
    if (listener->sockopt_params.cnt)
	status = pj_sock_setsockopt_params(sock, &listener->sockopt_params);

    /* tcp_create() expect pj_sockaddr, so copy src_addr to temporary var,
     * just in case.
     */
    pj_bzero(&tmp_src_addr, sizeof(tmp_src_addr));
    pj_sockaddr_cp(&tmp_src_addr, src_addr);

    /* Get local address */
    addr_len = sizeof(tmp_dst_addr);
    status = pj_sock_getsockname(sock, &tmp_dst_addr, &addr_len);
    if (status != PJ_SUCCESS) {
	pj_sockaddr_cp(&tmp_dst_addr, &listener->factory.local_addr);
    }

    /* 
     * Incoming connection!
     * Create TCP transport for the new socket.
     */
    status = tcp_create( listener, NULL, sock, PJ_TRUE,
			 &tmp_dst_addr, &tmp_src_addr, &tcp);
    if (status == PJ_SUCCESS) {
	status = tcp_start_read(tcp);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3,(tcp->base.obj_name, "New transport cancelled"));
	    tcp_destroy(&tcp->base, status);
	} else {
	    if (tcp->base.is_shutdown || tcp->base.is_destroying) {
		return PJ_TRUE;
	    }
	    /* Start keep-alive timer */
	    if (pjsip_cfg()->tcp.keep_alive_interval) {
		pj_time_val delay = { 0 };
		delay.sec = pjsip_cfg()->tcp.keep_alive_interval;
		pjsip_endpt_schedule_timer(listener->endpt, 
					   &tcp->ka_timer, 
					   &delay);
		tcp->ka_timer.id = PJ_TRUE;
		pj_gettimeofday(&tcp->last_activity);
	    }
	    /* Notify application of transport state accepted */
	    state_cb = pjsip_tpmgr_get_state_cb(tcp->base.tpmgr);
	    if (state_cb) {
		pjsip_transport_state_info state_info;

		pj_bzero(&state_info, sizeof(state_info));
		(*state_cb)(&tcp->base, PJSIP_TP_STATE_CONNECTED, &state_info);
	    }
	}
    }

    return PJ_TRUE;
}


/* 
 * Callback from ioqueue when packet is sent.
 */
static pj_bool_t on_data_sent(pj_activesock_t *asock,
			      pj_ioqueue_op_key_t *op_key,
			      pj_ssize_t bytes_sent)
{
    struct tcp_transport *tcp = (struct tcp_transport*) 
    				pj_activesock_get_user_data(asock);
    pjsip_tx_data_op_key *tdata_op_key = (pjsip_tx_data_op_key*)op_key;

    /* Note that op_key may be the op_key from keep-alive, thus
     * it will not have tdata etc.
     */

    tdata_op_key->tdata = NULL;

    if (tdata_op_key->callback) {
	/*
	 * Notify sip_transport.c that packet has been sent.
	 */
	if (bytes_sent == 0)
	    bytes_sent = -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);

	tdata_op_key->callback(&tcp->base, tdata_op_key->token, bytes_sent);

	/* Mark last activity time */
	pj_gettimeofday(&tcp->last_activity);

    }

    /* Check for error/closure */
    if (bytes_sent <= 0) {
	pj_status_t status;

	PJ_LOG(5,(tcp->base.obj_name, "TCP send() error, sent=%d", 
		  bytes_sent));

	status = (bytes_sent == 0) ? PJ_RETURN_OS_ERROR(OSERR_ENOTCONN) :
				     (pj_status_t)-bytes_sent;

	tcp_init_shutdown(tcp, status);

	return PJ_FALSE;
    }

    return PJ_TRUE;
}


/* 
 * This callback is called by transport manager to send SIP message 
 */
static pj_status_t tcp_send_msg(pjsip_transport *transport, 
				pjsip_tx_data *tdata,
				const pj_sockaddr_t *rem_addr,
				int addr_len,
				void *token,
				pjsip_transport_callback callback)
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
    PJ_ASSERT_RETURN(rem_addr && (addr_len==sizeof(pj_sockaddr_in) ||
	                          addr_len==sizeof(pj_sockaddr_in6)),
	             PJ_EINVAL);

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
             * Starting from #1583 (https://trac.pjsip.org/repos/ticket/1583),
             * we also add timeout value for the transmit data. When the
             * connect() is completed, the timeout value will be checked to
             * determine whether the transmit data needs to be sent.
	     */
	    delayed_tdata = PJ_POOL_ZALLOC_T(tdata->pool, 
					     struct delayed_tdata);
	    delayed_tdata->tdata_op_key = &tdata->op_key;
            if (tdata->msg && tdata->msg->type == PJSIP_REQUEST_MSG) {
                pj_gettickcount(&delayed_tdata->timeout);
                delayed_tdata->timeout.msec += pjsip_cfg()->tsx.td;
                pj_time_val_normalize(&delayed_tdata->timeout);
            }

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
	status = pj_activesock_send(tcp->asock, 
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

		tcp_init_shutdown(tcp, status);
	    }
	}
    }

    return status;
}


/* 
 * This callback is called by transport manager to shutdown transport.
 */
static pj_status_t tcp_shutdown(pjsip_transport *transport)
{
    struct tcp_transport *tcp = (struct tcp_transport*)transport;
    
    /* Stop keep-alive timer. */
    if (tcp->ka_timer.id) {
	pjsip_endpt_cancel_timer(tcp->base.endpt, &tcp->ka_timer);
	tcp->ka_timer.id = PJ_FALSE;
    }

    /* Stop initial timer. */
    if (tcp->initial_timer.id) {
	pjsip_endpt_cancel_timer(tcp->base.endpt, &tcp->initial_timer);
	tcp->initial_timer.id = PJ_FALSE;
    }

    return PJ_SUCCESS;
}


/* 
 * Callback from ioqueue that an incoming data is received from the socket.
 */
static pj_bool_t on_data_read(pj_activesock_t *asock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder)
{
    enum { MAX_IMMEDIATE_PACKET = 10 };
    struct tcp_transport *tcp;
    pjsip_rx_data *rdata;

    PJ_UNUSED_ARG(data);

    tcp = (struct tcp_transport*) pj_activesock_get_user_data(asock);
    rdata = &tcp->rdata;

    /* Don't do anything if transport is closing. */
    if (tcp->is_closing) {
	tcp->is_closing++;
	return PJ_FALSE;
    }

    if (tcp->initial_timer.id) {
	pjsip_endpt_cancel_timer(tcp->base.endpt, &tcp->initial_timer);
	tcp->initial_timer.id = PJ_FALSE;
    }

    /* Houston, we have packet! Report the packet to transport manager
     * to be parsed.
     */
    if (status == PJ_SUCCESS) {
	pj_size_t size_eaten;

	/* Mark this as an activity */
	pj_gettimeofday(&tcp->last_activity);

	pj_assert((void*)rdata->pkt_info.packet == data);

	/* Init pkt_info part. */
	rdata->pkt_info.len = size;
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
	*remainder = size - size_eaten;
	if (*remainder > 0 && *remainder != size) {
	    pj_memmove(rdata->pkt_info.packet,
		       rdata->pkt_info.packet + size_eaten,
		       *remainder);
	}

    } else {

	/* Transport is closed */
	PJ_LOG(4,(tcp->base.obj_name, "TCP connection closed"));
	
	tcp_init_shutdown(tcp, status);

	return PJ_FALSE;

    }

    /* Reset pool. */
    pj_pool_reset(rdata->tp_info.pool);

    return PJ_TRUE;
}


/* 
 * Callback from ioqueue when asynchronous connect() operation completes.
 */
static pj_bool_t on_connect_complete(pj_activesock_t *asock,
				     pj_status_t status)
{
    struct tcp_transport *tcp;
    pj_sockaddr addr;
    int addrlen;
    pjsip_tp_state_callback state_cb;
    char local_addr_buf[PJ_INET6_ADDRSTRLEN+10];
    char remote_addr_buf[PJ_INET6_ADDRSTRLEN+10];

    tcp = (struct tcp_transport*) pj_activesock_get_user_data(asock);

    /* Mark that pending connect() operation has completed. */
    tcp->has_pending_connect = PJ_FALSE;

    /* If transport is being shutdown/destroyed, proceed as error connect.
     * Note that it is important to notify application via on_data_sent()
     * as otherwise the transport reference counter may never reach zero
     * (see #1898).
     */
    if ((tcp->base.is_shutdown || tcp->base.is_destroying) &&
	status == PJ_SUCCESS)
    {
	status = PJ_ECANCELLED;
    }

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

	    on_data_sent(tcp->asock, op_key, -status);
	}

	tcp_init_shutdown(tcp, status);
	return PJ_FALSE;
    }

    PJ_LOG(4,(tcp->base.obj_name, 
	      "TCP transport %s is connected to %s",
	      pj_addr_str_print(&tcp->base.local_name.host, 
				tcp->base.local_name.port, local_addr_buf, 
				sizeof(local_addr_buf), 1),
	      pj_addr_str_print(&tcp->base.remote_name.host, 
				tcp->base.remote_name.port, remote_addr_buf, 
				sizeof(remote_addr_buf), 1)));


    /* Update (again) local address, just in case local address currently
     * set is different now that the socket is connected (could happen
     * on some systems, like old Win32 probably?).
     */
    addrlen = sizeof(addr);
    if (pj_sock_getsockname(tcp->sock, &addr, &addrlen)==PJ_SUCCESS) {
	pj_sockaddr *tp_addr = &tcp->base.local_addr;

	if (pj_sockaddr_has_addr(&addr) &&
	    pj_sockaddr_cmp(&addr, tp_addr) != 0)
	{
	    pj_sockaddr_cp(tp_addr, &addr);
	    sockaddr_to_host_port(tcp->base.pool, &tcp->base.local_name,
				  tp_addr);
	}
    }

    /* Start pending read */
    status = tcp_start_read(tcp);
    if (status != PJ_SUCCESS) {
	tcp_init_shutdown(tcp, status);
	return PJ_FALSE;
    }

    /* Notify application of transport state connected */
    state_cb = pjsip_tpmgr_get_state_cb(tcp->base.tpmgr);
    if (state_cb) {
	pjsip_transport_state_info state_info;
	
	pj_bzero(&state_info, sizeof(state_info));
	(*state_cb)(&tcp->base, PJSIP_TP_STATE_CONNECTED, &state_info);
    }

    /* Flush all pending send operations */
    tcp_flush_pending_tx(tcp);

    /* Start keep-alive timer */
    if (pjsip_cfg()->tcp.keep_alive_interval) {
	pj_time_val delay = { 0 };
	delay.sec = pjsip_cfg()->tcp.keep_alive_interval;
	pjsip_endpt_schedule_timer(tcp->base.endpt, &tcp->ka_timer, 
				   &delay);
	tcp->ka_timer.id = PJ_TRUE;
	pj_gettimeofday(&tcp->last_activity);
    }

    return PJ_TRUE;
}

/* Transport keep-alive timer callback */
static void tcp_keep_alive_timer(pj_timer_heap_t *th, pj_timer_entry *e)
{
    struct tcp_transport *tcp = (struct tcp_transport*) e->user_data;
    pj_time_val delay;
    pj_time_val now;
    pj_ssize_t size;
    pj_status_t status;
    char addr[PJ_INET6_ADDRSTRLEN+10];        

    PJ_UNUSED_ARG(th);

    tcp->ka_timer.id = PJ_TRUE;

    pj_gettimeofday(&now);
    PJ_TIME_VAL_SUB(now, tcp->last_activity);

    if (now.sec > 0 && now.sec < pjsip_cfg()->tcp.keep_alive_interval) {
	/* There has been activity, so don't send keep-alive */
	delay.sec = pjsip_cfg()->tcp.keep_alive_interval - now.sec;
	delay.msec = 0;

	pjsip_endpt_schedule_timer(tcp->base.endpt, &tcp->ka_timer, 
				   &delay);
	tcp->ka_timer.id = PJ_TRUE;
	return;
    }

    PJ_LOG(5,(tcp->base.obj_name, "Sending %d byte(s) keep-alive to %s", 
	      (int)tcp->ka_pkt.slen, 
	      pj_addr_str_print(&tcp->base.remote_name.host, 
				tcp->base.remote_name.port, addr, 
				sizeof(addr), 1)));

    /* Send the data */
    size = tcp->ka_pkt.slen;
    status = pj_activesock_send(tcp->asock, &tcp->ka_op_key.key,
				tcp->ka_pkt.ptr, &size, 0);

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	tcp_perror(tcp->base.obj_name, 
		   "Error sending keep-alive packet", status);
	tcp_init_shutdown(tcp, status);
	return;
    }

    /* Register next keep-alive */
    delay.sec = pjsip_cfg()->tcp.keep_alive_interval;
    delay.msec = 0;

    pjsip_endpt_schedule_timer(tcp->base.endpt, &tcp->ka_timer, 
			       &delay);
    tcp->ka_timer.id = PJ_TRUE;
}

/* Transport keep-alive timer callback */
static void tcp_initial_timer(pj_timer_heap_t *th, pj_timer_entry *e)
{
    pj_status_t status = PJ_ETIMEDOUT;
    struct tcp_transport *tcp = (struct tcp_transport*) e->user_data;

    PJ_UNUSED_ARG(th);

    tcp_init_shutdown(tcp, status);
}

PJ_DEF(pj_sock_t) pjsip_tcp_transport_get_socket(pjsip_transport *transport)
{
    struct tcp_transport *tcp = (struct tcp_transport*)transport;

    PJ_ASSERT_RETURN(transport, PJ_INVALID_SOCKET);
    return tcp->sock;
}


PJ_DEF(pj_status_t) pjsip_tcp_transport_lis_start(pjsip_tpfactory *factory,
						 const pj_sockaddr *local,
					         const pjsip_host_port *a_name)
{
    pj_activesock_cfg asock_cfg;
    pj_activesock_cb listener_cb;
    pj_sock_t sock = PJ_INVALID_SOCKET;
    int addr_len, af;    
    struct tcp_listener *listener = (struct tcp_listener *)factory;
    pj_sockaddr *listener_addr = &factory->local_addr;
    pj_status_t status = PJ_SUCCESS;

    /* Nothing to be done, if listener already started. */
    if (listener->asock)
	return PJ_SUCCESS;
    
    update_bound_addr(listener, local);
      
    addr_len = pj_sockaddr_get_len(listener_addr);
    af = pjsip_transport_type_get_af(listener->factory.type);

    /* Create socket */
    status = pj_sock_socket(af, pj_SOCK_STREAM(), 0, &sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(sock, listener->qos_type,
				&listener->qos_params, 2,
				listener->factory.obj_name,
				"SIP TCP listener socket");

    /* Apply SO_REUSEADDR */
    if (listener->reuse_addr) {
	int enabled = 1;
	status = pj_sock_setsockopt(sock, pj_SOL_SOCKET(), pj_SO_REUSEADDR(),
				    &enabled, sizeof(enabled));
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4, (listener->factory.obj_name, status,
			  "Warning: error applying SO_REUSEADDR"));
	}
    }

    /* Apply socket options, if specified */
    if (listener->sockopt_params.cnt)
	status = pj_sock_setsockopt_params(sock, &listener->sockopt_params);

    status = pj_sock_bind(sock, listener_addr, addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Retrieve the bound address */
    status = pj_sock_getsockname(sock, &listener->factory.local_addr, 
				 &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = update_factory_addr(listener, a_name);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Start listening to the address */
    status = pj_sock_listen(sock, PJSIP_TCP_TRANSPORT_BACKLOG);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    if (listener->async_cnt > MAX_ASYNC_CNT)
	asock_cfg.async_cnt = MAX_ASYNC_CNT;
    else
	asock_cfg.async_cnt = listener->async_cnt;

    asock_cfg.grp_lock = listener->grp_lock;
    pj_bzero(&listener_cb, sizeof(listener_cb));
    listener_cb.on_accept_complete = &on_accept_complete;

    status = pj_activesock_create(listener->factory.pool, sock,
				  pj_SOCK_STREAM(), &asock_cfg,
				  pjsip_endpt_get_ioqueue(listener->endpt),
				  &listener_cb, listener,
				  &listener->asock);

    /* Start pending accept() operations */
    status = pj_activesock_start_accept(listener->asock,
					listener->factory.pool);

    update_transport_info(listener);

    return status;

on_error:
    if (listener->asock == NULL && sock != PJ_INVALID_SOCKET)
	pj_sock_close(sock);

    return status;
}


PJ_DEF(pj_status_t) pjsip_tcp_transport_restart(pjsip_tpfactory *factory,
						const pj_sockaddr *local,
						const pjsip_host_port *a_name)
{
    pj_status_t status = PJ_SUCCESS;
    struct tcp_listener *listener = (struct tcp_listener *)factory;

    lis_close(listener);

    status = pjsip_tcp_transport_lis_start(factory, local, a_name);
    if (status != PJ_SUCCESS) {	
	tcp_perror(listener->factory.obj_name,
		   "Unable to start listener after closing it", status);

	return status;
    }
    
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
					    &listener->factory);
    if (status != PJ_SUCCESS) {
	tcp_perror(listener->factory.obj_name,
		   "Unable to register the transport listener", status);
    } else {
	listener->is_registered = PJ_TRUE;	
    }    

    return status;
}

#endif	/* PJ_HAS_TCP */


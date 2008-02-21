/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include "turn.h"

#define MAX_CLIENTS		32
#define MAX_PEERS_PER_CLIENT	8
#define MAX_HANDLES		(MAX_CLIENTS*MAX_PEERS_PER_CLIENT+MAX_LISTENERS)
#define MAX_TIMER		(MAX_HANDLES * 2)
#define MIN_PORT		49152
#define MAX_PORT		65535
#define MAX_LISTENERS		16
#define MAX_THREADS		2

#define MAX_CLIENT_BANDWIDTH	128  /* In Kbps */
#define DEFA_CLIENT_BANDWIDTH	64

#define MIN_LIFETIME		32
#define MAX_LIFETIME		600
#define DEF_LIFETIME		300


/* Globals */
PJ_DEF_DATA(int) PJTURN_TP_UDP = 1;
PJ_DEF_DATA(int) PJTURN_TP_TCP = 2;
PJ_DEF_DATA(int) PJTURN_TP_TLS = 3;

/* Prototypes */
static pj_status_t on_tx_stun_msg( pj_stun_session *sess,
				   const void *pkt,
				   pj_size_t pkt_size,
				   const pj_sockaddr_t *dst_addr,
				   unsigned addr_len);
static pj_status_t on_rx_stun_request(pj_stun_session *sess,
				      const pj_uint8_t *pkt,
				      unsigned pkt_len,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len);


/*
 * Create server.
 */
PJ_DEF(pj_status_t) pjturn_srv_create( pj_pool_factory *pf,
				       pjturn_srv **p_srv)
{
    pj_pool_t *pool;
    pjturn_srv *srv;
    pj_status_t status;

    PJ_ASSERT_RETURN(pf && p_srv, PJ_EINVAL);

    /* Create server and init core settings */
    pool = pj_pool_create(pf, "srv%p", 1000, 1000, NULL);
    srv = PJ_POOL_ZALLOC_T(pool, pjturn_srv);
    srv->core.obj_name = pool->obj_name;
    srv->core.pf = pf;
    srv->core.pool = pool;
    
    status = pj_ioqueue_create(pool, MAX_HANDLES, &srv->core.ioqueue);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_timer_heap_create(pool, MAX_TIMER, &srv->core.timer_heap);
    if (status != PJ_SUCCESS)
	goto on_error;

    srv->core.listener = pj_pool_calloc(pool, MAX_LISTENERS, 
					sizeof(srv->core.listener[0]));
    srv->core.stun_sess = pj_pool_calloc(pool, MAX_LISTENERS,
					 (sizeof(srv->core.stun_sess[0])));

    srv->core.thread_cnt = MAX_THREADS;
    srv->core.thread = pj_pool_calloc(pool, srv->core.thread_cnt, 
				      sizeof(pj_thread_t*));

    status = pj_lock_create_recursive_mutex(pool, "srv%p", &srv->core.lock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Create hash tables */
    srv->tables.alloc = pj_hash_create(pool, MAX_CLIENTS);
    srv->tables.res = pj_hash_create(pool, MAX_CLIENTS);
    srv->tables.peer = pj_hash_create(pool, MAX_CLIENTS*MAX_PEERS_PER_CLIENT);

    /* Init ports settings */
    srv->ports.min_udp = srv->ports.next_udp = MIN_PORT;
    srv->ports.max_tcp = MAX_PORT;
    srv->ports.min_tcp = srv->ports.next_tcp = MIN_PORT;
    srv->ports.max_tcp = MAX_PORT;

    /* Init STUN config */
    pj_stun_config_init(&srv->core.stun_cfg, pf, 0, srv->core.ioqueue,
		        srv->core.timer_heap);

    *p_srv = srv;
    return PJ_SUCCESS;

on_error:
    pjturn_srv_destroy(srv);
    return status;
}

/** 
 * Create server.
 */
PJ_DEF(pj_status_t) pjturn_srv_destroy(pjturn_srv *srv)
{
    return PJ_SUCCESS;
}

/** 
 * Add listener.
 */
PJ_DEF(pj_status_t) pjturn_srv_add_listener(pjturn_srv *srv,
					    pjturn_listener *lis)
{
    pj_stun_session_cb sess_cb;
    unsigned index;
    pj_stun_session *sess;
    pj_status_t status;

    PJ_ASSERT_RETURN(srv && lis, PJ_EINVAL);
    PJ_ASSERT_RETURN(srv->core.lis_cnt < MAX_LISTENERS, PJ_ETOOMANY);

    /* Add to array */
    index = srv->core.lis_cnt;
    srv->core.listener[index] = lis;
    lis->server = srv;

    /* Create STUN session to handle new allocation */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_rx_request = &on_rx_stun_request;
    sess_cb.on_send_msg = &on_tx_stun_msg;

    status = pj_stun_session_create(&srv->core.stun_cfg, "lis%p", &sess_cb,
				    PJ_FALSE, &sess);
    if (status != PJ_SUCCESS) {
	srv->core.listener[index] = NULL;
	return status;
    }

    pj_stun_session_set_user_data(sess, lis);

    srv->core.stun_sess[index] = sess;
    lis->id = index;
    srv->core.lis_cnt++;

    return PJ_SUCCESS;
}


/* Callback from our own STUN session to send packet */
static pj_status_t on_tx_stun_msg( pj_stun_session *sess,
				   const void *pkt,
				   pj_size_t pkt_size,
				   const pj_sockaddr_t *dst_addr,
				   unsigned addr_len)
{
    pjturn_listener *listener;
    
    listener = (pjturn_listener*) pj_stun_session_get_user_data(sess);

    PJ_ASSERT_RETURN(listener!=NULL, PJ_EINVALIDOP);

    return pjturn_listener_sendto(listener, pkt, pkt_size, 0, 
				  dst_addr, addr_len);
}

/* Create and send error response */
static pj_status_t respond_error(pj_stun_sess *sess, const pj_stun_msg *req,
				 pj_bool_t cache, int code, const char *err_msg,
				 const pj_sockaddr_t *addr, unsigned addr_len)
{
    pj_status_t status;
    pj_str_t reason;
    pj_stun_tx_data *tdata;

    status = pj_stun_session_create_res(sess, req, 
				        code, (err_msg?pj_cstr(&reason,err_msg):NULL), 
					&tdata);
    if (status != PJ_SUCCESS)
	return statys;

    status = pj_stun_session_send_msg(sess, cache, dst_addr,  addr_len, tdata);
    return status;

}

/* Parse ALLOCATE request */
static pj_status_t parse_allocate_req(pjturn_allocation_req *cfg,
				      pjturn_listener *listener,
				      pj_stun_session *sess,
				      const pj_stun_msg *req,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len)
{
    pj_stun_bandwidth_attr *attr_bw;
    pj_stun_req_transport_attr *attr_req_tp;
    pj_stun_req_ip_attr *attr_req_ip;
    pj_stun_req_port_props_attr *attr_rpp;
    pj_stun_lifetime_attr *attr_lifetime;

    pj_bzero(cfg, sizeof(*cfg));

    /* Get BANDWIDTH attribute, if any. */
    attr_bw = pj_stun_msg_find_attr(msg, PJ_STUN_BANDWIDTH_ATTR, 0);
    if (attr_bw) {
	cfg->bandwidth = attr_bw->value;
    } else {
	cfg->bandwidth = DEFA_CLIENT_BANDWIDTH;
    }

    /* Check if we can satisfy the bandwidth */
    if (cfg->bandwidth > MAX_CLIENT_BANDWIDTH) {
	respond_error(sess, msg, PJ_FALSE, 
		      PJ_STUN_SC_ALLOCATION_QUOTA_REACHED, 
		      "Invalid bandwidth", src_addr, src_addr_len);
	return -1;
    }

    /* Get REQUESTED-TRANSPORT attribute, is any */
    attr_req_tp = pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REQ_TRANSPORT, 0);
    if (attr_req_tp) {
	cfg->tp_type = PJ_STUN_GET_RT_PROTO(attr_req_tp->value);
    } else {
	cfg->tp_type = listener->tp_type;
    }

    /* Can only support UDP for now */
    if (cfg->tp_type != PJTURN_TP_UDP) {
	respond_error(sess, msg, PJ_FALSE, 
		      PJ_STUN_SC_UNSUPP_TRANSPORT_PROTO, 
		      NULL, src_addr, src_addr_len);
	return -1;
    }

    /* Get REQUESTED-IP attribute, if any */
    attr_req_ip = pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REQ_IP, 0);
    if (attr_req_ip) {
	pj_memcpy(&cfg->addr, &attr_req_ip->sockaddr, 
		  sizeof(attr_req_ip->sockaddr));
    }

    /* Get REQUESTED-PORT-PROPS attribute, if any */
    attr_rpp = pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REQ_PORT_PROPS, 0);
    if (attr_rpp) {
	cfg->rpp_bits = PJ_STUN_GET_RPP_BITS(attr_rpp->value);
	cfg->rpp_port = PJ_STUN_GET_RPP_PORT(attr_rpp->value);
    } else {
	cfg->rpp_bits = 0;
	cfg->rpp_port = 0;
    }

    /* Get LIFETIME attribute */
    attr_lifetime = pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_LIFETIME, 0);
    if (attr_lifetime) {
	cfg->lifetime = attr_lifetime->value;
	if (cfg->lifetime < MIN_LIFETIME || cfg->lifetime > MAX_LIFETIME) {
	    respond_error(sess, msg, PJ_FALSE, 
			  PJ_STUN_SC_BAD_REQUEST, 
			  "Invalid LIFETIME value", src_addr, 
			  src_addr_len);
	    return -1;
	}
    } else {
	cfg->lifetime = DEF_LIFETIME;
    }

    return PJ_SUCCESS;
}

/* Callback from our own STUN session when incoming request arrives */
static pj_status_t on_rx_stun_request(pj_stun_session *sess,
				      const pj_uint8_t *pkt,
				      unsigned pkt_len,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len)
{
    pjturn_listener *listener;
    pjturn_allocation_req req;
    pj_status_t status;

    listener = (pjturn_listener*) pj_stun_session_get_user_data(sess);

    /* Handle strayed REFRESH request */
    if (msg->hdr.type == PJ_STUN_REFRESH_REQUEST) {
	return respond_error(sess, msg, PJ_FALSE, 
			     PJ_STUN_SC_ALLOCATION_MISMATCH,
			     NULL, src_addr, src_addr_len);
    }

    /* Respond any other requests with Bad Request response */
    if (msg->hdr.type != PJ_STUN_ALLOCATE_REQUEST) {
	return respond_error(sess, msg, PJ_FALSE, PJ_STUN_SC_BAD_REQUEST,
			     NULL, src_addr, src_addr_len);
    }

    /* We have ALLOCATE request here, and it's authenticated. Parse the
     * request.
     */
    status = parse_allocate_req(&req, listener, sess, msg, src_addr, 
				src_addr_len);
    if (status != PJ_SUCCESS)
	return status;

    /* Ready to allocate now */

}


/* Handle packet from new client address. */
static void handle_new_client( pjturn_srv *srv, 
			       pjturn_pkt *pkt)
{
    pj_stun_msg *req, *res;
    unsigned options, lis_id;
    pj_status_t status;

    /* Check that this is a STUN message */
    options = PJ_STUN_CHECK_PACKET;
    if (pkt->listener->tp_type == PJTURN_TP_UDP)
	options |= PJ_STUN_IS_DATAGRAM;

    status = pj_stun_msg_check(pkt->pkt, pkt->len, options);
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	char ip[PJ_INET6_ADDRSTRLEN+10];

	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(5,(srv->core.obj_name, 
	          "Non STUN packet from %s is dropped: %s",
		  pj_sockaddr_print(&pkt->src.clt_addr, ip, sizeof(ip), 3),
		  errmsg));
	return;
    }

    lis_id = pkt->listener->id;

    /* Hand over processing to STUN session */
    options &= ~PJ_STUN_CHECK_PACKET;
    status = pj_stun_session_on_rx_pkt(srv->core.stun_sess[lis_id], pkt->pkt, 
				       pkt->len, options, NULL,
				       &pkt->src.clt_addr, 
				       pkt->src_addr_len);
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	char ip[PJ_INET6_ADDRSTRLEN+10];

	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(5,(srv->core.obj_name, 
	          "Error processing STUN packet from %s: %s",
		  pj_sockaddr_print(&pkt->src.clt_addr, ip, sizeof(ip), 3),
		  errmsg));
	return;
    }
}


/*
 * This callback is called by UDP listener on incoming packet.
 */
PJ_DEF(void) pjturn_srv_on_rx_pkt( pjturn_srv *srv, 
				   pjturn_pkt *pkt)
{
    pjturn_allocation *alloc;

    /* Get TURN allocation from the source address */
    pj_lock_acquire(srv->core.lock);
    alloc = pj_hash_get(srv->tables.alloc, &pkt->src, sizeof(pkt->src), NULL);
    pj_lock_release(srv->core.lock);

    /* If allocation is found, just hand over the packet to the
     * allocation.
     */
    if (alloc) {
	pjturn_allocation_on_rx_pkt(alloc, pkt);
    } else {
	/* Otherwise this is a new client */
	handle_new_client(srv, pkt);
    }
}



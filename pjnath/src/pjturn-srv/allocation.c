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

#define THIS_FILE   "allocation.c"


enum {
    TIMER_ID_NONE,
    TIMER_ID_TIMEOUT,
    TIMER_ID_DESTROY
};

#define DESTROY_DELAY	    {0, 500}
#define PEER_TABLE_SIZE	    32

/* ChannelData header */
typedef struct channel_data_hdr
{
    pj_uint16_t ch_number;
    pj_uint16_t length;
} channel_data_hdr;


/* Prototypes */
static pj_status_t create_relay(pjturn_allocation *alloc,
				const pjturn_allocation_req *req);
static void on_rx_from_peer(pj_ioqueue_key_t *key, 
                            pj_ioqueue_op_key_t *op_key, 
                            pj_ssize_t bytes_read);
static void destroy_relay(pjturn_relay_res *relay);
static pj_status_t stun_on_send_msg(pj_stun_session *sess,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len);
static pj_status_t stun_on_rx_request(pj_stun_session *sess,
				      const pj_uint8_t *pkt,
				      unsigned pkt_len,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len);
static pj_status_t stun_on_rx_indication(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len);

/* Log allocation error */
static void alloc_err(pjturn_allocation *alloc, const char *title,
		      pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(4,(alloc->obj_name, "%s for client %s: %s",
	      title, alloc->info, errmsg));
}

/*
 * Create new allocation.
 */
PJ_DEF(pj_status_t) pjturn_allocation_create(pjturn_listener *listener,
					     const pj_sockaddr_t *src_addr,
					     unsigned src_addr_len,
					     const pj_stun_msg *msg,
					     const pjturn_allocation_req *req,
					     pjturn_allocation **p_alloc)
{
    pjturn_srv *srv = listener->server;
    pj_pool_t *pool;
    pjturn_allocation *alloc;
    pj_stun_session_cb sess_cb;
    char relay_info[80];
    pj_status_t status;

    pool = pj_pool_create(srv->core.pf, "alloc%p", 1000, 1000, NULL);

    /* Init allocation structure */
    alloc = PJ_POOL_ZALLOC_T(pool, pjturn_allocation);
    alloc->pool = pool;
    alloc->obj_name = pool->obj_name;
    alloc->listener = listener;
    alloc->clt_sock = PJ_INVALID_SOCKET;
    alloc->relay.tp.sock = PJ_INVALID_SOCKET;

    alloc->bandwidth = req->bandwidth;

    alloc->hkey.tp_type = listener->tp_type;
    pj_memcpy(&alloc->hkey.clt_addr, src_addr, src_addr_len);

    status = pj_lock_create_recursive_mutex(pool, alloc->obj_name, 
					    &alloc->lock);
    if (status != PJ_SUCCESS) {
	pjturn_allocation_destroy(alloc);
	return status;
    }

    /* Create peer hash table */
    alloc->peer_table = pj_hash_create(pool, PEER_TABLE_SIZE);

    /* Create channel hash table */
    alloc->ch_table = pj_hash_create(pool, PEER_TABLE_SIZE);

    /* Print info */
    pj_ansi_strcpy(alloc->info, pjturn_tp_type_name(listener->tp_type));
    alloc->info[3] = ':';
    pj_sockaddr_print(src_addr, alloc->info+4, sizeof(alloc->info)-4, 3);

    /* Create STUN session to handle STUN communication with client */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_msg = &stun_on_send_msg;
    sess_cb.on_rx_request = &stun_on_rx_request;
    sess_cb.on_rx_indication = &stun_on_rx_indication;
    status = pj_stun_session_create(&srv->core.stun_cfg, alloc->obj_name,
				    &sess_cb, PJ_FALSE, &alloc->sess);
    if (status != PJ_SUCCESS) {
	pjturn_allocation_destroy(alloc);
	return status;
    }

    /* Attach to STUN session */
    pj_stun_session_set_user_data(alloc->sess, alloc);

    /* Create the relay resource */
    status = pjturn_allocation_create_relay(srv, alloc, msg, req, 
					    &alloc->relay);
    if (status != PJ_SUCCESS) {
	pjturn_allocation_destroy(alloc);
	return status;
    }

    /* Register this allocation */
    pjturn_srv_register_allocation(srv, alloc);

    pj_sockaddr_print(&alloc->relay.hkey.addr, relay_info, 
		      sizeof(relay_info), 3);
    PJ_LOG(4,(alloc->obj_name, "Client %s created, relay addr=%s:%s", 
	      alloc->info, pjturn_tp_type_name(req->tp_type), relay_info));

    /* Success */
    *p_alloc = alloc;
    return PJ_SUCCESS;
}


/*
 * Destroy allocation.
 */
PJ_DECL(void) pjturn_allocation_destroy(pjturn_allocation *alloc)
{
    pj_pool_t *pool;

    /* Unregister this allocation */
    pjturn_srv_unregister_allocation(alloc->listener->server, alloc);

    /* Destroy relay */
    destroy_relay(&alloc->relay);

    /* Must lock only after destroying relay otherwise deadlock */
    if (alloc->lock) {
	pj_lock_acquire(alloc->lock);
    }

    /* Destroy STUN session */
    if (alloc->sess) {
	pj_stun_session_destroy(alloc->sess);
	alloc->sess = NULL;
    }

    /* Destroy lock */
    if (alloc->lock) {
	pj_lock_release(alloc->lock);
	pj_lock_destroy(alloc->lock);
	alloc->lock = NULL;
    }

    /* Destroy pool */
    pool = alloc->pool;
    if (pool) {
	alloc->pool = NULL;
	pj_pool_release(pool);
    }
}


/* Destroy relay resource */
static void destroy_relay(pjturn_relay_res *relay)
{
    if (relay->timer.id) {
	pj_timer_heap_cancel(relay->allocation->listener->server->core.timer_heap, 
			     &relay->timer);
	relay->timer.id = PJ_FALSE;
    }

    if (relay->tp.key) {
	pj_ioqueue_unregister(relay->tp.key);
	relay->tp.key = NULL;
	relay->tp.sock = PJ_INVALID_SOCKET;
    } else if (relay->tp.sock != PJ_INVALID_SOCKET) {
	pj_sock_close(relay->tp.sock);
	relay->tp.sock = PJ_INVALID_SOCKET;
    }

    /* Mark as shutdown */
    relay->lifetime = 0;
}

/* Initiate shutdown sequence for this allocation */
static void alloc_shutdown(pjturn_allocation *alloc)
{
    pj_time_val destroy_delay = DESTROY_DELAY;

    /* Work with existing schedule */
    if (alloc->relay.timer.id == TIMER_ID_TIMEOUT) {
	/* Cancel existing timer */
	pj_timer_heap_cancel(alloc->listener->server->core.timer_heap,
			     &alloc->relay.timer);
	alloc->relay.timer.id = TIMER_ID_NONE;

    } else if (alloc->relay.timer.id == TIMER_ID_DESTROY) {
	/* We've been scheduled to be destroyed, ignore this
	 * shutdown request.
	 */
	return;
    } 

    pj_assert(alloc->relay.timer.id == TIMER_ID_NONE);

    /* Shutdown relay socket */
    destroy_relay(&alloc->relay);

    /* Don't unregister from hash table because we still need to
     * handle REFRESH retransmission.
     */

    /* Schedule destroy timer */
    alloc->relay.timer.id = TIMER_ID_DESTROY;
    pj_timer_heap_schedule(alloc->listener->server->core.timer_heap,
			   &alloc->relay.timer, &destroy_delay);
}

/* Reschedule timeout using current lifetime setting */
static pj_status_t resched_timeout(pjturn_allocation *alloc)
{
    pj_time_val delay;
    pj_status_t status;

    pj_gettimeofday(&alloc->relay.expiry);
    alloc->relay.expiry.sec += alloc->relay.lifetime;

    pj_assert(alloc->relay.timer.id != TIMER_ID_DESTROY);
    if (alloc->relay.timer.id != 0) {
	pj_timer_heap_cancel(alloc->listener->server->core.timer_heap,
			     &alloc->relay.timer);
	alloc->relay.timer.id = TIMER_ID_NONE;
    }

    delay.sec = alloc->relay.lifetime;
    delay.msec = 0;

    alloc->relay.timer.id = TIMER_ID_TIMEOUT;
    status = pj_timer_heap_schedule(alloc->listener->server->core.timer_heap,
				    &alloc->relay.timer, &delay);
    if (status != PJ_SUCCESS) {
	alloc->relay.timer.id = TIMER_ID_NONE;
	return status;
    }

    return PJ_SUCCESS;
}


/* Timer timeout callback */
static void relay_timeout_cb(pj_timer_heap_t *heap, pj_timer_entry *e)
{
    pjturn_relay_res *rel;
    pjturn_allocation *alloc;

    rel = (pjturn_relay_res*) e->user_data;
    alloc = rel->allocation;

    if (e->id == TIMER_ID_TIMEOUT) {

	e->id = TIMER_ID_NONE;

	PJ_LOG(4,(alloc->obj_name, 
		  "Client %s refresh timed-out, shutting down..", 
		  alloc->info));

	alloc_shutdown(alloc);

    } else if (e->id == TIMER_ID_DESTROY) {
	e->id = TIMER_ID_NONE;

	PJ_LOG(4,(alloc->obj_name, "Client %s destroying..", 
		  alloc->info));

	pjturn_allocation_destroy(alloc);
    }
}


/*
 * Create relay.
 */
PJ_DEF(pj_status_t) pjturn_allocation_create_relay(pjturn_srv *srv,
						   pjturn_allocation *alloc,
						   const pj_stun_msg *msg,
						   const pjturn_allocation_req *req,
						   pjturn_relay_res *relay)
{
    enum { RETRY = 40 };
    pj_pool_t *pool = alloc->pool;
    int retry, retry_max, sock_type;
    pj_ioqueue_callback icb;
    int af, namelen;
    pj_stun_string_attr *sa;
    pj_status_t status;

    pj_bzero(relay, sizeof(*relay));
    
    relay->allocation = alloc;
    relay->tp.sock = PJ_INVALID_SOCKET;
    
    /* TODO: get the requested address family from somewhere */
    af = alloc->listener->addr.addr.sa_family;

    /* Save realm */
    sa = (pj_stun_string_attr*)
	 pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REALM, 0);
    PJ_ASSERT_RETURN(sa, PJ_EINVALIDOP);
    pj_strdup(pool, &relay->realm, &sa->value);

    /* Save username */
    sa = (pj_stun_string_attr*)
	 pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_USERNAME, 0);
    PJ_ASSERT_RETURN(sa, PJ_EINVALIDOP);
    pj_strdup(pool, &relay->user, &sa->value);

    /* Lifetime and timeout */
    relay->lifetime = req->lifetime;
    pj_timer_entry_init(&relay->timer, TIMER_ID_NONE, relay, 
			&relay_timeout_cb);
    resched_timeout(alloc);
    
    /* Transport type */
    relay->hkey.tp_type = req->tp_type;

    /* Create the socket */
    if (req->tp_type == PJTURN_TP_UDP) {
	sock_type = pj_SOCK_DGRAM();
    } else if (req->tp_type == PJTURN_TP_TCP) {
	sock_type = pj_SOCK_STREAM();
    } else {
	pj_assert(!"Unknown transport");
	return PJ_EINVALIDOP;
    }

    status = pj_sock_socket(af, sock_type, 0, &relay->tp.sock);
    if (status != PJ_SUCCESS) {
	pj_bzero(relay, sizeof(*relay));
	return status;
    }

    /* Find suitable port for this allocation */
    if (req->rpp_port) {
	retry_max = 1;
    } else {
	retry_max = RETRY;
    }

    for (retry=0; retry<retry_max; ++retry) {
	pj_uint16_t port;
	pj_sockaddr bound_addr;

	pj_lock_acquire(srv->core.lock);

	if (req->rpp_port) {
	    port = (pj_uint16_t) req->rpp_port;
	} else if (req->tp_type == PJTURN_TP_UDP) {
	    port = (pj_uint16_t) srv->ports.next_udp++;
	    if (srv->ports.next_udp > srv->ports.max_udp)
		srv->ports.next_udp = srv->ports.min_udp;
	} else if (req->tp_type == PJTURN_TP_TCP) {
	    port = (pj_uint16_t) srv->ports.next_tcp++;
	    if (srv->ports.next_tcp > srv->ports.max_tcp)
		srv->ports.next_tcp = srv->ports.min_tcp;
	} else {
	    pj_assert(!"Invalid transport");
	}

	pj_lock_release(srv->core.lock);

	pj_sockaddr_init(af, &bound_addr, NULL, port);

	status = pj_sock_bind(relay->tp.sock, &bound_addr, 
			      pj_sockaddr_get_len(&bound_addr));
	if (status == PJ_SUCCESS)
	    break;
    }

    if (status != PJ_SUCCESS) {
	/* Unable to allocate port */
	PJ_LOG(4,(THIS_FILE, "bind() failed: err %d", 
		  status));
	pj_sock_close(relay->tp.sock);
	relay->tp.sock = PJ_INVALID_SOCKET;
	return status;
    }

    /* Init relay key */
    namelen = sizeof(relay->hkey.addr);
    status = pj_sock_getsockname(relay->tp.sock, &relay->hkey.addr, &namelen);
    if (status != PJ_SUCCESS) {
	PJ_LOG(4,(THIS_FILE, "pj_sock_getsockname() failed: err %d", 
		  status));
	pj_sock_close(relay->tp.sock);
	relay->tp.sock = PJ_INVALID_SOCKET;
	return status;
    }
    if (!pj_sockaddr_has_addr(&relay->hkey.addr)) {
	pj_sockaddr_copy_addr(&relay->hkey.addr, &alloc->listener->addr);
    }

    /* Init ioqueue */
    pj_bzero(&icb, sizeof(icb));
    icb.on_read_complete = &on_rx_from_peer;

    status = pj_ioqueue_register_sock(pool, srv->core.ioqueue, relay->tp.sock,
				      relay, &icb, &relay->tp.key);
    if (status != PJ_SUCCESS) {
	PJ_LOG(4,(THIS_FILE, "pj_ioqueue_register_sock() failed: err %d", 
		  status));
	pj_sock_close(relay->tp.sock);
	relay->tp.sock = PJ_INVALID_SOCKET;
	return status;
    }

    /* Kick off pending read operation */
    pj_ioqueue_op_key_init(&relay->tp.read_key, sizeof(relay->tp.read_key));
    on_rx_from_peer(relay->tp.key, &relay->tp.read_key, 0);

    /* Done */
    return PJ_SUCCESS;
}

/* Create and send error response */
static void send_reply_err(pjturn_allocation *alloc,
			   const pj_stun_msg *req,
			   pj_bool_t cache, 
			   int code, const char *errmsg)
{
    pj_status_t status;
    pj_str_t reason;
    pj_stun_tx_data *tdata;

    status = pj_stun_session_create_res(alloc->sess, req, 
				        code, (errmsg?pj_cstr(&reason,errmsg):NULL), 
					&tdata);
    if (status != PJ_SUCCESS) {
	alloc_err(alloc, "Error creating STUN error response", status);
	return;
    }

    status = pj_stun_session_send_msg(alloc->sess, cache, 
				      &alloc->hkey.clt_addr,  
				      pj_sockaddr_get_len(&alloc->hkey.clt_addr), 
				      tdata);
    if (status != PJ_SUCCESS) {
	alloc_err(alloc, "Error sending STUN error response", status);
	return;
    }
}

/* Create and send successful response */
static void send_reply_ok(pjturn_allocation *alloc,
		          const pj_stun_msg *req)
{
    pj_status_t status;
    unsigned interval;
    pj_stun_tx_data *tdata;

    status = pj_stun_session_create_res(alloc->sess, req, 0, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	alloc_err(alloc, "Error creating STUN success response", status);
	return;
    }

    /* Calculate time to expiration */
    if (alloc->relay.lifetime != 0) {
	pj_time_val now;
	pj_gettimeofday(&now);
	interval = alloc->relay.expiry.sec - now.sec;
    } else {
	interval = 0;
    }

    /* Add LIFETIME. */
    pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg,
			      PJ_STUN_ATTR_LIFETIME, interval);

    /* Add BANDWIDTH */
    pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg,
			      PJ_STUN_ATTR_BANDWIDTH,
			      alloc->bandwidth);

    status = pj_stun_session_send_msg(alloc->sess, PJ_TRUE, 
				      &alloc->hkey.clt_addr,  
				      pj_sockaddr_get_len(&alloc->hkey.clt_addr), 
				      tdata);
    if (status != PJ_SUCCESS) {
	alloc_err(alloc, "Error sending STUN success response", status);
	return;
    }
}


/* Create new permission */
static pjturn_permission *create_permission(pjturn_allocation *alloc,
					    const pj_sockaddr_t *peer_addr,
					    unsigned addr_len)
{
    pjturn_permission *perm;

    perm = PJ_POOL_ZALLOC_T(alloc->pool, pjturn_permission);
    pj_memcpy(&perm->hkey.peer_addr, peer_addr, addr_len);
    
    if (alloc->listener->tp_type == PJTURN_TP_UDP) {
	perm->sock = alloc->listener->sock;
    } else {
	pj_assert(!"TCP is not supported yet");
	return NULL;
    }

    perm->allocation = alloc;
    perm->channel = PJTURN_INVALID_CHANNEL;

    pj_gettimeofday(&perm->expiry);
    perm->expiry.sec += PJTURN_PERM_TIMEOUT;

    return perm;
}

/* Check if a permission isn't expired. Return NULL if expired. */
static pjturn_permission *check_permission_expiry(pjturn_permission *perm)
{
    pjturn_allocation *alloc = perm->allocation;
    pj_time_val now;

    pj_gettimeofday(&now);
    if (PJ_TIME_VAL_LT(perm->expiry, now)) {
	/* Permission has not expired */
	return perm;
    }

    /* Remove from permission hash table */
    pj_hash_set(NULL, alloc->peer_table, &perm->hkey, sizeof(perm->hkey),
		0, NULL);

    /* Remove from channel hash table, if assigned a channel number */
    if (perm->channel != PJTURN_INVALID_CHANNEL) {
	pj_hash_set(NULL, alloc->ch_table, &perm->channel, 
		    sizeof(perm->channel), 0, NULL);
    }

    return NULL;
}

/* Lookup permission in hash table by the peer address */
static pjturn_permission*
lookup_permission_by_addr(pjturn_allocation *alloc,
			  const pj_sockaddr_t *peer_addr,
			  unsigned addr_len)
{
    pjturn_permission_key key;
    pjturn_permission *perm;

    pj_bzero(&key, sizeof(key));
    pj_memcpy(&key, peer_addr, addr_len);

    /* Lookup in peer hash table */
    perm = (pjturn_permission*) pj_hash_get(alloc->peer_table, &key,
					    sizeof(key), NULL);
    return check_permission_expiry(perm);
}

/* Lookup permission in hash table by the channel number */
static pjturn_permission*
lookup_permission_by_chnum(pjturn_allocation *alloc,
			   unsigned chnum)
{
    pj_uint16_t chnum16 = (pj_uint16_t)chnum;
    pjturn_permission *perm;

    /* Lookup in peer hash table */
    perm = (pjturn_permission*) pj_hash_get(alloc->peer_table, &chnum16,
					    sizeof(chnum16), NULL);
    return check_permission_expiry(perm);
}

/* Update permission because of data from client to peer. 
 * Return PJ_TRUE is permission is found.
 */
static pj_bool_t refresh_permission(pjturn_permission *perm)
{
    pj_gettimeofday(&perm->expiry);
    if (perm->channel == PJTURN_INVALID_CHANNEL)
	perm->expiry.sec += PJTURN_PERM_TIMEOUT;
    else
	perm->expiry.sec += PJTURN_CHANNEL_TIMEOUT;
    return PJ_TRUE;
}

/*
 * Handle incoming packet from client.
 */
PJ_DEF(void) pjturn_allocation_on_rx_client_pkt( pjturn_allocation *alloc,
						 pjturn_pkt *pkt)
{
    pj_bool_t is_stun;
    pj_status_t status;

    /* Quickly check if this is STUN message */
    is_stun = ((*((pj_uint8_t*)pkt->pkt) & 0xC0) == 0);

    if (is_stun) {
	/*
	 * This could be an incoming STUN requests or indications.
	 * Pass this through to the STUN session, which will call
	 * our stun_on_rx_request() or stun_on_rx_indication()
	 * callbacks.
	 */
	unsigned options = PJ_STUN_CHECK_PACKET;
	if (pkt->listener->tp_type == PJTURN_TP_UDP)
	    options |= PJ_STUN_IS_DATAGRAM;

	status = pj_stun_session_on_rx_pkt(alloc->sess, pkt->pkt, pkt->len,
					   options, NULL,
					   &pkt->src.clt_addr, 
					   pkt->src_addr_len);
	if (status != PJ_SUCCESS) {
	    alloc_err(alloc, "Error handling STUN packet", status);
	    return;
	}

    } else {
	/*
	 * This is not a STUN packet, must be ChannelData packet.
	 */
	channel_data_hdr *cd = (channel_data_hdr*)pkt->pkt;
	pjturn_permission *perm;
	pj_ssize_t len;

	/* For UDP check the packet length */
	if (alloc->listener->tp_type == PJTURN_TP_UDP) {
	    if (pkt->len < pj_ntohs(cd->length)+sizeof(*cd)) {
		PJ_LOG(4,(alloc->obj_name, 
			  "ChannelData from %s discarded: UDP size error",
			  alloc->info));
		return;
	    }
	} else {
	    pj_assert(!"Unsupported transport");
	    return;
	}

	perm = lookup_permission_by_chnum(alloc, pj_ntohs(cd->ch_number));
	if (!perm) {
	    /* Discard */
	    PJ_LOG(4,(alloc->obj_name, 
		      "ChannelData from %s discarded: not found",
		      alloc->info));
	    return;
	}

	/* Relay the data */
	len = pj_ntohs(cd->length);
	pj_sock_sendto(alloc->relay.tp.sock, cd+1, &len, 0,
		       &perm->hkey.peer_addr,
		       pj_sockaddr_get_len(&perm->hkey.peer_addr));

	/* Refresh permission */
	refresh_permission(perm);
    }
}

/*
 * Handle incoming packet from peer. This function is called by 
 * on_rx_from_peer().
 */
static void on_rx_peer_pkt(pjturn_allocation *alloc,
			   pjturn_relay_res *rel,
			   char *pkt, pj_size_t len,
			   const pj_sockaddr *src_addr)
{
    pjturn_permission *perm;

    /* Lookup permission */
    perm = lookup_permission_by_addr(alloc, src_addr, 
				     pj_sockaddr_get_len(src_addr));
    if (perm == NULL) {
	/* No permission, discard data */
	return;
    }

    /* Send Data Indication or ChannelData, depends on whether
     * this permission is attached to a channel number.
     */
    if (perm->channel != PJTURN_INVALID_CHANNEL) {
	/* Send ChannelData */
	channel_data_hdr *cd = (channel_data_hdr*)rel->tp.tx_pkt;

	if (len > PJTURN_MAX_PKT_LEN) {
	    char peer_addr[80];
	    pj_sockaddr_print(src_addr, peer_addr, sizeof(peer_addr), 3);
	    PJ_LOG(1,(alloc->obj_name, "Client %s: discarded data from %s "
		      "because it's too long (%d bytes)",
		      alloc->info, peer_addr, len));
	    return;
	}

	/* Init header */
	cd->ch_number = pj_htons(perm->channel);
	cd->length = pj_htons((pj_uint16_t)len);

	/* Copy data */
	pj_memcpy(rel->tp.rx_pkt+sizeof(channel_data_hdr), pkt, len);

	/* Send to client */
	pjturn_listener_sendto(alloc->listener, rel->tp.tx_pkt,
			       len+sizeof(channel_data_hdr), 0,
			       &alloc->hkey.clt_addr,
			       pj_sockaddr_get_len(&alloc->hkey.clt_addr));
    } else {
	/* Send Data Indication */
	pj_stun_tx_data *tdata;
	pj_status_t status;

	status = pj_stun_session_create_ind(alloc->sess, 
					    PJ_STUN_DATA_INDICATION, &tdata);
	if (status != PJ_SUCCESS) {
	    alloc_err(alloc, "Error creating Data indication", status);
	    return;
	}
    }
}

/*
 * ioqueue notification on RX packets from the relay socket.
 */
static void on_rx_from_peer(pj_ioqueue_key_t *key, 
                            pj_ioqueue_op_key_t *op_key, 
                            pj_ssize_t bytes_read)
{
    pjturn_relay_res *rel;
    pj_status_t status;

    rel = (pjturn_relay_res*) pj_ioqueue_get_user_data(key);

    do {
	if (bytes_read > 0) {
	    on_rx_peer_pkt(rel->allocation, rel, rel->tp.rx_pkt,
			   bytes_read, &rel->tp.src_addr);
	}

	/* Read next packet */
	bytes_read = sizeof(rel->tp.rx_pkt);
	rel->tp.src_addr_len = sizeof(rel->tp.src_addr);
	status = pj_ioqueue_recvfrom(key, op_key,
				     rel->tp.rx_pkt, &bytes_read, 0,
				     &rel->tp.src_addr, 
				     &rel->tp.src_addr_len);

	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    bytes_read = -status;

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED);

}

/*
 * Callback notification from STUN session when it wants to send
 * a STUN message towards the client.
 */
static pj_status_t stun_on_send_msg(pj_stun_session *sess,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len)
{
    pjturn_allocation *alloc;

    alloc = (pjturn_allocation*) pj_stun_session_get_user_data(sess);

    return pjturn_listener_sendto(alloc->listener, pkt, pkt_size, 0,
				  dst_addr, addr_len);
}

/*
 * Callback notification from STUN session when it receives STUN
 * requests. This callback was trigger by STUN incoming message
 * processing in pjturn_allocation_on_rx_client_pkt().
 */
static pj_status_t stun_on_rx_request(pj_stun_session *sess,
				      const pj_uint8_t *pkt,
				      unsigned pkt_len,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len)
{
    pjturn_allocation *alloc;

    alloc = (pjturn_allocation*) pj_stun_session_get_user_data(sess);

    /* Refuse to serve any request if we've been shutdown */
    if (alloc->relay.lifetime == 0) {
	send_reply_err(alloc, msg, PJ_TRUE, 
		       PJ_STUN_SC_ALLOCATION_MISMATCH, NULL);
	return PJ_SUCCESS;
    }

    if (msg->hdr.type == PJ_STUN_REFRESH_REQUEST) {
	/* 
	 * Handle REFRESH request 
	 */
	pj_stun_lifetime_attr *lifetime;
	pj_stun_bandwidth_attr *bandwidth;

	/* Get LIFETIME attribute */
	lifetime = (pj_stun_lifetime_attr*)
		   pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_LIFETIME, 0);

	/* Get BANDWIDTH attribute */
	bandwidth = (pj_stun_bandwidth_attr*)
	            pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_BANDWIDTH, 0);

	if (lifetime && lifetime->value==0) {
	    /*
	     * This is deallocation request.
	     */
	    alloc->relay.lifetime = 0;

	    /* Respond first */
	    send_reply_ok(alloc, msg);

	    /* Shutdown allocation */
	    PJ_LOG(4,(alloc->obj_name, 
		      "Client %s request to dealloc, shutting down",
		      alloc->info));

	    alloc_shutdown(alloc);

	} else {
	    /*
	     * This is a refresh request.
	     */
	    
	    /* Update lifetime */
	    if (lifetime) {
		alloc->relay.lifetime = lifetime->value;
	    }

	    /* Update bandwidth */
	    // TODO:

	    /* Update expiration timer */
	    resched_timeout(alloc);

	    /* Send reply */
	    send_reply_ok(alloc, msg);
	}

    } else if (msg->hdr.type == PJ_STUN_CHANNEL_BIND_REQUEST) {
	/*
	 * ChannelBind request.
	 */
	pj_stun_channel_number_attr *ch_attr;
	pj_stun_peer_addr_attr *peer_attr;
	pjturn_permission *p1, *p2;

	ch_attr = (pj_stun_channel_number_attr*)
		  pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_CHANNEL_NUMBER, 0);
	peer_attr = (pj_stun_peer_addr_attr*)
		    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_PEER_ADDR, 0);

	if (!ch_attr || !peer_attr) {
	    send_reply_err(alloc, msg, PJ_TRUE, PJ_STUN_SC_BAD_REQUEST, NULL);
	    return PJ_SUCCESS;
	}

	/* Find permission with the channel number */
	p1 = lookup_permission_by_chnum(alloc, PJ_STUN_GET_CH_NB(ch_attr->value));

	/* If permission is found, this is supposed to be a channel bind 
	 * refresh. Make sure it's for the same peer.
	 */
	if (p1) {
	    if (pj_sockaddr_cmp(&p1->hkey.peer_addr, &peer_attr->sockaddr)) {
		/* Address mismatch. Send 400 */
		send_reply_err(alloc, msg, PJ_TRUE, 
			       PJ_STUN_SC_BAD_REQUEST, 
			       "Peer address mismatch");
		return PJ_SUCCESS;
	    }

	    /* Refresh permission */
	    refresh_permission(p1);

	    /* Done */
	    return PJ_SUCCESS;
	}

	/* If permission is not found, create a new one. Make sure the peer
	 * has not alreadyy assigned with a channel number.
	 */
	p2 = lookup_permission_by_addr(alloc, &peer_attr->sockaddr,
				       pj_sockaddr_get_len(&peer_attr->sockaddr));
	if (p2 && p2->channel != PJTURN_INVALID_CHANNEL) {
	    send_reply_err(alloc, msg, PJ_TRUE, PJ_STUN_SC_BAD_REQUEST, 
			   "Peer address already assigned a channel number");
	    return PJ_SUCCESS;
	}

	/* Create permission if it doesn't exist */
	if (!p2) {
	    p2 = create_permission(alloc, &peer_attr->sockaddr,
				   pj_sockaddr_get_len(&peer_attr->sockaddr));
	    if (!p2)
		return PJ_SUCCESS;
	}

	/* Assign channel number to permission */
	p2->channel = PJ_STUN_GET_CH_NB(ch_attr->value);

	/* Update */
	refresh_permission(p2);

	/* Reply */
	send_reply_ok(alloc, msg);

	return PJ_SUCCESS;

    } else if (msg->hdr.type == PJ_STUN_ALLOCATE_REQUEST) {

	/* Respond with 437 (section 6.3 turn-07) */
	send_reply_err(alloc, msg, PJ_TRUE, PJ_STUN_SC_ALLOCATION_MISMATCH, NULL);

    } else {

	/* Respond with Bad Request? */
	send_reply_err(alloc, msg, PJ_TRUE, PJ_STUN_SC_BAD_REQUEST, NULL);

    }

    return PJ_SUCCESS;
}

/*
 * Callback notification from STUN session when it receives STUN
 * indications. This callback was trigger by STUN incoming message
 * processing in pjturn_allocation_on_rx_client_pkt().
 */
static pj_status_t stun_on_rx_indication(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    pj_stun_peer_addr_attr *peer_attr;
    pj_stun_data_attr *data_attr;
    pjturn_allocation *alloc;
    pjturn_permission *perm;

    alloc = (pjturn_allocation*) pj_stun_session_get_user_data(sess);

    /* Only expect Send Indication */
    if (msg->hdr.type != PJ_STUN_SEND_INDICATION) {
	/* Ignore */
	return PJ_SUCCESS;
    }

    /* Get PEER-ADDRESS attribute */
    peer_attr = (pj_stun_peer_addr_attr*)
		pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_PEER_ADDR, 0);

    /* MUST have PEER-ADDRESS attribute */
    if (!peer_attr)
	return PJ_SUCCESS;

    /* Get DATA attribute */
    data_attr = (pj_stun_data_attr*)
		pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_DATA, 0);

    /* Create/update/refresh the permission */
    perm = lookup_permission_by_addr(alloc, &peer_attr->sockaddr,
				     pj_sockaddr_get_len(&peer_attr->sockaddr));
    if (perm == NULL) {
	perm = create_permission(alloc, &peer_attr->sockaddr,
				 pj_sockaddr_get_len(&peer_attr->sockaddr));
    }
    refresh_permission(perm);

    /* Return if we don't have data */
    if (data_attr == NULL)
	return PJ_SUCCESS;

    /* Relay the data to client */
    if (alloc->hkey.tp_type == PJTURN_TP_UDP) {
	pj_ssize_t len = data_attr->length;
	pj_sock_sendto(alloc->listener->sock, data_attr->data, 
		       &len, 0, &peer_attr->sockaddr,
		       pj_sockaddr_get_len(&peer_attr->sockaddr));
    } else {
	pj_assert(!"TCP is not supported");
    }

    return PJ_SUCCESS;
}



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
#include "server.h"

#define THIS_FILE   "turn_usage.c"

#define MAX_CLIENTS		8000
#define MAX_PEER_PER_CLIENT	16
#define START_PORT		2000
#define END_PORT		65530

/*
 * Forward declarations.
 */
struct turn_usage;
struct turn_client;

static void	   tu_on_rx_data(pj_stun_usage *usage,
				 void *pkt,
				 pj_size_t pkt_size,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len);
static void	   tu_on_destroy(pj_stun_usage *usage);
static pj_status_t tu_sess_on_send_msg(pj_stun_session *sess,
				       const void *pkt,
				       pj_size_t pkt_size,
				       const pj_sockaddr_t *dst_addr,
				       unsigned addr_len);
static pj_status_t tu_sess_on_rx_request(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len);

static pj_status_t handle_binding_req(pj_stun_session *session,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len);

static pj_status_t client_create(struct turn_usage *tu,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len,
				 struct turn_client **p_client);
static pj_status_t client_destroy(struct turn_client *client,
				  pj_status_t reason);
static pj_status_t client_handle_stun_msg(struct turn_client *client,
					  const pj_stun_msg *msg,
					  const pj_sockaddr_t *src_addr,
					  unsigned src_addr_len);


struct turn_usage
{
    pj_pool_factory	*pf;
    pj_stun_config	*cfg;
    pj_ioqueue_t	*ioqueue;
    pj_timer_heap_t	*timer_heap;
    pj_pool_t		*pool;
    pj_mutex_t		*mutex;
    pj_stun_usage	*usage;
    int			 type;
    pj_stun_session	*default_session;
    pj_hash_table_t	*client_htable;
    pj_stun_auth_cred	*cred;
    pj_bool_t		 use_fingerprint;

    unsigned		 max_bw_kbps;
    unsigned		 max_lifetime;

    unsigned		 next_port;
};

struct peer;

struct turn_client
{
    char		 obj_name[PJ_MAX_OBJ_NAME];
    struct turn_usage	*tu;
    pj_pool_t		*pool;
    pj_stun_session	*session;
    pj_mutex_t		*mutex;

    pj_sockaddr_in	 client_src_addr;

    /* Socket and socket address of the allocated port */
    int			 sock_type;
    pj_sock_t		 sock;
    pj_ioqueue_key_t	*key;
    pj_sockaddr_in	 alloc_addr;

    /* Allocation properties */
    unsigned		 bw_kbps;
    unsigned		 lifetime;
    pj_timer_entry	 expiry_timer;


    /* Hash table to keep all peers, key-ed by their address */
    pj_hash_table_t	*peer_htable;

    /* Active destination, or sin_addr.s_addr will be zero if
     * no active destination is set.
     */
    struct peer		*active_peer;

    /* Current packet received/sent from/to the allocated port */
    pj_uint8_t		 pkt[4000];
    pj_sockaddr_in	 pkt_src_addr;
    int			 pkt_src_addr_len;
    pj_ioqueue_op_key_t	 pkt_read_key;
    pj_ioqueue_op_key_t  pkt_write_key;
};

struct peer
{
    struct turn_client   *client;
    pj_sockaddr_in	  addr;
};

struct session_data
{
    struct turn_usage	*tu;
    struct turn_client	*client;
};


/*
 * This is the only public API, to create and start the TURN usage.
 */
PJ_DEF(pj_status_t) pj_stun_turn_usage_create(pj_stun_server *srv,
					      int type,
					      const pj_str_t *ip_addr,
					      unsigned port,
					      pj_bool_t use_fingerprint,
					      pj_stun_usage **p_bu)
{
    pj_pool_t *pool;
    struct turn_usage *tu;
    pj_stun_server_info *si;
    pj_stun_usage_cb usage_cb;
    pj_stun_session_cb sess_cb;
    struct session_data *sd;
    pj_sockaddr_in local_addr;
    pj_status_t status;

    PJ_ASSERT_RETURN(srv && (type==PJ_SOCK_DGRAM||type==PJ_SOCK_STREAM),
		     PJ_EINVAL);
    si = pj_stun_server_get_info(srv);

    pool = pj_pool_create(si->pf, "turn%p", 4000, 4000, NULL);
    tu = PJ_POOL_ZALLOC_T(pool, struct turn_usage);
    tu->pool = pool;
    tu->type = type;
    tu->pf = si->pf;
    tu->cfg = &si->stun_cfg;
    tu->ioqueue = si->ioqueue;
    tu->timer_heap = si->timer_heap;
    tu->next_port = START_PORT;
    tu->max_bw_kbps = 64;
    tu->max_lifetime = 10 * 60;
    tu->use_fingerprint = use_fingerprint;

    status = pj_sockaddr_in_init(&local_addr, ip_addr, (pj_uint16_t)port);
    if (status != PJ_SUCCESS)
	return status;

    /* Create usage */
    pj_bzero(&usage_cb, sizeof(usage_cb));
    usage_cb.on_rx_data = &tu_on_rx_data;
    usage_cb.on_destroy = &tu_on_destroy;
    status = pj_stun_usage_create(srv, "turn%p", &usage_cb,
				  PJ_AF_INET, tu->type, 0,
				  &local_addr, sizeof(local_addr),
				  &tu->usage);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }
    pj_stun_usage_set_user_data(tu->usage, tu);

    /* Init hash tables */
    tu->client_htable = pj_hash_create(tu->pool, MAX_CLIENTS);

    /* Create default session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_msg = &tu_sess_on_send_msg;
    sess_cb.on_rx_request = &tu_sess_on_rx_request;
    status = pj_stun_session_create(&si->stun_cfg, "turns%p", &sess_cb, 
				    use_fingerprint, &tu->default_session);
    if (status != PJ_SUCCESS) {
	pj_stun_usage_destroy(tu->usage);
	return status;
    }

    sd = PJ_POOL_ZALLOC_T(pool, struct session_data);
    sd->tu = tu;
    pj_stun_session_set_user_data(tu->default_session, sd);

    pj_stun_session_set_server_name(tu->default_session, NULL);

    /* Create mutex */
    status = pj_mutex_create_recursive(pool, "turn%p", &tu->mutex);
    if (status != PJ_SUCCESS) {
	pj_stun_usage_destroy(tu->usage);
	return status;
    }

    if (p_bu) {
	*p_bu = tu->usage;
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_turn_usage_set_credential(pj_stun_usage *turn,
						      const pj_stun_auth_cred *c)
{
    struct turn_usage *tu;
    tu = (struct turn_usage*) pj_stun_usage_get_user_data(turn);

    tu->cred = PJ_POOL_ZALLOC_T(tu->pool, pj_stun_auth_cred);
    pj_stun_auth_cred_dup(tu->pool, tu->cred, c);
    pj_stun_session_set_credential(tu->default_session, tu->cred);
    return PJ_SUCCESS;
}


/* 
 * This is a callback called by usage.c when the particular STUN usage
 * is to be destroyed.
 */
static void tu_on_destroy(pj_stun_usage *usage)
{
    struct turn_usage *tu;
    pj_hash_iterator_t hit, *it;

    tu = (struct turn_usage*) pj_stun_usage_get_user_data(usage);

    /* Destroy all clients */
    if (tu->client_htable) {
	it = pj_hash_first(tu->client_htable, &hit);
	while (it) {
	    struct turn_client *client;

	    client = (struct turn_client *)pj_hash_this(tu->client_htable, it);
	    client_destroy(client, PJ_SUCCESS);

	    it = pj_hash_first(tu->client_htable, &hit);
	}
    }

    pj_stun_session_destroy(tu->default_session);
    pj_mutex_destroy(tu->mutex);
    pj_pool_release(tu->pool);
}


/*
 * This is a callback called by the usage.c to notify the TURN usage, 
 * that incoming packet (may or may not be a STUN packet) is received
 * on the port where the TURN usage is listening.
 */
static void tu_on_rx_data(pj_stun_usage *usage,
			  void *pkt,
			  pj_size_t pkt_size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len)
{
    struct turn_usage *tu;
    struct turn_client *client;
    unsigned flags;
    pj_status_t status;

    /* Which usage instance is this */
    tu = (struct turn_usage*) pj_stun_usage_get_user_data(usage);

    /* Lookup client structure based on source address */
    client = (struct turn_client*) pj_hash_get(tu->client_htable, src_addr,
					       src_addr_len, NULL);

    /* STUN message decoding flag */
    flags = 0;
    if (tu->type == PJ_SOCK_DGRAM)
	flags |= PJ_STUN_IS_DATAGRAM;
    

    if (client) {
	status = pj_stun_msg_check((const pj_uint8_t*)pkt, pkt_size, flags);

	if (status == PJ_SUCCESS) {
	    if (client->session) {
		/* Received STUN message */
		status = pj_stun_session_on_rx_pkt(client->session, 
						   (pj_uint8_t*)pkt, pkt_size, 
						   flags, NULL, 
						   src_addr, src_addr_len);
	    } else {
		client_destroy(client, PJ_SUCCESS);
	    }
	} else if (client->active_peer) {
	    /* Received non-STUN message and client has active destination */
	    pj_ssize_t sz = pkt_size;
	    pj_ioqueue_sendto(client->key, &client->pkt_write_key,
			      pkt, &sz, 0,
			      &client->active_peer->addr,
			      sizeof(client->active_peer->addr));
	} else {
	    /* Received non-STUN message and client doesn't have active 
	     * destination.
	     */
	    /* Ignore */
	}

    } else {
	/* Received packet (could be STUN or no) from new source */
	flags |= PJ_STUN_CHECK_PACKET;
	pj_stun_session_on_rx_pkt(tu->default_session, (pj_uint8_t*)pkt, 
				  pkt_size, flags, NULL, 
				  src_addr, src_addr_len);
    }
}


/*
 * This is a utility function provided by TU (Turn Usage) to reserve
 * or allocate internal port/socket. The allocation needs to be 
 * coordinated to minimize bind() collissions.
 */
static pj_status_t tu_alloc_port(struct turn_usage *tu, 
				 int type, 
				 unsigned rpp_bits, 
				 const pj_sockaddr_in *req_addr,
				 pj_sock_t *p_sock,
				 int *err_code)
{
    enum { RETRY = 100 };
    pj_sockaddr_in addr;
    pj_sock_t sock = PJ_INVALID_SOCKET;
    unsigned retry;
    pj_status_t status;

    if (req_addr && req_addr->sin_port != 0) {

	*err_code = PJ_STUN_SC_INVALID_PORT;

	/* Allocate specific port */
	status = pj_sock_socket(PJ_AF_INET, type, 0, &sock);
	if (status != PJ_SUCCESS)
	    return status;

	/* Bind */
	status = pj_sock_bind(sock, req_addr, sizeof(pj_sockaddr_in));
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock);
	    return status;
	}

	/* Success */
	*p_sock = sock;
	return PJ_SUCCESS;

    } else {
	status = -1;
	*err_code = PJ_STUN_SC_INSUFFICIENT_CAPACITY;

	if (req_addr && req_addr->sin_addr.s_addr) {
	    *err_code = PJ_STUN_SC_INVALID_IP_ADDR;
	    pj_memcpy(&addr, req_addr, sizeof(pj_sockaddr_in));
	} else {
	    pj_sockaddr_in_init(&addr, NULL, 0);
	}

	for (retry=0; retry<RETRY && sock == PJ_INVALID_SOCKET; ++retry) {
	    switch (rpp_bits) {
	    case 1:
		if ((tu->next_port & 0x01)==0)
		    tu->next_port++;
		break;
	    case 2:
	    case 3:
		if ((tu->next_port & 0x01)==1)
		    tu->next_port++;
		break;
	    }

	    status = pj_sock_socket(PJ_AF_INET, type, 0, &sock);
	    if (status != PJ_SUCCESS)
		return status;

	    addr.sin_port = pj_htons((pj_uint16_t)tu->next_port);

	    if (++tu->next_port > END_PORT)
		tu->next_port = START_PORT;

	    status = pj_sock_bind(sock, &addr, sizeof(addr));
	    if (status != PJ_SUCCESS) {
		pj_sock_close(sock);
		sock = PJ_INVALID_SOCKET;

		/* If client requested specific IP address, assume that
		 * bind failed because the IP address is not valid. We
		 * don't want to retry that since it will exhaust our
		 * port space.
		 */
		if (req_addr && req_addr->sin_addr.s_addr)
		    break;
	    }
	}

	if (sock == PJ_INVALID_SOCKET) {
	    return status;
	}

	*p_sock = sock;
	return PJ_SUCCESS;
    }
}


/* 
 * This callback is called by the TU's STUN session when it receives
 * a valid STUN message. This is called from tu_on_rx_data above.
 */
static pj_status_t tu_sess_on_rx_request(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    struct session_data *sd;
    struct turn_client *client;
    pj_stun_tx_data *tdata;
    pj_status_t status;

    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);

    sd = (struct session_data*) pj_stun_session_get_user_data(sess);

    pj_assert(sd->client == NULL);

    if (msg->hdr.type == PJ_STUN_BINDING_REQUEST) {
	return handle_binding_req(sess, msg, src_addr, src_addr_len);

    } else if (msg->hdr.type != PJ_STUN_ALLOCATE_REQUEST) {
	if (PJ_STUN_IS_REQUEST(msg->hdr.type)) {
	    status = pj_stun_session_create_res(sess, msg, 
						PJ_STUN_SC_NO_BINDING,
						NULL, &tdata);
	    if (status==PJ_SUCCESS) {
		status = pj_stun_session_send_msg(sess, PJ_FALSE, 
						  src_addr, src_addr_len,
						  tdata);
	    }
	} else {
	    PJ_LOG(4,(THIS_FILE, 
		      "Received %s %s without matching Allocation, "
		      "ignored", pj_stun_get_method_name(msg->hdr.type),
		      pj_stun_get_class_name(msg->hdr.type)));
	}
	return PJ_SUCCESS;
    }

    status = client_create(sd->tu, src_addr, src_addr_len, &client);
    if (status != PJ_SUCCESS) {
	pj_stun_perror(THIS_FILE, "Error creating new TURN client", 
		       status);
	return status;
    }


    /* Hand over message to client */
    pj_mutex_lock(client->mutex);
    status = client_handle_stun_msg(client, msg, src_addr, src_addr_len);
    pj_mutex_unlock(client->mutex);

    return status;
}


/*
 * This callback is called by STUN session when it needs to send packet
 * to the network.
 */
static pj_status_t tu_sess_on_send_msg(pj_stun_session *sess,
				       const void *pkt,
				       pj_size_t pkt_size,
				       const pj_sockaddr_t *dst_addr,
				       unsigned addr_len)
{
    struct session_data *sd;

    sd = (struct session_data*) pj_stun_session_get_user_data(sess);

    if (sd->tu->type == PJ_SOCK_DGRAM) {
	return pj_stun_usage_sendto(sd->tu->usage, pkt, pkt_size, 0,
				    dst_addr, addr_len);
    } else {
	return PJ_ENOTSUP;
    }
}


/****************************************************************************/
/*
 * TURN client operations.
 */

/* Function prototypes */
static pj_status_t client_create_relay(struct turn_client *client);
static pj_status_t client_destroy_relay(struct turn_client *client);
static void	   client_on_expired(pj_timer_heap_t *th, pj_timer_entry *e);
static void	   client_on_read_complete(pj_ioqueue_key_t *key, 
					   pj_ioqueue_op_key_t *op_key, 
					   pj_ssize_t bytes_read);
static pj_status_t client_respond(struct turn_client *client, 
				  const pj_stun_msg *msg,
				  int err_code,
				  const char *err_msg,
				  const pj_sockaddr_t *dst_addr,
				  int dst_addr_len);
static struct peer* client_get_peer(struct turn_client *client,
				    const pj_sockaddr_in *peer_addr,
				    pj_uint32_t *hval);
static struct peer* client_add_peer(struct turn_client *client,
				    const pj_sockaddr_in *peer_addr,
				    pj_uint32_t hval);

static const char *get_tp_type(int type)
{
    if (type==PJ_SOCK_DGRAM)
	return "udp";
    else if (type==PJ_SOCK_STREAM)
	return "tcp";
    else
	return "???";
}


/*
 * This callback is called when incoming STUN message is received
 * in the TURN usage. This is called from by tu_on_rx_data() when
 * the packet is handed over to the client.
 */
static pj_status_t client_sess_on_rx_msg(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    struct session_data *sd;

    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);

    sd = (struct session_data*) pj_stun_session_get_user_data(sess);
    pj_assert(sd->client != PJ_SUCCESS);

    return client_handle_stun_msg(sd->client, msg, src_addr, src_addr_len);
}


/*
 * This callback is called by client's STUN session to send outgoing
 * STUN packet. It's called when client calls pj_stun_session_send_msg()
 * function.
 */
static pj_status_t client_sess_on_send_msg(pj_stun_session *sess,
					   const void *pkt,
					   pj_size_t pkt_size,
					   const pj_sockaddr_t *dst_addr,
					   unsigned addr_len)
{
    struct session_data *sd;

    sd = (struct session_data*) pj_stun_session_get_user_data(sess);

    if (sd->tu->type == PJ_SOCK_DGRAM) {
	return pj_stun_usage_sendto(sd->tu->usage, pkt, pkt_size, 0,
				    dst_addr, addr_len);
    } else {
	return PJ_ENOTSUP;
    }
}


/*
 * Create a new TURN client for the specified source address.
 */
static pj_status_t client_create(struct turn_usage *tu,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len,
				 struct turn_client **p_client)
{
    pj_pool_t *pool;
    struct turn_client *client;
    pj_stun_session_cb sess_cb;
    struct session_data *sd;
    pj_status_t status;

    PJ_ASSERT_RETURN(src_addr_len==sizeof(pj_sockaddr_in), PJ_EINVAL);

    pool = pj_pool_create(tu->pf, "turnc%p", 4000, 4000, NULL);
    client = PJ_POOL_ZALLOC_T(pool, struct turn_client);
    client->pool = pool;
    client->tu = tu;
    client->sock = PJ_INVALID_SOCKET;

    pj_memcpy(&client->client_src_addr, src_addr,
	      sizeof(client->client_src_addr));

    if (src_addr) {
	const pj_sockaddr_in *a4 = (const pj_sockaddr_in *)src_addr;
	pj_ansi_snprintf(client->obj_name, sizeof(client->obj_name),
			 "%s:%s:%d",
			 get_tp_type(tu->type),
			 pj_inet_ntoa(a4->sin_addr),
			 (int)pj_ntohs(a4->sin_port));
	client->obj_name[sizeof(client->obj_name)-1] = '\0';
    }

    /* Create session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_msg = &client_sess_on_send_msg;
    sess_cb.on_rx_request = &client_sess_on_rx_msg;
    sess_cb.on_rx_indication = &client_sess_on_rx_msg;
    status = pj_stun_session_create(tu->cfg, client->obj_name, 
				    &sess_cb, tu->use_fingerprint,
				    &client->session);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    if (tu->cred)
	pj_stun_session_set_credential(client->session, tu->cred);

    sd = PJ_POOL_ZALLOC_T(pool, struct session_data);
    sd->tu = tu;
    sd->client = client;
    pj_stun_session_set_user_data(client->session, sd);

    /* Mutex */
    status = pj_mutex_create_recursive(client->pool, pool->obj_name,
				       &client->mutex);
    if (status != PJ_SUCCESS) {
	client_destroy(client, status);
	return status;
    }

    /* Create hash table */
    client->peer_htable = pj_hash_create(client->pool, MAX_PEER_PER_CLIENT);
    if (client->peer_htable == NULL) {
	client_destroy(client, status);
	return PJ_ENOMEM;
    }

    /* Init timer entry */
    client->expiry_timer.user_data = client;
    client->expiry_timer.cb = &client_on_expired;
    client->expiry_timer.id = 0;

    /* Register to hash table */
    pj_mutex_lock(tu->mutex);
    pj_hash_set(pool, tu->client_htable, src_addr, src_addr_len, 0, client);
    pj_mutex_unlock(tu->mutex);

    /* Done */
    *p_client = client;

    PJ_LOG(4,(THIS_FILE, "TURN client %s created", client->obj_name));

    return PJ_SUCCESS;
}


/*
 * Destroy TURN client.
 */
static pj_status_t client_destroy(struct turn_client *client,
				  pj_status_t reason)
{
    struct turn_usage *tu = client->tu;
    char name[PJ_MAX_OBJ_NAME];

    pj_assert(sizeof(name)==sizeof(client->obj_name));
    pj_memcpy(name, client->obj_name, sizeof(name));

    /* Kill timer if it's active */
    if (client->expiry_timer.id != 0) {
	pj_timer_heap_cancel(tu->timer_heap, &client->expiry_timer);
	client->expiry_timer.id = PJ_FALSE;
    }

    /* Destroy relay */
    client_destroy_relay(client);

    /* Unregister client from hash table */
    pj_mutex_lock(tu->mutex);
    pj_hash_set(NULL, tu->client_htable, 
		&client->client_src_addr, sizeof(client->client_src_addr), 
		0, NULL);
    pj_mutex_unlock(tu->mutex);

    /* Destroy STUN session */
    if (client->session) {
	pj_stun_session_destroy(client->session);
	client->session = NULL;
    }

    /* Mutex */
    if (client->mutex) {
	pj_mutex_destroy(client->mutex);
	client->mutex = NULL;
    }

    /* Finally destroy pool */
    if (client->pool) {
	pj_pool_t *pool = client->pool;
	client->pool = NULL;
	pj_pool_release(pool);
    }

    if (reason == PJ_SUCCESS) {
	PJ_LOG(4,(THIS_FILE, "TURN client %s destroyed", name));
    }

    return PJ_SUCCESS;
}


/*
 * This utility function is used to setup relay (with ioqueue) after
 * socket has been allocated for the TURN client.
 */
static pj_status_t client_create_relay(struct turn_client *client)
{
    pj_ioqueue_callback client_ioq_cb;
    int addrlen;
    pj_status_t status;

    /* Update address */
    addrlen = sizeof(pj_sockaddr_in);
    status = pj_sock_getsockname(client->sock, &client->alloc_addr, 
			         &addrlen);
    if (status != PJ_SUCCESS) {
	pj_sock_close(client->sock);
	client->sock = PJ_INVALID_SOCKET;
	return status;
    }

    if (client->alloc_addr.sin_addr.s_addr == 0) {
	status = pj_gethostip(&client->alloc_addr.sin_addr);
	if (status != PJ_SUCCESS) {
	    pj_sock_close(client->sock);
	    client->sock = PJ_INVALID_SOCKET;
	    return status;
	}
    }

    /* Register to ioqueue */
    pj_bzero(&client_ioq_cb, sizeof(client_ioq_cb));
    client_ioq_cb.on_read_complete = &client_on_read_complete;
    status = pj_ioqueue_register_sock(client->pool, client->tu->ioqueue, 
				      client->sock, client,
				      &client_ioq_cb, &client->key);
    if (status != PJ_SUCCESS) {
	pj_sock_close(client->sock);
	client->sock = PJ_INVALID_SOCKET;
	return status;
    }

    pj_ioqueue_op_key_init(&client->pkt_read_key, 
			   sizeof(client->pkt_read_key));
    pj_ioqueue_op_key_init(&client->pkt_write_key, 
			   sizeof(client->pkt_write_key));

    /* Trigger the first read */
    client_on_read_complete(client->key, &client->pkt_read_key, 0);

    PJ_LOG(4,(THIS_FILE, "TURN client %s: relay allocated on %s:%s:%d",
	      client->obj_name,
	      get_tp_type(client->sock_type),
	      pj_inet_ntoa(client->alloc_addr.sin_addr),
	      (int)pj_ntohs(client->alloc_addr.sin_port)));

    return PJ_SUCCESS;
}


/*
 * This utility function is used to destroy the port allocated for
 * the TURN client.
 */
static pj_status_t client_destroy_relay(struct turn_client *client)
{
    /* Close socket */
    if (client->key) {
	pj_ioqueue_unregister(client->key);
	client->key = NULL;
	client->sock = PJ_INVALID_SOCKET;
    } else if (client->sock && client->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(client->sock);
	client->sock = PJ_INVALID_SOCKET;
    }

    PJ_LOG(4,(THIS_FILE, "TURN client %s: relay allocation %s:%s:%d destroyed",
	      client->obj_name,
	      get_tp_type(client->sock_type),
	      pj_inet_ntoa(client->alloc_addr.sin_addr),
	      (int)pj_ntohs(client->alloc_addr.sin_port)));
    return PJ_SUCCESS;
}


/*
 * From the source packet address, get the peer instance from hash table.
 */
static struct peer* client_get_peer(struct turn_client *client,
				    const pj_sockaddr_in *peer_addr,
				    pj_uint32_t *hval)
{
    return (struct peer*)
	pj_hash_get(client->peer_htable, peer_addr, sizeof(*peer_addr), hval);
}


/*
 * Add a peer instance to the peer hash table.
 */
static struct peer* client_add_peer(struct turn_client *client,
				    const pj_sockaddr_in *peer_addr,
				    unsigned hval)
{
    struct peer *peer;

    peer = PJ_POOL_ZALLOC_T(client->pool, struct peer);
    peer->client = client;
    pj_memcpy(&peer->addr, peer_addr, sizeof(peer->addr));

    pj_hash_set(client->pool, client->peer_htable,
		&peer->addr, sizeof(peer->addr), hval, peer);

    PJ_LOG(4,(THIS_FILE, "TURN client %s: peer %s:%s:%d added",
	      client->obj_name, get_tp_type(client->sock_type), 
	      pj_inet_ntoa(peer->addr.sin_addr),
	      (int)pj_ntohs(peer->addr.sin_port)));

    return peer;
}


/*
 * Utility to send STUN response message (normally to send error response).
 */
static pj_status_t client_respond(struct turn_client *client, 
				  const pj_stun_msg *msg,
				  int err_code,
				  const char *custom_msg,
				  const pj_sockaddr_t *dst_addr,
				  int dst_addr_len)
{
    pj_str_t err_msg;
    pj_str_t *p_err_msg = NULL;
    pj_stun_tx_data *response;
    pj_status_t status;

    if (custom_msg)
	pj_cstr(&err_msg, custom_msg), p_err_msg = &err_msg;
    
    status = pj_stun_session_create_res(client->session, msg, 
					err_code, p_err_msg, 
					&response);
    if (status == PJ_SUCCESS)
	status = pj_stun_session_send_msg(client->session, PJ_TRUE,
					  dst_addr, dst_addr_len, response);

    return status;
}


/*
 * Handle incoming initial or subsequent Allocate Request.
 * This function is called by client_handle_stun_msg() below.
 */
static pj_status_t client_handle_allocate_req(struct turn_client *client,
					      const pj_stun_msg *msg,
					      const pj_sockaddr_t *src_addr,
					      unsigned src_addr_len)
{
    const pj_stun_bandwidth_attr *a_bw;
    const pj_stun_lifetime_attr *a_lf;
    const pj_stun_req_port_props_attr *a_rpp;
    const pj_stun_req_transport_attr *a_rt;
    const pj_stun_req_ip_attr *a_rip;
    pj_stun_tx_data *response;
    pj_sockaddr_in req_addr;
    int addr_len;
    unsigned req_bw, rpp_bits;
    pj_time_val timeout;
    pj_status_t status;

    a_bw = (const pj_stun_bandwidth_attr *)
	   pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_BANDWIDTH, 0);
    a_lf = (const pj_stun_lifetime_attr*)
	    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_LIFETIME, 0);
    a_rpp = (const pj_stun_req_port_props_attr*)
	    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REQ_PORT_PROPS, 0);
    a_rt = (const pj_stun_req_transport_attr*)
	   pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REQ_TRANSPORT, 0);
    a_rip = (const pj_stun_req_ip_attr*)
	    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REQ_IP, 0);

    /* Init requested local address */
    pj_sockaddr_in_init(&req_addr, NULL, 0);

    /* Process BANDWIDTH attribute */
    if (a_bw && a_bw->value > client->tu->max_bw_kbps) {
	client_respond(client, msg, PJ_STUN_SC_INSUFFICIENT_CAPACITY, NULL,
		       src_addr, src_addr_len);
	return PJ_SUCCESS;
    } else if (a_bw) {
	client->bw_kbps = req_bw = a_bw->value;
    } else {
	req_bw = 0;
	client->bw_kbps = client->tu->max_bw_kbps;
    }

    /* Process REQUESTED-TRANSPORT attribute */
    if (a_rt && a_rt->value != 0) {
	client_respond(client, msg, PJ_STUN_SC_UNSUPP_TRANSPORT_PROTO, NULL,
		       src_addr, src_addr_len);
	return PJ_SUCCESS;
    } else if (a_rt) {
	client->sock_type = a_rt->value ? PJ_SOCK_STREAM : PJ_SOCK_DGRAM;
    } else {
	client->sock_type = client->tu->type;;
    }

    /* Process REQUESTED-IP attribute */
    if (a_rip && a_rip->sockaddr.addr.sa_family != PJ_AF_INET) {
	client_respond(client, msg, PJ_STUN_SC_INVALID_IP_ADDR, NULL,
		       src_addr, src_addr_len);
	return PJ_SUCCESS;
	
    } else if (a_rip) {
	req_addr.sin_addr.s_addr = a_rip->sockaddr.ipv4.sin_addr.s_addr;
    }

    /* Process REQUESTED-PORT-PROPS attribute */
    if (a_rpp) {
	unsigned port;

	rpp_bits = (a_rpp->value & 0x00030000) >> 16;
	port = (a_rpp->value & 0xFFFF);
	req_addr.sin_port = pj_htons((pj_uint8_t)port);
    } else {
	rpp_bits = 0;
    }

    /* Process LIFETIME attribute */
    if (a_lf && a_lf->value > client->tu->max_lifetime) {
	client->lifetime = client->tu->max_lifetime;
    } else if (a_lf) {
	client->lifetime = a_lf->value;
    } else {
	client->lifetime = client->tu->max_lifetime;
    }

    /* Allocate socket if we don't have one */
    if (client->key == NULL) {
	int err_code;

	PJ_LOG(4,(THIS_FILE, "TURN client %s: received initial Allocate "
			     "request, requested type:addr:port=%s:%s:%d, rpp "
			     "bits=%d, bw=%dkbps, lifetime=%d",
		  client->obj_name, get_tp_type(client->sock_type),
		  pj_inet_ntoa(req_addr.sin_addr), pj_ntohs(req_addr.sin_port),
		  rpp_bits, client->bw_kbps, client->lifetime));

	status = tu_alloc_port(client->tu, client->sock_type, rpp_bits, 
			       &req_addr, &client->sock, &err_code);
	if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(THIS_FILE, "TURN client %s: error allocating relay port"
				 ": %s",
		      client->obj_name, errmsg));

	    client_respond(client, msg, err_code, NULL,
			   src_addr, src_addr_len);

	    return status;
	}

	status = client_create_relay(client);
	if (status != PJ_SUCCESS) {
	    client_respond(client, msg, PJ_STUN_SC_SERVER_ERROR, NULL,
			   src_addr, src_addr_len);
	    return status;
	}
    } else {
	/* Otherwise check if the port parameter stays the same */
	/* TODO */
	PJ_LOG(4,(THIS_FILE, "TURN client %s: received Allocate refresh, "
			     "lifetime=%d",
		  client->obj_name, client->lifetime));
    }

    /* Refresh timer */
    if (client->expiry_timer.id != PJ_FALSE) {
	pj_timer_heap_cancel(client->tu->timer_heap, &client->expiry_timer);
	client->expiry_timer.id = PJ_FALSE;
    }
    timeout.sec = client->lifetime;
    timeout.msec = 0;
    pj_timer_heap_schedule(client->tu->timer_heap, &client->expiry_timer, &timeout);
    client->expiry_timer.id = PJ_TRUE;

    /* Done successfully, create and send success response */
    status = pj_stun_session_create_res(client->session, msg, 
					0, NULL, &response);
    if (status != PJ_SUCCESS) {
	return status;
    }

    pj_stun_msg_add_uint_attr(response->pool, response->msg, 
			      PJ_STUN_ATTR_BANDWIDTH, client->bw_kbps);
    pj_stun_msg_add_uint_attr(response->pool, response->msg,
			      PJ_STUN_ATTR_LIFETIME, client->lifetime);
    pj_stun_msg_add_sockaddr_attr(response->pool, response->msg,
				 PJ_STUN_ATTR_MAPPED_ADDR, PJ_FALSE,
				 src_addr, src_addr_len);
    pj_stun_msg_add_sockaddr_attr(response->pool, response->msg,
				 PJ_STUN_ATTR_XOR_MAPPED_ADDR, PJ_TRUE,
				 src_addr, src_addr_len);

    addr_len = sizeof(req_addr);
    pj_sock_getsockname(client->sock, &req_addr, &addr_len);
    pj_stun_msg_add_sockaddr_attr(response->pool, response->msg,
				 PJ_STUN_ATTR_RELAY_ADDR, PJ_FALSE,
				 &client->alloc_addr, addr_len);

    PJ_LOG(4,(THIS_FILE, "TURN client %s: relay allocated or refreshed, "
			 "internal address is %s:%s:%d",
			 client->obj_name,
			 get_tp_type(client->sock_type),
			 pj_inet_ntoa(req_addr.sin_addr),
			 (int)pj_ntohs(req_addr.sin_port)));

    return pj_stun_session_send_msg(client->session, PJ_TRUE, 
				    src_addr, src_addr_len, response);
}


/*
 * Handle incoming Binding request.
 * This function is called by client_handle_stun_msg() below.
 */
static pj_status_t handle_binding_req(pj_stun_session *session,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* Create response */
    status = pj_stun_session_create_res(session, msg, 0, NULL, 
					&tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create MAPPED-ADDRESS attribute */
    pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
				 PJ_STUN_ATTR_MAPPED_ADDR,
				PJ_FALSE,
				src_addr, src_addr_len);

    /* On the presence of magic, create XOR-MAPPED-ADDRESS attribute */
    if (msg->hdr.magic == PJ_STUN_MAGIC) {
	status = 
	    pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
					 PJ_STUN_ATTR_XOR_MAPPED_ADDR,
					 PJ_TRUE,
					 src_addr, src_addr_len);
    }

    /* Send */
    status = pj_stun_session_send_msg(session, PJ_TRUE, 
				      src_addr, src_addr_len, tdata);
    return status;
}


/* 
 * client handling incoming STUN Set Active Destination request 
 * This function is called by client_handle_stun_msg() below.
 */
static pj_status_t client_handle_sad(struct turn_client *client,
				     const pj_stun_msg *msg,
				     const pj_sockaddr_t *src_addr,
				     unsigned src_addr_len)
{
    pj_stun_remote_addr_attr *a_raddr;

    a_raddr = (pj_stun_remote_addr_attr*)
	      pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REMOTE_ADDR, 0);
    if (!a_raddr) {
	/* Remote active destination needs to be cleared */
	client->active_peer = NULL;

    } else if (a_raddr->sockaddr.addr.sa_family != PJ_AF_INET) {
	/* Bad request (not IPv4) */
	client_respond(client, msg, PJ_STUN_SC_BAD_REQUEST, NULL,
		       src_addr, src_addr_len);
	return PJ_SUCCESS;

    } else if (client->active_peer) {
	/* Client tries to set new active destination without clearing
	 * it first. Reject with 439.
	 */
	client_respond(client, msg, PJ_STUN_SC_TRANSITIONING, NULL,
		       src_addr, src_addr_len);
	return PJ_SUCCESS;

    } else {
	struct peer *peer;
	pj_uint32_t hval = 0;

	/* Add a new peer/permission if we don't have one for this address */
	peer = client_get_peer(client, &a_raddr->sockaddr.ipv4, &hval);
	if (peer==NULL) {
	    peer = client_add_peer(client, &a_raddr->sockaddr.ipv4, hval);
	}

	/* Set active destination */
	client->active_peer = peer;
    }

    if (client->active_peer) {
	PJ_LOG(4,(THIS_FILE, 
		  "TURN client %s: active destination set to %s:%d",
		  client->obj_name,
		  pj_inet_ntoa(client->active_peer->addr.sin_addr),
		  (int)pj_ntohs(client->active_peer->addr.sin_port)));
    } else {
	PJ_LOG(4,(THIS_FILE, "TURN client %s: active destination cleared",
		  client->obj_name));
    }

    /* Respond with successful response */
    client_respond(client, msg, 0, NULL, src_addr, src_addr_len);

    return PJ_SUCCESS;
}


/* 
 * client handling incoming STUN Send Indication 
 * This function is called by client_handle_stun_msg() below.
 */
static pj_status_t client_handle_send_ind(struct turn_client *client,
					  const pj_stun_msg *msg)
{
    pj_stun_remote_addr_attr *a_raddr;
    pj_stun_data_attr *a_data;
    pj_uint32_t hval = 0;
    const pj_uint8_t *data;
    pj_ssize_t datalen;

    /* Get REMOTE-ADDRESS attribute */
    a_raddr = (pj_stun_remote_addr_attr*)
	      pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_REMOTE_ADDR, 0);
    if (!a_raddr) {
	/* REMOTE-ADDRESS not present, discard packet */
	return PJ_SUCCESS;

    } else if (a_raddr->sockaddr.addr.sa_family != PJ_AF_INET) {
	/* REMOTE-ADDRESS present but not IPv4, discard packet */
	return PJ_SUCCESS;

    }

    /* Get the DATA attribute */
    a_data = (pj_stun_data_attr*)
	     pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_DATA, 0);
    if (a_data) {
	data = (const pj_uint8_t *)a_data->data;
	datalen = a_data->length;

    } else if (client->sock_type == PJ_SOCK_STREAM) {
	/* Discard if no Data and Allocation type is TCP */
	return PJ_SUCCESS;

    } else {
	data = (const pj_uint8_t *)"";
	datalen = 0;
    }

    /* Add to peer table if necessary */
    if (client_get_peer(client, &a_raddr->sockaddr.ipv4, &hval)==NULL)
	client_add_peer(client, &a_raddr->sockaddr.ipv4, hval);

    /* Send the packet */
    pj_ioqueue_sendto(client->key, &client->pkt_write_key, 
		      data, &datalen, 0,
		      &a_raddr->sockaddr.ipv4, sizeof(pj_sockaddr_in));

    return PJ_SUCCESS;
}


/* 
 * client handling unknown incoming STUN message.
 * This function is called by client_handle_stun_msg() below.
 */
static pj_status_t client_handle_unknown_msg(struct turn_client *client,
					     const pj_stun_msg *msg,
					     const pj_sockaddr_t *src_addr,
					     unsigned src_addr_len)
{
    PJ_LOG(4,(THIS_FILE, "TURN client %s: unhandled %s %s",
	      client->obj_name, pj_stun_get_method_name(msg->hdr.type),
	      pj_stun_get_class_name(msg->hdr.type)));

    if (PJ_STUN_IS_REQUEST(msg->hdr.type)) {
	return client_respond(client, msg, PJ_STUN_SC_BAD_REQUEST, NULL,
			      src_addr, src_addr_len);
    } else {
	/* Ignore */
	return PJ_SUCCESS;
    }
}


/* 
 * Main entry for handling STUN messages arriving on the main TURN port, 
 * for this client 
 */
static pj_status_t client_handle_stun_msg(struct turn_client *client,
					  const pj_stun_msg *msg,
					  const pj_sockaddr_t *src_addr,
					  unsigned src_addr_len)
{
    pj_status_t status;

    switch (msg->hdr.type) {
    case PJ_STUN_SEND_INDICATION:
	status = client_handle_send_ind(client, msg);
	break;

    case PJ_STUN_SET_ACTIVE_DESTINATION_REQUEST:
	status = client_handle_sad(client, msg,
				   src_addr, src_addr_len);
	break;

    case PJ_STUN_ALLOCATE_REQUEST:
	status = client_handle_allocate_req(client, msg,
					    src_addr, src_addr_len);
	break;

    case PJ_STUN_BINDING_REQUEST:
	status = handle_binding_req(client->session, msg,
				    src_addr, src_addr_len);
	break;

    default:
	status = client_handle_unknown_msg(client, msg,
					   src_addr, src_addr_len);
	break;
    }

    return status;
}


PJ_INLINE(pj_uint32_t) GET_VAL32(const pj_uint8_t *pdu, unsigned pos)
{
    return (pdu[pos+0] << 24) +
	   (pdu[pos+1] << 16) +
	   (pdu[pos+2] <<  8) +
	   (pdu[pos+3]);
}


/* 
 * Handle incoming data from peer 
 * This function is called by client_on_read_complete() below.
 */
static void client_handle_peer_data(struct turn_client *client,
				    unsigned bytes_read)
{
    struct peer *peer;
    pj_bool_t has_magic_cookie;
    pj_status_t status;

    /* Has the sender been registered as peer? */
    peer = client_get_peer(client, &client->pkt_src_addr, NULL);
    if (peer == NULL) {
	/* Nope. Discard packet */
	PJ_LOG(5,(THIS_FILE, 
		 "TURN client %s: discarded data from %s:%d",
		 client->obj_name,
		 pj_inet_ntoa(client->pkt_src_addr.sin_addr),
		 (int)pj_ntohs(client->pkt_src_addr.sin_port)));
	return;
    }

    /* Check if packet has STUN magic cookie */
    has_magic_cookie = (GET_VAL32(client->pkt, 4) == PJ_STUN_MAGIC);

    /* If this is the Active Destination and the packet doesn't have
     * STUN magic cookie, send the packet to client as is.
     */
    if (peer == client->active_peer && !has_magic_cookie) {
	pj_stun_usage_sendto(client->tu->usage, client->pkt, bytes_read, 0,
			     &client->client_src_addr, sizeof(pj_sockaddr_in));
    } else {
	/* Otherwise wrap in Data Indication */
	pj_stun_tx_data *data_ind;

	status = pj_stun_session_create_ind(client->session, 
					    PJ_STUN_DATA_INDICATION,
					    &data_ind);
	if (status != PJ_SUCCESS)
	    return;

	pj_stun_msg_add_sockaddr_attr(data_ind->pool, data_ind->msg, 
				      PJ_STUN_ATTR_REMOTE_ADDR, PJ_FALSE,
				      &client->pkt_src_addr,
				      client->pkt_src_addr_len);
	pj_stun_msg_add_binary_attr(data_ind->pool, data_ind->msg,
				    PJ_STUN_ATTR_DATA, 
				    client->pkt, bytes_read);


	pj_stun_session_send_msg(client->session, PJ_FALSE,
				 &client->client_src_addr, 
				 sizeof(pj_sockaddr_in),
				 data_ind);
    }
}


/* 
 * This callback is called by the ioqueue when read operation has
 * completed on the allocated relay port.
 */
static void client_on_read_complete(pj_ioqueue_key_t *key, 
				    pj_ioqueue_op_key_t *op_key, 
				    pj_ssize_t bytes_read)
{
    enum { MAX_LOOP = 10 };
    struct turn_client *client;
    unsigned count;
    pj_status_t status;

    PJ_UNUSED_ARG(op_key);

    client = (struct turn_client*) pj_ioqueue_get_user_data(key);
    
    /* Lock client */
    pj_mutex_lock(client->mutex);

    for (count=0; ; ++count) {
	unsigned flags;

	if (bytes_read > 0) {
	    /* Received data from peer! */
	    client_handle_peer_data(client, bytes_read);

	} else if (bytes_read < 0) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(-bytes_read, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(THIS_FILE, "TURN client %s: error reading data "
				 "from allocated relay port: %s",
				 client->obj_name, errmsg));
	}

	bytes_read = sizeof(client->pkt);
	flags = (count >= MAX_LOOP) ? PJ_IOQUEUE_ALWAYS_ASYNC : 0;
	client->pkt_src_addr_len = sizeof(client->pkt_src_addr);
	status = pj_ioqueue_recvfrom(client->key, 
				     &client->pkt_read_key,
				     client->pkt, &bytes_read, flags,
				     &client->pkt_src_addr, 
				     &client->pkt_src_addr_len);
	if (status == PJ_EPENDING)
	    break;
    }

    /* Unlock client */
    pj_mutex_unlock(client->mutex);
}


/* On Allocation timer timeout (i.e. we don't receive new Allocate request
 * to refresh the allocation in time)
 */
static void client_on_expired(pj_timer_heap_t *th, pj_timer_entry *e)
{
    struct turn_client *client;

    PJ_UNUSED_ARG(th);

    client = (struct turn_client*) e->user_data;

    PJ_LOG(4,(THIS_FILE, "TURN client %s: allocation timer timeout, "
			 "destroying client",
			 client->obj_name));
    client_destroy(client, PJ_SUCCESS);
}


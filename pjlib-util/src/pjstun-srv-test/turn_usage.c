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

#define THIS_FILE   "turn_usage.c"

#define MAX_CLIENTS		8000
#define MAX_PEER_PER_CLIENTS	16
#define START_PORT		2000
#define END_PORT		65530

static void tu_on_rx_data(pj_stun_usage *usage,
			  void *pkt,
			  pj_size_t pkt_size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len);
static void tu_on_destroy(pj_stun_usage *usage);

static pj_status_t sess_on_send_msg(pj_stun_session *sess,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len);
static pj_status_t sess_on_rx_request(pj_stun_session *sess,
				      const pj_uint8_t *pkt,
				      unsigned pkt_len,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len);


struct turn_usage
{
    pj_pool_factory	*pf;
    pj_stun_endpoint	*endpt;
    pj_ioqueue_t	*ioqueue;
    pj_pool_t		*pool;
    pj_stun_usage	*usage;
    int			 type;
    pj_stun_session	*default_session;
    pj_hash_table_t	*client_htable;
    pj_hash_table_t	*peer_htable;

    unsigned		 max_bw_kbps;
    unsigned		 max_lifetime;

    unsigned		 next_port;
};

struct peer;

struct turn_client
{
    struct turn_usage	*tu;
    pj_pool_t		*pool;
    pj_sockaddr_in	 addr;
    pj_stun_session	*session;

    unsigned		 bw_kbps;
    unsigned		 lifetime;

    pj_sock_t		 sock;
    pj_ioqueue_key_t	*key;
    char		 packet[4000];
    pj_sockaddr_in	 src_addr;
    int			 src_addr_len;
    pj_ioqueue_op_key_t	 read_key;
    pj_ioqueue_op_key_t  write_key;

    pj_bool_t		 has_ad;
    pj_bool_t		 ad_is_pending;
    pj_sockaddr_in	 ad;
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


PJ_DEF(pj_status_t) pj_stun_turn_usage_create(pj_stun_server *srv,
					      int type,
					      const pj_str_t *ip_addr,
					      unsigned port,
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

    PJ_ASSERT_RETURN(srv && (type==PJ_SOCK_DGRAM||type==PJ_SOCK_STREAM) &&
		     p_bu, PJ_EINVAL);
    si = pj_stun_server_get_info(srv);

    pool = pj_pool_create(si->pf, "turn%p", 4000, 4000, NULL);
    tu = PJ_POOL_ZALLOC_T(pool, struct turn_usage);
    tu->pool = pool;
    tu->type = type;
    tu->pf = si->pf;
    tu->endpt = si->endpt;
    tu->next_port = START_PORT;

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
    tu->peer_htable = pj_hash_create(tu->pool, MAX_CLIENTS);

    /* Create default session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_msg = &sess_on_send_msg;
    sess_cb.on_rx_request = &sess_on_rx_request;
    status = pj_stun_session_create(si->endpt, "turns%p", &sess_cb, PJ_FALSE,
				    &tu->default_session);
    if (status != PJ_SUCCESS) {
	pj_stun_usage_destroy(tu->usage);
	return status;
    }

    sd = PJ_POOL_ZALLOC_T(pool, struct session_data);
    sd->tu = tu;
    pj_stun_session_set_user_data(tu->default_session, sd);

    *p_bu = tu->usage;

    return PJ_SUCCESS;
}


static void tu_on_destroy(pj_stun_usage *usage)
{
    struct turn_usage *tu;

    tu = (struct turn_usage*) pj_stun_usage_get_user_data(usage);
    PJ_TODO(DESTROY_ALL_CLIENTS);
    pj_pool_release(tu->pool);
}


static void tu_on_rx_data(pj_stun_usage *usage,
			  void *pkt,
			  pj_size_t pkt_size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len)
{
    struct turn_usage *tu;
    pj_stun_session *session;
    struct turn_client *client;
    unsigned flags;
    pj_status_t status;

    /* Which usage instance is this */
    tu = (struct turn_usage*) pj_stun_usage_get_user_data(usage);

    /* Lookup client structure based on source address */
    client = (struct turn_client*) pj_hash_get(tu->client_htable, src_addr,
					       src_addr_len, NULL);

    if (client == NULL) {
	session = tu->default_session;
    } else {
	session = client->session;
    }
    
    /* Handle packet to session */
    flags = PJ_STUN_CHECK_PACKET;
    if (tu->type == PJ_SOCK_DGRAM)
	flags |= PJ_STUN_IS_DATAGRAM;

    status = pj_stun_session_on_rx_pkt(session, (pj_uint8_t*)pkt, pkt_size,
				       flags, NULL, src_addr, src_addr_len);
    if (status != PJ_SUCCESS) {
	pj_stun_perror(THIS_FILE, "Error handling incoming packet", status);
	return;
    }
}


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

	*err_code = PJ_STUN_STATUS_INVALID_PORT;

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
	*err_code = PJ_STUN_STATUS_INSUFFICIENT_CAPACITY;

	if (req_addr && req_addr->sin_addr.s_addr) {
	    *err_code = PJ_STUN_STATUS_INVALID_IP_ADDR;
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

	return PJ_SUCCESS;
    }
}

/****************************************************************************/

static pj_status_t client_create(struct turn_usage *tu,
				 const pj_stun_msg *msg,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len,
				 struct turn_client **p_client)
{
    pj_pool_t *pool;
    struct turn_client *client;
    pj_stun_session_cb sess_cb;
    struct session_data *sd;
    pj_status_t status;

    PJ_UNUSED_ARG(msg);

    pool = pj_pool_create(tu->pf, "turnc%p", 4000, 4000, NULL);
    client = PJ_POOL_ZALLOC_T(pool, struct turn_client);
    client->pool = pool;
    client->tu = tu;
    client->sock = PJ_INVALID_SOCKET;

    /* Create session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_msg = &sess_on_send_msg;
    sess_cb.on_rx_request = &sess_on_rx_request;
    status = pj_stun_session_create(tu->endpt, "turnc%p", &sess_cb, PJ_FALSE,
				    &client->session);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    sd = PJ_POOL_ZALLOC_T(pool, struct session_data);
    sd->tu = tu;
    sd->client = client;
    pj_stun_session_set_user_data(client->session, sd);

    /* Register to hash table */
    pj_hash_set(pool, tu->client_htable, src_addr, src_addr_len, 0, client);

    *p_client = client;
    return PJ_SUCCESS;
}

static pj_status_t client_destroy(struct turn_client *client)
{
}

static void client_on_read_complete(pj_ioqueue_key_t *key, 
				    pj_ioqueue_op_key_t *op_key, 
				    pj_ssize_t bytes_read)
{
}

static void client_on_write_complete(pj_ioqueue_key_t *key, 
				     pj_ioqueue_op_key_t *op_key, 
				     pj_ssize_t bytes_sent)
{
}

static pj_status_t client_create_relay(struct turn_client *client)
{
    pj_ioqueue_callback client_ioq_cb;
    pj_status_t status;

    /* Register to ioqueue */
    pj_bzero(&client_ioq_cb, sizeof(client_ioq_cb));
    client_ioq_cb.on_read_complete = &client_on_read_complete;
    client_ioq_cb.on_write_complete = &client_on_write_complete;
    status = pj_ioqueue_register_sock(client->pool, client->tu->ioqueue, 
				      client->sock, client,
				      &client_ioq_cb, &client->key);
    if (status != PJ_SUCCESS) {
	pj_sock_close(client->sock);
	client->sock = PJ_INVALID_SOCKET;
	return status;
    }

    pj_ioqueue_op_key_init(&client->read_key, sizeof(client->read_key));
    pj_ioqueue_op_key_init(&client->write_key, sizeof(client->write_key));

    /* Trigger the first read */
    client_on_read_complete(client->key, &client->read_key, 0);

    return PJ_SUCCESS;
}

static pj_status_t client_handle_allocate_req(struct turn_client *client,
					      const pj_uint8_t *pkt,
					      unsigned pkt_len,
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
    unsigned type;
    unsigned rpp_bits;
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
	status = pj_stun_session_create_response(client->session, msg, 
						 PJ_STUN_STATUS_INSUFFICIENT_CAPACITY, 
						 NULL, &response);
	if (status == PJ_SUCCESS && response) {
	    pj_stun_session_send_msg(client->session, PJ_TRUE, 
				     src_addr, src_addr_len, response);
	}
	return -1;
    } else if (a_bw) {
	client->bw_kbps = a_bw->value;
    } else {
	client->bw_kbps = client->tu->max_bw_kbps;
    }

    /* Process REQUESTED-TRANSPORT attribute */
    if (a_rt && a_rt->value != 0) {
	status = pj_stun_session_create_response(client->session, msg, 
						 PJ_STUN_STATUS_UNSUPP_TRANSPORT_PROTO, 
						 NULL, &response);
	if (status == PJ_SUCCESS && response) {
	    pj_stun_session_send_msg(client->session, PJ_TRUE, 
				     src_addr, src_addr_len, response);
	}
	return -1;
    } else if (a_rt) {
	type = a_rt->value ? PJ_SOCK_STREAM : PJ_SOCK_DGRAM;
    } else {
	type = client->tu->type;;
    }

    /* Process REQUESTED-IP attribute */
    if (a_rip && a_rip->addr.addr.sa_family != PJ_AF_INET) {
	status = pj_stun_session_create_response(client->session, msg, 
						 PJ_STUN_STATUS_INVALID_IP_ADDR, 
						 NULL, &response);
	if (status == PJ_SUCCESS && response) {
	    pj_stun_session_send_msg(client->session, PJ_TRUE, 
				     src_addr, src_addr_len, response);
	}
	return -1;
	
    } else if (a_rip) {
	req_addr.sin_addr.s_addr = a_rip->addr.ipv4.sin_addr.s_addr;
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
    if (client->sock == PJ_INVALID_SOCKET) {
	int err_code;

	status = tu_alloc_port(client->tu, type, rpp_bits, &req_addr, 
			       &client->sock, &err_code);
	if (status != PJ_SUCCESS) {

	    status = pj_stun_session_create_response(client->session, msg, 
						     err_code, NULL, 
						     &response);
	    if (status == PJ_SUCCESS && response) {
		pj_stun_session_send_msg(client->session, PJ_TRUE, 
					 src_addr, src_addr_len, response);
	    }
	    return -1;
	}

	status = client_create_relay(client);
	if (status != PJ_SUCCESS) {
	    status = pj_stun_session_create_response(client->session, msg, 
						     PJ_STUN_STATUS_SERVER_ERROR, 
						     NULL, &response);
	    if (status == PJ_SUCCESS && response) {
		pj_stun_session_send_msg(client->session, PJ_TRUE, 
					 src_addr, src_addr_len, response);
	    }
	    return -1;
	}
    } else {
	/* Otherwise check if the port parameter stays the same */
	/* TODO */
    }

    /* Done successfully, create and send success response */
    status = pj_stun_session_create_response(client->session, msg, 
					     0, NULL, &response);
    if (status != PJ_SUCCESS) {
	return -1;
    }

    pj_stun_msg_add_uint_attr(response->pool, response->msg, 
			      PJ_STUN_ATTR_BANDWIDTH, client->bw_kbps);
    pj_stun_msg_add_uint_attr(response->pool, response->msg,
			      PJ_STUN_ATTR_LIFETIME, client->lifetime);
    pj_stun_msg_add_ip_addr_attr(response->pool, response->msg,
				 PJ_STUN_ATTR_MAPPED_ADDR, PJ_FALSE,
				 src_addr, src_addr_len);
    pj_stun_msg_add_ip_addr_attr(response->pool, response->msg,
				 PJ_STUN_ATTR_XOR_MAPPED_ADDR, PJ_TRUE,
				 src_addr, src_addr_len);

    addr_len = sizeof(req_addr);
    pj_sock_getsockname(client->sock, &req_addr, &addr_len);
    pj_stun_msg_add_ip_addr_attr(response->pool, response->msg,
				 PJ_STUN_ATTR_RELAY_ADDR, PJ_FALSE,
				 &req_addr, addr_len);

    return pj_stun_session_send_msg(client->session, PJ_TRUE, 
				    src_addr, src_addr_len, response);
}

static pj_status_t client_handle_send_ind(struct turn_client *client,
					  const pj_uint8_t *pkt,
					  unsigned pkt_len,
					  const pj_stun_msg *msg,
					  const pj_sockaddr_t *src_addr,
					  unsigned src_addr_len)
{
}

static pj_status_t client_handle_unknown_msg(struct turn_client *client,
					     const pj_uint8_t *pkt,
					     unsigned pkt_len,
					     const pj_stun_msg *msg,
					     const pj_sockaddr_t *src_addr,
					     unsigned src_addr_len)
{
}

static pj_status_t client_handle_stun_msg(struct turn_client *client,
					  const pj_uint8_t *pkt,
					  unsigned pkt_len,
					  const pj_stun_msg *msg,
					  const pj_sockaddr_t *src_addr,
					  unsigned src_addr_len)
{
    pj_status_t status;

    switch (msg->hdr.type) {
    case PJ_STUN_ALLOCATE_REQUEST:
	return client_handle_allocate_req(client, pkt, pkt_len, msg,
					  src_addr, src_addr_len);

    case PJ_STUN_SEND_INDICATION:
	return client_handle_send_ind(client, pkt, pkt_len, msg,
				      src_addr, src_addr_len);

    default:
	return client_handle_unknown_msg(client, pkt, pkt_len, msg,
					 src_addr, src_addr_len);
    }
}


static pj_status_t sess_on_rx_request(pj_stun_session *sess,
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

    sd = (struct session_data*) pj_stun_session_get_user_data(sess);

    if (sd->client == NULL) {
	/* No client is associated with this source address. Create a new
	 * one if this is an Allocate request.
	 */
	if (msg->hdr.type != PJ_STUN_ALLOCATE_REQUEST) {
	    PJ_LOG(4,(THIS_FILE, "Received first packet not Allocate request"));
	    return PJ_SUCCESS;
	}

	PJ_TODO(SUPPORT_MOVE);

	status = client_create(sd->tu, msg, src_addr, src_addr_len, &client);
	if (status != PJ_SUCCESS) {
	    pj_stun_perror(THIS_FILE, "Error creating new TURN client", status);
	    return status;
	}

    } else {
	client = sd->client;
    }

    return client_handle_stun_msg(client, pkt, pkt_len, msg, 
				  src_addr, src_addr_len);
}

static pj_status_t sess_on_send_msg(pj_stun_session *sess,
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




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
#include <pjnath/turn_session.h>
#include <pjlib-util/srv_resolver.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/sock.h>


enum state_t
{
    STATE_NULL,
    STATE_RESOLVING,
    STATE_RESOLVED,
    STATE_ALLOCATING,
    STATE_READY
};

struct peer
{
    unsigned	    ch_id;
    pj_sockaddr	    peer_addr;
    pj_time_val	    expiry;
    pj_uint8_t	    tsx_id[12];	/* Pending ChannelBind request */
};

struct pj_turn_session
{
    pj_pool_t		*pool;
    const char		*obj_name;
    pj_turn_session_cb	 cb;

    enum state_t	 state;

    pj_stun_session	*stun;

    pj_dns_async_query	*dns_async;

    unsigned		 srv_addr_cnt;
    pj_sockaddr		*srv_addr_list;
    pj_sockaddr		*srv_addr;

    pj_bool_t		 pending_alloc;
    pj_turn_alloc_param	 alloc_param;

    /* tx_pkt must be 16bit aligned */
    pj_uint8_t		 tx_pkt[PJ_TURN_MAX_PKT_LEN];

    pj_uint16_t		 next_ch;
};


/*
 * Prototypes.
 */
static pj_status_t stun_on_send_msg(pj_stun_session *sess,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len);
static void stun_on_request_complete(pj_stun_session *sess,
				     pj_status_t status,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response,
				     const pj_sockaddr_t *src_addr,
				     unsigned src_addr_len);
static pj_status_t stun_on_rx_indication(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len);
static void dns_srv_resolver_cb(void *user_data,
				pj_status_t status,
				const pj_dns_srv_record *rec);
static void dns_a_resolver_cb(void *user_data,
			      pj_status_t status,
			      pj_dns_parsed_packet *response);
static struct peer *lookup_peer_by_addr(pj_turn_session *sess,
					const pj_sockaddr_t *addr,
					unsigned addr_len,
					pj_bool_t update);
static struct peer *lookup_peer_by_chnum(pj_turn_session *sess,
					 unsigned chnum);


/*
 * Create TURN client session.
 */
PJ_DEF(pj_status_t) pj_turn_session_create( pj_stun_config *cfg,
					    const pj_turn_session_cb *cb,
					    pj_turn_session **p_sess)
{
    pj_pool_t *pool;
    pj_turn_session *sess;
    pj_stun_session_cb stun_cb;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && cfg->pf && cb && p_sess, PJ_EINVAL);

    /* Allocate and create TURN session */
    pool = pj_pool_create(cfg->pf, "turn%p", 1000, 1000, NULL);
    sess = PJ_POOL_ZALLOC_T(pool, pj_turn_session);
    sess->pool = pool;
    sess->obj_name = pool->obj_name;

    pj_memcpy(&sess->cb, cb, sizeof(*cb));

    /* Create STUN session */
    pj_bzero(&stun_cb, sizeof(stun_cb));
    stun_cb.on_send_msg = &stun_on_send_msg;
    stun_cb.on_request_complete = &stun_on_request_complete;
    stun_cb.on_rx_indication = &stun_on_rx_indication;
    status = pj_stun_session_create(cfg, sess->obj_name, &stun_cb, PJ_FALSE,
				    &sess->stun);
    if (status != PJ_SUCCESS) {
	pj_turn_session_destroy(sess);
	return status;
    }

    /* Done for now */
    *p_sess = sess;
    return PJ_SUCCESS;
}


/*
 * Destroy TURN client session.
 */
PJ_DEF(pj_status_t) pj_turn_session_destroy(pj_turn_session *sess)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    /* TODO */
}


/*
 * Notify application and destroy the TURN session.
 */
static void destroy(pj_turn_session *sess,
		    pj_bool_t notify,
		    pj_status_t status)
{
}


/**
 * Set the server or domain name of the server.
 */
PJ_DEF(pj_status_t) pj_turn_session_set_server( pj_turn_session *sess,
					        const pj_str_t *domain,
					        const pj_str_t *res_name,
						int default_port,
						pj_dns_resolver *resolver)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && domain, PJ_EINVAL);

    if (res_name) {
	/* res_name is specified, resolve with DNS SRV resolution.
	 * Resolver must be specified in this case.
	 */
	PJ_ASSERT_RETURN(resolver, PJ_EINVAL);
    
	sess->state = STATE_RESOLVING;
	status = pj_dns_srv_resolve(domain, res_name, default_port, sess->pool,
				    resolver, PJ_DNS_SRV_FALLBACK_A, sess, 
				    &dns_srv_resolver_cb, &sess->dns_async);
	if (status != PJ_SUCCESS) {
	    sess->state = STATE_NULL;
	    return status;
	}

    } else if (resolver) {
	/* res_name is not specified, but resolver is specified.
	 * Resolve domain as a hostname with DNS A resolution.
	 */
	sess->state = STATE_RESOLVING;
	status = pj_dns_resolver_start_query(resolver, domain, PJ_DNS_TYPE_A,
					     0, &dns_a_resolver_cb,
					     sess, &sess->dns_async);
	if (status != PJ_SUCCESS) {
	    sess->state = STATE_NULL;
	    return status;
	}

    } else {
	/* Both res_name and resolver is not specified.
	 * Resolve with standard gethostbyname()
	 */
	pj_addrinfo ai[3];
	unsigned i, cnt = PJ_ARRAY_SIZE(ai);

	status = pj_getaddrinfo(pj_AF_INET(), domain, &cnt, ai);
	if (status != PJ_SUCCESS)
	    return status;

	sess->srv_addr_cnt = cnt;
	sess->srv_addr_list = (pj_sockaddr*)
		              pj_pool_calloc(sess->pool, cnt, 
					     sizeof(pj_sockaddr));
	for (i=0; i<cnt; ++i) {
	    pj_memcpy(&sess->srv_addr_list[i], &ai[i].ai_addr, 
		      sizeof(pj_sockaddr));
	}

	sess->srv_addr = &sess->srv_addr_list[0];
	sess->state = STATE_RESOLVED;
    }

    return PJ_SUCCESS;
}


/**
 * Set credential to be used by the session.
 */
PJ_DEF(pj_status_t) pj_turn_session_set_cred(pj_turn_session *sess,
					     const pj_stun_auth_cred *cred)
{
    PJ_ASSERT_RETURN(sess && cred, PJ_EINVAL);
    pj_stun_session_set_credential(sess->stun, cred);
    return PJ_SUCCESS;
}


/**
 * Create TURN allocation.
 */
PJ_DEF(pj_status_t) pj_turn_session_alloc(pj_turn_session *sess,
					  const pj_turn_alloc_param *param)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess, PJ_EINVAL);
    PJ_ASSERT_RETURN(sess->state <= STATE_RESOLVED, PJ_EINVALIDOP);

    if (sess->state < STATE_RESOLVED) {
	if (param)
	    pj_memcpy(&sess->alloc_param, param, sizeof(*param));
	sess->pending_alloc = PJ_TRUE;
	return PJ_SUCCESS;

    }

    /* Ready to allocate */
    pj_assert(sess->state == STATE_RESOLVED);
    
    /* Create a bare request */
    status = pj_stun_session_create_req(sess->stun, PJ_STUN_ALLOCATE_REQUEST,
					PJ_STUN_MAGIC, NULL, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* MUST include REQUESTED-TRANSPORT attribute */
    pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg,
			      PJ_STUN_ATTR_REQ_TRANSPORT, 
			      PJ_STUN_SET_RT_PROTO(PJ_TURN_TP_UDP));

    /* Include BANDWIDTH if requested */
    if (sess->alloc_param.bandwidth > 0) {
	pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg,
				  PJ_STUN_ATTR_BANDWIDTH,
				  sess->alloc_param.bandwidth);
    }

    /* Include LIFETIME if requested */
    if (sess->alloc_param.lifetime > 0) {
	pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg,
				  PJ_STUN_ATTR_LIFETIME,
				  sess->alloc_param.lifetime);
    }

    /* Select server address */
    pj_assert(sess->srv_addr != NULL);

    /* Send request */
    sess->state = STATE_ALLOCATING;
    status = pj_stun_session_send_msg(sess->stun, PJ_FALSE, sess->srv_addr,
				      pj_sockaddr_get_len(sess->srv_addr), 
				      tdata);
    if (status != PJ_SUCCESS) {
	sess->state = STATE_RESOLVED;
    }

    return status;
}


/**
 * Relay data to the specified peer through the session.
 */
PJ_DEF(pj_status_t) pj_turn_session_sendto( pj_turn_session *sess,
					    const pj_uint8_t *pkt,
					    unsigned pkt_len,
					    const pj_sockaddr_t *peer_addr,
					    unsigned addr_len)
{
    struct peer *peer;

    PJ_ASSERT_RETURN(sess && pkt && pkt_len && peer_addr && addr_len, 
		     PJ_EINVAL);

    /* Return error if we're not ready */
    if (sess->state != STATE_READY) {
	return PJ_EIGNORED;
    }

    /* Lookup peer to see whether we've assigned a channel number
     * to this peer.
     */
    peer = lookup_peer_by_addr(sess, peer_addr, addr_len, PJ_TRUE);
    pj_assert(peer != NULL);

    if (peer->ch_id != PJ_TURN_INVALID_CHANNEL) {
	/* Peer is assigned Channel number, we can use ChannelData */
	pj_turn_channel_data *cd = (pj_turn_channel_data*)sess->tx_pkt;
	
	pj_assert(sizeof(*cd)==4);

	if (pkt_len > sizeof(sess->tx_pkt)-sizeof(*cd))
	    return PJ_ETOOBIG;

	cd->ch_number = pj_htons((pj_uint16_t)peer->ch_id);
	cd->length = pj_htons((pj_uint16_t)pkt_len);
	pj_memcpy(cd+1, pkt, pkt_len);

	pj_assert(sess->srv_addr != NULL);

	return sess->cb.on_send_pkt(sess, sess->tx_pkt, pkt_len+sizeof(*cd),
				    sess->srv_addr,
				    pj_sockaddr_get_len(sess->srv_addr));

    } else {
	/* Peer has not been assigned Channel number, must use Send
	 * Indication.
	 */
	pj_stun_tx_data *tdata;
	pj_status_t status;

	/* Create blank SEND-INDICATION */
	status = pj_stun_session_create_ind(sess->stun, 
					    PJ_STUN_SEND_INDICATION, &tdata);
	if (status != PJ_SUCCESS)
	    return status;

	/* Add PEER-ADDRESS */
	pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
				      PJ_STUN_ATTR_PEER_ADDR, PJ_TRUE,
				      peer_addr, addr_len);

	/* Add DATA attribute */
	pj_stun_msg_add_binary_attr(tdata->pool, tdata->msg,
				    PJ_STUN_ATTR_DATA, pkt, pkt_len);

	/* Send the indication */
	return pj_stun_session_send_msg(sess->stun, PJ_FALSE, sess->srv_addr,
					pj_sockaddr_get_len(sess->srv_addr),
					tdata);
    }
}


/**
 * Bind a peer address to a channel number.
 */
PJ_DEF(pj_status_t) pj_turn_session_bind_channel(pj_turn_session *sess,
						 const pj_sockaddr_t *peer_adr,
						 unsigned addr_len)
{
    struct peer *peer;
    pj_stun_tx_data *tdata;
    unsigned ch_num;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && peer && addr_len, PJ_EINVAL);

    /* Create blank ChannelBind request */
    status = pj_stun_session_create_req(sess->stun, 
					PJ_STUN_CHANNEL_BIND_REQUEST,
					PJ_STUN_MAGIC, NULL, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Lookup peer */
    peer = lookup_peer_by_addr(sess, peer_adr, addr_len, PJ_TRUE);
    pj_assert(peer);

    if (peer->ch_id != PJ_TURN_INVALID_CHANNEL) {
	ch_num = peer->ch_id;
    } else {
	PJ_ASSERT_RETURN(sess->next_ch <= PJ_TURN_CHANNEL_MAX, PJ_ETOOMANY);
	ch_num = sess->next_ch++;
    }

    /* Add CHANNEL-NUMBER attribute */
    pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg,
			      PJ_STUN_ATTR_CHANNEL_NUMBER,
			      PJ_STUN_SET_CH_NB(sess->next_ch));

    /* Add PEER-ADDRESS attribute */
    pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
				  PJ_STUN_ATTR_PEER_ADDR, PJ_TRUE,
				  peer_adr, addr_len);

    /* Save transaction ID to peer */
    pj_memcpy(peer->tsx_id, tdata->msg->hdr.tsx_id, sizeof(peer->tsx_id));

    /* Send the request */
    return pj_stun_session_send_msg(sess->stun, PJ_FALSE, sess->srv_addr,
				    pj_sockaddr_get_len(sess->srv_addr),
				    tdata);
}


/**
 * Notify TURN client session upon receiving a packet from server.
 * The packet maybe a STUN packet or ChannelData packet.
 */
PJ_DEF(pj_status_t) pj_turn_session_on_rx_pkt(pj_turn_session *sess,
					      const pj_uint8_t *pkt,
					      unsigned pkt_len,
					      pj_bool_t is_datagram)
{
    pj_bool_t is_stun;

    /* Packet could be ChannelData or STUN message (response or
     * indication).
     */
    /* Quickly check if this is STUN message */
    is_stun = ((pkt[0] & 0xC0) == 0);

    if (is_stun) {
	/* This looks like STUN, give it to the STUN session */
	unsigned options;

	options = PJ_STUN_CHECK_PACKET;
	if (is_datagram)
	    options |= PJ_STUN_IS_DATAGRAM;
	return pj_stun_session_on_rx_pkt(sess->stun, pkt, pkt_len,
					 options, NULL,
					 sess->srv_addr,
					 pj_sockaddr_get_len(sess->srv_addr));
    } else {
	/* This must be ChannelData */
	pj_turn_channel_data cd;
	struct peer *peer;

	/* Lookup peer */
	pj_memcpy(&cd, pkt, sizeof(pj_turn_channel_data));
	peer = lookup_peer_by_chnum(sess, pj_ntohs(cd.ch_number));
	if (!peer)
	    return PJ_ENOTFOUND;

	/* Notify application */
	if (sess->cb.on_rx_data) {
	    (*sess->cb.on_rx_data)(sess, pkt+sizeof(cd), pj_ntohs(cd.length),
				   &peer->peer_addr,
				   pj_sockaddr_get_len(&peer->peer_addr));
	}

	return PJ_SUCCESS;
    }
}


/*
 * This is a callback from STUN session to send outgoing packet.
 */
static pj_status_t stun_on_send_msg(pj_stun_session *stun,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len)
{
    pj_turn_session *sess;

    sess = (pj_turn_session*) pj_stun_session_get_user_data(stun);
    return (*sess->cb.on_send_pkt)(sess, pkt, pkt_size, 
				   dst_addr, addr_len);
}


/*
 * Notification from STUN session on request completion.
 */
static void stun_on_request_complete(pj_stun_session *stun,
				     pj_status_t status,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response,
				     const pj_sockaddr_t *src_addr,
				     unsigned src_addr_len)
{
    pj_turn_session *sess;
    int method = PJ_STUN_GET_METHOD(response->hdr.type);

    sess = (pj_turn_session*)pj_stun_session_get_user_data(stun);

    if (method == PJ_STUN_ALLOCATE_METHOD) {
	/* Handle ALLOCATE response */
	if (PJ_STUN_IS_SUCCESS_RESPONSE(response->hdr.type)) {
	    /* Successful Allocate response */

	} else {
	    /* Error Allocate response */

	}

    } else if (method == PJ_STUN_CHANNEL_BIND_METHOD) {
	/* Handle ChannelBind response */
	if (PJ_STUN_IS_SUCCESS_RESPONSE(response->hdr.type)) {
	    /* Successful ChannelBind response */

	} else {
	    /* Error ChannelBind response */

	}

    } else {
	PJ_LOG(4,(sess->obj_name, "Unexpected STUN %s response",
		  pj_stun_get_method_name(response->hdr.type)));
    }
}


/*
 * Notification from STUN session on incoming STUN Indication
 * message.
 */
static pj_status_t stun_on_rx_indication(pj_stun_session *stun,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    pj_turn_session *sess;
    pj_stun_peer_addr_attr *peer_attr;
    pj_stun_data_attr *data_attr;

    sess = (pj_turn_session*)pj_stun_session_get_user_data(stun);

    /* Expecting Data Indication only */
    if (msg->hdr.type != PJ_STUN_DATA_INDICATION) {
	PJ_LOG(4,(sess->obj_name, "Unexpected STUN %s indication",
		  pj_stun_get_method_name(msg->hdr.type)));
	return PJ_EINVALIDOP;
    }

    /* Get PEER-ADDRESS attribute */
    peer_attr = (pj_stun_peer_addr_attr*)
		pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_PEER_ADDR, 0);

    /* Get DATA attribute */
    data_attr = (pj_stun_data_attr*)
		pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_DATA, 0);

    /* Must have both PEER-ADDRESS and DATA attributes */
    if (!peer_attr || !data_attr) {
	PJ_LOG(4,(sess->obj_name, 
		  "Received Data indication with missing attributes"));
	return PJ_EINVALIDOP;
    }

    /* Notify application */
    if (sess->cb.on_rx_data) {
	(*sess->cb.on_rx_data)(sess, pkt, pkt_len, 
			       &peer_attr->sockaddr,
			       pj_sockaddr_get_len(&peer_attr->sockaddr));
    }

    return PJ_SUCCESS;
}


/*
 * Notification on completion of DNS SRV resolution.
 */
static void dns_srv_resolver_cb(void *user_data,
				pj_status_t status,
				const pj_dns_srv_record *rec)
{
    pj_turn_session *sess = (pj_turn_session*) user_data;

    /* Check failure */
    if (status != PJ_SUCCESS) {
	destroy(sess, PJ_TRUE, status);
	return;
    }

    /* Copy results to server entries */

    /* Set state to STATE_RESOLVED */

    /* Run pending allocation */
}


/*
 * Notification on completion of DNS A resolution.
 */
static void dns_a_resolver_cb(void *user_data,
			      pj_status_t status,
			      pj_dns_parsed_packet *response)
{
}


/*
 * Lookup peer descriptor from its address.
 */
static struct peer *lookup_peer_by_addr(pj_turn_session *sess,
					const pj_sockaddr_t *addr,
					unsigned addr_len,
					pj_bool_t update)
{
}


/*
 * Lookup peer descriptor from its channel number.
 */
static struct peer *lookup_peer_by_chnum(pj_turn_session *sess,
					 unsigned chnum)
{
}



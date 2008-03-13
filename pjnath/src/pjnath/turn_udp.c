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
#include <pjnath/turn_udp.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/pool.h>
#include <pj/ioqueue.h>

struct pj_turn_udp
{
    pj_pool_t		*pool;
    pj_turn_session	*sess;
    pj_turn_udp_cb	 cb;
    void		*user_data;

    pj_sock_t		 sock;
    pj_ioqueue_key_t	*key;
    pj_ioqueue_op_key_t	 read_key;
    pj_uint8_t		 pkt[PJ_TURN_MAX_PKT_LEN];
    pj_sockaddr		 src_addr;
    int			 src_addr_len;
};


/*
 * Callback prototypes.
 */
static pj_status_t turn_on_send_pkt(pj_turn_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len);
static void turn_on_channel_bound(pj_turn_session *sess,
				  const pj_sockaddr_t *peer_addr,
				  unsigned addr_len,
				  unsigned ch_num);
static void turn_on_rx_data(pj_turn_session *sess,
			    const pj_uint8_t *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static void turn_on_state(pj_turn_session *sess, 
			  pj_turn_state_t old_state,
			  pj_turn_state_t new_state);
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);


/*
 * Create.
 */
PJ_DEF(pj_status_t) pj_turn_udp_create( pj_stun_config *cfg,
					int af,
					const pj_turn_udp_cb *cb,
					unsigned options,
					void *user_data,
					pj_turn_udp **p_udp_rel)
{
    pj_turn_udp *udp_rel;
    pj_turn_session_cb sess_cb;
    pj_ioqueue_callback ioq_cb;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && p_udp_rel, PJ_EINVAL);
    PJ_ASSERT_RETURN(options==0, PJ_EINVAL);

    /* Create and init basic data structure */
    pool = pj_pool_create(cfg->pf, "udprel%p", 1000, 1000, NULL);
    udp_rel = PJ_POOL_ZALLOC_T(pool, pj_turn_udp);
    udp_rel->pool = pool;
    udp_rel->user_data = user_data;

    if (cb) {
	pj_memcpy(&udp_rel->cb, cb, sizeof(*cb));
    }

    /* Init TURN session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_pkt = &turn_on_send_pkt;
    sess_cb.on_channel_bound = &turn_on_channel_bound;
    sess_cb.on_rx_data = &turn_on_rx_data;
    sess_cb.on_state = &turn_on_state;
    status = pj_turn_session_create(cfg, pool->obj_name, af, PJ_TURN_TP_UDP,
				    &sess_cb, udp_rel, 0, &udp_rel->sess);
    if (status != PJ_SUCCESS) {
	pj_turn_udp_destroy(udp_rel);
	return status;
    }

    /* Init socket */
    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &udp_rel->sock);
    if (status != PJ_SUCCESS) {
	pj_turn_udp_destroy(udp_rel);
	return status;
    }

    /* Register to ioqeuue */
    pj_bzero(&ioq_cb, sizeof(ioq_cb));
    ioq_cb.on_read_complete = &on_read_complete;
    status = pj_ioqueue_register_sock(udp_rel->pool, cfg->ioqueue, 
				      udp_rel->sock, udp_rel, 
				      &ioq_cb, &udp_rel->key);
    if (status != PJ_SUCCESS) {
	pj_turn_udp_destroy(udp_rel);
	return status;
    }

    /* Kick start pending read operation */
    pj_ioqueue_op_key_init(&udp_rel->read_key, sizeof(udp_rel->read_key));
    on_read_complete(udp_rel->key, &udp_rel->read_key, 0);

    *p_udp_rel = udp_rel;
    return PJ_SUCCESS;
}

/*
 * Destroy.
 */
PJ_DEF(void) pj_turn_udp_destroy(pj_turn_udp *udp_rel)
{
    if (udp_rel->sess) {
	pj_turn_session_destroy(udp_rel->sess);
	udp_rel->sess = NULL;
    }

    if (udp_rel->pool) {
	pj_pool_t *pool = udp_rel->pool;
	udp_rel->pool = NULL;
	pj_pool_release(pool);
    }
}

/*
 * Set user data.
 */
PJ_DEF(pj_status_t) pj_turn_udp_set_user_data( pj_turn_udp *udp_rel,
					       void *user_data)
{
    udp_rel->user_data = user_data;
    return PJ_SUCCESS;
}

/*
 * Get user data.
 */
PJ_DEF(void*) pj_turn_udp_get_user_data(pj_turn_udp *udp_rel)
{
    return udp_rel->user_data;
}

/**
 * Get info.
 */
PJ_DEF(pj_status_t) pj_turn_udp_get_info(pj_turn_udp *udp_rel,
					 pj_turn_session_info *info)
{
    PJ_ASSERT_RETURN(udp_rel && info, PJ_EINVAL);

    if (udp_rel->sess) {
	return pj_turn_session_get_info(udp_rel->sess, info);
    } else {
	pj_bzero(info, sizeof(*info));
	info->state = PJ_TURN_STATE_NULL;
	return PJ_SUCCESS;
    }
}

/*
 * Initialize.
 */
PJ_DEF(pj_status_t) pj_turn_udp_init( pj_turn_udp *udp_rel,
				      const pj_str_t *domain,
				      int default_port,
				      pj_dns_resolver *resolver,
				      const pj_stun_auth_cred *cred,
				      const pj_turn_alloc_param *param)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(udp_rel && domain, PJ_EINVAL);
    PJ_ASSERT_RETURN(udp_rel->sess, PJ_EINVALIDOP);

    status = pj_turn_session_set_server(udp_rel->sess, domain, default_port,
					resolver);
    if (status != PJ_SUCCESS)
	return status;

    if (cred) {
	status = pj_turn_session_set_cred(udp_rel->sess, cred);
	if (status != PJ_SUCCESS)
	    return status;
    }

    status = pj_turn_session_alloc(udp_rel->sess, param);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}

/*
 * Send packet.
 */ 
PJ_DEF(pj_status_t) pj_turn_udp_sendto( pj_turn_udp *udp_rel,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_sockaddr_t *addr,
					unsigned addr_len)
{
    PJ_ASSERT_RETURN(udp_rel && addr && addr_len, PJ_EINVAL);

    if (udp_rel->sess == NULL)
	return PJ_EINVALIDOP;

    return pj_turn_session_sendto(udp_rel->sess, pkt, pkt_len, 
				  addr, addr_len);
}

/*
 * Bind a peer address to a channel number.
 */
PJ_DEF(pj_status_t) pj_turn_udp_bind_channel( pj_turn_udp *udp_rel,
					      const pj_sockaddr_t *peer,
					      unsigned addr_len)
{
    PJ_ASSERT_RETURN(udp_rel && peer && addr_len, PJ_EINVAL);
    PJ_ASSERT_RETURN(udp_rel->sess != NULL, PJ_EINVALIDOP);

    return pj_turn_session_bind_channel(udp_rel->sess, peer, addr_len);
}


/*
 * Notification from ioqueue when incoming UDP packet is received.
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    pj_turn_udp *udp_rel;
    pj_status_t status;

    udp_rel = (pj_turn_udp*) pj_ioqueue_get_user_data(key);

    do {
	/* Report incoming packet to TURN session */
	if (bytes_read > 0 && udp_rel->sess) {
	    pj_turn_session_on_rx_pkt(udp_rel->sess, udp_rel->pkt, 
				      bytes_read, PJ_TRUE);
	}

	/* Read next packet */
	bytes_read = sizeof(udp_rel->pkt);
	udp_rel->src_addr_len = sizeof(udp_rel->src_addr);

	status = pj_ioqueue_recvfrom(udp_rel->key, op_key,
				     udp_rel->pkt, &bytes_read, 0,
				     &udp_rel->src_addr, 
				     &udp_rel->src_addr_len);

	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    bytes_read = -status;

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED);

}


/*
 * Callback from TURN session to send outgoing packet.
 */
static pj_status_t turn_on_send_pkt(pj_turn_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len)
{
    pj_turn_udp *udp_rel = (pj_turn_udp*) 
			   pj_turn_session_get_user_data(sess);
    pj_ssize_t len = pkt_len;

    return pj_sock_sendto(udp_rel->sock, pkt, &len, 0,
			  dst_addr, dst_addr_len);
}


/*
 * Callback from TURN session when a channel is successfully bound.
 */
static void turn_on_channel_bound(pj_turn_session *sess,
				  const pj_sockaddr_t *peer_addr,
				  unsigned addr_len,
				  unsigned ch_num)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(peer_addr);
    PJ_UNUSED_ARG(addr_len);
    PJ_UNUSED_ARG(ch_num);
}


/*
 * Callback from TURN session upon incoming data.
 */
static void turn_on_rx_data(pj_turn_session *sess,
			    const pj_uint8_t *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_turn_udp *udp_rel = (pj_turn_udp*) 
			   pj_turn_session_get_user_data(sess);
    if (udp_rel->cb.on_rx_data) {
	(*udp_rel->cb.on_rx_data)(udp_rel, pkt, pkt_len, 
				  peer_addr, addr_len);
    }
}


/*
 * Callback from TURN session when state has changed
 */
static void turn_on_state(pj_turn_session *sess, 
			  pj_turn_state_t old_state,
			  pj_turn_state_t new_state)
{
    pj_turn_udp *udp_rel = (pj_turn_udp*) 
			   pj_turn_session_get_user_data(sess);
    if (udp_rel->cb.on_state) {
	(*udp_rel->cb.on_state)(udp_rel, old_state, new_state);
    }

    if (new_state > PJ_TURN_STATE_READY) {
	udp_rel->sess = NULL;
    }
}



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
#include <pjnath/ice_mt.h>
#include <pjnath/errno.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


#define RTP_COMP_ID	1
#define RTCP_COMP_ID	2



/* ICE callbacks */
static void	   on_ice_complete(pj_ice *ice, pj_status_t status);
static pj_status_t on_tx_pkt(pj_ice *ice, unsigned comp_id,
			     const void *pkt, pj_size_t size,
			     const pj_sockaddr_t *dst_addr,
			     unsigned dst_addr_len);
static pj_status_t on_rx_data(pj_ice *ice, unsigned comp_id,
			      void *pkt, pj_size_t size,
			      const pj_sockaddr_t *src_addr,
			      unsigned src_addr_len);

/* Ioqueue callback */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);

static void destroy_ice_sock(pj_icemt_sock *is);

static pj_status_t create_ice_sock(pj_icemt *icemt,
				   pj_ioqueue_t *ioqueue,
				   unsigned comp_id,
				   unsigned port,
				   pj_icemt_sock *is)
{
    pj_ioqueue_callback ioqueue_cb;
    const pj_str_t H1 = { "H1", 2 };
    int addr_len;
    pj_status_t status;

    pj_bzero(is, sizeof(*is));
    is->sock = PJ_INVALID_SOCKET;
    is->comp_id = comp_id;
    is->icemt = icemt;

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &is->sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Bind and get the local IP address */
    pj_sockaddr_in_init(&is->base_addr.ipv4, NULL, (pj_uint16_t)port);
    status = pj_sock_bind(is->sock, &is->base_addr, sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    addr_len = sizeof(is->base_addr);
    status = pj_sock_getsockname(is->sock, &is->base_addr, &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    if (is->base_addr.ipv4.sin_addr.s_addr == 0) {
	status = pj_gethostip(&is->base_addr.ipv4.sin_addr);
	if (status != PJ_SUCCESS) 
	    goto on_error;
    }

    /* Register to ioqueue */
    pj_bzero(&ioqueue_cb, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = &on_read_complete;
    status = pj_ioqueue_register_sock(icemt->pool, ioqueue, is->sock, is,
				      &ioqueue_cb, &is->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_ioqueue_op_key_init(&is->read_op, sizeof(is->read_op));
    pj_ioqueue_op_key_init(&is->write_op, sizeof(is->write_op));

    on_read_complete(is->key, &is->read_op, 0);

    /* Add new ICE component */
    status = pj_ice_add_comp(icemt->ice, comp_id, &is->base_addr,
			     sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Add host candidate */
    status = pj_ice_add_cand(icemt->ice, comp_id, PJ_ICE_CAND_TYPE_HOST,
			     65535, &H1, &is->base_addr, &is->base_addr,
			     NULL, sizeof(pj_sockaddr_in), NULL);
    if (status != PJ_SUCCESS)
	goto on_error;
    
    return PJ_SUCCESS;

on_error:
    destroy_ice_sock(is);
    return status;
}


static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    pj_icemt_sock *is = (pj_icemt_sock*) pj_ioqueue_get_user_data(key);
    pj_ssize_t pkt_size;
    pj_status_t status;

    if (bytes_read > 0) {
	status = pj_ice_on_rx_pkt(is->icemt->ice, is->comp_id, 
				  is->pkt, bytes_read,
				  &is->src_addr, is->src_addr_len);
    }

    pkt_size = sizeof(is->pkt);
    is->src_addr_len = sizeof(is->src_addr);
    status = pj_ioqueue_recvfrom(key, op_key, is->pkt, &pkt_size, 
				 PJ_IOQUEUE_ALWAYS_ASYNC,
				 &is->src_addr, &is->src_addr_len);
    pj_assert(status == PJ_SUCCESS || status == PJ_EPENDING);
}


static void destroy_ice_sock(pj_icemt_sock *is)
{
    if (is->key) {
	pj_ioqueue_unregister(is->key);
	is->key = NULL;
	is->sock = PJ_INVALID_SOCKET;
    } else if (is->sock != PJ_INVALID_SOCKET && is->sock != 0) {
	pj_sock_close(is->sock);
	is->sock = PJ_INVALID_SOCKET;
    }
}


PJ_DEF(pj_status_t) pj_icemt_create( pj_stun_config *stun_cfg,
				     const char *name,
				     pj_ice_role role,
				     const pj_icemt_cb *cb,
				     unsigned rtp_port,
				     pj_bool_t has_rtcp,
				     pj_bool_t has_turn,
				     const pj_sockaddr *srv,
				     pj_icemt **p_icemt)
{
    pj_pool_t *pool;
    pj_icemt *icemt;
    pj_ice_cb ice_cb;
    pj_status_t status;

    pool = pj_pool_create(stun_cfg->pf, name, 512, 512, NULL);
    icemt = PJ_POOL_ZALLOC_T(pool, struct pj_icemt);
    icemt->pool = pool;


    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_ice_complete = &on_ice_complete;
    ice_cb.on_tx_pkt = &on_tx_pkt;
    ice_cb.on_rx_data = &on_rx_data;

    pj_memcpy(&icemt->cb, cb, sizeof(*cb));

    status = pj_ice_create(stun_cfg, name, role, &ice_cb, &icemt->ice);
    if (status != PJ_SUCCESS)
	goto on_error;

    icemt->ice->user_data = (void*)icemt;

    icemt->has_turn = has_turn;
    if (srv)
	pj_memcpy(&icemt->stun_srv, srv, sizeof(pj_sockaddr));

    status = create_ice_sock(icemt, stun_cfg->ioqueue, RTP_COMP_ID,
			     rtp_port, &icemt->rtp);
    if (status != PJ_SUCCESS)
	goto on_error;

    if (has_rtcp) {
	if (rtp_port) ++rtp_port;

	status = create_ice_sock(icemt, stun_cfg->ioqueue, RTCP_COMP_ID,
				 rtp_port, &icemt->rtcp);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    *p_icemt = icemt;
    return PJ_SUCCESS;

on_error:
    if (icemt->ice)
	pj_ice_destroy(icemt->ice);
    pj_pool_release(pool);
    return status;
}


PJ_DEF(pj_status_t) pj_icemt_destroy(pj_icemt *icemt)
{
    destroy_ice_sock(&icemt->rtp);
    destroy_ice_sock(&icemt->rtcp);

    pj_ice_destroy(icemt->ice);
    pj_pool_release(icemt->pool);

    return PJ_SUCCESS;
}


static void on_ice_complete(pj_ice *ice, pj_status_t status)
{
    pj_icemt *icemt = (pj_icemt*)ice->user_data;
    (*icemt->cb.on_ice_complete)(icemt, status);
}


static pj_status_t on_tx_pkt(pj_ice *ice, unsigned comp_id,
			     const void *pkt, pj_size_t size,
			     const pj_sockaddr_t *dst_addr,
			     unsigned dst_addr_len)
{
    pj_icemt *icemt = (pj_icemt*)ice->user_data;
    pj_icemt_sock *is;
    pj_ssize_t pkt_size;
    pj_status_t status;

    if (comp_id == RTP_COMP_ID)
	is = &icemt->rtp;
    else if (comp_id == RTCP_COMP_ID)
	is = &icemt->rtcp;
    else {
	pj_assert(!"Invalid comp_id");
	return -1;
    }

    pkt_size = size;
    status = pj_ioqueue_sendto(is->key, &is->write_op, 
			       pkt, &pkt_size, 0,
			       dst_addr, dst_addr_len);
    
    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? PJ_SUCCESS : status;
}


static pj_status_t on_rx_data(pj_ice *ice, unsigned comp_id,
			      void *pkt, pj_size_t size,
			      const pj_sockaddr_t *src_addr,
			      unsigned src_addr_len)
{
    pj_icemt *icemt = (pj_icemt*)ice->user_data;

    if (comp_id == RTP_COMP_ID && icemt->cb.on_rx_rtp) {
	(*icemt->cb.on_rx_rtp)(icemt, pkt, size, src_addr, src_addr_len);
    } else if (comp_id == RTCP_COMP_ID && icemt->cb.on_rx_rtcp) {
	(*icemt->cb.on_rx_rtcp)(icemt, pkt, size, src_addr, src_addr_len);
    }
    return PJ_SUCCESS;
}



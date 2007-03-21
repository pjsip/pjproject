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

#define THIS_FILE   "bind_usage.c"

static void usage_on_rx_data(pj_stun_usage *usage,
			     void *pkt,
			     pj_size_t pkt_size,
			     const pj_sockaddr_t *src_addr,
			     unsigned src_addr_len);
static void usage_on_destroy(pj_stun_usage *usage);
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

struct bind_usage
{
    pj_pool_t	    *pool;
    pj_stun_usage   *usage;
    pj_stun_session *session;
};


PJ_DEF(pj_status_t) pj_stun_bind_usage_create(pj_stun_server *srv,
					      const pj_str_t *ip_addr,
					      unsigned port,
					      pj_stun_usage **p_bu)
{
    pj_pool_t *pool;
    struct bind_usage *bu;
    pj_stun_server_info *si;
    pj_stun_usage_cb usage_cb;
    pj_stun_session_cb sess_cb;
    pj_sockaddr_in local_addr;
    pj_status_t status;

    si = pj_stun_server_get_info(srv);

    pool = pj_pool_create(si->pf, "bind%p", 128, 128, NULL);
    bu = PJ_POOL_ZALLOC_T(pool, struct bind_usage);
    bu->pool = pool;

    status = pj_sockaddr_in_init(&local_addr, ip_addr, (pj_uint16_t)port);
    if (status != PJ_SUCCESS)
	return status;

    pj_bzero(&usage_cb, sizeof(usage_cb));
    usage_cb.on_rx_data = &usage_on_rx_data;
    usage_cb.on_destroy = &usage_on_destroy;

    status = pj_stun_usage_create(srv, "bind%p", &usage_cb,
				  PJ_AF_INET, PJ_SOCK_DGRAM, 0,
				  &local_addr, sizeof(local_addr),
				  &bu->usage);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_msg = &sess_on_send_msg;
    sess_cb.on_rx_request = &sess_on_rx_request;
    status = pj_stun_session_create(&si->stun_cfg, "bind%p", &sess_cb, 
				    PJ_FALSE, &bu->session);
    if (status != PJ_SUCCESS) {
	pj_stun_usage_destroy(bu->usage);
	return status;
    }

    pj_stun_usage_set_user_data(bu->usage, bu);
    pj_stun_session_set_user_data(bu->session, bu);

    if (p_bu)
	*p_bu = bu->usage;

    return PJ_SUCCESS;
}


static void usage_on_rx_data(pj_stun_usage *usage,
			     void *pkt,
			     pj_size_t pkt_size,
			     const pj_sockaddr_t *src_addr,
			     unsigned src_addr_len)
{
    struct bind_usage *bu;
    pj_stun_session *session;
    pj_status_t status;

    bu = (struct bind_usage*) pj_stun_usage_get_user_data(usage);
    session = bu->session;

    /* Handle packet to session */
    status = pj_stun_session_on_rx_pkt(session, (pj_uint8_t*)pkt, pkt_size,
				       PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET,
				       NULL, src_addr, src_addr_len);
    if (status != PJ_SUCCESS) {
	pj_stun_perror(THIS_FILE, "Error handling incoming packet", status);
	return;
    }
}


static pj_status_t sess_on_send_msg(pj_stun_session *sess,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len)
{
    struct bind_usage *bu;
    pj_stun_usage *usage;

    bu = (struct bind_usage*) pj_stun_session_get_user_data(sess);
    usage = bu->usage;

    return pj_stun_usage_sendto(usage, pkt, pkt_size, 0,
				dst_addr, addr_len);
}


static pj_status_t sess_on_rx_request(pj_stun_session *sess,
				      const pj_uint8_t *pkt,
				      unsigned pkt_len,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);

    /* Create response */
    status = pj_stun_session_create_response(sess, msg, 0, NULL, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create MAPPED-ADDRESS attribute */
    status = pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
				   	   PJ_STUN_ATTR_MAPPED_ADDR,
					   PJ_FALSE,
				           src_addr, src_addr_len);
    if (status != PJ_SUCCESS) {
	pj_stun_perror(THIS_FILE, "Error creating response", status);
	pj_stun_msg_destroy_tdata(sess, tdata);
	return status;
    }

    /* On the presence of magic, create XOR-MAPPED-ADDRESS attribute */
    if (msg->hdr.magic == PJ_STUN_MAGIC) {
	status = 
	    pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg,
					  PJ_STUN_ATTR_XOR_MAPPED_ADDR,
					  PJ_TRUE,
					  src_addr, src_addr_len);
	if (status != PJ_SUCCESS) {
	    pj_stun_perror(THIS_FILE, "Error creating response", status);
	    pj_stun_msg_destroy_tdata(sess, tdata);
	    return status;
	}
    }

    /* Send */
    status = pj_stun_session_send_msg(sess, PJ_TRUE, 
				      src_addr, src_addr_len, tdata);
    return status;

}

static void usage_on_destroy(pj_stun_usage *usage)
{
    struct bind_usage *bu;

    bu = (struct bind_usage*) pj_stun_usage_get_user_data(usage);
    if (bu==NULL)
	return;

    pj_stun_session_destroy(bu->session);
    pj_pool_release(bu->pool);
}

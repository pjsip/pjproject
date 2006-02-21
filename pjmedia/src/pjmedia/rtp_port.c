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
#include <pjmedia/rtp_port.h>
#include <pjmedia/errno.h>
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/sock_select.h>
#include <pj/string.h>


#define RTP_PORT_SIGNATURE	('R'<<2 | 'T'<<1 | 'P')

struct rtp_port
{
    pjmedia_port	base;
    pjmedia_sock_info	sock_info;
    pj_sockaddr_in	rem_rtp_addr;
    pj_sockaddr_in	rem_rctp_addr;
    pjmedia_rtp_session	tx_session;
    pjmedia_rtp_session	rx_session;
    pj_rtcp_session	rtcp;
    char		tx_buf[1500];
    char		rx_buf[1500];
};


static pj_status_t rtp_on_upstream_connect(pj_pool_t *pool,
					   pjmedia_port *this_port,
					   pjmedia_port *upstream);
static pj_status_t rtp_put_frame( pjmedia_port *this_port, 
				  const pjmedia_frame *frame);
static pj_status_t rtp_get_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t rtp_on_destroy( pjmedia_port *this_port );


PJ_DEF(pj_status_t) pjmedia_rtp_port_create( pj_pool_t *pool,
					     pjmedia_sock_info *sock_info,
					     pjmedia_port **p_port)
{
    struct rtp_port *rtp;

    PJ_ASSERT_RETURN(pool && sock_info && p_port,
		     PJ_EINVAL);


    rtp = pj_pool_zalloc(pool, sizeof(struct rtp_port));
    rtp->base.info.name = pj_str("rtp");
    rtp->base.info.signature = RTP_PORT_SIGNATURE;
    rtp->base.info.type = PJMEDIA_TYPE_NONE;
    rtp->base.info.has_info = PJ_FALSE;
    rtp->base.info.need_info = PJ_TRUE;

    rtp->base.on_upstream_connect = &rtp_on_upstream_connect;
    rtp->base.put_frame = &rtp_put_frame;
    rtp->base.get_frame = &rtp_get_frame;
    rtp->base.on_destroy = &rtp_on_destroy;
    
    pj_memcpy(&rtp->sock_info, sock_info, sizeof(pjmedia_sock_info));

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_rtp_port_configure( pjmedia_port *rtp,
						const pj_sockaddr_in *rem_rtp,
						const pj_sockaddr_in *rem_rtcp)
{
    struct rtp_port *rtp_port = (struct rtp_port *)rtp;

    PJ_ASSERT_RETURN(rtp, PJ_EINVAL);

    if (rem_rtp) {
	pj_memcpy(&rtp_port->rem_rtp_addr, rem_rtp, sizeof(pj_sockaddr_in));
    }
    if (rem_rtcp) {
	pj_memcpy(&rtp_port->rem_rctp_addr, rem_rtcp, sizeof(pj_sockaddr_in));
    }

    return PJ_SUCCESS;
}


static pj_status_t rtp_on_upstream_connect(pj_pool_t *pool,
					   pjmedia_port *this_port,
					   pjmedia_port *upstream)
{
    struct rtp_port *rtp_port = (struct rtp_port *)this_port;

    rtp_port->base.info.type = upstream->info.type;
    rtp_port->base.info.pt = upstream->info.pt;
    pj_strdup(pool, &rtp_port->base.info.encoding_name, 
	      &upstream->info.encoding_name);
    rtp_port->base.info.sample_rate = upstream->info.sample_rate;
    rtp_port->base.info.bits_per_sample = upstream->info.bits_per_sample;
    rtp_port->base.info.samples_per_frame = upstream->info.samples_per_frame;
    rtp_port->base.info.bytes_per_frame = upstream->info.bytes_per_frame;

    rtp_port->base.info.has_info = PJ_TRUE;

    pjmedia_rtp_session_init(&rtp_port->tx_session, upstream->info.pt, 10);
    pjmedia_rtp_session_init(&rtp_port->rx_session, upstream->info.pt, 10);
    pj_rtcp_init(&rtp_port->rtcp, 10);

    return PJ_SUCCESS;
}

static pj_status_t rtp_put_frame( pjmedia_port *this_port, 
				  const pjmedia_frame *frame)
{
    struct rtp_port *rtp_port = (struct rtp_port *)this_port;
    void *rtphdr;
    int rtphdrlen;
    pj_ssize_t sent;
    pj_status_t status;

    if (!rtp_port->base.info.has_info)
	return PJMEDIA_RTP_ENOCONFIG;

    if (rtp_port->rem_rtp_addr.sin_family != PJ_AF_INET)
	return PJMEDIA_RTP_EBADDEST;

    status = pjmedia_rtp_encode_rtp(&rtp_port->tx_session, 
				    rtp_port->base.info.pt,
				    0, frame->size,
				    (frame->size / rtp_port->base.info.bytes_per_frame),
				    &rtphdr, &rtphdrlen);
    if (status != PJ_SUCCESS)
	return status;

    if (rtphdrlen != sizeof(pjmedia_rtp_hdr))
	return PJMEDIA_RTP_EINPKT;
    

    /* Scatter send in PJLIB will be nice here..! */
    pj_memcpy(rtp_port->tx_buf, rtphdr, sizeof(pjmedia_rtp_hdr));
    pj_memcpy(rtp_port->tx_buf+sizeof(pjmedia_rtp_hdr), frame->buf, frame->size);

    sent = sizeof(pjmedia_rtp_hdr) + frame->size;
    status = pj_sock_sendto( rtp_port->sock_info.rtp_sock,
			     rtp_port->tx_buf, &sent, 0,
			     &rtp_port->rem_rtp_addr,
			     sizeof(rtp_port->rem_rtp_addr));

    return status;
}


static pj_status_t rtp_get_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    struct rtp_port *rtp_port = (struct rtp_port *)this_port;
    pj_fd_set_t fds;
    pj_time_val timeout = { 0, 0};
    pj_ssize_t pktlen;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;
    pj_status_t status;

    PJ_FD_ZERO(&fds);
    PJ_FD_SET(rtp_port->sock_info.rtp_sock, &fds);

    /* Check for incoming packet. */
    status = pj_sock_select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
    if (status <= 0)
	goto on_error;

    /* Receive packet. */
    pktlen = sizeof(rtp_port->rx_buf);
    status = pj_sock_recv(rtp_port->sock_info.rtp_sock, 
			  rtp_port->rx_buf, &pktlen, 0);
    if (pktlen < 1 || status != PJ_SUCCESS)
	goto on_error;


    /* Update RTP and RTCP session. */
    status = pjmedia_rtp_decode_rtp(&rtp_port->rx_session, 
				    rtp_port->rx_buf, pktlen, 
				    &hdr, &payload, &payloadlen);
    if (status != PJ_SUCCESS)
	goto on_error;
    

    status = pjmedia_rtp_session_update(&rtp_port->rx_session, hdr);
    if (status != 0 && 
	status != PJMEDIA_RTP_ESESSPROBATION && 
	status != PJMEDIA_RTP_ESESSRESTART) 
    {
	goto on_error;
    }
    pj_rtcp_rx_rtp(&rtp_port->rtcp, pj_ntohs(hdr->seq), pj_ntohl(hdr->ts));

    /* Copy */
    if (frame->size > payloadlen) frame->size = payloadlen;
    pj_memcpy(frame->buf, payload, frame->size);
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->timestamp.u64 = pj_ntohl(hdr->ts);

    return PJ_SUCCESS;


on_error:
    frame->type = PJMEDIA_FRAME_TYPE_NONE;
    frame->size = 0;
    return PJ_SUCCESS;
}


static pj_status_t rtp_on_destroy( pjmedia_port *this_port )
{
    PJ_UNUSED_ARG(this_port);
    return PJ_SUCCESS;
}



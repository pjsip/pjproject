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
#include <pjmedia/transport_udp.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/string.h>


/* Maximum size of incoming RTP packet */
#define RTP_LEN	    1500

/* Maximum size of incoming RTCP packet */
#define RTCP_LEN    600


struct transport_udp
{
    pjmedia_transport	base;		/**< Base transport.		    */

    pj_pool_t	       *pool;		/**< Memory pool		    */

    pjmedia_stream     *stream;		/**< Stream user (may be NULL)	    */
    pj_sockaddr_in	rem_rtp_addr;	/**< Remote RTP address		    */
    pj_sockaddr_in	rem_rtcp_addr;	/**< Remote RTCP address	    */
    void  (*rtp_cb)(	pjmedia_stream*,/**< To report incoming RTP.	    */
			const void*,
			pj_ssize_t);
    void  (*rtcp_cb)(	pjmedia_stream*,/**< To report incoming RTCP.	    */
			const void*,
			pj_ssize_t);

    pj_sock_t	        rtp_sock;	/**< RTP socket			    */
    pj_ioqueue_key_t   *rtp_key;	/**< RTP socket key in ioqueue	    */
    pj_ioqueue_op_key_t	rtp_read_op;	/**< Pending read operation	    */
    pj_ioqueue_op_key_t	rtp_write_op;	/**< Pending write operation	    */
    pj_sockaddr_in	rtp_src_addr;	/**< Actual packet src addr.	    */
    unsigned		rtp_src_cnt;	/**< How many pkt from this addr.   */
    int			rtp_addrlen;	/**< Address length.		    */
    char		rtp_pkt[RTP_LEN];/**< Incoming RTP packet buffer    */

    pj_sock_t		rtcp_sock;	/**< RTCP socket		    */
    pj_ioqueue_key_t   *rtcp_key;	/**< RTCP socket key in ioqueue	    */
    pj_ioqueue_op_key_t rtcp_read_op;	/**< Pending read operation	    */
    pj_ioqueue_op_key_t rtcp_write_op;	/**< Pending write operation	    */
    char		rtcp_pkt[RTCP_LEN];/**< Incoming RTCP packet buffer */
};



static void on_rx_rtp( pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read);
static void on_rx_rtcp(pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read);

static pj_status_t transport_attach(   pjmedia_transport *tp,
				       pjmedia_stream *strm,
				       const pj_sockaddr_t *rem_addr,
				       unsigned addr_len,
				       void (*rtp_cb)(pjmedia_stream*,
						      const void*,
						      pj_ssize_t),
				       void (*rtcp_cb)(pjmedia_stream*,
						       const void*,
						       pj_ssize_t));
static void	   transport_detach(   pjmedia_transport *tp,
				       pjmedia_stream *strm);
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size);


static pjmedia_transport_op transport_udp_op = 
{
    &transport_attach,
    &transport_detach,
    &transport_send_rtp,
    &transport_send_rtcp,
    &pjmedia_transport_udp_close
};


/**
 * Create UDP stream transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_create( pjmedia_endpt *endpt,
						  const char *name,
						  int port,
						  pjmedia_transport **p_tp)
{
    pjmedia_sock_info si;
    pj_status_t status;

    
    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && port && p_tp, PJ_EINVAL);


    pj_memset(&si, 0, sizeof(pjmedia_sock_info));
    si.rtp_sock = si.rtcp_sock = PJ_INVALID_SOCKET;

    /* Create RTP socket */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &si.rtp_sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Bind RTP socket */
    si.rtp_addr_name.sin_family = PJ_AF_INET;
    si.rtp_addr_name.sin_port = pj_htons((pj_uint16_t)port);

    status = pj_sock_bind(si.rtp_sock, &si.rtp_addr_name, 
			  sizeof(si.rtp_addr_name));
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create RTCP socket */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &si.rtcp_sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Bind RTCP socket */
    si.rtcp_addr_name.sin_family = PJ_AF_INET;
    si.rtcp_addr_name.sin_port = pj_htons((pj_uint16_t)(port+1));

    status = pj_sock_bind(si.rtcp_sock, &si.rtcp_addr_name,
			  sizeof(si.rtcp_addr_name));
    if (status != PJ_SUCCESS)
	goto on_error;

    
    /* Create UDP transport by attaching socket info */
    return pjmedia_transport_udp_attach( endpt, name, &si, p_tp);


on_error:
    if (si.rtp_sock != PJ_INVALID_SOCKET)
	pj_sock_close(si.rtp_sock);
    if (si.rtcp_sock != PJ_INVALID_SOCKET)
	pj_sock_close(si.rtcp_sock);
    return status;
}


/**
 * Create UDP stream transport from existing socket info.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_attach( pjmedia_endpt *endpt,
						  const char *name,
						  const pjmedia_sock_info *si,
						  pjmedia_transport **p_tp)
{
    struct transport_udp *tp;
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_ioqueue_callback rtp_cb, rtcp_cb;
    pj_ssize_t size;
    pj_status_t status;


    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && si && p_tp, PJ_EINVAL);

    /* Check name */
    if (!name)
	name = "udpmedia";

    /* Get ioqueue instance */
    ioqueue = pjmedia_endpt_get_ioqueue(endpt);


    /* Create transport structure */
    pool = pjmedia_endpt_create_pool(endpt, name, 4000, 4000);
    if (!pool)
	return PJ_ENOMEM;

    tp = pj_pool_zalloc(pool, sizeof(struct transport_udp));
    tp->pool = pool;
    pj_ansi_strcpy(tp->base.name, name);
    tp->base.op = &transport_udp_op;

    /* Copy socket infos */
    tp->rtp_sock = si->rtp_sock;
    tp->rtcp_sock = si->rtcp_sock;


    /* Setup RTP socket with the ioqueue */
    pj_memset(&rtp_cb, 0, sizeof(rtp_cb));
    rtp_cb.on_read_complete = &on_rx_rtp;

    status = pj_ioqueue_register_sock(pool, ioqueue, tp->rtp_sock, tp,
				      &rtp_cb, &tp->rtp_key);
    if (status != PJ_SUCCESS)
	goto on_error;
    
    pj_ioqueue_op_key_init(&tp->rtp_read_op, sizeof(tp->rtp_read_op));
    pj_ioqueue_op_key_init(&tp->rtcp_write_op, sizeof(tp->rtcp_write_op));

    /* Kick of pending RTP read from the ioqueue */
    tp->rtp_addrlen = sizeof(tp->rtp_src_addr);
    size = sizeof(tp->rtp_pkt);
    status = pj_ioqueue_recvfrom(tp->rtp_key, &tp->rtp_read_op,
			         tp->rtp_pkt, &size, PJ_IOQUEUE_ALWAYS_ASYNC,
				 &tp->rtp_src_addr, &tp->rtp_addrlen);
    if (status != PJ_EPENDING)
	goto on_error;


    /* Setup RTCP socket with ioqueue */
    pj_memset(&rtcp_cb, 0, sizeof(rtcp_cb));
    rtcp_cb.on_read_complete = &on_rx_rtcp;

    status = pj_ioqueue_register_sock(pool, ioqueue, tp->rtcp_sock, tp,
				      &rtcp_cb, &tp->rtcp_key);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_ioqueue_op_key_init(&tp->rtcp_read_op, sizeof(tp->rtcp_read_op));
    pj_ioqueue_op_key_init(&tp->rtcp_write_op, sizeof(tp->rtcp_write_op));


    /* Kick of pending RTCP read from the ioqueue */
    size = sizeof(tp->rtcp_pkt);
    status = pj_ioqueue_recv(tp->rtcp_key, &tp->rtcp_read_op,
			     tp->rtcp_pkt, &size, PJ_IOQUEUE_ALWAYS_ASYNC);
    if (status != PJ_EPENDING)
	goto on_error;


    /* Done */
    *p_tp = &tp->base;
    return PJ_SUCCESS;


on_error:
    pjmedia_transport_udp_close(&tp->base);
    return status;
}


/**
 * Close UDP transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_close(pjmedia_transport *tp)
{
    struct transport_udp *udp = (struct transport_udp*) tp;

    /* Sanity check */
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    /* Must not close while stream is using this */
    PJ_ASSERT_RETURN(udp->stream == NULL, PJ_EINVALIDOP);
    

    if (udp->rtp_key) {
	pj_ioqueue_unregister(udp->rtp_key);
	udp->rtp_key = NULL;
    } else if (udp->rtp_sock != PJ_INVALID_SOCKET) {
	pj_sock_close(udp->rtp_sock);
	udp->rtp_sock = PJ_INVALID_SOCKET;
    }

    if (udp->rtcp_key) {
	pj_ioqueue_unregister(udp->rtcp_key);
	udp->rtcp_key = NULL;
    } else if (udp->rtcp_sock != PJ_INVALID_SOCKET) {
	pj_sock_close(udp->rtcp_sock);
	udp->rtcp_sock = PJ_INVALID_SOCKET;
    }

    pj_pool_release(udp->pool);

    return PJ_SUCCESS;
}


/* Notification from ioqueue about incoming RTP packet */
static void on_rx_rtp( pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read)
{
    struct transport_udp *udp;
    pj_status_t status;

    PJ_UNUSED_ARG(op_key);

    udp = pj_ioqueue_get_user_data(key);

    do {
	void (*cb)(pjmedia_stream*,const void*,pj_ssize_t);
	pjmedia_stream *stream;

	cb = udp->rtp_cb;
	stream = udp->stream;

	if (bytes_read > 0 && cb && stream)
	    (*cb)(stream, udp->rtp_pkt, bytes_read);

	/* See if source address of RTP packet is different than the 
	 * configured address.
	 */
	if ((udp->rem_rtp_addr.sin_addr.s_addr != 
	     udp->rtp_src_addr.sin_addr.s_addr) ||
	    (udp->rem_rtp_addr.sin_port != 
	     udp->rtp_src_addr.sin_port))
	{
	    udp->rtp_src_cnt++;

	    if (udp->rtp_src_cnt >= PJMEDIA_RTP_NAT_PROBATION_CNT) {
	    
		udp->rem_rtp_addr = udp->rtp_src_addr;
		udp->rtp_src_cnt = 0;

		PJ_LOG(4,(udp->base.name,
			  "Remote RTP address switched to %s:%d",
			  pj_inet_ntoa(udp->rtp_src_addr.sin_addr),
			  pj_ntohs(udp->rtp_src_addr.sin_port)));
	    }
	}

	bytes_read = sizeof(udp->rtp_pkt);
	udp->rtp_addrlen = sizeof(pj_sockaddr_in);
	status = pj_ioqueue_recvfrom(udp->rtp_key, &udp->rtp_read_op,
				     udp->rtp_pkt, &bytes_read, 0,
				     &udp->rtp_src_addr, 
				     &udp->rtp_addrlen);

    } while (status == PJ_SUCCESS);
}


/* Notification from ioqueue about incoming RTCP packet */
static void on_rx_rtcp(pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read)
{
    struct transport_udp *udp;
    pj_status_t status;

    PJ_UNUSED_ARG(op_key);

    udp = pj_ioqueue_get_user_data(key);

    do {
	void (*cb)(pjmedia_stream*,const void*,pj_ssize_t);
	pjmedia_stream *stream;

	cb = udp->rtcp_cb;
	stream = udp->stream;

	if (bytes_read > 0 && cb && stream)
	    (*cb)(stream, udp->rtcp_pkt, bytes_read);

	bytes_read = sizeof(udp->rtcp_pkt);
	status = pj_ioqueue_recv(udp->rtcp_key, &udp->rtcp_read_op,
				     udp->rtcp_pkt, &bytes_read, 0);

    } while (status == PJ_SUCCESS);
}


/* Called by stream to initialize the transport */
static pj_status_t transport_attach(   pjmedia_transport *tp,
				       pjmedia_stream *strm,
				       const pj_sockaddr_t *rem_addr,
				       unsigned addr_len,
				       void (*rtp_cb)(pjmedia_stream*,
						      const void*,
						      pj_ssize_t),
				       void (*rtcp_cb)(pjmedia_stream*,
						       const void*,
						       pj_ssize_t))
{
    struct transport_udp *udp = (struct transport_udp*) tp;

    /* Validate arguments */
    PJ_ASSERT_RETURN(tp && strm && rem_addr && addr_len, PJ_EINVAL);

    /* Must not be "attached" to existing stream */
    PJ_ASSERT_RETURN(udp->stream == NULL, PJ_EINVALIDOP);

    /* "Attach" the stream: */

    /* Copy remote RTP address */
    pj_memcpy(&udp->rem_rtp_addr, rem_addr, sizeof(pj_sockaddr_in));

    /* Guess RTCP address from RTP address */
    pj_memcpy(&udp->rem_rtcp_addr, rem_addr, sizeof(pj_sockaddr_in));
    udp->rem_rtcp_addr.sin_port = (pj_uint16_t) pj_htons((pj_uint16_t)(
				    pj_ntohs(udp->rem_rtp_addr.sin_port)+1));

    /* Save the callbacks */
    udp->rtp_cb = rtp_cb;
    udp->rtcp_cb = rtcp_cb;

    /* Last, save the stream to mark that we have a "client" */
    udp->stream = strm;

    return PJ_SUCCESS;
}


/* Called by stream when it no longer needs the transport */
static void transport_detach( pjmedia_transport *tp,
			      pjmedia_stream *strm)
{
    struct transport_udp *udp = (struct transport_udp*) tp;

    pj_assert(tp && strm);

    PJ_UNUSED_ARG(strm);

    /* Clear up stream infos from transport */
    udp->stream = NULL;
    udp->rtp_cb = NULL;
    udp->rtcp_cb = NULL;
}


/* Called by stream to send RTP packet */
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct transport_udp *udp = (struct transport_udp*)tp;
    pj_ssize_t sent;
    pj_status_t status;

    PJ_ASSERT_RETURN(udp->stream, PJ_EINVALIDOP);

    sent = size;
    status = pj_ioqueue_sendto( udp->rtp_key, &udp->rtp_write_op,
				pkt, &sent, 0,
				&udp->rem_rtp_addr, sizeof(pj_sockaddr_in));

    if (status==PJ_SUCCESS || status==PJ_EPENDING)
	return PJ_SUCCESS;

    return status;
}

/* Called by stream to send RTCP packet */
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct transport_udp *udp = (struct transport_udp*)tp;
    pj_ssize_t sent;
    pj_status_t status;

    PJ_ASSERT_RETURN(udp->stream, PJ_EINVALIDOP);

    sent = size;
    status = pj_ioqueue_sendto( udp->rtcp_key, &udp->rtcp_write_op,
				pkt, &sent, 0,
				&udp->rem_rtcp_addr, sizeof(pj_sockaddr_in));

    if (status==PJ_SUCCESS || status==PJ_EPENDING)
	return PJ_SUCCESS;

    return status;
}


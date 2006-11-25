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
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/rand.h>
#include <pj/string.h>


/* Maximum size of incoming RTP packet */
#define RTP_LEN	    1500

/* Maximum size of incoming RTCP packet */
#define RTCP_LEN    600

/* Maximum pending write operations */
#define MAX_PENDING 4

/* Pending write buffer */
typedef struct pending_write
{
    char		buffer[RTP_LEN];
    pj_ioqueue_op_key_t	op_key;
} pending_write;


struct transport_udp
{
    pjmedia_transport	base;		/**< Base transport.		    */

    pj_pool_t	       *pool;		/**< Memory pool		    */
    unsigned		options;	/**< Transport options.		    */
    void	       *user_data;	/**< Only valid when attached	    */
    pj_bool_t		attached;	/**< Has attachment?		    */
    pj_sockaddr_in	rem_rtp_addr;	/**< Remote RTP address		    */
    pj_sockaddr_in	rem_rtcp_addr;	/**< Remote RTCP address	    */
    void  (*rtp_cb)(	void*,		/**< To report incoming RTP.	    */
			const void*,
			pj_ssize_t);
    void  (*rtcp_cb)(	void*,		/**< To report incoming RTCP.	    */
			const void*,
			pj_ssize_t);

    unsigned		tx_drop_pct;	/**< Percent of tx pkts to drop.    */
    unsigned		rx_drop_pct;	/**< Percent of rx pkts to drop.    */

    pj_sock_t	        rtp_sock;	/**< RTP socket			    */
    pj_sockaddr_in	rtp_addr_name;	/**< Published RTP address.	    */
    pj_ioqueue_key_t   *rtp_key;	/**< RTP socket key in ioqueue	    */
    pj_ioqueue_op_key_t	rtp_read_op;	/**< Pending read operation	    */
    unsigned		rtp_write_op_id;/**< Next write_op to use	    */
    pending_write	rtp_pending_write[MAX_PENDING];  /**< Pending write */
    pj_sockaddr_in	rtp_src_addr;	/**< Actual packet src addr.	    */
    unsigned		rtp_src_cnt;	/**< How many pkt from this addr.   */
    int			rtp_addrlen;	/**< Address length.		    */
    char		rtp_pkt[RTP_LEN];/**< Incoming RTP packet buffer    */

    pj_sock_t		rtcp_sock;	/**< RTCP socket		    */
    pj_sockaddr_in	rtcp_addr_name;	/**< Published RTCP address.	    */
    pj_sockaddr_in	rtcp_src_addr;	/**< Actual source RTCP address.    */
    int			rtcp_addr_len;	/**< Length of RTCP src address.    */
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
				       void *user_data,
				       const pj_sockaddr_t *rem_addr,
				       const pj_sockaddr_t *rem_rtcp,
				       unsigned addr_len,
				       void (*rtp_cb)(void*,
						      const void*,
						      pj_ssize_t),
				       void (*rtcp_cb)(void*,
						       const void*,
						       pj_ssize_t));
static void	   transport_detach(   pjmedia_transport *tp,
				       void *strm);
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
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    return pjmedia_transport_udp_create2(endpt, name, NULL, port, options, 
					p_tp);
}

/**
 * Create UDP stream transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_create2(pjmedia_endpt *endpt,
						  const char *name,
						  const pj_str_t *addr,
						  int port,
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    pjmedia_sock_info si;
    pj_status_t status;

    
    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && port && p_tp, PJ_EINVAL);


    pj_bzero(&si, sizeof(pjmedia_sock_info));
    si.rtp_sock = si.rtcp_sock = PJ_INVALID_SOCKET;

    /* Create RTP socket */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &si.rtp_sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Bind RTP socket */
    pj_sockaddr_in_init(&si.rtp_addr_name, addr, (pj_uint16_t)port);
    status = pj_sock_bind(si.rtp_sock, &si.rtp_addr_name, 
			  sizeof(si.rtp_addr_name));
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create RTCP socket */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &si.rtcp_sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Bind RTCP socket */
    pj_sockaddr_in_init(&si.rtcp_addr_name, addr, (pj_uint16_t)(port+1));
    status = pj_sock_bind(si.rtcp_sock, &si.rtcp_addr_name,
			  sizeof(si.rtcp_addr_name));
    if (status != PJ_SUCCESS)
	goto on_error;

    
    /* Create UDP transport by attaching socket info */
    return pjmedia_transport_udp_attach( endpt, name, &si, options, p_tp);


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
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    struct transport_udp *tp;
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_ioqueue_callback rtp_cb, rtcp_cb;
    pj_ssize_t size;
    unsigned i;
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
    tp->options = options;
    pj_ansi_strcpy(tp->base.name, name);
    tp->base.op = &transport_udp_op;

    /* Copy socket infos */
    tp->rtp_sock = si->rtp_sock;
    tp->rtp_addr_name = si->rtp_addr_name;
    tp->rtcp_sock = si->rtcp_sock;
    tp->rtcp_addr_name = si->rtcp_addr_name;

    /* If address is 0.0.0.0, use host's IP address */
    if (tp->rtp_addr_name.sin_addr.s_addr == 0) {
	pj_in_addr hostip;

	status = pj_gethostip(&hostip);
	if (status != PJ_SUCCESS)
	    goto on_error;

	tp->rtp_addr_name.sin_addr = hostip;
    }

    /* Same with RTCP */
    if (tp->rtcp_addr_name.sin_addr.s_addr == 0) {
	tp->rtcp_addr_name.sin_addr.s_addr = tp->rtp_addr_name.sin_addr.s_addr;
    }

    /* Setup RTP socket with the ioqueue */
    pj_bzero(&rtp_cb, sizeof(rtp_cb));
    rtp_cb.on_read_complete = &on_rx_rtp;

    status = pj_ioqueue_register_sock(pool, ioqueue, tp->rtp_sock, tp,
				      &rtp_cb, &tp->rtp_key);
    if (status != PJ_SUCCESS)
	goto on_error;
    
    pj_ioqueue_op_key_init(&tp->rtp_read_op, sizeof(tp->rtp_read_op));
    for (i=0; i<PJ_ARRAY_SIZE(tp->rtp_pending_write); ++i)
	pj_ioqueue_op_key_init(&tp->rtp_pending_write[i].op_key, 
			       sizeof(tp->rtp_pending_write[i].op_key));

    /* Kick of pending RTP read from the ioqueue */
    tp->rtp_addrlen = sizeof(tp->rtp_src_addr);
    size = sizeof(tp->rtp_pkt);
    status = pj_ioqueue_recvfrom(tp->rtp_key, &tp->rtp_read_op,
			         tp->rtp_pkt, &size, PJ_IOQUEUE_ALWAYS_ASYNC,
				 &tp->rtp_src_addr, &tp->rtp_addrlen);
    if (status != PJ_EPENDING)
	goto on_error;


    /* Setup RTCP socket with ioqueue */
    pj_bzero(&rtcp_cb, sizeof(rtcp_cb));
    rtcp_cb.on_read_complete = &on_rx_rtcp;

    status = pj_ioqueue_register_sock(pool, ioqueue, tp->rtcp_sock, tp,
				      &rtcp_cb, &tp->rtcp_key);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_ioqueue_op_key_init(&tp->rtcp_read_op, sizeof(tp->rtcp_read_op));
    pj_ioqueue_op_key_init(&tp->rtcp_write_op, sizeof(tp->rtcp_write_op));


    /* Kick of pending RTCP read from the ioqueue */
    size = sizeof(tp->rtcp_pkt);
    tp->rtcp_addr_len = sizeof(tp->rtcp_src_addr);
    status = pj_ioqueue_recvfrom( tp->rtcp_key, &tp->rtcp_read_op,
				  tp->rtcp_pkt, &size, PJ_IOQUEUE_ALWAYS_ASYNC,
				  &tp->rtcp_src_addr, &tp->rtcp_addr_len);
    if (status != PJ_EPENDING)
	goto on_error;


    /* Done */
    *p_tp = &tp->base;
    return PJ_SUCCESS;


on_error:
    pjmedia_transport_udp_close(&tp->base);
    return status;
}


/*
 * Get media socket info.
 */
PJ_DEF(pj_status_t) 
pjmedia_transport_udp_get_info( pjmedia_transport *tp,
				pjmedia_transport_udp_info *inf)
{
    struct transport_udp *udp = (struct transport_udp*)tp;
    PJ_ASSERT_RETURN(tp && inf, PJ_EINVAL);

    inf->skinfo.rtp_sock = udp->rtp_sock;
    inf->skinfo.rtp_addr_name = udp->rtp_addr_name;
    inf->skinfo.rtcp_sock = udp->rtcp_sock;
    inf->skinfo.rtcp_addr_name = udp->rtcp_addr_name;

    return PJ_SUCCESS;
}


/**
 * Close UDP transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_close(pjmedia_transport *tp)
{
    struct transport_udp *udp = (struct transport_udp*) tp;

    /* Sanity check */
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    /* Must not close while application is using this */
    PJ_ASSERT_RETURN(!udp->attached, PJ_EINVALIDOP);
    

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
	void (*cb)(void*,const void*,pj_ssize_t);
	void *user_data;

	cb = udp->rtp_cb;
	user_data = udp->user_data;

	/* Simulate packet lost on RX direction */
	if (udp->rx_drop_pct) {
	    if ((pj_rand() % 100) <= (int)udp->rx_drop_pct) {
		PJ_LOG(5,(udp->base.name, 
			  "RX RTP packet dropped because of pkt lost "
			  "simulation"));
		goto read_next_packet;
	    }
	}


	if (udp->attached && cb)
	    (*cb)(user_data, udp->rtp_pkt, bytes_read);

	/* See if source address of RTP packet is different than the 
	 * configured address, and switch RTP remote address to 
	 * source packet address after several consecutive packets
	 * have been received.
	 */
	if (bytes_read>0 && 
	    (udp->options & PJMEDIA_UDP_NO_SRC_ADDR_CHECKING)==0) 
	{
	    if ((udp->rem_rtp_addr.sin_addr.s_addr != 
		 udp->rtp_src_addr.sin_addr.s_addr) ||
		(udp->rem_rtp_addr.sin_port != 
		 udp->rtp_src_addr.sin_port))
	    {
		udp->rtp_src_cnt++;

		if (udp->rtp_src_cnt >= PJMEDIA_RTP_NAT_PROBATION_CNT) {
		
		    /* Set remote RTP address to source address */
		    udp->rem_rtp_addr = udp->rtp_src_addr;

		    /* Reset counter */
		    udp->rtp_src_cnt = 0;

		    PJ_LOG(4,(udp->base.name,
			      "Remote RTP address switched to %s:%d",
			      pj_inet_ntoa(udp->rtp_src_addr.sin_addr),
			      pj_ntohs(udp->rtp_src_addr.sin_port)));

		    /* Also update remote RTCP address if actual RTCP source
		     * address is not heard yet.
		     */
		    if (udp->rtcp_src_addr.sin_addr.s_addr == 0) {
			pj_uint16_t port;

			pj_memcpy(&udp->rem_rtcp_addr, &udp->rem_rtp_addr, 
				  sizeof(pj_sockaddr_in));
			port = (pj_uint16_t)
			       (pj_ntohs(udp->rem_rtp_addr.sin_port)+1);
			udp->rem_rtcp_addr.sin_port = pj_htons(port);

			pj_memcpy(&udp->rtcp_src_addr, &udp->rem_rtcp_addr, 
				  sizeof(pj_sockaddr_in));

			PJ_LOG(4,(udp->base.name,
				  "Remote RTCP address switched to %s:%d",
				  pj_inet_ntoa(udp->rtcp_src_addr.sin_addr),
				  pj_ntohs(udp->rtcp_src_addr.sin_port)));

		    }
		}
	    }
	}

read_next_packet:
	bytes_read = sizeof(udp->rtp_pkt);
	udp->rtp_addrlen = sizeof(pj_sockaddr_in);
	status = pj_ioqueue_recvfrom(udp->rtp_key, &udp->rtp_read_op,
				     udp->rtp_pkt, &bytes_read, 0,
				     &udp->rtp_src_addr, 
				     &udp->rtp_addrlen);

    } while (status != PJ_EPENDING);
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
	void (*cb)(void*,const void*,pj_ssize_t);
	void *user_data;

	cb = udp->rtcp_cb;
	user_data = udp->user_data;

	if (udp->attached && cb)
	    (*cb)(user_data, udp->rtcp_pkt, bytes_read);

	/* Check if RTCP source address is the same as the configured
	 * remote address, and switch the address when they are
	 * different.
	 */
	if (bytes_read>0 &&
	    (udp->options & PJMEDIA_UDP_NO_SRC_ADDR_CHECKING)==0 &&
	    ((udp->rem_rtcp_addr.sin_addr.s_addr != 
	       udp->rtcp_src_addr.sin_addr.s_addr) ||
	     (udp->rem_rtcp_addr.sin_port != 
	       udp->rtcp_src_addr.sin_port)))
	{
	    pj_memcpy(&udp->rem_rtcp_addr, &udp->rtcp_src_addr,
		      sizeof(pj_sockaddr_in));
	    PJ_LOG(4,(udp->base.name,
		      "Remote RTCP address switched to %s:%d",
		      pj_inet_ntoa(udp->rtcp_src_addr.sin_addr),
		      pj_ntohs(udp->rtcp_src_addr.sin_port)));
	}

	bytes_read = sizeof(udp->rtcp_pkt);
	udp->rtcp_addr_len = sizeof(udp->rtcp_src_addr);
	status = pj_ioqueue_recvfrom(udp->rtcp_key, &udp->rtcp_read_op,
				     udp->rtcp_pkt, &bytes_read, 0,
				     &udp->rtcp_src_addr, 
				     &udp->rtcp_addr_len);

    } while (status != PJ_EPENDING);
}


/* Called by application to initialize the transport */
static pj_status_t transport_attach(   pjmedia_transport *tp,
				       void *user_data,
				       const pj_sockaddr_t *rem_addr,
				       const pj_sockaddr_t *rem_rtcp,
				       unsigned addr_len,
				       void (*rtp_cb)(void*,
						      const void*,
						      pj_ssize_t),
				       void (*rtcp_cb)(void*,
						       const void*,
						       pj_ssize_t))
{
    struct transport_udp *udp = (struct transport_udp*) tp;
    const pj_sockaddr_in *rtcp_addr;

    /* Validate arguments */
    PJ_ASSERT_RETURN(tp && rem_addr && addr_len, PJ_EINVAL);

    /* Must not be "attached" to existing application */
    PJ_ASSERT_RETURN(!udp->attached, PJ_EINVALIDOP);

    /* "Attach" the application: */

    /* Copy remote RTP address */
    pj_memcpy(&udp->rem_rtp_addr, rem_addr, sizeof(pj_sockaddr_in));

    /* Copy remote RTP address, if one is specified. */
    rtcp_addr = rem_rtcp;
    if (rtcp_addr && rtcp_addr->sin_addr.s_addr != 0) {
	pj_memcpy(&udp->rem_rtcp_addr, rem_rtcp, sizeof(pj_sockaddr_in));

    } else {
	int rtcp_port;

	/* Otherwise guess the RTCP address from the RTP address */
	pj_memcpy(&udp->rem_rtcp_addr, rem_addr, sizeof(pj_sockaddr_in));
	rtcp_port = pj_ntohs(udp->rem_rtp_addr.sin_port) + 1;
	udp->rem_rtcp_addr.sin_port = pj_htons((pj_uint16_t)rtcp_port);
    }

    /* Save the callbacks */
    udp->rtp_cb = rtp_cb;
    udp->rtcp_cb = rtcp_cb;
    udp->user_data = user_data;

    /* Last, mark transport as attached */
    udp->attached = PJ_TRUE;

    return PJ_SUCCESS;
}


/* Called by application when it no longer needs the transport */
static void transport_detach( pjmedia_transport *tp,
			      void *user_data)
{
    struct transport_udp *udp = (struct transport_udp*) tp;

    pj_assert(tp);

    if (udp->attached) {
	/* User data is unreferenced on Release build */
	PJ_UNUSED_ARG(user_data);

	/* As additional checking, check if the same user data is specified */
	pj_assert(user_data == udp->user_data);

	/* First, mark transport as unattached */
	udp->attached = PJ_FALSE;

	/* Clear up application infos from transport */
	udp->rtp_cb = NULL;
	udp->rtcp_cb = NULL;
	udp->user_data = NULL;
    }
}


/* Called by application to send RTP packet */
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct transport_udp *udp = (struct transport_udp*)tp;
    pj_ssize_t sent;
    unsigned id;
    struct pending_write *pw;
    pj_status_t status;

    /* Must be attached */
    PJ_ASSERT_RETURN(udp->attached, PJ_EINVALIDOP);

    /* Check that the size is supported */
    PJ_ASSERT_RETURN(size <= RTP_LEN, PJ_ETOOBIG);

    /* Simulate packet lost on TX direction */
    if (udp->tx_drop_pct) {
	if ((pj_rand() % 100) <= (int)udp->tx_drop_pct) {
	    PJ_LOG(5,(udp->base.name, 
		      "TX RTP packet dropped because of pkt lost "
		      "simulation"));
	    return PJ_SUCCESS;
	}
    }


    id = udp->rtp_write_op_id;
    pw = &udp->rtp_pending_write[id];

    /* We need to copy packet to our buffer because when the
     * operation is pending, caller might write something else
     * to the original buffer.
     */
    pj_memcpy(pw->buffer, pkt, size);

    sent = size;
    status = pj_ioqueue_sendto( udp->rtp_key, 
				&udp->rtp_pending_write[id].op_key,
				pw->buffer, &sent, 0,
				&udp->rem_rtp_addr, 
				sizeof(pj_sockaddr_in));

    udp->rtp_write_op_id = (udp->rtp_write_op_id + 1) %
			   PJ_ARRAY_SIZE(udp->rtp_pending_write);

    if (status==PJ_SUCCESS || status==PJ_EPENDING)
	return PJ_SUCCESS;

    return status;
}

/* Called by application to send RTCP packet */
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct transport_udp *udp = (struct transport_udp*)tp;
    pj_ssize_t sent;
    pj_status_t status;

    PJ_ASSERT_RETURN(udp->attached, PJ_EINVALIDOP);

    sent = size;
    status = pj_ioqueue_sendto( udp->rtcp_key, &udp->rtcp_write_op,
				pkt, &sent, 0,
				&udp->rem_rtcp_addr, sizeof(pj_sockaddr_in));

    if (status==PJ_SUCCESS || status==PJ_EPENDING)
	return PJ_SUCCESS;

    return status;
}


PJ_DEF(pj_status_t) pjmedia_transport_udp_simulate_lost(pjmedia_transport *tp,
							pjmedia_dir dir,
							unsigned pct_lost)
{
    struct transport_udp *udp = (struct transport_udp*)tp;

    PJ_ASSERT_RETURN(tp && 
		     (dir==PJMEDIA_DIR_ENCODING||dir==PJMEDIA_DIR_DECODING) &&
		     pct_lost <= 100, PJ_EINVAL);

    if (dir == PJMEDIA_DIR_ENCODING)
	udp->tx_drop_pct = pct_lost;
    else if (dir == PJMEDIA_DIR_DECODING)
	udp->rx_drop_pct = pct_lost;
    else
	return PJ_EINVAL;

    return PJ_SUCCESS;
}


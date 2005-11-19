/* $Id: $ */
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
#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/os.h>
#include <pj/lock.h>
#include <pj/string.h>
#include <pj/assert.h>


/* Struct udp_transport "inherits" struct pjsip_transport */
struct udp_transport
{
    pjsip_transport	base;
    pj_sock_t		sock;
    pj_ioqueue_key_t   *key;
    int			rdata_cnt;
    pjsip_rx_data     **rdata;
    int			is_closing;
};


/*
 * on_read_complete()
 *
 * This is callback notification from ioqueue that a pending recvfrom()
 * operation has completed.
 */
static void on_read_complete( pj_ioqueue_key_t *key, 
			      pj_ioqueue_op_key_t *op_key, 
			      pj_ssize_t bytes_read)
{
    enum { MAX_IMMEDIATE_PACKET = 10 };
    pjsip_rx_data_op_key *rdata_op_key = (pjsip_rx_data_op_key*) op_key;
    pjsip_rx_data *rdata = rdata_op_key->rdata;
    struct udp_transport *tp = (struct udp_transport*)rdata->tp_info.transport;
    int i;
    pj_status_t status;

    /* Don't do anything if transport is closing. */
    if (tp->is_closing)
	return;

    /*
     * The idea of the loop is to process immediate data received by
     * pj_ioqueue_recvfrom(), as long as i < MAX_IMMEDIATE_PACKET. When
     * i is >= MAX_IMMEDIATE_PACKET, we force the recvfrom() operation to
     * complete asynchronously, to allow other sockets to get their data.
     */
    for (i=0;; ++i) {
	pj_uint32_t flags;

	/* Report the packet to transport manager. */
	if (bytes_read > 0) {
	    pj_size_t size_eaten;

	    rdata->pkt_info.len = bytes_read;
	    rdata->pkt_info.zero = 0;
	    pj_gettimeofday(&rdata->pkt_info.timestamp);

	    size_eaten = 
		pjsip_tpmgr_receive_packet(rdata->tp_info.transport->tpmgr, 
					   rdata);

	    if (size_eaten < 0) {
		pj_assert(!"It shouldn't happen!");
		size_eaten = rdata->pkt_info.len;
	    }

	    /* Since this is UDP, the whole buffer is the message. */
	    rdata->pkt_info.len = 0;

	} else if (bytes_read == 0) {
	    /* TODO: */
	} else {
	    /* Report error to endpoint. */
	    PJSIP_ENDPT_LOG_ERROR((rdata->tp_info.transport->endpt,
				   rdata->tp_info.transport->obj_name,
				   -bytes_read, 
				   "Warning: pj_ioqueue_recvfrom()"
				   " callback error"));
	}

	if (i >= MAX_IMMEDIATE_PACKET) {
	    /* Force ioqueue_recvfrom() to return PJ_EPENDING */
	    flags = PJ_IOQUEUE_ALWAYS_ASYNC;
	} else {
	    flags = 0;
	}

	/* Read next packet. */
	bytes_read = sizeof(rdata->pkt_info.packet);
	status = pj_ioqueue_recvfrom(key, op_key, 
				     rdata->pkt_info.packet,
				     &bytes_read, flags,
				     &rdata->pkt_info.addr, 
				     &rdata->pkt_info.addr_len);

	if (status == PJ_SUCCESS) {
	    /* Continue loop. */
	    pj_assert(i < MAX_IMMEDIATE_PACKET);

	} else if (status == PJ_EPENDING) {
	    break;

	} else {

	    if (i < MAX_IMMEDIATE_PACKET) {
		/* Report error to endpoint. */
		PJSIP_ENDPT_LOG_ERROR((rdata->tp_info.transport->endpt,
				       rdata->tp_info.transport->obj_name,
				       status, 
				       "Warning: pj_ioqueue_recvfrom error"));
		/* Continue loop. */
		bytes_read = 0;
	    } else {
		/* This is fatal error.
		 * Ioqueue operation will stop for this transport!
		 */
		PJSIP_ENDPT_LOG_ERROR((rdata->tp_info.transport->endpt,
				       rdata->tp_info.transport->obj_name,
				       status, 
				       "FATAL: pj_ioqueue_recvfrom() error, "
				       "UDP transport stopping! Error"));
		break;
	    }
	}
    }
}

/*
 * on_write_complete()
 *
 * This is callback notification from ioqueue that a pending sendto()
 * operation has completed.
 */
static void on_write_complete( pj_ioqueue_key_t *key, 
			       pj_ioqueue_op_key_t *op_key,
			       pj_ssize_t bytes_sent)
{
    struct udp_transport *tp = pj_ioqueue_get_user_data(key);
    pjsip_tx_data_op_key *tdata_op_key = (pjsip_tx_data_op_key*)op_key;

    if (tdata_op_key->callback) {
	tdata_op_key->callback(&tp->base, tdata_op_key->token, bytes_sent);
    }
}

/*
 * transport_send_msg()
 *
 * This function is called by transport manager (by transport->send_msg())
 * to send outgoing message.
 */
static pj_status_t transport_send_msg( pjsip_transport *transport,
				       pjsip_tx_data *tdata,
				       const pj_sockaddr_in *rem_addr,
				       void *token,
				       void (*callback)(pjsip_transport*,
							void *token,
							pj_ssize_t))
{
    struct udp_transport *tp = (struct udp_transport*)transport;
    pj_ssize_t size;

    PJ_ASSERT_RETURN(transport && tdata, PJ_EINVAL);

    /* Init op key. */
    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;

    /* Send to ioqueue! */
    size = tdata->buf.cur - tdata->buf.start;
    return pj_ioqueue_sendto(tp->key, (pj_ioqueue_op_key_t*)&tdata->op_key,
			     tdata->buf.start, &size, 0,
			     rem_addr, (rem_addr ? sizeof(pj_sockaddr_in):0));
}

/*
 * transport_destroy()
 *
 * This function is called by transport manager (by transport->destroy()).
 */
static pj_status_t transport_destroy( pjsip_transport *transport )
{
    struct udp_transport *tp = (struct udp_transport*)transport;
    int i;

    /* Mark this transport as closing. */
    tp->is_closing = 1;

    /* Cancel all pending operations. */
    for (i=0; i<tp->rdata_cnt; ++i) {
	pj_ioqueue_post_completion(tp->key, 
				   &tp->rdata[i]->tp_info.op_key.op_key, -1);
    }

    /* Unregister from ioqueue. */
    if (tp->key)
	pj_ioqueue_unregister(tp->key);

    /* Close socket. */
    if (tp->sock && tp->sock != PJ_INVALID_SOCKET)
	pj_sock_close(tp->sock);

    /* Destroy reference counter. */
    if (tp->base.ref_cnt)
	pj_atomic_destroy(tp->base.ref_cnt);

    /* Destroy lock */
    if (tp->base.lock)
	pj_lock_destroy(tp->base.lock);

    /* Destroy pool. */
    pjsip_endpt_destroy_pool(tp->base.endpt, tp->base.pool);

    return PJ_SUCCESS;
}


/*
 * pjsip_udp_transport_start()
 *
 * Start an UDP transport/listener.
 */
PJ_DEF(pj_status_t) pjsip_udp_transport_start( pjsip_endpoint *endpt,
					       const pj_sockaddr_in *local,
					       const pj_sockaddr_in *pub_addr,
					       unsigned async_cnt,
					       pjsip_transport **p_transport)
{
    pj_pool_t *pool;
    struct udp_transport *tp;
    pj_ioqueue_t *ioqueue;
    pj_ioqueue_callback ioqueue_cb;
    unsigned i;
    pj_status_t status;

    /* Create pool. */
    pool = pjsip_endpt_create_pool(endpt, "udp%p", PJSIP_POOL_LEN_TRANSPORT, 
			       PJSIP_POOL_INC_TRANSPORT);
    if (!pool)
	return PJ_ENOMEM;

    tp = pj_pool_zalloc(pool, sizeof(struct udp_transport));
    tp->base.pool = pool;
    tp->base.endpt = endpt;

    /* Init type, type_name, and flag */
    tp->base.type = PJSIP_TRANSPORT_UDP;
    pj_native_strcpy(tp->base.type_name, "UDP");
    tp->base.flag = pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_UDP);

    /* Init addresses. */
    pj_memcpy(&tp->base.local_addr, local, sizeof(pj_sockaddr_in));
    pj_memcpy(&tp->base.public_addr, pub_addr, sizeof(pj_sockaddr_in));
    tp->base.rem_addr.sin_family = PJ_AF_INET;

    /* Init reference counter. */
    status = pj_atomic_create(pool, 0, &tp->base.ref_cnt);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init lock. */
    status = pj_lock_create_recursive_mutex(pool, "udp%p", &tp->base.lock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Create socket. */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &tp->sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Bind socket. */
    status = pj_sock_bind(tp->sock, local, sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register to ioqueue. */
    ioqueue = pjsip_endpt_get_ioqueue(endpt);
    pj_memset(&ioqueue_cb, 0, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = &on_read_complete;
    ioqueue_cb.on_write_complete = &on_write_complete;
    status = pj_ioqueue_register_sock(pool, ioqueue, tp->sock, tp, 
				      &ioqueue_cb, &tp->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Set functions. */
    tp->base.send_msg = &transport_send_msg;
    tp->base.destroy = &transport_destroy;

    /* This is a permanent transport, so we initialize the ref count
     * to one so that transport manager don't destroy this transport
     * when there's no user!
     */
    pj_atomic_inc(tp->base.ref_cnt);

    /* Register to transport manager. */
    tp->base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    status = pjsip_transport_register( tp->base.tpmgr, (pjsip_transport*)tp);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create rdata and put it in the array. */
    tp->rdata_cnt = 0;
    for (i=0; i<async_cnt; ++i) {
	pj_pool_t *rdata_pool = pjsip_endpt_create_pool(endpt, "rtd%p", 
							PJSIP_POOL_LEN_RDATA,
							PJSIP_POOL_INC_RDATA);
	if (!rdata_pool) {
	    pj_atomic_set(tp->base.ref_cnt, 0);
	    pjsip_transport_unregister(tp->base.tpmgr, &tp->base);
	    return PJ_ENOMEM;
	}

	tp->rdata[i] = pj_pool_zalloc(rdata_pool, sizeof(pjsip_rx_data));
	tp->rdata[i]->tp_info.pool = rdata_pool;
	tp->rdata[i]->tp_info.transport = &tp->base;
	pj_ioqueue_op_key_init(&tp->rdata[i]->tp_info.op_key.op_key, 
			       sizeof(pj_ioqueue_op_key_t));

	tp->rdata_cnt++;
    }

    /* Start reading the ioqueue. */
    for (i=0; i<async_cnt; ++i) {
	pj_ssize_t size;

	size = sizeof(tp->rdata[i]->pkt_info.packet);
	tp->rdata[i]->pkt_info.addr_len = sizeof(tp->rdata[i]->pkt_info.addr);
	status = pj_ioqueue_recvfrom(tp->key, 
				     &tp->rdata[i]->tp_info.op_key.op_key,
				     tp->rdata[i]->pkt_info.packet,
				     &size, PJ_IOQUEUE_ALWAYS_ASYNC,
				     &tp->rdata[i]->pkt_info.addr,
				     &tp->rdata[i]->pkt_info.addr_len);
	if (status == PJ_SUCCESS) {
	    pj_assert(!"Shouldn't happen because PJ_IOQUEUE_ALWAYS_ASYNC!");
	    on_read_complete(tp->key, &tp->rdata[i]->tp_info.op_key.op_key, 
			     size);
	} else if (status != PJ_EPENDING) {
	    /* Error! */
	    pjsip_transport_unregister(tp->base.tpmgr, &tp->base);
	    return status;
	}
    }

    /* Done. */
    *p_transport = &tp->base;
    return PJ_SUCCESS;

on_error:
    transport_destroy((pjsip_transport*)tp);
    return status;
}



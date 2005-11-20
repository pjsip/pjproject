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
#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_private.h>
#include <pjsip/sip_errno.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/ioqueue.h>
#include <pj/hash.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/lock.h>


#define THIS_FILE    "transport"

/*
 * Transport manager.
 */
struct pjsip_tpmgr 
{
    pj_hash_table_t *table;
    pj_lock_t	    *lock;
    pjsip_endpoint  *endpt;
    pjsip_tpfactory  factory_list;
    void           (*msg_cb)(pjsip_endpoint*, pj_status_t, pjsip_rx_data*);
};



/*****************************************************************************
 *
 * GENERAL TRANSPORT (NAMES, TYPES, ETC.)
 *
 *****************************************************************************/

/*
 * Transport names.
 */
const struct
{
    pjsip_transport_type_e type;
    pj_uint16_t		   port;
    pj_str_t		   name;
    unsigned		   flag;
} transport_names[] = 
{
    { PJSIP_TRANSPORT_UNSPECIFIED, 0, {NULL, 0}, 0},
    { PJSIP_TRANSPORT_UDP, 5060, {"UDP", 3}, PJSIP_TRANSPORT_DATAGRAM},
    { PJSIP_TRANSPORT_TCP, 5060, {"TCP", 3}, PJSIP_TRANSPORT_RELIABLE},
    { PJSIP_TRANSPORT_TLS, 5061, {"TLS", 3}, PJSIP_TRANSPORT_RELIABLE | PJSIP_TRANSPORT_SECURE},
    { PJSIP_TRANSPORT_SCTP, 5060, {"SCTP", 4}, PJSIP_TRANSPORT_RELIABLE}
};


/*
 * Get transport type from name.
 */
PJ_DEF(pjsip_transport_type_e) 
pjsip_transport_get_type_from_name(const pj_str_t *name)
{
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(transport_names); ++i) {
	if (pj_stricmp(name, &transport_names[i].name) == 0) {
	    return transport_names[i].type;
	}
    }

    pj_assert(!"Invalid transport name");
    return PJSIP_TRANSPORT_UNSPECIFIED;
}


/*
 * Get the transport type for the specified flags.
 */
PJ_DEF(pjsip_transport_type_e) 
pjsip_transport_get_type_from_flag(unsigned flag)
{
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(transport_names); ++i) {
	if (transport_names[i].flag == flag) {
	    return transport_names[i].type;
	}
    }

    pj_assert(!"Invalid transport type");
    return PJSIP_TRANSPORT_UNSPECIFIED;
}

PJ_DEF(unsigned)
pjsip_transport_get_flag_from_type( pjsip_transport_type_e type )
{
    PJ_ASSERT_RETURN(type < PJ_ARRAY_SIZE(transport_names), 0);
    return transport_names[type].flag;
}

/*
 * Get the default SIP port number for the specified type.
 */
PJ_DEF(int) 
pjsip_transport_get_default_port_for_type(pjsip_transport_type_e type)
{
    PJ_ASSERT_RETURN(type < PJ_ARRAY_SIZE(transport_names), 5060);
    return transport_names[type].port;
}


/*****************************************************************************
 *
 * TRANSMIT DATA BUFFER MANIPULATION.
 *
 *****************************************************************************/

/*
 * Create new transmit buffer.
 */
PJ_DEF(pj_status_t) pjsip_tx_data_create( pjsip_tpmgr *mgr,
					  pjsip_tx_data **p_tdata )
{
    pj_pool_t *pool;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(mgr && p_tdata, PJ_EINVAL);

    PJ_LOG(5, ("", "pjsip_tx_data_create"));

    pool = pjsip_endpt_create_pool( mgr->endpt, "tdta%p",
				    PJSIP_POOL_LEN_TDATA,
				    PJSIP_POOL_INC_TDATA );
    if (!pool)
	return PJ_ENOMEM;

    tdata = pj_pool_zalloc(pool, sizeof(pjsip_tx_data));
    tdata->pool = pool;
    tdata->mgr = mgr;
    pj_snprintf(tdata->obj_name, PJ_MAX_OBJ_NAME, "tdta%p", tdata);

    status = pj_atomic_create(tdata->pool, 0, &tdata->ref_cnt);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_destroy_pool( mgr->endpt, tdata->pool );
	return status;
    }
    
    //status = pj_lock_create_simple_mutex(pool, "tdta%p", &tdata->lock);
    status = pj_lock_create_null_mutex(pool, "tdta%p", &tdata->lock);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_destroy_pool( mgr->endpt, tdata->pool );
	return status;
    }

    pj_ioqueue_op_key_init(&tdata->op_key.key, sizeof(tdata->op_key));

    *p_tdata = tdata;
    return PJ_SUCCESS;
}


/*
 * Add reference to tx buffer.
 */
PJ_DEF(void) pjsip_tx_data_add_ref( pjsip_tx_data *tdata )
{
    pj_atomic_inc(tdata->ref_cnt);
}

/*
 * Decrease transport data reference, destroy it when the reference count
 * reaches zero.
 */
PJ_DEF(void) pjsip_tx_data_dec_ref( pjsip_tx_data *tdata )
{
    pj_assert( pj_atomic_get(tdata->ref_cnt) > 0);
    if (pj_atomic_dec_and_get(tdata->ref_cnt) <= 0) {
	PJ_LOG(5,(tdata->obj_name, "destroying txdata"));
	pj_atomic_destroy( tdata->ref_cnt );
	pj_lock_destroy( tdata->lock );
	pjsip_endpt_destroy_pool( tdata->mgr->endpt, tdata->pool );
    }
}

/*
 * Invalidate the content of the print buffer to force the message to be
 * re-printed when sent.
 */
PJ_DEF(void) pjsip_tx_data_invalidate_msg( pjsip_tx_data *tdata )
{
    tdata->buf.cur = tdata->buf.start;
}

PJ_DEF(pj_bool_t) pjsip_tx_data_is_valid( pjsip_tx_data *tdata )
{
    return tdata->buf.cur != tdata->buf.start;
}



/*****************************************************************************
 *
 * TRANSPORT KEY
 *
 *****************************************************************************/

/*
 * Transport key for indexing in the hash table.
 */
typedef struct transport_key
{
    pj_uint8_t	type;
    pj_uint8_t	zero;
    pj_uint16_t	port;
    pj_uint32_t	addr;
} transport_key;


/*****************************************************************************
 *
 * TRANSPORT
 *
 *****************************************************************************/

static void transport_send_callback(pjsip_transport *transport,
				    void *token,
				    pj_ssize_t size)
{
    pjsip_tx_data *tdata = token;

    PJ_UNUSED_ARG(transport);

    /* Mark pending off so that app can resend/reuse txdata from inside
     * the callback.
     */
    tdata->is_pending = 0;

    /* Call callback, if any. */
    if (tdata->cb) {
	(*tdata->cb)(tdata->token, tdata, size);
    }

    /* Decrement reference count. */
    pjsip_tx_data_dec_ref(tdata);
}

/*
 * Send a SIP message using the specified transport.
 */
PJ_DEF(pj_status_t) pjsip_transport_send(  pjsip_transport *tr, 
					   pjsip_tx_data *tdata,
					   const pj_sockaddr_in *addr,
					   void *token,
					   void (*cb)(void *token, 
						      pjsip_tx_data *tdata,
						      pj_ssize_t))
{
    pj_status_t status;

    PJ_ASSERT_RETURN(tr && tdata && addr, PJ_EINVAL);

    /* Is it currently being sent? */
    if (tdata->is_pending) {
	pj_assert(!"Invalid operation step!");
	return PJSIP_EPENDINGTX;
    }

    /* Allocate buffer if necessary. */
    if (tdata->buf.start == NULL) {
	tdata->buf.start = pj_pool_alloc( tdata->pool, PJSIP_MAX_PKT_LEN);
	tdata->buf.cur = tdata->buf.start;
	tdata->buf.end = tdata->buf.start + PJSIP_MAX_PKT_LEN;
    }

    /* Do we need to reprint? */
    if (!pjsip_tx_data_is_valid(tdata)) {
	pj_ssize_t size;

	size = pjsip_msg_print( tdata->msg, tdata->buf.start, 
			        tdata->buf.end - tdata->buf.start);
	if (size < 0) {
	    return PJSIP_EMSGTOOLONG;
	}
	pj_assert(size != 0);
	tdata->buf.cur += size;
	tdata->buf.cur[size] = '\0';
    }

    /* Save callback data. */
    tdata->token = token;
    tdata->cb = cb;

    /* Add reference counter. */
    pjsip_tx_data_add_ref(tdata);

    /* Mark as pending. */
    tdata->is_pending = 1;

    /* Send to transport. */
    status = (*tr->send_msg)(tr, tdata,  addr, (void*)tdata, 
			     &transport_send_callback);

    if (status != PJ_EPENDING) {
	tdata->is_pending = 0;
	pjsip_tx_data_dec_ref(tdata);
    }

    return status;
}

static void transport_idle_callback(pj_timer_heap_t *timer_heap,
				    struct pj_timer_entry *entry)
{
    pjsip_transport *tp = entry->user_data;
    pj_assert(tp != NULL);

    PJ_UNUSED_ARG(timer_heap);

    entry->id = PJ_FALSE;
    pjsip_transport_unregister(tp->tpmgr, tp);
}

/*
 * Add ref.
 */
PJ_DEF(pj_status_t) pjsip_transport_add_ref( pjsip_transport *tp )
{
    PJ_ASSERT_RETURN(tp != NULL, PJ_EINVAL);

    if (pj_atomic_inc_and_get(tp->ref_cnt) == 1) {
	pj_lock_acquire(tp->tpmgr->lock);
	/* Verify again. */
	if (pj_atomic_get(tp->ref_cnt) == 1) {
	    if (tp->idle_timer.id != PJ_FALSE) {
		pjsip_endpt_cancel_timer(tp->tpmgr->endpt, &tp->idle_timer);
		tp->idle_timer.id = PJ_FALSE;
	    }
	}
	pj_lock_release(tp->tpmgr->lock);
    }

    return PJ_SUCCESS;
}

/*
 * Dec ref.
 */
PJ_DEF(pj_status_t) pjsip_transport_dec_ref( pjsip_transport *tp )
{
    PJ_ASSERT_RETURN(tp != NULL, PJ_EINVAL);

    pj_assert(pj_atomic_get(tp->ref_cnt) > 0);

    if (pj_atomic_dec_and_get(tp->ref_cnt) == 0) {
	pj_lock_acquire(tp->tpmgr->lock);
	/* Verify again. */
	if (pj_atomic_get(tp->ref_cnt) == 0) {
	    pj_time_val delay = { PJSIP_TRANSPORT_IDLE_TIME, 0 };

	    pj_assert(tp->idle_timer.id == 0);
	    tp->idle_timer.id = PJ_TRUE;
	    pjsip_endpt_schedule_timer(tp->tpmgr->endpt, &tp->idle_timer, 
				       &delay);
	}
	pj_lock_release(tp->tpmgr->lock);
    }

    return PJ_SUCCESS;
}


/**
 * Register a transport.
 */
PJ_DEF(pj_status_t) pjsip_transport_register( pjsip_tpmgr *mgr,
					      pjsip_transport *tp )
{
    transport_key key;

    /* Init. */
    tp->tpmgr = mgr;
    pj_memset(&tp->idle_timer, 0, sizeof(tp->idle_timer));
    tp->idle_timer.user_data = tp;
    tp->idle_timer.cb = &transport_idle_callback;

    /* 
     * Register to hash table.
     */
    key.type = (pj_uint8_t)tp->type;
    key.zero = 0;
    key.addr = pj_ntohl(tp->rem_addr.sin_addr.s_addr);
    key.port = pj_ntohs(tp->rem_addr.sin_port);

    pj_lock_acquire(mgr->lock);
    pj_hash_set(tp->pool, mgr->table, &key, sizeof(key), tp);
    pj_lock_release(mgr->lock);

    return PJ_SUCCESS;
}


/**
 * Unregister transport.
 */
PJ_DEF(pj_status_t) pjsip_transport_unregister( pjsip_tpmgr *mgr,
						pjsip_transport *tp)
{
    transport_key key;

    PJ_ASSERT_RETURN(pj_atomic_get(tp->ref_cnt) == 0, PJSIP_EBUSY);

    pj_lock_acquire(tp->lock);
    pj_lock_acquire(mgr->lock);

    /*
     * Unregister timer, if any.
     */
    pj_assert(tp->idle_timer.id == PJ_FALSE);
    if (tp->idle_timer.id != PJ_FALSE) {
	pjsip_endpt_cancel_timer(mgr->endpt, &tp->idle_timer);
	tp->idle_timer.id = PJ_FALSE;
    }

    /*
     * Unregister from hash table.
     */
    key.type = (pj_uint8_t)tp->type;
    key.zero = 0;
    key.addr = pj_ntohl(tp->rem_addr.sin_addr.s_addr);
    key.port = pj_ntohs(tp->rem_addr.sin_port);

    pj_hash_set(tp->pool, mgr->table, &key, sizeof(key), NULL);

    pj_lock_release(mgr->lock);

    /* Destroy. */
    return tp->destroy(tp);
}



/*****************************************************************************
 *
 * TRANSPORT FACTORY
 *
 *****************************************************************************/


PJ_DEF(pj_status_t) pjsip_tpmgr_register_tpfactory( pjsip_tpmgr *mgr,
						    pjsip_tpfactory *tpf)
{
    pjsip_tpfactory *p;
    pj_status_t status;

    pj_lock_acquire(mgr->lock);

    /* Check that no factory with the same type has been registered. */
    status = PJ_SUCCESS;
    for (p=mgr->factory_list.next; p!=&mgr->factory_list; p=p->next) {
	if (p->type == tpf->type) {
	    status = PJSIP_ETYPEEXISTS;
	    break;
	}
	if (p == tpf) {
	    status = PJ_EEXISTS;
	    break;
	}
    }

    if (status != PJ_SUCCESS) {
	pj_lock_release(mgr->lock);
	return status;
    }

    pj_list_insert_before(&mgr->factory_list, tpf);

    pj_lock_release(mgr->lock);

    return PJ_SUCCESS;
}


/**
 * Unregister factory.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_unregister_tpfactory( pjsip_tpmgr *mgr,
						      pjsip_tpfactory *tpf)
{
    pj_lock_acquire(mgr->lock);

    pj_assert(pj_list_find_node(&mgr->factory_list, tpf) == tpf);
    pj_list_erase(tpf);

    pj_lock_release(mgr->lock);

    return PJ_SUCCESS;
}


/*****************************************************************************
 *
 * TRANSPORT MANAGER
 *
 *****************************************************************************/

/*
 * Create a new transport manager.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_create( pj_pool_t *pool,
					pjsip_endpoint *endpt,
					void (*cb)(pjsip_endpoint*,
						   pj_status_t,
						   pjsip_rx_data *),
					pjsip_tpmgr **p_mgr)
{
    pjsip_tpmgr *mgr;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && endpt && cb && p_mgr, PJ_EINVAL);

    PJ_LOG(5, (THIS_FILE, "pjsip_tpmgr_create()"));

    mgr = pj_pool_zalloc(pool, sizeof(*mgr));
    mgr->endpt = endpt;
    mgr->msg_cb = cb;
    pj_list_init(&mgr->factory_list);

    mgr->table = pj_hash_create(pool, PJSIP_TPMGR_HTABLE_SIZE);
    if (!mgr->table)
	return PJ_ENOMEM;

    status = pj_lock_create_recursive_mutex(pool, "tmgr%p", &mgr->lock);
    if (status != PJ_SUCCESS)
	return status;

    *p_mgr = mgr;
    return PJ_SUCCESS;
}

/*
 * pjsip_tpmgr_destroy()
 *
 * Destroy transport manager.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_destroy( pjsip_tpmgr *mgr )
{
    pj_hash_iterator_t itr_val;
    pj_hash_iterator_t *itr;
    
    PJ_LOG(5, (THIS_FILE, "pjsip_tpmgr_destroy()"));

    pj_lock_acquire(mgr->lock);

    itr = pj_hash_first(mgr->table, &itr_val);
    while (itr != NULL) {
	pj_hash_iterator_t *next;
	pjsip_transport *transport;
	
	transport = pj_hash_this(mgr->table, itr);

	next = pj_hash_next(mgr->table, itr);

	pj_atomic_set(transport->ref_cnt, 0);
	pjsip_transport_unregister(mgr, transport);

	itr = next;
    }

    pj_lock_release(mgr->lock);

    return PJ_SUCCESS;
}


/*
 * pjsip_tpmgr_receive_packet()
 *
 * Called by tranports when they receive a new packet.
 */
PJ_DEF(pj_ssize_t) pjsip_tpmgr_receive_packet( pjsip_tpmgr *mgr,
					       pjsip_rx_data *rdata)
{
    pjsip_transport *tr = rdata->tp_info.transport;
    pj_str_t s;

    char *current_pkt;
    pj_size_t remaining_len;
    pj_size_t total_processed = 0;

    /* Check size. */
    pj_assert(rdata->pkt_info.len > 0);
    if (rdata->pkt_info.len <= 0)
	return -1;
    
    current_pkt = rdata->pkt_info.packet;
    remaining_len = rdata->pkt_info.len;
    
    /* Must NULL terminate buffer. This is the requirement of the 
     * parser etc. 
     */
    current_pkt[remaining_len] = '\0';

    /* Process all message fragments. */
    while (total_processed < remaining_len) {

	pjsip_msg *msg;
	pj_size_t msg_fragment_size = 0;

	/* Initialize default fragment size. */
	msg_fragment_size = remaining_len;

	/* Null terminate packet. */

	/* Clear and init msg_info in rdata. 
	 * Endpoint might inspect the values there when we call the callback
	 * to report some errors.
	 */
	pj_memset(&rdata->msg_info, 0, sizeof(rdata->msg_info));
	pj_list_init(&rdata->msg_info.parse_err);
	rdata->msg_info.msg_buf = current_pkt;
	rdata->msg_info.len = remaining_len;

	/* For TCP transport, check if the whole message has been received. */
	if ((tr->flag & PJSIP_TRANSPORT_DATAGRAM) == 0) {
	    pj_status_t msg_status;
	    msg_status = pjsip_find_msg(current_pkt, remaining_len, PJ_FALSE, 
                                        &msg_fragment_size);
	    if (msg_status != PJ_SUCCESS) {
		if (remaining_len == PJSIP_MAX_PKT_LEN) {
		    mgr->msg_cb(mgr->endpt, PJSIP_ERXOVERFLOW, rdata);
		    /* Exhaust all data. */
		    return rdata->pkt_info.len;
		} else {
		    /* Not enough data in packet. */
		    return total_processed;
		}
	    }
	}

	/* Update msg_info. */
	rdata->msg_info.len = msg_fragment_size;

	/* Parse the message. */
	rdata->msg_info.msg = msg = 
	    pjsip_parse_rdata( current_pkt, msg_fragment_size, rdata);
	if (msg == NULL) {
	    mgr->msg_cb(mgr->endpt, PJSIP_EINVALIDMSG, rdata);
	    goto finish_process_fragment;
	}

	/* Perform basic header checking. */
	if (rdata->msg_info.call_id.ptr == NULL || 
	    rdata->msg_info.from == NULL || 
	    rdata->msg_info.to == NULL || 
	    rdata->msg_info.via == NULL || 
	    rdata->msg_info.cseq == NULL) 
	{
	    mgr->msg_cb(mgr->endpt, PJSIP_EMISSINGHDR, rdata);
	    goto finish_process_fragment;
	}

	/* If message is received from address that's different from sent-by,
  	 * MUST add received parameter to the via.
	 */
	s = pj_str(pj_inet_ntoa(rdata->pkt_info.addr.sin_addr));
	if (pj_strcmp(&s, &rdata->msg_info.via->sent_by.host) != 0) {
	    pj_strdup(rdata->tp_info.pool, 
		      &rdata->msg_info.via->recvd_param, &s);
	}

	/* RFC 3581:
	 * If message contains "rport" param, put the received port there.
	 */
	if (rdata->msg_info.via->rport_param == 0) {
	    rdata->msg_info.via->rport_param = 
		pj_ntohs(rdata->pkt_info.addr.sin_port);
	}

	/* Drop response message if it has more than one Via.
	*/
	if (msg->type == PJSIP_RESPONSE_MSG) {
	    pjsip_hdr *hdr;
	    hdr = (pjsip_hdr*)rdata->msg_info.via->next;
	    if (hdr != &msg->hdr) {
		hdr = pjsip_msg_find_hdr(msg, PJSIP_H_VIA, hdr);
		if (hdr) {
		    mgr->msg_cb(mgr->endpt, PJSIP_EMULTIPLEVIA, rdata);
		    goto finish_process_fragment;
		}
	    }
	}

	/* Call the transport manager's upstream message callback.
	 */
	mgr->msg_cb(mgr->endpt, PJ_SUCCESS, rdata);


finish_process_fragment:
	total_processed += msg_fragment_size;
	current_pkt += msg_fragment_size;
	remaining_len -= msg_fragment_size;

    }	/* while (rdata->pkt_info.len > 0) */


    return total_processed;
}


/*
 * pjsip_tpmgr_alloc_transport()
 *
 * Get transport suitable to communicate to remote. Create a new one
 * if necessary.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_alloc_transport( pjsip_tpmgr *mgr,
						 pjsip_transport_type_e type,
						 const pj_sockaddr_in *remote,
						 pjsip_transport **p_transport)
{
    transport_key key;
    pjsip_transport *transport;
    pjsip_tpfactory *factory;
    pj_status_t status;

    pj_lock_acquire(mgr->lock);

    /* First try to get exact destination. */
    key.type = (pj_uint8_t)type;
    key.zero = 0;
    key.addr = pj_ntohl(remote->sin_addr.s_addr);
    key.port = pj_ntohs(remote->sin_port);

    transport = pj_hash_get(mgr->table, &key, sizeof(key));
    if (transport != NULL) {
	unsigned flag = pjsip_transport_get_flag_from_type(type);
	
	/* For datagram transports, try lookup with zero address. */
	if (flag & PJSIP_TRANSPORT_DATAGRAM) {
	    key.addr = 0;
	    key.port = 0;

	    transport = pj_hash_get(mgr->table, &key, sizeof(key));
	}
    }
    
    if (transport != NULL) {
	/*
	 * Transport found!
	 */
	pjsip_transport_add_ref(transport);
	pj_lock_release(mgr->lock);
	*p_transport = transport;
	return PJ_SUCCESS;
    }

    /*
     * Transport not found!
     * Find factory that can create such transport.
     */
    factory = mgr->factory_list.next;
    while (factory != &mgr->factory_list) {
	if (factory->type == type)
	    break;
	factory = factory->next;
    }

    if (factory == &mgr->factory_list) {
	/* No factory can create the transport! */
	pj_lock_release(mgr->lock);
	return PJSIP_EUNSUPTRANSPORT;
    }

    /* Request factory to create transport. */
    status = factory->create_transport(factory, mgr, mgr->endpt,
				       remote, p_transport);

    pj_lock_release(mgr->lock);
    return status;
}

/**
 * Dump transport info.
 */
PJ_DEF(void) pjsip_tpmgr_dump_transports(pjsip_tpmgr *mgr)
{
#if PJ_LOG_MAX_LEVEL >= 3
    pj_hash_iterator_t itr_val;
    pj_hash_iterator_t *itr;

    pj_lock_acquire(mgr->lock);

    itr = pj_hash_first(mgr->table, &itr_val);
    if (itr) {
	PJ_LOG(3, (THIS_FILE, " Dumping transports:"));

	do {
	    char src_addr[128], dst_addr[128];
	    int src_port, dst_port;
	    pjsip_transport *t;

	    t = pj_hash_this(mgr->table, itr);
	    pj_native_strcpy(src_addr, pj_inet_ntoa(t->local_addr.sin_addr));
	    src_port = pj_ntohs(t->local_addr.sin_port);

	    pj_native_strcpy(dst_addr, pj_inet_ntoa(t->rem_addr.sin_addr));
	    dst_port = pj_ntohs(t->rem_addr.sin_port);

	    PJ_LOG(3, (THIS_FILE, "  %s %s %s:%d --> %s:%d (refcnt=%d)", 
		       t->type_name,
		       t->obj_name,
		       src_addr, src_port,
		       dst_addr, dst_port,
		       pj_atomic_get(t->ref_cnt)));

	    itr = pj_hash_next(mgr->table, itr);
	} while (itr);
    }

    pj_lock_release(mgr->lock);
#endif
}


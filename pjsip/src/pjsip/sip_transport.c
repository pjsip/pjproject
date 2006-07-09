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
#include <pjsip/sip_module.h>
#include <pj/except.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/ioqueue.h>
#include <pj/hash.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/lock.h>


#define THIS_FILE    "sip_transport.c"

#if 0
#   define TRACE_(x)	PJ_LOG(5,x)
#else
#   define TRACE_(x)
#endif

/* Prototype. */
static pj_status_t mod_on_tx_msg(pjsip_tx_data *tdata);

/* This module has sole purpose to print transmit data to contigous buffer
 * before actually transmitted to the wire. 
 */
static pjsip_module mod_msg_print = 
{
    NULL, NULL,				/* prev and next		    */
    { "mod-msg-print", 13},		/* Name.			    */
    -1,					/* Id				    */
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER,	/* Priority			    */
    NULL,				/* load()			    */
    NULL,				/* start()			    */
    NULL,				/* stop()			    */
    NULL,				/* unload()			    */
    NULL,				/* on_rx_request()		    */
    NULL,				/* on_rx_response()		    */
    &mod_on_tx_msg,			/* on_tx_request()		    */
    &mod_on_tx_msg,			/* on_tx_response()		    */
    NULL,				/* on_tsx_state()		    */
};

/*
 * Transport manager.
 */
struct pjsip_tpmgr 
{
    pj_hash_table_t *table;
    pj_lock_t	    *lock;
    pjsip_endpoint  *endpt;
    pjsip_tpfactory  factory_list;
#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    pj_atomic_t	    *tdata_counter;
#endif
    void           (*on_rx_msg)(pjsip_endpoint*, pj_status_t, pjsip_rx_data*);
    pj_status_t	   (*on_tx_msg)(pjsip_endpoint*, pjsip_tx_data*);
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
    { PJSIP_TRANSPORT_UNSPECIFIED, 0, {"Unspecified", 11}, 0},
    { PJSIP_TRANSPORT_UDP, 5060, {"UDP", 3}, PJSIP_TRANSPORT_DATAGRAM},
    { PJSIP_TRANSPORT_TCP, 5060, {"TCP", 3}, PJSIP_TRANSPORT_RELIABLE},
    { PJSIP_TRANSPORT_TLS, 5061, {"TLS", 3}, PJSIP_TRANSPORT_RELIABLE | PJSIP_TRANSPORT_SECURE},
    { PJSIP_TRANSPORT_SCTP, 5060, {"SCTP", 4}, PJSIP_TRANSPORT_RELIABLE},
    { PJSIP_TRANSPORT_LOOP, 15060, {"LOOP", 4}, PJSIP_TRANSPORT_RELIABLE}, 
    { PJSIP_TRANSPORT_LOOP_DGRAM, 15060, {"LOOP-DGRAM", 10}, PJSIP_TRANSPORT_DATAGRAM},
};


/*
 * Get transport type from name.
 */
PJ_DEF(pjsip_transport_type_e) 
pjsip_transport_get_type_from_name(const pj_str_t *name)
{
    unsigned i;

    /* Sanity check. 
     * Check that transport_names[] are indexed on transport type. 
     */
    PJ_ASSERT_RETURN(transport_names[PJSIP_TRANSPORT_UDP].type ==
		     PJSIP_TRANSPORT_UDP, PJSIP_TRANSPORT_UNSPECIFIED);

    if (name->slen == 0)
	return PJSIP_TRANSPORT_UNSPECIFIED;

    /* Get transport type from name. */
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

    /* Sanity check. 
     * Check that transport_names[] are indexed on transport type. 
     */
    PJ_ASSERT_RETURN(transport_names[PJSIP_TRANSPORT_UDP].type ==
		     PJSIP_TRANSPORT_UDP, PJSIP_TRANSPORT_UNSPECIFIED);

    /* Get the transport type for the specified flags. */
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
    /* Sanity check. 
     * Check that transport_names[] are indexed on transport type. 
     */
    PJ_ASSERT_RETURN(transport_names[PJSIP_TRANSPORT_UDP].type ==
		     PJSIP_TRANSPORT_UDP, 0);

    /* Check that argument is valid. */
    PJ_ASSERT_RETURN(type < PJ_ARRAY_SIZE(transport_names), 0);

    /* Return transport flag. */
    return transport_names[type].flag;
}

/*
 * Get the default SIP port number for the specified type.
 */
PJ_DEF(int) 
pjsip_transport_get_default_port_for_type(pjsip_transport_type_e type)
{
    /* Sanity check. 
     * Check that transport_names[] are indexed on transport type. 
     */
    PJ_ASSERT_RETURN(transport_names[PJSIP_TRANSPORT_UDP].type ==
		     PJSIP_TRANSPORT_UDP, 0);

    /* Check that argument is valid. */
    PJ_ASSERT_RETURN(type < PJ_ARRAY_SIZE(transport_names), 5060);

    /* Return the port. */
    return transport_names[type].port;
}

/*
 * Get transport name.
 */
PJ_DEF(const char*) pjsip_transport_get_type_name(pjsip_transport_type_e type)
{
    /* Sanity check. 
     * Check that transport_names[] are indexed on transport type. 
     */
    PJ_ASSERT_RETURN(transport_names[PJSIP_TRANSPORT_UDP].type ==
		     PJSIP_TRANSPORT_UDP, "Unknown");

    /* Check that argument is valid. */
    PJ_ASSERT_RETURN(type < PJ_ARRAY_SIZE(transport_names), "Unknown");

    /* Return the port. */
    return transport_names[type].name.ptr;
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

    pool = pjsip_endpt_create_pool( mgr->endpt, "tdta%p",
				    PJSIP_POOL_LEN_TDATA,
				    PJSIP_POOL_INC_TDATA );
    if (!pool)
	return PJ_ENOMEM;

    tdata = pj_pool_zalloc(pool, sizeof(pjsip_tx_data));
    tdata->pool = pool;
    tdata->mgr = mgr;
    pj_ansi_snprintf(tdata->obj_name, PJ_MAX_OBJ_NAME, "tdta%p", tdata);

    status = pj_atomic_create(tdata->pool, 0, &tdata->ref_cnt);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_release_pool( mgr->endpt, tdata->pool );
	return status;
    }
    
    //status = pj_lock_create_simple_mutex(pool, "tdta%p", &tdata->lock);
    status = pj_lock_create_null_mutex(pool, "tdta%p", &tdata->lock);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_release_pool( mgr->endpt, tdata->pool );
	return status;
    }

    pj_ioqueue_op_key_init(&tdata->op_key.key, sizeof(tdata->op_key));

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    pj_atomic_inc( tdata->mgr->tdata_counter );
#endif

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
PJ_DEF(pj_status_t) pjsip_tx_data_dec_ref( pjsip_tx_data *tdata )
{
    pj_assert( pj_atomic_get(tdata->ref_cnt) > 0);
    if (pj_atomic_dec_and_get(tdata->ref_cnt) <= 0) {
	PJ_LOG(5,(tdata->obj_name, "Destroying txdata %s",
		  pjsip_tx_data_get_info(tdata)));
#if defined(PJ_DEBUG) && PJ_DEBUG!=0
	pj_atomic_dec( tdata->mgr->tdata_counter );
#endif
	pj_atomic_destroy( tdata->ref_cnt );
	pj_lock_destroy( tdata->lock );
	pjsip_endpt_release_pool( tdata->mgr->endpt, tdata->pool );
	return PJSIP_EBUFDESTROYED;
    } else {
	return PJ_SUCCESS;
    }
}

/*
 * Invalidate the content of the print buffer to force the message to be
 * re-printed when sent.
 */
PJ_DEF(void) pjsip_tx_data_invalidate_msg( pjsip_tx_data *tdata )
{
    tdata->buf.cur = tdata->buf.start;
    tdata->info = NULL;
}

PJ_DEF(pj_bool_t) pjsip_tx_data_is_valid( pjsip_tx_data *tdata )
{
    return tdata->buf.cur != tdata->buf.start;
}

static char *get_msg_info(pj_pool_t *pool, const char *obj_name,
			  const pjsip_msg *msg)
{
    char info_buf[128], *info;
    const pjsip_cseq_hdr *cseq;
    int len;

    cseq = pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    PJ_ASSERT_RETURN(cseq != NULL, "INVALID MSG");

    if (msg->type == PJSIP_REQUEST_MSG) {
	len = pj_ansi_snprintf(info_buf, sizeof(info_buf), 
			       "Request msg %.*s/cseq=%d (%s)",
			       (int)msg->line.req.method.name.slen,
			       msg->line.req.method.name.ptr,
			       cseq->cseq, obj_name);
    } else {
	len = pj_ansi_snprintf(info_buf, sizeof(info_buf),
			       "Response msg %d/%.*s/cseq=%d (%s)",
			       msg->line.status.code,
			       (int)cseq->method.name.slen,
			       cseq->method.name.ptr,
			       cseq->cseq, obj_name);
    }

    if (len < 1 || len >= sizeof(info_buf)) {
	return (char*)obj_name;
    }

    info = pj_pool_alloc(pool, len+1);
    pj_memcpy(info, info_buf, len+1);

    return info;
}

PJ_DEF(char*) pjsip_tx_data_get_info( pjsip_tx_data *tdata )
{

    if (tdata==NULL || tdata->msg==NULL)
	return "NULL";

    if (tdata->info)
	return tdata->info;

    pj_lock_acquire(tdata->lock);
    tdata->info = get_msg_info(tdata->pool, tdata->obj_name, tdata->msg);
    pj_lock_release(tdata->lock);

    return tdata->info;
}

PJ_DEF(char*) pjsip_rx_data_get_info(pjsip_rx_data *rdata)
{
    char obj_name[16];

    PJ_ASSERT_RETURN(rdata->msg_info.msg, "INVALID MSG");

    if (rdata->msg_info.info)
	return rdata->msg_info.info;

    pj_ansi_strcpy(obj_name, "rdata");
    pj_ansi_sprintf(obj_name+5, "%p", rdata);

    rdata->msg_info.info = get_msg_info(rdata->tp_info.pool, obj_name,
					rdata->msg_info.msg);
    return rdata->msg_info.info;
}

/*****************************************************************************
 *
 * TRANSPORT KEY
 *
 *****************************************************************************/


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

/* This function is called by endpoint for on_tx_request() and on_tx_response()
 * notification.
 */
static pj_status_t mod_on_tx_msg(pjsip_tx_data *tdata)
{
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
	tdata->buf.cur[size] = '\0';
	tdata->buf.cur += size;
    }

    return PJ_SUCCESS;
}

/*
 * Send a SIP message using the specified transport.
 */
PJ_DEF(pj_status_t) pjsip_transport_send(  pjsip_transport *tr, 
					   pjsip_tx_data *tdata,
					   const pj_sockaddr_t *addr,
					   int addr_len,
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

    /* Fill in tp_info. */
    tdata->tp_info.transport = tr;
    pj_memcpy(&tdata->tp_info.dst_addr, addr, addr_len);
    tdata->tp_info.dst_addr_len = addr_len;
    if (((pj_sockaddr*)addr)->sa_family == PJ_AF_INET) {
	const char *str_addr;
	str_addr = pj_inet_ntoa(((pj_sockaddr_in*)addr)->sin_addr);
	pj_ansi_strcpy(tdata->tp_info.dst_name, str_addr);
	tdata->tp_info.dst_port = pj_ntohs(((pj_sockaddr_in*)addr)->sin_port);
    } else {
	pj_ansi_strcpy(tdata->tp_info.dst_name, "<unknown>");
	tdata->tp_info.dst_port = 0;
    }

    /* Distribute to modules. 
     * When the message reach mod_msg_print, the contents of the message will
     * be "printed" to contiguous buffer.
     */
    if (tr->tpmgr->on_tx_msg) {
	status = (*tr->tpmgr->on_tx_msg)(tr->endpt, tdata);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Save callback data. */
    tdata->token = token;
    tdata->cb = cb;

    /* Add reference counter. */
    pjsip_tx_data_add_ref(tdata);

    /* Mark as pending. */
    tdata->is_pending = 1;

    /* Send to transport. */
    status = (*tr->send_msg)(tr, tdata,  addr, addr_len, (void*)tdata, 
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
    pjsip_transport_destroy(tp);
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
	    pj_time_val delay;
	    
	    /* If transport is in graceful shutdown, then this is the
	     * last user who uses the transport. Schedule to destroy the
	     * transport immediately. Otherwise schedule idle timer.
	     */
	    if (tp->is_shutdown) {
		delay.sec = delay.msec = 0;
	    } else {
		delay.sec = PJSIP_TRANSPORT_IDLE_TIME;
		delay.msec = 0;
	    }

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
    int key_len;

    /* Init. */
    tp->tpmgr = mgr;
    pj_bzero(&tp->idle_timer, sizeof(tp->idle_timer));
    tp->idle_timer.user_data = tp;
    tp->idle_timer.cb = &transport_idle_callback;

    /* 
     * Register to hash table.
     */
    key_len = sizeof(tp->key.type) + tp->addr_len;
    pj_lock_acquire(mgr->lock);
    pj_hash_set(tp->pool, mgr->table, &tp->key, key_len, 0, tp);
    pj_lock_release(mgr->lock);

    TRACE_((THIS_FILE,"Transport %s registered: type=%s, remote=%s:%d",
		       tp->obj_name,
		       pjsip_transport_get_type_name(tp->key.type),
		       pj_inet_ntoa(((pj_sockaddr_in*)&tp->key.rem_addr)->sin_addr),
		       pj_ntohs(((pj_sockaddr_in*)&tp->key.rem_addr)->sin_port)));

    return PJ_SUCCESS;
}

/* Force destroy transport (e.g. during transport manager shutdown. */
static pj_status_t destroy_transport( pjsip_tpmgr *mgr,
				      pjsip_transport *tp )
{
    int key_len;

    TRACE_((THIS_FILE, "Transport %s is being destroyed", tp->obj_name));

    pj_lock_acquire(tp->lock);
    pj_lock_acquire(mgr->lock);

    /*
     * Unregister timer, if any.
     */
    //pj_assert(tp->idle_timer.id == PJ_FALSE);
    if (tp->idle_timer.id != PJ_FALSE) {
	pjsip_endpt_cancel_timer(mgr->endpt, &tp->idle_timer);
	tp->idle_timer.id = PJ_FALSE;
    }

    /*
     * Unregister from hash table.
     */
    key_len = sizeof(tp->key.type) + tp->addr_len;
    pj_assert(pj_hash_get(mgr->table, &tp->key, key_len, NULL) != NULL);
    pj_hash_set(tp->pool, mgr->table, &tp->key, key_len, 0, NULL);

    pj_lock_release(mgr->lock);

    /* Destroy. */
    return tp->destroy(tp);
}


/*
 * Start graceful shutdown procedure for this transport. 
 */
PJ_DEF(pj_status_t) pjsip_transport_shutdown(pjsip_transport *tp)
{
    pjsip_tpmgr *mgr;
    pj_status_t status;

    TRACE_((THIS_FILE, "Transport %s shutting down", tp->obj_name));

    pj_lock_acquire(tp->lock);

    mgr = tp->tpmgr;
    pj_lock_acquire(mgr->lock);

    /* Do nothing if transport is being shutdown already */
    if (tp->is_shutdown) {
	pj_lock_release(tp->lock);
	pj_lock_release(mgr->lock);
	return PJ_SUCCESS;
    }

    status = PJ_SUCCESS;

    /* Instruct transport to shutdown itself */
    if (tp->do_shutdown)
	status = tp->do_shutdown(tp);
    
    if (status == PJ_SUCCESS)
	tp->is_shutdown = PJ_TRUE;

    pj_lock_release(tp->lock);
    pj_lock_release(mgr->lock);

    return status;
}


/**
 * Unregister transport.
 */
PJ_DEF(pj_status_t) pjsip_transport_destroy( pjsip_transport *tp)
{
    /* Must have no user. */
    PJ_ASSERT_RETURN(pj_atomic_get(tp->ref_cnt) == 0, PJSIP_EBUSY);

    /* Destroy. */
    return destroy_transport(tp->tpmgr, tp);
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
					void (*rx_cb)(pjsip_endpoint*,
						      pj_status_t,
						      pjsip_rx_data *),
					pj_status_t (*tx_cb)(pjsip_endpoint*,
							     pjsip_tx_data*),
					pjsip_tpmgr **p_mgr)
{
    pjsip_tpmgr *mgr;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && endpt && rx_cb && p_mgr, PJ_EINVAL);

    /* Register mod_msg_print module. */
    status = pjsip_endpt_register_module(endpt, &mod_msg_print);
    if (status != PJ_SUCCESS)
	return status;

    /* Create and initialize transport manager. */
    mgr = pj_pool_zalloc(pool, sizeof(*mgr));
    mgr->endpt = endpt;
    mgr->on_rx_msg = rx_cb;
    mgr->on_tx_msg = tx_cb;
    pj_list_init(&mgr->factory_list);

    mgr->table = pj_hash_create(pool, PJSIP_TPMGR_HTABLE_SIZE);
    if (!mgr->table)
	return PJ_ENOMEM;

    status = pj_lock_create_recursive_mutex(pool, "tmgr%p", &mgr->lock);
    if (status != PJ_SUCCESS)
	return status;

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    status = pj_atomic_create(pool, 0, &mgr->tdata_counter);
    if (status != PJ_SUCCESS)
	return status;
#endif

    PJ_LOG(5, (THIS_FILE, "Transport manager created."));

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
    pjsip_tpfactory *factory;
    pjsip_endpoint *endpt = mgr->endpt;
    
    PJ_LOG(5, (THIS_FILE, "Destroying transport manager"));

    pj_lock_acquire(mgr->lock);

    /*
     * Destroy all transports.
     */
    itr = pj_hash_first(mgr->table, &itr_val);
    while (itr != NULL) {
	pj_hash_iterator_t *next;
	pjsip_transport *transport;
	
	transport = pj_hash_this(mgr->table, itr);

	next = pj_hash_next(mgr->table, itr);

	destroy_transport(mgr, transport);

	itr = next;
    }

    /*
     * Destroy all factories/listeners.
     */
    factory = mgr->factory_list.next;
    while (factory != &mgr->factory_list) {
	pjsip_tpfactory *next = factory->next;
	
	factory->destroy(factory);

	factory = next;
    }

    pj_lock_release(mgr->lock);
    pj_lock_destroy(mgr->lock);

    /* Unregister mod_msg_print. */
    if (mod_msg_print.id != -1) {
	pjsip_endpt_unregister_module(endpt, &mod_msg_print);
    }

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    /* If you encounter assert error on this line, it means there are
     * leakings in transmit data (i.e. some transmit data have not been
     * destroyed).
     */
    //pj_assert(pj_atomic_get(mgr->tdata_counter) == 0);
    if (pj_atomic_get(mgr->tdata_counter) != 0) {
	PJ_LOG(3,(THIS_FILE, "Warning: %d transmit buffer(s) not freed!",
		  pj_atomic_get(mgr->tdata_counter)));
    }
#endif

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
    while (remaining_len > 0) {

	pjsip_msg *msg;
	pj_size_t msg_fragment_size;

	/* Initialize default fragment size. */
	msg_fragment_size = remaining_len;

	/* Null terminate packet. */

	/* Clear and init msg_info in rdata. 
	 * Endpoint might inspect the values there when we call the callback
	 * to report some errors.
	 */
	pj_bzero(&rdata->msg_info, sizeof(rdata->msg_info));
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
		    mgr->on_rx_msg(mgr->endpt, PJSIP_ERXOVERFLOW, rdata);
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

	/* Check for parsing syntax error */
	if (msg==NULL || !pj_list_empty(&rdata->msg_info.parse_err)) {
	    pjsip_parser_err_report *err;
	    char buf[128];
	    pj_str_t tmp;

	    /* Gather syntax error information */
	    tmp.ptr = buf; tmp.slen = 0;
	    err = rdata->msg_info.parse_err.next;
	    while (err != &rdata->msg_info.parse_err) {
		int len;
		len = pj_ansi_snprintf(tmp.ptr+tmp.slen, sizeof(buf)-tmp.slen,
				       ": %s exception when parsing %.*s "
				       "header on line %d col %d",
				       pj_exception_id_name(err->except_code),
				       (int)err->hname.slen, err->hname.ptr,
				       err->line, err->col);
		if (len > 0 && len < (int) (sizeof(buf)-tmp.slen)) {
		    tmp.slen += len;
		}
		err = err->next;
	    }

	    PJ_LOG(1, (THIS_FILE, 
		      "Error processing %d bytes packet from %s:%d %.*s:\n"
		      "%.*s\n"
		      "-- end of packet.",
		      msg_fragment_size,
		      rdata->pkt_info.src_name, 
		      rdata->pkt_info.src_port,
		      (int)tmp.slen, tmp.ptr,
		      (int)msg_fragment_size,
		      rdata->msg_info.msg_buf));

	    goto finish_process_fragment;
	}

	/* Perform basic header checking. */
	if (rdata->msg_info.cid == NULL ||
	    rdata->msg_info.cid->id.slen == 0 || 
	    rdata->msg_info.from == NULL || 
	    rdata->msg_info.to == NULL || 
	    rdata->msg_info.via == NULL || 
	    rdata->msg_info.cseq == NULL) 
	{
	    mgr->on_rx_msg(mgr->endpt, PJSIP_EMISSINGHDR, rdata);
	    goto finish_process_fragment;
	}

	/* Always add received parameter to the via. */
	pj_strdup2(rdata->tp_info.pool, 
		   &rdata->msg_info.via->recvd_param, 
		   rdata->pkt_info.src_name);

	/* RFC 3581:
	 * If message contains "rport" param, put the received port there.
	 */
	if (rdata->msg_info.via->rport_param == 0) {
	    rdata->msg_info.via->rport_param = rdata->pkt_info.src_port;
	}

	/* Drop response message if it has more than one Via.
	*/
	if (msg->type == PJSIP_RESPONSE_MSG) {
	    pjsip_hdr *hdr;
	    hdr = (pjsip_hdr*)rdata->msg_info.via->next;
	    if (hdr != &msg->hdr) {
		hdr = pjsip_msg_find_hdr(msg, PJSIP_H_VIA, hdr);
		if (hdr) {
		    mgr->on_rx_msg(mgr->endpt, PJSIP_EMULTIPLEVIA, rdata);
		    goto finish_process_fragment;
		}
	    }
	}

	/* Call the transport manager's upstream message callback.
	 */
	mgr->on_rx_msg(mgr->endpt, PJ_SUCCESS, rdata);


finish_process_fragment:
	total_processed += msg_fragment_size;
	current_pkt += msg_fragment_size;
	remaining_len -= msg_fragment_size;

    }	/* while (rdata->pkt_info.len > 0) */


    return total_processed;
}


/*
 * pjsip_tpmgr_acquire_transport()
 *
 * Get transport suitable to communicate to remote. Create a new one
 * if necessary.
 */
PJ_DEF(pj_status_t) pjsip_tpmgr_acquire_transport(pjsip_tpmgr *mgr,
						  pjsip_transport_type_e type,
						  const pj_sockaddr_t *remote,
						  int addr_len,
						  pjsip_transport **tp)
{
    struct transport_key
    {
	pjsip_transport_type_e	type;
	pj_sockaddr		addr;
    } key;
    int key_len;
    pjsip_transport *transport;
    pjsip_tpfactory *factory;
    pj_status_t status;

    TRACE_((THIS_FILE,"Acquiring transport type=%s, remote=%s:%d",
		       pjsip_transport_get_type_name(type),
		       pj_inet_ntoa(((pj_sockaddr_in*)remote)->sin_addr),
		       pj_ntohs(((pj_sockaddr_in*)remote)->sin_port)));

    pj_lock_acquire(mgr->lock);

    key_len = sizeof(key.type) + addr_len;

    /* First try to get exact destination. */
    key.type = type;
    pj_memcpy(&key.addr, remote, addr_len);

    transport = pj_hash_get(mgr->table, &key, key_len, NULL);
    if (transport == NULL) {
	unsigned flag = pjsip_transport_get_flag_from_type(type);
	const pj_sockaddr *remote_addr = (const pj_sockaddr*)remote;

	/* Ignore address for loop transports. */
	if (type == PJSIP_TRANSPORT_LOOP ||
	         type == PJSIP_TRANSPORT_LOOP_DGRAM)
	{
	    pj_sockaddr_in *addr = (pj_sockaddr_in*)&key.addr;

	    pj_bzero(addr, sizeof(pj_sockaddr_in));
	    key_len = sizeof(key.type) + sizeof(pj_sockaddr_in);
	    transport = pj_hash_get(mgr->table, &key, key_len, NULL);
	}
	/* For datagram INET transports, try lookup with zero address.
	 */
	else if ((flag & PJSIP_TRANSPORT_DATAGRAM) && 
	         (remote_addr->sa_family == PJ_AF_INET)) 
	{
	    pj_sockaddr_in *addr = (pj_sockaddr_in*)&key.addr;

	    pj_bzero(addr, sizeof(pj_sockaddr_in));
	    addr->sin_family = PJ_AF_INET;

	    key_len = sizeof(key.type) + sizeof(pj_sockaddr_in);
	    transport = pj_hash_get(mgr->table, &key, key_len, NULL);
	}
    }
    
    if (transport!=NULL && !transport->is_shutdown) {
	/*
	 * Transport found!
	 */
	pjsip_transport_add_ref(transport);
	pj_lock_release(mgr->lock);
	*tp = transport;

	TRACE_((THIS_FILE, "Transport %s acquired", transport->obj_name));
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
	TRACE_((THIS_FILE, "No suitable factory was found either"));
	return PJSIP_EUNSUPTRANSPORT;
    }

    TRACE_((THIS_FILE, "%s, creating new one from factory",
	   (transport?"Transport is shutdown":"No transport found")));

    /* Request factory to create transport. */
    status = factory->create_transport(factory, mgr, mgr->endpt,
				       remote, addr_len, tp);
    if (status == PJ_SUCCESS) {
	PJ_ASSERT_ON_FAIL(tp!=NULL, 
	    {pj_lock_release(mgr->lock); return PJ_EBUG;});
	pjsip_transport_add_ref(*tp);
    }
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

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    PJ_LOG(3,(THIS_FILE, " Outstanding transmit buffers: %d",
	      pj_atomic_get(mgr->tdata_counter)));
#endif

    itr = pj_hash_first(mgr->table, &itr_val);
    if (itr) {
	PJ_LOG(3, (THIS_FILE, " Dumping transports:"));

	do {
	    pjsip_transport *t = pj_hash_this(mgr->table, itr);

	    PJ_LOG(3, (THIS_FILE, "  %s %s (refcnt=%d)", 
		       t->obj_name,
		       t->info,
		       pj_atomic_get(t->ref_cnt)));

	    itr = pj_hash_next(mgr->table, itr);
	} while (itr);
    }

    pj_lock_release(mgr->lock);
#else
    PJ_UNUSED_ARG(mgr);
#endif
}


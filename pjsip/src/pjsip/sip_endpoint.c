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
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_private.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_resolve.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_errno.h>
#include <pj/except.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/hash.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/lock.h>

#define PJSIP_EX_NO_MEMORY  PJ_NO_MEMORY_EXCEPTION
#define THIS_FILE	    "endpoint"

#define MAX_METHODS   32

/**
 * The SIP endpoint.
 */
struct pjsip_endpoint
{
    /** Pool to allocate memory for the endpoint. */
    pj_pool_t		*pool;

    /** Mutex for the pool, hash table, and event list/queue. */
    pj_mutex_t		*mutex;

    /** Pool factory. */
    pj_pool_factory	*pf;

    /** Name. */
    pj_str_t		 name;

    /** Transaction table. */
    pj_hash_table_t	*tsx_table;

    /** Mutex for transaction table. */
    pj_mutex_t		*tsx_table_mutex;

    /** Timer heap. */
    pj_timer_heap_t	*timer_heap;

    /** Transport manager. */
    pjsip_tpmgr		*transport_mgr;

    /** Ioqueue. */
    pj_ioqueue_t	*ioqueue;

    /** DNS Resolver. */
    pjsip_resolver_t	*resolver;

    /** Number of modules registered. */
    pj_uint32_t		 mod_count;

    /** Modules. */
    pjsip_module        *modules[PJSIP_MAX_MODULE];

    /** Number of supported methods. */
    unsigned		 method_cnt;

    /** Array of supported methods. */
    const pjsip_method	*methods[MAX_METHODS];

    /** Allow header. */
    pjsip_allow_hdr	*allow_hdr;

    /** Route header list. */
    pjsip_route_hdr	 route_hdr_list;

    /** Additional request headers. */
    pjsip_hdr		 req_hdr;
};



/*
 * Prototypes.
 */
static void endpt_transport_callback(pjsip_endpoint*, 
				     pj_status_t, pjsip_rx_data*);

void init_sip_parser(void);

/*
 * This is the global handler for memory allocation failure, for pools that
 * are created by the endpoint (by default, all pools ARE allocated by 
 * endpoint). The error is handled by throwing exception, and hopefully,
 * the exception will be handled by the application (or this library).
 */
static void pool_callback( pj_pool_t *pool, pj_size_t size )
{
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(size);

    PJ_THROW(PJSIP_EX_NO_MEMORY);
}


/*
 * Initialize modules.
 */
static pj_status_t init_modules( pjsip_endpoint *endpt )
{
    pj_status_t status;
    unsigned i;
    //pj_str_t str_COMMA = { ", ", 2 };
    extern pjsip_module aux_tsx_module;

    PJ_LOG(5, (THIS_FILE, "init_modules()"));

    /* Load static modules. */
    endpt->mod_count = PJSIP_MAX_MODULE;
    status = register_static_modules( &endpt->mod_count, endpt->modules );
    if (status != 0) {
	return status;
    }

    /* Add mini aux module. */
    endpt->modules[endpt->mod_count++] = &aux_tsx_module;

    /* Load dynamic modules. */
    // Not supported yet!

    /* Sort modules on the priority. */
    for (i=endpt->mod_count-1; i>0; --i) {
	pj_uint32_t max = 0;
	unsigned j;
	for (j=1; j<=i; ++j) {
	    if (endpt->modules[j]->priority > endpt->modules[max]->priority)
		max = j;
	}
	if (max != i) {
	    pjsip_module *temp = endpt->modules[max];
	    endpt->modules[max] = endpt->modules[i];
	    endpt->modules[i] = temp;
	}
    }

    /* Initialize each module. */
    for (i=0; i < endpt->mod_count; ++i) {
	int j;

	pjsip_module *mod = endpt->modules[i];
	if (mod->init_module) {
	    status = mod->init_module(endpt, mod, i);
	    if (status != 0) {
		return status;
	    }
	}

	/* Collect all supported methods from modules. */
	for (j=0; j<mod->method_cnt; ++j) {
	    unsigned k;
	    for (k=0; k<endpt->method_cnt; ++k) {
		if (pjsip_method_cmp(mod->methods[j], endpt->methods[k]) == 0)
		    break;
	    }
	    if (k == endpt->method_cnt) {
		if (endpt->method_cnt < MAX_METHODS) {
		    endpt->methods[endpt->method_cnt++] = mod->methods[j];
		} else {
		    PJ_LOG(1,(THIS_FILE, "Too many methods"));
		    return -1;
		}
	    }
	}
    }

    /* Create Allow header. */
    endpt->allow_hdr = pjsip_allow_hdr_create( endpt->pool );
    endpt->allow_hdr->count = endpt->method_cnt;
    for (i=0; i<endpt->method_cnt; ++i) {
	endpt->allow_hdr->values[i] = endpt->methods[i]->name;
    }

    /* Start each module. */
    for (i=0; i < endpt->mod_count; ++i) {
	pjsip_module *mod = endpt->modules[i];
	if (mod->start_module) {
	    status = mod->start_module(mod);
	    if (status != 0) {
		return status;
	    }
	}
    }

    /* Done. */
    return 0;
}

/*
 * Unregister the transaction from the hash table, and destroy the resources
 * from the transaction.
 */
PJ_DEF(void) pjsip_endpt_destroy_tsx( pjsip_endpoint *endpt,
				      pjsip_transaction *tsx)
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_destroy_tsx(%s)", tsx->obj_name));

    pj_assert(tsx->state == PJSIP_TSX_STATE_DESTROYED);

    /* No need to lock transaction. 
     * This function typically is called from the transaction callback, which
     * means that transaction mutex is being held.
     */
    pj_assert( pj_mutex_is_locked(tsx->mutex) );

    /* Lock endpoint. */
    pj_mutex_lock( endpt->tsx_table_mutex );

    /* Unregister from the hash table. */
    pj_hash_set( NULL, endpt->tsx_table, tsx->transaction_key.ptr, 
		 tsx->transaction_key.slen, NULL);

    /* Unlock endpoint mutex. */
    pj_mutex_unlock( endpt->tsx_table_mutex );

    /* Destroy transaction mutex. */
    pj_mutex_destroy( tsx->mutex );

    /* Release the pool for the transaction. */
    pj_pool_release(tsx->pool);

    PJ_LOG(4, (THIS_FILE, "tsx%p destroyed", tsx));
}


/*
 * Receive transaction events from transactions and dispatch them to the 
 * modules.
 */
static void endpt_do_event( pjsip_endpoint *endpt, pjsip_event *evt)
{
    unsigned i;

    /* Dispatch event to modules. */
    for (i=0; i<endpt->mod_count; ++i) {
	pjsip_module *mod = endpt->modules[i];
	if (mod && mod->tsx_handler) {
	    mod->tsx_handler( mod, evt );
	}
    }

    /* Destroy transaction if it is terminated. */
    if (evt->type == PJSIP_EVENT_TSX_STATE && 
	evt->body.tsx_state.tsx->state == PJSIP_TSX_STATE_DESTROYED) 
    {
	/* No need to lock mutex. Mutex is locked inside the destroy function */
	pjsip_endpt_destroy_tsx( endpt, evt->body.tsx_state.tsx );
    }
}

/*
 * Receive transaction events from transactions and put in the event queue
 * to be processed later.
 */
void pjsip_endpt_send_tsx_event( pjsip_endpoint *endpt, pjsip_event *evt )
{
    // Need to protect this with try/catch?
    endpt_do_event(endpt, evt);
}

/*
 * Get "Allow" header.
 */
PJ_DEF(const pjsip_allow_hdr*) pjsip_endpt_get_allow_hdr( pjsip_endpoint *endpt )
{
    return endpt->allow_hdr;
}

/*
 * Get additional headers to be put in outgoing request message.
 */
PJ_DEF(const pjsip_hdr*) pjsip_endpt_get_request_headers(pjsip_endpoint *endpt)
{
    return &endpt->req_hdr;
}

PJ_DEF(pj_status_t) pjsip_endpt_set_proxies( pjsip_endpoint *endpt,
					     int url_cnt, const pj_str_t url[])
{
    int i;
    pjsip_route_hdr *hdr;
    pj_str_t str_ROUTE = { "Route", 5 };

    /* Lock endpoint mutex. */
    pj_mutex_lock(endpt->mutex);

    pj_list_init(&endpt->route_hdr_list);

    for (i=0; i<url_cnt; ++i) {
	int len = url[i].slen;
	char *dup = pj_pool_alloc(endpt->pool, len + 1);
	pj_memcpy(dup, url[i].ptr, len);
	dup[len] = '\0';

	hdr = pjsip_parse_hdr(endpt->pool, &str_ROUTE, dup, len, NULL);
	if (!hdr) {
	    pj_mutex_unlock(endpt->mutex);
	    PJ_LOG(4,(THIS_FILE, "Invalid URL %s in proxy URL", dup));
	    return -1;
	}

	pj_assert(hdr->type == PJSIP_H_ROUTE);
	pj_list_insert_before(&endpt->route_hdr_list, hdr);
    }

    /* Unlock endpoint mutex. */
    pj_mutex_unlock(endpt->mutex);

    return 0;
}

/*
 * Get "Route" header list.
 */
PJ_DEF(const pjsip_route_hdr*) pjsip_endpt_get_routing( pjsip_endpoint *endpt )
{
    return &endpt->route_hdr_list;
}


/*
 * Initialize endpoint.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create(pj_pool_factory *pf,
				       const char *name,
                                       pjsip_endpoint **p_endpt)
{
    pj_status_t status;
    pj_pool_t *pool;
    pjsip_endpoint *endpt;
    pjsip_max_forwards_hdr *mf_hdr;
    pj_lock_t *lock = NULL;

    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_create()"));

    *p_endpt = NULL;

    /* Create pool */
    pool = pj_pool_create(pf, "pept%p", 
			  PJSIP_POOL_LEN_ENDPT, PJSIP_POOL_INC_ENDPT,
			  &pool_callback);
    if (!pool)
	return PJ_ENOMEM;

    /* Create endpoint. */
    endpt = pj_pool_calloc(pool, 1, sizeof(*endpt));
    endpt->pool = pool;
    endpt->pf = pf;

    /* Init parser. */
    init_sip_parser();

    /* Get name. */
    if (name != NULL) {
	pj_str_t temp;
	pj_strdup_with_null(endpt->pool, &endpt->name, pj_cstr(&temp, name));
    } else {
	pj_strdup_with_null(endpt->pool, &endpt->name, pj_gethostname());
    }

    /* Create mutex for the events, etc. */
    status = pj_mutex_create_recursive( endpt->pool, "ept%p", &endpt->mutex );
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Create mutex for the transaction table. */
    status = pj_mutex_create_recursive( endpt->pool, "mtbl%p", 
                                        &endpt->tsx_table_mutex);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Create hash table for transaction. */
    endpt->tsx_table = pj_hash_create( endpt->pool, PJSIP_MAX_TSX_COUNT );
    if (!endpt->tsx_table) {
	status = PJ_ENOMEM;
	goto on_error;
    }

    /* Create timer heap to manage all timers within this endpoint. */
    status = pj_timer_heap_create( endpt->pool, PJSIP_MAX_TIMER_COUNT, 
                                   &endpt->timer_heap);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Set recursive lock for the timer heap. */
    status = pj_lock_create_recursive_mutex( endpt->pool, "edpt%p", &lock);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }
    pj_timer_heap_set_lock(endpt->timer_heap, lock, PJ_TRUE);

    /* Set maximum timed out entries to process in a single poll. */
    pj_timer_heap_set_max_timed_out_per_poll(endpt->timer_heap, 
					     PJSIP_MAX_TIMED_OUT_ENTRIES);

    /* Create ioqueue. */
    status = pj_ioqueue_create( endpt->pool, PJSIP_MAX_TRANSPORTS, &endpt->ioqueue);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Create transport manager. */
    status = pjsip_tpmgr_create( endpt->pool, endpt,
			         &endpt_transport_callback,
				 &endpt->transport_mgr);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Create asynchronous DNS resolver. */
    endpt->resolver = pjsip_resolver_create(endpt->pool);
    if (!endpt->resolver) {
	PJ_LOG(4, (THIS_FILE, "pjsip_endpt_init(): error creating resolver"));
	goto on_error;
    }

    /* Initialize TLS ID for transaction lock. */
    status = pj_thread_local_alloc(&pjsip_tsx_lock_tls_id);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }
    pj_thread_local_set(pjsip_tsx_lock_tls_id, NULL);

    /* Initialize request headers. */
    pj_list_init(&endpt->req_hdr);

    /* Initialist "Route" header list. */
    pj_list_init(&endpt->route_hdr_list);

    /* Add "Max-Forwards" for request header. */
    mf_hdr = pjsip_max_forwards_hdr_create(endpt->pool);
    mf_hdr->ivalue = PJSIP_MAX_FORWARDS_VALUE;
    pj_list_insert_before( &endpt->req_hdr, mf_hdr);

    /* Load and init modules. */
    status = init_modules(endpt);
    if (status != PJ_SUCCESS) {
	PJ_LOG(4, (THIS_FILE, "pjsip_endpt_init(): error in init_modules()"));
	return status;
    }

    /* Done. */
    *p_endpt = endpt;
    return status;

on_error:
    if (endpt->transport_mgr) {
	pjsip_tpmgr_destroy(endpt->transport_mgr);
	endpt->transport_mgr = NULL;
    }
    if (endpt->mutex) {
	pj_mutex_destroy(endpt->mutex);
	endpt->mutex = NULL;
    }
    if (endpt->tsx_table_mutex) {
	pj_mutex_destroy(endpt->tsx_table_mutex);
	endpt->tsx_table_mutex = NULL;
    }
    pj_pool_release( endpt->pool );

    PJ_LOG(4, (THIS_FILE, "pjsip_endpt_init() failed"));
    return status;
}

/*
 * Destroy endpoint.
 */
PJ_DEF(void) pjsip_endpt_destroy(pjsip_endpoint *endpt)
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_destroy()"));

    /* Shutdown and destroy all transports. */
    pjsip_tpmgr_destroy(endpt->transport_mgr);

    /* Delete endpoint mutex. */
    pj_mutex_destroy(endpt->mutex);

    /* Delete transaction table mutex. */
    pj_mutex_destroy(endpt->tsx_table_mutex);

    /* Finally destroy pool. */
    pj_pool_release(endpt->pool);
}

/*
 * Get endpoint name.
 */
PJ_DEF(const pj_str_t*) pjsip_endpt_name(const pjsip_endpoint *endpt)
{
    return &endpt->name;
}


/*
 * Create new pool.
 */
PJ_DEF(pj_pool_t*) pjsip_endpt_create_pool( pjsip_endpoint *endpt,
					       const char *pool_name,
					       pj_size_t initial,
					       pj_size_t increment )
{
    pj_pool_t *pool;

    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_create_pool()"));

    /* Lock endpoint mutex. */
    pj_mutex_lock(endpt->mutex);

    /* Create pool */
    pool = pj_pool_create( endpt->pf, pool_name,
			   initial, increment, &pool_callback);

    /* Unlock mutex. */
    pj_mutex_unlock(endpt->mutex);

    if (pool) {
	PJ_LOG(5, (THIS_FILE, "   pool %s created", pj_pool_getobjname(pool)));
    } else {
	PJ_LOG(4, (THIS_FILE, "Unable to create pool %s!", pool_name));
    }

    return pool;
}

/*
 * Return back pool to endpoint's pool manager to be either destroyed or
 * recycled.
 */
PJ_DEF(void) pjsip_endpt_destroy_pool( pjsip_endpoint *endpt, pj_pool_t *pool )
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_destroy_pool(%s)", pj_pool_getobjname(pool)));

    pj_mutex_lock(endpt->mutex);
    pj_pool_release( pool );
    pj_mutex_unlock(endpt->mutex);
}

/*
 * Handle events.
 */
PJ_DEF(void) pjsip_endpt_handle_events( pjsip_endpoint *endpt,
					const pj_time_val *max_timeout)
{
    /* timeout is 'out' var. This just to make compiler happy. */
    pj_time_val timeout = { 0, 0};

    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_handle_events()"));

    /* Poll the timer. The timer heap has its own mutex for better 
     * granularity, so we don't need to lock end endpoint. 
     */
    timeout.sec = timeout.msec = 0;
    pj_timer_heap_poll( endpt->timer_heap, &timeout );

    /* If caller specifies maximum time to wait, then compare the value with
     * the timeout to wait from timer, and use the minimum value.
     */
    if (max_timeout && PJ_TIME_VAL_GT(timeout, *max_timeout)) {
	timeout = *max_timeout;
    }

    /* Poll ioqueue. */
    pj_ioqueue_poll( endpt->ioqueue, &timeout);
}

/*
 * Schedule timer.
 */
PJ_DEF(pj_status_t) pjsip_endpt_schedule_timer( pjsip_endpoint *endpt,
						pj_timer_entry *entry,
						const pj_time_val *delay )
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_schedule_timer(entry=%p, delay=%u.%u)",
			 entry, delay->sec, delay->msec));
    return pj_timer_heap_schedule( endpt->timer_heap, entry, delay );
}

/*
 * Cancel the previously registered timer.
 */
PJ_DEF(void) pjsip_endpt_cancel_timer( pjsip_endpoint *endpt, 
				       pj_timer_entry *entry )
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_cancel_timer(entry=%p)", entry));
    pj_timer_heap_cancel( endpt->timer_heap, entry );
}

/*
 * Create a new transaction.
 * Endpoint must then initialize the new transaction as either UAS or UAC, and
 * register it to the hash table.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_tsx(pjsip_endpoint *endpt,
					   pjsip_transaction **p_tsx)
{
    pj_pool_t *pool;

    PJ_ASSERT_RETURN(endpt && p_tsx, PJ_EINVAL);

    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_create_tsx()"));

    /* Request one pool for the transaction. Mutex is locked there. */
    pool = pjsip_endpt_create_pool(endpt, "ptsx%p", 
				      PJSIP_POOL_LEN_TSX, PJSIP_POOL_INC_TSX);
    if (pool == NULL) {
	return PJ_ENOMEM;
    }

    /* Create the transaction. */
    return pjsip_tsx_create(pool, endpt, p_tsx);
}

/*
 * Register the transaction to the endpoint.
 * This will put the transaction to the transaction hash table. Before calling
 * this function, the transaction must be INITIALIZED as either UAS or UAC, so
 * that the transaction key is built.
 */
PJ_DEF(void) pjsip_endpt_register_tsx( pjsip_endpoint *endpt,
				       pjsip_transaction *tsx)
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_register_tsx(%s)", tsx->obj_name));

    pj_assert(tsx->transaction_key.slen != 0);
    //pj_assert(tsx->state != PJSIP_TSX_STATE_NULL);

    /* Lock hash table mutex. */
    pj_mutex_lock(endpt->tsx_table_mutex);

    /* Register the transaction to the hash table. */
    pj_hash_set( tsx->pool, endpt->tsx_table, tsx->transaction_key.ptr,
		 tsx->transaction_key.slen, tsx);

    /* Unlock mutex. */
    pj_mutex_unlock(endpt->tsx_table_mutex);
}

/*
 * Find transaction by the key.
 */
PJ_DEF(pjsip_transaction*) pjsip_endpt_find_tsx( pjsip_endpoint *endpt,
					          const pj_str_t *key )
{
    pjsip_transaction *tsx;

    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_find_tsx()"));

    /* Start lock mutex in the endpoint. */
    pj_mutex_lock(endpt->tsx_table_mutex);

    /* Find the transaction in the hash table. */
    tsx = pj_hash_get( endpt->tsx_table, key->ptr, key->slen );

    /* Unlock mutex. */
    pj_mutex_unlock(endpt->tsx_table_mutex);

    return tsx;
}

/*
 * Create key.
 */
static void rdata_create_key( pjsip_rx_data *rdata)
{
    pjsip_role_e role;
    if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG) {
	role = PJSIP_ROLE_UAS;
    } else {
	role = PJSIP_ROLE_UAC;
    }
    pjsip_tsx_create_key(rdata->tp_info.pool, &rdata->endpt_info.key, role,
			 &rdata->msg_info.cseq->method, rdata);
}

/*
 * This is the callback that is called by the transport manager when it 
 * receives a message from the network.
 */
static void endpt_transport_callback( pjsip_endpoint *endpt,
				      pj_status_t status,
				      pjsip_rx_data *rdata )
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_transaction *tsx;
    pj_bool_t a_new_transaction_just_been_created = PJ_FALSE;

    PJ_LOG(5, (THIS_FILE, "endpt_transport_callback(rdata=%p)", rdata));

    if (status != PJ_SUCCESS) {
	const char *src_addr = pj_inet_ntoa(rdata->pkt_info.addr.sin_addr);
	int port = pj_ntohs(rdata->pkt_info.addr.sin_port);
	PJSIP_ENDPT_LOG_ERROR((endpt, "transport", status,
			       "Src.addr=%s:%d, packet:--\n"
			       "%s\n"
			       "-- end of packet. Error",
			       src_addr, port, rdata->msg_info.msg_buf));
	return;
    }

    /* For response, check that the value in Via sent-by match the transport.
     * If not matched, silently drop the response.
     * Ref: RFC3261 Section 18.1.2 Receiving Response
     */
    if (msg->type == PJSIP_RESPONSE_MSG) {
	const pj_sockaddr_in *addr;
	const char *addr_addr;
	int port = rdata->msg_info.via->sent_by.port;
	pj_bool_t mismatch = PJ_FALSE;
	if (port == 0) {
	    int type;
	    type = rdata->tp_info.transport->type;
	    port = pjsip_transport_get_default_port_for_type(type);
	}
	addr = &rdata->tp_info.transport->public_addr;
	addr_addr = pj_inet_ntoa(addr->sin_addr);
	if (pj_strcmp2(&rdata->msg_info.via->sent_by.host, addr_addr) != 0)
	    mismatch = PJ_TRUE;
	else if (port != pj_ntohs(addr->sin_port)) {
	    /* Port or address mismatch, we should discard response */
	    /* But we saw one implementation (we don't want to name it to 
	     * protect the innocence) which put wrong sent-by port although
	     * the "rport" parameter is correct.
	     * So we discard the response only if the port doesn't match
	     * both the port in sent-by and rport. We try to be lenient here!
	     */
	    if (rdata->msg_info.via->rport_param != pj_sockaddr_in_get_port(addr))
		mismatch = PJ_TRUE;
	    else {
		PJ_LOG(4,(THIS_FILE, "Response %p has mismatch port in sent-by"
				    " but the rport parameter is correct",
				    rdata));
	    }
	}

	if (mismatch) {
	    pjsip_event e;

	    PJSIP_EVENT_INIT_DISCARD_MSG(e, rdata, PJSIP_EINVALIDVIA);
	    endpt_do_event( endpt, &e );
	    return;
	}
    } 

    /* Create key for transaction lookup. */
    rdata_create_key( rdata);

    /* Find the transaction for the received message. */
    PJ_LOG(5, (THIS_FILE, "finding tsx with key=%.*s", 
			 rdata->endpt_info.key.slen, rdata->endpt_info.key.ptr));

    /* Start lock mutex in the endpoint. */
    pj_mutex_lock(endpt->tsx_table_mutex);

    /* Find the transaction in the hash table. */
    tsx = pj_hash_get( endpt->tsx_table, rdata->endpt_info.key.ptr, rdata->endpt_info.key.slen );

    /* Unlock mutex. */
    pj_mutex_unlock(endpt->tsx_table_mutex);

    /* If the transaction is not found... */
    if (tsx == NULL || tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	/* 
	 * For response message, discard the message, except if the response is
	 * an 2xx class response to INVITE, which in this case it must be
	 * passed to TU to be acked.
	 */
	if (msg->type == PJSIP_RESPONSE_MSG) {

	    /* Inform TU about the 200 message, only if it's INVITE. */
	    if (PJSIP_IS_STATUS_IN_CLASS(msg->line.status.code, 200) &&
		rdata->msg_info.cseq->method.id == PJSIP_INVITE_METHOD) 
	    {
		pjsip_event e;

		/* Should not happen for UA. Tsx theoritically lives until
		 * all responses are absorbed.
		 */
		pj_assert(0);

		PJSIP_EVENT_INIT_RX_200_MSG(e, rdata);
		endpt_do_event( endpt, &e );

	    } else {
		/* Just discard the response, inform TU. */
		pjsip_event e;

		PJSIP_EVENT_INIT_DISCARD_MSG(e, rdata, 
		    PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_CALL_TSX_DOES_NOT_EXIST));
		endpt_do_event( endpt, &e );
	    }

	/*
	 * For non-ACK request message, create a new transaction.
	 */
	} else if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {

	    pj_status_t status;

	    /* Create transaction, mutex is locked there. */
	    status = pjsip_endpt_create_tsx(endpt, &tsx);
	    if (status != PJ_SUCCESS) {
		PJSIP_ENDPT_LOG_ERROR((endpt, THIS_FILE, status,
				       "Unable to create transaction"));
		return;
	    }

	    /* Initialize transaction as UAS. */
	    pjsip_tsx_init_uas( tsx, rdata );

	    /* Register transaction, mutex is locked there. */
	    pjsip_endpt_register_tsx( endpt, tsx );

	    a_new_transaction_just_been_created = PJ_TRUE;
	}
    }

    /* If transaction is found (or newly created), pass the message.
     * Otherwise if it's an ACK request, pass directly to TU.
     */
    if (tsx && tsx->state != PJSIP_TSX_STATE_TERMINATED) {
	/* Dispatch message to transaction. */
	pjsip_tsx_on_rx_msg( tsx, rdata );

    } else if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD) {
	/*
	 * This is an ACK message, but the INVITE transaction could not
	 * be found (possibly because the branch parameter in Via in ACK msg
	 * is different than the branch in original INVITE). This happens with
	 * SER!
	 */
	pjsip_event event;

	PJSIP_EVENT_INIT_RX_ACK_MSG(event,rdata);
	endpt_do_event( endpt, &event );
    }

    /*
     * If a new request message has just been receieved, but no modules
     * seem to be able to handle the request message, then terminate the
     * transaction.
     *
     * Ideally for cases like "unsupported method", we should be able to
     * answer the request statelessly. But we can not do that since the
     * endpoint shoule be able to be used as both user agent and proxy stack,
     * and a proxy stack should be able to handle arbitrary methods.
     */
    if (a_new_transaction_just_been_created && tsx->status_code < 100) {
	/* Certainly no modules has sent any response message.
	 * Check that any modules has attached a module data.
	 */
	int i;
	for (i=0; i<PJSIP_MAX_MODULE; ++i) {
	    if (tsx->module_data[i] != NULL) {
		break;
	    }
	}
	if (i == PJSIP_MAX_MODULE) {
	    /* No modules have attached itself to the transaction. 
	     * Terminate the transaction with 501/Not Implemented.
	     */
	    pjsip_tx_data *tdata;
	    pj_status_t status;
	    
	    if (tsx->method.id == PJSIP_OPTIONS_METHOD) {
		status = pjsip_endpt_create_response(endpt, rdata, 200, 
						     &tdata);
	    } else {
		status = pjsip_endpt_create_response(endpt, rdata, 
						     PJSIP_SC_METHOD_NOT_ALLOWED,
						     &tdata);
	    }

	    if (status != PJ_SUCCESS) {
		PJSIP_ENDPT_LOG_ERROR((endpt, THIS_FILE, status,
				       "Unable to create response"));
		return;
	    }

	    if (endpt->allow_hdr) {
		pjsip_msg_add_hdr( tdata->msg, 
				   pjsip_hdr_shallow_clone(tdata->pool, endpt->allow_hdr));
	    }
	    pjsip_tsx_on_tx_msg( tsx, tdata );

	} else {
	    /*
	     * If a module has registered itself in the transaction but it
	     * hasn't responded the request, chances are the module wouldn't
	     * respond to the request at all. We terminate the request here
	     * with 500/Internal Server Error, to be safe.
	     */
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    status = pjsip_endpt_create_response(endpt, rdata, 500, &tdata);
	    if (status != PJ_SUCCESS) {
		PJSIP_ENDPT_LOG_ERROR((endpt, THIS_FILE, status,
				       "Unable to create response"));
		return;
	    }

	    pjsip_tsx_on_tx_msg(tsx, tdata);
	}
    }
}

/*
 * Create transmit data buffer.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_tdata(  pjsip_endpoint *endpt,
					       pjsip_tx_data **p_tdata)
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_create_tdata()"));
    return pjsip_tx_data_create(endpt->transport_mgr, p_tdata);
}

/*
 * Resolve
 */
PJ_DEF(void) pjsip_endpt_resolve( pjsip_endpoint *endpt,
				  pj_pool_t *pool,
				  pjsip_host_port *target,
				  void *token,
				  pjsip_resolver_callback *cb)
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_resolve()"));
    pjsip_resolve( endpt->resolver, pool, target, token, cb);
}

/*
 * Get transport manager.
 */
PJ_DEF(pjsip_tpmgr*) pjsip_endpt_get_tpmgr(pjsip_endpoint *endpt)
{
    return endpt->transport_mgr;
}

/*
 * Get ioqueue instance.
 */
PJ_DEF(pj_ioqueue_t*) pjsip_endpt_get_ioqueue(pjsip_endpoint *endpt)
{
    return endpt->ioqueue;
}

/*
 * Find/create transport.
 */
PJ_DEF(pj_status_t) pjsip_endpt_alloc_transport( pjsip_endpoint *endpt,
						  pjsip_transport_type_e type,
						  const pj_sockaddr_in *remote,
						  pjsip_transport **p_transport)
{
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_alloc_transport()"));
    return pjsip_tpmgr_alloc_transport( endpt->transport_mgr, type, remote,
					p_transport);
}


/*
 * Report error.
 */
PJ_DEF(void) pjsip_endpt_log_error(  pjsip_endpoint *endpt,
				     const char *sender,
                                     pj_status_t error_code,
                                     const char *format,
                                     ... )
{
#if PJ_LOG_MAX_LEVEL > 0
    char newformat[256];
    int len;
    va_list marker;

    va_start(marker, format);

    PJ_UNUSED_ARG(endpt);

    len = pj_native_strlen(format);
    if (len < sizeof(newformat)-30) {
	pj_str_t errstr;

	pj_native_strcpy(newformat, format);
	pj_snprintf(newformat+len, sizeof(newformat)-len-1,
		    ": [err %d] ", error_code);
	len += pj_native_strlen(newformat+len);

	errstr = pjsip_strerror(error_code, newformat+len, 
				sizeof(newformat)-len-1);

	len += errstr.slen;
	newformat[len] = '\0';

	pj_log(sender, 1, newformat, marker);
    } else {
	pj_log(sender, 1, format, marker);
    }

    va_end(marker);
#else
    PJ_UNUSED_ARG(format);
    PJ_UNUSED_ARG(error_code);
    PJ_UNUSED_ARG(sender);
    PJ_UNUSED_ARG(endpt);
#endif
}


/*
 * Dump endpoint.
 */
PJ_DEF(void) pjsip_endpt_dump( pjsip_endpoint *endpt, pj_bool_t detail )
{
#if PJ_LOG_MAX_LEVEL >= 3
    unsigned count;

    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_dump()"));

    /* Lock mutex. */
    pj_mutex_lock(endpt->mutex);

    PJ_LOG(3, (THIS_FILE, "Dumping endpoint %p:", endpt));
    
    /* Dumping pool factory. */
    (*endpt->pf->dump_status)(endpt->pf, detail);

    /* Pool health. */
    PJ_LOG(3, (THIS_FILE," Endpoint pool capacity=%u, used_size=%u",
	       pj_pool_get_capacity(endpt->pool),
	       pj_pool_get_used_size(endpt->pool)));

    /* Transaction tables. */
    count = pj_hash_count(endpt->tsx_table);
    PJ_LOG(3, (THIS_FILE, " Number of transactions: %u", count));

    if (count && detail) {
	pj_hash_iterator_t it_val;
	pj_hash_iterator_t *it;
	pj_time_val now;

	PJ_LOG(3, (THIS_FILE, " Dumping transaction tables:"));

	pj_gettimeofday(&now);
	it = pj_hash_first(endpt->tsx_table, &it_val);

	while (it != NULL) {
	    int timeout_diff;

	    /* Get the transaction. No need to lock transaction's mutex
	     * since we already hold endpoint mutex, so that no transactions
	     * will be deleted.
	     */
	    pjsip_transaction *tsx = pj_hash_this(endpt->tsx_table, it);

	    const char *role = (tsx->role == PJSIP_ROLE_UAS ? "UAS" : "UAC");
	
	    if (tsx->timeout_timer._timer_id != -1) {
		if (tsx->timeout_timer._timer_value.sec > now.sec) {
		    timeout_diff = tsx->timeout_timer._timer_value.sec - now.sec;
		} else {
		    timeout_diff = now.sec - tsx->timeout_timer._timer_value.sec;
		    timeout_diff = 0 - timeout_diff;
		}
	    } else {
		timeout_diff = -1;
	    }

	    PJ_LOG(3, (THIS_FILE, "  %s %s %10.*s %.9u %s t=%ds", 
		       tsx->obj_name, role, 
		       tsx->method.name.slen, tsx->method.name.ptr,
		       tsx->cseq,
		       pjsip_tsx_state_str(tsx->state),
		       timeout_diff));

	    it = pj_hash_next(endpt->tsx_table, it);
	}
    }

    /* Transports. 
     */
    pjsip_tpmgr_dump_transports( endpt->transport_mgr );

    /* Timer. */
    PJ_LOG(3,(THIS_FILE, " Timer heap has %u entries", 
			pj_timer_heap_count(endpt->timer_heap)));

    /* Unlock mutex. */
    pj_mutex_unlock(endpt->mutex);
#else
    PJ_UNUSED_ARG(endpt);
    PJ_UNUSED_ARG(detail);
    PJ_LOG(3,(THIS_FILE, "pjsip_end_dump: can't dump because it's disabled."));
#endif
}


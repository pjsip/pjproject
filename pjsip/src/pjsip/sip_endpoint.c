/* $Header: /pjproject/pjsip/src/pjsip/sip_endpoint.c 24    6/23/05 12:20a Bennylp $ */
/* 
 * PJSIP - SIP Stack
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_private.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_resolve.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_misc.h>
#include <pj/except.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/hash.h>


#define PJSIP_EX_NO_MEMORY  PJ_NO_MEMORY_EXCEPTION
#define LOG_THIS	    "endpoint..."

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

    /** Transaction table. */
    pj_hash_table_t	*tsx_table;

    /** Mutex for transaction table. */
    pj_mutex_t		*tsx_table_mutex;

    /** Timer heap. */
    pj_timer_heap_t	*timer_heap;

    /** Transport manager. */
    pjsip_transport_mgr *transport_mgr;

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
static void endpt_transport_callback( pjsip_endpoint *, pjsip_rx_data *rdata );


/*
 * Create transaction.
 * Defined in sip_transaction.c
 */
pjsip_transaction * pjsip_tsx_create( pj_pool_t *pool, pjsip_endpoint *endpt);

/*
 * This is the global handler for memory allocation failure, for pools that
 * are created by the endpoint (by default, all pools ARE allocated by 
 * endpoint). The error is handled by throwing exception, and hopefully,
 * the exception will be handled by the application (or this library).
 */
static void pool_callback( pj_pool_t *pool, pj_size_t size )
{
    PJ_UNUSED_ARG(pool)
    PJ_UNUSED_ARG(size)

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

    PJ_LOG(5, (LOG_THIS, "init_modules()"));

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
		    PJ_LOG(1,(LOG_THIS, "Too many methods"));
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
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_destroy_tsx(%s)", tsx->obj_name));

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

    PJ_LOG(4, (LOG_THIS, "tsx%p destroyed", tsx));
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
    if (evt->type == PJSIP_EVENT_TSX_STATE_CHANGED && 
	evt->obj.tsx->state == PJSIP_TSX_STATE_DESTROYED) 
    {
	/* No need to lock mutex. Mutex is locked inside the destroy function */
	pjsip_endpt_destroy_tsx( endpt, evt->obj.tsx );
    }
}

/*
 * Receive transaction events from transactions and put in the event queue
 * to be processed later.
 */
void pjsip_endpt_send_tsx_event( pjsip_endpoint *endpt, pjsip_event *evt )
{
    endpt_do_event(endpt, evt);
}

/*
 * Get "Allow" header.
 */
PJ_DECL(const pjsip_allow_hdr*) pjsip_endpt_get_allow_hdr( pjsip_endpoint *endpt )
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
	    PJ_LOG(4,(LOG_THIS, "Invalid URL %s in proxy URL", dup));
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
PJ_DEF(pjsip_endpoint*) pjsip_endpt_create(pj_pool_factory *pf)
{
    pj_status_t status;
    pj_pool_t *pool;
    pjsip_endpoint *endpt;
    pjsip_max_forwards_hdr *mf_hdr;

    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_create()"));

    /* Create pool */
    pool = pj_pool_create(pf, "pept%p", 
			  PJSIP_POOL_LEN_ENDPT, PJSIP_POOL_INC_ENDPT,
			  &pool_callback);
    if (!pool)
	return NULL;

    /* Create endpoint. */
    endpt = pj_pool_calloc(pool, 1, sizeof(*endpt));
    endpt->pool = pool;
    endpt->pf = pf;

    /* Create mutex for the events, etc. */
    endpt->mutex = pj_mutex_create( endpt->pool, "ept%p", 0 );
    if (!endpt->mutex) {
	PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init(): error creating endpoint mutex"));
	goto on_error;
    }

    /* Create mutex for the transaction table. */
    endpt->tsx_table_mutex = pj_mutex_create( endpt->pool, "mtbl%p", 0);
    if (!endpt->tsx_table_mutex) {
	PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init(): error creating endpoint mutex(2)"));
	goto on_error;
    }

    /* Create hash table for transaction. */
    endpt->tsx_table = pj_hash_create( endpt->pool, PJSIP_MAX_TSX_COUNT );
    if (!endpt->tsx_table) {
	PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init(): error creating tsx hash table"));
	goto on_error;
    }

    /* Create timer heap to manage all timers within this endpoint. */
    endpt->timer_heap = pj_timer_heap_create( endpt->pool, PJSIP_MAX_TIMER_COUNT, 0);
    if (!endpt->timer_heap) {
	PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init(): error creating timer heap"));
	goto on_error;
    }

    /* Create transport manager. */
    endpt->transport_mgr = pjsip_transport_mgr_create( endpt->pool,
						       endpt,
						       &endpt_transport_callback);
    if (!endpt->transport_mgr) {
	PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init(): error creating transport mgr"));
	goto on_error;
    }

    /* Create asynchronous DNS resolver. */
    endpt->resolver = pjsip_resolver_create(endpt->pool);
    if (!endpt->resolver) {
	PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init(): error creating resolver"));
	goto on_error;
    }

    /* Initialize TLS ID for transaction lock. */
    pjsip_tsx_lock_tls_id = pj_thread_local_alloc();
    if (pjsip_tsx_lock_tls_id == -1) {
	PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init(): error allocating TLS"));
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
    if (status != PJ_OK) {
	PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init(): error in init_modules()"));
	return NULL;
    }

    /* Done. */
    return endpt;

on_error:
    if (endpt->transport_mgr) {
	pjsip_transport_mgr_destroy(endpt->transport_mgr);
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

    PJ_LOG(4, (LOG_THIS, "pjsip_endpt_init() failed"));
    return NULL;
}

/*
 * Destroy endpoint.
 */
PJ_DEF(void) pjsip_endpt_destroy(pjsip_endpoint *endpt)
{
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_destroy()"));

    /* Shutdown and destroy all transports. */
    pjsip_transport_mgr_destroy(endpt->transport_mgr);

    /* Delete endpoint mutex. */
    pj_mutex_destroy(endpt->mutex);

    /* Delete transaction table mutex. */
    pj_mutex_destroy(endpt->tsx_table_mutex);

    /* Finally destroy pool. */
    pj_pool_release(endpt->pool);
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

    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_create_pool()"));

    /* Lock endpoint mutex. */
    pj_mutex_lock(endpt->mutex);

    /* Create pool */
    pool = pj_pool_create( endpt->pf, pool_name,
			   initial, increment, &pool_callback);

    /* Unlock mutex. */
    pj_mutex_unlock(endpt->mutex);

    if (pool) {
	PJ_LOG(5, (LOG_THIS, "   pool %s created", pj_pool_getobjname(pool)));
    } else {
	PJ_LOG(4, (LOG_THIS, "Unable to create pool %s!", pool_name));
    }

    return pool;
}

/*
 * Return back pool to endpoint's pool manager to be either destroyed or
 * recycled.
 */
PJ_DEF(void) pjsip_endpt_destroy_pool( pjsip_endpoint *endpt, pj_pool_t *pool )
{
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_destroy_pool(%s)", pj_pool_getobjname(pool)));

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
    pj_time_val timeout;
    int i;

    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_handle_events()"));

    /* Poll the timer. The timer heap has its own mutex for better 
     * granularity, so we don't need to lock end endpoint. We also keep
     * polling the timer while we have events.
     */
    timeout.sec = timeout.msec = 0; /* timeout is 'out' var. This just to make compiler happy. */
    for (i=0; i<10; ++i) {
	if (pj_timer_heap_poll( endpt->timer_heap, &timeout ) < 1)
	    break;
    }

    /* If caller specifies maximum time to wait, then compare the value with
     * the timeout to wait from timer, and use the minimum value.
     */
    if (max_timeout && PJ_TIME_VAL_GT(timeout, *max_timeout)) {
	timeout = *max_timeout;
    }

    /* Poll events in the transport manager. */
    pjsip_transport_mgr_handle_events( endpt->transport_mgr, &timeout);
}

/*
 * Schedule timer.
 */
PJ_DEF(pj_status_t) pjsip_endpt_schedule_timer( pjsip_endpoint *endpt,
						pj_timer_entry *entry,
						const pj_time_val *delay )
{
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_schedule_timer(entry=%p, delay=%u.%u)",
			 entry, delay->sec, delay->msec));
    return pj_timer_heap_schedule( endpt->timer_heap, entry, delay );
}

/*
 * Cancel the previously registered timer.
 */
PJ_DEF(void) pjsip_endpt_cancel_timer( pjsip_endpoint *endpt, 
				       pj_timer_entry *entry )
{
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_cancel_timer(entry=%p)", entry));
    pj_timer_heap_cancel( endpt->timer_heap, entry );
}

/*
 * Create a new transaction.
 * Endpoint must then initialize the new transaction as either UAS or UAC, and
 * register it to the hash table.
 */
PJ_DEF(pjsip_transaction*) pjsip_endpt_create_tsx(pjsip_endpoint *endpt)
{
    pj_pool_t *pool;
    pjsip_transaction *tsx;

    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_create_tsx()"));

    /* Request one pool for the transaction. Mutex is locked there. */
    pool = pjsip_endpt_create_pool(endpt, "ptsx%p", 
				      PJSIP_POOL_LEN_TSX, PJSIP_POOL_INC_TSX);
    if (pool == NULL) {
	PJ_LOG(2, (LOG_THIS, "failed to create transaction (no pool)"));
	return NULL;
    }

    /* Create the transaction. */
    tsx = pjsip_tsx_create(pool, endpt);

    /* Return */
    return tsx;
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
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_register_tsx(%s)", tsx->obj_name));

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
PJ_DECL(pjsip_transaction*) pjsip_endpt_find_tsx( pjsip_endpoint *endpt,
					          const pj_str_t *key )
{
    pjsip_transaction *tsx;

    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_find_tsx()"));

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
    if (rdata->msg->type == PJSIP_REQUEST_MSG) {
	role = PJSIP_ROLE_UAS;
    } else {
	role = PJSIP_ROLE_UAC;
    }
    pjsip_tsx_create_key(rdata->pool, &rdata->key, role,
			 &rdata->cseq->method, rdata);
}

/*
 * This is the callback that is called by the transport manager when it 
 * receives a message from the network.
 */
static void endpt_transport_callback( pjsip_endpoint *endpt,
				      pjsip_rx_data *rdata )
{
    pjsip_msg *msg = rdata->msg;
    pjsip_transaction *tsx;
    pj_bool_t a_new_transaction_just_been_created = PJ_FALSE;

    PJ_LOG(5, (LOG_THIS, "endpt_transport_callback(rdata=%p)", rdata));

    /* For response, check that the value in Via sent-by match the transport.
     * If not matched, silently drop the response.
     * Ref: RFC3261 Section 18.1.2 Receiving Response
     */
    if (msg->type == PJSIP_RESPONSE_MSG) {
	const pj_sockaddr_in *addr;
	const char *addr_addr;
	int port = rdata->via->sent_by.port;
	pj_bool_t mismatch = PJ_FALSE;
	if (port == 0) {
	    int type;
	    type = pjsip_transport_get_type(rdata->transport);
	    port = pjsip_transport_get_default_port_for_type(type);
	}
	addr = pjsip_transport_get_addr_name(rdata->transport);
	addr_addr = pj_sockaddr_get_str_addr(addr);
	if (pj_strcmp2(&rdata->via->sent_by.host, addr_addr) != 0)
	    mismatch = PJ_TRUE;
	else if (port != pj_sockaddr_get_port(addr)) {
	    /* Port or address mismatch, we should discard response */
	    /* But we saw one implementation (we don't want to name it to 
	     * protect the innocence) which put wrong sent-by port although
	     * the "rport" parameter is correct.
	     * So we discard the response only if the port doesn't match
	     * both the port in sent-by and rport. We try to be lenient here!
	     */
	    if (rdata->via->rport_param != pj_sockaddr_get_port(addr))
		mismatch = PJ_TRUE;
	    else {
		PJ_LOG(4,(LOG_THIS, "Response %p has mismatch port in sent-by"
				    " but the rport parameter is correct",
				    rdata));
	    }
	}

	if (mismatch) {
	    pjsip_event e;

	    PJ_LOG(3, (LOG_THIS, "Response %p discarded: sent-by mismatch",
				 rdata));

	    e.type = PJSIP_EVENT_DISCARD_MSG;
	    e.src_type = PJSIP_EVENT_RX_MSG;
	    e.src.rdata = rdata;
	    e.obj.ptr = NULL;
	    endpt_do_event( endpt, &e );
	    return;
	}
    } 

    /* Create key for transaction lookup. */
    rdata_create_key( rdata);

    /* Find the transaction for the received message. */
    PJ_LOG(5, (LOG_THIS, "finding tsx with key=%.*s", 
			 rdata->key.slen, rdata->key.ptr));

    /* Start lock mutex in the endpoint. */
    pj_mutex_lock(endpt->tsx_table_mutex);

    /* Find the transaction in the hash table. */
    tsx = pj_hash_get( endpt->tsx_table, rdata->key.ptr, rdata->key.slen );

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
		rdata->cseq->method.id == PJSIP_INVITE_METHOD) 
	    {
		pjsip_event e;

		/* Should not happen for UA. Tsx theoritically lives until
		 * all responses are absorbed.
		 */
		pj_assert(0);

		e.type = PJSIP_EVENT_RX_200_RESPONSE;
		e.src_type = PJSIP_EVENT_RX_MSG;
		e.src.rdata = rdata;
		e.obj.ptr = NULL;
		endpt_do_event( endpt, &e );

	    } else {
		/* Just discard the response, inform TU. */
		pjsip_event e;

		PJ_LOG(3, (LOG_THIS, "Response %p discarded: transaction not found",
			   rdata));

		e.type = PJSIP_EVENT_DISCARD_MSG;
		e.src_type = PJSIP_EVENT_RX_MSG;
		e.src.rdata = rdata;
		e.obj.ptr = NULL;
		endpt_do_event( endpt, &e );
	    }

	/*
	 * For non-ACK request message, create a new transaction.
	 */
	} else if (rdata->msg->line.req.method.id != PJSIP_ACK_METHOD) {
	    /* Create transaction, mutex is locked there. */
	    tsx = pjsip_endpt_create_tsx(endpt);
	    if (!tsx)
		return;

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

    } else if (rdata->msg->line.req.method.id == PJSIP_ACK_METHOD) {
	/*
	 * This is an ACK message, but the INVITE transaction could not
	 * be found (possibly because the branch parameter in Via in ACK msg
	 * is different than the branch in original INVITE). This happens with
	 * SER!
	 */
	pjsip_event event;

	event.type = PJSIP_EVENT_RX_ACK_MSG;
	event.src_type = PJSIP_EVENT_RX_MSG;
	event.src.rdata = rdata;
	event.obj.ptr = NULL;
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
	    
	    if (tsx->method.id == PJSIP_OPTIONS_METHOD) {
		tdata = pjsip_endpt_create_response(endpt, rdata, 200);
	    } else {
		tdata = pjsip_endpt_create_response(endpt, rdata, 
						    PJSIP_SC_METHOD_NOT_ALLOWED);
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
	    tdata = pjsip_endpt_create_response(endpt, rdata, 500);
	    pjsip_tsx_on_tx_msg(tsx, tdata);
	}
    }
}

/*
 * Create transmit data buffer.
 */
PJ_DEF(pjsip_tx_data*) pjsip_endpt_create_tdata( pjsip_endpoint *endpt )
{
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_create_tdata()"));
    return pjsip_tx_data_create(endpt->transport_mgr);
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
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_resolve()"));
    pjsip_resolve( endpt->resolver, pool, target, token, cb);
}

/*
 * Find/create transport.
 */
PJ_DEF(void) pjsip_endpt_get_transport( pjsip_endpoint *endpt,
					pj_pool_t *pool,
					pjsip_transport_type_e type,
					const pj_sockaddr_in *remote,
					void *token,
					pjsip_transport_completion_callback *cb)
{
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_get_transport()"));
    pjsip_transport_get( endpt->transport_mgr, pool, type,
			 remote, token, cb);
}


PJ_DEF(pj_status_t) pjsip_endpt_create_listener( pjsip_endpoint *endpt,
						 pjsip_transport_type_e type,
						 pj_sockaddr_in *addr,
						 const pj_sockaddr_in *addr_name)
{
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_create_listener()"));
    return pjsip_create_listener( endpt->transport_mgr, type, addr, addr_name );
}

PJ_DEF(pj_status_t) pjsip_endpt_create_udp_listener( pjsip_endpoint *endpt,
						     pj_sock_t sock,
						     const pj_sockaddr_in *addr_name)
{
    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_create_udp_listener()"));
    return pjsip_create_udp_listener( endpt->transport_mgr, sock, addr_name );
}

PJ_DEF(void) pjsip_endpt_dump( pjsip_endpoint *endpt, pj_bool_t detail )
{
#if PJ_LOG_MAX_LEVEL >= 3
    unsigned count;
    pj_hash_iterator_t itr_val;
    pj_hash_iterator_t *itr;

    PJ_LOG(5, (LOG_THIS, "pjsip_endpt_dump()"));

    /* Lock mutex. */
    pj_mutex_lock(endpt->mutex);

    PJ_LOG(3, (LOG_THIS, "Dumping endpoint %p:", endpt));
    
    /* Dumping pool factory. */
    (*endpt->pf->dump_status)(endpt->pf, detail);

    /* Pool health. */
    PJ_LOG(3, (LOG_THIS," Endpoint pool capacity=%u, used_size=%u",
	       pj_pool_get_capacity(endpt->pool),
	       pj_pool_get_used_size(endpt->pool)));

    /* Transaction tables. */
    count = pj_hash_count(endpt->tsx_table);
    PJ_LOG(3, (LOG_THIS, " Number of transactions: %u", count));

    if (count && detail) {
	pj_hash_iterator_t it_val;
	pj_hash_iterator_t *it;
	pj_time_val now;

	PJ_LOG(3, (LOG_THIS, " Dumping transaction tables:"));

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

	    PJ_LOG(3, (LOG_THIS, "  %s %s %10.*s %.9u %s t=%ds", 
		       tsx->obj_name, role, 
		       tsx->method.name.slen, tsx->method.name.ptr,
		       tsx->cseq,
		       pjsip_tsx_state_str(tsx->state),
		       timeout_diff));

	    it = pj_hash_next(endpt->tsx_table, it);
	}
    }

    /* Transports. 
     * Note: transport is not properly locked in this function.
     *       See pjsip_transport_first, pjsip_transport_next.
     */
    itr = pjsip_transport_first( endpt->transport_mgr, &itr_val );
    if (itr) {
	PJ_LOG(3, (LOG_THIS, " Dumping transports:"));

	do {
	    char src_addr[128], dst_addr[128];
	    int src_port, dst_port;
	    const pj_sockaddr_in *addr;
	    pjsip_transport_t *t;

	    t = pjsip_transport_this(endpt->transport_mgr, itr);
	    addr = pjsip_transport_get_local_addr(t);
	    strcpy(src_addr, pj_sockaddr_get_str_addr(addr));
	    src_port = pj_sockaddr_get_port(addr);

	    addr = pjsip_transport_get_remote_addr(t);
	    strcpy(dst_addr, pj_sockaddr_get_str_addr(addr));
	    dst_port = pj_sockaddr_get_port(addr);

	    PJ_LOG(3, (LOG_THIS, "  %s %s %s:%d --> %s:%d (refcnt=%d)", 
		       pjsip_transport_get_type_name(t),
		       pjsip_transport_get_obj_name(t),
		       src_addr, src_port,
		       dst_addr, dst_port,
		       pjsip_transport_get_ref_cnt(t)));

	    itr = pjsip_transport_next(endpt->transport_mgr, itr);
	} while (itr);
    }

    /* Timer. */
    PJ_LOG(3,(LOG_THIS, " Timer heap has %u entries", 
			pj_timer_heap_count(endpt->timer_heap)));

    /* Unlock mutex. */
    pj_mutex_unlock(endpt->mutex);
#else
    PJ_LOG(3,(LOG_THIS, "pjsip_end_dump: can't dump because it's disabled."));
#endif
}


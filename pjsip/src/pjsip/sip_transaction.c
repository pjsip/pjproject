/* $Header: /pjproject/pjsip/src/pjsip/sip_transaction.c 21    12/02/05 9:05p Bennylp $ */
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
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_config.h>
#include <pjsip/sip_misc.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_endpoint.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/guid.h>
#include <pj/pool.h>

/* Thread Local Storage ID for transaction lock (initialized by endpoint) */
int pjsip_tsx_lock_tls_id;

/* State names */
static const char *state_str[] = 
{
    "Null",
    "Calling",
    "Trying",
    "Proceeding",
    "Completed",
    "Confirmed",
    "Terminated",
    "Destroyed",
};

/* Role names */
static const char *role_name[] = 
{
    "Client",
    "Server"
};

/* Transaction lock. */
typedef struct tsx_lock_data {
    struct tsx_lock_data *prev;
    pjsip_transaction    *tsx;
    int			  is_alive;
} tsx_lock_data;

/* Timer timeout value constants */
static const pj_time_val t1_timer_val = { PJSIP_T1_TIMEOUT/1000, PJSIP_T1_TIMEOUT%1000 };
static const pj_time_val t4_timer_val = { PJSIP_T4_TIMEOUT/1000, PJSIP_T4_TIMEOUT%1000 };
static const pj_time_val td_timer_val = { PJSIP_TD_TIMEOUT/1000, PJSIP_TD_TIMEOUT%1000 };
static const pj_time_val timeout_timer_val = { (64*PJSIP_T1_TIMEOUT)/1000,
					       (64*PJSIP_T1_TIMEOUT)%1000 };

/* Internal timer IDs */
enum Transaction_Timer_Id
{
    TSX_TIMER_RETRANSMISSION,
    TSX_TIMER_TIMEOUT,
};

/* Function Prototypes */
static int  pjsip_tsx_on_state_null( pjsip_transaction *tsx, 
				     pjsip_event *event);
static int  pjsip_tsx_on_state_calling( pjsip_transaction *tsx, 
				        pjsip_event *event);
static int  pjsip_tsx_on_state_trying( pjsip_transaction *tsx, 
				       pjsip_event *event);
static int  pjsip_tsx_on_state_proceeding_uas( pjsip_transaction *tsx, 
					       pjsip_event *event);
static int  pjsip_tsx_on_state_proceeding_uac( pjsip_transaction *tsx, 
					       pjsip_event *event);
static int  pjsip_tsx_on_state_completed_uas( pjsip_transaction *tsx, 
					      pjsip_event *event);
static int  pjsip_tsx_on_state_completed_uac( pjsip_transaction *tsx, 
					      pjsip_event *event);
static int  pjsip_tsx_on_state_confirmed( pjsip_transaction *tsx, 
					  pjsip_event *event);
static int  pjsip_tsx_on_state_terminated( pjsip_transaction *tsx, 
					   pjsip_event *event);
static int  pjsip_tsx_on_state_destroyed( pjsip_transaction *tsx, 
					  pjsip_event *event);
static void tsx_timer_callback( pj_timer_heap_t *theap, 
			        pj_timer_entry *entry);
static int  tsx_send_msg( pjsip_transaction *tsx, pjsip_tx_data *tdata);
static void lock_tsx( pjsip_transaction *tsx, struct tsx_lock_data *lck );
static pj_status_t unlock_tsx( pjsip_transaction *tsx, struct tsx_lock_data *lck );

/* State handlers for UAC, indexed by state */
static int  (*tsx_state_handler_uac[PJSIP_TSX_STATE_MAX])(pjsip_transaction *tsx,
							  pjsip_event *event ) = 
{
    &pjsip_tsx_on_state_null,
    &pjsip_tsx_on_state_calling,
    &pjsip_tsx_on_state_trying,
    &pjsip_tsx_on_state_proceeding_uac,
    &pjsip_tsx_on_state_completed_uac,
    &pjsip_tsx_on_state_confirmed,
    &pjsip_tsx_on_state_terminated,
    &pjsip_tsx_on_state_destroyed,
};

/* State handlers for UAS */
static int  (*tsx_state_handler_uas[PJSIP_TSX_STATE_MAX])(pjsip_transaction *tsx, 
							  pjsip_event *event ) = 
{
    &pjsip_tsx_on_state_null,
    &pjsip_tsx_on_state_calling,
    &pjsip_tsx_on_state_trying,
    &pjsip_tsx_on_state_proceeding_uas,
    &pjsip_tsx_on_state_completed_uas,
    &pjsip_tsx_on_state_confirmed,
    &pjsip_tsx_on_state_terminated,
    &pjsip_tsx_on_state_destroyed,
};

/*
 * Get transaction state name.
 */
PJ_DEF(const char *) pjsip_tsx_state_str(pjsip_tsx_state_e state)
{
    return state_str[state];
}

/*
 * Get the role name.
 */
PJ_DEF(const char *) pjsip_role_name(pjsip_role_e role)
{
    return role_name[role];
}


/*
 * Create transaction key for RFC2543 compliant messages, which don't have
 * unique branch parameter in the top most Via header.
 *
 * INVITE requests matches a transaction if the following attributes
 * match the original request:
 *	- Request-URI
 *	- To tag
 *	- From tag
 *	- Call-ID
 *	- CSeq
 *	- top Via header
 *
 * CANCEL matching is done similarly as INVITE, except:
 *	- CSeq method will differ
 *	- To tag is not matched.
 *
 * ACK matching is done similarly, except that:
 *	- method of the CSeq will differ,
 *	- To tag is matched to the response sent by the server transaction.
 *
 * The transaction key is constructed from the common components of above
 * components. Additional comparison is needed to fully match a transaction.
 */
void create_tsx_key_2543( pj_pool_t *pool,
			  pj_str_t *str,
			  pjsip_role_e role,
			  const pjsip_method *method,
			  const pjsip_rx_data *rdata )
{
#define SEPARATOR   '$'
    char *key, *p, *end;
    int len;
    pj_size_t len_required;
    pjsip_uri *req_uri;
    pj_str_t *host;

    host = &rdata->via->sent_by.host;
    req_uri = (pjsip_uri*)rdata->msg->line.req.uri;

    /* Calculate length required. */
    len_required = PJSIP_MAX_URL_SIZE +	    /* URI */
		   9 +			    /* CSeq number */
		   rdata->from_tag.slen +   /* From tag. */
		   rdata->call_id.slen +    /* Call-ID */
		   host->slen +		    /* Via host. */
		   9 +			    /* Via port. */
		   32;			    /* Separator+Allowance. */
    key = p = pj_pool_alloc(pool, len_required);
    end = p + len_required;

    /* Add role. */
    *p++ = (char)(role==PJSIP_ROLE_UAC ? 'c' : 's');
    *p++ = SEPARATOR;

    /* Add Request-URI */
    /* This is BUG!
     * Response doesn't have Request-URI!
     *
    len = req_uri->vptr->print( PJSIP_URI_IN_REQ_URI, req_uri, p, end-p );
    p += len;
    *p++ = SEPARATOR;
     */

    /* Add method, except when method is INVITE or ACK. */
    if (method->id != PJSIP_INVITE_METHOD && method->id != PJSIP_ACK_METHOD) {
	pj_memcpy(p, method->name.ptr, method->name.slen);
	p += method->name.slen;
	*p++ = '$';
    }

    /* Add CSeq (only the number). */
    len = pj_utoa(rdata->cseq->cseq, p);
    p += len;
    *p++ = SEPARATOR;

    /* Add From tag. */
    len = rdata->from->tag.slen;
    pj_memcpy( p, rdata->from->tag.ptr, len);
    p += len;
    *p++ = SEPARATOR;

    /* Add Call-ID. */
    len = rdata->call_id.slen;
    pj_memcpy( p, rdata->call_id.ptr, len );
    p += len;
    *p++ = SEPARATOR;

    /* Add top Via header. 
     * We don't really care whether the port contains the real port (because
     * it can be omited if default port is used). Anyway this function is 
     * only used to match request retransmission, and we expect that the 
     * request retransmissions will contain the same port.
     */
    if ((end-p) < host->slen + 12) {
	goto on_error;
    }
    pj_memcpy(p, host->ptr, host->slen);
    p += host->slen;
    *p++ = ':';

    len = pj_utoa(rdata->via->sent_by.port, p);
    p += len;
    *p++ = SEPARATOR;
    
    *p++ = '\0';

    /* Done. */
    str->ptr = key;
    str->slen = p-key;

    return;

on_error:
    PJ_LOG(2,("tsx........", "Not enough buffer (%d) for transaction key",
	      len_required));
    pj_assert(0);
    str->ptr = NULL;
    str->slen = 0;
}

/*
 * Create transaction key for RFC3161 compliant system.
 */
void create_tsx_key_3261( pj_pool_t *pool,
			  pj_str_t *key,
			  pjsip_role_e role,
			  const pjsip_method *method,
			  const pj_str_t *branch )
{
    char *p;

    p = key->ptr = pj_pool_alloc(pool, branch->slen + method->name.slen + 4 );
    
    /* Add role. */
    *p++ = (char)(role==PJSIP_ROLE_UAC ? 'c' : 's');
    *p++ = SEPARATOR;

    /* Add method, except when method is INVITE or ACK. */
    if (method->id != PJSIP_INVITE_METHOD && method->id != PJSIP_ACK_METHOD) {
	pj_memcpy(p, method->name.ptr, method->name.slen);
	p += method->name.slen;
	*p++ = '$';
    }

    /* Add branch ID. */
    pj_memcpy(p, branch->ptr, branch->slen);
    p += branch->slen;

    /* Set length */
    key->slen = p - key->ptr;
}

/*
 * Create key from the incoming data, to be used to search the transaction
 * in the transaction hash table.
 */
PJ_DEF(void) pjsip_tsx_create_key( pj_pool_t *pool, pj_str_t *key, 
				   pjsip_role_e role, 
				   const pjsip_method *method, 
				   const pjsip_rx_data *rdata )
{
    pj_str_t rfc3261_branch = {PJSIP_RFC3261_BRANCH_ID, PJSIP_RFC3261_BRANCH_LEN};

    /* Get the branch parameter in the top-most Via.
     * If branch parameter is started with "z9hG4bK", then the message was
     * generated by agent compliant with RFC3261. Otherwise, it will be
     * handled as RFC2543.
     */
    const pj_str_t *branch = &rdata->via->branch_param;

    if (pj_strncmp(branch, &rfc3261_branch, PJSIP_RFC3261_BRANCH_LEN) == 0) {

	/* Create transaction key. */
	create_tsx_key_3261(pool, key, role, method, branch);

    } else {
	/* Create the key for the message. This key will be matched up with the
	 * transaction key. For RFC2563 transactions, the transaction key
	 * was created by the same function, so it will match the message.
	 */
	create_tsx_key_2543( pool, key, role, method, rdata );
    }
}


/*
 * Create new transaction.
 */
PJ_DEF(pjsip_transaction *) pjsip_tsx_create(pj_pool_t *pool,
					     pjsip_endpoint *endpt)
{
    pjsip_transaction *tsx;

    tsx = pj_pool_calloc(pool, 1, sizeof(pjsip_transaction));

    tsx->pool = pool;
    tsx->endpt = endpt;
    tsx->retransmit_timer.id = TSX_TIMER_RETRANSMISSION;
    tsx->retransmit_timer._timer_id = -1;
    tsx->retransmit_timer.user_data = tsx;
    tsx->retransmit_timer.cb = &tsx_timer_callback;
    tsx->timeout_timer.id = TSX_TIMER_TIMEOUT;
    tsx->timeout_timer._timer_id = -1;
    tsx->timeout_timer.user_data = tsx;
    tsx->timeout_timer.cb = &tsx_timer_callback;
    sprintf(tsx->obj_name, "tsx%p", tsx);
    tsx->mutex = pj_mutex_create(pool, "mtsx%p", 0);
    if (!tsx->mutex) {
	return NULL;
    }

    return tsx;
}

/*
 * Lock transaction and set the value of Thread Local Storage.
 */
static void lock_tsx(pjsip_transaction *tsx, struct tsx_lock_data *lck)
{
    struct tsx_lock_data *prev_data;

    pj_mutex_lock(tsx->mutex);
    prev_data = (struct tsx_lock_data *) pj_thread_local_get(pjsip_tsx_lock_tls_id);
    lck->prev = prev_data;
    lck->tsx = tsx;
    lck->is_alive = 1;
    pj_thread_local_set(pjsip_tsx_lock_tls_id, lck);
}


/*
 * Unlock transaction.
 * This will selectively unlock the mutex ONLY IF the transaction has not been 
 * destroyed. The function knows whether the transaction has been destroyed
 * because when transaction is destroyed the is_alive flag for the transaction
 * will be set to zero.
 */
static pj_status_t unlock_tsx(pjsip_transaction *tsx, struct tsx_lock_data *lck)
{
    pj_assert( (void*)pj_thread_local_get(pjsip_tsx_lock_tls_id) == lck);
    pj_assert( lck->tsx == tsx );
    pj_thread_local_set(pjsip_tsx_lock_tls_id, lck->prev);
    if (lck->is_alive)
	pj_mutex_unlock(tsx->mutex);

    return lck->is_alive ? 0 : -1;
}

/*
 * Set transaction state, and inform TU about the transaction state change.
 */
static void tsx_set_state( pjsip_transaction *tsx,
			   pjsip_tsx_state_e state,
			   const pjsip_event *event )
{
    pjsip_event e;

    PJ_LOG(4, (tsx->obj_name, "STATE %s-->%s, ev=%s (src:%s)", 
	       state_str[tsx->state], state_str[state], pjsip_event_str(event->type),
	       pjsip_event_str(event->src_type)));

    /* Change state. */
    tsx->state = state;

    /* Update the state handlers. */
    if (tsx->role == PJSIP_ROLE_UAC) {
	tsx->state_handler = tsx_state_handler_uac[state];
    } else {
	tsx->state_handler = tsx_state_handler_uas[state];
    }

    /* Inform TU */
    pj_memcpy(&e, event, sizeof(*event));
    e.type = PJSIP_EVENT_TSX_STATE_CHANGED;
    e.obj.tsx = tsx;
    pjsip_endpt_send_tsx_event( tsx->endpt, &e  );

    /* When the transaction is terminated, release transport, and free the
     * saved last transmitted message.
     */
    if (state == PJSIP_TSX_STATE_TERMINATED) {

	/* Decrement transport reference counter. */
	if (tsx->transport && tsx->transport_state == PJSIP_TSX_TRANSPORT_STATE_FINAL) {
	    pjsip_transport_dec_ref( tsx->transport );
	    tsx->transport = NULL;
	}
	/* Free last transmitted message. */
	if (tsx->last_tx) {
	    pjsip_tx_data_dec_ref( tsx->last_tx );
	    tsx->last_tx = NULL;
	}
	/* Cancel timeout timer. */
	if (tsx->timeout_timer._timer_id != -1) {
	    pjsip_endpt_cancel_timer(tsx->endpt, &tsx->timeout_timer);
	    tsx->timeout_timer._timer_id = -1;
	}
	/* Cancel retransmission timer. */
	if (tsx->retransmit_timer._timer_id != -1) {
	    pjsip_endpt_cancel_timer(tsx->endpt, &tsx->retransmit_timer);
	    tsx->retransmit_timer._timer_id = -1;
	}

	/* If transport is not pending, reschedule timeout timer to
	 * destroy this transaction.
	 */
	if (tsx->transport_state == PJSIP_TSX_TRANSPORT_STATE_FINAL) {
	    pj_time_val timeout = {0, 0};
	    pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, 
					&timeout);
	}

    } else if (state == PJSIP_TSX_STATE_DESTROYED) {

	/* Clear TLS, so that mutex will not be unlocked */
	struct tsx_lock_data *lck = pj_thread_local_get(pjsip_tsx_lock_tls_id);
	while (lck) {
	    if (lck->tsx == tsx) {
		lck->is_alive = 0;
	    }
	    lck = lck->prev;
	}
    }
}

/*
 * Look-up destination address and select which transport to be used to send
 * the request message. The procedure used here follows the guidelines on 
 * sending the request in RFC3261 chapter 8.1.2.
 *
 * This function also modifies the message (request line and Route headers)
 * accordingly.
 */
static pj_status_t tsx_process_route( pjsip_transaction *tsx,
				      pjsip_tx_data *tdata,
				      pjsip_host_port *send_addr )
{
    const pjsip_uri *new_request_uri, *target_uri;
    const pjsip_name_addr *topmost_route_uri;
    pjsip_route_hdr *first_route_hdr, *last_route_hdr;
    
    pj_assert(tdata->msg->type == PJSIP_REQUEST_MSG);

    /* Get the first "Route" header from the message. If the message doesn't
     * have any "Route" headers but the endpoint has, then copy the "Route"
     * headers from the endpoint first.
     */
    last_route_hdr = first_route_hdr = 
	pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, NULL);
    if (first_route_hdr) {
	topmost_route_uri = &first_route_hdr->name_addr;
	while (last_route_hdr->next != (void*)&tdata->msg->hdr) {
	    pjsip_route_hdr *hdr;
	    hdr = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, last_route_hdr->next);
	    if (!hdr)
		break;
	    last_route_hdr = hdr;
	}
    } else {
	const pjsip_route_hdr *hdr_list;
	hdr_list = (pjsip_route_hdr*)pjsip_endpt_get_routing(tsx->endpt);
	if (hdr_list->next != hdr_list) {
	    const pjsip_route_hdr *hdr = (pjsip_route_hdr*)hdr_list->next;
	    first_route_hdr = NULL;
	    topmost_route_uri = &hdr->name_addr;
	    do {
		last_route_hdr = pjsip_hdr_shallow_clone(tdata->pool, hdr);
		if (first_route_hdr == NULL)
		    first_route_hdr = last_route_hdr;
		pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)last_route_hdr);
		hdr = hdr->next;
	    } while (hdr != hdr_list);
	} else {
	    topmost_route_uri = NULL;
	}
    }

    /* If Route headers exist, and the first element indicates loose-route,
     * the URI is taken from the Request-URI, and we keep all existing Route
     * headers intact.
     * If Route headers exist, and the first element DOESN'T indicate loose
     * route, the URI is taken from the first Route header, and remove the
     * first Route header from the message.
     * Otherwise if there's no Route headers, the URI is taken from the
     * Request-URI.
     */
    if (topmost_route_uri) {
	pj_bool_t has_lr_param;

	if (PJSIP_URI_SCHEME_IS_SIP(topmost_route_uri) ||
	    PJSIP_URI_SCHEME_IS_SIPS(topmost_route_uri))
	{
	    const pjsip_url *url = pjsip_uri_get_uri((void*)topmost_route_uri);
	    has_lr_param = url->lr_param;
	} else {
	    has_lr_param = 0;
	}

	if (has_lr_param) {
	    new_request_uri = tdata->msg->line.req.uri;
	    /* We shouldn't need to delete topmost Route if it has lr param.
	     * But seems like it breaks some proxy implementation, so we
	     * delete it anyway.
	     */
	    /*
	    pj_list_erase(first_route_hdr);
	    if (first_route_hdr == last_route_hdr)
		last_route_hdr = NULL;
	    */
	} else {
	    new_request_uri = pjsip_uri_get_uri((void*)topmost_route_uri);
	    pj_list_erase(first_route_hdr);
	    if (first_route_hdr == last_route_hdr)
		last_route_hdr = NULL;
	}

	target_uri = (pjsip_uri*)topmost_route_uri;

    } else {
	target_uri = new_request_uri = tdata->msg->line.req.uri;
    }

    /* The target URI must be a SIP/SIPS URL so we can resolve it's address.
     * Otherwise we're in trouble (i.e. there's no host part in tel: URL).
     */
    pj_memset(send_addr, 0, sizeof(*send_addr));

    if (PJSIP_URI_SCHEME_IS_SIPS(target_uri)) {
	pjsip_uri *uri = (pjsip_uri*) target_uri;
	const pjsip_url *url = (const pjsip_url*)pjsip_uri_get_uri(uri);
	send_addr->flag |= (PJSIP_TRANSPORT_SECURE | PJSIP_TRANSPORT_RELIABLE);
	pj_strdup(tdata->pool, &send_addr->host, &url->host);
        send_addr->port = url->port;
	send_addr->type = pjsip_transport_get_type_from_name(&url->transport_param);

    } else if (PJSIP_URI_SCHEME_IS_SIP(target_uri)) {
	pjsip_uri *uri = (pjsip_uri*) target_uri;
	const pjsip_url *url = (const pjsip_url*)pjsip_uri_get_uri(uri);
	pj_strdup(tdata->pool, &send_addr->host, &url->host);
	send_addr->port = url->port;
	send_addr->type = pjsip_transport_get_type_from_name(&url->transport_param);
#if PJ_HAS_TCP
	if (send_addr->type == PJSIP_TRANSPORT_TCP || 
	    send_addr->type == PJSIP_TRANSPORT_SCTP) 
	{
	    send_addr->flag |= PJSIP_TRANSPORT_RELIABLE;
	}
#endif
    } else {
	PJ_LOG(2, (tsx->obj_name, "Unable to lookup destination address for "
				  "non SIP-URL"));   
	return -1;
    }

    /* If target URI is different than request URI, replace 
     * request URI add put the original URI in the last Route header.
     */
    if (new_request_uri && new_request_uri!=tdata->msg->line.req.uri) {
	pjsip_route_hdr *route = pjsip_route_hdr_create(tdata->pool);
	route->name_addr.uri = tdata->msg->line.req.uri;
	if (last_route_hdr)
	    pj_list_insert_after(last_route_hdr, route);
	else
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)route);
	tdata->msg->line.req.uri = (pjsip_uri*)new_request_uri;
    }

    /* Success. */
    return 0;  
}


/*
 * Callback from the transport job.
 * This callback is called when asychronous transport connect() operation
 * has completed, with or without error.
 */
static void tsx_transport_callback(pjsip_transport_t *tr, 
				   void *token, 
				   pj_status_t status)
{
    char addr[PJ_MAX_HOSTNAME];
    pjsip_transaction *tsx = token;
    struct tsx_lock_data lck;

    pj_memcpy(addr, tsx->dest_name.host.ptr, tsx->dest_name.host.slen);
    addr[tsx->dest_name.host.slen] = '\0';


    if (status == PJ_OK) {
	PJ_LOG(4, (tsx->obj_name, "%s connected to %s:%d",
				  pjsip_transport_get_type_name(tr),
				  addr, tsx->dest_name.port));
    } else {
	PJ_LOG(3, (tsx->obj_name, "%s unable to connect to %s:%d, status=%d", 
				  pjsip_transport_get_type_name(tr),
				  addr, tsx->dest_name.port, status));
    }

    /* Lock transaction. */
    lock_tsx(tsx, &lck);

    if (status != PJ_OK) {
	pjsip_event event;
	
	event.type = PJSIP_EVENT_TRANSPORT_ERROR;
	event.src_type = PJSIP_EVENT_TX_MSG;
	event.src.tdata = tsx->last_tx;

	tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;
	tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;
	tsx_set_state(tsx, PJSIP_TSX_STATE_TERMINATED, &event);

	/* Unlock transaction. */
	unlock_tsx(tsx, &lck);
	return;
    }

    /* See if transaction has already been terminated. If so, schedule to destroy
     * the transaction.
     */
    if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {
	pj_time_val timeout = {0, 0};
	pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, 
				    &timeout);

	/* Unlock transaction. */
	unlock_tsx(tsx, &lck);
	return;
    }

    /* Add reference counter to the transport. */
    pjsip_transport_add_ref(tr);

    /* Mark transport as ready. */
    tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;
    tsx->transport = tr;

    /* If there's a pending message to send, send it now. */
    if (tsx->has_unsent_msg) {
	tsx_send_msg( tsx, tsx->last_tx );
    }

    /* Unlock transaction. */
    unlock_tsx(tsx, &lck);
}

/*
 * Callback from the resolver job.
 */
static void tsx_resolver_callback(pj_status_t status,
				  void *token,
				  const struct pjsip_server_addresses *addr)
{
    pjsip_transaction *tsx = token;
    struct tsx_lock_data lck;

    PJ_LOG(4, (tsx->obj_name, "resolver job complete, status=%d", status));

    if (status != PJ_OK || addr->count == 0) {
	pjsip_event event;
	
	event.type = PJSIP_EVENT_TRANSPORT_ERROR;
	event.src_type = PJSIP_EVENT_TX_MSG;
	event.src.tdata = tsx->last_tx;

	lock_tsx(tsx, &lck);
	tsx->status_code = PJSIP_SC_TSX_RESOLVE_ERROR;
	tsx_set_state(tsx, PJSIP_TSX_STATE_TERMINATED, &event);
	unlock_tsx(tsx, &lck);
	return;
    }

    /* Lock transaction. */
    lock_tsx(tsx, &lck);

    /* Copy server addresses. */
    pj_memcpy(&tsx->remote_addr, addr, sizeof(*addr));

    /* Create/find the transport for the remote address. */
    PJ_LOG(5,(tsx->obj_name, "tsx getting transport for %s:%d",
			     pj_sockaddr_get_str_addr(&addr->entry[0].addr),
			     pj_sockaddr_get_port(&addr->entry[0].addr)));

    tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_CONNECTING;
    pjsip_endpt_get_transport(tsx->endpt, tsx->pool,
			      addr->entry[0].type, &addr->entry[0].addr,
			      tsx,
			      &tsx_transport_callback);

    /* Unlock transaction */
    unlock_tsx(tsx, &lck);

    /* There should be nothing to do after this point.
     * Execution for the transaction will resume when the callback for the 
     * transport is called.
     */
}

/*
 * Initialize the transaction as UAC transaction.
 */
PJ_DEF(pj_status_t) pjsip_tsx_init_uac( pjsip_transaction *tsx, 
					pjsip_tx_data *tdata)
{
    pjsip_msg *msg;
    pjsip_cseq_hdr *cseq;
    pjsip_via_hdr *via;
    struct tsx_lock_data lck;
    const pjsip_hdr *endpt_hdr;

    PJ_LOG(4,(tsx->obj_name, "initializing tsx as UAC (tdata=%p)", tdata));

    /* Lock transaction. */
    lock_tsx(tsx, &lck);

    /* Keep shortcut */
    msg = tdata->msg;

    /* Role is UAC. */
    tsx->role = PJSIP_ROLE_UAC;

    /* Save method. */
    pjsip_method_copy( tsx->pool, &tsx->method, &msg->line.req.method);

    /* Generate branch parameter if it doesn't exist. */
    via = pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL);
    if (via == NULL) {
	via = pjsip_via_hdr_create(tdata->pool);
	pjsip_msg_insert_first_hdr(msg, (pjsip_hdr*) via);
    }

    if (via->branch_param.slen == 0) {
	pj_str_t tmp;
	via->branch_param.ptr = pj_pool_alloc(tsx->pool, PJSIP_MAX_BRANCH_LEN);
	via->branch_param.slen = PJSIP_MAX_BRANCH_LEN;
	pj_memcpy(via->branch_param.ptr, PJSIP_RFC3261_BRANCH_ID, 
		  PJSIP_RFC3261_BRANCH_LEN);

	tmp.ptr = via->branch_param.ptr + PJSIP_RFC3261_BRANCH_LEN;
	pj_generate_unique_string( &tmp );
    }

    /* Copy branch parameter. */
    tsx->branch = via->branch_param;

    /* Add additional request headers from endpoint. */
    endpt_hdr = pjsip_endpt_get_request_headers(tsx->endpt)->next;
    while (endpt_hdr != pjsip_endpt_get_request_headers(tsx->endpt)) {
	pjsip_hdr *hdr = pjsip_hdr_shallow_clone(tdata->pool, endpt_hdr);
	pjsip_msg_add_hdr( tdata->msg, hdr );
	endpt_hdr = endpt_hdr->next;
    }

    /* Generate transaction key. */
    create_tsx_key_3261( tsx->pool, &tsx->transaction_key,
			 PJSIP_ROLE_UAC, &tsx->method, 
			 &via->branch_param);

    PJ_LOG(6, (tsx->obj_name, "tsx_key=%.*s", tsx->transaction_key.slen,
	       tsx->transaction_key.ptr));

    /* Save CSeq. */
    cseq = pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    if (!cseq) {
	PJ_LOG(4,(tsx->obj_name, "CSeq header not present in outgoing message!"));
	return -1;
    }
    tsx->cseq = cseq->cseq;


    /* Begin with State_Null.
     * Manually set-up the state becase we don't want to call the callback.
     */
    tsx->state = PJSIP_TSX_STATE_NULL;
    tsx->state_handler = pjsip_tsx_on_state_null;

    /* Get destination name from the message. */
    if (tsx_process_route(tsx, tdata, &tsx->dest_name) != 0) {
	pjsip_event event;
	PJ_LOG(3,(tsx->obj_name, "Error: unable to get destination address for request"));
	
	event.type = PJSIP_EVENT_TRANSPORT_ERROR;
	event.src_type = PJSIP_EVENT_TX_MSG;
	event.src.tdata = tsx->last_tx;

	tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;
	tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;
	tsx_set_state(tsx, PJSIP_TSX_STATE_TERMINATED, &event);
	unlock_tsx(tsx, &lck);
	return -1;
    }

    /* Resolve destination.
     * This will start asynchronous resolver job, and when it finishes, 
     * the callback will be called.
     */
    PJ_LOG(5,(tsx->obj_name, "tsx resolving destination %.*s:%d",
			     tsx->dest_name.host.slen, 
			     tsx->dest_name.host.ptr,
			     tsx->dest_name.port));

    tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_RESOLVING;
    pjsip_endpt_resolve( tsx->endpt, tsx->pool, &tsx->dest_name, 
			 tsx, &tsx_resolver_callback);

    /* There should be nothing to do after this point. 
     * Execution for the transaction will resume when the resolver callback is
     * called.
     */

    /* Unlock transaction and return.
     * If transaction has been destroyed WITHIN the current thread, the 
     * unlock_tsx() function will return -1.
     */
    return unlock_tsx(tsx, &lck);
}


/*
 * Initialize the transaction as UAS transaction.
 */
PJ_DEF(pj_status_t) pjsip_tsx_init_uas( pjsip_transaction *tsx, 
					pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg;
    pj_str_t *branch;
    pjsip_cseq_hdr *cseq;
    struct tsx_lock_data lck;

    PJ_LOG(4,(tsx->obj_name, "initializing tsx as UAS (rdata=%p)", rdata));

    /* Lock transaction. */
    lock_tsx(tsx, &lck);

    /* Keep shortcut to message */
    msg = rdata->msg;

    /* Role is UAS */
    tsx->role = PJSIP_ROLE_UAS;

    /* Save method. */
    pjsip_method_copy( tsx->pool, &tsx->method, &msg->line.req.method);

    /* Get transaction key either from branch for RFC3261 message, or
     * create transaction key.
     */
    pjsip_tsx_create_key(tsx->pool, &tsx->transaction_key, PJSIP_ROLE_UAS, 
			 &tsx->method, rdata);

    /* Duplicate branch parameter for transaction. */
    branch = &rdata->via->branch_param;
    pj_strdup(tsx->pool, &tsx->branch, branch);

    PJ_LOG(6, (tsx->obj_name, "tsx_key=%.*s", tsx->transaction_key.slen,
	       tsx->transaction_key.ptr));

    /* Save CSeq */
    cseq = rdata->cseq;
    tsx->cseq = cseq->cseq;

    /* Begin with state NULL
     * Manually set-up the state becase we don't want to call the callback.
     */
    tsx->state = PJSIP_TSX_STATE_NULL; 
    tsx->state_handler = &pjsip_tsx_on_state_null;

    /* Get the transport to send the response. 
     * According to section 18.2.2 of RFC3261, if the transport is reliable
     * then the response must be sent using that transport.
     */
    /* In addition, RFC 3581 says, if Via has "rport" parameter specified,
     * then return the response using the same transport.
     */
    if (PJSIP_TRANSPORT_IS_RELIABLE(rdata->transport) || 
	rdata->via->rport_param >= 0) 
    {
	tsx->transport = rdata->transport;
	pjsip_transport_add_ref(tsx->transport);
	tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;

	tsx->current_addr = 0;
	tsx->remote_addr.count = 1;
	tsx->remote_addr.entry[0].type = pjsip_transport_get_type(tsx->transport);
	pj_memcpy(&tsx->remote_addr.entry[0].addr, 
		  &rdata->addr, rdata->addr_len);
	
    } else {
	pj_status_t status;

	status = pjsip_get_response_addr(tsx->pool, rdata->transport,
					 rdata->via, &tsx->dest_name);
	if (status != PJ_OK) {
	    pjsip_event event;
	    PJ_LOG(2,(tsx->obj_name, "Unable to get destination address "
				     "for response"));
	    
	    event.type = PJSIP_EVENT_TRANSPORT_ERROR;
	    event.src_type = PJSIP_EVENT_TX_MSG;
	    event.src.tdata = tsx->last_tx;

	    tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;
	    tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;
	    tsx_set_state(tsx, PJSIP_TSX_STATE_TERMINATED, &event);
	    unlock_tsx(tsx, &lck);
	    return status;
	}

	/* Resolve destination.
	 * This will start asynchronous resolver job, and when it finishes, 
	 * the callback will be called.
	 */
	PJ_LOG(5,(tsx->obj_name, "tsx resolving destination %.*s:%d",
				 tsx->dest_name.host.slen, tsx->dest_name.host.ptr,
				 tsx->dest_name.port));

	tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_RESOLVING;
	pjsip_endpt_resolve( tsx->endpt, tsx->pool, &tsx->dest_name, 
			     tsx, &tsx_resolver_callback);
    }
    
    /* There should be nothing to do after this point. 
     * Execution for the transaction will resume when the resolver callback is
     * called.
     */

    /* Unlock transaction and return.
     * If transaction has been destroyed WITHIN the current thread, the 
     * unlock_tsx() function will return -1.
     */
    return unlock_tsx(tsx, &lck);
}

/*
 * Callback when timer expires.
 */
static void tsx_timer_callback( pj_timer_heap_t *theap, pj_timer_entry *entry)
{
    pjsip_event event;
    pjsip_transaction *tsx = entry->user_data;
    struct tsx_lock_data lck;

    PJ_UNUSED_ARG(theap);

    PJ_LOG(5,(tsx->obj_name, "got timer event (%s timer)", 
	     (entry->id == TSX_TIMER_RETRANSMISSION ? "Retransmit" : "Timeout")));

    event.type = event.src_type = PJSIP_EVENT_TIMER;
    event.src.timer = (entry->id == TSX_TIMER_RETRANSMISSION ? 
	&tsx->retransmit_timer : &tsx->timeout_timer);

    /* Dispatch event to transaction. */
    lock_tsx(tsx, &lck);
    (*tsx->state_handler)(tsx, &event);
    unlock_tsx(tsx, &lck);
}

/*
 * Transmit ACK message for 2xx/INVITE with this transaction. The ACK for
 * non-2xx/INVITE is automatically sent by the transaction.
 * This operation is only valid if the transaction is configured to handle ACK
 * (tsx->handle_ack is non-zero). If this attribute is not set, then the
 * transaction will comply with RFC-3261, i.e. it will set itself to 
 * TERMINATED state when it receives 2xx/INVITE.
 */
PJ_DEF(void) pjsip_tsx_on_tx_ack( pjsip_transaction *tsx, pjsip_tx_data *tdata)
{
    pjsip_msg *msg;
    pjsip_host_port dest_addr;
    pjsip_event event;
    pjsip_via_hdr *via;
    struct tsx_lock_data lck;

    lock_tsx(tsx, &lck);

    pj_assert(tsx->handle_ack != 0);
    
    msg = tdata->msg;

    /* Generate branch parameter if it doesn't exist. */
    via = pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL);
    if (via == NULL) {
	via = pjsip_via_hdr_create(tdata->pool);
	pjsip_msg_add_hdr(msg, (pjsip_hdr*) via);
    }

    if (via->branch_param.slen == 0) {
	via->branch_param = tsx->branch;
    } else {
	pj_assert( pj_strcmp(&via->branch_param, &tsx->branch) == 0 );
    }

    /* Get destination name from the message. */
    if (tsx_process_route(tsx, tdata, &dest_addr) != 0){
	PJ_LOG(2,(tsx->obj_name, "Unable to get destination address for request"));
	goto on_error;
    }

    /* Compare message's destination name with transaction's destination name.
     * If NOT equal, then we'll have to resolve the destination.
     */
    if (dest_addr.type == tsx->dest_name.type &&
	dest_addr.flag == tsx->dest_name.flag &&
	dest_addr.port == tsx->dest_name.port &&
	pj_stricmp(&dest_addr.host, &tsx->dest_name.host) == 0)
    {
	/* Equal destination. We can use current transport. */
	pjsip_tsx_on_tx_msg(tsx, tdata);
	unlock_tsx(tsx, &lck);
	return;

    }

    /* New destination; we'll have to resolve host and create new transport. */
    pj_memcpy(&tsx->dest_name, &dest_addr, sizeof(dest_addr));
    pj_strdup(tsx->pool, &tsx->dest_name.host, &dest_addr.host);

    PJ_LOG(5,(tsx->obj_name, "tsx resolving destination %.*s:%d",
			     tsx->dest_name.host.slen, 
			     tsx->dest_name.host.ptr,
			     tsx->dest_name.port));

    tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_RESOLVING;
    pjsip_transport_dec_ref(tsx->transport);
    tsx->transport = NULL;

    /* Put the message in queue. */
    pjsip_tsx_on_tx_msg(tsx, tdata);

    /* This is a bug!
     * We shouldn't change transaction's state before actually sending the
     * message. Otherwise transaction will terminate before message is sent,
     * and timeout timer will be scheduled.
     */
    PJ_TODO(TSX_DONT_CHANGE_STATE_BEFORE_SENDING_ACK)

    /* 
     * This will start asynchronous resolver job, and when it finishes, 
     * the callback will be called.
     */

    tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_RESOLVING;
    pjsip_endpt_resolve( tsx->endpt, tsx->pool, &tsx->dest_name, 
			 tsx, &tsx_resolver_callback);

    unlock_tsx(tsx, &lck);

    /* There should be nothing to do after this point. 
     * Execution for the transaction will resume when the resolver callback is
     * called.
     */
    return;

on_error:
    /* Failure condition. 
     * Send TERMINATED event.
     */
    tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;
    event.type = PJSIP_EVENT_TRANSPORT_ERROR;
    event.src_type = PJSIP_EVENT_TX_MSG;
    event.src.tdata = tdata;
    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, &event);

    unlock_tsx(tsx, &lck);
}


/*
 * This function is called by TU to send a message.
 */
PJ_DEF(void) pjsip_tsx_on_tx_msg( pjsip_transaction *tsx,
				  pjsip_tx_data *tdata )
{
    pjsip_event event;
    struct tsx_lock_data lck;

    PJ_LOG(5,(tsx->obj_name, "on transmit msg (tdata=%p)", tdata));

    event.type = event.src_type = PJSIP_EVENT_TX_MSG;
    event.src.tdata = tdata;

    PJ_LOG(5,(tsx->obj_name, "on state %s (ev=%s, src=%s, data=%p)", 
	      state_str[tsx->state], pjsip_event_str(event.type), 
	      pjsip_event_str(event.src_type), event.src.data));

    /* Dispatch to transaction. */
    lock_tsx(tsx, &lck);
    (*tsx->state_handler)(tsx, &event);
    unlock_tsx(tsx, &lck);
}

/*
 * This function is called by endpoint when incoming message for the 
 * transaction is received.
 */
PJ_DEF(void) pjsip_tsx_on_rx_msg( pjsip_transaction *tsx,
				  pjsip_rx_data *rdata)
{
    pjsip_event event;
    struct tsx_lock_data lck;

    event.type = event.src_type = PJSIP_EVENT_RX_MSG;
    event.src.rdata = rdata;

    PJ_LOG(5,(tsx->obj_name, "on state %s (ev=%s, src=%s, data=%p)", 
	      state_str[tsx->state], pjsip_event_str(event.type), 
	      pjsip_event_str(event.src_type), event.src.data));

    /* Dispatch to transaction. */
    lock_tsx(tsx, &lck);
    (*tsx->state_handler)(tsx, &event);
    unlock_tsx(tsx, &lck);
}

/*
 * Forcely terminate transaction.
 */
PJ_DEF(void) pjsip_tsx_terminate( pjsip_transaction *tsx, int code )
{
    pjsip_event event;
    struct tsx_lock_data lck;

    lock_tsx(tsx, &lck);

    tsx->status_code = code;
    event.type = PJSIP_EVENT_USER;
    event.src_type = PJSIP_EVENT_USER;
    event.src.data = 0;
    event.obj.tsx = tsx;
    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, &event);

    unlock_tsx(tsx, &lck);
}

/*
 * Send message to the transport.
 * If transport is not yet available, then do nothing. The message will be
 * transmitted when transport connection completion callback is called.
 */
static int tsx_send_msg( pjsip_transaction *tsx, pjsip_tx_data *tdata)
{
    pjsip_event event;

    PJ_LOG(5,(tsx->obj_name, "sending msg (tdata=%p)", tdata));

    if (tsx->transport_state == PJSIP_TSX_TRANSPORT_STATE_FINAL) {
	int sent;
	pjsip_event before_tx_event;

	pj_assert(tsx->transport != NULL);

	/* Make sure Via transport info is filled up properly for
	 * requests. 
	 */
	if (tdata->msg->type == PJSIP_REQUEST_MSG) {
	    const pj_sockaddr_in *addr_name;
	    pjsip_via_hdr *via = (pjsip_via_hdr*) 
		pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);

	    /* For request message, set "rport" parameter by default. */
	    if (tdata->msg->type == PJSIP_REQUEST_MSG)
		via->rport_param = 0;

	    /* Don't update Via sent-by on retransmission. */
	    if (via->sent_by.host.slen == 0) {
		addr_name = pjsip_transport_get_addr_name(tsx->transport);
		pj_strdup2(tdata->pool, &via->transport, 
			pjsip_transport_get_type_name(tsx->transport));
		pj_strdup2(tdata->pool, &via->sent_by.host, 
			pj_sockaddr_get_str_addr(addr_name));
		via->sent_by.port = pj_sockaddr_get_port(addr_name);
	    }
	}

	/* Notify everybody we're about to send message. */
	before_tx_event.type = PJSIP_EVENT_BEFORE_TX;
	before_tx_event.src_type = PJSIP_EVENT_TX_MSG;
	before_tx_event.obj.tsx = tsx;
	before_tx_event.src.tdata = tdata;
	before_tx_event.data.long_data = tsx->retransmit_count;
	pjsip_endpt_send_tsx_event( tsx->endpt, &before_tx_event  );

	tsx->has_unsent_msg = 0;
	sent = pjsip_transport_send_msg(
		tsx->transport, tdata,
		&tsx->remote_addr.entry[tsx->current_addr].addr
		);
	if (sent < 1) {
	    goto on_error;
	}
    } else {
	tsx->has_unsent_msg = 1;
    }

    return 0;

on_error:
    tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;
    event.type = PJSIP_EVENT_TRANSPORT_ERROR;
    event.src_type = PJSIP_EVENT_TX_MSG;
    event.src.tdata = tdata;
    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, &event);
    return -1;
}

/*
 * Retransmit last message sent.
 */
static pj_status_t pjsip_tsx_retransmit( pjsip_transaction *tsx,
					 int should_restart_timer)
{
    PJ_LOG(4,(tsx->obj_name, "retransmiting (tdata=%p, count=%d, restart?=%d)", 
	      tsx->last_tx, tsx->retransmit_count, should_restart_timer));

    pj_assert(tsx->last_tx != NULL);

    ++tsx->retransmit_count;

    if (tsx_send_msg( tsx, tsx->last_tx) != 0) {
	return -1;
    }
    
    /* Restart timer T1. */
    if (should_restart_timer) {
	pj_time_val timeout;
	int msec_time = (1 << (tsx->retransmit_count)) * PJSIP_T1_TIMEOUT;

	if (tsx->method.id != PJSIP_INVITE_METHOD && msec_time > PJSIP_T2_TIMEOUT) 
	    msec_time = PJSIP_T2_TIMEOUT;

	timeout.sec = msec_time / 1000;
	timeout.msec = msec_time % 1000;
	pjsip_endpt_schedule_timer( tsx->endpt, &tsx->retransmit_timer, &timeout);
    }

    return PJ_OK;
}

/*
 * Handler for events in state Null.
 */
static int  pjsip_tsx_on_state_null( pjsip_transaction *tsx, pjsip_event *event )
{
    pj_assert( tsx->state == PJSIP_TSX_STATE_NULL);
    pj_assert( tsx->last_tx == NULL );
    pj_assert( tsx->has_unsent_msg == 0);

    if (tsx->role == PJSIP_ROLE_UAS) {

	/* Set state to Trying. */
	pj_assert(event->type == PJSIP_EVENT_RX_MSG);
	tsx_set_state( tsx, PJSIP_TSX_STATE_TRYING, event);

    } else {
	pjsip_tx_data *tdata = event->src.tdata;

	/* Save the message for retransmission. */
	tsx->last_tx = tdata;
	pjsip_tx_data_add_ref(tdata);

	/* Send the message. */
	if (tsx_send_msg( tsx, tdata) != 0) {
	    return -1;
	}

	/* Start Timer B (or called timer F for non-INVITE) for transaction 
	 * timeout.
	 */
	pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, &timeout_timer_val);

	/* Start Timer A (or timer E) for retransmission only if unreliable 
	 * transport is being used.
	 */
	if (tsx->transport_state == PJSIP_TSX_TRANSPORT_STATE_FINAL &&
	    PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport)==0) 
	{
	    pjsip_endpt_schedule_timer(tsx->endpt, &tsx->retransmit_timer, &t1_timer_val);
	    tsx->retransmit_count = 0;
	}

	/* Move state. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_CALLING, event);
    }

    return PJ_OK;
}

/*
 * State Calling is for UAC after it sends request but before any responses
 * is received.
 */
static int  pjsip_tsx_on_state_calling( pjsip_transaction *tsx, 
				        pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_CALLING);
    pj_assert(tsx->role == PJSIP_ROLE_UAC);

    if (event->type == PJSIP_EVENT_TIMER && 
	event->src.timer == &tsx->retransmit_timer) 
    {

	/* Retransmit the request. */
	if (pjsip_tsx_retransmit( tsx, 1 ) != 0) {
	    return -1;
	}

    } else if (event->type == PJSIP_EVENT_TIMER && 
	      event->src.timer == &tsx->timeout_timer) 
    {

	/* Cancel retransmission timer. */
	if (PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport)==0) {
	    pjsip_endpt_cancel_timer(tsx->endpt, &tsx->retransmit_timer);
	}

	/* Set status code */
	tsx->status_code = PJSIP_SC_TSX_TIMEOUT;

	/* Inform TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, event );

	/* Transaction is destroyed */
	return -1;

    } else if (event->type == PJSIP_EVENT_RX_MSG) {
	int code;

	/* Cancel retransmission timer A. */
	if (PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport)==0)
	    pjsip_endpt_cancel_timer(tsx->endpt, &tsx->retransmit_timer);

	/* Cancel timer B (transaction timeout) */
	pjsip_endpt_cancel_timer(tsx->endpt, &tsx->timeout_timer);

	/* Discard retransmission message if it is not INVITE.
	 * The INVITE tdata is needed in case we have to generate ACK for
	 * the final response.
	 */
	/* Keep last_tx for authorization. */
	/* SOME PROXIES THEY ALWAYS SEND 100 RESPONSE, CAUSING
	 * tdata TO BE DESTROYED!!
	code = event->src.rdata->msg->line.status.code;
	if (tsx->method.id != PJSIP_INVITE_METHOD && code!=401 && code!=407) {
	    pjsip_tx_data_dec_ref(tsx->last_tx);
	    tsx->last_tx = NULL;
	}
	*/

	/* Processing is similar to state Proceeding. */
	pjsip_tsx_on_state_proceeding_uac( tsx, event);

    } else {
	pj_assert(0);
    }

    return PJ_OK;
}

/*
 * State Trying is for UAS after it received request but before any responses
 * is sent.
 * Note: this is different than RFC3261, which can use Trying state for
 *	 non-INVITE client transaction (bug in RFC?).
 */
static int  pjsip_tsx_on_state_trying( pjsip_transaction *tsx, pjsip_event *event)
{
    int result;

    pj_assert(tsx->state == PJSIP_TSX_STATE_TRYING);

    /* This state is only for UAS */
    pj_assert(tsx->role == PJSIP_ROLE_UAS);

    /* Better be transmission of response message.
     * If we've got request retransmission, this means that the TU hasn't
     * transmitted any responses within 500 ms, which is not allowed. If
     * this happens, just ignore the event (we couldn't retransmit last
     * response because we haven't sent any!).
     */
    //pj_assert(event->type == PJSIP_EVENT_TX_MSG);
    if (event->type != PJSIP_EVENT_TX_MSG) {
	return PJ_OK;
    }

    /* The rest of the processing of the event is exactly the same as in
     * "Proceeding" state.
     */
    result = pjsip_tsx_on_state_proceeding_uas( tsx, event);

    /* Inform the TU of the state transision if state is still State_Trying */
    if (result==0 && tsx->state == PJSIP_TSX_STATE_TRYING) {
	tsx_set_state( tsx, PJSIP_TSX_STATE_PROCEEDING, event);
    }

    return result;
}

/*
 * Handler for events in Proceeding for UAS
 * This state happens after the TU sends provisional response.
 */
static int pjsip_tsx_on_state_proceeding_uas( pjsip_transaction *tsx, pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_PROCEEDING || 
	      tsx->state == PJSIP_TSX_STATE_TRYING);

    /* This state is only for UAS. */
    pj_assert(tsx->role == PJSIP_ROLE_UAS);

    /* Receive request retransmission. */
    if (event->type == PJSIP_EVENT_RX_MSG) {

	/* Send last response. */
	if (pjsip_tsx_retransmit( tsx, 0 ) != 0) {
	    return -1;
	}
	
    } else if (event->type == PJSIP_EVENT_TX_MSG ) {
	pjsip_tx_data *tdata = event->src.tdata;

	/* The TU sends response message to the request. Save this message so
	 * that we can retransmit the last response in case we receive request
	 * retransmission.
	 */
	pjsip_msg *msg = tdata->msg;

	/* This can only be a response message. */
	pj_assert(msg->type == PJSIP_RESPONSE_MSG);

	/* Status code must be higher than last sent. */
	pj_assert(msg->line.status.code >= tsx->status_code);

	/* Update last status */
	tsx->status_code = msg->line.status.code;

	/* Discard the saved last response (it will be updated later as
	 * necessary).
	 */
	if (tsx->last_tx && tsx->last_tx != tdata) {
	    pjsip_tx_data_dec_ref( tsx->last_tx );
	    tsx->last_tx = NULL;
	}

	/* Send the message. */
	if (tsx_send_msg(tsx, tdata) != 0) {
	    return -1;
	}

	// Update To tag header for RFC2543 transaction.
	// TODO:

	/* Update transaction state */
	if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 100)) {

	    if (tsx->last_tx != tdata) {
		tsx->last_tx = tdata;
		pjsip_tx_data_add_ref( tdata );
	    }
	    tsx_set_state( tsx, PJSIP_TSX_STATE_PROCEEDING, event);

	} else if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 200)) {

	    if (tsx->method.id == PJSIP_INVITE_METHOD && tsx->handle_ack==0) {

		/* 2xx class message is not saved, because retransmission is handled
		 * by the TU.
		 */
		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, event);

		/* Transaction is destroyed. */
		return -1;

	    } else {
		pj_time_val timeout;

		if (tsx->method.id == PJSIP_INVITE_METHOD) {
		    tsx->retransmit_count = 0;
		    pjsip_endpt_schedule_timer( tsx->endpt, &tsx->retransmit_timer, 
						&t1_timer_val);
		}

		/* Save last response sent for retransmission when request 
		 * retransmission is received.
		 */
		if (tsx->last_tx != tdata) {
		    tsx->last_tx = tdata;
		    pjsip_tx_data_add_ref(tdata);
		}

		/* Start timer J at 64*T1 for unreliable transport or zero for
		 * reliable transport.
		 */
		if (PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport)==0) {
		    timeout = timeout_timer_val;
		} else {
		    timeout.sec = timeout.msec = 0;
		}

		pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, &timeout);

		/* Set state to "Completed" */
		tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, event);
	    }

	} else if (tsx->status_code >= 300) {

	    /* 3xx-6xx class message causes transaction to move to "Completed" state. */
	    if (tsx->last_tx != tdata) {
		tsx->last_tx = tdata;
		pjsip_tx_data_add_ref( tdata );
	    }

	    /* Start timer H for transaction termination */
	    pjsip_endpt_schedule_timer(tsx->endpt,&tsx->timeout_timer,&timeout_timer_val);

	    /* For INVITE, if unreliable transport is used, retransmission 
	     * timer G will be scheduled (retransmission).
	     */
	    if (PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport)==0) {
		pjsip_cseq_hdr *cseq = pjsip_msg_find_hdr( msg, PJSIP_H_CSEQ, NULL);
		if (cseq->method.id == PJSIP_INVITE_METHOD) {
		    tsx->retransmit_count = 0;
		    pjsip_endpt_schedule_timer(tsx->endpt, &tsx->retransmit_timer, 
					       &t1_timer_val);
		}
	    }

	    /* Inform TU */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, event);

	} else {
	    pj_assert(0);
	}


    } else if (event->type == PJSIP_EVENT_TIMER && 
	       event->src.timer == &tsx->retransmit_timer) {
	/* Retransmission timer elapsed. */

	/* Must have last response to retransmit. */
	pj_assert(tsx->last_tx != NULL);

	/* Retransmit the last response. */
	if (pjsip_tsx_retransmit( tsx, 1 ) != 0) {
	    return -1;
	}

    } else if (event->type == PJSIP_EVENT_TIMER && 
	       event->src.timer == &tsx->timeout_timer) {

	/* Timeout timer. should not happen? */
	pj_assert(0);

	tsx->status_code = PJSIP_SC_TSX_TIMEOUT;

	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, event);

	return -1;

    } else {
	pj_assert(0);
    }

    return 0;
}

/*
 * Handler for events in Proceeding for UAC
 * This state happens after provisional response(s) has been received from
 * UAS.
 */
static int pjsip_tsx_on_state_proceeding_uac( pjsip_transaction *tsx, pjsip_event *event)
{

    pj_assert(tsx->state == PJSIP_TSX_STATE_PROCEEDING || 
	      tsx->state == PJSIP_TSX_STATE_CALLING);

    if (event->type != PJSIP_EVENT_TIMER) {
	/* Must be incoming response, because we should not retransmit
	 * request once response has been received.
	 */
	pj_assert(event->type == PJSIP_EVENT_RX_MSG);
	if (event->type != PJSIP_EVENT_RX_MSG) {
	    return 0;
	}

	tsx->status_code = event->src.rdata->msg->line.status.code;
    } else {
	tsx->status_code = PJSIP_SC_TSX_TIMEOUT;
    }

    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 100)) {

	/* Inform the message to TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_PROCEEDING, event);

    } else if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code,200)) {

	/* Stop timeout timer B/F. */
	pjsip_endpt_cancel_timer( tsx->endpt, &tsx->timeout_timer );

	/* For INVITE, the state moves to Terminated state (because ACK is
	 * handled in TU). For non-INVITE, state moves to Completed.
	 */
	if (tsx->method.id == PJSIP_INVITE_METHOD && tsx->handle_ack == 0) {
	    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, event);
	    return -1;

	} else {
	    pj_time_val timeout;

	    /* For unreliable transport, start timer D (for INVITE) or 
	     * timer K for non-INVITE. */
	    if (PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport) == 0) {
		if (tsx->method.id == PJSIP_INVITE_METHOD) {
		    timeout = td_timer_val;
		} else {
		    timeout = t4_timer_val;
		}
	    } else {
		timeout.sec = timeout.msec = 0;
	    }
	    pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, &timeout);

	    /* Move state to Completed, inform TU. */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, event);
	}

    } else if (tsx->status_code >= 300 && tsx->status_code <= 699) {
	pj_time_val timeout;
	pjsip_tx_data *ack_tdata = NULL;

	/* Stop timer B. */
	pjsip_endpt_cancel_timer( tsx->endpt, &tsx->timeout_timer );

	/* Generate ACK now (for INVITE) but send it later because
	 * dialog need to use last_tx.
	 */
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    const pjsip_hdr *hdr;

	    ack_tdata = pjsip_endpt_create_tdata( tsx->endpt );

	    /* Create msg */
	    ack_tdata->msg = pjsip_msg_create( ack_tdata->pool, PJSIP_REQUEST_MSG);

	    /* Init request line. */
	    pjsip_method_set( &ack_tdata->msg->line.req.method, PJSIP_ACK_METHOD );
	    ack_tdata->msg->line.req.uri = pjsip_uri_clone( ack_tdata->pool, tsx->last_tx->msg->line.req.uri);

	    /* Duplicate headers. */
	    hdr = tsx->last_tx->msg->hdr.next;
	    while (hdr != &tsx->last_tx->msg->hdr) {
		pjsip_hdr *new_hdr;
		new_hdr = pjsip_hdr_clone(ack_tdata->pool, hdr);
		pjsip_msg_add_hdr( ack_tdata->msg, (pjsip_hdr*)new_hdr);
		hdr = hdr->next;
	    }

	    pjsip_endpt_create_ack( tsx->endpt, ack_tdata, event->src.rdata );
	}

	/* Inform TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, event);

	/* Generate and send ACK for INVITE. */
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    if (tsx_send_msg( tsx, ack_tdata) != 0) {
		return -1;
	    }
	}

	/* Start Timer D with TD/T4 timer if unreliable transport is used. */
	if (PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport) == 0) {
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {
		timeout = td_timer_val;
	    } else {
		timeout = t4_timer_val;
	    }
	} else {
	    timeout.sec = timeout.msec = 0;
	}
	pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, &timeout);

    } else {
	// Shouldn't happen because there's no timer for this state.
	pj_assert(0);
    }

    return PJ_OK;
}

/*
 * Handler for events in Completed state for UAS
 */
static int pjsip_tsx_on_state_completed_uas( pjsip_transaction *tsx, pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_COMPLETED);

    if (event->type == PJSIP_EVENT_RX_MSG) {
	pjsip_msg *msg = event->src.rdata->msg;
	pjsip_cseq_hdr *cseq = pjsip_msg_find_hdr( msg, PJSIP_H_CSEQ, NULL );

	/* On receive request retransmission, retransmit last response. */
	if (cseq->method.id != PJSIP_ACK_METHOD) {
	    if (pjsip_tsx_retransmit( tsx, 0 ) != 0) {
		return -1;
	    }

	} else {
	    /* Process incoming ACK request. */

	    /* Cease retransmission. */
	    pjsip_endpt_cancel_timer( tsx->endpt, &tsx->retransmit_timer );

	    /* Start timer I in T4 interval (transaction termination). */
	    pjsip_endpt_cancel_timer( tsx->endpt, &tsx->timeout_timer );
	    pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, &t4_timer_val);

	    /* Move state to "Confirmed" */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_CONFIRMED, event);
	}	

    } else if (event->type == PJSIP_EVENT_TIMER) {

	if (event->src.timer == &tsx->retransmit_timer) {
	    /* Retransmit message. */
	    if (pjsip_tsx_retransmit( tsx, 1 ) != 0) {
		return -1;
	    }

	} else {
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {

		/* For INVITE, this means that ACK was never received.
		 * Set state to Terminated, and inform TU.
		 */

		tsx->status_code = PJSIP_SC_TSX_TIMEOUT;

		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, event);

		return -1;

	    } else {
		/* Transaction terminated, it can now be deleted. */
		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, event);
		return -1;
	    }
	}

    } else {
	/* Ignore request to transmit. */
	pj_assert(event->src.tdata == tsx->last_tx);
    }

    return PJ_OK;
}

/*
 * Handler for events in Completed state for UAC transaction.
 */
static int pjsip_tsx_on_state_completed_uac( pjsip_transaction *tsx, pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_COMPLETED);

    if (event->type == PJSIP_EVENT_TIMER) {
	/* Must be the timeout timer. */
	pj_assert(event->src.timer == &tsx->timeout_timer);

	/* Move to Terminated state. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, event);

	/* Transaction has been destroyed. */
	return -1;

    } else if (event->type == PJSIP_EVENT_RX_MSG) {
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    /* On received of final response retransmission, retransmit the ACK.
	     * TU doesn't need to be informed.
	     */
	    pjsip_msg *msg = event->src.rdata->msg;
	    pj_assert(msg->type == PJSIP_RESPONSE_MSG);
	    if (msg->type==PJSIP_RESPONSE_MSG &&
		msg->line.status.code >= 200)
	    {
		if (pjsip_tsx_retransmit( tsx, 0 ) != 0) {
		    return -1;
		}
	    } else {
		/* Very late retransmission of privisional response. */
		pj_assert( msg->type == PJSIP_RESPONSE_MSG );
	    }
	} else {
	    /* Just drop the response. */
	}
    } else if (tsx->method.id == PJSIP_INVITE_METHOD &&
	       event->type == PJSIP_EVENT_TX_MSG &&
	       event->src.tdata->msg->line.req.method.id == PJSIP_ACK_METHOD) {

	/* Set last transmitted message. */
	if (tsx->last_tx != event->src.tdata) {
	    pjsip_tx_data_dec_ref( tsx->last_tx );
	    tsx->last_tx = event->src.tdata;
	    pjsip_tx_data_add_ref( tsx->last_tx );
	}

	/* No state changed, but notify app. 
	 * Must notify now, so app has chance to put SDP in outgoing ACK msg.
	 */
	tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, event );

	/* Send msg */
	tsx_send_msg(tsx, event->src.tdata);

    } else {
	pj_assert(0);
    }

    return 0;
}

/*
 * Handler for events in state Confirmed.
 */
static int pjsip_tsx_on_state_confirmed( pjsip_transaction *tsx, pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_CONFIRMED);

    /* This state is only for UAS for INVITE. */
    pj_assert(tsx->role == PJSIP_ROLE_UAS);
    pj_assert(tsx->method.id == PJSIP_INVITE_METHOD);

    /* Absorb any ACK received. */
    if (event->type == PJSIP_EVENT_RX_MSG) {
	/* Must be a request message. */
	pj_assert(event->src.rdata->msg->type == PJSIP_REQUEST_MSG);

	/* Must be an ACK request or a late INVITE retransmission. */
	pj_assert(event->src.rdata->msg->line.req.method.id == PJSIP_ACK_METHOD ||
		  event->src.rdata->msg->line.req.method.id == PJSIP_INVITE_METHOD);

    } else if (event->type == PJSIP_EVENT_TIMER) {
	/* Must be from timeout_timer_. */
	pj_assert(event->src.timer == &tsx->timeout_timer);

	/* Move to Terminated state. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, event);

	/* Transaction has been destroyed. */
	return -1;

    } else {
	pj_assert(0);
    }

    return PJ_OK;
}

/*
 * Handler for events in state Terminated.
 */
static int pjsip_tsx_on_state_terminated( pjsip_transaction *tsx, pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_TERMINATED);

    PJ_UNUSED_ARG(event)

    /* Destroy this transaction */
    tsx_set_state(tsx, PJSIP_TSX_STATE_DESTROYED, event);

    return PJ_OK;
}


static int pjsip_tsx_on_state_destroyed( pjsip_transaction *tsx, pjsip_event *event)
{
    PJ_UNUSED_ARG(tsx)
    PJ_UNUSED_ARG(event)
    return PJ_OK;
}


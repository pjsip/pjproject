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
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_config.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/guid.h>
#include <pj/pool.h>
#include <pj/assert.h>

#if 0	// XXX JUNK
    /* Initialize TLS ID for transaction lock. */
    status = pj_thread_local_alloc(&pjsip_tsx_lock_tls_id);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }
    pj_thread_local_set(pjsip_tsx_lock_tls_id, NULL);


    /* Create hash table for transaction. */
    endpt->tsx_table = pj_hash_create( endpt->pool, PJSIP_MAX_TSX_COUNT );
    if (!endpt->tsx_table) {
	status = PJ_ENOMEM;
	goto on_error;
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
	const char *src_addr = rdata->pkt_info.src_name;
	int port = rdata->pkt_info.src_port;
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
	const pj_str_t *addr_addr;
	int port = rdata->msg_info.via->sent_by.port;
	pj_bool_t mismatch = PJ_FALSE;
	if (port == 0) {
	    int type;
	    type = rdata->tp_info.transport->key.type;
	    port = pjsip_transport_get_default_port_for_type(type);
	}
	addr_addr = &rdata->tp_info.transport->local_name.host;
	if (pj_strcmp(&rdata->msg_info.via->sent_by.host, addr_addr) != 0)
	    mismatch = PJ_TRUE;
	else if (port != rdata->tp_info.transport->local_name.port) {
	    /* Port or address mismatch, we should discard response */
	    /* But we saw one implementation (we don't want to name it to 
	     * protect the innocence) which put wrong sent-by port although
	     * the "rport" parameter is correct.
	     * So we discard the response only if the port doesn't match
	     * both the port in sent-by and rport. We try to be lenient here!
	     */
	    if (rdata->msg_info.via->rport_param != rdata->tp_info.transport->local_name.port)
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



#endif	// XXX JUNK

/* Thread Local Storage ID for transaction lock (initialized by endpoint) */
long pjsip_tsx_lock_tls_id;

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
static const pj_time_val t1_timer_val = { PJSIP_T1_TIMEOUT/1000, 
                                          PJSIP_T1_TIMEOUT%1000 };
static const pj_time_val t4_timer_val = { PJSIP_T4_TIMEOUT/1000, 
                                          PJSIP_T4_TIMEOUT%1000 };
static const pj_time_val td_timer_val = { PJSIP_TD_TIMEOUT/1000, 
                                          PJSIP_TD_TIMEOUT%1000 };
static const pj_time_val timeout_timer_val = { (64*PJSIP_T1_TIMEOUT)/1000,
					       (64*PJSIP_T1_TIMEOUT)%1000 };

/* Internal timer IDs */
enum Transaction_Timer_Id
{
    TSX_TIMER_RETRANSMISSION,
    TSX_TIMER_TIMEOUT,
};

/* Function Prototypes */
static pj_status_t pjsip_tsx_on_state_null(     pjsip_transaction *tsx, 
				                pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_calling(  pjsip_transaction *tsx, 
				                pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_trying(   pjsip_transaction *tsx, 
				                pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_proceeding_uas( pjsip_transaction *tsx, 
					        pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_proceeding_uac( pjsip_transaction *tsx,
					        pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_completed_uas( pjsip_transaction *tsx, 
					        pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_completed_uac( pjsip_transaction *tsx,
					        pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_confirmed(pjsip_transaction *tsx, 
					        pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_terminated(pjsip_transaction *tsx, 
					        pjsip_event *event);
static pj_status_t pjsip_tsx_on_state_destroyed(pjsip_transaction *tsx, 
					        pjsip_event *event);

static void         tsx_timer_callback( pj_timer_heap_t *theap, 
			                pj_timer_entry *entry);
static int          tsx_send_msg( pjsip_transaction *tsx, 
                                  pjsip_tx_data *tdata);
static void         lock_tsx( pjsip_transaction *tsx, struct 
                               tsx_lock_data *lck );
static pj_status_t  unlock_tsx( pjsip_transaction *tsx, 
                               struct tsx_lock_data *lck );

/* State handlers for UAC, indexed by state */
static int  (*tsx_state_handler_uac[PJSIP_TSX_STATE_MAX])(pjsip_transaction *,
							  pjsip_event *) = 
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
static int  (*tsx_state_handler_uas[PJSIP_TSX_STATE_MAX])(pjsip_transaction *, 
							  pjsip_event *) = 
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
static pj_status_t create_tsx_key_2543( pj_pool_t *pool,
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

    PJ_ASSERT_RETURN(pool && str && method && rdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(rdata->msg_info.msg, PJ_EINVAL);
    PJ_ASSERT_RETURN(rdata->msg_info.via, PJSIP_EMISSINGHDR);
    PJ_ASSERT_RETURN(rdata->msg_info.cseq, PJSIP_EMISSINGHDR);
    PJ_ASSERT_RETURN(rdata->msg_info.from, PJSIP_EMISSINGHDR);

    host = &rdata->msg_info.via->sent_by.host;
    req_uri = (pjsip_uri*)rdata->msg_info.msg->line.req.uri;

    /* Calculate length required. */
    len_required = 9 +			    /* CSeq number */
		   rdata->msg_info.from->tag.slen +   /* From tag. */
		   rdata->msg_info.call_id.slen +    /* Call-ID */
		   host->slen +		    /* Via host. */
		   9 +			    /* Via port. */
		   16;			    /* Separator+Allowance. */
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
    len = pj_utoa(rdata->msg_info.cseq->cseq, p);
    p += len;
    *p++ = SEPARATOR;

    /* Add From tag. */
    len = rdata->msg_info.from->tag.slen;
    pj_memcpy( p, rdata->msg_info.from->tag.ptr, len);
    p += len;
    *p++ = SEPARATOR;

    /* Add Call-ID. */
    len = rdata->msg_info.call_id.slen;
    pj_memcpy( p, rdata->msg_info.call_id.ptr, len );
    p += len;
    *p++ = SEPARATOR;

    /* Add top Via header. 
     * We don't really care whether the port contains the real port (because
     * it can be omited if default port is used). Anyway this function is 
     * only used to match request retransmission, and we expect that the 
     * request retransmissions will contain the same port.
     */
    pj_memcpy(p, host->ptr, host->slen);
    p += host->slen;
    *p++ = ':';

    len = pj_utoa(rdata->msg_info.via->sent_by.port, p);
    p += len;
    *p++ = SEPARATOR;
    
    *p++ = '\0';

    /* Done. */
    str->ptr = key;
    str->slen = p-key;

    return PJ_SUCCESS;
}

/*
 * Create transaction key for RFC3161 compliant system.
 */
static pj_status_t create_tsx_key_3261( pj_pool_t *pool,
		                        pj_str_t *key,
		                        pjsip_role_e role,
		                        const pjsip_method *method,
		                        const pj_str_t *branch)
{
    char *p;

    PJ_ASSERT_RETURN(pool && key && method && branch, PJ_EINVAL);

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

    return PJ_SUCCESS;
}

/*
 * Create key from the incoming data, to be used to search the transaction
 * in the transaction hash table.
 */
PJ_DEF(pj_status_t) pjsip_tsx_create_key( pj_pool_t *pool, pj_str_t *key, 
				          pjsip_role_e role, 
				          const pjsip_method *method, 
				          const pjsip_rx_data *rdata)
{
    pj_str_t rfc3261_branch = {PJSIP_RFC3261_BRANCH_ID, 
                               PJSIP_RFC3261_BRANCH_LEN};


    /* Get the branch parameter in the top-most Via.
     * If branch parameter is started with "z9hG4bK", then the message was
     * generated by agent compliant with RFC3261. Otherwise, it will be
     * handled as RFC2543.
     */
    const pj_str_t *branch = &rdata->msg_info.via->branch_param;

    if (pj_strncmp(branch,&rfc3261_branch,PJSIP_RFC3261_BRANCH_LEN)==0) {

	/* Create transaction key. */
	return create_tsx_key_3261(pool, key, role, method, branch);

    } else {
	/* Create the key for the message. This key will be matched up 
         * with the transaction key. For RFC2563 transactions, the 
         * transaction key was created by the same function, so it will 
         * match the message.
	 */
	return create_tsx_key_2543( pool, key, role, method, rdata );
    }
}


/*
 * Create new transaction.
 */
PJ_DEF(pj_status_t) pjsip_tsx_create( pj_pool_t *pool,
				      pjsip_endpoint *endpt,
				      pjsip_transaction **p_tsx)
{
    pjsip_transaction *tsx;
    pj_status_t status;

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
    pj_sprintf(tsx->obj_name, "tsx%p", tsx);
    status = pj_mutex_create_recursive(pool, "mtsx%p", &tsx->mutex);
    if (status != PJ_SUCCESS) {
	return status;
    }

    *p_tsx = tsx;
    return PJ_SUCCESS;
}

/*
 * Lock transaction and set the value of Thread Local Storage.
 */
static void lock_tsx(pjsip_transaction *tsx, struct tsx_lock_data *lck)
{
    struct tsx_lock_data *prev_data;

    pj_mutex_lock(tsx->mutex);
    prev_data = (struct tsx_lock_data *) 
                    pj_thread_local_get(pjsip_tsx_lock_tls_id);
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
static pj_status_t unlock_tsx( pjsip_transaction *tsx, 
                               struct tsx_lock_data *lck)
{
    pj_assert( (void*)pj_thread_local_get(pjsip_tsx_lock_tls_id) == lck);
    pj_assert( lck->tsx == tsx );
    pj_thread_local_set(pjsip_tsx_lock_tls_id, lck->prev);
    if (lck->is_alive)
	pj_mutex_unlock(tsx->mutex);

    return lck->is_alive ? PJ_SUCCESS : PJSIP_ETSXDESTROYED;
}

/*
 * Set transaction state, and inform TU about the transaction state change.
 */
static void tsx_set_state( pjsip_transaction *tsx,
			   pjsip_tsx_state_e state,
			   pjsip_event_id_e event_src_type,
                           void *event_src )
{
    pjsip_event e;

    PJ_LOG(4, (tsx->obj_name, "STATE %s-->%s, cause = %s",
	       state_str[tsx->state], state_str[state], 
               pjsip_event_str(event_src_type)));

    /* Change state. */
    tsx->state = state;

    /* Update the state handlers. */
    if (tsx->role == PJSIP_ROLE_UAC) {
	tsx->state_handler = tsx_state_handler_uac[state];
    } else {
	tsx->state_handler = tsx_state_handler_uas[state];
    }

    /* Inform TU */
    PJSIP_EVENT_INIT_TSX_STATE(e, tsx, event_src_type, event_src);
    pjsip_endpt_send_tsx_event( tsx->endpt, &e  );

    /* When the transaction is terminated, release transport, and free the
     * saved last transmitted message.
     */
    if (state == PJSIP_TSX_STATE_TERMINATED) {

	/* Decrement transport reference counter. */
	if (tsx->transport && 
            tsx->transport_state == PJSIP_TSX_TRANSPORT_STATE_FINAL) 
        {
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
				      pjsip_host_info *send_addr )
{
    pjsip_route_hdr *route_hdr;
    
    pj_assert(tdata->msg->type == PJSIP_REQUEST_MSG);

    /* Get the first "Route" header from the message. If the message doesn't
     * have any "Route" headers but the endpoint has, then copy the "Route"
     * headers from the endpoint first.
     */
    route_hdr = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, NULL);
    if (!route_hdr) {
	const pjsip_route_hdr *hdr_list;
	const pjsip_route_hdr *hdr;
	hdr_list = (const pjsip_route_hdr*)pjsip_endpt_get_routing(tsx->endpt);
	hdr = hdr_list->next;
	while (hdr != hdr_list {
	    route_hdr = pjsip_hdr_shallow_clone(tdata->pool, hdr);
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)route_hdr);
	    hdr = hdr->next;
	}
    }

    return pjsip_get_request_addr(tdata, send_addr);  
}


/*
 * Callback from the transport job.
 * This callback is called when asychronous transport connect() operation
 * has completed, with or without error.
 */
static void tsx_transport_callback(pjsip_transport *tr, 
				   void *token, 
				   pj_status_t status)
{
    char addr[PJ_MAX_HOSTNAME];
    pjsip_transaction *tsx = token;
    struct tsx_lock_data lck;

    pj_memcpy(addr, tsx->dest_name.addr.host.ptr, tsx->dest_name.addr.host.slen);
    addr[tsx->dest_name.addr.host.slen] = '\0';


    if (status == PJ_SUCCESS) {
	PJ_LOG(4, (tsx->obj_name, "%s connected to %s:%d",
				  tr->type_name,
				  addr, tsx->dest_name.addr.port));
    } else {
	PJ_LOG(4, (tsx->obj_name, "%s unable to connect to %s:%d, status=%d", 
				  tr->type_name,
				  addr, tsx->dest_name.addr.port, status));
    }

    /* Lock transaction. */
    lock_tsx(tsx, &lck);

    if (status != PJ_SUCCESS) {
	tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;
	tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;

	tsx_set_state(tsx, PJSIP_TSX_STATE_TERMINATED,
                      PJSIP_EVENT_TRANSPORT_ERROR, (void*)status);

	/* Unlock transaction. */
	unlock_tsx(tsx, &lck);
	return;
    }

    /* See if transaction has already been terminated. 
     * If so, schedule to destroy the transaction.
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
    pjsip_transport *tp;

    PJ_LOG(4, (tsx->obj_name, "resolver job complete, status=%d", status));

    if (status != PJ_SUCCESS || addr->count == 0) {
	lock_tsx(tsx, &lck);
	tsx->status_code = PJSIP_SC_TSX_RESOLVE_ERROR;
	tsx_set_state(tsx, PJSIP_TSX_STATE_TERMINATED, 
                      PJSIP_EVENT_TRANSPORT_ERROR, (void*)status);
	unlock_tsx(tsx, &lck);
	return;
    }

    /* Lock transaction. */
    lock_tsx(tsx, &lck);

    /* Copy server addresses. */
    pj_memcpy(&tsx->remote_addr, addr, sizeof(*addr));

    /* Create/find the transport for the remote address. */
    tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_CONNECTING;
    status = pjsip_endpt_alloc_transport( tsx->endpt, addr->entry[0].type,
					  &addr->entry[0].addr,
					  addr->entry[0].addr_len,
					  &tp);
    tsx_transport_callback(tp, tsx, status);

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
    pj_status_t status;
    struct tsx_lock_data lck;

    PJ_LOG(4,(tsx->obj_name, "initializing tsx as UAC (tdata=%p)", tdata));

    /* Lock transaction. */
    lock_tsx(tsx, &lck);

    /* Keep shortcut */
    msg = tdata->msg;

    /* Role is UAC. */
    tsx->role = PJSIP_ROLE_UAC;

    /* Save method. */
    pjsip_method_copy( tsx->pool, &tsx->method, &msg->line.req.method);

    /* Generate Via header if it doesn't exist. */
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

        /* Save branch parameter. */
        tsx->branch = via->branch_param;
    } else {
        /* Copy branch parameter. */
        pj_strdup(tsx->pool, &tsx->branch, &via->branch_param);
    }


    /* Generate transaction key. */
    status = create_tsx_key_3261( tsx->pool, &tsx->transaction_key,
			          PJSIP_ROLE_UAC, &tsx->method, 
			          &via->branch_param);
    if (status != PJ_SUCCESS) {
        unlock_tsx(tsx, &lck);
        return status;
    }

    PJ_LOG(6, (tsx->obj_name, "tsx_key=%.*s", tsx->transaction_key.slen,
	       tsx->transaction_key.ptr));

    /* Save CSeq. */
    cseq = pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    if (!cseq) {
	pj_assert(!"CSeq header not present in outgoing message!");
        unlock_tsx(tsx, &lck);
	return PJSIP_EMISSINGHDR;
    }
    tsx->cseq = cseq->cseq;


    /* Begin with State_Null.
     * Manually set-up the state becase we don't want to call the callback.
     */
    tsx->state = PJSIP_TSX_STATE_NULL;
    tsx->state_handler = &pjsip_tsx_on_state_null;

    /* Get destination name from the message. */
    status = tsx_process_route(tsx, tdata, &tsx->dest_name);
    if (status != PJ_SUCCESS) {
	tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;
	tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;
	tsx_set_state(tsx, PJSIP_TSX_STATE_TERMINATED, 
                      PJSIP_EVENT_TRANSPORT_ERROR, (void*)status);
	unlock_tsx(tsx, &lck);
	return status;
    }

    /* Resolve destination.
     * This will start asynchronous resolver job, and when it finishes, 
     * the callback will be called.
     */
    PJ_LOG(5,(tsx->obj_name, "tsx resolving destination %.*s:%d",
			     tsx->dest_name.addr.host.slen, 
			     tsx->dest_name.addr.host.ptr,
			     tsx->dest_name.addr.port));

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
    pjsip_msg *msg = rdata->msg_info.msg;
    pj_str_t *branch;
    pjsip_cseq_hdr *cseq;
    pj_status_t status;
    struct tsx_lock_data lck;

    PJ_LOG(4,(tsx->obj_name, "initializing tsx as UAS (rdata=%p)", rdata));

    /* Lock transaction. */
    lock_tsx(tsx, &lck);

    /* Keep shortcut to message */
    msg = rdata->msg_info.msg;

    /* Role is UAS */
    tsx->role = PJSIP_ROLE_UAS;

    /* Save method. */
    pjsip_method_copy( tsx->pool, &tsx->method, &msg->line.req.method);

    /* Get transaction key either from branch for RFC3261 message, or
     * create transaction key.
     */
    status = pjsip_tsx_create_key(tsx->pool, &tsx->transaction_key, 
                                  PJSIP_ROLE_UAS, &tsx->method, rdata);
    if (status != PJ_SUCCESS) {
        unlock_tsx(tsx, &lck);
        return status;
    }

    /* Duplicate branch parameter for transaction. */
    branch = &rdata->msg_info.via->branch_param;
    pj_strdup(tsx->pool, &tsx->branch, branch);

    PJ_LOG(6, (tsx->obj_name, "tsx_key=%.*s", tsx->transaction_key.slen,
	       tsx->transaction_key.ptr));

    /* Save CSeq */
    cseq = rdata->msg_info.cseq;
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
    if (PJSIP_TRANSPORT_IS_RELIABLE(rdata->tp_info.transport) || 
	rdata->msg_info.via->rport_param >= 0) 
    {
	tsx->transport = rdata->tp_info.transport;
	pjsip_transport_add_ref(tsx->transport);
	tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;

	tsx->current_addr = 0;
	tsx->remote_addr.count = 1;
	tsx->remote_addr.entry[0].type = tsx->transport->key.type;
	pj_memcpy(&tsx->remote_addr.entry[0].addr, 
		  &rdata->pkt_info.src_addr, rdata->pkt_info.src_addr_len);
	
    } else {
	pj_status_t status;

	status = pjsip_get_response_addr(tsx->pool, rdata->tp_info.transport,
					 rdata->msg_info.via, &tsx->dest_name);
	if (status != PJ_SUCCESS) {
	    tsx->transport_state = PJSIP_TSX_TRANSPORT_STATE_FINAL;
	    tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;
	    tsx_set_state(tsx, PJSIP_TSX_STATE_TERMINATED, 
                          PJSIP_EVENT_TRANSPORT_ERROR, (void*)status);
	    unlock_tsx(tsx, &lck);
	    return status;
	}

	/* Resolve destination.
	 * This will start asynchronous resolver job, and when it finishes, 
	 * the callback will be called.
	 */
	PJ_LOG(5,(tsx->obj_name, "tsx resolving destination %.*s:%d",
				 tsx->dest_name.addr.host.slen, 
				 tsx->dest_name.addr.host.ptr,
				 tsx->dest_name.addr.port));

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
	     (entry->id==TSX_TIMER_RETRANSMISSION ? "Retransmit" : "Timeout")));


    if (entry->id == TSX_TIMER_RETRANSMISSION) {
        PJSIP_EVENT_INIT_TIMER(event, &tsx->retransmit_timer);
    } else {
        PJSIP_EVENT_INIT_TIMER(event, &tsx->timeout_timer);
    }

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
    pjsip_host_info dest_addr;
    pjsip_via_hdr *via;
    struct tsx_lock_data lck;
    pj_status_t status = PJ_SUCCESS;

    /* Lock tsx. */
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
    status = tsx_process_route(tsx, tdata, &dest_addr);
    if (status != 0){
	goto on_error;
    }

    /* Compare message's destination name with transaction's destination name.
     * If NOT equal, then we'll have to resolve the destination.
     */
    if (dest_addr.type == tsx->dest_name.type &&
	dest_addr.flag == tsx->dest_name.flag &&
	dest_addr.addr.port == tsx->dest_name.addr.port &&
	pj_stricmp(&dest_addr.addr.host, &tsx->dest_name.addr.host) == 0)
    {
	/* Equal destination. We can use current transport. */
	pjsip_tsx_on_tx_msg(tsx, tdata);
	unlock_tsx(tsx, &lck);
	return;

    }

    /* New destination; we'll have to resolve host and create new transport. */
    pj_memcpy(&tsx->dest_name, &dest_addr, sizeof(dest_addr));
    pj_strdup(tsx->pool, &tsx->dest_name.addr.host, &dest_addr.addr.host);

    PJ_LOG(5,(tsx->obj_name, "tsx resolving destination %.*s:%d",
			     tsx->dest_name.addr.host.slen, 
			     tsx->dest_name.addr.host.ptr,
			     tsx->dest_name.addr.port));

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

    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                   PJSIP_EVENT_TRANSPORT_ERROR, (void*)status);

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
    pj_status_t status;

    PJ_LOG(5,(tsx->obj_name, "Request to transmit msg on state %s (tdata=%p)",
                             state_str[tsx->state], tdata));

    PJSIP_EVENT_INIT_TX_MSG(event, tsx, tdata);

    /* Dispatch to transaction. */
    lock_tsx(tsx, &lck);
    status = (*tsx->state_handler)(tsx, &event);
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
    pj_status_t status;

    PJ_LOG(5,(tsx->obj_name, "Incoming msg on state %s (rdata=%p)", 
	      state_str[tsx->state], rdata));

    PJSIP_EVENT_INIT_RX_MSG(event, tsx, rdata);

    /* Dispatch to transaction. */
    lock_tsx(tsx, &lck);
    status = (*tsx->state_handler)(tsx, &event);
    unlock_tsx(tsx, &lck);
}

/*
 * Forcely terminate transaction.
 */
PJ_DEF(void) pjsip_tsx_terminate( pjsip_transaction *tsx, int code )
{
    struct tsx_lock_data lck;

    lock_tsx(tsx, &lck);
    tsx->status_code = code;
    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                   PJSIP_EVENT_USER, NULL);

    unlock_tsx(tsx, &lck);
}

/*
 * Transport send completion callback.
 */
static void tsx_on_send_complete(void *token, pjsip_tx_data *tdata,
				 pj_ssize_t bytes_sent)
{
    PJ_UNUSED_ARG(token);
    PJ_UNUSED_ARG(tdata);

    if (bytes_sent <= 0) {
	PJ_TODO(HANDLE_TRANSPORT_ERROR);
    }
}

/*
 * Send message to the transport.
 * If transport is not yet available, then do nothing. The message will be
 * transmitted when transport connection completion callback is called.
 */
static pj_status_t tsx_send_msg( pjsip_transaction *tsx, 
                                 pjsip_tx_data *tdata)
{
    pj_status_t status = PJ_SUCCESS;

    PJ_LOG(5,(tsx->obj_name, "sending msg (tdata=%p)", tdata));

    if (tsx->transport_state == PJSIP_TSX_TRANSPORT_STATE_FINAL) {
	pjsip_event before_tx_event;

	pj_assert(tsx->transport != NULL);

	/* Make sure Via transport info is filled up properly for
	 * requests. 
	 */
	if (tdata->msg->type == PJSIP_REQUEST_MSG) {
	    pjsip_via_hdr *via = (pjsip_via_hdr*) 
		pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);

	    /* For request message, set "rport" parameter by default. */
	    if (tdata->msg->type == PJSIP_REQUEST_MSG)
		via->rport_param = 0;

	    /* Don't update Via sent-by on retransmission. */
	    if (via->sent_by.host.slen == 0) {
		pj_strdup2(tdata->pool, &via->transport, 
			   tsx->transport->type_name);
		pj_strdup(tdata->pool, &via->sent_by.host, 
			  &tsx->transport->local_name.host);
		via->sent_by.port = tsx->transport->local_name.port;
	    }
	}

	/* Notify everybody we're about to send message. */
        PJSIP_EVENT_INIT_PRE_TX_MSG(before_tx_event, tsx, tdata, 
                                    tsx->retransmit_count);
	pjsip_endpt_send_tsx_event( tsx->endpt, &before_tx_event );

	tsx->has_unsent_msg = 0;
	status = pjsip_transport_send(tsx->transport, tdata,
			&tsx->remote_addr.entry[tsx->current_addr].addr,
			tsx->remote_addr.entry[tsx->current_addr].addr_len,
			tsx, &tsx_on_send_complete);
	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    PJ_TODO(HANDLE_TRANSPORT_ERROR);
	    goto on_error;
	}
    } else {
	tsx->has_unsent_msg = 1;
    }

    return 0;

on_error:
    tsx->status_code = PJSIP_SC_TSX_TRANSPORT_ERROR;
    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                   PJSIP_EVENT_TRANSPORT_ERROR, (void*)status);
    return status;
}

/*
 * Retransmit last message sent.
 */
static pj_status_t pjsip_tsx_retransmit( pjsip_transaction *tsx,
					 int should_restart_timer)
{
    pj_status_t status;

    PJ_LOG(4,(tsx->obj_name, "retransmiting (tdata=%p, count=%d, restart?=%d)", 
	      tsx->last_tx, tsx->retransmit_count, should_restart_timer));

    pj_assert(tsx->last_tx != NULL);

    ++tsx->retransmit_count;

    status = tsx_send_msg( tsx, tsx->last_tx);
    if (status != PJ_SUCCESS) {
	return status;
    }
    
    /* Restart timer T1. */
    if (should_restart_timer) {
	pj_time_val timeout;
	int msec_time = (1 << (tsx->retransmit_count)) * PJSIP_T1_TIMEOUT;

	if (tsx->method.id!=PJSIP_INVITE_METHOD && msec_time>PJSIP_T2_TIMEOUT) 
	    msec_time = PJSIP_T2_TIMEOUT;

	timeout.sec = msec_time / 1000;
	timeout.msec = msec_time % 1000;
	pjsip_endpt_schedule_timer( tsx->endpt, &tsx->retransmit_timer, 
				    &timeout);
    }

    return PJ_SUCCESS;
}

/*
 * Handler for events in state Null.
 */
static pj_status_t pjsip_tsx_on_state_null( pjsip_transaction *tsx, 
                                            pjsip_event *event )
{
    pj_status_t status;

    pj_assert( tsx->state == PJSIP_TSX_STATE_NULL);
    pj_assert( tsx->last_tx == NULL );
    pj_assert( tsx->has_unsent_msg == 0);

    if (tsx->role == PJSIP_ROLE_UAS) {

	/* Set state to Trying. */
	pj_assert(event->type == PJSIP_EVENT_RX_MSG);
	tsx_set_state( tsx, PJSIP_TSX_STATE_TRYING, 
                       PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata );

    } else {
	pjsip_tx_data *tdata = event->body.tx_msg.tdata;

	/* Save the message for retransmission. */
	tsx->last_tx = tdata;
	pjsip_tx_data_add_ref(tdata);

	/* Send the message. */
        status = tsx_send_msg( tsx, tdata);
	if (status != PJ_SUCCESS) {
	    return status;
	}

	/* Start Timer B (or called timer F for non-INVITE) for transaction 
	 * timeout.
	 */
	pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, 
                                    &timeout_timer_val);

	/* Start Timer A (or timer E) for retransmission only if unreliable 
	 * transport is being used.
	 */
	if (tsx->transport_state == PJSIP_TSX_TRANSPORT_STATE_FINAL &&
	    PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport)==0) 
	{
	    pjsip_endpt_schedule_timer(tsx->endpt, &tsx->retransmit_timer, 
                                       &t1_timer_val);
	    tsx->retransmit_count = 0;
	}

	/* Move state. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_CALLING, 
                       PJSIP_EVENT_TX_MSG, tdata);
    }

    return PJ_SUCCESS;
}

/*
 * State Calling is for UAC after it sends request but before any responses
 * is received.
 */
static pj_status_t pjsip_tsx_on_state_calling( pjsip_transaction *tsx, 
				               pjsip_event *event )
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_CALLING);
    pj_assert(tsx->role == PJSIP_ROLE_UAC);

    if (event->type == PJSIP_EVENT_TIMER && 
	event->body.timer.entry == &tsx->retransmit_timer) 
    {
        pj_status_t status;

	/* Retransmit the request. */
        status = pjsip_tsx_retransmit( tsx, 1 );
	if (status != PJ_SUCCESS) {
	    return status;
	}

    } else if (event->type == PJSIP_EVENT_TIMER && 
	       event->body.timer.entry == &tsx->timeout_timer) 
    {

	/* Cancel retransmission timer. */
	if (PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport)==0) {
	    pjsip_endpt_cancel_timer(tsx->endpt, &tsx->retransmit_timer);
	}

	/* Set status code */
	tsx->status_code = PJSIP_SC_TSX_TIMEOUT;

	/* Inform TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                       PJSIP_EVENT_TIMER, &tsx->timeout_timer);

	/* Transaction is destroyed */
	return PJSIP_ETSXDESTROYED;

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
	code = event->body.rx_msg.rdata->msg_info.msg->line.status.code;
	if (tsx->method.id != PJSIP_INVITE_METHOD && code!=401 && code!=407) {
	    pjsip_tx_data_dec_ref(tsx->last_tx);
	    tsx->last_tx = NULL;
	}

	/* Processing is similar to state Proceeding. */
	pjsip_tsx_on_state_proceeding_uac( tsx, event);

    } else {
	pj_assert(0);
        return PJ_EBUG;
    }

    return PJ_SUCCESS;
}

/*
 * State Trying is for UAS after it received request but before any responses
 * is sent.
 * Note: this is different than RFC3261, which can use Trying state for
 *	 non-INVITE client transaction (bug in RFC?).
 */
static pj_status_t pjsip_tsx_on_state_trying( pjsip_transaction *tsx, 
                                              pjsip_event *event)
{
    pj_status_t status;

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
	return PJ_SUCCESS;
    }

    /* The rest of the processing of the event is exactly the same as in
     * "Proceeding" state.
     */
    status = pjsip_tsx_on_state_proceeding_uas( tsx, event);

    /* Inform the TU of the state transision if state is still State_Trying */
    if (status==PJ_SUCCESS && tsx->state == PJSIP_TSX_STATE_TRYING) {
	tsx_set_state( tsx, PJSIP_TSX_STATE_PROCEEDING, 
                       PJSIP_EVENT_TX_MSG, event->body.tx_msg.tdata);
    }

    return status;
}

/*
 * Handler for events in Proceeding for UAS
 * This state happens after the TU sends provisional response.
 */
static pj_status_t pjsip_tsx_on_state_proceeding_uas( pjsip_transaction *tsx,
                                                      pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_PROCEEDING || 
	      tsx->state == PJSIP_TSX_STATE_TRYING);

    /* This state is only for UAS. */
    pj_assert(tsx->role == PJSIP_ROLE_UAS);

    /* Receive request retransmission. */
    if (event->type == PJSIP_EVENT_RX_MSG) {

        pj_status_t status;

	/* Send last response. */
        status = pjsip_tsx_retransmit( tsx, 0 );
	if (status != PJ_SUCCESS) {
	    return status;
	}
	
    } else if (event->type == PJSIP_EVENT_TX_MSG ) {
	pjsip_tx_data *tdata = event->body.tx_msg.tdata;
        pj_status_t status;

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
        status = tsx_send_msg(tsx, tdata);
	if (status != PJ_SUCCESS) {
	    return status;
	}

	// Update To tag header for RFC2543 transaction.
	// TODO:

	/* Update transaction state */
	if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 100)) {

	    if (tsx->last_tx != tdata) {
		tsx->last_tx = tdata;
		pjsip_tx_data_add_ref( tdata );
	    }
	    tsx_set_state( tsx, PJSIP_TSX_STATE_PROCEEDING, 
                           PJSIP_EVENT_TX_MSG, tdata );

	} else if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 200)) {

	    if (tsx->method.id == PJSIP_INVITE_METHOD && tsx->handle_ack==0) {

		/* 2xx class message is not saved, because retransmission 
                 * is handled by TU.
		 */
		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                               PJSIP_EVENT_TX_MSG, tdata );

		/* Transaction is destroyed. */
		return PJSIP_ETSXDESTROYED;

	    } else {
		pj_time_val timeout;

		if (tsx->method.id == PJSIP_INVITE_METHOD) {
		    tsx->retransmit_count = 0;
		    pjsip_endpt_schedule_timer( tsx->endpt, 
                                                &tsx->retransmit_timer, 
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

		pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, 
                                            &timeout);

		/* Set state to "Completed" */
		tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                               PJSIP_EVENT_TX_MSG, tdata );
	    }

	} else if (tsx->status_code >= 300) {

	    /* 3xx-6xx class message causes transaction to move to 
             * "Completed" state. 
             */
	    if (tsx->last_tx != tdata) {
		tsx->last_tx = tdata;
		pjsip_tx_data_add_ref( tdata );
	    }

	    /* Start timer H for transaction termination */
	    pjsip_endpt_schedule_timer(tsx->endpt,&tsx->timeout_timer,
                                       &timeout_timer_val);

	    /* For INVITE, if unreliable transport is used, retransmission 
	     * timer G will be scheduled (retransmission).
	     */
	    if (PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport)==0) {
		pjsip_cseq_hdr *cseq = pjsip_msg_find_hdr( msg, PJSIP_H_CSEQ,
                                                           NULL);
		if (cseq->method.id == PJSIP_INVITE_METHOD) {
		    tsx->retransmit_count = 0;
		    pjsip_endpt_schedule_timer(tsx->endpt, 
                                               &tsx->retransmit_timer, 
					       &t1_timer_val);
		}
	    }

	    /* Inform TU */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                           PJSIP_EVENT_TX_MSG, tdata );

	} else {
	    pj_assert(0);
	}


    } else if (event->type == PJSIP_EVENT_TIMER && 
	       event->body.timer.entry == &tsx->retransmit_timer) {
	/* Retransmission timer elapsed. */
        pj_status_t status;

	/* Must have last response to retransmit. */
	pj_assert(tsx->last_tx != NULL);

	/* Retransmit the last response. */
        status = pjsip_tsx_retransmit( tsx, 1 );
	if (status != PJ_SUCCESS) {
	    return status;
	}

    } else if (event->type == PJSIP_EVENT_TIMER && 
	       event->body.timer.entry == &tsx->timeout_timer) {

	/* Timeout timer. should not happen? */
	pj_assert(0);

	tsx->status_code = PJSIP_SC_TSX_TIMEOUT;

	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                       PJSIP_EVENT_TIMER, &tsx->timeout_timer);

	return PJ_EBUG;

    } else {
	pj_assert(0);
        return PJ_EBUG;
    }

    return PJ_SUCCESS;
}

/*
 * Handler for events in Proceeding for UAC
 * This state happens after provisional response(s) has been received from
 * UAS.
 */
static pj_status_t pjsip_tsx_on_state_proceeding_uac(pjsip_transaction *tsx, 
                                                     pjsip_event *event)
{

    pj_assert(tsx->state == PJSIP_TSX_STATE_PROCEEDING || 
	      tsx->state == PJSIP_TSX_STATE_CALLING);

    if (event->type != PJSIP_EVENT_TIMER) {
	/* Must be incoming response, because we should not retransmit
	 * request once response has been received.
	 */
	pj_assert(event->type == PJSIP_EVENT_RX_MSG);
	if (event->type != PJSIP_EVENT_RX_MSG) {
	    return PJ_EINVALIDOP;
	}

	tsx->status_code = event->body.rx_msg.rdata->msg_info.msg->line.status.code;
    } else {
	tsx->status_code = PJSIP_SC_TSX_TIMEOUT;
    }

    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 100)) {

	/* Inform the message to TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_PROCEEDING, 
                       PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata );

    } else if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code,200)) {

	/* Stop timeout timer B/F. */
	pjsip_endpt_cancel_timer( tsx->endpt, &tsx->timeout_timer );

	/* For INVITE, the state moves to Terminated state (because ACK is
	 * handled in TU). For non-INVITE, state moves to Completed.
	 */
	if (tsx->method.id == PJSIP_INVITE_METHOD && tsx->handle_ack == 0) {
	    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                           PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata );
	    return PJSIP_ETSXDESTROYED;

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
	    pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, 
					&timeout);

	    /* Move state to Completed, inform TU. */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                           PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata );
	}

    } else if (tsx->status_code >= 300 && tsx->status_code <= 699) {
	pj_time_val timeout;
        pj_status_t status;

	/* Stop timer B. */
	pjsip_endpt_cancel_timer( tsx->endpt, &tsx->timeout_timer );

	/* Generate and send ACK for INVITE. */
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    pjsip_endpt_create_ack( tsx->endpt, tsx->last_tx, 
                                    event->body.rx_msg.rdata );
            status = tsx_send_msg( tsx, tsx->last_tx);
	    if (status != PJ_SUCCESS) {
		return status;
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

	/* Inform TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                       PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata );

    } else {
	// Shouldn't happen because there's no timer for this state.
	pj_assert(0);
        return PJ_EBUG;
    }

    return PJ_SUCCESS;
}

/*
 * Handler for events in Completed state for UAS
 */
static pj_status_t pjsip_tsx_on_state_completed_uas( pjsip_transaction *tsx, 
                                                     pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_COMPLETED);

    if (event->type == PJSIP_EVENT_RX_MSG) {
	pjsip_msg *msg = event->body.rx_msg.rdata->msg_info.msg;
	pjsip_cseq_hdr *cseq = pjsip_msg_find_hdr( msg, PJSIP_H_CSEQ, NULL );

	/* On receive request retransmission, retransmit last response. */
	if (cseq->method.id != PJSIP_ACK_METHOD) {
            pj_status_t status;

            status = pjsip_tsx_retransmit( tsx, 0 );
	    if (status != PJ_SUCCESS) {
		return status;
	    }

	} else {
	    /* Process incoming ACK request. */

	    /* Cease retransmission. */
	    pjsip_endpt_cancel_timer( tsx->endpt, &tsx->retransmit_timer );

	    /* Start timer I in T4 interval (transaction termination). */
	    pjsip_endpt_cancel_timer( tsx->endpt, &tsx->timeout_timer );
	    pjsip_endpt_schedule_timer( tsx->endpt, &tsx->timeout_timer, 
					&t4_timer_val);

	    /* Move state to "Confirmed" */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_CONFIRMED, 
                           PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata );
	}	

    } else if (event->type == PJSIP_EVENT_TIMER) {

	if (event->body.timer.entry == &tsx->retransmit_timer) {
	    /* Retransmit message. */
            pj_status_t status;

            status = pjsip_tsx_retransmit( tsx, 1 );
	    if (status != PJ_SUCCESS) {
		return status;
	    }

	} else {
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {

		/* For INVITE, this means that ACK was never received.
		 * Set state to Terminated, and inform TU.
		 */

		tsx->status_code = PJSIP_SC_TSX_TIMEOUT;

		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                               PJSIP_EVENT_TIMER, &tsx->timeout_timer );

		return PJSIP_ETSXDESTROYED;

	    } else {
		/* Transaction terminated, it can now be deleted. */
		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                               PJSIP_EVENT_TIMER, &tsx->timeout_timer );
		return PJSIP_ETSXDESTROYED;
	    }
	}

    } else {
	/* Ignore request to transmit. */
	pj_assert(event->body.tx_msg.tdata == tsx->last_tx);
    }

    return PJ_SUCCESS;
}

/*
 * Handler for events in Completed state for UAC transaction.
 */
static pj_status_t pjsip_tsx_on_state_completed_uac( pjsip_transaction *tsx,
                                                     pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_COMPLETED);

    if (event->type == PJSIP_EVENT_TIMER) {
	/* Must be the timeout timer. */
	pj_assert(event->body.timer.entry == &tsx->timeout_timer);

	/* Move to Terminated state. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                       PJSIP_EVENT_TIMER, event->body.timer.entry );

	/* Transaction has been destroyed. */
	return PJSIP_ETSXDESTROYED;

    } else if (event->type == PJSIP_EVENT_RX_MSG) {
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    /* On received of final response retransmission, retransmit the ACK.
	     * TU doesn't need to be informed.
	     */
	    pjsip_msg *msg = event->body.rx_msg.rdata->msg_info.msg;
	    pj_assert(msg->type == PJSIP_RESPONSE_MSG);
	    if (msg->type==PJSIP_RESPONSE_MSG &&
		msg->line.status.code >= 200)
	    {
                pj_status_t status;

                status = pjsip_tsx_retransmit( tsx, 0 );
		if (status != PJ_SUCCESS) {
		    return status;
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
	       event->body.tx_msg.tdata->msg->line.req.method.id==PJSIP_ACK_METHOD) {

        pj_status_t status;

	/* Set last transmitted message. */
	if (tsx->last_tx != event->body.tx_msg.tdata) {
	    pjsip_tx_data_dec_ref( tsx->last_tx );
	    tsx->last_tx = event->body.tx_msg.tdata;
	    pjsip_tx_data_add_ref( tsx->last_tx );
	}

	/* No state changed, but notify app. 
	 * Must notify now, so app has chance to put SDP in outgoing ACK msg.
	 */
	tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                       PJSIP_EVENT_TX_MSG, event->body.tx_msg.tdata );

	/* Send msg */
	status = tsx_send_msg(tsx, event->body.tx_msg.tdata);
        if (status != PJ_SUCCESS)
            return status;

    } else {
	pj_assert(0);
        return PJ_EBUG;
    }

    return PJ_SUCCESS;
}

/*
 * Handler for events in state Confirmed.
 */
static pj_status_t pjsip_tsx_on_state_confirmed( pjsip_transaction *tsx,
                                                 pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_CONFIRMED);

    /* This state is only for UAS for INVITE. */
    pj_assert(tsx->role == PJSIP_ROLE_UAS);
    pj_assert(tsx->method.id == PJSIP_INVITE_METHOD);

    /* Absorb any ACK received. */
    if (event->type == PJSIP_EVENT_RX_MSG) {

        pjsip_method_e method_id = 
            event->body.rx_msg.rdata->msg_info.msg->line.req.method.id;

	/* Must be a request message. */
	pj_assert(event->body.rx_msg.rdata->msg_info.msg->type == PJSIP_REQUEST_MSG);

	/* Must be an ACK request or a late INVITE retransmission. */
	pj_assert(method_id == PJSIP_ACK_METHOD ||
		  method_id == PJSIP_INVITE_METHOD);

        /* Just so that compiler won't complain about unused vars when
         * building release code.
         */
        PJ_UNUSED_ARG(method_id);

    } else if (event->type == PJSIP_EVENT_TIMER) {
	/* Must be from timeout_timer_. */
	pj_assert(event->body.timer.entry == &tsx->timeout_timer);

	/* Move to Terminated state. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                       PJSIP_EVENT_TIMER, &tsx->timeout_timer );

	/* Transaction has been destroyed. */
	return PJSIP_ETSXDESTROYED;

    } else {
	pj_assert(0);
        return PJ_EBUG;
    }

    return PJ_SUCCESS;
}

/*
 * Handler for events in state Terminated.
 */
static pj_status_t pjsip_tsx_on_state_terminated( pjsip_transaction *tsx,
                                                  pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_TERMINATED);

    PJ_UNUSED_ARG(event);

    /* Destroy this transaction */
    tsx_set_state(tsx, PJSIP_TSX_STATE_DESTROYED, 
                  event->type, event->body.user.user1 );

    return PJ_SUCCESS;
}


static pj_status_t pjsip_tsx_on_state_destroyed(pjsip_transaction *tsx,
                                                pjsip_event *event)
{
    PJ_UNUSED_ARG(tsx);
    PJ_UNUSED_ARG(event);
    return PJ_SUCCESS;
}


/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjsip/sip_util.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_event.h>
#include <pjlib-util/errno.h>
#include <pj/hash.h>
#include <pj/pool.h>
#include <pj/os.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>

#define THIS_FILE   "sip_transaction.c"

#if 0
#define TSX_TRACE_(expr)    PJ_LOG(3,expr)
#else
#define TSX_TRACE_(expr)
#endif

/* When this macro is set, transaction will keep the hashed value
 * so that future lookup (to unregister transaction) does not need
 * to recalculate the hash again. It should gains a little bit of
 * performance, so generally we'd want this.
 */
#define PRECALC_HASH


/* Defined in sip_util_statefull.c */
extern pjsip_module mod_stateful_util;


/*****************************************************************************
 **
 ** Declarations and static variable definitions section.
 **
 *****************************************************************************
 **/
/* Prototypes. */
static pj_status_t mod_tsx_layer_load(pjsip_endpoint *endpt);
static pj_status_t mod_tsx_layer_start(void);
static pj_status_t mod_tsx_layer_stop(void);
static pj_status_t mod_tsx_layer_unload(void);
static pj_bool_t   mod_tsx_layer_on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t   mod_tsx_layer_on_rx_response(pjsip_rx_data *rdata);

/* Transaction layer module definition. */
static struct mod_tsx_layer
{
    struct pjsip_module  mod;
    pj_pool_t		*pool;
    pjsip_endpoint	*endpt;
    pj_mutex_t		*mutex;
    pj_hash_table_t	*htable;
} mod_tsx_layer = 
{   {
	NULL, NULL,			/* List's prev and next.    */
	{ "mod-tsx-layer", 13 },	/* Module name.		    */
	-1,				/* Module ID		    */
	PJSIP_MOD_PRIORITY_TSX_LAYER,	/* Priority.		    */
	mod_tsx_layer_load,		/* load().		    */
	mod_tsx_layer_start,		/* start()		    */
	mod_tsx_layer_stop,		/* stop()		    */
	mod_tsx_layer_unload,		/* unload()		    */
	mod_tsx_layer_on_rx_request,	/* on_rx_request()	    */
	mod_tsx_layer_on_rx_response,	/* on_rx_response()	    */
	NULL
    }
};

/* Transaction state names */
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
    "UAC",
    "UAS"
};

/* Transport flag. */
enum
{
    TSX_HAS_PENDING_TRANSPORT	= 1,
    TSX_HAS_PENDING_RESCHED	= 2,
    TSX_HAS_PENDING_SEND	= 4,
    TSX_HAS_PENDING_DESTROY	= 8,
    TSX_HAS_RESOLVED_SERVER	= 16,
};

/* Timer timeout value constants */
static pj_time_val t1_timer_val = { PJSIP_T1_TIMEOUT/1000, 
                                    PJSIP_T1_TIMEOUT%1000 };
static pj_time_val t2_timer_val = { PJSIP_T2_TIMEOUT/1000, 
                                    PJSIP_T2_TIMEOUT%1000 };
static pj_time_val t4_timer_val = { PJSIP_T4_TIMEOUT/1000, 
                                    PJSIP_T4_TIMEOUT%1000 };
static pj_time_val td_timer_val = { PJSIP_TD_TIMEOUT/1000, 
                                    PJSIP_TD_TIMEOUT%1000 };
static pj_time_val timeout_timer_val = { (64*PJSIP_T1_TIMEOUT)/1000,
					 (64*PJSIP_T1_TIMEOUT)%1000 };

#define TIMER_INACTIVE		0
#define RETRANSMIT_TIMER	1
#define TIMEOUT_TIMER		2
#define TRANSPORT_ERR_TIMER	3

/* Flags for tsx_set_state() */
enum
{
    NO_NOTIFY = 1,
    NO_SCHEDULE_HANDLER = 2,
};

/* Prototypes. */
static pj_status_t tsx_on_state_null(		pjsip_transaction *tsx, 
				                pjsip_event *event);
static pj_status_t tsx_on_state_calling(	pjsip_transaction *tsx, 
				                pjsip_event *event);
static pj_status_t tsx_on_state_trying(		pjsip_transaction *tsx, 
				                pjsip_event *event);
static pj_status_t tsx_on_state_proceeding_uas( pjsip_transaction *tsx, 
					        pjsip_event *event);
static pj_status_t tsx_on_state_proceeding_uac( pjsip_transaction *tsx,
					        pjsip_event *event);
static pj_status_t tsx_on_state_completed_uas(	pjsip_transaction *tsx, 
					        pjsip_event *event);
static pj_status_t tsx_on_state_completed_uac(	pjsip_transaction *tsx,
					        pjsip_event *event);
static pj_status_t tsx_on_state_confirmed(	pjsip_transaction *tsx, 
					        pjsip_event *event);
static pj_status_t tsx_on_state_terminated(	pjsip_transaction *tsx, 
					        pjsip_event *event);
static pj_status_t tsx_on_state_destroyed(	pjsip_transaction *tsx, 
					        pjsip_event *event);
static void        tsx_timer_callback( pj_timer_heap_t *theap, 
			               pj_timer_entry *entry);
static void	   tsx_tp_state_callback(
				       pjsip_transport *tp,
				       pjsip_transport_state state,
				       const pjsip_transport_state_info *info);
static void 	   tsx_set_state( pjsip_transaction *tsx,
            	                  pjsip_tsx_state_e state,
            	                  pjsip_event_id_e event_src_type,
            	                  void *event_src,
				  int flag);
static void 	   tsx_set_status_code(pjsip_transaction *tsx,
            	                       int code, const pj_str_t *reason);
static pj_status_t tsx_create( pjsip_module *tsx_user,
                               pj_grp_lock_t *grp_lock,
			       pjsip_transaction **p_tsx);
static void	   tsx_on_destroy(void *arg);
static pj_status_t tsx_shutdown( pjsip_transaction *tsx );
static void	   tsx_resched_retransmission( pjsip_transaction *tsx );
static pj_status_t tsx_retransmit( pjsip_transaction *tsx, int resched);
static int         tsx_send_msg( pjsip_transaction *tsx, 
                                 pjsip_tx_data *tdata);
static void        tsx_update_transport( pjsip_transaction *tsx, 
					 pjsip_transport *tp);


/* State handlers for UAC, indexed by state */
static int  (*tsx_state_handler_uac[PJSIP_TSX_STATE_MAX])(pjsip_transaction *,
							  pjsip_event *) = 
{
    &tsx_on_state_null,
    &tsx_on_state_calling,
    NULL,
    &tsx_on_state_proceeding_uac,
    &tsx_on_state_completed_uac,
    &tsx_on_state_confirmed,
    &tsx_on_state_terminated,
    &tsx_on_state_destroyed,
};

/* State handlers for UAS */
static int  (*tsx_state_handler_uas[PJSIP_TSX_STATE_MAX])(pjsip_transaction *, 
							  pjsip_event *) = 
{
    &tsx_on_state_null,
    NULL,
    &tsx_on_state_trying,
    &tsx_on_state_proceeding_uas,
    &tsx_on_state_completed_uas,
    &tsx_on_state_confirmed,
    &tsx_on_state_terminated,
    &tsx_on_state_destroyed,
};

/*****************************************************************************
 **
 ** Utilities
 **
 *****************************************************************************
 */
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
static pj_status_t create_tsx_key_2543( pj_pool_t *pool,
			                pj_str_t *str,
			                pjsip_role_e role,
			                const pjsip_method *method,
			                const pjsip_rx_data *rdata )
{
#define SEPARATOR   '$'
    char *key, *p;
    pj_ssize_t len;
    pj_size_t len_required;
    pj_str_t *host;

    PJ_ASSERT_RETURN(pool && str && method && rdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(rdata->msg_info.msg, PJ_EINVAL);
    PJ_ASSERT_RETURN(rdata->msg_info.via, PJSIP_EMISSINGHDR);
    PJ_ASSERT_RETURN(rdata->msg_info.cseq, PJSIP_EMISSINGHDR);
    PJ_ASSERT_RETURN(rdata->msg_info.from, PJSIP_EMISSINGHDR);

    host = &rdata->msg_info.via->sent_by.host;

    /* Calculate length required. */
    len_required = method->name.slen +      /* Method */
                   11 +			    /* CSeq number */
		   rdata->msg_info.from->tag.slen +   /* From tag. */
		   rdata->msg_info.cid->id.slen +    /* Call-ID */
		   host->slen +		    /* Via host. */
		   11 +			    /* Via port. */
		   16;			    /* Separator+Allowance. */
    key = p = (char*) pj_pool_alloc(pool, len_required);

    /* Add role. */
    *p++ = (char)(role==PJSIP_ROLE_UAC ? 'c' : 's');
    *p++ = SEPARATOR;

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
    len = rdata->msg_info.cid->id.slen;
    pj_memcpy( p, rdata->msg_info.cid->id.ptr, len );
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

    p = key->ptr = (char*) 
    		   pj_pool_alloc(pool, branch->slen + method->name.slen + 4 );
    
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

    if (pj_strnicmp(branch,&rfc3261_branch,PJSIP_RFC3261_BRANCH_LEN)==0) {

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

/*****************************************************************************
 **
 ** Transaction layer module
 **
 *****************************************************************************
 **/
/*
 * Create transaction layer module and registers it to the endpoint.
 */
PJ_DEF(pj_status_t) pjsip_tsx_layer_init_module(pjsip_endpoint *endpt)
{
    pj_pool_t *pool;
    pj_status_t status;


    PJ_ASSERT_RETURN(mod_tsx_layer.endpt==NULL, PJ_EINVALIDOP);

    /* Initialize timer values */
    t1_timer_val.sec  = pjsip_cfg()->tsx.t1 / 1000;
    t1_timer_val.msec = pjsip_cfg()->tsx.t1 % 1000;
    t2_timer_val.sec  = pjsip_cfg()->tsx.t2 / 1000;
    t2_timer_val.msec = pjsip_cfg()->tsx.t2 % 1000;
    t4_timer_val.sec  = pjsip_cfg()->tsx.t4 / 1000;
    t4_timer_val.msec = pjsip_cfg()->tsx.t4 % 1000;
    td_timer_val.sec  = pjsip_cfg()->tsx.td / 1000;
    td_timer_val.msec = pjsip_cfg()->tsx.td % 1000;
    /* Changed the initialization below to use td_timer_val instead, to enable
     * customization to the timeout value.
     */
    //timeout_timer_val.sec  = (64 * pjsip_cfg()->tsx.t1) / 1000;
    //timeout_timer_val.msec = (64 * pjsip_cfg()->tsx.t1) % 1000;
    timeout_timer_val = td_timer_val;

    /*
     * Initialize transaction layer structure.
     */

    /* Create pool for the module. */
    pool = pjsip_endpt_create_pool(endpt, "tsxlayer", 
				   PJSIP_POOL_TSX_LAYER_LEN,
				   PJSIP_POOL_TSX_LAYER_INC );
    if (!pool)
	return PJ_ENOMEM;

    
    /* Initialize some attributes. */
    mod_tsx_layer.pool = pool;
    mod_tsx_layer.endpt = endpt;


    /* Create hash table. */
    mod_tsx_layer.htable = pj_hash_create( pool, pjsip_cfg()->tsx.max_count );
    if (!mod_tsx_layer.htable) {
	pjsip_endpt_release_pool(endpt, pool);
	return PJ_ENOMEM;
    }

    /* Create group lock. */
    status = pj_mutex_create_recursive(pool, "tsxlayer", &mod_tsx_layer.mutex);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_release_pool(endpt, pool);
	return status;
    }

    /*
     * Register transaction layer module to endpoint.
     */
    status = pjsip_endpt_register_module( endpt, &mod_tsx_layer.mod );
    if (status != PJ_SUCCESS) {
	pj_mutex_destroy(mod_tsx_layer.mutex);
	pjsip_endpt_release_pool(endpt, pool);
	return status;
    }

    /* Register mod_stateful_util module (sip_util_statefull.c) */
    status = pjsip_endpt_register_module(endpt, &mod_stateful_util);
    if (status != PJ_SUCCESS) {
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Get the instance of transaction layer module.
 */
PJ_DEF(pjsip_module*) pjsip_tsx_layer_instance(void)
{
    return &mod_tsx_layer.mod;
}


/*
 * Unregister and destroy transaction layer module.
 */
PJ_DEF(pj_status_t) pjsip_tsx_layer_destroy(void)
{
    /* Are we registered? */
    PJ_ASSERT_RETURN(mod_tsx_layer.endpt!=NULL, PJ_EINVALIDOP);

    /* Unregister from endpoint. 
     * Clean-ups will be done in the unload() module callback.
     */
    return pjsip_endpt_unregister_module( mod_tsx_layer.endpt, 
					  &mod_tsx_layer.mod);
}


/*
 * Register the transaction to the hash table.
 */
static pj_status_t mod_tsx_layer_register_tsx( pjsip_transaction *tsx)
{
    pj_assert(tsx->transaction_key.slen != 0);

    /* Lock hash table mutex. */
    pj_mutex_lock(mod_tsx_layer.mutex);

    /* Check if no transaction with the same key exists. 
     * Do not use PJ_ASSERT_RETURN since it evaluates the expression
     * twice!
     */
    if(pj_hash_get_lower(mod_tsx_layer.htable, 
		         tsx->transaction_key.ptr,
		         (unsigned)tsx->transaction_key.slen, 
		         NULL))
    {
	pj_mutex_unlock(mod_tsx_layer.mutex);
	PJ_LOG(2,(THIS_FILE, 
		  "Unable to register %.*s transaction (key exists)",
		  (int)tsx->method.name.slen,
		  tsx->method.name.ptr));
	return PJ_EEXISTS;
    }

    TSX_TRACE_((THIS_FILE, 
		"Transaction %p registered with hkey=0x%p and key=%.*s",
		tsx, tsx->hashed_key, tsx->transaction_key.slen,
		tsx->transaction_key.ptr));

    /* Register the transaction to the hash table. */
#ifdef PRECALC_HASH
    pj_hash_set_lower( tsx->pool, mod_tsx_layer.htable,
                       tsx->transaction_key.ptr,
    		       (unsigned)tsx->transaction_key.slen, 
		       tsx->hashed_key, tsx);
#else
    pj_hash_set_lower( tsx->pool, mod_tsx_layer.htable,
                       tsx->transaction_key.ptr,
    		       tsx->transaction_key.slen, 0, tsx);
#endif

    /* Unlock mutex. */
    pj_mutex_unlock(mod_tsx_layer.mutex);

    return PJ_SUCCESS;
}


/*
 * Unregister the transaction from the hash table.
 */
static void mod_tsx_layer_unregister_tsx( pjsip_transaction *tsx)
{
    if (mod_tsx_layer.mod.id == -1) {
	/* The transaction layer has been unregistered. This could happen
	 * if the transaction was pending on transport and the application
	 * is shutdown. See http://trac.pjsip.org/repos/ticket/1033. In
	 * this case just do nothing.
	 */
	return;
    }

    pj_assert(tsx->transaction_key.slen != 0);
    //pj_assert(tsx->state != PJSIP_TSX_STATE_NULL);

    /* Lock hash table mutex. */
    pj_mutex_lock(mod_tsx_layer.mutex);

    /* Register the transaction to the hash table. */
#ifdef PRECALC_HASH
    pj_hash_set_lower( NULL, mod_tsx_layer.htable, tsx->transaction_key.ptr,
    		       (unsigned)tsx->transaction_key.slen, tsx->hashed_key, 
		       NULL);
#else
    pj_hash_set_lower( NULL, mod_tsx_layer.htable, tsx->transaction_key.ptr,
    		       tsx->transaction_key.slen, 0, NULL);
#endif

    TSX_TRACE_((THIS_FILE, 
		"Transaction %p unregistered, hkey=0x%p and key=%.*s",
		tsx, tsx->hashed_key, tsx->transaction_key.slen,
		tsx->transaction_key.ptr));

    /* Unlock mutex. */
    pj_mutex_unlock(mod_tsx_layer.mutex);
}


/*
 * Retrieve the current number of transactions currently registered in 
 * the hash table.
 */
PJ_DEF(unsigned) pjsip_tsx_layer_get_tsx_count(void)
{
    unsigned count;

    /* Are we registered? */
    PJ_ASSERT_RETURN(mod_tsx_layer.endpt!=NULL, 0);

    pj_mutex_lock(mod_tsx_layer.mutex);
    count = pj_hash_count(mod_tsx_layer.htable);
    pj_mutex_unlock(mod_tsx_layer.mutex);

    return count;
}


/*
 * Find a transaction.
 */
static pjsip_transaction* find_tsx( const pj_str_t *key, pj_bool_t lock,
				    pj_bool_t add_ref )
{
    pjsip_transaction *tsx;
    pj_uint32_t hval = 0;

    pj_mutex_lock(mod_tsx_layer.mutex);
    tsx = (pjsip_transaction*)
    	  pj_hash_get_lower( mod_tsx_layer.htable, key->ptr, 
			     (unsigned)key->slen, &hval );
    
    /* Prevent the transaction to get deleted before we have chance to lock it.
     */
    if (tsx)
        pj_grp_lock_add_ref(tsx->grp_lock);
    
    pj_mutex_unlock(mod_tsx_layer.mutex);

    TSX_TRACE_((THIS_FILE, 
		"Finding tsx with hkey=0x%p and key=%.*s: found %p",
		hval, key->slen, key->ptr, tsx));

    /* Simulate race condition! */
    PJ_RACE_ME(5);

    if (tsx) {
	if (lock)
	    pj_grp_lock_acquire(tsx->grp_lock);

        if (!add_ref)
            pj_grp_lock_dec_ref(tsx->grp_lock);
    }

    return tsx;
}


PJ_DEF(pjsip_transaction*) pjsip_tsx_layer_find_tsx( const pj_str_t *key,
						     pj_bool_t lock )
{
    return find_tsx(key, lock, PJ_FALSE);
}


PJ_DEF(pjsip_transaction*) pjsip_tsx_layer_find_tsx2( const pj_str_t *key,
						      pj_bool_t add_ref )
{
    return find_tsx(key, PJ_FALSE, add_ref);
}


/* This module callback is called when module is being loaded by
 * endpoint. It does nothing for this module.
 */
static pj_status_t mod_tsx_layer_load(pjsip_endpoint *endpt)
{
    PJ_UNUSED_ARG(endpt);
    return PJ_SUCCESS;
}


/* This module callback is called when module is being started by
 * endpoint. It does nothing for this module.
 */
static pj_status_t mod_tsx_layer_start(void)
{
    return PJ_SUCCESS;
}


/* This module callback is called when module is being stopped by
 * endpoint. 
 */
static pj_status_t mod_tsx_layer_stop(void)
{
    pj_hash_iterator_t it_buf, *it;

    PJ_LOG(4,(THIS_FILE, "Stopping transaction layer module"));

    pj_mutex_lock(mod_tsx_layer.mutex);

    /* Destroy all transactions. */
    it = pj_hash_first(mod_tsx_layer.htable, &it_buf);
    while (it) {
	pjsip_transaction *tsx = (pjsip_transaction*) 
				 pj_hash_this(mod_tsx_layer.htable, it);
	pj_hash_iterator_t *next = pj_hash_next(mod_tsx_layer.htable, it);
	if (tsx) {
	    pjsip_tsx_terminate(tsx, PJSIP_SC_SERVICE_UNAVAILABLE);
	    mod_tsx_layer_unregister_tsx(tsx);
	    tsx_shutdown(tsx);
	}
	it = next;
    }

    pj_mutex_unlock(mod_tsx_layer.mutex);

    PJ_LOG(4,(THIS_FILE, "Stopped transaction layer module"));

    return PJ_SUCCESS;
}


/* Destroy this module */
static void tsx_layer_destroy(pjsip_endpoint *endpt)
{
    PJ_UNUSED_ARG(endpt);

    /* Destroy mutex. */
    pj_mutex_destroy(mod_tsx_layer.mutex);

    /* Release pool. */
    pjsip_endpt_release_pool(mod_tsx_layer.endpt, mod_tsx_layer.pool);

    /* Mark as unregistered. */
    mod_tsx_layer.endpt = NULL;

    PJ_LOG(4,(THIS_FILE, "Transaction layer module destroyed"));
}


/* This module callback is called when module is being unloaded by
 * endpoint.
 */
static pj_status_t mod_tsx_layer_unload(void)
{
    /* Only self destroy when there's no transaction in the table.
     * Transaction may refuse to destroy when it has pending
     * transmission. If we destroy the module now, application will
     * crash when the pending transaction finally got error response
     * from transport and when it tries to unregister itself.
     */
    if (pj_hash_count(mod_tsx_layer.htable) != 0) {
	pj_status_t status;
	status = pjsip_endpt_atexit(mod_tsx_layer.endpt, &tsx_layer_destroy);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(3,(THIS_FILE, status,
		    "Failed to register transaction layer module destroy."));
	}
	return PJ_EBUSY;
    }

    tsx_layer_destroy(mod_tsx_layer.endpt);

    return PJ_SUCCESS;
}


/* This module callback is called when endpoint has received an
 * incoming request message.
 */
static pj_bool_t mod_tsx_layer_on_rx_request(pjsip_rx_data *rdata)
{
    pj_str_t key;
    pj_uint32_t hval = 0;
    pjsip_transaction *tsx;

    pjsip_tsx_create_key(rdata->tp_info.pool, &key, PJSIP_ROLE_UAS,
			 &rdata->msg_info.cseq->method, rdata);

    /* Find transaction. */
    pj_mutex_lock( mod_tsx_layer.mutex );

    tsx = (pjsip_transaction*) 
    	  pj_hash_get_lower( mod_tsx_layer.htable, key.ptr, (unsigned)key.slen, 
			     &hval );


    TSX_TRACE_((THIS_FILE, 
		"Finding tsx for request, hkey=0x%p and key=%.*s, found %p",
		hval, key.slen, key.ptr, tsx));


    if (tsx == NULL || tsx->state == PJSIP_TSX_STATE_TERMINATED) {
	/* Transaction not found.
	 * Reject the request so that endpoint passes the request to
	 * upper layer modules.
	 */
	pj_mutex_unlock( mod_tsx_layer.mutex);
	return PJ_FALSE;
    }

    /* Prevent the transaction to get deleted before we have chance to lock it
     * in pjsip_tsx_recv_msg().
     */
    pj_grp_lock_add_ref(tsx->grp_lock);
    
    /* Unlock hash table. */
    pj_mutex_unlock( mod_tsx_layer.mutex );

    /* Simulate race condition! */
    PJ_RACE_ME(5);

    /* Pass the message to the transaction. */
    pjsip_tsx_recv_msg(tsx, rdata );
    
    pj_grp_lock_dec_ref(tsx->grp_lock);

    return PJ_TRUE;
}


/* This module callback is called when endpoint has received an
 * incoming response message.
 */
static pj_bool_t mod_tsx_layer_on_rx_response(pjsip_rx_data *rdata)
{
    pj_str_t key;
    pj_uint32_t hval = 0;
    pjsip_transaction *tsx;

    pjsip_tsx_create_key(rdata->tp_info.pool, &key, PJSIP_ROLE_UAC,
			 &rdata->msg_info.cseq->method, rdata);

    /* Find transaction. */
    pj_mutex_lock( mod_tsx_layer.mutex );

    tsx = (pjsip_transaction*) 
    	  pj_hash_get_lower( mod_tsx_layer.htable, key.ptr, (unsigned)key.slen, 
			     &hval );


    TSX_TRACE_((THIS_FILE, 
		"Finding tsx for response, hkey=0x%p and key=%.*s, found %p",
		hval, key.slen, key.ptr, tsx));


    if (tsx == NULL || tsx->state == PJSIP_TSX_STATE_TERMINATED) {
	/* Transaction not found.
	 * Reject the request so that endpoint passes the request to
	 * upper layer modules.
	 */
	pj_mutex_unlock( mod_tsx_layer.mutex);
	return PJ_FALSE;
    }

    /* Prevent the transaction to get deleted before we have chance to lock it
     * in pjsip_tsx_recv_msg().
     */
    pj_grp_lock_add_ref(tsx->grp_lock);

    /* Unlock hash table. */
    pj_mutex_unlock( mod_tsx_layer.mutex );

    /* Simulate race condition! */
    PJ_RACE_ME(5);

    /* Pass the message to the transaction. */
    pjsip_tsx_recv_msg(tsx, rdata );
    
    pj_grp_lock_dec_ref(tsx->grp_lock);

    return PJ_TRUE;
}


/*
 * Get transaction instance in the rdata.
 */
PJ_DEF(pjsip_transaction*) pjsip_rdata_get_tsx( pjsip_rx_data *rdata )
{
    return (pjsip_transaction*) 
    	   rdata->endpt_info.mod_data[mod_tsx_layer.mod.id];
}


/*
 * Dump transaction layer.
 */
PJ_DEF(void) pjsip_tsx_layer_dump(pj_bool_t detail)
{
#if PJ_LOG_MAX_LEVEL >= 3
    pj_hash_iterator_t itbuf, *it;

    /* Lock mutex. */
    pj_mutex_lock(mod_tsx_layer.mutex);

    PJ_LOG(3, (THIS_FILE, "Dumping transaction table:"));
    PJ_LOG(3, (THIS_FILE, " Total %d transactions", 
			  pj_hash_count(mod_tsx_layer.htable)));

    if (detail) {
	it = pj_hash_first(mod_tsx_layer.htable, &itbuf);
	if (it == NULL) {
	    PJ_LOG(3, (THIS_FILE, " - none - "));
	} else {
	    while (it != NULL) {
		pjsip_transaction *tsx = (pjsip_transaction*) 
					 pj_hash_this(mod_tsx_layer.htable,it);

		PJ_LOG(3, (THIS_FILE, " %s %s|%d|%s",
			   tsx->obj_name,
			   (tsx->last_tx? 
				pjsip_tx_data_get_info(tsx->last_tx): 
				"none"),
			   tsx->status_code,
			   pjsip_tsx_state_str(tsx->state)));

		it = pj_hash_next(mod_tsx_layer.htable, it);
	    }
	}
    }

    /* Unlock mutex. */
    pj_mutex_unlock(mod_tsx_layer.mutex);
#endif
}

/*****************************************************************************
 **
 ** Transaction
 **
 *****************************************************************************
 **/
/* Lock transaction for accessing the timeout timer only. */
static void lock_timer(pjsip_transaction *tsx)
{
    pj_mutex_lock(tsx->mutex_b);
}

/* Unlock timer */
static void unlock_timer(pjsip_transaction *tsx)
{
    pj_mutex_unlock(tsx->mutex_b);
}

/* Utility: schedule a timer */
static pj_status_t tsx_schedule_timer(pjsip_transaction *tsx,
                                      pj_timer_entry *entry,
                                      const pj_time_val *delay,
                                      int active_id)
{
    pj_timer_heap_t *timer_heap = pjsip_endpt_get_timer_heap(tsx->endpt);
    pj_status_t status;

    pj_assert(active_id != 0);
    status = pj_timer_heap_schedule_w_grp_lock(timer_heap, entry,
                                               delay, active_id,
                                               tsx->grp_lock);

    return status;
}

/* Utility: cancel a timer */
static int tsx_cancel_timer(pjsip_transaction *tsx,
                            pj_timer_entry *entry)
{
    pj_timer_heap_t *timer_heap = pjsip_endpt_get_timer_heap(tsx->endpt);
    return pj_timer_heap_cancel_if_active(timer_heap, entry, TIMER_INACTIVE);
}

/* Create and initialize basic transaction structure.
 * This function is called by both UAC and UAS creation.
 */
static pj_status_t tsx_create( pjsip_module *tsx_user,
                               pj_grp_lock_t *grp_lock,
			       pjsip_transaction **p_tsx)
{
    pj_pool_t *pool;
    pjsip_transaction *tsx;
    pj_status_t status;

    pool = pjsip_endpt_create_pool( mod_tsx_layer.endpt, "tsx", 
				    PJSIP_POOL_TSX_LEN, PJSIP_POOL_TSX_INC );
    if (!pool)
	return PJ_ENOMEM;

    tsx = PJ_POOL_ZALLOC_T(pool, pjsip_transaction);
    tsx->pool = pool;
    tsx->tsx_user = tsx_user;
    tsx->endpt = mod_tsx_layer.endpt;

    pj_ansi_snprintf(tsx->obj_name, sizeof(tsx->obj_name), 
		     "tsx%p", tsx);
    pj_memcpy(pool->obj_name, tsx->obj_name, sizeof(pool->obj_name));

    tsx->handle_200resp = 1;
    tsx->retransmit_timer.id = TIMER_INACTIVE;
    tsx->retransmit_timer.user_data = tsx;
    tsx->retransmit_timer.cb = &tsx_timer_callback;
    tsx->timeout_timer.id = TIMER_INACTIVE;
    tsx->timeout_timer.user_data = tsx;
    tsx->timeout_timer.cb = &tsx_timer_callback;
    
    if (grp_lock) {
	tsx->grp_lock = grp_lock;
        
        pj_grp_lock_add_ref(tsx->grp_lock);
        pj_grp_lock_add_handler(tsx->grp_lock, tsx->pool, tsx, &tsx_on_destroy);
    } else {
	status = pj_grp_lock_create_w_handler(pool, NULL, tsx, &tsx_on_destroy,
					      &tsx->grp_lock);
	if (status != PJ_SUCCESS) {
	    pjsip_endpt_release_pool(mod_tsx_layer.endpt, pool);
	    return status;
	}
	
	pj_grp_lock_add_ref(tsx->grp_lock);
    }

    status = pj_mutex_create_simple(pool, tsx->obj_name, &tsx->mutex_b);
    if (status != PJ_SUCCESS) {
	tsx_shutdown(tsx);
	return status;
    }

    *p_tsx = tsx;
    return PJ_SUCCESS;
}

/* Really destroy transaction, when grp_lock reference is zero */
static void tsx_on_destroy( void *arg )
{
    pjsip_transaction *tsx = (pjsip_transaction*)arg;

    PJ_LOG(5,(tsx->obj_name, "Transaction destroyed!"));

    pj_mutex_destroy(tsx->mutex_b);
    pjsip_endpt_release_pool(tsx->endpt, tsx->pool);
}

/* Shutdown transaction. */
static pj_status_t tsx_shutdown( pjsip_transaction *tsx )
{
    /* Release the transport */
    tsx_update_transport(tsx, NULL);

    /* Decrement reference counter in transport selector, only if
     * we haven't been called before */
    if (!tsx->terminating) {
	pjsip_tpselector_dec_ref(&tsx->tp_sel);
    }

    /* Free last transmitted message. */
    if (tsx->last_tx) {
	pjsip_tx_data_dec_ref( tsx->last_tx );
	tsx->last_tx = NULL;
    }
    /* Cancel timeout timer. */
    tsx_cancel_timer(tsx, &tsx->timeout_timer);

    /* Cancel retransmission timer. */
    tsx_cancel_timer(tsx, &tsx->retransmit_timer);

    /* Clear some pending flags. */
    tsx->transport_flag &= ~(TSX_HAS_PENDING_RESCHED | TSX_HAS_PENDING_SEND);


    /* Refuse to destroy transaction if it has pending resolving. */
    if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
	tsx->transport_flag |= TSX_HAS_PENDING_DESTROY;
	tsx->tsx_user = NULL;
	PJ_LOG(4,(tsx->obj_name, "Will destroy later because transport is "
				 "in progress"));
    }

    if (!tsx->terminating) {
	tsx->terminating = PJ_TRUE;
	pj_grp_lock_dec_ref(tsx->grp_lock);
    }

    /* No acccess to tsx after this, it may have been destroyed */

    return PJ_SUCCESS;
}


/*
 * Callback when timer expires. Transport error also piggybacks this event
 * to avoid deadlock (https://trac.pjsip.org/repos/ticket/1646).
 */
static void tsx_timer_callback( pj_timer_heap_t *theap, pj_timer_entry *entry)
{
    pjsip_transaction *tsx = (pjsip_transaction*) entry->user_data;

    PJ_UNUSED_ARG(theap);

    /* Just return if transaction is already destroyed (see also #2102). */
    if (tsx->state >= PJSIP_TSX_STATE_DESTROYED) {
        return;
    }

    if (entry->id == TRANSPORT_ERR_TIMER) {
	/* Posted transport error event */
	entry->id = 0;
	if (tsx->state < PJSIP_TSX_STATE_TERMINATED) {
	    pjsip_tsx_state_e prev_state;
	    pj_time_val timeout = { 0, 0 };

	    pj_grp_lock_acquire(tsx->grp_lock);
	    prev_state = tsx->state;

	    /* Release transport as it's no longer working. */
	    tsx_update_transport(tsx, NULL);

	    if (tsx->status_code < 200) {
		pj_str_t err;
		char errmsg[PJ_ERR_MSG_SIZE];

		err = pj_strerror(tsx->transport_err, errmsg, sizeof(errmsg));
		tsx_set_status_code(tsx, PJSIP_SC_TSX_TRANSPORT_ERROR, &err);
	    }

	    /* Set transaction state etc, but don't notify TU now,
	     * otherwise we'll get a deadlock. See:
	     * https://trac.pjsip.org/repos/ticket/1646
	     */
	    /* Also don't schedule tsx handler, otherwise we'll get race
	     * condition of TU notifications due to delayed TERMINATED
	     * state TU notification. It happened in multiple worker threads
	     * environment between TERMINATED & DESTROYED! See:
	     * https://trac.pjsip.org/repos/ticket/1902
	     */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED,
	                   PJSIP_EVENT_TRANSPORT_ERROR, NULL,
			   NO_NOTIFY | NO_SCHEDULE_HANDLER);
	    pj_grp_lock_release(tsx->grp_lock);

	    /* Now notify TU about state change, WITHOUT holding the
	     * group lock. It should be safe to do so; transaction will
	     * not get destroyed because group lock reference counter
	     * has been incremented by the timer heap.
	     */
	    if (tsx->tsx_user && tsx->tsx_user->on_tsx_state) {
		pjsip_event e;
		PJSIP_EVENT_INIT_TSX_STATE(e, tsx,
		                           PJSIP_EVENT_TRANSPORT_ERROR, NULL,
					   prev_state);
		(*tsx->tsx_user->on_tsx_state)(tsx, &e);
	    }

	    /* Now let's schedule the tsx handler */
	    tsx_schedule_timer(tsx, &tsx->timeout_timer, &timeout,
			       TIMEOUT_TIMER);
	}
    } else {
	pjsip_event event;

	entry->id = 0;

	PJ_LOG(5,(tsx->obj_name, "%s timer event",
		 (entry==&tsx->retransmit_timer ? "Retransmit":"Timeout")));
	pj_log_push_indent();


	PJSIP_EVENT_INIT_TIMER(event, entry);

	/* Dispatch event to transaction. */
	pj_grp_lock_acquire(tsx->grp_lock);
	(*tsx->state_handler)(tsx, &event);
	pj_grp_lock_release(tsx->grp_lock);

	pj_log_pop_indent();
    }
}


/*
 * Set transaction state, and inform TU about the transaction state change.
 */
static void tsx_set_state( pjsip_transaction *tsx,
			   pjsip_tsx_state_e state,
			   pjsip_event_id_e event_src_type,
                           void *event_src,
			   int flag)
{
    pjsip_tsx_state_e prev_state = tsx->state;

    /* New state must be greater than previous state */
    pj_assert(state >= tsx->state);

    PJ_LOG(5, (tsx->obj_name, "State changed from %s to %s, event=%s",
	       state_str[tsx->state], state_str[state], 
               pjsip_event_str(event_src_type)));
    pj_log_push_indent();

    /* Change state. */
    tsx->state = state;

    /* Update the state handlers. */
    if (tsx->role == PJSIP_ROLE_UAC) {
	tsx->state_handler = tsx_state_handler_uac[state];
    } else {
	tsx->state_handler = tsx_state_handler_uas[state];
    }

    /* Before informing TU about state changed, inform TU about
     * rx event.
     */
    if (event_src_type==PJSIP_EVENT_RX_MSG && tsx->tsx_user &&
	(flag & NO_NOTIFY)==0)
    {
	pjsip_rx_data *rdata = (pjsip_rx_data*) event_src;

	pj_assert(rdata != NULL);

	if (rdata->msg_info.msg->type == PJSIP_RESPONSE_MSG &&
		   tsx->tsx_user->on_rx_response)
	{
	    (*tsx->tsx_user->on_rx_response)(rdata);
	}

    }

    /* Inform TU about state changed. */
    if (tsx->tsx_user && tsx->tsx_user->on_tsx_state &&
	(flag & NO_NOTIFY) == 0)
    {
	pjsip_event e;
	PJSIP_EVENT_INIT_TSX_STATE(e, tsx, event_src_type, event_src,
				   prev_state);

	/* For timer event, release lock to avoid deadlock.
	 * This should be safe because:
	 * 1. The tsx state just switches to TERMINATED or DESTROYED.
  	 * 2. There should be no other processing taking place. All other
  	 *    events, such as the ones handled by tsx_on_state_terminated()
  	 *    should be ignored.
         * 3. tsx_shutdown() hasn't been called.
	 * Refer to ticket #2001 (https://trac.pjsip.org/repos/ticket/2001).
	 */
	if (event_src_type == PJSIP_EVENT_TIMER &&
	    (pj_timer_entry *)event_src == &tsx->timeout_timer)
	{
	    pj_grp_lock_release(tsx->grp_lock);
	}

	(*tsx->tsx_user->on_tsx_state)(tsx, &e);

	if (event_src_type == PJSIP_EVENT_TIMER &&
	    (pj_timer_entry *)event_src == &tsx->timeout_timer)
	{
	    pj_grp_lock_acquire(tsx->grp_lock);
	}
    }
    

    /* When the transaction is terminated, release transport, and free the
     * saved last transmitted message.
     */
    if (state == PJSIP_TSX_STATE_TERMINATED) {
	pj_time_val timeout = { 0, 0 };

	/* If we're still waiting for a message to be sent.. */
	if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
	    /* Disassociate ourselves from the outstanding transmit data
	     * so that when the send callback is called we will be able
	     * to ignore that (otherwise we'll get assertion, see
	     * http://trac.pjsip.org/repos/ticket/1033)
	     */
	    if (tsx->pending_tx) {
		tsx->pending_tx->mod_data[mod_tsx_layer.mod.id] = NULL;
		tsx->pending_tx = NULL;

		/* Decrease pending send counter */
		pj_grp_lock_dec_ref(tsx->grp_lock);
	    }
	    tsx->transport_flag &= ~(TSX_HAS_PENDING_TRANSPORT);
	}

	lock_timer(tsx);
	tsx_cancel_timer(tsx, &tsx->timeout_timer);
	if ((flag & NO_SCHEDULE_HANDLER) == 0) {
	    tsx_schedule_timer(tsx, &tsx->timeout_timer, &timeout,
			       TIMEOUT_TIMER);
	}
	unlock_timer(tsx);

    } else if (state == PJSIP_TSX_STATE_DESTROYED) {

	/* Unregister transaction. */
	mod_tsx_layer_unregister_tsx(tsx);

	/* Destroy transaction. */
	tsx_shutdown(tsx);
    }

    pj_log_pop_indent();
}

/*
 * Create, initialize, and register UAC transaction.
 */
PJ_DEF(pj_status_t) pjsip_tsx_create_uac( pjsip_module *tsx_user,
					  pjsip_tx_data *tdata,
					  pjsip_transaction **p_tsx)
{
    return pjsip_tsx_create_uac2(tsx_user, tdata, NULL, p_tsx);
}

PJ_DEF(pj_status_t) pjsip_tsx_create_uac2(pjsip_module *tsx_user,
					  pjsip_tx_data *tdata,
					  pj_grp_lock_t *grp_lock,
					  pjsip_transaction **p_tsx)
{
    pjsip_transaction *tsx;
    pjsip_msg *msg;
    pjsip_cseq_hdr *cseq;
    pjsip_via_hdr *via;
    pjsip_host_info dst_info;
    pj_status_t status;

    /* Validate arguments. */
    PJ_ASSERT_RETURN(tdata && tdata->msg && p_tsx, PJ_EINVAL);
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* Method MUST NOT be ACK! */
    PJ_ASSERT_RETURN(tdata->msg->line.req.method.id != PJSIP_ACK_METHOD,
		     PJ_EINVALIDOP);

    /* Keep shortcut */
    msg = tdata->msg;

    /* Make sure CSeq header is present. */
    cseq = (pjsip_cseq_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    if (!cseq) {
	pj_assert(!"CSeq header not present in outgoing message!");
	return PJSIP_EMISSINGHDR;
    }


    /* Create transaction instance. */
    status = tsx_create( tsx_user, grp_lock, &tsx);
    if (status != PJ_SUCCESS)
	return status;


    /* Lock transaction.
     * We don't need to lock the group lock if none was supplied, while the
     * newly created group lock has not been exposed.
     */
    if (grp_lock)
        pj_grp_lock_acquire(tsx->grp_lock);

    /* Role is UAC. */
    tsx->role = PJSIP_ROLE_UAC;

    /* Save method. */
    pjsip_method_copy( tsx->pool, &tsx->method, &msg->line.req.method);

    /* Save CSeq. */
    tsx->cseq = cseq->cseq;

    /* Generate Via header if it doesn't exist. */
    via = (pjsip_via_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL);
    if (via == NULL) {
	via = pjsip_via_hdr_create(tdata->pool);
	pjsip_msg_insert_first_hdr(msg, (pjsip_hdr*) via);
    }

    /* Generate branch parameter if it doesn't exist. */
    if (via->branch_param.slen == 0) {
	pj_str_t tmp;
	via->branch_param.ptr = (char*)
				pj_pool_alloc(tsx->pool, PJSIP_MAX_BRANCH_LEN);
	via->branch_param.slen = PJSIP_MAX_BRANCH_LEN;
	pj_memcpy(via->branch_param.ptr, PJSIP_RFC3261_BRANCH_ID, 
		  PJSIP_RFC3261_BRANCH_LEN);
	tmp.ptr = via->branch_param.ptr + PJSIP_RFC3261_BRANCH_LEN + 2;
	*(tmp.ptr-2) = 80; *(tmp.ptr-1) = 106;
	pj_generate_unique_string( &tmp );

        /* Save branch parameter. */
        tsx->branch = via->branch_param;

    } else {
        /* Copy branch parameter. */
        pj_strdup(tsx->pool, &tsx->branch, &via->branch_param);
    }

   /* Generate transaction key. */
    create_tsx_key_3261( tsx->pool, &tsx->transaction_key,
			 PJSIP_ROLE_UAC, &tsx->method, 
			 &via->branch_param);

    /* Calculate hashed key value. */
#ifdef PRECALC_HASH
    tsx->hashed_key = pj_hash_calc_tolower(0, NULL, &tsx->transaction_key);
#endif

    PJ_LOG(6, (tsx->obj_name, "tsx_key=%.*s", tsx->transaction_key.slen,
	       tsx->transaction_key.ptr));

    /* Begin with State_Null.
     * Manually set-up the state becase we don't want to call the callback.
     */
    tsx->state = PJSIP_TSX_STATE_NULL;
    tsx->state_handler = &tsx_on_state_null;

    /* Save the message. */
    tsx->last_tx = tdata;
    pjsip_tx_data_add_ref(tsx->last_tx);

    /* Determine whether reliable transport should be used initially.
     * This will be updated whenever transport has changed.
     */
    status = pjsip_get_request_dest(tdata, &dst_info);
    if (status != PJ_SUCCESS) {
	if (grp_lock)
	    pj_grp_lock_release(tsx->grp_lock);
	tsx_shutdown(tsx);
	return status;
    }
    tsx->is_reliable = (dst_info.flag & PJSIP_TRANSPORT_RELIABLE);

    /* Register transaction to hash table. */
    status = mod_tsx_layer_register_tsx(tsx);
    if (status != PJ_SUCCESS) {
	/* The assertion is removed by #1090:
	pj_assert(!"Bug in branch_param generator (i.e. not unique)");
	*/
	if (grp_lock)
	    pj_grp_lock_release(tsx->grp_lock);
	tsx_shutdown(tsx);
	return status;
    }


    /* Unlock transaction and return. */
    if (grp_lock)
        pj_grp_lock_release(tsx->grp_lock);

    pj_log_push_indent();
    PJ_LOG(5,(tsx->obj_name, "Transaction created for %s",
	      pjsip_tx_data_get_info(tdata)));
    pj_log_pop_indent();

    *p_tsx = tsx;
    return PJ_SUCCESS;
}


/*
 * Create, initialize, and register UAS transaction.
 */
PJ_DEF(pj_status_t) pjsip_tsx_create_uas( pjsip_module *tsx_user,
					  pjsip_rx_data *rdata,
					  pjsip_transaction **p_tsx)
{
    return pjsip_tsx_create_uas2(tsx_user, rdata, NULL, p_tsx);
}

PJ_DEF(pj_status_t) pjsip_tsx_create_uas2(pjsip_module *tsx_user,
					  pjsip_rx_data *rdata,
					  pj_grp_lock_t *grp_lock,
					  pjsip_transaction **p_tsx)
{
    pjsip_transaction *tsx;
    pjsip_msg *msg;
    pj_str_t *branch;
    pjsip_cseq_hdr *cseq;
    pj_status_t status;

    /* Validate arguments. */
    PJ_ASSERT_RETURN(rdata && rdata->msg_info.msg && p_tsx, PJ_EINVAL);

    /* Keep shortcut to message */
    msg = rdata->msg_info.msg;
    
    /* Make sure this is a request message. */
    PJ_ASSERT_RETURN(msg->type == PJSIP_REQUEST_MSG, PJSIP_ENOTREQUESTMSG);

    /* Make sure method is not ACK */
    PJ_ASSERT_RETURN(msg->line.req.method.id != PJSIP_ACK_METHOD,
		     PJ_EINVALIDOP);

    /* Make sure CSeq header is present. */
    cseq = rdata->msg_info.cseq;
    if (!cseq)
	return PJSIP_EMISSINGHDR;

    /* Make sure Via header is present. */
    if (rdata->msg_info.via == NULL)
	return PJSIP_EMISSINGHDR;

    /* Check that method in CSeq header match request method.
     * Reference: PROTOS #1922
     */
    if (pjsip_method_cmp(&msg->line.req.method, 
			 &rdata->msg_info.cseq->method) != 0)
    {
	PJ_LOG(4,(THIS_FILE, "Error: CSeq header contains different "
			     "method than the request line"));
	return PJSIP_EINVALIDHDR;
    }

    /* 
     * Create transaction instance. 
     */
    status = tsx_create( tsx_user, grp_lock, &tsx);
    if (status != PJ_SUCCESS)
	return status;


    /* Lock transaction. */
    pj_grp_lock_acquire(tsx->grp_lock);

    /* Role is UAS */
    tsx->role = PJSIP_ROLE_UAS;

    /* Save method. */
    pjsip_method_copy( tsx->pool, &tsx->method, &msg->line.req.method);

    /* Save CSeq */
    tsx->cseq = cseq->cseq;

    /* Get transaction key either from branch for RFC3261 message, or
     * create transaction key.
     */
    status = pjsip_tsx_create_key(tsx->pool, &tsx->transaction_key, 
                                  PJSIP_ROLE_UAS, &tsx->method, rdata);
    if (status != PJ_SUCCESS) {
	pj_grp_lock_release(tsx->grp_lock);
	tsx_shutdown(tsx);
        return status;
    }

    /* Calculate hashed key value. */
#ifdef PRECALC_HASH
    tsx->hashed_key = pj_hash_calc_tolower(0, NULL, &tsx->transaction_key);
#endif

    /* Duplicate branch parameter for transaction. */
    branch = &rdata->msg_info.via->branch_param;
    pj_strdup(tsx->pool, &tsx->branch, branch);

    PJ_LOG(6, (tsx->obj_name, "tsx_key=%.*s", tsx->transaction_key.slen,
	       tsx->transaction_key.ptr));


    /* Begin with state NULL.
     * Manually set-up the state becase we don't want to call the callback.
     */
    tsx->state = PJSIP_TSX_STATE_NULL; 
    tsx->state_handler = &tsx_on_state_null;

    /* Get response address. */
    status = pjsip_get_response_addr( tsx->pool, rdata, &tsx->res_addr );
    if (status != PJ_SUCCESS) {
	pj_grp_lock_release(tsx->grp_lock);
	tsx_shutdown(tsx);
	return status;
    }

    /* If it's decided that we should use current transport, keep the
     * transport.
     */
    if (tsx->res_addr.transport) {
	tsx_update_transport(tsx, tsx->res_addr.transport);
	pj_memcpy(&tsx->addr, &tsx->res_addr.addr, tsx->res_addr.addr_len);
	tsx->addr_len = tsx->res_addr.addr_len;
	tsx->is_reliable = PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport);
    } else {
	tsx->is_reliable = 
	    (tsx->res_addr.dst_host.flag & PJSIP_TRANSPORT_RELIABLE);
    }


    /* Register the transaction. */
    status = mod_tsx_layer_register_tsx(tsx);
    if (status != PJ_SUCCESS) {
	pj_grp_lock_release(tsx->grp_lock);
	tsx_shutdown(tsx);
	return status;
    }

    /* Put this transaction in rdata's mod_data. */
    rdata->endpt_info.mod_data[mod_tsx_layer.mod.id] = tsx;

    /* Unlock transaction and return. */
    pj_grp_lock_release(tsx->grp_lock);

    pj_log_push_indent();
    PJ_LOG(5,(tsx->obj_name, "Transaction created for %s",
	      pjsip_rx_data_get_info(rdata)));
    pj_log_pop_indent();


    *p_tsx = tsx;
    return PJ_SUCCESS;
}


/*
 * Bind transaction to a specific transport/listener. 
 */
PJ_DEF(pj_status_t) pjsip_tsx_set_transport(pjsip_transaction *tsx,
					    const pjsip_tpselector *sel)
{
    /* Must be UAC transaction */
    PJ_ASSERT_RETURN(tsx && sel, PJ_EINVAL);

    /* Start locking the transaction. */
    pj_grp_lock_acquire(tsx->grp_lock);

    /* Decrement reference counter of previous transport selector */
    pjsip_tpselector_dec_ref(&tsx->tp_sel);

    /* Copy transport selector structure .*/
    pj_memcpy(&tsx->tp_sel, sel, sizeof(*sel));

    /* Increment reference counter */
    pjsip_tpselector_add_ref(&tsx->tp_sel);

    /* Unlock transaction. */
    pj_grp_lock_release(tsx->grp_lock);

    return PJ_SUCCESS;
}


/*
 * Set transaction status code and reason.
 */
static void tsx_set_status_code(pjsip_transaction *tsx,
			   	int code, const pj_str_t *reason)
{
    tsx->status_code = code;
    if (reason)
	pj_strdup(tsx->pool, &tsx->status_text, reason);
    else
	tsx->status_text = *pjsip_get_status_text(code);
}


/*
 * Forcely terminate transaction.
 */
PJ_DEF(pj_status_t) pjsip_tsx_terminate( pjsip_transaction *tsx, int code )
{
    PJ_ASSERT_RETURN(tsx != NULL, PJ_EINVAL);

    PJ_LOG(5,(tsx->obj_name, "Request to terminate transaction"));

    PJ_ASSERT_RETURN(code >= 200, PJ_EINVAL);

    pj_log_push_indent();

    pj_grp_lock_acquire(tsx->grp_lock);

    if (tsx->state < PJSIP_TSX_STATE_TERMINATED) {
        tsx_set_status_code(tsx, code, NULL);
        tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, PJSIP_EVENT_USER,
		       NULL, 0);
    }
    pj_grp_lock_release(tsx->grp_lock);

    pj_log_pop_indent();

    return PJ_SUCCESS;
}


/*
 * Cease retransmission on the UAC transaction. The UAC transaction is
 * still considered running, and it will complete when either final
 * response is received or the transaction times out.
 */
PJ_DEF(pj_status_t) pjsip_tsx_stop_retransmit(pjsip_transaction *tsx)
{
    PJ_ASSERT_RETURN(tsx != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(tsx->role == PJSIP_ROLE_UAC &&
		     tsx->method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVALIDOP);

    PJ_LOG(5,(tsx->obj_name, "Request to stop retransmission"));

    pj_log_push_indent();

    pj_grp_lock_acquire(tsx->grp_lock);
    /* Cancel retransmission timer. */
    tsx_cancel_timer(tsx, &tsx->retransmit_timer);
    pj_grp_lock_release(tsx->grp_lock);

    pj_log_pop_indent();

    return PJ_SUCCESS;
}


/*
 * Start a timer to terminate transaction after the specified time
 * has elapsed. 
 */
PJ_DEF(pj_status_t) pjsip_tsx_set_timeout( pjsip_transaction *tsx,
					   unsigned millisec)
{
    pj_time_val timeout;

    PJ_ASSERT_RETURN(tsx != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(tsx->role == PJSIP_ROLE_UAC &&
		     tsx->method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVALIDOP);

    /* Note: must not call pj_grp_lock_acquire(tsx->grp_lock) as
     * that would introduce deadlock. See #1121.
     */
    lock_timer(tsx);

    /* Transaction should normally not have final response, but as
     * #1121 says there is a (tolerable) window of race condition
     * where this might happen.
     */
    if (tsx->status_code >= 200 && tsx->timeout_timer.id != 0) {
	/* Timeout is already set */
	unlock_timer(tsx);
	return PJ_EEXISTS;
    }

    tsx_cancel_timer(tsx, &tsx->timeout_timer);

    timeout.sec = 0;
    timeout.msec = millisec;
    pj_time_val_normalize(&timeout);

    tsx_schedule_timer(tsx, &tsx->timeout_timer, &timeout, TIMEOUT_TIMER);

    unlock_timer(tsx);

    return PJ_SUCCESS;
}


/*
 * This function is called by TU to send a message.
 */
PJ_DEF(pj_status_t) pjsip_tsx_send_msg( pjsip_transaction *tsx, 
				        pjsip_tx_data *tdata )
{
    pjsip_event event;
    pj_status_t status;

    if (tdata == NULL)
	tdata = tsx->last_tx;

    PJ_ASSERT_RETURN(tdata != NULL, PJ_EINVALIDOP);

    PJ_LOG(5,(tsx->obj_name, "Sending %s in state %s",
                             pjsip_tx_data_get_info(tdata),
			     state_str[tsx->state]));
    pj_log_push_indent();

    PJSIP_EVENT_INIT_TX_MSG(event, tdata);

    /* Dispatch to transaction. */
    pj_grp_lock_acquire(tsx->grp_lock);

    /* Set transport selector to tdata */
    pjsip_tx_data_set_transport(tdata, &tsx->tp_sel);

    /* Dispatch to state handler */
    status = (*tsx->state_handler)(tsx, &event);

    pj_grp_lock_release(tsx->grp_lock);

    /* Only decrement reference counter when it returns success.
     * (This is the specification from the .PDF design document).
     */
    if (status == PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
    }

    pj_log_pop_indent();

    return status;
}


/*
 * This function is called by endpoint when incoming message for the 
 * transaction is received.
 */
PJ_DEF(void) pjsip_tsx_recv_msg( pjsip_transaction *tsx, 
				 pjsip_rx_data *rdata)
{
    pjsip_event event;

    PJ_LOG(5,(tsx->obj_name, "Incoming %s in state %s", 
	      pjsip_rx_data_get_info(rdata), state_str[tsx->state]));
    pj_log_push_indent();

    /* Put the transaction in the rdata's mod_data. */
    rdata->endpt_info.mod_data[mod_tsx_layer.mod.id] = tsx;

    /* Init event. */
    PJSIP_EVENT_INIT_RX_MSG(event, rdata);

    /* Dispatch to transaction. */
    pj_grp_lock_acquire(tsx->grp_lock);
    (*tsx->state_handler)(tsx, &event);
    pj_grp_lock_release(tsx->grp_lock);

    pj_log_pop_indent();
}


/* Callback called by send message framework */
static void send_msg_callback( pjsip_send_state *send_state,
			       pj_ssize_t sent, pj_bool_t *cont )
{
    pjsip_transaction *tsx = (pjsip_transaction*) send_state->token;
    pjsip_tx_data *tdata = send_state->tdata;

    /* Check if transaction has cancelled itself from this transmit
     * notification (https://trac.pjsip.org/repos/ticket/1033).
     * Also check if the transaction layer itself may have been shutdown
     * (https://trac.pjsip.org/repos/ticket/1535)
     */
    if (mod_tsx_layer.mod.id < 0 ||
	tdata->mod_data[mod_tsx_layer.mod.id] == NULL)
    {
	*cont = PJ_FALSE;

	/* Decrease pending send counter, but only if the transaction layer
	 * hasn't been shutdown.
	 */
	// If tsx has cancelled itself from this transmit notification
	// it should have also decreased pending send counter.
	//if (mod_tsx_layer.mod.id >= 0)
	//    pj_grp_lock_dec_ref(tsx->grp_lock);

	return;
    }

    pj_grp_lock_acquire(tsx->grp_lock);

    /* Decrease pending send counter */
    pj_grp_lock_dec_ref(tsx->grp_lock);

    /* Reset */
    tdata->mod_data[mod_tsx_layer.mod.id] = NULL;
    tsx->pending_tx = NULL;

    if (sent > 0) {
	/* Successfully sent! */
	pj_assert(send_state->cur_transport != NULL);

	if (tsx->transport != send_state->cur_transport) {
	    /* Update transport. */
	    tsx_update_transport(tsx, send_state->cur_transport);

	    /* Update remote address. */
	    tsx->addr_len = tdata->dest_info.addr.entry[tdata->dest_info.cur_addr].addr_len;
	    pj_memcpy(&tsx->addr, 
		      &tdata->dest_info.addr.entry[tdata->dest_info.cur_addr].addr,
		      tsx->addr_len);

	    /* Update is_reliable flag. */
	    tsx->is_reliable = PJSIP_TRANSPORT_IS_RELIABLE(tsx->transport);
	}

	/* Clear pending transport flag. */
	tsx->transport_flag &= ~(TSX_HAS_PENDING_TRANSPORT);

	/* Mark that we have resolved the addresses. */
	tsx->transport_flag |= TSX_HAS_RESOLVED_SERVER;

	/* Pending destroy? */
	if (tsx->transport_flag & TSX_HAS_PENDING_DESTROY) {
	    tsx_set_state( tsx, PJSIP_TSX_STATE_DESTROYED, 
			   PJSIP_EVENT_UNKNOWN, NULL, 0 );
	    pj_grp_lock_release(tsx->grp_lock);
	    return;
	}

	/* Need to transmit a message? */
	if (tsx->transport_flag & TSX_HAS_PENDING_SEND) {
	    tsx->transport_flag &= ~(TSX_HAS_PENDING_SEND);
	    tsx_send_msg(tsx, tsx->last_tx);
	}

	/* Need to reschedule retransmission?
	 * Note that when sending a pending message above, tsx_send_msg()
	 * may set the flag TSX_HAS_PENDING_TRANSPORT.
	 * Please refer to ticket #1875.
	 */
	if (tsx->transport_flag & TSX_HAS_PENDING_RESCHED &&
	    !(tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT))
	{
	    tsx->transport_flag &= ~(TSX_HAS_PENDING_RESCHED);

	    /* Only update when transport turns out to be unreliable. */
	    if (!tsx->is_reliable) {
		tsx_resched_retransmission(tsx);
	    }
	}

    } else {
	/* Failed to send! */
	pj_assert(sent != 0);

	/* If transaction is using the same transport as the failed one, 
	 * release the transport.
	 */
	if (send_state->cur_transport==tsx->transport)
	    tsx_update_transport(tsx, NULL);

	/* Also stop processing if transaction has been flagged with
	 * pending destroy (http://trac.pjsip.org/repos/ticket/906)
	 */
	if ((!*cont) || (tsx->transport_flag & TSX_HAS_PENDING_DESTROY)) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pjsip_status_code sc;
	    pj_str_t err;

	    tsx->transport_err = (pj_status_t)-sent;

	    err =pj_strerror((pj_status_t)-sent, errmsg, sizeof(errmsg));

	    PJ_LOG(3,(tsx->obj_name,
		      "Failed to send %s! err=%d (%s)",
		      pjsip_tx_data_get_info(send_state->tdata), -sent,
		      errmsg));

	    /* Clear pending transport flag. */
	    tsx->transport_flag &= ~(TSX_HAS_PENDING_TRANSPORT);

	    /* Mark that we have resolved the addresses. */
	    tsx->transport_flag |= TSX_HAS_RESOLVED_SERVER;

	    /* Server resolution error is now mapped to 502 instead of 503,
	     * since with 503 normally client should try again.
	     * See http://trac.pjsip.org/repos/ticket/870
	     */
	    if (-sent==PJ_ERESOLVE || -sent==PJLIB_UTIL_EDNS_NXDOMAIN)
		sc = PJSIP_SC_BAD_GATEWAY;
	    else
		sc = PJSIP_SC_TSX_TRANSPORT_ERROR;

	    /* Terminate transaction, if it's not already terminated. */
	    tsx_set_status_code(tsx, sc, &err);
	    if (tsx->state != PJSIP_TSX_STATE_TERMINATED &&
		tsx->state != PJSIP_TSX_STATE_DESTROYED)
	    {
		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
			       PJSIP_EVENT_TRANSPORT_ERROR,
			       send_state->tdata, 0);
	    } 
	    /* Don't forget to destroy if we have pending destroy flag
	     * (http://trac.pjsip.org/repos/ticket/906)
	     */
	    else if (tsx->transport_flag & TSX_HAS_PENDING_DESTROY)
	    {
		tsx_set_state( tsx, PJSIP_TSX_STATE_DESTROYED, 
			       PJSIP_EVENT_TRANSPORT_ERROR,
			       send_state->tdata, 0);
	    }

	} else {
	    PJ_PERROR(3,(tsx->obj_name, (pj_status_t)-sent,
		         "Temporary failure in sending %s, "
		         "will try next server",
		         pjsip_tx_data_get_info(send_state->tdata)));

	    /* Reset retransmission count */
	    tsx->retransmit_count = 0;

	    /* And reset timeout timer */
	    if (tsx->timeout_timer.id) {
		lock_timer(tsx);
		tsx_cancel_timer(tsx, &tsx->timeout_timer);
		tsx_schedule_timer( tsx, &tsx->timeout_timer,
				    &timeout_timer_val, TIMEOUT_TIMER);
		unlock_timer(tsx);
	    }

	    /* Put again pending tdata */
	    tdata->mod_data[mod_tsx_layer.mod.id] = tsx;
	    tsx->pending_tx = tdata;

	    /* Increment group lock again for the next sending retry,
	     * to prevent us from being destroyed prematurely (ticket #1859).
	     */
	    pj_grp_lock_add_ref(tsx->grp_lock);
	}
    }

    pj_grp_lock_release(tsx->grp_lock);
}


/* Transport callback. */
static void transport_callback(void *token, pjsip_tx_data *tdata,
			       pj_ssize_t sent)
{
    pjsip_transaction *tsx = (pjsip_transaction*) token;

    /* Check if the transaction layer has been shutdown. */
    if (mod_tsx_layer.mod.id < 0)
	return;

    /* In other circumstances, locking tsx->grp_lock AFTER transport mutex
     * will introduce deadlock if another thread is currently sending a
     * SIP message to the transport. But this should be safe as there should
     * be no way this callback could be called while another thread is
     * sending a message.
     */
    pj_grp_lock_acquire(tsx->grp_lock);
    tsx->transport_flag &= ~(TSX_HAS_PENDING_TRANSPORT);
    pj_grp_lock_release(tsx->grp_lock);

    if (sent < 0) {
	pj_time_val delay = {0, 0};

	PJ_PERROR(2,(tsx->obj_name, (pj_status_t)-sent,
		      "Transport failed to send %s!",
		      pjsip_tx_data_get_info(tdata)));

	/* Post the event for later processing, to avoid deadlock.
	 * See https://trac.pjsip.org/repos/ticket/1646
	 */
	lock_timer(tsx);
	tsx->transport_err = (pj_status_t)-sent;
	/* Don't cancel timeout timer if tsx state is already
	 * PJSIP_TSX_STATE_COMPLETED (see #2076).
	 */
	if (tsx->state < PJSIP_TSX_STATE_COMPLETED) {
	    tsx_cancel_timer(tsx, &tsx->timeout_timer);
	    tsx_schedule_timer(tsx, &tsx->timeout_timer, &delay,
			       TRANSPORT_ERR_TIMER);
	}
	unlock_timer(tsx);
   }

    /* Decrease pending send counter */
    pj_grp_lock_dec_ref(tsx->grp_lock);
}


/*
 * Callback when transport state changes.
 */
static void tsx_tp_state_callback( pjsip_transport *tp,
				   pjsip_transport_state state,
				   const pjsip_transport_state_info *info)
{
    PJ_UNUSED_ARG(tp);

    if (state == PJSIP_TP_STATE_DISCONNECTED) {
	pjsip_transaction *tsx;
	pj_time_val delay = {0, 0};

	pj_assert(tp && info && info->user_data);

	tsx = (pjsip_transaction*)info->user_data;

	/* Post the event for later processing, to avoid deadlock.
	 * See https://trac.pjsip.org/repos/ticket/1646
	 */
	lock_timer(tsx);
	tsx->transport_err = info->status;
	/* Don't cancel timeout timer if tsx state is already
	 * PJSIP_TSX_STATE_COMPLETED (see #2076).
	 */
	if (tsx->state < PJSIP_TSX_STATE_COMPLETED) {
	    tsx_cancel_timer(tsx, &tsx->timeout_timer);
	    tsx_schedule_timer(tsx, &tsx->timeout_timer, &delay,
			       TRANSPORT_ERR_TIMER);
	}
	unlock_timer(tsx);
    }
}


/*
 * Send message to the transport.
 */
static pj_status_t tsx_send_msg( pjsip_transaction *tsx, 
                                 pjsip_tx_data *tdata)
{
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(tsx && tdata, PJ_EINVAL);

    /* Send later if transport is still pending. */
    if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
	tsx->transport_flag |= TSX_HAS_PENDING_SEND;
	return PJ_SUCCESS;
    }

    /* Skip send if previous tdata transmission is pending (see #1665). */
    if (tdata->is_pending) {
	PJ_LOG(2,(THIS_FILE, "Unable to send %s: message is pending", 
			     pjsip_tx_data_get_info(tdata)));
	return PJ_SUCCESS;
    }

    /* If we have the transport, send the message using that transport.
     * Otherwise perform full transport resolution.
     */
    if (tsx->transport) {
	/* Increment group lock while waiting for send operation to complete,
	 * to prevent us from being destroyed prematurely. See
	 * https://trac.pjsip.org/repos/ticket/1646
	 */
	pj_grp_lock_add_ref(tsx->grp_lock);
	tsx->transport_flag |= TSX_HAS_PENDING_TRANSPORT;

	status = pjsip_transport_send( tsx->transport, tdata, &tsx->addr,
				       tsx->addr_len, tsx, 
				       &transport_callback);
	if (status == PJ_EPENDING)
	    status = PJ_SUCCESS;
	else {
	    /* Operation completes immediately */
	    tsx->transport_flag &= ~(TSX_HAS_PENDING_TRANSPORT);
	    pj_grp_lock_dec_ref(tsx->grp_lock);
	}

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(2,(tsx->obj_name, status,
		         "Error sending %s",
		         pjsip_tx_data_get_info(tdata)));

	    /* On error, release transport to force using full transport
	     * resolution procedure.
	     */
	    tsx_update_transport(tsx, NULL);

	    tsx->addr_len = 0;
	    tsx->res_addr.transport = NULL;
	    tsx->res_addr.addr_len = 0;
	} else {
	    return PJ_SUCCESS;
	}
    }

    /* We are here because we don't have transport, or we failed to send
     * the message using existing transport. If we haven't resolved the
     * server before, then begin the long process of resolving the server
     * and send the message with possibly new server.
     */
    pj_assert(status != PJ_SUCCESS || tsx->transport == NULL);

    /* If we have resolved the server, we treat the error as permanent error.
     * Terminate transaction with transport error failure.
     */
    if (tsx->transport_flag & TSX_HAS_RESOLVED_SERVER) {
	
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_str_t err;

	if (status == PJ_SUCCESS) {
	    pj_assert(!"Unexpected status!");
	    status = PJ_EUNKNOWN;
	}

	/* We have resolved the server!.
	 * Treat this as permanent transport error.
	 */
	err = pj_strerror(status, errmsg, sizeof(errmsg));

	PJ_LOG(2,(tsx->obj_name, 
		  "Transport error, terminating transaction. "
		  "Err=%d (%s)",
		  status, errmsg));

	tsx_set_status_code(tsx, PJSIP_SC_TSX_TRANSPORT_ERROR, &err);
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
		       PJSIP_EVENT_TRANSPORT_ERROR, NULL, 0 );

	return status;
    }

    /* Must add reference counter because the send request functions
     * decrement the reference counter.
     */
    pjsip_tx_data_add_ref(tdata);

    /* Also attach ourselves to the transmit data so that we'll be able
     * to unregister ourselves from the send notification of this
     * transmit data.
     */
    tdata->mod_data[mod_tsx_layer.mod.id] = tsx;
    tsx->pending_tx = tdata;

    /* Increment group lock while waiting for send operation to complete,
     * to prevent us from being destroyed prematurely (ticket #1859).
     */
    pj_grp_lock_add_ref(tsx->grp_lock);

    /* Begin resolving destination etc to send the message. */
    if (tdata->msg->type == PJSIP_REQUEST_MSG) {

	tsx->transport_flag |= TSX_HAS_PENDING_TRANSPORT;
	status = pjsip_endpt_send_request_stateless(tsx->endpt, tdata, tsx,
						    &send_msg_callback);
	if (status == PJ_EPENDING)
	    status = PJ_SUCCESS;
	if (status != PJ_SUCCESS) {
	    tsx->transport_flag &= ~(TSX_HAS_PENDING_TRANSPORT);
	    pj_grp_lock_dec_ref(tsx->grp_lock);
	    pjsip_tx_data_dec_ref(tdata);
	    tdata->mod_data[mod_tsx_layer.mod.id] = NULL;
	    tsx->pending_tx = NULL;
	}
	
	/* Check if transaction is terminated. */
	if (status==PJ_SUCCESS && tsx->state == PJSIP_TSX_STATE_TERMINATED)
	    status = tsx->transport_err;

    } else {

	tsx->transport_flag |= TSX_HAS_PENDING_TRANSPORT;
	status = pjsip_endpt_send_response( tsx->endpt, &tsx->res_addr, 
					    tdata, tsx, 
					    &send_msg_callback);
	if (status == PJ_EPENDING)
	    status = PJ_SUCCESS;
	if (status != PJ_SUCCESS) {
	    tsx->transport_flag &= ~(TSX_HAS_PENDING_TRANSPORT);
	    pj_grp_lock_dec_ref(tsx->grp_lock);
	    pjsip_tx_data_dec_ref(tdata);
	    tdata->mod_data[mod_tsx_layer.mod.id] = NULL;
	    tsx->pending_tx = NULL;
	}

	/* Check if transaction is terminated. */
	if (status==PJ_SUCCESS && tsx->state == PJSIP_TSX_STATE_TERMINATED)
	    status = tsx->transport_err;

    }


    return status;
}


/*
 * Manually retransmit the last messagewithout updating the transaction state.
 */
PJ_DEF(pj_status_t) pjsip_tsx_retransmit_no_state(pjsip_transaction *tsx,
						  pjsip_tx_data *tdata)
{
    pj_status_t status;

    pj_grp_lock_acquire(tsx->grp_lock);
    if (tdata == NULL) {
	tdata = tsx->last_tx;
	pjsip_tx_data_add_ref(tdata);
    }
    status = tsx_send_msg(tsx, tdata);
    pj_grp_lock_release(tsx->grp_lock);

    /* Only decrement reference counter when it returns success.
     * (This is the specification from the .PDF design document).
     */
    if (status == PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
    }

    return status;
}


/*
 * Retransmit last message sent.
 */
static void tsx_resched_retransmission( pjsip_transaction *tsx )
{
    pj_uint32_t msec_time;

    pj_assert((tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) == 0);

    if (tsx->role==PJSIP_ROLE_UAC && tsx->status_code >= 100)
	msec_time = pjsip_cfg()->tsx.t2;
    else
	msec_time = (1 << (tsx->retransmit_count)) * pjsip_cfg()->tsx.t1;

    if (tsx->role == PJSIP_ROLE_UAC) {
	pj_assert(tsx->status_code < 200);
	/* Retransmission for non-INVITE transaction caps-off at T2 */
	if (msec_time > pjsip_cfg()->tsx.t2 && 
	    tsx->method.id != PJSIP_INVITE_METHOD)
	{
	    msec_time = pjsip_cfg()->tsx.t2;
	}
    } else {
	/* For UAS, this can be retransmission of 2xx response for INVITE
	 * or non-100 1xx response.
	 */
	if (tsx->status_code < 200) {
	    /* non-100 1xx retransmission is at 60 seconds */
	    msec_time = PJSIP_TSX_1XX_RETRANS_DELAY * 1000;
	} else {
	    /* Retransmission of INVITE final response also caps-off at T2 */
	    pj_assert(tsx->status_code >= 200);
	    if (msec_time > pjsip_cfg()->tsx.t2)
		msec_time = pjsip_cfg()->tsx.t2;
	}
    }

    if (msec_time != 0) {
	pj_time_val timeout;

	timeout.sec = msec_time / 1000;
	timeout.msec = msec_time % 1000;
	tsx_schedule_timer( tsx, &tsx->retransmit_timer, &timeout,
	                    RETRANSMIT_TIMER);
    }
}

/*
 * Retransmit last message sent.
 */
static pj_status_t tsx_retransmit( pjsip_transaction *tsx, int resched)
{
    pj_status_t status;

    if (resched && pj_timer_entry_running(&tsx->retransmit_timer)) {
	/* We've been asked to reschedule but the timer is already rerunning.
	 * This can only happen in a race condition where, between removing
	 * this retransmit timer from the heap and actually scheduling it,
	 * another thread has got in and rescheduled the timer itself.  In
	 * this scenario, the transmission has already happened and so we
	 * should just quit out immediately, without either resending the
	 * message or restarting the timer.
	 */
	return PJ_SUCCESS;
    }

    PJ_ASSERT_RETURN(tsx->last_tx!=NULL, PJ_EBUG);

    PJ_LOG(5,(tsx->obj_name, "Retransmiting %s, count=%d, restart?=%d", 
	      pjsip_tx_data_get_info(tsx->last_tx), 
	      tsx->retransmit_count, resched));

    ++tsx->retransmit_count;

    /* Restart timer T1 first before sending the message to ensure that
     * retransmission timer is not engaged when loop transport is used.
     */
    if (resched) {
	pj_assert(tsx->state != PJSIP_TSX_STATE_CONFIRMED);
	if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
	    tsx->transport_flag |= TSX_HAS_PENDING_RESCHED;
	} else {
	    tsx_resched_retransmission(tsx);
	}
    }

    status = tsx_send_msg( tsx, tsx->last_tx);
    if (status != PJ_SUCCESS) {
	return status;
    }

    return PJ_SUCCESS;
}

static void tsx_update_transport( pjsip_transaction *tsx, 
				  pjsip_transport *tp)
{
    pj_assert(tsx);

    if (tsx->transport) {
	if (tsx->tp_st_key) {
	    pjsip_transport_remove_state_listener(tsx->transport,
						  tsx->tp_st_key, tsx);
	}
	pjsip_transport_dec_ref( tsx->transport );
	tsx->transport = NULL;
    }

    if (tp) {
	tsx->transport = tp;
	pjsip_transport_add_ref(tp);
	pjsip_transport_add_state_listener(tp, &tsx_tp_state_callback, tsx,
					    &tsx->tp_st_key);
        if (tp->is_shutdown) {
	    pjsip_transport_state_info info;

	    pj_bzero(&info, sizeof(info));
            info.user_data = tsx;
            info.status = PJSIP_SC_TSX_TRANSPORT_ERROR;
            tsx_tp_state_callback(tp, PJSIP_TP_STATE_DISCONNECTED, &info);
        }
    }
}

/*
 * Handler for events in state Null.
 */
static pj_status_t tsx_on_state_null( pjsip_transaction *tsx, 
                                      pjsip_event *event )
{
    pj_status_t status;

    pj_assert(tsx->state == PJSIP_TSX_STATE_NULL);

    if (tsx->role == PJSIP_ROLE_UAS) {

	/* Set state to Trying. */
	pj_assert(event->type == PJSIP_EVENT_RX_MSG &&
		  event->body.rx_msg.rdata->msg_info.msg->type == 
		    PJSIP_REQUEST_MSG);
	tsx_set_state( tsx, PJSIP_TSX_STATE_TRYING, PJSIP_EVENT_RX_MSG,
		       event->body.rx_msg.rdata, 0);

    } else {
	pjsip_tx_data *tdata;

	/* Must be transmit event. 
	 * You may got this assertion when using loop transport with delay 
	 * set to zero. That would cause on_rx_response() callback to be 
	 * called before tsx_send_msg() has completed.
	 */
	PJ_ASSERT_RETURN(event->type == PJSIP_EVENT_TX_MSG, PJ_EBUG);

	/* Get the txdata */
	tdata = event->body.tx_msg.tdata;

	/* Save the message for retransmission. */
	if (tsx->last_tx && tsx->last_tx != tdata) {
	    pjsip_tx_data_dec_ref(tsx->last_tx);
	    tsx->last_tx = NULL;
	}
	if (tsx->last_tx != tdata) {
	    tsx->last_tx = tdata;
	    pjsip_tx_data_add_ref(tdata);
	}

	/* Send the message. */
        status = tsx_send_msg( tsx, tdata);
	if (status != PJ_SUCCESS) {
	    return status;
	}

	/* Start Timer B (or called timer F for non-INVITE) for transaction 
	 * timeout.
	 */
	lock_timer(tsx);
	tsx_cancel_timer( tsx, &tsx->timeout_timer );
	tsx_schedule_timer( tsx, &tsx->timeout_timer, &timeout_timer_val,
	                    TIMEOUT_TIMER);
	unlock_timer(tsx);

	/* Start Timer A (or timer E) for retransmission only if unreliable 
	 * transport is being used.
	 */
	if (!tsx->is_reliable)  {
	    tsx->retransmit_count = 0;
	    if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
		tsx->transport_flag |= TSX_HAS_PENDING_RESCHED;
	    } else {
		tsx_schedule_timer(tsx, &tsx->retransmit_timer,
		                   &t1_timer_val, RETRANSMIT_TIMER);
	    }
	}

	/* Move state. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_CALLING, 
                       PJSIP_EVENT_TX_MSG, tdata, 0);
    }

    return PJ_SUCCESS;
}


/*
 * State Calling is for UAC after it sends request but before any responses
 * is received.
 */
static pj_status_t tsx_on_state_calling( pjsip_transaction *tsx, 
				         pjsip_event *event )
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_CALLING);
    pj_assert(tsx->role == PJSIP_ROLE_UAC);

    if (event->type == PJSIP_EVENT_TIMER && 
	event->body.timer.entry == &tsx->retransmit_timer) 
    {
        pj_status_t status;

	/* Retransmit the request. */
        status = tsx_retransmit( tsx, 1 );
	if (status != PJ_SUCCESS) {
	    return status;
	}

    } else if (event->type == PJSIP_EVENT_TIMER && 
	       event->body.timer.entry == &tsx->timeout_timer) 
    {
	/* Cancel retransmission timer. */
	tsx_cancel_timer(tsx, &tsx->retransmit_timer);

	tsx->transport_flag &= ~(TSX_HAS_PENDING_RESCHED);

	/* Set status code */
	tsx_set_status_code(tsx, PJSIP_SC_TSX_TIMEOUT, NULL);

	/* Inform TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED,
                       PJSIP_EVENT_TIMER, &tsx->timeout_timer, 0);

	/* Transaction is destroyed */
	//return PJSIP_ETSXDESTROYED;

    } else if (event->type == PJSIP_EVENT_RX_MSG) {
	pjsip_msg *msg;
	int code;

	/* Get message instance */
	msg = event->body.rx_msg.rdata->msg_info.msg;

	/* Better be a response message. */
	if (msg->type != PJSIP_RESPONSE_MSG)
	    return PJSIP_ENOTRESPONSEMSG;

	code = msg->line.status.code;

	/* If the response is final, cancel both retransmission and timeout
	 * timer.
	 */
	if (code >= 200) {
	    tsx_cancel_timer(tsx, &tsx->retransmit_timer);

	    if (tsx->timeout_timer.id != 0) {
		lock_timer(tsx);
		tsx_cancel_timer(tsx, &tsx->timeout_timer);
		unlock_timer(tsx);
	    }

	} else {
	    /* Cancel retransmit timer (for non-INVITE transaction, the
	     * retransmit timer will be rescheduled at T2.
	     */
	    tsx_cancel_timer(tsx, &tsx->retransmit_timer);

	    /* For provisional response, only cancel retransmit when this
	     * is an INVITE transaction. For non-INVITE, section 17.1.2.1
	     * of RFC 3261 says that:
	     *	- retransmit timer is set to T2
	     *	- timeout timer F is not deleted.
	     */
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {

		/* Cancel timeout timer */
		lock_timer(tsx);
		tsx_cancel_timer(tsx, &tsx->timeout_timer);
		unlock_timer(tsx);

	    } else {
		if (!tsx->is_reliable) {
		    tsx_schedule_timer(tsx, &tsx->retransmit_timer,
		                       &t2_timer_val, RETRANSMIT_TIMER);
		}
	    }
	}
	 
	tsx->transport_flag &= ~(TSX_HAS_PENDING_RESCHED);


	/* Discard retransmission message if it is not INVITE.
	 * The INVITE tdata is needed in case we have to generate ACK for
	 * the final response.
	 */
	/* Keep last_tx for authorization. */
	//blp: always keep last_tx until transaction is destroyed
	//code = msg->line.status.code;
	//if (tsx->method.id != PJSIP_INVITE_METHOD && code!=401 && code!=407) {
	//    pjsip_tx_data_dec_ref(tsx->last_tx);
	//    tsx->last_tx = NULL;
	//}

	/* Processing is similar to state Proceeding. */
	tsx_on_state_proceeding_uac( tsx, event);

    } else {
	pj_assert(!"Unexpected event");
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
static pj_status_t tsx_on_state_trying( pjsip_transaction *tsx, 
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
    if (event->type != PJSIP_EVENT_TX_MSG) {
	return PJ_SUCCESS;
    }

    /* The rest of the processing of the event is exactly the same as in
     * "Proceeding" state.
     */
    status = tsx_on_state_proceeding_uas( tsx, event);

    /* Inform the TU of the state transision if state is still State_Trying */
    if (status==PJ_SUCCESS && tsx->state == PJSIP_TSX_STATE_TRYING) {

	tsx_set_state( tsx, PJSIP_TSX_STATE_PROCEEDING, 
                       PJSIP_EVENT_TX_MSG, event->body.tx_msg.tdata, 0);

    }

    return status;
}


/*
 * Handler for events in Proceeding for UAS
 * This state happens after the TU sends provisional response.
 */
static pj_status_t tsx_on_state_proceeding_uas( pjsip_transaction *tsx,
                                                pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_PROCEEDING || 
	      tsx->state == PJSIP_TSX_STATE_TRYING);

    /* This state is only for UAS. */
    pj_assert(tsx->role == PJSIP_ROLE_UAS);

    /* Receive request retransmission. */
    if (event->type == PJSIP_EVENT_RX_MSG) {

        pj_status_t status;

	/* Must have last response sent. */
	PJ_ASSERT_RETURN(tsx->last_tx != NULL, PJ_EBUG);

	/* Send last response */
	if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
	    tsx->transport_flag |= TSX_HAS_PENDING_SEND;
	} else {
	    status = tsx_send_msg(tsx, tsx->last_tx);
	    if (status != PJ_SUCCESS)
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
	PJ_ASSERT_RETURN(msg->type==PJSIP_RESPONSE_MSG, PJSIP_ENOTRESPONSEMSG);

	/* Update last status */
	tsx_set_status_code(tsx, msg->line.status.code, 
			    &msg->line.status.reason);

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
                           PJSIP_EVENT_TX_MSG, tdata, 0 );

	    /* Retransmit provisional response every 1 minute if this is
	     * an INVITE provisional response greater than 100.
	     */
	    if (PJSIP_TSX_1XX_RETRANS_DELAY > 0 && 
		tsx->method.id==PJSIP_INVITE_METHOD && tsx->status_code>100)
	    {

		/* Stop 1xx retransmission timer, if any */
		tsx_cancel_timer(tsx, &tsx->retransmit_timer);

		/* Schedule retransmission */
		tsx->retransmit_count = 0;
		if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
		    tsx->transport_flag |= TSX_HAS_PENDING_RESCHED;
		} else {
		    pj_time_val delay = {PJSIP_TSX_1XX_RETRANS_DELAY, 0};
		    tsx_schedule_timer( tsx, &tsx->retransmit_timer, &delay,
		                        RETRANSMIT_TIMER);
		}
	    }

	} else if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 200)) {

	    /* Stop 1xx retransmission timer, if any */
	    tsx_cancel_timer(tsx, &tsx->retransmit_timer);

	    if (tsx->method.id == PJSIP_INVITE_METHOD && tsx->handle_200resp==0) {

		/* 2xx class message is not saved, because retransmission 
                 * is handled by TU.
		 */
		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                               PJSIP_EVENT_TX_MSG, tdata, 0 );

		/* Transaction is destroyed. */
		//return PJSIP_ETSXDESTROYED;

	    } else {
		pj_time_val timeout;

		if (tsx->method.id == PJSIP_INVITE_METHOD) {
		    tsx->retransmit_count = 0;
		    if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
			tsx->transport_flag |= TSX_HAS_PENDING_RESCHED;
		    } else {
			tsx_schedule_timer( tsx, &tsx->retransmit_timer,
					    &t1_timer_val, RETRANSMIT_TIMER);
		    }
		}

		/* Save last response sent for retransmission when request 
		 * retransmission is received.
		 */
		if (tsx->last_tx != tdata) {
		    tsx->last_tx = tdata;
		    pjsip_tx_data_add_ref(tdata);
		}

		/* Setup timeout timer: */
		
		if (tsx->method.id == PJSIP_INVITE_METHOD) {
		    
		    /* Start Timer H at 64*T1 for INVITE server transaction,
		     * regardless of transport.
		     */
		    timeout = timeout_timer_val;
		    
		} else if (!tsx->is_reliable) {
		    
		    /* For non-INVITE, start timer J at 64*T1 for unreliable
		     * transport.
		     */
		    timeout = timeout_timer_val;
		    
		} else {
		    
		    /* Transaction terminates immediately for non-INVITE when
		     * reliable transport is used.
		     */
		    timeout.sec = timeout.msec = 0;
		}

		lock_timer(tsx);
		tsx_cancel_timer(tsx, &tsx->timeout_timer);
		tsx_schedule_timer( tsx, &tsx->timeout_timer,
                                    &timeout, TIMEOUT_TIMER);
		unlock_timer(tsx);

		/* Set state to "Completed" */
		tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                               PJSIP_EVENT_TX_MSG, tdata, 0 );
	    }

	} else if (tsx->status_code >= 300) {

	    /* Stop 1xx retransmission timer, if any */
	    tsx_cancel_timer(tsx, &tsx->retransmit_timer);

	    /* 3xx-6xx class message causes transaction to move to 
             * "Completed" state. 
             */
	    if (tsx->last_tx != tdata) {
		tsx->last_tx = tdata;
		pjsip_tx_data_add_ref( tdata );
	    }

	    /* For INVITE, start timer H for transaction termination 
	     * regardless whether transport is reliable or not.
	     * For non-INVITE, start timer J with the value of 64*T1 for
	     * non-reliable transports, and zero for reliable transports.
	     */
	    lock_timer(tsx);
	    tsx_cancel_timer(tsx, &tsx->timeout_timer);
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {
		/* Start timer H for INVITE */
		tsx_schedule_timer(tsx, &tsx->timeout_timer,
				   &timeout_timer_val, TIMEOUT_TIMER);
	    } else if (!tsx->is_reliable) {
		/* Start timer J on 64*T1 seconds for non-INVITE */
		tsx_schedule_timer(tsx, &tsx->timeout_timer,
				   &timeout_timer_val, TIMEOUT_TIMER);
	    } else {
		/* Start timer J on zero seconds for non-INVITE */
		pj_time_val zero_time = { 0, 0 };
		tsx_schedule_timer(tsx, &tsx->timeout_timer,
				   &zero_time, TIMEOUT_TIMER);
	    }
	    unlock_timer(tsx);

	    /* For INVITE, if unreliable transport is used, retransmission 
	     * timer G will be scheduled (retransmission).
	     */
	    if (!tsx->is_reliable) {
		pjsip_cseq_hdr *cseq = (pjsip_cseq_hdr*)
				       pjsip_msg_find_hdr( msg, PJSIP_H_CSEQ,
                                                           NULL);
		if (cseq->method.id == PJSIP_INVITE_METHOD) {
		    tsx->retransmit_count = 0;
		    if (tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) {
			tsx->transport_flag |= TSX_HAS_PENDING_RESCHED;
		    } else {
			tsx_schedule_timer(tsx, &tsx->retransmit_timer,
					   &t1_timer_val, RETRANSMIT_TIMER);
		    }
		}
	    }

	    /* Inform TU */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                           PJSIP_EVENT_TX_MSG, tdata, 0 );

	} else {
	    pj_assert(0);
	}


    } else if (event->type == PJSIP_EVENT_TIMER && 
	       event->body.timer.entry == &tsx->retransmit_timer) {

	/* Retransmission timer elapsed. */
        pj_status_t status;

	/* Must not be triggered while transport is pending. */
	pj_assert((tsx->transport_flag & TSX_HAS_PENDING_TRANSPORT) == 0);

	/* Must have last response to retransmit. */
	pj_assert(tsx->last_tx != NULL);

	/* Retransmit the last response. */
        status = tsx_retransmit( tsx, 1 );
	if (status != PJ_SUCCESS) {
	    return status;
	}

    } else if (event->type == PJSIP_EVENT_TIMER && 
	       event->body.timer.entry == &tsx->timeout_timer) {

	/* Timeout timer. should not happen? */
	pj_assert(!"Should not happen(?)");

	tsx_set_status_code(tsx, PJSIP_SC_TSX_TIMEOUT, NULL);

	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED,
                       PJSIP_EVENT_TIMER, &tsx->timeout_timer, 0);

	return PJ_EBUG;

    } else {
	pj_assert(!"Unexpected event");
        return PJ_EBUG;
    }

    return PJ_SUCCESS;
}


/*
 * Handler for events in Proceeding for UAC
 * This state happens after provisional response(s) has been received from
 * UAS.
 */
static pj_status_t tsx_on_state_proceeding_uac(pjsip_transaction *tsx, 
                                               pjsip_event *event)
{

    pj_assert(tsx->state == PJSIP_TSX_STATE_PROCEEDING || 
	      tsx->state == PJSIP_TSX_STATE_CALLING);

    if (event->type != PJSIP_EVENT_TIMER) {
	pjsip_msg *msg;

	/* Must be incoming response, because we should not retransmit
	 * request once response has been received.
	 */
	pj_assert(event->type == PJSIP_EVENT_RX_MSG);
	if (event->type != PJSIP_EVENT_RX_MSG) {
	    return PJ_EINVALIDOP;
	}

	msg = event->body.rx_msg.rdata->msg_info.msg;

	/* Must be a response message. */
	if (msg->type != PJSIP_RESPONSE_MSG) {
	    pj_assert(!"Expecting response message!");
	    return PJSIP_ENOTRESPONSEMSG;
	}

	tsx_set_status_code(tsx, msg->line.status.code, 
			    &msg->line.status.reason);

    } else {
	if (event->body.timer.entry == &tsx->retransmit_timer) {
	    /* Retransmit message. */
            pj_status_t status;

            status = tsx_retransmit( tsx, 1 );
	    
	    return status;

	} else {
	    tsx_set_status_code(tsx, PJSIP_SC_TSX_TIMEOUT, NULL);
	}
    }

    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 100)) {

	/* Inform the message to TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_PROCEEDING, 
                       PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata, 0 );

    } else if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code,200)) {

	/* Stop timeout timer B/F. */
	lock_timer(tsx);
	tsx_cancel_timer( tsx, &tsx->timeout_timer );
	unlock_timer(tsx);

	/* For INVITE, the state moves to Terminated state (because ACK is
	 * handled in TU). For non-INVITE, state moves to Completed.
	 */
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                           PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata, 0 );
	    //return PJSIP_ETSXDESTROYED;

	} else {
	    pj_time_val timeout;

	    /* For unreliable transport, start timer D (for INVITE) or 
	     * timer K for non-INVITE. */
	    if (!tsx->is_reliable) {
		if (tsx->method.id == PJSIP_INVITE_METHOD) {
		    timeout = td_timer_val;
		} else {
		    timeout = t4_timer_val;
		}
	    } else {
		timeout.sec = timeout.msec = 0;
	    }
	    lock_timer(tsx);
	    tsx_schedule_timer( tsx, &tsx->timeout_timer,
				&timeout, TIMEOUT_TIMER);
	    unlock_timer(tsx);

	    /* Cancel retransmission timer */
	    tsx_cancel_timer(tsx, &tsx->retransmit_timer);

	    /* Move state to Completed, inform TU. */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                           PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata, 0 );
	}

    } else if (event->type == PJSIP_EVENT_TIMER &&
	       event->body.timer.entry == &tsx->timeout_timer) {

	/* Inform TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED,
		       PJSIP_EVENT_TIMER, &tsx->timeout_timer, 0);


    } else if (tsx->status_code >= 300 && tsx->status_code <= 699) {


#if 0
	/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
	/*
	 * This is the old code; it's broken for authentication.
	 */
	pj_time_val timeout;
        pj_status_t status;

	/* Stop timer B. */
	tsx_cancel_timer( tsx, &tsx->timeout_timer );

	/* Generate and send ACK for INVITE. */
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    pjsip_tx_data *ack;

	    status = pjsip_endpt_create_ack( tsx->endpt, tsx->last_tx, 
					     event->body.rx_msg.rdata,
					     &ack);
	    if (status != PJ_SUCCESS)
		return status;

	    if (ack != tsx->last_tx) {
		pjsip_tx_data_dec_ref(tsx->last_tx);
		tsx->last_tx = ack;
	    }

            status = tsx_send_msg( tsx, tsx->last_tx);
	    if (status != PJ_SUCCESS) {
		return status;
	    }
	}

	/* Start Timer D with TD/T4 timer if unreliable transport is used. */
	if (!tsx->is_reliable) {
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {
		timeout = td_timer_val;
	    } else {
		timeout = t4_timer_val;
	    }
	} else {
	    timeout.sec = timeout.msec = 0;
	}
	tsx_schedule_timer( tsx, &tsx->timeout_timer, &timeout, TIMEOUT_TIMER);

	/* Inform TU. 
	 * blp: You might be tempted to move this notification before
	 *      sending ACK, but I think you shouldn't. Better set-up
	 *      everything before calling tsx_user's callback to avoid
	 *      mess up.
	 */
	tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
                       PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata );

	/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
#endif

	/* New code, taken from 0.2.9.x branch */
	pj_time_val timeout;
	pjsip_tx_data *ack_tdata = NULL;

	/* Cancel retransmission timer */
	tsx_cancel_timer(tsx, &tsx->retransmit_timer);

	/* Stop timer B. */
	lock_timer(tsx);
	tsx_cancel_timer( tsx, &tsx->timeout_timer );
	unlock_timer(tsx);

	/* Generate and send ACK (for INVITE) */
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    pj_status_t status;

	    status = pjsip_endpt_create_ack( tsx->endpt, tsx->last_tx, 
					     event->body.rx_msg.rdata,
					     &ack_tdata);
	    if (status != PJ_SUCCESS)
		return status;

	    status = tsx_send_msg( tsx, ack_tdata);
	    if (status != PJ_SUCCESS)
		return status;
	}

	/* Inform TU. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_COMPLETED, 
		       PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata, 0);

	/* Generate and send ACK for INVITE. */
	if (tsx->method.id == PJSIP_INVITE_METHOD) {
	    if (ack_tdata != tsx->last_tx) {
		pjsip_tx_data_dec_ref(tsx->last_tx);
		tsx->last_tx = ack_tdata;

		/* This is a bug.
		   tsx_send_msg() does NOT decrement tdata's reference counter,
		   so if we add the reference counter here, tdata will have
		   reference counter 2, causing it to leak.
		pjsip_tx_data_add_ref(ack_tdata);
		*/
	    }
	}

	/* Start Timer D with TD/T4 timer if unreliable transport is used. */
	/* Note: tsx->transport may be NULL! */
	if (!tsx->is_reliable) {
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {
		timeout = td_timer_val;
	    } else {
		timeout = t4_timer_val;
	    }
	} else {
	    timeout.sec = timeout.msec = 0;
	}
	lock_timer(tsx);
	/* In the short period above timer may have been inserted
	 * by set_timeout() (by CANCEL). Cancel it if necessary. See:
	 *  https://trac.pjsip.org/repos/ticket/1374
	 */
	tsx_cancel_timer( tsx, &tsx->timeout_timer );
	tsx_schedule_timer( tsx, &tsx->timeout_timer, &timeout,
	                    TIMEOUT_TIMER);
	unlock_timer(tsx);

    } else {
	// Shouldn't happen because there's no timer for this state.
	pj_assert(!"Unexpected event");
        return PJ_EBUG;
    }

    return PJ_SUCCESS;
}


/*
 * Handler for events in Completed state for UAS
 */
static pj_status_t tsx_on_state_completed_uas( pjsip_transaction *tsx, 
                                               pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_COMPLETED);

    if (event->type == PJSIP_EVENT_RX_MSG) {
	pjsip_msg *msg = event->body.rx_msg.rdata->msg_info.msg;

	/* This must be a request message retransmission. */
	if (msg->type != PJSIP_REQUEST_MSG)
	    return PJSIP_ENOTREQUESTMSG;

	/* On receive request retransmission, retransmit last response. */
	if (msg->line.req.method.id != PJSIP_ACK_METHOD) {
            pj_status_t status;

            status = tsx_retransmit( tsx, 0 );
	    if (status != PJ_SUCCESS) {
		return status;
	    }

	} else {
	    pj_time_val timeout;

	    /* Process incoming ACK request. */

	    /* Verify that this is an INVITE transaction */
	    if (tsx->method.id != PJSIP_INVITE_METHOD) {
		PJ_LOG(2, (tsx->obj_name, 
			   "Received illegal ACK for %.*s transaction",
			   (int)tsx->method.name.slen,
			   tsx->method.name.ptr));
		return PJSIP_EINVALIDMETHOD;
	    }

	    /* Cease retransmission. */
	    tsx_cancel_timer(tsx, &tsx->retransmit_timer);

	    tsx->transport_flag &= ~(TSX_HAS_PENDING_RESCHED);

	    /* Reschedule timeout timer. */
	    lock_timer(tsx);
	    tsx_cancel_timer( tsx, &tsx->timeout_timer );

	    /* Timer I is T4 timer for unreliable transports, and
	     * zero seconds for reliable transports.
	     */
	    if (tsx->is_reliable) {
		timeout.sec = 0; 
		timeout.msec = 0;
	    } else {
		timeout.sec = t4_timer_val.sec;
		timeout.msec = t4_timer_val.msec;
	    }
	    tsx_schedule_timer( tsx, &tsx->timeout_timer,
				&timeout, TIMEOUT_TIMER);
	    unlock_timer(tsx);

	    /* Move state to "Confirmed" */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_CONFIRMED, 
                           PJSIP_EVENT_RX_MSG, event->body.rx_msg.rdata, 0 );
	}	

    } else if (event->type == PJSIP_EVENT_TIMER) {

	if (event->body.timer.entry == &tsx->retransmit_timer) {
	    /* Retransmit message. */
            pj_status_t status;

            status = tsx_retransmit( tsx, 1 );
	    if (status != PJ_SUCCESS) {
		return status;
	    }

	} else {
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {

		/* For INVITE, this means that ACK was never received.
		 * Set state to Terminated, and inform TU.
		 */

		tsx_set_status_code(tsx, PJSIP_SC_TSX_TIMEOUT, NULL);

		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                               PJSIP_EVENT_TIMER, &tsx->timeout_timer, 0 );

		//return PJSIP_ETSXDESTROYED;

	    } else {
		/* Transaction terminated, it can now be deleted. */
		tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED, 
                               PJSIP_EVENT_TIMER, &tsx->timeout_timer, 0 );
		//return PJSIP_ETSXDESTROYED;
	    }
	}

    } else {
	PJ_ASSERT_RETURN(event->type == PJSIP_EVENT_TX_MSG, 
			 PJ_EINVALIDOP);
	/* Ignore request to transmit a new message. */
	if (event->body.tx_msg.tdata != tsx->last_tx)
	    return PJ_EINVALIDOP;
    }

    return PJ_SUCCESS;
}


/*
 * Handler for events in Completed state for UAC transaction.
 */
static pj_status_t tsx_on_state_completed_uac( pjsip_transaction *tsx,
                                               pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_COMPLETED);

    if (event->type == PJSIP_EVENT_TIMER) {
	/* Ignore stray retransmit event
	 *  https://trac.pjsip.org/repos/ticket/1766
	 */
	if (event->body.timer.entry != &tsx->timeout_timer)
	    return PJ_SUCCESS;

	/* Move to Terminated state. */
	tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED,
                       PJSIP_EVENT_TIMER, event->body.timer.entry, 0 );

	/* Transaction has been destroyed. */
	//return PJSIP_ETSXDESTROYED;

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

                status = tsx_retransmit( tsx, 0 );
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

    } else {
	pj_assert(!"Unexpected event");
        return PJ_EINVALIDOP;
    }

    return PJ_SUCCESS;
}


/*
 * Handler for events in state Confirmed.
 */
static pj_status_t tsx_on_state_confirmed( pjsip_transaction *tsx,
                                           pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_CONFIRMED);

    /* This state is only for UAS for INVITE. */
    pj_assert(tsx->role == PJSIP_ROLE_UAS);
    pj_assert(tsx->method.id == PJSIP_INVITE_METHOD);

    /* Absorb any ACK received. */
    if (event->type == PJSIP_EVENT_RX_MSG) {

	pjsip_msg *msg = event->body.rx_msg.rdata->msg_info.msg;

	/* Only expecting request message. */
	if (msg->type != PJSIP_REQUEST_MSG)
	    return PJSIP_ENOTREQUESTMSG;

	/* Must be an ACK request or a late INVITE retransmission. */
	pj_assert(msg->line.req.method.id == PJSIP_ACK_METHOD || 
		  msg->line.req.method.id == PJSIP_INVITE_METHOD);

    } else if (event->type == PJSIP_EVENT_TIMER) {
	/* Ignore overlapped retransmit timer.
	 * https://trac.pjsip.org/repos/ticket/1746
	 */
	if (event->body.timer.entry == &tsx->retransmit_timer) {
	    /* Ignore */
	} else {
	    /* Must be from timeout_timer_. */
	    pj_assert(event->body.timer.entry == &tsx->timeout_timer);

	    /* Move to Terminated state. */
	    tsx_set_state( tsx, PJSIP_TSX_STATE_TERMINATED,
			   PJSIP_EVENT_TIMER, &tsx->timeout_timer, 0 );

	    /* Transaction has been destroyed. */
	    //return PJSIP_ETSXDESTROYED;
	}
    } else {
	pj_assert(!"Unexpected event");
        return PJ_EBUG;
    }

    return PJ_SUCCESS;
}


/*
 * Handler for events in state Terminated.
 */
static pj_status_t tsx_on_state_terminated( pjsip_transaction *tsx,
                                            pjsip_event *event)
{
    pj_assert(tsx->state == PJSIP_TSX_STATE_TERMINATED);

    /* Ignore events other than timer. This used to be an assertion but
     * events may genuinely arrive at this state.
     */
    if (event->type != PJSIP_EVENT_TIMER) {
	return PJ_EIGNORED;
    }

    /* Destroy this transaction */
    tsx_set_state(tsx, PJSIP_TSX_STATE_DESTROYED, 
                  event->type, event->body.user.user1, 0 );

    return PJ_SUCCESS;
}


/*
 * Handler for events in state Destroyed.
 * Shouldn't happen!
 */
static pj_status_t tsx_on_state_destroyed(pjsip_transaction *tsx,
                                          pjsip_event *event)
{
    PJ_UNUSED_ARG(tsx);
    PJ_UNUSED_ARG(event);

    // See https://trac.pjsip.org/repos/ticket/1432
    //pj_assert(!"Not expecting any events!!");

    return PJ_EIGNORED;
}


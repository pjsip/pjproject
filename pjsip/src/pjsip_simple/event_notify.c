/* $Header: /pjproject/pjsip/src/pjsip_simple/event_notify.c 11    8/31/05 9:05p Bennylp $ */
#include <pjsip_simple/event_notify.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_misc.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pj/pool.h>
#include <pj/timer.h>
#include <pj/string.h>
#include <pj/hash.h>
#include <pj/os.h>
#include <pj/except.h>
#include <pj/log.h>
#include <pj/guid.h>

#define THIS_FILE		"event_sub"

/* String names for state. 
 * The names here should be compliant with sub_state names in RFC3265.
 */
static const pj_str_t state[] = {
    { "null", 4 }, 
    { "active", 6 },
    { "pending", 7 },
    { "terminated", 10 },
    { "unknown", 7 }
};

/* Timer IDs */
#define TIMER_ID_REFRESH	1
#define TIMER_ID_UAS_EXPIRY	2

/* Static configuration. */
#define SECONDS_BEFORE_EXPIRY	10
#define MGR_POOL_SIZE		512
#define MGR_POOL_INC		0
#define SUB_POOL_SIZE		2048
#define SUB_POOL_INC		0
#define HASH_TABLE_SIZE		32

/* Static vars. */
static int mod_id;
static const pjsip_method SUBSCRIBE = { PJSIP_OTHER_METHOD, {"SUBSCRIBE", 9}};
static const pjsip_method NOTIFY = { PJSIP_OTHER_METHOD, { "NOTIFY", 6}};

typedef struct package
{
    PJ_DECL_LIST_MEMBER(struct package)
    pj_str_t		    event;
    int			    accept_cnt;
    pj_str_t		   *accept;
    pjsip_event_sub_pkg_cb  cb;
} package;

/* Event subscription manager singleton instance. */
static struct pjsip_event_sub_mgr
{
    pj_pool_t		    *pool;
    pj_hash_table_t	    *ht;
    pjsip_endpoint	    *endpt;
    pj_mutex_t		    *mutex;
    pjsip_allow_events_hdr  *allow_events;
    package		     pkg_list;
} mgr;

/* Fordward declarations for static functions. */
static pj_status_t	mod_init(pjsip_endpoint *, pjsip_module *, pj_uint32_t);
static pj_status_t	mod_deinit(pjsip_module*);
static void		tsx_handler(pjsip_module*, pjsip_event*);
static pjsip_event_sub *find_sub(pjsip_rx_data *);
static void		on_subscribe_request(pjsip_transaction*, pjsip_rx_data*);
static void		on_subscribe_response(void *, pjsip_event*);
static void		on_notify_request(pjsip_transaction *, pjsip_rx_data*);
static void		on_notify_response(void *, pjsip_event *);
static void		refresh_timer_cb(pj_timer_heap_t*, pj_timer_entry*);
static void		uas_expire_timer_cb(pj_timer_heap_t*, pj_timer_entry*);
static pj_status_t	send_sub_refresh( pjsip_event_sub *sub );

/* Module descriptor. */
static pjsip_module event_sub_module = 
{
    {"EventSub", 8},			/* Name.		*/
    0,					/* Flag			*/
    128,				/* Priority		*/
    &mgr,				/* User data.		*/
    2,					/* Number of methods supported . */
    { &SUBSCRIBE, &NOTIFY },		/* Array of methods */
    &mod_init,				/* init_module()	*/
    NULL,				/* start_module()	*/
    &mod_deinit,			/* deinit_module()	*/
    &tsx_handler,			/* tsx_handler()	*/
};

/*
 * Module initialization.
 * This will be called by endpoint when it initializes all modules.
 */
static pj_status_t mod_init( pjsip_endpoint *endpt,
			     struct pjsip_module *mod, pj_uint32_t id )
{
    pj_pool_t *pool;

    pool = pjsip_endpt_create_pool(endpt, "esubmgr", MGR_POOL_SIZE, MGR_POOL_INC);
    if (!pool)
	return -1;

    /* Manager initialization: create hash table and mutex. */
    mgr.pool = pool;
    mgr.endpt = endpt;
    mgr.ht = pj_hash_create(pool, HASH_TABLE_SIZE);
    if (!mgr.ht)
	return -1;

    mgr.mutex = pj_mutex_create(pool, "esubmgr", PJ_MUTEX_SIMPLE);
    if (!mgr.mutex)
	return -1;

    /* Attach manager to module. */
    mod->mod_data = &mgr;

    /* Init package list. */
    pj_list_init(&mgr.pkg_list);

    /* Init Allow-Events header. */
    mgr.allow_events = pjsip_allow_events_hdr_create(mgr.pool);

    /* Save the module ID. */
    mod_id = id;

    pjsip_event_notify_init_parser();
    return 0;
}

/*
 * Module deinitialization.
 * Called by endpoint.
 */
static pj_status_t mod_deinit( struct pjsip_module *mod )
{
    pj_mutex_lock(mgr.mutex);
    pj_mutex_destroy(mgr.mutex);
    pjsip_endpt_destroy_pool(mgr.endpt, mgr.pool);
    return 0;
}

/*
 * This public function is called by application to register callback.
 * In exchange, the instance of the module is returned.
 */
PJ_DEF(pjsip_module*) pjsip_event_sub_get_module(void)
{
    return &event_sub_module;
}

/*
 * Register event package.
 */
PJ_DEF(pj_status_t) pjsip_event_sub_register_pkg( const pj_str_t *event,
						  int accept_cnt,
						  const pj_str_t accept[],
						  const pjsip_event_sub_pkg_cb *cb )
{
    package *pkg;
    int i;

    pj_mutex_lock(mgr.mutex);

    /* Create and register new package. */
    pkg = pj_pool_alloc(mgr.pool, sizeof(*pkg));
    pj_strdup(mgr.pool, &pkg->event, event);
    pj_list_insert_before(&mgr.pkg_list, pkg);

    /* Save Accept specification. */
    pkg->accept_cnt = accept_cnt;
    pkg->accept = pj_pool_alloc(mgr.pool, accept_cnt*sizeof(pj_str_t));
    for (i=0; i<accept_cnt; ++i) {
	pj_strdup(mgr.pool, &pkg->accept[i], &accept[i]);
    }

    /* Copy callback. */
    pj_memcpy(&pkg->cb, cb, sizeof(*cb));

    /* Update Allow-Events header. */
    pj_assert(mgr.allow_events->event_cnt < PJSIP_MAX_ALLOW_EVENTS);
    mgr.allow_events->events[mgr.allow_events->event_cnt++] = pkg->event;

    pj_mutex_unlock(mgr.mutex);
    return 0;
}

/*
 * Create subscription key (for hash table).
 */
static void create_subscriber_key( pj_str_t *key, pj_pool_t *pool,
				   pjsip_role_e role, 
				   const pj_str_t *call_id, const pj_str_t *from_tag)
{
    char *p;

    p = key->ptr = pj_pool_alloc(pool, call_id->slen + from_tag->slen + 3);
    *p++ = (role == PJSIP_ROLE_UAS ? 'S' : 'C');
    *p++ = '$';
    pj_memcpy(p, call_id->ptr, call_id->slen);
    p += call_id->slen;
    *p++ = '$';
    pj_memcpy(p, from_tag->ptr, from_tag->slen);
    p += from_tag->slen;

    key->slen = p - key->ptr;
}


/*
 * Create UAC subscription.
 */
PJ_DEF(pjsip_event_sub*) pjsip_event_sub_create( pjsip_endpoint *endpt,
						 const pj_str_t *from,
						 const pj_str_t *to,
						 const pj_str_t *event,
						 int expires,
						 int accept_cnt,
						 const pj_str_t accept[],
						 void *user_data,
						 const pjsip_event_sub_cb *cb)
{
    pjsip_tx_data *tdata;
    pj_pool_t *pool;
    const pjsip_hdr *hdr;
    pjsip_event_sub *sub;
    PJ_USE_EXCEPTION;

    PJ_LOG(5,(THIS_FILE, "Creating event subscription %.*s to %.*s",
			 event->slen, event->ptr, to->slen, to->ptr));

    /* Create pool for the event subscription. */
    pool = pjsip_endpt_create_pool(endpt, "esub", SUB_POOL_SIZE, SUB_POOL_INC);
    if (!pool) {
	return NULL;
    }

    /* Init subscription. */
    sub = pj_pool_calloc(pool, 1, sizeof(*sub));
    sub->pool = pool;
    sub->endpt = endpt;
    sub->role = PJSIP_ROLE_UAC;
    sub->state = PJSIP_EVENT_SUB_STATE_PENDING;
    sub->state_str = state[sub->state];
    sub->user_data = user_data;
    sub->timer.id = 0;
    sub->default_interval = expires;
    pj_memcpy(&sub->cb, cb, sizeof(*cb));
    pj_list_init(&sub->auth_sess);
    pj_list_init(&sub->route_set);
    sub->mutex = pj_mutex_create(pool, "esub", PJ_MUTEX_RECURSE);
    if (!sub->mutex) {
	pjsip_endpt_destroy_pool(endpt, pool);
	return NULL;
    }

    /* The easiest way to parse the parameters is to create a dummy request! */
    tdata = pjsip_endpt_create_request( endpt, &SUBSCRIBE, to, from, to, from,
					NULL, -1, NULL);
    if (!tdata) {
	pj_mutex_destroy(sub->mutex);
	pjsip_endpt_destroy_pool(endpt, pool);
	return NULL;
    }

    /* 
     * Duplicate headers in the request to our structure. 
     */
    PJ_TRY {
	int i;

	/* From */
	hdr = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_FROM, NULL);
	pj_assert(hdr != NULL);
	sub->from = pjsip_hdr_clone(pool, hdr);
        
	/* To */
	hdr = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_TO, NULL);
	pj_assert(hdr != NULL);
	sub->to = pjsip_hdr_clone(pool, hdr);

	/* Contact. */
	sub->contact = pjsip_contact_hdr_create(pool);
	sub->contact->uri = sub->from->uri;

	/* Call-ID */
	hdr = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CALL_ID, NULL);
	pj_assert(hdr != NULL);
	sub->call_id = pjsip_hdr_clone(pool, hdr);

	/* CSeq */
	sub->cseq = pj_rand() % 0xFFFF;

	/* Event. */
	sub->event = pjsip_event_hdr_create(sub->pool);
	pj_strdup(pool, &sub->event->event_type, event);

	/* Expires. */
	sub->uac_expires = pjsip_expires_hdr_create(pool);
	sub->uac_expires->ivalue = expires;

	/* Accept. */
	sub->local_accept = pjsip_accept_hdr_create(pool);
	for (i=0; i<accept_cnt && i < PJSIP_MAX_ACCEPT_COUNT; ++i) {
	    sub->local_accept->count++;
	    pj_strdup(sub->pool, &sub->local_accept->values[i], &accept[i]);
	}

	/* Register to hash table. */
	create_subscriber_key( &sub->key, pool, PJSIP_ROLE_UAC, 
			       &sub->call_id->id, &sub->from->tag);
	pj_mutex_lock( mgr.mutex );
	pj_hash_set( pool, mgr.ht, sub->key.ptr, sub->key.slen, sub);
	pj_mutex_unlock( mgr.mutex );

    }
    PJ_DEFAULT {
	PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): caught exception %d during init", 
			     sub, state[sub->state].ptr, PJ_GET_EXCEPTION()));

	pjsip_tx_data_dec_ref(tdata);
	pj_mutex_destroy(sub->mutex);
	pjsip_endpt_destroy_pool(endpt, sub->pool);
	return NULL;
    }
    PJ_END;

    /* All set, delete temporary transmit data as we don't need it. */
    pjsip_tx_data_dec_ref(tdata);

    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): client created, target=%.*s, event=%.*s",
			 sub, state[sub->state].ptr,
			 to->slen, to->ptr, event->slen, event->ptr));

    return sub;
}

/*
 * Set credentials.
 */
PJ_DEF(pj_status_t) pjsip_event_sub_set_credentials( pjsip_event_sub *sub,
						     int count,
						     const pjsip_cred_info cred[])
{
    pj_mutex_lock(sub->mutex);
    if (count > 0) {
	sub->cred_info = pj_pool_alloc(sub->pool, count*sizeof(pjsip_cred_info));
	pj_memcpy( sub->cred_info, cred, count*sizeof(pjsip_cred_info));
    }
    sub->cred_cnt = count;
    pj_mutex_unlock(sub->mutex);
    return 0;
}

/*
 * Set route-set.
 */
PJ_DEF(pj_status_t) pjsip_event_sub_set_route_set( pjsip_event_sub *sub,
						   const pjsip_route_hdr *route_set )
{
    const pjsip_route_hdr *hdr;

    pj_mutex_lock(sub->mutex);

    /* Clear existing route set. */
    pj_list_init(&sub->route_set);

    /* Duplicate route headers. */
    hdr = route_set->next;
    while (hdr != route_set) {
	pjsip_route_hdr *new_hdr = pjsip_hdr_clone(sub->pool, hdr);
	pj_list_insert_before(&sub->route_set, new_hdr);
	hdr = hdr->next;
    }

    pj_mutex_unlock(sub->mutex);

    return 0;
}

/*
 * Send subscribe request.
 */
PJ_DEF(pj_status_t) pjsip_event_sub_subscribe( pjsip_event_sub *sub )
{
    pj_status_t status;

    pj_mutex_lock(sub->mutex);
    status = send_sub_refresh(sub);
    pj_mutex_unlock(sub->mutex);

    return status;
}

/*
 * Destroy subscription.
 * If there are pending transactions, then this will just set the flag.
 */
PJ_DEF(pj_status_t) pjsip_event_sub_destroy(pjsip_event_sub *sub)
{
    pj_assert(sub != NULL);
    if (sub == NULL)
	return -1;

    /* Application must terminate the subscription first. */
    pj_assert(sub->state == PJSIP_EVENT_SUB_STATE_NULL ||
	      sub->state == PJSIP_EVENT_SUB_STATE_TERMINATED);

    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): about to be destroyed", 
			 sub, state[sub->state].ptr));

    pj_mutex_lock(mgr.mutex);
    pj_mutex_lock(sub->mutex);

    /* Set delete flag. */
    sub->delete_flag = 1;

    /* Unregister timer, if any. */
    if (sub->timer.id != 0) {
	pjsip_endpt_cancel_timer(sub->endpt, &sub->timer);
	sub->timer.id = 0;
    }

    if (sub->pending_tsx > 0) {
	pj_mutex_unlock(sub->mutex);
	pj_mutex_unlock(mgr.mutex);
	PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): has %d pending, will destroy later",
			     sub, state[sub->state].ptr,
			     sub->pending_tsx));
	return 1;
    }

    /* Unregister from hash table. */
    pj_hash_set(sub->pool, mgr.ht, sub->key.ptr, sub->key.slen, NULL);

    /* Destroy. */
    pj_mutex_destroy(sub->mutex);
    pjsip_endpt_destroy_pool(sub->endpt, sub->pool);

    pj_mutex_unlock(mgr.mutex);

    PJ_LOG(4,(THIS_FILE, "event_sub%p: destroyed", sub));
    return 0;
}

/* Change state. */
static void sub_set_state( pjsip_event_sub *sub, int new_state)
{
    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): changed state to %s",
	      sub, state[sub->state].ptr, state[new_state].ptr));
    sub->state = new_state;
    sub->state_str = state[new_state];
}

/*
 * Refresh subscription.
 */
static pj_status_t send_sub_refresh( pjsip_event_sub *sub )
{
    pjsip_tx_data *tdata;
    pj_status_t status;
    const pjsip_route_hdr *route;

    pj_assert(sub->role == PJSIP_ROLE_UAC);
    pj_assert(sub->state != PJSIP_EVENT_SUB_STATE_TERMINATED);
    if (sub->role != PJSIP_ROLE_UAC || 
	sub->state == PJSIP_EVENT_SUB_STATE_TERMINATED)
    {
	return -1;
    }

    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): refreshing subscription", 
			 sub, state[sub->state].ptr));

    /* Create request. */
    tdata = pjsip_endpt_create_request_from_hdr( sub->endpt, 
						 &SUBSCRIBE,
						 sub->to->uri,
						 sub->from, sub->to, 
						 sub->contact, sub->call_id,
						 sub->cseq++,
						 NULL);

    if (!tdata) {
	PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): refresh: unable to create tx data!",
			     sub, state[sub->state].ptr));
	return -1;
    }

    pjsip_msg_add_hdr( tdata->msg, 
		       pjsip_hdr_shallow_clone(tdata->pool, sub->event));
    pjsip_msg_add_hdr( tdata->msg, 
		       pjsip_hdr_shallow_clone(tdata->pool, sub->uac_expires));
    pjsip_msg_add_hdr( tdata->msg, 
		       pjsip_hdr_shallow_clone(tdata->pool, sub->local_accept));
    pjsip_msg_add_hdr( tdata->msg, 
		       pjsip_hdr_shallow_clone(tdata->pool, mgr.allow_events));

    /* Authentication */
    pjsip_auth_init_req( sub->pool, tdata, &sub->auth_sess,
			 sub->cred_cnt, sub->cred_info);

    /* Route set. */
    route = sub->route_set.next;
    while (route != &sub->route_set) {
	pj_list_insert_before( &tdata->msg->hdr,
			       pjsip_hdr_shallow_clone(tdata->pool, route));
	route = route->next;
    }

    /* Send */
    status = pjsip_endpt_send_request( sub->endpt, tdata, -1, sub, 
				       &on_subscribe_response);
    if (status == 0) {
	sub->pending_tsx++;
    } else {
	PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): FAILED to refresh subscription!", 
			     sub, state[sub->state].ptr));
    }

    return status;
}

/*
 * Stop subscription.
 */
PJ_DEF(pj_status_t) pjsip_event_sub_unsubscribe( pjsip_event_sub *sub )
{
    pjsip_tx_data *tdata;
    const pjsip_route_hdr *route;
    pj_status_t status;

    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): unsubscribing...", 
			 sub, state[sub->state].ptr));

    /* Lock subscription. */
    pj_mutex_lock(sub->mutex);

    pj_assert(sub->role == PJSIP_ROLE_UAC);

    /* Kill refresh timer, if any. */
    if (sub->timer.id != 0) {
	sub->timer.id = 0;
	pjsip_endpt_cancel_timer(sub->endpt, &sub->timer);
    }

    /* Create request. */
    tdata = pjsip_endpt_create_request_from_hdr( sub->endpt, 
						 &SUBSCRIBE,
						 sub->to->uri,
						 sub->from, sub->to, 
						 sub->contact, sub->call_id,
						 sub->cseq++,
						 NULL);

    if (!tdata) {
	pj_mutex_unlock(sub->mutex);
	return -1;
    }

    /* Add headers to request. */
    pjsip_msg_add_hdr( tdata->msg, pjsip_hdr_shallow_clone(tdata->pool, sub->event));
    sub->uac_expires->ivalue = 0;
    pjsip_msg_add_hdr( tdata->msg, pjsip_hdr_shallow_clone(tdata->pool, sub->uac_expires));

    /* Add authentication. */
    pjsip_auth_init_req( sub->pool, tdata, &sub->auth_sess,
			 sub->cred_cnt, sub->cred_info);


    /* Route set. */
    route = sub->route_set.next;
    while (route != &sub->route_set) {
	pj_list_insert_before( &tdata->msg->hdr,
			       pjsip_hdr_shallow_clone(tdata->pool, route));
	route = route->next;
    }

    /* Prevent timer from refreshing itself. */
    sub->default_interval = 0;

    /* Set state. */
    sub_set_state( sub, PJSIP_EVENT_SUB_STATE_TERMINATED );

    /* Send the request. */
    status = pjsip_endpt_send_request( sub->endpt, tdata, -1, sub, 
				       &on_subscribe_response);
    if (status == 0) {
	sub->pending_tsx++;
    }

    pj_mutex_unlock(sub->mutex);

    if (status != 0) {
	PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): FAILED to unsubscribe!", 
			     sub, state[sub->state].ptr));
    }

    return status;
}

/*
 * Send notify.
 */
PJ_DEF(pj_status_t) pjsip_event_sub_notify(pjsip_event_sub *sub,
					   pjsip_event_sub_state new_state,
					   const pj_str_t *reason,
					   pjsip_msg_body *body)
{
    pjsip_tx_data *tdata;
    pjsip_sub_state_hdr *ss_hdr;
    const pjsip_route_hdr *route;
    pj_time_val now;
    pj_status_t status;
    pjsip_event_sub_state old_state = sub->state;

    pj_gettimeofday(&now);

    pj_assert(sub->role == PJSIP_ROLE_UAS);
    if (sub->role != PJSIP_ROLE_UAS)
	return -1;

    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): sending NOTIFY", 
			 sub, state[new_state].ptr));

    /* Lock subscription. */
    pj_mutex_lock(sub->mutex);

    /* Can not send NOTIFY if current state is NULL. We can accept TERMINATED. */
    if (sub->state==PJSIP_EVENT_SUB_STATE_NULL) {
	pj_assert(0);
	pj_mutex_unlock(sub->mutex);
	return -1;
    }

    /* Update state no matter what. */
    sub_set_state(sub, new_state);

    /* Create transmit data. */
    tdata = pjsip_endpt_create_request_from_hdr( sub->endpt,
						 &NOTIFY,
						 sub->to->uri,
						 sub->from, sub->to,
						 sub->contact, sub->call_id,
						 sub->cseq++,
						 NULL);
    if (!tdata) {
	pj_mutex_unlock(sub->mutex);
	return -1;
    }

    /* Add Event header. */
    pjsip_msg_add_hdr(tdata->msg, pjsip_hdr_shallow_clone(tdata->pool, sub->event));

    /* Add Subscription-State header. */
    ss_hdr = pjsip_sub_state_hdr_create(tdata->pool);
    ss_hdr->sub_state = state[new_state];
    ss_hdr->expires_param = sub->expiry_time.sec - now.sec;
    if (ss_hdr->expires_param < 0)
	ss_hdr->expires_param = 0;
    if (reason)
	pj_strdup(tdata->pool, &ss_hdr->reason_param, reason);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)ss_hdr);

    /* Add Allow-Events header. */
    pjsip_msg_add_hdr( tdata->msg, 
		       pjsip_hdr_shallow_clone(tdata->pool, mgr.allow_events));

    /* Add authentication */
    pjsip_auth_init_req( sub->pool, tdata, &sub->auth_sess,
			 sub->cred_cnt, sub->cred_info);

    /* Route set. */
    route = sub->route_set.next;
    while (route != &sub->route_set) {
	pj_list_insert_before( &tdata->msg->hdr,
			       pjsip_hdr_shallow_clone(tdata->pool, route));
	route = route->next;
    }

    /* Attach body. */
    tdata->msg->body = body;

    /* That's it, send! */
    status = pjsip_endpt_send_request( sub->endpt, tdata, -1, sub, &on_notify_response);
    if (status == 0)
	sub->pending_tsx++;

    /* If terminated notify application. */
    if (new_state!=old_state && new_state==PJSIP_EVENT_SUB_STATE_TERMINATED) {
	if (sub->cb.on_sub_terminated) {
	    sub->pending_tsx++;
	    (*sub->cb.on_sub_terminated)(sub, reason);
	    sub->pending_tsx--;
	}
    }

    /* Unlock subscription. */
    pj_mutex_unlock(sub->mutex);

    if (status != 0) {
	PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): failed to send NOTIFY", 
			     sub, state[sub->state].ptr));
    }

    if (sub->delete_flag && sub->pending_tsx <= 0) {
	pjsip_event_sub_destroy(sub);
    }
    return status;
}


/* If this timer callback is called, it means subscriber hasn't refreshed its
 * subscription on-time. Set the state to terminated. This will also send
 * NOTIFY with Subscription-State set to terminated.
 */
static void uas_expire_timer_cb( pj_timer_heap_t *timer_heap, pj_timer_entry *entry)
{
    pjsip_event_sub *sub = entry->user_data;
    pj_str_t reason = { "timeout", 7 };

    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): UAS subscription expired!", 
			 sub, state[sub->state].ptr));

    pj_mutex_lock(sub->mutex);
    sub->timer.id = 0;

    if (sub->cb.on_sub_terminated && sub->state!=PJSIP_EVENT_SUB_STATE_TERMINATED) {
	/* Notify application, but prevent app from destroying the sub. */
	++sub->pending_tsx;
	(*sub->cb.on_sub_terminated)(sub, &reason);
	--sub->pending_tsx;
    }
    //pjsip_event_sub_notify( sub, PJSIP_EVENT_SUB_STATE_TERMINATED, 
    //			    &reason, NULL);
    pj_mutex_unlock(sub->mutex);

}

/* Schedule notifier expiration. */
static void sub_schedule_uas_expire( pjsip_event_sub *sub, int sec_delay)
{
    pj_time_val delay = { 0, 0 };
    pj_parsed_time pt;

    if (sub->timer.id != 0)
	pjsip_endpt_cancel_timer(sub->endpt, &sub->timer);

    pj_gettimeofday(&sub->expiry_time);
    sub->expiry_time.sec += sec_delay;

    sub->timer.id = TIMER_ID_UAS_EXPIRY;
    sub->timer.user_data = sub;
    sub->timer.cb = &uas_expire_timer_cb;
    delay.sec = sec_delay;
    pjsip_endpt_schedule_timer( sub->endpt, &sub->timer, &delay);

    pj_time_decode(&sub->expiry_time, &pt);
    PJ_LOG(4,(THIS_FILE, 
	      "event_sub%p (%s)(UAS): will expire at %02d:%02d:%02d (in %d secs)",
	      sub, state[sub->state].ptr, pt.hour, pt.min, pt.sec, sec_delay));
}

/* This timer is called for UAC to refresh the subscription. */
static void refresh_timer_cb( pj_timer_heap_t *timer_heap, pj_timer_entry *entry)
{
    pjsip_event_sub *sub = entry->user_data;

    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): refresh subscription timer", 
			 sub, state[sub->state].ptr));

    pj_mutex_lock(sub->mutex);
    sub->timer.id = 0;
    send_sub_refresh(sub);
    pj_mutex_unlock(sub->mutex);
}


/* This will update the UAC's refresh schedule. */
static void update_next_refresh(pjsip_event_sub *sub, int interval)
{
    pj_time_val delay = {0, 0};
    pj_parsed_time pt;

    if (interval < SECONDS_BEFORE_EXPIRY) {
	PJ_LOG(4,(THIS_FILE, 
		  "event_sub%p (%s): expiration delay too short (%d sec)! updated.",
		  sub, state[sub->state].ptr, interval));
	interval = SECONDS_BEFORE_EXPIRY;
    }

    if (sub->timer.id != 0)
	pjsip_endpt_cancel_timer(sub->endpt, &sub->timer);

    sub->timer.id = TIMER_ID_REFRESH;
    sub->timer.user_data = sub;
    sub->timer.cb = &refresh_timer_cb;
    pj_gettimeofday(&sub->expiry_time);
    delay.sec = interval - SECONDS_BEFORE_EXPIRY;
    sub->expiry_time.sec += delay.sec;

    pj_time_decode(&sub->expiry_time, &pt);
    PJ_LOG(4,(THIS_FILE, 
	      "event_sub%p (%s): will send SUBSCRIBE at %02d:%02d:%02d (in %d secs)",
	      sub, state[sub->state].ptr, 
	      pt.hour, pt.min, pt.sec,
	      delay.sec));

    pjsip_endpt_schedule_timer( sub->endpt, &sub->timer, &delay );
}


/* Find subscription in the hash table. 
 * If found, lock the subscription before returning to caller.
 */
static pjsip_event_sub *find_sub(pjsip_rx_data *rdata)
{
    pj_str_t key;
    pjsip_role_e role;
    pjsip_event_sub *sub;
    pjsip_method *method = &rdata->msg->line.req.method;
    pj_str_t *tag;

    if (rdata->msg->type == PJSIP_REQUEST_MSG) {
	if (pjsip_method_cmp(method, &SUBSCRIBE)==0) {
	    role = PJSIP_ROLE_UAS;
	    tag = &rdata->to_tag;
	} else {
	    pj_assert(pjsip_method_cmp(method, &NOTIFY) == 0);
	    role = PJSIP_ROLE_UAC;
	    tag = &rdata->to_tag;
	}
    } else {
	if (pjsip_method_cmp(&rdata->cseq->method, &SUBSCRIBE)==0) {
	    role = PJSIP_ROLE_UAC;
	    tag = &rdata->from_tag;
	} else {
	    pj_assert(pjsip_method_cmp(method, &NOTIFY) == 0);
	    role = PJSIP_ROLE_UAS;
	    tag = &rdata->from_tag;
	}
    }
    create_subscriber_key( &key, rdata->pool, role, &rdata->call_id, tag);

    pj_mutex_lock(mgr.mutex);
    sub = pj_hash_get(mgr.ht, key.ptr, key.slen);
    if (sub)
	pj_mutex_lock(sub->mutex);
    pj_mutex_unlock(mgr.mutex);

    return sub;
}


/* This function is called when we receive SUBSCRIBE request message 
 * to refresh existing subscription.
 */
static void on_received_sub_refresh( pjsip_event_sub *sub, 
				     pjsip_transaction *tsx, pjsip_rx_data *rdata)
{
    pjsip_event_hdr *e;
    pjsip_expires_hdr *expires;
    pj_str_t hname;
    int status = 200;
    pj_str_t reason_phrase = { NULL, 0 };
    int new_state = sub->state;
    int old_state = sub->state;
    int new_interval = 0;
    pjsip_tx_data *tdata;

    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): received target refresh", 
			 sub, state[sub->state].ptr));

    /* Check that the event matches. */
    hname = pj_str("Event");
    e = pjsip_msg_find_hdr_by_name( rdata->msg, &hname, NULL);
    if (!e) {
	status = 400;
	reason_phrase = pj_str("Missing Event header");
	goto send_response;
    }
    if (pj_stricmp(&e->event_type, &sub->event->event_type) != 0 ||
	pj_stricmp(&e->id_param, &sub->event->id_param) != 0)
    {
	status = 481;
	reason_phrase = pj_str("Subscription does not exist");
	goto send_response;
    }

    /* Check server state. */
    if (sub->state == PJSIP_EVENT_SUB_STATE_TERMINATED) {
	status = 481;
	reason_phrase = pj_str("Subscription does not exist");
	goto send_response;
    }

    /* Check expires header. */
    expires = pjsip_msg_find_hdr(rdata->msg, PJSIP_H_EXPIRES, NULL);
    if (!expires) {
	/*
	status = 400;
	reason_phrase = pj_str("Missing Expires header");
	goto send_response;
	*/
	new_interval = sub->default_interval;
    } else {
	/* Check that interval is not too short. 
	 * Note that expires time may be zero (for unsubscription).
	 */
	new_interval = expires->ivalue;
	if (new_interval != 0 && new_interval < SECONDS_BEFORE_EXPIRY) {
	    status = PJSIP_SC_INTERVAL_TOO_BRIEF;
	    goto send_response;
	}
    }

    /* Update interval. */
    sub->default_interval = new_interval;
    pj_gettimeofday(&sub->expiry_time);
    sub->expiry_time.sec += new_interval;

    /* Update timer only if this is not unsubscription. */
    if (new_interval > 0) {
	sub->default_interval = new_interval;
	sub_schedule_uas_expire( sub, new_interval );

	/* Call callback. */
	if (sub->cb.on_received_refresh) {
	    sub->pending_tsx++;
	    (*sub->cb.on_received_refresh)(sub, rdata);
	    sub->pending_tsx--;
	}
    }

send_response:
    tdata = pjsip_endpt_create_response( sub->endpt, rdata, status);
    if (tdata) {
	if (reason_phrase.slen)
	    tdata->msg->line.status.reason = reason_phrase;

	/* Add Expires header. */
	expires = pjsip_expires_hdr_create(tdata->pool);
	expires->ivalue = sub->default_interval;
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)expires);

	if (PJSIP_IS_STATUS_IN_CLASS(status,200)) {
	    pjsip_msg_add_hdr(tdata->msg, 
			      pjsip_hdr_shallow_clone(tdata->pool, mgr.allow_events));
	}
	/* Send down to transaction. */
	pjsip_tsx_on_tx_msg(tsx, tdata);
    }

    if (sub->default_interval==0 || !PJSIP_IS_STATUS_IN_CLASS(status,200)) {
	/* Notify application if sub is terminated. */
	new_state = PJSIP_EVENT_SUB_STATE_TERMINATED;
	sub_set_state(sub, new_state);
	if (new_state!=old_state && sub->cb.on_sub_terminated) {
	    pj_str_t reason = {"", 0};
	    if (reason_phrase.slen) reason = reason_phrase;
	    else reason = *pjsip_get_status_text(status);

	    sub->pending_tsx++;
	    (*sub->cb.on_sub_terminated)(sub, &reason);
	    sub->pending_tsx--;
	}
    }

    pj_mutex_unlock(sub->mutex);

    /* Prefer to call log when we're not holding the mutex. */
    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): sent refresh response %s, status=%d", 
			 sub, state[sub->state].ptr,
			 (tdata ? tdata->obj_name : "null"), status));

    /* Check if application has requested deletion. */
    if (sub->delete_flag && sub->pending_tsx <= 0) {
	pjsip_event_sub_destroy(sub);
    }

}


/* This function is called when we receive SUBSCRIBE request message for 
 * a new subscription.
 */
static void on_new_subscription( pjsip_transaction *tsx, pjsip_rx_data *rdata )
{
    package *pkg;
    pj_pool_t *pool;
    pjsip_event_sub *sub = NULL;
    pj_str_t hname;
    int status = 200;
    pj_str_t reason = { NULL, 0 };
    pjsip_tx_data *tdata;
    pjsip_expires_hdr *expires;
    pjsip_accept_hdr *accept;
    pjsip_event_hdr *evhdr;

    /* Get the Event header. */
    hname = pj_str("Event");
    evhdr = pjsip_msg_find_hdr_by_name(rdata->msg, &hname, NULL);
    if (!evhdr) {
	status = 400;
	reason = pj_str("No Event header in request");
	goto send_response;
    }

    /* Find corresponding package. 
     * We don't lock the manager's mutex since we assume the package list
     * won't change once the application is running!
     */
    pkg = mgr.pkg_list.next;
    while (pkg != &mgr.pkg_list) {
	if (pj_stricmp(&pkg->event, &evhdr->event_type) == 0)
	    break;
	pkg = pkg->next;
    }

    if (pkg == &mgr.pkg_list) {
	/* Event type is not supported by any packages! */
	status = 489;
	reason = pj_str("Bad Event");
	goto send_response;
    }

    /* First check that the Accept specification matches the 
     * package's Accept types.
     */
    accept = pjsip_msg_find_hdr(rdata->msg, PJSIP_H_ACCEPT, NULL);
    if (accept) {
	unsigned i;
	pj_str_t *content_type = NULL;

	for (i=0; i<accept->count && !content_type; ++i) {
	    int j;
	    for (j=0; j<pkg->accept_cnt; ++j) {
		if (pj_stricmp(&accept->values[i], &pkg->accept[j])==0) {
		    content_type = &pkg->accept[j];
		    break;
		}
	    }
	}

	if (!content_type) {
	    status = PJSIP_SC_NOT_ACCEPTABLE_HERE;
	    goto send_response;
	}
    }

    /* Check whether the package wants to accept the subscription. */
    pj_assert(pkg->cb.on_query_subscribe != NULL);
    (*pkg->cb.on_query_subscribe)(rdata, &status);
    if (!PJSIP_IS_STATUS_IN_CLASS(status,200))
	goto send_response;

    /* Create new subscription record. */
    pool = pjsip_endpt_create_pool(tsx->endpt, "esub", 
				   SUB_POOL_SIZE, SUB_POOL_INC);
    if (!pool) {
	status = 500;
	goto send_response;
    }
    sub = pj_pool_calloc(pool, 1, sizeof(*sub));
    sub->pool = pool;
    sub->mutex = pj_mutex_create(pool, "esub", PJ_MUTEX_RECURSE);
    if (!sub->mutex) {
	status = 500;
	goto send_response;
    }

    PJ_LOG(4,(THIS_FILE, "event_sub%p: notifier is created.", sub));

    /* Start locking mutex. */
    pj_mutex_lock(sub->mutex);

    /* Init UAS subscription */
    sub->endpt = tsx->endpt;
    sub->role = PJSIP_ROLE_UAS;
    sub->state = PJSIP_EVENT_SUB_STATE_PENDING;
    sub->state_str = state[sub->state];
    pj_list_init(&sub->auth_sess);
    pj_list_init(&sub->route_set);
    sub->from = pjsip_hdr_clone(pool, rdata->to);
    pjsip_fromto_set_from(sub->from);
    if (sub->from->tag.slen == 0) {
	pj_create_unique_string(pool, &sub->from->tag);
	rdata->to->tag = sub->from->tag;
    }
    sub->to = pjsip_hdr_clone(pool, rdata->from);
    pjsip_fromto_set_to(sub->to);
    sub->contact = pjsip_contact_hdr_create(pool);
    sub->contact->uri = sub->from->uri;
    sub->call_id = pjsip_cid_hdr_create(pool);
    pj_strdup(pool, &sub->call_id->id, &rdata->call_id);
    sub->cseq = pj_rand() % 0xFFFF;
    
    expires = pjsip_msg_find_hdr( rdata->msg, PJSIP_H_EXPIRES, NULL);
    if (expires) {
	sub->default_interval = expires->ivalue;
	if (sub->default_interval > 0 && 
	    sub->default_interval < SECONDS_BEFORE_EXPIRY) 
	{
	    status = 423; /* Interval too short. */
	    goto send_response;
	}
    } else {
	sub->default_interval = 600;
    }

    /* Clone Event header. */
    sub->event = pjsip_hdr_clone(pool, evhdr);

    /* Register to hash table. */
    create_subscriber_key(&sub->key, pool, PJSIP_ROLE_UAS, &sub->call_id->id,
			  &sub->from->tag);
    pj_mutex_lock(mgr.mutex);
    pj_hash_set(pool, mgr.ht, sub->key.ptr, sub->key.slen, sub);
    pj_mutex_unlock(mgr.mutex);

    /* Set timer where subscription will expire only when expires<>0. 
     * Subscriber may send new subscription with expires==0.
     */
    if (sub->default_interval != 0) {
	sub_schedule_uas_expire( sub, sub->default_interval-SECONDS_BEFORE_EXPIRY);
    }

    /* Notify application. */
    if (pkg->cb.on_subscribe) {
	pjsip_event_sub_cb *cb = NULL;
	sub->pending_tsx++;
	(*pkg->cb.on_subscribe)(sub, rdata, &cb, &sub->default_interval);
	sub->pending_tsx--;
	if (cb == NULL)
	    pj_memset(&sub->cb, 0, sizeof(*cb));
	else
	    pj_memcpy(&sub->cb, cb, sizeof(*cb));
    }


send_response:
    PJ_LOG(4,(THIS_FILE, "event_sub%p (%s)(UAS): status=%d", 
			  sub, state[sub->state].ptr, status));

    tdata = pjsip_endpt_create_response( tsx->endpt, rdata, status);
    if (tdata) {
	if (reason.slen) {
	    /* Customize reason text. */
	    tdata->msg->line.status.reason = reason;
	}
	if (PJSIP_IS_STATUS_IN_CLASS(status,200)) {
	    /* Add Expires header. */
	    pjsip_expires_hdr *hdr;

	    hdr = pjsip_expires_hdr_create(tdata->pool);
	    hdr->ivalue = sub->default_interval;
	    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)hdr );
	}
	if (status == 423) {
	    /* Add Min-Expires header. */
	    pjsip_min_expires_hdr *hdr;

	    hdr = pjsip_min_expires_hdr_create(tdata->pool);
	    hdr->ivalue = SECONDS_BEFORE_EXPIRY;
	    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)hdr);
	}
	if (status == 489 || 
	    status==PJSIP_SC_NOT_ACCEPTABLE_HERE ||
	    PJSIP_IS_STATUS_IN_CLASS(status,200)) 
	{
	    /* Add Allow-Events header. */
	    pjsip_hdr *hdr;
	    hdr = pjsip_hdr_shallow_clone(tdata->pool, mgr.allow_events);
	    pjsip_msg_add_hdr(tdata->msg, hdr);

	    /* Should add Accept header?. */
	}

	pjsip_tsx_on_tx_msg(tsx, tdata);
    }

    /* If received new subscription with expires=0, terminate. */
    if (sub && sub->default_interval == 0) {
	pj_assert(sub->state == PJSIP_EVENT_SUB_STATE_TERMINATED);
	if (sub->cb.on_sub_terminated) {
	    pj_str_t reason = { "timeout", 7 };
	    (*sub->cb.on_sub_terminated)(sub, &reason);
	}
    }

    if (!PJSIP_IS_STATUS_IN_CLASS(status,200) || (sub && sub->delete_flag)) {
	if (sub && sub->mutex) {
	    pjsip_event_sub_destroy(sub);
	} else if (sub) {
	    pjsip_endpt_destroy_pool(tsx->endpt, sub->pool);
	}
    } else {
	pj_assert(status >= 200);
	pj_mutex_unlock(sub->mutex);
    }
}

/* This is the main callback when SUBSCRIBE request is received. */
static void on_subscribe_request(pjsip_transaction *tsx, pjsip_rx_data *rdata)
{
    pjsip_event_sub *sub = find_sub(rdata);

    if (sub)
	on_received_sub_refresh(sub, tsx, rdata);
    else
	on_new_subscription(tsx, rdata);
}


/* This callback is called when response to SUBSCRIBE is received. */
static void on_subscribe_response(void *token, pjsip_event *event)
{
    pjsip_event_sub *sub = token;
    pjsip_transaction *tsx = event->obj.tsx;
    int new_state, old_state = sub->state;

    pj_assert(tsx->status_code >= 200);
    if (tsx->status_code < 200)
	return;

    pj_assert(sub->role == PJSIP_ROLE_UAC);

    /* Lock mutex. */
    pj_mutex_lock(sub->mutex);

    /* If request failed with 401/407 error, silently retry the request. */
    if (tsx->status_code==401 || tsx->status_code==407) {
	pjsip_tx_data *tdata;
	tdata = pjsip_auth_reinit_req(sub->endpt,
				      sub->pool, &sub->auth_sess,
				      sub->cred_cnt, sub->cred_info,
				      tsx->last_tx, event->src.rdata );
	if (tdata) {
	    int status;
	    pjsip_cseq_hdr *cseq;
	    cseq = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
	    cseq->cseq = sub->cseq++;
	    status = pjsip_endpt_send_request( sub->endpt, tdata, 
					       -1, sub, 
					       &on_subscribe_response);
	    if (status == 0) {
		pj_mutex_unlock(sub->mutex);
		return;
	    }
	}
    }

    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code,200)) {
	/* Update To tag. */
	if (sub->to->tag.slen == 0)
	    pj_strdup(sub->pool, &sub->to->tag, &event->src.rdata->to_tag);

	new_state = sub->state;

    } else if (tsx->status_code == 481) {
	new_state = PJSIP_EVENT_SUB_STATE_TERMINATED;

    } else if (tsx->status_code >= 300) {
	/* RFC 3265 Section 3.1.4.2:
         * If a SUBSCRIBE request to refresh a subscription fails 
	 * with a non-481 response, the original subscription is still 
	 * considered valid for the duration of original exires.
	 *
	 * Note:
	 * Since we normally send SUBSCRIBE for refreshing the subscription,
	 * it means the subscription already expired anyway. So we terminate
	 * the subscription now.
	 */
	if (sub->state != PJSIP_EVENT_SUB_STATE_ACTIVE) {
	    new_state = PJSIP_EVENT_SUB_STATE_TERMINATED;
	} else {
	    /* Use this to be compliant with Section 3.1.4.2
	      new_state = sub->state;
	     */
	    new_state = PJSIP_EVENT_SUB_STATE_TERMINATED;
	}
    } else {
	pj_assert(0);
	new_state = sub->state;
    }

    if (new_state != sub->state && sub->state != PJSIP_EVENT_SUB_STATE_TERMINATED) {
	sub_set_state(sub, new_state);
    }

    if (sub->state == PJSIP_EVENT_SUB_STATE_ACTIVE ||
	sub->state == PJSIP_EVENT_SUB_STATE_PENDING)
    {
	/*
	 * Register timer for next subscription refresh, but only when
	 * we're not unsubscribing. Also update default_interval and Expires
	 * header.
	 */
	if (sub->default_interval > 0 && !sub->delete_flag) {
	    pjsip_expires_hdr *exp = NULL;
	    
	    /* Could be transaction timeout. */
	    if (event->src_type == PJSIP_EVENT_RX_MSG) {
		exp = pjsip_msg_find_hdr(event->src.rdata->msg,
					 PJSIP_H_EXPIRES, NULL);
	    }

	    if (exp) {
		int delay = exp->ivalue;
		if (delay > 0) {
		    pj_time_val new_expiry;
		    pj_gettimeofday(&new_expiry);
		    new_expiry.sec += delay;
		    if (sub->timer.id==0 || 
			new_expiry.sec < sub->expiry_time.sec-SECONDS_BEFORE_EXPIRY/2) 
		    {
		    //if (delay > 0 && delay < sub->default_interval) {
			sub->default_interval = delay;
			sub->uac_expires->ivalue = delay;
			update_next_refresh(sub, delay);
		    }
		}
	    }
	}
    }

    /* Call callback. */
    if (!sub->delete_flag) {
	if (sub->cb.on_received_sub_response) {
	    (*sub->cb.on_received_sub_response)(sub, event);
	}
    }

    /* Notify application if we're terminated. */
    if (new_state!=old_state && new_state==PJSIP_EVENT_SUB_STATE_TERMINATED) {
	if (sub->cb.on_sub_terminated) {
	    pj_str_t reason;
	    if (event->src_type == PJSIP_EVENT_RX_MSG)
		reason = event->src.rdata->msg->line.status.reason;
	    else
		reason = *pjsip_get_status_text(tsx->status_code);

	    (*sub->cb.on_sub_terminated)(sub, &reason);
	}
    }

    /* Decrement pending tsx count. */
    --sub->pending_tsx;
    pj_assert(sub->pending_tsx >= 0);

    if (sub->delete_flag && sub->pending_tsx <= 0) {
	pjsip_event_sub_destroy(sub);
    } else {
	pj_mutex_unlock(sub->mutex);
    }

    /* DO NOT ACCESS sub FROM NOW ON! IT MIGHT HAVE BEEN DELETED */
}

/*
 * This callback called when we receive incoming NOTIFY request.
 */
static void on_notify_request(pjsip_transaction *tsx, pjsip_rx_data *rdata)
{
    pjsip_event_sub *sub;
    pjsip_tx_data *tdata;
    int status = 200;
    int old_state;
    pj_str_t reason = { NULL, 0 };
    pj_str_t reason_phrase = { NULL, 0 };
    int new_state = PJSIP_EVENT_SUB_STATE_NULL;

    /* Find subscription based on Call-ID and From tag. 
     * This will also automatically lock the subscription, if it's found.
     */
    sub = find_sub(rdata);
    if (!sub) {
	/* RFC 3265: Section 3.2 Description of NOTIFY Behavior:
	 * Answer with 481 Subscription does not exist.
	 */
	PJ_LOG(4,(THIS_FILE, "Unable to find subscription for incoming NOTIFY!"));
	status = 481;
	reason_phrase = pj_str("Subscription does not exist");

    } else {
	pj_assert(sub->role == PJSIP_ROLE_UAC);
	PJ_LOG(4,(THIS_FILE, "event_sub%p (%s): received NOTIFY", 
			     sub, state[sub->state].ptr));

    }

    new_state = old_state = sub->state;

    /* RFC 3265: Section 3.2.1
     * Check that the Event header match the subscription. 
     */
    if (status == 200) {
	pjsip_event_hdr *hdr;
	pj_str_t hname = { "Event", 5 };

	hdr = pjsip_msg_find_hdr_by_name(rdata->msg, &hname, NULL);
	if (!hdr) {
	    status = PJSIP_SC_BAD_REQUEST;
	    reason_phrase = pj_str("No Event header found");
	} else if (pj_stricmp(&hdr->event_type, &sub->event->event_type) != 0 ||
		   pj_stricmp(&hdr->id_param, &sub->event->id_param) != 0) 
	{
	    status = 481;
	    reason_phrase = pj_str("Subscription does not exist");
	}
    }

    /* Update subscription state and timer. */
    if (status == 200) {
	pjsip_sub_state_hdr *hdr;
	const pj_str_t hname = { "Subscription-State", 18 };
	const pj_str_t state_active = { "active", 6 },
		       state_pending = { "pending", 7},
		       state_terminated = { "terminated", 10 };

	hdr = pjsip_msg_find_hdr_by_name( rdata->msg, &hname, NULL);
	if (!hdr) {
	    status = PJSIP_SC_BAD_REQUEST;
	    reason_phrase = pj_str("No Subscription-State header found");
	    goto process;
	} 

	/*
	 * Update subscription state.
	 */
	if (pj_stricmp(&hdr->sub_state, &state_active) == 0) {
	    if (sub->state != PJSIP_EVENT_SUB_STATE_TERMINATED)
		new_state = PJSIP_EVENT_SUB_STATE_ACTIVE;
	} else if (pj_stricmp(&hdr->sub_state, &state_pending) == 0) {
	    if (sub->state != PJSIP_EVENT_SUB_STATE_TERMINATED)
		new_state = PJSIP_EVENT_SUB_STATE_PENDING;
	} else if (pj_stricmp(&hdr->sub_state, &state_terminated) == 0) {
	    new_state = PJSIP_EVENT_SUB_STATE_TERMINATED;
	} else {
	    new_state = PJSIP_EVENT_SUB_STATE_UNKNOWN;
	}

	reason = hdr->reason_param;

	if (new_state != sub->state && new_state != PJSIP_EVENT_SUB_STATE_NULL &&
	    sub->state != PJSIP_EVENT_SUB_STATE_TERMINATED) 
	{
	    sub_set_state(sub, new_state);
	    if (new_state == PJSIP_EVENT_SUB_STATE_UNKNOWN) {
		pj_strdup_with_null(sub->pool, &sub->state_str, &hdr->sub_state);
	    } else {
		sub->state_str = state[new_state];
	    }
	}

	/*
	 * Update timeout timer in required, just in case notifier changed the 
         * expiration to shorter time.
	 * Section 3.2.2: the expires param can only shorten the interval.
	 */
	if ((sub->state==PJSIP_EVENT_SUB_STATE_ACTIVE || 
	     sub->state==PJSIP_EVENT_SUB_STATE_PENDING) && hdr->expires_param > 0) 
	{
	    pj_time_val now, new_expiry;

	    pj_gettimeofday(&now);
	    new_expiry.sec = now.sec + hdr->expires_param;
	    if (sub->timer.id==0 || 
		new_expiry.sec < sub->expiry_time.sec-SECONDS_BEFORE_EXPIRY/2) 
	    {
		update_next_refresh(sub, hdr->expires_param);
	    }
	}
    }

process:
    /* Note: here we sub MAY BE NULL! */

    /* Send response to NOTIFY */
    tdata = pjsip_endpt_create_response( tsx->endpt, rdata, status );
    if (tdata) {
	if (reason_phrase.slen)
	    tdata->msg->line.status.reason = reason_phrase;

	if (PJSIP_IS_STATUS_IN_CLASS(status,200)) {
	    pjsip_hdr *hdr;
	    hdr = pjsip_hdr_shallow_clone(tdata->pool, mgr.allow_events);
	    pjsip_msg_add_hdr( tdata->msg, hdr);
	}

	pjsip_tsx_on_tx_msg(tsx, tdata);
    }

    /* Call NOTIFY callback, if any. */
    if (sub && PJSIP_IS_STATUS_IN_CLASS(status,200) && sub->cb.on_received_notify) {
	sub->pending_tsx++;
	(*sub->cb.on_received_notify)(sub, rdata);
	sub->pending_tsx--;
    }

    /* Check if subscription is terminated and call callback. */
    if (sub && new_state!=old_state && new_state==PJSIP_EVENT_SUB_STATE_TERMINATED) {
	if (sub->cb.on_sub_terminated) {
	    sub->pending_tsx++;
	    (*sub->cb.on_sub_terminated)(sub, &reason);
	    sub->pending_tsx--;
	}
    }

    /* Check if application has requested deletion. */
    if (sub && sub->delete_flag && sub->pending_tsx <= 0) {
	pjsip_event_sub_destroy(sub);
    } else if (sub) {
	pj_mutex_unlock(sub->mutex);
    }
}

/* This callback is called when we received NOTIFY response. */
static void on_notify_response(void *token, pjsip_event *event)
{
    pjsip_event_sub *sub = token;
    pjsip_event_sub_state old_state = sub->state;
    pjsip_transaction *tsx = event->obj.tsx;

    /* Lock the subscription. */
    pj_mutex_lock(sub->mutex);

    pj_assert(sub->role == PJSIP_ROLE_UAS);

    /* If request failed with authorization failure, silently retry. */
    if (tsx->status_code==401 || tsx->status_code==407) {
	pjsip_tx_data *tdata;
	tdata = pjsip_auth_reinit_req(sub->endpt,
				      sub->pool, &sub->auth_sess,
				      sub->cred_cnt, sub->cred_info,
				      tsx->last_tx, event->src.rdata );
	if (tdata) {
	    int status;
	    pjsip_cseq_hdr *cseq;
	    cseq = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
	    cseq->cseq = sub->cseq++;
	    status = pjsip_endpt_send_request( sub->endpt, tdata, 
					       -1, sub, 
					       &on_notify_response);
	    if (status == 0) {
		pj_mutex_unlock(sub->mutex);
		return;
	    }
	}
    }

    /* Notify application. */
    if (sub->cb.on_received_notify_response)
	(*sub->cb.on_received_notify_response)(sub, event);

    /* Check for response 481. */
    if (event->obj.tsx->status_code == 481) {
	/* Remote says that the subscription does not exist! 
	 * Terminate subscription!
	 */
	sub_set_state(sub, PJSIP_EVENT_SUB_STATE_TERMINATED);
	if (sub->timer.id) {
	    pjsip_endpt_cancel_timer(sub->endpt, &sub->timer);
	    sub->timer.id = 0;
	}

	PJ_LOG(4, (THIS_FILE, 
		   "event_sub%p (%s): got 481 response to NOTIFY. Terminating...",
		   sub, state[sub->state].ptr));

	/* Notify app. */
	if (sub->state!=old_state && sub->cb.on_sub_terminated) 
	    (*sub->cb.on_sub_terminated)(sub, &event->src.rdata->msg->line.status.reason);
    }

    /* Decrement pending transaction count. */
    --sub->pending_tsx;
    pj_assert(sub->pending_tsx >= 0);

    /* Check that the subscription is marked for deletion. */
    if (sub->delete_flag && sub->pending_tsx <= 0) {
	pjsip_event_sub_destroy(sub);
    } else {
	pj_mutex_unlock(sub->mutex);
    }

    /* DO NOT ACCESS sub, IT MIGHT HAVE BEEN DESTROYED! */
}


/* This is the transaction handler for incoming SUBSCRIBE and NOTIFY 
 * requests. 
 */
static void tsx_handler( struct pjsip_module *mod, pjsip_event *event )
{
    pjsip_msg *msg;
    pjsip_rx_data *rdata;

    /* Only want incoming message events. */
    if (event->src_type != PJSIP_EVENT_RX_MSG)
	return;

    rdata = event->src.rdata;
    msg = rdata->msg;

    /* Only want to process request messages. */
    if (msg->type != PJSIP_REQUEST_MSG)
	return;

    /* Only want the first notification. */
    if (event->obj.tsx && event->obj.tsx->status_code >= 100)
	return;

    if (pjsip_method_cmp(&msg->line.req.method, &SUBSCRIBE)==0) {
	/* Process incoming SUBSCRIBE request. */
	on_subscribe_request( event->obj.tsx, rdata );
    } else if (pjsip_method_cmp(&msg->line.req.method, &NOTIFY)==0) {
	/* Process incoming NOTIFY request. */
	on_notify_request( event->obj.tsx, rdata );
    }
}


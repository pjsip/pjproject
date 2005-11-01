/* $Id$
 *
 */
#include <pjsip_mod_ua/sip_dialog.h>
#include <pjsip_mod_ua/sip_ua.h>
#include <pjsip_mod_ua/sip_ua_private.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_types.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_uri.h>
#include <pjsip/sip_misc.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_parser.h>
#include <pj/string.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/guid.h>
#include <pj/except.h>
#include <pj/pool.h>

/* TLS to keep dialog lock record (initialized by UA) */
int pjsip_dlg_lock_tls_id;

struct dialog_lock_data
{
    struct dialog_lock_data *prev;
    pjsip_dlg	    *dlg;
    int			     is_alive;
};

/*
 * Static function prototypes.
 */
static void dlg_create_request_throw( pjsip_tx_data **p_tdata,
				      pjsip_dlg *dlg,
				      const pjsip_method *method,
				      int cseq );
static int  dlg_on_all_state_pre(  pjsip_dlg *dlg, 
				   pjsip_transaction *tsx,
				   pjsip_event *event);
static int  dlg_on_all_state_post( pjsip_dlg *dlg, 
				   pjsip_transaction *tsx,
				   pjsip_event *event);
static int  dlg_on_state_null( pjsip_dlg *dlg, 
			       pjsip_transaction *tsx,
			       pjsip_event *event);
static int  dlg_on_state_incoming( pjsip_dlg *dlg, 
				   pjsip_transaction *tsx,
				   pjsip_event *event);
static int  dlg_on_state_calling( pjsip_dlg *dlg, 
				  pjsip_transaction *tsx,
				  pjsip_event *event);
static int  dlg_on_state_proceeding( pjsip_dlg *dlg, 
				     pjsip_transaction *tsx,
				     pjsip_event *event);
static int  dlg_on_state_proceeding_caller( pjsip_dlg *dlg, 
					    pjsip_transaction *tsx,
					    pjsip_event *event);
static int  dlg_on_state_proceeding_callee( pjsip_dlg *dlg, 
					    pjsip_transaction *tsx,
					    pjsip_event *event);
static int  dlg_on_state_connecting( pjsip_dlg *dlg, 
				     pjsip_transaction *tsx,
				     pjsip_event *event);
static int  dlg_on_state_established( pjsip_dlg *dlg, 
				      pjsip_transaction *tsx,
				      pjsip_event *event);
static int  dlg_on_state_disconnected( pjsip_dlg *dlg, 
				       pjsip_transaction *tsx,
				       pjsip_event *event);
static int  dlg_on_state_terminated( pjsip_dlg *dlg, 
				     pjsip_transaction *tsx,
				     pjsip_event *event);

/*
 * Dialog state handlers.
 */
static int  (*dlg_state_handlers[])(struct pjsip_dlg *, pjsip_transaction *,
				    pjsip_event *) = 
{
    &dlg_on_state_null,
    &dlg_on_state_incoming,
    &dlg_on_state_calling,
    &dlg_on_state_proceeding,
    &dlg_on_state_connecting,
    &dlg_on_state_established,
    &dlg_on_state_disconnected,
    &dlg_on_state_terminated,
};

/*
 * Dialog state names.
 */
static const char* dlg_state_names[] = 
{
    "STATE_NULL",
    "STATE_INCOMING",
    "STATE_CALLING",
    "STATE_PROCEEDING",
    "STATE_CONNECTING",
    "STATE_ESTABLISHED",
    "STATE_DISCONNECTED",
    "STATE_TERMINATED",
};


/*
 * Get the dialog string state, normally for logging purpose.
 */
const char *pjsip_dlg_state_str(pjsip_dlg_state_e state)
{
    return dlg_state_names[state];
}

/* Lock dialog mutex. */
static void lock_dialog(pjsip_dlg *dlg, struct dialog_lock_data *lck)
{
    struct dialog_lock_data *prev;

    pj_mutex_lock(dlg->mutex);
    prev = pj_thread_local_get(pjsip_dlg_lock_tls_id);
    lck->prev = prev;
    lck->dlg = dlg;
    lck->is_alive = 1;
    pj_thread_local_set(pjsip_dlg_lock_tls_id, lck);
}

/* Carefully unlock dialog mutex, protect against situation when the dialog
 * has already been destroyed.
 */
static pj_status_t unlock_dialog(pjsip_dlg *dlg, struct dialog_lock_data *lck)
{
    pj_assert(pj_thread_local_get(pjsip_dlg_lock_tls_id) == lck);
    pj_assert(dlg == lck->dlg);

    pj_thread_local_set(pjsip_dlg_lock_tls_id, lck->prev);
    if (lck->is_alive)
	pj_mutex_unlock(dlg->mutex);

    return lck->is_alive ? 0 : -1;
}

/*
 * This is called by dialog's FSM to change dialog's state.
 */
static void dlg_set_state( pjsip_dlg *dlg, pjsip_dlg_state_e state,
			   pjsip_event *event)
{
    PJ_UNUSED_ARG(event);

    PJ_LOG(4, (dlg->obj_name, "State %s-->%s (ev=%s, src=%s, data=%p)", 
	       pjsip_dlg_state_str(dlg->state), pjsip_dlg_state_str(state),
	       event ? pjsip_event_str(event->type) : "", 
	       event ? pjsip_event_str(event->src_type) : "",
	       event ? event->src.data : NULL));

    dlg->state = state;
    dlg->handle_tsx_event = dlg_state_handlers[state];
}

/*
 * Invoke dialog's callback.
 * This function is called by dialog's FSM, and interpret the event and call
 * the corresponding callback registered by application.
 */
static void dlg_call_dlg_callback( pjsip_dlg *dlg, pjsip_dlg_event_e dlg_event,
				   pjsip_event *event )
{
    pjsip_dlg_callback *cb = dlg->ua->dlg_cb;
    if (!cb) {
	PJ_LOG(4,(dlg->obj_name, "Can not call callback (none registered)."));
	return;
    }

    /* Low level event: call the all-events callback. */
    if (cb->on_all_events) {
	(*cb->on_all_events)(dlg, dlg_event, event);
    }

    /* Low level event: call the tx/rx callback if this is a tx/rx event. */
    if (event->type == PJSIP_EVENT_BEFORE_TX && cb->on_before_tx)
    {
	(*cb->on_before_tx)(dlg, event->obj.tsx, event->src.tdata, event->data.long_data);
    }
    else if (event->type == PJSIP_EVENT_TX_MSG && 
	event->src_type == PJSIP_EVENT_TX_MSG && cb->on_tx_msg) 
    {
	(*cb->on_tx_msg)(dlg, event->obj.tsx, event->src.tdata);
    } 
    else if (event->type == PJSIP_EVENT_RX_MSG &&
	     event->src_type == PJSIP_EVENT_RX_MSG && cb->on_rx_msg) {
	(*cb->on_rx_msg)(dlg, event->obj.tsx, event->src.rdata);
    }

    /* Now trigger high level events. 
     * High level event should only occurs when dialog's state has changed,
     * except for on_provisional, which may be called multiple times whenever
     * response message is sent
     */
    if (dlg->state == PJSIP_DIALOG_STATE_PROCEEDING &&
	(event->type== PJSIP_EVENT_TSX_STATE_CHANGED) && 
	event->obj.tsx == dlg->invite_tsx) 
    {
	/* Sent/received provisional responses. */
	if (cb->on_provisional)
	    (*cb->on_provisional)(dlg, event->obj.tsx, event);
    }

    if (dlg_event == PJSIP_DIALOG_EVENT_MID_CALL_REQUEST) {
	if (cb->on_mid_call_events)
	    (*cb->on_mid_call_events)(dlg, event);
	return;
    }

    if (dlg_event != PJSIP_DIALOG_EVENT_STATE_CHANGED)
	return;

    if (dlg->state == PJSIP_DIALOG_STATE_INCOMING) {

	/* New incoming dialog. */
	pj_assert (event->src_type == PJSIP_EVENT_RX_MSG);
	(*cb->on_incoming)(dlg, event->obj.tsx, event->src.rdata);

    } else if (dlg->state == PJSIP_DIALOG_STATE_CALLING) {

	/* Dialog has just sent the first INVITE. */
	if (cb->on_calling) {
	    (*cb->on_calling)(dlg, event->obj.tsx, event->src.tdata);
	}

    } else if (dlg->state == PJSIP_DIALOG_STATE_DISCONNECTED) {

	if (cb->on_disconnected)
	    (*cb->on_disconnected)(dlg, event);

    } else if (dlg->state == PJSIP_DIALOG_STATE_TERMINATED) {

	if (cb->on_terminated)
	    (*cb->on_terminated)(dlg);

	pjsip_ua_destroy_dialog(dlg);

    } else if (dlg->state == PJSIP_DIALOG_STATE_CONNECTING) {

	if (cb->on_connecting)
	    (*cb->on_connecting)(dlg, event);

    } else if (dlg->state == PJSIP_DIALOG_STATE_ESTABLISHED) {

	if (cb->on_established)
	    (*cb->on_established)(dlg, event);
    }
}

/*
 * This callback receives event from the transaction layer (via User Agent),
 * or from dialog timer (for 200/INVITE or ACK retransmission).
 */
void pjsip_dlg_on_tsx_event( pjsip_dlg *dlg, 
			     pjsip_transaction *tsx, 
			     pjsip_event *event)
{
    int status = 0;
    struct dialog_lock_data lck;

    PJ_LOG(4, (dlg->obj_name, "state=%s (evt=%s, src=%s)", 
			 pjsip_dlg_state_str(dlg->state),
			 pjsip_event_str(event->type),
			 pjsip_event_str(event->src_type)));


    lock_dialog(dlg, &lck);

    status = dlg_on_all_state_pre( dlg, tsx, event);

    if (status==0) {
	status = (*dlg->handle_tsx_event)(dlg, tsx, event);
    }
    if (status==0) {
	status = dlg_on_all_state_post( dlg, tsx, event);
    }
    
    unlock_dialog(dlg, &lck);
}

/*
 * This function contains common processing in all states, and it is called
 * before the FSM is invoked.
 */
static int  dlg_on_all_state_pre( pjsip_dlg *dlg, 
				  pjsip_transaction *tsx,
				  pjsip_event *event)
{
    PJ_UNUSED_ARG(event)

    if (event->type != PJSIP_EVENT_TSX_STATE_CHANGED)
	return 0;

    if (tsx && (tsx->state==PJSIP_TSX_STATE_CALLING || 
	tsx->state==PJSIP_TSX_STATE_TRYING)) 
    {
	++dlg->pending_tsx_count;

    } else if (tsx && tsx->state==PJSIP_TSX_STATE_DESTROYED) 
    {
	--dlg->pending_tsx_count;
	if (tsx == dlg->invite_tsx)
	    dlg->invite_tsx = NULL;
    }

    if (tsx->method.id == PJSIP_INVITE_METHOD) {
	tsx->handle_ack = 1;
    }
    return 0;
}


/*
 * This function contains common processing in all states, and it is called
 * after the FSM is invoked.
 */
static int  dlg_on_all_state_post( pjsip_dlg *dlg, 
				   pjsip_transaction *tsx,
				   pjsip_event *event)
{
    PJ_UNUSED_ARG(event)

    if (tsx && tsx->state==PJSIP_TSX_STATE_DESTROYED) {
	if (dlg->pending_tsx_count == 0 &&
	    dlg->state != PJSIP_DIALOG_STATE_CONNECTING &&
	    dlg->state != PJSIP_DIALOG_STATE_ESTABLISHED &&
	    dlg->state != PJSIP_DIALOG_STATE_TERMINATED) 
	{
	    dlg_set_state(dlg, PJSIP_DIALOG_STATE_TERMINATED, event);
	    dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);
	    return -1;
	}
    }

    return 0;
}


/*
 * Internal function to initialize dialog, contains common initialization
 * for both UAS and UAC dialog.
 */
static pj_status_t dlg_init( pjsip_dlg *dlg )
{
    /* Init mutex. */
    dlg->mutex = pj_mutex_create(dlg->pool, "mdlg%p", 0);
    if (!dlg->mutex) {
	PJ_PERROR((dlg->obj_name, "pj_mutex_create()"));
	return -1;
    }

    /* Init route-set (Initially empty) */
    pj_list_init(&dlg->route_set);

    /* Init auth credential list. */
    pj_list_init(&dlg->auth_sess);

    return PJ_SUCCESS;
}

/*
 * This one is called just before dialog is destroyed.
 * It is called while mutex is held.
 */
PJ_DEF(void) pjsip_on_dialog_destroyed( pjsip_dlg *dlg )
{
    struct dialog_lock_data *lck;

    //PJ_TODO(CHECK_THIS);
    pj_assert(dlg->pending_tsx_count == 0);

    /* Mark dialog as dead. */
    lck = pj_thread_local_get(pjsip_dlg_lock_tls_id);
    while (lck) {
	if (lck->dlg == dlg)
	    lck->is_alive = 0;
	lck = lck->prev;
    }
}

/*
 * Initialize dialog from the received request.
 * This is an internal function which is called by the User Agent (sip_ua.c), 
 * and it will initialize most of dialog's properties with values from the
 * received message.
 */
pj_status_t pjsip_dlg_init_from_rdata( pjsip_dlg *dlg, pjsip_rx_data *rdata )
{
    pjsip_msg *msg = rdata->msg;
    pjsip_to_hdr *to;
    pjsip_contact_hdr *contact;
    pjsip_name_addr *name_addr;
    pjsip_url *url;
    unsigned flag;
    pjsip_event event;

    pj_assert(dlg && rdata);

    PJ_LOG(5, (dlg->obj_name, "init_from_rdata(%p)", rdata));

    /* Must be an INVITE request. */
    pj_assert(msg->type == PJSIP_REQUEST_MSG && 
	      msg->line.req.method.id == PJSIP_INVITE_METHOD);

    /* Init general dialog data. */
    if (dlg_init(dlg) != PJ_SUCCESS) {
	return -1;
    }

    /* Get the To header. */
    to = rdata->to;

    /* Copy URI in the To header as our local URI. */
    dlg->local.info = pjsip_hdr_clone( dlg->pool, to);

    /* Set tag in the local info. */
    dlg->local.info->tag = dlg->local.tag;

    /* Create local Contact to be advertised in the response.
     * At the moment, just copy URI from the local URI as our contact.
     */
    dlg->local.contact = pjsip_contact_hdr_create( dlg->pool );
    dlg->local.contact->star = 0;
    name_addr = (pjsip_name_addr *)dlg->local.info->uri;
    dlg->local.contact->uri = (pjsip_uri*) name_addr;
    url = (pjsip_url*) name_addr->uri;
    //url->port = rdata->via->sent_by.port;
    //url->port = pj_sockaddr_get_port( pjsip_transport_get_local_addr(rdata->transport) );

    /* Save remote URI. */
    dlg->remote.info = pjsip_hdr_clone( dlg->pool, rdata->from );
    pjsip_fromto_set_to( dlg->remote.info );
    pj_strdup( dlg->pool, &dlg->remote.tag, &rdata->from->tag );

    /* Save remote Contact. */
    contact = pjsip_msg_find_hdr( msg, PJSIP_H_CONTACT, NULL);
    if (contact) {
    	dlg->remote.contact = pjsip_hdr_clone( dlg->pool, contact );
    } else {
	PJ_LOG(3,(dlg->obj_name, "No Contact header in INVITE from %s", 
		  pj_sockaddr_get_str_addr(&rdata->addr)));
   	dlg->remote.contact = pjsip_contact_hdr_create( dlg->pool );
	dlg->remote.contact->uri = dlg->remote.info->uri;
    }

    /* Save Call-ID. */
    dlg->call_id = pjsip_cid_hdr_create(dlg->pool);
    pj_strdup( dlg->pool, &dlg->call_id->id, &rdata->call_id );

    /* Initialize local CSeq and save remote CSeq.*/
    dlg->local.cseq = rdata->timestamp.sec & 0xFFFF;
    dlg->remote.cseq = rdata->cseq->cseq;

    /* Secure? */
    flag = pjsip_transport_get_flag(rdata->transport);
    dlg->secure = (flag & PJSIP_TRANSPORT_SECURE) != 0;

    /* Initial state is NULL. */
    event.type = event.src_type = PJSIP_EVENT_RX_MSG;
    event.src.rdata = rdata;
    dlg_set_state(dlg, PJSIP_DIALOG_STATE_NULL, &event);

    PJ_LOG(5, (dlg->obj_name, "init_from_rdata(%p) complete",  rdata));
    return PJ_SUCCESS;
}

/*
 * Set the contact details.
 */
PJ_DEF(pj_status_t) pjsip_dlg_set_contact( pjsip_dlg *dlg,
					   const pj_str_t *contact )
{
    pjsip_uri *local_uri;
    pj_str_t tmp;

    pj_strdup_with_null(dlg->pool, &tmp, contact);
    local_uri = pjsip_parse_uri( dlg->pool, tmp.ptr, tmp.slen, 
				 PJSIP_PARSE_URI_AS_NAMEADDR);
    if (local_uri == NULL) {
	PJ_LOG(2, (dlg->obj_name, "set_contact: invalid URI"));
	return -1;
    }

    dlg->local.contact->star = 0;
    dlg->local.contact->uri = local_uri;
    return 0;
}

/*
 * Set route set.
 */
PJ_DEF(pj_status_t) pjsip_dlg_set_route_set( pjsip_dlg *dlg,
					     const pjsip_route_hdr *route_set )
{
    pjsip_route_hdr *hdr;

    pj_list_init(&dlg->route_set);
    hdr = route_set->next;
    while (hdr != route_set) {
	pjsip_route_hdr *cloned = pjsip_hdr_clone(dlg->pool, hdr);
	pj_list_insert_before( &dlg->route_set, cloned);
	hdr = hdr->next;
    }
    return 0;
}

/*
 * Set route set without cloning the header.
 */
PJ_DEF(pj_status_t) pjsip_dlg_set_route_set_np( pjsip_dlg *dlg,
						pjsip_route_hdr *route_set)
{
    pjsip_route_hdr *hdr;

    pj_list_init(&dlg->route_set);
    hdr = route_set->next;
    while (hdr != route_set) {
	pj_list_insert_before( &dlg->route_set, hdr);
	hdr = hdr->next;
    }
    return 0;
}

/*
 * Application calls this function when it wants to initiate an outgoing
 * dialog (incoming dialogs are created automatically by UA when it receives
 * INVITE, by calling pjsip_dlg_init_from_rdata()).
 * This function should initialize most of the dialog's properties.
 */
PJ_DEF(pj_status_t) pjsip_dlg_init( pjsip_dlg *dlg,
				    const pj_str_t *c_local_info,
				    const pj_str_t *c_remote_info,
				    const pj_str_t *c_target)
{
    pj_time_val tv;
    pjsip_event event;
    pj_str_t buf;

    if (!dlg || !c_local_info || !c_remote_info) {
	pj_assert(dlg && c_local_info && c_remote_info);
	return -1;
    }

    PJ_LOG(5, (dlg->obj_name, "initializing"));

    /* Init general dialog */
    if (dlg_init(dlg) != PJ_SUCCESS) {
	return -1;
    }

    /* Duplicate local info. */
    pj_strdup_with_null( dlg->pool, &buf, c_local_info);

    /* Build local URI. */
    dlg->local.target = pjsip_parse_uri(dlg->pool, buf.ptr, buf.slen, 
				        PJSIP_PARSE_URI_AS_NAMEADDR);
    if (dlg->local.target == NULL) {
	PJ_LOG(2, (dlg->obj_name, 
		   "pjsip_dlg_init: invalid local URI %s", buf.ptr));
	return -1;
    }

    /* Set local URI. */
    dlg->local.info = pjsip_from_hdr_create(dlg->pool);
    dlg->local.info->uri = dlg->local.target;
    dlg->local.info->tag = dlg->local.tag;

    /* Create local Contact to be advertised in the response. */
    dlg->local.contact = pjsip_contact_hdr_create( dlg->pool );
    dlg->local.contact->star = 0;
    dlg->local.contact->uri = dlg->local.target;

    /* Set remote URI. */
    dlg->remote.info = pjsip_to_hdr_create(dlg->pool);

    /* Duplicate to buffer. */
    pj_strdup_with_null( dlg->pool, &buf, c_remote_info);

    /* Build remote info. */
    dlg->remote.info->uri = pjsip_parse_uri( dlg->pool, buf.ptr, buf.slen,
					     PJSIP_PARSE_URI_AS_NAMEADDR);
    if (dlg->remote.info->uri == NULL) {
	PJ_LOG(2, (dlg->obj_name, 
		   "pjsip_dlg_init: invalid remote URI %s", buf.ptr));
	return -1;
    }

    /* Set remote Contact initially equal to the remote URI. */
    dlg->remote.contact = pjsip_contact_hdr_create(dlg->pool);
    dlg->remote.contact->star = 0;
    dlg->remote.contact->uri = dlg->remote.info->uri;

    /* Set initial remote target. */
    if (c_target != NULL) {
	pj_strdup_with_null( dlg->pool, &buf, c_target);
	dlg->remote.target = pjsip_parse_uri( dlg->pool, buf.ptr, buf.slen, 0);
	if (dlg->remote.target == NULL) {
	    PJ_LOG(2, (dlg->obj_name, 
		       "pjsip_dlg_init: invalid remote target %s", buf.ptr));
	    return -1;
	}
    } else {
	dlg->remote.target = dlg->remote.info->uri;
    }

    /* Create globally unique Call-ID */
    dlg->call_id = pjsip_cid_hdr_create(dlg->pool);
    pj_create_unique_string( dlg->pool, &dlg->call_id->id );

    /* Local and remote CSeq */
    pj_gettimeofday(&tv);
    dlg->local.cseq = tv.sec & 0xFFFF;
    dlg->remote.cseq = 0;

    /* Initial state is NULL. */
    event.type = event.src_type = PJSIP_EVENT_TX_MSG;
    event.src.data = NULL;
    dlg_set_state(dlg, PJSIP_DIALOG_STATE_NULL, &event);

    /* Done. */
    PJ_LOG(4, (dlg->obj_name, "%s dialog initialized, From: %.*s, To: %.*s",
	       pjsip_role_name(dlg->role), 
	       c_local_info->slen, c_local_info->ptr,
	       c_remote_info->slen, c_remote_info->ptr));

    return PJ_SUCCESS;
}

/*
 * Set credentials.
 */
PJ_DEF(pj_status_t) pjsip_dlg_set_credentials(  pjsip_dlg *dlg,
					        int count,
						const pjsip_cred_info *cred)
{
    if (count > 0) {
	dlg->cred_info = pj_pool_alloc(dlg->pool, count * sizeof(pjsip_cred_info));
	pj_memcpy(dlg->cred_info, cred, count * sizeof(pjsip_cred_info));
    }
    dlg->cred_count = count;
    return 0;
}

/*
 * Create a new request within dialog (i.e. after the dialog session has been
 * established). The construction of such requests follows the rule in 
 * RFC3261 section 12.2.1.
 */
static void dlg_create_request_throw( pjsip_tx_data **p_tdata,
				      pjsip_dlg *dlg,
				      const pjsip_method *method,
				      int cseq )
{
    pjsip_tx_data *tdata;
    pjsip_contact_hdr *contact;
    pjsip_route_hdr *route, *end_list;

    /* Contact Header field.
     * Contact can only be present in requests that establish dialog (in the 
     * core SIP spec, only INVITE).
     */
    if (method->id == PJSIP_INVITE_METHOD)
	contact = dlg->local.contact;
    else
	contact = NULL;

    tdata = pjsip_endpt_create_request_from_hdr( dlg->ua->endpt,
						 method,
						 dlg->remote.target,
						 dlg->local.info,
						 dlg->remote.info,
						 contact,
						 dlg->call_id,
						 cseq,
						 NULL);
    if (!tdata) {
	PJ_THROW(1);
	return;
    }

    /* Just copy dialog route-set to Route header. 
     * The transaction will do the processing as specified in Section 12.2.1
     * of RFC 3261 in function tsx_process_route() in sip_transaction.c.
     */
    route = dlg->route_set.next;
    end_list = &dlg->route_set;
    for (; route != end_list; route = route->next ) {
	pjsip_route_hdr *r;
	r = pjsip_hdr_shallow_clone( tdata->pool, route );
	pjsip_routing_hdr_set_route(r);
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)r);
    }

    /* Copy authorization headers. */
    pjsip_auth_init_req( dlg->pool, tdata, &dlg->auth_sess,
			 dlg->cred_count, dlg->cred_info);

    *p_tdata = tdata;
}


/*
 * This function is called by application to create new outgoing request
 * message for this dialog. After the request is created, application can
 * modify the message (such adding headers), and eventually send the request.
 */
PJ_DEF(pjsip_tx_data*) pjsip_dlg_create_request( pjsip_dlg *dlg,
						 const pjsip_method *method,
						 int cseq)
{
    PJ_USE_EXCEPTION;
    struct dialog_lock_data lck;
    pjsip_tx_data *tdata = NULL;

    pj_assert(dlg != NULL && method != NULL);
    if (!dlg || !method) {
	return NULL;
    }

    PJ_LOG(5, (dlg->obj_name, "Creating request"));

    /* Lock dialog. */
    lock_dialog(dlg, &lck);

    /* Use outgoing CSeq and increment it by one. */
    if (cseq < 0)
	cseq = dlg->local.cseq + 1;

    PJ_LOG(5, (dlg->obj_name, "creating request %.*s cseq=%d", 
			      method->name.slen, method->name.ptr, cseq));

    /* Create the request. */
    PJ_TRY {
	dlg_create_request_throw(&tdata, dlg, method, cseq);
	PJ_LOG(5, (dlg->obj_name, "request data %s created", tdata->obj_name));
    }
    PJ_DEFAULT {
	/* Failed! Delete transmit data. */
	if (tdata) {
	    pjsip_tx_data_dec_ref( tdata );
	    tdata = NULL;
	}
    }
    PJ_END;

    /* Unlock dialog. */
    unlock_dialog(dlg, &lck);

    return tdata;
}

/*
 * Sends request.
 * Select the transport for the request message
 */
static pj_status_t dlg_send_request( pjsip_dlg *dlg, pjsip_tx_data *tdata )
{
    pjsip_transaction *tsx;
    pj_status_t status = PJ_SUCCESS;
    struct dialog_lock_data lck;

    pj_assert(dlg != NULL && tdata != NULL);
    if (!dlg || !tdata) {
	return -1;
    }

    PJ_LOG(5, (dlg->obj_name, "sending request %s", tdata->obj_name));

    /* Lock dialog. */
    lock_dialog(dlg, &lck);

    /* Create a new transaction. */
    tsx = pjsip_endpt_create_tsx( dlg->ua->endpt );
    if (!tsx) {
	unlock_dialog(dlg, &lck);
	return -1;
    }

    PJ_LOG(4, (dlg->obj_name, "Created new UAC transaction: %s", tsx->obj_name));

    /* Initialize transaction */
    tsx->module_data[dlg->ua->mod_id] = dlg;
    status = pjsip_tsx_init_uac( tsx, tdata );
    if (status != PJ_SUCCESS) {
	unlock_dialog(dlg, &lck);
	pjsip_endpt_destroy_tsx( dlg->ua->endpt, tsx );
	return -1;
    }
    pjsip_endpt_register_tsx( dlg->ua->endpt, tsx );

    /* Start the transaction. */
    pjsip_tsx_on_tx_msg(tsx, tdata);

    /* Unlock dialog. */
    unlock_dialog(dlg, &lck);

    return status;
}

/*
 * This function can be called by application to send ANY outgoing message
 * to remote party. 
 */
PJ_DEF(pj_status_t) pjsip_dlg_send_msg( pjsip_dlg *dlg, pjsip_tx_data *tdata )
{
    pj_status_t status;
    int tsx_status;
    struct dialog_lock_data lck;

    pj_assert(dlg != NULL && tdata != NULL);
    if (!dlg || !tdata) {
	return -1;
    }

    lock_dialog(dlg, &lck);

    if (tdata->msg->type == PJSIP_REQUEST_MSG) {
	int request_cseq;
	pjsip_msg *msg = tdata->msg;
	pjsip_cseq_hdr *cseq_hdr;

	switch (msg->line.req.method.id) {
	case PJSIP_CANCEL_METHOD:

	    /* Check the INVITE transaction state. */
	    tsx_status = dlg->invite_tsx->status_code;
	    if (tsx_status >= 200) {
		/* Already terminated. Can't cancel. */
		status = -1;
		goto on_return;
	    }

	    /* If we've got provisional response, then send CANCEL and wait for
	     * the response to INVITE to arrive. Otherwise just send CANCEL and
	     * terminate the INVITE.
	     */
	    if (tsx_status < 100) {
		pjsip_tsx_terminate( dlg->invite_tsx, 
				     PJSIP_SC_REQUEST_TERMINATED);
		status = 0;
		goto on_return;
	    }

	    status = 0;
	    request_cseq = dlg->invite_tsx->cseq;
	    break;

	case PJSIP_ACK_METHOD:
	    /* Sending ACK outside of transaction is not supported at present! */
	    pj_assert(0);
	    status = 0;
	    request_cseq = dlg->local.cseq;
	    break;

	case PJSIP_INVITE_METHOD:
	    /* For an initial INVITE, reset dialog state to NULL so we get
	     * 'normal' UAC notifications such as on_provisional(), etc.
	     * Initial INVITE is the request that is sent when the dialog has
	     * not been established yet. It's not necessarily the first INVITE
	     * sent, as when the Authorization fails, subsequent INVITE are also
	     * considered as an initial INVITE.
	     */
	    if (dlg->state != PJSIP_DIALOG_STATE_ESTABLISHED) {
		/* Set state to NULL. */
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_NULL, NULL);

	    } else {
		/* This is a re-INVITE */
	    }
	    status = 0;
	    request_cseq = dlg->local.cseq + 1;
	    break;

	default:
	    status = 0;
	    request_cseq = dlg->local.cseq + 1;
	    break;
	}

	if (status != 0)
	    goto on_return;

	/* Update dialog's local CSeq, if necessary. */
	if (request_cseq != dlg->local.cseq)
	    dlg->local.cseq = request_cseq;

	/* Update CSeq header in the request. */
	cseq_hdr = (pjsip_cseq_hdr*) pjsip_msg_find_hdr( tdata->msg,
							 PJSIP_H_CSEQ, NULL);
	pj_assert(cseq_hdr != NULL);

	/* Update the CSeq */
	cseq_hdr->cseq = request_cseq;

	/* Force the whole message to be re-printed. */
	pjsip_tx_data_invalidate_msg( tdata );

	/* Now send the request. */
	status = dlg_send_request(dlg, tdata);

    } else {
	/*
	 * This is only valid for sending response to INVITE!
	 */
	pjsip_cseq_hdr *cseq_hdr;

	if (dlg->invite_tsx == NULL || dlg->invite_tsx->status_code >= 200) {
	    status = -1;
	    goto on_return;
	}

	cseq_hdr = (pjsip_cseq_hdr*) pjsip_msg_find_hdr( tdata->msg,
							 PJSIP_H_CSEQ, NULL);
	pj_assert(cseq_hdr);

	if (cseq_hdr->method.id != PJSIP_INVITE_METHOD) {
	    status = -1;
	    goto on_return;
	}

	pj_assert(cseq_hdr->cseq == dlg->invite_tsx->cseq);
	
	pjsip_tsx_on_tx_msg(dlg->invite_tsx, tdata);
	status = 0;
    }

on_return:
    /* Unlock dialog. */
    unlock_dialog(dlg, &lck);
   
    /* Whatever happen delete the message. */
    pjsip_tx_data_dec_ref( tdata );

    return status;
}

/*
 * Sends outgoing invitation.
 */
PJ_DEF(pjsip_tx_data*) pjsip_dlg_invite( pjsip_dlg *dlg )
{
    pjsip_method method;
    struct dialog_lock_data lck;
    const pjsip_allow_hdr *allow_hdr;
    pjsip_tx_data *tdata;

    pj_assert(dlg != NULL);
    if (!dlg) {
	return NULL;
    }

    PJ_LOG(4, (dlg->obj_name, "request to send invitation"));

    /* Lock dialog. */
    lock_dialog(dlg, &lck);

    /* Create request. */
    pjsip_method_set( &method, PJSIP_INVITE_METHOD);
    tdata = pjsip_dlg_create_request( dlg, &method, -1 );
    if (tdata == NULL) {
	unlock_dialog(dlg, &lck);
	return NULL;
    }

    /* Invite SHOULD contain "Allow" header. */
    allow_hdr = pjsip_endpt_get_allow_hdr( dlg->ua->endpt );
    if (allow_hdr) {
	pjsip_msg_add_hdr( tdata->msg, 
			   pjsip_hdr_shallow_clone( tdata->pool, allow_hdr));
    }

    /* Unlock dialog. */
    unlock_dialog(dlg, &lck);

    return tdata;
}

/*
 * Cancel pending outgoing dialog invitation. 
 */
PJ_DEF(pjsip_tx_data*) pjsip_dlg_cancel( pjsip_dlg *dlg )
{
    pjsip_tx_data *tdata = NULL;
    struct dialog_lock_data lck;

    pj_assert(dlg != NULL);
    if (!dlg) {
	return NULL;
    }

    PJ_LOG(4, (dlg->obj_name, "request to cancel invitation"));

    lock_dialog(dlg, &lck);

    /* Check the INVITE transaction. */
    if (dlg->invite_tsx == NULL || dlg->role != PJSIP_ROLE_UAC) {
	PJ_LOG(2, (dlg->obj_name, "pjsip_dlg_cancel failed: "
				  "no INVITE transaction found"));
	goto on_return;
    }

    /* Construct the CANCEL request. */
    tdata = pjsip_endpt_create_cancel( dlg->ua->endpt,
				       dlg->invite_tsx->last_tx );
    if (tdata == NULL) {
	PJ_LOG(2, (dlg->obj_name, "pjsip_dlg_cancel failed: "
				  "unable to construct request"));
	goto on_return;
    }

    /* Add reference counter to tdata. */
    pjsip_tx_data_add_ref(tdata);

on_return:
    unlock_dialog(dlg, &lck);
    return tdata;
}


/*
 * Answer incoming dialog invitation, with either provisional responses
 * or a final response.
 */
PJ_DEF(pjsip_tx_data*) pjsip_dlg_answer( pjsip_dlg *dlg, int code )
{
    pjsip_tx_data *tdata = NULL;
    pjsip_msg *msg;
    struct dialog_lock_data lck;

    pj_assert(dlg != NULL);
    if (!dlg) {
	return NULL;
    }

    PJ_LOG(4, (dlg->obj_name, "pjsip_dlg_answer: code=%d", code));

    /* Lock dialog. */
    lock_dialog(dlg, &lck);

    /* Must have pending INVITE. */
    if (dlg->invite_tsx == NULL) {
	PJ_LOG(2, (dlg->obj_name, "pjsip_dlg_answer: no INVITE transaction found"));
	goto on_return;
    }
    /* Must be UAS. */
    if (dlg->role != PJSIP_ROLE_UAS) {
	PJ_LOG(2, (dlg->obj_name, "pjsip_dlg_answer: not UAS"));
	goto on_return;
    }
    /* Must have not answered with final response before. */
    if (dlg->invite_tsx->status_code >= 200) {
	PJ_LOG(2, (dlg->obj_name, "pjsip_dlg_answer: transaction already terminated "
				  "with status %d", dlg->invite_tsx->status_code));
	goto on_return;
    }

    /* Get transmit data and the message. 
     * We will rewrite the message with a new status code.
     */
    tdata = dlg->invite_tsx->last_tx;
    msg = tdata->msg;

    /* Set status code and reason phrase. */
    if (code < 100 || code >= 700) code = 500;
    msg->line.status.code = code;
    msg->line.status.reason = *pjsip_get_status_text(code);

    /* For 2xx response, Contact and Record-Route must be added. */
    if (PJSIP_IS_STATUS_IN_CLASS(code,200)) {
	const pjsip_allow_hdr *allow_hdr;

	if (pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL) == NULL) {
	    pjsip_contact_hdr *contact;
	    contact = pjsip_hdr_shallow_clone( tdata->pool, dlg->local.contact);
	    pjsip_msg_add_hdr( msg, (pjsip_hdr*)contact );
	}

	/* 2xx response MUST contain "Allow" header. */
	allow_hdr = pjsip_endpt_get_allow_hdr( dlg->ua->endpt );
	if (allow_hdr) {
	    pjsip_msg_add_hdr( msg, pjsip_hdr_shallow_clone( tdata->pool, allow_hdr));
	}
    }

    /* for all but 100 responses, To-tag must be set. */
    if (code != 100) {
	pjsip_to_hdr *to;
	to = pjsip_msg_find_hdr( msg, PJSIP_H_TO, NULL);
	to->tag = dlg->local.tag;
    }

    /* Reset packet buffer. */
    pjsip_tx_data_invalidate_msg(tdata);

    /* Add reference counter */
    pjsip_tx_data_add_ref(tdata);

on_return:

    /* Unlock dialog. */
    unlock_dialog(dlg, &lck);

    return tdata;
}


/*
 * Send BYE request to terminate the dialog's session.
 */
PJ_DEF(pjsip_tx_data*) pjsip_dlg_bye( pjsip_dlg *dlg )
{
    pjsip_method method;
    struct dialog_lock_data lck;
    pjsip_tx_data *tdata;

    if (!dlg) {
	pj_assert(dlg != NULL);
	return NULL;
    }

    PJ_LOG(4, (dlg->obj_name, "request to terminate session"));

    lock_dialog(dlg, &lck);

    pjsip_method_set( &method, PJSIP_BYE_METHOD);
    tdata = pjsip_dlg_create_request( dlg, &method, -1 );

    unlock_dialog(dlg, &lck);

    return tdata;
}

/*
 * High level function to disconnect dialog's session. Depending on dialog's 
 * state, this function will either send CANCEL, final response, or BYE to
 * trigger the disconnection. A status code must be supplied, which will be
 * sent if dialog will be transmitting a final response to INVITE.
 */
PJ_DEF(pjsip_tx_data*) pjsip_dlg_disconnect( pjsip_dlg *dlg, 
					     int status_code )
{
    pjsip_tx_data *tdata = NULL;

    pj_assert(dlg != NULL);
    if (!dlg) {
	return NULL;
    }

    switch (dlg->state) {
    case PJSIP_DIALOG_STATE_INCOMING:
	tdata = pjsip_dlg_answer(dlg, status_code);
	break;

    case PJSIP_DIALOG_STATE_CALLING:
	tdata = pjsip_dlg_cancel(dlg);
	break;

    case PJSIP_DIALOG_STATE_PROCEEDING:
	if (dlg->role == PJSIP_ROLE_UAC) {
	    tdata = pjsip_dlg_cancel(dlg);
	} else {
	    tdata = pjsip_dlg_answer(dlg, status_code);
	}
	break;

    case PJSIP_DIALOG_STATE_ESTABLISHED:
	tdata = pjsip_dlg_bye(dlg);
	break;

    default:
	PJ_LOG(4,(dlg->obj_name, "Invalid state %s in pjsip_dlg_disconnect()", 
		  dlg_state_names[dlg->state]));
	break;
    }

    return tdata;
}

/*
 * Handling of the receipt of 2xx/INVITE response.
 */
static void dlg_on_recv_2xx_invite( pjsip_dlg *dlg,
				    pjsip_event *event )
{
    pjsip_msg *msg;
    pjsip_contact_hdr *contact;
    pjsip_hdr *hdr, *end_hdr;
    pjsip_method method;
    pjsip_tx_data *ack_tdata;
    
    /* Get the message */
    msg = event->src.rdata->msg;
    
    /* Update remote's tag information. */
    pj_strdup(dlg->pool, &dlg->remote.info->tag, &event->src.rdata->to_tag);

    /* Copy Contact information in the 2xx/INVITE response to dialog's.
     * remote contact
     */
    contact = pjsip_msg_find_hdr( msg, PJSIP_H_CONTACT, NULL);
    if (contact) {
	dlg->remote.contact = pjsip_hdr_clone( dlg->pool, contact );
    } else {
	/* duplicate contact from "From" header (?) */
	PJ_LOG(4,(dlg->obj_name, "Received 200/OK to INVITE with no Contact!"));
	dlg->remote.contact = pjsip_contact_hdr_create(dlg->pool);
	dlg->remote.contact->uri = dlg->remote.info->uri;
    }
    
    /* Copy Record-Route header (in reverse order) as dialog's route-set,
     * overwriting previous route-set, if any, even if the received route-set
     * is empty.
     */
    pj_list_init(&dlg->route_set);
    end_hdr = &msg->hdr;
    for (hdr = msg->hdr.prev; hdr!=end_hdr; hdr = hdr->prev) {
	if (hdr->type == PJSIP_H_RECORD_ROUTE) {
	    pjsip_route_hdr *r;
	    r = pjsip_hdr_clone(dlg->pool, hdr);
	    pjsip_routing_hdr_set_route(r);
	    pj_list_insert_before(&dlg->route_set, r);
	}
    }
    
    /* On receipt of 200/INVITE response, send ACK. 
     * This ack must be saved and retransmitted whenever we receive
     * 200/INVITE retransmission, until 64*T1 seconds elapsed.
     */
    pjsip_method_set( &method, PJSIP_ACK_METHOD);
    ack_tdata = pjsip_dlg_create_request( dlg, &method, dlg->invite_tsx->cseq);
    if (ack_tdata == NULL) {
	//PJ_TODO(HANDLE_CREATE_ACK_FAILURE)
	PJ_LOG(2, (dlg->obj_name, "Error sending ACK msg: can't create request"));
	return;
    }	
    
    /* Send with the transaction. */
    pjsip_tsx_on_tx_ack( dlg->invite_tsx, ack_tdata);
	
    /* Decrement reference counter because pjsip_dlg_create_request 
     * automatically increments the request.
     */
    pjsip_tx_data_dec_ref( ack_tdata );
}

/*
 * State NULL, before any events have been received.
 */
static int  dlg_on_state_null( pjsip_dlg *dlg, 
			       pjsip_transaction *tsx,
			       pjsip_event *event)
{
    if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED &&
	event->src_type == PJSIP_EVENT_RX_MSG) 
    {
	pjsip_hdr *hdr, *hdr_list;

	pj_assert(tsx->method.id == PJSIP_INVITE_METHOD);

	/* Save the INVITE transaction. */
	dlg->invite_tsx = tsx;

	/* Change state to INCOMING */
	dlg_set_state(dlg, PJSIP_DIALOG_STATE_INCOMING, event);

	/* Create response buffer. */
	tsx->last_tx = pjsip_endpt_create_response( dlg->ua->endpt, event->src.rdata, 100);
	pjsip_tx_data_add_ref(tsx->last_tx);

	/* Copy the Record-Route headers into dialog's route_set, maintaining
	 * the order.
	 */
	pj_list_init(&dlg->route_set);
	hdr_list = &event->src.rdata->msg->hdr;
	hdr = hdr_list->next;
	while (hdr != hdr_list) {
	    if (hdr->type == PJSIP_H_RECORD_ROUTE) {
		pjsip_route_hdr *route;
		route = pjsip_hdr_clone(dlg->pool, hdr);
		pjsip_routing_hdr_set_route(route);
		pj_list_insert_before(&dlg->route_set, route);
	    }
	    hdr = hdr->next;
	}

	/* Notify application. */
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);

    } else if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED &&
	       event->src_type == PJSIP_EVENT_TX_MSG) 
    {
	pj_assert(tsx->method.id == PJSIP_INVITE_METHOD);

	/* Save the INVITE transaction. */
	dlg->invite_tsx = tsx;

	/* Change state to CALLING. */
	dlg_set_state(dlg, PJSIP_DIALOG_STATE_CALLING, event);

	/* Notify application. */
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);

    } else {
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_OTHER, event);
    }

    return 0;
}

/*
 * State INCOMING is after the (callee) dialog has been initialized with
 * the incoming request, but before any responses is sent by the dialog.
 */
static int  dlg_on_state_incoming( pjsip_dlg *dlg, 
				   pjsip_transaction *tsx,
				   pjsip_event *event)
{
    return dlg_on_state_proceeding_callee( dlg, tsx, event );
}

/*
 * State CALLING is after the (caller) dialog has sent outgoing invitation
 * but before any responses are received.
 */
static int  dlg_on_state_calling( pjsip_dlg *dlg, 
				  pjsip_transaction *tsx,
				  pjsip_event *event)
{
    if (tsx == dlg->invite_tsx) {
	return dlg_on_state_proceeding_caller( dlg, tsx, event );
    }
    return 0;
}

/*
 * State PROCEEDING is after provisional response is received.
 * Since the processing is similar to state CALLING, this function is also
 * called for CALLING state.
 */
static int  dlg_on_state_proceeding_caller( pjsip_dlg *dlg, 
					    pjsip_transaction *tsx,
					    pjsip_event *event)
{
    int dlg_is_terminated = 0;

    /* We only care about our INVITE transaction. 
     * Ignore other transaction progression (such as CANCEL).
     */
    if (tsx != dlg->invite_tsx) {
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_OTHER, event);
	return 0;
    }

    if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED) {
	switch (tsx->state) {
	case PJSIP_TSX_STATE_PROCEEDING:
	    if (dlg->state != PJSIP_DIALOG_STATE_PROCEEDING) {
		/* Change state to PROCEEDING */
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_PROCEEDING, event);

		/* Notify application. */
		dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);
	    } else {
		/* Also notify application. */
		dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_OTHER, event);
	    }
	    break;

	case PJSIP_TSX_STATE_COMPLETED:
	    /* Change dialog state. */
	    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 200)) {
		/* Update remote target, take it from the contact hdr. */
		pjsip_contact_hdr *contact;
		contact = pjsip_msg_find_hdr(event->src.rdata->msg, 
					     PJSIP_H_CONTACT, NULL);
		if (contact) {
		    dlg->remote.target = pjsip_uri_clone(dlg->pool, contact->uri);
		} else {
		    PJ_LOG(4,(dlg->obj_name, 
			      "Warning: found no Contact hdr in 200/OK"));
		}
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_CONNECTING, event);
	    } else if (tsx->status_code==401 || tsx->status_code==407) {
		/* Handle Authentication challenge. */
		pjsip_tx_data *tdata;
		tdata = pjsip_auth_reinit_req( dlg->ua->endpt,
					       dlg->pool, &dlg->auth_sess,
					       dlg->cred_count, dlg->cred_info,
					       tsx->last_tx, event->src.rdata);
		if (tdata) {
		    /* Re-use original request, with a new transaction. 
		     * Need not to worry about CSeq, dialog will take care.
		     */
		    pjsip_dlg_send_msg(dlg, tdata);
		    return 0;
		} else {
		    dlg_set_state(dlg, PJSIP_DIALOG_STATE_DISCONNECTED, event);
		}
	    } else {
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_DISCONNECTED, event);
	    }

	    /* Notify application. */
	    dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);

	    /* Send ACK when dialog is connected. */
	    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 200)) {
		pj_assert(event->src_type == PJSIP_EVENT_RX_MSG);
		dlg_on_recv_2xx_invite(dlg, event);
	    }
	    break;

	case PJSIP_TSX_STATE_TERMINATED:
	    /*
	     * Transaction is terminated because of timeout or transport error.
	     * To let the application go to normal state progression, call the
	     * callback twice. First is to emulate disconnection, and then call
	     * again (with state TERMINATED) to destroy the dialog.
	     */
	    dlg_set_state(dlg, PJSIP_DIALOG_STATE_DISCONNECTED, event);
	    dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);

	    /* The INVITE transaction will be destroyed, so release reference 
	     * to it. 
	     */
	    dlg->invite_tsx = NULL;

	    /* We should terminate the dialog now.
	     * But it's possible that we have other pending transactions (for 
	     * example, outgoing CANCEL is in progress).
	     * So destroy the dialog only if there's no other transaction.
	     */
	    if (dlg->pending_tsx_count == 0) {
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_TERMINATED, event);
		dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);
		dlg_is_terminated = 1;
	    }
	    break;

	default:
	    pj_assert(0);
	    break;
	}
    } else {
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_OTHER, event);
    }
    return dlg_is_terminated ? -1 : 0;
}

/*
 * State PROCEEDING for UAS is after the callee send provisional response.
 * This function is also called for INCOMING state.
 */
static int  dlg_on_state_proceeding_callee( pjsip_dlg *dlg, 
					    pjsip_transaction *tsx,
					    pjsip_event *event)
{
    int dlg_is_terminated = 0;

    pj_assert( dlg->invite_tsx != NULL );

    if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED &&
	event->src_type == PJSIP_EVENT_TX_MSG && 
	tsx == dlg->invite_tsx) 
    {
	switch (tsx->state) {
	case PJSIP_TSX_STATE_PROCEEDING:
	    /* Change state to PROCEEDING */
	    dlg_set_state(dlg, PJSIP_DIALOG_STATE_PROCEEDING, event);

	    /* Notify application. */
	    dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);
	    break;

	case PJSIP_TSX_STATE_COMPLETED:
	case PJSIP_TSX_STATE_TERMINATED:
	    /* Change dialog state. */
	    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 200)) {
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_CONNECTING, event);
	    } else {
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_DISCONNECTED, event);
	    }

	    /* Notify application. */
	    dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);

	    /* If transaction is terminated in non-2xx situation, 
	     * terminate dialog as well. This happens when something unexpected
	     * occurs, such as transport error.
	     */
	    if (tsx->state == PJSIP_TSX_STATE_TERMINATED && 
		!PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 200)) 
	    {
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_TERMINATED, event);
		dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);
		dlg_is_terminated = 1;
	    }
	    break;

	default:
	    pj_assert(0);
	    break;
	}

    } else if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED &&
	       event->src_type == PJSIP_EVENT_RX_MSG && 
	       tsx->method.id == PJSIP_CANCEL_METHOD) 
    {
	pjsip_tx_data *tdata;

	/* Check if sequence number matches the pending INVITE. */
	if (dlg->invite_tsx==NULL ||
	    pj_strcmp(&tsx->branch, &dlg->invite_tsx->branch) != 0) 
	{
	    PJ_LOG(4, (dlg->obj_name, "Received CANCEL with no matching INVITE"));

	    /* No matching INVITE transaction found. */
	    tdata = pjsip_endpt_create_response(dlg->ua->endpt,
						event->src.rdata,
						PJSIP_SC_CALL_TSX_DOES_NOT_EXIST );
	    pjsip_tsx_on_tx_msg(tsx, tdata);
	    return 0;
	}

	/* Always respond the CANCEL with 200/CANCEL no matter what. */
	tdata = pjsip_endpt_create_response(dlg->ua->endpt,
					    event->src.rdata,
					    200 );
	pjsip_tsx_on_tx_msg( tsx, tdata );

	/* Respond the INVITE transaction with 487, only if transaction has
	 * not completed. 
	 */
	if (dlg->invite_tsx->last_tx) {
	    if (dlg->invite_tsx->status_code < 200) {
		tdata = dlg->invite_tsx->last_tx;
		tdata->msg->line.status.code = 487;
		tdata->msg->line.status.reason = *pjsip_get_status_text(487);
		/* Reset packet buffer. */
		pjsip_tx_data_invalidate_msg(tdata);
		pjsip_tsx_on_tx_msg( dlg->invite_tsx, tdata );
	    } else {
		PJ_LOG(4, (dlg->obj_name, "Received CANCEL with no effect, "
					  "Transaction already terminated "
					  "with status %d",
					  dlg->invite_tsx->status_code));
	    }
	} else {
	    tdata = pjsip_endpt_create_response(dlg->ua->endpt,
						event->src.rdata,
						487);
	    pjsip_tsx_on_tx_msg( dlg->invite_tsx, tdata );
	}
    } else {
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_OTHER, event);
    }

    return dlg_is_terminated ? -1 : 0;
}

static int  dlg_on_state_proceeding( pjsip_dlg *dlg, 
				     pjsip_transaction *tsx,
				     pjsip_event *event)
{
    if (dlg->role == PJSIP_ROLE_UAC) {
	return dlg_on_state_proceeding_caller( dlg, tsx, event );
    } else {
	return dlg_on_state_proceeding_callee( dlg, tsx, event );
    }
}

static int  dlg_on_state_connecting( pjsip_dlg *dlg, 
				     pjsip_transaction *tsx,
				     pjsip_event *event)
{
    if (tsx == dlg->invite_tsx) {
	if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED &&
	    (tsx->state == PJSIP_TSX_STATE_TERMINATED ||
	    tsx->state == PJSIP_TSX_STATE_COMPLETED ||
	    tsx->state == PJSIP_TSX_STATE_CONFIRMED)) 
	{
	    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 200)) {
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_ESTABLISHED, event);
	    } else {
		/* Probably because we never get the ACK, or transport error
		* when sending ACK.
		*/
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_DISCONNECTED, event);
	    }
	    dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);
	} else {
	    dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_OTHER, event);
	}
    } else {
	/* Handle case when transaction is started when dialog is connecting
	* (e.g. BYE requests cross wire.
	*/
	if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED &&
	    event->src_type == PJSIP_EVENT_RX_MSG &&
	    tsx->role == PJSIP_ROLE_UAS)
	{
	    pjsip_tx_data *response;

	    if (tsx->status_code >= 200)
		return 0;

	    if (tsx->method.id == PJSIP_BYE_METHOD) {
		/* Set state to DISCONNECTED. */
		dlg_set_state(dlg, PJSIP_DIALOG_STATE_DISCONNECTED, event);

		/* Notify application. */
		dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);

		response = pjsip_endpt_create_response(	dlg->ua->endpt, 
							event->src.rdata, 200);
	    } else {
		response = pjsip_endpt_create_response( dlg->ua->endpt, event->src.rdata,
							PJSIP_SC_INTERNAL_SERVER_ERROR);
	    }

	    if (response)
		pjsip_tsx_on_tx_msg(tsx, response);

	    return 0;
	}
    }
    return 0;
}

static int  dlg_on_state_established( pjsip_dlg *dlg, 
				      pjsip_transaction *tsx,
				      pjsip_event *event)
{
    PJ_UNUSED_ARG(tsx)

    if (tsx && tsx->method.id == PJSIP_BYE_METHOD) {
	/* Set state to DISCONNECTED. */
	dlg_set_state(dlg, PJSIP_DIALOG_STATE_DISCONNECTED, event);

	/* Notify application. */
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);

	/* Answer with 200/BYE. */
	if (event->src_type == PJSIP_EVENT_RX_MSG) {
	    pjsip_tx_data *tdata;
	    tdata = pjsip_endpt_create_response(dlg->ua->endpt,
						event->src.rdata,
						200 );
	    if (tdata)
		pjsip_tsx_on_tx_msg( tsx, tdata );
	}
    } else if (tsx && event->src_type == PJSIP_EVENT_RX_MSG) {
	pjsip_method_e method = event->src.rdata->cseq->method.id;

	PJ_TODO(PROPERLY_HANDLE_REINVITATION)

	/* Reinvitation. The message may be INVITE or an ACK. */
	if (method == PJSIP_INVITE_METHOD) {
	    if (dlg->invite_tsx && dlg->invite_tsx->status_code < 200) {
		/* Section 14.2: A UAS that receives a second INVITE before it 
		* sends the final response to a first INVITE with a lower
		* CSeq sequence number on the same dialog MUST return a 500 
		* (Server Internal Error) response to the second INVITE and 
		* MUST include a Retry-After header field with a randomly 
		* chosen value of between 0 and 10 seconds.
		*/
		pjsip_retry_after_hdr *hdr;
		pjsip_tx_data *tdata = 
		    pjsip_endpt_create_response(dlg->ua->endpt, 
						event->src.rdata, 500);

		if (!tdata)
		    return 0;

		/* Add Retry-After. */
		hdr = pjsip_retry_after_hdr_create(tdata->pool);
		hdr->ivalue = 9;
		pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);

		/* Send. */
		pjsip_tsx_on_tx_msg(tsx, tdata);

		return 0;
	    }

	    /* Keep this as our current INVITE transaction. */
	    dlg->invite_tsx = tsx;

	    /* Create response buffer. */
	    tsx->last_tx = pjsip_endpt_create_response( dlg->ua->endpt, 
							event->src.rdata, 100);
	    pjsip_tx_data_add_ref(tsx->last_tx);

	}

	/* Notify application. */
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_MID_CALL_REQUEST, event);

    }  else {
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_OTHER, event);
    }

    return 0;
}

static int  dlg_on_state_disconnected( pjsip_dlg *dlg, 
				       pjsip_transaction *tsx,
				       pjsip_event *event)
{
    PJ_UNUSED_ARG(tsx)

    /* Handle case when transaction is started when dialog is disconnected
     * (e.g. BYE requests cross wire.
     */
    if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED &&
	event->src_type == PJSIP_EVENT_RX_MSG &&
	tsx->role == PJSIP_ROLE_UAS)
    {
	pjsip_tx_data *response = NULL;

	if (tsx->status_code >= 200)
	    return 0;

	if (tsx->method.id == PJSIP_BYE_METHOD) {
	    response = pjsip_endpt_create_response( dlg->ua->endpt, 
						    event->src.rdata, 200);
	} else {
	    response = pjsip_endpt_create_response( dlg->ua->endpt, event->src.rdata, 
						    PJSIP_SC_INTERNAL_SERVER_ERROR);
	}
	if (response)
	    pjsip_tsx_on_tx_msg(tsx, response);

	return 0;
    } 
    /* Handle case when outgoing BYE was rejected with 401/407 */
    else if (event->type == PJSIP_EVENT_TSX_STATE_CHANGED &&
	     event->src_type == PJSIP_EVENT_RX_MSG &&
	     tsx->role == PJSIP_ROLE_UAC)
    {
	if (tsx->status_code==401 || tsx->status_code==407) {
	    pjsip_tx_data *tdata;
	    tdata = pjsip_auth_reinit_req( dlg->ua->endpt, dlg->pool,
					   &dlg->auth_sess,
					   dlg->cred_count, dlg->cred_info,
					   tsx->last_tx, event->src.rdata);
	    if (tdata) {
		pjsip_dlg_send_msg(dlg, tdata);
	    }
	}
    }
	    

    if (dlg->pending_tsx_count == 0) {
	/* Set state to TERMINATED. */
	dlg_set_state(dlg, PJSIP_DIALOG_STATE_TERMINATED, event);

	/* Notify application. */
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_STATE_CHANGED, event);

	return -1;
    } else {
	dlg_call_dlg_callback(dlg, PJSIP_DIALOG_EVENT_OTHER, event);
    }

    return 0;
}

static int  dlg_on_state_terminated( pjsip_dlg *dlg, 
				     pjsip_transaction *tsx,
				     pjsip_event *event)
{
    PJ_UNUSED_ARG(dlg)
    PJ_UNUSED_ARG(tsx)
    PJ_UNUSED_ARG(event)

    return -1;
}


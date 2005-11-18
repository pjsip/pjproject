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
#include <pjsip_mod_ua/sip_ua.h>
#include <pjsip_mod_ua/sip_dialog.h>
#include <pjsip_mod_ua/sip_ua_private.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_misc.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_transaction.h>
#include <pj/list.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <pj/os.h>
#include <pj/hash.h>
#include <pj/pool.h>

#define PJSIP_POOL_LEN_USER_AGENT   1024
#define PJSIP_POOL_INC_USER_AGENT   0


#define LOG_THIS    "useragent.."

/*
 * Static prototypes.
 */
static pj_status_t ua_init( pjsip_endpoint *endpt,
			    struct pjsip_module *mod, pj_uint32_t id );
static pj_status_t ua_start( struct pjsip_module *mod );
static pj_status_t ua_deinit( struct pjsip_module *mod );
static void ua_tsx_handler( struct pjsip_module *mod, pjsip_event *evt );
static pjsip_dlg *find_dialog( pjsip_user_agent *ua,
			       pjsip_rx_data *rdata );
static pj_status_t ua_register_dialog( pjsip_user_agent *ua, pjsip_dlg *dlg,
				       pj_str_t *key );
PJ_DECL(void) pjsip_on_dialog_destroyed( pjsip_dlg *dlg );

/*
 * Default UA instance.
 */
static pjsip_user_agent ua_instance;

/*
 * Module interface.
 */
static struct pjsip_module mod_ua = 
{
    { "User-Agent", 10 },   /* Name.		*/
    0,			    /* Flag		*/
    128,		    /* Priority		*/
    NULL,		    /* User agent instance, initialized by APP.	*/
    0,			    /* Number of methods supported (will be initialized later). */
    { 0 },		    /* Array of methods (will be initialized later) */
    &ua_init,		    /* init_module()	*/
    &ua_start,		    /* start_module()	*/
    &ua_deinit,		    /* deinit_module()	*/
    &ua_tsx_handler,	    /* tsx_handler()	*/
};

/*
 * Initialize user agent instance.
 */
static pj_status_t ua_init( pjsip_endpoint *endpt,
			    struct pjsip_module *mod, pj_uint32_t id )
{
    static pjsip_method m_invite, m_ack, m_cancel, m_bye;
    pjsip_user_agent *ua = mod->mod_data;
    extern int pjsip_dlg_lock_tls_id;	/* defined in sip_dialog.c */

    pjsip_method_set( &m_invite, PJSIP_INVITE_METHOD );
    pjsip_method_set( &m_ack, PJSIP_ACK_METHOD );
    pjsip_method_set( &m_cancel, PJSIP_CANCEL_METHOD );
    pjsip_method_set( &m_bye, PJSIP_BYE_METHOD );

    mod->method_cnt = 4;
    mod->methods[0] = &m_invite;
    mod->methods[1] = &m_ack;
    mod->methods[2] = &m_cancel;
    mod->methods[3] = &m_bye;

    /* Initialize the user agent. */
    ua->endpt = endpt;
    ua->pool = pjsip_endpt_create_pool(endpt, "pua%p", PJSIP_POOL_LEN_UA, 
				       PJSIP_POOL_INC_UA);
    if (!ua->pool) {
	return -1;
    }
    ua->mod_id = id;
    ua->mutex = pj_mutex_create(ua->pool, " ua%p", 0);
    if (!ua->mutex) {
	return -1;
    }
    ua->dlg_table = pj_hash_create(ua->pool, PJSIP_MAX_DIALOG_COUNT);
    if (ua->dlg_table == NULL) {
	return -1;
    }
    pj_list_init(&ua->dlg_list);

    /* Initialize dialog lock. */
    pjsip_dlg_lock_tls_id = pj_thread_local_alloc();
    if (pjsip_dlg_lock_tls_id == -1) {
	return -1;
    }
    pj_thread_local_set(pjsip_dlg_lock_tls_id, NULL);

    return 0;
}

/*
 * Start user agent instance.
 */
static pj_status_t ua_start( struct pjsip_module *mod )
{
    PJ_UNUSED_ARG(mod)
    return 0;
}

/*
 * Destroy user agent.
 */
static pj_status_t ua_deinit( struct pjsip_module *mod )
{
    pjsip_user_agent *ua = mod->mod_data;

    pj_mutex_unlock(ua->mutex);

    /* Release pool */
    if (ua->pool) {
	pjsip_endpt_destroy_pool( ua->endpt, ua->pool );
    }
    return 0;
}

/*
 * Get the module interface for the UA module.
 */
PJ_DEF(pjsip_module*) pjsip_ua_get_module(void)
{
    mod_ua.mod_data = &ua_instance;
    return &mod_ua;
}

/*
 * Register callback to receive dialog notifications.
 */
PJ_DEF(void) pjsip_ua_set_dialog_callback( pjsip_user_agent *ua, 
					   pjsip_dlg_callback *cb )
{
    ua->dlg_cb = cb;
}

/* 
 * Find dialog.
 * This function is called for a new transactions, which a dialog hasn't been
 * 'attached' to the transaction.
 */
static pjsip_dlg *find_dialog( pjsip_user_agent *ua, pjsip_rx_data *rdata )
{
    pjsip_dlg *dlg;
    pj_str_t *tag;

    /* Non-CANCEL requests/response can be found by looking at the tag in the
     * hash table. CANCEL requests don't have tags, so instead we'll try to
     * find the UAS INVITE transaction in endpoint's hash table
     */
    if (rdata->cseq->method.id == PJSIP_CANCEL_METHOD) {

	/* Create key for the rdata, but this time, use INVITE as the
	 * method.
	 */
	pj_str_t key;
	pjsip_role_e role;
	pjsip_method invite_method;
	pjsip_transaction *invite_tsx;

	if (rdata->msg->type == PJSIP_REQUEST_MSG) {
	    role = PJSIP_ROLE_UAS;
	} else {
	    role = PJSIP_ROLE_UAC;
	}
	pjsip_method_set(&invite_method, PJSIP_INVITE_METHOD);
	pjsip_tsx_create_key(rdata->pool, &key, role, &invite_method, rdata);

	/* Lookup the INVITE transaction */
	invite_tsx = pjsip_endpt_find_tsx(ua->endpt, &key);

	/* We should find the dialog attached to the INVITE transaction */
	return invite_tsx ? 
	    (pjsip_dlg*) invite_tsx->module_data[ua->mod_id] : NULL;

    } else {
	if (rdata->msg->type == PJSIP_REQUEST_MSG) {
	    tag = &rdata->to_tag;
	} else {
	    tag = &rdata->from_tag;
	}
	/* Find the dialog in UA hash table */
	pj_mutex_lock(ua->mutex);
	dlg = pj_hash_get( ua->dlg_table, tag->ptr, tag->slen );
	pj_mutex_unlock(ua->mutex);
    }
    
    return dlg;
}

/*
 * This function receives event notification from transactions. It is called by
 * endpoint.
 */
static void ua_tsx_handler( struct pjsip_module *mod, pjsip_event *event )
{
    pjsip_user_agent *ua = mod->mod_data;
    pjsip_dlg *dlg = NULL;
    pjsip_transaction *tsx = event->obj.tsx;

    PJ_LOG(5, (LOG_THIS, "ua_tsx_handler(tsx=%s, evt=%s, src=%s, data=%p)", 
	       (tsx ? tsx->obj_name : "NULL"),  pjsip_event_str(event->type), 
	       pjsip_event_str(event->src_type), event->src.data));

    /* Special case to handle ACK which doesn't match any INVITE transactions. */
    if (event->type == PJSIP_EVENT_RX_ACK_MSG) {
	/* Find the dialog based on the "tag". */
	dlg = find_dialog( ua, event->src.rdata );

	/* We should be able to find it. */
	if (!dlg) {
	    PJ_LOG(4,(LOG_THIS, "Unable to find dialog for incoming ACK"));
	    return;
	}

	/* Match CSeq with pending INVITE in dialog. */
	if (dlg->invite_tsx && dlg->invite_tsx->cseq==event->src.rdata->cseq->cseq) {
	    /* A match found. */
	    tsx = dlg->invite_tsx;

	    /* Pass the event to transaction if transaction handles ACK. */
	    if (tsx->handle_ack) {
		PJ_LOG(4,(LOG_THIS, "Re-routing strandled ACK to transaction"));
		pjsip_tsx_on_rx_msg(tsx, event->src.rdata);
		return;
	    }
	} else {
	    tsx = NULL;
	    PJ_LOG(4,(LOG_THIS, "Unable to find INVITE tsx for incoming ACK"));
	    return;
	}
    }

    /* For discard event, transaction is NULL. */
    if (tsx == NULL) {
	return;
    }

    /* Try to pickup the dlg from the transaction. */
    dlg = (pjsip_dlg*) tsx->module_data[ua->mod_id];

    if (dlg != NULL) {

	/* Nothing to do now. */

    } else if (event->src_type == PJSIP_EVENT_RX_MSG)  {

	/* This must be a new UAS transaction. */

	/* Finds dlg that can handle this transaction. */
	dlg = find_dialog( ua, event->src.rdata);

	/* Create a new dlg if there's no existing dlg that can handle 
	   the request, ONLY if the incoming message is an INVITE request.
	 */
	if (dlg==NULL && event->src.rdata->msg->type == PJSIP_REQUEST_MSG) {

	    if (event->src.rdata->msg->line.req.method.id == PJSIP_INVITE_METHOD) {
		/* Create new dialog. */
		dlg = pjsip_ua_create_dialog( ua, PJSIP_ROLE_UAS );

		if (dlg == NULL ||
		    pjsip_dlg_init_from_rdata( dlg, event->src.rdata) != 0) 
		{
		    pjsip_tx_data *tdata;

		    /* Dialog initialization has failed. Respond request with 500 */
		    if (dlg) {
			pjsip_ua_destroy_dialog(dlg);
		    }
		    tdata = pjsip_endpt_create_response(ua->endpt, event->src.rdata, 
							PJSIP_SC_INTERNAL_SERVER_ERROR);
		    if (tdata) {
			pjsip_tsx_on_tx_msg( event->obj.tsx, tdata );
		    }
		    return;
		}

	    } else {
		pjsip_tx_data *tdata;

		/* Check the method */
		switch (tsx->method.id) {
		case PJSIP_INVITE_METHOD:
		case PJSIP_ACK_METHOD:
		case PJSIP_BYE_METHOD:
		case PJSIP_CANCEL_METHOD:
		    /* Stale non-INVITE request.
		     * For now, respond all stale requests with 481 (?).
		     */
		    tdata = pjsip_endpt_create_response(ua->endpt, event->src.rdata, 
							PJSIP_SC_CALL_TSX_DOES_NOT_EXIST);
		    if (tdata) {
			pjsip_tsx_on_tx_msg( event->obj.tsx, tdata );
		    }
		    break;
		}

		return;
	    }
	} else {
	    /* Check the method */
	    switch (tsx->method.id) {
	    case PJSIP_INVITE_METHOD:
	    case PJSIP_ACK_METHOD:
	    case PJSIP_BYE_METHOD:
	    case PJSIP_CANCEL_METHOD:
		/* These methods belongs to dialog.
		 * If we receive these methods while no dialog is found, 
		 * then it must be a stale responses.
		 */
		break;
	    default:
		return;
	    }

	}
	
	if (dlg == NULL) {
	    PJ_LOG(3, (LOG_THIS, "Receives spurious rdata %p from %s:%d",
		       event->src.rdata, 
		       pj_sockaddr_get_str_addr(&event->src.rdata->addr),
		       pj_sockaddr_get_port(&event->src.rdata->addr)));
	}

	/* Set the dlg in the transaction (dlg can be NULL). */
	tsx->module_data[ua->mod_id] = dlg;

    } else {
	/* This CAN happen with event->src_type == PJSIP_EVENT_TX_MSG
	 * if UAS is responding to a transaction which does not exist. 
	 * Just ignore.
	 */
	return;
    }

    /* Pass the event to the dlg. */
    if (dlg) {
	pjsip_dlg_on_tsx_event(dlg, tsx, event);
    }
}

/*
 * Register dialog to UA.
 */
static pj_status_t ua_register_dialog( pjsip_user_agent *ua, pjsip_dlg *dlg,
				       pj_str_t *key )
{
    /* Assure that no entry with similar key exists in the hash table. */
    pj_assert( pj_hash_get( ua->dlg_table, key->ptr, key->slen) == 0);

    /* Insert entry to hash table. */
    pj_hash_set( dlg->pool, ua->dlg_table, 
		 key->ptr, key->slen, dlg);

    /* Insert to the list. */
    pj_list_insert_before(&ua->dlg_list, dlg);
    return PJ_SUCCESS;
}

/*
 * Create a new dialog.
 */
PJ_DEF(pjsip_dlg*) pjsip_ua_create_dialog( pjsip_user_agent *ua,
					      pjsip_role_e role )
{
    pj_pool_t *pool;
    pjsip_dlg *dlg;

    PJ_UNUSED_ARG(ua)

    /* Create pool for the dialog. */
    pool = pjsip_endpt_create_pool( ua->endpt, "pdlg%p",
				    PJSIP_POOL_LEN_DIALOG,
				    PJSIP_POOL_INC_DIALOG);

    /* Create the dialog. */
    dlg = pj_pool_calloc(pool, 1, sizeof(pjsip_dlg));
    dlg->pool = pool;
    dlg->ua = ua;
    dlg->role = role;
    sprintf(dlg->obj_name, "dlg%p", dlg);

    /* Create mutex for the dialog. */
    dlg->mutex = pj_mutex_create(dlg->pool, "mdlg%p", 0);
    if (!dlg->mutex) {
	pjsip_endpt_destroy_pool(ua->endpt, pool);
	return NULL;
    }

    /* Create unique tag for the dialog. */
    pj_create_unique_string( pool, &dlg->local.tag );

    /* Register dialog. */
    pj_mutex_lock(ua->mutex);
    if (ua_register_dialog(ua, dlg, &dlg->local.tag) != PJ_SUCCESS) {
	pj_mutex_unlock(ua->mutex);
	pj_mutex_destroy(dlg->mutex);
	pjsip_endpt_destroy_pool( ua->endpt, pool );
	return NULL;
    }
    pj_mutex_unlock(ua->mutex);

    PJ_LOG(4, (dlg->obj_name, "new %s dialog created", pjsip_role_name(role)));
    return dlg;
}

/*
 * Destroy dialog.
 */
PJ_DEF(void) pjsip_ua_destroy_dialog( pjsip_dlg *dlg )
{
    PJ_LOG(5, (dlg->obj_name, "destroying.."));

    /* Lock dialog's mutex. 
     * Check the mutex validity first since this function can be called
     * on dialog initialization failure (which might be because mutex could not
     * be allocated in the first place).
     */
    if (dlg->mutex) {
	pj_mutex_lock(dlg->mutex);
    }

    /* This must be called while holding dialog's mutex, if any. */
    pjsip_on_dialog_destroyed(dlg);

    /* Lock UA. */
    pj_mutex_lock(dlg->ua->mutex);

    /* Erase from hash table. */
    pj_hash_set( dlg->pool, dlg->ua->dlg_table,
		 dlg->local.tag.ptr, dlg->local.tag.slen, NULL);

    /* Erase from the list. */
    pj_list_erase(dlg);

    /* Unlock UA. */
    pj_mutex_unlock(dlg->ua->mutex);

    /* Unlock mutex. */
    if (dlg->mutex) {
	pj_mutex_unlock(dlg->mutex);
    }

    /* Destroy the pool. */
    pjsip_endpt_destroy_pool( dlg->ua->endpt, dlg->pool);
}

/*
 * Dump user agent state to log file.
 */
PJ_DEF(void) pjsip_ua_dump(pjsip_user_agent *ua)
{
#if PJ_LOG_MAX_LEVEL >= 3
    PJ_LOG(3,(LOG_THIS, "Dumping user agent"));
    PJ_LOG(3,(LOG_THIS, "  Pool capacity=%u, used=%u", 
			pj_pool_get_capacity(ua->pool),
			pj_pool_get_used_size(ua->pool)));
    PJ_LOG(3,(LOG_THIS, "  Number of dialogs=%u", pj_hash_count(ua->dlg_table)));

    if (pj_hash_count(ua->dlg_table)) {
	pjsip_dlg *dlg;

	PJ_LOG(3,(LOG_THIS, "  Dumping dialog list:"));
	dlg = ua->dlg_list.next;
	while (dlg != (pjsip_dlg*) &ua->dlg_list) {
	    PJ_LOG(3, (LOG_THIS, "    %s %s", dlg->obj_name, 
		       pjsip_dlg_state_str(dlg->state)));
	    dlg = dlg->next;
	}
    }
#endif
}


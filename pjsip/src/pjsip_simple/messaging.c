/* $Id$
 *
 */
#include <pjsip_simple/messaging.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_misc.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/guid.h>
#include <pj/string.h>
#include <pj/log.h>
#include <stdio.h>
#include <stdlib.h>

#define THIS_FILE   "messaging"

struct messaging_data
{
    void		     *token;
    pjsip_messaging_cb	     cb;
};

struct pjsip_messaging_session
{
    pj_pool_t		*pool;
    pjsip_endpoint	*endpt;
    pjsip_from_hdr	*from;
    pjsip_to_hdr	*to;
    pjsip_cid_hdr	*call_id;
    pjsip_cseq_hdr	*cseq;
};

static int module_id;
static pjsip_on_new_msg_cb incoming_cb;
static pjsip_method message_method;


/*
 * Set global callback to receive incoming message.
 */
PJ_DEF(pjsip_on_new_msg_cb) 
pjsip_messaging_set_incoming_callback(pjsip_on_new_msg_cb cb)
{
    pjsip_on_new_msg_cb prev_cb = incoming_cb;
    incoming_cb = cb;
    return prev_cb;
}


/*
 * Create an independent message (ie. not associated with a session).
 */
PJ_DEF(pjsip_tx_data*) 
pjsip_messaging_create_msg_from_hdr(pjsip_endpoint *endpt, 
				    const pjsip_uri *target,
				    const pjsip_from_hdr *param_from,
				    const pjsip_to_hdr *param_to, 
				    const pjsip_cid_hdr *param_call_id,
				    int param_cseq, 
				    const pj_str_t *param_text)
{
    return pjsip_endpt_create_request_from_hdr( endpt, &message_method, 
						target,
						param_from, param_to,
						NULL, param_call_id,
						param_cseq, param_text );
}

/*
 * Create independent message from string (instead of from header).
 */
PJ_DEF(pjsip_tx_data*) 
pjsip_messaging_create_msg( pjsip_endpoint *endpt, 
			    const pj_str_t *target,
			    const pj_str_t *param_from,
			    const pj_str_t *param_to, 
			    const pj_str_t *param_call_id,
			    int param_cseq, 
			    const pj_str_t *param_text)
{
    return pjsip_endpt_create_request( endpt, &message_method, target, 
				       param_from, param_to, NULL, param_call_id,
				       param_cseq, param_text);
}

/*
 * Initiate transaction to send outgoing message.
 */
PJ_DEF(pj_status_t) 
pjsip_messaging_send_msg( pjsip_endpoint *endpt, pjsip_tx_data *tdata, 
			  void *token, pjsip_messaging_cb cb )
{
    pjsip_transaction *tsx;
    struct messaging_data *msg_data;

    /* Create transaction. */
    tsx = pjsip_endpt_create_tsx(endpt);
    if (!tsx) {
	pjsip_tx_data_dec_ref(tdata);
	return -1;
    }

    /* Save parameters to messaging data and attach to tsx. */
    msg_data = pj_pool_calloc(tsx->pool, 1, sizeof(struct messaging_data));
    msg_data->cb = cb;
    msg_data->token = token;

    /* Init transaction. */
    tsx->module_data[module_id] = msg_data;
    if (pjsip_tsx_init_uac(tsx, tdata) != 0) {
	pjsip_tx_data_dec_ref(tdata);
	pjsip_endpt_destroy_tsx(endpt, tsx);
	return -1;
    }

    pjsip_endpt_register_tsx(endpt, tsx);

    /* 
     * Instruct transaction to send message.
     * Further events will be received via transaction's event.
     */
    pjsip_tsx_on_tx_msg(tsx, tdata);

    /* Decrement reference counter. */
    pjsip_tx_data_dec_ref(tdata);
    return 0;
}


/*
 * Create 'IM session'.
 */
PJ_DEF(pjsip_messaging_session*) 
pjsip_messaging_create_session( pjsip_endpoint *endpt, const pj_str_t *param_from,
			        const pj_str_t *param_to )
{
    pj_pool_t *pool;
    pjsip_messaging_session *ses;
    pj_str_t tmp, to;

    pool = pjsip_endpt_create_pool(endpt, "imsess", 1024, 1024);
    if (!pool)
	return NULL;

    ses = pj_pool_calloc(pool, 1, sizeof(pjsip_messaging_session));
    ses->pool = pool;
    ses->endpt = endpt;

    ses->call_id = pjsip_cid_hdr_create(pool);
    pj_create_unique_string(pool, &ses->call_id->id);

    ses->cseq = pjsip_cseq_hdr_create(pool);
    ses->cseq->cseq = pj_rand();
    ses->cseq->method = message_method;

    ses->from = pjsip_from_hdr_create(pool);
    pj_strdup_with_null(pool, &tmp, param_from);
    ses->from->uri = pjsip_parse_uri(pool, tmp.ptr, tmp.slen, PJSIP_PARSE_URI_AS_NAMEADDR);
    if (ses->from->uri == NULL) {
	pjsip_endpt_destroy_pool(endpt, pool);
	return NULL;
    }
    pj_create_unique_string(pool, &ses->from->tag);

    ses->to = pjsip_to_hdr_create(pool);
    pj_strdup_with_null(pool, &to, param_from);
    ses->to->uri = pjsip_parse_uri(pool, to.ptr, to.slen, PJSIP_PARSE_URI_AS_NAMEADDR);
    if (ses->to->uri == NULL) {
	pjsip_endpt_destroy_pool(endpt, pool);
	return NULL;
    }

    PJ_LOG(4,(THIS_FILE, "IM session created: recipient=%s", to.ptr));
    return ses;
}


/*
 * Send IM message using identification from 'IM session'.
 */
PJ_DEF(pjsip_tx_data*)
pjsip_messaging_session_create_msg( pjsip_messaging_session *ses, const pj_str_t *text )
{
    return pjsip_endpt_create_request_from_hdr( ses->endpt,
						&message_method,
						ses->to->uri,
						ses->from,
						ses->to,
						NULL,
						ses->call_id,
						ses->cseq->cseq++,
						text);
}


/*
 * Destroy 'IM session'.
 */
PJ_DEF(pj_status_t)
pjsip_messaging_destroy_session( pjsip_messaging_session *ses )
{
    /*
     * NOTE ABOUT POSSIBLE BUG HERE...
     *
     * We don't check number of pending transaction before destroying IM
     * session. As the result, the headers in the txdata of pending transaction
     * wil be INVALID once the IM session is deleted (because we only
     * shallo_clone()-ed them).
     *
     * This normally should be okay, because once the message is
     * submitted to transaction, the transaction (or rather the transport)
     * will 'print' the message to a buffer, and once it is printed, it
     * won't try to access the original message again. So even when the 
     * original message has a dangling pointer, we should be safe.
     *
     * However, it will cause a problem if:
     *	- resolving completes asynchronously and with a substantial delay,
     *	  and before the resolver/transport finished its job the user
     *	  destroy the IM session.
     *	- if the transmit data is invalidated after the IM session is
     *	  destroyed.
     */

    pjsip_endpt_destroy_pool(ses->endpt, ses->pool);
    return 0;
}


static pj_status_t messaging_init( pjsip_endpoint *endpt,
				   struct pjsip_module *mod, pj_uint32_t id )
{
    PJ_UNUSED_ARG(endpt)
    PJ_UNUSED_ARG(mod)

    module_id = id;
    return 0;
}

static pj_status_t messaging_start( struct pjsip_module *mod )
{
    PJ_UNUSED_ARG(mod)
    return 0;
}

static pj_status_t messaging_deinit( struct pjsip_module *mod )
{
    PJ_UNUSED_ARG(mod)
    return 0;
}

static void messaging_tsx_handler( struct pjsip_module *mod, pjsip_event *event )
{
    pjsip_transaction *tsx = event->obj.tsx;
    struct messaging_data *mod_data;

    PJ_UNUSED_ARG(mod)

    /* Ignore non transaction event */
    if (event->type != PJSIP_EVENT_TSX_STATE_CHANGED || tsx == NULL)
	return;

    /* If this is an incoming message, inform application. */
    if (tsx->role == PJSIP_ROLE_UAS) {
	int status = 100;
	pjsip_tx_data *tdata;

	/* Check if we already answered this request. */
	if (tsx->status_code >= 200)
	    return;

	/* Only handle MESSAGE requests!. */
	if (pjsip_method_cmp(&tsx->method, &message_method) != 0)
	    return;

	/* Call application callback. */
	if (incoming_cb)
	    status = (*incoming_cb)(event->src.rdata);

	if (status < 200 || status >= 700)
	    status = PJSIP_SC_INTERNAL_SERVER_ERROR;

	/* Respond request. */
	tdata = pjsip_endpt_create_response(tsx->endpt, event->src.rdata, status );
	if (tdata)
	    pjsip_tsx_on_tx_msg(tsx, tdata);

	return;
    }

    /* Ignore if it's not something that came from messaging module. */
    mod_data = tsx->module_data[ module_id ];
    if (mod_data == NULL)
	return;

    /* Ignore non final response. */
    if (tsx->status_code < 200)
	return;

    /* Don't want to call the callback more than once. */
    tsx->module_data[ module_id ] = NULL;

    /* Now call the callback. */
    if (mod_data->cb) {
	(*mod_data->cb)(mod_data->token, tsx->status_code);
    }
}

static pjsip_module messaging_module = 
{
    { "Messaging", 9},	    /* Name.		*/
    0,			    /* Flag		*/
    128,		    /* Priority		*/
    NULL,		    /* User agent instance, initialized by APP.	*/
    0,			    /* Number of methods supported (will be initialized later). */
    { 0 },		    /* Array of methods (will be initialized later) */
    &messaging_init,	    /* init_module()	*/
    &messaging_start,	    /* start_module()	*/
    &messaging_deinit,	    /* deinit_module()	*/
    &messaging_tsx_handler, /* tsx_handler()	*/
};

PJ_DEF(pjsip_module*) pjsip_messaging_get_module()
{
    static pj_str_t method_str = { "MESSAGE", 7 };

    pjsip_method_init_np( &message_method, &method_str);

    messaging_module.method_cnt = 1;
    messaging_module.methods[0] = &message_method;

    return &messaging_module;
}


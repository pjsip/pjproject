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
#include <pjsip-ua/sip_inv.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_transaction.h>
#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>

#define THIS_FILE	"sip_invite_session.c"

/*
 * Static prototypes.
 */
static pj_status_t mod_inv_load(pjsip_endpoint *endpt);
static pj_status_t mod_inv_unload(void);
static pj_bool_t   mod_inv_on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t   mod_inv_on_rx_response(pjsip_rx_data *rdata);
static void	   mod_inv_on_tsx_state(pjsip_transaction*, pjsip_event*);

static void inv_on_state_null( pjsip_inv_session *s, pjsip_event *e);
static void inv_on_state_calling( pjsip_inv_session *s, pjsip_event *e);
static void inv_on_state_incoming( pjsip_inv_session *s, pjsip_event *e);
static void inv_on_state_early( pjsip_inv_session *s, pjsip_event *e);
static void inv_on_state_connecting( pjsip_inv_session *s, pjsip_event *e);
static void inv_on_state_confirmed( pjsip_inv_session *s, pjsip_event *e);
static void inv_on_state_disconnected( pjsip_inv_session *s, pjsip_event *e);
static void inv_on_state_terminated( pjsip_inv_session *s, pjsip_event *e);

static void (*inv_state_handler[])( pjsip_inv_session *s, pjsip_event *e) = 
{
    &inv_on_state_null,
    &inv_on_state_calling,
    &inv_on_state_incoming,
    &inv_on_state_early,
    &inv_on_state_connecting,
    &inv_on_state_confirmed,
    &inv_on_state_disconnected,
    &inv_on_state_terminated,
};

static struct mod_inv
{
    pjsip_module	 mod;
    pjsip_endpoint	*endpt;
    pjsip_inv_callback	 cb;
    pjsip_module	*app_user;
} mod_inv = 
{
    {
	NULL, NULL,		    /* prev, next.			*/
	{ "mod-invite", 10 },	    /* Name.				*/
	-1,			    /* Id				*/
	PJSIP_MOD_PRIORITY_APPLICATION-1,	/* Priority		*/
	NULL,			    /* User data.			*/
	&mod_inv_load,		    /* load()				*/
	NULL,			    /* start()				*/
	NULL,			    /* stop()				*/
	&mod_inv_unload,	    /* unload()				*/
	&mod_inv_on_rx_request,	    /* on_rx_request()			*/
	&mod_inv_on_rx_response,    /* on_rx_response()			*/
	NULL,			    /* on_tx_request.			*/
	NULL,			    /* on_tx_response()			*/
	&mod_inv_on_tsx_state,	    /* on_tsx_state()			*/
    }
};



static pj_status_t mod_inv_load(pjsip_endpoint *endpt)
{
    pj_str_t allowed[] = {{"INVITE", 6}, {"ACK",3}, {"BYE",3}, {"CANCEL",6}};

    /* Register supported methods: INVITE, ACK, BYE, CANCEL */
    pjsip_endpt_add_capability(endpt, &mod_inv.mod, PJSIP_H_ALLOW, NULL,
			       PJ_ARRAY_SIZE(allowed), allowed);

    return PJ_SUCCESS;
}

static pj_status_t mod_inv_unload(void)
{
    /* Should remove capability here */
    return PJ_SUCCESS;
}

static pj_bool_t mod_inv_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_dialog *dlg;

    /* Ignore requests outside dialog */
    dlg = pjsip_rdata_get_dlg(rdata);
    if (dlg == NULL)
	return PJ_FALSE;

    /* Answer BYE with 200/OK. */
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_BYE_METHOD) {
	pj_status_t status;
	pjsip_tx_data *tdata;

	status = pjsip_dlg_create_response(dlg, rdata, 200, NULL, &tdata);
	if (status == PJ_SUCCESS)
	    status = pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata),
					     tdata);

	return status==PJ_SUCCESS ? PJ_TRUE : PJ_FALSE;
    }

    return PJ_FALSE;
}

static pj_status_t send_ack(pjsip_inv_session *inv, pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    status = pjsip_dlg_create_request(inv->dlg, &pjsip_ack_method, 
				      rdata->msg_info.cseq->cseq, &tdata);
    if (status != PJ_SUCCESS) {
	/* Better luck next time */
	pj_assert(!"Unable to create ACK!");
	return status;
    }

    status = pjsip_dlg_send_request(inv->dlg, tdata, NULL);
    if (status != PJ_SUCCESS) {
	/* Better luck next time */
	pj_assert(!"Unable to send ACK!");
	return status;
    }

    return PJ_SUCCESS;
}

static pj_bool_t mod_inv_on_rx_response(pjsip_rx_data *rdata)
{
    pjsip_dialog *dlg;
    pjsip_inv_session *inv;
    pjsip_msg *msg = rdata->msg_info.msg;

    dlg = pjsip_rdata_get_dlg(rdata);

    /* Ignore responses outside dialog */
    if (dlg == NULL)
	return PJ_FALSE;

    /* Ignore responses not belonging to invite session */
    inv = pjsip_dlg_get_inv_session(dlg);
    if (inv == NULL)
	return PJ_FALSE;

    /* This MAY be retransmission of 2xx response to INVITE. 
     * If it is, we need to send ACK.
     */
    if (msg->type == PJSIP_RESPONSE_MSG && msg->line.status.code/100==2 &&
	rdata->msg_info.cseq->method.id == PJSIP_INVITE_METHOD) {

	send_ack(inv, rdata);
	return PJ_TRUE;

    }

    /* No other processing needs to be done here. */
    return PJ_FALSE;
}

static void mod_inv_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    pjsip_dialog *dlg;
    pjsip_inv_session *inv;

    dlg = pjsip_tsx_get_dlg(tsx);
    if (dlg == NULL)
	return;

    inv = pjsip_dlg_get_inv_session(dlg);
    if (inv == NULL)
	return;

    /* Call state handler for the invite session. */
    (*inv_state_handler[inv->state])(inv, e);

    /* Call on_tsx_state */
    if (mod_inv.cb.on_tsx_state_changed)
	(*mod_inv.cb.on_tsx_state_changed)(inv, tsx, e);

    /* Clear invite transaction when tsx is terminated. */
    if (tsx->state==PJSIP_TSX_STATE_TERMINATED && tsx == inv->invite_tsx)
	inv->invite_tsx = NULL;
}

PJ_DEF(pj_status_t) pjsip_inv_usage_init( pjsip_endpoint *endpt,
					  pjsip_module *app_module,
					  const pjsip_inv_callback *cb)
{
    pj_status_t status;

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && app_module && cb, PJ_EINVAL);

    /* Some callbacks are mandatory */
    PJ_ASSERT_RETURN(cb->on_state_changed && cb->on_new_session, PJ_EINVAL);

    /* Check if module already registered. */
    PJ_ASSERT_RETURN(mod_inv.mod.id == -1, PJ_EINVALIDOP);

    /* Copy param. */
    pj_memcpy(&mod_inv.cb, cb, sizeof(pjsip_inv_callback));

    mod_inv.endpt = endpt;
    mod_inv.app_user = app_module;

    /* Register the module. */
    status = pjsip_endpt_register_module(endpt, &mod_inv.mod);

    return status;
}

PJ_DEF(pjsip_module*) pjsip_inv_usage_instance(void)
{
    return &mod_inv.mod;
}


PJ_DEF(pjsip_inv_session*) pjsip_dlg_get_inv_session(pjsip_dialog *dlg)
{
    return dlg->mod_data[mod_inv.mod.id];
}

/*
 * Create UAC session.
 */
PJ_DEF(pj_status_t) pjsip_inv_create_uac( pjsip_dialog *dlg,
					  const pjmedia_sdp_session *local_sdp,
					  unsigned options,
					  pjsip_inv_session **p_inv)
{
    pjsip_inv_session *inv;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(dlg && p_inv, PJ_EINVAL);

    /* Normalize options */
    if (options & PJSIP_INV_REQUIRE_100REL)
	options |= PJSIP_INV_SUPPORT_100REL;

    if (options & PJSIP_INV_REQUIRE_TIMER)
	options |= PJSIP_INV_SUPPORT_TIMER;

    /* Create the session */
    inv = pj_pool_zalloc(dlg->pool, sizeof(pjsip_inv_session));
    PJ_ASSERT_RETURN(inv != NULL, PJ_ENOMEM);

    inv->pool = dlg->pool;
    inv->role = PJSIP_ROLE_UAC;
    inv->state = PJSIP_INV_STATE_NULL;
    inv->dlg = dlg;
    inv->options = options;

    /* Create negotiator if local_sdp is specified. */
    if (local_sdp) {
	status = pjmedia_sdp_neg_create_w_local_offer(dlg->pool, local_sdp,
						      &inv->neg);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Register invite as dialog usage. */
    status = pjsip_dlg_add_usage(dlg, &mod_inv.mod, inv);
    if (status != PJ_SUCCESS)
	return status;

    /* Increment dialog session */
    pjsip_dlg_inc_session(dlg);

    /* Done */
    *p_inv = inv;
    return PJ_SUCCESS;
}

/*
 * Verify incoming INVITE request.
 */
PJ_DEF(pj_status_t) pjsip_inv_verify_request(pjsip_rx_data *rdata,
					     unsigned *options,
					     const pjmedia_sdp_session *l_sdp,
					     pjsip_dialog *dlg,
					     pjsip_endpoint *endpt,
					     pjsip_tx_data **p_tdata)
{
    pjsip_msg *msg;
    pjsip_allow_hdr *allow;
    pjsip_supported_hdr *sup_hdr;
    pjsip_require_hdr *req_hdr;
    int code = 200;
    unsigned rem_option = 0;
    pj_status_t status = PJ_SUCCESS;
    pjsip_hdr res_hdr_list;

    /* Init return arguments. */
    if (p_tdata) *p_tdata = NULL;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(rdata != NULL && options != NULL, PJ_EINVAL);

    /* Normalize options */
    if (*options & PJSIP_INV_REQUIRE_100REL)
	*options |= PJSIP_INV_SUPPORT_100REL;

    if (*options & PJSIP_INV_REQUIRE_TIMER)
	*options |= PJSIP_INV_SUPPORT_TIMER;

    /* Get the message in rdata */
    msg = rdata->msg_info.msg;

    /* Must be INVITE request. */
    PJ_ASSERT_RETURN(msg->type == PJSIP_REQUEST_MSG &&
		     msg->line.req.method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVAL);

    /* If tdata is specified, then either dlg or endpt must be specified */
    PJ_ASSERT_RETURN((!p_tdata) || (endpt || dlg), PJ_EINVAL);

    /* Get the endpoint */
    endpt = endpt ? endpt : dlg->endpt;

    /* Init response header list */
    pj_list_init(&res_hdr_list);

    /* Check the request body, see if it's something that we support
     * (i.e. SDP). 
     */
    if (msg->body) {
	pjsip_msg_body *body = msg->body;
	pj_str_t str_application = {"application", 11};
	pj_str_t str_sdp = { "sdp", 3 };
	pjmedia_sdp_session *sdp;

	/* Check content type. */
	if (pj_stricmp(&body->content_type.type, &str_application) != 0 ||
	    pj_stricmp(&body->content_type.subtype, &str_sdp) != 0)
	{
	    /* Not "application/sdp" */
	    code = PJSIP_SC_UNSUPPORTED_MEDIA_TYPE;
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

	    if (p_tdata) {
		/* Add Accept header to response */
		pjsip_accept_hdr *acc;

		acc = pjsip_accept_hdr_create(rdata->tp_info.pool);
		PJ_ASSERT_RETURN(acc, PJ_ENOMEM);
		acc->values[acc->count++] = pj_str("application/sdp");
		pj_list_push_back(&res_hdr_list, acc);
	    }

	    goto on_return;
	}

	/* Parse and validate SDP */
	status = pjmedia_sdp_parse(rdata->tp_info.pool, body->data, body->len,
				   &sdp);
	if (status == PJ_SUCCESS)
	    status = pjmedia_sdp_validate(sdp);

	if (status != PJ_SUCCESS) {
	    /* Unparseable or invalid SDP */
	    code = PJSIP_SC_BAD_REQUEST;

	    if (p_tdata) {
		/* Add Warning header. */
		pjsip_warning_hdr *w;

		w = pjsip_warning_hdr_create_from_status(rdata->tp_info.pool,
							 pjsip_endpt_name(endpt),
							 status);
		PJ_ASSERT_RETURN(w, PJ_ENOMEM);

		pj_list_push_back(&res_hdr_list, w);
	    }

	    goto on_return;
	}

	/* Negotiate with local SDP */
	if (l_sdp) {
	    pjmedia_sdp_neg *neg;

	    /* Local SDP must be valid! */
	    PJ_ASSERT_RETURN((status=pjmedia_sdp_validate(l_sdp))==PJ_SUCCESS,
			     status);

	    /* Create SDP negotiator */
	    status = pjmedia_sdp_neg_create_w_remote_offer(
			    rdata->tp_info.pool, l_sdp, sdp, &neg);
	    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

	    /* Negotiate SDP */
	    status = pjmedia_sdp_neg_negotiate(rdata->tp_info.pool, neg, 0);
	    if (status != PJ_SUCCESS) {

		/* Incompatible media */
		code = PJSIP_SC_NOT_ACCEPTABLE_HERE;
		status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

		if (p_tdata) {
		    pjsip_accept_hdr *acc;
		    pjsip_warning_hdr *w;

		    /* Add Warning header. */
		    w = pjsip_warning_hdr_create_from_status(
					    rdata->tp_info.pool, 
					    pjsip_endpt_name(endpt), status);
		    PJ_ASSERT_RETURN(w, PJ_ENOMEM);

		    pj_list_push_back(&res_hdr_list, w);

		    /* Add Accept header to response */
		    acc = pjsip_accept_hdr_create(rdata->tp_info.pool);
		    PJ_ASSERT_RETURN(acc, PJ_ENOMEM);
		    acc->values[acc->count++] = pj_str("application/sdp");
		    pj_list_push_back(&res_hdr_list, acc);

		}

		goto on_return;
	    }
	}
    }

    /* Check supported methods, see if peer supports UPDATE.
     * We just assume that peer supports standard INVITE, ACK, CANCEL, and BYE
     * implicitly by sending this INVITE.
     */
    allow = pjsip_msg_find_hdr(msg, PJSIP_H_ALLOW, NULL);
    if (allow) {
	unsigned i;
	const pj_str_t STR_UPDATE = { "UPDATE", 6 };

	for (i=0; i<allow->count; ++i) {
	    if (pj_stricmp(&allow->values[i], &STR_UPDATE)==0)
		break;
	}

	if (i != allow->count) {
	    /* UPDATE is present in Allow */
	    rem_option |= PJSIP_INV_SUPPORT_UPDATE;
	}

    }

    /* Check Supported header */
    sup_hdr = pjsip_msg_find_hdr(msg, PJSIP_H_SUPPORTED, NULL);
    if (sup_hdr) {
	unsigned i;
	pj_str_t STR_100REL = { "100rel", 6};
	pj_str_t STR_TIMER = { "timer", 5 };

	for (i=0; i<sup_hdr->count; ++i) {
	    if (pj_stricmp(&sup_hdr->values[i], &STR_100REL)==0)
		rem_option |= PJSIP_INV_SUPPORT_100REL;
	    else if (pj_stricmp(&sup_hdr->values[i], &STR_TIMER)==0)
		rem_option |= PJSIP_INV_SUPPORT_TIMER;
	}
    }

    /* Check Require header */
    req_hdr = pjsip_msg_find_hdr(msg, PJSIP_H_REQUIRE, NULL);
    if (req_hdr) {
	unsigned i;
	pj_str_t STR_100REL = { "100rel", 6};
	pj_str_t STR_TIMER = { "timer", 5 };
	unsigned unsupp_cnt = 0;
	pj_str_t unsupp_tags[PJSIP_GENERIC_ARRAY_MAX_COUNT];
	
	for (i=0; i<req_hdr->count; ++i) {
	    if ((*options & PJSIP_INV_SUPPORT_100REL) && 
		pj_stricmp(&req_hdr->values[i], &STR_100REL)==0)
	    {
		rem_option |= PJSIP_INV_REQUIRE_100REL;

	    } else if ((*options && PJSIP_INV_SUPPORT_TIMER) &&
		       pj_stricmp(&req_hdr->values[i], &STR_TIMER)==0)
	    {
		rem_option |= PJSIP_INV_REQUIRE_TIMER;

	    } else {
		/* Unknown/unsupported extension tag!  */
		unsupp_tags[unsupp_cnt++] = req_hdr->values[i];
	    }
	}

	/* Check if there are required tags that we don't support */
	if (unsupp_cnt) {

	    code = PJSIP_SC_BAD_EXTENSION;
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

	    if (p_tdata) {
		pjsip_unsupported_hdr *unsupp_hdr;
		const pjsip_hdr *h;

		/* Add Unsupported header. */
		unsupp_hdr = pjsip_unsupported_hdr_create(rdata->tp_info.pool);
		PJ_ASSERT_RETURN(unsupp_hdr != NULL, PJ_ENOMEM);

		unsupp_hdr->count = unsupp_cnt;
		for (i=0; i<unsupp_cnt; ++i)
		    unsupp_hdr->values[i] = unsupp_tags[i];

		pj_list_push_back(&res_hdr_list, unsupp_hdr);

		/* Add Supported header. */
		h = pjsip_endpt_get_capability(endpt, PJSIP_H_SUPPORTED, 
					       NULL);
		pj_assert(h);
		if (h) {
		    sup_hdr = pjsip_hdr_clone(rdata->tp_info.pool, h);
		    pj_list_push_back(&res_hdr_list, sup_hdr);
		}
	    }

	    goto on_return;
	}
    }

    /* Check if there are local requirements that are not supported
     * by peer.
     */
    if ( ((*options & PJSIP_INV_REQUIRE_100REL)!=0 && 
	  (rem_option & PJSIP_INV_SUPPORT_100REL)==0) ||
	 ((*options & PJSIP_INV_REQUIRE_TIMER)!=0 &&
	  (rem_option & PJSIP_INV_SUPPORT_TIMER)==0))
    {
	code = PJSIP_SC_EXTENSION_REQUIRED;
	status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

	if (p_tdata) {
	    const pjsip_hdr *h;

	    /* Add Require header. */
	    req_hdr = pjsip_require_hdr_create(rdata->tp_info.pool);
	    PJ_ASSERT_RETURN(req_hdr != NULL, PJ_ENOMEM);

	    if (*options & PJSIP_INV_REQUIRE_100REL)
		req_hdr->values[req_hdr->count++] = pj_str("100rel");

	    if (*options & PJSIP_INV_REQUIRE_TIMER)
		req_hdr->values[req_hdr->count++] = pj_str("timer");

	    pj_list_push_back(&res_hdr_list, req_hdr);

	    /* Add Supported header. */
	    h = pjsip_endpt_get_capability(endpt, PJSIP_H_SUPPORTED, 
					   NULL);
	    pj_assert(h);
	    if (h) {
		sup_hdr = pjsip_hdr_clone(rdata->tp_info.pool, h);
		pj_list_push_back(&res_hdr_list, sup_hdr);
	    }

	}

	goto on_return;
    }

on_return:

    /* Create response if necessary */
    if (code != 200 && p_tdata) {
	pjsip_tx_data *tdata;
	const pjsip_hdr *h;

	if (dlg) {
	    status = pjsip_dlg_create_response(dlg, rdata, code, NULL, 
					       &tdata);
	} else {
	    status = pjsip_endpt_create_response(endpt, rdata, code, NULL, 
						 &tdata);
	}

	if (status != PJ_SUCCESS)
	    return status;

	/* Add response headers. */
	h = res_hdr_list.next;
	while (h != &res_hdr_list) {
	    pjsip_hdr *cloned;

	    cloned = pjsip_hdr_clone(tdata->pool, h);
	    PJ_ASSERT_RETURN(cloned, PJ_ENOMEM);

	    pjsip_msg_add_hdr(tdata->msg, cloned);

	    h = h->next;
	}

	*p_tdata = tdata;
    }

    return status;
}

/*
 * Create UAS session.
 */
PJ_DEF(pj_status_t) pjsip_inv_create_uas( pjsip_dialog *dlg,
					  pjsip_rx_data *rdata,
					  const pjmedia_sdp_session *local_sdp,
					  unsigned options,
					  pjsip_inv_session **p_inv)
{
    pjsip_inv_session *inv;
    pjsip_msg *msg;
    pjmedia_sdp_session *rem_sdp = NULL;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(dlg && rdata && p_inv, PJ_EINVAL);

    /* Dialog MUST have been initialised. */
    PJ_ASSERT_RETURN(pjsip_rdata_get_tsx(rdata) != NULL, PJ_EINVALIDOP);

    msg = rdata->msg_info.msg;

    /* rdata MUST contain INVITE request */
    PJ_ASSERT_RETURN(msg->type == PJSIP_REQUEST_MSG &&
		     msg->line.req.method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVALIDOP);

    /* Normalize options */
    if (options & PJSIP_INV_REQUIRE_100REL)
	options |= PJSIP_INV_SUPPORT_100REL;

    if (options & PJSIP_INV_REQUIRE_TIMER)
	options |= PJSIP_INV_SUPPORT_TIMER;

    /* Create the session */
    inv = pj_pool_zalloc(dlg->pool, sizeof(pjsip_inv_session));
    PJ_ASSERT_RETURN(inv != NULL, PJ_ENOMEM);

    inv->pool = dlg->pool;
    inv->role = PJSIP_ROLE_UAS;
    inv->state = PJSIP_INV_STATE_NULL;
    inv->dlg = dlg;
    inv->options = options;

    /* Parse SDP in message body, if present. */
    if (msg->body) {
	pjsip_msg_body *body = msg->body;

	/* Parse and validate SDP */
	status = pjmedia_sdp_parse(inv->pool, body->data, body->len,
				   &rem_sdp);
	if (status == PJ_SUCCESS)
	    status = pjmedia_sdp_validate(rem_sdp);

	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Create negotiator. */
    if (rem_sdp) {
	status = pjmedia_sdp_neg_create_w_remote_offer(inv->pool, local_sdp,
						       rem_sdp, &inv->neg);
						
    } else if (local_sdp) {
	status = pjmedia_sdp_neg_create_w_local_offer(inv->pool, local_sdp,
						      &inv->neg);
    } else {
	pj_assert(!"UAS dialog without remote and local offer is not supported!");
	PJ_TODO(IMPLEMENT_DELAYED_UAS_OFFER);
	status = PJ_ENOTSUP;
    }

    if (status != PJ_SUCCESS)
	return status;

    /* Register invite as dialog usage. */
    status = pjsip_dlg_add_usage(dlg, &mod_inv.mod, inv);
    if (status != PJ_SUCCESS)
	return status;

    /* Increment session in the dialog. */
    pjsip_dlg_inc_session(dlg);

    /* Save the invite transaction. */
    inv->invite_tsx = pjsip_rdata_get_tsx(rdata);
    inv->invite_tsx->mod_data[mod_inv.mod.id] = inv;

    /* Done */
    *p_inv = inv;
    return PJ_SUCCESS;
}

static void *clone_sdp(pj_pool_t *pool, const void *data, unsigned len)
{
    PJ_UNUSED_ARG(len);
    return pjmedia_sdp_session_clone(pool, data);
}

static int print_sdp(pjsip_msg_body *body, char *buf, pj_size_t len)
{
    return pjmedia_sdp_print(body->data, buf, len);
}

static pjsip_msg_body *create_sdp_body(pj_pool_t *pool,
				       const pjmedia_sdp_session *c_sdp)
{
    pjsip_msg_body *body;


    body = pj_pool_zalloc(pool, sizeof(pjsip_msg_body));
    PJ_ASSERT_RETURN(body != NULL, NULL);

    body->content_type.type = pj_str("application");
    body->content_type.subtype = pj_str("sdp");
    body->data = pjmedia_sdp_session_clone(pool, c_sdp);
    body->len = 0;
    body->clone_data = &clone_sdp;
    body->print_body = &print_sdp;

    return body;
}

/*
 * Create initial INVITE request.
 */
PJ_DEF(pj_status_t) pjsip_inv_invite( pjsip_inv_session *inv,
				      pjsip_tx_data **p_tdata )
{
    pjsip_tx_data *tdata;
    const pjsip_hdr *hdr;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* State MUST be NULL. */
    PJ_ASSERT_RETURN(inv->state == PJSIP_INV_STATE_NULL, PJ_EINVAL);

    /* Create the INVITE request. */
    status = pjsip_dlg_create_request(inv->dlg, &pjsip_invite_method, -1,
				      &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add SDP, if any. */
    if (inv->neg && 
	pjmedia_sdp_neg_get_state(inv->neg)==PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER)
    {
	const pjmedia_sdp_session *offer;

	status = pjmedia_sdp_neg_get_neg_local(inv->neg, &offer);
	if (status != PJ_SUCCESS)
	    return status;

	tdata->msg->body = create_sdp_body(tdata->pool, offer);
    }

    /* Add Allow header. */
    hdr = pjsip_endpt_get_capability(inv->dlg->endpt, PJSIP_H_ALLOW, NULL);
    if (hdr) {
	pjsip_msg_add_hdr(tdata->msg,
			  pjsip_hdr_shallow_clone(tdata->pool, hdr));
    }

    /* Add Supported header */
    hdr = pjsip_endpt_get_capability(inv->dlg->endpt, PJSIP_H_SUPPORTED, NULL);
    if (hdr) {
	pjsip_msg_add_hdr(tdata->msg,
			  pjsip_hdr_shallow_clone(tdata->pool, hdr));
    }

    /* Add Require header. */
    PJ_TODO(INVITE_ADD_REQUIRE_HEADER);

    /* Done. */
    *p_tdata = tdata;

    return PJ_SUCCESS;
}


/*
 * Answer initial INVITE.
 */ 
PJ_DEF(pj_status_t) pjsip_inv_answer(	pjsip_inv_session *inv,
					int st_code,
					const pj_str_t *st_text,
					const pjmedia_sdp_session *local_sdp,
					pjsip_tx_data **p_tdata )
{
    pjsip_tx_data *last_res;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* Must have INVITE transaction. */
    PJ_ASSERT_RETURN(inv->invite_tsx, PJ_EBUG);

    /* INVITE transaction MUST have transmitted a response (e.g. 100) */
    PJ_ASSERT_RETURN(inv->invite_tsx->last_tx, PJ_EINVALIDOP);

    /* If local_sdp is specified, then we MUST NOT have answered the
     * offer before. 
     */
    PJ_ASSERT_RETURN(!local_sdp ||
		     (pjmedia_sdp_neg_get_state(inv->neg)==PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER),
		     PJ_EINVALIDOP);
    if (local_sdp) {
	status = pjmedia_sdp_neg_set_local_answer(inv->pool, inv->neg,
						  local_sdp);
	if (status != PJ_SUCCESS)
	    return status;
    }

    last_res = inv->invite_tsx->last_tx;

    /* Modify last response. */
    status = pjsip_dlg_modify_response(inv->dlg, last_res, st_code, st_text);
    if (status != PJ_SUCCESS)
	return status;

    /* Include SDP for 18x and 2xx response. */
    if (st_code/10 == 18 || st_code/10 == 20) {
	const pjmedia_sdp_session *local;

	status = pjmedia_sdp_neg_get_neg_local(inv->neg, &local);
	if (status == PJ_SUCCESS)
	    last_res->msg->body = create_sdp_body(last_res->pool, local);
					      ;
    }

    /* Do we need to increment tdata's reference counter? */
    PJ_TODO(INV_ANSWER_MAY_HAVE_TO_INCREMENT_REF_COUNTER);

    *p_tdata = last_res;

    return PJ_SUCCESS;
}


/*
 * End session.
 */
PJ_DEF(pj_status_t) pjsip_inv_end_session(  pjsip_inv_session *inv,
					    int st_code,
					    const pj_str_t *st_text,
					    pjsip_tx_data **p_tdata )
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* Create appropriate message. */
    switch (inv->state) {
    case PJSIP_INV_STATE_CALLING:
    case PJSIP_INV_STATE_EARLY:
    case PJSIP_INV_STATE_INCOMING:

	if (inv->role == PJSIP_ROLE_UAC) {

	    /* For UAC when session has not been confirmed, create CANCEL. */

	    /* MUST have the original UAC INVITE transaction. */
	    PJ_ASSERT_RETURN(inv->invite_tsx != NULL, PJ_EBUG);

	    /* But CANCEL should only be called when we have received a
	     * provisional response. If we haven't received any responses,
	     * just destroy the transaction.
	     */
	    if (inv->invite_tsx->status_code < 100) {

		pjsip_tsx_terminate(inv->invite_tsx, 487);

		return PJSIP_ETSXDESTROYED;
	    }

	    /* The CSeq here assumes that the dialog is started with an
	     * INVITE session. This may not be correct; dialog can be 
	     * started as SUBSCRIBE session.
	     * So fix this!
	     */
	    status = pjsip_endpt_create_cancel(inv->dlg->endpt, 
					       inv->invite_tsx->last_tx,
					       &tdata);

	} else {

	    /* For UAS, send a final response. */
	    tdata = inv->invite_tsx->last_tx;
	    PJ_ASSERT_RETURN(tdata != NULL, PJ_EINVALIDOP);

	    status = pjsip_dlg_modify_response(inv->dlg, tdata, st_code,
					       st_text);
	}
	break;

    case PJSIP_INV_STATE_CONNECTING:
    case PJSIP_INV_STATE_CONFIRMED:
	/* For established dialog, send BYE */
	status = pjsip_dlg_create_request(inv->dlg, &pjsip_bye_method, -1, 
					  &tdata);
	break;

    case PJSIP_INV_STATE_DISCONNECTED:
	/* No need to do anything. */
	PJ_TODO(RETURN_A_PROPER_STATUS_CODE_HERE);
	return PJ_EINVALIDOP;

    default:
	pj_assert("!Invalid operation!");
	return PJ_EINVALIDOP;
    }

    if (status != PJ_SUCCESS)
	return status;


    /* Done */

    *p_tdata = tdata;

    return PJ_SUCCESS;
}


/*
 * Create re-INVITE.
 */
PJ_DEF(pj_status_t) pjsip_inv_reinvite( pjsip_inv_session *inv,
					const pj_str_t *new_contact,
					const pjmedia_sdp_session *new_offer,
					pjsip_tx_data **p_tdata )
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(new_contact);
    PJ_UNUSED_ARG(new_offer);
    PJ_UNUSED_ARG(p_tdata);

    PJ_TODO(CREATE_REINVITE_REQUEST);
    return PJ_ENOTSUP;
}

/*
 * Create UPDATE.
 */
PJ_DEF(pj_status_t) pjsip_inv_update (	pjsip_inv_session *inv,
					const pj_str_t *new_contact,
					const pjmedia_sdp_session *new_offer,
					pjsip_tx_data **p_tdata )
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(new_contact);
    PJ_UNUSED_ARG(new_offer);
    PJ_UNUSED_ARG(p_tdata);

    PJ_TODO(CREATE_UPDATE_REQUEST);
    return PJ_ENOTSUP;
}

/*
 * Send a request or response message.
 */
PJ_DEF(pj_status_t) pjsip_inv_send_msg( pjsip_inv_session *inv,
					pjsip_tx_data *tdata,
					void *token )
{
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && tdata, PJ_EINVAL);

    if (tdata->msg->type == PJSIP_REQUEST_MSG) {
	pjsip_transaction *tsx;

	status = pjsip_dlg_send_request(inv->dlg, tdata, &tsx);
	if (status != PJ_SUCCESS)
	    return status;

	tsx->mod_data[mod_inv.mod.id] = inv;
	tsx->mod_data[mod_inv.app_user->id] = token;

    } else {
	pjsip_cseq_hdr *cseq;

	/* Can only do this to send response to original INVITE
	 * request.
	 */
	PJ_ASSERT_RETURN((cseq=pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL)) != NULL &&
			 (cseq->cseq == inv->invite_tsx->cseq),
			 PJ_EINVALIDOP);

	status = pjsip_dlg_send_response(inv->dlg, inv->invite_tsx, tdata);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Done (?) */
    return PJ_SUCCESS;
}


void inv_set_state(pjsip_inv_session *inv, pjsip_inv_state state,
		   pjsip_event *e)
{
    inv->state = state;
    if (mod_inv.cb.on_state_changed)
	(*mod_inv.cb.on_state_changed)(inv, e);

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED)
	pjsip_dlg_dec_session(inv->dlg);
}

static void inv_on_state_null( pjsip_inv_session *s, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

    if (tsx->method.id == PJSIP_INVITE_METHOD) {

	if (dlg->role == PJSIP_ROLE_UAC) {

	    /* Keep the initial INVITE transaction. */
	    if (s->invite_tsx == NULL)
		s->invite_tsx = tsx;

	    switch (tsx->state) {
	    case PJSIP_TSX_STATE_CALLING:
		inv_set_state(s, PJSIP_INV_STATE_CALLING, e);
		break;
	    default:
		pj_assert(!"Unexpected state");
		break;
	    }

	} else {
	    switch (tsx->state) {
	    case PJSIP_TSX_STATE_TRYING:
		inv_set_state(s, PJSIP_INV_STATE_INCOMING, e);
		break;
	    default:
		pj_assert(!"Unexpected state");
	    }
	}

    } else {
	pj_assert(!"Unexpected transaction type");
    }
}

static void inv_on_state_calling( pjsip_inv_session *s, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);
    pj_status_t status;

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);
    
    if (tsx == s->invite_tsx) {

	switch (tsx->state) {

	case PJSIP_TSX_STATE_PROCEEDING:
	    if (dlg->remote.info->tag.slen) {
		inv_set_state(s, PJSIP_INV_STATE_EARLY, e);
	    } else {
		/* Ignore 100 (Trying) response, as it doesn't change
		 * session state. It only ceases retransmissions.
		 */
	    }
	    break;

	case PJSIP_TSX_STATE_COMPLETED:
	    if (tsx->status_code/100 == 2) {
		
		/* This should not happen.
		 * When transaction receives 2xx, it should be terminated
		 */
		pj_assert(0);
		inv_set_state(s, PJSIP_INV_STATE_CONNECTING, e);

	    } else if (tsx->status_code==401 || tsx->status_code==407) {

		/* Handle authentication failure:
		 * Resend the request with Authorization header.
		 */
		pjsip_tx_data *tdata;

		status = pjsip_auth_clt_reinit_req(&s->dlg->auth_sess, 
						   e->body.tsx_state.src.rdata,
						   tsx->last_tx,
						   &tdata);

		if (status != PJ_SUCCESS) {

		    /* Does not have proper credentials. 
		     * End the session.
		     */
		    inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);

		} else {

		    /* Restart session. */
		    s->state = PJSIP_INV_STATE_NULL;
		    s->invite_tsx = NULL;

		    /* Send the request. */
		    status = pjsip_inv_send_msg(s, tdata, NULL );
		}

	    } else {

		inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);

	    }
	    break;

	case PJSIP_TSX_STATE_TERMINATED:
	    /* INVITE transaction can be terminated either because UAC
	     * transaction received 2xx response or because of transport
	     * error.
	     */
	    if (tsx->status_code/100 == 2) {
		/* This must be receipt of 2xx response */

		/* Set state to CONNECTING */
		inv_set_state(s, PJSIP_INV_STATE_CONNECTING, e);

		/* Send ACK */
		pj_assert(e->body.tsx_state.type == PJSIP_EVENT_RX_MSG);

		send_ack(s, e->body.tsx_state.src.rdata);

	    } else  {
		inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);
	    }
	    break;

	default:
	    pj_assert(!"Unexpected state");
	}

    } else {
	pj_assert(!"Unexpected transaction type");
    }
}

static void inv_on_state_incoming( pjsip_inv_session *s, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

    if (tsx == s->invite_tsx) {
	switch (tsx->state) {
	case PJSIP_TSX_STATE_PROCEEDING:
	    if (tsx->status_code > 100)
		inv_set_state(s, PJSIP_INV_STATE_EARLY, e);
	    break;
	case PJSIP_TSX_STATE_COMPLETED:
	    if (tsx->status_code/100 == 2)
		inv_set_state(s, PJSIP_INV_STATE_CONNECTING, e);
	    else
		inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);
	    break;
	case PJSIP_TSX_STATE_TERMINATED:
	    /* This happens on transport error */
	    inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);
	    break;
	default:
	    pj_assert(!"Unexpected state");
	}
    } else {
	pj_assert(!"Unexpected transaction type");
    }
}

static void inv_on_state_early( pjsip_inv_session *s, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

    if (tsx == s->invite_tsx) {

	switch (tsx->state) {

	case PJSIP_TSX_STATE_PROCEEDING:
	    /* Send/received another provisional response. */
	    inv_set_state(s, PJSIP_INV_STATE_EARLY, e);
	    break;

	case PJSIP_TSX_STATE_COMPLETED:
	    if (tsx->status_code/100 == 2)
		inv_set_state(s, PJSIP_INV_STATE_CONNECTING, e);
	    else
		inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);
	    break;

	case PJSIP_TSX_STATE_TERMINATED:
	    /* INVITE transaction can be terminated either because UAC
	     * transaction received 2xx response or because of transport
	     * error.
	     */
	    if (tsx->status_code/100 == 2) {

		/* This must be receipt of 2xx response */

		/* Set state to CONNECTING */
		inv_set_state(s, PJSIP_INV_STATE_CONNECTING, e);

		/* if UAC, send ACK and move state to confirmed. */
		if (tsx->role == PJSIP_ROLE_UAC) {
		    pj_assert(e->body.tsx_state.type == PJSIP_EVENT_RX_MSG);

		    send_ack(s, e->body.tsx_state.src.rdata);

		    inv_set_state(s, PJSIP_INV_STATE_CONFIRMED, e);
		}

	    } else  {
		inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);
	    }
	    break;

	default:
	    pj_assert(!"Unexpected state");
	}

    } else if (tsx->method.id == PJSIP_CANCEL_METHOD) {

	/* Handle incoming CANCEL request. */
	if (tsx->role == PJSIP_ROLE_UAS && s->role == PJSIP_ROLE_UAS &&
	    e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) 
	{

	    pj_status_t status;
	    pjsip_tx_data *tdata;

	    /* Respond CANCEL with 200 (OK) */
	    status = pjsip_dlg_create_response(dlg, e->body.tsx_state.src.rdata,
					       200, NULL, &tdata);
	    if (status == PJ_SUCCESS) {
		pjsip_dlg_send_response(dlg, tsx, tdata);
	    }
    
	    /* Respond the original UAS transaction with 487 (Request 
	     * Terminated) response.
	     */
	    pj_assert(s->invite_tsx && s->invite_tsx->last_tx);
	    tdata = s->invite_tsx->last_tx;

	    status = pjsip_dlg_modify_response(dlg, tdata, 487, NULL);
	    if (status == PJ_SUCCESS) {
		pjsip_dlg_send_response(dlg, s->invite_tsx, tdata);
	    }
	}

    } else if (tsx->method.id == PJSIP_INVITE_METHOD) {

	/* Ignore previously failed INVITE transaction event
	 * (e.g. when rejected with 401/407)
	 */

    } else {
	pj_assert(!"Unexpected transaction type");
    }
}

static void inv_on_state_connecting( pjsip_inv_session *s, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

    if (tsx == s->invite_tsx) {

	switch (tsx->state) {

	case PJSIP_TSX_STATE_CONFIRMED:
	    break;

	case PJSIP_TSX_STATE_TERMINATED:
	    /* INVITE transaction can be terminated either because UAC
	     * transaction received 2xx response or because of transport
	     * error.
	     */
	    if (tsx->status_code/100 != 2) {
		inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);
	    }
	    break;

	case PJSIP_TSX_STATE_DESTROYED:
	    /* Do nothing. */
	    break;

	default:
	    pj_assert(!"Unexpected state");
	}

    } else {
	pj_assert(!"Unexpected transaction type");
    }
}

static void inv_on_state_confirmed( pjsip_inv_session *s, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

    if (tsx->method.id == PJSIP_BYE_METHOD) {
	inv_set_state(s, PJSIP_INV_STATE_DISCONNECTED, e);

    } else if (tsx == s->invite_tsx) {
	
	switch (tsx->state) {
	case PJSIP_TSX_STATE_TERMINATED:
	case PJSIP_TSX_STATE_DESTROYED:
	    break;
	default:
	    pj_assert(!"Unexpected state");
	    break;
	}

    } else if (tsx->method.id == PJSIP_INVITE_METHOD) {

	/* Re-INVITE */

    } else {
	pj_assert(!"Unexpected transaction type");
    }
}

static void inv_on_state_disconnected( pjsip_inv_session *s, pjsip_event *e)
{
    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(e);
}

static void inv_on_state_terminated( pjsip_inv_session *s, pjsip_event *e)
{
    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(e);
}


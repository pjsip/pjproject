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
#include <pjsua-lib/pjsua.h>
#include <pj/log.h>
#include "pjsua_imp.h"

/*
 * pjsua_im.c
 *
 * To handle incoming MESSAGE outside dialog.
 * Incoming MESSAGE inside dialog is hanlded in pjsua_call.c.
 */

#define THIS_FILE   "pjsua_im.c"


/* Declare MESSAGE method */
/* We put PJSIP_MESSAGE_METHOD as the enum here, so that when
 * somebody add that method into pjsip_method_e in sip_msg.h we
 * will get an error here.
 */
enum
{
    PJSIP_MESSAGE_METHOD = PJSIP_OTHER_METHOD
};

const pjsip_method pjsip_message_method =
{
    PJSIP_MESSAGE_METHOD,
    { "MESSAGE", 7 }
};


/* Proto */
static pj_bool_t im_on_rx_request(pjsip_rx_data *rdata);

/* The module instance. */
static pjsip_module mod_pjsua_im = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-im", 12 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &im_on_rx_request,			/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};


/* MIME constants. */
static const pj_str_t STR_MIME_APP	   = { "application", 11 };
static const pj_str_t STR_MIME_ISCOMPOSING = { "im-iscomposing+xml", 18 };
static const pj_str_t STR_MIME_TEXT	   = { "text", 4 };
static const pj_str_t STR_MIME_PLAIN	   = { "plain", 5 };


/* Check if content type is acceptable */
static pj_bool_t acceptable_message(const pjsip_media_type *mime)
{
    return (pj_stricmp(&mime->type, &STR_MIME_TEXT)==0 &&
	    pj_stricmp(&mime->subtype, &STR_MIME_PLAIN)==0)
	    ||
	   (pj_stricmp(&mime->type, &STR_MIME_APP)==0 &&
	    pj_stricmp(&mime->subtype, &STR_MIME_ISCOMPOSING)==0);
}


/**
 * Create Accept header for MESSAGE.
 */
pjsip_accept_hdr* pjsua_im_create_accept(pj_pool_t *pool)
{
    /* Create Accept header. */
    pjsip_accept_hdr *accept;

    accept = pjsip_accept_hdr_create(pool);
    accept->values[0] = pj_str("text/plain");
    accept->values[1] = pj_str("application/im-iscomposing+xml");
    accept->count = 2;

    return accept;
}

/**
 * Private: check if we can accept the message.
 */
pj_bool_t pjsua_im_accept_pager(pjsip_rx_data *rdata,
				pjsip_accept_hdr **p_accept_hdr)
{
    pjsip_ctype_hdr *ctype;
    pjsip_msg *msg;

    msg = rdata->msg_info.msg;

    /* Request MUST have message body, with Content-Type equal to
     * "text/plain".
     */
    ctype = pjsip_msg_find_hdr(msg, PJSIP_H_CONTENT_TYPE, NULL);
    if (msg->body == NULL || ctype == NULL || 
	!acceptable_message(&ctype->media)) 
    {
	/* Create Accept header. */
	if (p_accept_hdr)
	    *p_accept_hdr = pjsua_im_create_accept(rdata->tp_info.pool);

	return PJ_FALSE;
    }

    return PJ_TRUE;
}

/**
 * Private: process pager message.
 *	    This may trigger pjsua_ui_on_pager() or pjsua_ui_on_typing().
 */
void pjsua_im_process_pager(int call_index, const pj_str_t *from,
			    const pj_str_t *to, pjsip_rx_data *rdata)
{
    pjsip_msg_body *body = rdata->msg_info.msg->body;

    /* Body MUST have been checked before */
    pj_assert(body != NULL);

    if (pj_stricmp(&body->content_type.type, &STR_MIME_TEXT)==0 &&
	pj_stricmp(&body->content_type.subtype, &STR_MIME_PLAIN)==0)
    {
	pj_str_t text;

	/* Build the text. */
	text.ptr = rdata->msg_info.msg->body->data;
	text.slen = rdata->msg_info.msg->body->len;

	if (pjsua.cb.on_pager)
	    (*pjsua.cb.on_pager)(call_index, from, to, &text);

    } else {

	/* Expecting typing indication */

	pj_status_t status;
	pj_bool_t is_typing;

	status = pjsip_iscomposing_parse( rdata->tp_info.pool, body->data,
					  body->len, &is_typing, NULL, NULL,
					  NULL );
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Invalid MESSAGE body", status);
	    return;
	}

	if (pjsua.cb.on_typing)
	    (*pjsua.cb.on_typing)(call_index, from, to, is_typing);
    }

}


/*
 * Handler to receive incoming MESSAGE
 */
static pj_bool_t im_on_rx_request(pjsip_rx_data *rdata)
{
    pj_str_t from, to;
    pjsip_accept_hdr *accept_hdr;
    pjsip_contact_hdr *contact_hdr;
    pjsip_msg *msg;
    pj_status_t status;

    msg = rdata->msg_info.msg;

    /* Only want to handle MESSAGE requests. */
    if (pjsip_method_cmp(&msg->line.req.method, &pjsip_message_method) != 0) {
	return PJ_FALSE;
    }


    /* Should not have any transaction attached to rdata. */
    PJ_ASSERT_RETURN(pjsip_rdata_get_tsx(rdata)==NULL, PJ_FALSE);

    /* Should not have any dialog attached to rdata. */
    PJ_ASSERT_RETURN(pjsip_rdata_get_dlg(rdata)==NULL, PJ_FALSE);

    /* Check if we can accept the message. */
    if (!pjsua_im_accept_pager(rdata, &accept_hdr)) {
	pjsip_hdr hdr_list;

	pj_list_init(&hdr_list);
	pj_list_push_back(&hdr_list, accept_hdr);

	pjsip_endpt_respond_stateless(pjsua.endpt, rdata, 
				      PJSIP_SC_NOT_ACCEPTABLE_HERE, NULL, 
				      &hdr_list, NULL);
	return PJ_TRUE;
    }
    
    /* Respond with 200 first, so that remote doesn't retransmit in case
     * the UI takes too long to process the message. 
     */
    status = pjsip_endpt_respond( pjsua.endpt, NULL, rdata, 200, NULL,
				  NULL, NULL, NULL);

    /* For the source URI, we use Contact header if present, since
     * Contact header contains the port number information. If this is
     * not available, then use From header.
     */
    from.ptr = pj_pool_alloc(rdata->tp_info.pool, PJSIP_MAX_URL_SIZE);
    contact_hdr = pjsip_msg_find_hdr(rdata->msg_info.msg,
				     PJSIP_H_CONTACT, NULL);
    if (contact_hdr) {
	from.slen = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
				    contact_hdr->uri, 
				    from.ptr, PJSIP_MAX_URL_SIZE);
    } else {
	from.slen = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, 
				    rdata->msg_info.from->uri,
				    from.ptr, PJSIP_MAX_URL_SIZE);
    }

    if (from.slen < 1)
	from = pj_str("<--URI is too long-->");

    /* Build the To text. */
    to.ptr = pj_pool_alloc(rdata->tp_info.pool, PJSIP_MAX_URL_SIZE);
    to.slen = pjsip_uri_print( PJSIP_URI_IN_FROMTO_HDR, 
			       rdata->msg_info.to->uri,
			       to.ptr, PJSIP_MAX_URL_SIZE);
    if (to.slen < 1)
	to = pj_str("<--URI is too long-->");

    /* Process pager. */
    pjsua_im_process_pager(-1, &from, &to, rdata);

    /* Done. */
    return PJ_TRUE;
}


/* Outgoing IM callback. */
static void im_callback(void *token, pjsip_event *e)
{
    pj_str_t *text = token;

    if (e->type == PJSIP_EVENT_TSX_STATE) {

	pjsip_transaction *tsx = e->body.tsx_state.tsx;

	if (tsx->status_code/100 == 2) {
	    PJ_LOG(4,(THIS_FILE, 
		      "Message \'%s\' delivered successfully",
		      text->ptr));
	} else {
	    PJ_LOG(3,(THIS_FILE, 
		      "Failed to deliver message \'%s\': %s [st_code=%d]",
		      text->ptr,
		      pjsip_get_status_text(tsx->status_code)->ptr,
		      tsx->status_code));
	}
    }
}


/**
 * Send IM outside dialog.
 */
PJ_DEF(pj_status_t) pjsua_im_send(int acc_index, const pj_str_t *dst_uri, 
				  const pj_str_t *str)
{
    pjsip_tx_data *tdata;
    const pj_str_t STR_CONTACT = { "Contact", 7 };
    const pj_str_t mime_text = pj_str("text");
    const pj_str_t mime_plain = pj_str("plain");
    pj_str_t *text;
    pj_status_t status;

    /* Create request. */
    status = pjsip_endpt_create_request(pjsua.endpt, &pjsip_message_method,
					dst_uri, 
					&pjsua.config.acc_config[acc_index].id,
					dst_uri, NULL, NULL, -1, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create request", status);
	return status;
    }

    /* Add accept header. */
    pjsip_msg_add_hdr( tdata->msg, 
		       (pjsip_hdr*)pjsua_im_create_accept(tdata->pool));

    /* Add contact. */
    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
	pjsip_generic_string_hdr_create(tdata->pool, 
					&STR_CONTACT,
					&pjsua.config.acc_config[acc_index].contact));

    /* Duplicate text.
     * We need to keep the text because we will display it when we fail to
     * send the message.
     */
    text = pj_pool_alloc(tdata->pool, sizeof(pj_str_t));
    pj_strdup_with_null(tdata->pool, text, str);

    /* Add message body */
    tdata->msg->body = pjsip_msg_body_create( tdata->pool, &mime_text,
					      &mime_plain, text);
    if (tdata->msg->body == NULL) {
	pjsua_perror(THIS_FILE, "Unable to create msg body", PJ_ENOMEM);
	pjsip_tx_data_dec_ref(tdata);
	return PJ_ENOMEM;
    }

    /* Send request (statefully) */
    status = pjsip_endpt_send_request( pjsua.endpt, tdata, -1, 
				       text, &im_callback);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send request", status);
	return status;
    }

    return PJ_SUCCESS;
}


/**
 * Send typing indication outside dialog.
 */
PJ_DEF(pj_status_t) pjsua_im_typing(int acc_index, const pj_str_t *dst_uri, 
				    pj_bool_t is_typing)
{
    const pj_str_t STR_CONTACT = { "Contact", 7 };
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Create request. */
    status = pjsip_endpt_create_request( pjsua.endpt, &pjsip_message_method,
					 dst_uri, 
					 &pjsua.config.acc_config[acc_index].id,
					 dst_uri, NULL, NULL, -1, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create request", status);
	return status;
    }


    /* Add accept header. */
    pjsip_msg_add_hdr( tdata->msg, 
		       (pjsip_hdr*)pjsua_im_create_accept(tdata->pool));


    /* Add contact. */
    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
	pjsip_generic_string_hdr_create(tdata->pool, 
					&STR_CONTACT,
					&pjsua.config.acc_config[acc_index].contact));


    /* Create "application/im-iscomposing+xml" msg body. */
    tdata->msg->body = pjsip_iscomposing_create_body( tdata->pool, is_typing,
						      NULL, NULL, -1);

    /* Send request (statefully) */
    status = pjsip_endpt_send_request( pjsua.endpt, tdata, -1, 
				       NULL, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send request", status);
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Init pjsua IM module.
 */
pj_status_t pjsua_im_init(void)
{
    const pj_str_t msg_tag = { "MESSAGE", 7 };
    pj_status_t status;

    /* Register module */
    status = pjsip_endpt_register_module(pjsua.endpt, &mod_pjsua_im);
    if (status != PJ_SUCCESS)
	return status;

    /* Register support for MESSAGE method. */
    pjsip_endpt_add_capability( pjsua.endpt, &mod_pjsua_im, PJSIP_H_ALLOW,
				NULL, 1, &msg_tag);

    return PJ_SUCCESS;
}


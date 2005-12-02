/* $Header: /pjproject/pjsip/src/pjsip_mod_ua/sip_reg.c 14    8/31/05 9:05p Bennylp $ */
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
#include <pjsip_mod_ua/sip_reg.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_misc.h>
#include <pjsip/sip_auth_msg.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <pj/log.h>

#define REFRESH_TIMER		1
#define DELAY_BEFORE_REFRESH	5
#define THIS_FILE		"sip_regc.c"

/**
 * SIP client registration structure.
 */
struct pjsip_regc
{
    pj_pool_t	        *pool;
    pjsip_endpoint	*endpt;
    pj_bool_t		 _delete_flag;
    int			pending_tsx;

    void		*token;
    pjsip_regc_cb	*cb;

    pj_str_t		str_srv_url;
    pjsip_uri		*srv_url;
    pjsip_cid_hdr	*cid_hdr;
    pjsip_cseq_hdr	*cseq_hdr;
    pjsip_from_hdr	*from_hdr;
    pjsip_to_hdr	*to_hdr;
    char		*contact_buf;
    pjsip_generic_string_hdr	*contact_hdr;
    pjsip_expires_hdr	*expires_hdr;
    pjsip_contact_hdr	*unreg_contact_hdr;
    pjsip_expires_hdr	*unreg_expires_hdr;
    pj_uint32_t		 expires;

    /* Credentials. */
    int			 cred_count;
    pjsip_cred_info     *cred_info;
    
    /* Authorization sessions. */
    pjsip_auth_session	 auth_sess_list;

    /* Auto refresh registration. */
    pj_bool_t		 auto_reg;
    pj_timer_entry	 timer;
};



PJ_DEF(pjsip_regc*) pjsip_regc_create( pjsip_endpoint *endpt, void *token,
				       pjsip_regc_cb *cb)
{
    pj_pool_t *pool;
    pjsip_regc *regc;

    if (cb == NULL)
	return NULL;

    pool = pjsip_endpt_create_pool(endpt, "regc%p", 1024, 1024);
    regc = pj_pool_calloc(pool, 1, sizeof(struct pjsip_regc));

    regc->pool = pool;
    regc->endpt = endpt;
    regc->token = token;
    regc->cb = cb;
    regc->contact_buf = pj_pool_alloc(pool, PJSIP_REGC_CONTACT_BUF_SIZE);
    regc->expires = PJSIP_REGC_EXPIRATION_NOT_SPECIFIED;

    pj_list_init(&regc->auth_sess_list);

    return regc;
}


PJ_DEF(void) pjsip_regc_destroy(pjsip_regc *regc)
{
    if (regc->pending_tsx) {
	regc->_delete_flag = 1;
	regc->cb = NULL;
    } else {
	pjsip_endpt_destroy_pool(regc->endpt, regc->pool);
    }
}


PJ_DEF(pj_pool_t*) pjsip_regc_get_pool(pjsip_regc *regc)
{
    return regc->pool;
}

static void set_expires( pjsip_regc *regc, pj_uint32_t expires)
{
    if (expires != regc->expires) {
	regc->expires_hdr = pjsip_expires_hdr_create(regc->pool);
	regc->expires_hdr->ivalue = expires;
    } else {
	regc->expires_hdr = NULL;
    }
}


static pj_status_t set_contact( pjsip_regc *regc,
			        int contact_cnt,
				const pj_str_t contact[] )
{
    int i;
    char *s;
    const pj_str_t contact_STR = { "Contact", 7};

    /* Concatenate contacts. */
    for (i=0, s=regc->contact_buf; i<contact_cnt; ++i) {
	if ((s-regc->contact_buf) + contact[i].slen + 2 > PJSIP_REGC_CONTACT_BUF_SIZE) {
	    return -1;
	}
	pj_memcpy(s, contact[i].ptr, contact[i].slen);
	s += contact[i].slen;

	if (i != contact_cnt - 1) {
	    *s++ = ',';
	    *s++ = ' ';
	}
    }

    /* Set "Contact" header. */
    regc->contact_hdr = pjsip_generic_string_hdr_create( regc->pool, &contact_STR);
    regc->contact_hdr->hvalue.ptr = regc->contact_buf;
    regc->contact_hdr->hvalue.slen = (s - regc->contact_buf);

    return 0;
}


PJ_DEF(pj_status_t) pjsip_regc_init( pjsip_regc *regc,
				     const pj_str_t *srv_url,
				     const pj_str_t *from_url,
				     const pj_str_t *to_url,
				     int contact_cnt,
				     const pj_str_t contact[],
				     pj_uint32_t expires)
{
    pj_str_t tmp;

    /* Copy server URL. */
    pj_strdup_with_null(regc->pool, &regc->str_srv_url, srv_url);

    /* Set server URL. */
    tmp = regc->str_srv_url;
    regc->srv_url = pjsip_parse_uri( regc->pool, tmp.ptr, tmp.slen, 0);
    if (regc->srv_url == NULL) {
	return -1;
    }

    /* Set "From" header. */
    pj_strdup_with_null(regc->pool, &tmp, from_url);
    regc->from_hdr = pjsip_from_hdr_create(regc->pool);
    regc->from_hdr->uri = pjsip_parse_uri(regc->pool, tmp.ptr, tmp.slen, 
					  PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!regc->from_hdr->uri) {
	PJ_LOG(4,(THIS_FILE, "regc: invalid source URI %.*s", from_url->slen, from_url->ptr));
	return -1;
    }

    /* Set "To" header. */
    pj_strdup_with_null(regc->pool, &tmp, to_url);
    regc->to_hdr = pjsip_to_hdr_create(regc->pool);
    regc->to_hdr->uri = pjsip_parse_uri(regc->pool, tmp.ptr, tmp.slen, 
					PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!regc->to_hdr->uri) {
	PJ_LOG(4,(THIS_FILE, "regc: invalid target URI %.*s", to_url->slen, to_url->ptr));
	return -1;
    }


    /* Set "Contact" header. */
    if (set_contact( regc, contact_cnt, contact) != 0)
	return -1;

    /* Set "Expires" header, if required. */
    set_expires( regc, expires);

    /* Set "Call-ID" header. */
    regc->cid_hdr = pjsip_cid_hdr_create(regc->pool);
    pj_create_unique_string(regc->pool, &regc->cid_hdr->id);

    /* Set "CSeq" header. */
    regc->cseq_hdr = pjsip_cseq_hdr_create(regc->pool);
    regc->cseq_hdr->cseq = 0;
    pjsip_method_set( &regc->cseq_hdr->method, PJSIP_REGISTER_METHOD);

    /* Create "Contact" header used in unregistration. */
    regc->unreg_contact_hdr = pjsip_contact_hdr_create(regc->pool);
    regc->unreg_contact_hdr->star = 1;

    /* Create "Expires" header used in unregistration. */
    regc->unreg_expires_hdr = pjsip_expires_hdr_create( regc->pool);
    regc->unreg_expires_hdr->ivalue = 0;

    /* Done. */
    return 0;
}

PJ_DEF(pj_status_t) pjsip_regc_set_credentials( pjsip_regc *regc,
						int count,
						const pjsip_cred_info cred[] )
{
    if (count > 0) {
	regc->cred_info = pj_pool_alloc(regc->pool, count * sizeof(pjsip_cred_info));
	pj_memcpy(regc->cred_info, cred, count * sizeof(pjsip_cred_info));
    }
    regc->cred_count = count;
    return 0;
}

static pjsip_tx_data *create_request(pjsip_regc *regc)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg;

    /* Create transmit data. */
    tdata = pjsip_endpt_create_tdata(regc->endpt);
    if (!tdata) {
	return NULL;
    }

    /* Create request message. */
    msg = pjsip_msg_create(tdata->pool, PJSIP_REQUEST_MSG);
    tdata->msg = msg;

    /* Initialize request line. */
    pjsip_method_set(&msg->line.req.method, PJSIP_REGISTER_METHOD);
    msg->line.req.uri = regc->srv_url;

    /* Add headers. */
    pjsip_msg_add_hdr(msg, (pjsip_hdr*) regc->from_hdr);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*) regc->to_hdr);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*) regc->cid_hdr);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*) regc->cseq_hdr);
    
    /* Add cached authorization headers. */
    pjsip_auth_init_req( regc->pool, tdata, &regc->auth_sess_list,
			 regc->cred_count, regc->cred_info );

    /* Add reference counter to transmit data. */
    pjsip_tx_data_add_ref(tdata);

    return tdata;
}


PJ_DEF(pjsip_tx_data*) pjsip_regc_register(pjsip_regc *regc, pj_bool_t autoreg)
{
    pjsip_msg *msg;
    pjsip_tx_data *tdata;

    tdata = create_request(regc);
    if (!tdata)
	return NULL;

    msg = tdata->msg;
    pjsip_msg_add_hdr(msg, (pjsip_hdr*) regc->contact_hdr);
    if (regc->expires_hdr)
	pjsip_msg_add_hdr(msg, (pjsip_hdr*) regc->expires_hdr);

    if (regc->timer.id != 0) {
	pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
	regc->timer.id = 0;
    }

    regc->auto_reg = autoreg;

    return tdata;
}


PJ_DEF(pjsip_tx_data*) pjsip_regc_unregister(pjsip_regc *regc)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg;

    if (regc->timer.id != 0) {
	pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
	regc->timer.id = 0;
    }

    tdata = create_request(regc);
    if (!tdata)
	return NULL;

    msg = tdata->msg;
    pjsip_msg_add_hdr( msg, (pjsip_hdr*)regc->unreg_contact_hdr);
    pjsip_msg_add_hdr( msg, (pjsip_hdr*)regc->unreg_expires_hdr);

    return tdata;
}


PJ_DEF(pj_status_t) pjsip_regc_update_contact(  pjsip_regc *regc,
					        int contact_cnt,
						const pj_str_t contact[] )
{
    return set_contact( regc, contact_cnt, contact );
}


PJ_DEF(pj_status_t) pjsip_regc_update_expires(  pjsip_regc *regc,
					        pj_uint32_t expires )
{
    set_expires( regc, expires );
    return 0;
}


static void call_callback(pjsip_regc *regc, int status, const pj_str_t *reason,
			  pjsip_rx_data *rdata, pj_int32_t expiration,
			  int contact_cnt, pjsip_contact_hdr *contact[])
{
    struct pjsip_regc_cbparam cbparam;


    cbparam.regc = regc;
    cbparam.token = regc->token;
    cbparam.code = status;
    cbparam.reason = *reason;
    cbparam.rdata = rdata;
    cbparam.contact_cnt = contact_cnt;
    cbparam.expiration = expiration;
    if (contact_cnt) {
	pj_memcpy( cbparam.contact, contact, 
		   contact_cnt*sizeof(pjsip_contact_hdr*));
    }

    (*regc->cb)(&cbparam);
}

static void regc_refresh_timer_cb( pj_timer_heap_t *timer_heap,
				   struct pj_timer_entry *entry)
{
    pjsip_regc *regc = entry->user_data;
    pjsip_tx_data *tdata;
    
    PJ_UNUSED_ARG(timer_heap)

    entry->id = 0;
    tdata = pjsip_regc_register(regc, 1);
    if (tdata) {
	pjsip_regc_send(regc, tdata);
    } else {
	pj_str_t reason = pj_str("Unable to create txdata");
	call_callback(regc, -1, &reason, NULL, -1, 0, NULL);
    }
}

static void tsx_callback(void *token, pjsip_event *event)
{
    pjsip_regc *regc = token;
    pjsip_transaction *tsx = event->obj.tsx;
    
    /* If registration data has been deleted by user then remove registration 
     * data from transaction's callback, and don't call callback.
     */
    if (regc->_delete_flag) {
	--regc->pending_tsx;

    } else if (tsx->status_code == PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED ||
	       tsx->status_code == PJSIP_SC_UNAUTHORIZED)
    {
	pjsip_rx_data *rdata = event->src.rdata;
	pjsip_tx_data *tdata;

	tdata = pjsip_auth_reinit_req( regc->endpt,
				       regc->pool, &regc->auth_sess_list,
				       regc->cred_count, regc->cred_info,
				       tsx->last_tx, event->src.rdata );

	if (tdata) {
	    --regc->pending_tsx;
	    pjsip_regc_send(regc, tdata);
	    return;
	} else {
	    call_callback(regc, tsx->status_code, &rdata->msg->line.status.reason,
			  rdata, -1, 0, NULL);
	    --regc->pending_tsx;
	}
    } else {
	int contact_cnt = 0;
	pjsip_contact_hdr *contact[PJSIP_REGC_MAX_CONTACT];
	pjsip_rx_data *rdata;
	pj_int32_t expiration = 0xFFFF;

	if (tsx->status_code/100 == 2) {
	    int i;
	    pjsip_contact_hdr *hdr;
	    pjsip_msg *msg;
	    pjsip_expires_hdr *expires;

	    rdata = event->src.rdata;
	    msg = rdata->msg;
	    hdr = pjsip_msg_find_hdr( msg, PJSIP_H_CONTACT, NULL);
	    while (hdr) {
		contact[contact_cnt++] = hdr;
		hdr = hdr->next;
		if (hdr == (void*)&msg->hdr)
		    break;
		hdr = pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, hdr);
	    }

	    expires = pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);

	    if (expires)
		expiration = expires->ivalue;
	    
	    for (i=0; i<contact_cnt; ++i) {
		hdr = contact[i];
		if (hdr->expires >= 0 && hdr->expires < expiration)
		    expiration = contact[i]->expires;
	    }

	    if (regc->auto_reg && expiration != 0 && expiration != 0xFFFF) {
		pj_time_val delay = { 0, 0};

		delay.sec = expiration - DELAY_BEFORE_REFRESH;
		if (regc->expires != PJSIP_REGC_EXPIRATION_NOT_SPECIFIED && 
		    delay.sec > (pj_int32_t)regc->expires) 
		{
		    delay.sec = regc->expires;
		}
		if (delay.sec < DELAY_BEFORE_REFRESH) 
		    delay.sec = DELAY_BEFORE_REFRESH;
		regc->timer.cb = &regc_refresh_timer_cb;
		regc->timer.id = REFRESH_TIMER;
		regc->timer.user_data = regc;
		pjsip_endpt_schedule_timer( regc->endpt, &regc->timer, &delay);
	    }

	} else {
	    rdata = (event->src_type==PJSIP_EVENT_RX_MSG) ? event->src.rdata : NULL;
	}


	/* Call callback. */
	if (expiration == 0xFFFF) expiration = -1;
	call_callback(regc, tsx->status_code, 
		      (rdata ? &rdata->msg->line.status.reason 
			: pjsip_get_status_text(tsx->status_code)),
		      rdata, expiration, 
		      contact_cnt, contact);

	--regc->pending_tsx;
    }

    /* Delete the record if user destroy regc during the callback. */
    if (regc->_delete_flag && regc->pending_tsx==0) {
	pjsip_regc_destroy(regc);
    }
}

PJ_DEF(void) pjsip_regc_send(pjsip_regc *regc, pjsip_tx_data *tdata)
{
    int status;

    /* Make sure we don't have pending transaction. */
    if (regc->pending_tsx) {
	pj_str_t reason = pj_str("Transaction in progress");
	call_callback(regc, -1, &reason, NULL, -1, 0, NULL);
	pjsip_tx_data_dec_ref( tdata );
	return;
    }

    /* Invalidate message buffer. */
    pjsip_tx_data_invalidate_msg(tdata);

    /* Increment CSeq */
    regc->cseq_hdr->cseq++;

    /* Send. */
    status = pjsip_endpt_send_request(regc->endpt, tdata, -1, regc, &tsx_callback);
    if (status==0)
	++regc->pending_tsx;
    else {
	pj_str_t reason = pj_str("Unable to send request.");
	call_callback(regc, status, &reason, NULL, -1, 0, NULL);
    }
}



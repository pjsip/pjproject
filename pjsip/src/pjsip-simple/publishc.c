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
#include <pjsip-ua/sip_regc.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_auth_msg.h>
#include <pjsip/sip_errno.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/rand.h>
#include <pj/string.h>


#define REFRESH_TIMER		1
#define DELAY_BEFORE_REFRESH	5
#define THIS_FILE		"sip_regc.c"

/**
 * SIP client registration structure.
 */
struct pjsip_regc
{
    pj_pool_t			*pool;
    pjsip_endpoint		*endpt;
    pj_bool_t			 _delete_flag;
    int				 pending_tsx;

    void			*token;
    pjsip_regc_cb		*cb;

    pj_str_t			 str_srv_url;
    pjsip_uri			*srv_url;
    pjsip_cid_hdr		*cid_hdr;
    pjsip_cseq_hdr		*cseq_hdr;
    pj_str_t			 from_uri;
    pjsip_from_hdr		*from_hdr;
    pjsip_to_hdr		*to_hdr;
    char			*contact_buf;
    pjsip_generic_string_hdr	*contact_hdr;
    pjsip_expires_hdr		*expires_hdr;
    pjsip_contact_hdr		*unreg_contact_hdr;
    pjsip_expires_hdr		*unreg_expires_hdr;
    pj_uint32_t			 expires;
    pjsip_route_hdr		 route_set;

    /* Authorization sessions. */
    pjsip_auth_clt_sess		 auth_sess;

    /* Auto refresh registration. */
    pj_bool_t			 auto_reg;
    pj_time_val			 last_reg;
    pj_time_val			 next_reg;
    pj_timer_entry		 timer;
};



PJ_DEF(pj_status_t) pjsip_regc_create( pjsip_endpoint *endpt, void *token,
				       pjsip_regc_cb *cb,
				       pjsip_regc **p_regc)
{
    pj_pool_t *pool;
    pjsip_regc *regc;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(endpt && cb && p_regc, PJ_EINVAL);

    pool = pjsip_endpt_create_pool(endpt, "regc%p", 1024, 1024);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    regc = pj_pool_zalloc(pool, sizeof(struct pjsip_regc));

    regc->pool = pool;
    regc->endpt = endpt;
    regc->token = token;
    regc->cb = cb;
    regc->contact_buf = pj_pool_alloc(pool, PJSIP_REGC_CONTACT_BUF_SIZE);
    regc->expires = PJSIP_REGC_EXPIRATION_NOT_SPECIFIED;

    status = pjsip_auth_clt_init(&regc->auth_sess, endpt, regc->pool, 0);
    if (status != PJ_SUCCESS)
	return status;

    pj_list_init(&regc->route_set);

    /* Done */
    *p_regc = regc;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_destroy(pjsip_regc *regc)
{
    PJ_ASSERT_RETURN(regc, PJ_EINVAL);

    if (regc->pending_tsx) {
	regc->_delete_flag = 1;
	regc->cb = NULL;
    } else {
	pjsip_endpt_release_pool(regc->endpt, regc->pool);
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_get_info( pjsip_regc *regc,
					 pjsip_regc_info *info )
{
    PJ_ASSERT_RETURN(regc && info, PJ_EINVAL);

    info->server_uri = regc->str_srv_url;
    info->client_uri = regc->from_uri;
    info->is_busy = (regc->pending_tsx != 0);
    info->auto_reg = regc->auto_reg;
    info->interval = regc->expires;
    
    if (regc->pending_tsx)
	info->next_reg = 0;
    else if (regc->auto_reg == 0)
	info->next_reg = 0;
    else if (regc->expires < 0)
	info->next_reg = regc->expires;
    else {
	pj_time_val now, next_reg;

	next_reg = regc->next_reg;
	pj_gettimeofday(&now);
	PJ_TIME_VAL_SUB(next_reg, now);
	info->next_reg = next_reg.sec;
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_pool_t*) pjsip_regc_get_pool(pjsip_regc *regc)
{
    return regc->pool;
}

static void set_expires( pjsip_regc *regc, pj_uint32_t expires)
{
    if (expires != regc->expires) {
	regc->expires_hdr = pjsip_expires_hdr_create(regc->pool, expires);
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
	    return PJSIP_EURITOOLONG;
	}
	pj_memcpy(s, contact[i].ptr, contact[i].slen);
	s += contact[i].slen;

	if (i != contact_cnt - 1) {
	    *s++ = ',';
	    *s++ = ' ';
	}
    }

    /* Set "Contact" header. */
    regc->contact_hdr = pjsip_generic_string_hdr_create(regc->pool, 
							&contact_STR,
							NULL);
    regc->contact_hdr->hvalue.ptr = regc->contact_buf;
    regc->contact_hdr->hvalue.slen = (s - regc->contact_buf);

    return PJ_SUCCESS;
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
    pj_status_t status;

    PJ_ASSERT_RETURN(regc && srv_url && from_url && to_url && 
		     contact_cnt && contact && expires, PJ_EINVAL);

    /* Copy server URL. */
    pj_strdup_with_null(regc->pool, &regc->str_srv_url, srv_url);

    /* Set server URL. */
    tmp = regc->str_srv_url;
    regc->srv_url = pjsip_parse_uri( regc->pool, tmp.ptr, tmp.slen, 0);
    if (regc->srv_url == NULL) {
	return PJSIP_EINVALIDURI;
    }

    /* Set "From" header. */
    pj_strdup_with_null(regc->pool, &regc->from_uri, from_url);
    tmp = regc->from_uri;
    regc->from_hdr = pjsip_from_hdr_create(regc->pool);
    regc->from_hdr->uri = pjsip_parse_uri(regc->pool, tmp.ptr, tmp.slen, 
					  PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!regc->from_hdr->uri) {
	PJ_LOG(4,(THIS_FILE, "regc: invalid source URI %.*s", 
		  from_url->slen, from_url->ptr));
	return PJSIP_EINVALIDURI;
    }

    /* Set "To" header. */
    pj_strdup_with_null(regc->pool, &tmp, to_url);
    regc->to_hdr = pjsip_to_hdr_create(regc->pool);
    regc->to_hdr->uri = pjsip_parse_uri(regc->pool, tmp.ptr, tmp.slen, 
					PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!regc->to_hdr->uri) {
	PJ_LOG(4,(THIS_FILE, "regc: invalid target URI %.*s", to_url->slen, to_url->ptr));
	return PJSIP_EINVALIDURI;
    }


    /* Set "Contact" header. */
    status = set_contact( regc, contact_cnt, contact);
    if (status != PJ_SUCCESS)
	return status;

    /* Set "Expires" header, if required. */
    set_expires( regc, expires);

    /* Set "Call-ID" header. */
    regc->cid_hdr = pjsip_cid_hdr_create(regc->pool);
    pj_create_unique_string(regc->pool, &regc->cid_hdr->id);

    /* Set "CSeq" header. */
    regc->cseq_hdr = pjsip_cseq_hdr_create(regc->pool);
    regc->cseq_hdr->cseq = pj_rand() % 0xFFFF;
    pjsip_method_set( &regc->cseq_hdr->method, PJSIP_REGISTER_METHOD);

    /* Create "Contact" header used in unregistration. */
    regc->unreg_contact_hdr = pjsip_contact_hdr_create(regc->pool);
    regc->unreg_contact_hdr->star = 1;

    /* Create "Expires" header used in unregistration. */
    regc->unreg_expires_hdr = pjsip_expires_hdr_create( regc->pool, 0);

    /* Done. */
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_regc_set_credentials( pjsip_regc *regc,
						int count,
						const pjsip_cred_info cred[] )
{
    PJ_ASSERT_RETURN(regc && count && cred, PJ_EINVAL);
    return pjsip_auth_clt_set_credentials(&regc->auth_sess, count, cred);
}

PJ_DEF(pj_status_t) pjsip_regc_set_route_set( pjsip_regc *regc,
					      const pjsip_route_hdr *route_set)
{
    const pjsip_route_hdr *chdr;

    PJ_ASSERT_RETURN(regc && route_set, PJ_EINVAL);

    pj_list_init(&regc->route_set);

    chdr = route_set->next;
    while (chdr != route_set) {
	pj_list_push_back(&regc->route_set, pjsip_hdr_clone(regc->pool, chdr));
	chdr = chdr->next;
    }

    return PJ_SUCCESS;
}

static pj_status_t create_request(pjsip_regc *regc, 
				  pjsip_tx_data **p_tdata)
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    PJ_ASSERT_RETURN(regc && p_tdata, PJ_EINVAL);

    /* Create the request. */
    status = pjsip_endpt_create_request_from_hdr( regc->endpt, 
						  &pjsip_register_method,
						  regc->srv_url,
						  regc->from_hdr,
						  regc->to_hdr,
						  NULL,
						  regc->cid_hdr,
						  regc->cseq_hdr->cseq,
						  NULL,
						  &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add cached authorization headers. */
    pjsip_auth_clt_init_req( &regc->auth_sess, tdata );

    /* Add Route headers from route set, ideally after Via header */
    if (!pj_list_empty(&regc->route_set)) {
	pjsip_hdr *route_pos;
	const pjsip_route_hdr *route;

	route_pos = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
	if (!route_pos)
	    route_pos = &tdata->msg->hdr;

	route = regc->route_set.next;
	while (route != &regc->route_set) {
	    pjsip_hdr *new_hdr = pjsip_hdr_shallow_clone(tdata->pool, route);
	    pj_list_insert_after(route_pos, new_hdr);
	    route_pos = new_hdr;
	    route = route->next;
	}
    }

    /* Done. */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_register(pjsip_regc *regc, pj_bool_t autoreg,
					pjsip_tx_data **p_tdata)
{
    pjsip_msg *msg;
    pj_status_t status;
    pjsip_tx_data *tdata;

    PJ_ASSERT_RETURN(regc && p_tdata, PJ_EINVAL);

    status = create_request(regc, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add Contact header. */
    msg = tdata->msg;
    pjsip_msg_add_hdr(msg, (pjsip_hdr*) regc->contact_hdr);
    if (regc->expires_hdr)
	pjsip_msg_add_hdr(msg, (pjsip_hdr*) regc->expires_hdr);

    if (regc->timer.id != 0) {
	pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
	regc->timer.id = 0;
    }

    regc->auto_reg = autoreg;

    /* Done */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_unregister(pjsip_regc *regc,
					  pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg;
    pj_status_t status;

    PJ_ASSERT_RETURN(regc && p_tdata, PJ_EINVAL);

    if (regc->timer.id != 0) {
	pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
	regc->timer.id = 0;
    }

    status = create_request(regc, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    msg = tdata->msg;
    pjsip_msg_add_hdr( msg, (pjsip_hdr*)regc->unreg_contact_hdr);
    pjsip_msg_add_hdr( msg, (pjsip_hdr*)regc->unreg_expires_hdr);

    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_update_contact(  pjsip_regc *regc,
					        int contact_cnt,
						const pj_str_t contact[] )
{
    PJ_ASSERT_RETURN(regc, PJ_EINVAL);
    return set_contact( regc, contact_cnt, contact );
}


PJ_DEF(pj_status_t) pjsip_regc_update_expires(  pjsip_regc *regc,
					        pj_uint32_t expires )
{
    PJ_ASSERT_RETURN(regc, PJ_EINVAL);
    set_expires( regc, expires );
    return PJ_SUCCESS;
}


static void call_callback(pjsip_regc *regc, pj_status_t status, int st_code, 
			  const pj_str_t *reason,
			  pjsip_rx_data *rdata, pj_int32_t expiration,
			  int contact_cnt, pjsip_contact_hdr *contact[])
{
    struct pjsip_regc_cbparam cbparam;


    cbparam.regc = regc;
    cbparam.token = regc->token;
    cbparam.status = status;
    cbparam.code = st_code;
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
    pj_status_t status;
    
    PJ_UNUSED_ARG(timer_heap);

    entry->id = 0;
    status = pjsip_regc_register(regc, 1, &tdata);
    if (status == PJ_SUCCESS) {
	status = pjsip_regc_send(regc, tdata);
    } 
    
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_str_t reason = pj_strerror(status, errmsg, sizeof(errmsg));
	call_callback(regc, status, 400, &reason, NULL, -1, 0, NULL);
    }
}

static void tsx_callback(void *token, pjsip_event *event)
{
    pj_status_t status;
    pjsip_regc *regc = token;
    pjsip_transaction *tsx = event->body.tsx_state.tsx;
    
    /* Decrement pending transaction counter. */
    pj_assert(regc->pending_tsx > 0);
    --regc->pending_tsx;

    /* If registration data has been deleted by user then remove registration 
     * data from transaction's callback, and don't call callback.
     */
    if (regc->_delete_flag) {

	/* Nothing to do */
	;

    } else if (tsx->status_code == PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED ||
	       tsx->status_code == PJSIP_SC_UNAUTHORIZED)
    {
	pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
	pjsip_tx_data *tdata;

	status = pjsip_auth_clt_reinit_req( &regc->auth_sess,
					    rdata, 
					    tsx->last_tx,  
					    &tdata);

	if (status == PJ_SUCCESS) {
	    status = pjsip_regc_send(regc, tdata);
	} 
	
	if (status != PJ_SUCCESS) {
	    call_callback(regc, status, tsx->status_code, 
			  &rdata->msg_info.msg->line.status.reason,
			  rdata, -1, 0, NULL);
	}

	return;

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

	    rdata = event->body.tsx_state.src.rdata;
	    msg = rdata->msg_info.msg;
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
		pj_gettimeofday(&regc->last_reg);
		regc->next_reg = regc->last_reg;
		regc->next_reg.sec += delay.sec;
	    }

	} else {
	    rdata = (event->body.tsx_state.type==PJSIP_EVENT_RX_MSG) ? 
			event->body.tsx_state.src.rdata : NULL;
	}


	/* Call callback. */
	if (expiration == 0xFFFF) expiration = -1;
	call_callback(regc, PJ_SUCCESS, tsx->status_code, 
		      (rdata ? &rdata->msg_info.msg->line.status.reason 
			: pjsip_get_status_text(tsx->status_code)),
		      rdata, expiration, 
		      contact_cnt, contact);

    }

    /* Delete the record if user destroy regc during the callback. */
    if (regc->_delete_flag && regc->pending_tsx==0) {
	pjsip_regc_destroy(regc);
    }
}

PJ_DEF(pj_status_t) pjsip_regc_send(pjsip_regc *regc, pjsip_tx_data *tdata)
{
    pj_status_t status;
    pjsip_cseq_hdr *cseq_hdr;
    pj_uint32_t cseq;

    /* Make sure we don't have pending transaction. */
    if (regc->pending_tsx) {
	PJ_LOG(4,(THIS_FILE, "Unable to send request, regc has another "
			     "transaction pending"));
	pjsip_tx_data_dec_ref( tdata );
	return PJSIP_EBUSY;
    }

    /* Invalidate message buffer. */
    pjsip_tx_data_invalidate_msg(tdata);

    /* Increment CSeq */
    cseq = ++regc->cseq_hdr->cseq;
    cseq_hdr = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
    cseq_hdr->cseq = cseq;

    /* Increment pending transaction first, since transaction callback
     * may be called even before send_request() returns!
     */
    ++regc->pending_tsx;
    status = pjsip_endpt_send_request(regc->endpt, tdata, -1, regc, &tsx_callback);
    if (status!=PJ_SUCCESS) {
	--regc->pending_tsx;
	PJ_LOG(4,(THIS_FILE, "Error sending request, status=%d", status));
    }

    return status;
}



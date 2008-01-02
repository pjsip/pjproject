/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pjnath/stun_session.h>
#include <pjlib.h>

struct pj_stun_session
{
    pj_stun_config	*cfg;
    pj_pool_t		*pool;
    pj_mutex_t		*mutex;
    pj_stun_session_cb	 cb;
    void		*user_data;

    pj_bool_t		 use_fingerprint;
    pj_stun_auth_cred	*cred;
    pj_str_t		 srv_name;

    pj_stun_tx_data	 pending_request_list;
    pj_stun_tx_data	 cached_response_list;
};

#define SNAME(s_)		    ((s_)->pool->obj_name)

#if PJ_LOG_MAX_LEVEL >= 5
#   define TRACE_(expr)		    PJ_LOG(5,expr)
#else
#   define TRACE_(expr)
#endif

#define LOG_ERR_(sess,title,rc) pjnath_perror(sess->pool->obj_name,title,rc)

#define TDATA_POOL_SIZE		    PJNATH_POOL_LEN_STUN_TDATA
#define TDATA_POOL_INC		    PJNATH_POOL_INC_STUN_TDATA


static void stun_tsx_on_complete(pj_stun_client_tsx *tsx,
				 pj_status_t status, 
				 const pj_stun_msg *response,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len);
static pj_status_t stun_tsx_on_send_msg(pj_stun_client_tsx *tsx,
					const void *stun_pkt,
					pj_size_t pkt_size);
static void stun_tsx_on_destroy(pj_stun_client_tsx *tsx);

static pj_stun_tsx_cb tsx_cb = 
{
    &stun_tsx_on_complete,
    &stun_tsx_on_send_msg,
    &stun_tsx_on_destroy
};


static pj_status_t tsx_add(pj_stun_session *sess,
			   pj_stun_tx_data *tdata)
{
    pj_list_push_back(&sess->pending_request_list, tdata);
    return PJ_SUCCESS;
}

static pj_status_t tsx_erase(pj_stun_session *sess,
			     pj_stun_tx_data *tdata)
{
    PJ_UNUSED_ARG(sess);
    pj_list_erase(tdata);
    return PJ_SUCCESS;
}

static pj_stun_tx_data* tsx_lookup(pj_stun_session *sess,
				   const pj_stun_msg *msg)
{
    pj_stun_tx_data *tdata;

    tdata = sess->pending_request_list.next;
    while (tdata != &sess->pending_request_list) {
	pj_assert(sizeof(tdata->msg_key)==sizeof(msg->hdr.tsx_id));
	if (tdata->msg_magic == msg->hdr.magic &&
	    pj_memcmp(tdata->msg_key, msg->hdr.tsx_id, 
		      sizeof(msg->hdr.tsx_id))==0)
	{
	    return tdata;
	}
	tdata = tdata->next;
    }

    return NULL;
}

static pj_status_t create_tdata(pj_stun_session *sess,
			        pj_stun_tx_data **p_tdata)
{
    pj_pool_t *pool;
    pj_stun_tx_data *tdata;

    /* Create pool and initialize basic tdata attributes */
    pool = pj_pool_create(sess->cfg->pf, "tdata%p", 
			  TDATA_POOL_SIZE, TDATA_POOL_INC, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    tdata = PJ_POOL_ZALLOC_T(pool, pj_stun_tx_data);
    tdata->pool = pool;
    tdata->sess = sess;

    pj_list_init(tdata);

    *p_tdata = tdata;

    return PJ_SUCCESS;
}

static pj_status_t create_request_tdata(pj_stun_session *sess,
					unsigned msg_type,
					pj_uint32_t magic,
					const pj_uint8_t tsx_id[12],
					pj_stun_tx_data **p_tdata)
{
    pj_status_t status;
    pj_stun_tx_data *tdata;

    status = create_tdata(sess, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create STUN message */
    status = pj_stun_msg_create(tdata->pool, msg_type,  magic, 
				tsx_id, &tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_pool_release(tdata->pool);
	return status;
    }

    /* copy the request's transaction ID as the transaction key. */
    pj_assert(sizeof(tdata->msg_key)==sizeof(tdata->msg->hdr.tsx_id));
    tdata->msg_magic = tdata->msg->hdr.magic;
    pj_memcpy(tdata->msg_key, tdata->msg->hdr.tsx_id,
	      sizeof(tdata->msg->hdr.tsx_id));

    *p_tdata = tdata;

    return PJ_SUCCESS;
}


static void stun_tsx_on_destroy(pj_stun_client_tsx *tsx)
{
    pj_stun_tx_data *tdata;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);
    pj_stun_client_tsx_destroy(tsx);
    pj_pool_release(tdata->pool);
}

static void destroy_tdata(pj_stun_tx_data *tdata)
{
    if (tdata->res_timer.id != PJ_FALSE) {
	pj_timer_heap_cancel(tdata->sess->cfg->timer_heap, 
			     &tdata->res_timer);
	tdata->res_timer.id = PJ_FALSE;
	pj_list_erase(tdata);
    }

    if (tdata->client_tsx) {
	pj_time_val delay = {2, 0};
	tsx_erase(tdata->sess, tdata);
	pj_stun_client_tsx_schedule_destroy(tdata->client_tsx, &delay);
	tdata->client_tsx = NULL;

    } else {
	pj_pool_release(tdata->pool);
    }
}

/*
 * Destroy the transmit data.
 */
PJ_DEF(void) pj_stun_msg_destroy_tdata( pj_stun_session *sess,
					pj_stun_tx_data *tdata)
{
    PJ_UNUSED_ARG(sess);
    destroy_tdata(tdata);
}


/* Timer callback to be called when it's time to destroy response cache */
static void on_cache_timeout(pj_timer_heap_t *timer_heap,
			     struct pj_timer_entry *entry)
{
    pj_stun_tx_data *tdata;

    PJ_UNUSED_ARG(timer_heap);

    entry->id = PJ_FALSE;
    tdata = (pj_stun_tx_data*) entry->user_data;

    PJ_LOG(5,(SNAME(tdata->sess), "Response cache deleted"));

    pj_list_erase(tdata);
    pj_stun_msg_destroy_tdata(tdata->sess, tdata);
}

static pj_status_t get_key(pj_stun_session *sess, pj_pool_t *pool,
			   const pj_stun_msg *msg, pj_str_t *auth_key)
{
    if (sess->cred == NULL) {
	auth_key->slen = 0;
	return PJ_SUCCESS;
    } else if (sess->cred->type == PJ_STUN_AUTH_CRED_STATIC) {
	pj_stun_create_key(pool, auth_key, 
			   &sess->cred->data.static_cred.realm,
			   &sess->cred->data.static_cred.username,
			   &sess->cred->data.static_cred.data);
	return PJ_SUCCESS;
    } else if (sess->cred->type == PJ_STUN_AUTH_CRED_DYNAMIC) {
	pj_str_t realm, username, nonce;
	pj_str_t *password;
	void *user_data = sess->cred->data.dyn_cred.user_data;
	int data_type = 0;
	pj_status_t status;

	realm.slen = username.slen = nonce.slen = 0;
	password = PJ_POOL_ZALLOC_T(pool, pj_str_t);
	status = (*sess->cred->data.dyn_cred.get_cred)(msg, user_data, pool,
						       &realm, &username,
						       &nonce, &data_type,
						       password);
	if (status != PJ_SUCCESS)
	    return status;

	pj_stun_create_key(pool, auth_key, 
			   &realm, &username, password);

	return PJ_SUCCESS;

    } else {
	pj_assert(!"Unknown credential type");
	return PJ_EBUG;
    }
}

static pj_status_t apply_msg_options(pj_stun_session *sess,
				     pj_pool_t *pool,
				     pj_stun_msg *msg)
{
    pj_status_t status = 0;
    pj_bool_t need_auth;
    pj_str_t realm, username, nonce, password;
    int data_type = 0;

    realm.slen = username.slen = nonce.slen = password.slen = 0;

    /* The server SHOULD include a SERVER attribute in all responses */
    if (sess->srv_name.slen && PJ_STUN_IS_RESPONSE(msg->hdr.type)) {
	pj_stun_msg_add_string_attr(pool, msg, PJ_STUN_ATTR_SERVER,
				    &sess->srv_name);
    }

    need_auth = pj_stun_auth_valid_for_msg(msg);

    if (sess->cred && sess->cred->type == PJ_STUN_AUTH_CRED_STATIC &&
	need_auth)
    {
	realm = sess->cred->data.static_cred.realm;
	username = sess->cred->data.static_cred.username;
	data_type = sess->cred->data.static_cred.data_type;
	password = sess->cred->data.static_cred.data;
	nonce = sess->cred->data.static_cred.nonce;

    } else if (sess->cred && sess->cred->type == PJ_STUN_AUTH_CRED_DYNAMIC &&
	       need_auth) 
    {
	void *user_data = sess->cred->data.dyn_cred.user_data;

	status = (*sess->cred->data.dyn_cred.get_cred)(msg, user_data, pool,
						       &realm, &username,
						       &nonce, &data_type,
						       &password);
	if (status != PJ_SUCCESS)
	    return status;
    }


    /* Create and add USERNAME attribute for */
    if (username.slen && PJ_STUN_IS_REQUEST(msg->hdr.type)) {
	status = pj_stun_msg_add_string_attr(pool, msg,
					     PJ_STUN_ATTR_USERNAME,
					     &username);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }

    /* Add REALM only when long term credential is used */
    if (realm.slen &&  PJ_STUN_IS_REQUEST(msg->hdr.type)) {
	status = pj_stun_msg_add_string_attr(pool, msg,
					    PJ_STUN_ATTR_REALM,
					    &realm);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }

    /* Add NONCE when desired */
    if (nonce.slen && 
	(PJ_STUN_IS_REQUEST(msg->hdr.type) ||
	 PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))) 
    {
	status = pj_stun_msg_add_string_attr(pool, msg,
					    PJ_STUN_ATTR_NONCE,
					    &nonce);
    }

    /* Add MESSAGE-INTEGRITY attribute */
    if (username.slen && need_auth) {
	status = pj_stun_msg_add_msgint_attr(pool, msg);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }


    /* Add FINGERPRINT attribute if necessary */
    if (sess->use_fingerprint) {
	status = pj_stun_msg_add_uint_attr(pool, msg, 
					  PJ_STUN_ATTR_FINGERPRINT, 0);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }

    return PJ_SUCCESS;
}


static void stun_tsx_on_complete(pj_stun_client_tsx *tsx,
				 pj_status_t status, 
				 const pj_stun_msg *response,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len)
{
    pj_stun_tx_data *tdata;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);

    if (tdata->sess->cb.on_request_complete) {
	(*tdata->sess->cb.on_request_complete)(tdata->sess, status, tdata, 
					       response, 
					       src_addr, src_addr_len);
    }
}

static pj_status_t stun_tsx_on_send_msg(pj_stun_client_tsx *tsx,
					const void *stun_pkt,
					pj_size_t pkt_size)
{
    pj_stun_tx_data *tdata;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);

    return tdata->sess->cb.on_send_msg(tdata->sess, stun_pkt, pkt_size,
				       tdata->dst_addr, tdata->addr_len);
}

/* **************************************************************************/

PJ_DEF(pj_status_t) pj_stun_session_create( pj_stun_config *cfg,
					    const char *name,
					    const pj_stun_session_cb *cb,
					    pj_bool_t fingerprint,
					    pj_stun_session **p_sess)
{
    pj_pool_t	*pool;
    pj_stun_session *sess;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && cb && p_sess, PJ_EINVAL);

    if (name==NULL)
	name = "stuse%p";

    pool = pj_pool_create(cfg->pf, name, PJNATH_POOL_LEN_STUN_SESS, 
			  PJNATH_POOL_INC_STUN_SESS, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    sess = PJ_POOL_ZALLOC_T(pool, pj_stun_session);
    sess->cfg = cfg;
    sess->pool = pool;
    pj_memcpy(&sess->cb, cb, sizeof(*cb));
    sess->use_fingerprint = fingerprint;
    
    sess->srv_name.ptr = (char*) pj_pool_alloc(pool, 32);
    sess->srv_name.slen = pj_ansi_snprintf(sess->srv_name.ptr, 32,
					   "pj_stun-%s", pj_get_version());

    pj_list_init(&sess->pending_request_list);
    pj_list_init(&sess->cached_response_list);

    status = pj_mutex_create_recursive(pool, name, &sess->mutex);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    *p_sess = sess;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_destroy(pj_stun_session *sess)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    pj_mutex_lock(sess->mutex);
    while (!pj_list_empty(&sess->pending_request_list)) {
	pj_stun_tx_data *tdata = sess->pending_request_list.next;
	destroy_tdata(tdata);
    }
    while (!pj_list_empty(&sess->cached_response_list)) {
	pj_stun_tx_data *tdata = sess->cached_response_list.next;
	destroy_tdata(tdata);
    }
    pj_mutex_unlock(sess->mutex);

    pj_mutex_destroy(sess->mutex);
    pj_pool_release(sess->pool);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_session_set_user_data( pj_stun_session *sess,
						   void *user_data)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);
    pj_mutex_lock(sess->mutex);
    sess->user_data = user_data;
    pj_mutex_unlock(sess->mutex);
    return PJ_SUCCESS;
}

PJ_DEF(void*) pj_stun_session_get_user_data(pj_stun_session *sess)
{
    PJ_ASSERT_RETURN(sess, NULL);
    return sess->user_data;
}

PJ_DEF(pj_status_t) pj_stun_session_set_server_name(pj_stun_session *sess,
						    const pj_str_t *srv_name)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);
    if (srv_name)
	pj_strdup(sess->pool, &sess->srv_name, srv_name);
    else
	sess->srv_name.slen = 0;
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_stun_session_set_credential(pj_stun_session *sess,
					    const pj_stun_auth_cred *cred)
{
    PJ_ASSERT_ON_FAIL(sess, return);
    if (cred) {
	if (!sess->cred)
	    sess->cred = PJ_POOL_ALLOC_T(sess->pool, pj_stun_auth_cred);
	pj_stun_auth_cred_dup(sess->pool, sess->cred, cred);
    } else {
	sess->cred = NULL;
    }
}


PJ_DEF(pj_status_t) pj_stun_session_create_req(pj_stun_session *sess,
					       int method,
					       pj_uint32_t magic,
					       const pj_uint8_t tsx_id[12],
					       pj_stun_tx_data **p_tdata)
{
    pj_stun_tx_data *tdata = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && p_tdata, PJ_EINVAL);

    status = create_request_tdata(sess, method, magic, tsx_id, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    *p_tdata = tdata;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_create_ind(pj_stun_session *sess,
					       int msg_type,
					       pj_stun_tx_data **p_tdata)
{
    pj_stun_tx_data *tdata = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && p_tdata, PJ_EINVAL);

    status = create_tdata(sess, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create STUN message */
    msg_type |= PJ_STUN_INDICATION_BIT;
    status = pj_stun_msg_create(tdata->pool, msg_type,  PJ_STUN_MAGIC, 
				NULL, &tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_pool_release(tdata->pool);
	return status;
    }

    *p_tdata = tdata;
    return PJ_SUCCESS;
}

/*
 * Create a STUN response message.
 */
PJ_DEF(pj_status_t) pj_stun_session_create_res( pj_stun_session *sess,
						const pj_stun_msg *req,
						unsigned err_code,
						const pj_str_t *err_msg,
						pj_stun_tx_data **p_tdata)
{
    pj_status_t status;
    pj_stun_tx_data *tdata = NULL;

    status = create_tdata(sess, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create STUN response message */
    status = pj_stun_msg_create_response(tdata->pool, req, err_code, err_msg,
					 &tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_pool_release(tdata->pool);
	return status;
    }

    /* copy the request's transaction ID as the transaction key. */
    pj_assert(sizeof(tdata->msg_key)==sizeof(req->hdr.tsx_id));
    tdata->msg_magic = req->hdr.magic;
    pj_memcpy(tdata->msg_key, req->hdr.tsx_id, sizeof(req->hdr.tsx_id));

    *p_tdata = tdata;

    return PJ_SUCCESS;
}


/* Print outgoing message to log */
static void dump_tx_msg(pj_stun_session *sess, const pj_stun_msg *msg,
			unsigned pkt_size, const pj_sockaddr_t *addr)
{
    const char *dst_name;
    int dst_port;
    const pj_sockaddr *dst = (const pj_sockaddr*)addr;
    char buf[800];
    
    if (dst->addr.sa_family == pj_AF_INET()) {
	dst_name = pj_inet_ntoa(dst->ipv4.sin_addr);
	dst_port = pj_ntohs(dst->ipv4.sin_port);
    } else if (dst->addr.sa_family == pj_AF_INET6()) {
	dst_name = "IPv6";
	dst_port = pj_ntohs(dst->ipv6.sin6_port);
    } else {
	LOG_ERR_(sess, "Invalid address family", PJ_EINVAL);
	return;
    }

    PJ_LOG(5,(SNAME(sess), 
	      "TX %d bytes STUN message to %s:%d:\n"
	      "--- begin STUN message ---\n"
	      "%s"
	      "--- end of STUN message ---\n",
	      pkt_size, dst_name, dst_port,
	      pj_stun_msg_dump(msg, buf, sizeof(buf), NULL)));

}


PJ_DEF(pj_status_t) pj_stun_session_send_msg( pj_stun_session *sess,
					      pj_bool_t cache_res,
					      const pj_sockaddr_t *server,
					      unsigned addr_len,
					      pj_stun_tx_data *tdata)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && addr_len && server && tdata, PJ_EINVAL);

    /* Allocate packet */
    tdata->max_len = PJ_STUN_MAX_PKT_LEN;
    tdata->pkt = pj_pool_alloc(tdata->pool, tdata->max_len);

    /* Start locking the session now */
    pj_mutex_lock(sess->mutex);

    /* Apply options */
    status = apply_msg_options(sess, tdata->pool, tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_stun_msg_destroy_tdata(sess, tdata);
	pj_mutex_unlock(sess->mutex);
	LOG_ERR_(sess, "Error applying options", status);
	return status;
    }

    status = get_key(sess, tdata->pool, tdata->msg, &tdata->auth_key);
    if (status != PJ_SUCCESS) {
	pj_stun_msg_destroy_tdata(sess, tdata);
	pj_mutex_unlock(sess->mutex);
	LOG_ERR_(sess, "Error getting creadential's key", status);
	return status;
    }

    /* Encode message */
    status = pj_stun_msg_encode(tdata->msg, (pj_uint8_t*)tdata->pkt, 
    				tdata->max_len, 0, 
    				&tdata->auth_key,
				&tdata->pkt_size);
    if (status != PJ_SUCCESS) {
	pj_stun_msg_destroy_tdata(sess, tdata);
	pj_mutex_unlock(sess->mutex);
	LOG_ERR_(sess, "STUN encode() error", status);
	return status;
    }

    /* Dump packet */
    dump_tx_msg(sess, tdata->msg, tdata->pkt_size, server);

    /* If this is a STUN request message, then send the request with
     * a new STUN client transaction.
     */
    if (PJ_STUN_IS_REQUEST(tdata->msg->hdr.type)) {

	/* Create STUN client transaction */
	status = pj_stun_client_tsx_create(sess->cfg, tdata->pool, 
					   &tsx_cb, &tdata->client_tsx);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
	pj_stun_client_tsx_set_data(tdata->client_tsx, (void*)tdata);

	/* Save the remote address */
	tdata->addr_len = addr_len;
	tdata->dst_addr = server;

	/* Send the request! */
	status = pj_stun_client_tsx_send_msg(tdata->client_tsx, PJ_TRUE,
					     tdata->pkt, tdata->pkt_size);
	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    pj_stun_msg_destroy_tdata(sess, tdata);
	    pj_mutex_unlock(sess->mutex);
	    LOG_ERR_(sess, "Error sending STUN request", status);
	    return status;
	}

	/* Add to pending request list */
	tsx_add(sess, tdata);

    } else {
	if (cache_res && 
	    (PJ_STUN_IS_SUCCESS_RESPONSE(tdata->msg->hdr.type) ||
	     PJ_STUN_IS_ERROR_RESPONSE(tdata->msg->hdr.type))) 
	{
	    /* Requested to keep the response in the cache */
	    pj_time_val timeout;
	    
	    pj_memset(&tdata->res_timer, 0, sizeof(tdata->res_timer));
	    pj_timer_entry_init(&tdata->res_timer, PJ_TRUE, tdata, 
				&on_cache_timeout);

	    timeout.sec = sess->cfg->res_cache_msec / 1000;
	    timeout.msec = sess->cfg->res_cache_msec % 1000;

	    status = pj_timer_heap_schedule(sess->cfg->timer_heap, 
					    &tdata->res_timer,
					    &timeout);
	    if (status != PJ_SUCCESS) {
		pj_stun_msg_destroy_tdata(sess, tdata);
		pj_mutex_unlock(sess->mutex);
		LOG_ERR_(sess, "Error scheduling response timer", status);
		return status;
	    }

	    pj_list_push_back(&sess->cached_response_list, tdata);
	}
    
	/* Otherwise for non-request message, send directly to transport. */
	status = sess->cb.on_send_msg(sess, tdata->pkt, tdata->pkt_size,
				      server, addr_len);

	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    LOG_ERR_(sess, "Error sending STUN request", status);
	}

	/* Destroy only when response is not cached*/
	if (tdata->res_timer.id == 0) {
	    pj_stun_msg_destroy_tdata(sess, tdata);
	}
    }


    pj_mutex_unlock(sess->mutex);
    return status;
}

/*
 * Cancel outgoing STUN transaction. 
 */
PJ_DEF(pj_status_t) pj_stun_session_cancel_req( pj_stun_session *sess,
						pj_stun_tx_data *tdata,
						pj_bool_t notify,
						pj_status_t notify_status)
{
    PJ_ASSERT_RETURN(sess && tdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(!notify || notify_status!=PJ_SUCCESS, PJ_EINVAL);
    PJ_ASSERT_RETURN(PJ_STUN_IS_REQUEST(tdata->msg->hdr.type), PJ_EINVAL);

    pj_mutex_lock(sess->mutex);

    if (notify) {
	(sess->cb.on_request_complete)(sess, notify_status, tdata, NULL,
				       NULL, 0);
    }

    /* Just destroy tdata. This will destroy the transaction as well */
    pj_stun_msg_destroy_tdata(sess, tdata);

    pj_mutex_unlock(sess->mutex);
    return PJ_SUCCESS;
}

/*
 * Explicitly request retransmission of the request.
 */
PJ_DEF(pj_status_t) pj_stun_session_retransmit_req(pj_stun_session *sess,
						   pj_stun_tx_data *tdata)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && tdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(PJ_STUN_IS_REQUEST(tdata->msg->hdr.type), PJ_EINVAL);

    pj_mutex_lock(sess->mutex);

    status = pj_stun_client_tsx_retransmit(tdata->client_tsx);

    pj_mutex_unlock(sess->mutex);

    return status;
}


/* Send response */
static pj_status_t send_response(pj_stun_session *sess, 
				 pj_pool_t *pool, pj_stun_msg *response,
				 const pj_str_t *auth_key,
				 pj_bool_t retransmission,
				 const pj_sockaddr_t *addr, unsigned addr_len)
{
    pj_uint8_t *out_pkt;
    unsigned out_max_len, out_len;
    pj_status_t status;

    /* Apply options */
    if (!retransmission) {
	status = apply_msg_options(sess, pool, response);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Alloc packet buffer */
    out_max_len = PJ_STUN_MAX_PKT_LEN;
    out_pkt = (pj_uint8_t*) pj_pool_alloc(pool, out_max_len);

    /* Encode */
    status = pj_stun_msg_encode(response, out_pkt, out_max_len, 0, 
				auth_key, &out_len);
    if (status != PJ_SUCCESS) {
	LOG_ERR_(sess, "Error encoding message", status);
	return status;
    }

    /* Print log */
    dump_tx_msg(sess, response, out_len, addr);

    /* Send packet */
    status = sess->cb.on_send_msg(sess, out_pkt, out_len, addr, addr_len);

    return status;
}

/* Authenticate incoming message */
static pj_status_t authenticate_req(pj_stun_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_stun_msg *msg,
				    pj_pool_t *tmp_pool,
				    const pj_sockaddr_t *src_addr,
				    unsigned src_addr_len)
{
    pj_stun_msg *response;
    pj_status_t status;

    if (PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type) || sess->cred == NULL)
	return PJ_SUCCESS;

    status = pj_stun_authenticate_request(pkt, pkt_len, msg, sess->cred,
				          tmp_pool, &response);
    if (status != PJ_SUCCESS && response != NULL) {
	PJ_LOG(5,(SNAME(sess), "Message authentication failed"));
	send_response(sess, tmp_pool, response, NULL, PJ_FALSE, 
		      src_addr, src_addr_len);
    }

    return status;
}


/* Handle incoming response */
static pj_status_t on_incoming_response(pj_stun_session *sess,
					unsigned options,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					pj_stun_msg *msg,
					const pj_sockaddr_t *src_addr,
					unsigned src_addr_len)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* Lookup pending client transaction */
    tdata = tsx_lookup(sess, msg);
    if (tdata == NULL) {
	PJ_LOG(5,(SNAME(sess), 
		  "Transaction not found, response silently discarded"));
	return PJ_SUCCESS;
    }

    /* Authenticate the message, unless PJ_STUN_NO_AUTHENTICATE
     * is specified in the option.
     */
    if ((options & PJ_STUN_NO_AUTHENTICATE) == 0 && tdata->auth_key.slen != 0
	&& pj_stun_auth_valid_for_msg(msg))
    {
	status = pj_stun_authenticate_response(pkt, pkt_len, msg, 
					       &tdata->auth_key);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(5,(SNAME(sess), 
		      "Response authentication failed"));
	    return status;
	}
    }

    /* Pass the response to the transaction. 
     * If the message is accepted, transaction callback will be called,
     * and this will call the session callback too.
     */
    status = pj_stun_client_tsx_on_rx_msg(tdata->client_tsx, msg, 
					  src_addr, src_addr_len);
    if (status != PJ_SUCCESS) {
	return status;
    }

    /* If transaction has completed, destroy the transmit data.
     * This will remove the transaction from the pending list too.
     */
    if (pj_stun_client_tsx_is_complete(tdata->client_tsx)) {
	pj_stun_msg_destroy_tdata(sess, tdata);
	tdata = NULL;
    }

    return PJ_SUCCESS;
}


/* For requests, check if we cache the response */
static pj_status_t check_cached_response(pj_stun_session *sess,
					 pj_pool_t *tmp_pool,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    pj_stun_tx_data *t;

    /* First lookup response in response cache */
    t = sess->cached_response_list.next;
    while (t != &sess->cached_response_list) {
	if (t->msg_magic == msg->hdr.magic &&
	    t->msg->hdr.type == msg->hdr.type &&
	    pj_memcmp(t->msg_key, msg->hdr.tsx_id, 
		      sizeof(msg->hdr.tsx_id))==0)
	{
	    break;
	}
	t = t->next;
    }

    if (t != &sess->cached_response_list) {
	/* Found response in the cache */

	PJ_LOG(5,(SNAME(sess), 
		 "Request retransmission, sending cached response"));

	send_response(sess, tmp_pool, t->msg, &t->auth_key, PJ_TRUE, 
		      src_addr, src_addr_len);
	return PJ_SUCCESS;
    }

    return PJ_ENOTFOUND;
}

/* Handle incoming request */
static pj_status_t on_incoming_request(pj_stun_session *sess,
				       unsigned options,
				       pj_pool_t *tmp_pool,
				       const pj_uint8_t *in_pkt,
				       unsigned in_pkt_len,
				       const pj_stun_msg *msg,
				       const pj_sockaddr_t *src_addr,
				       unsigned src_addr_len)
{
    pj_status_t status;

    /* Authenticate the message, unless PJ_STUN_NO_AUTHENTICATE
     * is specified in the option.
     */
    if ((options & PJ_STUN_NO_AUTHENTICATE) == 0) {
	status = authenticate_req(sess, (const pj_uint8_t*) in_pkt, in_pkt_len,
				  msg, tmp_pool, src_addr, src_addr_len);
	if (status != PJ_SUCCESS) {
	    return status;
	}
    }

    /* Distribute to handler, or respond with Bad Request */
    if (sess->cb.on_rx_request) {
	status = (*sess->cb.on_rx_request)(sess, in_pkt, in_pkt_len, msg,
					   src_addr, src_addr_len);
    } else {
	pj_stun_msg *response;

	status = pj_stun_msg_create_response(tmp_pool, msg, 
					     PJ_STUN_SC_BAD_REQUEST, NULL,
					     &response);
	if (status == PJ_SUCCESS && response) {
	    status = send_response(sess, tmp_pool, response, 
				   NULL, PJ_FALSE, src_addr, src_addr_len);
	}
    }

    return status;
}


/* Handle incoming indication */
static pj_status_t on_incoming_indication(pj_stun_session *sess,
					  pj_pool_t *tmp_pool,
					  const pj_uint8_t *in_pkt,
					  unsigned in_pkt_len,
					  const pj_stun_msg *msg,
					  const pj_sockaddr_t *src_addr,
					  unsigned src_addr_len)
{
    PJ_UNUSED_ARG(tmp_pool);

    /* Distribute to handler */
    if (sess->cb.on_rx_indication) {
	return (*sess->cb.on_rx_indication)(sess, in_pkt, in_pkt_len, msg,
					    src_addr, src_addr_len);
    } else {
	return PJ_SUCCESS;
    }
}


PJ_DEF(pj_status_t) pj_stun_session_on_rx_pkt(pj_stun_session *sess,
					      const void *packet,
					      pj_size_t pkt_size,
					      unsigned options,
					      unsigned *parsed_len,
					      const pj_sockaddr_t *src_addr,
					      unsigned src_addr_len)
{
    pj_stun_msg *msg, *response;
    pj_pool_t *tmp_pool;
    char *dump;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && packet && pkt_size, PJ_EINVAL);

    tmp_pool = pj_pool_create(sess->cfg->pf, "tmpstun", 
			      PJNATH_POOL_LEN_STUN_TDATA, 
			      PJNATH_POOL_INC_STUN_TDATA, NULL);
    if (!tmp_pool)
	return PJ_ENOMEM;

    /* Try to parse the message */
    status = pj_stun_msg_decode(tmp_pool, (const pj_uint8_t*)packet,
			        pkt_size, options, 
				&msg, parsed_len, &response);
    if (status != PJ_SUCCESS) {
	LOG_ERR_(sess, "STUN msg_decode() error", status);
	if (response) {
	    send_response(sess, tmp_pool, response, NULL,
			  PJ_FALSE, src_addr, src_addr_len);
	}
	pj_pool_release(tmp_pool);
	return status;
    }

    dump = (char*) pj_pool_alloc(tmp_pool, PJ_STUN_MAX_PKT_LEN);

    PJ_LOG(5,(SNAME(sess),
	      "RX STUN message from %s:%d:\n"
	      "--- begin STUN message ---\n"
	      "%s"
	      "--- end of STUN message ---\n",
	      pj_inet_ntoa(((pj_sockaddr_in*)src_addr)->sin_addr),
	      pj_ntohs(((pj_sockaddr_in*)src_addr)->sin_port),
	      pj_stun_msg_dump(msg, dump, PJ_STUN_MAX_PKT_LEN, NULL)));

    pj_mutex_lock(sess->mutex);

    /* For requests, check if we have cached response */
    status = check_cached_response(sess, tmp_pool, msg, 
				   src_addr, src_addr_len);
    if (status == PJ_SUCCESS) {
	goto on_return;
    }

    /* Handle message */
    if (PJ_STUN_IS_SUCCESS_RESPONSE(msg->hdr.type) ||
	PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))
    {
	status = on_incoming_response(sess, options, 
				      (const pj_uint8_t*) packet, pkt_size, 
				      msg, src_addr, src_addr_len);

    } else if (PJ_STUN_IS_REQUEST(msg->hdr.type)) {

	status = on_incoming_request(sess, options, tmp_pool, 
				     (const pj_uint8_t*) packet, pkt_size, 
				     msg, src_addr, src_addr_len);

    } else if (PJ_STUN_IS_INDICATION(msg->hdr.type)) {

	status = on_incoming_indication(sess, tmp_pool, 
					(const pj_uint8_t*) packet, pkt_size,
					msg, src_addr, src_addr_len);

    } else {
	pj_assert(!"Unexpected!");
	status = PJ_EBUG;
    }

on_return:
    pj_mutex_unlock(sess->mutex);

    pj_pool_release(tmp_pool);
    return status;
}




/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util/stun_session.h>
#include <pjlib.h>

struct pj_stun_session
{
    pj_stun_endpoint	*endpt;
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

#if PJ_LOG_MAX_LEVEL >= 4
#   define LOG_ERR_(sess, title, rc) stun_perror(sess, title, rc)
static void stun_perror(pj_stun_session *sess, const char *title, 
			pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(4,(SNAME(sess), "%s: %s", title, errmsg));
}

#else
#   define LOG_ERR_(sess, title, rc)
#endif

#define TDATA_POOL_SIZE		    1024
#define TDATA_POOL_INC		    1024


static void tsx_on_complete(pj_stun_client_tsx *tsx,
			    pj_status_t status, 
			    const pj_stun_msg *response);
static pj_status_t tsx_on_send_msg(pj_stun_client_tsx *tsx,
				   const void *stun_pkt,
				   pj_size_t pkt_size);

static pj_stun_tsx_cb tsx_cb = 
{
    &tsx_on_complete,
    &tsx_on_send_msg
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
				void *user_data,
			        pj_stun_tx_data **p_tdata)
{
    pj_pool_t *pool;
    pj_stun_tx_data *tdata;

    /* Create pool and initialize basic tdata attributes */
    pool = pj_pool_create(sess->endpt->pf, "tdata%p", 
			  TDATA_POOL_SIZE, TDATA_POOL_INC, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    tdata = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_tx_data);
    tdata->pool = pool;
    tdata->sess = sess;
    tdata->user_data = user_data;

    *p_tdata = tdata;

    return PJ_SUCCESS;
}

static pj_status_t create_request_tdata(pj_stun_session *sess,
					unsigned msg_type,
					void *user_data,
					pj_stun_tx_data **p_tdata)
{
    pj_status_t status;
    pj_stun_tx_data *tdata;

    status = create_tdata(sess, user_data, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create STUN message */
    status = pj_stun_msg_create(tdata->pool, msg_type,  PJ_STUN_MAGIC, 
				NULL, &tdata->msg);
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

static void destroy_tdata(pj_stun_tx_data *tdata)
{
    if (tdata->client_tsx) {
	tsx_erase(tdata->sess, tdata);
	pj_stun_client_tsx_destroy(tdata->client_tsx);
	tdata->client_tsx = NULL;
    }
    if (tdata->res_timer.id != PJ_FALSE) {
	pj_timer_heap_cancel(tdata->sess->endpt->timer_heap, 
			     &tdata->res_timer);
	tdata->res_timer.id = PJ_FALSE;
    }
    pj_pool_release(tdata->pool);
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

static pj_str_t *get_passwd(pj_stun_session *sess)
{
    if (sess->cred == NULL)
	return NULL;
    else if (sess->cred->type == PJ_STUN_AUTH_CRED_STATIC)
	return &sess->cred->data.static_cred.data;
    else
	return NULL;
}

static pj_status_t apply_msg_options(pj_stun_session *sess,
				     pj_pool_t *pool,
				     pj_stun_msg *msg)
{
    pj_status_t status = 0;

    /* The server SHOULD include a SERVER attribute in all responses */
    if (PJ_STUN_IS_RESPONSE(msg->hdr.type) ||
	PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type)) 
    {
	pj_stun_msg_add_string_attr(pool, msg, PJ_STUN_ATTR_SERVER,
				    &sess->srv_name);
    }

    /* From draft-ietf-behave-rfc3489bis-05.txt
     * Section 8.3.1.  Formulating the Request Message
     *
     * Note: only put MESSAGE-INTEGRITY in non error response.
     */
    if (sess->cred && sess->cred->type == PJ_STUN_AUTH_CRED_STATIC &&
	!PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type)) 
    {
	const pj_str_t *username;

	/* Create and add USERNAME attribute */
	username = &sess->cred->data.static_cred.username;
	status = pj_stun_msg_add_string_attr(pool, msg,
					     PJ_STUN_ATTR_USERNAME,
					     username);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	/* Add REALM only when long term credential is used */
	if (sess->cred->data.static_cred.realm.slen) {
	    const pj_str_t *realm = &sess->cred->data.static_cred.realm;
	    status = pj_stun_msg_add_string_attr(pool, msg,
						PJ_STUN_ATTR_REALM,
						realm);
	}

	/* Add MESSAGE-INTEGRITY attribute */
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


static void tsx_on_complete(pj_stun_client_tsx *tsx,
			    pj_status_t status, 
			    const pj_stun_msg *response)
{
    pj_stun_tx_data *tdata;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);

    if (tdata->sess->cb.on_request_complete) {
	(*tdata->sess->cb.on_request_complete)(tdata->sess, status, tdata, 
					       response);
    }
}

static pj_status_t tsx_on_send_msg(pj_stun_client_tsx *tsx,
				   const void *stun_pkt,
				   pj_size_t pkt_size)
{
    pj_stun_tx_data *tdata;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);

    return tdata->sess->cb.on_send_msg(tdata->sess, stun_pkt, pkt_size,
				       tdata->dst_addr, tdata->addr_len);
}

/* **************************************************************************/

PJ_DEF(pj_status_t) pj_stun_session_create( pj_stun_endpoint *endpt,
					    const char *name,
					    const pj_stun_session_cb *cb,
					    pj_bool_t fingerprint,
					    pj_stun_session **p_sess)
{
    pj_pool_t	*pool;
    pj_stun_session *sess;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && cb && p_sess, PJ_EINVAL);

    if (name==NULL)
	name = "sess%p";

    pool = pj_pool_create(endpt->pf, name, 4000, 4000, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    sess = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_session);
    sess->endpt = endpt;
    sess->pool = pool;
    pj_memcpy(&sess->cb, cb, sizeof(*cb));
    sess->use_fingerprint = fingerprint;
    
    sess->srv_name.ptr = pj_pool_alloc(pool, 32);
    sess->srv_name.slen = pj_ansi_snprintf(sess->srv_name.ptr, 32,
					   "pj_stun-%s", PJ_VERSION);

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
    PJ_ASSERT_RETURN(sess && srv_name, PJ_EINVAL);
    pj_strdup(sess->pool, &sess->srv_name, srv_name);
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_stun_session_set_credential(pj_stun_session *sess,
					    const pj_stun_auth_cred *cred)
{
    PJ_ASSERT_ON_FAIL(sess, return);
    if (cred) {
	if (!sess->cred)
	    sess->cred = pj_pool_alloc(sess->pool, sizeof(pj_stun_auth_cred));
	pj_stun_auth_cred_dup(sess->pool, sess->cred, cred);
    } else {
	sess->cred = NULL;
    }
}


PJ_DEF(pj_status_t) pj_stun_session_create_bind_req(pj_stun_session *sess,
						    pj_stun_tx_data **p_tdata)
{
    pj_stun_tx_data *tdata = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && p_tdata, PJ_EINVAL);

    status = create_request_tdata(sess, PJ_STUN_BINDING_REQUEST, NULL, 
				  &tdata);
    if (status != PJ_SUCCESS)
	return status;

    *p_tdata = tdata;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_create_allocate_req(pj_stun_session *sess,
							pj_stun_tx_data **p_tdata)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(p_tdata);
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}


PJ_DEF(pj_status_t) 
pj_stun_session_create_set_active_destination_req(pj_stun_session *sess,
						  pj_stun_tx_data **p_tdata)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(p_tdata);
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) pj_stun_session_create_connect_req(	pj_stun_session *sess,
						        pj_stun_tx_data **p_tdata)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(p_tdata);
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) 
pj_stun_session_create_connection_status_ind(pj_stun_session *sess,
					     pj_stun_tx_data **p_tdata)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(p_tdata);
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) pj_stun_session_create_send_ind( pj_stun_session *sess,
						     pj_stun_tx_data **p_tdata)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(p_tdata);
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) pj_stun_session_create_data_ind( pj_stun_session *sess,
						     pj_stun_tx_data **p_tdata)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(p_tdata);
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}


/*
 * Create a STUN response message.
 */
PJ_DEF(pj_status_t) pj_stun_session_create_response( pj_stun_session *sess,
						     const pj_stun_msg *req,
						     unsigned err_code,
						     const pj_str_t *err_msg,
						     pj_stun_tx_data **p_tdata)
{
    pj_status_t status;
    pj_stun_tx_data *tdata = NULL;

    status = create_tdata(sess, NULL, &tdata);
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
    char buf[512];
    
    if (dst->sa_family == PJ_AF_INET) {
	const pj_sockaddr_in *dst4 = (const pj_sockaddr_in*)dst;
	dst_name = pj_inet_ntoa(dst4->sin_addr);
	dst_port = pj_ntohs(dst4->sin_port);
    } else if (dst->sa_family == PJ_AF_INET6) {
	const pj_sockaddr_in6 *dst6 = (const pj_sockaddr_in6*)dst;
	dst_name = "IPv6";
	dst_port = pj_ntohs(dst6->sin6_port);
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

    /* Encode message */
    status = pj_stun_msg_encode(tdata->msg, tdata->pkt, tdata->max_len,
			        0, get_passwd(sess), &tdata->pkt_size);
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
	status = pj_stun_client_tsx_create(sess->endpt, tdata->pool, 
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
	    (PJ_STUN_IS_RESPONSE(tdata->msg->hdr.type) ||
	     PJ_STUN_IS_ERROR_RESPONSE(tdata->msg->hdr.type))) 
	{
	    /* Requested to keep the response in the cache */
	    pj_time_val timeout;
	    
	    pj_memset(&tdata->res_timer, 0, sizeof(tdata->res_timer));
	    pj_timer_entry_init(&tdata->res_timer, PJ_TRUE, tdata, 
				&on_cache_timeout);

	    timeout.sec = sess->endpt->res_cache_msec / 1000;
	    timeout.msec = sess->endpt->res_cache_msec % 1000;

	    status = pj_timer_heap_schedule(sess->endpt->timer_heap, 
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


/* Send response */
static pj_status_t send_response(pj_stun_session *sess, 
				 pj_pool_t *pool, pj_stun_msg *response,
				 pj_bool_t retransmission,
				 const pj_sockaddr_t *addr, unsigned addr_len)
{
    pj_uint8_t *out_pkt;
    unsigned out_max_len, out_len;
    pj_status_t status;

    /* Alloc packet buffer */
    out_max_len = PJ_STUN_MAX_PKT_LEN;
    out_pkt = pj_pool_alloc(pool, out_max_len);

    /* Apply options */
    if (!retransmission) {
	apply_msg_options(sess, pool, response);
    }

    /* Encode */
    status = pj_stun_msg_encode(response, out_pkt, out_max_len, 0, 
				get_passwd(sess), &out_len);
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
static pj_status_t authenticate_msg(pj_stun_session *sess,
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

    status = pj_stun_verify_credential(pkt, pkt_len, msg, sess->cred,
				       tmp_pool, &response);
    if (status != PJ_SUCCESS && response != NULL) {
	PJ_LOG(5,(SNAME(sess), "Message authentication failed"));
	send_response(sess, tmp_pool, response, PJ_FALSE, 
		      src_addr, src_addr_len);
    }

    return status;
}


/* Handle incoming response */
static pj_status_t on_incoming_response(pj_stun_session *sess,
					pj_stun_msg *msg)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* Lookup pending client transaction */
    tdata = tsx_lookup(sess, msg);
    if (tdata == NULL) {
	PJ_LOG(4,(SNAME(sess), 
		  "Transaction not found, response silently discarded"));
	return PJ_SUCCESS;
    }

    /* Pass the response to the transaction. 
     * If the message is accepted, transaction callback will be called,
     * and this will call the session callback too.
     */
    status = pj_stun_client_tsx_on_rx_msg(tdata->client_tsx, msg);
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

	send_response(sess, tmp_pool, t->msg, PJ_TRUE, 
		      src_addr, src_addr_len);
	return PJ_SUCCESS;
    }

    return PJ_ENOTFOUND;
}

/* Handle incoming request */
static pj_status_t on_incoming_request(pj_stun_session *sess,
				       pj_pool_t *tmp_pool,
				       const pj_uint8_t *in_pkt,
				       unsigned in_pkt_len,
				       const pj_stun_msg *msg,
				       const pj_sockaddr_t *src_addr,
				       unsigned src_addr_len)
{
    pj_status_t status;

    /* Distribute to handler, or respond with Bad Request */
    if (sess->cb.on_rx_request) {
	status = (*sess->cb.on_rx_request)(sess, in_pkt, in_pkt_len, msg,
					   src_addr, src_addr_len);
    } else {
	pj_stun_msg *response;

	status = pj_stun_msg_create_response(tmp_pool, msg, 
					     PJ_STUN_STATUS_BAD_REQUEST, NULL,
					     &response);
	if (status == PJ_SUCCESS && response) {
	    status = send_response(sess, tmp_pool, response, 
				   PJ_FALSE, src_addr, src_addr_len);
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

    tmp_pool = pj_pool_create(sess->endpt->pf, "tmpstun", 1024, 1024, NULL);
    if (!tmp_pool)
	return PJ_ENOMEM;

    /* Try to parse the message */
    status = pj_stun_msg_decode(tmp_pool, (const pj_uint8_t*)packet,
			        pkt_size, options, 
				&msg, parsed_len, &response);
    if (status != PJ_SUCCESS) {
	LOG_ERR_(sess, "STUN msg_decode() error", status);
	if (response) {
	    send_response(sess, tmp_pool, response, 
			  PJ_FALSE, src_addr, src_addr_len);
	}
	pj_pool_release(tmp_pool);
	return status;
    }

    dump = pj_pool_alloc(tmp_pool, PJ_STUN_MAX_PKT_LEN);

    PJ_LOG(5,(SNAME(sess),
	      "RX STUN message:\n"
	      "--- begin STUN message ---\n"
	      "%s"
	      "--- end of STUN message ---\n",
	      pj_stun_msg_dump(msg, dump, PJ_STUN_MAX_PKT_LEN, NULL)));

    pj_mutex_lock(sess->mutex);

    /* For requests, check if we have cached response */
    status = check_cached_response(sess, tmp_pool, msg, 
				   src_addr, src_addr_len);
    if (status == PJ_SUCCESS) {
	goto on_return;
    }

    /* Authenticate the message */
    status = authenticate_msg(sess, packet, pkt_size, msg, tmp_pool, 
			      src_addr, src_addr_len);
    if (status != PJ_SUCCESS) {
	goto on_return;
    }

    /* Handle message */
    if (PJ_STUN_IS_RESPONSE(msg->hdr.type) ||
	PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))
    {
	status = on_incoming_response(sess, msg);

    } else if (PJ_STUN_IS_REQUEST(msg->hdr.type)) {

	status = on_incoming_request(sess, tmp_pool, packet, pkt_size, msg,
				     src_addr, src_addr_len);

    } else if (PJ_STUN_IS_INDICATION(msg->hdr.type)) {

	status = on_incoming_indication(sess, tmp_pool, packet, pkt_size,
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



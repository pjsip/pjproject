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
    pj_stun_session_cb	 cb;
    void		*user_data;

    /* Long term credential */
    pj_str_t		 l_realm;
    pj_str_t		 l_username;
    pj_str_t		 l_password;

    /* Short term credential */
    pj_str_t		 s_username;
    pj_str_t		 s_password;

    pj_stun_tx_data	 pending_request_list;
};

#define SNAME(s_)		    ((s_)->pool->obj_name)

#if PJ_LOG_MAX_LEVEL >= 5
#   define TRACE_(expr)		    PJ_LOG(5,expr)
#else
#   define TRACE_(expr)
#endif

#if PJ_LOG_MAX_LEVEL >= 4
#   define LOG_ERR_(sess, title, rc)
static void stun_perror(pj_stun_session *sess, const char *title, 
			pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(4,(SNAME(sess), "%s: %s", title, errmsg));
}

#else
#   define ERR_(sess, title, rc)
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
	pj_assert(sizeof(tdata->client_key)==sizeof(msg->hdr.tsx_id));
	if (pj_memcmp(tdata->client_key, msg->hdr.tsx_id, 
		      sizeof(msg->hdr.tsx_id))==0)
	{
	    return tdata;
	}
	tdata = tdata->next;
    }

    return NULL;
}

static pj_status_t create_tdata(pj_stun_session *sess,
				unsigned msg_type,
				void *user_data,
			        pj_stun_tx_data **p_tdata)
{
    pj_pool_t *pool;
    pj_status_t status;
    pj_stun_tx_data *tdata;

    /* Create pool and initialize basic tdata attributes */
    pool = pj_pool_create(sess->endpt->pf, "tdata%p", 
			  TDATA_POOL_SIZE, TDATA_POOL_INC, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    tdata = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_tx_data);
    tdata->pool = pool;
    tdata->sess = sess;
    tdata->user_data = user_data;

    /* Create STUN message */
    status = pj_stun_msg_create(pool, msg_type,  PJ_STUN_MAGIC, 
				NULL, &tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    /* If this is a request, then copy the request's transaction ID
     * as the transaction key.
     */
    if (PJ_STUN_IS_REQUEST(msg_type)) {
	pj_assert(sizeof(tdata->client_key)==sizeof(tdata->msg->hdr.tsx_id));
	pj_memcpy(tdata->client_key, tdata->msg->hdr.tsx_id,
		  sizeof(tdata->msg->hdr.tsx_id));
    }

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

    pj_pool_release(tdata->pool);
}

static pj_status_t session_apply_req(pj_stun_session *sess,
				     pj_pool_t *pool,
				     unsigned options,
				     pj_stun_msg *msg)
{
    pj_status_t status;

    /* From draft-ietf-behave-rfc3489bis-05.txt
     * Section 8.3.1.  Formulating the Request Message
     */
    if (options & PJ_STUN_USE_LONG_TERM_CRED) {
	pj_stun_generic_string_attr *auname;
	pj_stun_msg_integrity_attr *amsgi;
	pj_stun_generic_string_attr *arealm;

	/* Create and add USERNAME attribute */
	status = pj_stun_generic_string_attr_create(sess->pool, 
						    PJ_STUN_ATTR_USERNAME,
						    &sess->l_username,
						    &auname);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	status = pj_stun_msg_add_attr(msg, &auname->hdr);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	/* Add REALM only when long term credential is used */
	status = pj_stun_generic_string_attr_create(sess->pool, 
						    PJ_STUN_ATTR_REALM,
						    &sess->l_realm,
						    &arealm);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	status = pj_stun_msg_add_attr(msg, &arealm->hdr);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	/* Add MESSAGE-INTEGRITY attribute */
	status = pj_stun_msg_integrity_attr_create(sess->pool, &amsgi);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	status = pj_stun_msg_add_attr(msg, &amsgi->hdr);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	PJ_TODO(COMPUTE_MESSAGE_INTEGRITY1);

    } else if (options & PJ_STUN_USE_SHORT_TERM_CRED) {
	pj_stun_generic_string_attr *auname;
	pj_stun_msg_integrity_attr *amsgi;

	/* Create and add USERNAME attribute */
	status = pj_stun_generic_string_attr_create(sess->pool, 
						    PJ_STUN_ATTR_USERNAME,
						    &sess->s_username,
						    &auname);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	status = pj_stun_msg_add_attr(msg, &auname->hdr);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	/* Add MESSAGE-INTEGRITY attribute */
	status = pj_stun_msg_integrity_attr_create(sess->pool, &amsgi);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	status = pj_stun_msg_add_attr(msg, &amsgi->hdr);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	PJ_TODO(COMPUTE_MESSAGE_INTEGRITY2);
    }

    /* Add FINGERPRINT attribute if necessary */
    if (options & PJ_STUN_USE_FINGERPRINT) {
	pj_stun_fingerprint_attr *af;

	status = pj_stun_generic_uint_attr_create(sess->pool, 
						  PJ_STUN_ATTR_FINGERPRINT,
						  0, &af);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

	status = pj_stun_msg_add_attr(msg, &af->hdr);
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

    switch (PJ_STUN_GET_METHOD(tdata->msg->hdr.type)) {
    case PJ_STUN_BINDING_METHOD:
	tdata->sess->cb.on_bind_response(tdata->sess, status, tdata, response);
	break;
    case PJ_STUN_ALLOCATE_METHOD:
	tdata->sess->cb.on_allocate_response(tdata->sess, status,
					     tdata, response);
	break;
    case PJ_STUN_SET_ACTIVE_DESTINATION_METHOD:
	tdata->sess->cb.on_set_active_destination_response(tdata->sess, status,
							   tdata, response);
	break;
    case PJ_STUN_CONNECT_METHOD:
	tdata->sess->cb.on_connect_response(tdata->sess, status, tdata,
					    response);
	break;
    default:
	pj_assert(!"Unknown method");
	break;
    }
}

static pj_status_t tsx_on_send_msg(pj_stun_client_tsx *tsx,
				   const void *stun_pkt,
				   pj_size_t pkt_size)
{
    pj_stun_tx_data *tdata;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);

    return tdata->sess->cb.on_send_msg(tdata, stun_pkt, pkt_size,
				       tdata->addr_len, tdata->dst_addr);
}

/* **************************************************************************/

PJ_DEF(pj_status_t) pj_stun_session_create( pj_stun_endpoint *endpt,
					    const char *name,
					    const pj_stun_session_cb *cb,
					    pj_stun_session **p_sess)
{
    pj_pool_t	*pool;
    pj_stun_session *sess;

    PJ_ASSERT_RETURN(endpt && cb && p_sess, PJ_EINVAL);

    pool = pj_pool_create(endpt->pf, name, 4000, 4000, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    sess = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_session);
    sess->endpt = endpt;
    sess->pool = pool;
    pj_memcpy(&sess->cb, cb, sizeof(*cb));

    pj_list_init(&sess->pending_request_list);

    *p_sess = sess;

    PJ_TODO(MUTEX_PROTECTION);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_destroy(pj_stun_session *sess)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    pj_pool_release(sess->pool);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_session_set_user_data( pj_stun_session *sess,
						   void *user_data)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);
    sess->user_data = user_data;
    return PJ_SUCCESS;
}

PJ_DEF(void*) pj_stun_session_get_user_data(pj_stun_session *sess)
{
    PJ_ASSERT_RETURN(sess, NULL);
    return sess->user_data;
}

PJ_DEF(pj_status_t) 
pj_stun_session_set_long_term_credential(pj_stun_session *sess,
					 const pj_str_t *realm,
					 const pj_str_t *user,
					 const pj_str_t *passwd)
{
    pj_str_t nil = { NULL, 0 };

    PJ_ASSERT_RETURN(sess, PJ_EINVAL);
    pj_strdup_with_null(sess->pool, &sess->l_realm, realm ? realm : &nil);
    pj_strdup_with_null(sess->pool, &sess->l_username, user ? user : &nil);
    pj_strdup_with_null(sess->pool, &sess->l_password, passwd ? passwd : &nil);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) 
pj_stun_session_set_short_term_credential(pj_stun_session *sess,
					  const pj_str_t *user,
					  const pj_str_t *passwd)
{
    pj_str_t nil = { NULL, 0 };

    PJ_ASSERT_RETURN(sess, PJ_EINVAL);
    pj_strdup_with_null(sess->pool, &sess->s_username, user ? user : &nil);
    pj_strdup_with_null(sess->pool, &sess->s_password, passwd ? passwd : &nil);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_session_create_bind_req(pj_stun_session *sess,
						    pj_stun_tx_data **p_tdata)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && p_tdata, PJ_EINVAL);

    status = create_tdata(sess, PJ_STUN_BINDING_REQUEST, NULL, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    *p_tdata = tdata;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_create_allocate_req(pj_stun_session *sess,
							pj_stun_tx_data **p_tdata)
{
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}


PJ_DEF(pj_status_t) 
pj_stun_session_create_set_active_destination_req(pj_stun_session *sess,
						  pj_stun_tx_data **p_tdata)
{
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) pj_stun_session_create_connect_req(	pj_stun_session *sess,
						        pj_stun_tx_data **p_tdata)
{
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) 
pj_stun_session_create_connection_status_ind(pj_stun_session *sess,
					     pj_stun_tx_data **p_tdata)
{
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) pj_stun_session_create_send_ind( pj_stun_session *sess,
						     pj_stun_tx_data **p_tdata)
{
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) pj_stun_session_create_data_ind( pj_stun_session *sess,
						     pj_stun_tx_data **p_tdata)
{
    PJ_ASSERT_RETURN(PJ_FALSE, PJ_ENOTSUP);
}

PJ_DEF(pj_status_t) pj_stun_session_send_msg( pj_stun_session *sess,
					      unsigned options,
					      unsigned addr_len,
					      const pj_sockaddr_t *server,
					      pj_stun_tx_data *tdata)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && addr_len && server && tdata, PJ_EINVAL);

    /* Allocate packet */
    tdata->max_len = PJ_STUN_MAX_PKT_LEN;
    tdata->pkt = pj_pool_alloc(tdata->pool, tdata->max_len);

    if (PJ_LOG_MAX_LEVEL >= 5) {
	char *buf = (char*) tdata->pkt;
	const char *dst_name;
	int dst_port;
	const pj_sockaddr *dst = (const pj_sockaddr*)server;
	
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
	    return PJ_EINVAL;
	}

	PJ_LOG(5,(SNAME(sess), 
		  "Sending STUN message to %s:%d:\n"
		  "--- begin STUN message ---\n"
		  "%s"
		  "--- end of STUN message ---\n",
		  dst_name, dst_port,
		  pj_stun_msg_dump(tdata->msg, buf, tdata->max_len, NULL)));
    }

    /* Apply options */
    status = session_apply_req(sess, tdata->pool, options, tdata->msg);
    if (status != PJ_SUCCESS) {
	LOG_ERR_(sess, "Error applying options", status);
	destroy_tdata(tdata);
	return status;
    }

    /* Encode message */
    status = pj_stun_msg_encode(tdata->msg, tdata->pkt, tdata->max_len,
			        0, NULL, &tdata->pkt_size);
    if (status != PJ_SUCCESS) {
	LOG_ERR_(sess, "STUN encode() error", status);
	destroy_tdata(tdata);
	return status;
    }

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
	    LOG_ERR_(sess, "Error sending STUN request", status);
	    destroy_tdata(tdata);
	    return status;
	}

	/* Add to pending request list */
	tsx_add(sess, tdata);

    } else {
	/* Otherwise for non-request message, send directly to transport. */
	status = sess->cb.on_send_msg(tdata, tdata->pkt, tdata->pkt_size,
				      addr_len, server);

	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    LOG_ERR_(sess, "Error sending STUN request", status);
	    destroy_tdata(tdata);
	    return status;
	}
    }


    return status;
}


PJ_DEF(pj_status_t) pj_stun_session_on_rx_pkt(pj_stun_session *sess,
					      const void *packet,
					      pj_size_t pkt_size,
					      unsigned *parsed_len)
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
			        pkt_size, 0, &msg, parsed_len,
				&response);
    if (status != PJ_SUCCESS) {
	LOG_ERR_(sess, "STUN msg_decode() error", status);
	if (response) {
	    PJ_TODO(SEND_RESPONSE);
	}
	pj_pool_release(tmp_pool);
	return status;
    }

    dump = pj_pool_alloc(tmp_pool, PJ_STUN_MAX_PKT_LEN);

    PJ_LOG(4,(SNAME(sess), 
	      "RX STUN message:\n"
	      "--- begin STUN message ---"
	      "%s"
	      "--- end of STUN message ---\n",
	      pj_stun_msg_dump(msg, dump, PJ_STUN_MAX_PKT_LEN, NULL)));


    if (PJ_STUN_IS_RESPONSE(msg->hdr.type) ||
	PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))
    {
	pj_stun_tx_data *tdata;

	/* Lookup pending client transaction */
	tdata = tsx_lookup(sess, msg);
	if (tdata == NULL) {
	    LOG_ERR_(sess, "STUN error finding transaction", PJ_ENOTFOUND);
	    pj_pool_release(tmp_pool);
	    return PJ_ENOTFOUND;
	}

	/* Pass the response to the transaction. 
	 * If the message is accepted, transaction callback will be called,
	 * and this will call the session callback too.
	 */
	status = pj_stun_client_tsx_on_rx_msg(tdata->client_tsx, msg);
	if (status != PJ_SUCCESS) {
	    pj_pool_release(tmp_pool);
	    return status;
	}

	/* If transaction has completed, destroy the transmit data.
	 * This will remove the transaction from the pending list too.
	 */
	if (pj_stun_client_tsx_is_complete(tdata->client_tsx)) {
	    destroy_tdata(tdata);
	    tdata = NULL;
	}

	pj_pool_release(tmp_pool);
	return PJ_SUCCESS;

    } else if (PJ_STUN_IS_REQUEST(msg->hdr.type)) {

	PJ_TODO(HANDLE_INCOMING_STUN_REQUEST);

    } else if (PJ_STUN_IS_INDICATION(msg->hdr.type)) {

	PJ_TODO(HANDLE_INCOMING_STUN_INDICATION);

    } else {
	pj_assert(!"Unexpected!");
    }

    pj_pool_release(tmp_pool);
    return PJ_ENOTSUP;
}


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
#include <pjnath/ice.h>
#include <pjnath/errno.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


static const char *check_state_name[] = 
{
    "Frozen",
    "Waiting",
    "In Progress",
    "Succeeded",
    "Failed"
};

static void destroy_ice(pj_ice *ice,
			pj_status_t reason);
static void ice_set_state(pj_ice *ice,
			  pj_ice_state new_state);

static pj_status_t on_stun_send_msg(pj_stun_session *sess,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len);
static pj_status_t on_stun_rx_request(pj_stun_session *sess,
				      const pj_uint8_t *pkt,
				      unsigned pkt_len,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len);
static void on_stun_request_complete(pj_stun_session *sess,
				     pj_status_t status,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response);
static pj_status_t on_stun_rx_indication(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len);

static pj_status_t stun_auth_get_auth(void *user_data,
				      pj_pool_t *pool,
				      pj_str_t *realm,
				      pj_str_t *nonce);
static pj_status_t stun_auth_get_password(void *user_data, 
					  const pj_str_t *realm,
					  const pj_str_t *username,
					  pj_pool_t *pool,
					  int *data_type,
					  pj_str_t *data);
static pj_bool_t stun_auth_verify_nonce(void *user_data,
					const pj_str_t *realm,
					const pj_str_t *username,
					const pj_str_t *nonce);


PJ_DEF(pj_status_t) pj_ice_create(pj_stun_config *cfg,
				  const char *name,
				  pj_ice_role role,
				  const pj_ice_cb *cb,
				  int af,
				  int sock_type,
				  pj_ice **p_ice)
{
    pj_pool_t *pool;
    pj_ice *ice;
    pj_stun_session_cb sess_cb;
    pj_stun_auth_cred auth_cred;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && cb && p_ice, PJ_EINVAL);
    PJ_ASSERT_RETURN(sock_type==PJ_SOCK_DGRAM || sock_type==PJ_SOCK_STREAM,
		     PJ_EINVAL);

    if (!name)
	name = "ice%p";

    pool = pj_pool_create(cfg->pf, name, 4000, 4000, NULL);
    ice = PJ_POOL_ZALLOC_T(pool, pj_ice);
    ice->pool = pool;
    ice->af = af;
    ice->sock_type = sock_type;
    ice->role = role;

    pj_ansi_snprintf(ice->obj_name, sizeof(ice->obj_name),
		     name, ice);

    status = pj_mutex_create_recursive(pool, ice->obj_name, 
				       &ice->mutex);
    if (status != PJ_SUCCESS) {
	destroy_ice(ice, status);
	return status;
    }

    pj_memcpy(&ice->cb, cb, sizeof(*cb));

    /* Init STUN callbacks */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_request_complete = &on_stun_request_complete;
    sess_cb.on_rx_indication = &on_stun_rx_indication;
    sess_cb.on_rx_request = &on_stun_rx_request;
    sess_cb.on_send_msg = &on_stun_send_msg;

    /* Init STUN authentication credential */
    pj_bzero(&auth_cred, sizeof(auth_cred));
    auth_cred.type = PJ_STUN_AUTH_CRED_DYNAMIC;
    auth_cred.data.dyn_cred.get_auth = &stun_auth_get_auth;
    auth_cred.data.dyn_cred.get_password = &stun_auth_get_password;
    auth_cred.data.dyn_cred.verify_nonce = &stun_auth_verify_nonce;

    /* Create STUN session for outgoing requests */
    status = pj_stun_session_create(cfg, ice->obj_name, &sess_cb, PJ_FALSE,
				    &ice->tx_sess);
    if (status != PJ_SUCCESS) {
	destroy_ice(ice, status);
	return status;
    }

    pj_stun_session_set_user_data(ice->tx_sess, ice);
    auth_cred.data.dyn_cred.user_data = ice->tx_sess;
    pj_stun_session_set_credential(ice->tx_sess, &auth_cred);

    /* Create STUN session for incoming requests */
    status = pj_stun_session_create(cfg, ice->obj_name, &sess_cb, PJ_FALSE,
				    &ice->rx_sess);
    if (status != PJ_SUCCESS) {
	destroy_ice(ice, status);
	return status;
    }

    pj_stun_session_set_user_data(ice->rx_sess, ice);
    auth_cred.data.dyn_cred.user_data = ice->rx_sess;
    pj_stun_session_set_credential(ice->rx_sess, &auth_cred);

    /* Done */
    *p_ice = ice;

    PJ_LOG(4,(ice->obj_name, "ICE session created"));

    return PJ_SUCCESS;
}


static void destroy_ice(pj_ice *ice,
			pj_status_t reason)
{
    if (reason == PJ_SUCCESS) {
	PJ_LOG(4,(ice->obj_name, "Destroying ICE session"));
    }

    if (ice->resv_q) {
	pj_dns_resolver_cancel_query(ice->resv_q, PJ_FALSE);
	ice->resv_q = NULL;
    }

    if (ice->mutex) {
	pj_mutex_destroy(ice->mutex);
	ice->mutex = NULL;
    }

    if (ice->pool) {
	pj_pool_t *pool = ice->pool;
	ice->pool = NULL;
	pj_pool_release(pool);
    }
}


PJ_DEF(pj_status_t) pj_ice_destroy(pj_ice *ice)
{
    destroy_ice(ice, PJ_SUCCESS);
    return PJ_SUCCESS;
}


static void ice_set_state(pj_ice *ice,
			  pj_ice_state new_state)
{
    ice->state = new_state;
}

static void resolver_cb(void *user_data,
			pj_status_t status,
			pj_dns_parsed_packet *response)
{
    pj_assert(!"Not implemented yet!");
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(response);
}

PJ_DEF(pj_status_t) pj_ice_set_srv(pj_ice *ice,
				   pj_bool_t enable_relay,
				   pj_dns_resolver *resolver,
				   const pj_str_t *domain)
{
    char namebuf[128];
    char *tp_name;
    pj_str_t name;
    pj_status_t status;


    /* Not implemented yet! */
    return PJ_ENOTSUP;


    PJ_ASSERT_RETURN(ice && resolver && domain, PJ_EINVAL);

    /* Must not have a running resolver. This is because we couldn't
     * safely cancel the query (there is a race condition situation
     * between the callback acquiring the mutex and this function
     * acquiring the mutex)
     */
    PJ_ASSERT_RETURN(ice->resv_q==NULL, PJ_EBUSY);

    pj_mutex_lock(ice->mutex);

    /* Reset resolver and server addresses */
    ice->relay_enabled = enable_relay;
    ice->resv = resolver;
    pj_bzero(&ice->stun_srv, sizeof(ice->stun_srv));

    /* Build SRV record name */
    if (ice->sock_type == PJ_SOCK_DGRAM) {
	tp_name = "_udp";
    } else if (ice->sock_type == PJ_SOCK_STREAM) {
	tp_name = "_tcp";
    } else {
	pj_assert(!"Invalid sock_type");
	pj_mutex_unlock(ice->mutex);
	return PJ_EBUG;
    }

    if (enable_relay) {
	name.ptr = namebuf;
	name.slen = pj_ansi_snprintf(namebuf, sizeof(namebuf),
				     "_stun-relay.%s.%.*s",
				     tp_name,
				     (int)domain->slen,
				     domain->ptr);
    } else {
	name.ptr = namebuf;
	name.slen = pj_ansi_snprintf(namebuf, sizeof(namebuf),
				     "_stun.%s.%.*s",
				     tp_name,
				     (int)domain->slen,
				     domain->ptr);
    }

    if (name.slen < 1 || name.slen >= sizeof(namebuf)) {
	pj_mutex_unlock(ice->mutex);
	return PJ_ENAMETOOLONG;
    }

    /* Start DNS query */
    status = pj_dns_resolver_start_query(ice->resv, &name, 
					 PJ_DNS_TYPE_SRV, 0, 
					 &resolver_cb, 
					 ice, &ice->resv_q);
    if (status != PJ_SUCCESS) {
	pj_mutex_unlock(ice->mutex);
	return status;
    }

    pj_mutex_unlock(ice->mutex);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ice_set_srv_addr(pj_ice *ice,
					pj_bool_t enable_relay,
					const pj_sockaddr_t *srv_addr,
					unsigned addr_len)
{
    PJ_ASSERT_RETURN(ice && srv_addr, PJ_EINVAL);
    /* Must not have a running resolver. This is because we couldn't
     * safely cancel the query (there is a race condition situation
     * between the callback acquiring the mutex and this function
     * acquiring the mutex)
     */
    PJ_ASSERT_RETURN(ice->resv_q==NULL, PJ_EBUSY);

    pj_mutex_lock(ice->mutex);

    ice->relay_enabled = enable_relay;
    pj_memcpy(&ice->stun_srv, srv_addr, addr_len);

    pj_mutex_unlock(ice->mutex);

    return PJ_SUCCESS;

}


PJ_DEF(pj_status_t) pj_ice_add_comp(pj_ice *ice,
				    unsigned comp_id,
				    const pj_sockaddr_t *local_addr,
				    unsigned addr_len)
{
    pj_status_t status;
    pj_sock_t sock;

    PJ_ASSERT_RETURN(ice && local_addr && addr_len, PJ_EINVAL);

    status = pj_sock_socket(ice->af, ice->sock_type, 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_sock_bind(sock, local_addr, addr_len);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_ice_add_sock_comp(ice, comp_id, sock);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ice_add_sock_comp( pj_ice *ice,
					  unsigned comp_id,
					  pj_sock_t sock)
{
    PJ_ASSERT_RETURN(ice && sock != PJ_INVALID_SOCKET, PJ_EINVAL);
    PJ_ASSERT_RETURN(ice->comp_cnt < PJ_ARRAY_SIZE(ice->comp), PJ_ETOOMANY);

    pj_mutex_lock(ice->mutex);
    ice->comp[ice->comp_cnt].comp_id = comp_id;
    ice->comp[ice->comp_cnt].sock = sock;
    ++ice->comp_cnt;
    pj_mutex_unlock(ice->mutex);

    return PJ_SUCCESS;
}


static pj_status_t stun_auth_get_auth(void *user_data,
				      pj_pool_t *pool,
				      pj_str_t *realm,
				      pj_str_t *nonce)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(pool);

    realm->slen = 0;
    nonce->slen = 0;

    return PJ_SUCCESS;
}


static pj_status_t stun_auth_get_password(void *user_data, 
					  const pj_str_t *realm,
					  const pj_str_t *username,
					  pj_pool_t *pool,
					  int *data_type,
					  pj_str_t *data)
{
    pj_stun_session *sess = (pj_stun_session *)user_data;
    pj_ice *ice = (pj_ice*) pj_stun_session_get_user_data(sess);

    PJ_UNUSED_ARG(realm);
    PJ_UNUSED_ARG(pool);

    if (sess == ice->tx_sess) {
	/* Verify username */
	if (pj_strcmp(username, &ice->tx_uname) != 0)
	    return -1;
	*data_type = 0;
	*data = ice->tx_pass;
    } else {
	/* The agent MUST accept a credential if the username consists
	 * of two values separated by a colon, where the first value is
	 * equal to the username fragment generated by the agent in an offer
	 * or answer for a session in-progress, and the MESSAGE-INTEGRITY 
	 * is the output of a hash of the password and the STUN packet's 
	 * contents. 
	 */
	PJ_TODO(CHECK_USERNAME_FOR_INCOMING_STUN_REQUEST);
	*data_type = 0;
	*data = ice->rx_pass;
    }

    return PJ_SUCCESS;
}


static pj_bool_t stun_auth_verify_nonce(void *user_data,
					const pj_str_t *realm,
					const pj_str_t *username,
					const pj_str_t *nonce)
{
    /* We don't use NONCE */
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(realm);
    PJ_UNUSED_ARG(username);
    PJ_UNUSED_ARG(nonce);
    return PJ_TRUE;
}


PJ_DEF(pj_status_t) pj_ice_set_credentials(pj_ice *ice,
					   const pj_str_t *local_ufrag,
					   const pj_str_t *local_pass,
					   const pj_str_t *remote_ufrag,
					   const pj_str_t *remote_pass)
{
    char buf[128];
    pj_str_t username;

    username.ptr = buf;

    PJ_ASSERT_RETURN(ice && local_ufrag && local_pass &&
		     remote_ufrag && remote_pass, PJ_EINVAL);
    PJ_ASSERT_RETURN(local_ufrag->slen + remote_ufrag->slen <
		     sizeof(buf), PJ_ENAMETOOLONG);

    pj_strcpy(&username, remote_ufrag);
    pj_strcat2(&username, ":");
    pj_strcat(&username, local_ufrag);

    pj_strdup(ice->pool, &ice->tx_uname, &username);
    pj_strdup(ice->pool, &ice->tx_pass, remote_pass);

    pj_strcpy(&username, local_ufrag);
    pj_strcat2(&username, ":");
    pj_strcat(&username, remote_ufrag);

    pj_strdup(ice->pool, &ice->rx_uname, &username);
    pj_strdup(ice->pool, &ice->rx_pass, local_pass);

    return PJ_SUCCESS;
}


static pj_status_t gather_host_cands(pj_ice *ice)
{
    unsigned i;
    pj_status_t status;

    for (i=0; i<ice->comp_cnt; ++i) {
	pj_ice_comp *comp = &ice->comp[i];
	pj_sockaddr addr;
	int addr_len;

	addr_len = sizeof(addr);
	status = pj_sock_getsockname(comp->sock, &addr, &addr_len);
	if (status != PJ_SUCCESS)
	    return status;

	if (addr.ipv4.sin_addr.s_addr == 0) {
	    status = pj_gethostip(&addr.ipv4.sin_addr);
	    if (status != PJ_SUCCESS)
		return status;
	}

	status = pj_ice_add_cand(ice, i, PJ_ICE_CAND_TYPE_HOST, 65535,
				 NULL, &addr, &addr, NULL,
    				 sizeof(pj_sockaddr_in), NULL);
	if (status != PJ_SUCCESS)
	    return status;
    }

    return PJ_SUCCESS;
}

static pj_status_t gather_mapped_cands(pj_ice *ice)
{
    PJ_UNUSED_ARG(ice);
    return PJ_ENOTSUP;
}

static pj_status_t gather_relayed_cands(pj_ice *ice)
{
    PJ_UNUSED_ARG(ice);
    return PJ_ENOTSUP;
}

PJ_DEF(pj_status_t) pj_ice_start_gather(pj_ice *ice,
					unsigned flags)
{
    pj_status_t status;

    PJ_UNUSED_ARG(flags);

    /* Gather host candidate */
    status = gather_host_cands(ice);
    if (status != PJ_SUCCESS)
	return status;

    PJ_TODO(GATHER_MAPPED_AND_RELAYED_CANDIDATES);

    ice_set_state(ice, PJ_ICE_STATE_CAND_COMPLETE);

    return PJ_SUCCESS;
}


static pj_uint32_t CALC_CAND_PRIO(pj_ice_cand_type type,
				  pj_uint32_t local_pref,
				  pj_uint32_t comp_id)
{
    static pj_uint32_t type_pref[] =
    {
	PJ_ICE_HOST_PREF,
	PJ_ICE_MAPPED_PREF,
	PJ_ICE_PEER_MAPPED_PREF,
	PJ_ICE_RELAYED_PREF
    };

    return ((1 << 24) * type_pref[type]) + 
	   ((1 << 8) * local_pref) +
	   (256 - comp_id);
}


PJ_DEF(pj_status_t) pj_ice_add_cand(pj_ice *ice,
				    unsigned comp_id,
				    pj_ice_cand_type type,
				    pj_uint16_t local_pref,
				    const pj_str_t *foundation,
				    const pj_sockaddr_t *addr,
				    const pj_sockaddr_t *base_addr,
				    const pj_sockaddr_t *srv_addr,
				    int addr_len,
				    unsigned *p_cand_id)
{
    pj_ice_cand *lcand;
    pj_status_t status = PJ_SUCCESS;

    pj_mutex_lock(ice->mutex);

    if (ice->lcand_cnt >= PJ_ARRAY_SIZE(ice->lcand)) {
	status = PJ_ETOOMANY;
	goto on_error;
    }

    lcand = &ice->lcand[ice->lcand_cnt];
    lcand->comp_id = comp_id;
    lcand->type = type;
    pj_strdup(ice->pool, &lcand->foundation, foundation);
    lcand->prio = CALC_CAND_PRIO(type, local_pref, lcand->comp_id);
    pj_memcpy(&lcand->addr, addr, addr_len);
    pj_memcpy(&lcand->base_addr, base_addr, addr_len);
    if (srv_addr)
	pj_memcpy(&lcand->srv_addr, srv_addr, addr_len);
    else
	pj_bzero(&lcand->srv_addr, sizeof(lcand->srv_addr));

    if (p_cand_id)
	*p_cand_id = ice->lcand_cnt;

    ++ice->lcand_cnt;

on_error:
    pj_mutex_unlock(ice->mutex);
    return status;
}


PJ_DEF(unsigned) pj_ice_get_cand_cnt(pj_ice *ice)
{
    return ice->lcand_cnt;
}


PJ_DEF(pj_status_t) pj_ice_enum_cands(pj_ice *ice,
				      unsigned sort_by,
				      unsigned *p_count,
				      unsigned cand_ids[])
{
    unsigned i, count;

    PJ_UNUSED_ARG(sort_by);

    pj_mutex_lock(ice->mutex);

    count = (*p_count < ice->lcand_cnt) ? *p_count : ice->lcand_cnt;
    for (i=0; i<count; ++i)
	cand_ids[i] = i;

    *p_count = count;
    pj_mutex_unlock(ice->mutex);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ice_get_cand(pj_ice *ice,
				    unsigned cand_id,
				    pj_ice_cand **p_cand)
{
    PJ_ASSERT_RETURN(ice && p_cand, PJ_EINVAL);
    PJ_ASSERT_RETURN(cand_id <= ice->lcand_cnt, PJ_EINVAL);

    *p_cand = &ice->lcand[cand_id];

    return PJ_SUCCESS;
}

#ifndef MIN
#   define MIN(a,b) (a < b ? a : b)
#endif

#ifndef MAX
#   define MAX(a,b) (a > b ? a : b)
#endif

static pj_uint64_t CALC_CHECK_PRIO(pj_uint32_t O, pj_uint32_t A)
{
    return ((pj_uint64_t)1 << 32) * MIN(O, A) +
	   (pj_uint64_t)2 * MAX(O, A) + (O>A ? 1 : 0);
}

static const char *dump_check(char *buffer, unsigned bufsize,
			      const pj_ice *ice,
			      const pj_ice_check *check)
{
    char local_addr[128];
    int len;

    pj_ansi_strcpy(local_addr, 
		   pj_inet_ntoa(ice->lcand[check->cand_id].addr.ipv4.sin_addr));

    len = pj_ansi_snprintf(buffer, bufsize,
	      "%s:%d-->%s:%d",
	      local_addr,
	      (int)pj_ntohs(ice->lcand[check->cand_id].addr.ipv4.sin_port),
	      pj_inet_ntoa(check->rem_addr.ipv4.sin_addr),
	      (int)pj_ntohs(check->rem_addr.ipv4.sin_port));
    if (len < 0)
	len = 0;
    else if (len >= (int)bufsize)
	len = bufsize - 1;

    buffer[len] = '\0';
    return buffer;
}

#if PJ_LOG_MAX_LEVEL >= 4
static void dump_checklist(const char *title, const pj_ice *ice, 
			   const pj_ice_checklist *clist)
{
    unsigned i;
    char buffer[128];

    PJ_LOG(4,(ice->obj_name, "%s", title));
    for (i=0; i<clist->count; ++i) {
	const pj_ice_check *c = &clist->checks[i];
	PJ_LOG(4,(ice->obj_name, " %d: %s (prio=%u, state=%s)",
		  i, dump_check(buffer, sizeof(buffer), ice, c),
		  c->check_prio, check_state_name[c->check_state]));
    }
}
#else
#define dump_checklist(ice, clist)
#endif

static void sort_checklist(pj_ice_checklist *clist)
{
    unsigned i;

    for (i=0; i<clist->count-1; ++i) {
	unsigned j, highest = i;
	for (j=i+1; j<clist->count; ++j) {
	    if (clist->checks[j].check_prio > 
		clist->checks[highest].check_prio) 
	    {
		highest = j;
	    }
	}

	if (highest != i) {
	    pj_ice_check tmp;

	    pj_memcpy(&tmp, &clist->checks[i], sizeof(pj_ice_check));
	    pj_memcpy(&clist->checks[i], &clist->checks[highest], 
		      sizeof(pj_ice_check));
	    pj_memcpy(&clist->checks[highest], &tmp, sizeof(pj_ice_check));
	}
    }
}

PJ_DEF(pj_status_t) pj_ice_create_check_list(pj_ice *ice,
					     pj_bool_t is_remote_offer,
					     unsigned rem_cand_cnt,
					     const pj_ice_cand rem_cand[])
{
    pj_ice_checklist *clist;
    unsigned i, j;

    PJ_ASSERT_RETURN(ice && rem_cand_cnt && rem_cand, PJ_EINVAL);
    PJ_ASSERT_RETURN(rem_cand_cnt < PJ_ICE_MAX_CAND, PJ_ETOOMANY);

    pj_mutex_lock(ice->mutex);

    /* Save remote candidates */
    ice->rcand_cnt = 0;
    for (i=0; i<rem_cand_cnt; ++i) {
	pj_ice_cand *cn = &ice->rcand[ice->rcand_cnt++];
	pj_memcpy(cn, &rem_cand[i], sizeof(pj_ice_cand));
	pj_strdup(ice->pool, &cn->foundation, &rem_cand[i].foundation);
    }

    /* Generate checklist */
    clist = &ice->cklist;
    for (i=0; i<ice->lcand_cnt; ++i) {
	for (j=0; j<rem_cand_cnt; ++j) {

	    pj_ice_check *c = &clist->checks[clist->count++];

	    /* A local candidate is paired with a remote candidate if
	     * and only if the two candidates have the same component ID 
	     * and have the same IP address version. 
	     */
	    if (ice->lcand[i].comp_id != rem_cand[j].comp_id ||
		pj_strcmp(&ice->lcand[i].foundation,&rem_cand[j].foundation)==0)
	    {
		continue;
	    }

	    c->cand_id = i;
	    c->comp_id = ice->lcand[i].comp_id;
	    c->foundation = ice->lcand[i].foundation;

	    if (is_remote_offer) {
		c->check_prio = CALC_CHECK_PRIO(rem_cand[j].prio,
						ice->lcand[i].prio);
	    } else {
		c->check_prio = CALC_CHECK_PRIO(ice->lcand[i].prio, 
						rem_cand[j].prio);
	    }

	    c->rem_type = rem_cand[j].type;
	    pj_strdup(ice->pool, &c->rem_foundation, &rem_cand[j].foundation);
	    c->rem_prio = rem_cand[j].prio;
	    pj_memcpy(&c->rem_addr, &rem_cand[j].addr, 
		      sizeof(rem_cand[j].addr));
	    pj_memcpy(&c->rem_base_addr, &rem_cand[j].base_addr, 
		      sizeof(rem_cand[j].addr));
	}
    }

    /* Sort checklist based on priority */
    sort_checklist(clist);

    /* Prune the checklist */
    for (i=0; i<clist->count; ++i) {
	PJ_TODO(PRUNE_CHECKLIST);
    }

    /* Log checklist */
    dump_checklist("Checklist created:", ice, clist);

    pj_mutex_lock(ice->mutex);

    return PJ_SUCCESS;
}


struct req_data
{
    pj_ice		*ice;
    pj_ice_checklist	*clist;
    unsigned		 ckid;
};

/* Perform check on the specified candidate pair */
static pj_status_t perform_check(pj_ice *ice, pj_ice_checklist *clist,
				 unsigned check_id)
{
    pj_stun_tx_data *tdata;
    struct req_data *rd;
    pj_ice_check *check;
    pj_uint32_t prio;
    char buffer[128];
    pj_status_t status;

    check = &clist->checks[check_id];

    PJ_LOG(5,(ice->obj_name, 
	      "Sending connectivity check for check %d: %s", 
	      check_id, dump_check(buffer, sizeof(buffer), ice, check)));

    /* Create request */
    status = pj_stun_session_create_req(ice->tx_sess, 
					PJ_STUN_BINDING_REQUEST, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Attach data to be retrieved later when STUN request transaction
     * completes and on_stun_request_complete() callback is called.
     */
    rd = PJ_POOL_ZALLOC_T(tdata->pool, struct req_data);
    rd->ice = ice;
    rd->clist = clist;
    rd->ckid = check_id;
    tdata->user_data = (void*) rd;

    /* Add PRIORITY */
    prio = CALC_CAND_PRIO(check->rem_type, 65535, check->comp_id);
    pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg, PJ_STUN_ATTR_PRIORITY,
			      prio);

    /* Add USE-CANDIDATE */
    if (ice->role == PJ_ICE_ROLE_CONTROLLING) {
	pj_stun_msg_add_empty_attr(tdata->pool, tdata->msg, 
				   PJ_STUN_ATTR_USE_CANDIDATE);
    }

    /* Initiate STUN transaction to send the request */
    status = pj_stun_session_send_msg(ice->tx_sess, PJ_FALSE, 
				      &check->rem_addr, 
				      sizeof(pj_sockaddr_in), tdata);
    if (status != PJ_SUCCESS)
	return status;

    check->check_state = PJ_ICE_CHECK_STATE_IN_PROGRESS;
    return PJ_SUCCESS;
}

/* Start periodic check for the specified checklist */
static pj_status_t start_periodic_check(pj_ice *ice, pj_ice_checklist *clist)
{
    unsigned i, start_count=0;
    pj_status_t status;

    /* Checklist state must be idle or completed */
    pj_assert(clist->state == PJ_ICE_CHECKLIST_ST_IDLE ||
	      clist->state == PJ_ICE_CHECKLIST_ST_COMPLETED);

    /* Set checklist state to Running */
    clist->state = PJ_ICE_CHECKLIST_ST_RUNNING;

    PJ_LOG(4,(ice->obj_name, "Starting checklist periodic check"));

    /* Send STUN Binding request for checks in Waiting list */
    for (i=0; i<clist->count; ++i) {
	pj_ice_check *check = &clist->checks[i];

	if (check->check_state == PJ_ICE_CHECK_STATE_WAITING) {
	    status = perform_check(ice, clist, i);
	    if (status != PJ_SUCCESS)
		return status;

	    ++start_count;
	}
    }

    /* If we don't have anything in Waiting state, perform check to
     * highest priority pair that is in Frozen state.
     */
    if (start_count==0) {
	for (i=0; i<clist->count; ++i) {
	    pj_ice_check *check = &clist->checks[i];

	    if (check->check_state == PJ_ICE_CHECK_STATE_FROZEN) {
		status = perform_check(ice, clist, i);
		if (status != PJ_SUCCESS)
		    return status;

		++start_count;
		break;
	    }
	}
    }

    /* Cannot start check because there's no suitable candidate pair.
     * Set checklist state to Completed.
     */
    if (start_count==0) {
	clist->state = PJ_ICE_CHECKLIST_ST_COMPLETED;
	PJ_LOG(4,(ice->obj_name, "Checklist completed"));
    }

    return PJ_SUCCESS;
}


/* Start ICE check */
PJ_DEF(pj_status_t) pj_ice_start_check(pj_ice *ice)
{
    pj_ice_checklist *clist;
    unsigned i;

    PJ_ASSERT_RETURN(ice, PJ_EINVAL);

    clist = &ice->cklist;

    if (clist->count == 0)
	return PJ_EICENOCHECKLIST;

    /* Pickup the first pair and set the state to Waiting */
    clist->checks[0].check_state = PJ_ICE_CHECK_STATE_WAITING;

    /* Find all of the other pairs in that check list with the same
     * component ID, but different foundations, and sets all of their
     * states to Waiting as well.
     */
    for (i=1; i<clist->count; ++i) {
	pj_ice_check *cki = &clist->checks[i];

	if (cki->comp_id != clist->checks[0].comp_id)
	    continue;

	if (pj_strcmp(&cki->foundation, &clist->checks[0].foundation)==0)
	    continue;

	clist->checks[i].check_state = PJ_ICE_CHECK_STATE_WAITING;
    }

    /* Start periodic check */
    return start_periodic_check(ice, clist);
}


//////////////////////////////////////////////////////////////////////////////

static pj_status_t on_stun_send_msg(pj_stun_session *sess,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len)
{
    pj_ice *ice = (pj_ice*) pj_stun_session_get_user_data(sess);
    return (*ice->cb.on_send_pkt)(ice, pkt, pkt_size, dst_addr, addr_len);
}


static pj_status_t on_stun_rx_request(pj_stun_session *sess,
				      const pj_uint8_t *pkt,
				      unsigned pkt_len,
				      const pj_stun_msg *msg,
				      const pj_sockaddr_t *src_addr,
				      unsigned src_addr_len)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* 7.2.1.2.  Learning Peer Reflexive Candidates */
    PJ_TODO(LEARN_PEER_REFLEXIVE_CANDIDATES);

    /* Reject any requests except Binding request */
    if (msg->hdr.type != PJ_STUN_BINDING_REQUEST) {
	status = pj_stun_session_create_response(sess, msg, 
						 PJ_STUN_SC_BAD_REQUEST,
						 NULL, &tdata);
	if (status != PJ_SUCCESS)
	    return status;

	status = pj_stun_session_send_msg(sess, PJ_TRUE, 
					  src_addr, src_addr_len, tdata);
	return status;
    }

    status = pj_stun_session_create_response(sess, msg, 0, NULL, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_stun_msg_add_sockaddr_attr(tdata->pool, msg, 
					   PJ_STUN_ATTR_XOR_MAPPED_ADDR,
					   PJ_TRUE, src_addr, src_addr_len);

    status = pj_stun_session_send_msg(sess, PJ_TRUE, 
				      src_addr, src_addr_len, tdata);

    /* 7.2.1.3.  Triggered Checks:
     * Next, the agent constructs a pair whose local candidate is equal to
     * the transport address on which the STUN request was received, and a
     * remote candidate equal to the source transport address where the
     * request came from (which may be peer-reflexive remote candidate that
     * was just learned). 
     */
    
    return status;
}

/* This callback is called when outgoing STUN request completed */
static void on_stun_request_complete(pj_stun_session *sess,
				     pj_status_t status,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response)
{
    struct req_data *rd = (struct req_data*) tdata->user_data;
    pj_ice *ice;
    pj_ice_check *check, *valid_check;
    pj_ice_checklist *clist;
    char buffer[128];

    ice = rd->ice;
    check = &rd->clist->checks[rd->ckid];
    clist = rd->clist;

    PJ_LOG(5,(ice->obj_name, 
	      "Connectivity check %s for check %d: %s",
	      (status==PJ_SUCCESS ? "SUCCESS" : "FAILED"), rd->ckid, 
	      dump_check(buffer, sizeof(buffer), ice, check)));

    if (status != PJ_SUCCESS) {
	check->check_state = PJ_ICE_CHECK_STATE_FAILED;
	return;
    }

    /* The agent MUST check that the source IP address and port of the
     * response equals the destination IP address and port that the Binding
     * Request was sent to, and that the destination IP address and port of
     * the response match the source IP address and port that the Binding
     * Request was sent from.
     */
    PJ_TODO(CHECK_ICE_RESPONSE_SOURCE_ADDRESS);

    /* Get the STUN MAPPED-ADDRESS attribute. If the
     * transport address does not match any of the local candidates that the
     * agent knows about, the mapped address represents a new candidate - a
     * peer reflexive candidate 
     */
    PJ_TODO(CHECK_ICE_RESPONSE_SOURCE_ADDRESS2);

    /* Sets the state of the pair that generated the check to succeeded. */
    check->check_state = PJ_ICE_CHECK_STATE_SUCCEEDED;

    /* This is a valid pair, so add this to the valid list */
    valid_check = &ice->valid_list.checks[ice->valid_list.count++];
    pj_memcpy(valid_check, check, sizeof(*check));

    /* Sort valid_list */
    sort_checklist(&ice->valid_list);


    /* If the pair had a component ID of 1, the agent MUST change the
     * states for all other Frozen pairs for the same media stream and
     * same foundation, but different component IDs, to Waiting.
     */
    if (check->comp_id == 1) {
	unsigned i;
	for (i=0; i<clist->count; ++i)  {
	    pj_ice_check *c = &clist->checks[i];

	    if (c->check_state == PJ_ICE_CHECK_STATE_FROZEN &&
		c->comp_id != check->comp_id &&
		pj_strcmp(&c->foundation, &check->foundation)==0)
	    {
		/* Unfreeze and start check */
		PJ_LOG(5,(ice->obj_name, "Unfreezing check %d", i));
		c->check_state = PJ_ICE_CHECK_STATE_WAITING;
		perform_check(ice, clist, i);
	    }
	}

    } 
    /* If the pair had a component ID equal to the number of components
     * for the media stream (where this is the actual number of
     * components being used, in cases where the number of components
     * signaled in the SDP differs from offerer to answerer), the agent
     * MUST change the state for all other Frozen pairs for the first
     * component of different media streams (and thus in different check
     * lists) but the same foundation, to Waiting.
     */
    else if (0) {
	PJ_TODO(UNFREEZE_OTHER_COMPONENT_ID);
    }
    /* If the pair has any other component ID, no other pairs can be
     * unfrozen.
     */
    else {
	PJ_TODO(UNFREEZE_OTHER_COMPONENT_ID1);
    }

}

static pj_status_t on_stun_rx_indication(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    PJ_TODO(SUPPORT_RX_BIND_REQUEST_AS_INDICATION);
    return PJ_ENOTSUP;
}


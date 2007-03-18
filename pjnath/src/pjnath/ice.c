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
#include <pj/assert.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


static void destroy_ice(pj_ice *ice,
			pj_status_t reason);
static void ice_set_state(pj_ice *ice,
			  pj_ice_state new_state);


PJ_DEF(pj_status_t) pj_ice_create(pj_stun_config *cfg,
				  const char *name,
				  int af,
				  int sock_type,
				  pj_ice **p_ice)
{
    pj_pool_t *pool;
    pj_ice *ice;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && p_ice, PJ_EINVAL);
    PJ_ASSERT_RETURN(sock_type==PJ_SOCK_DGRAM || sock_type==PJ_SOCK_STREAM,
		     PJ_EINVAL);

    if (!name)
	name = "ice%p";

    pool = pj_pool_create(cfg->pf, name, 4000, 4000, NULL);
    ice = PJ_POOL_ZALLOC_T(pool, pj_ice);
    ice->pool = pool;
    ice->af = af;
    ice->sock_type = sock_type;

    pj_ansi_snprintf(ice->obj_name, sizeof(ice->obj_name),
		     name, ice);

    status = pj_mutex_create_recursive(pool, ice->obj_name, 
				       &ice->mutex);
    if (status != PJ_SUCCESS) {
	destroy_ice(ice, status);
	return status;
    }

    *p_ice = ice;

    return PJ_SUCCESS;
}


static void destroy_ice(pj_ice *ice,
			pj_status_t reason)
{
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
    return PJ_ENOTSUP;
}

static pj_status_t gather_relayed_cands(pj_ice *ice)
{
    return PJ_ENOTSUP;
}

PJ_DEF(pj_status_t) pj_ice_start_gather(pj_ice *ice,
					unsigned flags)
{
    pj_status_t status;

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
    pj_ice_cand *cand;
    pj_status_t status = PJ_SUCCESS;

    pj_mutex_lock(ice->mutex);

    if (ice->cand_cnt >= PJ_ARRAY_SIZE(ice->cand)) {
	status = PJ_ETOOMANY;
	goto on_error;
    }

    cand = &ice->cand[ice->cand_cnt];
    cand->comp_id = comp_id;
    cand->type = type;
    pj_strdup(ice->pool, &cand->foundation, foundation);
    cand->prio = CALC_CAND_PRIO(type, local_pref, cand->comp_id);
    pj_memcpy(&cand->addr, addr, addr_len);
    pj_memcpy(&cand->base_addr, base_addr, addr_len);
    if (srv_addr)
	pj_memcpy(&cand->srv_addr, srv_addr, addr_len);
    else
	pj_bzero(&cand->srv_addr, sizeof(cand->srv_addr));

    if (p_cand_id)
	*p_cand_id = ice->cand_cnt;

    ++ice->cand_cnt;

on_error:
    pj_mutex_unlock(ice->mutex);
    return status;
}


PJ_DEF(unsigned) pj_ice_get_cand_cnt(pj_ice *ice)
{
    return ice->cand_cnt;
}


PJ_DEF(pj_status_t) pj_ice_enum_cands(pj_ice *ice,
				      unsigned sort_by,
				      unsigned *p_count,
				      unsigned cand_ids[])
{
    unsigned i, count;

    pj_mutex_lock(ice->mutex);

    count = (*p_count < ice->cand_cnt) ? *p_count : ice->cand_cnt;
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
    PJ_ASSERT_RETURN(cand_id <= ice->cand_cnt, PJ_EINVAL);

    *p_cand = &ice->cand[cand_id];

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


PJ_DEF(pj_status_t) pj_ice_create_check_list(pj_ice *ice,
					     pj_bool_t is_remote_offer,
					     unsigned rem_cand_cnt,
					     const pj_ice_cand rem_cand[])
{
    pj_ice_checklist *clist;
    unsigned i, j, count;

    PJ_ASSERT_RETURN(ice && rem_cand_cnt && rem_cand, PJ_EINVAL);

    pj_mutex_lock(ice->mutex);

    /* Create checklist */
    clist = &ice->cklist;
    clist->checks = pj_pool_calloc(ice->pool, 
				   ice->cand_cnt * rem_cand_cnt,
				   sizeof(pj_ice_check));
    for (i=0, count=0; i<ice->cand_cnt; ++i) {
	for (j=0; j<rem_cand_cnt; ++j) {

	    pj_ice_check *c = &clist->checks[count++];

	    /* A local candidate is paired with a remote candidate if
	     * and only if the two candidates have the same component ID 
	     * and have the same IP address version. 
	     */
	    if (ice->cand[i].comp_id != rem_cand[j].comp_id ||
		pj_strcmp(&ice->cand[i].foundation,&rem_cand[j].foundation)==0)
	    {
		continue;
	    }

	    c->local_cand_id = i;

	    if (is_remote_offer) {
		c->check_prio = CALC_CHECK_PRIO(rem_cand[j].prio,
						ice->cand[i].prio);
	    } else {
		c->check_prio = CALC_CHECK_PRIO(ice->cand[i].prio, 
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

    clist->count = count;

    /* Sort checklist based on priority */
    for (i=0; i<clist->count-1; ++i) {
	unsigned highest = i;
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

    /* Prune the checklist */
    for (i=0; i<clist->count; ++i) {
	PJ_TODO(PRUNE_CHECKLIST);
    }

    pj_mutex_lock(ice->mutex);

    return PJ_SUCCESS;
}


/* Start periodic check for the specified checklist */
static pj_status_t start_periodic_check(pj_ice *ice, pj_ice_checklist *clist)
{
}


/* Start ICE check */
PJ_DEF(pj_status_t) pj_ice_start_check(pj_ice *ice)
{
    pj_ice_checklist *clist;
    unsigned i, comp_id;
    pj_str_t fnd;

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
    comp_id = ice->cand[clist->checks[0].local_cand_id].comp_id;
    fnd = ice->cand[clist->checks[0].local_cand_id].foundation;

    for (i=1; i<clist->count; ++i) {
	pj_ice_check *cki = &clist->checks[i];

	if (ice->cand[cki->local_cand_id].comp_id != comp_id)
	    continue;

	if (pj_strcmp(&ice->cand[cki->local_cand_id].foundation, &fnd)==0)
	    continue;

	clist->checks[i].check_state = PJ_ICE_CHECK_STATE_WAITING;
    }

    /* Start periodic check */
    return start_periodic_check(ice, clist);
}

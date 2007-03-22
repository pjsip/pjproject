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
#include <pjnath/ice_stream_transport.h>
#include <pjnath/errno.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>



/* ICE callbacks */
static void	   on_ice_complete(pj_ice *ice, pj_status_t status);
static pj_status_t on_tx_pkt(pj_ice *ice, 
			     unsigned comp_id, unsigned cand_id,
			     const void *pkt, pj_size_t size,
			     const pj_sockaddr_t *dst_addr,
			     unsigned dst_addr_len);
static void	   on_rx_data(pj_ice *ice, 
			      unsigned comp_id, unsigned cand_id,
			      void *pkt, pj_size_t size,
			      const pj_sockaddr_t *src_addr,
			      unsigned src_addr_len);

/* Ioqueue callback */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);

static void destroy_ice_interface(pj_ice_st_interface *is);
static void destroy_ice_st(pj_ice_st *ice_st, pj_status_t reason);

static void ice_st_perror(pj_ice_st *ice_st, const char *title, 
			  pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(ice_st->obj_name, "%s: %s", title, errmsg));
}


/* Get the prefix for the foundation */
static int get_type_prefix(pj_ice_cand_type type)
{
    switch (type) {
    case PJ_ICE_CAND_TYPE_HOST:	    return 'H';
    case PJ_ICE_CAND_TYPE_SRFLX:    return 'S';
    case PJ_ICE_CAND_TYPE_PRFLX:    return 'P';
    case PJ_ICE_CAND_TYPE_RELAYED:  return 'R';
    default:
	pj_assert(!"Invalid type");
	return 'U';
    }
}


/* 
 * Create new interface (i.e. socket) 
 */
static pj_status_t create_ice_interface(pj_ice_st *ice_st,
					pj_ice_cand_type type,
					unsigned comp_id,
					pj_uint16_t local_pref,
					const pj_sockaddr_in *addr,
					pj_ice_st_interface **p_is)
{
    pj_ioqueue_callback ioqueue_cb;
    pj_ice_st_interface *is;
    char foundation[32];
    int addr_len;
    pj_status_t status;

    is = PJ_POOL_ZALLOC_T(ice_st->pool, pj_ice_st_interface);
    is->type = type;
    is->comp_id = comp_id;
    is->cand_id = -1;
    is->sock = PJ_INVALID_SOCKET;
    is->ice_st = ice_st;
    is->local_pref = local_pref;

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &is->sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Bind and get the local IP address */
    if (addr) 
	pj_memcpy(&is->base_addr, addr, sizeof(pj_sockaddr_in));
    else 
	pj_sockaddr_in_init(&is->base_addr.ipv4, NULL, 0);

    status = pj_sock_bind(is->sock, &is->base_addr, sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    addr_len = sizeof(is->base_addr);
    status = pj_sock_getsockname(is->sock, &is->base_addr, &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    if (is->base_addr.ipv4.sin_addr.s_addr == 0) {
	status = pj_gethostip(&is->base_addr.ipv4.sin_addr);
	if (status != PJ_SUCCESS) 
	    goto on_error;
    }

    /* Assign foundation */
    pj_ansi_snprintf(foundation, sizeof(foundation), "%c%x", 
		     get_type_prefix(type),
		     (int)pj_ntohl(is->base_addr.ipv4.sin_addr.s_addr));
    pj_strdup2(ice_st->pool, &is->foundation, foundation);


    /* Register to ioqueue */
    pj_bzero(&ioqueue_cb, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = &on_read_complete;
    status = pj_ioqueue_register_sock(ice_st->pool, ice_st->stun_cfg.ioqueue, 
				      is->sock, is, &ioqueue_cb, &is->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_ioqueue_op_key_init(&is->read_op, sizeof(is->read_op));
    pj_ioqueue_op_key_init(&is->write_op, sizeof(is->write_op));

    /* Kick start reading the socket */
    on_read_complete(is->key, &is->read_op, 0);

    /* Done */
    *p_is = is;
    return PJ_SUCCESS;

on_error:
    destroy_ice_interface(is);
    return status;
}


/* 
 * This is callback called by ioqueue on incoming packet 
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    pj_ice_st_interface *is = (pj_ice_st_interface*) 
			      pj_ioqueue_get_user_data(key);
    pj_ssize_t pkt_size;
    pj_status_t status;

    if (bytes_read > 0) {

	/* If we have an active ICE session, hand over all incoming
	 * packets to the ICE session. Otherwise just drop the packet.
	 */
	if (is->ice_st->ice) {
	    status = pj_ice_on_rx_pkt(is->ice_st->ice, 
				      is->comp_id, is->cand_id,
				      is->pkt, bytes_read,
				      &is->src_addr, is->src_addr_len);
	}

    } else if (bytes_read < 0) {
	ice_st_perror(is->ice_st, "ioqueue read callback error", -bytes_read);
    }

    /* Read next packet */
    pkt_size = sizeof(is->pkt);
    is->src_addr_len = sizeof(is->src_addr);
    status = pj_ioqueue_recvfrom(key, op_key, is->pkt, &pkt_size, 
				 PJ_IOQUEUE_ALWAYS_ASYNC,
				 &is->src_addr, &is->src_addr_len);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	ice_st_perror(is->ice_st, "ioqueue recvfrom() error", status);
    }
}


/* 
 * Destroy an interface 
 */
static void destroy_ice_interface(pj_ice_st_interface *is)
{
    if (is->key) {
	pj_ioqueue_unregister(is->key);
	is->key = NULL;
	is->sock = PJ_INVALID_SOCKET;
    } else if (is->sock != PJ_INVALID_SOCKET && is->sock != 0) {
	pj_sock_close(is->sock);
	is->sock = PJ_INVALID_SOCKET;
    }
}


/* 
 * Create ICE stream transport 
 */
PJ_DECL(pj_status_t) pj_ice_st_create(pj_stun_config *stun_cfg,
				      const char *name,
				      void *user_data,
				      const pj_ice_st_cb *cb,
				      pj_ice_st **p_ice_st)
{
    pj_pool_t *pool;
    pj_ice_st *ice_st;

    PJ_ASSERT_RETURN(stun_cfg && cb && p_ice_st, PJ_EINVAL);
    PJ_ASSERT_RETURN(stun_cfg->ioqueue && stun_cfg->timer_heap, PJ_EINVAL);

    if (name == NULL)
	name = "icest%p";

    pool = pj_pool_create(stun_cfg->pf, name, 4000, 4000, NULL);
    ice_st = PJ_POOL_ZALLOC_T(pool, pj_ice_st);
    ice_st->pool = pool;
    pj_memcpy(ice_st->obj_name, pool->obj_name, PJ_MAX_OBJ_NAME);
    ice_st->user_data = user_data;
    
    pj_memcpy(&ice_st->cb, cb, sizeof(*cb));
    pj_memcpy(&ice_st->stun_cfg, stun_cfg, sizeof(*stun_cfg));

    PJ_LOG(4,(ice_st->obj_name, "ICE stream transport created"));

    *p_ice_st = ice_st;
    return PJ_SUCCESS;
}


static void destroy_ice_st(pj_ice_st *ice_st, pj_status_t reason)
{
    unsigned i;
    char obj_name[PJ_MAX_OBJ_NAME];

    if (reason == PJ_SUCCESS) {
	pj_memcpy(obj_name, ice_st->obj_name, PJ_MAX_OBJ_NAME);
	PJ_LOG(4,(obj_name, "ICE stream transport shutting down"));
    }

    /* Destroy ICE if we have ICE */
    if (ice_st->ice) {
	pj_ice_destroy(ice_st->ice);
	ice_st->ice = NULL;
    }

    /* Destroy all interfaces */
    for (i=0; i<ice_st->itf_cnt; ++i) {
	destroy_ice_interface(ice_st->itfs[i]);
	ice_st->itfs[i] = NULL;
    }
    ice_st->itf_cnt = 0;

    /* Done */
    pj_pool_release(ice_st->pool);

    if (reason == PJ_SUCCESS) {
	PJ_LOG(4,(obj_name, "ICE stream transport destroyed"));
    }
}


/*
 * Destroy ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_st_destroy(pj_ice_st *ice_st)
{
    destroy_ice_st(ice_st, PJ_SUCCESS);
    return PJ_SUCCESS;
}


/*
 * Resolve STUN server
 */
PJ_DEF(pj_status_t) pj_ice_st_set_stun( pj_ice_st *ice_st,
				        pj_dns_resolver *resolver,
					pj_bool_t enable_relay,
					const pj_str_t *domain)
{
    /* Yeah, TODO */
    PJ_UNUSED_ARG(ice_st);
    PJ_UNUSED_ARG(resolver);
    PJ_UNUSED_ARG(enable_relay);
    PJ_UNUSED_ARG(domain);
    return -1;
}


/*
 * Set STUN server address.
 */
PJ_DEF(pj_status_t) pj_ice_st_set_stun_addr( pj_ice_st *ice_st,
					     pj_bool_t enable_relay,
					     const pj_sockaddr_in *srv_addr)
{

    PJ_ASSERT_RETURN(ice_st && srv_addr, PJ_EINVAL);
    
    ice_st->relay_enabled = enable_relay;
    pj_strdup2(ice_st->pool, &ice_st->stun_domain,
	       pj_inet_ntoa(srv_addr->sin_addr));
    pj_memcpy(&ice_st->stun_srv, srv_addr, sizeof(pj_sockaddr_in));

    return PJ_SUCCESS;
}


/*
 * Add new component.
 */
PJ_DEF(pj_status_t) pj_ice_st_add_comp(pj_ice_st *ice_st,
				       unsigned comp_id)
{
    /* Verify arguments */
    PJ_ASSERT_RETURN(ice_st && comp_id, PJ_EINVAL);

    /* Can only add component when we don't have active ICE session */
    PJ_ASSERT_RETURN(ice_st->ice == NULL, PJ_EBUSY);

    /* Check that we don't have too many components */
    PJ_ASSERT_RETURN(ice_st->comp_cnt < PJ_ICE_MAX_COMP, PJ_ETOOMANY);

    /* Component ID must be valid */
    PJ_ASSERT_RETURN(comp_id <= PJ_ICE_MAX_COMP, PJ_EICEINCOMPID);

    /* First component ID must be 1, second must be 2, etc., and 
     * they must be registered in order.
     */
    PJ_ASSERT_RETURN(ice_st->comps[comp_id-1] == ice_st->comp_cnt, 
		     PJ_EICEINCOMPID);

    /* All in order, add the component. */
    ice_st->comps[ice_st->comp_cnt++] = comp_id;

    return PJ_SUCCESS;
}


/* Add interface */
static void add_interface(pj_ice_st *ice_st, pj_ice_st_interface *is,
			  unsigned *p_itf_id, pj_bool_t notify,
			  void *notify_data)
{
    unsigned itf_id;

    itf_id = ice_st->itf_cnt++;
    ice_st->itfs[itf_id] = is;

    if (p_itf_id)
	*p_itf_id = itf_id;

    if (notify && ice_st->cb.on_interface_status) {
	(*ice_st->cb.on_interface_status)(ice_st, notify_data, 
					  PJ_SUCCESS, itf_id);
    }
}

/*
 * Add new host interface.
 */
PJ_DEF(pj_status_t) pj_ice_st_add_host_interface(pj_ice_st *ice_st,
						 unsigned comp_id,
						 pj_uint16_t local_pref,
					         const pj_sockaddr_in *addr,
				    		 unsigned *p_itf_id,
						 pj_bool_t notify,
						 void *notify_data)
{
    pj_ice_st_interface *is;
    pj_status_t status;

    /* Verify arguments */
    PJ_ASSERT_RETURN(ice_st && comp_id, PJ_EINVAL);

    /* Check that component ID present */
    PJ_ASSERT_RETURN(comp_id <= ice_st->comp_cnt, PJ_EICEINCOMPID);

    /* Can't add new interface while ICE is running */
    PJ_ASSERT_RETURN(ice_st->ice == NULL, PJ_EBUSY);

    /* Create interface */
    status = create_ice_interface(ice_st, PJ_ICE_CAND_TYPE_HOST, comp_id,
				  local_pref, addr, &is);
    if (status != PJ_SUCCESS)
	return status;

    /* For host interface, the address is the base address */
    pj_memcpy(&is->addr, &is->base_addr, sizeof(is->addr));

    /* Store this interface */
    add_interface(ice_st, is, p_itf_id, notify, notify_data);

    return PJ_SUCCESS;
}


/*
 * Enumerate and add all host interfaces.
 */
PJ_DEF(pj_status_t) pj_ice_st_add_all_host_interfaces(pj_ice_st *ice_st,
						      unsigned comp_id,
						      unsigned port,
						      pj_bool_t notify,
						      void *notify_data)
{
    pj_sockaddr_in addr;
    pj_status_t status;

    /* Yeah, TODO.
     * For now just add the default interface.
     */
    pj_sockaddr_in_init(&addr, NULL, (pj_uint16_t)port);
    
    status = pj_gethostip(&addr.sin_addr);
    if (status != PJ_SUCCESS)
	return status;

    return pj_ice_st_add_host_interface(ice_st, comp_id, 65535, &addr, 
					NULL, notify, notify_data);
}


/*
 * Add STUN mapping interface.
 */
PJ_DEF(pj_status_t) pj_ice_st_add_stun_interface(pj_ice_st *ice_st,
						 unsigned comp_id,
						 unsigned local_port,
						 pj_bool_t notify,
						 void *notify_data)
{
    /* Yeah, TODO */
    PJ_UNUSED_ARG(ice_st);
    PJ_UNUSED_ARG(comp_id);
    PJ_UNUSED_ARG(local_port);
    PJ_UNUSED_ARG(notify);
    PJ_UNUSED_ARG(notify_data);
    return -1;
}


/*
 * Add TURN mapping interface.
 */
PJ_DEF(pj_status_t) pj_ice_st_add_relay_interface(pj_ice_st *ice_st,
						  unsigned comp_id,
						  unsigned local_port,
						  pj_bool_t notify,
						  void *notify_data)
{
    /* Yeah, TODO */
    PJ_UNUSED_ARG(ice_st);
    PJ_UNUSED_ARG(comp_id);
    PJ_UNUSED_ARG(local_port);
    PJ_UNUSED_ARG(notify);
    PJ_UNUSED_ARG(notify_data);
    return -1;
}


/*
 * Create ICE!
 */
PJ_DEF(pj_status_t) pj_ice_st_init_ice(pj_ice_st *ice_st,
				       pj_ice_role role,
				       const pj_str_t *local_ufrag,
				       const pj_str_t *local_passwd)
{
    pj_status_t status;
    unsigned i;
    pj_ice_cb ice_cb;

    /* Check arguments */
    PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);
    /* Must not have ICE */
    PJ_ASSERT_RETURN(ice_st->ice == NULL, PJ_EINVALIDOP);

    /* Init callback */
    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_ice_complete = &on_ice_complete;
    ice_cb.on_rx_data = &on_rx_data;
    ice_cb.on_tx_pkt = &on_tx_pkt;

    /* Create! */
    status = pj_ice_create(&ice_st->stun_cfg, ice_st->obj_name, role, 
			   &ice_cb, local_ufrag, local_passwd, &ice_st->ice);
    if (status != PJ_SUCCESS)
	return status;

    /* Associate user data */
    ice_st->ice->user_data = (void*)ice_st;

    /* Add components */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	status = pj_ice_add_comp(ice_st->ice, ice_st->comps[i]);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Add candidates */
    for (i=0; i<ice_st->itf_cnt; ++i) {
	pj_ice_st_interface *is= ice_st->itfs[i];
	status = pj_ice_add_cand(ice_st->ice, is->comp_id, is->type, 
				 is->local_pref, &is->foundation,
				 &is->addr, &is->base_addr, NULL, 
				 sizeof(pj_sockaddr_in), 
				 (unsigned*)&is->cand_id);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    return PJ_SUCCESS;

on_error:
    for (i=0; i<ice_st->itf_cnt; ++i) {
	ice_st->itfs[i]->cand_id = -1;
    }
    if (ice_st->ice) {
	pj_ice_destroy(ice_st->ice);
	ice_st->ice = NULL;
    }
    return status;
}


/*
 * Enum candidates
 */
PJ_DEF(pj_status_t) pj_ice_st_enum_cands(pj_ice_st *ice_st,
					 unsigned *count,
					 pj_ice_cand cand[])
{
    unsigned i, cnt;
    pj_ice_cand *pcand;
    pj_status_t status;

    PJ_ASSERT_RETURN(ice_st && count && cand, PJ_EINVAL);
    PJ_ASSERT_RETURN(ice_st->ice, PJ_EINVALIDOP);

    cnt = pj_ice_get_cand_cnt(ice_st->ice);
    cnt = (cnt > *count) ? *count : cnt;
    *count = 0;

    for (i=0; i<cnt; ++i) {
	status = pj_ice_get_cand(ice_st->ice, i, &pcand);
	if (status != PJ_SUCCESS)
	    return status;

	pj_memcpy(&cand[i], pcand, sizeof(pj_ice_cand));
    }

    *count = cnt;
    return PJ_SUCCESS;
}


/*
 * Start ICE processing !
 */
PJ_DEF(pj_status_t) pj_ice_st_start_ice( pj_ice_st *ice_st,
					 const pj_str_t *rem_ufrag,
					 const pj_str_t *rem_passwd,
					 unsigned rem_cand_cnt,
					 const pj_ice_cand rem_cand[])
{
    pj_status_t status;

    status = pj_ice_create_check_list(ice_st->ice, rem_ufrag, rem_passwd,
				      rem_cand_cnt, rem_cand);
    if (status != PJ_SUCCESS)
	return status;

    return pj_ice_start_check(ice_st->ice);
}


/*
 * Stop ICE!
 */
PJ_DECL(pj_status_t) pj_ice_st_stop_ice(pj_ice_st *ice_st)
{
    if (ice_st->ice) {
	pj_ice_destroy(ice_st->ice);
	ice_st->ice = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Send data to peer agent.
 */
PJ_DEF(pj_status_t) pj_ice_st_send_data( pj_ice_st *ice_st,
					 unsigned comp_id,
					 const void *data,
					 pj_size_t data_len)
{
    if (!ice_st->ice)
	return PJ_ENOICE;

    return pj_ice_send_data(ice_st->ice, comp_id, data, data_len);
}



/*
 * Callback called by ICE session when ICE processing is complete, either
 * successfully or with failure.
 */
static void on_ice_complete(pj_ice *ice, pj_status_t status)
{
    pj_ice_st *ice_st = (pj_ice_st*)ice->user_data;
    if (ice_st->cb.on_ice_complete) {
	(*ice_st->cb.on_ice_complete)(ice_st, status);
    }
}


/*
 * Callback called by ICE session when it wants to send outgoing packet.
 */
static pj_status_t on_tx_pkt(pj_ice *ice, 
			     unsigned comp_id, unsigned cand_id,
			     const void *pkt, pj_size_t size,
			     const pj_sockaddr_t *dst_addr,
			     unsigned dst_addr_len)
{
    pj_ice_st *ice_st = (pj_ice_st*)ice->user_data;
    pj_ice_st_interface *is = NULL;
    unsigned i;
    pj_ssize_t pkt_size;
    pj_status_t status;

    PJ_UNUSED_ARG(comp_id);

    for (i=0; i<ice_st->itf_cnt; ++i) {
	if (ice_st->itfs[i]->cand_id == (int)cand_id) {
	    is = ice_st->itfs[i];
	    break;
	}
    }
    if (is == NULL) {
	return PJ_EICEINCANDID;
    }

    pkt_size = size;
    status = pj_ioqueue_sendto(is->key, &is->write_op, 
			       pkt, &pkt_size, 0,
			       dst_addr, dst_addr_len);
    
    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? PJ_SUCCESS : status;
}


/*
 * Callback called by ICE session when it receives application data.
 */
static void on_rx_data(pj_ice *ice, 
		       unsigned comp_id, unsigned cand_id,
		       void *pkt, pj_size_t size,
		       const pj_sockaddr_t *src_addr,
		       unsigned src_addr_len)
{
    pj_ice_st *ice_st = (pj_ice_st*)ice->user_data;

    if (ice_st->cb.on_rx_data) {
	(*ice_st->cb.on_rx_data)(ice_st, comp_id, cand_id, 
				 pkt, size, src_addr, src_addr_len);
    }
}



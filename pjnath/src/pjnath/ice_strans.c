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
#include <pjnath/ice_strans.h>
#include <pjnath/errno.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/ip_helper.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/compat/socket.h>


#if 0
#  define TRACE_PKT(expr)	    PJ_LOG(5,expr)
#else
#  define TRACE_PKT(expr)
#endif



/* ICE callbacks */
static void	   on_ice_complete(pj_ice_sess *ice, pj_status_t status);
static pj_status_t ice_tx_pkt(pj_ice_sess *ice, 
			      unsigned comp_id,
			      unsigned cand_id,
			      const void *pkt, pj_size_t size,
			      const pj_sockaddr_t *dst_addr,
			      unsigned dst_addr_len);
static void	   ice_rx_data(pj_ice_sess *ice, 
			      unsigned comp_id, 
			      void *pkt, pj_size_t size,
			      const pj_sockaddr_t *src_addr,
			      unsigned src_addr_len);

/* Ioqueue callback */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);

static void destroy_component(pj_ice_strans_comp *comp);
static void destroy_ice_st(pj_ice_strans *ice_st, pj_status_t reason);


/* STUN session callback */
static pj_status_t stun_on_send_msg(pj_stun_session *sess,
				    void *token,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len);
static void stun_on_request_complete(pj_stun_session *sess,
				     pj_status_t status,
				     void *token,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response,
				     const pj_sockaddr_t *src_addr,
				     unsigned src_addr_len);

/* TURN callbacks */
static void turn_on_rx_data(pj_turn_sock *turn_sock,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static void turn_on_state(pj_turn_sock *turn_sock, pj_turn_state_t old_state,
			  pj_turn_state_t new_state);


/* Keep-alive timer */
static void start_ka_timer(pj_ice_strans *ice_st);
static void stop_ka_timer(pj_ice_strans *ice_st);

/* Utility: print error */
#define ice_st_perror(ice_st,msg,rc) pjnath_perror(ice_st->obj_name,msg,rc)

/* Validate configuration */
static pj_status_t pj_ice_strans_cfg_check_valid(const pj_ice_strans_cfg *cfg)
{
    pj_status_t status;

    status = pj_stun_config_check_valid(&cfg->stun_cfg);
    if (!status)
	return status;

    /* If TURN is specified then TURN credential must be specified */
    PJ_ASSERT_RETURN(!pj_sockaddr_has_addr(&cfg->turn_srv) ||
		      cfg->turn_cred.type != PJ_STUN_AUTH_NONE,
		     PJ_EINVAL);

    return PJ_SUCCESS;
}


/* 
 * Create ICE stream transport 
 */
PJ_DEF(pj_status_t) pj_ice_strans_create( const pj_ice_strans_cfg *cfg,
					  const char *name,
					  unsigned comp_cnt,
					  void *user_data,
					  const pj_ice_strans_cb *cb,
					  pj_ice_strans **p_ice_st)
{
    pj_pool_t *pool;
    pj_ice_strans *ice_st;
    pj_status_t status;

    status = pj_ice_strans_cfg_check_valid(cfg);
    if (status != PJ_SUCCESS)
	return status;

    PJ_ASSERT_RETURN(comp_cnt && cb && p_ice_st, PJ_EINVAL);

    if (name == NULL)
	name = "icstr%p";

    pool = pj_pool_create(cfg->stun_cfg.pf, name, PJNATH_POOL_LEN_ICE_STRANS, 
			  PJNATH_POOL_INC_ICE_STRANS, NULL);
    ice_st = PJ_POOL_ZALLOC_T(pool, pj_ice_strans);
    ice_st->pool = pool;
    pj_memcpy(&ice_st->cfg, cfg, sizeof(*cfg));
    pj_stun_auth_cred_dup(pool, &ice_st->cfg.turn_cred, &cfg->turn_cred);
    pj_memcpy(ice_st->obj_name, pool->obj_name, PJ_MAX_OBJ_NAME);
    ice_st->user_data = user_data;
    
    ice_st->comp_cnt = comp_cnt;
    ice_st->comp = (pj_ice_strans_comp**) pj_pool_calloc(pool, comp_cnt, 
						     sizeof(void*));

    pj_memcpy(&ice_st->cb, cb, sizeof(*cb));


    PJ_LOG(4,(ice_st->obj_name, "ICE stream transport created"));

    *p_ice_st = ice_st;
    return PJ_SUCCESS;
}

/* Destroy ICE */
static void destroy_ice_st(pj_ice_strans *ice_st, pj_status_t reason)
{
    unsigned i;
    char obj_name[PJ_MAX_OBJ_NAME];

    if (reason == PJ_SUCCESS) {
	pj_memcpy(obj_name, ice_st->obj_name, PJ_MAX_OBJ_NAME);
	PJ_LOG(4,(obj_name, "ICE stream transport shutting down"));
    }

    /* Kill keep-alive timer, if any */
    stop_ka_timer(ice_st);

    /* Destroy ICE if we have ICE */
    if (ice_st->ice) {
	pj_ice_sess_destroy(ice_st->ice);
	ice_st->ice = NULL;
    }

    /* Destroy all components */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	if (ice_st->comp[i]) {
	    destroy_component(ice_st->comp[i]);
	    ice_st->comp[i] = NULL;
	}
    }
    ice_st->comp_cnt = 0;

    /* Done */
    pj_pool_release(ice_st->pool);

    if (reason == PJ_SUCCESS) {
	PJ_LOG(4,(obj_name, "ICE stream transport destroyed"));
    }
}

/*
 * Destroy ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_strans_destroy(pj_ice_strans *ice_st)
{
    destroy_ice_st(ice_st, PJ_SUCCESS);
    return PJ_SUCCESS;
}

/*
 * Resolve STUN server
 */
PJ_DEF(pj_status_t) pj_ice_strans_set_stun_domain(pj_ice_strans *ice_st,
						  pj_dns_resolver *resolver,
						  const pj_str_t *domain)
{
    /* Yeah, TODO */
    PJ_UNUSED_ARG(ice_st);
    PJ_UNUSED_ARG(resolver);
    PJ_UNUSED_ARG(domain);
    return -1;
}

/* Add new candidate */
static pj_status_t add_cand( pj_ice_strans *ice_st,
			     pj_ice_strans_comp *comp,
			     unsigned comp_id,
			     pj_ice_cand_type type,
			     pj_uint16_t local_pref,
			     const pj_sockaddr_in *addr,
			     pj_bool_t set_default)
{
    pj_ice_strans_cand *cand;
    unsigned i;

    PJ_ASSERT_RETURN(ice_st && comp && addr, PJ_EINVAL);
    PJ_ASSERT_RETURN(comp->cand_cnt < PJ_ICE_ST_MAX_CAND, PJ_ETOOMANY);

    /* Check that we don't have candidate with the same
     * address.
     */
    for (i=0; i<comp->cand_cnt; ++i) {
	if (pj_memcmp(addr, &comp->cand_list[i].addr, 
		      sizeof(pj_sockaddr_in))==0)
	{
	    /* Duplicate */
	    PJ_LOG(5,(ice_st->obj_name, "Duplicate candidate not added"));
	    return PJ_SUCCESS;
	}
    }

    cand = &comp->cand_list[comp->cand_cnt];

    pj_bzero(cand, sizeof(*cand));
    cand->type = type;
    cand->status = PJ_SUCCESS;
    pj_memcpy(&cand->addr, addr, sizeof(pj_sockaddr_in));
    cand->ice_cand_id = -1;
    cand->local_pref = local_pref;
    pj_ice_calc_foundation(ice_st->pool, &cand->foundation, type, 
			   &comp->local_addr);

    if (set_default) 
	comp->default_cand = comp->cand_cnt;

    PJ_LOG(5,(ice_st->obj_name, 
	      "Candidate %s:%d (type=%s) added to component %d",
	      pj_inet_ntoa(addr->sin_addr),
	      (int)pj_ntohs(addr->sin_port), 
	      pj_ice_get_cand_type_name(type),
	      comp_id));
    
    comp->cand_cnt++;
    return PJ_SUCCESS;
}

/*  Create new component (i.e. socket)  */
static pj_status_t create_component(pj_ice_strans *ice_st,
				    unsigned comp_id,
				    pj_uint32_t options,
				    const pj_sockaddr_in *addr,
				    pj_ice_strans_comp **p_comp)
{
    enum { MAX_RETRY=100, PORT_INC=2 };
    pj_ioqueue_callback ioqueue_cb;
    pj_ice_strans_comp *comp;
    int retry, addr_len;
    struct {
	pj_uint32_t a1, a2, a3;
    } tsx_id;
    pj_status_t status;

    comp = PJ_POOL_ZALLOC_T(ice_st->pool, pj_ice_strans_comp);
    comp->ice_st = ice_st;
    comp->comp_id = comp_id;
    comp->options = options;
    comp->sock = PJ_INVALID_SOCKET;
    comp->last_status = PJ_SUCCESS;

    /* Create transaction ID for STUN keep alives */
    tsx_id.a1 = 0;
    tsx_id.a2 = comp_id;
    tsx_id.a3 = (pj_uint32_t) (unsigned long) ice_st;
    pj_memcpy(comp->ka_tsx_id, &tsx_id, sizeof(comp->ka_tsx_id));

    /* Create socket */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &comp->sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Init address */
    if (addr) 
	pj_memcpy(&comp->local_addr, addr, sizeof(pj_sockaddr_in));
    else 
	pj_sockaddr_in_init(&comp->local_addr.ipv4, NULL, 0);

    /* Retry binding socket */
    for (retry=0; retry<MAX_RETRY; ++retry) {
	pj_uint16_t port;

	status = pj_sock_bind(comp->sock, &comp->local_addr, 
			      sizeof(pj_sockaddr_in));
	if (status == PJ_SUCCESS)
	    break;

	if (options & PJ_ICE_ST_OPT_NO_PORT_RETRY)
	    goto on_error;

	port = pj_ntohs(comp->local_addr.ipv4.sin_port);
	port += PORT_INC;
	comp->local_addr.ipv4.sin_port = pj_htons(port);
    }

    /* Get the actual port where the socket is bound to.
     * (don't care about the address, it will be retrieved later)
     */
    addr_len = sizeof(comp->local_addr);
    status = pj_sock_getsockname(comp->sock, &comp->local_addr, &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register to ioqueue */
    pj_bzero(&ioqueue_cb, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = &on_read_complete;
    status = pj_ioqueue_register_sock(ice_st->pool, 
				      ice_st->cfg.stun_cfg.ioqueue, 
				      comp->sock, comp, &ioqueue_cb, 
				      &comp->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Disable concurrency */
    status = pj_ioqueue_set_concurrency(comp->key, PJ_FALSE);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_ioqueue_op_key_init(&comp->read_op, sizeof(comp->read_op));
    pj_ioqueue_op_key_init(&comp->write_op, sizeof(comp->write_op));

    /* Kick start reading the socket */
    on_read_complete(comp->key, &comp->read_op, 0);

    /* If the socket is bound to INADDR_ANY, then lookup all interfaces in
     * the host and add them into cand_list. Otherwise if the socket is bound
     * to a specific interface, then only add that specific interface to
     * cand_list.
     */
    if (((options & PJ_ICE_ST_OPT_DONT_ADD_CAND)==0) &&
	comp->local_addr.ipv4.sin_addr.s_addr == 0) 
    {
	/* Socket is bound to INADDR_ANY */
	unsigned i, ifs_cnt;
	pj_sockaddr ifs[PJ_ICE_ST_MAX_CAND-2];

	/* Reset default candidate */
	comp->default_cand = -1;

	/* Enum all IP interfaces in the host */
	ifs_cnt = PJ_ARRAY_SIZE(ifs);
	status = pj_enum_ip_interface(pj_AF_INET(), &ifs_cnt, ifs);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Set default IP interface as the base address */
	status = pj_gethostip(pj_AF_INET(), &comp->local_addr);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Add candidate entry for each interface */
	for (i=0; i<ifs_cnt; ++i) {
	    pj_sockaddr_in cand_addr;
	    pj_bool_t set_default;
	    pj_uint16_t local_pref;

	    /* Ignore 127.0.0.0/24 address */
	    if ((pj_ntohl(ifs[i].ipv4.sin_addr.s_addr) >> 24)==127)
		continue;

	    pj_memcpy(&cand_addr, &comp->local_addr, sizeof(pj_sockaddr_in));
	    cand_addr.sin_addr.s_addr = ifs[i].ipv4.sin_addr.s_addr;


	    /* If the IP address is equal to local address, assign it
	     * as default candidate.
	     */
	    if (ifs[i].ipv4.sin_addr.s_addr == 
		    comp->local_addr.ipv4.sin_addr.s_addr) 
	    {
		set_default = PJ_TRUE;
		local_pref = 65535;
	    } else {
		set_default = PJ_FALSE;
		local_pref = 0;
	    }

	    status = add_cand(ice_st, comp, comp_id, 
			      PJ_ICE_CAND_TYPE_HOST, 
			      local_pref, &cand_addr, set_default);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}


    } else if ((options & PJ_ICE_ST_OPT_DONT_ADD_CAND)==0) {
	/* Socket is bound to specific address. 
	 * In this case only add that address as a single entry in the
	 * cand_list table.
	 */
	status = add_cand(ice_st, comp, comp_id, 
			  PJ_ICE_CAND_TYPE_HOST, 
			  65535, &comp->local_addr.ipv4,
			  PJ_TRUE);
	if (status != PJ_SUCCESS)
	    goto on_error;

    } else if (options & PJ_ICE_ST_OPT_DONT_ADD_CAND) {
	/* If application doesn't want to add candidate, just fix local_addr
	 * in case its value is zero.
	 */
	if (comp->local_addr.ipv4.sin_addr.s_addr == 0) {
	    status = pj_gethostip(pj_AF_INET(), &comp->local_addr);
	    if (status != PJ_SUCCESS)
		return status;
	}
    }


    /* Done */
    if (p_comp)
	*p_comp = comp;

    return PJ_SUCCESS;

on_error:
    destroy_component(comp);
    return status;
}

/* 
 * This is callback called by ioqueue on incoming packet 
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    pj_ice_strans_comp *comp = (pj_ice_strans_comp*) 
			    pj_ioqueue_get_user_data(key);
    pj_ice_strans *ice_st = comp->ice_st;
    pj_ssize_t pkt_size;
    enum { RETRY = 10 };
    unsigned retry;
    pj_status_t status;

    if (bytes_read > 0) {
	/*
	 * Okay, we got a packet from the socket for the component. There is
	 * a bit of situation here, since this packet could be one of these:
	 *
	 * 1) this could be the response of STUN binding request sent by
	 *    this component to a) an initial request to get the STUN mapped
	 *    address of this component, or b) subsequent request to keep
	 *    the binding alive.
	 * 
	 * 2) this could be a STUN request or response sent as part of ICE
	 *    discovery process.
	 *
	 * 3) this could be application's packet, e.g. when ICE processing
	 *    is done and agents start sending RTP/RTCP packets to each
	 *    other, or when ICE processing is not done and this ICE stream
	 *    transport decides to allow sending data.
	 *
	 */
	unsigned cand_id;

	/* Find candidate ID for this packet */
	for (cand_id=0; cand_id<comp->cand_cnt; ++cand_id) {
	    if (comp->cand_list[cand_id].type != PJ_ICE_CAND_TYPE_RELAYED)
		break;
	}
	if (cand_id == comp->cand_cnt) {
	    //pj_assert(!"We should have at least one host/srflx candidate");
	    //cand_id = 0;
	    PJ_LOG(2,(ice_st->obj_name, 
		      "Received pkt on comp %d which doesn't have host/srflx "
		      "candidate",
		      comp->comp_id));
	    goto next_packet;
	}

	/* Is this a STUN message? */
	status = pj_stun_msg_check(comp->pkt, bytes_read, 
				   PJ_STUN_IS_DATAGRAM);

	if (status == PJ_SUCCESS) {
	    if (comp->stun_sess &&
		PJ_STUN_IS_RESPONSE(((pj_stun_msg_hdr*)comp->pkt)->type) &&
		pj_memcmp(comp->pkt+8, comp->ka_tsx_id, 12) == 0) 
	    {
		status = pj_stun_session_on_rx_pkt(comp->stun_sess, comp->pkt,
						   bytes_read, 
						   PJ_STUN_IS_DATAGRAM, NULL,
						   NULL, &comp->src_addr, 
						   comp->src_addr_len);
	    } else if (ice_st->ice) {
		TRACE_PKT((comp->ice_st->obj_name, 
			  "Component %d RX packet from %s:%d",
			  comp->comp_id,
			  pj_inet_ntoa(comp->src_addr.ipv4.sin_addr),
			  (int)pj_ntohs(comp->src_addr.ipv4.sin_port)));

		status = pj_ice_sess_on_rx_pkt(ice_st->ice, comp->comp_id, 
					       cand_id, comp->pkt, bytes_read,
					       &comp->src_addr, 
					       comp->src_addr_len);
	    } else {
		/* This must have been a very late STUN reponse,
		 * or an early STUN Binding Request when our local
		 * ICE has not been created yet. */
	    }
	} else {
	    (*ice_st->cb.on_rx_data)(ice_st, comp->comp_id, 
				     comp->pkt, bytes_read, 
				     &comp->src_addr, comp->src_addr_len);
	}

    } else if (bytes_read < 0) {
	ice_st_perror(comp->ice_st, "ioqueue read callback error", 
		      -bytes_read);
    }

    /* Read next packet */
next_packet:
    for (retry=0; retry<RETRY;) {
	pkt_size = sizeof(comp->pkt);
	comp->src_addr_len = sizeof(comp->src_addr);
	status = pj_ioqueue_recvfrom(key, op_key, comp->pkt, &pkt_size, 
				     PJ_IOQUEUE_ALWAYS_ASYNC,
				     &comp->src_addr, &comp->src_addr_len);
	if (status == PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK) ||
	    status == PJ_STATUS_FROM_OS(OSERR_EINPROGRESS) || 
	    status == PJ_STATUS_FROM_OS(OSERR_ECONNRESET))
	{
	    ice_st_perror(comp->ice_st, "ioqueue recvfrom() error", status);
	    ++retry;
	    continue;
	} else if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    retry += 2;
	    ice_st_perror(comp->ice_st, "ioqueue recvfrom() error", status);
	} else {
	    break;
	}
    }
}

/* 
 * Destroy a component 
 */
static void destroy_component(pj_ice_strans_comp *comp)
{
    if (comp->turn_relay) {
	pj_turn_sock_destroy(comp->turn_relay);
	comp->turn_relay = NULL;
    }

    if (comp->stun_sess) {
	pj_stun_session_destroy(comp->stun_sess);
	comp->stun_sess = NULL;
    }

    if (comp->key) {
	pj_ioqueue_unregister(comp->key);
	comp->key = NULL;
	comp->sock = PJ_INVALID_SOCKET;
    } else if (comp->sock != PJ_INVALID_SOCKET && comp->sock != 0) {
	pj_sock_close(comp->sock);
	comp->sock = PJ_INVALID_SOCKET;
    }
}


/* STUN keep-alive timer callback */
static void ka_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)te->user_data;
    unsigned i;
    pj_status_t status;

    PJ_UNUSED_ARG(th);

    ice_st->ka_timer.id = PJ_FALSE;

    for (i=0; i<ice_st->comp_cnt; ++i) {
	pj_ice_strans_comp *comp = ice_st->comp[i];
	pj_stun_tx_data *tdata;
	unsigned j;

	/* Does this component have STUN server reflexive candidate? */
	for (j=0; j<comp->cand_cnt; ++j) {
	    if (comp->cand_list[j].type == PJ_ICE_CAND_TYPE_SRFLX)
		break;
	}
	if (j == comp->cand_cnt)
	    continue;

	/* Create STUN binding request */
	status = pj_stun_session_create_req(comp->stun_sess,
					    PJ_STUN_BINDING_REQUEST, 
					    PJ_STUN_MAGIC,
					    comp->ka_tsx_id, &tdata);
	if (status != PJ_SUCCESS)
	    continue;

	/* tdata->user_data is NULL for keep-alive */
	//tdata->user_data = NULL;

	++comp->pending_cnt;


	/* Send STUN binding request */
	PJ_LOG(5,(ice_st->obj_name, "Sending STUN keep-alive from %s;%d",
		  pj_inet_ntoa(comp->local_addr.ipv4.sin_addr),
		  pj_ntohs(comp->local_addr.ipv4.sin_port)));
	status = pj_stun_session_send_msg(comp->stun_sess, &comp->cand_list[j],
					  PJ_FALSE, PJ_TRUE, 
					  &ice_st->cfg.stun_srv,
					  sizeof(pj_sockaddr_in), tdata);
	if (status != PJ_SUCCESS) {
	    --comp->pending_cnt;
	}
    }

    /* Start next timer */
    start_ka_timer(ice_st);
}

/* Start STUN keep-alive timer */
static void start_ka_timer(pj_ice_strans *ice_st)
{
    pj_time_val delay;

    /* Skip if timer is already running */
    if (ice_st->ka_timer.id != PJ_FALSE)
	return;

    delay.sec = PJ_ICE_ST_KEEP_ALIVE_MIN;
    delay.msec = pj_rand() % (PJ_ICE_ST_KEEP_ALIVE_MAX_RAND * 1000);
    pj_time_val_normalize(&delay);

    ice_st->ka_timer.cb = &ka_timer_cb;
    ice_st->ka_timer.user_data = ice_st;
    
    if (pj_timer_heap_schedule(ice_st->cfg.stun_cfg.timer_heap, 
			       &ice_st->ka_timer, &delay)==PJ_SUCCESS)
    {
	ice_st->ka_timer.id = PJ_TRUE;
    }
}


/* Stop STUN keep-alive timer */
static void stop_ka_timer(pj_ice_strans *ice_st)
{
    /* Skip if timer is already stop */
    if (ice_st->ka_timer.id == PJ_FALSE)
	return;

    pj_timer_heap_cancel(ice_st->cfg.stun_cfg.timer_heap, &ice_st->ka_timer);
    ice_st->ka_timer.id = PJ_FALSE;
}


/*
 * Add STUN mapping to a component.
 */
static pj_status_t get_stun_mapped_addr(pj_ice_strans *ice_st,
					pj_ice_strans_comp *comp)
{
    pj_ice_strans_cand *cand;
    pj_stun_session_cb sess_cb;
    pj_stun_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(ice_st && comp, PJ_EINVAL);
    
    /* Just return (successfully) if STUN server is not configured */
    if (ice_st->cfg.stun_srv.addr.sa_family == 0)
	return PJ_SUCCESS;


    /* Create STUN session for this component */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_request_complete = &stun_on_request_complete;
    sess_cb.on_send_msg = &stun_on_send_msg;
    status = pj_stun_session_create(&ice_st->cfg.stun_cfg, ice_st->obj_name,
				    &sess_cb, PJ_FALSE, &comp->stun_sess);
    if (status != PJ_SUCCESS)
	return status;

    /* Associate component with STUN session */
    pj_stun_session_set_user_data(comp->stun_sess, (void*)comp);

    /* Create STUN binding request */
    status = pj_stun_session_create_req(comp->stun_sess, 
					PJ_STUN_BINDING_REQUEST, 
					PJ_STUN_MAGIC,
					comp->ka_tsx_id, 
					&tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Will be attached to tdata in send_msg() */
    cand = &comp->cand_list[comp->cand_cnt];

    /* Add pending count first, since stun_on_request_complete()
     * may be called before this function completes
     */
    comp->pending_cnt++;

    /* Add new alias to this component */
    cand->type = PJ_ICE_CAND_TYPE_SRFLX;
    cand->status = PJ_SUCCESS;
    cand->ice_cand_id = -1;
    cand->local_pref = 65535;
    pj_ice_calc_foundation(ice_st->pool, &cand->foundation, 
			   PJ_ICE_CAND_TYPE_SRFLX, &comp->local_addr);

    ++comp->cand_cnt;

    /* Send STUN binding request */
    status = pj_stun_session_send_msg(comp->stun_sess, (void*)cand, PJ_FALSE, 
				      PJ_TRUE, &ice_st->cfg.stun_srv, 
				      sizeof(pj_sockaddr_in), tdata);
    if (status != PJ_SUCCESS) {
	--comp->pending_cnt;
	--comp->cand_cnt;
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Create the component.
 */
PJ_DEF(pj_status_t) pj_ice_strans_create_comp(pj_ice_strans *ice_st,
					      unsigned comp_id,
					      pj_uint32_t options,
					      const pj_sockaddr_in *addr)
{
    pj_ice_strans_comp *comp = NULL;
    pj_status_t status;

    /* Verify arguments */
    PJ_ASSERT_RETURN(ice_st && comp_id, PJ_EINVAL);

    /* Check that component ID present */
    PJ_ASSERT_RETURN(comp_id <= ice_st->comp_cnt, PJNATH_EICEINCOMPID);

    /* Can't add new component while ICE is running */
    PJ_ASSERT_RETURN(ice_st->ice == NULL, PJ_EBUSY);
    

    /* Create component */
    status = create_component(ice_st, comp_id, options, addr, &comp);
    if (status != PJ_SUCCESS)
	return status;

    /* Start STUN mapped address resolution */
    if ((options & PJ_ICE_ST_OPT_DISABLE_STUN) == 0 &&
	pj_sockaddr_has_addr(&ice_st->cfg.stun_srv)) 
    {
	status = get_stun_mapped_addr(ice_st, comp);
	if (status != PJ_SUCCESS) {
	    destroy_component(comp);
	    return status;
	}
    }

    /* Create TURN relay if wanted. */
    if ((options & PJ_ICE_ST_OPT_DISABLE_RELAY) == 0 &&
	pj_sockaddr_has_addr(&ice_st->cfg.turn_srv)) 
    {
	pj_turn_sock_cb turn_sock_cb;
	char ipaddr[PJ_INET6_ADDRSTRLEN+8];
	pj_str_t s;

	pj_assert(comp->cand_cnt < PJ_ICE_ST_MAX_CAND);

	/* Init TURN socket */
	pj_bzero(&turn_sock_cb, sizeof(turn_sock_cb));
	turn_sock_cb.on_rx_data = &turn_on_rx_data;
	turn_sock_cb.on_state = &turn_on_state;

	status = pj_turn_sock_create(&ice_st->cfg.stun_cfg, pj_AF_INET(),
				     ice_st->cfg.turn_conn_type,
				     &turn_sock_cb, 0, comp,
				     &comp->turn_relay);
	if (status != PJ_SUCCESS) {
	    destroy_component(comp);
	    return status;
	}

	pj_sockaddr_print(&ice_st->cfg.turn_srv, ipaddr, sizeof(ipaddr), 0);

	++comp->pending_cnt;

	/* Start allocation */
	status = pj_turn_sock_init(comp->turn_relay, pj_cstr(&s, ipaddr), 
				   pj_sockaddr_get_port(&ice_st->cfg.turn_srv),
				   NULL, &ice_st->cfg.turn_cred, 
				   &ice_st->cfg.turn_alloc_param);
	if (status != PJ_SUCCESS) {
	    if (comp->turn_relay) {
		pj_turn_sock_destroy(comp->turn_relay);
	    }
	    comp->turn_relay = NULL;
	    --comp->pending_cnt;
	    destroy_component(comp);
	    return status;
	}

    }


    /* Store this component */
    ice_st->comp[comp_id-1] = comp;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ice_strans_add_cand( pj_ice_strans *ice_st,
					    unsigned comp_id,
					    pj_ice_cand_type type,
					    pj_uint16_t local_pref,
					    const pj_sockaddr_in *addr,
					    pj_bool_t set_default)
{
    pj_ice_strans_comp *comp;


    PJ_ASSERT_RETURN(ice_st && comp_id && addr, PJ_EINVAL);
    PJ_ASSERT_RETURN(comp_id <= ice_st->comp_cnt, PJ_EINVAL);
    PJ_ASSERT_RETURN(ice_st->comp[comp_id-1] != NULL, PJ_EINVALIDOP);

    comp = ice_st->comp[comp_id-1];
    return add_cand(ice_st, comp, comp_id, type, local_pref, addr, 
		    set_default);
}


PJ_DEF(pj_status_t) pj_ice_strans_get_comps_status(pj_ice_strans *ice_st)
{
    unsigned i;
    pj_status_t worst = PJ_SUCCESS;

    for (i=0; i<ice_st->comp_cnt; ++i) {
	pj_ice_strans_comp *comp = ice_st->comp[i];

	if (comp->last_status == PJ_SUCCESS) {
	    /* okay */
	} else if (comp->pending_cnt && worst==PJ_SUCCESS) {
	    worst = PJ_EPENDING;
	    break;
	} else if (comp->last_status != PJ_SUCCESS) {
	    worst = comp->last_status;
	    break;
	}

	if (worst != PJ_SUCCESS)
	    break;
    }

    return worst;
}

/*
 * Create ICE!
 */
PJ_DEF(pj_status_t) pj_ice_strans_init_ice(pj_ice_strans *ice_st,
					   pj_ice_sess_role role,
					   const pj_str_t *local_ufrag,
					   const pj_str_t *local_passwd)
{
    pj_status_t status;
    unsigned i;
    pj_ice_sess_cb ice_cb;
    const pj_uint8_t srflx_prio[4] = { 100, 126, 110, 0 };

    /* Check arguments */
    PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);
    /* Must not have ICE */
    PJ_ASSERT_RETURN(ice_st->ice == NULL, PJ_EINVALIDOP);
    /* Components must have been created */
    PJ_ASSERT_RETURN(ice_st->comp[0] != NULL, PJ_EINVALIDOP);

    /* Init callback */
    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_ice_complete = &on_ice_complete;
    ice_cb.on_rx_data = &ice_rx_data;
    ice_cb.on_tx_pkt = &ice_tx_pkt;

    /* Create! */
    status = pj_ice_sess_create(&ice_st->cfg.stun_cfg, ice_st->obj_name, role,
			        ice_st->comp_cnt, &ice_cb, 
			        local_ufrag, local_passwd, &ice_st->ice);
    if (status != PJ_SUCCESS)
	return status;

    /* Associate user data */
    ice_st->ice->user_data = (void*)ice_st;

    /* If default candidate for components are SRFLX one, upload a custom
     * type priority to ICE session so that SRFLX candidates will get
     * checked first.
     */
    if (ice_st->comp[0]->default_cand >= 0 &&
	ice_st->comp[0]->cand_list[ice_st->comp[0]->default_cand].type 
	    == PJ_ICE_CAND_TYPE_SRFLX)
    {
	pj_ice_sess_set_prefs(ice_st->ice, srflx_prio);
    }


    /* Add candidates */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	unsigned j;
	pj_ice_strans_comp *comp= ice_st->comp[i];

	for (j=0; j<comp->cand_cnt; ++j) {
	    pj_ice_strans_cand *cand = &comp->cand_list[j];
	    pj_sockaddr_t *local_addr, *relay_addr;

	    /* Skip if candidate is not ready */
	    if (cand->status != PJ_SUCCESS) {
		PJ_LOG(5,(ice_st->obj_name, 
			  "Candidate %d in component %d is not added",
			  j, i));
		continue;
	    }

	    /* Skip if candidate has no address */
	    if (!pj_sockaddr_has_addr(&cand->addr)) {
		PJ_LOG(5,(ice_st->obj_name, 
			  "Candidate %d in component %d is not added",
			  j, i));
		continue;
	    }

	    if (cand->type == PJ_ICE_CAND_TYPE_RELAYED) {
		local_addr = &cand->addr;
		relay_addr = &cand->addr;
	    } else {
		local_addr = &comp->local_addr;
		relay_addr = NULL;
	    }
	    status = pj_ice_sess_add_cand(ice_st->ice, comp->comp_id, 
					  cand->type, cand->local_pref, 
					  &cand->foundation, &cand->addr, 
					  local_addr, relay_addr, 
					  sizeof(pj_sockaddr_in), 
					  (unsigned*)&cand->ice_cand_id);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    return PJ_SUCCESS;

on_error:
    pj_ice_strans_stop_ice(ice_st);
    return status;
}

/*
 * Enum candidates
 */
PJ_DEF(pj_status_t) pj_ice_strans_enum_cands(pj_ice_strans *ice_st,
					 unsigned *count,
					 pj_ice_sess_cand cand[])
{
    unsigned i, cnt;
    pj_ice_sess_cand *pcand;

    PJ_ASSERT_RETURN(ice_st && count && cand, PJ_EINVAL);
    PJ_ASSERT_RETURN(ice_st->ice, PJ_EINVALIDOP);

    cnt = ice_st->ice->lcand_cnt;
    cnt = (cnt > *count) ? *count : cnt;
    *count = 0;

    for (i=0; i<cnt; ++i) {
	pcand = &ice_st->ice->lcand[i];
	pj_memcpy(&cand[i], pcand, sizeof(pj_ice_sess_cand));
    }

    *count = cnt;
    return PJ_SUCCESS;
}

/*
 * Start ICE processing !
 */
PJ_DEF(pj_status_t) pj_ice_strans_start_ice( pj_ice_strans *ice_st,
					     const pj_str_t *rem_ufrag,
					     const pj_str_t *rem_passwd,
					     unsigned rem_cand_cnt,
					     const pj_ice_sess_cand rem_cand[])
{
    pj_status_t status;

    status = pj_ice_sess_create_check_list(ice_st->ice, rem_ufrag, rem_passwd,
					   rem_cand_cnt, rem_cand);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_ice_sess_start_check(ice_st->ice);
    if (status != PJ_SUCCESS) {
	pj_ice_strans_stop_ice(ice_st);
    }

    return status;
}

/*
 * Stop ICE!
 */
PJ_DEF(pj_status_t) pj_ice_strans_stop_ice(pj_ice_strans *ice_st)
{
    unsigned i;

    if (ice_st->ice) {
	pj_ice_sess_destroy(ice_st->ice);
	ice_st->ice = NULL;
    }

    /* Invalidate all candidate Ids */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	unsigned j;
	for (j=0; j<ice_st->comp[i]->cand_cnt; ++j) {
	    ice_st->comp[i]->cand_list[j].ice_cand_id = -1;
	}
    }

    return PJ_SUCCESS;
}

/*
 * Application wants to send outgoing packet.
 */
PJ_DEF(pj_status_t) pj_ice_strans_sendto( pj_ice_strans *ice_st,
					  unsigned comp_id,
					  const void *data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *dst_addr,
					  int dst_addr_len)
{
    pj_ssize_t pkt_size;
    pj_ice_strans_comp *comp;
    pj_status_t status;

    PJ_ASSERT_RETURN(ice_st && comp_id && comp_id <= ice_st->comp_cnt &&
		     dst_addr && dst_addr_len, PJ_EINVAL);

    comp = ice_st->comp[comp_id-1];

    /* If ICE is available, send data with ICE */
    if (ice_st->ice) {
	if (comp->turn_relay) {
	    pj_turn_sock_lock(comp->turn_relay);
	}
	status = pj_ice_sess_send_data(ice_st->ice, comp_id, data, data_len);
	if (comp->turn_relay) {
	    pj_turn_sock_unlock(comp->turn_relay);
	}
	return status;
    }

    /* Otherwise send direcly with the socket. This is for compatibility
     * with remote that doesn't support ICE.
     */
    pkt_size = data_len;
    status = pj_ioqueue_sendto(comp->key, &comp->write_op, 
			       data, &pkt_size, 0,
			       dst_addr, dst_addr_len);
    
    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? PJ_SUCCESS : status;
}

/*
 * Callback called by ICE session when ICE processing is complete, either
 * successfully or with failure.
 */
static void on_ice_complete(pj_ice_sess *ice, pj_status_t status)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;
    if (ice_st->cb.on_ice_complete) {
	(*ice_st->cb.on_ice_complete)(ice_st, status);
    }
}

/*
 * Callback called by ICE session when it wants to send outgoing packet.
 */
static pj_status_t ice_tx_pkt(pj_ice_sess *ice, 
			      unsigned comp_id, 
			      unsigned cand_id,
			      const void *pkt, pj_size_t size,
			      const pj_sockaddr_t *dst_addr,
			      unsigned dst_addr_len)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;
    pj_ice_strans_comp *comp;
    pj_ice_strans_cand *cand;
    pj_status_t status;

    PJ_ASSERT_RETURN(comp_id && comp_id <= ice_st->comp_cnt, PJ_EINVAL);

    comp = ice_st->comp[comp_id-1];
    cand = &comp->cand_list[cand_id];

    TRACE_PKT((comp->ice_st->obj_name, 
	      "Component %d candidate %d TX packet to %s:%d",
	      comp_id, cand_id,
	      pj_inet_ntoa(((pj_sockaddr_in*)dst_addr)->sin_addr),
	      (int)pj_ntohs(((pj_sockaddr_in*)dst_addr)->sin_port)));

    if (cand->type == PJ_ICE_CAND_TYPE_RELAYED) {
	if (comp->turn_relay) {
	    status = pj_turn_sock_sendto(comp->turn_relay, pkt, size,
					 dst_addr, dst_addr_len);
	} else {
	    status = PJ_EINVALIDOP;
	}
    } else {
	pj_ssize_t pkt_size = size;
	status = pj_ioqueue_sendto(comp->key, &comp->write_op, 
				   pkt, &pkt_size, 0,
				   dst_addr, dst_addr_len);
    }
    
    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? PJ_SUCCESS : status;
}

/*
 * Callback called by ICE session when it receives application data.
 */
static void ice_rx_data(pj_ice_sess *ice, 
		        unsigned comp_id, 
		        void *pkt, pj_size_t size,
		        const pj_sockaddr_t *src_addr,
		        unsigned src_addr_len)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;

    if (ice_st->cb.on_rx_data) {
	(*ice_st->cb.on_rx_data)(ice_st, comp_id, pkt, size, 
				 src_addr, src_addr_len);
    }
}

/*
 * Callback called by STUN session to send outgoing packet.
 */
static pj_status_t stun_on_send_msg(pj_stun_session *sess,
				    void *token,
				    const void *pkt,
				    pj_size_t size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len)
{
    pj_ice_strans_comp *comp;
    pj_ssize_t pkt_size;
    pj_status_t status;

    PJ_UNUSED_ARG(token);

    comp = (pj_ice_strans_comp*) pj_stun_session_get_user_data(sess);
    pkt_size = size;
    status = pj_ioqueue_sendto(comp->key, &comp->write_op, 
			       pkt, &pkt_size, 0,
			       dst_addr, dst_addr_len);
    
    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? PJ_SUCCESS : status;
}

/*
 * Callback sent by STUN session when outgoing STUN request has
 * completed.
 */
static void stun_on_request_complete(pj_stun_session *sess,
				     pj_status_t status,
				     void *token,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response,
				     const pj_sockaddr_t *src_addr,
				     unsigned src_addr_len)
{
    pj_ice_strans_comp *comp;
    pj_ice_strans_cand *cand = NULL;
    pj_stun_xor_mapped_addr_attr *xa;
    pj_stun_mapped_addr_attr *ma;
    pj_sockaddr *mapped_addr;
    char ip[20];

    comp = (pj_ice_strans_comp*) pj_stun_session_get_user_data(sess);
    cand = (pj_ice_strans_cand*) token;

    PJ_UNUSED_ARG(token);
    PJ_UNUSED_ARG(tdata);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    if (cand == NULL) {
	/* This is keep-alive */
	if (status != PJ_SUCCESS) {
	    ice_st_perror(comp->ice_st, "STUN keep-alive request failed",
			  status);
	}
	return;
    }

    /* Decrement pending count for this component */
    pj_assert(comp->pending_cnt > 0);
    comp->pending_cnt--;

    if (status == PJNATH_ESTUNTIMEDOUT) {

	PJ_LOG(4,(comp->ice_st->obj_name, 
		  "STUN Binding request has timed-out, will retry "
		  "again alter"));

	/* Restart keep-alive timer */
	start_ka_timer(comp->ice_st);
	return;

    } else if (status != PJ_SUCCESS) {
	comp->last_status = cand->status = status;
	ice_st_perror(comp->ice_st, "STUN Binding request failed", 
		      cand->status);
	return;
    }

    xa = (pj_stun_xor_mapped_addr_attr*)
	 pj_stun_msg_find_attr(response, PJ_STUN_ATTR_XOR_MAPPED_ADDR, 0);
    ma = (pj_stun_mapped_addr_attr*)
	 pj_stun_msg_find_attr(response, PJ_STUN_ATTR_MAPPED_ADDR, 0);

    if (xa)
	mapped_addr = &xa->sockaddr;
    else if (ma)
	mapped_addr = &ma->sockaddr;
    else {
	cand->status = PJNATH_ESTUNNOMAPPEDADDR;
	ice_st_perror(comp->ice_st, "STUN Binding request failed", 
		      cand->status);
	return;
    }

    /* Save IP address for logging */
    pj_ansi_strcpy(ip, pj_inet_ntoa(comp->local_addr.ipv4.sin_addr));

    /* Ignore response if it reports the same address */
    if (comp->local_addr.ipv4.sin_addr.s_addr == mapped_addr->ipv4.sin_addr.s_addr &&
	comp->local_addr.ipv4.sin_port == mapped_addr->ipv4.sin_port)
    {
	PJ_LOG(4,(comp->ice_st->obj_name, 
		  "Candidate %s:%d is directly connected to Internet, "
		  "STUN mapped address is ignored",
		  ip, pj_ntohs(comp->local_addr.ipv4.sin_port)));
	return;
    }

    PJ_LOG(5,(comp->ice_st->obj_name, 
	      "STUN mapped address for %s:%d is %s:%d",
	      ip, (int)pj_ntohs(comp->local_addr.ipv4.sin_port),
	      pj_inet_ntoa(mapped_addr->ipv4.sin_addr),
	      (int)pj_ntohs(mapped_addr->ipv4.sin_port)));
    pj_memcpy(&cand->addr, mapped_addr, sizeof(pj_sockaddr_in));
    cand->status = PJ_SUCCESS;

    /* Set this candidate as the default candidate */
    comp->default_cand = (cand - comp->cand_list);
    comp->last_status = PJ_SUCCESS;

    /* We have STUN, so we must start the keep-alive timer */
    start_ka_timer(comp->ice_st);

    /* Notify app that STUN address has changed. */
    if (comp->ice_st->cb.on_addr_change)
	(*comp->ice_st->cb.on_addr_change)(comp->ice_st, comp->comp_id, 
					   (cand - comp->cand_list));
}


/* Callback when TURN client has received a packet */
static void turn_on_rx_data(pj_turn_sock *turn_sock,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_ice_strans_comp *comp;
    unsigned cand_id;
    pj_status_t status;

    comp = (pj_ice_strans_comp*) pj_turn_sock_get_user_data(turn_sock);
    if (comp == NULL) {
	return;
    }

    if (comp->ice_st->ice == NULL) {
	/* The session is gone */
	return;
    }

    /* Find candidate ID for this packet */
    for (cand_id=0; cand_id<comp->cand_cnt; ++cand_id) {
	if (comp->cand_list[cand_id].type == PJ_ICE_CAND_TYPE_RELAYED)
	    break;
    }
    if (cand_id == comp->cand_cnt) {
	pj_assert(!"Missing relay candidate");
	return;
    }

    /* Hand over the packet to ICE */
    status = pj_ice_sess_on_rx_pkt(comp->ice_st->ice, comp->comp_id, 
				   cand_id, pkt, pkt_len,
				   peer_addr, addr_len);

    if (status != PJ_SUCCESS) {
	ice_st_perror(comp->ice_st, "Error processing packet from TURN relay", 
		      status);
    }
}


/* Callback when TURN client state has changed */
static void turn_on_state(pj_turn_sock *turn_sock, pj_turn_state_t old_state,
			  pj_turn_state_t new_state)
{
    pj_ice_strans_comp *comp;

    comp = (pj_ice_strans_comp*) pj_turn_sock_get_user_data(turn_sock);
    if (comp == NULL) {
	/* Not interested in further state notification once the relay is
	 * disconnecting.
	 */
	return;
    }

    PJ_LOG(5,(comp->ice_st->obj_name, "TURN client state changed %s --> %s",
	      pj_turn_state_name(old_state), pj_turn_state_name(new_state)));

    if (old_state < PJ_TURN_STATE_READY &&
	new_state >= PJ_TURN_STATE_READY)
    {
	pj_assert(comp->pending_cnt > 0);
	comp->pending_cnt--;
    }

    if (new_state == PJ_TURN_STATE_READY) {
	pj_turn_session_info rel_info;
	char ipaddr[PJ_INET6_ADDRSTRLEN+8];
	pj_ice_strans_cand *cand;

	/* Get allocation info */
	pj_turn_sock_get_info(turn_sock, &rel_info);

	/* Add a relay candidate to this component */
	pj_assert(comp->cand_cnt < PJ_ICE_ST_MAX_CAND);
	cand = &comp->cand_list[comp->cand_cnt++];

	/* Add new candidate to this component */
	cand->type = PJ_ICE_CAND_TYPE_RELAYED;
	cand->status = PJ_SUCCESS;
	cand->ice_cand_id = -1;
	cand->local_pref = 65535;
	pj_sockaddr_cp(&cand->addr, &rel_info.relay_addr);
	pj_ice_calc_foundation(comp->ice_st->pool, &cand->foundation, 
			       PJ_ICE_CAND_TYPE_RELAYED, 
			       &rel_info.relay_addr);

	PJ_LOG(4,(comp->ice_st->obj_name, 
		  "Component %d cand %d: relay address: %s",
		  comp->comp_id, cand - comp->cand_list,
		  pj_sockaddr_print(&rel_info.relay_addr, ipaddr, 
				     sizeof(ipaddr), 3)));

    } else if (new_state >= PJ_TURN_STATE_DEALLOCATING) {
	/* Unregister ourself from the TURN relay */
	pj_turn_sock_set_user_data(turn_sock, NULL);
	comp->turn_relay = NULL;

	PJ_LOG(4,(comp->ice_st->obj_name, "Relay destroyed"));
    }
}


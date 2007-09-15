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
#include <pjmedia/transport_ice.h>
#include <pjnath/errno.h>
#include <pj/assert.h>
#include <pj/log.h>

struct transport_ice
{
    pjmedia_transport	 base;
    pj_ice_strans	*ice_st;
    pjmedia_ice_cb	 cb;

    pj_time_val		 start_ice;
    
    void		*stream;
    pj_sockaddr_in	 remote_rtp;
    pj_sockaddr_in	 remote_rtcp;

    void	       (*rtp_cb)(void*,
			         const void*,
				 pj_ssize_t);
    void	       (*rtcp_cb)(void*,
				  const void*,
				  pj_ssize_t);
};


/*
 * These are media transport operations.
 */
static pj_status_t tp_get_info(pjmedia_transport *tp,
			       pjmedia_sock_info *info);
static pj_status_t tp_attach( pjmedia_transport *tp,
			      void *stream,
			      const pj_sockaddr_t *rem_addr,
			      const pj_sockaddr_t *rem_rtcp,
			      unsigned addr_len,
			      void (*rtp_cb)(void*,
					     const void*,
					     pj_ssize_t),
			      void (*rtcp_cb)(void*,
					      const void*,
					      pj_ssize_t));
static void	   tp_detach( pjmedia_transport *tp,
			      void *strm);
static pj_status_t tp_send_rtp( pjmedia_transport *tp,
			        const void *pkt,
			        pj_size_t size);
static pj_status_t tp_send_rtcp( pjmedia_transport *tp,
			         const void *pkt,
			         pj_size_t size);

/*
 * And these are ICE callbacks.
 */
static void ice_on_rx_data(pj_ice_strans *ice_st, unsigned comp_id, 
			   void *pkt, pj_size_t size,
			   const pj_sockaddr_t *src_addr,
			   unsigned src_addr_len);
static void ice_on_ice_complete(pj_ice_strans *ice_st, 
			        pj_status_t status);


static pjmedia_transport_op tp_ice_op = 
{
    &tp_get_info,
    &tp_attach,
    &tp_detach,
    &tp_send_rtp,
    &tp_send_rtcp,
    &pjmedia_ice_destroy
};


/*
 * Create ICE media transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_create(pjmedia_endpt *endpt,
				       const char *name,
				       unsigned comp_cnt,
				       pj_stun_config *stun_cfg,
				       const pjmedia_ice_cb *cb,
	    			       pjmedia_transport **p_tp)
{
    pj_ice_strans *ice_st;
    pj_ice_strans_cb ice_st_cb;
    struct transport_ice *tp_ice;
    pj_status_t status;

    PJ_UNUSED_ARG(endpt);

    /* Configure ICE callbacks */
    pj_bzero(&ice_st_cb, sizeof(ice_st_cb));
    ice_st_cb.on_ice_complete = &ice_on_ice_complete;
    ice_st_cb.on_rx_data = &ice_on_rx_data;

    /* Create ICE */
    status = pj_ice_strans_create(stun_cfg, name, comp_cnt, NULL, 
			      &ice_st_cb, &ice_st);
    if (status != PJ_SUCCESS)
	return status;


    /* Create transport instance and attach to ICE */
    tp_ice = PJ_POOL_ZALLOC_T(ice_st->pool, struct transport_ice);
    tp_ice->ice_st = ice_st;
    pj_ansi_strcpy(tp_ice->base.name, ice_st->obj_name);
    tp_ice->base.op = &tp_ice_op;
    tp_ice->base.type = PJMEDIA_TRANSPORT_TYPE_ICE;

    if (cb)
	pj_memcpy(&tp_ice->cb, cb, sizeof(pjmedia_ice_cb));

    ice_st->user_data = (void*)tp_ice;

    /* Done */
    if (p_tp)
	*p_tp = &tp_ice->base;

    return PJ_SUCCESS;
}


/*
 * Destroy ICE media transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_destroy(pjmedia_transport *tp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    if (tp_ice->ice_st) {
	pj_ice_strans_destroy(tp_ice->ice_st);
	/*Must not touch tp_ice after ice_st is destroyed!
	 (it has the pool)
	 tp_ice->ice_st = NULL;
	 */
    }

    return PJ_SUCCESS;
}


/*
 * Start media transport initialization.
 */
PJ_DEF(pj_status_t) pjmedia_ice_start_init( pjmedia_transport *tp,
					    unsigned options,
					    const pj_sockaddr_in *start_addr,
					    const pj_sockaddr_in *stun_srv,
					    const pj_sockaddr_in *turn_srv)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_status_t status;

    status = pj_ice_strans_set_stun_srv(tp_ice->ice_st, stun_srv, turn_srv);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_ice_strans_create_comp(tp_ice->ice_st, 1, options, start_addr);
    if (status != PJ_SUCCESS)
	return status;

    if (tp_ice->ice_st->comp_cnt > 1) {
	pj_sockaddr_in addr;
	pj_uint16_t port;

	pj_memcpy(&addr, &tp_ice->ice_st->comp[0]->local_addr.ipv4,
		  sizeof(pj_sockaddr_in));
	if (start_addr)
	    addr.sin_addr.s_addr = start_addr->sin_addr.s_addr;
	else
	    addr.sin_addr.s_addr = 0;

	port = pj_ntohs(addr.sin_port);
	++port;
	addr.sin_port = pj_htons(port);
	status = pj_ice_strans_create_comp(tp_ice->ice_st, 2, options, &addr);
	if (status != PJ_SUCCESS)
	    return status;
    }
    return status;
}


/*
 * Get the status of media transport initialization.
 */
PJ_DEF(pj_status_t) pjmedia_ice_get_init_status(pjmedia_transport *tp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    return pj_ice_strans_get_comps_status(tp_ice->ice_st);
}


/*
 * Get the component for the specified component ID.
 */
PJ_DEF(pj_status_t) pjmedia_ice_get_comp( pjmedia_transport *tp,
					  unsigned comp_id,
					  pj_ice_strans_comp *comp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    PJ_ASSERT_RETURN(tp && comp_id && comp_id <= tp_ice->ice_st->comp_cnt &&
		     comp, PJ_EINVAL);

    pj_memcpy(comp, tp_ice->ice_st->comp[comp_id-1], sizeof(pj_ice_strans_comp));
    return PJ_SUCCESS;		    
}


/*
 * Create ICE! This happens when:
 *  - UAC is ready to send offer
 *  - UAS have just received an offer.
 */
PJ_DEF(pj_status_t) pjmedia_ice_init_ice(pjmedia_transport *tp,
					 pj_ice_sess_role role,
					 const pj_str_t *local_ufrag,
					 const pj_str_t *local_passwd)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    return pj_ice_strans_init_ice(tp_ice->ice_st, role, local_ufrag, local_passwd);
}


/*
 * For both UAC and UAS, pass in the SDP before sending it to remote.
 * This will add ICE attributes to the SDP.
 */
PJ_DEF(pj_status_t) pjmedia_ice_modify_sdp(pjmedia_transport *tp,
					   pj_pool_t *pool,
					   pjmedia_sdp_session *sdp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    enum { MAXLEN = 256 };
    char *buffer;
    pjmedia_sdp_attr *attr;
    unsigned i, cand_cnt;

    buffer = (char*) pj_pool_alloc(pool, MAXLEN);

    /* Create ice-ufrag attribute */
    attr = pjmedia_sdp_attr_create(pool, "ice-ufrag", 
				   &tp_ice->ice_st->ice->rx_ufrag);
    sdp->attr[sdp->attr_count++] = attr;

    /* Create ice-pwd attribute */
    attr = pjmedia_sdp_attr_create(pool, "ice-pwd", 
				   &tp_ice->ice_st->ice->rx_pass);
    sdp->attr[sdp->attr_count++] = attr;

    /* Add all candidates (to media level) */
    cand_cnt = tp_ice->ice_st->ice->lcand_cnt;
    for (i=0; i<cand_cnt; ++i) {
	pj_ice_sess_cand *cand;
	pj_str_t value;
	int len;

	cand = &tp_ice->ice_st->ice->lcand[i];

	len = pj_ansi_snprintf( buffer, MAXLEN,
				"%.*s %d UDP %u %s %d typ ",
				(int)cand->foundation.slen,
				cand->foundation.ptr,
				cand->comp_id,
				cand->prio,
				pj_inet_ntoa(cand->addr.ipv4.sin_addr),
				(int)pj_ntohs(cand->addr.ipv4.sin_port));
	if (len < 1 || len >= MAXLEN)
	    return PJ_ENAMETOOLONG;

	switch (cand->type) {
	case PJ_ICE_CAND_TYPE_HOST:
	    len = pj_ansi_snprintf(buffer+len, MAXLEN-len,
			     "host");
	    break;
	case PJ_ICE_CAND_TYPE_SRFLX:
	    len = pj_ansi_snprintf(buffer+len, MAXLEN-len,
			     "srflx raddr %s rport %d",
			     pj_inet_ntoa(cand->base_addr.ipv4.sin_addr),
			     (int)pj_ntohs(cand->base_addr.ipv4.sin_port));
	    break;
	case PJ_ICE_CAND_TYPE_RELAYED:
	    PJ_TODO(RELATED_ADDR_FOR_RELAYED_ADDR);
	    len = pj_ansi_snprintf(buffer+len, MAXLEN-len,
			     "srflx raddr %s rport %d",
			     pj_inet_ntoa(cand->base_addr.ipv4.sin_addr),
			     (int)pj_ntohs(cand->base_addr.ipv4.sin_port));
	    break;
	case PJ_ICE_CAND_TYPE_PRFLX:
	    len = pj_ansi_snprintf(buffer+len, MAXLEN-len,
			     "prflx raddr %s rport %d",
			     pj_inet_ntoa(cand->base_addr.ipv4.sin_addr),
			     (int)pj_ntohs(cand->base_addr.ipv4.sin_port));
	    break;
	default:
	    pj_assert(!"Invalid candidate type");
	    break;
	}
	if (len < 1 || len >= MAXLEN)
	    return PJ_ENAMETOOLONG;

	value = pj_str(buffer);
	attr = pjmedia_sdp_attr_create(pool, "candidate", &value);
	sdp->media[0]->attr[sdp->media[0]->attr_count++] = attr;
    }

    /* Done */
    return PJ_SUCCESS;

}


/* Parse a=candidate line */
static pj_status_t parse_cand(pj_pool_t *pool,
			      const pj_str_t *orig_input,
			      pj_ice_sess_cand *cand)
{
    pj_str_t input;
    char *token, *host;
    pj_str_t s;
    pj_status_t status = PJNATH_EICEINCANDSDP;

    pj_bzero(cand, sizeof(*cand));
    pj_strdup_with_null(pool, &input, orig_input);

    /* Foundation */
    token = strtok(input.ptr, " ");
    if (!token)
	goto on_return;
    pj_strdup2(pool, &cand->foundation, token);

    /* Component ID */
    token = strtok(NULL, " ");
    if (!token)
	goto on_return;
    cand->comp_id = atoi(token);

    /* Transport */
    token = strtok(NULL, " ");
    if (!token)
	goto on_return;
    if (strcmp(token, "UDP") != 0)
	goto on_return;

    /* Priority */
    token = strtok(NULL, " ");
    if (!token)
	goto on_return;
    cand->prio = atoi(token);

    /* Host */
    host = strtok(NULL, " ");
    if (!host)
	goto on_return;
    if (pj_sockaddr_in_init(&cand->addr.ipv4, pj_cstr(&s, host), 0))
	goto on_return;

    /* Port */
    token = strtok(NULL, " ");
    if (!token)
	goto on_return;
    cand->addr.ipv4.sin_port = pj_htons((pj_uint16_t)atoi(token));

    /* typ */
    token = strtok(NULL, " ");
    if (!token)
	goto on_return;
    if (strcmp(token, "typ") != 0)
	goto on_return;

    /* candidate type */
    token = strtok(NULL, " ");
    if (!token)
	goto on_return;

    if (strcmp(token, "host") == 0) {
	cand->type = PJ_ICE_CAND_TYPE_HOST;

    } else if (strcmp(token, "srflx") == 0) {
	cand->type = PJ_ICE_CAND_TYPE_SRFLX;

    } else if (strcmp(token, "relay") == 0) {
	cand->type = PJ_ICE_CAND_TYPE_RELAYED;

    } else if (strcmp(token, "prflx") == 0) {
	cand->type = PJ_ICE_CAND_TYPE_PRFLX;

    } else {
	goto on_return;
    }


    status = PJ_SUCCESS;

on_return:
    return status;
}


/* Disable ICE when SDP from remote doesn't contain a=candidate line */
static void set_no_ice(struct transport_ice *tp_ice, const char *reason)
{
    PJ_LOG(4,(tp_ice->ice_st->obj_name, 
	      "Disabling local ICE, reason=%s", reason));
    pjmedia_ice_stop_ice(&tp_ice->base);
}


/*
 * Start ICE checks when both offer and answer are available.
 */
PJ_DEF(pj_status_t) pjmedia_ice_start_ice(pjmedia_transport *tp,
					  pj_pool_t *pool,
					  const pjmedia_sdp_session *rem_sdp,
					  unsigned media_index)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    const pjmedia_sdp_attr *attr;
    unsigned i, cand_cnt;
    pj_ice_sess_cand cand[PJ_ICE_MAX_CAND];
    const pjmedia_sdp_media *sdp_med;
    pj_bool_t remote_is_lite = PJ_FALSE;
    pj_bool_t ice_mismatch = PJ_FALSE;
    pjmedia_sdp_conn *conn = NULL;
    pj_sockaddr conn_addr;
    pj_bool_t conn_found_in_candidate = PJ_FALSE;
    const pj_str_t STR_CANDIDATE = {"candidate", 9};
    const pj_str_t STR_ICE_LITE = {"ice-lite", 8};
    const pj_str_t STR_ICE_MISMATCH = {"ice-mismatch", 12};
    pj_str_t uname, pass;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && pool && rem_sdp, PJ_EINVAL);
    PJ_ASSERT_RETURN(media_index < rem_sdp->media_count, PJ_EINVAL);

    sdp_med = rem_sdp->media[media_index];

    /* Get the SDP connection for the media stream.
     * We'll verify later if the SDP connection address is specified 
     * as one of the candidate.
     */
    conn = sdp_med->conn;
    if (conn == NULL)
	conn = rem_sdp->conn;

    if (conn == NULL) {
	/* Unable to find SDP connection */
	return PJMEDIA_SDP_EMISSINGCONN;
    }

    pj_sockaddr_in_init(&conn_addr.ipv4, &conn->addr, 
			(pj_uint16_t)sdp_med->desc.port);

    /* Find ice-ufrag attribute in media descriptor */
    attr = pjmedia_sdp_attr_find2(sdp_med->attr_count, sdp_med->attr,
				  "ice-ufrag", NULL);
    if (attr == NULL) {
	/* Find ice-ufrag attribute in session descriptor */
	attr = pjmedia_sdp_attr_find2(rem_sdp->attr_count, rem_sdp->attr,
				      "ice-ufrag", NULL);
	if (attr == NULL) {
	    set_no_ice(tp_ice, "ice-ufrag attribute not found");
	    return PJ_SUCCESS;
	}
    }
    uname = attr->value;

    /* Find ice-pwd attribute in media descriptor */
    attr = pjmedia_sdp_attr_find2(sdp_med->attr_count, sdp_med->attr,
				  "ice-pwd", NULL);
    if (attr == NULL) {
	/* Find ice-pwd attribute in session descriptor */
	attr = pjmedia_sdp_attr_find2(rem_sdp->attr_count, rem_sdp->attr,
				      "ice-pwd", NULL);
	if (attr == NULL) {
	    set_no_ice(tp_ice, "ice-pwd attribute not found");
	    return PJ_SUCCESS;
	}
    }
    pass = attr->value;

    /* Get all candidates in the media */
    cand_cnt = 0;
    for (i=0; i<sdp_med->attr_count; ++i) {
	pjmedia_sdp_attr *attr;

	attr = sdp_med->attr[i];

	/* Detect if remote is ICE lite */
	if (pj_strcmp(&attr->name, &STR_ICE_LITE)==0) {
	    remote_is_lite = PJ_TRUE;
	    continue;
	}

	/* Detect if remote has reported ICE mismatch */
	if (pj_strcmp(&attr->name, &STR_ICE_MISMATCH)==0) {
	    ice_mismatch = PJ_TRUE;
	    continue;
	}

	if (pj_strcmp(&attr->name, &STR_CANDIDATE)!=0)
	    continue;

	/* Parse candidate */
	status = parse_cand(pool, &attr->value, &cand[cand_cnt]);
	if (status != PJ_SUCCESS)
	    return status;

	/* Check if this candidate is equal to the connection line */
	if (!conn_found_in_candidate &&
	    pj_memcmp(&conn_addr.ipv4, &cand[cand_cnt].addr.ipv4,
		      sizeof(pj_sockaddr_in))==0)
	{
	    conn_found_in_candidate = PJ_TRUE;
	}

	cand_cnt++;
    }

    /* Handle ice-mismatch case */
    if (ice_mismatch) {
	set_no_ice(tp_ice, "remote reported ice-mismatch");
	return PJ_SUCCESS;
    }

    /* Handle case where SDP connection address is not specified as
     * one of the candidate.
     */
    if (!conn_found_in_candidate) {
	set_no_ice(tp_ice, "local reported ice-mismatch");
	return PJ_SUCCESS;
    }

    /* Mark start time */
    pj_gettimeofday(&tp_ice->start_ice);

    /* If our role was controlled but it turns out that remote is 
     * a lite implementation, change our role to controlling.
     */
    if (remote_is_lite && 
	tp_ice->ice_st->ice->role == PJ_ICE_SESS_ROLE_CONTROLLED)
    {
	pj_ice_sess_change_role(tp_ice->ice_st->ice, 
				PJ_ICE_SESS_ROLE_CONTROLLING);
    }

    /* Start ICE */
    return pj_ice_strans_start_ice(tp_ice->ice_st, &uname, &pass, cand_cnt, cand);
}


PJ_DEF(pj_status_t) pjmedia_ice_stop_ice(pjmedia_transport *tp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    return pj_ice_strans_stop_ice(tp_ice->ice_st);
}


static pj_status_t tp_get_info(pjmedia_transport *tp,
			       pjmedia_sock_info *info)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_ice_strans *ice_st = tp_ice->ice_st;
    pj_ice_strans_comp *comp;

    pj_bzero(info, sizeof(*info));
    info->rtp_sock = info->rtcp_sock = PJ_INVALID_SOCKET;

    /* Retrieve address of default candidate for component 1 (RTP) */
    comp = ice_st->comp[0];
    pj_assert(comp->default_cand >= 0);
    info->rtp_sock = comp->sock;
    pj_memcpy(&info->rtp_addr_name, 
	      &comp->cand_list[comp->default_cand].addr,
	      sizeof(pj_sockaddr_in));

    /* Retrieve address of default candidate for component 12(RTCP) */
    if (ice_st->comp_cnt > 1) {
	comp = ice_st->comp[1];
	pj_assert(comp->default_cand >= 0);
	info->rtp_sock = comp->sock;
	pj_memcpy(&info->rtcp_addr_name, 
		  &comp->cand_list[comp->default_cand].addr,
		  sizeof(pj_sockaddr_in));
    }


    return PJ_SUCCESS;
}


static pj_status_t tp_attach( pjmedia_transport *tp,
			      void *stream,
			      const pj_sockaddr_t *rem_addr,
			      const pj_sockaddr_t *rem_rtcp,
			      unsigned addr_len,
			      void (*rtp_cb)(void*,
					     const void*,
					     pj_ssize_t),
			      void (*rtcp_cb)(void*,
					      const void*,
					      pj_ssize_t))
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    tp_ice->stream = stream;
    tp_ice->rtp_cb = rtp_cb;
    tp_ice->rtcp_cb = rtcp_cb;

    pj_memcpy(&tp_ice->remote_rtp, rem_addr, addr_len);
    pj_memcpy(&tp_ice->remote_rtcp, rem_rtcp, addr_len);

    return PJ_SUCCESS;
}


static void tp_detach(pjmedia_transport *tp,
		      void *strm)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    tp_ice->rtp_cb = NULL;
    tp_ice->rtcp_cb = NULL;
    tp_ice->stream = NULL;

    PJ_UNUSED_ARG(strm);
}


static pj_status_t tp_send_rtp(pjmedia_transport *tp,
			       const void *pkt,
			       pj_size_t size)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    return pj_ice_strans_sendto(tp_ice->ice_st, 1, 
			        pkt, size, &tp_ice->remote_rtp,
				sizeof(pj_sockaddr_in));
}


static pj_status_t tp_send_rtcp(pjmedia_transport *tp,
			        const void *pkt,
			        pj_size_t size)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    if (tp_ice->ice_st->comp_cnt > 1) {
	return pj_ice_strans_sendto(tp_ice->ice_st, 2, 
				    pkt, size, &tp_ice->remote_rtcp,
				    sizeof(pj_sockaddr_in));
    } else {
	return PJ_SUCCESS;
    }
}


static void ice_on_rx_data(pj_ice_strans *ice_st, unsigned comp_id, 
			   void *pkt, pj_size_t size,
			   const pj_sockaddr_t *src_addr,
			   unsigned src_addr_len)
{
    struct transport_ice *tp_ice = (struct transport_ice*) ice_st->user_data;

    if (comp_id==1 && tp_ice->rtp_cb)
	(*tp_ice->rtp_cb)(tp_ice->stream, pkt, size);
    else if (comp_id==2 && tp_ice->rtcp_cb)
	(*tp_ice->rtcp_cb)(tp_ice->stream, pkt, size);

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    PJ_TODO(SWITCH_SOURCE_ADDRESS);
}


static void ice_on_ice_complete(pj_ice_strans *ice_st, 
			        pj_status_t result)
{
    struct transport_ice *tp_ice = (struct transport_ice*) ice_st->user_data;
    pj_time_val end_ice;
    pj_ice_sess_cand *lcand, *rcand;
    pj_ice_sess_check *check;
    char src_addr[32];
    char dst_addr[32];

    pj_gettimeofday(&end_ice);
    PJ_TIME_VAL_SUB(end_ice, tp_ice->start_ice);

    if (result != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(result, errmsg, sizeof(errmsg));
	PJ_LOG(1,(ice_st->obj_name, 
		  "ICE negotiation failed after %d:%03ds: %s", 
		  (int)end_ice.sec, (int)end_ice.msec,
		  errmsg));
    } else {
	check = &ice_st->ice->valid_list.checks[0];
    
	lcand = check->lcand;
	rcand = check->rcand;

	pj_ansi_strcpy(src_addr, pj_inet_ntoa(lcand->addr.ipv4.sin_addr));
	pj_ansi_strcpy(dst_addr, pj_inet_ntoa(rcand->addr.ipv4.sin_addr));

	PJ_LOG(4,(ice_st->obj_name, 
		  "ICE negotiation completed in %d.%03ds. Sending from "
		  "%s:%d to %s:%d",
		  (int)end_ice.sec, (int)end_ice.msec,
		  src_addr, pj_ntohs(lcand->addr.ipv4.sin_port),
		  dst_addr, pj_ntohs(rcand->addr.ipv4.sin_port)));
    }

    /* Notify application */
    if (tp_ice->cb.on_ice_complete)
	(*tp_ice->cb.on_ice_complete)(&tp_ice->base, result);
}



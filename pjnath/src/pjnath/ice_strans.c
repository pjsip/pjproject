/* $Id$ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/ip_helper.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/compat/socket.h>

#define ENABLE_TRACE 0

#if defined(ENABLE_TRACE) && (ENABLE_TRACE != 0)
#  define TRACE_PKT(expr)	    PJ_LOG(5,expr)
#else
#  define TRACE_PKT(expr)
#endif


/* Transport IDs */
enum tp_type
{
    TP_NONE,
    TP_STUN,
    TP_TURN
};


#define CREATE_TP_ID(type, idx)	    (pj_uint8_t)((type << 6) | idx)
#define GET_TP_TYPE(transport_id)   ((transport_id & 0xC0) >> 6)
#define GET_TP_IDX(transport_id)    (transport_id & 0x3F)


/* Candidate's local preference values. This is mostly used to
 * specify preference among candidates with the same type. Since
 * we don't have the facility to specify that, we'll just set it
 * all to the same value.
 */
#if PJNATH_ICE_PRIO_STD
#   define SRFLX_PREF  65535
#   define HOST_PREF   65535
#   define RELAY_PREF  65535
#else
#   define SRFLX_PREF  ((1 << PJ_ICE_LOCAL_PREF_BITS) - 1)
#   define HOST_PREF   ((1 << PJ_ICE_LOCAL_PREF_BITS) - 1)
#   define RELAY_PREF  ((1 << PJ_ICE_LOCAL_PREF_BITS) - 1)
#endif


/* The candidate type preference when STUN candidate is used */
static pj_uint8_t srflx_pref_table[PJ_ICE_CAND_TYPE_MAX] =
{
#if PJNATH_ICE_PRIO_STD
    100,    /**< PJ_ICE_HOST_PREF	    */
    110,    /**< PJ_ICE_SRFLX_PREF	    */
    126,    /**< PJ_ICE_PRFLX_PREF	    */
    0	    /**< PJ_ICE_RELAYED_PREF    */
#else
    /* Keep it to 2 bits */
    1,	/**< PJ_ICE_HOST_PREF	    */
    2,	/**< PJ_ICE_SRFLX_PREF	    */
    3,	/**< PJ_ICE_PRFLX_PREF	    */
    0	/**< PJ_ICE_RELAYED_PREF    */
#endif
};


/* ICE callbacks */
static void	   on_valid_pair(pj_ice_sess *ice);
static void	   on_ice_complete(pj_ice_sess *ice, pj_status_t status);
static pj_status_t ice_tx_pkt(pj_ice_sess *ice,
			      unsigned comp_id,
			      unsigned transport_id,
			      const void *pkt, pj_size_t size,
			      const pj_sockaddr_t *dst_addr,
			      unsigned dst_addr_len);
static void	   ice_rx_data(pj_ice_sess *ice,
			       unsigned comp_id,
			       unsigned transport_id,
			       void *pkt, pj_size_t size,
			       const pj_sockaddr_t *src_addr,
			       unsigned src_addr_len);


/* STUN socket callbacks */
/* Notification when incoming packet has been received. */
static pj_bool_t stun_on_rx_data(pj_stun_sock *stun_sock,
				 void *pkt,
				 unsigned pkt_len,
				 const pj_sockaddr_t *src_addr,
				 unsigned addr_len);
/* Notifification when asynchronous send operation has completed. */
static pj_bool_t stun_on_data_sent(pj_stun_sock *stun_sock,
				   pj_ioqueue_op_key_t *send_key,
				   pj_ssize_t sent);
/* Notification when the status of the STUN transport has changed. */
static pj_bool_t stun_on_status(pj_stun_sock *stun_sock,
				pj_stun_sock_op op,
				pj_status_t status);


/* TURN callbacks */
static void turn_on_rx_data(pj_turn_sock *turn_sock,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static pj_bool_t turn_on_data_sent(pj_turn_sock *turn_sock,
				   pj_ssize_t sent);
static void turn_on_state(pj_turn_sock *turn_sock, pj_turn_state_t old_state,
			  pj_turn_state_t new_state);



/* Forward decls */
static pj_bool_t on_data_sent(pj_ice_strans *ice_st, pj_ssize_t sent);
static void check_pending_send(pj_ice_strans *ice_st);
static void ice_st_on_destroy(void *obj);
static void destroy_ice_st(pj_ice_strans *ice_st);
#define ice_st_perror(ice_st,msg,rc) pjnath_perror(ice_st->obj_name,msg,rc)
static void sess_init_update(pj_ice_strans *ice_st);

/**
 * This structure describes an ICE stream transport component. A component
 * in ICE stream transport typically corresponds to a single socket created
 * for this component, and bound to a specific transport address. This
 * component may have multiple alias addresses, for example one alias
 * address for each interfaces in multi-homed host, another for server
 * reflexive alias, and another for relayed alias. For each transport
 * address alias, an ICE stream transport candidate (#pj_ice_sess_cand) will
 * be created, and these candidates will eventually registered to the ICE
 * session.
 */
typedef struct pj_ice_strans_comp
{
    pj_ice_strans	*ice_st;	/**< ICE stream transport.	*/
    unsigned		 comp_id;	/**< Component ID.		*/

    struct {
	pj_stun_sock	*sock;		/**< STUN transport.		*/
    } stun[PJ_ICE_MAX_STUN];

    struct {
	pj_turn_sock	*sock;		/**< TURN relay transport.	*/
	pj_bool_t	 log_off;	/**< TURN loggin off?		*/
	unsigned	 err_cnt;	/**< TURN disconnected count.	*/
    } turn[PJ_ICE_MAX_TURN];

    pj_bool_t		 creating;	/**< Is creating the candidates?*/
    unsigned		 cand_cnt;	/**< # of candidates/aliaes.	*/
    pj_ice_sess_cand	 cand_list[PJ_ICE_ST_MAX_CAND];	/**< Cand array	*/

    pj_bool_t		 ipv4_mapped;   /**< Is IPv6 addr mapped to IPv4?*/
    pj_sockaddr		 dst_addr;	/**< Destination address	*/
    pj_sockaddr		 synth_addr;	/**< Synthesized dest address	*/
    unsigned 		 synth_addr_len;/**< Synthesized dest addr len  */

    unsigned		 default_cand;	/**< Default candidate.		*/

} pj_ice_strans_comp;


/* Pending send buffer */
typedef struct pending_send
{
    void       	       *buffer;
    unsigned 		comp_id;
    pj_size_t 		data_len;
    pj_sockaddr       	dst_addr;
    int 		dst_addr_len;
} pending_send;

/**
 * This structure represents the ICE stream transport.
 */
struct pj_ice_strans
{
    char		    *obj_name;	/**< Log ID.			*/
    pj_pool_factory	    *pf;	/**< Pool factory.		*/
    pj_pool_t		    *pool;	/**< Pool used by this object.	*/
    void		    *user_data;	/**< Application data.		*/
    pj_ice_strans_cfg	     cfg;	/**< Configuration.		*/
    pj_ice_strans_cb	     cb;	/**< Application callback.	*/
    pj_grp_lock_t	    *grp_lock;  /**< Group lock.		*/

    pj_ice_strans_state	     state;	/**< Session state.		*/
    pj_ice_sess		    *ice;	/**< ICE session.		*/
    pj_ice_sess		    *ice_prev;	/**< Previous ICE session.	*/
    pj_grp_lock_handler	     ice_prev_hndlr; /**< Handler of prev ICE	*/
    pj_time_val		     start_time;/**< Time when ICE was started	*/

    unsigned		     comp_cnt;	/**< Number of components.	*/
    pj_ice_strans_comp	   **comp;	/**< Components array.		*/

    pj_pool_t		    *buf_pool;  /**< Pool for buffers.		*/
    unsigned		     num_buf;	/**< Number of buffers.		*/
    unsigned		     buf_idx;	/**< Index of buffer.		*/
    unsigned		     empty_idx;	/**< Index of empty buffer.	*/
    unsigned		     buf_size;  /**< Buffer size.		*/
    pending_send	    *send_buf;	/**< Send buffers.		*/
    pj_bool_t		     is_pending;/**< Any pending send?		*/

    pj_timer_entry	     ka_timer;	/**< STUN keep-alive timer.	*/

    pj_bool_t		     destroy_req;/**< Destroy has been called?	*/
    pj_bool_t		     cb_called;	/**< Init error callback called?*/
    pj_bool_t		     call_send_cb;/**< Need to call send cb?	*/

    pj_bool_t		     rem_cand_end;/**< Trickle ICE: remote has
					       signalled end of candidate? */
    pj_bool_t		     loc_cand_end;/**< Trickle ICE: local has
					       signalled end of candidate? */
};


/**
 * This structure describe user data for STUN/TURN sockets of the
 * ICE stream transport.
 */
typedef struct sock_user_data
{
    pj_ice_strans_comp	    *comp;
    pj_uint8_t		     transport_id;

} sock_user_data;


/* Validate configuration */
static pj_status_t pj_ice_strans_cfg_check_valid(const pj_ice_strans_cfg *cfg)
{
    pj_status_t status;

    status = pj_stun_config_check_valid(&cfg->stun_cfg);
    if (!status)
	return status;

    return PJ_SUCCESS;
}


/*
 * Initialize ICE transport configuration with default values.
 */
PJ_DEF(void) pj_ice_strans_cfg_default(pj_ice_strans_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->af = pj_AF_INET();
    pj_stun_config_init(&cfg->stun_cfg, NULL, 0, NULL, NULL);
    pj_ice_strans_stun_cfg_default(&cfg->stun);
    pj_ice_strans_turn_cfg_default(&cfg->turn);
    pj_ice_sess_options_default(&cfg->opt);

    cfg->num_send_buf = 4;
}


/*
 * Initialize ICE STUN transport configuration with default values.
 */
PJ_DEF(void) pj_ice_strans_stun_cfg_default(pj_ice_strans_stun_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->af = pj_AF_INET();
    cfg->port = PJ_STUN_PORT;
    cfg->max_host_cands = 64;
    cfg->ignore_stun_error = PJ_FALSE;
    pj_stun_sock_cfg_default(&cfg->cfg);
}


/*
 * Initialize ICE TURN transport configuration with default values.
 */
PJ_DEF(void) pj_ice_strans_turn_cfg_default(pj_ice_strans_turn_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->af = pj_AF_INET();
    cfg->conn_type = PJ_TURN_TP_UDP;
    pj_turn_alloc_param_default(&cfg->alloc_param);
    pj_turn_sock_cfg_default(&cfg->cfg);
}


/*
 * Copy configuration.
 */
PJ_DEF(void) pj_ice_strans_cfg_copy( pj_pool_t *pool,
				     pj_ice_strans_cfg *dst,
				     const pj_ice_strans_cfg *src)
{
    unsigned i;

    pj_memcpy(dst, src, sizeof(*src));

    if (src->stun.server.slen)
	pj_strdup(pool, &dst->stun.server, &src->stun.server);

    for (i = 0; i < src->stun_tp_cnt; ++i) {
	if (src->stun_tp[i].server.slen)
	    pj_strdup(pool, &dst->stun_tp[i].server,
		      &src->stun_tp[i].server);
    }

    if (src->turn.server.slen)
	pj_strdup(pool, &dst->turn.server, &src->turn.server);
    pj_stun_auth_cred_dup(pool, &dst->turn.auth_cred, &src->turn.auth_cred);

    for (i = 0; i < src->turn_tp_cnt; ++i) {
	if (src->turn_tp[i].server.slen)
	    pj_strdup(pool, &dst->turn_tp[i].server,
		      &src->turn_tp[i].server);
	pj_stun_auth_cred_dup(pool, &dst->turn_tp[i].auth_cred,
			      &src->turn_tp[i].auth_cred);
    }
}


/*
 * Add or update TURN candidate.
 */
static pj_status_t add_update_turn(pj_ice_strans *ice_st,
				   pj_ice_strans_comp *comp,
				   unsigned idx,
				   unsigned max_cand_cnt)
{
    pj_ice_sess_cand *cand = NULL;
    pj_ice_strans_turn_cfg *turn_cfg = &ice_st->cfg.turn_tp[idx];
    pj_turn_sock_cfg *sock_cfg  = &turn_cfg->cfg;
    unsigned comp_idx = comp->comp_id - 1;
    pj_turn_sock_cb turn_sock_cb;
    sock_user_data *data;
    unsigned i;
    pj_bool_t new_cand = PJ_FALSE;
    pj_uint8_t tp_id;
    pj_status_t status;

    /* Check if TURN transport is configured */
    if (turn_cfg->server.slen == 0)
	return PJ_SUCCESS;

    /* Find relayed candidate in the component */
    tp_id = CREATE_TP_ID(TP_TURN, idx);
    for (i=0; i<comp->cand_cnt; ++i) {
	if (comp->cand_list[i].transport_id == tp_id) {
	    cand = &comp->cand_list[i];
	    break;
	}
    }

    /* If candidate is found, invalidate it first */
    if (cand) {
	cand->status = PJ_EPENDING;

	/* Also if this component's default candidate is set to relay,
	 * move it temporarily to something else.
	 */
	if ((int)comp->default_cand == cand - comp->cand_list) {
	    /* Init to something */
	    comp->default_cand = 0;
	    /* Use srflx candidate as the default, if any */
	    for (i=0; i<comp->cand_cnt; ++i) {
		if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_SRFLX) {
		    comp->default_cand = i;
		    if (ice_st->cfg.af == pj_AF_UNSPEC() ||
		        comp->cand_list[i].base_addr.addr.sa_family ==
		        ice_st->cfg.af)
		    {
		        break;
		    }
		}
	    }
	}
    }

    /* Init TURN socket */
    pj_bzero(&turn_sock_cb, sizeof(turn_sock_cb));
    turn_sock_cb.on_rx_data = &turn_on_rx_data;
    turn_sock_cb.on_data_sent = &turn_on_data_sent;
    turn_sock_cb.on_state = &turn_on_state;

    /* Override with component specific QoS settings, if any */
    if (ice_st->cfg.comp[comp_idx].qos_type)
	sock_cfg->qos_type = ice_st->cfg.comp[comp_idx].qos_type;
    if (ice_st->cfg.comp[comp_idx].qos_params.flags)
	pj_memcpy(&sock_cfg->qos_params,
		  &ice_st->cfg.comp[comp_idx].qos_params,
		  sizeof(sock_cfg->qos_params));

    /* Override with component specific socket buffer size settings, if any */
    if (ice_st->cfg.comp[comp_idx].so_rcvbuf_size > 0)
	sock_cfg->so_rcvbuf_size = ice_st->cfg.comp[comp_idx].so_rcvbuf_size;
    if (ice_st->cfg.comp[comp_idx].so_sndbuf_size > 0)
	sock_cfg->so_sndbuf_size = ice_st->cfg.comp[comp_idx].so_sndbuf_size;

    /* Add relayed candidate with pending status if there's no existing one */
    if (cand == NULL) {
	PJ_ASSERT_RETURN(max_cand_cnt > 0, PJ_ETOOSMALL);

	cand = &comp->cand_list[comp->cand_cnt];
	cand->type = PJ_ICE_CAND_TYPE_RELAYED;
	cand->status = PJ_EPENDING;
	cand->local_pref = (pj_uint16_t)(RELAY_PREF - idx);
	cand->transport_id = tp_id;
	cand->comp_id = (pj_uint8_t) comp->comp_id;
	new_cand = PJ_TRUE;
    }

    /* Allocate and initialize TURN socket data */
    data = PJ_POOL_ZALLOC_T(ice_st->pool, sock_user_data);
    data->comp = comp;
    data->transport_id = cand->transport_id;

    /* Create the TURN transport */
    status = pj_turn_sock_create(&ice_st->cfg.stun_cfg, turn_cfg->af,
				 turn_cfg->conn_type,
				 &turn_sock_cb, sock_cfg,
				 data, &comp->turn[idx].sock);
    if (status != PJ_SUCCESS) {
	return status;
    }

    if (new_cand) {
	/* Commit the relayed candidate before pj_turn_sock_alloc(), as
	 * otherwise there can be race condition, please check
	 * https://github.com/pjsip/pjproject/pull/2525 for more info.
	 */
	comp->cand_cnt++;
    }

    /* Add pending job */
    ///sess_add_ref(ice_st);

    /* Start allocation */
    status=pj_turn_sock_alloc(comp->turn[idx].sock,
			      &turn_cfg->server,
			      turn_cfg->port,
			      ice_st->cfg.resolver,
			      &turn_cfg->auth_cred,
			      &turn_cfg->alloc_param);
    if (status != PJ_SUCCESS) {
	///sess_dec_ref(ice_st);
	cand->status = status;
	return status;
    }

    PJ_LOG(4,(ice_st->obj_name,
		  "Comp %d/%d: TURN relay candidate (tpid=%d) "
		  "waiting for allocation",
		  comp->comp_id, comp->cand_cnt-1, cand->transport_id));

    return PJ_SUCCESS;
}

static pj_bool_t ice_cand_equals(pj_ice_sess_cand *lcand, 
		    	         pj_ice_sess_cand *rcand)
{
    if (lcand == NULL && rcand == NULL){
        return PJ_TRUE;
    }
    if (lcand == NULL || rcand == NULL){
        return PJ_FALSE;
    }
    
    if (lcand->type != rcand->type
        || lcand->status != rcand->status
        || lcand->comp_id != rcand->comp_id
        || lcand->transport_id != rcand->transport_id
	// local pref is no longer a constant, so it may be different
        //|| lcand->local_pref != rcand->local_pref
        || lcand->prio != rcand->prio
        || pj_sockaddr_cmp(&lcand->addr, &rcand->addr) != 0
        || pj_sockaddr_cmp(&lcand->base_addr, &rcand->base_addr) != 0)
    {
        return PJ_FALSE;
    }
    
    return PJ_TRUE;
}


static pj_status_t add_stun_and_host(pj_ice_strans *ice_st,
				     pj_ice_strans_comp *comp,
				     unsigned idx,
				     unsigned max_cand_cnt)
{
    pj_ice_sess_cand *cand;
    pj_ice_strans_stun_cfg *stun_cfg = &ice_st->cfg.stun_tp[idx];
    pj_stun_sock_cfg *sock_cfg  = &stun_cfg->cfg;
    unsigned comp_idx = comp->comp_id - 1;
    pj_stun_sock_cb stun_sock_cb;
    sock_user_data *data;
    pj_status_t status;

    PJ_ASSERT_RETURN(max_cand_cnt > 0, PJ_ETOOSMALL);

    /* Check if STUN transport or host candidate is configured */
    if (stun_cfg->server.slen == 0 && stun_cfg->max_host_cands == 0)
	return PJ_SUCCESS;

    /* Initialize STUN socket callback */
    pj_bzero(&stun_sock_cb, sizeof(stun_sock_cb));
    stun_sock_cb.on_rx_data = &stun_on_rx_data;
    stun_sock_cb.on_status = &stun_on_status;
    stun_sock_cb.on_data_sent = &stun_on_data_sent;

    /* Override component specific QoS settings, if any */
    if (ice_st->cfg.comp[comp_idx].qos_type) {
	sock_cfg->qos_type = ice_st->cfg.comp[comp_idx].qos_type;
    }
    if (ice_st->cfg.comp[comp_idx].qos_params.flags) {
	pj_memcpy(&sock_cfg->qos_params,
		  &ice_st->cfg.comp[comp_idx].qos_params,
		  sizeof(sock_cfg->qos_params));
    }

    /* Override component specific socket buffer size settings, if any */
    if (ice_st->cfg.comp[comp_idx].so_rcvbuf_size > 0) {
	sock_cfg->so_rcvbuf_size = ice_st->cfg.comp[comp_idx].so_rcvbuf_size;
    }
    if (ice_st->cfg.comp[comp_idx].so_sndbuf_size > 0) {
	sock_cfg->so_sndbuf_size = ice_st->cfg.comp[comp_idx].so_sndbuf_size;
    }

    /* Prepare srflx candidate with pending status. */
    cand = &comp->cand_list[comp->cand_cnt];
    cand->type = PJ_ICE_CAND_TYPE_SRFLX;
    cand->status = PJ_EPENDING;
    cand->local_pref = (pj_uint16_t)(SRFLX_PREF - idx);
    cand->transport_id = CREATE_TP_ID(TP_STUN, idx);
    cand->comp_id = (pj_uint8_t) comp->comp_id;

    /* Allocate and initialize STUN socket data */
    data = PJ_POOL_ZALLOC_T(ice_st->pool, sock_user_data);
    data->comp = comp;
    data->transport_id = cand->transport_id;

    /* Create the STUN transport */
    status = pj_stun_sock_create(&ice_st->cfg.stun_cfg, NULL,
				 stun_cfg->af, &stun_sock_cb,
				 sock_cfg, data, &comp->stun[idx].sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Start STUN Binding resolution and add srflx candidate only if server
     * is set. When any error occur during STUN Binding resolution, let's
     * just skip it and generate host candidates.
     */
    while (stun_cfg->server.slen) {
	pj_stun_sock_info stun_sock_info;

	/* Add pending job */
	///sess_add_ref(ice_st);

	PJ_LOG(4,(ice_st->obj_name,
		  "Comp %d: srflx candidate (tpid=%d) starts "
		  "Binding discovery",
		  comp->comp_id, cand->transport_id));

	pj_log_push_indent();

	/* Start Binding resolution */
	status = pj_stun_sock_start(comp->stun[idx].sock, &stun_cfg->server,
				    stun_cfg->port, ice_st->cfg.resolver);
	if (status != PJ_SUCCESS) {
	    ///sess_dec_ref(ice_st);
	    PJ_PERROR(5,(ice_st->obj_name, status,
			 "Comp %d: srflx candidate (tpid=%d) failed in "
			 "pj_stun_sock_start()",
			 comp->comp_id, cand->transport_id));
	    pj_log_pop_indent();
	    break;
	}

	/* Enumerate addresses */
	status = pj_stun_sock_get_info(comp->stun[idx].sock, &stun_sock_info);
	if (status != PJ_SUCCESS) {
	    ///sess_dec_ref(ice_st);
	    PJ_PERROR(5,(ice_st->obj_name, status,
			 "Comp %d: srflx candidate (tpid=%d) failed in "
			 "pj_stun_sock_get_info()",
			 comp->comp_id, cand->transport_id));
	    pj_log_pop_indent();
	    break;
	}

	/* Update and commit the srflx candidate. */
	pj_sockaddr_cp(&cand->base_addr, &stun_sock_info.aliases[0]);
	pj_sockaddr_cp(&cand->rel_addr, &cand->base_addr);
	pj_ice_calc_foundation(ice_st->pool, &cand->foundation,
			       cand->type, &cand->base_addr);
	comp->cand_cnt++;
	max_cand_cnt--;

	/* Set default candidate to srflx */
	if (comp->cand_list[comp->default_cand].type != PJ_ICE_CAND_TYPE_SRFLX
	    || (ice_st->cfg.af != pj_AF_UNSPEC() &&
	        comp->cand_list[comp->default_cand].base_addr.addr.sa_family
	        != ice_st->cfg.af))
	{
	    comp->default_cand = (unsigned)(cand - comp->cand_list);
	}

	pj_log_pop_indent();

	/* Not really a loop, just trying to avoid complex 'if' blocks */
	break;
    }

    /* Add local addresses to host candidates, unless max_host_cands
     * is set to zero.
     */
    if (stun_cfg->max_host_cands) {
	pj_stun_sock_info stun_sock_info;
	unsigned i, cand_cnt = 0;

	/* Enumerate addresses */
	status = pj_stun_sock_get_info(comp->stun[idx].sock, &stun_sock_info);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(ice_st->obj_name, status,
			 "Failed in querying STUN socket info"));
	    return status;
	}

	for (i = 0; i < stun_sock_info.alias_cnt &&
		    cand_cnt < stun_cfg->max_host_cands; ++i)
	{
	    unsigned j;
	    pj_bool_t cand_duplicate = PJ_FALSE;
	    char addrinfo[PJ_INET6_ADDRSTRLEN+10];
	    const pj_sockaddr *addr = &stun_sock_info.aliases[i];

	    if (max_cand_cnt==0) {
		PJ_LOG(4,(ice_st->obj_name, "Too many host candidates"));
		break;
	    }

	    /* Ignore loopback addresses if cfg->stun.loop_addr is unset */
	    if (stun_cfg->loop_addr==PJ_FALSE) {
		if (stun_cfg->af == pj_AF_INET() && 
		    (pj_ntohl(addr->ipv4.sin_addr.s_addr)>>24)==127)
		{
		    continue;
		}
		else if (stun_cfg->af == pj_AF_INET6()) {
		    pj_in6_addr in6addr = {{{0}}};
		    in6addr.s6_addr[15] = 1;
		    if (pj_memcmp(&in6addr, &addr->ipv6.sin6_addr,
				  sizeof(in6addr))==0)
		    {
			continue;
		    }
		}
	    }

	    /* Ignore IPv6 link-local address, unless it is the default
	     * address (first alias).
	     */
	    if (stun_cfg->af == pj_AF_INET6() && i != 0) {
		const pj_in6_addr *a = &addr->ipv6.sin6_addr;
		if (a->s6_addr[0] == 0xFE && (a->s6_addr[1] & 0xC0) == 0x80)
		    continue;
	    }

	    cand = &comp->cand_list[comp->cand_cnt];

	    cand->type = PJ_ICE_CAND_TYPE_HOST;
	    cand->status = PJ_SUCCESS;
	    cand->local_pref = (pj_uint16_t)(HOST_PREF - cand_cnt);
	    cand->transport_id = CREATE_TP_ID(TP_STUN, idx);
	    cand->comp_id = (pj_uint8_t) comp->comp_id;
	    pj_sockaddr_cp(&cand->addr, addr);
	    pj_sockaddr_cp(&cand->base_addr, addr);
	    pj_bzero(&cand->rel_addr, sizeof(cand->rel_addr));
            
	    /* Check if not already in list */
	    for (j=0; j<comp->cand_cnt; j++) {
		if (ice_cand_equals(cand, &comp->cand_list[j])) {
		    cand_duplicate = PJ_TRUE;
		    break;
		}
	    }

	    if (cand_duplicate) {
		PJ_LOG(4, (ice_st->obj_name,
		       "Comp %d: host candidate %s (tpid=%d) is a duplicate",
		       comp->comp_id, pj_sockaddr_print(&cand->addr, addrinfo,
		       sizeof(addrinfo), 3), cand->transport_id));

		pj_bzero(&cand->addr, sizeof(cand->addr));
		pj_bzero(&cand->base_addr, sizeof(cand->base_addr));
		continue;
	    } else {
		comp->cand_cnt+=1;
		cand_cnt++;
		max_cand_cnt--;
	    }
            
	    pj_ice_calc_foundation(ice_st->pool, &cand->foundation,
				   cand->type, &cand->base_addr);

	    /* Set default candidate with the preferred default
	     * address family
	     */
	    if (comp->ice_st->cfg.af != pj_AF_UNSPEC() &&
	        addr->addr.sa_family == comp->ice_st->cfg.af &&
	        comp->cand_list[comp->default_cand].base_addr.addr.sa_family !=
	        ice_st->cfg.af)
	    {
	        comp->default_cand = (unsigned)(cand - comp->cand_list);
	    }

	    PJ_LOG(4,(ice_st->obj_name,
		      "Comp %d/%d: host candidate %s (tpid=%d) added",
		      comp->comp_id, comp->cand_cnt-1, 
		      pj_sockaddr_print(&cand->addr, addrinfo,
					sizeof(addrinfo), 3),
					cand->transport_id));
	}
    }

    return status;
}


/*
 * Create the component.
 */
static pj_status_t create_comp(pj_ice_strans *ice_st, unsigned comp_id)
{
    pj_ice_strans_comp *comp = NULL;
    unsigned i;
    pj_status_t status;

    /* Verify arguments */
    PJ_ASSERT_RETURN(ice_st && comp_id, PJ_EINVAL);

    /* Check that component ID present */
    PJ_ASSERT_RETURN(comp_id <= ice_st->comp_cnt, PJNATH_EICEINCOMPID);

    /* Create component */
    comp = PJ_POOL_ZALLOC_T(ice_st->pool, pj_ice_strans_comp);
    comp->ice_st = ice_st;
    comp->comp_id = comp_id;
    comp->creating = PJ_TRUE;

    ice_st->comp[comp_id-1] = comp;

    /* Initialize default candidate */
    comp->default_cand = 0;

    /* Create STUN transport if configured */
    for (i=0; i<ice_st->cfg.stun_tp_cnt; ++i) {
	unsigned max_cand_cnt = PJ_ICE_ST_MAX_CAND - comp->cand_cnt -
				ice_st->cfg.turn_tp_cnt;

	status = PJ_ETOOSMALL;

	if ((max_cand_cnt > 0) && (max_cand_cnt <= PJ_ICE_ST_MAX_CAND))
	    status = add_stun_and_host(ice_st, comp, i, max_cand_cnt);

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(3,(ice_st->obj_name, status,
			 "Failed creating STUN transport #%d for comp %d",
			 i, comp->comp_id));
	    //return status;
	}
    }

    /* Create TURN relay if configured. */
    for (i=0; i<ice_st->cfg.turn_tp_cnt; ++i) {
	unsigned max_cand_cnt = PJ_ICE_ST_MAX_CAND - comp->cand_cnt;

	status = PJ_ETOOSMALL;

	if ((max_cand_cnt > 0) && (max_cand_cnt <= PJ_ICE_ST_MAX_CAND))
	    status = add_update_turn(ice_st, comp, i, max_cand_cnt);

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(3,(ice_st->obj_name, status,
			 "Failed creating TURN transport #%d for comp %d",
			 i, comp->comp_id));

	    //return status;
	} else if (max_cand_cnt > 0) {
	    max_cand_cnt = PJ_ICE_ST_MAX_CAND - comp->cand_cnt;
	}
    }

    /* Done creating all the candidates */
    comp->creating = PJ_FALSE;

    /* It's possible that we end up without any candidates */
    if (comp->cand_cnt == 0) {
	PJ_LOG(4,(ice_st->obj_name,
		  "Error: no candidate is created due to settings"));
	return PJ_EINVAL;
    }

    return PJ_SUCCESS;
}

static pj_status_t alloc_send_buf(pj_ice_strans *ice_st, unsigned buf_size)
{
    if (buf_size > ice_st->buf_size) {
        unsigned i;
        
        if (ice_st->is_pending) {
            /* The current buffer is insufficient, but still currently used.*/
            return PJ_EBUSY;
        }

    	pj_pool_safe_release(&ice_st->buf_pool);

    	ice_st->buf_pool = pj_pool_create(ice_st->pf, "ice_buf",
    			       (buf_size + sizeof(pending_send)) *
    			       ice_st->num_buf, 512, NULL);
	if (!ice_st->buf_pool)
	    return PJ_ENOMEM;

	ice_st->buf_size = buf_size;
	ice_st->send_buf = pj_pool_calloc(ice_st->buf_pool, ice_st->num_buf,
			       		  sizeof(pending_send));
	for (i = 0; i < ice_st->num_buf; i++) {
	    ice_st->send_buf[i].buffer = pj_pool_alloc(ice_st->buf_pool,
	    					       buf_size);
	}
	ice_st->buf_idx = ice_st->empty_idx = 0;
    }
    
    return PJ_SUCCESS;
}

/*
 * Create ICE stream transport
 */
PJ_DEF(pj_status_t) pj_ice_strans_create( const char *name,
					  const pj_ice_strans_cfg *cfg,
					  unsigned comp_cnt,
					  void *user_data,
					  const pj_ice_strans_cb *cb,
					  pj_ice_strans **p_ice_st)
{
    pj_pool_t *pool;
    pj_ice_strans *ice_st;
    unsigned i;
    pj_status_t status;

    status = pj_ice_strans_cfg_check_valid(cfg);
    if (status != PJ_SUCCESS)
	return status;

    PJ_ASSERT_RETURN(comp_cnt && cb && p_ice_st &&
		     comp_cnt <= PJ_ICE_MAX_COMP , PJ_EINVAL);

    if (name == NULL)
	name = "ice%p";

    pool = pj_pool_create(cfg->stun_cfg.pf, name, PJNATH_POOL_LEN_ICE_STRANS,
			  PJNATH_POOL_INC_ICE_STRANS, NULL);
    ice_st = PJ_POOL_ZALLOC_T(pool, pj_ice_strans);
    ice_st->pool = pool;
    ice_st->pf = cfg->stun_cfg.pf;
    ice_st->obj_name = pool->obj_name;
    ice_st->user_data = user_data;

    PJ_LOG(4,(ice_st->obj_name,
	      "Creating ICE stream transport with %d component(s)",
	      comp_cnt));
    pj_log_push_indent();

    status = pj_grp_lock_create(pool, NULL, &ice_st->grp_lock);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	pj_log_pop_indent();
	return status;
    }

    /* Allocate send buffer */
    ice_st->num_buf = cfg->num_send_buf;
    status = alloc_send_buf(ice_st, cfg->send_buf_size);
    if (status != PJ_SUCCESS) {
	destroy_ice_st(ice_st);
	pj_log_pop_indent();
	return status;
    }

    pj_grp_lock_add_ref(ice_st->grp_lock);
    pj_grp_lock_add_handler(ice_st->grp_lock, pool, ice_st,
			    &ice_st_on_destroy);

    pj_ice_strans_cfg_copy(pool, &ice_st->cfg, cfg);

    /* To maintain backward compatibility, check if old/deprecated setting is set
     * and the new setting is not, copy the value to the new setting.
     */
    if (cfg->stun_tp_cnt == 0 && 
	(cfg->stun.server.slen || cfg->stun.max_host_cands))
    {
	ice_st->cfg.stun_tp_cnt = 1;
	ice_st->cfg.stun_tp[0] = ice_st->cfg.stun;
    }
    if (cfg->turn_tp_cnt == 0 && cfg->turn.server.slen) {
	ice_st->cfg.turn_tp_cnt = 1;
	ice_st->cfg.turn_tp[0] = ice_st->cfg.turn;
    }

    for (i=0; i<ice_st->cfg.stun_tp_cnt; ++i)
	ice_st->cfg.stun_tp[i].cfg.grp_lock = ice_st->grp_lock;
    for (i=0; i<ice_st->cfg.turn_tp_cnt; ++i)
	ice_st->cfg.turn_tp[i].cfg.grp_lock = ice_st->grp_lock;
    pj_memcpy(&ice_st->cb, cb, sizeof(*cb));

    ice_st->comp_cnt = comp_cnt;
    ice_st->comp = (pj_ice_strans_comp**)
		   pj_pool_calloc(pool, comp_cnt, sizeof(pj_ice_strans_comp*));

    /* Move state to candidate gathering */
    ice_st->state = PJ_ICE_STRANS_STATE_INIT;

    /* Acquire initialization mutex to prevent callback to be
     * called before we finish initialization.
     */
    pj_grp_lock_acquire(ice_st->grp_lock);

    for (i=0; i<comp_cnt; ++i) {
	status = create_comp(ice_st, i+1);
	if (status != PJ_SUCCESS) {
	    pj_grp_lock_release(ice_st->grp_lock);
	    destroy_ice_st(ice_st);
	    pj_log_pop_indent();
	    return status;
	}
    }

    /* Done with initialization */
    pj_grp_lock_release(ice_st->grp_lock);

    PJ_LOG(4,(ice_st->obj_name, "ICE stream transport %p created", ice_st));

    *p_ice_st = ice_st;

    /* Check if all candidates are ready (this may call callback) */
    sess_init_update(ice_st);

    /* If ICE init done, notify app about end of candidate gathering via
     * on_new_candidate() callback.
     */
    if (ice_st->state==PJ_ICE_STRANS_STATE_READY &&
	ice_st->cb.on_new_candidate)
    {
	(*ice_st->cb.on_new_candidate)(ice_st, NULL, PJ_TRUE);
    }

    pj_log_pop_indent();

    return PJ_SUCCESS;
}

/* REALLY destroy ICE */
static void ice_st_on_destroy(void *obj)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)obj;

    /* Destroy any previous ICE session */
    if (ice_st->ice_prev) {
	(*ice_st->ice_prev_hndlr)(ice_st->ice_prev);
	ice_st->ice_prev = NULL;
    }

    PJ_LOG(4,(ice_st->obj_name, "ICE stream transport %p destroyed", obj));

    /* Done */
    pj_pool_safe_release(&ice_st->buf_pool);
    pj_pool_safe_release(&ice_st->pool);
}

/* Destroy ICE */
static void destroy_ice_st(pj_ice_strans *ice_st)
{
    unsigned i;

    PJ_LOG(5,(ice_st->obj_name, "ICE stream transport %p destroy request..",
	      ice_st));
    pj_log_push_indent();

    /* Reset callback and user data */
    pj_bzero(&ice_st->cb, sizeof(ice_st->cb));
    ice_st->user_data = NULL;

    pj_grp_lock_acquire(ice_st->grp_lock);

    if (ice_st->destroy_req) {
	pj_grp_lock_release(ice_st->grp_lock);
	return;
    }

    ice_st->destroy_req = PJ_TRUE;

    /* Destroy ICE if we have ICE */
    if (ice_st->ice) {
	pj_ice_sess *ice = ice_st->ice;
	ice_st->ice = NULL;
	pj_ice_sess_destroy(ice);
    }

    /* Destroy all components */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	if (ice_st->comp[i]) {
	    pj_ice_strans_comp *comp = ice_st->comp[i];
	    unsigned j;
	    for (j = 0; j < ice_st->cfg.stun_tp_cnt; ++j) {
		if (comp->stun[j].sock) {
		    pj_stun_sock_destroy(comp->stun[j].sock);
		    comp->stun[j].sock = NULL;
		}
	    }
	    for (j = 0; j < ice_st->cfg.turn_tp_cnt; ++j) {
		if (comp->turn[j].sock) {
		    pj_turn_sock_destroy(comp->turn[j].sock);
		    comp->turn[j].sock = NULL;
		}
	    }
	}
    }

    pj_grp_lock_dec_ref(ice_st->grp_lock);
    pj_grp_lock_release(ice_st->grp_lock);

    pj_log_pop_indent();
}

/* Get ICE session state. */
PJ_DEF(pj_ice_strans_state) pj_ice_strans_get_state(pj_ice_strans *ice_st)
{
    return ice_st->state;
}

/* State string */
PJ_DEF(const char*) pj_ice_strans_state_name(pj_ice_strans_state state)
{
    const char *names[] = {
	"Null",
	"Candidate Gathering",
	"Candidate Gathering Complete",
	"Session Initialized",
	"Negotiation In Progress",
	"Negotiation Success",
	"Negotiation Failed"
    };

    PJ_ASSERT_RETURN(state <= PJ_ICE_STRANS_STATE_FAILED, "???");
    return names[state];
}

/* Notification about failure */
static void sess_fail(pj_ice_strans *ice_st, pj_ice_strans_op op,
		      const char *title, pj_status_t status)
{
    PJ_PERROR(4,(ice_st->obj_name, status, title));

    pj_log_push_indent();

    if (op==PJ_ICE_STRANS_OP_INIT && ice_st->cb_called) {
	pj_log_pop_indent();
	return;
    }

    ice_st->cb_called = PJ_TRUE;

    if (ice_st->cb.on_ice_complete)
	(*ice_st->cb.on_ice_complete)(ice_st, op, status);

    pj_log_pop_indent();
}

/* Update initialization status */
static void sess_init_update(pj_ice_strans *ice_st)
{
    unsigned i;
    pj_status_t status = PJ_EUNKNOWN;

    /* Ignore if ICE is destroying or init callback has been called */
    if (ice_st->destroy_req || ice_st->cb_called)
	return;

    /* Notify application when all candidates have been gathered */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	unsigned j;
	pj_ice_strans_comp *comp = ice_st->comp[i];

	/* This function can be called when all components or candidates
	 * have not been created.
	 */
	if (!comp || comp->creating) {
	    PJ_LOG(5, (ice_st->obj_name, "ICE init update: creating comp %d",
		       (comp?comp->comp_id:(i+1)) ));
	    return;
	}

	status = PJ_EUNKNOWN;
	for (j=0; j<comp->cand_cnt; ++j) {
	    pj_ice_sess_cand *cand = &comp->cand_list[j];

	    if (cand->status == PJ_EPENDING) {
		PJ_LOG(5, (ice_st->obj_name, "ICE init update: "
			   "comp %d/%d[%s] is pending",
			   comp->comp_id, j,
			   pj_ice_get_cand_type_name(cand->type)));
		return;
	    }
	    
	    if (status == PJ_EUNKNOWN) {
	    	status = cand->status;
	    } else {
	    	/* We only need one successful candidate. */
	    	if (cand->status == PJ_SUCCESS)
	    	    status = PJ_SUCCESS;
	    }
	}
	
	if (status != PJ_SUCCESS)
	    break;
    }

    /* All candidates have been gathered or there's no successful
     * candidate for a component.
     */
    ice_st->cb_called = PJ_TRUE;
    ice_st->state = PJ_ICE_STRANS_STATE_READY;
    if (ice_st->cb.on_ice_complete)
	(*ice_st->cb.on_ice_complete)(ice_st, PJ_ICE_STRANS_OP_INIT,
				      status);

    /* Tell ICE session that trickling is done */
    ice_st->loc_cand_end = PJ_TRUE;
    if (ice_st->ice && ice_st->ice->is_trickling && ice_st->rem_cand_end) {
	pj_ice_sess_update_check_list(ice_st->ice, NULL, NULL, 0, NULL,
				      PJ_TRUE);
    }
}

/*
 * Destroy ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_strans_destroy(pj_ice_strans *ice_st)
{
    destroy_ice_st(ice_st);
    return PJ_SUCCESS;
}


/*
 * Get user data
 */
PJ_DEF(void*) pj_ice_strans_get_user_data(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, NULL);
    return ice_st->user_data;
}


/*
 * Get the value of various options of the ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_strans_get_options( pj_ice_strans *ice_st,
					       pj_ice_sess_options *opt)
{
    PJ_ASSERT_RETURN(ice_st && opt, PJ_EINVAL);
    pj_memcpy(opt, &ice_st->cfg.opt, sizeof(*opt));
    return PJ_SUCCESS;
}

/*
 * Specify various options for this ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_strans_set_options(pj_ice_strans *ice_st,
					      const pj_ice_sess_options *opt)
{
    PJ_ASSERT_RETURN(ice_st && opt, PJ_EINVAL);
    pj_memcpy(&ice_st->cfg.opt, opt, sizeof(*opt));
    if (ice_st->ice)
	pj_ice_sess_set_options(ice_st->ice, &ice_st->cfg.opt);
    return PJ_SUCCESS;
}

/*
 * Update number of components of the ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_strans_update_comp_cnt( pj_ice_strans *ice_st,
						   unsigned comp_cnt)
{
    unsigned i;

    PJ_ASSERT_RETURN(ice_st && comp_cnt < ice_st->comp_cnt, PJ_EINVAL);
    PJ_ASSERT_RETURN(ice_st->ice == NULL, PJ_EINVALIDOP);

    pj_grp_lock_acquire(ice_st->grp_lock);

    for (i=comp_cnt; i<ice_st->comp_cnt; ++i) {
	pj_ice_strans_comp *comp = ice_st->comp[i];
	unsigned j;

	/* Destroy the component */
	for (j = 0; j < ice_st->cfg.stun_tp_cnt; ++j) {
	    if (comp->stun[j].sock) {
		pj_stun_sock_destroy(comp->stun[j].sock);
		comp->stun[j].sock = NULL;
	    }
	}
	for (j = 0; j < ice_st->cfg.turn_tp_cnt; ++j) {
	    if (comp->turn[j].sock) {
		pj_turn_sock_destroy(comp->turn[j].sock);
		comp->turn[j].sock = NULL;
	    }
	}
	comp->cand_cnt = 0;
	ice_st->comp[i] = NULL;
    }
    ice_st->comp_cnt = comp_cnt;
    pj_grp_lock_release(ice_st->grp_lock);

    PJ_LOG(4,(ice_st->obj_name,
	      "Updated ICE stream transport components number to %d",
	      comp_cnt));

    return PJ_SUCCESS;
}

/**
 * Get the group lock for this ICE stream transport.
 */
PJ_DEF(pj_grp_lock_t *) pj_ice_strans_get_grp_lock(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, NULL);
    return ice_st->grp_lock;
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
    //const pj_uint8_t srflx_prio[4] = { 100, 126, 110, 0 };

    /* Check arguments */
    PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);
    /* Must not have ICE */
    PJ_ASSERT_RETURN(ice_st->ice == NULL, PJ_EINVALIDOP);
    /* Components must have been created */
    PJ_ASSERT_RETURN(ice_st->comp[0] != NULL, PJ_EINVALIDOP);

    /* Init callback */
    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_valid_pair   = &on_valid_pair;
    ice_cb.on_ice_complete = &on_ice_complete;
    ice_cb.on_rx_data = &ice_rx_data;
    ice_cb.on_tx_pkt = &ice_tx_pkt;

    /* Release the pool of previous ICE session to avoid memory bloat,
     * as otherwise it will only be released after ICE strans is destroyed
     * (due to group lock).
     */
    if (ice_st->ice_prev) {
	(*ice_st->ice_prev_hndlr)(ice_st->ice_prev);
	ice_st->ice_prev = NULL;
    }

    /* Create! */
    status = pj_ice_sess_create(&ice_st->cfg.stun_cfg, ice_st->obj_name, role,
			        ice_st->comp_cnt, &ice_cb,
			        local_ufrag, local_passwd,
			        ice_st->grp_lock,
			        &ice_st->ice);
    if (status != PJ_SUCCESS)
	return status;

    /* Associate user data */
    ice_st->ice->user_data = (void*)ice_st;

    /* Set options */
    pj_ice_sess_set_options(ice_st->ice, &ice_st->cfg.opt);

    /* If default candidate for components are SRFLX one, upload a custom
     * type priority to ICE session so that SRFLX candidates will get
     * checked first.
     */
    if (ice_st->comp[0]->cand_list[ice_st->comp[0]->default_cand].type
	    == PJ_ICE_CAND_TYPE_SRFLX)
    {
	pj_ice_sess_set_prefs(ice_st->ice, srflx_pref_table);
    }

    /* Add components/candidates */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	unsigned j;
	pj_ice_strans_comp *comp = ice_st->comp[i];

	/* Re-enable logging for Send/Data indications */
	if (ice_st->cfg.turn_tp_cnt) {
	    PJ_LOG(5,(ice_st->obj_name,
		      "Enabling STUN Indication logging for "
		      "component %d", i+1));
	}
	for (j = 0; j < ice_st->cfg.turn_tp_cnt; ++j) {
	    if (comp->turn[j].sock) {
		pj_turn_sock_set_log(comp->turn[j].sock, 0xFFFF);
		comp->turn[j].log_off = PJ_FALSE;
	    }
	}

	for (j=0; j<comp->cand_cnt; ++j) {
	    pj_ice_sess_cand *cand = &comp->cand_list[j];
	    unsigned ice_cand_id;

	    /* Skip if candidate is not ready */
	    if (cand->status != PJ_SUCCESS) {
		PJ_LOG(5,(ice_st->obj_name,
			  "Candidate %d of comp %d is not added (pending)",
			  j, i));
		continue;
	    }

	    /* Must have address */
	    pj_assert(pj_sockaddr_has_addr(&cand->addr));

	    /* Skip if we are mapped to IPv4 address and this candidate
	     * is not IPv4.
	     */
	    if (comp->ipv4_mapped &&
	        cand->addr.addr.sa_family != pj_AF_INET())
	    {
	    	continue;
	    }

	    /* Add the candidate */
	    status = pj_ice_sess_add_cand(ice_st->ice, comp->comp_id,
					  cand->transport_id, cand->type,
					  cand->local_pref,
					  &cand->foundation, &cand->addr,
					  &cand->base_addr,  &cand->rel_addr,
					  pj_sockaddr_get_len(&cand->addr),
					  (unsigned*)&ice_cand_id);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    /* ICE session is ready for negotiation */
    ice_st->state = PJ_ICE_STRANS_STATE_SESS_READY;

    return PJ_SUCCESS;

on_error:
    pj_ice_strans_stop_ice(ice_st);
    return status;
}

/*
 * Check if the ICE stream transport has the ICE session created.
 */
PJ_DEF(pj_bool_t) pj_ice_strans_has_sess(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, PJ_FALSE);
    return ice_st->ice != NULL;
}

/*
 * Check if ICE negotiation is still running.
 */
PJ_DEF(pj_bool_t) pj_ice_strans_sess_is_running(pj_ice_strans *ice_st)
{
    // Trickle ICE can start ICE before remote candidate list is received
    return ice_st && ice_st->ice && /* ice_st->ice->rcand_cnt && */
	   ice_st->ice->clist.state == PJ_ICE_SESS_CHECKLIST_ST_RUNNING &&
	   !pj_ice_strans_sess_is_complete(ice_st);
}


/*
 * Check if ICE negotiation has completed.
 */
PJ_DEF(pj_bool_t) pj_ice_strans_sess_is_complete(pj_ice_strans *ice_st)
{
    return ice_st && ice_st->ice && ice_st->ice->is_complete;
}


/*
 * Get the current/running component count.
 */
PJ_DEF(unsigned) pj_ice_strans_get_running_comp_cnt(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);

    if (ice_st->ice && ice_st->ice->rcand_cnt) {
	return ice_st->ice->comp_cnt;
    } else {
	return ice_st->comp_cnt;
    }
}


/*
 * Get the ICE username fragment and password of the ICE session.
 */
PJ_DEF(pj_status_t) pj_ice_strans_get_ufrag_pwd( pj_ice_strans *ice_st,
						 pj_str_t *loc_ufrag,
						 pj_str_t *loc_pwd,
						 pj_str_t *rem_ufrag,
						 pj_str_t *rem_pwd)
{
    PJ_ASSERT_RETURN(ice_st && ice_st->ice, PJ_EINVALIDOP);

    if (loc_ufrag) *loc_ufrag = ice_st->ice->rx_ufrag;
    if (loc_pwd) *loc_pwd = ice_st->ice->rx_pass;

    if (rem_ufrag || rem_pwd) {
	// In trickle ICE, remote may send initial SDP with empty candidates
	//PJ_ASSERT_RETURN(ice_st->ice->rcand_cnt != 0, PJ_EINVALIDOP);
	if (rem_ufrag) *rem_ufrag = ice_st->ice->tx_ufrag;
	if (rem_pwd) *rem_pwd = ice_st->ice->tx_pass;
    }

    return PJ_SUCCESS;
}

/*
 * Get number of candidates
 */
PJ_DEF(unsigned) pj_ice_strans_get_cands_count(pj_ice_strans *ice_st,
					       unsigned comp_id)
{
    unsigned i, cnt;

    PJ_ASSERT_RETURN(ice_st && ice_st->ice && comp_id &&
		     comp_id <= ice_st->comp_cnt, 0);

    cnt = 0;
    for (i=0; i<ice_st->ice->lcand_cnt; ++i) {
	if (ice_st->ice->lcand[i].comp_id != comp_id)
	    continue;
	++cnt;
    }

    return cnt;
}

/*
 * Enum candidates
 */
PJ_DEF(pj_status_t) pj_ice_strans_enum_cands(pj_ice_strans *ice_st,
					     unsigned comp_id,
					     unsigned *count,
					     pj_ice_sess_cand cand[])
{
    unsigned i, cnt;

    PJ_ASSERT_RETURN(ice_st && ice_st->ice && comp_id &&
		     comp_id <= ice_st->comp_cnt && count && cand, PJ_EINVAL);

    cnt = 0;
    for (i=0; i<ice_st->ice->lcand_cnt && cnt<*count; ++i) {
	if (ice_st->ice->lcand[i].comp_id != comp_id)
	    continue;
	pj_memcpy(&cand[cnt], &ice_st->ice->lcand[i],
		  sizeof(pj_ice_sess_cand));
	++cnt;
    }

    *count = cnt;
    return PJ_SUCCESS;
}

/*
 * Get default candidate.
 */
PJ_DEF(pj_status_t) pj_ice_strans_get_def_cand( pj_ice_strans *ice_st,
						unsigned comp_id,
						pj_ice_sess_cand *cand)
{
    const pj_ice_sess_check *valid_pair;

    PJ_ASSERT_RETURN(ice_st && comp_id && comp_id <= ice_st->comp_cnt &&
		      cand, PJ_EINVAL);

    valid_pair = pj_ice_strans_get_valid_pair(ice_st, comp_id);
    if (valid_pair) {
	pj_memcpy(cand, valid_pair->lcand, sizeof(pj_ice_sess_cand));
    } else {
	pj_ice_strans_comp *comp = ice_st->comp[comp_id - 1];
	pj_assert(comp->default_cand<comp->cand_cnt);
	pj_memcpy(cand, &comp->cand_list[comp->default_cand],
		  sizeof(pj_ice_sess_cand));
    }
    return PJ_SUCCESS;
}

/*
 * Get the current ICE role.
 */
PJ_DEF(pj_ice_sess_role) pj_ice_strans_get_role(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st && ice_st->ice, PJ_ICE_SESS_ROLE_UNKNOWN);
    return ice_st->ice->role;
}

/*
 * Change session role.
 */
PJ_DEF(pj_status_t) pj_ice_strans_change_role( pj_ice_strans *ice_st,
					       pj_ice_sess_role new_role)
{
    PJ_ASSERT_RETURN(ice_st && ice_st->ice, PJ_EINVALIDOP);
    return pj_ice_sess_change_role(ice_st->ice, new_role);
}

static pj_status_t setup_turn_perm( pj_ice_strans *ice_st)
{
    unsigned n;
    pj_status_t status;

    for (n = 0; n < ice_st->cfg.turn_tp_cnt; ++n) {
	unsigned i, comp_cnt;

	comp_cnt = pj_ice_strans_get_running_comp_cnt(ice_st);
	for (i=0; i<comp_cnt; ++i) {
	    pj_ice_strans_comp *comp = ice_st->comp[i];
	    pj_turn_session_info info;
	    pj_sockaddr addrs[PJ_ICE_ST_MAX_CAND];
	    unsigned j, count=0;
	    unsigned rem_cand_cnt;
	    const pj_ice_sess_cand *rem_cand;

	    if (!comp->turn[n].sock)
		continue;

	    status = pj_turn_sock_get_info(comp->turn[n].sock, &info);
	    if (status != PJ_SUCCESS || info.state != PJ_TURN_STATE_READY)
		continue;

	    /* Gather remote addresses for this component */
	    rem_cand_cnt = ice_st->ice->rcand_cnt;
	    rem_cand = ice_st->ice->rcand;
	    if (status != PJ_SUCCESS)
		continue;

	    for (j=0; j<rem_cand_cnt && count<PJ_ARRAY_SIZE(addrs); ++j) {
		if (rem_cand[j].comp_id==i+1 &&
		    rem_cand[j].addr.addr.sa_family==
		    ice_st->cfg.turn_tp[n].af)
		{
		    pj_sockaddr_cp(&addrs[count++], &rem_cand[j].addr);
		}
	    }

	    if (count && !comp->turn[n].err_cnt && comp->turn[n].sock) {
		status = pj_turn_sock_set_perm(
				    comp->turn[n].sock, count,
				    addrs, PJ_ICE_ST_USE_TURN_PERMANENT_PERM);
		if (status != PJ_SUCCESS) {
		    pj_ice_strans_stop_ice(ice_st);
		    return status;
		}
	    }
	}
    }

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

    PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);
    PJ_ASSERT_RETURN(ice_st->ice, PJ_EINVALIDOP);

    /* Mark start time */
    pj_gettimeofday(&ice_st->start_time);

    /* Update check list */
    status = pj_ice_strans_update_check_list(ice_st, rem_ufrag, rem_passwd,
					     rem_cand_cnt, rem_cand,
					     !ice_st->ice->is_trickling);
    if (status != PJ_SUCCESS)
	return status;

    /* If we have TURN candidate, now is the time to create the permissions */
    status = setup_turn_perm(ice_st);
    if (status != PJ_SUCCESS) {
	pj_ice_strans_stop_ice(ice_st);
	return status;
    }

    /* Start ICE negotiation! */
    status = pj_ice_sess_start_check(ice_st->ice);
    if (status != PJ_SUCCESS) {
	pj_ice_strans_stop_ice(ice_st);
	return status;
    }

    ice_st->state = PJ_ICE_STRANS_STATE_NEGO;
    return status;
}


/*
 * Update check list after discovering and conveying new local ICE candidate,
 * or receiving update of remote ICE candidates in trickle ICE.
 */
PJ_DEF(pj_status_t) pj_ice_strans_update_check_list(
					 pj_ice_strans *ice_st,
					 const pj_str_t *rem_ufrag,
					 const pj_str_t *rem_passwd,
					 unsigned rem_cand_cnt,
					 const pj_ice_sess_cand rem_cand[],
					 pj_bool_t rcand_end)
{
    pj_bool_t checklist_created;
    pj_status_t status;

    PJ_ASSERT_RETURN(ice_st && ((rem_cand_cnt==0) ||
			        (rem_ufrag && rem_passwd && rem_cand)),
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(ice_st->ice, PJ_EINVALIDOP);

    pj_grp_lock_acquire(ice_st->grp_lock);

    checklist_created = ice_st->ice->tx_ufrag.slen > 0;

    /* Create checklist (if not yet) */
    if (rem_ufrag && !checklist_created) {
	status = pj_ice_sess_create_check_list(ice_st->ice, rem_ufrag,
					       rem_passwd, rem_cand_cnt,
					       rem_cand);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(ice_st->obj_name, status,
			 "Failed setting up remote ufrag"));
	    pj_grp_lock_release(ice_st->grp_lock);
	    return status;
	}
    }

    /* Update checklist for trickling ICE */
    if (ice_st->ice->is_trickling) {
	if (rcand_end && !ice_st->rem_cand_end)
	    ice_st->rem_cand_end = PJ_TRUE;

	status = pj_ice_sess_update_check_list(
			    ice_st->ice, rem_ufrag, rem_passwd,
			    (checklist_created? rem_cand_cnt:0), rem_cand,
			    (ice_st->rem_cand_end && ice_st->loc_cand_end));
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(ice_st->obj_name, status,
			 "Failed updating checklist"));
	    pj_grp_lock_release(ice_st->grp_lock);
	    return status;
	}
    }

    /* Update TURN permissions if periodic check has been started. */
    if (pj_ice_strans_sess_is_running(ice_st)) {
	status = setup_turn_perm(ice_st);
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(ice_st->obj_name, status,
			 "Failed setting up TURN permission"));
	    pj_grp_lock_release(ice_st->grp_lock);
	    return status;
	}
    }

    pj_grp_lock_release(ice_st->grp_lock);

    return PJ_SUCCESS;
}


/*
 * Get valid pair.
 */
PJ_DEF(const pj_ice_sess_check*)
pj_ice_strans_get_valid_pair(const pj_ice_strans *ice_st,
			     unsigned comp_id)
{
    PJ_ASSERT_RETURN(ice_st && comp_id && comp_id <= ice_st->comp_cnt,
		     NULL);

    if (ice_st->ice == NULL)
	return NULL;

    return ice_st->ice->comp[comp_id-1].valid_check;
}

/*
 * Stop ICE!
 */
PJ_DEF(pj_status_t) pj_ice_strans_stop_ice(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);
    
    /* Protect with group lock, since this may cause race condition with
     * pj_ice_strans_sendto2().
     * See ticket #1877.
     */
    pj_grp_lock_acquire(ice_st->grp_lock);

    if (ice_st->ice) {
	ice_st->ice_prev = ice_st->ice;
	ice_st->ice = NULL;
	pj_ice_sess_detach_grp_lock(ice_st->ice_prev, &ice_st->ice_prev_hndlr);
	pj_ice_sess_destroy(ice_st->ice_prev);
    }

    ice_st->state = PJ_ICE_STRANS_STATE_INIT;

    pj_grp_lock_release(ice_st->grp_lock);

    return PJ_SUCCESS;
}

static pj_status_t use_buffer( pj_ice_strans *ice_st,
			       unsigned comp_id,
			       const void *data,
			       pj_size_t data_len,
			       const pj_sockaddr_t *dst_addr,
			       int dst_addr_len,
			       void **buffer )
{
    unsigned idx;
    pj_status_t status;

    /* Allocate send buffer, if necessary. */
    status = alloc_send_buf(ice_st, (unsigned)data_len);
    if (status != PJ_SUCCESS)
    	return status;
    
    if (ice_st->is_pending && ice_st->empty_idx == ice_st->buf_idx) {
    	/* We don't use buffer or there's no more empty buffer. */
    	return PJ_EBUSY;
    }

    idx = ice_st->empty_idx;
    ice_st->empty_idx = (ice_st->empty_idx + 1) % ice_st->num_buf;
    ice_st->send_buf[idx].comp_id = comp_id;
    ice_st->send_buf[idx].data_len = data_len;
    pj_assert(ice_st->buf_size >= data_len);
    pj_memcpy(ice_st->send_buf[idx].buffer, data, data_len);
    pj_sockaddr_cp(&ice_st->send_buf[idx].dst_addr, dst_addr);
    ice_st->send_buf[idx].dst_addr_len = dst_addr_len;
    *buffer = ice_st->send_buf[idx].buffer;
    
    if (ice_st->is_pending) {
        /* We'll continue later since there's still a pending send. */
    	return PJ_EPENDING;
    }
    
    ice_st->is_pending = PJ_TRUE;
    ice_st->buf_idx = idx;

    return PJ_SUCCESS;
}

/*
 * Application wants to send outgoing packet.
 */
static pj_status_t send_data(pj_ice_strans *ice_st,
			     unsigned comp_id,
		      	     const void *data,
		      	     pj_size_t data_len,
			     const pj_sockaddr_t *dst_addr,
			     int dst_addr_len,
			     pj_bool_t use_buf,
			     pj_bool_t call_cb)
{
    pj_ice_strans_comp *comp;
    pj_ice_sess_cand *def_cand;
    void *buf = (void *)data;
    pj_status_t status;

    PJ_ASSERT_RETURN(ice_st && comp_id && comp_id <= ice_st->comp_cnt &&
		     dst_addr && dst_addr_len, PJ_EINVAL);

    comp = ice_st->comp[comp_id-1];

    /* Check that default candidate for the component exists */
    if (comp->default_cand >= comp->cand_cnt) {
	status = PJ_EINVALIDOP;
	if (call_cb)
    	    on_data_sent(ice_st, -status);
    	return status;
    }

    /* Protect with group lock, since this may cause race condition with
     * pj_ice_strans_stop_ice().
     * See ticket #1877.
     */
    pj_grp_lock_acquire(ice_st->grp_lock);

    if (use_buf && ice_st->num_buf > 0) {
    	status = use_buffer(ice_st, comp_id, data, data_len, dst_addr,
    			    dst_addr_len, &buf);

    	if (status == PJ_EPENDING || status != PJ_SUCCESS) {
    	    pj_grp_lock_release(ice_st->grp_lock);
    	    return status;
    	}
    }

    /* If ICE is available, send data with ICE. If ICE nego is not completed
     * yet, ICE will try to send using any valid candidate pair. For any
     * failure, it will fallback to sending with the default candidate
     * selected during initialization.
     *
     * https://trac.pjsip.org/repos/ticket/1416:
     * Once ICE has failed, also send data with the default candidate.
     */
    if (ice_st->ice && ice_st->state <= PJ_ICE_STRANS_STATE_RUNNING) {
	status = pj_ice_sess_send_data(ice_st->ice, comp_id, buf, data_len);
	if (status == PJ_SUCCESS || status == PJ_EPENDING) {
	    pj_grp_lock_release(ice_st->grp_lock);
	    goto on_return;
	}
    } 

    pj_grp_lock_release(ice_st->grp_lock);

    def_cand = &comp->cand_list[comp->default_cand];
    
    if (def_cand->status == PJ_SUCCESS) {
	unsigned tp_idx = GET_TP_IDX(def_cand->transport_id);

	if (def_cand->type == PJ_ICE_CAND_TYPE_RELAYED) {

	    enum {
		msg_disable_ind = 0xFFFF &
				  ~(PJ_STUN_SESS_LOG_TX_IND|
				    PJ_STUN_SESS_LOG_RX_IND)
	    };

	    /* https://trac.pjsip.org/repos/ticket/1316 */
	    if (comp->turn[tp_idx].sock == NULL) {
		/* TURN socket error */
		status = PJ_EINVALIDOP;
		goto on_return;
	    }

	    if (!comp->turn[tp_idx].log_off) {
		/* Disable logging for Send/Data indications */
		PJ_LOG(5,(ice_st->obj_name,
			  "Disabling STUN Indication logging for "
			  "component %d", comp->comp_id));
		pj_turn_sock_set_log(comp->turn[tp_idx].sock,
				     msg_disable_ind);
		comp->turn[tp_idx].log_off = PJ_TRUE;
	    }

	    status = pj_turn_sock_sendto(comp->turn[tp_idx].sock,
					 (const pj_uint8_t*)buf,
					 (unsigned)data_len,
					 dst_addr, dst_addr_len);
	    goto on_return;
	} else {
    	    const pj_sockaddr_t *dest_addr;
    	    unsigned dest_addr_len;

    	    if (comp->ipv4_mapped) {
    	    	if (comp->synth_addr_len == 0 ||
    	    	    pj_sockaddr_cmp(&comp->dst_addr, dst_addr) != 0)
    	    	{
    	    	    status = pj_sockaddr_synthesize(pj_AF_INET6(),
    	    					    &comp->synth_addr,
    	    					    dst_addr);
    	    	    if (status != PJ_SUCCESS)
    	            	goto on_return;

    	    	    pj_sockaddr_cp(&comp->dst_addr, dst_addr);
    	    	    comp->synth_addr_len = pj_sockaddr_get_len(
    	    	    			       &comp->synth_addr);
    	    	}
	    	dest_addr = &comp->synth_addr;
    	    	dest_addr_len = comp->synth_addr_len;
    	    } else {
    		dest_addr = dst_addr;
    		dest_addr_len = dst_addr_len;
    	    }

	    status = pj_stun_sock_sendto(comp->stun[tp_idx].sock, NULL, buf,
					 (unsigned)data_len, 0, dest_addr,
					 dest_addr_len);
	    goto on_return;
	}

    } else
	status = PJ_EINVALIDOP;

on_return:
    /* We continue later in on_data_sent() callback. */
    if (status == PJ_EPENDING)
    	return status;

    if (call_cb) {
    	on_data_sent(ice_st, (status == PJ_SUCCESS? data_len: -status));
    } else {
    	check_pending_send(ice_st);
    }

    return status;
}


#if !DEPRECATED_FOR_TICKET_2229
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
    pj_status_t status;

    PJ_LOG(1, (ice_st->obj_name, "pj_ice_strans_sendto() is deprecated. "
    				 "Application is recommended to use "
    				 "pj_ice_strans_sendto2() instead."));
    status = send_data(ice_st, comp_id, data, data_len, dst_addr,
    		       dst_addr_len, PJ_TRUE, PJ_FALSE);
    if (status == PJ_EPENDING)
    	status = PJ_SUCCESS;
    
    return status;
}
#endif


/*
 * Application wants to send outgoing packet.
 */
PJ_DEF(pj_status_t) pj_ice_strans_sendto2(pj_ice_strans *ice_st,
					  unsigned comp_id,
					  const void *data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *dst_addr,
					  int dst_addr_len)
{
    ice_st->call_send_cb = PJ_TRUE;
    return send_data(ice_st, comp_id, data, data_len, dst_addr,
    		     dst_addr_len, PJ_TRUE, PJ_FALSE);
}

static void on_valid_pair(pj_ice_sess *ice)
{
    pj_time_val t;
    unsigned msec;
    pj_ice_strans *ice_st = (pj_ice_strans *)ice->user_data;
    pj_ice_strans_cb cb   = ice_st->cb;
    pj_status_t status    = PJ_SUCCESS;

    pj_grp_lock_add_ref(ice_st->grp_lock);

    pj_gettimeofday(&t);
    PJ_TIME_VAL_SUB(t, ice_st->start_time);
    msec = PJ_TIME_VAL_MSEC(t);

    if (cb.on_valid_pair) {
	unsigned i;
	enum {
	    msg_disable_ind = 0xFFFF & ~(PJ_STUN_SESS_LOG_TX_IND |
	                                 PJ_STUN_SESS_LOG_RX_IND)
	};

	PJ_LOG(4,
	       (ice_st->obj_name, "First ICE candidate nominated in %ds:%03d",
	        msec / 1000, msec % 1000));

	for (i = 0; i < ice_st->comp_cnt; ++i) {
	    const pj_ice_sess_check *check;
	    pj_ice_strans_comp *comp = ice_st->comp[i];

	    check = pj_ice_strans_get_valid_pair(ice_st, i + 1);
	    if (check) {
		char lip[PJ_INET6_ADDRSTRLEN + 10];
		char rip[PJ_INET6_ADDRSTRLEN + 10];
		unsigned tp_idx = GET_TP_IDX(check->lcand->transport_id);
		unsigned tp_typ = GET_TP_TYPE(check->lcand->transport_id);

		pj_sockaddr_print(&check->lcand->addr, lip, sizeof(lip), 3);
		pj_sockaddr_print(&check->rcand->addr, rip, sizeof(rip), 3);

		if (tp_typ == TP_TURN) {
		    /* Activate channel binding for the remote address
		     * for more efficient data transfer using TURN.
		     */
		    status = pj_turn_sock_bind_channel(
		            comp->turn[tp_idx].sock, &check->rcand->addr,
		            sizeof(check->rcand->addr));

		    /* Disable logging for Send/Data indications */
		    PJ_LOG(5, (ice_st->obj_name,
		               "Disabling STUN Indication logging for "
		               "component %d",
		               i + 1));
		    pj_turn_sock_set_log(comp->turn[tp_idx].sock,
		                         msg_disable_ind);
		    comp->turn[tp_idx].log_off = PJ_TRUE;
		}

		PJ_LOG(4, (ice_st->obj_name,
		           " Comp %d: "
		           "sending from %s candidate %s to "
		           "%s candidate %s",
		           i + 1, pj_ice_get_cand_type_name(check->lcand->type),
		           lip, pj_ice_get_cand_type_name(check->rcand->type),
		           rip));

	    } else {
		PJ_LOG(4, (ice_st->obj_name, "Comp %d: disabled", i + 1));
	    }
	}

	ice_st->state = (status == PJ_SUCCESS) ? PJ_ICE_STRANS_STATE_RUNNING :
	                                         PJ_ICE_STRANS_STATE_FAILED;

	pj_log_push_indent();
	(*cb.on_valid_pair)(ice_st);
	pj_log_pop_indent();
    }

    pj_grp_lock_dec_ref(ice_st->grp_lock);
}

/*
 * Callback called by ICE session when ICE processing is complete, either
 * successfully or with failure.
 */
static void on_ice_complete(pj_ice_sess *ice, pj_status_t status)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;
    pj_time_val t;
    unsigned msec;
    pj_ice_strans_cb cb = ice_st->cb;

    pj_grp_lock_add_ref(ice_st->grp_lock);

    pj_gettimeofday(&t);
    PJ_TIME_VAL_SUB(t, ice_st->start_time);
    msec = PJ_TIME_VAL_MSEC(t);

    if (cb.on_ice_complete) {
	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(ice_st->obj_name, status,
			 "ICE negotiation failed after %ds:%03d",
			 msec/1000, msec%1000));
	} else {
	    unsigned i;
	    enum {
		msg_disable_ind = 0xFFFF &
				  ~(PJ_STUN_SESS_LOG_TX_IND|
				    PJ_STUN_SESS_LOG_RX_IND)
	    };

	    PJ_LOG(4,(ice_st->obj_name,
		      "ICE negotiation success after %ds:%03d",
		      msec/1000, msec%1000));

	    for (i=0; i<ice_st->comp_cnt; ++i) {
		const pj_ice_sess_check *check;
		pj_ice_strans_comp *comp = ice_st->comp[i];

		check = pj_ice_strans_get_valid_pair(ice_st, i+1);
		if (check) {
		    char lip[PJ_INET6_ADDRSTRLEN+10];
		    char rip[PJ_INET6_ADDRSTRLEN+10];
		    unsigned tp_idx = GET_TP_IDX(check->lcand->transport_id);
		    unsigned tp_typ = GET_TP_TYPE(check->lcand->transport_id);

		    pj_sockaddr_print(&check->lcand->addr, lip,
				      sizeof(lip), 3);
		    pj_sockaddr_print(&check->rcand->addr, rip,
				      sizeof(rip), 3);

		    if (tp_typ == TP_TURN) {
			/* Activate channel binding for the remote address
			 * for more efficient data transfer using TURN.
			 */
			status = pj_turn_sock_bind_channel(
					comp->turn[tp_idx].sock,
					&check->rcand->addr,
					sizeof(check->rcand->addr));

			/* Disable logging for Send/Data indications */
			PJ_LOG(5,(ice_st->obj_name,
				  "Disabling STUN Indication logging for "
				  "component %d", i+1));
			pj_turn_sock_set_log(comp->turn[tp_idx].sock,
					     msg_disable_ind);
			comp->turn[tp_idx].log_off = PJ_TRUE;
		    }

		    PJ_LOG(4,(ice_st->obj_name, " Comp %d: "
			      "sending from %s candidate %s to "
			      "%s candidate %s",
			      i+1,
			      pj_ice_get_cand_type_name(check->lcand->type),
			      lip,
			      pj_ice_get_cand_type_name(check->rcand->type),
			      rip));

		} else {
		    PJ_LOG(4,(ice_st->obj_name,
			      "Comp %d: disabled", i+1));
		}
	    }
	}

	ice_st->state = (status==PJ_SUCCESS) ? PJ_ICE_STRANS_STATE_RUNNING :
					       PJ_ICE_STRANS_STATE_FAILED;

	pj_log_push_indent();
	(*cb.on_ice_complete)(ice_st, PJ_ICE_STRANS_OP_NEGOTIATION, status);
	pj_log_pop_indent();

    }

    pj_grp_lock_dec_ref(ice_st->grp_lock);
}

/*
 * Callback called by ICE session when it wants to send outgoing packet.
 */
static pj_status_t ice_tx_pkt(pj_ice_sess *ice,
			      unsigned comp_id,
			      unsigned transport_id,
			      const void *pkt, pj_size_t size,
			      const pj_sockaddr_t *dst_addr,
			      unsigned dst_addr_len)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;
    pj_ice_strans_comp *comp;
    pj_status_t status;
    void *buf = (void *)pkt;
    pj_bool_t use_buf = PJ_FALSE;
#if defined(ENABLE_TRACE) && (ENABLE_TRACE != 0)
    char daddr[PJ_INET6_ADDRSTRLEN];
#endif
    unsigned tp_idx = GET_TP_IDX(transport_id);
    unsigned tp_typ = GET_TP_TYPE(transport_id);

    PJ_ASSERT_RETURN(comp_id && comp_id <= ice_st->comp_cnt, PJ_EINVAL);

    pj_grp_lock_acquire(ice_st->grp_lock);
    if (ice_st->num_buf > 0 &&
        (!ice_st->send_buf ||
         ice_st->send_buf[ice_st->buf_idx].buffer != pkt))
    {
        use_buf = PJ_TRUE;
    	status = use_buffer(ice_st, comp_id, pkt, size, dst_addr,
    			    dst_addr_len, &buf);
    	if (status == PJ_EPENDING || status != PJ_SUCCESS) {
    	    pj_grp_lock_release(ice_st->grp_lock);
    	    return status;
    	}
    }
    pj_grp_lock_release(ice_st->grp_lock);

    comp = ice_st->comp[comp_id-1];

    TRACE_PKT((comp->ice_st->obj_name,
	       "Component %d TX packet to %s:%d with transport %d",
	       comp_id,
	       pj_sockaddr_print(dst_addr, daddr, sizeof(addr), 2),
	       pj_sockaddr_get_port(dst_addr),
	       tp_typ));

    if (tp_typ == TP_TURN) {
	if (comp->turn[tp_idx].sock) {
	    status = pj_turn_sock_sendto(comp->turn[tp_idx].sock,
					 (const pj_uint8_t*)buf,
					 (unsigned)size,
					 dst_addr, dst_addr_len);
	} else {
	    status = PJ_EINVALIDOP;
	}
    } else if (tp_typ == TP_STUN) {
    	const pj_sockaddr_t *dest_addr;
    	unsigned dest_addr_len;

    	if (comp->ipv4_mapped) {
    	    if (comp->synth_addr_len == 0 ||
    	    	pj_sockaddr_cmp(&comp->dst_addr, dst_addr) != 0)
    	    {
    	    	status = pj_sockaddr_synthesize(pj_AF_INET6(),
    	    					&comp->synth_addr, dst_addr);
    	    	if (status != PJ_SUCCESS) {
    	    	    goto on_return;
    	    	}
    	    
    	    	pj_sockaddr_cp(&comp->dst_addr, dst_addr);
    	    	comp->synth_addr_len = pj_sockaddr_get_len(&comp->synth_addr);
    	    }
	    dest_addr = &comp->synth_addr;
    	    dest_addr_len = comp->synth_addr_len;
    	} else {
    	    dest_addr = dst_addr;
    	    dest_addr_len = dst_addr_len;
    	}

	status = pj_stun_sock_sendto(comp->stun[tp_idx].sock, NULL,
				     buf, (unsigned)size, 0,
				     dest_addr, dest_addr_len);
    } else {
	pj_assert(!"Invalid transport ID");
	status = PJ_EINVALIDOP;
    }

on_return:
    if (use_buf && status != PJ_EPENDING) {
        pj_grp_lock_acquire(ice_st->grp_lock);
    	if (ice_st->num_buf > 0) {
    	    ice_st->buf_idx = (ice_st->buf_idx + 1) % ice_st->num_buf;
    	    pj_assert(ice_st->buf_idx == ice_st->empty_idx);
    	}
    	ice_st->is_pending = PJ_FALSE;
    	pj_grp_lock_release(ice_st->grp_lock);
    }

    return status;
}

/*
 * Callback called by ICE session when it receives application data.
 */
static void ice_rx_data(pj_ice_sess *ice,
		        unsigned comp_id,
			unsigned transport_id,
		        void *pkt, pj_size_t size,
		        const pj_sockaddr_t *src_addr,
		        unsigned src_addr_len)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;

    PJ_UNUSED_ARG(transport_id);

    if (ice_st->cb.on_rx_data) {
	(*ice_st->cb.on_rx_data)(ice_st, comp_id, pkt, size,
				 src_addr, src_addr_len);
    }
}

static void check_pending_send(pj_ice_strans *ice_st)
{
    pj_grp_lock_acquire(ice_st->grp_lock);

    if (ice_st->num_buf > 0)
        ice_st->buf_idx = (ice_st->buf_idx + 1) % ice_st->num_buf;
    
    if (ice_st->num_buf > 0 && ice_st->buf_idx != ice_st->empty_idx) {
	/* There's some pending send. Send it one by one. */
        pending_send *ps = &ice_st->send_buf[ice_st->buf_idx];

	pj_grp_lock_release(ice_st->grp_lock);
    	send_data(ice_st, ps->comp_id, ps->buffer, ps->data_len,
    	    	  &ps->dst_addr, ps->dst_addr_len, PJ_FALSE, PJ_TRUE);
    } else {
    	ice_st->is_pending = PJ_FALSE;
    	pj_grp_lock_release(ice_st->grp_lock);
    }
}

/* Notifification when asynchronous send operation via STUN/TURN
 * has completed.
 */
static pj_bool_t on_data_sent(pj_ice_strans *ice_st, pj_ssize_t sent)
{
    if (ice_st->destroy_req || !ice_st->is_pending)
	return PJ_TRUE;

    if (ice_st->call_send_cb && ice_st->cb.on_data_sent) {
	(*ice_st->cb.on_data_sent)(ice_st, sent);
    }

    check_pending_send(ice_st);

    return PJ_TRUE;
}

/* Notification when incoming packet has been received from
 * the STUN socket.
 */
static pj_bool_t stun_on_rx_data(pj_stun_sock *stun_sock,
				 void *pkt,
				 unsigned pkt_len,
				 const pj_sockaddr_t *src_addr,
				 unsigned addr_len)
{
    sock_user_data *data;
    pj_ice_strans_comp *comp;
    pj_ice_strans *ice_st;
    pj_status_t status;

    data = (sock_user_data*) pj_stun_sock_get_user_data(stun_sock);
    if (data == NULL) {
	/* We have disassociated ourselves from the STUN socket */
	return PJ_FALSE;
    }

    comp = data->comp;
    ice_st = comp->ice_st;

    pj_grp_lock_add_ref(ice_st->grp_lock);

    if (ice_st->ice == NULL) {
	/* The ICE session is gone, but we're still receiving packets.
	 * This could also happen if remote doesn't do ICE. So just
	 * report this to application.
	 */
	if (ice_st->cb.on_rx_data) {
	    (*ice_st->cb.on_rx_data)(ice_st, comp->comp_id, pkt, pkt_len,
				     src_addr, addr_len);
	}

    } else {

	/* Hand over the packet to ICE session */
	status = pj_ice_sess_on_rx_pkt(comp->ice_st->ice, comp->comp_id,
				       data->transport_id,
				       pkt, pkt_len,
				       src_addr, addr_len);

	if (status != PJ_SUCCESS) {
	    ice_st_perror(comp->ice_st, "Error processing packet",
			  status);
	}
    }

    return pj_grp_lock_dec_ref(ice_st->grp_lock) ? PJ_FALSE : PJ_TRUE;
}

/* Notifification when asynchronous send operation to the STUN socket
 * has completed.
 */
static pj_bool_t stun_on_data_sent(pj_stun_sock *stun_sock,
				   pj_ioqueue_op_key_t *send_key,
				   pj_ssize_t sent)
{
    sock_user_data *data;

    PJ_UNUSED_ARG(send_key);

    data = (sock_user_data *)pj_stun_sock_get_user_data(stun_sock);
    if (!data || !data->comp || !data->comp->ice_st) return PJ_TRUE;

    return on_data_sent(data->comp->ice_st, sent);
}

/* Notification when the status of the STUN transport has changed. */
static pj_bool_t stun_on_status(pj_stun_sock *stun_sock,
				pj_stun_sock_op op,
				pj_status_t status)
{
    sock_user_data *data;
    pj_ice_strans_comp *comp;
    pj_ice_strans *ice_st;
    pj_ice_sess_cand *cand = NULL;
    unsigned i;
    int tp_idx;

    pj_assert(status != PJ_EPENDING);

    data = (sock_user_data*) pj_stun_sock_get_user_data(stun_sock);
    comp = data->comp;
    ice_st = comp->ice_st;

    pj_grp_lock_add_ref(ice_st->grp_lock);

    /* Wait until initialization completes */
    pj_grp_lock_acquire(ice_st->grp_lock);

    /* Find the srflx cancidate */
    for (i=0; i<comp->cand_cnt; ++i) {
	if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_SRFLX &&
	    comp->cand_list[i].transport_id == data->transport_id)
	{
	    cand = &comp->cand_list[i];
	    break;
	}
    }

    pj_grp_lock_release(ice_st->grp_lock);

    /* It is possible that we don't have srflx candidate even though this
     * callback is called. This could happen when we cancel adding srflx
     * candidate due to initialization error.
     */
    if (cand == NULL) {
	return pj_grp_lock_dec_ref(ice_st->grp_lock) ? PJ_FALSE : PJ_TRUE;
    }

    tp_idx = GET_TP_IDX(data->transport_id);

    switch (op) {
    case PJ_STUN_SOCK_DNS_OP:
	if (status != PJ_SUCCESS) {
	    /* May not have cand, e.g. when error during init */
	    if (cand)
		cand->status = status;
	    if (!ice_st->cfg.stun_tp[tp_idx].ignore_stun_error) {
		sess_fail(ice_st, PJ_ICE_STRANS_OP_INIT,
		          "DNS resolution failed", status);
	    } else {
		PJ_LOG(4,(ice_st->obj_name,
			  "STUN error is ignored for comp %d",
			  comp->comp_id));
	    }
	}
	break;
    case PJ_STUN_SOCK_BINDING_OP:
    case PJ_STUN_SOCK_MAPPED_ADDR_CHANGE:
	if (status == PJ_SUCCESS) {
	    pj_stun_sock_info info;

	    status = pj_stun_sock_get_info(stun_sock, &info);
	    if (status == PJ_SUCCESS) {
		char ipaddr[PJ_INET6_ADDRSTRLEN+10];
		const char *op_name = (op==PJ_STUN_SOCK_BINDING_OP) ?
				    "Binding discovery complete" :
				    "srflx address changed";
		pj_bool_t dup = PJ_FALSE;
		pj_bool_t init_done;

		if (info.mapped_addr.addr.sa_family == pj_AF_INET() &&
		    cand->base_addr.addr.sa_family == pj_AF_INET6())
		{
		    /* We get an IPv4 mapped address for our IPv6
		     * host address.
		     */		     
		    comp->ipv4_mapped = PJ_TRUE;

		    /* Find other host candidates with the same (IPv6)
		     * address, and replace it with the new (IPv4)
		     * mapped address.
		     */
		    for (i = 0; i < comp->cand_cnt; ++i) {
		        pj_sockaddr *a1, *a2;

		        if (comp->cand_list[i].type != PJ_ICE_CAND_TYPE_HOST)
		            continue;
		        
		        a1 = &comp->cand_list[i].addr;
		        a2 = &cand->base_addr;
		        if (pj_memcmp(pj_sockaddr_get_addr(a1),
		       		      pj_sockaddr_get_addr(a2),
		       		      pj_sockaddr_get_addr_len(a1)) == 0)
		       	{
		       	    pj_uint16_t port = pj_sockaddr_get_port(a1);
		       	    pj_sockaddr_cp(a1, &info.mapped_addr);
		       	    if (port != pj_sockaddr_get_port(a2))
		       	        pj_sockaddr_set_port(a1, port);
		       	    pj_sockaddr_cp(&comp->cand_list[i].base_addr, a1);
		       	}
		    }
		    pj_sockaddr_cp(&cand->base_addr, &info.mapped_addr);
		    pj_sockaddr_cp(&cand->rel_addr, &info.mapped_addr);
		}
		
		/* Eliminate the srflx candidate if the address is
		 * equal to other (host) candidates.
		 */
		for (i=0; i<comp->cand_cnt; ++i) {
		    if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_HOST &&
			pj_sockaddr_cmp(&comp->cand_list[i].addr,
					&info.mapped_addr) == 0)
		    {
			dup = PJ_TRUE;
			break;
		    }
		}

		if (dup) {
		    /* Duplicate found, remove the srflx candidate */
		    unsigned idx = (unsigned)(cand - comp->cand_list);

		    /* Update default candidate index */
		    if (comp->default_cand > idx) {
			--comp->default_cand;
		    } else if (comp->default_cand == idx) {
			comp->default_cand = 0;
		    }

		    /* Remove srflx candidate */
		    pj_array_erase(comp->cand_list, sizeof(comp->cand_list[0]),
				   comp->cand_cnt, idx);
		    --comp->cand_cnt;
		} else {
		    /* Otherwise update the address */
		    pj_sockaddr_cp(&cand->addr, &info.mapped_addr);
		    cand->status = PJ_SUCCESS;

		    /* Add the candidate (for trickle ICE) */
		    if (pj_ice_strans_has_sess(ice_st)) {
			status = pj_ice_sess_add_cand(
					ice_st->ice,
					comp->comp_id,
					cand->transport_id,
					cand->type,
					cand->local_pref,
					&cand->foundation,
					&cand->addr,
					&cand->base_addr,
					&cand->rel_addr,
					pj_sockaddr_get_len(&cand->addr),
					NULL);
		    }
		}

		PJ_LOG(4,(comp->ice_st->obj_name,
			  "Comp %d: %s, "
			  "srflx address is %s",
			  comp->comp_id, op_name,
			  pj_sockaddr_print(&info.mapped_addr, ipaddr,
					     sizeof(ipaddr), 3)));

		sess_init_update(ice_st);

		/* Invoke on_new_candidate() callback */
		init_done = (ice_st->state==PJ_ICE_STRANS_STATE_READY);
		if (op == PJ_STUN_SOCK_BINDING_OP && status == PJ_SUCCESS &&
		    ice_st->cb.on_new_candidate && (!dup || init_done))
		{
		    (*ice_st->cb.on_new_candidate)
					(ice_st, (dup? NULL:cand), init_done);
		}

		if (op == PJ_STUN_SOCK_MAPPED_ADDR_CHANGE &&
		    ice_st->cb.on_ice_complete)
		{
		    (*ice_st->cb.on_ice_complete)(ice_st, 
		    				  PJ_ICE_STRANS_OP_ADDR_CHANGE,
		    				  status);
		}
	    }
	}

	if (status != PJ_SUCCESS) {
	    /* May not have cand, e.g. when error during init */
	    if (cand)
		cand->status = status;
	    if (!ice_st->cfg.stun_tp[tp_idx].ignore_stun_error ||
		comp->cand_cnt==1)
	    {
		sess_fail(ice_st, PJ_ICE_STRANS_OP_INIT,
			  "STUN binding request failed", status);
	    } else {
		pj_bool_t init_done;

		PJ_LOG(4,(ice_st->obj_name,
			  "STUN error is ignored for comp %d",
			  comp->comp_id));

		if (cand) {
		    unsigned idx = (unsigned)(cand - comp->cand_list);

		    /* Update default candidate index */
		    if (comp->default_cand == idx) {
			comp->default_cand = !idx;
		    }
		}

		sess_init_update(ice_st);

		/* Invoke on_new_candidate() callback */
		init_done = (ice_st->state==PJ_ICE_STRANS_STATE_READY);
		if (op == PJ_STUN_SOCK_BINDING_OP &&
		    ice_st->cb.on_new_candidate && init_done)
		{
		    (*ice_st->cb.on_new_candidate) (ice_st, NULL, PJ_TRUE);
		}
	    }
	}
	break;
    case PJ_STUN_SOCK_KEEP_ALIVE_OP:
	if (status != PJ_SUCCESS) {
	    pj_assert(cand != NULL);
	    cand->status = status;
	    if (!ice_st->cfg.stun_tp[tp_idx].ignore_stun_error) {
		sess_fail(ice_st, PJ_ICE_STRANS_OP_INIT,
			  "STUN keep-alive failed", status);
	    } else {
		PJ_LOG(4,(ice_st->obj_name, "STUN error is ignored"));
	    }
	}
	break;
    }

    return pj_grp_lock_dec_ref(ice_st->grp_lock)? PJ_FALSE : PJ_TRUE;
}

/* Callback when TURN socket has received a packet */
static void turn_on_rx_data(pj_turn_sock *turn_sock,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_ice_strans_comp *comp;
    sock_user_data *data;
    pj_status_t status;

    data = (sock_user_data*) pj_turn_sock_get_user_data(turn_sock);
    if (data == NULL) {
	/* We have disassociated ourselves from the TURN socket */
	return;
    }

    comp = data->comp;

    pj_grp_lock_add_ref(comp->ice_st->grp_lock);

    if (comp->ice_st->ice == NULL) {
	/* The ICE session is gone, but we're still receiving packets.
	 * This could also happen if remote doesn't do ICE and application
	 * specifies TURN as the default address in SDP.
	 * So in this case just give the packet to application.
	 */
	if (comp->ice_st->cb.on_rx_data) {
	    (*comp->ice_st->cb.on_rx_data)(comp->ice_st, comp->comp_id, pkt,
					   pkt_len, peer_addr, addr_len);
	}

    } else {

	/* Hand over the packet to ICE */
	status = pj_ice_sess_on_rx_pkt(comp->ice_st->ice, comp->comp_id,
				       data->transport_id, pkt, pkt_len,
				       peer_addr, addr_len);

	if (status != PJ_SUCCESS) {
	    ice_st_perror(comp->ice_st,
			  "Error processing packet from TURN relay",
			  status);
	}
    }

    pj_grp_lock_dec_ref(comp->ice_st->grp_lock);
}

/* Notifification when asynchronous send operation to the TURN socket
 * has completed.
 */
static pj_bool_t turn_on_data_sent(pj_turn_sock *turn_sock,
				   pj_ssize_t sent)
{
    sock_user_data *data;

    data = (sock_user_data *)pj_turn_sock_get_user_data(turn_sock);
    if (!data || !data->comp || !data->comp->ice_st) return PJ_TRUE;

    return on_data_sent(data->comp->ice_st, sent);
}

/* Callback when TURN client state has changed */
static void turn_on_state(pj_turn_sock *turn_sock, pj_turn_state_t old_state,
			  pj_turn_state_t new_state)
{
    pj_ice_strans_comp *comp;
    sock_user_data *data;
    int tp_idx;

    data = (sock_user_data*) pj_turn_sock_get_user_data(turn_sock);
    if (data == NULL) {
	/* Not interested in further state notification once the relay is
	 * disconnecting.
	 */
	return;
    }

    comp = data->comp;
    tp_idx = GET_TP_IDX(data->transport_id);

    PJ_LOG(5,(comp->ice_st->obj_name, "TURN client state changed %s --> %s",
	      pj_turn_state_name(old_state), pj_turn_state_name(new_state)));
    pj_log_push_indent();

    pj_grp_lock_add_ref(comp->ice_st->grp_lock);

    if (new_state == PJ_TURN_STATE_READY) {
	pj_turn_session_info rel_info;
	char ipaddr[PJ_INET6_ADDRSTRLEN+8];
	pj_ice_sess_cand *cand = NULL;
	unsigned i, cand_idx = 0xFF;

	comp->turn[tp_idx].err_cnt = 0;

	/* Get allocation info */
	pj_turn_sock_get_info(turn_sock, &rel_info);

	/* Wait until initialization completes */
	pj_grp_lock_acquire(comp->ice_st->grp_lock);

	/* Find relayed candidate in the component */
	for (i=0; i<comp->cand_cnt; ++i) {
	    if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED &&
		comp->cand_list[i].transport_id == data->transport_id)
	    {
		cand = &comp->cand_list[i];
		cand_idx = i;
		break;
	    }
	}

	pj_grp_lock_release(comp->ice_st->grp_lock);

	if (cand == NULL)
	    goto on_return;

	/* Update candidate */
	pj_sockaddr_cp(&cand->addr, &rel_info.relay_addr);
	pj_sockaddr_cp(&cand->base_addr, &rel_info.relay_addr);
	pj_sockaddr_cp(&cand->rel_addr, &rel_info.mapped_addr);
	pj_ice_calc_foundation(comp->ice_st->pool, &cand->foundation,
			       PJ_ICE_CAND_TYPE_RELAYED,
			       &rel_info.relay_addr);
	cand->status = PJ_SUCCESS;

	/* Set default candidate to relay */
	if (comp->cand_list[comp->default_cand].type!=PJ_ICE_CAND_TYPE_RELAYED
	    || (comp->ice_st->cfg.af != pj_AF_UNSPEC() &&
	        comp->cand_list[comp->default_cand].addr.addr.sa_family
	        != comp->ice_st->cfg.af))
	{
	    comp->default_cand = (unsigned)(cand - comp->cand_list);
	}

	/* Prefer IPv4 relay as default candidate for better connectivity
	 * with IPv4 endpoints.
	 */
	/*
	if (cand->addr.addr.sa_family != pj_AF_INET()) {
	    for (i=0; i<comp->cand_cnt; ++i) {
		if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED &&
		    comp->cand_list[i].addr.addr.sa_family == pj_AF_INET() &&
		    comp->cand_list[i].status == PJ_SUCCESS)
		{
		    comp->default_cand = i;
		    break;
		}
	    }
	}
	*/

	PJ_LOG(4,(comp->ice_st->obj_name,
		  "Comp %d/%d: TURN allocation (tpid=%d) complete, "
		  "relay address is %s",
		  comp->comp_id, cand_idx, cand->transport_id,
		  pj_sockaddr_print(&rel_info.relay_addr, ipaddr,
				     sizeof(ipaddr), 3)));

	/* For trickle ICE, add the candidate to ICE session and setup TURN
	 * permission for remote candidates.
	 */
	if (comp->ice_st->cfg.opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED &&
	    pj_ice_strans_has_sess(comp->ice_st))
	{
	    pj_sockaddr addrs[PJ_ICE_ST_MAX_CAND];
	    pj_ice_sess *sess = comp->ice_st->ice;
	    unsigned j, count=0;
	    pj_status_t status;

	    /* Add the candidate */
	    status = pj_ice_sess_add_cand(comp->ice_st->ice,
					  comp->comp_id,
					  cand->transport_id,
					  cand->type,
					  cand->local_pref,
					  &cand->foundation,
					  &cand->addr,
					  &cand->base_addr, 
					  &cand->rel_addr,
					  pj_sockaddr_get_len(&cand->addr),
					  NULL);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(4,(comp->ice_st->obj_name, status,
			  "Comp %d/%d: failed to add TURN (tpid=%d) to ICE",
			  comp->comp_id, cand_idx, cand->transport_id));
		sess_fail(comp->ice_st, PJ_ICE_STRANS_OP_INIT,
			  "adding TURN candidate failed", status);
	    }

	    /* Gather remote addresses for this component */
	    for (j=0; j<sess->rcand_cnt && count<PJ_ARRAY_SIZE(addrs); ++j) {
		if (sess->rcand[j].addr.addr.sa_family==
		    rel_info.relay_addr.addr.sa_family)
		{
		    pj_sockaddr_cp(&addrs[count++], &sess->rcand[j].addr);
		}
	    }

	    if (count) {
		status = pj_turn_sock_set_perm(turn_sock, count, addrs, 0);
		if (status != PJ_SUCCESS) {
		    PJ_PERROR(4,(comp->ice_st->obj_name, status,
			      "Comp %d/%d: TURN set perm (tpid=%d) failed",
			      comp->comp_id, cand_idx, cand->transport_id));
		    sess_fail(comp->ice_st, PJ_ICE_STRANS_OP_INIT,
			      "TURN set permission failed", status);
		}
	    }
	}

	sess_init_update(comp->ice_st);

	/* Invoke on_new_candidate() callback */
	if (comp->ice_st->cb.on_new_candidate) {
	    (*comp->ice_st->cb.on_new_candidate)
			(comp->ice_st, cand,
			 (comp->ice_st->state==PJ_ICE_STRANS_STATE_READY));
	}

    } else if ((old_state == PJ_TURN_STATE_RESOLVING ||
                old_state == PJ_TURN_STATE_RESOLVED ||
                old_state == PJ_TURN_STATE_ALLOCATING) &&
	       new_state >= PJ_TURN_STATE_DEALLOCATING)
    {
	pj_ice_sess_cand *cand = NULL;
	unsigned i, cand_idx = 0xFF;

	/* DNS resolution or TURN transport creation/allocation
	 * has failed.
	 */
	++comp->turn[tp_idx].err_cnt;

	/* Unregister ourself from the TURN relay */
	pj_turn_sock_set_user_data(turn_sock, NULL);
	comp->turn[tp_idx].sock = NULL;

	/* Wait until initialization completes */
	pj_grp_lock_acquire(comp->ice_st->grp_lock);

	/* Find relayed candidate in the component */
	for (i=0; i<comp->cand_cnt; ++i) {
	    if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED &&
		comp->cand_list[i].transport_id == data->transport_id)
	    {
		cand = &comp->cand_list[i];
		cand_idx = i;
		break;
	    }
	}

	pj_grp_lock_release(comp->ice_st->grp_lock);

	/* If the error happens during pj_turn_sock_create() or
	 * pj_turn_sock_alloc(), the candidate hasn't been added
	 * to the list.
	 */
	if (cand) {
	    pj_turn_session_info info;

	    pj_turn_sock_get_info(turn_sock, &info);
	    cand->status = (old_state == PJ_TURN_STATE_RESOLVING)?
	    		   PJ_ERESOLVE : info.last_status;
	    PJ_LOG(4,(comp->ice_st->obj_name,
		      "Comp %d/%d: TURN error (tpid=%d) during state %s",
		      comp->comp_id, cand_idx, cand->transport_id,
		      pj_turn_state_name(old_state)));
	}

	sess_init_update(comp->ice_st);

	/* Invoke on_new_candidate() callback */
	if (comp->ice_st->cb.on_new_candidate &&
	    comp->ice_st->state==PJ_ICE_STRANS_STATE_READY)
	{
	    (*comp->ice_st->cb.on_new_candidate)(comp->ice_st, NULL, PJ_TRUE);
	}

    } else if (new_state >= PJ_TURN_STATE_DEALLOCATING) {
	pj_turn_session_info info;

	++comp->turn[tp_idx].err_cnt;

	pj_turn_sock_get_info(turn_sock, &info);

	/* Unregister ourself from the TURN relay */
	pj_turn_sock_set_user_data(turn_sock, NULL);
	comp->turn[tp_idx].sock = NULL;

	/* Set session to fail on error. last_status PJ_SUCCESS means normal
	 * deallocation, which should not trigger sess_fail as it may have
	 * been initiated by ICE destroy
	 */
	if (info.last_status != PJ_SUCCESS) {
	    if (comp->ice_st->state < PJ_ICE_STRANS_STATE_READY) {
		sess_fail(comp->ice_st, PJ_ICE_STRANS_OP_INIT,
			  "TURN allocation failed", info.last_status);
	    } else if (comp->turn[tp_idx].err_cnt > 1) {
		sess_fail(comp->ice_st, PJ_ICE_STRANS_OP_KEEP_ALIVE,
			  "TURN refresh failed", info.last_status);
	    } else {
		PJ_PERROR(4,(comp->ice_st->obj_name, info.last_status,
			  "Comp %d: TURN allocation failed, retrying",
			  comp->comp_id));
		add_update_turn(comp->ice_st, comp, tp_idx,
				PJ_ICE_ST_MAX_CAND - comp->cand_cnt);
	    }
	}
    }

on_return:
    pj_grp_lock_dec_ref(comp->ice_st->grp_lock);

    pj_log_pop_indent();
}


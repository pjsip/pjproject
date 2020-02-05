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
#include <pjnath/turn_sock.h>
#include <pj/activesock.h>
#include <pj/ssl_sock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/ioqueue.h>
#include <pj/compat/socket.h>

enum
{
    TIMER_NONE,
    TIMER_DESTROY
};


enum { MAX_BIND_RETRY = 100 };


#define INIT	0x1FFFFFFF

enum {
    DATACONN_STATE_NULL,
    DATACONN_STATE_INITSOCK,
    DATACONN_STATE_CONN_BINDING,
    DATACONN_STATE_READY,
};

/* This structure describe data connection of TURN TCP allocations
 * (RFC 6062).
 */
typedef struct tcp_data_conn_t
{
    pj_pool_t		 *pool;

    pj_uint32_t		 id;		/* Connection ID.		*/
    int			 state;		/* Connection state.		*/
    pj_sockaddr		 peer_addr;	/* Peer address (mapped).	*/
    unsigned		 peer_addr_len;

    pj_activesock_t	*asock;		/* Active socket.		*/
    pj_ioqueue_op_key_t  send_key;

    pj_turn_sock	*turn_sock;	/* TURN socket parent.		*/
} tcp_data_conn_t;


struct pj_turn_sock
{
    pj_pool_t		*pool;
    const char		*obj_name;
    pj_turn_session	*sess;
    pj_turn_sock_cb	 cb;
    void		*user_data;

    pj_bool_t		 is_destroying;
    pj_grp_lock_t	*grp_lock;

    pj_turn_alloc_param	 alloc_param;
    pj_stun_config	 cfg;
    pj_turn_sock_cfg	 setting;

    pj_timer_entry	 timer;

    int			 af;
    pj_turn_tp_type	 conn_type;
    pj_activesock_t	*active_sock;
#if PJ_HAS_SSL_SOCK
    pj_ssl_sock_t       *ssl_sock;
    pj_ssl_cert_t       *cert;
    pj_str_t             server_name;
#endif

    pj_ioqueue_op_key_t	 send_key;
    pj_ioqueue_op_key_t	 int_send_key;
    unsigned 		 pkt_len;
    unsigned 		 body_len;

    /* Data connection, when peer_conn_type==PJ_TURN_TP_TCP (RFC 6062) */
    unsigned		 data_conn_cnt;
    tcp_data_conn_t	 data_conn[PJ_TURN_MAX_TCP_CONN_CNT];
};


/*
 * Callback prototypes.
 */
static pj_status_t turn_on_send_pkt(pj_turn_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len);
static pj_status_t turn_on_stun_send_pkt(pj_turn_session *sess,
				    	 const pj_uint8_t *pkt,
				    	 unsigned pkt_len,
				    	 const pj_sockaddr_t *dst_addr,
				    	 unsigned dst_addr_len);
static void turn_on_channel_bound(pj_turn_session *sess,
				  const pj_sockaddr_t *peer_addr,
				  unsigned addr_len,
				  unsigned ch_num);
static void turn_on_rx_data(pj_turn_session *sess,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static void turn_on_state(pj_turn_session *sess, 
			  pj_turn_state_t old_state,
			  pj_turn_state_t new_state);
static void turn_on_connection_attempt(pj_turn_session *sess,
				       pj_uint32_t conn_id,
				       const pj_sockaddr_t *peer_addr,
				       unsigned addr_len);
static void turn_on_connection_bind_status(pj_turn_session *sess,
					   pj_status_t status,
					   pj_uint32_t conn_id,
					   const pj_sockaddr_t *peer_addr,
					   unsigned addr_len);

static pj_bool_t on_data_read(pj_turn_sock *turn_sock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder);
static pj_bool_t on_data_sent(pj_turn_sock *turn_sock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent);
static pj_bool_t on_connect_complete(pj_turn_sock *turn_sock,
				     pj_status_t status);

/*
 * Activesock callback
 */
static pj_bool_t on_connect_complete_asock(pj_activesock_t *asock,
					   pj_status_t status);
static pj_bool_t on_data_read_asock(pj_activesock_t *asock,
				    void *data,
				    pj_size_t size,
				    pj_status_t status,
				    pj_size_t *remainder);
static pj_bool_t on_data_sent_asock(pj_activesock_t *asock,
			      	     pj_ioqueue_op_key_t *send_key,
			      	     pj_ssize_t sent);

/*
 * SSL sock callback
 */
#if PJ_HAS_SSL_SOCK
static pj_bool_t on_connect_complete_ssl_sock(pj_ssl_sock_t *ssl_sock,
					      pj_status_t status);
static pj_bool_t on_data_read_ssl_sock(pj_ssl_sock_t *ssl_sock,
				       void *data,
				       pj_size_t size,
				       pj_status_t status,
				       pj_size_t *remainder);
#endif

static pj_bool_t dataconn_on_data_read(pj_activesock_t *asock,
				       void *data,
				       pj_size_t size,
				       pj_status_t status,
				       pj_size_t *remainder);
static pj_bool_t dataconn_on_data_sent(pj_activesock_t *asock,
			      	       pj_ioqueue_op_key_t *send_key,
			      	       pj_ssize_t sent);
static pj_bool_t dataconn_on_connect_complete(pj_activesock_t *asock,
					      pj_status_t status);
static void dataconn_cleanup(tcp_data_conn_t *conn);

static void turn_sock_on_destroy(void *comp);
static void destroy(pj_turn_sock *turn_sock);
static void timer_cb(pj_timer_heap_t *th, pj_timer_entry *e);


/* Init config */
PJ_DEF(void) pj_turn_sock_cfg_default(pj_turn_sock_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->max_pkt_size = PJ_TURN_MAX_PKT_LEN;
    cfg->qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    cfg->qos_ignore_error = PJ_TRUE;

#if PJ_HAS_SSL_SOCK
    pj_turn_sock_tls_cfg_default(&cfg->tls_cfg);
#endif
}

#if PJ_HAS_SSL_SOCK

PJ_DEF(void) pj_turn_sock_tls_cfg_default(pj_turn_sock_tls_cfg *tls_cfg)
{
    pj_bzero(tls_cfg, sizeof(*tls_cfg));
    pj_ssl_sock_param_default(&tls_cfg->ssock_param);
    tls_cfg->ssock_param.proto = PJ_TURN_TLS_DEFAULT_PROTO;
}

PJ_DEF(void) pj_turn_sock_tls_cfg_dup(pj_pool_t *pool,
				      pj_turn_sock_tls_cfg *dst,
				      const pj_turn_sock_tls_cfg *src)
{
    pj_memcpy(dst, src, sizeof(*dst));
    pj_strdup_with_null(pool, &dst->ca_list_file, &src->ca_list_file);
    pj_strdup_with_null(pool, &dst->ca_list_path, &src->ca_list_path);
    pj_strdup_with_null(pool, &dst->cert_file, &src->cert_file);
    pj_strdup_with_null(pool, &dst->privkey_file, &src->privkey_file);
    pj_strdup_with_null(pool, &dst->password, &src->password);
    pj_strdup(pool, &dst->ca_buf, &src->ca_buf);
    pj_strdup(pool, &dst->cert_buf, &src->cert_buf);
    pj_strdup(pool, &dst->privkey_buf, &src->privkey_buf);
    pj_ssl_sock_param_copy(pool, &dst->ssock_param, &src->ssock_param);
}

static void wipe_buf(pj_str_t *buf)
{
    volatile char *p = buf->ptr;
    pj_ssize_t len = buf->slen;
    while (len--) *p++ = 0;
    buf->slen = 0;
}

PJ_DEF(void) pj_turn_sock_tls_cfg_wipe_keys(pj_turn_sock_tls_cfg *tls_cfg)
{
    wipe_buf(&tls_cfg->ca_list_file);
    wipe_buf(&tls_cfg->ca_list_path);
    wipe_buf(&tls_cfg->cert_file);
    wipe_buf(&tls_cfg->privkey_file);
    wipe_buf(&tls_cfg->password);
    wipe_buf(&tls_cfg->ca_buf);
    wipe_buf(&tls_cfg->cert_buf);
    wipe_buf(&tls_cfg->privkey_buf); 
}
#endif

/*
 * Create.
 */
PJ_DEF(pj_status_t) pj_turn_sock_create(pj_stun_config *cfg,
					int af,
					pj_turn_tp_type conn_type,
					const pj_turn_sock_cb *cb,
					const pj_turn_sock_cfg *setting,
					void *user_data,
					pj_turn_sock **p_turn_sock)
{
    pj_turn_sock *turn_sock;
    pj_turn_session_cb sess_cb;
    pj_turn_sock_cfg default_setting;
    pj_pool_t *pool;
    const char *name_tmpl;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && p_turn_sock, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==pj_AF_INET() || af==pj_AF_INET6(), PJ_EINVAL);
    PJ_ASSERT_RETURN(conn_type!=PJ_TURN_TP_TCP || PJ_HAS_TCP, PJ_EINVAL);
    PJ_ASSERT_RETURN(conn_type!=PJ_TURN_TP_TLS || PJ_HAS_SSL_SOCK, PJ_EINVAL);

    if (!setting) {
	pj_turn_sock_cfg_default(&default_setting);
	setting = &default_setting;
    }

    switch (conn_type) {
    case PJ_TURN_TP_UDP:
	name_tmpl = "udprel%p";
	break;
    case PJ_TURN_TP_TCP:
	name_tmpl = "tcprel%p";
	break;
#if PJ_HAS_SSL_SOCK
    case PJ_TURN_TP_TLS:
	name_tmpl = "tlsrel%p";
	break;
#endif
    default:
	PJ_ASSERT_RETURN(!"Invalid TURN conn_type", PJ_EINVAL);
	name_tmpl = "tcprel%p";
	break;
    }

    /* Create and init basic data structure */
    pool = pj_pool_create(cfg->pf, name_tmpl, PJNATH_POOL_LEN_TURN_SOCK,
			  PJNATH_POOL_INC_TURN_SOCK, NULL);
    turn_sock = PJ_POOL_ZALLOC_T(pool, pj_turn_sock);
    turn_sock->pool = pool;
    turn_sock->obj_name = pool->obj_name;
    turn_sock->user_data = user_data;
    turn_sock->af = af;
    turn_sock->conn_type = conn_type;

    /* Copy STUN config (this contains ioqueue, timer heap, etc.) */
    pj_memcpy(&turn_sock->cfg, cfg, sizeof(*cfg));

    /* Copy setting (QoS parameters etc */
    pj_memcpy(&turn_sock->setting, setting, sizeof(*setting));
#if PJ_HAS_SSL_SOCK
    pj_turn_sock_tls_cfg_dup(turn_sock->pool, &turn_sock->setting.tls_cfg,
			     &setting->tls_cfg);
#endif

    /* Set callback */
    if (cb) {
	pj_memcpy(&turn_sock->cb, cb, sizeof(*cb));
    }

    /* Session lock */
    if (setting && setting->grp_lock) {
	turn_sock->grp_lock = setting->grp_lock;
    } else {
	status = pj_grp_lock_create(pool, NULL, &turn_sock->grp_lock);
	if (status != PJ_SUCCESS) {
	    pj_pool_release(pool);
	    return status;
	}
    }

    pj_grp_lock_add_ref(turn_sock->grp_lock);
    pj_grp_lock_add_handler(turn_sock->grp_lock, pool, turn_sock,
                            &turn_sock_on_destroy);

    /* Init timer */
    pj_timer_entry_init(&turn_sock->timer, TIMER_NONE, turn_sock, &timer_cb);

    /* Init TURN session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_pkt = &turn_on_send_pkt;
    sess_cb.on_stun_send_pkt = &turn_on_stun_send_pkt;
    sess_cb.on_channel_bound = &turn_on_channel_bound;
    sess_cb.on_rx_data = &turn_on_rx_data;
    sess_cb.on_state = &turn_on_state;
    sess_cb.on_connection_attempt = &turn_on_connection_attempt;
    sess_cb.on_connection_bind_status = &turn_on_connection_bind_status;
    status = pj_turn_session_create(cfg, pool->obj_name, af, conn_type,
                                    turn_sock->grp_lock, &sess_cb, 0,
                                    turn_sock, &turn_sock->sess);
    if (status != PJ_SUCCESS) {
	destroy(turn_sock);
	return status;
    }

    /* Note: socket and ioqueue will be created later once the TURN server
     * has been resolved.
     */

    *p_turn_sock = turn_sock;
    return PJ_SUCCESS;
}

/*
 * Destroy.
 */
static void turn_sock_on_destroy(void *comp)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) comp;

    if (turn_sock->pool) {
	PJ_LOG(4,(turn_sock->obj_name, "TURN socket destroyed"));
	pj_pool_safe_release(&turn_sock->pool);
    }
}

static void destroy(pj_turn_sock *turn_sock)
{
    unsigned i;

    PJ_LOG(4,(turn_sock->obj_name, "TURN socket destroy request, ref_cnt=%d",
	      pj_grp_lock_get_ref(turn_sock->grp_lock)));

    pj_grp_lock_acquire(turn_sock->grp_lock);
    if (turn_sock->is_destroying) {
	pj_grp_lock_release(turn_sock->grp_lock);
	return;
    }

    turn_sock->is_destroying = PJ_TRUE;
    if (turn_sock->sess)
	pj_turn_session_shutdown(turn_sock->sess);
    if (turn_sock->active_sock)
	pj_activesock_close(turn_sock->active_sock);
#if PJ_HAS_SSL_SOCK
    if (turn_sock->ssl_sock)
	pj_ssl_sock_close(turn_sock->ssl_sock);
#endif

    for (i=0; i < PJ_TURN_MAX_TCP_CONN_CNT; ++i) {
	dataconn_cleanup(&turn_sock->data_conn[i]);
    }
    turn_sock->data_conn_cnt = 0;

    pj_grp_lock_dec_ref(turn_sock->grp_lock);
    pj_grp_lock_release(turn_sock->grp_lock);
}

PJ_DEF(void) pj_turn_sock_destroy(pj_turn_sock *turn_sock)
{
    pj_grp_lock_acquire(turn_sock->grp_lock);
    if (turn_sock->is_destroying) {
	pj_grp_lock_release(turn_sock->grp_lock);
	return;
    }

    if (turn_sock->sess) {
	pj_turn_session_shutdown(turn_sock->sess);
	/* This will ultimately call our state callback, and when
	 * session state is DESTROYING we will schedule a timer to
	 * destroy ourselves.
	 */
    } else {
	destroy(turn_sock);
    }

    pj_grp_lock_release(turn_sock->grp_lock);
}


/* Timer callback */
static void timer_cb(pj_timer_heap_t *th, pj_timer_entry *e)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*)e->user_data;
    int eid = e->id;

    PJ_UNUSED_ARG(th);

    e->id = TIMER_NONE;

    switch (eid) {
    case TIMER_DESTROY:
	destroy(turn_sock);
	break;
    default:
	pj_assert(!"Invalid timer id");
	break;
    }
}


/* Display error */
static void show_err(pj_turn_sock *turn_sock, const char *title,
		     pj_status_t status)
{
    PJ_PERROR(4,(turn_sock->obj_name, status, title));
}

/* On error, terminate session */
static void sess_fail(pj_turn_sock *turn_sock, const char *title,
		      pj_status_t status)
{
    show_err(turn_sock, title, status);
    if (turn_sock->sess) {
	pj_turn_session_destroy(turn_sock->sess, status);
    }
}

/*
 * Set user data.
 */
PJ_DEF(pj_status_t) pj_turn_sock_set_user_data( pj_turn_sock *turn_sock,
					       void *user_data)
{
    PJ_ASSERT_RETURN(turn_sock, PJ_EINVAL);
    turn_sock->user_data = user_data;
    return PJ_SUCCESS;
}

/*
 * Get user data.
 */
PJ_DEF(void*) pj_turn_sock_get_user_data(pj_turn_sock *turn_sock)
{
    PJ_ASSERT_RETURN(turn_sock, NULL);
    return turn_sock->user_data;
}

/*
 * Get group lock.
 */
PJ_DEF(pj_grp_lock_t *) pj_turn_sock_get_grp_lock(pj_turn_sock *turn_sock)
{
    PJ_ASSERT_RETURN(turn_sock, NULL);
    return turn_sock->grp_lock;
}

/**
 * Get info.
 */
PJ_DEF(pj_status_t) pj_turn_sock_get_info(pj_turn_sock *turn_sock,
					  pj_turn_session_info *info)
{
    PJ_ASSERT_RETURN(turn_sock && info, PJ_EINVAL);

    if (turn_sock->sess) {
	return pj_turn_session_get_info(turn_sock->sess, info);
    } else {
	pj_bzero(info, sizeof(*info));
	info->state = PJ_TURN_STATE_NULL;
	return PJ_SUCCESS;
    }
}

/**
 * Lock the TURN socket. Application may need to call this function to
 * synchronize access to other objects to avoid deadlock.
 */
PJ_DEF(pj_status_t) pj_turn_sock_lock(pj_turn_sock *turn_sock)
{
    return pj_grp_lock_acquire(turn_sock->grp_lock);
}

/**
 * Unlock the TURN socket.
 */
PJ_DEF(pj_status_t) pj_turn_sock_unlock(pj_turn_sock *turn_sock)
{
    return pj_grp_lock_release(turn_sock->grp_lock);
}

/*
 * Set STUN message logging for this TURN session. 
 */
PJ_DEF(void) pj_turn_sock_set_log( pj_turn_sock *turn_sock,
				   unsigned flags)
{
    pj_turn_session_set_log(turn_sock->sess, flags);
}

/*
 * Set software name
 */
PJ_DEF(pj_status_t) pj_turn_sock_set_software_name( pj_turn_sock *turn_sock,
						    const pj_str_t *sw)
{
    return pj_turn_session_set_software_name(turn_sock->sess, sw);
}

/*
 * Initialize.
 */
PJ_DEF(pj_status_t) pj_turn_sock_alloc(pj_turn_sock *turn_sock,
				       const pj_str_t *domain,
				       int default_port,
				       pj_dns_resolver *resolver,
				       const pj_stun_auth_cred *cred,
				       const pj_turn_alloc_param *param)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(turn_sock && domain, PJ_EINVAL);
    PJ_ASSERT_RETURN(turn_sock->sess, PJ_EINVALIDOP);

    pj_grp_lock_acquire(turn_sock->grp_lock);

    /* Copy alloc param. We will call session_alloc() only after the 
     * server address has been resolved.
     */
    if (param) {
	pj_turn_alloc_param_copy(turn_sock->pool, &turn_sock->alloc_param, param);
    } else {
	pj_turn_alloc_param_default(&turn_sock->alloc_param);
    }

    /* Set credental */
    if (cred) {
	status = pj_turn_session_set_credential(turn_sock->sess, cred);
	if (status != PJ_SUCCESS) {
	    sess_fail(turn_sock, "Error setting credential", status);
	    pj_grp_lock_release(turn_sock->grp_lock);
	    return status;
	}
    }
#if PJ_HAS_SSL_SOCK
    if (turn_sock->conn_type == PJ_TURN_TP_TLS) {
	pj_strdup_with_null(turn_sock->pool, &turn_sock->server_name, domain);
    }
#endif

    /* Resolve server */
    status = pj_turn_session_set_server(turn_sock->sess, domain, default_port,
					resolver);
    if (status != PJ_SUCCESS) {
	sess_fail(turn_sock, "Error setting TURN server", status);
	pj_grp_lock_release(turn_sock->grp_lock);
	return status;
    } else if (!turn_sock->sess) {
	/* TURN session may have been destroyed here, i.e: when DNS resolution
	 * completed synchronously and TURN allocation failed.
	 */
	PJ_LOG(4,(turn_sock->obj_name, "TURN session destroyed in setting "
				       "TURN server"));
	pj_grp_lock_release(turn_sock->grp_lock);
	return PJ_EGONE;
    }

    /* Done for now. The next work will be done when session state moved
     * to RESOLVED state.
     */
    pj_grp_lock_release(turn_sock->grp_lock);
    return PJ_SUCCESS;
}

/*
 * Install permission
 */
PJ_DEF(pj_status_t) pj_turn_sock_set_perm( pj_turn_sock *turn_sock,
					   unsigned addr_cnt,
					   const pj_sockaddr addr[],
					   unsigned options)
{
    if (turn_sock->sess == NULL)
	return PJ_EINVALIDOP;

    return pj_turn_session_set_perm(turn_sock->sess, addr_cnt, addr, options);
}

/*
 * Send packet.
 */ 
PJ_DEF(pj_status_t) pj_turn_sock_sendto( pj_turn_sock *turn_sock,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_sockaddr_t *addr,
					unsigned addr_len)
{
    PJ_ASSERT_RETURN(turn_sock && addr && addr_len, PJ_EINVAL);

    if (turn_sock->sess == NULL)
	return PJ_EINVALIDOP;

    /* TURN session may add some headers to the packet, so we need
     * to store our actual data length to be sent here.
     */
    turn_sock->body_len = pkt_len;
    return pj_turn_session_sendto(turn_sock->sess, pkt, pkt_len, 
				  addr, addr_len);
}

/*
 * Bind a peer address to a channel number.
 */
PJ_DEF(pj_status_t) pj_turn_sock_bind_channel( pj_turn_sock *turn_sock,
					      const pj_sockaddr_t *peer,
					      unsigned addr_len)
{
    PJ_ASSERT_RETURN(turn_sock && peer && addr_len, PJ_EINVAL);
    PJ_ASSERT_RETURN(turn_sock->sess != NULL, PJ_EINVALIDOP);

    return pj_turn_session_bind_channel(turn_sock->sess, peer, addr_len);
}


/*
 * Notification when outgoing TCP socket has been connected.
 */
static pj_bool_t on_connect_complete(pj_turn_sock *turn_sock,
				     pj_status_t status)
{
    pj_grp_lock_acquire(turn_sock->grp_lock);

    /* TURN session may have already been destroyed here.
     * See ticket #1557 (http://trac.pjsip.org/repos/ticket/1557).
     */
    if (!turn_sock->sess) {
	sess_fail(turn_sock, "TURN session already destroyed", status);
	pj_grp_lock_release(turn_sock->grp_lock);
	return PJ_FALSE;
    }

    if (status != PJ_SUCCESS) {
	if (turn_sock->conn_type == PJ_TURN_TP_UDP)
	    sess_fail(turn_sock, "UDP connect() error", status);
	else if (turn_sock->conn_type == PJ_TURN_TP_TCP)
	    sess_fail(turn_sock, "TCP connect() error", status);
	else if (turn_sock->conn_type == PJ_TURN_TP_TLS)
	    sess_fail(turn_sock, "TLS connect() error", status);

	pj_grp_lock_release(turn_sock->grp_lock);
	return PJ_FALSE;
    }

    if (turn_sock->conn_type != PJ_TURN_TP_UDP) {
	PJ_LOG(5, (turn_sock->obj_name, "%s connected",
		   turn_sock->conn_type == PJ_TURN_TP_TCP ? "TCP" : "TLS"));
    }

    /* Kick start pending read operation */
    if (turn_sock->conn_type != PJ_TURN_TP_TLS) 
	status = pj_activesock_start_read(turn_sock->active_sock, 
					  turn_sock->pool,
					  turn_sock->setting.max_pkt_size, 
					  0);
#if PJ_HAS_SSL_SOCK
    else
	status = pj_ssl_sock_start_read(turn_sock->ssl_sock, turn_sock->pool,
					turn_sock->setting.max_pkt_size, 0);
#endif

    /* Init send_key */
    pj_ioqueue_op_key_init(&turn_sock->send_key, sizeof(turn_sock->send_key));
    pj_ioqueue_op_key_init(&turn_sock->int_send_key,
    			   sizeof(turn_sock->int_send_key));

    /* Send Allocate request */
    status = pj_turn_session_alloc(turn_sock->sess, &turn_sock->alloc_param);
    if (status != PJ_SUCCESS) {
	sess_fail(turn_sock, "Error sending ALLOCATE", status);
	pj_grp_lock_release(turn_sock->grp_lock);
	return PJ_FALSE;
    }

    pj_grp_lock_release(turn_sock->grp_lock);
    return PJ_TRUE;
}

static pj_bool_t on_connect_complete_asock(pj_activesock_t *asock,
					   pj_status_t status)
{
    pj_turn_sock *turn_sock;

    turn_sock = (pj_turn_sock*)pj_activesock_get_user_data(asock);
    if (!turn_sock)
	return PJ_FALSE;

    return on_connect_complete(turn_sock, status);
}

#if PJ_HAS_SSL_SOCK
static pj_bool_t on_connect_complete_ssl_sock(pj_ssl_sock_t *ssl_sock,
					      pj_status_t status)
{
    pj_turn_sock *turn_sock;

    turn_sock = (pj_turn_sock*)pj_ssl_sock_get_user_data(ssl_sock);
    if (!turn_sock)
	return PJ_FALSE;

    return on_connect_complete(turn_sock, status);
}
#endif

static pj_uint16_t GETVAL16H(const pj_uint8_t *buf, unsigned pos)
{
    return (pj_uint16_t) ((buf[pos + 0] << 8) | \
			  (buf[pos + 1] << 0));
}

/* Quick check to determine if there is enough packet to process in the
 * incoming buffer. Return the packet length, or zero if there's no packet.
 */
static unsigned has_packet(pj_turn_sock *turn_sock, const void *buf, pj_size_t bufsize)
{
    pj_bool_t is_stun;

    if (turn_sock->conn_type == PJ_TURN_TP_UDP)
	return (unsigned)bufsize;

    /* Quickly check if this is STUN message, by checking the first two bits and
     * size field which must be multiple of 4 bytes
     */
    is_stun = ((((pj_uint8_t*)buf)[0] & 0xC0) == 0) &&
	      ((GETVAL16H((const pj_uint8_t*)buf, 2) & 0x03)==0);

    if (is_stun) {
	pj_size_t msg_len = GETVAL16H((const pj_uint8_t*)buf, 2);
	return (unsigned)((msg_len+20 <= bufsize) ? msg_len+20 : 0);
    } else {
	/* This must be ChannelData. */
	pj_turn_channel_data cd;

	if (bufsize < 4)
	    return 0;

	/* Decode ChannelData packet */
	pj_memcpy(&cd, buf, sizeof(pj_turn_channel_data));
	cd.length = pj_ntohs(cd.length);

	if (bufsize >= cd.length+sizeof(cd)) 
	    return (cd.length+sizeof(cd)+3) & (~3);
	else
	    return 0;
    }
}

/*
 * Notification from ioqueue when incoming UDP packet is received.
 */
static pj_bool_t on_data_read(pj_turn_sock *turn_sock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder)
{
    pj_bool_t ret = PJ_TRUE;

    pj_grp_lock_acquire(turn_sock->grp_lock);

    if (status == PJ_SUCCESS && turn_sock->sess && !turn_sock->is_destroying) {
	/* Report incoming packet to TURN session, repeat while we have
	 * "packet" in the buffer (required for stream-oriented transports)
	 */
	unsigned pkt_len;

	//PJ_LOG(5,(turn_sock->pool->obj_name, 
	//	  "Incoming data, %lu bytes total buffer", size));

	while ((pkt_len=has_packet(turn_sock, data, size)) != 0) {
	    pj_size_t parsed_len;
	    //const pj_uint8_t *pkt = (const pj_uint8_t*)data;

	    //PJ_LOG(5,(turn_sock->pool->obj_name, 
	    //	      "Packet start: %02X %02X %02X %02X", 
	    //	      pkt[0], pkt[1], pkt[2], pkt[3]));

	    //PJ_LOG(5,(turn_sock->pool->obj_name, 
	    //	      "Processing %lu bytes packet of %lu bytes total buffer",
	    //	      pkt_len, size));

	    parsed_len = (unsigned)size;
	    pj_turn_session_on_rx_pkt(turn_sock->sess, data,  size, &parsed_len);

	    /* parsed_len may be zero if we have parsing error, so use our
	     * previous calculation to exhaust the bad packet.
	     */
	    if (parsed_len == 0)
		parsed_len = pkt_len;

	    if (parsed_len < (unsigned)size) {
		*remainder = size - parsed_len;
		pj_memmove(data, ((char*)data)+parsed_len, *remainder);
	    } else {
		*remainder = 0;
	    }
	    size = *remainder;

	    //PJ_LOG(5,(turn_sock->pool->obj_name, 
	    //	      "Buffer size now %lu bytes", size));
	}
    } else if (status != PJ_SUCCESS) {
	if (turn_sock->conn_type == PJ_TURN_TP_UDP)
	    sess_fail(turn_sock, "UDP connection closed", status);
	else if (turn_sock->conn_type == PJ_TURN_TP_TCP)
	    sess_fail(turn_sock, "TCP connection closed", status);
	else if (turn_sock->conn_type == PJ_TURN_TP_TLS)
	    sess_fail(turn_sock, "TLS connection closed", status);

	ret = PJ_FALSE;
	goto on_return;
    }

on_return:
    pj_grp_lock_release(turn_sock->grp_lock);

    return ret;
}

static pj_bool_t on_data_read_asock(pj_activesock_t *asock,
				    void *data,
				    pj_size_t size,
				    pj_status_t status,
				    pj_size_t *remainder)
{
    pj_turn_sock *turn_sock;

    turn_sock = (pj_turn_sock*)pj_activesock_get_user_data(asock);

    return on_data_read(turn_sock, data, size, status, remainder);
}

static pj_bool_t on_data_sent(pj_turn_sock *turn_sock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent)
{
    /* Don't report to callback if this is internal message. */
    if (send_key == &turn_sock->int_send_key) {
	return PJ_TRUE;
    }

    if (turn_sock->cb.on_data_sent) {
	pj_ssize_t header_len, sent_size;

        /* Remove the length of packet header from sent size. */
	header_len = turn_sock->pkt_len - turn_sock->body_len;
	sent_size = (sent > header_len)? (sent - header_len) : 0;
	(*turn_sock->cb.on_data_sent)(turn_sock, sent_size);
    }

    return PJ_TRUE;
}


static pj_bool_t on_data_sent_asock(pj_activesock_t *asock,
			      	     pj_ioqueue_op_key_t *send_key,
			      	     pj_ssize_t sent)
{
    pj_turn_sock *turn_sock;

    turn_sock = (pj_turn_sock*)pj_activesock_get_user_data(asock);

    return on_data_sent(turn_sock, send_key, sent);
}


#if PJ_HAS_SSL_SOCK
static pj_bool_t on_data_read_ssl_sock(pj_ssl_sock_t *ssl_sock,
				       void *data,
				       pj_size_t size,
				       pj_status_t status,
				       pj_size_t *remainder)
{
    pj_turn_sock *turn_sock;

    turn_sock = (pj_turn_sock*)pj_ssl_sock_get_user_data(ssl_sock);

    return on_data_read(turn_sock, data, size, status, remainder);
}

static pj_bool_t on_data_sent_ssl_sock(pj_ssl_sock_t *ssl_sock,
				       pj_ioqueue_op_key_t *op_key,
				       pj_ssize_t bytes_sent)
{
    pj_turn_sock *turn_sock;

    PJ_UNUSED_ARG(op_key);

    turn_sock = (pj_turn_sock*)pj_ssl_sock_get_user_data(ssl_sock);

    /* Check for error/closure */
    if (bytes_sent <= 0) {
	pj_status_t status;

	status = (bytes_sent == 0) ? PJ_RETURN_OS_ERROR(OSERR_ENOTCONN) :
	    (pj_status_t)-bytes_sent;

	sess_fail(turn_sock, "TLS send() error", status);

	return PJ_FALSE;
    }

    return on_data_sent(turn_sock, op_key, bytes_sent);
}
#endif


static pj_status_t send_pkt(pj_turn_session *sess,
			    pj_bool_t internal,
			    const pj_uint8_t *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *dst_addr,
			    unsigned dst_addr_len)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) 
			      pj_turn_session_get_user_data(sess);
    pj_ssize_t len = pkt_len;
    pj_status_t status = PJ_SUCCESS;
    pj_ioqueue_op_key_t *send_key = &turn_sock->send_key;

    if (turn_sock == NULL || turn_sock->is_destroying) {
	/* We've been destroyed */
	// https://trac.pjsip.org/repos/ticket/1316
	//pj_assert(!"We should shutdown gracefully");
	return PJ_EINVALIDOP;
    }

    if (internal)
    	send_key = &turn_sock->int_send_key;
    turn_sock->pkt_len = pkt_len;

    if (turn_sock->conn_type == PJ_TURN_TP_UDP) {
	status = pj_activesock_sendto(turn_sock->active_sock,
				      send_key, pkt, &len, 0,
				      dst_addr, dst_addr_len);
    } else if (turn_sock->alloc_param.peer_conn_type == PJ_TURN_TP_TCP) {
	pj_turn_session_info info;
	pj_turn_session_get_info(turn_sock->sess, &info);
	if (pj_sockaddr_cmp(&info.server, dst_addr) == 0) {
	    /* Destination address is TURN server */
	    status = pj_activesock_send(turn_sock->active_sock,
					send_key, pkt, &len, 0);
	} else {
	    /* Destination address is peer, lookup data connection */
	    unsigned i;

	    status = PJ_ENOTFOUND;
	    for (i=0; i < PJ_TURN_MAX_TCP_CONN_CNT; ++i) {
		tcp_data_conn_t *conn = &turn_sock->data_conn[i];
		if (conn->state < DATACONN_STATE_CONN_BINDING)
		    continue;
		if (pj_sockaddr_cmp(&conn->peer_addr, dst_addr) == 0) {
		    status = pj_activesock_send(conn->asock,
						&conn->send_key,
						pkt, &len, 0);
		    break;
		}
	    }
	}
    } else  if (turn_sock->conn_type == PJ_TURN_TP_TCP) {
	status = pj_activesock_send(turn_sock->active_sock,
				    send_key, pkt, &len, 0);
    }
#if PJ_HAS_SSL_SOCK
    else if (turn_sock->conn_type == PJ_TURN_TP_TLS) {
	status = pj_ssl_sock_send(turn_sock->ssl_sock,
				  send_key, pkt, &len, 0);
    }
#endif
    else {
	PJ_ASSERT_RETURN(!"Invalid TURN conn_type", PJ_EINVAL);
    }

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	show_err(turn_sock, "socket send()", status);
    }

    return status;
}


/*
 * Callback from TURN session to send outgoing packet.
 */
static pj_status_t turn_on_send_pkt(pj_turn_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len)
{
    return send_pkt(sess, PJ_FALSE, pkt, pkt_len,
    		    dst_addr, dst_addr_len);
}

static pj_status_t turn_on_stun_send_pkt(pj_turn_session *sess,
				    	 const pj_uint8_t *pkt,
				    	 unsigned pkt_len,
				    	 const pj_sockaddr_t *dst_addr,
				    	 unsigned dst_addr_len)
{
    return send_pkt(sess, PJ_TRUE, pkt, pkt_len,
    		    dst_addr, dst_addr_len);
}


/*
 * Callback from TURN session when a channel is successfully bound.
 */
static void turn_on_channel_bound(pj_turn_session *sess,
				  const pj_sockaddr_t *peer_addr,
				  unsigned addr_len,
				  unsigned ch_num)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(peer_addr);
    PJ_UNUSED_ARG(addr_len);
    PJ_UNUSED_ARG(ch_num);
}


/*
 * Callback from TURN session upon incoming data.
 */
static void turn_on_rx_data(pj_turn_session *sess,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) 
			   pj_turn_session_get_user_data(sess);
    if (turn_sock == NULL || turn_sock->is_destroying) {
	/* We've been destroyed */
	return;
    }

    if (turn_sock->alloc_param.peer_conn_type != PJ_TURN_TP_UDP) {
	/* Data traffic for RFC 6062 is not via TURN session */
	return;
    }

    if (turn_sock->cb.on_rx_data) {
	(*turn_sock->cb.on_rx_data)(turn_sock, pkt, pkt_len, 
				  peer_addr, addr_len);
    }
}


/*
 * Callback from TURN session when state has changed
 */
static void turn_on_state(pj_turn_session *sess, 
			  pj_turn_state_t old_state,
			  pj_turn_state_t new_state)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) 
			   pj_turn_session_get_user_data(sess);
    pj_status_t status = PJ_SUCCESS;

    if (turn_sock == NULL) {
	/* We've been destroyed */
	return;
    }

    /* Notify app first */
    if (turn_sock->cb.on_state) {
	(*turn_sock->cb.on_state)(turn_sock, old_state, new_state);
    }

    /* Make sure user hasn't destroyed us in the callback */
    if (turn_sock->sess && new_state == PJ_TURN_STATE_RESOLVED) {
	pj_turn_session_info info;
	pj_turn_session_get_info(turn_sock->sess, &info);
	new_state = info.state;
    }

    if (turn_sock->sess && new_state == PJ_TURN_STATE_RESOLVED) {
	/*
	 * Once server has been resolved, initiate outgoing TCP
	 * connection to the server.
	 */
	pj_turn_session_info info;
	char addrtxt[PJ_INET6_ADDRSTRLEN+8];
	int sock_type;
	pj_sock_t sock;
	pj_activesock_cfg asock_cfg;
	pj_activesock_cb asock_cb;
	pj_sockaddr bound_addr, *cfg_bind_addr;
	pj_uint16_t max_bind_retry;

	/* Close existing connection, if any. This happens when
	 * we're switching to alternate TURN server when either TCP
	 * connection or ALLOCATE request failed.
	 */
	if ((turn_sock->conn_type != PJ_TURN_TP_TLS) && 
	    (turn_sock->active_sock)) 
	{
	    pj_activesock_close(turn_sock->active_sock);
	    turn_sock->active_sock = NULL;
	}
#if PJ_HAS_SSL_SOCK
	else if ((turn_sock->conn_type == PJ_TURN_TP_TLS) &&
	    (turn_sock->ssl_sock))
	{
	    pj_ssl_sock_close(turn_sock->ssl_sock);
	    turn_sock->ssl_sock = NULL;
	}
#endif
	/* Get server address from session info */
	pj_turn_session_get_info(sess, &info);

	if (turn_sock->conn_type == PJ_TURN_TP_UDP)
	    sock_type = pj_SOCK_DGRAM();
	else
	    sock_type = pj_SOCK_STREAM();

	cfg_bind_addr = &turn_sock->setting.bound_addr;
	max_bind_retry = MAX_BIND_RETRY;
	if (turn_sock->setting.port_range &&
	    turn_sock->setting.port_range < max_bind_retry)
	{
	    max_bind_retry = turn_sock->setting.port_range;
	}
	pj_sockaddr_init(turn_sock->af, &bound_addr, NULL, 0);
	if (cfg_bind_addr->addr.sa_family == pj_AF_INET() || 
	    cfg_bind_addr->addr.sa_family == pj_AF_INET6())
	{
	    pj_sockaddr_cp(&bound_addr, cfg_bind_addr);
	}

	if (turn_sock->conn_type != PJ_TURN_TP_TLS) {
	    /* Init socket */
	    status = pj_sock_socket(turn_sock->af, sock_type, 0, &sock);
	    if (status != PJ_SUCCESS) {
		pj_turn_sock_destroy(turn_sock);
		return;
	    }

	    /* Bind socket */
	    status = pj_sock_bind_random(sock, &bound_addr,
					 turn_sock->setting.port_range,
					 max_bind_retry);
	    if (status != PJ_SUCCESS) {
		pj_turn_sock_destroy(turn_sock);
		return;
	    }
	    /* Apply QoS, if specified */
	    status = pj_sock_apply_qos2(sock, turn_sock->setting.qos_type,
				    &turn_sock->setting.qos_params,
				    (turn_sock->setting.qos_ignore_error?2:1),
				    turn_sock->pool->obj_name, NULL);
	    if (status != PJ_SUCCESS && !turn_sock->setting.qos_ignore_error) 
	    {
		pj_turn_sock_destroy(turn_sock);
		return;
	    }

	    /* Apply socket buffer size */
	    if (turn_sock->setting.so_rcvbuf_size > 0) {
		unsigned sobuf_size = turn_sock->setting.so_rcvbuf_size;
		status = pj_sock_setsockopt_sobuf(sock, pj_SO_RCVBUF(),
						  PJ_TRUE, &sobuf_size);
		if (status != PJ_SUCCESS) {
		    pj_perror(3, turn_sock->obj_name, status,
			      "Failed setting SO_RCVBUF");
		} else {
		    if (sobuf_size < turn_sock->setting.so_rcvbuf_size) {
			PJ_LOG(4, (turn_sock->obj_name,
				"Warning! Cannot set SO_RCVBUF as configured,"
				" now=%d, configured=%d", sobuf_size,
				turn_sock->setting.so_rcvbuf_size));
		    } else {
			PJ_LOG(5, (turn_sock->obj_name, "SO_RCVBUF set to %d",
				   sobuf_size));
		    }
		}
	    }
	    if (turn_sock->setting.so_sndbuf_size > 0) {
		unsigned sobuf_size = turn_sock->setting.so_sndbuf_size;
		status = pj_sock_setsockopt_sobuf(sock, pj_SO_SNDBUF(),
						  PJ_TRUE, &sobuf_size);
		if (status != PJ_SUCCESS) {
		    pj_perror(3, turn_sock->obj_name, status,
			      "Failed setting SO_SNDBUF");
		} else {
		    if (sobuf_size < turn_sock->setting.so_sndbuf_size) {
			PJ_LOG(4, (turn_sock->obj_name,
				"Warning! Cannot set SO_SNDBUF as configured,"
				" now=%d, configured=%d", sobuf_size,
				turn_sock->setting.so_sndbuf_size));
		    } else {
			PJ_LOG(5, (turn_sock->obj_name, "SO_SNDBUF set to %d",
				   sobuf_size));
		    }
		}
	    }

	    /* Create active socket */
	    pj_activesock_cfg_default(&asock_cfg);
	    asock_cfg.grp_lock = turn_sock->grp_lock;

	    pj_bzero(&asock_cb, sizeof(asock_cb));
	    asock_cb.on_data_read = &on_data_read_asock;
	    asock_cb.on_data_sent = &on_data_sent_asock;
	    asock_cb.on_connect_complete = &on_connect_complete_asock;
	    status = pj_activesock_create(turn_sock->pool, sock,
					  sock_type, &asock_cfg,
					  turn_sock->cfg.ioqueue, &asock_cb,
					  turn_sock,
					  &turn_sock->active_sock);
	}
#if PJ_HAS_SSL_SOCK
	else {
	    //TURN TLS
	    pj_ssl_sock_param param, *ssock_param;

	    ssock_param = &turn_sock->setting.tls_cfg.ssock_param;
	    pj_ssl_sock_param_default(&param);

	    pj_ssl_sock_param_copy(turn_sock->pool, &param, ssock_param);
	    param.cb.on_connect_complete = &on_connect_complete_ssl_sock;
	    param.cb.on_data_read = &on_data_read_ssl_sock;
	    param.cb.on_data_sent = &on_data_sent_ssl_sock;
	    param.ioqueue = turn_sock->cfg.ioqueue;
	    param.timer_heap = turn_sock->cfg.timer_heap;
	    param.grp_lock = turn_sock->grp_lock;
	    param.server_name = turn_sock->server_name;
	    param.user_data = turn_sock;
	    param.sock_type = sock_type;
	    param.sock_af = turn_sock->af;
	    if (param.send_buffer_size < PJ_TURN_MAX_PKT_LEN)
		param.send_buffer_size = PJ_TURN_MAX_PKT_LEN;
	    if (param.read_buffer_size < PJ_TURN_MAX_PKT_LEN)
		param.read_buffer_size = PJ_TURN_MAX_PKT_LEN;

	    param.qos_type = turn_sock->setting.qos_type;
	    param.qos_ignore_error = turn_sock->setting.qos_ignore_error;
	    pj_memcpy(&param.qos_params, &turn_sock->setting.qos_params,
		      sizeof(param.qos_params));

	    if (turn_sock->setting.tls_cfg.cert_file.slen ||
		turn_sock->setting.tls_cfg.ca_list_file.slen ||
		turn_sock->setting.tls_cfg.ca_list_path.slen ||
		turn_sock->setting.tls_cfg.privkey_file.slen)
	    {
		status = pj_ssl_cert_load_from_files2(
		    turn_sock->pool,
		    &turn_sock->setting.tls_cfg.ca_list_file,
		    &turn_sock->setting.tls_cfg.ca_list_path,
		    &turn_sock->setting.tls_cfg.cert_file,
		    &turn_sock->setting.tls_cfg.privkey_file,
		    &turn_sock->setting.tls_cfg.password,
		    &turn_sock->cert);

	    } else if (turn_sock->setting.tls_cfg.ca_buf.slen ||
		       turn_sock->setting.tls_cfg.cert_buf.slen ||
		       turn_sock->setting.tls_cfg.privkey_buf.slen)
	    {
		status = pj_ssl_cert_load_from_buffer(
		    turn_sock->pool,
		    &turn_sock->setting.tls_cfg.ca_buf,
		    &turn_sock->setting.tls_cfg.cert_buf,
		    &turn_sock->setting.tls_cfg.privkey_buf,
		    &turn_sock->setting.tls_cfg.password,
		    &turn_sock->cert);
	    }
	    if (status != PJ_SUCCESS) {
		pj_turn_sock_destroy(turn_sock);
		return;
	    }
	    if (turn_sock->cert) {
		pj_turn_sock_tls_cfg_wipe_keys(&turn_sock->setting.tls_cfg);
	    }

	    status = pj_ssl_sock_create(turn_sock->pool, &param,
					&turn_sock->ssl_sock);

	    if (status != PJ_SUCCESS) {
		pj_turn_sock_destroy(turn_sock);
		return;
	    }

	    if (turn_sock->cert) {
		status = pj_ssl_sock_set_certificate(turn_sock->ssl_sock,
						     turn_sock->pool,
						     turn_sock->cert);

		pj_ssl_cert_wipe_keys(turn_sock->cert);
		turn_sock->cert = NULL;
	    }

	}
#endif

	if (status != PJ_SUCCESS) {
	    pj_turn_sock_destroy(turn_sock);
	    return;
	}

	PJ_LOG(5,(turn_sock->pool->obj_name,
		  "Connecting to %s", 
		  pj_sockaddr_print(&info.server, addrtxt, 
				    sizeof(addrtxt), 3)));

	/* Initiate non-blocking connect */
	if (turn_sock->conn_type == PJ_TURN_TP_UDP) {
	    status = PJ_SUCCESS;
	}
#if PJ_HAS_TCP
	else if (turn_sock->conn_type == PJ_TURN_TP_TCP) {
	    status=pj_activesock_start_connect(
					turn_sock->active_sock, 
					turn_sock->pool,
					&info.server, 
					pj_sockaddr_get_len(&info.server));
	} 
#endif	
#if PJ_HAS_SSL_SOCK
	else if (turn_sock->conn_type == PJ_TURN_TP_TLS) {
	    pj_ssl_start_connect_param connect_param;
	    connect_param.pool = turn_sock->pool;
	    connect_param.localaddr = &bound_addr;
	    connect_param.local_port_range = turn_sock->setting.port_range;
	    connect_param.remaddr = &info.server;
	    connect_param.addr_len = pj_sockaddr_get_len(&info.server);

	    status = pj_ssl_sock_start_connect2(turn_sock->ssl_sock,
						&connect_param);
	}
#endif
	if (status == PJ_SUCCESS) {
	    on_connect_complete(turn_sock, PJ_SUCCESS);
	} else if (status != PJ_EPENDING) {
            PJ_PERROR(3, (turn_sock->pool->obj_name, status,
			  "Failed to connect to %s",
			  pj_sockaddr_print(&info.server, addrtxt,
					    sizeof(addrtxt), 3)));
	    pj_turn_sock_destroy(turn_sock);
	    return;
	}

	/* Done for now. Subsequent work will be done in 
	 * on_connect_complete() callback.
	 */
    }

    if (new_state >= PJ_TURN_STATE_DESTROYING && turn_sock->sess) {
	pj_time_val delay = {0, 0};

	turn_sock->sess = NULL;
	pj_turn_session_set_user_data(sess, NULL);

	pj_timer_heap_cancel_if_active(turn_sock->cfg.timer_heap,
	                               &turn_sock->timer, 0);
	pj_timer_heap_schedule_w_grp_lock(turn_sock->cfg.timer_heap,
	                                  &turn_sock->timer,
	                                  &delay, TIMER_DESTROY,
	                                  turn_sock->grp_lock);
    }
}


static void dataconn_cleanup(tcp_data_conn_t *conn)
{
    if (conn->asock)
	pj_activesock_close(conn->asock);

    pj_pool_safe_release(&conn->pool);

    pj_bzero(conn, sizeof(*conn));
}

static pj_bool_t dataconn_on_data_read(pj_activesock_t *asock,
				       void *data,
				       pj_size_t size,
				       pj_status_t status,
				       pj_size_t *remainder)
{
    tcp_data_conn_t *conn = (tcp_data_conn_t*)
			    pj_activesock_get_user_data(asock);
    pj_turn_sock *turn_sock = conn->turn_sock;

    pj_grp_lock_acquire(turn_sock->grp_lock);

    if (size == 0 && status != PJ_SUCCESS) {
	/* Connection gone, release data connection */
	dataconn_cleanup(conn);
	--turn_sock->data_conn_cnt;
	pj_grp_lock_release(turn_sock->grp_lock);
	return PJ_FALSE;
    }

    *remainder = size;
    while (*remainder > 0) {
	if (conn->state == DATACONN_STATE_READY) {
	    /* Application data */
	    if (turn_sock->cb.on_rx_data) {
		(*turn_sock->cb.on_rx_data)(turn_sock, data, *remainder,
					    &conn->peer_addr,
					    conn->peer_addr_len);
	    }
	    *remainder = 0;
	} else if (conn->state == DATACONN_STATE_CONN_BINDING) {
	    /* Waiting for ConnectionBind response */
	    pj_bool_t is_stun;
	    pj_turn_session_on_rx_pkt_param prm;

	    /* Ignore if this is not a STUN message */
	    is_stun = ((((pj_uint8_t*)data)[0] & 0xC0) == 0);
	    if (!is_stun)
		goto on_return;

	    pj_bzero(&prm, sizeof(prm));
	    prm.pkt = data;
	    prm.pkt_len = *remainder;
	    prm.src_addr = &conn->peer_addr;
	    prm.src_addr_len = conn->peer_addr_len;
	    pj_turn_session_on_rx_pkt2(conn->turn_sock->sess, &prm);
	    /* Got remainder? */
	    if (prm.parsed_len < *remainder && prm.parsed_len > 0) {
		pj_memmove(data, (pj_uint8_t*)data + prm.parsed_len,
			   *remainder);
	    }
	    *remainder -= prm.parsed_len;
	} else
	    goto on_return;
    }

on_return:
    pj_grp_lock_release(turn_sock->grp_lock);
    return PJ_TRUE;
}

static pj_bool_t dataconn_on_data_sent(pj_activesock_t *asock,
			      	       pj_ioqueue_op_key_t *send_key,
			      	       pj_ssize_t sent)
{
    tcp_data_conn_t *conn = (tcp_data_conn_t*)
			    pj_activesock_get_user_data(asock);
    pj_turn_sock *turn_sock = conn->turn_sock;

    return on_data_sent(turn_sock, send_key, sent);
}

static pj_bool_t dataconn_on_connect_complete(pj_activesock_t *asock,
					      pj_status_t status)
{
    tcp_data_conn_t *conn = (tcp_data_conn_t*)
			    pj_activesock_get_user_data(asock);
    pj_turn_sock *turn_sock = conn->turn_sock;

    pj_grp_lock_acquire(turn_sock->grp_lock);

    if (status == PJ_SUCCESS) {
	status = pj_activesock_start_read(asock, turn_sock->pool,
					  turn_sock->setting.max_pkt_size, 0);
    }
    if (status == PJ_SUCCESS) {
	conn->state = DATACONN_STATE_CONN_BINDING;
	status = pj_turn_session_connection_bind(turn_sock->sess,
						 conn->pool,
						 conn->id,
						 &conn->peer_addr,
						 conn->peer_addr_len);
    }
    if (status != PJ_SUCCESS) {
	dataconn_cleanup(conn);
	--turn_sock->data_conn_cnt;
	pj_grp_lock_release(turn_sock->grp_lock);
	return PJ_FALSE;
    }

    pj_grp_lock_release(turn_sock->grp_lock);
    return PJ_TRUE;
}


static void turn_on_connection_attempt(pj_turn_session *sess,
				       pj_uint32_t conn_id,
				       const pj_sockaddr_t *peer_addr,
				       unsigned addr_len)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) 
			      pj_turn_session_get_user_data(sess);
    pj_pool_t *pool;
    tcp_data_conn_t *new_conn;
    pj_turn_session_info info;
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_activesock_cfg asock_cfg;
    pj_activesock_cb asock_cb;
    pj_sockaddr bound_addr, *cfg_bind_addr;
    pj_uint16_t max_bind_retry;
    char addrtxt[PJ_INET6_ADDRSTRLEN+8];
    pj_status_t status;
    unsigned i;

    PJ_ASSERT_ON_FAIL(turn_sock->conn_type == PJ_TURN_TP_TCP &&
		      turn_sock->alloc_param.peer_conn_type == PJ_TURN_TP_TCP,
		      return);

    PJ_LOG(5,(turn_sock->pool->obj_name, "Connection attempt from peer %s",
	      pj_sockaddr_print(&peer_addr, addrtxt, sizeof(addrtxt), 3)));

    if (turn_sock == NULL) {
	/* We've been destroyed */
	return;
    }

    pj_grp_lock_acquire(turn_sock->grp_lock);

    if (turn_sock->data_conn_cnt == PJ_TURN_MAX_TCP_CONN_CNT) {
	/* Data connection has reached limit */
	pj_grp_lock_release(turn_sock->grp_lock);
	return;
    }

    /* Check if app wants to accept this connection */
    status = PJ_SUCCESS;
    if (turn_sock->cb.on_connection_attempt) {
	status = (*turn_sock->cb.on_connection_attempt)(turn_sock, conn_id,
							peer_addr, addr_len);
    }
    /* App rejects it */
    if (status != PJ_SUCCESS) {
	pj_perror(4, turn_sock->pool->obj_name, status,
		  "Rejected connection attempt from peer %s",
		  pj_sockaddr_print(peer_addr, addrtxt, sizeof(addrtxt), 3));
	pj_grp_lock_release(turn_sock->grp_lock);
	return;
    }

    /* Find free data connection slot */
    for (i=0; i < PJ_TURN_MAX_TCP_CONN_CNT; ++i) {
	if (turn_sock->data_conn[i].state == DATACONN_STATE_NULL)
	    break;
    }

    /* Verify that a free slot is found */
    pj_assert(i < PJ_TURN_MAX_TCP_CONN_CNT);
    ++turn_sock->data_conn_cnt;

    /* Init new data connection */
    new_conn = &turn_sock->data_conn[i];
    pj_bzero(new_conn, sizeof(*new_conn));
    pool = pj_pool_create(turn_sock->cfg.pf, "dataconn", 128, 128, NULL);
    new_conn->pool = pool;
    new_conn->id = conn_id;
    new_conn->turn_sock = turn_sock;
    pj_sockaddr_cp(&new_conn->peer_addr, peer_addr);
    new_conn->peer_addr_len = addr_len;
    pj_ioqueue_op_key_init(&new_conn->send_key, sizeof(new_conn->send_key));
    new_conn->state = DATACONN_STATE_INITSOCK;

    /* Init socket */
    status = pj_sock_socket(turn_sock->af, pj_SOCK_STREAM(), 0, &sock);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Bind socket */
    cfg_bind_addr = &turn_sock->setting.bound_addr;
    max_bind_retry = MAX_BIND_RETRY;
    if (turn_sock->setting.port_range &&
	turn_sock->setting.port_range < max_bind_retry)
    {
	max_bind_retry = turn_sock->setting.port_range;
    }
    pj_sockaddr_init(turn_sock->af, &bound_addr, NULL, 0);
    if (cfg_bind_addr->addr.sa_family == pj_AF_INET() ||
	cfg_bind_addr->addr.sa_family == pj_AF_INET6())
    {
	pj_sockaddr_cp(&bound_addr, cfg_bind_addr);
    }
    status = pj_sock_bind_random(sock, &bound_addr,
				 turn_sock->setting.port_range,
				 max_bind_retry);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Apply socket buffer size */
    if (turn_sock->setting.so_rcvbuf_size > 0) {
	unsigned sobuf_size = turn_sock->setting.so_rcvbuf_size;
	status = pj_sock_setsockopt_sobuf(sock, pj_SO_RCVBUF(), PJ_TRUE,
					  &sobuf_size);
	if (status != PJ_SUCCESS) {
	    pj_perror(3, turn_sock->obj_name, status,
		      "Failed setting SO_RCVBUF");
	} else {
	    if (sobuf_size < turn_sock->setting.so_rcvbuf_size) {
		PJ_LOG(4, (turn_sock->obj_name,
			   "Warning! Cannot set SO_RCVBUF as configured,"
			   " now=%d, configured=%d", sobuf_size,
			   turn_sock->setting.so_rcvbuf_size));
	    } else {
		PJ_LOG(5, (turn_sock->obj_name, "SO_RCVBUF set to %d",
			   sobuf_size));
	    }
	}
    }
    if (turn_sock->setting.so_sndbuf_size > 0) {
	unsigned sobuf_size = turn_sock->setting.so_sndbuf_size;
	status = pj_sock_setsockopt_sobuf(sock, pj_SO_SNDBUF(), PJ_TRUE,
					  &sobuf_size);
	if (status != PJ_SUCCESS) {
	    pj_perror(3, turn_sock->obj_name, status,
		      "Failed setting SO_SNDBUF");
	} else {
	    if (sobuf_size < turn_sock->setting.so_sndbuf_size) {
		PJ_LOG(4, (turn_sock->obj_name,
			   "Warning! Cannot set SO_SNDBUF as configured,"
			   " now=%d, configured=%d", sobuf_size,
			   turn_sock->setting.so_sndbuf_size));
	    } else {
		PJ_LOG(5, (turn_sock->obj_name, "SO_SNDBUF set to %d",
			   sobuf_size));
	    }
	}
    }

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.grp_lock = turn_sock->grp_lock;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = &dataconn_on_data_read;
    asock_cb.on_data_sent = &dataconn_on_data_sent;
    asock_cb.on_connect_complete = &dataconn_on_connect_complete;
    status = pj_activesock_create(pool, sock,
				  pj_SOCK_STREAM(), &asock_cfg,
				  turn_sock->cfg.ioqueue, &asock_cb,
				  new_conn, &new_conn->asock);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Connect to TURN server for data connection */
    pj_turn_session_get_info(turn_sock->sess, &info);
    status = pj_activesock_start_connect(new_conn->asock,
					 pool,
					 &info.server,
					 pj_sockaddr_get_len(&info.server));
    if (status == PJ_SUCCESS) {
	dataconn_on_connect_complete(new_conn->asock, PJ_SUCCESS);
	pj_grp_lock_release(turn_sock->grp_lock);
	return;
    }

on_return:
    if (status == PJ_EPENDING) {
	PJ_LOG(5,(pool->obj_name,
		  "Accepting connection from peer %s",
		  pj_sockaddr_print(peer_addr, addrtxt, sizeof(addrtxt), 3)));
    } else {
	/* not PJ_SUCCESS */
	pj_perror(4, pool->obj_name, status,
		  "Failed in accepting connection from peer %s",
		  pj_sockaddr_print(peer_addr, addrtxt, sizeof(addrtxt), 3));

	if (!new_conn->asock && sock != PJ_INVALID_SOCKET)
	    pj_sock_close(sock);    

	dataconn_cleanup(new_conn);
	--turn_sock->data_conn_cnt;

	/* Notify app for failure */
	if (turn_sock->cb.on_connection_status) {
	    (*turn_sock->cb.on_connection_status)(turn_sock, status, conn_id,
						  peer_addr, addr_len);
	}
    }
    pj_grp_lock_release(turn_sock->grp_lock);
}

static void turn_on_connection_bind_status(pj_turn_session *sess,
					   pj_status_t status,
					   pj_uint32_t conn_id,
					   const pj_sockaddr_t *peer_addr,
					   unsigned addr_len)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) 
			      pj_turn_session_get_user_data(sess);
    tcp_data_conn_t *conn = NULL;
    unsigned i;

    pj_grp_lock_acquire(turn_sock->grp_lock);

    for (i=0; i < PJ_TURN_MAX_TCP_CONN_CNT; ++i) {
	tcp_data_conn_t *c = &turn_sock->data_conn[i];
	if (c->id == conn_id &&
	    pj_sockaddr_cmp(peer_addr, &c->peer_addr) == 0)
	{
	    conn = c;
	    break;
	}
    }
    if (!conn) {
	PJ_LOG(5,(turn_sock->pool->obj_name,
		  "Warning: stray connection bind event"));
	pj_grp_lock_release(turn_sock->grp_lock);
	return;
    }

    if (status == PJ_SUCCESS) {
	conn->state = DATACONN_STATE_READY;
    } else {
	dataconn_cleanup(conn);
	--turn_sock->data_conn_cnt;
    }

    pj_grp_lock_release(turn_sock->grp_lock);

    if (turn_sock->cb.on_connection_status) {
	(*turn_sock->cb.on_connection_status)(turn_sock, status, conn_id,
					      peer_addr, addr_len);
    }
}

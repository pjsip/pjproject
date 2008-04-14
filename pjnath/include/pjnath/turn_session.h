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
#ifndef __PJNATH_TURN_SESSION_H__
#define __PJNATH_TURN_SESSION_H__

/**
 * @file turn_session.h
 * @brief Transport independent TURN client session.
 */
#include <pjnath/stun_session.h>
#include <pjlib-util/resolver.h>


PJ_BEGIN_DECL

/* **************************************************************************/
/**
 * @defgroup PJNATH_TURN_SESSION TURN client session
 * @brief Transport independent TURN client session
 * @ingroup PJNATH_STUN
 * @{
 */

/** 
 * Opaque declaration for TURN client session.
 */
typedef struct pj_turn_session pj_turn_session;


#define PJ_TURN_INVALID_CHANNEL	    0xFFFF
#define PJ_TURN_CHANNEL_MIN	    0x4000
#define PJ_TURN_CHANNEL_MAX	    0xFFFE  /* inclusive */
#define PJ_TURN_NO_TIMEOUT	    ((long)0x7FFFFFFF)
#define PJ_TURN_MAX_PKT_LEN	    3000
#define PJ_TURN_PERM_TIMEOUT	    300	/* Must be greater than REFRESH_SEC_BEFORE */
#define PJ_TURN_CHANNEL_TIMEOUT	    600	/* Must be greater than REFRESH_SEC_BEFORE */
#define PJ_TURN_REFRESH_SEC_BEFORE  60
#define PJ_TURN_KEEP_ALIVE_SEC	    15
#define PJ_TURN_PEER_HTABLE_SIZE    8


/** 
 * TURN transport types, which will be used both to specify the connection 
 * type for reaching TURN server and the type of allocation transport to be 
 * requested to server (the REQUESTED-TRANSPORT attribute).
 */
typedef enum pj_turn_tp_type
{
    /**
     * UDP transport, which value corresponds to IANA protocol number.
     */
    PJ_TURN_TP_UDP = 17,

    /**
     * TCP transport, which value corresponds to IANA protocol number.
     */
    PJ_TURN_TP_TCP = 6,

    /**
     * TLS transport. The TLS transport will only be used as the connection
     * type to reach the server and never as the allocation transport type.
     */
    PJ_TURN_TP_TLS = 255

} pj_turn_tp_type;


/** TURN session state */
typedef enum pj_turn_state_t
{
    /**
     * TURN session has just been created.
     */
    PJ_TURN_STATE_NULL,

    /**
     * TURN server has been configured and now is being resolved via
     * DNS SRV resolution.
     */
    PJ_TURN_STATE_RESOLVING,

    /**
     * TURN server has been resolved. If there is pending allocation to
     * be done, it will be invoked immediately.
     */
    PJ_TURN_STATE_RESOLVED,

    /**
     * TURN session has issued ALLOCATE request and is waiting for response
     * from the TURN server.
     */
    PJ_TURN_STATE_ALLOCATING,

    /**
     * TURN session has successfully allocated relay resoruce and now is
     * ready to be used.
     */
    PJ_TURN_STATE_READY,

    /**
     * TURN session has issued deallocate request and is waiting for a
     * response from the TURN server.
     */
    PJ_TURN_STATE_DEALLOCATING,

    /**
     * Deallocate response has been received. Normally the session will
     * proceed to DESTROYING state immediately.
     */
    PJ_TURN_STATE_DEALLOCATED,

    /**
     * TURN session is being destroyed.
     */
    PJ_TURN_STATE_DESTROYING

} pj_turn_state_t;


/* ChannelData header */
typedef struct pj_turn_channel_data
{
    pj_uint16_t ch_number;
    pj_uint16_t length;
} pj_turn_channel_data;



/**
 * Callback to receive events from TURN session.
 */
typedef struct pj_turn_session_cb
{
    /**
     * This callback will be called by the TURN session whenever it
     * needs to send outgoing message. Since the TURN session doesn't
     * have a socket on its own, this callback must be implemented.
     */
    pj_status_t (*on_send_pkt)(pj_turn_session *sess,
			       const pj_uint8_t *pkt,
			       unsigned pkt_len,
			       const pj_sockaddr_t *dst_addr,
			       unsigned dst_addr_len);

    /**
     * Notification when peer address has been bound successfully to 
     * a channel number.
     *
     * This callback is optional.
     */
    void (*on_channel_bound)(pj_turn_session *sess,
			     const pj_sockaddr_t *peer_addr,
			     unsigned addr_len,
			     unsigned ch_num);

    /**
     * Notification when incoming data has been received, either through
     * Data indication or ChannelData message from the TURN server.
     *
     * This callback is optional.
     */
    void (*on_rx_data)(pj_turn_session *sess,
		       void *pkt,
		       unsigned pkt_len,
		       const pj_sockaddr_t *peer_addr,
		       unsigned addr_len);

    /**
     * Notification when TURN session state has changed. Application should
     * implement this callback at least to know that the TURN session is
     * going to be destroyed.
     */
    void (*on_state)(pj_turn_session *sess, pj_turn_state_t old_state,
		     pj_turn_state_t new_state);

} pj_turn_session_cb;


/**
 * Allocate parameter.
 */
typedef struct pj_turn_alloc_param
{
    int	    bandwidth;
    int	    lifetime;
    int	    ka_interval;
} pj_turn_alloc_param;


/**
 * TURN session info.
 */
typedef struct pj_turn_session_info
{
    /**
     * Session state.
     */
    pj_turn_state_t state;

    /**
     * Type of connection to the TURN server.
     */
    pj_turn_tp_type tp_type;

    /**
     * The relay address
     */
    pj_sockaddr	    relay_addr;

    /**
     * The selected TURN server address.
     */
    pj_sockaddr	    server;

    /**
     * Current seconds before allocation expires.
     */
    int		    lifetime;

} pj_turn_session_info;


/**
 * Create default pj_turn_alloc_param.
 */
PJ_DECL(void) pj_turn_alloc_param_default(pj_turn_alloc_param *prm);

/**
 * Duplicate pj_turn_alloc_param.
 */
PJ_DECL(void) pj_turn_alloc_param_copy(pj_pool_t *pool, 
				       pj_turn_alloc_param *dst,
				       const pj_turn_alloc_param *src);

/**
 * Get TURN state name.
 */
PJ_DECL(const char*) pj_turn_state_name(pj_turn_state_t state);


/**
 * Create TURN client session.
 */
PJ_DECL(pj_status_t) pj_turn_session_create(const pj_stun_config *cfg,
					    const char *name,
					    int af,
					    pj_turn_tp_type conn_type,
					    const pj_turn_session_cb *cb,
					    void *user_data,
					    unsigned options,
					    pj_turn_session **p_sess);


/**
 * Shutdown TURN client session.
 */
PJ_DECL(pj_status_t) pj_turn_session_shutdown(pj_turn_session *sess);


/**
 * Forcefully destroy the TURN session.
 */
PJ_DECL(pj_status_t) pj_turn_session_destroy(pj_turn_session *sess);


/**
 * Get TURN session info.
 */
PJ_DECL(pj_status_t) pj_turn_session_get_info(pj_turn_session *sess,
					      pj_turn_session_info *info);

/**
 * Re-assign user data.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_user_data(pj_turn_session *sess,
						   void *user_data);

/**
 * Retrieve user data.
 */
PJ_DECL(void*) pj_turn_session_get_user_data(pj_turn_session *sess);

/**
 * Set the server or domain name of the server.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_server(pj_turn_session *sess,
					        const pj_str_t *domain,
						int default_port,
						pj_dns_resolver *resolver);


/**
 * Set credential to be used by the session.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_credential(pj_turn_session *sess,
					      const pj_stun_auth_cred *cred);


/**
 * Create TURN allocation.
 */
PJ_DECL(pj_status_t) pj_turn_session_alloc(pj_turn_session *sess,
					   const pj_turn_alloc_param *param);


/**
 * Relay data to the specified peer through the session.
 */
PJ_DECL(pj_status_t) pj_turn_session_sendto(pj_turn_session *sess,
					    const pj_uint8_t *pkt,
					    unsigned pkt_len,
					    const pj_sockaddr_t *addr,
					    unsigned addr_len);

/**
 * Bind a peer address to a channel number.
 */
PJ_DECL(pj_status_t) pj_turn_session_bind_channel(pj_turn_session *sess,
						  const pj_sockaddr_t *peer,
						  unsigned addr_len);

/**
 * Notify TURN client session upon receiving a packet from server.
 * The packet maybe a STUN packet or ChannelData packet.
 */
PJ_DECL(pj_status_t) pj_turn_session_on_rx_pkt(pj_turn_session *sess,
					       void *pkt,
					       unsigned pkt_len,
					       pj_bool_t is_datagram);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_TURN_SESSION_H__ */


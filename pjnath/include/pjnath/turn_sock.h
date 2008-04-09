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
#ifndef __PJNATH_turn_sock_H__
#define __PJNATH_turn_sock_H__

/**
 * @file turn_sock.h
 * @brief TURN relay using UDP client as transport protocol
 */
#include <pjnath/turn_session.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJNATH_TURN_UDP TURN TCP client
 * @brief TURN relay using TCP client as transport protocol
 * @ingroup PJNATH_STUN
 * @{
 */


/** 
 * Opaque declaration for TURN TCP client.
 */
typedef struct pj_turn_sock pj_turn_sock;


typedef struct pj_turn_sock_cb
{
    /**
     * Notification when incoming data has been received, either through
     * Data indication or ChannelData message from the TURN server.
     *
     * This callback is mandatory.
     */
    void (*on_rx_data)(pj_turn_sock *turn_sock,
		       const pj_uint8_t *pkt,
		       unsigned pkt_len,
		       const pj_sockaddr_t *peer_addr,
		       unsigned addr_len);

    /**
     * Notification when TURN session state has changed. Application should
     * implement this callback to know that the TURN session is no longer
     * available.
     */
    void (*on_state)(pj_turn_sock *turn_sock, pj_turn_state_t old_state,
		     pj_turn_state_t new_state);

} pj_turn_sock_cb;


/**
 * Create.
 */
PJ_DECL(pj_status_t) pj_turn_sock_create(pj_stun_config *cfg,
					 int af,
					 pj_turn_tp_type conn_type,
					 const pj_turn_sock_cb *cb,
					 unsigned options,
					 void *user_data,
					 pj_turn_sock **p_turn_sock);

/**
 * Destroy.
 */
PJ_DECL(void) pj_turn_sock_destroy(pj_turn_sock *turn_sock);

/**
 * Set user data.
 */
PJ_DECL(pj_status_t) pj_turn_sock_set_user_data(pj_turn_sock *turn_sock,
					       void *user_data);

/**
 * Get user data.
 */
PJ_DECL(void*) pj_turn_sock_get_user_data(pj_turn_sock *turn_sock);


/**
 * Get info.
 */
PJ_DECL(pj_status_t) pj_turn_sock_get_info(pj_turn_sock *turn_sock,
					  pj_turn_session_info *info);

/**
 * Initialize.
 */
PJ_DECL(pj_status_t) pj_turn_sock_init(pj_turn_sock *turn_sock,
				       const pj_str_t *domain,
				       int default_port,
				       pj_dns_resolver *resolver,
				       const pj_stun_auth_cred *cred,
				       const pj_turn_alloc_param *param);

/**
 * Send packet.
 */ 
PJ_DECL(pj_status_t) pj_turn_sock_sendto(pj_turn_sock *turn_sock,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_sockaddr_t *addr,
					unsigned addr_len);

/**
 * Bind a peer address to a channel number.
 */
PJ_DECL(pj_status_t) pj_turn_sock_bind_channel(pj_turn_sock *turn_sock,
					      const pj_sockaddr_t *peer,
					      unsigned addr_len);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_turn_sock_H__ */


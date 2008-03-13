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
#ifndef __PJNATH_TURN_UDP_H__
#define __PJNATH_TURN_UDP_H__

/**
 * @file turn_udp.h
 * @brief TURN relay using UDP client as transport protocol
 */
#include <pjnath/turn_session.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJNATH_TURN_UDP TURN UDP client
 * @brief TURN relay using UDP client as transport protocol
 * @ingroup PJNATH_STUN
 * @{
 */


/** 
 * Opaque declaration for TURN UDP client.
 */
typedef struct pj_turn_udp pj_turn_udp;


typedef struct pj_turn_udp_cb
{
    /**
     * Notification when incoming data has been received, either through
     * Data indication or ChannelData message from the TURN server.
     *
     * This callback is mandatory.
     */
    void (*on_rx_data)(pj_turn_udp *udp_rel,
		       const pj_uint8_t *pkt,
		       unsigned pkt_len,
		       const pj_sockaddr_t *peer_addr,
		       unsigned addr_len);

    /**
     * Notification when TURN session state has changed. Application should
     * implement this callback to know that the TURN session is no longer
     * available.
     */
    void (*on_state)(pj_turn_udp *udp_rel, pj_turn_state_t old_state,
		     pj_turn_state_t new_state);

} pj_turn_udp_cb;


/**
 * Create.
 */
PJ_DECL(pj_status_t) pj_turn_udp_create(pj_stun_config *cfg,
					int af,
					const pj_turn_udp_cb *cb,
					unsigned options,
					void *user_data,
					pj_turn_udp **p_udp_rel);

/**
 * Destroy.
 */
PJ_DECL(void) pj_turn_udp_destroy(pj_turn_udp *udp_rel);

/**
 * Set user data.
 */
PJ_DECL(pj_status_t) pj_turn_udp_set_user_data(pj_turn_udp *udp_rel,
					       void *user_data);

/**
 * Get user data.
 */
PJ_DECL(void*) pj_turn_udp_get_user_data(pj_turn_udp *udp_rel);


/**
 * Get info.
 */
PJ_DECL(pj_status_t) pj_turn_udp_get_info(pj_turn_udp *udp_rel,
					  pj_turn_session_info *info);

/**
 * Initialize.
 */
PJ_DECL(pj_status_t) pj_turn_udp_init(pj_turn_udp *udp_rel,
				      const pj_str_t *domain,
				      int default_port,
				      pj_dns_resolver *resolver,
				      const pj_stun_auth_cred *cred,
				      const pj_turn_alloc_param *param);

/**
 * Send packet.
 */ 
PJ_DECL(pj_status_t) pj_turn_udp_sendto(pj_turn_udp *udp_rel,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_sockaddr_t *addr,
					unsigned addr_len);

/**
 * Bind a peer address to a channel number.
 */
PJ_DECL(pj_status_t) pj_turn_udp_bind_channel(pj_turn_udp *udp_rel,
					      const pj_sockaddr_t *peer,
					      unsigned addr_len);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_TURN_UDP_H__ */


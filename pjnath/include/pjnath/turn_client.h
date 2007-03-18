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
#ifndef __PJNATH_TURN_CLIENT_H__
#define __PJNATH_TURN_CLIENT_H__

/**
 * @file turn_client.h
 * @brief TURN client session.
 */

#include <pjnath/stun_msg.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJNATH_TURN_CLIENT TURN Client Session
 * @brief Management of STUN/TURN client session
 * @ingroup PJNATH_STUN
 * @{
 */

typedef struct pj_turn_client pj_turn_client;

/**
 * This describes TURN client config.
 */
typedef struct pj_turn_client_cb
{
    /**
     * Callback to be called by the TURN session to send outgoing message.
     *
     * @param client	    The TURN client session.
     * @param pkt	    Packet to be sent.
     * @param pkt_size	    Size of the packet to be sent.
     * @param dst_addr	    The destination address.
     * @param addr_len	    Length of destination address.
     *
     * @return		    The callback should return the status of the
     *			    packet sending.
     */
    pj_status_t (*on_send_msg)(pj_turn_client *client,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len);

    /**
     * Callback to be called by TURN session when its state has changed.
     */
    pj_status_t (*on_state_changed)(pj_turn_client *client);

} pj_turn_client_cb;


/**
 * Options
 */
typedef struct pj_turn_client_config
{
    int	    bandwidth;
    int	    lifetime;
    int	    sock_type;
    int	    port;
} pj_turn_client_config;


PJ_INLINE(void) pj_turn_client_config_default(pj_turn_client_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->bandwidth = -1;
    cfg->lifetime = -1;
    cfg->sock_type = -1;
    cfg->port = -1;
}


/**
 * This describes the TURN client session.
 */
struct pj_turn_client
{
    pj_pool_t	    *pool;
    pj_stun_session *session;
    pj_timer_entry   alloc_timer;
    pj_sockaddr_in   mapped_addr;
    pj_sockaddr_in   relay_addr;
};




/**
 * Create the TURN client session.
 */
PJ_DECL(pj_status_t) pj_turn_client_create(pj_stun_endpoint *endpt,
					   const pj_turn_client_config *cfg,
					   const pj_turn_client_cb *cb,
					   pj_turn_client **p_client);

/**
 * Start the TURN client session by sending Allocate request to the server.
 */


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_TURN_CLIENT_H__ */


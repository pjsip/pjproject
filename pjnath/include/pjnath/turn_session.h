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
#define PJ_TURN_PERM_TIMEOUT	    300
#define PJ_TURN_CHANNEL_TIMEOUT	    600


/** Transport types */
enum {
    PJ_TURN_TP_UDP = 16,    /**< UDP.	*/
    PJ_TURN_TP_TCP = 6	    /**< TCP.	*/
};

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
     * Callback to send outgoing packet. This callback is mandatory.
     */
    pj_status_t (*on_send_pkt)(pj_turn_session *sess,
			       const pj_uint8_t *pkt,
			       unsigned pkt_len,
			       const pj_sockaddr_t *dst_addr,
			       unsigned dst_addr_len);

    /**
     * Notification when allocation completes, either successfully or
     * with failure.
     */
    void (*on_allocate_complete)(pj_turn_session *sess,
				 pj_status_t status);

    /**
     * Notification when data is received.
     */
    void (*on_rx_data)(pj_turn_session *sess,
		       const pj_uint8_t *pkt,
		       unsigned pkt_len,
		       const pj_sockaddr_t *peer_addr,
		       unsigned addr_len);

    /**
     * Notification when session has been destroyed.
     */
    void (*on_destroyed)(pj_turn_session *sess);

} pj_turn_session_cb;


/**
 * Allocate parameter.
 */
typedef struct pj_turn_alloc_param
{
    int	    bandwidth;
    int	    lifetime;
} pj_turn_alloc_param;


/**
 * TURN session info.
 */
typedef struct pj_turn_session_info
{
    pj_sockaddr	    server;
} pj_turn_session_info;


/**
 * Create TURN client session.
 */
PJ_DECL(pj_status_t) pj_turn_session_create(pj_stun_config *cfg,
					    const pj_turn_session_cb *cb,
					    pj_turn_session **p_sess);


/**
 * Destroy TURN client session.
 */
PJ_DECL(pj_status_t) pj_turn_session_destroy(pj_turn_session *sess);


/**
 * Set the server or domain name of the server.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_server(pj_turn_session *sess,
					        const pj_str_t *domain,
					        const pj_str_t *res_name,
						int default_port,
						pj_dns_resolver *resolver);


/**
 * Set credential to be used by the session.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_cred(pj_turn_session *sess,
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
					       const pj_uint8_t *pkt,
					       unsigned pkt_len,
					       pj_bool_t is_datagram);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_TURN_SESSION_H__ */


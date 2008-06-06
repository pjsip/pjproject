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
#ifndef __PJNATH_ICE_STRANS_H__
#define __PJNATH_ICE_STRANS_H__


/**
 * @file ice_strans.h
 * @brief ICE Stream Transport
 */
#include <pjnath/ice_session.h>
#include <pjnath/stun_sock.h>
#include <pjnath/turn_sock.h>
#include <pjlib-util/resolver.h>
#include <pj/ioqueue.h>
#include <pj/timer.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJNATH_ICE_STREAM_TRANSPORT ICE Stream Transport
 * @brief Transport for media streams using ICE
 * @ingroup PJNATH_ICE
 * @{
 *
 * This module describes ICE stream transport, as represented by #pj_ice_strans
 * structure, and is part of PJNATH - the Open Source NAT traversal helper
 * library.
 *
 * ICE stream transport, as represented by #pj_ice_strans structure, is an ICE
 * capable class for transporting media streams within a media session. 
 * It consists of one or more transport sockets (typically two for RTP
 * based communication - one for RTP and one for RTCP), and an 
 * \ref PJNATH_ICE_SESSION for performing connectivity checks among the.
 * various candidates of the transport addresses.
 *
 */

/** Forward declaration for ICE stream transport. */
typedef struct pj_ice_strans pj_ice_strans;

/** Transport operation types to be reported on \a on_status() callback */
typedef enum pj_ice_strans_op
{
    /** Initialization (candidate gathering) */
    PJ_ICE_STRANS_OP_INIT,

    /** Negotiation */
    PJ_ICE_STRANS_OP_NEGOTIATION

} pj_ice_strans_op;

/** 
 * This structure contains callbacks that will be called by the 
 * ICE stream transport.
 */
typedef struct pj_ice_strans_cb
{
    /**
     * This callback will be called when the ICE transport receives
     * incoming packet from the sockets which is not related to ICE
     * (for example, normal RTP/RTCP packet destined for application).
     *
     * @param ice_st	    The ICE stream transport.
     * @param comp_id	    The component ID.
     * @param pkt	    The packet.
     * @param size	    Size of the packet.
     * @param src_addr	    Source address of the packet.
     * @param src_addr_len  Length of the source address.
     */
    void    (*on_rx_data)(pj_ice_strans *ice_st,
			  unsigned comp_id, 
			  void *pkt, pj_size_t size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len);

    /**
     * Callback to report status.
     * 
     * @param ice_st	    The ICE stream transport.
     * @param op	    The operation
     * @param status	    Operation status.
     */
    void    (*on_ice_complete)(pj_ice_strans *ice_st, 
			       pj_ice_strans_op op,
			       pj_status_t status);

} pj_ice_strans_cb;


/**
 * This structure describes ICE stream transport configuration. Application
 * should initialize the structure by calling #pj_ice_strans_cfg_default()
 * before changing the settings.
 */
typedef struct pj_ice_strans_cfg
{
    /**
     * Address family, IPv4 or IPv6. Currently only pj_AF_INET() (IPv4)
     * is supported, and this is the default value.
     */
    int			af;

    /**
     * STUN configuration which contains the timer heap and
     * ioqueue instance to be used, and STUN retransmission
     * settings. This setting is mandatory.
     *
     * The default value is all zero. Application must initialize
     * this setting with #pj_stun_config_init().
     */
    pj_stun_config	 stun_cfg;

    /**
     * DNS resolver to be used to resolve servers. If DNS SRV
     * resolution is required, the resolver must be set.
     *
     * The default value is NULL.
     */
    pj_dns_resolver	*resolver;

    /**
     * STUN and local transport settings. This specifies the 
     * settings for local UDP socket, which will be resolved
     * to get the STUN mapped address.
     */
    struct {
	/**
	 * Optional configuration for STUN transport. The default
	 * value will be initialized with #pj_stun_sock_cfg_default().
	 */
	pj_stun_sock_cfg     cfg;

	/**
	 * Disable host candidates. When this option is set, no
	 * host candidates will be added.
	 *
	 * Default: PJ_FALSE
	 */
	pj_bool_t	     no_host_cands;

	/**
	 * Include loopback addresses in the host candidates.
	 *
	 * Default: PJ_FALSE
	 */
	pj_bool_t	     loop_addr;

	/**
	 * Specify the STUN server domain or hostname or IP address.
	 * If DNS SRV resolution is required, application must fill
	 * in this setting with the domain name of the STUN server 
	 * and set the resolver instance in the \a resolver field.
	 * Otherwise if the \a resolver setting is not set, this
	 * field will be resolved with hostname resolution and in
	 * this case the \a port field must be set.
	 *
	 * The \a port field should also be set even when DNS SRV
	 * resolution is used, in case the DNS SRV resolution fails.
	 *
	 * When this field is empty, STUN mapped address resolution
	 * will not be performed. In this case only ICE host candidates
	 * will be added to the ICE transport, unless if \a no_host_cands
	 * field is set. In this case, both host and srflx candidates 
	 * are disabled.
	 *
	 * The default value is empty.
	 */
	pj_str_t	     server;

	/**
	 * The port number of the STUN server, when \a server
	 * field specifies a hostname rather than domain name. This
	 * field should also be set even when the \a server
	 * specifies a domain name, to allow DNS SRV resolution
	 * to fallback to DNS A/AAAA resolution when the DNS SRV
	 * resolution fails.
	 *
	 * The default value is PJ_STUN_PORT.
	 */
	pj_uint16_t	     port;

    } stun;

    /**
     * TURN specific settings.
     */
    struct {
	/**
	 * Specify the TURN server domain or hostname or IP address.
	 * If DNS SRV resolution is required, application must fill
	 * in this setting with the domain name of the TURN server 
	 * and set the resolver instance in the \a resolver field.
	 * Otherwise if the \a resolver setting is not set, this
	 * field will be resolved with hostname resolution and in
	 * this case the \a port field must be set.
	 *
	 * The \a port field should also be set even when DNS SRV
	 * resolution is used, in case the DNS SRV resolution fails.
	 *
	 * When this field is empty, relay candidate will not be
	 * created.
	 *
	 * The default value is empty.
	 */
	pj_str_t	     server;

	/**
	 * The port number of the TURN server, when \a server
	 * field specifies a hostname rather than domain name. This
	 * field should also be set even when the \a server
	 * specifies a domain name, to allow DNS SRV resolution
	 * to fallback to DNS A/AAAA resolution when the DNS SRV
	 * resolution fails.
	 *
	 * Default is zero.
	 */
	pj_uint16_t	     port;

	/**
	 * Type of connection to the TURN server.
	 *
	 * Default is PJ_TURN_TP_UDP.
	 */
	pj_turn_tp_type	     conn_type;

	/**
	 * Credential to be used for the TURN session. This setting
	 * is mandatory.
	 *
	 * Default is to have no credential.
	 */
	pj_stun_auth_cred    auth_cred;

	/**
	 * Optional TURN Allocate parameter. The default value will be
	 * initialized by #pj_turn_alloc_param_default().
	 */
	pj_turn_alloc_param  alloc_param;

    } turn;

} pj_ice_strans_cfg;


/** 
 * Initialize ICE transport configuration with default values.
 *
 * @param cfg		The configuration to be initialized.
 */
PJ_DECL(void) pj_ice_strans_cfg_default(pj_ice_strans_cfg *cfg);


/**
 * Copy configuration.
 *
 * @param pool		Pool.
 * @param dst		Destination.
 * @param src		Source.
 */
PJ_DECL(void) pj_ice_strans_cfg_copy(pj_pool_t *pool,
				     pj_ice_strans_cfg *dst,
				     const pj_ice_strans_cfg *src);


/**
 * Create and initialize the ICE stream transport with the specified
 * parameters. 
 *
 * @param name		Optional name for logging identification.
 * @param cfg		Configuration.
 * @param comp_cnt	Number of components.
 * @param user_data	Arbitrary user data to be associated with this
 *			ICE stream transport.
 * @param cb		Callback.
 * @param p_ice_st	Pointer to receive the ICE stream transport
 *			instance.
 *
 * @return		PJ_SUCCESS if ICE stream transport is created
 *			successfully.
 */
PJ_DECL(pj_status_t) pj_ice_strans_create(const char *name,
					  const pj_ice_strans_cfg *cfg,
					  unsigned comp_cnt,
					  void *user_data,
					  const pj_ice_strans_cb *cb,
					  pj_ice_strans **p_ice_st);

/**
 * Destroy the ICE stream transport. This will destroy the ICE session
 * inside the ICE stream transport, close all sockets and release all
 * other resources.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_destroy(pj_ice_strans *ice_st);


/**
 * Get the user data associated with the ICE stream transport.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		The user data.
 */
PJ_DECL(void*) pj_ice_strans_get_user_data(pj_ice_strans *ice_st);


/**
 * Initialize the ICE session in the ICE stream transport.
 * When application is about to send an offer containing ICE capability,
 * or when it receives an offer containing ICE capability, it must
 * call this function to initialize the internal ICE session. This would
 * register all transport address aliases for each component to the ICE
 * session as candidates. Then application can enumerate all local
 * candidates by calling #pj_ice_strans_enum_cands(), and encode these
 * candidates in the SDP to be sent to remote agent.
 *
 * @param ice_st	The ICE stream transport.
 * @param role		ICE role.
 * @param local_ufrag	Optional local username fragment.
 * @param local_passwd	Optional local password.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_init_ice(pj_ice_strans *ice_st,
					    pj_ice_sess_role role,
					    const pj_str_t *local_ufrag,
					    const pj_str_t *local_passwd);

/**
 * Enumerate the local candidates for the specified component.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 * @param count		On input, it specifies the maximum number of
 *			elements. On output, it will be filled with
 *			the number of candidates copied to the
 *			array.
 * @param cand		Array of candidates.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_enum_cands(pj_ice_strans *ice_st,
					      unsigned comp_id,
					      unsigned *count,
					      pj_ice_sess_cand cand[]);

/**
 * Get the default candidate for the specified component. When this
 * function is called before ICE negotiation completes, the default
 * candidate is selected according to local preference criteria. When
 * this function is called after ICE negotiation completes, the
 * default candidate is the candidate that forms the valid pair.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 * @param cand		Pointer to receive the default candidate
 *			information.
 */
PJ_DECL(pj_status_t) pj_ice_strans_get_def_cand(pj_ice_strans *ice_st,
						unsigned comp_id,
						pj_ice_sess_cand *cand);

/**
 * Get the current ICE role. ICE session must have been initialized
 * before this function can be called.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		Current ICE role.
 */
PJ_DECL(pj_ice_sess_role) pj_ice_strans_get_role(pj_ice_strans *ice_st);


/**
 * Change session role. This happens for example when ICE session was
 * created with controlled role when receiving an offer, but it turns out
 * that the offer contains "a=ice-lite" attribute when the SDP gets
 * inspected. ICE session must have been initialized before this function
 * can be called.
 *
 * @param ice_st	The ICE stream transport.
 * @param new_role	The new role to be set.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_strans_change_role(pj_ice_strans *ice_st,
					       pj_ice_sess_role new_role);


/**
 * Start ICE connectivity checks. This function can only be called
 * after the ICE session has been created in the ICE stream transport
 * with #pj_ice_strans_init_ice().
 *
 * This function must be called once application has received remote
 * candidate list (typically from the remote SDP). This function pairs
 * local candidates with remote candidates, and starts ICE connectivity
 * checks. The ICE session/transport will then notify the application 
 * via the callback when ICE connectivity checks completes, either 
 * successfully or with failure.
 *
 * @param ice_st	The ICE stream transport.
 * @param rem_ufrag	Remote ufrag, as seen in the SDP received from 
 *			the remote agent.
 * @param rem_passwd	Remote password, as seen in the SDP received from
 *			the remote agent.
 * @param rcand_cnt	Number of remote candidates in the array.
 * @param rcand		Remote candidates array.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_start_ice(pj_ice_strans *ice_st,
					     const pj_str_t *rem_ufrag,
					     const pj_str_t *rem_passwd,
					     unsigned rcand_cnt,
					     const pj_ice_sess_cand rcand[]);

/**
 * Retrieve the candidate pair that has been nominated and successfully
 * checked for the specified component. If ICE negotiation is still in
 * progress or it has failed, this function will return NULL.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 *
 * @return		The valid pair as ICE checklist structure if the
 *			pair exist.
 */
PJ_DECL(const pj_ice_sess_check*) 
pj_ice_strans_get_valid_pair(const pj_ice_strans *ice_st,
			     unsigned comp_id);

/**
 * Stop and destroy the ICE session inside this media transport. Application
 * needs to call this function once the media session is over (the call has
 * been disconnected).
 *
 * Application MAY reuse this ICE stream transport for subsequent calls.
 * In this case, it must call #pj_ice_strans_stop_ice() when the call is
 * disconnected, and reinitialize the ICE stream transport for subsequent
 * call with #pj_ice_strans_init_ice()/#pj_ice_strans_start_ice(). In this
 * case, the ICE stream transport will maintain the internal sockets and
 * continue to send STUN keep-alive packets and TURN Refresh request to 
 * keep the NAT binding/TURN allocation open and to detect change in STUN
 * mapped address.
 *
 * If application does not want to reuse the ICE stream transport for
 * subsequent calls, it must call #pj_ice_strans_destroy() to destroy the
 * ICE stream transport altogether.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_stop_ice(pj_ice_strans *ice_st);


/**
 * Send outgoing packet using this transport. 
 * Application can send data (normally RTP or RTCP packets) at any time
 * by calling this function. This function takes a destination
 * address as one of the arguments, and this destination address should
 * be taken from the default transport address of the component (that is
 * the address in SDP c= and m= lines, or in a=rtcp attribute). 
 * If ICE negotiation is in progress, this function will send the data 
 * to the destination address. Otherwise if ICE negotiation has completed
 * successfully, this function will send the data to the nominated remote 
 * address, as negotiated by ICE.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 * @param data		The data or packet to be sent.
 * @param data_len	Size of data or packet, in bytes.
 * @param dst_addr	The destination address.
 * @param dst_addr_len	Length of destination address.
 *
 * @return		PJ_SUCCESS if data is sent successfully.
 */
PJ_DECL(pj_status_t) pj_ice_strans_sendto(pj_ice_strans *ice_st,
					  unsigned comp_id,
					  const void *data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *dst_addr,
					  int dst_addr_len);


/**
 * @}
 */


PJ_END_DECL



#endif	/* __PJNATH_ICE_STRANS_H__ */


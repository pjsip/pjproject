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
 * @addtogroup PJNATH_ICE_STREAM_TRANSPORT
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
 *
 * \section ice_strans_using_sec Using the ICE stream transport
 *
 * The steps below describe how to use ICE session:
 *
 *  - initialize a #pj_ice_strans_cfg structure. This contains various 
 *    settings for the ICE stream transport, and among other things contains
 *    the STUN and TURN settings.\n\n
 *  - create the instance with #pj_ice_strans_create(). Among other things,
 *    the function needs the following arguments:
 *	- the #pj_ice_strans_cfg structure for the main configurations
 *	- number of components to be supported
 *	- instance of #pj_ice_strans_cb structure to report callbacks to
 *	  application.\n\n
 *  - while the #pj_ice_strans_create() call completes immediately, the
 *    initialization will be running in the background to gather the 
 *    candidates (for example STUN and TURN candidates, if they are enabled
 *    in the #pj_ice_strans_cfg setting). Application will be notified when
 *    the initialization completes in the \a on_ice_complete callback of
 *    the #pj_ice_strans_cb structure (the \a op argument of this callback
 *    will be PJ_ICE_STRANS_OP_INIT).\n\n
 *  - when media stream is to be started (for example, a call is to be 
 *    started), create an ICE session by calling #pj_ice_strans_init_ice().\n\n
 *  - the application now typically will need to communicate local ICE
 *    information to remote host. It can achieve this by using the following
 *    functions to query local ICE information:
 *	- #pj_ice_strans_get_ufrag_pwd()
 *	- #pj_ice_strans_enum_cands()
 *	- #pj_ice_strans_get_def_cand()\n
 *    The application may need to encode the above information as SDP.\n\n
 *  - when the application receives remote ICE information (for example, from
 *    the SDP received from remote), it can now start ICE negotiation, by
 *    calling #pj_ice_strans_start_ice(). This function requires some
 *    information about remote ICE agent such as remote ICE username fragment
 *    and password as well as array of remote candidates.\n\n
 *  - note that the PJNATH library does not work with SDP; application would
 *    need to encode and parse the SDP itself.\n\n
 *  - once ICE negotiation has been started, application will be notified
 *    about the completion in the \a on_ice_complete() callback of the
 *    #pj_ice_strans_cb.\n\n
 *  - at any time, application may send or receive data. However the ICE
 *    stream transport may not be able to send it depending on its current
 *    state. Before ICE negotiation is started, the data will be sent using
 *    default candidate of the component. After negotiation is completed,
 *    data will be sent using the candidate from the successful/nominated
 *    pair. The ICE stream transport may not be able to send data while 
 *    negotiation is in progress.\n\n
 *  - application sends data by using #pj_ice_strans_sendto2(). Incoming
 *    data will be reported in \a on_rx_data() callback of the
 *    #pj_ice_strans_cb.\n\n
 *  - once the media session has finished (e.g. user hangs up the call),
 *    destroy the ICE session with #pj_ice_strans_stop_ice().\n\n
 *  - at this point, application may destroy the ICE stream transport itself,
 *    or let it run so that it can be reused to create other ICE session.
 *    The benefit of letting the ICE stream transport alive (without any
 *    session active) is to avoid delay with the initialization, howerver
 *    keeping the transport alive means the transport needs to keep the
 *    STUN binding open by using keep-alive and also TURN allocation alive,
 *    and this will consume power which is an important issue for mobile
 *    applications.\n\n
 */

/* Deprecated API pj_ice_strans_sendto() due to its limitations. See
 * below for more info and refer to
 * https://trac.pjsip.org/repos/ticket/2229 for more details.
 */
#ifndef DEPRECATED_FOR_TICKET_2229
#  define DEPRECATED_FOR_TICKET_2229	0
#endif

/** Forward declaration for ICE stream transport. */
typedef struct pj_ice_strans pj_ice_strans;

/** Transport operation types to be reported on \a on_status() callback */
typedef enum pj_ice_strans_op
{
    /** Initialization (candidate gathering) */
    PJ_ICE_STRANS_OP_INIT,

    /** Negotiation */
    PJ_ICE_STRANS_OP_NEGOTIATION,

    /** This operation is used to report failure in keep-alive operation.
     *  Currently it is only used to report TURN Refresh failure.
     */
    PJ_ICE_STRANS_OP_KEEP_ALIVE,

    /** IP address change notification from STUN keep-alive operation.
     */
    PJ_ICE_STRANS_OP_ADDR_CHANGE

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
     * This callback is optional and will be called to notify the status of
     * async send operations.
     *
     * @param ice_st	    The ICE stream transport.
     * @param sent	    If value is positive non-zero it indicates the
     *			    number of data sent. When the value is negative,
     *			    it contains the error code which can be retrieved
     *			    by negating the value (i.e. status=-sent).
     */
    void    (*on_data_sent)(pj_ice_strans *sock,
			    pj_ssize_t sent);

    /**
     * An optional callback that will be called by the ICE transport when a
     * valid pair has been found during ICE negotiation.
     *
     * @param ice_st	    The ICE stream transport.
     */
    void (*on_valid_pair)(pj_ice_strans *ice_st);

    /**
     * Callback to report status of various ICE operations.
     * 
     * @param ice_st	    The ICE stream transport.
     * @param op	    The operation which status is being reported.
     * @param status	    Operation status.
     */
    void    (*on_ice_complete)(pj_ice_strans *ice_st, 
			       pj_ice_strans_op op,
			       pj_status_t status);

    /**
     * Callback to report a new ICE local candidate, e.g: after successful
     * STUN Binding, after a successful TURN allocation. Only new candidates
     * whose type is server reflexive or relayed will be notified via this
     * callback. This callback also indicates end-of-candidate via parameter
     * 'last'.
     *
     * Trickle ICE can use this callback to convey the new candidate
     * to remote agent and monitor end-of-candidate indication.
     *
     * @param ice_st	    The ICE stream transport.
     * @param cand	    The new local candidate, can be NULL when the last
     *			    local candidate initialization failed/timeout.
     * @param end_of_cand   PJ_TRUE if this is the last of local candidate.
     */
    void    (*on_new_candidate)(pj_ice_strans *ice_st,
				const pj_ice_sess_cand *cand,
				pj_bool_t end_of_cand);

} pj_ice_strans_cb;


/**
 * STUN and local transport settings for ICE stream transport.
 */
typedef struct pj_ice_strans_stun_cfg
{
    /**
     * Address family, IPv4 or IPv6.
     *
     * Default value is pj_AF_INET() (IPv4)
     */
    int			 af;

    /**
     * Optional configuration for STUN transport. The default
     * value will be initialized with #pj_stun_sock_cfg_default().
     */
    pj_stun_sock_cfg	 cfg;

    /**
     * Maximum number of host candidates to be added. If the
     * value is zero, no host candidates will be added.
     *
     * Default: 64
     */
    unsigned		 max_host_cands;

    /**
     * Include loopback addresses in the host candidates.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t		 loop_addr;

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
     * If there are more than one STUN candidates per ICE stream
     * transport component, the standard recommends to use the same
     * STUN server for all STUN candidates.
     *
     * The default value is empty.
     */
    pj_str_t		 server;

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
    pj_uint16_t		 port;

    /**
     * Ignore STUN resolution error and proceed with just local
     * addresses.
     *
     * The default is PJ_FALSE
     */
    pj_bool_t		 ignore_stun_error;

} pj_ice_strans_stun_cfg;


/**
 * TURN transport settings for ICE stream transport.
 */
typedef struct pj_ice_strans_turn_cfg
{
    /**
     * Address family, IPv4 or IPv6.
     *
     * Default value is pj_AF_INET() (IPv4)
     */
    int			 af;

    /**
     * Optional TURN socket settings. The default values will be
     * initialized by #pj_turn_sock_cfg_default(). This contains
     * settings such as QoS.
     */
    pj_turn_sock_cfg	 cfg;

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
    pj_str_t		 server;

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
    pj_uint16_t		 port;

    /**
     * Type of connection to the TURN server.
     *
     * Default is PJ_TURN_TP_UDP.
     */
    pj_turn_tp_type	 conn_type;

    /**
     * Credential to be used for the TURN session. This setting
     * is mandatory.
     *
     * Default is to have no credential.
     */
    pj_stun_auth_cred	 auth_cred;

    /**
     * Optional TURN Allocate parameter. The default value will be
     * initialized by #pj_turn_alloc_param_default().
     */
    pj_turn_alloc_param	 alloc_param;

} pj_ice_strans_turn_cfg;


/**
 * This structure describes ICE stream transport configuration. Application
 * should initialize the structure by calling #pj_ice_strans_cfg_default()
 * before changing the settings.
 */
typedef struct pj_ice_strans_cfg
{
    /**
     * The address family which will be used as the default address
     * in the SDP offer. Setting this to pj_AF_UNSPEC() means that
     * the address family will not be considered during the process
     * of default candidate selection.
     *
     * The default value is pj_AF_INET() (IPv4).
     */
    int			 af;

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
     * This contains various STUN session options. Once the ICE stream
     * transport is created, application may also change the options
     * with #pj_ice_strans_set_options().
     */
    pj_ice_sess_options	 opt;

    /**
     * Warning: this field is deprecated, please use \a stun_tp field instead.
     * To maintain backward compatibility, if \a stun_tp_cnt is zero, the
     * value of this field will be copied to \a stun_tp.
     *
     * STUN and local transport settings. This specifies the settings
     * for local UDP socket address and STUN resolved address.
     */
    pj_ice_strans_stun_cfg stun;

    /**
     * Number of STUN transports.
     *
     * Default: 0
     */
    unsigned		 stun_tp_cnt;

    /**
     * STUN and local transport settings. This specifies the settings
     * for local UDP socket address and STUN resolved address.
     */
    pj_ice_strans_stun_cfg stun_tp[PJ_ICE_MAX_STUN];

    /**
     * Warning: this field is deprecated, please use \a turn_tp field instead.
     * To maintain backward compatibility, if \a turn_tp_cnt is zero, the
     * value of this field will be copied to \a turn_tp.
     *
     * TURN transport settings.
     */
    pj_ice_strans_turn_cfg turn;

    /**
     * Number of TURN transports.
     *
     * Default: 0
     */
    unsigned		 turn_tp_cnt;

    /**
     * TURN transport settings.
     */
    pj_ice_strans_turn_cfg turn_tp[PJ_ICE_MAX_TURN];

    /**
     * Number of send buffers used for pj_ice_strans_sendto2(). If the send
     * buffers are full, pj_ice_strans_sendto()/sendto2() will return
     * PJ_EBUSY.
     *
     * Set this to 0 to disable buffering (then application will have to
     * maintain the buffer passed to pj_ice_strans_sendto()/sendto2()
     * until it has been sent).
     *
     * Default: 4
     */
    unsigned 		 num_send_buf;

    /**
     * Buffer size used for pj_ice_strans_sendto2().
     *
     * Default: 0 (size determined by the size of the first packet sent).
     */
    unsigned 		 send_buf_size;

    /**
     * Component specific settings, which will override the settings in
     * the STUN and TURN settings above. For example, setting the QoS
     * parameters here allows the application to have different QoS
     * traffic type for RTP and RTCP component.
     */
    struct {
	/**
	 * QoS traffic type to be set on this transport. When application
	 * wants to apply QoS tagging to the transport, it's preferable to
	 * set this field rather than \a qos_param fields since this is 
	 * more portable.
	 *
	 * Default value is PJ_QOS_TYPE_BEST_EFFORT.
	 */
	pj_qos_type qos_type;

	/**
	 * Set the low level QoS parameters to the transport. This is a 
	 * lower level operation than setting the \a qos_type field and
	 * may not be supported on all platforms.
	 *
	 * By default all settings in this structure are disabled.
	 */
	pj_qos_params qos_params;

	/**
	 * Specify target value for socket receive buffer size. It will be
	 * applied using setsockopt(). When it fails to set the specified
	 * size, it will try with lower value until the highest possible is
	 * successfully set.
	 *
	 * When this is set to zero, this component will apply socket receive
	 * buffer size settings specified in STUN and TURN socket config
	 * above, i.e: \a stun::cfg::so_rcvbuf_size and
	 * \a turn::cfg::so_rcvbuf_size. Otherwise, this setting will be
	 * applied to STUN and TURN sockets for this component, overriding
	 * the setting specified in STUN/TURN socket config.
	 *
	 * Default: 0
	 */
	unsigned so_rcvbuf_size;

	/**
	 * Specify target value for socket send buffer size. It will be
	 * applied using setsockopt(). When it fails to set the specified
	 * size, it will try with lower value until the highest possible is
	 * successfully set.
	 *
	 * When this is set to zero, this component will apply socket send
	 * buffer size settings specified in STUN and TURN socket config
	 * above, i.e: \a stun::cfg::so_sndbuf_size and
	 * \a turn::cfg::so_sndbuf_size. Otherwise, this setting will be
	 * applied to STUN and TURN sockets for this component, overriding
	 * the setting specified in STUN/TURN socket config.
	 *
	 * Default: 0
	 */
	unsigned so_sndbuf_size;

    } comp[PJ_ICE_MAX_COMP];

} pj_ice_strans_cfg;


/**
 * ICE stream transport's state.
 */
typedef enum pj_ice_strans_state
{
    /**
     * ICE stream transport is not created.
     */
    PJ_ICE_STRANS_STATE_NULL,

    /**
     * ICE candidate gathering process is in progress.
     */
    PJ_ICE_STRANS_STATE_INIT,

    /**
     * ICE stream transport initialization/candidate gathering process is
     * complete, ICE session may be created on this stream transport.
     */
    PJ_ICE_STRANS_STATE_READY,

    /**
     * New session has been created and the session is ready.
     */
    PJ_ICE_STRANS_STATE_SESS_READY,

    /**
     * ICE negotiation is in progress.
     */
    PJ_ICE_STRANS_STATE_NEGO,

    /**
     * ICE negotiation has completed successfully and media is ready
     * to be used.
     */
    PJ_ICE_STRANS_STATE_RUNNING,

    /**
     * ICE negotiation has completed with failure.
     */
    PJ_ICE_STRANS_STATE_FAILED

} pj_ice_strans_state;


/** 
 * Initialize ICE transport configuration with default values.
 *
 * @param cfg		The configuration to be initialized.
 */
PJ_DECL(void) pj_ice_strans_cfg_default(pj_ice_strans_cfg *cfg);


/** 
 * Initialize ICE STUN transport configuration with default values.
 *
 * @param cfg		The configuration to be initialized.
 */
PJ_DECL(void) pj_ice_strans_stun_cfg_default(pj_ice_strans_stun_cfg *cfg);


/** 
 * Initialize ICE TURN transport configuration with default values.
 *
 * @param cfg		The configuration to be initialized.
 */
PJ_DECL(void) pj_ice_strans_turn_cfg_default(pj_ice_strans_turn_cfg *cfg);


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
 * Get ICE session state.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		ICE session state.
 */
PJ_DECL(pj_ice_strans_state) pj_ice_strans_get_state(pj_ice_strans *ice_st);


/**
 * Get string representation of ICE state.
 *
 * @param state		ICE stream transport state.
 *
 * @return		String.
 */
PJ_DECL(const char*) pj_ice_strans_state_name(pj_ice_strans_state state);


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
 * Get the value of various options of the ICE stream transport.
 *
 * @param ice_st	The ICE stream transport.
 * @param opt		The options to be initialized with the values
 *			from the ICE stream transport.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_strans_get_options(pj_ice_strans *ice_st,
					       pj_ice_sess_options *opt);

/**
 * Specify various options for this ICE stream transport. Application 
 * should call #pj_ice_strans_get_options() to initialize the options 
 * with their default values.
 *
 * @param ice_st	The ICE stream transport.
 * @param opt		Options to be applied to this ICE stream transport.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_strans_set_options(pj_ice_strans *ice_st,
					       const pj_ice_sess_options *opt);

/**
 * Update number of components of the ICE stream transport. This can only
 * reduce the number of components from the initial value specified in
 * pj_ice_strans_create() and before ICE session is initialized.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_cnt	Number of components.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_strans_update_comp_cnt(pj_ice_strans *ice_st,
						   unsigned comp_cnt);

/**
 * Get the group lock for this ICE stream transport.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		The group lock.
 */
PJ_DECL(pj_grp_lock_t *) pj_ice_strans_get_grp_lock(pj_ice_strans *ice_st);

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
 * Check if the ICE stream transport has the ICE session created. The
 * ICE session is created with #pj_ice_strans_init_ice().
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_TRUE if #pj_ice_strans_init_ice() has been
 *			called.
 */
PJ_DECL(pj_bool_t) pj_ice_strans_has_sess(pj_ice_strans *ice_st);


/**
 * Check if ICE negotiation is still running.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_TRUE if ICE session has been created and ICE 
 *			negotiation negotiation is in progress.
 */
PJ_DECL(pj_bool_t) pj_ice_strans_sess_is_running(pj_ice_strans *ice_st);


/**
 * Check if ICE negotiation has completed.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_TRUE if ICE session has been created and the
 *			negotiation is complete.
 */
PJ_DECL(pj_bool_t) pj_ice_strans_sess_is_complete(pj_ice_strans *ice_st);


/**
 * Get the current/running component count. If ICE negotiation has not
 * been started, the number of components will be equal to the number
 * when the ICE stream transport was created. Once negotiation been
 * started, the number of components will be the lowest number of 
 * component between local and remote agents.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		The running number of components.
 */
PJ_DECL(unsigned) pj_ice_strans_get_running_comp_cnt(pj_ice_strans *ice_st);


/**
 * Get the ICE username fragment and password of the ICE session. The
 * local username fragment and password can only be retrieved once ICE
 * session has been created with #pj_ice_strans_init_ice(). The remote
 * username fragment and password can only be retrieved once ICE session
 * has been started with #pj_ice_strans_start_ice().
 *
 * Note that the string returned by this function is only valid throughout
 * the duration of the ICE session, and the application must not modify
 * these strings. Once the ICE session has been stopped with
 * #pj_ice_strans_stop_ice(), the pointer in the string will no longer be
 * valid.
 *
 * @param ice_st	The ICE stream transport.
 * @param loc_ufrag	Optional pointer to receive ICE username fragment
 *			of local endpoint from the ICE session.
 * @param loc_pwd	Optional pointer to receive ICE password of local
 *			endpoint from the ICE session.
 * @param rem_ufrag	Optional pointer to receive ICE username fragment
 *			of remote endpoint from the ICE session.
 * @param rem_pwd	Optional pointer to receive ICE password of remote
 *			endpoint from the ICE session.
 *
 * @return		PJ_SUCCESS if the strings have been retrieved
 *			successfully.
 */
PJ_DECL(pj_status_t) pj_ice_strans_get_ufrag_pwd(pj_ice_strans *ice_st,
						 pj_str_t *loc_ufrag,
						 pj_str_t *loc_pwd,
						 pj_str_t *rem_ufrag,
						 pj_str_t *rem_pwd);


/**
 * Get the number of local candidates for the specified component ID.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 *
 * @return		The number of candidates.
 */
PJ_DECL(unsigned) pj_ice_strans_get_cands_count(pj_ice_strans *ice_st,
					        unsigned comp_id);

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
 * Update check list after receiving new remote ICE candidates or after
 * new local ICE candidates are found and conveyed to remote. This function
 * can also be called after receiving end of candidate indication from
 * either remote or local agent.
 *
 * This function is only applicable when trickle ICE is not disabled and
 * after ICE session has been created using pj_ice_strans_init_ice().
 *
 * @param ice_st	The ICE stream transport.
 * @param rem_ufrag	Remote ufrag, as seen in the SDP received from
 *			the remote agent.
 * @param rem_passwd	Remote password, as seen in the SDP received from
 *			the remote agent.
 * @param rcand_cnt	Number of new remote candidates in the array.
 * @param rcand		New remote candidates array.
 * @param rcand_end	Set to PJ_TRUE if remote has signalled
 *			end-of-candidate.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_update_check_list(
					     pj_ice_strans *ice_st,
					     const pj_str_t *rem_ufrag,
					     const pj_str_t *rem_passwd,
					     unsigned rcand_cnt,
					     const pj_ice_sess_cand rcand[],
					     pj_bool_t rcand_end);

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


#if !DEPRECATED_FOR_TICKET_2229
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
 * Limitations:
 * 1. This function cannot inform the app whether the data has been sent,
 *    or currently still pending.
 * 2. In case that the data is still pending, the application has no way
 *    of knowing the status of the send operation (whether it's a success
 *    or failure).
 * Due to these limitations, the API is deprecated and will be removed
 * in the future.
 *
 * Note that application shouldn't mix using pj_ice_strans_sendto() and
 * pj_ice_strans_sendto2() to avoid inconsistent calling of
 * on_data_sent() callback.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 * @param data		The data or packet to be sent.
 * @param data_len	Size of data or packet, in bytes.
 * @param dst_addr	The destination address.
 * @param dst_addr_len	Length of destination address.
 *
 * @return		PJ_SUCCESS if data has been sent, or will be sent
 *			later. No callback will be called.
 */
PJ_DECL(pj_status_t) pj_ice_strans_sendto(pj_ice_strans *ice_st,
					  unsigned comp_id,
					  const void *data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *dst_addr,
					  int dst_addr_len);
#endif


/**
 * Send outgoing packet using this transport.
 * Application can send data (normally RTP or RTCP packets) at any time
 * by calling this function. This function takes a destination
 * address as one of the arguments, and this destination address should
 * be taken from the default transport address of the component (that is
 * the address in SDP c= and m= lines, or in a=rtcp attribute).
 * If ICE negotiation is in progress, this function will try to send the data
 * via any valid candidate pair (which has passed ICE connectivity test).
 * If ICE negotiation has completed successfully, this function will send
 * the data to the nominated remote address, as negotiated by ICE.
 * If the ICE negotiation fails or valid candidate pair is not yet available,
 * this function will send the data using default candidate to the specified
 * destination address.
 *
 * Note that application shouldn't mix using pj_ice_strans_sendto() and
 * pj_ice_strans_sendto2() to avoid inconsistent calling of
 * on_data_sent() callback.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 * @param data		The data or packet to be sent.
 * @param data_len	Size of data or packet, in bytes.
 * @param dst_addr	The destination address.
 * @param dst_addr_len	Length of destination address.
 *
 * @return		PJ_SUCCESS if data has been sent, or
 *		    	PJ_EPENDING if data cannot be sent immediately. In
 *		    	this case the \a on_data_sent() callback will be
 *		    	called when data is actually sent. Any other return
 *		    	value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_ice_strans_sendto2(pj_ice_strans *ice_st,
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


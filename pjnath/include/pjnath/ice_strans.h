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
#ifndef __PJNATH_ICE_STREAM_TRANSPORT_H__
#define __PJNATH_ICE_STREAM_TRANSPORT_H__


/**
 * @file ice_strans.h
 * @brief ICE Stream Transport
 */
#include <pjnath/ice_session.h>
#include <pjlib-util/resolver.h>
#include <pj/ioqueue.h>
#include <pj/timer.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJNATH_ICE_STREAM_TRANSPORT ICE Stream Transport
 * @brief Transport for media stream using ICE
 * @ingroup PJNATH_ICE
 * @{
 *
 * This module describes ICE stream transport, as represented by #pj_ice_strans
 * structure, and is part of PJNATH - the Open Source NAT traversal helper
 * library.
 *
 * ICE stream transport, as represented by #pj_ice_strans structure, is an ICE
 * capable component for transporting media within a media stream. 
 * It consists of one or more transport sockets (typically two for RTP
 * based communication - one for RTP and one for RTCP), and an 
 * \ref PJNATH_ICE_SESSION for performing connectivity checks among the.
 * various candidates of the transport addresses.
 *
 * \section PJNATH_ICE_STREAM_TRANSPORT_USING Using the ICE Stream Transport
 *
 * Application may use the ICE stream transport in two ways:
 *  - it can create the ICE stream transports once during application 
 *    initialization and keep them alive throughout application lifetime, or
 *  - it can create and destroy the ICE stream transport as needed everytime 
 *     a call is made and destroyed. 
 *
 * Keeping the ICE stream transport alive throughout
 * application's lifetime is normally preferable, as initializing the
 * ICE stream transport may incur delay because the ICE stream transport
 * would need to communicate with the STUN/TURN server to get the
 * server reflexive and relayed candidates for the transports.
 *
 * Regardless of which usage scenario is being used, the ICE stream
 * transport is capable for restarting the ICE session being used and to
 * send STUN keep-alives for its STUN server reflexive and relayed
 * candidates.
 *
 * \subsection PJNATH_ICE_ST_TRA_INIT Stream Transport Initialization
 *
 * Application creates the ICE stream transport by calling 
 * #pj_ice_strans_create() function.
 *
 * After the ICE stream transport is created, application may set up the
 * STUN servers to be used to obtain STUN server reflexive and relayed
 * candidate, by calling #pj_ice_strans_set_stun_domain() or 
 * #pj_ice_strans_set_stun_srv(). Then it has to create each component by
 * calling #pj_ice_strans_create_comp(); this would create an actual socket
 * which listens to the specified local address, and it would also
 * perform lookup to find various transport address candidates for this
 * socket.
 * 
 * \subsection PJNATH_ICE_ST_TRA_INIT_ICE ICE Session Initialization
 *
 * When application is about to send an offer containing ICE capability,
 * or when it receives an offer containing ICE capability, it would
 * create the ICE session by calling #pj_ice_strans_init_ice(). This would
 * register all transport address aliases for each component to the ICE
 * session as candidates. After this application can enumerate all local
 * candidates by calling #pj_ice_strans_enum_cands(), and encode these
 * candidates in the SDP to be sent to remote agent.
 *
 * \subsection PJNATH_ICE_ST_TRA_START Starting Connectivity Checks
 *
 * Once application receives the SDP from remote, it can start ICE
 * connectivity checks by calling #pj_ice_strans_start_ice(), specifying
 * the username, password, and candidates of the remote agent. The ICE
 * session/transport will then notify the application via the callback
 * when ICE connectivity checks completes, either successfully or with
 * failure.
 *
 * \subsection PJNATH_ICE_ST_TRA_STOP Stopping ICE Session
 *
 * Once the call is terminated, application no longer needs to keep the
 * ICE session, so it should call #pj_ice_strans_stop_ice() to destroy the
 * ICE session within this ICE stream transport. Note that this WILL NOT
 * destroy the sockets/transports, it only destroys the ICE session
 * within this ICE stream transport.
 *
 * \subsection PJNATH_ICE_ST_TRA_RESTART Restarting ICE Session
 *
 * When a new call is made, application can repeat the above
 * #pj_ice_strans_init_ice() to #pj_ice_strans_stop_ice() cycle for 
 * the new call, using this same ICE stream transport.
 *
 * \subsection PJNATH_ICE_ST_TRA_DESTROY Destroying ICE Stream Transport
 *
 * Finally, when the ICE stream transport itself is no longer needed,
 * for example when the application quits, application should call
 * #pj_ice_strans_destroy() to release back all resources allocated by this
 * ICE stream transport.
 *
 */

/** Forward declaration for ICE stream transport. */
typedef struct pj_ice_strans pj_ice_strans;

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
     * This callback will be called when ICE checks have completed.
     * This callback is optional.
     * 
     * @param ice_st	    The ICE stream transport.
     * @param status	    The ICE connectivity check status.
     */
    void    (*on_ice_complete)(pj_ice_strans *ice_st, 
			       pj_status_t status);

} pj_ice_strans_cb;


/**
 * Various flags that can be specified when creating a component with
 * #pj_ice_strans_create_comp(). These options may be combined together
 * with bitmask operation.
 */
enum pj_ice_strans_option
{
    /**
     * If this option is specified, only a listening socket will be
     * created for the component, and no candidate will be added to
     * the component. Application must add the component manually
     * by inspecting the socket and transport address of the component.
     */
    PJ_ICE_ST_OPT_DONT_ADD_CAND = 1,

    /**
     * If this option is specified, then no STUN reflexive candidate
     * will be added to the component.
     */
    PJ_ICE_ST_OPT_DISABLE_STUN	= 2,

    /**
     * If this option is specified, then no STUN relay candidate
     * will be added to the component.
     */
    PJ_ICE_ST_OPT_DISABLE_RELAY	= 4,

    /**
     * If this option is specified, then when the function fails to
     * bind the socket to the specified port, it WILL NOT try to
     * bind the socket to the next available port.
     *
     * If this option is NOT specified, then the function will try to
     * bind the socket to next port+2, repetitively until the socket
     * is bound successfully.
     */
    PJ_ICE_ST_OPT_NO_PORT_RETRY	= 8,
};


/**
 * This structure describes ICE stream transport candidate. A "candidate"
 * in ICE stream transport can be viewed as alias transport address
 * for the socket.
 */
typedef struct pj_ice_strans_cand
{
    /**
     * Candidate type.
     */
    pj_ice_cand_type	type;

    /** 
     * Status of this candidate. This status is useful for ICE reflexive
     * and relay candidate, where the address needs to be resolved 
     * asynchronously by sending STUN request to STUN server.
     *
     * The value will be PJ_SUCCESS if candidate address has been resolved
     * successfully, PJ_EPENDING when the address resolution process is
     * in progress, or other value when the address resolution has
     * completed with failure.
     */
    pj_status_t		status;

    /**
     * The candidate transport address.
     */
    pj_sockaddr		addr;

    /**
     * The ICE session candidate ID after this candidate has been registered
     * to an ICE session. Before ICE session is created, or after ICE
     * session has been destroyed, the value will be -1.
     */
    int			ice_cand_id;

    /**
     * Local preference value, which typically is 65535.
     */
    pj_uint16_t		local_pref;

    /**
     * Foundation associated with this candidate, which value normally will be
     * calculated by the function.
     */
    pj_str_t		foundation;

} pj_ice_strans_cand;


/**
 * This structure describes an ICE stream transport component. A component
 * in ICE stream transport typically corresponds to a single socket created
 * for this component, and bound to a specific transport address. This
 * component may have multiple alias addresses, for example one alias 
 * address for each interfaces in multi-homed host, another for server
 * reflexive alias, and another for relayed alias. For each transport
 * address alias, an ICE stream transport candidate (#pj_ice_strans_cand) will
 * be created, and these candidates will eventually registered to the ICE
 * session.
 */
typedef struct pj_ice_strans_comp
{
    pj_ice_strans	*ice_st;	/**< ICE stream transport.	*/
    unsigned		 comp_id;	/**< Component ID.		*/
    pj_uint32_t		 options;	/**< Option flags.		*/
    pj_sock_t		 sock;		/**< Socket descriptor.		*/

    pj_stun_session	*stun_sess;	/**< STUN session.		*/

    pj_sockaddr		 local_addr;	/**< Local/base address.	*/

    unsigned		 pending_cnt;	/**< Pending resolution cnt.	*/
    pj_status_t		 last_status;	/**< Last status.		*/

    unsigned		 cand_cnt;	/**< # of candidates/aliaes.	*/
    pj_ice_strans_cand	 cand_list[PJ_ICE_ST_MAX_CAND];	/**< Cand array	*/
    int			 default_cand;	/**< Default candidate selected	*/

    pj_ioqueue_key_t	*key;		/**< ioqueue key.		*/
    pj_uint8_t		 pkt[1500];	/**< Incoming packet buffer.	*/
    pj_ioqueue_op_key_t	 read_op;	/**< ioqueue read operation key	*/
    pj_ioqueue_op_key_t	 write_op;	/**< ioqueue write op. key	*/
    pj_sockaddr		 src_addr;	/**< source packet address buf.	*/
    int			 src_addr_len;	/**< length of src addr. buf.	*/

} pj_ice_strans_comp;


/**
 * This structure represents the ICE stream transport.
 */
struct pj_ice_strans
{
    char		     obj_name[PJ_MAX_OBJ_NAME];	/**< Log ID.	*/

    pj_pool_t		    *pool;	/**< Pool used by this object.	*/
    void		    *user_data;	/**< Application data.		*/
    pj_stun_config	     stun_cfg;	/**< STUN settings.		*/
    pj_ice_strans_cb	     cb;	/**< Application callback.	*/

    pj_ice_sess		    *ice;	/**< ICE session.		*/

    unsigned		     comp_cnt;	/**< Number of components.	*/
    pj_ice_strans_comp	   **comp;	/**< Components array.		*/

    pj_dns_resolver	    *resolver;	/**< The resolver instance.	*/
    pj_bool_t		     has_rjob;	/**< Has pending resolve?	*/
    pj_sockaddr_in	     stun_srv;	/**< STUN server address.	*/
    pj_sockaddr_in	     turn_srv;	/**< TURN server address.	*/

    pj_timer_entry	     ka_timer;	/**< STUN keep-alive timer.	*/
};


/**
 * Create the ICE stream transport containing the specified number of
 * components. After the ICE stream transport is created, application
 * may initialize the STUN server settings, and after that it has to 
 * initialize each components by calling #pj_ice_strans_create_comp()
 * function.
 *
 * @param stun_cfg	The STUN settings.
 * @param name		Optional name for logging identification.
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
PJ_DECL(pj_status_t) pj_ice_strans_create(pj_stun_config *stun_cfg,
					  const char *name,
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
 * Set the domain to be used when resolving the STUN servers. If application
 * wants to utillize STUN, then STUN server must be specified, either by
 * calling this function or by calling #pj_ice_strans_set_stun_srv().
 *
 * If application calls this function, then the STUN/TURN servers will
 * be resolved by querying DNS SRV records for the specified domain.
 *
 * @param ice_st	The ICE stream transport.
 * @param resolver	The resolver instance that will be used to
 *			resolve the STUN/TURN servers.
 * @param domain	The target domain.
 *
 * @return		PJ_SUCCESS if DNS SRV resolution job can be
 *			started. The resolution process itself will
 *			complete asynchronously.
 */
PJ_DECL(pj_status_t) pj_ice_strans_set_stun_domain(pj_ice_strans *ice_st,
						   pj_dns_resolver *resolver,
						   const pj_str_t *domain);

/**
 * Set the STUN and TURN server addresses. If application
 * wants to utillize STUN, then STUN server must be specified, either by
 * calling this function or by calling #pj_ice_strans_set_stun_domain().
 *
 * With this function, the STUN and TURN server addresses will be 
 * assigned immediately, that is no DNS resolution will need to be 
 * performed.
 *
 * @param ice_st	The ICE stream transport.
 * @param stun_srv	The STUN server address, or NULL if STUN
 *			reflexive candidate is not to be used.
 * @param turn_srv	The TURN server address, or NULL if STUN
 *			relay candidate is not to be used.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) 
pj_ice_strans_set_stun_srv( pj_ice_strans *ice_st,
			    const pj_sockaddr_in *stun_srv,
			    const pj_sockaddr_in *turn_srv);

/**
 * Create and initialize the specified component. This function will
 * instantiate the socket descriptor for this component, optionally
 * bind the socket to the specified address (or bind to any address/port
 * if the \a addr parameter is NULL), and start finding all alias
 * addresses for this socket. For each alias addresses that if finds,
 * it will add an ICE stream transport candidate for this component.
 *
 * After all components have been initialized, application should poll
 * the #pj_ice_strans_get_comps_status() peridically to check if STUN
 * server reflexive and relayed candidates have been obtained
 * successfully.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	The component ID, which value must be greater than
 *			zero and less than or equal to the number of 
 *			components in this ICE stream transport.
 * @param options	Options, see #pj_ice_strans_option.
 * @param addr		Local address where socket will be bound to. This
 *			address will be used as follows:
 *			- if the value is NULL, then socket will be bound
 *			  to any available port.
 *			- if the value is not NULL, then if the port number
 *			  is not zero, it will used as the starting port 
 *			  where the socket will be bound to. If bind() to
 *			  this port fails, this function will try to bind
 *			  to port+2, repeatedly until it succeeded.
 *			  If application doesn't want this function to 
 *			  retry binding the socket to other port, it can
 *			  specify PJ_ICE_ST_OPT_NO_PORT_RETRY option.
 *			- if the value is not NULL, then if the address
 *			  is not INADDR_ANY, this function will bind the
 *			  socket to this particular interface only, and
 *			  no other host candidates will be added for this
 *			  socket.
 *			
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_create_comp(pj_ice_strans *ice_st,
					       unsigned comp_id,
					       pj_uint32_t options,
					       const pj_sockaddr_in *addr);

/**
 * Manually add a candidate (transport address alias) for the specified
 * component. Normally application shouldn't need to use this function,
 * as candidates will be added automatically when component is created
 * with #pj_ice_strans_create_comp().
 *
 * @param ice_st	ICE stream transport.
 * @param comp_id	The component ID.
 * @param type		The candidate type.
 * @param local_pref	The local preference for this candidate
 *			(typically the value is 65535).
 * @param addr		The candidate address.
 * @param set_default	Set to non-zero to make this candidate the 
 *			default candidate for this component.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_add_cand(pj_ice_strans *ice_st,
					    unsigned comp_id,
					    pj_ice_cand_type type,
					    pj_uint16_t local_pref,
					    const pj_sockaddr_in *addr,
					    pj_bool_t set_default);

/**
 * Get the status of components in the ICE stream transports. Since
 * some IP address candidates have to be obtained asynchronously (for
 * example, the STUN reflexive or relay candidate), application can
 * use this function to know whether the address resolution has 
 * completed.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_SUCCESS if all candidates have been resolved
 *			successfully, PJ_EPENDING if transport resolution
 *			is still in progress, or other status on failure.
 */
PJ_DECL(pj_status_t) pj_ice_strans_get_comps_status(pj_ice_strans *ice_st);

/**
 * Initialize the ICE session in the ICE stream transport.
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
 * Enumerate the local candidates. This function can only be called
 * after the ICE session has been created in the ICE stream transport.
 *
 * @param ice_st	The ICE stream transport.
 * @param count		On input, it specifies the maximum number of
 *			elements. On output, it will be filled with
 *			the number of candidates copied to the
 *			array.
 * @param cand		Array of candidates.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_enum_cands(pj_ice_strans *ice_st,
					      unsigned *count,
					      pj_ice_sess_cand cand[]);

/**
 * Start ICE connectivity checks. This function can only be called
 * after the ICE session has been created in the ICE stream transport.
 *
 * This function will pair the local and remote candidates to create 
 * check list. Once the check list is created and sorted based on the
 * priority, ICE periodic checks will be started. This function will 
 * return immediately, and application will be notified about the 
 * connectivity check status in the callback.
 *
 * @param ice_st	The ICE stream transport.
 * @param rem_ufrag	Remote ufrag, as seen in the SDP received from 
 *			the remote agent.
 * @param rem_passwd	Remote password, as seen in the SDP received from
 *			the remote agent.
 * @param rem_cand_cnt	Number of remote candidates.
 * @param rem_cand	Remote candidate array.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) 
pj_ice_strans_start_ice( pj_ice_strans *ice_st,
			 const pj_str_t *rem_ufrag,
			 const pj_str_t *rem_passwd,
			 unsigned rem_cand_cnt,
			 const pj_ice_sess_cand rem_cand[]);

/**
 * Stop and destroy the ICE session inside this media transport.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_stop_ice(pj_ice_strans *ice_st);


/**
 * Send outgoing packet using this transport. If ICE checks have not 
 * produced a valid check for the specified component ID, this function 
 * will return with failure. Otherwise it will send the packet to remote
 * destination using the nominated local candidate as have been checked
 * previously.
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



#endif	/* __PJNATH_ICE_STREAM_TRANSPORT_H__ */


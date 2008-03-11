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
#ifndef __PJMEDIA_TRANSPORT_H__
#define __PJMEDIA_TRANSPORT_H__


/**
 * @file transport.h Media Transport Interface
 * @brief Transport interface.
 */

#include <pjmedia/types.h>
#include <pjmedia/errno.h>

/**
 * @defgroup PJMEDIA_TRANSPORT Media Transports
 * @ingroup PJMEDIA
 * @brief Transports.
 * Transport related components.
 */

/**
 * @defgroup PJMEDIA_TRANSPORT_H Media Network Transport Interface
 * @ingroup PJMEDIA_TRANSPORT
 * @brief PJMEDIA object for sending/receiving media packets over the network
 * @{
 * The media transport (#pjmedia_transport) is the object to send and
 * receive media packets over the network. The media transport interface
 * allows the library to be extended to support different types of 
 * transports to send and receive packets. Currently only the standard
 * UDP transport implementation is provided (see \ref PJMEDIA_TRANSPORT_UDP),
 * but application designer may extend the library to support other types
 * of custom transports such as RTP/RTCP over TCP, RTP/RTCP over HTTP, etc.
 *
 * The media transport is declared as #pjmedia_transport "class", which
 * declares "interfaces" to use the class in #pjmedia_transport_op
 * structure. For the user of the media transport (normally the user of
 * media transport is media stream, see \ref PJMED_STRM), these transport
 * "methods" are wrapped with API such as #pjmedia_transport_attach(),
 * so it should not need to call the function pointer inside 
 * #pjmedia_transport_op directly.
 *
 * The connection between \ref PJMED_STRM and media transport is shown in
 * the diagram below:

   \image html media-transport.PNG


 * \section PJMEDIA_TRANSPORT_H_USING Using the Media Transport
 *
 * The media transport's life-cycle normally follows the following stages.
 *
 * \subsection PJMEDIA_TRANSPORT_H_CREATE Creating the Media Transport
 *
 *  Application creates the media transport when it needs to establish
 *    media session to remote peer. The media transport is created using
 *    specific function to create that particular transport; for example,
 *    for UDP media transport, it is created with #pjmedia_transport_udp_create()
 *    or #pjmedia_transport_udp_create2() functions. Different media
 *    transports will provide different API to create those transports.
 *
 *  Alternatively, application may create pool of media transports when
 *    it is first started up. Using this approach probably is better, since
 *    application has to specify the RTP port when sending the initial
 *    session establishment request (e.g. SIP INVITE request), thus if
 *    application only creates the media transport later when media is to be
 *    established (normally when 200/OK is received, or when 18x is received
 *    for early media), there is a possibility that the particular RTP
 *    port might have been occupied by other programs. Also it is more
 *    efficient since sockets don't need to be closed and re-opened between
 *    calls.
 *
 *
 * \subsection PJMEDIA_TRANSPORT_H_ATTACH Attaching and Using the Media Transport.
 *
 *  Application specifies the media transport instance when creating
 *    the media session (#pjmedia_session_create()). Alternatively, it
 *    may create the media stream directly with #pjmedia_stream_create()
 *    and specify the transport instance in the argument. (Note: media
 *    session is a high-level abstraction for media communications between
 *    two endpoints, and it may contain more than one media streams, for
 *    example, an audio stream and a video stream).
 *
 *  When stream is created, it will "attach" itself to the media 
 *    transport by calling #pjmedia_transport_attach(), which is a thin
 *    wrapper which calls "attach()" method of the media transport's 
 *    "virtual function pointer" (#pjmedia_transport_op). Among other things,
 *    the stream specifies two callback functions to the transport: one
 *    callback function will be called by transport when it receives RTP
 *    packet, and another callback for incoming RTCP packet. The 
 *    #pjmedia_transport_attach() function also establish the destination
 *    of the outgoing RTP and RTCP packets.
 *
 *  When the stream needs to send outgoing RTP/RTCP packets, it will
 *    call #pjmedia_transport_send_rtp() and #pjmedia_transport_send_rtcp()
 *    of the media transport API, which is a thin wrapper to call send_rtp() 
 *    and send_rtcp() methods in the media transport's "virtual function 
 *    pointer"  (#pjmedia_transport_op).
 *
 *  When the stream is destroyed, it will "detach" itself from
 *    the media transport by calling #pjmedia_transport_detach(), which is
 *    a thin wrapper which calls "detach()" method of the media transport's 
 *    "virtual function pointer" (#pjmedia_transport_op). After the transport
 *    is detached from its user (the stream), it will no longer report 
 *    incoming RTP/RTCP packets to the stream, and it will refuse to send
 *    outgoing packets since the destination has been cleared.
 *
 *
 * \subsection PJMEDIA_TRANSPORT_H_REUSE Reusing the Media Transport.
 *
 *  After transport has been detached, application may re-attach the
 *    transport to another stream if it wants to. Detaching and re-attaching
 *    media transport may be preferable than closing and re-opening the
 *    transport, since it is more efficient (sockets don't need to be
 *    closed and re-opened). However it is up to the application to choose
 *    which method is most suitable for its uses.
 *
 * 
 * \subsection PJMEDIA_TRANSPORT_H_DESTROY Destroying the Media Transport.
 *
 *  Finally if application no longer needs the media transport, it will
 *    call #pjmedia_transport_close() function, which is thin wrapper which 
 *    calls "destroy()" method of the media transport's  "virtual function 
 *    pointer" (#pjmedia_transport_op). This function releases
 *    all resources used by the transport, such as sockets and memory.
 *
 *
 * \section PJMEDIA_TRANSPORT_H_EXT Media Transport Extended API
 
   Apart from the basic interface above, the media transport provides some
   more APIs:

    - pjmedia_transport_media_create()
   \n
      This API is provided to allow the media transport to add more information
      in the SDP offer, before the offer is sent to remote. Additionally, for 
      answerer side, this callback allows the media transport to reject the 
      offer before this offer is processed by the SDP negotiator. 

    - pjmedia_transport_media_start()
    \n
      This API should be called after offer and answer are negotiated, and 
      both SDPs are available, and before the media is started. For answerer
      side, this callback will be called before the answer is sent to remote,
      to allow media transport to put additional info in the SDP. For 
      offerer side, this callback will be called after SDP answer is 
      received. In this callback, the media transport has the final chance 
      to negotiate/validate the offer and answer before media is really 
      started (and answer is sent, for answerer side). 

    - pjmedia_transport_media_stop()
    \n
      This API should be called when the media is stopped, to allow the media
      transport to release its resources. 

    - pjmedia_transport_simulate_lost()
    \n
      This API can be used to instruct media transport to simulate packet lost
      on a particular direction.

 * \section PJMEDIA_TRANSPORT_H_IMPL Implementing Media Transport
 *
 * To implement a new type of media transport, one needs to "subclass" the
 * media transport "class" (#pjmedia_transport) by providing the "methods"
 * in the media transport "interface" (#pjmedia_transport_op), and provides
 * a function to create this new type of transport (similar to 
 * #pjmedia_transport_udp_create() function).
 *
 * The media transport is expected to run indepently, that is there should
 * be no polling like function to poll the transport for incoming RTP/RTCP
 * packets. This normally can be done by registering the media sockets to
 * the media endpoint's IOQueue, which allows the transport to be notified
 * when incoming packet has arrived.
 *
 * Alternatively, media transport may utilize thread(s) internally to wait
 * for incoming packets. The thread then will call the appropriate RTP or
 * RTCP callback provided by its user (stream) whenever packet is received.
 * If the transport's user is a stream, then the callbacks provided by the
 * stream will be thread-safe, so the transport may call these callbacks
 * without having to serialize the access with some mutex protection. But
 * the media transport may still have to protect its internal data with
 * mutex protection, since it may be called by application's thread (for
 * example, to send RTP/RTCP packets).
 *
 * For an example of media transport implementation, please refer to 
 * <tt>transport_udp.h</tt> and <tt>transport_udp.c</tt> in PJMEDIA source
 * distribution.
 */

PJ_BEGIN_DECL

#include <pjmedia/sdp.h>


/**
 * Forward declaration for media transport.
 */
typedef struct pjmedia_transport pjmedia_transport;

/**
 * Forward declaration for media transport info.
 */
typedef struct pjmedia_transport_info pjmedia_transport_info;

/**
 * This enumeration specifies the general behaviour of media processing
 */
typedef enum pjmedia_tranport_media_option
{
    /**
     * When this flag is specified, the transport will not perform media
     * transport validation, this is useful when transport is stacked with
     * other transport, for example when transport UDP is stacked under
     * transport SRTP, media transport validation only need to be done by 
     * transport SRTP.
     */
    PJMEDIA_TPMED_NO_TRANSPORT_CHECKING = 1

} pjmedia_tranport_media_option;


/**
 * This structure describes the operations for the stream transport.
 */
struct pjmedia_transport_op
{
    /**
     * Get media socket info from the specified transport.
     *
     * Application should call #pjmedia_transport_get_info() instead
     */
    pj_status_t (*get_info)(pjmedia_transport *tp,
			    pjmedia_transport_info *info);

    /**
     * This function is called by the stream when the transport is about
     * to be used by the stream for the first time, and it tells the transport
     * about remote RTP address to send the packet and some callbacks to be 
     * called for incoming packets.
     *
     * Application should call #pjmedia_transport_attach() instead of 
     * calling this function directly.
     */
    pj_status_t (*attach)(pjmedia_transport *tp,
			  void *user_data,
			  const pj_sockaddr_t *rem_addr,
			  const pj_sockaddr_t *rem_rtcp,
			  unsigned addr_len,
			  void (*rtp_cb)(void *user_data,
					 void *pkt,
					 pj_ssize_t size),
			  void (*rtcp_cb)(void *user_data,
					  void *pkt,
					  pj_ssize_t size));

    /**
     * This function is called by the stream when the stream no longer
     * needs the transport (normally when the stream is about to be closed).
     * After the transport is detached, it will ignore incoming
     * RTP/RTCP packets, and will refuse to send outgoing RTP/RTCP packets.
     * Application may re-attach the media transport to another transport 
     * user (e.g. stream) after the transport has been detached.
     *
     * Application should call #pjmedia_transport_detach() instead of 
     * calling this function directly.
     */
    void (*detach)(pjmedia_transport *tp,
		   void *user_data);

    /**
     * This function is called by the stream to send RTP packet using the 
     * transport.
     *
     * Application should call #pjmedia_transport_send_rtp() instead of 
     * calling this function directly.
     */
    pj_status_t (*send_rtp)(pjmedia_transport *tp,
			    const void *pkt,
			    pj_size_t size);

    /**
     * This function is called by the stream to send RTCP packet using the
     * transport.
     *
     * Application should call #pjmedia_transport_send_rtcp() instead of 
     * calling this function directly.
     */
    pj_status_t (*send_rtcp)(pjmedia_transport *tp,
			     const void *pkt,
			     pj_size_t size);

    /**
     * This function is called by application to generate the SDP parts
     * related to transport type, e.g: ICE, SRTP.
     *
     * Application should call #pjmedia_transport_media_create() instead of 
     * calling this function directly.
     */
    pj_status_t (*media_create)(pjmedia_transport *tp,
				pj_pool_t *pool,
				unsigned options,
				pjmedia_sdp_session *sdp_local,
				const pjmedia_sdp_session *sdp_remote,
				unsigned media_index);

    /**
     * This function is called by application to start the transport
     * based on SDP negotiation result.
     *
     * Application should call #pjmedia_transport_media_start() instead of 
     * calling this function directly.
     */
    pj_status_t (*media_start) (pjmedia_transport *tp,
			        pj_pool_t *pool,
			        pjmedia_sdp_session *sdp_local,
			        const pjmedia_sdp_session *sdp_remote,
				unsigned media_index);

    /**
     * This function is called by application to stop the transport.
     *
     * Application should call #pjmedia_transport_media_stop() instead of 
     * calling this function directly.
     */
    pj_status_t (*media_stop)  (pjmedia_transport *tp);

    /**
     * This function can be called to simulate packet lost.
     *
     * Application should call #pjmedia_transport_simulate_lost() instead of 
     * calling this function directly.
     */
    pj_status_t (*simulate_lost)(pjmedia_transport *tp,
				 pjmedia_dir dir,
				 unsigned pct_lost);

    /**
     * This function can be called to destroy this transport.
     *
     * Application should call #pjmedia_transport_close() instead of 
     * calling this function directly.
     */
    pj_status_t (*destroy)(pjmedia_transport *tp);
};


/**
 * @see pjmedia_transport_op.
 */
typedef struct pjmedia_transport_op pjmedia_transport_op;


/** 
 * Media transport type.
 */
typedef enum pjmedia_transport_type
{
    /** Media transport using standard UDP */
    PJMEDIA_TRANSPORT_TYPE_UDP,

    /** Media transport using ICE */
    PJMEDIA_TRANSPORT_TYPE_ICE,

    /** 
     * Media transport SRTP, this transport is actually security adapter to be
     * stacked with other transport to enable encryption on the underlying
     * transport.
     */
    PJMEDIA_TRANSPORT_TYPE_SRTP

} pjmedia_transport_type;


/**
 * This structure declares stream transport. A stream transport is called
 * by the stream to transmit a packet, and will notify stream when
 * incoming packet is arrived.
 */
struct pjmedia_transport
{
    /** Transport name (for logging purpose). */
    char		     name[PJ_MAX_OBJ_NAME];

    /** Transport type. */
    pjmedia_transport_type   type;

    /** Transport's "virtual" function table. */
    pjmedia_transport_op    *op;
};

/**
 * This structure describes buffer storage of transport specific info.
 * The actual transport specific info contents will be defined by transport
 * implementation. Note that some transport implementations do not need to
 * provide specific info, since the general socket info is enough.
 */
typedef struct pjmedia_transport_specific_info
{
    /**
     * Specify media transport type.
     */
    pjmedia_transport_type   type;

    /**
     * Specify storage buffer size of transport specific info.
     */
    int			     cbsize;

    /**
     * Storage buffer of transport specific info.
     */
    char		     buffer[PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXSIZE];

} pjmedia_transport_specific_info;


/**
 * This structure describes transport informations, including general 
 * socket information and specific information of single transport or 
 * stacked transports (e.g: SRTP stacked on top of UDP)
 */
struct pjmedia_transport_info
{
    /**
     * General socket info.
     */
    pjmedia_sock_info sock_info;

    /**
     * Specifies number of transport specific info included.
     */
    int specific_info_cnt;

    /**
     * Buffer storage of transport specific info.
     */
    pjmedia_transport_specific_info spc_info[PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXCNT];

};


/**
 * Get media socket info from the specified transport. The socket info
 * contains information about the local address of this transport, and
 * would be needed for example to fill in the "c=" and "m=" line of local 
 * SDP.
 *
 * @param tp	    The transport.
 * @param info	    Media socket info to be initialized.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_get_info(pjmedia_transport *tp,
						  pjmedia_transport_info *info)
{
    if (tp->op->get_info)
	return (*tp->op->get_info)(tp, info);
    else
	return PJ_ENOTSUP;
}


/**
 * Attach callbacks to be called on receipt of incoming RTP/RTCP packets.
 * This is just a simple wrapper which calls <tt>attach()</tt> member of 
 * the transport.
 *
 * @param tp	    The media transport.
 * @param user_data Arbitrary user data to be set when the callbacks are 
 *		    called.
 * @param rem_addr  Remote RTP address to send RTP packet to.
 * @param rem_rtcp  Optional remote RTCP address. If the argument is NULL
 *		    or if the address is zero, the RTCP address will be
 *		    calculated from the RTP address (which is RTP port
 *		    plus one).
 * @param addr_len  Length of the remote address.
 * @param rtp_cb    Callback to be called when RTP packet is received on
 *		    the transport.
 * @param rtcp_cb   Callback to be called when RTCP packet is received on
 *		    the transport.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_attach(pjmedia_transport *tp,
					        void *user_data,
					        const pj_sockaddr_t *rem_addr,
						const pj_sockaddr_t *rem_rtcp,
					        unsigned addr_len,
					        void (*rtp_cb)(void *user_data,
							       void *pkt,
							       pj_ssize_t),
					        void (*rtcp_cb)(void *usr_data,
							        void*pkt,
							        pj_ssize_t))
{
    return tp->op->attach(tp, user_data, rem_addr, rem_rtcp, addr_len, 
			  rtp_cb, rtcp_cb);
}


/**
 * Detach callbacks from the transport.
 * This is just a simple wrapper which calls <tt>detach()</tt> member of 
 * the transport. After the transport is detached, it will ignore incoming
 * RTP/RTCP packets, and will refuse to send outgoing RTP/RTCP packets.
 * Application may re-attach the media transport to another transport user
 * (e.g. stream) after the transport has been detached.
 *
 * @param tp	    The media transport.
 * @param user_data User data which must match the previously set value
 *		    on attachment.
 */
PJ_INLINE(void) pjmedia_transport_detach(pjmedia_transport *tp,
					 void *user_data)
{
    tp->op->detach(tp, user_data);
}


/**
 * Send RTP packet with the specified media transport. This is just a simple
 * wrapper which calls <tt>send_rtp()</tt> member of the transport. The 
 * RTP packet will be delivered to the destination address specified in
 * #pjmedia_transport_attach() function.
 *
 * @param tp	    The media transport.
 * @param pkt	    The packet to send.
 * @param size	    Size of the packet.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_send_rtp(pjmedia_transport *tp,
						  const void *pkt,
						  pj_size_t size)
{
    return (*tp->op->send_rtp)(tp, pkt, size);
}


/**
 * Send RTCP packet with the specified media transport. This is just a simple
 * wrapper which calls <tt>send_rtcp()</tt> member of the transport. The 
 * RTCP packet will be delivered to the destination address specified in
 * #pjmedia_transport_attach() function.
 *
 * @param tp	    The media transport.
 * @param pkt	    The packet to send.
 * @param size	    Size of the packet.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_send_rtcp(pjmedia_transport *tp,
						  const void *pkt,
						  pj_size_t size)
{
    return (*tp->op->send_rtcp)(tp, pkt, size);
}


/**
 * Generate local SDP parts that are related to the specified media transport.
 * Remote SDP might be needed as reference when application is in deciding
 * side of negotiation (callee side), otherwise it should be NULL.
 *
 * This API is provided to allow the media transport to add more information
 * in the SDP offer, before the offer is sent to remote. Additionally, for 
 * answerer side, this callback allows the media transport to reject the 
 * offer before this offer is processed by the SDP negotiator. 
 *
 * This is just a simple wrapper which calls <tt>media_create()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 * @param pool		The memory pool.
 * @param option	Option flags, from #pjmedia_tranport_media_option
 * @param sdp_local	Local SDP.
 * @param sdp_remote	Remote SDP.
 * @param media_index	Media index in SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_media_create(pjmedia_transport *tp,
				    pj_pool_t *pool,
				    unsigned options,
				    pjmedia_sdp_session *sdp_local,
				    const pjmedia_sdp_session *sdp_remote,
				    unsigned media_index)
{
    return (*tp->op->media_create)(tp, pool, options, sdp_local, sdp_remote, 
				   media_index);
}

/**
 * Start the transport with regards to SDP negotiation result. 
 * This API should be called after offer and answer are negotiated, and 
 * both SDPs are available, and before the media is started. For answerer
 * side, this callback will be called before the answer is sent to remote,
 * to allow media transport to put additional info in the SDP. For 
 * offerer side, this callback will be called after SDP answer is 
 * received. In this callback, the media transport has the final chance 
 * to negotiate/validate the offer and answer before media is really 
 * started (and answer is sent, for answerer side). 
 *
 * This is just a simple wrapper which calls <tt>media_start()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 * @param pool		The memory pool.
 * @param option	The media transport option.
 * @param sdp_local	Local SDP.
 * @param sdp_remote	Remote SDP.
 * @param media_index	Media index to start.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_media_start(pjmedia_transport *tp,
				    pj_pool_t *pool,
				    pjmedia_sdp_session *sdp_local,
				    const pjmedia_sdp_session *sdp_remote,
				    unsigned media_index)
{
    return (*tp->op->media_start)(tp, pool, sdp_local, sdp_remote, media_index);
}


/**
 * Stop the transport. 
 * This API should be called when the media is stopped, to allow the media
 * transport to release its resources. 
 *
 * This is just a simple wrapper which calls <tt>media_stop()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_media_stop(pjmedia_transport *tp)
{
    return (*tp->op->media_stop)(tp);
}

/**
 * Close media transport. This is just a simple wrapper which calls 
 * <tt>destroy()</tt> member of the transport. This function will free
 * all resources created by this transport (such as sockets, memory, etc.).
 *
 * @param tp	    The media transport.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_close(pjmedia_transport *tp)
{
    if (tp->op->destroy)
	return (*tp->op->destroy)(tp);
    else
	return PJ_SUCCESS;
}

/**
 * Simulate packet lost in the specified direction (for testing purposes).
 * When enabled, the transport will randomly drop packets to the specified
 * direction.
 *
 * @param tp	    The media transport.
 * @param dir	    Media direction to which packets will be randomly dropped.
 * @param pct_lost  Percent lost (0-100). Set to zero to disable packet
 *		    lost simulation.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_simulate_lost(pjmedia_transport *tp,
						       pjmedia_dir dir,
						       unsigned pct_lost)
{
    return (*tp->op->simulate_lost)(tp, dir, pct_lost);
}


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_H__ */


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
#ifndef __PJMEDIA_TRANSPORT_H__
#define __PJMEDIA_TRANSPORT_H__


/**
 * @file transport.h Media Transport Interface
 * @brief Transport interface.
 */

#include <pjmedia/types.h>
#include <pjmedia/errno.h>
#include <pj/string.h>

/**
 * @defgroup PJMEDIA_TRANSPORT Media Transport
 * @brief Transports.
 * @{
 * The media transport (#pjmedia_transport) is the object to send and
 * receive media packets over the network. The media transport interface
 * allows the library to be extended to support different types of 
 * transports to send and receive packets.
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


 * \section PJMEDIA_TRANSPORT_H_USING Basic Media Transport Usage
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
 * \section offer_answer Interaction with SDP Offer/Answer
 
   For basic UDP transport, the \ref PJMEDIA_TRANSPORT_H_USING above is
   sufficient to use the media transport. However, more complex media
   transports such as \ref PJMEDIA_TRANSPORT_SRTP and \ref
   PJMEDIA_TRANSPORT_ICE requires closer interactions with SDP offer and
   answer negotiation.

   The media transports can interact with the SDP offer/answer via
   these APIs:
     - #pjmedia_transport_media_create(), to initialize the media transport
       for new media session,
     - #pjmedia_transport_encode_sdp(), to encode SDP offer or answer,
     - #pjmedia_transport_media_start(), to activate the settings that
       have been negotiated by SDP offer answer, and
     - #pjmedia_transport_media_stop(), to deinitialize the media transport
       and reset the transport to its idle state.
   
   The usage of these API in the context of SDP offer answer will be 
   described below.

   \subsection media_create Initializing Transport for New Session

   Application must call #pjmedia_transport_media_create() before using
   the transport for a new session.

   \subsection creat_oa Creating SDP Offer and Answer

   The #pjmedia_transport_encode_sdp() is used to put additional information
   from the transport to the local SDP, before the SDP is sent and negotiated
   with remote SDP.

   When creating an offer, call #pjmedia_transport_encode_sdp() with
   local SDP (and NULL as \a rem_sdp). The media transport will add the
   relevant attributes in the local SDP. Application then gives the local
   SDP to the invite session to be sent to remote agent.

   When creating an answer, also call #pjmedia_transport_encode_sdp(),
   but this time specify both local and remote SDP to the function. The 
   media transport will once again modify the local SDP and add relevant
   attributes to the local SDP, if the appropriate attributes related to
   the transport functionality are present in remote offer. The remote
   SDP does not contain the relevant attributes, then the specific transport
   functionality will not be activated for the session.

   The #pjmedia_transport_encode_sdp() should also be called when application
   sends subsequent SDP offer or answer. The media transport will encode
   the appropriate attributes based on the state of the session.

   \subsection media_start Offer/Answer Completion

   Once both local and remote SDP have been negotiated by the 
   \ref PJMEDIA_SDP_NEG (normally this is part of PJSIP invite session),
   application should give both local and remote SDP to 
   #pjmedia_transport_media_start() so that the settings are activated
   for the session. This function should be called for both initial and
   subsequent SDP negotiation.

   \subsection media_stop Stopping Transport

   Once session is stop application must call #pjmedia_transport_media_stop()
   to deactivate the transport feature. Application may reuse the transport
   for subsequent media session by repeating the #pjmedia_transport_media_create(),
   #pjmedia_transport_encode_sdp(), #pjmedia_transport_media_start(), and
   #pjmedia_transport_media_stop() above.

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
 */


#include <pjmedia/sdp.h>

PJ_BEGIN_DECL


/**
 * Forward declaration for media transport.
 */
typedef struct pjmedia_transport pjmedia_transport;

/**
 * Forward declaration for media transport info.
 */
typedef struct pjmedia_transport_info pjmedia_transport_info;

/**
 * Forward declaration for media transport attach param.
 */
typedef struct pjmedia_transport_attach_param pjmedia_transport_attach_param;

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
    PJMEDIA_TPMED_NO_TRANSPORT_CHECKING = 1,

    /**
     * When this flag is specified, the transport will allow multiplexing
     * RTP and RTCP, i.e. if the remote agrees, RTCP will be sent using
     * the same socket for RTP.
     */
    PJMEDIA_TPMED_RTCP_MUX = 2

} pjmedia_tranport_media_option;


/**
 * Media socket info is used to describe the underlying sockets
 * to be used as media transport.
 */
typedef struct pjmedia_sock_info
{
    /** The RTP socket handle */
    pj_sock_t	    rtp_sock;

    /** Address to be advertised as the local address for the RTP
     *  socket, which does not need to be equal as the bound
     *  address (for example, this address can be the address resolved
     *  with STUN).
     */
    pj_sockaddr	    rtp_addr_name;

    /** The RTCP socket handle. */
    pj_sock_t	    rtcp_sock;

    /** Address to be advertised as the local address for the RTCP
     *  socket, which does not need to be equal as the bound
     *  address (for example, this address can be the address resolved
     *  with STUN).
     */
    pj_sockaddr	    rtcp_addr_name;

} pjmedia_sock_info;


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
     * called for incoming packets. This function exists for backwards
     * compatibility. Transports should implement attach2 instead.
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
     * This function is called by the stream to send RTCP packet using the
     * transport with destination address other than default specified in
     * #pjmedia_transport_attach().
     *
     * Application should call #pjmedia_transport_send_rtcp2() instead of 
     * calling this function directly.
     */
    pj_status_t (*send_rtcp2)(pjmedia_transport *tp,
			      const pj_sockaddr_t *addr,
			      unsigned addr_len,
			      const void *pkt,
			      pj_size_t size);

    /**
     * Prepare the transport for a new media session.
     *
     * Application should call #pjmedia_transport_media_create() instead of 
     * calling this function directly.
     */
    pj_status_t (*media_create)(pjmedia_transport *tp,
				pj_pool_t *sdp_pool,
				unsigned options,
				const pjmedia_sdp_session *remote_sdp,
				unsigned media_index);

    /**
     * This function is called by application to generate the SDP parts
     * related to transport type, e.g: ICE, SRTP.
     *
     * Application should call #pjmedia_transport_encode_sdp() instead of
     * calling this function directly.
     */
    pj_status_t (*encode_sdp)(pjmedia_transport *tp,
			      pj_pool_t *sdp_pool,
			      pjmedia_sdp_session *sdp_local,
			      const pjmedia_sdp_session *rem_sdp,
			      unsigned media_index);

    /**
     * This function is called by application to start the transport
     * based on local and remote SDP.
     *
     * Application should call #pjmedia_transport_media_start() instead of 
     * calling this function directly.
     */
    pj_status_t (*media_start) (pjmedia_transport *tp,
			        pj_pool_t *tmp_pool,
			        const pjmedia_sdp_session *sdp_local,
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

    /**
     * This function is called by the stream when the transport is about
     * to be used by the stream for the first time, and it tells the transport
     * about remote RTP address to send the packet and some callbacks to be
     * called for incoming packets.
     *
     * Application should call #pjmedia_transport_attach2() instead of
     * calling this function directly.
     */
    pj_status_t (*attach2)(pjmedia_transport *tp,
			   pjmedia_transport_attach_param *att_param);
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
    PJMEDIA_TRANSPORT_TYPE_SRTP,

    /** Loopback media transport */
    PJMEDIA_TRANSPORT_TYPE_LOOP,

    /**
     * Start of user defined transport.
     */
    PJMEDIA_TRANSPORT_TYPE_USER

} pjmedia_transport_type;


/**
 * This structure declares media transport. A media transport is called
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

    /** Application/user data */
    void		    *user_data;
};

/**
 * This structure describes storage buffer of transport specific info.
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

    /**
     * The media transport instance.
     */
    pjmedia_transport	    *tp;

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
     * Remote address where RTP/RTCP originated from. In case this transport
     * hasn't ever received packet, the address can be invalid (zero).
     */
    pj_sockaddr	    src_rtp_name;
    pj_sockaddr	    src_rtcp_name;

    /**
     * Specifies number of transport specific info included.
     */
    unsigned specific_info_cnt;

    /**
     * Buffer storage of transport specific info.
     */
    pjmedia_transport_specific_info spc_info[PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXCNT];

};

/**
 * This structure describes the data passed when calling #rtp_cb2().
 */
typedef struct pjmedia_tp_cb_param
{
    /**
     * User data.
     */
    void 	       *user_data;

    /**
     * Packet buffer.
     */
    void 	       *pkt;

    /**
     * Packet size.
     */
    pj_ssize_t 		size;

    /**
     * Packet's source address.
     */
    pj_sockaddr	       *src_addr;

    /**
     * Should media transport switch remote address to \a rtp_src_addr?
     * Media transport should initialize it to PJ_FALSE, and application
     * can change the value as necessary.
     */
    pj_bool_t	        rem_switch;

} pjmedia_tp_cb_param;

/**
 * This structure describes the data passed when calling
 * #pjmedia_transport_attach2().
 */
struct pjmedia_transport_attach_param
{
    /**
     * The media stream.
     */
    void *stream;

    /**
     * Indicate the stream type, either it's audio (PJMEDIA_TYPE_AUDIO) 
     * or video (PJMEDIA_TYPE_VIDEO).
     */
    pjmedia_type media_type;

    /**
     * Remote RTP address to send RTP packet to.
     */
    pj_sockaddr rem_addr;

    /**
     * Optional remote RTCP address. If the argument is NULL
     * or if the address is zero, the RTCP address will be
     * calculated from the RTP address (which is RTP port plus one).
     */
    pj_sockaddr rem_rtcp;

    /**
     * Length of the remote address.
     */
    unsigned addr_len;

    /**
     * Arbitrary user data to be set when the callbacks are called.
     */
    void *user_data;

    /**
     * Callback to be called when RTP packet is received on the transport.
     */
    void (*rtp_cb)(void *user_data, void *pkt, pj_ssize_t);

    /**
     * Callback to be called when RTCP packet is received on the transport.
     */
    void (*rtcp_cb)(void *user_data, void *pkt, pj_ssize_t);

    /**
     * Callback to be called when RTP packet is received on the transport.
     */
    void (*rtp_cb2)(pjmedia_tp_cb_param *param);

};

/**
 * Initialize transport info.
 *
 * @param info	    Transport info to be initialized.
 */
PJ_INLINE(void) pjmedia_transport_info_init(pjmedia_transport_info *info)
{
    pj_bzero(info, sizeof(pjmedia_transport_info));
    info->sock_info.rtp_sock = info->sock_info.rtcp_sock = PJ_INVALID_SOCKET;
}


/**
 * Get media transport info from the specified transport and all underlying 
 * transports if any. The transport also contains information about socket info
 * which describes the local address of the transport, and would be needed
 * for example to fill in the "c=" and "m=" line of local SDP.
 *
 * @param tp	    The transport.
 * @param info	    Media transport info to be initialized.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_get_info(pjmedia_transport *tp,
						  pjmedia_transport_info *info)
{
    if (tp && tp->op && tp->op->get_info)
	return (*tp->op->get_info)(tp, info);
    
    return PJ_ENOTSUP;
}


/**
 * Utility API to get transport type specific info from the specified media
 * transport info.
 * 
 * @param info	    Media transport info.
 * @param type	    Media transport type.
 *
 * @return	    Pointer to media transport specific info, or NULL if
 * 		    specific info for the transport type is not found.
 */
PJ_INLINE(void*) pjmedia_transport_info_get_spc_info(
						pjmedia_transport_info *info,
						pjmedia_transport_type type)
{
    unsigned i;
    for (i = 0; i < info->specific_info_cnt; ++i) {
	if (info->spc_info[i].type == type)
	    return (void*)info->spc_info[i].buffer;
    }
    return NULL;
}


/**
 * Utility API to get the transport instance from the specified media
 * transport info.
 *
 * @param info	    Media transport info.
 * @param type	    Media transport type.
 *
 * @return	    The media transport instance, or NULL if
 * 		    the transport type is not found.
 */
PJ_INLINE(pjmedia_transport*) pjmedia_transport_info_get_transport(
						pjmedia_transport_info *info,
						pjmedia_transport_type type)
{
    unsigned i;
    for (i = 0; i < info->specific_info_cnt; ++i) {
	if (info->spc_info[i].type == type)
	    return info->spc_info[i].tp;
    }
    return NULL;
}


/**
 * Attach callbacks to be called on receipt of incoming RTP/RTCP packets.
 * This is just a simple wrapper which calls <tt>attach2()</tt> member of
 * the transport if it is implemented, otherwise it calls <tt>attach()</tt>
 * member of the transport.
 *
 * @param tp	    The media transport.
 * @param att_param The transport attach param.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_attach2(pjmedia_transport *tp,
                                  pjmedia_transport_attach_param *att_param)
{
    if (tp->op->attach2) {
	return (*tp->op->attach2)(tp, att_param);
    } else {
	return (*tp->op->attach)(tp, att_param->user_data, 
				 (pj_sockaddr_t*)&att_param->rem_addr, 
				 (pj_sockaddr_t*)&att_param->rem_rtcp, 
				 att_param->addr_len, att_param->rtp_cb, 
				 att_param->rtcp_cb);
    }
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
    if (tp->op->attach2) {
	pjmedia_transport_attach_param param;

	pj_bzero(&param, sizeof(param));
	param.user_data = user_data;
	pj_sockaddr_cp(&param.rem_addr, rem_addr);
	if (rem_rtcp && pj_sockaddr_has_addr(rem_rtcp)) {
	    pj_sockaddr_cp(&param.rem_rtcp, rem_rtcp);
	} else {
	    /* Copy RTCP address from the RTP address, with port + 1 */
	    pj_memcpy(&param.rem_rtcp, rem_addr, addr_len);
	    pj_sockaddr_set_port(&param.rem_rtcp,
				 pj_sockaddr_get_port(rem_addr) + 1);
	}
	param.addr_len = addr_len;
	param.rtp_cb = rtp_cb;
	param.rtcp_cb = rtcp_cb;

	return (*tp->op->attach2)(tp, &param);
    } else {
	return (*tp->op->attach)(tp, user_data, rem_addr, rem_rtcp, addr_len,
			         rtp_cb, rtcp_cb);
    }
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
    (*tp->op->detach)(tp, user_data);
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
 * Send RTCP packet with the specified media transport. This is just a simple
 * wrapper which calls <tt>send_rtcp2()</tt> member of the transport. The 
 * RTCP packet will be delivered to the destination address specified in
 * param addr, if addr is NULL, RTCP packet will be delivered to destination 
 * address specified in #pjmedia_transport_attach() function.
 *
 * @param tp	    The media transport.
 * @param addr	    The destination address.
 * @param addr_len  Length of destination address.
 * @param pkt	    The packet to send.
 * @param size	    Size of the packet.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_send_rtcp2(pjmedia_transport *tp,
						    const pj_sockaddr_t *addr,
						    unsigned addr_len,
						    const void *pkt,
						    pj_size_t size)
{
    return (*tp->op->send_rtcp2)(tp, addr, addr_len, pkt, size);
}


/**
 * Prepare the media transport for a new media session, Application must
 * call this function before starting a new media session using this
 * transport.
 *
 * This is just a simple wrapper which calls <tt>media_create()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 * @param sdp_pool	Pool object to allocate memory related to SDP
 *			messaging components.
 * @param options	Option flags, from #pjmedia_tranport_media_option
 * @param rem_sdp	Remote SDP if local SDP is an answer, otherwise
 *			specify NULL if SDP is an offer.
 * @param media_index	Media index in SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_media_create(pjmedia_transport *tp,
				    pj_pool_t *sdp_pool,
				    unsigned options,
				    const pjmedia_sdp_session *rem_sdp,
				    unsigned media_index)
{
    return (*tp->op->media_create)(tp, sdp_pool, options, rem_sdp, 
				   media_index);
}


/**
 * Put transport specific information into the SDP. This function can be
 * called to put transport specific information in the initial or
 * subsequent SDP offer or answer.
 *
 * This is just a simple wrapper which calls <tt>encode_sdp()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 * @param sdp_pool	Pool object to allocate memory related to SDP
 *			messaging components.
 * @param sdp		The local SDP to be filled in information from the
 *			media transport.
 * @param rem_sdp	Remote SDP if local SDP is an answer, otherwise
 *			specify NULL if SDP is an offer.
 * @param media_index	Media index in SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_encode_sdp(pjmedia_transport *tp,
					    pj_pool_t *sdp_pool,
					    pjmedia_sdp_session *sdp,
					    const pjmedia_sdp_session *rem_sdp,
					    unsigned media_index)
{
    return (*tp->op->encode_sdp)(tp, sdp_pool, sdp, rem_sdp, media_index);
}


/**
 * Start the transport session with the settings in both local and remote 
 * SDP. The actual work that is done by this function depends on the 
 * underlying transport type. For SRTP, this will activate the encryption
 * and decryption based on the keys found the SDPs. For ICE, this will
 * start ICE negotiation according to the information found in the SDPs.
 *
 * This is just a simple wrapper which calls <tt>media_start()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 * @param tmp_pool	The memory pool for allocating temporary objects.
 * @param sdp_local	Local SDP.
 * @param sdp_remote	Remote SDP.
 * @param media_index	Media index in the SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_media_start(pjmedia_transport *tp,
				    pj_pool_t *tmp_pool,
				    const pjmedia_sdp_session *sdp_local,
				    const pjmedia_sdp_session *sdp_remote,
				    unsigned media_index)
{
    return (*tp->op->media_start)(tp, tmp_pool, sdp_local, sdp_remote, 
				  media_index);
}


/**
 * This API should be called when the session is stopped, to allow the media
 * transport to release its resources used for the session.
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


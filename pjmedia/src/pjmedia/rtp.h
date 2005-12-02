/* $Header: /pjproject/pjmedia/src/pjmedia/rtp.h 6     8/24/05 10:30a Bennylp $ */
/* 
 * PJMEDIA - Multimedia over IP Stack 
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PJMEDIA_RTP_H__
#define __PJMEDIA_RTP_H__

#include <pj/types.h>

/**
 * @file rtp.h
 * @brief RTP implementation.
 */

PJ_BEGIN_DECL


/**
 * @defgroup PJMED_RTP RTP
 * @ingroup PJMEDIA
 * @{
 *
 * The RTP module is designed to be dependent only to PJLIB, it does not depend
 * on any other parts of PJMEDIA library. The RTP module does not even depend
 * on any transports (sockets), to promote even more use.
 *
 * An RTCP implementation is also separated from this module.
 *
 * The functions that are provided by this module:
 *  - creating RTP header for each outgoing packet.
 *  - decoding RTP packet into RTP header and payload.
 *  - provide simple RTP session management (sequence number, etc.)
 *
 * The RTP module does not use any dynamic memory at all.
 *
 * \section P1 How to Use the RTP Module
 * 
 * First application must call #pj_rtp_session_init to initialize the RTP 
 * session.
 *
 * When application wants to send RTP packet, it needs to call 
 * #pj_rtp_encode_rtp to build the RTP header. Note that this WILL NOT build
 * the complete RTP packet, but instead only the header. Application can
 * then either concatenate the header with the payload, or send the two
 * fragments (the header and the payload) using scatter-gather transport API
 * (e.g. \a sendv()).
 *
 * When application receives an RTP packet, first it should call
 * #pj_rtp_decode_rtp to decode RTP header and payload, then it should call
 * #pj_rtp_session_update to check whether we can process the RTP payload,
 * and to let the RTP session updates its internal status. The decode function
 * is guaranteed to point the payload to the correct position regardless of
 * any options present in the RTP packet.
 *
 */


#ifdef _MSC_VER
#  pragma warning ( disable : 4214 )
#endif


/**
 * Error codes.
 */
enum pj_rtp_error_t
{
    PJ_RTP_ERR_RTP_PACKING,	    /**< Invalid RTP packet. */
    PJ_RTP_ERR_INVALID_VERSION,	    /**< Invalid RTP version. */
    PJ_RTP_ERR_INVALID_SSRC,	    /**< Invalid SSRC. */
    PJ_RTP_ERR_INVALID_PT,	    /**< Invalid payload type. */
    PJ_RTP_ERR_INVALID_PACKET,	    /**< Invalid packet. */
    PJ_RTP_ERR_SESSION_RESTARTED,   /**< Session has just been restarted. */
    PJ_RTP_ERR_SESSION_PROBATION,   /**< Session in probation. */
    PJ_RTP_ERR_BAD_SEQUENCE,	    /**< Bad RTP sequence number. */
};

#pragma pack(1)
/**
 * RTP packet header.
 */
struct pj_rtp_hdr
{
#if defined(PJ_IS_BIG_ENDIAN) && (PJ_IS_BIG_ENDIAN!=0)
    pj_uint16_t v:2;	/**< packet type/version	    */
    pj_uint16_t p:1;	/**< padding flag		    */
    pj_uint16_t x:1;	/**< extension flag	    */
    pj_uint16_t cc:4;	/**< CSRC count		    */
    pj_uint16_t m:1;	/**< marker bit		    */
    pj_uint16_t pt:7;	/**< payload type		    */
#else
    pj_uint16_t cc:4;	/**< CSRC count		    */
    pj_uint16_t x:1;	/**< header extension flag    */ 
    pj_uint16_t p:1;	/**< padding flag		    */
    pj_uint16_t v:2;	/**< packet type/version	    */
    pj_uint16_t pt:7;	/**< payload type		    */
    pj_uint16_t m:1;	/**< marker bit		    */
#endif
    pj_uint16_t seq;	/**< sequence number	    */
    pj_uint32_t ts;	/**< timestamp		    */
    pj_uint32_t ssrc;	/**< synchronization source   */
};
#pragma pack()

typedef struct pj_rtp_hdr pj_rtp_hdr;

/**
 * RTP extendsion header.
 */
struct pj_rtp_ext_hdr
{
    pj_uint16_t	profile_data;
    pj_uint16_t	length;
};

typedef struct pj_rtp_ext_hdr pj_rtp_ext_hdr;

/**
 * A generic sequence number management, used by both RTP and RTCP.
 */
struct pj_rtp_seq_session
{
    pj_uint16_t	    max_seq;	    /**< highest sequence number heard */
    pj_uint32_t	    cycles;	    /**< shifted count of seq. number cycles */
    pj_uint32_t	    base_seq;	    /**< base seq number */
    pj_uint32_t	    bad_seq;        /**< last 'bad' seq number + 1 */
    pj_uint32_t	    probation;      /**< sequ. packets till source is valid */
};

typedef struct pj_rtp_seq_session pj_rtp_seq_session;

/**
 * RTP session descriptor.
 */
struct pj_rtp_session
{
    pj_rtp_hdr		out_hdr;    /**< Saved header for outgoing packets. */
    pj_rtp_seq_session	seq_ctrl;   /**< Sequence number management. */
    pj_uint16_t	        out_pt;	    /**< Default outgoing payload type. */
    pj_uint32_t	        out_extseq; /**< Outgoing extended sequence number. */
    pj_uint32_t	        peer_ssrc;  /**< Peer SSRC. */
    pj_uint32_t	        received;   /**< Number of received packets. */
};

typedef struct pj_rtp_session pj_rtp_session;

/**
 * \brief Initialize RTP session.
 * This function will initialize the RTP session according to given parameters.
 *
 * @param ses		The session.
 * @param default_pt	Default payload type.
 * @param sender_ssrc	SSRC used for outgoing packets.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_rtp_session_init( pj_rtp_session *ses,
					  int default_pt, pj_uint32_t sender_ssrc );

/**
 * \brief Encode outgoing RTP packet header.
 * Create the RTP header based on arguments and current state of the RTP
 * session.
 *
 * @param ses		The session.
 * @param pt		Payload type.
 * @param m		Marker flag.
 * @param payload_len	Payload length in bytes.
 * @param ts_len	Timestamp length.
 * @param rtphdr	Upon return will point to RTP packet header.
 * @param hdrlen	Upon return will indicate the size of RTP packet header
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_rtp_encode_rtp( pj_rtp_session *ses, int pt, int m,
				        int payload_len, int ts_len,
					const void **rtphdr, int *hdrlen );

/**
 * \brief Decode an incoming RTP packet.
 * This function will decode incoming packet into RTP header and payload.
 * The decode function is guaranteed to point the payload to the correct 
 * position regardless of any options present in the RTP packet.
 *
 * @param ses		The session.
 * @param pkt		The received RTP packet.
 * @param pkt_len	The length of the packet.
 * @param hdr		Upon return will point to the location of the RTP header
 *			inside the packet.
 * @param payload	Upon return will point to the location of the
 *			payload inside the packet.
 * @param payloadlen	Upon return will indicate the size of the payload.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_rtp_decode_rtp( pj_rtp_session *ses, 
				        const void *pkt, int pkt_len,
					const pj_rtp_hdr **hdr,
					const void **payload,
					unsigned *payloadlen);

/**
 * \brief Update RTP session with an incoming RTP packet. 
 * Call this function everytime
 * an RTP packet is received to check whether the packet can be received and to
 * let the RTP session performs its internal calculations.
 *
 * @param ses	    The session.
 * @param hdr	    The RTP header of the incoming packet.
 *
 * @return zero if the packet is valid and can be processed, otherwise will
 *         return one of the error in #pj_rtp_error_t.
 */
PJ_DECL(pj_status_t) pj_rtp_session_update( pj_rtp_session *ses, 
					    const pj_rtp_hdr *hdr);

/** 
* \brief Internal.
 * Internal function for sequence control, shared by RTCP implementation. 
 */
void pj_rtp_seq_init(pj_rtp_seq_session *seq_ctrl, pj_uint16_t seq);

/** 
* \brief Internal.
 * Internal function for sequence control, shared by RTCP implementation. 
 */
void pj_rtp_seq_restart(pj_rtp_seq_session *seq_ctrl, pj_uint16_t seq);

/** 
* \brief Internal.
 * Internal function for sequence control, shared by RTCP implementation. 
 */
int  pj_rtp_seq_update(pj_rtp_seq_session *seq_ctrl, pj_uint16_t seq);

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_RTP_H__ */

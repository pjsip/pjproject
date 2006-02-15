/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_RTP_H__
#define __PJMEDIA_RTP_H__


/**
 * @file rtp.h
 * @brief RTP packet and RTP session declarations.
 */
#include <pjmedia/types.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJMED_RTP RTP Packet and RTP Session Management
 * @ingroup PJMEDIA
 * @{
 *
 * The RTP module is designed to be dependent only to PJLIB, it does not depend
 * on any other parts of PJMEDIA library. The RTP module does not even depend
 * on any transports (sockets), to promote even more use.
 *
 * An RTCP implementation is available, in separate module.
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
 * First application must call #pjmedia_rtp_session_init() to initialize the RTP 
 * session.
 *
 * When application wants to send RTP packet, it needs to call 
 * #pjmedia_rtp_encode_rtp() to build the RTP header. Note that this WILL NOT build
 * the complete RTP packet, but instead only the header. Application can
 * then either concatenate the header with the payload, or send the two
 * fragments (the header and the payload) using scatter-gather transport API
 * (e.g. \a sendv()).
 *
 * When application receives an RTP packet, first it should call
 * #pjmedia_rtp_decode_rtp to decode RTP header and payload, then it should call
 * #pjmedia_rtp_session_update to check whether we can process the RTP payload,
 * and to let the RTP session updates its internal status. The decode function
 * is guaranteed to point the payload to the correct position regardless of
 * any options present in the RTP packet.
 *
 */


#pragma pack(1)

/**
 * RTP packet header.
 */
struct pjmedia_rtp_hdr
{
#if defined(PJ_IS_BIG_ENDIAN) && (PJ_IS_BIG_ENDIAN!=0)
    int v:2;		/**< packet type/version	    */
    int p:1;		/**< padding flag		    */
    int x:1;		/**< extension flag		    */
    int cc:4;		/**< CSRC count			    */
    int m:1;		/**< marker bit			    */
    int pt:7;		/**< payload type		    */
#else
    int cc:4;		/**< CSRC count			    */
    int x:1;		/**< header extension flag	    */ 
    int p:1;		/**< padding flag		    */
    int v:2;		/**< packet type/version	    */
    int pt:7;		/**< payload type		    */
    int m:1;		/**< marker bit			    */
#endif
    pj_uint16_t seq;	/**< sequence number		    */
    pj_uint32_t ts;	/**< timestamp			    */
    pj_uint32_t ssrc;	/**< synchronization source	    */
};

#pragma pack()

/**
 * @see pjmedia_rtp_hdr
 */
typedef struct pjmedia_rtp_hdr pjmedia_rtp_hdr;


/**
 * RTP extendsion header.
 */
struct pjmedia_rtp_ext_hdr
{
    pj_uint16_t	profile_data;	/**< Profile data.	    */
    pj_uint16_t	length;		/**< Length.		    */
};

/**
 * @see pjmedia_rtp_ext_hdr
 */
typedef struct pjmedia_rtp_ext_hdr pjmedia_rtp_ext_hdr;


/**
 * A generic sequence number management, used by both RTP and RTCP.
 */
struct pjmedia_rtp_seq_session
{
    pj_uint16_t	    max_seq;	    /**< Highest sequence number heard	    */
    pj_uint32_t	    cycles;	    /**< Shifted count of seq number cycles */
    pj_uint32_t	    base_seq;	    /**< Base seq number		    */
    pj_uint32_t	    bad_seq;        /**< Last 'bad' seq number + 1	    */
    pj_uint32_t	    probation;      /**< Sequ. packets till source is valid */
};

/**
 * @see pjmedia_rtp_seq_session
 */
typedef struct pjmedia_rtp_seq_session pjmedia_rtp_seq_session;


/**
 * RTP session descriptor.
 */
struct pjmedia_rtp_session
{
    pjmedia_rtp_hdr	    out_hdr;    /**< Saved hdr for outgoing pkts.   */
    pjmedia_rtp_seq_session seq_ctrl;   /**< Sequence number management.    */
    pj_uint16_t		    out_pt;	/**< Default outgoing payload type. */
    pj_uint32_t		    out_extseq; /**< Outgoing extended seq #.	    */
    pj_uint32_t		    peer_ssrc;  /**< Peer SSRC.			    */
    pj_uint32_t		    received;   /**< Number of received packets.    */
};

/**
 * @see pjmedia_rtp_session
 */
typedef struct pjmedia_rtp_session pjmedia_rtp_session;


/**
 * This function will initialize the RTP session according to given parameters.
 *
 * @param ses		The session.
 * @param default_pt	Default payload type.
 * @param sender_ssrc	SSRC used for outgoing packets.
 *
 * @return		PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjmedia_rtp_session_init( pjmedia_rtp_session *ses,
					       int default_pt, 
					       pj_uint32_t sender_ssrc );

/**
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
 * @return		PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjmedia_rtp_encode_rtp( pjmedia_rtp_session *ses, 
					     int pt, int m,
					     int payload_len, int ts_len,
					     const void **rtphdr, 
					     int *hdrlen );

/**
 * This function decodes incoming packet into RTP header and payload.
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
 * @return		PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjmedia_rtp_decode_rtp( pjmedia_rtp_session *ses, 
					     const void *pkt, int pkt_len,
					     const pjmedia_rtp_hdr **hdr,
					     const void **payload,
					     unsigned *payloadlen);

/**
 * Call this function everytime an RTP packet is received to check whether 
 * the packet can be received and to let the RTP session performs its internal
 * calculations.
 *
 * @param ses	    The session.
 * @param hdr	    The RTP header of the incoming packet.
 *
 * @return	    PJ_SUCCESS if the packet is valid and can be processed, 
 *		    otherwise will return the appropriate status code.
 */
PJ_DECL(pj_status_t) pjmedia_rtp_session_update( pjmedia_rtp_session *ses, 
						 const pjmedia_rtp_hdr *hdr);


/*
 * INTERNAL:
 */

/** 
 * Internal function for creating sequence number control, shared by RTCP 
 * implementation. 
 *
 * @param seq_ctrl  The sequence control instance.
 * @param seq	    Sequence number to initialize.
 */
void pjmedia_rtp_seq_init(pjmedia_rtp_seq_session *seq_ctrl, 
			  pj_uint16_t seq);


/** 
 * Internal function to restart the sequence number control, shared by RTCP 
 * implementation. 
 *
 * @param seq_ctrl  The sequence control instance.
 * @param seq	    Sequence number to restart.
 */
void pjmedia_rtp_seq_restart(pjmedia_rtp_seq_session *seq_ctrl, 
			     pj_uint16_t seq);

/** 
 * Internal function update sequence control, shared by RTCP implementation.
 *
 * @param seq_ctrl  The sequence control instance.
 * @param seq	    Sequence number to update.
 *
 * @return	    PJ_SUCCESS if the sequence number can be accepted.
 */
pj_status_t pjmedia_rtp_seq_update(pjmedia_rtp_seq_session *seq_ctrl, 
				   pj_uint16_t seq);

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_RTP_H__ */

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
#ifndef __PJMEDIA_RTCP_H__
#define __PJMEDIA_RTCP_H__

/**
 * @file rtcp.h
 * @brief RTCP implementation.
 */

#include <pjmedia/types.h>
#include <pjmedia/rtp.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJMED_RTCP RTCP Management
 * @ingroup PJMEDIA
 * @{
 */

#pragma pack(1)

/**
 * RTCP sender report.
 */
struct pjmedia_rtcp_sr
{
    pj_uint32_t	    ssrc;	    /**< SSRC identification.		*/
    pj_uint32_t	    ntp_sec;	    /**< NTP time, seconds part.	*/
    pj_uint32_t	    ntp_frac;	    /**< NTP time, fractions part.	*/
    pj_uint32_t	    rtp_ts;	    /**< RTP timestamp.			*/
    pj_uint32_t	    sender_pcount;  /**< Sender packet cound.		*/
    pj_uint32_t	    sender_bcount;  /**< Sender octet/bytes count.	*/
};

/**
 * @see pjmedia_rtcp_sr
 */
typedef struct pjmedia_rtcp_sr pjmedia_rtcp_sr;

/**
 * RTCP receiver report.
 */
struct pjmedia_rtcp_rr
{
    pj_uint32_t	    ssrc;	    /**< SSRC identification.		*/
#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    pj_uint32_t	    fract_lost:8;   /**< Fraction lost.			*/
    pj_uint32_t	    total_lost_2:8; /**< Total lost, bit 16-23.		*/
    pj_uint32_t	    total_lost_1:8; /**< Total lost, bit 8-15.		*/
    pj_uint32_t	    total_lost_0:8; /**< Total lost, bit 0-7.		*/
#else
    pj_uint32_t	    fract_lost:8;   /**< Fraction lost.			*/
    pj_uint32_t	    total_lost_0:8; /**< Total lost, bit 0-7.		*/
    pj_uint32_t	    total_lost_1:8; /**< Total lost, bit 8-15.		*/
    pj_uint32_t	    total_lost_2:8; /**< Total lost, bit 16-23.		*/
#endif	
    pj_uint32_t	    last_seq;	    /**< Last sequence number.		*/
    pj_uint32_t	    jitter;	    /**< Jitter.			*/
    pj_uint32_t	    lsr;	    /**< Last SR.			*/
    pj_uint32_t	    dlsr;	    /**< Delay since last SR.		*/
};

/**
 * @see pjmedia_rtcp_rr
 */
typedef struct pjmedia_rtcp_rr pjmedia_rtcp_rr;


/**
 * RTCP common header.
 */
struct pjmedia_rtcp_common
{
#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    unsigned	    version:2;	/**< packet type            */
    unsigned	    p:1;	/**< padding flag           */
    unsigned	    count:5;	/**< varies by payload type */
    unsigned	    pt:8;	/**< payload type           */
#else
    unsigned	    count:5;	/**< varies by payload type */
    unsigned	    p:1;	/**< padding flag           */
    unsigned	    version:2;	/**< packet type            */
    unsigned	    pt:8;	/**< payload type           */
#endif
    unsigned	    length:16;	/**< packet length          */
};

/**
 * @see pjmedia_rtcp_common
 */
typedef struct pjmedia_rtcp_common pjmedia_rtcp_common;

/**
 * RTCP packet.
 */
struct pjmedia_rtcp_pkt
{
    pjmedia_rtcp_common  common;	/**< Common header.	    */
    pjmedia_rtcp_sr sr;		/**< Sender report.	    */
    pjmedia_rtcp_rr rr;		/**< variable-length list   */
};

/**
 * @see pjmedia_rtcp_pkt
 */
typedef struct pjmedia_rtcp_pkt pjmedia_rtcp_pkt;


#pragma pack()


/**
 * NTP time representation.
 */
struct pjmedia_rtcp_ntp_rec
{
    pj_uint32_t	    hi;		/**< High order 32-bit part.	*/
    pj_uint32_t	    lo;		/**< Lo order 32-bit part.	*/
};

/**
 * @see pjmedia_rtcp_ntp_rec
 */
typedef struct pjmedia_rtcp_ntp_rec pjmedia_rtcp_ntp_rec;



/**
 * RTCP session.
 */
struct pjmedia_rtcp_session
{
    pjmedia_rtcp_pkt	    rtcp_pkt;	/**< Cached RTCP packet.	    */
    
    pjmedia_rtp_seq_session seq_ctrl;	/**< RTCP sequence number control.  */

    unsigned	    clock_rate;	    /**< Clock rate.			    */
    pj_uint32_t	    received;       /**< # pkts received		    */
    pj_uint32_t	    expected_prior; /**< # pkts expected at last interval   */
    pj_uint32_t	    received_prior; /**< # pkts received at last interval   */
    pj_int32_t	    transit;        /**< Relative trans time for prev pkt   */
    pj_uint32_t	    jitter;	    /**< Estimated jitter		    */
    pj_timestamp    ts_freq;	    /**< System timestamp frequency.	    */

    pjmedia_rtcp_ntp_rec rtcp_lsr;	 /**< NTP ts in last SR received    */
    unsigned 		 rtcp_lsr_time;  /**< Time when last SR is received.*/
    pj_uint32_t		 peer_ssrc;	 /**< Peer SSRC			    */
    
};

/**
 * @see pjmedia_rtcp_session
 */
typedef struct pjmedia_rtcp_session pjmedia_rtcp_session;


/**
 * Initialize RTCP session.
 *
 * @param session   The session
 * @param ssrc	    The SSRC used in to identify the session.
 */
PJ_DECL(void) pjmedia_rtcp_init( pjmedia_rtcp_session *session, 
				 unsigned clock_rate,
				 pj_uint32_t ssrc );


/**
 * Deinitialize RTCP session.
 *
 * @param session   The session.
 */
PJ_DECL(void) pjmedia_rtcp_fini( pjmedia_rtcp_session *session);


/**
 * Call this function everytime an RTP packet is received to let the RTCP
 * session do its internal calculations.
 *
 * @param session   The session.
 * @param seq	    The RTP packet sequence number, in host byte order.
 * @param ts	    The RTP packet timestamp, in host byte order.
 */
PJ_DECL(void) pjmedia_rtcp_rx_rtp( pjmedia_rtcp_session *session, 
				   pj_uint16_t seq, 
				   pj_uint32_t ts );


/**
 * Call this function everytime an RTP packet is sent to let the RTCP session
 * do its internal calculations.
 *
 * @param session   The session.
 * @param ptsize    The payload size of the RTP packet (ie packet minus
 *		    RTP header) in bytes.
 */
PJ_DECL(void) pjmedia_rtcp_tx_rtp( pjmedia_rtcp_session *session, 
				   pj_uint16_t ptsize );


/**
 * Build a RTCP SR/RR packet to be transmitted to remote RTP peer.
 * @param session The session.
 *
 * @param rtcp_pkt  [output] Upon return, it will contain pointer to the 
 *		    RTCP packet.
 * @param len	    [output] Upon return, it will indicate the size of 
 *		    the RTCP packet.
 */
PJ_DECL(void) pjmedia_rtcp_build_rtcp( pjmedia_rtcp_session *session, 
				       pjmedia_rtcp_pkt **rtcp_pkt, 
				       int *len );


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_RTCP_H__ */

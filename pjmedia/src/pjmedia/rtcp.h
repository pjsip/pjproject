/* $Header: /pjproject/pjmedia/src/pjmedia/rtcp.h 5     8/24/05 10:30a Bennylp $ */
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

#ifndef __PJMEDIA_RTCP_H__
#define __PJMEDIA_RTCP_H__

/**
 * @file rtcp.h
 * @brief RTCP implementation.
 */

#include <pj/types.h>
#include <pjmedia/rtp.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJMED_RTCP RTCP
 * @ingroup PJMEDIA
 * @{
 */

/**
 * RTCP sender report.
 */
struct pj_rtcp_sr
{
    pj_uint32_t	    ssrc;
    pj_uint32_t	    ntp_sec;
    pj_uint32_t	    ntp_frac;
    pj_uint32_t	    rtp_ts;
    pj_uint32_t	    sender_pcount;
    pj_uint32_t	    sender_bcount;
};

typedef struct pj_rtcp_sr pj_rtcp_sr;

/**
 * RTCP receiver report.
 */
struct pj_rtcp_rr
{
    pj_uint32_t	    ssrc;
#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    pj_uint32_t	    fract_lost:8;
    pj_uint32_t	    total_lost_2:8;
    pj_uint32_t	    total_lost_1:8;
    pj_uint32_t	    total_lost_0:8;
#else
    pj_uint32_t	    fract_lost:8;
    pj_uint32_t	    total_lost_0:8;
    pj_uint32_t	    total_lost_1:8;
    pj_uint32_t	    total_lost_2:8;
#endif	
    pj_uint32_t	    last_seq;
    pj_uint32_t	    jitter;
    pj_uint32_t	    lsr;
    pj_uint32_t	    dlsr;
};

typedef struct pj_rtcp_rr pj_rtcp_rr;

/**
 * RTCP common header.
 */
struct pj_rtcp_common
{
#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    unsigned	    version:2;	/* packet type            */
    unsigned	    p:1;	/* padding flag           */
    unsigned	    count:5;	/* varies by payload type */
    unsigned	    pt:8;	/* payload type           */
#else
    unsigned	    count:5;	/* varies by payload type */
    unsigned	    p:1;	/* padding flag           */
    unsigned	    version:2;	/* packet type            */
    unsigned	    pt:8;	/* payload type           */
#endif
    pj_uint16_t	    length;	/* packet length          */
};

typedef struct pj_rtcp_common pj_rtcp_common;

/**
 * RTCP packet.
 */
struct pj_rtcp_pkt
{
    pj_rtcp_common  common;	
    pj_rtcp_sr	    sr;
    pj_rtcp_rr	    rr;		/* variable-length list */
};

typedef struct pj_rtcp_pkt pj_rtcp_pkt;

/**
 * NTP time representation.
 */
struct pj_rtcp_ntp_rec
{
    pj_uint32_t	    hi;
    pj_uint32_t	    lo;
};

typedef struct pj_rtcp_ntp_rec pj_rtcp_ntp_rec;

/**
 * RTCP session.
 */
struct pj_rtcp_session
{
    pj_rtcp_pkt		rtcp_pkt;
    
    pj_rtp_seq_session	seq_ctrl;

    pj_uint32_t		received;       /* packets received */
    pj_uint32_t		expected_prior; /* packet expected at last interval */
    pj_uint32_t		received_prior; /* packet received at last interval */
    pj_int32_t		transit;        /* relative trans time for prev pkt */
    pj_uint32_t		jitter;		/* estimated jitter */
    
    pj_rtcp_ntp_rec	rtcp_lsr;	/* NTP timestamp in last sender report received */
    unsigned 		rtcp_lsr_time;  /* Time when last RTCP SR is received. */
    unsigned 		peer_ssrc;	/* Peer SSRC */
    
};

typedef struct pj_rtcp_session pj_rtcp_session;

/**
 * Init RTCP session.
 * @param session The session
 * @param ssrc The SSRC used in to identify the session.
 */
PJ_DECL(void) pj_rtcp_init( pj_rtcp_session *session, pj_uint32_t ssrc );

/**
 * Deinit RTCP session.
 * @param session The session.
 */
PJ_DECL(void) pj_rtcp_fini( pj_rtcp_session *session);

/**
 * Call this function everytime an RTP packet is received to let the RTCP
 * session do its internal calculations.
 * @param session The session.
 * @param seq The RTP packet sequence number, in host byte order.
 * @param ts The RTP packet timestamp, in host byte order.
 */
PJ_DECL(void) pj_rtcp_rx_rtp( pj_rtcp_session *session, pj_uint16_t seq, pj_uint32_t ts );

/**
 * Call this function everytime an RTP packet is sent to let the RTCP session
 * do its internal calculations.
 * @param session The session.
 * @param bytes_payload_size The payload size of the RTP packet (ie packet minus
 *             RTP header).
 */
PJ_DECL(void) pj_rtcp_tx_rtp( pj_rtcp_session *session, pj_uint16_t bytes_payload_size );

/**
 * Build a RTCP SR/RR packet to be transmitted to remote RTP peer.
 * @param session The session.
 * @param rtcp_pkt [output] Upon return, it will contain pointer to the RTCP packet.
 * @param len [output] Upon return, it will indicate the size of the RTCP packet.
 */
PJ_DECL(void) pj_rtcp_build_rtcp( pj_rtcp_session *session, pj_rtcp_pkt **rtcp_pkt, int *len );

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_RTCP_H__ */

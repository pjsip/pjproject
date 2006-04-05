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
#include <pjmedia/rtcp.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/sock.h>
#include <pj/string.h>

#define THIS_FILE "rtcp.c"

#define RTCP_SR   200
#define RTCP_RR   201


#if PJ_HAS_HIGH_RES_TIMER==0
#   error "High resolution timer needs to be enabled"
#endif

#if 0
#   define TRACE_(x)	PJ_LOG(3,x)
#else
#   define TRACE_(x)
#endif

/*
 * Get NTP time.
 */
static void rtcp_get_ntp_time(const pjmedia_rtcp_session *s, 
			      struct pjmedia_rtcp_ntp_rec *ntp)
{
    pj_time_val tv;
    pj_timestamp ts;

    pj_gettimeofday(&tv);
    pj_get_timestamp(&ts);
    
    /* Fill up the high 32bit part */
    ntp->hi = tv.sec;
    
    /* Calculate second fractions */
    ts.u64 %= s->ts_freq.u64;
    ts.u64 = (ts.u64 << 32) / s->ts_freq.u64;

    /* Fill up the low 32bit part */
    ntp->lo = ts.u32.lo;
}


PJ_DEF(void) pjmedia_rtcp_init(pjmedia_rtcp_session *s, 
			       unsigned clock_rate,
			       pj_uint32_t ssrc)
{
    pjmedia_rtcp_pkt *rtcp_pkt = &s->rtcp_pkt;
    
    pj_memset(rtcp_pkt, 0, sizeof(pjmedia_rtcp_pkt));
    
    /* Set clock rate */
    s->clock_rate = clock_rate;

    /* Init time */
    s->rtcp_lsr.hi = s->rtcp_lsr.lo = 0;
    s->rtcp_lsr_time.u64 = 0;
    
    /* Init common RTCP header */
    rtcp_pkt->common.version = 2;
    rtcp_pkt->common.count = 1;
    rtcp_pkt->common.pt = RTCP_SR;
    rtcp_pkt->common.length = pj_htons(12);
    
    /* Init SR */
    rtcp_pkt->sr.ssrc = pj_htonl(ssrc);
    
    /* Get timestamp frequency */
    pj_get_timestamp_freq(&s->ts_freq);

    /* RR will be initialized on receipt of the first RTP packet. */
}


PJ_DEF(void) pjmedia_rtcp_fini(pjmedia_rtcp_session *session)
{
    /* Nothing to do. */
    PJ_UNUSED_ARG(session);
}

static void rtcp_init_seq(pjmedia_rtcp_session *s, pj_uint16_t  seq)
{
    s->received = 0;
    s->exp_prior = 0;
    s->rx_prior = 0;
    s->transit = 0;
    s->jitter = 0;

    pjmedia_rtp_seq_restart(&s->seq_ctrl, seq);
}

PJ_DEF(void) pjmedia_rtcp_rx_rtp(pjmedia_rtcp_session *s, 
				 pj_uint16_t seq, 
				 pj_uint32_t rtp_ts)
{   
    pj_timestamp ts;
    pj_uint32_t arrival;
    pj_int32_t transit;
    int status;

    /* Update sequence numbers (received, lost, etc). */
    status = pjmedia_rtp_seq_update(&s->seq_ctrl, seq);
    if (status == PJMEDIA_RTP_ESESSRESTART) {
	rtcp_init_seq(s, seq);
	status = 0;
    }
    
    if (status != 0)
	return;

    ++s->received;

    /*
     * Calculate jitter (see RFC 3550 section A.8)
     */
    
    /* Get arrival time and convert timestamp to samples */
    pj_get_timestamp(&ts);
    ts.u64 = ts.u64 * s->clock_rate / s->ts_freq.u64;
    arrival = ts.u32.lo;

    transit = arrival - rtp_ts;
    
    if (s->transit == 0) {
	s->transit = transit;
    } else {
	pj_int32_t d;
	
	d = transit - s->transit;
	s->transit = transit;
	if (d < 0) 
	    d = -d;
	
	s->jitter += d - ((s->jitter + 8) >> 4);
    }
}

PJ_DEF(void) pjmedia_rtcp_tx_rtp(pjmedia_rtcp_session *s, 
				 pj_uint16_t  bytes_payload_size)
{
    pjmedia_rtcp_pkt *rtcp_pkt = &s->rtcp_pkt;

    /* Update number of packets */
    rtcp_pkt->sr.sender_pcount = 
	pj_htonl( pj_ntohl(rtcp_pkt->sr.sender_pcount) + 1);

    /* Update number of bytes */
    rtcp_pkt->sr.sender_bcount = 
	pj_htonl( pj_ntohl(rtcp_pkt->sr.sender_bcount) + bytes_payload_size );
}


PJ_DEF(void) pjmedia_rtcp_rx_rtcp( pjmedia_rtcp_session *session,
				   const void *pkt,
				   pj_size_t size)
{
    const struct pjmedia_rtcp_pkt *rtcp = pkt;

    /* Must at least contain SR */
    pj_assert(size >= sizeof(pjmedia_rtcp_common)+sizeof(pjmedia_rtcp_sr));

    /* Save NTP timestamp */
    session->rtcp_lsr.hi = pj_ntohl(rtcp->sr.ntp_sec);
    session->rtcp_lsr.lo = pj_ntohl(rtcp->sr.ntp_frac);

    /* Calculate SR arrival time for DLSR */
    pj_get_timestamp(&session->rtcp_lsr_time);

    /* Calculate RTT if it has RR */
    if (size >= sizeof(pjmedia_rtcp_pkt)) {
	
	/* Can only calculate if LSR and DLSR is present in RR */
	if (rtcp->rr.lsr && rtcp->rr.dlsr) {
	    pj_uint32_t lsr, now, dlsr;
	    pj_uint64_t eedelay;
	    pjmedia_rtcp_ntp_rec ntp;

	    /* LSR is the middle 32bit of NTP. It has 1/65536 second 
	     * resolution 
	     */
	    lsr = pj_ntohl(rtcp->rr.lsr);

	    /* DLSR is delay since LSR, also in 1/65536 resolution */
	    dlsr = pj_ntohl(rtcp->rr.dlsr);

	    /* Get current time, and convert to 1/65536 resolution */
	    rtcp_get_ntp_time(session, &ntp);
	    now = ((ntp.hi & 0xFFFF) << 16) + 
		  (ntp.lo >> 16);

	    /* End-to-end delay is (now-lsr-dlsr) */
	    eedelay = now - lsr - dlsr;

	    /* Convert end to end delay to usec (keeping the calculation in
             * 64bit space)::
	     *   session->ee_delay = (eedelay * 1000) / 65536;
	     */
	    eedelay = (eedelay * 1000000) >> 16;

	    TRACE_((THIS_FILE, "Rx RTCP: lsr=%p, dlsr=%p (%d:%03dms), "
			       "now=%p, rtt=%p",
		    lsr, dlsr, dlsr/65536, (dlsr%65536)*1000/65536,
		    now, (pj_uint32_t)eedelay));
	    
	    /* Only save calculation if "now" is greater than lsr, or
	     * otherwise rtt will be invalid 
	     */
	    if (now-dlsr >= lsr) {
		session->rtt_us = (pj_uint32_t)eedelay;
	    } else {
		TRACE_((THIS_FILE, "NTP clock running backwards?"));
	    }
	}
    }
}


static void rtcp_build_rtcp(pjmedia_rtcp_session *s, 
			    pj_uint32_t receiver_ssrc)
{   
    pj_uint32_t expected;
    pj_uint32_t u32;
    pj_uint32_t expected_interval, received_interval, lost_interval;
    pjmedia_rtcp_pkt *rtcp_pkt = &s->rtcp_pkt;
    
    /* SSRC and last_seq */
    rtcp_pkt->rr.ssrc = pj_htonl(receiver_ssrc);
    rtcp_pkt->rr.last_seq = (s->seq_ctrl.cycles & 0xFFFF0000L);
    rtcp_pkt->rr.last_seq += s->seq_ctrl.max_seq;
    rtcp_pkt->rr.last_seq = pj_htonl(rtcp_pkt->rr.last_seq);

    /* Jitter */
    rtcp_pkt->rr.jitter = pj_htonl(s->jitter >> 4);
    
    /* Total lost. */
    expected = pj_ntohl(rtcp_pkt->rr.last_seq) - s->seq_ctrl.base_seq;
    u32 = expected - s->received;
    rtcp_pkt->rr.total_lost_2 = (u32 >> 16) & 0x00FF;
    rtcp_pkt->rr.total_lost_1 = (u32 >> 8) & 0x00FF;
    rtcp_pkt->rr.total_lost_0 = u32 & 0x00FF;

    /* Fraction lost calculation */
    expected_interval = expected - s->exp_prior;
    s->exp_prior = expected;
    
    received_interval = s->received - s->rx_prior;
    s->rx_prior = s->received;
    
    lost_interval = expected_interval - received_interval;
    
    if (expected_interval==0 || lost_interval == 0) {
	rtcp_pkt->rr.fract_lost = 0;
    } else {
	rtcp_pkt->rr.fract_lost = (lost_interval << 8) / expected_interval;
    }
}

PJ_DEF(void) pjmedia_rtcp_build_rtcp(pjmedia_rtcp_session *session, 
				     pjmedia_rtcp_pkt **ret_p_pkt, 
				     int *len)
{
    pjmedia_rtcp_pkt *rtcp_pkt = &session->rtcp_pkt;
    pjmedia_rtcp_ntp_rec ntp;
    
    rtcp_build_rtcp(session, session->peer_ssrc);
    
    /* Get current NTP time. */
    rtcp_get_ntp_time(session, &ntp);
    
    /* Fill in NTP timestamp in SR. */
    rtcp_pkt->sr.ntp_sec = pj_htonl(ntp.hi);
    rtcp_pkt->sr.ntp_frac = pj_htonl(ntp.lo);
    
    if (session->rtcp_lsr_time.u64 == 0) {
	rtcp_pkt->rr.lsr = 0;
	rtcp_pkt->rr.dlsr = 0;
    } else {
	pj_timestamp ts;
	
	/* Fill in LSR.
	   LSR is the middle 32bit of the last SR NTP time received.
	 */
	rtcp_pkt->rr.lsr = ((session->rtcp_lsr.hi & 0x0000FFFF) << 16) | 
			   ((session->rtcp_lsr.lo >> 16) & 0xFFFF);
	rtcp_pkt->rr.lsr = pj_htonl(rtcp_pkt->rr.lsr);
	
	/* Fill in DLSR.
	   DLSR is Delay since Last SR, in 1/65536 seconds.
	 */
	pj_get_timestamp(&ts);

	/* Calculate DLSR */
	ts.u64 -= session->rtcp_lsr_time.u64;

	/* Convert interval to 1/65536 seconds value */
	ts.u64 = ((ts.u64 - session->rtcp_lsr_time.u64) << 16) / 
		    session->ts_freq.u64;

	rtcp_pkt->rr.dlsr = pj_htonl( (pj_uint32_t)ts.u64 );
    }
    

    /* Return pointer. */
    *ret_p_pkt = rtcp_pkt;
    *len = sizeof(pjmedia_rtcp_pkt);
}

 

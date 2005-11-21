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
#include <pj/os.h>	/* pj_gettimeofday */
#include <pj/sock.h>	/* pj_htonx, pj_ntohx */
#include <string.h>	/* memset */

#define RTCP_SR   200
#define RTCP_RR   201



/*
 * Get NTP time.
 */
static void rtcp_get_ntp_time(struct pj_rtcp_ntp_rec *ntp)
{
    pj_time_val tv;

    pj_gettimeofday(&tv);
    
    ntp->hi = tv.sec;
    tv.msec = tv.msec % 1000;
    ntp->lo = tv.msec * 0xFFFF / 1000;
    ntp->lo <<= 16;
}


PJ_DEF(void) pj_rtcp_init(pj_rtcp_session *s, pj_uint32_t ssrc)
{
    pj_rtcp_pkt *rtcp_pkt = &s->rtcp_pkt;
    
    memset(rtcp_pkt, 0, sizeof(pj_rtcp_pkt));
    
    /* Init time */
    s->rtcp_lsr.hi = s->rtcp_lsr.lo = 0;
    s->rtcp_lsr_time = 0;
    
    /* Init common RTCP header */
    rtcp_pkt->common.version = 2;
    rtcp_pkt->common.count = 1;
    rtcp_pkt->common.pt = RTCP_SR;
    rtcp_pkt->common.length = pj_htons(12);
    
    /* Init SR */
    rtcp_pkt->sr.ssrc = pj_htonl(ssrc);
    
    /* RR will be initialized on receipt of the first RTP packet. */
}

PJ_DEF(void) pj_rtcp_fini(pj_rtcp_session *session)
{
    /* Nothing to do. */
    PJ_UNUSED_ARG(session)
}

static void rtcp_init_seq(pj_rtcp_session *s, pj_uint16_t  seq)
{
    s->received = 0;
    s->expected_prior = 0;
    s->received_prior = 0;
    s->transit = 0;
    s->jitter = 0;

    pj_rtp_seq_restart(&s->seq_ctrl, seq);
}

PJ_DEF(void) pj_rtcp_rx_rtp(pj_rtcp_session *s, pj_uint16_t seq, pj_uint32_t rtp_ts)
{   
    pj_uint32_t arrival;
    pj_int32_t transit;
    unsigned long timer_tick;
    pj_time_val tv;
    int status;

    /* Update sequence numbers (received, lost, etc). */
    status = pj_rtp_seq_update(&s->seq_ctrl, seq);
    if (status == PJ_RTP_ERR_SESSION_RESTARTED) {
	rtcp_init_seq(s, seq);
	status = 0;
    }
    
    if (status != 0)
	return;

    ++s->received;

    pj_gettimeofday(&tv);
    timer_tick = tv.sec * 1000 + tv.msec;
    
    /*
     * Calculate jitter (s->jitter is in timer tick unit)
     */
    PJ_TODO(SUPPORT_JITTER_CALCULATION_FOR_NON_8KHZ_SAMPLE_RATE)

    arrival = timer_tick << 3;	// 8 samples per ms.
    transit = arrival - rtp_ts;
    
    if (s->transit == 0) {
	s->transit = transit;
    } else {
	pj_int32_t d, jitter = s->jitter;
	
	d = transit - s->transit;
	s->transit = transit;
	if (d < 0) 
	    d = -d;
	
	jitter += d - ((jitter + 8) >> 4);
	s->jitter = jitter;
    }
}

PJ_DEF(void) pj_rtcp_tx_rtp(pj_rtcp_session *s, pj_uint16_t  bytes_payload_size)
{
    pj_rtcp_pkt *rtcp_pkt = &s->rtcp_pkt;
    rtcp_pkt->sr.sender_pcount = pj_htonl( pj_ntohl(rtcp_pkt->sr.sender_pcount) + 1);
    rtcp_pkt->sr.sender_bcount = pj_htonl( pj_ntohl(rtcp_pkt->sr.sender_bcount) + bytes_payload_size );
}

static void rtcp_build_rtcp(pj_rtcp_session *s, pj_uint32_t receiver_ssrc)
{   
    pj_uint32_t expected;
    pj_uint32_t u32;
    pj_uint32_t expected_interval, received_interval, lost_interval;
    pj_rtcp_pkt *rtcp_pkt = &s->rtcp_pkt;
    
    /* SSRC and last_seq */
    rtcp_pkt->rr.ssrc = pj_htonl(receiver_ssrc);
    rtcp_pkt->rr.last_seq = (s->seq_ctrl.cycles & 0xFFFF0000L);
    rtcp_pkt->rr.last_seq += s->seq_ctrl.max_seq;
    rtcp_pkt->rr.last_seq = pj_htonl(rtcp_pkt->rr.last_seq);

    /* Jitter */
    rtcp_pkt->rr.jitter = pj_htonl(s->jitter >> 4);
    
    /* Total lost. */
    expected = pj_ntohl(rtcp_pkt->rr.last_seq) - s->seq_ctrl.base_seq + 1;
    u32 = expected - s->received;
    rtcp_pkt->rr.total_lost_2 = (u32 >> 16) & 0x00FF;
    rtcp_pkt->rr.total_lost_1 = (u32 >> 8) & 0x00FF;
    rtcp_pkt->rr.total_lost_0 = u32 & 0x00FF;

    /* Fraction lost calculation */
    expected_interval = expected - s->expected_prior;
    s->expected_prior = expected;
    
    received_interval = s->received - s->received_prior;
    s->received_prior = s->received;
    
    lost_interval = expected_interval - received_interval;
    
    if (expected_interval==0 || lost_interval == 0) {
	rtcp_pkt->rr.fract_lost = 0;
    } else {
	rtcp_pkt->rr.fract_lost = (lost_interval << 8) / expected_interval;
    }
}

PJ_DEF(void) pj_rtcp_build_rtcp(pj_rtcp_session *session, pj_rtcp_pkt **ret_p_pkt, int *len)
{
    pj_rtcp_pkt *rtcp_pkt = &session->rtcp_pkt;
    pj_rtcp_ntp_rec ntp;
    pj_time_val now;
    
    rtcp_build_rtcp(session, session->peer_ssrc);
    
    /* Get current NTP time. */
    rtcp_get_ntp_time(&ntp);
    
    /* Fill in NTP timestamp in SR. */
    rtcp_pkt->sr.ntp_sec = pj_htonl(ntp.hi);
    rtcp_pkt->sr.ntp_frac = pj_htonl(ntp.lo);
    
    if (session->rtcp_lsr_time == 0 || session->rtcp_lsr.lo == 0) {
	rtcp_pkt->rr.lsr = 0;
	rtcp_pkt->rr.dlsr = 0;
    } else {
	unsigned msec_elapsed;
	
	/* Fill in LSR.
	   LSR is the middle 32bit of the last SR NTP time received.
	 */
	rtcp_pkt->rr.lsr = ((session->rtcp_lsr.hi & 0x0000FFFF) << 16) | 
			   ((session->rtcp_lsr.lo >> 16) & 0xFFFF);
	rtcp_pkt->rr.lsr = pj_htonl(rtcp_pkt->rr.lsr);
	
	/* Fill in DLSR.
	   DLSR is Delay since Last SR, in 1/65536 seconds.
	 */
	pj_gettimeofday(&now);
	msec_elapsed = (now.msec - session->rtcp_lsr_time);
	rtcp_pkt->rr.dlsr = pj_htonl((msec_elapsed * 65536) / 1000);
    }
    
    /* Return pointer. */
    *ret_p_pkt = rtcp_pkt;
    *len = sizeof(pj_rtcp_pkt);
}


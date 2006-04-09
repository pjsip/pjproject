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
#   define TRACE_(x)	;
#endif


/*
 * Get NTP time.
 */
PJ_DEF(pj_status_t) pjmedia_rtcp_get_ntp_time(const pjmedia_rtcp_session *sess,
					      pjmedia_rtcp_ntp_rec *ntp)
{
/* Seconds between 1900-01-01 to 1970-01-01 */
#define NTP_DIFF    ((70 * 365 + 17) * 86400UL)
    pj_timestamp ts;
    pj_status_t status;

    status = pj_get_timestamp(&ts);

    /* Fill up the high 32bit part */
    ntp->hi = (pj_uint32_t)((ts.u64 - sess->ts_base.u64) / sess->ts_freq.u64)
	      + sess->tv_base.sec + NTP_DIFF;

    /* Calculate seconds fractions */
    ts.u64 %= sess->ts_freq.u64;
    pj_assert(ts.u64 < sess->ts_freq.u64);
    ts.u64 = (ts.u64 << 32) / sess->ts_freq.u64;

    /* Fill up the low 32bit part */
    ntp->lo = ts.u32.lo;


#if (defined(PJ_WIN32) && PJ_WIN32!=0) || \
    (defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0)

    /* On Win32, since we use QueryPerformanceCounter() as the backend
     * timestamp API, we need to protect against this bug:
     *   Performance counter value may unexpectedly leap forward
     *   http://support.microsoft.com/default.aspx?scid=KB;EN-US;Q274323
     */
    {
	/*
	 * Compare elapsed time reported by timestamp with actual elapsed 
	 * time. If the difference is too excessive, then we use system
	 * time instead.
	 */

	/* MIN_DIFF needs to be large enough so that "normal" diff caused
	 * by system activity or context switch doesn't trigger the time
	 * correction.
	 */
	enum { MIN_DIFF = 400 };

	pj_time_val ts_time, elapsed, diff;

	pj_gettimeofday(&elapsed);

	ts_time.sec = ntp->hi - sess->tv_base.sec - NTP_DIFF;
	ts_time.msec = (long)(ntp->lo * 1000.0 / 0xFFFFFFFF);

	PJ_TIME_VAL_SUB(elapsed, sess->tv_base);

	if (PJ_TIME_VAL_LT(ts_time, elapsed)) {
	    diff = elapsed;
	    PJ_TIME_VAL_SUB(diff, ts_time);
	} else {
	    diff = ts_time;
	    PJ_TIME_VAL_SUB(diff, elapsed);
	}

	if (PJ_TIME_VAL_MSEC(diff) >= MIN_DIFF) {

	    TRACE_((THIS_FILE, "NTP timestamp corrected by %d ms",
		    PJ_TIME_VAL_MSEC(diff)));


	    ntp->hi = elapsed.sec + sess->tv_base.sec + NTP_DIFF;
	    ntp->lo = (elapsed.msec * 65536 / 1000) << 16;
	}

    }
#endif

    return status;
}


PJ_DEF(void) pjmedia_rtcp_init(pjmedia_rtcp_session *sess, 
			       unsigned clock_rate,
			       unsigned samples_per_frame,
			       pj_uint32_t ssrc)
{
    pjmedia_rtcp_pkt *rtcp_pkt = &sess->rtcp_pkt;
    
    pj_memset(rtcp_pkt, 0, sizeof(pjmedia_rtcp_pkt));
    
    /* Set clock rate */
    sess->clock_rate = clock_rate;
    sess->pkt_size = samples_per_frame;

    /* Init time */
    sess->rx_lsr = 0;
    sess->rx_lsr_time.u64 = 0;
    
    /* Init common RTCP header */
    rtcp_pkt->common.version = 2;
    rtcp_pkt->common.count = 1;
    rtcp_pkt->common.pt = RTCP_SR;
    rtcp_pkt->common.length = pj_htons(12);
    
    /* Init SR */
    rtcp_pkt->sr.ssrc = pj_htonl(ssrc);
    
    /* Get time and timestamp base and frequency */
    pj_gettimeofday(&sess->tv_base);
    pj_get_timestamp(&sess->ts_base);
    pj_get_timestamp_freq(&sess->ts_freq);

    /* RR will be initialized on receipt of the first RTP packet. */
}


PJ_DEF(void) pjmedia_rtcp_fini(pjmedia_rtcp_session *sess)
{
    /* Nothing to do. */
    PJ_UNUSED_ARG(sess);
}

static void rtcp_init_seq(pjmedia_rtcp_session *sess)
{
    sess->received = 0;
    sess->exp_prior = 0;
    sess->rx_prior = 0;
    sess->transit = 0;
    sess->jitter = 0;
}

PJ_DEF(void) pjmedia_rtcp_rx_rtp(pjmedia_rtcp_session *sess, 
				 unsigned seq, 
				 unsigned rtp_ts,
				 unsigned payload)
{   
    pj_timestamp ts;
    pj_uint32_t arrival;
    pj_int32_t transit;
    pjmedia_rtp_status seq_st;
    unsigned last_seq;

    sess->stat.rx.pkt++;
    sess->stat.rx.bytes += payload;

    /* Update sequence numbers. */
    last_seq = sess->seq_ctrl.max_seq;
    pjmedia_rtp_seq_update(&sess->seq_ctrl, (pj_uint16_t)seq, &seq_st);
    if (seq_st.status.flag.restart) {
	rtcp_init_seq(sess);
    }
    
    if (seq_st.status.flag.dup)
	sess->stat.rx.dup++;
    if (seq_st.status.flag.outorder)
	sess->stat.rx.reorder++;

    if (seq_st.status.flag.bad) {
	sess->stat.rx.discard++;
	return;
    }


    /* Only mark "good" packets */
    ++sess->received;


    /*
     * Calculate jitter (see RFC 3550 section A.8)
     */
    
    /* Get arrival time and convert timestamp to samples */
    pj_get_timestamp(&ts);
    ts.u64 = ts.u64 * sess->clock_rate / sess->ts_freq.u64;
    arrival = ts.u32.lo;

    transit = arrival - rtp_ts;
    
    /* Ignore the first N packets as they normally have bad jitter
     * due to other threads working to establish the call
     */
    if (sess->transit == 0 || sess->received < 25 ) {
	sess->transit = transit;
	sess->stat.rx.jitter.min = 2000;
    } else {
	pj_int32_t d;
	pj_uint32_t jitter;
	
	d = transit - sess->transit;
	sess->transit = transit;
	if (d < 0) 
	    d = -d;
	
	sess->jitter += d - ((sess->jitter + 8) >> 4);

	/* Get jitter in usec */
	if (d < 4294)
	    jitter = d * 1000000 / sess->clock_rate;
	else {
	    jitter = d * 1000 / sess->clock_rate;
	    jitter *= 1000;
	}

	/* Update jitter stat */
	if (jitter < sess->stat.rx.jitter.min)
	    sess->stat.rx.jitter.min = jitter;
	if (jitter > sess->stat.rx.jitter.max)
	    sess->stat.rx.jitter.max = jitter;
	sess->stat.rx.jitter.last = jitter;
    }
}

PJ_DEF(void) pjmedia_rtcp_tx_rtp(pjmedia_rtcp_session *sess, 
				 unsigned bytes_payload_size)
{
    /* Update statistics */
    sess->stat.tx.pkt++;
    sess->stat.tx.bytes += bytes_payload_size;
}


PJ_DEF(void) pjmedia_rtcp_rx_rtcp( pjmedia_rtcp_session *sess,
				   const void *pkt,
				   pj_size_t size)
{
    const struct pjmedia_rtcp_pkt *rtcp = pkt;
    pj_uint32_t last_loss, jitter_samp, jitter;

    /* Must at least contain SR */
    pj_assert(size >= sizeof(pjmedia_rtcp_common)+sizeof(pjmedia_rtcp_sr));

    /* Save LSR from NTP timestamp of RTCP packet */
    sess->rx_lsr = ((pj_ntohl(rtcp->sr.ntp_sec) & 0x0000FFFF) << 16) | 
		   ((pj_ntohl(rtcp->sr.ntp_frac) >> 16) & 0xFFFF);

    /* Calculate SR arrival time for DLSR */
    pj_get_timestamp(&sess->rx_lsr_time);

    TRACE_((THIS_FILE, "Rx RTCP SR: ntp_ts=%p", 
	    sess->rx_lsr,
	    (pj_uint32_t)(sess->rx_lsr_time.u64*65536/sess->ts_freq.u64)));

    /* Nothing more to do if this is an SR only RTCP packet */
    if (size < sizeof(pjmedia_rtcp_pkt))
	return;
	

    last_loss = sess->stat.tx.loss;

    /* Get packet loss */
    sess->stat.tx.loss = (rtcp->rr.total_lost_2 << 16) +
			 (rtcp->rr.total_lost_1 << 8) +
			  rtcp->rr.total_lost_0;
    
    /* We can't calculate the exact loss period for TX, so just give the
     * best estimation.
     */
    if (sess->stat.tx.loss > last_loss) {
	unsigned period;

	/* Loss period in msec */
	period = (sess->stat.tx.loss - last_loss) * sess->pkt_size *
		 1000 / sess->clock_rate;

	/* Loss period in usec */
	period *= 1000;

	if (sess->stat.tx.update_cnt==0||sess->stat.tx.loss_period.min==0)
	    sess->stat.tx.loss_period.min = period;
	if (period < sess->stat.tx.loss_period.min)
	    sess->stat.tx.loss_period.min = period;
	if (period > sess->stat.tx.loss_period.max)
	    sess->stat.tx.loss_period.max = period;

	sess->stat.tx.loss_period.avg = 
	    (sess->stat.tx.loss_period.avg*sess->stat.tx.update_cnt+period)
	    / (sess->stat.tx.update_cnt + 1);
	sess->stat.tx.loss_period.last = period;
    }

    /* Get jitter value in usec */
    jitter_samp = pj_ntohl(rtcp->rr.jitter);
    /* Calculate jitter in usec, avoiding overflows */
    if (jitter_samp <= 4294)
	jitter = jitter_samp * 1000000 / sess->clock_rate;
    else {
	jitter = jitter_samp * 1000 / sess->clock_rate;
	jitter *= 1000;
    }

    /* Update jitter statistics */
    if (sess->stat.tx.update_cnt == 0)
	sess->stat.tx.jitter.min = jitter;
    if (jitter < sess->stat.tx.jitter.min && jitter)
	sess->stat.tx.jitter.min = jitter;
    if (jitter > sess->stat.tx.jitter.max)
	sess->stat.tx.jitter.max = jitter;
    sess->stat.tx.jitter.avg = 
	(sess->stat.tx.jitter.avg * sess->stat.tx.update_cnt + jitter) /
	(sess->stat.tx.update_cnt + 1);
    sess->stat.tx.jitter.last = jitter;


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
	pjmedia_rtcp_get_ntp_time(sess, &ntp);
	now = ((ntp.hi & 0xFFFF) << 16) + (ntp.lo >> 16);

	/* End-to-end delay is (now-lsr-dlsr) */
	eedelay = now - lsr - dlsr;

	/* Convert end to end delay to usec (keeping the calculation in
         * 64bit space)::
	 *   sess->ee_delay = (eedelay * 1000) / 65536;
	 */
	if (eedelay < 4294) {
	    eedelay = (eedelay * 1000000) >> 16;
	} else {
	    eedelay = (eedelay * 1000) >> 16;
	    eedelay *= 1000;
	}

	TRACE_((THIS_FILE, "Rx RTCP RR: lsr=%p, dlsr=%p (%d:%03dms), "
			   "now=%p, rtt=%p",
		lsr, dlsr, dlsr/65536, (dlsr%65536)*1000/65536,
		now, (pj_uint32_t)eedelay));
	
	/* Only save calculation if "now" is greater than lsr, or
	 * otherwise rtt will be invalid 
	 */
	if (now-dlsr >= lsr) {
	    unsigned rtt = (pj_uint32_t)eedelay;
	    
	    TRACE_((THIS_FILE, "RTT is set to %d usec", rtt));

	    if (rtt >= 1000000) {
		pjmedia_rtcp_ntp_rec ntp2;
		pj_thread_sleep(50);
		pjmedia_rtcp_get_ntp_time(sess, &ntp2);
		ntp2.lo = ntp2.lo;
	    }

	    if (sess->stat.rtt_update_cnt == 0)
		sess->stat.rtt.min = rtt;

	    if (rtt < sess->stat.rtt.min && rtt)
		sess->stat.rtt.min = rtt;
	    if (rtt > sess->stat.rtt.max)
		sess->stat.rtt.max = rtt;

	    sess->stat.rtt.avg = 
		(sess->stat.rtt.avg * sess->stat.rtt_update_cnt + rtt) / 
		(sess->stat.rtt_update_cnt + 1);

	    sess->stat.rtt.last = rtt;
	    sess->stat.rtt_update_cnt++;

	} else {
	    PJ_LOG(5, (THIS_FILE, "Internal NTP clock skew detected: "
				   "lsr=%p, now=%p, dlsr=%p (%d:%03dms), "
				   "diff=%d",
				   lsr, now, dlsr, dlsr/65536,
				   (dlsr%65536)*1000/65536,
				   dlsr-(now-lsr)));
	}
    }

    pj_gettimeofday(&sess->stat.tx.update);
    sess->stat.tx.update_cnt++;
}


PJ_DEF(void) pjmedia_rtcp_build_rtcp(pjmedia_rtcp_session *sess, 
				     pjmedia_rtcp_pkt **ret_p_pkt, 
				     int *len)
{
    pj_uint32_t expected, expected_interval, received_interval, lost_interval;
    pj_uint32_t jitter_samp, jitter;
    pjmedia_rtcp_pkt *rtcp_pkt = &sess->rtcp_pkt;
    pj_timestamp ts_now;
    pjmedia_rtcp_ntp_rec ntp;
    
    /* Packet count */
    rtcp_pkt->sr.sender_pcount = pj_htonl(sess->stat.tx.pkt);

    /* Octets count */
    rtcp_pkt->sr.sender_bcount = pj_htonl(sess->stat.tx.bytes);
    
    /* SSRC and last_seq */
    rtcp_pkt->rr.ssrc = pj_htonl(sess->peer_ssrc);
    rtcp_pkt->rr.last_seq = (sess->seq_ctrl.cycles & 0xFFFF0000L);
    rtcp_pkt->rr.last_seq += sess->seq_ctrl.max_seq;
    rtcp_pkt->rr.last_seq = pj_htonl(rtcp_pkt->rr.last_seq);


    /* Jitter */
    jitter_samp = (sess->jitter >> 4);
    rtcp_pkt->rr.jitter = pj_htonl(jitter_samp);
    
    /* Calculate jitter in usec, avoiding overflows */
    if (jitter_samp <= 4294)
	jitter = jitter_samp * 1000000 / sess->clock_rate;
    else {
	jitter = jitter_samp * 1000 / sess->clock_rate;
	jitter *= 1000;
    }

    /* Update jitter statistics */
    sess->stat.rx.jitter.avg = 
	(sess->stat.rx.jitter.avg * sess->stat.rx.update_cnt + jitter) /
	(sess->stat.rx.update_cnt + 1);
    
    /* Total lost. */
    expected = pj_ntohl(rtcp_pkt->rr.last_seq) - sess->seq_ctrl.base_seq;
    if (expected >= sess->received)
	sess->stat.rx.loss = expected - sess->received;
    rtcp_pkt->rr.total_lost_2 = (sess->stat.rx.loss >> 16) & 0xFF;
    rtcp_pkt->rr.total_lost_1 = (sess->stat.rx.loss >> 8) & 0xFF;
    rtcp_pkt->rr.total_lost_0 = (sess->stat.rx.loss & 0xFF);

    /* Fraction lost calculation */
    expected_interval = expected - sess->exp_prior;
    sess->exp_prior = expected;
    
    received_interval = sess->received - sess->rx_prior;
    sess->rx_prior = sess->received;
    
    lost_interval = expected_interval - received_interval;
    
    if (expected_interval==0 || lost_interval == 0) {
	rtcp_pkt->rr.fract_lost = 0;
    } else {
	rtcp_pkt->rr.fract_lost = (lost_interval << 8) / expected_interval;
    }
    
    /* Get current NTP time. */
    pj_get_timestamp(&ts_now);
    pjmedia_rtcp_get_ntp_time(sess, &ntp);
    
    /* Fill in NTP timestamp in SR. */
    rtcp_pkt->sr.ntp_sec = pj_htonl(ntp.hi);
    rtcp_pkt->sr.ntp_frac = pj_htonl(ntp.lo);

    TRACE_((THIS_FILE, "TX RTCP SR: ntp_ts=%p", 
		       ((ntp.hi & 0xFFFF) << 16) + ((ntp.lo & 0xFFFF0000) 
			    >> 16)));

    if (sess->rx_lsr_time.u64 == 0 || sess->rx_lsr == 0) {
	rtcp_pkt->rr.lsr = 0;
	rtcp_pkt->rr.dlsr = 0;
    } else {
	pj_timestamp ts;
	pj_uint32_t lsr = sess->rx_lsr;
	pj_uint64_t lsr_time = sess->rx_lsr_time.u64;
	pj_uint32_t dlsr;
	
	/* Convert LSR time to 1/65536 seconds resolution */
	lsr_time = (lsr_time << 16) / sess->ts_freq.u64;

	/* Fill in LSR.
	   LSR is the middle 32bit of the last SR NTP time received.
	 */
	rtcp_pkt->rr.lsr = pj_htonl(lsr);
	
	/* Fill in DLSR.
	   DLSR is Delay since Last SR, in 1/65536 seconds.
	 */
	ts.u64 = ts_now.u64;

	/* Convert interval to 1/65536 seconds value */
	ts.u64 = (ts.u64 << 16) / sess->ts_freq.u64;

	/* Get DLSR */
	dlsr = (pj_uint32_t)(ts.u64 - lsr_time);
	rtcp_pkt->rr.dlsr = pj_htonl(dlsr);

	TRACE_((THIS_FILE, "Tx RTCP RR: lsr=%p, lsr_time=%p, now=%p, dlsr=%p"
			   "(%ds:%03dms)",
			   lsr, 
			   (pj_uint32_t)lsr_time,
			   (pj_uint32_t)ts.u64, 
			   dlsr,
			   dlsr/65536,
			   (dlsr%65536)*1000/65536 ));
    }
    
    /* Update counter */
    pj_gettimeofday(&sess->stat.rx.update);
    sess->stat.rx.update_cnt++;


    /* Return pointer. */
    *ret_p_pkt = rtcp_pkt;
    *len = sizeof(pjmedia_rtcp_pkt);
}

 

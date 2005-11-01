/* $Header: /pjproject-0.3/pjmedia/src/pjmedia/rtp.c 5     10/14/05 12:23a Bennylp $ */

#include <pjmedia/rtp.h>
#include <pj/log.h>
#include <pj/os.h>	/* pj_gettimeofday() */
#include <pj/sock.h>	/* pj_htonx, pj_htonx */
#include <string.h>	/* memset() */

#define THIS_FILE   "rtp.c"

#define RTP_VERSION	2

#define RTP_SEQ_MOD	(1 << 16)
#define MAX_DROPOUT 	((pj_int16_t)3000)
#define MAX_MISORDER 	((pj_int16_t)100)
#define MIN_SEQUENTIAL  ((pj_int16_t)2)


PJ_DEF(pj_status_t) pj_rtp_session_init( pj_rtp_session *ses,
					 int default_pt, pj_uint32_t sender_ssrc )
{
    PJ_LOG(4, (THIS_FILE, "pj_rtp_session_init: ses=%p, default_pt=%d, ssrc=0x%x",
	       ses, default_pt, sender_ssrc));

    /* Check RTP header packing. */
    if (sizeof(struct pj_rtp_hdr) != 12) {
	pj_assert(!"Wrong RTP header packing!");
	return PJ_RTP_ERR_RTP_PACKING;
    }

    /* If sender_ssrc is not specified, create from time value. */
    if (sender_ssrc == 0 || sender_ssrc == (pj_uint32_t)-1) {
	pj_time_val tv;

	pj_gettimeofday(&tv);
	sender_ssrc  = (pj_uint32_t) pj_htonl(tv.sec);
    } else {
	sender_ssrc = pj_htonl(sender_ssrc);
    }

    /* Initialize session. */
    ses->out_extseq = 0;
    ses->peer_ssrc = 0;
    
    /* Sequence number will be initialized when the first RTP packet is receieved. */

    /* Build default header for outgoing RTP packet. */
    memset(ses, 0, sizeof(*ses));
    ses->out_hdr.v = RTP_VERSION;
    ses->out_hdr.p = 0;
    ses->out_hdr.x = 0;
    ses->out_hdr.cc = 0;
    ses->out_hdr.m = 0;
    ses->out_hdr.pt = (pj_uint8_t) default_pt;
    ses->out_hdr.seq = (pj_uint16_t) pj_htons( (pj_uint16_t)ses->out_extseq );
    ses->out_hdr.ts = 0;
    ses->out_hdr.ssrc = sender_ssrc;

    /* Keep some arguments as session defaults. */
    ses->out_pt = (pj_uint16_t) default_pt;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_rtp_encode_rtp( pj_rtp_session *ses, int pt, int m,
				       int payload_len, int ts_len,
				       const void **rtphdr, int *hdrlen )
{
    PJ_UNUSED_ARG(payload_len)

    PJ_LOG(6, (THIS_FILE, 
	      "pj_rtp_encode_rtp: ses=%p, pt=%d, m=%d, pt_len=%d, ts_len=%d",
	      ses, pt, m, payload_len, ts_len));

    /* Update session. */
    ses->out_extseq++;
    ses->out_hdr.ts = pj_htonl(pj_ntohl(ses->out_hdr.ts)+ts_len);

    /* Create outgoing header. */
    ses->out_hdr.pt = (pj_uint8_t) ((pt == -1) ? ses->out_pt : pt);
    ses->out_hdr.m = (pj_uint16_t) m;
    ses->out_hdr.seq = pj_htons( (pj_uint16_t) ses->out_extseq);

    /* Return values */
    *rtphdr = &ses->out_hdr;
    *hdrlen = sizeof(pj_rtp_hdr);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_rtp_decode_rtp( pj_rtp_session *ses, 
				       const void *pkt, int pkt_len,
				       const pj_rtp_hdr **hdr,
				       const void **payload,
				       unsigned *payloadlen)
{
    int offset;

    PJ_UNUSED_ARG(ses)

    PJ_LOG(6, (THIS_FILE, 
	      "pj_rtp_decode_rtp: ses=%p, pkt=%p, pkt_len=%d",
	      ses, pkt, pkt_len));

    /* Assume RTP header at the start of packet. We'll verify this later. */
    *hdr = (pj_rtp_hdr*)pkt;

    /* Check RTP header sanity. */
    if ((*hdr)->v != RTP_VERSION) {
	PJ_LOG(4, (THIS_FILE, "  invalid RTP version!"));
	return PJ_RTP_ERR_INVALID_VERSION;
    }

    /* Payload is located right after header plus CSRC */
    offset = sizeof(pj_rtp_hdr) + ((*hdr)->cc * sizeof(pj_uint32_t));

    /* Adjust offset if RTP extension is used. */
    if ((*hdr)->x) {
	pj_rtp_ext_hdr *ext = (pj_rtp_ext_hdr*) (((pj_uint8_t*)pkt) + offset);
	offset += (pj_ntohs(ext->length) * sizeof(pj_uint32_t));
    }

    /* Check that offset is less than packet size */
    if (offset >= pkt_len)
	return PJ_RTP_ERR_INVALID_PACKET;

    /* Find and set payload. */
    *payload = ((pj_uint8_t*)pkt) + offset;
    *payloadlen = pkt_len - offset;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_rtp_session_update( pj_rtp_session *ses, const pj_rtp_hdr *hdr)
{
    int status;

    /* Check SSRC. */
    if (ses->peer_ssrc == 0) ses->peer_ssrc = pj_ntohl(hdr->ssrc);
    /*
    if (pj_ntohl(ses->peer_ssrc) != hdr->ssrc) {
	PJ_LOG(4, (THIS_FILE, "pj_rtp_session_update: ses=%p, invalid ssrc 0x%p (!=0x%p)",
		   ses, pj_ntohl(hdr->ssrc), ses->peer_ssrc));
	return PJ_RTP_ERR_INVALID_SSRC;
    }
    */

    /* Check payload type. */
    if (hdr->pt != ses->out_pt) {
	PJ_LOG(4, (THIS_FILE, "pj_rtp_session_update: ses=%p, invalid payload type %d (!=%d)",
		   ses, hdr->pt, ses->out_pt));
	return PJ_RTP_ERR_INVALID_PT;
    }

    /* Initialize sequence number on first packet received. */
    if (ses->received == 0)
	pj_rtp_seq_init( &ses->seq_ctrl, pj_ntohs(hdr->seq) );

    /* Check sequence number to see if remote session has been restarted. */
    status = pj_rtp_seq_update( &ses->seq_ctrl, pj_ntohs(hdr->seq));
    if (status == PJ_RTP_ERR_SESSION_RESTARTED) {
	pj_rtp_seq_restart( &ses->seq_ctrl, pj_ntohs(hdr->seq));
	++ses->received;
    } else if (status == 0 || status == PJ_RTP_ERR_SESSION_PROBATION) {
	++ses->received;
    }


    return status;
}


void pj_rtp_seq_restart(pj_rtp_seq_session *sctrl, pj_uint16_t seq)
{
    sctrl->base_seq = seq;
    sctrl->max_seq = seq;
    sctrl->bad_seq = RTP_SEQ_MOD + 1;
    sctrl->cycles = 0;
}


void pj_rtp_seq_init(pj_rtp_seq_session *sctrl, pj_uint16_t seq)
{
    pj_rtp_seq_restart(sctrl, seq);

    sctrl->max_seq = (pj_uint16_t) (seq - 1);
    sctrl->probation = MIN_SEQUENTIAL;
}


int pj_rtp_seq_update(pj_rtp_seq_session *sctrl, pj_uint16_t seq)
{
    pj_uint16_t udelta = (pj_uint16_t) (seq - sctrl->max_seq);
    
    /*
     * Source is not valid until MIN_SEQUENTIAL packets with
     * sequential sequence numbers have been received.
     */
    if (sctrl->probation) {
	/* packet is in sequence */
        if (seq == sctrl->max_seq+ 1) {
	    sctrl->probation--;
            sctrl->max_seq = seq;
            if (sctrl->probation == 0) {
                return PJ_RTP_ERR_SESSION_RESTARTED;
            }
	} else {
	    sctrl->probation = MIN_SEQUENTIAL - 1;
	    sctrl->max_seq = seq;
        }
        return PJ_RTP_ERR_SESSION_PROBATION;

    } else if (udelta < MAX_DROPOUT) {
	/* in order, with permissible gap */
	if (seq < sctrl->max_seq) {
	    /* Sequence number wrapped - count another 64K cycle. */
	    sctrl->cycles += RTP_SEQ_MOD;
        }
        sctrl->max_seq = seq;

    } else if (udelta <= (RTP_SEQ_MOD - MAX_MISORDER)) {
	/* the sequence number made a very large jump */
        if (seq == sctrl->bad_seq) {
	    /*
	     * Two sequential packets -- assume that the other side
	     * restarted without telling us so just re-sync
	     * (i.e., pretend this was the first packet).
	     */
	    return PJ_RTP_ERR_SESSION_RESTARTED;
	}
        else {
	    sctrl->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
            return PJ_RTP_ERR_BAD_SEQUENCE;
        }
    } else {
	/* duplicate or reordered packet */
    }
    
    return 0;
}



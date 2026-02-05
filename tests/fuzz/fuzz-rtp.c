/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>
#include <pjmedia.h>
#include <pjmedia/rtp.h>
#include <pjmedia/jbuf.h>

/*
 * Minimum input length calculation:
 *   - Bytes 0-4: Session config (PT + SSRC)
 *   - Bytes 5-16: First RTP packet for decode_rtp test (12 bytes)
 *   - Bytes 17-28: Second RTP packet for decode_rtp2 test (12 bytes)
 *   - Bytes 29-35: Encode test parameters (7 bytes)
 *   - Bytes 36+: Multi-packet simulation
 *
 * Minimum: 5 config + 12 decode1 + 12 decode2 + 7 encode = 36 bytes
 */
#define kMinInputLength 36
#define kMaxInputLength 5120

/* For RTP session testing with randomized memory pool */
pj_pool_factory *mem;

/* Fixed Input byte offsets */
#define OFFSET_PT           0   /* Byte 0: Payload Type (7 bits) */
#define OFFSET_SSRC         1   /* Bytes 1-4: SSRC (32 bits) */
#define OFFSET_DECODE1      5   /* Bytes 5-16: First packet for decode_rtp test (12 bytes) */
#define OFFSET_DECODE2      17  /* Bytes 17-28: Second packet for decode_rtp2 test (12 bytes) */
#define OFFSET_ENCODE       29  /* Bytes 29-35: Encode parameters (7 bytes) */
#define OFFSET_MULTI_PKT    36  /* Bytes 36+: Multiple packet simulation */

/*
 * Extract RTP session configuration from fuzzer input
 */
static void setup_rtp_session_config(const uint8_t *data,
                                     uint8_t *pt,
                                     uint32_t *ssrc)
{
    /* Payload Type: 7 bits (0-127) */
    *pt = data[OFFSET_PT] & 0x7F;

    /* SSRC: 32-bit synchronization source identifier */
    *ssrc = ((uint32_t)data[OFFSET_SSRC] << 24) |
            ((uint32_t)data[OFFSET_SSRC + 1] << 16) |
            ((uint32_t)data[OFFSET_SSRC + 2] << 8) |
            ((uint32_t)data[OFFSET_SSRC + 3]);
}

/*
 * Test basic RTP packet decoding (pjmedia_rtp_decode_rtp)
 * Tests: header parsing, payload extraction, session state updates
 */
static void test_decode_basic(pjmedia_rtp_session *session,
                              const uint8_t *data, size_t size)
{
    pj_status_t status;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;
    pjmedia_rtp_status seq_st;

    if (size < OFFSET_DECODE1 + 12)
        return;

    status = pjmedia_rtp_decode_rtp(session, data + OFFSET_DECODE1,
                                    (int)(size - OFFSET_DECODE1),
                                    &hdr, &payload, &payloadlen);

    if (status == PJ_SUCCESS && hdr != NULL) {
        /* Update session state - tests sequence number tracking */
        pjmedia_rtp_session_update(session, hdr, &seq_st);

        /* Test update with check_pt disabled */
        pjmedia_rtp_session_update2(session, hdr, NULL, PJ_FALSE);
    }
}

/*
 * Test extended RTP packet decoding (pjmedia_rtp_decode_rtp2)
 * Tests: extension header parsing, detailed header info extraction
 */
static void test_decode_extended(pjmedia_rtp_session *session,
                                 const uint8_t *data, size_t size)
{
    pj_status_t status;
    pjmedia_rtp_dec_hdr dec_hdr;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;
    pjmedia_rtp_status seq_st;

    if (size < OFFSET_DECODE2 + 12)
        return;

    status = pjmedia_rtp_decode_rtp2(session, data + OFFSET_DECODE2,
                                     (int)(size - OFFSET_DECODE2),
                                     &hdr, &dec_hdr, &payload, &payloadlen);

    if (status == PJ_SUCCESS && hdr != NULL) {
        pjmedia_rtp_session_update(session, hdr, &seq_st);

        /* Access extension header if present */
        if (dec_hdr.ext_hdr && dec_hdr.ext && dec_hdr.ext_len > 0) {
            pj_ntohs(dec_hdr.ext_hdr->profile_data);
            pj_ntohs(dec_hdr.ext_hdr->length);
        }
    }
}

/*
 * Test RTP packet encoding (pjmedia_rtp_encode_rtp)
 * Tests: header generation, roundtrip encode->decode
 */
static void test_encode_roundtrip(pjmedia_rtp_session *session,
                                  const uint8_t *data, size_t size)
{
    pj_status_t status;
    const void *rtphdr = NULL;
    int hdrlen = 0;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;

    if (size < OFFSET_ENCODE + 7)
        return;

    /* Extract encode parameters from fuzzer input */
    pj_bool_t marker = (data[OFFSET_ENCODE] & 0x80) ? PJ_TRUE : PJ_FALSE;
    int pt_encode = data[OFFSET_ENCODE + 1] & 0x7F;
    int payload_len = data[OFFSET_ENCODE + 2];
    uint32_t ts = ((uint32_t)data[OFFSET_ENCODE + 3] << 24) |
                  ((uint32_t)data[OFFSET_ENCODE + 4] << 16) |
                  ((uint32_t)data[OFFSET_ENCODE + 5] << 8) |
                  ((uint32_t)data[OFFSET_ENCODE + 6]);

    /* Encode RTP header */
    status = pjmedia_rtp_encode_rtp(session, pt_encode, marker,
                                    payload_len, ts, &rtphdr, &hdrlen);

    if (status == PJ_SUCCESS && rtphdr != NULL && hdrlen > 0) {
        /* Roundtrip test: decode what we just encoded */
        pjmedia_rtp_status seq_st;
        status = pjmedia_rtp_decode_rtp(session, rtphdr, hdrlen,
                                        &hdr, &payload, &payloadlen);
        if (status == PJ_SUCCESS && hdr != NULL) {
            pjmedia_rtp_session_update(session, hdr, &seq_st);
        }
    }
}

/*
 * Test multiple packet simulation with jitter buffer
 * Tests: sequence number handling, packet loss detection, jitter buffer reordering
 */
static void test_multiple_packets(pjmedia_rtp_session *session,
                                  const uint8_t *data, size_t size,
                                  pj_pool_t *pool)
{
    pj_status_t status;
    const pjmedia_rtp_hdr *hdr;
    const void *payload;
    unsigned payloadlen;
    pjmedia_rtp_status seq_st;
    size_t offset = OFFSET_MULTI_PKT;
    int packet_num = 0;
    pjmedia_jbuf *jb = NULL;

    if (size <= OFFSET_MULTI_PKT)
        return;

    /* Jitter buffer configuration */
    unsigned frame_size = 160;
    unsigned ptime = 20;
    unsigned max_count = 50;
    unsigned prefetch = 10;

    /* Create jitter buffer with fixed, realistic parameters */
    pj_str_t jb_name = pj_str("fuzz-jb");
    status = pjmedia_jbuf_create(pool, &jb_name, frame_size, ptime, max_count, &jb);
    if (status != PJ_SUCCESS) {
        return;
    }

    /* Configure jitter buffer prefetch (initial delay) using adaptive mode */
    pjmedia_jbuf_set_adaptive(jb, prefetch, 5, max_count);

    /* Process up to 16 packets to test more complex scenarios */
    while (offset + 1 + 12 < size && packet_num < 16) {
        /* First byte: packet size (12-200 bytes) */
        uint8_t pkt_size = data[offset];
        if (pkt_size < 12) pkt_size = 12;
        if (pkt_size > 200) pkt_size = 200;

        offset++;

        if (offset + pkt_size > size)
            break;

        /* Decode packet */
        status = pjmedia_rtp_decode_rtp(session, data + offset, pkt_size,
                                        &hdr, &payload, &payloadlen);
        if (status == PJ_SUCCESS && hdr != NULL) {
            /* Update RTP session state */
            pjmedia_rtp_session_update(session, hdr, &seq_st);

            /* Test different jitter buffer operations based on fuzzer input */
            int seq = pj_ntohs(hdr->seq);

            /* Use packet number to decide which JB operation to test */
            if (payloadlen > 0 && (packet_num & 1)) {
                /* Put frame using basic API */
                pjmedia_jbuf_put_frame(jb, payload, payloadlen, seq);
            } else if (payloadlen > 0) {
                /* Put frame using extended API with bit_info */
                pj_uint32_t bit_info = payloadlen * 8;
                pj_bool_t discarded = PJ_FALSE;
                pjmedia_jbuf_put_frame2(jb, payload, payloadlen, bit_info, seq, &discarded);
            }

            /* Periodically test jitter buffer reset */
            if (packet_num == 5) {
                pjmedia_jbuf_reset(jb);
            }

            /* Test jitter buffer state query */
            if (packet_num % 4 == 0) {
                pjmedia_jb_state jb_state;
                pjmedia_jbuf_get_state(jb, &jb_state);
            }

            /* Exercise jitter buffer GET operations to drive dequeue path */
            if (packet_num > prefetch && packet_num % 3 == 0) {
                pjmedia_jb_state jb_state;
                pjmedia_jbuf_get_state(jb, &jb_state);

                /* Only call GET if there are frames in the buffer */
                if (jb_state.size > 0) {
                    char frame_type;
                    pj_size_t frame_size_out = jb_state.frame_size;
                    pj_uint32_t bit_info;
                    pj_uint32_t ts;
                    int seq_out;
                    char out_buf[512];

                    /* Test get_frame3 (most comprehensive API) */
                    pjmedia_jbuf_get_frame3(jb, out_buf, &frame_size_out,
                                            &frame_type, &bit_info,
                                            &ts, &seq_out);
                }
            }
        }

        offset += pkt_size;
        packet_num++;
    }

    pjmedia_jbuf_destroy(jb);
}

/*
 * Main RTP fuzzing test function
 */
void rtp_test(const uint8_t *data, size_t size)
{
    pj_status_t status;
    pj_pool_t *pool;
    pjmedia_rtp_session session;
    uint8_t pt;
    uint32_t ssrc;

    /* Extract session configuration from fuzzer input */
    setup_rtp_session_config(data, &pt, &ssrc);

    /* Create memory pool */
    pool = pj_pool_create(mem, "rtp_test", 4000, 4000, NULL);
    if (!pool)
        return;

    /* Initialize RTP session with fuzzer-controlled PT and SSRC */
    status = pjmedia_rtp_session_init(&session, pt, ssrc);
    if (status != PJ_SUCCESS) {
        pj_pool_release(pool);
        return;
    }

    /* Test basic decoding */
    test_decode_basic(&session, data, size);

    /* Test extended decoding with header extensions */
    test_decode_extended(&session, data, size);

    /* Test encode path and roundtrip */
    test_encode_roundtrip(&session, data, size);

    /* Test multiple packet scenarios with jitter buffer */
    test_multiple_packets(&session, data, size, pool);

    pj_pool_release(pool);
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    pj_caching_pool caching_pool;

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    /* Init */
    pj_init();
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    /* Configure the global blocking pool */
    mem = &caching_pool.factory;

    /* Fuzz RTP */
    rtp_test(Data, Size);

    /* Cleanup */
    pj_caching_pool_destroy(&caching_pool);

    return 0;
}

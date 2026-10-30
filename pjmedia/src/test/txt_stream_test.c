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
#include <pjmedia/endpoint.h>
#include <pjmedia/errno.h>
#include <pjmedia/rtp.h>
#include <pjmedia/transport.h>
#include <pjmedia/txt_stream.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>
#include "test.h"

#define THIS_FILE       "txt_stream_test.c"

/* Text stream rx sequence handling test.
 *
 * RTP packets with crafted sequence numbers are fed to a text stream via
 * a stub transport, and the text delivered to the application is checked.
 * Lost text markers (U+FFFD) are shown as '?'.
 */

#define T140_PT         98
#define RED_PT          100
#define SSRC            0x1234
#define MAX_PKTS        20
#define MAX_OUT         64

/* Stub transport, it only keeps the rx callback to feed packets. */
typedef struct stub_tp
{
    pjmedia_transport    base;
    void                *user_data;
    void               (*rtp_cb2)(pjmedia_tp_cb_param*);
} stub_tp;

static pj_status_t stub_get_info(pjmedia_transport *tp,
                                 pjmedia_transport_info *info)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(info);
    return PJ_SUCCESS;
}

static pj_status_t stub_attach2(pjmedia_transport *tp,
                                pjmedia_transport_attach_param *att_param)
{
    stub_tp *stub = (stub_tp*)tp;

    stub->user_data = att_param->user_data;
    stub->rtp_cb2 = att_param->rtp_cb2;
    return PJ_SUCCESS;
}

static void stub_detach(pjmedia_transport *tp, void *user_data)
{
    stub_tp *stub = (stub_tp*)tp;

    PJ_UNUSED_ARG(user_data);
    stub->user_data = NULL;
    stub->rtp_cb2 = NULL;
}

static pj_status_t stub_send_rtp(pjmedia_transport *tp, const void *pkt,
                                 pj_size_t size)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(size);
    return PJ_SUCCESS;
}

static pj_status_t stub_send_rtcp(pjmedia_transport *tp, const void *pkt,
                                  pj_size_t size)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(size);
    return PJ_SUCCESS;
}

static pj_status_t stub_send_rtcp2(pjmedia_transport *tp,
                                   const pj_sockaddr_t *addr,
                                   unsigned addr_len, const void *pkt,
                                   pj_size_t size)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(addr);
    PJ_UNUSED_ARG(addr_len);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(size);
    return PJ_SUCCESS;
}

static pj_status_t stub_destroy(pjmedia_transport *tp)
{
    PJ_UNUSED_ARG(tp);
    return PJ_SUCCESS;
}

static pjmedia_transport_op stub_op =
{
    &stub_get_info,
    NULL,               /* attach */
    &stub_detach,
    &stub_send_rtp,
    &stub_send_rtcp,
    &stub_send_rtcp2,
    NULL,               /* media_create */
    NULL,               /* encode_sdp */
    NULL,               /* media_start */
    NULL,               /* media_stop */
    NULL,               /* simulate_lost */
    &stub_destroy,
    &stub_attach2
};

/* Text received by the application */
typedef struct rx_text
{
    char        buf[MAX_OUT];
    unsigned    len;
} rx_text;

static void on_rx_text(pjmedia_txt_stream *stream, void *user_data,
                       const pjmedia_txt_stream_data *data)
{
    rx_text *out = (rx_text*)user_data;

    PJ_UNUSED_ARG(stream);

    if (out->len + 1 >= MAX_OUT)
        return;

    if (data->text.slen == 3 &&
        pj_memcmp(data->text.ptr, "\xEF\xBF\xBD", 3) == 0)
    {
        out->buf[out->len++] = '?';
    } else {
        unsigned n = (unsigned)data->text.slen;

        if (out->len + n >= MAX_OUT)
            n = MAX_OUT - 1 - out->len;
        pj_memcpy(out->buf + out->len, data->text.ptr, n);
        out->len += n;
    }
    out->buf[out->len] = '\0';
}

/* Text sent by the remote in the packet with the specified sequence:
 * a..z from seq 100, N..Q after the remote restarts at seq 40000,
 * A..P around the sequence wrap, X for stray packets, and nothing
 * otherwise.
 */
static unsigned text_of(pj_uint16_t seq, char *buf)
{
    if (seq >= 100 && seq < 140) {
        buf[0] = (char)('a' + (seq - 100));
        return 1;
    }
    if (seq >= 40000 && seq < 40004) {
        buf[0] = (char)('N' + (seq - 40000));
        return 1;
    }
    if (seq >= 65530 || seq < 10) {
        buf[0] = (char)('A' + (pj_uint16_t)(seq - 65530));
        return 1;
    }
    if (seq == 2100 || seq == 2101 || seq == 5000 || seq == 20100 ||
        seq == 20101)
    {
        buf[0] = 'X';
        return 1;
    }
    return 0;
}

/* Build a T.140 packet, or a RED packet carrying the text of the
 * previous red_level packets as redundant blocks (RFC 4103). The blocks
 * before first_seq are empty, unless it is -1.
 */
static unsigned build_pkt(pj_uint16_t seq, pj_uint32_t ssrc, int red_level,
                          int first_seq, pj_uint8_t *pkt)
{
    pjmedia_rtp_hdr *hdr = (pjmedia_rtp_hdr*)pkt;
    pj_uint32_t ts = (pj_uint32_t)seq * 300;
    pj_uint8_t *p = pkt + sizeof(pjmedia_rtp_hdr);
    char text[1];
    unsigned len;
    int i;

    pj_bzero(hdr, sizeof(*hdr));
    hdr->v = 2;
    hdr->pt = red_level ? RED_PT : T140_PT;
    hdr->seq = pj_htons(seq);
    hdr->ts = pj_htonl(ts);
    hdr->ssrc = pj_htonl(ssrc);

    if (red_level == 0) {
        len = text_of(seq, text);
        pj_memcpy(p, text, len);
        return (unsigned)sizeof(pjmedia_rtp_hdr) + len;
    }

    /* Redundant block headers: F bit, PT, ts offset (14), length (10) */
    for (i = red_level; i > 0; --i) {
        pj_uint32_t offset = (pj_uint32_t)i * 300;

        len = text_of((pj_uint16_t)(seq - i), text);
        if (first_seq != -1 && (pj_int16_t)(seq - i - first_seq) < 0)
            len = 0;
        *p++ = (pj_uint8_t)(0x80 | T140_PT);
        *p++ = (pj_uint8_t)(offset >> 6);
        *p++ = (pj_uint8_t)(((offset << 2) & 0xFC) | (len >> 8));
        *p++ = (pj_uint8_t)(len & 0xFF);
    }
    /* Primary block header */
    *p++ = T140_PT;

    for (i = red_level; i >= 0; --i) {
        len = text_of((pj_uint16_t)(seq - i), text);
        if (first_seq != -1 && (pj_int16_t)(seq - i - first_seq) < 0)
            len = 0;
        pj_memcpy(p, text, len);
        p += len;
    }

    return (unsigned)(p - pkt);
}

typedef struct scenario
{
    const char  *title;
    unsigned     cnt;
    pj_uint16_t  seq[MAX_PKTS];
    const char  *expected[2];       /* T.140, RED level 2 */
    unsigned     ssrc_mask;         /* Bit i set: packet i has another
                                       SSRC (no SSRC in SDP) */
    int          ssrc_start;        /* First sequence of the other SSRC,
                                       its earlier RED blocks are empty */
    pj_bool_t    has_rem_ssrc;      /* SSRC in SDP */
} scenario;

static const scenario scenarios[] =
{
    { "packet lost at stream start", 4,
      { 100, 102, 103, 104 },
      { "a?cde", "abcde" } },

    { "reordering at stream start", 5,
      { 100, 102, 101, 103, 104 },
      { "a?cde", "abcde" } },

    /* Plain T.140 drops a skip of more than MAX_PROBATION_SKIP, while RED
     * accepts it if its redundant blocks fill the gap.
     */
    { "deeper reordering at stream start", 5,
      { 100, 103, 101, 102, 104 },
      { "abc?e", "abcde" } },

    { "two packets lost at stream start", 4,
      { 100, 103, 104, 105 },
      { "a???ef", "abcdef" } },

    /* Skipping ahead more than MAX_PROBATION_SKIP during probation is
     * dropped, so the packets in between are not discarded as old.
     */
    { "early jump during probation", 9,
      { 100, 107, 101, 102, 103, 104, 105, 106, 108 },
      { "abcdefg?i", "abcdefghi" } },

    { "stray packet during probation", 5,
      { 100, 20100, 101, 102, 103 },
      { "abcd", "abcd" } },

    /* Two consecutive strays look like a sequence jump */
    { "two consecutive stray packets", 6,
      { 100, 20100, 20101, 101, 102, 103 },
      { "a???????X?cd", "a?????XXbcd" } },

    { "stray packet as the first packet", 5,
      { 5000, 100, 101, 102, 103 },
      { "X?bcd", "Xabcd" } },

    /* RTP accepts this jump (< MAX_DROPOUT), it needs confirmation */
    { "stray packet after probation", 6,
      { 100, 101, 102, 2100, 103, 104 },
      { "abcde", "abcde" } },

    /* After RTP accepted a stray, the next real packet is RTP-bad but
     * not in probation.
     */
    { "loss after stray packet", 6,
      { 100, 101, 102, 2100, 105, 106 },
      { "abc??fg", "abcdefg" } },

    /* A later stray must not confirm an earlier one */
    { "strays interleaved with the stream", 10,
      { 100, 101, 102, 2100, 103, 104, 105, 2101, 106, 107 },
      { "abcdefgh", "abcdefgh" } },

    { "long loss", 7,
      { 100, 101, 102, 123, 124, 125, 126 },
      { "abc???????yz{", "abc?????wxyz{" } },

    /* A reordered packet far ahead is not a late packet */
    { "long loss with reordering", 7,
      { 100, 101, 102, 124, 123, 125, 126 },
      { "abc???????x?z{", "abc?????vwxyz{" } },

    /* A late packet must not cancel a pending jump */
    { "late packet during long loss", 10,
      { 100, 101, 102, 103, 104, 105, 115, 104, 116, 117 },
      { "abcdef???????qr", "abcdef?????opqr" } },

    { "packet lost at sequence wrap", 7,
      { 65532, 65533, 65534, 0, 1, 2, 3 },
      { "CDE?GHIJ", "CDEFGHIJ" } },

    { "late packets", 16,
      { 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
        113, 100, 101 },
      { "abcdefghijklmn", "abcdefghijklmn" } },

    { "remote sequence restart", 7,
      { 100, 101, 102, 103, 40000, 40001, 40002 },
      { "abcd?OP", "abcdNOP" } },

    /* The unconfirmed packet is not skipped on restart */
    { "remote sequence restart with loss", 6,
      { 100, 101, 102, 103, 40000, 40002 },
      { "abcd??P", "abcdNOP" } },

    { "remote sequence restart with first packet lost", 6,
      { 100, 101, 102, 103, 40001, 40002 },
      { "abcd?P", "abcdNOP" } },

    /* Known limitation: a restart back by up to RTP's MAX_MISORDER (100)
     * with the same SSRC looks like late packets.
     */
    { "remote sequence restart back by less than 100", 9,
      { 110, 111, 112, 113, 114, 115, 100, 101, 102 },
      { "klmnop", "klmnop" } },

    /* A new SSRC restarts the sequence once confirmed */
    { "remote restart with a new SSRC", 7,
      { 100, 101, 102, 103, 40000, 40001, 40002 },
      { "abcd?OP", "abcdNOP" }, 0x70, 40000 },

    { "new SSRC with a nearby sequence", 7,
      { 100, 101, 102, 103, 120, 121, 122 },
      { "abcd?vw", "abcduvw" }, 0x70, 120 },

    { "new SSRC with a lower sequence", 9,
      { 110, 111, 112, 113, 114, 115, 100, 101, 102 },
      { "klmnop?bc", "klmnopabc" }, 0x1C0, 100 },

    { "new SSRC with loss", 6,
      { 100, 101, 102, 103, 40001, 40002 },
      { "abcd?P", "abcdNOP" }, 0x30, 40000 },

    { "new SSRC with reordering", 7,
      { 100, 101, 102, 103, 40001, 40000, 40002 },
      { "abcdN?P", "abcdNOP" }, 0x70, 40000 },

    { "stray packet with another SSRC", 6,
      { 100, 101, 102, 5000, 103, 104 },
      { "abcde", "abcde" }, 0x08, 5000 },

    { "interleaved SSRCs", 7,
      { 100, 101, 40000, 102, 40001, 103, 40002 },
      { "abcd", "abcd" }, 0x54, 40000 },

    /* Text already shown is not shown again when switching back */
    { "switch back to an earlier SSRC", 9,
      { 100, 101, 102, 103, 40000, 40001, 40002, 104, 105 },
      { "abcd?OP?f", "abcdNOPef" }, 0x70, 40000 },

    { "late packets of an earlier SSRC", 10,
      { 100, 101, 102, 103, 40000, 40001, 40002, 102, 103, 40003 },
      { "abcd?OPQ", "abcdNOPQ" }, 0x270, 40000 },

    /* Two strays confirm each other, like a real restart */
    { "two strays with another SSRC", 7,
      { 100, 101, 102, 5000, 5001, 103, 104 },
      { "abc??e", "abcXde" }, 0x18, 5000 },

    /* With SSRC in SDP, packets with another SSRC are dropped */
    { "strays with another SSRC, SSRC in SDP", 7,
      { 100, 101, 102, 5000, 5001, 103, 104 },
      { "abcde", "abcde" }, 0x18, 5000, PJ_TRUE },

    { "another SSRC during probation, SSRC in SDP", 4,
      { 100, 102, 103, 104 },
      { "a??de", "abcde" }, 0x02, 102, PJ_TRUE },
};

/* RTCP counts an RTP-bad packet as discarded only if the stream did */
static const struct rtcp_scenario
{
    scenario     sc;
    unsigned     discard;           /* Expected RTCP discard count */
} rtcp_scenarios[] =
{
    { { "RTCP discard of a used RTP-bad packet", 4,
        { 100, 102, 103, 104 },
        { "a?cde", "abcde" } }, 0 },

    { { "RTCP discard of a stray packet", 5,
        { 100, 20100, 101, 102, 103 },
        { "abcd", "abcd" } }, 1 },
};

/* Run a scenario, optionally returning the RTCP statistics */
static int run_scenario(pjmedia_endpt *endpt, const scenario *sc,
                        int red_level, pjmedia_rtcp_stat *stat)
{
    const char *expected = sc->expected[red_level ? 1 : 0];
    pjmedia_txt_stream_info si;
    pjmedia_txt_stream *stream = NULL;
    stub_tp stub;
    rx_text out;
    pj_str_t loopback = pj_str("127.0.0.1");
    unsigned i;
    int rc = 0;

    pj_bzero(&stub, sizeof(stub));
    pj_ansi_strxcpy(stub.base.name, "stub", sizeof(stub.base.name));
    stub.base.op = &stub_op;

    pj_bzero(&out, sizeof(out));

    pj_bzero(&si, sizeof(si));
    si.type = PJMEDIA_TYPE_TEXT;
    si.proto = PJMEDIA_TP_PROTO_RTP_AVP;
    si.dir = PJMEDIA_DIR_DECODING;
    pj_sockaddr_init(pj_AF_INET(), &si.rem_addr, &loopback, 4000);
    si.fmt.type = PJMEDIA_TYPE_TEXT;
    si.fmt.pt = T140_PT;
    si.fmt.encoding_name = pj_str("t140");
    si.fmt.clock_rate = 1000;
    si.tx_pt = si.rx_pt = T140_PT;
    if (red_level) {
        si.tx_red_pt = si.rx_red_pt = RED_PT;
        si.tx_red_level = si.rx_red_level = red_level;
    }
    si.rtcp_sdes_bye_disabled = PJ_TRUE;
    si.has_rem_ssrc = sc->has_rem_ssrc;
    si.rem_ssrc = SSRC;

    PJ_TEST_SUCCESS(pjmedia_txt_stream_create(endpt, NULL, &si, &stub.base,
                                              NULL, &stream),
                    sc->title, return -10);
    PJ_TEST_SUCCESS(pjmedia_txt_stream_set_rx_callback(stream, &on_rx_text,
                                                       &out, 0),
                    sc->title, {rc = -20; goto on_return;});
    PJ_TEST_SUCCESS(pjmedia_txt_stream_start(stream),
                    sc->title, {rc = -30; goto on_return;});
    PJ_TEST_NOT_NULL(stub.rtp_cb2, sc->title, {rc = -40; goto on_return;});

    for (i = 0; i < sc->cnt; ++i) {
        pj_uint8_t pkt[128];
        pjmedia_tp_cb_param param;
        pj_bool_t other = (sc->ssrc_mask & (1 << i)) != 0;

        pj_bzero(&param, sizeof(param));
        param.user_data = stub.user_data;
        param.pkt = pkt;
        param.size = build_pkt(sc->seq[i], other ? SSRC + 1 : SSRC,
                               red_level, other ? sc->ssrc_start : -1, pkt);
        (*stub.rtp_cb2)(&param);
    }

    if (pj_ansi_strcmp(out.buf, expected) != 0) {
        PJ_LOG(1, (THIS_FILE, "%s (%s): expecting \"%s\", got \"%s\"",
                   sc->title, red_level ? "RED" : "T.140", expected,
                   out.buf));
        rc = -50;
    }

    if (stat) {
        PJ_TEST_SUCCESS(pjmedia_stream_common_get_stat(
                        (pjmedia_stream_common *) stream, stat),
                        sc->title, rc = -60);
    }

on_return:
    pjmedia_txt_stream_destroy(stream);
    return rc;
}

int txt_stream_test(void)
{
    pjmedia_endpt *endpt;
    unsigned i;
    int status, rc = 0;

    PJ_TEST_SUCCESS(pjmedia_endpt_create2(mem, NULL, 0, &endpt),
                    NULL, return -1);

    /* Run all scenarios to report every mismatch */
    for (i = 0; i < PJ_ARRAY_SIZE(scenarios); ++i) {
        PJ_LOG(3, (THIS_FILE, "  %s", scenarios[i].title));
        status = run_scenario(endpt, &scenarios[i], 0, NULL);
        if (status != 0 && rc == 0)
            rc = status;
        status = run_scenario(endpt, &scenarios[i], 2, NULL);
        if (status != 0 && rc == 0)
            rc = status;
    }

    for (i = 0; i < PJ_ARRAY_SIZE(rtcp_scenarios); ++i) {
        const struct rtcp_scenario *rs = &rtcp_scenarios[i];
        pjmedia_rtcp_stat stat;
        int red_level;

        PJ_LOG(3, (THIS_FILE, "  %s", rs->sc.title));
        for (red_level = 0; red_level <= 2; red_level += 2) {
            pj_bzero(&stat, sizeof(stat));
            status = run_scenario(endpt, &rs->sc, red_level, &stat);
            if (status == 0 && stat.rx.discard != rs->discard) {
                PJ_LOG(1, (THIS_FILE, "%s (%s): expecting %u RTCP discard, "
                           "got %u", rs->sc.title,
                           red_level ? "RED" : "T.140", rs->discard,
                           stat.rx.discard));
                status = -70;
            }
            if (status != 0 && rc == 0)
                rc = status;
        }
    }

    pjmedia_endpt_destroy2(endpt);
    return rc;
}

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

#include <pjmedia.h>
#include <pjmedia/transport_srtp.h>
#include "test.h"

#define THIS_FILE "transport_srtp_test.c"


/* Tests for the SRTP RX roll over counter (ROC) handling:
 * - a RX stream initialized with a ROC that does not match the sender is
 *   recovered by retrying with the alternative ROC candidate,
 * - the ROC that authenticated is written back to the setting, so a later
 *   SRTP restart re-applies the correct one,
 * - a remote SSRC change is still accepted after the probation period,
 *   which requires the wildcard template to be kept alongside the specific
 *   stream in the RX policy list.
 *
 * The transports are connected through an in-memory loopback pair, so the
 * whole protect/unprotect path is exercised without sockets.
 */

#define TEST_SSRC       0x11111111u
#define TEST_SSRC_ALT   0x22222222u

/* 30-byte master keys (16-byte AES key + 14-byte salt) for the two
 * directions. The SRTP transport requires transmit and receive keys to
 * be different, and the sender's TX key must match the receiver's RX key.
 */
static char test_key_ab[31] = "0123456789abcdef0123456789abcd";
static char test_key_ba[31] = "fedcba9876543210fedcba98765432";


/* Minimal in-memory transport, delivering packets to a peer transport. */
typedef struct loopback_tp
{
    pjmedia_transport  base;
    struct loopback_tp *peer;

    /* Callbacks attached via attach2(). */
    void               *user_data;
    void              (*rtp_cb)(void *user_data, void *pkt, pj_ssize_t size);
    void              (*rtp_cb2)(pjmedia_tp_cb_param *param);
    void              (*rtcp_cb)(void *user_data, void *pkt, pj_ssize_t size);

    /* Writable, 32bit aligned buffer for delivering one packet. */
    pj_uint32_t         rx_buf[128];
} loopback_tp;


static pj_status_t lb_get_info(pjmedia_transport *tp,
                               pjmedia_transport_info *info)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(info);
    return PJ_SUCCESS;
}

static pj_status_t lb_attach2(pjmedia_transport *tp,
                              pjmedia_transport_attach_param *att)
{
    loopback_tp *lb = (loopback_tp*)tp;

    PJ_ASSERT_RETURN(tp && att, PJ_EINVAL);

    lb->user_data = att->user_data;
    lb->rtp_cb = att->rtp_cb;
    lb->rtp_cb2 = att->rtp_cb2;
    lb->rtcp_cb = att->rtcp_cb;

    return PJ_SUCCESS;
}

static void lb_detach(pjmedia_transport *tp, void *strm)
{
    loopback_tp *lb = (loopback_tp*)tp;

    PJ_UNUSED_ARG(strm);

    lb->user_data = NULL;
    lb->rtp_cb = NULL;
    lb->rtp_cb2 = NULL;
    lb->rtcp_cb = NULL;
}

static pj_status_t lb_send_rtp(pjmedia_transport *tp,
                               const void *pkt,
                               pj_size_t size)
{
    loopback_tp *lb = (loopback_tp*)tp;
    loopback_tp *peer = lb->peer;
    pjmedia_tp_cb_param prm;

    if (!peer)
        return PJ_SUCCESS;

    if (size > sizeof(peer->rx_buf))
        return PJ_ETOOBIG;

    /* Deliver a writable, 32bit aligned copy: the receiving SRTP layer
     * decrypts the packet in place.
     */
    pj_memcpy(peer->rx_buf, pkt, size);

    pj_bzero(&prm, sizeof(prm));
    prm.user_data = peer->user_data;
    prm.pkt = peer->rx_buf;
    prm.size = (pj_ssize_t)size;

    if (peer->rtp_cb2) {
        (*peer->rtp_cb2)(&prm);
    } else if (peer->rtp_cb) {
        (*peer->rtp_cb)(peer->user_data, peer->rx_buf, (pj_ssize_t)size);
    }

    return PJ_SUCCESS;
}

static pj_status_t lb_send_rtcp(pjmedia_transport *tp,
                                const void *pkt,
                                pj_size_t size)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(size);
    return PJ_SUCCESS;
}

static pj_status_t lb_send_rtcp2(pjmedia_transport *tp,
                                 const pj_sockaddr_t *addr,
                                 unsigned addr_len,
                                 const void *pkt,
                                 pj_size_t size)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(addr);
    PJ_UNUSED_ARG(addr_len);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(size);
    return PJ_SUCCESS;
}

static pj_status_t lb_media_create(pjmedia_transport *tp,
                                   pj_pool_t *sdp_pool,
                                   unsigned options,
                                   const pjmedia_sdp_session *sdp_remote,
                                   unsigned media_index)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(sdp_pool);
    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(sdp_remote);
    PJ_UNUSED_ARG(media_index);
    return PJ_SUCCESS;
}

static pj_status_t lb_encode_sdp(pjmedia_transport *tp,
                                 pj_pool_t *sdp_pool,
                                 pjmedia_sdp_session *sdp_local,
                                 const pjmedia_sdp_session *sdp_remote,
                                 unsigned media_index)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(sdp_pool);
    PJ_UNUSED_ARG(sdp_local);
    PJ_UNUSED_ARG(sdp_remote);
    PJ_UNUSED_ARG(media_index);
    return PJ_SUCCESS;
}

static pj_status_t lb_media_start(pjmedia_transport *tp,
                                  pj_pool_t *pool,
                                  const pjmedia_sdp_session *sdp_local,
                                  const pjmedia_sdp_session *sdp_remote,
                                  unsigned media_index)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(sdp_local);
    PJ_UNUSED_ARG(sdp_remote);
    PJ_UNUSED_ARG(media_index);
    return PJ_SUCCESS;
}

static pj_status_t lb_media_stop(pjmedia_transport *tp)
{
    PJ_UNUSED_ARG(tp);
    return PJ_SUCCESS;
}

static pj_status_t lb_simulate_lost(pjmedia_transport *tp,
                                    pjmedia_dir dir,
                                    unsigned pct_lost)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(dir);
    PJ_UNUSED_ARG(pct_lost);
    return PJ_SUCCESS;
}

static pj_status_t lb_destroy(pjmedia_transport *tp)
{
    PJ_UNUSED_ARG(tp);
    return PJ_SUCCESS;
}


static pjmedia_transport_op loopback_op =
{
    &lb_get_info,
    NULL, /* &lb_attach, */
    &lb_detach,
    &lb_send_rtp,
    &lb_send_rtcp,
    &lb_send_rtcp2,
    &lb_media_create,
    &lb_encode_sdp,
    &lb_media_start,
    &lb_media_stop,
    &lb_simulate_lost,
    &lb_destroy,
    &lb_attach2
};


/* Test fixture: SRTP sender over loopback A, SRTP receiver over loopback B. */
typedef struct srtp_fixture
{
    loopback_tp        *a, *b;
    pjmedia_transport  *tx, *rx;

    /* Observed at the receiver's application callback. */
    unsigned            rx_count;
    pj_uint32_t         rx_last_ssrc;
    pj_uint16_t         rx_last_seq;
} srtp_fixture;


static void on_rx_rtp(void *user_data, void *pkt, pj_ssize_t size)
{
    srtp_fixture *f = (srtp_fixture*)user_data;
    const pj_uint8_t *buf = (const pj_uint8_t*)pkt;

    if (size < 12)
        return;

    f->rx_count++;
    f->rx_last_seq = (pj_uint16_t)((buf[2] << 8) | buf[3]);
    f->rx_last_ssrc = ((pj_uint32_t)buf[8] << 24) | ((pj_uint32_t)buf[9] << 16) |
                      ((pj_uint32_t)buf[10] << 8) | (pj_uint32_t)buf[11];
}

static void loopback_init(loopback_tp *lb, const char *name, loopback_tp *peer)
{
    pj_bzero(lb, sizeof(*lb));
    pj_ansi_strncpy(lb->base.name, name, sizeof(lb->base.name) - 1);
    lb->base.type = PJMEDIA_TRANSPORT_TYPE_USER;
    lb->base.op = &loopback_op;
    lb->peer = peer;
}

static void make_crypto(pjmedia_srtp_crypto *crypto, char *key)
{
    pj_bzero(crypto, sizeof(*crypto));
    crypto->name = pj_str("AES_CM_128_HMAC_SHA1_80");
    crypto->key = pj_str(key);
}

static pj_status_t fixture_init(srtp_fixture *f,
                                pj_pool_t *pool,
                                pjmedia_endpt *endpt,
                                const pjmedia_srtp_setting *tx_setting,
                                const pjmedia_srtp_setting *rx_setting)
{
    pjmedia_srtp_crypto crypto_ab, crypto_ba;
    pjmedia_transport_attach_param att;
    pj_status_t status;

    pj_bzero(f, sizeof(*f));

    f->a = PJ_POOL_ZALLOC_T(pool, loopback_tp);
    f->b = PJ_POOL_ZALLOC_T(pool, loopback_tp);
    loopback_init(f->a, "loopA", f->b);
    loopback_init(f->b, "loopB", f->a);

    status = pjmedia_transport_srtp_create(endpt, &f->a->base, tx_setting,
                                           &f->tx);
    if (status != PJ_SUCCESS)
        return status;

    status = pjmedia_transport_srtp_create(endpt, &f->b->base, rx_setting,
                                           &f->rx);
    if (status != PJ_SUCCESS)
        return status;

    /* Key A protects A->B, key B protects B->A. */
    make_crypto(&crypto_ab, test_key_ab);
    make_crypto(&crypto_ba, test_key_ba);

    status = pjmedia_transport_srtp_start(f->tx, &crypto_ab, &crypto_ba);
    if (status != PJ_SUCCESS)
        return status;

    status = pjmedia_transport_srtp_start(f->rx, &crypto_ba, &crypto_ab);
    if (status != PJ_SUCCESS)
        return status;

    pj_bzero(&att, sizeof(att));
    att.media_type = PJMEDIA_TYPE_AUDIO;
    att.user_data = f;
    att.rtp_cb = &on_rx_rtp;

    return pjmedia_transport_attach2(f->rx, &att);
}

static void fixture_destroy(srtp_fixture *f)
{
    /* Also closes the member (loopback) transports. */
    pjmedia_transport_close(f->tx);
    pjmedia_transport_close(f->rx);
}

/* Send one RTP packet (20 bytes: 12 byte header + 8 byte payload). */
static void fixture_send(srtp_fixture *f, pj_uint32_t ssrc, pj_uint16_t seq)
{
    pj_uint8_t buf[20];
    pj_uint32_t v32;

    pj_bzero(buf, sizeof(buf));
    buf[0] = 0x80;                  /* V=2 */
    buf[1] = 96;                    /* dynamic PT, outside RTCP range */
    buf[2] = (pj_uint8_t)(seq >> 8);
    buf[3] = (pj_uint8_t)(seq & 0xFF);
    v32 = pj_htonl(160u * seq);     /* timestamp */
    pj_memcpy(&buf[4], &v32, 4);
    v32 = pj_htonl(ssrc);
    pj_memcpy(&buf[8], &v32, 4);
    pj_memset(&buf[12], 0x55, 8);   /* payload */

    pjmedia_transport_send_rtp(f->tx, buf, sizeof(buf));
}


#define CHECK(cond, msg) \
    if (!(cond)) { PJ_LOG(3,(THIS_FILE, "FAILED: %s", msg)); rc = 1; } \
    else { PJ_LOG(4,(THIS_FILE, "ok: %s", msg)); }


/* The receiver is initialized with ROC 1 while the sender starts at
 * ROC 0 (e.g. the remote restarted its stream). The first packet cannot
 * authenticate, the retry must find ROC 0, and the winning ROC must be
 * written back to the setting with the retry window closed afterwards.
 */
static int scenario_retry_recovery(pj_pool_t *pool, pjmedia_endpt *endpt)
{
    srtp_fixture f;
    pjmedia_srtp_setting tx_setting, rx_setting, after;
    pj_status_t status;
    int rc = 0;

    pjmedia_srtp_setting_default(&tx_setting);
    pjmedia_srtp_setting_default(&rx_setting);

    rx_setting.rx_roc.ssrc = TEST_SSRC;
    rx_setting.rx_roc.roc = 1;
    rx_setting.prev_rx_roc.ssrc = TEST_SSRC;
    rx_setting.prev_rx_roc.roc = 0;

    status = fixture_init(&f, pool, endpt, &tx_setting, &rx_setting);
    CHECK(status == PJ_SUCCESS, "fixture init (ROC mismatch)");
    if (status != PJ_SUCCESS)
        return 1;

    fixture_send(&f, TEST_SSRC, 1000);  /* must recover via the retry */
    fixture_send(&f, TEST_SSRC, 1001);
    fixture_send(&f, TEST_SSRC, 1002);

    CHECK(f.rx_count == 3, "all packets delivered despite ROC mismatch");

    CHECK(pjmedia_transport_srtp_get_setting(f.rx, &after) == PJ_SUCCESS,
          "get_setting");
    CHECK(after.rx_roc.ssrc == TEST_SSRC && after.rx_roc.roc == 0,
          "winning ROC 0 written back to rx_roc");
    CHECK(after.prev_rx_roc.ssrc == 0,
          "retry window closed after the stream authenticated");

    fixture_destroy(&f);
    return rc;
}


/* Remote SSRC change after the probation period: the receiver has a
 * specific stream for SSRC 1, but must still accept SSRC 2 through the
 * wildcard template kept alongside in the RX policy list.
 */
static int scenario_new_ssrc(pj_pool_t *pool, pjmedia_endpt *endpt)
{
    srtp_fixture f;
    pjmedia_srtp_setting tx_setting, rx_setting;
    pj_status_t status;
    unsigned i;
    int rc = 0;

    pjmedia_srtp_setting_default(&tx_setting);
    pjmedia_srtp_setting_default(&rx_setting);

    /* Selects ssrc_specific(TEST_SSRC) + template, and matches the
     * sender's ROC, so no retry is involved here.
     */
    rx_setting.rx_roc.ssrc = TEST_SSRC;
    rx_setting.rx_roc.roc = 0;
    rx_setting.prev_rx_roc.ssrc = TEST_SSRC;
    rx_setting.prev_rx_roc.roc = 1;

    status = fixture_init(&f, pool, endpt, &tx_setting, &rx_setting);
    CHECK(status == PJ_SUCCESS, "fixture init (SSRC change)");
    if (status != PJ_SUCCESS)
        return 1;

    /* Exhaust the SRTP probation (100 packets) on the known SSRC, so the
     * new SSRC cannot be recovered by the probation restart path.
     */
    for (i = 0; i < 105; i++)
        fixture_send(&f, TEST_SSRC, (pj_uint16_t)(2000 + i));

    CHECK(f.rx_count == 105, "packets of the known SSRC delivered");

    fixture_send(&f, TEST_SSRC_ALT, 5000);

    CHECK(f.rx_count == 106, "packet with new SSRC delivered via template");
    CHECK(f.rx_last_ssrc == TEST_SSRC_ALT, "delivered packet has the new SSRC");

    fixture_destroy(&f);
    return rc;
}


/* Remote change shape: the receiver expects ROC 0 while the sender
 * continues at ROC 1. The retry must recover with the non-zero ROC and
 * write it back, so a later SRTP restart re-applies the winning ROC.
 */
static int scenario_winner_writeback(pj_pool_t *pool, pjmedia_endpt *endpt)
{
    srtp_fixture f;
    pjmedia_srtp_setting tx_setting, rx_setting, after;
    pj_status_t status;
    int rc = 0;

    pjmedia_srtp_setting_default(&tx_setting);
    pjmedia_srtp_setting_default(&rx_setting);

    /* Sender transmits with ROC 1. */
    tx_setting.tx_roc.ssrc = TEST_SSRC;
    tx_setting.tx_roc.roc = 1;

    /* Receiver expects ROC 0, with the sender's ROC 1 as alternative. */
    rx_setting.rx_roc.ssrc = TEST_SSRC;
    rx_setting.rx_roc.roc = 0;
    rx_setting.prev_rx_roc.ssrc = TEST_SSRC;
    rx_setting.prev_rx_roc.roc = 1;

    status = fixture_init(&f, pool, endpt, &tx_setting, &rx_setting);
    CHECK(status == PJ_SUCCESS, "fixture init (winner write-back)");
    if (status != PJ_SUCCESS)
        return 1;

    fixture_send(&f, TEST_SSRC, 1000);  /* must recover via the retry */
    fixture_send(&f, TEST_SSRC, 1001);

    CHECK(f.rx_count == 2, "packets delivered despite ROC mismatch");

    CHECK(pjmedia_transport_srtp_get_setting(f.rx, &after) == PJ_SUCCESS,
          "get_setting");
    CHECK(after.rx_roc.ssrc == TEST_SSRC && after.rx_roc.roc == 1,
          "winning ROC 1 written back to rx_roc");
    CHECK(after.prev_rx_roc.ssrc == 0,
          "retry window closed after the stream authenticated");

    fixture_destroy(&f);
    return rc;
}


int transport_srtp_test(void)
{
    pjmedia_endpt *endpt = NULL;
    pj_pool_t *pool;
    pj_status_t status;
    int rc = 0;

    status = pjmedia_endpt_create2(mem, NULL, 0, &endpt);
    if (status != PJ_SUCCESS) {
        app_perror(status, "Cannot create media endpoint");
        return 1;
    }

    pool = pj_pool_create(mem, THIS_FILE, 4000, 4000, NULL);
    if (!pool) {
        pjmedia_endpt_destroy2(endpt);
        return 1;
    }

    rc |= scenario_retry_recovery(pool, endpt);
    rc |= scenario_new_ssrc(pool, endpt);
    rc |= scenario_winner_writeback(pool, endpt);

    pj_pool_release(pool);
    pjmedia_endpt_destroy2(endpt);

    return rc;
}
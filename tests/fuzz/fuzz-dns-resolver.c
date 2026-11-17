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
#include <string.h>

#include <pjlib.h>
#include <pjlib-util.h>

/*
 * Asynchronous DNS resolver (pjlib-util/resolver.c) fuzz target.
 *
 * The existing fuzz-dns target only exercises the stateless packet parser;
 * the resolver object itself (query state machine, retransmission, cache,
 * and cancellation) was never driven by any harness. This target runs a
 * real pj_dns_resolver against a fake nameserver: a harness-owned UDP
 * socket on 127.0.0.1. Each iteration starts a query for a fixed name,
 * waits for the query packet, and answers it with fuzzer-controlled
 * response bytes (patched with the real transaction ID and echoed
 * question so the anti-spoofing checks pass and the parsing layer is
 * reached). Joined queries, cache reuse, cancellation and timeout paths
 * are exercised via the control byte. Callbacks run from
 * pj_dns_resolver_handle_events() on the harness thread. DNS responses are
 * attacker-controllable in the wild (malicious resolver, spoofing, forged
 * replies), which is exactly the threat this models.
 *
 * Input layout:
 *   data[0]        control byte:
 *                    bit0: query type (0 = A, 1 = SRV)
 *                    bit1: start a second identical query before the
 *                          first completes (joined-query path)
 *                    bit2: after the first completes, repeat the query
 *                          (cache-hit path)
 *                    bit3: start a query and cancel it (cancel path)
 *   data[1..]      response payload bytes, sent as the DNS reply with the
 *                  first two bytes overwritten by the real transaction ID.
 */

#define MEM_SIZE            (2*1024*1024)
#define QUERY_NAME          "fuzz.example.com"
#define ANSWER_MAX          PJ_DNS_RESOLVER_MAX_UDP_SIZE
/* A short retransmit delay keeps every query timeline inside the wait
 * budgets: with QRETR_COUNT = 2 the retransmit goes out at QRETR_DELAY ms
 * and a reply that is never accepted fails at about 2*QRETR_DELAY ms (the
 * sole nameserver goes BAD after the last retransmit), so both outcomes are
 * deterministic and dropped-reply inputs stay fast. */
#define QRETR_DELAY         10
#define QRETR_COUNT         2

static pj_caching_pool caching_pool;
static pj_dns_resolver *resolver;
static pj_sock_t ns_sock = PJ_INVALID_SOCKET;
static pj_uint16_t ns_port;

/* Incremented from on_dns_response on the harness thread. */
static volatile unsigned g_cb_count;

typedef struct {
    pj_bool_t done;
} fuzz_qdata;

static void on_dns_response(void *user_data,
                            pj_status_t status,
                            pj_dns_parsed_packet *response)
{
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(response);
    if (user_data)
        ((fuzz_qdata*)user_data)->done = PJ_TRUE;
    g_cb_count++;
}

/* Drop any unread datagrams left by a prior query/cancel. */
static void drain_ns_sock(void)
{
    uint8_t buf[ANSWER_MAX];
    unsigned i;

    for (i = 0; i < 32; ++i) {
        pj_fd_set_t rset;
        pj_time_val delay = {0, 0};
        pj_ssize_t rx = sizeof(buf);
        pj_sockaddr src;
        int srclen = sizeof(src);

        PJ_FD_ZERO(&rset);
        PJ_FD_SET(ns_sock, &rset);
        if (pj_sock_select((int)ns_sock + 1, &rset, NULL, NULL, &delay) <= 0)
            break;
        if (pj_sock_recvfrom(ns_sock, buf, &rx, 0, &src, &srclen) != PJ_SUCCESS)
            break;
    }
}

/* Clear cache, restore nameserver health, and empty the fake-NS socket so
 * each input starts from a clean resolver state. */
static void reset_resolver_state(void)
{
    pj_dns_resolver_clear_cache(resolver);
    pj_dns_resolver_reset_ns_state(resolver);
    drain_ns_sock();
}

static pj_bool_t query_wait_readable(void)
{
    pj_fd_set_t rset;
    pj_time_val delay = {0, 1};

    PJ_FD_ZERO(&rset);
    PJ_FD_SET(ns_sock, &rset);

    return pj_sock_select((int)ns_sock + 1, &rset, NULL, NULL, &delay) > 0;
}

/*
 * Receive query packets on the fake nameserver socket and answer each with
 * the fuzzer payload. The reply envelope is auto-corrected so that parsing
 * is always reached: transaction ID and the question section are mirrored
 * from the query (legitimate DNS servers echo them), qdcount forced to 1,
 * while header flags and the answer/authority/additional sections stay
 * fuzzer-controlled. Short payloads are zero-padded, so a fuzzed ancount
 * against missing answer bytes exercises the parser bounds checking.
 */
static void serve_queries(const uint8_t *payload, pj_size_t len,
                          unsigned max_rounds, unsigned base_cb)
{
    uint8_t rbuf[ANSWER_MAX];
    uint8_t qbuf[ANSWER_MAX];
    unsigned round;

    if (len > sizeof(rbuf))
        len = sizeof(rbuf);

    for (round = 0; round < max_rounds; ++round) {
        pj_ssize_t rx = sizeof(qbuf);
        pj_sockaddr src;
        int srclen = sizeof(src);
        pj_size_t qlen, rlen, qbody;
        pj_status_t status;
        unsigned wait;

        if (!query_wait_readable()) {
            if (g_cb_count > base_cb)
                return;
            continue;
        }

        status = pj_sock_recvfrom(ns_sock, qbuf, &rx, 0, &src, &srclen);
        if (status != PJ_SUCCESS || rx < 17) /* 12 hdr + name + 4 */
            continue;

        /* Walk the query's question labels to find its full length. */
        qlen = 12;
        while (qlen < (pj_size_t)rx && qbuf[qlen] != 0)
            qlen += 1 + qbuf[qlen];
        qlen += 1 + 4; /* root label + type + class */
        if (qlen > (pj_size_t)rx)
            continue;

        /* Build the reply: fuzz payload, then overlay the echoed parts. */
        pj_memset(rbuf, 0, sizeof(rbuf));
        if (len < 12) {
            /* Too short to be a header: still deliver, parser must cope. */
            rlen = len;
            pj_memcpy(rbuf, payload, len);
        } else {
            rlen = len;
            pj_memcpy(rbuf, payload, len);
            rbuf[0] = qbuf[0];
            rbuf[1] = qbuf[1];
            rbuf[4] = 0;
            rbuf[5] = 1; /* qdcount = 1 */
            /* qlen is the end offset of the question; copy only the body. */
            qbody = qlen - 12;
            if (rlen < qlen)
                rlen = qlen;
            if (rlen > sizeof(rbuf))
                rlen = sizeof(rbuf);
            if (12 + qbody <= sizeof(rbuf))
                pj_memcpy(rbuf + 12, qbuf + 12, qbody);
        }

        rx = (pj_ssize_t)rlen;
        pj_sock_sendto(ns_sock, rbuf, &rx, 0, &src, srclen);

        /* Pump the resolver so it reads the reply. */
        for (wait = 0; wait < 40 && g_cb_count <= base_cb; ++wait) {
            pj_time_val d = {0, 1};
            pj_dns_resolver_handle_events(resolver, &d);
        }
        if (g_cb_count > base_cb)
            return;
    }
}

static void wait_callbacks(unsigned target, unsigned max_ms)
{
    unsigned waited;

    /* The resolver must be pumped for its ioqueue/timer events (NULL was
     * passed for both at create). */
    for (waited = 0; waited < max_ms && g_cb_count < target; ++waited) {
        pj_time_val d = {0, 1};
        pj_dns_resolver_handle_events(resolver, &d);
    }
}

/* Unconditionally pump the resolver (timers keep running after cancel). */
static void pump_resolver(unsigned max_ms)
{
    unsigned waited;

    for (waited = 0; waited < max_ms; ++waited) {
        pj_time_val d = {0, 1};
        pj_dns_resolver_handle_events(resolver, &d);
    }
}

/* Cancel only if the callback has not fired yet (avoids stale recycled
 * handles once a completed query node is reused). */
static void cancel_if_pending(pj_dns_async_query *q, fuzz_qdata *qd)
{
    if (q && qd && !qd->done)
        pj_dns_resolver_cancel_query(q, PJ_TRUE);
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    pj_status_t status;
    pj_sockaddr bound;
    int addrlen = sizeof(bound);
    pj_str_t ns_addr = pj_str("127.0.0.1");
    pj_str_t ns_addrs[1];
    pj_uint16_t ns_ports[1];
    pj_dns_settings settings;

    PJ_UNUSED_ARG(argc);
    PJ_UNUSED_ARG(argv);

    status = pj_init();
    if (status != PJ_SUCCESS)
        abort();

    pj_log_set_level(0);
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy,
                         MEM_SIZE);

    /* Fake nameserver: a plain UDP socket bound to the loopback, so the
     * harness never answers datagrams from (or for) anyone else. */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &ns_sock);
    if (status != PJ_SUCCESS)
        abort();

    pj_sockaddr_in_init(&bound.ipv4, &ns_addr, 0);
    status = pj_sock_bind(ns_sock, &bound, sizeof(bound.ipv4));
    if (status != PJ_SUCCESS)
        abort();

    status = pj_sock_getsockname(ns_sock, &bound, &addrlen);
    if (status != PJ_SUCCESS)
        abort();

    ns_port = pj_sockaddr_get_port(&bound);

    /* Retransmit once at QRETR_DELAY ms: the nameserver stays PROBING until
     * the retransmit budget is used up, so on_timeout() sends a second
     * packet before the resolver considers failing the query. */
    pj_dns_settings_default(&settings);
    settings.qretr_delay = QRETR_DELAY;
    settings.qretr_count = QRETR_COUNT;

    status = pj_dns_resolver_create(&caching_pool.factory, "dns-res-fuzzer",
                                    0, NULL, NULL, &resolver);
    if (status != PJ_SUCCESS)
        abort();

    status = pj_dns_resolver_set_settings(resolver, &settings);
    if (status != PJ_SUCCESS)
        abort();

    ns_addrs[0] = ns_addr;
    ns_ports[0] = ns_port;
    status = pj_dns_resolver_set_ns(resolver, 1, ns_addrs, ns_ports);
    if (status != PJ_SUCCESS)
        abort();

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    uint8_t ctl;
    const uint8_t *payload;
    pj_size_t payload_len;
    int qtype;
    pj_str_t name = pj_str(QUERY_NAME);
    pj_dns_async_query *q1 = NULL, *q2 = NULL, *qx = NULL;
    fuzz_qdata qd1, qd2, qdx;
    unsigned expected_cb = 1;
    pj_status_t status;

    if (Size < 2)
        return 0;

    ctl = Data[0];
    payload = Data + 1;
    payload_len = Size - 1;
    if (payload_len > ANSWER_MAX)
        payload_len = ANSWER_MAX;
    qtype = (ctl & 0x01) ? PJ_DNS_TYPE_SRV : PJ_DNS_TYPE_A;
    g_cb_count = 0;
    pj_bzero(&qd1, sizeof(qd1));
    pj_bzero(&qd2, sizeof(qd2));
    pj_bzero(&qdx, sizeof(qdx));

    reset_resolver_state();

    status = pj_dns_resolver_start_query(resolver, &name, qtype, 0,
                                         &on_dns_response, &qd1, &q1);
    if (status == PJ_SUCCESS && q1 != NULL) {
        /* Start the joined query before serving so it becomes a child of
         * q1 rather than a post-completion cache hit or fresh query. */
        if (ctl & 0x02) {
            expected_cb = 2;
            pj_dns_resolver_start_query(resolver, &name, qtype, 0,
                                        &on_dns_response, &qd2, &q2);
        }

        serve_queries(payload, payload_len, 6, expected_cb - 1);

        wait_callbacks(expected_cb, QRETR_DELAY + 100);
        if (g_cb_count < expected_cb) {
            cancel_if_pending(q1, &qd1);
            cancel_if_pending(q2, &qd2);
            wait_callbacks(expected_cb, 100);
        }
    }
    /* Synchronous completion (cache hit) leaves q1 == NULL; nothing to do. */

    if (ctl & 0x04) {
        /* Cache-hit path: restore the nameserver without clearing the
         * cache (reset_ns_state keeps positive answers) so an accepted
         * first reply is reused synchronously; after a rejected reply the
         * NS is restored so a fresh query can still start. */
        pj_dns_resolver_reset_ns_state(resolver);
        drain_ns_sock();
        g_cb_count = 0;
        pj_bzero(&qdx, sizeof(qdx));
        status = pj_dns_resolver_start_query(resolver, &name, qtype, 0,
                                             &on_dns_response, &qdx, &qx);
        if (status == PJ_SUCCESS && qx != NULL) {
            serve_queries(payload, payload_len, 4, 0);
            wait_callbacks(1, QRETR_DELAY + 50);
            if (g_cb_count < 1) {
                cancel_if_pending(qx, &qdx);
                wait_callbacks(1, 100);
            }
        }
    }

    if (ctl & 0x08) {
        /* Cancel path: callback must fire with PJ_ECANCELLED. Reset first
         * so a carried-over cache or BAD nameserver state does not stop
         * the query from starting. */
        reset_resolver_state();
        g_cb_count = 0;
        pj_bzero(&qdx, sizeof(qdx));
        status = pj_dns_resolver_start_query(resolver, &name, qtype, 0,
                                             &on_dns_response, &qdx, &qx);
        if (status == PJ_SUCCESS && qx != NULL) {
            cancel_if_pending(qx, &qdx);
            /* cancel_query fires the callback synchronously but leaves the
             * query pending with its retransmit timer; pump until that whole
             * timeline has run out (last expiry at
             * (QRETR_COUNT+1)*QRETR_DELAY ms) so it does not fire in the
             * next input. */
            wait_callbacks(1, 100);
            pump_resolver(QRETR_DELAY * (QRETR_COUNT + 2));
        }
    }

    /* Leave no pending query packets or BAD/cache state for the next run. */
    reset_resolver_state();

    return 0;
}

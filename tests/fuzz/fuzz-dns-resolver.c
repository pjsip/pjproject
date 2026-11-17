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
 * nameserver failover state, SRV fallback, CNAME chasing, TCP requery) was
 * never driven by any harness. This target runs a real pj_dns_resolver
 * against a fake nameserver: a harness-owned UDP socket on 127.0.0.1. Each
 * iteration starts a query for a fixed name, waits for the query packet,
 * and answers it with fuzzer-controlled response bytes (patched with the
 * real transaction ID so the anti-spoofing source/matching checks pass and
 * the parsing layer is reached). Joined queries, cache reuse, cancellation
 * and timeout paths are exercised via the control byte. DNS responses are
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
#define ANSWER_MAX          2048

static pj_caching_pool caching_pool;
static pj_dns_resolver *resolver;
static pj_sock_t ns_sock = PJ_INVALID_SOCKET;
static pj_uint16_t ns_port;

/* Touched from the resolver's worker thread; single flags, benign races. */
static volatile unsigned g_cb_count;

static void on_dns_response(void *user_data,
                            pj_status_t status,
                            pj_dns_parsed_packet *response)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(response);
    g_cb_count++;
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
    uint8_t qbuf[512];
    unsigned round;

    if (len > sizeof(rbuf))
        len = sizeof(rbuf);

    for (round = 0; round < max_rounds; ++round) {
        pj_ssize_t rx = sizeof(qbuf);
        pj_sockaddr src;
        int srclen = sizeof(src);
        pj_size_t qlen, rlen;
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
            if (rlen < 12 + qlen)
                rlen = 12 + qlen;
            if (rlen > sizeof(rbuf))
                rlen = sizeof(rbuf);
            pj_memcpy(rbuf + 12, qbuf + 12, qlen);
        }

        rx = (pj_ssize_t)rlen;
        pj_sock_sendto(ns_sock, rbuf, &rx, 0, &src, srclen);

        /* Pump the resolver so it reads the reply; it has no own thread. */
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
        return 1;

    pj_log_set_level(0);
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy,
                         MEM_SIZE);

    /* Fake nameserver: a plain UDP socket on the loopback. */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &ns_sock);
    if (status != PJ_SUCCESS)
        return 1;

    pj_sockaddr_in_init(&bound.ipv4, NULL, 0);
    status = pj_sock_bind(ns_sock, &bound, sizeof(bound));
    if (status != PJ_SUCCESS)
        return 1;

    status = pj_sock_getsockname(ns_sock, &bound, &addrlen);
    if (status != PJ_SUCCESS)
        return 1;

    ns_port = pj_sockaddr_get_port(&bound);

    /* Resolver with fast retransmission so timeouts stay short. */
    pj_dns_settings_default(&settings);
    settings.qretr_delay = 100;
    settings.qretr_count = 1;

    status = pj_dns_resolver_create(&caching_pool.factory, "dns-res-fuzzer",
                                    0, NULL, NULL, &resolver);
    if (status != PJ_SUCCESS)
        return 1;

    status = pj_dns_resolver_set_settings(resolver, &settings);
    if (status != PJ_SUCCESS)
        return 1;

    ns_addrs[0] = ns_addr;
    ns_ports[0] = ns_port;
    status = pj_dns_resolver_set_ns(resolver, 1, ns_addrs, ns_ports);
    if (status != PJ_SUCCESS)
        return 1;

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
    unsigned expected_cb = 1;
    pj_status_t status;

    if (Size < 20)
        return 0;

    ctl = Data[0];
    payload = Data + 1;
    payload_len = Size - 1;
    qtype = (ctl & 0x01) ? PJ_DNS_TYPE_SRV : PJ_DNS_TYPE_A;
    g_cb_count = 0;

    status = pj_dns_resolver_start_query(resolver, &name, qtype, 0,
                                         &on_dns_response, NULL, &q1);
    if (status == PJ_SUCCESS && q1 != NULL) {
        /* Asynchronous: serve the query from the fake nameserver. */
        serve_queries(payload, payload_len, 6, 0);

        if (ctl & 0x02) {
            expected_cb = 2;
            pj_dns_resolver_start_query(resolver, &name, qtype, 0,
                                        &on_dns_response, NULL, &q2);
            serve_queries(payload, payload_len, 4, 1);
        }

        wait_callbacks(expected_cb, 300);
        if (g_cb_count < expected_cb) {
            if (q1)
                pj_dns_resolver_cancel_query(q1, PJ_TRUE);
            if (q2)
                pj_dns_resolver_cancel_query(q2, PJ_TRUE);
            wait_callbacks(expected_cb, 100);
        }
    }
    /* Synchronous completion (cache hit) leaves q1 == NULL; nothing to do. */

    if (ctl & 0x04) {
        /* Repeat: served from cache when the previous reply was cached. */
        g_cb_count = 0;
        status = pj_dns_resolver_start_query(resolver, &name, qtype, 0,
                                             &on_dns_response, NULL, &qx);
        if (status == PJ_SUCCESS && qx != NULL) {
            serve_queries(payload, payload_len, 4, 0);
            wait_callbacks(1, 200);
            if (g_cb_count < 1 && qx) {
                pj_dns_resolver_cancel_query(qx, PJ_TRUE);
                wait_callbacks(1, 100);
            }
        }
    }

    if (ctl & 0x08) {
        /* Cancel path: callback must fire with PJ_ECANCELLED. */
        g_cb_count = 0;
        status = pj_dns_resolver_start_query(resolver, &name, qtype, 0,
                                             &on_dns_response, NULL, &qx);
        if (status == PJ_SUCCESS && qx != NULL) {
            pj_dns_resolver_cancel_query(qx, PJ_TRUE);
            wait_callbacks(1, 100);
        }
    }

    return 0;
}

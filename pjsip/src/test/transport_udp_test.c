/* 
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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

#include "test.h"
#include <pjsip.h>
#include <pjlib.h>
#include <pj/compat/socket.h>

#define THIS_FILE   "transport_udp_test.c"

static pj_status_t multi_transport_test(pjsip_transport *tp[], unsigned max_tp)
{
#define ERR(rc__)   { rc=rc__; goto on_return; }
    int rc;
    unsigned i, num_tp=0;
    pj_str_t s;
    pjsip_transport *other_udp_tp = NULL, *udp_tp = NULL;
    pj_sockaddr_in rem_addr;    
    pjsip_tpselector tp_sel;

    /* Check if an UDP transport has been registered by other part of
     * pjsip-test */
    pj_sockaddr_in_init(&rem_addr, pj_cstr(&s, "1.1.1.1"), 80);
    pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_UDP,
                                      &rem_addr, sizeof(rem_addr),
                                      NULL, &other_udp_tp);

    for (i=num_tp; i<max_tp; ++i) {
        pj_sockaddr_in addr;

        pj_sockaddr_in_init(&addr, NULL, (pj_uint16_t)(TEST_UDP_PORT+i));

        /* Start UDP transport. */
        PJ_TEST_SUCCESS(pjsip_udp_transport_start( endpt, &addr, NULL, 1, &udp_tp),
                        NULL, ERR(-110));

        /* UDP transport must have initial reference counter set to 1. */
        PJ_TEST_EQ(pj_atomic_get(udp_tp->ref_cnt), 1, NULL, ERR(-120));

        tp[num_tp++] = udp_tp;
    }

    for (i = 0; i < num_tp; ++i) {
        PJ_TEST_EQ(pj_atomic_get(tp[i]->ref_cnt), 1, NULL, ERR(-130));

        /* Test basic transport attributes */
        rc = generic_transport_test(udp_tp);
        if (rc != 0)
            goto on_return;
    }

    /* Acquire transport test without selector. */
    pj_sockaddr_in_init(&rem_addr, pj_cstr(&s, "1.1.1.1"), 80);
    PJ_TEST_SUCCESS(pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_UDP,
                                           &rem_addr, sizeof(rem_addr),
                                           NULL, &udp_tp),
                    NULL, ERR(-140));

    for (i = 0; i < num_tp; ++i) {
        if (udp_tp == tp[i]) {
            break;
        }
    }
    PJ_TEST_TRUE(i<num_tp || udp_tp==other_udp_tp, NULL, ERR(-150));

    pjsip_transport_dec_ref(udp_tp);

    if (udp_tp == other_udp_tp) {
        PJ_TEST_GT(pj_atomic_get(udp_tp->ref_cnt), 1, NULL, ERR(-160));
    } else {
        PJ_TEST_EQ(pj_atomic_get(udp_tp->ref_cnt), 1, NULL, ERR(-165));
    }

    /* Acquire transport test with selector. */
    pj_bzero(&tp_sel, sizeof(tp_sel));
    tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
    tp_sel.u.transport = tp[num_tp-1];
    pj_sockaddr_in_init(&rem_addr, pj_cstr(&s, "1.1.1.1"), 80);
    PJ_TEST_SUCCESS( pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_UDP,
                                           &rem_addr, sizeof(rem_addr),
                                           &tp_sel, &udp_tp),
                     NULL, ERR(-170));

    PJ_TEST_EQ(udp_tp, tp[num_tp-1], NULL, ERR(-180));

    pjsip_transport_dec_ref(udp_tp);

    PJ_TEST_EQ(pj_atomic_get(udp_tp->ref_cnt), 1, NULL, ERR(-190));

    rc = 0;

on_return:
    if (other_udp_tp) {
        pjsip_transport_dec_ref(other_udp_tp);
    }
    if (rc != 0) {
        for (i=0; i<num_tp; ++i) {
            pjsip_transport_dec_ref(tp[i]);
        }
    }

    return rc;
#undef ERR
}

/*
 * UDP transport test.
 */
int transport_udp_test(void)
{
    enum { SEND_RECV_LOOP = 8 };
    enum { NUM_TP = 4 };
    pjsip_transport *tp[NUM_TP], *udp_tp;
    pj_sockaddr_in rem_addr;
    pj_str_t s;
    pj_status_t status;
    int rtt[SEND_RECV_LOOP], min_rtt;
    int i, pkt_lost;

    status = multi_transport_test(&tp[0], NUM_TP);
    if (status != PJ_SUCCESS)
        return status;

    /* Basic transport's send/receive loopback test. */
    pj_sockaddr_in_init(&rem_addr, pj_cstr(&s, "127.0.0.1"), TEST_UDP_PORT);
    for (i=0; i<SEND_RECV_LOOP; ++i) {
        status = transport_send_recv_test(PJSIP_TRANSPORT_UDP, tp[0], 
                                          "127.0.0.1:"TEST_UDP_PORT_STR,
                                          &rtt[i]);
        if (status != 0)
            return status;
    }

    min_rtt = 0xFFFFFFF;
    for (i=0; i<SEND_RECV_LOOP; ++i)
        if (rtt[i] < min_rtt) min_rtt = rtt[i];

    report_ival("udp-rtt-usec", min_rtt, "usec",
                "Best UDP transport round trip time, in microseconds "
                "(time from sending request until response is received. "
                "Tests were performed on local machine only)");


    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_UDP, tp[0], 
                               "127.0.0.1:"TEST_UDP_PORT_STR, 
                               &pkt_lost);
    if (status != 0)
        return status;

    if (pkt_lost != 0)
        PJ_LOG(3,(THIS_FILE, "   note: %d packet(s) was lost", pkt_lost));

    for (i = 0; i < NUM_TP; ++i) {
        udp_tp = tp[i];

        /* Check again that reference counter is 1. */
        if (pj_atomic_get(udp_tp->ref_cnt) != 1)
            return -80;

        /* Destroy this transport. */
        pjsip_transport_dec_ref(udp_tp);
        status = pjsip_transport_destroy(udp_tp);
        if (status != PJ_SUCCESS)
            return -90;
    }

    /* Flush events. */
    PJ_LOG(3,(THIS_FILE, "   Flushing events, 1 second..."));
    flush_events(1000);

    /* Done */
    return 0;
}

#if PJ_HAS_THREADS

#define RESTART_TEST_PORT   (TEST_UDP_PORT + 20)

#if defined(PJ_WIN32) && PJ_WIN32!=0
#   define RESTART_ADDR_IN_USE  PJ_STATUS_FROM_OS(WSAEADDRINUSE)
#else
#   define RESTART_ADDR_IN_USE  PJ_STATUS_FROM_OS(EADDRINUSE)
#endif

/* Each restart takes a new ioqueue key, freed only after
 * PJ_IOQUEUE_KEY_FREE_DELAY: more loops can run out of keys.
 */
enum { RESTART_THREADS = 2, RESTART_LOOP = 12, RESTART_RETRY = 50 };

static struct restart_test
{
    pjsip_transport *tp;
    pj_sockaddr      bind_addr;
    pj_sock_t        flood_sock;
    pj_atomic_t     *quit;
    pj_status_t      status[RESTART_THREADS];
} rt;

static int restart_poll_thread(void *arg)
{
    PJ_UNUSED_ARG(arg);
    while (!pj_atomic_get(rt.quit)) {
        pj_time_val timeout = {0, 1};
        pjsip_endpt_handle_events(endpt, &timeout);
    }
    return 0;
}

static int restart_flood_thread(void *arg)
{
    pj_sockaddr_in dst;
    pj_str_t s;

    PJ_UNUSED_ARG(arg);
    (void)pj_sockaddr_in_init(&dst, pj_cstr(&s, "127.0.0.1"),
                              RESTART_TEST_PORT);
    while (!pj_atomic_get(rt.quit)) {
        pj_ssize_t len = 4;
        (void)pj_sock_sendto(rt.flood_sock, "\r\n\r\n", &len, 0, &dst,
                             sizeof(dst));
        pj_thread_sleep(1);
    }
    return 0;
}

/* Restart the transport, retrying while the port is still in use: on some
 * platforms the old socket keeps the port until the thread polling the
 * ioqueue returns from select().
 */
static pj_status_t restart_tp(void)
{
    pj_status_t status;
    int i = 0;

    for (;;) {
        status = pjsip_udp_transport_restart2(rt.tp,
                                        PJSIP_UDP_TRANSPORT_DESTROY_SOCKET,
                                        PJ_INVALID_SOCKET, &rt.bind_addr,
                                        NULL);
        if (status != RESTART_ADDR_IN_USE || ++i == RESTART_RETRY)
            break;
        pj_thread_sleep(2);
    }
    return status;
}

static int restart_thread(void *arg)
{
    unsigned idx = (unsigned)(pj_ssize_t)arg;
    int i;

    for (i = 0; i < RESTART_LOOP; ++i) {
        pj_status_t status = restart_tp();

        if (status != PJ_SUCCESS && rt.status[idx] == PJ_SUCCESS)
            rt.status[idx] = status;
        pj_thread_sleep(1);
    }
    return 0;
}

/*
 * Restarting a UDP transport from several threads at once while it
 * receives packets.
 */
int transport_udp_restart_test(void)
{
    pj_pool_t *pool;
    pj_thread_t *poll_th = NULL, *flood_th = NULL;
    pj_thread_t *th[RESTART_THREADS];
    char host_port[32];
    unsigned i, th_cnt = 0;
    int rtt, rc = 0;
    pj_status_t status;

    pj_bzero(&rt, sizeof(rt));
    rt.flood_sock = PJ_INVALID_SOCKET;
    pool = pjsip_endpt_create_pool(endpt, "udprestart", 512, 512);
    PJ_TEST_NOT_NULL(pool, NULL, return -10);

    PJ_TEST_SUCCESS(pj_atomic_create(pool, 0, &rt.quit), NULL,
                    {rc = -15; goto on_return;});
    PJ_TEST_SUCCESS(pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0,
                                   &rt.flood_sock),
                    NULL, {rc = -17; goto on_return;});

    (void)pj_sockaddr_init(pj_AF_INET(), &rt.bind_addr, NULL,
                           RESTART_TEST_PORT);
    PJ_TEST_SUCCESS(pjsip_udp_transport_start(endpt, &rt.bind_addr.ipv4, NULL,
                                              1, &rt.tp),
                    NULL, {rc = -20; goto on_return;});

    PJ_TEST_SUCCESS(pj_thread_create(pool, "udprpoll", &restart_poll_thread,
                                     NULL, 0, 0, &poll_th),
                    NULL, {rc = -30; goto on_return;});
    PJ_TEST_SUCCESS(pj_thread_create(pool, "udprflood", &restart_flood_thread,
                                     NULL, 0, 0, &flood_th),
                    NULL, {rc = -40; goto on_return;});

    for (i = 0; i < RESTART_THREADS; ++i) {
        PJ_TEST_SUCCESS(pj_thread_create(pool, "udprestart", &restart_thread,
                                         (void*)(pj_ssize_t)i, 0, 0,
                                         &th[th_cnt]),
                        NULL, {rc = -50; goto on_return;});
        ++th_cnt;
    }

on_return:
    for (i = 0; i < th_cnt; ++i) {
        pj_thread_join(th[i]);
        pj_thread_destroy(th[i]);
        if (rc == 0)
            PJ_TEST_SUCCESS(rt.status[i], NULL, rc = -55);
    }

    /* A restart must still succeed after the concurrent ones */
    if (rc == 0 && rt.tp) {
        PJ_TEST_SUCCESS(restart_tp(), NULL, rc = -60);
    }

    if (rt.quit)
        pj_atomic_set(rt.quit, 1);
    if (flood_th) {
        pj_thread_join(flood_th);
        pj_thread_destroy(flood_th);
    }
    if (poll_th) {
        pj_thread_join(poll_th);
        pj_thread_destroy(poll_th);
    }
    if (rt.flood_sock != PJ_INVALID_SOCKET)
        pj_sock_close(rt.flood_sock);

    /* And the transport must still receive */
    if (rc == 0) {
        pj_ansi_snprintf(host_port, sizeof(host_port), "127.0.0.1:%d",
                         RESTART_TEST_PORT);
        PJ_TEST_EQ(transport_send_recv_test(PJSIP_TRANSPORT_UDP, rt.tp,
                                            host_port, &rtt), 0,
                   NULL, rc = -65);
    }

    if (rt.tp) {
        pjsip_transport_dec_ref(rt.tp);
        status = pjsip_transport_destroy(rt.tp);
        if (rc == 0 && status != PJ_SUCCESS)
            rc = -70;
    }
    flush_events(500);
    if (rt.quit)
        pj_atomic_destroy(rt.quit);
    pj_pool_release(pool);
    return rc;
}

#endif  /* PJ_HAS_THREADS */

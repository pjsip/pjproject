/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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

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

#define THIS_FILE   "transport_loop_test.c"

static void send_cb(pjsip_send_state *st, pj_ssize_t sent, pj_bool_t *cont)
{
    int *loop_resolve_status = (int*)st->token;
    if (sent < 0) {
        /* Success! */
        *loop_resolve_status = (int)-sent;
    } else {
        PJ_LOG(3,(THIS_FILE,
                  "   error: expecting error in send_cb() but got sent=%ld",
                 sent));
        *loop_resolve_status = 0;
    }
}

static int loop_resolve_error()
{
    pjsip_transport *loop = NULL;
    pj_sockaddr_in addr;
    pj_str_t url;
    pjsip_tx_data *tdata;
    pjsip_tpselector tp_sel;
    int i, loop_resolve_status;
    pj_status_t status;

    loop_resolve_status = 0;
    pj_bzero(&addr, sizeof(addr));

    status = pjsip_endpt_acquire_transport( endpt, PJSIP_TRANSPORT_LOOP_DGRAM,
                                            &addr, sizeof(addr), NULL, &loop);
    if (status != PJ_SUCCESS) {
        app_perror("   error: loop transport is not configured", status);
        return -210;
    }

    url = pj_str("sip:bob@unresolved-host");

    for (i=0; i<2; ++i) {
        /* variant a: without tp_selector */
        status = pjsip_endpt_create_request( endpt, &pjsip_options_method,
                                            &url, &url, &url, NULL, NULL, -1, 
                                            NULL, &tdata);
        if (status != PJ_SUCCESS) {
            app_perror("   Error: unable to create request", status);
            loop_resolve_status = -220;
            goto on_return;
        }

        if (i==1) {
            /* variant b: with tp_selector */
            pj_bzero(&tp_sel, sizeof(tp_sel));
            tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
            tp_sel.u.transport = loop;

            pjsip_tx_data_set_transport(tdata, &tp_sel);
        }
        loop_resolve_status = PJ_EPENDING;
        status = pjsip_endpt_send_request_stateless(endpt, tdata, 
                                                    &loop_resolve_status,
                                                    &send_cb);
        if (status!=PJ_EPENDING && status!=PJ_SUCCESS) {
            /* Success! (we're expecting error and got immediate error)*/
            loop_resolve_status = 0;
        } else {
            flush_events(500);
            if (loop_resolve_status!=PJ_EPENDING && loop_resolve_status!=PJ_SUCCESS) {
                /* Success! (we're expecting error in callback)*/
                //PJ_PERROR(3, (THIS_FILE, loop_resolve_status, 
                //              "   correctly got error"));
                loop_resolve_status = 0;
            } else {
                PJ_LOG(3,(THIS_FILE, "   error: expecting error but status=%d", status));
                loop_resolve_status = -240;
                goto on_return;
            }
        }
    }

on_return:
    if (loop)
        pjsip_transport_dec_ref(loop);
    return loop_resolve_status;

}

static int datagram_loop_test()
{
    enum { LOOP = 8 };
    pjsip_transport *loop;
    int i, pkt_lost;
    pj_sockaddr_in addr;
    pj_status_t status;
    long ref_cnt;
    int rtt[LOOP], min_rtt;

    PJ_LOG(3,(THIS_FILE, "testing datagram loop transport"));

    pj_sockaddr_in_init(&addr, NULL, 0);
    /* Test acquire transport. */
    status = pjsip_endpt_acquire_transport( endpt, PJSIP_TRANSPORT_LOOP_DGRAM,
                                            &addr, sizeof(addr), NULL, &loop);
    if (status != PJ_SUCCESS) {
        app_perror("   error: loop transport is not configured", status);
        return -20;
    }

    /* Get initial reference counter */
    ref_cnt = pj_atomic_get(loop->ref_cnt);

    /* Test basic transport attributes */
    status = generic_transport_test(loop);
    if (status != PJ_SUCCESS)
        return status;

    /* Basic transport's send/receive loopback test. */
    for (i=0; i<LOOP; ++i) {
        status = transport_send_recv_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
                                          "sip:bob@130.0.0.1;transport=loop-dgram",
                                          &rtt[i]);
        if (status != 0)
            return status;
    }

    /* Resolve error */
    status = loop_resolve_error();
    if (status)
        return status;

    min_rtt = 0xFFFFFFF;
    for (i=0; i<LOOP; ++i)
        if (rtt[i] < min_rtt) min_rtt = rtt[i];

    report_ival("loop-rtt-usec", min_rtt, "usec",
                "Best Loopback transport round trip time, in microseconds "
                "(time from sending request until response is received. "
                "Tests were performed on local machine only)");


    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
                               "sip:bob@130.0.0.1;transport=loop-dgram",
                               &pkt_lost);
    if (status != 0)
        return status;

    if (pkt_lost != 0) {
        PJ_LOG(3,(THIS_FILE, "   error: %d packet(s) was lost", pkt_lost));
        return -40;
    }

    /* Put delay. */
    PJ_LOG(3,(THIS_FILE,"  setting network delay to 10 ms"));
    pjsip_loop_set_delay(loop, 10);

    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
                               "sip:bob@130.0.0.1;transport=loop-dgram",
                               &pkt_lost);
    if (status != 0)
        return status;

    if (pkt_lost != 0) {
        PJ_LOG(3,(THIS_FILE, "   error: %d packet(s) was lost", pkt_lost));
        return -50;
    }

    /* Restore delay. */
    pjsip_loop_set_delay(loop, 0);

    /* Check reference counter. */
    if (pj_atomic_get(loop->ref_cnt) != ref_cnt) {
        PJ_LOG(3,(THIS_FILE, "   error: ref counter is not %ld (%ld)", 
                             ref_cnt, pj_atomic_get(loop->ref_cnt)));
        return -51;
    }

    /* Decrement reference. */
    pjsip_transport_dec_ref(loop);

    return 0;
}

int transport_loop_test(void)
{
    int status;

    status = datagram_loop_test();
    if (status != 0)
        return status;

    return 0;
}

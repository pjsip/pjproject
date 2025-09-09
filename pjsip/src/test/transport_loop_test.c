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

static pjsip_transport *cur_loop;
static int loop_test_status;

static pj_bool_t on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t on_rx_response(pjsip_rx_data *rdata);
static pj_status_t on_tx_message(pjsip_tx_data *tdata);

static pjsip_module loop_tester_mod =
{
    NULL, NULL,                         /* prev and next        */
    { "transport_loop_test", 19},       /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_UA_PROXY_LAYER-1,/* Priority             */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    &on_rx_request,                     /* on_rx_request()      */
    &on_rx_response,                    /* on_rx_response()     */
    &on_tx_message,                     /* on_tx_request()      */
    &on_tx_message,                     /* on_tx_response()     */
    NULL,                               /* on_tsx_state()       */
};


static pj_bool_t on_rx_request(pjsip_rx_data *rdata)
{
#define ERR(rc__)   {loop_test_status=rc__; return PJ_TRUE; }
    if (!is_user_equal(rdata->msg_info.from, "transport_loop_multi_test"))
        return PJ_FALSE;
    
    PJ_TEST_EQ(rdata->tp_info.transport, cur_loop, NULL, ERR(-100));
    PJ_TEST_SUCCESS(pjsip_endpt_respond_stateless(endpt, rdata, PJSIP_SC_ACCEPTED,
                                                  NULL, NULL, NULL),
                    NULL, ERR(-100));
    return PJ_TRUE;
#undef ERR
}

static pj_bool_t on_rx_response(pjsip_rx_data *rdata)
{
#define ERR(rc__)   {loop_test_status=rc__; return PJ_TRUE; }
    if (!is_user_equal(rdata->msg_info.from, "transport_loop_multi_test"))
        return PJ_FALSE;
    
    PJ_TEST_EQ(rdata->tp_info.transport, cur_loop, NULL, ERR(-150));
    PJ_TEST_EQ(rdata->msg_info.msg->line.status.code, PJSIP_SC_ACCEPTED, NULL,
               ERR(-160));

    loop_test_status = 0;
    return PJ_TRUE;
#undef ERR
}

static pj_status_t on_tx_message(pjsip_tx_data *tdata)
{
    pjsip_from_hdr *from_hdr = (pjsip_from_hdr*)
                               pjsip_msg_find_hdr(tdata->msg, PJSIP_H_FROM, NULL);
    
    if (!is_user_equal(from_hdr, "transport_loop_multi_test"))
        return PJ_SUCCESS;

    PJ_TEST_EQ(tdata->tp_info.transport, cur_loop,
               tdata->msg->type==PJSIP_REQUEST_MSG? 
                "request transport mismatch" : "response transport mismatch",
               loop_test_status=-200);

    return PJ_SUCCESS;
}

/* Test that request and responses are sent/received on the correct loop
 * instance when multiple instances of transport loop are created
 */
int transport_loop_multi_test(void)
{
#define ERR(rc__)   { rc=rc__; goto on_return; }
    enum { N = 4, START_PORT=5000, TIMEOUT=1000 };
    pjsip_transport *loops[N];
    int i, rc;

    pj_bzero(loops, sizeof(loops));

    PJ_TEST_SUCCESS(pjsip_endpt_register_module(endpt, &loop_tester_mod),
                    NULL, ERR(-5));

    for (i=0; i<N; ++i) {
        PJ_TEST_SUCCESS(pjsip_loop_start(endpt, &loops[i]), NULL, ERR(-10));
        pjsip_transport_add_ref(loops[i]);
    }

    for (i=0; i<N; ++i) {
        pj_str_t url;
        pjsip_tpselector tp_sel;
        pjsip_tx_data *tdata;
        pj_time_val timeout, now;

        loop_test_status = PJ_EPENDING;
        cur_loop = loops[i];

        pj_bzero(&tp_sel, sizeof(tp_sel));
        tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
        tp_sel.u.transport = loops[i];

        url = pj_str("sip:transport_loop_multi_test@127.0.0.1");

        PJ_TEST_SUCCESS(pjsip_endpt_create_request(endpt, &pjsip_options_method,
                                                   &url, &url, &url,
                                                   NULL, NULL, -1, NULL,
                                                   &tdata),
                        NULL, ERR(-20));
        PJ_TEST_SUCCESS(pjsip_tx_data_set_transport(tdata, &tp_sel),
                        NULL, ERR(-30));
        PJ_TEST_SUCCESS(pjsip_endpt_send_request_stateless(endpt, tdata, NULL, NULL),
                        NULL, ERR(-40));

        pj_gettimeofday(&timeout);
        now = timeout;
        timeout.msec += TIMEOUT;
        pj_time_val_normalize(&timeout);

        while (loop_test_status==PJ_EPENDING && PJ_TIME_VAL_LT(now, timeout)) {
            flush_events(100);
            pj_gettimeofday(&now);
        }

        PJ_TEST_NEQ(loop_test_status, PJ_EPENDING, "test has timed-out",
                    ERR(-50));
        if (loop_test_status != 0) {
            rc = -60;
            goto on_return;
        }
    }

    rc = 0;
    
on_return:
    for (i=0; i<N; ++i) {
        if (loops[i]) {
            /* Order must be shutdown then dec_ref so it gets destroyed */
            pjsip_transport_shutdown(loops[i]);
            pjsip_transport_dec_ref(loops[i]);
        }
    }
    if (loop_tester_mod.id != -1) {
        pjsip_endpt_unregister_module(endpt, &loop_tester_mod);
    }
    /* let transport destroy run its course */
    flush_events(500);
    return rc;
#undef ERR
}

static void send_cb(pjsip_send_state *st, pj_ssize_t sent, pj_bool_t *cont)
{
    int *loop_resolve_status = (int*)st->token;

    PJ_UNUSED_ARG(cont);

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

static int loop_resolve_error(pj_bool_t disable_no_selector)
{
    pjsip_transport *loop = NULL;
    pj_sockaddr_in addr;
    pj_str_t url;
    pjsip_tx_data *tdata;
    pjsip_tpselector tp_sel;
    int i, loop_resolve_status;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "testing loop resolve error"));

    loop_resolve_status = 0;
    pj_bzero(&addr, sizeof(addr));

    status = pjsip_endpt_acquire_transport( endpt, PJSIP_TRANSPORT_LOOP_DGRAM,
                                            &addr, sizeof(addr), NULL, &loop);
    if (status != PJ_SUCCESS) {
        app_perror("   error: loop transport is not configured", status);
        return -210;
    }

    url = pj_str("sip:bob@unresolved-host;transport=loop-dgram");

    for (i=0; i<2; ++i) {
        if (i==0 && disable_no_selector)
            continue;

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
            unsigned j;

            for (j=0; j<40 && loop_resolve_status==PJ_EPENDING; ++j)
                flush_events(500);

            if (loop_resolve_status!=PJ_EPENDING && loop_resolve_status!=PJ_SUCCESS) {
                /* Success! (we're expecting error in callback)*/
                //PJ_PERROR(3, (THIS_FILE, loop_resolve_status, 
                //              "   correctly got error"));
                loop_resolve_status = 0;
            } else {
                PJ_LOG(3,(THIS_FILE, 
                          "   error (variant %d): expecting error but loop_resolve_status=%d",
                          i, loop_resolve_status));
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

int transport_loop_test()
{
    enum { LOOP = 8 };
    pjsip_transport *loop = NULL;
    char host_port_transport[64];
    int i, pkt_lost;
    int status;
    long ref_cnt;
    int rtt[LOOP], min_rtt;


    PJ_TEST_SUCCESS(pjsip_loop_start(endpt, &loop), NULL, return -3);
    pjsip_transport_add_ref(loop);

    pj_ansi_snprintf(host_port_transport, sizeof(host_port_transport),
                     "%.*s:%d;transport=loop-dgram",
                     (int)loop->local_name.host.slen,
                     loop->local_name.host.ptr,
                     loop->local_name.port);

    /* Get initial reference counter */
    ref_cnt = pj_atomic_get(loop->ref_cnt);

    /* Test basic transport attributes */
    status = generic_transport_test(loop);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Basic transport's send/receive loopback test. */
    for (i=0; i<LOOP; ++i) {
        status = transport_send_recv_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
                                          host_port_transport, &rtt[i]);
        if (status != 0)
            goto on_return;
    }

    min_rtt = 0xFFFFFFF;
    for (i=0; i<LOOP; ++i)
        if (rtt[i] < min_rtt) min_rtt = rtt[i];

    report_ival("loop-rtt-usec", min_rtt, "usec",
                "Best Loopback transport round trip time, in microseconds "
                "(time from sending request until response is received. "
                "Tests were performed on local machine only)");


    /* Multi-threaded round-trip test. */
    pjsip_loop_set_discard(loop, 0, NULL);
    pjsip_loop_set_failure(loop, 0, NULL);
    pjsip_loop_set_delay(loop, 0);
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
                               "130.0.0.1;transport=loop-dgram", &pkt_lost);
    if (status != 0)
        goto on_return;

    PJ_TEST_EQ(pkt_lost, 0, NULL, { status = -40; goto on_return;});

    /* Put delay. */
    PJ_LOG(3,(THIS_FILE,"  setting network delay to 10 ms"));
    pjsip_loop_set_delay(loop, 10);

    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
                               "130.0.0.1;transport=loop-dgram",
                               &pkt_lost);
    if (status != 0)
        goto on_return;

    PJ_TEST_EQ(pkt_lost, 0, NULL, { status = -50; goto on_return;});

    /* Restore delay. */
    pjsip_loop_set_delay(loop, 0);

    /* Check reference counter. */
    if (pj_atomic_get(loop->ref_cnt) != ref_cnt) {
        PJ_LOG(3,(THIS_FILE, "   error: ref counter is not %ld (%ld)", 
                             ref_cnt, pj_atomic_get(loop->ref_cnt)));
        status = -51;
        goto on_return;
    }

    status = 0;

on_return:
    /* Decrement reference. */
    pjsip_transport_dec_ref(loop);
    return status;
}


/* tgus needs to be exclusive, because there must NOT be any other
 * loop transport otherwise some test will fail (e.g. sending will
 * fallback to that transport)
 */
int transport_loop_resolve_error_test()
{
    pjsip_transport *loop;
    pj_bool_t disable_no_selector = PJ_FALSE;
    int status;

    PJ_LOG(3,(THIS_FILE, "  Loop transport count: %d",
                pjsip_tpmgr_get_transport_count_by_type(
                pjsip_endpt_get_tpmgr(endpt),
                PJSIP_TRANSPORT_LOOP_DGRAM)));
                
    /* if there is another transport, wait sometime until it's gone */
    loop = wait_loop_transport_clear(10);

    if (loop) {
        /* We are not supposed to have another instance of loop transport, but
         * we found one. If there are more than one, test will fail. Otherwise
         * test should succeed
         */
        PJ_LOG(2,(THIS_FILE,
                  "Warning: found %d loop transport instances",
                  pjsip_tpmgr_get_transport_count_by_type(
                    pjsip_endpt_get_tpmgr(endpt),
                    PJSIP_TRANSPORT_LOOP_DGRAM)));
        disable_no_selector = PJ_TRUE;
        pjsip_loop_set_discard(loop, 0, NULL);
        pjsip_loop_set_failure(loop, 0, NULL);
        pjsip_loop_set_delay(loop, 0);
    } else {
        PJ_TEST_EQ(loop, NULL, NULL, return -2);
        PJ_TEST_SUCCESS(pjsip_loop_start(endpt, &loop), NULL, return -3);
        pjsip_transport_add_ref(loop);
    }

    /* Resolve error */
    status = loop_resolve_error(disable_no_selector);

    /* Decrement reference. */
    pjsip_transport_dec_ref(loop);
    return status;
}

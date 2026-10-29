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

#define THIS_FILE   "tsx_basic_test.c"

struct send_request2_data
{
    pj_bool_t   cb_called;
    int         status_code;
};

static struct tsx_basic_test_global_t
{
    char TARGET_URI[PJSIP_MAX_URL_SIZE];
    char FROM_URI[PJSIP_MAX_URL_SIZE];
    pjsip_transport *tp;

    /* Must outlive the test function, as the callback may still be called
     * after the test returns, e.g. when the test fails to terminate the
     * transaction.
     */
    struct send_request2_data sr;
} g[MAX_TSX_TESTS];


/* Test transaction layer. */
static int tsx_layer_test(unsigned tid)
{
    pj_str_t target, from, tsx_key;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx, *found;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  transaction layer test"));

    target = pj_str(g[tid].TARGET_URI);
    from = pj_str(g[tid].FROM_URI);

    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, &target,
                                        &from, &target, NULL, NULL, -1, NULL,
                                        &tdata);
    if (status != PJ_SUCCESS) {
        app_perror("  error: unable to create request", status);
        return -110;
    }

    status = pjsip_tsx_create_uac(NULL, tdata, &tsx);
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to create transaction", status);
        return -120;
    }

    if (g[tid].tp) {
        pjsip_tpselector tp_sel;

        pj_bzero(&tp_sel, sizeof(tp_sel));
        tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
        tp_sel.u.transport = g[tid].tp;

        pjsip_tsx_set_transport(tsx, &tp_sel);
    }

    pj_strdup(tdata->pool, &tsx_key, &tsx->transaction_key);

    found = pjsip_tsx_layer_find_tsx(&tsx_key, PJ_FALSE);
    if (found != tsx) {
        return -130;
    }

    pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    flush_events(500);

    if (pjsip_tx_data_dec_ref(tdata) != PJSIP_EBUFDESTROYED) {
        return -140;
    }

    return 0;
}

/* Double terminate test. */
static int double_terminate(unsigned tid)
{
    pj_str_t target, from, tsx_key;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  double terminate test"));

    target = pj_str(g[tid].TARGET_URI);
    from = pj_str(g[tid].FROM_URI);

    /* Create request. */
    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, &target,
                                        &from, &target, NULL, NULL, -1, NULL,
                                        &tdata);
    if (status != PJ_SUCCESS) {
        app_perror("  error: unable to create request", status);
        return -10;
    }

    /* Create transaction. */
    status = pjsip_tsx_create_uac(NULL, tdata, &tsx);
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to create transaction", status);
        return -20;
    }

    if (g[tid].tp) {
        pjsip_tpselector tp_sel;

        pj_bzero(&tp_sel, sizeof(tp_sel));
        tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
        tp_sel.u.transport = g[tid].tp;

        pjsip_tsx_set_transport(tsx, &tp_sel);
    }

    /* Save transaction key for later. */
    pj_strdup_with_null(tdata->pool, &tsx_key, &tsx->transaction_key);

    /* Add reference to transmit buffer (tsx_send_msg() will dec txdata). */
    pjsip_tx_data_add_ref(tdata);

    /* Send message to start timeout timer. */
    status = pjsip_tsx_send_msg(tsx, NULL);

    /* Terminate transaction. */
    status = pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to terminate transaction", status);
        return -30;
    }

    tsx = pjsip_tsx_layer_find_tsx(&tsx_key, PJ_TRUE);
    if (tsx) {
        /* Terminate transaction again. */
        status = pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
        if (status != PJ_SUCCESS) {
            app_perror("   error: unable to terminate transaction", status);
            return -40;
        }
        pj_grp_lock_release(tsx->grp_lock);
    }

    flush_events(500);
    if (pjsip_tx_data_dec_ref(tdata) != PJSIP_EBUFDESTROYED) {
        return -50;
    }

    return PJ_SUCCESS;
}

static void send_request2_cb(void *token, pjsip_event *e)
{
    struct send_request2_data *sr = (struct send_request2_data*)token;

    sr->cb_called = PJ_TRUE;
    if (e->type == PJSIP_EVENT_TSX_STATE && e->body.tsx_state.tsx)
        sr->status_code = e->body.tsx_state.tsx->status_code;
}

/* Test terminating a request sent using pjsip_endpt_send_request2(), e.g.
 * to stop the retransmissions before any response is received.
 */
static int send_request2_test(unsigned tid)
{
    struct send_request2_data *sr = &g[tid].sr;
    pj_str_t target, from, tsx_key;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx = NULL;
    pjsip_tpselector tp_sel;
    pj_bool_t prev_discard;
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  send request2 and terminate test"));

    target = pj_str(g[tid].TARGET_URI);
    from = pj_str(g[tid].FROM_URI);

    /* Discard the outgoing request, so it will never be answered. */
    pjsip_loop_set_discard(g[tid].tp, PJ_TRUE, &prev_discard);

    status = pjsip_endpt_create_request(endpt, &pjsip_options_method, &target,
                                        &from, &target, NULL, NULL, -1, NULL,
                                        &tdata);
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to create request", status);
        rc = -200;
        goto on_return;
    }

    pj_bzero(&tp_sel, sizeof(tp_sel));
    tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
    tp_sel.u.transport = g[tid].tp;
    pjsip_tx_data_set_transport(tdata, &tp_sel);

    pj_bzero(sr, sizeof(*sr));

    status = pjsip_endpt_send_request2(endpt, tdata, -1, sr,
                                       &send_request2_cb, &tsx);
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to send request", status);
        rc = -210;
        goto on_return;
    }

    if (tsx == NULL) {
        PJ_LOG(3,(THIS_FILE, "   error: transaction is not returned"));
        rc = -220;
        goto on_return;
    }

    /* Save the key, to verify that the transaction is unregistered later. */
    pj_strdup_with_null(tsx->pool, &tsx_key, &tsx->transaction_key);

    /* Nothing should complete the transaction by itself. */
    flush_events(100);
    if (sr->cb_called) {
        PJ_LOG(3,(THIS_FILE, "   error: transaction completed prematurely"));
        rc = -230;
        goto on_dec_ref;
    }

    status = pjsip_tsx_terminate_async(tsx, PJSIP_SC_REQUEST_TERMINATED);
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to terminate transaction", status);
        rc = -240;
        goto on_dec_ref;
    }

    flush_events(1000);

    if (!sr->cb_called) {
        PJ_LOG(3,(THIS_FILE, "   error: callback is not called"));
        rc = -250;
        goto on_dec_ref;
    }

    if (sr->status_code != PJSIP_SC_REQUEST_TERMINATED) {
        PJ_LOG(3,(THIS_FILE, "   error: unexpected status code %d",
                  sr->status_code));
        rc = -260;
        goto on_dec_ref;
    }

    /* The transaction must have been unregistered from the transaction
     * layer, while our reference keeps the instance alive.
     */
    if (pjsip_tsx_layer_find_tsx2(&tsx_key, PJ_FALSE) != NULL) {
        PJ_LOG(3,(THIS_FILE, "   error: transaction is still registered"));
        rc = -270;
    }

on_dec_ref:
    pj_grp_lock_dec_ref(tsx->grp_lock);

on_return:
    pjsip_loop_set_discard(g[tid].tp, prev_discard, NULL);
    return rc;
}

static pj_bool_t answer_1xx_on_rx_request(pjsip_rx_data *rdata);
static void answer_1xx_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e);

/* Whether to answer, and the provisional responses reported to the
 * transaction user.
 */
static pj_bool_t answer_1xx_respond;
static unsigned answer_1xx_reported;

/* Answers the requests of terminate_async_test() with 100, and an INVITE
 * also with 180, or drops them, and is the user of their transactions.
 */
static pjsip_module answer_1xx_mod =
{
    NULL, NULL,                         /* prev and next        */
    { "Answer-1xx", 10},                /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_UA_PROXY_LAYER-1,/* Priority             */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    &answer_1xx_on_rx_request,          /* on_rx_request()      */
    NULL,                               /* on_rx_response()     */
    NULL,                               /* on_tx_request()      */
    NULL,                               /* on_tx_response()     */
    &answer_1xx_on_tsx_state,           /* on_tsx_state()       */
};

static pj_bool_t answer_1xx_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_sip_uri *from_uri;

    from_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.from->uri);
    if (pj_strcmp2(&from_uri->user, "tsx_basic_1xx"))
        return PJ_FALSE;

    if (!answer_1xx_respond)
        return PJ_TRUE;

    pjsip_endpt_respond_stateless(endpt, rdata, 100, NULL, NULL, NULL);
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_INVITE_METHOD)
        pjsip_endpt_respond_stateless(endpt, rdata, 180, NULL, NULL, NULL);
    return PJ_TRUE;
}

static void answer_1xx_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    if (tsx->state == PJSIP_TSX_STATE_PROCEEDING &&
        e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
    {
        ++answer_1xx_reported;
    }
}

enum terminate_async_case
{
    TA_RETRANSMIT,      /* the request is retransmitted */
    TA_1XX,             /* the INVITE is answered with 100 and 180 */
    TA_SENDING          /* still being sent, not answered */
};

/* A request being terminated asynchronously must not be retransmitted, and
 * a provisional response must reach the user without cancelling the
 * termination.
 */
static int terminate_async_test(unsigned tid, enum terminate_async_case tc)
{
    static const char *title[] = { "retransmission", "provisional response",
                                   "pending send" };
    char from_buf[PJSIP_MAX_URL_SIZE];
    pj_str_t target, from;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    pjsip_tpselector tp_sel;
    pj_bool_t prev_discard;
    unsigned prev_recv_delay, prev_send_delay;
    unsigned t1 = pjsip_cfg()->tsx.t1;
    unsigned waited;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  terminate async test: %s", title[tc]));

    target = pj_str(g[tid].TARGET_URI);
    if (tc != TA_RETRANSMIT) {
        PJ_TEST_SUCCESS(pjsip_endpt_register_module(endpt, &answer_1xx_mod),
                        NULL, return -400);
        pj_ansi_snprintf(from_buf, sizeof(from_buf),
                         "sip:tsx_basic_1xx@127.0.0.1:%d;transport=%s",
                         tsx_test[tid].port, tsx_test[tid].tp_type);
        from = pj_str(from_buf);
    } else {
        from = pj_str(g[tid].FROM_URI);
    }

    /* The answer, and the end of the send, come after we start terminating */
    pjsip_loop_set_discard(g[tid].tp, tc == TA_RETRANSMIT, &prev_discard);
    pjsip_loop_set_recv_delay(g[tid].tp, tc != TA_RETRANSMIT ? t1 / 4 : 0,
                              &prev_recv_delay);
    pjsip_loop_set_send_callback_delay(g[tid].tp,
                                       tc == TA_SENDING ? t1 / 8 : 0,
                                       &prev_send_delay);

    PJ_TEST_SUCCESS(pjsip_endpt_create_request(endpt,
                                               tc == TA_1XX ?
                                                   &pjsip_invite_method :
                                                   &pjsip_options_method,
                                               &target, &from, &target, NULL,
                                               NULL, -1, NULL, &tdata),
                    NULL, { rc = -405; goto on_return; });
    answer_1xx_respond = (tc == TA_1XX);
    answer_1xx_reported = 0;
    PJ_TEST_SUCCESS(pjsip_tsx_create_uac(tc == TA_RETRANSMIT ? NULL :
                                             &answer_1xx_mod,
                                         tdata, &tsx), NULL,
                    { pjsip_tx_data_dec_ref(tdata); rc = -410;
                      goto on_return; });

    pj_bzero(&tp_sel, sizeof(tp_sel));
    tp_sel.type = PJSIP_TPSELECTOR_TRANSPORT;
    tp_sel.u.transport = g[tid].tp;
    PJ_TEST_SUCCESS(pjsip_tsx_set_transport(tsx, &tp_sel), NULL,
                    { pjsip_tx_data_dec_ref(tdata);
                      pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
                      rc = -415; goto on_return; });

    PJ_TEST_SUCCESS(pjsip_tsx_send_msg(tsx, NULL), NULL,
                    { pjsip_tx_data_dec_ref(tdata);
                      pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
                      rc = -420; goto on_return; });

    /* Keep the transaction to check it afterwards */
    pj_grp_lock_add_ref(tsx->grp_lock);

    /* Terminate after the first retransmission or the answer is due */
    PJ_TEST_SUCCESS(pjsip_tsx_terminate_async2(tsx,
                                               PJSIP_SC_REQUEST_TERMINATED,
                                               NULL, t1 * 4),
                    NULL, { pjsip_tsx_terminate(tsx,
                                                PJSIP_SC_REQUEST_TERMINATED);
                            rc = -430; });

    /* The pending retransmission is cancelled */
    pj_grp_lock_acquire(tsx->grp_lock);
    if (rc == 0 && pj_timer_entry_running(&tsx->retransmit_timer))
        rc = -432;
    pj_grp_lock_release(tsx->grp_lock);

    /* Once sent, the request is not to be retransmitted */
    flush_events(t1);
    pj_grp_lock_acquire(tsx->grp_lock);
    if (rc == 0 && tc == TA_SENDING &&
        pj_timer_entry_running(&tsx->retransmit_timer))
    {
        rc = -435;
    }
    pj_grp_lock_release(tsx->grp_lock);

    /* Wait for the termination, generously for slow runners */
    for (waited = 0; rc == 0 && waited < t1 * 64 &&
                     tsx->state < PJSIP_TSX_STATE_TERMINATED; waited += 100)
    {
        flush_events(100);
    }

    pj_grp_lock_acquire(tsx->grp_lock);
    if (rc == 0 && tsx->retransmit_count != 0)
        rc = -440;
    if (rc == 0 && tsx->state < PJSIP_TSX_STATE_TERMINATED)
        rc = -445;
    if (rc == 0 && tsx->status_code != PJSIP_SC_REQUEST_TERMINATED)
        rc = -450;
    if (rc == 0 && answer_1xx_reported != (tc == TA_1XX ? 2 : 0))
        rc = -452;
    pj_grp_lock_release(tsx->grp_lock);
    if (rc != 0)
        pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    pj_grp_lock_dec_ref(tsx->grp_lock);

on_return:
    pjsip_loop_set_send_callback_delay(g[tid].tp, prev_send_delay, NULL);
    pjsip_loop_set_recv_delay(g[tid].tp, prev_recv_delay, NULL);
    pjsip_loop_set_discard(g[tid].tp, prev_discard, NULL);
    if (tc != TA_RETRANSMIT) {
        PJ_TEST_SUCCESS(pjsip_endpt_unregister_module(endpt,
                                                      &answer_1xx_mod),
                        NULL, if (rc == 0) rc = -455);
    }
    return rc;
}

int tsx_basic_test(unsigned tid)
{
    struct tsx_test_param *param = &tsx_test[tid];
    pjsip_transport *loop = NULL;
    int status;

    g[tid].tp = NULL;

    if (param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
        PJ_TEST_SUCCESS(pjsip_loop_start(endpt, &loop), NULL, return -10);
        pjsip_transport_add_ref(loop);
        g[tid].tp = loop;
    }
    pj_ansi_snprintf(g[tid].TARGET_URI, sizeof(g[tid].TARGET_URI),
                    "sip:tsx_basic_test@127.0.0.1:%d;transport=%s",
                    param->port, param->tp_type);
    pj_ansi_snprintf(g[tid].FROM_URI, sizeof(g[tid].FROM_URI),
                    "sip:tsx_basic_test@127.0.0.1:%d;transport=%s",
                    param->port, param->tp_type);

    status = tsx_layer_test(tid);
    if (status != 0)
        goto on_return;

    status = double_terminate(tid);
    if (status != 0)
        goto on_return;

    /* These tests need the loop transport to blackhole the request. */
    if (g[tid].tp) {
        status = send_request2_test(tid);
        if (status != 0)
            goto on_return;

        status = terminate_async_test(tid, TA_RETRANSMIT);
        if (status != 0)
            goto on_return;

        status = terminate_async_test(tid, TA_1XX);
        if (status != 0)
            goto on_return;

        status = terminate_async_test(tid, TA_SENDING);
        if (status != 0)
            goto on_return;
    }

    status = 0;

on_return:
    if (loop) {
        /* Order must be shutdown then dec_ref so it gets destroyed */
        pjsip_transport_shutdown(loop);
        pjsip_transport_dec_ref(loop);
        flush_events(500);
    }
    return status;
}

/**************************************************************************/

struct tsx_test_state
{
    pj_size_t pool_cnt;
};

static void save_tsx_test_state(struct tsx_test_state *st)
{
    st->pool_cnt = caching_pool.used_count;
}

static pj_status_t check_tsx_test_state(struct tsx_test_state *st)
{
    if (caching_pool.used_count > st->pool_cnt)
        return -1;

    return 0;
}

static void destroy_endpt()
{
    pjsip_endpt_destroy(endpt);
    endpt = NULL;
}

static pj_status_t init_endpt()
{
    pj_str_t ns = { "10.187.27.172", 13};       /* just a random, unreachable IP */
    pj_dns_resolver *resolver;
    pj_status_t rc;

    rc = pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);
    if (rc != PJ_SUCCESS) {
        app_perror("pjsip_endpt_create", rc);
        return rc;
    }

    /* Start transaction layer module. */
    rc = pjsip_tsx_layer_init_module(endpt);
    if (rc != PJ_SUCCESS) {
        app_perror("tsx_layer_init", rc);
        return rc;
    }

    rc = pjsip_udp_transport_start(endpt, NULL, NULL, 1,  NULL);
    if (rc != PJ_SUCCESS) {
        app_perror("udp init", rc);
        return rc;
    }

#if PJ_HAS_TCP
    rc = pjsip_tcp_transport_start(endpt, NULL, 1, NULL);
    if (rc != PJ_SUCCESS) {
        app_perror("tcp init", rc);
        return rc;
    }
#endif

    rc = pjsip_endpt_create_resolver(endpt, &resolver);
    if (rc != PJ_SUCCESS) {
        app_perror("create resolver", rc);
        return rc;
    }

    rc = pj_dns_resolver_set_ns(resolver, 1, &ns, NULL);
    if (rc != PJ_SUCCESS) {
        app_perror("set nameserver", rc);
        return rc;
    }

    rc = pjsip_endpt_set_resolver(endpt, resolver);
    if (rc != PJ_SUCCESS) {
        app_perror("set resolver", rc);
        return rc;
    }

    return PJ_SUCCESS;
}

static int tsx_create_and_send_req(void *arg)
{
    pj_str_t dst_uri = pj_str((char*)arg);
    pj_str_t from_uri = pj_str((char*)"<sip:user@host>");
    pjsip_tx_data *tdata;
    pj_status_t status;

    status = pjsip_endpt_create_request(endpt, &pjsip_options_method,
                                        &dst_uri, &from_uri, &dst_uri,
                                        NULL, NULL, -1, NULL,
                                        &tdata);
    if (status != PJ_SUCCESS)
        return status;

    status = pjsip_endpt_send_request(endpt, tdata, -1, NULL, NULL);
    if (status != PJ_SUCCESS)
        return status;

    return PJ_SUCCESS;
}

int tsx_destroy_test()
{
    struct tsx_test_state state;
    struct test_desc
    {
        const char *title;
        int (*func)(void*);
        void *arg;
        int sleep_before_unload;
        int sleep_after_unload;
    } test_entries[] =
    {
        {
            "normal unable to resolve",
            &tsx_create_and_send_req,
            "sip:user@somehost",
            20000,
            1
        },
        {
            "resolve and destroy, wait",
            &tsx_create_and_send_req,
            "sip:user@somehost",
            1,
            20000
        },
#if PJ_HAS_TCP
        {
            "tcp connect and destroy",
            &tsx_create_and_send_req,
            "sip:user@10.125.36.63:58517;transport=tcp",
            60000,
            1000
        },
        {
            "tcp connect and destroy 2",
            &tsx_create_and_send_req,
            "sip:user@10.125.36.63:58517;transport=tcp",
            1,
            60000
        },
#endif
    };
    int rc;
    unsigned i;
    const int INDENT = 2;

    pj_log_add_indent(INDENT);
    destroy_endpt();

    for (i=0; i<PJ_ARRAY_SIZE(test_entries); ++i) {
        struct test_desc *td = &test_entries[i];

        PJ_LOG(3,(THIS_FILE, "%s", td->title));

        pj_log_add_indent(INDENT);
        save_tsx_test_state(&state);

        rc = init_endpt();
        if (rc != PJ_SUCCESS) {
            pj_log_add_indent(-INDENT*2);
            return -10;
        }

        rc = td->func(td->arg);
        if (rc != PJ_SUCCESS) {
            pj_log_add_indent(-INDENT*2);
            return -20;
        }

        flush_events(td->sleep_before_unload);
        pjsip_tsx_layer_destroy();
        flush_events(td->sleep_after_unload);
        destroy_endpt();

        rc = check_tsx_test_state(&state);
        if (rc != PJ_SUCCESS) {
            init_endpt();
            pj_log_add_indent(-INDENT*2);
            return -30;
        }

        pj_log_add_indent(-INDENT);
    }

    init_endpt();

    pj_log_add_indent(-INDENT);
    return 0;
}


#if PJ_HAS_TCP && PJ_HAS_THREADS
static volatile pj_bool_t poller_quit;

static int poller_proc(void *arg)
{
    PJ_UNUSED_ARG(arg);

    while (!poller_quit) {
        pj_time_val timeout = {0, 10};
        pjsip_endpt_handle_events(endpt, &timeout);
    }
    return 0;
}

/* Terminate a transaction while its send callback waits for its lock. Without
 * the fix the callback releases the transaction twice, which is caught by the
 * group lock assertion or by ASan.
 */
int tsx_send_cb_terminate_test(unsigned tid)
{
    char uri[PJSIP_MAX_URL_SIZE];
    pj_str_t target;
    pjsip_tpselector tp_sel;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    pj_pool_t *pool;
    pj_thread_t *thread = NULL;
    int rc = 0;

    pj_ansi_snprintf(uri, sizeof(uri),
                     "sip:tsx_send_cb_test@127.0.0.1:%d;transport=tcp",
                     tsx_test[tid].port);
    target = pj_str(uri);

    PJ_TEST_SUCCESS(pjsip_endpt_create_request(endpt, &pjsip_invite_method,
                                               &target, &target, &target,
                                               NULL, NULL, -1, NULL, &tdata),
                    NULL, return -300);
    PJ_TEST_SUCCESS(pjsip_tsx_create_uac(NULL, tdata, &tsx), NULL,
                    { pjsip_tx_data_dec_ref(tdata); return -305; });

    /* A new connection, so that the send waits for the connect */
    pj_bzero(&tp_sel, sizeof(tp_sel));
    tp_sel.disable_connection_reuse = PJ_TRUE;
    PJ_TEST_SUCCESS(pjsip_tsx_set_transport(tsx, &tp_sel), NULL,
                    { rc = -310; goto on_error; });

    pool = pjsip_endpt_create_pool(endpt, "tsxsendcb", 512, 512);
    PJ_TEST_NOT_NULL(pool, NULL, { rc = -312; goto on_error; });

    pjsip_tx_data_add_ref(tdata);

    pj_grp_lock_acquire(tsx->grp_lock);
    PJ_TEST_SUCCESS(pjsip_tsx_send_msg(tsx, NULL), NULL,
                    { pjsip_tx_data_dec_ref(tdata); rc = -315;
                      goto on_terminate; });
    PJ_TEST_TRUE(tdata->is_pending, "the send should wait for the connect",
                 { rc = -320; goto on_terminate; });

    poller_quit = PJ_FALSE;
    PJ_TEST_SUCCESS(pj_thread_create(pool, "tsxsendcb", &poller_proc, NULL,
                                     0, 0, &thread),
                    NULL, { rc = -325; goto on_terminate; });

    /* Let the poller complete the connect, then its send callback waits for
     * our lock. If it is slower, the test passes without reaching the race.
     */
    pj_thread_sleep(500);

on_terminate:
    pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    pj_grp_lock_release(tsx->grp_lock);

    if (thread) {
        flush_events(500);
        poller_quit = PJ_TRUE;
        pj_thread_join(thread);
        pj_thread_destroy(thread);
    }
    pj_pool_release(pool);

    flush_events(500);
    if (pjsip_tx_data_dec_ref(tdata) != PJSIP_EBUFDESTROYED && rc == 0)
        rc = -330;

    return rc;

on_error:
    pjsip_tx_data_dec_ref(tdata);
    pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
    return rc;
}
#endif

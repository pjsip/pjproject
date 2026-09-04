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

#include "test.h"
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "dlg_target_refresh_test.c"

/*
 * Regression test: a secure (SIPS) dialog's target refresh must not accept
 * a non-SIPS Contact. Before the fix, a target refresh (in-dialog request,
 * or a response to a dialog-creating/refreshing request) applied any
 * Contact URI to dlg->target without checking its scheme against
 * dlg->secure, and did so even when the request itself was ultimately
 * rejected (sip_dialog.c, "Updating remote contact in target refresh").
 * Since dlg->target becomes the Request-URI of subsequent in-dialog
 * requests (sip_dialog.c: dlg_create_request_throw), and
 * pjsip_get_dest_info()'s automatic-secure-transport logic (ticket #1740)
 * checks that very same (by-then-downgraded) URI, a single malicious
 * Contact permanently downgrades all further traffic in the dialog to
 * plaintext.
 *
 * Cases 1-3 exercise the in-dialog request-path check in
 * pjsip_dlg_on_rx_request(), right after the CSeq check: a non-SIPS
 * Contact on a secure dialog is rejected outright with 400, before
 * dialog usages ever see the request (RFC 5630 Section 5.1.2). Case 4
 * exercises the response-path gate (pjsip_dlg_on_rx_response, via
 * dlg_target_refresh_allowed() in sip_dialog.c) that a 2xx response to an
 * in-dialog target refresh goes through -- a response can't be rejected
 * the way a request can, so the Contact is silently ignored there
 * instead (target left unchanged) and only logged as a warning. The
 * third target-refresh site (the response to the *initial*
 * dialog-creating request, e.g. 1xx/2xx to INVITE) uses the identical
 * response-path gate but isn't separately exercised here, since
 * establishing that scenario needs a second dialog pair with the secure
 * flag forced before the first response arrives.
 *
 * Case 5 covers a gap in the request-path reject: it keys on the
 * request-line method, but the target-refresh block itself keys on the
 * CSeq header's method, and ACK -- which never creates a UAS transaction
 * -- skips whatever would otherwise catch a CSeq/request-line method
 * mismatch. A forged ACK with a spoofed CSeq method reaches the
 * target-refresh block directly, where dlg_target_refresh_allowed() is
 * still applied as a fallback (ignore, since ACK can't be answered).
 */

static char contact_buf[80];
static pj_str_t contact_uri;

static struct dlg_tr_test_t
{
    pjsip_dialog    *uas_dlg;
    char             target_buf[PJSIP_MAX_URL_SIZE];
    pj_str_t         target_before;

    /* When non-NULL, the dialog "usage" installed on uas_dlg (below)
     * overrides the Contact it puts in a 200 response to an in-dialog
     * UPDATE with this URI, to exercise the response-path target-refresh
     * gate (Case 4).
     */
    const char      *uas_response_contact;

    /* Status code of the last response to an in-dialog UPDATE seen by
     * the UAC side (0 if none yet), set by on_rx_response() below.
     */
    int              last_update_status;
} dlg_tr_test;

static pj_str_t print_target(pjsip_dialog *dlg, char *buf, pj_size_t buf_len)
{
    pj_str_t s;
    pj_ssize_t len;

    len = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR, dlg->target, buf,
                          buf_len);
    s.ptr = buf;
    s.slen = (len < 1)? 0 : len;
    return s;
}

static pjsip_module mod_dlg_tr_test;
static pjsip_module mod_dlg_usage;

/**************** MODULE TO RECEIVE INITIAL INVITE ******************/

static pj_bool_t on_rx_request(pjsip_rx_data *rdata)
{
    if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG &&
        rdata->msg_info.msg->line.req.method.id == PJSIP_INVITE_METHOD)
    {
        pjsip_dialog *dlg;
        pjsip_tx_data *tdata;
        pj_str_t uri;

        if (!is_user_equal(rdata->msg_info.from, "dlg_target_refresh_test"))
            return PJ_FALSE;

        uri = contact_uri;
        PJ_TEST_SUCCESS(pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(),
                                                          rdata, &uri, &dlg),
                        NULL, { pj_assert(0); return PJ_FALSE; });

        /* Simulate a dialog that was established as a secure (SIPS)
         * dialog, e.g. via "INVITE sips:... transport=tls". We don't spin
         * up a real TLS transport for this test: dlg->secure is the only
         * thing the target refresh code (and this fix) consults, so it is
         * set directly here.
         */
        dlg->secure = PJ_TRUE;
        dlg_tr_test.uas_dlg = dlg;
        dlg_tr_test.target_before = print_target(dlg, dlg_tr_test.target_buf,
                                                 sizeof(dlg_tr_test.target_buf));

        /* Keep the dialog alive for the duration of the test. */
        pjsip_dlg_inc_session(dlg, &mod_dlg_tr_test);

        /* Answer subsequent in-dialog UPDATE requests ourselves, instead
         * of letting them fall through to the dialog's default "500
         * Unhandled by dialog usages" (also lets Case 4 below control the
         * Contact of the 200 response).
         */
        PJ_TEST_SUCCESS(pjsip_dlg_add_usage(dlg, &mod_dlg_usage, NULL),
                        NULL, { pj_assert(0); return PJ_FALSE; });

        PJ_TEST_SUCCESS(pjsip_dlg_create_response(dlg, rdata, 200, NULL,
                                                  &tdata),
                        NULL, { pj_assert(0); return PJ_FALSE; });
        PJ_TEST_SUCCESS(pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata),
                                                tdata),
                        NULL, { pj_assert(0); return PJ_FALSE; });

        pjsip_dlg_dec_lock(dlg);
        return PJ_TRUE;
    }

    return PJ_FALSE;
}

static pjsip_module mod_dlg_tr_test =
{
    NULL, NULL,                     /* prev, next.              */
    { "mod-dlg-tr-test", 15 },      /* Name.                    */
    -1,                             /* Id                       */
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority                 */
    NULL,                           /* load()                   */
    NULL,                           /* start()                  */
    NULL,                           /* stop()                   */
    NULL,                           /* unload()                 */
    &on_rx_request,                 /* on_rx_request()          */
    NULL,                           /* on_rx_response()         */
    NULL,                           /* on_tx_request.           */
    NULL,                           /* on_tx_response()         */
    NULL,                           /* on_tsx_state()           */
};


/**************** MODULE TO OBSERVE UPDATE RESPONSE STATUS ******************/

/* By the time a response reaches endpoint-module priority
 * PJSIP_MOD_PRIORITY_APPLICATION, the transaction layer
 * (PJSIP_MOD_PRIORITY_TSX_LAYER, a lower/earlier priority) has already
 * matched it to the pending UAC transaction and consumed it -- an
 * ordinary on_rx_response() at APPLICATION priority never sees it. Log
 * it instead at a priority ahead of the transaction layer, mirroring
 * inv_offer_answer_test.c's mod_msg_logger, purely to observe the status
 * code; always return PJ_FALSE so normal transaction processing is
 * unaffected.
 */
static pj_bool_t on_rx_response(pjsip_rx_data *rdata)
{
    if (rdata->msg_info.msg->type == PJSIP_RESPONSE_MSG &&
        pj_stricmp2(&rdata->msg_info.cseq->method.name, "UPDATE") == 0 &&
        is_user_equal(rdata->msg_info.to, "dlg_target_refresh_test"))
    {
        dlg_tr_test.last_update_status = rdata->msg_info.msg->line.status.code;
    }

    return PJ_FALSE;
}

static pjsip_module mod_dlg_tr_logger =
{
    NULL, NULL,                          /* prev, next.              */
    { "mod-dlg-tr-logger", 18 },         /* Name.                    */
    -1,                                  /* Id                       */
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority                 */
    NULL,                                /* load()                   */
    NULL,                                /* start()                  */
    NULL,                                /* stop()                   */
    NULL,                                /* unload()                 */
    NULL,                                /* on_rx_request()          */
    &on_rx_response,                     /* on_rx_response()         */
    NULL,                                /* on_tx_request.           */
    NULL,                                /* on_tx_response()         */
    NULL,                                /* on_tsx_state()           */
};


/**************** DIALOG USAGE TO ANSWER IN-DIALOG UPDATE ******************/

/* pjsip_dlg_on_rx_request() reports the request to dialog usages *before*
 * feeding it to the transaction (pjsip_tsx_recv_msg()), so the UAS
 * transaction is still in state Null while a usage's on_rx_request() is
 * running -- too early to send a response through it. Build the response
 * synchronously (it only needs the still-valid rdata), but defer the
 * actual send to a zero-delay timer, by which time
 * pjsip_dlg_on_rx_request()'s own (unconditional) pjsip_tsx_recv_msg()
 * call has moved the transaction to Trying. Mirrors the
 * schedule_send_response() pattern in tsx_uas_test.c.
 */
struct dlg_tr_deferred_response
{
    pj_str_t        tsx_key;
    pjsip_tx_data  *tdata;
};

static void dlg_tr_send_response_timer(pj_timer_heap_t *timer_heap,
                                       struct pj_timer_entry *entry)
{
    struct dlg_tr_deferred_response *r =
        (struct dlg_tr_deferred_response*) entry->user_data;
    pjsip_transaction *tsx;

    PJ_UNUSED_ARG(timer_heap);

    tsx = pjsip_tsx_layer_find_tsx(&r->tsx_key, PJ_TRUE);
    if (!tsx) {
        pjsip_tx_data_dec_ref(r->tdata);
        return;
    }

    pjsip_tsx_send_msg(tsx, r->tdata);
    pj_grp_lock_release(tsx->grp_lock);
}

static pj_bool_t usage_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_dialog *dlg = pjsip_rdata_get_dlg(rdata);
    pjsip_transaction *tsx;
    pjsip_tx_data *tdata;
    pjsip_contact_hdr *contact;
    pj_size_t len;
    char *buf;
    struct dlg_tr_deferred_response *r;
    pj_timer_entry *timer;
    pj_time_val delay;

    /* mod_dlg_usage is registered as a normal endpoint module (so it has
     * a valid module id for pjsip_dlg_add_usage()), but it must only
     * ever act as a usage of dlg_tr_test.uas_dlg specifically -- other
     * pjsip-test cases share the same endpoint and may send UPDATE
     * requests of their own, in-dialog or not.
     */
    if (!dlg || dlg != dlg_tr_test.uas_dlg)
        return PJ_FALSE;

    if (pj_stricmp2(&rdata->msg_info.msg->line.req.method.name,
                    "UPDATE") != 0)
    {
        return PJ_FALSE;
    }

    tsx = pjsip_rdata_get_tsx(rdata);
    if (!tsx)
        return PJ_FALSE;

    if (pjsip_dlg_create_response(dlg, rdata, 200, NULL, &tdata) != PJ_SUCCESS)
        return PJ_FALSE;

    /* pjsip_dlg_create_response() doesn't add a Contact header on its
     * own (unlike pjsip_dlg_modify_response()); add one here so the
     * response-path target-refresh gate has a Contact to evaluate, and
     * optionally override its URI for Case 4.
     */
    contact = (pjsip_contact_hdr*)
              pjsip_hdr_clone(tdata->pool, dlg->local.contact);
    if (dlg_tr_test.uas_response_contact) {
        len = pj_ansi_strlen(dlg_tr_test.uas_response_contact);
        buf = (char*) pj_pool_alloc(tdata->pool, len+1);
        pj_memcpy(buf, dlg_tr_test.uas_response_contact, len+1);
        contact->uri = pjsip_parse_uri(tdata->pool, buf, len, 0);
    }
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)contact);

    r = PJ_POOL_ALLOC_T(tdata->pool, struct dlg_tr_deferred_response);
    pj_strdup(tdata->pool, &r->tsx_key, &tsx->transaction_key);
    r->tdata = tdata;

    timer = PJ_POOL_ZALLOC_T(tdata->pool, pj_timer_entry);
    timer->user_data = r;
    timer->cb = &dlg_tr_send_response_timer;

    pj_bzero(&delay, sizeof(delay));
    if (pjsip_endpt_schedule_timer(dlg->endpt, timer, &delay) != PJ_SUCCESS) {
        pjsip_tx_data_dec_ref(tdata);
        return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pjsip_module mod_dlg_usage =
{
    NULL, NULL,                     /* prev, next.              */
    { "mod-dlg-tr-usage", 16 },     /* Name.                    */
    -1,                             /* Id                       */
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority                 */
    NULL,                           /* load()                   */
    NULL,                           /* start()                  */
    NULL,                           /* stop()                   */
    NULL,                           /* unload()                 */
    &usage_on_rx_request,           /* on_rx_request()          */
    NULL,                           /* on_rx_response()         */
    NULL,                           /* on_tx_request.           */
    NULL,                           /* on_tx_response()         */
    NULL,                           /* on_tsx_state()           */
};


/**************** THE TEST ******************/

/* Send an in-dialog UPDATE from uac_dlg with the given Contact URI, and
 * wait for it to be processed by the UAS dialog.
 */
static pj_status_t send_update_with_contact(pjsip_dialog *uac_dlg,
                                            const char *contact_uri_str)
{
    pjsip_method method;
    pj_str_t update_str = { "UPDATE", 6 };
    pjsip_tx_data *tdata;
    pjsip_contact_hdr *contact;
    char *uri_buf;
    pj_size_t uri_len;
    pj_status_t status;

    pjsip_method_init_np(&method, &update_str);

    status = pjsip_dlg_create_request(uac_dlg, &method, -1, &tdata);
    if (status != PJ_SUCCESS)
        return status;

    contact = (pjsip_contact_hdr*)
              pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, NULL);
    if (!contact) {
        pjsip_tx_data_dec_ref(tdata);
        return PJSIP_EMISSINGHDR;
    }

    uri_len = pj_ansi_strlen(contact_uri_str);
    uri_buf = (char*) pj_pool_alloc(tdata->pool, uri_len+1);
    pj_memcpy(uri_buf, contact_uri_str, uri_len+1);

    contact->uri = pjsip_parse_uri(tdata->pool, uri_buf, uri_len, 0);
    if (!contact->uri) {
        pjsip_tx_data_dec_ref(tdata);
        return PJSIP_EINVALIDURI;
    }

    return pjsip_dlg_send_request(uac_dlg, tdata, -1, NULL);
}

/* Send a forged ACK from uac_dlg: request line is ACK (so it never
 * creates a UAS transaction and skips whatever CSeq/request-line method
 * match check that would otherwise catch this), but the CSeq header's
 * method is overwritten to an unrelated target-refresh-eligible method
 * (INVITE), and a Contact header is added even though ACK doesn't
 * normally carry one. Reproduces the gap nanangizz identified: the
 * target-refresh block keys on the CSeq method, not the request line.
 */
static pj_status_t send_forged_ack_with_contact(pjsip_dialog *uac_dlg,
                                                const char *contact_uri_str)
{
    pjsip_tx_data *tdata;
    pjsip_cseq_hdr *cseq;
    pjsip_contact_hdr *contact;
    char *uri_buf;
    pj_size_t uri_len;
    pj_status_t status;

    status = pjsip_dlg_create_request(uac_dlg, pjsip_get_ack_method(),
                                      99999, &tdata);
    if (status != PJ_SUCCESS)
        return status;

    cseq = (pjsip_cseq_hdr*)
           pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
    if (!cseq) {
        pjsip_tx_data_dec_ref(tdata);
        return PJSIP_EMISSINGHDR;
    }
    pjsip_method_set(&cseq->method, PJSIP_INVITE_METHOD);

    uri_len = pj_ansi_strlen(contact_uri_str);
    uri_buf = (char*) pj_pool_alloc(tdata->pool, uri_len+1);
    pj_memcpy(uri_buf, contact_uri_str, uri_len+1);

    contact = pjsip_contact_hdr_create(tdata->pool);
    contact->uri = pjsip_parse_uri(tdata->pool, uri_buf, uri_len, 0);
    if (!contact->uri) {
        pjsip_tx_data_dec_ref(tdata);
        return PJSIP_EINVALIDURI;
    }
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)contact);

    return pjsip_dlg_send_request(uac_dlg, tdata, -1, NULL);
}

int dlg_target_refresh_test(void)
{
    pjsip_dialog *uac_dlg;
    pj_str_t uri;
    pjsip_tx_data *tdata;
    char attacker_buf[PJSIP_MAX_URL_SIZE];
    char legit_buf[PJSIP_MAX_URL_SIZE];
    char uac_before_buf[PJSIP_MAX_URL_SIZE];
    pj_str_t target_after;
    pj_str_t uac_target_before;
    int rc = 0;

    /* Init UA layer */
    if (pjsip_ua_instance()->id == -1) {
        pjsip_ua_init_param ua_param;
        pj_bzero(&ua_param, sizeof(ua_param));
        pjsip_ua_init_module(endpt, &ua_param);
    }

    PJ_TEST_SUCCESS(pjsip_endpt_register_module(endpt, &mod_dlg_tr_test),
                    NULL, return -2);
    PJ_TEST_SUCCESS(pjsip_endpt_register_module(endpt, &mod_dlg_usage),
                    NULL, return -3);
    PJ_TEST_SUCCESS(pjsip_endpt_register_module(endpt, &mod_dlg_tr_logger),
                    NULL, return -4);

    /* Create SIP UDP transport on ephemeral port */
    {
        pj_sockaddr_in addr;
        pjsip_transport *tp;

        pj_sockaddr_in_init(&addr, NULL, 0);
        PJ_TEST_SUCCESS(pjsip_udp_transport_start(endpt, &addr, NULL, 1, &tp),
                        NULL, return -5);

        pj_ansi_snprintf(contact_buf, sizeof(contact_buf),
                         "sip:dlg_target_refresh_test@127.0.0.1:%d",
                         tp->local_name.port);
        contact_uri = pj_str(contact_buf);
    }

    pj_bzero(&dlg_tr_test, sizeof(dlg_tr_test));

    /*
     * Establish a dialog with itself, then mark the UAS side as secure
     * (see comment in on_rx_request()).
     */
    uri = contact_uri;
    PJ_TEST_SUCCESS(pjsip_dlg_create_uac(pjsip_ua_instance(), &uri, &uri,
                                         &uri, &uri, &uac_dlg),
                    NULL, return -10);
    pjsip_dlg_inc_session(uac_dlg, &mod_dlg_tr_test);

    PJ_TEST_SUCCESS(pjsip_dlg_create_request(uac_dlg,
                                             pjsip_get_invite_method(), -1,
                                             &tdata),
                    NULL, return -20);
    PJ_TEST_SUCCESS(pjsip_dlg_send_request(uac_dlg, tdata, -1, NULL),
                    NULL, return -30);

    flush_events(500);

    PJ_TEST_NOT_NULL(dlg_tr_test.uas_dlg, "UAS dialog was not created",
                     return -40);
    PJ_TEST_EQ(uac_dlg->state, PJSIP_DIALOG_STATE_ESTABLISHED,
              "UAC dialog did not reach ESTABLISHED state after 200/INVITE",
              return -50);

    /*
     * Case 1: malicious in-dialog UPDATE from an attacker-controlled
     * Contact using "sip:" while the dialog is secure. This must be
     * rejected with 400 before any dialog usage sees it, and must NOT
     * change the UAS dialog's target.
     */
    dlg_tr_test.last_update_status = 0;
    pj_ansi_snprintf(attacker_buf, sizeof(attacker_buf),
                     "sip:mallory@127.0.0.1:44444;transport=udp");
    PJ_TEST_SUCCESS(send_update_with_contact(uac_dlg, attacker_buf),
                    NULL, return -60);

    flush_events(500);

    target_after = print_target(dlg_tr_test.uas_dlg, legit_buf,
                                sizeof(legit_buf));
    PJ_TEST_EQ(pj_strcmp(&target_after, &dlg_tr_test.target_before), 0,
              "secure dialog target was downgraded by a non-SIPS Contact "
              "in target refresh",
              { rc = -70; goto on_return; });
    PJ_TEST_EQ(dlg_tr_test.uas_dlg->secure, PJ_TRUE,
              "dlg->secure flag was unexpectedly cleared",
              { rc = -71; goto on_return; });
    PJ_TEST_EQ(dlg_tr_test.last_update_status, 400,
              "malicious UPDATE on a secure dialog was not rejected "
              "with 400",
              { rc = -72; goto on_return; });

    /*
     * Case 2: legitimate refresh with a "sips:" Contact on a secure
     * dialog must still be applied (the fix must not block same/higher
     * security refreshes).
     */
    dlg_tr_test.last_update_status = 0;
    PJ_TEST_SUCCESS(send_update_with_contact(uac_dlg,
                                             "sips:bob@127.0.0.1:44445"),
                    NULL, { rc = -80; goto on_return; });

    flush_events(500);

    target_after = print_target(dlg_tr_test.uas_dlg, legit_buf,
                                sizeof(legit_buf));
    PJ_TEST_EQ(pj_strcmp2(&target_after, "<sips:bob@127.0.0.1:44445>"), 0,
              "legitimate SIPS target refresh on a secure dialog was "
              "incorrectly blocked",
              { rc = -90; goto on_return; });
    PJ_TEST_EQ(dlg_tr_test.last_update_status, 200,
              "legitimate SIPS UPDATE on a secure dialog was not "
              "answered normally",
              { rc = -91; goto on_return; });

    /*
     * Case 3: on a non-secure dialog, a "sip:" Contact refresh must
     * still be applied as before (the fix must not affect plain
     * dialogs).
     */
    dlg_tr_test.uas_dlg->secure = PJ_FALSE;
    dlg_tr_test.last_update_status = 0;

    PJ_TEST_SUCCESS(send_update_with_contact(uac_dlg,
                                             "sip:carol@127.0.0.1:44446"),
                    NULL, { rc = -100; goto on_return; });

    flush_events(500);

    target_after = print_target(dlg_tr_test.uas_dlg, legit_buf,
                                sizeof(legit_buf));
    PJ_TEST_EQ(pj_strcmp2(&target_after, "<sip:carol@127.0.0.1:44446>"), 0,
              "target refresh on a non-secure dialog regressed",
              { rc = -110; goto on_return; });
    PJ_TEST_EQ(dlg_tr_test.last_update_status, 200,
              "sip: UPDATE on a non-secure dialog was not answered "
              "normally",
              { rc = -111; goto on_return; });

    /*
     * Case 4: a malicious 2xx response to an in-dialog UPDATE must not
     * downgrade the UAC's own secure dialog either. This exercises the
     * response-path target-refresh gate (pjsip_dlg_on_rx_response),
     * which shares dlg_target_refresh_allowed() with the request-path
     * gate covered by Cases 1-3.
     */
    uac_dlg->secure = PJ_TRUE;
    uac_target_before = print_target(uac_dlg, uac_before_buf,
                                     sizeof(uac_before_buf));
    dlg_tr_test.uas_response_contact =
        "sip:mallory2@127.0.0.1:44447;transport=udp";

    PJ_TEST_SUCCESS(send_update_with_contact(uac_dlg, contact_buf),
                    NULL, { rc = -120; goto on_return; });

    flush_events(500);

    target_after = print_target(uac_dlg, legit_buf, sizeof(legit_buf));
    PJ_TEST_EQ(pj_strcmp(&target_after, &uac_target_before), 0,
              "secure dialog's own target was downgraded by a non-SIPS "
              "Contact in a 2xx response to an in-dialog target refresh",
              { rc = -130; goto on_return; });

    /*
     * Case 5: a forged ACK -- request line ACK, but CSeq method
     * overwritten to INVITE, carrying a malicious Contact -- must not
     * downgrade the UAS's secure dialog either. ACK never creates a UAS
     * transaction, so it skips the request-path reject-with-400 check
     * (which keys on the request line method and can't apply here
     * anyway, since ACK can't be answered); this exercises
     * dlg_target_refresh_allowed() being applied at the target-refresh
     * block itself as the fallback for this case.
     */
    dlg_tr_test.uas_dlg->secure = PJ_TRUE;
    dlg_tr_test.target_before = print_target(dlg_tr_test.uas_dlg,
                                             dlg_tr_test.target_buf,
                                             sizeof(dlg_tr_test.target_buf));

    PJ_TEST_SUCCESS(send_forged_ack_with_contact(uac_dlg,
                          "sip:mallory3@127.0.0.1:44448;transport=udp"),
                    NULL, { rc = -140; goto on_return; });

    flush_events(500);

    target_after = print_target(dlg_tr_test.uas_dlg, legit_buf,
                                sizeof(legit_buf));
    PJ_TEST_EQ(pj_strcmp(&target_after, &dlg_tr_test.target_before), 0,
              "secure dialog target was downgraded by a non-SIPS Contact "
              "in a forged ACK (request-line ACK, CSeq method INVITE)",
              { rc = -150; goto on_return; });

on_return:
    pjsip_dlg_dec_session(uac_dlg, &mod_dlg_tr_test);
    if (dlg_tr_test.uas_dlg)
        pjsip_dlg_dec_session(dlg_tr_test.uas_dlg, &mod_dlg_tr_test);
    flush_events(500);
    return rc;
}

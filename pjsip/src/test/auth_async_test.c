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

/*
 * Unit tests for the manual/asynchronous client authentication feature
 * (pjsip_auth_clt_async_configure / pjsip_auth_clt_async_send_req).
 *
 * Scenarios covered:
 *   1. Synchronous resend: application calls pjsip_auth_clt_async_send_req
 *      from within the challenge callback (deadlock-fix path).
 *   2. Asynchronous (deferred) resend: application stores the token and tdata
 *      during the callback, then sends from the event loop afterwards.
 *   3. Double-send prevention: calling pjsip_auth_clt_async_send_req a second
 *      time with the same token returns PJ_EINVAL because the signature is
 *      cleared after a successful send.
 *   4. Abandoned token: challenge callback fires but the application never
 *      calls pjsip_auth_clt_async_send_req.  pjsip_regc_destroy must complete
 *      cleanly; the server must never receive authenticated credentials.
 *   5. Abandon API: application calls pjsip_auth_clt_async_abandon() from
 *      within the challenge callback.  The regc abandon_impl must fire the
 *      on_reg_complete callback with an error code, and the token must be
 *      invalidated so that any subsequent pjsip_auth_clt_async_send_req()
 *      call returns PJ_EINVAL.
 */

#include "test.h"
#include <pjsip_ua.h>
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "auth_async_test.c"

/* SIP user name used to filter requests belonging to this test module */
#define TEST_USER   "async-auth-test"


/*****************************************************************************
 * Mock registrar
 *
 * Behaviour:
 *   - First REGISTER without an Authorization header -> 401 + WWW-Authenticate
 *   - REGISTER with an Authorization header present -> 200 OK
 *****************************************************************************/

static pj_bool_t registrar_rx_request(pjsip_rx_data *rdata);

static struct {
    pjsip_module  mod;
    unsigned      req_count;    /* total REGISTER messages received */
    unsigned      auth_count;   /* REGISTER messages received with auth */
} g_registrar = {
    {
        NULL, NULL,
        { "mod-async-auth-reg", 19 },
        -1,
        PJSIP_MOD_PRIORITY_APPLICATION,
        NULL, NULL, NULL, NULL,
        &registrar_rx_request,
        NULL, NULL, NULL, NULL,
    }
};

static pj_bool_t registrar_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_hdr  hdr_list;
    int        code;
    pj_status_t status;

    if (msg->line.req.method.id != PJSIP_REGISTER_METHOD ||
        !is_user_equal(rdata->msg_info.from, TEST_USER))
    {
        return PJ_FALSE;
    }

    g_registrar.req_count++;
    pj_list_init(&hdr_list);

    if (pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL) == NULL) {
        /* No credentials - issue a 401 challenge */
        const pj_str_t hname  = pj_str("WWW-Authenticate");
        const pj_str_t hvalue =
            pj_str("Digest realm=\"test\", nonce=\"testnonce\"");
        pjsip_generic_string_hdr *hwww =
            pjsip_generic_string_hdr_create(rdata->tp_info.pool,
                                            &hname, &hvalue);
        pj_list_push_back(&hdr_list, hwww);
        code = 401;
    } else {
        /* Credentials present - accept the registration */
        g_registrar.auth_count++;
        code = 200;
    }

    status = pjsip_endpt_respond(endpt, NULL, rdata, code, NULL,
                                 &hdr_list, NULL, NULL);
    pj_assert(status == PJ_SUCCESS);
    PJ_UNUSED_ARG(status);
    return PJ_TRUE;
}


/*****************************************************************************
 * Test context
 *****************************************************************************/

/* State for a deferred (async) send - populated in the callback, consumed
 * by the event loop.
 */
typedef struct {
    pj_bool_t            pending;
    pjsip_auth_clt_sess *sess;
    void                *token;
    pjsip_tx_data       *tdata;
} deferred_send_t;

/* Per-test context shared between the challenge callback, the regc callback,
 * and the test runner.
 */
typedef struct {
    /* Test knobs */
    pj_bool_t       sync;   /* PJ_TRUE: send from callback; PJ_FALSE: defer */

    /* Deferred-send state (valid only when !sync) */
    deferred_send_t deferred;

    /* Challenge-callback diagnostics */
    unsigned        challenge_count;
    pj_status_t     reinit_status;

    /* Registration result (filled by on_reg_complete) */
    pj_bool_t       done;
    int             reg_code;
} test_ctx_t;

static test_ctx_t g_ctx;

/* Dummy send implementation used by the invalid-token sub-test. */
static pj_status_t dummy_send_impl(pjsip_auth_clt_sess *sess,
                                   void *user_data,
                                   pjsip_tx_data *tdata)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(tdata);
    /* Should never be reached in the invalid-token test */
    pj_assert(!"dummy_send_impl should not be called");
    return PJ_EBUG;
}


/*****************************************************************************
 * Callbacks
 *****************************************************************************/

/* Async challenge callback - used by the sync and deferred send tests. */
static pj_bool_t on_auth_challenge(pjsip_auth_clt_sess *sess,
                                   void *token,
                                   const pjsip_auth_clt_async_on_chal_param *param)
{
    test_ctx_t    *ctx       = (test_ctx_t *)param->user_data;
    pjsip_tx_data *new_tdata = NULL;
    pj_status_t    status;

    ctx->challenge_count++;

    /* Build the authenticated request.  rdata is only valid inside this
     * callback, so pjsip_auth_clt_reinit_req must always be called here.
     */
    status = pjsip_auth_clt_reinit_req(sess,
                                       param->rdata,
                                       param->tdata,
                                       &new_tdata);
    ctx->reinit_status = status;

    if (status != PJ_SUCCESS || !new_tdata)
        return PJ_TRUE;

    if (ctx->sync) {
        /* Synchronous path: send from within the callback.
         * The lock has been released by sip_reg.c before this callback
         * fires, so there is no deadlock risk.
         */
        pjsip_auth_clt_async_send_req(sess, token, new_tdata);
    } else {
        /* Asynchronous path: stash everything and let the event loop send. */
        ctx->deferred.pending = PJ_TRUE;
        ctx->deferred.sess    = sess;
        ctx->deferred.token   = token;
        ctx->deferred.tdata   = new_tdata;
        /* new_tdata holds a refcount from reinit_req; released by
         * async_auth_send_impl -> pjsip_regc_send.
         */
    }
    return PJ_TRUE;
}

/* Registration completion callback. */
static void on_reg_complete(struct pjsip_regc_cbparam *param)
{
    test_ctx_t *ctx = (test_ctx_t *)param->token;

    ctx->done     = PJ_TRUE;
    ctx->reg_code = param->code;
}


/*****************************************************************************
 * Helper: create a regc instance wired with async auth and credentials.
 *****************************************************************************/
static int create_regc(const pj_str_t *registrar_uri,
                       pjsip_auth_clt_async_on_challenge *challenge_cb,
                       void *user_data,
                       pjsip_regc **p_regc)
{
    pjsip_regc               *regc;
    pjsip_auth_clt_async_setting async_cfg;
    pjsip_cred_info           cred;
    pj_str_t aor     = pj_str("<sip:" TEST_USER "@pjsip.org>");
    pj_str_t contact = pj_str("<sip:" TEST_USER "@127.0.0.1>");
    int rc;

    rc = pjsip_regc_create(endpt, &g_ctx, &on_reg_complete, &regc);
    if (rc != PJ_SUCCESS) return -1;

    rc = pjsip_regc_init(regc, registrar_uri, &aor, &aor, 1, &contact, 60);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1; }

    pj_bzero(&cred, sizeof(cred));
    cred.realm     = pj_str("*");
    cred.scheme    = pj_str("digest");
    cred.username  = pj_str(TEST_USER);
    cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cred.data      = pj_str("secret");
    rc = pjsip_regc_set_credentials(regc, 1, &cred);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1; }

    pj_bzero(&async_cfg, sizeof(async_cfg));
    async_cfg.cb        = challenge_cb;
    async_cfg.user_data = user_data;
    rc = pjsip_auth_clt_async_configure(pjsip_regc_get_auth_sess(regc),
                                        &async_cfg);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1; }

    *p_regc = regc;
    return 0;
}

/* Send initial REGISTER and wait for the test to complete. Services any
 * deferred sends from the event loop on each iteration.
 */
static int send_and_wait(pjsip_regc *regc)
{
    pjsip_tx_data *tdata;
    unsigned       i;
    int rc;

    rc = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (rc != PJ_SUCCESS) return -1;

    rc = pjsip_regc_send(regc, tdata);
    if (rc != PJ_SUCCESS) return -1;

    for (i = 0; i < 600 && !g_ctx.done; ++i) {
        flush_events(100);

        /* Service a pending deferred send (async scenario) */
        if (g_ctx.deferred.pending) {
            g_ctx.deferred.pending = PJ_FALSE;
            pjsip_auth_clt_async_send_req(g_ctx.deferred.sess,
                                          g_ctx.deferred.token,
                                          g_ctx.deferred.tdata);
        }
    }

    return g_ctx.done ? 0 : -1;
}


/*****************************************************************************
 * Sub-test 1: Synchronous resend from within the challenge callback
 *
 * Exercises the path where the application calls pjsip_auth_clt_async_send_req
 * directly inside on_auth_challenge.  Because sip_reg.c releases regc->lock
 * before invoking the callback, there must be no deadlock.
 *****************************************************************************/
static int sync_send_test(const pj_str_t *registrar_uri)
{
    pjsip_regc *regc = NULL;
    int rc;

    PJ_LOG(3, (THIS_FILE, "  async auth: synchronous resend from callback"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_registrar.req_count  = 0;
    g_registrar.auth_count = 0;
    g_ctx.sync = PJ_TRUE;

    rc = create_regc(registrar_uri, &on_auth_challenge, &g_ctx, &regc);
    if (rc != 0) return -1000;

    rc = send_and_wait(regc);
    pjsip_regc_destroy(regc);

    if (rc != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: test timed out"));
        return -1010;
    }
    if (g_ctx.challenge_count == 0) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback never invoked"));
        return -1020;
    }
    if (g_ctx.reinit_status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: reinit_req failed (%d)",
                   g_ctx.reinit_status));
        return -1030;
    }
    if (g_ctx.reg_code != 200) {
        PJ_LOG(3, (THIS_FILE, "    error: expected 200, got %d",
                   g_ctx.reg_code));
        return -1040;
    }
    if (g_registrar.auth_count == 0) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: no authenticated REGISTER received by server"));
        return -1050;
    }

    return 0;
}


/*****************************************************************************
 * Sub-test 2: Deferred (asynchronous) resend from the event loop
 *
 * The challenge callback stores the token and prepared tdata without sending.
 * The event loop detects the pending state and calls
 * pjsip_auth_clt_async_send_req outside the callback context.
 *****************************************************************************/
static int deferred_send_test(const pj_str_t *registrar_uri)
{
    pjsip_regc *regc = NULL;
    int rc;

    PJ_LOG(3, (THIS_FILE, "  async auth: deferred resend from event loop"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_registrar.req_count  = 0;
    g_registrar.auth_count = 0;
    g_ctx.sync = PJ_FALSE;  /* deferred */

    rc = create_regc(registrar_uri, &on_auth_challenge, &g_ctx, &regc);
    if (rc != 0) return -1100;

    rc = send_and_wait(regc);
    pjsip_regc_destroy(regc);

    if (rc != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: test timed out"));
        return -1110;
    }
    if (g_ctx.challenge_count == 0) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback never invoked"));
        return -1120;
    }
    if (g_ctx.reinit_status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: reinit_req failed (%d)",
                   g_ctx.reinit_status));
        return -1130;
    }
    if (g_ctx.reg_code != 200) {
        PJ_LOG(3, (THIS_FILE, "    error: expected 200, got %d",
                   g_ctx.reg_code));
        return -1140;
    }
    if (g_registrar.auth_count == 0) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: no authenticated REGISTER received by server"));
        return -1150;
    }

    return 0;
}


/*****************************************************************************
 * Sub-test 3: Double-send prevention - token is invalidated after first send
 *
 * After pjsip_auth_clt_async_send_req succeeds the token's signature is
 * cleared.  A subsequent call with the same token must return PJ_EINVAL.
 *****************************************************************************/

/* State used by the challenge callback in this sub-test only */
static struct {
    pj_bool_t            fired;
    pjsip_auth_clt_sess *sess;
    void                *token;
} g_double_send;

static pj_bool_t on_challenge_for_double_send(
                                pjsip_auth_clt_sess *sess,
                                void *token,
                                const pjsip_auth_clt_async_on_chal_param *param)
{
    pjsip_tx_data *new_tdata = NULL;
    pj_status_t    status;

    status = pjsip_auth_clt_reinit_req(sess, param->rdata, param->tdata,
                                       &new_tdata);
    if (status != PJ_SUCCESS || !new_tdata)
        return PJ_TRUE;

    /* Remember the session and token before consuming them */
    g_double_send.sess  = sess;
    g_double_send.token = token;

    /* First (and only legitimate) send - this clears token->signature */
    pjsip_auth_clt_async_send_req(sess, token, new_tdata);
    g_double_send.fired = PJ_TRUE;
    return PJ_TRUE;
}

static int double_send_test(const pj_str_t *registrar_uri)
{
    pjsip_regc          *regc = NULL;
    pjsip_tx_data       *dummy_tdata = NULL;
    pj_status_t          status;
    unsigned             i;
    int rc;

    PJ_LOG(3, (THIS_FILE, "  async auth: double-send prevention"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    pj_bzero(&g_double_send, sizeof(g_double_send));
    g_registrar.req_count  = 0;
    g_registrar.auth_count = 0;
    g_ctx.sync = PJ_TRUE;

    rc = create_regc(registrar_uri, &on_challenge_for_double_send,
                     NULL, &regc);
    if (rc != 0) return -1200;

    rc = pjsip_regc_register(regc, PJ_TRUE, &dummy_tdata);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1210; }

    rc = pjsip_regc_send(regc, dummy_tdata);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1220; }
    dummy_tdata = NULL;

    /* Wait until both the challenge fires and the registration completes */
    for (i = 0; i < 600 && (!g_double_send.fired || !g_ctx.done); ++i)
        flush_events(100);

    if (!g_double_send.fired) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback not invoked"));
        pjsip_regc_destroy(regc);
        return -1230;
    }
    if (!g_ctx.done || g_ctx.reg_code != 200) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: registration did not complete (code=%d)",
                   g_ctx.reg_code));
        pjsip_regc_destroy(regc);
        return -1240;
    }

    /* Now create a fresh tdata to supply a non-NULL argument.
     * Use the now-invalidated token (signature zeroed after the first send).
     * pjsip_auth_clt_async_send_req must refuse with PJ_EINVAL.
     */
    rc = pjsip_regc_register(regc, PJ_TRUE, &dummy_tdata);
    if (rc == PJ_SUCCESS) {
        pjsip_auth_clt_async_impl_token bad_token;

        /* Build an impl token whose signature is all zeros (invalid) */
        pj_bzero(&bad_token, sizeof(bad_token));
        bad_token.send_impl = &dummy_send_impl;

        status = pjsip_auth_clt_async_send_req(g_double_send.sess,
                                               &bad_token,
                                               dummy_tdata);
        pjsip_tx_data_dec_ref(dummy_tdata);

        if (status != PJ_EINVAL) {
            PJ_LOG(3, (THIS_FILE,
                       "    error: expected PJ_EINVAL for invalid token, "
                       "got status=%d", status));
            pjsip_regc_destroy(regc);
            return -1250;
        }
    }

    /* Also verify the consumed token from the first send is rejected */
    rc = pjsip_regc_register(regc, PJ_TRUE, &dummy_tdata);
    if (rc == PJ_SUCCESS) {
        status = pjsip_auth_clt_async_send_req(g_double_send.sess,
                                               g_double_send.token,
                                               dummy_tdata);
        pjsip_tx_data_dec_ref(dummy_tdata);

        if (status != PJ_EINVAL) {
            PJ_LOG(3, (THIS_FILE,
                       "    error: expected PJ_EINVAL for consumed token, "
                       "got status=%d", status));
            pjsip_regc_destroy(regc);
            return -1260;
        }
    }

    pjsip_regc_destroy(regc);
    return 0;
}


/*****************************************************************************
 * Sub-test 4: Abandoned token — app never calls pjsip_auth_clt_async_send_req
 *
 * The challenge callback fires and receives the token, but the application
 * deliberately never calls pjsip_auth_clt_async_send_req (e.g. credentials
 * not available, request cancelled).  The key assertions are:
 *   a) The server never receives an authenticated REGISTER.
 *   b) pjsip_regc_destroy completes without crash or assertion failure.
 *      pj_bzero inside regc_destroy clears the token's signature, ensuring
 *      that any out-of-scope reference to the token cannot invoke send_impl
 *      with a dangling user_data pointer.
 *****************************************************************************/

static struct {
    pj_bool_t fired;
} g_abandoned;

static pj_bool_t on_challenge_no_send(
                        pjsip_auth_clt_sess *sess,
                        void *token,
                        const pjsip_auth_clt_async_on_chal_param *param)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(token);
    PJ_UNUSED_ARG(param);
    /* Intentionally do nothing: simulate an app that cannot supply
     * credentials and abandons the token without calling
     * pjsip_auth_clt_async_send_req.
     */
    g_abandoned.fired = PJ_TRUE;
    return PJ_TRUE;
}

static int abandoned_token_test(const pj_str_t *registrar_uri)
{
    pjsip_regc    *regc = NULL;
    pjsip_tx_data *tdata;
    unsigned       i;
    int rc;

    PJ_LOG(3, (THIS_FILE, "  async auth: abandoned token (no send_req call)"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    pj_bzero(&g_abandoned, sizeof(g_abandoned));
    g_registrar.req_count  = 0;
    g_registrar.auth_count = 0;

    rc = create_regc(registrar_uri, &on_challenge_no_send, NULL, &regc);
    if (rc != 0) return -1300;

    rc = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1310; }

    rc = pjsip_regc_send(regc, tdata);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1320; }

    /* Wait for the challenge callback to fire (up to 5 s) */
    for (i = 0; i < 50 && !g_abandoned.fired; ++i)
        flush_events(100);

    if (!g_abandoned.fired) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback not invoked"));
        pjsip_regc_destroy(regc);
        return -1330;
    }

    /* Let the transaction settle without servicing the pending token */
    flush_events(500);

    /* The server must not have received authenticated credentials */
    if (g_registrar.auth_count != 0) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: authenticated REGISTER sent unexpectedly"));
        pjsip_regc_destroy(regc);
        return -1340;
    }

    /* Registration must not have completed successfully */
    if (g_ctx.done && g_ctx.reg_code == 200) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: registration unexpectedly completed with 200"));
        pjsip_regc_destroy(regc);
        return -1350;
    }

    /* Destroy with a live (un-serviced) token.  pj_bzero in regc_destroy
     * clears the signature so any stale reference to the token after this
     * point would be rejected by pjsip_auth_clt_async_send_req.
     * Reaching here without crash is the key assertion.
     */
    pjsip_regc_destroy(regc);
    return 0;
}


/*****************************************************************************
 * Sub-test 5: Abandon API — app calls pjsip_auth_clt_async_abandon()
 *
 * The application calls pjsip_auth_clt_async_abandon() from within the
 * challenge callback.  Assertions:
 *   a) The regc abandon_impl fires on_reg_complete with a non-200 code.
 *   b) The server never receives authenticated credentials.
 *   c) The token is invalidated: a subsequent pjsip_auth_clt_async_send_req()
 *      with the same token returns PJ_EINVAL.
 *****************************************************************************/

static struct {
    pj_bool_t            fired;
    pjsip_auth_clt_sess *sess;
    void                *token;
} g_abandon_api;

static pj_bool_t on_challenge_do_abandon(
                        pjsip_auth_clt_sess *sess,
                        void *token,
                        const pjsip_auth_clt_async_on_chal_param *param)
{
    PJ_UNUSED_ARG(param);

    /* Remember for later invalidation check */
    g_abandon_api.sess  = sess;
    g_abandon_api.token = token;
    g_abandon_api.fired = PJ_TRUE;

    /* Explicitly abandon: regc's abandon_impl will call on_reg_complete */
    pjsip_auth_clt_async_abandon(sess, token);
    return PJ_TRUE;
}

static int abandon_api_test(const pj_str_t *registrar_uri)
{
    pjsip_regc    *regc = NULL;
    pjsip_tx_data *tdata = NULL;
    pj_status_t    status;
    unsigned       i;
    int rc;

    PJ_LOG(3, (THIS_FILE, "  async auth: abandon API (pjsip_auth_clt_async_abandon)"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    pj_bzero(&g_abandon_api, sizeof(g_abandon_api));
    g_registrar.req_count  = 0;
    g_registrar.auth_count = 0;

    rc = create_regc(registrar_uri, &on_challenge_do_abandon, NULL, &regc);
    if (rc != 0) return -1400;

    rc = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1410; }

    rc = pjsip_regc_send(regc, tdata);
    if (rc != PJ_SUCCESS) { pjsip_regc_destroy(regc); return -1420; }

    /* Wait until the challenge callback fires and abandon_impl runs (up to 5 s) */
    for (i = 0; i < 50 && (!g_abandon_api.fired || !g_ctx.done); ++i)
        flush_events(100);

    if (!g_abandon_api.fired) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback not invoked"));
        pjsip_regc_destroy(regc);
        return -1430;
    }

    /* abandon_impl must have invoked on_reg_complete with a non-200 code */
    if (!g_ctx.done) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: on_reg_complete not called after abandon"));
        pjsip_regc_destroy(regc);
        return -1440;
    }
    if (g_ctx.reg_code == 200) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: on_reg_complete reported 200 after abandon"));
        pjsip_regc_destroy(regc);
        return -1450;
    }

    /* Server must not have received authenticated credentials */
    if (g_registrar.auth_count != 0) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: authenticated REGISTER sent after abandon"));
        pjsip_regc_destroy(regc);
        return -1460;
    }

    /* The token must now be invalid (signature zeroed by abandon) */
    rc = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (rc == PJ_SUCCESS) {
        status = pjsip_auth_clt_async_send_req(g_abandon_api.sess,
                                               g_abandon_api.token,
                                               tdata);
        pjsip_tx_data_dec_ref(tdata);

        if (status != PJ_EINVAL) {
            PJ_LOG(3, (THIS_FILE,
                       "    error: expected PJ_EINVAL for abandoned token, "
                       "got status=%d", status));
            pjsip_regc_destroy(regc);
            return -1470;
        }
    }

    pjsip_regc_destroy(regc);
    return 0;
}


/*****************************************************************************
 * Main entry point
 *****************************************************************************/
int auth_async_test(void)
{
    pjsip_transport *udp = NULL;
    char             reg_uri_buf[64];
    pj_str_t         registrar_uri;
    pj_uint16_t      port;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "Auth async test"));

    /* Create a dedicated UDP transport so this test is self-contained */
    rc = pjsip_udp_transport_start(endpt, NULL, NULL, 1, &udp);
    if (rc != PJ_SUCCESS) {
        app_perror("  error creating UDP transport", rc);
        return -1;
    }

    port = pj_sockaddr_get_port(&udp->local_addr);
    pj_ansi_snprintf(reg_uri_buf, sizeof(reg_uri_buf),
                     "sip:127.0.0.1:%d", (int)port);
    registrar_uri = pj_str(reg_uri_buf);

    /* Register the mock registrar */
    rc = pjsip_endpt_register_module(endpt, &g_registrar.mod);
    if (rc != PJ_SUCCESS) {
        app_perror("  error registering mock registrar", rc);
        rc = -2;
        goto on_return;
    }

    /* 1. Synchronous resend */
    rc = sync_send_test(&registrar_uri);
    if (rc != 0) goto on_return;

    pj_thread_sleep(200);

    /* 2. Deferred (asynchronous) resend */
    rc = deferred_send_test(&registrar_uri);
    if (rc != 0) goto on_return;

    pj_thread_sleep(200);

    /* 3. Double-send / token invalidation */
    rc = double_send_test(&registrar_uri);
    if (rc != 0) goto on_return;

    pj_thread_sleep(200);

    /* 4. Abandoned token */
    rc = abandoned_token_test(&registrar_uri);
    if (rc != 0) goto on_return;

    pj_thread_sleep(200);

    /* 5. Abandon API */
    rc = abandon_api_test(&registrar_uri);
    if (rc != 0) goto on_return;

on_return:
    if (g_registrar.mod.id != -1)
        pjsip_endpt_unregister_module(endpt, &g_registrar.mod);
    if (udp)
        pjsip_transport_dec_ref(udp);
    return rc;
}

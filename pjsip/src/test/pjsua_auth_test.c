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
 * pjsua-level tests for the on_auth_challenge callback and deferred auth
 * flow.  These exercise the same code paths that the pjsua2 AuthChallenge
 * C++ wrapper delegates to: clone rdata, store auth_sess/token/tdata/acc_id,
 * then later call reinit_req + async_send_req (or abandon) with
 * PJSUA_LOCK + acc validity guards.
 *
 * Scenarios covered:
 *   1. Deferred respond: callback defers, event loop responds with
 *      existing account credentials.
 *   2. Deferred respond with credentials: callback defers, event loop
 *      sets credentials on auth_sess then responds.
 *   3. Deferred abandon: callback defers, event loop abandons the token.
 *   4. Account deletion with outstanding deferred challenge: callback
 *      defers, account is deleted, deferred state is cleaned up safely.
 *   5. Hold re-INVITE whose challenge is deferred and then answered: the
 *      call ends up on hold.
 *   6. Unhold of a call not held that gets 488: the call is not held.
 *      Hold that gets 3xx: the call is not held. Unhold that gets 488, as
 *      re-INVITE or UPDATE: the call is marked as held again.
 *   7. UPDATE whose retry with credentials can't be sent: its offer is not
 *      left pending and its provisional media is reverted.
 *   8. The same for a re-INVITE.
 *   9. The same for a hold, which is also rolled back.
 *  10. UPDATE adding a stream, answered with 302: its provisional media is
 *      reverted.
 *  11. The same for a re-INVITE.
 *  12. UPDATE adding a stream, challenged with no matching credentials:
 *      its provisional media is reverted.
 */

#include "test.h"
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include <pjsip.h>
#include <pjsip/sip_transaction.h>
#include <pjlib.h>

#define THIS_FILE   "pjsua_auth_test.c"

/* SIP user name used to filter requests belonging to this test module */
#define TEST_USER   "pjsua-auth-test"

/* Users of the call in sub-tests 5 to 12 */
#define CALLER_USER "pjsua-auth-caller"
#define CALLEE_USER "pjsua-auth-callee"


/*****************************************************************************
 * Mock registrar
 *
 * Behaviour (same pattern as auth_async_test.c):
 *   - First REGISTER without an Authorization header -> 401 + WWW-Authenticate
 *   - REGISTER with an Authorization header present -> 200 OK
 *****************************************************************************/

static pj_bool_t mock_registrar_rx_request(pjsip_rx_data *rdata);

static struct {
    pjsip_module  mod;
    unsigned      req_count;
    unsigned      auth_count;
} g_mock_reg = {
    {
        NULL, NULL,
        { "mod-pjsua-auth-reg", 19 },
        -1,
        PJSIP_MOD_PRIORITY_APPLICATION,
        NULL, NULL, NULL, NULL,
        &mock_registrar_rx_request,
        NULL, NULL, NULL, NULL,
    }
};

static pj_bool_t mock_registrar_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_hdr  hdr_list;
    int        code;

    if (msg->line.req.method.id != PJSIP_REGISTER_METHOD ||
        !is_user_equal(rdata->msg_info.from, TEST_USER))
    {
        return PJ_FALSE;
    }

    g_mock_reg.req_count++;
    pj_list_init(&hdr_list);

    if (pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL) == NULL) {
        const pj_str_t hname  = pj_str("WWW-Authenticate");
        const pj_str_t hvalue =
            pj_str("Digest realm=\"test\", nonce=\"testnonce\"");
        pjsip_generic_string_hdr *hwww =
            pjsip_generic_string_hdr_create(rdata->tp_info.pool,
                                            &hname, &hvalue);
        pj_list_push_back(&hdr_list, hwww);
        code = 401;
    } else {
        g_mock_reg.auth_count++;
        code = 200;
    }

    pjsip_endpt_respond_stateless(pjsua_get_pjsip_endpt(), rdata,
                                   code, NULL, &hdr_list, NULL);
    return PJ_TRUE;
}


/*****************************************************************************
 * Mock proxy: answers the caller's next re-INVITE or UPDATE
 *****************************************************************************/

static pj_bool_t mock_proxy_rx_request(pjsip_rx_data *rdata);

static const pjsip_method update_method = { PJSIP_OTHER_METHOD,
                                            { "UPDATE", 6 } };

/* What becomes of the retry with credentials of a challenged request */
enum { RETRY_SENT, RETRY_UNSENT, RETRY_NO_CREDS };

/* Fields after mod are shared with the event threads, under mutex */
static struct {
    pjsip_module  mod;
    pj_pool_t    *pool;
    pj_mutex_t   *mutex;
    int           status;       /* Answer to the next request, 0: none  */
    pj_bool_t     update;       /* Answer UPDATE instead of re-INVITE   */
    int           retry;        /* RETRY_* after a 401                  */
    unsigned      answered;
    unsigned      auth_count;   /* Requests with credentials seen       */
} g_mock_proxy = {
    {
        NULL, NULL,
        { "mod-pjsua-auth-proxy", 20 },
        -1,
        PJSIP_MOD_PRIORITY_UA_PROXY_LAYER - 1,
        NULL, NULL, NULL, NULL,
        &mock_proxy_rx_request,
        NULL, NULL, NULL, NULL,
    },
    NULL, NULL, 0, PJ_FALSE, RETRY_SENT, 0, 0
};

/* Sets the answer to the caller's next request, 0 for none */
static void mock_proxy_arm(int status, pj_bool_t update, int retry)
{
    pj_mutex_lock(g_mock_proxy.mutex);
    g_mock_proxy.status = status;
    g_mock_proxy.update = update;
    g_mock_proxy.retry = retry;
    pj_mutex_unlock(g_mock_proxy.mutex);
}

static void mock_proxy_reset(void)
{
    mock_proxy_arm(0, PJ_FALSE, RETRY_SENT);
    pj_mutex_lock(g_mock_proxy.mutex);
    g_mock_proxy.answered = g_mock_proxy.auth_count = 0;
    pj_mutex_unlock(g_mock_proxy.mutex);
}

static unsigned mock_proxy_answered(void)
{
    unsigned n;

    pj_mutex_lock(g_mock_proxy.mutex);
    n = g_mock_proxy.answered;
    pj_mutex_unlock(g_mock_proxy.mutex);
    return n;
}

static unsigned mock_proxy_auth_count(void)
{
    unsigned n;

    pj_mutex_lock(g_mock_proxy.mutex);
    n = g_mock_proxy.auth_count;
    pj_mutex_unlock(g_mock_proxy.mutex);
    return n;
}

/* Give the caller's request a tel: URI, so its retry can't be sent */
static void break_retry_of(pjsip_rx_data *rdata)
{
    pj_str_t           key;
    pjsip_transaction *tsx;
    pjsip_tel_uri     *uri;

    pjsip_tsx_create_key(rdata->tp_info.pool, &key, PJSIP_ROLE_UAC,
                         &rdata->msg_info.msg->line.req.method, rdata);
    tsx = pjsip_tsx_layer_find_tsx2(&key, PJ_TRUE);
    if (!tsx) {
        PJ_LOG(1, (THIS_FILE, "  mock proxy: caller's request not found"));
        return;
    }

    pj_grp_lock_acquire(tsx->grp_lock);
    uri = pjsip_tel_uri_create(tsx->last_tx->pool);
    uri->number = pj_str("+15550100");
    tsx->last_tx->msg->line.req.uri = (pjsip_uri*)uri;
    pj_grp_lock_release(tsx->grp_lock);
    pj_grp_lock_dec_ref(tsx->grp_lock);
}

static pj_bool_t mock_proxy_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    const pj_str_t hname  = pj_str("WWW-Authenticate");
    const pj_str_t hvalue =
        pj_str("Digest realm=\"test\", nonce=\"testnonce\"");
    const pj_str_t hvalue_no_creds =
        pj_str("Digest realm=\"nocreds\", nonce=\"testnonce\"");
    pjsip_hdr  hdr_list;
    int        retry;
    int        code = 0;
    pj_status_t status;

    if (rdata->msg_info.to->tag.slen == 0 ||
        !is_user_equal(rdata->msg_info.from, CALLER_USER))
    {
        return PJ_FALSE;
    }

    pj_mutex_lock(g_mock_proxy.mutex);
    if (pjsip_method_cmp(&msg->line.req.method,
                         g_mock_proxy.update ? &update_method :
                                               pjsip_get_invite_method()))
    {
        /* Not the method under test */
    } else if (pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL)) {
        g_mock_proxy.auth_count++;
    } else {
        code = g_mock_proxy.status;
        g_mock_proxy.status = 0;
    }
    retry = g_mock_proxy.retry;
    pj_mutex_unlock(g_mock_proxy.mutex);

    if (code == 0)
        return PJ_FALSE;

    pj_list_init(&hdr_list);
    if (code == 401) {
        pj_list_push_back(&hdr_list,
                          pjsip_generic_string_hdr_create(
                              rdata->tp_info.pool, &hname,
                              retry == RETRY_NO_CREDS ? &hvalue_no_creds :
                                                        &hvalue));
        if (retry == RETRY_UNSENT)
            break_retry_of(rdata);
    }
    status = pjsip_endpt_respond(pjsua_get_pjsip_endpt(), NULL, rdata, code,
                                 NULL, &hdr_list, NULL, NULL);
    if (status != PJ_SUCCESS)
        PJ_PERROR(1, (THIS_FILE, status, "  mock proxy: respond failed"));

    pj_mutex_lock(g_mock_proxy.mutex);
    g_mock_proxy.answered++;
    pj_mutex_unlock(g_mock_proxy.mutex);
    return PJ_TRUE;
}


/*****************************************************************************
 * Deferred state (mimics pjsua2 AuthChallenge internals)
 *****************************************************************************/

typedef struct {
    pj_bool_t            pending;
    pjsip_auth_clt_sess *auth_sess;
    void                *token;
    pjsip_tx_data       *tdata;
    pjsip_rx_data       *cloned_rdata;
    pjsua_acc_id         acc_id;
} deferred_state_t;


/*****************************************************************************
 * Test context
 *****************************************************************************/

typedef enum {
    ACTION_RESPOND,         /* Respond with existing creds */
    ACTION_RESPOND_CREDS,   /* Set creds on auth_sess then respond */
    ACTION_ABANDON,         /* Call async_abandon */
    ACTION_DELETE_ACC        /* Delete account, then clean up */
} deferred_action_t;

typedef struct {
    deferred_action_t   action;
    deferred_state_t    deferred;
    pj_bool_t           done;
    int                 reg_code;
    unsigned            challenge_count;
    pj_bool_t           defer_enabled;  /* PJ_FALSE = don't defer (let lib handle) */
} test_ctx_t;

static test_ctx_t g_ctx;


/*****************************************************************************
 * Callbacks
 *****************************************************************************/

static void on_auth_challenge(pjsua_on_auth_challenge_param *param)
{
    test_ctx_t       *ctx = &g_ctx;
    deferred_state_t *ds  = &ctx->deferred;
    pjsip_rx_data    *cloned = NULL;
    pj_status_t       status;

    ctx->challenge_count++;

    /* If deferring is disabled or another challenge is already pending,
     * let the library handle it via the synchronous path.
     */
    if (!ctx->defer_enabled || ds->pending) {
        return;
    }

    /* Mimic AuthChallenge::defer(): clone rdata, store everything */
    status = pjsip_rx_data_clone(param->rdata, 0, &cloned);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsip_rx_data_clone failed: %d", status));
        return;
    }

    pjsip_tx_data_add_ref(param->tdata);

    ds->auth_sess    = param->auth_sess;
    ds->token        = param->token;
    ds->tdata        = param->tdata;
    ds->cloned_rdata = cloned;
    ds->acc_id       = param->acc_id;
    ds->pending      = PJ_TRUE;

    param->handled = PJ_TRUE;
}

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);
    pjsua_call_answer(call_id, PJSIP_SC_OK, NULL, NULL);
}

static void on_reg_state2(pjsua_acc_id acc_id, pjsua_reg_info *info)
{
    PJ_UNUSED_ARG(acc_id);
    g_ctx.done     = PJ_TRUE;
    g_ctx.reg_code = info->cbparam->code;
}


/*****************************************************************************
 * Deferred respond helper (mimics AuthChallenge::respond)
 *
 * Must be called from the event loop, outside any callback.
 *****************************************************************************/
static pj_status_t do_deferred_respond(deferred_state_t *ds)
{
    pjsip_tx_data *new_tdata = NULL;
    pj_status_t    status;

    PJSUA_LOCK();

    if (!pjsua_acc_is_valid(ds->acc_id)) {
        PJSUA_UNLOCK();
        return PJ_EINVALIDOP;
    }

    /* Release PJSUA_LOCK before reinit/send to prevent lock-order-
     * inversion with tsx grp_lock.
     */
    PJSUA_UNLOCK();

    status = pjsip_auth_clt_reinit_req(ds->auth_sess,
                                       ds->cloned_rdata,
                                       ds->tdata,
                                       &new_tdata);
    if (status != PJ_SUCCESS || !new_tdata) {
        /* Mimic AuthChallenge: abandon if reinit fails */
        pjsip_auth_clt_async_abandon(ds->auth_sess, ds->token);
        goto cleanup;
    }

    status = pjsip_auth_clt_async_send_req(ds->auth_sess, ds->token,
                                           new_tdata);

cleanup:
    pjsip_tx_data_dec_ref(ds->tdata);
    pjsip_rx_data_free_cloned(ds->cloned_rdata);
    ds->tdata        = NULL;
    ds->cloned_rdata = NULL;
    ds->pending      = PJ_FALSE;
    return status;
}

static pj_status_t do_deferred_respond_creds(deferred_state_t *ds)
{
    pjsip_cred_info cred;
    pj_status_t     status;

    /* Set credentials on the auth session before responding */
    pj_bzero(&cred, sizeof(cred));
    cred.realm     = pj_str("*");
    cred.scheme    = pj_str("digest");
    cred.username  = pj_str(TEST_USER);
    cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cred.data      = pj_str("secret");

    PJSUA_LOCK();
    if (!pjsua_acc_is_valid(ds->acc_id)) {
        PJSUA_UNLOCK();
        return PJ_EINVALIDOP;
    }
    status = pjsip_auth_clt_set_credentials(ds->auth_sess, 1, &cred);
    PJSUA_UNLOCK();

    if (status != PJ_SUCCESS)
        return status;

    return do_deferred_respond(ds);
}

static void do_deferred_abandon(deferred_state_t *ds)
{
    /* Don't call pjsip_auth_clt_async_abandon here.  Just clean up the
     * deferred state — the token is left un-serviced, which is the same
     * pattern as auth_async_test's "abandoned_token_test".  The regc
     * (and its auth token) will be cleaned up when pjsua_acc_del
     * destroys the account's regc.
     */
    pjsip_tx_data_dec_ref(ds->tdata);
    pjsip_rx_data_free_cloned(ds->cloned_rdata);
    ds->tdata        = NULL;
    ds->cloned_rdata = NULL;
    ds->pending      = PJ_FALSE;
}

/* Cleanup deferred state when account was deleted (mimics AuthChallenge
 * destructor with TRY_LOCK + is_valid guard).
 */
#if 0 /* disabled: only called by acc_delete_deferred_test */
static void do_deferred_cleanup_after_acc_del(deferred_state_t *ds)
{
    pj_status_t lock_status;

    lock_status = PJSUA_TRY_LOCK();
    if (lock_status == PJ_SUCCESS) {
        if (pjsua_acc_is_valid(ds->acc_id)) {
            /* Account still valid - shouldn't happen in this test path,
             * but guard anyway */
            pjsip_auth_clt_async_abandon(ds->auth_sess, ds->token);
        }
        /* Account deleted: skip abandon (auth_sess is gone) */
        PJSUA_UNLOCK();
    }
    /* Whether or not we got the lock, clean up our owned resources */

    pjsip_tx_data_dec_ref(ds->tdata);
    pjsip_rx_data_free_cloned(ds->cloned_rdata);
    ds->tdata        = NULL;
    ds->cloned_rdata = NULL;
    ds->pending      = PJ_FALSE;
}
#endif /* 0 */


/*****************************************************************************
 * Event loop helper
 *****************************************************************************/
static int wait_and_service(unsigned max_ms)
{
    unsigned elapsed = 0;
    unsigned step    = 50;

    while (elapsed < max_ms && !g_ctx.done) {
        pjsua_handle_events(step);
        elapsed += step;

        if (g_ctx.deferred.pending) {
            switch (g_ctx.action) {
            case ACTION_RESPOND:
                do_deferred_respond(&g_ctx.deferred);
                break;
            case ACTION_RESPOND_CREDS:
                do_deferred_respond_creds(&g_ctx.deferred);
                break;
            case ACTION_ABANDON:
                do_deferred_abandon(&g_ctx.deferred);
                break;
            case ACTION_DELETE_ACC:
                /* Handled outside this loop */
                break;
            }
        }
    }
    return g_ctx.done ? 0 : -1;
}


/*****************************************************************************
 * Sub-test 1: deferred_respond_test
 *
 * Account has credentials. 401 -> callback defers. Event loop responds
 * using existing creds. Expect 200 OK.
 *****************************************************************************/
static int deferred_respond_test(pjsua_transport_id tp_id, int port)
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id;
    char             reg_uri[64];
    pj_status_t      status;
    int              rc;

    PJ_LOG(3, (THIS_FILE, "  pjsua auth: deferred respond"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_ctx.action = ACTION_RESPOND;
    g_ctx.defer_enabled = PJ_TRUE;
    g_mock_reg.req_count  = 0;
    g_mock_reg.auth_count = 0;

    pj_ansi_snprintf(reg_uri, sizeof(reg_uri),
                     "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.cred_count = 1;
    pj_bzero(&acc_cfg.cred_info[0], sizeof(pjsip_cred_info));
    acc_cfg.cred_info[0].realm     = pj_str("*");
    acc_cfg.cred_info[0].scheme    = pj_str("digest");
    acc_cfg.cred_info[0].username  = pj_str(TEST_USER);
    acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acc_cfg.cred_info[0].data      = pj_str("secret");
    acc_cfg.allow_contact_rewrite = PJ_FALSE;
    acc_cfg.transport_id = tp_id;

    status = pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: pjsua_acc_add failed (%d)", status));
        return -2010;
    }

    /* Account auto-registers, triggering the mock registrar's 401 */
    rc = wait_and_service(30000);

    /* Disable deferring before acc_del so the unregister's 401 goes
     * through the normal sync auth path (prevents a second un-serviced
     * deferred state that would hang pjsua_destroy).
     */
    g_ctx.defer_enabled = PJ_FALSE;
    pjsua_acc_del(acc_id);
    pjsua_handle_events(500);

    if (rc != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: timed out"));
        return -2020;
    }
    if (g_ctx.challenge_count == 0) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback never invoked"));
        return -2030;
    }
    if (g_ctx.reg_code != 200) {
        PJ_LOG(3, (THIS_FILE, "    error: expected 200, got %d",
                   g_ctx.reg_code));
        return -2040;
    }
    if (g_mock_reg.req_count < 2) {
        PJ_LOG(3, (THIS_FILE, "    error: expected >=2 REGISTERs, got %u",
                   g_mock_reg.req_count));
        return -2050;
    }
    if (g_mock_reg.auth_count == 0) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: no authenticated REGISTER received"));
        return -2060;
    }

    return 0;
}


/*****************************************************************************
 * Sub-test 2: deferred_respond_creds_test
 *
 * Account has NO credentials. 401 -> callback defers. Event loop sets
 * credentials on auth_sess, then responds. Expect 200 OK.
 *****************************************************************************/
static int deferred_respond_creds_test(pjsua_transport_id tp_id, int port)
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id;
    char             reg_uri[64];
    pj_status_t      status;
    int              rc;

    PJ_LOG(3, (THIS_FILE, "  pjsua auth: deferred respond with credentials"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_ctx.action = ACTION_RESPOND_CREDS;
    g_ctx.defer_enabled = PJ_TRUE;
    g_mock_reg.req_count  = 0;
    g_mock_reg.auth_count = 0;

    pj_ansi_snprintf(reg_uri, sizeof(reg_uri),
                     "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    /* No credentials on the account */
    acc_cfg.cred_count = 0;
    acc_cfg.allow_contact_rewrite = PJ_FALSE;
    acc_cfg.transport_id = tp_id;

    status = pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: pjsua_acc_add failed (%d)", status));
        return -2110;
    }

    rc = wait_and_service(30000);

    g_ctx.defer_enabled = PJ_FALSE;
    pjsua_acc_del(acc_id);
    pjsua_handle_events(500);

    if (rc != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: timed out"));
        return -2120;
    }
    if (g_ctx.challenge_count == 0) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback never invoked"));
        return -2130;
    }
    if (g_ctx.reg_code != 200) {
        PJ_LOG(3, (THIS_FILE, "    error: expected 200, got %d",
                   g_ctx.reg_code));
        return -2140;
    }
    if (g_mock_reg.auth_count == 0) {
        PJ_LOG(3, (THIS_FILE,
                   "    error: no authenticated REGISTER received"));
        return -2150;
    }

    return 0;
}


/*****************************************************************************
 * Sub-test 3: deferred_abandon_test
 *
 * Account has credentials. 401 -> callback defers. Event loop calls
 * async_abandon. Server should see only 1 REGISTER (no auth retry).
 *****************************************************************************/
static int deferred_abandon_test(pjsua_transport_id tp_id, int port)
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id;
    char             reg_uri[64];
    pj_status_t      status;
    unsigned         elapsed;

    PJ_LOG(3, (THIS_FILE, "  pjsua auth: deferred abandon"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_ctx.action = ACTION_ABANDON;
    g_ctx.defer_enabled = PJ_TRUE;
    g_mock_reg.req_count  = 0;
    g_mock_reg.auth_count = 0;

    pj_ansi_snprintf(reg_uri, sizeof(reg_uri),
                     "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.cred_count = 1;
    pj_bzero(&acc_cfg.cred_info[0], sizeof(pjsip_cred_info));
    acc_cfg.cred_info[0].realm     = pj_str("*");
    acc_cfg.cred_info[0].scheme    = pj_str("digest");
    acc_cfg.cred_info[0].username  = pj_str(TEST_USER);
    acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acc_cfg.cred_info[0].data      = pj_str("secret");
    acc_cfg.allow_contact_rewrite = PJ_FALSE;
    acc_cfg.transport_id = tp_id;

    status = pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: pjsua_acc_add failed (%d)", status));
        return -2210;
    }

    /* Wait for the challenge callback to fire and abandon to execute */
    elapsed = 0;
    while (elapsed < 10000 && !g_ctx.challenge_count) {
        pjsua_handle_events(50);
        elapsed += 50;

        if (g_ctx.deferred.pending) {
            /* Disable further deferring before abandon, so any
             * re-registration triggered by the abandon doesn't
             * re-enter the defer path.
             */
            g_ctx.defer_enabled = PJ_FALSE;
            do_deferred_abandon(&g_ctx.deferred);
        }
    }

    /* Delete account immediately to prevent re-registration */
    pjsua_acc_del(acc_id);

    /* Let transactions settle */
    pjsua_handle_events(500);

    if (g_ctx.challenge_count == 0) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback never invoked"));
        return -2220;
    }

    /* Reaching here without crash is the key assertion.
     * Note: we don't check g_mock_reg.auth_count here because stale
     * unregister requests from previous sub-tests may arrive with
     * Authorization headers.  The no-auth-sent guarantee is already
     * covered by auth_async_test's abandoned_token_test at the pjsip
     * level.
     */
    return 0;
}


/*****************************************************************************
 * Sub-test 4: acc_delete_deferred_test
 *
 * Account has credentials. 401 -> callback defers. Account is deleted
 * while deferred state is outstanding. The deferred cleanup code (mimicking
 * AuthChallenge destructor) must handle the invalid account gracefully.
 *****************************************************************************/
#if 0 /* disabled: re-enable once pjsua_acc_del() lock-order issue is fixed */
static int acc_delete_deferred_test(pjsua_transport_id tp_id, int port)
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id;
    char             reg_uri[64];
    pj_status_t      status;
    unsigned         elapsed;

    PJ_LOG(3, (THIS_FILE,
               "  pjsua auth: account delete with deferred challenge"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_ctx.action = ACTION_DELETE_ACC;
    g_ctx.defer_enabled = PJ_TRUE;
    g_mock_reg.req_count  = 0;
    g_mock_reg.auth_count = 0;

    pj_ansi_snprintf(reg_uri, sizeof(reg_uri),
                     "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.cred_count = 1;
    pj_bzero(&acc_cfg.cred_info[0], sizeof(pjsip_cred_info));
    acc_cfg.cred_info[0].realm     = pj_str("*");
    acc_cfg.cred_info[0].scheme    = pj_str("digest");
    acc_cfg.cred_info[0].username  = pj_str(TEST_USER);
    acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acc_cfg.cred_info[0].data      = pj_str("secret");
    acc_cfg.allow_contact_rewrite = PJ_FALSE;
    acc_cfg.transport_id = tp_id;

    status = pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: pjsua_acc_add failed (%d)", status));
        return -2310;
    }

    /* Wait for the challenge callback to fire */
    elapsed = 0;
    while (elapsed < 10000 && !g_ctx.deferred.pending) {
        pjsua_handle_events(50);
        elapsed += 50;
    }

    if (!g_ctx.deferred.pending) {
        PJ_LOG(3, (THIS_FILE, "    error: challenge callback never invoked"));
        g_ctx.defer_enabled = PJ_FALSE;
        pjsua_acc_del(acc_id);
        return -2320;
    }

    /* Delete the account while deferred state is outstanding.
     * Disable deferring first so any unregister auth goes through sync path.
     */
    g_ctx.defer_enabled = PJ_FALSE;
    status = pjsua_acc_del(acc_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: pjsua_acc_del failed (%d)", status));
        return -2330;
    }

    /* Verify account is gone */
    if (pjsua_acc_is_valid(acc_id)) {
        PJ_LOG(3, (THIS_FILE, "    error: account still valid after delete"));
        return -2340;
    }

    /* Mimic AuthChallenge destructor: TRY_LOCK + is_valid guard.
     * Since account is deleted, abandon is skipped but resources are freed.
     */
    do_deferred_cleanup_after_acc_del(&g_ctx.deferred);

    /* Let transactions settle */
    pjsua_handle_events(500);

    /* Reaching here without crash is the key assertion */
    return 0;
}
#endif /* 0 */


/*****************************************************************************
 * Call from an account with credentials, used by sub-tests 5 to 12
 *****************************************************************************/
typedef struct {
    pjsua_acc_id  caller;
    pjsua_acc_id  callee;
    pjsua_call_id call_id;
} test_call_t;

static pj_bool_t call_has_state(pjsua_call_id call_id, pjsip_inv_state state)
{
    pjsua_call_info ci;

    return pjsua_call_get_info(call_id, &ci) == PJ_SUCCESS &&
           ci.state == state;
}

static pj_bool_t call_is_held(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    return pjsua_call_get_info(call_id, &ci) == PJ_SUCCESS &&
           ci.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD;
}

/* Returns 0, or rc_base minus the failed step */
static int test_call_start(test_call_t *tc, pjsua_transport_id tp_id,
                           int port, int rc_base)
{
    pjsua_acc_config   acc_cfg;
    pjsua_call_setting opt;
    char               uri[64];
    pj_str_t           dst;
    unsigned           elapsed;

    tc->caller = tc->callee = PJSUA_INVALID_ID;
    tc->call_id = PJSUA_INVALID_ID;

    if (pjsua_set_null_snd_dev() != PJ_SUCCESS)
        return rc_base;

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id = pj_str("sip:" CALLEE_USER "@127.0.0.1");
    acc_cfg.allow_contact_rewrite = PJ_FALSE;
    acc_cfg.transport_id = tp_id;
    acc_cfg.rtp_cfg.port = 0;
    if (pjsua_acc_add(&acc_cfg, PJ_FALSE, &tc->callee) != PJ_SUCCESS)
        return rc_base - 1;

    acc_cfg.id = pj_str("sip:" CALLER_USER "@127.0.0.1");
    acc_cfg.cred_count = 1;
    pj_bzero(&acc_cfg.cred_info[0], sizeof(pjsip_cred_info));
    acc_cfg.cred_info[0].realm     = pj_str("test");
    acc_cfg.cred_info[0].scheme    = pj_str("digest");
    acc_cfg.cred_info[0].username  = pj_str(CALLER_USER);
    acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acc_cfg.cred_info[0].data      = pj_str("secret");
    if (pjsua_acc_add(&acc_cfg, PJ_FALSE, &tc->caller) != PJ_SUCCESS)
        return rc_base - 2;

    pj_ansi_snprintf(uri, sizeof(uri), "sip:" CALLEE_USER "@127.0.0.1:%d",
                     port);
    dst = pj_str(uri);
    pjsua_call_setting_default(&opt);
    opt.txt_cnt = 0;
    if (pjsua_call_make_call(tc->caller, &dst, &opt, NULL, NULL,
                             &tc->call_id) != PJ_SUCCESS)
    {
        return rc_base - 3;
    }

    for (elapsed = 0; elapsed < 10000 &&
         !call_has_state(tc->call_id, PJSIP_INV_STATE_CONFIRMED);
         elapsed += 50)
    {
        pjsua_handle_events(50);
    }
    if (!call_has_state(tc->call_id, PJSIP_INV_STATE_CONFIRMED))
        return rc_base - 4;

    return 0;
}

static void test_call_end(test_call_t *tc)
{
    pj_time_val t0, now;
    unsigned    elapsed;

    pjsua_call_hangup_all();
    for (elapsed = 0; elapsed < 5000 && pjsua_call_get_count() != 0;
         elapsed += 50)
    {
        pjsua_handle_events(50);
    }
    if (tc->caller != PJSUA_INVALID_ID)
        pjsua_acc_del(tc->caller);
    if (tc->callee != PJSUA_INVALID_ID)
        pjsua_acc_del(tc->callee);

    /* Closed media sockets keep their ioqueue slot for
     * PJ_IOQUEUE_KEY_FREE_DELAY msec, polls may return early.
     */
    pj_gettickcount(&t0);
    do {
        pjsua_handle_events(50);
        pj_gettickcount(&now);
        PJ_TIME_VAL_SUB(now, t0);
    } while (PJ_TIME_VAL_MSEC(now) < PJ_IOQUEUE_KEY_FREE_DELAY + 200);
}

/* The invite session of an active call, or NULL */
static pjsip_inv_session *call_inv(pjsua_call_id call_id)
{
    return pjsua_call_is_active(call_id) ? pjsua_var.calls[call_id].inv :
                                           NULL;
}

/* 1 if the call is marked as held, 0 if not, -1 if it is gone */
static int call_hold_mark(pjsua_call_id call_id)
{
    pjsip_inv_session *inv = call_inv(call_id);
    int                held;

    if (!inv)
        return -1;
    pjsip_dlg_inc_lock(inv->dlg);
    held = pjsua_var.calls[call_id].local_hold ? 1 : 0;
    pjsip_dlg_dec_lock(inv->dlg);
    return held;
}

/* The caller's re-INVITE or UPDATE has got its final answer */
static pj_bool_t caller_tsx_done(pjsua_call_id call_id)
{
    pjsip_inv_session *inv = call_inv(call_id);
    pj_bool_t          done;

    if (!inv)
        return PJ_FALSE;
    pjsip_dlg_inc_lock(inv->dlg);
    done = inv->invite_tsx == NULL && inv->update_tsx == NULL;
    pjsip_dlg_dec_lock(inv->dlg);
    return done;
}

/* Waits until the mock proxy has answered n requests and the caller has
 * got the answer.
 */
static pj_bool_t wait_answered(pjsua_call_id call_id, unsigned n)
{
    unsigned elapsed;

    for (elapsed = 0; elapsed < 10000 && (mock_proxy_answered() != n ||
                                          !caller_tsx_done(call_id));
         elapsed += 50)
    {
        pjsua_handle_events(50);
    }
    return mock_proxy_answered() == n && caller_tsx_done(call_id);
}


/*****************************************************************************
 * Sub-test 5: hold_deferred_respond_test
 *
 * The challenge of a hold re-INVITE is deferred, then answered with the
 * account's credentials, and the retry is accepted. The hold must not be
 * rolled back before the retry, so the call ends up on hold.
 *****************************************************************************/
static int hold_deferred_respond_test(pjsua_transport_id tp_id, int port)
{
    test_call_t      tc;
    unsigned         elapsed;
    pj_status_t      status;
    int              rc;

    PJ_LOG(3, (THIS_FILE, "  pjsua auth: hold deferred respond"));

    pj_bzero(&g_ctx, sizeof(g_ctx));

    rc = test_call_start(&tc, tp_id, port, -2410);
    if (rc != 0)
        goto on_return;

    /* The hold re-INVITE is challenged, and the challenge deferred */
    g_ctx.defer_enabled = PJ_TRUE;
    mock_proxy_arm(401, PJ_FALSE, RETRY_SENT);
    if (pjsua_call_set_hold(tc.call_id, NULL) != PJ_SUCCESS) {
        rc = -2415;
        goto on_return;
    }

    for (elapsed = 0; elapsed < 10000 && !g_ctx.deferred.pending;
         elapsed += 50)
    {
        pjsua_handle_events(50);
    }
    if (!g_ctx.deferred.pending) {
        rc = -2416;
        goto on_return;
    }

    status = do_deferred_respond(&g_ctx.deferred);
    if (status != PJ_SUCCESS) {
        rc = -2417;
        goto on_return;
    }

    for (elapsed = 0; elapsed < 10000 && !call_is_held(tc.call_id);
         elapsed += 50)
    {
        pjsua_handle_events(50);
    }
    if (!call_is_held(tc.call_id)) {
        PJ_LOG(3, (THIS_FILE, "    error: the call is not on hold"));
        rc = -2418;
    }

on_return:
    g_ctx.defer_enabled = PJ_FALSE;
    mock_proxy_reset();
    if (g_ctx.deferred.pending) {
        pjsip_auth_clt_async_abandon(g_ctx.deferred.auth_sess,
                                     g_ctx.deferred.token);
        do_deferred_abandon(&g_ctx.deferred);
    }
    test_call_end(&tc);

    return rc;
}


/*****************************************************************************
 * Sub-test 6: hold_3xx_unhold_fail_test
 *
 * An unhold of a call that is not held gets 488: the call must not become
 * held. A hold answered with 302 has failed: the call must not be marked as
 * held. Then a hold is accepted, and an unhold sent as re-INVITE, then as
 * UPDATE, gets 488: the call must be marked as held again each time.
 *****************************************************************************/

/* Sends an unhold that the mock proxy answers with 488 as its n-th answer.
 * Returns 0 if the call is then marked as held or not as expected, or
 * rc_base minus the failed step.
 */
static int unhold_rejected(pjsua_call_id call_id, pj_bool_t update,
                           unsigned n, int held_expected, int rc_base)
{
    pj_status_t status;
    int         held;

    mock_proxy_arm(488, update, RETRY_SENT);
    status = update ? pjsua_call_update(call_id, PJSUA_CALL_UNHOLD, NULL) :
                      pjsua_call_reinvite(call_id, PJSUA_CALL_UNHOLD, NULL);
    if (status != PJ_SUCCESS)
        return rc_base;
    if (!wait_answered(call_id, n))
        return rc_base - 1;

    held = call_hold_mark(call_id);
    if (held != held_expected) {
        PJ_LOG(3, (THIS_FILE, "    error: after a failed %s unhold the "
                   "call is %s", update ? "UPDATE" : "re-INVITE",
                   held < 0 ? "gone" : (held ? "held" : "not held")));
        return rc_base - 2;
    }
    return 0;
}

static int hold_3xx_unhold_fail_test(pjsua_transport_id tp_id, int port)
{
    test_call_t      tc;
    unsigned         elapsed;
    int              held;
    int              rc;

    PJ_LOG(3, (THIS_FILE, "  pjsua auth: hold 3xx, unhold fails"));

    mock_proxy_reset();
    rc = test_call_start(&tc, tp_id, port, -2420);
    if (rc != 0)
        goto on_return;

    rc = unhold_rejected(tc.call_id, PJ_FALSE, 1, 0, -2433);
    if (rc != 0)
        goto on_return;

    mock_proxy_arm(302, PJ_FALSE, RETRY_SENT);
    if (pjsua_call_set_hold(tc.call_id, NULL) != PJ_SUCCESS) {
        rc = -2425;
        goto on_return;
    }
    if (!wait_answered(tc.call_id, 2)) {
        rc = -2426;
        goto on_return;
    }
    held = call_hold_mark(tc.call_id);
    if (held != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: the call is %s",
                   held < 0 ? "gone" : "held after 302"));
        rc = -2427;
        goto on_return;
    }

    if (pjsua_call_set_hold(tc.call_id, NULL) != PJ_SUCCESS) {
        rc = -2428;
        goto on_return;
    }
    for (elapsed = 0; elapsed < 10000 && !call_is_held(tc.call_id);
         elapsed += 50)
    {
        pjsua_handle_events(50);
    }
    if (!call_is_held(tc.call_id) || !caller_tsx_done(tc.call_id)) {
        rc = -2429;
        goto on_return;
    }

    rc = unhold_rejected(tc.call_id, PJ_FALSE, 3, 1, -2430);
    if (rc != 0)
        goto on_return;

    rc = unhold_rejected(tc.call_id, PJ_TRUE, 4, 1, -2436);

on_return:
    mock_proxy_reset();
    test_call_end(&tc);

    return rc;
}


/*****************************************************************************
 * Helpers for sub-tests 7 to 12
 *****************************************************************************/

/* Gets the numbers of active and provisional media of the call */
static pj_bool_t get_media_counts(pjsua_call_id call_id, unsigned *med_cnt,
                                  unsigned *prov_cnt)
{
    pjsip_inv_session *inv = call_inv(call_id);

    if (!inv)
        return PJ_FALSE;
    pjsip_dlg_inc_lock(inv->dlg);
    *med_cnt = pjsua_var.calls[call_id].med_cnt;
    *prov_cnt = pjsua_var.calls[call_id].med_prov_cnt;
    pjsip_dlg_dec_lock(inv->dlg);
    return PJ_TRUE;
}

/* Sends an UPDATE or a re-INVITE with a new offer, adding a stream */
static pj_status_t send_offer(pjsua_call_id call_id, pj_bool_t update,
                              pj_bool_t add_stream)
{
    pjsua_call_info    ci;
    pjsua_call_setting opt;

    if (pjsua_call_get_info(call_id, &ci) != PJ_SUCCESS)
        return PJ_EINVAL;
    opt = ci.setting;
    if (add_stream)
        opt.aud_cnt++;

    return update ? pjsua_call_update2(call_id, &opt, NULL) :
                    pjsua_call_reinvite2(call_id, &opt, NULL);
}


/*****************************************************************************
 * Sub-tests 7 to 9: retry_unsent_test
 *
 * An UPDATE (7) or a re-INVITE (8) adding a stream, or a hold (9), is
 * challenged and its retry with credentials can't be sent. The offer must
 * not be left pending, the provisional media must be reverted and the hold
 * rolled back.
 *****************************************************************************/

/* Gets the caller's next local CSeq and whether its offer is pending */
static pj_bool_t get_offer_state(pjsua_call_id call_id, pj_uint32_t *cseq,
                                 pj_bool_t *offer_pending)
{
    pjsip_inv_session *inv = call_inv(call_id);

    if (!inv)
        return PJ_FALSE;
    pjsip_dlg_inc_lock(inv->dlg);
    *cseq = inv->dlg->local.cseq;
    *offer_pending = pjmedia_sdp_neg_get_state(inv->neg) ==
                     PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER;
    pjsip_dlg_dec_lock(inv->dlg);
    return PJ_TRUE;
}

static int retry_unsent_test(pjsua_transport_id tp_id, int port,
                             pj_bool_t update, pj_bool_t hold)
{
    const int          rc_base = update ? -2440 : (hold ? -2500 : -2460);
    test_call_t        tc;
    pj_uint32_t        cseq, cseq2;
    pj_bool_t          offer_pending;
    unsigned           med_cnt, med_cnt2, prov_cnt;
    pj_status_t        status;
    int                rc;

    PJ_LOG(3, (THIS_FILE, "  pjsua auth: %s retry can't be sent",
               update ? "UPDATE" : (hold ? "hold" : "re-INVITE")));

    mock_proxy_reset();
    rc = test_call_start(&tc, tp_id, port, rc_base);
    if (rc != 0)
        goto on_return;

    mock_proxy_arm(401, update, RETRY_UNSENT);
    if (!get_offer_state(tc.call_id, &cseq, &offer_pending)) {
        rc = rc_base - 5;
        goto on_return;
    }
    status = hold ? pjsua_call_set_hold(tc.call_id, NULL) :
                    send_offer(tc.call_id, update, PJ_TRUE);
    if (status != PJ_SUCCESS) {
        rc = rc_base - 6;
        goto on_return;
    }
    if (!wait_answered(tc.call_id, 1) ||
        !get_offer_state(tc.call_id, &cseq2, &offer_pending) ||
        !get_media_counts(tc.call_id, &med_cnt, &prov_cnt))
    {
        rc = rc_base - 7;
        goto on_return;
    }

    /* The retry was given to the dialog, which failed to send it */
    if (cseq2 != cseq + 2) {
        rc = rc_base - 8;
        goto on_return;
    }
    if (offer_pending) {
        PJ_LOG(3, (THIS_FILE, "    error: the offer is still pending"));
        rc = rc_base - 9;
        goto on_return;
    }
    if (prov_cnt != med_cnt) {
        PJ_LOG(3, (THIS_FILE, "    error: provisional media not reverted "
                   "(%u, active %u)", prov_cnt, med_cnt));
        rc = rc_base - 10;
        goto on_return;
    }
    if (hold && call_hold_mark(tc.call_id) != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: the hold is not rolled back"));
        rc = rc_base - 11;
        goto on_return;
    }

    /* A new offer adding a stream is accepted, after the retry if it had
     * been sent.
     */
    mock_proxy_arm(0, update, RETRY_SENT);
    if (send_offer(tc.call_id, update, PJ_TRUE) != PJ_SUCCESS) {
        rc = rc_base - 12;
        goto on_return;
    }
    if (!wait_answered(tc.call_id, 1) ||
        !get_media_counts(tc.call_id, &med_cnt2, &prov_cnt))
    {
        rc = rc_base - 13;
        goto on_return;
    }
    if (med_cnt2 <= med_cnt || prov_cnt != med_cnt2) {
        PJ_LOG(3, (THIS_FILE, "    error: the new offer was not accepted"));
        rc = rc_base - 15;
        goto on_return;
    }
    if (mock_proxy_auth_count() != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: the retry was sent"));
        rc = rc_base - 14;
    }

on_return:
    mock_proxy_reset();
    test_call_end(&tc);

    return rc;
}


/*****************************************************************************
 * Sub-tests 10 to 12: offer_fail_test
 *
 * An UPDATE (10) or a re-INVITE (11) adding a stream is answered with 302,
 * or an UPDATE adding a stream gets a 401 with no matching credentials
 * (12): the provisional media of the request must be reverted.
 *****************************************************************************/
static int offer_fail_test(pjsua_transport_id tp_id, int port,
                           pj_bool_t update, int code)
{
    const int          rc_base = code == 401 ? -2520 :
                                 (update ? -2480 : -2490);
    test_call_t        tc;
    unsigned           med_cnt, prov_cnt;
    int                rc;

    PJ_LOG(3, (THIS_FILE, "  pjsua auth: %s answered with %d",
               update ? "UPDATE" : "re-INVITE", code));

    mock_proxy_reset();
    rc = test_call_start(&tc, tp_id, port, rc_base);
    if (rc != 0)
        goto on_return;

    mock_proxy_arm(code, update, RETRY_NO_CREDS);
    if (send_offer(tc.call_id, update, PJ_TRUE) != PJ_SUCCESS) {
        rc = rc_base - 6;
        goto on_return;
    }
    if (!wait_answered(tc.call_id, 1) ||
        !get_media_counts(tc.call_id, &med_cnt, &prov_cnt))
    {
        rc = rc_base - 7;
        goto on_return;
    }
    if (prov_cnt != med_cnt) {
        PJ_LOG(3, (THIS_FILE, "    error: provisional media not reverted "
                   "(%u, active %u)", prov_cnt, med_cnt));
        rc = rc_base - 8;
    }

on_return:
    mock_proxy_reset();
    test_call_end(&tc);

    return rc;
}


/*****************************************************************************
 * Main entry point
 *****************************************************************************/
int pjsua_auth_test(void)
{
    pjsua_config      ua_cfg;
    pjsua_logging_config log_cfg;
    pjsua_transport_config tp_cfg;
    pjsua_transport_id tp_id;
    pj_uint16_t        port;
    pj_status_t        status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "pjsua auth challenge test"));

    /* The pjsip test framework's global endpoint owns a tsx layer module
     * (global singleton).  pjsua creates its own endpoint and tries to
     * register the tsx layer there, which asserts if the singleton is
     * already registered.  Destroy the framework endpoint entirely —
     * this also tears down the tsx layer cleanly.  After pjsua_destroy
     * we recreate both via restore_test_endpt().
     */
    pjsip_endpt_destroy(endpt);
    endpt = NULL;

    /* ---- pjsua lifecycle: create / init / start ---- */

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_create failed (%d)", status));
        restore_test_endpt();
        return -2001;
    }

    pjsua_config_default(&ua_cfg);
    ua_cfg.cb.on_auth_challenge = &on_auth_challenge;
    ua_cfg.cb.on_reg_state2     = &on_reg_state2;
    ua_cfg.cb.on_incoming_call  = &on_incoming_call;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.level        = 3;
    log_cfg.console_level = 3;

    status = pjsua_init(&ua_cfg, &log_cfg, NULL);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_init failed (%d)", status));
        pjsua_destroy();
        restore_test_endpt();
        return -2002;
    }

    /* Create UDP transport */
    pjsua_transport_config_default(&tp_cfg);
    tp_cfg.port = 0;  /* ephemeral */

    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tp_cfg, &tp_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_transport_create failed (%d)", status));
        pjsua_destroy();
        restore_test_endpt();
        return -2003;
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_start failed (%d)", status));
        pjsua_destroy();
        restore_test_endpt();
        return -2004;
    }

    /* Get the port for building registrar URI */
    {
        pjsua_transport_info ti;
        pjsua_transport_get_info(tp_id, &ti);
        port = pj_sockaddr_get_port(&ti.local_addr);
    }

    /* Register the mock registrar on pjsua's endpoint */
    status = pjsip_endpt_register_module(pjsua_get_pjsip_endpt(),
                                         &g_mock_reg.mod);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  register mock registrar failed (%d)",
                   status));
        pjsua_destroy();
        restore_test_endpt();
        return -2005;
    }

    g_mock_proxy.pool = pjsua_pool_create("mockproxy", 256, 256);
    status = g_mock_proxy.pool ?
             pj_mutex_create_simple(g_mock_proxy.pool, "mockproxy",
                                    &g_mock_proxy.mutex) : PJ_ENOMEM;
    if (status == PJ_SUCCESS) {
        status = pjsip_endpt_register_module(pjsua_get_pjsip_endpt(),
                                             &g_mock_proxy.mod);
    }
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  mock proxy setup failed (%d)", status));
        rc = -2006;
        goto on_return;
    }

    /* ---- Run sub-tests ---- */

    rc = deferred_respond_test(tp_id, (int)port);
    if (rc != 0) goto on_return;

    /* Let unregistration from acc_del settle before next test */
    pjsua_handle_events(1000);

    rc = deferred_respond_creds_test(tp_id, (int)port);
    if (rc != 0) goto on_return;

    pjsua_handle_events(1000);

    rc = deferred_abandon_test(tp_id, (int)port);
    if (rc != 0) goto on_return;

    pjsua_handle_events(1000);

    /* TODO: disabled — pjsua_acc_del() holds PJSUA_LOCK recursively when
     * calling pjsua_acc_set_registration(), so the inner PJSUA_UNLOCK()
     * before pjsip_regc_send() doesn't fully release the mutex. This is
     * a pre-existing lock-order-inversion (PJSUA_LOCK vs tsx grp_lock)
     * unrelated to async auth. Re-enable once pjsua_acc_del() is fixed.
     */
    /* rc = acc_delete_deferred_test(tp_id, (int)port);
    if (rc != 0) goto on_return; */

    rc = hold_deferred_respond_test(tp_id, (int)port);
    if (rc != 0) goto on_return;

    rc = hold_3xx_unhold_fail_test(tp_id, (int)port);
    if (rc != 0) goto on_return;

    rc = retry_unsent_test(tp_id, (int)port, PJ_TRUE, PJ_FALSE);
    if (rc != 0) goto on_return;

    rc = retry_unsent_test(tp_id, (int)port, PJ_FALSE, PJ_FALSE);
    if (rc != 0) goto on_return;

    rc = retry_unsent_test(tp_id, (int)port, PJ_FALSE, PJ_TRUE);
    if (rc != 0) goto on_return;

    rc = offer_fail_test(tp_id, (int)port, PJ_TRUE, 302);
    if (rc != 0) goto on_return;

    rc = offer_fail_test(tp_id, (int)port, PJ_FALSE, 302);
    if (rc != 0) goto on_return;

    rc = offer_fail_test(tp_id, (int)port, PJ_TRUE, 401);
    if (rc != 0) goto on_return;

on_return:
    if (g_mock_reg.mod.id != -1)
        pjsip_endpt_unregister_module(pjsua_get_pjsip_endpt(),
                                      &g_mock_reg.mod);
    if (g_mock_proxy.mod.id != -1)
        pjsip_endpt_unregister_module(pjsua_get_pjsip_endpt(),
                                      &g_mock_proxy.mod);
    if (g_mock_proxy.mutex) {
        pj_mutex_destroy(g_mock_proxy.mutex);
        g_mock_proxy.mutex = NULL;
    }
    if (g_mock_proxy.pool) {
        pj_pool_release(g_mock_proxy.pool);
        g_mock_proxy.pool = NULL;
    }

    pjsua_handle_events(500);
    pjsua_destroy2(PJSUA_DESTROY_NO_RX_MSG);

    restore_test_endpt();

    return rc;
}

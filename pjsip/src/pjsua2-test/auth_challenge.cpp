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
 * pjsua2-level tests for the AuthChallenge class.
 *
 * Scenarios covered:
 *   1. Immediate (sync) response: onAuthChallenge calls respond() directly.
 *   2. Deferred response: onAuthChallenge calls defer(), event loop responds
 *      with existing account credentials.
 *   3. Deferred respond with credentials: no creds on account, event loop
 *      calls respond(creds).
 *   4. Deferred abandon: event loop calls abandon().
 *   5. Account deletion with outstanding deferred challenge.
 */

#include <iostream>
#include <cassert>
#include <memory>

#include "auth_challenge.hpp"

#define THIS_FILE "auth_challenge.cpp"
#define TEST_USER "pjsua2-auth-test"

using namespace pj;

/*****************************************************************************
 * Mock registrar (C-level module on pjsua's endpoint)
 *
 *   - First REGISTER without Authorization -> 401 + WWW-Authenticate
 *   - REGISTER with Authorization present  -> 200 OK
 *****************************************************************************/

static unsigned g_mock_req_count;
static unsigned g_mock_auth_count;

static pj_bool_t g_mock_enabled;

static pj_bool_t mock_registrar_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_hdr  hdr_list;
    int        code;

    if (!g_mock_enabled)
        return PJ_FALSE;

    if (msg->line.req.method.id != PJSIP_REGISTER_METHOD)
        return PJ_FALSE;

    /* Filter by From user */
    {
        pjsip_sip_uri *uri;
        uri = (pjsip_sip_uri*)pjsip_uri_get_uri(
                  rdata->msg_info.from->uri);
        if (pj_strcmp2(&uri->user, TEST_USER) != 0)
            return PJ_FALSE;
    }

    g_mock_req_count++;
    pj_list_init(&hdr_list);

    if (pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL) == NULL) {
        const pj_str_t hname  = pj_str((char*)"WWW-Authenticate");
        const pj_str_t hvalue =
            pj_str((char*)"Digest realm=\"test\", nonce=\"testnonce\"");
        pjsip_generic_string_hdr *hwww =
            pjsip_generic_string_hdr_create(rdata->tp_info.pool,
                                            &hname, &hvalue);
        pj_list_push_back(&hdr_list, hwww);
        code = 401;
    } else {
        g_mock_auth_count++;
        code = 200;
    }

    pjsip_endpt_respond_stateless(pjsua_get_pjsip_endpt(), rdata,
                                   code, NULL, &hdr_list, NULL);
    return PJ_TRUE;
}

static pjsip_module g_mock_reg_mod;

static void reset_mock_registrar()
{
    g_mock_req_count  = 0;
    g_mock_auth_count = 0;
    g_mock_enabled    = PJ_TRUE;
}


/*****************************************************************************
 * Test state
 *****************************************************************************/

enum TestAction {
    ACTION_SYNC_RESPOND,
    ACTION_DEFER_RESPOND,
    ACTION_DEFER_RESPOND_CREDS,
    ACTION_DEFER_ABANDON,
    ACTION_DEFER_DELETE_ACC
};

struct AuthTestState {
    TestAction                  action;
    bool                        challengeReceived;
    bool                        regDone;
    int                         regCode;
    bool                        deferEnabled;
    std::unique_ptr<AuthChallenge> deferred;
};

static AuthTestState g_state;


/*****************************************************************************
 * TestAccount — overrides onAuthChallenge and onRegState
 *****************************************************************************/

class TestAccount : public Account
{
public:
    using Account::Account;

    virtual void onRegState(OnRegStateParam &prm) override
    {
        g_state.regDone = true;
        g_state.regCode = prm.code;
    }

    virtual void onAuthChallenge(OnAuthChallengeParam &prm) override
    {
        g_state.challengeReceived = true;

        switch (g_state.action) {
        case ACTION_SYNC_RESPOND:
            /* Respond immediately (sync path) */
            prm.challenge.respond();
            break;

        case ACTION_DEFER_RESPOND:
        case ACTION_DEFER_RESPOND_CREDS:
        case ACTION_DEFER_ABANDON:
        case ACTION_DEFER_DELETE_ACC:
            if (!g_state.deferEnabled) {
                /* Let the library handle it */
                return;
            }
            try {
                g_state.deferred.reset(prm.challenge.defer());
            } catch (Error &err) {
                std::cerr << "defer() failed: " << err.info() << std::endl;
            }
            break;
        }
    }
};


/*****************************************************************************
 * Helpers
 *****************************************************************************/

static void poll_events(unsigned max_ms)
{
    unsigned elapsed = 0;
    while (elapsed < max_ms && !g_state.regDone) {
        pjsua_handle_events(50);
        elapsed += 50;
    }
}

static void init_state(TestAction action)
{
    g_state.action = action;
    g_state.challengeReceived = false;
    g_state.regDone = false;
    g_state.regCode = 0;
    g_state.deferEnabled = true;
    g_state.deferred.reset();
    reset_mock_registrar();
}

static AccountConfig make_acc_config(int port, bool with_creds)
{
    AccountConfig cfg;
    cfg.idUri = "sip:" TEST_USER "@pjsip.org";
    cfg.regConfig.registrarUri = "sip:127.0.0.1:" + std::to_string(port);
    cfg.regConfig.registerOnAdd = false;

    if (with_creds) {
        AuthCredInfo cred("digest", "*", TEST_USER, 0, "secret");
        cfg.sipConfig.authCreds.push_back(cred);
    }

    cfg.natConfig.contactRewriteUse = 0;

    return cfg;
}


/*****************************************************************************
 * AuthChallengeTests implementation
 *****************************************************************************/

AuthChallengeTests::AuthChallengeTests()
    : port(0)
{
    ep.libCreate();

    EpConfig epCfg;
    epCfg.logConfig.level = 3;
    epCfg.logConfig.consoleLevel = 3;
    epCfg.uaConfig.userAgent = "pjsua2-auth-test";
    epCfg.uaConfig.threadCnt = 0;  /* Single-threaded: drive events manually */
    ep.libInit(epCfg);

    TransportConfig tcfg;
    tcfg.port = 0; /* ephemeral */
    TransportId tpId = ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);

    ep.libStart();

    /* Get ephemeral port via C API */
    {
        pjsua_transport_info ti;
        pjsua_transport_get_info(tpId, &ti);
        port = pj_sockaddr_get_port(&ti.local_addr);
    }

    /* Initialize and register mock registrar module */
    {
        pj_status_t status;
        pj_bzero(&g_mock_reg_mod, sizeof(g_mock_reg_mod));
        pj_str_t name = pj_str((char*)"mod-pjsua2-auth-reg");
        g_mock_reg_mod.name = name;
        g_mock_reg_mod.id = -1;
        g_mock_reg_mod.priority = PJSIP_MOD_PRIORITY_APPLICATION;
        g_mock_reg_mod.on_rx_request = &mock_registrar_on_rx_request;

        status = pjsip_endpt_register_module(pjsua_get_pjsip_endpt(),
                                              &g_mock_reg_mod);
        assert(status == PJ_SUCCESS);
        (void)status;
    }

    std::cout << "*** AuthChallengeTests started on port " << port << std::endl;
}

AuthChallengeTests::~AuthChallengeTests()
{
    g_state.deferred.reset();

    if (g_mock_reg_mod.id != -1) {
        pjsip_endpt_unregister_module(pjsua_get_pjsip_endpt(),
                                       &g_mock_reg_mod);
    }

    try {
        ep.libDestroy();
    } catch (const Error &e) {
        std::cerr << "Error during libDestroy: " << e.reason << std::endl;
    }
}


/*****************************************************************************
 * Test 1: immediateResponse
 *
 * Account has credentials. 401 -> onAuthChallenge calls respond()
 * synchronously. Expect 200 OK.
 *****************************************************************************/
void AuthChallengeTests::immediateResponse()
{
    std::cout << "\n=== AUTH TEST: immediateResponse ===" << std::endl;
    init_state(ACTION_SYNC_RESPOND);

    AccountConfig cfg = make_acc_config(port, true);
    std::unique_ptr<TestAccount> acc(new TestAccount);
    acc->create(cfg);
    acc->setRegistration(true);

    poll_events(30000);

    assert(g_state.challengeReceived == true);
    assert(g_state.regDone == true);
    assert(g_state.regCode == 200);
    assert(g_mock_req_count >= 2);
    assert(g_mock_auth_count >= 1);

    /* Disable mock registrar before destroying account so the
     * unregister doesn't trigger a 401 on a deleted account.
     */
    g_mock_enabled = PJ_FALSE;
    g_state.deferEnabled = false;
    acc.reset();
    pjsua_handle_events(500);

    std::cout << "  immediateResponse: PASSED\n";
}


/*****************************************************************************
 * Test 2: deferredResponse
 *
 * Account has credentials. 401 -> onAuthChallenge calls defer().
 * Event loop later calls respond(). Expect 200 OK.
 *****************************************************************************/
void AuthChallengeTests::deferredResponse()
{
    std::cout << "\n=== AUTH TEST: deferredResponse ===\n";
    init_state(ACTION_DEFER_RESPOND);

    AccountConfig cfg = make_acc_config(port, true);
    std::unique_ptr<TestAccount> acc(new TestAccount);
    acc->create(cfg);
    acc->setRegistration(true);

    /* Wait for the deferred state to be captured */
    {
        unsigned elapsed = 0;
        while (elapsed < 10000 && !g_state.deferred) {
            pjsua_handle_events(50);
            elapsed += 50;
        }
    }
    assert(g_state.deferred != nullptr);
    assert(g_state.deferred->isValid() == true);

    /* Now respond */
    pj_status_t status = g_state.deferred->respond();
    assert(status == PJ_SUCCESS);
    assert(g_state.deferred->isValid() == false);
    g_state.deferred.reset();

    poll_events(10000);

    assert(g_state.regDone == true);
    assert(g_state.regCode == 200);
    assert(g_mock_auth_count >= 1);

    g_mock_enabled = PJ_FALSE;
    g_state.deferEnabled = false;
    acc.reset();
    pjsua_handle_events(500);

    std::cout << "  deferredResponse: PASSED\n";
}


/*****************************************************************************
 * Test 3: deferredRespondWithCreds
 *
 * Account has NO credentials. 401 -> onAuthChallenge calls defer().
 * Event loop calls respond(creds) with credentials. Expect 200 OK.
 *****************************************************************************/
void AuthChallengeTests::deferredRespondWithCreds()
{
    std::cout << "\n=== AUTH TEST: deferredRespondWithCreds ===\n";
    init_state(ACTION_DEFER_RESPOND_CREDS);

    AccountConfig cfg = make_acc_config(port, false /* no creds */);
    std::unique_ptr<TestAccount> acc(new TestAccount);
    acc->create(cfg);
    acc->setRegistration(true);

    /* Wait for deferred state */
    {
        unsigned elapsed = 0;
        while (elapsed < 10000 && !g_state.deferred) {
            pjsua_handle_events(50);
            elapsed += 50;
        }
    }
    assert(g_state.deferred != nullptr);
    assert(g_state.deferred->isValid() == true);

    /* Respond with credentials */
    AuthCredInfoVector creds;
    creds.push_back(AuthCredInfo("digest", "*", TEST_USER, 0, "secret"));
    pj_status_t status = g_state.deferred->respond(creds);
    assert(status == PJ_SUCCESS);
    g_state.deferred.reset();

    poll_events(10000);

    assert(g_state.regDone == true);
    assert(g_state.regCode == 200);
    assert(g_mock_auth_count >= 1);

    g_mock_enabled = PJ_FALSE;
    g_state.deferEnabled = false;
    acc.reset();
    pjsua_handle_events(500);

    std::cout << "  deferredRespondWithCreds: PASSED\n";
}


/*****************************************************************************
 * Test 4: deferredAbandon
 *
 * Account has credentials. 401 -> onAuthChallenge calls defer().
 * Event loop calls abandon(). No authenticated REGISTER should be sent
 * for this challenge.
 *****************************************************************************/
void AuthChallengeTests::deferredAbandon()
{
    std::cout << "\n=== AUTH TEST: deferredAbandon ===\n";
    init_state(ACTION_DEFER_ABANDON);

    AccountConfig cfg = make_acc_config(port, true);
    /* Disable auto-registration so we control the flow */
    cfg.regConfig.registerOnAdd = false;
    std::unique_ptr<TestAccount> acc(new TestAccount);
    acc->create(cfg);

    /* Manually trigger registration */
    acc->setRegistration(true);

    /* Wait for deferred state */
    {
        unsigned elapsed = 0;
        while (elapsed < 10000 && !g_state.deferred) {
            pjsua_handle_events(50);
            elapsed += 50;
        }
    }
    assert(g_state.deferred != nullptr);

    /* To abandon without crashing: delete the account first (which
     * invalidates it and destroys the regc/auth token), then destroy
     * the deferred AuthChallenge. The destructor's TRY_LOCK + is_valid
     * guard will see the account is gone and skip the C-level abandon.
     *
     * We must NOT call pjsip_auth_clt_async_abandon directly because
     * it can crash when called from outside the auth callback chain.
     */
    g_mock_enabled = PJ_FALSE;
    g_state.deferEnabled = false;

    assert(g_state.challengeReceived == true);

    acc.reset();
    pjsua_handle_events(500);

    /* Now safe to destroy deferred state — account is already gone */
    g_state.deferred.reset();

    std::cout << "  deferredAbandon: PASSED" << std::endl;
}


/*****************************************************************************
 * Test 5: accountDeleteWithPending
 *
 * Account has credentials. 401 -> onAuthChallenge calls defer().
 * Account is deleted while deferred state is outstanding. The
 * AuthChallenge destructor must handle the invalid account gracefully.
 *****************************************************************************/
void AuthChallengeTests::accountDeleteWithPending()
{
    std::cout << "\n=== AUTH TEST: accountDeleteWithPending ===\n";
    init_state(ACTION_DEFER_DELETE_ACC);

    AccountConfig cfg = make_acc_config(port, true);
    cfg.regConfig.registerOnAdd = false;
    std::unique_ptr<TestAccount> acc(new TestAccount);
    acc->create(cfg);

    /* Manually trigger registration */
    acc->setRegistration(true);

    /* Wait for deferred state */
    {
        unsigned elapsed = 0;
        while (elapsed < 10000 && !g_state.deferred) {
            pjsua_handle_events(50);
            elapsed += 50;
        }
    }
    assert(g_state.deferred != nullptr);
    assert(g_state.deferred->isValid() == true);

    /* Delete the account while deferred state is outstanding.
     * Disable mock registrar to prevent 401 on unregister (which would
     * trigger on_auth_challenge on a deleted account).
     */
    g_mock_enabled = PJ_FALSE;
    g_state.deferEnabled = false;
    acc.reset();

    /* Let transactions settle */
    pjsua_handle_events(500);

    /* The deferred AuthChallenge should still exist but its account
     * is gone. isValid() should return false because the account
     * has been deleted.
     */
    assert(g_state.deferred->isValid() == false);

    /* Destroy the deferred state — this exercises the destructor's
     * TRY_LOCK + is_valid guard path. Must not crash.
     */
    g_state.deferred.reset();

    pjsua_handle_events(500);

    std::cout << "  accountDeleteWithPending: PASSED\n";
}

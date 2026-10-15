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
 * pjsua2-level tests for the lifetime of Call and Account objects.
 *
 * The endpoint calls itself: "caller" account makes the call and
 * "callee" account receives it, so each call takes two call slots.
 *
 *   1. Deleting a Call object of an active call hangs up the call.
 *   2. Deleting a Call object after its call has been disconnected and
 *      its slot reused by a new call must not touch the new call.
 *   3. Deleting an Account object after its account has been shut down
 *      and its slot reused by a new account must not touch the new
 *      account, while deleting the new one still deletes its account.
 *   4. The same when the account was deleted without the object.
 */

#include <iostream>
#include <memory>

#include "object_lifetime.hpp"

#define THIS_FILE "object_lifetime.cpp"

/* Always-active assertion (not compiled out by NDEBUG). */
#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            PJ_LOG(1,(THIS_FILE, "TEST ASSERT FAILED: %s (%s:%d)", \
                      #expr, THIS_FILE, __LINE__)); \
            throw Error(PJ_EINVAL, "test assertion", \
                        #expr, THIS_FILE, __LINE__); \
        } \
    } while (0)

#define WAIT_MS     10000

using namespace pj;

/*****************************************************************************
 * TestCall and TestAccount
 *****************************************************************************/

namespace {

class TestCall : public Call
{
public:
    pjsip_inv_state state;

    TestCall(Account &acc, int call_id = PJSUA_INVALID_ID)
    : Call(acc, call_id), state(PJSIP_INV_STATE_NULL)
    {}

    /* No exceptions here: this runs from a pjsua callback */
    virtual void onCallState(OnCallStateParam &prm) override
    {
        pjsua_call_info ci;

        PJ_UNUSED_ARG(prm);
        if (pjsua_call_get_info(getId(), &ci) == PJ_SUCCESS)
            state = ci.state;
    }
};

typedef std::unique_ptr<TestCall> TestCallPtr;

static TestCallPtr g_incoming;
static pj_status_t g_ringing_status;

class TestAccount : public Account
{
public:
    virtual void onIncomingCall(OnIncomingCallParam &iprm) override
    {
        pjsua_call_setting opt;

        pjsua_call_setting_default(&opt);
        opt.vid_cnt = 0;
        g_incoming.reset(new TestCall(*this, iprm.callId));
        g_ringing_status = pjsua_call_answer2(iprm.callId, &opt,
                                              PJSIP_SC_RINGING, NULL, NULL);
    }
};

}


/*****************************************************************************
 * Helpers
 *****************************************************************************/

static bool wait_state(TestCall *call, pjsip_inv_state state)
{
    unsigned elapsed = 0;

    while (elapsed < WAIT_MS && call->state != state) {
        pjsua_handle_events(50);
        elapsed += 50;
    }
    return call->state == state;
}

static bool wait_no_calls()
{
    unsigned elapsed = 0;

    while (elapsed < WAIT_MS && pjsua_call_get_count() != 0) {
        pjsua_handle_events(50);
        elapsed += 50;
    }
    return pjsua_call_get_count() == 0;
}

/* Make a call from the caller to the callee and wait until it rings. */
static void make_ringing_call(Account &caller, pj_uint16_t port,
                              TestCallPtr &out, TestCallPtr &in)
{
    CallOpParam prm(true);
    unsigned elapsed = 0;

    prm.opt.videoCount = 0;
    g_incoming.reset();
    out.reset(new TestCall(caller));
    out->makeCall("sip:callee@127.0.0.1:" + std::to_string(port), prm);

    while (elapsed < WAIT_MS && !g_incoming) {
        pjsua_handle_events(50);
        elapsed += 50;
    }
    in = std::move(g_incoming);
    TEST_ASSERT(in);
    TEST_ASSERT(g_ringing_status == PJ_SUCCESS);
    TEST_ASSERT(wait_state(out.get(), PJSIP_INV_STATE_EARLY));
}


/*****************************************************************************
 * ObjectLifetimeTests implementation
 *****************************************************************************/

ObjectLifetimeTests::ObjectLifetimeTests()
    : port(0)
{
    ep.libCreate();

    EpConfig epCfg;
    epCfg.logConfig.level = 3;
    epCfg.logConfig.consoleLevel = 3;
    epCfg.uaConfig.userAgent = "pjsua2-lifetime-test";
    epCfg.uaConfig.threadCnt = 0;  /* Single-threaded: drive events manually */
    epCfg.uaConfig.maxCalls = 2;   /* One call takes both slots */
    epCfg.medConfig.threadCnt = 0;
    ep.libInit(epCfg);

    TransportConfig tcfg;
    tcfg.port = 0; /* ephemeral */
    TransportId tpId = ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);

    ep.libStart();
    ep.audDevManager().setNullDev();

    {
        pjsua_transport_info ti;
        TEST_ASSERT(pjsua_transport_get_info(tpId, &ti) == PJ_SUCCESS);
        port = pj_sockaddr_get_port(&ti.local_addr);
    }

    AccountConfig cfg;
    cfg.mediaConfig.transportConfig.port = 0;
    cfg.idUri = "sip:caller@127.0.0.1";
    callerAcc.reset(new TestAccount);
    callerAcc->create(cfg);

    cfg.idUri = "sip:callee@127.0.0.1";
    calleeAcc.reset(new TestAccount);
    calleeAcc->create(cfg);

    std::cout << "*** ObjectLifetimeTests started on port " << port
              << std::endl;
}

ObjectLifetimeTests::~ObjectLifetimeTests()
{
    calleeAcc.reset();
    callerAcc.reset();

    try {
        ep.libDestroy();
    } catch (const Error &e) {
        std::cerr << "Error during libDestroy: " << e.reason << std::endl;
    }
}


/*****************************************************************************
 * Test 1: deleteActiveCall
 *
 * Deleting the caller's Call object while the call is ringing hangs up
 * the call, so the callee sees it disconnected.
 *****************************************************************************/
void ObjectLifetimeTests::deleteActiveCall()
{
    TestCallPtr out, in;

    std::cout << "\n=== CALL TEST: deleteActiveCall ===" << std::endl;

    make_ringing_call(*callerAcc, port, out, in);

    out.reset();

    TEST_ASSERT(wait_state(in.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    std::cout << "  deleteActiveCall: PASSED\n";
}


/*****************************************************************************
 * Test 2: deleteCallAfterSlotReuse
 *
 * The Call objects of a disconnected call are kept alive while a new call
 * takes their slots, then deleted. The new call must stay up and keep
 * reporting its state to its own Call objects.
 *****************************************************************************/
void ObjectLifetimeTests::deleteCallAfterSlotReuse()
{
    TestCallPtr out1, in1, out2, in2;
    CallOpParam prm(true);

    prm.opt.videoCount = 0;
    std::cout << "\n=== CALL TEST: deleteCallAfterSlotReuse ===" << std::endl;

    make_ringing_call(*callerAcc, port, out1, in1);
    prm.statusCode = PJSIP_SC_BUSY_HERE;
    in1->hangup(prm);
    TEST_ASSERT(wait_state(out1.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    make_ringing_call(*callerAcc, port, out2, in2);

    /* Both slots of the old call now belong to the new call. */
    TEST_ASSERT(out1->getId() != in1->getId());
    TEST_ASSERT(out2->getId() == out1->getId() ||
                out2->getId() == in1->getId());
    TEST_ASSERT(in2->getId() == out1->getId() ||
                in2->getId() == in1->getId());
    TEST_ASSERT(Call::lookup(out2->getId()) == out2.get());
    TEST_ASSERT(Call::lookup(in2->getId()) == in2.get());

    out1.reset();
    in1.reset();

    TEST_ASSERT(out2->isActive());
    TEST_ASSERT(in2->isActive());
    TEST_ASSERT(Call::lookup(out2->getId()) == out2.get());
    TEST_ASSERT(Call::lookup(in2->getId()) == in2.get());

    prm.statusCode = PJSIP_SC_OK;
    in2->answer(prm);
    TEST_ASSERT(wait_state(out2.get(), PJSIP_INV_STATE_CONFIRMED));
    TEST_ASSERT(wait_state(in2.get(), PJSIP_INV_STATE_CONFIRMED));

    prm.statusCode = PJSIP_SC_OK;
    out2->hangup(prm);
    TEST_ASSERT(wait_state(in2.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    std::cout << "  deleteCallAfterSlotReuse: PASSED\n";
}


/*****************************************************************************
 * Test 3: deleteAccountAfterSlotReuse
 *
 * An Account object is kept alive after shutdown() while a new account
 * takes its id, then deleted. The new account must stay, and deleting it
 * must still delete its account.
 *****************************************************************************/
void ObjectLifetimeTests::deleteAccountAfterSlotReuse()
{
    std::unique_ptr<Account> acc1(new TestAccount);
    std::unique_ptr<Account> acc2(new TestAccount);
    AccountConfig cfg;
    int id, old_id;

    std::cout << "\n=== ACCOUNT TEST: deleteAccountAfterSlotReuse ==="
              << std::endl;

    cfg.idUri = "sip:old@127.0.0.1";
    acc1->create(cfg);
    old_id = acc1->getId();
    acc1->shutdown();
    TEST_ASSERT(!pjsua_acc_is_valid(old_id));

    cfg.idUri = "sip:new@127.0.0.1";
    acc2->create(cfg);
    id = acc2->getId();
    TEST_ASSERT(id == old_id);

    /* The old object no longer refers to the id */
    TEST_ASSERT(!acc1->isValid());

    acc1.reset();

    TEST_ASSERT(pjsua_acc_is_valid(id));

    acc2.reset();

    TEST_ASSERT(!pjsua_acc_is_valid(id));

    std::cout << "  deleteAccountAfterSlotReuse: PASSED\n";
}


/*****************************************************************************
 * Test 4: deleteAccountDeletedUnderneath
 *
 * The account of an Account object is deleted without the object, e.g. by
 * a library restart, and a new account takes its id. Deleting the old
 * object must not touch the new account.
 *****************************************************************************/
void ObjectLifetimeTests::deleteAccountDeletedUnderneath()
{
    std::unique_ptr<Account> acc1(new TestAccount);
    std::unique_ptr<Account> acc2(new TestAccount);
    AccountConfig cfg;
    int id, old_id;

    std::cout << "\n=== ACCOUNT TEST: deleteAccountDeletedUnderneath ==="
              << std::endl;

    cfg.idUri = "sip:old@127.0.0.1";
    acc1->create(cfg);
    old_id = acc1->getId();
    TEST_ASSERT(pjsua_acc_del(old_id) == PJ_SUCCESS);

    cfg.idUri = "sip:new@127.0.0.1";
    acc2->create(cfg);
    id = acc2->getId();
    TEST_ASSERT(id == old_id);

    acc1.reset();

    TEST_ASSERT(pjsua_acc_is_valid(id));

    acc2.reset();

    TEST_ASSERT(!pjsua_acc_is_valid(id));

    std::cout << "  deleteAccountDeletedUnderneath: PASSED\n";
}

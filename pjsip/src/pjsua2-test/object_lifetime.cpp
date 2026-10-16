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
 *   1. Deleting a Call object of an active call hangs up the call, also
 *      while the call is still creating its media or its dialog is busy.
 *      When the library is too busy for that, the call must not keep
 *      pointing to the deleted object.
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
#include <pjsua-lib/pjsua_internal.h>

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
    pjsip_inv_state deleteOnState;

    TestCall(Account &acc, int call_id = PJSUA_INVALID_ID)
    : Call(acc, call_id), state(PJSIP_INV_STATE_NULL),
      deleteOnState(PJSIP_INV_STATE_NULL)
    {}

    /* No exceptions here: this runs from a pjsua callback */
    virtual void onCallState(OnCallStateParam &prm) override
    {
        pjsua_call_info ci;

        PJ_UNUSED_ARG(prm);
        if (pjsua_call_get_info(getId(), &ci) == PJ_SUCCESS)
            state = ci.state;
        if (deleteOnState != PJSIP_INV_STATE_NULL && state == deleteOnState)
            delete this;
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
    in = PJSUA2_MOVE(g_incoming);
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
    g_incoming.reset();
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
 * Test 1b: deleteCallCreatingMedia
 *
 * With ICE the caller's media is created asynchronously, and without worker
 * threads it can't complete before events are polled. Deleting the Call
 * object meanwhile must end the call before its INVITE is sent.
 *****************************************************************************/
void ObjectLifetimeTests::deleteCallCreatingMedia()
{
    std::unique_ptr<Account> acc(new TestAccount);
    AccountConfig cfg;
    TestCallPtr out;
    CallOpParam prm(true);
    pjsua_call_info ci;
    unsigned elapsed = 0;
    int id;

    std::cout << "\n=== CALL TEST: deleteCallCreatingMedia ===" << std::endl;

    cfg.idUri = "sip:ice@127.0.0.1";
    cfg.natConfig.iceEnabled = true;
    acc->create(cfg);

    prm.opt.videoCount = 0;
    g_incoming.reset();
    out.reset(new TestCall(*acc));
    out->makeCall("sip:callee@127.0.0.1:" + std::to_string(port), prm);
    id = out->getId();
    TEST_ASSERT(pjsua_call_get_info(id, &ci) == PJ_SUCCESS);
    TEST_ASSERT(ci.state == PJSIP_INV_STATE_NULL);
    TEST_ASSERT(pjsua_var.calls[id].med_ch_cb != NULL);

    out.reset();

    /* The call doesn't count as a call before its INVITE is created */
    while (elapsed < WAIT_MS && pjsua_call_get_info(id, &ci) == PJ_SUCCESS)
    {
        pjsua_handle_events(50);
        elapsed += 50;
    }
    TEST_ASSERT(pjsua_call_get_info(id, &ci) != PJ_SUCCESS);
    TEST_ASSERT(!g_incoming);

    std::cout << "  deleteCallCreatingMedia: PASSED\n";
}


/*****************************************************************************
 * Test 1c: deleteCallWithLibraryBusy
 *
 * Another thread holds the PJSUA lock for longer than a call API waits for
 * it. The call must not keep the deleted Call object as its owner, and it
 * must still be hung up later.
 *****************************************************************************/
#if PJ_HAS_THREADS
static pj_sem_t *g_locked, *g_done;
static unsigned g_hold_ms;
static int g_check_id = PJSUA_INVALID_ID;
static void *g_owner_while_deleting;

/* Hold the dialog lock if arg is a dialog, the PJSUA lock otherwise,
 * until told to release it, or for g_hold_ms if set. With g_check_id set,
 * look at that call's owner while the Call object is being deleted.
 */
static int hold_lock(void *arg)
{
    pjsip_dialog *dlg = (pjsip_dialog*)arg;

    if (dlg)
        pjsip_dlg_inc_lock(dlg);
    else
        PJSUA_LOCK();
    pj_sem_post(g_locked);
    if (g_check_id != PJSUA_INVALID_ID) {
        /* Well within the deletion's wait for this lock */
        pj_thread_sleep(PJSUA_ACQUIRE_CALL_TIMEOUT / 2);
        pj_mutex_lock(pjsua_var.call_owner_mutex);
        g_owner_while_deleting = pjsua_var.calls[g_check_id].user_data;
        pj_mutex_unlock(pjsua_var.call_owner_mutex);
    }
    if (g_hold_ms)
        pj_thread_sleep(g_hold_ms);
    else
        pj_sem_wait(g_done);
    if (dlg)
        pjsip_dlg_dec_lock(dlg);
    else
        PJSUA_UNLOCK();
    return 0;
}

static pj_status_t g_release_status;

/* Delete the Call object while another thread holds the lock. With hold_ms
 * set, release its call directly first to get the result.
 */
static bool delete_while_locked(TestCallPtr &call, pjsip_dialog *dlg,
                                unsigned hold_ms = 0)
{
    pj_pool_t *pool;
    pj_thread_t *thread = NULL;
    bool ok;

    pool = pjsua_pool_create("busy", 512, 512);
    if (!pool)
        return false;
    g_locked = g_done = NULL;
    g_hold_ms = hold_ms;
    ok = pj_sem_create(pool, "locked", 0, 1, &g_locked) == PJ_SUCCESS &&
         pj_sem_create(pool, "done", 0, 1, &g_done) == PJ_SUCCESS &&
         pj_thread_create(pool, "busy", &hold_lock, dlg, 0, 0,
                          &thread) == PJ_SUCCESS;

    if (ok) {
        pj_sem_wait(g_locked);
        if (hold_ms)
            g_release_status = pjsua_call_release_owner(call->getId(),
                                                        call.get());
        call.reset();
        pj_sem_post(g_done);
        pj_thread_join(thread);
        pj_thread_destroy(thread);
    }
    if (g_locked)
        pj_sem_destroy(g_locked);
    if (g_done)
        pj_sem_destroy(g_done);
    pj_pool_release(pool);
    return ok;
}
#endif

void ObjectLifetimeTests::deleteCallWithLibraryBusy()
{
#if PJ_HAS_THREADS
    TestCallPtr out, in;
    bool ok, released;
    int id;

    std::cout << "\n=== CALL TEST: deleteCallWithLibraryBusy ===" << std::endl;

    make_ringing_call(*callerAcc, port, out, in);
    id = out->getId();

    ok = delete_while_locked(out, NULL);
    released = (pjsua_call_get_user_data(id) == NULL);

    /* Don't let the call's events reach the deleted object */
    if (!released && !out)
        pjsua_call_set_user_data(id, NULL);
    if (!ok)
        out.reset();
    TEST_ASSERT(ok);
    TEST_ASSERT(released);

    /* The hangup is retried after CALL_HANGUP_RETRY_INTERVAL (5 s) */
    TEST_ASSERT(wait_state(in.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    std::cout << "  deleteCallWithLibraryBusy: PASSED\n";
#endif
}


/*****************************************************************************
 * Test 1d: deleteCallWithDialogBusy
 *
 * Another thread holds the call's dialog lock for longer than a call API
 * waits for it. Deleting the Call object must detach it from the call at
 * once and still end the call, later. Its media objects must not leak
 * (checked by LeakSanitizer).
 *****************************************************************************/
void ObjectLifetimeTests::deleteCallWithDialogBusy()
{
#if PJ_HAS_THREADS
    TestCallPtr out, in;
    CallOpParam prm(true);
    bool ok;
    int id;

    std::cout << "\n=== CALL TEST: deleteCallWithDialogBusy ===" << std::endl;

    make_ringing_call(*callerAcc, port, out, in);
    id = out->getId();
    prm.statusCode = PJSIP_SC_OK;
    in->answer(prm);
    TEST_ASSERT(wait_state(out.get(), PJSIP_INV_STATE_CONFIRMED));
    TEST_ASSERT(out->getMedia(0) != NULL);

    g_check_id = id;
    g_owner_while_deleting = out.get();
    ok = delete_while_locked(out, pjsua_var.calls[id].inv->dlg);
    g_check_id = PJSUA_INVALID_ID;
    if (!ok)
        out.reset();

    TEST_ASSERT(ok);
    /* Lookups must not find the object while it waits for the call */
    TEST_ASSERT(g_owner_while_deleting == NULL);
    TEST_ASSERT(pjsua_call_get_user_data(id) == NULL);
    /* The hangup is retried after CALL_HANGUP_RETRY_INTERVAL (5 s) */
    TEST_ASSERT(wait_state(in.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    std::cout << "  deleteCallWithDialogBusy: PASSED\n";
#endif
}


/*****************************************************************************
 * Test 1e: deleteCallWithLibraryBriefBusy
 *
 * Another thread holds the PJSUA lock for a while, shorter than a call API
 * waits for it. The hangup waits for the lock, it isn't put off.
 *****************************************************************************/
void ObjectLifetimeTests::deleteCallWithLibraryBriefBusy()
{
#if PJ_HAS_THREADS
    TestCallPtr out, in;
    bool ok;

    std::cout << "\n=== CALL TEST: deleteCallWithLibraryBriefBusy ==="
              << std::endl;

    make_ringing_call(*callerAcc, port, out, in);

    g_release_status = PJ_EUNKNOWN;
    ok = delete_while_locked(out, NULL, 300);
    if (!ok)
        out.reset();

    TEST_ASSERT(ok);
    TEST_ASSERT(g_release_status == PJ_SUCCESS);
    TEST_ASSERT(wait_state(in.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    std::cout << "  deleteCallWithLibraryBriefBusy: PASSED\n";
#endif
}


/*****************************************************************************
 * Test 1f: hangupRetryAfterSlotReuse
 *
 * The hangup of a deleted Call object is retried later, but its call ends
 * and a new call takes the slot first. The retry must not touch it.
 *****************************************************************************/
void ObjectLifetimeTests::hangupRetryAfterSlotReuse()
{
#if PJ_HAS_THREADS
    TestCallPtr out1, in1, out2, in2;
    CallOpParam prm(true);
    unsigned elapsed = 0;
    bool ok;
    int id;

    std::cout << "\n=== CALL TEST: hangupRetryAfterSlotReuse ===" << std::endl;

    make_ringing_call(*callerAcc, port, out1, in1);
    id = out1->getId();

    ok = delete_while_locked(out1, pjsua_var.calls[id].inv->dlg);
    if (!ok)
        out1.reset();
    TEST_ASSERT(ok);

    prm.statusCode = PJSIP_SC_BUSY_HERE;
    in1->hangup(prm);
    TEST_ASSERT(wait_no_calls());

    make_ringing_call(*callerAcc, port, out2, in2);
    TEST_ASSERT(out2->getId() == id || in2->getId() == id);

    /* Let the retry run, CALL_HANGUP_RETRY_INTERVAL (5 s) after the first
     * attempt
     */
    while (elapsed < WAIT_MS) {
        pjsua_handle_events(50);
        elapsed += 50;
    }
    TEST_ASSERT(out2->isActive());
    TEST_ASSERT(in2->isActive());

    prm.statusCode = PJSIP_SC_OK;
    out2->hangup(prm);
    TEST_ASSERT(wait_state(in2.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    std::cout << "  hangupRetryAfterSlotReuse: PASSED\n";
#endif
}


/*****************************************************************************
 * Test 1g: hangupRetryWhileBusy
 *
 * The call's dialog lock stays busy for many hangup retries of a deleted
 * Call object. The retries must not hold up event polling, and must go on
 * until the lock is free, so the call is still hung up.
 *****************************************************************************/
void ObjectLifetimeTests::hangupRetryWhileBusy()
{
#if PJ_HAS_THREADS
    /* Long enough for several retries */
    const long hold_ms = 25000;
    TestCallPtr out, in;
    pj_pool_t *pool;
    pj_thread_t *thread = NULL;
    pj_time_val start, now, poll_start;
    long longest_poll = 0;
    bool ok;
    int id;

    std::cout << "\n=== CALL TEST: hangupRetryWhileBusy ===" << std::endl;

    make_ringing_call(*callerAcc, port, out, in);
    id = out->getId();

    pool = pjsua_pool_create("stuck", 512, 512);
    g_locked = g_done = NULL;
    g_hold_ms = 0;
    ok = pool &&
         pj_sem_create(pool, "locked", 0, 1, &g_locked) == PJ_SUCCESS &&
         pj_sem_create(pool, "done", 0, 1, &g_done) == PJ_SUCCESS &&
         pj_thread_create(pool, "stuck", &hold_lock,
                          pjsua_var.calls[id].inv->dlg, 0, 0,
                          &thread) == PJ_SUCCESS;
    if (ok) {
        pj_sem_wait(g_locked);
        out.reset();
        pj_gettimeofday(&start);
        do {
            pj_gettimeofday(&poll_start);
            pjsua_handle_events(50);
            pj_gettimeofday(&now);
            PJ_TIME_VAL_SUB(now, poll_start);
            if (PJ_TIME_VAL_MSEC(now) > longest_poll)
                longest_poll = PJ_TIME_VAL_MSEC(now);
            pj_gettimeofday(&now);
            PJ_TIME_VAL_SUB(now, start);
        } while (PJ_TIME_VAL_MSEC(now) < hold_ms);
        pj_sem_post(g_done);
        pj_thread_join(thread);
        pj_thread_destroy(thread);
    }
    if (g_locked)
        pj_sem_destroy(g_locked);
    if (g_done)
        pj_sem_destroy(g_done);
    if (pool)
        pj_pool_release(pool);
    if (!ok)
        out.reset();
    TEST_ASSERT(ok);

    /* A retry doesn't wait for the lock */
    TEST_ASSERT(longest_poll < PJSUA_ACQUIRE_CALL_TIMEOUT);
    TEST_ASSERT(wait_state(in.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    std::cout << "  hangupRetryWhileBusy: PASSED\n";
#endif
}


/*****************************************************************************
 * Test 1h: deleteCallInOwnCallback
 *
 * A Call object deletes itself from its own call state callback, as pjsua2
 * applications commonly do: when the call is disconnected, and while it is
 * still up, which then hangs it up. A call with media must not leak its
 * media objects (checked by LeakSanitizer).
 *****************************************************************************/
void ObjectLifetimeTests::deleteCallInOwnCallback()
{
    TestCallPtr out, in;
    TestCall *caller, *callee;
    CallOpParam prm(true);
    unsigned elapsed = 0;
    int id;

    std::cout << "\n=== CALL TEST: deleteCallInOwnCallback ===" << std::endl;

    make_ringing_call(*callerAcc, port, out, in);
    id = out->getId();
    out->deleteOnState = PJSIP_INV_STATE_DISCONNECTED;
    out.release();

    prm.statusCode = PJSIP_SC_BUSY_HERE;
    in->hangup(prm);
    TEST_ASSERT(wait_no_calls());
    TEST_ASSERT(pjsua_call_get_user_data(id) == NULL);
    /* Before the slot is reused, so the old object can't affect it */
    in.reset();

    /* The caller deletes itself when the callee's 180 arrives */
    prm.opt.videoCount = 0;
    g_incoming.reset();
    caller = new TestCall(*callerAcc);
    caller->deleteOnState = PJSIP_INV_STATE_EARLY;
    caller->makeCall("sip:callee@127.0.0.1:" + std::to_string(port), prm);

    while (elapsed < WAIT_MS && !g_incoming) {
        pjsua_handle_events(50);
        elapsed += 50;
    }
    in = PJSUA2_MOVE(g_incoming);
    TEST_ASSERT(in);
    TEST_ASSERT(wait_state(in.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());
    in.reset();

    /* The callee deletes itself when the ACK confirms the call */
    make_ringing_call(*callerAcc, port, out, in);
    callee = in.release();
    callee->deleteOnState = PJSIP_INV_STATE_CONFIRMED;
    prm.statusCode = PJSIP_SC_OK;
    callee->answer(prm);
    TEST_ASSERT(wait_state(out.get(), PJSIP_INV_STATE_DISCONNECTED));
    TEST_ASSERT(wait_no_calls());

    std::cout << "  deleteCallInOwnCallback: PASSED\n";
}


/*****************************************************************************
 * Test 1i: deleteCallWhileClosing
 *
 * While the library is being destroyed, deleting a Call object only
 * detaches it from its call, and only if the call is still its own.
 *****************************************************************************/
void ObjectLifetimeTests::deleteCallWhileClosing()
{
    TestCallPtr out, in, second;
    bool detached, kept;
    unsigned elapsed = 0;
    int id;

    std::cout << "\n=== CALL TEST: deleteCallWhileClosing ===" << std::endl;

    make_ringing_call(*callerAcc, port, out, in);
    id = out->getId();

    /* As during libDestroy(), with no events polled meanwhile */
    pjsua_set_state(PJSUA_STATE_CLOSING);
    out.reset();
    detached = (pjsua_call_get_user_data(id) == NULL);
    out.reset(new TestCall(*callerAcc, id));
    second.reset(new TestCall(*callerAcc, id));
    out.reset();
    kept = (pjsua_call_get_user_data(id) == second.get());
    pjsua_set_state(PJSUA_STATE_RUNNING);

    pjsua_call_set_user_data(id, NULL);
    second.reset();

    /* The library hangs up the calls itself, the Calls don't */
    while (elapsed < 1000) {
        pjsua_handle_events(50);
        elapsed += 50;
    }
    TEST_ASSERT(in->isActive());

    pjsua_call_hangup_all();
    TEST_ASSERT(wait_no_calls());
    TEST_ASSERT(detached);
    TEST_ASSERT(kept);

    std::cout << "  deleteCallWhileClosing: PASSED\n";
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
    {
        int def = pjsua_acc_get_default();
        bool failed = false;

        try {
            acc1->setDefault();
        } catch (const Error &) {
            failed = true;
        }
        TEST_ASSERT(failed);
        TEST_ASSERT(pjsua_acc_get_default() == def);
    }

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

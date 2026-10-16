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
 * Tests for values that the application returns to pjsua from call
 * callbacks: they must stay valid after the callback has returned.
 */

#include <pjsua2.hpp>
#include <pjsua-lib/pjsua_internal.h>
#include <iostream>
#include <memory>

using namespace pj;

#define THIS_FILE "call_replace.cpp"

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

static const std::string REJECT_REASON =
    "Replaces Rejected By Application Policy Check";

static const std::string XFER_CALL_ID =
    "transfer-call-id-set-by-application@example.org";

static struct {
    bool calleeConfirmed;
    bool replaceRequested;
    bool callerBDisconnected;
    int callerBStatus;
    std::string callerBReason;
    bool targetIncoming;
    std::string targetCallId;
} st;

class CalleeCall : public Call
{
public:
    Account &myAcc;
    std::unique_ptr<Call> xferCall;

    CalleeCall(Account &acc, int call_id = PJSUA_INVALID_ID)
    : Call(acc, call_id), myAcc(acc) {}

    virtual void onCallTransferRequest(OnCallTransferRequestParam &prm)
    override
    {
        prm.opt.customCallId = XFER_CALL_ID;
        xferCall.reset(new CalleeCall(myAcc));
        prm.newCall = xferCall.get();
    }

    virtual void onCallState(OnCallStateParam &prm) override
    {
        PJ_UNUSED_ARG(prm);
        if (getInfo().state == PJSIP_INV_STATE_CONFIRMED)
            st.calleeConfirmed = true;
    }

    virtual void onCallReplaceRequest(OnCallReplaceRequestParam &prm) override
    {
        st.replaceRequested = true;
        prm.statusCode = PJSIP_SC_BUSY_HERE;
        prm.reason = REJECT_REASON;
    }
};

class CallerCall : public Call
{
public:
    bool isB;
    CallerCall(Account &acc, bool b) : Call(acc), isB(b) {}

    virtual void onCallState(OnCallStateParam &prm) override
    {
        PJ_UNUSED_ARG(prm);
        CallInfo ci = getInfo();
        if (isB && ci.state == PJSIP_INV_STATE_DISCONNECTED) {
            st.callerBStatus = ci.lastStatusCode;
            st.callerBReason = ci.lastReason;
            st.callerBDisconnected = true;
        }
    }
};

class CalleeAccount : public Account
{
public:
    std::unique_ptr<CalleeCall> call;

    virtual void onIncomingCall(OnIncomingCallParam &iprm) override
    {
        call.reset(new CalleeCall(*this, iprm.callId));
        CallOpParam prm;
        prm.statusCode = PJSIP_SC_OK;
        call->answer(prm);
    }
};

class TargetAccount : public Account
{
public:
    std::unique_ptr<Call> call;

    virtual void onIncomingCall(OnIncomingCallParam &iprm) override
    {
        call.reset(new Call(*this, iprm.callId));
        st.targetCallId = call->getInfo().callIdString;
        st.targetIncoming = true;
        CallOpParam prm;
        prm.statusCode = PJSIP_SC_BUSY_HERE;
        call->answer(prm);
    }
};

static bool waitFor(Endpoint &ep, bool &flag)
{
    for (int i = 0; i < 100 && !flag; ++i)
        ep.libHandleEvents(50);
    return flag;
}

static void pollEvents(Endpoint &ep, unsigned msec)
{
    for (unsigned i = 0; i < msec / 50; ++i)
        ep.libHandleEvents(50);
}

/* Returns the local URI prefix "sip:<user>@127.0.0.1:<port>" */
static std::string startEndpoint(Endpoint &ep)
{
    EpConfig epCfg;
    epCfg.logConfig.level = 4;
    epCfg.logConfig.consoleLevel = 3;
    epCfg.uaConfig.threadCnt = 0;  /* Single-threaded: drive events manually */
    ep.libCreate();
    ep.libInit(epCfg);
    ep.audDevManager().setNullDev();

    TransportConfig tcfg;
    tcfg.port = 0; /* ephemeral */
    TransportId tpId = ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
    ep.libStart();

    pjsua_transport_info ti;
    pjsua_transport_get_info(tpId, &ti);
    return "@127.0.0.1:" +
           std::to_string(pj_sockaddr_get_port(&ti.local_addr));
}

/* Replace request rejected with a reason set by the application. */
void callReplaceRejectReasonTest()
{
    Endpoint ep;
    st = {};

    std::string host = startEndpoint(ep);

    {
        AccountConfig calleeCfg;
        calleeCfg.idUri = "sip:callee" + host;
        CalleeAccount callee;
        callee.create(calleeCfg);

        AccountConfig callerCfg;
        callerCfg.idUri = "sip:caller" + host;
        Account caller;
        caller.create(callerCfg);

        CallerCall callA(caller, false);
        CallOpParam opA(true);
        callA.makeCall("sip:callee" + host, opA);
        TEST_ASSERT(waitFor(ep, st.calleeConfirmed));

        /* Build a Replaces header for call A, as seen by the callee */
        std::string replaces;
        {
            pjsip_dialog *dlg = pjsua_var.calls[callA.getId()].inv->dlg;
            replaces = std::string(dlg->call_id->id.ptr,
                                   dlg->call_id->id.slen) +
                       ";to-tag=" +
                       std::string(dlg->remote.info->tag.ptr,
                                   dlg->remote.info->tag.slen) +
                       ";from-tag=" +
                       std::string(dlg->local.info->tag.ptr,
                                   dlg->local.info->tag.slen);
        }

        CallerCall callB(caller, true);
        CallOpParam opB(true);
        SipHeader hdr;
        hdr.hName = "Replaces";
        hdr.hValue = PJSUA2_MOVE(replaces);
        opB.txOption.headers.push_back(PJSUA2_MOVE(hdr));
        callB.makeCall("sip:callee" + host, opB);

        TEST_ASSERT(waitFor(ep, st.callerBDisconnected));
        TEST_ASSERT(st.replaceRequested);
        TEST_ASSERT(st.callerBStatus == PJSIP_SC_BUSY_HERE);
        TEST_ASSERT(st.callerBReason == REJECT_REASON);

        ep.hangupAllCalls();
        pollEvents(ep, 500);
    }

    ep.libDestroy();
    std::cout << "Replace rejected with application reason: "
              << st.callerBReason << std::endl;
}

/* Transferred call made with a Call-ID set by the application. */
void callTransferCustomCallIdTest()
{
    Endpoint ep;
    st = {};

    std::string host = startEndpoint(ep);

    {
        AccountConfig targetCfg;
        targetCfg.idUri = "sip:target" + host;
        TargetAccount target;
        target.create(targetCfg);

        AccountConfig calleeCfg;
        calleeCfg.idUri = "sip:callee" + host;
        CalleeAccount callee;
        callee.create(calleeCfg);

        AccountConfig callerCfg;
        callerCfg.idUri = "sip:caller" + host;
        Account caller;
        caller.create(callerCfg);

        CallerCall callA(caller, false);
        CallOpParam opA(true);
        callA.makeCall("sip:callee" + host, opA);
        TEST_ASSERT(waitFor(ep, st.calleeConfirmed));

        CallOpParam opX(true);
        callA.xfer("sip:target" + host, opX);

        TEST_ASSERT(waitFor(ep, st.targetIncoming));
        TEST_ASSERT(st.targetCallId == XFER_CALL_ID);

        ep.hangupAllCalls();
        pollEvents(ep, 500);
    }

    ep.libDestroy();
    std::cout << "Transferred call used application Call-ID: "
              << st.targetCallId << std::endl;
}

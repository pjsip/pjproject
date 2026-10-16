/*
 * Copyright (C) 2025-2026 Teluu Inc. (http://www.teluu.com)
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

#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <memory>

#include "instant_messaging.hpp"

#define THIS_FILE "instant_messaging.cpp"

using namespace pj;

// Definition of static member
TestState InstantMessagingTests::testState;

ReceiverAccount::ReceiverAccount()
{
}

ReceiverAccount::~ReceiverAccount()
{
}

void ReceiverAccount::onInstantMessage(OnInstantMessageParam &prm)
{
    std::cout << "*** ReceiverAccount: Instant message received from "
              << prm.fromUri << "\n";
    std::cout << "*** Message content: " << prm.msgBody << "\n";

    deferredResponse = DeferredResponse(prm);
    lastTsx = pjsip_rdata_get_tsx(static_cast<pjsip_rx_data*>(prm.rdata.pjRxData));

    InstantMessagingTests::testState.messageReceived = true;
    InstantMessagingTests::testState.receivedMessage = prm.msgBody;
    InstantMessagingTests::testState.receivedContentType = prm.contentType;

    std::cout << "*** ReceiverAccount: Message deferred, will respond later\n";
}

void ReceiverAccount::sendDeferredResponse(int statusCode, const std::string& reason)
{
    try {
        SendResponseParam respParam;
        respParam.deferredResponse = std::move(deferredResponse);
        respParam.code = statusCode;
        respParam.reason = reason;

        sendResponse(respParam);

        InstantMessagingTests::testState.responseSent = true;
        std::cout << "*** ReceiverAccount: Deferred response sent - "
                  << statusCode << " " << reason << "\n";
    } catch (Error& err) {
        InstantMessagingTests::testState.responseError = err.status;
        std::cout << "*** ReceiverAccount: Error sending deferred response: "
                  << err.info() << "\n";
    }
}

void SenderAccount::onInstantMessageStatus(OnInstantMessageStatusParam &prm)
{
    std::cout << "*** SenderAccount: Message status update - "
              << prm.code << " " << prm.reason << "\n";

    InstantMessagingTests::testState.responseReceived = true;
    InstantMessagingTests::testState.responseStatusCode = prm.code;
    InstantMessagingTests::testState.responseReason = prm.reason;
}

InstantMessagingTests::InstantMessagingTests()
{
    ep.libCreate();

    EpConfig epCfg;
    epCfg.logConfig.level = 4;
    epCfg.uaConfig.userAgent = "pjsua++-test";
    ep.libInit(epCfg);

    TransportConfig tcfg;
    tcfg.port = 5060;
    ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);

    ep.libStart();
    std::cout << "*** PJSUA2 STARTED ***\n";
}

InstantMessagingTests::~InstantMessagingTests()
{
    senderBuddy.reset();
    senderAcc.reset();
    receiverAcc.reset();

    try {
        ep.libDestroy();
    } catch (const pj::Error& e) {
        // Log the error but don't rethrow from destructor
        std::cerr << "Error during libDestroy: " << e.reason << std::endl;
    }
}

void InstantMessagingTests::immediateResponse()
{
    testState = TestState{};

    AccountConfig receiverCfg;
    receiverCfg.idUri = "sip:receiver@localhost:5060";
    receiverCfg.sipConfig.autoRespondSipMessage = PJ_TRUE;

    receiverAcc = std::unique_ptr<ReceiverAccount>(new ReceiverAccount());
    receiverAcc->create(receiverCfg);
    std::cout << "*** Receiver account created: " << receiverCfg.idUri << "\n";

    AccountConfig senderCfg;
    senderCfg.idUri = "sip:sender@localhost:5060";
    senderCfg.sipConfig.autoRespondSipMessage = PJ_TRUE;

    senderAcc = std::unique_ptr<SenderAccount>(new SenderAccount());
    senderAcc->create(senderCfg);
    std::cout << "*** Sender account created: " << senderCfg.idUri << "\n";

    std::cout << "\n=== TEST STEP 1: Creating buddy and sending SIP MESSAGE ===\n";

    BuddyConfig buddyCfg;
    buddyCfg.uri = "sip:receiver@localhost:5060";
    buddyCfg.subscribe = false;
    buddyCfg.subscribe_dlg_event = false;

    senderBuddy = std::unique_ptr<Buddy>(new Buddy());
    senderBuddy->create(*senderAcc, buddyCfg);

    const std::string testMessage = "Hello world!";

    SendInstantMessageParam msgParam;
    msgParam.content = testMessage;
    msgParam.contentType = "text/plain";

    senderBuddy->sendInstantMessage(msgParam);
    std::cout << "*** SenderAccount: Message sent via buddy - content: " << testMessage << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    assert(testState.messageReceived == true);
    assert(testState.receivedMessage == testMessage);
    assert(testState.responseSent == false);
    assert(testState.responseReceived == true);
    assert(testState.responseStatusCode == 200);
    assert(testState.responseReason == "OK");

    std::cout << "✓ Immediate response successfully received\n";
    std::cout << "✓ Response code: " << testState.responseStatusCode << "\n";
    std::cout << "✓ Response reason: " << testState.responseReason << "\n";

}

void InstantMessagingTests::deferredResponse()
{
    testState = TestState{};

    AccountConfig receiverCfg;
    receiverCfg.idUri = "sip:receiver@localhost:5060";
    receiverCfg.sipConfig.autoRespondSipMessage = PJ_FALSE;

    receiverAcc = std::unique_ptr<ReceiverAccount>(new ReceiverAccount());
    receiverAcc->create(receiverCfg);
    std::cout << "*** Receiver account created: " << receiverCfg.idUri << "\n";

    AccountConfig senderCfg;
    senderCfg.idUri = "sip:sender@localhost:5060";
    senderCfg.sipConfig.autoRespondSipMessage = PJ_FALSE;

    senderAcc = std::unique_ptr<SenderAccount>(new SenderAccount());
    senderAcc->create(senderCfg);
    std::cout << "*** Sender account created: " << senderCfg.idUri << "\n";

    std::cout << "\n=== TEST STEP 1: Creating buddy and sending SIP MESSAGE ===\n";

    BuddyConfig buddyCfg;
    buddyCfg.uri = "sip:receiver@localhost:5060";
    buddyCfg.subscribe = false;
    buddyCfg.subscribe_dlg_event = false;

    senderBuddy = std::unique_ptr<Buddy>(new Buddy());
    senderBuddy->create(*senderAcc, buddyCfg);

    const std::string testMessage = "Hello world!";

    SendInstantMessageParam msgParam;
    msgParam.content = testMessage;
    msgParam.contentType = "text/plain";

    senderBuddy->sendInstantMessage(msgParam);
    std::cout << "*** SenderAccount: Message sent via buddy - content: " << testMessage << "\n";

    // Wait for message to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    assert(testState.messageReceived == true);
    assert(testState.receivedMessage == testMessage);
    std::cout << "✓ Message successfully received by receiver account\n";

    std::cout << "\n=== TEST STEP 2: Sending deferred response ===\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const int responseCode = 200;
    const std::string responseReason = "OK";
    receiverAcc->sendDeferredResponse(responseCode, responseReason);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    assert(testState.responseSent == true);
    assert(testState.responseReceived == true);
    assert(testState.responseStatusCode == responseCode);
    assert(testState.responseReason == responseReason);

    std::cout << "✓ Deferred response successfully sent and received\n";
    std::cout << "✓ Response code: " << testState.responseStatusCode << "\n";
    std::cout << "✓ Response reason: " << testState.responseReason << "\n";
}

void InstantMessagingTests::longContentType()
{
    testState = TestState{};

    AccountConfig receiverCfg;
    receiverCfg.idUri = "sip:receiver@localhost:5060";
    receiverCfg.sipConfig.autoRespondSipMessage = PJ_TRUE;

    receiverAcc = std::unique_ptr<ReceiverAccount>(new ReceiverAccount());
    receiverAcc->create(receiverCfg);

    AccountConfig senderCfg;
    senderCfg.idUri = "sip:sender@localhost:5060";
    senderCfg.sipConfig.autoRespondSipMessage = PJ_TRUE;

    senderAcc = std::unique_ptr<SenderAccount>(new SenderAccount());
    senderAcc->create(senderCfg);

    std::cout << "\n=== TEST: SIP MESSAGE with long Content-Type ===\n";

    BuddyConfig buddyCfg;
    buddyCfg.uri = "sip:receiver@localhost:5060";
    buddyCfg.subscribe = false;
    buddyCfg.subscribe_dlg_event = false;

    senderBuddy = std::unique_ptr<Buddy>(new Buddy());
    senderBuddy->create(*senderAcc, buddyCfg);

    const std::string testMessage = "Hello world!";
    const std::string contentType = "text/" + std::string(300, 'x');

    SendInstantMessageParam msgParam;
    msgParam.content = testMessage;
    msgParam.contentType = contentType;

    senderBuddy->sendInstantMessage(msgParam);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    assert(testState.messageReceived == true);
    assert(testState.receivedMessage == testMessage);
    assert(testState.receivedContentType == contentType);

    std::cout << "✓ Long content type successfully received\n";
}

void InstantMessagingTests::deferredResponseAfterTsxEnded()
{
    testState = TestState{};

    AccountConfig receiverCfg;
    receiverCfg.idUri = "sip:receiver@localhost:5060";
    receiverCfg.sipConfig.autoRespondSipMessage = PJ_FALSE;

    receiverAcc = std::unique_ptr<ReceiverAccount>(new ReceiverAccount());
    receiverAcc->create(receiverCfg);

    AccountConfig senderCfg;
    senderCfg.idUri = "sip:sender@localhost:5060";
    senderCfg.sipConfig.autoRespondSipMessage = PJ_TRUE;

    senderAcc = std::unique_ptr<SenderAccount>(new SenderAccount());
    senderAcc->create(senderCfg);

    std::cout << "\n=== TEST: Deferred response after the transaction ended ===\n";

    BuddyConfig buddyCfg;
    buddyCfg.uri = "sip:receiver@localhost:5060";
    buddyCfg.subscribe = false;
    buddyCfg.subscribe_dlg_event = false;

    senderBuddy = std::unique_ptr<Buddy>(new Buddy());
    senderBuddy->create(*senderAcc, buddyCfg);

    SendInstantMessageParam msgParam;
    msgParam.content = "Hello world!";
    msgParam.contentType = "text/plain";

    /* No retransmission, which would create a new transaction */
    const unsigned t1 = pjsip_cfg()->tsx.t1;
    pjsip_tsx_set_timers(30000, 0, 0, 0);
    senderBuddy->sendInstantMessage(msgParam);
    pjsip_tsx_set_timers(t1, 0, 0, 0);

    for (int i = 0; i < 50 && !testState.messageReceived; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    assert(testState.messageReceived == true);
    assert(receiverAcc->lastTsx != nullptr);

    const unsigned tsxCnt = pjsip_tsx_layer_get_tsx_count();
    pjsip_tsx_terminate(receiverAcc->lastTsx, PJSIP_SC_TSX_TRANSPORT_ERROR);

    for (int i = 0; i < 50 && pjsip_tsx_layer_get_tsx_count() >= tsxCnt; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(pjsip_tsx_layer_get_tsx_count() < tsxCnt);

    receiverAcc->sendDeferredResponse(200, "OK");

    assert(testState.responseSent == false);
    assert(testState.responseError == PJ_ENOTFOUND);

    std::cout << "✓ Deferred response rejected after the transaction ended\n";
}

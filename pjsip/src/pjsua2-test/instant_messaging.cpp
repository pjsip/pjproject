/*
 * Copyright (C) 2025 Teluu Inc. (http://www.teluu.com)
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

    InstantMessagingTests::testState.messageReceived = true;
    InstantMessagingTests::testState.receivedMessage = prm.msgBody;

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

    ep.libDestroy();
}

void InstantMessagingTests::immediateResponse()
{
    testState = TestState{};

    AccountConfig receiverCfg;
    receiverCfg.idUri = "sip:receiver@localhost:5060";
    receiverCfg.sipConfig.autoRespondSipMessage = PJ_TRUE;

    receiverAcc = PJSUA2_MAKE_UNIQUE<ReceiverAccount>();
    receiverAcc->create(receiverCfg);
    std::cout << "*** Receiver account created: " << receiverCfg.idUri << "\n";

    AccountConfig senderCfg;
    senderCfg.idUri = "sip:sender@localhost:5060";
    senderCfg.sipConfig.autoRespondSipMessage = PJ_TRUE;

    senderAcc = PJSUA2_MAKE_UNIQUE<SenderAccount>();
    senderAcc->create(senderCfg);
    std::cout << "*** Sender account created: " << senderCfg.idUri << "\n";

    std::cout << "\n=== TEST STEP 1: Creating buddy and sending SIP MESSAGE ===\n";

    BuddyConfig buddyCfg;
    buddyCfg.uri = "sip:receiver@localhost:5060";
    buddyCfg.subscribe = false;
    buddyCfg.subscribe_dlg_event = false;

    senderBuddy = PJSUA2_MAKE_UNIQUE<Buddy>();
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

    receiverAcc = PJSUA2_MAKE_UNIQUE<ReceiverAccount>();
    receiverAcc->create(receiverCfg);
    std::cout << "*** Receiver account created: " << receiverCfg.idUri << "\n";

    AccountConfig senderCfg;
    senderCfg.idUri = "sip:sender@localhost:5060";
    senderCfg.sipConfig.autoRespondSipMessage = PJ_FALSE;

    senderAcc = PJSUA2_MAKE_UNIQUE<SenderAccount>();
    senderAcc->create(senderCfg);
    std::cout << "*** Sender account created: " << senderCfg.idUri << "\n";

    std::cout << "\n=== TEST STEP 1: Creating buddy and sending SIP MESSAGE ===\n";

    BuddyConfig buddyCfg;
    buddyCfg.uri = "sip:receiver@localhost:5060";
    buddyCfg.subscribe = false;
    buddyCfg.subscribe_dlg_event = false;

    senderBuddy = PJSUA2_MAKE_UNIQUE<Buddy>();
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

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

/**
 * @file test_deferred_message_response.cpp
 * @brief Test program for PJSUA2 deferred SIP MESSAGE responses
 *
 * This test program demonstrates the new deferred SIP MESSAGE response 
 * functionality in PJSUA2. It creates two accounts where:
 * 1. First account receives SIP MESSAGE and defers the response
 * 2. Second account sends SIP MESSAGE to the first account
 * 3. First account later sends a deferred response with specific status code
 */

#include <pjsua2.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <memory>

#define THIS_FILE "test_deferred_message_response.cpp"

using namespace pj;

struct TestState {
    bool messageReceived = false;
    bool responseSent = false;
    bool responseReceived = false;
    int responseStatusCode = 0;
    std::string receivedMessage;
    std::string responseReason;
};

TestState testState;

class TestEndpoint : public Endpoint
{
public:
    using Endpoint::Endpoint;
};

/**
 * Account that receives SIP MESSAGE and defers the response
 */
class ReceiverAccount : public Account
{
private:
    DeferredResponse deferredResponse;
    
public:
    ReceiverAccount() {}
    virtual ~ReceiverAccount() {}

    /**
     * Override onInstantMessage to store deferred response
     */
    virtual void onInstantMessage(OnInstantMessageParam &prm) override
    {
        std::cout << "*** ReceiverAccount: Instant message received from " 
                  << prm.fromUri << "\n";
        std::cout << "*** Message content: " << prm.msgBody << "\n";
        
        // Store the deferred response for later use
        deferredResponse = DeferredResponse(prm);
        
        // Update test state
        testState.messageReceived = true;
        testState.receivedMessage = prm.msgBody;
        
        std::cout << "*** ReceiverAccount: Message deferred, will respond later\n";
    }
    
    /**
     * Send deferred response with specified status code and reason
     */
    void sendDeferredResponse(int statusCode, const std::string& reason)
    {
        try {
            SendInstantMessageResponseParam respParam;
            respParam.deferredResponse = std::move(deferredResponse);
            respParam.code = statusCode;
            respParam.reason = reason;
            
            sendInstantMessageResponse(respParam);
            
            testState.responseSent = true;
            std::cout << "*** ReceiverAccount: Deferred response sent - " 
                      << statusCode << " " << reason << "\n";
        } catch (Error& err) {
            std::cout << "*** ReceiverAccount: Error sending deferred response: " 
                      << err.info() << "\n";
        }
    }
};

/**
 * Account that sends SIP MESSAGE to the receiver account
 */
class SenderAccount : public Account
{
public:
    using Account::Account; 

    /**
     * Override onInstantMessageStatus to track response reception
     */
    virtual void onInstantMessageStatus(OnInstantMessageStatusParam &prm) override
    {
        std::cout << "*** SenderAccount: Message status update - "
                  << prm.code << " " << prm.reason << "\n";
        
        // Update test state
        testState.responseReceived = true;
        testState.responseStatusCode = prm.code;
        testState.responseReason = prm.reason;
    }
};


/**
 * Test runner class to manage objects properly
 */
class DeferredMessageTestRunner
{
public:
    TestEndpoint ep;
    std::unique_ptr<ReceiverAccount> receiverAcc;
    std::unique_ptr<SenderAccount> senderAcc;
    std::unique_ptr<Buddy> senderBuddy;
    
    DeferredMessageTestRunner() {}
    
    ~DeferredMessageTestRunner() = default;
    
    void runTest() {
        try {
            ep.libCreate();
            
            EpConfig epCfg;
            epCfg.logConfig.level = 4;
            ep.libInit(epCfg);
            
            TransportConfig tcfg;
            tcfg.port = 5060;
            ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
            
            ep.libStart();
            std::cout << "*** PJSUA2 STARTED ***\n";
            
            AccountConfig receiverCfg;
            receiverCfg.idUri = "sip:receiver@localhost:5060";
            receiverCfg.sipConfig.processSipMessageAsync = PJ_TRUE;
            
            receiverAcc = std::make_unique<ReceiverAccount>();
            receiverAcc->create(receiverCfg);
            std::cout << "*** Receiver account created: " << receiverCfg.idUri << "\n";

            AccountConfig senderCfg;
            senderCfg.idUri = "sip:sender@localhost:5060";
            senderCfg.sipConfig.processSipMessageAsync = PJ_TRUE; 
            
            
            senderAcc = std::make_unique<SenderAccount>();
            senderAcc->create(senderCfg);
            std::cout << "*** Sender account created: " << senderCfg.idUri << "\n";
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            std::cout << "\n=== TEST STEP 1: Creating buddy and sending SIP MESSAGE ===\n";
            
            BuddyConfig buddyCfg;
            buddyCfg.uri = "sip:receiver@localhost:5060";
            buddyCfg.subscribe = false;
            
            senderBuddy = std::make_unique<Buddy>();
            senderBuddy->create(*senderAcc, buddyCfg);
            
            const std::string testMessage = "Hello, this is a test message for deferred response!";
            
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
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            assert(testState.responseSent == true);
            assert(testState.responseReceived == true);
            assert(testState.responseStatusCode == responseCode);
            assert(testState.responseReason == responseReason);
            
            std::cout << "✓ Deferred response successfully sent and received\n";
            std::cout << "✓ Response code: " << testState.responseStatusCode << "\n";
            std::cout << "✓ Response reason: " << testState.responseReason << "\n";
            
        } catch (Error& err) {
            std::cout << "*** Test Error: " << err.info() << "\n";
            assert(false && "Test failed with exception");
        }
    }
};

int main()
{  
    try {
        DeferredMessageTestRunner testRunner;
        testRunner.runTest();
        return 0;
    } catch (...) {
        return 1;
    }
}
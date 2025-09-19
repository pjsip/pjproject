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

#ifndef INSTANT_MESSAGING_TESTS_HPP
#define INSTANT_MESSAGING_TESTS_HPP

#include <pjsua2.hpp>
#include <memory>

using namespace pj;

/**
 * Account that receives SIP MESSAGE and defers the response
 */
class ReceiverAccount : public Account
{
private:
    DeferredResponse deferredResponse;

public:
    ReceiverAccount();
    virtual ~ReceiverAccount();

    /**
     * Override onInstantMessage to store deferred response
     */
    virtual void onInstantMessage(OnInstantMessageParam &prm) override;

    /**
     * Send deferred response with specified status code and reason
     */
    void sendDeferredResponse(int statusCode, const std::string& reason);
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
    virtual
    void onInstantMessageStatus(OnInstantMessageStatusParam &prm) override;
};

/**
 * Structure to hold test state information
 */
struct TestState {
    bool messageReceived = false;
    bool responseSent = false;
    bool responseReceived = false;
    int responseStatusCode = 0;
    std::string receivedMessage{};
    std::string responseReason{};
};

/**
 * Test class for deferred SIP MESSAGE response functionality
 */
class InstantMessagingTests
{
    inline static TestState testState;
    friend class ReceiverAccount;
    friend class SenderAccount;

public:   
    InstantMessagingTests();
    InstantMessagingTests(const InstantMessagingTests&) = delete;
    InstantMessagingTests& operator=(const InstantMessagingTests&) = delete;
    InstantMessagingTests(InstantMessagingTests&&) = delete;
    InstantMessagingTests& operator=(InstantMessagingTests&&) = delete;
    ~InstantMessagingTests();

    void immediateResponse();
    void deferredResponse();
private:
    Endpoint ep;
    std::unique_ptr<ReceiverAccount> receiverAcc;
    std::unique_ptr<SenderAccount> senderAcc;
    std::unique_ptr<Buddy> senderBuddy;
};

#endif // INSTANT_MESSAGING_TESTS_HPP

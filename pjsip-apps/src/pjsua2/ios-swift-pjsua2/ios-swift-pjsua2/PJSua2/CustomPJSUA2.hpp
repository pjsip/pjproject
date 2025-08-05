/*
 * Copyright (C) 2012-2012 Teluu Inc. (http://www.teluu.com)
 * Contributed by Emre Tufekci (github.com/emretufekci)
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

#include <string>

// Ignore doxygen warning
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#include <pjsua2.hpp>
#pragma clang diagnostic pop

#include <dispatch/dispatch.h>

/**
 Create a class to be able to use it from objective-c++
 */
class PJSua2{
public:
    
    //Lib
    /**
     Create Lib with EpConfig
     */
    void createLib();
    
    /**
     Delete lib
     */
    void deleteLib();
    
    //Account
    /**
     Create Account via following config(string username, string password, string registrar, string port)
     */
    void createAccount(std::string username, std::string password,
                       std::string registrar, std::string port);
    
    /**
     Unregister account
     */
    void unregisterAccount();
    
    //Call
    /**
     Answer incoming call
     */
    void answerCall();
    
    /**
     Hangup active call (Incoming/Outgoing/Active)
     */
    void hangupCall();

    /**
     Hold the call
     */
    void holdCall();
    
    /**
     unhold the call
     */
    void unholdCall();
    
    /**
     Make outgoing call (string dest_uri) -> e.g. makeCall(sip:<SIP_USERNAME@SIP_IP:SIP_PORT>)
     */
    void outgoingCall(std::string dest_uri);

    //Call Info
    /**
     Get caller id for incoming call, checks account currently registered (ai.regIsActive)
     */
    std::string incomingCallInfo();

    //Listener
    /**
     Listener (When we have incoming call, this function pointer will notify swift.)
     */
    void incoming_call(void(*function)());

    /**
     Listener (When we have changes on the call state, this function pointer will notify swift.)
     */
    void call_listener(void(*function)(int));

    /**
     Listener (When we have changes on the acc reg state, this function pointer will notify swift.)
     */
    void acc_listener(void (*function)(bool));

    /**
     Listener (When we have video, this function pointer will notify swift.)
     */
    void update_video(void (*function)(void *));

};


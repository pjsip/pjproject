//
//  pjsua2_emre.hpp
//  ios-swift-pjsua2
//
//  Created by Emre on 7.01.2021.
//
#include <string>
#include <pjsua2.hpp>
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
     Create Account via following config(string username, string password, string ip, string port)
     */
    void createAccount(std::string username, std::string password, std::string ip, std::string port);
    
    /**
     Unregister account
     */
    void unregisterAccount();
    
    
    
    //Register State Info
    /**
     Get register state true / false
     */
    bool registerStateInfo();
    
    
    
    //Call Info
    /**
     Get caller id for incoming call, checks account currently registered (ai.regIsActive)
     */
    std::string incomingCallInfo();

    /**
     Listener (When we have incoming call, this function pointer will notify swift.)
     */
    void incoming_call(void(*function)());

    /**
     Listener (When we have changes on the call state, this function pointer will notify swift.)
     */
    void call_listener(void(*function)(int));
    
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

};


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

#include "CustomPJSUA2.hpp"
#include <iostream>
#include <list>
#include <string.h>
#include <regex>

using namespace pj;

// Split Function
std::vector<std::string> split(std::string input, std::string regExp){
    std::cout <<input <<std::endl;
    std::regex ws_re(regExp);
    std::vector<std::string> result{
        std::sregex_token_iterator(input.begin(), input.end(), ws_re, -1), {}
    };
    return result;
}

// Listen swift code via function pointers
void (*incomingCallPtr)() = 0;
void (*callStatusListenerPtr)(int) = 0;

/**
 Dispatch queue to manage ios thread serially or concurrently on app's main thread
 for more information please visit:
 https://developer.apple.com/documentation/dispatch/dispatchqueue
 */
dispatch_queue_t queue;

//Getter & Setter function
std::string callerId;
std::string onIncomingCallHeader;

bool registerState = false;


void setCallerId(std::string callerIdStr){
    callerId = callerIdStr;
}

std::string getCallerId(){
    return callerId;
}

void setRegisterState(bool registerStateBool){
    registerState = registerStateBool;
}

bool getRegisterState(){
    return registerState;
}


//Call object to manage call operations.
Call *call = NULL;


// Subclass to extend the Call and get notifications etc.
class MyCall : public Call
{
public:
    MyCall(Account &acc, int call_id = PJSUA_INVALID_ID) : Call(acc, call_id)
    { }
    ~MyCall()
    { }

    // Notification when call's state has changed.
    virtual void onCallState(OnCallStateParam &prm){
        CallInfo ci = getInfo();
        
                
        if (ci.state == PJSIP_INV_STATE_INCOMING)   {
            /**
             Since, ci.remoteUri starts with <sip:xxxx@ip:port>
             start with index 5 upto find position of character "@".
             stoi -> string to integer.
             */
            
            /**
             There is no time to fix it... Always rush...
             */
            //FIXME:: Following part seems not good. Fix it.
            std::cout<<"Ci remote uri";
            std::cout<<ci.remoteUri;
            int remoteUri = std::stoi(ci.remoteUri.substr(5, ci.remoteUri.find("@")-5));
            std::cout<<"NUMBER:"<<remoteUri;
            
        }
        
           if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
               callStatusListenerPtr(0);
               /* Delete the call */
               delete call;
               call = NULL;
           }
        
        if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
            callStatusListenerPtr(1);
        }
        
        setCallerId(ci.remoteUri);
        
        //Notify caller ID:
        PJSua2 pjsua2;
        pjsua2.incomingCallInfo();
        
    }

    // Notification when call's media state has changed.
    virtual void onCallMediaState(OnCallMediaStateParam &prm){
        CallInfo ci = getInfo();
        // Iterate all the call medias
        for (unsigned i = 0; i < ci.media.size(); i++) {
            if (ci.media[i].type==PJMEDIA_TYPE_AUDIO && getMedia(i)) {
                AudioMedia *aud_med = (AudioMedia *)getMedia(i);
                
                // Connect the call audio media to sound device
                AudDevManager& mgr = Endpoint::instance().audDevManager();
                aud_med->startTransmit(mgr.getPlaybackDevMedia());
                mgr.getCaptureDevMedia().startTransmit(*aud_med);
            }
        }
    }
};


// Subclass to extend the Account and get notifications etc.
class MyAccount : public Account {
    public:
    MyAccount() {}
    ~MyAccount()
    {
    // Invoke shutdown() first..
    shutdown();
    // ..before deleting any member objects.
    }
    
    
    // This is getting for register status!
    virtual void onRegState(OnRegStateParam &prm);
    
    // This is getting for incoming call (We can either answer or hangup the incoming call)
    virtual void onIncomingCall(OnIncomingCallParam &iprm);
};


//Creating objects
Endpoint *ep = new Endpoint;
MyAccount *acc = new MyAccount;


void MyAccount::onRegState(OnRegStateParam &prm){
    AccountInfo ai = getInfo();
    std::cout << (ai.regIsActive? "*** Register: code=" : "*** Unregister: code=") << prm.code << std::endl;
    PJSua2 pjsua2;
    setRegisterState(ai.regIsActive);
    pjsua2.registerStateInfo();

}

void MyAccount::onIncomingCall(OnIncomingCallParam &iprm) {
    
    incomingCallPtr();
    
    //Incoming DevID
    call = new MyCall(*this, iprm.callId);
    
}

EpConfig *ep_cfg = new EpConfig;

/**
 Create Lib with EpConfig
 */
void PJSua2::createLib() {
    try {
        ep->libCreate();
    } catch (Error& err){
        std::cout << "Startup error: " << err.info() << std::endl;
    }
    
    //LibInit
    try {
        ep_cfg->uaConfig.userAgent = "CUSTOM AGENT";
        ep_cfg->logConfig.level = 6;
        ep_cfg->logConfig.consoleLevel = 6;
        
        ep->libInit( *ep_cfg );
    } catch(Error& err) {
        std::cout << "Initialization error: " << err.info() << std::endl;
    }
    
    // Create SIP transport. Error handling sample is shown
    try {
    TransportConfig tcfg;
    
    //TLS
    pjsip_cfg();
    //tcfg.tlsConfig.method = PJSIP_TLSV1_3_METHOD;
    tcfg.port = 0;
    TransportId tidUDP = ep->transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
    TransportId tidTLS = ep->transportCreate(PJSIP_TRANSPORT_TLS, tcfg);

    } catch(Error& err) {
    std::cout << "Transport creation error: " << err.info() << std::endl;
    }

    // Start the library (worker threads etc)
    try { ep->libStart();
    } catch(Error& err) {
    std::cout << "Startup error: " << err.info() << std::endl;
    }
}

/**
 Delete lib
 */
void PJSua2::deleteLib() {
    
    // Here we don't have anything else to do..
    pj_thread_sleep(5000);
    
    try{
        if(acc != NULL && acc->getId()>=0){
            acc->shutdown();
            delete acc;
        }
        
        ep->libDestroy();
        delete ep;
        
    } catch (Error& err){
        std::cout<<"Error happened while destroying PJSIP lib. >>"<<err.info();
    }
    
    // Delete the account. This will unregister from server
    //delete acc;
}


/**
 Create Account via following config(string username, string password, string ip, string port)
 */
void PJSua2::createAccount(
                           std::string username, std::string password, std::string ip, std::string port,
                           std::string stunIp, std::string stunPort, std::string turnUsername, std::string turnPassword, std::string turnIp, std::string turnPort, bool stunEnabled,
                           bool turnEnabled, bool tlsEnabled, bool iceEnabled
                           ) {
    
    
    ep_cfg->uaConfig.stunServer.push_back(stunIp+":"+stunPort);
    
    UaConfig ua_cfg = ep_cfg->uaConfig;

    
    AccountConfig acfg;
    ep_cfg->uaConfig.userAgent = "PJSUA2 SWIFT CLIENT";

    
    
    //NAT CONFIGs
    acfg.natConfig.iceEnabled = PJ_TRUE;
    
    //TLS CONFIG
    if(tlsEnabled){
        acfg.sipConfig.proxies.push_back("sip:" + ip + ";hide;transport=tls");
    }
    
    // Ice Configs
    if(iceEnabled == true){
        //ICE
        acfg.natConfig.iceMaxHostCands = -1;
        
        //STUN
        acfg.natConfig.sipStunUse = PJSUA_STUN_USE_DEFAULT;
        acfg.natConfig.mediaStunUse = PJSUA_STUN_RETRY_ON_FAILURE;
        ep->natUpdateStunServers(ep_cfg->uaConfig.stunServer, PJ_FALSE);
        
        //TURN
        acfg.natConfig.turnEnabled = PJ_TRUE;
        acfg.natConfig.turnServer = turnIp+":"+turnPort;
        acfg.natConfig.turnUserName = turnUsername;
        acfg.natConfig.turnPassword = turnPassword;
        
    }
    else if(turnEnabled == true){
        // Turn Configs
        acfg.natConfig.turnEnabled = turnEnabled;
        acfg.natConfig.sipStunUse = PJSUA_STUN_USE_DISABLED;
        acfg.natConfig.mediaStunUse = PJSUA_STUN_USE_DISABLED;
        acfg.natConfig.iceMaxHostCands = 0;
        acfg.natConfig.turnEnabled = PJ_TRUE;
        
        if(!turnIp.empty() && turnPort.empty()) {
            acfg.natConfig.turnServer = turnIp+":"+turnPort;
            acfg.natConfig.turnUserName = turnUsername;
            acfg.natConfig.turnPassword = turnPassword;
        }
    }
    else if (stunEnabled == true){
        acfg.natConfig.sipStunUse = PJSUA_STUN_USE_DEFAULT;
        acfg.natConfig.mediaStunUse = PJSUA_STUN_RETRY_ON_FAILURE;
        acfg.natConfig.iceMaxHostCands = -1;
        
        //bj_bzero -> std::fill
        std::fill(EpConfig().uaConfig.stunServer.begin(), EpConfig().uaConfig.stunServer.end(), 0);
        ep->natUpdateStunServers(EpConfig().uaConfig.stunServer, PJ_FALSE);
    
    }
    else {
        acfg.natConfig.iceEnabled = PJ_FALSE;
        acfg.natConfig.turnEnabled = PJ_FALSE;
        acfg.natConfig.sipStunUse = PJSUA_STUN_USE_DISABLED;
        acfg.natConfig.mediaStunUse = PJSUA_STUN_USE_DISABLED;
    }
    

    // Configure an AccountConfig
    acfg.idUri = "sip:" + username + "@" + ip + ":" + port;
    acfg.regConfig.registrarUri = "sip:" + ip + ":" + port;
    AuthCredInfo cred("digest", "*", username, 0, password);
    acfg.sipConfig.authCreds.push_back(cred);
    
    
    

    //  TODO:: GET ID -1 IS EXPERIMENTAL, I'M NOT SURE THAT, IT IS GOOD WAY TO CHECK ACC IS CREATED. FIX IT!
    if(acc->getId() == -1){
        std::cout<<"FIRST TIME HERE";
        // Create the account
        try {
            acc->create(acfg);
        } catch(Error& err) {
            std::cout << "Account creation error: " << err.info() << std::endl;
        }
    }else {
        // Modify the account
        try {
            //Update the registration
            acc->modify(acfg);
            acc->setRegistration(true);
        } catch(Error& err) {
            std::cout << "Account modify error: " << err.info() << std::endl;
        }
    }
}


/**
 Unregister account
 */
void PJSua2::unregisterAccount() {

    if(acc != NULL && acc->getId()>=0){
        acc->setRegistration(false);
    }
    
}


/**
 Get register state true / false
 */
bool PJSua2::registerStateInfo(){
    return getRegisterState();
}


/**
 Get caller id for incoming call, checks account currently registered (ai.regIsActive)
 */
std::string PJSua2::incomingCallInfo() {
    return getCallerId();
}


/**
 Listener (When we have incoming call, this function pointer will notify swift.)
 */
void PJSua2::incoming_call(void (* funcpntr)()){
    incomingCallPtr = funcpntr;
}


/**
 Listener (When we have changes on the call state, this function pointer will notify swift.)
 */
void PJSua2::call_listener(void (* funcpntr)(int)){
    callStatusListenerPtr = funcpntr;
}


/**
 Answer incoming call
 */
void PJSua2::answerCall(){
    CallOpParam op;
    op.statusCode = PJSIP_SC_OK;
    call->answer(op);
}


/**
 Hangup active call (Incoming/Outgoing/Active)
 */
void PJSua2::hangupCall(){
    
    if (call != NULL) {
        CallOpParam op;
        op.statusCode = PJSIP_SC_DECLINE;
        call->hangup(op);
        delete call;
        call = NULL;
    }
}

/**
 Hold the call
 */
void PJSua2::holdCall(){
    
    if (call != NULL) {
        CallOpParam op;
        
        try {
            call->setHold(op);
        } catch(Error& err) {
            std::cout << "Hold error: " << err.info() << std::endl;
        }
    }
    
}

/**
 Unhold the call
 */
void PJSua2::unholdCall(){

    if (call != NULL) {
        
        CallOpParam op;
        op.opt.flag=PJSUA_CALL_UNHOLD;
        
        try {
            call->reinvite(op);
        } catch(Error& err) {
            std::cout << "Unhold/Reinvite error: " << err.info() << std::endl;
        }
    }
    
}
/**
 Make outgoing call (string dest_uri) -> e.g. makeCall(sip:<SIP_USERNAME@SIP_IP:SIP_PORT>)
 */
void PJSua2::outgoingCall(std::string dest_uri) {
    CallOpParam prm(true); // Use default call settings
    
    try {
    call = new MyCall(*acc);
    call->makeCall(dest_uri, prm);
    } catch(Error& err) {
    std::cout << err.info() << std::endl;
    }
}




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

using namespace pj;

class MyEndpoint : public Endpoint
{
public:
    virtual void onTimer(const OnTimerParam &prm);
};

// Subclass to extend the Account and get notifications etc.
class MyAccount : public Account
{
public:
    std::string dest_uri;

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

// Subclass to extend the Call and get notifications etc.
class MyCall : public Call
{
public:
    MyCall(Account &acc, int call_id = PJSUA_INVALID_ID)
        : Call(acc, call_id)
    { }
    ~MyCall()
    { }

    // Notification when call's state has changed.
    virtual void onCallState(OnCallStateParam &prm);

    // Notification when call's media state has changed.
    virtual void onCallMediaState(OnCallMediaStateParam &prm);
};

enum {
    MAKE_CALL = 1,
    ANSWER_CALL = 2,
    HOLD_CALL = 3,
    UNHOLD_CALL = 4,
    HANGUP_CALL = 5
};

Call *call = NULL;
Endpoint *ep = NULL;
MyAccount *acc = NULL;

// Listen swift code via function pointers
void (*incomingCallPtr)() = 0;
void (*callStatusListenerPtr)(int) = 0;
void (*accStatusListenerPtr)(bool) = 0;
void (*updateVideoPtr)(void *) = 0;

//Getter & Setter function
std::string callerId;

void MyEndpoint::onTimer(const OnTimerParam &prm)
{
    /* IMPORTANT:
     * We need to call PJSIP API from a separate thread since
     * PJSIP API can potentially block the main/GUI thread.
     * And make sure we don't use Apple's Dispatch / gcd since
     * it's incompatible with POSIX threads.
     * In this example, we take advantage of PJSUA2's timer thread
     * to perform call operations. For a more complex application,
     * it is recommended to create your own separate thread
     * instead for this purpose.
     */
    long code = (long) prm.userData;
    if (code == MAKE_CALL) {
        CallOpParam prm(true); // Use default call settings
        prm.opt.videoCount = 1;
        try {
            call = new MyCall(*acc);
            call->makeCall(acc->dest_uri, prm);
        } catch(Error& err) {
            std::cout << err.info() << std::endl;
        }
    } else if (code == ANSWER_CALL) {
        CallOpParam op(true);
        op.statusCode = PJSIP_SC_OK;
        call->answer(op);
    } else if (code == HOLD_CALL) {
        if (call != NULL) {
            CallOpParam op(true);
            
            try {
                call->setHold(op);
            } catch(Error& err) {
                std::cout << "Hold error: " << err.info() << std::endl;
            }
        }
    } else if (code == UNHOLD_CALL) {
        if (call != NULL) {
            CallOpParam op(true);
            op.opt.flag = PJSUA_CALL_UNHOLD;
            
            try {
                call->reinvite(op);
            } catch(Error& err) {
                std::cout << "Unhold/Reinvite error: " << err.info() << std::endl;
            }
        }
    } else if (code == HANGUP_CALL) {
        if (call != NULL) {
            CallOpParam op(true);
            op.statusCode = PJSIP_SC_DECLINE;
            call->hangup(op);
            delete call;
            call = NULL;
        }
    }
}

void MyAccount::onRegState(OnRegStateParam &prm)
{
    AccountInfo ai = getInfo();
    std::cout << (ai.regIsActive? "*** Register: code=" : "*** Unregister: code=")
              << prm.code << std::endl;
    accStatusListenerPtr(ai.regIsActive);
}

void MyAccount::onIncomingCall(OnIncomingCallParam &iprm)
{
    incomingCallPtr();
    call = new MyCall(*this, iprm.callId);
}

void setCallerId(std::string callerIdStr)
{
    callerId = callerIdStr;
}

std::string getCallerId()
{
    return callerId;
}

void MyCall::onCallState(OnCallStateParam &prm)
{
    CallInfo ci = getInfo();
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        callStatusListenerPtr(0);
        
        /* Delete the call */
        delete call;
        call = NULL;
        return;
    }
    
    setCallerId(ci.remoteUri);
    
    if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        callStatusListenerPtr(1);
    }
    
    //Notify caller ID:
    PJSua2 pjsua2;
    pjsua2.incomingCallInfo();
}

void MyCall::onCallMediaState(OnCallMediaStateParam &prm)
{
    CallInfo ci = getInfo();
    // Iterate all the call medias
    for (unsigned i = 0; i < ci.media.size(); i++) {
        if (ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE ||
            ci.media[i].status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
        {
            if (ci.media[i].type==PJMEDIA_TYPE_AUDIO) {
                AudioMedia *aud_med = (AudioMedia *)getMedia(i);
                
                // Connect the call audio media to sound device
                AudDevManager& mgr = Endpoint::instance().audDevManager();
                aud_med->startTransmit(mgr.getPlaybackDevMedia());
                mgr.getCaptureDevMedia().startTransmit(*aud_med);
            } else if (ci.media[i].type==PJMEDIA_TYPE_VIDEO) {
                void *window = ci.media[i].videoWindow.getInfo().winHandle.handle.window;
                updateVideoPtr(window);
            }
        }
    }
}

/**
 Create Lib with EpConfig
 */
void PJSua2::createLib()
{
    ep = new MyEndpoint;

    try {
        ep->libCreate();
    } catch (Error& err){
        std::cout << "Startup error: " << err.info() << std::endl;
    }

    //LibInit
    try {
        EpConfig ep_cfg;
        ep->libInit( ep_cfg );
    } catch(Error& err) {
        std::cout << "Initialization error: " << err.info() << std::endl;
    }

    // Create SIP transport
    try {
        TransportConfig tcfg;
        tcfg.port = 5060;
        ep->transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
    } catch(Error& err) {
        std::cout << "Transport creation error: " << err.info() << std::endl;
    }

    // Start the library (worker threads etc)
    try {
        ep->libStart();
    } catch(Error& err) {
        std::cout << "Startup error: " << err.info() << std::endl;
    }
}

/**
 Delete lib
 */
void PJSua2::deleteLib()
{
    // Here we don't have anything else to do..
    pj_thread_sleep(500);
    
    // Delete the account. This will unregister from server
    delete acc;
    
    ep->libDestroy();
    delete ep;
}

/**
 Create Account via following config(string username, string password, string ip, string port)
 */
void PJSua2::createAccount(std::string username, std::string password,
                           std::string registrar, std::string port)
{
    // Configure an AccountConfig
    AccountConfig acfg;
    acfg.idUri = "sip:" + username + "@" + registrar + ":" + port;;
    acfg.regConfig.registrarUri = "sip:" + registrar + ":" + port;;
    AuthCredInfo cred("digest", "*", username, 0, password);
    acfg.sipConfig.authCreds.push_back(cred);

    acfg.videoConfig.autoShowIncoming = true;
    acfg.videoConfig.autoTransmitOutgoing = true;
    
    if (!acc) {
        // Create the account
        acc = new MyAccount;
        try {
            acc->create(acfg);
        } catch(Error& err) {
            std::cout << "Account creation error: " << err.info() << std::endl;
        }
    } else {
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
void PJSua2::unregisterAccount()
{
    acc->setRegistration(false);
}

/**
 Make outgoing call (string dest_uri) -> e.g. makeCall(sip:<SIP_USERNAME@SIP_IP:SIP_PORT>)
 */
void PJSua2::outgoingCall(std::string dest_uri)
{
    acc->dest_uri = dest_uri;
    ep->utilTimerSchedule(0, (Token)MAKE_CALL);
}

/**
 Answer incoming call
 */
void PJSua2::answerCall()
{
    ep->utilTimerSchedule(0, (Token)ANSWER_CALL);
}

/**
 Hangup active call (Incoming/Outgoing/Active)
 */
void PJSua2::hangupCall()
{
    ep->utilTimerSchedule(0, (Token)HANGUP_CALL);
}

/**
 Hold the call
 */
void PJSua2::holdCall()
{
    ep->utilTimerSchedule(0, (Token)HOLD_CALL);
}

/**
 Unhold the call
 */
void PJSua2::unholdCall()
{
    ep->utilTimerSchedule(0, (Token)UNHOLD_CALL);
}

/**
 Get caller id for incoming call, checks account currently registered (ai.regIsActive)
 */
std::string PJSua2::incomingCallInfo()
{
    return getCallerId();
}

/**
 Listener (When we have incoming call, this function pointer will notify swift.)
 */
void PJSua2::incoming_call(void (* funcpntr)())
{
    incomingCallPtr = funcpntr;
}

/**
 Listener (When we have changes on the call state, this function pointer will notify swift.)
 */
void PJSua2::call_listener(void (* funcpntr)(int))
{
    callStatusListenerPtr = funcpntr;
}

/**
 Listener (When we have changes on the acc reg state, this function pointer will notify swift.)
 */
void PJSua2::acc_listener(void (* funcpntr)(bool))
{
    accStatusListenerPtr = funcpntr;
}

/**
 Listener (When we have video, this function pointer will notify swift.)
 */
void PJSua2::update_video(void (*funcpntr)(void *))
{
    updateVideoPtr = funcpntr;
}

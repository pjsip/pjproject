/* $Id$ */
/*
 * Copyright (C) 2008-2013 Teluu Inc. (http://www.teluu.com)
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
#include <pjsua2.hpp>
#include <iostream>
#include <memory>
#include <pj/file_access.h>

using namespace pj;

class MyAccount;

class MyCall : public Call
{
private:
    MyAccount *myAcc;

public:
    MyCall(Account &acc, int call_id = PJSUA_INVALID_ID)
    : Call(acc, call_id)
    {
        myAcc = (MyAccount *)&acc;
    }
    
    virtual void onCallState(OnCallStateParam &prm);
};

class MyAccount : public Account
{
public:
    std::vector<Call *> calls;
    
public:
    MyAccount()
    {}

    ~MyAccount()
    {
        std::cout << "*** Account is being deleted: No of calls="
                  << calls.size() << std::endl;
    }
    
    void removeCall(Call *call)
    {
        for (std::vector<Call *>::iterator it = calls.begin();
             it != calls.end(); ++it)
        {
            if (*it == call) {
                calls.erase(it);
                break;
            }
        }
    }

    virtual void onRegState(OnRegStateParam &prm)
    {
	AccountInfo ai = getInfo();
	std::cout << (ai.regIsActive? "*** Register: code=" : "*** Unregister: code=")
		  << prm.code << std::endl;
    }
    
    virtual void onIncomingCall(OnIncomingCallParam &iprm)
    {
        Call *call = new MyCall(*this, iprm.callId);
        CallInfo ci = call->getInfo();
        CallOpParam prm;
        
        std::cout << "*** Incoming Call: " <<  ci.remoteUri << " ["
                  << ci.stateText << "]" << std::endl;
        
        calls.push_back(call);
        prm.statusCode = (pjsip_status_code)200;
        call->answer(prm);
    }
};

void MyCall::onCallState(OnCallStateParam &prm)
{
    CallInfo ci = getInfo();
    std::cout << "*** Call: " <<  ci.remoteUri << " [" << ci.stateText
              << "]" << std::endl;
    
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        myAcc->removeCall(this);
        /* Delete the call */
        delete this;
    }
}

static void mainProg1() throw(Error)
{
    Endpoint ep;

    // Create library
    ep.libCreate();

    // Init library
    EpConfig ep_cfg;
    ep_cfg.logConfig.level = 4;
    ep.libInit( ep_cfg );

    // Transport
    TransportConfig tcfg;
    tcfg.port = 5060;
    ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);

    // Start library
    ep.libStart();
    std::cout << "*** PJSUA2 STARTED ***" << std::endl;

    // Add account
    AccountConfig acc_cfg;
    acc_cfg.idUri = "sip:test1@pjsip.org";
    acc_cfg.regConfig.registrarUri = "sip:pjsip.org";
    acc_cfg.sipConfig.authCreds.push_back( AuthCredInfo("digest", "*",
                                                        "test1", 0, "test1") );
    std::auto_ptr<MyAccount> acc(new MyAccount);
    acc->create(acc_cfg);
    
    pj_thread_sleep(2000);
    
    // Make outgoing call
    Call *call = new MyCall(*acc);
    acc->calls.push_back(call);
    CallOpParam prm(true);
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    call->makeCall("sip:test1@pjsip.org", prm);
    
    // Hangup all calls
    pj_thread_sleep(8000);
    ep.hangupAllCalls();
    pj_thread_sleep(4000);
    
    // Destroy library
    std::cout << "*** PJSUA2 SHUTTING DOWN ***" << std::endl;
}

void mainProg2() throw(Error)
{
    Endpoint ep;

    // Create library
    ep.libCreate();

    string json_str;

    {
	EpConfig epCfg;
	JsonDocument jDoc;

	epCfg.uaConfig.maxCalls = 61;
	epCfg.uaConfig.userAgent = "Just JSON Test";
	epCfg.uaConfig.stunServer.push_back("stun1.pjsip.org");
	epCfg.uaConfig.stunServer.push_back("stun2.pjsip.org");
	epCfg.logConfig.filename = "THE.LOG";

	jDoc.writeObject(epCfg);
	json_str = jDoc.saveString();
	std::cout << json_str << std::endl << std::endl;
    }

    {
	EpConfig epCfg;
	JsonDocument rDoc;
	string output;

	rDoc.loadString(json_str);
	rDoc.readObject(epCfg);

	JsonDocument wDoc;

	wDoc.writeObject(epCfg);
	json_str = wDoc.saveString();
	std::cout << json_str << std::endl << std::endl;

	wDoc.saveFile("jsontest.js");
    }

    {
	EpConfig epCfg;
	JsonDocument rDoc;

	rDoc.loadFile("jsontest.js");
	rDoc.readObject(epCfg);
	pj_file_delete("jsontest.js");
    }

    ep.libDestroy();
}

void mainProg() throw(Error)
{
    Endpoint ep;

    // Create library
    ep.libCreate();

    string json_str;

    {
	JsonDocument jdoc;
	AccountConfig accCfg;

	accCfg.idUri = "\"Just Test\" <sip:test@pjsip.org>";
	accCfg.regConfig.registrarUri = "sip:pjsip.org";
	SipHeader h;
	h.hName = "X-Header";
	h.hValue = "User header";
	accCfg.regConfig.headers.push_back(h);

	accCfg.sipConfig.proxies.push_back("<sip:sip.pjsip.org;transport=tcp>");
	accCfg.sipConfig.proxies.push_back("<sip:sip.pjsip.org;transport=tls>");

	accCfg.mediaConfig.transportConfig.tlsConfig.ciphers.push_back(1);
	accCfg.mediaConfig.transportConfig.tlsConfig.ciphers.push_back(2);
	accCfg.mediaConfig.transportConfig.tlsConfig.ciphers.push_back(3);

	AuthCredInfo aci;
	aci.scheme = "digest";
	aci.username = "test";
	aci.data = "passwd";
	aci.realm = "*";
	accCfg.sipConfig.authCreds.push_back(aci);

	jdoc.writeObject(accCfg);
	json_str = jdoc.saveString();
	std::cout << "Original:" << std::endl;
	std::cout << json_str << std::endl << std::endl;
    }

    {
	JsonDocument rdoc;

	rdoc.loadString(json_str);
	AccountConfig accCfg;
	rdoc.readObject(accCfg);

	JsonDocument wdoc;
	wdoc.writeObject(accCfg);
	json_str = wdoc.saveString();

	std::cout << "Parsed:" << std::endl;
	std::cout << json_str << std::endl << std::endl;
    }

    ep.libDestroy();
}

int main()
{
    int ret = 0;

    try {
	mainProg1();
	std::cout << "Success" << std::endl;
    } catch (Error & err) {
	std::cout << "Exception: " << err.info() << std::endl;
	ret = 1;
    }

    return ret;
}



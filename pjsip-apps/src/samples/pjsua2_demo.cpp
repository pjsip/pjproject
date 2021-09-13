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
#include <pj/file_access.h>

#define THIS_FILE 	"pjsua2_demo.cpp"

using namespace pj;

/* Valid test number:
 * 0: JSON account config test
 * 1: call test
 * 2: JSON endpoint config test
 * 3: media player and recorder test
 * 4: simple registration test
 */
#define USE_TEST 1

class MyAccount;

class MyCall : public Call
{
private:
    MyAccount *myAcc;
    AudioMediaPlayer *wav_player;

public:
    MyCall(Account &acc, int call_id = PJSUA_INVALID_ID)
    : Call(acc, call_id)
    {
    	wav_player = NULL;
        myAcc = (MyAccount *)&acc;
    }
    
    ~MyCall()
    {
    	if (wav_player)
    	    delete wav_player;
    }
    
    virtual void onCallState(OnCallStateParam &prm);
    virtual void onCallTransferRequest(OnCallTransferRequestParam &prm);
    virtual void onCallReplaced(OnCallReplacedParam &prm);
    virtual void onCallMediaState(OnCallMediaStateParam &prm);
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

	for (std::vector<Call *>::iterator it = calls.begin();
             it != calls.end(); )
        {
	    delete (*it);
	    it = calls.erase(it);
        }
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
    PJ_UNUSED_ARG(prm);

    CallInfo ci = getInfo();
    std::cout << "*** Call: " <<  ci.remoteUri << " [" << ci.stateText
              << "]" << std::endl;
    
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        //myAcc->removeCall(this);
        /* Delete the call */
        //delete this;
    }
}

void MyCall::onCallMediaState(OnCallMediaStateParam &prm)
{
    PJ_UNUSED_ARG(prm);

    CallInfo ci = getInfo();
    AudioMedia aud_med;
    AudioMedia& play_dev_med =
    	Endpoint::instance().audDevManager().getPlaybackDevMedia();

    try {
    	// Get the first audio media
    	aud_med = getAudioMedia(-1);
    } catch(...) {
	std::cout << "Failed to get audio media" << std::endl;
	return;
    }

    if (!wav_player) {
    	wav_player = new AudioMediaPlayer();
   	try {
   	    wav_player->createPlayer(
   	    	"../../../../tests/pjsua/wavs/input.16.wav", 0);
   	} catch (...) {
	    std::cout << "Failed opening wav file" << std::endl;
	    delete wav_player;
	    wav_player = NULL;
    	}
    }

    // This will connect the wav file to the call audio media
    if (wav_player)
    	wav_player->startTransmit(aud_med);

    // And this will connect the call audio media to the sound device/speaker
    aud_med.startTransmit(play_dev_med);
}

void MyCall::onCallTransferRequest(OnCallTransferRequestParam &prm)
{
    /* Create new Call for call transfer */
    prm.newCall = new MyCall(*myAcc);
}

void MyCall::onCallReplaced(OnCallReplacedParam &prm)
{
    /* Create new Call for call replace */
    prm.newCall = new MyCall(*myAcc, prm.newCallId);
}


#if USE_TEST == 1
static void mainProg1(Endpoint &ep)
{
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
    acc_cfg.regConfig.registrarUri = "sip:sip.pjsip.org";
    acc_cfg.sipConfig.authCreds.push_back( AuthCredInfo("digest", "*",
                                                        "test1", 0, "test1") );
    MyAccount *acc(new MyAccount);
    try {
	acc->create(acc_cfg);
    } catch (...) {
	std::cout << "Adding account failed" << std::endl;
    }
    
    pj_thread_sleep(2000);
    
    // Make outgoing call
    Call *call = new MyCall(*acc);
    acc->calls.push_back(call);
    CallOpParam prm(true);
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    call->makeCall("sip:test1@pjsip.org", prm);
    
    // Hangup all calls
    pj_thread_sleep(4000);
    ep.hangupAllCalls();
    pj_thread_sleep(4000);
    
    // Destroy library
    std::cout << "*** PJSUA2 SHUTTING DOWN ***" << std::endl;
    delete acc; /* Will delete all calls too */
}
#endif


#if USE_TEST == 2
static void mainProg2()
{
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
}
#endif


#if USE_TEST == 3
static void mainProg3(Endpoint &ep)
{
    const char *paths[] = { "../../../../tests/pjsua/wavs/input.16.wav",
			    "../../tests/pjsua/wavs/input.16.wav",
			    "input.16.wav"};
    unsigned i;
    const char *filename = NULL;

    // Init library
    EpConfig ep_cfg;
    ep.libInit( ep_cfg );

    for (i=0; i<PJ_ARRAY_SIZE(paths); ++i) {
       if (pj_file_exists(paths[i])) {
          filename = paths[i];
          break;
       }
    }

    if (!filename) {
	PJSUA2_RAISE_ERROR3(PJ_ENOTFOUND, "mainProg3()",
			   "Could not locate input.16.wav");
    }

    // Start library
    ep.libStart();
    std::cout << "*** PJSUA2 STARTED ***" << std::endl;

    /* Use Null Audio Device as main media clock. This is useful for improving
     * media clock (see also https://trac.pjsip.org/repos/wiki/FAQ#tx-timing)
     * especially when sound device clock is jittery.
     */
    ep.audDevManager().setNullDev();

    /* And install sound device using Extra Audio Device */
    ExtraAudioDevice auddev2(-1, -1);
    try {
	auddev2.open();
    } catch (...) {
	std::cout << "Extra sound device failed" << std::endl;
    }

    // Create player and recorder
    {
	AudioMediaPlayer amp;
	amp.createPlayer(filename);

	AudioMediaRecorder amr;
	amr.createRecorder("recorder_test_output.wav");

	amp.startTransmit(amr);
	if (auddev2.isOpened())
	    amp.startTransmit(auddev2);

	pj_thread_sleep(5000);
    }
}
#endif


#if USE_TEST == 0
static void mainProg()
{
    string json_str;

    {
	JsonDocument jdoc;
	AccountConfig accCfg;

	accCfg.idUri = "\"Just Test\" <sip:test@pjsip.org>";
	accCfg.regConfig.registrarUri = "sip:sip.pjsip.org";
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
}
#endif


#if USE_TEST == 4
static void mainProg4(Endpoint &ep)
{
    // Init library
    EpConfig ep_cfg;
    ep.libInit( ep_cfg );

    // Create transport
    TransportConfig tcfg;
    tcfg.port = 5060;
    ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
    ep.transportCreate(PJSIP_TRANSPORT_TCP, tcfg);

    // Add account
    AccountConfig acc_cfg;
    acc_cfg.idUri = "sip:localhost";
    MyAccount *acc(new MyAccount);
    acc->create(acc_cfg);

    // Start library
    ep.libStart();
    std::cout << "*** PJSUA2 STARTED ***" << std::endl;

    // Just wait for ENTER key
    std::cout << "Press ENTER to quit..." << std::endl;
    std::cin.get();

    delete acc;
}
#endif


int main()
{
    int ret = 0;
    Endpoint ep;

    try {
	ep.libCreate();

#if USE_TEST == 0
	mainProg(ep);
#endif
#if USE_TEST == 1
	mainProg1(ep);
#endif
#if USE_TEST == 2
	mainProg2(ep);
#endif
#if USE_TEST == 3
	mainProg3(ep);
#endif
#if USE_TEST == 4
	mainProg4(ep);
#endif

	ret = PJ_SUCCESS;
    } catch (Error & err) {
	std::cout << "Exception: " << err.info() << std::endl;
	ret = 1;
    }

    try {
	ep.libDestroy();
    } catch(Error &err) {
	std::cout << "Exception: " << err.info() << std::endl;
	ret = 1;
    }

    if (ret == PJ_SUCCESS) {
	std::cout << "Success" << std::endl;
    } else {
	std::cout << "Error Found" << std::endl;
    }

    return ret;
}



/* $Id$ */
/*
* Copyright (C) 2016 Teluu Inc. (http://www.teluu.com)
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

#include "MyApp.h"
#include <memory>
#include <winerror.h>
#include <pplawait.h>

using namespace VoipBackEnd;
using namespace Windows::Storage;

const std::string CONFIG_NAME = "pjsua2.json";
const int SIP_PORT  = 5060;
const int LOG_LEVEL = 5;
const std::string THIS_FILE = "MyApp.cpp";

#define GenHresultFromITF(x) (MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, x))

MyAppRT^ MyAppRT::singleton = nullptr;

#define DEFAULT_DESTRUCTOR() \
    delete inPtr;

#define CHECK_EXCEPTION(expr) \
	do { \
	    try { \
		(expr); 	\
	    } catch (pj::Error e) { \
		Platform::Exception^ exp = ref new Platform::COMException( \
		    GenHresultFromITF(e.status)); \
	    } \
	} while (0)

///////////////////////////////////////////////////////////////////////////////
std::string make_string(const std::wstring& wstring)
{
    auto wideData = wstring.c_str();
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideData, 
					 -1, nullptr, 0, NULL, NULL);

    std::unique_ptr<char[]> utf8;
    utf8.reset(new char[bufferSize]);    

    if (WideCharToMultiByte(CP_UTF8, 0, wideData, -1,
	utf8.get(), bufferSize, NULL, NULL) == 0) 
    {	
	return std::string();
    }

    return std::string(utf8.get());    
}

std::wstring make_wstring(const std::string& string)
{
    auto utf8Data = string.c_str();
    int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8Data, -1, nullptr, 0);

    std::unique_ptr<wchar_t[]> wide;
    wide.reset(new wchar_t[bufferSize]);

    if (MultiByteToWideChar(CP_UTF8, 0, utf8Data, -1,
	wide.get(), bufferSize) == 0)
    {	
	return std::wstring();
    }	    

    return std::wstring(wide.get());;
}

std::string to_std_str(Platform::String ^in_str)
{
    std::wstring wsstr(in_str->Data());
    return make_string(wsstr);
}

Platform::String^ to_platform_str(const std::string& in_str)
{
    std::wstring wsstr = make_wstring(in_str);
    return ref new Platform::String(wsstr.c_str(), wsstr.length());
}

///////////////////////////////////////////////////////////////////////////////
void ImpLogWriter::write(const pj::LogEntry &entry)
{
    std::wstring msg = make_wstring(entry.msg) + L"\r\n";

    ::OutputDebugString(msg.c_str());    
}
///////////////////////////////////////////////////////////////////////////////
OnRegStateParamRT::OnRegStateParamRT(const pj::OnRegStateParam& param)

{
    inPtr = new pj::OnRegStateParam(param);
}

OnRegStateParamRT::OnRegStateParamRT()
{
    inPtr = new pj::OnRegStateParam();
}

OnRegStateParamRT::~OnRegStateParamRT()
{
    DEFAULT_DESTRUCTOR();
}

pj_status_t OnRegStateParamRT::status::get()
{
    return inPtr->status;
}

unsigned OnRegStateParamRT::code::get()
{
    return static_cast<unsigned>(inPtr->code);
}

Platform::String^ OnRegStateParamRT::reason::get()
{
    return to_platform_str(inPtr->reason);
}

int OnRegStateParamRT::expiration::get()
{
    return inPtr->expiration;
}
///////////////////////////////////////////////////////////////////////////////
void ImpAccount::setCallback(IntAccount^ callback)
{
    cb = callback;
}

void ImpAccount::onIncomingCall(pj::OnIncomingCallParam& prm)
{
    if (cb) {
	if (MyAppRT::Instance->handleIncomingCall(prm.callId)) {
	    CallInfoRT^ info = MyAppRT::Instance->getCallInfo();
	    if (info != nullptr)
		cb->onIncomingCall(info);
	}
    }
}


void ImpAccount::onRegState(pj::OnRegStateParam& prm)
{
    if (cb)
	cb->onRegState(ref new OnRegStateParamRT(prm));
}

///////////////////////////////////////////////////////////////////////////////
CallInfoRT::CallInfoRT(const pj::CallInfo& info)
{
    inPtr = new pj::CallInfo(info);
}

Platform::String^ CallInfoRT::localUri::get()
{
    return to_platform_str(inPtr->localUri);
}

Platform::String^ CallInfoRT::localContact::get()
{
    return to_platform_str(inPtr->localContact);
}

Platform::String^ CallInfoRT::remoteUri::get()
{
    return to_platform_str(inPtr->remoteUri);
}

Platform::String^ CallInfoRT::remoteContact::get()
{
    return to_platform_str(inPtr->remoteContact);
}

INV_STATE CallInfoRT::state::get()
{
    return static_cast<INV_STATE>(inPtr->state);
}

CallInfoRT::~CallInfoRT()
{
    DEFAULT_DESTRUCTOR();
}
///////////////////////////////////////////////////////////////////////////////
CallOpParamRT::CallOpParamRT()
{
    inPtr = new pj::CallOpParam(true);
}

unsigned CallOpParamRT::statusCode::get() 
{
    return static_cast<unsigned>(inPtr->statusCode);
}

void CallOpParamRT::statusCode::set(unsigned val) 
{
    inPtr->statusCode = static_cast<pjsip_status_code>(val);
}

Platform::String^ CallOpParamRT::reason::get() 
{
    return to_platform_str(inPtr->reason);
}

void CallOpParamRT::reason::set(Platform::String^ val) 
{
    inPtr->reason = to_std_str(val);
}

pj::CallOpParam* CallOpParamRT::getPtr()
{
    return inPtr;
}

CallOpParamRT::~CallOpParamRT()
{
    DEFAULT_DESTRUCTOR();
}

///////////////////////////////////////////////////////////////////////////////
ImpCall::ImpCall(pj::Account& account, int call_id) : pj::Call(account,call_id)
{};

void ImpCall::setCallback(IntCall^ callback)
{
    cb = callback;
}

void ImpCall::onCallState(pj::OnCallStateParam& prm)
{
    if (cb) {
	pj::CallInfo info = getInfo();	
	cb->onCallState(ref new CallInfoRT(info));
    }
};

void ImpCall::onCallMediaState(pj::OnCallMediaStateParam& prm)
{
    pj::CallInfo info;    
    try {
	info = getInfo();
    } catch (pj::Error& e) {
	MyAppRT::Instance->writeLog(2, to_platform_str(e.info()));
	return;
    }

    for (unsigned i = 0; i < info.media.size(); i++) {
	pj::CallMediaInfo med_info = info.media[i];
	if ((med_info.type == PJMEDIA_TYPE_AUDIO) &&
	    ((med_info.status == PJSUA_CALL_MEDIA_ACTIVE) ||
	    (med_info.status == PJSUA_CALL_MEDIA_REMOTE_HOLD))
	    )
	{
	    pj::Media* m = getMedia(i);
	    pj::AudioMedia *am = pj::AudioMedia::typecastFromMedia(m);
	    pj::AudDevManager& aud_mgr = 
		pj::Endpoint::instance().audDevManager();

	    try {
		aud_mgr.getCaptureDevMedia().startTransmit(*am);
		am->startTransmit(aud_mgr.getPlaybackDevMedia());	

	    } catch (pj::Error& e) {
		MyAppRT::Instance->writeLog(2, to_platform_str(e.info()));
		continue;
	    }	
	}
    }
};
///////////////////////////////////////////////////////////////////////////////

AccountInfo::AccountInfo(pj::AccountConfig* accConfig) : cfg(accConfig)
{}

Platform::String^ AccountInfo::id::get() {
    return  to_platform_str(cfg->idUri);
}

Platform::String^ AccountInfo::registrar::get() {
    return to_platform_str(cfg->regConfig.registrarUri);
}

Platform::String^ AccountInfo::proxy::get() {
    if (!cfg->sipConfig.proxies.empty()) {
	return to_platform_str(cfg->sipConfig.proxies[0]);
    } else {
	return ref new Platform::String(L"");
    }
}

Platform::String^ AccountInfo::username::get() {
    if (!cfg->sipConfig.authCreds.empty()) {
	pj::AuthCredInfo& info = cfg->sipConfig.authCreds[0];

	return to_platform_str(info.username);
    } else {
	return ref new Platform::String(L"");
    }
}

Platform::String^ AccountInfo::password::get() {
    if (!cfg->sipConfig.authCreds.empty()) {
	pj::AuthCredInfo& info = cfg->sipConfig.authCreds[0];

	return to_platform_str(info.data);
    } else {
	return ref new Platform::String(L"");
    }
}

///////////////////////////////////////////////////////////////////////////////
MyAppRT::MyAppRT()
{        
    logWriter = new ImpLogWriter();

    StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;

    appDir = to_std_str(localFolder->Path);
}

MyAppRT::~MyAppRT()
{        
    delete logWriter;
    if (account)
	delete account;

    if (epConfig)
	delete epConfig;

    if (accConfig)
	delete accConfig;

    if (sipTpConfig)
	delete sipTpConfig;
}

void MyAppRT::loadConfig()
{
    pj::JsonDocument* json = new pj::JsonDocument();

    std::string configPath = appDir + "/" + CONFIG_NAME;

    try {
	/* Load file */
	json->loadFile(configPath);
	pj::ContainerNode root = json->getRootContainer();

	/* Read endpoint config */
	epConfig->readObject(root);

	/* Read transport config */
	pj::ContainerNode tp_node = root.readContainer("SipTransport");
	sipTpConfig->readObject(tp_node);

	/* Read account configs */
	pj::ContainerNode acc_node = root.readContainer("Account");
	accConfig->readObject(acc_node);
    } catch (pj::Error) {
	ep.utilLogWrite(2, "loadConfig", "Failed loading config");

	sipTpConfig->port = SIP_PORT;
    }

    delete json;
}

void MyAppRT::saveConfig()
{
    pj::JsonDocument* json = new pj::JsonDocument();

    std::string configPath = appDir + "/" + CONFIG_NAME;

    try {
	/* Write endpoint config */
	json->writeObject(*epConfig);	

	/* Write transport config */
	pj::ContainerNode tp_node = json->writeNewContainer("SipTransport");
	sipTpConfig->writeObject(tp_node);

	/* Write account configs */	
	pj::ContainerNode acc_node = json->writeNewContainer("Account");
	accConfig->writeObject(acc_node);

	/* Save file */
	json->saveFile(configPath);
    } catch (pj::Error) {
	ep.utilLogWrite(2, "saveConfig", "Failed saving config");

	sipTpConfig->port = SIP_PORT;
    }

    delete json;
}

MyAppRT^ MyAppRT::Instance::get()
{
    if (MyAppRT::singleton == nullptr)
    {
	if (MyAppRT::singleton == nullptr)
	{
	    MyAppRT::singleton = ref new MyAppRT();
	}
    }

    return MyAppRT::singleton;
}

void MyAppRT::init(IntAccount^ iAcc, IntCall^ iCall)
{    
    /* Create endpoint */
    try {
	ep.libCreate();
    } catch (pj::Error e) {
	return;
    }

    intAcc = iAcc;
    intCall = iCall;

    epConfig = new pj::EpConfig;
    accConfig = new pj::AccountConfig;
    sipTpConfig = new pj::TransportConfig;

    /* Load config */    
    loadConfig();

    /* Override log level setting */
    epConfig->logConfig.level = LOG_LEVEL;
    epConfig->logConfig.consoleLevel = LOG_LEVEL;

    /* Set log config. */
    pj::LogConfig *log_cfg = &epConfig->logConfig;
    log_cfg->writer = logWriter;
    log_cfg->decor = log_cfg->decor &
	~(::pj_log_decoration::PJ_LOG_HAS_CR |
	  ::pj_log_decoration::PJ_LOG_HAS_NEWLINE);

    /* Set ua config. */
    pj::UaConfig* ua_cfg = &epConfig->uaConfig;
    ua_cfg->userAgent = "Pjsua2 WinRT " + ep.libVersion().full;
    ua_cfg->mainThreadOnly = false;

    /* Init endpoint */
    try
    {
	ep.libInit(*epConfig);
    } catch (pj::Error& e)
    {
	ep.utilLogWrite(2,THIS_FILE,e.info());
	return;
    }

    /* Create transports. */

    try {
	sipTpConfig->port = 5060;
	ep.transportCreate(::pjsip_transport_type_e::PJSIP_TRANSPORT_TCP,
			   *sipTpConfig);
    } catch (pj::Error& e) {
	ep.utilLogWrite(2,THIS_FILE,e.info());	
    }

    try {
	ep.transportCreate(::pjsip_transport_type_e::PJSIP_TRANSPORT_UDP,
			   *sipTpConfig);
    } catch (pj::Error& e) {
	ep.utilLogWrite(2,THIS_FILE,e.info());
    }    

    /* Create accounts. */    
    account = new ImpAccount();
    if (accConfig->idUri.length() == 0) {
	accConfig->idUri = "sip:localhost";	
    } else {
	ua_cfg->stunServer.push_back("stun.pjsip.org");	
    }
    //accConfig->natConfig.iceEnabled = true;
    account->create(*accConfig);
    account->setCallback(iAcc);

    /* Start. */
    try {
	ep.libStart();
    } catch (pj::Error& e) {
	ep.utilLogWrite(2,THIS_FILE,e.info());
	return;
    }
}

void MyAppRT::deInit()
{
    saveConfig();

    try {
	ep.libDestroy();
    } catch (pj::Error) {}

}

void MyAppRT::writeLog(int level, Platform::String^ message)
{
    ep.utilLogWrite(level, "MyAppRT", to_std_str(message));
}

void MyAppRT::hangupCall()
{
    if (activeCall) {
	if (!isThreadRegistered())
	{
	    registerThread("hangupCall");
	}
	
	delete activeCall;
	activeCall = NULL;
    }
};

void MyAppRT::answerCall(CallOpParamRT^ prm)
{
    if (!isThreadRegistered())
    {
	registerThread("answerCall");
    }
    CHECK_EXCEPTION(activeCall->answer(*prm->getPtr()));
};

void MyAppRT::makeCall(Platform::String^ dst_uri)
{
    if (!isThreadRegistered())
    {
	registerThread("makeCall");
    }
    CallOpParamRT^ param = ref new CallOpParamRT;
    activeCall = new ImpCall(*account, -1);
    activeCall->setCallback(intCall);

    CHECK_EXCEPTION(activeCall->makeCall(to_std_str(dst_uri),*param->getPtr()));
};

CallInfoRT^ MyAppRT::getCallInfo()
{
    if (activeCall) {
	pj::CallInfo info = activeCall->getInfo();
	return ref new CallInfoRT(info);
    }

    return nullptr;
}

pj_bool_t MyAppRT::handleIncomingCall(int callId)
{    
    if (activeCall) {
	return PJ_FALSE;
    }

    activeCall = new ImpCall(*account, callId);
    activeCall->setCallback(intCall);

    return PJ_TRUE;
}

void MyAppRT::registerThread(Platform::String^ name)
{
    CHECK_EXCEPTION(ep.libRegisterThread(to_std_str(name)));
}

bool MyAppRT::isThreadRegistered()
{
    // Some threads are registered using PJLIB API, ep.libIsThreadRegistered() will return false on those threads.
    //return ep.libIsThreadRegistered();
    return pj_thread_is_registered() != PJ_FALSE;
}

AccountInfo^ MyAppRT::getAccountInfo()

{    
    return ref new AccountInfo(accConfig);
}

void MyAppRT::modifyAccount(Platform::String^ id, 
			    Platform::String^ registrar,
			    Platform::String^ proxy, 			    
			    Platform::String^ username,
			    Platform::String^ password)
{    
    if (id->IsEmpty()) {
	accConfig->idUri = "sip:localhost";
	accConfig->regConfig.registrarUri = "";
	accConfig->sipConfig.authCreds.clear();
	accConfig->sipConfig.proxies.clear();
    } else {
	pj::AuthCredInfo info = pj::AuthCredInfo("Digest", "*", 
						 to_std_str(username), 0, 
						 to_std_str(password));

	accConfig->idUri = to_std_str(id);
	accConfig->regConfig.registrarUri = to_std_str(registrar);

	accConfig->sipConfig.authCreds.clear();    
	accConfig->sipConfig.authCreds.push_back(info);

	accConfig->sipConfig.proxies.clear();
	accConfig->sipConfig.proxies.push_back(to_std_str(proxy));
    }

    if (!isThreadRegistered())
    {
	registerThread("registerAccount");
    }

    account->modify(*accConfig);

    /* Save config. */
    saveConfig();
}

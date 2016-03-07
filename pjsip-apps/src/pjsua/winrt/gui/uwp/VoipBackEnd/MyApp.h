/* $Id*/
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

#pragma once
#include <pjsua2.hpp>
#include <collection.h>

namespace VoipBackEnd
{
    typedef int TransportId;

    typedef Platform::Collections::Vector<Platform::String^> StringVector;
    typedef Windows::Foundation::Collections::IVector<Platform::String^> 
	IStringVector;

    typedef public enum class INV_STATE
    {
	PJSIP_INV_STATE_NULL,	      /**< Before INVITE is sent or received */
	PJSIP_INV_STATE_CALLING,      /**< After INVITE is sent		     */
	PJSIP_INV_STATE_INCOMING,     /**< After INVITE is received.	     */
	PJSIP_INV_STATE_EARLY,	      /**< After response with To tag.	     */
	PJSIP_INV_STATE_CONNECTING,   /**< After 2xx is sent/received.	     */
	PJSIP_INV_STATE_CONFIRMED,    /**< After ACK is sent/received.	     */
	PJSIP_INV_STATE_DISCONNECTED, /**< Session is terminated.	     */
    } INV_STATE;

    class ImpLogWriter : public pj::LogWriter
    {
    public:
	virtual void write(const pj::LogEntry &entry);
    };

    /* Account related data type. */
    public ref struct OnRegStateParamRT sealed
    {
	OnRegStateParamRT();

	property pj_status_t status
	{
	    pj_status_t get();
	};

	property unsigned code 
	{
	    unsigned get();
	};

	property Platform::String^ reason
	{
	    Platform::String^ get();
	};

	property int expiration
	{
	    int get();
	};

    internal:
	OnRegStateParamRT(const pj::OnRegStateParam& param);

    private:
	pj::OnRegStateParam* inPtr;	    

	~OnRegStateParamRT();
    };

    public ref struct CallInfoRT sealed
    {	
	property Platform::String^ localUri
	{
	    Platform::String^ get();
	}

	property Platform::String^ localContact
	{
	    Platform::String^ get();	    
	}

	property Platform::String^ remoteUri
	{
	    Platform::String^ get();
	}

	property Platform::String^ remoteContact
	{
	    Platform::String^ get();
	}

	property INV_STATE state
	{
	    INV_STATE get();
	};

    internal:
	CallInfoRT(const pj::CallInfo& info);

    private:
	pj::CallInfo* inPtr;

	~CallInfoRT();

    };

    public interface class IntAccount
    {
    public:
	virtual void onRegState(OnRegStateParamRT^ prm);
	virtual void onIncomingCall(CallInfoRT^ info);
    };

    class ImpAccount : public pj::Account
    {
    public:
	void setCallback(IntAccount^ callback);

	virtual void onIncomingCall(pj::OnIncomingCallParam& prm);
	virtual void onRegState(pj::OnRegStateParam& prm);

	~ImpAccount() {};

    private:
	IntAccount^ cb;
    };

    /* Call related data type. */
    public ref class CallOpParamRT sealed
    {
    public:
	CallOpParamRT();

	property unsigned statusCode
	{
	    unsigned get();
	    void set(unsigned val);
	}

	property Platform::String^ reason {
	    Platform::String^ get();
	    void set(Platform::String^ val);
	}

    internal:
	pj::CallOpParam* getPtr();

    private:
	pj::CallOpParam* inPtr;

	~CallOpParamRT();
    };

    public interface class IntCall
    {

    public:
	virtual void onCallState(CallInfoRT^ info);	    
    };

    class ImpCall : public pj::Call
    {	    
    public:	    	    
	ImpCall(pj::Account& account, int call_id);
	virtual ~ImpCall() {};

	void setCallback(IntCall^ callback);

	virtual void onCallState(pj::OnCallStateParam& prm);
	virtual void onCallMediaState(pj::OnCallMediaStateParam& prm);	    

    private:
	IntCall^ cb;
    };

    public ref class AccountInfo sealed
    {
    public:
	property Platform::String^ id {
	    Platform::String^ get();
	}
	property Platform::String^ registrar {
	    Platform::String^ get();
	}
	property Platform::String^ proxy {
	    Platform::String^ get();
	}
	property Platform::String^ username {
	    Platform::String^ get();
	}
	property Platform::String^ password {
	    Platform::String^ get();
	}
    internal:
	AccountInfo(pj::AccountConfig* accConfig);
    private:
	pj::AccountConfig* cfg;
    };

    /* App class. */
    public ref class MyAppRT sealed
    {
    public:
	static property MyAppRT^ Instance {
	    MyAppRT^ get();
	}

	void init(IntAccount^ iAcc, IntCall^ iCall);
	void deInit();

	/* Util */
	void writeLog(int level, Platform::String^ message);

	/* Call handling. */
	void hangupCall();
	void answerCall(CallOpParamRT^ prm);
	void makeCall(Platform::String^ dst_uri);	
	CallInfoRT^ getCallInfo();

	/* Thread handling. */
	void registerThread(Platform::String^ name);
	bool isThreadRegistered();

	/* Account handling. */
	AccountInfo^ getAccountInfo();

	void modifyAccount(Platform::String^ id, Platform::String^ registrar, 
			   Platform::String^ proxy, Platform::String^ username, 
			   Platform::String^ password);

    internal:
	pj_bool_t handleIncomingCall(int callId);

    private:
	MyAppRT();
	~MyAppRT();

	void loadConfig();
	void saveConfig();

	static MyAppRT^ singleton;

	ImpLogWriter* logWriter;
	pj::Endpoint ep;

	/* Configs */
	pj::EpConfig* epConfig;
	pj::AccountConfig* accConfig;
	pj::TransportConfig* sipTpConfig;

	std::string appDir;
	ImpAccount* account;
	ImpCall* activeCall;

	IntAccount^ intAcc;
	IntCall^ intCall;
    };
}
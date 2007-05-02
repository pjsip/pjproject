/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
 * simple_pjsua.c
 *
 * This is a very simple but fully featured SIP user agent, with the 
 * following capabilities:
 *  - SIP registration
 *  - Making and receiving call
 *  - Audio/media to sound device.
 *
 * Usage:
 *  - To make outgoing call, start simple_pjsua with the URL of remote
 *    destination to contact.
 *    E.g.:
 *	 simpleua sip:user@remote
 *
 *  - Incoming calls will automatically be answered with 200.
 *
 * This program will quit once it has completed a single call.
 */

#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include "ua.h"

#define THIS_FILE	"symbian_ua.cpp"

//
// Account
//
#define HAS_SIP_ACCOUNT	0	// 0 to disable registration
#define SIP_DOMAIN	"colinux"
#define SIP_USER	"bulukucing"
#define SIP_PASSWD	"netura"

//
// Outbound proxy for all accounts
//
#define SIP_PROXY	NULL
//#define SIP_PROXY	"sip:192.168.0.1"




/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &ci);

    PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!",
			 (int)ci.remote_info.slen,
			 ci.remote_info.ptr));

    /* Automatically answer incoming calls with 200/OK */
    pjsua_call_answer(call_id, 200, NULL, NULL);
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &ci);
    PJ_LOG(3,(THIS_FILE, "Call %d state=%.*s", call_id,
			 (int)ci.state_text.slen,
			 ci.state_text.ptr));
}

/* Callback called by the library when call's media state has changed */
static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);

    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
	// When media is active, connect call to sound device.
	pjsua_conf_connect(ci.conf_slot, 0);
	pjsua_conf_connect(0, ci.conf_slot);
    }
}


/* Logging callback */
static void log_writer(int level, const char *buf, unsigned len)
{
    wchar_t buf16[PJ_LOG_MAX_SIZE];

    PJ_UNUSED_ARG(level);
    
    pj_ansi_to_unicode(buf, len, buf16, PJ_ARRAY_SIZE(buf16));

    TPtrC16 aBuf((const TUint16*)buf16, (TInt)len);
    console->Write(aBuf);
}

/*
 * app_startup()
 *
 * url may contain URL to call.
 */
static pj_status_t app_startup(char *url)
{
    pjsua_acc_id acc_id = 0;
    pj_status_t status;

    /* Redirect log before pjsua_init() */
    pj_log_set_log_func((void (*)(int,const char*,int)) &log_writer);

    /* Create pjsua first! */
    status = pjsua_create();
    if (status != PJ_SUCCESS) {
    	pjsua_perror(THIS_FILE, "pjsua_create() error", status);
    	return status;
    }

    /* If argument is specified, it's got to be a valid SIP URL */
    if (url) {
	status = pjsua_verify_sip_url(url);
	if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Invalid URL", status);
		return status;
	}
    }

    /* Init pjsua */
    {
	pjsua_config cfg;
	pjsua_logging_config log_cfg;
	pjsua_media_config med_cfg;

	pjsua_config_default(&cfg);
	cfg.max_calls = 2;
	cfg.thread_cnt = 0; // Disable threading on Symbian
	cfg.cb.on_incoming_call = &on_incoming_call;
	cfg.cb.on_call_media_state = &on_call_media_state;
	cfg.cb.on_call_state = &on_call_state;

	if (SIP_PROXY) {
		cfg.outbound_proxy_cnt = 1;
		cfg.outbound_proxy[0] = pj_str(SIP_PROXY);
	}
	
	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = 4;
	log_cfg.cb = &log_writer;

	pjsua_media_config_default(&med_cfg);
	med_cfg.thread_cnt = 0; // Disable threading on Symbian
	med_cfg.has_ioqueue = PJ_FALSE;
	med_cfg.clock_rate = 8000;
	med_cfg.ec_tail_len = 0;
	
	status = pjsua_init(&cfg, &log_cfg, &med_cfg);
	if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "pjsua_init() error", status);
		pjsua_destroy();
		return status;
	}
    }

    /* Add UDP transport. */
    {
	pjsua_transport_config cfg;
	pjsua_transport_id tid;

	pjsua_transport_config_default(&cfg);
	cfg.port = 5060;
	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &tid);
	if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Error creating transport", status);
		pjsua_destroy();
		return status;
	}
	
	pjsua_acc_add_local(tid, PJ_TRUE, &acc_id);
    }

    /* Initialization is done, now start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) {
    	pjsua_perror(THIS_FILE, "Error starting pjsua", status);
    	pjsua_destroy();
    	return status;
    }

    /* Register to SIP server by creating SIP account. */
    if (HAS_SIP_ACCOUNT) {
	pjsua_acc_config cfg;

	pjsua_acc_config_default(&cfg);
	cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
	cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
	cfg.cred_count = 1;
	cfg.cred_info[0].realm = pj_str(SIP_DOMAIN);
	cfg.cred_info[0].scheme = pj_str("digest");
	cfg.cred_info[0].username = pj_str(SIP_USER);
	cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	cfg.cred_info[0].data = pj_str(SIP_PASSWD);

	status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
	if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Error adding account", status);
		pjsua_destroy();
		return status;
	}
    }

    /* If URL is specified, make call to the URL. */
    if (url != NULL) {
	pj_str_t uri = pj_str(url);
	status = pjsua_call_make_call(acc_id, &uri, 0, NULL, NULL, NULL);
	if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Error making call", status);
		pjsua_destroy();
		return status;
	}
			
    }

    return PJ_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////
#include <e32base.h>

class ConsoleUI : public CActive 
{
public:
	ConsoleUI(CActiveSchedulerWait *asw, CConsoleBase *con);
    
	// Run console UI
	void Run();

	// Stop
	void Stop();
    
protected:
	// Cancel asynchronous read.
	void DoCancel();

	// Implementation: called when read has completed.
	void RunL();
    
private:
	CActiveSchedulerWait *asw_;
	CConsoleBase *con_;
};


ConsoleUI::ConsoleUI(CActiveSchedulerWait *asw, CConsoleBase *con) 
: CActive(EPriorityStandard), asw_(asw), con_(con)
{
	CActiveScheduler::Add(this);
}

// Run console UI
void ConsoleUI::Run() 
{
	con_->Read(iStatus);
	SetActive();
}

// Stop console UI
void ConsoleUI::Stop() 
{
	DoCancel();
}

// Cancel asynchronous read.
void ConsoleUI::DoCancel() 
{
	con_->ReadCancel();
}

static void PrintMenu() 
{
	PJ_LOG(3, (THIS_FILE, "\n\n"
		"Menu:\n"
		"  d    Dump states\n"
		"  D    Dump all states (detail)\n"
		"  P    Dump pool factory\n"
		"  h    Hangup all calls\n"
		"  q    Quit\n"));
}

// Implementation: called when read has completed.
void ConsoleUI::RunL() 
{
	TKeyCode kc = con_->KeyCode();

	switch (kc) {
	case 'q':
		asw_->AsyncStop();
		break;
	case 'D':
	case 'd':
		pjsua_dump(kc == 'D');
		Run();
		break;
	case 'P':
		pj_pool_factory_dump(&pjsua_var.cp.factory, PJ_TRUE);
		break;
	case 'h':
		pjsua_call_hangup_all();
		Run();
		break;
	default:
		PJ_LOG(3,(THIS_FILE, "Keycode '%c' (%d) is pressed",
			  kc, kc));
		Run();
		break;
	}

	PrintMenu();
}

////////////////////////////////////////////////////////////////////////////
int ua_main() 
{
	pj_status_t status;
	
	// Initialize pjsua
	status  = app_startup("sip:192.168.0.77");
	if (status != PJ_SUCCESS)
		return status;
	
	
	// Run the UI
	CActiveSchedulerWait *asw = new CActiveSchedulerWait;
	ConsoleUI *con = new ConsoleUI(asw, console);
	
	con->Run();
	
	PrintMenu();
	asw->Start();
	
	delete con;
	delete asw;
	
	// Shutdown pjsua
	pjsua_destroy();
	
    	return 0;
}


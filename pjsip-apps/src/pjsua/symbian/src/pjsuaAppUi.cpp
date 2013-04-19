/*
========================================================================
 Name        : pjsuaAppUi.cpp
 Author      : nanang
 Copyright   : Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 Description : 
========================================================================
*/
// [[[ begin generated region: do not modify [Generated System Includes]
#include <eikmenub.h>
#include <akncontext.h>
#include <akntitle.h>
#include <pjsua.rsg>
// ]]] end generated region [Generated System Includes]

// [[[ begin generated region: do not modify [Generated User Includes]
#include "pjsuaAppUi.h"
#include "pjsua.hrh"
#include "pjsuaContainerView.h"
// ]]] end generated region [Generated User Includes]

// [[[ begin generated region: do not modify [Generated Constants]
// ]]] end generated region [Generated Constants]

#include "../../pjsua_app.h"

/* Global vars */
static CpjsuaAppUi *appui = NULL;
static pj_ioqueue_t *app_ioqueue = NULL;
static int restart_argc = 0;
static char **restart_argv = NULL;

/* Helper funtions to init/restart/destroy the pjsua */
static void LibInitL();
static void LibDestroyL();
static void LibRestartL();

/* pjsua app callbacks */
static void lib_on_started(pj_status_t status, const char* title);
static pj_bool_t lib_on_stopped(pj_bool_t restart, int argc, char** argv);
static void lib_on_config_init(pjsua_app_config *cfg);

/* Helper class to schedule function execution */
class MyTimer : public CActive 
{
public:
    typedef void (*timer_func)();
    
public:
    static MyTimer* NewL(int ms, timer_func f) {
	MyTimer *self = new MyTimer(f);
	CleanupStack::PushL(self);
	self->ConstructL(ms);
	CleanupStack::Pop(self);
	return self;
    }
    
    MyTimer(timer_func f) : CActive(EPriorityStandard), func(f) {}
    ~MyTimer() {
	Cancel();
	rtimer.Close();
    }
    
    virtual void RunL() { (*func)(); delete this; }
    virtual void DoCancel() { rtimer.Cancel(); }

private:	
    RTimer rtimer;
    timer_func func;
    
    void ConstructL(int ms) {
	rtimer.CreateLocal();
	CActiveScheduler::Add(this);
	rtimer.After(iStatus, ms * 1000);
	SetActive();
    }
};

/**
 * Construct the CpjsuaAppUi instance
 */ 
CpjsuaAppUi::CpjsuaAppUi()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}

/** 
 * The appui's destructor removes the container from the control
 * stack and destroys it.
 */
CpjsuaAppUi::~CpjsuaAppUi()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}

// [[[ begin generated function: do not modify
void CpjsuaAppUi::InitializeContainersL()
	{
	iPjsuaContainerView = CpjsuaContainerView::NewL();
	AddViewL( iPjsuaContainerView );
	SetDefaultViewL( *iPjsuaContainerView );
	}
// ]]] end generated function

/**
 * Handle a command for this appui (override)
 * @param aCommand command id to be handled
 */
void CpjsuaAppUi::HandleCommandL( TInt aCommand )
	{
	// [[[ begin generated region: do not modify [Generated Code]
	TBool commandHandled = EFalse;
	switch ( aCommand )
		{ // code to dispatch to the AppUi's menu and CBA commands is generated here
		default:
			break;
		}
	
		
	if ( !commandHandled ) 
		{
		if ( aCommand == EAknSoftkeyExit || aCommand == EEikCmdExit )
			{
			Exit();
			}
		}
	// ]]] end generated region [Generated Code]
	
	}

/** 
 * Override of the HandleResourceChangeL virtual function
 */
void CpjsuaAppUi::HandleResourceChangeL( TInt aType )
	{
	CAknViewAppUi::HandleResourceChangeL( aType );
	// [[[ begin generated region: do not modify [Generated Code]
	// ]]] end generated region [Generated Code]
	
	}
				
/** 
 * Override of the HandleKeyEventL virtual function
 * @return EKeyWasConsumed if event was handled, EKeyWasNotConsumed if not
 * @param aKeyEvent 
 * @param aType 
 */
TKeyResponse CpjsuaAppUi::HandleKeyEventL(
		const TKeyEvent& aKeyEvent,
		TEventCode aType )
	{
	// The inherited HandleKeyEventL is private and cannot be called
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]

	// Left or right softkey pressed
	if (aType==EEventKeyDown && 
	    (aKeyEvent.iScanCode == EStdKeyDevice0 || 
	     aKeyEvent.iScanCode == EStdKeyDevice1))    
	{
	    Cba()->MakeVisible(ETrue);
	} else {
	    Cba()->MakeVisible(EFalse);   
	}

	return EKeyWasNotConsumed;
	}

/** 
 * Override of the HandleViewDeactivation virtual function
 *
 * @param aViewIdToBeDeactivated 
 * @param aNewlyActivatedViewId 
 */
void CpjsuaAppUi::HandleViewDeactivation( 
		const TVwsViewId& aViewIdToBeDeactivated, 
		const TVwsViewId& aNewlyActivatedViewId )
	{
	CAknViewAppUi::HandleViewDeactivation( 
			aViewIdToBeDeactivated, 
			aNewlyActivatedViewId );
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}

/**
 * @brief Completes the second phase of Symbian object construction. 
 * Put initialization code that could leave here. 
 */ 
void CpjsuaAppUi::ConstructL()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	
	BaseConstructL( EAknEnableSkin  | 
					 EAknEnableMSK ); 
	InitializeContainersL();
	// ]]] end generated region [Generated Contents]
	
	// Save pointer to this AppUi
	appui = this;
	
	// Full screen
	StatusPane()->MakeVisible(EFalse);
	Cba()->MakeVisible(EFalse);

	// Schedule Lib Init
	MyTimer::NewL(100, &LibInitL);
	}

/* Called by Symbian GUI framework when app is about to exit */
void CpjsuaAppUi::PrepareToExit()
{
    TRAPD(result, LibDestroyL());
    CAknViewAppUi::PrepareToExit();
}

/* Print message on screen */
void CpjsuaAppUi::PutMsg(const char *msg)
{
    iPjsuaContainerView->PutMessage(msg);
}

#include <es_sock.h>

static RSocketServ aSocketServer;
static RConnection aConn;

/* Called when pjsua is started */
void lib_on_started(pj_status_t status, const char* title)
{
    appui->PutMsg(title);
}

/* Called when pjsua is stopped */
pj_bool_t lib_on_stopped(pj_bool_t restart, int argc, char** argv)
{
    if (restart) {
	restart_argc = argc;
	restart_argv = argv;

	// Schedule Lib Init
	MyTimer::NewL(100, &LibRestartL);
    } else {
	/* Destroy & quit GUI, e.g: clean up window, resources  */
	appui->Exit();
    }

    return PJ_FALSE;
}

/* Called before pjsua initializing config.
 * We need to override some settings here.
 */
void lib_on_config_init(pjsua_app_config *cfg)
{
    /* Disable threading */
    cfg->cfg.thread_cnt = 0;
    cfg->cfg.thread_cnt = 0;
    cfg->media_cfg.thread_cnt = 0;
    cfg->media_cfg.has_ioqueue = PJ_FALSE;

    /* Create ioqueue for telnet CLI */
    if (app_ioqueue ==  NULL) {
	pj_ioqueue_create(cfg->pool, 0, &app_ioqueue);
    }
    cfg->cli_cfg.telnet_cfg.ioqueue = app_ioqueue; 
}

void LibInitL()
{
    pj_symbianos_params sym_params;
    char* argv[] = {
	"",
	"--use-cli",
	"--cli-telnet-port=0",
	"--no-cli-console"
    };
    app_cfg_t app_cfg;
    pj_status_t status;
    TInt err;

    // Initialize RSocketServ
    if ((err=aSocketServer.Connect(32)) != KErrNone) {
    	status = PJ_STATUS_FROM_OS(err);
    	goto on_return;
    }
    
    // Open up a connection
    if ((err=aConn.Open(aSocketServer)) != KErrNone) {
	aSocketServer.Close();
	status = PJ_STATUS_FROM_OS(err);
    	goto on_return;
    }
    if ((err=aConn.Start()) != KErrNone) {
	aConn.Close();
    	aSocketServer.Close();
    	status = PJ_STATUS_FROM_OS(err);
    	goto on_return;
    }
    
    // Set Symbian OS parameters in pjlib.
    // This must be done before pj_init() is called.
    pj_bzero(&sym_params, sizeof(sym_params));
    sym_params.rsocketserv = &aSocketServer;
    sym_params.rconnection = &aConn;
    pj_symbianos_set_params(&sym_params);

    pj_bzero(&app_cfg, sizeof(app_cfg));
    app_cfg.argc = PJ_ARRAY_SIZE(argv);
    app_cfg.argv = argv;
    app_cfg.on_started = &lib_on_started;
    app_cfg.on_stopped = &lib_on_stopped;
    app_cfg.on_config_init = &lib_on_config_init;

    appui->PutMsg("Initializing..");
    status = app_init(&app_cfg);
    if (status != PJ_SUCCESS)
	goto on_return;
    
    appui->PutMsg("Starting..");
    status = app_run(PJ_FALSE);
    if (status != PJ_SUCCESS)
	goto on_return;

on_return:
    if (status != PJ_SUCCESS)
	appui->PutMsg("Initialization failed");
}

void LibDestroyL()
{
    if (app_ioqueue) {
	pj_ioqueue_destroy(app_ioqueue);
	app_ioqueue = NULL;
    }
    app_destroy();
    CloseSTDLIB();
}

void LibRestartL()
{
    app_cfg_t app_cfg;
    pj_status_t status;
    
    /* Destroy pjsua app first */

    if (app_ioqueue) {
	pj_ioqueue_destroy(app_ioqueue);
	app_ioqueue = NULL;
    }
    app_destroy();

    /* Reinit pjsua app */
    
    pj_bzero(&app_cfg, sizeof(app_cfg));
    app_cfg.argc = restart_argc;
    app_cfg.argv = restart_argv;
    app_cfg.on_started = &lib_on_started;
    app_cfg.on_stopped = &lib_on_stopped;
    app_cfg.on_config_init = &lib_on_config_init;

    status = app_init(&app_cfg);
    if (status != PJ_SUCCESS) {
	appui->PutMsg("app_init() failed");
	return;
    }
	
    /* Run pjsua app */

    status = app_run(PJ_FALSE);
    if (status != PJ_SUCCESS) {
	appui->PutMsg("app_run() failed");
	return;
    }
}

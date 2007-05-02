//Auto-generated file. Please do not modify.
//#include <e32cmn.h>

//#pragma data_seg(".SYMBIAN")
//__EMULATOR_IMAGE_HEADER2 (0x1000007a,0x00000000,0x00000000,EPriorityForeground,0x00000000u,0x00000000u,0x00000000,0x00000000,0x00000000,0)
//#pragma data_seg()

#include "ua.h"
#include <stdlib.h>
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/unicode.h>
#include <stdio.h>

#include <e32std.h>

#include <pj/os.h>

#include <e32base.h>
#include <e32std.h>



//  Global Variables
CConsoleBase* console;
static CActiveSchedulerWait *asw;


//  Local Functions

LOCAL_C void MainL()
{
    //
    // add your program code here, example code below
    //
    int rc = ua_main();

    asw->AsyncStop();
}

class MyScheduler : public CActiveScheduler
{
public:
    MyScheduler()
    {}

    void Error(TInt aError) const;
};

void MyScheduler::Error(TInt aError) const
{
    PJ_UNUSED_ARG(aError);
}

class MyTask : public CActive
{
public:
    static MyTask *NewL();
    void Start();

protected:
    MyTask();
    void ConstructL();
    virtual void RunL();
    virtual void DoCancel();
    TInt RunError(TInt aError);

private:
    RTimer timer_;
};

MyTask::MyTask()
: CActive(EPriorityNormal)
{
}

void MyTask::ConstructL()
{
    timer_.CreateLocal();
    CActiveScheduler::Add(this);
}

MyTask *MyTask::NewL()
{
    MyTask *self = new (ELeave) MyTask;
    CleanupStack::PushL(self);

    self->ConstructL();

    CleanupStack::Pop(self);
    return self;
}

void MyTask::Start()
{
    timer_.After(iStatus, 0);
    SetActive();
}

void MyTask::RunL()
{
    MainL();
}

void MyTask::DoCancel()
{
}

TInt MyTask::RunError(TInt aError)
{
    PJ_UNUSED_ARG(aError);
    return KErrNone;
}


LOCAL_C void DoStartL()
{
    // Create active scheduler (to run active objects)
    MyScheduler* scheduler = new (ELeave) MyScheduler;
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);

    MyTask *task = MyTask::NewL();
    task->Start();

    asw = new CActiveSchedulerWait;
    asw->Start();
    
    delete asw;
    CleanupStack::Pop(scheduler);
}


////////////////////////////////////////////////////////////////////////////

class TMyTrapHandler : public TTrapHandler 
{
public:
	void Install();
	void Uninstall();
	virtual IMPORT_C void Trap();
	virtual IMPORT_C void UnTrap();
	virtual IMPORT_C void Leave(TInt aValue);
	
private:
	TTrapHandler *prev_;
};

void TMyTrapHandler::Install() {
	prev_ = User::SetTrapHandler(this);
}

void TMyTrapHandler::Uninstall() {
	User::SetTrapHandler(prev_);
}

IMPORT_C void TMyTrapHandler::Trap() 
{
	prev_->Trap();
}

IMPORT_C void TMyTrapHandler::UnTrap() 
{
	prev_->UnTrap();
}

IMPORT_C void TMyTrapHandler::Leave(TInt aValue) 
{
	prev_->Leave(aValue);
}


////////////////////////////////////////////////////////////////////////////

//  Global Functions
GLDEF_C TInt E32Main()
{
    TMyTrapHandler th;
    
    th.Install();
    
    // Create cleanup stack
    //__UHEAP_MARK;
    CTrapCleanup* cleanup = CTrapCleanup::New();

    // Create output console
    TRAPD(createError, console = Console::NewL(_L("Console"), TSize(KConsFullScreen,KConsFullScreen)));
    if (createError)
        return createError;

    TRAPD(startError, DoStartL());

    console->Printf(_L("[press any key to close]\n"));
    console->Getch();
    
    delete console;
    delete cleanup;
    //__UHEAP_MARKEND;
    
    th.Uninstall();
    return KErrNone;
}


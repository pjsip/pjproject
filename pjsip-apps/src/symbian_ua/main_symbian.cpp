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


/////////////////////////////////////
class MyTask : public CActive
{
public:
    static MyTask *NewL(CActiveSchedulerWait *asw);
    ~MyTask();
    void Start();

protected:
    MyTask(CActiveSchedulerWait *asw);
    void ConstructL();
    virtual void RunL();
    virtual void DoCancel();

private:
    RTimer timer_;
    CActiveSchedulerWait *asw_;
};

MyTask::MyTask(CActiveSchedulerWait *asw)
: CActive(EPriorityNormal), asw_(asw)
{
}

MyTask::~MyTask() 
{
    timer_.Close();
}

void MyTask::ConstructL()
{
    timer_.CreateLocal();
    CActiveScheduler::Add(this);
}

MyTask *MyTask::NewL(CActiveSchedulerWait *asw)
{
    MyTask *self = new (ELeave) MyTask(asw);
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
    int rc = ua_main();
    asw_->AsyncStop();
}

void MyTask::DoCancel()
{
}

LOCAL_C void DoStartL()
{
    CActiveScheduler *scheduler = new (ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);

    CActiveSchedulerWait *asw = new CActiveSchedulerWait;
    CleanupStack::PushL(asw);
    
    MyTask *task = MyTask::NewL(asw);
    task->Start();

    asw->Start();
    
    delete task;
    
    CleanupStack::Pop(asw);
    delete asw;
    
    CActiveScheduler::Install(NULL);
    CleanupStack::Pop(scheduler);
    delete scheduler;
}


////////////////////////////////////////////////////////////////////////////

//  Global Functions
GLDEF_C TInt E32Main()
{
    // Create cleanup stack
    __UHEAP_MARK;
    CTrapCleanup* cleanup = CTrapCleanup::New();

    // Create output console
    TRAPD(createError, console = Console::NewL(_L("Console"), TSize(KConsFullScreen,KConsFullScreen)));
    if (createError)
        return createError;

    TRAPD(startError, DoStartL());

    console->Printf(_L("[press any key to close]\n"));
    //console->Getch();
    
    delete console;
    delete cleanup;

    CloseSTDLIB();    
    __UHEAP_MARKEND;
    return KErrNone;
}


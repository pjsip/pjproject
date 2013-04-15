// Default empty project template
#include "applicationui.h"

#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/Label>

#define THIS_FILE	"applicationui.cpp"

using namespace bb::cascades;

#include "../../pjsua_common.h"

extern pj_cli_telnet_on_started on_started_cb;
extern pj_cli_on_quit           on_quit_cb;

extern "C" int main_func(int argc, char *argv[]);

ApplicationUI *ApplicationUI::instance_;

class CliThread : public QThread
{
    Q_OBJECT
public:
    virtual ~CliThread() {}
protected:
     void run();
};

static void bb10_show_msg(const char *msg)
{
    /* Qt's way to invoke method from "foreign" thread */
    QMetaObject::invokeMethod((QObject*)ApplicationUI::instance(), "displayMsg",
			      Qt::QueuedConnection,
			      Q_ARG(QString,msg));
}

static void bb10_telnet_started(pj_cli_telnet_info *telnet_info)
{
    char msg[64];

    pj_ansi_snprintf(msg, sizeof(msg),
		     "Telnet to %.*s:%d",
	    	     (int)telnet_info->ip_address.slen,
	    	     telnet_info->ip_address.ptr,
	    	     telnet_info->port);

    PJ_LOG(3,(THIS_FILE, "Started: %s", msg));

    bb10_show_msg(msg);
}

static void bb10_on_quit (pj_bool_t is_restarted)
{
    PJ_LOG(3,("ipjsua", "CLI quit, restart(%d)", is_restarted));
    if (!is_restarted) {
	bb10_show_msg("Shutting down..");
	ApplicationUI::instance()->isShuttingDown = true;
	bb::cascades::Application *app = bb::cascades::Application::instance();
	app->quit();
    }
}

void CliThread::run()
{
    // TODO: read from config?
    char *argv[] = { (char*)"pjsuabb",
		     (char*)"--use-cli",
		     (char*)"--no-cli-console",
		     (char*)"--cli-telnet-port=2323",
		     (char*)"--dis-codec=*",
		     (char*)"--add-codec=g722",
		      NULL };
    int argc = PJ_ARRAY_SIZE(argv) -1;
    pj_thread_desc thread_desc;
    pj_thread_t *thread;

    pj_thread_register("CliThread", thread_desc, &thread);
    // Wait UI to be created
    pj_thread_sleep(100);

    on_started_cb = &bb10_telnet_started;
    on_quit_cb = &bb10_on_quit;
    main_func(argc, argv);
}

ApplicationUI::ApplicationUI(bb::cascades::Application *app)
: QObject(app), isShuttingDown(false)
{
    instance_ = this;

    // create scene document from main.qml asset
    // set parent to created document to ensure it exists for the whole application lifetime
    QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);

    // create root object for the UI
    AbstractPane *root = qml->createRootObject<AbstractPane>();
    // set created root object as a scene
    app->setScene(root);

    app->setAutoExit(true);
    connect(app, SIGNAL(aboutToQuit()), this, SLOT(aboutToQuit()));

    pj_init();

    // Run CLI
    cliThread = new CliThread;
    cliThread->start();
}

ApplicationUI::~ApplicationUI()
{
    pj_shutdown();
    instance_ = NULL;
}

ApplicationUI* ApplicationUI::instance()
{
    return instance_;
}

void ApplicationUI::aboutToQuit()
{
    static pj_thread_desc thread_desc;
    pj_thread_t *thread;

    if (!pj_thread_is_registered())
	pj_thread_register("UIThread", thread_desc, &thread);

    if (!isShuttingDown) {
	isShuttingDown = true;
	PJ_LOG(3,(THIS_FILE, "Quit signal from GUI, shutting down pjsua.."));
	pjsua_destroy();
    }
}

void ApplicationUI::displayMsg(const QString &msg)
{
    bb::cascades::Application *app = bb::cascades::Application::instance();
    Label *telnetMsg = app->scene()->findChild<Label*>("telnetMsg");
    if (telnetMsg) {
	telnetMsg->setText(msg);
    }
}

#include "applicationui.moc"

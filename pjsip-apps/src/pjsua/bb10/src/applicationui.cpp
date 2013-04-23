// Default empty project template
#include "applicationui.h"

#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/Label>

#define THIS_FILE	"applicationui.cpp"

using namespace bb::cascades;

/* appUI singleton */
ApplicationUI *ApplicationUI::instance_;

#include "../../pjsua_app_config.h"

void ApplicationUI::extDisplayMsg(const char *msg)
{
    /* Qt's way to invoke method from "foreign" thread */
    QMetaObject::invokeMethod((QObject*)ApplicationUI::instance(),
			      "displayMsg", Qt::AutoConnection,
			      Q_ARG(QString,msg));
}


void ApplicationUI::pjsuaOnStartedCb(pj_status_t status, const char* msg)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    if (status != PJ_SUCCESS && (!msg || !*msg)) {
	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(3,(THIS_FILE, "Error: %s", errmsg));
	msg = errmsg;
    } else {
	PJ_LOG(3,(THIS_FILE, "Started: %s", msg));
    }

    ApplicationUI::extDisplayMsg(msg);
}


void ApplicationUI::pjsuaOnStoppedCb(pj_bool_t restart,
				     int argc, char** argv)
{
    PJ_LOG(3,("ipjsua", "CLI %s request", (restart? "restart" : "shutdown")));
    if (restart) {
	ApplicationUI::extDisplayMsg("Restarting..");
	pj_thread_sleep(100);
	ApplicationUI::instance()->extRestartRequest(argc, argv);
    } else {
	ApplicationUI::extDisplayMsg("Shutting down..");
	pj_thread_sleep(100);
	ApplicationUI::instance()->isShuttingDown = true;

	bb::cascades::Application *app = bb::cascades::Application::instance();
	app->quit();
    }
}


void ApplicationUI::pjsuaOnAppConfigCb(pjsua_app_config *cfg)
{
    PJ_UNUSED_ARG(cfg);
}


void ApplicationUI::extRestartRequest(int argc, char **argv)
{
    restartArgc = argc;
    restartArgv = argv;
    QMetaObject::invokeMethod((QObject*)this, "restartPjsua",
			      Qt::QueuedConnection);
}


void ApplicationUI::pjsuaStart()
{
    // TODO: read from config?
    const char **argv = pjsua_app_def_argv;
    int argc = PJ_ARRAY_SIZE(pjsua_app_def_argv) -1;
    pjsua_app_cfg_t app_cfg;
    pj_status_t status;

    isShuttingDown = false;
    displayMsg("Starting..");

    pj_bzero(&app_cfg, sizeof(app_cfg));
    if (restartArgc) {
	app_cfg.argc = restartArgc;
	app_cfg.argv = restartArgv;
    } else {
	app_cfg.argc = argc;
	app_cfg.argv = (char**)argv;
    }
    app_cfg.on_started = &pjsuaOnStartedCb;
    app_cfg.on_stopped = &pjsuaOnStoppedCb;
    app_cfg.on_config_init = &pjsuaOnAppConfigCb;

    status = pjsua_app_init(&app_cfg);
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));
	displayMsg(QString("Init error:") + errmsg);
	pjsua_app_destroy();
	return;
    }

    status = pjsua_app_run(PJ_FALSE);
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));
	displayMsg(QString("Error:") + errmsg);
	pjsua_app_destroy();
    }

    restartArgv = NULL;
    restartArgc = 0;
}

void ApplicationUI::pjsuaDestroy()
{
    pjsua_app_destroy();
}


ApplicationUI::ApplicationUI(bb::cascades::Application *app)
: QObject(app), isShuttingDown(false), restartArgv(NULL), restartArgc(0)
{
    instance_ = this;

    QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);
    AbstractPane *root = qml->createRootObject<AbstractPane>();
    app->setScene(root);

    app->setAutoExit(true);
    connect(app, SIGNAL(aboutToQuit()), this, SLOT(aboutToQuit()));

    pjsuaStart();
}


ApplicationUI::~ApplicationUI()
{
    instance_ = NULL;
}


ApplicationUI* ApplicationUI::instance()
{
    return instance_;
}


void ApplicationUI::aboutToQuit()
{
    if (!isShuttingDown) {
	isShuttingDown = true;
	PJ_LOG(3,(THIS_FILE, "Quit signal from GUI, shutting down pjsua.."));
	pjsuaDestroy();
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


void ApplicationUI::restartPjsua()
{
    pjsuaDestroy();
    pjsuaStart();
}

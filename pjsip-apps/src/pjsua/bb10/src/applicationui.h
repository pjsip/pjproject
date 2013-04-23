// Default empty project template
#ifndef ApplicationUI_HPP_
#define ApplicationUI_HPP_

#include <QObject>

#include "../../pjsua_app.h"

namespace bb { namespace cascades { class Application; }}

/*!
 * @brief Application pane object
 *
 *Use this object to create and init app UI, to create context objects, to register the new meta types etc.
 */
class ApplicationUI : public QObject
{
    Q_OBJECT
public:
    ApplicationUI(bb::cascades::Application *app);
    virtual ~ApplicationUI();

    bool isShuttingDown;
    static ApplicationUI *instance();

    /* Write msg to label (from different thread) */
    static void extDisplayMsg(const char *msg);

    /* Restart request (from different thread) */
    void extRestartRequest(int argc, char **argv);

public slots:
    void aboutToQuit();

    Q_INVOKABLE void restartPjsua();
    Q_INVOKABLE void displayMsg(const QString &msg);

private:
    static ApplicationUI *instance_;
    char **restartArgv;
    int restartArgc;

    /* pjsua main operations */
    void pjsuaStart();
    void pjsuaDestroy();

    /* pjsua app callbacks */
    static void 	pjsuaOnStartedCb(pj_status_t status, const char* msg);
    static pj_bool_t 	pjsuaOnStoppedCb(pj_bool_t restart, int argc, char** argv);
    static void 	pjsuaOnAppConfigCb(pjsua_app_config *cfg);
};


#endif /* ApplicationUI_HPP_ */

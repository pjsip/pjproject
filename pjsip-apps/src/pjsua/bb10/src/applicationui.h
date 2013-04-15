// Default empty project template
#ifndef ApplicationUI_HPP_
#define ApplicationUI_HPP_

#include <QObject>

namespace bb { namespace cascades { class Application; }}

class CliThread;

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

public slots:
    void aboutToQuit();

    Q_INVOKABLE void displayMsg(const QString &msg);

private:
    CliThread *cliThread;
    static ApplicationUI *instance_;
};


#endif /* ApplicationUI_HPP_ */

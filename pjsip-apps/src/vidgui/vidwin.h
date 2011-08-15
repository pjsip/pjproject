#ifndef VIDWIN_H
#define VIDWIN_H

#include <pjsua.h>
#include <QWidget>

class VidWin : public QWidget
{
    Q_OBJECT

public:
    VidWin(const pjmedia_vid_dev_hwnd *hwnd,
	   QWidget* parent = 0,
	   Qt::WindowFlags f = 0);
    virtual ~VidWin();
    QSize sizeHint() const { return size_hint; }

protected:
    virtual bool event(QEvent *e);

private:
    pjmedia_vid_dev_hwnd hwnd;
    void *orig_parent;
    QSize size_hint;

    void attach();
    void detach();
    void set_size();
    void get_size();
};

#endif


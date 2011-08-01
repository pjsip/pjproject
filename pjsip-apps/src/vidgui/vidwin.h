#ifndef VIDWIN_H
#define VIDWIN_H

#include <pjsua.h>
#include <QWidget>

class VidWin : public QWidget
{
    Q_OBJECT

public:
    // hwnd	    Handle of the video rendering window.
    VidWin(pjmedia_vid_dev_hwnd *hwnd = NULL, 
	   QWidget* parent = 0,
	   Qt::WindowFlags f = 0);
    virtual ~VidWin();

protected:
    void resizeEvent(QResizeEvent *e);

private:
    pjmedia_vid_dev_hwnd hwnd;
    //pjmedia_vid_dev_hwnd old_parent_hwnd;
    pj_timer_entry timer_entry;

    static void timer_cb(pj_timer_heap_t *timer_heap,
			 struct pj_timer_entry *entry);

    void embed();
    void resize();
};

#endif

#include "vidwin.h"

#define THIS_FILE	"vidwin.cpp"
#define TIMER_EMBED	1
#define TIMER_RESIZE	2

void VidWin::timer_cb(pj_timer_heap_t *timer_heap,
		      struct pj_timer_entry *entry)
{
    VidWin *vw = (VidWin*)entry->user_data;
    
    PJ_UNUSED_ARG(timer_heap);

    switch(entry->id) {
    case TIMER_EMBED:
	vw->embed();
	vw->resize();
	break;
    case TIMER_RESIZE:
	vw->resize();
	break;
    default:
	break;
    }

    entry->id = 0;
}


VidWin::VidWin(pjmedia_vid_dev_hwnd *hwnd_, QWidget* parent, Qt::WindowFlags f) :
    QWidget(parent, f), hwnd(*hwnd_)
{
#if 0
    // A proof that QWidget::create() change window proc!
    // And that will cause SDL rendering not working.
    HWND h = (HWND)hwnd->info.win.hwnd;
    LONG wl;

    wl = GetWindowLong(h, GWL_WNDPROC);
    printf("%p old proc: %p\n", h, wl);

    create(WId(hwnd->info.win.hwnd), false, true);
    printf("%p qwidgetwid: %p\n", h, winId());

    wl = GetWindowLong(h, GWL_WNDPROC);
    printf("%p new proc: %p\n", h, wl);
#endif

    setAttribute(Qt::WA_NativeWindow);
    setMinimumSize(320, 200);

    /* Make this widget a bit "lighter" */
    //setAttribute(Qt::WA_UpdatesDisabled);
    //setAttribute(Qt::WA_PaintOnScreen);
    //setAttribute(Qt::WA_NoSystemBackground);
    //setAttribute(Qt::WA_PaintOutsidePaintEvent);
    //setUpdatesEnabled(false);
    
    /* Schedule embed, as at this point widget initialization is not 
     * completely done yet (e.g: bad size).
     */
    pj_timer_entry_init(&timer_entry, TIMER_EMBED, this, &timer_cb);
    pj_time_val delay = {0, 100};
    pjsua_schedule_timer(&timer_entry, &delay);
}


VidWin::~VidWin()
{
    if (timer_entry.id) {
	pjsua_cancel_timer(&timer_entry);
	timer_entry.id = 0;
    }
}


void VidWin::resizeEvent(QResizeEvent*)
{
    /* Resizing SDL window must be scheduled (via timer),
     * as on Windows platform, the SDL resizing process
     * will steal focus (SDL bug?).
     */
    if (timer_entry.id && timer_entry.id != TIMER_RESIZE)
	return;

    if (timer_entry.id == TIMER_RESIZE)
	pjsua_cancel_timer(&timer_entry);

    timer_entry.id = TIMER_RESIZE;
    timer_entry.cb = &timer_cb;
    timer_entry.user_data = this;

    pj_time_val delay = {0, 300};
    pjsua_schedule_timer(&timer_entry, &delay);
}

/* Platform specific code */

#if defined(PJ_WIN32) && !defined(PJ_WIN32_WINCE)

#include <windows.h>

void VidWin::embed()
{
    /* Embed hwnd to widget */
    pj_assert(hwnd.type == PJMEDIA_VID_DEV_HWND_TYPE_WINDOWS);
    HWND h = (HWND)hwnd.info.win.hwnd;
    HWND new_parent = (HWND)winId();

    //old_parent_hwnd.type = PJMEDIA_VID_DEV_HWND_TYPE_WINDOWS;
    //old_parent_hwnd.info.win.hwnd = GetParent(h);

    SetParent(h, new_parent);
    SetWindowLong(h, GWL_STYLE, WS_CHILD);
    ShowWindow(h, SW_SHOWNOACTIVATE);
    PJ_LOG(3, (THIS_FILE, "%p parent handle = %p", h, new_parent));
}

void VidWin::resize()
{
    /* Update position and size */
    HWND h = (HWND)hwnd.info.win.hwnd;
    QRect qr = rect();
    UINT swp_flag = SWP_SHOWWINDOW | SWP_NOACTIVATE;
    SetWindowPos(h, HWND_TOP, 0, 0, qr.width(), qr.height(), swp_flag);
    PJ_LOG(3, (THIS_FILE, "%p new size = %d x %d", h, qr.width(), qr.height()));
}

#elif defined(PJ_DARWINOS)

#import<Cocoa/Cocoa.h>

void VidWin::embed()
{
    /* Embed hwnd to widget */
    pj_assert(hwnd.type != PJMEDIA_VID_DEV_HWND_TYPE_WINDOWS);
    NSWindow *w = (NSWindow*)hwnd.info.cocoa.window;
    NSWindow *parent = [(NSView*)winId() window];

    //[w setStyleMask:NSBorderlessWindowMask];

    //[w setParentWindow:parent];
    [parent addChildWindow:w ordered:NSWindowAbove];
    PJ_LOG(3, (THIS_FILE, "%p parent handle = %p", w, parent));
}


void VidWin::resize()
{
    /* Update position and size */
    NSWindow *w = (NSWindow*)hwnd.info.cocoa.window;
    NSRect r;

    NSView* v = (NSView*)winId();
    r = [v bounds];
    //PJ_LOG(3, (THIS_FILE, "before: (%d,%d) %dx%d", r.origin.x, r.origin.y, r.size.width, r.size.height));
    r = [v convertRectToBase:r];
    r.origin = [[v window] convertBaseToScreen:r.origin];
    //PJ_LOG(3, (THIS_FILE, "after: (%d,%d) %dx%d", r.origin.x, r.origin.y, r.size.width, r.size.height));

    QRect qr = rect();
/*
    QPoint p = pos();
    QPoint pp = parentWidget()->pos();
    PJ_LOG(3, (THIS_FILE, "this pos: (%d,%d)", p.x(), p.y()));
    PJ_LOG(3, (THIS_FILE, "parent pos: (%d,%d)", pp.x(), pp.y()));
    
    //qr.setTopLeft(mapToGlobal(qr.topLeft()));
    r.origin.x = qr.x();
    r.origin.y = qr.y();
    r.size.width = qr.width();
    r.size.height = qr.height();
    //r.origin = [w convertBaseToScreen:r.origin];
*/
    [w setFrame:r display:NO]; 

    PJ_LOG(3, (THIS_FILE, "%p new size = %d x %d", w, qr.width(), qr.height()));
}

#elif defined(PJ_LINUX)

#include <X11/Xlib.h>
//#include <QX11Info>

void VidWin::embed()
{
    /* Embed hwnd to widget */
    pj_assert(hwnd.type != PJMEDIA_VID_DEV_HWND_TYPE_WINDOWS);
    Display *d = (Display*)hwnd.info.x11.display;
    //Display *d = QX11Info::display();
    Window w = (Window)hwnd.info.x11.window;
    Window parent = (Window)this->winId();

    XSetWindowBorderWidth(d, w, 0);

    int err = XReparentWindow(d, w, parent, 0, 0);
    PJ_LOG(3, (THIS_FILE, "XReparentWindow() err = %d", err));
    //XRaiseWindow(d, w);
    //XMapSubwindows(d, parent);
    //XMapWindow(d, parent);
    //XMapWindow(d, w);
    //XSync(d, False);
    
    PJ_LOG(3, (THIS_FILE, "[%p,%p] parent handle = %p", d, w, parent));
}


void VidWin::resize()
{
    /* Update position and size */
    Display *d = (Display*)hwnd.info.x11.display;
    Window w = (Window)hwnd.info.x11.window;
    QRect qr = rect();
    //XResizeWindow(d, w, qr.width(), qr.height());
    XMoveResizeWindow(d, w, 0, 0, qr.width(), qr.height());

    PJ_LOG(3, (THIS_FILE, "[%p,%p] new size = %d x %d", d, w, qr.width(), qr.height()));
    //XSync(d, False);
    XFlush(d);
}

#endif


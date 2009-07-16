#ifndef __GUI_H__
#define __GUI_H__

#include <pj/types.h>

PJ_BEGIN_DECL

typedef char gui_title[32];

typedef void (*gui_menu_handler) (void);

typedef struct gui_menu 
{
    gui_title		 title;
    gui_menu_handler	 handler;
    unsigned		 submenu_cnt;
    struct gui_menu	*submenus[16];
} gui_menu;

enum gui_flag 
{
    WITH_OK = 0,
    WITH_YESNO = 1,
    WITH_OKCANCEL = 2
};

enum gui_key
{
    KEY_CANCEL = '9',
    KEY_NO = '0',
    KEY_YES = '1',
    KEY_OK = '1',
};

/* Initialize GUI with the menus and stuff */
PJ_DECL(pj_status_t) gui_init(gui_menu *menu);

/* Run GUI main loop */
PJ_DECL(pj_status_t) gui_start(gui_menu *menu);

/* Signal GUI mainloop to stop */
PJ_DECL(void) gui_destroy(void);

/* AUX: display messagebox */
PJ_DECL(enum gui_key) gui_msgbox(const char *title, const char *message, enum gui_flag flag);

/* AUX: sleep */
PJ_DECL(void) gui_sleep(unsigned sec);


PJ_END_DECL


#endif	/* __GUI_H__ */

/* $Id$ */
/* 
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <winuserm.h>
#include <aygshell.h>
#include "..\pjsua_app.h"
#include "..\pjsua_app_config.h"

#define MAINWINDOWCLASS TEXT("PjsuaDlg")
#define MAINWINDOWTITLE TEXT("PJSUA")
#define LOGO_PATH TEXT("\\Program Files\\pjsua\\pjsua.bmp")

#define WM_APP_INIT	WM_USER + 1
#define WM_APP_DESTROY	WM_USER + 2
#define WM_APP_RESTART	WM_USER + 3

static HINSTANCE	 g_hInst;
static HWND		 g_hWndMenuBar;
static HWND		 g_hWndMain;
static HWND		 g_hWndLbl;
static HWND		 g_hWndImg;
static HBITMAP		 g_hBmp;

static int		 start_argc;
static char	       **start_argv;

/* Helper funtions to init/destroy the pjsua */
static void PjsuaInit();
static void PjsuaDestroy();

/* pjsua app callbacks */
static void PjsuaOnStarted(pj_status_t status, const char* title);
static void PjsuaOnStopped(pj_bool_t restart, int argc, char** argv);
static void PjsuaOnConfig(pjsua_app_config *cfg);

LRESULT CALLBACK DialogProc(const HWND hWnd,
			    const UINT Msg, 
			    const WPARAM wParam,
			    const LPARAM lParam) 
{   
    LRESULT res = 0;

    switch (Msg) {
    case WM_CREATE:
	g_hWndMain = hWnd;
	break;

    case WM_COMMAND: /* Exit menu */
    case WM_CLOSE:
	PostQuitMessage(0);
	break;

    case WM_HOTKEY:
	/* Exit app when back is pressed. */
	if (VK_TBACK == HIWORD(lParam) && (0 != (MOD_KEYUP & LOWORD(lParam)))) {
	    PostQuitMessage(0);
	} else {
	    return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
	break;

    case WM_CTLCOLORSTATIC:
	/* Set text and background color for static windows */
	SetTextColor((HDC)wParam, RGB(255, 255, 255));
	SetBkColor((HDC)wParam, RGB(0, 0, 0));
	return (LRESULT)GetStockObject(BLACK_BRUSH);

    case WM_APP_INIT:
    case WM_APP_RESTART:
	PjsuaInit();
	break;

    case WM_APP_DESTROY:
	PostQuitMessage(0);
	break;

    default:
	return DefWindowProc(hWnd, Msg, wParam, lParam);
    }

    return res;
}


/* === GUI === */

pj_status_t GuiInit()
{
    WNDCLASS wc;
    HWND hWnd = NULL;	
    RECT r;
    DWORD dwStyle;
    enum { LABEL_HEIGHT = 30 };
    enum { MENU_ID_EXIT = 50000 };
    BITMAP bmp;
    HMENU hRootMenu;
    SHMENUBARINFO mbi;

    pj_status_t status  = PJ_SUCCESS;
    
    /* Check if app is running. If it's running then focus on the window */
    hWnd = FindWindow(MAINWINDOWCLASS, MAINWINDOWTITLE);

    if (NULL != hWnd) {
	SetForegroundWindow(hWnd);    
	return status;
    }

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC)DialogProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_hInst;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName	= 0;
    wc.lpszClassName = MAINWINDOWCLASS;
    
    if (!RegisterClass(&wc) != 0) {
	DWORD err = GetLastError();
	return PJ_RETURN_OS_ERROR(err);
    }

    /* Create the app. window */
    g_hWndMain = CreateWindow(MAINWINDOWCLASS, MAINWINDOWTITLE,
			      WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 
			      CW_USEDEFAULT, CW_USEDEFAULT,
			      (HWND)NULL, NULL, g_hInst, (LPSTR)NULL);
    if (g_hWndMain == NULL) {
	DWORD err = GetLastError();
	return PJ_RETURN_OS_ERROR(err);
    }

    /* Create exit menu */
    hRootMenu = CreateMenu();
    AppendMenu(hRootMenu, MF_STRING, MENU_ID_EXIT, L"Exit");

    /* Initialize menubar */
    ZeroMemory(&mbi, sizeof(SHMENUBARINFO));
    mbi.cbSize      = sizeof(SHMENUBARINFO);
    mbi.hwndParent  = g_hWndMain;
    mbi.dwFlags	    = SHCMBF_HIDESIPBUTTON|SHCMBF_HMENU;
    mbi.nToolBarId  = (UINT)hRootMenu;
    mbi.hInstRes    = g_hInst;

    if (FALSE == SHCreateMenuBar(&mbi)) {
	DWORD err = GetLastError();
        return PJ_RETURN_OS_ERROR(err);
    }

    /* Store menu window handle */
    g_hWndMenuBar = mbi.hwndMB;

    /* Show the menu */
    DrawMenuBar(g_hWndMain);
    ShowWindow(g_hWndMenuBar, SW_SHOW);

    /* Override back button */
    SendMessage(g_hWndMenuBar, SHCMBM_OVERRIDEKEY, VK_TBACK,
	    MAKELPARAM(SHMBOF_NODEFAULT | SHMBOF_NOTIFY,
	    SHMBOF_NODEFAULT | SHMBOF_NOTIFY));

    /* Get main window size */
    GetClientRect(g_hWndMain, &r);
#if defined(WIN32_PLATFORM_PSPC) && WIN32_PLATFORM_PSPC != 0
    /* Adjust the height for PocketPC platform */
    r.bottom -= GetSystemMetrics(SM_CYMENU);
#endif

    /* Create logo */
    g_hBmp = SHLoadDIBitmap(LOGO_PATH); /* for jpeg, uses SHLoadImageFile() */
    if (g_hBmp == NULL) {
	DWORD err = GetLastError();
	return PJ_RETURN_OS_ERROR(err);
    }
    GetObject(g_hBmp, sizeof(bmp), &bmp);

    dwStyle = SS_CENTERIMAGE | SS_REALSIZEIMAGE | SS_BITMAP |
	      WS_CHILD | WS_VISIBLE;
    g_hWndImg = CreateWindow(TEXT("STATIC"), NULL, dwStyle,
			     (r.right-r.left-bmp.bmWidth)/2,
			     (r.bottom-r.top-bmp.bmHeight)/2,
			     bmp.bmWidth, bmp.bmHeight,
			     g_hWndMain, (HMENU)0, g_hInst, NULL);
    if (g_hWndImg == NULL) {
	DWORD err = GetLastError();
	return PJ_RETURN_OS_ERROR(err);
    }
    SendMessage(g_hWndImg, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)g_hBmp);

    /* Create label */
    dwStyle = WS_CHILD | WS_VISIBLE | ES_CENTER;
    g_hWndLbl = CreateWindow(TEXT("STATIC"), NULL, dwStyle,
		0, r.bottom-LABEL_HEIGHT, r.right-r.left, LABEL_HEIGHT,
                g_hWndMain, (HMENU)0, g_hInst, NULL);
    if (g_hWndLbl == NULL) {
	DWORD err = GetLastError();
	return PJ_RETURN_OS_ERROR(err);
    }
    SetWindowText(g_hWndLbl, _T("Please wait.."));

    return status;
}


pj_status_t GuiStart()
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (msg.wParam);
}

void GuiDestroy(void)
{
    if (g_hWndMain) {
	DestroyWindow(g_hWndMain);
	g_hWndMain = NULL;
    }
    if (g_hWndMenuBar) {
	DestroyWindow(g_hWndMenuBar);
	g_hWndMenuBar = NULL;
    }
    if (g_hWndLbl) {
	DestroyWindow(g_hWndLbl);
	g_hWndLbl = NULL;
    }
    if (g_hWndImg) {
	DestroyWindow(g_hWndImg);
	g_hWndImg = NULL;
    }
    if (g_hBmp) {
	DeleteObject(g_hBmp);
	g_hBmp = NULL;
    }
    UnregisterClass(MAINWINDOWCLASS, g_hInst);
}

/* === ENGINE === */

/* Called when pjsua is started */
void PjsuaOnStarted(pj_status_t status, const char* title)
{
    wchar_t wtitle[128];
    char err_msg[128];

    if (status != PJ_SUCCESS || title == NULL) {
	char err_str[PJ_ERR_MSG_SIZE];
	pj_strerror(status, err_str, sizeof(err_str));
	pj_ansi_snprintf(err_msg, sizeof(err_msg), "%s: %s",
			 (title?title:"App start error"), err_str);
	title = err_msg;
    }

    pj_ansi_to_unicode(title, strlen(title), wtitle, PJ_ARRAY_SIZE(wtitle));
    SetWindowText(g_hWndLbl, wtitle);
}

/* Called when pjsua is stopped */
void PjsuaOnStopped(pj_bool_t restart, int argc, char** argv)
{
    if (restart) {
	start_argc = argc;
	start_argv = argv;

	// Schedule Lib Restart
	PostMessage(g_hWndMain, WM_APP_RESTART, 0, 0);
    } else {
	/* Destroy & quit GUI, e.g: clean up window, resources  */
	PostMessage(g_hWndMain, WM_APP_DESTROY, 0, 0);
    }
}

/* Called before pjsua initializing config. */
void PjsuaOnConfig(pjsua_app_config *cfg)
{
    PJ_UNUSED_ARG(cfg);
}

void PjsuaInit()
{
    pjsua_app_cfg_t app_cfg;
    pj_status_t status;

    /* Destroy pjsua app first */
    pjsua_app_destroy();

    /* Init pjsua app */
    pj_bzero(&app_cfg, sizeof(app_cfg));
    app_cfg.argc = start_argc;
    app_cfg.argv = start_argv;
    app_cfg.on_started = &PjsuaOnStarted;
    app_cfg.on_stopped = &PjsuaOnStopped;
    app_cfg.on_config_init = &PjsuaOnConfig;

    SetWindowText(g_hWndLbl, _T("Initializing.."));
    status = pjsua_app_init(&app_cfg);
    if (status != PJ_SUCCESS)
	goto on_return;
    
    SetWindowText(g_hWndLbl, _T("Starting.."));
    status = pjsua_app_run(PJ_FALSE);
    if (status != PJ_SUCCESS)
	goto on_return;

on_return:
    if (status != PJ_SUCCESS)
	SetWindowText(g_hWndLbl, _T("Initialization failed"));
}

void PjsuaDestroy()
{
    pjsua_app_destroy();
}

/* === MAIN === */

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine,
    int nShowCmd
)
{
    int status;

    PJ_UNUSED_ARG(hPrevInstance);
    PJ_UNUSED_ARG(lpCmdLine);
    PJ_UNUSED_ARG(nShowCmd);

    // store the hInstance in global
    g_hInst = hInstance;

    // Start GUI
    status = GuiInit();
    if (status != 0)
	goto on_return;

    // Setup args and start pjsua
    start_argc = pjsua_app_def_argc;
    start_argv = (char**)pjsua_app_def_argv;
    PostMessage(g_hWndMain, WM_APP_INIT, 0, 0);

    status = GuiStart();
	
on_return:
    PjsuaDestroy();
    GuiDestroy();

    return status;
}

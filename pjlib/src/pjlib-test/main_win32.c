/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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
#include "test.h"

#include <pj/string.h>
#include <pj/compat/unicode.h>
#include <pj/sock.h>
#include <pj/log.h>

#include <windows.h>
#include <commctrl.h>

#define MAX_LOADSTRING 100

#define IDC_HELLO_WINCE		3
#define ID_LOGWINDOW		104
///#define IDI_HELLO_WINCE		101
///#define IDM_MENU		102
///#define IDD_ABOUTBOX		103
///#define IDM_FILE_EXIT		40002
///#define IDM_HELP_ABOUT		40003

// Global Variables:
HINSTANCE			hInst;			// The current instance
///HWND				hwndCB;			// The command bar handle
HWND				hwLogWnd;

// Forward declarations of functions included in this code module:
ATOM			MyRegisterClass	(HINSTANCE, LPTSTR);
BOOL			InitInstance	(HINSTANCE, int);
LRESULT CALLBACK	WndProc		(HWND, UINT, WPARAM, LPARAM);
///LRESULT CALLBACK	About		(HWND, UINT, WPARAM, LPARAM);

static TCHAR logbuf[8192];
PJ_DECL_UNICODE_TEMP_BUF(wdata,256);

static void write_log(int level, const char *data, int len)
{
    GetWindowText(hwLogWnd, logbuf, PJ_ARRAY_SIZE(logbuf));
    wcscat(logbuf, PJ_NATIVE_STRING(data,wdata));
    SetWindowText(hwLogWnd, logbuf);
    UpdateWindow(hwLogWnd);
}


int WINAPI WinMain(HINSTANCE hInstance,
		   HINSTANCE hPrevInstance,
		   LPTSTR    lpCmdLine,
		   int       nCmdShow)
{
    MSG msg;
    HACCEL hAccelTable;
    
    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow)) 
    {
	return FALSE;
    }
    
    pj_log_set_log_func( &write_log );
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR);

    test_main();

    hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_HELLO_WINCE);
    
    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0)) 
    {
	if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
	{
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
    }
    
    return msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    It is important to call this function so that the application 
//    will get 'well formed' small icons associated with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
    WNDCLASS	wc;
    
    wc.style		= CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc	= (WNDPROC) WndProc;
    wc.cbClsExtra	= 0;
    wc.cbWndExtra	= 0;
    wc.hInstance	= hInstance;
    ///wc.hIcon		= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HELLO_WINCE));
    wc.hIcon		= NULL;
    wc.hCursor		= 0;
    wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName	= 0;
    wc.lpszClassName	= szWindowClass;
    
    return RegisterClass(&wc);
}

//
//  FUNCTION: InitInstance(HANDLE, int)
//
//  PURPOSE: Saves instance handle and creates main window
//
//  COMMENTS:
//
//    In this function, we save the instance handle in a global variable and
//    create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND	hWnd;
    TCHAR	*szTitle = L"PJSIP Test";
    TCHAR	*szWindowClass = L"PJSIP_TEST";
    
    hInst = hInstance;		// Store instance handle in our global variable
    
    MyRegisterClass(hInstance, szWindowClass);
    
    hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE,
	CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
    
    if (!hWnd)
    {	
	return FALSE;
    }
    
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    ///if (hwndCB)
    ///	CommandBar_Show(hwndCB, TRUE);
    if (hwLogWnd)
	ShowWindow(hwLogWnd, TRUE);
    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    RECT rt;
    DWORD dwStyle;
    TCHAR *szHello = L"Hello world!";
    
    switch (message) 
    {
    case WM_COMMAND:
	wmId    = LOWORD(wParam); 
	wmEvent = HIWORD(wParam); 
	// Parse the menu selections:
	switch (wmId)
	{
	///case IDM_HELP_ABOUT:
	    ///DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
	///    break;
	///case IDM_FILE_EXIT:
	///    DestroyWindow(hWnd);
	///    break;
	default:
	    return DefWindowProc(hWnd, message, wParam, lParam);
	}
	break;
	case WM_CREATE:
	    ///hwndCB = CommandBar_Create(hInst, hWnd, 1);			
	    ///CommandBar_InsertMenubar(hwndCB, hInst, IDM_MENU, 0);
	    ///CommandBar_AddAdornments(hwndCB, 0, 0);
	    GetClientRect(hWnd, &rt);
	    dwStyle = WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | 
		      WS_BORDER | ES_LEFT | ES_MULTILINE | ES_NOHIDESEL | 
		      ES_AUTOHSCROLL | ES_AUTOVSCROLL; 
	    hwLogWnd = CreateWindow( TEXT("edit"),  // class
				     NULL,	    // window text
				     dwStyle,	    // style
				     0,		    // x-left
				     0,		    // y-top
				     rt.right-rt.left, // w
				     rt.bottom-rt.top, // h
				     hWnd,	    // parent
				     (HMENU)ID_LOGWINDOW,   // id
				     hInst,	    // instance
				     NULL);	    // NULL for control.
	    break;
	case WM_PAINT:
	    ///hdc = BeginPaint(hWnd, &ps);
	    ///GetClientRect(hWnd, &rt);
	    ///DrawText(hdc, szHello, _tcslen(szHello), &rt, 
	    ///	DT_SINGLELINE | DT_VCENTER | DT_CENTER);
	    ///EndPaint(hWnd, &ps);
	    break;
	case WM_ACTIVATE:
	    if (LOWORD(wParam) == WA_INACTIVE)
		DestroyWindow(hWnd);
	    break;
	case WM_CLOSE:
	    DestroyWindow(hWnd);
	    break;
	case WM_DESTROY:
	    ///CommandBar_Destroy(hwndCB);
	    PostQuitMessage(0);
	    break;
	default:
	    return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


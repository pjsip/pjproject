/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include "pjsua_app.h"

#define THIS_FILE	"main.c"

static pj_bool_t	    running = PJ_TRUE;
static pj_status_t	    receive_end_sig;
static pj_thread_t	    *sig_thread;
static pjsua_app_cfg_t	    cfg;

/* Called when CLI (re)started */
void on_app_started(pj_status_t status, const char *msg)
{
    pj_perror(3, THIS_FILE, status, (msg)?msg:"");
}

void on_app_stopped(pj_bool_t restart, int argc, char** argv)
{
    if (argv) {
	cfg.argc = argc;
	cfg.argv = argv;
    }

    running = restart;
}

#if defined(PJ_WIN32) && PJ_WIN32!=0
#include <windows.h>

static pj_thread_desc handler_desc;

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{   
    switch (fdwCtrlType) 
    { 
        // Handle the CTRL+C signal. 
 
        case CTRL_C_EVENT: 
        case CTRL_CLOSE_EVENT: 
        case CTRL_BREAK_EVENT: 
        case CTRL_LOGOFF_EVENT: 
        case CTRL_SHUTDOWN_EVENT: 
	    pj_thread_register("ctrlhandler", handler_desc, &sig_thread);
	    PJ_LOG(3,(THIS_FILE, "Ctrl-C detected, quitting.."));
	    receive_end_sig = PJ_TRUE;
            pjsua_app_destroy();	    
	    ExitProcess(1);
            PJ_UNREACHED(return TRUE;)
 
        default: 
 
            return FALSE; 
    } 
}

static void setup_socket_signal()
{
}

static void setup_signal_handler(void)
{
    SetConsoleCtrlHandler(&CtrlHandler, TRUE);
}

#elif PJ_LINUX || PJ_DARWINOS

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static void print_stack(int sig)
{
    void *array[16];
    size_t size;

    size = backtrace(array, 16);
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

static void setup_socket_signal()
{
    signal(SIGPIPE, SIG_IGN);
}

static void setup_signal_handler(void)
{
    signal(SIGSEGV, &print_stack);
}

#else

#include <signal.h>

static void setup_socket_signal()
{
    signal(SIGPIPE, SIG_IGN);
}

static void setup_signal_handler(void) {}

#endif

int main_func(int argc, char *argv[])
{
    pj_status_t status = PJ_TRUE;

    pj_bzero(&cfg, sizeof(cfg));
    cfg.on_started = &on_app_started;
    cfg.on_stopped = &on_app_stopped;
    cfg.argc = argc;
    cfg.argv = argv;

    setup_signal_handler();
    setup_socket_signal();

    while (running) {        
	status = pjsua_app_init(&cfg);
	if (status == PJ_SUCCESS) {
	    status = pjsua_app_run(PJ_TRUE);
	} else {
	    running = PJ_FALSE;
	}

	if (!receive_end_sig) {
	    pjsua_app_destroy();

	    /* This is on purpose */
	    pjsua_app_destroy();
	} else {
	    pj_thread_join(sig_thread);
	}
    }
    return 0;
}

int main(int argc, char *argv[])
{
    return pj_run_app(&main_func, argc, argv, 0);
}

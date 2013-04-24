/* $Id: pjsua_app_callback.cpp $ */
/* 
 * Copyright (C) 2012-2012 Teluu Inc. (http://www.teluu.com)
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

#include "pjsua_app_callback.h"
#include "../../pjsua_app.h"
#include "../../pjsua_app_config.h"

#if defined(PJ_ANDROID) && PJ_ANDROID != 0

static PjsuaAppCallback* registeredCallbackObject = NULL;
static pjsua_app_cfg_t android_app_config;
static int restart_argc;
static char **restart_argv;

extern const char *pjsua_app_def_argv[];

#define THIS_FILE	"pjsua_app_callback.cpp"

/** Callback wrapper **/
void on_cli_started(pj_status_t status, const char *msg)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    if (registeredCallbackObject) {
	if ((status != PJ_SUCCESS) && (!msg || !*msg)) {
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    msg = errmsg;
	}
	registeredCallbackObject->onStarted(msg);
    }
}

void on_cli_stopped(pj_bool_t restart, int argc, char **argv)
{
    if (restart) {
	restart_argc = argc;
	restart_argv = argv;
    }

    if (registeredCallbackObject) {
	registeredCallbackObject->onStopped(restart);
    }
}

static int initMain(int argc, char **argv)
{
    pj_status_t status;
    android_app_config.argc = argc;
    android_app_config.argv = argv;

    status = pjsua_app_init(&android_app_config);
    if (status == PJ_SUCCESS) {
	status = pjsua_app_run(PJ_FALSE);
    } else {
	pjsua_app_destroy();
    }

    return status;
}

int pjsuaStart()
{
    pj_status_t status;

    const char **argv = pjsua_app_def_argv;
    int argc = pjsua_app_def_argc;

    pj_bzero(&android_app_config, sizeof(android_app_config));

    android_app_config.on_started = &on_cli_started;
    android_app_config.on_stopped = &on_cli_stopped;

    return initMain(argc, (char**)argv);
}

void pjsuaDestroy()
{
    pjsua_app_destroy();

    /** This is on purpose **/
    pjsua_app_destroy();
}

int pjsuaRestart()
{
    pj_status_t status;

    pjsuaDestroy();

    return initMain(restart_argc, restart_argv);
}

void setCallbackObject(PjsuaAppCallback* callback)
{
    registeredCallbackObject = callback;
}

#endif

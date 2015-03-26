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

#include <android/log.h>

static PjsuaAppCallback* registeredCallbackObject = NULL;
static pjsua_app_cfg_t android_app_config;
static int restart_argc;
static char **restart_argv;
static pjsua_callback pjsua_cb_orig;
static jobject callVideoSurface;

extern const char *pjsua_app_def_argv[];

#define THIS_FILE	"pjsua_app_callback.cpp"

extern "C" {
static void log_writer(int level, const char *data, int len)
{
    __android_log_write(ANDROID_LOG_INFO, "pjsua", data);
}
}

static void on_call_media_state(pjsua_call_id call_id)
{
#if PJMEDIA_HAS_VIDEO
    pjsua_call_info call_info;
    unsigned mi;

    pjsua_call_get_info(call_id, &call_info);

    for (mi=0; mi<call_info.media_cnt; ++mi) {
        pjsua_call_media_info *med_info = &call_info.media[mi];
	if (med_info->type == PJMEDIA_TYPE_VIDEO &&
	    med_info->status == PJSUA_CALL_MEDIA_ACTIVE &&
	    med_info->stream.vid.win_in != PJSUA_INVALID_ID)
	{
	    pjmedia_vid_dev_hwnd vhwnd;

	    /* Setup renderer surface */
	    pj_bzero(&vhwnd, sizeof(vhwnd));
	    vhwnd.type = PJMEDIA_VID_DEV_HWND_TYPE_ANDROID;
	    vhwnd.info.window = callVideoSurface;
	    pjsua_vid_win_set_win(med_info->stream.vid.win_in, &vhwnd);
	    break;
	}
    }
#endif
    
    /* Forward to original callback */
    if (pjsua_cb_orig.on_call_media_state)
	(*pjsua_cb_orig.on_call_media_state)(call_id);
}

/** Callback wrapper **/
static void on_cli_config(pjsua_app_config *cfg)
{
    pjsua_cb_orig = cfg->cfg.cb;
    cfg->log_cfg.cb = &log_writer;
    
    /* Override pjsua callback, e.g: to install renderer view */
    cfg->cfg.cb.on_call_media_state = &on_call_media_state;
}

static void on_cli_started(pj_status_t status, const char *msg)
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

static void on_cli_stopped(pj_bool_t restart, int argc, char **argv)
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

    android_app_config.on_config_init = &on_cli_config;
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


void setIncomingVideoRenderer(jobject surface)
{
    callVideoSurface = surface;
}

#endif

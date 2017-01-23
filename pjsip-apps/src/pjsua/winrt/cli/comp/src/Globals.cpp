/* $Id$ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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

#include "Globals.h"
#include "../../../../../pjsua_app.h"
#include "../../../../../pjsua_app_config.h"
#include <memory>
#include "PjsuaCallback.h"

using namespace PjsuaCLI::BackEnd;
using namespace Platform;

#define THIS_FILE	"Globals.cpp"

#if defined (PJ_WIN32_WINPHONE8) && (PJ_WIN32_WINPHONE8 != 0)

static pjsua_app_cfg_t wp_app_config;
static int restart_argc;
static char **restart_argv;
extern const char *pjsua_app_def_argv[];

Globals^ Globals::singleton = nullptr;

/** Callback wrapper **/
void on_cli_started(pj_status_t status, const char *msg)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    if (Globals::Instance->PjsuaCallback) {
	static wchar_t msgBuff[ PJ_ERR_MSG_SIZE ];
	Platform::String ^outMsg = nullptr;
	if ((status != PJ_SUCCESS) && (!msg || !*msg)) {
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    msg = errmsg;
	}
	mbstowcs(msgBuff, msg, PJ_ERR_MSG_SIZE);
	outMsg = ref new Platform::String(msgBuff);
	Globals::Instance->PjsuaCallback->OnStarted(outMsg);
    }
}

void on_cli_stopped(pj_bool_t restart, int argc, char **argv)
{
    if (restart) {
	restart_argc = argc;
	restart_argv = argv;
    }

    if (Globals::Instance->PjsuaCallback) {
	Globals::Instance->PjsuaCallback->OnStopped(restart);
    }
}

void on_config_init (pjsua_app_config *cfg)
{
    PJ_UNUSED_ARG(cfg);
    //cfg->media_cfg.snd_clock_rate = 16000;
    //cfg->media_cfg.snd_play_latency = 140;
}

static int initMain(int argc, char **argv)
{
    pj_status_t status;
    wp_app_config.argc = argc;
    wp_app_config.argv = argv;

    status = pjsua_app_init(&wp_app_config);
    if (status == PJ_SUCCESS) {
	status = pjsua_app_run(PJ_FALSE);
    } else {
	pjsua_app_destroy();
    }

    return status;
}

int Globals::pjsuaStart()
{
    const char **argv = pjsua_app_def_argv;
    int argc = pjsua_app_def_argc;

    pj_bzero(&wp_app_config, sizeof(wp_app_config));

    wp_app_config.on_started = &on_cli_started;
    wp_app_config.on_stopped = &on_cli_stopped;
    wp_app_config.on_config_init = &on_config_init;

    return initMain(argc, (char**)argv);
}

void Globals::pjsuaDestroy()
{
    pjsua_app_destroy();

    /** This is on purpose **/
    pjsua_app_destroy();
}

int Globals::pjsuaRestart()
{
    pjsuaDestroy();

    return initMain(restart_argc, restart_argv);
}

Globals^ Globals::Instance::get()
{
    if (Globals::singleton == nullptr)
    {
        if (Globals::singleton == nullptr)
        {
            Globals::singleton = ref new Globals();
        }
    }

    return Globals::singleton;
}

PjsuaCallback^ Globals::PjsuaCallback::get()
{
    if (this->callback == nullptr)
    {
	if (this->callback == nullptr)
        {
	    this->callback = ref new PjsuaCLI::BackEnd::PjsuaCallback();
        }
    }

    return this->callback;
}

Globals::Globals()
{
    this->callback = ref new PjsuaCLI::BackEnd::PjsuaCallback();
}

Globals::~Globals()
{

}

#endif
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

#ifndef __PJSUA_APP_H__
#define __PJSUA_APP_H__

/**
 * Interface for user application to use pjsua with CLI/menu based UI. 
 */

#include "pjsua_common.h"

PJ_BEGIN_DECL

/**
 * This structure contains the configuration of application.
 */
typedef struct app_cfg_t
{
    /**
     * The number of runtime arguments passed to the application.
     */
    int       argc;

    /**
     * The array of arguments string passed to the application. 
     */
    char    **argv;

    /** 
     * Tell app that CLI (and pjsua) is (re)started.
     * msg will contain start error message such as “Telnet to X:Y”,
     * “failed to start pjsua lib”, “port busy”..
     */
    void (*on_started)(pj_status_t status, const char* title);

    /**
     * Tell app that library request to stopped/restart.
     * GUI app needs to use a timer mechanism to wait before invoking the 
     * cleanup procedure.
     */
    pj_bool_t (*on_stopped)(pj_bool_t restart, int argc, char** argv);

    /**
     * This will enable application to supply customize configuration other than
     * the basic configuration provided by pjsua. 
     */
    void (*on_config_init)(pjsua_app_config *cfg);
} app_cfg_t;

/**
 * This will initiate the pjsua and the user interface (CLI/menu UI) based on 
 * the provided configuration.
 */
PJ_DECL(pj_status_t) app_init(const app_cfg_t *app_cfg);

/**
 * This will run the CLI/menu based UI.
 * wait_telnet_cli is used for CLI based UI. It will tell the library to block
 * or wait until user invoke the "shutdown"/"restart" command. GUI based app
 * should define this param as PJ_FALSE.
 */
PJ_DECL(pj_status_t) app_run(pj_bool_t wait_telnet_cli);

/**
 * This will destroy/cleanup the application library.
 */
PJ_DECL(pj_status_t) app_destroy();

PJ_END_DECL
    
#endif	/* __PJSUA_APP_H__ */

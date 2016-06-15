/* $Id$ */
/* 
 * Copyright (C) 2010 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJLIB_UTIL_CLI_TELNET_H__
#define __PJLIB_UTIL_CLI_TELNET_H__

/**
 * @file cli_telnet.h
 * @brief Command Line Interface Telnet Front End API
 */

#include <pjlib-util/cli_imp.h>

PJ_BEGIN_DECL

/**
 * @ingroup PJLIB_UTIL_CLI_IMP
 * @{
 *
 */

 /**
 * This structure contains the information about the telnet.
 * Application will get updated information each time the telnet is started/
 * restarted.
 */
typedef struct pj_cli_telnet_info
{
    /**
     * The telnet's ip address.
     */
    pj_str_t	ip_address;

    /**
     * The telnet's port number.
     */
    pj_uint16_t port;

    /* Internal buffer for IP address */
    char buf_[32];

} pj_cli_telnet_info;

/**
 * This specifies the callback called when telnet is started
 *
 * @param status	The status of telnet startup process.
 *
 */
typedef void (*pj_cli_telnet_on_started)(pj_status_t status);

/**
 * This structure contains various options to instantiate the telnet daemon.
 * Application must call pj_cli_telnet_cfg_default() to initialize
 * this structure with its default values.
 */
typedef struct pj_cli_telnet_cfg
{
    /**
     * Listening port number. The value may be 0 to let the system choose
     * the first available port.
     *
     * Default value: PJ_CLI_TELNET_PORT
     */
    pj_uint16_t port;

    /**
     * Ioqueue instance to be used. If this field is NULL, an internal
     * ioqueue and worker thread will be created.
     */
    pj_ioqueue_t *ioqueue;

    /**
     * Default log verbosity level for the session.
     *
     * Default value: PJ_CLI_TELNET_LOG_LEVEL
     */
    int log_level;

    /**
     * Specify a password to be asked to the end user to access the
     * application. Currently this is not implemented yet.
     *
     * Default: empty (no password)
     */
    pj_str_t passwd;

    /**
     * Specify text message to be displayed to newly connected users.
     * Currently this is not implemented yet.
     *
     * Default: empty
     */
    pj_str_t welcome_msg;

    /**
     * Specify text message as a prompt string to user.
     *
     * Default: empty
     */
    pj_str_t prompt_str;

    /**
     * Specify the pj_cli_telnet_on_started callback.
     *
     * Default: empty
     */
    pj_cli_telnet_on_started on_started;

} pj_cli_telnet_cfg;

/**
 * Initialize pj_cli_telnet_cfg with its default values.
 *
 * @param param		The structure to be initialized.
 */
PJ_DECL(void) pj_cli_telnet_cfg_default(pj_cli_telnet_cfg *param);


/**
 * Create, initialize, and start a telnet daemon for the application.
 *
 * @param cli		The CLI application instance.
 * @param param		Optional parameters for creating the telnet daemon.
 * 			If this value is NULL, default parameters will be used.
 * @param p_fe		Optional pointer to receive the front-end instance
 * 			of the telnet front-end just created.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_cli_telnet_create(pj_cli_t *cli,
					  pj_cli_telnet_cfg *param,
					  pj_cli_front_end **p_fe);


/**
 * Retrieve cli telnet info.
 *
 * @param telnet_info   The telnet runtime information.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_cli_telnet_get_info(pj_cli_front_end *fe, 
					    pj_cli_telnet_info *info); 

/**
 * @}
 */

PJ_END_DECL

#endif /* __PJLIB_UTIL_CLI_TELNET_H__ */

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
#ifndef __PJLIB_UTIL_CLI_CONSOLE_H__
#define __PJLIB_UTIL_CLI_CONSOLE_H__

/**
 * @file cli_console.h
 * @brief Command Line Interface Console Front End API
 */

#include <pjlib-util/cli_imp.h>


PJ_BEGIN_DECL

/**
 * @ingroup PJLIB_UTIL_CLI_IMP
 * @{
 *
 */


/**
 * This structure contains various options for CLI console front-end.
 * Application must call pj_cli_console_cfg_default() to initialize
 * this structure with its default values.
 */
typedef struct pj_cli_console_cfg
{
    /**
     * Default log verbosity level for the session.
     *
     * Default value: PJ_CLI_CONSOLE_LOG_LEVEL
     */
    int log_level;

    /**
     * Specify text message as a prompt string to user.
     *
     * Default: empty
     */
    pj_str_t prompt_str;

} pj_cli_console_cfg;


/**
 * Initialize pj_cli_console_cfg with its default values.
 *
 * @param param		The structure to be initialized.
 */
PJ_DECL(void) pj_cli_console_cfg_default(pj_cli_console_cfg *param);


/**
 * Create a console front-end for the specified CLI application, and return
 * the only session instance for the console front end. On Windows operating
 * system, this may open a new console window.
 *
 * @param cli		The CLI application instance.
 * @param param		Optional console CLI parameters. If this value is
 * 			NULL, default parameters will be used.
 * @param p_sess	Pointer to receive the session instance for the
 * 			console front end.
 * @param p_fe		Optional pointer to receive the front-end instance
 * 			of the console front-end just created.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_cli_console_create(pj_cli_t *cli,
					   const pj_cli_console_cfg *param,
					   pj_cli_sess **p_sess,
					   pj_cli_front_end **p_fe);

/**
 * Retrieve a cmdline from console stdin and process the input accordingly.
 *
 * @param sess		The CLI session.
 * @param buf		Pointer to receive the buffer.
 * @param maxlen	Maximum length to read.
 *
 * @return		PJ_SUCCESS if an input was read
 */
PJ_DECL(pj_status_t) pj_cli_console_process(pj_cli_sess *sess);

/**
 * @}
 */

PJ_END_DECL

#endif /* __PJLIB_UTIL_CLI_CONSOLE_H__ */

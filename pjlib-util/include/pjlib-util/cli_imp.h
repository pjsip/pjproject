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
#ifndef __PJLIB_UTIL_CLI_IMP_H__
#define __PJLIB_UTIL_CLI_IMP_H__

/**
 * @file cli_imp.h
 * @brief Command Line Interface Implementor's API
 */

#include <pjlib-util/cli.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJLIB_UTIL_CLI_IMP Command Line Interface Implementor's API
 * @ingroup PJLIB_UTIL_CLI
 * @{
 *
 */

/**
 * Default log level for console sessions.
 */
#ifndef PJ_CLI_CONSOLE_LOG_LEVEL
#   define PJ_CLI_CONSOLE_LOG_LEVEL	PJ_LOG_MAX_LEVEL
#endif

/**
 * Default log level for telnet sessions.
 */
#ifndef PJ_CLI_TELNET_LOG_LEVEL
#   define PJ_CLI_TELNET_LOG_LEVEL	4
#endif

/**
 * Default port number for telnet daemon.
 */
#ifndef PJ_CLI_TELNET_PORT
#   define PJ_CLI_TELNET_PORT		0
#endif

/**
 * This enumeration specifies front end types.
 */
typedef enum pj_cli_front_end_type
{
    PJ_CLI_CONSOLE_FRONT_END,	/**< Console front end.	*/
    PJ_CLI_TELNET_FRONT_END,	/**< Telnet front end.	*/
    PJ_CLI_HTTP_FRONT_END,	/**< HTTP front end.	*/
    PJ_CLI_GUI_FRONT_END	/**< GUI front end.	*/
} pj_cli_front_end_type;


/**
 * Front end operations. Only the CLI should call these functions
 * directly.
 */
typedef struct pj_cli_front_end_op
{
    /**
     * Callback to write a log message to the appropriate sessions belonging
     * to this front end. The front end would only send the log message to
     * the session if the session's log verbosity level is greater than the
     * level of this log message.
     *
     * @param fe	The front end.
     * @param level	Verbosity level of this message message.
     * @param data	The message itself.
     * @param len 	Length of this message.
     */
    void (*on_write_log)(pj_cli_front_end *fe, int level,
		         const char *data, pj_size_t len);

    /**
     * Callback to be called when the application is quitting, to signal the
     * front-end to end its main loop or any currently blocking functions,
     * if any.
     *
     * @param fe	The front end.
     * @param req	The session which requested the application quit.
     */
    void (*on_quit)(pj_cli_front_end *fe, pj_cli_sess *req);

    /**
     * Callback to be called to close and self destroy the front-end. This
     * must also close any active sessions created by this front-ends.
     *
     * @param fe	The front end.
     */
    void (*on_destroy)(pj_cli_front_end *fe);

} pj_cli_front_end_op;


/**
 * This structure describes common properties of CLI front-ends. A front-
 * end is a mean to interact with end user, for example the CLI application
 * may interact with console, telnet, web, or even a GUI.
 *
 * Each end user's interaction will create an instance of pj_cli_sess.
 *
 * Application instantiates the front end by calling the appropriate
 * function to instantiate them.
 */
struct pj_cli_front_end
{
    /**
     * Linked list members
     */
    PJ_DECL_LIST_MEMBER(struct pj_cli_front_end);

    /**
     * Front end type.
     */
    pj_cli_front_end_type type;

    /**
     * The CLI application.
     */
    pj_cli_t *cli;

    /**
     * Front end operations.
     */
    pj_cli_front_end_op *op;
};


/**
 * Session operations.
 */
typedef struct pj_cli_sess_op
{
    /**
     * Callback to be called to close and self destroy the session.
     *
     * @param sess	The session to destroy.
     */
    void (*destroy)(pj_cli_sess *sess);

} pj_cli_sess_op;


/**
 * This structure describes common properties of a CLI session. A CLI session
 * is the interaction of an end user to the CLI application via a specific
 * renderer, where the renderer can be console, telnet, web, or a GUI app for
 * mobile. A session is created by its renderer, and it's creation procedures
 * vary among renderer (for example, a telnet session is created when the
 * end user connects to the application, while a console session is always
 * instantiated once the program is run).
 */
struct pj_cli_sess
{
    /**
     * Linked list members
     */
    PJ_DECL_LIST_MEMBER(struct pj_cli_sess);

    /**
     * Pointer to the front-end instance which created this session.
     */
    pj_cli_front_end *fe;

    /**
     * Session operations.
     */
    pj_cli_sess_op *op;

    /**
     * Text containing session info, which is filled by the renderer when
     * the session is created.
     */
    pj_str_t info;

    /**
     * Log verbosity of this session.
     */
    int log_level;

};

/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJLIB_UTIL_CLI_IMP_H__ */

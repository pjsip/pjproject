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
#ifndef __PJLIB_UTIL_CLI_H__
#define __PJLIB_UTIL_CLI_H__

/**
 * @file cli.h
 * @brief Command Line Interface
 */

#include <pjlib-util/types.h>
#include <pj/list.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJLIB_UTIL_CLI Command Line Interface Framework
 * @{
 * A CLI framework features an interface for defining command specification, 
 * parsing, and executing a command. 
 * It also features an interface to communicate with various front-ends, 
 * such as console, telnet.
 * Application normally needs only one CLI instance to be created. 
 * On special cases, application could also create multiple CLI 
 * instances, with each instance has specific command structure.
 *
\verbatim
| vid help                  Show this help screen                             |
| vid enable|disable        Enable or disable video in next offer/answer      |
| vid call add              Add video stream for current call                 |
| vid call cap N ID         Set capture dev ID for stream #N in current call  |
| disable_codec g711|g722   Show this help screen                             |
<CMD name='vid' id='0' desc="">
       <CMD name='help' id='0' desc='' />
       <CMD name='enable' id='0' desc='' />
       <CMD name='disable' id='0' desc='' />
       <CMD name='call' id='0' desc='' >
                <CMD name='add' id='101' desc='...' />
                <CMD name='cap' id='102' desc='...' >
                   <ARG name='streamno' type='int' desc='...' id='1'/>
                   <ARG name='devid' type='int' optional='1' id='2'/>
                </CMD>
       </CMD>
</CMD>
<CMD name='disable_codec' id=0 desc="">
        <ARG name='codec_list' type='choice' id='3'>
            <CHOICE value='g711'/>
            <CHOICE value='g722'/>
        </ARG>
</CMD>
\endverbatim 
 */

/**
 * This opaque structure represents a CLI application. A CLI application is
 * the root placeholder of other CLI objects. In an application, one (and
 * normally only one) CLI application instance needs to be created.
 */
typedef struct pj_cli_t pj_cli_t;

/**
 * Type of command id.
 */
typedef int pj_cli_cmd_id;

/**
 * This describes the parameters to be specified when creating a CLI
 * application with pj_cli_create(). Application MUST initialize this
 * structure by calling pj_cli_cfg_default() before using it.
 */
typedef struct pj_cli_cfg
{
    /**
     * The application name, which will be used in places such as logs.
     * This field is mandatory.
     */
    pj_str_t name;

    /**
     * Optional application title, which will be used in places such as
     * window title. If not specified, the application name will be used
     * as the title.
     */
    pj_str_t title;

    /**
     * The pool factory where all memory allocations will be taken from.
     * This field is mandatory.
     */
    pj_pool_factory *pf;

} pj_cli_cfg;

/**
 * Type of argument id.
 */
typedef int pj_cli_arg_id;

/**
 * Forward declaration of pj_cli_cmd_spec structure.
 */
typedef struct pj_cli_cmd_spec pj_cli_cmd_spec;

/**
 * Forward declaration for pj_cli_sess, which will be declared in cli_imp.h.
 */
typedef struct pj_cli_sess pj_cli_sess;

/**
 * Forward declaration for CLI front-end.
 */
typedef struct pj_cli_front_end pj_cli_front_end;

/**
 * Forward declaration for CLI argument spec structure.
 */
typedef struct pj_cli_arg_spec pj_cli_arg_spec;

/**
 * This structure contains the command to be executed by command handler.
 */
typedef struct pj_cli_cmd_val
{
    /** The session on which the command was executed on. */
    pj_cli_sess *sess;

    /** The command specification being executed. */
    const pj_cli_cmd_spec *cmd;

    /** Number of argvs. */
    int argc;

    /** Array of args, with argv[0] specifies the  name of the cmd. */
    pj_str_t argv[PJ_CLI_MAX_ARGS];

} pj_cli_cmd_val;

/**
 * This structure contains the hints information for the end user. 
 * This structure could contain either command or argument information.
 * The front-end will format the information and present it to the user.
 */
typedef struct pj_cli_hint_info
{
    /**
     * The hint value.
     */
    pj_str_t name;

    /**
     * The hint type.
     */
    pj_str_t type;

    /**
     * Helpful description of the hint value. 
     */
    pj_str_t desc;

} pj_cli_hint_info;

/**
 * This structure contains extra information returned by pj_cli_sess_exec()/
 * pj_cli_sess_parse().
 * Upon return from the function, various other fields in this structure will
 * be set by the function.
 */
typedef struct pj_cli_exec_info
{
    /**
     * If command parsing failed, on return this will point to the location
     * where the failure occurs, otherwise the value will be set to -1.
     */
    int err_pos;

    /**
     * If a command matching the command in the cmdline was found, on return
     * this will be set to the command id of the command, otherwise it will be
     * set to PJ_CLI_INVALID_CMD_ID.
     */
    pj_cli_cmd_id cmd_id;

    /**
     * If a command was executed, on return this will be set to the return
     * value of the command, otherwise it will contain PJ_SUCCESS.
     */
    pj_status_t cmd_ret;

    /**
     * The number of hint elements
     **/
    unsigned hint_cnt;

    /**
     * If pj_cli_sess_parse() fails because of a missing argument or ambigous 
     * command/argument, the function returned PJ_CLI_EMISSINGARG or 
     * PJ_CLI_EAMBIGUOUS error. 
     * This field will contain the hint information. This is useful to give 
     * helpful information to the end_user.
     */
    pj_cli_hint_info hint[PJ_CLI_MAX_HINTS];

} pj_cli_exec_info;

/**
 * This structure contains the information returned from the dynamic 
 * argument callback.
 */
typedef struct pj_cli_arg_choice_val
{
    /**
     * The argument choice value
     */
    pj_str_t value;

    /**
     * Helpful description of the choice value. This text will be used when
     * displaying the help texts for the choice value
     */
    pj_str_t desc;

} pj_cli_arg_choice_val;

/**
 * This structure contains the parameters for pj_cli_get_dyn_choice
 */
typedef struct pj_cli_dyn_choice_param
{
    /**
     * The session on which the command was executed on.
     */
    pj_cli_sess *sess;

    /**
     * The command being processed.
     */
    pj_cli_cmd_spec *cmd;

    /**
     * The argument id.
     */
    pj_cli_arg_id arg_id;

    /**
     * The maximum number of values that the choice can hold.
     */
    unsigned max_cnt;

    /**
     * The pool to allocate memory from.
     */
    pj_pool_t *pool;

    /**
     * The choice values count.
     */
    unsigned cnt;

    /**
     * Array containing the valid choice values.
     */
    pj_cli_arg_choice_val choice[PJ_CLI_MAX_CHOICE_VAL];
} pj_cli_dyn_choice_param;

/**
 * This specifies the callback type for argument handlers, which will be
 * called to get the valid values of the choice type arguments.
 */
typedef void (*pj_cli_get_dyn_choice) (pj_cli_dyn_choice_param *param);

/**
 * This specifies the callback type for command handlers, which will be
 * executed when the specified command is invoked.
 *
 * @param cval   The command that is specified by the user.
 *
 * @return          Return the status of the command execution.
 */
typedef pj_status_t (*pj_cli_cmd_handler)(pj_cli_cmd_val *cval);

/**
 * Write a log message to the CLI application. The CLI application
 * will send the log message to all the registered front-ends.
 *
 * @param cli           The CLI application instance.
 * @param level         Verbosity level of this message message.
 * @param buffer        The message itself.
 * @param len           Length of this message.
 */
PJ_DECL(void) pj_cli_write_log(pj_cli_t *cli,
                               int level,
                               const char *buffer,
                               int len);

/**
 * Create a new CLI application instance.
 *
 * @param cfg           CLI application creation parameters.
 * @param p_cli         Pointer to receive the returned instance.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_cli_create(pj_cli_cfg *cfg,
                                   pj_cli_t **p_cli);

/**
 * This specifies the function to get the id of the specified command
 * 
 * @param cmd           The specified command.
 *
 * @return              The command id
 */
PJ_DECL(pj_cli_cmd_id) pj_cli_get_cmd_id(const pj_cli_cmd_spec *cmd);

/**
 * Get the internal parameter of the CLI instance.
 *
 * @param cli           The CLI application instance.
 *
 * @return              CLI parameter instance.
 */
PJ_DECL(pj_cli_cfg*) pj_cli_get_param(pj_cli_t *cli);

/**
 * Call this to signal application shutdown. Typically application would
 * call this from it's "Quit" menu or similar command to quit the
 * application.
 *
 * See also pj_cli_sess_end_session() to end a session instead of quitting the
 * whole application.
 *
 * @param cli           The CLI application instance.
 * @param req           The session on which the shutdown request is
 *                      received.
 * @param restart       Indicate whether application restart is wanted.
 */
PJ_DECL(void) pj_cli_quit(pj_cli_t *cli, pj_cli_sess *req,
                          pj_bool_t restart);
/**
 * Check if application shutdown or restart has been requested.
 *
 * @param cli           The CLI application instance.
 *
 * @return              PJ_TRUE if pj_cli_quit() has been called.
 */
PJ_DECL(pj_bool_t) pj_cli_is_quitting(pj_cli_t *cli);

/**
 * Check if application restart has been requested.
 *
 * @param cli           The CLI application instance.
 *
 * @return              PJ_TRUE if pj_cli_quit() has been called with
 *                      restart parameter set.
 */
PJ_DECL(pj_bool_t) pj_cli_is_restarting(pj_cli_t *cli);

/**
 * Destroy a CLI application instance. This would also close all sessions
 * currently running for this CLI application.
 *
 * @param cli           The CLI application.
 */
PJ_DECL(void) pj_cli_destroy(pj_cli_t *cli);

/**
 * Initialize a pj_cli_cfg with its default values.
 *
 * @param param  The instance to be initialized.
 */
PJ_DECL(void) pj_cli_cfg_default(pj_cli_cfg *param);

/**
 * Register a front end to the CLI application.
 *
 * @param cli           The CLI application.
 * @param fe            The CLI front end to be registered.
 */
PJ_DECL(void) pj_cli_register_front_end(pj_cli_t *cli,
                                        pj_cli_front_end *fe);

/**
 * Create a new complete command specification from an XML node text and
 * register it to the CLI application.
 *
 * Note that the input string MUST be NULL terminated.
 *
 * @param cli           The CLI application.
 * @param group         Optional group to which this command will be added
 *                      to, or specify NULL if this command is a root
 *                      command.
 * @param xml           Input string containing XML node text for the
 *                      command, MUST be NULL terminated.
 * @param handler       Function handler for the command. This must be NULL
 *                      if the command specifies a command group.
 * @param p_cmd         Optional pointer to store the newly created
 *                      specification.
 * @param get_choice    Function handler for the argument. Specify this for 
 *                      dynamic choice type arguments.
 *
 * @return              PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_cli_add_cmd_from_xml(pj_cli_t *cli,
                                             pj_cli_cmd_spec *group,
                                             const pj_str_t *xml,
                                             pj_cli_cmd_handler handler,
                                             pj_cli_cmd_spec **p_cmd, 
                                             pj_cli_get_dyn_choice get_choice);
/**
 * Initialize pj_cli_exec_info with its default values.
 *
 * @param param         The param to be initialized.
 */
PJ_DECL(void) pj_cli_exec_info_default(pj_cli_exec_info *param);

/**
 * Write a log message to the specific CLI session. 
 *
 * @param sess          The CLI active session.
 * @param buffer        The message itself.
 * @param len           Length of this message.
 */
PJ_DECL(void) pj_cli_sess_write_msg(pj_cli_sess *sess,                               
                                    const char *buffer,
                                    pj_size_t len);

/**
 * Parse an input cmdline string. The first word of the command line is the
 * command itself, which will be matched against command  specifications
 * registered in the CLI application.
 *
 * Zero or more arguments follow the command name. Arguments are separated by
 * one or more whitespaces. Argument may be placed inside a pair of quotes,
 * double quotes, '{' and '}', or '[' and ']' pairs. This is useful when the
 * argument itself contains whitespaces or other restricted characters. If
 * the quote character itself is to appear in the argument, the argument then
 * must be quoted with different quote characters. There is no character
 * escaping facility provided by this function (such as the use of backslash
 * '\' character).
 *
 * The cmdline may be followed by an extra newline (LF or CR-LF characters),
 * which will be removed by the function. However any more characters
 * following this newline will cause an error to be returned.
 *
 * @param sess          The CLI session.
 * @param cmdline       The command line string to be parsed.
 * @param val           Structure to store the parsing result.
 * @param pool          The pool to allocate memory from.
 * @param info          Additional info to be returned regarding the parsing.
 *
 * @return              This function returns the status of the parsing,
 *                      which can be one of the following :
 *                        - PJ_SUCCESS: a command was executed successfully.
 *                        - PJ_EINVAL: invalid parameter to this function.
 *                        - PJ_ENOTFOUND: command is not found.
 *                        - PJ_CLI_EAMBIGUOUS: command/argument is ambiguous.
 *                        - PJ_CLI_EMISSINGARG: missing argument.
 *                        - PJ_CLI_EINVARG: invalid command argument.
 *                        - PJ_CLI_EEXIT: "exit" has been called to end
 *                            the current session. This is a signal for the
 *                            application to end it's main loop.
 */
PJ_DECL(pj_status_t) pj_cli_sess_parse(pj_cli_sess *sess,
                                       char *cmdline,
                                       pj_cli_cmd_val *val,
                                       pj_pool_t *pool,
                                       pj_cli_exec_info *info);

/**
 * End the specified session, and destroy it to release all resources used
 * by the session.
 *
 * See also pj_cli_sess and pj_cli_front_end for more info regarding the 
 * creation process.
 * See also pj_cli_quit() to quit the whole application instead.
 *
 * @param sess          The CLI session to be destroyed.
 */
PJ_DECL(void) pj_cli_sess_end_session(pj_cli_sess *sess);

/**
 * Execute a command line. This function will parse the input string to find
 * the appropriate command and verify whether the string matches the command
 * specifications. If matches, the command will be executed, and the return
 * value of the command will be set in the \a cmd_ret field of the \a info
 * argument, if specified.
 *
 * See also pj_cli_sess_parse() for more info regarding the cmdline format.
 *
 * @param sess          The CLI session.
 * @param cmdline       The command line string to be executed. 
 * @param pool          The pool to allocate memory from.
 * @param info          Optional pointer to receive additional information
 *                      related to the execution of the command (such as
 *                      the command return value).
 *
 * @return              This function returns the status of the command
 *                      parsing and execution (note that the return value
 *                      of the handler itself will be returned in \a info
 *                      argument, if specified). Please see the return value
 *                      of pj_cli_sess_parse() for possible return values.
 */
PJ_DECL(pj_status_t) pj_cli_sess_exec(pj_cli_sess *sess,
                                      char *cmdline,
                                      pj_pool_t *pool,
                                      pj_cli_exec_info *info);

/**
 * @}
 */

PJ_END_DECL

#endif /* __PJLIB_UTIL_CLI_H__ */

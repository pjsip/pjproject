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
 *
 */

/**
 * Maximum length of command buffer.
 */
#ifndef PJ_CLI_MAX_CMDBUF
#   define PJ_CLI_MAX_CMDBUF		120
#endif

/**
 * Maximum command arguments.
 */
#ifndef PJ_CLI_MAX_ARGS
#   define PJ_CLI_MAX_ARGS		8
#endif

/**
 * Maximum short name version (shortcuts) for a command.
 */
#ifndef PJ_CLI_MAX_SHORTCUTS
#   define PJ_CLI_MAX_SHORTCUTS		4
#endif

/*
 *  New error constants (note: to be placed in errno.h with new values)
 */
/**
 * @hideinitializer
 * End the current session. This is a special error code returned by
 * pj_cli_exec() to indicate that "exit" or equivalent command has been
 * called to end the current session.
 */
#define PJ_CLI_EEXIT        		-101
/**
 * @hideinitializer
 * A required CLI argument is not specified.
 */
#define PJ_CLI_EMISSINGARG    		-104
 /**
 * @hideinitializer
 * Too many CLI arguments.
 */
#define PJ_CLI_ETOOMANYARGS    		-105
/**
 * @hideinitializer
 * Invalid CLI argument. Typically this is caused by extra characters
 * specified in the command line which does not match any arguments.
 */
#define PJ_CLI_EINVARG        		-106
/**
 * @hideinitializer
 * CLI command with the specified name already exist.
 */
#define PJ_CLI_EBADNAME        		-107

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
 * Reserved command id constants.
 */
typedef enum pj_cli_std_cmd_id
{
    /**
     * Constant to indicate an invalid command id.
     */
    PJ_CLI_INVALID_CMD_ID = -1,

    /**
     * A special command id to indicate that a command id denotes
     * a command group.
     */
    PJ_CLI_CMD_ID_GROUP = -2

} pj_cli_std_cmd_id;


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

    /**
     * Specify whether only exact matching command will be executed. If
     * PJ_FALSE, the framework will accept any unique abbreviations of
     * the command. Please see the description of pj_cli_parse() function
     * for more info.
     *
     * Default: PJ_FALSE
     */
    pj_bool_t exact_cmd;

} pj_cli_cfg;


/**
 * This describes the type of an argument (pj_cli_arg_spec).
 */
typedef enum pj_cli_arg_type
{
    /**
     * Unformatted string.
     */
    PJ_CLI_ARG_TEXT,

    /**
     * An integral number.
     */
    PJ_CLI_ARG_INT

} pj_cli_arg_type;

/**
 * This structure describe the specification of a command argument.
 */
typedef struct pj_cli_arg_spec
{
    /**
     * Argument name.
     */
    pj_str_t name;

    /**
     * Helpful description of the argument. This text will be used when
     * displaying help texts for the command/argument.
     */
    pj_str_t desc;

    /**
     * Argument type, which will be used for rendering the argument and
     * to perform basic validation against an input value.
     */
    pj_cli_arg_type type;

} pj_cli_arg_spec;


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
 * This specifies the callback type for command handlers, which will be
 * executed when the specified command is invoked.
 *
 * @param sess      The CLI session where the command is invoked.
 * @param cmd_val   The command that is specified by the user.
 *
 * @return          Return the status of the command execution.
 */
typedef pj_status_t (*pj_cli_cmd_handler)(pj_cli_cmd_val *cval);

/**
 * This structure describes the full specification of a CLI command. A CLI
 * command mainly consists of the name of the command, zero or more arguments,
 * and a callback function to be called to execute the command.
 *
 * Application can create this specification by forming an XML document and
 * calling pj_cli_create_cmd_from_xml() to instantiate the spec. A sample XML
 * document containing a command spec is as follows:
 *
 \verbatim
  <CMD name='makecall' id='101' sc='m,mc' desc='Make outgoing call'>
      <ARGS>
	  <ARG name='target' type='text' desc='The destination'/>
      </ARGS>
  </CMD>
 \endverbatim

 */
struct pj_cli_cmd_spec
{
    /**
     * To make list of child cmds.
     */
    PJ_DECL_LIST_MEMBER(struct pj_cli_cmd_spec);

    /**
     * Command ID assigned to this command by the application during command
     * creation. If this value is PJ_CLI_CMD_ID_GROUP (-2), then this is
     * a command group and it can't be executed.
     */
    pj_cli_cmd_id id;

    /**
     * The command name.
     */
    pj_str_t name;

    /**
     * The full description of the command.
     */
    pj_str_t desc;

    /**
     * Number of optional shortcuts
     */
    unsigned sc_cnt;

    /**
     * Optional array of shortcuts, if any. Shortcut is a short name version
     * of the command. If the command doesn't have any shortcuts, this
     * will be initialized to NULL.
     */
    pj_str_t *sc;

    /**
     * The command handler, to be executed when a command matching this command
     * specification is invoked by the end user. The value may be NULL if this
     * is a command group.
     */
    pj_cli_cmd_handler handler;

    /**
     * Number of arguments.
     */
    unsigned arg_cnt;

    /**
     * Array of arguments.
     */
    pj_cli_arg_spec *arg;

    /**
     * Child commands, if any. A command will only have subcommands if it is
     * a group. If the command doesn't have subcommands, this field will be
     * initialized with NULL.
     */
    pj_cli_cmd_spec *sub_cmd;
};


/**
 * This contains extra parameters to be specified when calling pj_cli_exec().
 * Upon return from the function, various other fields in this structure will
 * be set by the function.
 *
 * Application must call pj_cli_exec_info_default() to initialize this
 * structure with its default values.
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
     * If pj_cli_exec() fails because an argument is missing (the function
     * returned PJ_CLI_EMISSINGARG error), this field will be set to the
     * index of the missing argument. This is useful to give more helpful
     * error info to the end user, or to facilitate a more interactive
     * input display.
     */
    int arg_idx;

} pj_cli_exec_info;


/**
 * Initialize a pj_cli_cfg with its default values.
 *
 * @param param  The instance to be initialized.
 */
PJ_DECL(void) pj_cli_cfg_default(pj_cli_cfg *param);

/**
 * Initialize pj_cli_exec_info with its default values.
 *
 * @param param		The param to be initialized.
 */
PJ_DECL(void) pj_cli_exec_info_default(pj_cli_exec_info *param);

/**
 * Create a new CLI application instance.
 *
 * @param cfg		CLI application creation parameters.
 * @param p_cli		Pointer to receive the returned instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_cli_create(pj_cli_cfg *cfg,
                                   pj_cli_t **p_cli);

/**
 * Get the internal parameter of the CLI instance.
 *
 * @param cli		The CLI application instance.
 *
 * @return		CLI parameter instance.
 */
PJ_DECL(pj_cli_cfg*) pj_cli_get_param(pj_cli_t *cli);


/**
 * Call this to signal application shutdown. Typically application would
 * call this from it's "Quit" menu or similar command to quit the
 * application.
 *
 * See also pj_cli_end_session() to end a session instead of quitting the
 * whole application.
 *
 * @param cli		The CLI application instance.
 * @param req		The session on which the shutdown request is
 * 			received.
 * @param restart	Indicate whether application restart is wanted.
 */
PJ_DECL(void) pj_cli_quit(pj_cli_t *cli, pj_cli_sess *req,
			  pj_bool_t restart);

/**
 * Check if application shutdown or restart has been requested.
 *
 * @param cli		The CLI application instance.
 *
 * @return		PJ_TRUE if pj_cli_quit() has been called.
 */
PJ_DECL(pj_bool_t) pj_cli_is_quitting(pj_cli_t *cli);


/**
 * Check if application restart has been requested.
 *
 * @param cli		The CLI application instance.
 *
 * @return		PJ_TRUE if pj_cli_quit() has been called with
 * 			restart parameter set.
 */
PJ_DECL(pj_bool_t) pj_cli_is_restarting(pj_cli_t *cli);


/**
 * Destroy a CLI application instance. This would also close all sessions
 * currently running for this CLI application.
 *
 * @param cli		The CLI application.
 */
PJ_DECL(void) pj_cli_destroy(pj_cli_t *cli);


/**
 * End the specified session, and destroy it to release all resources used
 * by the session.
 *
 * See also pj_cli_quit() to quit the whole application instead.
 *
 * @param sess		The CLI session to be destroyed.
 */
PJ_DECL(void) pj_cli_end_session(pj_cli_sess *sess);

/**
 * Register a front end to the CLI application.
 *
 * @param CLI		The CLI application.
 * @param fe		The CLI front end to be registered.
 */
PJ_DECL(void) pj_cli_register_front_end(pj_cli_t *cli,
                                        pj_cli_front_end *fe);

/**
 * Create a new complete command specification from an XML node text and
 * register it to the CLI application.
 *
 * @param cli		The CLI application.
 * @param group		Optional group to which this command will be added
 * 			to, or specify NULL if this command is a root
 * 			command.
 * @param xml		Input string containing XML node text for the
 * 			command.
 * @param handler	Function handler for the command. This must be NULL
 * 			if the command specifies a command group.
 * @param p_cmd		Optional pointer to store the newly created
 * 			specification.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_cli_add_cmd_from_xml(pj_cli_t *cli,
					     pj_cli_cmd_spec *group,
                                             const pj_str_t *xml,
                                             pj_cli_cmd_handler handler,
                                             pj_cli_cmd_spec *p_cmd);

/**
 * Parse an input cmdline string. The first word of the command line is the
 * command itself, which will be matched against command  specifications
 * registered in the CLI application.
 *
 * By default, a command may be matched by any shorter abbreviations of the
 * command that uniquely identify the command. For example, suppose two
 * commands "help" and "hold" are currently the only commands registered in
 * the CLI application. In this case, specifying "he" and "hel" would also
 * match "help" command, and similarly "ho" and "hol" would also match "hold"
 * command, but specifying "h" only would yield an error as it would match
 * more than one commands. This matching behavior can be turned off by
 * setting \a pj_cli_cfg.exact_cmd to PJ_TRUE.
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
 * which simply will be ignored. However any more characters following this
 * newline will cause an error to be returned.
 *
 * @param sess		The CLI session.
 * @param cmdline	The command line string to be parsed.
 * @param val		Structure to store the parsing result.
 * @param info		Additional info to be returned regarding the parsing.
 *
 * @return		This function returns the status of the parsing,
 * 			which can be one of the following :
 *			  - PJ_SUCCESS: a command was executed successfully.
 *			  - PJ_EINVAL: invalid parameter to this function.
 *			  - PJ_ENOTFOUND: command is not found.
 *			  - PJ_CLI_EMISSINGARG: missing argument.
 *			  - PJ_CLI_EINVARG: invalid command argument.
 *			  - PJ_CLI_EEXIT: "exit" has been called to end
 *			      the current session. This is a signal for the
 *			      application to end it's main loop.
 */
PJ_DECL(pj_status_t) pj_cli_parse(pj_cli_sess *sess,
				  char *cmdline,
				  pj_cli_cmd_val *val,
				  pj_cli_exec_info *info);

/**
 * Execute a command line. This function will parse the input string to find
 * the appropriate command and verify whether the string matches the command
 * specifications. If matches, the command will be executed, and the return
 * value of the command will be set in the \a cmd_ret field of the \a eparam
 * argument, if specified.
 *
 * Please also see pj_cli_parse() for more info regarding the cmdline format.
 *
 * @param sess		The CLI session.
 * @param cmdline	The command line string to be executed. See the
 * 			description of pj_cli_parse() API for more info
 * 			regarding the cmdline format.
 * @param info		Optional pointer to receive additional information
 * 			related to the execution of the command (such as
 * 			the command return value).
 *
 * @return		This function returns the status of the command
 * 			parsing and execution (note that the return value
 * 			of the handler itself will be returned in \a info
 * 			argument, if specified). Please see the return value
 * 			of pj_cli_parse() for possible return values.
 */
pj_status_t pj_cli_exec(pj_cli_sess *sess,
                        char *cmdline,
                        pj_cli_exec_info *info);


/**
 * @}
 */

PJ_END_DECL

#endif /* __PJLIB_UTIL_CLI_H__ */

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

#include <pjlib-util/cli_imp.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/except.h>
#include <pj/hash.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pjlib-util/errno.h>
#include <pjlib-util/scanner.h>
#include <pjlib-util/xml.h>

#define CMD_HASH_TABLE_SIZE 63	/* Hash table size */

#define CLI_CMD_CHANGE_LOG  30000
#define CLI_CMD_EXIT        30001

#define MAX_CMD_HASH_NAME_LENGTH PJ_CLI_MAX_CMDBUF
#define MAX_CMD_ID_LENGTH 16

#if 1
    /* Enable some tracing */
    #define THIS_FILE   "cli.c"
    #define TRACE_(arg)	PJ_LOG(3,arg)
#else
    #define TRACE_(arg)
#endif

/**
 * This structure describes the full specification of a CLI command. A CLI
 * command mainly consists of the name of the command, zero or more arguments,
 * and a callback function to be called to execute the command.
 *
 * Application can create this specification by forming an XML document and
 * calling pj_cli_add_cmd_from_xml() to instantiate the spec. A sample XML
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

struct pj_cli_t
{
    pj_pool_t	       *pool;           /* Pool to allocate memory from */
    pj_cli_cfg          cfg;            /* CLI configuration */
    pj_cli_cmd_spec     root;           /* Root of command tree structure */
    pj_cli_front_end    fe_head;        /* List of front-ends */
    pj_hash_table_t    *cmd_name_hash;  /* Command name hash table, this will 
					   include the command name and shortcut 
					   as hash key */
    pj_hash_table_t    *cmd_id_hash;    /* Command id hash table */

    pj_bool_t           is_quitting;
    pj_bool_t           is_restarting;
};

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
    PJ_CLI_ARG_INT,

    /**
     * Choice type
    */
    PJ_CLI_ARG_CHOICE

} pj_cli_arg_type;

struct arg_type
{
    const pj_str_t msg;
} arg_type[3] = 
{
    {{"Text", 4}},
    {{"Int", 3}},
    {{"Choice", 6}}
};

/**
 * This structure describe the specification of a command argument.
 */
struct pj_cli_arg_spec
{
    /**
     * Argument id
     */
    pj_cli_arg_id id;

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

    /**
     * Argument status
     */
    pj_bool_t optional;

    /**
     * Validate choice values
     */
    pj_bool_t validate;

    /**
     * Static Choice Values count
     */
    unsigned stat_choice_cnt; 

    /**
     * Static Choice Values
     */
    pj_cli_arg_choice_val *stat_choice_val; 

    /**
     * Argument callback to get the valid values
     */
    pj_cli_get_dyn_choice get_dyn_choice;

};

/**
 * This describe the parse mode of the command line
 */
typedef enum pj_cli_parse_mode {
    PARSE_NONE,
    PARSE_COMPLETION,	/* Complete the command line */
    PARSE_NEXT_AVAIL,   /* Find the next available command line */
    PARSE_EXEC		/* Exec the command line */
} pj_cli_parse_mode;

/** 
 * This is used to get the matched command/argument from the 
 * command/argument structure.
 * 
 * @param sess		The session on which the command is execute on.
 * @param cmd		The active command.
 * @param cmd_val	The command value to match.
 * @param argc		The number of argument that the 
 *			current active command have.
 * @param pool		The memory pool to allocate memory.
 * @param get_cmd	Set true to search matching command from sub command.
 * @param parse_mode	The parse mode.
 * @param p_cmd		The command that mathes the command value.
 * @param info		The output information containing any hints for 
 *			matching command/arg.
 * @return		This function return the status of the 
 *			matching process.Please see the return value
 * 			of pj_cli_sess_parse() for possible return values.
 */
static pj_status_t get_available_cmds(pj_cli_sess *sess,
				      pj_cli_cmd_spec *cmd, 
				      pj_str_t *cmd_val,
				      unsigned argc,
				      pj_pool_t *pool,
				      pj_bool_t get_cmd,
				      pj_cli_parse_mode parse_mode,
				      pj_cli_cmd_spec **p_cmd,
				      pj_cli_exec_info *info);

PJ_DEF(pj_cli_cmd_id) pj_cli_get_cmd_id(const pj_cli_cmd_spec *cmd)
{
    return cmd->id;
}

PJ_DEF(void) pj_cli_cfg_default(pj_cli_cfg *param)
{
    pj_assert(param);
    pj_bzero(param, sizeof(*param));
    pj_strset2(&param->name, "");
}

PJ_DEF(void) pj_cli_exec_info_default(pj_cli_exec_info *param)
{
    pj_assert(param);
    pj_bzero(param, sizeof(*param));
    param->err_pos = -1;
    param->cmd_id = PJ_CLI_INVALID_CMD_ID;
    param->cmd_ret = PJ_SUCCESS;
}

PJ_DEF(void) pj_cli_write_log(pj_cli_t *cli,
                              int level,
                              const char *buffer,
                              int len)
{
    struct pj_cli_front_end *fe;

    pj_assert(cli);

    fe = cli->fe_head.next;
    while (fe != &cli->fe_head) {
        if (fe->op && fe->op->on_write_log)
            (*fe->op->on_write_log)(fe, level, buffer, len);
        fe = fe->next;
    }
}

PJ_DEF(void) pj_cli_sess_write_msg(pj_cli_sess *sess,                               
				   const char *buffer,
				   pj_size_t len)
{
    struct pj_cli_front_end *fe;

    pj_assert(sess);

    fe = sess->fe;
    if (fe->op && fe->op->on_write_log)
        (*fe->op->on_write_log)(fe, 0, buffer, len);
}

/* Command handler */
static pj_status_t cmd_handler(pj_cli_cmd_val *cval)
{
    unsigned level;

    switch(cval->cmd->id) {
    case CLI_CMD_CHANGE_LOG:
        level = pj_strtoul(&cval->argv[1]);
        if (!level && cval->argv[1].slen > 0 && (cval->argv[1].ptr[0] < '0' ||
            cval->argv[1].ptr[0] > '9'))
	{
            return PJ_CLI_EINVARG;
	}
        cval->sess->log_level = level;
        return PJ_SUCCESS;
    case CLI_CMD_EXIT:
        pj_cli_sess_end_session(cval->sess);
        return PJ_CLI_EEXIT;
    default:
        return PJ_SUCCESS;
    }
}

PJ_DEF(pj_status_t) pj_cli_create(pj_cli_cfg *cfg,
                                  pj_cli_t **p_cli)
{
    pj_pool_t *pool;
    pj_cli_t *cli;    
    unsigned i;

    /* This is an example of the command structure */
    char* cmd_xmls[] = {
     "<CMD name='log' id='30000' sc='' desc='Change log level'>"
     "    <ARG name='level' type='int' optional='0' desc='Log level'/>"
     "</CMD>",     
     "<CMD name='exit' id='30001' sc='' desc='Exit session'>"     
     "</CMD>",
    };

    PJ_ASSERT_RETURN(cfg && cfg->pf && p_cli, PJ_EINVAL);

    pool = pj_pool_create(cfg->pf, "cli", PJ_CLI_POOL_SIZE, 
                          PJ_CLI_POOL_INC, NULL);
    if (!pool)
        return PJ_ENOMEM;
    cli = PJ_POOL_ZALLOC_T(pool, struct pj_cli_t);

    pj_memcpy(&cli->cfg, cfg, sizeof(*cfg));
    cli->pool = pool;
    pj_list_init(&cli->fe_head);

    cli->cmd_name_hash = pj_hash_create(pool, CMD_HASH_TABLE_SIZE);
    cli->cmd_id_hash = pj_hash_create(pool, CMD_HASH_TABLE_SIZE);

    cli->root.sub_cmd = PJ_POOL_ZALLOC_T(pool, pj_cli_cmd_spec);
    pj_list_init(cli->root.sub_cmd);

    /* Register some standard commands. */
    for (i = 0; i < sizeof(cmd_xmls)/sizeof(cmd_xmls[0]); i++) {
        pj_str_t xml = pj_str(cmd_xmls[i]);

        if (pj_cli_add_cmd_from_xml(cli, NULL, &xml, 
				    &cmd_handler, NULL, NULL) != PJ_SUCCESS) 
	{
            TRACE_((THIS_FILE, "Failed to add command #%d", i));
	}
    }

    *p_cli = cli;

    return PJ_SUCCESS;
}

PJ_DEF(pj_cli_cfg*) pj_cli_get_param(pj_cli_t *cli)
{
    PJ_ASSERT_RETURN(cli, NULL);

    return &cli->cfg;
}

PJ_DEF(void) pj_cli_register_front_end(pj_cli_t *cli,
                                       pj_cli_front_end *fe)
{
    pj_list_push_back(&cli->fe_head, fe);
}

PJ_DEF(void) pj_cli_quit(pj_cli_t *cli, pj_cli_sess *req,
			 pj_bool_t restart)
{
    pj_cli_front_end *fe;

    pj_assert(cli);
    if (cli->is_quitting)
	return;

    cli->is_quitting = PJ_TRUE;
    cli->is_restarting = restart;

    fe = cli->fe_head.next;
    while (fe != &cli->fe_head) {
        if (fe->op && fe->op->on_quit)
            (*fe->op->on_quit)(fe, req);
        fe = fe->next;
    }
}

PJ_DEF(pj_bool_t) pj_cli_is_quitting(pj_cli_t *cli)
{
    PJ_ASSERT_RETURN(cli, PJ_FALSE);

    return cli->is_quitting;
}

PJ_DEF(pj_bool_t) pj_cli_is_restarting(pj_cli_t *cli)
{
    PJ_ASSERT_RETURN(cli, PJ_FALSE);

    return cli->is_restarting;
}

PJ_DEF(void) pj_cli_destroy(pj_cli_t *cli)
{
    pj_cli_front_end *fe;

    if (!cli)
	return;

    if (!pj_cli_is_quitting(cli))
        pj_cli_quit(cli, NULL, PJ_FALSE);

    fe = cli->fe_head.next;
    while (fe != &cli->fe_head) {
        pj_list_erase(fe);
	if (fe->op && fe->op->on_destroy)
	    (*fe->op->on_destroy)(fe);

        fe = cli->fe_head.next;
    }
    cli->is_quitting = PJ_FALSE;
    pj_pool_release(cli->pool);
}

PJ_DEF(void) pj_cli_sess_end_session(pj_cli_sess *sess)
{
    pj_assert(sess);

    if (sess->op && sess->op->destroy)
        (*sess->op->destroy)(sess);
}

/* Syntax error handler for parser. */
static void on_syntax_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner);
    PJ_THROW(PJ_EINVAL);
}

/* Get the command from the command hash */
static pj_cli_cmd_spec *get_cmd_name(const pj_cli_t *cli, 
				     const pj_cli_cmd_spec *group, 
				     const pj_str_t *cmd)
{
    pj_str_t cmd_val;
    char cmd_ptr[MAX_CMD_HASH_NAME_LENGTH];

    cmd_val.ptr = cmd_ptr;
    cmd_val.slen = 0;

    if (group) {
	char cmd_str[MAX_CMD_ID_LENGTH];
	pj_ansi_sprintf(cmd_str, "%d", group->id);
	pj_strcat2(&cmd_val, cmd_str);	
    }
    pj_strcat(&cmd_val, cmd);
    return (pj_cli_cmd_spec *)pj_hash_get(cli->cmd_name_hash, cmd_val.ptr, 
					  (unsigned)cmd_val.slen, NULL);
}

/* Add command to the command hash */
static void add_cmd_name(pj_cli_t *cli, pj_cli_cmd_spec *group, 
			 pj_cli_cmd_spec *cmd, pj_str_t *cmd_name)
{
    pj_str_t cmd_val;
    pj_str_t add_cmd;
    char cmd_ptr[MAX_CMD_HASH_NAME_LENGTH];

    cmd_val.ptr = cmd_ptr;
    cmd_val.slen = 0;

    if (group) {
	char cmd_str[MAX_CMD_ID_LENGTH];
	pj_ansi_sprintf(cmd_str, "%d", group->id);
	pj_strcat2(&cmd_val, cmd_str);	
    }
    pj_strcat(&cmd_val, cmd_name);
    pj_strdup(cli->pool, &add_cmd, &cmd_val);
    
    pj_hash_set(cli->pool, cli->cmd_name_hash, cmd_val.ptr, 
		(unsigned)cmd_val.slen, 0, cmd);
}

/**
 * This method is to parse and add the choice type 
 * argument values to command structure.
 **/
static pj_status_t add_choice_node(pj_cli_t *cli,
				   pj_xml_node *xml_node,
				   pj_cli_arg_spec *arg,
				   pj_cli_get_dyn_choice get_choice)
{
    pj_xml_node *choice_node;
    pj_xml_node *sub_node;
    pj_cli_arg_choice_val choice_values[PJ_CLI_MAX_CHOICE_VAL];
    pj_status_t status = PJ_SUCCESS;

    sub_node = xml_node;
    arg->type = PJ_CLI_ARG_CHOICE;
    arg->get_dyn_choice = get_choice;						

    choice_node = sub_node->node_head.next;
    while (choice_node != (pj_xml_node*)&sub_node->node_head) {
	pj_xml_attr *choice_attr;
	unsigned *stat_cnt = &arg->stat_choice_cnt;
	pj_cli_arg_choice_val *choice_val = &choice_values[*stat_cnt];		     
	pj_bzero(choice_val, sizeof(*choice_val));

	choice_attr = choice_node->attr_head.next;
	while (choice_attr != &choice_node->attr_head) {
	    if (!pj_stricmp2(&choice_attr->name, "value")) {
		pj_strassign(&choice_val->value, &choice_attr->value);
	    } else if (!pj_stricmp2(&choice_attr->name, "desc")) {
		pj_strassign(&choice_val->desc, &choice_attr->value);
	    }
	    choice_attr = choice_attr->next;
	}		
	if (++(*stat_cnt) >= PJ_CLI_MAX_CHOICE_VAL)
	    break;
	choice_node = choice_node->next;
    }    
    if (arg->stat_choice_cnt > 0) {
        unsigned i;

	arg->stat_choice_val = (pj_cli_arg_choice_val *)
				pj_pool_zalloc(cli->pool, 
					       arg->stat_choice_cnt *
					       sizeof(pj_cli_arg_choice_val));

        for (i = 0; i < arg->stat_choice_cnt; i++) {
	    pj_strdup(cli->pool, &arg->stat_choice_val[i].value, 
		      &choice_values[i].value);
            pj_strdup(cli->pool, &arg->stat_choice_val[i].desc, 
		      &choice_values[i].desc);            
        }
    }
    return status;
}

/**
 * This method is to parse and add the argument attribute to command structure.
 **/
static pj_status_t add_arg_node(pj_cli_t *cli,
				pj_xml_node *xml_node,
				pj_cli_cmd_spec *cmd,
				pj_cli_arg_spec *arg,
				pj_cli_get_dyn_choice get_choice)
{    
    pj_xml_attr *attr;
    pj_status_t status = PJ_SUCCESS;
    pj_xml_node *sub_node = xml_node;

    if (cmd->arg_cnt >= PJ_CLI_MAX_ARGS)
	return PJ_CLI_ETOOMANYARGS;
    
    pj_bzero(arg, sizeof(*arg));
    attr = sub_node->attr_head.next;
    arg->optional = PJ_FALSE;
    arg->validate = PJ_TRUE;
    while (attr != &sub_node->attr_head) {	
	if (!pj_stricmp2(&attr->name, "name")) {
	    pj_strassign(&arg->name, &attr->value);
	} else if (!pj_stricmp2(&attr->name, "id")) {
	    arg->id = pj_strtol(&attr->value);
	} else if (!pj_stricmp2(&attr->name, "type")) {
	    if (!pj_stricmp2(&attr->value, "text")) {
		arg->type = PJ_CLI_ARG_TEXT;
	    } else if (!pj_stricmp2(&attr->value, "int")) {
		arg->type = PJ_CLI_ARG_INT;
	    } else if (!pj_stricmp2(&attr->value, "choice")) {
		/* Get choice value */
		add_choice_node(cli, xml_node, arg, get_choice);
	    } 
	} else if (!pj_stricmp2(&attr->name, "desc")) {
	    pj_strassign(&arg->desc, &attr->value);
	} else if (!pj_stricmp2(&attr->name, "optional")) {
	    if (!pj_strcmp2(&attr->value, "1")) {
		arg->optional = PJ_TRUE;
	    }
	} else if (!pj_stricmp2(&attr->name, "validate")) {
	    if (!pj_strcmp2(&attr->value, "1")) {
		arg->validate = PJ_TRUE;
	    } else {
		arg->validate = PJ_FALSE;
	    }	
	} 
	attr = attr->next;
    }
    cmd->arg_cnt++;
    return status;
}

/**
 * This method is to parse and add the command attribute to command structure.
 **/
static pj_status_t add_cmd_node(pj_cli_t *cli,				  
				pj_cli_cmd_spec *group,					 
				pj_xml_node *xml_node,
				pj_cli_cmd_handler handler,
				pj_cli_cmd_spec **p_cmd,
				pj_cli_get_dyn_choice get_choice)
{
    pj_xml_node *root = xml_node;
    pj_xml_attr *attr;
    pj_xml_node *sub_node;
    pj_cli_cmd_spec *cmd;
    pj_cli_arg_spec args[PJ_CLI_MAX_ARGS];
    pj_str_t sc[PJ_CLI_MAX_SHORTCUTS];
    pj_status_t status = PJ_SUCCESS;

    if (pj_stricmp2(&root->name, "CMD"))
        return PJ_EINVAL;

    /* Initialize the command spec */
    cmd = PJ_POOL_ZALLOC_T(cli->pool, struct pj_cli_cmd_spec);
    
    /* Get the command attributes */
    attr = root->attr_head.next;
    while (attr != &root->attr_head) {
        if (!pj_stricmp2(&attr->name, "name")) {
            pj_strltrim(&attr->value);
            if (!attr->value.slen || 
		(get_cmd_name(cli, group, &attr->value)))                
            {
                return PJ_CLI_EBADNAME;
            }
            pj_strdup(cli->pool, &cmd->name, &attr->value);
        } else if (!pj_stricmp2(&attr->name, "id")) {	    
	    pj_bool_t is_valid = PJ_FALSE;
            if (attr->value.slen) {		
		pj_cli_cmd_id cmd_id = pj_strtol(&attr->value);
		if (!pj_hash_get(cli->cmd_id_hash, &cmd_id, 
		                 sizeof(pj_cli_cmd_id), NULL))
		    is_valid = PJ_TRUE;
	    } 
	    if (!is_valid)
		return PJ_CLI_EBADID;
            cmd->id = (pj_cli_cmd_id)pj_strtol(&attr->value);
        } else if (!pj_stricmp2(&attr->name, "sc")) {
            pj_scanner scanner;
            pj_str_t str;

            PJ_USE_EXCEPTION;

            pj_scan_init(&scanner, attr->value.ptr, attr->value.slen,
                         PJ_SCAN_AUTOSKIP_WS, &on_syntax_error);

            PJ_TRY {
                while (!pj_scan_is_eof(&scanner)) {
                    pj_scan_get_until_ch(&scanner, ',', &str);
                    pj_strrtrim(&str);
                    if (!pj_scan_is_eof(&scanner))
                        pj_scan_advance_n(&scanner, 1, PJ_TRUE);
                    if (!str.slen)
                        continue;

                    if (cmd->sc_cnt >= PJ_CLI_MAX_SHORTCUTS) {
                        PJ_THROW(PJ_CLI_ETOOMANYARGS);
                    }
                    /* Check whether the shortcuts are already used */
                    if (get_cmd_name(cli, &cli->root, &str)) {
                        PJ_THROW(PJ_CLI_EBADNAME);
                    }

                    pj_strassign(&sc[cmd->sc_cnt++], &str);
                }
            }
            PJ_CATCH_ANY {
                pj_scan_fini(&scanner);
                return (PJ_GET_EXCEPTION());
            }
            PJ_END;
            
        } else if (!pj_stricmp2(&attr->name, "desc")) {
            pj_strdup(cli->pool, &cmd->desc, &attr->value);
        }
        attr = attr->next;
    }

    /* Get the command childs/arguments */
    sub_node = root->node_head.next;
    while (sub_node != (pj_xml_node*)&root->node_head) {
	if (!pj_stricmp2(&sub_node->name, "CMD")) {
	    status = add_cmd_node(cli, cmd, sub_node, handler, NULL, 
				  get_choice);
	    if (status != PJ_SUCCESS)
		return status;
	} else if (!pj_stricmp2(&sub_node->name, "ARG")) {
	    /* Get argument attribute */
	    status = add_arg_node(cli, sub_node, 
			          cmd, &args[cmd->arg_cnt], 
		                  get_choice);

	    if (status != PJ_SUCCESS)
		return status;
        }
        sub_node = sub_node->next;
    }

    if (!cmd->name.slen)
        return PJ_CLI_EBADNAME;

    if (!cmd->id)
        return PJ_CLI_EBADID;

    if (cmd->arg_cnt) {
        unsigned i;

        cmd->arg = (pj_cli_arg_spec *)pj_pool_zalloc(cli->pool, cmd->arg_cnt *
                                                     sizeof(pj_cli_arg_spec));
	
        for (i = 0; i < cmd->arg_cnt; i++) {
            pj_strdup(cli->pool, &cmd->arg[i].name, &args[i].name);
            pj_strdup(cli->pool, &cmd->arg[i].desc, &args[i].desc);
	    cmd->arg[i].id = args[i].id;
            cmd->arg[i].type = args[i].type;
	    cmd->arg[i].optional = args[i].optional;
	    cmd->arg[i].validate = args[i].validate;
	    cmd->arg[i].get_dyn_choice = args[i].get_dyn_choice;
	    cmd->arg[i].stat_choice_cnt = args[i].stat_choice_cnt;
	    cmd->arg[i].stat_choice_val = args[i].stat_choice_val;
        }
    }

    if (cmd->sc_cnt) {
        unsigned i;

        cmd->sc = (pj_str_t *)pj_pool_zalloc(cli->pool, cmd->sc_cnt *
                                             sizeof(pj_str_t));
        for (i = 0; i < cmd->sc_cnt; i++) {
            pj_strdup(cli->pool, &cmd->sc[i], &sc[i]);	
	    /** Add shortcut to root command **/
	    add_cmd_name(cli, &cli->root, cmd, &sc[i]);
        }
    }
    
    add_cmd_name(cli, group, cmd, &cmd->name);    
    pj_hash_set(cli->pool, cli->cmd_id_hash, 
		&cmd->id, sizeof(pj_cli_cmd_id), 0, cmd);

    cmd->handler = handler;

    if (group) {
	if (!group->sub_cmd) {
	    group->sub_cmd = PJ_POOL_ALLOC_T(cli->pool, struct pj_cli_cmd_spec);
	    pj_list_init(group->sub_cmd);
	}
        pj_list_push_back(group->sub_cmd, cmd);
    } else {
        pj_list_push_back(cli->root.sub_cmd, cmd);
    }

    if (p_cmd)
        *p_cmd = cmd;

    return status;
}

PJ_DEF(pj_status_t) pj_cli_add_cmd_from_xml(pj_cli_t *cli,
					    pj_cli_cmd_spec *group,
                                            const pj_str_t *xml,
                                            pj_cli_cmd_handler handler,
                                            pj_cli_cmd_spec **p_cmd, 
					    pj_cli_get_dyn_choice get_choice)
{ 
    pj_pool_t *pool;
    pj_xml_node *root;
    pj_status_t status = PJ_SUCCESS;
    
    PJ_ASSERT_RETURN(cli && xml, PJ_EINVAL);

    /* Parse the xml */
    pool = pj_pool_create(cli->cfg.pf, "xml", 1024, 1024, NULL);
    if (!pool)
        return PJ_ENOMEM;

    root = pj_xml_parse(pool, xml->ptr, xml->slen);
    if (!root) {
	TRACE_((THIS_FILE, "Error: unable to parse XML"));
	pj_pool_release(pool);
	return PJ_CLI_EBADXML;
    }    
    status = add_cmd_node(cli, group, root, handler, p_cmd, get_choice);
    pj_pool_release(pool);
    return status;
}

PJ_DEF(pj_status_t) pj_cli_sess_parse(pj_cli_sess *sess,
				      char *cmdline,
				      pj_cli_cmd_val *val,
				      pj_pool_t *pool,
				      pj_cli_exec_info *info)
{    
    pj_scanner scanner;
    pj_str_t str;
    pj_size_t len;    
    pj_cli_cmd_spec *cmd;
    pj_cli_cmd_spec *next_cmd;
    pj_status_t status = PJ_SUCCESS;
    pj_cli_parse_mode parse_mode = PARSE_NONE;    

    PJ_USE_EXCEPTION;

    PJ_ASSERT_RETURN(sess && cmdline && val, PJ_EINVAL);

    PJ_UNUSED_ARG(pool);

    str.slen = 0;
    pj_cli_exec_info_default(info);

    /* Set the parse mode based on the latest char. */
    len = pj_ansi_strlen(cmdline);
    if (len > 0 && ((cmdline[len - 1] == '\r')||(cmdline[len - 1] == '\n'))) {
        cmdline[--len] = 0;
	parse_mode = PARSE_EXEC;
    } else if (len > 0 && 
	       (cmdline[len - 1] == '\t' || cmdline[len - 1] == '?')) 
    {
	cmdline[--len] = 0;
	if (len == 0) {
	    parse_mode = PARSE_NEXT_AVAIL;
	} else {
	    if (cmdline[len - 1] == ' ') 
		parse_mode = PARSE_NEXT_AVAIL;
	    else 
		parse_mode = PARSE_COMPLETION;
	}
    }
    val->argc = 0;
    info->err_pos = 0;
    cmd = &sess->fe->cli->root;
    if (len > 0) {
	pj_scan_init(&scanner, cmdline, len, PJ_SCAN_AUTOSKIP_WS, 
		     &on_syntax_error);
	PJ_TRY {
	    val->argc = 0;	    
	    while (!pj_scan_is_eof(&scanner)) {
		info->err_pos = (int)(scanner.curptr - scanner.begin);
		if (*scanner.curptr == '\'' || *scanner.curptr == '"' ||
		    *scanner.curptr == '{')
		{
		    pj_scan_get_quotes(&scanner, "'\"{", "'\"}", 3, &str);
		    /* Remove the quotes */
		    str.ptr++;
		    str.slen -= 2;
		} else {
		    pj_scan_get_until_chr(&scanner, " \t\r\n", &str);
		}
		++val->argc;
		if (val->argc == PJ_CLI_MAX_ARGS)
		    PJ_THROW(PJ_CLI_ETOOMANYARGS);		
		
		status = get_available_cmds(sess, cmd, &str, val->argc-1, 
					    pool, PJ_TRUE, parse_mode, 
					    &next_cmd, info);

		if (status != PJ_SUCCESS)
		    PJ_THROW(status);
		
		if (cmd != next_cmd) {
		    /* Found new command, set it as the active command */
		    cmd = next_cmd;
		    val->argc = 1;
		    val->cmd = cmd;
		}
		if (parse_mode == PARSE_EXEC) 
		    pj_strassign(&val->argv[val->argc-1], &info->hint->name);
		else 
		    pj_strassign(&val->argv[val->argc-1], &str);

	    }            
	    if (!pj_scan_is_eof(&scanner)) 
		PJ_THROW(PJ_CLI_EINVARG);

	}
	PJ_CATCH_ANY {
	    pj_scan_fini(&scanner);
	    return PJ_GET_EXCEPTION();
	}
	PJ_END;
    } 
    
    if ((parse_mode == PARSE_NEXT_AVAIL) || (parse_mode == PARSE_EXEC)) {
	/* If exec mode, just get the matching argument */
	status = get_available_cmds(sess, cmd, NULL, val->argc, pool, 
				    (parse_mode==PARSE_NEXT_AVAIL), 
				    parse_mode,
				    NULL, info);
	if ((status != PJ_SUCCESS) && (status != PJ_CLI_EINVARG)) {
	    pj_str_t data = pj_str(cmdline);
	    pj_strrtrim(&data);
	    data.ptr[data.slen] = ' ';
	    data.ptr[data.slen+1] = 0;

	    info->err_pos = (int)pj_ansi_strlen(cmdline);
	}
    } 
   
    val->sess = sess;
    return status;
}

PJ_DECL(pj_status_t) pj_cli_sess_exec(pj_cli_sess *sess,
				      char *cmdline,
				      pj_pool_t *pool,
				      pj_cli_exec_info *info)
{
    pj_cli_cmd_val val;
    pj_status_t status;
    pj_cli_exec_info einfo;
    pj_str_t cmd;

    PJ_ASSERT_RETURN(sess && cmdline, PJ_EINVAL);

    PJ_UNUSED_ARG(pool);

    cmd.ptr = cmdline;
    cmd.slen = pj_ansi_strlen(cmdline);

    if (pj_strtrim(&cmd)->slen == 0)
	return PJ_SUCCESS;

    if (!info)
        info = &einfo;
	
    status = pj_cli_sess_parse(sess, cmdline, &val, pool, info);
    if (status != PJ_SUCCESS)
        return status;

    if ((val.argc > 0) && (val.cmd->handler)) {
        info->cmd_ret = (*val.cmd->handler)(&val);
        if (info->cmd_ret == PJ_CLI_EINVARG ||
            info->cmd_ret == PJ_CLI_EEXIT)
	{
            return info->cmd_ret;
	}
    }

    return PJ_SUCCESS;
}

static pj_bool_t hint_inserted(const pj_str_t *name, 
			       const pj_str_t *desc, 
			       const pj_str_t *type, 
			       pj_cli_exec_info *info)
{
    unsigned i;
    for(i=0; i<info->hint_cnt; ++i) {
	pj_cli_hint_info *hint = &info->hint[i];
	if ((!pj_strncmp(&hint->name, name, hint->name.slen)) &&
	    (!pj_strncmp(&hint->desc, desc, hint->desc.slen)) &&
	    (!pj_strncmp(&hint->type, type, hint->type.slen)))
	{
	    return PJ_TRUE;
	}
    }
    return PJ_FALSE;
}

/** This will insert new hint with the option to check for the same 
    previous entry **/
static pj_status_t insert_new_hint2(pj_pool_t *pool, 
				    pj_bool_t unique_insert,
				    const pj_str_t *name, 
				    const pj_str_t *desc, 
				    const pj_str_t *type, 
				    pj_cli_exec_info *info)
{
    pj_cli_hint_info *hint;
    PJ_ASSERT_RETURN(pool && info, PJ_EINVAL);
    PJ_ASSERT_RETURN((info->hint_cnt < PJ_CLI_MAX_HINTS), PJ_EINVAL);

    if ((unique_insert) && (hint_inserted(name, desc, type, info)))
	return PJ_SUCCESS;

    hint = &info->hint[info->hint_cnt];

    pj_strdup(pool, &hint->name, name);

    if (desc && (desc->slen > 0))  {
	pj_strdup(pool, &hint->desc, desc);
    } else {
	hint->desc.slen = 0;
    }
    
    if (type && (type->slen > 0)) {
	pj_strdup(pool, &hint->type, type);
    } else {
	hint->type.slen = 0;
    }

    ++info->hint_cnt;    
    return PJ_SUCCESS;
}

/** This will insert new hint without checking for the same previous entry **/
static pj_status_t insert_new_hint(pj_pool_t *pool, 
				   const pj_str_t *name, 
				   const pj_str_t *desc, 
				   const pj_str_t *type, 
				   pj_cli_exec_info *info)
{        
    return insert_new_hint2(pool, PJ_FALSE, name, desc, type, info);
}

/** This will get a complete/exact match of a command from the cmd hash **/
static pj_status_t get_comp_match_cmds(const pj_cli_t *cli, 
				       const pj_cli_cmd_spec *group,
				       const pj_str_t *cmd_val,	
				       pj_pool_t *pool, 
				       pj_cli_cmd_spec **p_cmd,
				       pj_cli_exec_info *info)
{
    pj_cli_cmd_spec *cmd;    
    PJ_ASSERT_RETURN(cli && group && cmd_val && pool && info, PJ_EINVAL);   

    cmd = get_cmd_name(cli, group, cmd_val);

    if (cmd) {
	pj_status_t status;    
	status = insert_new_hint(pool, cmd_val, &cmd->desc, NULL, info);

	if (status != PJ_SUCCESS)
	    return status;

	*p_cmd = cmd;
    }

    return PJ_SUCCESS;
}

/** This method will search for any shortcut with pattern match to the input
    command. This method should be called from root command, as shortcut could
    only be executed from root **/
static pj_status_t get_pattern_match_shortcut(const pj_cli_t *cli,
					      const pj_str_t *cmd_val,
					      pj_pool_t *pool, 
					      pj_cli_cmd_spec **p_cmd,
					      pj_cli_exec_info *info)
{
    pj_hash_iterator_t it_buf, *it;
    pj_status_t status;
    PJ_ASSERT_RETURN(cli && pool && cmd_val && info, PJ_EINVAL);
  
    it = pj_hash_first(cli->cmd_name_hash, &it_buf);
    while (it) {
	unsigned i;
	pj_cli_cmd_spec *cmd = (pj_cli_cmd_spec *)
			       pj_hash_this(cli->cmd_name_hash, it);

	PJ_ASSERT_RETURN(cmd, PJ_EINVAL);	    
	
	for (i=0; i < cmd->sc_cnt; ++i) {
	    static const pj_str_t SHORTCUT = {"SC", 2};
	    pj_str_t *sc = &cmd->sc[i];
	    PJ_ASSERT_RETURN(sc, PJ_EINVAL);

	    if (!pj_strncmp(sc, cmd_val, cmd_val->slen)) {
		/** Unique hints needed because cmd hash contain command name
		    and shortcut referencing to the same command **/
		status = insert_new_hint2(pool, PJ_TRUE, sc, &cmd->desc, 
					  &SHORTCUT, info);
		if (status != PJ_SUCCESS)
		    return status;

		if (p_cmd)
		    *p_cmd = cmd;
	    }
	}
	
	it = pj_hash_next(cli->cmd_name_hash, it);
    }

    return PJ_SUCCESS;
}

/** This method will search a pattern match to the input command from the child
    command list of the current/active command. **/
static pj_status_t get_pattern_match_cmds(pj_cli_cmd_spec *cmd, 
					  const pj_str_t *cmd_val,				      
					  pj_pool_t *pool, 
					  pj_cli_cmd_spec **p_cmd, 
					  pj_cli_parse_mode parse_mode,
					  pj_cli_exec_info *info)
{
    pj_status_t status;
    PJ_ASSERT_RETURN(cmd && pool && info && cmd_val, PJ_EINVAL);   

    if (p_cmd)
	*p_cmd = cmd;

    /* Get matching command */
    if (cmd->sub_cmd) {
	pj_cli_cmd_spec *child_cmd = cmd->sub_cmd->next;
	while (child_cmd != cmd->sub_cmd) {
	    pj_bool_t found = PJ_FALSE;
	    if (!pj_strncmp(&child_cmd->name, cmd_val, cmd_val->slen)) {		
		status = insert_new_hint(pool, &child_cmd->name, 
					 &child_cmd->desc, NULL, info);
		if (status != PJ_SUCCESS)
		    return status;

		found = PJ_TRUE;
	    }
	    if (found) {
		if (parse_mode == PARSE_NEXT_AVAIL) {
		    /** Only insert shortcut on next available commands mode **/
		    unsigned i;
		    for (i=0; i < child_cmd->sc_cnt; ++i) {
			static const pj_str_t SHORTCUT = {"SC", 2};
			pj_str_t *sc = &child_cmd->sc[i];
			PJ_ASSERT_RETURN(sc, PJ_EINVAL);

			status = insert_new_hint(pool, sc, 
						 &child_cmd->desc, &SHORTCUT, 
						 info);
			if (status != PJ_SUCCESS)
			    return status;
		    }
		}

		if (p_cmd)
		    *p_cmd = child_cmd;			    
	    }	
	    child_cmd = child_cmd->next;
	}
    }
    return PJ_SUCCESS;
}

/** This will match the arguments passed to the command with the argument list
    of the specified command list. **/
static pj_status_t get_match_args(pj_cli_sess *sess,
				  pj_cli_cmd_spec *cmd, 
				  const pj_str_t *cmd_val,
				  unsigned argc,
				  pj_pool_t *pool, 
				  pj_cli_parse_mode parse_mode,
				  pj_cli_exec_info *info)
{
    pj_cli_arg_spec *arg;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(cmd && pool && cmd_val && info, PJ_EINVAL);

    if ((argc > cmd->arg_cnt) && (!cmd->sub_cmd)) {
	if (cmd_val->slen > 0)
	    return PJ_CLI_ETOOMANYARGS;
	else
	    return PJ_SUCCESS;
    }

    if (cmd->arg_cnt > 0) {
	arg = &cmd->arg[argc-1];
	PJ_ASSERT_RETURN(arg, PJ_EINVAL);
	if (arg->type == PJ_CLI_ARG_CHOICE) {	    
	    unsigned j;	    	    

	    if ((parse_mode == PARSE_EXEC) && (!arg->validate)) {
		/* If no validation needed, then insert the values */
		status = insert_new_hint(pool, cmd_val, NULL, NULL, info);		
		return status;
	    }

	    for (j=0; j < arg->stat_choice_cnt; ++j) {
		pj_cli_arg_choice_val *choice_val = &arg->stat_choice_val[j];		
    	    
		PJ_ASSERT_RETURN(choice_val, PJ_EINVAL);		
    	    
		if (!pj_strncmp(&choice_val->value, cmd_val, cmd_val->slen)) {		    
		    status = insert_new_hint(pool, 
					     &choice_val->value, 
					     &choice_val->desc, 
					     &arg_type[PJ_CLI_ARG_CHOICE].msg, 
					     info);
		    if (status != PJ_SUCCESS)
			return status;		    
		}
	    }
	    if (arg->get_dyn_choice) {
		pj_cli_dyn_choice_param dyn_choice_param;
		static pj_str_t choice_str = {"choice", 6};

		/* Get the dynamic choice values */	    
		dyn_choice_param.sess = sess;
		dyn_choice_param.cmd = cmd;
		dyn_choice_param.arg_id = arg->id;
		dyn_choice_param.max_cnt = PJ_CLI_MAX_CHOICE_VAL;
		dyn_choice_param.pool = pool;
		dyn_choice_param.cnt = 0;	    

		(*arg->get_dyn_choice)(&dyn_choice_param);
		for (j=0; j < dyn_choice_param.cnt; ++j) {
		    pj_cli_arg_choice_val *choice = &dyn_choice_param.choice[j];
		    if (!pj_strncmp(&choice->value, cmd_val, cmd_val->slen)) {
			pj_strassign(&info->hint[info->hint_cnt].name, 
				     &choice->value);
			pj_strassign(&info->hint[info->hint_cnt].type, 
				     &choice_str);
			pj_strassign(&info->hint[info->hint_cnt].desc, 
				     &choice->desc);
			if ((++info->hint_cnt) >= PJ_CLI_MAX_HINTS)
			    break;
		    }
		}
		if ((info->hint_cnt == 0) && (!arg->optional))
		    return PJ_CLI_EMISSINGARG;
	    }
	} else {
	    if (cmd_val->slen == 0) {
		if (info->hint_cnt == 0) {
		    if (!((parse_mode == PARSE_EXEC) && (arg->optional))) {
			/* For exec mode,no need to insert hint if optional */
			status = insert_new_hint(pool, 
						 &arg->name, 
				  	         &arg->desc, 
						 &arg_type[arg->type].msg,
						 info);
			if (status != PJ_SUCCESS)
			    return status;
		    }
		    if (!arg->optional)
			return PJ_CLI_EMISSINGARG;
		} 
	    } else {
		return insert_new_hint(pool, cmd_val, NULL, NULL, info);
	    }
	}
    } 
    return status;
}

/** This will check for a match of the commands/arguments input. Any match 
    will be inserted to the hint data. **/
static pj_status_t get_available_cmds(pj_cli_sess *sess,
				      pj_cli_cmd_spec *cmd, 
				      pj_str_t *cmd_val,
				      unsigned argc,
				      pj_pool_t *pool,
				      pj_bool_t get_cmd,
				      pj_cli_parse_mode parse_mode,
				      pj_cli_cmd_spec **p_cmd,
				      pj_cli_exec_info *info)
{
    pj_status_t status = PJ_SUCCESS;
    pj_str_t *prefix;
    pj_str_t EMPTY_STR = {NULL, 0};

    prefix = cmd_val?(pj_strtrim(cmd_val)):(&EMPTY_STR);

    info->hint_cnt = 0;    

    if (get_cmd) {
	status = get_comp_match_cmds(sess->fe->cli, cmd, prefix, pool, p_cmd, 
				     info);
	if (status != PJ_SUCCESS)
	    return status;
	
	/** If exact match found, then no need to search for pattern match **/
	if (info->hint_cnt == 0) {
	    if ((parse_mode != PARSE_NEXT_AVAIL) && 
		(cmd == &sess->fe->cli->root)) 
	    {
		/** Pattern match for shortcut needed on root command only **/
		status = get_pattern_match_shortcut(sess->fe->cli, prefix, pool,
						    p_cmd, info);

		if (status != PJ_SUCCESS)
		    return status;
	    }

	    status = get_pattern_match_cmds(cmd, prefix, pool, p_cmd, 
					    parse_mode, info);
	}

	if (status != PJ_SUCCESS)
	    return status;
    }

    if (argc > 0)
	status = get_match_args(sess, cmd, prefix, argc, 
			        pool, parse_mode, info);

    if (status == PJ_SUCCESS) {	
	if (prefix->slen > 0) {
	    /** If a command entered is not a an empty command, and have a 
		single match in the command list then it is a valid command **/
	    if (info->hint_cnt == 0) {
		status = PJ_CLI_EINVARG;
	    } else if (info->hint_cnt > 1) {
		status = PJ_CLI_EAMBIGUOUS;
	    }
	} else {
	    if (info->hint_cnt > 0)
		status = PJ_CLI_EAMBIGUOUS;
	}
    } 

    return status;
}


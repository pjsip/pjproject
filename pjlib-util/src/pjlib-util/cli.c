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

#if 1
    /* Enable some tracing */
    #define THIS_FILE   "cli.c"
    #define TRACE_(arg)	PJ_LOG(3,arg)
#else
    #define TRACE_(arg)
#endif

#define CMD_HASH_TABLE_SIZE 63		/* Hash table size */

struct pj_cli_t
{
    pj_pool_t	       *pool;          /* Pool to allocate memory from */
    pj_cli_cfg          cfg;            /* CLI configuration */
    pj_cli_cmd_spec     root;           /* Root of command tree structure */
    pj_cli_front_end    fe_head;        /* List of front-ends */
    pj_hash_table_t    *hash;          /* Command hash table */

    pj_bool_t           is_quitting;
    pj_bool_t           is_restarting;
};

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

PJ_DEF(pj_status_t) pj_cli_create(pj_cli_cfg *cfg,
                                  pj_cli_t **p_cli)
{
    pj_pool_t *pool;
    pj_cli_t *cli;

    PJ_ASSERT_RETURN(cfg && cfg->pf && p_cli, PJ_EINVAL);

    pool = pj_pool_create(cfg->pf, "cli", 1024, 1024, NULL);
    cli = PJ_POOL_ZALLOC_T(pool, struct pj_cli_t);
    if (!cli)
        return PJ_ENOMEM;

    pj_memcpy(&cli->cfg, cfg, sizeof(*cfg));
    cli->pool = pool;
    pj_list_init(&cli->fe_head);

    cli->hash = pj_hash_create(pool, CMD_HASH_TABLE_SIZE);

    cli->root.sub_cmd = PJ_POOL_ZALLOC_T(pool, pj_cli_cmd_spec);
    pj_list_init(cli->root.sub_cmd);

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
    pj_assert(!cli->is_quitting);

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

    pj_assert(cli);

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

PJ_DEF(void) pj_cli_end_session(pj_cli_sess *sess)
{
    pj_assert(sess);

    if (sess->op->destroy)
        (*sess->op->destroy)(sess);
}

/* Syntax error handler for parser. */
static void on_syntax_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner);
    PJ_THROW(PJ_EINVAL);
}

PJ_DEF(pj_status_t) pj_cli_add_cmd_from_xml(pj_cli_t *cli,
					    pj_cli_cmd_spec *group,
                                            const pj_str_t *xml,
                                            pj_cli_cmd_handler handler,
                                            pj_cli_cmd_spec *p_cmd)
{
    pj_pool_t *pool;
    pj_xml_node *root;
    pj_xml_attr *attr;
    pj_xml_node *sub_node;
    pj_cli_cmd_spec *cmd;
    pj_cli_arg_spec args[PJ_CLI_MAX_ARGS];
    pj_str_t sc[PJ_CLI_MAX_SHORTCUTS];
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(cli && xml, PJ_EINVAL);

    /* Parse the xml */
    pool = pj_pool_create(cli->cfg.pf, "xml", 1024, 1024, NULL);
    root = pj_xml_parse(pool, xml->ptr, xml->slen);
    if (!root) {
	TRACE_((THIS_FILE, "Error: unable to parse XML"));
	status = PJ_EINVAL;
        goto on_exit;
    }

    if (pj_stricmp2(&root->name, "CMD")) {
	status = PJ_EINVAL;
        goto on_exit;
    }

    /* Initialize the command spec */
    cmd = PJ_POOL_ZALLOC_T(cli->pool, struct pj_cli_cmd_spec);
    if (!cmd) {
        status = PJ_ENOMEM;
        goto on_exit;
    }
    
    /* Get the command attributes */
    attr = root->attr_head.next;
    while (attr != &root->attr_head) {
        if (!pj_stricmp2(&attr->name, "name")) {
            pj_strltrim(&attr->value);
            pj_strrtrim(&attr->value);
            /* Check whether command with the specified name already exists */
            if (!attr->value.slen ||
                pj_hash_get(cli->hash, attr->value.ptr, 
                            attr->value.slen, NULL))
            {
                status = PJ_CLI_EBADNAME;
                goto on_exit;
            }

            pj_strdup(cli->pool, &cmd->name, &attr->value);
        } else if (!pj_stricmp2(&attr->name, "id")) {
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
                    if (pj_hash_get(cli->hash, str.ptr, 
                                    str.slen, NULL))
                    {
                        PJ_THROW(PJ_CLI_EBADNAME);
                    }

                    pj_strassign(&sc[cmd->sc_cnt++], &str);
                }
            }
            PJ_CATCH_ANY {
                pj_scan_fini(&scanner);
                status = PJ_GET_EXCEPTION();
                goto on_exit;
            }
            PJ_END;
            
        } else if (!pj_stricmp2(&attr->name, "desc")) {
            pj_strdup(cli->pool, &cmd->desc, &attr->value);
        }

        attr = attr->next;
    }

    /* Get the command arguments */
    sub_node = root->node_head.next;
    while (sub_node != (pj_xml_node*)&root->node_head) {
        if (!pj_stricmp2(&sub_node->name, "ARGS")) {
            pj_xml_node *arg_node;

            arg_node = sub_node->node_head.next;
            while (arg_node != (pj_xml_node*)&sub_node->node_head) {
                if (!pj_stricmp2(&arg_node->name, "ARG")) {
                    pj_cli_arg_spec *arg;

                    if (cmd->arg_cnt >= PJ_CLI_MAX_ARGS) {
                        status = PJ_CLI_ETOOMANYARGS;
                        goto on_exit;
                    }
                    arg = &args[cmd->arg_cnt];
                    pj_bzero(arg, sizeof(*arg));
                    attr = arg_node->attr_head.next;
                    while (attr != &arg_node->attr_head) {
                        if (!pj_stricmp2(&attr->name, "name")) {
                            pj_strassign(&arg->name, &attr->value);
                        } else if (!pj_stricmp2(&attr->name, "type")) {
                            if (!pj_stricmp2(&attr->value, "text")) {
                                arg->type = PJ_CLI_ARG_TEXT;
                            } else if (!pj_stricmp2(&attr->value, "int")) {
                                arg->type = PJ_CLI_ARG_INT;
                            }
                        } else if (!pj_stricmp2(&attr->name, "desc")) {
                            pj_strassign(&arg->desc, &attr->value);
                        }

                        attr = attr->next;
                    }
                    cmd->arg_cnt++;
                }

                arg_node = arg_node->next;
            }
        }
        sub_node = sub_node->next;
    }

    if (cmd->id == PJ_CLI_CMD_ID_GROUP) {
        /* Command group shouldn't have any shortcuts nor arguments */
        if (!cmd->sc_cnt || !cmd->arg_cnt) {
            status = PJ_EINVAL;
            goto on_exit;
        }
        cmd->sub_cmd = PJ_POOL_ALLOC_T(cli->pool, struct pj_cli_cmd_spec);
        pj_list_init(cmd->sub_cmd);
    }

    if (!cmd->name.slen) {
        status = PJ_CLI_EBADNAME;
        goto on_exit;
    }

    if (cmd->arg_cnt) {
        unsigned i;

        cmd->arg = (pj_cli_arg_spec *)pj_pool_zalloc(cli->pool, cmd->arg_cnt *
                                                     sizeof(pj_cli_arg_spec));
        if (!cmd->arg) {
            status = PJ_ENOMEM;
            goto on_exit;
        }
        for (i = 0; i < cmd->arg_cnt; i++) {
            pj_strdup(cli->pool, &cmd->arg[i].name, &args[i].name);
            pj_strdup(cli->pool, &cmd->arg[i].desc, &args[i].desc);
            cmd->arg[i].type = args[i].type;
        }
    }

    if (cmd->sc_cnt) {
        unsigned i;

        cmd->sc = (pj_str_t *)pj_pool_zalloc(cli->pool, cmd->sc_cnt *
                                             sizeof(pj_str_t));
        if (!cmd->sc) {
            status = PJ_ENOMEM;
            goto on_exit;
        }
        for (i = 0; i < cmd->sc_cnt; i++) {
            pj_strdup(cli->pool, &cmd->sc[i], &sc[i]);
            pj_hash_set(cli->pool, cli->hash, cmd->sc[i].ptr, 
                        cmd->sc[i].slen, 0, cmd);
        }
    }

    pj_hash_set(cli->pool, cli->hash, cmd->name.ptr, cmd->name.slen, 0, cmd);

    cmd->handler = handler;

    if (group)
        pj_list_push_back(group->sub_cmd, cmd);
    else
        pj_list_push_back(cli->root.sub_cmd, cmd);

    if (p_cmd)
        p_cmd = cmd;

on_exit:
    pj_pool_release(pool);

    return status;
}

PJ_DEF(pj_status_t) pj_cli_parse(pj_cli_sess *sess,
				 char *cmdline,
				 pj_cli_cmd_val *val,
				 pj_cli_exec_info *info)
{
    pj_scanner scanner;
    pj_str_t str;
    int len;
    pj_cli_exec_info einfo;

    PJ_USE_EXCEPTION;

    PJ_ASSERT_RETURN(sess && cmdline && val, PJ_EINVAL);

    if (!info)
        info = &einfo;
    pj_cli_exec_info_default(info);

    /* Parse the command line. */
    len = pj_ansi_strlen(cmdline);
    pj_scan_init(&scanner, cmdline, len, PJ_SCAN_AUTOSKIP_WS, 
                 &on_syntax_error);
    PJ_TRY {
        pj_scan_get_until_chr(&scanner, " \t\r\n", &str);

        /* Find the command from the hash table */
        val->cmd = pj_hash_get(sess->fe->cli->hash, str.ptr, 
                               str.slen, NULL);

        /* Error, command not found */
        if (!val->cmd)
            PJ_THROW(PJ_ENOTFOUND);

        info->cmd_id = val->cmd->id;

        /* Parse the command arguments */
        val->argc = 1;
        pj_strassign(&val->argv[0], &str);
        while (!pj_scan_is_eof(&scanner)) {
            if (*scanner.curptr == '\'' || *scanner.curptr == '"' ||
                *scanner.curptr == '[' || *scanner.curptr == '{')
            {
                pj_scan_get_quotes(&scanner, "'\"[{", "'\"]}", 4, &str);
                /* Remove the quotes */
                str.ptr++;
                str.slen -= 2;
            } else {
                pj_scan_get_until_chr(&scanner, " \t\r\n", &str);
            }
            if (val->argc == PJ_CLI_MAX_ARGS)
                PJ_THROW(PJ_CLI_ETOOMANYARGS);
            pj_strassign(&val->argv[val->argc++], &str);
        }
        
        if (!pj_scan_is_eof(&scanner)) {
            pj_scan_get_newline(&scanner);
            if (!pj_scan_is_eof(&scanner))
                PJ_THROW(PJ_CLI_EINVARG);
        }
    }
    PJ_CATCH_ANY {
        pj_scan_fini(&scanner);
        return PJ_GET_EXCEPTION();
    }
    PJ_END;

    if ((val->argc - 1) < (int)val->cmd->arg_cnt) {
        info->arg_idx = val->argc;
        return PJ_CLI_EMISSINGARG;
    } else if ((val->argc - 1) > (int)val->cmd->arg_cnt) {
        info->arg_idx = val->cmd->arg_cnt + 1;
        return PJ_CLI_ETOOMANYARGS;
    }

    val->sess = sess;

    return PJ_SUCCESS;
}

pj_status_t pj_cli_exec(pj_cli_sess *sess,
                        char *cmdline,
                        pj_cli_exec_info *info)
{
    pj_cli_cmd_val val;
    pj_status_t status;
    pj_cli_exec_info einfo;

    PJ_ASSERT_RETURN(sess && cmdline, PJ_EINVAL);

    if (!info)
        info = &einfo;
    status = pj_cli_parse(sess, cmdline, &val, info);

    if (status != PJ_SUCCESS)
        return status;

    if (val.cmd->handler) {
        info->cmd_ret = (*val.cmd->handler)(&val);
        if (info->cmd_ret == PJ_CLI_EINVARG ||
            info->cmd_ret == PJ_CLI_EEXIT)
            return info->cmd_ret;
    }

    return PJ_SUCCESS;
}

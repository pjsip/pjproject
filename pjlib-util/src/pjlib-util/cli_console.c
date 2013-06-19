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
#include <pjlib-util/cli_console.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pjlib-util/errno.h>

/**
 * This specify the state of output character parsing.
 */
typedef enum out_parse_state
{
    OP_NORMAL,
    OP_TYPE,
    OP_SHORTCUT,
    OP_CHOICE
} out_parse_state;

struct cli_console_fe
{
    pj_cli_front_end    base;
    pj_pool_t          *pool;
    pj_cli_sess        *sess;
    pj_thread_t        *input_thread;
    pj_bool_t           thread_quit;
    pj_sem_t           *thread_sem;   
    pj_cli_console_cfg  cfg;    

    struct async_input_t
    {        
        char       *buf;
        unsigned    maxlen;	
        pj_sem_t   *sem;
    } input;
};

static void console_write_log(pj_cli_front_end *fe, int level,
		              const char *data, pj_size_t len)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    if (cfe->sess->log_level > level)
        printf("%.*s", (int)len, data);
}

static void console_quit(pj_cli_front_end *fe, pj_cli_sess *req)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    PJ_UNUSED_ARG(req);

    pj_assert(cfe);
    if (cfe->input_thread) {
        cfe->thread_quit = PJ_TRUE;
        pj_sem_post(cfe->input.sem);
        pj_sem_post(cfe->thread_sem);
    }
}

static void console_destroy(pj_cli_front_end *fe)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    pj_assert(cfe);
    console_quit(fe, NULL);

    if (cfe->input_thread)
        pj_thread_join(cfe->input_thread);

    if (cfe->input_thread) {
        pj_thread_destroy(cfe->input_thread);
	cfe->input_thread = NULL;
    }

    pj_sem_destroy(cfe->thread_sem);
    pj_sem_destroy(cfe->input.sem);
    pj_pool_release(cfe->pool);
}

PJ_DEF(void) pj_cli_console_cfg_default(pj_cli_console_cfg *param)
{
    pj_assert(param);

    param->log_level = PJ_CLI_CONSOLE_LOG_LEVEL;
    param->prompt_str.slen = 0;
    param->quit_command.slen = 0;
}

PJ_DEF(pj_status_t) pj_cli_console_create(pj_cli_t *cli,
					  const pj_cli_console_cfg *param,
					  pj_cli_sess **p_sess,
					  pj_cli_front_end **p_fe)
{
    pj_cli_sess *sess;
    struct cli_console_fe *fe;
    pj_cli_console_cfg cfg;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_ASSERT_RETURN(cli && p_sess, PJ_EINVAL);

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "console_fe",
                          PJ_CLI_CONSOLE_POOL_SIZE, PJ_CLI_CONSOLE_POOL_INC,
                          NULL);
    if (!pool)
        return PJ_ENOMEM;
	
    sess = PJ_POOL_ZALLOC_T(pool, pj_cli_sess);
    fe = PJ_POOL_ZALLOC_T(pool, struct cli_console_fe);

    if (!param) {
        pj_cli_console_cfg_default(&cfg);
        param = &cfg;
    }
    sess->fe = &fe->base;
    sess->log_level = param->log_level;    
    sess->op = PJ_POOL_ZALLOC_T(pool, struct pj_cli_sess_op);
    fe->base.op = PJ_POOL_ZALLOC_T(pool, struct pj_cli_front_end_op);
    fe->base.cli = cli;
    fe->base.type = PJ_CLI_CONSOLE_FRONT_END;
    fe->base.op->on_write_log = &console_write_log;
    fe->base.op->on_quit = &console_quit;
    fe->base.op->on_destroy = &console_destroy;
    fe->pool = pool;
    fe->sess = sess;
    status = pj_sem_create(pool, "console_fe", 0, 1, &fe->thread_sem);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_sem_create(pool, "console_fe", 0, 1, &fe->input.sem);
    if (status != PJ_SUCCESS)
	return status;

    pj_cli_register_front_end(cli, &fe->base);
    if (param->prompt_str.slen == 0) {	
	pj_str_t prompt_sign = pj_str(">>> ");
	fe->cfg.prompt_str.ptr = pj_pool_alloc(fe->pool, prompt_sign.slen+1);
	pj_strcpy(&fe->cfg.prompt_str, &prompt_sign);	
    } else {
	fe->cfg.prompt_str.ptr = pj_pool_alloc(fe->pool, 
					       param->prompt_str.slen+1);
	pj_strcpy(&fe->cfg.prompt_str, &param->prompt_str);
    }    
    fe->cfg.prompt_str.ptr[fe->cfg.prompt_str.slen] = 0; 

    if (param->quit_command.slen)
	pj_strdup(fe->pool, &fe->cfg.quit_command, &param->quit_command);

    *p_sess = sess;
    if (p_fe)
        *p_fe = &fe->base;

    return PJ_SUCCESS;
}

static void send_prompt_str(pj_cli_sess *sess)
{
    pj_str_t send_data;
    char data_str[128];
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    send_data.ptr = data_str;
    send_data.slen = 0;
    
    pj_strcat(&send_data, &fe->cfg.prompt_str);
    send_data.ptr[send_data.slen] = 0;

    printf("%s", send_data.ptr);
}

static void send_err_arg(pj_cli_sess *sess, 
			 const pj_cli_exec_info *info, 
			 const pj_str_t *msg,
			 pj_bool_t with_return)
{
    pj_str_t send_data;
    char data_str[256];
    pj_size_t len;
    unsigned i;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    send_data.ptr = data_str;
    send_data.slen = 0;

    if (with_return)
	pj_strcat2(&send_data, "\r\n");

    len = fe->cfg.prompt_str.slen + info->err_pos;

    for (i=0;i<len;++i) {
	pj_strcat2(&send_data, " ");
    }
    pj_strcat2(&send_data, "^");
    pj_strcat2(&send_data, "\r\n");
    pj_strcat(&send_data, msg);
    pj_strcat(&send_data, &fe->cfg.prompt_str);

    send_data.ptr[send_data.slen] = 0;
    printf("%s", send_data.ptr);    
}

static void send_inv_arg(pj_cli_sess *sess, 
			 const pj_cli_exec_info *info,
			 pj_bool_t with_return)
{
    static const pj_str_t ERR_MSG = {"%Error : Invalid Arguments\r\n", 28};
    send_err_arg(sess, info, &ERR_MSG, with_return);
}

static void send_too_many_arg(pj_cli_sess *sess, 
			      const pj_cli_exec_info *info,
			      pj_bool_t with_return)
{
    static const pj_str_t ERR_MSG = {"%Error : Too Many Arguments\r\n", 29};
    send_err_arg(sess, info, &ERR_MSG, with_return);
}

static void send_hint_arg(pj_str_t *send_data, 
			  const pj_str_t *desc,
			  pj_ssize_t cmd_len,
			  pj_ssize_t max_len)
{
    if ((desc) && (desc->slen > 0)) {
	int j;

	for (j=0;j<(max_len-cmd_len);++j) {
	    pj_strcat2(send_data, " ");
	}
	pj_strcat2(send_data, "  ");
	pj_strcat(send_data, desc);
	send_data->ptr[send_data->slen] = 0;
	printf("%s", send_data->ptr);
	send_data->slen = 0;
    }
}

static void send_ambi_arg(pj_cli_sess *sess, 
			  const pj_cli_exec_info *info,
			  pj_bool_t with_return)
{
    unsigned i;
    pj_size_t len;
    pj_str_t send_data;
    char data[1028];
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    const pj_cli_hint_info *hint = info->hint;
    out_parse_state parse_state = OP_NORMAL;
    pj_ssize_t max_length = 0;
    pj_ssize_t cmd_length = 0;
    const pj_str_t *cmd_desc = 0;
    static const pj_str_t sc_type = {"sc", 2};
    static const pj_str_t choice_type = {"choice", 6};
    send_data.ptr = data;
    send_data.slen = 0;
    
    if (with_return)
	pj_strcat2(&send_data, "\r\n");

    len = fe->cfg.prompt_str.slen + info->err_pos;

    for (i=0;i<len;++i) {
	pj_strcat2(&send_data, " ");
    }
    pj_strcat2(&send_data, "^");        
    /* Get the max length of the command name */
    for (i=0;i<info->hint_cnt;++i) {
	if ((&hint[i].type) && (hint[i].type.slen > 0)) {	    
	    if (pj_stricmp(&hint[i].type, &sc_type) == 0) {		
		if ((i > 0) && (!pj_stricmp(&hint[i-1].desc, &hint[i].desc))) {
		    cmd_length += (hint[i].name.slen + 3);
		} else {
		    cmd_length = hint[i].name.slen;
		}		
	    } else {
		cmd_length = hint[i].name.slen;
	    }
	} else {	    	    
	    cmd_length = hint[i].name.slen;
	}

	if (cmd_length > max_length) {
	    max_length = cmd_length;
	}
    }

    cmd_length = 0;
    for (i=0;i<info->hint_cnt;++i) {
	if ((&hint[i].type) && (hint[i].type.slen > 0)) {	    
	    if (pj_stricmp(&hint[i].type, &sc_type) == 0) {
		parse_state = OP_SHORTCUT;
	    } else if (pj_stricmp(&hint[i].type, &choice_type) == 0) {
		parse_state = OP_CHOICE;
	    } else {
		parse_state = OP_TYPE;
	    }
	} else {	    	    
	    parse_state = OP_NORMAL;
	}

	if (parse_state != OP_SHORTCUT) {
	    pj_strcat2(&send_data, "\r\n  ");
	    cmd_length = hint[i].name.slen;
	}
    
	switch (parse_state) {
	case OP_CHOICE:
	    pj_strcat2(&send_data, "[");
	    pj_strcat(&send_data, &hint[i].name);
	    pj_strcat2(&send_data, "]");	
	    break;
	case OP_TYPE:
	    pj_strcat2(&send_data, "<");
	    pj_strcat(&send_data, &hint[i].type);
	    pj_strcat2(&send_data, ">");	
	    break;
	case OP_SHORTCUT:
	    /* Format : "Command | sc |  description" */
	    {		
		cmd_length += hint[i].name.slen;
		if ((i > 0) && (!pj_stricmp(&hint[i-1].desc, &hint[i].desc))) {
		    pj_strcat2(&send_data, " | ");
		    cmd_length += 3;		    
		} else {
		    pj_strcat2(&send_data, "\r\n  ");
		}
		pj_strcat(&send_data, &hint[i].name);	
	    }
	    break;
	default:
	    pj_strcat(&send_data, &hint[i].name);
	    cmd_desc = &hint[i].desc;
	    break;
	}
    	
	if ((parse_state == OP_TYPE) || (parse_state == OP_CHOICE) || 
	    ((i+1) >= info->hint_cnt) ||
	    (pj_strncmp(&hint[i].desc, &hint[i+1].desc, hint[i].desc.slen))) 
	{
	    /* Add description info */
	    send_hint_arg(&send_data, &hint[i].desc, cmd_length, max_length);

	    cmd_length = 0;
	}
    }  
    pj_strcat2(&send_data, "\r\n");
    pj_strcat(&send_data, &fe->cfg.prompt_str);
    send_data.ptr[send_data.slen] = 0;
    printf("%s", send_data.ptr);    
}

static pj_bool_t handle_hint(pj_cli_sess *sess)
{
    pj_status_t status;
    pj_bool_t retval = PJ_TRUE;
    
    pj_pool_t *pool;
    pj_cli_cmd_val *cmd_val;
    pj_cli_exec_info info;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    char *recv_buf = fe->input.buf;
    pj_cli_t *cli = sess->fe->cli;

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "handle_hint",
                          PJ_CLI_CONSOLE_POOL_SIZE, PJ_CLI_CONSOLE_POOL_INC,
                          NULL);

    cmd_val = PJ_POOL_ZALLOC_T(pool, pj_cli_cmd_val);
    
    status = pj_cli_sess_parse(sess, recv_buf, cmd_val, 
			       pool, &info);

    switch (status) {
    case PJ_CLI_EINVARG:
	send_inv_arg(sess, &info, PJ_TRUE);		
	break;
    case PJ_CLI_ETOOMANYARGS:
	send_too_many_arg(sess, &info, PJ_TRUE);
	break;
    case PJ_CLI_EMISSINGARG:
    case PJ_CLI_EAMBIGUOUS:
	send_ambi_arg(sess, &info, PJ_TRUE);
	break;
    case PJ_SUCCESS:	
	if (info.hint_cnt > 0) {	
	    /* Compelete command */	    
	    send_ambi_arg(sess, &info, PJ_TRUE);	    		
	} else {
	    retval = PJ_FALSE;
	}	
	break;
    }

    pj_pool_release(pool);	
    return retval; 
}

static pj_bool_t handle_exec(pj_cli_sess *sess)
{
    pj_status_t status;
    pj_bool_t retval = PJ_TRUE;
    
    pj_pool_t *pool;    
    pj_cli_exec_info info;
    pj_cli_t *cli = sess->fe->cli;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    char *recv_buf = fe->input.buf;
        
    printf("\r\n");

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "handle_exec",
			  PJ_CLI_CONSOLE_POOL_SIZE, PJ_CLI_CONSOLE_POOL_INC,
			  NULL);    
    
    status = pj_cli_sess_exec(sess, recv_buf, 
			      pool, &info);

    switch (status) {
    case PJ_CLI_EINVARG:
	send_inv_arg(sess, &info, PJ_FALSE);	
	break;
    case PJ_CLI_ETOOMANYARGS:
	send_too_many_arg(sess, &info, PJ_FALSE);
	break;
    case PJ_CLI_EAMBIGUOUS:
    case PJ_CLI_EMISSINGARG:
	send_ambi_arg(sess, &info, PJ_FALSE);
	break;
    case PJ_CLI_EEXIT:
	retval = PJ_FALSE;
	break;
    case PJ_SUCCESS:
	send_prompt_str(sess);	
	break;
    }    

    pj_pool_release(pool);	
    return retval; 
}

static int readline_thread(void * p)
{    
    struct cli_console_fe * fe = (struct cli_console_fe *)p;

    printf("%s", fe->cfg.prompt_str.ptr);

    while (!fe->thread_quit) {
	pj_size_t input_len = 0;
	pj_str_t input_str;
	char *recv_buf = fe->input.buf;
	pj_bool_t is_valid = PJ_TRUE;

	if (fgets(recv_buf, fe->input.maxlen, stdin) == NULL) {
	    /* 
	     * Be friendly to users who redirect commands into
	     * program, when file ends, resume with kbd.
	     * If exit is desired end script with q for quit
	     */
 	    /* Reopen stdin/stdout/stderr to /dev/console */
#if ((defined(PJ_WIN32) && PJ_WIN32!=0) || \
     (defined(PJ_WIN64) && PJ_WIN64!=0)) && \
     (!defined(PJ_WIN32_WINCE) || PJ_WIN32_WINCE==0)
	    if (freopen ("CONIN$", "r", stdin) == NULL) {
#else
	    if (1) {
#endif
		puts("Cannot switch back to console from file redirection");
		if (fe->cfg.quit_command.slen) {
		    pj_memcpy(recv_buf, fe->cfg.quit_command.ptr, 
			      fe->input.maxlen);
		}
		recv_buf[fe->cfg.quit_command.slen] = '\0';		
	    } else {
		puts("Switched back to console from file redirection");
		continue;
	    }
	}

	input_str.ptr = recv_buf;
	input_str.slen = pj_ansi_strlen(recv_buf);
	pj_strrtrim(&input_str);
	recv_buf[input_str.slen] = '\n';
	recv_buf[input_str.slen+1] = 0;
	if (fe->thread_quit) {
	    break;
	}
	input_len = pj_ansi_strlen(fe->input.buf);	
	if ((input_len > 1) && (fe->input.buf[input_len-2] == '?')) {
	    fe->input.buf[input_len-1] = 0;
	    is_valid = handle_hint(fe->sess);
	    if (!is_valid)
		printf("%s", fe->cfg.prompt_str.ptr);
	} else {
	    is_valid = handle_exec(fe->sess);
	}

        pj_sem_post(fe->input.sem);        
        pj_sem_wait(fe->thread_sem);
    }    

    return 0;
}

PJ_DEF(pj_status_t) pj_cli_console_process(pj_cli_sess *sess, 
					   char *buf,
					   unsigned maxlen)
{
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    fe->input.buf = buf;
    fe->input.maxlen = maxlen;

    if (!fe->input_thread) {
        pj_status_t status;

        status = pj_thread_create(fe->pool, NULL, &readline_thread, fe,
                                  0, 0, &fe->input_thread);
        if (status != PJ_SUCCESS)
            return status;
    } else {
        /* Wake up readline thread */
        pj_sem_post(fe->thread_sem);
    }

    pj_sem_wait(fe->input.sem);

    return (pj_cli_is_quitting(fe->base.cli)? PJ_CLI_EEXIT : PJ_SUCCESS);
}

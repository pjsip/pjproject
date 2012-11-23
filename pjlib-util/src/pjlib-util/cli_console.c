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

#if defined(PJ_LINUX) && PJ_LINUX != 0 || \
    defined(PJ_DARWINOS) && PJ_DARWINOS != 0
#include <termios.h>

static struct termios old, new;

/* Initialize new terminal i/o settings */
void initTermios(int echo) 
{
    tcgetattr(0, &old); 
    new = old; 
    new.c_lflag &= ~ICANON; 
    new.c_lflag &= echo ? ECHO : ~ECHO; 
    tcsetattr(0, TCSANOW, &new); 
}

/* Restore old terminal i/o settings */
void resetTermios(void) 
{
    tcsetattr(0, TCSANOW, &old);
}

/* Read 1 character - echo defines echo mode */
char getch_(int echo) 
{
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}

/* Read 1 character without echo */
char getch(void) 
{
    return getch_(0);
}

#endif

/**
 * This specify the state of input character parsing.
 */
typedef enum cmd_parse_state
{
    ST_NORMAL,
    ST_SCANMODE,
    ST_ESC,
    ST_ARROWMODE
} cmd_parse_state;

/**
 * This structure contains the command line shown to the user.  
 */
typedef struct console_recv_buf {
    /**
     * Buffer containing the characters, NULL terminated.
     */
    unsigned char	    rbuf[PJ_CLI_MAX_CMDBUF];

    /**
     * Current length of the command line.
     */
    unsigned		    len;

    /**
     * Current cursor position.
     */
    unsigned		    cur_pos;
} console_recv_buf;

typedef struct cmd_history
{
    PJ_DECL_LIST_MEMBER(struct cmd_history);
    pj_str_t command;
} cmd_history;

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
	console_recv_buf    recv_buf;        
        pj_sem_t	   *sem;
    } input;

    cmd_history		    *history;
    cmd_history		    *active_history;
};

static unsigned recv_buf_right_len(console_recv_buf *recv_buf)
{
    return (recv_buf->len - recv_buf->cur_pos);
}

static pj_bool_t recv_buf_insert(console_recv_buf *recv_buf, 
				 unsigned char *data) 
{    
    if (recv_buf->len+1 >= PJ_CLI_MAX_CMDBUF) {
	return PJ_FALSE;
    } else {	
	if (*data == '\t' || *data == '?' || *data == '\r') {
	    /* Always insert to the end of line */
	    recv_buf->rbuf[recv_buf->len] = *data;
	} else {
	    /* Insert based on the current cursor pos */
	    unsigned cur_pos = recv_buf->cur_pos;
	    unsigned rlen = recv_buf_right_len(recv_buf);	    
	    if (rlen > 0) {		    
		/* Shift right characters */
		pj_memmove(&recv_buf->rbuf[cur_pos+1], 
			   &recv_buf->rbuf[cur_pos], 
			   rlen+1);
	    } 
	    recv_buf->rbuf[cur_pos] = *data;
	}
	++recv_buf->cur_pos;
	++recv_buf->len;
	recv_buf->rbuf[recv_buf->len] = 0;
    }
    return PJ_TRUE;
}

static pj_bool_t recv_buf_backspace(console_recv_buf *recv_buf)
{
    if ((recv_buf->cur_pos == 0) || (recv_buf->len == 0)) {
	return PJ_FALSE;
    } else {
	unsigned rlen = recv_buf_right_len(recv_buf);
	if (rlen) {
	    unsigned cur_pos = recv_buf->cur_pos;
	    pj_memmove(&recv_buf->rbuf[cur_pos-1], &recv_buf->rbuf[cur_pos], 
		       rlen);
	}
	--recv_buf->cur_pos;
	--recv_buf->len;
	recv_buf->rbuf[recv_buf->len] = 0;
    }    
    return PJ_TRUE;
}

static void cli_console_write_log(pj_cli_front_end *fe, int level,
		                  const char *data, int len)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    if (cfe->sess->log_level > level)
        printf("%.*s", len, data);
}


static void cli_console_quit(pj_cli_front_end *fe, pj_cli_sess *req)
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

static void cli_console_destroy(pj_cli_front_end *fe)
{
    struct cli_console_fe * cfe = (struct cli_console_fe *)fe;

    pj_assert(cfe);
    cli_console_quit(fe, NULL);

    if (cfe->input_thread)
        pj_thread_destroy(cfe->input_thread);
    pj_sem_destroy(cfe->thread_sem);
    pj_sem_destroy(cfe->input.sem);
    pj_pool_release(cfe->pool);
}

PJ_DEF(void) pj_cli_console_cfg_default(pj_cli_console_cfg *param)
{
    pj_assert(param);

    param->log_level = PJ_CLI_CONSOLE_LOG_LEVEL;    
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

    PJ_ASSERT_RETURN(cli && p_sess, PJ_EINVAL);

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "console_fe",
                          PJ_CLI_CONSOLE_POOL_SIZE, PJ_CLI_CONSOLE_POOL_INC,
                          NULL);
    if (!pool)
        return PJ_ENOMEM;
    sess = PJ_POOL_ZALLOC_T(pool, pj_cli_sess);
    fe = PJ_POOL_ZALLOC_T(pool, struct cli_console_fe);
    fe->history = PJ_POOL_ZALLOC_T(pool, struct cmd_history);
    pj_list_init(fe->history);
    fe->active_history = fe->history;

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
    fe->base.op->on_write_log = &cli_console_write_log;
    fe->base.op->on_quit = &cli_console_quit;
    fe->base.op->on_destroy = &cli_console_destroy;
    fe->pool = pool;
    fe->sess = sess;
    pj_sem_create(pool, "console_fe", 0, 1, &fe->thread_sem);
    pj_sem_create(pool, "console_fe", 0, 1, &fe->input.sem);
    pj_cli_register_front_end(cli, &fe->base);

    if (fe->cfg.prompt_str.slen == 0) {	
	pj_str_t prompt_sign = pj_str(">>> ");
	char *prompt_data = pj_pool_alloc(fe->pool, 5);	
	fe->cfg.prompt_str.ptr = prompt_data;	

	pj_strcpy(&fe->cfg.prompt_str, &prompt_sign);
	prompt_data[4] = 0;
    }

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

    send_data.ptr = &data_str[0];
    send_data.slen = 0;
    
    pj_strcat(&send_data, &fe->cfg.prompt_str);
    send_data.ptr[send_data.slen] = 0;

    printf("%s", send_data.ptr);
}

static void send_err_arg(pj_cli_sess *sess, 
			 const pj_cli_exec_info *info, 
			 const pj_str_t *msg,
			 pj_bool_t with_return,
			 pj_bool_t with_last_cmd)
{
    pj_str_t send_data;
    char data_str[256];
    unsigned len;
    unsigned i;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    console_recv_buf *recv_buf = &fe->input.recv_buf;

    send_data.ptr = &data_str[0];
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
    if (with_last_cmd)
	pj_strcat2(&send_data, (char *)&recv_buf->rbuf[0]);

    send_data.ptr[send_data.slen] = 0;
    printf("%s", send_data.ptr);
}

static void send_return_key(pj_cli_sess *sess) 
{    
    PJ_UNUSED_ARG(sess);
    printf("\r\n");    
}

static void send_inv_arg(pj_cli_sess *sess, 
			 const pj_cli_exec_info *info,
			 pj_bool_t with_return,
			 pj_bool_t with_last_cmd)
{
    static const pj_str_t ERR_MSG = {"%Error : Invalid Arguments\r\n", 28};
    send_err_arg(sess, info, &ERR_MSG, with_return, with_last_cmd);
}

static void send_too_many_arg(pj_cli_sess *sess, 
			      const pj_cli_exec_info *info,
			      pj_bool_t with_return,
			      pj_bool_t with_last_cmd)
{
    static const pj_str_t ERR_MSG = {"%Error : Too Many Arguments\r\n", 29};
    send_err_arg(sess, info, &ERR_MSG, with_return, with_last_cmd);
}

static void send_ambi_arg(pj_cli_sess *sess, 
			  const pj_cli_exec_info *info,
			  pj_bool_t with_return,
			  pj_bool_t with_last_cmd)
{
    unsigned i;
    pj_ssize_t j;
    unsigned len;
    pj_str_t send_data;
    char data[1028];
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    console_recv_buf *recv_buf = &fe->input.recv_buf;
    const pj_cli_hint_info *hint = info->hint;
    pj_bool_t is_process_sc = PJ_FALSE;
    pj_bool_t valid_type = PJ_FALSE;
    pj_ssize_t max_length = 0;
    const pj_str_t sc_type = pj_str("SC");
    send_data.ptr = &data[0];
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
	if (hint[i].name.slen > max_length) {
	    max_length = hint[i].name.slen;
	}
    }

    for (i=0;i<info->hint_cnt;++i) {	
	if ((&hint[i].type) && (hint[i].type.slen > 0)) {
	    valid_type = PJ_TRUE;
	    if (pj_stricmp(&hint[i].type, &sc_type) == 0) {
		if (is_process_sc) {
		    pj_strcat2(&send_data, ", ");
		} else {
		    pj_strcat2(&send_data, "\r\n\t Shorcut: ");
		    is_process_sc = PJ_TRUE;
		}
		pj_strcat(&send_data, &hint[i].name);
	    } else {
		is_process_sc = PJ_FALSE;
	    }
	} else {	    
	    valid_type = PJ_FALSE;
	    is_process_sc = PJ_FALSE;
	}

	if (!is_process_sc) {
	    pj_strcat2(&send_data, "\r\n\t");

	    if (valid_type) {
		pj_strcat2(&send_data, "<");
		pj_strcat(&send_data, &hint[i].type);
		pj_strcat2(&send_data, ">");	
	    } else {
		pj_strcat(&send_data, &hint[i].name);	    
	    }
    	
	    if ((&hint[i].desc) && (hint[i].desc.slen > 0)) {
		if (!valid_type) {
		    for (j=0;j<(max_length-hint[i].name.slen);++j) {
			pj_strcat2(&send_data, " ");
		    }
		}
		pj_strcat2(&send_data, "\t");
		pj_strcat(&send_data, &hint[i].desc);	    
	    }
	}
    }        
    pj_strcat2(&send_data, "\r\n");
    pj_strcat(&send_data, &fe->cfg.prompt_str);
    if (with_last_cmd)
	pj_strcat2(&send_data, (char *)&recv_buf->rbuf[0]);

    send_data.ptr[send_data.slen] = 0;
    printf("%s", send_data.ptr);
}

static void send_comp_arg(pj_cli_exec_info *info)
{
    pj_str_t send_data;
    char data[128];

    pj_strcat2(&info->hint[0].name, " ");

    send_data.ptr = &data[0];
    send_data.slen = 0;

    pj_strcat(&send_data, &info->hint[0].name);        

    send_data.ptr[send_data.slen] = 0;
    printf("%s", send_data.ptr);
}

static int compare_str(void *value, const pj_list_type *nd)
{
    cmd_history *node = (cmd_history*)nd;
    return (pj_strcmp((pj_str_t *)value, &node->command));
}

static pj_status_t insert_history(pj_cli_sess *sess, 				 
				  char *cmd_val)
{
    cmd_history *in_history;
    pj_str_t cmd;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;    
    cmd.ptr = cmd_val;
    cmd.slen = pj_ansi_strlen(cmd_val)-1;

    if (cmd.slen == 0)
	return PJ_SUCCESS;

    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    /* Find matching history */
    in_history = pj_list_search(fe->history, (void*)&cmd, compare_str);
    if (!in_history) {
	if (pj_list_size(fe->history) < PJ_CLI_MAX_CMD_HISTORY) {	    
	    char *data_history;
	    in_history = PJ_POOL_ZALLOC_T(fe->pool, cmd_history);
	    pj_list_init(in_history);
	    data_history = (char *)pj_pool_calloc(fe->pool, 
			   sizeof(char), PJ_CLI_MAX_CMDBUF);
	    in_history->command.ptr = data_history;
	    in_history->command.slen = 0;
	} else {
	    /* Get the oldest history */
	    in_history = fe->history->prev;
	}	    
    } else {
	pj_list_insert_nodes_after(in_history->prev, in_history->next);
    }
    pj_strcpy(&in_history->command, pj_strtrim(&cmd));
    pj_list_push_front(fe->history, in_history);
    fe->active_history = fe->history;

    return PJ_SUCCESS;
}

static pj_str_t* get_prev_history(pj_cli_sess *sess, pj_bool_t is_forward)
{
    pj_str_t *retval;
    pj_size_t history_size;
    cmd_history *node;
    cmd_history *root;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    PJ_ASSERT_RETURN(sess && fe, NULL);

    node = fe->active_history;
    root = fe->history;
    history_size = pj_list_size(fe->history);

    if (history_size == 0) {
	return NULL;
    } else {
	if (is_forward) {	    
	    node = (node->next==root)?node->next->next:node->next;	    
	} else {
	    node = (node->prev==root)?node->prev->prev:node->prev;	    
	}
	retval = &node->command;
	fe->active_history = node;
    }
    return retval;
}

static pj_bool_t handle_alfa_num(console_recv_buf *recv_buf, 
				 unsigned char *cdata)
{        
    if (recv_buf_right_len(recv_buf) > 0) {
	char out_str[255];	
	pj_memset(&out_str[0], 0, 255);
	out_str[0] = *cdata;
	pj_memcpy(&out_str[1], &recv_buf->rbuf[recv_buf->cur_pos], 
	          recv_buf_right_len(recv_buf));
	pj_memset(&out_str[recv_buf_right_len(recv_buf)+1], '\b', 
	          recv_buf_right_len(recv_buf));	
	printf("%s", out_str); 
    } else {	
	printf("%c", *cdata);
    }    
    return PJ_TRUE;
}

static pj_bool_t handle_backspace(console_recv_buf *recv_buf)
{    
    if (recv_buf_backspace(recv_buf)) {
	if(recv_buf_right_len(recv_buf) > 0) {
	    char out_str[255];
	    pj_memset(&out_str[0], 0, 255);
	    out_str[0] = '\b';
	    pj_memcpy(&out_str[1], &recv_buf->rbuf[recv_buf->cur_pos], 
		recv_buf_right_len(recv_buf));
	    out_str[recv_buf_right_len(recv_buf)+1] = ' '; 
	    pj_memset(&out_str[recv_buf_right_len(recv_buf)+2], '\b', 
		recv_buf_right_len(recv_buf)+1);
	    printf("%s", out_str);
	} else {
	    char out_str[4];
	    out_str[0] = '\b';
	    out_str[1] = ' ';
	    out_str[2] = '\b';
	    out_str[3] = 0;
	    printf("%s", out_str);
	}
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

static pj_bool_t handle_tab(pj_cli_sess *sess)
{
    pj_status_t status;
    pj_bool_t retval = PJ_TRUE;
    unsigned len;
    
    pj_pool_t *pool;
    pj_cli_cmd_val *cmd_val;
    pj_cli_exec_info info;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    console_recv_buf *recv_buf = &fe->input.recv_buf;
    pj_cli_t *cli = sess->fe->cli;

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "handle_tab",
                          PJ_CLI_CONSOLE_POOL_SIZE, PJ_CLI_CONSOLE_POOL_INC,
                          NULL);

    cmd_val = PJ_POOL_ZALLOC_T(pool, pj_cli_cmd_val);
    
    status = pj_cli_sess_parse(sess, (char *)recv_buf->rbuf, cmd_val, 
			       pool, &info);    

    len = pj_ansi_strlen((char *)recv_buf->rbuf);

    switch (status) {
    case PJ_CLI_EINVARG:
	send_inv_arg(sess, &info, PJ_TRUE, PJ_TRUE);		
	break;
    case PJ_CLI_ETOOMANYARGS:
	send_too_many_arg(sess, &info, PJ_TRUE, PJ_TRUE);
	break;
    case PJ_CLI_EMISSINGARG:
    case PJ_CLI_EAMBIGUOUS:
	send_ambi_arg(sess, &info, PJ_TRUE, PJ_TRUE);
	break;
    case PJ_SUCCESS:
	if (len > recv_buf->cur_pos)
	{
	    /* Send the cursor to EOL */
	    unsigned char *data_sent = &recv_buf->rbuf[recv_buf->cur_pos-1];
	    printf("%s", data_sent);	    
	}
	if (info.hint_cnt > 0) {	
	    /* Compelete command */
	    send_comp_arg(&info);

	    pj_memcpy(&recv_buf->rbuf[len],  
		      &info.hint[0].name.ptr[0], info.hint[0].name.slen);

	    len += info.hint[0].name.slen;
	    recv_buf->rbuf[len] = 0;		    
	} else {
	    retval = PJ_FALSE;
	}
	break;
    }
    recv_buf->len = len;
    recv_buf->cur_pos = len;

    pj_pool_release(pool);	
    return retval; 
}

static pj_bool_t handle_return(pj_cli_sess *sess)
{
    pj_status_t status;
    pj_bool_t retval = PJ_TRUE;
    
    pj_pool_t *pool;    
    pj_cli_exec_info info;
    pj_cli_t *cli = sess->fe->cli;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    console_recv_buf *recv_buf = &fe->input.recv_buf;
    
    send_return_key(sess);    
    insert_history(sess, (char *)&recv_buf->rbuf[0]);

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "handle_return",
			  PJ_CLI_CONSOLE_POOL_SIZE, PJ_CLI_CONSOLE_POOL_INC,
			  NULL);    
    
    status = pj_cli_sess_exec(sess, (char *)&recv_buf->rbuf[0], 
			      pool, &info);

    switch (status) {
    case PJ_CLI_EINVARG:
	send_inv_arg(sess, &info, PJ_FALSE, PJ_FALSE);	
	break;
    case PJ_CLI_ETOOMANYARGS:
	send_too_many_arg(sess, &info, PJ_FALSE, PJ_FALSE);
	break;
    case PJ_CLI_EAMBIGUOUS:
    case PJ_CLI_EMISSINGARG:
	send_ambi_arg(sess, &info, PJ_FALSE, PJ_FALSE);
	break;
    case PJ_CLI_EEXIT:
	retval = PJ_FALSE;
	break;
    case PJ_SUCCESS:
	send_prompt_str(sess);
	break;
    }    
    if (retval) {
	recv_buf->rbuf[0] = 0;
	recv_buf->len = 0;
	recv_buf->cur_pos = 0;
    }

    pj_pool_release(pool);	
    return retval; 
}

static pj_bool_t handle_up_down(pj_cli_sess *sess, pj_bool_t is_up)
{
    pj_str_t *history;
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;
    console_recv_buf *recv_buf = &fe->input.recv_buf;

    PJ_ASSERT_RETURN(sess && fe, PJ_FALSE);

    history = get_prev_history(sess, is_up);
    if (history) {
	pj_str_t send_data;
	char str[PJ_CLI_MAX_CMDBUF];
	send_data.ptr = &str[0];
	send_data.slen = 0;

	if (recv_buf->cur_pos > 0) {
	    pj_memset(send_data.ptr, 0x08, recv_buf->cur_pos);
	    send_data.slen = recv_buf->cur_pos;
	}

	if (recv_buf->len > (unsigned)history->slen) {
	    unsigned buf_len = recv_buf->len;
	    pj_memset(&send_data.ptr[send_data.slen], 0x20, buf_len);
	    send_data.slen += buf_len;
	    pj_memset(&send_data.ptr[send_data.slen], 0x08, buf_len);
	    send_data.slen += buf_len;
	} 
	/* Send data */
	pj_strcat(&send_data, history);	
	send_data.ptr[send_data.slen] = 0;
	printf("%s", send_data.ptr);
	pj_ansi_strncpy((char*)&recv_buf->rbuf, history->ptr, history->slen);
	recv_buf->rbuf[history->slen] = 0;
	recv_buf->len = history->slen;
	recv_buf->cur_pos = recv_buf->len;
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

static pj_bool_t handle_left_key(console_recv_buf *recv_buf)
{
    const static unsigned char BACK_SPACE = 0x08;
    if (recv_buf->cur_pos) {
	printf("%c", BACK_SPACE);
	--recv_buf->cur_pos;
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

static pj_bool_t handle_right_key(console_recv_buf *recv_buf)
{
    if (recv_buf_right_len(recv_buf)) {
	unsigned char *data = &recv_buf->rbuf[recv_buf->cur_pos++];
	printf("%c", *data);
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

static int readchar_thread(void * p)
{    
    struct cli_console_fe * fe = (struct cli_console_fe *)p;
    cmd_parse_state parse_state = ST_NORMAL;

    printf("%s", fe->cfg.prompt_str.ptr);

    while (!fe->thread_quit) {
	unsigned char cdata;
	console_recv_buf *recv_buf = &fe->input.recv_buf;
	pj_bool_t is_valid = PJ_TRUE;

	cdata = (unsigned char)getch();

	switch (parse_state) {
	case ST_NORMAL:
	    if (cdata == '\b') {
		is_valid = handle_backspace(recv_buf);
	    } else if (cdata == 224) {	    
		parse_state = ST_SCANMODE;
	    } else if (cdata == 27) {	    
		parse_state = ST_ESC;
	    } else {
		if (cdata == '\n')
		    cdata = '\r';
		if (recv_buf_insert(recv_buf, &cdata)) {
		    if (cdata == '\r') {
			is_valid = handle_return(fe->sess);
		    } else if ((cdata == '\t') || (cdata == '?')) {
			is_valid = handle_tab(fe->sess);
		    } else if (cdata > 31 && cdata < 127) {
			is_valid = handle_alfa_num(recv_buf, &cdata);			
		    }
		} else {
		    is_valid = PJ_FALSE;
		}
	    }	    
	    break;
	case ST_SCANMODE:
	    switch (cdata) {
	    case 72:
		//UP
	    case 80:
		//DOwN
		is_valid = handle_up_down(fe->sess, (cdata==72));
		break;
	    case 75:
		is_valid = handle_left_key(recv_buf);
		//LEFT
		break;
	    case 77:
		is_valid = handle_right_key(recv_buf);
		//RIGHT
		break;
	    };
	    parse_state = ST_NORMAL;
	    break;
	case ST_ESC:
	    parse_state = (cdata == 91)?ST_ARROWMODE:ST_NORMAL;
	    break;
	case ST_ARROWMODE:
	    switch (cdata) {
	    case 65:
		//UP
	    case 66:
		//DOwN
		is_valid = handle_up_down(fe->sess, (cdata==65));
		break;
	    case 68:
		is_valid = handle_left_key(recv_buf);
		//LEFT
		break;
	    case 67:
		is_valid = handle_right_key(recv_buf);
		//RIGHT
		break;
	    };
	    parse_state = ST_NORMAL;	    
	    break;
	};

        pj_sem_post(fe->input.sem);        
        pj_sem_wait(fe->thread_sem);
    }
    fe->input_thread = NULL;

    return 0;
}

PJ_DEF(pj_status_t) pj_cli_console_process(pj_cli_sess *sess)
{
    struct cli_console_fe *fe = (struct cli_console_fe *)sess->fe;

    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    if (!fe->input_thread) {
        pj_status_t status;

        status = pj_thread_create(fe->pool, NULL, &readchar_thread, fe,
                                  0, 0, &fe->input_thread);
        if (status != PJ_SUCCESS)
            return status;
    } else {
        /* Wake up readchar thread */
        pj_sem_post(fe->thread_sem);
    }

    pj_sem_wait(fe->input.sem);

    return (pj_cli_is_quitting(fe->base.cli)? PJ_CLI_EEXIT : PJ_SUCCESS);
}

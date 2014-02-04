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
#include <pjlib-util/cli_telnet.h>
#include <pj/activesock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/except.h>
#include <pjlib-util/errno.h>
#include <pjlib-util/scanner.h>
#include <pj/addr_resolv.h>
#include <pj/compat/socket.h>

#if (defined(PJ_WIN32) && PJ_WIN32!=0) || \
    (defined(PJ_WIN64) && PJ_WIN64!=0) || \
    (defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0)

#define EADDRINUSE WSAEADDRINUSE

#endif

#define CLI_TELNET_BUF_SIZE 256

#define CUT_MSG "<..data truncated..>\r\n"
#define MAX_CUT_MSG_LEN 25

#if 1
    /* Enable some tracing */
    #define THIS_FILE   "cli_telnet.c"
    #define TRACE_(arg)	PJ_LOG(3,arg)
#else
    #define TRACE_(arg)
#endif

#define MAX_CLI_TELNET_OPTIONS 256
/** Maximum retry on Telnet Restart **/
#define MAX_RETRY_ON_TELNET_RESTART 100
/** Minimum number of millisecond to wait before retrying to re-bind on
 * telnet restart **/
#define MIN_WAIT_ON_TELNET_RESTART 20
/** Maximum number of millisecod to wait before retrying to re-bind on
 *  telnet restart **/
#define MAX_WAIT_ON_TELNET_RESTART 1000

/**
 * This specify the state for the telnet option negotiation.
 */
enum cli_telnet_option_states
{
    OPT_DISABLE,		/* Option disable */
    OPT_ENABLE,			/* Option enable */
    OPT_EXPECT_DISABLE,		/* Already send disable req, expecting resp */
    OPT_EXPECT_ENABLE,		/* Already send enable req, expecting resp */
    OPT_EXPECT_DISABLE_REV,	/* Already send disable req, expecting resp,
				 * need to send enable req */
    OPT_EXPECT_ENABLE_REV	/* Already send enable req, expecting resp,
				 * need to send disable req */
};

/**
 * This structure contains information for telnet session option negotiation.
 * It contains the local/peer option config and the option negotiation state.
 */
typedef struct cli_telnet_sess_option
{
    /**
     * Local setting for the option.
     * Default: FALSE;
     */
    pj_bool_t local_is_enable;

    /**
     * Remote setting for the option.
     * Default: FALSE;
     */
    pj_bool_t peer_is_enable;

    /**
     * Local state of the option negotiation.
     */
    enum cli_telnet_option_states local_state;

    /**
     * Remote state of the option negotiation.
     */
    enum cli_telnet_option_states peer_state;
} cli_telnet_sess_option;

/**
 * This specify the state of input character parsing.
 */
typedef enum cmd_parse_state
{
    ST_NORMAL,
    ST_CR,
    ST_ESC,
    ST_VT100,
    ST_IAC,
    ST_DO,
    ST_DONT,
    ST_WILL,
    ST_WONT
} cmd_parse_state;

typedef enum cli_telnet_command
{
    SUBNEGO_END	    = 240,	/* End of subnegotiation parameters. */
    NOP		    = 241,	/* No operation. */
    DATA_MARK	    = 242,	/* Marker for NVT cleaning. */
    BREAK	    = 243,	/* Indicates that the "break" key was hit. */
    INT_PROCESS	    = 244,	/* Suspend, interrupt or abort the process. */
    ABORT_OUTPUT    = 245,	/* Abort output, abort output stream. */
    ARE_YOU_THERE   = 246,	/* Are you there. */
    ERASE_CHAR	    = 247,	/* Erase character, erase the current char. */
    ERASE_LINE	    = 248,	/* Erase line, erase the current line. */
    GO_AHEAD	    = 249,	/* Go ahead, other end can transmit. */
    SUBNEGO_BEGIN   = 250,	/* Subnegotiation begin. */
    WILL	    = 251,	/* Accept the use of option. */
    WONT	    = 252,	/* Refuse the use of option. */
    DO		    = 253,	/* Request to use option. */
    DONT	    = 254,	/* Request to not use option. */
    IAC		    = 255	/* Interpret as command */
} cli_telnet_command;

enum cli_telnet_options
{
    TRANSMIT_BINARY	= 0,	/* Transmit Binary. */
    TERM_ECHO		= 1,	/* Echo. */
    RECONNECT		= 2,	/* Reconnection. */
    SUPPRESS_GA		= 3,	/* Suppress Go Aheah. */
    MESSAGE_SIZE_NEGO	= 4,	/* Approx Message Size Negotiation. */
    STATUS		= 5,	/* Status. */
    TIMING_MARK		= 6,	/* Timing Mark. */
    RTCE_OPTION		= 7,	/* Remote Controlled Trans and Echo. */
    OUTPUT_LINE_WIDTH	= 8,	/* Output Line Width. */
    OUTPUT_PAGE_SIZE	= 9,	/* Output Page Size. */
    CR_DISPOSITION	= 10,	/* Carriage-Return Disposition. */
    HORI_TABSTOPS	= 11,	/* Horizontal Tabstops. */
    HORI_TAB_DISPO	= 12,	/* Horizontal Tab Disposition. */
    FF_DISP0		= 13,	/* Formfeed Disposition. */
    VERT_TABSTOPS	= 14,	/* Vertical Tabstops. */
    VERT_TAB_DISPO	= 15,	/* Vertical Tab Disposition. */
    LF_DISP0		= 16,	/* Linefeed Disposition. */
    EXT_ASCII		= 17, 	/* Extended ASCII. */
    LOGOUT		= 18,	/* Logout. */
    BYTE_MACRO		= 19,	/* Byte Macro. */
    DE_TERMINAL		= 20,	/* Data Entry Terminal. */
    SUPDUP_PROTO	= 21,	/* SUPDUP Protocol. */
    SUPDUP_OUTPUT	= 22,	/* SUPDUP Output. */
    SEND_LOC		= 23,	/* Send Location. */
    TERM_TYPE		= 24,	/* Terminal Type. */
    EOR			= 25,	/* End of Record. */
    TACACS_UID		= 26,	/* TACACS User Identification. */
    OUTPUT_MARKING	= 27,	/* Output Marking. */
    TTYLOC		= 28,	/* Terminal Location Number. */
    USE_3270_REGIME	= 29,	/* Telnet 3270 Regime. */
    USE_X3_PAD		= 30,	/* X.3 PAD. */
    WINDOW_SIZE		= 31,	/* Window Size. */
    TERM_SPEED		= 32,	/* Terminal Speed. */
    REM_FLOW_CONTROL	= 33,	/* Remote Flow Control. */
    LINE_MODE		= 34,	/* Linemode. */
    X_DISP_LOC		= 35,	/* X Display Location. */
    ENVIRONMENT		= 36,	/* Environment. */
    AUTH		= 37,	/* Authentication. */
    ENCRYPTION		= 38, 	/* Encryption Option. */
    NEW_ENVIRONMENT	= 39,	/* New Environment. */
    TN_3270E		= 40,	/* TN3270E. */
    XAUTH		= 41,	/* XAUTH. */
    CHARSET		= 42,	/* CHARSET. */
    REM_SERIAL_PORT	= 43,	/* Telnet Remote Serial Port. */
    COM_PORT_CONTROL	= 44,	/* Com Port Control. */
    SUPP_LOCAL_ECHO	= 45,	/* Telnet Suppress Local Echo. */
    START_TLS		= 46,	/* Telnet Start TLS. */
    KERMIT		= 47,	/* KERMIT. */
    SEND_URL		= 48,	/* SEND-URL. */
    FWD_X		= 49,	/* FORWARD_X. */
    EXT_OPTIONS		= 255	/* Extended-Options-List */
};

enum terminal_cmd
{
    TC_ESC		= 27,
    TC_UP		= 65,
    TC_DOWN		= 66,
    TC_RIGHT		= 67,
    TC_LEFT		= 68,
    TC_END		= 70,
    TC_HOME		= 72,
    TC_CTRL_C		= 3,
    TC_CR		= 13,
    TC_BS		= 8,
    TC_TAB		= 9,
    TC_QM		= 63,
    TC_BELL		= 7,
    TC_DEL		= 127
};

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

/**
 * This structure contains the command line shown to the user.
 * The telnet also needs to maintain and manage command cursor position.
 * Due to that reason, the insert/delete character process from buffer will
 * consider its current cursor position.
 */
typedef struct telnet_recv_buf {
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
} telnet_recv_buf;

/**
 * This structure contains the command history executed by user.
 * Besides storing the command history, it is necessary to be able
 * to browse it.
 */
typedef struct cmd_history
{
    PJ_DECL_LIST_MEMBER(struct cmd_history);
    pj_str_t command;
} cmd_history;

typedef struct cli_telnet_sess
{
    pj_cli_sess		    base;
    pj_pool_t		    *pool;
    pj_activesock_t	    *asock;
    pj_bool_t		    authorized;
    pj_ioqueue_op_key_t	    op_key;
    pj_mutex_t		    *smutex;
    cmd_parse_state	    parse_state;
    cli_telnet_sess_option  telnet_option[MAX_CLI_TELNET_OPTIONS];
    cmd_history		    *history;
    cmd_history		    *active_history;

    telnet_recv_buf	    *rcmd;
    unsigned char	    buf[CLI_TELNET_BUF_SIZE + MAX_CUT_MSG_LEN];
    unsigned		    buf_len;
} cli_telnet_sess;

typedef struct cli_telnet_fe
{
    pj_cli_front_end        base;
    pj_pool_t              *pool;
    pj_cli_telnet_cfg       cfg;
    pj_bool_t               own_ioqueue;
    pj_cli_sess             sess_head;

    pj_activesock_t	   *asock;
    pj_thread_t            *worker_thread;
    pj_bool_t               is_quitting;
    pj_mutex_t             *mutex;
} cli_telnet_fe;

/* Forward Declaration */
static pj_status_t telnet_sess_send2(cli_telnet_sess *sess,
                                     const unsigned char *str, int len);

static pj_status_t telnet_sess_send(cli_telnet_sess *sess,
                                    const pj_str_t *str);

static pj_status_t telnet_start(cli_telnet_fe *fe);
static pj_status_t telnet_restart(cli_telnet_fe *tfe);

/**
 * Return the number of characters between the current cursor position
 * to the end of line.
 */
static unsigned recv_buf_right_len(telnet_recv_buf *recv_buf)
{
    return (recv_buf->len - recv_buf->cur_pos);
}

/**
 * Insert character to the receive buffer.
 */
static pj_bool_t recv_buf_insert(telnet_recv_buf *recv_buf,
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

/**
 * Delete character on the previous cursor position of the receive buffer.
 */
static pj_bool_t recv_buf_backspace(telnet_recv_buf *recv_buf)
{
    if ((recv_buf->cur_pos == 0) || (recv_buf->len == 0)) {
	return PJ_FALSE;
    } else {
	unsigned rlen = recv_buf_right_len(recv_buf);
	if (rlen) {
	    unsigned cur_pos = recv_buf->cur_pos;
	    /* Shift left characters */
	    pj_memmove(&recv_buf->rbuf[cur_pos-1], &recv_buf->rbuf[cur_pos],
		       rlen);
	}
	--recv_buf->cur_pos;
	--recv_buf->len;
	recv_buf->rbuf[recv_buf->len] = 0;
    }
    return PJ_TRUE;
}

static int compare_str(void *value, const pj_list_type *nd)
{
    cmd_history *node = (cmd_history*)nd;
    return (pj_strcmp((pj_str_t *)value, &node->command));
}

/**
 * Insert the command to history. If the entered command is not on the list,
 * a new entry will be created. All entered command will be moved to
 * the first entry of the history.
 */
static pj_status_t insert_history(cli_telnet_sess *sess,
				  char *cmd_val)
{
    cmd_history *in_history;
    pj_str_t cmd;
    cmd.ptr = cmd_val;
    cmd.slen = pj_ansi_strlen(cmd_val)-1;

    if (cmd.slen == 0)
	return PJ_SUCCESS;

    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    /* Find matching history */
    in_history = pj_list_search(sess->history, (void*)&cmd, compare_str);
    if (!in_history) {
	if (pj_list_size(sess->history) < PJ_CLI_MAX_CMD_HISTORY) {
	    char *data_history;
	    in_history = PJ_POOL_ZALLOC_T(sess->pool, cmd_history);
	    pj_list_init(in_history);
	    data_history = (char *)pj_pool_calloc(sess->pool,
			   sizeof(char), PJ_CLI_MAX_CMDBUF);
	    in_history->command.ptr = data_history;
	    in_history->command.slen = 0;
	} else {
	    /* Get the oldest history */
	    in_history = sess->history->prev;
	}
    } else {
	pj_list_insert_nodes_after(in_history->prev, in_history->next);
    }
    pj_strcpy(&in_history->command, pj_strtrim(&cmd));
    pj_list_push_front(sess->history, in_history);
    sess->active_history = sess->history;

    return PJ_SUCCESS;
}

/**
 * Get the next or previous history of the shown/active history.
 */
static pj_str_t* get_prev_history(cli_telnet_sess *sess, pj_bool_t is_forward)
{
    pj_str_t *retval;
    pj_size_t history_size;
    cmd_history *node;
    cmd_history *root;

    PJ_ASSERT_RETURN(sess, NULL);

    node = sess->active_history;
    root = sess->history;
    history_size = pj_list_size(sess->history);

    if (history_size == 0) {
	return NULL;
    } else {
	if (is_forward) {
	    node = (node->next==root)?node->next->next:node->next;
	} else {
	    node = (node->prev==root)?node->prev->prev:node->prev;
	}
	retval = &node->command;
	sess->active_history = node;
    }
    return retval;
}

/*
 * This method is used to send option negotiation command.
 * The commands dealing with option negotiation are
 * three byte sequences, the third byte being the code for the option
 * referenced - (RFC-854).
 */
static pj_bool_t send_telnet_cmd(cli_telnet_sess *sess,
				 cli_telnet_command cmd,
				 unsigned char option)
{
    unsigned char buf[3];
    PJ_ASSERT_RETURN(sess, PJ_FALSE);

    buf[0] = IAC;
    buf[1] = cmd;
    buf[2] = option;
    telnet_sess_send2(sess, buf, 3);

    return PJ_TRUE;
}

/**
 * This method will handle sending telnet's ENABLE option negotiation.
 * For local option: send WILL.
 * For remote option: send DO.
 * This method also handle the state transition of the ENABLE
 * negotiation process.
 */
static pj_bool_t send_enable_option(cli_telnet_sess *sess,
				    pj_bool_t is_local,
				    unsigned char option)
{
    cli_telnet_sess_option *sess_option;
    enum cli_telnet_option_states *state;
    PJ_ASSERT_RETURN(sess, PJ_FALSE);

    sess_option = &sess->telnet_option[option];
    state = is_local?(&sess_option->local_state):(&sess_option->peer_state);
    switch (*state) {
	case OPT_ENABLE:
	    /* Ignore if already enabled */
	    break;
	case OPT_DISABLE:
	    *state = OPT_EXPECT_ENABLE;
	    send_telnet_cmd(sess, (is_local?WILL:DO), option);
	    break;
	case OPT_EXPECT_ENABLE:
	    *state = OPT_DISABLE;
	    break;
	case OPT_EXPECT_DISABLE:
	    *state = OPT_EXPECT_DISABLE_REV;
	    break;
	case OPT_EXPECT_ENABLE_REV:
	    *state = OPT_EXPECT_ENABLE;
	    break;
	case OPT_EXPECT_DISABLE_REV:
	    *state = OPT_DISABLE;
	    break;
	default:
	    return PJ_FALSE;
    }
    return PJ_TRUE;
}

static pj_bool_t send_cmd_do(cli_telnet_sess *sess,
			     unsigned char option)
{
    return send_enable_option(sess, PJ_FALSE, option);
}

static pj_bool_t send_cmd_will(cli_telnet_sess *sess,
			       unsigned char option)
{
    return send_enable_option(sess, PJ_TRUE, option);
}

/**
 * This method will handle receiving telnet's ENABLE option negotiation.
 * This method also handle the state transition of the ENABLE
 * negotiation process.
 */
static pj_bool_t receive_enable_option(cli_telnet_sess *sess,
				       pj_bool_t is_local,
				       unsigned char option)
{
    cli_telnet_sess_option *sess_opt;
    enum cli_telnet_option_states *state;
    pj_bool_t opt_ena;
    PJ_ASSERT_RETURN(sess, PJ_FALSE);

    sess_opt = &sess->telnet_option[option];
    state = is_local?(&sess_opt->local_state):(&sess_opt->peer_state);
    opt_ena = is_local?sess_opt->local_is_enable:sess_opt->peer_is_enable;
    switch (*state) {
	case OPT_ENABLE:
	    /* Ignore if already enabled */
    	    break;
	case OPT_DISABLE:
	    if (opt_ena) {
		*state = OPT_ENABLE;
		send_telnet_cmd(sess, is_local?WILL:DO, option);
	    } else {
		send_telnet_cmd(sess, is_local?WONT:DONT, option);
	    }
	    break;
	case OPT_EXPECT_ENABLE:
	    *state = OPT_ENABLE;
	    break;
	case OPT_EXPECT_DISABLE:
	    *state = OPT_DISABLE;
	    break;
	case OPT_EXPECT_ENABLE_REV:
	    *state = OPT_EXPECT_DISABLE;
	    send_telnet_cmd(sess, is_local?WONT:DONT, option);
	    break;
	case OPT_EXPECT_DISABLE_REV:
	    *state = OPT_EXPECT_DISABLE;
	    break;
	default:
	    return PJ_FALSE;
    }
    return PJ_TRUE;
}

/**
 * This method will handle receiving telnet's DISABLE option negotiation.
 * This method also handle the state transition of the DISABLE
 * negotiation process.
 */
static pj_bool_t receive_disable_option(cli_telnet_sess *sess,
					pj_bool_t is_local,
					unsigned char option)
{
    cli_telnet_sess_option *sess_opt;
    enum cli_telnet_option_states *state;

    PJ_ASSERT_RETURN(sess, PJ_FALSE);

    sess_opt = &sess->telnet_option[option];
    state = is_local?(&sess_opt->local_state):(&sess_opt->peer_state);

    switch (*state) {
	case OPT_ENABLE:
	    /* Disabling option always need to be accepted */
	    *state = OPT_DISABLE;
	    send_telnet_cmd(sess, is_local?WONT:DONT, option);
    	    break;
	case OPT_DISABLE:
	    /* Ignore if already enabled */
	    break;
	case OPT_EXPECT_ENABLE:
	case OPT_EXPECT_DISABLE:
	    *state = OPT_DISABLE;
	    break;
	case OPT_EXPECT_ENABLE_REV:
	    *state = OPT_DISABLE;
	    send_telnet_cmd(sess, is_local?WONT:DONT, option);
	    break;
	case OPT_EXPECT_DISABLE_REV:
	    *state = OPT_EXPECT_ENABLE;
	    send_telnet_cmd(sess, is_local?WILL:DO, option);
	    break;
	default:
	    return PJ_FALSE;
    }
    return PJ_TRUE;
}

static pj_bool_t receive_do(cli_telnet_sess *sess, unsigned char option)
{
    return receive_enable_option(sess, PJ_TRUE, option);
}

static pj_bool_t receive_dont(cli_telnet_sess *sess, unsigned char option)
{
    return receive_disable_option(sess, PJ_TRUE, option);
}

static pj_bool_t receive_will(cli_telnet_sess *sess, unsigned char option)
{
    return receive_enable_option(sess, PJ_FALSE, option);
}

static pj_bool_t receive_wont(cli_telnet_sess *sess, unsigned char option)
{
    return receive_disable_option(sess, PJ_FALSE, option);
}

static void set_local_option(cli_telnet_sess *sess,
			     unsigned char option,
			     pj_bool_t enable)
{
    sess->telnet_option[option].local_is_enable = enable;
}

static void set_peer_option(cli_telnet_sess *sess,
			    unsigned char option,
			    pj_bool_t enable)
{
    sess->telnet_option[option].peer_is_enable = enable;
}

static pj_bool_t is_local_option_state_ena(cli_telnet_sess *sess,
					   unsigned char option)
{
    return (sess->telnet_option[option].local_state == OPT_ENABLE);
}

static void send_return_key(cli_telnet_sess *sess)
{
    telnet_sess_send2(sess, (unsigned char*)"\r\n", 2);
}

static void send_bell(cli_telnet_sess *sess) {
    static const unsigned char bell = 0x07;
    telnet_sess_send2(sess, &bell, 1);
}

static void send_prompt_str(cli_telnet_sess *sess)
{
    pj_str_t send_data;
    char data_str[128];
    cli_telnet_fe *fe = (cli_telnet_fe *)sess->base.fe;

    send_data.ptr = data_str;
    send_data.slen = 0;

    pj_strcat(&send_data, &fe->cfg.prompt_str);

    telnet_sess_send(sess, &send_data);
}

/*
 * This method is used to send error message to client, including
 * the error position of the source command.
 */
static void send_err_arg(cli_telnet_sess *sess,
			 const pj_cli_exec_info *info,
			 const pj_str_t *msg,
			 pj_bool_t with_return,
			 pj_bool_t with_last_cmd)
{
    pj_str_t send_data;
    char data_str[256];
    pj_size_t len;
    unsigned i;
    cli_telnet_fe *fe = (cli_telnet_fe *)sess->base.fe;

    send_data.ptr = data_str;
    send_data.slen = 0;

    if (with_return)
	pj_strcat2(&send_data, "\r\n");

    len = fe->cfg.prompt_str.slen + info->err_pos;

    /* Set the error pointer mark */
    for (i=0;i<len;++i) {
	pj_strcat2(&send_data, " ");
    }
    pj_strcat2(&send_data, "^");
    pj_strcat2(&send_data, "\r\n");
    pj_strcat(&send_data, msg);
    pj_strcat(&send_data, &fe->cfg.prompt_str);
    if (with_last_cmd)
	pj_strcat2(&send_data, (char *)sess->rcmd->rbuf);

    telnet_sess_send(sess, &send_data);
}

static void send_inv_arg(cli_telnet_sess *sess,
			 const pj_cli_exec_info *info,
			 pj_bool_t with_return,
			 pj_bool_t with_last_cmd)
{
    static const pj_str_t ERR_MSG = {"%Error : Invalid Arguments\r\n", 28};
    send_err_arg(sess, info, &ERR_MSG, with_return, with_last_cmd);
}

static void send_too_many_arg(cli_telnet_sess *sess,
			      const pj_cli_exec_info *info,
			      pj_bool_t with_return,
			      pj_bool_t with_last_cmd)
{
    static const pj_str_t ERR_MSG = {"%Error : Too Many Arguments\r\n", 29};
    send_err_arg(sess, info, &ERR_MSG, with_return, with_last_cmd);
}

static void send_hint_arg(cli_telnet_sess *sess,
			  pj_str_t *send_data,
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
	telnet_sess_send(sess, send_data);
	send_data->slen = 0;
    }
}

/*
 * This method is used to notify to the client that the entered command
 * is ambiguous. It will show the matching command as the hint information.
 */
static void send_ambi_arg(cli_telnet_sess *sess,
			  const pj_cli_exec_info *info,
			  pj_bool_t with_return,
			  pj_bool_t with_last_cmd)
{
    unsigned i;
    pj_size_t len;
    pj_str_t send_data;
    char data[1028];
    cli_telnet_fe *fe = (cli_telnet_fe *)sess->base.fe;
    const pj_cli_hint_info *hint = info->hint;
    out_parse_state parse_state = OP_NORMAL;
    pj_ssize_t max_length = 0;
    pj_ssize_t cmd_length = 0;
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
    /* Build hint information */
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
	    /* Format : "[Choice Value]  description" */
	    pj_strcat2(&send_data, "[");
	    pj_strcat(&send_data, &hint[i].name);
	    pj_strcat2(&send_data, "]");
	    break;
	case OP_TYPE:
	    /* Format : "<Argument Type>  description" */
	    pj_strcat2(&send_data, "<");
	    pj_strcat(&send_data, &hint[i].name);
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
	    /* Command */
	    pj_strcat(&send_data, &hint[i].name);
	    break;
	}

	if ((parse_state == OP_TYPE) || (parse_state == OP_CHOICE) ||
	    ((i+1) >= info->hint_cnt) ||
	    (pj_strncmp(&hint[i].desc, &hint[i+1].desc, hint[i].desc.slen)))
	{
	    /* Add description info */
	    send_hint_arg(sess, &send_data,
			  &hint[i].desc, cmd_length,
			  max_length);

	    cmd_length = 0;
	}
    }
    pj_strcat2(&send_data, "\r\n");
    pj_strcat(&send_data, &fe->cfg.prompt_str);
    if (with_last_cmd)
	pj_strcat2(&send_data, (char *)sess->rcmd->rbuf);

    telnet_sess_send(sess, &send_data);
}

/*
 * This method is to send command completion of the entered command.
 */
static void send_comp_arg(cli_telnet_sess *sess,
			  pj_cli_exec_info *info)
{
    pj_str_t send_data;
    char data[128];

    pj_strcat2(&info->hint[0].name, " ");

    send_data.ptr = data;
    send_data.slen = 0;

    pj_strcat(&send_data, &info->hint[0].name);

    telnet_sess_send(sess, &send_data);
}

/*
 * This method is to process the alfa numeric character sent by client.
 */
static pj_bool_t handle_alfa_num(cli_telnet_sess *sess, unsigned char *data)
{
    if (is_local_option_state_ena(sess, TERM_ECHO)) {
	if (recv_buf_right_len(sess->rcmd) > 0) {
	    /* Cursor is not at EOL, insert character */
	    unsigned char echo[5] = {0x1b, 0x5b, 0x31, 0x40, 0x00};
	    echo[4] = *data;
	    telnet_sess_send2(sess, echo, 5);
	} else {
	    /* Append character */
	    telnet_sess_send2(sess, data, 1);
	}
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

/*
 * This method is to process the backspace character sent by client.
 */
static pj_bool_t handle_backspace(cli_telnet_sess *sess, unsigned char *data)
{
    unsigned rlen = recv_buf_right_len(sess->rcmd);
    if (recv_buf_backspace(sess->rcmd)) {
	if (rlen) {
	    /*
	     * Cursor is not at the end of line, move the characters
	     * after the cursor to left
	     */
	    unsigned char echo[5] = {0x00, 0x1b, 0x5b, 0x31, 0x50};
	    echo[0] = *data;
	    telnet_sess_send2(sess, echo, 5);
	} else {
	    const static unsigned char echo[3] = {0x08, 0x20, 0x08};
	    telnet_sess_send2(sess, echo, 3);
	}
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

/*
 * Syntax error handler for parser.
 */
static void on_syntax_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner);
    PJ_THROW(PJ_EINVAL);
}

/*
 * This method is to process the backspace character sent by client.
 */
static pj_status_t get_last_token(pj_str_t *cmd, pj_str_t *str)
{
    pj_scanner scanner;
    PJ_USE_EXCEPTION;
    pj_scan_init(&scanner, cmd->ptr, cmd->slen, PJ_SCAN_AUTOSKIP_WS,
		 &on_syntax_error);
    PJ_TRY {
	while (!pj_scan_is_eof(&scanner)) {
	    pj_scan_get_until_chr(&scanner, " \t\r\n", str);
	}
    }
    PJ_CATCH_ANY {
	pj_scan_fini(&scanner);
	return PJ_GET_EXCEPTION();
    }
    PJ_END;
    return PJ_SUCCESS;
}

/*
 * This method is to process the tab character sent by client.
 */
static pj_bool_t handle_tab(cli_telnet_sess *sess)
{
    pj_status_t status;
    pj_bool_t retval = PJ_TRUE;
    unsigned len;

    pj_pool_t *pool;
    pj_cli_cmd_val *cmd_val;
    pj_cli_exec_info info;
    pool = pj_pool_create(sess->pool->factory, "handle_tab",
			  PJ_CLI_TELNET_POOL_SIZE, PJ_CLI_TELNET_POOL_INC,
			  NULL);

    cmd_val = PJ_POOL_ZALLOC_T(pool, pj_cli_cmd_val);

    status = pj_cli_sess_parse(&sess->base, (char *)&sess->rcmd->rbuf, cmd_val,
			       pool, &info);

    len = (unsigned)pj_ansi_strlen((char *)sess->rcmd->rbuf);

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
	if (len > sess->rcmd->cur_pos)
	{
	    /* Send the cursor to EOL */
	    unsigned rlen = len - sess->rcmd->cur_pos+1;
	    unsigned char *data_sent = &sess->rcmd->rbuf[sess->rcmd->cur_pos-1];
	    telnet_sess_send2(sess, data_sent, rlen);
	}
	if (info.hint_cnt > 0) {
	    /* Complete command */
	    pj_str_t cmd = pj_str((char *)sess->rcmd->rbuf);
	    pj_str_t last_token;

	    if (get_last_token(&cmd, &last_token) == PJ_SUCCESS) {
		/* Hint contains the match to the last command entered */
		pj_str_t *hint_info = &info.hint[0].name;
		pj_strtrim(&last_token);
		if (hint_info->slen >= last_token.slen) {
		    hint_info->slen -= last_token.slen;
		    pj_memmove(hint_info->ptr,
			       &hint_info->ptr[last_token.slen],
			       hint_info->slen);
		}
		send_comp_arg(sess, &info);

		pj_memcpy(&sess->rcmd->rbuf[len], info.hint[0].name.ptr,
			  info.hint[0].name.slen);

		len += (unsigned)info.hint[0].name.slen;
		sess->rcmd->rbuf[len] = 0;
	    }
	} else {
	    retval = PJ_FALSE;
	}
	break;
    }
    sess->rcmd->len = len;
    sess->rcmd->cur_pos = sess->rcmd->len;

    pj_pool_release(pool);
    return retval;
}

/*
 * This method is to process the return character sent by client.
 */
static pj_bool_t handle_return(cli_telnet_sess *sess)
{
    pj_status_t status;
    pj_bool_t retval = PJ_TRUE;

    pj_pool_t *pool;
    pj_cli_exec_info info;

    send_return_key(sess);
    insert_history(sess, (char *)&sess->rcmd->rbuf);

    pool = pj_pool_create(sess->pool->factory, "handle_return",
			  PJ_CLI_TELNET_POOL_SIZE, PJ_CLI_TELNET_POOL_INC,
			  NULL);

    status = pj_cli_sess_exec(&sess->base, (char *)&sess->rcmd->rbuf,
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
	sess->rcmd->rbuf[0] = 0;
	sess->rcmd->len = 0;
	sess->rcmd->cur_pos = sess->rcmd->len;
    }

    pj_pool_release(pool);
    return retval;
}

/*
 * This method is to process the right key character sent by client.
 */
static pj_bool_t handle_right_key(cli_telnet_sess *sess)
{
    if (recv_buf_right_len(sess->rcmd)) {
	unsigned char *data = &sess->rcmd->rbuf[sess->rcmd->cur_pos++];
	telnet_sess_send2(sess, data, 1);
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

/*
 * This method is to process the left key character sent by client.
 */
static pj_bool_t handle_left_key(cli_telnet_sess *sess)
{
    static const unsigned char move_cursor_left = 0x08;
    if (sess->rcmd->cur_pos) {
	telnet_sess_send2(sess, &move_cursor_left, 1);
	--sess->rcmd->cur_pos;
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

/*
 * This method is to process the up/down key character sent by client.
 */
static pj_bool_t handle_up_down(cli_telnet_sess *sess, pj_bool_t is_up)
{
    pj_str_t *history;

    PJ_ASSERT_RETURN(sess, PJ_FALSE);

    history = get_prev_history(sess, is_up);
    if (history) {
	pj_str_t send_data;
	char str[PJ_CLI_MAX_CMDBUF];
	enum {
	    MOVE_CURSOR_LEFT = 0x08,
	    CLEAR_CHAR = 0x20
	};
	send_data.ptr = str;
	send_data.slen = 0;

	/* Move cursor position to the beginning of line */
	if (sess->rcmd->cur_pos > 0) {
	    pj_memset(send_data.ptr, MOVE_CURSOR_LEFT, sess->rcmd->cur_pos);
	    send_data.slen = sess->rcmd->cur_pos;
	}

	if (sess->rcmd->len > (unsigned)history->slen) {
	    /* Clear the command currently shown*/
	    unsigned buf_len = sess->rcmd->len;
	    pj_memset(&send_data.ptr[send_data.slen], CLEAR_CHAR, buf_len);
	    send_data.slen += buf_len;

	    /* Move cursor position to the beginning of line */
	    pj_memset(&send_data.ptr[send_data.slen], MOVE_CURSOR_LEFT,
		      buf_len);
	    send_data.slen += buf_len;
	}
	/* Send data */
	pj_strcat(&send_data, history);
	telnet_sess_send(sess, &send_data);
	pj_ansi_strncpy((char*)&sess->rcmd->rbuf, history->ptr, history->slen);
	sess->rcmd->rbuf[history->slen] = 0;
	sess->rcmd->len = (unsigned)history->slen;
	sess->rcmd->cur_pos = sess->rcmd->len;
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

static pj_status_t process_vt100_cmd(cli_telnet_sess *sess,
				     unsigned char *cmd)
{
    pj_status_t status = PJ_TRUE;
    switch (*cmd) {
	case TC_ESC:
	    break;
	case TC_UP:
	    status = handle_up_down(sess, PJ_TRUE);
	    break;
	case TC_DOWN:
	    status = handle_up_down(sess, PJ_FALSE);
	    break;
	case TC_RIGHT:
	    status = handle_right_key(sess);
	    break;
	case TC_LEFT:
	    status = handle_left_key(sess);
	    break;
	case TC_END:
	    break;
	case TC_HOME:
	    break;
	case TC_CTRL_C:
	    break;
	case TC_CR:
	    break;
	case TC_BS:
	    break;
	case TC_TAB:
	    break;
	case TC_QM:
	    break;
	case TC_BELL:
	    break;
	case TC_DEL:
	    break;
    };
    return status;
}

PJ_DEF(void) pj_cli_telnet_cfg_default(pj_cli_telnet_cfg *param)
{
    pj_assert(param);

    pj_bzero(param, sizeof(*param));
    param->port = PJ_CLI_TELNET_PORT;
    param->log_level = PJ_CLI_TELNET_LOG_LEVEL;
}

/*
 * Send a message to a telnet session
 */
static pj_status_t telnet_sess_send(cli_telnet_sess *sess,
				    const pj_str_t *str)
{
    pj_ssize_t sz;
    pj_status_t status = PJ_SUCCESS;

    sz = str->slen;
    if (!sz)
        return PJ_SUCCESS;

    pj_mutex_lock(sess->smutex);

    if (sess->buf_len == 0)
        status = pj_activesock_send(sess->asock, &sess->op_key,
                                    str->ptr, &sz, 0);
    /* If we cannot send now, append it at the end of the buffer
     * to be sent later.
     */
    if (sess->buf_len > 0 ||
        (status != PJ_SUCCESS && status != PJ_EPENDING))
    {
        int clen = (int)sz;

        if (sess->buf_len + clen > CLI_TELNET_BUF_SIZE)
            clen = CLI_TELNET_BUF_SIZE - sess->buf_len;
        if (clen > 0)
            pj_memmove(sess->buf + sess->buf_len, str->ptr, clen);
        if (clen < sz) {
            pj_ansi_snprintf((char *)sess->buf + CLI_TELNET_BUF_SIZE,
                             MAX_CUT_MSG_LEN, CUT_MSG);
            sess->buf_len = (unsigned)(CLI_TELNET_BUF_SIZE +
                            pj_ansi_strlen((char *)sess->buf+
				            CLI_TELNET_BUF_SIZE));
        } else
            sess->buf_len += clen;
    } else if (status == PJ_SUCCESS && sz < str->slen) {
        pj_mutex_unlock(sess->smutex);
        return PJ_CLI_ETELNETLOST;
    }

    pj_mutex_unlock(sess->smutex);

    return PJ_SUCCESS;
}

/*
 * Send a message to a telnet session with formatted text
 * (add single linefeed character with carriage return)
 */
static pj_status_t telnet_sess_send_with_format(cli_telnet_sess *sess,
						const pj_str_t *str)
{
    pj_scanner scanner;
    pj_str_t out_str;
    static const pj_str_t CR_LF = {("\r\n"), 2};
    int str_len = 0;
    char *str_begin = 0;

    PJ_USE_EXCEPTION;

    pj_scan_init(&scanner, str->ptr, str->slen,
	         PJ_SCAN_AUTOSKIP_WS, &on_syntax_error);

    str_begin = scanner.begin;

    PJ_TRY {
	while (!pj_scan_is_eof(&scanner)) {
	    pj_scan_get_until_ch(&scanner, '\n', &out_str);
	    str_len = (int)(scanner.curptr - str_begin);
	    if (*scanner.curptr == '\n') {
		if ((str_len > 1) && (out_str.ptr[str_len-2] == '\r'))
		{
		    continue;
		} else {
		    int str_pos = (int)(str_begin - scanner.begin);

		    if (str_len > 0) {
			pj_str_t s;
			pj_strset(&s, &str->ptr[str_pos], str_len);
			telnet_sess_send(sess, &s);
		    }
		    telnet_sess_send(sess, &CR_LF);

		    if (!pj_scan_is_eof(&scanner)) {
			pj_scan_advance_n(&scanner, 1, PJ_TRUE);
			str_begin = scanner.curptr;
		    }
		}
	    } else {
		pj_str_t s;
		int str_pos = (int)(str_begin - scanner.begin);

		pj_strset(&s, &str->ptr[str_pos], str_len);
		telnet_sess_send(sess, &s);
	    }
	}
    }
    PJ_CATCH_ANY {
	pj_scan_fini(&scanner);
	return (PJ_GET_EXCEPTION());
    }
    PJ_END;

    return PJ_SUCCESS;
}

static pj_status_t telnet_sess_send2(cli_telnet_sess *sess,
                                     const unsigned char *str, int len)
{
    pj_str_t s;

    pj_strset(&s, (char *)str, len);
    return telnet_sess_send(sess, &s);
}

static void telnet_sess_destroy(pj_cli_sess *sess)
{
    cli_telnet_sess *tsess = (cli_telnet_sess *)sess;
    pj_mutex_t *mutex = ((cli_telnet_fe *)sess->fe)->mutex;

    pj_mutex_lock(mutex);
    pj_list_erase(sess);
    pj_mutex_unlock(mutex);

    pj_mutex_lock(tsess->smutex);
    pj_mutex_unlock(tsess->smutex);
    pj_activesock_close(tsess->asock);
    pj_mutex_destroy(tsess->smutex);
    pj_pool_release(tsess->pool);
}

static void telnet_fe_write_log(pj_cli_front_end *fe, int level,
		                const char *data, pj_size_t len)
{
    cli_telnet_fe *tfe = (cli_telnet_fe *)fe;
    pj_cli_sess *sess;

    pj_mutex_lock(tfe->mutex);

    sess = tfe->sess_head.next;
    while (sess != &tfe->sess_head) {
        cli_telnet_sess *tsess = (cli_telnet_sess *)sess;

        sess = sess->next;
	if (tsess->base.log_level > level) {
	    pj_str_t s;

	    pj_strset(&s, (char *)data, len);
	    telnet_sess_send_with_format(tsess, &s);
	}
    }

    pj_mutex_unlock(tfe->mutex);
}

static void telnet_fe_destroy(pj_cli_front_end *fe)
{
    cli_telnet_fe *tfe = (cli_telnet_fe *)fe;
    pj_cli_sess *sess;

    tfe->is_quitting = PJ_TRUE;
    if (tfe->worker_thread) {
        pj_thread_join(tfe->worker_thread);
    }

    pj_mutex_lock(tfe->mutex);

    /* Destroy all the sessions */
    sess = tfe->sess_head.next;
    while (sess != &tfe->sess_head) {
        (*sess->op->destroy)(sess);
        sess = tfe->sess_head.next;
    }

    pj_mutex_unlock(tfe->mutex);

    pj_activesock_close(tfe->asock);
    if (tfe->own_ioqueue)
        pj_ioqueue_destroy(tfe->cfg.ioqueue);

    if (tfe->worker_thread) {
	pj_thread_destroy(tfe->worker_thread);
	tfe->worker_thread = NULL;
    }

    pj_mutex_destroy(tfe->mutex);
    pj_pool_release(tfe->pool);
}

static int poll_worker_thread(void *p)
{
    cli_telnet_fe *fe = (cli_telnet_fe *)p;

    while (!fe->is_quitting) {
	pj_time_val delay = {0, 50};
        pj_ioqueue_poll(fe->cfg.ioqueue, &delay);
    }

    return 0;
}

static pj_bool_t telnet_sess_on_data_sent(pj_activesock_t *asock,
 				          pj_ioqueue_op_key_t *op_key,
				          pj_ssize_t sent)
{
    cli_telnet_sess *sess = (cli_telnet_sess *)
			    pj_activesock_get_user_data(asock);

    PJ_UNUSED_ARG(op_key);

    if (sent <= 0) {
	TRACE_((THIS_FILE, "Error On data send"));
        pj_cli_sess_end_session(&sess->base);
        return PJ_FALSE;
    }

    pj_mutex_lock(sess->smutex);

    if (sess->buf_len) {
        int len = sess->buf_len;

        sess->buf_len = 0;
        if (telnet_sess_send2(sess, sess->buf, len) != PJ_SUCCESS) {
            pj_mutex_unlock(sess->smutex);
            pj_cli_sess_end_session(&sess->base);
            return PJ_FALSE;
        }
    }

    pj_mutex_unlock(sess->smutex);

    return PJ_TRUE;
}

static pj_bool_t telnet_sess_on_data_read(pj_activesock_t *asock,
		                          void *data,
			                  pj_size_t size,
			                  pj_status_t status,
			                  pj_size_t *remainder)
{
    cli_telnet_sess *sess = (cli_telnet_sess *)
                            pj_activesock_get_user_data(asock);
    cli_telnet_fe *tfe = (cli_telnet_fe *)sess->base.fe;
    unsigned char *cdata = (unsigned char*)data;
    pj_status_t is_valid = PJ_TRUE;

    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(remainder);

    if (tfe->is_quitting)
        return PJ_FALSE;

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	TRACE_((THIS_FILE, "Error on data read %d", status));
        return PJ_FALSE;
    }

    pj_mutex_lock(sess->smutex);

    switch (sess->parse_state) {
	case ST_CR:
	    sess->parse_state = ST_NORMAL;
	    if (*cdata == 0 || *cdata == '\n')
		pj_mutex_unlock(sess->smutex);
		is_valid = handle_return(sess);
		if (!is_valid)
		    return PJ_FALSE;
		pj_mutex_lock(sess->smutex);
		break;
	case ST_NORMAL:
	    if (*cdata == IAC) {
		sess->parse_state = ST_IAC;
	    } else if (*cdata == 127) {
		is_valid = handle_backspace(sess, cdata);
	    } else if (*cdata == 27) {
		sess->parse_state = ST_ESC;
	    } else {
		if (recv_buf_insert(sess->rcmd, cdata)) {
		    if (*cdata == '\r') {
			sess->parse_state = ST_CR;
		    } else if ((*cdata == '\t') || (*cdata == '?')) {
			is_valid = handle_tab(sess);
		    } else if (*cdata > 31 && *cdata < 127) {
			is_valid = handle_alfa_num(sess, cdata);
		    }
		} else {
		    is_valid = PJ_FALSE;
		}
	    }
	    break;
	case ST_ESC:
	    if (*cdata == 91) {
		sess->parse_state = ST_VT100;
	    } else {
		sess->parse_state = ST_NORMAL;
	    }
	    break;
	case ST_VT100:
	    sess->parse_state = ST_NORMAL;
	    is_valid = process_vt100_cmd(sess, cdata);
	    break;
	case ST_IAC:
	    switch ((unsigned) *cdata) {
		case DO:
		    sess->parse_state = ST_DO;
		    break;
		case DONT:
		    sess->parse_state = ST_DONT;
		    break;
		case WILL:
		    sess->parse_state = ST_WILL;
		    break;
		case WONT:
		    sess->parse_state = ST_WONT;
		    break;
		default:
		    sess->parse_state = ST_NORMAL;
		    break;
	    }
	    break;
	case ST_DO:
	    receive_do(sess, *cdata);
	    sess->parse_state = ST_NORMAL;
	    break;
	case ST_DONT:
	    receive_dont(sess, *cdata);
	    sess->parse_state = ST_NORMAL;
	    break;
	case ST_WILL:
	    receive_will(sess, *cdata);
	    sess->parse_state = ST_NORMAL;
	    break;
	case ST_WONT:
	    receive_wont(sess, *cdata);
	    sess->parse_state = ST_NORMAL;
	    break;
	default:
	    sess->parse_state = ST_NORMAL;
	    break;
    }
    if (!is_valid) {
	send_bell(sess);
    }

    pj_mutex_unlock(sess->smutex);

    return PJ_TRUE;
}

static pj_bool_t telnet_fe_on_accept(pj_activesock_t *asock,
				     pj_sock_t newsock,
				     const pj_sockaddr_t *src_addr,
				     int src_addr_len,
				     pj_status_t status)
{
    cli_telnet_fe *fe = (cli_telnet_fe *) pj_activesock_get_user_data(asock);

    pj_status_t sstatus;
    pj_pool_t *pool;
    cli_telnet_sess *sess = NULL;
    pj_activesock_cb asock_cb;

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    if (fe->is_quitting)
        return PJ_FALSE;

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	TRACE_((THIS_FILE, "Error on data accept %d", status));
	if (status == PJ_ESOCKETSTOP)
	    telnet_restart(fe);

        return PJ_FALSE;
    }

    /* An incoming connection is accepted, create a new session */
    pool = pj_pool_create(fe->pool->factory, "telnet_sess",
                          PJ_CLI_TELNET_POOL_SIZE, PJ_CLI_TELNET_POOL_INC,
                          NULL);
    if (!pool) {
        TRACE_((THIS_FILE,
                "Not enough memory to create a new telnet session"));
        return PJ_TRUE;
    }

    sess = PJ_POOL_ZALLOC_T(pool, cli_telnet_sess);
    sess->pool = pool;
    sess->base.fe = &fe->base;
    sess->base.log_level = fe->cfg.log_level;
    sess->base.op = PJ_POOL_ZALLOC_T(pool, struct pj_cli_sess_op);
    sess->base.op->destroy = &telnet_sess_destroy;
    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = &telnet_sess_on_data_read;
    asock_cb.on_data_sent = &telnet_sess_on_data_sent;
    sess->rcmd = PJ_POOL_ZALLOC_T(pool, telnet_recv_buf);
    sess->history = PJ_POOL_ZALLOC_T(pool, struct cmd_history);
    pj_list_init(sess->history);
    sess->active_history = sess->history;

    sstatus = pj_mutex_create_recursive(pool, "mutex_telnet_sess",
                                        &sess->smutex);
    if (sstatus != PJ_SUCCESS)
        goto on_exit;

    sstatus = pj_activesock_create(pool, newsock, pj_SOCK_STREAM(),
                                   NULL, fe->cfg.ioqueue,
			           &asock_cb, sess, &sess->asock);
    if (sstatus != PJ_SUCCESS) {
        TRACE_((THIS_FILE, "Failure creating active socket"));
        goto on_exit;
    }

    pj_memset(sess->telnet_option, 0, sizeof(sess->telnet_option));
    set_local_option(sess, TRANSMIT_BINARY, PJ_TRUE);
    set_local_option(sess, STATUS, PJ_TRUE);
    set_local_option(sess, SUPPRESS_GA, PJ_TRUE);
    set_local_option(sess, TIMING_MARK, PJ_TRUE);
    set_local_option(sess, TERM_SPEED, PJ_TRUE);
    set_local_option(sess, TERM_TYPE, PJ_TRUE);

    set_peer_option(sess, TRANSMIT_BINARY, PJ_TRUE);
    set_peer_option(sess, SUPPRESS_GA, PJ_TRUE);
    set_peer_option(sess, STATUS, PJ_TRUE);
    set_peer_option(sess, TIMING_MARK, PJ_TRUE);
    set_peer_option(sess, TERM_ECHO, PJ_TRUE);

    send_cmd_do(sess, SUPPRESS_GA);
    send_cmd_will(sess, TERM_ECHO);
    send_cmd_will(sess, STATUS);
    send_cmd_will(sess, SUPPRESS_GA);

    /* Send prompt string */
    telnet_sess_send(sess, &fe->cfg.prompt_str);

    /* Start reading for input from the new telnet session */
    sstatus = pj_activesock_start_read(sess->asock, pool, 1, 0);
    if (sstatus != PJ_SUCCESS) {
        TRACE_((THIS_FILE, "Failure reading active socket"));
        goto on_exit;
    }

    pj_ioqueue_op_key_init(&sess->op_key, sizeof(sess->op_key));
    pj_mutex_lock(fe->mutex);
    pj_list_push_back(&fe->sess_head, &sess->base);
    pj_mutex_unlock(fe->mutex);

    return PJ_TRUE;

on_exit:
    if (sess->asock)
        pj_activesock_close(sess->asock);
    else
        pj_sock_close(newsock);

    if (sess->smutex)
        pj_mutex_destroy(sess->smutex);

    pj_pool_release(pool);

    return PJ_TRUE;
}

PJ_DEF(pj_status_t) pj_cli_telnet_create(pj_cli_t *cli,
					 pj_cli_telnet_cfg *param,
					 pj_cli_front_end **p_fe)
{
    cli_telnet_fe *fe;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_ASSERT_RETURN(cli, PJ_EINVAL);

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "telnet_fe",
                          PJ_CLI_TELNET_POOL_SIZE, PJ_CLI_TELNET_POOL_INC,
                          NULL);
    fe = PJ_POOL_ZALLOC_T(pool, cli_telnet_fe);
    if (!fe)
        return PJ_ENOMEM;

    fe->base.op = PJ_POOL_ZALLOC_T(pool, struct pj_cli_front_end_op);

    if (!param)
        pj_cli_telnet_cfg_default(&fe->cfg);
    else
        pj_memcpy(&fe->cfg, param, sizeof(*param));

    pj_list_init(&fe->sess_head);
    fe->base.cli = cli;
    fe->base.type = PJ_CLI_TELNET_FRONT_END;
    fe->base.op->on_write_log = &telnet_fe_write_log;
    fe->base.op->on_destroy = &telnet_fe_destroy;
    fe->pool = pool;

    if (!fe->cfg.ioqueue) {
        /* Create own ioqueue if application doesn't supply one */
        status = pj_ioqueue_create(pool, 8, &fe->cfg.ioqueue);
        if (status != PJ_SUCCESS)
            goto on_exit;
        fe->own_ioqueue = PJ_TRUE;
    }

    status = pj_mutex_create_recursive(pool, "mutex_telnet_fe", &fe->mutex);
    if (status != PJ_SUCCESS)
        goto on_exit;

    /* Start telnet daemon */
    status = telnet_start(fe);
    if (status != PJ_SUCCESS)
	goto on_exit;

    pj_cli_register_front_end(cli, &fe->base);

    if (p_fe)
        *p_fe = &fe->base;

    return PJ_SUCCESS;

on_exit:
    if (fe->own_ioqueue)
        pj_ioqueue_destroy(fe->cfg.ioqueue);

    if (fe->mutex)
        pj_mutex_destroy(fe->mutex);

    pj_pool_release(pool);
    return status;
}

static pj_status_t telnet_start(cli_telnet_fe *fe)
{
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_activesock_cb asock_cb;
    pj_sockaddr_in addr;
    pj_status_t status;
    int val;
    int restart_retry;
    unsigned msec;

    /* Start telnet daemon */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock);

    if (status != PJ_SUCCESS)
        goto on_exit;

    pj_sockaddr_in_init(&addr, NULL, fe->cfg.port);

    val = 1;
    status = pj_sock_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				&val, sizeof(val));

    if (status != PJ_SUCCESS) {
	PJ_LOG(3, (THIS_FILE, "Failed setting socket options"));
    }

    /* The loop is silly, but what else can we do? */
    for (msec=MIN_WAIT_ON_TELNET_RESTART, restart_retry=0;
	 restart_retry < MAX_RETRY_ON_TELNET_RESTART;
	 ++restart_retry, msec=(msec<MAX_WAIT_ON_TELNET_RESTART?
		          msec*2 : MAX_WAIT_ON_TELNET_RESTART))
    {
	status = pj_sock_bind(sock, &addr, sizeof(addr));
	if (status != PJ_STATUS_FROM_OS(EADDRINUSE))
	    break;
	PJ_LOG(4,(THIS_FILE, "Address is still in use, retrying.."));
	pj_thread_sleep(msec);
    }

    if (status == PJ_SUCCESS) {
	int addr_len = sizeof(addr);

	status = pj_sock_getsockname(sock, &addr, &addr_len);
	if (status != PJ_SUCCESS)
	    goto on_exit;

        fe->cfg.port = pj_sockaddr_in_get_port(&addr);

	if (fe->cfg.prompt_str.slen == 0) {
	    pj_str_t prompt_sign = {"> ", 2};
	    char *prompt_data = pj_pool_alloc(fe->pool,
					      pj_gethostname()->slen+2);
	    fe->cfg.prompt_str.ptr = prompt_data;

	    pj_strcpy(&fe->cfg.prompt_str, pj_gethostname());
	    pj_strcat(&fe->cfg.prompt_str, &prompt_sign);
	}
    } else {
        PJ_LOG(3, (THIS_FILE, "Failed binding the socket"));
        goto on_exit;
    }

    status = pj_sock_listen(sock, 4);
    if (status != PJ_SUCCESS)
        goto on_exit;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_accept_complete2 = &telnet_fe_on_accept;
    status = pj_activesock_create(fe->pool, sock, pj_SOCK_STREAM(),
                                  NULL, fe->cfg.ioqueue,
		                  &asock_cb, fe, &fe->asock);
    if (status != PJ_SUCCESS)
        goto on_exit;

    status = pj_activesock_start_accept(fe->asock, fe->pool);
    if (status != PJ_SUCCESS)
        goto on_exit;

    if (fe->own_ioqueue) {
        /* Create our own worker thread */
        status = pj_thread_create(fe->pool, "worker_telnet_fe",
                                  &poll_worker_thread, fe, 0, 0,
                                  &fe->worker_thread);
        if (status != PJ_SUCCESS)
            goto on_exit;
    }

    return PJ_SUCCESS;

on_exit:
    if (fe->cfg.on_started) {
	(*fe->cfg.on_started)(status);
    }

    if (fe->asock)
        pj_activesock_close(fe->asock);
    else if (sock != PJ_INVALID_SOCKET)
        pj_sock_close(sock);

    if (fe->own_ioqueue)
        pj_ioqueue_destroy(fe->cfg.ioqueue);

    if (fe->mutex)
        pj_mutex_destroy(fe->mutex);

    pj_pool_release(fe->pool);
    return status;
}

static pj_status_t telnet_restart(cli_telnet_fe *fe)
{
    pj_status_t status;
    pj_cli_sess *sess;

    fe->is_quitting = PJ_TRUE;
    if (fe->worker_thread) {
	pj_thread_join(fe->worker_thread);
    }

    pj_mutex_lock(fe->mutex);

    /* Destroy all the sessions */
    sess = fe->sess_head.next;
    while (sess != &fe->sess_head) {
	(*sess->op->destroy)(sess);
	sess = fe->sess_head.next;
    }

    pj_mutex_unlock(fe->mutex);

    /** Close existing activesock **/
    status = pj_activesock_close(fe->asock);
    if (status != PJ_SUCCESS)
	goto on_exit;

    if (fe->worker_thread) {
	pj_thread_destroy(fe->worker_thread);
	fe->worker_thread = NULL;
    }

    fe->is_quitting = PJ_FALSE;

    /** Start Telnet **/
    status = telnet_start(fe);
    if (status == PJ_SUCCESS) {
	if (fe->cfg.on_started) {
	    (*fe->cfg.on_started)(status);
	}
	TRACE_((THIS_FILE, "Telnet Restarted"));
    }
on_exit:
    return status;
}

PJ_DEF(pj_status_t) pj_cli_telnet_get_info(pj_cli_front_end *fe,
					   pj_cli_telnet_info *info)
{
    pj_sockaddr hostip;
    pj_status_t status;
    cli_telnet_fe *tfe = (cli_telnet_fe*) fe;

    PJ_ASSERT_RETURN(fe && (fe->type == PJ_CLI_TELNET_FRONT_END) && info,
		     PJ_EINVAL);

    pj_strset(&info->ip_address, info->buf_, 0);

    status = pj_gethostip(pj_AF_INET(), &hostip);
    if (status != PJ_SUCCESS)
	return status;

    pj_strcpy2(&info->ip_address, pj_inet_ntoa(hostip.ipv4.sin_addr));

    info->port = tfe->cfg.port;

    return PJ_SUCCESS;
}

/* $Id$ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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

#include "pjsua_app_common.h"

#define THIS_FILE	"pjsua_app_cli.c"

#define CHECK_PJSUA_RUNNING() if (pjsua_get_state()!=PJSUA_STATE_RUNNING) \
				  return PJ_EINVALIDOP

/* CLI command id */
/* level 1 command */
#define CMD_CALL		    100
#define CMD_PRESENCE		    200
#define CMD_ACCOUNT		    300
#define CMD_MEDIA		    400
#define CMD_CONFIG		    500
#define CMD_VIDEO		    600
#define CMD_SLEEP		    700
#define CMD_ECHO		    800
#define CMD_NETWORK		    900
#define CMD_QUIT		    110
#define CMD_RESTART		    120

/* call level 2 command */
#define CMD_CALL_NEW		    ((CMD_CALL*10)+1)
#define CMD_CALL_MULTI		    ((CMD_CALL*10)+2)
#define CMD_CALL_ANSWER		    ((CMD_CALL*10)+3)
#define CMD_CALL_HANGUP		    ((CMD_CALL*10)+4)
#define CMD_CALL_HANGUP_ALL	    ((CMD_CALL*10)+5)
#define CMD_CALL_HOLD		    ((CMD_CALL*10)+6)
#define CMD_CALL_REINVITE	    ((CMD_CALL*10)+7)
#define CMD_CALL_UPDATE		    ((CMD_CALL*10)+8)
#define CMD_CALL_NEXT		    ((CMD_CALL*10)+9)
#define CMD_CALL_PREVIOUS	    ((CMD_CALL*10)+10)
#define CMD_CALL_TRANSFER	    ((CMD_CALL*10)+11)
#define CMD_CALL_TRANSFER_REPLACE   ((CMD_CALL*10)+12)
#define CMD_CALL_REDIRECT	    ((CMD_CALL*10)+13)
#define CMD_CALL_D2833		    ((CMD_CALL*10)+14)
#define CMD_CALL_INFO		    ((CMD_CALL*10)+15)
#define CMD_CALL_DUMP_Q		    ((CMD_CALL*10)+16)
#define CMD_CALL_SEND_ARB	    ((CMD_CALL*10)+17)
#define CMD_CALL_LIST		    ((CMD_CALL*10)+18)

/* im & presence level 2 command */
#define CMD_PRESENCE_ADD_BUDDY	    ((CMD_PRESENCE*10)+1)
#define CMD_PRESENCE_DEL_BUDDY	    ((CMD_PRESENCE*10)+2)
#define CMD_PRESENCE_SEND_IM	    ((CMD_PRESENCE*10)+3)
#define CMD_PRESENCE_SUB	    ((CMD_PRESENCE*10)+4)
#define CMD_PRESENCE_UNSUB	    ((CMD_PRESENCE*10)+5)
#define CMD_PRESENCE_TOG_STATE	    ((CMD_PRESENCE*10)+6)
#define CMD_PRESENCE_TEXT	    ((CMD_PRESENCE*10)+7)
#define CMD_PRESENCE_LIST	    ((CMD_PRESENCE*10)+8)

/* account level 2 command */
#define CMD_ACCOUNT_ADD		    ((CMD_ACCOUNT*10)+1)
#define CMD_ACCOUNT_DEL		    ((CMD_ACCOUNT*10)+2)
#define CMD_ACCOUNT_MOD		    ((CMD_ACCOUNT*10)+3)
#define CMD_ACCOUNT_REG		    ((CMD_ACCOUNT*10)+4)
#define CMD_ACCOUNT_UNREG	    ((CMD_ACCOUNT*10)+5)
#define CMD_ACCOUNT_NEXT	    ((CMD_ACCOUNT*10)+6)
#define CMD_ACCOUNT_PREV	    ((CMD_ACCOUNT*10)+7)
#define CMD_ACCOUNT_SHOW	    ((CMD_ACCOUNT*10)+8)

/* conference & media level 2 command */
#define CMD_MEDIA_LIST		    ((CMD_MEDIA*10)+1)
#define CMD_MEDIA_CONF_CONNECT	    ((CMD_MEDIA*10)+2)
#define CMD_MEDIA_CONF_DISCONNECT   ((CMD_MEDIA*10)+3)
#define CMD_MEDIA_ADJUST_VOL	    ((CMD_MEDIA*10)+4)
#define CMD_MEDIA_CODEC_PRIO	    ((CMD_MEDIA*10)+5)
#define CMD_MEDIA_SPEAKER_TOGGLE    ((CMD_MEDIA*10)+6)

/* status & config level 2 command */
#define CMD_CONFIG_DUMP_STAT	    ((CMD_CONFIG*10)+1)
#define CMD_CONFIG_DUMP_DETAIL	    ((CMD_CONFIG*10)+2)
#define CMD_CONFIG_DUMP_CONF	    ((CMD_CONFIG*10)+3)
#define CMD_CONFIG_WRITE_SETTING    ((CMD_CONFIG*10)+4)

/* video level 2 command */
#define CMD_VIDEO_ENABLE	    ((CMD_VIDEO*10)+1)
#define CMD_VIDEO_DISABLE	    ((CMD_VIDEO*10)+2)
#define CMD_VIDEO_ACC		    ((CMD_VIDEO*10)+3)
#define CMD_VIDEO_CALL		    ((CMD_VIDEO*10)+4)
#define CMD_VIDEO_DEVICE	    ((CMD_VIDEO*10)+5)
#define CMD_VIDEO_CODEC		    ((CMD_VIDEO*10)+6)
#define CMD_VIDEO_WIN		    ((CMD_VIDEO*10)+7)

/* video level 3 command */
#define CMD_VIDEO_ACC_SHOW	    ((CMD_VIDEO_ACC*10)+1)
#define CMD_VIDEO_ACC_AUTORX	    ((CMD_VIDEO_ACC*10)+2)
#define CMD_VIDEO_ACC_AUTOTX	    ((CMD_VIDEO_ACC*10)+3)
#define CMD_VIDEO_ACC_CAP_ID	    ((CMD_VIDEO_ACC*10)+4)
#define CMD_VIDEO_ACC_REN_ID	    ((CMD_VIDEO_ACC*10)+5)
#define CMD_VIDEO_CALL_RX	    ((CMD_VIDEO_CALL*10)+1)
#define CMD_VIDEO_CALL_TX	    ((CMD_VIDEO_CALL*10)+2)
#define CMD_VIDEO_CALL_ADD	    ((CMD_VIDEO_CALL*10)+3)
#define CMD_VIDEO_CALL_ENABLE	    ((CMD_VIDEO_CALL*10)+4)
#define CMD_VIDEO_CALL_DISABLE	    ((CMD_VIDEO_CALL*10)+5)
#define CMD_VIDEO_CALL_CAP	    ((CMD_VIDEO_CALL*10)+6)
#define CMD_VIDEO_DEVICE_LIST	    ((CMD_VIDEO_DEVICE*10)+1)
#define CMD_VIDEO_DEVICE_REFRESH    ((CMD_VIDEO_DEVICE*10)+2)
#define CMD_VIDEO_DEVICE_PREVIEW    ((CMD_VIDEO_DEVICE*10)+3)
#define CMD_VIDEO_CODEC_LIST	    ((CMD_VIDEO_CODEC*10)+1)
#define CMD_VIDEO_CODEC_PRIO	    ((CMD_VIDEO_CODEC*10)+2)
#define CMD_VIDEO_CODEC_FPS	    ((CMD_VIDEO_CODEC*10)+3)
#define CMD_VIDEO_CODEC_BITRATE	    ((CMD_VIDEO_CODEC*10)+4)
#define CMD_VIDEO_CODEC_SIZE	    ((CMD_VIDEO_CODEC*10)+5)
#define CMD_VIDEO_WIN_LIST	    ((CMD_VIDEO_WIN*10)+1)
#define CMD_VIDEO_WIN_ARRANGE	    ((CMD_VIDEO_WIN*10)+2)
#define CMD_VIDEO_WIN_SHOW	    ((CMD_VIDEO_WIN*10)+3)
#define CMD_VIDEO_WIN_HIDE	    ((CMD_VIDEO_WIN*10)+4)
#define CMD_VIDEO_WIN_MOVE	    ((CMD_VIDEO_WIN*10)+5)
#define CMD_VIDEO_WIN_RESIZE	    ((CMD_VIDEO_WIN*10)+6)

/* dynamic choice argument list */
#define DYN_CHOICE_START	    9900
#define DYN_CHOICE_BUDDY_ID	    (DYN_CHOICE_START)+1
#define DYN_CHOICE_ACCOUNT_ID	    (DYN_CHOICE_START)+2
#define DYN_CHOICE_MEDIA_PORT	    (DYN_CHOICE_START)+3
#define DYN_CHOICE_AUDIO_CODEC_ID   (DYN_CHOICE_START)+4
#define DYN_CHOICE_CAP_DEV_ID	    (DYN_CHOICE_START)+5
#define DYN_CHOICE_REN_DEV_ID	    (DYN_CHOICE_START)+6
#define DYN_CHOICE_VID_DEV_ID	    (DYN_CHOICE_START)+7
#define DYN_CHOICE_STREAM_ID	    (DYN_CHOICE_START)+8
#define DYN_CHOICE_VIDEO_CODEC_ID   (DYN_CHOICE_START)+9
#define DYN_CHOICE_WIN_ID	    (DYN_CHOICE_START)+10
#define DYN_CHOICE_CALL_ID	    (DYN_CHOICE_START)+11
#define DYN_CHOICE_ADDED_BUDDY_ID   (DYN_CHOICE_START)+12

static pj_bool_t	   pj_inited = PJ_FALSE;
static pj_caching_pool	   cli_cp;
static pj_bool_t	   cli_cp_inited = PJ_FALSE;
static pj_cli_t		   *cli = NULL;
static pj_cli_sess	   *cli_cons_sess = NULL;
static pj_cli_front_end	   *telnet_front_end = NULL;

/** Forward declaration **/
pj_status_t cli_setup_command(pj_cli_t *cli);
void cli_destroy();

PJ_DEF(void) cli_get_info(char *info, pj_size_t size)
{
    pj_cli_telnet_info telnet_info;
    pj_cli_telnet_get_info(telnet_front_end, &telnet_info);

    pj_ansi_snprintf(info, size, "Telnet to %.*s:%d",
		     (int)telnet_info.ip_address.slen,
		     telnet_info.ip_address.ptr,
		     telnet_info.port);
}

static void cli_log_writer(int level, const char *buffer, int len)
{
    if (cli)
        pj_cli_write_log(cli, level, buffer, len);
}

pj_status_t cli_init()
{
    pj_status_t status;

    pj_cli_cfg *cfg = &app_config.cli_cfg.cfg;

    /* Init PJLIB */
    status = pj_init();
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_inited = PJ_TRUE;

    /* Init PJLIB-UTIL */
    status = pjlib_util_init();
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init CLI */
    pj_caching_pool_init(&cli_cp, NULL, 0);
    cli_cp_inited = PJ_TRUE;
    cfg->pf = &cli_cp.factory;
    cfg->name = pj_str("pjsua_cli");
    cfg->title = pj_str("Pjsua CLI Application");
    status = pj_cli_create(cfg, &cli);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = cli_setup_command(cli);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init telnet frontend */
    if (app_config.cli_cfg.cli_fe & CLI_FE_TELNET) {
	pj_cli_telnet_cfg *fe_cfg = &app_config.cli_cfg.telnet_cfg;
	pj_pool_t *pool;

	pool = pj_pool_create(cfg->pf, "cli_cp", 128, 128, NULL);
	pj_assert(pool);

	status = pj_cli_telnet_create(cli, fe_cfg, &telnet_front_end);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Init console frontend */
    if (app_config.cli_cfg.cli_fe & CLI_FE_CONSOLE) {
	pj_cli_console_cfg *fe_cfg = &app_config.cli_cfg.console_cfg;

	fe_cfg->quit_command = pj_str("shutdown");
	status = pj_cli_console_create(cli, fe_cfg,
				       &cli_cons_sess, NULL);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    return PJ_SUCCESS;

on_error:
    cli_destroy();
    return status;
}

pj_status_t cli_main(pj_bool_t wait_telnet_cli)
{
    char cmdline[PJ_CLI_MAX_CMDBUF];

    /* ReInit logging */
    app_config.log_cfg.cb = &cli_log_writer;
    pjsua_reconfigure_logging(&app_config.log_cfg);

    if (app_config.cli_cfg.cli_fe & CLI_FE_CONSOLE) {
	/* Main loop for CLI FE console */
	while (!pj_cli_is_quitting(cli)) {
	    pj_cli_console_process(cli_cons_sess, cmdline, sizeof(cmdline));
	}
    } else if (wait_telnet_cli) {
	/* Just wait for CLI quit */
	while (!pj_cli_is_quitting(cli)) {
	    pj_thread_sleep(200);
	}
    }

    return PJ_SUCCESS;
}

void cli_destroy()
{
    /* Destroy CLI, it will automatically destroy any FEs */
    if (cli) {
	pj_cli_destroy(cli);
	cli = NULL;
    }

    /* Destroy CLI caching pool factory */
    if (cli_cp_inited) {
	pj_caching_pool_destroy(&cli_cp);
	cli_cp_inited = PJ_FALSE;
    }

    /* Shutdown PJLIB */
    if (pj_inited) {
	pj_shutdown();
	pj_inited = PJ_FALSE;
    }
}

/* Get input URL */
static void get_input_url(char *buf,
			  pj_size_t len,
			  pj_cli_cmd_val *cval,
			  struct input_result *result)
{
    static const pj_str_t err_invalid_input = {"Invalid input\n", 15};
    result->nb_result = PJSUA_APP_NO_NB;
    result->uri_result = NULL;

    len = strlen(buf);

    /* Left trim */
    while (pj_isspace(*buf)) {
	++buf;
	--len;
    }

    /* Remove trailing newlines */
    while (len && (buf[len-1] == '\r' || buf[len-1] == '\n'))
	buf[--len] = '\0';

    if (len == 0 || buf[0]=='q')
	return;

    if (pj_isdigit(*buf) || *buf=='-') {

	unsigned i;

	if (*buf=='-')
	    i = 1;
	else
	    i = 0;

	for (; i<len; ++i) {
	    if (!pj_isdigit(buf[i])) {
		pj_cli_sess_write_msg(cval->sess, err_invalid_input.ptr,
				      (int)err_invalid_input.slen);
		return;
	    }
	}

	result->nb_result = my_atoi(buf);

	if (result->nb_result >= 0 &&
	    result->nb_result <= (int)pjsua_get_buddy_count())
	{
	    return;
	}
	if (result->nb_result == -1)
	    return;

	pj_cli_sess_write_msg(cval->sess, err_invalid_input.ptr,
			      (int)err_invalid_input.slen);
	result->nb_result = PJSUA_APP_NO_NB;
	return;

    } else {
	pj_status_t status;

	if ((status=pjsua_verify_url(buf)) != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Invalid URL", status);
	    return;
	}

	result->uri_result = buf;
    }
}

/* CLI dynamic choice handler */
/* Get buddy id */
static void get_buddy_id(pj_cli_dyn_choice_param *param)
{
    if (param->cnt < param->max_cnt) {
	pjsua_buddy_id ids[64];
	int i = 0;
	unsigned count = PJ_ARRAY_SIZE(ids);
	char data_out[64];

	pjsua_enum_buddies(ids, &count);

	if (count > 0) {
	    for (i=0; i<(int)count; ++i) {
		pjsua_buddy_info info;

		if (pjsua_buddy_get_info(ids[i], &info) != PJ_SUCCESS)
		    continue;

		/* Fill buddy id */
		pj_ansi_snprintf(data_out, sizeof(data_out), "%d", ids[i]+1);
		pj_strdup2(param->pool, &param->choice[i].value, data_out);
		pj_bzero(data_out, PJ_ARRAY_SIZE(data_out));

		/* Format & fill description */
		pj_ansi_snprintf(data_out,
				sizeof(data_out),
				"<%.*s>  %.*s",
				(int)info.status_text.slen,
				info.status_text.ptr,
				(int)info.uri.slen,
				info.uri.ptr);

		pj_strdup2(param->pool, &param->choice[i].desc, data_out);
		if (++param->cnt >= (param->max_cnt-1))
		    break;
	    }
	}
	if (param->arg_id == DYN_CHOICE_BUDDY_ID) {
	    /* Add URL input option */
	    pj_ansi_snprintf(data_out, sizeof(data_out), "URL");
	    pj_strdup2(param->pool, &param->choice[i].value, data_out);
	    pj_ansi_snprintf(data_out, sizeof(data_out), "An URL");
	    pj_strdup2(param->pool, &param->choice[i].desc, data_out);
	    ++param->cnt;
	}
    }
}

static void get_account_id(pj_cli_dyn_choice_param *param)
{
    if (param->cnt < param->max_cnt) {
	char buf[8];
	char buf_out[80];
	pjsua_acc_info info;

	pjsua_acc_id acc_ids[16];
	unsigned count = PJ_ARRAY_SIZE(acc_ids);
	int i;

	pjsua_enum_accs(acc_ids, &count);

	for (i=0; i<(int)count; ++i) {
	    pj_bzero(&buf_out[0], PJ_ARRAY_SIZE(buf_out));

	    pjsua_acc_get_info(acc_ids[i], &info);

	    pj_ansi_snprintf(buf_out,
			     sizeof(buf_out),
			     "%c%.*s",
			     (acc_ids[i]==current_acc?'*':' '),
			     (int)info.acc_uri.slen,
			     info.acc_uri.ptr);

	    pj_bzero(buf, sizeof(buf));
	    pj_ansi_snprintf(buf, sizeof(buf), "%d", acc_ids[i]);
	    pj_strdup2(param->pool, &param->choice[i].value, buf);
	    pj_strdup2(param->pool, &param->choice[i].desc, buf_out);
	    if (++param->cnt >= param->max_cnt)
		break;
	}
    }
}

static void get_media_port(pj_cli_dyn_choice_param *param)
{
    unsigned i, count;
    pjsua_conf_port_id id[PJSUA_MAX_CONF_PORTS];

    count = PJ_ARRAY_SIZE(id);
    pjsua_enum_conf_ports(id, &count);

    for (i=0; i<count; ++i) {
	char slot_id[8];
	char desc[256];
	char txlist[256];
	unsigned j;
	pjsua_conf_port_info info;

	pjsua_conf_get_port_info(id[i], &info);

	pj_ansi_snprintf(slot_id, sizeof(slot_id),
			 "%d", info.slot_id);
	pj_strdup2(param->pool, &param->choice[i].value, slot_id);

	txlist[0] = '\0';
	for (j=0; j<info.listener_cnt; ++j) {
	    char s[10];
	    pj_ansi_snprintf(s, sizeof(s), "#%d ", info.listeners[j]);
	    pj_ansi_strcat(txlist, s);
	}

	pj_ansi_snprintf(desc,
	       sizeof(desc),
	       "[%2dKHz/%dms/%d] %20.*s  transmitting to: %s",
	       info.clock_rate/1000,
	       info.samples_per_frame*1000/info.channel_count/info.clock_rate,
	       info.channel_count,
	       (int)info.name.slen,
	       info.name.ptr,
	       txlist);

	pj_strdup2(param->pool, &param->choice[i].desc, desc);
	if (++param->cnt >= param->max_cnt)
	    break;
    }
}

static void get_audio_codec_id(pj_cli_dyn_choice_param *param)
{
    if (param->cnt < param->max_cnt) {
	pjsua_codec_info c[32];
	unsigned i, count = PJ_ARRAY_SIZE(c);
	char codec_id[64];
	char desc[128];

	pjsua_enum_codecs(c, &count);
	for (i=0; i<count; ++i) {
	    pj_ansi_snprintf(codec_id, sizeof(codec_id),
			     "%.*s", (int)c[i].codec_id.slen,
			     c[i].codec_id.ptr);

	    pj_strdup2(param->pool, &param->choice[param->cnt].value, codec_id);

	    pj_ansi_snprintf(desc, sizeof(desc),
			     "Audio, prio: %d%s%.*s",
			     c[i].priority,
			     c[i].desc.slen? " - ":"",
			     (int)c[i].desc.slen,
			     c[i].desc.ptr);

	    pj_strdup2(param->pool, &param->choice[param->cnt].desc, desc);
	    if (++param->cnt >= param->max_cnt)
		break;
	}
    }
}

#if PJSUA_HAS_VIDEO
static void get_video_stream_id(pj_cli_dyn_choice_param *param)
{
    if (param->cnt < param->max_cnt) {
	pjsua_call_info call_info;

	if (current_call != PJSUA_INVALID_ID) {
	    unsigned i;
	    pjsua_call_get_info(current_call, &call_info);
	    for (i=0; i<call_info.media_cnt; ++i) {
		if (call_info.media[i].type == PJMEDIA_TYPE_VIDEO) {
		    char med_idx[8];
		    pj_ansi_snprintf(med_idx, sizeof(med_idx), "%d",
				     call_info.media[i].index);
		    pj_strdup2(param->pool, &param->choice[i].value, med_idx);

		    switch (call_info.media[i].status) {
		    case PJSUA_CALL_MEDIA_NONE:
			pj_strdup2(param->pool, &param->choice[i].desc,
				   "Status:None");
			break;
		    case PJSUA_CALL_MEDIA_ACTIVE:
			pj_strdup2(param->pool, &param->choice[i].desc,
				   "Status:Active");
			break;
		    case PJSUA_CALL_MEDIA_LOCAL_HOLD:
			pj_strdup2(param->pool, &param->choice[i].desc,
				   "Status:Local Hold");
			break;
		    case PJSUA_CALL_MEDIA_REMOTE_HOLD:
			pj_strdup2(param->pool, &param->choice[i].desc,
				   "Status:Remote Hold");
			break;
		    case PJSUA_CALL_MEDIA_ERROR:
			pj_strdup2(param->pool, &param->choice[i].desc,
				   "Status:Media Error");
			break;
		    }
		    if (++param->cnt >= param->max_cnt)
			break;
		}
	    }
	}
    }
}

static void get_video_dev_hint(pj_cli_dyn_choice_param *param,
			       pjmedia_vid_dev_info *vdi,
			       unsigned vid_dev_id)
{
    char desc[128];
    char dev_id[8];
    pj_ansi_snprintf(dev_id, sizeof(dev_id),
	"%d", vid_dev_id);
    pj_ansi_snprintf(desc, sizeof(desc), "%s [%s]",
	vdi->name, vdi->driver);

    pj_strdup2(param->pool, &param->choice[param->cnt].value,
	       dev_id);
    pj_strdup2(param->pool, &param->choice[param->cnt].desc,
	       desc);
}

static void get_video_dev_id(pj_cli_dyn_choice_param *param,
			     pj_bool_t all,
			     pj_bool_t capture)
{
    if (param->cnt < param->max_cnt) {
	unsigned i, count;
	pjmedia_vid_dev_info vdi;
	pj_status_t status;

	count = pjsua_vid_dev_count();
	if (count == 0) {
	    return;
	}

	for (i=0; i<count; ++i) {
	    status = pjsua_vid_dev_get_info(i, &vdi);
	    if (status == PJ_SUCCESS) {
		if ((all) ||
		    ((capture) && (vdi.dir == PJMEDIA_DIR_CAPTURE)) ||
		    ((!capture) && (vdi.dir == PJMEDIA_DIR_RENDER)))
		{
		    get_video_dev_hint(param, &vdi, i);
		    if (++param->cnt >= param->max_cnt)
			break;
		}
	    }
	}
    }
}

static void get_video_codec_id(pj_cli_dyn_choice_param *param)
{
    if (param->cnt < param->max_cnt) {
	pjsua_codec_info ci[32];
	unsigned i, count = PJ_ARRAY_SIZE(ci);
	char codec_id[64];
	char desc[128];

	pjsua_vid_enum_codecs(ci, &count);
	for (i=0; i<count; ++i) {
	    pjmedia_vid_codec_param cp;
	    pjmedia_video_format_detail *vfd;
	    pj_status_t status = PJ_SUCCESS;

	    status = pjsua_vid_codec_get_param(&ci[i].codec_id, &cp);
	    if (status != PJ_SUCCESS)
		continue;

	    vfd = pjmedia_format_get_video_format_detail(&cp.enc_fmt, PJ_TRUE);

	    pj_ansi_snprintf(codec_id, sizeof(codec_id),
			     "%.*s", (int)ci[i].codec_id.slen,
			     ci[i].codec_id.ptr);

	    pj_strdup2(param->pool, &param->choice[param->cnt].value, codec_id);

	    pj_ansi_snprintf(desc, sizeof(desc),
	    		     "Video, p[%d], f[%.2f], b[%d/%d], s[%dx%d]",
	    		     ci[i].priority,
			     (vfd->fps.num*1.0/vfd->fps.denum),
			     vfd->avg_bps/1000, vfd->max_bps/1000,
			     vfd->size.w, vfd->size.h);

	    pj_strdup2(param->pool, &param->choice[param->cnt].desc, desc);
	    if (++param->cnt >= param->max_cnt)
		break;
	}
    }
}

static void get_video_window_id(pj_cli_dyn_choice_param *param)
{
    if (param->cnt < param->max_cnt) {
	pjsua_vid_win_id wids[PJSUA_MAX_VID_WINS];
	unsigned i, cnt = PJ_ARRAY_SIZE(wids);
	char win_id[64];
	char desc[128];

	pjsua_vid_enum_wins(wids, &cnt);

	for (i = 0; i < cnt; ++i) {
	    pjsua_vid_win_info wi;

	    pjsua_vid_win_get_info(wids[i], &wi);
	    pj_ansi_snprintf(win_id, sizeof(win_id), "%d", wids[i]);
	    pj_strdup2(param->pool, &param->choice[i].value, win_id);

	    pj_ansi_snprintf(desc, sizeof(desc),
			     "Show:%c Pos(%d,%d)  Size(%dx%d)",
			     (wi.show?'Y':'N'), wi.pos.x, wi.pos.y,
			     wi.size.w, wi.size.h);

	    pj_strdup2(param->pool, &param->choice[i].desc, desc);
	    if (++param->cnt >= param->max_cnt)
		break;
	}
    }
}

static void get_call_id(pj_cli_dyn_choice_param *param)
{
    if (param->cnt < param->max_cnt) {
	char call_id[64];
	char desc[128];
	unsigned i, count;
	pjsua_call_id ids[PJSUA_MAX_CALLS];
	int call = current_call;

	count = PJ_ARRAY_SIZE(ids);
	pjsua_enum_calls(ids, &count);

	if (count > 1) {
	    for (i=0; i<count; ++i) {
		pjsua_call_info call_info;

		if (ids[i] == call)
		    return;

		pjsua_call_get_info(ids[i], &call_info);
		pj_ansi_snprintf(call_id, sizeof(call_id), "%d", ids[i]);
		pj_strdup2(param->pool, &param->choice[i].value, call_id);
		pj_ansi_snprintf(desc, sizeof(desc), "%.*s [%.*s]",
		    (int)call_info.remote_info.slen,
		    call_info.remote_info.ptr,
		    (int)call_info.state_text.slen,
		    call_info.state_text.ptr);
		pj_strdup2(param->pool, &param->choice[i].desc, desc);
		if (++param->cnt >= param->max_cnt)
		    break;

	    }
	}
    }
}

#endif

static void get_choice_value(pj_cli_dyn_choice_param *param)
{
    switch (param->arg_id) {
    case DYN_CHOICE_BUDDY_ID:
    case DYN_CHOICE_ADDED_BUDDY_ID:
	get_buddy_id(param);
	break;
    case DYN_CHOICE_ACCOUNT_ID:
	get_account_id(param);
	break;
    case DYN_CHOICE_MEDIA_PORT:
	get_media_port(param);
	break;
    case DYN_CHOICE_AUDIO_CODEC_ID:
	get_audio_codec_id(param);
	break;
#if PJSUA_HAS_VIDEO
    case DYN_CHOICE_CAP_DEV_ID:
    case DYN_CHOICE_REN_DEV_ID:
    case DYN_CHOICE_VID_DEV_ID:
	get_video_dev_id(param,
			 (param->arg_id==DYN_CHOICE_VID_DEV_ID),
			 (param->arg_id==DYN_CHOICE_CAP_DEV_ID));
	break;
    case DYN_CHOICE_STREAM_ID:
	get_video_stream_id(param);
	break;
    case DYN_CHOICE_VIDEO_CODEC_ID:
	get_video_codec_id(param);
	break;
    case DYN_CHOICE_WIN_ID:
	get_video_window_id(param);
	break;
    case DYN_CHOICE_CALL_ID:
	get_call_id(param);
	break;
#endif
    default:
	param->cnt = 0;
	break;
    }
}

/*
 * CLI command handler
 */

/* Add account */
static pj_status_t cmd_add_account(pj_cli_cmd_val *cval)
{
    pjsua_acc_config acc_cfg;
    pj_status_t status;

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id = cval->argv[1];
    acc_cfg.reg_uri = cval->argv[2];
    acc_cfg.cred_count = 1;
    acc_cfg.cred_info[0].scheme = pj_str("Digest");
    acc_cfg.cred_info[0].realm = cval->argv[3];
    acc_cfg.cred_info[0].username = cval->argv[4];
    acc_cfg.cred_info[0].data_type = 0;
    acc_cfg.cred_info[0].data = cval->argv[5];

    acc_cfg.rtp_cfg = app_config.rtp_cfg;
    app_config_init_video(&acc_cfg);

    status = pjsua_acc_add(&acc_cfg, PJ_TRUE, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error adding new account", status);
    }

    return status;
}

/* Delete account */
static pj_status_t cmd_del_account(pj_cli_cmd_val *cval)
{
    char out_str[64];
    unsigned str_len;

    int i = my_atoi(cval->argv[1].ptr);

    if (!pjsua_acc_is_valid(i)) {
	pj_ansi_snprintf(out_str, sizeof(out_str),
		        "Invalid account id %d\n", i);
	str_len = (unsigned)pj_ansi_strlen(out_str);
	pj_cli_sess_write_msg(cval->sess, out_str, str_len);
    } else {
	pjsua_acc_del(i);
	pj_ansi_snprintf(out_str, sizeof(out_str),
			 "Account %d deleted\n", i);
	str_len = (unsigned)pj_ansi_strlen(out_str);
	pj_cli_sess_write_msg(cval->sess, out_str, str_len);
    }
    return PJ_SUCCESS;
}

/* Modify account */
static pj_status_t cmd_mod_account(pj_cli_cmd_val *cval)
{
    PJ_UNUSED_ARG(cval);
    return PJ_SUCCESS;
}

/* Register account */
static pj_status_t cmd_reg_account()
{
    pjsua_acc_set_registration(current_acc, PJ_TRUE);
    return PJ_SUCCESS;
}

/* Unregister account */
static pj_status_t cmd_unreg_account()
{
    pjsua_acc_set_registration(current_acc, PJ_FALSE);
    return PJ_SUCCESS;
}

/* Select account to be used for sending outgoing request */
static pj_status_t cmd_next_account(pj_cli_cmd_val *cval)
{
    int i = my_atoi(cval->argv[1].ptr);
    if (pjsua_acc_is_valid(i)) {
	pjsua_acc_set_default(i);
	PJ_LOG(3,(THIS_FILE, "Current account changed to %d", i));
    } else {
	PJ_LOG(3,(THIS_FILE, "Invalid account id %d", i));
    }
    return PJ_SUCCESS;
}

/* Show account list */
static pj_status_t cmd_show_account(pj_cli_cmd_val *cval)
{
    pjsua_acc_id acc_ids[16];
    unsigned count = PJ_ARRAY_SIZE(acc_ids);
    int i;
    static const pj_str_t header = {"Account list:\n", 15};

    pjsua_enum_accs(acc_ids, &count);

    pj_cli_sess_write_msg(cval->sess, header.ptr, header.slen);
    for (i=0; i<(int)count; ++i) {
	char acc_info[80];
	char out_str[160];
	pjsua_acc_info info;

	pjsua_acc_get_info(acc_ids[i], &info);

	if (!info.has_registration) {
	    pj_ansi_snprintf(acc_info, sizeof(acc_info), "%.*s",
			     (int)info.status_text.slen,
			     info.status_text.ptr);

	} else {
	    pj_ansi_snprintf(acc_info, sizeof(acc_info),
			     "%d/%.*s (expires=%d)",
			     info.status,
			     (int)info.status_text.slen,
			     info.status_text.ptr,
			     info.expires);

	}

	pj_ansi_snprintf(out_str, sizeof(out_str),
			 " %c[%2d] %.*s: %s\n",
			 (acc_ids[i]==current_acc?'*':' '), acc_ids[i],
			 (int)info.acc_uri.slen, info.acc_uri.ptr,
			 acc_info);
	pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));

	pj_bzero(out_str, sizeof(out_str));
	pj_ansi_snprintf(out_str, sizeof(out_str),
			 "       Online status: %.*s\n",
			 (int)info.online_status_text.slen,
			 info.online_status_text.ptr);

	pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));
    }

    return PJ_SUCCESS;
}

/* Account command handler */
pj_status_t cmd_account_handler(pj_cli_cmd_val *cval)
{
    pj_status_t status = PJ_SUCCESS;

    CHECK_PJSUA_RUNNING();

    switch(pj_cli_get_cmd_id(cval->cmd)) {
    case CMD_ACCOUNT_ADD:
	status = cmd_add_account(cval);
	break;
    case CMD_ACCOUNT_DEL:
	status = cmd_del_account(cval);
	break;
    case CMD_ACCOUNT_MOD:
	status = cmd_mod_account(cval);
	break;
    case CMD_ACCOUNT_REG:
	status = cmd_reg_account();
	break;
    case CMD_ACCOUNT_UNREG:
	status = cmd_unreg_account();
	break;
    case CMD_ACCOUNT_NEXT:
    case CMD_ACCOUNT_PREV:
	status = cmd_next_account(cval);
	break;
    case CMD_ACCOUNT_SHOW:
	status = cmd_show_account(cval);
	break;
    }
    return status;
}

/* Add buddy */
static pj_status_t cmd_add_buddy(pj_cli_cmd_val *cval)
{
    char out_str[80];
    pjsua_buddy_config buddy_cfg;
    pjsua_buddy_id buddy_id;
    pj_status_t status = PJ_SUCCESS;
    cval->argv[1].ptr[cval->argv[1].slen] = 0;

    if (pjsua_verify_url(cval->argv[1].ptr) != PJ_SUCCESS) {
	pj_ansi_snprintf(out_str, sizeof(out_str),
			 "Invalid URI '%s'\n", cval->argv[1].ptr);
    } else {
	pj_bzero(&buddy_cfg, sizeof(pjsua_buddy_config));

	buddy_cfg.uri = pj_str(cval->argv[1].ptr);
	buddy_cfg.subscribe = PJ_TRUE;

	status = pjsua_buddy_add(&buddy_cfg, &buddy_id);
	if (status == PJ_SUCCESS) {
	    pj_ansi_snprintf(out_str, sizeof(out_str),
			      "New buddy '%s' added at index %d\n",
			      cval->argv[1].ptr, buddy_id+1);
	}
    }
    pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));
    return status;
}

/* Delete buddy */
static pj_status_t cmd_del_buddy(pj_cli_cmd_val *cval)
{
    int i = my_atoi(cval->argv[1].ptr) - 1;
    char out_str[80];

    if (!pjsua_buddy_is_valid(i)) {
	pj_ansi_snprintf(out_str, sizeof(out_str),
			 "Invalid buddy id %d\n", i);
    } else {
	pjsua_buddy_del(i);
	pj_ansi_snprintf(out_str, sizeof(out_str),
			 "Buddy %d deleted\n", i);
    }
    pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));
    return PJ_SUCCESS;
}

/* Send IM */
static pj_status_t cmd_send_im(pj_cli_cmd_val *cval)
{
    int i = -1;
    struct input_result result;
    char dest[64];
    pj_str_t tmp = pj_str(dest);

    /* make compiler happy. */
    char *uri = NULL;

    pj_strncpy_with_null(&tmp, &cval->argv[1], sizeof(dest));

    /* input destination. */
    get_input_url(tmp.ptr, tmp.slen, cval, &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {

	if (result.nb_result == -1) {
	    static const pj_str_t err_msg = {"you can't send broadcast im "
					     "like that!\n", 40 };
	    pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
	    return PJ_SUCCESS;
	} else if (result.nb_result == 0) {
	    i = current_call;
	} else {
	    pjsua_buddy_info binfo;
	    pjsua_buddy_get_info(result.nb_result-1, &binfo);
	    pj_strncpy_with_null(&tmp, &binfo.uri, sizeof(dest));
	    uri = tmp.ptr;
	}

    } else if (result.uri_result) {
	uri = result.uri_result;
    }

    /* send typing indication. */
    if (i != -1)
	pjsua_call_send_typing_ind(i, PJ_TRUE, NULL);
    else {
	pj_str_t tmp_uri = pj_str(uri);
	pjsua_im_typing(current_acc, &tmp_uri, PJ_TRUE, NULL);
    }

    /* send the im */
    if (i != -1)
	pjsua_call_send_im(i, NULL, &cval->argv[2], NULL, NULL);
    else {
	pj_str_t tmp_uri = pj_str(uri);
	pjsua_im_send(current_acc, &tmp_uri, NULL, &cval->argv[2], NULL, NULL);
    }
    return PJ_SUCCESS;
}

/* Subscribe/unsubscribe presence */
static pj_status_t cmd_subs_pres(pj_cli_cmd_val *cval, pj_bool_t subscribe)
{
    struct input_result result;
    char dest[64] = {0};
    pj_str_t tmp = pj_str(dest);

    pj_strncpy_with_null(&tmp, &cval->argv[1], sizeof(dest));
    get_input_url(tmp.ptr, tmp.slen, cval, &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {
	if (result.nb_result == -1) {
	    int i, count;
	    count = pjsua_get_buddy_count();
	    for (i=0; i<count; ++i)
		pjsua_buddy_subscribe_pres(i, subscribe);
	} else if (result.nb_result == 0) {
	    static const pj_str_t err_msg = {"Sorry, can only subscribe to "
					     "buddy's presence, not from "
					     "existing call\n", 71};
	    pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
	} else {
	    pjsua_buddy_subscribe_pres(result.nb_result-1, subscribe);
	}

    } else if (result.uri_result) {
	static const pj_str_t err_msg = {"Sorry, can only subscribe to "
					 "buddy's presence, not arbitrary "
					 "URL (for now)\n", 76};
	pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
    }
    return PJ_SUCCESS;
}

/* Toggle online state */
static pj_status_t cmd_toggle_state(pj_cli_cmd_val *cval)
{
    char out_str[128];
    pjsua_acc_info acc_info;

    pjsua_acc_get_info(current_acc, &acc_info);
    acc_info.online_status = !acc_info.online_status;
    pjsua_acc_set_online_status(current_acc, acc_info.online_status);
    pj_ansi_snprintf(out_str, sizeof(out_str),
		     "Setting %s online status to %s\n",
		     acc_info.acc_uri.ptr,
		    (acc_info.online_status?"online":"offline"));
    pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));
    return PJ_SUCCESS;
}

/* Set presence text */
static pj_status_t cmd_set_presence_text(pj_cli_cmd_val *cval)
{
    pjrpid_element elem;
    int choice;
    pj_bool_t online_status;

    enum {
	AVAILABLE, BUSY, OTP, IDLE, AWAY, BRB, OFFLINE, OPT_MAX
    };

    choice = pj_strtol(&cval->argv[1]) - 1;

    pj_bzero(&elem, sizeof(elem));
    elem.type = PJRPID_ELEMENT_TYPE_PERSON;

    online_status = PJ_TRUE;

    switch (choice) {
    case AVAILABLE:
	break;
    case BUSY:
	elem.activity = PJRPID_ACTIVITY_BUSY;
	elem.note = pj_str("Busy");
	break;
    case OTP:
	elem.activity = PJRPID_ACTIVITY_BUSY;
	elem.note = pj_str("On the phone");
	break;
    case IDLE:
	elem.activity = PJRPID_ACTIVITY_UNKNOWN;
	elem.note = pj_str("Idle");
	break;
    case AWAY:
	elem.activity = PJRPID_ACTIVITY_AWAY;
	elem.note = pj_str("Away");
	break;
    case BRB:
	elem.activity = PJRPID_ACTIVITY_UNKNOWN;
	elem.note = pj_str("Be right back");
	break;
    case OFFLINE:
	online_status = PJ_FALSE;
	break;
    }
    pjsua_acc_set_online_status2(current_acc, online_status, &elem);
    return PJ_SUCCESS;
}

/* Show buddy list */
static pj_status_t cmd_show_buddy(pj_cli_cmd_val *cval)
{
    pjsua_buddy_id ids[64];
    int i;
    unsigned count = PJ_ARRAY_SIZE(ids);
    static const pj_str_t header = {"Buddy list:\n", 13};
    char out_str[64];

    pj_cli_sess_write_msg(cval->sess, header.ptr, header.slen);

    pjsua_enum_buddies(ids, &count);

    if (count == 0) {
	pj_ansi_snprintf(out_str, sizeof(out_str), " -none-\n");
	pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));
    } else {
	for (i=0; i<(int)count; ++i) {
	    pjsua_buddy_info info;
	    pj_bzero(out_str, sizeof(out_str));

	    if (pjsua_buddy_get_info(ids[i], &info) != PJ_SUCCESS)
		continue;

	    pj_ansi_snprintf(out_str, sizeof(out_str),
		    " [%2d] <%.*s>  %.*s\n",
		    ids[i]+1,
		    (int)info.status_text.slen,
		    info.status_text.ptr,
		    (int)info.uri.slen,
		    info.uri.ptr);

	    pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));
	}
    }
    return PJ_SUCCESS;
}

/* Presence/buddy command handler */
pj_status_t cmd_presence_handler(pj_cli_cmd_val *cval)
{
    pj_status_t status = PJ_SUCCESS;

    CHECK_PJSUA_RUNNING();

    switch(pj_cli_get_cmd_id(cval->cmd)) {
    case CMD_PRESENCE_ADD_BUDDY:
	status = cmd_add_buddy(cval);
	break;
    case CMD_PRESENCE_DEL_BUDDY:
	status = cmd_del_buddy(cval);
	break;
    case CMD_PRESENCE_SEND_IM:
	status = cmd_send_im(cval);
	break;
    case CMD_PRESENCE_SUB:
    case CMD_PRESENCE_UNSUB:
	status = cmd_subs_pres(cval,
		               pj_cli_get_cmd_id(cval->cmd)==CMD_PRESENCE_SUB);
	break;
    case CMD_PRESENCE_TOG_STATE:
	status = cmd_toggle_state(cval);
	break;
    case CMD_PRESENCE_TEXT:
	status = cmd_set_presence_text(cval);
	break;
    case CMD_PRESENCE_LIST:
	status = cmd_show_buddy(cval);
	break;
    }

    return status;
}

/* Show conference list */
static pj_status_t cmd_media_list(pj_cli_cmd_val *cval)
{
    unsigned i, count;
    pjsua_conf_port_id id[PJSUA_MAX_CONF_PORTS];
    static const pj_str_t header = {"Conference ports:\n", 19};

    pj_cli_sess_write_msg(cval->sess, header.ptr, header.slen);

    count = PJ_ARRAY_SIZE(id);
    pjsua_enum_conf_ports(id, &count);

    for (i=0; i<count; ++i) {
	char out_str[128];
	char txlist[16];
	unsigned j;
	pjsua_conf_port_info info;

	pjsua_conf_get_port_info(id[i], &info);

	pj_bzero(txlist, sizeof(txlist));
	for (j=0; j<info.listener_cnt; ++j) {
	    char s[10];
	    pj_ansi_snprintf(s, sizeof(s), "#%d ", info.listeners[j]);
	    pj_ansi_strcat(txlist, s);
	}
	pj_ansi_snprintf(out_str,
	       sizeof(out_str),
	       "Port #%02d[%2dKHz/%dms/%d] %20.*s  transmitting to: %s\n",
	       info.slot_id,
	       info.clock_rate/1000,
	       info.samples_per_frame*1000/info.channel_count/info.clock_rate,
	       info.channel_count,
	       (int)info.name.slen,
	       info.name.ptr,
	       txlist);
	pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));
    }
    return PJ_SUCCESS;
}

/* Conference connect/disconnect */
static pj_status_t cmd_media_connect(pj_cli_cmd_val *cval, pj_bool_t connect)
{
    pj_status_t status;

    if (connect)
	status = pjsua_conf_connect(pj_strtol(&cval->argv[1]),
				    pj_strtol(&cval->argv[2]));
    else
	status = pjsua_conf_disconnect(pj_strtol(&cval->argv[1]),
				       pj_strtol(&cval->argv[2]));

    if (status == PJ_SUCCESS) {
	static const pj_str_t success_msg = {"Success\n", 9};
	pj_cli_sess_write_msg(cval->sess, success_msg.ptr, success_msg.slen);
    } else {
	static const pj_str_t err_msg = {"ERROR!!\n", 9};
	pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
    }
    return status;
}

/* Adjust audio volume */
static pj_status_t cmd_adjust_vol(pj_cli_cmd_val *cval)
{
    char buf[80];
    float orig_level;
    char *err;
    char level_val[16] = {0};
    pj_str_t tmp = pj_str(level_val);

    /* Adjust mic level */
    orig_level = app_config.mic_level;
    pj_strncpy_with_null(&tmp, &cval->argv[1], sizeof(level_val));
    app_config.mic_level = (float)strtod(level_val, &err);
    pjsua_conf_adjust_rx_level(0, app_config.mic_level);

    pj_ansi_snprintf(buf, sizeof(buf),
		     "Adjust mic level: [%4.1fx] -> [%4.1fx]\n",
		     orig_level, app_config.mic_level);

    pj_cli_sess_write_msg(cval->sess, buf, pj_ansi_strlen(buf));

    /* Adjust speaker level */
    orig_level = app_config.speaker_level;
    pj_strncpy_with_null(&tmp, &cval->argv[2], sizeof(level_val));
    app_config.speaker_level = (float)strtod(level_val, &err);
    pjsua_conf_adjust_tx_level(0, app_config.speaker_level);

    pj_ansi_snprintf(buf, sizeof(buf),
		      "Adjust speaker level: [%4.1fx] -> [%4.1fx]\n",
		      orig_level, app_config.speaker_level);

    pj_cli_sess_write_msg(cval->sess, buf, pj_ansi_strlen(buf));

    return PJ_SUCCESS;
}

/* Set codec priority */
static pj_status_t cmd_set_codec_prio(pj_cli_cmd_val *cval)
{
    int new_prio;
    pj_status_t status;

    new_prio = pj_strtol(&cval->argv[2]);
    if (new_prio < 0)
	new_prio = 0;
    else if (new_prio > PJMEDIA_CODEC_PRIO_HIGHEST)
	new_prio = PJMEDIA_CODEC_PRIO_HIGHEST;

    status = pjsua_codec_set_priority(&cval->argv[1],
				      (pj_uint8_t)new_prio);
#if PJSUA_HAS_VIDEO
    if (status != PJ_SUCCESS) {
	status = pjsua_vid_codec_set_priority(&cval->argv[1],
					      (pj_uint8_t)new_prio);
    }
#endif
    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error setting codec priority", status);

    return status;
}

/* Conference/media command handler */
pj_status_t cmd_media_handler(pj_cli_cmd_val *cval)
{
    pj_status_t status = PJ_SUCCESS;

    CHECK_PJSUA_RUNNING();

    switch(pj_cli_get_cmd_id(cval->cmd)) {
    case CMD_MEDIA_LIST:
	status = cmd_media_list(cval);
	break;
    case CMD_MEDIA_CONF_CONNECT:
    case CMD_MEDIA_CONF_DISCONNECT:
	status = cmd_media_connect(cval,
		          pj_cli_get_cmd_id(cval->cmd)==CMD_MEDIA_CONF_CONNECT);
	break;
    case CMD_MEDIA_ADJUST_VOL:
	status = cmd_adjust_vol(cval);
	break;
    case CMD_MEDIA_CODEC_PRIO:
	status = cmd_set_codec_prio(cval);
	break;
    case CMD_MEDIA_SPEAKER_TOGGLE:
	{
	    static int route = PJMEDIA_AUD_DEV_ROUTE_DEFAULT;
	    status = pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE,
					   &route);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(2, (THIS_FILE, status,
			      "Warning: unable to retrieve route setting"));
	    }

	    if (route == PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER)
		route = PJMEDIA_AUD_DEV_ROUTE_DEFAULT;
	    else
		route = PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER;

	    PJ_LOG(4,(THIS_FILE, "Setting output route to %s %s",
		      (route==PJMEDIA_AUD_DEV_ROUTE_DEFAULT?
				      "default" : "loudspeaker"),
		      (status? "anyway" : "")));

	    status = pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE,
					   &route, PJ_TRUE);
	    PJ_PERROR(4,(THIS_FILE, status, "Result"));
	}
	break;
    }

    return status;
}

/* Dump status */
static pj_status_t cmd_stat_dump(pj_bool_t detail)
{
    pjsua_dump(detail);
    return PJ_SUCCESS;
}

static pj_status_t cmd_show_config()
{
    char settings[2000];
    int len;

    len = write_settings(&app_config, settings, sizeof(settings));
    if (len < 1)
	PJ_LOG(1,(THIS_FILE, "Error: not enough buffer"));
    else
	PJ_LOG(3,(THIS_FILE,
		  "Dumping configuration (%d bytes):\n%s\n",
		  len, settings));

    return PJ_SUCCESS;
}

static pj_status_t cmd_write_config(pj_cli_cmd_val *cval)
{
    char settings[2000];
    char buf[128] = {0};
    int len;
    pj_str_t tmp = pj_str(buf);

    pj_strncpy_with_null(&tmp, &cval->argv[1], sizeof(buf));

    len = write_settings(&app_config, settings, sizeof(settings));
    if (len < 1)
	PJ_LOG(1,(THIS_FILE, "Error: not enough buffer"));
    else {
	pj_oshandle_t fd;
	pj_status_t status;

	status = pj_file_open(app_config.pool, buf, PJ_O_WRONLY, &fd);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to open file", status);
	} else {
	    char out_str[256];
	    pj_ssize_t size = len;
	    pj_file_write(fd, settings, &size);
	    pj_file_close(fd);

	    pj_ansi_snprintf(out_str, sizeof(out_str),
			     "Settings successfully written to '%s'\n", buf);

	    pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));
	}
    }

    return PJ_SUCCESS;
}

/* Status and config command handler */
pj_status_t cmd_config_handler(pj_cli_cmd_val *cval)
{
    pj_status_t status = PJ_SUCCESS;

    CHECK_PJSUA_RUNNING();

    switch(pj_cli_get_cmd_id(cval->cmd)) {
    case CMD_CONFIG_DUMP_STAT:
	status = cmd_stat_dump(PJ_FALSE);
	break;
    case CMD_CONFIG_DUMP_DETAIL:
	status = cmd_stat_dump(PJ_TRUE);
	break;
    case CMD_CONFIG_DUMP_CONF:
	status = cmd_show_config();
	break;
    case CMD_CONFIG_WRITE_SETTING:
	status = cmd_write_config(cval);
	break;
    }

    return status;
}

/* Make single call */
static pj_status_t cmd_make_single_call(pj_cli_cmd_val *cval)
{
    struct input_result result;
    char dest[64] = {0};
    char out_str[128];
    pj_str_t tmp = pj_str(dest);

    pj_strncpy_with_null(&tmp, &cval->argv[1], sizeof(dest));

    pj_ansi_snprintf(out_str,
		     sizeof(out_str),
		     "(You currently have %d calls)\n",
		     pjsua_call_get_count());

    pj_cli_sess_write_msg(cval->sess, out_str, pj_ansi_strlen(out_str));

    /* input destination. */
    get_input_url(tmp.ptr, tmp.slen, cval, &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {
	pjsua_buddy_info binfo;
	if (result.nb_result == -1 || result.nb_result == 0) {
	    static const pj_str_t err_msg =
		    {"You can't do that with make call!\n", 35};
	    pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
	    return PJ_SUCCESS;
	}
	pjsua_buddy_get_info(result.nb_result-1, &binfo);
	pj_strncpy(&tmp, &binfo.uri, sizeof(dest));
    } else if (result.uri_result) {
	tmp = pj_str(result.uri_result);
    } else {
	tmp.slen = 0;
    }

    pjsua_msg_data_init(&msg_data);
    TEST_MULTIPART(&msg_data);
    pjsua_call_make_call(current_acc, &tmp, &call_opt, NULL,
			 &msg_data, &current_call);
    return PJ_SUCCESS;
}

/* Make multi call */
static pj_status_t cmd_make_multi_call(pj_cli_cmd_val *cval)
{
    struct input_result result;
    char dest[64] = {0};
    char out_str[128];
    int i, count;
    pj_str_t tmp = pj_str(dest);

    pj_ansi_snprintf(out_str,
		     sizeof(out_str),
		     "(You currently have %d calls)\n",
		     pjsua_call_get_count());

    count = pj_strtol(&cval->argv[1]);
    if (count < 1)
	return PJ_SUCCESS;

    pj_strncpy_with_null(&tmp, &cval->argv[2], sizeof(dest));

    /* input destination. */
    get_input_url(tmp.ptr, tmp.slen, cval, &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {
	pjsua_buddy_info binfo;
	if (result.nb_result == -1 || result.nb_result == 0) {
	    static const pj_str_t err_msg =
			    {"You can't do that with make call!\n", 35};
	    pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
	    return PJ_SUCCESS;
	}
	pjsua_buddy_get_info(result.nb_result-1, &binfo);
	pj_strncpy(&tmp, &binfo.uri, sizeof(dest));
    } else {
	tmp = pj_str(result.uri_result);
    }

    for (i=0; i<count; ++i) {
	pj_status_t status;

	status = pjsua_call_make_call(current_acc, &tmp, &call_opt, NULL,
	    NULL, NULL);
	if (status != PJ_SUCCESS)
	    break;
    }
    return PJ_SUCCESS;
}

/* Answer call */
static pj_status_t cmd_answer_call(pj_cli_cmd_val *cval)
{
    pjsua_call_info call_info;

    if (current_call != PJSUA_INVALID_ID) {
	pjsua_call_get_info(current_call, &call_info);
    } else {
	/* Make compiler happy */
	call_info.role = PJSIP_ROLE_UAC;
	call_info.state = PJSIP_INV_STATE_DISCONNECTED;
    }

    if (current_call == PJSUA_INVALID_ID ||
	call_info.role != PJSIP_ROLE_UAS ||
	call_info.state >= PJSIP_INV_STATE_CONNECTING)
    {
	static const pj_str_t err_msg = {"No pending incoming call\n", 26};
	pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);

    } else {
	int st_code;
	char contact[120];
	pj_str_t hname = { "Contact", 7 };
	pj_str_t hvalue;
	pjsip_generic_string_hdr hcontact;

	st_code = pj_strtol(&cval->argv[1]);
	if ((st_code < 100) || (st_code > 699))
	    return PJ_SUCCESS;

	pjsua_msg_data_init(&msg_data);

	if (st_code/100 == 3) {
	    if (cval->argc < 3) {
		static const pj_str_t err_msg = {"Enter URL to be put "
						 "in Contact\n",  32};
		pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
		return PJ_SUCCESS;
	    }

	    hvalue = pj_str(contact);
	    pjsip_generic_string_hdr_init2(&hcontact, &hname, &hvalue);

	    pj_list_push_back(&msg_data.hdr_list, &hcontact);
	}

	/*
	* Must check again!
	* Call may have been disconnected while we're waiting for
	* keyboard input.
	*/
	if (current_call == PJSUA_INVALID_ID) {
	    static const pj_str_t err_msg = {"Call has been disconnected\n",
					     28};
	    pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
	}

	pjsua_call_answer2(current_call, &call_opt, st_code, NULL, &msg_data);
    }
    return PJ_SUCCESS;
}

/* Hangup call */
static pj_status_t cmd_hangup_call(pj_cli_cmd_val *cval, pj_bool_t all)
{
    if (current_call == PJSUA_INVALID_ID) {
	static const pj_str_t err_msg = {"No current call\n", 17};
	pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
    } else {
	if (all)
	    pjsua_call_hangup_all();
	else
	    pjsua_call_hangup(current_call, 0, NULL, NULL);
    }
    return PJ_SUCCESS;
}

/* Hold call */
static pj_status_t cmd_hold_call()
{
    if (current_call != PJSUA_INVALID_ID) {
	pjsua_call_set_hold(current_call, NULL);

    } else {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    }
    return PJ_SUCCESS;
}

/* Call reinvite */
static pj_status_t cmd_call_reinvite()
{
    if (current_call != PJSUA_INVALID_ID) {
	/*
	 * re-INVITE
	 */
	call_opt.flag |= PJSUA_CALL_UNHOLD;
	pjsua_call_reinvite2(current_call, &call_opt, NULL);

    } else {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    }
    return PJ_SUCCESS;
}

/* Send update */
static pj_status_t cmd_call_update()
{
    if (current_call != PJSUA_INVALID_ID) {
	pjsua_call_update2(current_call, &call_opt, NULL);
    } else {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    }
    return PJ_SUCCESS;
}

/* Select next call */
static pj_status_t cmd_next_call(pj_bool_t next)
{
    /*
     * Cycle next/prev dialog.
     */
    if (next) {
	find_next_call();
    } else {
	find_prev_call();
    }

    if (current_call != PJSUA_INVALID_ID) {
	pjsua_call_info call_info;

	pjsua_call_get_info(current_call, &call_info);
	PJ_LOG(3,(THIS_FILE,"Current dialog: %.*s",
		  (int)call_info.remote_info.slen,
		  call_info.remote_info.ptr));

    } else {
	PJ_LOG(3,(THIS_FILE,"No current dialog"));
    }
    return PJ_SUCCESS;
}

/* Transfer call */
static pj_status_t cmd_transfer_call(pj_cli_cmd_val *cval)
{
    if (current_call == PJSUA_INVALID_ID) {

	PJ_LOG(3,(THIS_FILE, "No current call"));

    } else {
	char out_str[64];
	int call = current_call;
	char dest[64] = {0};
	pj_str_t tmp = pj_str(dest);
	struct input_result result;
	pjsip_generic_string_hdr refer_sub;
	pj_str_t STR_REFER_SUB = { "Refer-Sub", 9 };
	pj_str_t STR_FALSE = { "false", 5 };
	pjsua_call_info ci;

	pj_strncpy_with_null(&tmp, &cval->argv[1], sizeof(dest));

	pjsua_call_get_info(current_call, &ci);
	pj_ansi_snprintf(out_str,
			 sizeof(out_str),
			 "Transferring current call [%d] %.*s\n",
			 current_call,
			 (int)ci.remote_info.slen,
			 ci.remote_info.ptr);

	get_input_url(tmp.ptr, tmp.slen, cval, &result);

	/* Check if call is still there. */

	if (call != current_call) {
	    puts("Call has been disconnected");
	    return PJ_SUCCESS;
	}

	pjsua_msg_data_init(&msg_data);
	if (app_config.no_refersub) {
	    /* Add Refer-Sub: false in outgoing REFER request */
	    pjsip_generic_string_hdr_init2(&refer_sub, &STR_REFER_SUB,
		&STR_FALSE);
	    pj_list_push_back(&msg_data.hdr_list, &refer_sub);
	}
	if (result.nb_result != PJSUA_APP_NO_NB) {
	    if (result.nb_result == -1 || result.nb_result == 0) {
		static const pj_str_t err_msg = {"You can't do that with "
						 "transfer call!\n", 39};

		pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
	    } else {
		pjsua_buddy_info binfo;
		pjsua_buddy_get_info(result.nb_result-1, &binfo);
		pjsua_call_xfer( current_call, &binfo.uri, &msg_data);
	    }
	} else if (result.uri_result) {
	    pj_str_t tmp;
	    tmp = pj_str(result.uri_result);
	    pjsua_call_xfer( current_call, &tmp, &msg_data);
	}
    }
    return PJ_SUCCESS;
}

/* Transfer call */
static pj_status_t cmd_transfer_replace_call(pj_cli_cmd_val *cval)
{
    if (current_call == -1) {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    } else {
	int call = current_call;
	int dst_call;
	pjsip_generic_string_hdr refer_sub;
	pj_str_t STR_REFER_SUB = { "Refer-Sub", 9 };
	pj_str_t STR_FALSE = { "false", 5 };
	pjsua_call_id ids[PJSUA_MAX_CALLS];
	pjsua_msg_data msg_data;
	char buf[8] = {0};
	pj_str_t tmp = pj_str(buf);
	unsigned count;
	static const pj_str_t err_invalid_num =
				    {"Invalid destination call number\n", 32 };
	count = PJ_ARRAY_SIZE(ids);
	pjsua_enum_calls(ids, &count);

	if (count <= 1) {
	    static const pj_str_t err_no_other_call =
				    {"There are no other calls\n", 25};

	    pj_cli_sess_write_msg(cval->sess, err_no_other_call.ptr,
				  err_no_other_call.slen);
	    return PJ_SUCCESS;
	}

	pj_strncpy_with_null(&tmp, &cval->argv[1], sizeof(buf));
	dst_call = my_atoi(tmp.ptr);

	/* Check if call is still there. */
	if (call != current_call) {
	    static pj_str_t err_call_dc =
				    {"Call has been disconnected\n", 27};

	    pj_cli_sess_write_msg(cval->sess, err_call_dc.ptr,
				  err_call_dc.slen);
	    return PJ_SUCCESS;
	}

	/* Check that destination call is valid. */
	if (dst_call == call) {
	    static pj_str_t err_same_num =
				    {"Destination call number must not be the "
				     "same as the call being transferred\n", 74};

	    pj_cli_sess_write_msg(cval->sess, err_same_num.ptr,
				  err_same_num.slen);
	    return PJ_SUCCESS;
	}

	if (dst_call >= PJSUA_MAX_CALLS) {
	    pj_cli_sess_write_msg(cval->sess, err_invalid_num.ptr,
				  err_invalid_num.slen);
	    return PJ_SUCCESS;
	}

	if (!pjsua_call_is_active(dst_call)) {
	    pj_cli_sess_write_msg(cval->sess, err_invalid_num.ptr,
				  err_invalid_num.slen);
	    return PJ_SUCCESS;
	}

	pjsua_msg_data_init(&msg_data);
	if (app_config.no_refersub) {
	    /* Add Refer-Sub: false in outgoing REFER request */
	    pjsip_generic_string_hdr_init2(&refer_sub, &STR_REFER_SUB,
					   &STR_FALSE);
	    pj_list_push_back(&msg_data.hdr_list, &refer_sub);
	}

	pjsua_call_xfer_replaces(call, dst_call,
				 PJSUA_XFER_NO_REQUIRE_REPLACES,
				 &msg_data);
    }
    return PJ_SUCCESS;
}

static pj_status_t cmd_redirect_call(pj_cli_cmd_val *cval)
{
    if (current_call == PJSUA_INVALID_ID) {
	PJ_LOG(3,(THIS_FILE, "No current call"));
	return PJ_SUCCESS;
    }
    if (!pjsua_call_is_active(current_call)) {
	PJ_LOG(1,(THIS_FILE, "Call %d has gone", current_call));
    } else {
	enum {
	    ACCEPT_REPLACE, ACCEPT, REJECT, STOP
	};
	int choice = pj_strtol(&cval->argv[1]);

	switch (choice) {
	case ACCEPT_REPLACE:
	    pjsua_call_process_redirect(current_call,
	    				PJSIP_REDIRECT_ACCEPT_REPLACE);
	    break;
	case ACCEPT:
	    pjsua_call_process_redirect(current_call, PJSIP_REDIRECT_ACCEPT);
	    break;
	case REJECT:
	    pjsua_call_process_redirect(current_call, PJSIP_REDIRECT_REJECT);
	    break;
	default:
	    pjsua_call_process_redirect(current_call, PJSIP_REDIRECT_STOP);
	    break;
	}
    }
    return PJ_SUCCESS;
}

/* Send DTMF (RFC2833) */
static pj_status_t cmd_dtmf_2833(pj_cli_cmd_val *cval)
{
    if (current_call == PJSUA_INVALID_ID) {

	PJ_LOG(3,(THIS_FILE, "No current call"));

    } else if (!pjsua_call_has_media(current_call)) {

	PJ_LOG(3,(THIS_FILE, "Media is not established yet!"));

    } else {
	int call = current_call;
	pj_status_t status;

	if (call != current_call) {
	    static const pj_str_t err_msg = {"Call has been disconnected\n",
					     28};
	    pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
	    return PJ_SUCCESS;;
	}

	status = pjsua_call_dial_dtmf(current_call, &cval->argv[1]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to send DTMF", status);
	} else {
	    static const pj_str_t msg = {"DTMF digits enqueued "
					 "for transmission\n", 39};
	    pj_cli_sess_write_msg(cval->sess, msg.ptr, msg.slen);
	}
    }
    return PJ_SUCCESS;
}

/* Send DTMF with SIP Info */
static pj_status_t cmd_call_info(pj_cli_cmd_val *cval)
{
    if (current_call == PJSUA_INVALID_ID) {

	PJ_LOG(3,(THIS_FILE, "No current call"));

    } else {
	const pj_str_t SIP_INFO = pj_str("INFO");
	int call = current_call;
	int i;
	pj_status_t status;

	if (call != current_call) {
	    static const pj_str_t err_msg = {"Call has been disconnected\n",
					     28};
	    pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
	    return PJ_SUCCESS;;
	}

	for (i=0; i<cval->argv[1].slen; ++i) {
	    char body[64];

	    pjsua_msg_data_init(&msg_data);
	    msg_data.content_type = pj_str("application/dtmf-relay");

	    pj_ansi_snprintf(body,
			     sizeof(body),
			     "Signal=%c\n"
			     "Duration=160",
			     cval->argv[1].ptr[i]);

	    msg_data.msg_body = pj_str(body);

	    status = pjsua_call_send_request(current_call, &SIP_INFO,
		&msg_data);
	    if (status != PJ_SUCCESS) {
		break;
	    }
	}
    }
    return PJ_SUCCESS;
}

/* Dump call quality */
static pj_status_t cmd_call_quality()
{
    if (current_call != PJSUA_INVALID_ID) {
	log_call_dump(current_call);
    } else {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    }
    return PJ_SUCCESS;
}

/* Send arbitrary request */
static pj_status_t cmd_send_arbitrary(pj_cli_cmd_val *cval)
{
    if (pjsua_acc_get_count() == 0) {
	static const pj_str_t err_msg = {"Sorry, need at least one "
					 "account configured\n", 45};
	pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
    } else {
	char *uri;
	char dest[64] = {0};
	pj_str_t tmp = pj_str(dest);
	struct input_result result;
	static const pj_str_t header = {"Send arbitrary request to "
					"remote host\n", 39};

	pj_cli_sess_write_msg(cval->sess, header.ptr, header.slen);

	pj_strncpy_with_null(&tmp, &cval->argv[2], sizeof(dest));
	/* Input destination URI */
	uri = NULL;
	get_input_url(tmp.ptr, tmp.slen, cval, &result);
	if (result.nb_result != PJSUA_APP_NO_NB) {
	    if (result.nb_result == -1) {
		static const pj_str_t err_msg = {"Sorry you can't do that!\n",
						 26};
		pj_cli_sess_write_msg(cval->sess, err_msg.ptr, err_msg.slen);
		return PJ_SUCCESS;
	    } else if (result.nb_result == 0) {
		uri = NULL;
		if (current_call == PJSUA_INVALID_ID) {
		    static const pj_str_t err_msg = {"No current call\n",
						     17};
		    pj_cli_sess_write_msg(cval->sess, err_msg.ptr,
				          err_msg.slen);

		    return PJ_SUCCESS;
		}
	    } else {
		pjsua_buddy_info binfo;
		pjsua_buddy_get_info(result.nb_result-1, &binfo);
		pj_strncpy_with_null(&tmp, &binfo.uri, sizeof(dest));
		uri = tmp.ptr;
	    }
	} else if (result.uri_result) {
	    uri = result.uri_result;
	} else {
	    return PJ_SUCCESS;;
	}

	if (uri) {
	    char method[64] = {0};
	    pj_str_t tmp_method = pj_str(method);
	    pj_strncpy_with_null(&tmp_method, &cval->argv[1], sizeof(method));
	    tmp = pj_str(uri);
	    send_request(method, &tmp);
	} else {
	    /* If you send call control request using this method
	    * (such requests includes BYE, CANCEL, etc.), it will
	    * not go well with the call state, so don't do it
	    * unless it's for testing.
	    */
	    pjsua_call_send_request(current_call, &cval->argv[1], NULL);
	}
    }
    return PJ_SUCCESS;
}

static pj_status_t cmd_show_current_call(pj_cli_cmd_val *cval)
{
    char out_str[128];
    int i = pjsua_call_get_count();
    pj_ansi_snprintf(out_str, sizeof(out_str),
		     "You have %d active call%s\n", i, (i>1?"s":""));

    pj_cli_sess_write_msg(cval->sess, out_str,
	pj_ansi_strlen(out_str));

    if (current_call != PJSUA_INVALID_ID) {
	pjsua_call_info ci;
	if (pjsua_call_get_info(current_call, &ci)==PJ_SUCCESS) {
	    pj_ansi_snprintf(out_str, sizeof(out_str),
		   "Current call id=%d to %.*s [%.*s]\n", current_call,
		   (int)ci.remote_info.slen, ci.remote_info.ptr,
		   (int)ci.state_text.slen, ci.state_text.ptr);

	    pj_cli_sess_write_msg(cval->sess, out_str,
				  pj_ansi_strlen(out_str));
	}
    }
    return PJ_SUCCESS;
}

/* Call handler */
pj_status_t cmd_call_handler(pj_cli_cmd_val *cval)
{
    pj_status_t status = PJ_SUCCESS;
    pj_cli_cmd_id cmd_id = pj_cli_get_cmd_id(cval->cmd);

    CHECK_PJSUA_RUNNING();

    switch(cmd_id) {
    case CMD_CALL_NEW:
	status = cmd_make_single_call(cval);
	break;
    case CMD_CALL_MULTI:
	status = cmd_make_multi_call(cval);
	break;
    case CMD_CALL_ANSWER:
	status = cmd_answer_call(cval);
	break;
    case CMD_CALL_HANGUP:
    case CMD_CALL_HANGUP_ALL:
	status = cmd_hangup_call(cval, (cmd_id==CMD_CALL_HANGUP_ALL));
	break;
    case CMD_CALL_HOLD:
	status = cmd_hold_call();
	break;
    case CMD_CALL_REINVITE:
	status = cmd_call_reinvite();
	break;
    case CMD_CALL_UPDATE:
	status = cmd_call_update();
	break;
    case CMD_CALL_NEXT:
    case CMD_CALL_PREVIOUS:
	status = cmd_next_call(cmd_id==CMD_CALL_NEXT);
	break;
    case CMD_CALL_TRANSFER:
	status = cmd_transfer_call(cval);
	break;
    case CMD_CALL_TRANSFER_REPLACE:
	status = cmd_transfer_replace_call(cval);
	break;
    case CMD_CALL_REDIRECT:
	status = cmd_redirect_call(cval);
	break;
    case CMD_CALL_D2833:
	status = cmd_dtmf_2833(cval);
	break;
    case CMD_CALL_INFO:
	status = cmd_call_info(cval);
	break;
    case CMD_CALL_DUMP_Q:
	status = cmd_call_quality();
	break;
    case CMD_CALL_SEND_ARB:
	status = cmd_send_arbitrary(cval);
	break;
    case CMD_CALL_LIST:
	status = cmd_show_current_call(cval);
	break;
    }

    return status;
}

#if PJSUA_HAS_VIDEO
static pj_status_t cmd_set_video_enable(pj_bool_t enabled)
{
    app_config.vid.vid_cnt = (enabled ? 1 : 0);
    PJ_LOG(3,(THIS_FILE, "Video will be %s in next offer/answer",
	(enabled?"enabled":"disabled")));

    return PJ_SUCCESS;
}

static pj_status_t modify_video_account(pjsua_acc_config *acc_cfg)
{
    pj_status_t status = pjsua_acc_modify(current_acc, acc_cfg);
    if (status != PJ_SUCCESS)
	PJ_PERROR(1,(THIS_FILE, status, "Error modifying account %d",
		     current_acc));

    return status;
}

static pj_status_t cmd_show_account_video()
{
    pjsua_acc_config acc_cfg;
    pj_pool_t *pool = pjsua_pool_create("tmp-pjsua", 1000, 1000);

    pjsua_acc_get_config(current_acc, pool, &acc_cfg);
    app_config_show_video(current_acc, &acc_cfg);
    pj_pool_release(pool);
    return PJ_SUCCESS;
}

static pj_status_t cmd_video_acc_handler(pj_cli_cmd_val *cval)
{
    pjsua_acc_config acc_cfg;
    pj_cli_cmd_id cmd_id = pj_cli_get_cmd_id(cval->cmd);
    pj_pool_t *pool = pjsua_pool_create("tmp-pjsua", 1000, 1000);

    CHECK_PJSUA_RUNNING();

    pjsua_acc_get_config(current_acc, pool, &acc_cfg);

    switch(cmd_id) {
    case CMD_VIDEO_ACC_AUTORX:
    case CMD_VIDEO_ACC_AUTOTX:
	{
	    int on = (pj_ansi_strnicmp(cval->argv[1].ptr, "On", 2)==0);

	    if (cmd_id == CMD_VIDEO_ACC_AUTORX)
		acc_cfg.vid_in_auto_show = on;
	    else
		acc_cfg.vid_out_auto_transmit = on;
	}
	break;
    case CMD_VIDEO_ACC_CAP_ID:
    case CMD_VIDEO_ACC_REN_ID:
	{
	    int dev = pj_strtol(&cval->argv[1]);

	    if (cmd_id == CMD_VIDEO_ACC_CAP_ID)
		acc_cfg.vid_cap_dev = dev;
	    else
		acc_cfg.vid_rend_dev = dev;
	}
	break;
    }
    modify_video_account(&acc_cfg);
    pj_pool_release(pool);
    return PJ_SUCCESS;
}

static pj_status_t cmd_add_vid_strm()
{
    return pjsua_call_set_vid_strm(current_call,
				   PJSUA_CALL_VID_STRM_ADD, NULL);
}

static pj_status_t cmd_enable_vid_rx(pj_cli_cmd_val *cval)
{
    pjsua_call_vid_strm_op_param param;
    pjsua_stream_info si;
    pj_status_t status = PJ_SUCCESS;
    pj_bool_t on = (pj_ansi_strnicmp(cval->argv[1].ptr, "On", 2) == 0);

    pjsua_call_vid_strm_op_param_default(&param);

    param.med_idx = pj_strtol(&cval->argv[2]);
    if (pjsua_call_get_stream_info(current_call, param.med_idx, &si) ||
	si.type != PJMEDIA_TYPE_VIDEO)
    {
	PJ_PERROR(1,(THIS_FILE, PJ_EINVAL, "Invalid stream"));
	return status;
    }

    if (on) param.dir = (si.info.vid.dir | PJMEDIA_DIR_DECODING);
    else param.dir = (si.info.vid.dir & PJMEDIA_DIR_ENCODING);

    status = pjsua_call_set_vid_strm(current_call,
				     PJSUA_CALL_VID_STRM_CHANGE_DIR,
				     &param);
    return status;
}

static pj_status_t cmd_enable_vid_tx(pj_cli_cmd_val *cval)
{
    pjsua_call_vid_strm_op_param param;
    pj_status_t status = PJ_SUCCESS;
    pj_bool_t on = (pj_ansi_strnicmp(cval->argv[1].ptr, "On", 2) == 0);

    pjsua_call_vid_strm_op op = on? PJSUA_CALL_VID_STRM_START_TRANSMIT :
				PJSUA_CALL_VID_STRM_STOP_TRANSMIT;

    pjsua_call_vid_strm_op_param_default(&param);

    param.med_idx = pj_strtol(&cval->argv[2]);

    status = pjsua_call_set_vid_strm(current_call, op, &param);
    return status;
}

static pj_status_t cmd_enable_vid_stream(pj_cli_cmd_val *cval,
					   pj_bool_t enable)
{
    pjsua_call_vid_strm_op_param param;
    pjsua_call_vid_strm_op op = enable? PJSUA_CALL_VID_STRM_CHANGE_DIR :
				PJSUA_CALL_VID_STRM_REMOVE;

    pjsua_call_vid_strm_op_param_default(&param);

    param.med_idx = cval->argc > 1 ? pj_strtol(&cval->argv[1]) : -1;
    param.dir = PJMEDIA_DIR_ENCODING_DECODING;
    return pjsua_call_set_vid_strm(current_call, op, &param);
}

static pj_status_t cmd_set_cap_dev_id(pj_cli_cmd_val *cval)
{
    pjsua_call_vid_strm_op_param param;

    pjsua_call_vid_strm_op_param_default(&param);
    param.med_idx = cval->argc > 1? pj_strtol(&cval->argv[1]) : -1;
    param.cap_dev = cval->argc > 2? pj_strtol(&cval->argv[2]) :
				    PJMEDIA_VID_DEFAULT_CAPTURE_DEV;

    return pjsua_call_set_vid_strm(current_call,
				   PJSUA_CALL_VID_STRM_CHANGE_CAP_DEV,
				   &param);
}

static pj_status_t cmd_list_vid_dev()
{
    vid_list_devs();
    return PJ_SUCCESS;
}

static pj_status_t cmd_vid_device_refresh()
{
    pjmedia_vid_dev_refresh();
    return PJ_SUCCESS;
}

static pj_status_t cmd_vid_device_preview(pj_cli_cmd_val *cval)
{
    int dev_id = pj_strtol(&cval->argv[2]);
    pj_bool_t on = (pj_ansi_strnicmp(cval->argv[1].ptr, "On", 2) == 0);

    if (on) {
	pjsua_vid_preview_param param;

	pjsua_vid_preview_param_default(&param);
	param.wnd_flags = PJMEDIA_VID_DEV_WND_BORDER |
	    PJMEDIA_VID_DEV_WND_RESIZABLE;
	pjsua_vid_preview_start(dev_id, &param);
	arrange_window(pjsua_vid_preview_get_win(dev_id));
    } else {
	pjsua_vid_win_id wid;
	wid = pjsua_vid_preview_get_win(dev_id);
	if (wid != PJSUA_INVALID_ID) {
	    /* Preview window hiding once it is stopped is
	    * responsibility of app */
	    pjsua_vid_win_set_show(wid, PJ_FALSE);
	    pjsua_vid_preview_stop(dev_id);
	}
    }
    return PJ_SUCCESS;
}

static pj_status_t cmd_vid_codec_list()
{
    pjsua_codec_info ci[PJMEDIA_CODEC_MGR_MAX_CODECS];
    unsigned count = PJ_ARRAY_SIZE(ci);
    pj_status_t status = pjsua_vid_enum_codecs(ci, &count);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status, "Error enumerating codecs"));
    } else {
	unsigned i;
	PJ_LOG(3,(THIS_FILE, "Found %d video codecs:", count));
	PJ_LOG(3,(THIS_FILE, "codec id      prio  fps    bw(kbps)   size"));
	PJ_LOG(3,(THIS_FILE, "------------------------------------------"));
	for (i=0; i<count; ++i) {
	    pjmedia_vid_codec_param cp;
	    pjmedia_video_format_detail *vfd;

	    status = pjsua_vid_codec_get_param(&ci[i].codec_id, &cp);
	    if (status != PJ_SUCCESS)
		continue;

	    vfd = pjmedia_format_get_video_format_detail(&cp.enc_fmt,
		PJ_TRUE);
	    PJ_LOG(3,(THIS_FILE, "%.*s%.*s %3d %7.2f  %4d/%4d  %dx%d",
		(int)ci[i].codec_id.slen, ci[i].codec_id.ptr,
		13-(int)ci[i].codec_id.slen, "                ",
		ci[i].priority,
		(vfd->fps.num*1.0/vfd->fps.denum),
		vfd->avg_bps/1000, vfd->max_bps/1000,
		vfd->size.w, vfd->size.h));
	}
    }
    return PJ_SUCCESS;
}

static pj_status_t cmd_set_vid_codec_prio(pj_cli_cmd_val *cval)
{
    int prio = pj_strtol(&cval->argv[2]);
    pj_status_t status;

    status = pjsua_vid_codec_set_priority(&cval->argv[1], (pj_uint8_t)prio);
    if (status != PJ_SUCCESS)
	PJ_PERROR(1,(THIS_FILE, status, "Set codec priority error"));

    return PJ_SUCCESS;
}

static pj_status_t cmd_set_vid_codec_fps(pj_cli_cmd_val *cval)
{
    pjmedia_vid_codec_param cp;
    int M, N;
    pj_status_t status;

    M = pj_strtol(&cval->argv[2]);
    N = pj_strtol(&cval->argv[3]);
    status = pjsua_vid_codec_get_param(&cval->argv[1], &cp);
    if (status == PJ_SUCCESS) {
	cp.enc_fmt.det.vid.fps.num = M;
	cp.enc_fmt.det.vid.fps.denum = N;
	status = pjsua_vid_codec_set_param(&cval->argv[1], &cp);
    }
    if (status != PJ_SUCCESS)
	PJ_PERROR(1,(THIS_FILE, status, "Set codec framerate error"));

    return PJ_SUCCESS;
}

static pj_status_t cmd_set_vid_codec_bitrate(pj_cli_cmd_val *cval)
{
    pjmedia_vid_codec_param cp;
    int M, N;
    pj_status_t status;

    M = pj_strtol(&cval->argv[2]);
    N = pj_strtol(&cval->argv[3]);
    status = pjsua_vid_codec_get_param(&cval->argv[1], &cp);
    if (status == PJ_SUCCESS) {
	cp.enc_fmt.det.vid.avg_bps = M * 1000;
	cp.enc_fmt.det.vid.max_bps = N * 1000;
	status = pjsua_vid_codec_set_param(&cval->argv[1], &cp);
    }
    if (status != PJ_SUCCESS)
	PJ_PERROR(1,(THIS_FILE, status, "Set codec bitrate error"));

    return status;
}

static pj_status_t cmd_set_vid_codec_size(pj_cli_cmd_val *cval)
{
    pjmedia_vid_codec_param cp;
    int M, N;
    pj_status_t status;

    M = pj_strtol(&cval->argv[2]);
    N = pj_strtol(&cval->argv[3]);
    status = pjsua_vid_codec_get_param(&cval->argv[1], &cp);
    if (status == PJ_SUCCESS) {
	cp.enc_fmt.det.vid.size.w = M;
	cp.enc_fmt.det.vid.size.h = N;
	status = pjsua_vid_codec_set_param(&cval->argv[1], &cp);
    }
    if (status != PJ_SUCCESS)
	PJ_PERROR(1,(THIS_FILE, status, "Set codec size error"));

    return status;
}

static pj_status_t cmd_vid_win_list()
{
    pjsua_vid_win_id wids[PJSUA_MAX_VID_WINS];
    unsigned i, cnt = PJ_ARRAY_SIZE(wids);

    pjsua_vid_enum_wins(wids, &cnt);

    PJ_LOG(3,(THIS_FILE, "Found %d video windows:", cnt));
    PJ_LOG(3,(THIS_FILE, "WID show    pos       size"));
    PJ_LOG(3,(THIS_FILE, "------------------------------"));
    for (i = 0; i < cnt; ++i) {
	pjsua_vid_win_info wi;
	pjsua_vid_win_get_info(wids[i], &wi);
	PJ_LOG(3,(THIS_FILE, "%3d   %c  (%d,%d)  %dx%d",
	    wids[i], (wi.show?'Y':'N'), wi.pos.x, wi.pos.y,
	    wi.size.w, wi.size.h));
    }
    return PJ_SUCCESS;
}

static pj_status_t cmd_arrange_vid_win()
{
    arrange_window(PJSUA_INVALID_ID);
    return PJ_SUCCESS;
}

static pj_status_t cmd_show_vid_win(pj_cli_cmd_val *cval, pj_bool_t show)
{
    pjsua_vid_win_id wid = pj_strtol(&cval->argv[1]);
    return pjsua_vid_win_set_show(wid, show);
}

static pj_status_t cmd_move_vid_win(pj_cli_cmd_val *cval)
{
    pjsua_vid_win_id wid = pj_strtol(&cval->argv[1]);
    pjmedia_coord pos;

    pos.x = pj_strtol(&cval->argv[2]);
    pos.y = pj_strtol(&cval->argv[3]);
    return pjsua_vid_win_set_pos(wid, &pos);
}

static pj_status_t cmd_resize_vid_win(pj_cli_cmd_val *cval)
{
    pjsua_vid_win_id wid = pj_strtol(&cval->argv[1]);
    pjmedia_rect_size size;

    size.w = pj_strtol(&cval->argv[2]);
    size.h = pj_strtol(&cval->argv[3]);
    return pjsua_vid_win_set_size(wid, &size);
}

/* Video handler */
static pj_status_t cmd_video_handler(pj_cli_cmd_val *cval)
{
    pj_status_t status = PJ_SUCCESS;
    pj_cli_cmd_id cmd_id = pj_cli_get_cmd_id(cval->cmd);

    CHECK_PJSUA_RUNNING();

    switch(cmd_id) {
    case CMD_VIDEO_ENABLE:
	status = cmd_set_video_enable(PJ_TRUE);
	break;
    case CMD_VIDEO_DISABLE:
	status = cmd_set_video_enable(PJ_FALSE);
	break;
    case CMD_VIDEO_ACC_SHOW:
	status = cmd_show_account_video();
	break;
    case CMD_VIDEO_ACC_AUTORX:
    case CMD_VIDEO_ACC_AUTOTX:
    case CMD_VIDEO_ACC_CAP_ID:
    case CMD_VIDEO_ACC_REN_ID:
	status = cmd_video_acc_handler(cval);
	break;
    case CMD_VIDEO_CALL_ADD:
	status = cmd_add_vid_strm();
	break;
    case CMD_VIDEO_CALL_RX:
	status = cmd_enable_vid_rx(cval);
	break;
    case CMD_VIDEO_CALL_TX:
	status = cmd_enable_vid_tx(cval);
	break;
    case CMD_VIDEO_CALL_ENABLE:
    case CMD_VIDEO_CALL_DISABLE:
	status = cmd_enable_vid_stream(cval, (cmd_id==CMD_VIDEO_CALL_ENABLE));
	break;
    case CMD_VIDEO_CALL_CAP:
	status = cmd_set_cap_dev_id(cval);
	break;
    case CMD_VIDEO_DEVICE_LIST:
	status = cmd_list_vid_dev();
	break;
    case CMD_VIDEO_DEVICE_REFRESH:
	status = cmd_vid_device_refresh();
	break;
    case CMD_VIDEO_DEVICE_PREVIEW:
	status = cmd_vid_device_preview(cval);
	break;
    case CMD_VIDEO_CODEC_LIST:
	status = cmd_vid_codec_list();
	break;
    case CMD_VIDEO_CODEC_PRIO:
	status = cmd_set_vid_codec_prio(cval);
	break;
    case CMD_VIDEO_CODEC_FPS:
	status = cmd_set_vid_codec_fps(cval);
	break;
    case CMD_VIDEO_CODEC_BITRATE:
	status = cmd_set_vid_codec_bitrate(cval);
	break;
    case CMD_VIDEO_CODEC_SIZE:
	status = cmd_set_vid_codec_size(cval);
	break;
    case CMD_VIDEO_WIN_LIST:
	status = cmd_vid_win_list();
	break;
    case CMD_VIDEO_WIN_ARRANGE:
	status = cmd_arrange_vid_win();
	break;
    case CMD_VIDEO_WIN_SHOW:
    case CMD_VIDEO_WIN_HIDE:
	status = cmd_show_vid_win(cval, (cmd_id==CMD_VIDEO_WIN_SHOW));
	break;
    case CMD_VIDEO_WIN_MOVE:
	status = cmd_move_vid_win(cval);
	break;
    case CMD_VIDEO_WIN_RESIZE:
	status = cmd_resize_vid_win(cval);
	break;
    }

    return status;
}
#endif

/* Other command handler */
static pj_status_t cmd_sleep_handler(pj_cli_cmd_val *cval)
{
    int delay;

    delay = pj_strtoul(&cval->argv[1]);
    if (delay < 0) delay = 0;
    pj_thread_sleep(delay);

    return PJ_SUCCESS;
}

static pj_status_t cmd_network_handler(pj_cli_cmd_val *cval)
{
    pj_status_t status = PJ_SUCCESS;
    PJ_UNUSED_ARG(cval);

    CHECK_PJSUA_RUNNING();

    status = pjsua_detect_nat_type();
    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error", status);

    return status;
}

static pj_status_t cmd_quit_handler(pj_cli_cmd_val *cval)
{
    PJ_LOG(3,(THIS_FILE, "Quitting app.."));
    pj_cli_quit(cval->sess->fe->cli, cval->sess, PJ_FALSE);

    /* Invoke CLI stop callback (defined in pjsua_app.c) */
    cli_on_stopped(PJ_FALSE, 0, NULL);

    return PJ_SUCCESS;
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
 * This method will parse buffer string info array of argument string
 * @argc On input, maximum array size of argument. On output, number of argument
 * parsed
 * @argv Array of argument string
 */
static pj_status_t get_options(pj_str_t *options, unsigned *argc,
			       pj_str_t argv[])
{
    pj_scanner scanner;
    unsigned max_argc = *argc;

    PJ_USE_EXCEPTION;

    if (!options)
	return PJ_SUCCESS;

    pj_scan_init(&scanner, options->ptr, options->slen, PJ_SCAN_AUTOSKIP_WS,
		 &on_syntax_error);
    PJ_TRY {
	*argc = 0;
	while (!pj_scan_is_eof(&scanner) && (max_argc > *argc)) {
	    pj_str_t str;

	    pj_scan_get_until_chr(&scanner, " \t\r\n", &str);
	    argv[*argc] = str;
	    ++(*argc);
	}
    }
    PJ_CATCH_ANY {
	pj_scan_fini(&scanner);
	return PJ_GET_EXCEPTION();
    }
    PJ_END;
    return PJ_SUCCESS;
}

static pj_status_t cmd_restart_handler(pj_cli_cmd_val *cval)
{
    enum { MAX_ARGC = 64 };
    int i;
    unsigned argc = 1;
    static char argv_buffer[PJ_CLI_MAX_CMDBUF];
    static char *argv[MAX_ARGC] = {NULL};
    char *pbuf = argv_buffer;

    PJ_LOG(3,(THIS_FILE, "Restarting app.."));
    pj_cli_quit(cval->sess->fe->cli, cval->sess, PJ_TRUE);

    /** Get the pjsua option **/
    for (i=1; i < cval->argc; i++) {
	pj_str_t argvst[MAX_ARGC];
	unsigned j, ac;

	ac = MAX_ARGC - argc;
	get_options(&cval->argv[i], &ac, argvst);
	for (j = 0; j < ac; j++) {
	    pj_ansi_strncpy(pbuf, argvst[j].ptr, argvst[j].slen);
	    pbuf[argvst[j].slen] = '\0';
	    argv[argc + j] = pbuf;
	    pbuf += argvst[j].slen + 1;
	}
	argc += ac;
    }

    /* Invoke CLI stop callback (defined in pjsua_app.c) */
    cli_on_stopped(PJ_TRUE, argc, (char**)argv);

    return PJ_SUCCESS;
}

static pj_status_t add_call_command(pj_cli_t *cli)
{
    char* call_command =
	"<CMD name='call' id='100' desc='Call related commands'>"
	"  <CMD name='new' id='1001' desc='Make a new call/INVITE'>"
	"    <ARG name='buddy_id' type='choice' id='9901' validate='0' "
	"     desc='Buddy Id'>"
	"      <CHOICE value='-1' desc='All buddies'/>"
	"      <CHOICE value='0' desc='Current dialog'/>"
	"    </ARG>"
	"  </CMD>"
	"  <CMD name='multi' id='1002' desc='Make multiple calls'>"
	"    <ARG name='number_of_calls' type='int' desc='Number of calls'/>"
	"    <ARG name='buddy_id' type='choice' id='9901' validate='0' "
	"     desc='Buddy Id'>"
	"      <CHOICE value='-1' desc='All buddies'/>"
	"      <CHOICE value='0' desc='Current dialog'/>"
	"    </ARG>"
	"  </CMD>"
	"  <CMD name='answer' id='1003' desc='Answer call'>"
	"    <ARG name='code' type='int' desc='Answer code'/>"
	"    <ARG name='new_url' type='string' optional='1' "
	"     desc='New URL(for 3xx resp)'/>"
	"  </CMD>"
	"  <CMD name='hangup' id='1004' sc='g' desc='Hangup call'/>"
	"  <CMD name='hangup_all' id='1005' sc='hA' desc='Hangup all call'/>"
	"  <CMD name='hold' id='1006' sc='H' desc='Hold call'/>"
	"  <CMD name='reinvite' id='1007' sc='v' "
	"   desc='Re-invite (release hold)'/>"
	"  <CMD name='update' id='1008' sc='U' desc='Send Update request'/>"
	"  <CMD name='next' id='1009' sc=']' desc='Select next call'/>"
	"  <CMD name='previous' id='1010' sc='[' desc='Select previous call'/>"
	"  <CMD name='transfer' id='1011' sc='x' desc='Transfer call'>"
	"    <ARG name='buddy_id' type='choice' id='9901' validate='0' "
	"     desc='Buddy Id'>"
	"      <CHOICE value='-1' desc='All buddies'/>"
	"      <CHOICE value='0' desc='Current dialog'/>"
	"    </ARG>"
	"  </CMD>"
	"  <CMD name='transfer_replaces' id='1012' sc='X' "
	"   desc='Transfer replace call'>"
	"    <ARG name='call_id' type='choice' id='9911' desc='Call Id'/>"
	"  </CMD>"
	"  <CMD name='redirect' id='1013' sc='R' desc='Redirect call'>"
	"    <ARG name='redirect_option' type='choice' desc='Redirect option'>"
	"      <CHOICE value='0' desc='Redirect accept replace'/>"
	"      <CHOICE value='1' desc='Redirect accept'/>"
	"      <CHOICE value='2' desc='Redirect reject'/>"
	"      <CHOICE value='3' desc='Redirect stop'/>"
	"    </ARG>"
	"  </CMD>"
	"  <CMD name='d_2833' id='1014' sc='#' desc='Send DTMF (RFC 2833)'>"
	"    <ARG name='dtmf_to_send' type='string' "
	"     desc='DTMF String to send'/>"
	"  </CMD>"
	"  <CMD name='d_info' id='1015' sc='*' desc='Send DTMF with SIP INFO'>"
	"    <ARG name='dtmf_to_send' type='string' "
	"     desc='DTMF String to send'/>"
	"  </CMD>"
	"  <CMD name='dump_q' id='1016' sc='dq' desc='Dump (call) quality'/>"
	"  <CMD name='send_arb' id='1017' sc='S' desc='Send arbitrary request'>"
	"    <ARG name='request_method' type='string' desc='Request method'/>"
	"    <ARG name='buddy_id' type='choice' id='9901' validate='0' "
	"     desc='Buddy Id'>"
	"      <CHOICE value='-1' desc='All buddies'/>"
	"      <CHOICE value='0' desc='Current dialog'/>"
	"    </ARG>"
	"  </CMD>"
	"  <CMD name='list' id='1018' desc='Show current call'/>"
	"</CMD>";

    pj_str_t xml = pj_str(call_command);
    return pj_cli_add_cmd_from_xml(cli, NULL,
				   &xml, cmd_call_handler,
				   NULL, get_choice_value);
}

static pj_status_t add_presence_command(pj_cli_t *cli)
{
    char* presence_command =
	"<CMD name='im' id='200' desc='IM and Presence Commands'>"
	"  <CMD name='add_b' id='2001' sc='+b' desc='Add buddy'>"
	"    <ARG name='buddy_uri' type='string' desc='Buddy URI'/>"
	"  </CMD>"
	"  <CMD name='del_b' id='2002' sc='-b' desc='Delete buddy'>"
	"    <ARG name='added_buddy_id' type='choice' id='9912' "
	"     desc='Buddy ID'/>"
	"  </CMD>"
	"  <CMD name='send_im' id='2003' sc='i' desc='Send IM'>"
	"    <ARG name='buddy_id' type='choice' id='9901' validate='0' "
	"     desc='Buddy Id'>"
	"      <CHOICE value='-1' desc='All buddies'/>"
	"      <CHOICE value='0' desc='Current dialog'/>"
	"    </ARG>"
	"    <ARG name='message_content' type='string' desc='Message Content'/>"
	"  </CMD>"
	"  <CMD name='sub_pre' id='2004' desc='Subscribe presence'>"
	"    <ARG name='buddy_id' type='choice' id='9901' validate='0' "
	"     desc='Buddy Id'>"
	"      <CHOICE value='-1' desc='All buddies'/>"
	"      <CHOICE value='0' desc='Current dialog'/>"
	"    </ARG>"
	"  </CMD>"
	"  <CMD name='unsub_pre' id='2005' desc='Unsubscribe Presence'>"
	"    <ARG name='buddy_id' type='choice' id='9901' validate='0' "
	"     desc='Buddy Id'>"
	"      <CHOICE value='-1' desc='All buddies'/>"
	"      <CHOICE value='0' desc='Current dialog'/>"
	"    </ARG>"
	"  </CMD>"
	"  <CMD name='tog_state' id='2006' desc='Toggle online state'/>"
	"  <CMD name='pre_text' id='2007' sc='T' "
	"   desc='Specify custom presence text'>"
	"    <ARG name='online_state' type='choice' desc='Online state'>"
	"      <CHOICE value='1' desc='Available'/>"
	"      <CHOICE value='2' desc='Busy'/>"
	"      <CHOICE value='3' desc='On The Phone'/>"
	"      <CHOICE value='4' desc='Idle'/>"
	"      <CHOICE value='5' desc='Away'/>"
	"      <CHOICE value='6' desc='Be Right Back'/>"
	"      <CHOICE value='7' desc='Offline'/>"
	"    </ARG>"
	"  </CMD>"
	"  <CMD name='bud_list' id='2008' sc='bl' desc='Show buddy list'/>"
	"</CMD>";

    pj_str_t xml = pj_str(presence_command);

    return pj_cli_add_cmd_from_xml(cli, NULL,
				   &xml, cmd_presence_handler,
				   NULL, get_choice_value);
}

static pj_status_t add_account_command(pj_cli_t *cli)
{
    char* account_command =
	"<CMD name='acc' id='300' desc='Account commands'>"
	"  <CMD name='add' id='3001' sc='+a' desc='Add new account'>"
	"    <ARG name='sip_url' type='string' desc='Your SIP URL'/>"
	"    <ARG name='registrar_url' type='string' "
	"     desc='URL of the registrar'/>"
	"    <ARG name='auth_realm' type='string' desc='Auth realm'/>"
	"    <ARG name='auth_username' type='string' desc='Auth username'/>"
	"    <ARG name='auth_password' type='string' desc='Auth password'/>"
	"  </CMD>"
	"  <CMD name='del' id='3002' sc='-a' desc='Delete account'>"
	"    <ARG name='account_id' type='choice' id='9902' desc='Account Id'/>"
	"  </CMD>"
	"  <CMD name='mod' id='3003' sc='!a' desc='Modify account'>"
	"    <ARG name='account_id' type='choice' id='9902' desc='Account Id'/>"
	"    <ARG name='sip_url' type='string' desc='Your SIP URL'/>"
	"    <ARG name='registrar_url' type='string' "
	"     desc='URL of the registrar'/>"
	"    <ARG name='auth_realm' type='string' desc='Auth realm'/>"
	"    <ARG name='auth_username' type='string' desc='Auth username'/>"
	"    <ARG name='auth_password' type='string' desc='Auth password'/>"
	"  </CMD>"
	"  <CMD name='reg' id='3004' sc='rr' "
	"   desc='Send (Refresh) Register request to register'/>"
	"  <CMD name='unreg' id='3005' sc='ru' "
	"   desc='Send Register request to unregister'/>"
	"  <CMD name='next' id='3006' sc='<' "
	"   desc='Select the next account for sending outgoing requests'>"
	"    <ARG name='account_id' type='choice' id='9902' desc='Account Id'/>"
	"  </CMD>"
	"  <CMD name='prev' id='3007' sc='>' "
	"   desc='Select the previous account for sending outgoing requests'>"
	"    <ARG name='account_id' type='choice' id='9902' desc='Account Id'/>"
	"  </CMD>"
	"  <CMD name='show' id='3008' sc='l' desc='Show account list'/>"
	"</CMD>";

    pj_str_t xml = pj_str(account_command);
    return pj_cli_add_cmd_from_xml(cli, NULL,
				   &xml, cmd_account_handler,
				   NULL, get_choice_value);
}

static pj_status_t add_media_command(pj_cli_t *cli)
{
    char* media_command =
	"<CMD name='audio' id='400' desc='Conference and Media commands'>"
	"  <CMD name='list' id='4001' sc='cl' desc='Show conference list'/>"
	"  <CMD name='conf_con' id='4002' sc='cc' desc='Conference connect'>"
	"    <ARG name='source_port' type='choice' id='9903' "
	"     desc='Source Port'/>"
	"    <ARG name='destination_port' type='choice' id='9903' "
	"     desc='Destination Port'/>"
	"  </CMD>"
	"  <CMD name='conf_dis' id='4003' sc='cd' desc='Conference disconnect'>"
	"    <ARG name='source_port' type='choice' id='9903' "
	"     desc='Source Port'/>"
	"    <ARG name='destination_port' type='choice' id='9903' "
	"     desc='Destination Port'/>"
	"  </CMD>"
	"  <CMD name='adjust_vol' id='4004' sc='V' desc='Adjust volume'>"
	"    <ARG name='mic_level' type='int' desc='Mic Level'/>"
	"    <ARG name='speaker_port' type='int' desc='Speaker Level'/>"
	"  </CMD>"
	"  <CMD name='speakertog' id='4006' desc='Toggle audio output route' />"
	"  <CMD name='codec_prio' id='4005' sc='Cp' "
	"   desc='Arrange codec priorities'>"
	"    <ARG name='codec_id' type='choice' id='9904' desc='Codec Id'/>"
	"    <ARG name='priority' type='int' desc='Codec Priority'/>"
	"  </CMD>"
	"</CMD>";

    pj_str_t xml = pj_str(media_command);
    return pj_cli_add_cmd_from_xml(cli, NULL,
				   &xml, cmd_media_handler,
				   NULL, get_choice_value);
}

static pj_status_t add_config_command(pj_cli_t *cli)
{
    char* config_command =
	"<CMD name='stat' id='500' desc='Status and config commands'>"
	"  <CMD name='dump_stat' id='5001' sc='ds' desc='Dump status'/>"
	"  <CMD name='dump_detail' id='5002' sc='dd' "
	"   desc='Dump detail status'/>"
	"  <CMD name='dump_conf' id='5003' sc='dc' "
	"   desc='Dump configuration to screen'/>"
	"  <CMD name='write_setting' id='5004' sc='f' "
	"   desc='Write current configuration file'>"
	"    <ARG name='output_file' type='string' desc='Output filename'/>"
	"  </CMD>"
	"</CMD>";

    pj_str_t xml = pj_str(config_command);
    return pj_cli_add_cmd_from_xml(cli, NULL,
				   &xml, cmd_config_handler,
				   NULL, get_choice_value);
}

#if PJSUA_HAS_VIDEO
static pj_status_t add_video_command(pj_cli_t *cli)
{
    char* video_command =
	"<CMD name='video' id='600' desc='Video commands'>"
	"  <CMD name='enable' id='6001' desc='Enable video'/>"
	"  <CMD name='disable' id='6002' desc='Disable video'/>"
	"  <CMD name='acc' id='6003' desc='Video setting for current account'>"
	"    <CMD name='show' id='60031' "
	"     desc='Show video setting for current account'/>"
	"    <CMD name='autorx' id='60032' sc='ar' "
	"     desc='Automatically show incoming video'>"
	"      <ARG name='enable_option' type='choice' "
	"       desc='Enable/Disable option'>"
	"        <CHOICE value='On' desc='Enable'/>"
	"        <CHOICE value='Off' desc='Disable'/>"
	"      </ARG>"
	"    </CMD>"
	"    <CMD name='autotx' id='60033' sc='at' "
	"     desc='Automatically offer video'>"
	"      <ARG name='enable_option' type='choice' "
	"       desc='Enable/Disable option'>"
	"        <CHOICE value='On' desc='Enable'/>"
	"        <CHOICE value='Off' desc='Disable'/>"
	"      </ARG>"
	"    </CMD>"
	"    <CMD name='cap_id' id='60034' sc='ci' "
	"     desc='Set default capture device for current account'>"
	"      <ARG name='cap_dev_id' type='choice' id='9905' "
	"       desc='Capture device Id'/>"
	"    </CMD>"
	"    <CMD name='ren_id' id='60035' sc='ri' "
	"     desc='Set default renderer device for current account'>"
	"      <ARG name='ren_dev_id' type='choice' id='9906' "
	"       desc='Renderer device Id'/>"
	"    </CMD>"
	"  </CMD>"
	"  <CMD name='call' id='6004' sc='vcl' "
	"   desc='Video call commands/settings'>"
	"    <CMD name='rx' id='60041' "
	"     desc='Enable/disable video RX for stream in curr call'>"
	"      <ARG name='enable_option' type='choice' "
	"       desc='Enable/Disable option'>"
	"        <CHOICE value='On' desc='Enable'/>"
	"        <CHOICE value='Off' desc='Disable'/>"
	"      </ARG>"
	"      <ARG name='stream_id' type='choice' id='9908' desc='Stream Id'/>"
	"    </CMD>"
	"    <CMD name='tx' id='60042' "
	"     desc='Enable/disable video TX for stream in curr call'>"
	"      <ARG name='enable_option' type='choice' "
	"       desc='Enable/Disable option'>"
	"        <CHOICE value='On' desc='Enable'/>"
	"        <CHOICE value='Off' desc='Disable'/>"
	"      </ARG>"
	"      <ARG name='stream_id' type='choice' id='9908' desc='Stream Id'/>"
	"    </CMD>"
	"    <CMD name='add' id='60043' "
	"     desc='Add video stream for current call'/>"
	"    <CMD name='enable' id='60044' "
	"     desc='Enable stream #N in current call'>"
	"      <ARG name='stream_id' type='choice' id='9908' optional='1' "
	"       desc='Stream Id'/>"
	"    </CMD>"
	"    <CMD name='disable' id='60045' "
	"     desc='Disable stream #N in current call'>"
	"      <ARG name='stream_id' type='choice' id='9908' optional='1' "
	"       desc='Stream Id'/>"
	"    </CMD>"
	"    <CMD name='cap' id='60046' "
	"     desc='Set capture dev ID for stream #N in current call'>"
	"      <ARG name='stream_id' type='choice' id='9908' desc='Stream Id'/>"
	"      <ARG name='cap_device_id' type='choice' id='9905' "
	"       desc='Device Id'/>"
	"    </CMD>"
	"  </CMD>"
	"  <CMD name='device' id='6005' sc='vv' desc='Video device commands'>"
	"    <CMD name='list' id='60051' desc='Show all video devices'/>"
	"    <CMD name='refresh' id='60052' desc='Refresh video device list'/>"
	"    <CMD name='prev' id='60053' "
	"     desc='Enable/disable preview for specified device ID'>"
	"      <ARG name='enable_option' type='choice' "
	"       desc='Enable/Disable option'>"
	"        <CHOICE value='On' desc='Enable'/>"
	"        <CHOICE value='Off' desc='Disable'/>"
	"      </ARG>"
	"      <ARG name='device_id' type='choice' id='9907' "
	"       desc='Video Device Id'/>"
	"    </CMD>"
	"  </CMD>"
	"  <CMD name='codec' id='6006' desc='Video codec commands'>"
	"    <CMD name='list' id='60061' desc='Show video codec list'/>"
	"    <CMD name='prio' id='60062' desc='Set video codec priority'>"
	"      <ARG name='codec_id' type='choice' id='9909' desc='Codec Id'/>"
	"      <ARG name='priority' type='int' desc='Priority'/>"
	"    </CMD>"
	"    <CMD name='fps' id='60063' desc='Set video codec framerate'>"
	"      <ARG name='codec_id' type='choice' id='9909' desc='Codec Id'/>"
	"      <ARG name='num' type='int' desc='Numerator'/>"
	"      <ARG name='denum' type='int' desc='Denumerator'/>"
	"    </CMD>"
	"    <CMD name='bitrate' id='60064' desc='Set video codec bitrate'>"
	"      <ARG name='codec_id' type='choice' id='9909' desc='Codec Id'/>"
	"      <ARG name='avg' type='int' desc='Average bps'/>"
	"      <ARG name='max' type='int' desc='Maximum bps'/>"
	"    </CMD>"
	"    <CMD name='size' id='60065' desc='Set codec ID size/resolution'>"
	"      <ARG name='codec_id' type='choice' id='9909' desc='Codec Id'/>"
	"      <ARG name='width' type='int' desc='Width'/>"
	"      <ARG name='height' type='int' desc='Height'/>"
	"    </CMD>"
	"  </CMD>"
	"  <CMD name='win' id='6007' desc='Video windows settings/commands'>"
	"    <CMD name='list' id='60071' desc='List all active video windows'/>"
	"    <CMD name='arrange' id='60072' desc='Auto arrange windows'/>"
	"    <CMD name='show' id='60073' desc='Show specific windows'>"
	"      <ARG name='window_id' type='choice' id='9910' "
	"       desc='Windows Id'/>"
	"    </CMD>"
	"    <CMD name='hide' id='60074' desc='Hide specific windows'>"
	"      <ARG name='window_id' type='choice' id='9910' "
	"       desc='Windows Id'/>"
	"    </CMD>"
	"    <CMD name='move' id='60075' desc='Move window position'>"
	"      <ARG name='window_id' type='choice' id='9910' "
	"       desc='Windows Id'/>"
	"      <ARG name='x' type='int' desc='Horizontal position'/>"
	"      <ARG name='y' type='int' desc='Vertical position'/>"
	"    </CMD>"
	"    <CMD name='resize' id='60076' "
	"     desc='Resize window to specific width/height'>"
	"      <ARG name='window_id' type='choice' id='9910' "
	"       desc='Windows Id'/>"
	"      <ARG name='width' type='int' desc='Width'/>"
	"      <ARG name='height' type='int' desc='Height'/>"
	"    </CMD>"
	"  </CMD>"
	"</CMD>";

    pj_str_t xml = pj_str(video_command);
    return pj_cli_add_cmd_from_xml(cli, NULL,
				   &xml, cmd_video_handler,
				   NULL, get_choice_value);
}
#endif

static pj_status_t add_other_command(pj_cli_t *cli)
{
    char* sleep_command =
	"<CMD name='sleep' id='700' desc='Suspend keyboard input'>"
	"  <ARG name='msec' type='int' desc='Millisecond'/>"
	"</CMD>";

    char* network_command =
	"<CMD name='network' id='900' desc='Detect network type'/>";

    char* shutdown_command =
	"<CMD name='shutdown' id='110' desc='Shutdown application'/>";

    char* restart_command =
	"<CMD name='restart' id='120' desc='Restart application'>"
	"  <ARG name='options1' type='string' desc='Options' optional='1'/>"
	"  <ARG name='options2' type='string' desc='Options' optional='1'/>"
	"  <ARG name='options3' type='string' desc='Options' optional='1'/>"
	"  <ARG name='options4' type='string' desc='Options' optional='1'/>"
	"</CMD>";

    pj_status_t status;
    pj_str_t sleep_xml = pj_str(sleep_command);
    pj_str_t network_xml = pj_str(network_command);
    pj_str_t shutdown_xml = pj_str(shutdown_command);
    pj_str_t restart_xml = pj_str(restart_command);

    status = pj_cli_add_cmd_from_xml(cli, NULL,
				     &sleep_xml, cmd_sleep_handler,
				     NULL, NULL);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_cli_add_cmd_from_xml(cli, NULL,
				     &network_xml, cmd_network_handler,
				     NULL, NULL);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_cli_add_cmd_from_xml(cli, NULL,
				     &shutdown_xml, cmd_quit_handler,
				     NULL, NULL);

    if (status != PJ_SUCCESS)
	return status;

    status = pj_cli_add_cmd_from_xml(cli, NULL,
				     &restart_xml, cmd_restart_handler,
				     NULL, NULL);

    return status;
}

pj_status_t cli_setup_command(pj_cli_t *cli)
{
    pj_status_t status;

    status = add_call_command(cli);
    if (status != PJ_SUCCESS)
	return status;

    status = add_presence_command(cli);
    if (status != PJ_SUCCESS)
	return status;

    status = add_account_command(cli);
    if (status != PJ_SUCCESS)
	return status;

    status = add_media_command(cli);
    if (status != PJ_SUCCESS)
	return status;

    status = add_config_command(cli);
    if (status != PJ_SUCCESS)
	return status;

#if PJSUA_HAS_VIDEO
    status = add_video_command(cli);
    if (status != PJ_SUCCESS)
	return status;
#endif

    status = add_other_command(cli);

    return status;
}

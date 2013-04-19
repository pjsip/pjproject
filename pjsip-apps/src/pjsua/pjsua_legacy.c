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

#include <pjsua-lib/pjsua.h>
#include "pjsua_common.h"

#define THIS_FILE	"pjsua_legacy.c"

static pj_bool_t	cmd_echo;

/*
 * Print buddy list.
 */
static void print_buddy_list()
{
    pjsua_buddy_id ids[64];
    int i;
    unsigned count = PJ_ARRAY_SIZE(ids);

    puts("Buddy list:");

    pjsua_enum_buddies(ids, &count);

    if (count == 0)
	puts(" -none-");
    else {
	for (i=0; i<(int)count; ++i) {
	    pjsua_buddy_info info;

	    if (pjsua_buddy_get_info(ids[i], &info) != PJ_SUCCESS)
		continue;

	    printf(" [%2d] <%.*s>  %.*s\n", 
		    ids[i]+1, 
		    (int)info.status_text.slen,
		    info.status_text.ptr, 
		    (int)info.uri.slen,
		    info.uri.ptr);
	}
    }
    puts("");
}

/*
 * Input URL.
 */
static void ui_input_url(const char *title, char *buf, int len, 
			 input_result *result)
{
    result->nb_result = PJSUA_APP_NO_NB;
    result->uri_result = NULL;

    print_buddy_list();

    printf("Choices:\n"
	   "   0         For current dialog.\n"
	   "  -1         All %d buddies in buddy list\n"
	   "  [1 -%2d]    Select from buddy list\n"
	   "  URL        An URL\n"
	   "  <Enter>    Empty input (or 'q') to cancel\n"
	   , pjsua_get_buddy_count(), pjsua_get_buddy_count());
    printf("%s: ", title);

    fflush(stdout);
    if (fgets(buf, len, stdin) == NULL)
	return;
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
	
	int i;
	
	if (*buf=='-')
	    i = 1;
	else
	    i = 0;

	for (; i<len; ++i) {
	    if (!pj_isdigit(buf[i])) {
		puts("Invalid input");
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

	puts("Invalid input");
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

static pj_bool_t simple_input(const char *title, char *buf, pj_size_t len)
{
    char *p;

    printf("%s (empty to cancel): ", title); fflush(stdout);
    if (fgets(buf, len, stdin) == NULL)
	return PJ_FALSE;

    /* Remove trailing newlines. */
    for (p=buf; ; ++p) {
	if (*p=='\r' || *p=='\n') *p='\0';
	else if (!*p) break;
    }

    if (!*buf)
	return PJ_FALSE;
    
    return PJ_TRUE;
}

/*
 * Print account status.
 */
static void print_acc_status(int acc_id)
{
    char buf[80];
    pjsua_acc_info info;

    pjsua_acc_get_info(acc_id, &info);

    if (!info.has_registration) {
	pj_ansi_snprintf(buf, sizeof(buf), "%.*s", 
			 (int)info.status_text.slen,
			 info.status_text.ptr);

    } else {
	pj_ansi_snprintf(buf, sizeof(buf),
			 "%d/%.*s (expires=%d)",
			 info.status,
			 (int)info.status_text.slen,
			 info.status_text.ptr,
			 info.expires);

    }

    printf(" %c[%2d] %.*s: %s\n", (acc_id==current_acc?'*':' '),
	   acc_id,  (int)info.acc_uri.slen, info.acc_uri.ptr, buf);
    printf("       Online status: %.*s\n", 
	(int)info.online_status_text.slen,
	info.online_status_text.ptr);
}

/*
 * Show a bit of help.
 */
static void keystroke_help()
{
    pjsua_acc_id acc_ids[16];
    unsigned count = PJ_ARRAY_SIZE(acc_ids);
    int i;

    printf(">>>>\n");

    pjsua_enum_accs(acc_ids, &count);

    printf("Account list:\n");
    for (i=0; i<(int)count; ++i)
	print_acc_status(acc_ids[i]);

    print_buddy_list();
    
    //puts("Commands:");
    puts("+=============================================================================+");
    puts("|       Call Commands:         |   Buddy, IM & Presence:  |     Account:      |");
    puts("|                              |                          |                   |");
    puts("|  m  Make new call            | +b  Add new buddy       .| +a  Add new accnt |");
    puts("|  M  Make multiple calls      | -b  Delete buddy         | -a  Delete accnt. |");
    puts("|  a  Answer call              |  i  Send IM              | !a  Modify accnt. |");
    puts("|  h  Hangup call  (ha=all)    |  s  Subscribe presence   | rr  (Re-)register |");
    puts("|  H  Hold call                |  u  Unsubscribe presence | ru  Unregister    |");
    puts("|  v  re-inVite (release hold) |  t  ToGgle Online status |  >  Cycle next ac.|");
    puts("|  U  send UPDATE              |  T  Set online status    |  <  Cycle prev ac.|");
    puts("| ],[ Select next/prev call    +--------------------------+-------------------+");
    puts("|  x  Xfer call                |      Media Commands:     |  Status & Config: |");
    puts("|  X  Xfer with Replaces       |                          |                   |");
    puts("|  #  Send RFC 2833 DTMF       | cl  List ports           |  d  Dump status   |");
    puts("|  *  Send DTMF with INFO      | cc  Connect port         | dd  Dump detailed |");
    puts("| dq  Dump curr. call quality  | cd  Disconnect port      | dc  Dump config   |");
    puts("|                              |  V  Adjust audio Volume  |  f  Save config   |");
    puts("|  S  Send arbitrary REQUEST   | Cp  Codec priorities     |                   |");
    puts("+-----------------------------------------------------------------------------+");
#if PJSUA_HAS_VIDEO
    puts("| Video: \"vid help\" for more info                                             |");
    puts("+-----------------------------------------------------------------------------+");
#endif
    puts("|  q  QUIT   L  ReLoad   sleep MS   echo [0|1|txt]     n: detect NAT type     |");
    puts("+=============================================================================+");

    i = pjsua_call_get_count();
    printf("You have %d active call%s\n", i, (i>1?"s":""));

    if (current_call != PJSUA_INVALID_ID) {
	pjsua_call_info ci;
	if (pjsua_call_get_info(current_call, &ci)==PJ_SUCCESS)
	    printf("Current call id=%d to %.*s [%.*s]\n", current_call,
		   (int)ci.remote_info.slen, ci.remote_info.ptr,
		   (int)ci.state_text.slen, ci.state_text.ptr);
    }
}

/* Help screen for video */
#if PJSUA_HAS_VIDEO
static void vid_show_help()
{
    pj_bool_t vid_enabled = (app_config.vid.vid_cnt > 0);

    puts("+=============================================================================+");
    puts("|                            Video commands:                                  |");
    puts("|                                                                             |");
    puts("| vid help                  Show this help screen                             |");
    puts("| vid enable|disable        Enable or disable video in next offer/answer      |");
    puts("| vid acc show              Show current account video settings               |");
    puts("| vid acc autorx on|off     Automatically show incoming video on/off          |");
    puts("| vid acc autotx on|off     Automatically offer video on/off                  |");
    puts("| vid acc cap ID            Set default capture device for current acc        |");
    puts("| vid acc rend ID           Set default renderer device for current acc       |");
    puts("| vid call rx on|off N      Enable/disable video RX for stream N in curr call |");
    puts("| vid call tx on|off N      Enable/disable video TX for stream N in curr call |");
    puts("| vid call add              Add video stream for current call                 |");
    puts("| vid call enable|disable N Enable/disable stream #N in current call          |");
    puts("| vid call cap N ID         Set capture dev ID for stream #N in current call  |");
    puts("| vid dev list              List all video devices                            |");
    puts("| vid dev refresh           Refresh video device list                         |");
    puts("| vid dev prev on|off ID    Enable/disable preview for specified device ID    |");
    puts("| vid codec list            List video codecs                                 |");
    puts("| vid codec prio ID PRIO    Set codec ID priority to PRIO                     |");
    puts("| vid codec fps ID NUM DEN  Set codec ID framerate to (NUM/DEN) fps           |");
    puts("| vid codec bw ID AVG MAX   Set codec ID bitrate to AVG & MAX kbps            |");
    puts("| vid codec size ID W H     Set codec ID size/resolution to W x H             |");
    puts("| vid win list              List all active video windows                     |");
    puts("| vid win arrange           Auto arrange windows                              |");
    puts("| vid win show|hide ID      Show/hide the specified video window ID           |");
    puts("| vid win move ID X Y       Move window ID to position X,Y                    |");
    puts("| vid win resize ID w h     Resize window ID to the specified width, height   |");
    puts("+=============================================================================+");
    printf("| Video will be %s in the next offer/answer %s                            |\n",
	   (vid_enabled? "enabled" : "disabled"), (vid_enabled? " " : ""));
    puts("+=============================================================================+");
}

static void vid_handle_menu(char *menuin)
{
    char *argv[8];
    int argc = 0;

    /* Tokenize */
    argv[argc] = strtok(menuin, " \t\r\n");
    while (argv[argc] && *argv[argc]) {
	argc++;
	argv[argc] = strtok(NULL, " \t\r\n");
    }

    if (argc == 1 || strcmp(argv[1], "help")==0) {
	vid_show_help();
    } else if (argc == 2 && (strcmp(argv[1], "enable")==0 ||
			     strcmp(argv[1], "disable")==0))
    {
	pj_bool_t enabled = (strcmp(argv[1], "enable")==0);
	app_config.vid.vid_cnt = (enabled ? 1 : 0);
	PJ_LOG(3,(THIS_FILE, "Video will be %s in next offer/answer",
		  (enabled?"enabled":"disabled")));
    } else if (strcmp(argv[1], "acc")==0) {
	pjsua_acc_config acc_cfg;
	pj_bool_t changed = PJ_FALSE;

	pjsua_acc_get_config(current_acc, &acc_cfg);

	if (argc == 3 && strcmp(argv[2], "show")==0) {
	    app_config_show_video(current_acc, &acc_cfg);
	} else if (argc == 4 && strcmp(argv[2], "autorx")==0) {
	    int on = (strcmp(argv[3], "on")==0);
	    acc_cfg.vid_in_auto_show = on;
	    changed = PJ_TRUE;
	} else if (argc == 4 && strcmp(argv[2], "autotx")==0) {
	    int on = (strcmp(argv[3], "on")==0);
	    acc_cfg.vid_out_auto_transmit = on;
	    changed = PJ_TRUE;
	} else if (argc == 4 && strcmp(argv[2], "cap")==0) {
	    int dev = atoi(argv[3]);
	    acc_cfg.vid_cap_dev = dev;
	    changed = PJ_TRUE;
	} else if (argc == 4 && strcmp(argv[2], "rend")==0) {
	    int dev = atoi(argv[3]);
	    acc_cfg.vid_rend_dev = dev;
	    changed = PJ_TRUE;
	} else {
	    goto on_error;
	}

	if (changed) {
	    pj_status_t status = pjsua_acc_modify(current_acc, &acc_cfg);
	    if (status != PJ_SUCCESS)
		PJ_PERROR(1,(THIS_FILE, status, "Error modifying account %d",
			     current_acc));
	}

    } else if (strcmp(argv[1], "call")==0) {
	pjsua_call_vid_strm_op_param param;
	pj_status_t status = PJ_SUCCESS;

	pjsua_call_vid_strm_op_param_default(&param);

	if (argc == 5 && strcmp(argv[2], "rx")==0) {
	    pjsua_stream_info si;
	    pj_bool_t on = (strcmp(argv[3], "on") == 0);

	    param.med_idx = atoi(argv[4]);
	    if (pjsua_call_get_stream_info(current_call, param.med_idx, &si) ||
		si.type != PJMEDIA_TYPE_VIDEO)
	    {
		PJ_PERROR(1,(THIS_FILE, PJ_EINVAL, "Invalid stream"));
		return;
	    }

	    if (on) param.dir = (si.info.vid.dir | PJMEDIA_DIR_DECODING);
	    else param.dir = (si.info.vid.dir & PJMEDIA_DIR_ENCODING);

	    status = pjsua_call_set_vid_strm(current_call,
	                                     PJSUA_CALL_VID_STRM_CHANGE_DIR,
	                                     &param);
	}
	else if (argc == 5 && strcmp(argv[2], "tx")==0) {
	    pj_bool_t on = (strcmp(argv[3], "on") == 0);
	    pjsua_call_vid_strm_op op = on? PJSUA_CALL_VID_STRM_START_TRANSMIT :
					    PJSUA_CALL_VID_STRM_STOP_TRANSMIT;

	    param.med_idx = atoi(argv[4]);

	    status = pjsua_call_set_vid_strm(current_call, op, &param);
	}
	else if (argc == 3 && strcmp(argv[2], "add")==0) {
	    status = pjsua_call_set_vid_strm(current_call,
	                                     PJSUA_CALL_VID_STRM_ADD, NULL);
	}
	else if (argc >= 3 && 
		 (strcmp(argv[2], "disable")==0 || strcmp(argv[2], "enable")==0))
	{
	    pj_bool_t enable = (strcmp(argv[2], "enable") == 0);
	    pjsua_call_vid_strm_op op = enable? PJSUA_CALL_VID_STRM_CHANGE_DIR :
						PJSUA_CALL_VID_STRM_REMOVE;

	    param.med_idx = argc >= 4? atoi(argv[3]) : -1;
	    param.dir = PJMEDIA_DIR_ENCODING_DECODING;
	    status = pjsua_call_set_vid_strm(current_call, op, &param);
	}
	else if (argc >= 3 && strcmp(argv[2], "cap")==0) {
	    param.med_idx = argc >= 4? atoi(argv[3]) : -1;
	    param.cap_dev = argc >= 5? atoi(argv[4]) : PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
	    status = pjsua_call_set_vid_strm(current_call,
	                                     PJSUA_CALL_VID_STRM_CHANGE_CAP_DEV,
	                                     &param);
	} else
	    goto on_error;

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(1,(THIS_FILE, status, "Error modifying video stream"));
	}

    } else if (argc >= 3 && strcmp(argv[1], "dev")==0) {
	if (strcmp(argv[2], "list")==0) {
	    vid_list_devs();
	} else if (strcmp(argv[2], "refresh")==0) {
	    pjmedia_vid_dev_refresh();
	} else if (strcmp(argv[2], "prev")==0) {
	    if (argc != 5) {
		goto on_error;
	    } else {
		pj_bool_t on = (strcmp(argv[3], "on") == 0);
		int dev_id = atoi(argv[4]);
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
	    }
	} else
	    goto on_error;
    } else if (strcmp(argv[1], "win")==0) {
	pj_status_t status = PJ_SUCCESS;

	if (argc==3 && strcmp(argv[2], "list")==0) {
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
	} else if (argc==4 && (strcmp(argv[2], "show")==0 ||
			       strcmp(argv[2], "hide")==0))
	{
	    pj_bool_t show = (strcmp(argv[2], "show")==0);
	    pjsua_vid_win_id wid = atoi(argv[3]);
	    status = pjsua_vid_win_set_show(wid, show);
	} else if (argc==6 && strcmp(argv[2], "move")==0) {
	    pjsua_vid_win_id wid = atoi(argv[3]);
	    pjmedia_coord pos;

	    pos.x = atoi(argv[4]);
	    pos.y = atoi(argv[5]);
	    status = pjsua_vid_win_set_pos(wid, &pos);
	} else if (argc==6 && strcmp(argv[2], "resize")==0) {
	    pjsua_vid_win_id wid = atoi(argv[3]);
	    pjmedia_rect_size size;

	    size.w = atoi(argv[4]);
	    size.h = atoi(argv[5]);
	    status = pjsua_vid_win_set_size(wid, &size);
	} else if (argc==3 && strcmp(argv[2], "arrange")==0) {
	    arrange_window(PJSUA_INVALID_ID);
	} else
	    goto on_error;

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(1,(THIS_FILE, status, "Window operation error"));
	}

    } else if (strcmp(argv[1], "codec")==0) {
	pjsua_codec_info ci[PJMEDIA_CODEC_MGR_MAX_CODECS];
	unsigned count = PJ_ARRAY_SIZE(ci);
	pj_status_t status;

	if (argc==3 && strcmp(argv[2], "list")==0) {
	    status = pjsua_vid_enum_codecs(ci, &count);
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
	} else if (argc==5 && strcmp(argv[2], "prio")==0) {
	    pj_str_t cid;
	    int prio;
	    cid = pj_str(argv[3]);
	    prio = atoi(argv[4]);
	    status = pjsua_vid_codec_set_priority(&cid, (pj_uint8_t)prio);
	    if (status != PJ_SUCCESS)
		PJ_PERROR(1,(THIS_FILE, status, "Set codec priority error"));
	} else if (argc==6 && strcmp(argv[2], "fps")==0) {
	    pjmedia_vid_codec_param cp;
	    pj_str_t cid;
	    int M, N;
	    cid = pj_str(argv[3]);
	    M = atoi(argv[4]);
	    N = atoi(argv[5]);
	    status = pjsua_vid_codec_get_param(&cid, &cp);
	    if (status == PJ_SUCCESS) {
		cp.enc_fmt.det.vid.fps.num = M;
		cp.enc_fmt.det.vid.fps.denum = N;
		status = pjsua_vid_codec_set_param(&cid, &cp);
	    }
	    if (status != PJ_SUCCESS)
		PJ_PERROR(1,(THIS_FILE, status, "Set codec framerate error"));
	} else if (argc==6 && strcmp(argv[2], "bw")==0) {
	    pjmedia_vid_codec_param cp;
	    pj_str_t cid;
	    int M, N;
	    cid = pj_str(argv[3]);
	    M = atoi(argv[4]);
	    N = atoi(argv[5]);
	    status = pjsua_vid_codec_get_param(&cid, &cp);
	    if (status == PJ_SUCCESS) {
		cp.enc_fmt.det.vid.avg_bps = M * 1000;
		cp.enc_fmt.det.vid.max_bps = N * 1000;
		status = pjsua_vid_codec_set_param(&cid, &cp);
	    }
	    if (status != PJ_SUCCESS)
		PJ_PERROR(1,(THIS_FILE, status, "Set codec bitrate error"));
	} else if (argc==6 && strcmp(argv[2], "size")==0) {
	    pjmedia_vid_codec_param cp;
	    pj_str_t cid;
	    int M, N;
	    cid = pj_str(argv[3]);
	    M = atoi(argv[4]);
	    N = atoi(argv[5]);
	    status = pjsua_vid_codec_get_param(&cid, &cp);
	    if (status == PJ_SUCCESS) {
		cp.enc_fmt.det.vid.size.w = M;
		cp.enc_fmt.det.vid.size.h = N;
		status = pjsua_vid_codec_set_param(&cid, &cp);
	    }
	    if (status != PJ_SUCCESS)
		PJ_PERROR(1,(THIS_FILE, status, "Set codec size error"));
	} else
	    goto on_error;
    } else
	goto on_error;

    return;

on_error:
    PJ_LOG(1,(THIS_FILE, "Invalid command, use 'vid help'"));
}

#endif /* PJSUA_HAS_VIDEO */

/** UI Command **/
static void ui_make_new_call()
{    
    char buf[128];
    pjsua_msg_data msg_data;
    input_result result;
    pj_str_t tmp;

    printf("(You currently have %d calls)\n", pjsua_call_get_count());
    
    ui_input_url("Make call", buf, sizeof(buf), &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {

	if (result.nb_result == -1 || result.nb_result == 0) {
	    puts("You can't do that with make call!");
	    return;
	} else {
	    pjsua_buddy_info binfo;
	    pjsua_buddy_get_info(result.nb_result-1, &binfo);
	    tmp.ptr = buf;
	    pj_strncpy(&tmp, &binfo.uri, sizeof(buf));
	}

    } else if (result.uri_result) {
	tmp = pj_str(result.uri_result);
    } else {
	tmp.slen = 0;
    }

    pjsua_msg_data_init(&msg_data);
    TEST_MULTIPART(&msg_data);
    pjsua_call_make_call(current_acc, &tmp, &call_opt, NULL, 
			 &msg_data, &current_call);
}

static void ui_make_multi_call()
{
    char menuin[32];
    int count;
    char buf[128];
    input_result result;
    pj_str_t tmp;
    int i;

    printf("(You currently have %d calls)\n", pjsua_call_get_count());

    if (!simple_input("Number of calls", menuin, sizeof(menuin)))
	return;

    count = my_atoi(menuin);
    if (count < 1)
	return;

    ui_input_url("Make call", buf, sizeof(buf), &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {
	pjsua_buddy_info binfo;
	if (result.nb_result == -1 || result.nb_result == 0) {
	    puts("You can't do that with make call!");
	    return;
	}
	pjsua_buddy_get_info(result.nb_result-1, &binfo);
	tmp.ptr = buf;
	pj_strncpy(&tmp, &binfo.uri, sizeof(buf));
    } else {
	tmp = pj_str(result.uri_result);
    }

    for (i=0; i<my_atoi(menuin); ++i) {
	pj_status_t status;

	status = pjsua_call_make_call(current_acc, &tmp, &call_opt, NULL,
	    NULL, NULL);
	if (status != PJ_SUCCESS)
	    break;
    }
}

static void ui_detect_nat_type()
{
    int i = pjsua_detect_nat_type();
    if (i != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error", i);
}

static void ui_send_instant_message()
{
    char *uri = NULL;
    /* i is for call index to send message, if any */
    int i = -1;
    input_result result;
    char buf[128];
    char text[128];
    pj_str_t tmp;

    /* Input destination. */
    ui_input_url("Send IM to", buf, sizeof(buf), &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {

	if (result.nb_result == -1) {
	    puts("You can't send broadcast IM like that!");
	    return;

	} else if (result.nb_result == 0) {
	    i = current_call;
	} else {
	    pjsua_buddy_info binfo;
	    pjsua_buddy_get_info(result.nb_result-1, &binfo);
	    tmp.ptr = buf;
	    pj_strncpy_with_null(&tmp, &binfo.uri, sizeof(buf));
	    uri = buf;
	}

    } else if (result.uri_result) {
	uri = result.uri_result;
    }


    /* Send typing indication. */
    if (i != -1)
	pjsua_call_send_typing_ind(i, PJ_TRUE, NULL);
    else {
	pj_str_t tmp_uri = pj_str(uri);
	pjsua_im_typing(current_acc, &tmp_uri, PJ_TRUE, NULL);
    }

    /* Input the IM . */
    if (!simple_input("Message", text, sizeof(text))) {
	/*
	* Cancelled.
	* Send typing notification too, saying we're not typing.
	*/
	if (i != -1)
	    pjsua_call_send_typing_ind(i, PJ_FALSE, NULL);
	else {
	    pj_str_t tmp_uri = pj_str(uri);
	    pjsua_im_typing(current_acc, &tmp_uri, PJ_FALSE, NULL);
	}
	return;
    }

    tmp = pj_str(text);

    /* Send the IM */
    if (i != -1)
	pjsua_call_send_im(i, NULL, &tmp, NULL, NULL);
    else {
	pj_str_t tmp_uri = pj_str(uri);
	pjsua_im_send(current_acc, &tmp_uri, NULL, &tmp, NULL, NULL);
    }
}

static void ui_answer_call()
{
    pjsua_call_info call_info;
    char buf[128];
    pjsua_msg_data msg_data;

    if (current_call != -1) {
	pjsua_call_get_info(current_call, &call_info);
    } else {
	/* Make compiler happy */
	call_info.role = PJSIP_ROLE_UAC;
	call_info.state = PJSIP_INV_STATE_DISCONNECTED;
    }

    if (current_call == -1 || 
	call_info.role != PJSIP_ROLE_UAS ||
	call_info.state >= PJSIP_INV_STATE_CONNECTING)
    {
	puts("No pending incoming call");
	fflush(stdout);
	return;

    } else {
	int st_code;
	char contact[120];
	pj_str_t hname = { "Contact", 7 };
	pj_str_t hvalue;
	pjsip_generic_string_hdr hcontact;

	if (!simple_input("Answer with code (100-699)", buf, sizeof(buf)))
	    return;

	st_code = my_atoi(buf);
	if (st_code < 100)
	    return;

	pjsua_msg_data_init(&msg_data);

	if (st_code/100 == 3) {
	    if (!simple_input("Enter URL to be put in Contact", 
		contact, sizeof(contact)))
		return;
	    hvalue = pj_str(contact);
	    pjsip_generic_string_hdr_init2(&hcontact, &hname, &hvalue);

	    pj_list_push_back(&msg_data.hdr_list, &hcontact);
	}

	/*
	* Must check again!
	* Call may have been disconnected while we're waiting for 
	* keyboard input.
	*/
	if (current_call == -1) {
	    puts("Call has been disconnected");
	    fflush(stdout);
	    return;
	}

	pjsua_call_answer2(current_call, &call_opt, st_code, NULL, &msg_data);
    }    
}

static void ui_hangup_call(char menuin[])
{
    if (current_call == -1) {
	puts("No current call");
	fflush(stdout);
	return;

    } else if (menuin[1] == 'a') {
	/* Hangup all calls */
	pjsua_call_hangup_all();
    } else {
	/* Hangup current calls */
	pjsua_call_hangup(current_call, 0, NULL, NULL);
    }
}

static void ui_cycle_dialog(char menuin[])
{
    if (menuin[0] == ']') {
	find_next_call();

    } else {
	find_prev_call();
    }

    if (current_call != -1) {
	pjsua_call_info call_info;

	pjsua_call_get_info(current_call, &call_info);
	PJ_LOG(3,(THIS_FILE,"Current dialog: %.*s", 
	    (int)call_info.remote_info.slen, 
	    call_info.remote_info.ptr));

    } else {
	PJ_LOG(3,(THIS_FILE,"No current dialog"));
    }
}

static void ui_cycle_account()
{
    int i;
    char buf[128];

    if (!simple_input("Enter account ID to select", buf, sizeof(buf)))
	return;

    i = my_atoi(buf);
    if (pjsua_acc_is_valid(i)) {
	pjsua_acc_set_default(i);
	PJ_LOG(3,(THIS_FILE, "Current account changed to %d", i));
    } else {
	PJ_LOG(3,(THIS_FILE, "Invalid account id %d", i));
    }
}

static void ui_add_buddy()
{
    char buf[128];
    pjsua_buddy_config buddy_cfg;
    pjsua_buddy_id buddy_id;
    pj_status_t status;

    if (!simple_input("Enter buddy's URI:", buf, sizeof(buf)))
	return;

    if (pjsua_verify_url(buf) != PJ_SUCCESS) {
	printf("Invalid URI '%s'\n", buf);
	return;
    }

    pj_bzero(&buddy_cfg, sizeof(pjsua_buddy_config));

    buddy_cfg.uri = pj_str(buf);
    buddy_cfg.subscribe = PJ_TRUE;

    status = pjsua_buddy_add(&buddy_cfg, &buddy_id);
    if (status == PJ_SUCCESS) {
	printf("New buddy '%s' added at index %d\n",
	    buf, buddy_id+1);
    }
}

static void ui_add_account(pjsua_transport_config *rtp_cfg)
{
    char id[80], registrar[80], realm[80], uname[80], passwd[30];
    pjsua_acc_config acc_cfg;
    pj_status_t status;

    if (!simple_input("Your SIP URL:", id, sizeof(id)))
	return;
    if (!simple_input("URL of the registrar:", registrar, sizeof(registrar)))
	return;
    if (!simple_input("Auth Realm:", realm, sizeof(realm)))
	return;
    if (!simple_input("Auth Username:", uname, sizeof(uname)))
	return;
    if (!simple_input("Auth Password:", passwd, sizeof(passwd)))
	return;

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id = pj_str(id);
    acc_cfg.reg_uri = pj_str(registrar);
    acc_cfg.cred_count = 1;
    acc_cfg.cred_info[0].scheme = pj_str("Digest");
    acc_cfg.cred_info[0].realm = pj_str(realm);
    acc_cfg.cred_info[0].username = pj_str(uname);
    acc_cfg.cred_info[0].data_type = 0;
    acc_cfg.cred_info[0].data = pj_str(passwd);

    acc_cfg.rtp_cfg = *rtp_cfg;    
    app_config_init_video(&acc_cfg);

    status = pjsua_acc_add(&acc_cfg, PJ_TRUE, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error adding new account", status);
    }
}

static void ui_delete_buddy()
{
    char buf[128];
    int i;

    if (!simple_input("Enter buddy ID to delete", buf, sizeof(buf)))
	return;

    i = my_atoi(buf) - 1;

    if (!pjsua_buddy_is_valid(i)) {
	printf("Invalid buddy id %d\n", i);
    } else {
	pjsua_buddy_del(i);
	printf("Buddy %d deleted\n", i);
    }
}

static void ui_delete_account()
{
    char buf[128];
    int i;

    if (!simple_input("Enter account ID to delete", buf, sizeof(buf)))
	return;

    i = my_atoi(buf);

    if (!pjsua_acc_is_valid(i)) {
	printf("Invalid account id %d\n", i);
    } else {
	pjsua_acc_del(i);
	printf("Account %d deleted\n", i);
    }
}

static void ui_call_hold()
{
    if (current_call != -1) {		
	pjsua_call_set_hold(current_call, NULL);
    } else {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    }
}

static void ui_call_reinvite()
{
    call_opt.flag |= PJSUA_CALL_UNHOLD;
    pjsua_call_reinvite2(current_call, &call_opt, NULL);
}

static void ui_send_update()
{
    if (current_call != -1) {		
	pjsua_call_update2(current_call, &call_opt, NULL);
    } else {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    }
}

/*
 * Change codec priorities.
 */
static void ui_manage_codec_prio()
{
    pjsua_codec_info c[32];
    unsigned i, count = PJ_ARRAY_SIZE(c);
    char input[32];
    char *codec, *prio;
    pj_str_t id;
    int new_prio;
    pj_status_t status;

    printf("List of audio codecs:\n");
    pjsua_enum_codecs(c, &count);
    for (i=0; i<count; ++i) {
	printf("  %d\t%.*s\n", c[i].priority, (int)c[i].codec_id.slen,
			       c[i].codec_id.ptr);
    }

#if PJSUA_HAS_VIDEO
    puts("");
    printf("List of video codecs:\n");
    pjsua_vid_enum_codecs(c, &count);
    for (i=0; i<count; ++i) {
	printf("  %d\t%.*s%s%.*s\n", c[i].priority,
				     (int)c[i].codec_id.slen,
				     c[i].codec_id.ptr,
				     c[i].desc.slen? " - ":"",
				     (int)c[i].desc.slen,
				     c[i].desc.ptr);
    }
#endif

    puts("");
    puts("Enter codec id and its new priority (e.g. \"speex/16000 200\", "
	 """\"H263 200\"),");
    puts("or empty to cancel.");

    printf("Codec name (\"*\" for all) and priority: ");
    if (fgets(input, sizeof(input), stdin) == NULL)
	return;
    if (input[0]=='\r' || input[0]=='\n') {
	puts("Done");
	return;
    }

    codec = strtok(input, " \t\r\n");
    prio = strtok(NULL, " \r\n");

    if (!codec || !prio) {
	puts("Invalid input");
	return;
    }

    new_prio = atoi(prio);
    if (new_prio < 0) 
	new_prio = 0;
    else if (new_prio > PJMEDIA_CODEC_PRIO_HIGHEST) 
	new_prio = PJMEDIA_CODEC_PRIO_HIGHEST;

    status = pjsua_codec_set_priority(pj_cstr(&id, codec), 
				      (pj_uint8_t)new_prio);
#if PJSUA_HAS_VIDEO
    if (status != PJ_SUCCESS) {
	status = pjsua_vid_codec_set_priority(pj_cstr(&id, codec), 
					      (pj_uint8_t)new_prio);
    }
#endif
    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error setting codec priority", status);
}

static void ui_call_transfer(pj_bool_t no_refersub)
{
    if (current_call == -1) {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    } else {
	int call = current_call;
	char buf[128];
	pjsip_generic_string_hdr refer_sub;
	pj_str_t STR_REFER_SUB = { "Refer-Sub", 9 };
	pj_str_t STR_FALSE = { "false", 5 };
	pjsua_call_info ci;
	input_result result;
	pjsua_msg_data msg_data;

	pjsua_call_get_info(current_call, &ci);
	printf("Transfering current call [%d] %.*s\n", current_call,
	       (int)ci.remote_info.slen, ci.remote_info.ptr);

	ui_input_url("Transfer to URL", buf, sizeof(buf), &result);

	/* Check if call is still there. */

	if (call != current_call) {
	    puts("Call has been disconnected");
	    return;
	}

	pjsua_msg_data_init(&msg_data);
	if (no_refersub) {
	    /* Add Refer-Sub: false in outgoing REFER request */
	    pjsip_generic_string_hdr_init2(&refer_sub, &STR_REFER_SUB,
		&STR_FALSE);
	    pj_list_push_back(&msg_data.hdr_list, &refer_sub);
	}
	if (result.nb_result != PJSUA_APP_NO_NB) {
	    if (result.nb_result == -1 || result.nb_result == 0)
		puts("You can't do that with transfer call!");
	    else {
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
}

static void ui_call_transfer_replaces(pj_bool_t no_refersub)
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
	pjsua_call_info ci;
	pjsua_msg_data msg_data;
	char buf[128];
	unsigned i, count;

	count = PJ_ARRAY_SIZE(ids);
	pjsua_enum_calls(ids, &count);

	if (count <= 1) {
	    puts("There are no other calls");
	    return;
	}

	pjsua_call_get_info(current_call, &ci);
	printf("Transfer call [%d] %.*s to one of the following:\n",
	       current_call,
	       (int)ci.remote_info.slen, ci.remote_info.ptr);

	for (i=0; i<count; ++i) {
	    pjsua_call_info call_info;

	    if (ids[i] == call)
		return;

	    pjsua_call_get_info(ids[i], &call_info);
	    printf("%d  %.*s [%.*s]\n",
		ids[i],
		(int)call_info.remote_info.slen,
		call_info.remote_info.ptr,
		(int)call_info.state_text.slen,
		call_info.state_text.ptr);
	}

	if (!simple_input("Enter call number to be replaced", buf, sizeof(buf)))
	    return;

	dst_call = my_atoi(buf);

	/* Check if call is still there. */

	if (call != current_call) {
	    puts("Call has been disconnected");
	    return;
	}

	/* Check that destination call is valid. */
	if (dst_call == call) {
	    puts("Destination call number must not be the same "
		"as the call being transfered");
	    return;
	}
	if (dst_call >= PJSUA_MAX_CALLS) {
	    puts("Invalid destination call number");
	    return;
	}
	if (!pjsua_call_is_active(dst_call)) {
	    puts("Invalid destination call number");
	    return;
	}

	pjsua_msg_data_init(&msg_data);
	if (no_refersub) {
	    /* Add Refer-Sub: false in outgoing REFER request */
	    pjsip_generic_string_hdr_init2(&refer_sub, &STR_REFER_SUB, 
					   &STR_FALSE);
	    pj_list_push_back(&msg_data.hdr_list, &refer_sub);
	}

	pjsua_call_xfer_replaces(call, dst_call, 
				 PJSUA_XFER_NO_REQUIRE_REPLACES, 
				 &msg_data);
    }
}

static void ui_send_dtmf_2833()
{
    if (current_call == -1) {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    } else if (!pjsua_call_has_media(current_call)) {
	PJ_LOG(3,(THIS_FILE, "Media is not established yet!"));
    } else {
	pj_str_t digits;
	int call = current_call;
	pj_status_t status;
	char buf[128];

	if (!simple_input("DTMF strings to send (0-9*#A-B)", buf, 
	    sizeof(buf)))
	{
	    return;
	}

	if (call != current_call) {
	    puts("Call has been disconnected");
	    return;
	}

	digits = pj_str(buf);
	status = pjsua_call_dial_dtmf(current_call, &digits);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to send DTMF", status);
	} else {
	    puts("DTMF digits enqueued for transmission");
	}
    }
}

static void ui_send_dtmf_info()
{
    if (current_call == -1) {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    } else {
	const pj_str_t SIP_INFO = pj_str("INFO");
	pj_str_t digits;
	int call = current_call;
	int i;
	pj_status_t status;
	char buf[128];

	if (!simple_input("DTMF strings to send (0-9*#A-B)", buf, 
	    sizeof(buf)))
	{
	    return;
	}

	if (call != current_call) {
	    puts("Call has been disconnected");
	    return;
	}

	digits = pj_str(buf);
	for (i=0; i<digits.slen; ++i) {
	    char body[80];
	    pjsua_msg_data msg_data;

	    pjsua_msg_data_init(&msg_data);
	    msg_data.content_type = pj_str("application/dtmf-relay");

	    pj_ansi_snprintf(body, sizeof(body),
		"Signal=%c\r\n"
		"Duration=160",
		buf[i]);
	    msg_data.msg_body = pj_str(body);

	    status = pjsua_call_send_request(current_call, &SIP_INFO, 
		&msg_data);
	    if (status != PJ_SUCCESS) {
		return;
	    }
	}
    }
}

static void ui_send_arbitrary_request()
{
    char text[128];
    char buf[128];
    char *uri;
    input_result result;
    pj_str_t tmp;

    if (pjsua_acc_get_count() == 0) {
	puts("Sorry, need at least one account configured");
	return;
    }

    puts("Send arbitrary request to remote host");

    /* Input METHOD */
    if (!simple_input("Request method:",text,sizeof(text)))
	return;

    /* Input destination URI */
    uri = NULL;
    ui_input_url("Destination URI", buf, sizeof(buf), &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {

	if (result.nb_result == -1) {
	    puts("Sorry you can't do that!");
	    return;
	} else if (result.nb_result == 0) {
	    uri = NULL;
	    if (current_call == PJSUA_INVALID_ID) {
		puts("No current call");
		return;
	    }
	} else {	    
	    pjsua_buddy_info binfo;
	    pjsua_buddy_get_info(result.nb_result-1, &binfo);
	    tmp.ptr = buf;
	    pj_strncpy_with_null(&tmp, &binfo.uri, sizeof(buf));
	    uri = buf;
	}

    } else if (result.uri_result) {
	uri = result.uri_result;
    } else {
	return;
    }

    if (uri) {
	tmp = pj_str(uri);
	send_request(text, &tmp);
    } else {
	/* If you send call control request using this method
	* (such requests includes BYE, CANCEL, etc.), it will
	* not go well with the call state, so don't do it
	* unless it's for testing.
	*/
	pj_str_t method = pj_str(text);
	pjsua_call_send_request(current_call, &method, NULL);
    }	    
}

static void ui_echo(char menuin[])
{
    if (pj_ansi_strnicmp(menuin, "echo", 4)==0) {
	pj_str_t tmp;

	tmp.ptr = menuin+5;
	tmp.slen = pj_ansi_strlen(menuin)-6;

	if (tmp.slen < 1) {
	    puts("Usage: echo [0|1]");
	    return;
	}
	cmd_echo = *tmp.ptr != '0' || tmp.slen!=1;
    }
}

static void ui_sleep(char menuin[])
{
    if (pj_ansi_strnicmp(menuin, "sleep", 5)==0) {
	pj_str_t tmp;
	int delay;

	tmp.ptr = menuin+6;
	tmp.slen = pj_ansi_strlen(menuin)-7;

	if (tmp.slen < 1) {
	    puts("Usage: sleep MSEC");
	    return;
	}

	delay = pj_strtoul(&tmp);
	if (delay < 0) delay = 0;
	pj_thread_sleep(delay);		
    }
}

static void ui_subscribe(char menuin[])
{
    char buf[128];
    input_result result;

    ui_input_url("(un)Subscribe presence of", buf, sizeof(buf), &result);
    if (result.nb_result != PJSUA_APP_NO_NB) {
	if (result.nb_result == -1) {
	    int i, count;
	    count = pjsua_get_buddy_count();
	    for (i=0; i<count; ++i)
		pjsua_buddy_subscribe_pres(i, menuin[0]=='s');
	} else if (result.nb_result == 0) {
	    puts("Sorry, can only subscribe to buddy's presence, "
		"not from existing call");
	} else {
	    pjsua_buddy_subscribe_pres(result.nb_result-1, (menuin[0]=='s'));
	}

    } else if (result.uri_result) {
	puts("Sorry, can only subscribe to buddy's presence, "
	    "not arbitrary URL (for now)");
    }
}

static void ui_register(char menuin[])
{
    switch (menuin[1]) {
    case 'r':
	/*
	* Re-Register.
	*/
	pjsua_acc_set_registration(current_acc, PJ_TRUE);
	break;
    case 'u':
	/*
	* Unregister
	*/
	pjsua_acc_set_registration(current_acc, PJ_FALSE);
	break;
    }
}

static void ui_toggle_state()
{
    pjsua_acc_info acc_info;

    pjsua_acc_get_info(current_acc, &acc_info);
    acc_info.online_status = !acc_info.online_status;
    pjsua_acc_set_online_status(current_acc, acc_info.online_status);
    printf("Setting %s online status to %s\n",
	   acc_info.acc_uri.ptr,
	   (acc_info.online_status?"online":"offline"));
}

/*
 * Change extended online status.
 */
static void ui_change_online_status()
{
    char menuin[32];
    pj_bool_t online_status;
    pjrpid_element elem;
    int i, choice;

    enum {
	AVAILABLE, BUSY, OTP, IDLE, AWAY, BRB, OFFLINE, OPT_MAX
    };

    struct opt {
	int id;
	char *name;
    } opts[] = {
	{ AVAILABLE, "Available" },
	{ BUSY, "Busy"},
	{ OTP, "On the phone"},
	{ IDLE, "Idle"},
	{ AWAY, "Away"},
	{ BRB, "Be right back"},
	{ OFFLINE, "Offline"}
    };

    printf("\n"
	   "Choices:\n");
    for (i=0; i<PJ_ARRAY_SIZE(opts); ++i) {
	printf("  %d  %s\n", opts[i].id+1, opts[i].name);
    }

    if (!simple_input("Select status", menuin, sizeof(menuin)))
	return;

    choice = atoi(menuin) - 1;
    if (choice < 0 || choice >= OPT_MAX) {
	puts("Invalid selection");
	return;
    }

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
}

/*
 * List the ports in conference bridge
 */
static void ui_conf_list()
{
    unsigned i, count;
    pjsua_conf_port_id id[PJSUA_MAX_CALLS];

    printf("Conference ports:\n");

    count = PJ_ARRAY_SIZE(id);
    pjsua_enum_conf_ports(id, &count);

    for (i=0; i<count; ++i) {
	char txlist[PJSUA_MAX_CALLS*4+10];
	unsigned j;
	pjsua_conf_port_info info;

	pjsua_conf_get_port_info(id[i], &info);

	txlist[0] = '\0';
	for (j=0; j<info.listener_cnt; ++j) {
	    char s[10];
	    pj_ansi_snprintf(s, sizeof(s), "#%d ", info.listeners[j]);
	    pj_ansi_strcat(txlist, s);
	}
	printf("Port #%02d[%2dKHz/%dms/%d] %20.*s  transmitting to: %s\n", 
	       info.slot_id, 
	       info.clock_rate/1000,
	       info.samples_per_frame*1000/info.channel_count/info.clock_rate,
	       info.channel_count,
	       (int)info.name.slen, 
	       info.name.ptr,
	       txlist);

    }
    puts("");
}

static void ui_conf_connect(char menuin[])
{
    char tmp[10], src_port[10], dst_port[10];
    pj_status_t status;
    int cnt;
    const char *src_title, *dst_title;

    cnt = sscanf(menuin, "%s %s %s", tmp, src_port, dst_port);

    if (cnt != 3) {
	ui_conf_list();

	src_title = (menuin[1]=='c'? "Connect src port #":
				     "Disconnect src port #");
	dst_title = (menuin[1]=='c'? "To dst port #":"From dst port #");

	if (!simple_input(src_title, src_port, sizeof(src_port)))
	    return;

	if (!simple_input(dst_title, dst_port, sizeof(dst_port)))
	    return;
    }

    if (menuin[1]=='c') {
	status = pjsua_conf_connect(my_atoi(src_port), my_atoi(dst_port));
    } else {
	status = pjsua_conf_disconnect(my_atoi(src_port), my_atoi(dst_port));
    }
    if (status == PJ_SUCCESS) {
	puts("Success");
    } else {
	puts("ERROR!!");
    }
}

static void ui_adjust_volume()
{
    char buf[128];
    char text[128];
    sprintf(buf, "Adjust mic level: [%4.1fx] ", app_config.mic_level);
    if (simple_input(buf,text,sizeof(text))) {
	char *err;
	app_config.mic_level = (float)strtod(text, &err);
	pjsua_conf_adjust_rx_level(0, app_config.mic_level);
    }
    sprintf(buf, "Adjust speaker level: [%4.1fx] ", app_config.speaker_level);
    if (simple_input(buf,text,sizeof(text))) {
	char *err;
	app_config.speaker_level = (float)strtod(text, &err);
	pjsua_conf_adjust_tx_level(0, app_config.speaker_level);
    }
}

static void ui_dump_call_quality()
{
    if (current_call != PJSUA_INVALID_ID) {
	log_call_dump(current_call);
    } else {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    }
}

static void ui_dump_configuration()
{
    char settings[2000];
    int len;

    len = write_settings(&app_config, settings, sizeof(settings));
    if (len < 1)
	PJ_LOG(1,(THIS_FILE, "Error: not enough buffer"));
    else
	PJ_LOG(3,(THIS_FILE, "Dumping configuration (%d bytes):\n%s\n",	
		  len, settings));	
}

static void ui_write_settings()
{
    char settings[2000];
    int len;
    char buf[128];

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
	    pj_ssize_t size = len;
	    pj_file_write(fd, settings, &size);
	    pj_file_close(fd);

	    printf("Settings successfully written to '%s'\n", buf);
	}
    }
}

/*
 * Dump application states.
 */
static void ui_app_dump(pj_bool_t detail)
{
    pjsua_dump(detail);
}

static void ui_call_redirect(char menuin[])
{
    if (current_call == PJSUA_INVALID_ID) {
	PJ_LOG(3,(THIS_FILE, "No current call"));
    } else {
	if (!pjsua_call_is_active(current_call)) {
	    PJ_LOG(1,(THIS_FILE, "Call %d has gone", current_call));
	} else if (menuin[1] == 'a') {
	    pjsua_call_process_redirect(current_call, 
		PJSIP_REDIRECT_ACCEPT_REPLACE);
	} else if (menuin[1] == 'A') {
	    pjsua_call_process_redirect(current_call, 
		PJSIP_REDIRECT_ACCEPT);
	} else if (menuin[1] == 'r') {
	    pjsua_call_process_redirect(current_call,
		PJSIP_REDIRECT_REJECT);
	} else {
	    pjsua_call_process_redirect(current_call,
		PJSIP_REDIRECT_STOP);
	}
    }
}

/*
 * Main "user interface" loop.
 */
PJ_DEF(void) legacy_main()
{    
    char menuin[80];    
    char buf[128];    

    keystroke_help(current_call);

    for (;;) {

	printf(">>> ");
	fflush(stdout);

	if (fgets(menuin, sizeof(menuin), stdin) == NULL) {
	    /* 
	     * Be friendly to users who redirect commands into
	     * program, when file ends, resume with kbd.
	     * If exit is desired end script with q for quit
	     */
 	    /* Reopen stdin/stdout/stderr to /dev/console */
#if defined(PJ_WIN32) && PJ_WIN32!=0
	    if (freopen ("CONIN$", "r", stdin) == NULL) {
#else
	    if (1) {
#endif
		puts("Cannot switch back to console from file redirection");
		menuin[0] = 'q';
		menuin[1] = '\0';
	    } else {
		puts("Switched back to console from file redirection");
		continue;
	    }
	}

	if (cmd_echo) {
	    printf("%s", menuin);
	}

	/* Update call setting */
	pjsua_call_setting_default(&call_opt);
	call_opt.aud_cnt = app_config.aud_cnt;
	call_opt.vid_cnt = app_config.vid.vid_cnt;

	switch (menuin[0]) {

	case 'm':
	    /* Make call! : */
	    ui_make_new_call();
	    break;

	case 'M':
	    /* Make multiple calls! : */
	    ui_make_multi_call();
	    break;

	case 'n':
	    ui_detect_nat_type();
	    break;

	case 'i':
	    /* Send instant messaeg */
	    ui_send_instant_message();
	    break;

	case 'a':
	    ui_answer_call();
	    break;

	case 'h':
	    ui_hangup_call(menuin);
	    break;

	case ']':
	case '[':
	    /*
	     * Cycle next/prev dialog.
	     */
	    ui_cycle_dialog(menuin);
	    break;

	case '>':
	case '<':
	    ui_cycle_account();
	    break;

	case '+':
	    if (menuin[1] == 'b') {
		ui_add_buddy();
	    } else if (menuin[1] == 'a') {
		ui_add_account(&app_config.rtp_cfg);
	    } else {
		printf("Invalid input %s\n", menuin);
	    }
	    break;

	case '-':
	    if (menuin[1] == 'b') {
		ui_delete_buddy();
	    } else if (menuin[1] == 'a') {
		ui_delete_account();
	    } else {
		printf("Invalid input %s\n", menuin);
	    }
	    break;

	case 'H':
	    /*
	     * Hold call.
	     */
	    ui_call_hold();
	    break;

	case 'v':	    
#if PJSUA_HAS_VIDEO
	    if (menuin[1]=='i' && menuin[2]=='d' && menuin[3]==' ') {
		vid_handle_menu(menuin);
	    } else
#endif
	    if (current_call != -1) {
		/*
		 * re-INVITE
		 */
		ui_call_reinvite();
	    } else {
		PJ_LOG(3,(THIS_FILE, "No current call"));
	    }
	    break;

	case 'U':
	    /*
	     * Send UPDATE
	     */
	    ui_send_update();
	    break;

	case 'C':
	    if (menuin[1] == 'p') {
		ui_manage_codec_prio();
	    }
	    break;

	case 'x':
	    /*
	     * Transfer call.
	     */
	    ui_call_transfer(app_config.no_refersub);
	    break;

	case 'X':
	    /*
	     * Transfer call with replaces.
	     */
	    ui_call_transfer_replaces(app_config.no_refersub);
	    break;

	case '#':
	    /*
	     * Send DTMF strings.
	     */
	    ui_send_dtmf_2833();
	    break;

	case '*':
	    /* Send DTMF with INFO */
	    ui_send_dtmf_info();
	    break;

	case 'S':
	    /*
	     * Send arbitrary request
	     */
	    ui_send_arbitrary_request();
	    break;

	case 'e':
	    ui_echo(menuin);
	    break;

	case 's':	    
	    ui_sleep(menuin);
	    break;
	    /* Continue below */

	case 'u':
	    /*
	     * Subscribe/unsubscribe presence.
	     */
	    ui_subscribe(menuin);
	    break;

	case 'r':
	    ui_register(menuin);
	    break;
	    
	case 't':
	    ui_toggle_state();
	    break;

	case 'T':
	    ui_change_online_status();
	    break;

	case 'c':
	    switch (menuin[1]) {
	    case 'l':
		ui_conf_list();
		break;
	    case 'c':
	    case 'd':
		ui_conf_connect(menuin);
		break;
	    }
	    break;

	case 'V':
	    /* Adjust audio volume */	    
	    ui_adjust_volume();
	    break;

	case 'd':
	    if (menuin[1] == 'c') {
		ui_dump_configuration();	
	    } else if (menuin[1] == 'q') {
		ui_dump_call_quality();
	    } else {		
		ui_app_dump(menuin[1]=='d');
	    }
	    break;

	case 'f':
	    if (simple_input("Enter output filename", buf, sizeof(buf))) {
		ui_write_settings();		
	    }
	    break;

	case 'L':   /* Restart */
	case 'q':
	    legacy_on_stopped(menuin[0]=='L');
	    goto on_exit;

	case 'R':	    
	    ui_call_redirect(menuin);
	    break;

	default:
	    if (menuin[0] != '\n' && menuin[0] != '\r') {
		printf("Invalid input %s", menuin);
	    }
	    keystroke_help();
	    break;
	}
    }

on_exit:
    ;
}

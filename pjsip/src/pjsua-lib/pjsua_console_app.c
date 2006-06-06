/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjsua-lib/pjsua_console_app.h>
#include <stdlib.h>		/* atoi */
#include <stdio.h>

#define THIS_FILE	"main.c"

/* Current dialog */
static int current_acc;
static int current_call = -1;


/*
 * Find next call.
 */
static pj_bool_t find_next_call(void)
{
    int i, max;

    max = pjsua_call_get_max_count();
    for (i=current_call+1; i<max; ++i) {
	if (pjsua_call_is_active(i)) {
	    current_call = i;
	    return PJ_TRUE;
	}
    }

    for (i=0; i<current_call; ++i) {
	if (pjsua_call_is_active(i)) {
	    current_call = i;
	    return PJ_TRUE;
	}
    }

    current_call = -1;
    return PJ_FALSE;
}


/*
 * Find previous call.
 */
static pj_bool_t find_prev_call(void)
{
    int i, max;

    max = pjsua_call_get_max_count();
    for (i=current_call-1; i>=0; --i) {
	if (pjsua_call_is_active(i)) {
	    current_call = i;
	    return PJ_TRUE;
	}
    }

    for (i=max-1; i>current_call; --i) {
	if (pjsua_call_is_active(i)) {
	    current_call = i;
	    return PJ_TRUE;
	}
    }

    current_call = -1;
    return PJ_FALSE;
}



/*
 * Notify UI when invite state has changed.
 */
static void console_on_call_state(int call_index, pjsip_event *e)
{
    pjsua_call_info call_info;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_index, &call_info);

    if (call_info.state == PJSIP_INV_STATE_DISCONNECTED) {

	PJ_LOG(3,(THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%s)]", 
		  call_index,
		  call_info.last_status,
		  call_info.last_status_text.ptr));

	if ((int)call_index == current_call) {
	    find_next_call();
	}

    } else {

	PJ_LOG(3,(THIS_FILE, "Call %d state changed to %s", 
		  call_index,
		  call_info.state_text.ptr));

	if (current_call==-1)
	    current_call = call_index;

    }
}

/**
 * Notify UI when registration status has changed.
 */
static void console_on_reg_state(int acc_index)
{
    PJ_UNUSED_ARG(acc_index);

    // Log already written.
}


/**
 * Notify UI on buddy state changed.
 */
static void console_on_buddy_state(int buddy_index)
{
    pjsua_buddy_info info;
    pjsua_buddy_get_info(buddy_index, &info);

    PJ_LOG(3,(THIS_FILE, "%.*s status is %.*s",
	      (int)info.uri.slen,
	      info.uri.ptr,
	      (int)info.status_text.slen,
	      info.status_text.ptr));
}


/**
 * Incoming IM message (i.e. MESSAGE request)!
 */
static void console_on_pager(int call_index, const pj_str_t *from, 
			     const pj_str_t *to, const pj_str_t *text)
{
    /* Note: call index may be -1 */
    PJ_UNUSED_ARG(call_index);
    PJ_UNUSED_ARG(to);

    PJ_LOG(3,(THIS_FILE,"MESSAGE from %.*s: %.*s",
	      (int)from->slen, from->ptr,
	      (int)text->slen, text->ptr));
}


/**
 * Typing indication
 */
static void console_on_typing(int call_index, const pj_str_t *from,
			      const pj_str_t *to, pj_bool_t is_typing)
{
    PJ_UNUSED_ARG(call_index);
    PJ_UNUSED_ARG(to);

    PJ_LOG(3,(THIS_FILE, "IM indication: %.*s %s",
	      (int)from->slen, from->ptr,
	      (is_typing?"is typing..":"has stopped typing")));
}


/*
 * Print buddy list.
 */
static void print_buddy_list(void)
{
    int i, count;

    puts("Buddy list:");

    count = pjsua_get_buddy_count();

    if (count == 0)
	puts(" -none-");
    else {
	for (i=0; i<count; ++i) {
	    pjsua_buddy_info info;

	    if (pjsua_buddy_get_info(i, &info) != PJ_SUCCESS)
		continue;

	    if (!info.is_valid)
		continue;

	    printf(" [%2d] <%7s>  %.*s\n", 
		    i+1, info.status_text.ptr, 
		    (int)info.uri.slen,
		    info.uri.ptr);
	}
    }
    puts("");
}


/*
 * Print account status.
 */
static void print_acc_status(int acc_index)
{
    char buf[80];
    pjsua_acc_info info;

    pjsua_acc_get_info(acc_index, &info);

    if (!info.has_registration) {
	pj_ansi_strcpy(buf, " -not registered to server-");

    } else {
	pj_ansi_snprintf(buf, sizeof(buf),
			 "%.*s (%.*s;expires=%d)",
			 (int)info.status_text.slen,
			 info.status_text.ptr,
			 (int)info.acc_id.slen,
			 info.acc_id.ptr,
			 info.expires);

    }

    printf("[%2d] Registration status: %s\n", acc_index, buf);
    printf("     Online status: %s\n", 
	   (info.online_status ? "Online" : "Invisible"));
}

/*
 * Show a bit of help.
 */
static void keystroke_help(void)
{
    int i;

    printf(">>>>\n");

    for (i=0; i<(int)pjsua_get_acc_count(); ++i)
	print_acc_status(i);

    print_buddy_list();
    
    //puts("Commands:");
    puts("+=============================================================================+");
    puts("|       Call Commands:         |      IM & Presence:      |   Misc:           |");
    puts("|                              |                          |                   |");
    puts("|  m  Make new call            |  i  Send IM              |  o  Send OPTIONS  |");
    puts("|  M  Make multiple calls      |  s  Subscribe presence   | rr  (Re-)register |");
    puts("|  a  Answer call              |  u  Unsubscribe presence | ru  Unregister    |");
    puts("|  h  Hangup call  (ha=all)    |  t  ToGgle Online status |                   |");
    puts("|  H  Hold call                |                          |                   |");
    puts("|  v  re-inVite (release hold) +--------------------------+-------------------+");
    puts("|  ]  Select next dialog       |     Conference Command   |                   |");
    puts("|  [  Select previous dialog   | cl  List ports           |  d  Dump status   |");
    puts("|  x  Xfer call                | cc  Connect port         | dd  Dump detailed |");
    puts("|  #  Send DTMF string         | cd  Disconnect port      | dc  Dump config   |");
    puts("+------------------------------+--------------------------+-------------------+");
    puts("|  q  QUIT                                                                    |");
    puts("+=============================================================================+");
}


/*
 * Input simple string
 */
static pj_bool_t simple_input(const char *title, char *buf, pj_size_t len)
{
    char *p;

    printf("%s (empty to cancel): ", title); fflush(stdout);
    fgets(buf, len, stdin);

    /* Remove trailing newlines. */
    for (p=buf; ; ++p) {
	if (*p=='\r' || *p=='\n') *p='\0';
	else if (!*p) break;
    }

    if (!*buf)
	return PJ_FALSE;
    
    return PJ_TRUE;
}


#define NO_NB	-2
struct input_result
{
    int	  nb_result;
    char *uri_result;
};


/*
 * Input URL.
 */
static void ui_input_url(const char *title, char *buf, int len, 
			 struct input_result *result)
{
    result->nb_result = NO_NB;
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
    fgets(buf, len, stdin);
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

	result->nb_result = atoi(buf);

	if (result->nb_result >= 0 && 
	    result->nb_result <= (int)pjsua_get_buddy_count()) 
	{
	    return;
	}
	if (result->nb_result == -1)
	    return;

	puts("Invalid input");
	result->nb_result = NO_NB;
	return;

    } else {
	pj_status_t status;

	if ((status=pjsua_verify_sip_url(buf)) != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Invalid URL", status);
	    return;
	}

	result->uri_result = buf;
    }
}

static void conf_list(void)
{
    unsigned i, count;
    pjsua_conf_port_id id[PJSUA_MAX_CALLS];

    printf("Conference ports:\n");

    count = PJ_ARRAY_SIZE(id);
    pjsua_conf_enum_port_ids(id, &count);

    for (i=0; i<count; ++i) {
	char txlist[PJSUA_MAX_CALLS*4+10];
	unsigned j;
	pjsua_conf_port_info info;

	pjsua_conf_get_port_info(id[i], &info);

	txlist[0] = '\0';
	for (j=0; j<info.listener_cnt; ++j) {
	    char s[10];
	    pj_ansi_sprintf(s, "#%d ", info.listeners[j]);
	    pj_ansi_strcat(txlist, s);
	}
	printf("Port #%02d[%2dKHz/%dms] %20.*s  transmitting to: %s\n", 
	       info.slot_id, 
	       info.clock_rate/1000,
	       info.samples_per_frame * 1000 / info.clock_rate,
	       (int)info.name.slen, 
	       info.name.ptr,
	       txlist);

    }
    puts("");
}


void pjsua_console_app_main(const pj_str_t *uri_to_call)
{
    char menuin[10];
    char buf[128];
    char text[128];
    int i, count;
    char *uri;
    pj_str_t tmp;
    struct input_result result;
    pjsua_call_info call_info;
    pjsua_acc_info acc_info;


    /* If user specifies URI to call, then call the URI */
    if (uri_to_call->slen) {
	pjsua_call_make_call( current_acc, uri_to_call, NULL);
    }

    keystroke_help();

    for (;;) {

	printf(">>> ");
	fflush(stdout);

	fgets(menuin, sizeof(menuin), stdin);

	switch (menuin[0]) {

	case 'm':
	    /* Make call! : */
	    printf("(You currently have %d calls)\n", 
		     pjsua_call_get_count());
	    
	    uri = NULL;
	    ui_input_url("Make call", buf, sizeof(buf), &result);
	    if (result.nb_result != NO_NB) {

		if (result.nb_result == -1 || result.nb_result == 0) {
		    puts("You can't do that with make call!");
		    continue;
		} else {
		    pjsua_buddy_info binfo;
		    pjsua_buddy_get_info(result.nb_result-1, &binfo);
		    uri = binfo.uri.ptr;
		}

	    } else if (result.uri_result) {
		uri = result.uri_result;
	    }
	    
	    tmp = pj_str(uri);
	    pjsua_call_make_call( current_acc, &tmp, NULL);
	    break;

	case 'M':
	    /* Make multiple calls! : */
	    printf("(You currently have %d calls)\n", 
		   pjsua_call_get_count());
	    
	    if (!simple_input("Number of calls", menuin, sizeof(menuin)))
		continue;

	    count = atoi(menuin);
	    if (count < 1)
		continue;

	    ui_input_url("Make call", buf, sizeof(buf), &result);
	    if (result.nb_result != NO_NB) {
		pjsua_buddy_info binfo;
		if (result.nb_result == -1 || result.nb_result == 0) {
		    puts("You can't do that with make call!");
		    continue;
		}
		pjsua_buddy_get_info(result.nb_result-1, &binfo);
		uri = binfo.uri.ptr;
	    } else {
		uri =  result.uri_result;
	    }

	    for (i=0; i<atoi(menuin); ++i) {
		pj_status_t status;
	    
		tmp = pj_str(uri);
		status = pjsua_call_make_call(current_acc, &tmp, NULL);
		if (status != PJ_SUCCESS)
		    break;
	    }
	    break;

	case 'i':
	    /* Send instant messaeg */

	    /* i is for call index to send message, if any */
	    i = -1;
    
	    /* Make compiler happy. */
	    uri = NULL;

	    /* Input destination. */
	    ui_input_url("Send IM to", buf, sizeof(buf), &result);
	    if (result.nb_result != NO_NB) {

		if (result.nb_result == -1) {
		    puts("You can't send broadcast IM like that!");
		    continue;

		} else if (result.nb_result == 0) {
    
		    i = current_call;

		} else {
		    pjsua_buddy_info binfo;
		    pjsua_buddy_get_info(result.nb_result-1, &binfo);
		    uri = binfo.uri.ptr;
		}

	    } else if (result.uri_result) {
		uri = result.uri_result;
	    }
	    

	    /* Send typing indication. */
	    if (i != -1)
		pjsua_call_send_typing_ind(i, PJ_TRUE);
	    else {
		pj_str_t tmp_uri = pj_str(uri);
		pjsua_im_typing(current_acc, &tmp_uri, PJ_TRUE);
	    }

	    /* Input the IM . */
	    if (!simple_input("Message", text, sizeof(text))) {
		/*
		 * Cancelled.
		 * Send typing notification too, saying we're not typing.
		 */
		if (i != -1)
		    pjsua_call_send_typing_ind(i, PJ_FALSE);
		else {
		    pj_str_t tmp_uri = pj_str(uri);
		    pjsua_im_typing(current_acc, &tmp_uri, PJ_FALSE);
		}
		continue;
	    }

	    tmp = pj_str(text);

	    /* Send the IM */
	    if (i != -1)
		pjsua_call_send_im(i, &tmp);
	    else {
		pj_str_t tmp_uri = pj_str(uri);
		pjsua_im_send(current_acc, &tmp_uri, &tmp);
	    }

	    break;

	case 'a':

	    if (current_call != -1) {
		pjsua_call_get_info(current_call, &call_info);
	    } else {
		/* Make compiler happy */
		call_info.active = 0;
		call_info.role = PJSIP_ROLE_UAC;
		call_info.state = PJSIP_INV_STATE_DISCONNECTED;
	    }

	    if (current_call == -1 || 
		call_info.active==0 ||
		call_info.role != PJSIP_ROLE_UAS ||
		call_info.state >= PJSIP_INV_STATE_CONNECTING)
	    {
		puts("No pending incoming call");
		fflush(stdout);
		continue;

	    } else {
		if (!simple_input("Answer with code (100-699)", buf, sizeof(buf)))
		    continue;
		
		if (atoi(buf) < 100)
		    continue;

		/*
		 * Must check again!
		 * Call may have been disconnected while we're waiting for 
		 * keyboard input.
		 */
		if (current_call == -1) {
		    puts("Call has been disconnected");
		    fflush(stdout);
		    continue;
		}

		pjsua_call_answer(current_call, atoi(buf));
	    }

	    break;


	case 'h':

	    if (current_call == -1) {
		puts("No current call");
		fflush(stdout);
		continue;

	    } else if (menuin[1] == 'a') {
		
		/* Hangup all calls */
		pjsua_call_hangup_all();

	    } else {

		/* Hangup current calls */
		pjsua_call_hangup(current_call);
	    }
	    break;

	case ']':
	case '[':
	    /*
	     * Cycle next/prev dialog.
	     */
	    if (menuin[0] == ']') {
		find_next_call();

	    } else {
		find_prev_call();
	    }

	    if (current_call != -1) {
		
		pjsua_call_get_info(current_call, &call_info);
		PJ_LOG(3,(THIS_FILE,"Current dialog: %.*s", 
			  (int)call_info.remote_info.slen, 
			  call_info.remote_info.ptr));

	    } else {
		PJ_LOG(3,(THIS_FILE,"No current dialog"));
	    }
	    break;

	case 'H':
	    /*
	     * Hold call.
	     */
	    if (current_call != -1) {
		
		pjsua_call_set_hold(current_call);

	    } else {
		PJ_LOG(3,(THIS_FILE, "No current call"));
	    }
	    break;

	case 'v':
	    /*
	     * Send re-INVITE (to release hold, etc).
	     */
	    if (current_call != -1) {
		
		pjsua_call_reinvite(current_call);

	    } else {
		PJ_LOG(3,(THIS_FILE, "No current call"));
	    }
	    break;

	case 'x':
	    /*
	     * Transfer call.
	     */
	    if (current_call == -1) {
		
		PJ_LOG(3,(THIS_FILE, "No current call"));

	    } else {
		int call = current_call;

		ui_input_url("Transfer to URL", buf, sizeof(buf), &result);

		/* Check if call is still there. */

		if (call != current_call) {
		    puts("Call has been disconnected");
		    continue;
		}

		if (result.nb_result != NO_NB) {
		    if (result.nb_result == -1 || result.nb_result == 0)
			puts("You can't do that with transfer call!");
		    else {
			pjsua_buddy_info binfo;
			pjsua_buddy_get_info(result.nb_result-1, &binfo);
			pjsua_call_xfer( current_call,
					 &binfo.uri);
		    }

		} else if (result.uri_result) {
		    pj_str_t tmp;
		    tmp = pj_str(result.uri_result);
		    pjsua_call_xfer( current_call, &tmp);
		}
	    }
	    break;

	case '#':
	    /*
	     * Send DTMF strings.
	     */
	    if (current_call == -1) {
		
		PJ_LOG(3,(THIS_FILE, "No current call"));

	    } else if (!pjsua_call_has_media(current_call)) {

		PJ_LOG(3,(THIS_FILE, "Media is not established yet!"));

	    } else {
		pj_str_t digits;
		int call = current_call;
		pj_status_t status;

		if (!simple_input("DTMF strings to send (0-9*#A-B)", buf, 
				  sizeof(buf)))
		{
			break;
		}

		if (call != current_call) {
		    puts("Call has been disconnected");
		    continue;
		}

		digits = pj_str(buf);
		status = pjsua_call_dial_dtmf(current_call, &digits);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Unable to send DTMF", status);
		} else {
		    puts("DTMF digits enqueued for transmission");
		}
	    }
	    break;

	case 's':
	case 'u':
	    /*
	     * Subscribe/unsubscribe presence.
	     */
	    ui_input_url("(un)Subscribe presence of", buf, sizeof(buf), &result);
	    if (result.nb_result != NO_NB) {
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

	    break;

	case 'r':
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
	    break;
	    
	case 't':
	    pjsua_acc_get_info(current_acc, &acc_info);
	    acc_info.online_status = !acc_info.online_status;
	    pjsua_acc_set_online_status(current_acc, acc_info.online_status);
	    printf("Setting %s online status to %s\n",
		   acc_info.acc_id.ptr,
		   (acc_info.online_status?"online":"offline"));
	    break;

	case 'c':
	    switch (menuin[1]) {
	    case 'l':
		conf_list();
		break;
	    case 'c':
	    case 'd':
		{
		    char src_port[10], dst_port[10];
		    pj_status_t status;
		    const char *src_title, *dst_title;

		    conf_list();

		    src_title = (menuin[1]=='c'?
				 "Connect src port #":
				 "Disconnect src port #");
		    dst_title = (menuin[1]=='c'?
				 "To dst port #":
				 "From dst port #");

		    if (!simple_input(src_title, src_port, sizeof(src_port)))
			break;

		    if (!simple_input(dst_title, dst_port, sizeof(dst_port)))
			break;

		    if (menuin[1]=='c') {
			status = pjsua_conf_connect(atoi(src_port), 
						    atoi(dst_port));
		    } else {
			status = pjsua_conf_disconnect(atoi(src_port), 
						       atoi(dst_port));
		    }
		    if (status == PJ_SUCCESS) {
			puts("Success");
		    } else {
			puts("ERROR!!");
		    }
		}
		break;
	    }
	    break;

	case 'd':
	    if (menuin[1] == 'c') {
		char settings[2000];
		int len;

		len = pjsua_dump_settings(NULL, settings, 
					  sizeof(settings));
		if (len < 1)
		    PJ_LOG(3,(THIS_FILE, "Error: not enough buffer"));
		else
		    PJ_LOG(3,(THIS_FILE, 
			      "Dumping configuration (%d bytes):\n%s\n",
			      len, settings));
	    } else {
		pjsua_dump(menuin[1]=='d');
	    }
	    break;

	case 'q':
	    goto on_exit;

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


/*****************************************************************************
 * Error display:
 */

/*
 * Display error message for the specified error code.
 */
void pjsua_perror(const char *sender, const char *title, 
		  pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(1,(sender, "%s: %s [code=%d]", title, errmsg, status));
}



pjsua_callback console_callback = 
{
    &console_on_call_state,
    NULL,   /* on_incoming_call		*/
    NULL,   /* default accept transfer	*/
    &console_on_reg_state,
    &console_on_buddy_state,
    &console_on_pager,
    &console_on_typing,
};

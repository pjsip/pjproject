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
#include "pjsua.h"
#include <stdlib.h>


#define THIS_FILE	"main.c"

/* Current dialog */
static struct pjsua_inv_data *inv_session;

/*
 * Notify UI when invite state has changed.
 */
void pjsua_ui_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);

    PJ_LOG(3,(THIS_FILE, "INVITE session state changed to %s", 
	      pjsua_inv_state_names[inv->state]));

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
	if (inv == inv_session->inv) {
	    inv_session = inv_session->next;
	    if (inv_session == &pjsua.inv_list)
		inv_session = pjsua.inv_list.next;
	}

    } else {

	if (inv_session == &pjsua.inv_list || inv_session == NULL)
	    inv_session = inv->mod_data[pjsua.mod.id];

    }
}

/**
 * Notify UI when registration status has changed.
 */
void pjsua_ui_regc_on_state_changed(int code)
{
    PJ_UNUSED_ARG(code);

    // Log already written.
}


/*
 * Print buddy list.
 */
static void print_buddy_list(void)
{
    unsigned i;

    puts("Buddy list:");
    //puts("-------------------------------------------------------------------------------");
    if (pjsua.buddy_cnt == 0)
	puts(" -none-");
    else {
	for (i=0; i<pjsua.buddy_cnt; ++i) {
	    const char *status;

	    if (pjsua.buddies[i].sub == NULL || 
		pjsua.buddies[i].status.info_cnt==0)
	    {
		status = "   ?   ";
	    } 
	    else if (pjsua.buddies[i].status.info[0].basic_open)
		status = " Online";
	    else
		status = "Offline";

	    printf(" [%2d] <%s>  %s\n", 
		    i+1, status, pjsua.buddies[i].uri.ptr);
	}
    }
    puts("");
}

/*
 * Show a bit of help.
 */
static void keystroke_help(void)
{
    char reg_status[128];

    if (pjsua.regc == NULL) {
	pj_ansi_strcpy(reg_status, " -not registered to server-");
    } else if (pjsua.regc_last_err != PJ_SUCCESS) {
	pj_strerror(pjsua.regc_last_err, reg_status, sizeof(reg_status));
    } else if (pjsua.regc_last_code>=200 && pjsua.regc_last_code<=699) {

	pjsip_regc_info info;

	pjsip_regc_get_info(pjsua.regc, &info);

	pj_snprintf(reg_status, sizeof(reg_status),
		    "%s (%.*s;expires=%d)",
		    pjsip_get_status_text(pjsua.regc_last_code)->ptr,
		    (int)info.client_uri.slen,
		    info.client_uri.ptr,
		    info.next_reg);

    } else {
	pj_sprintf(reg_status, "in progress (%d)", pjsua.regc_last_code);
    }

    printf(">>>>\nRegistration status: %s\n", reg_status);
    printf("Online status: %s\n", 
	   (pjsua.online_status ? "Online" : "Invisible"));
    print_buddy_list();
    
    //puts("Commands:");
    puts("+=============================================================================+");
    puts("|       Call Commands:         |      IM & Presence:      |   Misc:           |");
    puts("|                              |                          |                   |");
    puts("|  m  Make new call            |  i  Send IM              |  o  Send OPTIONS  |");
    puts("|  a  Answer call              |  s  Subscribe presence   | rr  (Re-)register |");
    puts("|  h  Hangup call              |  u  Unsubscribe presence | ru  Unregister    |");
    puts("|  ]  Select next dialog       |  t  ToGgle Online status |  d  Dump status   |");
    puts("|  [  Select previous dialog   |                          |                   |");
    puts("|                              +--------------------------+-------------------+");
    puts("|  H  Hold call                |     Conference Command   |                   |");
    puts("|  v  re-inVite (release hold) | cl  List ports           |                   |");
    puts("|  x  Xfer call                | cc  Connect port         |                   |");
    puts("|  #  Send DTMF string         | cd  Disconnect port      |                   |");
    puts("+------------------------------+--------------------------+-------------------+");
    puts("|  q  QUIT                                                                    |");
    puts("+=============================================================================+");
    printf(">>> ");


    fflush(stdout);
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
	   , pjsua.buddy_cnt, pjsua.buddy_cnt);
    printf("%s: ", title);

    fflush(stdout);
    fgets(buf, len, stdin);
    len = strlen(buf);

    /* Left trim */
    while (isspace(*buf)) {
	++buf;
	--len;
    }

    /* Remove trailing newlines */
    while (len && (buf[len-1] == '\r' || buf[len-1] == '\n'))
	buf[--len] = '\0';

    if (len == 0 || buf[0]=='q')
	return;

    if (isdigit(*buf) || *buf=='-') {
	
	int i;
	
	if (*buf=='-')
	    i = 1;
	else
	    i = 0;

	for (; i<len; ++i) {
	    if (!isdigit(buf[i])) {
		puts("Invalid input");
		return;
	    }
	}

	result->nb_result = atoi(buf);

	if (result->nb_result > 0 && result->nb_result <= (int)pjsua.buddy_cnt) {
	    --result->nb_result;
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
    pjmedia_conf_port_info info[16];

    printf("Conference ports:\n");

    count = PJ_ARRAY_SIZE(info);
    pjmedia_conf_get_ports_info(pjsua.mconf, &count, info);
    for (i=0; i<count; ++i) {
	char txlist[80];
	unsigned j;
	pjmedia_conf_port_info *port_info = &info[i];	
	
	txlist[0] = '\0';
	for (j=0; j<pjsua.max_ports; ++j) {
	    char s[10];
	    if (port_info->listener[j]) {
		pj_sprintf(s, "#%d ", j);
		pj_ansi_strcat(txlist, s);
	    }
	}
	printf("Port #%02d %20.*s  transmitting to: %s\n", 
	       port_info->slot, 
	       (int)port_info->name.slen, 
	       port_info->name.ptr,
	       txlist);

    }
    puts("");
}


static void ui_console_main(void)
{
    char menuin[10];
    char buf[128];
    struct input_result result;

    //keystroke_help();

    for (;;) {

	keystroke_help();
	fgets(menuin, sizeof(menuin), stdin);

	switch (menuin[0]) {

	case 'm':
	    /* Make call! : */
	    if (pj_list_size(&pjsua.inv_list))
		printf("(You have %d calls)\n", pj_list_size(&pjsua.inv_list));
	    
	    ui_input_url("Make call", buf, sizeof(buf), &result);
	    if (result.nb_result != NO_NB) {
		if (result.nb_result == -1)
		    puts("You can't do that with make call!");
		else
		    pjsua_invite(pjsua.buddies[result.nb_result].uri.ptr, NULL);
	    } else if (result.uri_result)
		pjsua_invite(result.uri_result, NULL);
	    
	    break;


	case 'a':

	    if (inv_session == &pjsua.inv_list || 
		inv_session->inv->role != PJSIP_ROLE_UAS ||
		inv_session->inv->state >= PJSIP_INV_STATE_CONNECTING) 
	    {
		puts("No pending incoming call");
		fflush(stdout);
		continue;

	    } else {
		pj_status_t status;
		pjsip_tx_data *tdata;

		if (!simple_input("Answer with code (100-699)", buf, sizeof(buf)))
		    continue;
		
		if (atoi(buf) < 100)
		    continue;

		/*
		 * Must check again!
		 * Call may have been disconnected while we're waiting for 
		 * keyboard input.
		 */
		if (inv_session == &pjsua.inv_list) {
		    puts("Call has been disconnected");
		    fflush(stdout);
		    continue;
		}

		status = pjsip_inv_answer(inv_session->inv, atoi(buf), 
					  NULL, NULL, &tdata);
		if (status == PJ_SUCCESS)
		    status = pjsip_inv_send_msg(inv_session->inv, tdata, NULL);

		if (status != PJ_SUCCESS)
		    pjsua_perror(THIS_FILE, "Unable to create/send response", 
				 status);
	    }

	    break;


	case 'h':

	    if (inv_session == &pjsua.inv_list) {
		puts("No current call");
		fflush(stdout);
		continue;

	    } else {
		pjsua_inv_hangup(inv_session, PJSIP_SC_DECLINE);
	    }
	    break;

	case ']':
	case '[':
	    /*
	     * Cycle next/prev dialog.
	     */
	    if (menuin[0] == ']') {
		inv_session = inv_session->next;
		if (inv_session == &pjsua.inv_list)
		    inv_session = pjsua.inv_list.next;

	    } else {
		inv_session = inv_session->prev;
		if (inv_session == &pjsua.inv_list)
		    inv_session = pjsua.inv_list.prev;
	    }

	    if (inv_session != &pjsua.inv_list) {
		char url[PJSIP_MAX_URL_SIZE];
		int len;

		len = pjsip_uri_print(0, inv_session->inv->dlg->remote.info->uri,
				      url, sizeof(url)-1);
		if (len < 1) {
		    pj_ansi_strcpy(url, "<uri is too long>");
		} else {
		    url[len] = '\0';
		}

		PJ_LOG(3,(THIS_FILE,"Current dialog: %s", url));

	    } else {
		PJ_LOG(3,(THIS_FILE,"No current dialog"));
	    }
	    break;

	case 'H':
	    /*
	     * Hold call.
	     */
	    if (inv_session != &pjsua.inv_list) {
		
		pjsua_inv_set_hold(inv_session);

	    } else {
		PJ_LOG(3,(THIS_FILE, "No current call"));
	    }
	    break;

	case 'v':
	    /*
	     * Send re-INVITE (to release hold, etc).
	     */
	    if (inv_session != &pjsua.inv_list) {
		
		pjsua_inv_reinvite(inv_session);

	    } else {
		PJ_LOG(3,(THIS_FILE, "No current call"));
	    }
	    break;

	case 'x':
	    /*
	     * Transfer call.
	     */
	    if (inv_session == &pjsua.inv_list) {
		
		PJ_LOG(3,(THIS_FILE, "No current call"));

	    } else {
		struct pjsua_inv_data *cur = inv_session;

		ui_input_url("Transfer to URL", buf, sizeof(buf), &result);

		/* Check if call is still there. */

		if (cur != inv_session) {
		    puts("Call has been disconnected");
		    continue;
		}

		if (result.nb_result != NO_NB) {
		    if (result.nb_result == -1) 
			puts("You can't do that with transfer call!");
		    else
			pjsua_inv_xfer_call( inv_session,
					     pjsua.buddies[result.nb_result].uri.ptr);

		} else if (result.uri_result) {
		    pjsua_inv_xfer_call( inv_session, result.uri_result);
		}
	    }
	    break;

	case '#':
	    /*
	     * Send DTMF strings.
	     */
	    if (inv_session == &pjsua.inv_list) {
		
		PJ_LOG(3,(THIS_FILE, "No current call"));

	    } else if (inv_session->session == NULL) {

		PJ_LOG(3,(THIS_FILE, "Media is not established yet!"));

	    } else {
		pj_str_t digits;
		struct pjsua_inv_data *cur = inv_session;
		pj_status_t status;

		if (!simple_input("DTMF strings to send (0-9*#A-B)", buf, 
				  sizeof(buf)))
		{
			break;
		}

		if (cur != inv_session) {
		    puts("Call has been disconnected");
		    continue;
		}

		digits = pj_str(buf);
		status = pjmedia_session_dial_dtmf(inv_session->session, 0, 
						   &digits);
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
		    unsigned i;
		    for (i=0; i<pjsua.buddy_cnt; ++i)
			pjsua.buddies[i].monitor = (menuin[0]=='s');
		} else {
		    pjsua.buddies[result.nb_result].monitor = (menuin[0]=='s');
		}

		pjsua_pres_refresh();

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
		pjsua_regc_update(PJ_TRUE);
		break;
	    case 'u':
		/*
		 * Unregister
		 */
		pjsua_regc_update(PJ_FALSE);
		break;
	    }
	    break;
	    
	case 't':
	    pjsua.online_status = !pjsua.online_status;
	    pjsua_pres_refresh();
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
			status = pjmedia_conf_connect_port(pjsua.mconf, 
							   atoi(src_port), 
							   atoi(dst_port));
		    } else {
			status = pjmedia_conf_disconnect_port(pjsua.mconf, 
							      atoi(src_port), 
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
	    pjsua_dump();
	    break;

	case 'q':
	    goto on_exit;
	}
    }

on_exit:
    ;
}


/*****************************************************************************
 * This is a very simple PJSIP module, whose sole purpose is to display
 * incoming and outgoing messages to log. This module will have priority
 * higher than transport layer, which means:
 *
 *  - incoming messages will come to this module first before reaching
 *    transaction layer.
 *
 *  - outgoing messages will come to this module last, after the message
 *    has been 'printed' to contiguous buffer by transport layer and
 *    appropriate transport instance has been decided for this message.
 *
 */

/* Notification on incoming messages */
static pj_bool_t console_on_rx_msg(pjsip_rx_data *rdata)
{
    PJ_LOG(4,(THIS_FILE, "RX %d bytes %s from %s:%d:\n"
			 "%s\n"
			 "--end msg--",
			 rdata->msg_info.len,
			 pjsip_rx_data_get_info(rdata),
			 rdata->pkt_info.src_name,
			 rdata->pkt_info.src_port,
			 rdata->msg_info.msg_buf));
    
    /* Always return false, otherwise messages will not get processed! */
    return PJ_FALSE;
}

/* Notification on outgoing messages */
static pj_status_t console_on_tx_msg(pjsip_tx_data *tdata)
{
    
    /* Important note:
     *	tp_info field is only valid after outgoing messages has passed
     *	transport layer. So don't try to access tp_info when the module
     *	has lower priority than transport layer.
     */

    PJ_LOG(4,(THIS_FILE, "TX %d bytes %s to %s:%d:\n"
			 "%s\n"
			 "--end msg--",
			 (tdata->buf.cur - tdata->buf.start),
			 pjsip_tx_data_get_info(tdata),
			 tdata->tp_info.dst_name,
			 tdata->tp_info.dst_port,
			 tdata->buf.start));

    /* Always return success, otherwise message will not get sent! */
    return PJ_SUCCESS;
}

/* The module instance. */
static pjsip_module console_msg_logger = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-log", 13 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &console_on_rx_msg,			/* on_rx_request()	*/
    &console_on_rx_msg,			/* on_rx_response()	*/
    &console_on_tx_msg,			/* on_tx_request.	*/
    &console_on_tx_msg,			/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};



/*****************************************************************************
 * Console application custom logging:
 */


static FILE *log_file;


static void app_log_writer(int level, const char *buffer, int len)
{
    /* Write to both stdout and file. */

    if (level <= pjsua.app_log_level)
	pj_log_write(level, buffer, len);

    if (log_file) {
	fwrite(buffer, len, 1, log_file);
	fflush(log_file);
    }
}


void app_logging_init(void)
{
    /* Redirect log function to ours */

    pj_log_set_log_func( &app_log_writer );

    /* If output log file is desired, create the file: */

    if (pjsua.log_filename)
	log_file = fopen(pjsua.log_filename, "wt");
}


void app_logging_shutdown(void)
{
    /* Close logging file, if any: */

    if (log_file) {
	fclose(log_file);
	log_file = NULL;
    }
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




/*****************************************************************************
 * main():
 */
int main(int argc, char *argv[])
{

    /* Init default settings. */

    pjsua_default();


    /* Initialize pjsua (to create pool etc).
     */

    if (pjsua_init() != PJ_SUCCESS)
	return 1;


    /* Parse command line arguments: */

    if (pjsua_parse_args(argc, argv) != PJ_SUCCESS)
	return 1;


    /* Init logging: */

    app_logging_init();


    /* Register message logger to print incoming and outgoing
     * messages.
     */

    pjsip_endpt_register_module(pjsua.endpt, &console_msg_logger);


    /* Start pjsua! */

    if (pjsua_start() != PJ_SUCCESS) {

	pjsua_destroy();
	return 1;
    }


    /* Sleep for a while, let any messages get printed to console: */

    pj_thread_sleep(500);


    /* No current call initially: */

    inv_session = &pjsua.inv_list;


    /* Start UI console main loop: */

    ui_console_main();


    /* Destroy pjsua: */

    pjsua_destroy();


    /* Close logging: */

    app_logging_shutdown();


    /* Exit... */

    return 0;
}


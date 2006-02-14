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

static pjsip_inv_session *inv_session;

/*
 * Notify UI when invite state has changed.
 */
void pjsua_ui_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);

    PJ_LOG(3,(THIS_FILE, "INVITE session state changed to %s", 
	      pjsua_inv_state_names[inv->state]));

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
	if (inv == inv_session)
	    inv_session = NULL;

    } else {

	inv_session = inv;

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
 * Show a bit of help.
 */
static void ui_help(void)
{
    puts("");
    puts("Console keys:");
    puts("  m    Make a call/another call");
    puts("  d    Dump application states");
    puts("  a    Answer incoming call");
    puts("  h    Hangup current call");
    puts("  q    Quit");
    puts("");
    fflush(stdout);
}

static pj_bool_t input(const char *title, char *buf, pj_size_t len)
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

static void ui_console_main(void)
{
    char buf[128];
    pjsip_inv_session *inv;

    //ui_help();

    for (;;) {

	ui_help();
	fgets(buf, sizeof(buf), stdin);

	switch (buf[0]) {

	case 'm':
	    if (inv_session != NULL) {
		puts("Can not make call while another one is in progress");
		fflush(stdout);
		continue;
	    }

#if 1
	    /* Make call! : */
	    if (!input("Enter URL to call", buf, sizeof(buf)))
		continue;
	    pjsua_invite(buf, &inv);

#else

	    pjsua_invite("sip:localhost:5061", &inv);
#endif
	    break;


	case 'd':
	    pjsua_dump();
	    break;

	case 'a':

	    if (inv_session == NULL || inv_session->role != PJSIP_ROLE_UAS ||
		inv_session->state >= PJSIP_INV_STATE_CONNECTING) 
	    {
		puts("No pending incoming call");
		fflush(stdout);
		continue;

	    } else {
		pj_status_t status;
		pjsip_tx_data *tdata;

		if (!input("Answer with code (100-699)", buf, sizeof(buf)))
		    continue;
		
		if (atoi(buf) < 100)
		    continue;

		status = pjsip_inv_answer(inv_session, atoi(buf), NULL, NULL, 
					  &tdata);
		if (status == PJ_SUCCESS)
		    status = pjsip_inv_send_msg(inv_session, tdata, NULL);

		if (status != PJ_SUCCESS)
		    pjsua_perror("Unable to create/send response", status);
	    }

	    break;

	case 'h':

	    if (inv_session == NULL) {
		puts("No current call");
		fflush(stdout);
		continue;

	    } else {
		pj_status_t status;
		pjsip_tx_data *tdata;

		status = pjsip_inv_end_session(inv_session, PJSIP_SC_DECLINE, 
					       NULL, &tdata);
		if (status != PJ_SUCCESS) {
		    pjsua_perror("Failed to create end session message", status);
		    continue;
		}

		status = pjsip_inv_send_msg(inv_session, tdata, NULL);
		if (status != PJ_SUCCESS) {
		    pjsua_perror("Failed to send end session message", status);
		    continue;
		}
	    }

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
    { "mod-console-msg-logger", 22 },	/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* User data.		*/
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
void pjsua_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(1,(THIS_FILE, "%s: %s [code=%d]", title, errmsg, status));
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


    /* Start UI console main loop: */

    ui_console_main();


    /* Destroy pjsua: */

    pjsua_destroy();


    /* Close logging: */

    app_logging_shutdown();


    /* Exit... */

    return 0;
}


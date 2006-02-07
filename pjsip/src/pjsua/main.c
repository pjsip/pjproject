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

/* For debugging, disable threading. */
//#define NO_WORKER_THREAD

#ifdef NO_WORKER_THREAD
#include <conio.h>
#endif

#define THIS_FILE	"main.c"

static pjsip_inv_session *inv_session;

/*
 * Notify UI when invite state has changed.
 */
void ui_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
    const char *state_names[] =
    {
	"NULL",
	"CALLING",
	"INCOMING",
	"EARLY",
	"CONNECTING",
	"CONFIRMED",
	"DISCONNECTED",
	"TERMINATED",
    };

    PJ_UNUSED_ARG(e);

    PJ_LOG(3,(THIS_FILE, "INVITE session state changed to %s", state_names[inv->state]));

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED ||
	inv->state == PJSIP_INV_STATE_TERMINATED)
    {
	if (inv == inv_session)
	    inv_session = NULL;

    } else {

	inv_session = inv;

    }
}

static void ui_help(void)
{
    puts("");
    puts("Console keys:");
    puts("  m    Make a call");
    puts("  h    Hangup current call");
    puts("  q    Quit");
    puts("");
}

static void ui_console_main(void)
{
    char keyin[10];
    char buf[128];
    char *p;
    pjsip_inv_session *inv;

    //ui_help();

    for (;;) {

#ifdef NO_WORKER_THREAD
	pj_time_val timeout = { 0, 10 };
	pjsip_endpt_handle_events (pjsua.endpt, &timeout);

	if (kbhit())
	    fgets(keyin, sizeof(keyin), stdin);
#else
	ui_help();
	fgets(keyin, sizeof(keyin), stdin);
#endif

	switch (keyin[0]) {

	case 'm':
	    if (inv_session != NULL) {
		puts("Can not make call while another one is in progress");
		continue;
	    }

#if 0
	    printf("Enter URL to call: ");
	    fgets(buf, sizeof(buf), stdin);

	    if (buf[0]=='\r' || buf[0]=='\n') {
		/* Cancelled. */
		puts("<cancelled>");
		continue;
	    }

	    /* Remove trailing newlines. */
	    for (p=buf; ; ++p) {
		if (*p=='\r' || *p=='\n') *p='\0';
		else if (!*p) break;
	    }
	    /* Make call! : */

	    pjsua_invite(buf, &inv);
#endif

	    pjsua_invite("sip:localhost:5061", &inv);
	    break;


	case 'h':

	    if (inv_session == NULL) {
		puts("No current call");
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
    
    /* Must return false for logger! */
    return PJ_FALSE;
}

static pj_status_t console_on_tx_msg(pjsip_tx_data *tdata)
{
    PJ_LOG(4,(THIS_FILE, "TX %d bytes %s to %s:%d:\n"
			 "%s\n"
			 "--end msg--",
			 (tdata->buf.cur - tdata->buf.start),
			 pjsip_tx_data_get_info(tdata),
			 tdata->tp_info.dst_name,
			 tdata->tp_info.dst_port,
			 tdata->buf.start));

    return PJ_SUCCESS;
}

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


int main()
{
    /* Init default settings. */

    pjsua_default();


#ifdef NO_WORKER_THREAD
    pjsua.thread_cnt = 0;
#endif


    /* Initialize pjsua.
     * This will start worker thread, client registration, etc.
     */

    if (pjsua_init() != PJ_SUCCESS)
	return 1;

    /* Register message logger to print incoming and outgoing
     * messages.
     */

    pjsip_endpt_register_module(pjsua.endpt, &console_msg_logger);


    /* Sleep for a while, let any messages get printed to console: */

    pj_thread_sleep(500);


    /* Start UI console main loop: */

    ui_console_main();


    /* Destroy pjsua: */

    pjsua_destroy();

    /* Exit... */

    return 0;
}


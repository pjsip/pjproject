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
#include "getopt.h"
#include <stdlib.h>


#define THIS_FILE	"main.c"

static pjsip_inv_session *inv_session;

/*
 * Notify UI when invite state has changed.
 */
void pjsua_ui_inv_on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
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

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
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
 * Command line argument processing:
 */


/* Show usage */
static void usage(void)
{
    puts("Usage:");
    puts("  pjsua [options] [sip-url]");
    puts("");
    puts("  [sip-url]   Default URL to invite.");
    puts("");
    puts("General options:");
    puts("  --config-file=file  Read the config/arguments from file.");
    puts("  --log-file=fname    Log to filename (default stderr)");
    puts("  --log-level=N       Set log max level to N (0(none) to 6(trace))");
    puts("  --app-log-level=N   Set log max level for stdout display to N");
    puts("  --help              Display this help screen");
    puts("  --version           Display version info");
    puts("");
    puts("Media options:");
    puts("  --null-audio        Use NULL audio device");
    puts("");
    puts("User Agent options:");
    puts("  --auto-answer=sec   Auto-answer all incoming calls after sec seconds.");
    puts("  --auto-hangup=sec   Auto-hangup all calls after sec seconds.");
    puts("");
    puts("SIP options:");
    puts("  --local-port=port   Set TCP/UDP port");
    puts("  --id=url            Set the URL of local ID (used in From header)");
    puts("  --contact=url       Override the Contact information");
    puts("  --proxy=url         Set the URL of proxy server");
    puts("  --outbound=url      Set the URL of outbound proxy server");
    puts("  --registrar=url     Set the URL of registrar server");
    puts("  --reg-timeout=secs  Set registration interval to secs (default 3600)");
    puts("");
    puts("Authentication options:");
    puts("  --realm=string      Set realm");
    puts("  --username=string   Set authentication username");
    puts("  --password=string   Set authentication password");
    puts("");
    puts("STUN options (all must be specified):");
    puts("  --use-stun1=host[:port]");
    puts("  --use-stun2=host[:port]  Use STUN and set host name and port of STUN servers");
    puts("");
    puts("SIMPLE options (may be specified more than once):");
    puts("  --add-buddy url     Add the specified URL to the buddy list.");
    puts("  --offer-x-ms-msg    Offer \"x-ms-message\" in outgoing INVITE");
    puts("  --no-presence	Do not subscribe presence of buddies");
    puts("");
    fflush(stdout);
}


/*
 * Verify that valid SIP url is given.
 */
static pj_status_t verify_sip_url(char *url)
{
    pjsip_uri *p;
    pj_pool_t *pool;
    int len = (url ? strlen(url) : 0);

    if (!len) return -1;

    pool = pj_pool_create(&pjsua.cp.factory, "check%p", 1024, 0, NULL);
    if (!pool) return -1;

    p = pjsip_parse_uri(pool, url, len, 0);
    if (!p || pj_stricmp2(pjsip_uri_get_scheme(p), "sip") != 0)
	p = NULL;

    pj_pool_release(pool);
    return p ? 0 : -1;
}


/*
 * Read command arguments from config file.
 */
static int read_config_file(pj_pool_t *pool, const char *filename, 
			    int *app_argc, char ***app_argv)
{
    int i;
    FILE *fhnd;
    char line[200];
    int argc = 0;
    char **argv;
    enum { MAX_ARGS = 64 };

    /* Allocate MAX_ARGS+1 (argv needs to be terminated with NULL argument) */
    argv = pj_pool_calloc(pool, MAX_ARGS+1, sizeof(char*));
    argv[argc++] = *app_argv[0];

    /* Open config file. */
    fhnd = fopen(filename, "rt");
    if (!fhnd) {
	printf("Unable to open config file %s\n", filename);
	fflush(stdout);
	return -1;
    }

    /* Scan tokens in the file. */
    while (argc < MAX_ARGS && !feof(fhnd)) {
	char *token, *p = line;

	if (fgets(line, sizeof(line), fhnd) == NULL) break;

	for (token = strtok(p, " \t\r\n"); argc < MAX_ARGS; 
	     token = strtok(NULL, " \t\r\n"))
	{
	    int token_len;
	    
	    if (!token) break;
	    if (*token == '#') break;

	    token_len = strlen(token);
	    if (!token_len)
		continue;
	    argv[argc] = pj_pool_alloc(pool, token_len+1);
	    pj_memcpy(argv[argc], token, token_len+1);
	    ++argc;
	}
    }

    /* Copy arguments from command line */
    for (i=1; i<*app_argc && argc < MAX_ARGS; ++i)
	argv[argc++] = (*app_argv)[i];

    if (argc == MAX_ARGS && (i!=*app_argc || !feof(fhnd))) {
	printf("Too many arguments specified in cmd line/config file\n");
	fflush(stdout);
	fclose(fhnd);
	return -1;
    }

    fclose(fhnd);

    /* Assign the new command line back to the original command line. */
    *app_argc = argc;
    *app_argv = argv;
    return 0;

}


/* Parse arguments. */
static pj_status_t parse_args(int argc, char *argv[])
{
    int c;
    int option_index;
    enum { OPT_CONFIG_FILE, OPT_LOG_FILE, OPT_LOG_LEVEL, OPT_APP_LOG_LEVEL, 
	   OPT_HELP, OPT_VERSION, OPT_NULL_AUDIO,
	   OPT_LOCAL_PORT, OPT_PROXY, OPT_OUTBOUND_PROXY, OPT_REGISTRAR,
	   OPT_REG_TIMEOUT, OPT_ID, OPT_CONTACT, 
	   OPT_REALM, OPT_USERNAME, OPT_PASSWORD,
	   OPT_USE_STUN1, OPT_USE_STUN2, 
	   OPT_ADD_BUDDY, OPT_OFFER_X_MS_MSG, OPT_NO_PRESENCE,
	   OPT_AUTO_ANSWER, OPT_AUTO_HANGUP};
    struct option long_options[] = {
	{ "config-file",1, 0, OPT_CONFIG_FILE},
	{ "log-file",	1, 0, OPT_LOG_FILE},
	{ "log-level",	1, 0, OPT_LOG_LEVEL},
	{ "app-log-level",1,0,OPT_APP_LOG_LEVEL},
	{ "help",	0, 0, OPT_HELP},
	{ "version",	0, 0, OPT_VERSION},
	{ "null-audio", 0, 0, OPT_NULL_AUDIO},
	{ "local-port", 1, 0, OPT_LOCAL_PORT},
	{ "proxy",	1, 0, OPT_PROXY},
	{ "outbound",	1, 0, OPT_OUTBOUND_PROXY},
	{ "registrar",	1, 0, OPT_REGISTRAR},
	{ "reg-timeout",1, 0, OPT_REG_TIMEOUT},
	{ "id",		1, 0, OPT_ID},
	{ "contact",	1, 0, OPT_CONTACT},
	{ "realm",	1, 0, OPT_REALM},
	{ "username",	1, 0, OPT_USERNAME},
	{ "password",	1, 0, OPT_PASSWORD},
	{ "use-stun1",  1, 0, OPT_USE_STUN1},
	{ "use-stun2",  1, 0, OPT_USE_STUN2},
	{ "add-buddy",  1, 0, OPT_ADD_BUDDY},
	{ "offer-x-ms-msg",0,0,OPT_OFFER_X_MS_MSG},
	{ "no-presence", 0, 0, OPT_NO_PRESENCE},
	{ "auto-answer",1, 0, OPT_AUTO_ANSWER},
	{ "auto-hangup",1, 0, OPT_AUTO_HANGUP},
	{ NULL, 0, 0, 0}
    };
    pj_status_t status;
    char *config_file = NULL;

    /* Run getopt once to see if user specifies config file to read. */
    while ((c=getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
	switch (c) {
	case OPT_CONFIG_FILE:
	    config_file = optarg;
	    break;
	}
	if (config_file)
	    break;
    }

    if (config_file) {
	status = read_config_file(pjsua.pool, config_file, &argc, &argv);
	if (status != 0)
	    return status;
    }


    /* Reinitialize and re-run getopt again, possibly with new arguments
     * read from config file.
     */
    optind = 0;
    while ((c=getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
	char *p;
	pj_str_t tmp;
	long lval;

	switch (c) {

	case OPT_LOG_FILE:
	    pjsua.log_filename = optarg;
	    break;

	case OPT_LOG_LEVEL:
	    c = pj_strtoul(pj_cstr(&tmp, optarg));
	    if (c < 0 || c > 6) {
		printf("Error: expecting integer value 0-6 for --log-level\n");
		return PJ_EINVAL;
	    }
	    pj_log_set_level( c );
	    break;

	case OPT_APP_LOG_LEVEL:
	    pjsua.app_log_level = pj_strtoul(pj_cstr(&tmp, optarg));
	    if (pjsua.app_log_level < 0 || pjsua.app_log_level > 6) {
		printf("Error: expecting integer value 0-6 for --app-log-level\n");
		return PJ_EINVAL;
	    }
	    break;

	case OPT_HELP:
	    usage();
	    return PJ_EINVAL;

	case OPT_VERSION:   /* version */
	    pj_dump_config();
	    return PJ_EINVAL;

	case OPT_NULL_AUDIO:
	    pjsua.null_audio = 1;
	    break;

	case OPT_LOCAL_PORT:   /* local-port */
	    lval = pj_strtoul(pj_cstr(&tmp, optarg));
	    if (lval < 1 || lval > 65535) {
		printf("Error: expecting integer value for --local-port\n");
		return PJ_EINVAL;
	    }
	    pjsua.sip_port = (pj_uint16_t)lval;
	    break;

	case OPT_PROXY:   /* proxy */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in proxy argument\n", optarg);
		return PJ_EINVAL;
	    }
	    pjsua.proxy = pj_str(optarg);
	    break;

	case OPT_OUTBOUND_PROXY:   /* outbound proxy */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in outbound proxy argument\n", optarg);
		return PJ_EINVAL;
	    }
	    pjsua.outbound_proxy = pj_str(optarg);
	    break;

	case OPT_REGISTRAR:   /* registrar */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in registrar argument\n", optarg);
		return PJ_EINVAL;
	    }
	    pjsua.registrar_uri = pj_str(optarg);
	    break;

	case OPT_REG_TIMEOUT:   /* reg-timeout */
	    pjsua.reg_timeout = pj_strtoul(pj_cstr(&tmp,optarg));
	    if (pjsua.reg_timeout < 1 || pjsua.reg_timeout > 3600) {
		printf("Error: invalid value for --reg-timeout (expecting 1-3600)\n");
		return PJ_EINVAL;
	    }
	    break;

	case OPT_ID:   /* id */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in local id argument\n", optarg);
		return PJ_EINVAL;
	    }
	    pjsua.local_uri = pj_str(optarg);
	    break;

	case OPT_CONTACT:   /* contact */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in contact argument\n", optarg);
		return PJ_EINVAL;
	    }
	    pjsua.contact_uri = pj_str(optarg);
	    break;

	case OPT_USERNAME:   /* Default authentication user */
	    if (!pjsua.cred_count) pjsua.cred_count = 1;
	    pjsua.cred_info[0].username = pj_str(optarg);
	    break;

	case OPT_REALM:	    /* Default authentication realm. */
	    if (!pjsua.cred_count) pjsua.cred_count = 1;
	    pjsua.cred_info[0].realm = pj_str(optarg);
	    break;

	case OPT_PASSWORD:   /* authentication password */
	    if (!pjsua.cred_count) pjsua.cred_count = 1;
	    pjsua.cred_info[0].data_type = 0;
	    pjsua.cred_info[0].data = pj_str(optarg);
	    break;

	case OPT_USE_STUN1:   /* STUN server 1 */
	    p = pj_native_strchr(optarg, ':');
	    if (p) {
		*p = '\0';
		pjsua.stun_srv1 = pj_str(optarg);
		pjsua.stun_port1 = pj_strtoul(pj_cstr(&tmp, p+1));
		if (pjsua.stun_port1 < 1 || pjsua.stun_port1 > 65535) {
		    printf("Error: expecting port number with option --use-stun1\n");
		    return PJ_EINVAL;
		}
	    } else {
		pjsua.stun_port1 = 3478;
		pjsua.stun_srv1 = pj_str(optarg);
	    }
	    break;

	case OPT_USE_STUN2:   /* STUN server 2 */
	    p = pj_native_strchr(optarg, ':');
	    if (p) {
		*p = '\0';
		pjsua.stun_srv2 = pj_str(optarg);
		pjsua.stun_port2 = pj_strtoul(pj_cstr(&tmp,p+1));
		if (pjsua.stun_port2 < 1 || pjsua.stun_port2 > 65535) {
		    printf("Error: expecting port number with option --use-stun2\n");
		    return PJ_EINVAL;
		}
	    } else {
		pjsua.stun_port2 = 3478;
		pjsua.stun_srv2 = pj_str(optarg);
	    }
	    break;
	}
    }

    if (optind != argc) {
	printf("Error: unknown options %s\n", argv[optind]);
	return PJ_EINVAL;
    }

    if (pjsua.reg_timeout == 0)
	pjsua.reg_timeout = 3600;


    return PJ_SUCCESS;
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

    if (parse_args(argc, argv) != PJ_SUCCESS)
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


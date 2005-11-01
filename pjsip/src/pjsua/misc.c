/* $Header: /pjproject/pjsip/src/pjsua/misc.c 21    6/23/05 12:36a Bennylp $ */

/*
 * THIS FILE IS INCLUDED BY main.c.
 * IT WON'T COMPILE BY ITSELF.
 */

#include "getopt.h"
#include <stdio.h>
 

/*
 * Display program usage
 */
static void usage()
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

/* Display keystroke help. */
static void keystroke_help()
{
    int i;

    printf("Advertise status as: %s\n", (global.hide_status ? "Offline" : "Online"));
    puts("");
    puts("Buddy list:");
    puts("-------------------------------------------------------------------------------");
    for (i=0; i<global.buddy_cnt; ++i) {
	printf(" %d\t%s  <%s>\n", i+1, global.buddy[i].ptr,
		(global.buddy_status[i]?"Online":"Offline"));
    }
    //printf("-------------------------------------\n");
    puts("");
    //puts("Commands:");
    puts("+=============================================================================+");
    puts("|       Call Commands:         |      IM & Presence:      |   Misc:           |");
    puts("|                              |                          |                   |");
    puts("|  m  Make new call            |  i  Send IM              |  o  Send OPTIONS  |");
    puts("|  a  Answer call              | su  Subscribe presence   |  d  Dump status   |");
    puts("|  h  Hangup call              | us  Unsubscribe presence |  d1 Dump detailed |");
    puts("|  ]  Select next dialog       |  t  Toggle Online status |                   |");
    puts("|  [  Select previous dialog   |                          |                   |");
    puts("+-----------------------------------------------------------------------------+");
    puts("|  q  QUIT                                                                    |");
    puts("+=============================================================================+");
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

    pool = pj_pool_create(global.pf, "check%p", 1024, 0, NULL);
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
	fclose(fhnd);
	return -1;
    }

    fclose(fhnd);

    /* Assign the new command line back to the original command line. */
    *app_argc = argc;
    *app_argv = argv;
    return 0;

}

/*
 * Parse program arguments
 */
static int parse_args(pj_pool_t *pool, int argc, char *argv[])
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
    char *config_file = NULL;

    /* Run getopt once to see if user specifies config file to read. */
    while ((c=getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
	switch (c) {
	case 0:
	    config_file = optarg;
	    break;
	}
	if (config_file)
	    break;
    }

    if (config_file) {
	if (read_config_file(pool, config_file, &argc, &argv) != 0)
	    return -1;
    }

    /* Reinitialize and re-run getopt again, possibly with new arguments
     * read from config file.
     */
    optind = 0;
    while ((c=getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
	char *err, *p;

	switch (c) {
	case OPT_LOG_FILE:
	    global.log_filename = optarg;
	    break;
	case OPT_LOG_LEVEL:
	    c = strtoul(optarg, &err, 10);
	    if (*err) {
		printf("Error: expecting integer value 0-6 for --log-level\n");
		return -1;
	    }
	    pj_log_set_level( c );
	    break;
	case OPT_APP_LOG_LEVEL:
	    global.app_log_level = strtoul(optarg, &err, 10);
	    if (*err) {
		printf("Error: expecting integer value 0-6 for --app-log-level\n");
		return -1;
	    }
	    break;
	case OPT_HELP:
	    usage();
	    return -1;
	case OPT_VERSION:   /* version */
	    pj_dump_config();
	    return -1;
	case OPT_NULL_AUDIO:
	    global.null_audio = 1;
	    break;
	case OPT_LOCAL_PORT:   /* local-port */
	    global.sip_port = strtoul(optarg, &err, 10);
	    if (*err) {
		printf("Error: expecting integer value for --local-port\n");
		return -1;
	    }
	    break;
	case OPT_PROXY:   /* proxy */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in proxy argument\n", optarg);
		return -1;
	    }
	    global.proxy = pj_str(optarg);
	    break;
	case OPT_OUTBOUND_PROXY:   /* outbound proxy */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in outbound proxy argument\n", optarg);
		return -1;
	    }
	    global.outbound_proxy = pj_str(optarg);
	    break;
	case OPT_REGISTRAR:   /* registrar */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in registrar argument\n", optarg);
		return -1;
	    }
	    global.registrar_uri = pj_str(optarg);
	    break;
	case OPT_REG_TIMEOUT:   /* reg-timeout */
	    global.reg_timeout = strtoul(optarg, &err, 10);
	    if (*err) {
		printf("Error: expecting integer value for --reg-timeout\n");
		return -1;
	    }
	    break;
	case OPT_ID:   /* id */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in local id argument\n", optarg);
		return -1;
	    }
	    global.local_uri = pj_str(optarg);
	    break;
	case OPT_CONTACT:   /* contact */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in contact argument\n", optarg);
		return -1;
	    }
	    global.contact = pj_str(optarg);
	    break;
	case OPT_USERNAME:   /* Default authentication user */
	    if (!global.cred_count) global.cred_count = 1;
	    global.cred_info[0].username = pj_str(optarg);
	    break;
	case OPT_REALM:	    /* Default authentication realm. */
	    if (!global.cred_count) global.cred_count = 1;
	    global.cred_info[0].realm = pj_str(optarg);
	    break;
	case OPT_PASSWORD:   /* authentication password */
	    if (!global.cred_count) global.cred_count = 1;
	    global.cred_info[0].data_type = 0;
	    global.cred_info[0].data = pj_str(optarg);
	    break;
	case OPT_USE_STUN1:   /* STUN server 1 */
	    p = strchr(optarg, ':');
	    if (p) {
		*p = '\0';
		global.stun_srv1 = pj_str(optarg);
		global.stun_port1 = strtoul(p+1, &err, 10);
		if (*err || global.stun_port1==0) {
		    printf("Error: expecting port number with option --use-stun1\n");
		    return -1;
		}
	    } else {
		global.stun_port1 = 3478;
		global.stun_srv1 = pj_str(optarg);
	    }
	    break;
	case OPT_USE_STUN2:   /* STUN server 2 */
	    p = strchr(optarg, ':');
	    if (p) {
		*p = '\0';
		global.stun_srv2 = pj_str(optarg);
		global.stun_port2 = strtoul(p+1, &err, 10);
		if (*err || global.stun_port2==0) {
		    printf("Error: expecting port number with option --use-stun2\n");
		    return -1;
		}
	    } else {
		global.stun_port2 = 3478;
		global.stun_srv2 = pj_str(optarg);
	    }
	    break;
	case OPT_ADD_BUDDY: /* Add to buddy list. */
	    if (verify_sip_url(optarg) != 0) {
		printf("Error: invalid URL '%s' in --add-buddy option\n", optarg);
		return -1;
	    }
	    if (global.buddy_cnt == MAX_BUDDIES) {
		printf("Error: too many buddies in buddy list.\n");
		return -1;
	    }
	    global.buddy[global.buddy_cnt++] = pj_str(optarg);
	    break;
	case OPT_OFFER_X_MS_MSG:
	    global.offer_x_ms_msg = 1;
	    break;
	case OPT_NO_PRESENCE:
	    global.no_presence = 1;
	    break;
	case OPT_AUTO_ANSWER:
	    global.auto_answer = strtoul(optarg, &err, 10);
	    if (*err) {
		printf("Error: expecting integer value for --auto-answer option\n");
		return -1;
	    }
	    break;
	case OPT_AUTO_HANGUP:
	    global.auto_hangup = strtoul(optarg, &err, 10);
	    if (*err) {
		printf("Error: expecting integer value for --auto-hangup option\n");
		return -1;
	    }
	    break;
	}
    }

    if (optind != argc) {
	printf("Error: unknown options %s\n", argv[optind]);
	return -1;
    }

    if (global.reg_timeout == 0)
	global.reg_timeout = 3600;

    return 0;
}

/* Print dialog. */
static void print_dialog(pjsip_dlg *dlg)
{
    if (!dlg) {
	puts("none");
	return;
    }

    printf("%s: call-id=%.*s", dlg->obj_name, 
			       (int)dlg->call_id->id.slen, 
			       dlg->call_id->id.ptr);

    printf(" (%s, %s)\n", pjsip_role_name(dlg->role),
			  pjsip_dlg_state_str(dlg->state));
}

/* Dump media statistic */
void dump_media_statistic(pjsip_dlg *dlg)
{
    struct dialog_data *dlg_data = dlg->user_data;
    pj_media_stream_stat stat[2];
    const char *statname[2] = { "TX", "RX" };
    int i;

    pj_media_session_get_stat (dlg_data->msession, 0, &stat[0], &stat[1]);

    printf("Media statistic:\n");
    for (i=0; i<2; ++i) {
	printf("  %s statistics:\n", statname[i]);
	printf("    Pkt      TX=%d RX=%d\n", stat[i].pkt_tx, stat[i].pkt_rx);
	printf("    Octets   TX=%d RX=%d\n", stat[i].oct_tx, stat[i].oct_rx);
	printf("    Jitter   %d ms\n", stat[i].jitter);
	printf("    Pkt lost %d\n", stat[i].pkt_lost);
    }
    printf("\n");
}

/* Print all dialogs. */
static void print_all_dialogs()
{
    pjsip_dlg *dlg = (pjsip_dlg *)global.user_agent->dlg_list.next;

    puts("List all dialogs:");

    while (dlg != (pjsip_dlg *) &global.user_agent->dlg_list) {
	printf("%c", (dlg==global.cur_dlg ? '*' : ' '));
	print_dialog(dlg);
	dlg = dlg->next;
    }

    puts("");
}


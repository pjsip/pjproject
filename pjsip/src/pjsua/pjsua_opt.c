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

#define THIS_FILE   "pjsua_opt.c"


const char *pjsua_inv_state_names[] =
{
    "NULL      ",
    "CALLING   ",
    "INCOMING  ",
    "EARLY     ",
    "CONNECTING",
    "CONFIRMED ",
    "DISCONNCTD",
    "TERMINATED",
};



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
    //puts("");
    //puts("User Agent options:");
    //puts("  --auto-answer=sec   Auto-answer all incoming calls after sec seconds.");
    //puts("  --auto-hangup=sec   Auto-hangup all calls after sec seconds.");
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
    //puts("  --offer-x-ms-msg    Offer \"x-ms-message\" in outgoing INVITE");
    //puts("  --no-presence	Do not subscribe presence of buddies");
    puts("");
    fflush(stdout);
}



/*
 * Verify that valid SIP url is given.
 */
pj_status_t pjsua_verify_sip_url(const char *c_url)
{
    pjsip_uri *p;
    pj_pool_t *pool;
    char *url;
    int len = (c_url ? pj_ansi_strlen(c_url) : 0);

    if (!len) return -1;

    pool = pj_pool_create(&pjsua.cp.factory, "check%p", 1024, 0, NULL);
    if (!pool) return -1;

    url = pj_pool_alloc(pool, len+1);
    pj_ansi_strcpy(url, c_url);

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
pj_status_t pjsua_parse_args(int argc, char *argv[])
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
	    if (pjsua_verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in proxy argument\n", optarg);
		return PJ_EINVAL;
	    }
	    pjsua.proxy = pj_str(optarg);
	    break;

	case OPT_OUTBOUND_PROXY:   /* outbound proxy */
	    if (pjsua_verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in outbound proxy argument\n", optarg);
		return PJ_EINVAL;
	    }
	    pjsua.outbound_proxy = pj_str(optarg);
	    break;

	case OPT_REGISTRAR:   /* registrar */
	    if (pjsua_verify_sip_url(optarg) != 0) {
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
	    if (pjsua_verify_sip_url(optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in local id argument\n", optarg);
		return PJ_EINVAL;
	    }
	    pjsua.local_uri = pj_str(optarg);
	    break;

	case OPT_CONTACT:   /* contact */
	    if (pjsua_verify_sip_url(optarg) != 0) {
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
	    p = pj_ansi_strchr(optarg, ':');
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
	    p = pj_ansi_strchr(optarg, ':');
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

	case OPT_ADD_BUDDY: /* Add to buddy list. */
	    if (pjsua_verify_sip_url(optarg) != 0) {
		printf("Error: invalid URL '%s' in --add-buddy option\n", optarg);
		return -1;
	    }
	    if (pjsua.buddy_cnt == PJSUA_MAX_BUDDIES) {
		printf("Error: too many buddies in buddy list.\n");
		return -1;
	    }
	    pjsua.buddies[pjsua.buddy_cnt++] = pj_str(optarg);
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



static void print_invite_session(const char *title,
				 struct pjsua_inv_data *inv_data, 
				 char *buf, pj_size_t size)
{
    int len;
    pjsip_inv_session *inv = inv_data->inv;
    pjsip_dialog *dlg = inv->dlg;
    char userinfo[128];

    /* Dump invite sesion info. */

    len = pjsip_hdr_print_on(dlg->remote.info, userinfo, sizeof(userinfo));
    if (len < 1)
	pj_ansi_strcpy(userinfo, "<--uri too long-->");
    else
	userinfo[len] = '\0';
    
    len = pj_snprintf(buf, size, "%s[%s] %s",
		      title,
		      pjsua_inv_state_names[inv->state],
		      userinfo);
    if (len < 1 || len >= (int)size) {
	pj_ansi_strcpy(buf, "<--uri too long-->");
	len = 18;
    } else
	buf[len] = '\0';
}

static void dump_media_session(pjmedia_session *session)
{
    unsigned i;
    pjmedia_session_info info;

    pjmedia_session_get_info(session, &info);

    for (i=0; i<info.stream_cnt; ++i) {
	pjmedia_stream_stat strm_stat;
	const char *rem_addr;
	int rem_port;
	const char *dir;

	pjmedia_session_get_stream_stat(session, i, &strm_stat);
	rem_addr = pj_inet_ntoa(info.stream_info[i].rem_addr.sin_addr);
	rem_port = pj_ntohs(info.stream_info[i].rem_addr.sin_port);

	if (info.stream_info[i].dir == PJMEDIA_DIR_ENCODING)
	    dir = "sendonly";
	else if (info.stream_info[i].dir == PJMEDIA_DIR_DECODING)
	    dir = "recvonly";
	else if (info.stream_info[i].dir == PJMEDIA_DIR_ENCODING_DECODING)
	    dir = "sendrecv";
	else
	    dir = "inactive";

	
	PJ_LOG(3,(THIS_FILE, 
		  "%s[Media strm#%d] %.*s, %s, peer=%s:%d",
		  "               ",
		  i,
		  info.stream_info[i].fmt.encoding_name.slen,
		  info.stream_info[i].fmt.encoding_name.ptr,
		  dir,
		  rem_addr, rem_port));
	PJ_LOG(3,(THIS_FILE, 
		  "%s tx {pkt=%u, bytes=%u} rx {pkt=%u, bytes=%u}",
		  "                             ",
		  strm_stat.enc.pkt, strm_stat.enc.bytes,
		  strm_stat.dec.pkt, strm_stat.dec.bytes));

    }
}

/*
 * Dump application states.
 */
void pjsua_dump(void)
{
    struct pjsua_inv_data *inv_data;
    char buf[128];
    unsigned old_decor;

    PJ_LOG(3,(THIS_FILE, "Start dumping application states:"));

    old_decor = pj_log_get_decor();
    pj_log_set_decor(old_decor & (PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));

    pjsip_endpt_dump(pjsua.endpt, 1);
    pjmedia_endpt_dump(pjsua.med_endpt);
    pjsip_ua_dump();


    /* Dump all invite sessions: */
    PJ_LOG(3,(THIS_FILE, "Dumping invite sessions:"));

    if (pj_list_empty(&pjsua.inv_list)) {

	PJ_LOG(3,(THIS_FILE, "  - no sessions -"));

    } else {

	inv_data = pjsua.inv_list.next;

	while (inv_data != &pjsua.inv_list) {

	    print_invite_session("  ", inv_data, buf, sizeof(buf));
	    PJ_LOG(3,(THIS_FILE, "%s", buf));

	    if (inv_data->session)
		dump_media_session(inv_data->session);

	    inv_data = inv_data->next;
	}
    }

    pj_log_set_decor(old_decor);
    PJ_LOG(3,(THIS_FILE, "Dump complete"));
}


/*
 * Load settings.
 */
pj_status_t pjsua_load_settings(const char *filename)
{
    int argc = 3;
    char *argv[] = { "pjsua", "--config-file", (char*)filename, NULL};

    return pjsua_parse_args(argc, argv);
}


/*
 * Save settings.
 */
pj_status_t pjsua_save_settings(const char *filename)
{
    unsigned i;
    pj_str_t cfg;
    char line[128];
    pj_pool_t *pool;
    FILE *fhnd;

    /* Create pool for temporary buffer. */
    pool = pj_pool_create(&pjsua.cp.factory, "settings", 4000, 0, NULL);
    if (!pool)
	return PJ_ENOMEM;


    cfg.ptr = pj_pool_alloc(pool, 3800);
    if (!cfg.ptr) {
	pj_pool_release(pool);
	return PJ_EBUG;
    }
    cfg.slen = 0;


    /* Identity */
    if (pjsua.local_uri.slen) {
	pj_ansi_sprintf(line, "--id %.*s\n", 
			(int)pjsua.local_uri.slen, 
			pjsua.local_uri.ptr);
	pj_strcat2(&cfg, line);
    }

    /* Credentials. */
    for (i=0; i<pjsua.cred_count; ++i) {
	if (pjsua.cred_info[i].realm.slen) {
	    pj_ansi_sprintf(line, "--realm %.*s\n",
				  (int)pjsua.cred_info[i].realm.slen,
				  pjsua.cred_info[i].realm.ptr);
	    pj_strcat2(&cfg, line);
	}

	pj_ansi_sprintf(line, "--username %.*s\n",
			      (int)pjsua.cred_info[i].username.slen,
			      pjsua.cred_info[i].username.ptr);
	pj_strcat2(&cfg, line);

	pj_ansi_sprintf(line, "--password %.*s\n",
			      (int)pjsua.cred_info[i].data.slen,
			      pjsua.cred_info[i].data.ptr);
	pj_strcat2(&cfg, line);
    }

    /* Registrar server */
    if (pjsua.registrar_uri.slen) {
	pj_ansi_sprintf(line, "--registrar %.*s\n",
			      (int)pjsua.registrar_uri.slen,
			      pjsua.registrar_uri.ptr);
	pj_strcat2(&cfg, line);
    }


    /* Outbound proxy */
    if (pjsua.outbound_proxy.slen) {
	pj_ansi_sprintf(line, "--outbound %.*s\n",
			      (int)pjsua.outbound_proxy.slen,
			      pjsua.outbound_proxy.ptr);
	pj_strcat2(&cfg, line);
    }

    /* Media */
    if (pjsua.null_audio)
	pj_strcat2(&cfg, "--null-audio\n");


    /* Transport. */
    pj_ansi_sprintf(line, "--local-port %d\n", pjsua.sip_port);
    pj_strcat2(&cfg, line);


    /* STUN */
    if (pjsua.stun_port1) {
	pj_ansi_sprintf(line, "--use-stun1 %.*s:%d\n",
			(int)pjsua.stun_srv1.slen, 
			pjsua.stun_srv1.ptr, 
			pjsua.stun_port1);
	pj_strcat2(&cfg, line);
    }

    if (pjsua.stun_port2) {
	pj_ansi_sprintf(line, "--use-stun2 %.*s:%d\n",
			(int)pjsua.stun_srv2.slen, 
			pjsua.stun_srv2.ptr, 
			pjsua.stun_port2);
	pj_strcat2(&cfg, line);
    }


    /* Add buddies. */
    for (i=0; i<pjsua.buddy_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-buddy %.*s\n",
			      (int)pjsua.buddies[i].slen,
			      pjsua.buddies[i].ptr);
	pj_strcat2(&cfg, line);
    }


    /* Write to file. */
    fhnd = fopen(filename, "wt");
    if (!fhnd) {
	pj_pool_release(pool);
	return pj_get_os_error();
    }

    fwrite(cfg.ptr, cfg.slen, 1, fhnd);
    fclose(fhnd);

    pj_pool_release(pool);
    return PJ_SUCCESS;
}

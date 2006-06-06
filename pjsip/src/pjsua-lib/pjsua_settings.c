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
#include <stdio.h>
#include "pjsua_imp.h"


/*
 * pjsua_settings.c
 *
 * Anything to do with configuration and state dump.
 */

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
    puts  ("Usage:");
    puts  ("  pjsua [options]");
    puts  ("");
    puts  ("General options:");
    puts  ("  --help              Display this help screen");
    puts  ("  --version           Display version info");
    puts  ("");
    puts  ("Logging options:");
    puts  ("  --config-file=file  Read the config/arguments from file.");
    puts  ("  --log-file=fname    Log to filename (default stderr)");
    puts  ("  --log-level=N       Set log max level to N (0(none) to 6(trace)) (default=5)");
    puts  ("  --app-log-level=N   Set log max level for stdout display (default=4)");
    puts  ("");
    puts  ("SIP Account options:");
    puts  ("  --registrar=url     Set the URL of registrar server");
    puts  ("  --id=url            Set the URL of local ID (used in From header)");
    puts  ("  --contact=url       Optionally override the Contact information");
    puts  ("  --proxy=url         Optional URL of proxy server to visit");
    puts  ("  --realm=string      Set realm");
    puts  ("  --username=string   Set authentication username");
    puts  ("  --password=string   Set authentication password");
    puts  ("  --reg-timeout=SEC   Optional registration interval (default 55)");
    puts  ("");
    puts  ("SIP Account Control:");
    puts  ("  --next-account      Add more account");
    puts  ("");
    puts  ("Transport Options:");
    puts  ("  --local-port=port        Set TCP/UDP port");
    puts  ("  --outbound=url           Set the URL of outbound proxy server");
    puts  ("  --use-stun1=host[:port]");
    puts  ("  --use-stun2=host[:port]  Resolve local IP with the specified STUN servers");
    puts  ("");
    puts  ("Media Options:");
    puts  ("  --add-codec=name    Manually add codec (default is to enable all)");
    puts  ("  --clock-rate=N      Override sound device clock rate");
    puts  ("  --null-audio        Use NULL audio device");
    puts  ("  --play-file=file    Play WAV file in conference bridge");
    puts  ("  --auto-play         Automatically play the file (to incoming calls only)");
    puts  ("  --auto-loop         Automatically loop incoming RTP to outgoing RTP");
    puts  ("  --auto-conf         Automatically put incoming calls to conference");
    puts  ("  --rtp-port=N        Base port to try for RTP (default=4000)");
    puts  ("  --complexity=N      Specify encoding complexity (0-10, default=none(-1))");
    puts  ("  --quality=N         Specify encoding quality (0-10, default=4)");
    puts  ("  --ptime=MSEC        Override codec ptime to MSEC (default=specific)");
    puts  ("");
    puts  ("Buddy List (can be more than one):");
    puts  ("  --add-buddy url     Add the specified URL to the buddy list.");
    puts  ("");
    puts  ("User Agent options:");
    puts  ("  --auto-answer=code  Automatically answer incoming calls with code (e.g. 200)");
    puts  ("  --max-calls=N       Maximum number of concurrent calls (default:4, max:255)");
    puts  ("  --uas-refresh=N     Interval in UAS to send re-INVITE (default:-1)");
    puts  ("  --uas-duration=N    Maximum duration of incoming call (default:-1)");
    puts  ("");
    fflush(stdout);
}



/*
 * Verify that valid SIP url is given.
 */
PJ_DEF(pj_status_t) pjsua_verify_sip_url(const char *c_url)
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
	PJ_LOG(1,(THIS_FILE, "Unable to open config file %s", filename));
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
	PJ_LOG(1,(THIS_FILE, 
		  "Too many arguments specified in cmd line/config file"));
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

static int my_atoi(const char *cs)
{
    pj_str_t s;
    return pj_strtoul(pj_cstr(&s, cs));
}


/* Parse arguments. */
PJ_DEF(pj_status_t) pjsua_parse_args(int argc, char *argv[],
				     pjsua_config *cfg,
				     pj_str_t *uri_to_call)
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
	   OPT_AUTO_ANSWER, OPT_AUTO_HANGUP, OPT_AUTO_PLAY, OPT_AUTO_LOOP,
	   OPT_AUTO_CONF, OPT_CLOCK_RATE,
	   OPT_PLAY_FILE, OPT_RTP_PORT, OPT_ADD_CODEC,
	   OPT_COMPLEXITY, OPT_QUALITY, OPT_PTIME,
	   OPT_NEXT_ACCOUNT, OPT_MAX_CALLS, OPT_UAS_REFRESH,
	   OPT_UAS_DURATION,
    };
    struct pj_getopt_option long_options[] = {
	{ "config-file",1, 0, OPT_CONFIG_FILE},
	{ "log-file",	1, 0, OPT_LOG_FILE},
	{ "log-level",	1, 0, OPT_LOG_LEVEL},
	{ "app-log-level",1,0,OPT_APP_LOG_LEVEL},
	{ "help",	0, 0, OPT_HELP},
	{ "version",	0, 0, OPT_VERSION},
	{ "clock-rate",	1, 0, OPT_CLOCK_RATE},
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
	{ "auto-play",  0, 0, OPT_AUTO_PLAY},
	{ "auto-loop",  0, 0, OPT_AUTO_LOOP},
	{ "auto-conf",  0, 0, OPT_AUTO_CONF},
	{ "play-file",  1, 0, OPT_PLAY_FILE},
	{ "rtp-port",	1, 0, OPT_RTP_PORT},
	{ "add-codec",  1, 0, OPT_ADD_CODEC},
	{ "complexity",	1, 0, OPT_COMPLEXITY},
	{ "quality",	1, 0, OPT_QUALITY},
	{ "ptime",      1, 0, OPT_PTIME},
	{ "next-account",0,0, OPT_NEXT_ACCOUNT},
	{ "max-calls",	1, 0, OPT_MAX_CALLS},
	{ "uas-refresh",1, 0, OPT_UAS_REFRESH},
	{ "uas-duration",1,0, OPT_UAS_DURATION},
	{ NULL, 0, 0, 0}
    };
    pj_status_t status;
    pjsua_acc_config *cur_acc;
    char errmsg[80];
    char *config_file = NULL;
    unsigned i;

    /* Run pj_getopt once to see if user specifies config file to read. */ 
    while ((c=pj_getopt_long(argc, argv, "", long_options, 
			     &option_index)) != -1) 
    {
	switch (c) {
	case OPT_CONFIG_FILE:
	    config_file = pj_optarg;
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

    cfg->acc_cnt = 0;
    cur_acc = &cfg->acc_config[0];


    /* Reinitialize and re-run pj_getopt again, possibly with new arguments
     * read from config file.
     */
    pj_optind = 0;
    while((c=pj_getopt_long(argc,argv, "", long_options,&option_index))!=-1) {
	char *p;
	pj_str_t tmp;
	long lval;

	switch (c) {

	case OPT_LOG_FILE:
	    cfg->log_filename = pj_str(pj_optarg);
	    break;

	case OPT_LOG_LEVEL:
	    c = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (c < 0 || c > 6) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: expecting integer value 0-6 "
			  "for --log-level"));
		return PJ_EINVAL;
	    }
	    cfg->log_level = c;
	    pj_log_set_level( c );
	    break;

	case OPT_APP_LOG_LEVEL:
	    cfg->app_log_level = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (cfg->app_log_level < 0 || cfg->app_log_level > 6) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: expecting integer value 0-6 "
			  "for --app-log-level"));
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
	    cfg->null_audio = 1;
	    break;

	case OPT_CLOCK_RATE:
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 8000 || lval > 48000) {
		PJ_LOG(1,(THIS_FILE, "Error: expecting value between "
				     "8000-48000 for clock rate"));
		return PJ_EINVAL;
	    }
	    cfg->clock_rate = lval; 
	    break;

	case OPT_LOCAL_PORT:   /* local-port */
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 1 || lval > 65535) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: expecting integer value for "
			  "--local-port"));
		return PJ_EINVAL;
	    }
	    cfg->udp_port = (pj_uint16_t)lval;
	    break;

	case OPT_PROXY:   /* proxy */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid SIP URL '%s' "
			  "in proxy argument", pj_optarg));
		return PJ_EINVAL;
	    }
	    cur_acc->proxy = pj_str(pj_optarg);
	    break;

	case OPT_OUTBOUND_PROXY:   /* outbound proxy */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid SIP URL '%s' "
			  "in outbound proxy argument", pj_optarg));
		return PJ_EINVAL;
	    }
	    cfg->outbound_proxy = pj_str(pj_optarg);
	    break;

	case OPT_REGISTRAR:   /* registrar */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid SIP URL '%s' in "
			  "registrar argument", pj_optarg));
		return PJ_EINVAL;
	    }
	    cur_acc->reg_uri = pj_str(pj_optarg);
	    break;

	case OPT_REG_TIMEOUT:   /* reg-timeout */
	    cur_acc->reg_timeout = pj_strtoul(pj_cstr(&tmp,pj_optarg));
	    if (cur_acc->reg_timeout < 1 || cur_acc->reg_timeout > 3600) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid value for --reg-timeout "
			  "(expecting 1-3600)"));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_ID:   /* id */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid SIP URL '%s' "
			  "in local id argument", pj_optarg));
		return PJ_EINVAL;
	    }
	    cur_acc->id = pj_str(pj_optarg);
	    break;

	case OPT_CONTACT:   /* contact */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid SIP URL '%s' "
			  "in contact argument", pj_optarg));
		return PJ_EINVAL;
	    }
	    cur_acc->contact = pj_str(pj_optarg);
	    break;

	case OPT_NEXT_ACCOUNT: /* Add more account. */
	    cfg->acc_cnt++;
	    cur_acc = &cfg->acc_config[cfg->acc_cnt - 1];
	    break;

	case OPT_USERNAME:   /* Default authentication user */
	    cur_acc->cred_info[0].username = pj_str(pj_optarg);
	    break;

	case OPT_REALM:	    /* Default authentication realm. */
	    cur_acc->cred_info[0].realm = pj_str(pj_optarg);
	    break;

	case OPT_PASSWORD:   /* authentication password */
	    cur_acc->cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	    cur_acc->cred_info[0].data = pj_str(pj_optarg);
	    break;

	case OPT_USE_STUN1:   /* STUN server 1 */
	    p = pj_ansi_strchr(pj_optarg, ':');
	    if (p) {
		*p = '\0';
		cfg->stun_srv1 = pj_str(pj_optarg);
		cfg->stun_port1 = pj_strtoul(pj_cstr(&tmp, p+1));
		if (cfg->stun_port1 < 1 || cfg->stun_port1 > 65535) {
		    PJ_LOG(1,(THIS_FILE, 
			      "Error: expecting port number with "
			      "option --use-stun1"));
		    return PJ_EINVAL;
		}
	    } else {
		cfg->stun_port1 = 3478;
		cfg->stun_srv1 = pj_str(pj_optarg);
	    }
	    break;

	case OPT_USE_STUN2:   /* STUN server 2 */
	    p = pj_ansi_strchr(pj_optarg, ':');
	    if (p) {
		*p = '\0';
		cfg->stun_srv2 = pj_str(pj_optarg);
		cfg->stun_port2 = pj_strtoul(pj_cstr(&tmp,p+1));
		if (cfg->stun_port2 < 1 || cfg->stun_port2 > 65535) {
		    PJ_LOG(1,(THIS_FILE, 
			      "Error: expecting port number with "
			      "option --use-stun2"));
		    return PJ_EINVAL;
		}
	    } else {
		cfg->stun_port2 = 3478;
		cfg->stun_srv2 = pj_str(pj_optarg);
	    }
	    break;

	case OPT_ADD_BUDDY: /* Add to buddy list. */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid URL '%s' in "
			  "--add-buddy option", pj_optarg));
		return -1;
	    }
	    if (cfg->buddy_cnt == PJSUA_MAX_BUDDIES) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: too many buddies in buddy list."));
		return -1;
	    }
	    cfg->buddy_uri[cfg->buddy_cnt++] = pj_str(pj_optarg);
	    break;

	case OPT_AUTO_PLAY:
	    cfg->auto_play = 1;
	    break;

	case OPT_AUTO_LOOP:
	    cfg->auto_loop = 1;
	    break;

	case OPT_AUTO_CONF:
	    cfg->auto_conf = 1;
	    break;

	case OPT_PLAY_FILE:
	    cfg->wav_file = pj_str(pj_optarg);
	    break;

	case OPT_RTP_PORT:
	    cfg->start_rtp_port = my_atoi(pj_optarg);
	    if (cfg->start_rtp_port < 1 || cfg->start_rtp_port > 65535) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: rtp-port argument value "
			  "(expecting 1-65535"));
		return -1;
	    }
	    break;

	case OPT_ADD_CODEC:
	    cfg->codec_arg[cfg->codec_cnt++] = pj_str(pj_optarg);
	    break;

	case OPT_COMPLEXITY:
	    cfg->complexity = my_atoi(pj_optarg);
	    if (cfg->complexity < 0 || cfg->complexity > 10) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --complexity (expecting 0-10"));
		return -1;
	    }
	    break;

	case OPT_QUALITY:
	    cfg->quality = my_atoi(pj_optarg);
	    if (cfg->quality < 0 || cfg->quality > 10) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --quality (expecting 0-10"));
		return -1;
	    }
	    break;

	case OPT_PTIME:
	    cfg->ptime = my_atoi(pj_optarg);
	    if (cfg->ptime < 10 || cfg->ptime > 1000) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --ptime option"));
		return -1;
	    }
	    break;

	case OPT_AUTO_ANSWER:
	    cfg->auto_answer = my_atoi(pj_optarg);
	    if (cfg->auto_answer < 100 || cfg->auto_answer > 699) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid code in --auto-answer "
			  "(expecting 100-699"));
		return -1;
	    }
	    break;

	case OPT_MAX_CALLS:
	    cfg->max_calls = my_atoi(pj_optarg);
	    if (cfg->max_calls < 1 || cfg->max_calls > 255) {
		PJ_LOG(1,(THIS_FILE,"Too many calls for max-calls (1-255)"));
		return -1;
	    }
	    break;

	case OPT_UAS_REFRESH:
	    cfg->uas_refresh = my_atoi(pj_optarg);
	    if (cfg->uas_refresh < 1) {
		PJ_LOG(1,(THIS_FILE,
			  "Invalid value for --uas-refresh (must be >0)"));
		return -1;
	    }
	    break;

	case OPT_UAS_DURATION:
	    cfg->uas_duration = my_atoi(pj_optarg);
	    if (cfg->uas_duration < 1) {
		PJ_LOG(1,(THIS_FILE,
			  "Invalid value for --uas-duration "
			  "(must be >0)"));
		return -1;
	    }
	    break;
	}
    }

    if (pj_optind != argc) {
	pj_str_t uri_arg;

	if (pjsua_verify_sip_url(argv[pj_optind]) != PJ_SUCCESS) {
	    PJ_LOG(1,(THIS_FILE, "Invalid SIP URI %s", argv[pj_optind]));
	    return -1;
	}
	uri_arg = pj_str(argv[pj_optind]);
	if (uri_to_call)
	    *uri_to_call = uri_arg;
	pj_optind++;

	/* Add URI to call to buddy list if it's not already there */
	for (i=0; i<cfg->buddy_cnt; ++i) {
	    if (pj_stricmp(&cfg->buddy_uri[i], &uri_arg)==0)
		break;
	}
	if (i == cfg->buddy_cnt && cfg->buddy_cnt < PJSUA_MAX_BUDDIES) {
	    cfg->buddy_uri[cfg->buddy_cnt++] = uri_arg;
	}

    } else {
	if (uri_to_call)
	    uri_to_call->slen = 0;
    }

    if (pj_optind != argc) {
	PJ_LOG(1,(THIS_FILE, "Error: unknown options %s", argv[pj_optind]));
	return PJ_EINVAL;
    }

    if (cfg->acc_config[0].id.slen && cfg->acc_cnt==0)
	cfg->acc_cnt = 1;

    for (i=0; i<cfg->acc_cnt; ++i) {
	if (cfg->acc_config[i].cred_info[0].username.slen ||
	    cfg->acc_config[i].cred_info[0].realm.slen)
	{
	    cfg->acc_config[i].cred_count = 1;
	    cfg->acc_config[i].cred_info[0].scheme = pj_str("digest");
	}
    }

    if (pjsua_test_config(cfg, errmsg, sizeof(errmsg)) != PJ_SUCCESS) {
	PJ_LOG(1,(THIS_FILE, "Error: %s", errmsg));
	return -1;
    }

    return PJ_SUCCESS;
}



static void print_call(const char *title,
		       int call_index, 
		       char *buf, pj_size_t size)
{
    int len;
    pjsip_inv_session *inv = pjsua.calls[call_index].inv;
    pjsip_dialog *dlg = inv->dlg;
    char userinfo[128];

    /* Dump invite sesion info. */

    len = pjsip_hdr_print_on(dlg->remote.info, userinfo, sizeof(userinfo));
    if (len < 1)
	pj_ansi_strcpy(userinfo, "<--uri too long-->");
    else
	userinfo[len] = '\0';
    
    len = pj_ansi_snprintf(buf, size, "%s[%s] %s",
			   title,
			   pjsua_inv_state_names[inv->state],
			   userinfo);
    if (len < 1 || len >= (int)size) {
	pj_ansi_strcpy(buf, "<--uri too long-->");
	len = 18;
    } else
	buf[len] = '\0';
}

static const char *good_number(char *buf, pj_int32_t val)
{
    if (val < 1000) {
	pj_ansi_sprintf(buf, "%d", val);
    } else if (val < 1000000) {
	pj_ansi_sprintf(buf, "%d.%dK", 
			val / 1000,
			(val % 1000) / 100);
    } else {
	pj_ansi_sprintf(buf, "%d.%02dM", 
			val / 1000000,
			(val % 1000000) / 10000);
    }

    return buf;
}

static void dump_media_session(const char *indent, 
			       char *buf, unsigned maxlen,
			       pjmedia_session *session)
{
    unsigned i;
    char *p = buf, *end = buf+maxlen;
    int len;
    pjmedia_session_info info;

    pjmedia_session_get_info(session, &info);

    for (i=0; i<info.stream_cnt; ++i) {
	pjmedia_rtcp_stat stat;
	const char *rem_addr;
	int rem_port;
	const char *dir;
	char last_update[40];
	char packets[16], bytes[16], ipbytes[16];
	pj_time_val now;

	pjmedia_session_get_stream_stat(session, i, &stat);
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

	
	len = pj_ansi_snprintf(buf, end-p, 
		  "%s  #%d %.*s @%dKHz, %s, peer=%s:%d",
		  indent, i,
		  info.stream_info[i].fmt.encoding_name.slen,
		  info.stream_info[i].fmt.encoding_name.ptr,
		  info.stream_info[i].fmt.clock_rate / 1000,
		  dir,
		  rem_addr, rem_port);
	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';

	if (stat.rx.update_cnt == 0)
	    strcpy(last_update, "never");
	else {
	    pj_gettimeofday(&now);
	    PJ_TIME_VAL_SUB(now, stat.rx.update);
	    sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		    now.sec / 3600,
		    (now.sec % 3600) / 60,
		    now.sec % 60,
		    now.msec);
	}

	len = pj_ansi_snprintf(p, end-p,
	       "%s     RX pt=%d, stat last update: %s\n"
	       "%s        total %spkt %sB (%sB +IP hdr)\n"
	       "%s        pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)\n"
	       "%s              (msec)    min     avg     max     last\n"
	       "%s        loss period: %7.3f %7.3f %7.3f %7.3f\n"
	       "%s        jitter     : %7.3f %7.3f %7.3f %7.3f%s",
	       indent, info.stream_info[i].fmt.pt,
	       last_update,
	       indent,
	       good_number(packets, stat.rx.pkt),
	       good_number(bytes, stat.rx.bytes),
	       good_number(ipbytes, stat.rx.bytes + stat.rx.pkt * 32),
	       indent,
	       stat.rx.loss,
	       stat.rx.loss * 100.0 / stat.rx.pkt,
	       stat.rx.dup, 
	       stat.rx.dup * 100.0 / stat.rx.pkt,
	       stat.rx.reorder, 
	       stat.rx.reorder * 100.0 / stat.rx.pkt,
	       indent, indent,
	       stat.rx.loss_period.min / 1000.0, 
	       stat.rx.loss_period.avg / 1000.0, 
	       stat.rx.loss_period.max / 1000.0,
	       stat.rx.loss_period.last / 1000.0,
	       indent,
	       stat.rx.jitter.min / 1000.0,
	       stat.rx.jitter.avg / 1000.0,
	       stat.rx.jitter.max / 1000.0,
	       stat.rx.jitter.last / 1000.0,
	       ""
	       );

	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';
	
	if (stat.tx.update_cnt == 0)
	    strcpy(last_update, "never");
	else {
	    pj_gettimeofday(&now);
	    PJ_TIME_VAL_SUB(now, stat.tx.update);
	    sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		    now.sec / 3600,
		    (now.sec % 3600) / 60,
		    now.sec % 60,
		    now.msec);
	}

	len = pj_ansi_snprintf(p, end-p,
	       "%s     TX pt=%d, ptime=%dms, stat last update: %s\n"
	       "%s        total %spkt %sB (%sB +IP hdr)\n"
	       "%s        pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)\n"
	       "%s              (msec)    min     avg     max     last\n"
	       "%s        loss period: %7.3f %7.3f %7.3f %7.3f\n"
	       "%s        jitter     : %7.3f %7.3f %7.3f %7.3f%s",
	       indent,
	       info.stream_info[i].tx_pt,
	       info.stream_info[i].param->info.frm_ptime *
		info.stream_info[i].param->setting.frm_per_pkt,
	       last_update,

	       indent,
	       good_number(packets, stat.tx.pkt),
	       good_number(bytes, stat.tx.bytes),
	       good_number(ipbytes, stat.tx.bytes + stat.tx.pkt * 32),

	       indent,
	       stat.tx.loss,
	       stat.tx.loss * 100.0 / stat.tx.pkt,
	       stat.tx.dup, 
	       stat.tx.dup * 100.0 / stat.tx.pkt,
	       stat.tx.reorder, 
	       stat.tx.reorder * 100.0 / stat.tx.pkt,

	       indent, indent,
	       stat.tx.loss_period.min / 1000.0, 
	       stat.tx.loss_period.avg / 1000.0, 
	       stat.tx.loss_period.max / 1000.0,
	       stat.tx.loss_period.last / 1000.0,
	       indent,
	       stat.tx.jitter.min / 1000.0,
	       stat.tx.jitter.avg / 1000.0,
	       stat.tx.jitter.max / 1000.0,
	       stat.tx.jitter.last / 1000.0,
	       ""
	       );

	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';

	len = pj_ansi_snprintf(p, end-p,
	       "%s    RTT msec       : %7.3f %7.3f %7.3f %7.3f", 
	       indent,
	       stat.rtt.min / 1000.0,
	       stat.rtt.avg / 1000.0,
	       stat.rtt.max / 1000.0,
	       stat.rtt.last / 1000.0
	       );
	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';
    }
}

PJ_DEF(void) pjsua_call_dump(int call_index, int with_media, 
			     char *buffer, unsigned maxlen,
			     const char *indent)
{
    pjsua_call *call = &pjsua.calls[call_index];
    pj_time_val duration, res_delay, con_delay;
    char tmp[128];
    char *p, *end;
    int len;

    *buffer = '\0';
    p = buffer;
    end = buffer + maxlen;
    len = 0;

    PJ_ASSERT_ON_FAIL(call_index >= 0 && 
		      call_index < PJ_ARRAY_SIZE(pjsua.calls), return);

    if (call->inv == NULL)
	return;

    print_call(indent, call_index, tmp, sizeof(tmp));
    
    len = pj_ansi_strlen(tmp);
    pj_ansi_strcpy(buffer, tmp);

    p += len;
    *p++ = '\r';
    *p++ = '\n';

    /* Calculate call duration */
    if (call->inv->state >= PJSIP_INV_STATE_CONFIRMED) {
	pj_gettimeofday(&duration);
	PJ_TIME_VAL_SUB(duration, call->conn_time);
	con_delay = call->conn_time;
	PJ_TIME_VAL_SUB(con_delay, call->start_time);
    } else {
	duration.sec = duration.msec = 0;
	con_delay.sec = con_delay.msec = 0;
    }

    /* Calculate first response delay */
    if (call->inv->state >= PJSIP_INV_STATE_EARLY) {
	res_delay = call->res_time;
	PJ_TIME_VAL_SUB(res_delay, call->start_time);
    } else {
	res_delay.sec = res_delay.msec = 0;
    }

    /* Print duration */
    len = pj_ansi_snprintf(p, end-p, 
		           "%s  Call time: %02dh:%02dm:%02ds, "
		           "1st res in %d ms, conn in %dms",
			   indent,
		           (duration.sec / 3600),
		           ((duration.sec % 3600)/60),
		           (duration.sec % 60),
		           PJ_TIME_VAL_MSEC(res_delay), 
		           PJ_TIME_VAL_MSEC(con_delay));
    
    if (len > 0 && len < end-p) {
	p += len;
	*p++ = '\n';
	*p = '\0';
    }

    /* Dump session statistics */
    if (with_media && call->session)
	dump_media_session(indent, p, end-p, call->session);

}

/*
 * Dump application states.
 */
PJ_DEF(void) pjsua_dump(pj_bool_t detail)
{
    unsigned old_decor;
    char buf[1024];

    PJ_LOG(3,(THIS_FILE, "Start dumping application states:"));

    old_decor = pj_log_get_decor();
    pj_log_set_decor(old_decor & (PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));

    if (detail)
	pj_dump_config();

    pjsip_endpt_dump(pjsua.endpt, detail);
    pjmedia_endpt_dump(pjsua.med_endpt);
    pjsip_tsx_layer_dump(detail);
    pjsip_ua_dump(detail);


    /* Dump all invite sessions: */
    PJ_LOG(3,(THIS_FILE, "Dumping invite sessions:"));

    if (pjsua.call_cnt == 0) {

	PJ_LOG(3,(THIS_FILE, "  - no sessions -"));

    } else {
	unsigned i;

	for (i=0; i<pjsua.config.max_calls; ++i) {
	    if (pjsua.calls[i].inv) {
		pjsua_call_dump(i, detail, buf, sizeof(buf), "  ");
		PJ_LOG(3,(THIS_FILE, "%s", buf));
	    }
	}
    }

    /* Dump presence status */
    pjsua_pres_dump(detail);

    pj_log_set_decor(old_decor);
    PJ_LOG(3,(THIS_FILE, "Dump complete"));
}


/*
 * Load settings.
 */
PJ_DECL(pj_status_t) pjsua_load_settings(const char *filename,
					 pjsua_config *cfg,
					 pj_str_t *uri_to_call)
{
    int argc = 3;
    char *argv[4] = { "pjsua", "--config-file", NULL, NULL};

    argv[2] = (char*)filename;
    return pjsua_parse_args(argc, argv, cfg, uri_to_call);
}


/*
 * Save account settings
 */
static void save_account_settings(int acc_index, pj_str_t *result)
{
    char line[128];
    pjsua_acc_config *acc_cfg = &pjsua.config.acc_config[acc_index];

    
    pj_ansi_sprintf(line, "#\n# Account %d:\n#\n", acc_index);
    pj_strcat2(result, line);


    /* Identity */
    if (acc_cfg->id.slen) {
	pj_ansi_sprintf(line, "--id %.*s\n", 
			(int)acc_cfg->id.slen, 
			acc_cfg->id.ptr);
	pj_strcat2(result, line);
    }

    /* Registrar server */
    if (acc_cfg->reg_uri.slen) {
	pj_ansi_sprintf(line, "--registrar %.*s\n",
			      (int)acc_cfg->reg_uri.slen,
			      acc_cfg->reg_uri.ptr);
	pj_strcat2(result, line);

	pj_ansi_sprintf(line, "--reg-timeout %u\n",
			      acc_cfg->reg_timeout);
	pj_strcat2(result, line);
    }


    /* Proxy */
    if (acc_cfg->proxy.slen) {
	pj_ansi_sprintf(line, "--proxy %.*s\n",
			      (int)acc_cfg->proxy.slen,
			      acc_cfg->proxy.ptr);
	pj_strcat2(result, line);
    }

    if (acc_cfg->cred_info[0].realm.slen) {
	pj_ansi_sprintf(line, "--realm %.*s\n",
			      (int)acc_cfg->cred_info[0].realm.slen,
			      acc_cfg->cred_info[0].realm.ptr);
	pj_strcat2(result, line);
    }

    if (acc_cfg->cred_info[0].username.slen) {
	pj_ansi_sprintf(line, "--username %.*s\n",
			      (int)acc_cfg->cred_info[0].username.slen,
			      acc_cfg->cred_info[0].username.ptr);
	pj_strcat2(result, line);
    }

    if (acc_cfg->cred_info[0].data.slen) {
	pj_ansi_sprintf(line, "--password %.*s\n",
			      (int)acc_cfg->cred_info[0].data.slen,
			      acc_cfg->cred_info[0].data.ptr);
	pj_strcat2(result, line);
    }

}



/*
 * Dump settings.
 */
PJ_DEF(int) pjsua_dump_settings(const pjsua_config *config,
				char *buf, pj_size_t max)
{
    unsigned acc_index;
    unsigned i;
    pj_str_t cfg;
    char line[128];

    PJ_UNUSED_ARG(max);

    if (config == NULL)
	config = &pjsua.config;

    cfg.ptr = buf;
    cfg.slen = 0;


    /* Logging. */
    pj_strcat2(&cfg, "#\n# Logging options:\n#\n");
    pj_ansi_sprintf(line, "--log-level %d\n",
		    config->log_level);
    pj_strcat2(&cfg, line);

    pj_ansi_sprintf(line, "--app-log-level %d\n",
		    config->app_log_level);
    pj_strcat2(&cfg, line);

    if (config->log_filename.slen) {
	pj_ansi_sprintf(line, "--log-file %s\n",
			config->log_filename.ptr);
	pj_strcat2(&cfg, line);
    }


    /* Save account settings. */
    for (acc_index=0; acc_index < config->acc_cnt; ++acc_index) {
	
	save_account_settings(acc_index, &cfg);

	if (acc_index < config->acc_cnt-1)
	    pj_strcat2(&cfg, "--next-account\n");
    }


    pj_strcat2(&cfg, "#\n# Network settings:\n#\n");

    /* Outbound proxy */
    if (config->outbound_proxy.slen) {
	pj_ansi_sprintf(line, "--outbound %.*s\n",
			      (int)config->outbound_proxy.slen,
			      config->outbound_proxy.ptr);
	pj_strcat2(&cfg, line);
    }


    /* Transport. */
    pj_ansi_sprintf(line, "--local-port %d\n", config->udp_port);
    pj_strcat2(&cfg, line);


    /* STUN */
    if (config->stun_port1) {
	pj_ansi_sprintf(line, "--use-stun1 %.*s:%d\n",
			(int)config->stun_srv1.slen, 
			config->stun_srv1.ptr, 
			config->stun_port1);
	pj_strcat2(&cfg, line);
    }

    if (config->stun_port2) {
	pj_ansi_sprintf(line, "--use-stun2 %.*s:%d\n",
			(int)config->stun_srv2.slen, 
			config->stun_srv2.ptr, 
			config->stun_port2);
	pj_strcat2(&cfg, line);
    }


    pj_strcat2(&cfg, "#\n# Media settings:\n#\n");


    /* Media */
    if (config->null_audio)
	pj_strcat2(&cfg, "--null-audio\n");
    if (config->auto_play)
	pj_strcat2(&cfg, "--auto-play\n");
    if (config->auto_loop)
	pj_strcat2(&cfg, "--auto-loop\n");
    if (config->auto_conf)
	pj_strcat2(&cfg, "--auto-conf\n");
    if (config->wav_file.slen) {
	pj_ansi_sprintf(line, "--play-file %s\n",
			config->wav_file.ptr);
	pj_strcat2(&cfg, line);
    }
    /* Media clock rate. */
    if (config->clock_rate) {
	pj_ansi_sprintf(line, "--clock-rate %d\n",
			config->clock_rate);
	pj_strcat2(&cfg, line);
    }


    /* Encoding quality and complexity */
    if (config->quality > 0) {
	pj_ansi_sprintf(line, "--quality %d\n",
			config->quality);
	pj_strcat2(&cfg, line);
    }
    if (config->complexity > 0) {
	pj_ansi_sprintf(line, "--complexity %d\n",
			config->complexity);
	pj_strcat2(&cfg, line);
    }

    /* ptime */
    if (config->ptime) {
	pj_ansi_sprintf(line, "--ptime %d\n",
			config->ptime);
	pj_strcat2(&cfg, line);
    }

    /* Start RTP port. */
    pj_ansi_sprintf(line, "--rtp-port %d\n",
		    config->start_rtp_port);
    pj_strcat2(&cfg, line);

    /* Add codec. */
    for (i=0; i<config->codec_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-codec %s\n",
		    config->codec_arg[i].ptr);
	pj_strcat2(&cfg, line);
    }

    pj_strcat2(&cfg, "#\n# User agent:\n#\n");

    /* Auto-answer. */
    if (config->auto_answer != 0) {
	pj_ansi_sprintf(line, "--auto-answer %d\n",
			config->auto_answer);
	pj_strcat2(&cfg, line);
    }

    /* Max calls. */
    pj_ansi_sprintf(line, "--max-calls %d\n",
		    config->max_calls);
    pj_strcat2(&cfg, line);

    /* Uas-refresh. */
    if (config->uas_refresh > 0) {
	pj_ansi_sprintf(line, "--uas-refresh %d\n",
			config->uas_refresh);
	pj_strcat2(&cfg, line);
    }

    /* Uas-duration. */
    if (config->uas_duration > 0) {
	pj_ansi_sprintf(line, "--uas-duration %d\n",
			config->uas_duration);
	pj_strcat2(&cfg, line);
    }

    pj_strcat2(&cfg, "#\n# Buddies:\n#\n");

    /* Add buddies. */
    for (i=0; i<config->buddy_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-buddy %.*s\n",
			      (int)config->buddy_uri[i].slen,
			      config->buddy_uri[i].ptr);
	pj_strcat2(&cfg, line);
    }


    *(cfg.ptr + cfg.slen) = '\0';
    return cfg.slen;
}

/*
 * Save settings.
 */
PJ_DEF(pj_status_t) pjsua_save_settings(const char *filename,
					const pjsua_config *config)
{
    pj_str_t cfg;
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


    cfg.slen = pjsua_dump_settings(config, cfg.ptr, 3800);
    if (cfg.slen < 1) {
	pj_pool_release(pool);
	return PJ_ENOMEM;
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

/**
 * Get pjsua running config.
 */
PJ_DEF(void) pjsua_get_config(pj_pool_t *pool,
			      pjsua_config *cfg)
{
    unsigned i;

    pjsua_copy_config(pool, cfg, &pjsua.config);

    /* Compact buddy uris. */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.config.buddy_uri)-1; ++i) {
	if (pjsua.config.buddy_uri[i].slen == 0) {
	    unsigned j;

	    for (j=i+1; j<PJ_ARRAY_SIZE(pjsua.config.buddy_uri); ++j) {
		if (pjsua.config.buddy_uri[j].slen != 0)
		    break;
	    }
	
	    if (j == PJ_ARRAY_SIZE(pjsua.config.buddy_uri))
		break;
	    else
		pjsua.config.buddy_uri[i] = pjsua.config.buddy_uri[j];
	}
    }

    /* Compact accounts. */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.config.acc_config)-1; ++i) {

	if (pjsua.acc[i].valid == PJ_FALSE || pjsua.acc[i].auto_gen) {
	    unsigned j;

	    for (j=i+1; j<PJ_ARRAY_SIZE(pjsua.config.acc_config); ++j) {
		if (pjsua.acc[j].valid && !pjsua.acc[j].auto_gen)
		    break;
	    }
	
	    if (j == PJ_ARRAY_SIZE(pjsua.config.acc_config)) {
		break;
	    } else {
		pj_memcpy(&pjsua.config.acc_config[i] ,
			  &pjsua.config.acc_config[j],
			  sizeof(pjsua_acc_config));
	    }
	}

    }

    /* Remove auto generated account from config */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua.config.acc_config); ++i) {
	if (pjsua.acc[i].auto_gen)
	    --cfg->acc_cnt;
    }
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
static pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
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
static pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
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
pjsip_module pjsua_msg_logger = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-log", 13 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &logging_on_rx_msg,			/* on_rx_request()	*/
    &logging_on_rx_msg,			/* on_rx_response()	*/
    &logging_on_tx_msg,			/* on_tx_request.	*/
    &logging_on_tx_msg,			/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};


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


#define THIS_FILE	"pjsua.c"



/* Pjsua application data */
static struct app_config
{
    pjsua_config	    cfg;
    pjsua_logging_config    log_cfg;
    pjsua_media_config	    media_cfg;
    pjsua_transport_config  udp_cfg;
    pjsua_transport_config  rtp_cfg;

    unsigned		    acc_cnt;
    pjsua_acc_config	    acc_cfg[PJSUA_MAX_ACC];

    unsigned		    buddy_cnt;
    pjsua_buddy_config	    buddy_cfg[PJSUA_MAX_BUDDIES];

    pj_pool_t		   *pool;
    /* Compatibility with older pjsua */

    unsigned		    codec_cnt;
    pj_str_t		    codec_arg[32];
    unsigned		    clock_rate;
    pj_bool_t		    null_audio;
    pj_str_t		    wav_file;
    pjsua_player_id	    wav_id;
    pjsua_conf_port_id	    wav_port;
    pj_bool_t		    auto_play;
    pj_bool_t		    auto_loop;
    unsigned		    ptime;
    unsigned		    quality;
    unsigned		    complexity;
    unsigned		    auto_answer;
    unsigned		    duration;
} app_config;


static pjsua_acc_id	current_acc;
static pjsua_call_id	current_call;
static pj_str_t		uri_arg;

/*****************************************************************************
 * Configuration manipulation
 */

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
    puts  ("                      May be specified multiple times");
    puts  ("  --realm=string      Set realm");
    puts  ("  --username=string   Set authentication username");
    puts  ("  --password=string   Set authentication password");
    puts  ("  --reg-timeout=SEC   Optional registration interval (default 55)");
    puts  ("");
    puts  ("SIP Account Control:");
    puts  ("  --next-account      Add more account");
    puts  ("");
    puts  ("Transport Options:");
    puts  ("  --local-port=port   Set TCP/UDP port");
    puts  ("  --outbound=url      Set the URL of global outbound proxy server");
    puts  ("                      May be specified multiple times");
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
    puts  ("  --duration=SEC      Set maximum call duration (default:no limit)");
    puts  ("");
    fflush(stdout);
}


/* Set default config. */
static void default_config(struct app_config *cfg)
{
    pjsua_config_default(&cfg->cfg);
    pjsua_logging_config_default(&cfg->log_cfg);
    pjsua_media_config_default(&cfg->media_cfg);
    pjsua_transport_config_default(&cfg->udp_cfg);
    cfg->udp_cfg.port = 5060;
    pjsua_transport_config_default(&cfg->rtp_cfg);
    cfg->rtp_cfg.port = 4000;
    cfg->duration = (unsigned)-1;
    cfg->wav_id = PJSUA_INVALID_ID;
    cfg->wav_port = PJSUA_INVALID_ID;
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
static pj_status_t parse_args(int argc, char *argv[],
			      struct app_config *cfg,
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
	   OPT_NEXT_ACCOUNT, OPT_MAX_CALLS, 
	   OPT_DURATION,
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
	{ "duration",1,0, OPT_DURATION},
	{ NULL, 0, 0, 0}
    };
    pj_status_t status;
    pjsua_acc_config *cur_acc;
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
	status = read_config_file(app_config.pool, config_file, &argc, &argv);
	if (status != 0)
	    return status;
    }

    cfg->acc_cnt = 0;
    cur_acc = &cfg->acc_cfg[0];


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
	    cfg->log_cfg.log_filename = pj_str(pj_optarg);
	    break;

	case OPT_LOG_LEVEL:
	    c = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (c < 0 || c > 6) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: expecting integer value 0-6 "
			  "for --log-level"));
		return PJ_EINVAL;
	    }
	    cfg->log_cfg.level = c;
	    pj_log_set_level( c );
	    break;

	case OPT_APP_LOG_LEVEL:
	    cfg->log_cfg.console_level = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (cfg->log_cfg.console_level < 0 || cfg->log_cfg.console_level > 6) {
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
	    cfg->null_audio = PJ_TRUE;
	    break;

	case OPT_CLOCK_RATE:
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 8000 || lval > 48000) {
		PJ_LOG(1,(THIS_FILE, "Error: expecting value between "
				     "8000-48000 for clock rate"));
		return PJ_EINVAL;
	    }
	    cfg->media_cfg.clock_rate = lval; 
	    break;

	case OPT_LOCAL_PORT:   /* local-port */
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 1 || lval > 65535) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: expecting integer value for "
			  "--local-port"));
		return PJ_EINVAL;
	    }
	    cfg->udp_cfg.port = (pj_uint16_t)lval;
	    break;

	case OPT_PROXY:   /* proxy */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid SIP URL '%s' "
			  "in proxy argument", pj_optarg));
		return PJ_EINVAL;
	    }
	    cur_acc->proxy[cur_acc->proxy_cnt++] = pj_str(pj_optarg);
	    break;

	case OPT_OUTBOUND_PROXY:   /* outbound proxy */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid SIP URL '%s' "
			  "in outbound proxy argument", pj_optarg));
		return PJ_EINVAL;
	    }
	    cfg->cfg.outbound_proxy[cfg->cfg.outbound_proxy_cnt++] = pj_str(pj_optarg);
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
	    cur_acc = &cfg->acc_cfg[cfg->acc_cnt - 1];
	    break;

	case OPT_USERNAME:   /* Default authentication user */
	    cur_acc->cred_count = 1;
	    cur_acc->cred_info[0].username = pj_str(pj_optarg);
	    break;

	case OPT_REALM:	    /* Default authentication realm. */
	    cur_acc->cred_count = 1;
	    cur_acc->cred_info[0].realm = pj_str(pj_optarg);
	    break;

	case OPT_PASSWORD:   /* authentication password */
	    cur_acc->cred_count = 1;
	    cur_acc->cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	    cur_acc->cred_info[0].data = pj_str(pj_optarg);
	    break;

	case OPT_USE_STUN1:   /* STUN server 1 */
	    p = pj_ansi_strchr(pj_optarg, ':');
	    if (p) {
		*p = '\0';
		cfg->udp_cfg.stun_config.stun_srv1 = pj_str(pj_optarg);
		cfg->udp_cfg.stun_config.stun_port1 = pj_strtoul(pj_cstr(&tmp, p+1));
		if (cfg->udp_cfg.stun_config.stun_port1 < 1 || cfg->udp_cfg.stun_config.stun_port1 > 65535) {
		    PJ_LOG(1,(THIS_FILE, 
			      "Error: expecting port number with "
			      "option --use-stun1"));
		    return PJ_EINVAL;
		}
	    } else {
		cfg->udp_cfg.stun_config.stun_port1 = 3478;
		cfg->udp_cfg.stun_config.stun_srv1 = pj_str(pj_optarg);
	    }
	    cfg->udp_cfg.use_stun = PJ_TRUE;
	    break;

	case OPT_USE_STUN2:   /* STUN server 2 */
	    p = pj_ansi_strchr(pj_optarg, ':');
	    if (p) {
		*p = '\0';
		cfg->udp_cfg.stun_config.stun_srv2 = pj_str(pj_optarg);
		cfg->udp_cfg.stun_config.stun_port2 = pj_strtoul(pj_cstr(&tmp,p+1));
		if (cfg->udp_cfg.stun_config.stun_port2 < 1 || cfg->udp_cfg.stun_config.stun_port2 > 65535) {
		    PJ_LOG(1,(THIS_FILE, 
			      "Error: expecting port number with "
			      "option --use-stun2"));
		    return PJ_EINVAL;
		}
	    } else {
		cfg->udp_cfg.stun_config.stun_port2 = 3478;
		cfg->udp_cfg.stun_config.stun_srv2 = pj_str(pj_optarg);
	    }
	    break;

	case OPT_ADD_BUDDY: /* Add to buddy list. */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid URL '%s' in "
			  "--add-buddy option", pj_optarg));
		return -1;
	    }
	    if (cfg->buddy_cnt == PJ_ARRAY_SIZE(cfg->buddy_cfg)) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: too many buddies in buddy list."));
		return -1;
	    }
	    cfg->buddy_cfg[cfg->buddy_cnt].uri = pj_str(pj_optarg);
	    cfg->buddy_cnt++;
	    break;

	case OPT_AUTO_PLAY:
	    cfg->auto_play = 1;
	    break;

	case OPT_AUTO_LOOP:
	    cfg->auto_loop = 1;
	    break;

	case OPT_PLAY_FILE:
	    cfg->wav_file = pj_str(pj_optarg);
	    break;

	case OPT_RTP_PORT:
	    cfg->rtp_cfg.port = my_atoi(pj_optarg);
	    if (cfg->rtp_cfg.port < 1 || cfg->rtp_cfg.port > 65535) {
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
	    cfg->cfg.max_calls = my_atoi(pj_optarg);
	    if (cfg->cfg.max_calls < 1 || cfg->cfg.max_calls > 255) {
		PJ_LOG(1,(THIS_FILE,"Too many calls for max-calls (1-255)"));
		return -1;
	    }
	    break;

	case OPT_DURATION:
	    cfg->duration = my_atoi(pj_optarg);
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
	    if (pj_stricmp(&cfg->buddy_cfg[i].uri, &uri_arg)==0)
		break;
	}
	if (i == cfg->buddy_cnt && cfg->buddy_cnt < PJSUA_MAX_BUDDIES) {
	    cfg->buddy_cfg[cfg->buddy_cnt++].uri = uri_arg;
	}

    } else {
	if (uri_to_call)
	    uri_to_call->slen = 0;
    }

    if (pj_optind != argc) {
	PJ_LOG(1,(THIS_FILE, "Error: unknown options %s", argv[pj_optind]));
	return PJ_EINVAL;
    }

    if (cfg->acc_cfg[0].id.slen && cfg->acc_cnt==0)
	cfg->acc_cnt = 1;

    for (i=0; i<cfg->acc_cnt; ++i) {
	if (cfg->acc_cfg[i].cred_info[0].username.slen ||
	    cfg->acc_cfg[i].cred_info[0].realm.slen)
	{
	    cfg->acc_cfg[i].cred_count = 1;
	    cfg->acc_cfg[i].cred_info[0].scheme = pj_str("digest");
	}
    }


    return PJ_SUCCESS;
}


/*
 * Save account settings
 */
static void write_account_settings(int acc_index, pj_str_t *result)
{
    unsigned i;
    char line[128];
    pjsua_acc_config *acc_cfg = &app_config.acc_cfg[acc_index];

    
    pj_ansi_sprintf(line, "\n#\n# Account %d:\n#\n", acc_index);
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

    /* Contact */
    if (acc_cfg->contact.slen) {
	pj_ansi_sprintf(line, "--contact %.*s\n", 
			(int)acc_cfg->contact.slen, 
			acc_cfg->contact.ptr);
	pj_strcat2(result, line);
    }

    /* Proxy */
    for (i=0; i<acc_cfg->proxy_cnt; ++i) {
	pj_ansi_sprintf(line, "--proxy %.*s\n",
			      (int)acc_cfg->proxy[i].slen,
			      acc_cfg->proxy[i].ptr);
	pj_strcat2(result, line);
    }

    /* Credentials */
    for (i=0; i<acc_cfg->cred_count; ++i) {
	if (acc_cfg->cred_info[i].realm.slen) {
	    pj_ansi_sprintf(line, "--realm %.*s\n",
				  (int)acc_cfg->cred_info[i].realm.slen,
				  acc_cfg->cred_info[i].realm.ptr);
	    pj_strcat2(result, line);
	}

	if (acc_cfg->cred_info[i].username.slen) {
	    pj_ansi_sprintf(line, "--username %.*s\n",
				  (int)acc_cfg->cred_info[i].username.slen,
				  acc_cfg->cred_info[i].username.ptr);
	    pj_strcat2(result, line);
	}

	if (acc_cfg->cred_info[i].data.slen) {
	    pj_ansi_sprintf(line, "--password %.*s\n",
				  (int)acc_cfg->cred_info[i].data.slen,
				  acc_cfg->cred_info[i].data.ptr);
	    pj_strcat2(result, line);
	}
    }

}


/*
 * Write settings.
 */
static int write_settings(const struct app_config *config,
			  char *buf, pj_size_t max)
{
    unsigned acc_index;
    unsigned i;
    pj_str_t cfg;
    char line[128];

    PJ_UNUSED_ARG(max);

    cfg.ptr = buf;
    cfg.slen = 0;

    /* Logging. */
    pj_strcat2(&cfg, "#\n# Logging options:\n#\n");
    pj_ansi_sprintf(line, "--log-level %d\n",
		    config->log_cfg.level);
    pj_strcat2(&cfg, line);

    pj_ansi_sprintf(line, "--app-log-level %d\n",
		    config->log_cfg.console_level);
    pj_strcat2(&cfg, line);

    if (config->log_cfg.log_filename.slen) {
	pj_ansi_sprintf(line, "--log-file %.*s\n",
			(int)config->log_cfg.log_filename.slen,
			config->log_cfg.log_filename.ptr);
	pj_strcat2(&cfg, line);
    }


    /* Save account settings. */
    for (acc_index=0; acc_index < config->acc_cnt; ++acc_index) {
	
	write_account_settings(acc_index, &cfg);

	if (acc_index < config->acc_cnt-1)
	    pj_strcat2(&cfg, "--next-account\n");
    }


    pj_strcat2(&cfg, "\n#\n# Network settings:\n#\n");

    /* Outbound proxy */
    for (i=0; i<config->cfg.outbound_proxy_cnt; ++i) {
	pj_ansi_sprintf(line, "--outbound %.*s\n",
			      (int)config->cfg.outbound_proxy[i].slen,
			      config->cfg.outbound_proxy[i].ptr);
	pj_strcat2(&cfg, line);
    }


    /* UDP Transport. */
    pj_ansi_sprintf(line, "--local-port %d\n", config->udp_cfg.port);
    pj_strcat2(&cfg, line);


    /* STUN */
    if (config->udp_cfg.stun_config.stun_port1) {
	pj_ansi_sprintf(line, "--use-stun1 %.*s:%d\n",
			(int)config->udp_cfg.stun_config.stun_srv1.slen, 
			config->udp_cfg.stun_config.stun_srv1.ptr, 
			config->udp_cfg.stun_config.stun_port1);
	pj_strcat2(&cfg, line);
    }

    if (config->udp_cfg.stun_config.stun_port2) {
	pj_ansi_sprintf(line, "--use-stun2 %.*s:%d\n",
			(int)config->udp_cfg.stun_config.stun_srv2.slen, 
			config->udp_cfg.stun_config.stun_srv2.ptr, 
			config->udp_cfg.stun_config.stun_port2);
	pj_strcat2(&cfg, line);
    }


    pj_strcat2(&cfg, "\n#\n# Media settings:\n#\n");


    /* Media */
    if (config->null_audio)
	pj_strcat2(&cfg, "--null-audio\n");
    if (config->auto_play)
	pj_strcat2(&cfg, "--auto-play\n");
    if (config->auto_loop)
	pj_strcat2(&cfg, "--auto-loop\n");
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
		    config->rtp_cfg.port);
    pj_strcat2(&cfg, line);

    /* Add codec. */
    for (i=0; i<config->codec_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-codec %s\n",
		    config->codec_arg[i].ptr);
	pj_strcat2(&cfg, line);
    }

    pj_strcat2(&cfg, "\n#\n# User agent:\n#\n");

    /* Auto-answer. */
    if (config->auto_answer != 0) {
	pj_ansi_sprintf(line, "--auto-answer %d\n",
			config->auto_answer);
	pj_strcat2(&cfg, line);
    }

    /* Max calls. */
    pj_ansi_sprintf(line, "--max-calls %d\n",
		    config->cfg.max_calls);
    pj_strcat2(&cfg, line);

    /* Uas-duration. */
    if (config->duration != (unsigned)-1) {
	pj_ansi_sprintf(line, "--duration %d\n",
			config->duration);
	pj_strcat2(&cfg, line);
    }

    pj_strcat2(&cfg, "\n#\n# Buddies:\n#\n");

    /* Add buddies. */
    for (i=0; i<config->buddy_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-buddy %.*s\n",
			      (int)config->buddy_cfg[i].uri.slen,
			      config->buddy_cfg[i].uri.ptr);
	pj_strcat2(&cfg, line);
    }


    *(cfg.ptr + cfg.slen) = '\0';
    return cfg.slen;
}


/*
 * Dump application states.
 */
static void app_dump(pj_bool_t detail)
{
    unsigned old_decor;
    char buf[1024];

    PJ_LOG(3,(THIS_FILE, "Start dumping application states:"));

    old_decor = pj_log_get_decor();
    pj_log_set_decor(old_decor & (PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));

    if (detail)
	pj_dump_config();

    pjsip_endpt_dump(pjsua_get_pjsip_endpt(), detail);
    pjmedia_endpt_dump(pjsua_get_pjmedia_endpt());
    pjsip_tsx_layer_dump(detail);
    pjsip_ua_dump(detail);


    /* Dump all invite sessions: */
    PJ_LOG(3,(THIS_FILE, "Dumping invite sessions:"));

    if (pjsua_call_get_count() == 0) {

	PJ_LOG(3,(THIS_FILE, "  - no sessions -"));

    } else {
	unsigned i;

	for (i=0; i<app_config.cfg.max_calls; ++i) {
	    if (pjsua_call_is_active(i)) {
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


/*****************************************************************************
 * Console application
 */

/*
 * Find next call when current call is disconnected or when user
 * press ']'
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

    current_call = PJSUA_INVALID_ID;
    return PJ_FALSE;
}


/*
 * Find previous call when user press '['
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

    current_call = PJSUA_INVALID_ID;
    return PJ_FALSE;
}



/*
 * Handler when invite state has changed.
 */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info call_info;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &call_info);

    if (call_info.state == PJSIP_INV_STATE_DISCONNECTED) {

	PJ_LOG(3,(THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%s)]", 
		  call_id,
		  call_info.last_status,
		  call_info.last_status_text.ptr));

	if (call_id == current_call) {
	    find_next_call();
	}

    } else {

	PJ_LOG(3,(THIS_FILE, "Call %d state changed to %s", 
		  call_id,
		  call_info.state_text.ptr));

	if (current_call==PJSUA_INVALID_ID)
	    current_call = call_id;

    }
}


/**
 * Handler when there is incoming call.
 */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata)
{
    pjsua_call_info call_info;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &call_info);

    if (app_config.auto_answer > 0) {
	pjsua_call_answer(call_id, app_config.auto_answer, NULL, NULL);
    }

    if (app_config.auto_answer < 200) {
	PJ_LOG(3,(THIS_FILE,
		  "Incoming call for account %d!\n"
		  "From: %s\n"
		  "To: %s\n"
		  "Press a to answer or h to reject call",
		  acc_id,
		  call_info.remote_info.ptr,
		  call_info.local_info.ptr));
    }
}


/*
 * Callback on media state changed event.
 * The action may connect the call to sound device, to file, or
 * to loop the call.
 */
static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info call_info;

    pjsua_call_get_info(call_id, &call_info);

    if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
	pj_bool_t connect_sound = PJ_TRUE;

	/* Loopback sound, if desired */
	if (app_config.auto_loop) {
	    pjsua_conf_connect(call_info.conf_slot, call_info.conf_slot);
	    connect_sound = PJ_FALSE;
	}

	/* Stream a file, if desired */
	if (app_config.auto_play && app_config.wav_port != PJSUA_INVALID_ID) {
	    pjsua_conf_connect(app_config.wav_port, call_info.conf_slot);
	    connect_sound = PJ_FALSE;
	}

	/* Otherwise connect to sound device */
	if (connect_sound) {
	    pjsua_conf_connect(call_info.conf_slot, 0);
	    pjsua_conf_connect(0, call_info.conf_slot);
	}

	PJ_LOG(3,(THIS_FILE, "Media for call %d is active", call_id));

    } else if (call_info.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD) {
	PJ_LOG(3,(THIS_FILE, "Media for call %d is suspended (hold) by local",
		  call_id));
    } else if (call_info.media_status == PJSUA_CALL_MEDIA_REMOTE_HOLD) {
	PJ_LOG(3,(THIS_FILE, 
		  "Media for call %d is suspended (hold) by remote",
		  call_id));
    } else {
	PJ_LOG(3,(THIS_FILE, 
		  "Media for call %d is inactive",
		  call_id));
    }
}


/*
 * Handler registration status has changed.
 */
static void on_reg_state(pjsua_acc_id acc_id)
{
    PJ_UNUSED_ARG(acc_id);

    // Log already written.
}


/*
 * Handler on buddy state changed.
 */
static void on_buddy_state(pjsua_buddy_id buddy_id)
{
    pjsua_buddy_info info;
    pjsua_buddy_get_info(buddy_id, &info);

    PJ_LOG(3,(THIS_FILE, "%.*s status is %.*s",
	      (int)info.uri.slen,
	      info.uri.ptr,
	      (int)info.status_text.slen,
	      info.status_text.ptr));
}


/**
 * Incoming IM message (i.e. MESSAGE request)!
 */
static void on_pager(pjsua_call_id call_id, const pj_str_t *from, 
		     const pj_str_t *to, const pj_str_t *contact,
		     const pj_str_t *mime_type, const pj_str_t *text)
{
    /* Note: call index may be -1 */
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);
    PJ_UNUSED_ARG(mime_type);

    PJ_LOG(3,(THIS_FILE,"MESSAGE from %.*s: %.*s",
	      (int)from->slen, from->ptr,
	      (int)text->slen, text->ptr));
}


/**
 * Received typing indication
 */
static void on_typing(pjsua_call_id call_id, const pj_str_t *from,
		      const pj_str_t *to, const pj_str_t *contact,
		      pj_bool_t is_typing)
{
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);

    PJ_LOG(3,(THIS_FILE, "IM indication: %.*s %s",
	      (int)from->slen, from->ptr,
	      (is_typing?"is typing..":"has stopped typing")));
}


/*
 * Print buddy list.
 */
static void print_buddy_list(void)
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

	    printf(" [%2d] <%7s>  %.*s\n", 
		    ids[i]+1, info.status_text.ptr, 
		    (int)info.uri.slen,
		    info.uri.ptr);
	}
    }
    puts("");
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
    printf("       Online status: %s\n", 
	   (info.online_status ? "Online" : "Invisible"));
}


/*
 * Show a bit of help.
 */
static void keystroke_help(void)
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
    puts("|  a  Answer call              | !b  Modify buddy         | !a  Modify accnt. |");
    puts("|  h  Hangup call  (ha=all)    |  i  Send IM              | rr  (Re-)register |");
    puts("|  H  Hold call                |  s  Subscribe presence   | ru  Unregister    |");
    puts("|  v  re-inVite (release hold) |  u  Unsubscribe presence |  >  Cycle next ac.|");
    puts("|  ]  Select next dialog       |  t  ToGgle Online status |  <  Cycle prev ac.|");
    puts("|  [  Select previous dialog   +--------------------------+-------------------+");
    puts("|  x  Xfer call                |      Media Commands:     |  Status & Config: |");
    puts("|  #  Send DTMF string         |                          |                   |");
    puts("|                              | cl  List ports           |  d  Dump status   |");
    puts("|                              | cc  Connect port         | dd  Dump detailed |");
    puts("|                              | cd  Disconnect port      | dc  Dump config   |");
    puts("|                              |                          |  f  Save config   |");
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

	result->nb_result = my_atoi(buf);

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

/*
 * List the ports in conference bridge
 */
static void conf_list(void)
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


/*
 * Main "user interface" loop.
 */
void console_app_main(const pj_str_t *uri_to_call)
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
	pjsua_call_make_call( current_acc, uri_to_call, 0, NULL, NULL, NULL);
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
	    pjsua_call_make_call( current_acc, &tmp, 0, NULL, NULL, NULL);
	    break;

	case 'M':
	    /* Make multiple calls! : */
	    printf("(You currently have %d calls)\n", 
		   pjsua_call_get_count());
	    
	    if (!simple_input("Number of calls", menuin, sizeof(menuin)))
		continue;

	    count = my_atoi(menuin);
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

	    for (i=0; i<my_atoi(menuin); ++i) {
		pj_status_t status;
	    
		tmp = pj_str(uri);
		status = pjsua_call_make_call(current_acc, &tmp, 0, NULL,
					      NULL, NULL);
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
		continue;
	    }

	    tmp = pj_str(text);

	    /* Send the IM */
	    if (i != -1)
		pjsua_call_send_im(i, NULL, &tmp, NULL, NULL);
	    else {
		pj_str_t tmp_uri = pj_str(uri);
		pjsua_im_send(current_acc, &tmp_uri, NULL, &tmp, NULL, NULL);
	    }

	    break;

	case 'a':

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
		continue;

	    } else {
		if (!simple_input("Answer with code (100-699)", buf, sizeof(buf)))
		    continue;
		
		if (my_atoi(buf) < 100)
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

		pjsua_call_answer(current_call, my_atoi(buf), NULL, NULL);
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
		pjsua_call_hangup(current_call, 0, NULL, NULL);
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


	case '>':
	case '<':
	    if (!simple_input("Enter account ID to select", buf, sizeof(buf)))
		break;

	    i = my_atoi(buf);
	    if (pjsua_acc_is_valid(i)) {
		current_acc = i;
		PJ_LOG(3,(THIS_FILE, "Current account changed to %d", i));
	    } else {
		PJ_LOG(3,(THIS_FILE, "Invalid account id %d", i));
	    }
	    break;


	case '+':
	    if (menuin[1] == 'b') {
	    
		pjsua_buddy_config buddy_cfg;
		pjsua_buddy_id buddy_id;
		pj_status_t status;

		if (!simple_input("Enter buddy's URI:", buf, sizeof(buf)))
		    break;

		if (pjsua_verify_sip_url(buf) != PJ_SUCCESS) {
		    printf("Invalid SIP URI '%s'\n", buf);
		    break;
		}

		pj_memset(&buddy_cfg, 0, sizeof(pjsua_buddy_config));

		buddy_cfg.uri = pj_str(buf);
		buddy_cfg.subscribe = PJ_TRUE;

		status = pjsua_buddy_add(&buddy_cfg, &buddy_id);
		if (status == PJ_SUCCESS) {
		    printf("New buddy '%s' added at index %d\n",
			   buf, buddy_id+1);
		}

	    } else if (menuin[1] == 'a') {

		printf("Sorry, this command is not supported yet\n");

	    } else {
		printf("Invalid input %s\n", menuin);
	    }
	    break;

	case '-':
	    if (menuin[1] == 'b') {
		if (!simple_input("Enter buddy ID to delete",buf,sizeof(buf)))
		    break;

		i = my_atoi(buf) - 1;

		if (!pjsua_buddy_is_valid(i)) {
		    printf("Invalid buddy id %d\n", i);
		} else {
		    pjsua_buddy_del(i);
		    printf("Buddy %d deleted\n", i);
		}

	    } else if (menuin[1] == 'a') {

		if (!simple_input("Enter account ID to delete",buf,sizeof(buf)))
		    break;

		i = my_atoi(buf);

		if (!pjsua_acc_is_valid(i)) {
		    printf("Invalid account id %d\n", i);
		} else {
		    pjsua_acc_del(i);
		    printf("Account %d deleted\n", i);
		}

	    } else {
		printf("Invalid input %s\n", menuin);
	    }
	    break;

	case 'H':
	    /*
	     * Hold call.
	     */
	    if (current_call != -1) {
		
		pjsua_call_set_hold(current_call, NULL);

	    } else {
		PJ_LOG(3,(THIS_FILE, "No current call"));
	    }
	    break;

	case 'v':
	    /*
	     * Send re-INVITE (to release hold, etc).
	     */
	    if (current_call != -1) {
		
		pjsua_call_reinvite(current_call, PJ_TRUE, NULL);

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
			pjsua_call_xfer( current_call, &binfo.uri, NULL);
		    }

		} else if (result.uri_result) {
		    pj_str_t tmp;
		    tmp = pj_str(result.uri_result);
		    pjsua_call_xfer( current_call, &tmp, NULL);
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
		   acc_info.acc_uri.ptr,
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
			status = pjsua_conf_connect(my_atoi(src_port), 
						    my_atoi(dst_port));
		    } else {
			status = pjsua_conf_disconnect(my_atoi(src_port), 
						       my_atoi(dst_port));
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

		len = write_settings(&app_config, settings, sizeof(settings));
		if (len < 1)
		    PJ_LOG(3,(THIS_FILE, "Error: not enough buffer"));
		else
		    PJ_LOG(3,(THIS_FILE, 
			      "Dumping configuration (%d bytes):\n%s\n",
			      len, settings));
	    } else {
		app_dump(menuin[1]=='d');
	    }
	    break;


	case 'f':
	    if (simple_input("Enter output filename", buf, sizeof(buf))) {
		char settings[2000];
		int len;

		len = write_settings(&app_config, settings, sizeof(settings));
		if (len < 1)
		    PJ_LOG(3,(THIS_FILE, "Error: not enough buffer"));
		else {
		    pj_oshandle_t fd;
		    pj_status_t status;

		    status = pj_file_open(app_config.pool, buf, 
					  PJ_O_WRONLY, &fd);
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
 * Public API
 */

pj_status_t app_init(int argc, char *argv[])
{
    pjsua_transport_id transport_id;
    unsigned i;
    pj_status_t status;

    /* Create pjsua */
    status = pjsua_create();
    if (status != PJ_SUCCESS)
	return status;

    /* Create pool for application */
    app_config.pool = pjsua_pool_create("pjsua", 4000, 4000);

    /* Initialize default config */
    default_config(&app_config);
    
    /* Parse the arguments */
    status = parse_args(argc, argv, &app_config, &uri_arg);
    if (status != PJ_SUCCESS)
	return status;

    /* Copy udp_cfg STUN config to rtp_cfg */
    app_config.rtp_cfg.use_stun = app_config.udp_cfg.use_stun;
    app_config.rtp_cfg.stun_config = app_config.udp_cfg.stun_config;


    /* Initialize application callbacks */
    app_config.cfg.cb.on_call_state = &on_call_state;
    app_config.cfg.cb.on_call_media_state = &on_call_media_state;
    app_config.cfg.cb.on_incoming_call = &on_incoming_call;
    app_config.cfg.cb.on_reg_state = &on_reg_state;
    app_config.cfg.cb.on_buddy_state = &on_buddy_state;
    app_config.cfg.cb.on_pager = &on_pager;
    app_config.cfg.cb.on_typing = &on_typing;

    /* Initialize pjsua */
    status = pjsua_init(&app_config.cfg, &app_config.log_cfg,
			&app_config.media_cfg);
    if (status != PJ_SUCCESS)
	return status;

    /* Optionally registers WAV file */
    if (app_config.wav_file.slen) {
	status = pjsua_player_create(&app_config.wav_file, 0, NULL,
				     &app_config.wav_id);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Add UDP transport */
    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP,
				    &app_config.udp_cfg, 
				    &transport_id);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Add local account */
    pjsua_acc_add_local(transport_id, PJ_TRUE, &current_acc);
    pjsua_acc_set_online_status(current_acc, PJ_TRUE);


    /* Add accounts */
    for (i=0; i<app_config.acc_cnt; ++i) {
	status = pjsua_acc_add(&app_config.acc_cfg[i], PJ_TRUE, &current_acc);
	if (status != PJ_SUCCESS)
	    goto on_error;
	pjsua_acc_set_online_status(current_acc, PJ_TRUE);
    }

    /* Add buddies */
    for (i=0; i<app_config.buddy_cnt; ++i) {
	status = pjsua_buddy_add(&app_config.buddy_cfg[i], NULL);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Optionally set codec orders */
    for (i=0; i<app_config.codec_cnt; ++i) {
	pjsua_codec_set_priority(&app_config.codec_arg[i],
				 (pj_uint8_t)(PJMEDIA_CODEC_PRIO_NORMAL+i+9));
    }

    /* Add RTP transports */
    status = pjsua_media_transports_create(&app_config.rtp_cfg);
    if (status != PJ_SUCCESS)
	goto on_error;

    return PJ_SUCCESS;

on_error:
    pjsua_destroy();
    return status;
}


pj_status_t app_main(void)
{
    pj_status_t status;

    /* Start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) {
	pjsua_destroy();
	return status;
    }

    console_app_main(&uri_arg);

    return PJ_SUCCESS;
}

pj_status_t app_destroy(void)
{
    if (app_config.pool) {
	pj_pool_release(app_config.pool);
	app_config.pool = NULL;
    }

    return pjsua_destroy();
}

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
#include <pjsua-lib/getopt.h>
#include <stdio.h>

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
    puts  ("  --id=url            Set the URL of local ID (used in From header)");
    puts  ("  --contact=url       Override the Contact information");
    puts  ("  --proxy=url         Set the URL of proxy server");
    puts  ("");
    puts  ("SIP Account Registration Options:");
    puts  ("  --registrar=url     Set the URL of registrar server");
    puts  ("  --reg-timeout=secs  Set registration interval to secs (default 3600)");
    puts  ("");
    puts  ("SIP Account Control:");
    puts  ("  --next-account      Add more account");
    puts  ("");
    puts  ("Authentication options:");
    puts  ("  --realm=string      Set realm");
    puts  ("  --username=string   Set authentication username");
    puts  ("  --password=string   Set authentication password");
    puts  ("  --next-cred         Add more credential");
    puts  ("");
    puts  ("Transport Options:");
    puts  ("  --local-port=port        Set TCP/UDP port");
    puts  ("  --outbound=url           Set the URL of outbound proxy server");
    puts  ("  --use-stun1=host[:port]");
    puts  ("  --use-stun2=host[:port]  Resolve local IP with the specified STUN servers");
    puts  ("");
    puts  ("Media Options:");
    puts  ("  --wb                Enable wideband codecs and set clock-rate to 16KHz");
    puts  ("  --uwb               Enable ultra-wideband codecs and set clock-rate to 32KHz");
    puts  ("  --clock-rate=N      Override sound device clock rate");
    puts  ("  --null-audio        Use NULL audio device");
    puts  ("  --no-mic            Disable microphone device");
    puts  ("  --play-file=file    Play WAV file in conference bridge");
    puts  ("  --auto-play         Automatically play the file (to incoming calls only)");
    puts  ("  --auto-loop         Automatically loop incoming RTP to outgoing RTP");
    puts  ("  --auto-conf         Automatically put incoming calls to conference");
    puts  ("  --rtp-port=N        Base port to try for RTP (default=4000)");
    puts  ("  --add-codec=name    Specify alternate codec order");
    puts  ("  --complexity=N      Specify encoding complexity (0-10, default=none(-1))");
    puts  ("  --quality=N         Specify encoding quality (0-10, default=4)");
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

static int my_atoi(const char *cs)
{
    pj_str_t s;
    return pj_strtoul(pj_cstr(&s, cs));
}


/* Parse arguments. */
pj_status_t pjsua_parse_args(int argc, char *argv[])
{
    int c;
    int option_index;
    enum { OPT_CONFIG_FILE, OPT_LOG_FILE, OPT_LOG_LEVEL, OPT_APP_LOG_LEVEL, 
	   OPT_HELP, OPT_VERSION, OPT_NULL_AUDIO, OPT_NO_MIC,
	   OPT_LOCAL_PORT, OPT_PROXY, OPT_OUTBOUND_PROXY, OPT_REGISTRAR,
	   OPT_REG_TIMEOUT, OPT_ID, OPT_CONTACT, 
	   OPT_REALM, OPT_USERNAME, OPT_PASSWORD,
	   OPT_USE_STUN1, OPT_USE_STUN2, 
	   OPT_ADD_BUDDY, OPT_OFFER_X_MS_MSG, OPT_NO_PRESENCE,
	   OPT_AUTO_ANSWER, OPT_AUTO_HANGUP, OPT_AUTO_PLAY, OPT_AUTO_LOOP,
	   OPT_AUTO_CONF, OPT_CLOCK_RATE,
	   OPT_PLAY_FILE, OPT_WB, OPT_UWB, OPT_RTP_PORT, OPT_ADD_CODEC,
	   OPT_COMPLEXITY, OPT_QUALITY,
	   OPT_NEXT_ACCOUNT, OPT_NEXT_CRED, OPT_MAX_CALLS, OPT_UAS_REFRESH,
	   OPT_UAS_DURATION,
    };
    struct pj_getopt_option long_options[] = {
	{ "config-file",1, 0, OPT_CONFIG_FILE},
	{ "log-file",	1, 0, OPT_LOG_FILE},
	{ "log-level",	1, 0, OPT_LOG_LEVEL},
	{ "app-log-level",1,0,OPT_APP_LOG_LEVEL},
	{ "help",	0, 0, OPT_HELP},
	{ "version",	0, 0, OPT_VERSION},
	{ "wb",		0, 0, OPT_WB},
	{ "uwb",	0, 0, OPT_UWB},
	{ "clock-rate",	1, 0, OPT_CLOCK_RATE},
	{ "null-audio", 0, 0, OPT_NULL_AUDIO},
	{ "no-mic",	0, 0, OPT_NO_MIC},
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
	{ "next-account",0,0, OPT_NEXT_ACCOUNT},
	{ "next-cred",	0, 0, OPT_NEXT_CRED},
	{ "max-calls",	1, 0, OPT_MAX_CALLS},
	{ "uas-refresh",1, 0, OPT_UAS_REFRESH},
	{ "uas-duration",1,0, OPT_UAS_DURATION},
	{ NULL, 0, 0, 0}
    };
    pj_status_t status;
    pjsua_acc *cur_acc;
    pjsip_cred_info *cur_cred;
    char *config_file = NULL;

    /* Run pj_getopt once to see if user specifies config file to read. */ 
    while ((c=pj_getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
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


    cur_acc = &pjsua.acc[0];
    cur_cred = &pjsua.cred_info[0];


    /* Reinitialize and re-run pj_getopt again, possibly with new arguments
     * read from config file.
     */
    pj_optind = 0;
    while((c=pj_getopt_long(argc, argv, "", long_options, &option_index))!=-1) {
	char *p;
	pj_str_t tmp;
	long lval;

	switch (c) {

	case OPT_LOG_FILE:
	    pjsua.log_filename = pj_optarg;
	    break;

	case OPT_LOG_LEVEL:
	    c = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (c < 0 || c > 6) {
		printf("Error: expecting integer value 0-6 for --log-level\n");
		return PJ_EINVAL;
	    }
	    pj_log_set_level( c );
	    break;

	case OPT_APP_LOG_LEVEL:
	    pjsua.app_log_level = pj_strtoul(pj_cstr(&tmp, pj_optarg));
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

	case OPT_NO_MIC:
	    pjsua.no_mic = 1;
	    break;

	case OPT_WB:
	    pjsua.has_wb = 1;
	    break;

	case OPT_UWB:
	    pjsua.has_uwb = 1;
	    break;

	case OPT_CLOCK_RATE:
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 8000 || lval > 48000) {
		printf("Error: expecting value between 8000-48000 for clock rate\n");
		return PJ_EINVAL;
	    }
	    pjsua.clock_rate = (int)lval; 
	    break;

	case OPT_LOCAL_PORT:   /* local-port */
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 1 || lval > 65535) {
		printf("Error: expecting integer value for --local-port\n");
		return PJ_EINVAL;
	    }
	    pjsua.sip_port = (pj_uint16_t)lval;
	    break;

	case OPT_PROXY:   /* proxy */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in proxy argument\n", pj_optarg);
		return PJ_EINVAL;
	    }
	    cur_acc->proxy = pj_str(pj_optarg);
	    break;

	case OPT_OUTBOUND_PROXY:   /* outbound proxy */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in outbound proxy argument\n", pj_optarg);
		return PJ_EINVAL;
	    }
	    pjsua.outbound_proxy = pj_str(pj_optarg);
	    break;

	case OPT_REGISTRAR:   /* registrar */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in registrar argument\n", pj_optarg);
		return PJ_EINVAL;
	    }
	    cur_acc->reg_uri = pj_str(pj_optarg);
	    break;

	case OPT_REG_TIMEOUT:   /* reg-timeout */
	    cur_acc->reg_timeout = pj_strtoul(pj_cstr(&tmp,pj_optarg));
	    if (cur_acc->reg_timeout < 1 || cur_acc->reg_timeout > 3600) {
		printf("Error: invalid value for --reg-timeout (expecting 1-3600)\n");
		return PJ_EINVAL;
	    }
	    break;

	case OPT_ID:   /* id */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in local id argument\n", pj_optarg);
		return PJ_EINVAL;
	    }
	    cur_acc->local_uri = pj_str(pj_optarg);
	    pjsua.has_acc = 1;
	    break;

	case OPT_CONTACT:   /* contact */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		printf("Error: invalid SIP URL '%s' in contact argument\n", pj_optarg);
		return PJ_EINVAL;
	    }
	    cur_acc->contact_uri = pj_str(pj_optarg);
	    break;

	case OPT_NEXT_ACCOUNT: /* Add more account. */
	    pjsua.acc_cnt++;
	    cur_acc = &pjsua.acc[pjsua.acc_cnt - 1];
	    break;

	case OPT_USERNAME:   /* Default authentication user */
	    if (pjsua.cred_count==0) pjsua.cred_count=1;
	    cur_cred->username = pj_str(pj_optarg);
	    break;

	case OPT_REALM:	    /* Default authentication realm. */
	    if (pjsua.cred_count==0) pjsua.cred_count=1;
	    cur_cred->realm = pj_str(pj_optarg);
	    break;

	case OPT_PASSWORD:   /* authentication password */
	    if (pjsua.cred_count==0) pjsua.cred_count=1;
	    cur_cred->data_type = 0;
	    cur_cred->data = pj_str(pj_optarg);
	    break;

	case OPT_NEXT_CRED: /* Next credential */
	    pjsua.cred_count++;
	    cur_cred = &pjsua.cred_info[pjsua.cred_count - 1];
	    break;

	case OPT_USE_STUN1:   /* STUN server 1 */
	    p = pj_ansi_strchr(pj_optarg, ':');
	    if (p) {
		*p = '\0';
		pjsua.stun_srv1 = pj_str(pj_optarg);
		pjsua.stun_port1 = pj_strtoul(pj_cstr(&tmp, p+1));
		if (pjsua.stun_port1 < 1 || pjsua.stun_port1 > 65535) {
		    printf("Error: expecting port number with option --use-stun1\n");
		    return PJ_EINVAL;
		}
	    } else {
		pjsua.stun_port1 = 3478;
		pjsua.stun_srv1 = pj_str(pj_optarg);
	    }
	    break;

	case OPT_USE_STUN2:   /* STUN server 2 */
	    p = pj_ansi_strchr(pj_optarg, ':');
	    if (p) {
		*p = '\0';
		pjsua.stun_srv2 = pj_str(pj_optarg);
		pjsua.stun_port2 = pj_strtoul(pj_cstr(&tmp,p+1));
		if (pjsua.stun_port2 < 1 || pjsua.stun_port2 > 65535) {
		    printf("Error: expecting port number with option --use-stun2\n");
		    return PJ_EINVAL;
		}
	    } else {
		pjsua.stun_port2 = 3478;
		pjsua.stun_srv2 = pj_str(pj_optarg);
	    }
	    break;

	case OPT_ADD_BUDDY: /* Add to buddy list. */
	    if (pjsua_verify_sip_url(pj_optarg) != 0) {
		printf("Error: invalid URL '%s' in --add-buddy option\n", pj_optarg);
		return -1;
	    }
	    if (pjsua.buddy_cnt == PJSUA_MAX_BUDDIES) {
		printf("Error: too many buddies in buddy list.\n");
		return -1;
	    }
	    pjsua.buddies[pjsua.buddy_cnt++].uri = pj_str(pj_optarg);
	    break;

	case OPT_AUTO_PLAY:
	    pjsua.auto_play = 1;
	    break;

	case OPT_AUTO_LOOP:
	    pjsua.auto_loop = 1;
	    break;

	case OPT_AUTO_CONF:
	    pjsua.auto_conf = 1;
	    break;

	case OPT_PLAY_FILE:
	    pjsua.wav_file = pj_optarg;
	    break;

	case OPT_RTP_PORT:
	    pjsua.start_rtp_port = my_atoi(pj_optarg);
	    if (pjsua.start_rtp_port < 1 || pjsua.start_rtp_port > 65535) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: rtp-port argument value (expecting 1-65535"));
		return -1;
	    }
	    break;

	case OPT_ADD_CODEC:
	    pjsua.codec_arg[pjsua.codec_cnt++] = pj_str(pj_optarg);
	    break;

	case OPT_COMPLEXITY:
	    pjsua.complexity = my_atoi(pj_optarg);
	    if (pjsua.complexity < 0 || pjsua.complexity > 10) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --complexity (expecting 0-10"));
		return -1;
	    }
	    break;

	case OPT_QUALITY:
	    pjsua.quality = my_atoi(pj_optarg);
	    if (pjsua.quality < 0 || pjsua.quality > 10) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --quality (expecting 0-10"));
		return -1;
	    }
	    break;

	case OPT_AUTO_ANSWER:
	    pjsua.auto_answer = my_atoi(pj_optarg);
	    if (pjsua.auto_answer < 100 || pjsua.auto_answer > 699) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid code in --auto-answer (expecting 100-699"));
		return -1;
	    }
	    break;

	case OPT_MAX_CALLS:
	    pjsua.max_calls = my_atoi(pj_optarg);
	    if (pjsua.max_calls < 1 || pjsua.max_calls > 255) {
		PJ_LOG(1,(THIS_FILE,"Too many calls for max-calls (1-255)"));
		return -1;
	    }
	    break;

	case OPT_UAS_REFRESH:
	    pjsua.uas_refresh = my_atoi(pj_optarg);
	    if (pjsua.uas_refresh < 1) {
		PJ_LOG(1,(THIS_FILE,"Invalid value for --uas-refresh (must be >0)"));
		return -1;
	    }
	    break;

	case OPT_UAS_DURATION:
	    pjsua.uas_duration = my_atoi(pj_optarg);
	    if (pjsua.uas_duration < 1) {
		PJ_LOG(1,(THIS_FILE,"Invalid value for --uas-duration (must be >0)"));
		return -1;
	    }
	    break;
	}
    }

    if (pj_optind != argc) {
	printf("Error: unknown options %s\n", argv[pj_optind]);
	return PJ_EINVAL;
    }

    /* Adjust clock rate */
    if (pjsua.clock_rate == 8000 && pjsua.has_uwb)
	pjsua.clock_rate = 32000;
    else if (pjsua.clock_rate == 8000 && pjsua.has_wb)
	pjsua.clock_rate = 16000;

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
	char stxpkt[10], stxoct[10], srxpkt[10], srxoct[10];

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
		  "%s#%d %.*s @%dKHz, %s, peer=%s:%d",
		  "               ",
		  i,
		  info.stream_info[i].fmt.encoding_name.slen,
		  info.stream_info[i].fmt.encoding_name.ptr,
		  info.stream_info[i].fmt.sample_rate / 1000,
		  dir,
		  rem_addr, rem_port));
	PJ_LOG(3,(THIS_FILE, 
		  "%s tx{pt=%d,pkt=%s,oct=%s} rx{pt=%d,pkt=%s,oct=%s}",
		  "                 ",
		  info.stream_info[i].tx_pt,
		  good_number(stxpkt, strm_stat.enc.pkt), 
		  good_number(stxoct, strm_stat.enc.bytes),
		  info.stream_info[i].fmt.pt,
		  good_number(srxpkt, strm_stat.dec.pkt), 
		  good_number(srxoct, strm_stat.dec.bytes)));

    }
}

/*
 * Dump application states.
 */
void pjsua_dump(pj_bool_t detail)
{
    char buf[128];
    unsigned old_decor;

    PJ_LOG(3,(THIS_FILE, "Start dumping application states:"));

    old_decor = pj_log_get_decor();
    pj_log_set_decor(old_decor & (PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));

    pjsip_endpt_dump(pjsua.endpt, detail);
    pjmedia_endpt_dump(pjsua.med_endpt);
    pjsip_tsx_layer_dump(detail);
    pjsip_ua_dump(detail);


    /* Dump all invite sessions: */
    if (detail) {
	PJ_LOG(3,(THIS_FILE, "Dumping invite sessions:"));

	if (pjsua.call_cnt == 0) {

	    PJ_LOG(3,(THIS_FILE, "  - no sessions -"));

	} else {
	    int i;

	    for (i=0; i<pjsua.max_calls; ++i) {

		if (pjsua.calls[i].inv == NULL)
		    continue;

		print_call("  ", i, buf, sizeof(buf));
		PJ_LOG(3,(THIS_FILE, "%s", buf));

		if (pjsua.calls[i].session)
		    dump_media_session(pjsua.calls[i].session);
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
pj_status_t pjsua_load_settings(const char *filename)
{
    int argc = 3;
    char *argv[4] = { "pjsua", "--config-file", NULL, NULL};

    argv[3] = (char*)filename;
    return pjsua_parse_args(argc, argv);
}


/*
 * Save account settings
 */
static void save_account_settings(int acc_index, pj_str_t *result)
{
    char line[128];
    pjsua_acc *acc = &pjsua.acc[acc_index];

    
    pj_ansi_sprintf(line, "#\n# Account %d:\n#\n", acc_index);
    pj_strcat2(result, line);


    /* Identity */
    if (acc->local_uri.slen) {
	pj_ansi_sprintf(line, "--id %.*s\n", 
			(int)acc->local_uri.slen, 
			acc->local_uri.ptr);
	pj_strcat2(result, line);
    }

    /* Registrar server */
    if (acc->reg_uri.slen) {
	pj_ansi_sprintf(line, "--registrar %.*s\n",
			      (int)acc->reg_uri.slen,
			      acc->reg_uri.ptr);
	pj_strcat2(result, line);

	pj_ansi_sprintf(line, "--reg-timeout %u\n",
			      acc->reg_timeout);
	pj_strcat2(result, line);
    }


    /* Proxy */
    if (acc->proxy.slen) {
	pj_ansi_sprintf(line, "--proxy %.*s\n",
			      (int)acc->proxy.slen,
			      acc->proxy.ptr);
	pj_strcat2(result, line);
    }
}



/*
 * Dump settings.
 */
int pjsua_dump_settings(char *buf, pj_size_t max)
{
    int acc_index;
    int i;
    pj_str_t cfg;
    char line[128];

    cfg.ptr = buf;
    cfg.slen = 0;


    /* Logging. */
    pj_strcat2(&cfg, "#\n# Logging options:\n#\n");
    pj_ansi_sprintf(line, "--log-level %d\n",
		    pjsua.log_level);
    pj_strcat2(&cfg, line);

    pj_ansi_sprintf(line, "--app-log-level %d\n",
		    pjsua.app_log_level);
    pj_strcat2(&cfg, line);

    if (pjsua.log_filename) {
	pj_ansi_sprintf(line, "--log-file %s\n",
			pjsua.log_filename);
	pj_strcat2(&cfg, line);
    }


    /* Save account settings. */
    if (pjsua.has_acc) {
	for (acc_index=0; acc_index < pjsua.acc_cnt; ++acc_index) {
	    
	    save_account_settings(acc_index, &cfg);

	    if (acc_index < pjsua.acc_cnt-1)
		pj_strcat2(&cfg, "--next-account\n");
	}
    }

    /* Credentials. */
    for (i=0; i<pjsua.cred_count; ++i) {

	pj_ansi_sprintf(line, "#\n# Credential %d:\n#\n", i);
	pj_strcat2(&cfg, line);

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

	if (i < pjsua.cred_count-1)
	    pj_strcat2(&cfg, "--next-cred\n");
    }


    pj_strcat2(&cfg, "#\n# Network settings:\n#\n");

    /* Outbound proxy */
    if (pjsua.outbound_proxy.slen) {
	pj_ansi_sprintf(line, "--outbound %.*s\n",
			      (int)pjsua.outbound_proxy.slen,
			      pjsua.outbound_proxy.ptr);
	pj_strcat2(&cfg, line);
    }


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


    pj_strcat2(&cfg, "#\n# Media settings:\n#\n");


    /* Media */
    if (pjsua.null_audio)
	pj_strcat2(&cfg, "--null-audio\n");
    if (pjsua.auto_play)
	pj_strcat2(&cfg, "--auto-play\n");
    if (pjsua.auto_loop)
	pj_strcat2(&cfg, "--auto-loop\n");
    if (pjsua.auto_conf)
	pj_strcat2(&cfg, "--auto-conf\n");
    if (pjsua.wav_file) {
	pj_ansi_sprintf(line, "--play-file %s\n",
			pjsua.wav_file);
	pj_strcat2(&cfg, line);
    }
    /* Media clock rate. */
    if (pjsua.has_uwb)
	pj_strcat2(&cfg, "--uwb\n");

    if (pjsua.has_wb)
	pj_strcat2(&cfg, "--wb\n");

    pj_ansi_sprintf(line, "--clock-rate %d\n",
		    pjsua.clock_rate);
    pj_strcat2(&cfg, line);


    /* Encoding quality and complexity */
    pj_ansi_sprintf(line, "--quality %d\n",
		    pjsua.quality);
    pj_strcat2(&cfg, line);
    pj_ansi_sprintf(line, "--complexity %d\n",
		    pjsua.complexity);
    pj_strcat2(&cfg, line);

    /* Start RTP port. */
    pj_ansi_sprintf(line, "--rtp-port %d\n",
		    pjsua.start_rtp_port);
    pj_strcat2(&cfg, line);

    /* Add codec. */
    for (i=0; i<pjsua.codec_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-codec %s\n",
		    pjsua.codec_arg[i].ptr);
	pj_strcat2(&cfg, line);
    }

    pj_strcat2(&cfg, "#\n# User agent:\n#\n");

    /* Auto-answer. */
    if (pjsua.auto_answer != 0) {
	pj_ansi_sprintf(line, "--auto-answer %d\n",
			pjsua.auto_answer);
	pj_strcat2(&cfg, line);
    }

    /* Max calls. */
    pj_ansi_sprintf(line, "--max-calls %d\n",
		    pjsua.max_calls);
    pj_strcat2(&cfg, line);

    /* Uas-refresh. */
    if (pjsua.uas_refresh > 0) {
	pj_ansi_sprintf(line, "--uas-refresh %d\n",
			pjsua.uas_refresh);
	pj_strcat2(&cfg, line);
    }

    /* Uas-duration. */
    if (pjsua.uas_duration > 0) {
	pj_ansi_sprintf(line, "--uas-duration %d\n",
			pjsua.uas_duration);
	pj_strcat2(&cfg, line);
    }

    pj_strcat2(&cfg, "#\n# Buddies:\n#\n");

    /* Add buddies. */
    for (i=0; i<pjsua.buddy_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-buddy %.*s\n",
			      (int)pjsua.buddies[i].uri.slen,
			      pjsua.buddies[i].uri.ptr);
	pj_strcat2(&cfg, line);
    }


    *(cfg.ptr + cfg.slen) = '\0';
    return cfg.slen;
}

/*
 * Save settings.
 */
pj_status_t pjsua_save_settings(const char *filename)
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


    cfg.slen = pjsua_dump_settings(cfg.ptr, 3800);
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

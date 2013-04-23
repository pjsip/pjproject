/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include "pjsua_app_common.h"

#define THIS_FILE	"pjsua_app_config.c"

#define MAX_APP_OPTIONS 128

char   *stdout_refresh_text = "STDOUT_REFRESH";

/* Show usage */
static void usage(void)
{
    puts  ("Usage:");
    puts  ("  pjsua [options] [SIP URL to call]");
    puts  ("");
    puts  ("General options:");
    puts  ("  --config-file=file  Read the config/arguments from file.");
    puts  ("  --help              Display this help screen");
    puts  ("  --version           Display version info");
    puts  ("");
    puts  ("Logging options:");
    puts  ("  --log-file=fname    Log to filename (default stderr)");
    puts  ("  --log-level=N       Set log max level to N (0(none) to 6(trace)) (default=5)");
    puts  ("  --app-log-level=N   Set log max level for stdout display (default=4)");
    puts  ("  --log-append        Append instead of overwrite existing log file.\n");
    puts  ("  --color             Use colorful logging (default yes on Win32)");
    puts  ("  --no-color          Disable colorful logging");
    puts  ("  --light-bg          Use dark colors for light background (default is dark bg)");
    puts  ("  --no-stderr         Disable stderr");

    puts  ("");
    puts  ("SIP Account options:");
    puts  ("  --registrar=url     Set the URL of registrar server");
    puts  ("  --id=url            Set the URL of local ID (used in From header)");
    puts  ("  --realm=string      Set realm");
    puts  ("  --username=string   Set authentication username");
    puts  ("  --password=string   Set authentication password");
    puts  ("  --contact=url       Optionally override the Contact information");
    puts  ("  --contact-params=S  Append the specified parameters S in Contact header");
    puts  ("  --contact-uri-params=S  Append the specified parameters S in Contact URI");
    puts  ("  --proxy=url         Optional URL of proxy server to visit");
    puts  ("                      May be specified multiple times");
    printf("  --reg-timeout=SEC   Optional registration interval (default %d)\n",
	    PJSUA_REG_INTERVAL);
    printf("  --rereg-delay=SEC   Optional auto retry registration interval (default %d)\n",
	    PJSUA_REG_RETRY_INTERVAL);
    puts  ("  --reg-use-proxy=N   Control the use of proxy settings in REGISTER.");
    puts  ("                      0=no proxy, 1=outbound only, 2=acc only, 3=all (default)");
    puts  ("  --publish           Send presence PUBLISH for this account");
    puts  ("  --mwi               Subscribe to message summary/waiting indication");
    puts  ("  --use-ims           Enable 3GPP/IMS related settings on this account");
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    puts  ("  --use-srtp=N        Use SRTP?  0:disabled, 1:optional, 2:mandatory,");
    puts  ("                      3:optional by duplicating media offer (def:0)");
    puts  ("  --srtp-secure=N     SRTP require secure SIP? 0:no, 1:tls, 2:sips (def:1)");
#endif
    puts  ("  --use-100rel        Require reliable provisional response (100rel)");
    puts  ("  --use-timer=N       Use SIP session timers? (default=1)");
    puts  ("                      0:inactive, 1:optional, 2:mandatory, 3:always");
    printf("  --timer-se=N        Session timers expiration period, in secs (def:%d)\n",
	    PJSIP_SESS_TIMER_DEF_SE);
    puts  ("  --timer-min-se=N    Session timers minimum expiration period, in secs (def:90)");
    puts  ("  --outb-rid=string   Set SIP outbound reg-id (default:1)");
    puts  ("  --auto-update-nat=N Where N is 0 or 1 to enable/disable SIP traversal behind");
    puts  ("                      symmetric NAT (default 1)");
    puts  ("  --disable-stun      Disable STUN for this account");
    puts  ("  --next-cred         Add another credentials");
    puts  ("");
    puts  ("SIP Account Control:");
    puts  ("  --next-account      Add more account");
    puts  ("");
    puts  ("Transport Options:");
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6
    puts  ("  --ipv6              Use IPv6 instead for SIP and media.");
#endif
    puts  ("  --set-qos           Enable QoS tagging for SIP and media.");
    puts  ("  --local-port=port   Set TCP/UDP port. This implicitly enables both ");
    puts  ("                      TCP and UDP transports on the specified port, unless");
    puts  ("                      if TCP or UDP is disabled.");
    puts  ("  --ip-addr=IP        Use the specifed address as SIP and RTP addresses.");
    puts  ("                      (Hint: the IP may be the public IP of the NAT/router)");
    puts  ("  --bound-addr=IP     Bind transports to this IP interface");
    puts  ("  --no-tcp            Disable TCP transport.");
    puts  ("  --no-udp            Disable UDP transport.");
    puts  ("  --nameserver=NS     Add the specified nameserver to enable SRV resolution");
    puts  ("                      This option can be specified multiple times.");
    puts  ("  --outbound=url      Set the URL of global outbound proxy server");
    puts  ("                      May be specified multiple times");
    puts  ("  --stun-srv=FORMAT   Set STUN server host or domain. This option may be");
    puts  ("                      specified more than once. FORMAT is hostdom[:PORT]");

#if defined(PJSIP_HAS_TLS_TRANSPORT) && (PJSIP_HAS_TLS_TRANSPORT != 0)
    puts  ("");
    puts  ("TLS Options:");
    puts  ("  --use-tls           Enable TLS transport (default=no)");
    puts  ("  --tls-ca-file       Specify TLS CA file (default=none)");
    puts  ("  --tls-cert-file     Specify TLS certificate file (default=none)");
    puts  ("  --tls-privkey-file  Specify TLS private key file (default=none)");
    puts  ("  --tls-password      Specify TLS password to private key file (default=none)");
    puts  ("  --tls-verify-server Verify server's certificate (default=no)");
    puts  ("  --tls-verify-client Verify client's certificate (default=no)");
    puts  ("  --tls-neg-timeout   Specify TLS negotiation timeout (default=no)");
    puts  ("  --tls-srv-name      Specify TLS server name for multihosting server");
    puts  ("  --tls-cipher        Specify prefered TLS cipher (optional).");
    puts  ("                      May be specified multiple times");
#endif

    puts  ("");
    puts  ("Audio Options:");
    puts  ("  --add-codec=name    Manually add codec (default is to enable all)");
    puts  ("  --dis-codec=name    Disable codec (can be specified multiple times)");
    puts  ("  --clock-rate=N      Override conference bridge clock rate");
    puts  ("  --snd-clock-rate=N  Override sound device clock rate");
    puts  ("  --stereo            Audio device and conference bridge opened in stereo mode");
    puts  ("  --null-audio        Use NULL audio device");
    puts  ("  --play-file=file    Register WAV file in conference bridge.");
    puts  ("                      This can be specified multiple times.");
    puts  ("  --play-tone=FORMAT  Register tone to the conference bridge.");
    puts  ("                      FORMAT is 'F1,F2,ON,OFF', where F1,F2 are");
    puts  ("                      frequencies, and ON,OFF=on/off duration in msec.");
    puts  ("                      This can be specified multiple times.");
    puts  ("  --auto-play         Automatically play the file (to incoming calls only)");
    puts  ("  --auto-loop         Automatically loop incoming RTP to outgoing RTP");
    puts  ("  --auto-conf         Automatically put calls in conference with others");
    puts  ("  --rec-file=file     Open file recorder (extension can be .wav or .mp3");
    puts  ("  --auto-rec          Automatically record conversation");
    puts  ("  --quality=N         Specify media quality (0-10, default=6)");
    puts  ("  --ptime=MSEC        Override codec ptime to MSEC (default=specific)");
    puts  ("  --no-vad            Disable VAD/silence detector (default=vad enabled)");
    puts  ("  --ec-tail=MSEC      Set echo canceller tail length (default=256)");
    puts  ("  --ec-opt=OPT        Select echo canceller algorithm (0=default, ");
    puts  ("                        1=speex, 2=suppressor)");
    puts  ("  --ilbc-mode=MODE    Set iLBC codec mode (20 or 30, default is 30)");
    puts  ("  --capture-dev=id    Audio capture device ID (default=-1)");
    puts  ("  --playback-dev=id   Audio playback device ID (default=-1)");
    puts  ("  --capture-lat=N     Audio capture latency, in ms (default=100)");
    puts  ("  --playback-lat=N    Audio playback latency, in ms (default=100)");
    puts  ("  --snd-auto-close=N  Auto close audio device when idle for N secs (default=1)");
    puts  ("                      Specify N=-1 to disable this feature.");
    puts  ("                      Specify N=0 for instant close when unused.");
    puts  ("  --no-tones          Disable audible tones");
    puts  ("  --jb-max-size       Specify jitter buffer maximum size, in frames (default=-1)");
    puts  ("  --extra-audio       Add one more audio stream");

#if PJSUA_HAS_VIDEO
    puts  ("");
    puts  ("Video Options:");
    puts  ("  --video             Enable video");
    puts  ("  --vcapture-dev=id   Video capture device ID (default=-1)");
    puts  ("  --vrender-dev=id    Video render device ID (default=-2)");
    puts  ("  --play-avi=FILE     Load this AVI as virtual capture device");
    puts  ("  --auto-play-avi     Automatically play the AVI media to call");
#endif

    puts  ("");
    puts  ("Media Transport Options:");
    puts  ("  --use-ice           Enable ICE (default:no)");
    puts  ("  --ice-regular       Use ICE regular nomination (default: aggressive)");
    puts  ("  --ice-max-hosts=N   Set maximum number of ICE host candidates");
    puts  ("  --ice-no-rtcp       Disable RTCP component in ICE (default: no)");
    puts  ("  --rtp-port=N        Base port to try for RTP (default=4000)");
    puts  ("  --rx-drop-pct=PCT   Drop PCT percent of RX RTP (for pkt lost sim, default: 0)");
    puts  ("  --tx-drop-pct=PCT   Drop PCT percent of TX RTP (for pkt lost sim, default: 0)");
    puts  ("  --use-turn          Enable TURN relay with ICE (default:no)");
    puts  ("  --turn-srv          Domain or host name of TURN server (\"NAME:PORT\" format)");
    puts  ("  --turn-tcp          Use TCP connection to TURN server (default no)");
    puts  ("  --turn-user         TURN username");
    puts  ("  --turn-passwd       TURN password");

    puts  ("");
    puts  ("Buddy List (can be more than one):");
    puts  ("  --add-buddy url     Add the specified URL to the buddy list.");
    puts  ("");
    puts  ("User Agent options:");
    puts  ("  --auto-answer=code  Automatically answer incoming calls with code (e.g. 200)");
    puts  ("  --max-calls=N       Maximum number of concurrent calls (default:4, max:255)");
    puts  ("  --thread-cnt=N      Number of worker threads (default:1)");
    puts  ("  --duration=SEC      Set maximum call duration (default:no limit)");
    puts  ("  --norefersub        Suppress event subscription when transfering calls");
    puts  ("  --use-compact-form  Minimize SIP message size");
    puts  ("  --no-force-lr       Allow strict-route to be used (i.e. do not force lr)");
    puts  ("  --accept-redirect=N Specify how to handle call redirect (3xx) response.");
    puts  ("                      0: reject, 1: follow automatically,");
    puts  ("                      2: follow + replace To header (default), 3: ask");

    puts  ("");
    puts  ("CLI options:");
    puts  ("  --use-cli           Use CLI as user interface");
    puts  ("  --cli-telnet-port=N CLI telnet port");
    puts  ("  --no-cli-console    Disable CLI console");
    puts  ("");

    puts  ("");
    puts  ("When URL is specified, pjsua will immediately initiate call to that URL");
    puts  ("");

    fflush(stdout);
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
    enum { MAX_ARGS = 128 };

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
	char  *token;
	char  *p;
	const char *whitespace = " \t\r\n";
	char  cDelimiter;
	int   len, token_len;
	
	pj_bzero(line, sizeof(line));
	if (fgets(line, sizeof(line), fhnd) == NULL) break;
	
	// Trim ending newlines
	len = strlen(line);
	if (line[len-1]=='\n')
	    line[--len] = '\0';
	if (line[len-1]=='\r')
	    line[--len] = '\0';

	if (len==0) continue;

	for (p = line; *p != '\0' && argc < MAX_ARGS; p++) {
	    // first, scan whitespaces
	    while (*p != '\0' && strchr(whitespace, *p) != NULL) p++;

	    if (*p == '\0')		    // are we done yet?
		break;
	    
	    if (*p == '"' || *p == '\'') {    // is token a quoted string
		cDelimiter = *p++;	    // save quote delimiter
		token = p;
		
		while (*p != '\0' && *p != cDelimiter) p++;
		
		if (*p == '\0')		// found end of the line, but,
		    cDelimiter = '\0';	// didn't find a matching quote

	    } else {			// token's not a quoted string
		token = p;
		
		while (*p != '\0' && strchr(whitespace, *p) == NULL) p++;
		
		cDelimiter = *p;
	    }
	    
	    *p = '\0';
	    token_len = p-token;
	    
	    if (token_len > 0) {
		if (*token == '#')
		    break;  // ignore remainder of line
		
		argv[argc] = pj_pool_alloc(pool, token_len + 1);
		pj_memcpy(argv[argc], token, token_len + 1);
		++argc;
	    }
	    
	    *p = cDelimiter;
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

/* Parse arguments. */
static pj_status_t parse_args(int argc, char *argv[], 			      
			      pj_str_t *uri_to_call)
{
    int c;
    int option_index;
    pjsua_app_config *cfg = &app_config;
    enum { OPT_CONFIG_FILE=127, OPT_LOG_FILE, OPT_LOG_LEVEL, OPT_APP_LOG_LEVEL, 
	   OPT_LOG_APPEND, OPT_COLOR, OPT_NO_COLOR, OPT_LIGHT_BG, OPT_NO_STDERR,
	   OPT_HELP, OPT_VERSION, OPT_NULL_AUDIO, OPT_SND_AUTO_CLOSE,
	   OPT_LOCAL_PORT, OPT_IP_ADDR, OPT_PROXY, OPT_OUTBOUND_PROXY, 
	   OPT_REGISTRAR, OPT_REG_TIMEOUT, OPT_PUBLISH, OPT_ID, OPT_CONTACT,
	   OPT_BOUND_ADDR, OPT_CONTACT_PARAMS, OPT_CONTACT_URI_PARAMS,
	   OPT_100REL, OPT_USE_IMS, OPT_REALM, OPT_USERNAME, OPT_PASSWORD,
	   OPT_REG_RETRY_INTERVAL, OPT_REG_USE_PROXY,
	   OPT_MWI, OPT_NAMESERVER, OPT_STUN_SRV, OPT_OUTB_RID,
	   OPT_ADD_BUDDY, OPT_OFFER_X_MS_MSG, OPT_NO_PRESENCE,
	   OPT_AUTO_ANSWER, OPT_AUTO_PLAY, OPT_AUTO_PLAY_HANGUP, OPT_AUTO_LOOP,
	   OPT_AUTO_CONF, OPT_CLOCK_RATE, OPT_SND_CLOCK_RATE, OPT_STEREO,
	   OPT_USE_ICE, OPT_ICE_REGULAR, OPT_USE_SRTP, OPT_SRTP_SECURE,
	   OPT_USE_TURN, OPT_ICE_MAX_HOSTS, OPT_ICE_NO_RTCP, OPT_TURN_SRV, 
	   OPT_TURN_TCP, OPT_TURN_USER, OPT_TURN_PASSWD,
	   OPT_PLAY_FILE, OPT_PLAY_TONE, OPT_RTP_PORT, OPT_ADD_CODEC, 
	   OPT_ILBC_MODE, OPT_REC_FILE, OPT_AUTO_REC,
	   OPT_COMPLEXITY, OPT_QUALITY, OPT_PTIME, OPT_NO_VAD,
	   OPT_RX_DROP_PCT, OPT_TX_DROP_PCT, OPT_EC_TAIL, OPT_EC_OPT,
	   OPT_NEXT_ACCOUNT, OPT_NEXT_CRED, OPT_MAX_CALLS, 
	   OPT_DURATION, OPT_NO_TCP, OPT_NO_UDP, OPT_THREAD_CNT,
	   OPT_NOREFERSUB, OPT_ACCEPT_REDIRECT,
	   OPT_USE_TLS, OPT_TLS_CA_FILE, OPT_TLS_CERT_FILE, OPT_TLS_PRIV_FILE,
	   OPT_TLS_PASSWORD, OPT_TLS_VERIFY_SERVER, OPT_TLS_VERIFY_CLIENT,
	   OPT_TLS_NEG_TIMEOUT, OPT_TLS_CIPHER,
	   OPT_CAPTURE_DEV, OPT_PLAYBACK_DEV,
	   OPT_CAPTURE_LAT, OPT_PLAYBACK_LAT, OPT_NO_TONES, OPT_JB_MAX_SIZE,
	   OPT_STDOUT_REFRESH, OPT_STDOUT_REFRESH_TEXT, OPT_IPV6, OPT_QOS,
#ifdef _IONBF
	   OPT_STDOUT_NO_BUF,
#endif
	   OPT_AUTO_UPDATE_NAT,OPT_USE_COMPACT_FORM,OPT_DIS_CODEC,
	   OPT_DISABLE_STUN, OPT_NO_FORCE_LR,
	   OPT_TIMER, OPT_TIMER_SE, OPT_TIMER_MIN_SE,
	   OPT_VIDEO, OPT_EXTRA_AUDIO,
	   OPT_VCAPTURE_DEV, OPT_VRENDER_DEV, OPT_PLAY_AVI, OPT_AUTO_PLAY_AVI,
	   OPT_USE_CLI, OPT_CLI_TELNET_PORT, OPT_DISABLE_CLI_CONSOLE
    };
    struct pj_getopt_option long_options[] = {
	{ "config-file",1, 0, OPT_CONFIG_FILE},
	{ "log-file",	1, 0, OPT_LOG_FILE},
	{ "log-level",	1, 0, OPT_LOG_LEVEL},
	{ "app-log-level",1,0,OPT_APP_LOG_LEVEL},
	{ "log-append", 0, 0, OPT_LOG_APPEND},
	{ "color",	0, 0, OPT_COLOR},
	{ "no-color",	0, 0, OPT_NO_COLOR},
	{ "light-bg",		0, 0, OPT_LIGHT_BG},
	{ "no-stderr",  0, 0, OPT_NO_STDERR},
	{ "help",	0, 0, OPT_HELP},
	{ "version",	0, 0, OPT_VERSION},
	{ "clock-rate",	1, 0, OPT_CLOCK_RATE},
	{ "snd-clock-rate",	1, 0, OPT_SND_CLOCK_RATE},
	{ "stereo",	0, 0, OPT_STEREO},
	{ "null-audio", 0, 0, OPT_NULL_AUDIO},
	{ "local-port", 1, 0, OPT_LOCAL_PORT},
	{ "ip-addr",	1, 0, OPT_IP_ADDR},
	{ "bound-addr", 1, 0, OPT_BOUND_ADDR},
	{ "no-tcp",     0, 0, OPT_NO_TCP},
	{ "no-udp",     0, 0, OPT_NO_UDP},
	{ "norefersub", 0, 0, OPT_NOREFERSUB},
	{ "proxy",	1, 0, OPT_PROXY},
	{ "outbound",	1, 0, OPT_OUTBOUND_PROXY},
	{ "registrar",	1, 0, OPT_REGISTRAR},
	{ "reg-timeout",1, 0, OPT_REG_TIMEOUT},
	{ "publish",    0, 0, OPT_PUBLISH},
	{ "mwi",	0, 0, OPT_MWI},
	{ "use-100rel", 0, 0, OPT_100REL},
	{ "use-ims",    0, 0, OPT_USE_IMS},
	{ "id",		1, 0, OPT_ID},
	{ "contact",	1, 0, OPT_CONTACT},
	{ "contact-params",1,0, OPT_CONTACT_PARAMS},
	{ "contact-uri-params",1,0, OPT_CONTACT_URI_PARAMS},
	{ "auto-update-nat",	1, 0, OPT_AUTO_UPDATE_NAT},
	{ "disable-stun",0,0, OPT_DISABLE_STUN},
        { "use-compact-form",	0, 0, OPT_USE_COMPACT_FORM},
	{ "accept-redirect", 1, 0, OPT_ACCEPT_REDIRECT},
	{ "no-force-lr",0, 0, OPT_NO_FORCE_LR},
	{ "realm",	1, 0, OPT_REALM},
	{ "username",	1, 0, OPT_USERNAME},
	{ "password",	1, 0, OPT_PASSWORD},
	{ "rereg-delay",1, 0, OPT_REG_RETRY_INTERVAL},
	{ "reg-use-proxy", 1, 0, OPT_REG_USE_PROXY},
	{ "nameserver", 1, 0, OPT_NAMESERVER},
	{ "stun-srv",   1, 0, OPT_STUN_SRV},
	{ "add-buddy",  1, 0, OPT_ADD_BUDDY},
	{ "offer-x-ms-msg",0,0,OPT_OFFER_X_MS_MSG},
	{ "no-presence", 0, 0, OPT_NO_PRESENCE},
	{ "auto-answer",1, 0, OPT_AUTO_ANSWER},
	{ "auto-play",  0, 0, OPT_AUTO_PLAY},
	{ "auto-play-hangup",0, 0, OPT_AUTO_PLAY_HANGUP},
	{ "auto-rec",   0, 0, OPT_AUTO_REC},
	{ "auto-loop",  0, 0, OPT_AUTO_LOOP},
	{ "auto-conf",  0, 0, OPT_AUTO_CONF},
	{ "play-file",  1, 0, OPT_PLAY_FILE},
	{ "play-tone",  1, 0, OPT_PLAY_TONE},
	{ "rec-file",   1, 0, OPT_REC_FILE},
	{ "rtp-port",	1, 0, OPT_RTP_PORT},

	{ "use-ice",    0, 0, OPT_USE_ICE},
	{ "ice-regular",0, 0, OPT_ICE_REGULAR},
	{ "use-turn",	0, 0, OPT_USE_TURN},
	{ "ice-max-hosts",1, 0, OPT_ICE_MAX_HOSTS},
	{ "ice-no-rtcp",0, 0, OPT_ICE_NO_RTCP},
	{ "turn-srv",	1, 0, OPT_TURN_SRV},
	{ "turn-tcp",	0, 0, OPT_TURN_TCP},
	{ "turn-user",	1, 0, OPT_TURN_USER},
	{ "turn-passwd",1, 0, OPT_TURN_PASSWD},

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
	{ "use-srtp",   1, 0, OPT_USE_SRTP},
	{ "srtp-secure",1, 0, OPT_SRTP_SECURE},
#endif
	{ "add-codec",  1, 0, OPT_ADD_CODEC},
	{ "dis-codec",  1, 0, OPT_DIS_CODEC},
	{ "complexity",	1, 0, OPT_COMPLEXITY},
	{ "quality",	1, 0, OPT_QUALITY},
	{ "ptime",      1, 0, OPT_PTIME},
	{ "no-vad",     0, 0, OPT_NO_VAD},
	{ "ec-tail",    1, 0, OPT_EC_TAIL},
	{ "ec-opt",	1, 0, OPT_EC_OPT},
	{ "ilbc-mode",	1, 0, OPT_ILBC_MODE},
	{ "rx-drop-pct",1, 0, OPT_RX_DROP_PCT},
	{ "tx-drop-pct",1, 0, OPT_TX_DROP_PCT},
	{ "next-account",0,0, OPT_NEXT_ACCOUNT},
	{ "next-cred",	0, 0, OPT_NEXT_CRED},
	{ "max-calls",	1, 0, OPT_MAX_CALLS},
	{ "duration",	1, 0, OPT_DURATION},
	{ "thread-cnt",	1, 0, OPT_THREAD_CNT},
#if defined(PJSIP_HAS_TLS_TRANSPORT) && (PJSIP_HAS_TLS_TRANSPORT != 0)
	{ "use-tls",	0, 0, OPT_USE_TLS}, 
	{ "tls-ca-file",1, 0, OPT_TLS_CA_FILE},
	{ "tls-cert-file",1,0, OPT_TLS_CERT_FILE}, 
	{ "tls-privkey-file",1,0, OPT_TLS_PRIV_FILE},
	{ "tls-password",1,0, OPT_TLS_PASSWORD},
	{ "tls-verify-server", 0, 0, OPT_TLS_VERIFY_SERVER},
	{ "tls-verify-client", 0, 0, OPT_TLS_VERIFY_CLIENT},
	{ "tls-neg-timeout", 1, 0, OPT_TLS_NEG_TIMEOUT},
	{ "tls-cipher", 1, 0, OPT_TLS_CIPHER},
#endif
	{ "capture-dev",    1, 0, OPT_CAPTURE_DEV},
	{ "playback-dev",   1, 0, OPT_PLAYBACK_DEV},
	{ "capture-lat",    1, 0, OPT_CAPTURE_LAT},
	{ "playback-lat",   1, 0, OPT_PLAYBACK_LAT},
	{ "stdout-refresh", 1, 0, OPT_STDOUT_REFRESH},
	{ "stdout-refresh-text", 1, 0, OPT_STDOUT_REFRESH_TEXT},
#ifdef _IONBF
	{ "stdout-no-buf",  0, 0, OPT_STDOUT_NO_BUF },
#endif
	{ "snd-auto-close", 1, 0, OPT_SND_AUTO_CLOSE},
	{ "no-tones",    0, 0, OPT_NO_TONES},
	{ "jb-max-size", 1, 0, OPT_JB_MAX_SIZE},
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6
	{ "ipv6",	 0, 0, OPT_IPV6},
#endif
	{ "set-qos",	 0, 0, OPT_QOS},
	{ "use-timer",  1, 0, OPT_TIMER},
	{ "timer-se",   1, 0, OPT_TIMER_SE},
	{ "timer-min-se", 1, 0, OPT_TIMER_MIN_SE},
	{ "outb-rid",	1, 0, OPT_OUTB_RID},
	{ "video",	0, 0, OPT_VIDEO},
	{ "extra-audio",0, 0, OPT_EXTRA_AUDIO},
	{ "vcapture-dev", 1, 0, OPT_VCAPTURE_DEV},
	{ "vrender-dev",  1, 0, OPT_VRENDER_DEV},
	{ "play-avi",	1, 0, OPT_PLAY_AVI},
	{ "auto-play-avi", 0, 0, OPT_AUTO_PLAY_AVI},
	{ "use-cli",	0, 0, OPT_USE_CLI},
	{ "cli-telnet-port", 1, 0, OPT_CLI_TELNET_PORT},
	{ "no-cli-console", 0, 0, OPT_DISABLE_CLI_CONSOLE},
	{ NULL, 0, 0, 0}
    };
    pj_status_t status;
    pjsua_acc_config *cur_acc;
    char *config_file = NULL;
    unsigned i;

    /* Run pj_getopt once to see if user specifies config file to read. */ 
    pj_optind = 0;
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
	status = read_config_file(cfg->pool, config_file, &argc, &argv);
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
	pj_str_t tmp;
	long lval;

	switch (c) {

	case OPT_CONFIG_FILE:
	    /* Ignore as this has been processed before */
	    break;
	
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

	case OPT_LOG_APPEND:
	    cfg->log_cfg.log_file_flags |= PJ_O_APPEND;
	    break;

	case OPT_COLOR:
	    cfg->log_cfg.decor |= PJ_LOG_HAS_COLOR;
	    break;

	case OPT_NO_COLOR:
	    cfg->log_cfg.decor &= ~PJ_LOG_HAS_COLOR;
	    break;

	case OPT_LIGHT_BG:
	    pj_log_set_color(1, PJ_TERM_COLOR_R);
	    pj_log_set_color(2, PJ_TERM_COLOR_R | PJ_TERM_COLOR_G);
	    pj_log_set_color(3, PJ_TERM_COLOR_B | PJ_TERM_COLOR_G);
	    pj_log_set_color(4, 0);
	    pj_log_set_color(5, 0);
	    pj_log_set_color(77, 0);
	    break;

	case OPT_NO_STDERR:
#if !defined(PJ_WIN32_WINCE) || PJ_WIN32_WINCE==0
	    freopen("/dev/null", "w", stderr);
#endif
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
	    if (lval < 8000 || lval > 192000) {
		PJ_LOG(1,(THIS_FILE, "Error: expecting value between "
				     "8000-192000 for conference clock rate"));
		return PJ_EINVAL;
	    }
	    cfg->media_cfg.clock_rate = lval; 
	    break;

	case OPT_SND_CLOCK_RATE:
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 8000 || lval > 192000) {
		PJ_LOG(1,(THIS_FILE, "Error: expecting value between "
				     "8000-192000 for sound device clock rate"));
		return PJ_EINVAL;
	    }
	    cfg->media_cfg.snd_clock_rate = lval; 
	    break;

	case OPT_STEREO:
	    cfg->media_cfg.channel_count = 2;
	    break;

	case OPT_LOCAL_PORT:   /* local-port */
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 0 || lval > 65535) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: expecting integer value for "
			  "--local-port"));
		return PJ_EINVAL;
	    }
	    cfg->udp_cfg.port = (pj_uint16_t)lval;
	    break;

	case OPT_IP_ADDR: /* ip-addr */
	    cfg->udp_cfg.public_addr = pj_str(pj_optarg);
	    cfg->rtp_cfg.public_addr = pj_str(pj_optarg);
	    break;

	case OPT_BOUND_ADDR: /* bound-addr */
	    cfg->udp_cfg.bound_addr = pj_str(pj_optarg);
	    cfg->rtp_cfg.bound_addr = pj_str(pj_optarg);
	    break;

	case OPT_NO_UDP: /* no-udp */
	    if (cfg->no_tcp && !cfg->use_tls) {
	      PJ_LOG(1,(THIS_FILE,"Error: cannot disable both TCP and UDP"));
	      return PJ_EINVAL;
	    }

	    cfg->no_udp = PJ_TRUE;
	    break;

	case OPT_NOREFERSUB: /* norefersub */
	    cfg->no_refersub = PJ_TRUE;
	    break;

	case OPT_NO_TCP: /* no-tcp */
	    if (cfg->no_udp && !cfg->use_tls) {
	      PJ_LOG(1,(THIS_FILE,"Error: cannot disable both TCP and UDP"));
	      return PJ_EINVAL;
	    }

	    cfg->no_tcp = PJ_TRUE;
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

	case OPT_PUBLISH:   /* publish */
	    cur_acc->publish_enabled = PJ_TRUE;
	    break;

	case OPT_MWI:	/* mwi */
	    cur_acc->mwi_enabled = PJ_TRUE;
	    break;

	case OPT_100REL: /** 100rel */
	    cur_acc->require_100rel = PJSUA_100REL_MANDATORY;
	    cfg->cfg.require_100rel = PJSUA_100REL_MANDATORY;
	    break;

	case OPT_TIMER: /** session timer */
	    lval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (lval < 0 || lval > 3) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: expecting integer value 0-3 for --use-timer"));
		return PJ_EINVAL;
	    }
	    cur_acc->use_timer = lval;
	    cfg->cfg.use_timer = lval;
	    break;

	case OPT_TIMER_SE: /** session timer session expiration */
	    cur_acc->timer_setting.sess_expires = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (cur_acc->timer_setting.sess_expires < 90) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid value for --timer-se "
			  "(expecting higher than 90)"));
		return PJ_EINVAL;
	    }
	    cfg->cfg.timer_setting.sess_expires = cur_acc->timer_setting.sess_expires;
	    break;

	case OPT_TIMER_MIN_SE: /** session timer minimum session expiration */
	    cur_acc->timer_setting.min_se = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (cur_acc->timer_setting.min_se < 90) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: invalid value for --timer-min-se "
			  "(expecting higher than 90)"));
		return PJ_EINVAL;
	    }
	    cfg->cfg.timer_setting.min_se = cur_acc->timer_setting.min_se;
	    break;

	case OPT_OUTB_RID: /* Outbound reg-id */
	    cur_acc->rfc5626_reg_id = pj_str(pj_optarg);
	    break;

	case OPT_USE_IMS: /* Activate IMS settings */
	    cur_acc->auth_pref.initial_auth = PJ_TRUE;
	    break;

	case OPT_ID:   /* id */
	    if (pjsua_verify_url(pj_optarg) != 0) {
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
	    cur_acc->force_contact = pj_str(pj_optarg);
	    break;

	case OPT_CONTACT_PARAMS:
	    cur_acc->contact_params = pj_str(pj_optarg);
	    break;

	case OPT_CONTACT_URI_PARAMS:
	    cur_acc->contact_uri_params = pj_str(pj_optarg);
	    break;

	case OPT_AUTO_UPDATE_NAT:   /* OPT_AUTO_UPDATE_NAT */
            cur_acc->allow_contact_rewrite  = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    break;

	case OPT_DISABLE_STUN:
	    cur_acc->sip_stun_use = PJSUA_STUN_USE_DISABLED;
	    cur_acc->media_stun_use = PJSUA_STUN_USE_DISABLED;
	    break;

	case OPT_USE_COMPACT_FORM:
	    /* enable compact form - from Ticket #342 */
            {
		extern pj_bool_t pjsip_use_compact_form;
		extern pj_bool_t pjsip_include_allow_hdr_in_dlg;
		extern pj_bool_t pjmedia_add_rtpmap_for_static_pt;

		pjsip_use_compact_form = PJ_TRUE;
		/* do not transmit Allow header */
		pjsip_include_allow_hdr_in_dlg = PJ_FALSE;
		/* Do not include rtpmap for static payload types (<96) */
		pjmedia_add_rtpmap_for_static_pt = PJ_FALSE;
            }
	    break;

	case OPT_ACCEPT_REDIRECT:
	    cfg->redir_op = my_atoi(pj_optarg);
	    if (cfg->redir_op<0 || cfg->redir_op>PJSIP_REDIRECT_STOP) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: accept-redirect value '%s' ", pj_optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_NO_FORCE_LR:
	    cfg->cfg.force_lr = PJ_FALSE;
	    break;

	case OPT_NEXT_ACCOUNT: /* Add more account. */
	    cfg->acc_cnt++;
	    cur_acc = &cfg->acc_cfg[cfg->acc_cnt];
	    break;

	case OPT_USERNAME:   /* Default authentication user */
	    cur_acc->cred_info[cur_acc->cred_count].username = pj_str(pj_optarg);
	    cur_acc->cred_info[cur_acc->cred_count].scheme = pj_str("Digest");
	    break;

	case OPT_REALM:	    /* Default authentication realm. */
	    cur_acc->cred_info[cur_acc->cred_count].realm = pj_str(pj_optarg);
	    break;

	case OPT_PASSWORD:   /* authentication password */
	    cur_acc->cred_info[cur_acc->cred_count].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	    cur_acc->cred_info[cur_acc->cred_count].data = pj_str(pj_optarg);
#if PJSIP_HAS_DIGEST_AKA_AUTH
	    cur_acc->cred_info[cur_acc->cred_count].data_type |= PJSIP_CRED_DATA_EXT_AKA;
	    cur_acc->cred_info[cur_acc->cred_count].ext.aka.k = pj_str(pj_optarg);
	    cur_acc->cred_info[cur_acc->cred_count].ext.aka.cb = &pjsip_auth_create_aka_response;
#endif
	    break;

	case OPT_REG_RETRY_INTERVAL:
	    cur_acc->reg_retry_interval = pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    break;

	case OPT_REG_USE_PROXY:
	    cur_acc->reg_use_proxy = (unsigned)pj_strtoul(pj_cstr(&tmp, pj_optarg));
	    if (cur_acc->reg_use_proxy > 3) {
		PJ_LOG(1,(THIS_FILE, "Error: invalid --reg-use-proxy value '%s'",
			  pj_optarg));
		return PJ_EINVAL;
	    }
	    break;

	case OPT_NEXT_CRED: /* next credential */
	    cur_acc->cred_count++;
	    break;

	case OPT_NAMESERVER: /* nameserver */
	    cfg->cfg.nameserver[cfg->cfg.nameserver_count++] = pj_str(pj_optarg);
	    if (cfg->cfg.nameserver_count > PJ_ARRAY_SIZE(cfg->cfg.nameserver)) {
		PJ_LOG(1,(THIS_FILE, "Error: too many nameservers"));
		return PJ_ETOOMANY;
	    }
	    break;

	case OPT_STUN_SRV:   /* STUN server */
	    cfg->cfg.stun_host = pj_str(pj_optarg);
	    if (cfg->cfg.stun_srv_cnt==PJ_ARRAY_SIZE(cfg->cfg.stun_srv)) {
		PJ_LOG(1,(THIS_FILE, "Error: too many STUN servers"));
		return PJ_ETOOMANY;
	    }
	    cfg->cfg.stun_srv[cfg->cfg.stun_srv_cnt++] = pj_str(pj_optarg);
	    break;

	case OPT_ADD_BUDDY: /* Add to buddy list. */
	    if (pjsua_verify_url(pj_optarg) != 0) {
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

	case OPT_AUTO_PLAY_HANGUP:
	    cfg->auto_play_hangup = 1;
	    break;

	case OPT_AUTO_REC:
	    cfg->auto_rec = 1;
	    break;

	case OPT_AUTO_LOOP:
	    cfg->auto_loop = 1;
	    break;

	case OPT_AUTO_CONF:
	    cfg->auto_conf = 1;
	    break;

	case OPT_PLAY_FILE:
	    cfg->wav_files[cfg->wav_count++] = pj_str(pj_optarg);
	    break;

	case OPT_PLAY_TONE:
	    {
		int f1, f2, on, off;
		int n;

		n = sscanf(pj_optarg, "%d,%d,%d,%d", &f1, &f2, &on, &off);
		if (n != 4) {
		    puts("Expecting f1,f2,on,off in --play-tone");
		    return -1;
		}

		cfg->tones[cfg->tone_count].freq1 = (short)f1;
		cfg->tones[cfg->tone_count].freq2 = (short)f2;
		cfg->tones[cfg->tone_count].on_msec = (short)on;
		cfg->tones[cfg->tone_count].off_msec = (short)off;
		++cfg->tone_count;
	    }
	    break;

	case OPT_REC_FILE:
	    cfg->rec_file = pj_str(pj_optarg);
	    break;

	case OPT_USE_ICE:
	    cfg->media_cfg.enable_ice =
		    cur_acc->ice_cfg.enable_ice = PJ_TRUE;
	    break;

	case OPT_ICE_REGULAR:
	    cfg->media_cfg.ice_opt.aggressive =
		    cur_acc->ice_cfg.ice_opt.aggressive = PJ_FALSE;
	    break;

	case OPT_USE_TURN:
	    cfg->media_cfg.enable_turn =
		    cur_acc->turn_cfg.enable_turn = PJ_TRUE;
	    break;

	case OPT_ICE_MAX_HOSTS:
	    cfg->media_cfg.ice_max_host_cands =
		    cur_acc->ice_cfg.ice_max_host_cands = my_atoi(pj_optarg);
	    break;

	case OPT_ICE_NO_RTCP:
	    cfg->media_cfg.ice_no_rtcp =
		    cur_acc->ice_cfg.ice_no_rtcp = PJ_TRUE;
	    break;

	case OPT_TURN_SRV:
	    cfg->media_cfg.turn_server =
		    cur_acc->turn_cfg.turn_server = pj_str(pj_optarg);
	    break;

	case OPT_TURN_TCP:
	    cfg->media_cfg.turn_conn_type =
		    cur_acc->turn_cfg.turn_conn_type = PJ_TURN_TP_TCP;
	    break;

	case OPT_TURN_USER:
	    cfg->media_cfg.turn_auth_cred.type =
		    cur_acc->turn_cfg.turn_auth_cred.type = PJ_STUN_AUTH_CRED_STATIC;
	    cfg->media_cfg.turn_auth_cred.data.static_cred.realm =
		    cur_acc->turn_cfg.turn_auth_cred.data.static_cred.realm = pj_str("*");
	    cfg->media_cfg.turn_auth_cred.data.static_cred.username =
		    cur_acc->turn_cfg.turn_auth_cred.data.static_cred.username = pj_str(pj_optarg);
	    break;

	case OPT_TURN_PASSWD:
	    cfg->media_cfg.turn_auth_cred.data.static_cred.data_type =
		    cur_acc->turn_cfg.turn_auth_cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
	    cfg->media_cfg.turn_auth_cred.data.static_cred.data =
		    cur_acc->turn_cfg.turn_auth_cred.data.static_cred.data = pj_str(pj_optarg);
	    break;

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
	case OPT_USE_SRTP:
	    app_config.cfg.use_srtp = my_atoi(pj_optarg);
	    if (!pj_isdigit(*pj_optarg) || app_config.cfg.use_srtp > 3) {
		PJ_LOG(1,(THIS_FILE, "Invalid value for --use-srtp option"));
		return -1;
	    }
	    if ((int)app_config.cfg.use_srtp == 3) {
		/* SRTP optional mode with duplicated media offer */
		app_config.cfg.use_srtp = PJMEDIA_SRTP_OPTIONAL;
		app_config.cfg.srtp_optional_dup_offer = PJ_TRUE;
		cur_acc->srtp_optional_dup_offer = PJ_TRUE;
	    }
	    cur_acc->use_srtp = app_config.cfg.use_srtp;
	    break;
	case OPT_SRTP_SECURE:
	    app_config.cfg.srtp_secure_signaling = my_atoi(pj_optarg);
	    if (!pj_isdigit(*pj_optarg) || 
		app_config.cfg.srtp_secure_signaling > 2) 
	    {
		PJ_LOG(1,(THIS_FILE, "Invalid value for --srtp-secure option"));
		return -1;
	    }
	    cur_acc->srtp_secure_signaling = app_config.cfg.srtp_secure_signaling;
	    break;
#endif

	case OPT_RTP_PORT:
	    cfg->rtp_cfg.port = my_atoi(pj_optarg);
	    if (cfg->rtp_cfg.port == 0) {
		enum { START_PORT=4000 };
		unsigned range;

		range = (65535-START_PORT-PJSUA_MAX_CALLS*2);
		cfg->rtp_cfg.port = START_PORT + 
				    ((pj_rand() % range) & 0xFFFE);
	    }

	    if (cfg->rtp_cfg.port < 1 || cfg->rtp_cfg.port > 65535) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: rtp-port argument value "
			  "(expecting 1-65535"));
		return -1;
	    }
	    break;

	case OPT_DIS_CODEC:
            cfg->codec_dis[cfg->codec_dis_cnt++] = pj_str(pj_optarg);
	    break;

	case OPT_ADD_CODEC:
	    cfg->codec_arg[cfg->codec_cnt++] = pj_str(pj_optarg);
	    break;

	/* These options were no longer valid after new pjsua */
	/*
	case OPT_COMPLEXITY:
	    cfg->complexity = my_atoi(pj_optarg);
	    if (cfg->complexity < 0 || cfg->complexity > 10) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --complexity (expecting 0-10"));
		return -1;
	    }
	    break;
	*/

	case OPT_DURATION:
	    cfg->duration = my_atoi(pj_optarg);
	    break;

	case OPT_THREAD_CNT:
	    cfg->cfg.thread_cnt = my_atoi(pj_optarg);
	    if (cfg->cfg.thread_cnt > 128) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --thread-cnt option"));
		return -1;
	    }
	    break;

	case OPT_PTIME:
	    cfg->media_cfg.ptime = my_atoi(pj_optarg);
	    if (cfg->media_cfg.ptime < 10 || cfg->media_cfg.ptime > 1000) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --ptime option"));
		return -1;
	    }
	    break;

	case OPT_NO_VAD:
	    cfg->media_cfg.no_vad = PJ_TRUE;
	    break;

	case OPT_EC_TAIL:
	    cfg->media_cfg.ec_tail_len = my_atoi(pj_optarg);
	    if (cfg->media_cfg.ec_tail_len > 1000) {
		PJ_LOG(1,(THIS_FILE, "I think the ec-tail length setting "
			  "is too big"));
		return -1;
	    }
	    break;

	case OPT_EC_OPT:
	    cfg->media_cfg.ec_options = my_atoi(pj_optarg);
	    break;

	case OPT_QUALITY:
	    cfg->media_cfg.quality = my_atoi(pj_optarg);
	    if (cfg->media_cfg.quality < 0 || cfg->media_cfg.quality > 10) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --quality (expecting 0-10"));
		return -1;
	    }
	    break;

	case OPT_ILBC_MODE:
	    cfg->media_cfg.ilbc_mode = my_atoi(pj_optarg);
	    if (cfg->media_cfg.ilbc_mode!=20 && cfg->media_cfg.ilbc_mode!=30) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --ilbc-mode (expecting 20 or 30"));
		return -1;
	    }
	    break;

	case OPT_RX_DROP_PCT:
	    cfg->media_cfg.rx_drop_pct = my_atoi(pj_optarg);
	    if (cfg->media_cfg.rx_drop_pct > 100) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --rx-drop-pct (expecting <= 100"));
		return -1;
	    }
	    break;
	    
	case OPT_TX_DROP_PCT:
	    cfg->media_cfg.tx_drop_pct = my_atoi(pj_optarg);
	    if (cfg->media_cfg.tx_drop_pct > 100) {
		PJ_LOG(1,(THIS_FILE,
			  "Error: invalid --tx-drop-pct (expecting <= 100"));
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
	    if (cfg->cfg.max_calls < 1 || cfg->cfg.max_calls > PJSUA_MAX_CALLS) {
		PJ_LOG(1,(THIS_FILE,"Error: maximum call setting exceeds "
				    "compile time limit (PJSUA_MAX_CALLS=%d)",
			  PJSUA_MAX_CALLS));
		return -1;
	    }
	    break;

#if defined(PJSIP_HAS_TLS_TRANSPORT) && (PJSIP_HAS_TLS_TRANSPORT != 0)
	case OPT_USE_TLS:
	    cfg->use_tls = PJ_TRUE;
	    break;
	    
	case OPT_TLS_CA_FILE:
	    cfg->udp_cfg.tls_setting.ca_list_file = pj_str(pj_optarg);
	    break;
	    
	case OPT_TLS_CERT_FILE:
	    cfg->udp_cfg.tls_setting.cert_file = pj_str(pj_optarg);
	    break;
	    
	case OPT_TLS_PRIV_FILE:
	    cfg->udp_cfg.tls_setting.privkey_file = pj_str(pj_optarg);
	    break;

	case OPT_TLS_PASSWORD:
	    cfg->udp_cfg.tls_setting.password = pj_str(pj_optarg);
	    break;

	case OPT_TLS_VERIFY_SERVER:
	    cfg->udp_cfg.tls_setting.verify_server = PJ_TRUE;
	    break;

	case OPT_TLS_VERIFY_CLIENT:
	    cfg->udp_cfg.tls_setting.verify_client = PJ_TRUE;
	    cfg->udp_cfg.tls_setting.require_client_cert = PJ_TRUE;
	    break;

	case OPT_TLS_NEG_TIMEOUT:
	    cfg->udp_cfg.tls_setting.timeout.sec = atoi(pj_optarg);
	    break;

	case OPT_TLS_CIPHER:
	    {
		pj_ssl_cipher cipher;

		if (pj_ansi_strnicmp(pj_optarg, "0x", 2) == 0) {
		    pj_str_t cipher_st = pj_str(pj_optarg + 2);
		    cipher = pj_strtoul2(&cipher_st, NULL, 16);
		} else {
		    cipher = atoi(pj_optarg);
		}

		if (pj_ssl_cipher_is_supported(cipher)) {
		    static pj_ssl_cipher tls_ciphers[128];

		    tls_ciphers[cfg->udp_cfg.tls_setting.ciphers_num++] = cipher;
		    cfg->udp_cfg.tls_setting.ciphers = tls_ciphers;
		} else {
		    pj_ssl_cipher ciphers[128];
		    unsigned j, ciphers_cnt;

		    ciphers_cnt = PJ_ARRAY_SIZE(ciphers);
		    pj_ssl_cipher_get_availables(ciphers, &ciphers_cnt);
		    
		    PJ_LOG(1,(THIS_FILE, "Cipher \"%s\" is not supported by "
					 "TLS/SSL backend.", pj_optarg));
		    printf("Available TLS/SSL ciphers (%d):\n", ciphers_cnt);
		    for (j=0; j<ciphers_cnt; ++j)
			printf("- 0x%06X: %s\n", ciphers[j], pj_ssl_cipher_name(ciphers[j]));
		    return -1;
		}
	    }
 	    break;
#endif /* PJSIP_HAS_TLS_TRANSPORT */

	case OPT_CAPTURE_DEV:
	    cfg->capture_dev = atoi(pj_optarg);
	    break;

	case OPT_PLAYBACK_DEV:
	    cfg->playback_dev = atoi(pj_optarg);
	    break;

	case OPT_STDOUT_REFRESH:
	    stdout_refresh = atoi(pj_optarg);
	    break;

	case OPT_STDOUT_REFRESH_TEXT:
	    stdout_refresh_text = pj_optarg;
	    break;

#ifdef _IONBF
	case OPT_STDOUT_NO_BUF:
	    setvbuf(stdout, NULL, _IONBF, 0);
	    break;
#endif

	case OPT_CAPTURE_LAT:
	    cfg->capture_lat = atoi(pj_optarg);
	    break;

	case OPT_PLAYBACK_LAT:
	    cfg->playback_lat = atoi(pj_optarg);
	    break;

	case OPT_SND_AUTO_CLOSE:
	    cfg->media_cfg.snd_auto_close_time = atoi(pj_optarg);
	    break;

	case OPT_NO_TONES:
	    cfg->no_tones = PJ_TRUE;
	    break;

	case OPT_JB_MAX_SIZE:
	    cfg->media_cfg.jb_max = atoi(pj_optarg);
	    break;

#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6
	case OPT_IPV6:
	    cfg->ipv6 = PJ_TRUE;
	    break;
#endif
	case OPT_QOS:
	    cfg->enable_qos = PJ_TRUE;
	    /* Set RTP traffic type to Voice */
	    cfg->rtp_cfg.qos_type = PJ_QOS_TYPE_VOICE;
	    /* Directly apply DSCP value to SIP traffic. Say lets
	     * set it to CS3 (DSCP 011000). Note that this will not 
	     * work on all platforms.
	     */
	    cfg->udp_cfg.qos_params.flags = PJ_QOS_PARAM_HAS_DSCP;
	    cfg->udp_cfg.qos_params.dscp_val = 0x18;
	    break;
	case OPT_VIDEO:
	    cfg->vid.vid_cnt = 1;
	    cfg->vid.in_auto_show = PJ_TRUE;
	    cfg->vid.out_auto_transmit = PJ_TRUE;
	    break;
	case OPT_EXTRA_AUDIO:
	    cfg->aud_cnt++;
	    break;

	case OPT_VCAPTURE_DEV:
	    cfg->vid.vcapture_dev = atoi(pj_optarg);
	    cur_acc->vid_cap_dev = cfg->vid.vcapture_dev;
	    break;

	case OPT_VRENDER_DEV:
	    cfg->vid.vrender_dev = atoi(pj_optarg);
	    cur_acc->vid_rend_dev = cfg->vid.vrender_dev;
	    break;

	case OPT_PLAY_AVI:
	    if (app_config.avi_cnt >= PJSUA_APP_MAX_AVI) {
		PJ_LOG(1,(THIS_FILE, "Too many AVIs"));
		return -1;
	    }
	    app_config.avi[app_config.avi_cnt++].path = pj_str(pj_optarg);
	    break;

	case OPT_AUTO_PLAY_AVI:
	    app_config.avi_auto_play = PJ_TRUE;
	    break;

	case OPT_USE_CLI:
	    cfg->use_cli = PJ_TRUE;
	    break;

	case OPT_CLI_TELNET_PORT:
	    cfg->cli_cfg.telnet_cfg.port = (pj_uint16_t)atoi(pj_optarg);
	    cfg->cli_cfg.cli_fe |= CLI_FE_TELNET;
	    break;

	case OPT_DISABLE_CLI_CONSOLE:
	    cfg->cli_cfg.cli_fe &= (~CLI_FE_CONSOLE);
	    break;

	default:
	    PJ_LOG(1,(THIS_FILE, 
		      "Argument \"%s\" is not valid. Use --help to see help",
		      argv[pj_optind-1]));
	    return -1;
	}
    }

    if (pj_optind != argc) {
	pj_str_t uri_arg;

	if (pjsua_verify_url(argv[pj_optind]) != PJ_SUCCESS) {
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

    if (cfg->acc_cfg[cfg->acc_cnt].id.slen)
	cfg->acc_cnt++;

    for (i=0; i<cfg->acc_cnt; ++i) {
	pjsua_acc_config *acfg = &cfg->acc_cfg[i];

	if (acfg->cred_info[acfg->cred_count].username.slen)
	{
	    acfg->cred_count++;
	}

	if (acfg->ice_cfg.enable_ice) {
	    acfg->ice_cfg_use = PJSUA_ICE_CONFIG_USE_CUSTOM;
	}
	if (acfg->turn_cfg.enable_turn) {
	    acfg->turn_cfg_use = PJSUA_TURN_CONFIG_USE_CUSTOM;
	}

	/* When IMS mode is enabled for the account, verify that settings
	 * are okay.
	 */
	/* For now we check if IMS mode is activated by looking if
	 * initial_auth is set.
	 */
	if (acfg->auth_pref.initial_auth && acfg->cred_count) {
	    /* Realm must point to the real domain */
	    if (*acfg->cred_info[0].realm.ptr=='*') {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: cannot use '*' as realm with IMS"));
		return PJ_EINVAL;
	    }

	    /* Username for authentication must be in a@b format */
	    if (strchr(acfg->cred_info[0].username.ptr, '@')==0) {
		PJ_LOG(1,(THIS_FILE, 
			  "Error: Username for authentication must "
			  "be in user@domain format with IMS"));
		return PJ_EINVAL;
	    }
	}
    }
    return PJ_SUCCESS;
}

/* Set default config. */
static void default_config()
{
    char tmp[80];
    unsigned i;
    pjsua_app_config *cfg = &app_config;

    pjsua_config_default(&cfg->cfg);
    pj_ansi_sprintf(tmp, "PJSUA v%s %s", pj_get_version(),
		    pj_get_sys_info()->info.ptr);
    pj_strdup2_with_null(app_config.pool, &cfg->cfg.user_agent, tmp);

    pjsua_logging_config_default(&cfg->log_cfg);
    pjsua_media_config_default(&cfg->media_cfg);
    pjsua_transport_config_default(&cfg->udp_cfg);
    cfg->udp_cfg.port = 5060;
    pjsua_transport_config_default(&cfg->rtp_cfg);
    cfg->rtp_cfg.port = 4000;
    cfg->redir_op = PJSIP_REDIRECT_ACCEPT_REPLACE;
    cfg->duration = PJSUA_APP_NO_LIMIT_DURATION;
    cfg->wav_id = PJSUA_INVALID_ID;
    cfg->rec_id = PJSUA_INVALID_ID;
    cfg->wav_port = PJSUA_INVALID_ID;
    cfg->rec_port = PJSUA_INVALID_ID;
    cfg->mic_level = cfg->speaker_level = 1.0;
    cfg->capture_dev = PJSUA_INVALID_ID;
    cfg->playback_dev = PJSUA_INVALID_ID;
    cfg->capture_lat = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    cfg->playback_lat = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
    cfg->ringback_slot = PJSUA_INVALID_ID;
    cfg->ring_slot = PJSUA_INVALID_ID;

    for (i=0; i<PJ_ARRAY_SIZE(cfg->acc_cfg); ++i)
	pjsua_acc_config_default(&cfg->acc_cfg[i]);

    for (i=0; i<PJ_ARRAY_SIZE(cfg->buddy_cfg); ++i)
	pjsua_buddy_config_default(&cfg->buddy_cfg[i]);

    cfg->vid.vcapture_dev = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
    cfg->vid.vrender_dev = PJMEDIA_VID_DEFAULT_RENDER_DEV;
    cfg->aud_cnt = 1;

    cfg->avi_def_idx = PJSUA_INVALID_ID;

    cfg->use_cli = PJ_FALSE;
    cfg->cli_cfg.cli_fe = CLI_FE_CONSOLE;
    cfg->cli_cfg.telnet_cfg.port = 0;
}

static pj_status_t parse_config(int argc, char *argv[], pj_str_t *uri_arg)
{
    pj_status_t status;

    /* Initialize default config */
    default_config(app_config);

    /* Parse the arguments */
    status = parse_args(argc, argv, uri_arg);
    return status;
}

pj_status_t load_config(int argc,
				char **argv,
				pj_str_t *uri_arg)
{
    pj_status_t status;
    pj_bool_t use_cli = PJ_FALSE;
    int cli_fe = 0;
    pj_uint16_t cli_telnet_port = 0;

    /** CLI options are not changable **/
    if (app_running) {
	use_cli = app_config.use_cli;
	cli_fe = app_config.cli_cfg.cli_fe;
	cli_telnet_port = app_config.cli_cfg.telnet_cfg.port;
    }

    status = parse_config(argc, argv, uri_arg);
    if (status != PJ_SUCCESS)
	return status;

    if (app_running) {    
	app_config.use_cli = use_cli;
	app_config.cli_cfg.cli_fe = cli_fe;
	app_config.cli_cfg.telnet_cfg.port = cli_telnet_port;
    }

    return status;
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
    if (acc_cfg->force_contact.slen) {
	pj_ansi_sprintf(line, "--contact %.*s\n",
			(int)acc_cfg->force_contact.slen,
			acc_cfg->force_contact.ptr);
	pj_strcat2(result, line);
    }

    /* Contact header parameters */
    if (acc_cfg->contact_params.slen) {
	pj_ansi_sprintf(line, "--contact-params %.*s\n",
			(int)acc_cfg->contact_params.slen,
			acc_cfg->contact_params.ptr);
	pj_strcat2(result, line);
    }

    /* Contact URI parameters */
    if (acc_cfg->contact_uri_params.slen) {
	pj_ansi_sprintf(line, "--contact-uri-params %.*s\n",
			(int)acc_cfg->contact_uri_params.slen,
			acc_cfg->contact_uri_params.ptr);
	pj_strcat2(result, line);
    }

    /*  */
    if (acc_cfg->allow_contact_rewrite!=1)
    {
	pj_ansi_sprintf(line, "--auto-update-nat %i\n",
			(int)acc_cfg->allow_contact_rewrite);
	pj_strcat2(result, line);
    }

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* SRTP */
    if (acc_cfg->use_srtp) {
	int use_srtp = (int)acc_cfg->use_srtp;
	if (use_srtp == PJMEDIA_SRTP_OPTIONAL &&
	    acc_cfg->srtp_optional_dup_offer)
	{
	    use_srtp = 3;
	}
	pj_ansi_sprintf(line, "--use-srtp %i\n", use_srtp);
	pj_strcat2(result, line);
    }
    if (acc_cfg->srtp_secure_signaling !=
	PJSUA_DEFAULT_SRTP_SECURE_SIGNALING)
    {
	pj_ansi_sprintf(line, "--srtp-secure %d\n",
			acc_cfg->srtp_secure_signaling);
	pj_strcat2(result, line);
    }
#endif

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

	if (i != acc_cfg->cred_count - 1)
	    pj_strcat2(result, "--next-cred\n");
    }

    /* reg-use-proxy */
    if (acc_cfg->reg_use_proxy != 3) {
	pj_ansi_sprintf(line, "--reg-use-proxy %d\n",
			      acc_cfg->reg_use_proxy);
	pj_strcat2(result, line);
    }

    /* rereg-delay */
    if (acc_cfg->reg_retry_interval != PJSUA_REG_RETRY_INTERVAL) {
	pj_ansi_sprintf(line, "--rereg-delay %d\n",
		              acc_cfg->reg_retry_interval);
	pj_strcat2(result, line);
    }

    /* 100rel extension */
    if (acc_cfg->require_100rel) {
	pj_strcat2(result, "--use-100rel\n");
    }

    /* Session Timer extension */
    if (acc_cfg->use_timer) {
	pj_ansi_sprintf(line, "--use-timer %d\n",
			      acc_cfg->use_timer);
	pj_strcat2(result, line);
    }
    if (acc_cfg->timer_setting.min_se != 90) {
	pj_ansi_sprintf(line, "--timer-min-se %d\n",
			      acc_cfg->timer_setting.min_se);
	pj_strcat2(result, line);
    }
    if (acc_cfg->timer_setting.sess_expires != PJSIP_SESS_TIMER_DEF_SE) {
	pj_ansi_sprintf(line, "--timer-se %d\n",
			      acc_cfg->timer_setting.sess_expires);
	pj_strcat2(result, line);
    }

    /* Publish */
    if (acc_cfg->publish_enabled)
	pj_strcat2(result, "--publish\n");

    /* MWI */
    if (acc_cfg->mwi_enabled)
	pj_strcat2(result, "--mwi\n");

    if (acc_cfg->sip_stun_use != PJSUA_STUN_USE_DEFAULT ||
	acc_cfg->media_stun_use != PJSUA_STUN_USE_DEFAULT)
    {
	pj_strcat2(result, "--disable-stun\n");
    }

    /* Media Transport*/
    if (acc_cfg->ice_cfg.enable_ice)
	pj_strcat2(result, "--use-ice\n");

    if (acc_cfg->ice_cfg.ice_opt.aggressive == PJ_FALSE)
	pj_strcat2(result, "--ice-regular\n");

    if (acc_cfg->turn_cfg.enable_turn)
	pj_strcat2(result, "--use-turn\n");

    if (acc_cfg->ice_cfg.ice_max_host_cands >= 0) {
	pj_ansi_sprintf(line, "--ice_max_host_cands %d\n",
	                acc_cfg->ice_cfg.ice_max_host_cands);
	pj_strcat2(result, line);
    }

    if (acc_cfg->ice_cfg.ice_no_rtcp)
	pj_strcat2(result, "--ice-no-rtcp\n");

    if (acc_cfg->turn_cfg.turn_server.slen) {
	pj_ansi_sprintf(line, "--turn-srv %.*s\n",
			(int)acc_cfg->turn_cfg.turn_server.slen,
			acc_cfg->turn_cfg.turn_server.ptr);
	pj_strcat2(result, line);
    }

    if (acc_cfg->turn_cfg.turn_conn_type == PJ_TURN_TP_TCP)
	pj_strcat2(result, "--turn-tcp\n");

    if (acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.username.slen) {
	pj_ansi_sprintf(line, "--turn-user %.*s\n",
			(int)acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.username.slen,
			acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.username.ptr);
	pj_strcat2(result, line);
    }

    if (acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.data.slen) {
	pj_ansi_sprintf(line, "--turn-passwd %.*s\n",
			(int)acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.data.slen,
			acc_cfg->turn_cfg.turn_auth_cred.data.static_cred.data.ptr);
	pj_strcat2(result, line);
    }
}

/*
 * Write settings.
 */
int write_settings(pjsua_app_config *config, char *buf, pj_size_t max)
{
    unsigned acc_index;
    unsigned i;
    pj_str_t cfg;
    char line[128];
    extern pj_bool_t pjsip_use_compact_form;

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

    if (config->log_cfg.log_file_flags & PJ_O_APPEND) {
	pj_strcat2(&cfg, "--log-append\n");
    }

    /* Save account settings. */
    for (acc_index=0; acc_index < config->acc_cnt; ++acc_index) {

	write_account_settings(acc_index, &cfg);

	if (acc_index < config->acc_cnt-1)
	    pj_strcat2(&cfg, "--next-account\n");
    }

    pj_strcat2(&cfg, "\n#\n# Network settings:\n#\n");

    /* Nameservers */
    for (i=0; i<config->cfg.nameserver_count; ++i) {
	pj_ansi_sprintf(line, "--nameserver %.*s\n",
			      (int)config->cfg.nameserver[i].slen,
			      config->cfg.nameserver[i].ptr);
	pj_strcat2(&cfg, line);
    }

    /* Outbound proxy */
    for (i=0; i<config->cfg.outbound_proxy_cnt; ++i) {
	pj_ansi_sprintf(line, "--outbound %.*s\n",
			      (int)config->cfg.outbound_proxy[i].slen,
			      config->cfg.outbound_proxy[i].ptr);
	pj_strcat2(&cfg, line);
    }

    /* Transport options */
    if (config->ipv6) {
	pj_strcat2(&cfg, "--ipv6\n");
    }
    if (config->enable_qos) {
	pj_strcat2(&cfg, "--set-qos\n");
    }

    /* UDP Transport. */
    pj_ansi_sprintf(line, "--local-port %d\n", config->udp_cfg.port);
    pj_strcat2(&cfg, line);

    /* IP address, if any. */
    if (config->udp_cfg.public_addr.slen) {
	pj_ansi_sprintf(line, "--ip-addr %.*s\n",
			(int)config->udp_cfg.public_addr.slen,
			config->udp_cfg.public_addr.ptr);
	pj_strcat2(&cfg, line);
    }

    /* Bound IP address, if any. */
    if (config->udp_cfg.bound_addr.slen) {
	pj_ansi_sprintf(line, "--bound-addr %.*s\n",
			(int)config->udp_cfg.bound_addr.slen,
			config->udp_cfg.bound_addr.ptr);
	pj_strcat2(&cfg, line);
    }

    /* No TCP ? */
    if (config->no_tcp) {
	pj_strcat2(&cfg, "--no-tcp\n");
    }

    /* No UDP ? */
    if (config->no_udp) {
	pj_strcat2(&cfg, "--no-udp\n");
    }

    /* STUN */
    for (i=0; i<config->cfg.stun_srv_cnt; ++i) {
	pj_ansi_sprintf(line, "--stun-srv %.*s\n",
			(int)config->cfg.stun_srv[i].slen,
			config->cfg.stun_srv[i].ptr);
	pj_strcat2(&cfg, line);
    }

#if defined(PJSIP_HAS_TLS_TRANSPORT) && (PJSIP_HAS_TLS_TRANSPORT != 0)
    /* TLS */
    if (config->use_tls)
	pj_strcat2(&cfg, "--use-tls\n");
    if (config->udp_cfg.tls_setting.ca_list_file.slen) {
	pj_ansi_sprintf(line, "--tls-ca-file %.*s\n",
			(int)config->udp_cfg.tls_setting.ca_list_file.slen,
			config->udp_cfg.tls_setting.ca_list_file.ptr);
	pj_strcat2(&cfg, line);
    }
    if (config->udp_cfg.tls_setting.cert_file.slen) {
	pj_ansi_sprintf(line, "--tls-cert-file %.*s\n",
			(int)config->udp_cfg.tls_setting.cert_file.slen,
			config->udp_cfg.tls_setting.cert_file.ptr);
	pj_strcat2(&cfg, line);
    }
    if (config->udp_cfg.tls_setting.privkey_file.slen) {
	pj_ansi_sprintf(line, "--tls-privkey-file %.*s\n",
			(int)config->udp_cfg.tls_setting.privkey_file.slen,
			config->udp_cfg.tls_setting.privkey_file.ptr);
	pj_strcat2(&cfg, line);
    }

    if (config->udp_cfg.tls_setting.password.slen) {
	pj_ansi_sprintf(line, "--tls-password %.*s\n",
			(int)config->udp_cfg.tls_setting.password.slen,
			config->udp_cfg.tls_setting.password.ptr);
	pj_strcat2(&cfg, line);
    }

    if (config->udp_cfg.tls_setting.verify_server)
	pj_strcat2(&cfg, "--tls-verify-server\n");

    if (config->udp_cfg.tls_setting.verify_client)
	pj_strcat2(&cfg, "--tls-verify-client\n");

    if (config->udp_cfg.tls_setting.timeout.sec) {
	pj_ansi_sprintf(line, "--tls-neg-timeout %d\n",
			(int)config->udp_cfg.tls_setting.timeout.sec);
	pj_strcat2(&cfg, line);
    }

    for (i=0; i<config->udp_cfg.tls_setting.ciphers_num; ++i) {
	pj_ansi_sprintf(line, "--tls-cipher 0x%06X # %s\n",
			config->udp_cfg.tls_setting.ciphers[i],
			pj_ssl_cipher_name(config->udp_cfg.tls_setting.ciphers[i]));
	pj_strcat2(&cfg, line);
    }
#endif

    pj_strcat2(&cfg, "\n#\n# Media settings:\n#\n");

    /* Video & extra audio */
    for (i=0; i<config->vid.vid_cnt; ++i) {
	pj_strcat2(&cfg, "--video\n");
    }
    for (i=1; i<config->aud_cnt; ++i) {
	pj_strcat2(&cfg, "--extra-audio\n");
    }

    /* SRTP */
#if PJMEDIA_HAS_SRTP
    if (app_config.cfg.use_srtp != PJSUA_DEFAULT_USE_SRTP) {
	int use_srtp = (int)app_config.cfg.use_srtp;
	if (use_srtp == PJMEDIA_SRTP_OPTIONAL &&
	    app_config.cfg.srtp_optional_dup_offer)
	{
	    use_srtp = 3;
	}
	pj_ansi_sprintf(line, "--use-srtp %d\n", use_srtp);
	pj_strcat2(&cfg, line);
    }
    if (app_config.cfg.srtp_secure_signaling !=
	PJSUA_DEFAULT_SRTP_SECURE_SIGNALING)
    {
	pj_ansi_sprintf(line, "--srtp-secure %d\n",
			app_config.cfg.srtp_secure_signaling);
	pj_strcat2(&cfg, line);
    }
#endif

    /* Media */
    if (config->null_audio)
	pj_strcat2(&cfg, "--null-audio\n");
    if (config->auto_play)
	pj_strcat2(&cfg, "--auto-play\n");
    if (config->auto_loop)
	pj_strcat2(&cfg, "--auto-loop\n");
    if (config->auto_conf)
	pj_strcat2(&cfg, "--auto-conf\n");
    for (i=0; i<config->wav_count; ++i) {
	pj_ansi_sprintf(line, "--play-file %s\n",
			config->wav_files[i].ptr);
	pj_strcat2(&cfg, line);
    }
    for (i=0; i<config->tone_count; ++i) {
	pj_ansi_sprintf(line, "--play-tone %d,%d,%d,%d\n",
			config->tones[i].freq1, config->tones[i].freq2,
			config->tones[i].on_msec, config->tones[i].off_msec);
	pj_strcat2(&cfg, line);
    }
    if (config->rec_file.slen) {
	pj_ansi_sprintf(line, "--rec-file %s\n",
			config->rec_file.ptr);
	pj_strcat2(&cfg, line);
    }
    if (config->auto_rec)
	pj_strcat2(&cfg, "--auto-rec\n");
    if (config->capture_dev != PJSUA_INVALID_ID) {
	pj_ansi_sprintf(line, "--capture-dev %d\n", config->capture_dev);
	pj_strcat2(&cfg, line);
    }
    if (config->playback_dev != PJSUA_INVALID_ID) {
	pj_ansi_sprintf(line, "--playback-dev %d\n", config->playback_dev);
	pj_strcat2(&cfg, line);
    }
    if (config->media_cfg.snd_auto_close_time != -1) {
	pj_ansi_sprintf(line, "--snd-auto-close %d\n",
			config->media_cfg.snd_auto_close_time);
	pj_strcat2(&cfg, line);
    }
    if (config->no_tones) {
	pj_strcat2(&cfg, "--no-tones\n");
    }
    if (config->media_cfg.jb_max != -1) {
	pj_ansi_sprintf(line, "--jb-max-size %d\n",
			config->media_cfg.jb_max);
	pj_strcat2(&cfg, line);
    }

    /* Sound device latency */
    if (config->capture_lat != PJMEDIA_SND_DEFAULT_REC_LATENCY) {
	pj_ansi_sprintf(line, "--capture-lat %d\n", config->capture_lat);
	pj_strcat2(&cfg, line);
    }
    if (config->playback_lat != PJMEDIA_SND_DEFAULT_PLAY_LATENCY) {
	pj_ansi_sprintf(line, "--playback-lat %d\n", config->playback_lat);
	pj_strcat2(&cfg, line);
    }

    /* Media clock rate. */
    if (config->media_cfg.clock_rate != PJSUA_DEFAULT_CLOCK_RATE) {
	pj_ansi_sprintf(line, "--clock-rate %d\n",
			config->media_cfg.clock_rate);
	pj_strcat2(&cfg, line);
    } else {
	pj_ansi_sprintf(line, "#using default --clock-rate %d\n",
			config->media_cfg.clock_rate);
	pj_strcat2(&cfg, line);
    }

    if (config->media_cfg.snd_clock_rate &&
	config->media_cfg.snd_clock_rate != config->media_cfg.clock_rate)
    {
	pj_ansi_sprintf(line, "--snd-clock-rate %d\n",
			config->media_cfg.snd_clock_rate);
	pj_strcat2(&cfg, line);
    }

    /* Stereo mode. */
    if (config->media_cfg.channel_count == 2) {
	pj_ansi_sprintf(line, "--stereo\n");
	pj_strcat2(&cfg, line);
    }

    /* quality */
    if (config->media_cfg.quality != PJSUA_DEFAULT_CODEC_QUALITY) {
	pj_ansi_sprintf(line, "--quality %d\n",
			config->media_cfg.quality);
	pj_strcat2(&cfg, line);
    } else {
	pj_ansi_sprintf(line, "#using default --quality %d\n",
			config->media_cfg.quality);
	pj_strcat2(&cfg, line);
    }

    if (config->vid.vcapture_dev != PJMEDIA_VID_DEFAULT_CAPTURE_DEV) {
	pj_ansi_sprintf(line, "--vcapture-dev %d\n", config->vid.vcapture_dev);
	pj_strcat2(&cfg, line);
    }
    if (config->vid.vrender_dev != PJMEDIA_VID_DEFAULT_RENDER_DEV) {
	pj_ansi_sprintf(line, "--vrender-dev %d\n", config->vid.vrender_dev);
	pj_strcat2(&cfg, line);
    }
    for (i=0; i<config->avi_cnt; ++i) {
	pj_ansi_sprintf(line, "--play-avi %s\n", config->avi[i].path.ptr);
	pj_strcat2(&cfg, line);
    }
    if (config->avi_auto_play) {
	pj_ansi_sprintf(line, "--auto-play-avi\n");
	pj_strcat2(&cfg, line);
    }

    /* ptime */
    if (config->media_cfg.ptime) {
	pj_ansi_sprintf(line, "--ptime %d\n",
			config->media_cfg.ptime);
	pj_strcat2(&cfg, line);
    }

    /* no-vad */
    if (config->media_cfg.no_vad) {
	pj_strcat2(&cfg, "--no-vad\n");
    }

    /* ec-tail */
    if (config->media_cfg.ec_tail_len != PJSUA_DEFAULT_EC_TAIL_LEN) {
	pj_ansi_sprintf(line, "--ec-tail %d\n",
			config->media_cfg.ec_tail_len);
	pj_strcat2(&cfg, line);
    } else {
	pj_ansi_sprintf(line, "#using default --ec-tail %d\n",
			config->media_cfg.ec_tail_len);
	pj_strcat2(&cfg, line);
    }

    /* ec-opt */
    if (config->media_cfg.ec_options != 0) {
	pj_ansi_sprintf(line, "--ec-opt %d\n",
			config->media_cfg.ec_options);
	pj_strcat2(&cfg, line);
    }

    /* ilbc-mode */
    if (config->media_cfg.ilbc_mode != PJSUA_DEFAULT_ILBC_MODE) {
	pj_ansi_sprintf(line, "--ilbc-mode %d\n",
			config->media_cfg.ilbc_mode);
	pj_strcat2(&cfg, line);
    } else {
	pj_ansi_sprintf(line, "#using default --ilbc-mode %d\n",
			config->media_cfg.ilbc_mode);
	pj_strcat2(&cfg, line);
    }

    /* RTP drop */
    if (config->media_cfg.tx_drop_pct) {
	pj_ansi_sprintf(line, "--tx-drop-pct %d\n",
			config->media_cfg.tx_drop_pct);
	pj_strcat2(&cfg, line);

    }
    if (config->media_cfg.rx_drop_pct) {
	pj_ansi_sprintf(line, "--rx-drop-pct %d\n",
			config->media_cfg.rx_drop_pct);
	pj_strcat2(&cfg, line);

    }

    /* Start RTP port. */
    pj_ansi_sprintf(line, "--rtp-port %d\n",
		    config->rtp_cfg.port);
    pj_strcat2(&cfg, line);

    /* Disable codec */
    for (i=0; i<config->codec_dis_cnt; ++i) {
	pj_ansi_sprintf(line, "--dis-codec %s\n",
		    config->codec_dis[i].ptr);
	pj_strcat2(&cfg, line);
    }
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

    /* accept-redirect */
    if (config->redir_op != PJSIP_REDIRECT_ACCEPT_REPLACE) {
	pj_ansi_sprintf(line, "--accept-redirect %d\n",
			config->redir_op);
	pj_strcat2(&cfg, line);
    }

    /* Max calls. */
    pj_ansi_sprintf(line, "--max-calls %d\n",
		    config->cfg.max_calls);
    pj_strcat2(&cfg, line);

    /* Uas-duration. */
    if (config->duration != PJSUA_APP_NO_LIMIT_DURATION) {
	pj_ansi_sprintf(line, "--duration %d\n",
			config->duration);
	pj_strcat2(&cfg, line);
    }

    /* norefersub ? */
    if (config->no_refersub) {
	pj_strcat2(&cfg, "--norefersub\n");
    }

    if (pjsip_use_compact_form)
    {
	pj_strcat2(&cfg, "--use-compact-form\n");
    }

    if (!config->cfg.force_lr) {
	pj_strcat2(&cfg, "--no-force-lr\n");
    }

    pj_strcat2(&cfg, "\n#\n# Buddies:\n#\n");

    /* Add buddies. */
    for (i=0; i<config->buddy_cnt; ++i) {
	pj_ansi_sprintf(line, "--add-buddy %.*s\n",
			      (int)config->buddy_cfg[i].uri.slen,
			      config->buddy_cfg[i].uri.ptr);
	pj_strcat2(&cfg, line);
    }

    /* SIP extensions. */
    pj_strcat2(&cfg, "\n#\n# SIP extensions:\n#\n");
    /* 100rel extension */
    if (config->cfg.require_100rel) {
	pj_strcat2(&cfg, "--use-100rel\n");
    }
    /* Session Timer extension */
    if (config->cfg.use_timer) {
	pj_ansi_sprintf(line, "--use-timer %d\n",
			      config->cfg.use_timer);
	pj_strcat2(&cfg, line);
    }
    if (config->cfg.timer_setting.min_se != 90) {
	pj_ansi_sprintf(line, "--timer-min-se %d\n",
			      config->cfg.timer_setting.min_se);
	pj_strcat2(&cfg, line);
    }
    if (config->cfg.timer_setting.sess_expires != PJSIP_SESS_TIMER_DEF_SE) {
	pj_ansi_sprintf(line, "--timer-se %d\n",
			      config->cfg.timer_setting.sess_expires);
	pj_strcat2(&cfg, line);
    }

    *(cfg.ptr + cfg.slen) = '\0';
    return cfg.slen;
}

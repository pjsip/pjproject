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


/**
 * \page page_pjmedia_samples_streamutil_c Samples: Remote Streaming
 *
 * This example mainly demonstrates how to stream media file to remote
 * peer using RTP.
 *
 * This file is pjsip-apps/src/samples/streamutil.c
 *
 * \includelineno streamutil.c
 */

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjmedia/transport_srtp.h>

#include <stdlib.h>     /* atoi() */
#include <stdio.h>

#include "util.h"

/* An experimental feature to add multicast option to this app for providing
 * the capability to send IP datagrams from a single source to more than
 * one receivers.
 */
#define HAVE_MULTICAST 1

static const char *desc = 
 " streamutil                                                           \n"
 "                                                                      \n"
 " PURPOSE:                                                             \n"
 "  Demonstrate how to use pjmedia stream component to transmit/receive \n"
 "  RTP packets to/from sound device.                                   \n"
 "\n"
 "\n"
 " USAGE:                                                               \n"
 "  streamutil [options]                                                \n"
 "\n"
 "\n"
 " Options:\n"
 "  --codec=CODEC         Set the codec name.                           \n"
 "  --local-port=PORT     Set local RTP port (default=4000)             \n"
 "  --remote=IP:PORT      Set the remote peer. If this option is set,   \n"
 "                        the program will transmit RTP audio to the    \n"
 "                        specified address. (default: recv only)       \n"
#if HAVE_MULTICAST
 "  --mcast-add=IP        Joins the multicast group as specified by     \n"
 "                        the address. Sample usage:                    \n"
 "                        Sender:                                       \n"
 "                        streamutil --remote=[multicast_addr]:[port]   \n"
 "                        Receivers:                                    \n"
 "                        streamutil --local-port=[port]                \n"
 "                                   --mcast-add=[multicast_addr]       \n"
#endif
 "  --play-file=WAV       Send audio from the WAV file instead of from  \n"
 "                        the sound device.                             \n"
 "  --record-file=WAV     Record incoming audio to WAV file instead of  \n"
 "                        playing it to sound device.                   \n"
 "  --send-recv           Set stream direction to bidirectional.        \n"
 "  --send-only           Set stream direction to send only             \n"
 "  --recv-only           Set stream direction to recv only (default)   \n"

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
 "  --use-srtp[=NAME]     Enable SRTP with crypto suite NAME            \n"
 "                        e.g: AES_CM_128_HMAC_SHA1_80 (default),       \n"
 "                             AES_CM_128_HMAC_SHA1_32                  \n"
 "                        Use this option along with the TX & RX keys,  \n"
 "                        formated of 60 hex digits (e.g: E148DA..)     \n"
 "  --srtp-tx-key         SRTP key for transmiting                      \n"
 "  --srtp-rx-key         SRTP key for receiving                        \n"
 "  --srtp-dtls-client    Use DTLS for SRTP keying, as DTLS client      \n"
 "  --srtp-dtls-server    Use DTLS for SRTP keying, as DTLS server      \n"
#endif

#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC != 0)
"  --opus-pt=PT           Opus payload type                             \n"
"  --opus-ch=CH           Opus channel count                            \n"
"  --opus-clock-rate=CR   Opus clock rate                               \n"
#endif

 "\n"
;




#define THIS_FILE       "streamutil.c"



/* Prototype */
static void print_stream_stat(pjmedia_stream *stream, 
                              const pjmedia_codec_param *codec_param);

/* Hexa string to octet array */
int my_hex_string_to_octet_string(char *raw, char *hex, int len)
{
    int i;
    for (i = 0; i < len; i+=2) {
        int tmp;
        if (i+1 >= len || !pj_isxdigit(hex[i]) || !pj_isxdigit(hex[i+1]))
            return i;
        tmp  = pj_hex_digit_to_val((unsigned char)hex[i]) << 4;
        tmp |= pj_hex_digit_to_val((unsigned char)hex[i+1]);
        raw[i/2] = (char)(tmp & 0xFF);
    }
    return len;
}

/* 
 * Register all codecs. 
 */
static pj_status_t init_codecs(pjmedia_endpt *med_endpt)
{
    return pjmedia_codec_register_audio_codecs(med_endpt, NULL);
}


/* 
 * Create stream based on the codec, dir, remote address, etc. 
 */
static pj_status_t create_stream( pj_pool_t *pool,
                                  pjmedia_endpt *med_endpt,
                                  const pjmedia_codec_info *codec_info,
                                  pjmedia_dir dir,
                                  pj_uint16_t local_port,
                                  const pj_sockaddr_in *rem_addr,
                                  pj_bool_t mcast,
                                  const pj_sockaddr_in *mcast_addr,
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
                                  pj_bool_t use_srtp,
                                  const pj_str_t *crypto_suite,
                                  const pj_str_t *srtp_tx_key,
                                  const pj_str_t *srtp_rx_key,
                                  pj_bool_t is_dtls_client,
                                  pj_bool_t is_dtls_server,
#endif
                                  pjmedia_stream **p_stream )
{
    pjmedia_stream_info info;
    pjmedia_transport *transport = NULL;
    pj_status_t status;
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    pjmedia_transport *srtp_tp = NULL;
#endif


    /* Reset stream info. */
    pj_bzero(&info, sizeof(info));


    /* Initialize stream info formats */
    info.type = PJMEDIA_TYPE_AUDIO;
    info.dir = dir;
    pj_memcpy(&info.fmt, codec_info, sizeof(pjmedia_codec_info));
    info.tx_pt = codec_info->pt;
    info.rx_pt = codec_info->pt;
    info.ssrc = pj_rand();

#if PJMEDIA_HAS_RTCP_XR && PJMEDIA_STREAM_ENABLE_XR
    /* Set default RTCP XR enabled/disabled */
    info.rtcp_xr_enabled = PJ_TRUE;
#endif

    /* Copy remote address */
    pj_memcpy(&info.rem_addr, rem_addr, sizeof(pj_sockaddr_in));

    /* If remote address is not set, set to an arbitrary address
     * (otherwise stream will assert).
     */
    if (info.rem_addr.addr.sa_family == 0) {
        const pj_str_t addr = pj_str("127.0.0.1");
        pj_sockaddr_in_init(&info.rem_addr.ipv4, &addr, 0);
    }

    pj_sockaddr_cp(&info.rem_rtcp, &info.rem_addr);
    pj_sockaddr_set_port(&info.rem_rtcp,
                         pj_sockaddr_get_port(&info.rem_rtcp)+1);

    if (mcast) {
        pjmedia_sock_info si;
        int reuse = 1;

        pj_bzero(&si, sizeof(pjmedia_sock_info));
        si.rtp_sock = si.rtcp_sock = PJ_INVALID_SOCKET;

        /* Create RTP socket */
        status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0,
                                &si.rtp_sock);
        if (status != PJ_SUCCESS)
            return status;

        status = pj_sock_setsockopt(si.rtp_sock, pj_SOL_SOCKET(),
                                    pj_SO_REUSEADDR(), &reuse, sizeof(reuse));
        if (status != PJ_SUCCESS)
            return status;

        /* Bind RTP socket */
        status = pj_sockaddr_init(pj_AF_INET(), &si.rtp_addr_name,
                                  NULL, local_port);
        if (status != PJ_SUCCESS)
            return status;
    
        status = pj_sock_bind(si.rtp_sock, &si.rtp_addr_name,
                              pj_sockaddr_get_len(&si.rtp_addr_name));
        if (status != PJ_SUCCESS)
            return status;

        /* Create RTCP socket */
        status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0,
                                &si.rtcp_sock);
        if (status != PJ_SUCCESS)
            return status;

        status = pj_sock_setsockopt(si.rtcp_sock, pj_SOL_SOCKET(),
                                    pj_SO_REUSEADDR(), &reuse, sizeof(reuse));
        if (status != PJ_SUCCESS)
            return status;

        /* Bind RTCP socket */
        status = pj_sockaddr_init(pj_AF_INET(), &si.rtcp_addr_name,
                                  NULL, local_port+1);
        if (status != PJ_SUCCESS)
            return status;
    
        status = pj_sock_bind(si.rtcp_sock, &si.rtcp_addr_name,
                              pj_sockaddr_get_len(&si.rtcp_addr_name));
        if (status != PJ_SUCCESS)
            return status;

#ifdef HAVE_MULTICAST
        {
            unsigned char loop;
            struct pj_ip_mreq imr;
        
            pj_memset(&imr, 0, sizeof(struct pj_ip_mreq));
            imr.imr_multiaddr.s_addr = mcast_addr->sin_addr.s_addr;
            imr.imr_interface.s_addr = pj_htonl(PJ_INADDR_ANY);
            status = pj_sock_setsockopt(si.rtp_sock, pj_SOL_IP(),
                                        pj_IP_ADD_MEMBERSHIP(),
                                        &imr, sizeof(struct pj_ip_mreq));
            if (status != PJ_SUCCESS)
                return status;

            status = pj_sock_setsockopt(si.rtcp_sock, pj_SOL_IP(),
                                        pj_IP_ADD_MEMBERSHIP(),
                                        &imr, sizeof(struct pj_ip_mreq));
            if (status != PJ_SUCCESS)
                return status;

            /* Disable local reception of local sent packets */
            loop = 0;
            pj_sock_setsockopt(si.rtp_sock, pj_SOL_IP(),
                               pj_IP_MULTICAST_LOOP(), &loop, sizeof(loop));
            pj_sock_setsockopt(si.rtcp_sock, pj_SOL_IP(),
                               pj_IP_MULTICAST_LOOP(), &loop, sizeof(loop));
        }
#endif
        
        /* Create media transport from existing sockets */
        status = pjmedia_transport_udp_attach( med_endpt, NULL, &si, 
                                PJMEDIA_UDP_NO_SRC_ADDR_CHECKING, &transport);
        if (status != PJ_SUCCESS)
            return status;      
        
    } else {
        /* Create media transport */
        status = pjmedia_transport_udp_create(med_endpt, NULL, local_port,
                                              0, &transport);
        if (status != PJ_SUCCESS)
            return status;
    }

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* Check if SRTP enabled */
    if (use_srtp) {
        status = pjmedia_transport_srtp_create(med_endpt, transport, 
                                               NULL, &srtp_tp);
        if (status != PJ_SUCCESS)
            return status;

        if (is_dtls_client || is_dtls_server) {
            char fp[128];
            pj_size_t fp_len = sizeof(fp);
            pjmedia_srtp_dtls_nego_param dtls_param;
            
            pjmedia_transport_srtp_dtls_get_fingerprint(srtp_tp, "SHA-256", fp, &fp_len);
            PJ_LOG(3, (THIS_FILE, "Local cert fingerprint: %s", fp));

            pj_bzero(&dtls_param, sizeof(dtls_param));
            pj_sockaddr_cp(&dtls_param.rem_addr, rem_addr);
            pj_sockaddr_cp(&dtls_param.rem_rtcp, rem_addr);
            dtls_param.is_role_active = is_dtls_client;

            status = pjmedia_transport_srtp_dtls_start_nego(srtp_tp, &dtls_param);
            if (status != PJ_SUCCESS)
                return status;
        } else {
            pjmedia_srtp_crypto tx_plc, rx_plc;

            pj_bzero(&tx_plc, sizeof(pjmedia_srtp_crypto));
            pj_bzero(&rx_plc, sizeof(pjmedia_srtp_crypto));

            tx_plc.key = *srtp_tx_key;
            tx_plc.name = *crypto_suite;
            rx_plc.key = *srtp_rx_key;
            rx_plc.name = *crypto_suite;
        
            status = pjmedia_transport_srtp_start(srtp_tp, &tx_plc, &rx_plc);
            if (status != PJ_SUCCESS)
                return status;
        }

        transport = srtp_tp;
    }
#endif

    /* Now that the stream info is initialized, we can create the 
     * stream.
     */

    status = pjmedia_stream_create( med_endpt, pool, &info, 
                                    transport, 
                                    NULL, p_stream);

    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Error creating stream", status);
        pjmedia_transport_close(transport);
        return status;
    }

    /* Start media transport */
    pjmedia_transport_media_start(transport, 0, 0, 0, 0);


    return PJ_SUCCESS;
}


/*
 * usage()
 */
static void usage()
{
    puts(desc);
}

/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_port *rec_file_port = NULL, *play_file_port = NULL;
    pjmedia_master_port *master_port = NULL;
    pjmedia_snd_port *snd_port = NULL;
    pjmedia_stream *stream = NULL;
    pjmedia_port *stream_port;
    char tmp[10];
    char addr[PJ_INET_ADDRSTRLEN];
    pj_status_t status; 

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* SRTP variables */
    pj_bool_t use_srtp = PJ_FALSE;
    char tmp_tx_key[64];
    char tmp_rx_key[64];
    pj_str_t  srtp_tx_key = {NULL, 0};
    pj_str_t  srtp_rx_key = {NULL, 0};
    pj_str_t  srtp_crypto_suite = {NULL, 0};
    pj_bool_t is_dtls_client = PJ_FALSE;
    pj_bool_t is_dtls_server = PJ_FALSE;
    int tmp_key_len;
#endif

#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC != 0)
    int opus_pt = -1;
    int opus_clock_rate = -1;
    int opus_ch = -1;
#endif

    /* Default values */
    const pjmedia_codec_info *codec_info;
    pjmedia_codec_param codec_param;
    pjmedia_dir dir = PJMEDIA_DIR_DECODING;
    pj_sockaddr_in remote_addr;
    pj_sockaddr_in mcast_addr;
    pj_uint16_t local_port = 4000;
    char *codec_id = NULL;
    char *rec_file = NULL;
    char *play_file = NULL;
    int mcast = 0;

    enum {
        OPT_CODEC       = 'c',
        OPT_LOCAL_PORT  = 'p',
        OPT_REMOTE      = 'r',
        OPT_MCAST       = 'm',
        OPT_PLAY_FILE   = 'w',
        OPT_RECORD_FILE = 'R',
        OPT_SEND_RECV   = 'b',
        OPT_SEND_ONLY   = 's',
        OPT_RECV_ONLY   = 'i',
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
        OPT_USE_SRTP    = 'S',
#endif
#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC != 0)
        OPT_OPUS_PT = 'P',
        OPT_OPUS_CH = 'C',
        OPT_OPUS_CLOCK_RATE = 'K',
#endif
        OPT_SRTP_TX_KEY = 'x',
        OPT_SRTP_RX_KEY = 'y',
        OPT_SRTP_DTLS_CLIENT = 'd',
        OPT_SRTP_DTLS_SERVER = 'D',
        OPT_HELP        = 'h',
    };

    struct pj_getopt_option long_options[] = {
        { "codec",          1, 0, OPT_CODEC },
        { "local-port",     1, 0, OPT_LOCAL_PORT },
        { "remote",         1, 0, OPT_REMOTE },
        { "mcast-add",      1, 0, OPT_MCAST },
        { "play-file",      1, 0, OPT_PLAY_FILE },
        { "record-file",    1, 0, OPT_RECORD_FILE },
        { "send-recv",      0, 0, OPT_SEND_RECV },
        { "send-only",      0, 0, OPT_SEND_ONLY },
        { "recv-only",      0, 0, OPT_RECV_ONLY },
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
        { "use-srtp",       2, 0, OPT_USE_SRTP },
        { "srtp-tx-key",    1, 0, OPT_SRTP_TX_KEY },
        { "srtp-rx-key",    1, 0, OPT_SRTP_RX_KEY },
        { "srtp-dtls-client", 0, 0, OPT_SRTP_DTLS_CLIENT },
        { "srtp-dtls-server", 0, 0, OPT_SRTP_DTLS_SERVER },
#endif
#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC != 0)
        { "opus-pt", 1, 0, OPT_OPUS_PT },
        { "opus-ch", 1, 0, OPT_OPUS_CH },
        { "opus-clock-rate", 1, 0, OPT_OPUS_CLOCK_RATE },
#endif
        { "help",           0, 0, OPT_HELP },
        { NULL, 0, 0, 0 },
    };

    int c;
    int option_index;


    pj_bzero(&remote_addr, sizeof(remote_addr));


    /* init PJLIB : */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Parse arguments */
    pj_optind = 0;
    while((c=pj_getopt_long(argc,argv, "h", long_options, &option_index))!=-1) {

        switch (c) {
        case OPT_CODEC:
            codec_id = pj_optarg;
            break;

        case OPT_LOCAL_PORT:
            local_port = (pj_uint16_t) atoi(pj_optarg);
            if (local_port < 1) {
                printf("Error: invalid local port %s\n", pj_optarg);
                return 1;
            }
            break;

        case OPT_REMOTE:
            {
                pj_str_t ip = pj_str(strtok(pj_optarg, ":"));
                pj_uint16_t port = (pj_uint16_t) atoi(strtok(NULL, ":"));

                status = pj_sockaddr_in_init(&remote_addr, &ip, port);
                if (status != PJ_SUCCESS) {
                    app_perror(THIS_FILE, "Invalid remote address", status);
                    return 1;
                }
            }
            break;

        case OPT_MCAST:
            {
                pj_str_t ip = pj_str(pj_optarg);

                mcast = 1;
                status = pj_sockaddr_in_init(&mcast_addr, &ip, 0);
                if (status != PJ_SUCCESS) {
                    app_perror(THIS_FILE, "Invalid mcast address", status);
                    return 1;
                }
            }
            break;

        case OPT_PLAY_FILE:
            play_file = pj_optarg;
            break;

        case OPT_RECORD_FILE:
            rec_file = pj_optarg;
            break;

        case OPT_SEND_RECV:
            dir = PJMEDIA_DIR_ENCODING_DECODING;
            break;

        case OPT_SEND_ONLY:
            dir = PJMEDIA_DIR_ENCODING;
            break;

        case OPT_RECV_ONLY:
            dir = PJMEDIA_DIR_DECODING;
            break;

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
        case OPT_USE_SRTP:
            use_srtp = PJ_TRUE;
            if (pj_optarg) {
                pj_strset(&srtp_crypto_suite, pj_optarg, strlen(pj_optarg));
            } else {
                srtp_crypto_suite = pj_str("AES_CM_128_HMAC_SHA1_80");
            }
            break;

        case OPT_SRTP_TX_KEY:
            tmp_key_len = my_hex_string_to_octet_string(tmp_tx_key, pj_optarg, 
                                                        (int)strlen(pj_optarg));
            pj_strset(&srtp_tx_key, tmp_tx_key, tmp_key_len/2);
            break;

        case OPT_SRTP_RX_KEY:
            tmp_key_len = my_hex_string_to_octet_string(tmp_rx_key, pj_optarg, 
                                                        (int)strlen(pj_optarg));
            pj_strset(&srtp_rx_key, tmp_rx_key, tmp_key_len/2);
            break;
        case OPT_SRTP_DTLS_CLIENT:
            is_dtls_client = PJ_TRUE;
            if (is_dtls_server) {
                printf("Error: Cannot be as both DTLS server & client\n");
                return 1;
            }
            break;
        case OPT_SRTP_DTLS_SERVER:
            is_dtls_server = PJ_TRUE;
            if (is_dtls_client) {
                printf("Error: Cannot be as both DTLS server & client\n");
                return 1;
            }
            break;
#endif

#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC != 0)
        case OPT_OPUS_PT:
            opus_pt = atoi(pj_optarg);
            break;
        case OPT_OPUS_CLOCK_RATE:
            opus_clock_rate = atoi(pj_optarg);
            break;
        case OPT_OPUS_CH:
            opus_ch = atoi(pj_optarg);
            break;
#endif

        case OPT_HELP:
            usage();
            return 1;

        default:
            printf("Invalid options %s\n", argv[pj_optind]);
            return 1;
        }

    }


    /* Verify arguments. */
    if (dir & PJMEDIA_DIR_ENCODING
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
        || is_dtls_client || is_dtls_server
#endif
       ) {
        if (remote_addr.sin_addr.s_addr == 0) {
            printf("Error: remote address must be set\n");
            return 1;
        }
    }

    if (play_file != NULL && dir != PJMEDIA_DIR_ENCODING) {
        printf("Direction is set to --send-only because of --play-file\n");
        dir = PJMEDIA_DIR_ENCODING;
    }

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* SRTP validation */
    if (use_srtp) {
        if (!is_dtls_client && !is_dtls_server && 
            (!srtp_tx_key.slen || !srtp_rx_key.slen))
        {
            printf("Error: Key for each SRTP stream direction must be set\n");
            return 1;
        }
    }
#endif

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for application purpose */
    pool = pj_pool_create( &cp.factory,     /* pool factory         */
                           "app",           /* pool name.           */
                           4000,            /* init size            */
                           4000,            /* increment size       */
                           NULL             /* callback on error    */
                           );


    /* Register all supported codecs */
    status = init_codecs(med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Find which codec to use. */
    if (codec_id) {
        unsigned count = 1;
        pj_str_t str_codec_id = pj_str(codec_id);
        pjmedia_codec_mgr *codec_mgr = pjmedia_endpt_get_codec_mgr(med_endpt);
        status = pjmedia_codec_mgr_find_codecs_by_id( codec_mgr,
                                                      &str_codec_id, &count,
                                                      &codec_info, NULL);
        if (status != PJ_SUCCESS) {
            printf("Error: unable to find codec %s\n", codec_id);
            return 1;
        }
    } else {
        /* Default to pcmu */
        pjmedia_codec_mgr_get_codec_info( pjmedia_endpt_get_codec_mgr(med_endpt),
                                          0, &codec_info);
    }

    /* Create event manager */
    status = pjmedia_event_mgr_create(pool, 0, NULL);
    if (status != PJ_SUCCESS)
        goto on_exit;

#if defined(PJMEDIA_HAS_OPUS_CODEC) && (PJMEDIA_HAS_OPUS_CODEC != 0)
    /* Opus custom settings */
    if (!pj_stricmp2(&codec_info->encoding_name, "opus")) {
        /* Alloc temp codec info to update settings */
        pjmedia_codec_info *tmp = PJ_POOL_ALLOC_T(pool, pjmedia_codec_info);
        pj_memcpy(tmp, codec_info, sizeof(pjmedia_codec_info));

        if (opus_pt > 0)
            tmp->pt = opus_pt;
        if (opus_clock_rate > 0)
            tmp->clock_rate = opus_clock_rate;
        if (opus_ch > 0)
            tmp->channel_cnt = opus_ch;

        /* Use the temp codec info  */
        codec_info = tmp;
    }
#endif

    /* Create stream based on program arguments */
    status = create_stream(pool, med_endpt, codec_info, dir, local_port,
                           &remote_addr, mcast, &mcast_addr,
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
                           use_srtp, &srtp_crypto_suite,
                           &srtp_tx_key, &srtp_rx_key,
                           is_dtls_client, is_dtls_server,
#endif
                           &stream);
    if (status != PJ_SUCCESS)
        goto on_exit;

    /* Get codec default param for info */
    status = pjmedia_codec_mgr_get_default_param(
                                    pjmedia_endpt_get_codec_mgr(med_endpt),
                                    codec_info,
                                    &codec_param);
    /* Should be ok, as create_stream() above succeeded */
    pj_assert(status == PJ_SUCCESS);

    /* Get the port interface of the stream */
    status = pjmedia_stream_get_port( stream, &stream_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    if (play_file) {
        unsigned wav_ptime;

        wav_ptime = PJMEDIA_PIA_PTIME(&stream_port->info);
        status = pjmedia_wav_player_port_create(pool, play_file, wav_ptime,
                                                0, -1, &play_file_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Unable to use file", status);
            goto on_exit;
        }

        status = pjmedia_master_port_create(pool, play_file_port, stream_port,
                                            0, &master_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Unable to create master port", status);
            goto on_exit;
        }

        status = pjmedia_master_port_start(master_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Error starting master port", status);
            goto on_exit;
        }

        printf("Playing from WAV file %s..\n", play_file);

    } else if (rec_file) {

        status = pjmedia_wav_writer_port_create(pool, rec_file,
                                                PJMEDIA_PIA_SRATE(&stream_port->info),
                                                PJMEDIA_PIA_CCNT(&stream_port->info),
                                                PJMEDIA_PIA_SPF(&stream_port->info),
                                                PJMEDIA_PIA_BITS(&stream_port->info),
                                                0, 0, &rec_file_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Unable to use file", status);
            goto on_exit;
        }

        status = pjmedia_master_port_create(pool, stream_port, rec_file_port, 
                                            0, &master_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Unable to create master port", status);
            goto on_exit;
        }

        status = pjmedia_master_port_start(master_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Error starting master port", status);
            goto on_exit;
        }

        printf("Recording to WAV file %s..\n", rec_file);
        
    } else {

        /* Create sound device port. */
        if (dir == PJMEDIA_DIR_ENCODING_DECODING)
            status = pjmedia_snd_port_create(pool, -1, -1, 
                                        PJMEDIA_PIA_SRATE(&stream_port->info),
                                        PJMEDIA_PIA_CCNT(&stream_port->info),
                                        PJMEDIA_PIA_SPF(&stream_port->info),
                                        PJMEDIA_PIA_BITS(&stream_port->info),
                                        0, &snd_port);
        else if (dir == PJMEDIA_DIR_ENCODING)
            status = pjmedia_snd_port_create_rec(pool, -1, 
                                        PJMEDIA_PIA_SRATE(&stream_port->info),
                                        PJMEDIA_PIA_CCNT(&stream_port->info),
                                        PJMEDIA_PIA_SPF(&stream_port->info),
                                        PJMEDIA_PIA_BITS(&stream_port->info),
                                        0, &snd_port);
        else
            status = pjmedia_snd_port_create_player(pool, -1, 
                                        PJMEDIA_PIA_SRATE(&stream_port->info),
                                        PJMEDIA_PIA_CCNT(&stream_port->info),
                                        PJMEDIA_PIA_SPF(&stream_port->info),
                                        PJMEDIA_PIA_BITS(&stream_port->info),
                                        0, &snd_port);


        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Unable to create sound port", status);
            goto on_exit;
        }

        /* Connect sound port to stream */
        status = pjmedia_snd_port_connect( snd_port, stream_port );
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    }

    /* Start streaming */
    pjmedia_stream_start(stream);


    /* Done */

    if (dir == PJMEDIA_DIR_DECODING)
        printf("Stream is active, dir is recv-only, local port is %d\n",
               local_port);
    else if (dir == PJMEDIA_DIR_ENCODING)
        printf("Stream is active, dir is send-only, sending to %s:%d\n",
               pj_inet_ntop2(pj_AF_INET(), &remote_addr.sin_addr, addr,
                             sizeof(addr)),
               pj_ntohs(remote_addr.sin_port));
    else
        printf("Stream is active, send/recv, local port is %d, "
               "sending to %s:%d\n",
               local_port,
               pj_inet_ntop2(pj_AF_INET(), &remote_addr.sin_addr, addr,
                             sizeof(addr)),
               pj_ntohs(remote_addr.sin_port));


    for (;;) {

        puts("");
        puts("Commands:");
        puts("  s     Display media statistics");
        puts("  q     Quit");
        puts("");

        printf("Command: "); fflush(stdout);

        if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
            puts("EOF while reading stdin, will quit now..");
            break;
        }

        if (tmp[0] == 's')
            print_stream_stat(stream, &codec_param);
        else if (tmp[0] == 'q')
            break;

    }

    /* Start deinitialization: */
on_exit:

    /* Destroy sound device */
    if (snd_port) {
        pjmedia_snd_port_destroy( snd_port );
        PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Destroy master port */
    if (master_port) {
        pjmedia_master_port_destroy(master_port, PJ_FALSE);
    }

    /* Destroy stream */
    if (stream) {
        pjmedia_transport *tp;

        tp = pjmedia_stream_get_transport(stream);
        pjmedia_stream_destroy(stream);
        
        pjmedia_transport_media_stop(tp);
        pjmedia_transport_close(tp);
    }

    /* Destroy file ports */
    if (play_file_port)
        pjmedia_port_destroy( play_file_port );
    if (rec_file_port)
        pjmedia_port_destroy( rec_file_port );

    /* Destroy event manager */
    pjmedia_event_mgr_destroy(NULL);

    /* Release application pool */
    pj_pool_release( pool );

    /* Destroy media endpoint. */
    pjmedia_endpt_destroy( med_endpt );

    /* Destroy pool factory */
    pj_caching_pool_destroy( &cp );

    /* Shutdown PJLIB */
    pj_shutdown();


    return (status == PJ_SUCCESS) ? 0 : 1;
}




static const char *good_number(char *buf, unsigned buf_size, pj_int32_t val)
{
    if (val < 1000) {
        pj_ansi_snprintf(buf, buf_size, "%d", val);
    } else if (val < 1000000) {
        pj_ansi_snprintf(buf, buf_size, "%d.%dK", 
                        val / 1000,
                        (val % 1000) / 100);
    } else {
        pj_ansi_snprintf(buf, buf_size, "%d.%02dM", 
                        val / 1000000,
                        (val % 1000000) / 10000);
    }

    return buf;
}


#define SAMPLES_TO_USEC(usec, samples, clock_rate) \
    do { \
        if (samples <= 4294) \
            usec = samples * 1000000 / clock_rate; \
        else { \
            usec = samples * 1000 / clock_rate; \
            usec *= 1000; \
        } \
    } while(0)

#define PRINT_VOIP_MTC_VAL(s, v) \
    if (v == 127) \
        snprintf(s, sizeof(s), "(na)"); \
    else \
        snprintf(s, sizeof(s), "%d", v)


/*
 * Print stream statistics
 */
static void print_stream_stat(pjmedia_stream *stream,
                              const pjmedia_codec_param *codec_param)
{
    char duration[80], last_update[80];
    char bps[16], ipbps[16], packets[16], bytes[16], ipbytes[16];
    pjmedia_port *port;
    pjmedia_rtcp_stat stat;
    pj_time_val now;

#define SNPRINTF2(s,val)     pj_ansi_strxcpy(s, val, sizeof(s))
#define SNPRINTF3(s,fmt,val) snprintf(s, sizeof(s), fmt, val)

    pj_gettimeofday(&now);
    pjmedia_stream_get_stat(stream, &stat);
    pjmedia_stream_get_port(stream, &port);

    puts("Stream statistics:");

    /* Print duration */
    PJ_TIME_VAL_SUB(now, stat.start);
    snprintf(duration, sizeof(duration),
            " Duration: %02ld:%02ld:%02ld.%03ld",
            now.sec / 3600,
            (now.sec % 3600) / 60,
            (now.sec % 60),
            now.msec);


    printf(" Info: audio %dHz, %dms/frame, %sB/s (%sB/s +IP hdr)\n",
        PJMEDIA_PIA_SRATE(&port->info),
        PJMEDIA_PIA_PTIME(&port->info),
        good_number(bps, sizeof(bps), (codec_param->info.avg_bps+7)/8),
        good_number(ipbps, sizeof(ipbps), ((codec_param->info.avg_bps+7)/8) + 
                           (40 * 1000 /
                            codec_param->setting.frm_per_pkt /
                            codec_param->info.frm_ptime)));

    if (stat.rx.update_cnt == 0)
        pj_ansi_strxcpy(last_update, "never", sizeof(last_update));
    else {
        pj_gettimeofday(&now);
        PJ_TIME_VAL_SUB(now, stat.rx.update);
        pj_ansi_snprintf(last_update, sizeof(last_update),
                "%02ldh:%02ldm:%02ld.%03lds ago",
                now.sec / 3600,
                (now.sec % 3600) / 60,
                now.sec % 60,
                now.msec);
    }

    printf(" RX stat last update: %s\n"
           "    total %s packets %sB received (%sB +IP hdr)%s\n"
           "    pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
           "          (msec)    min     avg     max     last    dev\n"
           "    loss period: %7.3f %7.3f %7.3f %7.3f %7.3f%s\n"
           "    jitter     : %7.3f %7.3f %7.3f %7.3f %7.3f%s\n",
           last_update,
           good_number(packets, sizeof(packets), stat.rx.pkt),
           good_number(bytes, sizeof(bytes), stat.rx.bytes),
           good_number(ipbytes, sizeof(ipbytes), stat.rx.bytes + stat.rx.pkt * 32),
           "",
           stat.rx.loss,
           stat.rx.loss * 100.0 / (stat.rx.pkt + stat.rx.loss),
           stat.rx.dup, 
           stat.rx.dup * 100.0 / (stat.rx.pkt + stat.rx.loss),
           stat.rx.reorder, 
           stat.rx.reorder * 100.0 / (stat.rx.pkt + stat.rx.loss),
           "",
           stat.rx.loss_period.min / 1000.0, 
           stat.rx.loss_period.mean / 1000.0, 
           stat.rx.loss_period.max / 1000.0,
           stat.rx.loss_period.last / 1000.0,
           pj_math_stat_get_stddev(&stat.rx.loss_period) / 1000.0,
           "",
           stat.rx.jitter.min / 1000.0,
           stat.rx.jitter.mean / 1000.0,
           stat.rx.jitter.max / 1000.0,
           stat.rx.jitter.last / 1000.0,
           pj_math_stat_get_stddev(&stat.rx.jitter) / 1000.0,
           ""
           );


    if (stat.tx.update_cnt == 0)
        pj_ansi_strxcpy(last_update, "never", sizeof(last_update));
    else {
        pj_gettimeofday(&now);
        PJ_TIME_VAL_SUB(now, stat.tx.update);
        pj_ansi_snprintf(last_update, sizeof(last_update),
                "%02ldh:%02ldm:%02ld.%03lds ago",
                now.sec / 3600,
                (now.sec % 3600) / 60,
                now.sec % 60,
                now.msec);
    }

    printf(" TX stat last update: %s\n"
           "    total %s packets %sB sent (%sB +IP hdr)%s\n"
           "    pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
           "          (msec)    min     avg     max     last    dev\n"
           "    loss period: %7.3f %7.3f %7.3f %7.3f %7.3f%s\n"
           "    jitter     : %7.3f %7.3f %7.3f %7.3f %7.3f%s\n",
           last_update,
           good_number(packets, sizeof(packets), stat.tx.pkt),
           good_number(bytes, sizeof(bytes), stat.tx.bytes),
           good_number(ipbytes, sizeof(ipbytes), stat.tx.bytes + stat.tx.pkt * 32),
           "",
           stat.tx.loss,
           stat.tx.loss * 100.0 / (stat.tx.pkt + stat.tx.loss),
           stat.tx.dup, 
           stat.tx.dup * 100.0 / (stat.tx.pkt + stat.tx.loss),
           stat.tx.reorder, 
           stat.tx.reorder * 100.0 / (stat.tx.pkt + stat.tx.loss),
           "",
           stat.tx.loss_period.min / 1000.0, 
           stat.tx.loss_period.mean / 1000.0, 
           stat.tx.loss_period.max / 1000.0,
           stat.tx.loss_period.last / 1000.0,
           pj_math_stat_get_stddev(&stat.tx.loss_period) / 1000.0,
           "",
           stat.tx.jitter.min / 1000.0,
           stat.tx.jitter.mean / 1000.0,
           stat.tx.jitter.max / 1000.0,
           stat.tx.jitter.last / 1000.0,
           pj_math_stat_get_stddev(&stat.tx.jitter) / 1000.0,
           ""
           );


    printf(" RTT delay     : %7.3f %7.3f %7.3f %7.3f %7.3f%s\n", 
           stat.rtt.min / 1000.0,
           stat.rtt.mean / 1000.0,
           stat.rtt.max / 1000.0,
           stat.rtt.last / 1000.0,
           pj_math_stat_get_stddev(&stat.rtt) / 1000.0,
           ""
           );

#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
    /* RTCP XR Reports */
    do {
        char loss[16], dup[16];
        char jitter[80];
        char toh[80];
        char plc[16], jba[16], jbr[16];
        char signal_lvl[16], noise_lvl[16], rerl[16];
        char r_factor[16], ext_r_factor[16], mos_lq[16], mos_cq[16];
        pjmedia_rtcp_xr_stat xr_stat;

        if (pjmedia_stream_get_stat_xr(stream, &xr_stat) != PJ_SUCCESS)
            break;

        puts("\nExtended reports:");

        /* Statistics Summary */
        puts(" Statistics Summary");

        if (xr_stat.rx.stat_sum.l)
            SNPRINTF3(loss, "%d", xr_stat.rx.stat_sum.lost);
        else
            SNPRINTF2(loss, "(na)");

        if (xr_stat.rx.stat_sum.d)
            SNPRINTF3(dup, "%d", xr_stat.rx.stat_sum.dup);
        else
            SNPRINTF2(dup, "(na)");

        if (xr_stat.rx.stat_sum.j) {
            unsigned jmin, jmax, jmean, jdev;

            SAMPLES_TO_USEC(jmin, xr_stat.rx.stat_sum.jitter.min, 
                            port->info.fmt.det.aud.clock_rate);
            SAMPLES_TO_USEC(jmax, xr_stat.rx.stat_sum.jitter.max, 
                            port->info.fmt.det.aud.clock_rate);
            SAMPLES_TO_USEC(jmean, xr_stat.rx.stat_sum.jitter.mean, 
                            port->info.fmt.det.aud.clock_rate);
            SAMPLES_TO_USEC(jdev, 
                           pj_math_stat_get_stddev(&xr_stat.rx.stat_sum.jitter),
                           port->info.fmt.det.aud.clock_rate);
            snprintf(jitter, sizeof(jitter), "%7.3f %7.3f %7.3f %7.3f",
                    jmin/1000.0, jmean/1000.0, jmax/1000.0, jdev/1000.0);
        } else
            SNPRINTF2(jitter, "(report not available)");

        if (xr_stat.rx.stat_sum.t) {
            snprintf(toh, sizeof(toh), "%11d %11d %11d %11d",
                    xr_stat.rx.stat_sum.toh.min,
                    xr_stat.rx.stat_sum.toh.mean,
                    xr_stat.rx.stat_sum.toh.max,
                    pj_math_stat_get_stddev(&xr_stat.rx.stat_sum.toh));
        } else
            SNPRINTF2(toh, "(report not available)");

        if (xr_stat.rx.stat_sum.update.sec == 0)
            pj_ansi_strxcpy(last_update, "never", sizeof(last_update));
        else {
            pj_gettimeofday(&now);
            PJ_TIME_VAL_SUB(now, xr_stat.rx.stat_sum.update);
            pj_ansi_snprintf(last_update, sizeof(last_update),
                    "%02ldh:%02ldm:%02ld.%03lds ago",
                    now.sec / 3600,
                    (now.sec % 3600) / 60,
                    now.sec % 60,
                    now.msec);
        }

        printf(" RX last update: %s\n"
               "    begin seq=%d, end seq=%d%s\n"
               "    pkt loss=%s, dup=%s%s\n"
               "          (msec)    min     avg     max     dev\n"
               "    jitter     : %s\n"
               "    toh        : %s\n",
               last_update,
               xr_stat.rx.stat_sum.begin_seq, xr_stat.rx.stat_sum.end_seq,
               "",
               loss, dup,
               "",
               jitter,
               toh
               );

        if (xr_stat.tx.stat_sum.l)
            SNPRINTF3(loss, "%d", xr_stat.tx.stat_sum.lost);
        else
            SNPRINTF2(loss, "(na)");

        if (xr_stat.tx.stat_sum.d)
            SNPRINTF3(dup, "%d", xr_stat.tx.stat_sum.dup);
        else
            SNPRINTF2(dup, "(na)");

        if (xr_stat.tx.stat_sum.j) {
            unsigned jmin, jmax, jmean, jdev;

            SAMPLES_TO_USEC(jmin, xr_stat.tx.stat_sum.jitter.min, 
                            port->info.fmt.det.aud.clock_rate);
            SAMPLES_TO_USEC(jmax, xr_stat.tx.stat_sum.jitter.max, 
                            port->info.fmt.det.aud.clock_rate);
            SAMPLES_TO_USEC(jmean, xr_stat.tx.stat_sum.jitter.mean, 
                            port->info.fmt.det.aud.clock_rate);
            SAMPLES_TO_USEC(jdev, 
                           pj_math_stat_get_stddev(&xr_stat.tx.stat_sum.jitter),
                           port->info.fmt.det.aud.clock_rate);
            snprintf(jitter, sizeof(jitter), "%7.3f %7.3f %7.3f %7.3f",
                    jmin/1000.0, jmean/1000.0, jmax/1000.0, jdev/1000.0);
        } else
            SNPRINTF2(jitter, "(report not available)");

        if (xr_stat.tx.stat_sum.t) {
            snprintf(toh, sizeof(toh), "%11d %11d %11d %11d",
                    xr_stat.tx.stat_sum.toh.min,
                    xr_stat.tx.stat_sum.toh.mean,
                    xr_stat.tx.stat_sum.toh.max,
                    pj_math_stat_get_stddev(&xr_stat.rx.stat_sum.toh));
        } else
            SNPRINTF2(toh,    "(report not available)");

        if (xr_stat.tx.stat_sum.update.sec == 0)
            pj_ansi_strxcpy(last_update, "never", sizeof(last_update));
        else {
            pj_gettimeofday(&now);
            PJ_TIME_VAL_SUB(now, xr_stat.tx.stat_sum.update);
            pj_ansi_snprintf(last_update, sizeof(last_update),
                    "%02ldh:%02ldm:%02ld.%03lds ago",
                    now.sec / 3600,
                    (now.sec % 3600) / 60,
                    now.sec % 60,
                    now.msec);
        }

        printf(" TX last update: %s\n"
               "    begin seq=%d, end seq=%d%s\n"
               "    pkt loss=%s, dup=%s%s\n"
               "          (msec)    min     avg     max     dev\n"
               "    jitter     : %s\n"
               "    toh        : %s\n",
               last_update,
               xr_stat.tx.stat_sum.begin_seq, xr_stat.tx.stat_sum.end_seq,
               "",
               loss, dup,
               "",
               jitter,
               toh
               );

        /* VoIP Metrics */
        puts(" VoIP Metrics");

        PRINT_VOIP_MTC_VAL(signal_lvl, xr_stat.rx.voip_mtc.signal_lvl);
        PRINT_VOIP_MTC_VAL(noise_lvl, xr_stat.rx.voip_mtc.noise_lvl);
        PRINT_VOIP_MTC_VAL(rerl, xr_stat.rx.voip_mtc.rerl);
        PRINT_VOIP_MTC_VAL(r_factor, xr_stat.rx.voip_mtc.r_factor);
        PRINT_VOIP_MTC_VAL(ext_r_factor, xr_stat.rx.voip_mtc.ext_r_factor);
        PRINT_VOIP_MTC_VAL(mos_lq, xr_stat.rx.voip_mtc.mos_lq);
        PRINT_VOIP_MTC_VAL(mos_cq, xr_stat.rx.voip_mtc.mos_cq);

        switch ((xr_stat.rx.voip_mtc.rx_config>>6) & 3) {
            case PJMEDIA_RTCP_XR_PLC_DIS:
                SNPRINTF2(plc, "DISABLED");
                break;
            case PJMEDIA_RTCP_XR_PLC_ENH:
                SNPRINTF2(plc, "ENHANCED");
                break;
            case PJMEDIA_RTCP_XR_PLC_STD:
                SNPRINTF2(plc, "STANDARD");
                break;
            case PJMEDIA_RTCP_XR_PLC_UNK:
            default:
                SNPRINTF2(plc, "UNKNOWN");
                break;
        }

        switch ((xr_stat.rx.voip_mtc.rx_config>>4) & 3) {
            case PJMEDIA_RTCP_XR_JB_FIXED:
                SNPRINTF2(jba, "FIXED");
                break;
            case PJMEDIA_RTCP_XR_JB_ADAPTIVE:
                SNPRINTF2(jba, "ADAPTIVE");
                break;
            default:
                SNPRINTF2(jba, "UNKNOWN");
                break;
        }

        SNPRINTF3(jbr, "%d", xr_stat.rx.voip_mtc.rx_config & 0x0F);

        if (xr_stat.rx.voip_mtc.update.sec == 0)
            pj_ansi_strxcpy(last_update, "never", sizeof(last_update));
        else {
            pj_gettimeofday(&now);
            PJ_TIME_VAL_SUB(now, xr_stat.rx.voip_mtc.update);
            pj_ansi_snprintf(last_update, sizeof(last_update),
                    "%02ldh:%02ldm:%02ld.%03lds ago",
                    now.sec / 3600,
                    (now.sec % 3600) / 60,
                    now.sec % 60,
                    now.msec);
        }

        printf(" RX last update: %s\n"
               "    packets    : loss rate=%d (%.2f%%), discard rate=%d (%.2f%%)\n"
               "    burst      : density=%d (%.2f%%), duration=%d%s\n"
               "    gap        : density=%d (%.2f%%), duration=%d%s\n"
               "    delay      : round trip=%d%s, end system=%d%s\n"
               "    level      : signal=%s%s, noise=%s%s, RERL=%s%s\n"
               "    quality    : R factor=%s, ext R factor=%s\n"
               "                 MOS LQ=%s, MOS CQ=%s\n"
               "    config     : PLC=%s, JB=%s, JB rate=%s, Gmin=%d\n"
               "    JB delay   : cur=%d%s, max=%d%s, abs max=%d%s\n",
               last_update,
               /* pakcets */
               xr_stat.rx.voip_mtc.loss_rate, xr_stat.rx.voip_mtc.loss_rate*100.0/256,
               xr_stat.rx.voip_mtc.discard_rate, xr_stat.rx.voip_mtc.discard_rate*100.0/256,
               /* burst */
               xr_stat.rx.voip_mtc.burst_den, xr_stat.rx.voip_mtc.burst_den*100.0/256,
               xr_stat.rx.voip_mtc.burst_dur, "ms",
               /* gap */
               xr_stat.rx.voip_mtc.gap_den, xr_stat.rx.voip_mtc.gap_den*100.0/256,
               xr_stat.rx.voip_mtc.gap_dur, "ms",
               /* delay */
               xr_stat.rx.voip_mtc.rnd_trip_delay, "ms",
               xr_stat.rx.voip_mtc.end_sys_delay, "ms",
               /* level */
               signal_lvl, "dB",
               noise_lvl, "dB",
               rerl, "",
               /* quality */
               r_factor, ext_r_factor, mos_lq, mos_cq,
               /* config */
               plc, jba, jbr, xr_stat.rx.voip_mtc.gmin,
               /* JB delay */
               xr_stat.rx.voip_mtc.jb_nom, "ms",
               xr_stat.rx.voip_mtc.jb_max, "ms",
               xr_stat.rx.voip_mtc.jb_abs_max, "ms"
               );

        PRINT_VOIP_MTC_VAL(signal_lvl, xr_stat.tx.voip_mtc.signal_lvl);
        PRINT_VOIP_MTC_VAL(noise_lvl, xr_stat.tx.voip_mtc.noise_lvl);
        PRINT_VOIP_MTC_VAL(rerl, xr_stat.tx.voip_mtc.rerl);
        PRINT_VOIP_MTC_VAL(r_factor, xr_stat.tx.voip_mtc.r_factor);
        PRINT_VOIP_MTC_VAL(ext_r_factor, xr_stat.tx.voip_mtc.ext_r_factor);
        PRINT_VOIP_MTC_VAL(mos_lq, xr_stat.tx.voip_mtc.mos_lq);
        PRINT_VOIP_MTC_VAL(mos_cq, xr_stat.tx.voip_mtc.mos_cq);

        switch ((xr_stat.tx.voip_mtc.rx_config>>6) & 3) {
            case PJMEDIA_RTCP_XR_PLC_DIS:
                SNPRINTF2(plc, "DISABLED");
                break;
            case PJMEDIA_RTCP_XR_PLC_ENH:
                SNPRINTF2(plc, "ENHANCED");
                break;
            case PJMEDIA_RTCP_XR_PLC_STD:
                SNPRINTF2(plc, "STANDARD");
                break;
            case PJMEDIA_RTCP_XR_PLC_UNK:
            default:
                SNPRINTF2(plc, "unknown");
                break;
        }

        switch ((xr_stat.tx.voip_mtc.rx_config>>4) & 3) {
            case PJMEDIA_RTCP_XR_JB_FIXED:
                SNPRINTF2(jba, "FIXED");
                break;
            case PJMEDIA_RTCP_XR_JB_ADAPTIVE:
                SNPRINTF2(jba, "ADAPTIVE");
                break;
            default:
                SNPRINTF2(jba, "unknown");
                break;
        }

        SNPRINTF3(jbr, "%d", xr_stat.tx.voip_mtc.rx_config & 0x0F);

        if (xr_stat.tx.voip_mtc.update.sec == 0)
            pj_ansi_strxcpy(last_update, "never", sizeof(last_update));
        else {
            pj_gettimeofday(&now);
            PJ_TIME_VAL_SUB(now, xr_stat.tx.voip_mtc.update);
            pj_ansi_snprintf(last_update, sizeof(last_update),
                    "%02ldh:%02ldm:%02ld.%03lds ago",
                    now.sec / 3600,
                    (now.sec % 3600) / 60,
                    now.sec % 60,
                    now.msec);
        }

        printf(" TX last update: %s\n"
               "    packets    : loss rate=%d (%.2f%%), discard rate=%d (%.2f%%)\n"
               "    burst      : density=%d (%.2f%%), duration=%d%s\n"
               "    gap        : density=%d (%.2f%%), duration=%d%s\n"
               "    delay      : round trip=%d%s, end system=%d%s\n"
               "    level      : signal=%s%s, noise=%s%s, RERL=%s%s\n"
               "    quality    : R factor=%s, ext R factor=%s\n"
               "                 MOS LQ=%s, MOS CQ=%s\n"
               "    config     : PLC=%s, JB=%s, JB rate=%s, Gmin=%d\n"
               "    JB delay   : cur=%d%s, max=%d%s, abs max=%d%s\n",
               last_update,
               /* pakcets */
               xr_stat.tx.voip_mtc.loss_rate, xr_stat.tx.voip_mtc.loss_rate*100.0/256,
               xr_stat.tx.voip_mtc.discard_rate, xr_stat.tx.voip_mtc.discard_rate*100.0/256,
               /* burst */
               xr_stat.tx.voip_mtc.burst_den, xr_stat.tx.voip_mtc.burst_den*100.0/256,
               xr_stat.tx.voip_mtc.burst_dur, "ms",
               /* gap */
               xr_stat.tx.voip_mtc.gap_den, xr_stat.tx.voip_mtc.gap_den*100.0/256,
               xr_stat.tx.voip_mtc.gap_dur, "ms",
               /* delay */
               xr_stat.tx.voip_mtc.rnd_trip_delay, "ms",
               xr_stat.tx.voip_mtc.end_sys_delay, "ms",
               /* level */
               signal_lvl, "dB",
               noise_lvl, "dB",
               rerl, "",
               /* quality */
               r_factor, ext_r_factor, mos_lq, mos_cq,
               /* config */
               plc, jba, jbr, xr_stat.tx.voip_mtc.gmin,
               /* JB delay */
               xr_stat.tx.voip_mtc.jb_nom, "ms",
               xr_stat.tx.voip_mtc.jb_max, "ms",
               xr_stat.tx.voip_mtc.jb_abs_max, "ms"
               );


        /* RTT delay (by receiver side) */
        printf("          (msec)    min     avg     max     last    dev\n");
        printf(" RTT delay     : %7.3f %7.3f %7.3f %7.3f %7.3f%s\n", 
               xr_stat.rtt.min / 1000.0,
               xr_stat.rtt.mean / 1000.0,
               xr_stat.rtt.max / 1000.0,
               xr_stat.rtt.last / 1000.0,
               pj_math_stat_get_stddev(&xr_stat.rtt) / 1000.0,
               ""
               );
    } while (0);
#endif /* PJMEDIA_HAS_RTCP_XR */

}


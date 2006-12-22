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


static const char *desc = 
 " streamutil								\n"
 "									\n"
 " PURPOSE:								\n"
 "  Demonstrate how to use pjmedia stream component to transmit/receive \n"
 "  RTP packets to/from sound device.		    			\n"
 "\n"
 "\n"
 " USAGE:								\n"
 "  streamutil [options]                                                \n"
 "\n"
 "\n"
 " Options:\n"
 "  --codec=CODEC         Set the codec name.                           \n"
 "  --local-port=PORT     Set local RTP port (default=4000)		\n"
 "  --remote=IP:PORT      Set the remote peer. If this option is set,	\n"
 "                        the program will transmit RTP audio to the	\n"
 "                        specified address. (default: recv only)	\n"
 "  --play-file=WAV       Send audio from the WAV file instead of from	\n"
 "                        the sound device.				\n"
 "  --record-file=WAV     Record incoming audio to WAV file instead of	\n"
 "                        playing it to sound device.			\n"
 "  --send-recv           Set stream direction to bidirectional.        \n"
 "  --send-only           Set stream direction to send only		\n"
 "  --recv-only           Set stream direction to recv only (default)   \n"
 "\n"
;



#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>

#include <stdlib.h>	/* atoi() */
#include <stdio.h>

#include "util.h"


#define THIS_FILE	"stream.c"



/* Prototype */
static void print_stream_stat(pjmedia_stream *stream);


/* 
 * Register all codecs. 
 */
static pj_status_t init_codecs(pjmedia_endpt *med_endpt)
{
    pj_status_t status;

#if defined(PJMEDIA_HAS_G711_CODEC) && PJMEDIA_HAS_G711_CODEC!=0
    status = pjmedia_codec_g711_init(med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

#if defined(PJMEDIA_HAS_GSM_CODEC) && PJMEDIA_HAS_GSM_CODEC!=0
    status = pjmedia_codec_gsm_init(med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

#if defined(PJMEDIA_HAS_SPEEX_CODEC) && PJMEDIA_HAS_SPEEX_CODEC!=0
    status = pjmedia_codec_speex_init(med_endpt, 0, -1, -1);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

#if defined(PJMEDIA_HAS_L16_CODEC) && PJMEDIA_HAS_L16_CODEC!=0
    status = pjmedia_codec_l16_init(med_endpt, 0);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

    return PJ_SUCCESS;
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
				  pjmedia_stream **p_stream )
{
    pjmedia_stream_info info;
    pjmedia_transport *transport;
    pj_status_t status;


    /* Reset stream info. */
    pj_bzero(&info, sizeof(info));


    /* Initialize stream info formats */
    info.type = PJMEDIA_TYPE_AUDIO;
    info.dir = dir;
    pj_memcpy(&info.fmt, codec_info, sizeof(pjmedia_codec_info));
    info.tx_pt = codec_info->pt;
    info.ssrc = pj_rand();
    

    /* Copy remote address */
    pj_memcpy(&info.rem_addr, rem_addr, sizeof(pj_sockaddr_in));


    /* Create media transport */
    status = pjmedia_transport_udp_create(med_endpt, NULL, local_port,
					  0, &transport);
    if (status != PJ_SUCCESS)
	return status;


    /* Now that the stream info is initialized, we can create the 
     * stream.
     */

    status = pjmedia_stream_create( med_endpt, pool, &info, 
				    transport, NULL, p_stream);

    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error creating stream", status);
	pjmedia_transport_udp_close(transport);
	return status;
    }


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
    pj_status_t status; 


    /* Default values */
    const pjmedia_codec_info *codec_info;
    pjmedia_dir dir = PJMEDIA_DIR_DECODING;
    pj_sockaddr_in remote_addr;
    pj_uint16_t local_port = 4000;
    char *codec_id = NULL;
    char *rec_file = NULL;
    char *play_file = NULL;

    enum {
	OPT_CODEC	= 'c',
	OPT_LOCAL_PORT	= 'p',
	OPT_REMOTE	= 'r',
	OPT_PLAY_FILE	= 'w',
	OPT_RECORD_FILE	= 'R',
	OPT_SEND_RECV	= 'b',
	OPT_SEND_ONLY	= 's',
	OPT_RECV_ONLY	= 'i',
	OPT_HELP	= 'h',
    };

    struct pj_getopt_option long_options[] = {
	{ "codec",	    1, 0, OPT_CODEC },
	{ "local-port",	    1, 0, OPT_LOCAL_PORT },
	{ "remote",	    1, 0, OPT_REMOTE },
	{ "play-file",	    1, 0, OPT_PLAY_FILE },
	{ "record-file",    1, 0, OPT_RECORD_FILE },
	{ "send-recv",      0, 0, OPT_SEND_RECV },
	{ "send-only",      0, 0, OPT_SEND_ONLY },
	{ "recv-only",      0, 0, OPT_RECV_ONLY },
	{ "help",	    0, 0, OPT_HELP },
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

	case OPT_HELP:
	    usage();
	    return 1;

	default:
	    printf("Invalid options %s\n", argv[pj_optind]);
	    return 1;
	}

    }


    /* Verify arguments. */
    if (dir & PJMEDIA_DIR_ENCODING) {
	if (remote_addr.sin_addr.s_addr == 0) {
	    printf("Error: remote address must be set\n");
	    return 1;
	}
    }

    if (play_file != NULL && dir != PJMEDIA_DIR_ENCODING) {
	printf("Direction is set to --send-only because of --play-file\n");
	dir = PJMEDIA_DIR_ENCODING;
    }


    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for application purpose */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "app",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
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

    /* Create stream based on program arguments */
    status = create_stream(pool, med_endpt, codec_info, dir, local_port, 
			   &remote_addr, &stream);
    if (status != PJ_SUCCESS)
	goto on_exit;


    /* Get the port interface of the stream */
    status = pjmedia_stream_get_port( stream, &stream_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    if (play_file) {
	unsigned wav_ptime;

	wav_ptime = stream_port->info.samples_per_frame * 1000 /
		    stream_port->info.clock_rate;
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
					        stream_port->info.clock_rate,
						stream_port->info.channel_count,
						stream_port->info.samples_per_frame,
						stream_port->info.bits_per_sample,
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
					stream_port->info.clock_rate,
					stream_port->info.channel_count,
					stream_port->info.samples_per_frame,
					stream_port->info.bits_per_sample,
					0, &snd_port);
	else if (dir == PJMEDIA_DIR_ENCODING)
	    status = pjmedia_snd_port_create_rec(pool, -1, 
					stream_port->info.clock_rate,
					stream_port->info.channel_count,
					stream_port->info.samples_per_frame,
					stream_port->info.bits_per_sample,
					0, &snd_port);
	else
	    status = pjmedia_snd_port_create_player(pool, -1, 
					stream_port->info.clock_rate,
					stream_port->info.channel_count,
					stream_port->info.samples_per_frame,
					stream_port->info.bits_per_sample,
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
	       pj_inet_ntoa(remote_addr.sin_addr),
	       pj_ntohs(remote_addr.sin_port));
    else
	printf("Stream is active, send/recv, local port is %d, "
	       "sending to %s:%d\n",
	       local_port,
	       pj_inet_ntoa(remote_addr.sin_addr),
	       pj_ntohs(remote_addr.sin_port));


    for (;;) {

	puts("");
	puts("Commands:");
	puts("  s     Display media statistics");
	puts("  q     Quit");
	puts("");

	printf("Command: "); fflush(stdout);

	fgets(tmp, sizeof(tmp), stdin);

	if (tmp[0] == 's')
	    print_stream_stat(stream);
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

    /* If there is master port, then we just need to destroy master port
     * (it will recursively destroy upstream and downstream ports, which
     * in this case are file_port and stream_port).
     */
    if (master_port) {
	pjmedia_master_port_destroy(master_port, PJ_TRUE);
	play_file_port = NULL;
	stream = NULL;
    }

    /* Destroy stream */
    if (stream) {
	pjmedia_transport *tp;

	tp = pjmedia_stream_get_transport(stream);
	pjmedia_stream_destroy(stream);
	pjmedia_transport_udp_close(tp);
    }

    /* Destroy file ports */
    if (play_file_port)
	pjmedia_port_destroy( play_file_port );
    if (rec_file_port)
	pjmedia_port_destroy( rec_file_port );


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



/*
 * Print stream statistics
 */
static void print_stream_stat(pjmedia_stream *stream)
{
    char duration[80], last_update[80];
    char bps[16], ipbps[16], packets[16], bytes[16], ipbytes[16];
    pjmedia_port *port;
    pjmedia_rtcp_stat stat;
    pj_time_val now;


    pj_gettimeofday(&now);
    pjmedia_stream_get_stat(stream, &stat);
    pjmedia_stream_get_port(stream, &port);

    puts("Stream statistics:");

    /* Print duration */
    PJ_TIME_VAL_SUB(now, stat.start);
    sprintf(duration, " Duration: %02ld:%02ld:%02ld.%03ld",
	    now.sec / 3600,
	    (now.sec % 3600) / 60,
	    (now.sec % 60),
	    now.msec);


    printf(" Info: audio %.*s@%dHz, %dms/frame, %sB/s (%sB/s +IP hdr)\n",
   	(int)port->info.encoding_name.slen,
	port->info.encoding_name.ptr,
	port->info.clock_rate,
	port->info.samples_per_frame * 1000 / port->info.clock_rate,
	good_number(bps, port->info.bytes_per_frame * port->info.clock_rate /
		    port->info.samples_per_frame),
	good_number(ipbps, (port->info.bytes_per_frame+32) * 
			    port->info.clock_rate / port->info.clock_rate));

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

    printf(" RX stat last update: %s\n"
	   "    total %s packets %sB received (%sB +IP hdr)%s\n"
	   "    pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
	   "          (msec)    min     avg     max     last\n"
	   "    loss period: %7.3f %7.3f %7.3f %7.3f%s\n"
	   "    jitter     : %7.3f %7.3f %7.3f %7.3f%s\n",
	   last_update,
	   good_number(packets, stat.rx.pkt),
	   good_number(bytes, stat.rx.bytes),
	   good_number(ipbytes, stat.rx.bytes + stat.rx.pkt * 32),
	   "",
	   stat.rx.loss,
	   stat.rx.loss * 100.0 / (stat.rx.pkt + stat.rx.loss),
	   stat.rx.dup, 
	   stat.rx.dup * 100.0 / (stat.rx.pkt + stat.rx.loss),
	   stat.rx.reorder, 
	   stat.rx.reorder * 100.0 / (stat.rx.pkt + stat.rx.loss),
	   "",
	   stat.rx.loss_period.min / 1000.0, 
	   stat.rx.loss_period.avg / 1000.0, 
	   stat.rx.loss_period.max / 1000.0,
	   stat.rx.loss_period.last / 1000.0,
	   "",
	   stat.rx.jitter.min / 1000.0,
	   stat.rx.jitter.avg / 1000.0,
	   stat.rx.jitter.max / 1000.0,
	   stat.rx.jitter.last / 1000.0,
	   ""
	   );


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

    printf(" TX stat last update: %s\n"
	   "    total %s packets %sB sent (%sB +IP hdr)%s\n"
	   "    pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
	   "          (msec)    min     avg     max     last\n"
	   "    loss period: %7.3f %7.3f %7.3f %7.3f%s\n"
	   "    jitter     : %7.3f %7.3f %7.3f %7.3f%s\n",
	   last_update,
	   good_number(packets, stat.tx.pkt),
	   good_number(bytes, stat.tx.bytes),
	   good_number(ipbytes, stat.tx.bytes + stat.tx.pkt * 32),
	   "",
	   stat.tx.loss,
	   stat.tx.loss * 100.0 / (stat.tx.pkt + stat.tx.loss),
	   stat.tx.dup, 
	   stat.tx.dup * 100.0 / (stat.tx.pkt + stat.tx.loss),
	   stat.tx.reorder, 
	   stat.tx.reorder * 100.0 / (stat.tx.pkt + stat.tx.loss),
	   "",
	   stat.tx.loss_period.min / 1000.0, 
	   stat.tx.loss_period.avg / 1000.0, 
	   stat.tx.loss_period.max / 1000.0,
	   stat.tx.loss_period.last / 1000.0,
	   "",
	   stat.tx.jitter.min / 1000.0,
	   stat.tx.jitter.avg / 1000.0,
	   stat.tx.jitter.max / 1000.0,
	   stat.tx.jitter.last / 1000.0,
	   ""
	   );


    printf(" RTT delay     : %7.3f %7.3f %7.3f %7.3f%s\n", 
	   stat.rtt.min / 1000.0,
	   stat.rtt.avg / 1000.0,
	   stat.rtt.max / 1000.0,
	   stat.rtt.last / 1000.0,
	   ""
	   );

}


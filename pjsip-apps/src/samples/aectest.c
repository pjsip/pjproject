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
 * \page page_pjmedia_samples_aectest_c Samples: AEC Test (aectest.c)
 *
 * Play a file to speaker, run AEC, and record the microphone input
 * to see if echo is coming.
 *
 * This file is pjsip-apps/src/samples/aectest.c
 *
 * \includelineno aectest.c
 */
#include <pjmedia.h>
#include <pjlib-util.h>	/* pj_getopt */
#include <pjlib.h>

/* For logging purpose. */
#define THIS_FILE   "playfile.c"
#define PTIME	    20

static const char *desc = 
" FILE		    						    \n"
"		    						    \n"
"  aectest.c	    						    \n"
"		    						    \n"
" PURPOSE	    						    \n"
"		    						    \n"
"  Test the AEC effectiveness.					    \n"
"		    						    \n"
" USAGE		    						    \n"
"		    						    \n"
"  aectest INPUT.WAV OUTPUT.WAV					    \n"
"		    						    \n"
"  INPUT.WAV is the file to be played to the speaker.		    \n"
"  OUTPUT.WAV is the output file containing recorded signal from the\n"
"  microphone.";


static void app_perror(const char *sender, const char *title, pj_status_t st)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(st, errmsg, sizeof(errmsg));
    PJ_LOG(3,(sender, "%s: %s", title, errmsg));
}


/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t	  *pool;
    pjmedia_port  *play_port;
    pjmedia_port  *rec_port;
    pjmedia_port  *bidir_port;
    pjmedia_snd_port *snd;
    char tmp[10];
    pj_status_t status;


    if (argc != 3) {
    	puts("Error: arguments required");
	puts(desc);
	return 1;
    }


    /* Must init PJLIB first: */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for our file player */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    /* Create file media port from the WAV file */
    status = pjmedia_wav_player_port_create(  pool,	/* memory pool	    */
					      argv[1],	/* file to play	    */
					      PTIME,	/* ptime.	    */
					      0,	/* flags	    */
					      0,	/* default buffer   */
					      &play_port);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open input WAV file", status);
	return 1;
    }

    if (play_port->info.channel_count != 1) {
	puts("Error: input WAV must have 1 channel audio");
	return 1;
    }
    if (play_port->info.bits_per_sample != 16) {
	puts("Error: input WAV must be encoded as 16bit PCM");
	return 1;
    }

#ifdef PJ_DARWINOS
    /* Need to force clock rate on MacOS */
    if (play_port->info.clock_rate != 44100) {
	pjmedia_port *resample_port;

	status = pjmedia_resample_port_create(pool, play_port, 44100, 0,
					      &resample_port);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to create resampling port", status);
	    return 1;
	}

	data.play_port = resample_port;
    }
#endif

    /* Create WAV output file port */
    status = pjmedia_wav_writer_port_create(pool, argv[2], 
					    play_port->info.clock_rate,
					    play_port->info.channel_count,
					    play_port->info.samples_per_frame,
					    play_port->info.bits_per_sample,
					    0, 0, &rec_port);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open output file", status);
	return 1;
    }

    /* Create bidirectional port from the WAV ports */
    pjmedia_bidirectional_port_create(pool, play_port, rec_port, &bidir_port);

    /* Create AEC port */
    if (0) {
	pjmedia_aec_port_create(pool, bidir_port, 
				bidir_port->info.clock_rate * 200 / 1000,
				&bidir_port);
    }

    /* Create sound device. */
    status = pjmedia_snd_port_create(pool, -1, -1, 
				     play_port->info.clock_rate,
				     play_port->info.channel_count,
				     play_port->info.samples_per_frame,
				     play_port->info.bits_per_sample,
				     0, &snd);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open sound device", status);
	return 1;
    }


    /* Connect sound to the port */
    pjmedia_snd_port_connect(snd, bidir_port);


    puts("");
    printf("Playing %s and recording to %s\n", argv[1], argv[2]);
    puts("Press <ENTER> to quit");

    fgets(tmp, sizeof(tmp), stdin);

    
    /* Start deinitialization: */

    /* Destroy sound device */
    status = pjmedia_snd_port_destroy( snd );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Destroy file port(s) */
    status = pjmedia_port_destroy( play_port );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    status = pjmedia_port_destroy( rec_port );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Release application pool */
    pj_pool_release( pool );

    /* Destroy media endpoint. */
    pjmedia_endpt_destroy( med_endpt );

    /* Destroy pool factory */
    pj_caching_pool_destroy( &cp );


    /* Done. */
    return 0;
}


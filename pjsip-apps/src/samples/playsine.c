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

/*
 * playsine.c
 *
 * PURPOSE:
 *  Demonstrate how to create and use custom media port which
 *  simply feed a sine wav to the sound player.
 *
 * USAGE:
 *  playsine [nchannel]
 *
 * where:
 *  nchannel is 1 for mono (this is the default) or 2 for stereo.
 */

#include <pjmedia.h>
#include <pjlib.h>

#include <stdlib.h>	/* atoi() */
#include <stdio.h>


/* For logging purpose. */
#define THIS_FILE   "playsine.c"


/* Util to display the error message for the specified error code  */
static int app_perror( const char *sender, const char *title, 
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    printf("%s: %s [code=%d]\n", title, errmsg, status);
    return 1;
}


/* Struct attached to sine generator */
typedef struct
{
    pj_int16_t	*samples;	/* Sine samples.    */
} port_data;


/* This callback is called to feed more samples */
static pj_status_t sine_get_frame( pjmedia_port *port, 
				   pjmedia_frame *frame)
{
    port_data *sine = port->user_data;
    pj_int16_t *samples = frame->buf;
    unsigned i, count, left, right;

    /* Get number of samples */
    count = frame->size / 2 / port->info.channel_count;

    left = 0;
    right = 0;

    for (i=0; i<count; ++i) {
	*samples++ = sine->samples[left];
	++left;

	if (port->info.channel_count == 2) {
	    *samples++ = sine->samples[right];
	    right += 2; /* higher pitch so we can distinguish left and right. */
	    if (right >= count)
		right = 0;
	}
    }

    /* Must set frame->type correctly, otherwise the sound device
     * will refuse to play.
     */
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;

    return PJ_SUCCESS;
}

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

/*
 * Create a media port to generate sine wave samples.
 */
static pj_status_t create_sine_port(pj_pool_t *pool,
				    unsigned sampling_rate,
				    unsigned channel_count,
				    pjmedia_port **p_port)
{
    pjmedia_port *port;
    unsigned i;
    unsigned count;
    port_data *sine;

    PJ_ASSERT_RETURN(pool && channel_count > 0 && channel_count <= 2, 
		     PJ_EINVAL);

    port = pj_pool_zalloc(pool, sizeof(pjmedia_port));
    PJ_ASSERT_RETURN(port != NULL, PJ_ENOMEM);

    /* Fill in port info. */
    port->info.bits_per_sample = 16;
    port->info.channel_count = channel_count;
    port->info.encoding_name = pj_str("pcm");
    port->info.has_info = 1;
    port->info.name = pj_str("sine generator");
    port->info.need_info = 0;
    port->info.pt = 0xFF;
    port->info.sample_rate = sampling_rate;
    port->info.samples_per_frame = sampling_rate * 20 / 1000 * channel_count;
    port->info.bytes_per_frame = port->info.samples_per_frame * 2;
    port->info.type = PJMEDIA_TYPE_AUDIO;
    
    /* Set the function to feed frame */
    port->get_frame = &sine_get_frame;

    /* Create sine port data */
    port->user_data = sine = pj_pool_zalloc(pool, sizeof(port_data));

    /* Create samples */
    count = port->info.samples_per_frame / channel_count;
    sine->samples = pj_pool_alloc(pool, count * sizeof(pj_int16_t));
    PJ_ASSERT_RETURN(sine->samples != NULL, PJ_ENOMEM);

    /* initialise sinusoidal wavetable */
    for( i=0; i<count; i++ )
    {
        sine->samples[i] = (pj_int16_t) (10000.0 * 
		sin(((double)i/(double)count) * M_PI * 8.) );
    }

    *p_port = port;

    return PJ_SUCCESS;
}


/* Show usage */
static void usage(void)
{
    puts("");
    puts("Usage: playsine [nchannel]");
    puts("");
    puts("where");
    puts(" nchannel is number of audio channels (1 for mono, or 2 for stereo).");
    puts(" Default is 1 (mono).");
    puts("");
}


/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_port *sine_port;
    pjmedia_snd_port *snd_port;
    char tmp[10];
    int channel_count = 1;
    pj_status_t status;

    if (argc == 2) {
	channel_count = atoi(argv[1]);
	if (channel_count < 1 || channel_count > 2) {
	    puts("Error: invalid arguments");
	    usage();
	    return 1;
	}
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

    /* Create memory pool for our sine generator */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    /* Create a media port to generate sine wave samples. */
    status = create_sine_port( pool,	    /* memory pool	    */
			       11025,	    /* sampling rate	    */
			       channel_count,/* # of channels	    */
			       &sine_port   /* returned port	    */
		             );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create sine port", status);
	return 1;
    }

    /* Create sound player port. */
    status = pjmedia_snd_port_create_player( 
		 pool,				    /* pool		    */
		 -1,				    /* use default dev.	    */
		 sine_port->info.sample_rate,	    /* clock rate.	    */
		 sine_port->info.channel_count,	    /* # of channels.	    */
		 sine_port->info.samples_per_frame, /* samples per frame.   */
		 sine_port->info.bits_per_sample,   /* bits per sample.	    */
		 0,				    /* options		    */
		 &snd_port			    /* returned port	    */
		 );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open sound device", status);
	return 1;
    }

    /* Connect sine generator port to the sound player 
     * Stream playing will commence immediately.
     */
    status = pjmedia_snd_port_connect( snd_port, sine_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);



    /* 
     * Audio should be playing in a loop now, using sound device's thread. 
     */


    /* Sleep to allow log messages to flush */
    pj_thread_sleep(100);


    puts("Playing sine wave..");
    puts("");
    puts("Press <ENTER> to stop playing and quit");

    fgets(tmp, sizeof(tmp), stdin);

    
    /* Start deinitialization: */

    /* Destroy sound device */
    status = pjmedia_snd_port_destroy( snd_port );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Destroy sine generator */
    status = pjmedia_port_destroy( sine_port );
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

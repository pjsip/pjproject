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

static const char *desc = 
 " sndinfo.c								    \n"
 "									    \n"
 " PURPOSE:								    \n"
 "  Print sound device info and test open device.			    \n"
 "									    \n"
 " USAGE:								    \n"
 "  sndinfo [id rec/play/both clockrate nchan bits]			    \n"
 "									    \n"
 " DESCRIPTION:								    \n"
 "  When invoked without any arguments, it displays information about all   \n"
 "  sound devices in the system.					    \n"
 "									    \n"
 "  When invoked with arguments, the program tests if device can be opened  \n"
 "  with the specified arguments. All these arguments must be specified:    \n"
 "  - id             The device ID (-1 for the first capable device)	    \n"
 "  - rec/play/both  Specify which streams to open.			    \n"
 "  - clockrate      Specify clock rate (e.g. 8000, 11025, etc.)	    \n"
 "  - nchan	     Number of channels (1=mono, 2=stereo).		    \n"
 "  - bits	     Number of bits per sample (normally 16).		    \n";

#include <pjmedia.h>
#include <pjlib.h>

#include <stdlib.h>	/* atoi() */
#include <stdio.h>


static void enum_devices(void)
{
    int i, count;
    
    count = pjmedia_snd_get_dev_count();
    if (count == 0) {
	puts("No devices found");
	return;
    }

    for (i=0; i<count; ++i) {
	const pjmedia_snd_dev_info *info;

	info = pjmedia_snd_get_dev_info(i);
	pj_assert(info != NULL);

	printf( "Device #%02d: \n"
		"  Name                : %s\n"
		"  # of input channels : %d\n"
		"  # of output channels: %d\n"
		"  Default clock rate  : %d Hz\n\n",
		i, info->name, info->input_count, info->output_count,
		info->default_samples_per_sec);
    }
    puts("");
    puts("Run with -h to get more options");
}

static unsigned clock_rate;
static unsigned play_counter;
static unsigned rec_counter;
static unsigned min_delay = 0xFFFF, max_delay;
static char play_delays[1000];
static pj_uint32_t last_play_timestamp, last_rec_timestamp;

static pj_status_t play_cb(void *user_data, pj_uint32_t timestamp,
			   void *output, unsigned size)
{
    static pj_timestamp last_cb;


    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(output);
    PJ_UNUSED_ARG(size);


    ++play_counter;
    last_play_timestamp = timestamp;

    if (last_cb.u64 == 0) {
	pj_get_timestamp(&last_cb);
    } else if (play_counter <= PJ_ARRAY_SIZE(play_delays)) {
	pj_timestamp now;
	unsigned delay;

	pj_get_timestamp(&now);
	
	delay = pj_elapsed_msec(&last_cb, &now);
	if (delay < min_delay)
	    min_delay = delay;
	if (delay > max_delay)
	    max_delay = delay;

	last_cb = now;

	play_delays[play_counter-1] = (char)delay;
    }

    return PJ_SUCCESS;
}

static pj_status_t rec_cb(void *user_data, pj_uint32_t timestamp,
			  const void *input, unsigned size)
{

    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(input);
    PJ_UNUSED_ARG(user_data);


    ++rec_counter;

    if (timestamp - last_rec_timestamp >= clock_rate && last_play_timestamp) {
	int diff;
	diff = last_play_timestamp - timestamp;
	printf("Play timestamp=%u, capture timestamp=%u, diff=%d\n",
	       last_play_timestamp, timestamp, diff);
	last_rec_timestamp = timestamp;
    }
    return PJ_SUCCESS;
}

static void app_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));	
    printf( "%s: %s (err=%d)\n",
	    title, errmsg, status);
}

static int open_device(int dev_id, pjmedia_dir dir,
		       int nchannel, int bits)
{
    pj_status_t status = PJ_SUCCESS;
    unsigned nsamples;
    pjmedia_snd_stream *strm;
    const char *dirtype;
    char tmp[10];
    unsigned i;

    switch (dir) {
    case PJMEDIA_DIR_CAPTURE:
	dirtype = "capture"; break;    
    case PJMEDIA_DIR_PLAYBACK:
	dirtype = "playback"; break;    
    case PJMEDIA_DIR_CAPTURE_PLAYBACK:
	dirtype = "capture/playback"; break;    
    default:
	return 1;
    }
    
    nsamples = clock_rate * 20 / 1000;

    printf( "Opening device %d for %s: clockrate=%d, nchannel=%d, "
	    "bits=%d, nsamples=%d..\n",
	    dev_id, dirtype, clock_rate, nchannel, bits, nsamples);

    if (dir == PJMEDIA_DIR_CAPTURE) {
	status = pjmedia_snd_open_rec( dev_id, clock_rate, nchannel,
				       nsamples, bits, &rec_cb, NULL,
				       &strm);
    } else if (dir == PJMEDIA_DIR_PLAYBACK) {
	status = pjmedia_snd_open_player( dev_id, clock_rate, nchannel,
					  nsamples, bits, &play_cb, NULL,
					  &strm);
    } else {
	status = pjmedia_snd_open( dev_id, dev_id, clock_rate, nchannel,
				   nsamples, bits, &rec_cb, &play_cb, NULL,
				   &strm);
    }
    
    if (status != PJ_SUCCESS) {
        app_perror("Unable to open device for capture", status);
        return 1;
    }

    status = pjmedia_snd_stream_start(strm);
    if (status != PJ_SUCCESS) {
        app_perror("Unable to start capture stream", status);
        return 1;
    }
    
    /* Let playback/capture runs for a while */
    //pj_thread_sleep(1000);
    puts("Press <ENTER> to stop");
    fgets(tmp, sizeof(tmp), stdin);


    pjmedia_snd_stream_close(strm);

    if ((dir & PJMEDIA_DIR_CAPTURE) && rec_counter==0) {
	printf("Error: capture stream was not running\n");
	return 1;
    }

    if ((dir & PJMEDIA_DIR_PLAYBACK) && play_counter==0) {
	printf("Error: playback stream was not running\n");
	return 1;
    }

    puts("Success.");

    printf("Delay: ");
    for (i=0; i<play_counter; ++i)
	printf("%d ", play_delays[i]);

    puts("");
    if (dir & PJMEDIA_DIR_PLAYBACK) {
	printf("Callback interval: min interval=%d ms, max interval=%d ms\n",
	       min_delay, max_delay);
    }
    

    return 0;
}


int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_status_t status;

    /* Init pjlib */
    status = pj_init();
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, 1);
    
    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    
    if (argc == 1) {
	enum_devices();
	return 0;
    } else if (argc == 6) {
	
	int dev_id;
	pjmedia_dir dir = PJMEDIA_DIR_NONE;
	int nchannel;
	int bits;

	dev_id = atoi(argv[1]);

	if (strcmp(argv[2], "rec")==0)
	    dir = PJMEDIA_DIR_CAPTURE;
	else if (strcmp(argv[2], "play")==0)
	    dir = PJMEDIA_DIR_PLAYBACK;
	else if (strcmp(argv[2], "both")==0)
	    dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;

	clock_rate = atoi(argv[3]);
	nchannel = atoi(argv[4]);
	bits = atoi(argv[5]);

	return open_device(dev_id, dir, nchannel, bits);

    } else {
	puts("Error: invalid arguments");
	puts(desc);
	return 1;
    }

    return 0;
}



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
 * \page page_pjmedia_samples_sndtest_c Samples: Sound Card Benchmark
 *
 * This example can be used to benchmark the quality of the sound card
 * installed in the system. At the end of the test, it will report
 * the jitter and clock drifts of the device.
 *
 * This file is pjsip-apps/src/samples/sndtest.c
 *
 * Screenshots on WinXP: \image html sndtest.jpg
 *
 * \includelineno sndtest.c
 */


#include <pjmedia.h>
#include <pjlib.h>
#include <pjlib-util.h>

#include <stdlib.h>	/* atoi() */
#include <stdio.h>



#define THIS_FILE	    "sndtest.c"

/* Warn (print log with yellow color) if frame jitter is larger than
 * this value (in usec).
 */
#define WARN_JITTER_USEC    1000

/* Test duration in msec */
#define DURATION	    10000

/* Skip the first msec from the calculation */
#define SKIP_DURATION	    1000

/* Max frames per sec (to calculate number of delays to keep). */
#define MAX_FRAMES_PER_SEC  100

/* Number of frame durations to keep */
#define MAX_DELAY_COUNTER   (((DURATION/1000)+1)*MAX_FRAMES_PER_SEC)


struct stream_data
{
    pj_uint32_t	    first_timestamp;
    pj_uint32_t	    last_timestamp;
    pj_timestamp    last_called;
    unsigned	    counter;
    unsigned	    min_delay;
    unsigned	    max_delay;
    unsigned	    delay[MAX_DELAY_COUNTER];
};

struct test_data {
    pjmedia_dir dir;
    unsigned clock_rate;
    unsigned samples_per_frame;
    unsigned channel_count;
    pj_bool_t running;
    pj_bool_t has_error;

    struct stream_data capture_data;
    struct stream_data playback_data;
};



static const char *desc = 
 " sndtest.c						\n"
 "							\n"
 " PURPOSE:						\n"
 "  Test the performance of sound device.		\n"
 "							\n"
 " USAGE:						\n"
 "  sndtest --help					\n"
 "  sndtest [options]					\n"
 "							\n"
 " where options:					\n"
 "  --id=ID          -i  Use device ID (default is -1)	\n"
 "  --rate=HZ        -r  Set test clock rate (default=8000)\n"
 "  --frame=SAMPLES  -f  Set number of samples per frame\n"
 "  --channel=CH     -n  Set number of channels (default=1)\n"
 "  --verbose        -v  Show verbose result		\n"
 "  --help           -h  Show this screen		\n"
;



static void enum_devices(void)
{
    int i, count;
    
    count = pjmedia_snd_get_dev_count();
    if (count == 0) {
	PJ_LOG(3,(THIS_FILE, "No devices found"));
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Found %d devices:", count));
    for (i=0; i<count; ++i) {
	const pjmedia_snd_dev_info *info;

	info = pjmedia_snd_get_dev_info(i);
	pj_assert(info != NULL);

	PJ_LOG(3,(THIS_FILE," %d: %s (capture=%d, playback=%d)",
	          i, info->name, info->input_count, info->output_count));
    }
}


static const char *get_dev_name(int dev_id)
{
    const pjmedia_snd_dev_info *info;

    if (dev_id == -1)
	dev_id = 0;

    info = pjmedia_snd_get_dev_info(dev_id);
    if (info == NULL)
	return "????";

    return info->name;
}


static pj_status_t play_cb(void *user_data, pj_uint32_t timestamp,
			   void *output, unsigned size)
{
    struct test_data *test_data = user_data;
    struct stream_data *strm_data = &test_data->playback_data;

    /* Skip frames when test is not started or test has finished */
    if (!test_data->running) {
	pj_bzero(output, size);
	return PJ_SUCCESS;
    }

    /* Save last timestamp seen (to calculate drift) */
    strm_data->last_timestamp = timestamp;

    if (strm_data->last_called.u64 == 0) {
	pj_get_timestamp(&strm_data->last_called);
	/* Init min_delay to one frame */
	strm_data->min_delay = test_data->samples_per_frame * 1000000 /
			       test_data->clock_rate;
	strm_data->first_timestamp = timestamp;

    } else if (strm_data->counter <= MAX_DELAY_COUNTER) {
	pj_timestamp now;
	unsigned delay;

	pj_get_timestamp(&now);
	
	/* Calculate frame interval */
	delay = pj_elapsed_usec(&strm_data->last_called, &now);
	if (delay < strm_data->min_delay)
	    strm_data->min_delay = delay;
	if (delay > strm_data->max_delay)
	    strm_data->max_delay = delay;

	strm_data->last_called = now;

	/* Save the frame interval for later calculation */
	strm_data->delay[strm_data->counter] = delay;
	++strm_data->counter;

    } else {

	/* No space, can't take anymore frames */
	test_data->running = 0;

    }

    pj_bzero(output, size);
    return PJ_SUCCESS;
}

static pj_status_t rec_cb(void *user_data, pj_uint32_t timestamp,
			  void *input, unsigned size)
{

    struct test_data *test_data = user_data;
    struct stream_data *strm_data = &test_data->capture_data;

    PJ_UNUSED_ARG(input);
    PJ_UNUSED_ARG(size);

    /* Skip frames when test is not started or test has finished */
    if (!test_data->running) {
	return PJ_SUCCESS;
    }

    /* Save last timestamp seen (to calculate drift) */
    strm_data->last_timestamp = timestamp;

    if (strm_data->last_called.u64 == 0) {
	pj_get_timestamp(&strm_data->last_called);
	/* Init min_delay to one frame */
	strm_data->min_delay = test_data->samples_per_frame * 1000000 /
			       test_data->clock_rate;
	strm_data->first_timestamp = timestamp;

    } else if (strm_data->counter <= MAX_DELAY_COUNTER) {
	pj_timestamp now;
	unsigned delay;

	pj_get_timestamp(&now);

	/* Calculate frame interval */
	delay = pj_elapsed_usec(&strm_data->last_called, &now);
	if (delay < strm_data->min_delay)
	    strm_data->min_delay = delay;
	if (delay > strm_data->max_delay)
	    strm_data->max_delay = delay;

	strm_data->last_called = now;

	/* Save the frame interval for later calculation */
	strm_data->delay[strm_data->counter] = delay;
	++strm_data->counter;

    } else {

	/* No space, can't take anymore frames */
	test_data->running = 0;

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


static void print_stream_data(const char *title, 
			      struct test_data *test_data,
			      struct stream_data *strm_data,
			      int verbose)
{
    unsigned i, dur;
    int ptime;
    unsigned min_jitter, max_jitter, sum_jitter, avg_jitter=0;

    PJ_LOG(3,(THIS_FILE, "  %s stream report:", title));

    /* Check that frames are captured/played */
    if (strm_data->counter == 0) {
	PJ_LOG(1,(THIS_FILE, "   Error: no frames are captured/played!"));
	test_data->has_error = 1;
	return;
    }

    /* Duration */
    dur = (strm_data->counter+1) * test_data->samples_per_frame * 1000 /
		test_data->clock_rate;
    PJ_LOG(3,(THIS_FILE, "   Duration: %ds.%03d",
	      dur/1000, dur%1000));

    /* Frame interval */
    if (strm_data->max_delay - strm_data->min_delay < WARN_JITTER_USEC) {
	PJ_LOG(3,(THIS_FILE, 
		  "   Frame interval: min=%d.%03dms, max=%d.%03dms",
		  strm_data->min_delay/1000, strm_data->min_delay%1000,
		  strm_data->max_delay/1000, strm_data->max_delay%1000));
    } else {
	test_data->has_error = 1;
	PJ_LOG(2,(THIS_FILE, 
		  "   Frame interval: min=%d.%03dms, max=%d.%03dms",
		  strm_data->min_delay/1000, strm_data->min_delay%1000,
		  strm_data->max_delay/1000, strm_data->max_delay%1000));
    }

    if (verbose) {
	unsigned i;
	unsigned decor = pj_log_get_decor();

	PJ_LOG(3,(THIS_FILE, "    Dumping frame delays:"));

	pj_log_set_decor(0);
	for (i=0; i<strm_data->counter; ++i)
	    PJ_LOG(3,(THIS_FILE, " %d.%03d", strm_data->delay[i]/1000, 
		      strm_data->delay[i]%1000));
	PJ_LOG(3,(THIS_FILE, "\r\n"));
	pj_log_set_decor(decor);
    }

    /* Calculate frame ptime in usec */
    ptime = test_data->samples_per_frame * 1000000 /
	    test_data->clock_rate;

    /* Calculate jitter */
    min_jitter = 0xFFFFF;
    max_jitter = 0;
    sum_jitter = 0;

    for (i=1; i<strm_data->counter; ++i) {
	int jitter1, jitter2, jitter;

	/* jitter1 is interarrival difference */
	jitter1 = strm_data->delay[i] - strm_data->delay[i-1];
	if (jitter1 < 0) jitter1 = -jitter1;
	
	/* jitter2 is difference between actual and scheduled arrival.
	 * This is intended to capture situation when frames are coming
	 * instantaneously, which will calculate as zero jitter with
	 * jitter1 calculation.
	 */
	jitter2 = ptime - strm_data->delay[i];
	if (jitter2 < 0) jitter2 = -jitter2;

	/* Set jitter as the maximum of the two jitter calculations. 
	 * This is intended to show the worst result.
	 */
	jitter = (jitter1>jitter2) ? jitter1 : jitter2;

	/* Calculate min, max, avg jitter */
	if (jitter < (int)min_jitter) min_jitter = jitter;
	if (jitter > (int)max_jitter) max_jitter = jitter;

	sum_jitter += jitter;
    }

    avg_jitter = (sum_jitter) / (strm_data->counter - 1);

    if (max_jitter < WARN_JITTER_USEC) {
	PJ_LOG(3,(THIS_FILE, 
		  "   Jitter: min=%d.%03dms, avg=%d.%03dms, max=%d.%03dms",
		  min_jitter/1000, min_jitter%1000, 
		  avg_jitter/1000, avg_jitter%1000,
		  max_jitter/1000, max_jitter%1000));
    } else {
	test_data->has_error = 1;
	PJ_LOG(2,(THIS_FILE, 
		  "   Jitter: min=%d.%03dms, avg=%d.%03dms, max=%d.%03dms",
		  min_jitter/1000, min_jitter%1000, 
		  avg_jitter/1000, avg_jitter%1000,
		  max_jitter/1000, max_jitter%1000));
    }
}


static int perform_test(const char *title, int dev_id, pjmedia_dir dir,
		        unsigned clock_rate, unsigned samples_per_frame, 
			unsigned nchannel, int verbose)
{
    pj_status_t status = PJ_SUCCESS;
    pjmedia_snd_stream *strm;
    struct test_data test_data;


    /*
     * Init test parameters
     */
    pj_bzero(&test_data, sizeof(test_data));
    test_data.dir = dir;
    test_data.clock_rate = clock_rate;
    test_data.samples_per_frame = samples_per_frame;
    test_data.channel_count = nchannel;

    /*
     * Open device.
     */
    PJ_LOG(3,(THIS_FILE, "Testing %s", title));

    if (dir == PJMEDIA_DIR_CAPTURE) {
	status = pjmedia_snd_open_rec( dev_id, clock_rate, nchannel,
				       samples_per_frame, 16, &rec_cb, 
				       &test_data, &strm);
    } else if (dir == PJMEDIA_DIR_PLAYBACK) {
	status = pjmedia_snd_open_player( dev_id, clock_rate, nchannel,
					  samples_per_frame, 16, &play_cb, 
					  &test_data, &strm);
    } else {
	status = pjmedia_snd_open( dev_id, dev_id, clock_rate, nchannel,
				   samples_per_frame, 16, &rec_cb, &play_cb, 
				   &test_data, &strm);
    }
    
    if (status != PJ_SUCCESS) {
        app_perror("Unable to open device for capture", status);
        return status;
    }

    /* Sleep for a while to let sound device "settles" */
    pj_thread_sleep(200);


    /*
     * Start the stream.
     */
    status = pjmedia_snd_stream_start(strm);
    if (status != PJ_SUCCESS) {
        app_perror("Unable to start capture stream", status);
        return status;
    }

    PJ_LOG(3,(THIS_FILE,
	      " Please wait while test is in progress (~%d secs)..",
	      (DURATION+SKIP_DURATION)/1000));

    /* Let the stream runs for few msec/sec to get stable result.
     * (capture normally begins with frames available simultaneously).
     */
    pj_thread_sleep(SKIP_DURATION);


    /* Begin gather data */
    test_data.running = 1;

    /* 
     * Let the test runs for a while.
     */
    pj_thread_sleep(DURATION);


    /*
     * Close stream.
     */
    test_data.running = 0;
    pjmedia_snd_stream_close(strm);


    /* 
     * Print results.
     */
    PJ_LOG(3,(THIS_FILE, " Dumping results:"));

    PJ_LOG(3,(THIS_FILE, "  Parameters: clock rate=%dHz, %d samples/frame",
	      clock_rate, samples_per_frame));

    if (dir & PJMEDIA_DIR_PLAYBACK)
	print_stream_data("Playback", &test_data, &test_data.playback_data, 
			  verbose);
    if (dir & PJMEDIA_DIR_CAPTURE)
	print_stream_data("Capture", &test_data, &test_data.capture_data, 
			   verbose);

    /* Check drifting */
    if (dir == PJMEDIA_DIR_CAPTURE_PLAYBACK) {
	int end_diff, start_diff, drift;

	end_diff = test_data.capture_data.last_timestamp -
		   test_data.playback_data.last_timestamp;
	start_diff = test_data.capture_data.first_timestamp-
		      test_data.playback_data.first_timestamp;
	drift = end_diff - start_diff;

	PJ_LOG(3,(THIS_FILE, "  Checking for clock drifts:"));

	/* Allow one frame tolerance for clock drift detection */
	if (drift < (int)samples_per_frame) {
	    PJ_LOG(3,(THIS_FILE, "   No clock drifts is detected"));
	} else {
	    const char *which = (drift<0 ? "slower" : "faster");
	    unsigned msec_dur;

	    if (drift < 0) drift = -drift;


	    msec_dur = (test_data.capture_data.last_timestamp - 
		       test_data.capture_data.first_timestamp) * 1000 /
		       test_data.clock_rate;

	    PJ_LOG(2,(THIS_FILE, 
		      "   Sound capture is %d samples %s than playback "
		      "at the end of the test (average is %d samples"
		      " per second)",
		      drift, which, 
		      drift * 1000 / msec_dur));

	}
    }

    if (test_data.has_error == 0) {
	PJ_LOG(3,(THIS_FILE, " Test completed, sound device looks okay."));
	return 0;
    } else {
	PJ_LOG(2,(THIS_FILE, " Test completed with some warnings"));
	return 1;
    }
}


int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    int id = -1, verbose = 0;
    int clock_rate = 8000;
    int frame = -1;
    int channel = 1;
    struct pj_getopt_option long_options[] = {
	{ "id",	     1, 0, 'i' },
	{ "rate",    1, 0, 'r' },
	{ "frame",   1, 0, 'f' },
	{ "channel", 1, 0, 'n' },
	{ "verbose", 0, 0, 'v' },
	{ "help",    0, 0, 'h' },
	{ NULL, 0, 0, 0 }
    };
    int c, option_index;
    

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

    /* Print devices */
    enum_devices();

    /* Parse options */
    pj_optind = 0;
    while((c=pj_getopt_long(argc,argv, "i:r:f:n:vh", 
			    long_options, &option_index))!=-1) 
    {
	switch (c) {
	case 'i':
	    id = atoi(pj_optarg);
	    break;
	case 'r':
	    clock_rate = atoi(pj_optarg);
	    break;
	case 'f':
	    frame = atoi(pj_optarg);
	    break;
	case 'n':
	    channel = atoi(pj_optarg);
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case 'h':
	    puts(desc);
	    return 0;
	    break;
	default:
	    printf("Error: invalid options %s\n", argv[pj_optind-1]);
	    puts(desc);
	    return 1;
	}
    }

    if (pj_optind != argc) {
	printf("Error: invalid options\n");
	puts(desc);
	return 1;
    }

    if (!verbose)
	pj_log_set_level(3);

    if (frame == -1)
	frame = 10 * clock_rate / 1000;


    status = perform_test(get_dev_name(id), id, PJMEDIA_DIR_CAPTURE_PLAYBACK, 
			  clock_rate, frame, channel, verbose);
    if (status != 0)
	return 1;

    
    return 0;
}



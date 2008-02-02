/* $Id: sndtest.c 1663 2008-01-04 18:00:11Z bennylp $ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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

#include <pjmedia.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia/jbuf2.h>

#include <stdlib.h>	/* atoi() */
#include <stdio.h>

#define THIS_FILE	    "jbtest.c"

struct test_data {
    unsigned clock_rate;
    unsigned samples_per_frame;
    pjmedia_jb2_t *jb;
    pjmedia_port *read_port;
    pjmedia_port *write_port;
    pj_lock_t *mutex;
    pj_int16_t seq;
    unsigned ts;
};



static const char *desc = 
 THIS_FILE "\n"
 "							\n"
 " PURPOSE:						\n"
 "  Test jbuf2 with real sound device.			\n"
 "							\n"
 " USAGE:						\n"
 "  sndtest --help					\n"
 "  sndtest [options]					\n"
 "							\n"
 " options:						\n"
 "  --id=ID          -i  Use device ID (default is -1)	\n"
 "  --rate=HZ        -r  Set test clock rate (default=8000)\n"
 "  --file=FILENAME  -l  Set input filename		\n"
 "  --drift=N	     -d  Set clock drift (-2000 < N < 2000)\n"
 "  --verbose        -v  Show verbose result		\n"
 "  --help           -h  Show this screen		\n"
;

static void app_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));	
    printf( "%s: %s (err=%d)\n",
	    title, errmsg, status);
}

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

/*
 * Callback to be called for each clock ticks.
 */
static void clock_callback(const pj_timestamp *ts, void *user_data)
{
    struct test_data *test_data = user_data;
    pjmedia_jb2_frame f;
    pjmedia_frame f_;
    pj_status_t status;
    char buf[1024];

    PJ_UNUSED_ARG(ts);

    PJ_ASSERT_ON_FAIL(test_data, return);

    f_.buf = buf;
    f_.size = test_data->samples_per_frame*2;
    status = pjmedia_port_get_frame(test_data->read_port, &f_);
    if (status != PJ_SUCCESS) {
	app_perror("Failed to get frame from file", status);
	return;
    }

    test_data->seq++;
    test_data->ts += test_data->samples_per_frame;

    f.buffer = f_.buf;
    f.pt = 0;
    f.seq = test_data->seq;
    f.ts = test_data->ts;
    f.size = f_.size;
    f.type = PJMEDIA_JB_FT_NORMAL_RAW_FRAME;


    pj_lock_acquire(test_data->mutex);
    pjmedia_jb2_put_frame(test_data->jb, &f);
    pj_lock_release(test_data->mutex);
}

static pj_status_t play_cb(void *user_data, pj_uint32_t timestamp,
			   void *output, unsigned size)
{
    struct test_data *test_data = user_data;
    pjmedia_jb2_frame f;
    pjmedia_frame f_;
    pj_status_t status;

    PJ_UNUSED_ARG(timestamp);
    PJ_ASSERT_RETURN(test_data, PJ_EINVAL);

    f.buffer = output;
    f.size = size;
    pj_lock_acquire(test_data->mutex);
    pjmedia_jb2_get_frame(test_data->jb, &f);
    pj_lock_release(test_data->mutex);

    f_.buf = f.buffer;
    f_.size = f.size;
    f_.type = PJMEDIA_FRAME_TYPE_AUDIO;
    f_.timestamp.u64 = f.ts;
    status = pjmedia_port_put_frame(test_data->write_port, &f_);
    if (status != PJ_SUCCESS) {
	app_perror("Failed to put frame to file", status);
	return status;
    }

    return PJ_SUCCESS;
}

static int perform_test(pj_pool_t *pool, const char *inputfile, int dev_id,
		        unsigned clock_rate, int drift)
{
    const int PTIME = 20;

    pj_status_t status = PJ_SUCCESS;
    pjmedia_snd_stream *strm;
    pjmedia_clock *clock;
    struct test_data test_data;
    pjmedia_snd_stream_info si;
    unsigned samples_per_frame;
    
    pjmedia_jb2_setting  jb_setting;
    pjmedia_jb2_cb	 jb_cb;
    pjmedia_jb2_state	 jb_state;
    pjmedia_jb2_stat	 jb_stat;
    char s[8];

    PJ_ASSERT_RETURN(pool && inputfile, PJ_EINVAL);

    samples_per_frame = PTIME * clock_rate / 1000;
    /*
     * Init test parameters
     */
    pj_bzero(&test_data, sizeof(test_data));
    test_data.clock_rate = clock_rate;
    test_data.samples_per_frame = samples_per_frame;

    jb_setting.max_frames = 0;
    jb_setting.samples_per_frame = samples_per_frame;
    jb_setting.frame_size = samples_per_frame * 2;

    pj_bzero(&jb_cb, sizeof(jb_cb));

    pj_lock_create_recursive_mutex(pool, "sndtest", &test_data.mutex); 

    /* Create WAV player port */
    status = pjmedia_wav_player_port_create(  pool,	/* memory pool	    */
					      inputfile,/* file to play	    */
					      PTIME,	/* ptime.	    */
					      0,	/* flags	    */
					      0,	/* default buffer   */
					      &test_data.read_port
							/* returned port    */
					      );
    if (status != PJ_SUCCESS) {
	app_perror("Unable to create WAV player", status);
	return 1;
    }

    PJ_TODO(wav_nchannel_validation);
    PJ_TODO(resample);

    /* Create WAV writer port */
    status = pjmedia_wav_writer_port_create(  pool, "jbtestout.wav",
					      clock_rate, 1,
					      samples_per_frame, 
					      16, 0, 0, 
					      &test_data.write_port);
    if (status != PJ_SUCCESS) {
	app_perror("Unable to create WAV writer", status);
	return 1;
    }

    /* Create jitter buffer */
    status = pjmedia_jb2_create(pool, NULL, &jb_setting, &jb_cb, &test_data.jb);
    if (status != PJ_SUCCESS) {
        app_perror("Unable to create jitter buffer", status);
        return status;
    }

    /* Create media clock */
    status = pjmedia_clock_create(pool, clock_rate + drift, samples_per_frame, 
				  0, &clock_callback, &test_data, &clock);

    if (status != PJ_SUCCESS) {
        app_perror("Unable to create clock", status);
        return status;
    }

    /*
     * Open device.
     */
    status = pjmedia_snd_open_player( dev_id, clock_rate, 1,
				      samples_per_frame, 16, &play_cb, 
				      &test_data, &strm);
    
    if (status != PJ_SUCCESS) {
        app_perror("Unable to open device for playing", status);
        return status;
    }

    pjmedia_snd_stream_get_info(strm, &si);
    if (si.play_id >= 0) {
	PJ_LOG(3,(THIS_FILE, "Testing playback device %s", 
		  pjmedia_snd_get_dev_info(si.play_id)->name));
    }

    /*
     * Start the stream.
     */
    status = pjmedia_snd_stream_start(strm);
    if (status != PJ_SUCCESS) {
        app_perror("Unable to start capture stream", status);
        return status;
    }

    /*
     * Start the clock.
     */
    status = pjmedia_clock_start(clock);
    if (status != PJ_SUCCESS) {
        app_perror("Unable to start clock", status);
        return status;
    }

    /* Begin gather data */

    puts("\nTest is running...");
    puts("\nPress <ENTER> to quit");
    fgets(s, sizeof(s), stdin);

    /* 
     * Print results.
     */
    pjmedia_jb2_get_state(test_data.jb, &jb_state);
    printf("\nStop condition:\n"); 
    printf("drift:%3d/%d ", jb_state.drift, jb_state.drift_span); 
    printf("level:%3d ", jb_state.level); 
    printf("cur_size:%5d ", jb_state.cur_size); 
    printf("opt_size:%5d ", jb_state.opt_size); 
    printf("frame_cnt:%d ", jb_state.frame_cnt); 
    printf("\n"); 

    pjmedia_jb2_get_stat(test_data.jb, &jb_stat);
    printf("lost\t\t = %d\n", jb_stat.lost); 
    printf("ooo\t\t = %d\n", jb_stat.ooo); 
    printf("full\t\t = %d\n", jb_stat.full); 
    printf("empty\t\t = %d\n", jb_stat.empty); 
    printf("out\t\t = %d\n", jb_stat.out); 
    printf("in\t\t = %d\n", jb_stat.in); 
    printf("max_size\t = %d\n", jb_stat.max_size); 
    printf("max_comp\t = %d\n", jb_stat.max_comp); 
    printf("max_drift\t = %d/%d\n", jb_stat.max_drift, 
				    jb_stat.max_drift_span); 

    /* Close stream */
    pjmedia_snd_stream_close(strm);

    /* Destroy clock */
    pjmedia_clock_destroy(clock);

    /* Destroy file port */
    pjmedia_port_destroy(test_data.read_port);
    pjmedia_port_destroy(test_data.write_port);

    /* Destroy jitter buffer */
    pjmedia_jb2_destroy(test_data.jb);

    return PJ_SUCCESS;
}


int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pj_pool_t *pool;
    pjmedia_endpt *med_endpt;
    int id = -1, verbose = 0;
    int clock_rate = 8000;
    char *inputfile = NULL;
    int drift = 0;
    struct pj_getopt_option long_options[] = {
	{ "id",	     1, 0, 'i' },
	{ "rate",    1, 0, 'r' },
	{ "file",    1, 0, 'l' },
	{ "drift",   1, 0, 'd' },
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

    /* Also create pool for misc purposes */
    pool = pj_pool_create(&cp.factory, "jbuf2test", 1000, 1000, NULL);

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
	case 'd':
	    drift = atoi(pj_optarg);
	    break;
	case 'l':
	    inputfile = pj_optarg;
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
    else
	pj_log_set_level(4);


    status = perform_test(pool, inputfile, id, clock_rate, drift);

    pjmedia_endpt_destroy(med_endpt);
    pj_pool_release(pool);
    pj_caching_pool_destroy(&cp);
    pj_shutdown();
    
    return status == PJ_SUCCESS ? 0 : 1;
}




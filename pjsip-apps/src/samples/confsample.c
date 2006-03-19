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

#include <pjmedia.h>

/*
 * FILE:
 *
 *  confsample.c
 *
 * PURPOSE:
 *
 *  Demonstrate how to use conference bridge.
 *
 * USAGE:
 *
 *  confsample [file1.wav] [file2.wav] ...
 *
 * where:
 *  fileN.wav are optional WAV files to be connected to the conference
 *  bridge. The WAV files MUST have single channel (mono) and 16 bit PCM
 *  samples. It can have arbitrary sampling rate.
 *
 * DESCRIPTION:
 *
 *  Here we create a conference bridge, with at least two ports:
 *  - port zero is for the sound device (capture and play), 
 *  - port one is to play a generated sine wave.
 *
 *  If WAV files are specified, the WAV file player ports will be connected
 *  to slot starting from number two in the bridge. The WAV files can have 
 *  arbitrary sampling rate; the bridge will convert the samples to its clock
 *  rate. However, the files MUST have a single audio channel only (i.e. mono).
 */

#include <pjmedia.h>
#include <pjlib.h>

#include <stdlib.h>	/* atoi() */
#include <stdio.h>


/* For logging purpose. */
#define THIS_FILE   "confsample.c"

/* 
 * Prototypes: 
 */

/* List the ports in the conference bridge */
static void conf_list(pjmedia_conf *conf, pj_bool_t detail);

/* Display VU meter */
static void monitor_level(pjmedia_conf *conf, int slot, int dir, int dur);


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


/* This callback is called to feed sine wave samples from the sine 
 * generator. 
 */
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
        sine->samples[i] = (pj_int16_t) (8000.0 * 
		sin(((double)i/(double)count) * M_PI * 8.) );
    }

    *p_port = port;

    return PJ_SUCCESS;
}


/* Show usage */
static void usage(void)
{
    puts("");
    puts("Usage: confsample [file1.wav] [file2.wav] ...");
    puts("");
    puts("where:");
    puts("  fileN.WAV are optional WAV files to be connected to the conference");
    puts("  bridge. The WAV files MUST have single channel (mono) and 16 bit PCM");
    puts("  samples. They can have arbitrary/different sampling rate.");
}



/* Input simple string */
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


/*****************************************************************************
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_conf *conf;
    pjmedia_port *sine_port;

    int i, file_count;
    pjmedia_port **file_port;	/* Array of file ports */

    char tmp[10];
    pj_status_t status;


    /* Just in case user needs help */
    if (argc > 1 && (*argv[1]=='-' || *argv[1]=='/' || *argv[1]=='?')) {
	usage();
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
    status = pjmedia_endpt_create(&cp.factory, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool to allocate memory */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );


    /* Create the conference bridge. 
     * With default options (zero), the bridge will create an instance of
     * sound capture and playback device and connect them to slot zero.
     */
    status = pjmedia_conf_create( pool,	    /* pool to use	    */
				  2+argc-1, /* number of ports	    */
				  16000,    /* sampling rate	    */
				  1,	    /* # of channels.	    */
				  320,	    /* samples per frame    */
				  16,	    /* bits per sample	    */
				  0,	    /* options		    */
				  &conf	    /* result		    */
				  );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create conference bridge", status);
	return 1;
    }


    /* Create a media port to generate sine wave samples. */
    status = create_sine_port( pool,	    /* memory pool	    */
			       11025,	    /* sampling rate	    */
			       1,	    /* # of channels	    */
			       &sine_port   /* returned port	    */
		             );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Add the sine generator port to the conference bridge. */
    status = pjmedia_conf_add_port(conf,	/* the conference bridge    */
				   pool,	/* pool			    */
				   sine_port,	/* the sine generator	    */
				   NULL,	/* use port's name	    */
				   NULL		/* ptr to receive slot #    */
				   );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);



    /* Create file ports. */
    file_count = argc-1;
    file_port = pj_pool_alloc(pool, file_count * sizeof(pjmedia_port*));

    for (i=0; i<file_count; ++i) {

	/* Load the WAV file to file port. */
	status = pjmedia_file_player_port_create( pool,	    /* pool.	    */
						  argv[i+1],/* filename	    */
						  0,	    /* flags	    */
						  0,	    /* buf size	    */
						  NULL,	    /* user data    */
						  &file_port[i]	/* result   */
						  );
	if (status != PJ_SUCCESS) {
	    char title[80];
	    pj_ansi_sprintf(title, "Unable to use %s", argv[i+1]);
	    app_perror(THIS_FILE, title, status);
	    usage();
	    return 1;
	}

	/* Add the file port to conference bridge */
	status = pjmedia_conf_add_port( conf,		/* The bridge	    */
					pool,		/* pool		    */
					file_port[i],	/* port to connect  */
					NULL,		/* Use port's name  */
					NULL		/* ptr to receive slot # */
					);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to add conference port", status);
	    return 1;
	}
    }


    /* 
     * All ports are set up in the conference bridge.
     * But at this point, no media will be flowing since no ports are
     * "connected". User must connect the port manually.
     */


    /* Sleep to allow log messages to flush */
    pj_thread_sleep(100);


    /*
     * UI Menu: 
     */
    for (;;) {
	char tmp1[10];
	char tmp2[10];
	char *err;
	int src, dst, level;

	puts("");
	conf_list(conf, 0);
	puts("");
	puts("Menu:");
	puts("  s    Show ports details");
	puts("  c    Connect one port to another");
	puts("  d    Disconnect port connection");
	puts("  t    Adjust signal level transmitted (tx) to a port");
	puts("  r    Adjust signal level received (rx) from a port");
	puts("  v    Display VU meter for a particular port");
	puts("  q    Quit");
	puts("");
	
	printf("Enter selection: "); fflush(stdout);

	fgets(tmp, sizeof(tmp), stdin);

	switch (tmp[0]) {
	case 's':
	    puts("");
	    conf_list(conf, 1);
	    break;

	case 'c':
	    puts("");
	    puts("Connect source port to destination port");
	    if (!simple_input("Enter source port number", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src > file_count+2) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!simple_input("Enter destination port number", tmp2, sizeof(tmp2)) )
		continue;
	    dst = strtol(tmp2, &err, 10);
	    if (*err || dst < 0 || dst >= file_count+2) {
		puts("Invalid slot number");
		continue;
	    }

	    status = pjmedia_conf_connect_port(conf, src, dst, 0);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error connecting port", status);
	    
	    break;

	case 'd':
	    puts("");
	    puts("Disconnect port connection");
	    if (!simple_input("Enter source port number", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src > file_count+2) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!simple_input("Enter destination port number", tmp2, sizeof(tmp2)) )
		continue;
	    dst = strtol(tmp2, &err, 10);
	    if (*err || dst < 0 || dst >= file_count+2) {
		puts("Invalid slot number");
		continue;
	    }

	    status = pjmedia_conf_disconnect_port(conf, src, dst);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error connecting port", status);
	    

	    break;

	case 't':
	    puts("");
	    puts("Adjust transmit level of a port");
	    if (!simple_input("Enter port number", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src > file_count+2) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!simple_input("Enter level (-128 to +127, 0 for normal)", 
			      tmp2, sizeof(tmp2)) )
		continue;
	    level = strtol(tmp2, &err, 10);
	    if (*err || level < -128 || level > 127) {
		puts("Invalid level");
		continue;
	    }

	    status = pjmedia_conf_adjust_tx_level( conf, src, level);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error adjusting level", status);
	    break;


	case 'r':
	    puts("");
	    puts("Adjust receive level of a port");
	    if (!simple_input("Enter port number", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src > file_count+2) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!simple_input("Enter level (-128 to +127, 0 for normal)", 
			      tmp2, sizeof(tmp2)) )
		continue;
	    level = strtol(tmp2, &err, 10);
	    if (*err || level < -128 || level > 127) {
		puts("Invalid level");
		continue;
	    }

	    status = pjmedia_conf_adjust_rx_level( conf, src, level);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error adjusting level", status);
	    break;

	case 'v':
	    puts("");
	    puts("Display VU meter");
	    if (!simple_input("Enter port number to monitor", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src > file_count+2) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!simple_input("Enter r for rx level or t for tx level", tmp2, sizeof(tmp2)))
		continue;
	    if (tmp2[0] != 'r' && tmp2[0] != 't') {
		puts("Invalid option");
		continue;
	    }

	    if (!simple_input("Duration to monitor (in seconds)", tmp1, sizeof(tmp1)) )
		continue;
	    strtol(tmp1, &err, 10);
	    if (*err) {
		puts("Invalid duration number");
		continue;
	    }

	    monitor_level(conf, src, tmp2[0], strtol(tmp1, &err, 10));
	    break;

	case 'q':
	    goto on_quit;

	default:
	    printf("Invalid input character '%c'\n", tmp[0]);
	    break;
	}
    }

on_quit:
    
    /* Start deinitialization: */

    /* Destroy conference bridge */
    status = pjmedia_conf_destroy( conf );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Destroy file ports */
    for (i=0; i<file_count; ++i) {
	status = pjmedia_port_destroy( file_port[i]);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Destroy sine generator port */
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


/*
 * List the ports in conference bridge
 */
static void conf_list(pjmedia_conf *conf, int detail)
{
    enum { MAX_PORTS = 32 };
    unsigned i, count;
    pjmedia_conf_port_info info[MAX_PORTS];

    printf("Conference ports:\n");

    count = PJ_ARRAY_SIZE(info);
    pjmedia_conf_get_ports_info(conf, &count, info);

    for (i=0; i<count; ++i) {
	char txlist[4*MAX_PORTS];
	unsigned j;
	pjmedia_conf_port_info *port_info = &info[i];	
	
	txlist[0] = '\0';
	for (j=0; j<count; ++j) {
	    char s[10];
	    if (port_info->listener[j]) {
		pj_ansi_sprintf(s, "#%d ", j);
		pj_ansi_strcat(txlist, s);
	    }
	}

	if (txlist[0] == '\0') {
	    txlist[0] = '-';
	    txlist[1] = '\0';
	}

	if (!detail) {
	    printf("Port #%02d %-25.*s  transmitting to: %s\n", 
		   port_info->slot, 
		   (int)port_info->name.slen, 
		   port_info->name.ptr,
		   txlist);
	} else {
	    unsigned tx_level, rx_level;

	    pjmedia_conf_get_signal_level(conf, port_info->slot,
					  &tx_level, &rx_level);

	    printf("Port #%02d:\n"
		   "  Name                    : %.*s\n"
		   "  Sampling rate           : %d Hz\n"
		   "  Frame time              : %d ms\n"
		   "  Signal level adjustment : tx=%d, rx=%d\n"
		   "  Current signal level    : tx=%u, rx=%u\n"
		   "  Transmitting to ports   : %s\n\n",
		   port_info->slot,
		   (int)port_info->name.slen,
		   port_info->name.ptr,
		   port_info->clock_rate,
		   port_info->samples_per_frame*1000/port_info->clock_rate,
		   port_info->tx_adj_level,
		   port_info->rx_adj_level,
		   tx_level,
		   rx_level,
		   txlist);
	}

    }
    puts("");
}


/*
 * Display VU meter
 */
static void monitor_level(pjmedia_conf *conf, int slot, int dir, int dur)
{
    enum { SLEEP = 100};
    pj_status_t status;
    int i, count;

    puts("");
    printf("Displaying VU meter for port %d for about %d seconds\n",
	   slot, dur);

    count = dur * 1000 / SLEEP;

    for (i=0; i<count; ++i) {
	unsigned tx_level, rx_level;
	int j, level;
	char meter[21];

	status = pjmedia_conf_get_signal_level(conf, slot, 
					       &tx_level, &rx_level);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to read level", status);
	    return;
	}

	level = 20 * (dir=='r'?rx_level:tx_level) / 255;
	for (j=0; j<level; ++j)
	    meter[j] = '#';
	for (; j<20; ++j)
	    meter[j] = ' ';
	meter[20] = '\0';

	printf("Port #%02d %cx level: [%s]\r",
	       slot, dir, meter);

	pj_thread_sleep(SLEEP);
    }

    puts("");
}


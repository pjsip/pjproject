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
 "  sndinfo [id r/p clockrate nchan bits]				    \n"
 "									    \n"
 " DESCRIPTION:								    \n"
 "  When invoked without any arguments, it displays information about all   \n"
 "  sound devices in the system.					    \n"
 "									    \n"
 "  When invoked with arguments, the program tests if device can be opened  \n"
 "  with the specified arguments. All these arguments must be specified:    \n"
 "  - id         The device ID (-1 for the first capable device)	    \n"
 "  - r/p        Specify r for recording/capture, p for playing.	    \n"
 "  - clockrate  Specify clock rate (e.g. 8000, 11025, etc.)		    \n"
 "  - nchan      Number of channels (1=mono, 2=stereo).			    \n"
 "  - bits       Number of bits per sample (normally 16).		    \n";

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
}


static pj_status_t play_cb(void *user_data, pj_uint32_t timestamp,
			   void *output, unsigned size)
{
    return PJ_SUCCESS;
}

static pj_status_t rec_cb(void *user_data, pj_uint32_t timestamp,
			  const void *input, unsigned size)
{
    return PJ_SUCCESS;
}

static int open_device(int dev_id, int capturing, int clock_rate, 
		       int nchannel, int bits)
{
    pj_status_t status;
    unsigned nsamples;
    pjmedia_snd_stream *strm;

    nsamples = clock_rate * 20 / 1000;

    printf( "Opening device %d: clockrate=%d, nchannel=%d, bits=%d, "
	    "nsamples=%d..\n",
	    dev_id, clock_rate, nchannel, bits, nsamples);

    if (capturing) {
	status = pjmedia_snd_open_recorder( dev_id, clock_rate, nchannel,
					    nsamples, bits, &rec_cb, NULL,
					    &strm);
    } else {
	status = pjmedia_snd_open_player( dev_id, clock_rate, nchannel,
					  nsamples, bits, &play_cb, NULL,
					  &strm);
    }
    
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];

	pj_strerror(status, errmsg, sizeof(errmsg));	
	printf( "Error: unable to open device %d for %s: %s (err=%d)\n",
		dev_id, (capturing ? "capture" : "playback"),
		errmsg, status);
	return 1;
	
    } else {
	puts("Device opened successfully");
    }

    pjmedia_snd_stream_close(strm);
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
	int capturing;
	int clock_rate;
	int nchannel;
	int bits;

	dev_id = atoi(argv[1]);
	capturing = (strcmp(argv[2], "r") == 0);
	clock_rate = atoi(argv[3]);
	nchannel = atoi(argv[4]);
	bits = atoi(argv[5]);

	return open_device(dev_id, capturing, clock_rate, nchannel, bits);

    } else {
	puts("Error: invalid arguments");
	puts(desc);
	return 1;
    }

    return 0;
}



/* $Id$ */
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
#include <pjmedia/sound.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>

#if PJMEDIA_SOUND_IMPLEMENTATION==PJMEDIA_SOUND_NULL_SOUND

static pjmedia_snd_dev_info null_info = 
{
    "Null Device",
    1,
    1,
    8000
};

static pj_pool_factory *pool_factory;

struct pjmedia_snd_stream 
{
	pj_pool_t		*pool;
	pjmedia_dir 		dir;
	int 			rec_id;
	int 			play_id;
	unsigned		clock_rate;
	unsigned		channel_count;
	unsigned		samples_per_frame;
	unsigned		bits_per_sample;
	pjmedia_snd_rec_cb	rec_cb;
	pjmedia_snd_play_cb	play_cb;
	void			*user_data;
};


PJ_DEF(pj_status_t) pjmedia_snd_init(pj_pool_factory *factory)
{
    pool_factory = factory;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_snd_deinit(void)
{
    return PJ_SUCCESS;
}

PJ_DEF(int) pjmedia_snd_get_dev_count(void)
{
    return 1;
}

PJ_DEF(const pjmedia_snd_dev_info*) pjmedia_snd_get_dev_info(unsigned index)
{
    PJ_ASSERT_RETURN(index==0 || index==(unsigned)-1, NULL);
    return &null_info;
}

PJ_DEF(pj_status_t) pjmedia_snd_open_rec( int index,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned bits_per_sample,
					  pjmedia_snd_rec_cb rec_cb,
					  void *user_data,
					  pjmedia_snd_stream **p_snd_strm)
{
    return pjmedia_snd_open(index, -2, clock_rate, channel_count,
    			    samples_per_frame, bits_per_sample,
    			    rec_cb, NULL, user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open_player( int index,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned bits_per_sample,
					pjmedia_snd_play_cb play_cb,
					void *user_data,
					pjmedia_snd_stream **p_snd_strm )
{
    return pjmedia_snd_open(-2, index, clock_rate, channel_count,
    			    samples_per_frame, bits_per_sample,
    			    NULL, play_cb, user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open( int rec_id,
				      int play_id,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned bits_per_sample,
				      pjmedia_snd_rec_cb rec_cb,
				      pjmedia_snd_play_cb play_cb,
				      void *user_data,
				      pjmedia_snd_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_snd_stream *snd_strm;

    pool = pj_pool_create(pool_factory, NULL, 128, 128, NULL);
    snd_strm = PJ_POOL_ZALLOC_T(pool, pjmedia_snd_stream);
    
    snd_strm->pool = pool;
    
    if (rec_id == -1) rec_id = 0;
    if (play_id == -1) play_id = 0;
    
    if (rec_id != -2 && play_id != -2)
    	snd_strm->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    else if (rec_id != -2)
    	snd_strm->dir = PJMEDIA_DIR_CAPTURE;
    else if (play_id != -2)
    	snd_strm->dir = PJMEDIA_DIR_PLAYBACK;
    
    snd_strm->rec_id = rec_id;
    snd_strm->play_id = play_id;
    snd_strm->clock_rate = clock_rate;
    snd_strm->channel_count = channel_count;
    snd_strm->samples_per_frame = samples_per_frame;
    snd_strm->bits_per_sample = bits_per_sample;
    snd_strm->rec_cb = rec_cb;
    snd_strm->play_cb = play_cb;
    snd_strm->user_data = user_data;
    
    *p_snd_strm = snd_strm;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_stream_start(pjmedia_snd_stream *stream)
{
    PJ_UNUSED_ARG(stream);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_snd_stream_stop(pjmedia_snd_stream *stream)
{
    PJ_UNUSED_ARG(stream);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_snd_stream_get_info(pjmedia_snd_stream *strm,
						pjmedia_snd_stream_info *pi)
{

    pj_bzero(pi, sizeof(pjmedia_snd_stream_info));
    pi->dir = strm->dir;
    pi->play_id = strm->play_id;
    pi->rec_id = strm->rec_id;
    pi->clock_rate = strm->clock_rate;
    pi->channel_count = strm->channel_count;
    pi->samples_per_frame = strm->samples_per_frame;
    pi->bits_per_sample = strm->bits_per_sample;
    pi->rec_latency = 0;
    pi->play_latency = 0;
    
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_stream_close(pjmedia_snd_stream *stream)
{
    pj_pool_release(stream->pool);
    return PJ_SUCCESS;
}

/*
 * Set sound latency.
 */
PJ_DEF(pj_status_t) pjmedia_snd_set_latency(unsigned input_latency, 
					    unsigned output_latency)
{
    /* Nothing to do */
    PJ_UNUSED_ARG(input_latency);
    PJ_UNUSED_ARG(output_latency);
    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_SOUND_IMPLEMENTATION */

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
#include <pjmedia/sound.h>
#include <pj/assert.h>

#if PJMEDIA_SOUND_IMPLEMENTATION==PJMEDIA_SOUND_NULL_SOUND

static pjmedia_snd_dev_info null_info = 
{
    "Null Device",
    1,
    1,
    8000
};


PJ_DEF(pj_status_t) pjmedia_snd_init(pj_pool_factory *factory)
{
    PJ_UNUSED_ARG(factory);
    return PJ_SUCCESS;
}

PJ_DEF(int) pjmedia_snd_get_dev_count(void)
{
    return 1;
}

PJ_DEF(const pjmedia_snd_dev_info*) pjmedia_snd_get_dev_info(unsigned index)
{
    PJ_ASSERT_RETURN(index==0, NULL);
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
    PJ_UNUSED_ARG(index);
    PJ_UNUSED_ARG(clock_rate);
    PJ_UNUSED_ARG(channel_count);
    PJ_UNUSED_ARG(samples_per_frame);
    PJ_UNUSED_ARG(bits_per_sample);
    PJ_UNUSED_ARG(rec_cb);
    PJ_UNUSED_ARG(user_data);

    *p_snd_strm = (void*)1;

    return PJ_SUCCESS;
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
    PJ_UNUSED_ARG(index);
    PJ_UNUSED_ARG(clock_rate);
    PJ_UNUSED_ARG(channel_count);
    PJ_UNUSED_ARG(samples_per_frame);
    PJ_UNUSED_ARG(bits_per_sample);
    PJ_UNUSED_ARG(play_cb);
    PJ_UNUSED_ARG(user_data);

    *p_snd_strm = (void*)1;

    return PJ_SUCCESS;
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
    PJ_UNUSED_ARG(rec_id);
    PJ_UNUSED_ARG(play_id);
    PJ_UNUSED_ARG(clock_rate);
    PJ_UNUSED_ARG(channel_count);
    PJ_UNUSED_ARG(samples_per_frame);
    PJ_UNUSED_ARG(bits_per_sample);
    PJ_UNUSED_ARG(rec_cb);
    PJ_UNUSED_ARG(play_cb);
    PJ_UNUSED_ARG(user_data);

    *p_snd_strm = (void*)1;

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

PJ_DEF(pj_status_t) pjmedia_snd_stream_close(pjmedia_snd_stream *stream)
{
    PJ_UNUSED_ARG(stream);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_snd_deinit(void)
{
    return PJ_SUCCESS;
}


#endif	/* PJMEDIA_SOUND_IMPLEMENTATION */

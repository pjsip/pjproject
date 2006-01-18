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

static pj_snd_dev_info null_info = 
{
    "Null Device",
    1,
    1,
    8000
};


PJ_DEF(pj_status_t) pj_snd_init(pj_pool_factory *factory)
{
    PJ_UNUSED_ARG(factory);
    return PJ_SUCCESS;
}

PJ_DEF(int) pj_snd_get_dev_count(void)
{
    return 1;
}

PJ_DEF(const pj_snd_dev_info*) pj_snd_get_dev_info(unsigned index)
{
    PJ_ASSERT_RETURN(index==0, NULL);
    return &null_info;
}

PJ_DEF(pj_snd_stream*) pj_snd_open_recorder( int index,
					     const pj_snd_stream_info *param,
					     pj_snd_rec_cb rec_cb,
					     void *user_data)
{
    PJ_UNUSED_ARG(index);
    PJ_UNUSED_ARG(param);
    PJ_UNUSED_ARG(rec_cb);
    PJ_UNUSED_ARG(user_data);
    return (void*)1;
}

PJ_DEF(pj_snd_stream*) pj_snd_open_player( int index,
					   const pj_snd_stream_info *param,
					   pj_snd_play_cb play_cb,
					   void *user_data)
{
    PJ_UNUSED_ARG(index);
    PJ_UNUSED_ARG(param);
    PJ_UNUSED_ARG(play_cb);
    PJ_UNUSED_ARG(user_data);
    return (void*)1;
}

PJ_DEF(pj_status_t) pj_snd_stream_start(pj_snd_stream *stream)
{
    PJ_UNUSED_ARG(stream);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_snd_stream_stop(pj_snd_stream *stream)
{
    PJ_UNUSED_ARG(stream);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_snd_stream_close(pj_snd_stream *stream)
{
    PJ_UNUSED_ARG(stream);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_snd_deinit(void)
{
    return PJ_SUCCESS;
}

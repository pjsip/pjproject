/* $Id$ */
/* 
 * Copyright (C) 2008-2010 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_AVI_STREAM_H__
#define __PJMEDIA_AVI_STREAM_H__

/**
 * @file avi_stream.h
 * @brief AVI file player.
 */
#include <pjmedia/port.h>



PJ_BEGIN_DECL


/**
 * @defgroup PJMEDIA_FILE_PLAY AVI File Player
 * @ingroup PJMEDIA_PORT
 * @brief Video and audio playback from AVI file
 * @{
 */

/**
 * AVI file player options.
 */
enum pjmedia_avi_file_player_option
{
    /**
     * Tell the file player to return NULL frame when the whole
     * file has been played.
     */
    PJMEDIA_AVI_FILE_NO_LOOP = 1
};

typedef pjmedia_port pjmedia_avi_stream;

typedef struct pjmedia_avi_streams pjmedia_avi_streams;

/**
 * Create avi streams to play an AVI file. AVI player supports 
 * reading AVI file with uncompressed video format and 
 * 16 bit PCM or compressed G.711 A-law/U-law audio format.
 *
 * @param pool		Pool to create the streams.
 * @param filename	File name to open.
 * @param flags		Avi streams creation flags.
 * @param p_streams	Pointer to receive the avi streams instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_avi_player_create_streams(pj_pool_t *pool,
                                  const char *filename,
                                  unsigned flags,
                                  pjmedia_avi_streams **p_streams);

PJ_DECL(pj_uint8_t)
pjmedia_avi_streams_get_num_streams(pjmedia_avi_streams *streams);

PJ_DECL(pjmedia_avi_stream *)
pjmedia_avi_streams_get_stream(pjmedia_avi_streams *streams,
                               pj_uint8_t idx);

PJ_DECL(pjmedia_avi_stream *)
pjmedia_avi_streams_get_stream_by_media(pjmedia_avi_streams *streams,
                                        pj_uint8_t start_idx,
                                        pjmedia_type media_type);

PJ_INLINE(pjmedia_port *)
pjmedia_avi_stream_get_port(pjmedia_avi_stream *stream)
{
    return (pjmedia_port *)stream;
}

/**
 * Get the data length, in bytes.
 *
 * @param port		The AVI stream.
 *
 * @return		The length of the data, in bytes. Upon error it will
 *			return negative value.
 */
PJ_DECL(pj_ssize_t) pjmedia_avi_stream_get_len(pjmedia_avi_stream *stream);


/**
 * Register a callback to be called when the file reading has reached the
 * end of file. If the file is set to play repeatedly, then the callback
 * will be called multiple times. Note that only one callback can be 
 * registered for each AVI stream.
 *
 * @param port		The AVI stream.
 * @param user_data	User data to be specified in the callback
 * @param cb		Callback to be called. If the callback returns non-
 *			PJ_SUCCESS, the playback will stop. Note that if
 *			application destroys the file port in the callback,
 *			it must return non-PJ_SUCCESS here.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_avi_stream_set_eof_cb(pjmedia_avi_stream *stream,
			      void *user_data,
			      pj_status_t (*cb)(pjmedia_avi_stream *stream,
					        void *usr_data));

/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJMEDIA_AVI_STREAM_H__ */

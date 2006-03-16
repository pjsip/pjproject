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
#ifndef __PJMEDIA_SOUND_H__
#define __PJMEDIA_SOUND_H__


/**
 * @file sound.h
 * @brief Sound player and recorder device framework.
 */
#include <pjmedia/types.h>
#include <pj/pool.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJMED_SND Sound device abstraction.
 * @ingroup PJMEDIA
 * @{
 */

/** Opaque data type for audio stream. */
typedef struct pj_snd_stream pj_snd_stream;

/**
 * Device information structure returned by #pj_snd_get_dev_info.
 */
typedef struct pj_snd_dev_info
{
    char	name[64];	        /**< Device name.		    */
    unsigned	input_count;	        /**< Max number of input channels.  */
    unsigned	output_count;	        /**< Max number of output channels. */
    unsigned	default_samples_per_sec;/**< Default sampling rate.	    */
} pj_snd_dev_info;

/** 
 * This callback is called by player stream when it needs additional data
 * to be played by the device. Application must fill in the whole of output 
 * buffer with sound samples.
 *
 * @param user_data	User data associated with the stream.
 * @param timestamp	Timestamp, in samples.
 * @param output	Buffer to be filled out by application.
 * @param size		The size requested in bytes, which will be equal to
 *			the size of one whole packet.
 *
 * @return		Non-zero to stop the stream.
 */
typedef pj_status_t (*pj_snd_play_cb)(/* in */   void *user_data,
				      /* in */   pj_uint32_t timestamp,
				      /* out */  void *output,
				      /* out */  unsigned size);

/**
 * This callback is called by recorder stream when it has captured the whole
 * packet worth of audio samples.
 *
 * @param user_data	User data associated with the stream.
 * @param timestamp	Timestamp, in samples.
 * @param output	Buffer containing the captured audio samples.
 * @param size		The size of the data in the buffer, in bytes.
 *
 * @return		Non-zero to stop the stream.
 */
typedef pj_status_t (*pj_snd_rec_cb)(/* in */   void *user_data,
				     /* in */   pj_uint32_t timestamp,
				     /* in */   const void *input,
				     /* in*/    unsigned size);

/**
 * Init the sound library.
 *
 * @param factory	The sound factory.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_snd_init(pj_pool_factory *factory);


/**
 * Get the number of devices detected by the library.
 *
 * @return		Number of devices.
 */
PJ_DECL(int) pj_snd_get_dev_count(void);


/**
 * Get device info.
 *
 * @param index		The index of the device, which should be in the range
 *			from zero to #pj_snd_get_dev_count - 1.
 */
PJ_DECL(const pj_snd_dev_info*) pj_snd_get_dev_info(unsigned index);


/**
 * Create a new audio stream for audio capture purpose.
 *
 * @param index		Device index, or -1 to let the library choose the first
 *			available device, or -2 to use NULL device.
 * @param param		Stream parameters.
 * @param rec_cb	Callback to handle captured audio samples.
 * @param user_data	User data to be associated with the stream.
 *
 * @return		Audio stream, or NULL if failed.
 */
PJ_DECL(pj_status_t) pj_snd_open_recorder( int index,
					   unsigned clock_rate,
					   unsigned channel_count,
					   unsigned samples_per_frame,
					   unsigned bits_per_sample,
					   pj_snd_rec_cb rec_cb,
					   void *user_data,
					   pj_snd_stream **p_snd_strm);

/**
 * Create a new audio stream for playing audio samples.
 *
 * @param index		Device index, or -1 to let the library choose the first
 *			available device, or -2 to use NULL device.
 * @param param		Stream parameters.
 * @param play_cb	Callback to supply audio samples.
 * @param user_data	User data to be associated with the stream.
 *
 * @return		Audio stream, or NULL if failed.
 */
PJ_DECL(pj_status_t) pj_snd_open_player( int index,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned bits_per_sample,
					 pj_snd_play_cb play_cb,
					 void *user_data,
					 pj_snd_stream **p_snd_strm );

/**
 * Start the stream.
 *
 * @param stream	The recorder or player stream.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_snd_stream_start(pj_snd_stream *stream);

/**
 * Stop the stream.
 *
 * @param stream	The recorder or player stream.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_snd_stream_stop(pj_snd_stream *stream);

/**
 * Destroy the stream.
 *
 * @param stream	The recorder of player stream.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_snd_stream_close(pj_snd_stream *stream);

/**
 * Deinitialize sound library.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pj_snd_deinit(void);



/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_SOUND_H__ */

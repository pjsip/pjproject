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
#ifndef __PJMEDIA_WAV_PORT_H__
#define __PJMEDIA_WAV_PORT_H__

/**
 * @file wav_port.h
 * @brief WAV file player and writer.
 */
#include <pjmedia/port.h>



PJ_BEGIN_DECL


/**
 * Create a media port to play streams from a WAV file.
 *
 * @param pool		Pool to create memory buffers for this port.
 * @param filename	File name to open.
 * @param ptime		The duration (in miliseconds) of each frame read
 *			from this port. If the value is zero, the default
 *			duration (20ms) will be used.
 * @param flags		Port creation flags.
 * @param buf_size	Buffer size to be allocated. If the value is zero or
 *			negative, the port will use default buffer size (which
 *			is about 4KB).
 * @param user_data	User data to be associated with the file player port.
 * @param p_port	Pointer to receive the file port instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_wav_player_port_create( pj_pool_t *pool,
						     const char *filename,
						     unsigned ptime,
						     unsigned flags,
						     pj_ssize_t buff_size,
						     void *user_data,
						     pjmedia_port **p_port );


/**
 * Set the play position of WAV player.
 *
 * @param port		The file player port.
 * @param samples	Sample position (zero as start of file).
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_wav_player_port_set_pos( pjmedia_port *port,
						      pj_uint32_t samples );


/**
 * Create a media port to record streams to a WAV file. Note that the port
 * must be closed properly (with #pjmedia_port_destroy()) so that the WAV
 * header can be filled with correct values (such as the file length).
 *
 * @param pool		Pool to create memory buffers for this port.
 * @param filename	File name.
 * @param clock_rate	The sampling rate.
 * @param channel_count	Number of channels.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sampe Number of bits per sample (eg 16).
 * @param flags		Port creation flags (must be 0 at present).
 * @param buf_size	Buffer size to be allocated. If the value is zero or
 *			negative, the port will use default buffer size (which
 *			is about 4KB).
 * @param user_data	User data to be associated with the file writer port.
 * @param p_port	Pointer to receive the file port instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_wav_writer_port_create(pj_pool_t *pool,
						    const char *filename,
						    unsigned clock_rate,
						    unsigned channel_count,
						    unsigned samples_per_frame,
						    unsigned bits_per_sample,
						    unsigned flags,
						    pj_ssize_t buff_size,
						    void *user_data,
						    pjmedia_port **p_port );




PJ_END_DECL


#endif	/* __PJMEDIA_WAV_PORT_H__ */

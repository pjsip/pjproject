/* $Id: cb_port.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny at prijono.org>
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
#ifndef __PJMEDIA_CB_PORT_H__
#define __PJMEDIA_CB_PORT_H__

/**
 * @file cb_port.h
 * @brief Audio callback media port.
 */
#include <pjmedia/port.h>



/**
 * @defgroup PJMEDIA_CB_PORT Audio Callback Port
 * @ingroup PJMEDIA_PORT
 * @brief Calls specified functions when an audio frame arrived or needed.
 * @{
 *
 * Audio callback port provides a simple way to access raw audio streams
 * frame by frame in a higher level application. This allows to connect
 * for example a speech synthesizer or a speech recognizer easily
 * with application-defined buffering or no buffering at all
 * as opposed with @ref PJMEDIA_MEM_PLAYER.
 */


PJ_BEGIN_DECL


/**
 * Create Audio Callback port. 
 *
 * @param pool            Pool to allocate memory.
 * @param sampling_rate        Sampling rate of the port.
 * @param channel_count        Number of channels.
 * @param samples_per_frame    Number of samples per frame.
 * @param bits_per_sample    Number of bits per sample.
 * @param user_data            User data to be specified in the callback
 * @param cb_get_frame        Callback to be called when audio data needed.
 *                    The buffer should be filled in the callback.
 * @param cb_put_frame        Callback to be called when audio data arrived.
 *                    The callback function should process the buffer.
 * @param p_port        Pointer to receive the port instance.
 *
 * @return            PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_cb_port_create( pj_pool_t *pool,
                            unsigned sampling_rate,
                            unsigned channel_count,
                            unsigned samples_per_frame,
                            unsigned bits_per_sample,
                            void *user_data,
                            pj_status_t (*cb_get_frame)(
                                pjmedia_port *port,
                                void *usr_data,
                                void *buffer,
                                pj_size_t buf_size),
                            pj_status_t (*cb_put_frame)(
                                pjmedia_port *port,
                                void *usr_data,
                                const void *buffer,
                                pj_size_t buf_size),
                            pjmedia_port **p_port );


/**
 * Get user object associated with the audio callback port.
 *
 * @param port      The audio callback port whose object to get.
 * @param user_data Pointer to receive the user object.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_cb_port_userdata_get( pjmedia_port *port,
                            void** user_data );


PJ_END_DECL

/**
 * @}
 */


#endif    /* __PJMEDIA_CB_PORT_H__ */

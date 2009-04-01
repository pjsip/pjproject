/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_CODECS_G7221_H__
#define __PJMEDIA_CODECS_G7221_H__

/**
 * @file pjmedia-codec/g7221.h
 * @brief G722.1 codec.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_G7221_CODEC G722.1 Codec
 * @ingroup PJMEDIA_CODEC
 * @brief Implementation of G722.1 codec
 * @{
 *
 * <b>G722.1 licensed from Polycom®</b>
 * <b>G722.1 Annex C licensed from Polycom®</b>
 *
 * This section describes functions to register and register G722.1 codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * PJMEDIA G722.1 codec implementation is based on ITU-T Recommendation 
 * G.722.1 (05/2005) C fixed point implementation including its Annex C.
 *
 * G722.1 is a low complexity codec that supports for 7kHz and 14kHz bandwidth
 * audio signals working at bitrates ranging from 16kbps to 48kbps. It may be
 * used with speech or music inputs.
 *
 * The codec implementation supports for standard and non-standard bitrates.
 * By default, the standard bitrates are enabled upon initialization, i.e.:
 * - 24kbps and 32kbps for audio bandwidth 7 kHz (16kHz sampling rate),
 * - 24kbps, 32kbps, and 48kbps for audio bandwidth 14 kHz (32kHz sampling 
 *   rate).
 * The usage of non-standard bitrates must follow this requirements:
 * - for sampling rate 16kHz: 16000 to 32000 bps, it must be a multiple of 400
 * - for sampling rate 32kHz: 24000 to 48000 bps, it must be a multiple of 400
 * Note that currently it is only up to two non-standard modes can be enabled
 * at one time.
 */

PJ_BEGIN_DECL

/**
 * Initialize and register G722.1 codec factory to pjmedia endpoint.
 *
 * @param endpt	    The pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g7221_init( pjmedia_endpt *endpt );


/**
 * Enable and disable G722.1 mode. By default, the standard modes are 
 * enabled upon initialization, i.e.:
 * - sampling rate 16kHz, bitrate 24kbps and 32kbps.
 * - sampling rate 32kHz, bitrate 24kbps, 32kbps, and 48kbps.
 * This function can also be used for enabling non-standard modes.
 * Note that currently it is only up to two non-standard modes can be 
 * enabled at one time.
 *
 * @param sample_rate	PCM sampling rate, in Hz, valid values are only 
 *			16000 and 32000.
 * @param bitrate	G722.1 bitrate, in bps, the valid values are
 *			standard and non-standard bitrates as described 
 *			above.
 * @param enabled	PJ_TRUE for enabling specified mode.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g7221_set_mode(unsigned sample_rate, 
						  unsigned bitrate, 
						  pj_bool_t enabled);


/**
 * Unregister G722.1 codecs factory from pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g7221_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODECS_G7221_H__ */


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
#ifndef __PJMEDIA_VAD_H__
#define __PJMEDIA_VAD_H__


/**
 * @file vad.h
 * @brief Voice Activity Detector.
 */
#include <pjmedia/types.h>


/**
 * Opaque data type for pjmedia vad.
 */
typedef struct pjmedia_vad  pjmedia_vad;


/**
 * Create voice activity detector with default settings. The default settings
 * are to perform adaptive silence detection, which adjusts the noise level
 * dynamically based on current input level.
 *
 * @param pool		Pool for allocating the structure.
 * @param p_vad		Pointer to receive the VAD instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vad_create( pj_pool_t *pool,
					 pjmedia_vad **p_vad );


/**
 * Calculate average signal level for the given samples.
 *
 * @param samples	Pointer to 16-bit PCM samples.
 * @param count		Number of samples in the input.
 *
 * @return		The average signal level, which simply is total level
 *			divided by number of samples.
 */
PJ_DECL(pj_uint32_t) pjmedia_vad_calc_avg_signal_level( pj_int16_t samples[],
							pj_size_t count );


/**
 * Perform voice activity detection on the given input samples.
 *
 * @param vad		The VAD instance.
 * @param samples	Pointer to 16-bit PCM input samples.
 * @param count		Number of samples in the input.
 * @param p_silence	Pointer to receive the silence detection result.
 *			Non-zero value indicates that that input is considered
 *			as silence.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vad_detect_silence( pjmedia_vad *vad,
						 pj_int16_t samples[],
						 pj_size_t count,
						 pj_bool_t *p_silence);



#endif	/* __PJMEDIA_VAD_H__ */


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

PJ_BEGIN_DECL


/**
 * @see pjmedia_vad
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
 * Set the vad to operate in adaptive mode.
 *
 * @param vad		    The vad
 * @param frame_size Number of samplse per frame.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vad_set_adaptive( pjmedia_vad *vad,
					       unsigned frame_size);


/**
 * Set the vad to operate in fixed threshold mode.
 *
 * @param vad		    The vad
 * @param frame_size Number of samplse per frame.
 * @param threshold	    The silence threshold.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vad_set_fixed( pjmedia_vad *vad,
					    unsigned frame_size,
					    unsigned threshold );

/**
 * Disable the vad.
 *
 * @param vad		The vad
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vad_disable( pjmedia_vad *vad );


/**
 * Calculate average signal level for the given samples.
 *
 * @param samples	Pointer to 16-bit PCM samples.
 * @param count		Number of samples in the input.
 *
 * @return		The average signal level, which simply is total level
 *			divided by number of samples.
 */
PJ_DECL(pj_int32_t) pjmedia_vad_calc_avg_signal( const pj_int16_t samples[],
						 pj_size_t count );


/**
 * Perform voice activity detection on the given input samples.
 *
 * @param vad		The VAD instance.
 * @param samples	Pointer to 16-bit PCM input samples.
 * @param count		Number of samples in the input.
 * @param p_level	Optional pointer to receive average signal level
 *			of the input samples.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_bool_t) pjmedia_vad_detect_silence( pjmedia_vad *vad,
					       const pj_int16_t samples[],
					       pj_size_t count,
					       pj_int32_t *p_level);


PJ_END_DECL

#endif	/* __PJMEDIA_VAD_H__ */


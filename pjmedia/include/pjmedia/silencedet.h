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
#ifndef __PJMEDIA_SILENCE_DET_H__
#define __PJMEDIA_SILENCE_DET_H__


/**
 * @file silencedet.h
 * @brief Adaptive silence detector.
 */
#include <pjmedia/types.h>

PJ_BEGIN_DECL


/**
 * Opaque declaration for silence detector.
 */
typedef struct pjmedia_silence_det pjmedia_silence_det;


/**
 * Create voice activity detector with default settings. The default settings
 * are to perform adaptive silence detection, which adjusts the noise level
 * dynamically based on current input level.
 *
 * @param pool		Pool for allocating the structure.
 * @param p_sd		Pointer to receive the silence detector instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_create( pj_pool_t *pool,
						 pjmedia_silence_det **p_sd );


/**
 * Set the sd to operate in adaptive mode.
 *
 * @param sd	        The silence detector
 * @param frame_size	Number of samples per frame.
 *
 * @return	        PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_set_adaptive( pjmedia_silence_det *sd,
						       unsigned frame_size);


/**
 * Set the sd to operate in fixed threshold mode.
 *
 * @param sd		    The silence detector
 * @param frame_size Number of samplse per frame.
 * @param threshold	    The silence threshold.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_set_fixed( pjmedia_silence_det *sd,
						    unsigned frame_size,
						    unsigned threshold );

/**
 * Disable the silence detector.
 *
 * @param sd		The silence detector
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_disable( pjmedia_silence_det *sd );


/**
 * Calculate average signal level for the given samples.
 *
 * @param samples	Pointer to 16-bit PCM samples.
 * @param count		Number of samples in the input.
 *
 * @return		The average signal level, which simply is total level
 *			divided by number of samples.
 */
PJ_DECL(pj_int32_t) pjmedia_silence_det_calc_avg_signal( const pj_int16_t samples[],
							 pj_size_t count );


/**
 * Perform voice activity detection on the given input samples.
 *
 * @param sd		The silence detector instance.
 * @param samples	Pointer to 16-bit PCM input samples.
 * @param count		Number of samples in the input.
 * @param p_level	Optional pointer to receive average signal level
 *			of the input samples.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_bool_t) pjmedia_silence_det_detect_silence( pjmedia_silence_det *sd,
						       const pj_int16_t samples[],
						       pj_size_t count,
						       pj_int32_t *p_level);


PJ_END_DECL

#endif	/* __PJMEDIA_SILENCE_DET_H__ */


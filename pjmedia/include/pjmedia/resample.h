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
#ifndef __PJMEDIA_RESAMPLE_H__
#define __PJMEDIA_RESAMPLE_H__



/**
 * @file reample.h
 * @brief Sample rate converter.
 */
#include "pjmedia/types.h"

PJ_BEGIN_DECL

/**
 * Opaque resample session.
 */
typedef struct pjmedia_resample pjmedia_resample;

/**
 * Create a frame based resample session.
 *
 * @param pool			Pool to allocate the structure and buffers.
 * @param high_quality		If true, then high quality conversion will be
 *				used, at the expense of more CPU and memory,
 *				because temporary buffer needs to be created.
 * @param large_filter		If true, large filter size will be used.
 * @param rate_in		Clock rate of the input samples.
 * @param rate_out		Clock rate of the output samples.
 * @param samples_per_frame	Number of samples per frame in the input.
 * @param p_resample		Pointer to receive the resample session.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_resample_create(pj_pool_t *pool,
					     pj_bool_t high_quality,
					     pj_bool_t large_filter,
					     unsigned rate_in,
					     unsigned rate_out,
					     unsigned samples_per_frame,
					     pjmedia_resample **p_resample);


/**
 * Resample a frame.
 *
 * @param resample		The resample session.
 * @param input			Buffer containing the input samples.
 * @param output		Buffer to store the output samples.
 */
PJ_DECL(void) pjmedia_resample_run( pjmedia_resample *resample,
				    const pj_int16_t *input,
				    pj_int16_t *output );


PJ_END_DECL

#endif	/* __PJMEDIA_RESAMPLE_H__ */


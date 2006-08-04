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
#ifndef __PJMEDIA_AEC_H__
#define __PJMEDIA_AEC_H__


/**
 * @file aec.h
 * @brief AEC (Accoustic Echo Cancellation) API.
 */
#include <pjmedia/types.h>



/**
 * @defgroup PJMEDIA_AEC AEC AEC (Accoustic Echo Cancellation)
 * @ingroup PJMEDIA_PORT
 * @brief AEC (Accoustic Echo Cancellation) API.
 * @{
 */


PJ_BEGIN_DECL


/**
 * Opaque type for PJMEDIA AEC.
 */
typedef struct pjmedia_aec pjmedia_aec;


/**
 * Create the AEC. 
 *
 * @param pool		    Pool to allocate memory.
 * @param clock_rate	    Media clock rate/sampling rate.
 * @param samples_per_frame Number of samples per frame.
 * @param tail_size	    Tail length, in number of samples.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_aec_create( pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned samples_per_frame,
					 unsigned tail_size,
					 unsigned options,
					 pjmedia_aec **p_aec );


/**
 * Destroy the AEC. 
 *
 * @param aec		The AEC.
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_aec_destroy(pjmedia_aec *aec );


/**
 * Let the AEC knows that a frame has been played to the speaker.
 * The AEC will keep the frame in its internal buffer, to be used
 * when cancelling the echo with #pjmedia_aec_capture().
 *
 * @param aec		The AEC.
 * @param ts		Optional timestamp to let the AEC knows the
 *			position of the frame relative to capture
 *			position. If NULL, the AEC assumes that
 *			application will supply the AEC with continuously
 *			increasing timestamp.
 * @param play_frm	Sample buffer containing frame to be played
 *			(or has been played) to the playback device.
 *			The frame must contain exactly samples_per_frame 
 *			number of samples.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_aec_playback( pjmedia_aec *aec,
					   pj_int16_t *play_frm );


/**
 * Let the AEC knows that a frame has been captured from the microphone.
 * The AEC will cancel the echo from the captured signal, using the
 * internal buffer (supplied by #pjmedia_aec_playback()) as the
 * FES (Far End Speech) reference.
 *
 * @param aec		The AEC.
 * @param rec_frm	On input, it contains the input signal (captured 
 *			from microphone) which echo is to be removed.
 *			Upon returning this function, this buffer contain
 *			the processed signal with the echo removed.
 *			The frame must contain exactly samples_per_frame 
 *			number of samples.
 * @param options	Echo cancellation options, reserved for future use.
 *			Put zero for now.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_aec_capture( pjmedia_aec *aec,
					  pj_int16_t *rec_frm,
					  unsigned options );


/**
 * Perform echo cancellation.
 *
 * @param aec		The AEC.
 * @param rec_frm	On input, it contains the input signal (captured 
 *			from microphone) which echo is to be removed.
 *			Upon returning this function, this buffer contain
 *			the processed signal with the echo removed.
 * @param play_frm	Sample buffer containing frame to be played
 *			(or has been played) to the playback device.
 *			The frame must contain exactly samples_per_frame 
 *			number of samples.
 * @param options	Echo cancellation options, reserved for future use.
 *			Put zero for now.
 * @param reserved	Reserved for future use, put NULL for now.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_aec_cancel_echo( pjmedia_aec *aec,
					      pj_int16_t *rec_frm,
					      const pj_int16_t *play_frm,
					      unsigned options,
					      void *reserved );


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_AEC_H__ */


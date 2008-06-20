/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_WSOLA_H__
#define __PJMEDIA_WSOLA_H__

/**
 * @file wsola.h
 * @brief Waveform Similarity Based Overlap-Add (WSOLA)
 */
#include <pjmedia/types.h>

/**
 * @defgroup PJMED_WSOLA Waveform Similarity Based Overlap-Add (WSOLA)
 * @ingroup PJMEDIA_FRAME_OP
 * @brief Time-scale modification to audio without affecting the pitch
 * @{
 *
 * This section describes Waveform Similarity Based Overlap-Add (WSOLA)
 * implementation in PJMEDIA. The WSOLA API here can be used both to 
 * compress (speed-up) and stretch (expand, slow down) audio playback
 * without altering the pitch, or as a mean for performing packet loss
 * concealment (WSOLA).
 *
 * The WSOLA implementation is used by \ref PJMED_DELAYBUF and \ref PJMED_PLC.
 */

PJ_BEGIN_DECL


/**
 * Opaque declaration for WSOLA structure.
 */
typedef struct pjmedia_wsola pjmedia_wsola;


/**
 * WSOLA options, can be combined with bitmask operation.
 */
enum pjmedia_wsola_option
{
    /**
     * Disable Hanning window to conserve memory.
     */
    PJMEDIA_WSOLA_NO_HANNING	= 1,

    /**
     * Specify that the WSOLA will not be used for PLC.
     */
    PJMEDIA_WSOLA_NO_PLC = 2,

    /**
     * Specify that the WSOLA will not be used to discard frames in
     * non-contiguous buffer.
     */
    PJMEDIA_WSOLA_NO_DISCARD = 4
};



/**
 * Create and initialize WSOLA.
 *
 * @param pool		    Pool to allocate memory for WSOLA.
 * @param clock_rate	    Sampling rate of audio playback.
 * @param samples_per_frame Number of samples per frame.
 * @param channel_count	    Number of channels.
 * @param options	    Option flags, bitmask combination of
 *			    #pjmedia_wsola_option.
 * @param p_wsola	    Pointer to receive WSOLA structure.
 *
 * @return		    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_wsola_create(pj_pool_t *pool, 
					  unsigned clock_rate,
					  unsigned samples_per_frame,
					  unsigned channel_count,
					  unsigned options,
					  pjmedia_wsola **p_wsola);


/**
 * Destroy WSOLA.
 *
 * @param wsola	    WSOLA session.
 *
 * @return	    PJ_SUCCESS normally.
 */
PJ_DECL(pj_status_t) pjmedia_wsola_destroy(pjmedia_wsola *wsola);


/**
 * Reset the buffer contents of WSOLA.
 *
 * @param wsola	    WSOLA session.
 * @param options   Reset options, must be zero for now.
 *
 * @return	    PJ_SUCCESS normally.
 */
PJ_DECL(pj_status_t) pjmedia_wsola_reset(pjmedia_wsola *wsola,
					 unsigned options);


/**
 * Give one good frame to WSOLA to be kept as reference. Application
 * must continuously give WSOLA good frames to keep its session up to
 * date with current playback. Depending on the WSOLA implementation,
 * this function may modify the content of the frame.
 *
 * @param wsola	    WSOLA session.
 * @param frm	    The frame, which length must match the samples per
 *		    frame setting of the WSOLA session.
 * @param prev_lost If application previously generated a synthetic
 *		    frame with #pjmedia_wsola_generate() before calling
 *		    this function, specify whether that was because of
 *		    packet lost. If so, set this parameter to PJ_TRUE
 *		    to make WSOLA interpolate this frame with its buffer.
 *		    Otherwise if this value is PJ_FALSE, WSOLA will
 *		    just append this frame to the end of its buffer.
 *
 * @return	    PJ_SUCCESS normally.
 */
PJ_DECL(pj_status_t) pjmedia_wsola_save(pjmedia_wsola *wsola, 
					short frm[], 
					pj_bool_t prev_lost);

/**
 * Generate one synthetic frame from WSOLA.
 *
 * @param wsola	    WSOLA session.
 * @param frm	    Buffer to receive the frame.
 *
 * @return	    PJ_SUCCESS normally.
 */
PJ_DECL(pj_status_t) pjmedia_wsola_generate(pjmedia_wsola *wsola, 
					    short frm[]);


/**
 * Compress or compact the specified buffer by removing some audio samples
 * from the buffer, without altering the pitch. For this function to work, 
 * total length of the buffer must be more than twice \a erase_cnt.
 * 
 * @param wsola	    WSOLA session.
 * @param buf1	    Pointer to buffer. 
 * @param buf1_cnt  Number of samples in the buffer.
 * @param buf2	    Pointer to second buffer, if the buffer is not
 *		    contiguous. Otherwise this parameter must be NULL.
 * @param buf2_cnt  Number of samples in the second buffer, if the buffer
 *		    is not contiguous. Otherwise this parameter should be
 *		    zero.
 * @param erase_cnt On input, specify the number of samples to be erased.
 *		    This function may erase more or less than the requested 
 *		    number, and the actual number of samples erased will be 
 *		    given on this argument upon returning from the function.
 *
 * @return	    PJ_SUCCESS if some samples have been erased, PJ_ETOOSMALL
 *		    if buffer is too small to be reduced, PJ_EINVAL if any
 *		    of the parameters are not valid.
 */
PJ_DECL(pj_status_t) pjmedia_wsola_discard(pjmedia_wsola *wsola, 
					   short buf1[],
					   unsigned buf1_cnt, 
					   short buf2[],
					   unsigned buf2_cnt,
					   unsigned *erase_cnt);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_WSOLA_H__ */


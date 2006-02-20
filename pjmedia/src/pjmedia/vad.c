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
#include <pjmedia/vad.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>


#define THIS_FILE   "vad.c"

typedef enum pjmedia_vad_mode {
    VAD_MODE_NONE,
    VAD_MODE_FIXED,
    VAD_MODE_ADAPTIVE
} pjmedia_vad_mode;


/**
 * This structure holds the vad state.
 */
struct pjmedia_vad
{
    int	      mode;		/**< VAD mode.				    */
    unsigned  frame_size;	/**< Samples per frame.			    */


    unsigned  min_signal_cnt;	/**< # of signal frames.before talk burst   */
    unsigned  min_silence_cnt;	/**< # of silence frames before silence.    */
    unsigned  recalc_cnt;	/**< # of frames before adaptive recalc.    */

    pj_bool_t in_talk;		/**< In talk burst?			    */
    unsigned  cur_cnt;		/**< # of frames in current mode.	    */
    unsigned  signal_cnt;	/**< # of signal frames received.	    */
    unsigned  silence_cnt;	/**< # of silence frames received	    */
    unsigned  cur_threshold;	/**< Current silence threshold.		    */
    unsigned  weakest_signal;	/**< Weakest signal detected.		    */
    unsigned  loudest_silence;	/**< Loudest silence detected.		    */
};



unsigned char linear2ulaw(int		pcm_val);

PJ_DEF(pj_status_t) pjmedia_vad_create( pj_pool_t *pool,
					pjmedia_vad **p_vad)
{
    pjmedia_vad *vad;

    PJ_ASSERT_RETURN(pool && p_vad, PJ_EINVAL);

    vad = pj_pool_zalloc(pool, sizeof(struct pjmedia_vad));

    vad->weakest_signal = 0xFFFFFFFFUL;
    vad->loudest_silence = 0;
    vad->signal_cnt = 0;
    vad->silence_cnt = 0;
    
    /* Restart in adaptive, silent mode */
    vad->in_talk = PJ_FALSE;
    pjmedia_vad_set_adaptive( vad, 160 );

    *p_vad = vad;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_vad_set_adaptive( pjmedia_vad *vad,
					      unsigned frame_size)
{
    PJ_ASSERT_RETURN(vad && frame_size, PJ_EINVAL);

    vad->frame_size = frame_size;
    vad->mode = VAD_MODE_ADAPTIVE;
    vad->min_signal_cnt = 3;
    vad->min_silence_cnt = 20;
    vad->recalc_cnt = 30;
    vad->cur_threshold = 20;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_vad_set_fixed( pjmedia_vad *vad,
					   unsigned frame_size,
					   unsigned threshold )
{
    PJ_ASSERT_RETURN(vad && frame_size, PJ_EINVAL);

    vad->mode = VAD_MODE_FIXED;
    vad->frame_size = frame_size;
    vad->cur_threshold = threshold;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_vad_disable( pjmedia_vad *vad )
{
    PJ_ASSERT_RETURN(vad, PJ_EINVAL);

    vad->mode = VAD_MODE_NONE;

    return PJ_SUCCESS;
}


PJ_DEF(pj_int32_t) pjmedia_vad_calc_avg_signal(const pj_int16_t samples[],
					       pj_size_t count)
{
    pj_uint32_t sum = 0;
    
    const pj_int16_t * pcm = samples;
    const pj_int16_t * end = samples + count;

    if (count==0)
	return 0;

    while (pcm != end) {
	if (*pcm < 0)
	    sum -= *pcm++;
	else
	    sum += *pcm++;
    }
    
    return (pj_int32_t)(sum / count);
}

PJ_DEF(pj_bool_t) pjmedia_vad_detect_silence( pjmedia_vad *vad,
					      const pj_int16_t samples[],
					      pj_size_t count,
					      pj_int32_t *p_level)
{
    pj_uint32_t level;
    pj_bool_t have_signal;

    /* Always return false if VAD is disabled */
    if (vad->mode == VAD_MODE_NONE) {
	if (p_level)
	    *p_level = -1;
	return PJ_FALSE;
    }
    
    /* Calculate average signal level. */
    level = pjmedia_vad_calc_avg_signal(samples, count);
    
    /* Report to caller, if required. */
    if (p_level)
	*p_level = level;

    /* Convert PCM level to ulaw */
    level = linear2ulaw(level) ^ 0xff;
    
    /* Do we have signal? */
    have_signal = level > vad->cur_threshold;
    
    /* We we're in transition between silence and signel, increment the 
     * current frame counter. We will only switch mode when we have enough
     * frames.
     */
    if (vad->in_talk != have_signal) {
	unsigned limit;

	vad->cur_cnt++;

	limit = (vad->in_talk ? vad->min_silence_cnt : 
				vad->min_signal_cnt);

	if (vad->cur_cnt > limit) {

	    /* Swap mode */
	    vad->in_talk = !vad->in_talk;
	    
	    /* Restart adaptive cur_threshold measurements */
	    vad->weakest_signal = 0xFFFFFFFFUL;
	    vad->loudest_silence = 0;
	    vad->signal_cnt = 0;
	    vad->silence_cnt = 0;
	}

    } else {
	/* Reset frame count */
	vad->cur_cnt = 0;
    }
    
    /* For fixed threshold vad, everything is done. */
    if (vad->mode == VAD_MODE_FIXED) {
	return !vad->in_talk;
    }
    

    /* Count the number of silent and signal frames and calculate min/max */
    if (have_signal) {
	if (level < vad->weakest_signal)
	    vad->weakest_signal = level;
	vad->signal_cnt++;
    }
    else {
	if (level > vad->loudest_silence)
	    vad->loudest_silence = level;
	vad->silence_cnt++;
    }

    /* See if we have had enough frames to look at proportions of 
     * silence/signal frames.
     */
    if ((vad->signal_cnt + vad->silence_cnt) > vad->recalc_cnt) {
	
	/* Adjust silence threshold by looking at the proportions of
	 * signal and silence frames.
	 */
	if (vad->signal_cnt >= vad->recalc_cnt) {
	    /* All frames where signal frames.
	     * Increase silence threshold.
	     */
	    vad->cur_threshold += (vad->weakest_signal - vad->cur_threshold)/4;
	    PJ_LOG(5,(THIS_FILE, "Vad cur_threshold increased to %d",
		      vad->cur_threshold));
	}
	else if (vad->silence_cnt >= vad->recalc_cnt) {
	    /* All frames where silence frames.
	     * Decrease silence threshold.
	     */
	    vad->cur_threshold = (vad->cur_threshold+vad->loudest_silence)/2+1;
	    PJ_LOG(5,(THIS_FILE, "Vad cur_threshold decreased to %d",
		      vad->cur_threshold));
	}
	else { 
	    pj_bool_t updated = PJ_TRUE;

	    /* Adjust according to signal/silence proportions. */
	    if (vad->signal_cnt > vad->silence_cnt * 2)
		vad->cur_threshold++;
	    else if (vad->silence_cnt >  vad->signal_cnt* 2)
		vad->cur_threshold--;
	    else
		updated = PJ_FALSE;

	    if (updated) {
		PJ_LOG(5,(THIS_FILE,
			  "Vad cur_threshold updated to %d",
			  vad->cur_threshold));
	    }
	}

	/* Reset. */
	vad->weakest_signal = 0xFFFFFFFFUL;
	vad->loudest_silence = 0;
	vad->signal_cnt = 0;
	vad->silence_cnt = 0;
    }
    
    return !vad->in_talk;
}


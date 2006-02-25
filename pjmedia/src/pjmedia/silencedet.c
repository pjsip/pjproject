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
#include <pjmedia/silencedet.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>


#define THIS_FILE   "silencedet.c"

typedef enum pjmedia_silence_det_mode {
    VAD_MODE_NONE,
    VAD_MODE_FIXED,
    VAD_MODE_ADAPTIVE
} pjmedia_silence_det_mode;


/**
 * This structure holds the silence detector state.
 */
struct pjmedia_silence_det
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



unsigned char linear2ulaw(int pcm_val);


PJ_DEF(pj_status_t) pjmedia_silence_det_create( pj_pool_t *pool,
						pjmedia_silence_det **p_sd)
{
    pjmedia_silence_det *sd;

    PJ_ASSERT_RETURN(pool && p_sd, PJ_EINVAL);

    sd = pj_pool_zalloc(pool, sizeof(struct pjmedia_silence_det));

    sd->weakest_signal = 0xFFFFFFFFUL;
    sd->loudest_silence = 0;
    sd->signal_cnt = 0;
    sd->silence_cnt = 0;
    
    /* Restart in adaptive, silent mode */
    sd->in_talk = PJ_FALSE;
    pjmedia_silence_det_set_adaptive( sd, 160 );

    *p_sd = sd;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_silence_det_set_adaptive( pjmedia_silence_det *sd,
						      unsigned frame_size)
{
    PJ_ASSERT_RETURN(sd && frame_size, PJ_EINVAL);

    sd->frame_size = frame_size;
    sd->mode = VAD_MODE_ADAPTIVE;
    sd->min_signal_cnt = 10;
    sd->min_silence_cnt = 64;
    sd->recalc_cnt = 250;
    sd->cur_threshold = 20;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_silence_det_set_fixed( pjmedia_silence_det *sd,
						   unsigned frame_size,
						   unsigned threshold )
{
    PJ_ASSERT_RETURN(sd && frame_size, PJ_EINVAL);

    sd->mode = VAD_MODE_FIXED;
    sd->frame_size = frame_size;
    sd->cur_threshold = threshold;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_silence_det_disable( pjmedia_silence_det *sd )
{
    PJ_ASSERT_RETURN(sd, PJ_EINVAL);

    sd->mode = VAD_MODE_NONE;

    return PJ_SUCCESS;
}


PJ_DEF(pj_int32_t) pjmedia_silence_det_calc_avg_signal(const pj_int16_t samples[],
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

PJ_DEF(pj_bool_t) pjmedia_silence_det_detect_silence( pjmedia_silence_det *sd,
						      const pj_int16_t samples[],
						      pj_size_t count,
						      pj_int32_t *p_level)
{
    pj_uint32_t level;
    pj_bool_t have_signal;

    /* Always return false if VAD is disabled */
    if (sd->mode == VAD_MODE_NONE) {
	if (p_level)
	    *p_level = -1;
	return PJ_FALSE;
    }
    
    /* Calculate average signal level. */
    level = pjmedia_silence_det_calc_avg_signal(samples, count);
    
    /* Report to caller, if required. */
    if (p_level)
	*p_level = level;

    /* Convert PCM level to ulaw */
    level = linear2ulaw(level) ^ 0xff;
    
    /* Do we have signal? */
    have_signal = level > sd->cur_threshold;
    
    /* We we're in transition between silence and signel, increment the 
     * current frame counter. We will only switch mode when we have enough
     * frames.
     */
    if (sd->in_talk != have_signal) {
	unsigned limit;

	sd->cur_cnt++;

	limit = (sd->in_talk ? sd->min_silence_cnt : 
				sd->min_signal_cnt);

	if (sd->cur_cnt > limit) {

	    /* Swap mode */
	    sd->in_talk = !sd->in_talk;
	    
	    /* Restart adaptive cur_threshold measurements */
	    sd->weakest_signal = 0xFFFFFFFFUL;
	    sd->loudest_silence = 0;
	    sd->signal_cnt = 0;
	    sd->silence_cnt = 0;
	}

    } else {
	/* Reset frame count */
	sd->cur_cnt = 0;
    }
    
    /* For fixed threshold sd, everything is done. */
    if (sd->mode == VAD_MODE_FIXED) {
	return !sd->in_talk;
    }
    

    /* Count the number of silent and signal frames and calculate min/max */
    if (have_signal) {
	if (level < sd->weakest_signal)
	    sd->weakest_signal = level;
	sd->signal_cnt++;
    }
    else {
	if (level > sd->loudest_silence)
	    sd->loudest_silence = level;
	sd->silence_cnt++;
    }

    /* See if we have had enough frames to look at proportions of 
     * silence/signal frames.
     */
    if ((sd->signal_cnt + sd->silence_cnt) > sd->recalc_cnt) {
	
	/* Adjust silence threshold by looking at the proportions of
	 * signal and silence frames.
	 */
	if (sd->signal_cnt >= sd->recalc_cnt) {
	    /* All frames where signal frames.
	     * Increase silence threshold.
	     */
	    sd->cur_threshold += (sd->weakest_signal - sd->cur_threshold)/4;
	    PJ_LOG(6,(THIS_FILE, "Vad cur_threshold increased to %d",
		      sd->cur_threshold));
	}
	else if (sd->silence_cnt >= sd->recalc_cnt) {
	    /* All frames where silence frames.
	     * Decrease silence threshold.
	     */
	    sd->cur_threshold = (sd->cur_threshold+sd->loudest_silence)/2+1;
	    PJ_LOG(6,(THIS_FILE, "Vad cur_threshold decreased to %d",
		      sd->cur_threshold));
	}
	else { 
	    pj_bool_t updated = PJ_TRUE;

	    /* Adjust according to signal/silence proportions. */
	    if (sd->signal_cnt > sd->silence_cnt * 2)
		sd->cur_threshold++;
	    else if (sd->silence_cnt >  sd->signal_cnt* 2)
		sd->cur_threshold--;
	    else
		updated = PJ_FALSE;

	    if (updated) {
		PJ_LOG(6,(THIS_FILE,
			  "Vad cur_threshold updated to %d",
			  sd->cur_threshold));
	    }
	}

	/* Reset. */
	sd->weakest_signal = 0xFFFFFFFFUL;
	sd->loudest_silence = 0;
	sd->signal_cnt = 0;
	sd->silence_cnt = 0;
    }
    
    return !sd->in_talk;
}


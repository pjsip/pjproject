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

#include <pjmedia/aec.h>
#include <pjmedia/errno.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>


#define THIS_FILE   "aec_speex.c"
#define BUF_COUNT   8


struct frame
{
    pj_int16_t	   *buf;
};

struct pjmedia_aec
{
    SpeexEchoState	 *state;
    SpeexPreprocessState *preprocess;

    unsigned	     samples_per_frame;
    unsigned	     options;
    pj_int16_t	    *tmp_frame;
    spx_int32_t	    *residue;

    pj_lock_t	    *lock;		/* To protect buffers, if required  */

    unsigned	     rpos;		/* Index to get oldest frame.	    */
    unsigned	     wpos;		/* Index to put newest frame.	    */
    struct frame     frames[BUF_COUNT];	/* Playback frame buffers.	    */
};



/*
 * Create the AEC. 
 */
PJ_DEF(pj_status_t) pjmedia_aec_create( pj_pool_t *pool,
					unsigned clock_rate,
					unsigned samples_per_frame,
					unsigned tail_ms,
					unsigned options,
					pjmedia_aec **p_aec )
{
    pjmedia_aec *aec;
    int sampling_rate;
    unsigned i;
    pj_status_t status;

    *p_aec = NULL;

    aec = pj_pool_zalloc(pool, sizeof(pjmedia_aec));
    PJ_ASSERT_RETURN(aec != NULL, PJ_ENOMEM);

    status = pj_lock_create_simple_mutex(pool, "aec%p", &aec->lock);
    if (status != PJ_SUCCESS)
	return status;

    aec->samples_per_frame = samples_per_frame;
    aec->options = options;

    aec->state = speex_echo_state_init(samples_per_frame,
					clock_rate * tail_ms / 1000);
    if (aec->state == NULL) {
	pj_lock_destroy(aec->lock);
	return PJ_ENOMEM;
    }

    aec->preprocess = speex_preprocess_state_init(samples_per_frame, 
						  clock_rate);
    if (aec->preprocess == NULL) {
	speex_echo_state_destroy(aec->state);
	pj_lock_destroy(aec->lock);
	return PJ_ENOMEM;
    }

    /* Set sampling rate */
    sampling_rate = clock_rate;
    speex_echo_ctl(aec->state, SPEEX_ECHO_SET_SAMPLING_RATE, 
		   &sampling_rate);

    /* Create temporary frame for echo cancellation */
    aec->tmp_frame = pj_pool_zalloc(pool, 2 * samples_per_frame);
    PJ_ASSERT_RETURN(aec->tmp_frame != NULL, PJ_ENOMEM);

    /* Create temporary frame to receive residue */
    aec->residue = pj_pool_zalloc(pool, sizeof(spx_int32_t) * 
					    samples_per_frame);
    PJ_ASSERT_RETURN(aec->residue != NULL, PJ_ENOMEM);

    /* Create internal playback buffers */
    for (i=0; i<BUF_COUNT; ++i) {
	aec->frames[i].buf = pj_pool_zalloc(pool, samples_per_frame * 2);
	PJ_ASSERT_RETURN(aec->frames[i].buf != NULL, PJ_ENOMEM);
    }


    /* Done */
    *p_aec = aec;

    PJ_LOG(4,(THIS_FILE, "Echo canceller/AEC created, clock_rate=%d, "
			 "samples per frame=%d, tail length=%d ms", 
			 clock_rate,
			 samples_per_frame,
			 tail_ms));
    return PJ_SUCCESS;

}


/*
 * Destroy AEC
 */
PJ_DEF(pj_status_t) pjmedia_aec_destroy(pjmedia_aec *aec )
{
    PJ_ASSERT_RETURN(aec && aec->state, PJ_EINVAL);

    if (aec->lock)
	pj_lock_acquire(aec->lock);

    if (aec->state) {
	speex_echo_state_destroy(aec->state);
	aec->state = NULL;
    }

    if (aec->preprocess) {
	speex_preprocess_state_destroy(aec->preprocess);
	aec->preprocess = NULL;
    }

    if (aec->lock) {
	pj_lock_destroy(aec->lock);
	aec->lock = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Let the AEC knows that a frame has been played to the speaker.
 */
PJ_DEF(pj_status_t) pjmedia_aec_playback(pjmedia_aec *aec,
					 pj_int16_t *play_frm )
{
    /* Sanity checks */
    PJ_ASSERT_RETURN(aec && play_frm, PJ_EINVAL);

    /* The AEC must be configured to support internal playback buffer */
    PJ_ASSERT_RETURN(aec->frames[0].buf != NULL, PJ_EINVALIDOP);

    pj_lock_acquire(aec->lock);

    /* Check for overflows */
    if (aec->wpos == aec->rpos) {
	PJ_LOG(5,(THIS_FILE, "AEC overflow (playback runs faster, "
			     "wpos=%d, rpos=%d)",
			     aec->wpos, aec->rpos));
	aec->rpos = (aec->wpos - BUF_COUNT/2) % BUF_COUNT;
	speex_echo_state_reset(aec->state);
    }

    /* Save fhe frame */
    pjmedia_copy_samples(aec->frames[aec->wpos].buf,
			 play_frm, aec->samples_per_frame);
    aec->wpos = (aec->wpos+1) % BUF_COUNT;

    pj_lock_release(aec->lock);

    return PJ_SUCCESS;
}


/*
 * Let the AEC knows that a frame has been captured from the microphone.
 */
PJ_DEF(pj_status_t) pjmedia_aec_capture( pjmedia_aec *aec,
					 pj_int16_t *rec_frm,
					 unsigned options )
{
    pj_status_t status;

    /* Sanity checks */
    PJ_ASSERT_RETURN(aec && rec_frm, PJ_EINVAL);

    /* The AEC must be configured to support internal playback buffer */
    PJ_ASSERT_RETURN(aec->frames[0].buf != NULL, PJ_EINVALIDOP);

    /* Lock mutex */
    pj_lock_acquire(aec->lock);


    /* Check for underflow */
    if (aec->rpos == aec->wpos) {
	/* Return frame as it is */
	pj_lock_release(aec->lock);

	PJ_LOG(5,(THIS_FILE, "AEC underflow (capture runs faster than "
			     "playback, wpos=%d, rpos=%d)", 
			     aec->wpos, aec->rpos));
	aec->rpos = (aec->wpos - BUF_COUNT/2) % BUF_COUNT;
	speex_echo_state_reset(aec->state);

	return PJ_SUCCESS;
    }
    

    /* Cancel echo */
    status = pjmedia_aec_cancel_echo(aec, rec_frm, 
				     aec->frames[aec->rpos].buf, options,
				     NULL);

    aec->rpos = (aec->rpos + 1) % BUF_COUNT;

    pj_lock_release(aec->lock);
    return status;
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) pjmedia_aec_cancel_echo( pjmedia_aec *aec,
					     pj_int16_t *rec_frm,
					     const pj_int16_t *play_frm,
					     unsigned options,
					     void *reserved )
{
    /* Sanity checks */
    PJ_ASSERT_RETURN(aec && rec_frm && play_frm && options==0 &&
		     reserved==NULL, PJ_EINVAL);

    /* Cancel echo, put output in temporary buffer */
    speex_echo_cancel(aec->state, (const spx_int16_t*)rec_frm, 
		      (const spx_int16_t*)play_frm, 
		      (spx_int16_t*)aec->tmp_frame, 
		      aec->residue);


    /* Preprocess output */
    speex_preprocess(aec->preprocess, (spx_int16_t*)aec->tmp_frame, 
		     aec->residue);

    /* Copy temporary buffer back to original rec_frm */
    pjmedia_copy_samples(rec_frm, aec->tmp_frame, aec->samples_per_frame);

    return PJ_SUCCESS;

}


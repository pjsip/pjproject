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

#include <pjmedia/echo.h>
#include <pjmedia/errno.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>


#define THIS_FILE   "echo_speex.c"
#define BUF_COUNT   8

/*
 * Prototypes
 */
PJ_DECL(pj_status_t) speex_aec_create(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned options,
				      void **p_state );
PJ_DECL(pj_status_t) speex_aec_destroy(void *state );
PJ_DECL(pj_status_t) speex_aec_playback(void *state,
				        pj_int16_t *play_frm );
PJ_DECL(pj_status_t) speex_aec_capture(void *state,
				       pj_int16_t *rec_frm,
				       unsigned options );
PJ_DECL(pj_status_t) speex_aec_cancel_echo(void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved );


struct frame
{
    pj_int16_t	   *buf;
};

typedef struct speex_ec
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
} speex_ec;



/*
 * Create the AEC. 
 */
PJ_DEF(pj_status_t) speex_aec_create(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned samples_per_frame,
				     unsigned tail_ms,
				     unsigned options,
				     void **p_echo )
{
    speex_ec *echo;
    int sampling_rate;
    unsigned i;
    int disabled;
    pj_status_t status;

    *p_echo = NULL;

    echo = pj_pool_zalloc(pool, sizeof(speex_ec));
    PJ_ASSERT_RETURN(echo != NULL, PJ_ENOMEM);

    if (options & PJMEDIA_ECHO_NO_LOCK) {
	status = pj_lock_create_null_mutex(pool, "aec%p", &echo->lock);
	if (status != PJ_SUCCESS)
	    return status;
    } else {
	status = pj_lock_create_simple_mutex(pool, "aec%p", &echo->lock);
	if (status != PJ_SUCCESS)
	    return status;
    }

    echo->samples_per_frame = samples_per_frame;
    echo->options = options;

    echo->state = speex_echo_state_init(samples_per_frame,
					clock_rate * tail_ms / 1000);
    if (echo->state == NULL) {
	pj_lock_destroy(echo->lock);
	return PJ_ENOMEM;
    }

    echo->preprocess = speex_preprocess_state_init(samples_per_frame, 
						   clock_rate);
    if (echo->preprocess == NULL) {
	speex_echo_state_destroy(echo->state);
	pj_lock_destroy(echo->lock);
	return PJ_ENOMEM;
    }

    /* Disable all preprocessing, we only want echo cancellation */
    disabled = 0;
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_DENOISE, 
			 &disabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_AGC, 
			 &disabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_VAD, 
			 &disabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_DEREVERB, 
			 &disabled);

    /* Set sampling rate */
    sampling_rate = clock_rate;
    speex_echo_ctl(echo->state, SPEEX_ECHO_SET_SAMPLING_RATE, 
		   &sampling_rate);

    /* Create temporary frame for echo cancellation */
    echo->tmp_frame = pj_pool_zalloc(pool, 2 * samples_per_frame);
    PJ_ASSERT_RETURN(echo->tmp_frame != NULL, PJ_ENOMEM);

    /* Create temporary frame to receive residue */
    echo->residue = pj_pool_zalloc(pool, sizeof(spx_int32_t) * 
					    samples_per_frame);
    PJ_ASSERT_RETURN(echo->residue != NULL, PJ_ENOMEM);

    /* Create internal playback buffers */
    for (i=0; i<BUF_COUNT; ++i) {
	echo->frames[i].buf = pj_pool_zalloc(pool, samples_per_frame * 2);
	PJ_ASSERT_RETURN(echo->frames[i].buf != NULL, PJ_ENOMEM);
    }


    /* Done */
    *p_echo = echo;

    PJ_LOG(4,(THIS_FILE, "Speex Echo canceller/AEC created, clock_rate=%d, "
			 "samples per frame=%d, tail length=%d ms", 
			 clock_rate,
			 samples_per_frame,
			 tail_ms));
    return PJ_SUCCESS;

}


/*
 * Destroy AEC
 */
PJ_DEF(pj_status_t) speex_aec_destroy(void *state )
{
    speex_ec *echo = state;

    PJ_ASSERT_RETURN(echo && echo->state, PJ_EINVAL);

    if (echo->lock)
	pj_lock_acquire(echo->lock);

    if (echo->state) {
	speex_echo_state_destroy(echo->state);
	echo->state = NULL;
    }

    if (echo->preprocess) {
	speex_preprocess_state_destroy(echo->preprocess);
	echo->preprocess = NULL;
    }

    if (echo->lock) {
	pj_lock_destroy(echo->lock);
	echo->lock = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Let the AEC knows that a frame has been played to the speaker.
 */
PJ_DEF(pj_status_t) speex_aec_playback(void *state,
				       pj_int16_t *play_frm )
{
    speex_ec *echo = state;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && play_frm, PJ_EINVAL);

    /* The AEC must be configured to support internal playback buffer */
    PJ_ASSERT_RETURN(echo->frames[0].buf != NULL, PJ_EINVALIDOP);

    pj_lock_acquire(echo->lock);

    /* Check for overflows */
    if (echo->wpos == echo->rpos) {
	PJ_LOG(5,(THIS_FILE, "Speex AEC overflow (playback runs faster, "
			     "wpos=%d, rpos=%d)",
			     echo->wpos, echo->rpos));
	echo->rpos = (echo->wpos - BUF_COUNT/2) % BUF_COUNT;
	speex_echo_state_reset(echo->state);
    }

    /* Save fhe frame */
    pjmedia_copy_samples(echo->frames[echo->wpos].buf,
			 play_frm, echo->samples_per_frame);
    echo->wpos = (echo->wpos+1) % BUF_COUNT;

    pj_lock_release(echo->lock);

    return PJ_SUCCESS;
}


/*
 * Let the AEC knows that a frame has been captured from the microphone.
 */
PJ_DEF(pj_status_t) speex_aec_capture( void *state,
				       pj_int16_t *rec_frm,
				       unsigned options )
{
    speex_ec *echo = state;
    pj_status_t status;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm, PJ_EINVAL);

    /* The AEC must be configured to support internal playback buffer */
    PJ_ASSERT_RETURN(echo->frames[0].buf != NULL, PJ_EINVALIDOP);

    /* Lock mutex */
    pj_lock_acquire(echo->lock);


    /* Check for underflow */
    if (echo->rpos == echo->wpos) {
	/* Return frame as it is */
	pj_lock_release(echo->lock);

	PJ_LOG(5,(THIS_FILE, "Speex AEC underflow (capture runs faster than "
			     "playback, wpos=%d, rpos=%d)", 
			     echo->wpos, echo->rpos));
	echo->rpos = (echo->wpos - BUF_COUNT/2) % BUF_COUNT;
	speex_echo_state_reset(echo->state);

	return PJ_SUCCESS;
    }
    

    /* Cancel echo */
    status = speex_aec_cancel_echo(echo, rec_frm, 
				   echo->frames[echo->rpos].buf, options,
				   NULL);

    echo->rpos = (echo->rpos + 1) % BUF_COUNT;

    pj_lock_release(echo->lock);
    return status;
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) speex_aec_cancel_echo( void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved )
{
    speex_ec *echo = state;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm && play_frm && options==0 &&
		     reserved==NULL, PJ_EINVAL);

    /* Cancel echo, put output in temporary buffer */
    speex_echo_cancel(echo->state, (const spx_int16_t*)rec_frm, 
		      (const spx_int16_t*)play_frm, 
		      (spx_int16_t*)echo->tmp_frame, 
		      echo->residue);


    /* Preprocess output */
    speex_preprocess(echo->preprocess, (spx_int16_t*)echo->tmp_frame, 
		     echo->residue);

    /* Copy temporary buffer back to original rec_frm */
    pjmedia_copy_samples(rec_frm, echo->tmp_frame, echo->samples_per_frame);

    return PJ_SUCCESS;

}


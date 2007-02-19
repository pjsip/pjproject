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
#include <pjmedia/types.h>
#include <pjmedia/errno.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>


#define THIS_FILE			    "echo_suppress.c"


/*
 * Simple echo suppresor
 */
typedef struct echo_supp
{
    pj_bool_t		 suppressing;
    pjmedia_silence_det	*sd;
    pj_time_val		 last_signal;
    unsigned		 samples_per_frame;
    unsigned		 tail_ms;
} echo_supp;



/*
 * Prototypes.
 */
PJ_DECL(pj_status_t) echo_supp_create(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned latency_ms,
				      unsigned options,
				      void **p_state );
PJ_DECL(pj_status_t) echo_supp_destroy(void *state);
PJ_DECL(pj_status_t) echo_supp_playback(void *state,
					pj_int16_t *play_frm );
PJ_DECL(pj_status_t) echo_supp_capture(void *state,
				       pj_int16_t *rec_frm,
				       unsigned options );
PJ_DECL(pj_status_t) echo_supp_cancel_echo(void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved );



/*
 * Create. 
 */
PJ_DEF(pj_status_t) echo_supp_create( pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned latency_ms,
				      unsigned options,
				      void **p_state )
{
    echo_supp *ec;
    pj_status_t status;

    PJ_UNUSED_ARG(clock_rate);
    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(latency_ms);

    ec = pj_pool_zalloc(pool, sizeof(struct echo_supp));
    ec->samples_per_frame = samples_per_frame;
    ec->tail_ms = tail_ms;

    status = pjmedia_silence_det_create(pool, clock_rate, samples_per_frame,
					&ec->sd);
    if (status != PJ_SUCCESS)
	return status;

    pjmedia_silence_det_set_name(ec->sd, "ecsu%p");
    pjmedia_silence_det_set_adaptive(ec->sd, PJMEDIA_ECHO_SUPPRESS_THRESHOLD);
    pjmedia_silence_det_set_params(ec->sd, 100, 500, 3000);

    *p_state = ec;
    return PJ_SUCCESS;
}


/*
 * Destroy. 
 */
PJ_DEF(pj_status_t) echo_supp_destroy(void *state)
{
    PJ_UNUSED_ARG(state);
    return PJ_SUCCESS;
}


/*
 * Let the AEC knows that a frame has been played to the speaker.
 */
PJ_DEF(pj_status_t) echo_supp_playback( void *state,
					pj_int16_t *play_frm )
{
    echo_supp *ec = state;
    pj_bool_t silence;
    pj_bool_t last_suppressing = ec->suppressing;

    silence = pjmedia_silence_det_detect(ec->sd, play_frm,
					 ec->samples_per_frame, NULL);

    ec->suppressing = !silence;

    if (ec->suppressing) {
	pj_gettimeofday(&ec->last_signal);
    }

    if (ec->suppressing!=0 && last_suppressing==0) {
	PJ_LOG(5,(THIS_FILE, "Start suppressing.."));
    } else if (ec->suppressing==0 && last_suppressing!=0) {
	PJ_LOG(5,(THIS_FILE, "Stop suppressing.."));
    }

    return PJ_SUCCESS;
}


/*
 * Let the AEC knows that a frame has been captured from the microphone.
 */
PJ_DEF(pj_status_t) echo_supp_capture( void *state,
				       pj_int16_t *rec_frm,
				       unsigned options )
{
    echo_supp *ec = state;
    pj_time_val now;
    unsigned delay_ms;

    PJ_UNUSED_ARG(options);

    pj_gettimeofday(&now);

    PJ_TIME_VAL_SUB(now, ec->last_signal);
    delay_ms = PJ_TIME_VAL_MSEC(now);

    if (delay_ms < ec->tail_ms) {
#if defined(PJMEDIA_ECHO_SUPPRESS_FACTOR) && PJMEDIA_ECHO_SUPPRESS_FACTOR!=0
	unsigned i;
	for (i=0; i<ec->samples_per_frame; ++i) {
	    rec_frm[i] = (pj_int16_t)(rec_frm[i] >> 
				      PJMEDIA_ECHO_SUPPRESS_FACTOR);
	}
#else
	pjmedia_zero_samples(rec_frm, ec->samples_per_frame);
#endif
    }

    return PJ_SUCCESS;
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) echo_supp_cancel_echo( void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved )
{
    echo_supp *ec = state;
    pj_bool_t silence;

    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(reserved);

    silence = pjmedia_silence_det_detect(ec->sd, play_frm, 
					 ec->samples_per_frame, NULL);

    if (!silence) {
#if defined(PJMEDIA_ECHO_SUPPRESS_FACTOR) && PJMEDIA_ECHO_SUPPRESS_FACTOR!=0
	unsigned i;
	for (i=0; i<ec->samples_per_frame; ++i) {
	    rec_frm[i] = (pj_int16_t)(rec_frm[i] >> 
				      PJMEDIA_ECHO_SUPPRESS_FACTOR);
	}
#else
	pjmedia_zero_samples(rec_frm, ec->samples_per_frame);
#endif
    }

    return PJ_SUCCESS;
}


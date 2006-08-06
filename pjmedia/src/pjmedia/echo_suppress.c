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
#include <pjmedia/types.h>
#include <pjmedia/errno.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>


#define THIS_FILE			    "echo_suppress.c"
#define PJMEDIA_ECHO_SUPPRESS_THRESHOLD	    20


/*
 * Simple echo suppresor
 */
typedef struct echo_supp
{
    unsigned	threshold;
    pj_bool_t	suppressing;
    pj_time_val	last_signal;
    unsigned	samples_per_frame;
    unsigned	tail_ms;
} echo_supp;



/*
 * Prototypes.
 */
PJ_DECL(pj_status_t) echo_supp_create(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
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
				      unsigned options,
				      void **p_state )
{
    echo_supp *ec;

    PJ_UNUSED_ARG(clock_rate);
    PJ_UNUSED_ARG(options);

    ec = pj_pool_zalloc(pool, sizeof(struct echo_supp));
    ec->threshold = PJMEDIA_ECHO_SUPPRESS_THRESHOLD;
    ec->samples_per_frame = samples_per_frame;
    ec->tail_ms = tail_ms;

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
    pj_bool_t last_suppressing = ec->suppressing;
    unsigned level;

    level = pjmedia_calc_avg_signal(play_frm, ec->samples_per_frame);
    level = linear2ulaw(level) ^ 0xff;

    if (level >= ec->threshold) {
	pj_gettimeofday(&ec->last_signal);
	ec->suppressing = 1;
    } else {
	ec->suppressing = 0;
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
	pjmedia_zero_samples(rec_frm, ec->samples_per_frame);
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
    unsigned level;

    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(reserved);

    level = pjmedia_calc_avg_signal(play_frm, ec->samples_per_frame);
    level = linear2ulaw(level) ^ 0xff;

    if (level >= ec->threshold) {
	pjmedia_zero_samples(rec_frm, ec->samples_per_frame);
    }

    return PJ_SUCCESS;
}



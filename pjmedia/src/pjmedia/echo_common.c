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

#include <pjmedia/config.h>
#include <pjmedia/echo.h>
#include <pj/assert.h>
#include <pj/pool.h>


typedef struct ec_operations ec_operations;

struct pjmedia_echo_state
{
    void	    *state;
    ec_operations   *op;
};


struct ec_operations
{
    pj_status_t (*ec_create)(pj_pool_t *pool,
			    unsigned clock_rate,
			    unsigned samples_per_frame,
			    unsigned tail_ms,
			    unsigned latency_ms,
			    unsigned options,
			    void **p_state );
    pj_status_t (*ec_destroy)(void *state );
    pj_status_t (*ec_playback)(void *state,
			      pj_int16_t *play_frm );
    pj_status_t (*ec_capture)(void *state,
			      pj_int16_t *rec_frm,
			      unsigned options );
    pj_status_t (*ec_cancel)(void *state,
			     pj_int16_t *rec_frm,
			     const pj_int16_t *play_frm,
			     unsigned options,
			     void *reserved );
};



/*
 * Simple echo suppressor
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

static struct ec_operations echo_supp_op = 
{
    &echo_supp_create,
    &echo_supp_destroy,
    &echo_supp_playback,
    &echo_supp_capture,
    &echo_supp_cancel_echo
};



/*
 * Speex AEC prototypes
 */
#if defined(PJMEDIA_HAS_SPEEX_AEC) && PJMEDIA_HAS_SPEEX_AEC!=0
PJ_DECL(pj_status_t) speex_aec_create(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned latency_ms,
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

static struct ec_operations aec_op = 
{
    &speex_aec_create,
    &speex_aec_destroy,
    &speex_aec_playback,
    &speex_aec_capture,
    &speex_aec_cancel_echo
};

#else
#define aec_op echo_supp_op
#endif



/*
 * Create the echo canceller. 
 */
PJ_DEF(pj_status_t) pjmedia_echo_create( pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned samples_per_frame,
					 unsigned tail_ms,
					 unsigned latency_ms,
					 unsigned options,
					 pjmedia_echo_state **p_echo )
{
    pjmedia_echo_state *ec;
    pj_status_t status;

    /* Force to use simple echo suppressor if AEC is not available */
#if !defined(PJMEDIA_HAS_SPEEX_AEC) || PJMEDIA_HAS_SPEEX_AEC==0
    options |= PJMEDIA_ECHO_SIMPLE;
#endif

    ec = PJ_POOL_ZALLOC_T(pool, struct pjmedia_echo_state);

    if (options & PJMEDIA_ECHO_SIMPLE) {
	ec->op = &echo_supp_op;
	status = (*echo_supp_op.ec_create)(pool, clock_rate, samples_per_frame,
					   tail_ms, latency_ms, options,
					   &ec->state);
    } else {
	ec->op = &aec_op;
	status = (*aec_op.ec_create)(pool, clock_rate, 
				     samples_per_frame,
				     tail_ms, latency_ms, options,
				     &ec->state);
    }

    if (status != PJ_SUCCESS)
	return status;

    pj_assert(ec->state != NULL);

    *p_echo = ec;

    return PJ_SUCCESS;
}


/*
 * Destroy the Echo Canceller. 
 */
PJ_DEF(pj_status_t) pjmedia_echo_destroy(pjmedia_echo_state *echo )
{
    return (*echo->op->ec_destroy)(echo->state);
}



/*
 * Let the Echo Canceller knows that a frame has been played to the speaker.
 */
PJ_DEF(pj_status_t) pjmedia_echo_playback( pjmedia_echo_state *echo,
					   pj_int16_t *play_frm )
{
    return (*echo->op->ec_playback)(echo->state, play_frm);
}


/*
 * Let the Echo Canceller knows that a frame has been captured from 
 * the microphone.
 */
PJ_DEF(pj_status_t) pjmedia_echo_capture( pjmedia_echo_state *echo,
					  pj_int16_t *rec_frm,
					  unsigned options )
{
    return (*echo->op->ec_capture)(echo->state, rec_frm, options);
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) pjmedia_echo_cancel( pjmedia_echo_state *echo,
					 pj_int16_t *rec_frm,
					 const pj_int16_t *play_frm,
					 unsigned options,
					 void *reserved )
{
    return (*echo->op->ec_cancel)( echo->state, rec_frm, play_frm, options, 
				   reserved);
}


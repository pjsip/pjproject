/* $Id$ */
/* 
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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

#include "echo_internal.h"

#define THIS_FILE			    "echo_suppress.c"


/*
 * Simple echo suppresor
 */
typedef struct echo_supp
{
    pjmedia_silence_det	*sd;
    unsigned		 samples_per_frame;
    unsigned		 tail_ms;
} echo_supp;



/*
 * Create. 
 */
PJ_DEF(pj_status_t) echo_supp_create( pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned options,
				      void **p_state )
{
    echo_supp *ec;
    pj_status_t status;

    PJ_UNUSED_ARG(clock_rate);
    PJ_UNUSED_ARG(channel_count);
    PJ_UNUSED_ARG(options);

    ec = PJ_POOL_ZALLOC_T(pool, struct echo_supp);
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
 * Reset
 */
PJ_DEF(void) echo_supp_reset(void *state)
{
    PJ_UNUSED_ARG(state);
    return;
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
    echo_supp *ec = (echo_supp*) state;
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


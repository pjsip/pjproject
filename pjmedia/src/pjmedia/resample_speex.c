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

#include <pjmedia/resample.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

#include <speex/speex_resampler.h>

#define THIS_FILE   "resample.c"


struct pjmedia_resample
{
    SpeexResamplerState *state;
#if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT != 0
    float		*in_buffer;
    float		*out_buffer;
#endif
    unsigned		 in_samples_per_frame;
    unsigned		 out_samples_per_frame;
};


PJ_DEF(pj_status_t) pjmedia_resample_create( pj_pool_t *pool,
					     pj_bool_t high_quality,
					     pj_bool_t large_filter,
					     unsigned channel_count,
					     unsigned rate_in,
					     unsigned rate_out,
					     unsigned samples_per_frame,
					     pjmedia_resample **p_resample)
{
    pjmedia_resample *resample;
    int quality;
    int err;

    PJ_ASSERT_RETURN(pool && p_resample && rate_in &&
		     rate_out && samples_per_frame, PJ_EINVAL);

    resample = PJ_POOL_ZALLOC_T(pool, pjmedia_resample);
    PJ_ASSERT_RETURN(resample, PJ_ENOMEM);

    if (high_quality) {
	if (large_filter)
	    quality = 8;
	else
	    quality = 7;
    } else {
	quality = 3;
    }

    resample->in_samples_per_frame = samples_per_frame;
    resample->out_samples_per_frame = rate_out / (rate_in / samples_per_frame);
    resample->state = speex_resampler_init(channel_count,  rate_in, rate_out, 
                                           quality, &err);
    if (resample->state == NULL || err != RESAMPLER_ERR_SUCCESS)
	return PJ_ENOMEM;

#if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT != 0
    resample->in_buffer = pj_pool_calloc(pool, resample->in_samples_per_frame, 
					 sizeof(float));
    resample->out_buffer=pj_pool_calloc(pool, resample->out_samples_per_frame,
				        sizeof(float));
#endif

    *p_resample = resample;

    PJ_LOG(5,(THIS_FILE, 
	      "resample created: quality=%d, ch=%d, in/out rate=%d/%d", 
	      quality, channel_count, rate_in, rate_out));
    return PJ_SUCCESS;
}


PJ_DEF(void) pjmedia_resample_run( pjmedia_resample *resample,
				   const pj_int16_t *input,
				   pj_int16_t *output )
{
    spx_uint32_t in_length, out_length;
    float *fp;
    unsigned i;

    PJ_ASSERT_ON_FAIL(resample, return);

    in_length = resample->in_samples_per_frame;
    out_length = resample->out_samples_per_frame;

#if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT != 0
    fp = resample->in_buffer;
    for (i=0; i<in_length; ++i) {
	fp[i] = input[i];
    }
    speex_resampler_process_interleaved_float(resample->state,
					      resample->in_buffer, &in_length,
					      resample->out_buffer, &out_length);
    fp = resample->out_buffer;
    for (i=0; i<out_length; ++i) {
	output[i] = (pj_int16_t)fp[i];
    }
#else
    PJ_UNUSED_ARG(dst);
    PJ_UNUSED_ARG(i);
    speex_resampler_process_interleaved_int(resample->state,
					    (const __int16 *)input, &in_length,
					    (__int16 *)output, &out_length);
#endif

    pj_assert(in_length == resample->in_samples_per_frame);
    pj_assert(out_length == resample->out_samples_per_frame);
}


PJ_DEF(unsigned) pjmedia_resample_get_input_size(pjmedia_resample *resample)
{
    PJ_ASSERT_RETURN(resample != NULL, 0);
    return resample->in_samples_per_frame;
}


PJ_DEF(void) pjmedia_resample_destroy(pjmedia_resample *resample)
{
    PJ_ASSERT_ON_FAIL(resample, return);
    if (resample->state) {
	speex_resampler_destroy(resample->state);
	resample->state = NULL;
    }
}


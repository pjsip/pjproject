/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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

#include <pjmedia/echo.h>
#include <pjmedia/errno.h>
#include <pjmedia/frame.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

#if defined(PJMEDIA_HAS_SPEEX_AEC) && PJMEDIA_HAS_SPEEX_AEC != 0

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include "echo_internal.h"

typedef struct speex_ec
{
    SpeexEchoState       *state;
    SpeexDecorrState     *decorr;
    SpeexPreprocessState **preprocess;

    unsigned              samples_per_frame;
    unsigned              channel_count;
    unsigned              spf_per_channel;
    unsigned              options;
    pj_int16_t           *tmp_frame;
} speex_ec;



/*
 * Create the AEC. 
 */
PJ_DEF(pj_status_t) speex_aec_create(pj_pool_t *pool,
                                     unsigned clock_rate,
                                     unsigned channel_count,
                                     unsigned samples_per_frame,
                                     unsigned tail_ms,
                                     unsigned options,
                                     void **p_echo )
{
    speex_ec *echo;
    int sampling_rate;
    unsigned i;

    *p_echo = NULL;

    echo = PJ_POOL_ZALLOC_T(pool, speex_ec);
    PJ_ASSERT_RETURN(echo != NULL, PJ_ENOMEM);

    echo->channel_count = channel_count;
    echo->samples_per_frame = samples_per_frame;
    echo->spf_per_channel = samples_per_frame / channel_count;
    echo->options = options;

#if 1
    echo->state = speex_echo_state_init_mc(echo->spf_per_channel,
                                           clock_rate * tail_ms / 1000,
                                           channel_count, channel_count);
#else
    if (channel_count != 1) {
        PJ_LOG(2,("echo_speex.c", "Multichannel EC is not supported by this "
                                  "echo canceller. It may not work."));
    }
    echo->state = speex_echo_state_init(echo->samples_per_frame,
                                        clock_rate * tail_ms / 1000);
#endif
    if (echo->state == NULL)
        return PJ_ENOMEM;

    echo->decorr = speex_decorrelate_new(clock_rate, channel_count,
                                         echo->spf_per_channel);
    if (echo->decorr == NULL)
        return PJ_ENOMEM;

    /* Set sampling rate */
    sampling_rate = clock_rate;
    speex_echo_ctl(echo->state, SPEEX_ECHO_SET_SAMPLING_RATE, 
                   &sampling_rate);

    /* We need to create one state per channel processed. */
    echo->preprocess = PJ_POOL_ZALLOC_T(pool, SpeexPreprocessState *);
    for (i = 0; i < channel_count; i++) {
        spx_int32_t enabled;

        echo->preprocess[i] = speex_preprocess_state_init(
                                  echo->spf_per_channel, clock_rate);
        if (echo->preprocess[i] == NULL) {
            speex_aec_destroy(echo);
            return PJ_ENOMEM;
        }

        /* Enable/disable AGC & denoise */
        enabled = PJMEDIA_SPEEX_AEC_USE_AGC;
        speex_preprocess_ctl(echo->preprocess[i], SPEEX_PREPROCESS_SET_AGC,
                             &enabled);

        enabled = PJMEDIA_SPEEX_AEC_USE_DENOISE;
        speex_preprocess_ctl(echo->preprocess[i],
                             SPEEX_PREPROCESS_SET_DENOISE, &enabled);

        /* Currently, VAD and dereverb are set at default setting. */
        /*
        enabled = 1;
        speex_preprocess_ctl(echo->preprocess[i], SPEEX_PREPROCESS_SET_VAD,
                             &enabled);
        speex_preprocess_ctl(echo->preprocess[i],
                             SPEEX_PREPROCESS_SET_DEREVERB,
                             &enabled);
        */

        /* Control echo cancellation in the preprocessor */
        speex_preprocess_ctl(echo->preprocess[i],
                             SPEEX_PREPROCESS_SET_ECHO_STATE, echo->state);
    }

    /* Create temporary frame for echo cancellation */
    echo->tmp_frame = (pj_int16_t*) pj_pool_zalloc(pool, sizeof(pj_int16_t) *
                                                   channel_count *
                                                   samples_per_frame);
    if (!echo->tmp_frame) {
        speex_aec_destroy(echo);
        return PJ_ENOMEM;
    }

    /* Done */
    *p_echo = echo;
    return PJ_SUCCESS;

}


/*
 * Destroy AEC
 */
PJ_DEF(pj_status_t) speex_aec_destroy(void *state )
{
    speex_ec *echo = (speex_ec*) state;
    unsigned i;

    PJ_ASSERT_RETURN(echo && echo->state, PJ_EINVAL);

    if (echo->state) {
        speex_echo_state_destroy(echo->state);
        echo->state = NULL;
    }

    if (echo->decorr) {
        speex_decorrelate_destroy(echo->decorr);
        echo->decorr = NULL;
    }

    if (echo->preprocess) {
        for (i = 0; i < echo->channel_count; i++) {
            if (echo->preprocess[i]) {
                speex_preprocess_state_destroy(echo->preprocess[i]);
                echo->preprocess[i] = NULL;
            }
        }
        echo->preprocess = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Reset AEC
 */
PJ_DEF(void) speex_aec_reset(void *state )
{
    speex_ec *echo = (speex_ec*) state;
    speex_echo_state_reset(echo->state);
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
    speex_ec *echo = (speex_ec*) state;
    unsigned i;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm && play_frm && options==0 &&
                     reserved==NULL, PJ_EINVAL);

    /* Cancel echo, put output in temporary buffer */
    speex_echo_cancellation(echo->state, (const spx_int16_t*)rec_frm,
                            (const spx_int16_t*)play_frm,
                            (spx_int16_t*)echo->tmp_frame);


    /* Preprocess output per channel */
    for (i = 0; i < echo->channel_count; i++) {
        spx_int16_t *buf = (spx_int16_t*)echo->tmp_frame;
        unsigned j;

        /* De-interleave each channel. */
        if (echo->channel_count > 1) {
            for (j = 0; j < echo->spf_per_channel; j++) {
                rec_frm[j] = echo->tmp_frame[j * echo->channel_count + i];
            }
            buf = (spx_int16_t*)rec_frm;
        }

        speex_preprocess_run(echo->preprocess[i], buf);

        if (echo->channel_count > 1) {
            for (j = 0; j < echo->spf_per_channel; j++) {
                echo->tmp_frame[j * echo->channel_count + i] = rec_frm[j];
            }
        }
    }

    /* Copy temporary buffer back to original rec_frm */
    pjmedia_copy_samples(rec_frm, echo->tmp_frame, echo->samples_per_frame);

    return PJ_SUCCESS;

}

/*
 * Let AEC know that a frame was queued to be played.
 */
PJ_DEF(pj_status_t) speex_aec_playback( void *state,
                                        pj_int16_t *play_frm )
{
    speex_ec *echo = (speex_ec*) state;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && play_frm, PJ_EINVAL);

    /* Channel decorrelation algorithm is useful for multi-channel echo
     * cancellation only .
     */
    if (echo->channel_count > 1) {
        pjmedia_copy_samples(echo->tmp_frame, play_frm, echo->samples_per_frame);
        speex_decorrelate(echo->decorr, (spx_int16_t*)echo->tmp_frame,
                          (spx_int16_t*)play_frm, 100);
    }

    speex_echo_playback(echo->state, (spx_int16_t*)play_frm);

    return PJ_SUCCESS;

}

/*
 * Perform echo cancellation to captured frame.
 */
PJ_DEF(pj_status_t) speex_aec_capture( void *state,
                                       pj_int16_t *rec_frm,
                                       unsigned options )
{
    speex_ec *echo = (speex_ec*) state;
    unsigned i;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    /* Cancel echo */
    speex_echo_capture(echo->state,
                       (spx_int16_t*)rec_frm,
                       (spx_int16_t*)echo->tmp_frame);

    /* Apply preprocessing per channel. */
    for (i = 0; i < echo->channel_count; i++) {
        spx_int16_t *buf = (spx_int16_t*)echo->tmp_frame;
        unsigned j;

        /* De-interleave each channel. */
        if (echo->channel_count > 1) {
            for (j = 0; j < echo->spf_per_channel; j++) {
                rec_frm[j] = echo->tmp_frame[j * echo->channel_count + i];
            }
            buf = (spx_int16_t*)rec_frm;
        }

        speex_preprocess_run(echo->preprocess[i], buf);

        if (echo->channel_count > 1) {
            for (j = 0; j < echo->spf_per_channel; j++) {
                echo->tmp_frame[j * echo->channel_count + i] = rec_frm[j];
            }
        }
    }

    pjmedia_copy_samples(rec_frm, echo->tmp_frame, echo->samples_per_frame);

    return PJ_SUCCESS;
}


#endif

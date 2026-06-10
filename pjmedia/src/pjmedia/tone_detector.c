/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 * Copyright (C) 2024      Julien Chavanton <jchavanton@gmail.com>
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
#include <pjmedia/wav_port.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/errno.h>
#include <pjmedia/wave.h>
#include <pj/assert.h>
#include <pj/file_access.h>
#include <pj/file_io.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <math.h>


#define THIS_FILE	    "tone_detector.c"
#define SIGNATURE	    PJMEDIA_SIG_PORT_WAV_WRITER

#define TONE_440HZ 0
#define TONE_480HZ 1

/* Mean-square energy gate: ~ -30 dBFS relative to int16 full-scale. */
static const float energy_min_threshold = 0.01f * 32767.0f * 32767.0f * 0.7f;

/* Per-frequency energy ratio above which a bin is considered "present". */
static const float freq_energy_ratio_threshold = 0.4f;

/* Number of consecutive matching frames required before firing the callback.
 * At 20ms/frame this is ~60ms, enough to reject speech transients. */
#define TONE_DETECT_DEBOUNCE_FRAMES 3

typedef struct goertzel_state{
        float coef;
} goertzel_state_t;

static void goertzel_state_init(goertzel_state_t *gs, int frequency, int sampling_frequency){
        gs->coef=(float)2*(float)cos(2*M_PI*((float)frequency/(float)sampling_frequency));
}

static float goertzel_state_run(goertzel_state_t *gs,int16_t  *samples, int nsamples, float mean_square_energy){
        int i;
        float tmp;
        float q1=0;
        float q2=0;
        float freq_en;

        for(i=0;i<nsamples;++i){
                tmp=q1;
                q1=(gs->coef*q1) - q2 + (float)samples[i];
                q2=tmp;
        }

        freq_en= (q1*q1) + (q2*q2) - (q1*q2*gs->coef);
        /* Normalize: bin energy / (mean-square * nsamples^2 * 0.5).
         * Equivalent to bin / total-signal-energy / (nsamples * 0.5),
         * but expressed in terms of mean-square so the caller's gate is
         * frame-size independent. */
        return freq_en / (mean_square_energy * (float)nsamples * (float)nsamples * 0.5f);
}

/* Returns mean-square energy (sum of squares divided by sample count) so the
 * threshold is independent of frame size. */
static float compute_energy(int16_t *samples, int nsamples){
        float en=0;
        int i;
        for(i=0;i<nsamples;++i){
                float s=(float)samples[i];
                en+=s*s;
        }
        return en / (float)nsamples;
}

struct tone_detector_port {
    pjmedia_port     base;
    pj_bool_t	     cb_called;
    unsigned	     consecutive_hits;
    goertzel_state_t    state[2];
    pj_status_t	     (*cb)(pjmedia_port*, void*);
};

static pj_status_t process_frame(pjmedia_port *this_port, pjmedia_frame *frame);
static pj_status_t tone_detector_on_destroy(pjmedia_port *this_port);

/*
 * Create tone detector port.
 */
PJ_DEF(pj_status_t) pjmedia_tone_detector_port_create( pj_pool_t *pool,
						     const char *filename,
						     unsigned sampling_rate,
						     unsigned channel_count,
						     unsigned samples_per_frame,
						     unsigned bits_per_sample,
						     unsigned flags,
						     pj_ssize_t buff_size,
						     pjmedia_port **p_port,
						     pj_status_t (*cb)(pjmedia_port *port, void *usr_data),
						     void *cb_user_data)
{
    struct tone_detector_port *td_port;
    pj_str_t name;

    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(buff_size);

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && filename && p_port, PJ_EINVAL);

    /* Only supports 16bits per sample for now. */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    td_port = PJ_POOL_ZALLOC_T(pool, struct tone_detector_port);
    PJ_ASSERT_RETURN(td_port != NULL, PJ_ENOMEM);

    /* Initialize port info. */
    pj_strdup2(pool, &name, filename);
    pjmedia_port_info_init(&td_port->base.info, &name, SIGNATURE,
			   sampling_rate, channel_count, bits_per_sample,
			   samples_per_frame);

    goertzel_state_init(&td_port->state[0], 480, sampling_rate);
    goertzel_state_init(&td_port->state[1], 440, sampling_rate);

    td_port->base.put_frame = &process_frame;
    td_port->base.on_destroy = &tone_detector_on_destroy;

    *p_port = &td_port->base;

    td_port->base.port_data.pdata = cb_user_data;
    td_port->cb = cb;
    td_port->cb_called = PJ_FALSE;
    td_port->consecutive_hits = 0;
    return PJ_SUCCESS;
}

static pj_status_t process_frame(pjmedia_port *this_port, pjmedia_frame *frame)
{
	struct tone_detector_port *td_port = (struct tone_detector_port *)this_port;
	pj_int16_t *src;
	int nsamples;
	float en;
	pj_bool_t hit = PJ_FALSE;

	if (!this_port || !frame || td_port->cb_called) {
		return PJ_SUCCESS;
	}

	src = (pj_int16_t*)frame->buf;
	nsamples = (int)(frame->size / 2);
	en = compute_energy(src, nsamples);

	if (en > energy_min_threshold) {
		float r1 = goertzel_state_run(&td_port->state[0], src, nsamples, en);
		float r2 = goertzel_state_run(&td_port->state[1], src, nsamples, en);
		hit = (r1 >= freq_energy_ratio_threshold) &&
		      (r2 >= freq_energy_ratio_threshold);
		PJ_LOG(5,(THIS_FILE, "process_frame energy[%f] Hz1[%f] Hz2[%f] hit=%d streak=%u",
		          en, r1, r2, hit, td_port->consecutive_hits));
	}

	if (hit) {
		td_port->consecutive_hits++;
	} else {
		td_port->consecutive_hits = 0;
	}

	if (td_port->consecutive_hits >= TONE_DETECT_DEBOUNCE_FRAMES && td_port->cb) {
		td_port->cb_called = PJ_TRUE;
		(*td_port->cb)(&td_port->base, td_port->base.port_data.pdata);
	}
	return PJ_SUCCESS;
}

static pj_status_t tone_detector_on_destroy(pjmedia_port *this_port)
{
	PJ_UNUSED_ARG(this_port);
	PJ_LOG(4,(THIS_FILE, "tone detector on destroy"));
	return PJ_SUCCESS;
}


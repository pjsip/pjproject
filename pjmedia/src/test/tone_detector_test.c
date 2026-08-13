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
#include <pjmedia/tone_detector.h>
#include <pjmedia/event.h>
#include <pjmedia/port.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include "test.h"

#include <math.h>
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#define THIS_FILE       "tone_detector_test.c"

#define CLOCK_RATE      8000
#define CHANNEL_COUNT   1
#define SAMPLES_PER_FRAME (CLOCK_RATE / 50)   /* 20ms ptime */
#define BITS_PER_SAMPLE 16

/* Detection state shared with the callback. */
typedef struct test_ctx_t {
    int cb_count;
    unsigned last_duration_ms;
    unsigned last_n_freqs;
    unsigned last_freqs[PJMEDIA_TONE_DETECT_MAX_FREQS];
} test_ctx_t;

static void on_tone_detected(pjmedia_port *port, void *user_data,
                             const pjmedia_tone_detect_event *event)
{
    test_ctx_t *ctx = (test_ctx_t*)user_data;
    unsigned i;

    PJ_UNUSED_ARG(port);
    ctx->cb_count++;
    ctx->last_duration_ms = event->duration_ms;
    ctx->last_n_freqs = event->n_freqs;
    for (i = 0; i < event->n_freqs && i < PJMEDIA_TONE_DETECT_MAX_FREQS; ++i)
        ctx->last_freqs[i] = event->freqs[i];
}

/* Fill a 20ms frame with the sum of the given sine tones at half full-scale
 * each, so even with two tones the signal stays inside int16 range. */
static void synth_tones(pj_int16_t *buf, unsigned nsamples,
                        const unsigned *freqs, unsigned n_freqs,
                        unsigned sample_offset)
{
    unsigned i, f;
    const float amp = 16000.0f / (float)n_freqs;

    for (i = 0; i < nsamples; ++i) {
        float s = 0.0f;
        const float t = (float)(sample_offset + i) / (float)CLOCK_RATE;
        for (f = 0; f < n_freqs; ++f) {
            s += amp * sinf(2.0f * (float)M_PI * (float)freqs[f] * t);
        }
        buf[i] = (pj_int16_t)s;
    }
}

static void synth_silence(pj_int16_t *buf, unsigned nsamples)
{
    pj_bzero(buf, nsamples * sizeof(pj_int16_t));
}

static int feed_frames(pjmedia_port *port, pj_int16_t *buf, unsigned nframes,
                       pjmedia_frame_type type)
{
    pjmedia_frame frame;
    unsigned i;
    pj_status_t status;

    pj_bzero(&frame, sizeof(frame));
    frame.type = type;
    frame.buf = buf;
    frame.size = SAMPLES_PER_FRAME * sizeof(pj_int16_t);

    for (i = 0; i < nframes; ++i) {
        status = pjmedia_port_put_frame(port, &frame);
        PJ_TEST_SUCCESS(status, "put_frame", return -1);
    }
    return 0;
}

/* Give pjmedia's event worker time to dispatch any PJMEDIA_EVENT_CALLBACK
 * that process_frame queued via PJMEDIA_EVENT_PUBLISH_POST_EVENT, so the
 * callback runs (and updates ctx) before we assert. */
static void drain_events(void)
{
    pj_thread_sleep(100);
}

int tone_detector_test(void)
{
    pj_pool_t *pool;
    pjmedia_port *port = NULL;
    pj_int16_t *frame_buf;
    test_ctx_t ctx;
    const unsigned freqs[] = { 440u, 480u };
    unsigned sample_offset = 0;
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "tone_detector_test()"));

    pool = pj_pool_create(mem, "tone_det_test", 4000, 4000, NULL);
    PJ_TEST_NOT_NULL(pool, "pool create", return -10);

    frame_buf = (pj_int16_t*)pj_pool_alloc(pool,
                                          SAMPLES_PER_FRAME * sizeof(pj_int16_t));
    PJ_TEST_NOT_NULL(frame_buf, "frame buf alloc", {rc=-20; goto cleanup;});

    pj_bzero(&ctx, sizeof(ctx));

    status = pjmedia_tone_detector_port_create(pool, CLOCK_RATE, CHANNEL_COUNT,
                                               SAMPLES_PER_FRAME, BITS_PER_SAMPLE,
                                               freqs, PJ_ARRAY_SIZE(freqs),
                                               &port, &on_tone_detected, &ctx);
    PJ_TEST_SUCCESS(status, "port create", {rc=-30; goto cleanup;});

    /* 1. Silence must not trigger detection. */
    synth_silence(frame_buf, SAMPLES_PER_FRAME);
    if (feed_frames(port, frame_buf,
                    PJMEDIA_TONE_DETECT_DEBOUNCE_FRAMES + 2,
                    PJMEDIA_FRAME_TYPE_AUDIO) != 0)
    {
        rc = -40;
        goto cleanup;
    }
    drain_events();
    PJ_TEST_EQ(ctx.cb_count, 0, NULL, {rc=-41; goto cleanup;});

    /* 2. Non-AUDIO frames must be skipped (no NULL deref, no false detect). */
    if (feed_frames(port, NULL, 3, PJMEDIA_FRAME_TYPE_NONE) != 0) {
        rc = -50;
        goto cleanup;
    }
    drain_events();
    PJ_TEST_EQ(ctx.cb_count, 0, NULL, {rc=-51; goto cleanup;});

    /* 3. Sustained 440+480 tone must trigger exactly once. Feed extra
     *    frames after detection to confirm the callback is one-shot. */
    {
        const unsigned nframes = PJMEDIA_TONE_DETECT_DEBOUNCE_FRAMES + 5;
        unsigned f;
        for (f = 0; f < nframes; ++f) {
            synth_tones(frame_buf, SAMPLES_PER_FRAME,
                        freqs, PJ_ARRAY_SIZE(freqs), sample_offset);
            sample_offset += SAMPLES_PER_FRAME;
            if (feed_frames(port, frame_buf, 1,
                            PJMEDIA_FRAME_TYPE_AUDIO) != 0)
            {
                rc = -60;
                goto cleanup;
            }
        }
    }
    drain_events();
    PJ_TEST_EQ(ctx.cb_count, 1, "exactly one detection",
               {rc=-61; goto cleanup;});
    PJ_TEST_EQ(ctx.last_n_freqs, PJ_ARRAY_SIZE(freqs),
               "reported freq count", {rc=-62; goto cleanup;});
    PJ_TEST_EQ(ctx.last_freqs[0], 440u, "freq 0", {rc=-63; goto cleanup;});
    PJ_TEST_EQ(ctx.last_freqs[1], 480u, "freq 1", {rc=-64; goto cleanup;});

cleanup:
    if (port)
        pjmedia_port_destroy(port);
    pj_pool_release(pool);
    return rc;
}

/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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

/*
 * Contributed by:
 *  Julien Chavanton <jchavanton@gmail.com>
 */
#include <pjmedia/tone_detector.h>
#include <pjmedia/errno.h>
#include <pjmedia/event.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <math.h>

/* MSVC and some Windows toolchains don't define M_PI in <math.h> unless
 * _USE_MATH_DEFINES is set before the include. Provide a fallback. */
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif


#define THIS_FILE           "tone_detector.c"
#define SIGNATURE           PJMEDIA_SIG_PORT_TONEDET
#define TONE_DET_PORT_NAME  "tone_det"

/* Mean-square energy gate: ~ -30 dBFS relative to int16 full-scale. */
static const float energy_min_threshold = 0.01f * 32767.0f * 32767.0f * 0.7f;

/* Per-frequency energy ratio above which a bin is considered "present". */
static const float freq_energy_ratio_threshold = 0.4f;

typedef struct goertzel_state {
    float coef;
} goertzel_state_t;

static void goertzel_state_init(goertzel_state_t *gs, int frequency,
                                int sampling_frequency)
{
    gs->coef = (float)2 * (float)cos(2 * M_PI *
                                     ((float)frequency / (float)sampling_frequency));
}

static float goertzel_state_run(goertzel_state_t *gs, pj_int16_t *samples,
                                int nsamples, float mean_square_energy)
{
    int i;
    float tmp;
    float q1 = 0;
    float q2 = 0;
    float freq_en;

    for (i = 0; i < nsamples; ++i) {
        tmp = q1;
        q1 = (gs->coef * q1) - q2 + (float)samples[i];
        q2 = tmp;
    }

    freq_en = (q1 * q1) + (q2 * q2) - (q1 * q2 * gs->coef);
    /* Normalize: bin energy / (mean-square * nsamples^2 * 0.5).
     * Equivalent to bin / total-signal-energy / (nsamples * 0.5),
     * but expressed in terms of mean-square so the caller's gate is
     * frame-size independent. */
    return freq_en / (mean_square_energy * (float)nsamples *
                      (float)nsamples * 0.5f);
}

/* Returns mean-square energy (sum of squares divided by sample count) so the
 * threshold is independent of frame size. */
static float compute_energy(pj_int16_t *samples, int nsamples)
{
    float en = 0;
    int i;
    for (i = 0; i < nsamples; ++i) {
        float s = (float)samples[i];
        en += s * s;
    }
    return en / (float)nsamples;
}

/* The detection payload must fit in pjmedia_event.data.user[] so it can be
 * copied through the event queue. PJMEDIA_TONE_DETECT_MAX_FREQS is sized
 * (4) to keep sizeof(pjmedia_tone_detect_event) well under the event slot.
 * Verified at runtime in pjmedia_tone_detector_port_create(). */
struct tone_detector_port {
    pjmedia_port     base;
    pj_bool_t        subscribed;
    pj_bool_t        cb_called;
    unsigned         consecutive_hits;
    unsigned         n_freqs;
    unsigned         freqs[PJMEDIA_TONE_DETECT_MAX_FREQS];
    goertzel_state_t state[PJMEDIA_TONE_DETECT_MAX_FREQS];
    pj_time_val      start_ts;
    void           (*cb)(pjmedia_port*, void*,
                         const pjmedia_tone_detect_event*);
};

static pj_status_t process_frame(pjmedia_port *this_port, pjmedia_frame *frame);
static pj_status_t tone_detector_on_destroy(pjmedia_port *this_port);

/*
 * Event dispatcher: pjmedia delivers PJMEDIA_EVENT_CALLBACK on the main
 * pjmedia event thread, so the user callback runs outside the conf bridge
 * worker thread and is free to call back into pjsua/pjmedia.
 *
 * The detection payload travels inside event->data.user, copied at publish
 * time. That avoids a shared mutable struct in the port and the cross-thread
 * data race that would come with it.
 */
static pj_status_t tone_detector_on_event(pjmedia_event *event, void *user_data)
{
    struct tone_detector_port *td_port = (struct tone_detector_port*)user_data;

    if (event->type == PJMEDIA_EVENT_CALLBACK && td_port->cb) {
        const pjmedia_tone_detect_event *payload =
            (const pjmedia_tone_detect_event*)&event->data.user;
        (*td_port->cb)(&td_port->base, td_port->base.port_data.pdata, payload);
    }
    return PJ_SUCCESS;
}

/*
 * Create tone detector port.
 */
PJ_DEF(pj_status_t) pjmedia_tone_detector_port_create(
                                pj_pool_t *pool,
                                unsigned sampling_rate,
                                unsigned channel_count,
                                unsigned samples_per_frame,
                                unsigned bits_per_sample,
                                const unsigned *freqs,
                                unsigned n_freqs,
                                pjmedia_port **p_port,
                                void (*cb)(pjmedia_port *port,
                                           void *usr_data,
                                           const pjmedia_tone_detect_event *event),
                                void *cb_user_data)
{
    struct tone_detector_port *td_port;
    pj_str_t name = pj_str((char*)TONE_DET_PORT_NAME);
    unsigned i;

    PJ_ASSERT_RETURN(pool && p_port && freqs && cb, PJ_EINVAL);
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);
    PJ_ASSERT_RETURN(n_freqs >= 1 && n_freqs <= PJMEDIA_TONE_DETECT_MAX_FREQS,
                     PJ_EINVAL);
    /* Payload travels in event->data.user; verify it fits. */
    PJ_ASSERT_RETURN(sizeof(pjmedia_tone_detect_event) <=
                     sizeof(pjmedia_event_user_data), PJ_EBUG);

    /* The Goertzel filter treats the buffer as a contiguous sample stream;
     * interleaved stereo would corrupt detection. Always declare ourselves
     * mono — pjmedia_conf_add_port() will downmix a stereo bridge for us. */
    channel_count = 1;

    td_port = PJ_POOL_ZALLOC_T(pool, struct tone_detector_port);
    PJ_ASSERT_RETURN(td_port != NULL, PJ_ENOMEM);

    pjmedia_port_info_init(&td_port->base.info, &name, SIGNATURE,
                           sampling_rate, channel_count, bits_per_sample,
                           samples_per_frame);

    td_port->n_freqs = n_freqs;
    for (i = 0; i < n_freqs; ++i) {
        td_port->freqs[i] = freqs[i];
        goertzel_state_init(&td_port->state[i], (int)freqs[i],
                            (int)sampling_rate);
    }
    pj_gettimeofday(&td_port->start_ts);

    td_port->base.put_frame = &process_frame;
    td_port->base.on_destroy = &tone_detector_on_destroy;
    td_port->base.port_data.pdata = cb_user_data;
    td_port->cb = cb;

    /* Subscribe to pjmedia events up front so the lone failure path is
     * reported synchronously here, rather than per-frame after debounce. */
    {
        pj_status_t status;
        status = pjmedia_event_subscribe(NULL, &tone_detector_on_event,
                                         td_port, td_port);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(2,(THIS_FILE, status,
                         "Failed to subscribe to pjmedia events"));
            return status;
        }
        td_port->subscribed = PJ_TRUE;
    }

    *p_port = &td_port->base;
    return PJ_SUCCESS;
}

static pj_status_t process_frame(pjmedia_port *this_port, pjmedia_frame *frame)
{
    struct tone_detector_port *td_port = (struct tone_detector_port *)this_port;
    pj_int16_t *src;
    int nsamples;
    float en;
    pj_bool_t hit = PJ_FALSE;
    unsigned i;

    if (!this_port || !frame || td_port->cb_called) {
        return PJ_SUCCESS;
    }
    /* The conference bridge can feed us silence/CN frames with type !=
     * PJMEDIA_FRAME_TYPE_AUDIO, a NULL buffer, or zero size. Skip those
     * to avoid dereferencing NULL or dividing by zero in compute_energy. */
    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO || !frame->buf ||
        frame->size < sizeof(pj_int16_t))
    {
        return PJ_SUCCESS;
    }

    src = (pj_int16_t*)frame->buf;
    nsamples = (int)(frame->size / sizeof(pj_int16_t));
    en = compute_energy(src, nsamples);

    if (en > energy_min_threshold) {
        hit = PJ_TRUE;
        for (i = 0; i < td_port->n_freqs; ++i) {
            float r = goertzel_state_run(&td_port->state[i], src, nsamples, en);
            if (r < freq_energy_ratio_threshold) {
                hit = PJ_FALSE;
                /* keep looping only for the log; cheap relative to per-frame
                 * work */
            }
            PJ_LOG(5,(THIS_FILE, "process_frame energy[%f] f=%uHz r=%f",
                      en, td_port->freqs[i], r));
        }
        PJ_LOG(5,(THIS_FILE, "process_frame hit=%d streak=%u", hit,
                  td_port->consecutive_hits));
    }

    if (hit) {
        td_port->consecutive_hits++;
    } else {
        td_port->consecutive_hits = 0;
    }

    if (td_port->consecutive_hits >= PJMEDIA_TONE_DETECT_DEBOUNCE_FRAMES &&
        td_port->cb && !td_port->cb_called)
    {
        pj_time_val now, diff;
        pjmedia_event ev;
        pjmedia_tone_detect_event *payload;

        td_port->cb_called = PJ_TRUE;

        pj_gettimeofday(&now);
        diff = now;
        PJ_TIME_VAL_SUB(diff, td_port->start_ts);

        /* Build the payload directly inside the event so it travels
         * through the pjmedia event queue (which crosses thread
         * boundaries with proper synchronization). No shared mutable
         * state in the port struct. */
        pjmedia_event_init(&ev, PJMEDIA_EVENT_CALLBACK, NULL, td_port);
        payload = (pjmedia_tone_detect_event*)&ev.data.user;
        pj_bzero(payload, sizeof(*payload));
        payload->n_freqs = td_port->n_freqs;
        for (i = 0; i < td_port->n_freqs; ++i)
            payload->freqs[i] = td_port->freqs[i];
        payload->duration_ms = (unsigned)PJ_TIME_VAL_MSEC(diff);

        pjmedia_event_publish(NULL, td_port, &ev,
                              PJMEDIA_EVENT_PUBLISH_POST_EVENT);
    }
    return PJ_SUCCESS;
}

static pj_status_t tone_detector_on_destroy(pjmedia_port *this_port)
{
    struct tone_detector_port *td_port = (struct tone_detector_port*)this_port;

    if (td_port->subscribed) {
        pjmedia_event_unsubscribe(NULL, &tone_detector_on_event,
                                  td_port, td_port);
        td_port->subscribed = PJ_FALSE;
    }
    PJ_LOG(4,(THIS_FILE, "tone detector destroyed"));
    return PJ_SUCCESS;
}

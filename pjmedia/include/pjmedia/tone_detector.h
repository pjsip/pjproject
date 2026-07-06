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
#ifndef __PJMEDIA_TONE_DETECTOR_H__
#define __PJMEDIA_TONE_DETECTOR_H__

/**
 * @file tone_detector.h
 * @brief Goertzel-based call-progress tone detector port.
 */
#include <pjmedia/port.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJMEDIA_TONE_DETECT Tone Detector
 * @ingroup PJMEDIA_PORT
 * @brief Frequency-selective call-progress tone detector
 * @{
 *
 * The tone detector is a sink port that runs a Goertzel filter on each
 * incoming audio frame for one to a few configured frequencies, and fires
 * a callback the first time all frequencies are simultaneously present
 * for ~60ms. It is intended for narrowband call-progress tones (US
 * ringback 440+480, dial tone 350+440, busy 480+620, SIT segments,
 * 1004Hz milliwatt, etc.) that aren't covered by DTMF detection.
 */


/** Maximum simultaneous frequencies a tone detector can watch. */
#define PJMEDIA_TONE_DETECT_MAX_FREQS 4

/**
 * Number of consecutive matching frames required before the detector fires.
 * The effective debounce duration is
 *   PJMEDIA_TONE_DETECT_DEBOUNCE_FRAMES * samples_per_frame / clock_rate
 * — at the default 20ms ptime that's roughly 60ms, which is short enough
 * to feel responsive on ringback yet long enough to reject speech transients.
 */
#ifndef PJMEDIA_TONE_DETECT_DEBOUNCE_FRAMES
#  define PJMEDIA_TONE_DETECT_DEBOUNCE_FRAMES 3
#endif


/**
 * Reported back to the application when the detector fires.
 */
typedef struct pjmedia_tone_detect_event
{
    unsigned freqs[PJMEDIA_TONE_DETECT_MAX_FREQS]; /**< Frequencies watched (Hz). */
    unsigned n_freqs;                              /**< Number of entries used. */
    unsigned duration_ms;                          /**< Time from port creation
                                                        to detection (ms).      */
} pjmedia_tone_detect_event;


/**
 * Create a media port that watches the audio stream for the simultaneous
 * presence (AND) of \a n_freqs frequencies and fires \a cb the first time the
 * combined tone is sustained for PJMEDIA_TONE_DETECT_DEBOUNCE_FRAMES
 * consecutive frames (≈60ms at the default 20ms ptime; scales with the
 * configured \a samples_per_frame and \a clock_rate).
 *
 * The port consumes frames (put_frame) but does not produce them; it can be
 * connected to a conference bridge as a sink in place of a recorder.
 *
 * @param pool              Pool to allocate the port from.
 * @param clock_rate        The sampling rate (Hz).
 * @param channel_count     Conf bridge channel count, accepted but ignored:
 *                          the detector always runs as mono internally and
 *                          relies on pjmedia_conf_add_port() to downmix.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Must be 16.
 * @param freqs             Array of frequencies (Hz) to watch.
 * @param n_freqs           Number of frequencies (1..PJMEDIA_TONE_DETECT_MAX_FREQS).
 * @param p_port            Receives the created port.
 * @param cb                Callback fired on first sustained detection.
 *                          Delivered via the pjmedia event mechanism, so it
 *                          runs on the pjmedia event thread (not the conf
 *                          bridge worker). The event pointer references
 *                          internal storage and is valid only for the
 *                          duration of the callback; do not retain it.
 * @param usr_data          Opaque user data passed back to \a cb.
 *
 * @return                  PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_tone_detector_port_create(pj_pool_t *pool,
                                                    unsigned clock_rate,
                                                    unsigned channel_count,
                                                    unsigned samples_per_frame,
                                                    unsigned bits_per_sample,
                                                    const unsigned *freqs,
                                                    unsigned n_freqs,
                                                    pjmedia_port **p_port,
                                                    void (*cb)(pjmedia_port *port,
                                                               void *usr_data,
                                                               const pjmedia_tone_detect_event *event),
                                                    void *usr_data);


/**
 * @}
 */


PJ_END_DECL


#endif  /* __PJMEDIA_TONE_DETECTOR_H__ */

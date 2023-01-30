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
#ifndef __PJMEDIA_ECHO_H__
#define __PJMEDIA_ECHO_H__


/**
 * @file echo.h
 * @brief Echo Cancellation  API.
 */
#include <pjmedia/types.h>



/**
 * @defgroup PJMEDIA_Echo_Cancel Accoustic Echo Cancellation API
 * @ingroup PJMEDIA_PORT
 * @brief Echo Cancellation API.
 * @{
 *
 * This section describes API to perform echo cancellation to audio signal.
 * There may be multiple echo canceller implementation in PJMEDIA, ranging
 * from simple echo suppressor to a full Accoustic Echo Canceller/AEC. By 
 * using this API, application should be able to use which EC backend to
 * use base on the requirement and capability of the platform.
 */


PJ_BEGIN_DECL


/**
 * Opaque type for PJMEDIA Echo Canceller state.
 */
typedef struct pjmedia_echo_state pjmedia_echo_state;


/**
 * Echo cancellation options.
 */
typedef enum pjmedia_echo_flag
{
    /**
     * Use any available backend echo canceller algorithm. This is
     * the default settings. You can only choose one backend.
     */
    PJMEDIA_ECHO_DEFAULT= 0,

    /**
     * Force to use Speex AEC as the backend echo canceller algorithm.
     * You can only choose one backend.
     */
    PJMEDIA_ECHO_SPEEX  = 1,

    /**
     * If PJMEDIA_ECHO_SIMPLE flag is specified during echo canceller
     * creation, then a simple echo suppressor will be used instead of
     * an accoustic echo cancellation. You can only choose one backend.
     */
    PJMEDIA_ECHO_SIMPLE = 2,

    /**
     * Force to use WebRTC AEC as the backend echo canceller algorithm.
     * You can only choose one backend.
     */
    PJMEDIA_ECHO_WEBRTC = 3,

    /**
     * Force to use WebRTC AEC3 as the backend echo canceller algorithm.
     * You can only choose one backend.
     */
    PJMEDIA_ECHO_WEBRTC_AEC3 = 4,

    /**
     * For internal use.
     */
    PJMEDIA_ECHO_ALGO_MASK = 15,

    /**
     * If PJMEDIA_ECHO_NO_LOCK flag is specified, no mutex will be created
     * for the echo canceller, but application will guarantee that echo
     * canceller will not be called by different threads at the same time.
     */
    PJMEDIA_ECHO_NO_LOCK = 16,

    /**
     * If PJMEDIA_ECHO_USE_SIMPLE_FIFO flag is specified, the delay buffer
     * created for the echo canceller will use simple FIFO mechanism, i.e.
     * without using WSOLA to expand and shrink audio samples.
     */
    PJMEDIA_ECHO_USE_SIMPLE_FIFO = 32,

    /**
     * If PJMEDIA_ECHO_USE_SW_ECHO flag is specified, software echo canceller
     * will be used instead of device EC.
     */
    PJMEDIA_ECHO_USE_SW_ECHO = 64,
    
    /**
     * If PJMEDIA_ECHO_USE_NOISE_SUPPRESSOR flag is specified, the echo
     * canceller will also apply noise suppressor method to reduce noise.
     */
    PJMEDIA_ECHO_USE_NOISE_SUPPRESSOR = 128,

    /**
     * If PJMEDIA_ECHO_USE_GAIN_CONTROLLER flag is specified, the echo
     * canceller will also apply automatic gain control.
     */
    PJMEDIA_ECHO_USE_GAIN_CONTROLLER = 256,
    
    /**
     * Use default aggressiveness setting for the echo canceller algorithm. 
     * This setting is mutually exclusive with the other aggressiveness
     * settings.
     */
    PJMEDIA_ECHO_AGGRESSIVENESS_DEFAULT = 0,
    
    /**
     * Use conservative aggressiveness setting for the echo canceller
     * algorithm. This setting is mutually exclusive with the other
     * aggressiveness settings.
     */
    PJMEDIA_ECHO_AGGRESSIVENESS_CONSERVATIVE = 0x1000,
    
    /**
     * Use moderate aggressiveness setting for the echo canceller algorithm. 
     * This setting is mutually exclusive with the other aggressiveness
     * settings.
     */
    PJMEDIA_ECHO_AGGRESSIVENESS_MODERATE = 0x2000,
    
    /**
     * Use aggressive aggressiveness setting for the echo canceller
     * algorithm. This setting is mutually exclusive with the other
     * aggressiveness settings.
     */
    PJMEDIA_ECHO_AGGRESSIVENESS_AGGRESSIVE = 0x3000,
    
    /**
     * For internal use.
     */
    PJMEDIA_ECHO_AGGRESSIVENESS_MASK = 0xF000

} pjmedia_echo_flag;


/** Statistic not specified. */
#define PJMEDIA_ECHO_STAT_NOT_SPECIFIED         999999

/**
 * Echo cancellation statistics.
 */
typedef struct pjmedia_echo_stat
{
    /**
     * The name of the EC backend.
     * NULL if not specified.
     */    
    const char *name;

    /**
     * Echo delay value (in ms).
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    int         delay;

    /**
     * Echo return loss.
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    double      return_loss;

    /**
     * Echo return loss enhancement.
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    double      return_loss_enh;

    /**
     * Echo delay standard deviation (in ms).
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    int         std;

    /**
     * Fraction of poor delay. Value between 0 to 1. The closer to 1,
     * the poorer the EC quality.
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    float       frac_delay;

    /**
     * Learning still in progress? PJ_TRUE if yes, false if done.
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    unsigned    learning;

    /**
     * Learning duration (in ms).
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    unsigned    duration;

    /**
     * Detected echo tail length (in ms).
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    unsigned    tail;

    /**
     * Minimum scaling factor (in ms).
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    int         min_factor;

    /**
     * Average scaling factor (in ms).
     * PJMEDIA_ECHO_STAT_NOT_SPECIFIED if unavailable.
     */
    int         avg_factor;

    /**
     * Text describing the statistic.
     */
    pj_str_t    stat_info;

    /**
     * Internal buffer.
     */
    char        buf_[128];

} pjmedia_echo_stat;


/**
 * Initialize Echo cancellation stat.
 *
 * @param stat              The statistic to be initialized.
 */
PJ_DECL(void) pjmedia_echo_stat_default(pjmedia_echo_stat *stat);


/**
 * Create the echo canceller. 
 *
 * @param pool              Pool to allocate memory.
 * @param clock_rate        Media clock rate/sampling rate.
 * @param samples_per_frame Number of samples per frame.
 * @param tail_ms           Tail length, miliseconds.
 * @param latency_ms        Total lacency introduced by playback and 
 *                          recording device. Set to zero if the latency
 *                          is not known.
 * @param options           Options. If PJMEDIA_ECHO_SIMPLE is specified,
 *                          then a simple echo suppressor implementation 
 *                          will be used instead of an accoustic echo 
 *                          cancellation.
 *                          See #pjmedia_echo_flag for other options.
 * @param p_echo            Pointer to receive the Echo Canceller state.
 *
 * @return                  PJ_SUCCESS on success, or the appropriate status.
 */
PJ_DECL(pj_status_t) pjmedia_echo_create(pj_pool_t *pool,
                                         unsigned clock_rate,
                                         unsigned samples_per_frame,
                                         unsigned tail_ms,
                                         unsigned latency_ms,
                                         unsigned options,
                                         pjmedia_echo_state **p_echo );

/**
 * Create multi-channel the echo canceller. 
 *
 * @param pool              Pool to allocate memory.
 * @param clock_rate        Media clock rate/sampling rate.
 * @param channel_count     Number of channels.
 * @param samples_per_frame Number of samples per frame.
 * @param tail_ms           Tail length, miliseconds.
 * @param latency_ms        Total lacency introduced by playback and 
 *                          recording device. Set to zero if the latency
 *                          is not known.
 * @param options           Options. If PJMEDIA_ECHO_SIMPLE is specified,
 *                          then a simple echo suppressor implementation 
 *                          will be used instead of an accoustic echo 
 *                          cancellation.
 *                          See #pjmedia_echo_flag for other options.
 * @param p_echo            Pointer to receive the Echo Canceller state.
 *
 * @return                  PJ_SUCCESS on success, or the appropriate status.
 */
PJ_DECL(pj_status_t) pjmedia_echo_create2(pj_pool_t *pool,
                                          unsigned clock_rate,
                                          unsigned channel_count,
                                          unsigned samples_per_frame,
                                          unsigned tail_ms,
                                          unsigned latency_ms,
                                          unsigned options,
                                          pjmedia_echo_state **p_echo );

/**
 * Destroy the Echo Canceller. 
 *
 * @param echo          The Echo Canceller.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_echo_destroy(pjmedia_echo_state *echo );


/**
 * Reset the echo canceller.
 *
 * @param echo          The Echo Canceller.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_echo_reset(pjmedia_echo_state *echo );


/**
 * Get the echo canceller statistics.
 *
 * @param echo          The Echo Canceller.
 * @param p_stat        Pointer to receive the stat.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_echo_get_stat(pjmedia_echo_state *echo,
                                           pjmedia_echo_stat *p_stat);


/**
 * Let the Echo Canceller know that a frame has been played to the speaker.
 * The Echo Canceller will keep the frame in its internal buffer, to be used
 * when cancelling the echo with #pjmedia_echo_capture().
 *
 * @param echo          The Echo Canceller.
 * @param play_frm      Sample buffer containing frame to be played
 *                      (or has been played) to the playback device.
 *                      The frame must contain exactly samples_per_frame 
 *                      number of samples.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_echo_playback(pjmedia_echo_state *echo,
                                           pj_int16_t *play_frm );


/**
 * Let the Echo Canceller know that a frame has been captured from the 
 * microphone. The Echo Canceller will cancel the echo from the captured
 * signal, using the internal buffer (supplied by #pjmedia_echo_playback())
 * as the FES (Far End Speech) reference.
 *
 * @param echo          The Echo Canceller.
 * @param rec_frm       On input, it contains the input signal (captured 
 *                      from microphone) which echo is to be removed.
 *                      Upon returning this function, this buffer contain
 *                      the processed signal with the echo removed.
 *                      The frame must contain exactly samples_per_frame 
 *                      number of samples.
 * @param options       Echo cancellation options, reserved for future use.
 *                      Put zero for now.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_echo_capture(pjmedia_echo_state *echo,
                                          pj_int16_t *rec_frm,
                                          unsigned options );


/**
 * Perform echo cancellation.
 *
 * @param echo          The Echo Canceller.
 * @param rec_frm       On input, it contains the input signal (captured 
 *                      from microphone) which echo is to be removed.
 *                      Upon returning this function, this buffer contain
 *                      the processed signal with the echo removed.
 * @param play_frm      Sample buffer containing frame to be played
 *                      (or has been played) to the playback device.
 *                      The frame must contain exactly samples_per_frame 
 *                      number of samples.
 * @param options       Echo cancellation options, reserved for future use.
 *                      Put zero for now.
 * @param reserved      Reserved for future use, put NULL for now.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_echo_cancel( pjmedia_echo_state *echo,
                                          pj_int16_t *rec_frm,
                                          const pj_int16_t *play_frm,
                                          unsigned options,
                                          void *reserved );


PJ_END_DECL

/**
 * @}
 */


#endif  /* __PJMEDIA_ECHO_H__ */


/*
 * Copyright (C) 2025 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_AV_SYNC_H__
#define __PJMEDIA_AV_SYNC_H__

 /**
  * @file av_sync.h
  * @brief Media Presentation Synchronization.
  */
#include <pjmedia/types.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJMEDIA_AV_SYNC Media Presentation Synchronization
 * @ingroup PJMEDIA_PORT_CLOCK
 * @brief Synchronize media presentation
 * @{
 */


/*
 * How it works in general:
 * 1. App creates AV sync using pjmedia_av_sync_create().
 * 2. App adds all media to be synchronized using pjmedia_av_sync_add_media().
 * 3. Each time a media receives an RTCP-SR, update AV sync using
 *    pjmedia_av_sync_update_ref().
 * 4. Each time a media returns a frame to be rendered, e.g: via
 *    port.get_frame(), update AV sync using pjmedia_av_sync_update_pts(),
 *    the function will return a number:
 *    - zero    : nothing to do,
 *    - positive: increase delay as many as the returned number,
 *    - negative: decrease delay as many as the returned number.
 *
 * Logic in pjmedia_av_sync_update_pts():
 * - Calculate the absolute/NTP timestamp of the frame playback based on
     the reference NTP and the supplied frame RTP timestamp. This timestamp
     is usually called presentation time (pts).
 * - If pts is the largest or most recent, set it as AV sync's max_ntp.
 *   Otherwise, calculate the difference/delay of this pts to the max_ntp,
 *   then request the media to speed up as much as the delay.
 * - If after some requests the delay is still relatively high, request
 *   the fastest media to slow down instead.
 *
 * Changes needed in stream:
 * 1. Update ref whenever receive RTCP-SR.
 * 2. Update presentation time whenever returns a frame (to play/render).
 * 3. Add mechanism to calculate current delay & optimal delay from
 *    jitter buffer size.
 * 4. When AV sync requests to speed up, as long as the end delay is
 *    not lower than the optimal delay, do it.
 * 5. When AV sync requests to slow down, do it, i.e: increase and maintain
 *    the jitter buffer size.
 */


/**
 * AV sync, opaque.
 */
typedef struct pjmedia_av_sync pjmedia_av_sync;


/**
 * Media, opaque.
 */
typedef struct pjmedia_av_sync_media pjmedia_av_sync_media;


/**
 * Media setting.
 */
typedef struct {
    char                       *name;
    unsigned                    clock_rate;
} pjmedia_av_sync_media_setting;


/**
 * Get default settings for media.
 */
PJ_DECL(void) pjmedia_av_sync_media_setting_default(
                                pjmedia_av_sync_media_setting *setting);

/**
 * Create media synchronizer.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_create(
                                pjmedia_endpt *endpt,
                                const void *setting,
                                pjmedia_av_sync **av_sync);


/**
 * Destroy media synchronizer.
 */
PJ_DECL(void) pjmedia_av_sync_destroy(pjmedia_av_sync* av_sync);


/**
 * Add media to synchronizer.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_add_media(
                                pjmedia_av_sync* av_sync,
                                const pjmedia_av_sync_media_setting* setting,
                                pjmedia_av_sync_media** av_sync_media);


/**
 * Remove media from synchronizer.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_del_media(
                                pjmedia_av_sync *av_sync,
                                pjmedia_av_sync_media *av_sync_media);


/**
 * Update synchronizer about the last presentation timestamp of the specified
 * media.
 *
 * Normally this function is called after a media renderer (video renderer or
 * speaker) invokes get_frame() of the media source (e.g: audio/video stream).
 *
 * @return      0  : media is in sync, no action is needed.
 *              <0 : speed up presentation by returned value milliseconds.
 *              >0 : slow down presentation by returned value milliseconds.
 */
PJ_DECL(pj_int32_t) pjmedia_av_sync_update_pts(
                                pjmedia_av_sync_media *av_sync_media,
                                const pj_timestamp *pts);


/**
 * Update synchronizer about the last timestamp reference of the specified
 * media.
 *
 * Normally this function is called after a media source receive RTCP-SR
 * with info of an NTP timestamp correspoding to an RTP timestamp.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_update_ref(
                                pjmedia_av_sync_media *av_sync_media,
                                const pj_timestamp *ntp,
                                const pj_timestamp *ts);



/**
 * @}
 */


PJ_END_DECL


#endif  /* __PJMEDIA_AV_SYNC_H__ */

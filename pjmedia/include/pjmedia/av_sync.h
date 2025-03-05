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
  * @brief Inter-media Synchronization.
  */
#include <pjmedia/types.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJMEDIA_AV_SYNC Inter-media Synchronization
 * @ingroup PJMEDIA_SESSION
 * @brief Synchronize presentation time of multiple media in a session.
 * @{
 *
 * A call session may consist of multiple media, e.g: some audio and some
 * video, which frequently have different delays when presented in the
 * receiver side. This module synchronizes all media in the same session
 * based on NTP timestamp & RTP timestamp info provided by the sender in
 * RTCP SR.
 *
 * Here are steps to use this module:
 * 1. Create AV sync using #pjmedia_av_sync_create().
 * 2. Adds all media to be synchronized using #pjmedia_av_sync_add_media().
 * 3. Call #pjmedia_av_sync_update_ref() each time the media receiving
 *    an RTCP SR packet.
 * 4. Call #pjmedia_av_sync_update_pts() each time the media returning
 *    a frame to be presented, e.g: via port.get_frame(). See more about
 *    this function below.
 * 5. Call #pjmedia_av_sync_del_media() when a media is removed from the
 *    session.
 * 6. Call #pjmedia_av_sync_destroy() when the session is ended.
 * 
 * More about #pjmedia_av_sync_update_pts():
 * - The function will return non-zero when delay adjustment in the media is
 *   needed for synchronization. When it returns a positive number, the media
 *   should increase the delay as many as the returned number, in millisecond.
 *   When it returns a negative number, the media should try to decrease
 *   the delay as many as the returned number, in millisecond.
 *   For example in audio stream, the delay adjustment can managed using
 *   #pjmedia_jbuf_set_min_delay().
 * - The function will keep track on the delay adjustment progress, for
 *   example, after some delay adjustment are requested to a media and
 *   the delay of the media is still relatively high, it will trigger a
 *   request to the fastest media to slow down.
 */


/**
 * Inter-media synchronizer, opaque.
 */
typedef struct pjmedia_av_sync pjmedia_av_sync;


/**
 * Media synchronization handle, opaque.
 */
typedef struct pjmedia_av_sync_media pjmedia_av_sync_media;


/**
 * Media settings.
 */
typedef struct {
    /**
     * Name of the media
     */
    char                       *name;

    /**
     * Media clock rate or sampling rate.
     */
    unsigned                    clock_rate;
} pjmedia_av_sync_media_setting;


/**
 * Get default settings for media.
 *
 * @param setting           The media setting.
 */
PJ_DECL(void) pjmedia_av_sync_media_setting_default(
                                pjmedia_av_sync_media_setting *setting);

/**
 * Create media synchronizer.
 *
 * @param endpt             The media endpoint.
 * @param setting           Synchronization setting, must be NULL for now.
 * @param av_sync           The pointer to receive the media synchronizer.
 *
 * @return                  PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_create(
                                pjmedia_endpt *endpt,
                                const void *setting,
                                pjmedia_av_sync **av_sync);


/**
 * Destroy media synchronizer.
 *
 * @param av_sync           The media synchronizer.
 */
PJ_DECL(void) pjmedia_av_sync_destroy(pjmedia_av_sync* av_sync);


/**
 * Add a media to synchronizer.
 *
 * @param av_sync           The media synchronizer.
 * @param setting           The media setting.
 * @param av_sync_media     The pointer to receive the media synchronization
 *                          handle.
 *
 * @return                  PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_add_media(
                                pjmedia_av_sync* av_sync,
                                const pjmedia_av_sync_media_setting* setting,
                                pjmedia_av_sync_media** av_sync_media);


/**
 * Remove a media from synchronizer.
 *
 * @param av_sync           The media synchronizer.
 * @param av_sync_media     The media synchronization handle.
 *
 * @return                  PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_del_media(
                                pjmedia_av_sync *av_sync,
                                pjmedia_av_sync_media *av_sync_media);


/**
 * Update synchronizer about the last presentation timestamp of the specified
 * media. Normally this function is called when media is producing a frame
 * to be rendered (e.g: to a video renderer or audio device speaker).
 * The media may be requested to adjust its playback delay by the number
 * returned by the function, for example audio stream can adjust delay
 * using #pjmedia_jbuf_set_min_delay().
 *
 * @param av_sync_media     The media synchronization handle.
 * @param pts               The presentation timestamp.
 *
 * @return                  0  : no action is needed, because the media is
 *                               already synchronized or the calculation
 *                               is not completed (e.g: need more data).
 *                          <0 : decrease playback delay by returned number,
 *                               in milliseconds.
 *                          >0 : increase playback delay by returned number,
 *                               in milliseconds.
 */
PJ_DECL(pj_int32_t) pjmedia_av_sync_update_pts(
                                pjmedia_av_sync_media *av_sync_media,
                                const pj_timestamp *pts);


/**
 * Update synchronizer about reference timestamps of the specified media.
 * Normally this function is called after a media receives RTCP SR packet.
 *
 * @param av_sync_media     The media synchronization handle.
 * @param ntp               The NTP timestamp info from RTCP SR.
 * @param ts                The RTP timestamp info from RTCP SR.
 *
 * @return                  PJ_SUCCESS on success.
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

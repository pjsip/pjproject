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
 *    a frame to be presented, e.g: via port.get_frame(). The function may
 *    request the media to adjust its delay.
 * 5. Call #pjmedia_av_sync_del_media() when a media is removed from the
 *    session.
 * 6. Call #pjmedia_av_sync_destroy() when the session is ended.
 * 
 * The primary synchronization logic is implemented within the
 * #pjmedia_av_sync_update_pts() function. This function will calculate
 * the lag between the calling media to the earliest media and will provide
 * a feedback to the calling media whether it is in synchronized state,
 * late, or early so the media can respond accordingly.
 * Initially this function will try to request slower media to speed up.
 * If after a specific number of requests (i.e: configurable via
 * PJMEDIA_AVSYNC_MAX_SPEEDUP_REQ_CNT) and the lag is still beyond a tolerable
 * value (i.e: configurable via PJMEDIA_AVSYNC_MAX_TOLERABLE_LAG_MSEC), the
 * function will issue slow down request to the fastest media.
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
 * Synchronizer settings.
 */
typedef struct {
    /**
     * Name of the syncrhonizer
     */
    char                       *name;

    /**
     * Streaming mode. If set to PJ_TRUE, the delay adjustment values will
     * be smoothened and marked up to prevent possible delay increase on
     * all media.
     */
    pj_bool_t                   is_streaming;

} pjmedia_av_sync_setting;


/**
 * Media settings.
 */
typedef struct {
    /**
     * Name of the media
     */
    char                       *name;

    /**
     * Media type.
     */
    pjmedia_type                type;

    /**
     * Media clock rate or sampling rate.
     */
    unsigned                    clock_rate;

} pjmedia_av_sync_media_setting;


/**
 * Get default settings for synchronizer.
 *
 * @param setting           The synchronizer settings.
 */
PJ_DECL(void) pjmedia_av_sync_setting_default(
                                pjmedia_av_sync_setting *setting);

/**
 * Get default settings for media.
 *
 * @param setting           The media settings.
 */
PJ_DECL(void) pjmedia_av_sync_media_setting_default(
                                pjmedia_av_sync_media_setting *setting);

/**
 * Create media synchronizer.
 *
 * @param pool              The memory pool.
 * @param option            The synchronizer settings.
 * @param av_sync           The pointer to receive the media synchronizer.
 *
 * @return                  PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_create(
                                pj_pool_t *pool,
                                const pjmedia_av_sync_setting *setting,
                                pjmedia_av_sync **av_sync);


/**
 * Destroy media synchronizer.
 *
 * @param av_sync           The media synchronizer.
 */
PJ_DECL(void) pjmedia_av_sync_destroy(pjmedia_av_sync *av_sync);


/**
 * Reset synchronization states. Any existing media will NOT be removed,
 * but their states will be reset.
 *
 * @param av_sync           The media synchronizer.
 *
 * @return                  PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_reset(pjmedia_av_sync *av_sync);


/**
 * Add a media to synchronizer.
 *
 * @param av_sync           The media synchronizer.
 * @param setting           The media settings.
 * @param av_sync_media     The pointer to receive the media synchronization
 *                          handle.
 *
 * @return                  PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_add_media(
                                pjmedia_av_sync* av_sync,
                                const pjmedia_av_sync_media_setting *setting,
                                pjmedia_av_sync_media **av_sync_media);


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
 * media. Normally this function is called each time the media produces
 * a frame to be rendered (e.g: in port's get_frame() method). Upon returning,
 * the media may be requested to adjust its delay so it matches to the
 * earliest or the latest media, i.e: by speeding up or slowing down.
 *
 * Initially this function will try to request slower media to speed up.
 * If after a specific number of requests (i.e: configurable via
 * PJMEDIA_AVSYNC_MAX_SPEEDUP_REQ_CNT) and the lag is still beyond a tolerable
 * value (i.e: configurable via PJMEDIA_AVSYNC_MAX_TOLERABLE_LAG_MSEC), the
 * function will issue slow down request to the fastest media.
 *
 * @param av_sync_media     The media synchronization handle.
 * @param pts               The presentation timestamp.
 * @param adjust_delay      Optional pointer to receive adjustment delay
 *                          required, in milliseconds, to make this media
 *                          synchronized to the fastest media.
 *                          Possible output values are:
 *                          0 when no action is needed,
 *                          possitive value when increasing delay is needed,
 *                          or negative value when decreasing delay is needed.
 *
 * @return                  PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_av_sync_update_pts(
                                pjmedia_av_sync_media *av_sync_media,
                                const pj_timestamp *pts,
                                pj_int32_t *adjust_delay);


/**
 * Update synchronizer about reference timestamps of the specified media.
 * Normally this function is called each time the media receives RTCP SR
 * packet.
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

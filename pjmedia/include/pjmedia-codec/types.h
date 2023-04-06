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
#ifndef __PJMEDIA_CODEC_TYPES_H__
#define __PJMEDIA_CODEC_TYPES_H__

/**
 * @file types.h
 * @brief PJMEDIA-CODEC types and constants
 */

#include <pjmedia-codec/config.h>
#include <pjmedia/codec.h>

/**
 * @defgroup pjmedia_codec_types PJMEDIA-CODEC Types and Constants
 * @ingroup PJMEDIA_CODEC
 * @brief Constants used by PJMEDIA-CODEC
 * @{
 */



/**
 * These are the dynamic payload types that are used by audio codecs in
 * this library. Also see the header file <pjmedia/codec.h> for list
 * of static payload types.
 *
 * These enumeration is for older audio codecs only, newer audio codec using
 * dynamic payload type can simply assign PJMEDIA_RTP_PT_DYNAMIC in its
 * payload type (i.e: pjmedia_codec_info.pt). Endpoint will automatically
 * rearrange dynamic payload types in SDP generation.
 */
enum pjmedia_audio_pt
{
    /* According to IANA specifications, dynamic payload types are to be in
     * the range 96-127 (inclusive), but this enum allows the value to be
     * outside that range, later endpoint will rearrange dynamic payload types
     * in SDP generation to be inside the 96-127 range and not equal to
     * PJMEDIA_RTP_PT_TELEPHONE_EVENTS.
     *
     * PJMEDIA_RTP_PT_DYNAMIC is defined in <pjmedia/codec.h>. It is defined
     * to be 96.
     */
    PJMEDIA_RTP_PT_START = (PJMEDIA_RTP_PT_DYNAMIC-1),

    PJMEDIA_RTP_PT_SPEEX_NB,                    /**< Speex narrowband/8KHz  */
    PJMEDIA_RTP_PT_SPEEX_WB,                    /**< Speex wideband/16KHz   */
    PJMEDIA_RTP_PT_SPEEX_UWB,                   /**< Speex 32KHz            */
    PJMEDIA_RTP_PT_SILK_NB,                     /**< SILK narrowband/8KHz   */
    PJMEDIA_RTP_PT_SILK_MB,                     /**< SILK mediumband/12KHz  */
    PJMEDIA_RTP_PT_SILK_WB,                     /**< SILK wideband/16KHz    */
    PJMEDIA_RTP_PT_SILK_SWB,                    /**< SILK 24KHz             */
    PJMEDIA_RTP_PT_ILBC,                        /**< iLBC (13.3/15.2Kbps)   */
    PJMEDIA_RTP_PT_AMR,                         /**< AMR (4.75 - 12.2Kbps)  */
    PJMEDIA_RTP_PT_AMRWB,                       /**< AMRWB (6.6 - 23.85Kbps)*/
    PJMEDIA_RTP_PT_AMRWBE,                      /**< AMRWBE                 */
    PJMEDIA_RTP_PT_G726_16,                     /**< G726 @ 16Kbps          */
    PJMEDIA_RTP_PT_G726_24,                     /**< G726 @ 24Kbps          */
    PJMEDIA_RTP_PT_G726_32,                     /**< G726 @ 32Kbps          */
    PJMEDIA_RTP_PT_G726_40,                     /**< G726 @ 40Kbps          */
    PJMEDIA_RTP_PT_G722_1_16,                   /**< G722.1 (16Kbps)        */
    PJMEDIA_RTP_PT_G722_1_24,                   /**< G722.1 (24Kbps)        */
    PJMEDIA_RTP_PT_G722_1_32,                   /**< G722.1 (32Kbps)        */
    PJMEDIA_RTP_PT_G7221C_24,                   /**< G722.1 Annex C (24Kbps)*/
    PJMEDIA_RTP_PT_G7221C_32,                   /**< G722.1 Annex C (32Kbps)*/
    PJMEDIA_RTP_PT_G7221C_48,                   /**< G722.1 Annex C (48Kbps)*/
    PJMEDIA_RTP_PT_G7221_RSV1,                  /**< G722.1 reserve         */
    PJMEDIA_RTP_PT_G7221_RSV2,                  /**< G722.1 reserve         */
    PJMEDIA_RTP_PT_OPUS,                        /**< OPUS                   */
#if PJMEDIA_CODEC_L16_HAS_8KHZ_MONO
    PJMEDIA_RTP_PT_L16_8KHZ_MONO,               /**< L16 @ 8KHz, mono       */
#endif
#if PJMEDIA_CODEC_L16_HAS_8KHZ_STEREO
    PJMEDIA_RTP_PT_L16_8KHZ_STEREO,             /**< L16 @ 8KHz, stereo     */
#endif
    //PJMEDIA_RTP_PT_L16_11KHZ_MONO,            /**< L16 @ 11KHz, mono      */
    //PJMEDIA_RTP_PT_L16_11KHZ_STEREO,          /**< L16 @ 11KHz, stereo    */
#if PJMEDIA_CODEC_L16_HAS_16KHZ_MONO
    PJMEDIA_RTP_PT_L16_16KHZ_MONO,              /**< L16 @ 16KHz, mono      */
#endif
#if PJMEDIA_CODEC_L16_HAS_16KHZ_STEREO
    PJMEDIA_RTP_PT_L16_16KHZ_STEREO,            /**< L16 @ 16KHz, stereo    */
#endif
    //PJMEDIA_RTP_PT_L16_22KHZ_MONO,            /**< L16 @ 22KHz, mono      */
    //PJMEDIA_RTP_PT_L16_22KHZ_STEREO,          /**< L16 @ 22KHz, stereo    */
    //PJMEDIA_RTP_PT_L16_32KHZ_MONO,            /**< L16 @ 32KHz, mono      */
    //PJMEDIA_RTP_PT_L16_32KHZ_STEREO,          /**< L16 @ 32KHz, stereo    */
#if PJMEDIA_CODEC_L16_HAS_48KHZ_MONO
    PJMEDIA_RTP_PT_L16_48KHZ_MONO,              /**< L16 @ 48KHz, mono      */
#endif
#if PJMEDIA_CODEC_L16_HAS_48KHZ_STEREO
    PJMEDIA_RTP_PT_L16_48KHZ_STEREO,            /**< L16 @ 48KHz, stereo    */
#endif
};

/**
 * These are the dynamic payload types that are used by video codecs in
 * this library.
 */
enum pjmedia_video_pt
{
     /* Video payload types */
     PJMEDIA_RTP_PT_VID_START = (PJMEDIA_RTP_PT_DYNAMIC-1),
     PJMEDIA_RTP_PT_H263P,      /* used by ffmpeg avcodec     */
     PJMEDIA_RTP_PT_H264,       /* used by OpenH264           */
     PJMEDIA_RTP_PT_H264_RSV1,  /* used by video toolbox      */
     PJMEDIA_RTP_PT_H264_RSV2,  /* used by MediaCodec         */
     PJMEDIA_RTP_PT_H264_RSV3,  /* used by ffmpeg avcodec     */
     PJMEDIA_RTP_PT_H264_RSV4,

     PJMEDIA_RTP_PT_VP8,        /* used by VPX                */
     PJMEDIA_RTP_PT_VP8_RSV1,   /* used by MediaCodec         */
     PJMEDIA_RTP_PT_VP8_RSV2,   /* used by ffmpeg avcodec     */
     PJMEDIA_RTP_PT_VP9,        /* used by VPX                */
     PJMEDIA_RTP_PT_VP9_RSV1,   /* used by MediaCodec         */
     PJMEDIA_RTP_PT_VP9_RSV2,   /* used by ffmpeg avcodec     */

     /* Caution!
      * Ensure the value of the last pt above is <= 127.
      */
};


/**
 * @}
 */


#endif  /* __PJMEDIA_CODEC_TYPES_H__ */

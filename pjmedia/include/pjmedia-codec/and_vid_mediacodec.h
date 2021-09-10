/*
 * Copyright (C)2020 Teluu Inc. (http://www.teluu.com)
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

#ifndef __PJMEDIA_CODEC_AND_VID_MEDIACODEC_H__
#define __PJMEDIA_CODEC_AND_VID_MEDIACODEC_H__

#include <pjmedia-codec/types.h>
#include <pjmedia/vid_codec.h>

/**
 * @file pjmedia-codec/and_vid_mediacodec.h
 * @brief Android video Mediacodec codecs.
 */

PJ_BEGIN_DECL

/**
 * @defgroup PJMEDIA_HAS_ANDROID_MEDIACODEC Android Mediacodec Codec
 * @ingroup PJMEDIA_CODEC_VID_CODECS
 * @{
 *
 *
 * Video MediaCodec codec wrapper for Android.
 *
 * This codec wrapper contains varius codecs: i.e: H.264/AVC, VP8 and VP9.
 * The H.264 codec wrapper only supports non-interleaved packetization
 * mode. If remote uses a different mode (e.g: single-nal), this will cause
 * unpacketization issue and affect decoding process.
 */

/**
 * Initialize and register Android Mediacodec video codec factory.
 *
 * @param mgr	    The video codec manager instance where this codec will
 * 		    be registered to. Specify NULL to use default instance
 * 		    (in that case, an instance of video codec manager must
 * 		    have been created beforehand).
 * @param pf	    Pool factory.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_and_media_vid_init(
				    pjmedia_vid_codec_mgr *mgr,
                                    pj_pool_factory *pf);

/**
 * Unregister Android Mediacodec video codecs factory from the video codec
 * manager and deinitialize the codec library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_and_media_vid_deinit(void);


/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJMEDIA_CODEC_AND_VID_MEDIACODEC_H__ */

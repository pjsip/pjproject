/* 
 * Copyright (C) 2017 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_CODEC_VID_TOOLBOX_H__
#define __PJMEDIA_CODEC_VID_TOOLBOX_H__

#include <pjmedia-codec/types.h>
#include <pjmedia/vid_codec.h>

/**
 * @file pjmedia-codec/vid_toolbox.h
 * @brief Video Toolbox codec
 */


PJ_BEGIN_DECL

/**
 * @defgroup PJMEDIA_CODEC_VID_TOOLBOX Video Toolbox Codec
 * @ingroup PJMEDIA_CODEC_VID_CODECS
 * @{
 *
 * Video Toolbox H.264 codec wrapper for Mac and iOS.
 */

/**
 * Initialize and register Video Toolbox codec factory.
 *
 * @param mgr       The video codec manager instance where this codec will
 *                  be registered to. Specify NULL to use default instance
 *                  (in that case, an instance of video codec manager must
 *                  have been created beforehand).
 * @param pf        Pool factory.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_vid_toolbox_init(pjmedia_vid_codec_mgr *mgr,
                                                     pj_pool_factory *pf);

/**
 * Unregister Video Toolbox video codecs factory from the video codec manager
 * and deinitialize the codec library.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_vid_toolbox_deinit(void);


/**
 * @}  PJMEDIA_CODEC_VIDEO_TOOLBOX
 */


PJ_END_DECL

#endif  /* __PJMEDIA_CODEC_VID_TOOLBOX_H__ */

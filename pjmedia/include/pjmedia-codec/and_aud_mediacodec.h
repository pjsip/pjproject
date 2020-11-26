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

#ifndef __PJMEDIA_CODEC_AND_AUD_MEDIACODEC_H__
#define __PJMEDIA_CODEC_AND_AUD_MEDIACODEC_H__

/**
 * @file and_aud_mediacodec.h
 * @brief Android audio MediaCodec codec.
 */

#include <pjmedia-codec/types.h>

PJ_BEGIN_DECL

/**
 * Initialize and register Android audio MediaCodec factory to pjmedia
 * endpoint.
 *
 * @param endpt		The pjmedia endpoint.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_and_media_aud_init( pjmedia_endpt *endpt );

/**
 * Unregister Android audio MediaCodec factory from pjmedia endpoint 
 * and deinitialize the codec library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_and_media_aud_deinit( void );

PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_AND_AUD_MEDIACODEC_H__ */

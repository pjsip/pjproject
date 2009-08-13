/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_CODECS_PASSTHROUGH_H__
#define __PJMEDIA_CODECS_PASSTHROUGH_H__

/**
 * @file pjmedia-codec/passthrough.h
 * @brief Passthrough codecs.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_PASSTHROUGH_CODEC Passthrough Codecs
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of passthrough codecs
 * @{
 *
 * This section describes functions to register and register passthrough 
 * codecs factory to the codec manager. After the codec factory has been 
 * registered, application can use @ref PJMEDIA_CODEC API to manipulate 
 * the codec. This codec factory contains various codecs, e.g: G.729, iLBC,
 * AMR, and G.711.
 *
 * Passthrough codecs are codecs wrapper that does not perform encoding 
 * or decoding, it just pack and parse encoded audio data from/into RTP 
 * payload. This will accomodate pjmedia ports which work with encoded
 * audio data, e.g: encoded audio files, sound device with capability
 * of playing/recording encoded audio data.
 */

PJ_BEGIN_DECL


/** 
 * Codec passthrough configuration settings.
 */
typedef struct pjmedia_codec_passthrough_setting
{
    unsigned		 fmt_cnt;	/**< Number of encoding formats
					     to be enabled.		*/
    pjmedia_format	*fmts;		/**< Encoding formats to be 
					     enabled.			*/
    unsigned		 ilbc_mode;	/**< iLBC default mode.		*/
} pjmedia_codec_passthrough_setting;


/**
 * Initialize and register passthrough codecs factory to pjmedia endpoint,
 * all supported encoding formats will be enabled.
 *
 * @param endpt	    The pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_passthrough_init( pjmedia_endpt *endpt );


/**
 * Initialize and register passthrough codecs factory to pjmedia endpoint
 * with only specified encoding formats enabled.
 *
 * @param endpt	    The pjmedia endpoint.
 * @param setting   The settings, see @pjmedia_codec_passthrough_setting.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_passthrough_init2(
		       pjmedia_endpt *endpt,
		       const pjmedia_codec_passthrough_setting *setting);


/**
 * Unregister passthrough codecs factory from pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_passthrough_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODECS_PASSTHROUGH_H__ */


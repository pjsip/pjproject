/* 
 * Copyright (C) 2012-2012 Teluu Inc. (http://www.teluu.com)
 * Contributed by Regis Montoya (aka r3gis - www.r3gis.fr)
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
#ifndef __PJMEDIA_CODEC_SILK_H__
#define __PJMEDIA_CODEC_SILK_H__

/**
 * @file silk.h
 * @brief SILK codec.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_SILK SILK Codec Family
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of SILK codecs (narrow/medium/wide/superwide-band).
 * @{
 *
 * This section describes functions to initialize and register SILK codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * The SILK codec uses multiple bit rates, and supports super wideband 
 * (24 kHz sampling rate), wideband (16 kHz sampling rate), medium (12kHz
 * sampling rate), and narrowband (telephone quality, 8 kHz sampling rate).
 *
 * \section silk_codec_setting Codec Settings
 *
 * \subsection silk_general_setting General Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 * \subsection silk_specific_setting Codec Specific Settings
 *
 * The following settings are applicable for this codec.
 *
 * \subsubsection silk_quality_vs_complexity Quality vs Complexity
 *
 * The SILK codec quality versus computational complexity and bandwidth
 * requirement can be adjusted by modifying the quality and complexity
 * setting, by calling #pjmedia_codec_silk_set_config().
 *
 * The default setting of quality is specified in 
 * #PJMEDIA_CODEC_SILK_DEFAULT_QUALITY. And the default setting of
 * complexity is specified in #PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY.
 */

PJ_BEGIN_DECL

typedef struct pjmedia_codec_silk_setting
{
    pj_bool_t   enabled;    /**< Enable/disable.                            */
    int         quality;    /**< Encoding quality, or use -1 for default 
                                 (@see PJMEDIA_CODEC_SILK_DEFAULT_QUALITY). */
    int         complexity; /**< Encoding complexity, or use -1 for default
                                 (@see PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY)*/
} pjmedia_codec_silk_setting;


/**
 * Initialize and register SILK codec factory to pjmedia endpoint. By default,
 * only narrowband (8kHz sampling rate) and wideband (16kHz sampling rate)
 * will be enabled. Quality and complexity for those sampling rate modes
 * will be set to the default values (see #PJMEDIA_CODEC_SILK_DEFAULT_QUALITY
 * and #PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY), application may modify these
 * settings via #pjmedia_codec_silk_set_config().
 *
 * @param endpt         The pjmedia endpoint.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_silk_init(pjmedia_endpt *endpt);


/**
 * Change the configuration setting of the SILK codec for the specified
 * clock rate.
 *
 * @param clock_rate    PCM sampling rate, in Hz, valid values are 8000,
 *                      12000, 16000 and 24000.
 * @param opt           The setting to be applied for the specified
 *                      clock rate.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_silk_set_config(
                                    unsigned clock_rate, 
                                    const pjmedia_codec_silk_setting *opt);


/**
 * Unregister SILK codec factory from pjmedia endpoint and deinitialize
 * the SILK codec library.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_silk_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif  /* __PJMEDIA_CODEC_SILK_H__ */


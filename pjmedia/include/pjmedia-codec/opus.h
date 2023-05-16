/*
 * Copyright (C) 2015-2016 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2012-2015 Zaark Technology AB
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
/* This file is the header of Opus codec wrapper and was contributed by
 * Zaark Technology AB
 */

#ifndef __PJMEDIA_CODEC_OPUS_H__
#define __PJMEDIA_CODEC_OPUS_H__

/**
 * @file opus.h
 * @brief Opus codec.
 */

#include <pjmedia-codec/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJMED_OPUS Opus Codec Family
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Opus codec wrapper
 * @{
 *
 * This section describes functions to initialize and register Opus codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * Opus codec uses multiple bit rates, and supports fullband (48 kHz
 * sampling rate), super wideband (24 kHz sampling rate), wideband (16 kHz
 * sampling rate), medium band (12kHz sampling rate), and narrowband
 * (8 kHz sampling rate).
 *
 *
 * \section opus_codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param
 * (see the documentation of #pjmedia_codec_param for more info).
 *
 * For Opus codec specific settings, such as sample rate,
 * channel count, bit rate, complexity, and CBR, can be configured
 * in #pjmedia_codec_opus_config.
 * The default setting of sample rate is specified in 
 * #PJMEDIA_CODEC_OPUS_DEFAULT_SAMPLE_RATE. The default setting of
 * bitrate is specified in #PJMEDIA_CODEC_OPUS_DEFAULT_BIT_RATE.
 * And the default setting of complexity is specified in
 * #PJMEDIA_CODEC_OPUS_DEFAULT_COMPLEXITY.
 *
 * After modifying any of these settings, application needs to call
 * #pjmedia_codec_opus_set_default_param(), which will generate the
 * appropriate decoding fmtp attributes.
 *
 * Here is an example of modifying the codec settings:
 \code
    pjmedia_codec_param param;
    pjmedia_codec_opus_config opus_cfg;

    pjmedia_codec_mgr_get_default_param(.., &param);
    pjmedia_codec_opus_get_config(&opus_cfg);
    ...
    // Set VAD
    param.setting.vad = 1;
    // Set PLC
    param.setting.vad = 1;
    // Set sample rate
    opus_cfg.sample_rate = 16000;
    // Set channel count
    opus_cfg.channel_cnt = 2;
    // Set bit rate
    opus_cfg.bit_rate = 20000;
    ...
    pjmedia_codec_opus_set_default_param(&opus_cfg, &param);
 \endcode
 *
 */

/**
 * Opus codec configuration.
 */
typedef struct pjmedia_codec_opus_config
{
    unsigned   sample_rate; /**< Sample rate in Hz.                     */
    unsigned   channel_cnt; /**< Number of channels.                    */
    unsigned   frm_ptime;   /**< Frame ptime in msec.                   */
    unsigned   frm_ptime_denum;/**< Frame ptime denumerator, can be zero*/
    unsigned   bit_rate;    /**< Encoder bit rate in bps.               */
    unsigned   packet_loss; /**< Encoder's expected packet loss pct.    */
    unsigned   complexity;  /**< Encoder complexity, 0-10(10 is highest)*/
    pj_bool_t  cbr;         /**< Constant bit rate?                     */
} pjmedia_codec_opus_config;


/**
 * Initialize and register Opus codec factory to pjmedia endpoint.
 *
 * @param endpt         The pjmedia endpoint.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opus_init( pjmedia_endpt *endpt );

/**
 * Unregister Opus codec factory from pjmedia endpoint and deinitialize
 * the Opus codec library.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_opus_deinit( void );

/**
 * Get the default Opus configuration.
 *
 * @param cfg           Opus codec configuration.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_codec_opus_get_config( pjmedia_codec_opus_config *cfg );

/**
 * Set the default Opus configuration and set the default Opus codec param.
 * Note that the function will call #pjmedia_codec_mgr_set_default_param().
 *
 * @param cfg           Opus codec configuration.
 * @param param         On input, the default Opus codec parameter to be set.
 *                      On output, the current default Opus codec parameter
 *                      after setting. This may be different from the input
 *                      because some settings can be rejected, or overwritten
 *                      by the Opus codec configuration above.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_codec_opus_set_default_param(const pjmedia_codec_opus_config *cfg,
                                     pjmedia_codec_param *param );

PJ_END_DECL

/**
 * @}
 */

#endif  /* __PJMEDIA_CODEC_OPUS_H__ */

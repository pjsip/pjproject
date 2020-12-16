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
 * @brief Android audio MediaCodec codecs.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMEDIA_CODEC_AUD_MEDIACODEC Audio MediaCodec Codec
 * @ingroup PJMEDIA_CODEC_CODECS
 * @{
 *
 * Audio MediaCodec codec wrapper for Android.
 *
 * This codec wrapper contains varius codecs: i.e: AMR and AMR-WB.
 *
 * \section pjmedia_codec_mediacodec_AMR MediaCodec AMRNB/AMR-WB
 *
 * MediaCodec AMR supports 16-bit PCM audio signal with sampling rate 8000Hz,
 * 20ms frame length and producing various bitrates that ranges from 4.75kbps
 * to 12.2kbps.
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param.
 * Please see the documentation of #pjmedia_codec_param for more info.
 * Note that MediaCodec doesn't provide internal VAD/PLC feature, they will be
 * provided by PJMEDIA instead.
 *
 * \subsubsection bitrate Bitrate
 *
 * By default, encoding bitrate is 7400bps. This default setting can be
 * modified using #pjmedia_codec_mgr_set_default_param() by specifying
 * prefered AMR bitrate in field <tt>info::avg_bps</tt> of
 * #pjmedia_codec_param. Valid bitrates could be seen in
 * #pjmedia_codec_amrnb_bitrates.
 *
 * \subsubsection payload_format Payload Format
 *
 * There are two AMR payload format types, bandwidth-efficient and
 * octet-aligned. Default setting is using octet-aligned. This default payload
 * format can be modified using #pjmedia_codec_mgr_set_default_param().
 *
 * In #pjmedia_codec_param, payload format can be set by specifying SDP
 * format parameters "octet-align" in the SDP "a=fmtp" attribute for
 * decoding direction. Valid values are "0" (for bandwidth efficient mode)
 * and "1" (for octet-aligned mode).
 *
 * \subsubsection mode_set Mode-Set
 *
 * Mode-set is used for restricting AMR modes in decoding direction.
 *
 * By default, no mode-set restriction applied. This default setting can be
 * be modified using #pjmedia_codec_mgr_set_default_param().
 *
 * In #pjmedia_codec_param, mode-set could be specified via format parameters
 * "mode-set" in the SDP "a=fmtp" attribute for decoding direction. Valid
 * value is a comma separated list of modes from the set 0 - 7, e.g:
 * "4,5,6,7". When this parameter is omitted, no mode-set restrictions applied.
 *
 * Here is an example of modifying AMR default codec param:
 \code
    pjmedia_codec_param param;

    pjmedia_codec_mgr_get_default_param(.., &param);
    ...
    // set default encoding bitrate to the highest 12.2kbps
    param.info.avg_bps = 12200;

    // restrict decoding bitrate to 10.2kbps and 12.2kbps only
    param.setting.dec_fmtp.param[0].name = pj_str("mode-set");
    param.setting.dec_fmtp.param[0].val  = pj_str("6,7");

    // also set to use bandwidth-efficient payload format
    param.setting.dec_fmtp.param[1].name = pj_str("octet-align");
    param.setting.dec_fmtp.param[1].val  = pj_str("0");

    param.setting.dec_fmtp.cnt = 2;
    ...
    pjmedia_codec_mgr_set_default_param(.., &param);
 \endcode
 */

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

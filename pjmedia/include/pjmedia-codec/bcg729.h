/* $Id$ */
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
#ifndef __PJMEDIA_CODEC_BCG729_H__
#define __PJMEDIA_CODEC_BCG729_H__

/**
 * @file bcg729.h
 * @brief BCG729 codec.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_BCG729 BCG729 Codec
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of BCG729 codecs.
 * @{
 *
 * This section describes functions to initialize and register BCG729 codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * This codec factory contains G.729 codec.
 * 
 * \section pjmedia_codec_bcg729 BCG729
 *
 * BCG729 is compliant with ITU-T G.729 and Annexes A, B specifications.
 *
 * BCGG729 supports 16-bit PCM audio signal with sampling rate 8000Hz, 
 * frame length 10ms, and resulting in bitrate 8000bps.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 * Note that G.729 VAD status should be signalled in SDP, see more
 * description below.
 *
 * \subsubsection annexb Annex B
 *
 * The capability of VAD/DTX is specified in Annex B.
 *
 * By default, Annex B is enabled. This default setting of Annex B can 
 * be modified using #pjmedia_codec_mgr_set_default_param().
 *
 * In #pjmedia_codec_param, Annex B is configured via VAD setting and
 * format parameter "annexb" in the SDP "a=fmtp" attribute in
 * decoding fmtp field. Valid values are "yes" and "no",
 * the implementation default is "yes". When this parameter is omitted
 * in the SDP, the value will be "yes" (RFC 4856 Section 2.1.9).
 *
 * Here is an example of modifying default setting of Annex B to
 * be disabled using #pjmedia_codec_mgr_set_default_param():
 \code
    pjmedia_codec_param param;

    pjmedia_codec_mgr_get_default_param(.., &param);
    ...
    // Set VAD
    param.setting.vad = 0;
    // Set SDP format parameter
    param.setting.dec_fmtp.cnt = 1;
    param.setting.dec_fmtp.param[0].name = pj_str("annexb");
    param.setting.dec_fmtp.param[0].val  = pj_str("no");
    ...
    pjmedia_codec_mgr_set_default_param(.., &param);
 \endcode
 *
 * \note
 * The difference of Annex B status in SDP offer/answer may be considered as 
 * incompatible codec in SDP negotiation.
 *
 */

PJ_BEGIN_DECL

/**
 * Initialize and register BCG729 codec factory to pjmedia endpoint. 
 *
 * @param endpt		The pjmedia endpoint.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_bcg729_init(pjmedia_endpt *endpt);

/**
 * Unregister BCG729 codec factory from pjmedia endpoint and deinitialize
 * the BCG729 codec library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_bcg729_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_BCG729_H__ */


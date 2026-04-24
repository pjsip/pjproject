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
 *
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
 * \subsection opus_codec_config_fields Configuration Fields
 *
 * The fields of #pjmedia_codec_opus_config control encoder behavior and the
 * decoder fmtp advertised in the SDP offer:
 *
 * - \b sample_rate: Opus internal clock rate in Hz. Valid values are 8000,
 *   12000, 16000, 24000, and 48000. Defaults to
 *   #PJMEDIA_CODEC_OPUS_DEFAULT_SAMPLE_RATE (48000). When not 48000 this
 *   is advertised in both \a maxplaybackrate and \a sprop-maxcapturerate
 *   fmtp (see \ref opus_sdp_fmtp).
 * - \b channel_cnt: 1 (mono) or 2 (stereo). When set to 2, the
 *   \a stereo / \a sprop-stereo fmtp parameters are advertised so remote
 *   endpoints know to send/expect stereo frames.
 * - \b frm_ptime and \b frm_ptime_denum: Encoder frame ptime in msec, with
 *   optional denominator to express fractional values. Use frm_ptime=20,
 *   frm_ptime_denum=0 for the 20 ms default. To express 2.5 ms, use
 *   frm_ptime=5 and frm_ptime_denum=2.
 * - \b bit_rate: Encoder target bit rate in bps. 0
 *   (#PJMEDIA_CODEC_OPUS_DEFAULT_BIT_RATE) lets Opus pick based on sample
 *   rate, channel count, and content. Non-zero values are advertised as
 *   \a maxaveragebitrate. On the receive side, a remote's advertised
 *   \a maxaveragebitrate is clamped to the RFC 7587 range
 *   [6000, 510000] bps with a log warning.
 * - \b packet_loss: Expected network packet loss percentage. Passed to
 *   Opus as the \a OPUS_SET_PACKET_LOSS_PERC encoder hint so Opus can tune
 *   its internal resilience. This field does not itself advertise any
 *   fmtp; the \a useinbandfec attribute is driven by the PLC setting in
 *   #pjmedia_codec_param (see below).
 * - \b complexity: Encoder complexity 0..10. Higher yields better quality
 *   at the cost of CPU. Default #PJMEDIA_CODEC_OPUS_DEFAULT_COMPLEXITY (5).
 * - \b cbr: Constant vs variable bit rate. Default
 *   #PJMEDIA_CODEC_OPUS_DEFAULT_CBR (PJ_FALSE, i.e. VBR). When PJ_TRUE
 *   the \a cbr=1 fmtp is advertised.
 *
 *
 * \section opus_on_the_fly Changing Parameters During a Call
 *
 * Added in \pr{3189}. Opus accepts
 * changes to bit rate, bandwidth (clock rate), CBR, VAD/DTX, PLC/FEC,
 * packet loss, complexity, and ptime mid-call, without renegotiating
 * SDP. Other codecs typically accept only the VAD/PLC settings via the
 * same API.
 *
 * The entry points at each API layer:
 *
 * - \b PJMEDIA: #pjmedia_stream_modify_codec_param() accepts a new
 *   #pjmedia_codec_param and applies whatever the codec supports
 *   changing at run-time.
 * - \b PJSUA-LIB: #pjsua_call_aud_stream_modify_codec_param() wraps the
 *   above for a given call and media index.
 * - \b PJSUA2: \a Call::audStreamModifyCodecParam() is the C++ binding
 *   of the PJSUA-LIB function.
 *
 * A typical use is adjusting the bit rate when network conditions change.
 * Fetch the current codec param from the stream (or construct one from
 * the current #pjmedia_codec_opus_config), patch the fields of interest,
 * and pass it back.
 *
 *
 * \section opus_pjsua2 PJSUA2 API
 *
 * In PJSUA2 (added in \pr{3935}), the
 * Opus configuration is exposed as \a pj::CodecOpusConfig and manipulated
 * through the endpoint:
 *
 * - \a Endpoint::getCodecOpusConfig() reads the current default
 *   configuration.
 * - \a Endpoint::setCodecOpusConfig() installs a new default configuration
 *   and regenerates the SDP fmtp attributes, equivalent to calling
 *   #pjmedia_codec_opus_set_default_param() at the C level.
 *
 * \a pj::CodecOpusConfig mirrors #pjmedia_codec_opus_config field by field,
 * so the descriptions above apply to both. For on-the-fly changes during a
 * call, use \a Call::audStreamModifyCodecParam() as noted above.
 *
 *
 * \section opus_sdp_fmtp SDP Format Parameters
 *
 * The Opus codec negotiates the following parameters via
 * <tt>a=fmtp</tt> lines in the SDP (see RFC 7587). They are generated
 * automatically from the #pjmedia_codec_opus_config and the VAD/PLC bits
 * in #pjmedia_codec_param by #pjmedia_codec_opus_set_default_param();
 * applications rarely set them by hand.
 *
 * Driven by #pjmedia_codec_opus_config:
 *
 * - \a maxplaybackrate and \a sprop-maxcapturerate: both advertised when
 *   \a sample_rate is not 48000, reflecting the endpoint's decode (and
 *   mirror capture) clock rate.
 * - \a stereo and \a sprop-stereo: advertised when \a channel_cnt is 2.
 *   \a stereo tells the remote "you may send stereo to me";
 *   \a sprop-stereo tells the remote "I may send stereo to you".
 * - \a maxaveragebitrate: advertised only when \a bit_rate is non-zero.
 *   On the decoding side, the value received from the remote is clamped
 *   to a practical range and a log warning is emitted if adjusted.
 * - \a cbr: advertised as \a cbr=1 when \a cbr is PJ_TRUE, otherwise
 *   omitted (variable bit rate).
 *
 * Note that the effective session clock rate and channel count are
 * negotiated as the minimum of what each side advertises: the session
 * clock rate becomes min(remote's \a maxplaybackrate,
 * local's \a sprop-maxcapturerate) and the channel count becomes
 * min(remote's \a stereo, local's \a sprop-stereo). If the remote
 * omits either attribute, RFC 7587 section 7 defaults apply: 48000 Hz
 * and mono. Therefore, to actually run stereo or a non-48000 rate both
 * peers must advertise it.
 *
 * Driven by #pjmedia_codec_param settings:
 *
 * - \a useinbandfec: advertised as \a useinbandfec=1 when
 *   \a setting.plc is non-zero, signalling that in-band FEC is enabled
 *   on the encoder. This pairs with #pjmedia_codec_opus_config::packet_loss,
 *   which is passed to Opus as the expected loss percentage so FEC is
 *   actually produced.
 * - \a usedtx: advertised as \a usedtx=1 when \a setting.vad is non-zero,
 *   i.e. the encoder is allowed to skip frames during silence.
 *
 * When interoperating with WebRTC peers or SIP gateways that are strict
 * about Opus parameters, keep \a sample_rate at 48000 and leave
 * \a bit_rate at 0 unless you have a specific reason to override.
 *
 *
 * \section opus_troubleshooting Troubleshooting
 *
 * - \b One-way \b audio \b or \b truncated \b frames \b after \b on-the-fly
 *   \b changes: \a param.info.max_rx_frame_size must fit the largest
 *   Opus frame the remote may produce after a VBR&rarr;CBR, ptime, or
 *   sampling rate change. PJSIP sets a safe default (1275 bytes, Opus's
 *   worst case); this bullet applies if an application has explicitly
 *   lowered it. See \issue{2089} for background.
 * - \b Stereo \b not \b negotiated: Stereo runs only when both sides
 *   advertise it (see \ref opus_sdp_fmtp). Local \a channel_cnt must be
 *   2 so \a sprop-stereo is advertised; if the remote omits \a stereo
 *   or sends \a stereo=0, the negotiated channel count is mono. Inspect
 *   the SDP log to confirm what each side advertised.
 * - \b Clock \b rate \b lower \b than \b expected: Similarly, the
 *   session clock rate is the minimum of remote \a maxplaybackrate and
 *   local \a sprop-maxcapturerate, defaulting to 48000 per RFC 7587
 *   when the remote omits the attribute.
 * - \b MSVC \b link \b errors \b on \b libopus: see
 *   \ref opus_windows_linking below.
 *
 *
 * \section opus_windows_linking Windows Linking
 *
 * On MSVC, the codec source automatically links to the Opus library using
 * \a \#pragma \a comment(lib,...). The default library name is \a "opus.lib",
 * which is the standard convention for MSVC builds of the Opus library.
 *
 * If the Opus library has a different name (e.g., when built with MinGW
 * producing \a "libopus.a"), define #PJMEDIA_CODEC_OPUS_LIB_NAME in
 * \a config_site.h before enabling the codec:
 *
 \code
    // In config_site.h
    #define PJMEDIA_CODEC_OPUS_LIB_NAME  "libopus.a"
    #define PJMEDIA_HAS_OPUS_CODEC       1
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

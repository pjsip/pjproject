/*
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_CODEC_LYRA_H__
#define __PJMEDIA_CODEC_LYRA_H__

 /**
  * @file pjmedia-codec/lyra.hpp
  * @brief lyra codec.
  */

#include <pjmedia-codec/types.h>

  /**
   * @defgroup PJMED_LYRA Lyra Codec (experimental)
   * @ingroup PJMEDIA_CODEC_CODECS
   * @brief Implementation of Lyra Codec
   * @{
   *
   * \warning Lyra is **experimental** in PJSIP. It is not a
   * standardised codec — there is no RFC, no IANA-registered MIME
   * type, and no formally specified SDP / RTP payload format. The
   * \a fmtp:bitrate=N parameter PJSIP advertises (see
   * \ref lyra_sdp_fmtp) is an internal convention, not an
   * interoperable spec; expect interop limited to peers running the
   * same PJSIP Lyra integration.
   *
   * Lyra is Google's open-source neural speech codec
   * (https://github.com/google/lyra), targeting low bit rates
   * (3200, 6000, or 9200 bps) at speech-quality fidelity by running
   * a TFLite model on every frame. Audio characteristics:
   *
   * - 16-bit PCM input.
   * - 20 ms frame length.
   * - Clock rates 8000 / 16000 / 32000 / 48000 Hz, individually
   *   gated by compile-time switches (see \ref lyra_build_pjsip).
   *
   * Once registered with #pjmedia_codec_lyra_init() the codec is
   * driven through the standard @ref PJMEDIA_CODEC API; the rest of
   * this section describes Lyra-specific settings, build
   * integration, and the runtime model files Lyra needs.
   *
   * Initial integration: \pr{3949}.
   *
   *
   * \section lyra_codec_setting Codec Settings
   *
   * General codec settings (VAD, PLC) live in \a setting on
   * #pjmedia_codec_param. Lyra-specific settings (bit rate, model
   * path) live in #pjmedia_codec_lyra_config and are applied via:
   *
   \code
      pjmedia_codec_lyra_config lyra_cfg;
      pjmedia_codec_lyra_get_config(&lyra_cfg);

      lyra_cfg.bit_rate   = 6000;
      lyra_cfg.model_path = pj_str("/var/data/lyra/model_coeffs");

      pjmedia_codec_lyra_set_config(&lyra_cfg);
   \endcode
   *
   * Defaults come from the compile-time
   * #PJMEDIA_CODEC_LYRA_DEFAULT_BIT_RATE and
   * #PJMEDIA_CODEC_LYRA_DEFAULT_MODEL_PATH macros, applied at
   * #pjmedia_codec_lyra_init() time.
   *
   *
   * \subsection lyra_codec_config_fields Configuration Fields
   *
   * The fields of #pjmedia_codec_lyra_config:
   *
   * - \b bit_rate: The local endpoint's <em>decoder</em> bit rate,
   *   advertised to the remote so it knows what rate to encode at.
   *   Valid values are 3200, 6000, and 9200 bps. The two endpoints
   *   may pick different decoder bit rates: each side reads the
   *   other's advertised \a bitrate and encodes accordingly, so a
   *   3200&nbsp;↔&nbsp;6000 asymmetric pairing is a normal
   *   configuration.
   * - \b model_path: Folder containing the four Lyra model files.
   *   If the path is invalid (folder missing or files absent),
   *   codec creation fails. See \ref lyra_model_files for the
   *   file list and deployment implications.
   *
   *
   * \section lyra_sdp_fmtp SDP Format Parameters
   *
   * Lyra is offered with a single fmtp parameter:
   *
   \code
      m=audio 4000 RTP/AVP 99
      a=rtpmap:99 lyra/16000/1
      a=fmtp:99 bitrate=3200
   \endcode
   *
   * The payload type (here 99) is dynamic; the rtpmap encoding name
   * is \a "lyra" and the clock rate is one of the enabled clock
   * rates. The \a fmtp \a bitrate parameter carries the local
   * endpoint's decoder bit rate from
   * #pjmedia_codec_lyra_config.bit_rate.
   *
   * Asymmetric bit rates work naturally — each side advertises its
   * own decoder rate, the peer encodes at that rate. The bit rate
   * cannot be renegotiated mid-stream; change it via SDP re-INVITE.
   *
   * \warning The \a "lyra" encoding name and the \a bitrate fmtp
   * parameter are not registered with IANA and are not specified in
   * any RFC. Interop with non-PJSIP Lyra integrations is not
   * guaranteed.
   *
   *
   * \section lyra_build_lib Building the Lyra Library
   *
   * Lyra is an external dependency that must be built and installed
   * before PJSIP can link against it. Authoritative build
   * instructions live with the Lyra project:
   *
   *   https://github.com/google/lyra
   *
   * Lyra uses Bazel (not autotools or CMake) and depends on Abseil,
   * gulrak filesystem, and glog. PJSIP's autoconf \a --with-lyra
   * flag expects the prefix to expose those headers under
   * \a include/com_google_absl, \a include/gulrak_filesystem, and
   * \a include/com_google_glog/src — see \a aconfigure.ac for the
   * exact \a -I paths assembled into \a LYRA_CXXFLAGS. C++17 is
   * required.
   *
   * The Lyra project evolves frequently. The PJSIP integration was
   * developed against Lyra v2 (\pr{3949}); newer revisions may need
   * adjustments to the configure / link flags.
   *
   *
   * \section lyra_build_pjsip Building PJSIP with Lyra
   *
   * Three build paths are supported.
   *
   * <b>Autoconf:</b> pass \a --with-lyra=DIR to \a ./configure
   * pointing at the Lyra installation prefix:
   *
   \code
      ./configure --with-lyra=/opt/lyra
   \endcode
   *
   * The configure script detects Lyra, defines
   * #PJMEDIA_HAS_LYRA_CODEC to 1, and sets
   * #PJMEDIA_CODEC_LYRA_DEFAULT_MODEL_PATH to
   * \a &lt;prefix&gt;/model_coeffs. Use \a --disable-lyra to opt
   * out explicitly.
   *
   * <b>CMake:</b> Lyra is located via the bundled
   * \a cmake/FindLyra.cmake module (\a find_package(Lyra REQUIRED)),
   * which uses pkg-config when available.
   *
   * <b>config_site.h fallback</b> (Visual Studio, custom builds):
   * set #PJMEDIA_HAS_LYRA_CODEC to 1 in \a config_site.h and add
   * Lyra's include / lib paths to the project manually.
   *
   * Compile-time clock-rate switches (see \a pjmedia-codec/config.h):
   *
   * - #PJMEDIA_CODEC_LYRA_HAS_8KHZ  — default 0.
   * - #PJMEDIA_CODEC_LYRA_HAS_16KHZ — default 1 (the only clock rate
   *   enabled by default).
   * - #PJMEDIA_CODEC_LYRA_HAS_32KHZ — default 0.
   * - #PJMEDIA_CODEC_LYRA_HAS_48KHZ — default 0.
   *
   * Each enabled clock rate registers a separate codec instance, so
   * a peer can negotiate any of the enabled rates. Disable rates the
   * application doesn't need to shrink the codec list and avoid
   * pointless negotiation rounds.
   *
   *
   * \section lyra_model_files Runtime Model Files
   *
   * Lyra requires four model files at runtime, located in the folder
   * #pjmedia_codec_lyra_config.model_path points at:
   *
   * - \a lyra_config.binarypb
   * - \a lyragan.tflite
   * - \a quantizer.tflite
   * - \a soundstream_encoder.tflite
   *
   * These ship with the Lyra source tree under \a model_coeffs/ and
   * are loaded at codec init — there is no embedded fallback. For
   * mobile / embedded deployments the files must be packaged in the
   * application bundle and \a model_path set to the unpacked
   * location.
   *
   *
   * \section lyra_caveats Caveats
   *
   * - <b>Experimental:</b> restated for emphasis. No RFC, no IANA
   *   registration; the fmtp \a bitrate parameter is PJSIP-internal.
   *   Interop is essentially limited to other PJSIP peers running
   *   the same integration.
   * - <b>CPU cost:</b> Lyra runs a TFLite model on every 20 ms
   *   frame. Per-call CPU is significantly higher than traditional
   *   codecs; profile on the target platform before shipping.
   * - <b>Speech only:</b> Lyra is trained on speech and produces
   *   poor results on music or non-speech audio. Pair it with
   *   appropriate call routing.
   * - <b>Model file size:</b> the four model files together total
   *   several megabytes; account for this in mobile bundle sizing.
   *
   */

PJ_BEGIN_DECL

/**
 * Lyra codec setting;
 */
typedef struct pjmedia_codec_lyra_config
{
    /**
     * The value represents the decoder bitrate requested by the receiver.
     * Endpoints can be configured with different bitrates. For example,
     * the local endpoint might be set to a bitrate of 3200, while
     * the remote endpoint is set to 6000. In this scenario, the remote
     * endpoint will send data at 3200 bitrate, while the local endpoint
     * will send data at 6000 bitrate. Valid bitrate: 3200, 6000, 9200.
     * By default it is set to PJMEDIA_CODEC_LYRA_DEFAULT_BIT_RATE.
     */
    unsigned    bit_rate;

    /**
     * Lyra required some additional (model) files, including
     * \b lyra_config.binarypb , \b lyragan.tflite , \b quantizer.tflite and
     * \b soundstream_encoder.tflite .
     * This setting represents the folder containing the above files.
     * The specified folder should contain these files. If an invalid folder
     * is provided, the codec creation will fail.
     */
    pj_str_t    model_path;

} pjmedia_codec_lyra_config;

/**
 * Initialize and register lyra codec factory to pjmedia endpoint.
 *
 * @param endpt     The pjmedia endpoint.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_lyra_init(pjmedia_endpt *endpt);

/**
 * Unregister lyra codec factory from pjmedia endpoint and deinitialize
 * the lyra codec library.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_lyra_deinit(void);

/**
 * Get the default Lyra configuration.
 *
 * @param cfg           Lyra codec configuration.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_codec_lyra_get_config( pjmedia_codec_lyra_config *cfg);

/**
 * Set the default Lyra configuration.
 *
 * @param cfg           Lyra codec configuration.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_codec_lyra_set_config(const pjmedia_codec_lyra_config *cfg);


PJ_END_DECL


/**
 * @}
 */

#endif  /* __PJMEDIA_CODEC_LYRA_H__ */


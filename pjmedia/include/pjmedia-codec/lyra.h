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
   * @defgroup PJMED_LYRA lyra Codec
   * @ingroup PJMEDIA_CODEC_CODECS
   * @brief Implementation of lyra Codec
   * @{
   *
   * This section describes functions to initialize and register lyra codec
   * factory to the codec manager. After the codec factory has been registered,
   * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
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
     * will send data at 6000 bitrate.
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


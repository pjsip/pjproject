/*
 * Copyright (C) 2024-2025 Teluu Inc. (http://www.teluu.com)
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

#ifndef __PJMEDIA_TXT_STREAM_H__
#define __PJMEDIA_TXT_STREAM_H__


/**
 * @file txt_stream.h
 * @brief Text Stream.
 */

#include <pjmedia/endpoint.h>
#include <pjmedia/jbuf.h>
#include <pjmedia/port.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/rtcp_fb.h>
#include <pjmedia/transport.h>
#include <pjmedia/stream_common.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJMED_TXT_STRM Text streams
 * @ingroup PJMEDIA_PORT
 * @brief Text communication via the network
 * @{
 *
 * A text stream is a bidirectional real-time text (RTT) communication
 * between two endpoints as specified in RFC 4103. It corresponds to
 * a text media description ("m=text" line) in SDP session descriptor.
 *
 * A text stream consists of two unidirectional channels:
 *  - encoding channel, which transmits unidirectional text to remote, and
 *  - decoding channel, which receives unidirectional text from remote.
 *
 * A text stream internally manages the following objects:
 *  - a @ref PJMED_JBUF,
 *  - two instances of RTP sessions (#pjmedia_rtp_session, one for each
 *    direction),
 *  - one instance of RTCP session (#pjmedia_rtcp_session),
 *  - and a reference to media transport to send and receive packets
 *    to/from the network (see @ref PJMEDIA_TRANSPORT).
 *
 * Text streams are created by calling #pjmedia_txt_stream_create(),
 * specifying #pjmedia_txt_stream_info structure in the parameter. Application
 * can construct the #pjmedia_txt_stream_info structure manually, or use 
 * #pjmedia_txt_stream_info_from_sdp() function to construct the
 * #pjmedia_txt_stream_info from local and remote SDP session descriptors.
 */


/**
 * This structure describes text stream information. Each text stream
 * corresponds to one "m=text" line in SDP session descriptor, and it has
 * its own RTP/RTCP socket pair.
 */
typedef struct pjmedia_txt_stream_info
{
    PJ_DECL_STREAM_INFO_COMMON_MEMBER()

    pjmedia_codec_info  fmt;        /**< Incoming codec format info.    */
    pjmedia_codec_fmtp  enc_fmtp;   /**< Encoder's fmtp params.         */
    pjmedia_codec_fmtp  dec_fmtp;   /**< Decoder's fmtp params.         */
    unsigned            buffer_time;/**< Buffering time.                */
} pjmedia_txt_stream_info;


/**
 * This structure describes the text data passed in the callback set
 * via #pjmedia_txt_stream_set_rx_callback().
 */
typedef struct pjmedia_txt_stream_data {
    int                 seq;        /**< Sequence.                      */
    pj_uint32_t         ts;         /**< Timestamp.                     */
    pj_str_t            text;       /**< The incoming text data.        */
} pjmedia_txt_stream_data;


/**
 * This function will initialize the text stream info based on information
 * in both SDP session descriptors for the specified text stream index.
 *
 * @param si            Text stream info structure to be initialized.
 * @param pool          Pool to allocate memory.
 * @param endpt         PJMEDIA endpoint instance.
 * @param local         Local SDP session descriptor.
 * @param remote        Remote SDP session descriptor.
 * @param stream_idx    Text media stream index in the session descriptor.
 *
 * @return              PJ_SUCCESS if stream info is successfully initialized.
 */
PJ_DECL(pj_status_t)
pjmedia_txt_stream_info_from_sdp( pjmedia_txt_stream_info *si,
                                  pj_pool_t *pool,
                                  pjmedia_endpt *endpt,
                                  const pjmedia_sdp_session *local,
                                  const pjmedia_sdp_session *remote,
                                  unsigned stream_idx);


/*
 * Opaque declaration for text stream.
 */
typedef struct pjmedia_txt_stream pjmedia_txt_stream;


/**
 * Create a text stream based on the specified parameter.
 *
 * @param endpt         Media endpoint.
 * @param pool          Pool to allocate memory for the stream.
 * @param info          Stream information.
 * @param tp            Media transport instance used to transmit 
 *                      and receive RTP/RTCP packets to/from the underlying 
 *                      transport. 
 * @param user_data     Arbitrary user data (for future callback feature).
 * @param p_stream      Pointer to receive the text stream.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_txt_stream_create(pjmedia_endpt *endpt,
                          pj_pool_t *pool,
                          const pjmedia_txt_stream_info *info,
                          pjmedia_transport *tp,
                          void *user_data,
                          pjmedia_txt_stream **p_stream);


/**
 * Destroy the text stream.
 *
 * @param stream        The text stream.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_txt_stream_destroy(pjmedia_txt_stream *stream);


/**
 * Send text using the text stream.
 *
 * @param stream        The text stream.
 * @param text          Text to be sent.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_txt_stream_send_text(pjmedia_txt_stream *stream, const pj_str_t *text);


/**
 * Start the text stream. This will start the appropriate channels
 * in the text stream, depending on the media direction that was set
 * when the stream was created.
 *
 * @param stream        The text stream.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_txt_stream_start(pjmedia_txt_stream *stream);


/**
 * Set callback to be called upon receiving text data.
 *
 * IMPORTANT: Application shall not destroy the text stream from within
 * the callback.
 *
 * @param stream        The text stream.
 * @param cb            Callback to be called upon receiving text data.
 *                      See #pjmedia_txt_stream_data.
 * @param user_data     User data to be returned back when the callback
 *                      is called.
 * @param option        Option, must be 0 for now.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_txt_stream_set_rx_callback(pjmedia_txt_stream *stream,
                                   void (*cb)(pjmedia_txt_stream*,
                                              void *user_data,
                                              const pjmedia_txt_stream_data *data),
                                   void *user_data,
                                   unsigned option);


/**
 * Get the stream info.
 *
 * @param stream        The text stream.
 * @param info          Stream info.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_txt_stream_get_info(const pjmedia_txt_stream *stream,
                            pjmedia_txt_stream_info *info);


/**
 * @}
 */

PJ_END_DECL


#endif  /* __PJMEDIA_STREAM_H__ */

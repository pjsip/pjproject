/* 
 * Copyright (C) 2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_STREAM_COMMON_H__
#define __PJMEDIA_STREAM_COMMON_H__


/**
 * @file stream_common.h
 * @brief Stream common functions.
 */

#include <pjmedia/codec.h>
#include <pjmedia/sdp.h>
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>


PJ_BEGIN_DECL

/**
 * This structure describes rtp/rtcp session information of the media stream.
 */
typedef struct pjmedia_stream_rtp_sess_info
{
    /**
     * The decode RTP session.
     */
    const pjmedia_rtp_session *rx_rtp;

    /**
     * The encode RTP session.
     */
    const pjmedia_rtp_session *tx_rtp;

    /**
     * The decode RTCP session.
     */
    const pjmedia_rtcp_session *rtcp;

} pjmedia_stream_rtp_sess_info;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0

/**
 * Structure of configuration settings for stream keepalive.
 */
typedef struct pjmedia_stream_ka_config
{
    /**
     * The number of keepalive to be sent after the stream is created.
     * When this is set to 0, keepalive will be sent once for NAT hole
     * punching if stream's use_ka is enabled.
     *
     * Default: PJMEDIA_STREAM_START_KA_CNT
     */
    unsigned                        start_count;

    /**
     * The keepalive sending interval after the stream is created.
     *
     * Default: PJMEDIA_STREAM_START_KA_INTERVAL_MSEC
     */
    unsigned                        start_interval;

    /**
     * The keepalive sending interval, after #start_count number keepalive 
     * was sent.
     * 
     * Default: PJMEDIA_STREAM_KA_INTERVAL (seconds)
     */
    unsigned                        ka_interval;

} pjmedia_stream_ka_config;

/**
 * Initialize the stream send keep-alive with default settings.
 *
 * @param cfg           Stream send keep-alive structure to be initialized.
 */
PJ_DECL(void)
pjmedia_stream_ka_config_default(pjmedia_stream_ka_config *cfg);

#endif

/**
 * This is internal function for parsing SDP format parameter of specific
 * format or payload type, used by stream in generating stream info from SDP.
 *
 * @param pool          Pool to allocate memory, if pool is NULL, the fmtp
 *                      string pointers will point to the original string in
 *                      the SDP media descriptor.
 * @param m             The SDP media containing the format parameter to
 *                      be parsed.
 * @param pt            The format or payload type.
 * @param fmtp          The format parameter to store the parsing result.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_info_parse_fmtp(pj_pool_t *pool,
                                                    const pjmedia_sdp_media *m,
                                                    unsigned pt,
                                                    pjmedia_codec_fmtp *fmtp);


PJ_END_DECL


#endif /* __PJMEDIA_STREAM_COMMON_H__ */

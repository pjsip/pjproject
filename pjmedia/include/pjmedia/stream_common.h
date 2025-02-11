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
#include <pjmedia/jbuf.h>
#include <pjmedia/sdp.h>
#include <pjmedia/rtp.h>
#include <pjmedia/rtcp.h>
#include <pjmedia/transport.h>


PJ_BEGIN_DECL

/*****************************************************************************
 *
 * COMMON MEDIA STREAM
 *
 *****************************************************************************/

/* Tracing jitter buffer operations in a stream session to a CSV file.
 * The trace will contain JB operation timestamp, frame info, RTP info, and
 * the JB state right after the operation.
 */
#define PJMEDIA_STREAM_TRACE_JB     0

/* Forward declarations. */
typedef struct pjmedia_stream_info_common pjmedia_stream_info_common;
typedef struct pjmedia_channel pjmedia_channel;

/**
 * This structure describes media stream.
 * A media stream is bidirectional media transmission between two endpoints.
 * It consists of two channels, i.e. encoding and decoding channels.
 * A media stream corresponds to a single "m=" line in a SDP session
 * description.
 */
typedef struct pjmedia_stream_common
{
    pjmedia_endpt           *endpt;         /**< Media endpoint.            */
    pj_grp_lock_t           *grp_lock;      /**< Group lock.                */
    pjmedia_stream_info_common *si;         /**< Creation parameter.        */
    pjmedia_port             port;          /**< Port interface.            */
    pjmedia_channel         *enc;           /**< Encoding channel.          */
    pjmedia_channel         *dec;           /**< Decoding channel.          */
    pj_pool_t               *own_pool;      /**< Only created if not given  */

    pjmedia_dir              dir;           /**< Stream direction.          */
    void                    *user_data;     /**< User data.                 */
    pj_str_t                 name;          /**< Stream name                */
    pj_str_t                 cname;         /**< SDES CNAME                 */

    pjmedia_transport       *transport;     /**< Stream transport.          */

    pj_int16_t              *enc_buf;       /**< Encoding buffer, when enc's
                                                 ptime is different than dec.
                                                 Otherwise it's NULL.       */

    unsigned                 frame_size;    /**< Size of encoded base frame.*/

    pj_mutex_t              *jb_mutex;
    pjmedia_jbuf            *jb;            /**< Jitter buffer.             */
    char                     jb_last_frm;   /**< Last frame type from jb    */
    unsigned                 jb_last_frm_cnt;/**< Last JB frame type counter*/

    pjmedia_rtcp_session     rtcp;          /**< RTCP for incoming RTP.     */

    pj_uint32_t              rtcp_last_tx;  /**< RTCP tx time in timestamp  */
    pj_timestamp             rtcp_fb_last_tx;/**< Last RTCP-FB tx time.     */
    pj_uint32_t              rtcp_interval; /**< Interval, in timestamp.    */
    pj_bool_t                initial_rr;    /**< Initial RTCP RR sent       */
    pj_bool_t                rtcp_sdes_bye_disabled;/**< Send RTCP SDES/BYE?*/
    void                    *out_rtcp_pkt;  /**< Outgoing RTCP packet.      */
    unsigned                 out_rtcp_pkt_size;
                                            /**< Outgoing RTCP packet size. */

#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
    pj_uint32_t              rtcp_xr_last_tx;  /**< RTCP XR tx time
                                                    in timestamp.           */
    pj_uint32_t              rtcp_xr_interval; /**< Interval, in timestamp. */
    pj_sockaddr              rtcp_xr_dest;     /**< Additional remote RTCP XR
                                                    dest. If sin_family is
                                                    zero, it will be ignored*/
    unsigned                 rtcp_xr_dest_len; /**< Length of RTCP XR dest
                                                    address                 */
#endif

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    pj_bool_t                use_ka;           /**< Stream keep-alive with non-
                                                    codec-VAD mechanism is
                                                    enabled?                */
    unsigned                 ka_interval;      /**< The keepalive sending
                                                    interval                */
    pj_time_val              last_frm_ts_sent; /**< Time of last sending
                                                    packet                  */
    unsigned                 start_ka_count;   /**< The number of keep-alive
                                                    to be sent after it is
                                                    created                 */
    unsigned                 start_ka_interval;/**< The keepalive sending
                                                    interval after the stream
                                                    is created              */
    pj_timestamp             last_start_ka_tx; /**< Timestamp of the last
                                                    keepalive sent          */
#endif

    pj_sockaddr              rem_rtp_addr;     /**< Remote RTP address      */
    unsigned                 rem_rtp_flag;     /**< Indicator flag about
                                                    packet from this addr.
                                                    0=no pkt, 1=good ssrc,
                                                    2=bad ssrc pkts         */
    pj_sockaddr              rtp_src_addr;     /**< Actual packet src addr.    */
    unsigned                 rtp_src_cnt;      /**< How many pkt from
                                                    this addr.              */

#if defined(PJMEDIA_STREAM_TRACE_JB) && PJMEDIA_STREAM_TRACE_JB != 0
    pj_oshandle_t            trace_jb_fd;      /**< Jitter tracing file handle.*/
    char                    *trace_jb_buf;     /**< Jitter tracing buffer.     */
#endif

    pj_uint32_t              rtp_rx_last_ts;        /**< Last received RTP
                                                         timestamp          */
    pj_uint32_t              rtp_tx_err_cnt;        /**< The number of RTP
                                                         send() error       */
    pj_uint32_t              rtcp_tx_err_cnt;       /**< The number of RTCP
                                                         send() error       */

    /* RTCP Feedback */
    pj_bool_t                send_rtcp_fb_nack;     /**< Send NACK?         */
    int                      pending_rtcp_fb_nack;  /**< Any pending NACK?  */
    pjmedia_rtcp_fb_nack     rtcp_fb_nack;          /**< TX NACK state.     */
    int                      rtcp_fb_nack_cap_idx;  /**< RX NACK cap idx.   */
} pjmedia_stream_common;


/**
 * Media channel.
 * Media channel is unidirectional flow of media from sender to
 * receiver.
 */
typedef struct pjmedia_channel
{
    pjmedia_stream_common  *stream;         /**< Parent stream.             */
    pjmedia_dir             dir;            /**< Channel direction.         */
    pjmedia_port            port;           /**< Port interface.            */
    unsigned                pt;             /**< Payload type.              */
    pj_bool_t               paused;         /**< Paused?.                   */
    void                   *buf;            /**< Output buffer.             */
    unsigned                buf_size;       /**< Size of output buffer.     */
    pjmedia_rtp_session     rtp;            /**< RTP session.               */
} pjmedia_channel;


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


/**
 * Get the stream statistics. See also
 * #pjmedia_stream_get_stat_jbuf()
 *
 * @param stream        The media stream.
 * @param stat          Media stream statistics.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_stream_common_get_stat( const pjmedia_stream_common *stream,
                                pjmedia_rtcp_stat *stat);


/**
 * Reset the stream statistics.
 *
 * @param stream        The media stream.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_stream_common_reset_stat(pjmedia_stream_common *stream);


/**
 * Send RTCP SDES for the media stream.
 *
 * @param stream        The media stream.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_stream_common_send_rtcp_sdes( pjmedia_stream_common *stream );

/**
 * Send RTCP BYE for the media stream.
 *
 * @param stream        The media stream.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_stream_common_send_rtcp_bye( pjmedia_stream_common *stream );


/**
 * Get the RTP session information of the media stream. This function can be
 * useful for app with custom media transport to inject/filter some
 * outgoing/incoming proprietary packets into normal audio RTP traffics.
 * This will return the original pointer to the internal states of the stream,
 * and generally it is not advisable for app to modify them.
 *
 * @param stream        The media stream.
 *
 * @param session_info  The stream session info.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_stream_common_get_rtp_session_info(pjmedia_stream_common *stream,
                                   pjmedia_stream_rtp_sess_info *session_info);


/* Internal function. */

/* Internal:  * Send RTCP SDES for the media stream. */
pj_status_t pjmedia_stream_send_rtcp(pjmedia_stream_common *c_strm,
                                     pj_bool_t with_sdes,
                                     pj_bool_t with_bye,
                                     pj_bool_t with_xr,
                                     pj_bool_t with_fb,
                                     pj_bool_t with_fb_nack,
                                     pj_bool_t with_fb_pli);


/*****************************************************************************
 *
 * COMMON MEDIA STREAM INFORMATION
 *
 *****************************************************************************/

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

#define PJ_DECL_STREAM_INFO_KA_MEMBER() \
    pj_bool_t           use_ka;     /**< Stream keep-alive and NAT hole punch \
                                         (see #PJMEDIA_STREAM_ENABLE_KA) \
                                         is enabled?                        */ \
    pjmedia_stream_ka_config ka_cfg; \
                                    /**< Stream send kep-alive settings.    */ \

#else

#define PJ_DECL_STREAM_INFO_KA_MEMBER()

#endif


/**
 * This structure describes the common media stream information.
 */
#define PJ_DECL_STREAM_INFO_COMMON_MEMBER() \
    pjmedia_type        type;       /**< Media type (audio, video)          */ \
    pjmedia_tp_proto    proto;      /**< Transport protocol (RTP/AVP, etc.) */ \
    pjmedia_dir         dir;        /**< Media direction.                   */ \
    pj_sockaddr         local_addr; /**< Local RTP address                  */ \
    pj_sockaddr         rem_addr;   /**< Remote RTP address                 */ \
    pj_sockaddr         rem_rtcp;   /**< Optional remote RTCP address. If \
                                         sin_family is zero, the RTP address \
                                         will be calculated from RTP.       */ \
    pj_bool_t           rtcp_mux;   /**< Use RTP and RTCP multiplexing.     */ \
\
    pj_bool_t           rtcp_xr_enabled; \
                                    /**< Specify whether RTCP XR is enabled.*/ \
    pj_uint32_t         rtcp_xr_interval; /**< RTCP XR interval.            */ \
    pj_sockaddr         rtcp_xr_dest;/**<Additional remote RTCP XR address. \
                                         This is useful for third-party (e.g: \
                                         network monitor) to monitor the \
                                         stream. If sin_family is zero, \
                                         this will be ignored.              */ \
\
    pjmedia_rtcp_fb_info loc_rtcp_fb; /**< Local RTCP-FB info.              */ \
    pjmedia_rtcp_fb_info rem_rtcp_fb; /**< Remote RTCP-FB info.             */ \
\
    unsigned            tx_pt;      /**< Outgoing codec payload type.       */ \
    unsigned            rx_pt;      /**< Incoming codec payload type.       */ \
\
    pj_uint32_t         ssrc;       /**< RTP SSRC.                          */ \
    pj_str_t            cname;      /**< RTCP CNAME.                        */ \
    pj_bool_t           has_rem_ssrc;/**<Has remote RTP SSRC?               */ \
    pj_uint32_t         rem_ssrc;   /**< Remote RTP SSRC.                   */ \
    pj_str_t            rem_cname;  /**< Remote RTCP CNAME.                 */ \
    pj_uint32_t         rtp_ts;     /**< Initial RTP timestamp.             */ \
    pj_uint16_t         rtp_seq;    /**< Initial RTP sequence number.       */ \
    pj_uint8_t          rtp_seq_ts_set; \
                                    /**< Bitmask flags if initial RTP sequence \
                                         and/or timestamp for sender are set. \
                                         bit 0/LSB : sequence flag  \
                                         bit 1     : timestamp flag         */ \
    int                 jb_init;    /**< Jitter buffer init delay in msec. \
                                         (-1 for default).                  */ \
    int                 jb_min_pre; /**< Jitter buffer minimum prefetch \
                                         delay in msec (-1 for default).    */ \
    int                 jb_max_pre; /**< Jitter buffer maximum prefetch \
                                         delay in msec (-1 for default).    */ \
    int                 jb_max;     /**< Jitter buffer max delay in msec.   */ \
    pjmedia_jb_discard_algo jb_discard_algo; \
                                    /**< Jitter buffer discard algorithm.   */ \
\
    PJ_DECL_STREAM_INFO_KA_MEMBER() \
\
    pj_bool_t           rtcp_sdes_bye_disabled; \
                                    /**< Disable automatic sending of RTCP \
                                         SDES and BYE.                      */ \


/**
 * This structure describes media stream information. Each media stream
 * corresponds to one "m=" line in SDP session descriptor, and it has
 * its own RTP/RTCP socket pair.
 */
typedef struct pjmedia_stream_info_common
{
    PJ_DECL_STREAM_INFO_COMMON_MEMBER()
} pjmedia_stream_info_common;


/**
 * This function will initialize the stream info based on information
 * in both SDP session descriptors for the specified stream index.
 *
 * @param si            Stream info structure to be initialized.
 * @param pool          Pool to allocate memory.
 * @param endpt         PJMEDIA endpoint instance.
 * @param local         Local SDP session descriptor.
 * @param remote        Remote SDP session descriptor.
 * @param stream_idx    Media stream index in the session descriptor.
 *
 * @return              PJ_SUCCESS if stream info is successfully initialized.
 */
PJ_DECL(pj_status_t)
pjmedia_stream_info_common_from_sdp(pjmedia_stream_info_common *si,
                                    pj_pool_t *pool,
                                    pjmedia_endpt *endpt,
                                    const pjmedia_sdp_session *local,
                                    const pjmedia_sdp_session *remote,
                                    unsigned stream_idx);


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


/**
 * This is an internal function for parsing fmtp data from a raw buffer.
 *
 * @param pool          Pool to allocate memory, if pool is NULL, the fmtp
 *                      string pointers will point to the original string.
 * @param str           The fmtp string to be parsed.
 * @param fmtp          The format parameter to store the parsing result.
 *
 * @return              PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_info_parse_fmtp_data(pj_pool_t *pool,
                                                         const pj_str_t *str,
                                                         pjmedia_codec_fmtp *fmtp);


PJ_END_DECL


#endif /* __PJMEDIA_STREAM_COMMON_H__ */

/* $Id$ */
/* 
 * Copyright (C) 2018 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_RTCP_FB_H__
#define __PJMEDIA_RTCP_FB_H__

/**
 * @file rtcp_fb.h
 * @brief RTCP Feedback implementation.
 */

#include <pjmedia/rtcp.h>
#include <pjmedia/sdp.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJMED_RTCP_FB RTCP Feedback - RFC 4585
 * @ingroup PJMEDIA_SESSION
 * @brief RTCP Feedback extension to RTCP session
 * @{
 *
 * PJMEDIA implements RTCP Feedback specification (RFC 4585) to enable
 * receivers to provide more immediate feedback to the senders.
 */


/**
 * Enumeration of RTCP Feedback types. Each feedback type may have subtypes,
 * which should be specified in feedback parameters.
 */
typedef enum pjmedia_rtcp_fb_type
{
    /**
     * Positive acknowledgement feedbacks. Sample subtypes are Reference Picture
     * Selection Indication (RPSI) and application layer feedbacks.
     */
    PJMEDIA_RTCP_FB_ACK,

    /**
     * Negative acknowledgement feedbacks. Sample subtypes are generic NACK,
     * Picture Loss Indication (PLI), Slice Loss Indication (SLI), Reference
     * Picture Selection Indication (RPSI), and application layer feedbacks.
     */
    PJMEDIA_RTCP_FB_NACK,

    /**
     * Minimum interval between two regular RTCP packets.
     */
    PJMEDIA_RTCP_FB_TRR_INT,

    /**
     * Other feedback types.
     */
    PJMEDIA_RTCP_FB_OTHER

} pjmedia_rtcp_fb_type;


/**
 * This structure declare RTCP Feedback capability.
 */
typedef struct pjmedia_rtcp_fb_cap
{
    /**
     * Specify the codecs to which the capability is applicable. Codec ID is
     * using the same format as in pjmedia_codec_mgr_find_codecs_by_id() and
     * pjmedia_vid_codec_mgr_find_codecs_by_id(), e.g: "L16/8000/1", "PCMU",
     * "H264". This can also be an asterisk ("*") to represent all codecs.
     */
    pj_str_t		    codec_id;

    /**
     * Specify the RTCP Feedback type.
     */
    pjmedia_rtcp_fb_type    type;

    /**
     * Specify the type name if RTCP Feedback type is PJMEDIA_RTCP_FB_OTHER.
     */
    pj_str_t		    type_name;

    /**
     * Specify the RTCP Feedback parameters. Feedback subtypes should be
     * specified in this field, e.g:
     * - 'pli' for Picture Loss Indication feedback,
     * - 'sli' for Slice Loss Indication feedback,
     * - 'rpsi' for Reference Picture Selection Indication feedback,
     * - 'app' for specific/proprietary application layer feedback.
     */
    pj_str_t		    param;

} pjmedia_rtcp_fb_cap;


/**
 * This structure declares RTCP Feedback information.
 */
typedef struct pjmedia_rtcp_fb_info
{
    /**
     * Number of RTCP Feedback capabilities.
     */
    unsigned		     cap_count;

    /**
     * The RTCP Feedback capabilities.
     */
    pjmedia_rtcp_fb_cap	     caps[PJMEDIA_RTCP_FB_MAX_CAP];

} pjmedia_rtcp_fb_info;


/**
 * This structure declares RTCP Feedback configuration settings.
 */
typedef struct pjmedia_rtcp_fb_setting
{
    /**
     * Specify whether transport protocol in SDP media description uses
     * RTP/AVP instead of RTP/AVPF. Note that RFC4585 mandates to signal
     * RTP/AVPF profile, but it may cause SDP negotiation failure when
     * negotiating with endpoints that does not support RTP/AVPF (including
     * older version of PJSIP), furthermore, there is RFC8643 that promotes
     * interoperability over the strictness of RTP profile specifications.
     *
     * Default: PJ_TRUE.
     */
    pj_bool_t		     dont_use_avpf;

    /**
     * Number of RTCP Feedback capabilities.
     */
    unsigned		     cap_count;

    /**
     * The RTCP Feedback capabilities.
     */
    pjmedia_rtcp_fb_cap	     caps[PJMEDIA_RTCP_FB_MAX_CAP];

} pjmedia_rtcp_fb_setting;


/**
 * This structure declares RTCP Feedback Generic NACK message.
 */
typedef struct pjmedia_rtcp_fb_nack
{
    pj_int32_t		 pid;		/**< Packet ID (RTP seq)    */
    pj_uint16_t		 blp;		/**< Bitmask of following lost
					     packets		    */
} pjmedia_rtcp_fb_nack;


/**
 * This structure declares RTCP Feedback Slice Loss Indication (SLI) message.
 */
typedef struct pjmedia_rtcp_fb_sli
{
    pj_uint16_t		 first;		/**< First lost macroblock	*/
    pj_uint16_t		 number;	/**< The number of lost macroblocks
					     packets			*/
    pj_uint8_t		 pict_id;	/**< Picture ID (temporal ref)	*/
} pjmedia_rtcp_fb_sli;


/**
 * This structure declares RTCP Feedback Reference Picture Selection
 * Indication (RPSI) message.
 */
typedef struct pjmedia_rtcp_fb_rpsi
{
    pj_uint8_t		 pt;		/**< Payload Type		*/
    pj_str_t		 rpsi;		/**< Native RPSI bit string	*/
    pj_size_t		 rpsi_bit_len;	/**< Length of RPSI in bit	*/
} pjmedia_rtcp_fb_rpsi;


/**
 * Event data for incoming RTCP Feedback message event
 * (PJMEDIA_EVENT_RX_RTCP_FB).
 */
typedef struct pjmedia_event_rx_rtcp_fb_data
{
    pjmedia_rtcp_fb_cap		cap;
    union {
	pjmedia_rtcp_fb_nack	nack;
	pjmedia_rtcp_fb_sli	sli;
	pjmedia_rtcp_fb_rpsi	rpsi;
    } msg;

} pjmedia_event_rx_rtcp_fb_data;


/**
 * Initialize RTCP Feedback setting with default values.
 *
 * @param opt		The RTCP Feedback setting to be initialized.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_setting_default(
					pjmedia_rtcp_fb_setting *opt);


/**
 * Duplicate RTCP Feedback setting.
 *
 * @param pool		Pool to be used for duplicating the config.
 * @param dst		Destination configuration.
 * @param src		Source configuration.
 *
 */
PJ_DECL(void) pjmedia_rtcp_fb_setting_dup(pj_pool_t *pool,
					  pjmedia_rtcp_fb_setting *dst,
					  const pjmedia_rtcp_fb_setting *src);


/**
 * Duplicate RTCP Feedback info.
 *
 * @param pool		Pool to be used for duplicating the info.
 * @param dst		Destination info.
 * @param src		Source info.
 *
 */
PJ_DECL(void) pjmedia_rtcp_fb_info_dup(pj_pool_t *pool,
				       pjmedia_rtcp_fb_info *dst,
				       const pjmedia_rtcp_fb_info *src);


/**
 * Encode RTCP Feedback specific information into local SDP according to
 * the provided RTCP Feedback setting. This is useful to signal remote
 * endpoint that local endpoint is capable and willing to receive RTCP
 * Feedback packets as described in the local SDP.
 *
 * @param pool		Pool object to allocate memory in updating local SDP.
 * @param endpt		The media endpoint.
 * @param opt		RTCP Feedback setting.
 * @param sdp_local	The local SDP to be filled in information from the
 *			media transport.
 * @param med_idx	The SDP media index.
 * @param sdp_remote	Remote SDP or NULL if local is offerer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_encode_sdp(
				    pj_pool_t *pool,
				    pjmedia_endpt *endpt,
				    const pjmedia_rtcp_fb_setting *opt,
				    pjmedia_sdp_session *sdp_local,
				    unsigned med_idx,
				    const pjmedia_sdp_session *sdp_remote);


/**
 * Decode RTCP Feedback specific information from SDP media.
 *
 * @param pool		Pool object to allocate memory.
 * @param endpt		The media endpoint.
 * @param opt		Options, currently it must be NULL.
 * @param sdp		The SDP.
 * @param med_idx	The SDP media index.
 * @param info		The RTCP-FB info fetched from SDP.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_decode_sdp(
				    pj_pool_t *pool,
				    pjmedia_endpt *endpt,
				    const void *opt,
				    const pjmedia_sdp_session *sdp,
				    unsigned med_idx,
				    pjmedia_rtcp_fb_info *info);


/**
 * Decode RTCP Feedback specific information from SDP media for the specified
 * payload type. If the payload type is set to negative value, it will decode
 * RTCP Feedback info for all payload types in the SDP media.
 *
 * @param pool		Pool object to allocate memory.
 * @param endpt		The media endpoint.
 * @param opt		Options, currently it must be NULL.
 * @param sdp		The SDP.
 * @param med_idx	The SDP media index.
 * @param pt		The payload type.
 * @param info		The RTCP-FB info fetched from SDP.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_decode_sdp2(
				    pj_pool_t *pool,
				    pjmedia_endpt *endpt,
				    const void *opt,
				    const pjmedia_sdp_session *sdp,
				    unsigned med_idx,
				    int pt,
				    pjmedia_rtcp_fb_info *info);


/**
 * Build an RTCP Feedback Generic NACK packet. This packet can be appended to
 * other RTCP packets, e.g: RTCP RR/SR, to compose a compound RTCP packet.
 * See also RFC 4585 Section 6.2.1 about Generic NACK message.
 *
 * @param session   The RTCP session.
 * @param buf	    The buffer to receive RTCP Feedback packet.
 * @param length    On input, it will contain the buffer length.
 *		    On output, it will contain the generated RTCP Feedback
 *		    packet length.
 * @param nack_cnt  The number of RTCP Feedback Generic NACK messages.
 * @param nack	    The array of RTCP Feedback Generic NACK messages.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_build_nack(
					pjmedia_rtcp_session *session,
					void *buf,
					pj_size_t *length,
					unsigned nack_cnt,
					const pjmedia_rtcp_fb_nack nack[]);


/**
 * Build an RTCP Feedback Picture Loss Indication (PLI) packet. This packet
 * can be appended to other RTCP packets, e.g: RTCP RR/SR, to compose a
 * compound RTCP packet. See also RFC 4585 Section 6.3.1 about PLI FB message.
 *
 * @param session   The RTCP session.
 * @param buf	    The buffer to receive RTCP Feedback packet.
 * @param length    On input, it will contain the buffer length.
 *		    On output, it will contain the generated RTCP Feedback
 *		    packet length.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_build_pli(
					pjmedia_rtcp_session *session, 
					void *buf,
					pj_size_t *length);


/**
 * Build an RTCP Feedback Slice Loss Indication (SLI) packet. This packet can
 * be appended to other RTCP packets, e.g: RTCP RR/SR, to compose a compound
 * RTCP packet. See also RFC 4585 Section 6.3.2 about SLI FB message.
 *
 * @param session   The RTCP session.
 * @param buf	    The buffer to receive RTCP Feedback packet.
 * @param length    On input, it will contain the buffer length.
 *		    On output, it will contain the generated RTCP Feedback
 *		    packet length.
 * @param sli_cnt   The number of RTCP Feedback SLI messages.
 * @param sli	    The array of RTCP Feedback SLI messages.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_build_sli(
					pjmedia_rtcp_session *session, 
					void *buf,
					pj_size_t *length,
					unsigned sli_cnt,
					const pjmedia_rtcp_fb_sli sli[]);


/**
 * Build an RTCP Feedback Reference Picture Selection Indication (RPSI)
 * packet. This packet can be appended to other RTCP packets, e.g: RTCP RR/SR,
 * to compose a compound RTCP packet. See also RFC 4585 Section 6.3.3 about
 * RPSI FB message.
 *
 * @param session   The RTCP session.
 * @param buf	    The buffer to receive RTCP Feedback packet.
 * @param length    On input, it will contain the buffer length.
 *		    On output, it will contain the generated RTCP Feedback
 *		    packet length.
 * @param rpsi	    The RTCP Feedback RPSI message.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_build_rpsi(
					pjmedia_rtcp_session *session, 
					void *buf,
					pj_size_t *length,
					const pjmedia_rtcp_fb_rpsi *rpsi);


/**
 * Check whether the specified payload contains RTCP feedback generic NACK
 * message, and parse the payload if it does.
 *
 * @param buf	    The payload buffer.
 * @param length    The payload length.
 * @param nack_cnt  On input, it specifies the maximum number of generic NACK
 *		    messages.
 *		    On output, it specifies the number of parsed generic NACK
 *		    messages.
 * @param nack	    The array of RTCP Feedback Generic NACK messages.
 *
 * @return	    PJ_SUCCESS if the payload contains generic NACK message
 *		    and has been parsed successfully.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_parse_nack(
					const void *buf,
					pj_size_t length,
					unsigned *nack_cnt,
					pjmedia_rtcp_fb_nack nack[]);


/**
 * Check whether the specified payload contains RTCP feedback Picture Loss
 * Indication (PLI) message.
 *
 * @param buf	    The payload buffer.
 * @param length    The payload length.
 *
 * @return	    PJ_SUCCESS if the payload contains PLI message.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_parse_pli(
					const void *buf,
					pj_size_t length);


/**
 * Check whether the specified payload contains RTCP feedback Slice Loss
 * Indication (SLI) message, and parse the payload if it does.
 *
 * @param buf	    The payload buffer.
 * @param length    The payload length.
 * @param sli_cnt   On input, it specifies the maximum number of SLI messages.
 *		    On output, it specifies the number of parsed SLI messages.
 * @param sli	    The array of RTCP Feedback SLI messages.
 *
 * @return	    PJ_SUCCESS if the payload contains SLI messages and
 *		    has been parsed successfully.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_parse_sli(
					const void *buf,
					pj_size_t length,
					unsigned *sli_cnt,
					pjmedia_rtcp_fb_sli sli[]);


/**
 * Check whether the specified payload contains RTCP feedback Reference
 * Picture Selection Indication (RPSI) message, and parse the payload
 * if it does.
 *
 * @param buf	    The payload buffer.
 * @param length    The payload length.
 * @param rpsi	    The parsed RTCP Feedback RPSI messages.
 *
 * @return	    PJ_SUCCESS if the payload contains SLI messages and
 *		    has been parsed successfully.
 */
PJ_DECL(pj_status_t) pjmedia_rtcp_fb_parse_rpsi(
					const void *buf,
					pj_size_t length,
					pjmedia_rtcp_fb_rpsi *rpsi);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_RTCP_FB_H__ */

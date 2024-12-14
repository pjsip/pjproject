/* 
 * Copyright (C) 2024 Green and Silver Leaves. (https://github.com/BSVN)
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
#ifndef __PJSIP_SIPREC_H__
#define __PJSIP_SIPREC_H__

/**
 * @file sip_siprec.h
 * @brief SIP Session Recording Protocol (siprec)
 * support (RFC 7866 - Session Recording Protocol in SIP)
 */


#include <pjsip-ua/sip_inv.h>
#include <pjsip/sip_msg.h>

/**
 * @defgroup PJSIP_SIPREC SIP Session Recording Protocol (siprec)
 * support (RFC 7866 - Session Recording Protocol in SIP)
 * @brief SIP Session Recording Protocol support
 * (RFC 7866 - Session Recording Protocol in SIP)
 * @{
 *
 * \section PJSIP_SIPREC_REFERENCE References
 *
 * References:
 *  - <A HREF="http://www.ietf.org/rfc/rfc7866.txt">
 *    RFC 7866: Session Recording Protocol (siprec)
 *    in the Session Initiation Protocol (SIP)</A>
 */
PJ_BEGIN_DECL

/**
 * Initialize siprec module. This function must be called once during
 * application initialization, to register siprec module to SIP endpoint.
 *
 * @param endpt         The SIP endpoint instance.
 *
 * @return              PJ_SUCCESS if module is successfully initialized.
 */
PJ_DECL(pj_status_t) pjsip_siprec_init_module(pjsip_endpoint *endpt);


/**
 * Check if the value of Require header is equal to siprec.
 * 
 * @param req_hdr      Require header.
 * 
 * @return             PJ_TRUE if value of Require header is equal to siprec.
 */
PJ_DECL(pj_status_t)
pjsip_siprec_verify_require_hdr(pjsip_require_hdr *req_hdr);


/**
 * Checks if there is an attribute label for each media in the SDP.
 * 
 * @param media          The SDP media.
 * 
 * @return               PJ_TRUE if a label exists in the SDP.
 */
PJ_INLINE(pj_status_t)
pjsip_siprec_verify_sdp_attr_label(pjmedia_sdp_session *sdp)
{
    const pj_str_t STR_LABEL = {"label", 5};
    for (unsigned mi=0; mi<sdp->media_count; ++mi) {
        if(!pjmedia_sdp_media_find_attr(sdp->media[mi], &STR_LABEL, NULL))
            return PJ_FALSE;
    }
    return PJ_TRUE;
}


/**
 * Verifies that the incoming request has the siprec value
 * in the Require header and "+sip.src" parameter exist in the Contact header.
 * If both conditions are met, according to RFC 7866,
 * the INVITE request is a siprec. Otherwise, 
 * no changes are made to the request. if INVITE request is a siprec 
 * must have media attribute label exist in the SDP
 *
 * @param rdata         The incoming request to be verified.
 * @param metadata      The siprec metadata information
 * @param sdp_offer     The SDP media.
 * @param options       The options argument is bitmask combination of SIP 
 *                      features in pjsip_inv_option enumeration
 * @param dlg           The dialog instance.
 * @param endpt         Media endpoint instance.
 * @param p_tdata       Upon error, it will be filled with the final response
 *                      to be sent to the request sender.
 * 
 * @return   The function returns the following:
 *             - If the request includes the value siprec in the Require header
 *               and also includes "+sip.src" in the Contact header.
 *               PJ_SUCCESS and set PJSIP_INV_REQUIRE_SIPREC to options
 *             - Upon error condition (as described by RFC 7866), the
 *               function returns non-PJ_SUCCESS, and \a p_tdata 
 *               parameter SHOULD be set with a final response message
 *               to be sent to the sender of the request.
 */
PJ_DECL(pj_status_t) pjsip_siprec_verify_request(pjsip_rx_data *rdata,
                                                pj_str_t *metadata,    
                                                pjmedia_sdp_session *sdp_offer,                                       
                                                unsigned *options,
                                                pjsip_dialog *dlg,
                                                pjsip_endpoint *endpt,
                                                pjsip_tx_data **p_tdata);


/**
 * Find siprec metadata information from the message body
 * with "rs-metadata" Content-Type.
 *
 * @param pool               Pool to allocate memory.
 * @param body               The message body.
 * @param metadata           The siprec metadata
 *
 * @return                   Return PJ_SUCCESS if metadata exists.
 */

PJ_DECL(pj_status_t) pjsip_siprec_get_metadata(pj_pool_t *pool,
                                                pjsip_msg_body *body,
                                                pj_str_t* metadata);


PJ_END_DECL


/**
 * @}
 */


#endif  /* __PJSIP_SIPREC_H__ */
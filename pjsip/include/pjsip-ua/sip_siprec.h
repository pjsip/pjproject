#ifndef __PJSIP_SIPREC_H__
#define __PJSIP_SIPREC_H__

/**
 * @file sip_siprec.h
 * @brief SIP Session Recording Protocol (siprec) support (RFC 7866 - Session Recording Protocol in SIP)
 */


#include <pjsip-ua/sip_inv.h>
#include <pjsip/sip_msg.h>

/**
 * @defgroup PJSIP_SIPREC SIP Session Recording Protocol (siprec) support (RFC 7866 - Session Recording Protocol in SIP)
 * @brief SIP Session Recording Protocol support (RFC 7866 - Session Recording Protocol in SIP)
 * @{
 *
 * \section PJSIP_SIPREC_REFERENCE References
 *
 * References:
 *  - <A HREF="http://www.ietf.org/rfc/rfc7866.txt">RFC 7866: Session Recording Protocol (siprec)
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

PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_create_label(pjmedia_sdp_media *media);

PJ_DEF(pj_status_t) pjsip_siprec_verify_request(pjsip_rx_data *rdata,
                                              const pjmedia_sdp_session *sdp);
                                              
PJ_END_DECL


/**
 * @}
 */


#endif  /* __PJSIP_SIPREC_H__ */

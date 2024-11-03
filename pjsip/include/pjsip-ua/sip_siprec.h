#ifndef __PJSIP_SIPREC_H__
#define __PJSIP_SIPREC_H__

/**
 * @file sip_siprec.h
 * @brief SIP Session Timers support (RFC 4028 - Session Timer in SIP)
 */


#include <pjsip-ua/sip_inv.h>
#include <pjsip/sip_msg.h>


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


#endif  /* __PJSIP_SIPREC_H__ */

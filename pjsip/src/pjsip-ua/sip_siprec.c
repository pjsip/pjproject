#include <pjsip-ua/sip_siprec.h>
#include <pjsip/print_util.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_transaction.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/pool.h>


#define THIS_FILE               "sip_siprec.c"


static const pj_str_t STR_SIPREC         = {"siprec", 6};
static const pj_str_t STR_LABEL          = {"label", 5};


/* Deinitialize siprec */
static void pjsip_siprec_deinit_module(pjsip_endpoint *endpt)
{
    PJ_TODO(provide_initialized_flag_for_each_endpoint);
    PJ_UNUSED_ARG(endpt);
}


/*
 * Initialize siprec support in PJSIP. 
 */
PJ_DEF(pj_status_t) pjsip_siprec_init_module(pjsip_endpoint *endpt)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    /* Register 'siprec' capability to endpoint */
    status = pjsip_endpt_add_capability(endpt, NULL, PJSIP_H_SUPPORTED,
                                        NULL, 1, &STR_SIPREC);

    if (status != PJ_SUCCESS)
        return status;

    /* Register deinit module to be executed when PJLIB shutdown */
    if (pjsip_endpt_atexit(endpt, &pjsip_siprec_deinit_module) != PJ_SUCCESS) {
        /* Failure to register this function may cause this module won't 
         * work properly when the stack is restarted (without quitting 
         * application).
         */
        pj_assert(!"Failed to register Session Timer deinit.");
        PJ_LOG(1, (THIS_FILE, "Failed to register Session Timer deinit."));
    }

    return PJ_SUCCESS;
}


PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_create_label(pjmedia_sdp_media *answer)
{
    pjmedia_sdp_attr *attr;
    attr = pjmedia_sdp_media_find_attr(answer, &STR_LABEL, NULL);
    return attr;
}


/*
 * Verifies that the incoming request has the siprec value in the Require header.
 */
PJ_DEF(pj_status_t) pjsip_siprec_verify_request(pjsip_rx_data *rdata)
{
    pjsip_require_hdr *req_hdr;
    const pj_str_t str_require = {"Require", 7};

    /* Find Require header */
    req_hdr = (pjsip_require_hdr*) pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_require, NULL);

    if(req_hdr)
    {
        for (int i=0; i<req_hdr->count; ++i) {
            /* Check request has the siprec value in the Require header.*/
            if (pj_stricmp(&req_hdr->values[i], &STR_SIPREC)==0)
            {
                return PJ_TRUE;
            }
        }
    }

    /* No Require header or not exist siprec value in Require header */
    return PJ_FALSE;
}

#include <pjsip-ua/sip_siprec.h>
#include <pjsip/print_util.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_multipart.h>
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


/**
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


/**
 * Returns the label attribute in the SDP offer 
 */
PJ_DEF(pjmedia_sdp_attr*) pjmedia_sdp_attr_get_label(pjmedia_sdp_media *sdp_media)
{
    pjmedia_sdp_attr *attr;
    attr = pjmedia_sdp_media_find_attr(sdp_media, &STR_LABEL, NULL);
    return attr;
}


/**
 * Checks if there is an attribute label for each media in the SDP.
 */
PJ_DEF(pj_status_t) pjsip_siprec_verify_sdp_attr_label(pjmedia_sdp_session *sdp)
{
    for (unsigned mi=0; mi<sdp->media_count; ++mi) {
        if(!pjmedia_sdp_attr_get_label(sdp->media[mi]))
            return PJ_FALSE;
    }
    return PJ_TRUE;
}


/**
 * Check if the value of Require header is equal to siprec.
 */ 
PJ_DEF(pj_status_t) pjsip_siprec_verify_require_hdr(pjsip_require_hdr *req_hdr)
{
    for (int i=0; i<req_hdr->count; ++i) {
        /* Check request has the siprec value in the Require header.*/
        if (pj_stricmp(&req_hdr->values[i], &STR_SIPREC)==0)
        {
            return PJ_TRUE;
        }
    }
    return PJ_FALSE;
}


/**
 * Verifies that the incoming request is a siprec request or not.
 */
PJ_DEF(pj_status_t) pjsip_siprec_verify_request(pjsip_rx_data *rdata,    
                                                pjmedia_sdp_session *sdp_offer,                                     
                                                unsigned *options,
                                                pjsip_dialog *dlg,
                                                pjsip_endpoint *endpt,
                                                pjsip_tx_data **p_tdata)
{
    pjsip_require_hdr *req_hdr;
    pjsip_contact_hdr *conatct_hdr;
    const pj_str_t str_require = {"Require", 7};
    const pj_str_t str_src = {"+sip.src", 8};
    int code = 200;
    pj_status_t status = PJ_SUCCESS;
    const char *warn_text = NULL;
    pjsip_hdr res_hdr_list;
    pjsip_param  *param;

    /* Init return arguments. */
    if (p_tdata) *p_tdata = NULL;

    /* Init response header list */
    pj_list_init(&res_hdr_list);

    /* Find Require header */
    req_hdr = (pjsip_require_hdr*) pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_require, NULL);

    if(!req_hdr || (pjsip_siprec_verify_require_hdr(req_hdr) == PJ_FALSE)){
        return PJ_SUCCESS;
    }
    
    /* Find Conatct header */
    conatct_hdr = (pjsip_contact_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, NULL);

    if(!conatct_hdr || !conatct_hdr->uri){
        return PJ_SUCCESS;
    }

    /* Check "+sip.src" parameter exist in the Contact header */
    if(!pjsip_param_find(&conatct_hdr->other_param, &str_src)){
        return PJ_SUCCESS;
    }

    /* Currently, SIPREC INVITE requests without SPD are not supported. */
    if(!sdp_offer){
        code = PJSIP_SC_BAD_REQUEST;
        warn_text = "SIPREC INVITE without SDP is not supported";
        goto on_return;
    }

    /* Check that the media attribute label exist in the SDP */
    if(pjsip_siprec_verify_sdp_attr_label(sdp_offer) == PJ_FALSE){
        code = PJSIP_SC_BAD_REQUEST;
        warn_text = "SDP must have label media attribute";
        goto on_return;
    }

    *options |= PJSIP_INV_REQUIRE_SIPREC;
    return status;

on_return:

    /* Create response if necessary */
    if (code != 200) {
        pjsip_tx_data *tdata;
        const pjsip_hdr *h;

        if (dlg) {
            status = pjsip_dlg_create_response(dlg, rdata, code, NULL, 
                                               &tdata);
        } else {
            status = pjsip_endpt_create_response(endpt, rdata, code, NULL, 
                                                 &tdata);
        }

        if (status != PJ_SUCCESS)
            return status;

        /* Add response headers. */
        h = res_hdr_list.next;
        while (h != &res_hdr_list) {    
            pjsip_hdr *cloned;

            cloned = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, h);
            PJ_ASSERT_RETURN(cloned, PJ_ENOMEM);

            pjsip_msg_add_hdr(tdata->msg, cloned);

            h = h->next;
        }

        /* Add warn text, if any */
        if (warn_text) {
            pjsip_warning_hdr *warn_hdr;
            pj_str_t warn_value = pj_str((char*)warn_text);

            warn_hdr=pjsip_warning_hdr_create(tdata->pool, 399, 
                                                pjsip_endpt_name(endpt),
                                                &warn_value);
            pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)warn_hdr);
        }

        *p_tdata = tdata;

        /* Can not return PJ_SUCCESS when response message is produced.
         * Ref: PROTOS test ~#2490
         */
        if (status == PJ_SUCCESS)
            status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

    }

    return status;
}


PJ_DECL(pj_str_t*) pjsip_siprec_get_metadata(pj_pool_t *pool,
                                                 pjsip_msg_body *body)
{
    pj_str_t* siprec_metadata;
    pjsip_media_type application_metadata;

    siprec_metadata = PJ_POOL_ZALLOC_T(pool, pj_str_t);

    if (!body) {
        return siprec_metadata;
    }

    pjsip_media_type_init2(&application_metadata, "application", "rs-metadata+xml");

    pjsip_multipart_part *metadata_part;
    metadata_part = pjsip_multipart_find_part(body, &application_metadata, NULL);

    if(metadata_part){
        siprec_metadata->ptr = (char*)metadata_part->body->data;
        siprec_metadata->slen = metadata_part->body->len;
    }
    
    return siprec_metadata;
}
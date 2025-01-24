/* 
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
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
 * Checks if the INVITE request is SIPREC.
 */
static pj_status_t pjsip_siprec_check_request(pjsip_rx_data *rdata)
{
    const pj_str_t str_require = {"Require", 7};
    const pj_str_t str_src = {"+sip.src", 8};
    pjsip_require_hdr *req_hdr;
    pjsip_contact_hdr *contact_hdr;

    /* Find Require header */
    req_hdr = (pjsip_require_hdr*)
        pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_require, NULL);

    if(!req_hdr || (pjsip_siprec_verify_require_hdr(req_hdr) == PJ_FALSE)){
        return PJ_FALSE;
    }

    /* Find Contact header */
    contact_hdr = (pjsip_contact_hdr*)
            pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, NULL);

    if(!contact_hdr || !contact_hdr->uri){
        return PJ_FALSE;
    }

    /* Check "+sip.src" parameter exist in the Contact header */
    if(!pjsip_param_find(&contact_hdr->other_param, &str_src)){
        return PJ_FALSE;
    }
    return PJ_TRUE;
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
    if (pjsip_endpt_atexit(endpt, &pjsip_siprec_deinit_module) != PJ_SUCCESS)
    {
        /* Failure to register this function may cause this module won't 
         * work properly when the stack is restarted (without quitting 
         * application).
         */
        pj_assert(!"Failed to register Siprec deinit.");
        PJ_LOG(1, (THIS_FILE, "Failed to register Siprec deinit."));
    }

    return PJ_SUCCESS;
}


/**
 * Check if the value of Require header is equal to siprec.
 */ 
PJ_DEF(pj_status_t) pjsip_siprec_verify_require_hdr(pjsip_require_hdr *req_hdr)
{
    unsigned i;
    for (i=0; i<req_hdr->count; ++i) {
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
                                              pj_str_t *metadata,
                                              pjmedia_sdp_session *sdp_offer,
                                              unsigned *options,
                                              pjsip_dialog *dlg,
                                              pjsip_endpoint *endpt,
                                              pjsip_tx_data **p_tdata)
{
    int code = 200;
    pj_status_t status = PJ_SUCCESS;
    const char *warn_text = NULL;
    pjsip_hdr res_hdr_list;
    unsigned mi;

    /* Init return arguments. */
    if (p_tdata) *p_tdata = NULL;

    /* Init response header list */
    pj_list_init(&res_hdr_list);

    /* Checks if The SIPREC request option is inactive */
    if (!(*options & PJSIP_INV_SUPPORT_SIPREC)){
        return PJ_SUCCESS;
    }

    /* Checks if the INVITE request is SIPREC */
    if (pjsip_siprec_check_request(rdata) == PJ_FALSE){
        /* The SIPREC request option is mandatory */ 
        if (*options & PJSIP_INV_REQUIRE_SIPREC){
            code = PJSIP_SC_BAD_REQUEST;
            warn_text = "The INVITE request must be SIPREC";
            goto on_return;    
        }else {
            /* The SIPREC request option is optional */ 
            return PJ_SUCCESS;
        }
    }

    /* Checks if the body exists */
    if (!rdata->msg_info.msg->body) {
        code = PJSIP_SC_BAD_REQUEST;
        warn_text = "SIPREC INVITE must have a body";
        goto on_return; 
    }

    /* Currently, SIPREC INVITE requests without SDP are not supported. */
    if(!sdp_offer){
        code = PJSIP_SC_BAD_REQUEST;
        warn_text = "SIPREC INVITE without SDP is not supported";
        goto on_return;
    }

    /* Check that the media attribute label exist in the SDP */
    for (mi=0; mi<sdp_offer->media_count; ++mi) {
        if (!pjmedia_sdp_media_find_attr(sdp_offer->media[mi],
                                            &STR_LABEL, NULL)){
            code = PJSIP_SC_BAD_REQUEST;
            warn_text = "SDP must have label media attribute";
            goto on_return;
        }
    }

    status = pjsip_siprec_get_metadata(rdata->tp_info.pool,
                                        rdata->msg_info.msg->body,
                                        metadata);
    
    if(status != PJ_SUCCESS) {
        code = PJSIP_SC_BAD_REQUEST;
        warn_text = "SIPREC INVITE must have a 'rs-metadata' Content-Type";
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


/**
 * Find siprec metadata from the message body
 */
PJ_DEF(pj_status_t) pjsip_siprec_get_metadata(pj_pool_t *pool,
                                              pjsip_msg_body *body,
                                              pj_str_t* metadata)
{
    pjsip_media_type app_metadata;
    pjsip_multipart_part *metadata_part;

    PJ_UNUSED_ARG(pool);
    pjsip_media_type_init2(&app_metadata, "application", "rs-metadata");
    metadata_part = pjsip_multipart_find_part(body, &app_metadata, NULL);

    if(!metadata_part)
        return PJ_ENOTFOUND;

    metadata->ptr  = (char*)metadata_part->body->data;
    metadata->slen = metadata_part->body->len;
    
    return PJ_SUCCESS;
}

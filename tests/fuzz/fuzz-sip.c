/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjsip.h>
#include <pjsip/sip_types.h>
#include <pjsip/sip_multipart.h>
#include <pjsip/sip_tel_uri.h>


pjsip_endpoint *endpt;
pj_caching_pool caching_pool;

#define POOL_SIZE       8000
#define PJSIP_TEST_MEM_SIZE         (2*1024*1024)

#define kMinInputLength 10
#define kMaxInputLength 5120

/* Standard header types to test */
static const pjsip_hdr_e hdr_types[] = {
    PJSIP_H_AUTHORIZATION,
    PJSIP_H_PROXY_AUTHORIZATION,
    PJSIP_H_WWW_AUTHENTICATE,
    PJSIP_H_PROXY_AUTHENTICATE,
    PJSIP_H_ROUTE,
    PJSIP_H_RECORD_ROUTE,
    PJSIP_H_CONTACT
};

static pjsip_msg* parse_message(pj_pool_t *pool, char *data, size_t size)
{
    pjsip_parser_err_report err_list;
    pj_list_init(&err_list);
    return pjsip_parse_msg(pool, data, size, &err_list);
}

/* Multipart body parsing */
static void do_test_multipart(pj_pool_t *pool, pjsip_msg *msg)
{
    if (!msg->body)
        return;

    pjsip_msg_body *body = msg->body;

    if (body->content_type.type.slen > 0 &&
        pj_stricmp2(&body->content_type.type, "multipart") == 0)
    {
        pjsip_msg_body *multipart_body = NULL;
        if (body->data && body->len > 0) {
            multipart_body = pjsip_multipart_parse(pool, (char *)body->data,
                                                   body->len, &body->content_type, 0);
            if (!multipart_body)
                return;
        } else {
            multipart_body = body;
        }

        pjsip_multipart_part *part = pjsip_multipart_get_first_part(multipart_body);

        while (part) {
            pjsip_media_type ctype_app, ctype_text;

            ctype_app.type = pj_str("application");
            ctype_app.subtype = pj_str("sdp");
            ctype_text.type = pj_str("text");
            ctype_text.subtype = pj_str("plain");

            pjsip_multipart_find_part(multipart_body, &ctype_app, NULL);
            pjsip_multipart_find_part(multipart_body, &ctype_text, NULL);

            part = pjsip_multipart_get_next_part(multipart_body, part);
        }

        pjsip_media_type search_type;
        search_type.type = pj_str("application");
        search_type.subtype = pj_str("sdp");
        pjsip_multipart_find_part(multipart_body, &search_type, NULL);
    }
}

/* Tel URI parsing */
static void do_test_tel_uri(pj_pool_t *pool, char *data, size_t size)
{
    pj_str_t uri_str;
    pjsip_uri *uri;

    if (size < 4 || size > 256)
        return;

    uri_str.ptr = data;
    uri_str.slen = (pj_ssize_t)size;

    uri = pjsip_parse_uri(pool, uri_str.ptr, uri_str.slen, 0);

    if (uri && size > 20) {
        pj_str_t uri2_str;
        pjsip_uri *uri2;

        uri2_str.ptr = data + 10;
        uri2_str.slen = (pj_ssize_t)(size - 10);

        uri2 = pjsip_parse_uri(pool, uri2_str.ptr, uri2_str.slen, 0);
        if (uri2) {
            pjsip_uri_cmp(PJSIP_URI_IN_FROMTO_HDR, uri, uri2);
            pjsip_uri_cmp(PJSIP_URI_IN_REQ_URI, uri, uri2);
        }
    }
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (Size < kMinInputLength || Size > kMaxInputLength){
        return 1;
    }

    char *DataFx;
    DataFx = (char *)calloc((Size+1), sizeof(char));
    if (DataFx == NULL)
        return 0;
    memcpy((void *)DataFx, (void *)Data, Size);

    static int initialized = 0;
    if (!initialized) {
        pj_status_t status;
        pj_log_set_level(0);

        status = pj_init();
        if (status != PJ_SUCCESS) {
            free(DataFx);
            return 0;
        }
        status = pjlib_util_init();
        if (status != PJ_SUCCESS) {
            free(DataFx);
            return 0;
        }

        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy,
                            PJSIP_TEST_MEM_SIZE);

        status = pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);
        if (status != PJ_SUCCESS || endpt == NULL) {
            free(DataFx);
            return 0;
        }
        status = pjsip_tsx_layer_init_module(endpt);
        if (status != PJ_SUCCESS) {
            free(DataFx);
            return 0;
        }
        status = pjsip_loop_start(endpt, NULL);
        if (status != PJ_SUCCESS) {
            free(DataFx);
            return 0;
        }

        initialized = 1;
    }

    /* Parse message once and reuse it across all tests to improve throughput */
    pj_pool_t *pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
    pjsip_msg *msg = parse_message(pool, DataFx, Size);
    if (msg) {
        do_test_multipart(pool, msg);

        const char *hdr_names[] = {
            "Replaces", "Refer-To", "Refer-Sub", "Subscription-State",
            "Session-Expires", "Min-SE", "RSeq", "RAck"
        };
        for (int i = 0; i < (int)(sizeof(hdr_names) / sizeof(hdr_names[0])); i++) {
            pj_str_t hdr_name = pj_str((char *)hdr_names[i]);
            pjsip_msg_find_hdr_by_name(msg, &hdr_name, NULL);
        }

        /* Test standard header types using enum-based loop */
        for (int i = 0; i < (int)(sizeof(hdr_types) / sizeof(hdr_types[0])); i++) {
            pjsip_msg_find_hdr(msg, hdr_types[i], NULL);
        }
    }

    /* Tel URI parsing still needs separate handling as it doesn't use the msg */
    do_test_tel_uri(pool, DataFx, Size);

    pjsip_endpt_release_pool(endpt, pool);

    free(DataFx);

    return 0;
}

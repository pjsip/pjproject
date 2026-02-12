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

static pjsip_msg* parse_message(pj_pool_t *pool, char *data, size_t size)
{
    pjsip_parser_err_report err_list;
    pj_list_init(&err_list);
    return pjsip_parse_msg(pool, data, size, &err_list);
}

/* Basic message parsing */
int sipParser(char *DataFx, size_t Size)
{
    pj_pool_t *pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
    pjsip_msg *msg = parse_message(pool, DataFx, Size);
    int ret = (msg == NULL) ? 1 : 0;
    pjsip_endpt_release_pool(endpt, pool);
    return ret;
}

/* Multipart body parsing */
static void do_test_multipart(pj_pool_t *pool, pjsip_msg *msg)
{
    if (!msg->body)
        return;

    pjsip_msg_body *body = msg->body;

    if (body->content_type.type.slen > 0 &&
        pj_stricmp2(&body->content_type.type, "multipart") == 0 &&
        body->data && body->len > 0)
    {
        pjsip_msg_body *multipart_body = pjsip_multipart_parse(pool, (char *)body->data,
                                                                body->len, &body->content_type, 0);
        if (!multipart_body)
            return;

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

void test_multipart(char *DataFx, size_t Size)
{
    pj_pool_t *pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
    pjsip_msg *msg = parse_message(pool, DataFx, Size);
    if (msg) {
        do_test_multipart(pool, msg);
    }
    pjsip_endpt_release_pool(endpt, pool);
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

void test_tel_uri(char *DataFx, size_t Size)
{
    pj_pool_t *pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
    do_test_tel_uri(pool, DataFx, Size);
    pjsip_endpt_release_pool(endpt, pool);
}

/* Dialog and transfer headers */
void test_dialog_headers(char *DataFx, size_t Size)
{
    const char *hdr_names[] = {
        "Replaces", "Refer-To", "Refer-Sub", "Subscription-State",
        "Session-Expires", "Min-SE", "RSeq", "RAck"
    };
    int i;

    pj_pool_t *pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
    pjsip_msg *msg = parse_message(pool, DataFx, Size);
    if (msg) {
        for (i = 0; i < (int)(sizeof(hdr_names) / sizeof(hdr_names[0])); i++) {
            pj_str_t hdr_name = pj_str((char *)hdr_names[i]);
            pjsip_msg_find_hdr_by_name(msg, &hdr_name, NULL);
        }
    }
    pjsip_endpt_release_pool(endpt, pool);
}

/* Authentication challenge parsing */
void test_auth_parsing(char *DataFx, size_t Size)
{
    pj_pool_t *pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
    pjsip_msg *msg = parse_message(pool, DataFx, Size);
    if (msg) {
        pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL);
        pjsip_msg_find_hdr(msg, PJSIP_H_PROXY_AUTHORIZATION, NULL);
        pjsip_msg_find_hdr(msg, PJSIP_H_WWW_AUTHENTICATE, NULL);
        pjsip_msg_find_hdr(msg, PJSIP_H_PROXY_AUTHENTICATE, NULL);
    }
    pjsip_endpt_release_pool(endpt, pool);
}

/* Route and Record-Route processing */
void test_routing(char *DataFx, size_t Size)
{
    pj_pool_t *pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
    pjsip_msg *msg = parse_message(pool, DataFx, Size);
    if (msg) {
        pjsip_msg_find_hdr(msg, PJSIP_H_ROUTE, NULL);
        pjsip_msg_find_hdr(msg, PJSIP_H_RECORD_ROUTE, NULL);
        pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
    }
    pjsip_endpt_release_pool(endpt, pool);
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (Size < kMinInputLength || Size > kMaxInputLength){
        return 1;
    }

    char *DataFx;
    DataFx = (char *)calloc((Size+1), sizeof(char));
    memcpy((void *)DataFx, (void *)Data, Size);

    static int initialized = 0;
    if (!initialized) {
        pj_log_set_level(0);

        pj_init();
        pjlib_util_init();

        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy,
                            PJSIP_TEST_MEM_SIZE);

        pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);
        pjsip_tsx_layer_init_module(endpt);
        pjsip_loop_start(endpt, NULL);

        initialized = 1;
    }

    sipParser(DataFx, Size);
    test_multipart(DataFx, Size);
    test_tel_uri(DataFx, Size);
    test_dialog_headers(DataFx, Size);
    test_auth_parsing(DataFx, Size);
    test_routing(DataFx, Size);

    free(DataFx);

    return 0;
}

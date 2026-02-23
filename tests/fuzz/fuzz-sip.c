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
#include <pjsip/sip_auth.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_dialog.h>

pjsip_endpoint *endpt;
pj_caching_pool caching_pool;

#define POOL_SIZE       8000
#define PJSIP_TEST_MEM_SIZE         (2*1024*1024)

#define kMinInputLength 10
#define kMaxInputLength 5120

/* Transaction user module */
static pjsip_module tsx_user_module;

/* Transaction state callback */
static void on_tsx_state(pjsip_transaction *tsx, pjsip_event *event)
{
    PJ_UNUSED_ARG(tsx);
    PJ_UNUSED_ARG(event);
}

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

/* Test UAS transaction creation and message feeding */
static void do_test_transaction_layer(pjsip_msg *msg, pjsip_rx_data *rdata)
{
    pj_str_t tsx_key;
    pjsip_via_hdr *via_hdr;

    if (!msg || msg->type != PJSIP_REQUEST_MSG || !rdata)
        return;

    /* Verify required headers exist */
    if (!pjsip_msg_find_hdr(msg, PJSIP_H_CALL_ID, NULL) ||
        !pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL))
        return;

    via_hdr = (pjsip_via_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL);
    if (!via_hdr)
        return;

    /* Skip transaction creation if transport not available */
    if (!rdata->tp_info.transport)
        return;

    /* Ensure From/To headers have tags */
    pjsip_from_hdr *from_hdr = (pjsip_from_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL);
    pjsip_to_hdr *to_hdr = (pjsip_to_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);

    if (from_hdr && pj_stricmp2(&from_hdr->tag, "") == 0)
        from_hdr->tag = pj_str("from-tag-123");

    if (to_hdr && pj_stricmp2(&to_hdr->tag, "") == 0)
        to_hdr->tag = pj_str("to-tag-456");

    /* Ensure Via header has valid host/port for response addressing */
    if (via_hdr->sent_by.host.slen == 0) {
        via_hdr->sent_by.host = pj_str("127.0.0.1");
        via_hdr->sent_by.port = 5060;
    }

    /* Test transaction key generation */
    if (pjsip_tsx_create_key(rdata->tp_info.pool, &tsx_key, PJSIP_ROLE_UAS,
                             &msg->line.req.method, rdata) == PJ_SUCCESS) {
        pjsip_tsx_layer_find_tsx(&tsx_key, PJ_FALSE);
    }
}

/* Test synthetic transaction creation */
static void do_test_transaction_synthetic(void)
{
    pj_str_t target_uri_str = pj_str("sip:test@example.com");
    pj_str_t from_str = pj_str("sip:caller@example.com");
    pj_str_t to_str = pj_str("sip:callee@example.com");
    pj_str_t call_id_str = pj_str("call-id-fuzz-test@example.com");
    pjsip_tx_data *tdata;

    if (pjsip_endpt_create_request(endpt, &pjsip_invite_method,
                                   &target_uri_str, &from_str, &to_str,
                                   NULL, &call_id_str, -1, NULL, &tdata) == PJ_SUCCESS) {
        pjsip_tx_data_dec_ref(tdata);
    }
}

/* Test UAC transaction response handling */
static void do_test_transaction_uac(pjsip_msg *msg, pjsip_rx_data *rdata)
{
    pj_str_t tsx_key;

    if (!msg || msg->type != PJSIP_RESPONSE_MSG || !rdata)
        return;

    /* Verify CSeq header exists */
    if (!pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL))
        return;

    /* Skip if no transport available */
    if (!rdata->tp_info.transport)
        return;

    /* Test transaction key generation for responses */
    if (pjsip_tsx_create_key(rdata->tp_info.pool, &tsx_key, PJSIP_ROLE_UAC,
                             &rdata->msg_info.cseq->method, rdata) == PJ_SUCCESS) {
        pjsip_tsx_layer_find_tsx(&tsx_key, PJ_FALSE);
    }
}

/* Test authentication client */
static void do_test_auth_client(pj_pool_t *pool, pjsip_msg *msg)
{
    pjsip_auth_clt_sess auth_sess;
    pjsip_hdr *hdr = NULL;
    pj_status_t status;

    /* Only test with 401/407 responses */
    if (!msg || msg->type != PJSIP_RESPONSE_MSG)
        return;

    if (msg->line.status.code != 401 && msg->line.status.code != 407)
        return;

    /* Initialize auth client session */
    if (pjsip_auth_clt_init(&auth_sess, endpt, pool, 0) != PJ_SUCCESS)
        return;

    /* Set up credentials */
    pjsip_cred_info cred;
    pj_bzero(&cred, sizeof(cred));
    cred.realm = pj_str("test");
    cred.username = pj_str("user");
    cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cred.data = pj_str("password");

    if (pjsip_auth_clt_set_credentials(&auth_sess, 1, &cred) != PJ_SUCCESS)
        goto cleanup;

    /* Parse authentication headers */
    if (msg->line.status.code == 401)
        hdr = pjsip_msg_find_hdr(msg, PJSIP_H_WWW_AUTHENTICATE, NULL);
    else
        hdr = pjsip_msg_find_hdr(msg, PJSIP_H_PROXY_AUTHENTICATE, NULL);

    if (hdr) {
        /* Create a dummy request to test reinit */
        pjsip_tx_data *tdata = NULL;
        pjsip_method method;
        pj_str_t target_uri = pj_str("sip:server.com");
        pj_str_t from_uri = pj_str("sip:client@local");
        pj_str_t to_uri = pj_str("sip:server.com");
        pj_str_t contact = pj_str("sip:client@local");

        pjsip_method_set(&method, PJSIP_REGISTER_METHOD);

        status = pjsip_endpt_create_request(endpt, &method, &target_uri,
                                           &from_uri, &to_uri, &contact,
                                           NULL, -1, NULL, &tdata);
        if (status != PJ_SUCCESS || !tdata)
            goto cleanup;

        /* Initialize request with auth */
        pjsip_auth_clt_init_req(&auth_sess, tdata);

        /* Create rx_data from the 401/407 message */
        pjsip_rx_data rdata;
        pj_bzero(&rdata, sizeof(rdata));
        rdata.msg_info.msg = msg;
        rdata.msg_info.info = NULL;
        rdata.msg_info.len = 1024;

        /* Try to reinit with the challenge */
        pjsip_tx_data *new_tdata = NULL;
        if (pjsip_auth_clt_reinit_req(&auth_sess, &rdata, tdata, &new_tdata) == PJ_SUCCESS && new_tdata) {
            pjsip_tx_data_dec_ref(new_tdata);
        }

        pjsip_tx_data_dec_ref(tdata);
    }

cleanup:
    pjsip_auth_clt_deinit(&auth_sess);
}

/* Test dialog creation */
static void do_test_dialog(pjsip_msg *msg)
{
    pjsip_dialog *dlg = NULL;
    pjsip_to_hdr *to;
    pjsip_from_hdr *from;
    pj_str_t local_uri = pj_str("sip:local@test.com");
    pj_str_t remote_uri = pj_str("sip:remote@test.com");
    pj_str_t target = pj_str("sip:remote@test.com");

    if (!msg || msg->type != PJSIP_REQUEST_MSG)
        return;

    if (msg->line.req.method.id != PJSIP_INVITE_METHOD)
        return;

    /* Get To and From headers */
    to = (pjsip_to_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);
    from = (pjsip_from_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL);
    if (!to || !from)
        return;

    /* Create UAC dialog */
    if (pjsip_dlg_create_uac(pjsip_ua_instance(), &local_uri, NULL,
                             &remote_uri, &target, &dlg) == PJ_SUCCESS && dlg) {
        pjsip_dlg_inc_lock(dlg);

        /* Build route set from all Route headers */
        pjsip_route_hdr route_set;
        pjsip_route_hdr *route;
        pj_list_init(&route_set);
        route = (pjsip_route_hdr*)
                pjsip_msg_find_hdr(msg, PJSIP_H_ROUTE, NULL);
        while (route) {
            pjsip_route_hdr *route_clone = (pjsip_route_hdr*)
                    pjsip_hdr_clone(dlg->pool, route);
            pj_list_push_back(&route_set, route_clone);
            route = (pjsip_route_hdr*)
                    pjsip_msg_find_hdr(msg, PJSIP_H_ROUTE,
                                        route->next);
        }
        if (!pj_list_empty(&route_set)) {
            pjsip_dlg_set_route_set(dlg, &route_set);
        }

        pjsip_dlg_dec_lock(dlg);
    }
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    char *DataFx;
    pj_pool_t *pool;
    pjsip_msg *msg;
    static int initialized = 0;

    if (Size < kMinInputLength || Size > kMaxInputLength)
        return 1;

    DataFx = (char *)calloc((Size+1), sizeof(char));
    if (DataFx == NULL)
        return 0;
    memcpy((void *)DataFx, (void *)Data, Size);

    if (!initialized) {
        pj_log_set_level(0);

        if (pj_init() != PJ_SUCCESS || pjlib_util_init() != PJ_SUCCESS) {
            free(DataFx);
            return 0;
        }

        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy,
                            PJSIP_TEST_MEM_SIZE);

        if (pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt) != PJ_SUCCESS || !endpt) {
            free(DataFx);
            return 0;
        }

        if (pjsip_tsx_layer_init_module(endpt) != PJ_SUCCESS ||
            pjsip_loop_start(endpt, NULL) != PJ_SUCCESS) {
            free(DataFx);
            return 0;
        }

        pjsip_ua_init_module(endpt, NULL);

        /* Initialize transaction user module */
        pj_bzero(&tsx_user_module, sizeof(tsx_user_module));
        tsx_user_module.name = pj_str("tsx-user");
        tsx_user_module.id = -1;
        tsx_user_module.priority = PJSIP_MOD_PRIORITY_APPLICATION;
        tsx_user_module.on_tsx_state = &on_tsx_state;
        pjsip_endpt_register_module(endpt, &tsx_user_module);

        do_test_transaction_synthetic();

        initialized = 1;
    }

    pool = pjsip_endpt_create_pool(endpt, NULL, POOL_SIZE, POOL_SIZE);
    msg = parse_message(pool, DataFx, Size);

    if (msg) {
        pjsip_rx_data rdata;
        pj_sockaddr_in remote_addr;
        pjsip_transport *fake_transport = NULL;
        int i;

        do_test_multipart(pool, msg);

        /* Test named headers */
        const char *hdr_names[] = {
            "Replaces", "Refer-To", "Refer-Sub", "Subscription-State",
            "Session-Expires", "Min-SE", "RSeq", "RAck"
        };
        for (i = 0; i < (int)(sizeof(hdr_names) / sizeof(hdr_names[0])); i++) {
            pj_str_t hdr_name = pj_str((char *)hdr_names[i]);
            pjsip_msg_find_hdr_by_name(msg, &hdr_name, NULL);
        }

        /* Test standard header types */
        for (i = 0; i < (int)(sizeof(hdr_types) / sizeof(hdr_types[0])); i++) {
            pjsip_msg_find_hdr(msg, hdr_types[i], NULL);
        }

        /* Setup rx_data for transaction testing */
        pj_bzero(&rdata, sizeof(rdata));
        rdata.msg_info.msg = msg;
        rdata.msg_info.len = Size;
        rdata.msg_info.info = DataFx;
        rdata.tp_info.pool = pool;

        /* Populate header shortcuts */
        rdata.msg_info.from = (pjsip_from_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL);
        rdata.msg_info.to = (pjsip_to_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);
        rdata.msg_info.via = (pjsip_via_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL);
        rdata.msg_info.cseq = (pjsip_cseq_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
        rdata.msg_info.cid = (pjsip_cid_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CALL_ID, NULL);
        rdata.msg_info.max_fwd = (pjsip_max_fwd_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_MAX_FORWARDS, NULL);

        /* Setup transport info */
        pj_bzero(&remote_addr, sizeof(remote_addr));
        remote_addr.sin_family = PJ_AF_INET;
        remote_addr.sin_addr.s_addr = pj_htonl(0x7F000001);
        remote_addr.sin_port = pj_htons(5060);

        pj_memcpy(&rdata.pkt_info.src_addr, &remote_addr, sizeof(remote_addr));
        rdata.pkt_info.src_addr_len = sizeof(remote_addr);
        if (Size < sizeof(rdata.pkt_info.packet)) {
            pj_memcpy(rdata.pkt_info.packet, DataFx, Size);
            rdata.pkt_info.len = Size;
        } else {
            pj_memcpy(rdata.pkt_info.packet, DataFx, sizeof(rdata.pkt_info.packet));
            rdata.pkt_info.len = sizeof(rdata.pkt_info.packet);
        }

        /* Acquire dummy UDP transport */
        if (pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_UDP,
                                          &remote_addr, sizeof(remote_addr),
                                          NULL, &fake_transport) == PJ_SUCCESS) {
            rdata.tp_info.transport = fake_transport;
        }

        do_test_transaction_layer(msg, &rdata);

        if (msg->type == PJSIP_RESPONSE_MSG) {
            do_test_transaction_uac(msg, &rdata);
        }

        do_test_auth_client(pool, msg);
        do_test_dialog(msg);

        /* Release transport */
        if (fake_transport) {
            pjsip_transport_dec_ref(fake_transport);
        }
    }

    do_test_tel_uri(pool, DataFx, Size);

    pjsip_endpt_release_pool(endpt, pool);
    free(DataFx);

    return 0;
}


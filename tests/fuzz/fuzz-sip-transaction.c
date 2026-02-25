/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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
#include <string.h>

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjsip.h>

/* Global PJSIP endpoint and memory pool */
pjsip_endpoint *endpt;
pj_caching_pool caching_pool;

#define POOL_SIZE 8000
#define PJSIP_TEST_MEM_SIZE (2*1024*1024)

/* Transaction user module for receiving transaction state callbacks */
static pjsip_module tsx_user;

/* Callback for transaction state changes (no-op for fuzzing) */
static void on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    PJ_UNUSED_ARG(tsx);
    PJ_UNUSED_ARG(e);
}

/* Test server-side (UAS) transaction creation and message handling */
static void test_uas_transaction(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    /* Null-terminate input for parser (required by pjsip_parse_msg API) */
    char *data_copy = (char*)pj_pool_alloc(pool, size + 1);
    pj_memcpy(data_copy, data, size);
    data_copy[size] = '\0';

    pjsip_parser_err_report err_list;
    pj_list_init(&err_list);

    pjsip_msg *msg = pjsip_parse_msg(pool, data_copy, size, &err_list);
    if (!msg || msg->type != PJSIP_REQUEST_MSG)
        return;

    /* Setup received message data structure before creating transaction */
    pjsip_rx_data rdata;
    pj_bzero(&rdata, sizeof(rdata));
    rdata.msg_info.msg = msg;
    rdata.tp_info.pool = pool;
    
    /* Populate message info fields from parsed message headers */
    rdata.msg_info.via = (pjsip_via_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL);
    rdata.msg_info.from = (pjsip_from_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL);
    rdata.msg_info.to = (pjsip_to_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);
    rdata.msg_info.cseq = (pjsip_cseq_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    
    /* Skip if required headers are missing */
    if (!rdata.msg_info.via || !rdata.msg_info.from || !rdata.msg_info.to || !rdata.msg_info.cseq)
        return;

    /* Create UAS transaction with rdata */
    pjsip_transaction *tsx = NULL;
    if (pjsip_tsx_create_uas(&tsx_user, &rdata, &tsx) != PJ_SUCCESS || !tsx)
        return;

    /* Feed parsed message into transaction */
    pjsip_tsx_recv_msg(tsx, &rdata);

    /* Create and send response through transaction */
    pjsip_tx_data *tdata = NULL;
    if (pjsip_endpt_create_response(endpt, &rdata, 200, NULL, &tdata) == PJ_SUCCESS && tdata) {
        pjsip_tsx_send_msg(tsx, tdata);
    }

    /* Force transaction termination to prevent leaks */
    if (tsx->state != PJSIP_TSX_STATE_TERMINATED)
        pjsip_tsx_terminate(tsx, 500);
}

/* Test client-side (UAC) transaction creation and sending */
static void test_uac_transaction(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    if (size < 20)
        return;

    /* Extract fuzzer input as SIP URIs */
    pj_str_t target = {(char*)data, size > 100 ? 100 : (pj_ssize_t)size};
    pj_str_t from = {(char*)data + 10, size > 110 ? 50 : (pj_ssize_t)(size - 10)};
    pj_str_t to = {(char*)data + 15, size > 115 ? 50 : (pj_ssize_t)(size - 15)};

    /* Create outgoing request message */
    pjsip_tx_data *tdata = NULL;
    if (pjsip_endpt_create_request(endpt, &pjsip_options_method,
                                    &target, &from, &to,
                                    NULL, NULL, -1, NULL, &tdata) != PJ_SUCCESS || !tdata)
        return;

    /* Create UAC transaction and send message */
    pjsip_transaction *tsx = NULL;
    pj_status_t status = pjsip_tsx_create_uac(&tsx_user, tdata, &tsx);
    
    if (status == PJ_SUCCESS && tsx) {
        pjsip_tsx_send_msg(tsx, NULL);
        
        /* Force transaction termination to prevent leaks */
        if (tsx->state != PJSIP_TSX_STATE_TERMINATED)
            pjsip_tsx_terminate(tsx, 408);
    }
    
    /* Release transmit data buffer */
    pjsip_tx_data_dec_ref(tdata);
}

/* Test sip_util.c request creation functions */
static void test_create_request(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    if (size < 30)
        return;

    /* Extract URIs from fuzzer input */
    pj_str_t uri = {(char*)data, size > 100 ? 100 : (pj_ssize_t)size};
    pj_str_t from = {(char*)data + 10, size > 110 ? 50 : (pj_ssize_t)(size - 10)};
    pj_str_t to = {(char*)data + 20, size > 120 ? 50 : (pj_ssize_t)(size - 20)};

    pjsip_tx_data *tdata = NULL;
    pjsip_endpt_create_request(endpt, &pjsip_options_method,
                                &uri, &from, &to, NULL, NULL, -1, NULL, &tdata);
    if (tdata)
        pjsip_tx_data_dec_ref(tdata);
}

/* Test sip_util.c response creation functions */
static void test_create_response(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    /* Null-terminate input for parser */
    char *data_copy = (char*)pj_pool_alloc(pool, size + 1);
    pj_memcpy(data_copy, data, size);
    data_copy[size] = '\0';

    pjsip_parser_err_report err_list;
    pj_list_init(&err_list);

    pjsip_msg *msg = pjsip_parse_msg(pool, data_copy, size, &err_list);
    if (!msg || msg->type != PJSIP_REQUEST_MSG)
        return;

    /* Initialize rdata with parsed message and required fields */
    pjsip_rx_data rdata;
    pj_bzero(&rdata, sizeof(rdata));
    rdata.msg_info.msg = msg;
    rdata.tp_info.pool = pool;
    
    /* Populate message info fields from parsed message headers */
    rdata.msg_info.via = (pjsip_via_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL);
    rdata.msg_info.from = (pjsip_from_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL);
    rdata.msg_info.to = (pjsip_to_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);
    rdata.msg_info.cseq = (pjsip_cseq_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    
    /* Skip if required headers are missing */
    if (!rdata.msg_info.via || !rdata.msg_info.from || !rdata.msg_info.to || !rdata.msg_info.cseq)
        return;

    pjsip_tx_data *tdata = NULL;
    if (pjsip_endpt_create_response(endpt, &rdata, 200, NULL, &tdata) == PJ_SUCCESS && tdata)
        pjsip_tx_data_dec_ref(tdata);
}

/* Test ACK and CANCEL message generation */
static void test_create_ack_cancel(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    if (size < 30)
        return;

    pj_str_t uri = {(char*)data, size > 100 ? 100 : (pj_ssize_t)size};
    pj_str_t from = {(char*)data + 10, size > 110 ? 50 : (pj_ssize_t)(size - 10)};
    pj_str_t to = {(char*)data + 20, size > 120 ? 50 : (pj_ssize_t)(size - 20)};

    pjsip_tx_data *invite = NULL;
    if (pjsip_endpt_create_request(endpt, &pjsip_invite_method,
                                    &uri, &from, &to,
                                    NULL, NULL, -1, NULL, &invite) != PJ_SUCCESS || !invite)
        return;

    /* Create CANCEL request from INVITE */
    pjsip_tx_data *cancel_tdata = NULL;
    pjsip_endpt_create_cancel(endpt, invite, &cancel_tdata);
    if (cancel_tdata)
        pjsip_tx_data_dec_ref(cancel_tdata);

    pjsip_tx_data_dec_ref(invite);
}

/* Test route set processing in sip_util.c */
static void test_process_route_set(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    if (size < 30)
        return;

    /* Extract URIs from fuzzer input */
    pj_str_t uri = {(char*)data, size > 100 ? 100 : (pj_ssize_t)size};
    pj_str_t from = {(char*)data + 10, size > 110 ? 50 : (pj_ssize_t)(size - 10)};
    pj_str_t to = {(char*)data + 20, size > 120 ? 50 : (pj_ssize_t)(size - 20)};

    pjsip_tx_data *tdata = NULL;
    if (pjsip_endpt_create_request(endpt, &pjsip_options_method,
                                    &uri, &from, &to,
                                    NULL, NULL, -1, NULL, &tdata) != PJ_SUCCESS)
        return;

    if (!tdata)
        return;

    pjsip_host_info dest_info;
    pj_bzero(&dest_info, sizeof(dest_info));
    pjsip_process_route_set(tdata, &dest_info);

    pjsip_tx_data_dec_ref(tdata);
}

/* Main fuzzer entry point */
extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (Size < 0)
        return 1;

    /* One-time initialization of PJSIP stack */
    static int initialized = 0;
    if (!initialized) {
        pj_status_t status;
        pj_log_set_level(0);

        status = pj_init();
        if (status != PJ_SUCCESS)
            return 0;

        status = pjlib_util_init();
        if (status != PJ_SUCCESS)
            return 0;

        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy,
                            PJSIP_TEST_MEM_SIZE);

        status = pjsip_endpt_create(&caching_pool.factory, "fuzz", &endpt);
        if (status != PJ_SUCCESS || !endpt)
            return 0;

        status = pjsip_tsx_layer_init_module(endpt);
        if (status != PJ_SUCCESS)
            return 0;

        status = pjsip_loop_start(endpt, NULL);
        if (status != PJ_SUCCESS)
            return 0;

        tsx_user.name = pj_str("tsx-user");
        tsx_user.id = -1;
        tsx_user.priority = PJSIP_MOD_PRIORITY_APPLICATION;
        tsx_user.on_tsx_state = &on_tsx_state;

        status = pjsip_endpt_register_module(endpt, &tsx_user);
        if (status != PJ_SUCCESS)
            return 0;

        initialized = 1;
    }

    /* Create per-iteration memory pool for stateless fuzzing */
    pj_pool_t *pool = pjsip_endpt_create_pool(endpt, "fuzz", POOL_SIZE, POOL_SIZE);

    /* Run all tests with same input, processing pending events between tests for cleanup */
    pj_time_val timeout = {0, 0};
    
    test_uas_transaction(pool, Data, Size);
    pjsip_endpt_handle_events(endpt, &timeout);
    
    test_uac_transaction(pool, Data, Size);
    pjsip_endpt_handle_events(endpt, &timeout);
    
    test_create_request(pool, Data, Size);
    pjsip_endpt_handle_events(endpt, &timeout);
    
    test_create_response(pool, Data, Size);
    pjsip_endpt_handle_events(endpt, &timeout);
    
    test_create_ack_cancel(pool, Data, Size);
    pjsip_endpt_handle_events(endpt, &timeout);
    
    test_process_route_set(pool, Data, Size);
    pjsip_endpt_handle_events(endpt, &timeout);

    /* Release pool to ensure stateless operation */
    pjsip_endpt_release_pool(endpt, pool);

    return 0;
}

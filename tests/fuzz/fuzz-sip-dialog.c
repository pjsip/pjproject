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

pjsip_endpoint *endpt;
pj_caching_pool caching_pool;
pjsip_transport *loop_transport;

#define POOL_SIZE 8000
#define PJSIP_TEST_MEM_SIZE (2*1024*1024)

static pjsip_module dialog_test_mod;

static char* alloc_str_from_data(pj_pool_t *pool, const uint8_t *data,
                                  size_t offset, size_t len)
{
    char *buf = (char*)pj_pool_alloc(pool, len + 1);
    pj_memcpy(buf, data + offset, len);
    buf[len] = '\0';
    return buf;
}

static void test_uac_dialog(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    if (size < 3)
        return;

    /* Split input data into three URI strings */
    pj_ssize_t len1 = (size / 3 > 50) ? 50 : size / 3;
    pj_ssize_t len2 = (size / 3 > 50) ? 50 : size / 3;
    pj_ssize_t len3 = (size - len1 - len2 > 50) ? 50 : size - len1 - len2;

    pj_str_t local_uri = {alloc_str_from_data(pool, data, 0, len1), len1};
    pj_str_t remote_uri = {alloc_str_from_data(pool, data, len1, len2), len2};
    pj_str_t target = {alloc_str_from_data(pool, data, len1 + len2, len3), len3};

    /* Create UAC dialog */
    pjsip_dialog *dlg = NULL;
    pj_status_t status = pjsip_dlg_create_uac(pjsip_ua_instance(), &local_uri,
                                               NULL, &remote_uri, &target, &dlg);
    if (status != PJ_SUCCESS || !dlg)
        return;

    pjsip_dlg_inc_lock(dlg);

    /* Parse and set route header from fuzzer data */
    if (size > 150) {
        char *route_data = alloc_str_from_data(pool, data, 150, size - 150);
        pj_str_t route_name = pj_str("Route");
        pjsip_hdr *hdr = (pjsip_hdr*)pjsip_parse_hdr(pool, &route_name,
                                                      route_data, size - 150, NULL);
        if (hdr && hdr->type == PJSIP_H_ROUTE)
            pjsip_dlg_set_route_set(dlg, (pjsip_route_hdr*)hdr);
    }

    /* Create request within dialog */
    pjsip_tx_data *tdata = NULL;
    status = pjsip_dlg_create_request(dlg, &pjsip_options_method, -1, &tdata);
    if (status == PJ_SUCCESS && tdata)
        pjsip_tx_data_dec_ref(tdata);

    /* Test session management */
    pjsip_dlg_inc_session(dlg, &dialog_test_mod);
    pjsip_dlg_dec_session(dlg, &dialog_test_mod);

    pjsip_dlg_dec_lock(dlg);
}

static void test_uas_dialog(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    if (size < 1)
        return;

    /* Parse input data as SIP message */
    char *data_copy = alloc_str_from_data(pool, data, 0, size);
    pjsip_parser_err_report err_list;
    pj_list_init(&err_list);

    pjsip_msg *msg = pjsip_parse_msg(pool, data_copy, size, &err_list);
    if (!msg || msg->type != PJSIP_REQUEST_MSG)
        return;

    /* Setup received message structure */
    pjsip_rx_data rdata;
    pj_bzero(&rdata, sizeof(rdata));
    rdata.msg_info.msg = msg;
    rdata.tp_info.pool = pool;
    rdata.tp_info.transport = loop_transport;
    rdata.msg_info.via = (pjsip_via_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL);
    rdata.msg_info.from = (pjsip_from_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL);
    rdata.msg_info.to = (pjsip_to_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);
    rdata.msg_info.cseq = (pjsip_cseq_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL);
    rdata.msg_info.cid = (pjsip_cid_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CALL_ID, NULL);

    /* Validate required headers */
    if (!rdata.msg_info.via || !rdata.msg_info.from || !rdata.msg_info.to ||
        !rdata.msg_info.cseq || !rdata.msg_info.cid)
        return;

    /* Set recvd_param required by PJSIP transport layer */
    if (rdata.msg_info.via->recvd_param.slen == 0)
        rdata.msg_info.via->recvd_param = pj_str("127.0.0.1");

    /* Skip if method doesn't create a dialog */
    if (!pjsip_method_creates_dialog(&msg->line.req.method))
        return;

    /* Skip if To tag already exists */
    if (rdata.msg_info.to->tag.slen != 0)
        return;

    /* Create UAS dialog from incoming request */
    pjsip_dialog *dlg = NULL;
    pj_status_t status = pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(),
                                                            &rdata, NULL, &dlg);
    if (status != PJ_SUCCESS || !dlg)
        return;

    /* Create response within dialog */
    pjsip_tx_data *tdata = NULL;
    status = pjsip_dlg_create_response(dlg, &rdata, 200, NULL, &tdata);
    if (status == PJ_SUCCESS && tdata)
        pjsip_tx_data_dec_ref(tdata);

    /* Test session management */
    pjsip_dlg_inc_session(dlg, &dialog_test_mod);
    pjsip_dlg_dec_session(dlg, &dialog_test_mod);

    pjsip_dlg_dec_lock(dlg);
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    pj_status_t status;

    PJ_UNUSED_ARG(argc);
    PJ_UNUSED_ARG(argv);

    /* Initialize PJLIB */
    status = pj_init();
    if (status != PJ_SUCCESS)
        return 1;

    /* Initialize memory pool factory */
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy,
                         PJSIP_TEST_MEM_SIZE);

    /* Create SIP endpoint */
    status = pjsip_endpt_create(&caching_pool.factory, "dialog-fuzzer", &endpt);
    if (status != PJ_SUCCESS)
        return 1;

    /* Initialize transaction layer */
    status = pjsip_tsx_layer_init_module(endpt);
    if (status != PJ_SUCCESS)
        return 1;

    /* Initialize UA layer */
    status = pjsip_ua_init_module(endpt, NULL);
    if (status != PJ_SUCCESS)
        return 1;

    /* Initialize loop transport */
    status = pjsip_loop_start(endpt, &loop_transport);
    if (status != PJ_SUCCESS)
        return 1;

    /* Register dialog test module */
    pj_bzero(&dialog_test_mod, sizeof(dialog_test_mod));
    dialog_test_mod.name = pj_str("dialog-test-mod");
    dialog_test_mod.id = -1;
    dialog_test_mod.priority = PJSIP_MOD_PRIORITY_APPLICATION;

    status = pjsip_endpt_register_module(endpt, &dialog_test_mod);
    if (status != PJ_SUCCESS)
        return 1;

    /* Disable logging for performance */
    pj_log_set_level(0);

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    pj_pool_t *pool;

    /* Create pool for this iteration */
    pool = pjsip_endpt_create_pool(endpt, "dialog-fuzz", POOL_SIZE, POOL_SIZE);
    if (!pool)
        return 0;

    /* Test both UAC and UAS dialog creation */
    test_uac_dialog(pool, Data, Size);
    test_uas_dialog(pool, Data, Size);

    /* Release pool */
    pjsip_endpt_release_pool(endpt, pool);

    return 0;
}



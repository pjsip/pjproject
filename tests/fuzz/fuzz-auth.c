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
#include <pjsip/sip_auth.h>
#if defined(PJSIP_HAS_DIGEST_AKA_AUTH) && PJSIP_HAS_DIGEST_AKA_AUTH
#include <pjsip/sip_auth_aka.h>
#endif

#define POOL_SIZE 4000

/* Global resources (one-time initialization) */
static pj_caching_pool caching_pool;
static pjsip_endpoint *endpt;
static pjsip_auth_srv auth_srv;

/* Credential lookup for server authentication */
static pj_status_t lookup_cred(pj_pool_t *pool, const pj_str_t *realm,
                                const pj_str_t *acc_name, pjsip_cred_info *cred)
{
    pj_bzero(cred, sizeof(*cred));
    cred->realm = *realm;
    cred->username = *acc_name;
    cred->data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cred->data = pj_str("secret");
    return PJ_SUCCESS;
}

/* Helper: Parse SIP message from fuzzer input */
static pjsip_msg* parse_sip_message(pj_pool_t *pool, const uint8_t *data, 
                                     size_t size, int require_full_headers)
{
    char *msg_buf;
    pjsip_parser_err_report err_list;
    pjsip_msg *msg;

    if (size < 10)
        return NULL;

    /* Copy to null-terminated buffer */
    msg_buf = (char*)pj_pool_alloc(pool, size + 1);
    pj_memcpy(msg_buf, data, size);
    msg_buf[size] = '\0';

    /* Parse SIP message */
    pj_list_init(&err_list);
    msg = pjsip_parse_msg(pool, msg_buf, size, &err_list);

    if (!msg || msg->type != PJSIP_REQUEST_MSG || !msg->line.req.uri)
        return NULL;

    /* Check required headers for response creation */
    if (require_full_headers) {
        if (!pjsip_msg_find_hdr(msg, PJSIP_H_VIA, NULL) ||
            !pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL) ||
            !pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL) ||
            !pjsip_msg_find_hdr(msg, PJSIP_H_CALL_ID, NULL) ||
            !pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL))
            return NULL;
    }

    return msg;
}

/* Server authentication verification */
static void test_auth_server_verify(pj_pool_t *pool, pjsip_msg *msg)
{
    pjsip_rx_data rdata;
    pjsip_authorization_hdr *auth_hdr;

    /* Look for Authorization or Proxy-Authorization header */
    auth_hdr = (pjsip_authorization_hdr*)
               pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL);
    if (!auth_hdr) {
        auth_hdr = (pjsip_authorization_hdr*)
                   pjsip_msg_find_hdr(msg, PJSIP_H_PROXY_AUTHORIZATION, NULL);
    }
    if (!auth_hdr)
        return;

    /* Setup minimal rdata */
    pj_bzero(&rdata, sizeof(rdata));
    rdata.msg_info.msg = msg;
    rdata.tp_info.pool = pool;

    /* Test verification */
    pjsip_auth_srv_verify(&auth_srv, &rdata, NULL);
}

/* Server challenge generation */
static void test_auth_server_challenge(pj_pool_t *pool, pjsip_msg *msg)
{
    pjsip_rx_data rdata;
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Setup minimal rdata */
    pj_bzero(&rdata, sizeof(rdata));
    rdata.msg_info.msg = msg;
    rdata.tp_info.pool = pool;

    /* Create 401 response */
    status = pjsip_endpt_create_response(endpt, &rdata, 401, NULL, &tdata);
    if (status != PJ_SUCCESS || !tdata)
        return;

    /* Add authentication challenge */
    pjsip_auth_srv_challenge(&auth_srv, NULL, NULL, NULL, PJ_FALSE, tdata);
    pjsip_tx_data_dec_ref(tdata);
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    static int initialized = 0;
    pj_pool_t *pool;
    pjsip_msg *msg;
    pj_time_val timeout = {0, 0};

    /* === One-time initialization === */
    if (!initialized) {
        pj_status_t status;
        pj_pool_t *init_pool;
        pj_str_t realm;

        pj_init();
        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
        pj_log_set_level(0);

        /* Create SIP endpoint */
        status = pjsip_endpt_create(&caching_pool.factory, "fuzz", &endpt);
        if (status != PJ_SUCCESS)
            return 0;

        /* Initialize auth server */
        init_pool = pj_pool_create(&caching_pool.factory, "init", 1000, 1000, NULL);
        realm = pj_str("example.com");
        pjsip_auth_srv_init(init_pool, &auth_srv, &realm, &lookup_cred, 0);

        initialized = 1;
    }

    pool = pjsip_endpt_create_pool(endpt, "fuzz", POOL_SIZE, POOL_SIZE);
    if (!pool)
        return 0;

    /* Parse message and test auth server functions */
    msg = parse_sip_message(pool, Data, Size, 0);
    if (msg) {
        test_auth_server_verify(pool, msg);
    }

    msg = parse_sip_message(pool, Data, Size, 1);
    if (msg) {
        test_auth_server_challenge(pool, msg);
    }

    /* Cleanup */
    pjsip_endpt_release_pool(endpt, pool);
    pjsip_endpt_handle_events(endpt, &timeout);

    return 0;
}

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

/*
 * pjsua-level tests for the NAT Contact rewrite in acc_check_nat_addr().
 *
 * acc_check_nat_addr() rebuilds acc->contact from the Via "received"
 * param of the registrar's response.  That param is remote controlled
 * and is scanned with pjsip_VIA_PARAM_SPEC (TOKEN_SPEC plus "[:]"),
 * which is a wider character set than a SIP URI host accepts.  A value
 * that does not survive the round trip used to be stored anyway, and
 * the pjsip_parse_hdr() at the top of the *next* acc_check_nat_addr()
 * then returned NULL and was dereferenced.
 *
 * Each sub-test drives at least three REGISTER round trips, so that a
 * Contact stored on the first response is parsed back on a later one.
 *
 * Scenarios covered:
 *   1. IPv6 "received" over an IPv4 transport -> bracketed and stored.
 *   2. IPv4 "received"                        -> stored as-is.
 *   3. "received" with a token character that is not valid in a host
 *      -> rewrite declined, previous Contact retained.
 *   4. Colon bearing non-address token        -> rewrite declined.
 */

#include "test.h"
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "pjsua_acc_test.c"

/* SIP user name used to filter requests belonging to this test module */
#define TEST_USER   "pjsua-acc-test"

/* Expiration returned by the mock registrar */
#define TEST_EXPIRES 300


/*****************************************************************************
 * Mock registrar
 *
 * Answers every REGISTER with 200 OK, after overwriting the Via "received"
 * param of the request.  pjsip_endpt_create_response() clones the request's
 * Via into the response, so this is what the account sees coming back and
 * what acc_check_nat_addr() rewrites the Contact from.
 *****************************************************************************/

static pj_bool_t mock_registrar_rx_request(pjsip_rx_data *rdata);

static struct {
    pjsip_module  mod;
    const char   *received;     /* forced Via received param, or NULL */
    unsigned      req_count;
} g_mock_reg = {
    {
        NULL, NULL,
        { "mod-pjsua-acc-reg", 17 },
        -1,
        PJSIP_MOD_PRIORITY_APPLICATION,
        NULL, NULL, NULL, NULL,
        &mock_registrar_rx_request,
        NULL, NULL, NULL, NULL,
    }
};

static pj_bool_t mock_registrar_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_hdr  hdr_list;

    if (msg->line.req.method.id != PJSIP_REGISTER_METHOD ||
        !is_user_equal(rdata->msg_info.from, TEST_USER))
    {
        return PJ_FALSE;
    }

    g_mock_reg.req_count++;

    if (g_mock_reg.received) {
        pj_strdup2(rdata->tp_info.pool, &rdata->msg_info.via->recvd_param,
                   g_mock_reg.received);
    }

    pj_list_init(&hdr_list);
    pj_list_push_back(&hdr_list,
                      pjsip_expires_hdr_create(rdata->tp_info.pool,
                                               TEST_EXPIRES));

    pjsip_endpt_respond_stateless(pjsua_get_pjsip_endpt(), rdata,
                                  200, NULL, &hdr_list, NULL);
    return PJ_TRUE;
}


/*****************************************************************************
 * Test context
 *****************************************************************************/

static struct {
    pj_bool_t   reg_done;
    int         reg_code;
    unsigned    reg_ok_count;
} g_ctx;

static void on_reg_state2(pjsua_acc_id acc_id, pjsua_reg_info *info)
{
    PJ_UNUSED_ARG(acc_id);

    g_ctx.reg_code = info->cbparam->code;
    if (info->cbparam->code / 100 == 2) {
        g_ctx.reg_ok_count++;
        g_ctx.reg_done = PJ_TRUE;
    }
}


/*****************************************************************************
 * Helpers
 *****************************************************************************/

/* Parse the account's current Contact and copy the URI host out of it.
 * Returns 0 on success, -1 if the Contact is not a parseable SIP Contact.
 * This is exactly the operation acc_check_nat_addr() performs on entry,
 * so it detects a Contact that would have crashed the next check.
 */
static int get_acc_contact_host(pjsua_acc_id acc_id, char *host,
                                unsigned host_size)
{
    const pj_str_t STR_CONTACT = { "Contact", 7 };
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pjsip_contact_hdr *hdr;
    pjsip_sip_uri *uri;
    pj_pool_t *pool;
    int rc = -1;

    pool = pjsua_pool_create("acctest", 512, 512);
    hdr = (pjsip_contact_hdr*)
          pjsip_parse_hdr(pool, &STR_CONTACT, acc->contact.ptr,
                          acc->contact.slen, NULL);
    if (hdr && hdr->uri &&
        (PJSIP_URI_SCHEME_IS_SIP(hdr->uri) ||
         PJSIP_URI_SCHEME_IS_SIPS(hdr->uri)))
    {
        uri = (pjsip_sip_uri*) pjsip_uri_get_uri(hdr->uri);
        if (uri->host.slen < (pj_ssize_t)host_size) {
            pj_ansi_snprintf(host, host_size, "%.*s",
                             (int)uri->host.slen, uri->host.ptr);
            rc = 0;
        }
    }

    pj_pool_release(pool);
    return rc;
}

/* Pump the event loop until the pending registration completes. */
static int wait_reg(unsigned max_ms)
{
    unsigned elapsed = 0;

    while (elapsed < max_ms && !g_ctx.reg_done) {
        pjsua_handle_events(50);
        elapsed += 50;
    }
    return g_ctx.reg_done ? 0 : -1;
}


/*****************************************************************************
 * Sub-test driver
 *
 * 'received'    : value forced into the response's Via received param.
 * 'expect_host' : Contact host expected afterwards, or NULL if the rewrite
 *                 must be declined and the original Contact retained.
 *****************************************************************************/
static int rewrite_test(pjsua_transport_id tp_id, int port,
                        const char *title, const char *received,
                        const char *expect_host, int err_base)
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id;
    char             reg_uri[64];
    char             orig_contact[PJSIP_MAX_URL_SIZE];
    char             host[PJ_INET6_ADDRSTRLEN + 16];
    pj_status_t      status;
    unsigned         round;
    int              rc = 0;

    PJ_LOG(3, (THIS_FILE, "  pjsua acc: %s", title));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_mock_reg.received  = received;
    g_mock_reg.req_count = 0;

    pj_ansi_snprintf(reg_uri, sizeof(reg_uri), "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.transport_id = tp_id;
    acc_cfg.register_on_acc_add = PJ_FALSE;
    /* 2 = always rewrite, which skips the private/public address
     * heuristics and keeps the test independent of them.
     */
    acc_cfg.allow_contact_rewrite = 2;
    /* Keep the Via sent-by out of it; this test is about the Contact. */
    acc_cfg.allow_via_rewrite = PJ_FALSE;

    status = pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: pjsua_acc_add failed (%d)", status));
        return err_base - 1;
    }

    /* First registration. The Contact is built here, snapshot it before
     * any response can arrive (no worker threads in this test).
     */
    g_ctx.reg_done = PJ_FALSE;
    status = pjsua_acc_set_registration(acc_id, PJ_TRUE);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: set_registration failed (%d)",
                   status));
        rc = err_base - 2;
        goto on_return;
    }
    pj_ansi_snprintf(orig_contact, sizeof(orig_contact), "%.*s",
                     (int)pjsua_var.acc[acc_id].contact.slen,
                     pjsua_var.acc[acc_id].contact.ptr);

    /* Three round trips: the first response stores the rewritten Contact,
     * a later one parses it back. That second parse is what used to
     * dereference NULL.
     */
    for (round = 0; round < 3; ++round) {
        if (round > 0) {
            g_ctx.reg_done = PJ_FALSE;
            /* A rewrite triggers its own re-registration; if one is
             * already in flight this returns PJSIP_EBUSY and we simply
             * wait for it instead.
             */
            pjsua_acc_set_registration(acc_id, PJ_TRUE);
        }
        if (wait_reg(10000) != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: registration %u timed out, "
                       "last code %d", round, g_ctx.reg_code));
            rc = err_base - 3;
            goto on_return;
        }
    }

    if (g_mock_reg.req_count < 3) {
        PJ_LOG(3, (THIS_FILE, "    error: expected >=3 REGISTERs, got %u",
                   g_mock_reg.req_count));
        rc = err_base - 4;
        goto on_return;
    }

    /* The stored Contact must always be parseable, whether or not the
     * rewrite was accepted.
     */
    if (get_acc_contact_host(acc_id, host, sizeof(host)) != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: account Contact is unparseable: "
                   "'%.*s'", (int)pjsua_var.acc[acc_id].contact.slen,
                   pjsua_var.acc[acc_id].contact.ptr));
        rc = err_base - 5;
        goto on_return;
    }

    if (expect_host) {
        if (pj_ansi_strcmp(host, expect_host) != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: expected Contact host '%s', "
                       "got '%s'", expect_host, host));
            rc = err_base - 6;
            goto on_return;
        }
    } else {
        /* Rewrite must have been declined, leaving the original intact */
        if (pj_strcmp2(&pjsua_var.acc[acc_id].contact, orig_contact) != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: Contact should have been kept "
                       "as '%s', got '%.*s'", orig_contact,
                       (int)pjsua_var.acc[acc_id].contact.slen,
                       pjsua_var.acc[acc_id].contact.ptr));
            rc = err_base - 7;
            goto on_return;
        }
    }

on_return:
    g_mock_reg.received = NULL;
    pjsua_acc_del(acc_id);
    pjsua_handle_events(500);
    return rc;
}


/*****************************************************************************
 * Main entry point
 *****************************************************************************/

/* Recreate the test framework's endpoint + tsx layer after pjsua_destroy. */
static void restore_endpt(void)
{
    pj_status_t status;

    status = pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error creating endpoint"));
        return;
    }
    status = pjsip_tsx_layer_init_module(endpt);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error initializing tsx layer"));
    }
}

int pjsua_acc_test(void)
{
    pjsua_config           ua_cfg;
    pjsua_logging_config   log_cfg;
    pjsua_transport_config tp_cfg;
    pjsua_transport_id     tp_id;
    pj_uint16_t            port;
    pj_status_t            status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "pjsua account Contact rewrite test"));

    /* The pjsip test framework's global endpoint owns the tsx layer
     * singleton, which pjsua would try to register on its own endpoint.
     * Destroy it here and recreate it via restore_endpt() afterwards.
     */
    pjsip_endpt_destroy(endpt);
    endpt = NULL;

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_create failed (%d)", status));
        restore_endpt();
        return -2401;
    }

    pjsua_config_default(&ua_cfg);
    ua_cfg.cb.on_reg_state2 = &on_reg_state2;
    /* No worker threads: everything runs from pjsua_handle_events() so
     * the Contact can be inspected at known points.
     */
    ua_cfg.thread_cnt = 0;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.level         = 3;
    log_cfg.console_level = 3;

    status = pjsua_init(&ua_cfg, &log_cfg, NULL);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_init failed (%d)", status));
        pjsua_destroy();
        restore_endpt();
        return -2402;
    }

    pjsua_transport_config_default(&tp_cfg);
    tp_cfg.port = 0;  /* ephemeral */

    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tp_cfg, &tp_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_transport_create failed (%d)", status));
        pjsua_destroy();
        restore_endpt();
        return -2403;
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_start failed (%d)", status));
        pjsua_destroy();
        restore_endpt();
        return -2404;
    }

    {
        pjsua_transport_info ti;
        pjsua_transport_get_info(tp_id, &ti);
        port = pj_sockaddr_get_port(&ti.local_addr);
    }

    status = pjsip_endpt_register_module(pjsua_get_pjsip_endpt(),
                                         &g_mock_reg.mod);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  register mock registrar failed (%d)",
                   status));
        pjsua_destroy();
        restore_endpt();
        return -2405;
    }

    /* ---- Run sub-tests ---- */

    /* An IPv6 address seen by the registrar on a transport that is not
     * typed IPv6 (dual stack SBC, NAT64). Must be bracketed, otherwise
     * the stored Contact is <sip:user@2001:db8::1:PORT> and unparseable.
     */
    rc = rewrite_test(tp_id, (int)port, "IPv6 received over IPv4 transport",
                      "2001:db8::1", "2001:db8::1", -2410);
    if (rc != 0) goto on_return;

    /* Plain IPv4: the ordinary rewrite must still happen. */
    rc = rewrite_test(tp_id, (int)port, "IPv4 received",
                      "1.2.3.4", "1.2.3.4", -2420);
    if (rc != 0) goto on_return;

    /* '!' is accepted by the Via param scanner but not by the URI host
     * scanner, so the rebuilt Contact would not parse back.
     */
    rc = rewrite_test(tp_id, (int)port, "received with invalid host char",
                      "1.2.3.4!", NULL, -2430);
    if (rc != 0) goto on_return;

    /* Colon bearing token that is not an IPv6 address: must not be
     * bracketed into a Contact that parses but means nothing.
     */
    rc = rewrite_test(tp_id, (int)port, "received with non-address colon",
                      "foo:bar", NULL, -2440);
    if (rc != 0) goto on_return;

on_return:
    if (g_mock_reg.mod.id != -1)
        pjsip_endpt_unregister_module(pjsua_get_pjsip_endpt(),
                                      &g_mock_reg.mod);

    pjsua_handle_events(500);
    pjsua_destroy2(PJSUA_DESTROY_NO_RX_MSG);

    restore_endpt();

    return rc;
}

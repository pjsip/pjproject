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
 *   5. As 3, with PJSUA_CONTACT_REWRITE_UNREGISTER: the declined rewrite
 *      must not unregister and tear down the regc on the way.
 *   6. Accepted rewrite with reg_contact_uri_params, which sends
 *      update_regc_contact() through its parse-and-regenerate path.
 *   7. force_contact "*", which parses to a Contact with no URI.
 *   8. force_contact with a non-SIP URI, with reg_contact_uri_params set
 *      so both the acc_check_nat_addr() entry guard and the
 *      update_regc_contact() regeneration path see a non-SIP URI.
 *   9. A Contact that pjsip_regc refuses must leave the existing
 *      registration binding alone rather than queue it for removal.
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
    char          last_contact[PJSIP_MAX_URL_SIZE];  /* as seen on the wire */
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

    /* Record the whole Contact header as it arrived, so a test can tell
     * whether a rejected pjsip_regc_update_contact() disturbed the
     * binding. The header must be captured in full, not just the URI: a
     * binding queued for removal is re-sent with the same URI and only
     * ";expires=0" to distinguish it.
     */
    {
        pjsip_contact_hdr *hc = (pjsip_contact_hdr*)
                                pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT,
                                                   NULL);
        g_mock_reg.last_contact[0] = '\0';
        if (hc) {
            int len = pjsip_hdr_print_on(hc, g_mock_reg.last_contact,
                                         sizeof(g_mock_reg.last_contact)-1);
            g_mock_reg.last_contact[len > 0? len : 0] = '\0';
        }
    }

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

/* Check the registration Contact that update_regc_contact() produced:
 * it must be NULL terminated (the scanner requires it) and parse back as
 * a Contact header. Returns 0 on success.
 */
static int check_acc_reg_contact(pjsua_acc_id acc_id, pj_bool_t require_uri)
{
    const pj_str_t STR_CONTACT = { "Contact", 7 };
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pjsip_contact_hdr *hdr;
    pj_pool_t *pool;
    int rc = -1;

    if (acc->reg_contact.slen == 0 ||
        acc->reg_contact.ptr[acc->reg_contact.slen] != '\0')
    {
        return -1;
    }

    pool = pjsua_pool_create("acctest", 512, 512);
    hdr = (pjsip_contact_hdr*)
          pjsip_parse_hdr(pool, &STR_CONTACT, acc->reg_contact.ptr,
                          acc->reg_contact.slen, NULL);
    if (hdr && (hdr->uri || !require_uri))
        rc = 0;

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
 *****************************************************************************/

typedef struct {
    const char *title;
    const char *received;       /* forced Via received param */
    const char *expect_host;    /* Contact host expected afterwards, or
                                 * NULL if the rewrite must be declined
                                 * and the original Contact retained */
    const char *force_contact;  /* acc_cfg.force_contact, or NULL */
    const char *uri_params;     /* reg_contact_uri_params, or NULL */
    int         rewrite_method; /* 0 = leave at the default */
    int         err_base;
    pj_bool_t   expect_reg_fail;/* Contact cannot be registered at all */
    pj_bool_t   probe_bad_update;/* poke regc with an invalid Contact */
} rewrite_case;

static int rewrite_test(pjsua_transport_id tp_id, int port,
                        const rewrite_case *c)
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id;
    char             reg_uri[64];
    char             orig_contact[PJSIP_MAX_URL_SIZE];
    char             host[PJ_INET6_ADDRSTRLEN + 16];
    const int        err_base = c->err_base;
    pj_status_t      status;
    unsigned         round;
    int              rc = 0;

    PJ_LOG(3, (THIS_FILE, "  pjsua acc: %s", c->title));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_mock_reg.received  = c->received;
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
    if (c->rewrite_method)
        acc_cfg.contact_rewrite_method = c->rewrite_method;
    if (c->force_contact)
        acc_cfg.force_contact = pj_str((char*)c->force_contact);
    if (c->uri_params)
        acc_cfg.reg_contact_uri_params = pj_str((char*)c->uri_params);

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
    if (c->expect_reg_fail) {
        /* The Contact cannot be registered at all. What matters is that
         * building it did not dereference a NULL URI on the way, and that
         * it is refused rather than half-registered.
         */
        if (status == PJ_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "    error: registration unexpectedly "
                       "accepted Contact '%s'", c->force_contact));
            rc = err_base - 10;
        }
        goto on_return;
    }
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

    if (c->probe_bad_update) {
        pj_str_t star = pj_str("*");
        char contact_before[PJSIP_MAX_URL_SIZE];

        pj_ansi_snprintf(contact_before, sizeof(contact_before), "%s",
                         g_mock_reg.last_contact);

        /* An invalid Contact must be refused without disturbing the
         * registration already in place. set_contact() moves the current
         * Contacts to the removed list with expires=0 before it can fail,
         * so validating only partway would silently unregister them on
         * the next REGISTER.
         */
        status = pjsip_regc_update_contact(pjsua_var.acc[acc_id].regc,
                                           1, &star);
        if (status != PJSIP_EINVALIDURI) {
            PJ_LOG(3, (THIS_FILE, "    error: update_contact with '*' "
                       "returned %d, expected PJSIP_EINVALIDURI", status));
            rc = err_base - 11;
            goto on_return;
        }

        g_ctx.reg_done = PJ_FALSE;
        pjsua_acc_set_registration(acc_id, PJ_TRUE);
        if (wait_reg(10000) != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: registration after a rejected "
                       "update timed out, last code %d", g_ctx.reg_code));
            rc = err_base - 12;
            goto on_return;
        }
        if (pj_ansi_strcmp(g_mock_reg.last_contact, contact_before) != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: rejected update changed the "
                       "registered Contact: '%s' -> '%s'",
                       contact_before, g_mock_reg.last_contact));
            rc = err_base - 13;
            goto on_return;
        }
    }

    /* A Contact we generated or rewrote must always parse back, which is
     * the operation acc_check_nat_addr() performs on entry. A
     * force_contact case deliberately starts from something that does
     * not (e.g. "*"), and only has to survive being checked.
     */
    if (!c->force_contact) {
        if (get_acc_contact_host(acc_id, host, sizeof(host)) != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: account Contact is "
                       "unparseable: '%.*s'",
                       (int)pjsua_var.acc[acc_id].contact.slen,
                       pjsua_var.acc[acc_id].contact.ptr));
            rc = err_base - 5;
            goto on_return;
        }
    }

    if (c->expect_host) {
        if (pj_ansi_strcmp(host, c->expect_host) != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: expected Contact host '%s', "
                       "got '%s'", c->expect_host, host));
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

        /* A declined rewrite must not have unregistered and torn down
         * the client registration on the way, which is why the
         * PJSUA_CONTACT_REWRITE_UNREGISTER step runs after validation.
         */
        if (pjsua_var.acc[acc_id].regc == NULL) {
            PJ_LOG(3, (THIS_FILE, "    error: regc was destroyed by a "
                       "declined rewrite"));
            rc = err_base - 8;
            goto on_return;
        }
    }

    /* The registration Contact derived from it must be well formed too.
     * "Contact: *" has no URI by definition, so only require one when the
     * account Contact was not forced to something exotic.
     */
    if (check_acc_reg_contact(acc_id, (pj_bool_t)(c->force_contact == NULL))
        != 0)
    {
        PJ_LOG(3, (THIS_FILE, "    error: registration Contact is not NULL "
                   "terminated or does not parse: '%.*s'",
                   (int)pjsua_var.acc[acc_id].reg_contact.slen,
                   pjsua_var.acc[acc_id].reg_contact.ptr));
        rc = err_base - 9;
        goto on_return;
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
    {
        static const rewrite_case cases[] = {
            /* An IPv6 address seen by the registrar on a transport that
             * is not typed IPv6 (dual stack SBC, NAT64). Must be
             * bracketed, otherwise the stored Contact is
             * <sip:user@2001:db8::1:PORT> and no longer parses.
             */
            { "IPv6 received over IPv4 transport",
              "2001:db8::1", "2001:db8::1", NULL, NULL, 0, -2410 },

            /* Plain IPv4: the ordinary rewrite must still happen. */
            { "IPv4 received",
              "1.2.3.4", "1.2.3.4", NULL, NULL, 0, -2420 },

            /* '!' is accepted by the Via param scanner but not by the URI
             * host scanner, so the rebuilt Contact would not parse back.
             */
            { "received with invalid host char",
              "1.2.3.4!", NULL, NULL, NULL, 0, -2430 },

            /* Colon bearing token that is not an IPv6 address: must not
             * be bracketed into a Contact that parses but means nothing.
             */
            { "received with non-address colon",
              "foo:bar", NULL, NULL, NULL, 0, -2440 },

            /* Same rejection, but with PJSUA_CONTACT_REWRITE_UNREGISTER:
             * the declined rewrite must leave the registration alone.
             */
            { "declined rewrite with UNREGISTER method",
              "1.2.3.4!", NULL, NULL, NULL,
              PJSUA_CONTACT_REWRITE_UNREGISTER, -2450 },

            /* reg_contact_uri_params sends update_regc_contact() through
             * its parse-and-regenerate path on the rewritten Contact.
             */
            { "accepted rewrite with reg_contact_uri_params",
              "1.2.3.4", "1.2.3.4", NULL, ";acc-test=1", 0, -2460 },

            /* "Contact: *" parses to a header with a NULL uri. Building
             * the registration Contact from it must fall back instead of
             * dereferencing it, and pjsip_regc must then refuse it rather
             * than register a Contact it will later dereference.
             */
            { "force_contact star",
              "1.2.3.4", NULL, "*", ";acc-test=1", 0, -2470, PJ_TRUE },

            /* A non-SIP URI parses fine but must not be cast to
             * pjsip_sip_uri* and read as one.
             */
            { "force_contact non-SIP URI",
              "1.2.3.4", NULL, "<tel:+15551234>", ";acc-test=1", 0, -2480 },

            /* A Contact that pjsip_regc refuses must leave the existing
             * registration binding untouched, not queued for removal.
             */
            { "rejected update_contact keeps the binding",
              "1.2.3.4", "1.2.3.4", NULL, NULL, 0, -2490,
              PJ_FALSE, PJ_TRUE },
        };
        unsigned i;

        for (i=0; i<PJ_ARRAY_SIZE(cases); ++i) {
            rc = rewrite_test(tp_id, (int)port, &cases[i]);
            if (rc != 0) goto on_return;
        }
    }

on_return:
    if (g_mock_reg.mod.id != -1)
        pjsip_endpt_unregister_module(pjsua_get_pjsip_endpt(),
                                      &g_mock_reg.mod);

    pjsua_handle_events(500);
    pjsua_destroy2(PJSUA_DESTROY_NO_RX_MSG);

    restore_endpt();

    return rc;
}

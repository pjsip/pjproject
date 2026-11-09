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
 *  10. The REGISTER sent by an accepted rewrite fails inside the send:
 *      the nested failure report must leave contact_rewritten clear, so
 *      that the retry's 2xx rewrites the Contact again.
 *  11. PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE alone, which names no 2xx
 *      method: the rewrite must still reach the registrar.
 *  12. UNREGISTER and NO_UNREG together, likewise.
 *  13. pjsua must not register an account that the application
 *      unregistered: not on IP change, by an auto retry or by a NAT
 *      Contact rewrite, also when the unregistration failed or went out
 *      while another REGISTER was completing. A call refused with
 *      PJSIP_EBUSY changes nothing, presence stays published, a later
 *      registration undoes it.
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
    /* host of the first Contact not sent with expires=0 */
    char          live_host[PJ_INET6_ADDRSTRLEN + 16];
    unsigned      reg_count;    /* requests that add a binding */
    int           code;         /* response code, or 0 for 200 */
    pj_bool_t     code_once;    /* code only for the next request */
    unsigned      code_skip;    /* requests answered 200 before code */
    pj_bool_t     drop;         /* don't answer */
    pj_bool_t     dropped;      /* a request was not answered */
    pj_bool_t     challenge;    /* 401 to a request without credentials */
} g_mock_reg = {
    {
        NULL, NULL,
        { "mod-pjsua-acc-reg", 17 },
        -1,
        /* Before pjsua's module, which takes PJSUA_LOCK */
        PJSIP_MOD_PRIORITY_APPLICATION - 1,
        NULL, NULL, NULL, NULL,
        &mock_registrar_rx_request,
        NULL, NULL, NULL, NULL,
    }
};

static pj_bool_t mock_registrar_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_hdr  hdr_list;
    int        code;

    if (!is_user_equal(rdata->msg_info.from, TEST_USER))
        return PJ_FALSE;

    /* Accept a PUBLISH here, before pjsua's module would take it */
    if (pjsip_method_cmp(&msg->line.req.method, &pjsip_publish_method)==0) {
        pjsip_endpt_respond_stateless(pjsua_get_pjsip_endpt(), rdata, 200,
                                      NULL, NULL, NULL);
        return PJ_TRUE;
    }
    if (msg->line.req.method.id != PJSIP_REGISTER_METHOD)
        return PJ_FALSE;

    g_mock_reg.req_count++;
    if (g_mock_reg.drop) {
        g_mock_reg.dropped = PJ_TRUE;
        return PJ_TRUE;
    }

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

    /* And the host of the first Contact it registers rather than removes */
    {
        pjsip_contact_hdr *hc = NULL;

        g_mock_reg.live_host[0] = '\0';
        while ((hc = (pjsip_contact_hdr*)
                     pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT,
                                        hc ? hc->next : NULL)) != NULL)
        {
            pjsip_sip_uri *uri;

            if (hc->expires == 0 || hc->uri == NULL ||
                !(PJSIP_URI_SCHEME_IS_SIP(hc->uri) ||
                  PJSIP_URI_SCHEME_IS_SIPS(hc->uri)))
            {
                continue;
            }
            uri = (pjsip_sip_uri*) pjsip_uri_get_uri(hc->uri);
            pj_ansi_snprintf(g_mock_reg.live_host,
                             sizeof(g_mock_reg.live_host), "%.*s",
                             (int)uri->host.slen, uri->host.ptr);
            break;
        }
    }

    {
        pjsip_expires_hdr *he = (pjsip_expires_hdr*)
                                pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES,
                                                   NULL);
        if (g_mock_reg.live_host[0] && (!he || he->ivalue))
            g_mock_reg.reg_count++;
    }

    if (g_mock_reg.received) {
        pj_strdup2(rdata->tp_info.pool, &rdata->msg_info.via->recvd_param,
                   g_mock_reg.received);
    }

    pj_list_init(&hdr_list);
    if (g_mock_reg.challenge &&
        !pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL))
    {
        const pj_str_t hname = pj_str("WWW-Authenticate");
        const pj_str_t hvalue = pj_str("Digest realm=\"test\", "
                                       "nonce=\"testnonce\"");

        pj_list_push_back(&hdr_list,
                          pjsip_generic_string_hdr_create(
                              rdata->tp_info.pool, &hname, &hvalue));
        pjsip_endpt_respond_stateless(pjsua_get_pjsip_endpt(), rdata, 401,
                                      NULL, &hdr_list, NULL);
        return PJ_TRUE;
    }
    pj_list_push_back(&hdr_list,
                      pjsip_expires_hdr_create(rdata->tp_info.pool,
                                               TEST_EXPIRES));

    if (g_mock_reg.code_skip) {
        g_mock_reg.code_skip--;
        code = 200;
    } else {
        code = g_mock_reg.code ? g_mock_reg.code : 200;
        if (g_mock_reg.code_once)
            g_mock_reg.code = 0;
    }
    pjsip_endpt_respond_stateless(pjsua_get_pjsip_endpt(), rdata, code,
                                  NULL, &hdr_list, NULL);
    return PJ_TRUE;
}


/*****************************************************************************
 * Send failure
 *
 * Fails the next REGISTER whose Contact has the given host before it is
 * sent, as an unreachable network would, so that its transaction ends
 * inside the send.
 *****************************************************************************/

static pj_status_t fail_tx_on_tx_request(pjsip_tx_data *tdata);

static struct {
    pjsip_module  mod;
    const char   *host;         /* Contact host to fail, or NULL */
    unsigned      skip;         /* matching requests to send first */
} g_fail_tx = {
    {
        NULL, NULL,
        { "mod-pjsua-acc-fail-tx", 21 },
        -1,
        PJSIP_MOD_PRIORITY_APPLICATION,
        NULL, NULL, NULL, NULL,
        NULL, NULL,
        &fail_tx_on_tx_request,
        NULL, NULL,
    }
};

static pj_status_t fail_tx_on_tx_request(pjsip_tx_data *tdata)
{
    pjsip_contact_hdr *hc;

    if (!g_fail_tx.host ||
        tdata->msg->line.req.method.id != PJSIP_REGISTER_METHOD)
    {
        return PJ_SUCCESS;
    }

    hc = (pjsip_contact_hdr*) pjsip_msg_find_hdr(tdata->msg,
                                                 PJSIP_H_CONTACT, NULL);
    while (hc) {
        if (hc->uri && PJSIP_URI_SCHEME_IS_SIP(hc->uri)) {
            pjsip_sip_uri *uri = (pjsip_sip_uri*)
                                 pjsip_uri_get_uri(hc->uri);

            if (pj_strcmp2(&uri->host, g_fail_tx.host) == 0) {
                if (g_fail_tx.skip) {
                    g_fail_tx.skip--;
                    return PJ_SUCCESS;
                }
                g_fail_tx.host = NULL;
                return PJ_ECANCELLED;
            }
        }
        hc = (pjsip_contact_hdr*) pjsip_msg_find_hdr(tdata->msg,
                                                     PJSIP_H_CONTACT,
                                                     hc->next);
    }

    return PJ_SUCCESS;
}


/*****************************************************************************
 * Test context
 *****************************************************************************/

static struct {
    pj_bool_t   reg_done;
    int         reg_code;
    unsigned    reg_ok_count;
    pj_int32_t  reg_ok_cseq;    /* CSeq of the last 2xx reported */
    pj_bool_t   unreg_done;     /* an unregistration was answered */
    int         ipc_update_cnt; /* ACC_UPDATE_CONTACT reports */
    int         ipc_done_cnt;   /* COMPLETED reports */
    pj_bool_t   published;      /* still published after a refused unreg */
    pj_bool_t   unpublished;    /* unpublished by a successful unreg */
} g_ctx;

static void on_reg_state2(pjsua_acc_id acc_id, pjsua_reg_info *info)
{
    PJ_UNUSED_ARG(acc_id);

    if (!info->renew)
        g_ctx.unreg_done = PJ_TRUE;
    g_ctx.reg_code = info->cbparam->code;
    if (info->cbparam->code / 100 == 2) {
        g_ctx.reg_ok_count++;
        g_ctx.reg_ok_cseq = info->cbparam->cseq;
        g_ctx.reg_done = PJ_TRUE;
    }
}


static void on_ip_change_progress(pjsua_ip_change_op op, pj_status_t status,
                                  const pjsua_ip_change_op_info *info)
{
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(info);

    if (op == PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT)
        g_ctx.ipc_update_cnt++;
    else if (op == PJSUA_IP_CHANGE_OP_COMPLETED)
        g_ctx.ipc_done_cnt++;
}


/* A challenge the application answers later */
static struct {
    pj_bool_t             enabled;
    int                   pending;
    pjsip_auth_clt_sess  *sess;
    void                 *token;
} g_defer;

static void on_auth_challenge(pjsua_on_auth_challenge_param *param)
{
    if (!g_defer.enabled || g_defer.pending)
        return;
    g_defer.sess = param->auth_sess;
    g_defer.token = param->token;
    g_defer.pending = PJ_TRUE;
    param->handled = PJ_TRUE;
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

/* Pump the event loop for the whole period, or until *flag is set.
 * Without timers, only the network.
 */
static void pump_events(unsigned ms, const int *flag, pj_bool_t timers)
{
    pj_ioqueue_t *ioq = pjsip_endpt_get_ioqueue(pjsua_get_pjsip_endpt());
    pj_time_val end, now, timeout = {0, 10};

    pj_gettickcount(&end);
    end.msec += ms;
    pj_time_val_normalize(&end);
    do {
        if (timers)
            pjsua_handle_events(10);
        else
            pj_ioqueue_poll(ioq, &timeout);
        pj_gettickcount(&now);
    } while ((!flag || !*flag) && PJ_TIME_VAL_LT(now, end));
}

/* Pump the event loop until the pending registration completes. */
static int wait_reg(unsigned max_ms)
{
    pump_events(max_ms, &g_ctx.reg_done, PJ_TRUE);
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
    char             rewrite_host[PJ_INET6_ADDRSTRLEN + 16];
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
        /* The first success answers the REGISTER a rewrite sends */
        if (round == 0) {
            pj_ansi_strxcpy(rewrite_host, g_mock_reg.live_host,
                            sizeof(rewrite_host));
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

        /* The regc must register it too, not only the account hold it:
         * in the REGISTER the rewrite sent, and still in the last one.
         */
        if (pj_ansi_strcmp(rewrite_host, c->expect_host) != 0 ||
            pj_ansi_strcmp(g_mock_reg.live_host, c->expect_host) != 0)
        {
            PJ_LOG(3, (THIS_FILE, "    error: registrar received host '%s' "
                       "from the rewrite and '%s' last, expected '%s'",
                       rewrite_host, g_mock_reg.live_host, c->expect_host));
            rc = err_base - 14;
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


/* The REGISTER that acc_check_nat_addr() sends fails inside the send, so
 * regc_cb() runs again, nested, for the failure. contact_rewritten must
 * come out of it clear, or the retry's 2xx skips the NAT check and the
 * account stays registered with the Contact the registrar did not see.
 */
static int rewrite_send_fail_test(pjsua_transport_id tp_id, int port)
{
    const char      *public_host = "1.2.3.4";
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id;
    char             reg_uri[64];
    char             host[PJ_INET6_ADDRSTRLEN + 16];
    unsigned         elapsed;
    pj_status_t      status;
    int              rc = 0;

    PJ_LOG(3, (THIS_FILE, "  pjsua acc: rewrite whose REGISTER fails at "
               "once"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    g_mock_reg.received  = public_host;
    g_fail_tx.host       = public_host;

    pj_ansi_snprintf(reg_uri, sizeof(reg_uri), "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.transport_id = tp_id;
    acc_cfg.register_on_acc_add = PJ_FALSE;
    acc_cfg.allow_contact_rewrite = 2;
    acc_cfg.allow_via_rewrite = PJ_FALSE;
    /* Retry the failed REGISTER after exactly one second */
    acc_cfg.reg_first_retry_interval = 1;
    acc_cfg.reg_retry_interval = 1;
    acc_cfg.reg_retry_random_interval = 0;

    status = pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: pjsua_acc_add failed (%d)", status));
        g_mock_reg.received = NULL;
        g_fail_tx.host = NULL;
        return -2540;
    }

    status = pjsua_acc_set_registration(acc_id, PJ_TRUE);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    error: set_registration failed (%d)",
                   status));
        rc = -2541;
        goto on_return;
    }

    /* The 2xx that triggers the rewrite is not reported, so the first
     * code seen is the failure of the rewrite's REGISTER.
     */
    for (elapsed = 0; elapsed < 5000 && g_ctx.reg_code < 300; elapsed += 50)
        pjsua_handle_events(50);
    if (g_ctx.reg_code < 300 || g_fail_tx.host != NULL) {
        PJ_LOG(3, (THIS_FILE, "    error: the rewrite's REGISTER did not "
                   "fail, last code %d", g_ctx.reg_code));
        rc = -2542;
        goto on_return;
    }

    if (pjsua_var.acc[acc_id].contact_rewritten) {
        PJ_LOG(3, (THIS_FILE, "    error: contact_rewritten is still set "
                   "after the rewrite's REGISTER failed"));
        rc = -2543;
        goto on_return;
    }

    /* The retry registers a fresh Contact, which its 2xx must rewrite */
    if (wait_reg(10000) != 0) {
        PJ_LOG(3, (THIS_FILE, "    error: retry did not register, last "
                   "code %d", g_ctx.reg_code));
        rc = -2544;
        goto on_return;
    }

    if (get_acc_contact_host(acc_id, host, sizeof(host)) != 0 ||
        pj_ansi_strcmp(host, public_host) != 0)
    {
        PJ_LOG(3, (THIS_FILE, "    error: expected Contact host '%s' after "
                   "the retry, got '%.*s'", public_host,
                   (int)pjsua_var.acc[acc_id].contact.slen,
                   pjsua_var.acc[acc_id].contact.ptr));
        rc = -2545;
        goto on_return;
    }

on_return:
    g_mock_reg.received = NULL;
    g_fail_tx.host = NULL;
    pjsua_acc_del(acc_id);
    pjsua_handle_events(500);
    return rc;
}


/*****************************************************************************
 * Unregistered by the application
 *****************************************************************************/

typedef enum unreg_action
{
    UA_UNREG,           /* unregister */
    UA_UNREG_FAILED,    /* unregistration fails with 503 */
    UA_UNREG_SEND_FAIL, /* unregistration fails inside the send */
    UA_REG_FAILED,      /* registration fails with 503, then unregister */
    UA_UNREG_BUSY,      /* unregister while a REGISTER is pending */
    UA_UNREG_RENEW,     /* unregister, then register again */
    UA_UNREG_MODIFY,    /* unregister, then a modify that registers */
    UA_IPC_DONE,        /* unregister before the IP change handles calls */
    UA_NAT_WINDOW,      /* unregister while a NAT rewrite 2xx waits */
    UA_IPC_WINDOW,      /* unregister while the IP change's 2xx waits */
} unreg_action;

typedef struct unreg_case
{
    const char         *title;
    unreg_action        action;
    int                 method;         /* contact_rewrite_method */
    pj_bool_t           expect_reg;     /* IP change must register */
    int                 rc;
} unreg_case;

static pj_status_t ip_change(void)
{
    pjsua_ip_change_param param;

    pjsua_ip_change_param_default(&param);
    /* Without a listener restart the accounts are handled at once */
    param.restart_listener = PJ_FALSE;
    return pjsua_handle_ip_change(&param);
}

#if PJ_HAS_THREADS
static pj_bool_t g_poll_stop;

static int poll_thread(void *arg)
{
    PJ_UNUSED_ARG(arg);
    while (!g_poll_stop)
        pjsua_handle_events(10);
    return 0;
}

/* Unregister while another thread's registration callback for the 2xx of
 * the request sent first waits for PJSUA_LOCK.
 */
static pj_bool_t unreg_in_window(pjsua_acc_id acc_id, pj_bool_t ipc,
                                 unsigned *reg_count)
{
    pj_thread_t *thread;
    pj_status_t status, unreg;
    unsigned i;

    g_poll_stop = PJ_FALSE;
    if (pj_thread_create(pjsua_var.pool, "acc_poll", &poll_thread, NULL,
                         0, 0, &thread) != PJ_SUCCESS)
    {
        return PJ_FALSE;
    }

    PJSUA_LOCK();
    status = ipc? ip_change() : pjsua_acc_set_registration(acc_id, PJ_TRUE);
    /* Accepted once the 2xx has cleared the pending transaction */
    unreg = PJSIP_EBUSY;
    for (i=0; status == PJ_SUCCESS && unreg == PJSIP_EBUSY && i<5000; ++i) {
        if (i)
            pj_thread_sleep(1);
        unreg = pjsua_acc_set_registration(acc_id, PJ_FALSE);
    }
    *reg_count = g_mock_reg.reg_count;
    PJSUA_UNLOCK();

    g_poll_stop = PJ_TRUE;
    pj_thread_join(thread);
    pj_thread_destroy(thread);
    return status == PJ_SUCCESS && unreg == PJ_SUCCESS;
}
#endif

/* Run the action, and take the count of REGISTERs that add a binding once
 * the account is unregistered. Returns PJ_TRUE if it ended as expected.
 */
static pj_bool_t unreg_do_action(pjsua_acc_id acc_id,
                                 pjsua_acc_config *acc_cfg,
                                 unreg_action action, unsigned *reg_count)
{
    pjsua_acc *acc = &pjsua_var.acc[acc_id];
    pj_bool_t ok = PJ_FALSE;
    pj_size_t pools;
    pj_status_t status;

    g_ctx.reg_done = PJ_FALSE;
    g_ctx.reg_code = 0;

    switch (action) {
    case UA_UNREG:
    case UA_UNREG_RENEW:
    case UA_UNREG_MODIFY:
        status = pjsua_acc_set_registration(acc_id, PJ_FALSE);
        ok = status == PJ_SUCCESS && wait_reg(5000) == 0;
        g_ctx.unpublished = !acc->cfg.publish_enabled;
        if (!ok || action == UA_UNREG)
            break;
        g_ctx.reg_done = PJ_FALSE;
        if (action == UA_UNREG_RENEW) {
            status = pjsua_acc_set_registration(acc_id, PJ_TRUE);
        } else {
            acc_cfg->reg_contact_params = pj_str(";test=modify");
            status = pjsua_acc_modify(acc_id, acc_cfg);
        }
        ok = status == PJ_SUCCESS && wait_reg(5000) == 0;
        break;

    case UA_UNREG_FAILED:
    case UA_REG_FAILED:
        g_mock_reg.code = PJSIP_SC_SERVICE_UNAVAILABLE;
        status = pjsua_acc_set_registration(acc_id,
                                            action == UA_REG_FAILED);
        if (status == PJ_SUCCESS)
            pump_events(5000, &g_ctx.reg_code, PJ_TRUE);
        g_mock_reg.code = 0;
        ok = g_ctx.reg_code == PJSIP_SC_SERVICE_UNAVAILABLE &&
             (action == UA_UNREG_FAILED ||
              pjsua_acc_set_registration(acc_id, PJ_FALSE) ==
              PJ_EINVALIDOP);
        break;

    case UA_UNREG_SEND_FAIL:
        g_fail_tx.host = g_mock_reg.live_host;
        status = pjsua_acc_set_registration(acc_id, PJ_FALSE);
        pump_events(1000, NULL, PJ_TRUE);
        ok = g_fail_tx.host == NULL &&
             (status != PJ_SUCCESS || g_ctx.reg_code >= 300);
        break;

    case UA_UNREG_BUSY:
        /* The REGISTER is answered on retransmission */
        g_mock_reg.drop = PJ_TRUE;
        g_mock_reg.dropped = PJ_FALSE;
        status = pjsua_acc_set_registration(acc_id, PJ_TRUE);
        if (status == PJ_SUCCESS)
            pump_events(5000, &g_mock_reg.dropped, PJ_TRUE);
        g_mock_reg.drop = PJ_FALSE;
        /* The refused request is released */
        pools = pjsua_var.cp.used_count;
        status = pjsua_acc_set_registration(acc_id, PJ_FALSE);
        g_ctx.published = acc->publish_sess && acc->cfg.publish_enabled;
        ok = g_mock_reg.dropped && status == PJSIP_EBUSY &&
             pjsua_var.cp.used_count == pools &&
             wait_reg(5000) == 0 && acc->regc != NULL;
        break;

    case UA_IPC_DONE:
        /* The IP change's REGISTER completes, its call handling waits for
         * the timer.
         */
        if (ip_change() != PJ_SUCCESS)
            break;
        pump_events(5000, &g_ctx.ipc_update_cnt, PJ_FALSE);
        if (g_ctx.ipc_update_cnt != 1 ||
            pjsua_acc_set_registration(acc_id, PJ_FALSE) != PJ_SUCCESS)
        {
            break;
        }
        *reg_count = g_mock_reg.reg_count;
        pump_events(5000, &g_ctx.unreg_done, PJ_FALSE);
        return g_ctx.unreg_done;

    case UA_NAT_WINDOW:
    case UA_IPC_WINDOW:
#if PJ_HAS_THREADS
        if (action == UA_NAT_WINDOW) {
            /* Its callback acts on >= 400 only. Without it, the 2xx waits
             * in the registration callback, after the regc is done with it.
             */
            pjsip_regc_set_reg_tsx_cb(acc->regc, NULL);
            g_mock_reg.received = "1.2.3.4";
        }
        return unreg_in_window(acc_id, action == UA_IPC_WINDOW, reg_count);
#else
        break;
#endif
    }

    *reg_count = g_mock_reg.reg_count;
    return ok;
}

static int unreg_test(pjsua_transport_id tp_id, int port,
                      const unreg_case *c)
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id = PJSUA_INVALID_ID;
    char             reg_uri[64];
    char             id[80];
    unsigned         reg_count = 0;
    pj_bool_t        window = (c->action >= UA_IPC_DONE);
    pj_bool_t        ok;
    int              rc = 0;

    PJ_LOG(3, (THIS_FILE, "  pjsua acc: unregistered by %s", c->title));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    pj_ansi_snprintf(reg_uri, sizeof(reg_uri), "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.transport_id = tp_id;
    acc_cfg.register_on_acc_add = PJ_FALSE;
    acc_cfg.allow_contact_rewrite = 2;
    acc_cfg.allow_via_rewrite = PJ_FALSE;
    if (c->method)
        acc_cfg.contact_rewrite_method = c->method;
    /* An auto retry would come within a second */
    acc_cfg.reg_retry_interval = c->expect_reg? 0 : 1;
    acc_cfg.reg_first_retry_interval = acc_cfg.reg_retry_interval;
    acc_cfg.reg_retry_random_interval = 0;
    if (c->action == UA_UNREG_BUSY || c->action == UA_UNREG) {
        /* PUBLISH goes to the account's domain: keep it local */
        pj_ansi_snprintf(id, sizeof(id), "sip:" TEST_USER "@127.0.0.1:%d",
                         port);
        acc_cfg.id = pj_str(id);
        acc_cfg.publish_enabled = PJ_TRUE;
    }

    if (pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id) != PJ_SUCCESS ||
        (c->action != UA_REG_FAILED &&
         (pjsua_acc_set_registration(acc_id, PJ_TRUE) != PJ_SUCCESS ||
          wait_reg(5000) != 0)))
    {
        rc = c->rc - 1;
        goto on_return;
    }

    ok = unreg_do_action(acc_id, &acc_cfg, c->action, &reg_count);
    if (c->action == UA_UNREG_BUSY && !g_ctx.published) {
        PJ_LOG(3, (THIS_FILE, "    error: the refused unregistration "
                   "unpublished the presence"));
        rc = c->rc - 6;
        goto on_return;
    }
    if (!ok) {
        PJ_LOG(3, (THIS_FILE, "    error: the action did not end as "
                   "expected (code %d)", g_ctx.reg_code));
        rc = c->rc - 2;
        goto on_return;
    }
    if (c->action == UA_UNREG && !g_ctx.unpublished) {
        PJ_LOG(3, (THIS_FILE, "    error: the unregistration did not "
                   "unpublish the presence"));
        rc = c->rc - 7;
        goto on_return;
    }

    if (c->expect_reg) {
        g_ctx.reg_done = PJ_FALSE;
        if (ip_change() != PJ_SUCCESS || wait_reg(5000) != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: IP change did not register "
                       "the account"));
            rc = c->rc - 3;
        }
        goto on_return;
    }

    /* Auto retry, then IP change */
    pump_events(2000, NULL, PJ_TRUE);
    if (!window) {
        if (ip_change() != PJ_SUCCESS) {
            rc = c->rc - 3;
            goto on_return;
        }
        pump_events(1000, NULL, PJ_TRUE);
    }
    if (g_mock_reg.reg_count != reg_count ||
        pjsua_var.acc[acc_id].auto_rereg.timer.id)
    {
        PJ_LOG(3, (THIS_FILE, "    error: the account was registered"));
        rc = c->rc - 4;
        goto on_return;
    }
    if (g_ctx.ipc_update_cnt != (c->action == UA_IPC_DONE ||
                                 c->action == UA_IPC_WINDOW) ||
        g_ctx.ipc_done_cnt != (c->action != UA_NAT_WINDOW))
    {
        PJ_LOG(3, (THIS_FILE, "    error: IP change reported %d updates "
                   "and %d completions", g_ctx.ipc_update_cnt,
                   g_ctx.ipc_done_cnt));
        rc = c->rc - 5;
    }

on_return:
    g_mock_reg.received = NULL;
    g_mock_reg.drop = PJ_FALSE;
    g_mock_reg.code = 0;
    g_fail_tx.host = NULL;
    if (acc_id != PJSUA_INVALID_ID)
        pjsua_acc_del(acc_id);
    pjsua_handle_events(500);
    return rc;
}


/* An IP change whose REGISTER fails at once, the failure being reported
 * from inside the send: the re-REGISTER after the un-REGISTER of
 * PJSUA_CONTACT_REWRITE_UNREGISTER, or the retry after a REGISTER still
 * pending. The IP change ends once.
 */
static int ipc_send_fail_test(pjsua_transport_id tp_id, int port,
                              pj_bool_t busy)
{
    static char      host[PJ_INET6_ADDRSTRLEN + 16];
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id = PJSUA_INVALID_ID;
    char             reg_uri[64];
    int              base = busy? -4170 : -4160;
    int              rc = 0;

    PJ_LOG(3, (THIS_FILE, "  pjsua acc: IP change whose %s fails at once",
               busy? "retry" : "re-REGISTER"));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    pj_ansi_snprintf(reg_uri, sizeof(reg_uri), "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.transport_id = tp_id;
    acc_cfg.register_on_acc_add = PJ_FALSE;
    acc_cfg.allow_via_rewrite = PJ_FALSE;
    if (!busy)
        acc_cfg.contact_rewrite_method = PJSUA_CONTACT_REWRITE_UNREGISTER;
    acc_cfg.reg_retry_interval = 0;

    if (pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id) != PJ_SUCCESS ||
        pjsua_acc_set_registration(acc_id, PJ_TRUE) != PJ_SUCCESS ||
        wait_reg(5000) != 0 ||
        get_acc_contact_host(acc_id, host, sizeof(host)) != 0)
    {
        rc = base - 1;
        goto on_return;
    }

    if (busy) {
        /* The IP change finds this REGISTER pending */
        g_mock_reg.drop = PJ_TRUE;
        if (pjsua_acc_set_registration(acc_id, PJ_TRUE) != PJ_SUCCESS) {
            rc = base - 2;
            goto on_return;
        }
        pump_events(500, NULL, PJ_TRUE);
    } else {
        /* The un-REGISTER is sent, the re-REGISTER fails */
        g_fail_tx.skip = 1;
    }
    g_fail_tx.host = host;
    /* With busy, the retry fails within the call and returns the error */
    if (ip_change() != PJ_SUCCESS && !busy) {
        rc = base - 5;
        goto on_return;
    }
    pump_events(2000, NULL, PJ_TRUE);
    if (g_fail_tx.host != NULL) {
        PJ_LOG(3, (THIS_FILE, "    error: the REGISTER was not failed"));
        rc = base - 3;
        goto on_return;
    }
    if (g_ctx.ipc_done_cnt != 1) {
        PJ_LOG(3, (THIS_FILE, "    error: IP change reported %d "
                   "completions", g_ctx.ipc_done_cnt));
        rc = base - 4;
    }

on_return:
    g_mock_reg.drop = PJ_FALSE;
    g_fail_tx.host = NULL;
    g_fail_tx.skip = 0;
    if (acc_id != PJSUA_INVALID_ID)
        pjsua_acc_del(acc_id);
    pjsua_handle_events(500);
    return rc;
}


#if PJ_HAS_THREADS
static volatile pj_bool_t g_abandon_reported;
static pj_bool_t g_abandon_ignored;
static pj_log_func *g_log_func;
static int g_log_level;

/* Tells that the abandon is reported to pjsua, and that pjsua ignored it */
static void abandon_log_func(int level, const char *data, int len)
{
    if (pj_ansi_strstr(data, "Reporting the abandoned authentication"))
        g_abandon_reported = PJ_TRUE;
    if (pj_ansi_strstr(data, "of an earlier REGISTER ignored"))
        g_abandon_ignored = PJ_TRUE;
    if (level <= g_log_level)
        (*g_log_func)(level, data, len);
}

static int abandon_thread(void *arg)
{
    PJ_UNUSED_ARG(arg);
    pjsip_auth_clt_async_abandon(g_defer.sess, g_defer.token);
    return 0;
}

/* A deferred challenge abandoned while an IP change sends its REGISTER: the
 * abandon's registration callback waits for PJSUA_LOCK, which the IP change
 * holds while it claims the registration. With answered, the IP change's
 * REGISTER also gets its 200 before the abandon gets the lock. The abandon
 * must not report an error, end the IP change or end the registration.
 */
static int abandon_window_test(pjsua_transport_id tp_id, int port,
                               pj_bool_t answered)
{
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id = PJSUA_INVALID_ID;
    pj_thread_t     *thread = NULL;
    char             reg_uri[64];
    int              base = answered? -4110 : -4100;
    int              log_level;
    pj_status_t      status;
    int              rc = 0;

    PJ_LOG(3, (THIS_FILE, "  pjsua acc: challenge abandoned during an IP "
               "change%s", answered? ", which registers first" : ""));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    pj_bzero(&g_defer, sizeof(g_defer));
    pj_ansi_snprintf(reg_uri, sizeof(reg_uri), "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.transport_id = tp_id;
    acc_cfg.register_on_acc_add = PJ_FALSE;
    acc_cfg.allow_via_rewrite = PJ_FALSE;
    acc_cfg.reg_retry_interval = 0;

    if (pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id) != PJ_SUCCESS ||
        pjsua_acc_set_registration(acc_id, PJ_TRUE) != PJ_SUCCESS ||
        wait_reg(5000) != 0)
    {
        rc = base - 1;
        goto on_return;
    }

    /* The refresh is challenged and the challenge deferred */
    g_mock_reg.challenge = PJ_TRUE;
    g_defer.enabled = PJ_TRUE;
    if (pjsua_acc_set_registration(acc_id, PJ_TRUE) != PJ_SUCCESS) {
        rc = base - 2;
        goto on_return;
    }
    pump_events(5000, &g_defer.pending, PJ_TRUE);
    g_mock_reg.challenge = PJ_FALSE;
    if (!g_defer.pending) {
        rc = base - 3;
        goto on_return;
    }

    /* Without answered, the IP change's REGISTER stays pending */
    g_mock_reg.drop = !answered;
    g_ctx.reg_code = 0;
    g_ctx.reg_done = PJ_FALSE;
    g_abandon_reported = PJ_FALSE;
    g_abandon_ignored = PJ_FALSE;
    log_level = pj_log_get_level();
    g_log_level = PJ_MAX(log_level, 4);
    g_log_func = pj_log_get_log_func();
    pj_log_set_level(5);
    pj_log_set_log_func(&abandon_log_func);

    PJSUA_LOCK();
    status = pj_thread_create(pjsua_var.pool, "acc_abandon",
                              &abandon_thread, NULL, 0, 0, &thread);
    if (status == PJ_SUCCESS) {
        unsigned i;

        /* The abandon then waits for PJSUA_LOCK in the registration
         * callback
         */
        for (i=0; !g_abandon_reported && i<5000; ++i)
            pj_thread_sleep(1);
        status = ip_change();
    }
    /* The 200 is handled on this thread, which holds PJSUA_LOCK */
    if (status == PJ_SUCCESS && answered)
        pump_events(5000, &g_ctx.reg_done, PJ_TRUE);
    PJSUA_UNLOCK();
    if (thread) {
        pj_thread_join(thread);
        pj_thread_destroy(thread);
        g_defer.pending = PJ_FALSE;
    }
    pj_log_set_log_func(g_log_func);
    pj_log_set_level(log_level);
    if (status != PJ_SUCCESS) {
        rc = base - 4;
        goto on_return;
    }
    /* The IP change ends on a timer */
    if (answered)
        pump_events(2000, &g_ctx.ipc_done_cnt, PJ_TRUE);

    if (g_ctx.reg_code != (answered? 200 : 0) ||
        g_ctx.ipc_done_cnt != (answered? 1 : 0) ||
        pjsua_var.acc[acc_id].regc == NULL ||
        pjsua_var.acc[acc_id].reg_last_code != 200)
    {
        PJ_LOG(3, (THIS_FILE, "    error: the abandoned challenge reported "
                   "%d, the IP change ended %d time(s), the registration "
                   "%s", g_ctx.reg_code, g_ctx.ipc_done_cnt,
                   pjsua_var.acc[acc_id].regc? "remains" : "ended"));
        rc = base - 5;
        goto on_return;
    }
    if (!g_abandon_ignored) {
        PJ_LOG(3, (THIS_FILE, "    error: the abandon did not reach pjsua "
                   "during the IP change"));
        rc = base - 8;
        goto on_return;
    }
    if (answered)
        goto on_return;

    /* The IP change's REGISTER is answered on retransmission */
    g_mock_reg.drop = PJ_FALSE;
    if (wait_reg(10000) != 0 || g_ctx.reg_code != 200) {
        rc = base - 6;
        goto on_return;
    }
    pump_events(2000, &g_ctx.ipc_done_cnt, PJ_TRUE);
    if (g_ctx.ipc_done_cnt != 1) {
        rc = base - 7;
    }

on_return:
    if (g_defer.pending)
        pjsip_auth_clt_async_abandon(g_defer.sess, g_defer.token);
    pj_bzero(&g_defer, sizeof(g_defer));
    g_mock_reg.challenge = PJ_FALSE;
    g_mock_reg.drop = PJ_FALSE;
    if (acc_id != PJSUA_INVALID_ID)
        pjsua_acc_del(acc_id);
    pjsua_handle_events(500);
    return rc;
}

typedef enum final_kind
{
    FINAL_503,      /* a REGISTER gets a 503 */
    FINAL_UNREG,    /* an unregistration gets a 200 */
    FINAL_REG,      /* a REGISTER gets a 200 */
    FINAL_REG_FAIL, /* a REGISTER gets a 200, the newer one a 503 */
} final_kind;

/* A REGISTER is sent while the registration callback of the request before
 * it waits for PJSUA_LOCK on another thread. That earlier result must not
 * be acted on: neither end the registration of the newer REGISTER nor be
 * reported.
 */
static int final_window_test(pjsua_transport_id tp_id, int port,
                             final_kind kind)
{
    static const char *title[] = { "503", "unregistration 200", "200",
                                   "200 (the newer one fails)" };
    pjsua_acc_config acc_cfg;
    pjsua_acc_id     acc_id = PJSUA_INVALID_ID;
    pj_thread_t     *thread = NULL;
    pjsip_regc_info  info;
    char             reg_uri[64];
    int              base = -4120 - 10 * (int)kind;
    pj_status_t      status, reg;
    unsigned         i;
    int              rc = 0;

    PJ_LOG(3, (THIS_FILE, "  pjsua acc: REGISTER sent while the %s of the "
               "previous request waits", title[kind]));

    pj_bzero(&g_ctx, sizeof(g_ctx));
    pj_ansi_snprintf(reg_uri, sizeof(reg_uri), "sip:127.0.0.1:%d", port);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id       = pj_str("sip:" TEST_USER "@pjsip.org");
    acc_cfg.reg_uri  = pj_str(reg_uri);
    acc_cfg.transport_id = tp_id;
    acc_cfg.register_on_acc_add = PJ_FALSE;
    acc_cfg.allow_via_rewrite = PJ_FALSE;
    acc_cfg.reg_retry_interval = 0;
    if (kind == FINAL_REG_FAIL) {
        acc_cfg.reg_retry_interval = 300;
        acc_cfg.reg_first_retry_interval = 300;
    }

    if (pjsua_acc_add(&acc_cfg, PJ_FALSE, &acc_id) != PJ_SUCCESS ||
        pjsua_acc_set_registration(acc_id, PJ_TRUE) != PJ_SUCCESS ||
        wait_reg(5000) != 0)
    {
        rc = base - 1;
        goto on_return;
    }

    g_ctx.reg_done = PJ_FALSE;
    g_ctx.reg_code = 0;
    g_ctx.reg_ok_count = 0;
    g_poll_stop = PJ_FALSE;
    if (pj_thread_create(pjsua_var.pool, "acc_poll", &poll_thread, NULL,
                         0, 0, &thread) != PJ_SUCCESS)
    {
        rc = base - 2;
        goto on_return;
    }

    PJSUA_LOCK();
    if (kind == FINAL_503) {
        g_mock_reg.code = PJSIP_SC_SERVICE_UNAVAILABLE;
        g_mock_reg.code_once = PJ_TRUE;
    } else if (kind == FINAL_REG_FAIL) {
        /* The earlier REGISTER is a retry */
        pjsua_var.acc[acc_id].auto_rereg.attempt_cnt = 1;
        g_mock_reg.code = PJSIP_SC_SERVICE_UNAVAILABLE;
        g_mock_reg.code_once = PJ_TRUE;
        g_mock_reg.code_skip = 1;
    }
    status = pjsua_acc_set_registration(acc_id, kind != FINAL_UNREG);
    /* Accepted once the final response has cleared the pending
     * transaction
     */
    reg = PJSIP_EBUSY;
    for (i=0; status == PJ_SUCCESS && reg == PJSIP_EBUSY && i<5000; ++i) {
        if (i)
            pj_thread_sleep(1);
        reg = pjsua_acc_set_registration(acc_id, PJ_TRUE);
    }
    if (status == PJ_SUCCESS)
        status = reg;
    PJSUA_UNLOCK();

    g_poll_stop = PJ_TRUE;
    pj_thread_join(thread);
    pj_thread_destroy(thread);
    if (status != PJ_SUCCESS) {
        rc = base - 3;
        goto on_return;
    }

    /* The earlier result is handled first, on the polling thread. Then
     * the newer REGISTER completes.
     */
    if (kind == FINAL_REG_FAIL) {
        pump_events(5000, &g_ctx.reg_code, PJ_TRUE);
        if (g_ctx.reg_code != PJSIP_SC_SERVICE_UNAVAILABLE ||
            g_ctx.reg_ok_count != 0)
        {
            PJ_LOG(3, (THIS_FILE, "    error: reported %d after %u 200s",
                       g_ctx.reg_code, g_ctx.reg_ok_count));
            rc = base - 4;
        } else if (pjsua_var.acc[acc_id].auto_rereg.attempt_cnt != 0) {
            PJ_LOG(3, (THIS_FILE, "    error: the earlier 200 did not "
                       "reset the retry count"));
            rc = base - 6;
        }
        goto on_return;
    }
    wait_reg(5000);
    for (i=0; pjsua_var.acc[acc_id].regc && i<500; ++i) {
        if (pjsip_regc_get_info(pjsua_var.acc[acc_id].regc,
                                &info) != PJ_SUCCESS || !info.is_busy)
        {
            break;
        }
        pump_events(10, NULL, PJ_TRUE);
    }
    if (!g_ctx.reg_done || pjsua_var.acc[acc_id].regc == NULL ||
        pjsua_var.acc[acc_id].reg_last_code != 200)
    {
        PJ_LOG(3, (THIS_FILE, "    error: the earlier result ended the "
                   "registration of the newer REGISTER"));
        rc = base - 4;
        goto on_return;
    }
    if (g_ctx.reg_ok_count != 1) {
        PJ_LOG(3, (THIS_FILE, "    error: %u registration 200s reported",
                   g_ctx.reg_ok_count));
        rc = base - 5;
        goto on_return;
    }
    if (!pjsip_regc_is_latest(pjsua_var.acc[acc_id].regc,
                              g_ctx.reg_ok_cseq))
    {
        PJ_LOG(3, (THIS_FILE, "    error: the 200 of the earlier request "
                   "was reported"));
        rc = base - 6;
    }

on_return:
    g_mock_reg.code = 0;
    g_mock_reg.code_once = PJ_FALSE;
    g_mock_reg.code_skip = 0;
    if (acc_id != PJSUA_INVALID_ID)
        pjsua_acc_del(acc_id);
    pjsua_handle_events(500);
    return rc;
}
#endif


/*****************************************************************************
 * Main entry point
 *****************************************************************************/

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
     * Destroy it here and recreate it via restore_test_endpt() afterwards.
     */
    pjsip_endpt_destroy(endpt);
    endpt = NULL;

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_create failed (%d)", status));
        restore_test_endpt();
        return -2401;
    }

    pjsua_config_default(&ua_cfg);
    ua_cfg.cb.on_reg_state2 = &on_reg_state2;
    ua_cfg.cb.on_ip_change_progress = &on_ip_change_progress;
    ua_cfg.cb.on_auth_challenge = &on_auth_challenge;
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
        restore_test_endpt();
        return -2402;
    }

    pjsua_transport_config_default(&tp_cfg);
    tp_cfg.port = 0;  /* ephemeral */

    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tp_cfg, &tp_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_transport_create failed (%d)", status));
        pjsua_destroy();
        restore_test_endpt();
        return -2403;
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_start failed (%d)", status));
        pjsua_destroy();
        restore_test_endpt();
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
        restore_test_endpt();
        return -2405;
    }

    status = pjsip_endpt_register_module(pjsua_get_pjsip_endpt(),
                                         &g_fail_tx.mod);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  register send failure module failed (%d)",
                   status));
        rc = -2406;
        goto on_return;
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
              "2001:db8::1", "2001:db8::1", NULL, NULL, 0, -2600 },

            /* Plain IPv4: the ordinary rewrite must still happen. */
            { "IPv4 received",
              "1.2.3.4", "1.2.3.4", NULL, NULL, 0, -2700 },

            /* '!' is accepted by the Via param scanner but not by the URI
             * host scanner, so the rebuilt Contact would not parse back.
             */
            { "received with invalid host char",
              "1.2.3.4!", NULL, NULL, NULL, 0, -2800 },

            /* Colon bearing token that is not an IPv6 address: must not
             * be bracketed into a Contact that parses but means nothing.
             */
            { "received with non-address colon",
              "foo:bar", NULL, NULL, NULL, 0, -2900 },

            /* Same rejection, but with PJSUA_CONTACT_REWRITE_UNREGISTER:
             * the declined rewrite must leave the registration alone.
             */
            { "declined rewrite with UNREGISTER method",
              "1.2.3.4!", NULL, NULL, NULL,
              PJSUA_CONTACT_REWRITE_UNREGISTER, -3000 },

            /* reg_contact_uri_params sends update_regc_contact() through
             * its parse-and-regenerate path on the rewritten Contact.
             */
            { "accepted rewrite with reg_contact_uri_params",
              "1.2.3.4", "1.2.3.4", NULL, ";acc-test=1", 0, -3100 },

            /* "Contact: *" parses to a header with a NULL uri. Building
             * the registration Contact from it must fall back instead of
             * dereferencing it, and pjsip_regc must then refuse it rather
             * than register a Contact it will later dereference.
             */
            { "force_contact star",
              "1.2.3.4", NULL, "*", ";acc-test=1", 0, -3200, PJ_TRUE },

            /* A non-SIP URI parses fine but must not be cast to
             * pjsip_sip_uri* and read as one.
             */
            { "force_contact non-SIP URI",
              "1.2.3.4", NULL, "<tel:+15551234>", ";acc-test=1", 0, -3300 },

            /* A Contact that pjsip_regc refuses must leave the existing
             * registration binding untouched, not queued for removal.
             */
            { "rejected update_contact keeps the binding",
              "1.2.3.4", "1.2.3.4", NULL, NULL, 0, -3400,
              PJ_FALSE, PJ_TRUE },

            /* ALWAYS_UPDATE names no 2xx method; the 2xx rewrite must
             * still reach the registrar, not only the account.
             */
            { "accepted rewrite with ALWAYS_UPDATE alone",
              "1.2.3.4", "1.2.3.4", NULL, NULL,
              PJSUA_CONTACT_REWRITE_ALWAYS_UPDATE, -3500 },

            /* Both 2xx methods: UNREGISTER would lose the new Contact */
            { "accepted rewrite with UNREGISTER and NO_UNREG",
              "1.2.3.4", "1.2.3.4", NULL, NULL,
              PJSUA_CONTACT_REWRITE_UNREGISTER |
              PJSUA_CONTACT_REWRITE_NO_UNREG, -3600 },
        };
        unsigned i;

        for (i=0; i<PJ_ARRAY_SIZE(cases); ++i) {
            rc = rewrite_test(tp_id, (int)port, &cases[i]);
            if (rc != 0) goto on_return;
        }
    }

    rc = rewrite_send_fail_test(tp_id, (int)port);
    if (rc != 0) goto on_return;

    {
        static const unreg_case cases[] = {
            { "unregistration", UA_UNREG, 0, PJ_FALSE, -4000 },
            { "a failed unregistration", UA_UNREG_FAILED, 0, PJ_FALSE,
              -4010 },
            { "an unregistration failed in the send", UA_UNREG_SEND_FAIL,
              0, PJ_FALSE, -4020 },
            { "unregistering a failed registration", UA_REG_FAILED, 0,
              PJ_FALSE, -4030 },
            { "a refused unregistration", UA_UNREG_BUSY, 0, PJ_TRUE,
              -4040 },
            { "unregistration, then registration", UA_UNREG_RENEW, 0,
              PJ_TRUE, -4050 },
            { "unregistration, then modify", UA_UNREG_MODIFY, 0, PJ_TRUE,
              -4060 },
            { "unregistration before IP change call handling", UA_IPC_DONE,
              0, PJ_FALSE, -4070 },
#if PJ_HAS_THREADS
            { "unregistration during a NAT rewrite", UA_NAT_WINDOW,
              PJSUA_CONTACT_REWRITE_UNREGISTER, PJ_FALSE, -4080 },
            { "unregistration during an IP change", UA_IPC_WINDOW,
              PJSUA_CONTACT_REWRITE_UNREGISTER, PJ_FALSE, -4090 },
#endif
        };
        unsigned i;

        for (i=0; i<PJ_ARRAY_SIZE(cases); ++i) {
            rc = unreg_test(tp_id, (int)port, &cases[i]);
            if (rc != 0) goto on_return;
        }
    }

    rc = ipc_send_fail_test(tp_id, (int)port, PJ_FALSE);
    if (rc != 0) goto on_return;
    rc = ipc_send_fail_test(tp_id, (int)port, PJ_TRUE);
    if (rc != 0) goto on_return;

#if PJ_HAS_THREADS
    rc = abandon_window_test(tp_id, (int)port, PJ_FALSE);
    if (rc != 0) goto on_return;
    rc = abandon_window_test(tp_id, (int)port, PJ_TRUE);
    if (rc != 0) goto on_return;
    rc = final_window_test(tp_id, (int)port, FINAL_503);
    if (rc != 0) goto on_return;
    rc = final_window_test(tp_id, (int)port, FINAL_UNREG);
    if (rc != 0) goto on_return;
    rc = final_window_test(tp_id, (int)port, FINAL_REG);
    if (rc != 0) goto on_return;
    rc = final_window_test(tp_id, (int)port, FINAL_REG_FAIL);
    if (rc != 0) goto on_return;
#endif

on_return:
    if (g_fail_tx.mod.id != -1)
        pjsip_endpt_unregister_module(pjsua_get_pjsip_endpt(),
                                      &g_fail_tx.mod);
    if (g_mock_reg.mod.id != -1)
        pjsip_endpt_unregister_module(pjsua_get_pjsip_endpt(),
                                      &g_mock_reg.mod);

    pjsua_handle_events(500);
    pjsua_destroy2(PJSUA_DESTROY_NO_RX_MSG);

    restore_test_endpt();

    return rc;
}

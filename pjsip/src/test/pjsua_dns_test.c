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
 * pjsua-level tests for the nameserver configuration.
 *
 * The two local DNS servers hold different records, so a successful
 * resolution tells which of them the pjsua resolver has asked.
 */

#include "test.h"
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>

#define THIS_FILE   "pjsua_dns_test.c"

#if PJSIP_HAS_RESOLVER

#define NAME_ONE    "one.pjsua-dns.test"
#define NAME_TWO    "two.pjsua-dns.test"
#define NAME_THREE  "three.pjsua-dns.test"
#define NAME_FOUR   "four.pjsua-dns.test"

static struct {
    pj_bool_t   done;
    pj_status_t status;
    char        addr[PJ_INET6_ADDRSTRLEN];
} g_res;

static void resolve_cb(pj_status_t status, void *token,
                       const struct pjsip_server_addresses *addr)
{
    PJ_UNUSED_ARG(token);

    g_res.status = status;
    if (status == PJ_SUCCESS && addr->count) {
        pj_sockaddr_print(&addr->entry[0].addr, g_res.addr,
                          sizeof(g_res.addr), 0);
    }
    g_res.done = PJ_TRUE;
}

static int resolve(const char *host, const char *expected)
{
    pjsip_host_info target;
    pj_pool_t *pool;
    unsigned i;

    pj_bzero(&g_res, sizeof(g_res));
    pj_bzero(&target, sizeof(target));
    target.type = PJSIP_TRANSPORT_UDP;
    target.addr.host = pj_str((char*)host);
    target.addr.port = 5060;

    pool = pjsua_pool_create("dnstest", 512, 512);
    pjsip_endpt_resolve(pjsua_get_pjsip_endpt(), pool, &target, NULL,
                        &resolve_cb);
    for (i = 0; i < 500 && !g_res.done; ++i)
        pjsua_handle_events(10);

    /* The pending query still refers to the pool */
    if (!g_res.done) {
        PJ_LOG(1,(THIS_FILE, "  resolving %s timed out", host));
        return -1;
    }
    pj_pool_release(pool);

    /* NULL expected address means the host must not resolve */
    if (!expected) {
        if (g_res.status == PJ_SUCCESS) {
            PJ_LOG(1,(THIS_FILE, "  %s resolved to %s", host, g_res.addr));
            return -1;
        }
        return 0;
    }
    if (g_res.status != PJ_SUCCESS) {
        PJ_PERROR(1,(THIS_FILE, g_res.status, "  resolving %s failed", host));
        return -1;
    }
    if (pj_ansi_strcmp(g_res.addr, expected)) {
        PJ_LOG(1,(THIS_FILE, "  %s resolved to %s instead of %s",
                  host, g_res.addr, expected));
        return -1;
    }
    return 0;
}

static pj_status_t create_server(int af, const char *name, const char *ip,
                                 pj_dns_server **p_srv, pj_uint16_t *port)
{
    pj_dns_parsed_rr rr;
    pj_str_t res_name;
    pj_in_addr ip_addr;
    pj_sockaddr bound_addr;
    pj_status_t status;

    status = pj_dns_server_create(pjsua_get_pool_factory(),
                                  pjsip_endpt_get_ioqueue(
                                      pjsua_get_pjsip_endpt()),
                                  af, 0, 0, p_srv);
    if (status != PJ_SUCCESS)
        return status;

    res_name = pj_str((char*)name);
    ip_addr = pj_inet_addr2(ip);
    pj_dns_init_a_rr(&rr, &res_name, PJ_DNS_CLASS_IN, 60, &ip_addr);
    status = pj_dns_server_add_rec(*p_srv, 1, &rr);
    if (status == PJ_SUCCESS)
        status = pj_dns_server_get_addr(*p_srv, &bound_addr);
    if (status != PJ_SUCCESS) {
        pj_dns_server_destroy(*p_srv);
        *p_srv = NULL;
        return status;
    }

    *port = pj_sockaddr_get_port(&bound_addr);
    return PJ_SUCCESS;
}

static pj_status_t add_a_rec(pj_dns_server *srv, const char *name,
                             const char *ip)
{
    pj_dns_parsed_rr rr;
    pj_str_t res_name = pj_str((char*)name);
    pj_in_addr ip_addr = pj_inet_addr2(ip);

    pj_dns_init_a_rr(&rr, &res_name, PJ_DNS_CLASS_IN, 60, &ip_addr);
    return pj_dns_server_add_rec(srv, 1, &rr);
}

static int update_ns(unsigned count, const char *entries[],
                     pj_status_t expected_status,
                     unsigned expected_count, const char *expected_first)
{
    pj_str_t srv[8];
    pj_status_t status;
    unsigned i;

    for (i = 0; i < count; ++i)
        srv[i] = pj_str((char*)entries[i]);

    status = pjsua_update_nameservers(count, srv);
    if (status != expected_status) {
        PJ_PERROR(1,(THIS_FILE, status,
                     "  pjsua_update_nameservers() returned %d, expected %d",
                     status, expected_status));
        return -1;
    }

    PJ_TEST_EQ(pjsua_get_var()->ua_cfg.nameserver_count, expected_count,
               NULL, return -1);
    if (expected_first) {
        pj_str_t first = pj_str((char*)expected_first);
        PJ_TEST_EQ(pj_strcmp(&pjsua_get_var()->ua_cfg.nameserver[0], &first),
                   0, NULL, return -1);
    }
    return 0;
}

static pj_dns_server *g_srv_a, *g_srv_b;
static char g_ns_a[32], g_ns_b[32];

static void stop_pjsua(void)
{
    if (g_srv_a) {
        pj_dns_server_destroy(g_srv_a);
        g_srv_a = NULL;
    }
    if (g_srv_b) {
        pj_dns_server_destroy(g_srv_b);
        g_srv_b = NULL;
    }
    pjsua_destroy2(PJSUA_DESTROY_NO_RX_MSG);
}

static pj_status_t start_pjsua(pj_bool_t with_servers,
                               const char *invalid_ns, pj_bool_t with_ns_a)
{
    pjsua_config ua_cfg;
    pjsua_logging_config log_cfg;
    char ns_buf[2][48];
    pj_str_t ns_any = pj_str("127.0.0.1");
    pj_uint16_t port_a, port_b;
    pj_status_t status;

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1,(THIS_FILE, status, "  pjsua_create failed"));
        return status;
    }

    status = pjsua_update_nameservers(1, &ns_any);
    if (status != PJ_EINVALIDOP) {
        PJ_PERROR(1,(THIS_FILE, status, "  update before pjsua_init returned"));
        pjsua_destroy2(PJSUA_DESTROY_NO_RX_MSG);
        return PJ_EBUG;
    }

    if (with_servers) {
        status = create_server(pj_AF_INET(), NAME_ONE, "10.0.0.1", &g_srv_a,
                               &port_a);
        if (status == PJ_SUCCESS)
            status = create_server(pj_AF_INET(), NAME_TWO, "10.0.0.2",
                                   &g_srv_b, &port_b);
        if (status == PJ_SUCCESS)
            status = add_a_rec(g_srv_b, NAME_THREE, "10.0.0.3");
        if (status == PJ_SUCCESS)
            status = add_a_rec(g_srv_b, NAME_ONE, "10.0.0.11");
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1,(THIS_FILE, status, "  creating DNS servers failed"));
            stop_pjsua();
            return status;
        }
        pj_ansi_snprintf(g_ns_a, sizeof(g_ns_a), "127.0.0.1:%d", port_a);
        pj_ansi_snprintf(g_ns_b, sizeof(g_ns_b), "127.0.0.1:%d", port_b);
    }

    pjsua_config_default(&ua_cfg);
    ua_cfg.thread_cnt = 0;
    if (with_ns_a) {
        pj_ansi_strxcpy(ns_buf[ua_cfg.nameserver_count], g_ns_a,
                        sizeof(ns_buf[0]));
        ua_cfg.nameserver[ua_cfg.nameserver_count] =
            pj_str(ns_buf[ua_cfg.nameserver_count]);
        ++ua_cfg.nameserver_count;
    }
    if (invalid_ns) {
        pj_ansi_strxcpy(ns_buf[ua_cfg.nameserver_count], invalid_ns,
                        sizeof(ns_buf[0]));
        ua_cfg.nameserver[ua_cfg.nameserver_count] =
            pj_str(ns_buf[ua_cfg.nameserver_count]);
        ++ua_cfg.nameserver_count;
    }

    pjsua_logging_config_default(&log_cfg);
    log_cfg.level = 3;
    log_cfg.console_level = 3;

    status = pjsua_init(&ua_cfg, &log_cfg, NULL);

    /* pjsua must keep its own copy of the config strings */
    pj_memset(ns_buf, 'x', sizeof(ns_buf));

    if (status != PJ_SUCCESS)
        stop_pjsua();
    return status;
}

static int update_test(void)
{
    pjsip_endpoint *sip_endpt = pjsua_get_pjsip_endpt();
    pj_dns_resolver *res, *app_res;
    const char *entries[8];
    char ns_c[48];
    pj_dns_server *srv_c = NULL;
    pj_uint16_t port_c;
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  initialized without nameserver"));
    PJ_TEST_EQ(pjsip_endpt_get_resolver(sip_endpt), NULL, NULL,
               return -2610);
    PJ_TEST_EQ(pjsua_get_var()->ua_cfg.nameserver_count, 0, NULL,
               return -2611);

    PJ_LOG(3,(THIS_FILE, "  set nameservers, ignoring invalid entries"));
    entries[0] = "1.2.3.4:abc";
    entries[1] = g_ns_a;
    entries[2] = "[::1";
    if (update_ns(3, entries, PJ_SUCCESS, 1, g_ns_a))
        return -2620;
    res = pjsip_endpt_get_resolver(sip_endpt);
    PJ_TEST_NOT_NULL(res, NULL, return -2621);
    PJ_TEST_EQ(pjsua_get_var()->resolver, res, NULL, return -2622);
    if (resolve(NAME_ONE, "10.0.0.1"))
        return -2623;
    if (resolve(NAME_TWO, NULL))
        return -2624;

    PJ_LOG(3,(THIS_FILE, "  change nameservers"));
    entries[0] = g_ns_b;
    if (update_ns(1, entries, PJ_SUCCESS, 1, g_ns_b))
        return -2630;
    PJ_TEST_EQ(pjsip_endpt_get_resolver(sip_endpt), res, NULL,
               return -2631);
    if (resolve(NAME_TWO, "10.0.0.2"))
        return -2632;
    if (resolve(NAME_ONE, "10.0.0.11"))
        return -2633;

    PJ_LOG(3,(THIS_FILE, "  no valid entry keeps the nameservers"));
    entries[0] = "1.2.3.4:abc";
    entries[1] = "";
    if (update_ns(2, entries, PJLIB_UTIL_EDNSINNSADDR, 1, g_ns_b))
        return -2640;
    if (resolve(NAME_THREE, "10.0.0.3"))
        return -2641;

    PJ_LOG(3,(THIS_FILE, "  too many entries"));
    entries[0] = entries[1] = entries[2] = entries[3] = entries[4] = g_ns_a;
    if (update_ns(5, entries, PJ_ETOOMANY, 1, g_ns_b))
        return -2650;

    PJ_LOG(3,(THIS_FILE, "  resolver replaced by the application"));
    status = pjsip_endpt_create_resolver(sip_endpt, &app_res);
    PJ_TEST_SUCCESS(status, NULL, return -2660);
    pjsip_endpt_set_resolver(sip_endpt, app_res);
    entries[0] = g_ns_a;
    rc = update_ns(1, entries, PJ_EINVALIDOP, 1, g_ns_b);
    pjsip_endpt_set_resolver(sip_endpt, res);
    pj_dns_resolver_destroy(app_res, PJ_FALSE);
    if (rc)
        return -2661;

    PJ_LOG(3,(THIS_FILE, "  empty list detaches the resolver"));
    if (update_ns(0, NULL, PJ_SUCCESS, 0, NULL))
        return -2670;
    PJ_TEST_EQ(pjsip_endpt_get_resolver(sip_endpt), NULL, NULL,
               return -2671);
    PJ_TEST_EQ(pjsua_get_var()->resolver, NULL, NULL, return -2672);

    PJ_LOG(3,(THIS_FILE, "  next list attaches the same resolver"));
    entries[0] = g_ns_b;
    if (update_ns(1, entries, PJ_SUCCESS, 1, g_ns_b))
        return -2680;
    PJ_TEST_EQ(pjsip_endpt_get_resolver(sip_endpt), res, NULL,
               return -2681);
    if (resolve(NAME_THREE, "10.0.0.3"))
        return -2682;

#if PJ_HAS_IPV6
    status = create_server(pj_AF_INET6(), NAME_FOUR, "10.0.0.4", &srv_c,
                           &port_c);
    if (status == PJ_SUCCESS) {
        PJ_LOG(3,(THIS_FILE, "  IPv6 nameserver"));
        pj_ansi_snprintf(ns_c, sizeof(ns_c), "[::1]:%d", port_c);
        entries[0] = ns_c;
        rc = update_ns(1, entries, PJ_SUCCESS, 1, ns_c);
        if (rc == 0 && resolve(NAME_FOUR, "10.0.0.4"))
            rc = -1;
        pj_dns_server_destroy(srv_c);
        if (rc)
            return -2690;
    } else {
        PJ_PERROR(3,(THIS_FILE, status,
                     "  skipping IPv6 nameserver test"));
    }
#else
    PJ_UNUSED_ARG(srv_c);
    PJ_UNUSED_ARG(port_c);
    PJ_UNUSED_ARG(ns_c);
#endif

    /* Leave the resolver detached for pjsua_destroy() */
    if (update_ns(0, NULL, PJ_SUCCESS, 0, NULL))
        return -2695;

    return 0;
}

static int init_test(void)
{
    pj_str_t ns_a = pj_str(g_ns_a);
    pjsua_config *cfg = &pjsua_get_var()->ua_cfg;

    PJ_LOG(3,(THIS_FILE, "  initialized with an invalid nameserver"));
    PJ_TEST_EQ(cfg->nameserver_count, 1, NULL, return -2710);
    PJ_TEST_EQ(pj_strcmp(&cfg->nameserver[0], &ns_a), 0, NULL,
               return -2711);
    PJ_TEST_NOT_NULL(pjsip_endpt_get_resolver(pjsua_get_pjsip_endpt()),
                     NULL, return -2712);
    if (resolve(NAME_ONE, "10.0.0.1"))
        return -2713;

    return 0;
}

int pjsua_dns_test(void)
{
    pj_status_t status;
    int rc;

    PJ_LOG(3,(THIS_FILE, "pjsua nameserver update test"));

    /* The pjsip test framework's global endpoint owns the tsx layer
     * singleton, which pjsua would try to register on its own endpoint.
     */
    pjsip_endpt_destroy(endpt);
    endpt = NULL;

    status = start_pjsua(PJ_TRUE, NULL, PJ_FALSE);
    if (status != PJ_SUCCESS) {
        rc = -2600;
        goto on_restore;
    }
    rc = update_test();
    stop_pjsua();
    if (rc)
        goto on_restore;

    status = start_pjsua(PJ_TRUE, "1.2.3.4:abc", PJ_TRUE);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1,(THIS_FILE, status, "  pjsua_init failed"));
        rc = -2700;
        goto on_restore;
    }
    rc = init_test();
    stop_pjsua();
    if (rc)
        goto on_restore;

    PJ_LOG(3,(THIS_FILE, "  initialized without any valid nameserver"));
    status = start_pjsua(PJ_FALSE, "1.2.3.4:abc", PJ_FALSE);
    if (status != PJLIB_UTIL_EDNSINNSADDR) {
        PJ_PERROR(1,(THIS_FILE, status, "  pjsua_init returned"));
        if (status == PJ_SUCCESS)
            stop_pjsua();
        rc = -2720;
    }

on_restore:
    status = pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);
    if (status == PJ_SUCCESS)
        status = pjsip_tsx_layer_init_module(endpt);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1,(THIS_FILE, status, "  restoring the endpoint failed"));
        if (rc == 0)
            rc = -2603;
    }

    return rc;
}

#else   /* PJSIP_HAS_RESOLVER */

int pjsua_dns_test(void)
{
    pj_str_t srv = pj_str("127.0.0.1");

    PJ_LOG(3,(THIS_FILE, "pjsua nameserver update test (no resolver)"));

    PJ_TEST_EQ(pjsua_update_nameservers(1, &srv), PJ_EINVALIDOP, NULL,
               return -2600);
    return 0;
}

#endif  /* PJSIP_HAS_RESOLVER */

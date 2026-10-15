/*
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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

#include "test.h"
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "dlg_core_test.c"

/*
 * Regression test for issue #5100: pj_hash_calc_tolower() can legitimately
 * return zero for certain inputs. The dialog set hash table in sip_ua_layer.c
 * keys dialog sets by the (lowercased) local tag, and pjsip_ua_register_dlg()
 * used to reject a zero tag hash as if it were an "uncomputed" marker. A zero
 * hash is however a valid value: the dialog table must be able to register a
 * dialog set under such a tag and then find it again on incoming traffic.
 *
 * This test exercises the exact hash table usage pattern of sip_ua_layer.c
 * (register with the tag hash, then look up both with NULL and with the cached
 * hash) using a tag that is known to hash to zero, and verifies the dialog set
 * round-trips. It fails if any scheme stores the entry under a different hash
 * than the lookups recompute (e.g. remapping the zero hash on insert only).
 */
static int tag_hval_zero_test(void)
{
    /* This particular UUID djb2-hashes (lowercased, 32-bit) to exactly zero;
     * it is the local tag from the original bug report. */
    const pj_str_t zero_tag = { "d48600d9-514b-4655-a606-43a65dc1f54b", 36 };
    int marker = 12345;         /* stand-in for a dlg_set pointer value */
    pj_pool_t *pool;
    pj_hash_table_t *ht;
    pj_uint32_t hval;
    void *found;
    int rc = 0;

    /* Confirm the trigger condition still holds: the raw hash is zero. If the
     * hash algorithm ever changes, this vector may need to be regenerated. */
    hval = pj_hash_calc_tolower(0, NULL, &zero_tag);
    PJ_TEST_EQ(hval, 0, "test vector no longer hashes to zero", return -10);

    pool = pjsip_endpt_create_pool(endpt, "dlgtest", 4000, 4000);
    PJ_TEST_NOT_NULL(pool, NULL, return -20);
    ht = pj_hash_create(pool, 32);
    PJ_TEST_NOT_NULL(ht, NULL, {rc = -30; goto on_return;});

    /* Register the entry, mirroring pjsip_ua_register_dlg(): pass the cached
     * (zero) tag hash. */
    pj_hash_set_lower(pool, ht, zero_tag.ptr, (unsigned)zero_tag.slen,
                      hval, &marker);

    /* Lookup as the incoming-message paths do (sip_ua_layer.c:503/623/822):
     * NULL hval, so the table recomputes the hash. */
    found = pj_hash_get_lower(ht, zero_tag.ptr, (unsigned)zero_tag.slen, NULL);
    PJ_TEST_EQ(found, &marker, "zero-hash entry not found via recompute",
               {rc = -40; goto on_return;});

    /* Lookup as pjsip_ua_register_dlg()/unregister do: pass the cached hash. */
    found = pj_hash_get_lower(ht, zero_tag.ptr, (unsigned)zero_tag.slen, &hval);
    PJ_TEST_EQ(found, &marker, "zero-hash entry not found via cached hval",
               {rc = -50; goto on_return;});

    /* A different tag must not collide onto the zero-hash entry. */
    {
        const pj_str_t other = { "abc123", 6 };
        found = pj_hash_get_lower(ht, other.ptr, (unsigned)other.slen, NULL);
        PJ_TEST_EQ(found, NULL, "unexpected match for a different tag",
                   {rc = -60; goto on_return;});
    }

on_return:
    pjsip_endpt_release_pool(endpt, pool);
    return rc;
}

int dlg_core_test(void)
{
    return tag_hval_zero_test();
}

/* An external resolver that holds the query until the test answers it */
static struct {
    pj_bool_t                started;
    void                    *token;
    pjsip_resolver_callback *cb;
} held;

static void hold_resolve(pjsip_resolver_t *resolver, pj_pool_t *pool,
                         const pjsip_host_info *target, void *token,
                         pjsip_resolver_callback *cb)
{
    PJ_UNUSED_ARG(resolver);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(target);

    held.started = PJ_TRUE;
    held.token = token;
    held.cb = cb;
}

/*
 * An ACK for a 2xx has no transaction, so it may still wait for its
 * destination to be resolved after the dialog is destroyed. It must not
 * use the dialog's memory then. ASan reports the access; the Route and Via
 * checks at the end fail without it. As in inv_dec_ref_unlocked_test(), the
 * dialog pool is grown so that its release is a real free.
 */
int dlg_ack_resolve_test(void)
{
    static pjsip_ext_resolver ext = { &hold_resolve };
    pj_str_t local = pj_str("sip:alice@127.0.0.1");
    pj_str_t remote = pj_str("sip:bob@127.0.0.1");
    pj_str_t route_name = pj_str("Route");
    char route_buf[] = "<sip:proxy.invalid;lr>";
    pjsip_route_hdr route_set, *route, *ack_route;
    pjsip_host_port via_addr;
    pj_dns_resolver *prev_res;
    pjsip_server_addresses addr;
    pj_str_t lo = pj_str("127.0.0.1");
    pjsip_dialog *dlg;
    pjsip_tx_data *tdata;
    pj_pool_t *pool;
    pj_uint32_t dlg_set_cnt;
    pj_bool_t route_shared, via_shared;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  ACK outliving its dialog during resolution"));

    PJ_TEST_SUCCESS(init_ua_layer(NULL), NULL, return -200);

    pool = pjsip_endpt_create_pool(endpt, "acktest", 1000, 1000);
    PJ_TEST_NOT_NULL(pool, NULL, return -210);

    /* Detach the endpoint's resolver, so the external one doesn't destroy
     * it, and put it back at the end.
     */
    prev_res = pjsip_endpt_get_resolver(endpt);
    if (prev_res)
        pjsip_endpt_set_resolver(endpt, NULL);

    pj_bzero(&held, sizeof(held));
    PJ_TEST_SUCCESS(pjsip_endpt_set_ext_resolver(endpt, &ext), NULL,
                    {rc = -220; goto on_return;});

    dlg_set_cnt = pjsip_ua_get_dlg_set_count();
    PJ_TEST_SUCCESS(pjsip_dlg_create_uac(pjsip_ua_instance(), &local, &local,
                                         &remote, &remote, &dlg),
                    NULL, {rc = -230; goto on_return;});
    /* Without a session, any unlock would destroy the dialog: hold it */
    pjsip_dlg_inc_lock(dlg);
    PJ_TEST_NOT_NULL(pj_pool_alloc(dlg->pool, 128 * 1024), NULL,
                     {pjsip_dlg_dec_lock(dlg); rc = -240; goto on_return;});

    /* A loose route to a host name, so that the ACK has to be resolved */
    route = (pjsip_route_hdr*)
            pjsip_parse_hdr(pool, &route_name, route_buf,
                            sizeof(route_buf) - 1, NULL);
    PJ_TEST_NOT_NULL(route, NULL,
                     {pjsip_dlg_dec_lock(dlg); rc = -250; goto on_return;});
    pj_list_init(&route_set);
    pj_list_push_back(&route_set, route);
    PJ_TEST_SUCCESS(pjsip_dlg_set_route_set(dlg, &route_set), NULL,
                    {pjsip_dlg_dec_lock(dlg); rc = -260; goto on_return;});

    /* A Via sent-by, as pjsua sets for its calls */
    via_addr.host = pj_str("192.0.2.1");
    via_addr.port = 5060;
    PJ_TEST_SUCCESS(pjsip_dlg_set_via_sent_by(dlg, &via_addr, NULL), NULL,
                    {pjsip_dlg_dec_lock(dlg); rc = -265; goto on_return;});

    PJ_TEST_SUCCESS(pjsip_dlg_create_request(dlg, &pjsip_ack_method, 1,
                                             &tdata),
                    NULL, {pjsip_dlg_dec_lock(dlg); rc = -270;
                           goto on_return;});
    ack_route = (pjsip_route_hdr*)
                pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, NULL);
    route_shared = !ack_route || ack_route->name_addr.uri ==
                   ((pjsip_route_hdr*)dlg->route_set.next)->name_addr.uri;

    PJ_TEST_SUCCESS(pjsip_dlg_send_request(dlg, tdata, -1, NULL), NULL,
                    {pjsip_dlg_dec_lock(dlg); rc = -280; goto on_return;});
    PJ_TEST_TRUE(held.started, "the ACK was not resolved asynchronously",
                 {pjsip_dlg_dec_lock(dlg); rc = -290; goto on_return;});
    /* The ACK is kept by the pending resolution */
    via_shared = tdata->via_addr.host.ptr == dlg->via_addr.host.ptr;

    /* From here on the held query is always answered, to free the ACK */
    PJ_TEST_EQ(dlg->tsx_count, 0, "the ACK holds a transaction", rc = -300);

    /* Nothing else holds the dialog: releasing the lock destroys it */
    pjsip_dlg_dec_lock(dlg);
    PJ_TEST_EQ(pjsip_ua_get_dlg_set_count(), dlg_set_cnt,
               "the dialog was not destroyed", rc = -310);

    /* The answer arrives after the dialog is gone and the ACK is sent */
    pj_bzero(&addr, sizeof(addr));
    addr.count = 1;
    addr.entry[0].type = PJSIP_TRANSPORT_UDP;
    addr.entry[0].name = lo;
    pj_sockaddr_init(pj_AF_INET(), &addr.entry[0].addr, &lo, 9);
    addr.entry[0].addr_len = pj_sockaddr_get_len(&addr.entry[0].addr);
    (*held.cb)(PJ_SUCCESS, held.token, &addr);
    flush_events(100);

    PJ_TEST_TRUE(!route_shared, "the ACK's Route points into the dialog",
                 rc = -320);
    PJ_TEST_TRUE(!via_shared, "the ACK's Via points into the dialog",
                 rc = -330);

on_return:
    pjsip_endpt_set_ext_resolver(endpt, NULL);
    if (prev_res)
        pjsip_endpt_set_resolver(endpt, prev_res);
    pjsip_endpt_release_pool(endpt, pool);
    return rc;
}

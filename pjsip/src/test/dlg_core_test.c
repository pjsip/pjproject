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

/* What the test sees of the ACK and its dialog */
static struct {
    pjsip_dialog            *dlg;
    pj_bool_t                destroyed;
    pj_bool_t                ack_sent;
    pj_bool_t                dlg_alive_at_tx;
} ack_state;

static void on_dlg_destroy(void *arg)
{
    PJ_UNUSED_ARG(arg);
    ack_state.destroyed = PJ_TRUE;
}

static pj_status_t ack_on_tx_request(pjsip_tx_data *tdata)
{
    if (tdata->msg->line.req.method.id == PJSIP_ACK_METHOD &&
        pjsip_tdata_get_dlg(tdata) == ack_state.dlg)
    {
        ack_state.ack_sent = PJ_TRUE;
        ack_state.dlg_alive_at_tx = !ack_state.destroyed;
    }
    return PJ_SUCCESS;
}

static pjsip_module mod_ack_tx =
{
    NULL, NULL,                         /* prev, next.          */
    { "mod-ack-tx", 10 },               /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority            */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    NULL,                               /* on_rx_request()      */
    NULL,                               /* on_rx_response()     */
    &ack_on_tx_request,                 /* on_tx_request()      */
    NULL,                               /* on_tx_response()     */
    NULL,                               /* on_tsx_state()       */
};

/* How the held query is answered */
enum ack_answer
{
    ACK_RESOLVED,           /* the ACK is sent */
    ACK_UNRESOLVED,         /* the ACK fails */
    ACK_SECOND_ADDR,        /* the first address fails, the second is used */
    ACK_ANSWER_CNT
};

/* Send an ACK, end its dialog, then answer the query. Error codes are
 * offset by 200 for each kind of answer.
 */
static int ack_resolve_case(pj_pool_t *pool, enum ack_answer answer)
{
    pj_str_t local = pj_str("sip:alice@127.0.0.1");
    pj_str_t remote = pj_str("sip:bob@127.0.0.1");
    pj_str_t route_name = pj_str("Route");
    char route_buf[] = "<sip:proxy.invalid;lr>";
    pjsip_route_hdr route_set, *route;
    pjsip_host_port via_addr;
    pjsip_server_addresses addr;
    pj_str_t lo = pj_str("127.0.0.1");
    pjsip_dialog *dlg;
    pjsip_tx_data *tdata;
    pj_uint32_t dlg_set_cnt;
    unsigned i;
    int off = 200 * (int)answer;
    int rc = 0;

    pj_bzero(&held, sizeof(held));
    pj_bzero(&ack_state, sizeof(ack_state));

    dlg_set_cnt = pjsip_ua_get_dlg_set_count();
    PJ_TEST_SUCCESS(pjsip_dlg_create_uac(pjsip_ua_instance(), &local, &local,
                                         &remote, &remote, &dlg),
                    NULL, return -230 - off);
    /* Without a session, any unlock would destroy the dialog: hold it */
    pjsip_dlg_inc_lock(dlg);
    ack_state.dlg = dlg;
    PJ_TEST_SUCCESS(pj_grp_lock_add_handler(dlg->grp_lock_, pool, NULL,
                                            &on_dlg_destroy),
                    NULL, {pjsip_dlg_dec_lock(dlg); return -235 - off;});
    PJ_TEST_NOT_NULL(pj_pool_alloc(dlg->pool, 128 * 1024), NULL,
                     {rc = -240 - off; goto on_error;});

    /* A loose route to a host name, so that the ACK has to be resolved */
    route = (pjsip_route_hdr*)
            pjsip_parse_hdr(pool, &route_name, route_buf,
                            sizeof(route_buf) - 1, NULL);
    PJ_TEST_NOT_NULL(route, NULL, {rc = -250 - off; goto on_error;});
    pj_list_init(&route_set);
    pj_list_push_back(&route_set, route);
    PJ_TEST_SUCCESS(pjsip_dlg_set_route_set(dlg, &route_set), NULL,
                    {rc = -260 - off; goto on_error;});

    /* A Via sent-by, as pjsua sets for its calls */
    via_addr.host = pj_str("192.0.2.1");
    via_addr.port = 5060;
    PJ_TEST_SUCCESS(pjsip_dlg_set_via_sent_by(dlg, &via_addr, NULL), NULL,
                    {rc = -265 - off; goto on_error;});

    PJ_TEST_SUCCESS(pjsip_dlg_create_request(dlg, &pjsip_ack_method, 1,
                                             &tdata),
                    NULL, {rc = -270 - off; goto on_error;});
    PJ_TEST_SUCCESS(pjsip_dlg_send_request(dlg, tdata, -1, NULL), NULL,
                    {rc = -280 - off; goto on_error;});
    PJ_TEST_TRUE(held.started, "the ACK was not resolved asynchronously",
                 {rc = -290 - off; goto on_error;});

    /* From here on the held query is always answered, to free the ACK */
    PJ_TEST_EQ(dlg->tsx_count, 0, "the ACK holds a transaction",
               rc = -300 - off);

    /* Releasing the lock ends the dialog as before... */
    pjsip_dlg_dec_lock(dlg);
    PJ_TEST_EQ(pjsip_ua_get_dlg_set_count(), dlg_set_cnt,
               "the dialog was not unregistered", rc = -310 - off);
    /* ...but the ACK keeps its memory */
    PJ_TEST_TRUE(!ack_state.destroyed,
                 "the dialog was destroyed before its ACK was done",
                 rc = -315 - off);

    /* The answer arrives after the dialog has ended */
    pj_bzero(&addr, sizeof(addr));
    if (answer == ACK_SECOND_ADDR) {
        /* No UDP6 transport, so the first address fails */
        addr.entry[addr.count].type = PJSIP_TRANSPORT_UDP6;
        addr.entry[addr.count].name = pj_str("::1");
        pj_sockaddr_init(pj_AF_INET6(), &addr.entry[addr.count].addr,
                         &addr.entry[addr.count].name, 9);
        addr.entry[addr.count].addr_len =
            pj_sockaddr_get_len(&addr.entry[addr.count].addr);
        ++addr.count;
    }
    addr.entry[addr.count].type = PJSIP_TRANSPORT_UDP;
    addr.entry[addr.count].name = lo;
    pj_sockaddr_init(pj_AF_INET(), &addr.entry[addr.count].addr, &lo, 9);
    addr.entry[addr.count].addr_len =
        pj_sockaddr_get_len(&addr.entry[addr.count].addr);
    ++addr.count;

    if (answer == ACK_UNRESOLVED)
        (*held.cb)(PJ_ERESOLVE, held.token, NULL);
    else
        (*held.cb)(PJ_SUCCESS, held.token, &addr);
    for (i = 0; i < 50 && !ack_state.destroyed; ++i)
        flush_events(20);

    if (answer == ACK_UNRESOLVED) {
        PJ_TEST_TRUE(!ack_state.ack_sent, "an unresolved ACK was sent",
                     rc = -320 - off);
    } else {
        PJ_TEST_TRUE(ack_state.ack_sent, "the ACK was not sent",
                     rc = -320 - off);
        PJ_TEST_TRUE(ack_state.dlg_alive_at_tx,
                     "the ACK was sent with its dialog destroyed",
                     rc = -325 - off);
    }
    PJ_TEST_TRUE(ack_state.destroyed,
                 "the ACK did not release its dialog", rc = -330 - off);

    /* A dialog that is still alive keeps the handler from the pool */
    if (!ack_state.destroyed)
        pj_grp_lock_del_handler(dlg->grp_lock_, NULL, &on_dlg_destroy);
    return rc;

on_error:
    pj_grp_lock_del_handler(dlg->grp_lock_, NULL, &on_dlg_destroy);
    pjsip_dlg_dec_lock(dlg);
    return rc;
}

/*
 * An ACK for a 2xx has no transaction, so it may still wait for its
 * destination to be resolved when the dialog is released. The dialog,
 * whose memory the ACK uses, must live until the ACK is sent or fails.
 * ASan also reports the access. As in inv_dec_ref_unlocked_test(), the
 * dialog pool is grown so that its release is a real free.
 */
int dlg_ack_resolve_test(void)
{
    static pjsip_ext_resolver ext = { &hold_resolve };
    pj_str_t lo = pj_str("127.0.0.1");
    pj_dns_resolver *prev_res;
    pj_sockaddr_in tp_addr;
    pjsip_transport *tp = NULL;
    pj_pool_t *pool;
    pj_bool_t mod_registered = PJ_FALSE;
    int answer;
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

    PJ_TEST_SUCCESS(pjsip_endpt_set_ext_resolver(endpt, &ext), NULL,
                    {rc = -220; goto on_return;});

    /* The ACK is sent over UDP: don't depend on other tests for it */
    pj_sockaddr_in_init(&tp_addr, &lo, 0);
    PJ_TEST_SUCCESS(pjsip_udp_transport_start(endpt, &tp_addr, NULL, 1, &tp),
                    NULL, {rc = -222; goto on_return;});
    PJ_TEST_SUCCESS(pjsip_endpt_register_module(endpt, &mod_ack_tx), NULL,
                    {rc = -224; goto on_return;});
    mod_registered = PJ_TRUE;

    for (answer = 0; answer < ACK_ANSWER_CNT && rc == 0; ++answer)
        rc = ack_resolve_case(pool, (enum ack_answer)answer);

on_return:
    if (mod_registered)
        pjsip_endpt_unregister_module(endpt, &mod_ack_tx);
    if (tp)
        pjsip_transport_shutdown(tp);
    pjsip_endpt_set_ext_resolver(endpt, NULL);
    if (prev_res)
        pjsip_endpt_set_resolver(endpt, prev_res);
    pjsip_endpt_release_pool(endpt, pool);
    return rc;
}

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
#include <pjmedia.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>

/*
 * INVITE usage (pjsip-ua/sip_inv.c) fuzz target.
 *
 * Drives whole call lifetimes through the loop transport: a UAC dialog is
 * created from fuzz URIs, pjsip_inv_create_uac() is given a fuzz SDP offer
 * and the INVITE is sent. A stub resolver maps every target to the loop
 * transport so the request is fed back into this same endpoint, where an
 * application module creates the UAS leg and answers with a fuzz-selected
 * code. Responses loop back to the UAC, so offer/answer negotiation,
 * provisional responses with 100rel, session timer negotiation,
 * re-INVITE/UPDATE and BYE teardown all run inside pjsip_inv_usage on
 * attacker-shaped input. Every untrusted string (URIs, SDP bytes) comes
 * from the fuzzer, mirroring what a malicious remote peer controls on the
 * wire.
 *
 * Input layout:
 *   data[0]        control byte:
 *                    bit0: PJSIP_INV_SUPPORT_100REL
 *                    bit1: PJSIP_INV_REQUIRE_100REL
 *                    bit2: PJSIP_INV_SUPPORT_TIMER | ALWAYS_USE_TIMER
 *                    bit3: PJSIP_INV_SUPPORT_UPDATE
 *                    bits4-6: UAS answer selector {200, 183, 486, 488, 500}
 *                    bit7: mid-call re-INVITE (UPDATE if supported)
 *   data[1..]      NUL-separated fields: remote URI (<=50 bytes),
 *                  target URI (<=50 bytes), SDP offer text (remainder).
 *                  Inputs without NULs fall back to equal thirds.
 */

pjsip_endpoint *endpt;
pj_caching_pool caching_pool;
pjsip_transport *loop_transport;

#define POOL_SIZE           8000
#define INV_TEST_MEM_SIZE   (2*1024*1024)
#define MAX_PUMPS           400     /* x 1ms cap, bounded */
#define MAX_JOBS            8

/* Fixed, valid SDP used for UAS answers and app-created offers. */
static const char *str_answer_sdp =
    "v=0\r\n"
    "o=- 3414953978 3414953978 IN IP4 127.0.0.1\r\n"
    "s=inv-fuzz\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=audio 4000 RTP/AVP 0\r\n"
    "a=rtpmap:0 PCMU/8000\r\n";

/* Parsed once in LLVMFuzzerInitialize(); callees clone as needed. */
static pj_pool_t *g_sdp_pool;
static pjmedia_sdp_session *g_answer_sdp;

typedef enum job_type {
    ESTABLISH_CALL,     /* UAS: answer 200 to complete the call   */
    MID_CALL_OFFER,     /* UAC: re-INVITE/UPDATE, offerless       */
    HANGUP              /* UAS: end session (BYE)                 */
} job_type;

/* Jobs carry the leg pointers of the iteration that queued them, so a job
 * always acts on its own call even if the globals have moved on.  The
 * sessions are kept alive by references held for the whole run. */
typedef struct job_t {
    job_type           type;
    pjsip_inv_session *uac;
    pjsip_inv_session *uas;
} job_t;

/* Per-iteration state, reset on every run.  With a receive delay set, the
 * loop transport delivers every message from its worker thread, so the
 * callbacks run there too and every touch of this state is guarded by
 * g_lock.  The two sessions are additionally kept alive by references taken
 * for the whole run, so the pointers stay valid across threads. */
static pjsip_inv_session *g_uac, *g_uas;
static pj_bool_t         g_done;
static unsigned          g_answer_code;
static pj_bool_t         g_answer_183;
static pj_bool_t         g_do_mid_call;
static job_t             g_jobs[MAX_JOBS];
static unsigned          g_job_cnt;
static pj_mutex_t       *g_lock;

/* Stub resolver: map every target to loopback on the loop transport so the
 * fuzzer never blocks in getaddrinfo() and INVITEs always reach the UAS.
 * The callback is invoked from a zero-delay timer rather than synchronously:
 * answering the INVITE inside this callback would run while the UAC
 * transaction is still in NULL state and abort in tsx_on_state_null() (see
 * the comment there in sip_transaction.c); the one-tick deferral lets the
 * transaction reach CALLING first, keeping everything on the harness
 * thread. */
typedef struct fuzz_resolve_data {
    pj_timer_entry           timer;
    pj_pool_t               *pool;
    pjsip_resolver_callback *cb;
    void                    *token;
    pjsip_server_addresses   addr;
} fuzz_resolve_data;

static void on_fuzz_resolve_timer(pj_timer_heap_t *timer_heap,
                                  pj_timer_entry *entry)
{
    fuzz_resolve_data *d = (fuzz_resolve_data*)entry->user_data;
    pj_pool_t *pool = d->pool;

    PJ_UNUSED_ARG(timer_heap);

    (*d->cb)(PJ_SUCCESS, d->token, &d->addr);

    pjsip_endpt_release_pool(endpt, pool);
}

static void fuzz_resolve(pjsip_resolver_t *resolver, pj_pool_t *pool,
                         const pjsip_host_info *target, void *token,
                         pjsip_resolver_callback *cb)
{
    fuzz_resolve_data *d;
    pj_pool_t *rpool;
    pj_time_val delay = {0, 1};

    PJ_UNUSED_ARG(resolver);
    PJ_UNUSED_ARG(pool);

    /* Per-resolve pool: released by the timer callback after the deferred
     * callback has run, so nothing accumulates across inputs. */
    rpool = pjsip_endpt_create_pool(endpt, "fuzzresolve", 512, 512);
    if (!rpool)
        return;

    d = (fuzz_resolve_data*)pj_pool_alloc(rpool, sizeof(*d));

    pj_bzero(&d->addr, sizeof(d->addr));
    d->addr.count = 1;
    d->addr.entry[0].name = target->addr.host;
    d->addr.entry[0].type = PJSIP_TRANSPORT_LOOP_DGRAM;
    pj_sockaddr_in_init(&d->addr.entry[0].addr.ipv4, NULL,
                        (pj_uint16_t)(target->addr.port ? target->addr.port
                                                        : 5060));
    d->addr.entry[0].addr.ipv4.sin_addr.s_addr = pj_htonl(0x7F000001);
    d->addr.entry[0].addr_len = sizeof(pj_sockaddr_in);

    /* entry[0].name points into the transaction's pool, which stays alive
     * while the transaction is in CALLING, i.e. past this callback. */
    d->pool  = rpool;
    d->cb    = cb;
    d->token = token;
    pj_timer_entry_init(&d->timer, 0, d, &on_fuzz_resolve_timer);
    pjsip_endpt_schedule_timer(endpt, &d->timer, &delay);
}

static pjsip_ext_resolver fuzz_ext_resolver = { &fuzz_resolve };

static char* alloc_str_from_data(pj_pool_t *pool, const uint8_t *data,
                                 size_t offset, size_t len)
{
    char *buf = (char*)pj_pool_alloc(pool, len + 1);
    pj_memcpy(buf, data + offset, len);
    buf[len] = '\0';
    return buf;
}

static pjmedia_sdp_session *parse_sdp(pj_pool_t *pool, const char *text,
                                      size_t len)
{
    pj_str_t dup;
    pjmedia_sdp_session *sdp = NULL;

    if (len == 0)
        return NULL;

    dup.ptr = (char*)pj_pool_alloc(pool, len + 1);
    pj_memcpy(dup.ptr, text, len);
    dup.slen = (pj_ssize_t)len;
    dup.ptr[len] = '\0';

    if (pjmedia_sdp_parse(pool, dup.ptr, dup.slen, &sdp) != PJ_SUCCESS)
        return NULL;

    return sdp;
}

/* Split payload into remote URI, target URI, and SDP. Prefer NUL separators
 * (seed layout); fall back to equal thirds for mutated inputs without NULs. */
static void split_fields(pj_pool_t *pool, const uint8_t *data, size_t size,
                         pj_str_t *remote, pj_str_t *target,
                         const uint8_t **sdp_ptr, size_t *sdp_len)
{
    const uint8_t *n1, *n2;
    size_t len1, len2, len3;

    n1 = size ? (const uint8_t*)memchr(data, 0, size) : NULL;
    if (n1) {
        len1 = (size_t)(n1 - data);
        if (len1 > 50)
            len1 = 50;
        n2 = (n1 + 1 < data + size)
           ? (const uint8_t*)memchr(n1 + 1, 0, (size_t)(data + size - (n1 + 1)))
           : NULL;
        if (n2) {
            len2 = (size_t)(n2 - (n1 + 1));
            if (len2 > 50)
                len2 = 50;
            *sdp_ptr = n2 + 1;
            *sdp_len = (size_t)(data + size - (n2 + 1));
        } else {
            len2 = (size_t)(data + size - (n1 + 1));
            if (len2 > 50)
                len2 = 50;
            *sdp_ptr = n1 + 1 + len2;
            *sdp_len = (size_t)(data + size - (n1 + 1 + len2));
        }
        remote->ptr = alloc_str_from_data(pool, data, 0, len1);
        remote->slen = (pj_ssize_t)len1;
        target->ptr = alloc_str_from_data(pool, n1 + 1, 0, len2);
        target->slen = (pj_ssize_t)len2;
        return;
    }

    len1 = size / 3;
    len2 = size / 3;
    if (len1 > 50) len1 = 50;
    if (len2 > 50) len2 = 50;
    len3 = size - len1 - len2;

    remote->ptr = alloc_str_from_data(pool, data, 0, len1);
    remote->slen = (pj_ssize_t)len1;
    target->ptr = alloc_str_from_data(pool, data, len1, len2);
    target->slen = (pj_ssize_t)len2;
    *sdp_ptr = data + len1 + len2;
    *sdp_len = len3;
}

/* g_lock must be held. */
static void enqueue_job_locked(job_type type, pjsip_inv_session *uac,
                               pjsip_inv_session *uas)
{
    if (g_job_cnt >= MAX_JOBS)
        return;
    g_jobs[g_job_cnt].type = type;
    g_jobs[g_job_cnt].uac  = uac;
    g_jobs[g_job_cnt].uas  = uas;
    g_job_cnt++;
}

/**************** INVITE SESSION CALLBACKS ******************/

static void on_media_update(pjsip_inv_session *inv_ses, pj_status_t status)
{
    PJ_UNUSED_ARG(inv_ses);
    PJ_UNUSED_ARG(status);
}

static void on_rx_offer(pjsip_inv_session *inv,
                        const pjmedia_sdp_session *offer)
{
    PJ_UNUSED_ARG(offer);
    /* Always answer with the fixed valid SDP. */
    pjsip_inv_set_sdp_answer(inv, g_answer_sdp);
}

static void on_create_offer(pjsip_inv_session *inv,
                            pjmedia_sdp_session **p_offer)
{
    PJ_UNUSED_ARG(inv);
    *p_offer = g_answer_sdp;
}

static void on_new_session(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}

static void on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);

    /* This runs on the loop transport's worker thread now that deliveries
     * are delayed, hence the lock around the shared state. */
    pj_mutex_lock(g_lock);

    /* Only the current iteration's UAC may mark the pump done, so a late
     * disconnect from a prior iteration cannot truncate this run. */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
        if (inv == g_uac)
            g_done = PJ_TRUE;
        pj_mutex_unlock(g_lock);
        return;
    }

    if (inv->state == PJSIP_INV_STATE_CONFIRMED && inv == g_uac) {
        /* Mid-call surface is queued from here and drained from the event
         * loop, never re-entrantly inside the callback. */
        if (g_do_mid_call)
            enqueue_job_locked(MID_CALL_OFFER, inv, g_uas);
        enqueue_job_locked(HANGUP, inv, g_uas);
    }

    pj_mutex_unlock(g_lock);
}

/**************** MODULE TO RECEIVE INITIAL INVITE ******************/

static pj_bool_t on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_dialog *dlg;
    pjsip_inv_session *uac;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata;
    pj_status_t status;
    unsigned options;
    unsigned answer_code;
    pj_bool_t answer_183;
    pj_bool_t known_call;

    if (rdata->msg_info.msg->type != PJSIP_REQUEST_MSG ||
        rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD)
    {
        return PJ_FALSE;
    }

    /* This runs on the loop transport's worker thread: snapshot the shared
     * state, then do the pjsip work without holding g_lock.  The callbacks
     * take g_lock while the stack holds its own locks, so the harness thread
     * must never call into pjsip with g_lock held. */
    pj_mutex_lock(g_lock);
    uac = g_uac;
    known_call = uac != NULL &&
                 pj_strcmp(&rdata->msg_info.call_id->id,
                           &uac->dlg->call_id->id) == 0;
    options = known_call ? uac->options : 0;
    answer_code = g_answer_code;
    answer_183 = g_answer_183;
    pj_mutex_unlock(g_lock);

    /* Only this run's UAC dialog may be served: a retransmission from a
     * call left over from an earlier run would otherwise replace g_uas and
     * take g_uac's options.  Reject anything else statelessly. */
    if (!known_call) {
        pjsip_endpt_respond_stateless(endpt, rdata,
                                      PJSIP_SC_CALL_TSX_DOES_NOT_EXIST,
                                      NULL, NULL, NULL);
        return PJ_TRUE;
    }

    /* Mirror the UAC options so feature paths pair up on both legs. */
    status = pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(), rdata,
                                               NULL, &dlg);
    if (status != PJ_SUCCESS) {
        /* Answer statelessly, or the UAC stays in CALLING retransmitting
         * the INVITE until Timer B. */
        pjsip_endpt_respond_stateless(endpt, rdata,
                                      PJSIP_SC_INTERNAL_SERVER_ERROR,
                                      NULL, NULL, NULL);
        return PJ_TRUE; /* handled: keep the loop transport flow clean */
    }

    /* Pass fixed answer SDP so the initial offer is negotiated; on_rx_offer
     * only fires for subsequent re-INVITE/UPDATE offers. */
    status = pjsip_inv_create_uas(dlg, rdata, g_answer_sdp, options, &inv);
    if (status != PJ_SUCCESS) {
        tdata = NULL;
        if (pjsip_dlg_create_response(dlg, rdata, 500, NULL, &tdata)
                == PJ_SUCCESS && tdata)
        {
            pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), tdata);
        }
        pjsip_dlg_dec_lock(dlg);
        return PJ_TRUE;
    }

    /* Hold a reference for the rest of the run: the globals may move on or
     * be reset while the worker thread is still inside this leg. */
    pjsip_inv_add_ref(inv);

    pj_mutex_lock(g_lock);
    g_uas = inv;
    pj_mutex_unlock(g_lock);
    pjsip_dlg_dec_lock(dlg);

    /* First response must use pjsip_inv_initial_answer() (final codes too).
     * The local answer SDP was already given to pjsip_inv_create_uas(), so
     * the 183 carries it from the negotiator; passing it again asserts in
     * process_answer(). */
    if (answer_183) {
        /* Reliable provisional path: 183 with SDP, then 200 via job. */
        status = pjsip_inv_initial_answer(inv, rdata, 183, NULL,
                                          NULL, &tdata);
        if (status == PJ_SUCCESS && tdata) {
            pjsip_inv_send_msg(inv, tdata);
            pj_mutex_lock(g_lock);
            enqueue_job_locked(ESTABLISH_CALL, NULL, inv);
            pj_mutex_unlock(g_lock);
        } else {
            /* Tell the UAC the call failed before terminating: without a
             * final response it stays in CALLING retransmitting the INVITE
             * until Timer B, which would bleed into later runs. */
            pjsip_tx_data *res = NULL;

            if (pjsip_inv_initial_answer(inv, rdata,
                                         PJSIP_SC_NOT_ACCEPTABLE_HERE,
                                         NULL, NULL, &res) == PJ_SUCCESS &&
                res)
            {
                pjsip_inv_send_msg(inv, res);
            }
            pjsip_inv_terminate(inv, PJSIP_SC_INTERNAL_SERVER_ERROR, PJ_FALSE);
            pj_mutex_lock(g_lock);
            if (g_uas == inv)
                g_uas = NULL;
            pj_mutex_unlock(g_lock);
        }
    } else {
        status = pjsip_inv_initial_answer(inv, rdata, answer_code, NULL,
                                          NULL, &tdata);
        if (status == PJ_SUCCESS && tdata) {
            pjsip_inv_send_msg(inv, tdata);
        } else {
            /* Respond with a final error the UAC cannot mistake for the
             * selected answer, then terminate: retrying with answer_code
             * would just re-send a 200 whose negotiation already failed. */
            pjsip_tx_data *res = NULL;

            if (pjsip_inv_initial_answer(inv, rdata,
                                         PJSIP_SC_NOT_ACCEPTABLE_HERE,
                                         NULL, NULL, &res) == PJ_SUCCESS &&
                res)
            {
                pjsip_inv_send_msg(inv, res);
            }
            pjsip_inv_terminate(inv, PJSIP_SC_INTERNAL_SERVER_ERROR, PJ_FALSE);
            pj_mutex_lock(g_lock);
            if (g_uas == inv)
                g_uas = NULL;
            pj_mutex_unlock(g_lock);
        }
    }

    return PJ_TRUE;
}

static pjsip_module mod_inv_fuzz =
{
    NULL, NULL,                     /* prev, next.              */
    { "mod-inv-fuzz", 13 },         /* Name.                    */
    -1,                             /* Id                       */
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority                 */
    NULL,                           /* load()                   */
    NULL,                           /* start()                  */
    NULL,                           /* stop()                   */
    NULL,                           /* unload()                 */
    &on_rx_request,                 /* on_rx_request()          */
    NULL,                           /* on_rx_response()         */
    NULL,                           /* on_tx_request.           */
    NULL,                           /* on_tx_response()         */
    NULL,                           /* on_tsx_state()           */
};

/**************** PER-ITERATION DRIVER ******************/

static void run_job(job_t *j)
{
    pjsip_tx_data *tdata = NULL;
    pj_status_t status;

    switch (j->type) {
    case ESTABLISH_CALL:
        if (!j->uas)
            return;
        status = pjsip_inv_answer(j->uas, 200, NULL, NULL, &tdata);
        if (status == PJ_SUCCESS && tdata)
            pjsip_inv_send_msg(j->uas, tdata);
        break;
    case MID_CALL_OFFER:
        if (!j->uac)
            return;
        /* Offerless re-INVITE/UPDATE: drives the UAS offer-creation and
         * application answer paths (on_rx_offer / on_create_offer). */
        if (j->uac->options & PJSIP_INV_SUPPORT_UPDATE)
            status = pjsip_inv_update(j->uac, NULL, NULL, &tdata);
        else
            status = pjsip_inv_reinvite(j->uac, NULL, NULL, &tdata);
        if (status == PJ_SUCCESS && tdata)
            pjsip_inv_send_msg(j->uac, tdata);
        break;
    case HANGUP:
        /* Re-checked here: the state can have moved on between the readiness
         * test and this call. */
        if (!j->uas || j->uas->state != PJSIP_INV_STATE_CONFIRMED)
            return; /* end_session(BYE) is only valid once confirmed */
        status = pjsip_inv_end_session(j->uas, PJSIP_SC_DECLINE, NULL, &tdata);
        if (status == PJ_SUCCESS && tdata)
            pjsip_inv_send_msg(j->uas, tdata);
        break;
    }
}

/* A queued job runs once its leg is in the right state: HANGUP in particular
 * stays queued until the ACK has reached the UAS (making that leg CONFIRMED)
 * and the UAC has no INVITE transaction pending, so the BYE cannot cut a
 * re-INVITE's offer/answer short. */
static pj_bool_t job_ready(const job_t *j)
{
    switch (j->type) {
    case ESTABLISH_CALL:
        return j->uas != NULL;
    case MID_CALL_OFFER:
        return j->uac != NULL;
    case HANGUP:
        return j->uas != NULL &&
               j->uas->state == PJSIP_INV_STATE_CONFIRMED &&
               (j->uac == NULL || j->uac->invite_tsx == NULL);
    }
    return PJ_TRUE;
}

static pj_bool_t both_legs_disconnected(void)
{
    pj_bool_t ret;

    pj_mutex_lock(g_lock);
    ret = (!g_uac || g_uac->state == PJSIP_INV_STATE_DISCONNECTED) &&
          (!g_uas || g_uas->state == PJSIP_INV_STATE_DISCONNECTED);
    pj_mutex_unlock(g_lock);

    return ret;
}

static void pump_events(unsigned max_pumps)
{
    unsigned i;

    for (i = 0; i < max_pumps; ++i) {
        job_t ready[MAX_JOBS];
        unsigned ready_cnt = 0;
        unsigned idx;
        pj_time_val delay = {0, 1};
        pj_bool_t stop;

        pjsip_endpt_handle_events(endpt, &delay);

        /* Snapshot the jobs whose legs are ready and run them without the
         * lock: the pjsip calls below take the stack's own locks, and the
         * worker thread takes g_lock from inside its callbacks. */
        pj_mutex_lock(g_lock);
        for (idx = 0; idx < g_job_cnt; ) {
            if (job_ready(&g_jobs[idx])) {
                ready[ready_cnt++] = g_jobs[idx];
                pj_array_erase(g_jobs, sizeof(g_jobs[0]), g_job_cnt, idx);
                g_job_cnt--;
            } else {
                idx++;
            }
        }
        stop = g_done;
        pj_mutex_unlock(g_lock);

        for (idx = 0; idx < ready_cnt; ++idx)
            run_job(&ready[idx]);

        /* Stop once both legs are disconnected: pumping further only
         * burns the iteration budget. */
        if (stop || both_legs_disconnected())
            return;
    }
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    pj_status_t status;
    pjsip_inv_callback inv_cb;

    PJ_UNUSED_ARG(argc);
    PJ_UNUSED_ARG(argv);

    status = pj_init();
    if (status != PJ_SUCCESS)
        return 1;

    pj_log_set_level(0);
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy,
                         INV_TEST_MEM_SIZE);

    status = pjsip_endpt_create(&caching_pool.factory, "inv-fuzzer", &endpt);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsip_tsx_layer_init_module(endpt);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsip_ua_init_module(endpt, NULL);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsip_100rel_init_module(endpt);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsip_timer_init_module(endpt);
    if (status != PJ_SUCCESS)
        return 1;

    pj_bzero(&inv_cb, sizeof(inv_cb));
    inv_cb.on_media_update = &on_media_update;
    inv_cb.on_rx_offer = &on_rx_offer;
    inv_cb.on_create_offer = &on_create_offer;
    inv_cb.on_state_changed = &on_state_changed;
    inv_cb.on_new_session = &on_new_session;

    status = pjsip_inv_usage_init(endpt, &inv_cb);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsip_loop_start(endpt, &loop_transport);
    if (status != PJ_SUCCESS)
        return 1;

    /* Hold a reference for the process lifetime, otherwise the transport is
     * destroyed by the idle timer once the last transaction using it is gone,
     * leaving loop_transport dangling (same as the sibling targets). */
    pjsip_transport_add_ref(loop_transport);

    /* Deliver every message from the loop transport's worker thread instead
     * of synchronously inside the sending call: a CANCEL reuses the INVITE's
     * already-resolved destination and would otherwise be delivered while
     * the send is still in progress, reaching the UAC's CANCEL transaction
     * while it is still in NULL state (asserts in tsx_on_state_null(), see
     * the comment there in sip_transaction.c).  The resolver deferral cannot
     * help there, because such a request never goes through resolution. */
    status = pjsip_loop_set_recv_delay(loop_transport, 1, NULL);
    if (status != PJ_SUCCESS)
        return 1;

    /* With the receive delay, deliveries (and hence the callbacks) come from
     * the worker thread, so all shared harness state is guarded by g_lock. */
    status = pj_mutex_create_simple(&caching_pool.factory, "inv-fuzz-lock",
                                    &g_lock);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsip_endpt_set_ext_resolver(endpt, &fuzz_ext_resolver);
    if (status != PJ_SUCCESS)
        return 1;

    status = pjsip_endpt_register_module(endpt, &mod_inv_fuzz);
    if (status != PJ_SUCCESS)
        return 1;

    g_sdp_pool = pjsip_endpt_create_pool(endpt, "inv-sdp", 1024, 1024);
    if (!g_sdp_pool)
        return 1;
    g_answer_sdp = parse_sdp(g_sdp_pool, str_answer_sdp, strlen(str_answer_sdp));
    if (!g_answer_sdp)
        return 1;

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    pj_pool_t *pool;
    pj_str_t local_uri, contact, remote_uri, target;
    pjsip_dialog *dlg = NULL;
    pjsip_inv_session *inv = NULL;
    pjsip_inv_session *uac;
    pjsip_inv_session *uas;
    pjmedia_sdp_session *offer = NULL;
    pjsip_tx_data *tdata = NULL;
    pj_status_t status;
    unsigned options;
    unsigned st;
    pj_bool_t need_teardown;
    pj_bool_t has_invite_tsx;
    pj_bool_t done;
    uint8_t ctl;
    const uint8_t *sdp_ptr;
    size_t sdp_len;

    if (Size < 4)
        return 0;

    pool = pjsip_endpt_create_pool(endpt, "inv-fuzz", POOL_SIZE, POOL_SIZE);
    if (!pool)
        return 0;

    ctl = Data[0];
    split_fields(pool, Data + 1, Size - 1, &remote_uri, &target,
                 &sdp_ptr, &sdp_len);

    local_uri = pj_str("sip:inv-fuzzer@127.0.0.1");
    contact = pj_str("sip:inv-fuzzer@127.0.0.1:5060");

    offer = parse_sdp(pool, (const char*)sdp_ptr, sdp_len);

    options = 0;
    if (ctl & 0x01) options |= PJSIP_INV_SUPPORT_100REL;
    if (ctl & 0x02) options |= PJSIP_INV_REQUIRE_100REL;
    if (ctl & 0x04) options |= PJSIP_INV_SUPPORT_TIMER |
                               PJSIP_INV_ALWAYS_USE_TIMER;
    if (ctl & 0x08) options |= PJSIP_INV_SUPPORT_UPDATE;

    {
        static const unsigned codes[5] = {
            PJSIP_SC_OK, 183, PJSIP_SC_BUSY_HERE,
            PJSIP_SC_NOT_ACCEPTABLE_HERE, PJSIP_SC_INTERNAL_SERVER_ERROR
        };
        /* bits 4-6 only; bit7 is mid-call and must not affect the code. */
        /* Reset while nothing of this iteration is being delivered yet; the
         * worker thread may still be draining strays from the previous run,
         * which are rejected by their call-id. */
        pj_mutex_lock(g_lock);
        g_answer_code = codes[((ctl >> 4) & 0x07) % 5];
        g_answer_183 = (g_answer_code == 183);
        g_do_mid_call = (ctl & 0x80) != 0;
        g_done = PJ_FALSE;
        g_uac = NULL;
        g_uas = NULL;
        g_job_cnt = 0;
        pj_mutex_unlock(g_lock);
    }

    status = pjsip_dlg_create_uac(pjsip_ua_instance(), &local_uri, &contact,
                                  &remote_uri, &target, &dlg);
    if (status == PJ_SUCCESS && dlg) {
        status = pjsip_inv_create_uac(dlg, offer, options, &inv);
        if (status != PJ_SUCCESS) {
            pjsip_dlg_terminate(dlg);
            dlg = NULL;
        } else if (inv) {
            /* Hold a reference for the rest of the run so the globals cannot
             * dangle while the worker thread is still delivering traffic for
             * this leg. */
            pjsip_inv_add_ref(inv);

            pj_mutex_lock(g_lock);
            g_uac = inv;
            pj_mutex_unlock(g_lock);

            status = pjsip_inv_invite(inv, &tdata);
            if (status != PJ_SUCCESS) {
                pjsip_inv_terminate(inv, PJSIP_SC_INTERNAL_SERVER_ERROR,
                                    PJ_FALSE);
                pj_mutex_lock(g_lock);
                if (g_uac == inv)
                    g_uac = NULL;
                pj_mutex_unlock(g_lock);
            } else if (tdata) {
                pjsip_inv_send_msg(inv, tdata);
                pump_events(MAX_PUMPS);
            }
        }
    }

    /* Force teardown so iterations do not accumulate live sessions. Only
     * states where end_session() is documented to work are used: BYE once
     * confirmed, CANCEL pre-confirm while the INVITE transaction still
     * exists; otherwise pending work is left to expire under pumping.  The
     * reads are snapshot under the lock, the calls run without it. */
    pj_mutex_lock(g_lock);
    uac = g_uac;
    need_teardown = uac != NULL && !g_done;
    st = need_teardown ? uac->state : 0;
    has_invite_tsx = need_teardown && uac->invite_tsx != NULL;
    pj_mutex_unlock(g_lock);

    if (need_teardown) {
        pjsip_tx_data *tbye = NULL;
        if (st == PJSIP_INV_STATE_CONFIRMED) {
            if (pjsip_inv_end_session(uac, PJSIP_SC_BUSY_EVERYWHERE, NULL,
                                      &tbye) == PJ_SUCCESS && tbye)
                pjsip_inv_send_msg(uac, tbye);
        } else if (has_invite_tsx) {
            if (pjsip_inv_end_session(uac, PJSIP_SC_REQUEST_TERMINATED,
                                      NULL, &tbye) == PJ_SUCCESS && tbye)
                pjsip_inv_send_msg(uac, tbye);
        }
    }
    pj_mutex_lock(g_lock);
    done = g_done;
    pj_mutex_unlock(g_lock);
    {
        unsigned i;
        pj_time_val delay = {0, 1};
        unsigned flush = done ? 5 : 20;
        for (i = 0; i < flush; ++i) {
            pjsip_endpt_handle_events(endpt, &delay);
            if (both_legs_disconnected())
                break;
        }
    }

    /* Drop the run's references last: releasing them may destroy the
     * sessions, which must not happen while a late delivery could still
     * reach the callbacks above. */
    pj_mutex_lock(g_lock);
    uac = g_uac;
    uas = g_uas;
    g_uac = NULL;
    g_uas = NULL;
    pj_mutex_unlock(g_lock);
    if (uac)
        pjsip_inv_dec_ref(uac);
    if (uas)
        pjsip_inv_dec_ref(uas);

    pjsip_endpt_release_pool(endpt, pool);

    return 0;
}

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
 * pjsua-level regression tests for the call media-count bounds checks that
 * prevent overflowing the fixed-size per-call media arrays
 * (pjsua_call.media[PJSUA_MAX_CALL_MEDIA]).
 *
 * Scenarios covered:
 *   1. Outgoing pjsua_call_make_call() rejects a call setting whose media
 *      count exceeds PJSUA_MAX_CALL_MEDIA (per-field and combined-sum),
 *      returning PJ_ETOOMANY, while a setting exactly at the limit is
 *      accepted.
 *   2. Incoming pjsua_call_answer2() rejects an over-limit setting with
 *      PJ_ETOOMANY on the first answer, then accepts a valid one.
 *   3. A re-INVITE that keeps existing (retained) m-lines and appends new
 *      media of a different type is rejected with PJ_ETOOMANY when the
 *      resulting count would overflow, even though the requested setting
 *      alone is within the limit (apply_call_setting() accepts it). This is
 *      the reinit/reoffer path in pjsua_media_channel_init().
 *
 * The over-limit settings are rejected before any media is instantiated, so
 * the tests use text media (txt_cnt) as the "second" media type: this keeps
 * the tests independent of whether video is compiled in, and no text media
 * is ever actually created (the settings are rejected first).
 */

#include "test.h"
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "pjsua_call_test.c"

/* SIP user for the loopback account. */
#define TEST_USER   "pjsua-call-test"


/* pjmedia globals (defined in endpoint.c; config.h only doc-comments them). */
extern pj_bool_t pjmedia_add_rtpmap_for_static_pt;
extern pj_bool_t pjmedia_add_bandwidth_tias_in_sdp;

/*****************************************************************************
 * Message size saver
 *
 * The tests in this file check what happens when you try to establish calls
 * with the maximum number of media allowed.
 * With the defaults that are currently in place,
 * this causes INVITE messages to become so large that they exceed
 * the default max packet length (PJSIP_MAX_PKT_LEN)
 * which causes wrong failures.
 * We tweak the library settings here to bring down the payload size
 * and restore the settings after the tests so we don't affect other tests.
 *****************************************************************************/
typedef struct msg_size_saved
{
    pj_bool_t        disable_tcp_switch;
    pj_bool_t        compact_form;
    pj_bool_t        rtpmap_static;
    pj_bool_t        bandw_tias;
    pj_bool_t        tel_event;
    pjsua_codec_info codecs[PJMEDIA_CODEC_MGR_MAX_CODECS];
    unsigned         codec_count;
} msg_size_saved;

/* Tweak library settings in order to bring down payload size */
static void minimize_msg_size(msg_size_saved *sv)
{
    pjmedia_endpt *endpt = pjsua_get_pjmedia_endpt();
    pj_bool_t no = PJ_FALSE;
    const pj_str_t all = pj_str("*");
    const pj_str_t pcmu = pj_str("PCMU/8000");

    /* get the current settings... */
    sv->disable_tcp_switch  = pjsip_cfg()->endpt.disable_tcp_switch;
    sv->compact_form        = pjsip_cfg()->endpt.use_compact_form;
    sv->rtpmap_static       = pjmedia_add_rtpmap_for_static_pt;
    sv->bandw_tias          = pjmedia_add_bandwidth_tias_in_sdp;
    pjmedia_endpt_get_flag(endpt, PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG,
                           &sv->tel_event);
    sv->codec_count = PJ_ARRAY_SIZE(sv->codecs);
    if (pjsua_enum_codecs(sv->codecs, &sv->codec_count) != PJ_SUCCESS)
        sv->codec_count = 0;

    /* apply settings */
    pjsip_cfg()->endpt.disable_tcp_switch   = PJ_TRUE;
    pjsip_cfg()->endpt.use_compact_form     = PJ_TRUE;
    pjmedia_add_rtpmap_for_static_pt        = PJ_FALSE;
    pjmedia_add_bandwidth_tias_in_sdp       = PJ_FALSE;
    pjmedia_endpt_set_flag(endpt, PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG, &no);
    /* codec wise, we'll disable everything and allow only PCMU */
    pjsua_codec_set_priority(&all,  PJMEDIA_CODEC_PRIO_DISABLED);
    pjsua_codec_set_priority(&pcmu, PJMEDIA_CODEC_PRIO_HIGHEST);
}

/* Restore original settings, so other tests are not affected */
static void restore_msg_size(const msg_size_saved *sv)
{
    pjmedia_endpt *endpt = pjsua_get_pjmedia_endpt();
    unsigned i;
    pjsip_cfg()->endpt.disable_tcp_switch   = sv->disable_tcp_switch;
    pjsip_cfg()->endpt.use_compact_form     = sv->compact_form;
    pjmedia_add_rtpmap_for_static_pt        = sv->rtpmap_static;
    pjmedia_add_bandwidth_tias_in_sdp       = sv->bandw_tias;
    pjmedia_endpt_set_flag(endpt, PJMEDIA_ENDPT_HAS_TELEPHONE_EVENT_FLAG,
                           &sv->tel_event);
    for (i = 0; i < sv->codec_count; ++i) {
        pjsua_codec_set_priority(&sv->codecs[i].codec_id,
                                 sv->codecs[i].priority);
    }
}

/*****************************************************************************
 * Shared test state
 *****************************************************************************/

static struct
{
    pjsua_acc_id  acc_id;
    char          self_uri[128];
    /* Result recorded by on_incoming_call when it probes answer2 with an
     * over-limit setting. Initialized to PJ_SUCCESS (an unexpected value)
     * so the main flow can detect the callback actually ran. */
    pj_status_t   answer2_overlimit_status;
    pj_bool_t     incoming_seen;
    /* Incoming (callee) leg of the most recent self-call. */
    pjsua_call_id incoming_call_id;
    /* When set, on_call_sdp_created appends an m=application line to the
     * local SDP offer, so the peer establishes a call holding a media slot
     * whose type is neither audio, video nor text. */
    pj_bool_t     inject_app_mline;
} g_ctx;


/*****************************************************************************
 * Callbacks
 *****************************************************************************/

/* Auto-answer incoming calls. First probe that an over-limit setting is
 * rejected by answer2() (recorded for the main flow to assert), then answer
 * with a valid single-audio setting to establish the call.
 */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    pjsua_call_setting opt;
    pj_status_t status;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    g_ctx.incoming_call_id = call_id;

    /* Probe: over-limit audio count must be rejected. answer2() leaves
     * call->opt_inited FALSE on failure, so the valid answer below still
     * applies.
     */
    pjsua_call_setting_default(&opt);
    opt.aud_cnt = PJSUA_MAX_CALL_MEDIA + 1;
    opt.vid_cnt = 0;
    opt.txt_cnt = 0;
    g_ctx.answer2_overlimit_status =
        pjsua_call_answer2(call_id, &opt, 200, NULL, NULL);

    /* Establish the call with a valid single-audio setting. */
    pjsua_call_setting_default(&opt);
    opt.aud_cnt = 1;
    opt.vid_cnt = 0;
    opt.txt_cnt = 0;
    status = pjsua_call_answer2(call_id, &opt, 200, NULL, NULL);
    if (status != PJ_SUCCESS)
        PJ_PERROR(1, (THIS_FILE, status, "  valid answer2 failed"));

    g_ctx.incoming_seen = PJ_TRUE;
}

/* Optionally append an m=application line to a locally-created SDP offer.
 * PJSUA syncs med_prov_cnt up to the callback's SDP media count, and the
 * remote peer that answers this offer keeps the extra m-line as a disabled
 * media slot of non-audio/video/text type. Only offers (rem_sdp == NULL) are
 * modified; answers are left untouched so negotiation succeeds.
 */
static void on_call_sdp_created(pjsua_call_id call_id,
                                pjmedia_sdp_session *sdp,
                                pj_pool_t *pool,
                                const pjmedia_sdp_session *rem_sdp)
{
    pjmedia_sdp_media *m;

    PJ_UNUSED_ARG(call_id);

    if (!g_ctx.inject_app_mline || rem_sdp != NULL)
        return;
    if (sdp->media_count >= PJMEDIA_MAX_SDP_MEDIA)
        return;

    m = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_media);
    pj_strdup2(pool, &m->desc.media, "application");
    m->desc.port = 4000;
    pj_strdup2(pool, &m->desc.transport, "RTP/AVP");
    m->desc.fmt_count = 1;
    pj_strdup2(pool, &m->desc.fmt[0], "0");

    /* A connection line is required by SDP validation. Reuse the one from an
     * existing media (or the session), whichever is present.
     */
    if (sdp->media_count > 0 && sdp->media[0]->conn)
        m->conn = pjmedia_sdp_conn_clone(pool, sdp->media[0]->conn);
    else if (sdp->conn)
        m->conn = pjmedia_sdp_conn_clone(pool, sdp->conn);

    sdp->media[sdp->media_count++] = m;
}


/*****************************************************************************
 * Helpers
 *****************************************************************************/

/* Pump the pjsua event loop until predicate() returns non-zero or
 * timeout_ms of wall-clock time has elapsed. Returns PJ_TRUE if the
 * predicate was satisfied. A NULL predicate simply pumps for the whole
 * timeout. Elapsed time is measured (not derived from the poll timeout,
 * which is only an upper bound), so the wait is robust on slow CI
 * runners and in busy periods where polls return early.
 */
static pj_bool_t wait_until(pj_bool_t (*predicate)(pjsua_call_id),
                            pjsua_call_id call_id, unsigned timeout_ms)
{
    pj_time_val t0, now;

    pj_gettimeofday(&t0);
    for (;;) {
        if (predicate && predicate(call_id))
            return PJ_TRUE;
        pj_gettimeofday(&now);
        PJ_TIME_VAL_SUB(now, t0);
        if ((unsigned)PJ_TIME_VAL_MSEC(now) >= timeout_ms)
            break;
        pjsua_handle_events(50);
    }
    return predicate ? predicate(call_id) : PJ_FALSE;
}

static pj_bool_t call_is_confirmed(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    if (pjsua_call_get_info(call_id, &ci) != PJ_SUCCESS)
        return PJ_FALSE;
    return ci.state == PJSIP_INV_STATE_CONFIRMED;
}

/* Tear down every call and pump the event loop until none remain (or a
 * generous cap elapses). Re-issues hangup each round so a leg that only
 * materializes after the first hangup_all() (e.g. an auto-answered incoming
 * leg whose INVITE had not been polled yet) is also cleaned up.
 */
static void drain_all_calls(void)
{
    unsigned i;

    for (i = 0; i < 60 && pjsua_call_get_count() > 0; ++i) {
        pjsua_call_hangup_all();
        pjsua_handle_events(50);
    }

    /* Closed media sockets keep their ioqueue slot for
     * PJ_IOQUEUE_KEY_FREE_DELAY msec (safe unregistration), and a max-media
     * loopback call uses 2 legs x PJSUA_MAX_CALL_MEDIA x 2 (RTP+RTCP) sockets
     * which equal the media ioqueue's whole capacity (PJ_IOQUEUE_MAX_HANDLES).
     * Wait out the delay so the slots are
     * recycled before the next sub-test creates new media sockets.
     */
    wait_until(NULL, PJSUA_INVALID_ID, PJ_IOQUEUE_KEY_FREE_DELAY + 200);
}


/*****************************************************************************
 * Sub-tests
 *****************************************************************************/

/* Outgoing make_call() bounds. The rejected settings return before any
 * dialog/media is created, so no call slot leaks (make_call cleans up on
 * error).
 */
static int test_make_call_bounds(void)
{
    pjsua_call_setting opt;
    pj_str_t uri = pj_str(g_ctx.self_uri);
    pj_status_t status;
    pjsua_call_id cid = PJSUA_INVALID_ID;

    PJ_LOG(3, (THIS_FILE, "  outgoing make_call bounds"));

    /* Per-field over limit -> PJ_ETOOMANY. */
    pjsua_call_setting_default(&opt);
    opt.aud_cnt = PJSUA_MAX_CALL_MEDIA + 1;
    opt.vid_cnt = 0;
    opt.txt_cnt = 0;
    status = pjsua_call_make_call(g_ctx.acc_id, &uri, &opt, NULL, NULL, &cid);
    if (status != PJ_ETOOMANY) {
        PJ_LOG(1, (THIS_FILE, "    expected PJ_ETOOMANY (aud over limit), "
                   "got %d", status));
        return -1100;
    }

    /* Combined sum over limit (aud == max, +1 text) -> PJ_ETOOMANY. */
    pjsua_call_setting_default(&opt);
    opt.aud_cnt = PJSUA_MAX_CALL_MEDIA;
    opt.vid_cnt = 0;
    opt.txt_cnt = 1;
    status = pjsua_call_make_call(g_ctx.acc_id, &uri, &opt, NULL, NULL, &cid);
    if (status != PJ_ETOOMANY) {
        PJ_LOG(1, (THIS_FILE, "    expected PJ_ETOOMANY (sum over limit), "
                   "got %d", status));
        return -1101;
    }

    /* Combined sum exactly at the limit is accepted (not rejected by the
     * bounds check). Tear the resulting call down immediately.
     */
    pjsua_call_setting_default(&opt);
    opt.aud_cnt = PJSUA_MAX_CALL_MEDIA;
    opt.vid_cnt = 0;
    opt.txt_cnt = 0;
    status = pjsua_call_make_call(g_ctx.acc_id, &uri, &opt, NULL, NULL, &cid);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "    boundary count (== max) failed (%d)",
                   status));
        return -1102;
    }
    if (status == PJ_SUCCESS && cid != PJSUA_INVALID_ID)
        pjsua_call_hangup(cid, 0, NULL, NULL);

    /* Tear down the boundary call and its (possibly late) answered peer. */
    drain_all_calls();

    return 0;
}

/* Establish a loopback call, verifying answer2() rejects an over-limit
 * setting, then exercise the reinit/reoffer overflow guard: a re-INVITE
 * that disables the audio m-line and requests a full set of a second media
 * type would retain the audio line and append the new ones, overflowing the
 * media array; it must be rejected with PJ_ETOOMANY without crashing.
 */
static int test_reinvite_reinit_bounds(void)
{
    pjsua_call_setting opt;
    pj_str_t uri = pj_str(g_ctx.self_uri);
    pj_status_t status;
    pjsua_call_id cid = PJSUA_INVALID_ID;
    pjsua_call_info ci;

    PJ_LOG(3, (THIS_FILE, "  answer2 bounds + reinit/reoffer overflow guard"));

    g_ctx.answer2_overlimit_status = PJ_SUCCESS;
    g_ctx.incoming_seen = PJ_FALSE;

    /* Place a single-audio call to ourselves; on_incoming_call answers it. */
    pjsua_call_setting_default(&opt);
    opt.aud_cnt = 1;
    opt.vid_cnt = 0;
    opt.txt_cnt = 0;
    status = pjsua_call_make_call(g_ctx.acc_id, &uri, &opt, NULL, NULL, &cid);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "    make_call failed (%d)", status));
        return -1200;
    }

    if (!wait_until(&call_is_confirmed, cid, 8000)) {
        PJ_LOG(1, (THIS_FILE, "    call did not reach CONFIRMED in time"));
        pjsua_call_hangup_all();
        return -1201;
    }

    /* answer2() must have rejected the over-limit probe. */
    if (!g_ctx.incoming_seen) {
        PJ_LOG(1, (THIS_FILE, "    on_incoming_call never ran"));
        pjsua_call_hangup_all();
        return -1202;
    }
    if (g_ctx.answer2_overlimit_status != PJ_ETOOMANY) {
        PJ_LOG(1, (THIS_FILE, "    answer2 over-limit expected PJ_ETOOMANY, "
                   "got %d", g_ctx.answer2_overlimit_status));
        pjsua_call_hangup_all();
        return -1203;
    }

    /* Re-INVITE: disable audio and request PJSUA_MAX_CALL_MEDIA text m-lines.
     * apply_call_setting() accepts it (sum == max), but the existing audio
     * m-line is retained (media count never decreases) so the effective
     * count would be max+1. The reinit guard must reject with PJ_ETOOMANY.
     */
    pjsua_call_setting_default(&opt);
    opt.aud_cnt = 0;
    opt.vid_cnt = 0;
    opt.txt_cnt = PJSUA_MAX_CALL_MEDIA;
    status = pjsua_call_reinvite2(cid, &opt, NULL);
    if (status != PJ_ETOOMANY) {
        PJ_LOG(1, (THIS_FILE, "    reinvite reinit-overflow expected "
                   "PJ_ETOOMANY, got %d", status));
        pjsua_call_hangup_all();
        return -1204;
    }

    /* The call must survive the rejected re-INVITE. */
    if (pjsua_call_get_info(cid, &ci) != PJ_SUCCESS ||
        ci.state != PJSIP_INV_STATE_CONFIRMED)
    {
        PJ_LOG(1, (THIS_FILE, "    call not alive after rejected reinvite"));
        pjsua_call_hangup_all();
        return -1205;
    }

    /* Clean up. */
    drain_all_calls();

    return 0;
}

/* Same reinit/reoffer overflow guard, but with an established call that holds
 * a media slot of non-audio/video/text type (an injected m=application line).
 * That slot counts toward the call's media total but is invisible to the
 * per-media-type totals, so a guard that only summed per-type counts would
 * under-count the existing media and still overflow. Here we keep the 1 audio
 * and request PJSUA_MAX_CALL_MEDIA-1 text m-lines: the per-type view is
 * 1 + (max-1) = max (looks acceptable), but the retained application m-line
 * makes the real count max+1, which must be rejected.
 */
static int test_reinit_bounds_untyped_mline(void)
{
    pjsua_call_setting opt;
    pj_str_t uri = pj_str(g_ctx.self_uri);
    pj_status_t status;
    pjsua_call_id cid = PJSUA_INVALID_ID;
    pjsua_call_info ci;

    PJ_LOG(3, (THIS_FILE, "  reinit guard with untyped (application) m-line"));

    g_ctx.inject_app_mline = PJ_TRUE;

    /* Offer 1 audio + 1 application m-line to ourselves. */
    pjsua_call_setting_default(&opt);
    opt.aud_cnt = 1;
    opt.vid_cnt = 0;
    opt.txt_cnt = 0;
    status = pjsua_call_make_call(g_ctx.acc_id, &uri, &opt, NULL, NULL, &cid);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "    make_call failed (%d)", status));
        g_ctx.inject_app_mline = PJ_FALSE;
        drain_all_calls();
        return -1300;
    }

    if (!wait_until(&call_is_confirmed, cid, 8000)) {
        PJ_LOG(1, (THIS_FILE, "    call did not reach CONFIRMED in time"));
        g_ctx.inject_app_mline = PJ_FALSE;
        drain_all_calls();
        return -1301;
    }

    /* Stop injecting; the reinvite below must not add further m-lines. */
    g_ctx.inject_app_mline = PJ_FALSE;

    /* Sanity/diagnostic: the call should now carry the extra m-line. The
     * assertion below is self-validating regardless (PJ_ETOOMANY is only
     * returned when the retained untyped slot pushes the count over max).
     */
    if (pjsua_call_get_info(cid, &ci) == PJ_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "    established with media_cnt=%u",
                   ci.media_cnt));
    }

    pjsua_call_setting_default(&opt);
    opt.aud_cnt = 1;
    opt.vid_cnt = 0;
    opt.txt_cnt = PJSUA_MAX_CALL_MEDIA - 1;
    status = pjsua_call_reinvite2(cid, &opt, NULL);
    if (status != PJ_ETOOMANY) {
        PJ_LOG(1, (THIS_FILE, "    reinit overflow with untyped m-line "
                   "expected PJ_ETOOMANY, got %d", status));
        drain_all_calls();
        return -1302;
    }

    if (pjsua_call_get_info(cid, &ci) != PJ_SUCCESS ||
        ci.state != PJSIP_INV_STATE_CONFIRMED)
    {
        PJ_LOG(1, (THIS_FILE, "    call not alive after rejected reinvite"));
        drain_all_calls();
        return -1303;
    }

    drain_all_calls();
    return 0;
}


/*****************************************************************************
 * Main entry point
 *****************************************************************************/
int pjsua_call_test(void)
{
    extern pjsip_endpoint *endpt;       /* test framework endpoint */
    extern pj_caching_pool  caching_pool;
    msg_size_saved          lib_settings;
    pjsua_config            ua_cfg;
    pjsua_logging_config    log_cfg;
    pjsua_media_config      media_cfg;
    pjsua_transport_config  tp_cfg;
    pjsua_transport_id      tp_id;
    pj_uint16_t             port;
    pj_status_t             status;
    int rc = 0;

    PJ_LOG(3, (THIS_FILE, "pjsua call media-count bounds test"));
    pj_bzero(&lib_settings, sizeof(lib_settings));

    /* pjsua creates its own endpoint and registers the (singleton) tsx layer
     * module; destroy the framework endpoint first to avoid the duplicate
     * registration assert, and recreate it afterwards. Same pattern as
     * pjsua_auth_test.c.
     */
    pjsip_endpt_destroy(endpt);
    endpt = NULL;

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_create failed (%d)", status));
        rc = -2001;
        goto on_restore;
    }

    pjsua_config_default(&ua_cfg);
    ua_cfg.cb.on_incoming_call = &on_incoming_call;
    ua_cfg.cb.on_call_sdp_created = &on_call_sdp_created;
    /* Single-threaded: event pumping happens on this thread, so callbacks
     * run here too and no locking races arise in the test itself. */
    ua_cfg.thread_cnt = 0;

    /* Make sure PRACK and session timer are off and don't bother with SRTP */
    ua_cfg.require_100rel   = PJSUA_100REL_NOT_USED;
    ua_cfg.use_timer        = PJSUA_SIP_TIMER_INACTIVE;
    ua_cfg.use_srtp         = PJMEDIA_SRTP_DISABLED;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.level         = 3;
    log_cfg.console_level = 3;

    pjsua_media_config_default(&media_cfg);
    media_cfg.no_vad        = PJ_TRUE;
    media_cfg.enable_ice    = PJ_FALSE;

    status = pjsua_init(&ua_cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_init failed (%d)", status));
        pjsua_destroy();
        rc = -2002;
        goto on_restore;
    }

    pjsua_transport_config_default(&tp_cfg);
    tp_cfg.port = 0;    /* ephemeral */
    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tp_cfg, &tp_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  transport_create failed (%d)", status));
        pjsua_destroy();
        rc = -2003;
        goto on_restore;
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  pjsua_start failed (%d)", status));
        pjsua_destroy();
        rc = -2004;
        goto on_restore;
    }

    /* Use a null (dummy) sound device so calls with audio can be set up on
     * hosts/CI without real audio hardware.
     */
    status = pjsua_set_null_snd_dev();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  set_null_snd_dev failed (%d)", status));
        pjsua_destroy();
        rc = -2007;
        goto on_restore;
    }

    /* Local account bound to the UDP transport, and a self URI to dial. */
    status = pjsua_acc_add_local(tp_id, PJ_TRUE, &g_ctx.acc_id);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "  acc_add_local failed (%d)", status));
        pjsua_destroy();
        rc = -2005;
        goto on_restore;
    }

    {
        pjsua_transport_info ti;
        status = pjsua_transport_get_info(tp_id, &ti);
        if (status != PJ_SUCCESS) {
            PJ_LOG(1, (THIS_FILE, "  transport_get_info failed (%d)", status));
            pjsua_destroy();
            rc = -2006;
            goto on_restore;
        }
        port = pj_sockaddr_get_port(&ti.local_addr);
    }
    pj_ansi_snprintf(g_ctx.self_uri, sizeof(g_ctx.self_uri),
                     "sip:%s@127.0.0.1:%u", TEST_USER, (unsigned)port);

    /* Adjust library settings to bring down the payload size before
     * running the sub-tests.
     */
    minimize_msg_size(&lib_settings);

    /* ---- Run sub-tests ---- */
    rc = test_make_call_bounds();
    if (rc != 0) goto on_return;

    rc = test_reinvite_reinit_bounds();
    if (rc != 0) goto on_return;

    rc = test_reinit_bounds_untyped_mline();
    if (rc != 0) goto on_return;

on_return:
    drain_all_calls();
    restore_msg_size(&lib_settings);
    pjsua_destroy2(PJSUA_DESTROY_NO_RX_MSG);

on_restore:
    /* Recreate the framework endpoint + tsx layer for subsequent tests. */
    status = pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error recreating endpoint"));
        return -1000;
    }
    status = pjsip_tsx_layer_init_module(endpt);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error re-initializing tsx layer"));
    }

    return rc;
}

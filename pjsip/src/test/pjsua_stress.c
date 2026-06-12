/*
 * pjsua_stress.c
 *
 * A self-contained PJSUA stress test that drives many concurrent calls and
 * random call operations to surface deadlocks, races, and memory issues
 * under TSan and ASan.
 *
 * One PJSUA instance is both caller and callee. Outgoing calls dial
 * sip:stress@127.0.0.1:<port>, the incoming-call callback auto-answers, and
 * each conversation produces two pjsua_call_id legs in the same process.
 *
 * Phases:
 *   1. Init: pjsua_create/init/start, null sound dev, single anonymous
 *      account, colorbar capture device for video.
 *   2. Ramp: keep issuing pjsua_call_make_call until the tracked-leg count
 *      reaches -n N (with up to -v V of those legs carrying video).
 *   3. Stress: spawn -t T worker threads. Each thread loops, picking a
 *      random op (hangup / make / conf-conn / conf-disc / hold / unhold)
 *      on a random tracked leg, until a deadline.
 *   4. Cleanup: hangup_all, wait for the call count to drain, pjsua_destroy.
 *
 * Usage:
 *   pjsua-stress [options]
 *     -n N            Max audio call legs   (default 64)
 *     -v V            Max video call legs   (default 0)
 *     -t T            Worker thread count   (default 4)
 *     --duration S    Stress phase seconds  (default 300)
 *     --port P        Local UDP SIP port    (default 5060)
 *     --self-uri URI  Outgoing dial target  (default sip:stress@127.0.0.1:<port>)
 *     --seed S        RNG seed              (default time-based)
 *     --log-level L   PJ console log level  (default 3)
 *     -h, --help      Show usage
 *
 * Note: PJSUA_MAX_CALLS, PJSUA_MAX_CONF_PORTS and PJ_IOQUEUE_MAX_HANDLES
 * must be sized in config_site.h to be at least N. See
 * pjlib/include/pj/config_site_stress.h.
 */

#include <pjsua-lib/pjsua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__APPLE__)
#  include <execinfo.h>
#  include <signal.h>
#  include <unistd.h>
#  define PJSUA_STRESS_HAS_BACKTRACE 1
#endif

#define THIS_FILE  "pjsua_stress"

/* Default CLI values. */
#define DEF_MAX_LEGS        64
#define DEF_MAX_VID_LEGS    0
#define DEF_WORKERS         4
#define DEF_DURATION_SEC    300
#define DEF_PORT            5060
#define DEF_LOG_LEVEL       3

/* Ramp phase will fail if it takes longer than this. Sized for 1024 call
 * legs at the ~7-leg/sec rate observed on the GitHub ASan runner, with
 * headroom for TSan's slower instrumentation. */
#define RAMP_TIMEOUT_SEC    300

/* How long to wait for pjsua_call_hangup_all() to drain before falling
 * through to pjsua_destroy(). Under stress some calls end up parked in
 * "Delaying BYE request until ACK is received" because their previous
 * re-INVITE never got an ACK; those won't drain on their own and waiting
 * is pointless — pjsua_destroy() force-tears-down the remainder. */
#define CLEANUP_WAIT_SEC    5

/* Stats print interval. */
#define STATS_INTERVAL_MS   5000

/* Operation IDs. */
enum {
    OP_HANGUP = 0,
    OP_MAKE_AUDIO,
    OP_MAKE_VIDEO,
    OP_CONF_CONN,
    OP_CONF_DISC,
    OP_HOLD,
    OP_UNHOLD,
    OP_COUNT
};

static const char *op_name(int op)
{
    static const char *names[] = {
        "hangup", "make_audio", "make_video", "conf_conn", "conf_disc",
        "hold", "unhold"
    };
    return (op >= 0 && op < OP_COUNT) ? names[op] : "?";
}

/* Tracked call leg. */
typedef struct leg_entry {
    pjsua_call_id   id;
    pj_bool_t       has_video;
} leg_entry_t;

/* CLI options. */
typedef struct stress_opts {
    int             max_legs;
    int             max_video_legs;
    int             worker_count;
    int             duration_sec;
    int             port;
    char            self_uri[256];
    unsigned        seed;
    int             log_level;
} stress_opts_t;

/* Worker thread context. */
typedef struct worker_ctx {
    int             id;
    pj_thread_t    *thread;
    unsigned        rng_state;
    pj_uint64_t     ops[OP_COUNT];
    pj_uint64_t     ok_ops[OP_COUNT];
} worker_ctx_t;

/* Global state. */
static struct {
    /* Own caching pool factory so pool/mutex outlive pjsua_destroy(). */
    pj_caching_pool     cp;
    pj_bool_t           cp_inited;
    pj_pool_t          *pool;
    pj_mutex_t         *mutex;          /* protects legs[], leg_count, video_leg_count */
    leg_entry_t        *legs;           /* size = max_legs */
    int                 leg_count;
    int                 video_leg_count;
    pjsua_acc_id        acc_id;
    pjmedia_vid_dev_index colorbar_dev;
    pjmedia_vid_dev_index null_rend_dev;
    stress_opts_t       opts;
    pj_atomic_t        *stop_flag;
    pj_thread_t        *stats_thread;
    worker_ctx_t       *workers;        /* size = worker_count */
    pj_timer_entry     *deferred_state_timers;  /* size = max_legs */
} g;

static pj_bool_t stop_flag_get(void)
{
    return g.stop_flag && pj_atomic_get(g.stop_flag) != 0;
}

static void stop_flag_set(pj_bool_t stop)
{
    if (g.stop_flag)
        pj_atomic_set(g.stop_flag, stop ? 1 : 0);
}


/*============================================================================
 * Registry helpers
 *==========================================================================*/

/* Add a leg. Caller must NOT hold the registry mutex. */
static void registry_add(pjsua_call_id id, pj_bool_t has_video)
{
    int i;
    pj_mutex_lock(g.mutex);
    /* Skip if already tracked (callback can fire CONFIRMED more than once
     * across re-INVITEs). */
    for (i = 0; i < g.leg_count; ++i) {
        if (g.legs[i].id == id) {
            pj_mutex_unlock(g.mutex);
            return;
        }
    }
    if (g.leg_count < g.opts.max_legs) {
        g.legs[g.leg_count].id = id;
        g.legs[g.leg_count].has_video = has_video;
        g.leg_count++;
        if (has_video)
            g.video_leg_count++;
    }
    pj_mutex_unlock(g.mutex);
}

/* Remove a leg by id. */
static void registry_remove(pjsua_call_id id)
{
    int i;
    pj_mutex_lock(g.mutex);
    for (i = 0; i < g.leg_count; ++i) {
        if (g.legs[i].id == id) {
            if (g.legs[i].has_video)
                g.video_leg_count--;
            /* swap with last */
            g.legs[i] = g.legs[g.leg_count - 1];
            g.leg_count--;
            break;
        }
    }
    pj_mutex_unlock(g.mutex);
}

/* Pick a random leg's id, returning PJSUA_INVALID_ID if empty. */
static pjsua_call_id registry_pick_random(unsigned *rng)
{
    pjsua_call_id id = PJSUA_INVALID_ID;
    pj_mutex_lock(g.mutex);
    if (g.leg_count > 0) {
        int idx = rand_r(rng) % g.leg_count;
        id = g.legs[idx].id;
    }
    pj_mutex_unlock(g.mutex);
    return id;
}

/* Pick two distinct random leg ids. Returns PJ_FALSE if fewer than 2 legs. */
static pj_bool_t registry_pick_two(unsigned *rng, pjsua_call_id *a,
                                   pjsua_call_id *b)
{
    pj_bool_t ok = PJ_FALSE;
    pj_mutex_lock(g.mutex);
    if (g.leg_count >= 2) {
        int i = rand_r(rng) % g.leg_count;
        int j = rand_r(rng) % (g.leg_count - 1);
        if (j >= i) ++j;
        *a = g.legs[i].id;
        *b = g.legs[j].id;
        ok = PJ_TRUE;
    }
    pj_mutex_unlock(g.mutex);
    return ok;
}

static int registry_leg_count(void)
{
    int n;
    pj_mutex_lock(g.mutex);
    n = g.leg_count;
    pj_mutex_unlock(g.mutex);
    return n;
}

static int registry_video_count(void)
{
    int n;
    pj_mutex_lock(g.mutex);
    n = g.video_leg_count;
    pj_mutex_unlock(g.mutex);
    return n;
}


/*============================================================================
 * PJSUA callbacks
 *==========================================================================*/

/* Build a pjsua_call_setting that matches what this stress test wants:
 * one audio, no text, optional video. */
static void make_call_setting(pjsua_call_setting *opt, pj_bool_t with_video)
{
    pjsua_call_setting_default(opt);
    opt->aud_cnt = 1;
    opt->vid_cnt = with_video ? 1 : 0;
    opt->txt_cnt = 0;
}

static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    pjsua_call_setting opt;
    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);
    make_call_setting(&opt, (g.opts.max_video_legs > 0));
    pjsua_call_answer2(call_id, &opt, 200, NULL, NULL);
}

/* Runs from the pjsua event loop, NOT under the dialog grp_lock — see
 * on_call_state() for the lock-order rationale. */
static void on_call_state_change_deferred(pj_timer_heap_t *th,
                                          pj_timer_entry *te)
{
    pjsua_call_id call_id = (pjsua_call_id)(pj_ssize_t)te->user_data;
    pjsua_call_info ci;
    pj_bool_t has_video = PJ_FALSE;
    unsigned i;

    PJ_UNUSED_ARG(th);

    if (pjsua_call_get_info(call_id, &ci) != PJ_SUCCESS) {
        registry_remove(call_id);
        return;
    }

    for (i = 0; i < ci.media_cnt; ++i) {
        if (ci.media[i].type == PJMEDIA_TYPE_VIDEO &&
            ci.media[i].dir != PJMEDIA_DIR_NONE)
        {
            has_video = PJ_TRUE;
            break;
        }
    }

    if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
        registry_add(call_id, has_video);
    } else if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        registry_remove(call_id);
    }

    /* Connect audio call to itself so the conf bridge drains audio frames. */
    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE &&
        ci.conf_slot != PJSUA_INVALID_ID)
    {
        pjsua_conf_connect(ci.conf_slot, ci.conf_slot);
    }
}

/* Defer the registry update: calling pjsua_call_get_info() here would
 * acquire PJSUA_LOCK while the SIP stack already holds the dialog grp_lock,
 * inverting the order that pjsua_call_make_call uses (PJSUA_LOCK first). */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pj_time_val zero = {0, 0};
    PJ_UNUSED_ARG(e);
    if (!g.deferred_state_timers ||
        call_id < 0 || call_id >= g.opts.max_legs)
        return;
    pjsua_schedule_timer(&g.deferred_state_timers[call_id], &zero);
}

static void on_call_media_state(pjsua_call_id call_id)
{
    pj_time_val zero = {0, 0};
    if (!g.deferred_state_timers ||
        call_id < 0 || call_id >= g.opts.max_legs)
        return;
    pjsua_schedule_timer(&g.deferred_state_timers[call_id], &zero);
}


/*============================================================================
 * Worker thread: random ops
 *==========================================================================*/

static void do_hangup(worker_ctx_t *w)
{
    pjsua_call_id id = registry_pick_random(&w->rng_state);
    if (id == PJSUA_INVALID_ID) return;
    w->ops[OP_HANGUP]++;
    if (pjsua_call_hangup(id, 200, NULL, NULL) == PJ_SUCCESS)
        w->ok_ops[OP_HANGUP]++;
}

static void do_make_call(worker_ctx_t *w, pj_bool_t with_video)
{
    pjsua_call_setting opt;
    pj_str_t uri;
    pj_status_t status;
    int op = with_video ? OP_MAKE_VIDEO : OP_MAKE_AUDIO;

    /* Cap check. We compare against pjsua_call_get_count() (total active call
     * slots) rather than registry leg_count (CONFIRMED only). Each new call
     * also reserves an incoming-leg slot, so reaching PJSUA_MAX_CALLS while
     * make_call is mid-flight produces PJ_ETOOMANY and can leave the slot in
     * an inconsistent state on retry. Reserve room for both legs. */
    if ((int)pjsua_call_get_count() + 2 > g.opts.max_legs)
        return;
    pj_mutex_lock(g.mutex);
    if (with_video && g.video_leg_count + 2 > g.opts.max_video_legs) {
        pj_mutex_unlock(g.mutex);
        return;
    }
    pj_mutex_unlock(g.mutex);

    make_call_setting(&opt, with_video);
    uri = pj_str(g.opts.self_uri);

    w->ops[op]++;
    status = pjsua_call_make_call(g.acc_id, &uri, &opt, NULL, NULL, NULL);
    if (status == PJ_SUCCESS)
        w->ok_ops[op]++;
}

static void do_conf_conn(worker_ctx_t *w, pj_bool_t connect)
{
    pjsua_call_id a, b;
    pjsua_call_info ci_a, ci_b;
    int op = connect ? OP_CONF_CONN : OP_CONF_DISC;

    if (!registry_pick_two(&w->rng_state, &a, &b))
        return;
    w->ops[op]++;
    if (pjsua_call_get_info(a, &ci_a) != PJ_SUCCESS) return;
    if (pjsua_call_get_info(b, &ci_b) != PJ_SUCCESS) return;
    if (ci_a.conf_slot == PJSUA_INVALID_ID) return;
    if (ci_b.conf_slot == PJSUA_INVALID_ID) return;
    if (connect) {
        if (pjsua_conf_connect(ci_a.conf_slot, ci_b.conf_slot) == PJ_SUCCESS)
            w->ok_ops[op]++;
    } else {
        if (pjsua_conf_disconnect(ci_a.conf_slot, ci_b.conf_slot) == PJ_SUCCESS)
            w->ok_ops[op]++;
    }
}

static void do_hold(worker_ctx_t *w)
{
    pjsua_call_id id = registry_pick_random(&w->rng_state);
    if (id == PJSUA_INVALID_ID) return;
    w->ops[OP_HOLD]++;
    if (pjsua_call_set_hold(id, NULL) == PJ_SUCCESS)
        w->ok_ops[OP_HOLD]++;
}

static void do_unhold(worker_ctx_t *w)
{
    pjsua_call_id id = registry_pick_random(&w->rng_state);
    pjsua_call_setting opt;
    if (id == PJSUA_INVALID_ID) return;
    make_call_setting(&opt, (g.opts.max_video_legs > 0));
    opt.flag = PJSUA_CALL_UNHOLD;
    w->ops[OP_UNHOLD]++;
    /* reinvite2 with PJSUA_CALL_UNHOLD; older reinvite() takes a flag bool. */
    if (pjsua_call_reinvite2(id, &opt, NULL) == PJ_SUCCESS)
        w->ok_ops[OP_UNHOLD]++;
}

static int worker_thread(void *arg)
{
    worker_ctx_t *w = (worker_ctx_t *)arg;
    pj_bool_t video_enabled = (g.opts.max_video_legs > 0);
    int op_modulus = OP_COUNT;

    PJ_LOG(4, (THIS_FILE, "worker %d started", w->id));
    while (!stop_flag_get()) {
        int op = rand_r(&w->rng_state) % op_modulus;
        switch (op) {
        case OP_HANGUP:      do_hangup(w);             break;
        case OP_MAKE_AUDIO:  do_make_call(w, PJ_FALSE); break;
        case OP_MAKE_VIDEO:
            if (video_enabled) do_make_call(w, PJ_TRUE);
            else               do_make_call(w, PJ_FALSE);
            break;
        case OP_CONF_CONN:   do_conf_conn(w, PJ_TRUE);  break;
        case OP_CONF_DISC: do_conf_conn(w, PJ_FALSE); break;
        case OP_HOLD:        do_hold(w);                break;
        case OP_UNHOLD:      do_unhold(w);              break;
        }
        /* Back-off to leave CPU for pjsua's worker threads (which run
         * re-INVITE state machines, transport callbacks, etc.). Without
         * this, workers can starve pjsua's reinv_timer and trigger spurious
         * deadlock-suspect warnings even when nothing is actually deadlocked. */
        pj_thread_sleep(5 + (rand_r(&w->rng_state) % 10));
    }
    PJ_LOG(4, (THIS_FILE, "worker %d stopping", w->id));
    return 0;
}


/*============================================================================
 * Stats / watchdog
 *==========================================================================*/

static int stats_thread_proc(void *arg)
{
    PJ_UNUSED_ARG(arg);
    while (!stop_flag_get()) {
        unsigned i;
        for (i = 0; i < STATS_INTERVAL_MS / 100 && !stop_flag_get(); ++i)
            pj_thread_sleep(100);
        if (stop_flag_get()) break;
        PJ_LOG(3, (THIS_FILE,
                   "stats: tracked_legs=%d video=%d pjsua_calls=%u",
                   registry_leg_count(), registry_video_count(),
                   pjsua_call_get_count()));
    }
    return 0;
}


/*============================================================================
 * Setup / teardown
 *==========================================================================*/

static void error_exit(const char *title, pj_status_t status)
{
    pjsua_perror(THIS_FILE, title, status);
    pjsua_destroy();
    exit(1);
}

static int setup_pjsua(const stress_opts_t *opts)
{
    pjsua_config         cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config   media_cfg;
    pjsua_transport_config tcfg;
    pj_status_t          status;

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "pjsua_create() failed", status);
        return 1;
    }

    pjsua_config_default(&cfg);
    cfg.max_calls = opts->max_legs;
    cfg.cb.on_incoming_call    = &on_incoming_call;
    cfg.cb.on_call_state       = &on_call_state;
    cfg.cb.on_call_media_state = &on_call_media_state;
    /* Use pjsua's own worker threads for SIP/media; our worker threads only
     * issue API calls. */
    cfg.thread_cnt = 2;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = opts->log_level;
    log_cfg.level = opts->log_level;
    log_cfg.msg_logging = PJ_FALSE;

    pjsua_media_config_default(&media_cfg);
    /* Fewer codec threads keeps sanitizer noise down. */
    media_cfg.no_vad = PJ_TRUE;
    /* Uniform 8 kHz across sound device, conference bridge, and codecs
     * so the audio pipeline never resamples. Pairs with the G.711-only
     * codec setup below (PCMU/PCMA are 8 kHz). */
    media_cfg.clock_rate     = 8000;
    media_cfg.snd_clock_rate = 8000;

    status = pjsua_init(&cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS) error_exit("pjsua_init() failed", status);

    pjsua_transport_config_default(&tcfg);
    tcfg.port = opts->port;
    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tcfg, NULL);
    if (status != PJ_SUCCESS) error_exit("transport create failed", status);

    /* Resolve video devices. Capture: Colorbar-active — the "active"
     * variant pushes frames on its own clock so the video media flow
     * actually moves under stress; the passive Colorbar device only
     * emits frames when polled, which doesn't happen because no clock
     * is wired to drive it here.
     * Render: null (discards frames). Using a null renderer keeps the
     * test out of the real renderer back-ends (SDL / Metal / Darwin /
     * etc.), which would otherwise need a main-thread runloop on macOS
     * and would slow stress runs everywhere by opening real windows.
     * Both fall back to PJMEDIA_VID_DEFAULT_* if the device isn't
     * available (e.g. colorbar or null factory wasn't compiled in). */
#if defined(PJMEDIA_HAS_VIDEO) && PJMEDIA_HAS_VIDEO != 0
    g.colorbar_dev  = PJMEDIA_VID_DEFAULT_CAPTURE_DEV;
    g.null_rend_dev = PJMEDIA_VID_DEFAULT_RENDER_DEV;
    if (opts->max_video_legs > 0) {
        pjmedia_vid_dev_index idx;
        status = pjmedia_vid_dev_lookup("Colorbar", "Colorbar-active",
                                        &idx);
        if (status == PJ_SUCCESS) {
            g.colorbar_dev = idx;
            PJ_LOG(3, (THIS_FILE,
                       "Using Colorbar-active capture dev id=%d", idx));
        } else {
            PJ_LOG(2, (THIS_FILE,
                       "Colorbar-active device not found, using default"));
        }
        status = pjmedia_vid_dev_lookup("Null", "Null renderer", &idx);
        if (status == PJ_SUCCESS) {
            g.null_rend_dev = idx;
            PJ_LOG(3, (THIS_FILE, "Using Null render dev id=%d", idx));
        } else {
            PJ_LOG(2, (THIS_FILE, "Null render device not found, using default"));
        }
    }
#else
    if (opts->max_video_legs > 0) {
        PJ_LOG(1, (THIS_FILE,
                   "Video disabled at compile time, -v is ignored"));
    }
#endif

    /* Acquire the transport id from the just-created UDP transport. */
    {
        pjsua_transport_id tid;
        pjsua_transport_info tinfo;
        pjsua_transport_id ids[8];
        unsigned cnt = PJ_ARRAY_SIZE(ids);
        unsigned i;
        pjsua_acc_config acc_cfg;

        status = pjsua_enum_transports(ids, &cnt);
        if (status != PJ_SUCCESS || cnt == 0)
            error_exit("enum_transports failed", status);
        (void)tinfo;
        tid = ids[0];
        for (i = 0; i < cnt; ++i) {
            if (pjsua_transport_get_info(ids[i], &tinfo) == PJ_SUCCESS &&
                tinfo.type == PJSIP_TRANSPORT_UDP)
            {
                tid = ids[i];
                break;
            }
        }
        status = pjsua_acc_add_local(tid, PJ_TRUE, &g.acc_id);
        if (status != PJ_SUCCESS) error_exit("acc_add_local failed", status);

        /* Round-trip through get_config so we preserve the SIP id, contact and
         * other fields that pjsua_acc_add_local() filled in. */
        {
            pj_pool_t *tmp_pool = pjsua_pool_create("acc-tmp", 512, 512);
            pjsua_acc_config_default(&acc_cfg);
            status = pjsua_acc_get_config(g.acc_id, tmp_pool, &acc_cfg);
            if (status == PJ_SUCCESS) {
                acc_cfg.vid_in_auto_show      = PJ_FALSE;
                acc_cfg.vid_out_auto_transmit = PJ_TRUE;
                acc_cfg.vid_cap_dev           = g.colorbar_dev;
                acc_cfg.vid_rend_dev          = g.null_rend_dev;
                pjsua_acc_modify(g.acc_id, &acc_cfg);
            }
            pj_pool_release(tmp_pool);
        }
    }

    pjsua_set_null_snd_dev();

    status = pjsua_start();
    if (status != PJ_SUCCESS) error_exit("pjsua_start() failed", status);

    /* Use G.711 (PCMU/PCMA, 8 kHz) only so the pipeline matches the
     * 8 kHz clock_rate set above and never has to resample. Disable
     * every other audio codec by priority 0. */
    {
        pjsua_codec_info codecs[64];
        unsigned i, ccnt = PJ_ARRAY_SIZE(codecs);
        status = pjsua_enum_codecs(codecs, &ccnt);
        if (status == PJ_SUCCESS) {
            for (i = 0; i < ccnt; ++i) {
                pj_uint8_t prio = 0;
                if (pj_strnicmp2(&codecs[i].codec_id, "PCMU/", 5) == 0 ||
                    pj_strnicmp2(&codecs[i].codec_id, "PCMA/", 5) == 0)
                {
                    prio = PJMEDIA_CODEC_PRIO_NORMAL;
                }
                pjsua_codec_set_priority(&codecs[i].codec_id, prio);
            }
        }
    }

    return 0;
}

static int run_ramp(const stress_opts_t *opts)
{
    pj_timestamp start, now;
    int last_count = -1;
    PJ_LOG(3, (THIS_FILE, "Ramp phase: target %d legs (%d video) ...",
               opts->max_legs, opts->max_video_legs));
    pj_get_timestamp(&start);

    for (;;) {
        int legs = registry_leg_count();
        int vlegs = registry_video_count();
        if (legs >= opts->max_legs)
            break;
        pj_get_timestamp(&now);
        if (pj_elapsed_msec(&start, &now) >
            (pj_uint32_t)RAMP_TIMEOUT_SEC * 1000)
        {
            PJ_LOG(1, (THIS_FILE,
                       "Ramp timed out after %d s at %d legs (target %d)",
                       RAMP_TIMEOUT_SEC, legs, opts->max_legs));
            return 1;
        }

        /* Issue one new call per loop iteration. Audio-only unless we still
         * need video legs. Bound against PJSUA_MAX_CALLS (counting both legs)
         * to avoid PJ_ETOOMANY races. */
        if ((int)pjsua_call_get_count() + 2 <= opts->max_legs) {
            pjsua_call_setting opt;
            pj_str_t uri = pj_str(g.opts.self_uri);
            pj_status_t status;
            pj_bool_t want_video = (vlegs + 2 <= opts->max_video_legs);

            pjsua_call_setting_default(&opt);
            opt.aud_cnt = 1;
            opt.vid_cnt = want_video ? 1 : 0;
            opt.txt_cnt = 0;
            status = pjsua_call_make_call(g.acc_id, &uri, &opt, NULL, NULL,
                                          NULL);
            if (status != PJ_SUCCESS) {
                PJ_LOG(2, (THIS_FILE,
                           "make_call failed during ramp (status=%d)",
                           status));
                pj_thread_sleep(50);
            }
        } else {
            /* At capacity — wait briefly for slots to free up. */
            pj_thread_sleep(20);
        }

        /* Brief pause to let the ioqueue process the resulting INVITE/ACK. */
        pj_thread_sleep(5);

        /* Periodic progress log. */
        if (legs / 32 != last_count / 32) {
            PJ_LOG(3, (THIS_FILE, "  ramp: %d/%d legs (%d video)",
                       legs, opts->max_legs, vlegs));
            last_count = legs;
        }
    }

    PJ_LOG(3, (THIS_FILE, "Ramp complete: %d legs (%d video)",
               registry_leg_count(), registry_video_count()));
    return 0;
}

static int run_stress(const stress_opts_t *opts)
{
    int i;
    pj_status_t status;
    pj_timestamp start, now;

    PJ_LOG(3, (THIS_FILE, "Stress phase: %d threads for %d s",
               opts->worker_count, opts->duration_sec));

    stop_flag_set(PJ_FALSE);

    /* Spawn stats thread. */
    status = pj_thread_create(g.pool, "stats", &stats_thread_proc, NULL,
                              0, 0, &g.stats_thread);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "stats thread create failed", status);
        return 1;
    }

    /* Spawn workers. */
    for (i = 0; i < opts->worker_count; ++i) {
        char name[32];
        worker_ctx_t *w = &g.workers[i];
        w->id = i;
        w->rng_state = opts->seed ^ (unsigned)(i * 2654435761u);
        pj_ansi_snprintf(name, sizeof(name), "stress-%d", i);
        status = pj_thread_create(g.pool, name, &worker_thread, w,
                                  0, 0, &w->thread);
        if (status != PJ_SUCCESS) {
            pjsua_perror(THIS_FILE, "worker thread create failed", status);
            stop_flag_set(PJ_TRUE);
            return 1;
        }
    }

    /* Sleep until deadline. */
    pj_get_timestamp(&start);
    for (;;) {
        pj_get_timestamp(&now);
        if (pj_elapsed_msec(&start, &now) >=
            (pj_uint32_t)opts->duration_sec * 1000)
            break;
        pj_thread_sleep(500);
    }

    PJ_LOG(3, (THIS_FILE, "Stress duration elapsed; signaling stop"));
    stop_flag_set(PJ_TRUE);

    for (i = 0; i < opts->worker_count; ++i) {
        if (g.workers[i].thread) {
            pj_thread_join(g.workers[i].thread);
            pj_thread_destroy(g.workers[i].thread);
            g.workers[i].thread = NULL;
        }
    }
    if (g.stats_thread) {
        pj_thread_join(g.stats_thread);
        pj_thread_destroy(g.stats_thread);
        g.stats_thread = NULL;
    }
    return 0;
}

static void print_op_totals(void)
{
    int op, w;
    pj_uint64_t totals[OP_COUNT] = {0};
    pj_uint64_t ok_totals[OP_COUNT] = {0};

    for (w = 0; w < g.opts.worker_count; ++w) {
        for (op = 0; op < OP_COUNT; ++op) {
            totals[op] += g.workers[w].ops[op];
            ok_totals[op] += g.workers[w].ok_ops[op];
        }
    }
    PJ_LOG(3, (THIS_FILE, "Op totals (ok / attempted):"));
    for (op = 0; op < OP_COUNT; ++op) {
        PJ_LOG(3, (THIS_FILE, "  %-12s %llu / %llu", op_name(op),
                   (unsigned long long)ok_totals[op],
                   (unsigned long long)totals[op]));
    }
}

static int run_cleanup(void)
{
    int i;
    PJ_LOG(3, (THIS_FILE,
               "Cleanup: hangup_all (%u calls active), waiting for drain",
               pjsua_call_get_count()));
    pjsua_call_hangup_all();
    for (i = 0; i < CLEANUP_WAIT_SEC * 10; ++i) {
        if (pjsua_call_get_count() == 0)
            break;
        pj_thread_sleep(100);
    }
    if (pjsua_call_get_count() != 0) {
        /* See CLEANUP_WAIT_SEC's comment: under stress some calls park in
         * "Delaying BYE request until ACK is received" and never drain on
         * their own; pjsua_destroy() force-tears-down the remainder, so
         * this isn't a test failure. */
        PJ_LOG(2, (THIS_FILE,
                   "Cleanup: %u calls still active after %d s; "
                   "leaving force-teardown to pjsua_destroy()",
                   pjsua_call_get_count(), CLEANUP_WAIT_SEC));
    }
    return 0;
}


/*============================================================================
 * CLI
 *==========================================================================*/

static void usage(void)
{
    puts(
        "Usage: pjsua-stress [options]\n"
        "  -n N            Max audio call legs        (default 64)\n"
        "  -v V            Max video call legs        (default 0)\n"
        "  -t T            Worker thread count        (default 4)\n"
        "  --duration S    Stress phase seconds       (default 300)\n"
        "  --port P        Local UDP SIP port         (default 5060)\n"
        "  --self-uri URI  Outgoing dial target       (default sip:stress@127.0.0.1:<port>)\n"
        "  --seed S        RNG seed                   (default time-based)\n"
        "  --log-level L   PJ console log level       (default 3)\n"
        "  -h, --help      Show this help and exit\n"
    );
}

static int parse_int(const char *s, int *out)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (*end != '\0') return -1;
    *out = (int)v;
    return 0;
}

static int parse_args(int argc, char *argv[], stress_opts_t *opts)
{
    int i;
    int seed_set = 0;

    opts->max_legs       = DEF_MAX_LEGS;
    opts->max_video_legs = DEF_MAX_VID_LEGS;
    opts->worker_count   = DEF_WORKERS;
    opts->duration_sec   = DEF_DURATION_SEC;
    opts->port           = DEF_PORT;
    opts->log_level      = DEF_LOG_LEVEL;
    opts->self_uri[0]    = '\0';
    opts->seed           = (unsigned)pj_rand();

    for (i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage();
            return 1;
        } else if (!strcmp(a, "-n") && i + 1 < argc) {
            if (parse_int(argv[++i], &opts->max_legs)) goto bad;
        } else if (!strcmp(a, "-v") && i + 1 < argc) {
            if (parse_int(argv[++i], &opts->max_video_legs)) goto bad;
        } else if (!strcmp(a, "-t") && i + 1 < argc) {
            if (parse_int(argv[++i], &opts->worker_count)) goto bad;
        } else if (!strcmp(a, "--duration") && i + 1 < argc) {
            if (parse_int(argv[++i], &opts->duration_sec)) goto bad;
        } else if (!strcmp(a, "--port") && i + 1 < argc) {
            if (parse_int(argv[++i], &opts->port)) goto bad;
        } else if (!strcmp(a, "--self-uri") && i + 1 < argc) {
            pj_ansi_strxcpy(opts->self_uri, argv[++i],
                            sizeof(opts->self_uri));
        } else if (!strcmp(a, "--seed") && i + 1 < argc) {
            int s;
            if (parse_int(argv[++i], &s)) goto bad;
            opts->seed = (unsigned)s;
            seed_set = 1;
        } else if (!strcmp(a, "--log-level") && i + 1 < argc) {
            if (parse_int(argv[++i], &opts->log_level)) goto bad;
        } else {
            fprintf(stderr, "Unknown option: %s\n", a);
            goto bad;
        }
    }

    if (opts->max_legs <= 0 || opts->max_legs > PJSUA_MAX_CALLS) {
        fprintf(stderr,
                "-n must be in 1..PJSUA_MAX_CALLS (=%d). config_site.h sets "
                "this; see config_site_stress.h.\n", PJSUA_MAX_CALLS);
        return -1;
    }
    if (opts->max_video_legs < 0 || opts->max_video_legs > opts->max_legs) {
        fprintf(stderr, "-v must be in 0..N\n");
        return -1;
    }
    if (opts->worker_count <= 0 || opts->worker_count > 64) {
        fprintf(stderr, "-t must be in 1..64\n");
        return -1;
    }
    if (opts->duration_sec <= 0) {
        fprintf(stderr, "--duration must be > 0\n");
        return -1;
    }
    if (opts->self_uri[0] == '\0') {
        pj_ansi_snprintf(opts->self_uri, sizeof(opts->self_uri),
                         "sip:stress@127.0.0.1:%d", opts->port);
    }
    if (!seed_set) {
        pj_time_val tv;
        pj_gettickcount(&tv);
        opts->seed ^= (unsigned)tv.sec ^ (unsigned)tv.msec;
    }
    return 0;

bad:
    usage();
    return -1;
}


/*============================================================================
 * Crash backtrace
 *==========================================================================*/

#ifdef PJSUA_STRESS_HAS_BACKTRACE
/* Async-signal-safe handler: dumps a native backtrace, then re-raises
 * the signal with the default disposition so the process still dies
 * with the original status (e.g. 134 for SIGABRT). The build links
 * with -rdynamic so non-static symbols resolve to names. */
static void crash_signal_handler(int sig)
{
    static const char header[] = "\n=== pjsua_stress backtrace ===\n";
    void *frames[64];
    int n;

    (void)!write(STDERR_FILENO, header, sizeof(header) - 1);
    n = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    backtrace_symbols_fd(frames, n, STDERR_FILENO);

    signal(sig, SIG_DFL);
    raise(sig);
    _exit(128 + sig);  /* fallback if raise() somehow returns */
}

static void install_crash_handler(void)
{
    void *dummy[1];
    /* Prime the libgcc unwinder so backtrace() inside the handler
     * doesn't take the slow first-call path that may allocate. */
    backtrace(dummy, 1);
    signal(SIGABRT, crash_signal_handler);
    signal(SIGSEGV, crash_signal_handler);
}
#else
static void install_crash_handler(void) {}
#endif


/*============================================================================
 * main
 *==========================================================================*/

int main(int argc, char *argv[])
{
    int rc;
    pj_status_t status;
    int parse_rc;

    install_crash_handler();

    /* Independent pjlib ref + caching pool so g.pool/g.mutex outlive
     * pjsua_destroy() and survive late on_call_state callbacks. */
    status = pj_init();
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "pj_init() failed"));
        return 1;
    }

    parse_rc = parse_args(argc, argv, &g.opts);
    if (parse_rc < 0) {
        pj_shutdown();
        return 2;
    }
    if (parse_rc > 0) {
        pj_shutdown();
        return 0;
    }

    pj_caching_pool_init(&g.cp, &pj_pool_factory_default_policy, 0);
    g.cp_inited = PJ_TRUE;

    if (setup_pjsua(&g.opts) != 0) {
        pj_caching_pool_destroy(&g.cp);
        pj_shutdown();
        return 1;
    }

    PJ_LOG(3, (THIS_FILE,
               "pjsua_stress: N=%d V=%d T=%d duration=%ds port=%d seed=%u",
               g.opts.max_legs, g.opts.max_video_legs, g.opts.worker_count,
               g.opts.duration_sec, g.opts.port, g.opts.seed));

    g.pool = pj_pool_create(&g.cp.factory, "stress", 4096, 4096, NULL);
    if (!g.pool) {
        PJ_LOG(1, (THIS_FILE, "pjsua_pool_create failed"));
        pjsua_destroy();
        return 1;
    }
    g.legs = (leg_entry_t *)pj_pool_zalloc(
        g.pool, sizeof(leg_entry_t) * g.opts.max_legs);
    g.workers = (worker_ctx_t *)pj_pool_zalloc(
        g.pool, sizeof(worker_ctx_t) * g.opts.worker_count);
    g.deferred_state_timers = (pj_timer_entry *)pj_pool_zalloc(
        g.pool, sizeof(pj_timer_entry) * g.opts.max_legs);
    if (!g.legs || !g.workers || !g.deferred_state_timers) {
        PJ_LOG(1, (THIS_FILE, "pool alloc failed"));
        pjsua_destroy();
        return 1;
    }
    {
        int idx;
        for (idx = 0; idx < g.opts.max_legs; ++idx) {
            pj_timer_entry_init(&g.deferred_state_timers[idx], 0,
                                (void*)(pj_ssize_t)idx,
                                &on_call_state_change_deferred);
        }
    }
    if (pj_mutex_create_recursive(g.pool, "stress-reg", &g.mutex)
        != PJ_SUCCESS)
    {
        PJ_LOG(1, (THIS_FILE, "mutex create failed"));
        pjsua_destroy();
        return 1;
    }
    status = pj_atomic_create(g.pool, 0, &g.stop_flag);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "atomic create failed"));
        pjsua_destroy();
        return 1;
    }

    rc = run_ramp(&g.opts);
    if (rc == 0) {
        rc = run_stress(&g.opts);
        print_op_totals();
    }

    if (run_cleanup() != 0 && rc == 0)
        rc = 1;

    /* Destroy pjsua first so its workers stop firing on_call_state
     * callbacks before we tear down g.mutex (held via registry_remove). */
    pjsua_destroy();

    if (g.mutex) {
        pj_mutex_destroy(g.mutex);
        g.mutex = NULL;
    }
    if (g.stop_flag) {
        pj_atomic_destroy(g.stop_flag);
        g.stop_flag = NULL;
    }
    if (g.pool) {
        pj_pool_release(g.pool);
        g.pool = NULL;
    }
    if (g.cp_inited) {
        pj_caching_pool_destroy(&g.cp);
        g.cp_inited = PJ_FALSE;
    }

    pj_shutdown();
    return rc;
}

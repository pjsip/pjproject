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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */
#include "test.h"
#include <pjmedia/conference.h>
#include <pjmedia/config.h>
#include <pjmedia/null_port.h>
#include <pjmedia/errno.h>
#include <pj/pool.h>
#include <pj/log.h>

#define THIS_FILE   "conf_test.c"

#define CLOCK_RATE  16000
#define CHANNELS    1
#define SPF         (CLOCK_RATE/100)   /* 10ms */
#define BPS         16

/* A different clock rate to exercise the resampler/buffer rebuild path.
 * A cross-rate replace needs sample-rate conversion, so those steps only run
 * when a resample implementation is compiled in.
 */
#define ALT_RATE    8000
#define ALT_SPF     (ALT_RATE/100)

#if PJMEDIA_RESAMPLE_IMP != PJMEDIA_RESAMPLE_NONE
#  define HAS_RESAMPLE  1
#else
#  define HAS_RESAMPLE  0
#endif

/* Distinctive level and mute settings applied to the replaced slot, to verify
 * per-slot state survives detach/replace. */
#define TEST_LEVEL  42

/* Drive the conference clock so queued (async) operations are processed. */
static pj_status_t pump(pjmedia_port *master, unsigned n)
{
    pj_int16_t buf[SPF];
    pjmedia_frame f;
    unsigned i;
    pj_status_t status;

    for (i = 0; i < n; ++i) {
        pj_bzero(&f, sizeof(f));
        f.type = PJMEDIA_FRAME_TYPE_AUDIO;
        f.buf = buf;
        f.size = sizeof(buf);
        status = pjmedia_port_get_frame(master, &f);
        if (status != PJ_SUCCESS)
            return status;
    }
    return PJ_SUCCESS;
}

/* Assert slot1 still transmits to slot2, slot2 kept its rx level + tx mute,
 * and the port attached to slot2 has the expected clock rate (0 = don't check,
 * e.g. right after a detach when no port is attached).
 */
static int check_state(pjmedia_conf *conf, unsigned slot1, unsigned slot2,
                       unsigned exp_rate, const char *stage)
{
    pjmedia_conf_port_info info;
    pj_status_t status;

    status = pjmedia_conf_get_port_info(conf, slot1, &info);
    if (status != PJ_SUCCESS) return -1;
    if (info.listener_cnt != 1 || info.listener_slots[0] != slot2) {
        PJ_LOG(1,(THIS_FILE, "   %s: connection not preserved", stage));
        return -2;
    }

    status = pjmedia_conf_get_port_info(conf, slot2, &info);
    if (status != PJ_SUCCESS) return -3;
    if (info.slot != slot2) return -4;
    if (info.rx_adj_level != TEST_LEVEL) {
        PJ_LOG(1,(THIS_FILE, "   %s: rx level not preserved", stage));
        return -5;
    }
    if (info.tx_setting != PJMEDIA_PORT_MUTE) {
        PJ_LOG(1,(THIS_FILE, "   %s: tx mute not preserved", stage));
        return -6;
    }
    if (exp_rate && info.clock_rate != exp_rate) {
        PJ_LOG(1,(THIS_FILE, "   %s: attached port rate %d != expected %d",
                  stage, info.clock_rate, exp_rate));
        return -7;
    }
    return 0;
}

/*
 * Verify that pjmedia_conf_detach_port()/pjmedia_conf_replace_port() preserve
 * the slot id, its connections, and its level/mute settings across a port swap
 * (both the same-format pointer-swap and the format-changing rebuild paths).
 *
 * Returns 0 on success, 1 if the backend does not support the APIs (skipped),
 * or a negative error code on failure.
 */
static int detach_replace_test(void)
{
    pj_pool_t *pool = NULL;
    pjmedia_conf *conf = NULL;
    pjmedia_port *master, *p1 = NULL, *p2 = NULL, *p2b = NULL;
    unsigned slot1 = 0, slot2 = 0;
    int rc = 0;
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  conf detach/replace preserves slot state"));

    pool = pj_pool_create(mem, "conf_test", 4000, 4000, NULL);
    if (!pool) return -5;

    status = pjmedia_conf_create(pool, 8, CLOCK_RATE, CHANNELS, SPF, BPS,
                                 PJMEDIA_CONF_NO_DEVICE, &conf);
    if (status != PJ_SUCCESS) { rc = -10; goto on_return; }
    master = pjmedia_conf_get_master_port(conf);

    status = pjmedia_null_port_create(pool, CLOCK_RATE, CHANNELS, SPF, BPS, &p1);
    if (status != PJ_SUCCESS) { rc = -20; goto on_return; }
    status = pjmedia_null_port_create(pool, CLOCK_RATE, CHANNELS, SPF, BPS, &p2);
    if (status != PJ_SUCCESS) { rc = -21; goto on_return; }

    status = pjmedia_conf_add_port(conf, pool, p1, NULL, &slot1);
    if (status != PJ_SUCCESS) { rc = -30; goto on_return; }
    status = pjmedia_conf_add_port(conf, pool, p2, NULL, &slot2);
    if (status != PJ_SUCCESS) { rc = -31; goto on_return; }
    pump(master, 2);

    /* slot1 -> slot2; apply a distinctive rx level and tx mute on slot2 (the
     * slot that will be detached/replaced), so the checks are meaningful. */
    status = pjmedia_conf_connect_port(conf, slot1, slot2, 0);
    if (status != PJ_SUCCESS) { rc = -40; goto on_return; }
    status = pjmedia_conf_adjust_rx_level(conf, slot2, TEST_LEVEL);
    if (status != PJ_SUCCESS) { rc = -41; goto on_return; }
    status = pjmedia_conf_configure_port(conf, slot2, PJMEDIA_PORT_MUTE,
                                         PJMEDIA_PORT_ENABLE);
    if (status != PJ_SUCCESS) { rc = -42; goto on_return; }
    pump(master, 2);

    /* Detach slot2. If the backend doesn't support it (e.g. switchboard),
     * skip the test rather than fail. */
    status = pjmedia_conf_detach_port(conf, slot2);
    if (status == PJ_ENOTSUP) {
        PJ_LOG(3,(THIS_FILE, "  detach/replace not supported by this backend"
                             " - skipped"));
        rc = 1;
        goto on_return;
    }
    if (status != PJ_SUCCESS) { rc = -60; goto on_return; }
    pump(master, 2);

    /* Connection to a detached slot must survive. */
    if ((rc = check_state(conf, slot1, slot2, 0, "after-detach")) != 0)
        goto on_return;

    /* Replace with the same format (pure pointer-swap path). */
    status = pjmedia_null_port_create(pool, CLOCK_RATE, CHANNELS, SPF, BPS, &p2b);
    if (status != PJ_SUCCESS) { rc = -70; goto on_return; }
    status = pjmedia_conf_replace_port(conf, pool, slot2, p2b);
    if (status != PJ_SUCCESS) { rc = -71; goto on_return; }
    pump(master, 3);
    if ((rc = check_state(conf, slot1, slot2, CLOCK_RATE, "after-replace-same-fmt")) != 0)
        goto on_return;

#if HAS_RESAMPLE
    /* Replace with a DIFFERENT clock rate to exercise the resampler/buffer
     * rebuild path. Needs sample-rate conversion, so resample-capable only. */
    {
        pjmedia_port *p2c = NULL;

        status = pjmedia_conf_detach_port(conf, slot2);
        if (status != PJ_SUCCESS) { rc = -75; goto on_return; }
        pump(master, 2);
        status = pjmedia_null_port_create(pool, ALT_RATE, CHANNELS, ALT_SPF,
                                          BPS, &p2c);
        if (status != PJ_SUCCESS) { rc = -76; goto on_return; }
        status = pjmedia_conf_replace_port(conf, pool, slot2, p2c);
        if (status != PJ_SUCCESS) { rc = -77; goto on_return; }
        pump(master, 5);
        if ((rc = check_state(conf, slot1, slot2, ALT_RATE,
                              "after-replace-diff-fmt")) != 0)
            goto on_return;
    }

    /* Repeatedly replace with alternating rates to exercise the switchable
     * buffer-pool reset path (which reclaims a generation only from the third
     * format change onward) and let ASan catch any leak/use-after-free. State
     * must survive every cycle. */
    {
        unsigned i;
        for (i = 0; i < 6; ++i) {
            unsigned r = (i & 1) ? CLOCK_RATE : ALT_RATE;
            unsigned spf = r / 100;
            pjmedia_port *pn = NULL;

            status = pjmedia_conf_detach_port(conf, slot2);
            if (status != PJ_SUCCESS) { rc = -80; goto on_return; }
            pump(master, 2);
            status = pjmedia_null_port_create(pool, r, CHANNELS, spf, BPS, &pn);
            if (status != PJ_SUCCESS) { rc = -81; goto on_return; }
            status = pjmedia_conf_replace_port(conf, pool, slot2, pn);
            if (status != PJ_SUCCESS) { rc = -82; goto on_return; }
            pump(master, 5);
            if ((rc = check_state(conf, slot1, slot2, r, "after-replace-loop"))!=0)
                goto on_return;
        }
        /* The last iteration (i=5) leaves slot2 attached at CLOCK_RATE, ready
         * for the rate-independent tests below. */
    }
#endif  /* HAS_RESAMPLE */

    /* Replace an ATTACHED (non-NULL) port directly, without a preceding
     * detach: op_replace_port() must detach the current port itself. */
    {
        pjmedia_port *pd1 = NULL;

        status = pjmedia_null_port_create(pool, CLOCK_RATE, CHANNELS, SPF, BPS,
                                          &pd1);
        if (status != PJ_SUCCESS) { rc = -90; goto on_return; }
        status = pjmedia_conf_replace_port(conf, pool, slot2, pd1);
        if (status != PJ_SUCCESS) { rc = -91; goto on_return; }
        pump(master, 3);
        if ((rc = check_state(conf, slot1, slot2, CLOCK_RATE, "direct-replace-same"))!=0)
            goto on_return;

#if HAS_RESAMPLE
        {
            pjmedia_port *pd2 = NULL;

            status = pjmedia_null_port_create(pool, ALT_RATE, CHANNELS, ALT_SPF,
                                              BPS, &pd2);
            if (status != PJ_SUCCESS) { rc = -92; goto on_return; }
            status = pjmedia_conf_replace_port(conf, pool, slot2, pd2);
            if (status != PJ_SUCCESS) { rc = -93; goto on_return; }
            pump(master, 5);
            if ((rc = check_state(conf, slot1, slot2, ALT_RATE,
                                  "direct-replace-diff")) != 0)
                goto on_return;

            /* Restore base rate for the capability-change test below. */
            status = pjmedia_null_port_create(pool, CLOCK_RATE, CHANNELS, SPF,
                                              BPS, &pd1);
            if (status != PJ_SUCCESS) { rc = -94; goto on_return; }
            status = pjmedia_conf_replace_port(conf, pool, slot2, pd1);
            if (status != PJ_SUCCESS) { rc = -95; goto on_return; }
            pump(master, 3);
        }
#endif  /* HAS_RESAMPLE */
    }

    /* Capability-changing replace: swap slot2 for a source-only port (no
     * put_frame), then back to a sink-capable port. On the parallel backend
     * this exercises the active_listener reconciliation (remove then re-add);
     * on the serial backend it is just two more replaces. State must survive
     * and ASan must stay clean. */
    {
        pjmedia_port *psrc = NULL, *psink = NULL;

        status = pjmedia_null_port_create(pool, CLOCK_RATE, CHANNELS, SPF, BPS,
                                          &psrc);
        if (status != PJ_SUCCESS) { rc = -100; goto on_return; }
        psrc->put_frame = NULL;      /* make it source-only (player-like) */
        status = pjmedia_conf_replace_port(conf, pool, slot2, psrc);
        if (status != PJ_SUCCESS) { rc = -101; goto on_return; }
        pump(master, 4);
        if ((rc = check_state(conf, slot1, slot2, CLOCK_RATE,
                              "replace-source-only")) != 0)
            goto on_return;

        status = pjmedia_null_port_create(pool, CLOCK_RATE, CHANNELS, SPF, BPS,
                                          &psink);
        if (status != PJ_SUCCESS) { rc = -102; goto on_return; }
        status = pjmedia_conf_replace_port(conf, pool, slot2, psink);
        if (status != PJ_SUCCESS) { rc = -103; goto on_return; }
        pump(master, 4);
        if ((rc = check_state(conf, slot1, slot2, CLOCK_RATE,
                              "replace-sink-again")) != 0)
            goto on_return;
    }

on_return:
    if (conf)
        pjmedia_conf_destroy(conf);
    if (pool)
        pj_pool_release(pool);
    return rc;
}

int conf_test(void)
{
    int rc;

    rc = detach_replace_test();
    if (rc == 1) {
        /* Skipped (backend unsupported) - treat as pass. */
        return 0;
    }
    if (rc != 0) {
        PJ_LOG(1,(THIS_FILE, "  conf detach/replace test failed (rc=%d)", rc));
        return rc;
    }
    return 0;
}

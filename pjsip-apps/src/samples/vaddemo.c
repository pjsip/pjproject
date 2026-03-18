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
 * vaddemo.c
 *
 * PURPOSE:
 *  Diagnostic tool for the pjmedia silence detector (VAD).
 *  Captures microphone audio and logs per-frame signal level
 *  and voiced/silence state. Use this to test and adjust VAD
 *  parameters (threshold, timing) for a specific environment
 *  before deploying in an application.
 *
 * USAGE:
 *  vaddemo [options]
 *
 *  Options:
 *   -r N          Clock rate in Hz (default: 16000)
 *   -p N          Ptime in ms (default: 20)
 *   -t N          Fixed threshold (0 = adaptive, default: 0)
 *   -L N          Log level (default: 3)
 */

#include <pjmedia.h>
#include <pjmedia-audiodev/audiodev.h>
#include <pjlib.h>
#include <pjlib-util.h>

#include <stdio.h>
#include <stdlib.h>

#define THIS_FILE       "vaddemo.c"

static struct app_t
{
    pj_caching_pool      cp;
    pj_pool_t           *pool;

    pjmedia_silence_det *vad;
    pjmedia_aud_stream  *strm;

    unsigned             clock_rate;
    unsigned             ptime;
    unsigned             channel_count;
    unsigned             samples_per_frame;
    pj_int32_t           fixed_threshold;
    int                  log_level;

    /* Stats */
    unsigned             frame_count;
    unsigned             voiced_count;
    unsigned             silence_count;
    pj_int32_t           level_min;
    pj_int32_t           level_max;
    pj_int64_t           level_sum;

    /* State tracking for transition logging */
    pj_bool_t            prev_silence;
    unsigned             state_frames;
    unsigned             sil_run;

    volatile pj_bool_t   running;
} app;


static pj_status_t rec_cb(void *user_data, pjmedia_frame *frame)
{
    pj_int32_t level;
    pj_bool_t is_silence;
    const pj_int16_t *samples = (const pj_int16_t *)frame->buf;
    unsigned sample_count;

    PJ_UNUSED_ARG(user_data);

    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO || !app.running)
        return PJ_SUCCESS;

    sample_count = (unsigned)(frame->size / 2);

    /* Run VAD with p_level output */
    is_silence = pjmedia_silence_det_detect(app.vad, samples,
                                            sample_count, &level);

    /* Update stats */
    app.frame_count++;
    if (is_silence)
        app.silence_count++;
    else
        app.voiced_count++;

    app.level_sum += level;
    if (level < app.level_min)
        app.level_min = level;
    if (level > app.level_max)
        app.level_max = level;

    /* Log frame with silence compression */
    {
        unsigned ms = app.frame_count * app.ptime;
        pj_bool_t is_transition;

        /* Detect state transitions */
        is_transition = (app.frame_count == 1 ||
                         is_silence != app.prev_silence);
        if (is_transition) {
            /* Print summary of previous silence run before transition */
            if (app.sil_run > 3 && app.prev_silence) {
                unsigned sil_end_ms = (app.frame_count - 1) * app.ptime;
                printf("[%3u.%03u] ... (%u silence frames, %ums)\n",
                       sil_end_ms / 1000, sil_end_ms % 1000,
                       app.sil_run, app.sil_run * app.ptime);
            }
            app.state_frames = 0;
            app.sil_run = 0;
        }
        app.state_frames++;
        app.prev_silence = is_silence;

        if (is_silence) {
            app.sil_run++;
            /* Print first 3 silence frames, then compress */
            if (app.sil_run <= 3) {
                char line[120];
                int len;

                len = pj_ansi_snprintf(line, sizeof(line),
                          "[%3u.%03u] lvl=%5d SIL%s\n",
                          ms / 1000, ms % 1000, (int)level,
                          (is_transition && app.frame_count > 1) ?
                          " << SILENCE" : "");
                if (len > 0)
                    fwrite(line, 1, len, stdout);
            }
        } else {
            /* Always print voiced frames with visual bar */
            char line[120];
            char bar[53];
            int bar_len = (int)(level * 50 / 5000);
            int len, i;

            if (bar_len > 50) bar_len = 50;
            bar[0] = '|';
            for (i = 0; i < 50; i++)
                bar[i + 1] = (i < bar_len) ? '#' : ' ';
            bar[51] = '|';
            bar[52] = '\0';

            len = pj_ansi_snprintf(line, sizeof(line),
                      "[%3u.%03u] lvl=%5d VOX %s%s\n",
                      ms / 1000, ms % 1000, (int)level, bar,
                      (is_transition && app.frame_count > 1) ?
                      " << VOICED" : "");
            if (len > 0)
                fwrite(line, 1, len, stdout);
        }
    }

    return PJ_SUCCESS;
}


static void print_usage(void)
{
    puts("vaddemo - Silence Detector (VAD) Diagnostic Tool");
    puts("");
    puts("Usage: vaddemo [options]");
    puts("");
    puts("Options:");
    puts("  -r N   Clock rate in Hz (default: 16000)");
    puts("  -p N   Ptime in ms (default: 20)");
    puts("  -t N   Fixed threshold (0 = adaptive, default: 0)");
    puts("  -L N   Log level (default: 3)");
    puts("  -h     Show this help");
    puts("");
    puts("Output format:");
    puts("  VOX frames: [time] lvl=N VOX |#### visual bar|");
    puts("  SIL frames: [time] lvl=N SIL  (first 3 shown, rest compressed)");
    puts("  << marks state transitions");
}


int main(int argc, char *argv[])
{
    pjmedia_aud_param param;
    int c;
    pj_status_t status;

    /* Defaults */
    pj_bzero(&app, sizeof(app));
    app.clock_rate = 16000;
    app.ptime = 20;
    app.channel_count = 1;
    app.fixed_threshold = 0;
    app.log_level = 3;
    app.level_min = 0x7FFFFFFF;
    app.level_max = 0;

    /* Parse args */
    while ((c = pj_getopt(argc, argv, "r:p:t:L:h")) != -1) {
        switch (c) {
        case 'r':
            app.clock_rate = (unsigned)atoi(pj_optarg);
            break;
        case 'p':
            app.ptime = (unsigned)atoi(pj_optarg);
            break;
        case 't':
            app.fixed_threshold = atoi(pj_optarg);
            break;
        case 'L':
            app.log_level = atoi(pj_optarg);
            break;
        case 'h':
            print_usage();
            return 0;
        default:
            print_usage();
            return 1;
        }
    }

    app.samples_per_frame = app.clock_rate * app.ptime / 1000;

    /* Init PJLIB */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pj_log_set_level(app.log_level);

    /* Init PJLIB-UTIL */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create pool factory and pool */
    pj_caching_pool_init(&app.cp, &pj_pool_factory_default_policy, 0);
    app.pool = pj_pool_create(&app.cp.factory, "vaddemo", 4096, 4096,
                              NULL);

    /* Init audio subsystem */
    status = pjmedia_aud_subsys_init(&app.cp.factory);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error init audio subsystem"));
        goto on_error;
    }

    /* Create silence detector */
    status = pjmedia_silence_det_create(app.pool, app.clock_rate,
                                        app.samples_per_frame, &app.vad);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error creating VAD"));
        goto on_error;
    }

    /* Set threshold mode */
    if (app.fixed_threshold > 0) {
        pjmedia_silence_det_set_fixed(app.vad, app.fixed_threshold);
        PJ_LOG(3, (THIS_FILE, "Using fixed threshold: %d",
                   app.fixed_threshold));
    } else {
        pjmedia_silence_det_set_adaptive(app.vad, -1);
        PJ_LOG(3, (THIS_FILE, "Using adaptive threshold"));
    }

    /* Open capture device */
    status = pjmedia_aud_dev_default_param(PJMEDIA_AUD_DEFAULT_CAPTURE_DEV,
                                           &param);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error getting default param"));
        goto on_error;
    }

    param.dir = PJMEDIA_DIR_CAPTURE;
    param.clock_rate = app.clock_rate;
    param.channel_count = app.channel_count;
    param.samples_per_frame = app.samples_per_frame;
    param.bits_per_sample = 16;

    status = pjmedia_aud_stream_create(&param, &rec_cb, NULL, NULL,
                                       &app.strm);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error opening capture device"));
        goto on_error;
    }

    /* Start capture */
    app.running = PJ_TRUE;

    status = pjmedia_aud_stream_start(app.strm);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Error starting capture"));
        goto on_error;
    }

    PJ_LOG(3, (THIS_FILE, "VAD demo started: rate=%uHz, ptime=%ums, "
               "spf=%u",
               app.clock_rate, app.ptime,
               app.samples_per_frame));
    PJ_LOG(3, (THIS_FILE, "Speak into your microphone. "
               "Press ENTER to stop."));
    puts("");
    printf("[  time ] lvl=    N STA "
           "|visual bar for voiced frames                  |\n");
    printf("---------------------------------------------"
           "--------------------------------------------------\n");

    /* Wait for ENTER */
    {
        char tmp[10];
        if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
            /* EOF */
        }
    }

    /* Stop */
    app.running = PJ_FALSE;
    pj_thread_sleep(100);

    /* Print trailing silence summary */
    if (app.sil_run > 3) {
        unsigned sil_end_ms = app.frame_count * app.ptime;
        printf("[%3u.%03u] ... (%u silence frames, %ums)\n",
               sil_end_ms / 1000, sil_end_ms % 1000,
               app.sil_run, app.sil_run * app.ptime);
    }

    /* Print summary */
    puts("");
    puts("=== Summary ===");
    printf("Total frames  : %u (%.1fs)\n",
           app.frame_count,
           (float)app.frame_count * app.ptime / 1000.0f);
    printf("Voiced frames : %u (%.1f%%)\n",
           app.voiced_count,
           app.frame_count ? (float)app.voiced_count * 100.0f /
                             app.frame_count : 0.0f);
    printf("Silence frames: %u (%.1f%%)\n",
           app.silence_count,
           app.frame_count ? (float)app.silence_count * 100.0f /
                             app.frame_count : 0.0f);
    printf("Level min/max/avg: %d / %d / %d\n",
           (int)app.level_min, (int)app.level_max,
           app.frame_count ? (int)(app.level_sum / app.frame_count) : 0);

on_error:
    if (app.strm) {
        pjmedia_aud_stream_stop(app.strm);
        pjmedia_aud_stream_destroy(app.strm);
    }

    pjmedia_aud_subsys_shutdown();

    if (app.pool)
        pj_pool_release(app.pool);

    pj_caching_pool_destroy(&app.cp);
    pj_shutdown();

    return 0;
}

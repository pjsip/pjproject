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
 * aidemo.c
 *
 * PURPOSE:
 *  Demonstrate the AI media port by connecting the local sound device
 *  (or null audio) to the OpenAI Realtime API. Captures microphone
 *  audio, sends it to OpenAI, and plays the AI response through the
 *  speaker.
 *
 * USAGE:
 *  aidemo [options]
 *
 *  The OPENAI_API_KEY environment variable must be set.
 *
 *  Options:
 *   -m URL        WebSocket URL for the OpenAI endpoint.
 *   -n            Use null audio (no sound device). Useful for
 *                 headless testing.
 *   -d seconds    Duration in seconds for null-audio mode (default 10).
 *   -L N          Set pjlib log level (0-6, default 4).
 */

#include <pjmedia.h>
#include <pjmedia/ai_port.h>
#include <pjlib-util.h>
#include <pjlib.h>

#include <stdio.h>
#include <stdlib.h>

#define THIS_FILE       "aidemo.c"

/* Audio ptime - port clock rate is derived from the AI backend */
#define PTIME_MS        20

/* Default sound device clock rate. 0 means match the AI backend's
 * native rate (e.g. 24kHz for OpenAI). Override with -r option.
 */
#define SND_CLOCK_RATE  0

/* Default OpenAI Realtime API URL */
#define DEFAULT_URL \
    "wss://api.openai.com/v1/realtime?model=gpt-4o-mini-realtime-preview"

/* Application data */
static struct app_t
{
    pj_caching_pool      cp;
    pj_pool_t           *pool;
    pjmedia_endpt       *med_endpt;

    pj_ioqueue_t        *ioqueue;
    pj_timer_heap_t     *timer_heap;
    pj_thread_t         *worker_thread;
    pj_bool_t            worker_quit;

    pjmedia_ai_backend  *backend;
    pjmedia_ai_port     *ai_port;

    /* Sound device mode */
    pjmedia_snd_port    *snd_port;
    pjmedia_port        *resamp_port;

    /* Null audio mode */
    pj_bool_t            null_audio;
    int                  duration;
    pjmedia_port        *null_port;
    pjmedia_master_port *master_port;

    pj_bool_t            connected;

    char                 url[512];
    int                  log_level;
    unsigned             snd_clock_rate;
} app;


static int app_perror(const char *sender, const char *title,
                      pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1, (sender, "%s: %s [code=%d]", title, errmsg, status));
    return 1;
}


/*
 * Worker thread: polls ioqueue and timer_heap for WebSocket I/O.
 * Uses a short poll timeout to ensure low-latency delivery of
 * WebSocket audio data.
 */
static int worker_thread_func(void *arg)
{
    PJ_UNUSED_ARG(arg);

    while (!app.worker_quit) {
        pj_time_val timeout = {0, 10};
        int c;

        c = pj_timer_heap_poll(app.timer_heap, &timeout);
        if (c == 0) {
            timeout.sec = 0;
            timeout.msec = 10;
        }
        if (timeout.msec > 10)
            timeout.msec = 10;

        pj_ioqueue_poll(app.ioqueue, &timeout);
    }

    return 0;
}


/*
 * AI port event callback.
 */
static void on_ai_event(pjmedia_ai_port *ai_port,
                        const pjmedia_ai_event *event)
{
    PJ_UNUSED_ARG(ai_port);

    switch (event->type) {
    case PJMEDIA_AI_EVENT_CONNECTED:
        PJ_LOG(3, (THIS_FILE, "Connected to AI service"));
        app.connected = PJ_TRUE;
        break;

    case PJMEDIA_AI_EVENT_DISCONNECTED:
        if (event->status == PJ_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Disconnected from AI service"));
        } else {
            PJ_PERROR(3, (THIS_FILE, event->status,
                          "AI service disconnected with error"));
        }
        app.connected = PJ_FALSE;
        break;

    case PJMEDIA_AI_EVENT_TRANSCRIPT:
        printf("\n[AI] %.*s", (int)event->text.slen, event->text.ptr);
        fflush(stdout);
        break;

    case PJMEDIA_AI_EVENT_RESPONSE_START:
        PJ_LOG(4, (THIS_FILE, "AI response started"));
        break;

    case PJMEDIA_AI_EVENT_RESPONSE_DONE:
        printf("\n");
        PJ_LOG(4, (THIS_FILE, "AI response done"));
        break;

    case PJMEDIA_AI_EVENT_SPEECH_STARTED:
        PJ_LOG(4, (THIS_FILE, "Speech detected"));
        break;

    case PJMEDIA_AI_EVENT_SPEECH_STOPPED:
        PJ_LOG(4, (THIS_FILE, "Speech ended"));
        break;
    }
}


static void print_usage(void)
{
    puts("aidemo - AI Media Port Demo (OpenAI Realtime API)");
    puts("");
    puts("Usage: aidemo [options]");
    puts("");
    puts("Options:");
    puts("  -m URL        WebSocket URL for OpenAI endpoint");
    puts("  -r N          Sound device clock rate in Hz (default: AI native)");
    puts("  -n            Use null audio (no sound device)");
    puts("  -d seconds    Duration for null-audio mode (default 10)");
    puts("  -L N          Set log level (0-6, default 4)");
    puts("");
    puts("Environment:");
    puts("  OPENAI_API_KEY   Required. Your OpenAI API key.");
    puts("");
    puts("The demo captures microphone audio, sends it to the OpenAI");
    puts("Realtime API, and plays the AI response through the speaker.");
    puts("With -n, runs headless with silence input for the specified");
    puts("duration. Press ENTER to quit (in normal mode).");
}


int main(int argc, char *argv[])
{
    const char *api_key;
    pj_str_t url_str;
    pj_str_t token_str;
    pjmedia_ai_port_param ai_param;
    pj_ssl_sock_param ssl_param;
    pjmedia_port *ai_media_port;
    unsigned ai_rate, ai_ccnt, ai_spf, ai_bps;
    char tmp[10];
    int c;
    pj_status_t status;

    /* Defaults */
    pj_bzero(&app, sizeof(app));
    pj_ansi_strxcpy(app.url, DEFAULT_URL, sizeof(app.url));
    app.log_level = 4;
    app.snd_clock_rate = SND_CLOCK_RATE;
    app.duration = 10;

    /* Parse args */
    while ((c = pj_getopt(argc, argv, "m:L:r:nd:h")) != -1) {
        switch (c) {
        case 'm':
            pj_ansi_strxcpy(app.url, pj_optarg, sizeof(app.url));
            break;
        case 'L':
            app.log_level = atoi(pj_optarg);
            break;
        case 'r':
            app.snd_clock_rate = (unsigned)atoi(pj_optarg);
            break;
        case 'n':
            app.null_audio = PJ_TRUE;
            break;
        case 'd':
            app.duration = atoi(pj_optarg);
            if (app.duration <= 0) app.duration = 10;
            break;
        case 'h':
            print_usage();
            return 0;
        default:
            print_usage();
            return 1;
        }
    }

    /* Get API key from environment */
    api_key = getenv("OPENAI_API_KEY");
    if (!api_key || !*api_key) {
        puts("Error: OPENAI_API_KEY environment variable is not set.");
        puts("");
        print_usage();
        return 1;
    }

    /* Init PJLIB */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pj_log_set_level(app.log_level);

    /* Init PJLIB-UTIL (needed for JSON, base64) */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create pool factory */
    pj_caching_pool_init(&app.cp, &pj_pool_factory_default_policy, 0);

    /* Create pool */
    app.pool = pj_pool_create(&app.cp.factory, "aidemo", 4096, 4096,
                              NULL);

    /* Create media endpoint with its own internal ioqueue.
     * Pass 0 worker threads since this demo doesn't use RTP.
     */
    status = pjmedia_endpt_create(&app.cp.factory, NULL, 0,
                                  &app.med_endpt);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Error creating media endpoint", status);
        goto on_error;
    }

    /* Create ioqueue and timer heap for WebSocket I/O */
    status = pj_ioqueue_create(app.pool, 16, &app.ioqueue);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Error creating ioqueue", status);
        goto on_error;
    }

    status = pj_timer_heap_create(app.pool, 100, &app.timer_heap);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Error creating timer heap", status);
        goto on_error;
    }

    /* Start worker thread for WebSocket ioqueue and timer polling */
    status = pj_thread_create(app.pool, "ai_worker",
                              &worker_thread_func, NULL,
                              0, 0, &app.worker_thread);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Error creating worker thread", status);
        goto on_error;
    }

    /* Create OpenAI backend */
    status = pjmedia_ai_openai_backend_create(app.pool, &app.backend);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Error creating OpenAI backend", status);
        goto on_error;
    }

    /* Create AI port.
     * Port rate is derived from the backend's native rate (24kHz for
     * OpenAI). The conference bridge / sound port handles resampling
     * if the sound device uses a different rate.
     */
    pjmedia_ai_port_param_default(&ai_param);
    ai_param.ioqueue = app.ioqueue;
    ai_param.timer_heap = app.timer_heap;
    ai_param.cb.on_event = &on_ai_event;
    ai_param.backend = app.backend;

    /* Enable TLS certificate verification for wss:// connections.
     * This prevents MITM attacks when sending the API key.
     */
    pj_ssl_sock_param_default(&ssl_param);
    ssl_param.verify_peer = PJ_TRUE;
    ai_param.ssl_param = &ssl_param;

    status = pjmedia_ai_port_create(app.pool, &ai_param, &app.ai_port);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Error creating AI port", status);
        goto on_error;
    }

    ai_media_port = pjmedia_ai_port_get_port(app.ai_port);

    /* Get port info to derive clock settings for sound device */
    ai_rate = PJMEDIA_PIA_SRATE(&ai_media_port->info);
    ai_ccnt = PJMEDIA_PIA_CCNT(&ai_media_port->info);
    ai_spf  = PJMEDIA_PIA_SPF(&ai_media_port->info);
    ai_bps  = PJMEDIA_PIA_BITS(&ai_media_port->info);

    if (app.null_audio) {
        /*
         * Null audio mode: use a null_port + master_port to drive
         * the clock. The null port provides silence as input and
         * discards output. Useful for headless/CI testing.
         */
        status = pjmedia_null_port_create(app.pool, ai_rate, ai_ccnt,
                                          ai_spf, ai_bps,
                                          &app.null_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Error creating null port", status);
            goto on_error;
        }

        /* Master port: upstream=null_port, downstream=ai_port.
         * This drives get_frame on null_port, put_frame on ai_port,
         * and vice versa every ptime.
         */
        status = pjmedia_master_port_create(app.pool,
                                            app.null_port,
                                            ai_media_port,
                                            0, &app.master_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Error creating master port", status);
            goto on_error;
        }

        status = pjmedia_master_port_start(app.master_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Error starting master port", status);
            goto on_error;
        }

        PJ_LOG(3, (THIS_FILE, "Null audio mode, duration=%ds",
                   app.duration));
    } else {
        unsigned snd_rate = app.snd_clock_rate ? app.snd_clock_rate
                                                : ai_rate;
        unsigned snd_spf = snd_rate * PTIME_MS / 1000;

        /* Create bidirectional sound port at a standard rate. */
        status = pjmedia_snd_port_create(
                    app.pool,
                    -1,                     /* capture dev (default) */
                    -1,                     /* playback dev (default) */
                    snd_rate,
                    ai_ccnt,
                    snd_spf,
                    ai_bps,
                    0,                      /* options */
                    &app.snd_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Error creating sound device", status);
            goto on_error;
        }

        /* Enable AEC to suppress speaker-to-mic echo. Without this,
         * the AI's own playback would be picked up by the microphone
         * and sent back, causing the server VAD to trigger turn
         * detection and cancel the AI response mid-sentence.
         * AEC allows barge-in (user can interrupt the AI naturally).
         */
        status = pjmedia_snd_port_set_ec(app.snd_port, app.pool,
                                          200, 0);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(3, (THIS_FILE, status,
                          "Warning: failed to enable AEC"));
        }

        /* Create resample port if sound device rate differs from
         * the AI port's native rate.
         */
        if (snd_rate != ai_rate) {
            status = pjmedia_resample_port_create(
                         app.pool, ai_media_port, snd_rate,
                         PJMEDIA_RESAMPLE_DONT_DESTROY_DN,
                         &app.resamp_port);
            if (status != PJ_SUCCESS) {
                app_perror(THIS_FILE, "Error creating resample port",
                           status);
                goto on_error;
            }
        }

        /* Connect sound port to AI port (via resample port if needed) */
        status = pjmedia_snd_port_connect(app.snd_port,
                                          app.resamp_port ?
                                          app.resamp_port :
                                          ai_media_port);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Error connecting sound to AI port",
                       status);
            goto on_error;
        }
    }

    /* Connect to OpenAI */
    PJ_LOG(3, (THIS_FILE, "Connecting to %s ...", app.url));

    url_str = pj_str(app.url);
    token_str = pj_str((char*)api_key);

    status = pjmedia_ai_port_connect(app.ai_port, &url_str,
                                     &token_str);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Error initiating AI connection", status);
        goto on_error;
    }

    /* Wait for connection */
    {
        int wait_ms = 0;
        while (!app.connected && wait_ms < 15000) {
            pj_thread_sleep(100);
            wait_ms += 100;
        }
        if (!app.connected) {
            PJ_LOG(1, (THIS_FILE,
                       "Timeout waiting for AI service connection"));
            goto on_error;
        }
    }

    PJ_LOG(3, (THIS_FILE, "AI service connected."));

    if (app.null_audio) {
        /* In null audio mode, run for the specified duration */
        PJ_LOG(3, (THIS_FILE, "Running with null audio for %d seconds..",
                   app.duration));
        pj_thread_sleep(app.duration * 1000);
        PJ_LOG(3, (THIS_FILE, "Duration elapsed, shutting down"));
    } else {
        puts("");
        puts("==========================================");
        puts("  AI Demo - OpenAI Realtime API");
        puts("  Speak into your microphone.");
        puts("  AI responses will be played back and");
        puts("  transcripts shown below.");
        puts("  Press ENTER to quit.");
        puts("==========================================");
        puts("");

        if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
            puts("EOF on stdin, quitting..");
        }
    }

    /* Cleanup */
on_error:
    PJ_LOG(3, (THIS_FILE, "Shutting down.."));

    /* Stop the audio clock first so put_frame/get_frame stop */
    if (app.master_port)
        pjmedia_master_port_stop(app.master_port);

    if (app.snd_port)
        pjmedia_snd_port_disconnect(app.snd_port);

    /* Disconnect AI service while worker thread still polls ioqueue,
     * so the WebSocket close handshake can complete.
     */
    if (app.ai_port && app.connected) {
        pjmedia_ai_port_disconnect(app.ai_port);
        pj_thread_sleep(500);
    }

    /* Now stop worker thread */
    if (app.worker_thread) {
        app.worker_quit = PJ_TRUE;
        pj_thread_join(app.worker_thread);
        pj_thread_destroy(app.worker_thread);
    }

    /* Destroy master port (null audio mode) */
    if (app.master_port)
        pjmedia_master_port_destroy(app.master_port, PJ_FALSE);

    /* Destroy null port */
    if (app.null_port)
        pjmedia_port_destroy(app.null_port);

    /* Destroy sound port */
    if (app.snd_port)
        pjmedia_snd_port_destroy(app.snd_port);

    /* Destroy resample port (does NOT destroy downstream AI port
     * because PJMEDIA_RESAMPLE_DONT_DESTROY_DN was used).
     */
    if (app.resamp_port)
        pjmedia_port_destroy(app.resamp_port);

    /* Destroy AI port (releases websock, pools, grp_lock) */
    if (app.ai_port)
        pjmedia_port_destroy(pjmedia_ai_port_get_port(app.ai_port));

    /* Destroy media endpoint */
    if (app.med_endpt)
        pjmedia_endpt_destroy(app.med_endpt);

    /* Destroy WebSocket ioqueue and timer heap */
    if (app.ioqueue)
        pj_ioqueue_destroy(app.ioqueue);
    if (app.timer_heap)
        pj_timer_heap_destroy(app.timer_heap);

    /* Release pool */
    if (app.pool)
        pj_pool_release(app.pool);

    pj_caching_pool_destroy(&app.cp);
    pj_shutdown();

    return 0;
}

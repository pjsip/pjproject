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
 * Fuzzer for the WAV (RIFF) and AVI container file-format parsers in pjmedia.
 *
 * pjmedia_wav_player_port_create() (wav_player.c / wave.c) and
 * pjmedia_avi_player_create_streams() (avi_player.c) both parse fully
 * attacker-controlled file headers: RIFF/WAVE chunk walking, fmt/data
 * sub-chunk sizes, and AVI LIST/strl/strf header descriptors.
 *
 * Both APIs take a path rather than a memory buffer, so the input is written
 * to a temporary file before parsing.
 */
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pjlib.h>

#include <pjmedia/wav_port.h>
#include <pjmedia/avi_stream.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/converter.h>
#include <pjmedia/event.h>
#include <pjmedia/format.h>

#define kMinInputLength 16
#define kMaxInputLength 16384

static pj_caching_pool caching_pool;
static pjmedia_endpt  *med_endpt;

/* Write the fuzz input to a temporary file and return the path in name_buf,
 * or PJ_FALSE on failure. */
static pj_bool_t write_temp_file(const uint8_t *Data, size_t Size,
                                 char *name_buf, size_t name_buf_len)
{
    int fd;
    size_t total;

    if (name_buf_len < 1)
        return PJ_FALSE;

    strncpy(name_buf, "/tmp/pjsip-fuzz-wav-XXXXXX", name_buf_len - 1);
    name_buf[name_buf_len - 1] = '\0';

    fd = mkstemp(name_buf);
    if (fd < 0)
        return PJ_FALSE;

    /* write() may be interrupted (EINTR) or perform a short write; loop until
     * the whole buffer is written so valid inputs are not dropped. */
    total = 0;
    while (total < Size) {
        ssize_t n = write(fd, Data + total, Size - total);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            close(fd);
            unlink(name_buf);
            return PJ_FALSE;
        }
        total += (size_t)n;
    }

    close(fd);

    return PJ_TRUE;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    static pj_bool_t initialized = PJ_FALSE;
    char filename[64];
    pj_pool_t *pool;
    pjmedia_port *wav_port;
    pjmedia_avi_streams *avi_streams;
    pj_status_t status;

    if (Size < kMinInputLength || Size > kMaxInputLength)
        return 0;

    /* One-time PJLIB / PJMEDIA initialisation. */
    if (!initialized) {
        pj_log_set_level(0);

        if (pj_init() != PJ_SUCCESS)
            return 0;

        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);

        /* A media endpoint registers codec/format managers; avoid audio init. */
        if (pjmedia_endpt_create2(&caching_pool.factory, NULL, 0,
                                  &med_endpt) != PJ_SUCCESS)
        {
            pj_caching_pool_destroy(&caching_pool);
            return 0;
        }

        /* pjmedia_endpt_create2() registers the video codec manager but not
         * the video format/converter/event managers. Without these, parsing an
         * AVI with a recognizable video stream calls pjmedia_get_video_format_info()
         * with a NULL manager and aborts (format.c: "Assertion mgr != NULL"),
         * so the AVI video header path could never be fuzzed. Create them here,
         * from a long-lived pool, so that path is actually exercised. */
        {
            pj_pool_t *vpool = pj_pool_create(&caching_pool.factory,
                                              "fuzz-wav-init",
                                              4000, 4000, NULL);
            if (vpool) {
                if (!pjmedia_video_format_mgr_instance())
                    pjmedia_video_format_mgr_create(vpool, 64, 0, NULL);
                if (!pjmedia_converter_mgr_instance())
                    pjmedia_converter_mgr_create(vpool, NULL);
                if (!pjmedia_event_mgr_instance())
                    pjmedia_event_mgr_create(vpool, 0, NULL);
            }
        }

        initialized = PJ_TRUE;
    }

    if (!write_temp_file(Data, Size, filename, sizeof(filename)))
        return 0;

    pool = pj_pool_create(&caching_pool.factory, "fuzz-wav", 4000, 4000, NULL);
    if (!pool) {
        unlink(filename);
        return 0;
    }

    /* Parse as a WAV/RIFF file. */
    wav_port = NULL;
    status = pjmedia_wav_player_port_create(pool, filename, 0, 0, 0, &wav_port);
    if (status == PJ_SUCCESS && wav_port) {
        pjmedia_wav_player_info info;
        pjmedia_wav_player_get_info(wav_port, &info);
        pjmedia_port_destroy(wav_port);
    }

    /* Parse as an AVI file. */
    avi_streams = NULL;
    status = pjmedia_avi_player_create_streams(pool, filename, 0, &avi_streams);
    if (status == PJ_SUCCESS && avi_streams) {
        unsigned i, num = pjmedia_avi_streams_get_num_streams(avi_streams);
        for (i = 0; i < num; ++i) {
            pjmedia_avi_stream *stream =
                pjmedia_avi_streams_get_stream(avi_streams, i);
            if (stream) {
                pjmedia_port *port = pjmedia_avi_stream_get_port(stream);
                if (port)
                    pjmedia_port_destroy(port);
            }
        }
    }

    pj_pool_release(pool);
    unlink(filename);

    return 0;
}

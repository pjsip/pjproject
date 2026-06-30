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
 * Fuzz target for the SDP "fmtp" (format parameters) parser
 * pjmedia_stream_info_parse_fmtp_data() in
 * pjmedia/src/pjmedia/stream_common.c.
 *
 * An SDP "a=fmtp:<pt> <params>" line carries codec-specific parameters that
 * arrive from a remote, untrusted peer (e.g. H.264 profile-level-id, Opus
 * minptime/useinbandfec, AMR octet-align/mode-set). pjmedia tokenises that
 * raw parameter string into name/value pairs. This parsing code is not
 * exercised by any existing harness, so stream_common.c is reported at 0%
 * coverage. The harness feeds the fuzz input directly as the fmtp parameter
 * string.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>
#include <pjmedia.h>
#include <pjmedia/stream_common.h>

#define kMinInputLength 2
#define kMaxInputLength 5120

pj_pool_factory *mem;

int Fmtp_parse(uint8_t *data, size_t Size) {

    pj_pool_t *pool;
    pjmedia_codec_fmtp fmtp;
    pj_str_t str;

    pool = pj_pool_create(mem, "fmtp", 1000, 1000, NULL);

    pj_bzero(&fmtp, sizeof(fmtp));
    str.ptr = (char *)data;
    str.slen = (pj_ssize_t)Size;

    /* Parse the attacker-controlled fmtp parameter string. */
    pjmedia_stream_info_parse_fmtp_data(pool, &str, &fmtp);

    pj_pool_release(pool);
    return 0;
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    uint8_t *data;
    pj_caching_pool caching_pool;

    /* Add NULL byte */
    data = (uint8_t *)calloc((Size+1), sizeof(uint8_t));
    memcpy((void *)data, (void *)Data, Size);

    /* init Calls */
    pj_init();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    mem = &caching_pool.factory;

    /* Call fuzzer */
    Fmtp_parse(data, Size);

    free(data);
    pj_caching_pool_destroy(&caching_pool);

    return 0;
}

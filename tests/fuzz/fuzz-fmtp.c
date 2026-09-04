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
 * Fuzz target for the SDP "fmtp" (format parameters) parser in
 * pjmedia/src/pjmedia/stream_common.c.
 *
 * An SDP "a=fmtp:<pt> <params>" line carries codec-specific parameters that
 * arrive from a remote, untrusted peer (e.g. H.264 profile-level-id, Opus
 * minptime/useinbandfec, AMR octet-align/mode-set). pjmedia tokenises that
 * raw parameter string into name/value pairs. None of this is exercised by
 * any existing harness, so stream_common.c is reported at 0% coverage.
 *
 * The harness drives the fmtp code from both ends:
 *
 *   1. pjmedia_stream_info_parse_fmtp_data() is fed the fuzz input directly
 *      as a raw parameter string. This keeps the tokeniser reachable from
 *      unstructured input, without depending on the input being valid SDP.
 *
 *   2. pjmedia_stream_info_parse_fmtp() is driven from a parsed SDP session,
 *      once per (media, format) pair. This additionally covers the attribute
 *      lookup and pjmedia_sdp_attr_get_fmtp(), which is how the parameter
 *      string is reached in production.
 */
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>
#include <pjmedia.h>

#include <pjmedia/sdp.h>
#include <pjmedia/stream_common.h>

static pj_caching_pool caching_pool;
static pj_pool_factory *mem;

/* Sink for parsed results, so that the name/value ranges handed back by the
 * parser are actually read. Without this the compiler is free to discard the
 * accesses and ASan never validates the returned pj_str_t ranges.
 */
static volatile unsigned long g_sink;

static void consume_fmtp(const pjmedia_codec_fmtp *fmtp)
{
    unsigned long sum = 0;
    unsigned i;

    for (i = 0; i < fmtp->cnt && i < PJMEDIA_CODEC_MAX_FMTP_CNT; ++i) {
        const pj_str_t *name = &fmtp->param[i].name;
        const pj_str_t *val = &fmtp->param[i].val;
        pj_ssize_t j;

        for (j = 0; j < name->slen; ++j)
            sum += (unsigned char)name->ptr[j];
        for (j = 0; j < val->slen; ++j)
            sum += (unsigned char)val->ptr[j];
    }

    g_sink += sum;
}

/* Feed the input straight to the parameter tokeniser.
 *
 * pjmedia_stream_info_parse_fmtp_data() takes an explicit slen and never
 * relies on NUL termination, so the fuzz input is passed through without a
 * copy.
 */
static void fuzz_fmtp_data(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    pjmedia_codec_fmtp fmtp;
    pj_str_t str;

    pj_bzero(&fmtp, sizeof(fmtp));
    str.ptr = (char *)data;
    str.slen = (pj_ssize_t)size;

    if (pjmedia_stream_info_parse_fmtp_data(pool, &str, &fmtp) == PJ_SUCCESS)
        consume_fmtp(&fmtp);
}

/* Reach the fmtp parser the way production code does: via an SDP media
 * description and its payload types.
 */
static void fuzz_fmtp_sdp(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    pjmedia_sdp_session *sdp;
    char *buf;
    unsigned mi;

    /* pjmedia_sdp_parse() scans in place and retains pointers into the
     * buffer, so it needs a writable, NUL-terminated copy.
     */
    buf = (char *)pj_pool_alloc(pool, size + 1);
    if (!buf)
        return;
    pj_memcpy(buf, data, size);
    buf[size] = '\0';

    if (pjmedia_sdp_parse(pool, buf, size, &sdp) != PJ_SUCCESS)
        return;

    for (mi = 0; mi < sdp->media_count; ++mi) {
        const pjmedia_sdp_media *m = sdp->media[mi];
        unsigned ai, fi;

        if (!m)
            continue;

        /* Once per declared format: exercises the "fmtp" attribute lookup,
         * pjmedia_sdp_attr_get_fmtp() and the tokeniser.
         */
        for (fi = 0; fi < m->desc.fmt_count && fi < PJMEDIA_MAX_SDP_FMT; ++fi) {
            pjmedia_codec_fmtp fmtp;
            unsigned long pt;

            pt = pj_strtoul(&m->desc.fmt[fi]);

            pj_bzero(&fmtp, sizeof(fmtp));
            if (pjmedia_stream_info_parse_fmtp(pool, m, (unsigned)pt,
                                               &fmtp) == PJ_SUCCESS)
                consume_fmtp(&fmtp);
        }

        /* Also convert every fmtp attribute directly, so that attributes
         * whose format does not appear in the m= line are still parsed.
         */
        for (ai = 0; ai < m->attr_count; ++ai) {
            pjmedia_sdp_attr *attr = m->attr[ai];
            pjmedia_sdp_fmtp sdp_fmtp;

            if (!attr || pj_strcmp2(&attr->name, "fmtp") != 0)
                continue;

            if (pjmedia_sdp_attr_get_fmtp(attr, &sdp_fmtp) == PJ_SUCCESS) {
                pjmedia_codec_fmtp fmtp;

                pj_bzero(&fmtp, sizeof(fmtp));
                if (pjmedia_stream_info_parse_fmtp_data(pool,
                                                        &sdp_fmtp.fmt_param,
                                                        &fmtp) == PJ_SUCCESS)
                    consume_fmtp(&fmtp);
            }
        }
    }
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    PJ_UNUSED_ARG(argc);
    PJ_UNUSED_ARG(argv);

    /* Silence logging before pj_init(), which itself logs at level 4. */
    pj_log_set_level(0);

    /* Initialise PJLIB once for the whole process. Doing this per iteration
     * would bump PJLIB's internal init counter on every execution without a
     * matching pj_shutdown().
     */
    if (pj_init() != PJ_SUCCESS)
        return 1;

    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    mem = &caching_pool.factory;

    return 0;
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    pj_pool_t *pool;

    pool = pj_pool_create(mem, "fuzz-fmtp", 2000, 2000, NULL);
    if (!pool)
        return 0;

    fuzz_fmtp_data(pool, Data, Size);
    fuzz_fmtp_sdp(pool, Data, Size);

    pj_pool_release(pool);
    return 0;
}

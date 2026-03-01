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

#include <pjlib.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>

#define CLOCK_RATE 8000
#define SAMPLES_PER_FRAME 160
#define CHANNEL_COUNT 1

static pj_caching_pool caching_pool;
static pjmedia_endpt *endpt = NULL;
static int initialized = 0;

static int init_media(void)
{
    pj_status_t status;

    if (initialized) {
        return 0;
    }

    pj_log_set_level(0);

    status = pj_init();
    if (status != PJ_SUCCESS) {
        return -1;
    }

    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);

    status = pjmedia_endpt_create(&caching_pool.factory, NULL, 1, &endpt);
    if (status != PJ_SUCCESS) {
        return -1;
    }

    pjmedia_codec_g711_init(endpt);
    pjmedia_codec_gsm_init(endpt);
    pjmedia_codec_ilbc_init(endpt, 30);

    initialized = 1;
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    if (Size < 10) {
        return 0;
    }

    if (init_media() != 0) {
        return 0;
    }

    pj_pool_t *pool = pj_pool_create(&caching_pool.factory, "fuzz", 4000, 4000, NULL);
    if (!pool) {
        return 0;
    }

    /* Select codec based on fuzzer input */
    unsigned codec_id = Data[0] % 4;
    pjmedia_codec_info codec_info;
    unsigned samples_needed;

    pj_bzero(&codec_info, sizeof(codec_info));
    codec_info.type = PJMEDIA_TYPE_AUDIO;
    codec_info.clock_rate = CLOCK_RATE;
    codec_info.channel_cnt = CHANNEL_COUNT;

    switch (codec_id) {
        case 0:
            codec_info.pt = PJMEDIA_RTP_PT_PCMU;
            codec_info.encoding_name = pj_str("PCMU");
            samples_needed = 80;
            break;
        case 1:
            codec_info.pt = PJMEDIA_RTP_PT_PCMA;
            codec_info.encoding_name = pj_str("PCMA");
            samples_needed = 80;
            break;
        case 2:
            codec_info.pt = PJMEDIA_RTP_PT_GSM;
            codec_info.encoding_name = pj_str("GSM");
            samples_needed = 160;
            break;
        case 3:
            codec_info.pt = 97;
            codec_info.encoding_name = pj_str("iLBC");
            samples_needed = 240;
            break;
        default:
            pj_pool_release(pool);
            return 0;
    }

    /* Get codec manager */
    pjmedia_codec_mgr *mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!mgr) {
        pj_pool_release(pool);
        return 0;
    }

    /* Verify sufficient input size for codec */
    size_t offset = 1;
    if (Size <= offset + samples_needed * 2) {
        pj_pool_release(pool);
        return 0;
    }

    /* Get default codec parameters */
    pjmedia_codec *codec = NULL;
    pjmedia_codec_param codec_param;

    pj_status_t status = pjmedia_codec_mgr_get_default_param(mgr, &codec_info, &codec_param);
    if (status != PJ_SUCCESS) {
        pj_pool_release(pool);
        return 0;
    }

    /* Allocate codec instance */
    status = pjmedia_codec_mgr_alloc_codec(mgr, &codec_info, &codec);
    if (status != PJ_SUCCESS || !codec) {
        pj_pool_release(pool);
        return 0;
    }

    /* Initialize codec */
    status = pjmedia_codec_init(codec, pool);
    if (status != PJ_SUCCESS) {
        pjmedia_codec_mgr_dealloc_codec(mgr, codec);
        pj_pool_release(pool);
        return 0;
    }

    /* Open codec with parameters */
    status = pjmedia_codec_open(codec, &codec_param);
    if (status != PJ_SUCCESS) {
        pjmedia_codec_mgr_dealloc_codec(mgr, codec);
        pj_pool_release(pool);
        return 0;
    }

    /* Prepare input and output frames */
    pjmedia_frame input;
    pjmedia_frame output;
    pj_int16_t pcm_in[320];
    pj_uint8_t encoded[300];

    pj_memcpy(pcm_in, Data + offset, PJ_MIN(samples_needed * 2, Size - offset));
    offset += samples_needed * 2;

    input.type = PJMEDIA_FRAME_TYPE_AUDIO;
    input.buf = pcm_in;
    input.size = samples_needed * 2;
    input.timestamp.u64 = 0;

    output.buf = encoded;
    output.size = sizeof(encoded);

    /* Test encode operation */
    status = pjmedia_codec_encode(codec, &input, sizeof(encoded), &output);

    /* Test decode operation if encode succeeded */
    if (status == PJ_SUCCESS && output.size > 0 && output.size < sizeof(encoded)) {
        /* G.711 requires exact frame size of 80 bytes */
        if (codec_id >= 2 || output.size == 80) {
            pjmedia_frame dec_input;
            pjmedia_frame dec_output;
            pj_int16_t pcm_out[320];

            dec_input.type = PJMEDIA_FRAME_TYPE_AUDIO;
            dec_input.buf = output.buf;
            dec_input.size = output.size;
            dec_input.timestamp.u64 = 0;

            dec_output.buf = pcm_out;
            dec_output.size = sizeof(pcm_out);

            pjmedia_codec_decode(codec, &dec_input, sizeof(pcm_out), &dec_output);
        }
    }

    /* Cleanup */
    pjmedia_codec_close(codec);
    pjmedia_codec_mgr_dealloc_codec(mgr, codec);
    pj_pool_release(pool);

    return 0;
}

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
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjmedia-codec/g722.h>
#include <pjmedia-codec/gsm.h>
#include <pjmedia-codec/speex.h>
#include <pjmedia-codec/ilbc.h>
#include <pjmedia-codec/l16.h>

/* Codec configuration structure */
typedef struct {
    pjmedia_codec **codec_ptr;
    const char *name;
    size_t min_frame_size;
    size_t pcm_frame_size;
} codec_config_t;

/* Global state for persistent fuzzing */
static pj_caching_pool caching_pool;
static pjmedia_endpt *endpt = NULL;
static pj_pool_t *pool = NULL;
static pjmedia_codec_mgr *codec_mgr = NULL;
static int initialized = 0;

/* Codec instances */
static pjmedia_codec *codec_pcma = NULL;
static pjmedia_codec *codec_pcmu = NULL;
static pjmedia_codec *codec_g722 = NULL;
static pjmedia_codec *codec_gsm = NULL;
static pjmedia_codec *codec_speex = NULL;
static pjmedia_codec *codec_ilbc = NULL;
static pjmedia_codec *codec_l16_8k = NULL;
static pjmedia_codec *codec_l16_16k = NULL;

/* Codec configurations array */
static codec_config_t codec_configs[] = {
    {&codec_pcma,     "G.711 A-Law", 80,  160},
    {&codec_pcmu,     "G.711 U-Law", 80,  160},
    {&codec_g722,     "G.722",       80,  320},
    {&codec_gsm,      "GSM",         33,  320},
    {&codec_speex,    "Speex",       10,  320},
    {&codec_ilbc,     "iLBC",        38,  480},
    {&codec_l16_8k,   "L16 8kHz",    160, 160},
    {&codec_l16_16k,  "L16 16kHz",   320, 320},
};

#define NUM_CODECS (sizeof(codec_configs) / sizeof(codec_configs[0]))

/* Codec ID strings for initialization */
static const char* codec_id_strings[] = {
    "PCMA/8000/1",
    "PCMU/8000/1",
    "G722/16000/1",
    "GSM/8000/1",
    "speex/8000/1",
    "iLBC/8000/1",
    "L16/8000/1",
    "L16/16000/1"
};

/* Helper function to allocate and open a codec */
static void alloc_codec(const char *id_str, pjmedia_codec **codec_ptr)
{
    pj_status_t status;
    pjmedia_codec_param param;
    const pjmedia_codec_info *codec_info;
    unsigned count = 1;
    pj_str_t codec_id = pj_str((char*)id_str);

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, &codec_id,
                                                  &count, &codec_info, NULL);
    if (status == PJ_SUCCESS && count > 0) {
        status = pjmedia_codec_mgr_get_default_param(codec_mgr, codec_info, &param);
        if (status == PJ_SUCCESS) {
            status = pjmedia_codec_mgr_alloc_codec(codec_mgr, codec_info, codec_ptr);
            if (status == PJ_SUCCESS && *codec_ptr) {
                pjmedia_codec_open(*codec_ptr, &param);
            }
        }
    }
}

static int init_codecs(void)
{
    pj_status_t status;
    size_t i;

    if (initialized) {
        return 0;
    }

    /* Initialize PJLIB */
    status = pj_init();
    if (status != PJ_SUCCESS) {
        return -1;
    }

    /* Initialize caching pool */
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);

    /* Create memory pool */
    pool = pj_pool_create(&caching_pool.factory, "fuzzer", 4000, 4000, NULL);
    if (!pool) {
        return -1;
    }

    /* Create media endpoint */
    status = pjmedia_endpt_create(&caching_pool.factory, NULL, 1, &endpt);
    if (status != PJ_SUCCESS) {
        return -1;
    }

    /* Get codec manager */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
        return -1;
    }

    /* Register all codecs */
    pjmedia_codec_g711_init(endpt);
    pjmedia_codec_g722_init(endpt);
    pjmedia_codec_gsm_init(endpt);
    pjmedia_codec_speex_init(endpt, 0, -1, -1);
    pjmedia_codec_ilbc_init(endpt, 30);
    pjmedia_codec_l16_init(endpt, 0);

    /* Allocate all codecs using the configuration array */
    for (i = 0; i < NUM_CODECS; i++) {
        alloc_codec(codec_id_strings[i], codec_configs[i].codec_ptr);
    }

    initialized = 1;
    return 0;
}

/* Helper function to test codec encode/decode cycle */
static void test_codec_cycle(pjmedia_codec *codec, const uint8_t *Data, size_t Size,
                              size_t min_frame_size, size_t pcm_frame_size)
{
    if (!codec || Size < min_frame_size) return;

    pj_status_t status;
    pjmedia_frame input_frame, output_frame;
    pj_int16_t pcm_buffer[4096] = {0};
    pj_uint8_t encoded_buffer[2048];

    /* Initialise pjmedia_frame */
    pj_bzero(&input_frame, sizeof(input_frame));
    pj_bzero(&output_frame, sizeof(output_frame));

    /* Test decode */
    input_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    input_frame.buf = (void*)Data;
    input_frame.size = min_frame_size;
    input_frame.timestamp.u64 = 0;

    output_frame.buf = pcm_buffer;
    output_frame.size = sizeof(pcm_buffer);

    status = pjmedia_codec_decode(codec, &input_frame, sizeof(pcm_buffer), &output_frame);

    /* If decode succeeded, try to encode the output back */
    if (status == PJ_SUCCESS && output_frame.type == PJMEDIA_FRAME_TYPE_AUDIO) {
        input_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        input_frame.buf = output_frame.buf;
        input_frame.size = output_frame.size;
        input_frame.timestamp.u64 = 0;

        output_frame.buf = encoded_buffer;
        output_frame.size = sizeof(encoded_buffer);

        pjmedia_codec_encode(codec, &input_frame, sizeof(encoded_buffer), &output_frame);
    }

    /* Test encode with fuzzer input as PCM - use proper frame size */
    if (Size >= pcm_frame_size && pcm_frame_size <= sizeof(pcm_buffer)) {
        memcpy(pcm_buffer, Data, pcm_frame_size);

        input_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        input_frame.buf = pcm_buffer;
        input_frame.size = pcm_frame_size;
        input_frame.timestamp.u64 = 0;

        output_frame.buf = encoded_buffer;
        output_frame.size = sizeof(encoded_buffer);

        pjmedia_codec_encode(codec, &input_frame, sizeof(encoded_buffer), &output_frame);
    }

    /* Test parse */
    if (codec->op->parse) {
        pjmedia_frame frames[256];
        unsigned frame_cnt = 256;

        input_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        input_frame.buf = (void*)Data;
        input_frame.size = Size;
        input_frame.timestamp.u64 = 0;

        status = pjmedia_codec_parse(codec, (void*)Data, Size, &input_frame.timestamp,
                                      &frame_cnt, frames);
        (void)status;
    }

    /* Test PLC if supported */
    if (codec->op->recover) {
        output_frame.buf = pcm_buffer;
        output_frame.size = sizeof(pcm_buffer);

        pjmedia_codec_recover(codec, sizeof(pcm_buffer), &output_frame);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    /* Initialize codecs on first run */
    if (init_codecs() != 0) {
        return 0;
    }

    const uint8_t *current_data = Data;
    size_t remaining_size = Size;
    size_t i;

    /* Test each codec sequentially with non-overlapping chunks of data */
    for (i = 0; i < NUM_CODECS; i++) {
        codec_config_t *config = &codec_configs[i];
        pjmedia_codec *codec = *(config->codec_ptr);

        /* Skip if codec not available or not enough data */
        if (!codec || remaining_size < config->min_frame_size) {
            continue;
        }

        /* Calculate chunk size for this codec (at least min_frame_size, max pcm_frame_size) */
        size_t chunk_size = config->pcm_frame_size;
        if (chunk_size > remaining_size) {
            chunk_size = remaining_size;
        }

        /* Test this codec with current data chunk */
        test_codec_cycle(codec, current_data, chunk_size,
                        config->min_frame_size, config->pcm_frame_size);

        /* Advance to next chunk of data */
        size_t consumed = config->min_frame_size;
        if (consumed > remaining_size) {
            consumed = remaining_size;
        }
        current_data += consumed;
        remaining_size -= consumed;

        /* Stop if we've run out of data */
        if (remaining_size < 10) {
            break;
        }
    }

    /* Test low-level G.711 conversion functions with remaining data */
    if (remaining_size > 1) {
        for (i = 0; i + 1 < remaining_size && i < 256; i += 2) {
            pj_int16_t pcm_val = (pj_int16_t)((current_data[i] << 8) | current_data[i+1]);

            /* G.711 conversions */
            pj_uint8_t alaw = pjmedia_linear2alaw(pcm_val);
            pj_uint8_t ulaw = pjmedia_linear2ulaw(pcm_val);
            pcm_val = pjmedia_alaw2linear(alaw);
            pcm_val = pjmedia_ulaw2linear(ulaw);
            ulaw = pjmedia_alaw2ulaw(current_data[i]);
            alaw = pjmedia_ulaw2alaw(current_data[i]);
        }
    }

    return 0;
}

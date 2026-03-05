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
#include <pjmedia-codec/h264_packetizer.h>
#include <pjmedia-codec/h263_packetizer.h>
#include <pjmedia-codec/vpx_packetizer.h>

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#define kMinInputLength 10
#define kMaxInputLength 5120

pj_pool_factory *mem;
static pj_caching_pool caching_pool;
static int initialized = 0;

/* Common context shared by all operations */
typedef struct {
    pj_pool_t *pool;
    uint8_t *data;
    size_t size;
    uint8_t *output;
    size_t output_size;
} codec_test_ctx;

/* Create a pool for codec operations */
static pj_pool_t* create_codec_pool(const char *name)
{
    return pj_pool_create(mem, name, 4000, 4000, NULL);
}

/* Helper to allocate and copy input buffer */
static pj_uint8_t* alloc_and_copy_buffer(pj_pool_t *pool, const uint8_t *data, size_t size)
{
    pj_uint8_t *buffer = (pj_uint8_t *)pj_pool_alloc(pool, size);
    if (buffer) {
        pj_memcpy(buffer, data, size);
    }
    return buffer;
}

/* Test H.264 codec */
static int test_h264_codec(int operation, codec_test_ctx *ctx)
{
    pjmedia_h264_packetizer_cfg cfg;
    pjmedia_h264_packetizer *pktz;
    unsigned bits_pos = 0;

    if (!ctx->pool) return -1;

    pj_bzero(&cfg, sizeof(cfg));
    cfg.mtu = 1500;
    cfg.unpack_nal_start = 4;
    cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_NON_INTERLEAVED;

    if (pjmedia_h264_packetizer_create(ctx->pool, &cfg, &pktz) != PJ_SUCCESS)
        return -1;

    if (operation == 0) {
        /* Unpacketize: RTP -> H.264 bitstream */
        pjmedia_h264_unpacketize(pktz, ctx->data, ctx->size, ctx->output,
                                 ctx->output_size, &bits_pos);
    } else {
        /* Packetize: H.264 bitstream -> RTP */
        pj_uint8_t *buffer = alloc_and_copy_buffer(ctx->pool, ctx->data, ctx->size);
        if (buffer) {
            const pj_uint8_t *payloads[100];
            pj_size_t payload_lens[100];
            unsigned payload_cnt = 100;
            pjmedia_h264_packetize(pktz, buffer, ctx->size, &payload_cnt,
                                   payloads, payload_lens);
        }
    }

    return 0;
}

/* Test H.263 codec */
static int test_h263_codec(int operation, codec_test_ctx *ctx)
{
    pjmedia_h263_packetizer_cfg cfg;
    pjmedia_h263_packetizer *pktz;
    unsigned bits_pos = 0;

    if (!ctx->pool) return -1;

    pj_bzero(&cfg, sizeof(cfg));
    cfg.mtu = 1500;
    cfg.mode = PJMEDIA_H263_PACKETIZER_MODE_RFC4629;

    if (pjmedia_h263_packetizer_create(ctx->pool, &cfg, &pktz) != PJ_SUCCESS)
        return -1;

    if (operation == 0) {
        /* Unpacketize: RTP -> H.263 bitstream */
        pjmedia_h263_unpacketize(pktz, ctx->data, ctx->size, ctx->output,
                                 ctx->output_size, &bits_pos);
    } else {
        /* Packetize: H.263 bitstream -> RTP */
        pj_uint8_t *buffer = alloc_and_copy_buffer(ctx->pool, ctx->data, ctx->size);
        pj_uint8_t *payload_buf = (pj_uint8_t *)pj_pool_alloc(ctx->pool, 1500);
        if (buffer && payload_buf) {
            pj_size_t payload_len = 1500;
            pjmedia_h263_packetize(pktz, buffer, ctx->size, &bits_pos,
                                   &payload_buf, &payload_len);
        }
    }

    return 0;
}

/* Test VPX codec */
static int test_vpx_codec(int operation, codec_test_ctx *ctx)
{
    pjmedia_vpx_packetizer_cfg cfg;
    pjmedia_vpx_packetizer *pktz;

    if (!ctx->pool) return -1;

    pj_bzero(&cfg, sizeof(cfg));
    pjmedia_vpx_packetizer_cfg_default(&cfg);
    cfg.mtu = 1500;

    if (pjmedia_vpx_packetizer_create(ctx->pool, &cfg, &pktz) != PJ_SUCCESS)
        return -1;

    if (operation == 0) {
        /* Unpacketize: RTP -> VPX bitstream */
        unsigned desc_len = 0;
        pjmedia_vpx_unpacketize(pktz, ctx->data, ctx->size, &desc_len);
    } else {
        /* Packetize: VPX bitstream -> RTP */
        if (ctx->size >= 20) {
            pj_uint8_t *payload;
            pj_size_t payload_len;
            unsigned bits_pos = 0;
            payload = (pj_uint8_t *)pj_pool_alloc(ctx->pool, 1500);
            payload_len = 1500;
            pjmedia_vpx_packetize(pktz, ctx->size, &bits_pos, PJ_TRUE,
                                  &payload, &payload_len);
        }
    }

    return 0;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    codec_test_ctx ctx;

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 0;
    }

    /* Initialize pjlib once */
    if (!initialized) {
        pj_init();
        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
        pj_log_set_level(0);
        mem = &caching_pool.factory;
        initialized = 1;
    }

    /* Allocate buffers */
    ctx.data = (uint8_t *)calloc(Size, sizeof(uint8_t));
    if (!ctx.data) return 0;
    memcpy((void *)ctx.data, Data, Size);
    ctx.size = Size;

    ctx.output_size = Size + 1024;
    ctx.output = (uint8_t *)calloc(ctx.output_size, sizeof(uint8_t));
    if (!ctx.output) {
        free((void *)ctx.data);
        return 0;
    }

    /* Test all video codec packetizers with the SAME input data */
#define TEST_CODEC(name, func, op) \
    ctx.pool = create_codec_pool(name); \
    if (ctx.pool) { \
        func(op, &ctx); \
        pj_pool_release(ctx.pool); \
    }

    TEST_CODEC("h264_u", test_h264_codec, 0);
    TEST_CODEC("h264_p", test_h264_codec, 1);
    TEST_CODEC("h263_u", test_h263_codec, 0);
    TEST_CODEC("h263_p", test_h263_codec, 1);
    TEST_CODEC("vpx_u", test_vpx_codec, 0);
    TEST_CODEC("vpx_p", test_vpx_codec, 1);

#undef TEST_CODEC

    /* Cleanup */
    free((void *)ctx.data);
    free(ctx.output);

    return 0;
}

#else
/* If video support is not compiled, provide a stub */
extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    (void)Data;
    (void)Size;
    return 0;
}
#endif

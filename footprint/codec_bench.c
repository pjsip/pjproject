/*
 * codec_bench.c -- encode+decode N frames of synthetic audio with one
 * pjmedia codec, for CPU-cost comparison (G.722 vs G.711).
 *
 * Usage: codec_bench <codec-id> <frames>     e.g.  codec_bench G722/16000 5000
 *
 * Run natively for wall-clock, or under `valgrind --tool=callgrind` for
 * instruction counts.  Two runs with different frame counts give the
 * per-frame cost with the fixed init cost subtracted.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>

#define THIS_FILE   "codec_bench.c"

int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *endpt;
    pjmedia_codec_mgr *mgr;
    const pjmedia_codec_info *ci[1];
    unsigned cnt = 1, i, n, samples, enc_bytes;
    pjmedia_codec *codec;
    pjmedia_codec_param param;
    pj_pool_t *pool;
    pj_str_t id;
    pj_int16_t *pcm, *pcm2;
    pj_uint8_t *enc;
    pjmedia_frame fin, fenc, fout;
    pj_timestamp t0, t1;
    pj_status_t status;

    if (argc < 3) {
        puts("usage: codec_bench <codec-id> <frames>");
        return 1;
    }
    id = pj_str(argv[1]);
    n = (unsigned)atoi(argv[2]);

    pj_log_set_level(1);
    status = pj_init();
    if (status != PJ_SUCCESS) return 1;
    status = pjlib_util_init();
    if (status != PJ_SUCCESS) return 1;
    pj_caching_pool_init(&cp, NULL, 0);

    status = pjmedia_endpt_create(&cp.factory, NULL, 0, &endpt);
    if (status != PJ_SUCCESS) return 1;
    pjmedia_codec_g711_init(endpt);
    pjmedia_codec_g722_init(endpt);

    mgr = pjmedia_endpt_get_codec_mgr(endpt);
    status = pjmedia_codec_mgr_find_codecs_by_id(mgr, &id, &cnt, ci, NULL);
    if (status != PJ_SUCCESS || cnt < 1) {
        printf("codec %s not found\n", argv[1]);
        return 1;
    }
    pjmedia_codec_mgr_get_default_param(mgr, ci[0], &param);
    param.setting.vad = 0;   /* measure codec cost, not silence detection */
    status = pjmedia_codec_mgr_alloc_codec(mgr, ci[0], &codec);
    if (status != PJ_SUCCESS) return 1;

    pool = pj_pool_create(&cp.factory, "bench", 8000, 8000, NULL);
    codec->op->init(codec, pool);
    status = codec->op->open(codec, &param);
    if (status != PJ_SUCCESS) return 1;

    samples = param.info.clock_rate * param.info.frm_ptime / 1000 *
              param.info.channel_cnt;
    enc_bytes = param.info.max_bps * param.info.frm_ptime / 8000 + 64;
    pcm  = (pj_int16_t*) pj_pool_alloc(pool, samples * 2);
    pcm2 = (pj_int16_t*) pj_pool_alloc(pool, samples * 2);
    enc  = (pj_uint8_t*) pj_pool_alloc(pool, enc_bytes);

    /* synthetic content: 440 Hz + 1.3 kHz tones, moderate level */
    for (i = 0; i < samples; ++i) {
        double t = (double)i / param.info.clock_rate;
        pcm[i] = (pj_int16_t)(8000 * sin(2 * 3.14159265 * 440 * t) +
                              4000 * sin(2 * 3.14159265 * 1300 * t));
    }

    pj_bzero(&fin, sizeof(fin)); pj_bzero(&fenc, sizeof(fenc));
    pj_bzero(&fout, sizeof(fout));
    fin.type = PJMEDIA_FRAME_TYPE_AUDIO; fin.buf = pcm; fin.size = samples * 2;

    pj_get_timestamp(&t0);
    for (i = 0; i < n; ++i) {
        fenc.buf = enc; fenc.size = enc_bytes;
        status = codec->op->encode(codec, &fin, enc_bytes, &fenc);
        if (status != PJ_SUCCESS) { printf("encode error %d\n", status); return 1; }
        if (fenc.size == 0) continue;
        fout.buf = pcm2; fout.size = samples * 2;
        status = codec->op->decode(codec, &fenc, samples * 2, &fout);
        if (status != PJ_SUCCESS) { printf("decode error %d\n", status); return 1; }
    }
    pj_get_timestamp(&t1);

    printf("%s: %u frames of %u samples (%u ms) enc+dec in %u us "
           "(%.2f us/frame, %u enc bytes/frame)\n",
           argv[1], n, samples, param.info.frm_ptime,
           pj_elapsed_usec(&t0, &t1),
           (double)pj_elapsed_usec(&t0, &t1) / n, (unsigned)fenc.size);

    codec->op->close(codec);
    pjmedia_codec_mgr_dealloc_codec(mgr, codec);
    pjmedia_endpt_destroy(endpt);
    pj_caching_pool_destroy(&cp);
    return 0;
}

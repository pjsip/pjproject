/*
 * Copyright (C)2020 Teluu Inc. (http://www.teluu.com)
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

#include <pjmedia-codec/and_vid_mediacodec.h>
#include <pjmedia-codec/h264_packetizer.h>
#include <pjmedia-codec/vpx_packetizer.h>
#include <pjmedia/vid_codec_util.h>
#include <pjmedia/errno.h>
#include <pj/log.h>

#if defined(PJMEDIA_HAS_ANDROID_MEDIACODEC) && \
            PJMEDIA_HAS_ANDROID_MEDIACODEC != 0 && \
    defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

/* Android AMediaCodec: */
#include "media/NdkMediaCodec.h"

/*
 * Constants
 */
#define THIS_FILE                   "and_vid_mediacodec.cpp"
#define AND_MEDIA_KEY_COLOR_FMT     "color-format"
#define AND_MEDIA_KEY_WIDTH         "width"
#define AND_MEDIA_KEY_HEIGHT        "height"
#define AND_MEDIA_KEY_BIT_RATE      "bitrate"
#define AND_MEDIA_KEY_PROFILE       "profile"
#define AND_MEDIA_KEY_FRAME_RATE    "frame-rate"
#define AND_MEDIA_KEY_IFR_INTTERVAL "i-frame-interval"
#define AND_MEDIA_KEY_MIME          "mime"
#define AND_MEDIA_KEY_REQUEST_SYNCF "request-sync"
#define AND_MEDIA_KEY_CSD0          "csd-0"
#define AND_MEDIA_KEY_CSD1          "csd-1"
#define AND_MEDIA_KEY_MAX_INPUT_SZ  "max-input-size"
#define AND_MEDIA_KEY_ENCODER       "encoder"
#define AND_MEDIA_KEY_PRIORITY      "priority"
#define AND_MEDIA_KEY_STRIDE        "stride"
#define AND_MEDIA_I420_PLANAR_FMT   0x13
#define AND_MEDIA_QUEUE_TIMEOUT     2000*100

#define DEFAULT_WIDTH           352
#define DEFAULT_HEIGHT          288

#define DEFAULT_FPS             15
#define DEFAULT_AVG_BITRATE     256000
#define DEFAULT_MAX_BITRATE     256000

#define SPS_PPS_BUF_SIZE        64

#define MAX_RX_WIDTH            1200
#define MAX_RX_HEIGHT           800

/* Maximum duration from one key frame to the next (in seconds). */
#define KEYFRAME_INTERVAL       1

#define CODEC_WAIT_RETRY        10
#define CODEC_THREAD_WAIT       10
/* Timeout until the buffer is ready in ms. */
#define CODEC_DEQUEUE_TIMEOUT   20

#define AND_MED_H264_PT         PJMEDIA_RTP_PT_H264_RSV2
#define AND_MED_VP8_PT          PJMEDIA_RTP_PT_VP8_RSV1
#define AND_MED_VP9_PT          PJMEDIA_RTP_PT_VP9_RSV1

/*
 * Factory operations.
 */
static pj_status_t and_media_test_alloc(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info );
static pj_status_t and_media_default_attr(pjmedia_vid_codec_factory *factory,
                                      const pjmedia_vid_codec_info *info,
                                      pjmedia_vid_codec_param *attr );
static pj_status_t and_media_enum_info(pjmedia_vid_codec_factory *factory,
                                   unsigned *count,
                                   pjmedia_vid_codec_info codecs[]);
static pj_status_t and_media_alloc_codec(pjmedia_vid_codec_factory *factory,
                                     const pjmedia_vid_codec_info *info,
                                     pjmedia_vid_codec **p_codec);
static pj_status_t and_media_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                       pjmedia_vid_codec *codec );


/*
 * Codec operations
 */
static pj_status_t and_media_codec_init(pjmedia_vid_codec *codec,
                                    pj_pool_t *pool );
static pj_status_t and_media_codec_open(pjmedia_vid_codec *codec,
                                    pjmedia_vid_codec_param *param );
static pj_status_t and_media_codec_close(pjmedia_vid_codec *codec);
static pj_status_t and_media_codec_modify(pjmedia_vid_codec *codec,
                                      const pjmedia_vid_codec_param *param);
static pj_status_t and_media_codec_get_param(pjmedia_vid_codec *codec,
                                         pjmedia_vid_codec_param *param);
static pj_status_t and_media_codec_encode_begin(pjmedia_vid_codec *codec,
                                            const pjmedia_vid_encode_opt *opt,
                                            const pjmedia_frame *input,
                                            unsigned out_size,
                                            pjmedia_frame *output,
                                            pj_bool_t *has_more);
static pj_status_t and_media_codec_encode_more(pjmedia_vid_codec *codec,
                                           unsigned out_size,
                                           pjmedia_frame *output,
                                           pj_bool_t *has_more);
static pj_status_t and_media_codec_decode(pjmedia_vid_codec *codec,
                                      pj_size_t count,
                                      pjmedia_frame packets[],
                                      unsigned out_size,
                                      pjmedia_frame *output);

/* Definition for Android AMediaCodec operations. */
static pjmedia_vid_codec_op and_media_codec_op =
{
    &and_media_codec_init,
    &and_media_codec_open,
    &and_media_codec_close,
    &and_media_codec_modify,
    &and_media_codec_get_param,
    &and_media_codec_encode_begin,
    &and_media_codec_encode_more,
    &and_media_codec_decode,
    NULL
};

/* Definition for Android AMediaCodec factory operations. */
static pjmedia_vid_codec_factory_op and_media_factory_op =
{
    &and_media_test_alloc,
    &and_media_default_attr,
    &and_media_enum_info,
    &and_media_alloc_codec,
    &and_media_dealloc_codec
};

static struct and_media_factory
{
    pjmedia_vid_codec_factory    base;
    pjmedia_vid_codec_mgr       *mgr;
    pj_pool_factory             *pf;
    pj_pool_t                   *pool;
} and_media_factory;

enum and_media_frm_type {
    AND_MEDIA_FRM_TYPE_DEFAULT = 0,
    AND_MEDIA_FRM_TYPE_KEYFRAME = 1,
    AND_MEDIA_FRM_TYPE_CONFIG = 2
};

typedef struct h264_codec_data {
    pjmedia_h264_packetizer     *pktz;

    pj_uint8_t                   enc_sps_pps_buf[SPS_PPS_BUF_SIZE];
    unsigned                     enc_sps_pps_len;
    pj_bool_t                    enc_sps_pps_ex;

    pj_uint8_t                  *dec_sps_buf;
    unsigned                     dec_sps_len;
    pj_uint8_t                  *dec_pps_buf;
    unsigned                     dec_pps_len;
} h264_codec_data;

typedef struct vpx_codec_data {
    pjmedia_vpx_packetizer      *pktz;
} vpx_codec_data;

typedef struct and_media_codec_data
{
    pj_pool_t                   *pool;
    pj_uint8_t                   codec_idx;
    pjmedia_vid_codec_param     *prm;
    pj_bool_t                    whole;
    void                        *ex_data;

    /* Encoder state */
    AMediaCodec                 *enc;
    unsigned                     enc_input_size;
    pj_uint8_t                  *enc_frame_whole;
    unsigned                     enc_frame_size;
    unsigned                     enc_processed;
    AMediaCodecBufferInfo        enc_buf_info;
    int                          enc_output_buf_idx;

    /* Decoder state */
    AMediaCodec                 *dec;
    pj_uint8_t                  *dec_buf;
    pj_uint8_t                  *dec_input_buf;
    unsigned                     dec_input_buf_len;
    pj_size_t                    dec_input_buf_max_size;
    pj_ssize_t                   dec_input_buf_idx;
    unsigned                     dec_has_output_frame;
    unsigned                     dec_stride_len;
    unsigned                     dec_buf_size;
    AMediaCodecBufferInfo        dec_buf_info;
} and_media_codec_data;

/* Custom callbacks. */

/* This callback is useful when specific method is needed when opening
 * the codec (e.g: applying fmtp or setting up the packetizer)
 */
typedef pj_status_t (*open_cb)(and_media_codec_data *and_media_data);

/* This callback is useful for handling configure frame produced by encoder.
 * Output frame might want to be stored the configuration frame and append it
 * to a keyframe for sending later (e.g: on H264 codec). The default behavior
 * is to send the configuration frame regardless.
 */
typedef pj_status_t (*process_encode_cb)(and_media_codec_data *and_media_data);

/* This callback is to process more encoded packets/payloads from the codec.*/
typedef pj_status_t(*encode_more_cb)(and_media_codec_data *and_media_data,
                                     unsigned out_size,
                                     pjmedia_frame *output,
                                     pj_bool_t *has_more);

/* This callback is to decode packets. */
typedef pj_status_t(*decode_cb)(pjmedia_vid_codec *codec,
                                pj_size_t count,
                                pjmedia_frame packets[],
                                unsigned out_size,
                                pjmedia_frame *output);


/* Custom callback implementation. */
#if PJMEDIA_HAS_AND_MEDIA_H264
static pj_status_t open_h264(and_media_codec_data *and_media_data);
static pj_status_t process_encode_h264(and_media_codec_data *and_media_data);
static pj_status_t encode_more_h264(and_media_codec_data *and_media_data,
                                    unsigned out_size,
                                    pjmedia_frame *output,
                                    pj_bool_t *has_more);
static pj_status_t decode_h264(pjmedia_vid_codec *codec,
                               pj_size_t count,
                               pjmedia_frame packets[],
                               unsigned out_size,
                               pjmedia_frame *output);
#endif

#if PJMEDIA_HAS_AND_MEDIA_VP8 || PJMEDIA_HAS_AND_MEDIA_VP9
static pj_status_t open_vpx(and_media_codec_data *and_media_data);
static pj_status_t encode_more_vpx(and_media_codec_data *and_media_data,
                                   unsigned out_size,
                                   pjmedia_frame *output,
                                   pj_bool_t *has_more);
static pj_status_t decode_vpx(pjmedia_vid_codec *codec,
                              pj_size_t count,
                              pjmedia_frame packets[],
                              unsigned out_size,
                              pjmedia_frame *output);
#endif


#if PJMEDIA_HAS_AND_MEDIA_H264
static pj_str_t H264_sw_encoder[] = {{(char *)"OMX.google.h264.encoder\0",
                                      23}};
static pj_str_t H264_hw_encoder[] =
                                  {{(char *)"OMX.qcom.video.encoder.avc\0", 26},
                                  {(char *)"OMX.Exynos.avc.Encoder\0", 22}};
static pj_str_t H264_sw_decoder[] = {{(char *)"OMX.google.h264.decoder\0",
                                      23}};
static pj_str_t H264_hw_decoder[] =
                                  {{(char *)"OMX.qcom.video.decoder.avc\0", 26},
                                  {(char *)"OMX.Exynos.avc.dec\0", 18}};
#endif

#if PJMEDIA_HAS_AND_MEDIA_VP8
static pj_str_t VP8_sw_encoder[] = {{(char *)"OMX.google.vp8.encoder\0", 23}};
static pj_str_t VP8_hw_encoder[] =
                                 {{(char *)"OMX.qcom.video.encoder.vp8\0", 26},
                                 {(char *)"OMX.Exynos.vp8.Encoder\0", 22}};
static pj_str_t VP8_sw_decoder[] = {{(char *)"OMX.google.vp8.decoder\0", 23}};
static pj_str_t VP8_hw_decoder[] =
                                 {{(char *)"OMX.qcom.video.decoder.vp8\0", 26},
                                 {(char *)"OMX.Exynos.vp8.dec\0", 18}};
#endif

#if PJMEDIA_HAS_AND_MEDIA_VP9
static pj_str_t VP9_sw_encoder[] = {{(char *)"OMX.google.vp9.encoder\0", 23}};
static pj_str_t VP9_hw_encoder[] =
                                 {{(char *)"OMX.qcom.video.encoder.vp9\0", 26},
                                 {(char *)"OMX.Exynos.vp9.Encoder\0", 22}};
static pj_str_t VP9_sw_decoder[] = {{(char *)"OMX.google.vp9.decoder\0", 23}};
static pj_str_t VP9_hw_decoder[] =
                                 {{(char *)"OMX.qcom.video.decoder.vp9\0", 26},
                                 {(char *)"OMX.Exynos.vp9.dec\0", 18}};
#endif

static struct and_media_codec {
    int                enabled;           /* Is this codec enabled?          */
    const char        *name;              /* Codec name.                     */
    const char        *description;       /* Codec description.              */
    const char        *mime_type;         /* Mime type.                      */
    pj_str_t          *encoder_name;      /* Encoder name.                   */
    pj_str_t          *decoder_name;      /* Decoder name.                   */
    pj_uint8_t         pt;                /* Payload type.                   */
    pjmedia_format_id  fmt_id;            /* Format id.                      */
    pj_uint8_t         keyframe_interval; /* Keyframe interval.              */

    open_cb            open_codec;
    process_encode_cb  process_encode;
    encode_more_cb     encode_more;
    decode_cb          decode;

    pjmedia_codec_fmtp dec_fmtp;          /* Decoder's fmtp params.          */
}
and_media_codec[] = {
#if PJMEDIA_HAS_AND_MEDIA_H264
    {0, "H264", "Android MediaCodec H264 codec", "video/avc",
        NULL, NULL,
        AND_MED_H264_PT, PJMEDIA_FORMAT_H264, KEYFRAME_INTERVAL,
        &open_h264, &process_encode_h264, &encode_more_h264, &decode_h264,
        {2, {{{(char *)"profile-level-id", 16}, {(char *)"42e01e", 6}},
             {{(char *)" packetization-mode", 19}, {(char *)"1", 1}}}
        }
    },
#endif
#if PJMEDIA_HAS_AND_MEDIA_VP8
    {0, "VP8",  "Android MediaCodec VP8 codec", "video/x-vnd.on2.vp8",
        NULL, NULL,
        AND_MED_VP8_PT, PJMEDIA_FORMAT_VP8, KEYFRAME_INTERVAL,
        &open_vpx, NULL, &encode_more_vpx, &decode_vpx,
        {2, {{{(char *)"max-fr", 6}, {(char *)"30", 2}},
             {{(char *)" max-fs", 7}, {(char *)"580", 3}}}
        }
    },
#endif
#if PJMEDIA_HAS_AND_MEDIA_VP9
    {0, "VP9",  "Android MediaCodec VP9 codec", "video/x-vnd.on2.vp9",
        NULL, NULL,
        AND_MED_VP9_PT, PJMEDIA_FORMAT_VP9, KEYFRAME_INTERVAL,
        &open_vpx, NULL, &encode_more_vpx, &decode_vpx,
        {2, {{{(char *)"max-fr", 6}, {(char *)"30", 2}},
             {{(char *)" max-fs", 7}, {(char *)"580", 3}}}
        }
    }
#endif
};

static pj_status_t configure_encoder(and_media_codec_data *and_media_data)
{
    media_status_t am_status;
    AMediaFormat *vid_fmt;
    pjmedia_vid_codec_param *param = and_media_data->prm;

    vid_fmt = AMediaFormat_new();
    if (!vid_fmt) {
        PJ_LOG(4, (THIS_FILE, "Encoder failed creating media format"));
        return PJ_ENOMEM;
    }

    AMediaFormat_setString(vid_fmt, AND_MEDIA_KEY_MIME,
                          and_media_codec[and_media_data->codec_idx].mime_type);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_COLOR_FMT,
                          AND_MEDIA_I420_PLANAR_FMT);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_HEIGHT,
                          param->enc_fmt.det.vid.size.h);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_WIDTH,
                          param->enc_fmt.det.vid.size.w);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_BIT_RATE,
                          param->enc_fmt.det.vid.avg_bps);
    //AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_PROFILE, 1);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_IFR_INTTERVAL,
                          KEYFRAME_INTERVAL);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_FRAME_RATE,
                          (param->enc_fmt.det.vid.fps.num /
                           param->enc_fmt.det.vid.fps.denum));
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_PRIORITY, 0);

    /* Configure and start encoder. */
    am_status = AMediaCodec_configure(and_media_data->enc, vid_fmt, NULL, NULL,
                                      AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    AMediaFormat_delete(vid_fmt);
    if (am_status != AMEDIA_OK) {
        PJ_LOG(4, (THIS_FILE, "Encoder configure failed, status=%d",
                   am_status));
        return PJMEDIA_CODEC_EFAILED;
    }
    am_status = AMediaCodec_start(and_media_data->enc);
    if (am_status != AMEDIA_OK) {
        PJ_LOG(4, (THIS_FILE, "Encoder start failed, status=%d",
                am_status));
        return PJMEDIA_CODEC_EFAILED;
    }
    return PJ_SUCCESS;
}

static pj_status_t configure_decoder(and_media_codec_data *and_media_data) {
    media_status_t am_status;
    AMediaFormat *vid_fmt;

    vid_fmt = AMediaFormat_new();
    if (!vid_fmt) {
        PJ_LOG(4, (THIS_FILE, "Decoder failed creating media format"));
        return PJ_ENOMEM;
    }
    AMediaFormat_setString(vid_fmt, AND_MEDIA_KEY_MIME,
                          and_media_codec[and_media_data->codec_idx].mime_type);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_COLOR_FMT,
                          AND_MEDIA_I420_PLANAR_FMT);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_HEIGHT,
                          and_media_data->prm->dec_fmt.det.vid.size.h);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_WIDTH,
                          and_media_data->prm->dec_fmt.det.vid.size.w);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_MAX_INPUT_SZ, 0);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_ENCODER, 0);
    AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_PRIORITY, 0);

    if (and_media_codec[and_media_data->codec_idx].fmt_id ==
        PJMEDIA_FORMAT_H264)
    {
        h264_codec_data *h264_data = (h264_codec_data *)and_media_data->ex_data;

        if (h264_data->dec_sps_len) {
            AMediaFormat_setBuffer(vid_fmt, AND_MEDIA_KEY_CSD0,
                                   h264_data->dec_sps_buf,
                                   h264_data->dec_sps_len);
        }
        if (h264_data->dec_pps_len) {
            AMediaFormat_setBuffer(vid_fmt, AND_MEDIA_KEY_CSD1,
                                   h264_data->dec_pps_buf,
                                   h264_data->dec_pps_len);
        }
    }
    am_status = AMediaCodec_configure(and_media_data->dec, vid_fmt, NULL,
                                      NULL, 0);

    AMediaFormat_delete(vid_fmt);
    if (am_status != AMEDIA_OK) {
        PJ_LOG(4, (THIS_FILE, "Decoder configure failed, status=%d, fmt_id=%d",
                   am_status, and_media_data->prm->dec_fmt.id));
        return PJMEDIA_CODEC_EFAILED;
    }

    am_status = AMediaCodec_start(and_media_data->dec);
    if (am_status != AMEDIA_OK) {
        PJ_LOG(4, (THIS_FILE, "Decoder start failed, status=%d",
                   am_status));
        return PJMEDIA_CODEC_EFAILED;
    }
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_codec_and_media_vid_init(
                                                pjmedia_vid_codec_mgr *mgr,
                                                pj_pool_factory *pf)
{
    const pj_str_t h264_name = { (char*)"H264", 4};
    pj_status_t status;

    if (and_media_factory.pool != NULL) {
        /* Already initialized. */
        return PJ_SUCCESS;
    }

    if (!mgr) mgr = pjmedia_vid_codec_mgr_instance();
    PJ_ASSERT_RETURN(mgr, PJ_EINVAL);

    /* Create Android AMediaCodec codec factory. */
    and_media_factory.base.op = &and_media_factory_op;
    and_media_factory.base.factory_data = NULL;
    and_media_factory.mgr = mgr;
    and_media_factory.pf = pf;
    and_media_factory.pool = pj_pool_create(pf, "and_media_vid_factory",
                                            256, 256, NULL);
    if (!and_media_factory.pool)
        return PJ_ENOMEM;

#if PJMEDIA_HAS_AND_MEDIA_H264
    /* Registering format match for SDP negotiation */
    status = pjmedia_sdp_neg_register_fmt_match_cb(
                                        &h264_name,
                                        &pjmedia_vid_codec_h264_match_sdp);
    if (status != PJ_SUCCESS)
        goto on_error;
#endif

    /* Register codec factory to codec manager. */
    status = pjmedia_vid_codec_mgr_register_factory(mgr,
                                                    &and_media_factory.base);
    if (status != PJ_SUCCESS)
        goto on_error;

    PJ_LOG(4,(THIS_FILE, "Android AMediaCodec initialized"));

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(and_media_factory.pool);
    and_media_factory.pool = NULL;
    return status;
}

/*
 * Unregister Android AMediaCodec factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_and_media_vid_deinit(void)
{
    pj_status_t status = PJ_SUCCESS;

    if (and_media_factory.pool == NULL) {
        /* Already deinitialized */
        return PJ_SUCCESS;
    }

    /* Unregister Android AMediaCodec factory. */
    status = pjmedia_vid_codec_mgr_unregister_factory(and_media_factory.mgr,
                                                      &and_media_factory.base);

    /* Destroy pool. */
    pj_pool_release(and_media_factory.pool);
    and_media_factory.pool = NULL;

    return status;
}

static pj_status_t and_media_test_alloc(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info )
{
    unsigned i;

    PJ_ASSERT_RETURN(factory == &and_media_factory.base, PJ_EINVAL);

    for (i = 0; i < PJ_ARRAY_SIZE(and_media_codec); ++i) {
        if (and_media_codec[i].enabled && info->pt == and_media_codec[i].pt &&
            (info->fmt_id == and_media_codec[i].fmt_id))
        {
            return PJ_SUCCESS;
        }
    }

    return PJMEDIA_CODEC_EUNSUP;
}

static pj_status_t and_media_default_attr(pjmedia_vid_codec_factory *factory,
                                      const pjmedia_vid_codec_info *info,
                                      pjmedia_vid_codec_param *attr )
{
    unsigned i;

    PJ_ASSERT_RETURN(factory == &and_media_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(info && attr, PJ_EINVAL);

    for (i = 0; i < PJ_ARRAY_SIZE(and_media_codec); ++i) {
        if (and_media_codec[i].enabled && info->pt != 0 &&
            (info->fmt_id == and_media_codec[i].fmt_id))
        {
            break;
        }
    }

    if (i == PJ_ARRAY_SIZE(and_media_codec))
        return PJ_EINVAL;

    pj_bzero(attr, sizeof(pjmedia_vid_codec_param));

    attr->dir = PJMEDIA_DIR_ENCODING_DECODING;
    attr->packing = PJMEDIA_VID_PACKING_PACKETS;

    /* Encoded format */
    pjmedia_format_init_video(&attr->enc_fmt, info->fmt_id,
                              DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS, 1);

    /* Decoded format */
    pjmedia_format_init_video(&attr->dec_fmt, PJMEDIA_FORMAT_I420,
                              DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS, 1);

    attr->dec_fmtp = and_media_codec[i].dec_fmtp;

    /* Bitrate */
    attr->enc_fmt.det.vid.avg_bps = DEFAULT_AVG_BITRATE;
    attr->enc_fmt.det.vid.max_bps = DEFAULT_MAX_BITRATE;

    /* Encoding MTU */
    attr->enc_mtu = PJMEDIA_MAX_VID_PAYLOAD_SIZE;

    return PJ_SUCCESS;
}

static pj_bool_t codec_exists(const pj_str_t *codec_name)
{
    AMediaCodec *codec;
    char *codec_txt;

    codec_txt = codec_name->ptr;

    codec = AMediaCodec_createCodecByName(codec_txt);
    if (!codec) {
        PJ_LOG(4, (THIS_FILE, "Failed creating codec : %.*s",
                   (int)codec_name->slen, codec_name->ptr));
        return PJ_FALSE;
    }
    AMediaCodec_delete(codec);

    return PJ_TRUE;
}

void add_codec(struct and_media_codec *codec,
               unsigned *count, pjmedia_vid_codec_info *info)
{
    info[*count].fmt_id = codec->fmt_id;
    info[*count].pt = codec->pt;
    info[*count].encoding_name = pj_str((char *)codec->name);
    info[*count].encoding_desc = pj_str((char *)codec->description);

    info[*count].clock_rate = 90000;
    info[*count].dir = PJMEDIA_DIR_ENCODING_DECODING;
    info[*count].dec_fmt_id_cnt = 1;
    info[*count].dec_fmt_id[0] = PJMEDIA_FORMAT_I420;
    info[*count].packings = PJMEDIA_VID_PACKING_PACKETS;
    info[*count].fps_cnt = 3;
    info[*count].fps[0].num = 15;
    info[*count].fps[0].denum = 1;
    info[*count].fps[1].num = 25;
    info[*count].fps[1].denum = 1;
    info[*count].fps[2].num = 30;
    info[*count].fps[2].denum = 1;
    ++*count;
}

static void get_codec_name(pj_bool_t is_enc,
                           pj_bool_t prio,
                           pjmedia_format_id fmt_id,
                           pj_str_t **codec_name,
                           unsigned *codec_num)
{
    pj_bool_t use_sw_enc = PJMEDIA_AND_MEDIA_PRIO_SW_VID_ENC;
    pj_bool_t use_sw_dec = PJMEDIA_AND_MEDIA_PRIO_SW_VID_DEC;

    *codec_num = 0;

    switch (fmt_id) {

#if PJMEDIA_HAS_AND_MEDIA_H264
    case PJMEDIA_FORMAT_H264:
        if (is_enc) {
            if ((prio && use_sw_enc) || (!prio && !use_sw_enc)) {
                *codec_name = &H264_sw_encoder[0];
                *codec_num = PJ_ARRAY_SIZE(H264_sw_encoder);
            } else {
                *codec_name = &H264_hw_encoder[0];
                *codec_num = PJ_ARRAY_SIZE(H264_hw_encoder);
            }
        } else {
            if ((prio && use_sw_dec) || (!prio && !use_sw_dec)) {
                *codec_name = &H264_sw_decoder[0];
                *codec_num = PJ_ARRAY_SIZE(H264_sw_decoder);
            } else {
                *codec_name = &H264_hw_decoder[0];
                *codec_num = PJ_ARRAY_SIZE(H264_hw_decoder);
            }
        }
        break;
#endif
#if PJMEDIA_HAS_AND_MEDIA_VP8
    case PJMEDIA_FORMAT_VP8:
        if (is_enc) {
            if ((prio && use_sw_enc) || (!prio && !use_sw_enc)) {
                *codec_name = &VP8_sw_encoder[0];
                *codec_num = PJ_ARRAY_SIZE(VP8_sw_encoder);
            } else {
                *codec_name = &VP8_hw_encoder[0];
                *codec_num = PJ_ARRAY_SIZE(VP8_hw_encoder);
            }
        } else {
            if ((prio && use_sw_dec) || (!prio && !use_sw_dec)) {
                *codec_name = &VP8_sw_decoder[0];
                *codec_num = PJ_ARRAY_SIZE(VP8_sw_decoder);
            } else {
                *codec_name = &VP8_hw_decoder[0];
                *codec_num = PJ_ARRAY_SIZE(VP8_hw_decoder);
            }
        }
        break;
#endif
#if PJMEDIA_HAS_AND_MEDIA_VP9
    case PJMEDIA_FORMAT_VP9:
        if (is_enc) {
            if ((prio && use_sw_enc) || (!prio && !use_sw_enc)) {
                *codec_name = &VP9_sw_encoder[0];
                *codec_num = PJ_ARRAY_SIZE(VP9_sw_encoder);
            } else {
                *codec_name = &VP9_hw_encoder[0];
                *codec_num = PJ_ARRAY_SIZE(VP9_hw_encoder);
            }
        } else {
            if ((prio && use_sw_dec) || (!prio && !use_sw_dec)) {
                *codec_name = &VP9_sw_decoder[0];
                *codec_num = PJ_ARRAY_SIZE(VP9_sw_decoder);
            } else {
                *codec_name = &VP9_hw_decoder[0];
                *codec_num = PJ_ARRAY_SIZE(VP9_hw_decoder);
            }
        }
        break;
#endif
    default:
        break;
    }
}

static pj_status_t and_media_enum_info(pjmedia_vid_codec_factory *factory,
                                   unsigned *count,
                                   pjmedia_vid_codec_info info[])
{
    unsigned i, max;

    PJ_ASSERT_RETURN(info && *count > 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &and_media_factory.base, PJ_EINVAL);

    max = *count;

    for (i = 0, *count = 0; i < PJ_ARRAY_SIZE(and_media_codec) && *count < max;
         ++i)
    {
        unsigned enc_idx = 0;
        unsigned dec_idx = 0;
        pj_str_t *enc_name = NULL;
        unsigned num_enc;
        pj_str_t *dec_name = NULL;
        unsigned num_dec;

        get_codec_name(PJ_TRUE, PJ_TRUE, and_media_codec[i].fmt_id,
                       &enc_name, &num_enc);

        for (enc_idx = 0; enc_idx < num_enc ;++enc_idx, ++enc_name) {
            if (codec_exists(enc_name)) {
                break;
            }
        }
        if (enc_idx == num_enc) {
            get_codec_name(PJ_TRUE, PJ_FALSE, and_media_codec[i].fmt_id,
                           &enc_name, &num_enc);

            for (enc_idx = 0; enc_idx < num_enc ;++enc_idx, ++enc_name) {
                if (codec_exists(enc_name)) {
                    break;
                }
            }
            if (enc_idx == num_enc)
                continue;
        }

        get_codec_name(PJ_FALSE, PJ_TRUE, and_media_codec[i].fmt_id,
                       &dec_name, &num_dec);
        for (dec_idx = 0; dec_idx < num_dec ;++dec_idx, ++dec_name) {
            if (codec_exists(dec_name)) {
                break;
            }
        }
        if (dec_idx == num_dec) {
            get_codec_name(PJ_FALSE, PJ_FALSE, and_media_codec[i].fmt_id,
                           &dec_name, &num_dec);
            for (enc_idx = 0; enc_idx < num_enc ;++enc_idx, ++enc_name) {
                if (codec_exists(enc_name)) {
                    break;
                }
            }
            if (dec_idx == num_dec)
                continue;
        }

        and_media_codec[i].encoder_name = enc_name;
        and_media_codec[i].decoder_name = dec_name;
        PJ_LOG(4, (THIS_FILE, "Found encoder [%d]: %.*s and decoder: %.*s ",
                   *count, (int)enc_name->slen, enc_name->ptr,
                   (int)dec_name->slen, dec_name->ptr));
        add_codec(&and_media_codec[*count], count, info);
        and_media_codec[i].enabled = PJ_TRUE;
    }

    return PJ_SUCCESS;
}

static void create_codec(struct and_media_codec_data *and_media_data)
{
    char *enc_name;
    char *dec_name;

    if (!and_media_codec[and_media_data->codec_idx].encoder_name ||
        !and_media_codec[and_media_data->codec_idx].decoder_name)
    {
        return;
    }

    enc_name = and_media_codec[and_media_data->codec_idx].encoder_name->ptr;
    dec_name = and_media_codec[and_media_data->codec_idx].decoder_name->ptr;

    if (!and_media_data->enc) {
        and_media_data->enc = AMediaCodec_createCodecByName(enc_name);
        if (!and_media_data->enc) {
            PJ_LOG(4, (THIS_FILE, "Failed creating encoder: %s", enc_name));
        }
    }

    if (!and_media_data->dec) {
        and_media_data->dec = AMediaCodec_createCodecByName(dec_name);
        if (!and_media_data->dec) {
            PJ_LOG(4, (THIS_FILE, "Failed creating decoder: %s", dec_name));
        }
    }
    PJ_LOG(4, (THIS_FILE, "Created encoder: %s, decoder: %s", enc_name,
               dec_name));
}

static pj_status_t and_media_alloc_codec(pjmedia_vid_codec_factory *factory,
                                     const pjmedia_vid_codec_info *info,
                                     pjmedia_vid_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_vid_codec *codec;
    and_media_codec_data *and_media_data;
    int i, idx;

    PJ_ASSERT_RETURN(factory == &and_media_factory.base && info && p_codec,
                     PJ_EINVAL);

    idx = -1;
    for (i = 0; i < PJ_ARRAY_SIZE(and_media_codec); ++i) {
        if ((info->fmt_id == and_media_codec[i].fmt_id) &&
            (and_media_codec[i].enabled))
        {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        *p_codec = NULL;
        return PJMEDIA_CODEC_EFAILED;
    }

    *p_codec = NULL;
    pool = pj_pool_create(and_media_factory.pf, "anmedvid%p", 512, 512, NULL);
    if (!pool)
        return PJ_ENOMEM;

    /* codec instance */
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_codec);
    codec->factory = factory;
    codec->op = &and_media_codec_op;

    /* codec data */
    and_media_data = PJ_POOL_ZALLOC_T(pool, and_media_codec_data);
    and_media_data->pool = pool;
    and_media_data->codec_idx = idx;
    codec->codec_data = and_media_data;

    create_codec(and_media_data);
    if (!and_media_data->enc || !and_media_data->dec) {
        goto on_error;
    }

    *p_codec = codec;
    return PJ_SUCCESS;

on_error:
    and_media_dealloc_codec(factory, codec);
    return PJMEDIA_CODEC_EFAILED;
}

static pj_status_t and_media_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                       pjmedia_vid_codec *codec )
{
    and_media_codec_data *and_media_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    PJ_UNUSED_ARG(factory);

    and_media_data = (and_media_codec_data*) codec->codec_data;
    if (and_media_data->enc) {
        AMediaCodec_stop(and_media_data->enc);
        AMediaCodec_delete(and_media_data->enc);
        and_media_data->enc = NULL;
    }

    if (and_media_data->dec) {
        AMediaCodec_stop(and_media_data->dec);
        AMediaCodec_delete(and_media_data->dec);
        and_media_data->dec = NULL;
    }
    pj_pool_release(and_media_data->pool);
    return PJ_SUCCESS;
}

static pj_status_t and_media_codec_init(pjmedia_vid_codec *codec,
                                    pj_pool_t *pool )
{
    PJ_ASSERT_RETURN(codec && pool, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

static pj_status_t and_media_codec_open(pjmedia_vid_codec *codec,
                                    pjmedia_vid_codec_param *codec_param)
{
    and_media_codec_data *and_media_data;
    pjmedia_vid_codec_param *param;
    pj_status_t status = PJ_SUCCESS;

    and_media_data = (and_media_codec_data*) codec->codec_data;
    and_media_data->prm = pjmedia_vid_codec_param_clone( and_media_data->pool,
                                                     codec_param);
    param = and_media_data->prm;
    if (and_media_codec[and_media_data->codec_idx].open_codec) {
        status = and_media_codec[and_media_data->codec_idx].open_codec(
                                                                and_media_data);
        if (status != PJ_SUCCESS)
            return status;
    }
    and_media_data->whole = (param->packing == PJMEDIA_VID_PACKING_WHOLE);
    status = configure_encoder(and_media_data);
    if (status != PJ_SUCCESS) {
        return PJMEDIA_CODEC_EFAILED;
    }
    status = configure_decoder(and_media_data);
    if (status != PJ_SUCCESS) {
        return PJMEDIA_CODEC_EFAILED;
    }
    if (and_media_data->dec_buf_size == 0) {
        and_media_data->dec_buf_size = (MAX_RX_WIDTH * MAX_RX_HEIGHT * 3 >> 1) +
                                       (MAX_RX_WIDTH);
    }
    and_media_data->dec_buf = (pj_uint8_t*)pj_pool_alloc(and_media_data->pool,
                                                  and_media_data->dec_buf_size);
    /* Need to update param back after values are negotiated */
    pj_memcpy(codec_param, param, sizeof(*codec_param));

    return PJ_SUCCESS;
}

static pj_status_t and_media_codec_close(pjmedia_vid_codec *codec)
{
    PJ_ASSERT_RETURN(codec, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    return PJ_SUCCESS;
}

static pj_status_t and_media_codec_modify(pjmedia_vid_codec *codec,
                                      const pjmedia_vid_codec_param *param)
{
    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(param);
    return PJ_EINVALIDOP;
}

static pj_status_t and_media_codec_get_param(pjmedia_vid_codec *codec,
                                         pjmedia_vid_codec_param *param)
{
    struct and_media_codec_data *and_media_data;

    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);

    and_media_data = (and_media_codec_data*) codec->codec_data;
    pj_memcpy(param, and_media_data->prm, sizeof(*param));

    return PJ_SUCCESS;
}

static pj_status_t and_media_codec_encode_begin(pjmedia_vid_codec *codec,
                                            const pjmedia_vid_encode_opt *opt,
                                            const pjmedia_frame *input,
                                            unsigned out_size,
                                            pjmedia_frame *output,
                                            pj_bool_t *has_more)
{
    struct and_media_codec_data *and_media_data;
    unsigned i;
    pj_ssize_t buf_idx;

    PJ_ASSERT_RETURN(codec && input && out_size && output && has_more,
                     PJ_EINVAL);

    and_media_data = (and_media_codec_data*) codec->codec_data;

    if (opt && opt->force_keyframe) {
#if __ANDROID_API__ >=26
        AMediaFormat *vid_fmt = NULL;
        media_status_t am_status;

        vid_fmt = AMediaFormat_new();
        if (!vid_fmt) {
            return PJMEDIA_CODEC_EFAILED;
        }
        AMediaFormat_setInt32(vid_fmt, AND_MEDIA_KEY_REQUEST_SYNCF, 0);
        am_status = AMediaCodec_setParameters(and_media_data->enc, vid_fmt);

        if (am_status != AMEDIA_OK)
            PJ_LOG(4,(THIS_FILE, "Encoder setParameters failed %d", am_status));

        AMediaFormat_delete(vid_fmt);
#else
        PJ_LOG(5, (THIS_FILE, "Encoder cannot be forced to send keyframe"));
#endif
    }

    buf_idx = AMediaCodec_dequeueInputBuffer(and_media_data->enc,
                                             CODEC_DEQUEUE_TIMEOUT);
    if (buf_idx >= 0) {
        media_status_t am_status;
        pj_size_t output_size;
        pj_uint8_t *input_buf = AMediaCodec_getInputBuffer(and_media_data->enc,
                                                    buf_idx, &output_size);
        if (input_buf && output_size >= input->size) {
            pj_memcpy(input_buf, input->buf, input->size);
            am_status = AMediaCodec_queueInputBuffer(and_media_data->enc,
                                                buf_idx, 0, input->size, 0, 0);
            if (am_status != AMEDIA_OK) {
                PJ_LOG(4, (THIS_FILE, "Encoder queueInputBuffer return %d",
                           am_status));
                goto on_return;
            }
        } else {
            if (!input_buf) {
                PJ_LOG(4,(THIS_FILE, "Encoder getInputBuffer "
                                     "returns no input buff"));
            } else {
                PJ_LOG(4,(THIS_FILE, "Encoder getInputBuffer "
                                     "size: %lu, expecting %lu.",
                                     output_size, input->size));
            }
            goto on_return;
        }
    } else {
        PJ_LOG(4,(THIS_FILE, "Encoder dequeueInputBuffer failed[%ld]",
                             buf_idx));
        goto on_return;
    }

    for (i = 0; i < CODEC_WAIT_RETRY; ++i) {
        buf_idx = AMediaCodec_dequeueOutputBuffer(and_media_data->enc,
                                                  &and_media_data->enc_buf_info,
                                                  CODEC_DEQUEUE_TIMEOUT);
        if (buf_idx == -1) {
            /* Timeout, wait until output buffer is availble. */
            PJ_LOG(5, (THIS_FILE, "Encoder dequeueOutputBuffer timeout[%d]",
                       i+1));
            pj_thread_sleep(CODEC_THREAD_WAIT);
        } else {
            break;
        }
    }

    if (buf_idx >= 0) {
        pj_size_t output_size;
        pj_uint8_t *output_buf = AMediaCodec_getOutputBuffer(
                                                        and_media_data->enc,
                                                        buf_idx,
                                                        &output_size);
        if (!output_buf) {
            PJ_LOG(4, (THIS_FILE, "Encoder failed getting output buffer, "
                       "buffer size %d, offset %d, flags %d",
                       and_media_data->enc_buf_info.size,
                       and_media_data->enc_buf_info.offset,
                       and_media_data->enc_buf_info.flags));
            goto on_return;
        }
        and_media_data->enc_processed = 0;
        and_media_data->enc_frame_whole = output_buf;
        and_media_data->enc_output_buf_idx = buf_idx;
        and_media_data->enc_frame_size = and_media_data->enc_buf_info.size;

        if (and_media_codec[and_media_data->codec_idx].process_encode) {
            pj_status_t status;

            status = and_media_codec[and_media_data->codec_idx].process_encode(
                                                                and_media_data);

            if (status != PJ_SUCCESS)
                goto on_return;
        }

        if(and_media_data->enc_buf_info.flags & AND_MEDIA_FRM_TYPE_KEYFRAME) {
            output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;
        }

        if (and_media_data->whole) {
            unsigned payload_size = 0;
            unsigned start_data = 0;

            *has_more = PJ_FALSE;

            if ((and_media_data->prm->enc_fmt.id == PJMEDIA_FORMAT_H264) &&
                (and_media_data->enc_buf_info.flags &
                                                   AND_MEDIA_FRM_TYPE_KEYFRAME))
            {
                h264_codec_data *h264_data =
                                     (h264_codec_data *)and_media_data->ex_data;
                start_data = h264_data->enc_sps_pps_len;
                pj_memcpy(output->buf, h264_data->enc_sps_pps_buf,
                          h264_data->enc_sps_pps_len);
            }

            payload_size = and_media_data->enc_buf_info.size + start_data;

            if (payload_size > out_size)
                return PJMEDIA_CODEC_EFRMTOOSHORT;

            output->type = PJMEDIA_FRAME_TYPE_VIDEO;
            output->size = payload_size;
            output->timestamp = input->timestamp;
            pj_memcpy((pj_uint8_t*)output->buf+start_data,
                      and_media_data->enc_frame_whole,
                      and_media_data->enc_buf_info.size);

            AMediaCodec_releaseOutputBuffer(and_media_data->enc,
                                            buf_idx,
                                            0);

            return PJ_SUCCESS;
        }
    } else {
        if (buf_idx == -2) {
            int width, height, color_fmt, stride;

            /* Format change. */
            AMediaFormat *vid_fmt = AMediaCodec_getOutputFormat(
                                                           and_media_data->enc);

            AMediaFormat_getInt32(vid_fmt, AND_MEDIA_KEY_WIDTH, &width);
            AMediaFormat_getInt32(vid_fmt, AND_MEDIA_KEY_HEIGHT, &height);
            AMediaFormat_getInt32(vid_fmt, AND_MEDIA_KEY_COLOR_FMT, &color_fmt);
            AMediaFormat_getInt32(vid_fmt, AND_MEDIA_KEY_STRIDE, &stride);
            PJ_LOG(5, (THIS_FILE, "Encoder detect new width %d, height %d, "
                       "color_fmt 0x%X, stride %d buf_size %d",
                       width, height, color_fmt, stride,
                       and_media_data->enc_buf_info.size));

            AMediaFormat_delete(vid_fmt);
        } else {
            PJ_LOG(4, (THIS_FILE, "Encoder dequeueOutputBuffer failed[%ld]",
                       buf_idx));
        }
        goto on_return;
    }

    return and_media_codec_encode_more(codec, out_size, output, has_more);

on_return:
    output->size = 0;
    output->type = PJMEDIA_FRAME_TYPE_NONE;
    *has_more = PJ_FALSE;
    return PJ_SUCCESS;
}

static pj_status_t and_media_codec_encode_more(pjmedia_vid_codec *codec,
                                           unsigned out_size,
                                           pjmedia_frame *output,
                                           pj_bool_t *has_more)
{
    struct and_media_codec_data *and_media_data;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(codec && out_size && output && has_more, PJ_EINVAL);

    and_media_data = (and_media_codec_data*) codec->codec_data;

    status = and_media_codec[and_media_data->codec_idx].encode_more(
                                                            and_media_data,
                                                            out_size, output,
                                                            has_more);
    if (!(*has_more)) {
        AMediaCodec_releaseOutputBuffer(and_media_data->enc,
                                        and_media_data->enc_output_buf_idx,
                                        0);
    }

    return status;
}

static int write_yuv(pj_uint8_t *buf,
                     unsigned dst_len,
                     unsigned char* input,
                     int stride_len,
                     int iWidth,
                     int iHeight)
{
    unsigned req_size;
    pj_uint8_t *dst = buf;
    pj_uint8_t *max = dst + dst_len;
    int   i;
    unsigned char*  pPtr = NULL;

    req_size = (iWidth * iHeight) + (iWidth / 2 * iHeight / 2) +
               (iWidth / 2 * iHeight / 2);
    if (dst_len < req_size)
        return -1;

    pPtr = input;
    for (i = 0; i < iHeight && (dst + iWidth < max); i++) {
        pj_memcpy(dst, pPtr, iWidth);
        pPtr += stride_len;
        dst += iWidth;
    }

    if (i < iHeight)
        return -1;

    iHeight = iHeight / 2;
    iWidth = iWidth / 2;
    for (i = 0; i < iHeight && (dst + iWidth <= max); i++) {
        pj_memcpy(dst, pPtr, iWidth);
        pPtr += stride_len/2;
        dst += iWidth;
    }

    if (i < iHeight)
        return -1;

    for (i = 0; i < iHeight && (dst + iWidth <= max); i++) {
        pj_memcpy(dst, pPtr, iWidth);
        pPtr += stride_len/2;
        dst += iWidth;
    }

    if (i < iHeight)
        return -1;

    return dst - buf;
}

static void and_media_get_input_buffer(
                                    struct and_media_codec_data *and_media_data)
{
    pj_ssize_t buf_idx = -1;

    buf_idx = AMediaCodec_dequeueInputBuffer(and_media_data->dec,
                                             CODEC_DEQUEUE_TIMEOUT);

    if (buf_idx < 0) {
        PJ_LOG(4,(THIS_FILE, "Decoder dequeueInputBuffer failed return %ld",
                  buf_idx));

        and_media_data->dec_input_buf = NULL;

        if (buf_idx == -10000) {
            PJ_LOG(5, (THIS_FILE, "Resetting decoder"));
            AMediaCodec_stop(and_media_data->dec);
            AMediaCodec_delete(and_media_data->dec);
            and_media_data->dec = NULL;

            create_codec(and_media_data);
            if (and_media_data->dec)
                configure_decoder(and_media_data);
        }
        return;
    }

    and_media_data->dec_input_buf_len = 0;
    and_media_data->dec_input_buf_idx = buf_idx;
    and_media_data->dec_input_buf = AMediaCodec_getInputBuffer(
                                       and_media_data->dec,
                                       buf_idx,
                                       &and_media_data->dec_input_buf_max_size);
}

static pj_status_t and_media_decode(pjmedia_vid_codec *codec,
                                struct and_media_codec_data *and_media_data,
                                pj_uint8_t *input_buf, unsigned buf_size,
                                int buf_flag, pj_timestamp *input_ts,
                                pj_bool_t write_output, pjmedia_frame *output)
{
    pj_ssize_t buf_idx = 0;
    pj_status_t status = PJ_SUCCESS;
    media_status_t am_status;

    if ((and_media_data->dec_input_buf_max_size > 0) &&
        (and_media_data->dec_input_buf_len + buf_size >
         and_media_data->dec_input_buf_max_size))
    {
        am_status = AMediaCodec_queueInputBuffer(and_media_data->dec,
                                            and_media_data->dec_input_buf_idx,
                                            0,
                                            and_media_data->dec_input_buf_len,
                                            input_ts->u32.lo,
                                            buf_flag);
        if (am_status != AMEDIA_OK) {
            PJ_LOG(4,(THIS_FILE, "Decoder queueInputBuffer idx[%ld] return %d",
                    and_media_data->dec_input_buf_idx, am_status));
            return status;
        }
        and_media_data->dec_input_buf = NULL;
    }

    if (and_media_data->dec_input_buf == NULL)
    {
        and_media_get_input_buffer(and_media_data);

        if (and_media_data->dec_input_buf == NULL) {
            PJ_LOG(4,(THIS_FILE, "Decoder failed getting input buffer"));
            return status;
        }
    }
    pj_memcpy(and_media_data->dec_input_buf + and_media_data->dec_input_buf_len,
              input_buf, buf_size);

    and_media_data->dec_input_buf_len += buf_size;

    if (!write_output)
        return status;

    am_status = AMediaCodec_queueInputBuffer(and_media_data->dec,
                                             and_media_data->dec_input_buf_idx,
                                             0,
                                             and_media_data->dec_input_buf_len,
                                             input_ts->u32.lo,
                                             buf_flag);
    if (am_status != AMEDIA_OK) {
        PJ_LOG(4,(THIS_FILE, "Decoder queueInputBuffer failed return %d",
                  am_status));
        and_media_data->dec_input_buf = NULL;
        return status;
    }
    and_media_data->dec_input_buf_len += buf_size;

    buf_idx = AMediaCodec_dequeueOutputBuffer(and_media_data->dec,
                                              &and_media_data->dec_buf_info,
                                              CODEC_DEQUEUE_TIMEOUT);

    if (buf_idx >= 0) {
        pj_size_t output_size;
        int len;

        pj_uint8_t *output_buf = AMediaCodec_getOutputBuffer(
                                                        and_media_data->dec,
                                                        buf_idx,
                                                        &output_size);
        if (output_buf == NULL) {
            am_status = AMediaCodec_releaseOutputBuffer(and_media_data->dec,
                                            buf_idx,
                                            0);
            PJ_LOG(4,(THIS_FILE, "Decoder getOutputBuffer failed"));
            return status;
        }
        len = write_yuv((pj_uint8_t *)output->buf,
                        and_media_data->dec_buf_info.size,
                        output_buf,
                        and_media_data->dec_stride_len,
                        and_media_data->prm->dec_fmt.det.vid.size.w,
                        and_media_data->prm->dec_fmt.det.vid.size.h);

        am_status = AMediaCodec_releaseOutputBuffer(and_media_data->dec,
                                                    buf_idx,
                                                    0);

        if (len > 0) {
            if (!and_media_data->dec_has_output_frame) {
                output->type = PJMEDIA_FRAME_TYPE_VIDEO;
                output->size = len;
                output->timestamp = *input_ts;

                and_media_data->dec_has_output_frame = PJ_TRUE;
            }
        } else {
            status = PJMEDIA_CODEC_EFRMTOOSHORT;
        }
    } else if (buf_idx == -2) {
        int width, height, stride;
        AMediaFormat *vid_fmt;
        /* Get output format */
        vid_fmt = AMediaCodec_getOutputFormat(and_media_data->dec);

        AMediaFormat_getInt32(vid_fmt, AND_MEDIA_KEY_WIDTH, &width);
        AMediaFormat_getInt32(vid_fmt, AND_MEDIA_KEY_HEIGHT, &height);
        AMediaFormat_getInt32(vid_fmt, AND_MEDIA_KEY_STRIDE, &stride);

        AMediaFormat_delete(vid_fmt);
        and_media_data->dec_stride_len = stride;
        if (width != and_media_data->prm->dec_fmt.det.vid.size.w ||
            height != and_media_data->prm->dec_fmt.det.vid.size.h)
        {
            pjmedia_event event;

            and_media_data->prm->dec_fmt.det.vid.size.w = width;
            and_media_data->prm->dec_fmt.det.vid.size.h = height;

            PJ_LOG(4,(THIS_FILE, "Frame size changed to %dx%d",
                      and_media_data->prm->dec_fmt.det.vid.size.w,
                      and_media_data->prm->dec_fmt.det.vid.size.h));

            /* Broadcast format changed event */
            pjmedia_event_init(&event, PJMEDIA_EVENT_FMT_CHANGED,
                               &output->timestamp, codec);
            event.data.fmt_changed.dir = PJMEDIA_DIR_DECODING;
            pjmedia_format_copy(&event.data.fmt_changed.new_fmt,
                                &and_media_data->prm->dec_fmt);
            pjmedia_event_publish(NULL, codec, &event,
                                  PJMEDIA_EVENT_PUBLISH_DEFAULT);
        }
    } else {
        PJ_LOG(4,(THIS_FILE, "Decoder dequeueOutputBuffer failed [%ld]",
                  buf_idx));
    }
    return status;
}

static pj_status_t and_media_codec_decode(pjmedia_vid_codec *codec,
                                      pj_size_t count,
                                      pjmedia_frame packets[],
                                      unsigned out_size,
                                      pjmedia_frame *output)
{
    struct and_media_codec_data *and_media_data;
    pj_status_t status = PJ_EINVAL;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    and_media_data = (and_media_codec_data*) codec->codec_data;
    and_media_data->dec_has_output_frame = PJ_FALSE;
    and_media_data->dec_input_buf = NULL;
    and_media_data->dec_input_buf_len = 0;

    if (and_media_codec[and_media_data->codec_idx].decode) {
        status = and_media_codec[and_media_data->codec_idx].decode(codec, count,
                                                           packets, out_size,
                                                           output);
    }
    if (status != PJ_SUCCESS) {
        return status;
    }
    if (!and_media_data->dec_has_output_frame) {
        pjmedia_event event;

        /* Broadcast missing keyframe event */
        pjmedia_event_init(&event, PJMEDIA_EVENT_KEYFRAME_MISSING,
                           &packets[0].timestamp, codec);
        pjmedia_event_publish(NULL, codec, &event,
                              PJMEDIA_EVENT_PUBLISH_DEFAULT);

        PJ_LOG(4,(THIS_FILE, "Decoder couldn't produce output frame"));

        output->type = PJMEDIA_FRAME_TYPE_NONE;
        output->size = 0;
        output->timestamp = packets[0].timestamp;
    }
    return PJ_SUCCESS;
}

#if PJMEDIA_HAS_AND_MEDIA_H264

static pj_status_t open_h264(and_media_codec_data *and_media_data)
{
    pj_status_t status;
    pjmedia_vid_codec_param *param = and_media_data->prm;
    pjmedia_h264_packetizer_cfg  pktz_cfg;
    pjmedia_vid_codec_h264_fmtp  h264_fmtp;
    h264_codec_data *h264_data;

    /* Parse remote fmtp */
    pj_bzero(&h264_fmtp, sizeof(h264_fmtp));
    status = pjmedia_vid_codec_h264_parse_fmtp(&param->enc_fmtp,
                                               &h264_fmtp);
    if (status != PJ_SUCCESS)
        return status;

    /* Apply SDP fmtp to format in codec param */
    if (!param->ignore_fmtp) {
        status = pjmedia_vid_codec_h264_apply_fmtp(param);
        if (status != PJ_SUCCESS)
            return status;
    }
    h264_data = PJ_POOL_ZALLOC_T(and_media_data->pool, h264_codec_data);
    if (!h264_data)
        return PJ_ENOMEM;

    pj_bzero(&pktz_cfg, sizeof(pktz_cfg));
    pktz_cfg.mtu = param->enc_mtu;
    pktz_cfg.unpack_nal_start = 4;
    /* Packetization mode */
    if (h264_fmtp.packetization_mode == 0)
        pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_SINGLE_NAL;
    else if (h264_fmtp.packetization_mode == 1)
        pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_NON_INTERLEAVED;
    else
        return PJ_ENOTSUP;

    /* Android H264 only supports Non Interleaved mode. */
    pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_NON_INTERLEAVED;
    status = pjmedia_h264_packetizer_create(and_media_data->pool, &pktz_cfg,
                                            &h264_data->pktz);
    if (status != PJ_SUCCESS)
        return status;

    and_media_data->ex_data = h264_data;
    and_media_data->dec_buf_size = (MAX_RX_WIDTH * MAX_RX_HEIGHT * 3 >> 1) +
                                   (MAX_RX_WIDTH);

    /* If available, use the "sprop-parameter-sets" fmtp from remote SDP
     * to create the decoder.
     */
    if (h264_fmtp.sprop_param_sets_len) {
        const pj_uint8_t start_code[3] = { 0, 0, 1 };
        const int code_size = PJ_ARRAY_SIZE(start_code);
        const pj_uint8_t med_start_code[4] = { 0, 0, 0, 1 };
        const int med_code_size = PJ_ARRAY_SIZE(med_start_code);
        unsigned i, j;

        for (i = h264_fmtp.sprop_param_sets_len - code_size;
             i >= code_size; i--)
        {
            for (j = 0; j < code_size; j++) {
                if (h264_fmtp.sprop_param_sets[i + j] != start_code[j]) {
                    break;
                }
            }
        }

        if (i >= code_size) {
            h264_data->dec_sps_len = i + med_code_size - code_size;
            h264_data->dec_pps_len = h264_fmtp.sprop_param_sets_len +
                med_code_size - code_size - i;

            h264_data->dec_sps_buf = (pj_uint8_t *)pj_pool_alloc(
                                and_media_data->pool, h264_data->dec_sps_len);
            h264_data->dec_pps_buf = (pj_uint8_t *)pj_pool_alloc(
                                and_media_data->pool, h264_data->dec_pps_len);

            pj_memcpy(h264_data->dec_sps_buf, med_start_code,
                      med_code_size);
            pj_memcpy(h264_data->dec_sps_buf + med_code_size,
                      &h264_fmtp.sprop_param_sets[code_size],
                      h264_data->dec_sps_len - med_code_size);
            pj_memcpy(h264_data->dec_pps_buf, med_start_code,
                      med_code_size);
            pj_memcpy(h264_data->dec_pps_buf + med_code_size,
                      &h264_fmtp.sprop_param_sets[i + code_size],
                      h264_data->dec_pps_len - med_code_size);
        }
    }
    return status;
}

static pj_status_t process_encode_h264(and_media_codec_data *and_media_data)
{
    pj_status_t status = PJ_SUCCESS;
    h264_codec_data *h264_data;

    h264_data = (h264_codec_data *)and_media_data->ex_data;
    if (and_media_data->enc_buf_info.flags & AND_MEDIA_FRM_TYPE_CONFIG) {

        /*
        * Config data or SPS+PPS. Update the SPS and PPS buffer,
        * this will be sent later when sending Keyframe.
        */
        h264_data->enc_sps_pps_len = PJ_MIN(and_media_data->enc_buf_info.size,
                                        sizeof(h264_data->enc_sps_pps_buf));
        pj_memcpy(h264_data->enc_sps_pps_buf, and_media_data->enc_frame_whole,
                  h264_data->enc_sps_pps_len);

        AMediaCodec_releaseOutputBuffer(and_media_data->enc,
                                        and_media_data->enc_output_buf_idx,
                                        0);

        return PJ_EIGNORED;
    }
    if (and_media_data->enc_buf_info.flags & AND_MEDIA_FRM_TYPE_KEYFRAME) {
        h264_data->enc_sps_pps_ex = PJ_TRUE;
        and_media_data->enc_frame_size = h264_data->enc_sps_pps_len;
    } else {
        h264_data->enc_sps_pps_ex = PJ_FALSE;
    }

    return status;
}

static pj_status_t encode_more_h264(and_media_codec_data *and_media_data,
                                    unsigned out_size,
                                    pjmedia_frame *output,
                                    pj_bool_t *has_more)
{
    const pj_uint8_t *payload;
    pj_size_t payload_len;
    pj_status_t status;
    pj_uint8_t *data_buf = NULL;
    h264_codec_data *h264_data;

    h264_data = (h264_codec_data *)and_media_data->ex_data;
    if (h264_data->enc_sps_pps_ex) {
        data_buf = h264_data->enc_sps_pps_buf;
    } else {
        data_buf = and_media_data->enc_frame_whole;
    }
    /* We have outstanding frame in packetizer */
    status = pjmedia_h264_packetize(h264_data->pktz,
                                    data_buf,
                                    and_media_data->enc_frame_size,
                                    &and_media_data->enc_processed,
                                    &payload, &payload_len);
    if (status != PJ_SUCCESS) {
        /* Reset */
        and_media_data->enc_frame_size = and_media_data->enc_processed = 0;
        *has_more = (and_media_data->enc_processed <
                     and_media_data->enc_frame_size);

        PJ_PERROR(3,(THIS_FILE, status, "pjmedia_h264_packetize() error"));
        return status;
    }

    PJ_ASSERT_RETURN(payload_len <= out_size, PJMEDIA_CODEC_EFRMTOOSHORT);

    output->type = PJMEDIA_FRAME_TYPE_VIDEO;
    pj_memcpy(output->buf, payload, payload_len);
    output->size = payload_len;

    if (and_media_data->enc_processed >= and_media_data->enc_frame_size) {
        h264_codec_data *h264_data = (h264_codec_data *)and_media_data->ex_data;

        if (h264_data->enc_sps_pps_ex) {
            *has_more = PJ_TRUE;
            h264_data->enc_sps_pps_ex = PJ_FALSE;
            and_media_data->enc_processed = 0;
            and_media_data->enc_frame_size = and_media_data->enc_buf_info.size;
        } else {
            *has_more = PJ_FALSE;
        }
    } else {
        *has_more = PJ_TRUE;
    }

    return PJ_SUCCESS;
}

static pj_status_t decode_h264(pjmedia_vid_codec *codec,
                               pj_size_t count,
                               pjmedia_frame packets[],
                               unsigned out_size,
                               pjmedia_frame *output)
{
    struct and_media_codec_data *and_media_data;
    const pj_uint8_t start_code[] = { 0, 0, 0, 1 };
    const int code_size = PJ_ARRAY_SIZE(start_code);
    unsigned buf_pos, whole_len = 0;
    unsigned i, frm_cnt;
    pj_status_t status;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    and_media_data = (and_media_codec_data*) codec->codec_data;

    /*
     * Step 1: unpacketize the packets/frames
     */
    whole_len = 0;
    if (and_media_data->whole) {
        for (i=0; i<count; ++i) {
            if (whole_len + packets[i].size > and_media_data->dec_buf_size) {
                PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [1]"));
                return PJMEDIA_CODEC_EFRMTOOSHORT;
            }

            pj_memcpy( and_media_data->dec_buf + whole_len,
                       (pj_uint8_t*)packets[i].buf,
                       packets[i].size);
            whole_len += packets[i].size;
        }

    } else {
        h264_codec_data *h264_data = (h264_codec_data *)and_media_data->ex_data;

        for (i=0; i<count; ++i) {

            if (whole_len + packets[i].size + code_size >
                and_media_data->dec_buf_size)
            {
                PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [2]"));
                return PJMEDIA_CODEC_EFRMTOOSHORT;
            }

            status = pjmedia_h264_unpacketize( h264_data->pktz,
                                               (pj_uint8_t*)packets[i].buf,
                                               packets[i].size,
                                               and_media_data->dec_buf,
                                               and_media_data->dec_buf_size,
                                               &whole_len);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4,(THIS_FILE, status, "Unpacketize error"));
                continue;
            }
        }
    }

    if (whole_len + code_size > and_media_data->dec_buf_size ||
        whole_len <= code_size + 1)
    {
        PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow or unpacketize error "
                             "size: %d, buffer: %d", whole_len,
                             and_media_data->dec_buf_size));
        return PJMEDIA_CODEC_EFRMTOOSHORT;
    }

    /* Dummy NAL sentinel */
    pj_memcpy(and_media_data->dec_buf + whole_len, start_code, code_size);

    /*
     * Step 2: parse the individual NAL and give to decoder
     */
    buf_pos = 0;
    for ( frm_cnt=0; ; ++frm_cnt) {
        pj_uint32_t frm_size;
        pj_bool_t write_output = PJ_FALSE;
        unsigned char *start;

        for (i = code_size - 1; buf_pos + i < whole_len; i++) {
            if (and_media_data->dec_buf[buf_pos + i] == 0 &&
                and_media_data->dec_buf[buf_pos + i + 1] == 0 &&
                and_media_data->dec_buf[buf_pos + i + 2] == 0 &&
                and_media_data->dec_buf[buf_pos + i + 3] == 1)
            {
                break;
            }
        }

        frm_size = i;
        start = and_media_data->dec_buf + buf_pos;
        write_output = (buf_pos + frm_size >= whole_len);

        status = and_media_decode(codec, and_media_data, start, frm_size, 0,
                              &packets[0].timestamp, write_output, output);
        if (status != PJ_SUCCESS)
            return status;

        if (write_output)
            break;

        buf_pos += frm_size;
    }

    PJ_UNUSED_ARG(frm_cnt);

    return PJ_SUCCESS;
}

#endif

#if PJMEDIA_HAS_AND_MEDIA_VP8 || PJMEDIA_HAS_AND_MEDIA_VP9

static pj_status_t open_vpx(and_media_codec_data *and_media_data)
{
    vpx_codec_data *vpx_data;
    pjmedia_vid_codec_vpx_fmtp vpx_fmtp;
    pjmedia_vpx_packetizer_cfg pktz_cfg;
    pj_status_t status = PJ_SUCCESS;
    unsigned max_res = MAX_RX_WIDTH;

    if (!and_media_data->prm->ignore_fmtp) {
        status = pjmedia_vid_codec_vpx_apply_fmtp(and_media_data->prm);
        if (status != PJ_SUCCESS)
            return status;
    }

    vpx_data = PJ_POOL_ZALLOC_T(and_media_data->pool, vpx_codec_data);
    if (!vpx_data)
        return PJ_ENOMEM;

    /* Parse local fmtp */
    status = pjmedia_vid_codec_vpx_parse_fmtp(&and_media_data->prm->dec_fmtp,
                                              &vpx_fmtp);
    if (status != PJ_SUCCESS)
        return status;

    if (vpx_fmtp.max_fs > 0) {
        max_res = ((int)pj_isqrt(vpx_fmtp.max_fs * 8)) * 16;
    }
    and_media_data->dec_buf_size = (max_res * max_res * 3 >> 1) + (max_res);

    pj_bzero(&pktz_cfg, sizeof(pktz_cfg));
    pktz_cfg.mtu = and_media_data->prm->enc_mtu;
    pktz_cfg.fmt_id = and_media_data->prm->enc_fmt.id;

    status = pjmedia_vpx_packetizer_create(and_media_data->pool, &pktz_cfg,
                                            &vpx_data->pktz);
    if (status != PJ_SUCCESS)
        return status;

    and_media_data->ex_data = vpx_data;

    return status;
}

static pj_status_t encode_more_vpx(and_media_codec_data *and_media_data,
                                   unsigned out_size,
                                   pjmedia_frame *output,
                                   pj_bool_t *has_more)
{
    pj_status_t status = PJ_SUCCESS;
    struct vpx_codec_data *vpx_data = (vpx_codec_data *)and_media_data->ex_data;

    PJ_ASSERT_RETURN(and_media_data && out_size && output && has_more,
                     PJ_EINVAL);

    if ((and_media_data->prm->enc_fmt.id != PJMEDIA_FORMAT_VP8) &&
        (and_media_data->prm->enc_fmt.id != PJMEDIA_FORMAT_VP9))
    {
        *has_more = PJ_FALSE;
        output->size = 0;
        output->type = PJMEDIA_FRAME_TYPE_NONE;

        return PJ_SUCCESS;
    }

    if (and_media_data->enc_processed < and_media_data->enc_frame_size) {
        unsigned payload_desc_size = 1;
        pj_size_t payload_len = out_size;
        pj_uint8_t *p = (pj_uint8_t *)output->buf;
        pj_bool_t is_keyframe = and_media_data->enc_buf_info.flags &
                                AND_MEDIA_FRM_TYPE_KEYFRAME;

        status = pjmedia_vpx_packetize(vpx_data->pktz,
                                       and_media_data->enc_frame_size,
                                       &and_media_data->enc_processed,
                                       is_keyframe,
                                       &p,
                                       &payload_len);
        if (status != PJ_SUCCESS) {
            return status;
        }
        pj_memcpy(p + payload_desc_size,
              (and_media_data->enc_frame_whole + and_media_data->enc_processed),
              payload_len);
        output->size = payload_len + payload_desc_size;
        if (is_keyframe) {
            output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;
        }
        and_media_data->enc_processed += payload_len;
        *has_more = (and_media_data->enc_processed <
                     and_media_data->enc_frame_size);
    }

    return status;
}

static pj_status_t decode_vpx(pjmedia_vid_codec *codec,
                              pj_size_t count,
                              pjmedia_frame packets[],
                              unsigned out_size,
                              pjmedia_frame *output)
{
    unsigned i, whole_len = 0;
    pj_status_t status;
    and_media_codec_data *and_media_data =
                                      (and_media_codec_data*) codec->codec_data;
    struct vpx_codec_data *vpx_data = (vpx_codec_data *)and_media_data->ex_data;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    whole_len = 0;
    if (and_media_data->whole) {
        for (i = 0; i < count; ++i) {
            if (whole_len + packets[i].size > and_media_data->dec_buf_size) {
                PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [1]"));
                return PJMEDIA_CODEC_EFRMTOOSHORT;
            }

            pj_memcpy( and_media_data->dec_buf + whole_len,
                       (pj_uint8_t*)packets[i].buf,
                       packets[i].size);
            whole_len += packets[i].size;
        }
        status = and_media_decode(codec, and_media_data,
                                  and_media_data->dec_buf, whole_len, 0,
                                  &packets[0].timestamp, PJ_TRUE, output);

        if (status != PJ_SUCCESS)
            return status;

    } else {
        for (i = 0; i < count; ++i) {
            unsigned desc_len;
            unsigned packet_size = packets[i].size;
            pj_status_t status;
            pj_bool_t write_output;

            status = pjmedia_vpx_unpacketize(vpx_data->pktz,
                                             (pj_uint8_t *)packets[i].buf,
                                             packet_size,
                                             &desc_len);
            if (status != PJ_SUCCESS) {
                PJ_LOG(4,(THIS_FILE, "Unpacketize error packet size[%d]",
                          packet_size));
                return status;
            }

            packet_size -= desc_len;
            if (whole_len + packet_size > and_media_data->dec_buf_size) {
                PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [2]"));
                return PJMEDIA_CODEC_EFRMTOOSHORT;
            }

            write_output = (i == count - 1);

            status = and_media_decode(codec, and_media_data,
                                  (pj_uint8_t *)packets[i].buf + desc_len,
                                  packet_size, 0, &packets[0].timestamp,
                                  write_output, output);
            if (status != PJ_SUCCESS)
                return status;

            whole_len += packet_size;
        }
    }
    return PJ_SUCCESS;
}

#endif

#endif  /* PJMEDIA_HAS_ANDROID_MEDIACODEC */

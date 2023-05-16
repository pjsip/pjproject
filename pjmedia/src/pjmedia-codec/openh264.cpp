/* 
 * Copyright (C)2014 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-codec/openh264.h>
#include <pjmedia-codec/h264_packetizer.h>
#include <pjmedia/vid_codec_util.h>
#include <pjmedia/errno.h>
#include <pj/log.h>

#if defined(PJMEDIA_HAS_OPENH264_CODEC) && \
            PJMEDIA_HAS_OPENH264_CODEC != 0 && \
    defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#ifdef _MSC_VER
#   include <stdint.h>
#   pragma comment( lib, "openh264.lib")
#endif

/* OpenH264: */
#include <wels/codec_api.h>
#include <wels/codec_app_def.h>

/*
 * Constants
 */
#define THIS_FILE               "openh264.cpp"

#if (defined(PJ_DARWINOS) && PJ_DARWINOS != 0 && TARGET_OS_IPHONE) || \
     defined(__ANDROID__)
#  define DEFAULT_WIDTH         352
#  define DEFAULT_HEIGHT        288
#else
#  define DEFAULT_WIDTH         720
#  define DEFAULT_HEIGHT        480
#endif

#define DEFAULT_FPS             15
#define DEFAULT_AVG_BITRATE     256000
#define DEFAULT_MAX_BITRATE     256000

#define MAX_RX_WIDTH            1200
#define MAX_RX_HEIGHT           800

/* OpenH264 default PT */
#define OH264_PT                PJMEDIA_RTP_PT_H264

/* Minimum interval (in msec) between generating two missing keyframe events.
 * This is to avoid sending too many events during consecutive decode
 * failures.
 */
#define MISSING_KEYFRAME_EV_MIN_INTERVAL        1000

/*
 * Factory operations.
 */
static pj_status_t oh264_test_alloc(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info );
static pj_status_t oh264_default_attr(pjmedia_vid_codec_factory *factory,
                                      const pjmedia_vid_codec_info *info,
                                      pjmedia_vid_codec_param *attr );
static pj_status_t oh264_enum_info(pjmedia_vid_codec_factory *factory,
                                   unsigned *count,
                                   pjmedia_vid_codec_info codecs[]);
static pj_status_t oh264_alloc_codec(pjmedia_vid_codec_factory *factory,
                                     const pjmedia_vid_codec_info *info,
                                     pjmedia_vid_codec **p_codec);
static pj_status_t oh264_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                       pjmedia_vid_codec *codec );


/*
 * Codec operations
 */
static pj_status_t oh264_codec_init(pjmedia_vid_codec *codec,
                                    pj_pool_t *pool );
static pj_status_t oh264_codec_open(pjmedia_vid_codec *codec,
                                    pjmedia_vid_codec_param *param );
static pj_status_t oh264_codec_close(pjmedia_vid_codec *codec);
static pj_status_t oh264_codec_modify(pjmedia_vid_codec *codec,
                                      const pjmedia_vid_codec_param *param);
static pj_status_t oh264_codec_get_param(pjmedia_vid_codec *codec,
                                         pjmedia_vid_codec_param *param);
static pj_status_t oh264_codec_encode_begin(pjmedia_vid_codec *codec,
                                            const pjmedia_vid_encode_opt *opt,
                                            const pjmedia_frame *input,
                                            unsigned out_size,
                                            pjmedia_frame *output,
                                            pj_bool_t *has_more);
static pj_status_t oh264_codec_encode_more(pjmedia_vid_codec *codec,
                                           unsigned out_size,
                                           pjmedia_frame *output,
                                           pj_bool_t *has_more);
static pj_status_t oh264_codec_decode(pjmedia_vid_codec *codec,
                                      pj_size_t count,
                                      pjmedia_frame packets[],
                                      unsigned out_size,
                                      pjmedia_frame *output);

/* Definition for OpenH264 codecs operations. */
static pjmedia_vid_codec_op oh264_codec_op =
{
    &oh264_codec_init,
    &oh264_codec_open,
    &oh264_codec_close,
    &oh264_codec_modify,
    &oh264_codec_get_param,
    &oh264_codec_encode_begin,
    &oh264_codec_encode_more,
    &oh264_codec_decode,
    NULL
};

/* Definition for OpenH264 codecs factory operations. */
static pjmedia_vid_codec_factory_op oh264_factory_op =
{
    &oh264_test_alloc,
    &oh264_default_attr,
    &oh264_enum_info,
    &oh264_alloc_codec,
    &oh264_dealloc_codec
};


static struct oh264_factory
{
    pjmedia_vid_codec_factory    base;
    pjmedia_vid_codec_mgr       *mgr;
    pj_pool_factory             *pf;
    pj_pool_t                   *pool;
} oh264_factory;


typedef struct oh264_codec_data
{
    pj_pool_t                   *pool;
    pjmedia_vid_codec_param     *prm;
    pj_bool_t                    whole;
    pjmedia_h264_packetizer     *pktz;

    /* Encoder state */
    ISVCEncoder                 *enc;
    SSourcePicture              *esrc_pic;
    unsigned                     enc_input_size;
    pj_uint8_t                  *enc_frame_whole;
    unsigned                     enc_frame_size;
    unsigned                     enc_processed;
    pj_timestamp                 ets;
    SFrameBSInfo                 bsi;
    int                          ilayer;

    /* Decoder state */
    ISVCDecoder                 *dec;
    pj_uint8_t                  *dec_buf;
    unsigned                     dec_buf_size;
    unsigned                     missing_kf_interval;
    unsigned                     last_missing_kf_event;
} oh264_codec_data;

struct SLayerPEncCtx
{
    pj_int32_t                  iDLayerQp;
    SSliceArgument              sSliceArgument;
};

static void log_print(void* ctx, int level, const char* string) {
    PJ_UNUSED_ARG(ctx);
    PJ_LOG(4,("[OPENH264_LOG]", "[L%d] %s", level, string));
}

PJ_DEF(pj_status_t) pjmedia_codec_openh264_vid_init(pjmedia_vid_codec_mgr *mgr,
                                                    pj_pool_factory *pf)
{
    const pj_str_t h264_name = { (char*)"H264", 4};
    pj_status_t status;

    if (oh264_factory.pool != NULL) {
        /* Already initialized. */
        return PJ_SUCCESS;
    }

    if (!mgr) mgr = pjmedia_vid_codec_mgr_instance();
    PJ_ASSERT_RETURN(mgr, PJ_EINVAL);

    /* Create OpenH264 codec factory. */
    oh264_factory.base.op = &oh264_factory_op;
    oh264_factory.base.factory_data = NULL;
    oh264_factory.mgr = mgr;
    oh264_factory.pf = pf;
    oh264_factory.pool = pj_pool_create(pf, "oh264factory", 256, 256, NULL);
    if (!oh264_factory.pool)
        return PJ_ENOMEM;

    /* Registering format match for SDP negotiation */
    status = pjmedia_sdp_neg_register_fmt_match_cb(
                                        &h264_name,
                                        &pjmedia_vid_codec_h264_match_sdp);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Register codec factory to codec manager. */
    status = pjmedia_vid_codec_mgr_register_factory(mgr,
                                                    &oh264_factory.base);
    if (status != PJ_SUCCESS)
        goto on_error;

    PJ_LOG(4,(THIS_FILE, "OpenH264 codec initialized"));

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(oh264_factory.pool);
    oh264_factory.pool = NULL;
    return status;
}

/*
 * Unregister OpenH264 codecs factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_openh264_vid_deinit(void)
{
    pj_status_t status = PJ_SUCCESS;

    if (oh264_factory.pool == NULL) {
        /* Already deinitialized */
        return PJ_SUCCESS;
    }

    /* Unregister OpenH264 codecs factory. */
    status = pjmedia_vid_codec_mgr_unregister_factory(oh264_factory.mgr,
                                                      &oh264_factory.base);

    /* Destroy pool. */
    pj_pool_release(oh264_factory.pool);
    oh264_factory.pool = NULL;

    return status;
}

static pj_status_t oh264_test_alloc(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info )
{
    PJ_ASSERT_RETURN(factory == &oh264_factory.base, PJ_EINVAL);

    if (info->fmt_id == PJMEDIA_FORMAT_H264 &&
        info->pt == OH264_PT)
    {
        return PJ_SUCCESS;
    }

    return PJMEDIA_CODEC_EUNSUP;
}

static pj_status_t oh264_default_attr(pjmedia_vid_codec_factory *factory,
                                      const pjmedia_vid_codec_info *info,
                                      pjmedia_vid_codec_param *attr )
{
    PJ_ASSERT_RETURN(factory == &oh264_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(info && attr, PJ_EINVAL);

    pj_bzero(attr, sizeof(pjmedia_vid_codec_param));

    attr->dir = PJMEDIA_DIR_ENCODING_DECODING;
    attr->packing = PJMEDIA_VID_PACKING_PACKETS;

    /* Encoded format */
    pjmedia_format_init_video(&attr->enc_fmt, PJMEDIA_FORMAT_H264,
                              DEFAULT_WIDTH, DEFAULT_HEIGHT,
                              DEFAULT_FPS, 1);

    /* Decoded format */
    pjmedia_format_init_video(&attr->dec_fmt, PJMEDIA_FORMAT_I420,
                              DEFAULT_WIDTH, DEFAULT_HEIGHT,
                              DEFAULT_FPS, 1);

    /* Decoding fmtp */
    attr->dec_fmtp.cnt = 2;
    attr->dec_fmtp.param[0].name = pj_str((char*)"profile-level-id");
    attr->dec_fmtp.param[0].val = pj_str((char*)"42e01e");
    attr->dec_fmtp.param[1].name = pj_str((char*)" packetization-mode");
    attr->dec_fmtp.param[1].val = pj_str((char*)"1");

    /* Bitrate */
    attr->enc_fmt.det.vid.avg_bps = DEFAULT_AVG_BITRATE;
    attr->enc_fmt.det.vid.max_bps = DEFAULT_MAX_BITRATE;

    /* Encoding MTU */
    attr->enc_mtu = PJMEDIA_MAX_VID_PAYLOAD_SIZE;

    return PJ_SUCCESS;
}

static pj_status_t oh264_enum_info(pjmedia_vid_codec_factory *factory,
                                   unsigned *count,
                                   pjmedia_vid_codec_info info[])
{
    PJ_ASSERT_RETURN(info && *count > 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &oh264_factory.base, PJ_EINVAL);

    *count = 1;
    info->fmt_id = PJMEDIA_FORMAT_H264;
    info->pt = OH264_PT;
    info->encoding_name = pj_str((char*)"H264");
    info->encoding_desc = pj_str((char*)"OpenH264 codec");
    info->clock_rate = 90000;
    info->dir = PJMEDIA_DIR_ENCODING_DECODING;
    info->dec_fmt_id_cnt = 1;
    info->dec_fmt_id[0] = PJMEDIA_FORMAT_I420;
    info->packings = PJMEDIA_VID_PACKING_PACKETS |
                     PJMEDIA_VID_PACKING_WHOLE;
    info->fps_cnt = 3;
    info->fps[0].num = 15;
    info->fps[0].denum = 1;
    info->fps[1].num = 25;
    info->fps[1].denum = 1;
    info->fps[2].num = 30;
    info->fps[2].denum = 1;

    return PJ_SUCCESS;

}

static pj_status_t oh264_alloc_codec(pjmedia_vid_codec_factory *factory,
                                     const pjmedia_vid_codec_info *info,
                                     pjmedia_vid_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_vid_codec *codec;
    oh264_codec_data *oh264_data;
    int rc;
    WelsTraceCallback log_cb = &log_print;
    int log_level = PJMEDIA_CODEC_OPENH264_LOG_LEVEL;

    PJ_ASSERT_RETURN(factory == &oh264_factory.base && info && p_codec,
                     PJ_EINVAL);

    *p_codec = NULL;

    pool = pj_pool_create(oh264_factory.pf, "oh264%p", 512, 512, NULL);
    if (!pool)
        return PJ_ENOMEM;

    /* codec instance */
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_codec);
    codec->factory = factory;
    codec->op = &oh264_codec_op;

    /* codec data */
    oh264_data = PJ_POOL_ZALLOC_T(pool, oh264_codec_data);
    oh264_data->pool = pool;
    codec->codec_data = oh264_data;

    /* encoder allocation */
    rc = WelsCreateSVCEncoder(&oh264_data->enc);
    if (rc != 0)
        goto on_error;

    oh264_data->esrc_pic = PJ_POOL_ZALLOC_T(pool, SSourcePicture);

    /* decoder allocation */
    rc = WelsCreateDecoder(&oh264_data->dec);
    if (rc != 0)
        goto on_error;

    oh264_data->enc->SetOption(ENCODER_OPTION_TRACE_LEVEL, &log_level);
    oh264_data->enc->SetOption(ENCODER_OPTION_TRACE_CALLBACK, &log_cb);
    oh264_data->dec->SetOption(DECODER_OPTION_TRACE_LEVEL, &log_level);
    oh264_data->dec->SetOption(DECODER_OPTION_TRACE_CALLBACK, &log_cb);

    *p_codec = codec;
    return PJ_SUCCESS;

on_error:
    oh264_dealloc_codec(factory, codec);
    return PJMEDIA_CODEC_EFAILED;
}

static pj_status_t oh264_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                       pjmedia_vid_codec *codec )
{
    oh264_codec_data *oh264_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    PJ_UNUSED_ARG(factory);

    oh264_data = (oh264_codec_data*) codec->codec_data;
    if (oh264_data->enc) {
        WelsDestroySVCEncoder(oh264_data->enc);
        oh264_data->enc = NULL;
    }
    if (oh264_data->dec) {
        oh264_data->dec->Uninitialize();
        WelsDestroyDecoder(oh264_data->dec);
        oh264_data->dec = NULL;
    }
    pj_pool_release(oh264_data->pool);
    return PJ_SUCCESS;
}

static pj_status_t oh264_codec_init(pjmedia_vid_codec *codec,
                                    pj_pool_t *pool )
{
    PJ_ASSERT_RETURN(codec && pool, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

static pj_status_t oh264_codec_open(pjmedia_vid_codec *codec,
                                    pjmedia_vid_codec_param *codec_param )
{
    oh264_codec_data    *oh264_data;
    pjmedia_vid_codec_param     *param;
    pjmedia_h264_packetizer_cfg  pktz_cfg;
    pjmedia_vid_codec_h264_fmtp  h264_fmtp;
    SEncParamExt         eprm;
    SSpatialLayerConfig *elayer = &eprm.sSpatialLayers[0];
    SLayerPEncCtx        elayer_ctx;
    SDecodingParam       sDecParam = {0};
    int                  rc;
    pj_status_t          status;

    PJ_ASSERT_RETURN(codec && codec_param, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Opening codec.."));

    oh264_data = (oh264_codec_data*) codec->codec_data;
    oh264_data->prm = pjmedia_vid_codec_param_clone( oh264_data->pool,
                                                     codec_param);
    param = oh264_data->prm;

    /* Parse remote fmtp */
    pj_bzero(&h264_fmtp, sizeof(h264_fmtp));
    status = pjmedia_vid_codec_h264_parse_fmtp(&param->enc_fmtp, &h264_fmtp);
    if (status != PJ_SUCCESS)
        return status;

    /* Apply SDP fmtp to format in codec param */
    if (!param->ignore_fmtp) {
        status = pjmedia_vid_codec_h264_apply_fmtp(param);
        if (status != PJ_SUCCESS)
            return status;
    }

    pj_bzero(&pktz_cfg, sizeof(pktz_cfg));
    pktz_cfg.mtu = param->enc_mtu;
    /* Packetization mode */
#if 0
    if (h264_fmtp.packetization_mode == 0)
        pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_SINGLE_NAL;
    else if (h264_fmtp.packetization_mode == 1)
        pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_NON_INTERLEAVED;
    else
        return PJ_ENOTSUP;
#else
    if (h264_fmtp.packetization_mode!=
                                PJMEDIA_H264_PACKETIZER_MODE_SINGLE_NAL &&
        h264_fmtp.packetization_mode!=
                                PJMEDIA_H264_PACKETIZER_MODE_NON_INTERLEAVED)
    {
        return PJ_ENOTSUP;
    }
    /* Better always send in single NAL mode for better compatibility */
    pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_SINGLE_NAL;
#endif

    status = pjmedia_h264_packetizer_create(oh264_data->pool, &pktz_cfg,
                                            &oh264_data->pktz);
    if (status != PJ_SUCCESS)
        return status;

    oh264_data->whole = (param->packing == PJMEDIA_VID_PACKING_WHOLE);

    /*
     * Encoder
     */

    /* Init encoder parameters */
    oh264_data->enc->GetDefaultParams (&eprm);
    eprm.iComplexityMode                = MEDIUM_COMPLEXITY;
    eprm.sSpatialLayers[0].uiProfileIdc = PRO_BASELINE;
    eprm.iPicWidth                      = param->enc_fmt.det.vid.size.w;
    eprm.iUsageType                     = CAMERA_VIDEO_REAL_TIME;
    eprm.iPicHeight                     = param->enc_fmt.det.vid.size.h;
    eprm.fMaxFrameRate                  = (param->enc_fmt.det.vid.fps.num *
                                           1.0f /
                                           param->enc_fmt.det.vid.fps.denum);
    eprm.iTemporalLayerNum              = 1;
    eprm.uiIntraPeriod                  = 0; /* I-Frame interval in frames */
    eprm.eSpsPpsIdStrategy              = (oh264_data->whole ? CONSTANT_ID :
                                           INCREASING_ID);
    eprm.bEnableFrameCroppingFlag       = true;
    eprm.iLoopFilterDisableIdc          = 0;
    eprm.iLoopFilterAlphaC0Offset       = 0;
    eprm.iLoopFilterBetaOffset          = 0;
    eprm.iMultipleThreadIdc             = 1;
    //eprm.bEnableRc                    = 1;
    eprm.iTargetBitrate                 = param->enc_fmt.det.vid.avg_bps;
    eprm.bEnableFrameSkip               = 1;
    eprm.bEnableDenoise                 = 0;
    eprm.bEnableSceneChangeDetect       = 1;
    eprm.bEnableBackgroundDetection     = 1;
    eprm.bEnableAdaptiveQuant           = 1;
    eprm.bEnableLongTermReference       = 0;
    eprm.iLtrMarkPeriod                 = 30;
    eprm.bPrefixNalAddingCtrl           = false;
    eprm.iSpatialLayerNum               = 1;
    if (!oh264_data->whole) {
        eprm.uiMaxNalSize                       = param->enc_mtu;
    }

    pj_bzero(&elayer_ctx, sizeof (SLayerPEncCtx));
    elayer_ctx.iDLayerQp                = 24;
    elayer_ctx.sSliceArgument.uiSliceMode = (oh264_data->whole ?
                                             SM_SINGLE_SLICE : 
                                             SM_SIZELIMITED_SLICE);

    /* uiSliceSizeConstraint = uiMaxNalSize - NAL_HEADER_ADD_0X30BYTES */
    elayer_ctx.sSliceArgument.uiSliceSizeConstraint = param->enc_mtu - 50;
    elayer_ctx.sSliceArgument.uiSliceNum      = 1;
    elayer_ctx.sSliceArgument.uiSliceMbNum[0] = 960;
    elayer_ctx.sSliceArgument.uiSliceMbNum[1] = 0;
    elayer_ctx.sSliceArgument.uiSliceMbNum[2] = 0;
    elayer_ctx.sSliceArgument.uiSliceMbNum[3] = 0;
    elayer_ctx.sSliceArgument.uiSliceMbNum[4] = 0;
    elayer_ctx.sSliceArgument.uiSliceMbNum[5] = 0;
    elayer_ctx.sSliceArgument.uiSliceMbNum[6] = 0;
    elayer_ctx.sSliceArgument.uiSliceMbNum[7] = 0;

    elayer->iVideoWidth                 = eprm.iPicWidth;
    elayer->iVideoHeight                = eprm.iPicHeight;
    elayer->fFrameRate                  = eprm.fMaxFrameRate;
    elayer->uiProfileIdc                = eprm.sSpatialLayers[0].uiProfileIdc;
    elayer->iSpatialBitrate             = eprm.iTargetBitrate;
    elayer->iDLayerQp                   = elayer_ctx.iDLayerQp;
    elayer->sSliceArgument.uiSliceMode = elayer_ctx.sSliceArgument.uiSliceMode;

    memcpy ( &elayer->sSliceArgument,
             &elayer_ctx.sSliceArgument,
             sizeof (SSliceArgument));
    memcpy ( &elayer->sSliceArgument.uiSliceMbNum[0],
             &elayer_ctx.sSliceArgument.uiSliceMbNum[0],
             sizeof (elayer_ctx.sSliceArgument.uiSliceMbNum));

    /* Init input picture */
    oh264_data->esrc_pic->iColorFormat  = videoFormatI420;
    oh264_data->esrc_pic->uiTimeStamp   = 0;
    oh264_data->esrc_pic->iPicWidth     = eprm.iPicWidth;
    oh264_data->esrc_pic->iPicHeight    = eprm.iPicHeight;
    oh264_data->esrc_pic->iStride[0]    = oh264_data->esrc_pic->iPicWidth;
    oh264_data->esrc_pic->iStride[1]    =
            oh264_data->esrc_pic->iStride[2] =
                                          oh264_data->esrc_pic->iStride[0]>>1;

    oh264_data->enc_input_size = oh264_data->esrc_pic->iPicWidth *
                                 oh264_data->esrc_pic->iPicHeight * 3 >> 1;

    /* Initialize encoder */
    rc = oh264_data->enc->InitializeExt (&eprm);
    if (rc != cmResultSuccess) {
        PJ_LOG(4,(THIS_FILE, "SVC encoder Initialize failed, rc=%d", rc));
        return PJMEDIA_CODEC_EFAILED;
    }

    int videoFormat = videoFormatI420;
    rc = oh264_data->enc->SetOption (ENCODER_OPTION_DATAFORMAT, &videoFormat);
    if (rc != cmResultSuccess) {
        PJ_LOG(4,(THIS_FILE, "SVC encoder SetOption videoFormatI420 failed, "
                             "rc=%d", rc));
        return PJMEDIA_CODEC_EFAILED;
    }
    
    /*
     * Decoder
     */
    sDecParam.sVideoProperty.size       = sizeof (sDecParam.sVideoProperty);
    sDecParam.uiTargetDqLayer           = (pj_uint8_t) - 1;
    sDecParam.eEcActiveIdc              = ERROR_CON_SLICE_COPY;
    sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

    /* Calculate minimum missing keyframe event interval in frames. */
    oh264_data->missing_kf_interval =
        (unsigned)((1.0f * param->dec_fmt.det.vid.fps.num /
        param->dec_fmt.det.vid.fps.denum) *
        MISSING_KEYFRAME_EV_MIN_INTERVAL/1000);
    oh264_data->last_missing_kf_event = oh264_data->missing_kf_interval;

    //TODO:
    // Apply "sprop-parameter-sets" here

    /* Initialize decoder */
    rc = oh264_data->dec->Initialize (&sDecParam);
    if (rc) {
        PJ_LOG(4,(THIS_FILE, "Decoder initialization failed, rc=%d", rc));
        return PJMEDIA_CODEC_EFAILED;
    }

    oh264_data->dec_buf_size = (MAX_RX_WIDTH * MAX_RX_HEIGHT * 3 >> 1) +
                               (MAX_RX_WIDTH);
    oh264_data->dec_buf = (pj_uint8_t*)pj_pool_alloc(oh264_data->pool,
                                                     oh264_data->dec_buf_size);

    /* Need to update param back after values are negotiated */
    pj_memcpy(codec_param, param, sizeof(*codec_param));

    return PJ_SUCCESS;
}

static pj_status_t oh264_codec_close(pjmedia_vid_codec *codec)
{
    PJ_ASSERT_RETURN(codec, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    return PJ_SUCCESS;
}

static pj_status_t oh264_codec_modify(pjmedia_vid_codec *codec,
                                      const pjmedia_vid_codec_param *param)
{
    struct oh264_codec_data *oh264_data;
    int rc;
    SBitrateInfo bitrate;

    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);

    oh264_data = (oh264_codec_data*) codec->codec_data;

    bitrate.iLayer = SPATIAL_LAYER_ALL;
    bitrate.iBitrate = param->enc_fmt.det.vid.avg_bps;
    rc = oh264_data->enc->SetOption (ENCODER_OPTION_BITRATE, &bitrate);
    if (rc != cmResultSuccess) {
        PJ_LOG(4,(THIS_FILE, "OpenH264 encoder SetOption bitrate failed, "
                             "rc=%d", rc));
        return PJMEDIA_CODEC_EUNSUP;
    }

    oh264_data->prm->enc_fmt.det.vid.avg_bps = param->enc_fmt.det.vid.avg_bps;

    bitrate.iBitrate = param->enc_fmt.det.vid.max_bps;
    rc = oh264_data->enc->SetOption (ENCODER_OPTION_MAX_BITRATE, &bitrate);
    if (rc != cmResultSuccess) {
        PJ_LOG(4,(THIS_FILE, "OpenH264 encoder SetOption max bitrate failed, "
                             "rc=%d", rc));
    } else {
        oh264_data->prm->enc_fmt.det.vid.max_bps =
            param->enc_fmt.det.vid.max_bps;

        PJ_LOG(4, (THIS_FILE, "OpenH264 encoder bitrate is modified to "
                              "%d avg bps and %d max bps",
                              param->enc_fmt.det.vid.avg_bps,
                              param->enc_fmt.det.vid.max_bps));
    }

    return PJ_SUCCESS;
}

static pj_status_t oh264_codec_get_param(pjmedia_vid_codec *codec,
                                         pjmedia_vid_codec_param *param)
{
    struct oh264_codec_data *oh264_data;

    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);

    oh264_data = (oh264_codec_data*) codec->codec_data;
    pj_memcpy(param, oh264_data->prm, sizeof(*param));

    return PJ_SUCCESS;
}

static pj_status_t oh264_codec_encode_begin(pjmedia_vid_codec *codec,
                                            const pjmedia_vid_encode_opt *opt,
                                            const pjmedia_frame *input,
                                            unsigned out_size,
                                            pjmedia_frame *output,
                                            pj_bool_t *has_more)
{
    struct oh264_codec_data *oh264_data;
    int rc;

    PJ_ASSERT_RETURN(codec && input && out_size && output && has_more,
                     PJ_EINVAL);

    oh264_data = (oh264_codec_data*) codec->codec_data;

    PJ_ASSERT_RETURN(input->size == oh264_data->enc_input_size,
                     PJMEDIA_CODEC_EFRMINLEN);

    if (opt && opt->force_keyframe) {
        oh264_data->enc->ForceIntraFrame(true);
    }

    oh264_data->esrc_pic->pData[0] = (pj_uint8_t*)input->buf;
    oh264_data->esrc_pic->pData[1] = oh264_data->esrc_pic->pData[0] +
                                        (oh264_data->esrc_pic->iPicWidth *
                                         oh264_data->esrc_pic->iPicHeight);
    oh264_data->esrc_pic->pData[2] = oh264_data->esrc_pic->pData[1] +
                                        (oh264_data->esrc_pic->iPicWidth *
                                         oh264_data->esrc_pic->iPicHeight >>2);

    pj_memset (&oh264_data->bsi, 0, sizeof (SFrameBSInfo));
    rc = oh264_data->enc->EncodeFrame( oh264_data->esrc_pic, &oh264_data->bsi);
    if (rc != cmResultSuccess) {
        PJ_LOG(5,(THIS_FILE, "EncodeFrame() error, ret: %d", rc));
        return PJMEDIA_CODEC_EFAILED;
    }

    if (oh264_data->bsi.eFrameType == videoFrameTypeSkip) {
        output->size = 0;
        output->type = PJMEDIA_FRAME_TYPE_NONE;
        output->timestamp = input->timestamp;
        return PJ_SUCCESS;
    }

    oh264_data->ets = input->timestamp;
    oh264_data->ilayer = 0;
    oh264_data->enc_frame_size = oh264_data->enc_processed = 0;

    if (oh264_data->whole) {
        SLayerBSInfo* pLayerBsInfo;
        pj_uint8_t *payload;
        unsigned i, payload_size = 0;

        *has_more = PJ_FALSE;

        /* Find which layer with biggest payload */
        oh264_data->ilayer = 0;
        payload_size = oh264_data->bsi.sLayerInfo[0].pNalLengthInByte[0];
        for (i=0; i < (unsigned)oh264_data->bsi.iLayerNum; ++i) {
            unsigned j;
            pLayerBsInfo = &oh264_data->bsi.sLayerInfo[i];
            for (j=0; j < (unsigned)pLayerBsInfo->iNalCount; ++j) {
                if (pLayerBsInfo->pNalLengthInByte[j] > (int)payload_size) {
                    payload_size = pLayerBsInfo->pNalLengthInByte[j];
                    oh264_data->ilayer = i;
                }
            }
        }

        pLayerBsInfo = &oh264_data->bsi.sLayerInfo[oh264_data->ilayer];
        if (pLayerBsInfo == NULL) {
            output->size = 0;
            output->type = PJMEDIA_FRAME_TYPE_NONE;
            return PJ_SUCCESS;
        }

        payload = pLayerBsInfo->pBsBuf;
        payload_size = 0;
        for (int inal = pLayerBsInfo->iNalCount - 1; inal >= 0; --inal) {
            payload_size += pLayerBsInfo->pNalLengthInByte[inal];
        }

        if (payload_size > out_size)
            return PJMEDIA_CODEC_EFRMTOOSHORT;

        output->type = PJMEDIA_FRAME_TYPE_VIDEO;
        output->size = payload_size;
        output->timestamp = input->timestamp;
        pj_memcpy(output->buf, payload, payload_size);

        return PJ_SUCCESS;
    }

    return oh264_codec_encode_more(codec, out_size, output, has_more);
}


static pj_status_t oh264_codec_encode_more(pjmedia_vid_codec *codec,
                                           unsigned out_size,
                                           pjmedia_frame *output,
                                           pj_bool_t *has_more)
{
    struct oh264_codec_data *oh264_data;
    const pj_uint8_t *payload;
    pj_size_t payload_len;
    pj_status_t status;

    PJ_ASSERT_RETURN(codec && out_size && output && has_more,
                     PJ_EINVAL);

    oh264_data = (oh264_codec_data*) codec->codec_data;

    if (oh264_data->enc_processed < oh264_data->enc_frame_size) {
        /* We have outstanding frame in packetizer */
        status = pjmedia_h264_packetize(oh264_data->pktz,
                                        oh264_data->enc_frame_whole,
                                        oh264_data->enc_frame_size,
                                        &oh264_data->enc_processed,
                                        &payload, &payload_len);
        if (status != PJ_SUCCESS) {
            /* Reset */
            oh264_data->enc_frame_size = oh264_data->enc_processed = 0;
            *has_more = (oh264_data->enc_processed <
                            oh264_data->enc_frame_size) ||
                        (oh264_data->ilayer < oh264_data->bsi.iLayerNum);

            PJ_PERROR(3,(THIS_FILE, status, "pjmedia_h264_packetize() error"));
            return status;
        }

        PJ_ASSERT_RETURN(payload_len <= out_size, PJMEDIA_CODEC_EFRMTOOSHORT);

        output->type = PJMEDIA_FRAME_TYPE_VIDEO;
        pj_memcpy(output->buf, payload, payload_len);
        output->size = payload_len;

        if (oh264_data->bsi.eFrameType == videoFrameTypeIDR) {
            output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;
        }

        *has_more = (oh264_data->enc_processed < oh264_data->enc_frame_size) ||
                    (oh264_data->ilayer < oh264_data->bsi.iLayerNum);
        return PJ_SUCCESS;
    }

    if (oh264_data->ilayer >= oh264_data->bsi.iLayerNum) {
        /* No more unretrieved frame */
        goto no_frame;
    }

    SLayerBSInfo* pLayerBsInfo;
    pLayerBsInfo = &oh264_data->bsi.sLayerInfo[oh264_data->ilayer++];
    if (pLayerBsInfo == NULL) {
        goto no_frame;
    }

    oh264_data->enc_frame_size = 0;
    for (int inal = pLayerBsInfo->iNalCount - 1; inal >= 0; --inal) {
        oh264_data->enc_frame_size += pLayerBsInfo->pNalLengthInByte[inal];
    }

    oh264_data->enc_frame_whole = pLayerBsInfo->pBsBuf;
    oh264_data->enc_processed = 0;


    status = pjmedia_h264_packetize(oh264_data->pktz,
                                    oh264_data->enc_frame_whole,
                                    oh264_data->enc_frame_size,
                                    &oh264_data->enc_processed,
                                    &payload, &payload_len);
    if (status != PJ_SUCCESS) {
        /* Reset */
        oh264_data->enc_frame_size = oh264_data->enc_processed = 0;
        *has_more = (oh264_data->ilayer < oh264_data->bsi.iLayerNum);

        PJ_PERROR(3,(THIS_FILE, status, "pjmedia_h264_packetize() error [2]"));
        return status;
    }

    PJ_ASSERT_RETURN(payload_len <= out_size, PJMEDIA_CODEC_EFRMTOOSHORT);

    output->type = PJMEDIA_FRAME_TYPE_VIDEO;
    pj_memcpy(output->buf, payload, payload_len);
    output->size = payload_len;

    if (oh264_data->bsi.eFrameType == videoFrameTypeIDR) {
        output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;
    }

    *has_more = (oh264_data->enc_processed < oh264_data->enc_frame_size) ||
            (oh264_data->ilayer < oh264_data->bsi.iLayerNum);

    return PJ_SUCCESS;

no_frame:
    *has_more = PJ_FALSE;
    output->size = 0;
    output->type = PJMEDIA_FRAME_TYPE_NONE;
    return PJ_SUCCESS;
}

static int write_yuv(pj_uint8_t *buf,
                     unsigned dst_len,
                     unsigned char* pData[3],
                     int iStride[2],
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

    pPtr = pData[0];
    for (i = 0; i < iHeight && (dst + iWidth < max); i++) {
        pj_memcpy(dst, pPtr, iWidth);
        pPtr += iStride[0];
        dst += iWidth;
    }

    if (i < iHeight)
        return -1;

    iHeight = iHeight / 2;
    iWidth = iWidth / 2;
    pPtr = pData[1];
    for (i = 0; i < iHeight && (dst + iWidth <= max); i++) {
        pj_memcpy(dst, pPtr, iWidth);
        pPtr += iStride[1];
        dst += iWidth;
    }

    if (i < iHeight)
        return -1;

    pPtr = pData[2];
    for (i = 0; i < iHeight && (dst + iWidth <= max); i++) {
        pj_memcpy(dst, pPtr, iWidth);
        pPtr += iStride[1];
        dst += iWidth;
    }

    if (i < iHeight)
        return -1;

    return (int)(dst - buf);
}

static pj_status_t oh264_got_decoded_frame(pjmedia_vid_codec *codec,
                                           struct oh264_codec_data *oh264_data,
                                           unsigned char *pData[3],
                                           SBufferInfo *sDstBufInfo,
                                           pj_timestamp *timestamp,
                                           unsigned out_size,
                                           pjmedia_frame *output)
{
    pj_uint8_t* pDst[3] = {NULL};

    pDst[0] = (pj_uint8_t*)pData[0];
    pDst[1] = (pj_uint8_t*)pData[1];
    pDst[2] = (pj_uint8_t*)pData[2];

    /* Do not reset size as it may already contain frame
    output->size = 0;
    */

    if (!pDst[0] || !pDst[1] || !pDst[2]) {
        return PJ_SUCCESS;
    }

    int iStride[2];
    int iWidth = sDstBufInfo->UsrData.sSystemBuffer.iWidth;
    int iHeight = sDstBufInfo->UsrData.sSystemBuffer.iHeight;

    iStride[0] = sDstBufInfo->UsrData.sSystemBuffer.iStride[0];
    iStride[1] = sDstBufInfo->UsrData.sSystemBuffer.iStride[1];

    int len = write_yuv((pj_uint8_t *)output->buf, out_size,
                        pDst, iStride, iWidth, iHeight);
    if (len > 0) {
        output->timestamp = *timestamp;
        output->size = len;
        output->type = PJMEDIA_FRAME_TYPE_VIDEO;
    } else {
        /* buffer is damaged, reset size */
        output->size = 0;
        return PJMEDIA_CODEC_EFRMTOOSHORT;
    }

    /* Detect format change */
    if (iWidth != (int)oh264_data->prm->dec_fmt.det.vid.size.w ||
        iHeight != (int)oh264_data->prm->dec_fmt.det.vid.size.h)
    {
        pjmedia_event event;

        PJ_LOG(4,(THIS_FILE, "Frame size changed: %dx%d --> %dx%d",
                  oh264_data->prm->dec_fmt.det.vid.size.w,
                  oh264_data->prm->dec_fmt.det.vid.size.h,
                  iWidth, iHeight));

        oh264_data->prm->dec_fmt.det.vid.size.w = iWidth;
        oh264_data->prm->dec_fmt.det.vid.size.h = iHeight;

        /* Broadcast format changed event */
        pjmedia_event_init(&event, PJMEDIA_EVENT_FMT_CHANGED,
                           timestamp, codec);
        event.data.fmt_changed.dir = PJMEDIA_DIR_DECODING;
        pjmedia_format_copy(&event.data.fmt_changed.new_fmt,
                            &oh264_data->prm->dec_fmt);
        pjmedia_event_publish(NULL, codec, &event,
                              PJMEDIA_EVENT_PUBLISH_DEFAULT);
    }

    return PJ_SUCCESS;
}

static pj_status_t oh264_codec_decode(pjmedia_vid_codec *codec,
                                      pj_size_t count,
                                      pjmedia_frame packets[],
                                      unsigned out_size,
                                      pjmedia_frame *output)
{
    struct oh264_codec_data *oh264_data;
    unsigned char* pData[3] = {NULL};
    const pj_uint8_t nal_start[] = { 0, 0, 1 };
    SBufferInfo sDstBufInfo;
    pj_bool_t has_frame = PJ_FALSE;
    pj_bool_t kf_requested = PJ_FALSE;
    unsigned buf_pos, whole_len = 0;
    unsigned i;
    pj_status_t status = PJ_SUCCESS;
    DECODING_STATE ret;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    oh264_data = (oh264_codec_data*) codec->codec_data;
    oh264_data->last_missing_kf_event++;
    /* Check if we have recently generated missing keyframe event. */
    if (oh264_data->last_missing_kf_event < oh264_data->missing_kf_interval)
        kf_requested = PJ_TRUE;

    /*
     * Step 1: unpacketize the packets/frames
     */
    whole_len = 0;
    if (oh264_data->whole) {
        for (i=0; i<count; ++i) {
            if (whole_len + packets[i].size > oh264_data->dec_buf_size) {
                PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [1]"));
                return PJMEDIA_CODEC_EFRMTOOSHORT;
            }

            pj_memcpy( oh264_data->dec_buf + whole_len,
                       (pj_uint8_t*)packets[i].buf,
                       packets[i].size);
            whole_len += (unsigned)packets[i].size;
        }

    } else {
        for (i=0; i<count; ++i) {

            if (whole_len + packets[i].size + sizeof(nal_start) >
                                                oh264_data->dec_buf_size)
            {
                PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [1]"));
                return PJMEDIA_CODEC_EFRMTOOSHORT;
            }

            status = pjmedia_h264_unpacketize( oh264_data->pktz,
                                               (pj_uint8_t*)packets[i].buf,
                                               packets[i].size,
                                               oh264_data->dec_buf,
                                               oh264_data->dec_buf_size,
                                               &whole_len);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4,(THIS_FILE, status, "Unpacketize error"));
                continue;
            }
        }
    }

    if (whole_len + sizeof(nal_start) > oh264_data->dec_buf_size) {
        PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [2]"));
        return PJMEDIA_CODEC_EFRMTOOSHORT;
    }

    /* Dummy NAL sentinel */
    pj_memcpy( oh264_data->dec_buf + whole_len, nal_start, sizeof(nal_start));

    /*
     * Step 2: parse the individual NAL and give to decoder
     */
    buf_pos = 0;
    while (1) {
        unsigned frm_size;
        unsigned char *start;

        for (i = 0; buf_pos + i < whole_len; i++) {
            if (oh264_data->dec_buf[buf_pos + i] == 0 &&
                oh264_data->dec_buf[buf_pos + i + 1] == 0 &&
                oh264_data->dec_buf[buf_pos + i + 2] == 1 &&
                i > 1)
            {
                break;
            }
        }
        frm_size = i;

        pj_bzero( pData, sizeof(pData));
        pj_bzero( &sDstBufInfo, sizeof (SBufferInfo));

        start = oh264_data->dec_buf + buf_pos;

        /* Decode */
        ret = oh264_data->dec->DecodeFrame2( start, frm_size, pData,
                                             &sDstBufInfo);

        if (ret != dsErrorFree && !kf_requested) {
            /* Broadcast missing keyframe event */
            pjmedia_event event;
            pjmedia_event_init(&event, PJMEDIA_EVENT_KEYFRAME_MISSING,
                               &packets[0].timestamp, codec);
            pjmedia_event_publish(NULL, codec, &event,
                                  PJMEDIA_EVENT_PUBLISH_DEFAULT);
            kf_requested = PJ_TRUE;
            oh264_data->last_missing_kf_event = 0;
        }

        if (0 && sDstBufInfo.iBufferStatus == 1) {
            // Better to just get the frame later after all NALs are consumed
            // by the decoder, it should have the best quality and save some
            // CPU load.
            /* May overwrite existing frame but that's ok. */
            status = oh264_got_decoded_frame(codec, oh264_data, pData,
                                             &sDstBufInfo,
                                             &packets[0].timestamp, out_size,
                                             output);
            has_frame = (status==PJ_SUCCESS && output->size != 0);
        }

        if (buf_pos + frm_size >= whole_len)
            break;

        buf_pos += frm_size;
    }

    /* Signal that we have no more frames */
    pj_int32_t iEndOfStreamFlag = true;
    oh264_data->dec->SetOption( DECODER_OPTION_END_OF_STREAM,
                                (void*)&iEndOfStreamFlag);

    /* Retrieve the decoded frame */
    pj_bzero(pData, sizeof(pData));
    pj_bzero(&sDstBufInfo, sizeof (SBufferInfo));
    ret = oh264_data->dec->DecodeFrame2 (NULL, 0, pData, &sDstBufInfo);

    if (sDstBufInfo.iBufferStatus == 1 &&
        !(ret & dsRefLost) && !(ret & dsNoParamSets) &&
        !(ret & dsDepLayerLost))
    {
        /* Overwrite existing output frame and that's ok, because we assume
         * newer frame have better quality because it has more NALs
         */
        status = oh264_got_decoded_frame(codec, oh264_data, pData,
                                         &sDstBufInfo, &packets[0].timestamp,
                                         out_size, output);
        has_frame = (status==PJ_SUCCESS && output->size != 0);
    }

    if (ret != dsErrorFree) {
        if (!kf_requested) {
            /* Broadcast missing keyframe event */
            pjmedia_event event;
            pjmedia_event_init(&event, PJMEDIA_EVENT_KEYFRAME_MISSING,
                               &packets[0].timestamp, codec);
            pjmedia_event_publish(NULL, codec, &event,
                                  PJMEDIA_EVENT_PUBLISH_DEFAULT);
            oh264_data->last_missing_kf_event = 0;
        }

        if (has_frame) {
            PJ_LOG(5,(oh264_data->pool->obj_name,
                      "Decoder returned non error free frame, ret=%d", ret));
        }
    }
        
    if (!has_frame) {
        output->type = PJMEDIA_FRAME_TYPE_NONE;
        output->size = 0;
        output->timestamp = packets[0].timestamp;

        PJ_LOG(5,(THIS_FILE, "Decode couldn't produce picture, "
                  "input nframes=%lu, concatenated size=%d bytes, ret=%d",
                  count, whole_len, ret));
    }

    return status;
}

#endif  /* PJMEDIA_HAS_OPENH264_CODEC */

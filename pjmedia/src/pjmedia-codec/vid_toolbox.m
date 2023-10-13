/* 
 * Copyright (C)2017 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-codec/vid_toolbox.h>
#include <pjmedia-codec/h264_packetizer.h>
#include <pjmedia/vid_codec_util.h>
#include <pjmedia/errno.h>
#include <pj/log.h>
#include <pj/math.h>

#if defined(PJMEDIA_HAS_VID_TOOLBOX_CODEC) && \
            PJMEDIA_HAS_VID_TOOLBOX_CODEC != 0 && \
    defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#import <Foundation/Foundation.h>
#import <VideoToolbox/VideoToolbox.h>

#include "TargetConditionals.h"

#define THIS_FILE               "vid_toolbox.m"

#if (defined(PJ_DARWINOS) && PJ_DARWINOS != 0 && TARGET_OS_IPHONE)
#import <UIKit/UIKit.h>
#endif

#define DEFAULT_WIDTH           720
#define DEFAULT_HEIGHT          480
#define DEFAULT_FPS             15
#define DEFAULT_AVG_BITRATE     384000
#define DEFAULT_MAX_BITRATE     512000

#define MAX_RX_WIDTH            1280
#define MAX_RX_HEIGHT           800

#define SPS_PPS_BUF_SIZE        32

/* For better compatibility with other codecs (OpenH264 and x264),
 * we decode the whole packets at once.
 */
#define DECODE_WHOLE            PJ_TRUE

/* Maximum duration from one key frame to the next (in seconds). */
#define KEYFRAME_INTERVAL       PJMEDIA_CODEC_VID_TOOLBOX_MAX_KEYFRAME_INTERVAL

/* vidtoolbox H264 default PT */
#define VT_H264_PT              PJMEDIA_RTP_PT_H264_RSV1
/*
 * Factory operations.
 */
static pj_status_t vtool_test_alloc(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info );
static pj_status_t vtool_default_attr(pjmedia_vid_codec_factory *factory,
                                      const pjmedia_vid_codec_info *info,
                                      pjmedia_vid_codec_param *attr );
static pj_status_t vtool_enum_info(pjmedia_vid_codec_factory *factory,
                                   unsigned *count,
                                   pjmedia_vid_codec_info codecs[]);
static pj_status_t vtool_alloc_codec(pjmedia_vid_codec_factory *factory,
                                     const pjmedia_vid_codec_info *info,
                                     pjmedia_vid_codec **p_codec);
static pj_status_t vtool_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                       pjmedia_vid_codec *codec );


/*
 * Codec operations
 */
static pj_status_t vtool_codec_init(pjmedia_vid_codec *codec,
                                    pj_pool_t *pool );
static pj_status_t vtool_codec_open(pjmedia_vid_codec *codec,
                                    pjmedia_vid_codec_param *param );
static pj_status_t vtool_codec_close(pjmedia_vid_codec *codec);
static pj_status_t vtool_codec_modify(pjmedia_vid_codec *codec,
                                      const pjmedia_vid_codec_param *param);
static pj_status_t vtool_codec_get_param(pjmedia_vid_codec *codec,
                                         pjmedia_vid_codec_param *param);
static pj_status_t vtool_codec_encode_begin(pjmedia_vid_codec *codec,
                                            const pjmedia_vid_encode_opt *opt,
                                            const pjmedia_frame *input,
                                            unsigned out_size,
                                            pjmedia_frame *output,
                                            pj_bool_t *has_more);
static pj_status_t vtool_codec_encode_more(pjmedia_vid_codec *codec,
                                           unsigned out_size,
                                           pjmedia_frame *output,
                                           pj_bool_t *has_more);
static pj_status_t vtool_codec_decode(pjmedia_vid_codec *codec,
                                      pj_size_t count,
                                      pjmedia_frame packets[],
                                      unsigned out_size,
                                      pjmedia_frame *output);

/* Definition for Video Toolbox codecs operations. */
static pjmedia_vid_codec_op vtool_codec_op =
{
    &vtool_codec_init,
    &vtool_codec_open,
    &vtool_codec_close,
    &vtool_codec_modify,
    &vtool_codec_get_param,
    &vtool_codec_encode_begin,
    &vtool_codec_encode_more,
    &vtool_codec_decode,
    NULL
};

/* Definition for Video Toolbox codecs factory operations. */
static pjmedia_vid_codec_factory_op vtool_factory_op =
{
    &vtool_test_alloc,
    &vtool_default_attr,
    &vtool_enum_info,
    &vtool_alloc_codec,
    &vtool_dealloc_codec
};


static struct vtool_factory
{
    pjmedia_vid_codec_factory    base;
    pjmedia_vid_codec_mgr       *mgr;
    pj_pool_factory             *pf;
    pj_pool_t                   *pool;
} vtool_factory;


typedef struct vtool_codec_data
{
    pjmedia_vid_codec           *codec;
    pj_pool_t                   *pool;
    pjmedia_vid_codec_param     *prm;
    pj_bool_t                    whole;
    pjmedia_h264_packetizer     *pktz;

    /* Encoder */
    VTCompressionSessionRef      enc;
    void                        *enc_buf;
    unsigned                     enc_buf_size;

    unsigned                     enc_input_size;
    unsigned                     enc_wxh;
    unsigned                     enc_fps;
    unsigned                     enc_frm_cnt;
    unsigned                     enc_frame_size;
    unsigned                     enc_processed;
    pj_bool_t                    enc_is_keyframe;
    
    /* Decoder */
    VTDecompressionSessionRef    dec;
    pj_uint8_t                  *dec_buf;
    unsigned                     dec_buf_size;
    CMFormatDescriptionRef       dec_format;
    OSStatus                     dec_status;

    unsigned                     dec_sps_size;
    unsigned                     dec_pps_size;
    unsigned char                dec_sps[SPS_PPS_BUF_SIZE];
    unsigned char                dec_pps[SPS_PPS_BUF_SIZE];
    
    pjmedia_frame               *dec_frame;
    pj_bool_t                    dec_fmt_change;
} vtool_codec_data;

/* Prototypes */
static OSStatus create_decoder(struct vtool_codec_data *vtool_data);

#if TARGET_OS_IPHONE
static void dispatch_sync_on_main_queue(void (^block)(void))
{
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}
#endif

PJ_DEF(pj_status_t) pjmedia_codec_vid_toolbox_init(pjmedia_vid_codec_mgr *mgr,
                                                   pj_pool_factory *pf)
{
    const pj_str_t h264_name = { (char*)"H264", 4};
    pj_status_t status;

    if (vtool_factory.pool != NULL) {
        /* Already initialized. */
        return PJ_SUCCESS;
    }

    if (!mgr) mgr = pjmedia_vid_codec_mgr_instance();
    PJ_ASSERT_RETURN(mgr, PJ_EINVAL);

    /* Create Video Toolbox codec factory. */
    vtool_factory.base.op = &vtool_factory_op;
    vtool_factory.base.factory_data = NULL;
    vtool_factory.mgr = mgr;
    vtool_factory.pf = pf;
    vtool_factory.pool = pj_pool_create(pf, "vtoolfactory", 256, 256, NULL);
    if (!vtool_factory.pool)
        return PJ_ENOMEM;

    /* Registering format match for SDP negotiation */
    status = pjmedia_sdp_neg_register_fmt_match_cb(
                                        &h264_name,
                                        &pjmedia_vid_codec_h264_match_sdp);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Register codec factory to codec manager. */
    status = pjmedia_vid_codec_mgr_register_factory(mgr,
                                                    &vtool_factory.base);
    if (status != PJ_SUCCESS)
        goto on_error;

    PJ_LOG(4,(THIS_FILE, "Video Toolbox codec initialized"));

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(vtool_factory.pool);
    vtool_factory.pool = NULL;
    return status;
}

/*
 * Unregister Video Toolbox codecs factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_vid_toolbox_deinit(void)
{
    pj_status_t status = PJ_SUCCESS;

    if (vtool_factory.pool == NULL) {
        /* Already deinitialized */
        return PJ_SUCCESS;
    }

    /* Unregister Video Toolbox codecs factory. */
    status = pjmedia_vid_codec_mgr_unregister_factory(vtool_factory.mgr,
                                                      &vtool_factory.base);

    /* Destroy pool. */
    pj_pool_release(vtool_factory.pool);
    vtool_factory.pool = NULL;

    return status;
}

static pj_status_t vtool_test_alloc(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info )
{
    PJ_ASSERT_RETURN(factory == &vtool_factory.base, PJ_EINVAL);

    if (info->fmt_id == PJMEDIA_FORMAT_H264 &&
        info->pt == VT_H264_PT)
    {
        return PJ_SUCCESS;
    }

    return PJMEDIA_CODEC_EUNSUP;
}

static pj_status_t vtool_default_attr(pjmedia_vid_codec_factory *factory,
                                      const pjmedia_vid_codec_info *info,
                                      pjmedia_vid_codec_param *attr )
{
    PJ_ASSERT_RETURN(factory == &vtool_factory.base, PJ_EINVAL);
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

static pj_status_t vtool_enum_info(pjmedia_vid_codec_factory *factory,
                                   unsigned *count,
                                   pjmedia_vid_codec_info info[])
{
    PJ_ASSERT_RETURN(info && *count > 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &vtool_factory.base, PJ_EINVAL);

    *count = 1;
    info->fmt_id = PJMEDIA_FORMAT_H264;
    info->pt = VT_H264_PT;
    info->encoding_name = pj_str((char*)"H264");
    info->encoding_desc = pj_str((char*)"Video Toolbox codec");
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

static pj_status_t vtool_alloc_codec(pjmedia_vid_codec_factory *factory,
                                     const pjmedia_vid_codec_info *info,
                                     pjmedia_vid_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_vid_codec *codec;
    vtool_codec_data *vtool_data;

    PJ_ASSERT_RETURN(factory == &vtool_factory.base && info && p_codec,
                     PJ_EINVAL);

    *p_codec = NULL;

    pool = pj_pool_create(vtool_factory.pf, "vtool%p", 16000, 4000, NULL);
    if (!pool)
        return PJ_ENOMEM;

    /* codec instance */
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_codec);
    codec->factory = factory;
    codec->op = &vtool_codec_op;

    /* codec data */
    vtool_data = PJ_POOL_ZALLOC_T(pool, vtool_codec_data);
    vtool_data->pool = pool;
    vtool_data->codec = codec;
    codec->codec_data = vtool_data;

    *p_codec = codec;
    return PJ_SUCCESS;
}

static pj_status_t vtool_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                       pjmedia_vid_codec *codec )
{
    vtool_codec_data *vtool_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    PJ_UNUSED_ARG(factory);

    vtool_data = (vtool_codec_data*) codec->codec_data;
    pj_pool_release(vtool_data->pool);
    return PJ_SUCCESS;
}

static pj_status_t vtool_codec_init(pjmedia_vid_codec *codec,
                                    pj_pool_t *pool )
{
    PJ_ASSERT_RETURN(codec && pool, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

static void encode_cb(void *outputCallbackRefCon,
                      void *sourceFrameRefCon,
                      OSStatus status,
                      VTEncodeInfoFlags infoFlags,
                      CMSampleBufferRef sampleBuffer)
{
    struct vtool_codec_data *vtool_data;
    const pj_uint8_t start_code[] = { 0, 0, 0, 1 };
    const int code_size = PJ_ARRAY_SIZE(start_code);
    const int avcc_size = sizeof(uint32_t);
    CFArrayRef array;
    CFDictionaryRef dict = NULL;
    CMBlockBufferRef block_buf;
    size_t offset = 0, length = 0;
    size_t buf_pos;
    char *data, *buf;

    /* This callback can be called from another, unregistered thread.
     * So do not call pjlib functions here.
     */

    if (status != noErr || !CMSampleBufferDataIsReady(sampleBuffer)) return;

    vtool_data = (struct vtool_codec_data *)outputCallbackRefCon;
    vtool_data->enc_is_keyframe = PJ_FALSE;
    buf = vtool_data->enc_buf;

    /* Check if the encoded frame is keyframe */
    array = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    if (array) dict = (CFDictionaryRef)CFArrayGetValueAtIndex(array, 0);
    if (dict && !CFDictionaryContainsKey(dict,kCMSampleAttachmentKey_NotSync))
        vtool_data->enc_is_keyframe = PJ_TRUE;

    if (vtool_data->enc_is_keyframe) {
        CMFormatDescriptionRef format;
        size_t enc_sps_size, enc_sps_cnt;
        size_t enc_pps_size, enc_pps_cnt;
        const uint8_t *enc_sps, *enc_pps;
        OSStatus status;
        
        format = CMSampleBufferGetFormatDescription(sampleBuffer);
       
        /* Get SPS */
        status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
                     format, 0, &enc_sps, &enc_sps_size, &enc_sps_cnt, 0 );
        if (status != noErr) return;

        /* Get PPS */
        status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
                     format, 1, &enc_pps, &enc_pps_size, &enc_pps_cnt, 0 );
        if (status != noErr) return;

        /* Append SPS and PPS to output frame */
        pj_assert (enc_sps_size + enc_pps_size + 2 * code_size <=
                   vtool_data->enc_buf_size);
        pj_memcpy(buf + offset, start_code, code_size);
        offset += code_size;
        pj_memcpy(buf + offset, enc_sps, enc_sps_size);
        offset += enc_sps_size;
        pj_memcpy(buf + offset, start_code, code_size);
        offset += code_size;
        pj_memcpy(buf + offset, enc_pps, enc_pps_size);
        offset += enc_pps_size;
    }
    
    pj_assert(CMSampleBufferGetNumSamples(sampleBuffer) == 1);

    /* Get data pointer of the encoded frame */
    block_buf = CMSampleBufferGetDataBuffer(sampleBuffer);
    status = CMBlockBufferGetDataPointer(block_buf, 0, &length, NULL, &data);
    if (status != noErr || (offset + length) > vtool_data->enc_buf_size)
        return;
        
    pj_assert(CMBlockBufferIsRangeContiguous(block_buf, 0, length));
    pj_assert(length == CMBlockBufferGetDataLength(block_buf));

    buf_pos = 0;
    while (buf_pos < length - avcc_size) {
        uint32_t data_length;

        /* Get data length and copy the data to the output buffer */
        pj_memcpy(&data_length, data + buf_pos, avcc_size);
        data_length = pj_ntohl(data_length);
        pj_assert(buf_pos + data_length + avcc_size <= length);
        pj_memcpy(buf + offset, data + buf_pos, data_length + avcc_size);

        /* Replace data length with NAL start code */
        pj_memcpy(buf + offset, start_code, code_size);
        
        buf_pos += avcc_size + data_length;
        offset += avcc_size + data_length;
    }
    vtool_data->enc_frame_size = offset;
}


static OSStatus create_encoder(vtool_codec_data *vtool_data)
{
    pjmedia_vid_codec_param     *param = vtool_data->prm;
    CFDictionaryRef              supported_prop;
    OSStatus                     ret;

    /* Destroy if initialized before */
    if (vtool_data->enc) {
        VTCompressionSessionInvalidate(vtool_data->enc);
        CFRelease(vtool_data->enc);
        vtool_data->enc = NULL;
    }

    /* Create encoder session */
    ret = VTCompressionSessionCreate(NULL, (int)param->enc_fmt.det.vid.size.w,
                                     (int)param->enc_fmt.det.vid.size.h, 
                                     kCMVideoCodecType_H264, NULL, NULL,
                                     NULL, encode_cb, vtool_data,
                                     &vtool_data->enc);
    if (ret != noErr) {
        PJ_LOG(4,(THIS_FILE, "VTCompressionCreate failed, ret=%d", ret));
        return ret;
    }

#define SET_PROPERTY(sess, prop, val) \
{ \
    ret = VTSessionSetProperty(sess, prop, val); \
    if (ret != noErr) \
        PJ_LOG(5,(THIS_FILE, "Failed to set session property %s", #prop)); \
}

    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_ProfileLevel,
                 kVTProfileLevel_H264_Baseline_AutoLevel);
    SET_PROPERTY(vtool_data->enc, kVTCompressionPropertyKey_RealTime,
                 kCFBooleanTrue);
    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_AllowFrameReordering,
                 kCFBooleanFalse);
    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_AverageBitRate,
                 (__bridge CFTypeRef)@(param->enc_fmt.det.vid.avg_bps));
    vtool_data->enc_fps = param->enc_fmt.det.vid.fps.num /
                          param->enc_fmt.det.vid.fps.denum;
    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_ExpectedFrameRate,
                 (__bridge CFTypeRef)@(vtool_data->enc_fps));
    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_DataRateLimits,
                 ((__bridge CFArrayRef) // [Bytes, second]
                 @[@(param->enc_fmt.det.vid.max_bps >> 3), @(1)]));
    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_MaxKeyFrameInterval,
                 (__bridge CFTypeRef)@(KEYFRAME_INTERVAL *
                 param->enc_fmt.det.vid.fps.num /
                 param->enc_fmt.det.vid.fps.denum));
    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration,
                 (__bridge CFTypeRef)@(KEYFRAME_INTERVAL));

    ret = VTSessionCopySupportedPropertyDictionary(vtool_data->enc,
                                                   &supported_prop);
    if (ret == noErr &&
        CFDictionaryContainsKey(supported_prop,
                                kVTCompressionPropertyKey_MaxH264SliceBytes))
    {
        /* kVTCompressionPropertyKey_MaxH264SliceBytes is not yet supported
         * by Apple. We leave it here for possible future enhancements.
        SET_PROPERTY(vtool_data->enc,
                     kVTCompressionPropertyKey_MaxH264SliceBytes,
                     // param->enc_mtu - NAL_HEADER_ADD_0X30BYTES
                     (__bridge CFTypeRef)@(param->enc_mtu - 50));
         */
    }

    VTCompressionSessionPrepareToEncodeFrames(vtool_data->enc);

    PJ_LOG(4, (THIS_FILE, "Video Toolbox encoder bitrate initialized to "
                          "%d avg bps and %d max bps",
                          param->enc_fmt.det.vid.avg_bps,
                          param->enc_fmt.det.vid.max_bps));

    return ret;
}


static pj_status_t vtool_codec_open(pjmedia_vid_codec *codec,
                                    pjmedia_vid_codec_param *codec_param )
{
    vtool_codec_data            *vtool_data;
    pjmedia_vid_codec_param     *param;
    pjmedia_h264_packetizer_cfg  pktz_cfg;
    pjmedia_vid_codec_h264_fmtp  h264_fmtp;
    pj_status_t                  status;
    OSStatus                     ret;

    PJ_ASSERT_RETURN(codec && codec_param, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Opening codec.."));

    vtool_data = (vtool_codec_data*) codec->codec_data;
    vtool_data->prm = pjmedia_vid_codec_param_clone( vtool_data->pool,
                                                     codec_param);
    param = vtool_data->prm;

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
    pktz_cfg.unpack_nal_start = 4;
    /* Packetization mode */
    if (h264_fmtp.packetization_mode == 0)
        pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_SINGLE_NAL;
    else if (h264_fmtp.packetization_mode == 1)
        pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_NON_INTERLEAVED;
    else
        return PJ_ENOTSUP;
    /* Video Toolbox encoder doesn't support setting maximum slice size,
     * so we cannot use single NAL mode since the NAL size likely
     * exceeds the MTU.
     */
    pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_NON_INTERLEAVED;

    status = pjmedia_h264_packetizer_create(vtool_data->pool, &pktz_cfg,
                                            &vtool_data->pktz);
    if (status != PJ_SUCCESS)
        return status;

    vtool_data->whole = (param->packing == PJMEDIA_VID_PACKING_WHOLE);
    if (1) {
        /* Init format info and apply-param of encoder */
        const pjmedia_video_format_info *enc_vfi;
        pjmedia_video_apply_fmt_param    enc_vafp;

        enc_vfi = pjmedia_get_video_format_info(NULL,codec_param->dec_fmt.id);
        if (!enc_vfi)
            return PJ_EINVAL;
    
        pj_bzero(&enc_vafp, sizeof(enc_vafp));
        enc_vafp.size = codec_param->enc_fmt.det.vid.size;
        enc_vafp.buffer = NULL;
        status = (*enc_vfi->apply_fmt)(enc_vfi, &enc_vafp);
        if (status != PJ_SUCCESS)
            return status;

        vtool_data->enc_wxh = codec_param->enc_fmt.det.vid.size.w *
                              codec_param->enc_fmt.det.vid.size.h;
        vtool_data->enc_input_size = enc_vafp.framebytes;
        if (!vtool_data->whole) {
            vtool_data->enc_buf_size = (unsigned)enc_vafp.framebytes;
            vtool_data->enc_buf = pj_pool_alloc(vtool_data->pool,
                                                vtool_data->enc_buf_size);
        }
    }

    /* Create encoder */
    ret = create_encoder(vtool_data);
    if (ret != noErr)
        return PJMEDIA_CODEC_EFAILED;

    /* If available, use the "sprop-parameter-sets" fmtp from remote SDP
     * to create the decoder.
     */
    if (h264_fmtp.sprop_param_sets_len) {
        const pj_uint8_t start_code[3] = {0, 0, 1};
        const int code_size = PJ_ARRAY_SIZE(start_code);
        unsigned i, j;
        
        for (i = h264_fmtp.sprop_param_sets_len-code_size; i >= code_size;
             i--)
        {
            for (j = 0; j < code_size; j++) {
                if (h264_fmtp.sprop_param_sets[i+j] != start_code[j]) {
                    break;
                }
            }
        }
        
        if (i >= code_size) {
            vtool_data->dec_sps_size = i - code_size;
            pj_memcpy(vtool_data->dec_sps,
                      &h264_fmtp.sprop_param_sets[code_size],
                      vtool_data->dec_sps_size);

            vtool_data->dec_pps_size = h264_fmtp.sprop_param_sets_len - 
                                       code_size-i;
            pj_memcpy(vtool_data->dec_pps,
                      &h264_fmtp.sprop_param_sets[i + code_size],
                      vtool_data->dec_pps_size);
                      
            create_decoder(vtool_data);
        }
    }
    
    /* Create decoder buffer */
    vtool_data->dec_buf_size = (MAX_RX_WIDTH * MAX_RX_HEIGHT * 3 >> 1) +
                               (MAX_RX_WIDTH);
    vtool_data->dec_buf = (pj_uint8_t*)pj_pool_alloc(vtool_data->pool,
                                                     vtool_data->dec_buf_size);

    /* Need to update param back after values are negotiated */
    pj_memcpy(codec_param, param, sizeof(*codec_param));

    return PJ_SUCCESS;
}

static pj_status_t vtool_codec_close(pjmedia_vid_codec *codec)
{
    struct vtool_codec_data *vtool_data;
 
    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    vtool_data = (vtool_codec_data*) codec->codec_data;

    if (vtool_data->enc) {
        VTCompressionSessionInvalidate(vtool_data->enc);
        CFRelease(vtool_data->enc);
        vtool_data->enc = NULL;
    }

    if (vtool_data->dec) {
        VTDecompressionSessionInvalidate(vtool_data->dec);
        CFRelease(vtool_data->dec);
        vtool_data->dec = NULL;
    }
    
    if (vtool_data->dec_format)
        CFRelease(vtool_data->dec_format);

    return PJ_SUCCESS;
}

static pj_status_t vtool_codec_modify(pjmedia_vid_codec *codec,
                                      const pjmedia_vid_codec_param *param)
{
    struct vtool_codec_data *vtool_data;
    OSStatus ret;

    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);

    vtool_data = (vtool_codec_data*) codec->codec_data;

    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_AverageBitRate,
                 (__bridge CFTypeRef)@(param->enc_fmt.det.vid.avg_bps));
    if (ret != noErr)
        return PJMEDIA_CODEC_EUNSUP;

    vtool_data->prm->enc_fmt.det.vid.avg_bps = param->enc_fmt.det.vid.avg_bps;

    SET_PROPERTY(vtool_data->enc,
                 kVTCompressionPropertyKey_DataRateLimits,
                 ((__bridge CFArrayRef) // [Bytes, second]
                 @[@(param->enc_fmt.det.vid.max_bps >> 3), @(1)]));
    if (ret == noErr) {
        vtool_data->prm->enc_fmt.det.vid.max_bps =
            param->enc_fmt.det.vid.max_bps;

        PJ_LOG(4, (THIS_FILE, "Video Toolbox encoder bitrate is modified to "
                              "%d avg bps and %d max bps",
                              param->enc_fmt.det.vid.avg_bps,
                              param->enc_fmt.det.vid.max_bps));
    }

    return PJ_SUCCESS;
}

static pj_status_t vtool_codec_get_param(pjmedia_vid_codec *codec,
                                         pjmedia_vid_codec_param *param)
{
    struct vtool_codec_data *vtool_data;

    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);

    vtool_data = (vtool_codec_data*) codec->codec_data;
    pj_memcpy(param, vtool_data->prm, sizeof(*param));

    return PJ_SUCCESS;
}

static pj_status_t vtool_codec_encode_begin(pjmedia_vid_codec *codec,
                                            const pjmedia_vid_encode_opt *opt,
                                            const pjmedia_frame *input,
                                            unsigned out_size,
                                            pjmedia_frame *output,
                                            pj_bool_t *has_more)
{
    struct vtool_codec_data *vtool_data;
    CMTime ts, dur;
    CVImageBufferRef image_buf;
    void *base_addr[3];
    size_t plane_w[3], plane_h[3], plane_bpr[3];
    NSDictionary *frm_prop = NULL;
    OSStatus ret;
 
    PJ_ASSERT_RETURN(codec && input && out_size && output && has_more,
                     PJ_EINVAL);

    vtool_data = (vtool_codec_data*) codec->codec_data;

    base_addr[0] = input->buf;
    base_addr[1] = input->buf + vtool_data->enc_wxh;
    base_addr[2] = base_addr[1] + (vtool_data->enc_wxh >> 2);
    plane_w[0] = vtool_data->prm->enc_fmt.det.vid.size.w;
    plane_h[0] = vtool_data->prm->enc_fmt.det.vid.size.h;
    plane_w[1] = plane_w[2] = vtool_data->prm->enc_fmt.det.vid.size.w >> 1;
    plane_h[1] = plane_h[2] = vtool_data->prm->enc_fmt.det.vid.size.h >> 1;
    plane_bpr[0] = vtool_data->prm->enc_fmt.det.vid.size.w;
    plane_bpr[1] = plane_bpr[2] = vtool_data->prm->enc_fmt.det.vid.size.w >> 1;

#if TARGET_OS_IPHONE
    ret = CVPixelBufferCreate(NULL, 
                vtool_data->prm->enc_fmt.det.vid.size.w,
                vtool_data->prm->enc_fmt.det.vid.size.h,
                kCVPixelFormatType_420YpCbCr8Planar, /* I420 */
                NULL, &image_buf);
    if (ret == noErr) {
        size_t i, count;

        CVPixelBufferLockBaseAddress(image_buf, 0);

        count = CVPixelBufferGetPlaneCount(image_buf);
        for (i = 0; i < count; i++) {
            char *ptr = (char*)CVPixelBufferGetBaseAddressOfPlane(image_buf, i);
            char *src = (char*)base_addr[i];
            size_t bpr = CVPixelBufferGetBytesPerRowOfPlane(image_buf, i);
            int j;

            pj_assert(bpr >= plane_bpr[i]);
            for (j = 0; j < plane_h[i]; ++j) {
                pj_memcpy(ptr, src, plane_bpr[i]);
                src += plane_bpr[i];
                ptr += bpr;
            }
        }

        CVPixelBufferUnlockBaseAddress(image_buf, 0);
    }
#else
    ret = CVPixelBufferCreateWithPlanarBytes(NULL,
                vtool_data->prm->enc_fmt.det.vid.size.w,
                vtool_data->prm->enc_fmt.det.vid.size.h,
                kCVPixelFormatType_420YpCbCr8Planar, /* I420 */
                NULL, vtool_data->enc_input_size,
                3, /* number of planes of I420 */
                base_addr,
                (size_t *)plane_w, (size_t *)plane_h, (size_t *)plane_bpr,
                NULL, NULL, NULL, &image_buf);
#endif

    if (ret != noErr) {
        PJ_LOG(4,(THIS_FILE, "Failed to create pixel buffer"));
        return PJMEDIA_CODEC_EFAILED;
    }

    ts = CMTimeMake(++vtool_data->enc_frm_cnt, vtool_data->enc_fps);
    dur = CMTimeMake(1, vtool_data->enc_fps);
    vtool_data->enc_frame_size = vtool_data->enc_processed = 0;

    if (vtool_data->whole) {
        vtool_data->enc_buf = output->buf;
        vtool_data->enc_buf_size = out_size;
    }

    if (opt && opt->force_keyframe) {
        frm_prop = @{ (__bridge NSString *)
                      kVTEncodeFrameOptionKey_ForceKeyFrame: @YES };
    }

    ret = VTCompressionSessionEncodeFrame(vtool_data->enc, image_buf, 
                                          ts, dur,
                                          (__bridge CFDictionaryRef)frm_prop,
                                          NULL, NULL);
    if (ret == kVTInvalidSessionErr) {
#if TARGET_OS_IPHONE
        /* Just return if app is not active, i.e. in the bg. */
        __block UIApplicationState state;

        dispatch_sync_on_main_queue(^{
            state = [UIApplication sharedApplication].applicationState;
        });
        if (state != UIApplicationStateActive) {
            *has_more = PJ_FALSE;
            output->size = 0;
            output->type = PJMEDIA_FRAME_TYPE_NONE;

            CVPixelBufferRelease(image_buf);
            return PJ_SUCCESS;
        }
#endif

        /* Reset compression session */
        ret = create_encoder(vtool_data);
        PJ_LOG(3,(THIS_FILE, "Encoder needs to be reset [1]: %s (%d)",
                  (ret == noErr? "success": "fail"), ret));
        if (ret == noErr) {
            /* Retry encoding the frame after successful encoder reset. */
            ret = VTCompressionSessionEncodeFrame(vtool_data->enc, image_buf,
                                                  ts, dur,
                                                  (__bridge CFDictionaryRef)
                                                  frm_prop,
                                                  NULL, NULL);
        }
    }

    if (ret != noErr) {
        PJ_LOG(4,(THIS_FILE, "Failed to encode frame %d", ret));
        CVPixelBufferRelease(image_buf);
        return PJMEDIA_CODEC_EFAILED;
    }
    
    /* EncodeFrame is async, so tell it to finish the encoding. */
    ts.flags = kCMTimeFlags_Indefinite;
    ret = VTCompressionSessionCompleteFrames(vtool_data->enc, ts);
    if (ret == kVTInvalidSessionErr) {
        /* Reset compression session */
        ret = create_encoder(vtool_data);
        PJ_LOG(3,(THIS_FILE, "Encoder needs to be reset [2]: %s (%d)",
                  (ret == noErr? "success": "fail"), ret));
        if (ret == PJ_SUCCESS) {
            /* Retry finishing the encoding after successful encoder reset. */
            ret = VTCompressionSessionCompleteFrames(vtool_data->enc, ts);
        }
    }

    if (ret != noErr) {
        PJ_LOG(4,(THIS_FILE, "Failed to complete encoding %d", ret));
        CVPixelBufferRelease(image_buf);
        return PJMEDIA_CODEC_EFAILED;
    }

    CVPixelBufferRelease(image_buf);

    if (vtool_data->whole) {
        *has_more = PJ_FALSE;
        output->size = vtool_data->enc_frame_size;
        return PJ_SUCCESS;
    }    

    return vtool_codec_encode_more(codec, out_size, output, has_more);
}


static pj_status_t vtool_codec_encode_more(pjmedia_vid_codec *codec,
                                           unsigned out_size,
                                           pjmedia_frame *output,
                                           pj_bool_t *has_more)
{
    struct vtool_codec_data *vtool_data;
    const pj_uint8_t *payload;
    pj_size_t payload_len;
    pj_status_t status;

    PJ_ASSERT_RETURN(codec && out_size && output && has_more,
                     PJ_EINVAL);

    vtool_data = (vtool_codec_data*) codec->codec_data;

    if (vtool_data->enc_processed >= vtool_data->enc_frame_size) {
        /* No more frame */
        *has_more = PJ_FALSE;
        output->size = 0;
        output->type = PJMEDIA_FRAME_TYPE_NONE;

        return PJ_SUCCESS;
    }

    /* We have outstanding frame in packetizer */
    status = pjmedia_h264_packetize(vtool_data->pktz,
                                    (pj_uint8_t*)vtool_data->enc_buf,
                                    vtool_data->enc_frame_size,
                                    &vtool_data->enc_processed,
                                    &payload, &payload_len);
    if (status != PJ_SUCCESS) {
        /* Reset */
        vtool_data->enc_frame_size = vtool_data->enc_processed = 0;
        *has_more = (vtool_data->enc_processed < vtool_data->enc_frame_size);
    
        PJ_PERROR(4,(THIS_FILE, status, "pjmedia_h264_packetize() error"));
        return status;
    }
    
    PJ_ASSERT_RETURN(payload_len <= out_size, PJMEDIA_CODEC_EFRMTOOSHORT);
    
    output->type = PJMEDIA_FRAME_TYPE_VIDEO;
    pj_memcpy(output->buf, payload, payload_len);
    output->size = payload_len;
    if (vtool_data->enc_is_keyframe)
        output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;

    *has_more = (vtool_data->enc_processed < vtool_data->enc_frame_size);
    return PJ_SUCCESS;
}


/* Copy I420 frame from source to destination and clip if necessary */
static int process_i420(CVImageBufferRef src_buf, pj_uint8_t *dst)
{
    pj_uint8_t *pdst = dst;
    pj_size_t i, count;
    
    count = CVPixelBufferGetPlaneCount(src_buf);
    for (i = 0; i < count; i++) {
        pj_uint8_t *psrc;
        pj_size_t src_w, dst_w, h;
        
        psrc = CVPixelBufferGetBaseAddressOfPlane(src_buf, i);
        src_w = CVPixelBufferGetBytesPerRowOfPlane(src_buf, i);
        dst_w = CVPixelBufferGetWidthOfPlane(src_buf, i);
        h = CVPixelBufferGetHeightOfPlane(src_buf, i);

        /* Check if clipping is required */
        if (src_w == dst_w) {
            pj_size_t plane_size = dst_w * h;
            pj_memcpy(pdst, psrc, plane_size);
            pdst += plane_size;
        } else {
            pj_size_t j = 0;
            for (; j < h; ++j) {
                pj_memcpy(pdst, psrc, dst_w);
                pdst += dst_w;
                psrc += src_w;
            }
        }
    }
    
    return (pdst - dst);
}


static void decode_cb(void *decompressionOutputRefCon,
                      void *sourceFrameRefCon,
                      OSStatus status,
                      VTDecodeInfoFlags infoFlags,
                      CVImageBufferRef imageBuffer,
                      CMTime presentationTimeStamp,
                      CMTime presentationDuration)
{
    struct vtool_codec_data *vtool_data;
    pj_size_t width, height, len = 0;
    
    /* This callback can be called from another, unregistered thread.
     * So do not call pjlib functions here.
     */
    vtool_data = (struct vtool_codec_data *)decompressionOutputRefCon;
    vtool_data->dec_status = status;
    if (vtool_data->dec_status != noErr)
        return;

    CVPixelBufferLockBaseAddress(imageBuffer,0);
    
    width = CVPixelBufferGetWidth(imageBuffer); 
    height = CVPixelBufferGetHeight(imageBuffer);

    /* Detect format change */
    if (width != vtool_data->prm->dec_fmt.det.vid.size.w ||
        height != vtool_data->prm->dec_fmt.det.vid.size.h)
    {
        vtool_data->dec_fmt_change = PJ_TRUE;
        vtool_data->prm->dec_fmt.det.vid.size.w = width;
        vtool_data->prm->dec_fmt.det.vid.size.h = height;
    } else {
        vtool_data->dec_fmt_change = PJ_FALSE;
    }

    if (vtool_data->dec_frame->size >= width * height * 3 / 2) {
        len = process_i420(imageBuffer,
                           (pj_uint8_t *)vtool_data->dec_frame->buf);
    } else {
        vtool_data->dec_status = (OSStatus)PJMEDIA_CODEC_EFRMTOOSHORT;
    }
    vtool_data->dec_frame->size = len;

    CVPixelBufferUnlockBaseAddress(imageBuffer,0);
}

static OSStatus create_decoder(struct vtool_codec_data *vtool_data)
{
    uint8_t *param_ptrs[2] = {vtool_data->dec_sps,
                              vtool_data->dec_pps};
    const size_t param_sizes[2] = {vtool_data->dec_sps_size,
                                   vtool_data->dec_pps_size};
    const int code_size = 4; // PJ_ARRAY_SIZE(start_code);
    CMFormatDescriptionRef dec_format;
    VTDecompressionOutputCallbackRecord cbr;
    NSDictionary *dst_attr;
    OSStatus ret;

    /* Create video format description based on H264 SPS and PPS
     * parameters.
     */
    ret = CMVideoFormatDescriptionCreateFromH264ParameterSets(
              kCFAllocatorDefault, 2,
              (const uint8_t * const *)param_ptrs, 
              param_sizes, code_size, &dec_format);
    if (ret != noErr) {
        PJ_LOG(4,(THIS_FILE, "Failed to create video format "
                             "description %d", ret));
        return ret;
     }

    if (!vtool_data->dec || !vtool_data->dec_format ||
        !CMFormatDescriptionEqual(dec_format, vtool_data->dec_format))
    {
        if (vtool_data->dec_format)
            CFRelease(vtool_data->dec_format);
        vtool_data->dec_format = dec_format;
     } else {
        CFRelease(dec_format);
        return noErr;
     }

    cbr.decompressionOutputCallback = decode_cb;
    cbr.decompressionOutputRefCon = vtool_data;

    if (vtool_data->dec) {
        VTDecompressionSessionInvalidate(vtool_data->dec);
        CFRelease(vtool_data->dec);
        vtool_data->dec = NULL;
    }

    dst_attr = [NSDictionary dictionaryWithObjectsAndKeys:
                    [NSNumber numberWithInt: /* I420 */
                        kCVPixelFormatType_420YpCbCr8Planar],
                    kCVPixelBufferPixelFormatTypeKey,
                    nil];
    ret = VTDecompressionSessionCreate(NULL, vtool_data->dec_format, NULL,
                                       (__bridge CFDictionaryRef)dst_attr,
                                       &cbr, &vtool_data->dec);
    if (ret != noErr) {
        PJ_LOG(3,(THIS_FILE, "Failed to create decompression session %d",
                  ret));
    }
    
    SET_PROPERTY(vtool_data->dec, kVTCompressionPropertyKey_RealTime,
                 kCFBooleanTrue);
#if !TARGET_OS_IPHONE
    SET_PROPERTY(vtool_data->dec,
        kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
        kCFBooleanTrue);
#endif
    
    return ret;
}

static pj_status_t vtool_codec_decode(pjmedia_vid_codec *codec,
                                      pj_size_t count,
                                      pjmedia_frame packets[],
                                      unsigned out_size,
                                      pjmedia_frame *output)
{
    struct vtool_codec_data *vtool_data;
    const pj_uint8_t start_code[] = { 0, 0, 0, 1 };
    const int code_size = PJ_ARRAY_SIZE(start_code);
    pj_bool_t has_frame = PJ_FALSE;
    unsigned buf_pos, whole_len = 0;
    unsigned i;
    pj_status_t status = PJ_SUCCESS;
    pj_bool_t decode_whole = DECODE_WHOLE;
    OSStatus ret;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    vtool_data = (vtool_codec_data*) codec->codec_data;

    /*
     * Step 1: unpacketize the packets/frames
     */
    whole_len = 0;
    if (vtool_data->whole) {
        for (i=0; i<count; ++i) {
            if (whole_len + packets[i].size > vtool_data->dec_buf_size) {
                PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow"));
                status = PJMEDIA_CODEC_EFRMTOOSHORT;
                break;
            }

            pj_memcpy( vtool_data->dec_buf + whole_len,
                       (pj_uint8_t*)packets[i].buf,
                       packets[i].size);
            whole_len += packets[i].size;
        }

    } else {
        for (i=0; i<count; ++i) {
            if (whole_len + packets[i].size + code_size >
                vtool_data->dec_buf_size)
            {
                PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [1]"));
                status = PJMEDIA_CODEC_EFRMTOOSHORT;
                break;
            }

            status = pjmedia_h264_unpacketize( vtool_data->pktz,
                                               (pj_uint8_t*)packets[i].buf,
                                               packets[i].size,
                                               vtool_data->dec_buf,
                                               vtool_data->dec_buf_size,
                                               &whole_len);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4,(THIS_FILE, status, "Unpacketize error"));
                continue;
            }
        }
    }

    if (whole_len + code_size > vtool_data->dec_buf_size ||
        whole_len <= code_size + 1)
    {
        PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow or unpacketize error "
                             "size: %d, buffer: %d", whole_len,
                             vtool_data->dec_buf_size));
        status = PJMEDIA_CODEC_EFRMTOOSHORT;
    }
    
    if (status != PJ_SUCCESS)
        goto on_return;
    
    /* Dummy NAL sentinel */
    pj_memcpy(vtool_data->dec_buf + whole_len, start_code, code_size);

    /*
     * Step 2: parse the individual NAL and give to decoder
     */
    buf_pos = 0;
    while (1) {
        uint32_t frm_size, nalu_type, data_length;
        unsigned char *start;

        for (i = code_size - 1; buf_pos + i < whole_len; i++) {
            if (vtool_data->dec_buf[buf_pos + i] == 0 &&
                vtool_data->dec_buf[buf_pos + i + 1] == 0 &&
                vtool_data->dec_buf[buf_pos + i + 2] == 0 &&
                vtool_data->dec_buf[buf_pos + i + 3] == 1)
            {
                break;
            }
        }

        frm_size = i;
        start = vtool_data->dec_buf + buf_pos;
        nalu_type = (start[code_size] & 0x1F);
        
#if TARGET_OS_IPHONE
        /* On iOS, packets preceded by SEI frame (type 6), such as the ones
         * sent by Mac VideoToolbox encoder will cause DecodeFrame to fail
         * with -12911 (kVTVideoDecoderMalfunctionErr). The workaround
         * is to decode the whole packets at once.
         */
        if (nalu_type == 6)
            decode_whole = PJ_TRUE;
#endif

        /* AVCC format requires us to replace the start code header
         * on this NAL with its frame size.
         */
        data_length = pj_htonl(frm_size - code_size);
        pj_memcpy(start, &data_length, sizeof (data_length));

        if (nalu_type == 7) {
            /* NALU type 7 is the SPS parameter NALU */
            vtool_data->dec_sps_size = PJ_MIN(frm_size - code_size,
                                              sizeof(vtool_data->dec_sps));
            pj_memcpy(vtool_data->dec_sps, &start[code_size],
                      vtool_data->dec_sps_size);
        } else if (nalu_type == 8) {
            /* NALU type 8 is the PPS parameter NALU */
            vtool_data->dec_pps_size = PJ_MIN(frm_size - code_size,
                                              sizeof(vtool_data->dec_pps));
            pj_memcpy(vtool_data->dec_pps, &start[code_size],
                      vtool_data->dec_pps_size);
            
            ret = create_decoder(vtool_data);
        } else if (vtool_data->dec &&
                   (!decode_whole || (buf_pos + frm_size >= whole_len)))
        {
            CMBlockBufferRef block_buf = NULL;
            CMSampleBufferRef sample_buf = NULL;

            if (decode_whole) {
                /* We decode all the packets at once. */
                frm_size = whole_len;
                start = vtool_data->dec_buf;
            }

            /* Create a block buffer from the NALU */
            ret = CMBlockBufferCreateWithMemoryBlock(NULL,
                                                     start, frm_size,
                                                     kCFAllocatorNull, NULL,
                                                     0, frm_size,
                                                     0, &block_buf);
            if (ret == noErr) {
                const size_t sample_size = frm_size;
                ret = CMSampleBufferCreate(kCFAllocatorDefault,
                                           block_buf, true, NULL, NULL,
                                           vtool_data->dec_format,
                                           1, 0, NULL, 1,
                                           &sample_size, &sample_buf);
                if (ret != noErr) {
                    PJ_LOG(4,(THIS_FILE, "Failed to create sample buffer"));
                    CFRelease(block_buf);
                }
            } else {
                PJ_LOG(4,(THIS_FILE, "Failed to create block buffer"));
            }
            
            if (ret == noErr) {
                vtool_data->dec_frame = output;
                vtool_data->dec_frame->size = out_size;
                ret = VTDecompressionSessionDecodeFrame(
                          vtool_data->dec, sample_buf, 0,
                          NULL, NULL);
                if (ret == kVTInvalidSessionErr) {
#if TARGET_OS_IPHONE
                    /* Just return if app is not active, i.e. in the bg. */
                    __block UIApplicationState state;

                    dispatch_sync_on_main_queue(^{
                        state = [UIApplication sharedApplication].applicationState;
                    });
                    if (state != UIApplicationStateActive) {
                        output->type = PJMEDIA_FRAME_TYPE_NONE;
                        output->size = 0;
                        output->timestamp = packets[0].timestamp;

                        CFRelease(block_buf);
                        CFRelease(sample_buf);
                        return PJ_SUCCESS;
                    }
#endif
                    if (vtool_data->dec_format)
                        CFRelease(vtool_data->dec_format);
                    vtool_data->dec_format = NULL;
                    ret = create_decoder(vtool_data);
                    PJ_LOG(3,(THIS_FILE, "Decoder needs to be reset: %s (%d)",
                              (ret == noErr? "success": "fail"), ret));

                    if (ret == noErr) {
                        /* Retry decoding the frame after successful reset */
                        ret = VTDecompressionSessionDecodeFrame(
                                  vtool_data->dec, sample_buf, 0,
                                  NULL, NULL);
                    }
                }

                if ((ret != noErr) || (vtool_data->dec_status != noErr)) {
                    char *ret_err = (ret != noErr)?"decode err":"cb err";
                    OSStatus err_code = (ret != noErr)? ret:
                                        vtool_data->dec_status;

                    PJ_LOG(5,(THIS_FILE, "Failed to decode frame %d of size "
                                 "%d %s:%d", nalu_type, frm_size, ret_err,
                                 err_code));
                } else {
                    has_frame = PJ_TRUE;
                    output->type = PJMEDIA_FRAME_TYPE_VIDEO;
                    output->timestamp = packets[0].timestamp;

                    /* Broadcast format changed event */
                    if (vtool_data->dec_fmt_change) {
                        pjmedia_event event;

                        PJ_LOG(4,(THIS_FILE, "Frame size changed to %dx%d",
                                  vtool_data->prm->dec_fmt.det.vid.size.w,
                                  vtool_data->prm->dec_fmt.det.vid.size.h));

                        /* Broadcast format changed event */
                        pjmedia_event_init(&event, PJMEDIA_EVENT_FMT_CHANGED,
                                           &output->timestamp, codec);
                        event.data.fmt_changed.dir = PJMEDIA_DIR_DECODING;
                        pjmedia_format_copy(&event.data.fmt_changed.new_fmt,
                                            &vtool_data->prm->dec_fmt);
                        pjmedia_event_publish(NULL, codec, &event,
                                              PJMEDIA_EVENT_PUBLISH_DEFAULT);
                    }
                }

                CFRelease(block_buf);
                CFRelease(sample_buf);
            }
        }

        if (buf_pos + frm_size >= whole_len)
            break;

        buf_pos += frm_size;
    }

on_return:
    if (!has_frame) {
        pjmedia_event event;

        /* Broadcast missing keyframe event */
        pjmedia_event_init(&event, PJMEDIA_EVENT_KEYFRAME_MISSING,
                           &packets[0].timestamp, codec);
        pjmedia_event_publish(NULL, codec, &event,
                              PJMEDIA_EVENT_PUBLISH_DEFAULT);

        PJ_LOG(5,(THIS_FILE, "Decode couldn't produce picture, "
                  "input nframes=%ld, concatenated size=%d bytes",
                  count, whole_len));

        output->type = PJMEDIA_FRAME_TYPE_NONE;
        output->size = 0;
        output->timestamp = packets[0].timestamp;
    }

    return PJ_SUCCESS;
}

#endif  /* PJMEDIA_HAS_VID_TOOLBOX_CODEC */

/* $Id$ */
/* 
 * Copyright (C)2019 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-codec/vpx.h>
#include <pjmedia/vid_codec_util.h>
#include <pjmedia/errno.h>
#include <pj/log.h>
#include <pj/math.h>

#if defined(PJMEDIA_HAS_VPX_CODEC) && \
            PJMEDIA_HAS_VPX_CODEC != 0 && \
    defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#ifdef _MSC_VER
#   pragma comment( lib, "vpx.lib")
#endif

#include <pjmedia-codec/vpx_packetizer.h>

/* VPX */
#include <vpx/vpx_encoder.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>

/*
 * Constants
 */
#define THIS_FILE		"vpx.c"

#if (defined(PJ_DARWINOS) && PJ_DARWINOS != 0 && TARGET_OS_IPHONE) || \
     defined(__ANDROID__)
#  define DEFAULT_WIDTH		352
#  define DEFAULT_HEIGHT	288
#else
#  define DEFAULT_WIDTH		720
#  define DEFAULT_HEIGHT	480
#endif

#define DEFAULT_FPS		15
#define DEFAULT_AVG_BITRATE	200000
#define DEFAULT_MAX_BITRATE	200000

#define MAX_RX_RES		1200

/* VPX VP8 default PT */
#define VPX_VP8_PT		PJMEDIA_RTP_PT_VP8
/* VPX VP9 default PT */
#define VPX_VP9_PT		PJMEDIA_RTP_PT_VP9

/*
 * Factory operations.
 */
static pj_status_t vpx_test_alloc(pjmedia_vid_codec_factory *factory,
                                  const pjmedia_vid_codec_info *info );
static pj_status_t vpx_default_attr(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info,
                                    pjmedia_vid_codec_param *attr );
static pj_status_t vpx_enum_info(pjmedia_vid_codec_factory *factory,
                                 unsigned *count,
                                 pjmedia_vid_codec_info codecs[]);
static pj_status_t vpx_alloc_codec(pjmedia_vid_codec_factory *factory,
                                   const pjmedia_vid_codec_info *info,
                                   pjmedia_vid_codec **p_codec);
static pj_status_t vpx_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                     pjmedia_vid_codec *codec );


/*
 * Codec operations
 */
static pj_status_t vpx_codec_init(pjmedia_vid_codec *codec,
                                  pj_pool_t *pool );
static pj_status_t vpx_codec_open(pjmedia_vid_codec *codec,
                                  pjmedia_vid_codec_param *param );
static pj_status_t vpx_codec_close(pjmedia_vid_codec *codec);
static pj_status_t vpx_codec_modify(pjmedia_vid_codec *codec,
                                    const pjmedia_vid_codec_param *param);
static pj_status_t vpx_codec_get_param(pjmedia_vid_codec *codec,
                                       pjmedia_vid_codec_param *param);
static pj_status_t vpx_codec_encode_begin(pjmedia_vid_codec *codec,
                                          const pjmedia_vid_encode_opt *opt,
                                          const pjmedia_frame *input,
                                          unsigned out_size,
                                          pjmedia_frame *output,
                                          pj_bool_t *has_more);
static pj_status_t vpx_codec_encode_more(pjmedia_vid_codec *codec,
                                         unsigned out_size,
                                         pjmedia_frame *output,
                                         pj_bool_t *has_more);
static pj_status_t vpx_codec_decode_(pjmedia_vid_codec *codec,
                                     pj_size_t count,
                                     pjmedia_frame packets[],
                                     unsigned out_size,
                                     pjmedia_frame *output);

/* Definition for VPX codecs operations. */
static pjmedia_vid_codec_op vpx_codec_op =
{
    &vpx_codec_init,
    &vpx_codec_open,
    &vpx_codec_close,
    &vpx_codec_modify,
    &vpx_codec_get_param,
    &vpx_codec_encode_begin,
    &vpx_codec_encode_more,
    &vpx_codec_decode_,
    NULL
};

/* Definition for VPX codecs factory operations. */
static pjmedia_vid_codec_factory_op vpx_factory_op =
{
    &vpx_test_alloc,
    &vpx_default_attr,
    &vpx_enum_info,
    &vpx_alloc_codec,
    &vpx_dealloc_codec
};


static struct vpx_factory
{
    pjmedia_vid_codec_factory    base;
    pjmedia_vid_codec_mgr	*mgr;
    pj_pool_factory             *pf;
    pj_pool_t		        *pool;
} vpx_factory;


typedef struct vpx_codec_data
{
    pj_pool_t			*pool;
    pjmedia_vid_codec_param	*prm;
    pj_bool_t			 whole;
    pjmedia_vpx_packetizer	*pktz;

    /* Encoder */
    vpx_codec_iface_t 		*(*enc_if)();
    vpx_codec_ctx_t 		 enc;
    vpx_codec_iter_t 		 enc_iter;
    unsigned		 	 enc_input_size;
    pj_uint8_t			*enc_frame_whole;
    unsigned			 enc_frame_size;
    unsigned			 enc_processed;
    pj_bool_t			 enc_frame_is_keyframe;
    pj_timestamp		 ets;

    /* Decoder */
    vpx_codec_iface_t 		*(*dec_if)();
    vpx_codec_ctx_t 		 dec;
    pj_uint8_t			*dec_buf;
    unsigned			 dec_buf_size;
} vpx_codec_data;


PJ_DEF(pj_status_t) pjmedia_codec_vpx_vid_init(pjmedia_vid_codec_mgr *mgr,
                                               pj_pool_factory *pf)
{
    pj_status_t status;

    if (vpx_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    if (!mgr) mgr = pjmedia_vid_codec_mgr_instance();
    PJ_ASSERT_RETURN(mgr, PJ_EINVAL);

    /* Create VPX codec factory. */
    vpx_factory.base.op = &vpx_factory_op;
    vpx_factory.base.factory_data = NULL;
    vpx_factory.mgr = mgr;
    vpx_factory.pf = pf;
    vpx_factory.pool = pj_pool_create(pf, "vpxfactory", 256, 256, NULL);
    if (!vpx_factory.pool)
	return PJ_ENOMEM;

    /* Register codec factory to codec manager. */
    status = pjmedia_vid_codec_mgr_register_factory(mgr,
						    &vpx_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    PJ_LOG(4,(THIS_FILE, "VPX codec initialized"));

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(vpx_factory.pool);
    vpx_factory.pool = NULL;
    return status;
}

/*
 * Unregister VPX codecs factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_vpx_vid_deinit(void)
{
    pj_status_t status = PJ_SUCCESS;

    if (vpx_factory.pool == NULL) {
	/* Already deinitialized */
	return PJ_SUCCESS;
    }

    /* Unregister VPX codecs factory. */
    status = pjmedia_vid_codec_mgr_unregister_factory(vpx_factory.mgr,
						      &vpx_factory.base);

    /* Destroy pool. */
    pj_pool_release(vpx_factory.pool);
    vpx_factory.pool = NULL;

    return status;
}

static pj_status_t vpx_test_alloc(pjmedia_vid_codec_factory *factory,
                                  const pjmedia_vid_codec_info *info )
{
    PJ_ASSERT_RETURN(factory == &vpx_factory.base, PJ_EINVAL);

    if (((info->fmt_id == PJMEDIA_FORMAT_VP8) && (info->pt == VPX_VP8_PT)) ||
        ((info->fmt_id == PJMEDIA_FORMAT_VP9) && (info->pt == VPX_VP9_PT)))
    {
	return PJ_SUCCESS;
    }

    return PJMEDIA_CODEC_EUNSUP;
}

static pj_status_t vpx_default_attr(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info,
                                    pjmedia_vid_codec_param *attr )
{
    PJ_ASSERT_RETURN(factory == &vpx_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(info && attr, PJ_EINVAL);

    pj_bzero(attr, sizeof(pjmedia_vid_codec_param));

    attr->dir = PJMEDIA_DIR_ENCODING_DECODING;
    attr->packing = PJMEDIA_VID_PACKING_PACKETS;

    /* Encoded format */
    pjmedia_format_init_video(&attr->enc_fmt, info->fmt_id,
                              DEFAULT_WIDTH, DEFAULT_HEIGHT,
			      DEFAULT_FPS, 1);

    /* Decoded format */
    pjmedia_format_init_video(&attr->dec_fmt, PJMEDIA_FORMAT_I420,
                              DEFAULT_WIDTH, DEFAULT_HEIGHT,
			      DEFAULT_FPS, 1);

    /* Decoding fmtp */
    /* If the implementation is willing to receive media, both parameters
     * MUST be provided.
     */
    attr->dec_fmtp.cnt = 2;
    attr->dec_fmtp.param[0].name = pj_str((char*)"max-fr");
    attr->dec_fmtp.param[0].val = pj_str((char*)"30");
    attr->dec_fmtp.param[1].name = pj_str((char*)" max-fs");
    attr->dec_fmtp.param[1].val = pj_str((char*)"580");

    /* Bitrate */
    attr->enc_fmt.det.vid.avg_bps = DEFAULT_AVG_BITRATE;
    attr->enc_fmt.det.vid.max_bps = DEFAULT_MAX_BITRATE;

    /* Encoding MTU */
    attr->enc_mtu = PJMEDIA_MAX_VID_PAYLOAD_SIZE;

    return PJ_SUCCESS;
}

static pj_status_t vpx_enum_info(pjmedia_vid_codec_factory *factory,
                                 unsigned *count,
                                 pjmedia_vid_codec_info info[])
{
    unsigned i = 0;

    PJ_ASSERT_RETURN(info && *count > 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &vpx_factory.base, PJ_EINVAL);

#if PJMEDIA_HAS_VPX_CODEC_VP8
    info[i].fmt_id = PJMEDIA_FORMAT_VP8;
    info[i].pt = VPX_VP8_PT;
    info[i].encoding_name = pj_str((char*)"VP8");
    info[i].encoding_desc = pj_str((char*)"VPX VP8 codec");
    i++;
#endif

#if PJMEDIA_HAS_VPX_CODEC_VP9
    if (i + 1 < *count) {
    	info[i].fmt_id = PJMEDIA_FORMAT_VP9;
        info[i].pt = VPX_VP9_PT;
    	info[i].encoding_name = pj_str((char*)"VP9");
    	info[i].encoding_desc = pj_str((char*)"VPX VP9 codec");
    	i++;
    }
#endif

    *count = i;
    for (i = 0; i < *count; i++) {
    	info[i].clock_rate = 90000;
    	info[i].dir = PJMEDIA_DIR_ENCODING_DECODING;
    	info[i].dec_fmt_id_cnt = 1;
    	info[i].dec_fmt_id[0] = PJMEDIA_FORMAT_I420;
    	info[i].packings = PJMEDIA_VID_PACKING_PACKETS;
    	info[i].fps_cnt = 3;
    	info[i].fps[0].num = 15;
    	info[i].fps[0].denum = 1;
    	info[i].fps[1].num = 25;
    	info[i].fps[1].denum = 1;
    	info[i].fps[2].num = 30;
    	info[i].fps[2].denum = 1;
    }

    return PJ_SUCCESS;

}

static pj_status_t vpx_alloc_codec(pjmedia_vid_codec_factory *factory,
                                   const pjmedia_vid_codec_info *info,
                                   pjmedia_vid_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_vid_codec *codec;
    vpx_codec_data *vpx_data;

    PJ_ASSERT_RETURN(factory == &vpx_factory.base && info && p_codec,
                     PJ_EINVAL);

    *p_codec = NULL;

    pool = pj_pool_create(vpx_factory.pf, "vpx%p", 512, 512, NULL);
    if (!pool)
	return PJ_ENOMEM;

    /* codec instance */
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_codec);
    codec->factory = factory;
    codec->op = &vpx_codec_op;

    /* codec data */
    vpx_data = PJ_POOL_ZALLOC_T(pool, vpx_codec_data);
    vpx_data->pool = pool;
    codec->codec_data = vpx_data;

    /* encoder and decoder interfaces */
    if (info->fmt_id == PJMEDIA_FORMAT_VP8) {
    	vpx_data->enc_if = &vpx_codec_vp8_cx;
    	vpx_data->dec_if = &vpx_codec_vp8_dx;
    } else if (info->fmt_id == PJMEDIA_FORMAT_VP9) {
    	vpx_data->enc_if = &vpx_codec_vp9_cx;
    	vpx_data->dec_if = &vpx_codec_vp9_dx;
    }

    *p_codec = codec;
    return PJ_SUCCESS;
}

static pj_status_t vpx_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                     pjmedia_vid_codec *codec )
{
    vpx_codec_data *vpx_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    PJ_UNUSED_ARG(factory);

    vpx_data = (vpx_codec_data*) codec->codec_data;
    vpx_data->enc_if = NULL;
    vpx_data->dec_if = NULL;

    pj_pool_release(vpx_data->pool);
    return PJ_SUCCESS;
}

static pj_status_t vpx_codec_init(pjmedia_vid_codec *codec,
                                  pj_pool_t *pool )
{
    PJ_ASSERT_RETURN(codec && pool, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

static pj_status_t vpx_codec_open(pjmedia_vid_codec *codec,
                                  pjmedia_vid_codec_param *codec_param )
{
    vpx_codec_data 		*vpx_data;
    pjmedia_vid_codec_param	*param;
    pjmedia_vid_codec_vpx_fmtp   vpx_fmtp;
    vpx_codec_enc_cfg_t cfg;
    vpx_codec_err_t res;
    pjmedia_vpx_packetizer_cfg  pktz_cfg;
    unsigned max_res = MAX_RX_RES;
    pj_status_t	status;

    PJ_ASSERT_RETURN(codec && codec_param, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Opening codec.."));

    vpx_data = (vpx_codec_data*) codec->codec_data;
    vpx_data->prm = pjmedia_vid_codec_param_clone(vpx_data->pool,
                                                  codec_param);
    param = vpx_data->prm;
    vpx_data->whole = (param->packing == PJMEDIA_VID_PACKING_WHOLE);

    /* Apply SDP fmtp to format in codec param */
    if (!param->ignore_fmtp) {
	status = pjmedia_vid_codec_vpx_apply_fmtp(param);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /*
     * Encoder
     */

    /* Init encoder parameters */
    res = vpx_codec_enc_config_default(vpx_data->enc_if(), &cfg, 0);
    if (res) {
    	PJ_LOG(3, (THIS_FILE, "Failed to get encoder default config"));
	return PJMEDIA_CODEC_EFAILED;
    }
    
    cfg.g_w = vpx_data->prm->enc_fmt.det.vid.size.w;
    cfg.g_h = vpx_data->prm->enc_fmt.det.vid.size.h;
    /* timebase is the inverse of fps */
    cfg.g_timebase.num = vpx_data->prm->enc_fmt.det.vid.fps.denum;
    cfg.g_timebase.den = vpx_data->prm->enc_fmt.det.vid.fps.num;
    /* bitrate in KBps */
    cfg.rc_target_bitrate = vpx_data->prm->enc_fmt.det.vid.avg_bps / 1000;
    
    cfg.g_pass = VPX_RC_ONE_PASS;
    cfg.rc_end_usage = VPX_CBR;
    cfg.g_threads = 4;
    cfg.g_lag_in_frames = 0;
    cfg.g_error_resilient = 0;
    cfg.rc_undershoot_pct = 95;
    cfg.rc_min_quantizer = 4;
    cfg.rc_max_quantizer = 56;
    cfg.rc_buf_initial_sz = 400;
    cfg.rc_buf_optimal_sz = 500;
    cfg.rc_buf_sz = 600;
    /* kf max distance is 60s. */
    cfg.kf_max_dist = 60 * vpx_data->prm->enc_fmt.det.vid.fps.num/
    		      vpx_data->prm->enc_fmt.det.vid.fps.denum;
    cfg.rc_resize_allowed = 0;
    cfg.rc_dropframe_thresh = 25;

    vpx_data->enc_input_size = cfg.g_w * cfg.g_h * 3 >> 1;

    /* Initialize encoder */
    res = vpx_codec_enc_init(&vpx_data->enc, vpx_data->enc_if(), &cfg, 0);
    if (res) {
    	PJ_LOG(3, (THIS_FILE, "Failed to initialize encoder"));
	return PJMEDIA_CODEC_EFAILED;
    }

    /* Values greater than 0 will increase encoder speed at the expense of
     * quality.
     * Valid range for VP8: -16..16
     * Valid range for VP9: -9..9
     */
    vpx_codec_control(&vpx_data->enc, VP8E_SET_CPUUSED, 9);

    /*
     * Decoder
     */
    res = vpx_codec_dec_init(&vpx_data->dec, vpx_data->dec_if(), NULL, 0);
    if (res) {
    	PJ_LOG(3, (THIS_FILE, "Failed to initialize decoder"));
	return PJMEDIA_CODEC_EFAILED;
    }

    /* Parse local fmtp */
    status = pjmedia_vid_codec_vpx_parse_fmtp(&param->dec_fmtp, &vpx_fmtp);
    if (status != PJ_SUCCESS)
	return status;

    if (vpx_fmtp.max_fs > 0) {
    	max_res = ((int)pj_isqrt(vpx_fmtp.max_fs * 8)) * 16;
    }
    vpx_data->dec_buf_size = (max_res * max_res * 3 >> 1) + (max_res);
    vpx_data->dec_buf = (pj_uint8_t*)pj_pool_alloc(vpx_data->pool,
                                                   vpx_data->dec_buf_size);

    /* Need to update param back after values are negotiated */
    pj_memcpy(codec_param, param, sizeof(*codec_param));

    pj_bzero(&pktz_cfg, sizeof(pktz_cfg));
    pktz_cfg.mtu = param->enc_mtu;
    pktz_cfg.fmt_id = param->enc_fmt.id;

    status = pjmedia_vpx_packetizer_create(vpx_data->pool, &pktz_cfg,
                                           &vpx_data->pktz);
    if (status != PJ_SUCCESS)
        return status;

    return PJ_SUCCESS;
}

static pj_status_t vpx_codec_close(pjmedia_vid_codec *codec)
{
    struct vpx_codec_data *vpx_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    vpx_data = (vpx_codec_data*) codec->codec_data;
    vpx_codec_destroy(&vpx_data->enc);
    vpx_codec_destroy(&vpx_data->dec);

    return PJ_SUCCESS;
}

static pj_status_t vpx_codec_modify(pjmedia_vid_codec *codec,
                                    const pjmedia_vid_codec_param *param)
{
    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(param);
    return PJ_EINVALIDOP;
}

static pj_status_t vpx_codec_get_param(pjmedia_vid_codec *codec,
                                       pjmedia_vid_codec_param *param)
{
    struct vpx_codec_data *vpx_data;

    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);

    vpx_data = (vpx_codec_data*) codec->codec_data;
    pj_memcpy(param, vpx_data->prm, sizeof(*param));

    return PJ_SUCCESS;
}

static pj_status_t vpx_codec_encode_begin(pjmedia_vid_codec *codec,
                                          const pjmedia_vid_encode_opt *opt,
                                          const pjmedia_frame *input,
                                          unsigned out_size,
                                          pjmedia_frame *output,
                                          pj_bool_t *has_more)
{
    struct vpx_codec_data *vpx_data;
    vpx_image_t img;
    vpx_enc_frame_flags_t flags = 0;
    vpx_codec_err_t res;

    PJ_ASSERT_RETURN(codec && input && out_size && output && has_more,
                     PJ_EINVAL);

    vpx_data = (vpx_codec_data*) codec->codec_data;

    PJ_ASSERT_RETURN(input->size == vpx_data->enc_input_size,
                     PJMEDIA_CODEC_EFRMINLEN);

    vpx_img_wrap(&img, VPX_IMG_FMT_I420,
            	 vpx_data->prm->enc_fmt.det.vid.size.w,
            	 vpx_data->prm->enc_fmt.det.vid.size.h,
            	 1, (unsigned char *)input->buf);

    if (opt && opt->force_keyframe) {
	flags |= VPX_EFLAG_FORCE_KF;
    }

    vpx_data->ets = input->timestamp;
    vpx_data->enc_frame_size = vpx_data->enc_processed = 0;
    vpx_data->enc_iter = NULL;

    res = vpx_codec_encode(&vpx_data->enc, &img, vpx_data->ets.u64, 1,
    			   flags, VPX_DL_REALTIME);
    if (res) {
	PJ_LOG(4, (THIS_FILE, "Failed to encode frame %s",
		   vpx_codec_error(&vpx_data->enc)));
	return PJMEDIA_CODEC_EFAILED;
    }

    do {
    	const vpx_codec_cx_pkt_t *pkt;

    	pkt = vpx_codec_get_cx_data(&vpx_data->enc, &vpx_data->enc_iter);
    	if (!pkt) break;
    	if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
    	    /* We have a valid frame packet */
    	    vpx_data->enc_frame_whole = pkt->data.frame.buf;
            vpx_data->enc_frame_size = pkt->data.frame.sz;
            vpx_data->enc_processed = 0;
            if (pkt->data.frame.flags & VPX_FRAME_IS_KEY)
            	vpx_data->enc_frame_is_keyframe = PJ_TRUE;
            else
            	vpx_data->enc_frame_is_keyframe = PJ_FALSE;
            	
            break;
    	}
    } while (1);

    if (vpx_data->enc_frame_size == 0) {
    	*has_more = PJ_FALSE;
    	output->size = 0;
    	output->type = PJMEDIA_FRAME_TYPE_NONE;

	if (vpx_data->enc.err) {
	    PJ_LOG(4, (THIS_FILE, "Failed to get encoded frame %s",
		   		  vpx_codec_error(&vpx_data->enc)));
	} else {
	    return PJ_SUCCESS;
	}
    }
    
    if (vpx_data->whole) {
    	*has_more = PJ_FALSE;
	if (vpx_data->enc_frame_size > out_size)
	    return PJMEDIA_CODEC_EFRMTOOSHORT;

        output->timestamp = vpx_data->ets;
        output->type = PJMEDIA_FRAME_TYPE_VIDEO;
        output->size = vpx_data->enc_frame_size;
        output->bit_info = 0;
        if (vpx_data->enc_frame_is_keyframe) {
            output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;
        }

        pj_memcpy(output->buf, vpx_data->enc_frame_whole, output->size);
        
        return PJ_SUCCESS;
    }

    return vpx_codec_encode_more(codec, out_size, output, has_more);
}


static pj_status_t vpx_codec_encode_more(pjmedia_vid_codec *codec,
                                         unsigned out_size,
                                         pjmedia_frame *output,
                                         pj_bool_t *has_more)
{
    struct vpx_codec_data *vpx_data;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(codec && out_size && output && has_more,
                     PJ_EINVAL);

    vpx_data = (vpx_codec_data*) codec->codec_data;
    
    if (vpx_data->enc_processed < vpx_data->enc_frame_size) {
    	unsigned payload_desc_size = 1;
    	pj_size_t payload_len = out_size;
    	pj_uint8_t *p = (pj_uint8_t *)output->buf;

    	status = pjmedia_vpx_packetize(vpx_data->pktz,
				       vpx_data->enc_frame_size,
				       &vpx_data->enc_processed,
				       vpx_data->enc_frame_is_keyframe,
				       &p,
				       &payload_len);

    	if (status != PJ_SUCCESS) {
    	    return status;
    	}
        pj_memcpy(p + payload_desc_size,
        	  (vpx_data->enc_frame_whole + vpx_data->enc_processed),
        	  payload_len);
        output->size = payload_len + payload_desc_size;
        output->timestamp = vpx_data->ets;
        output->type = PJMEDIA_FRAME_TYPE_VIDEO;
        output->bit_info = 0;
        if (vpx_data->enc_frame_is_keyframe) {
            output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;
        }
        vpx_data->enc_processed += payload_len;
        *has_more = (vpx_data->enc_processed < vpx_data->enc_frame_size);
    }

    return status;
}

static pj_status_t vpx_codec_decode_(pjmedia_vid_codec *codec,
                                     pj_size_t count,
                                     pjmedia_frame packets[],
                                     unsigned out_size,
                                     pjmedia_frame *output)
{
    struct vpx_codec_data *vpx_data;
    pj_bool_t has_frame = PJ_FALSE;
    unsigned i, whole_len = 0;
    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = NULL;
    vpx_codec_err_t res;
    unsigned pos = 0;
    int plane;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    vpx_data = (vpx_codec_data*) codec->codec_data;

    /*
     * Step 1: unpacketize the packets/frames
     */
    whole_len = 0;
    if (vpx_data->whole) {
	for (i = 0; i < count; ++i) {
	    if (whole_len + packets[i].size > vpx_data->dec_buf_size) {
		PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [1]"));
		return PJMEDIA_CODEC_EFRMTOOSHORT;
	    }

	    pj_memcpy( vpx_data->dec_buf + whole_len,
	               (pj_uint8_t*)packets[i].buf,
	               packets[i].size);
	    whole_len += packets[i].size;
	}

    } else {
    	for (i = 0; i < count; ++i) {
    	    unsigned desc_len;
    	    unsigned packet_size = packets[i].size;
    	    pj_status_t status;

            status = pjmedia_vpx_unpacketize(vpx_data->pktz, packets[i].buf,
                                             packet_size, &desc_len);
    	    if (status != PJ_SUCCESS) {
	    	PJ_LOG(4,(THIS_FILE, "Unpacketize error"));
	    	return status;
    	    }

	    packet_size -= desc_len;
    	    if (whole_len + packet_size > vpx_data->dec_buf_size) {
	    	PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [2]"));
	    	return PJMEDIA_CODEC_EFRMTOOSHORT;
            }

	    pj_memcpy(vpx_data->dec_buf + whole_len,
		      (char *)packets[i].buf + desc_len, packet_size);
	    whole_len += packet_size;
    	}
    }

    /* Decode */
    res = vpx_codec_decode(&vpx_data->dec, vpx_data->dec_buf, whole_len,
            		   0, VPX_DL_REALTIME);
    if (res) {
	PJ_LOG(4, (THIS_FILE, "Failed to decode frame %s",
		   vpx_codec_error(&vpx_data->dec)));
	goto on_return;
    }

    img = vpx_codec_get_frame(&vpx_data->dec, &iter);
    if (!img) {
	PJ_LOG(4, (THIS_FILE, "Failed to get decoded frame %s",
		   vpx_codec_error(&vpx_data->dec)));
	goto on_return;
    }

    has_frame = PJ_TRUE;

    /* Detect format change */
    if (img->d_w != vpx_data->prm->dec_fmt.det.vid.size.w ||
	img->d_h != vpx_data->prm->dec_fmt.det.vid.size.h)
    {
	pjmedia_event event;

	PJ_LOG(4,(THIS_FILE, "Frame size changed: %dx%d --> %dx%d",
		  vpx_data->prm->dec_fmt.det.vid.size.w,
		  vpx_data->prm->dec_fmt.det.vid.size.h,
		  img->d_w, img->d_h));

	vpx_data->prm->dec_fmt.det.vid.size.w = img->d_w;
	vpx_data->prm->dec_fmt.det.vid.size.h = img->d_h;

	/* Broadcast format changed event */
	pjmedia_event_init(&event, PJMEDIA_EVENT_FMT_CHANGED,
	                   &packets[0].timestamp, codec);
	event.data.fmt_changed.dir = PJMEDIA_DIR_DECODING;
	pjmedia_format_copy(&event.data.fmt_changed.new_fmt,
	                    &vpx_data->prm->dec_fmt);
	pjmedia_event_publish(NULL, codec, &event,
	                      PJMEDIA_EVENT_PUBLISH_DEFAULT);
    }

    if (img->d_w * img->d_h * 3/2 > output->size)
    	return PJMEDIA_CODEC_EFRMTOOSHORT;

    output->type = PJMEDIA_FRAME_TYPE_VIDEO;
    output->timestamp = packets[0].timestamp;

    for (plane = 0; plane < 3; ++plane) {
        const unsigned char *buf = img->planes[plane];
    	const int stride = img->stride[plane];
    	const int w = (plane? img->d_w / 2: img->d_w);
    	const int h = (plane? img->d_h / 2: img->d_h);
    	int y;

    	for (y = 0; y < h; ++y) {
    	    pj_memcpy((char *)output->buf + pos, buf, w);
    	    pos += w;
      	    buf += stride;
    	}
    }

    output->size = pos;
	
on_return:
    if (!has_frame) {
	pjmedia_event event;

	/* Broadcast missing keyframe event */
	pjmedia_event_init(&event, PJMEDIA_EVENT_KEYFRAME_MISSING,
	                   &packets[0].timestamp, codec);
	pjmedia_event_publish(NULL, codec, &event,
	                      PJMEDIA_EVENT_PUBLISH_DEFAULT);

	PJ_LOG(4,(THIS_FILE, "Decode couldn't produce picture, "
		  "input nframes=%d, concatenated size=%d bytes",
		  count, whole_len));

	output->type = PJMEDIA_FRAME_TYPE_NONE;
	output->size = 0;
	output->timestamp = packets[0].timestamp;
    }

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_HAS_VPX_CODEC */

/* $Id$ */
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
#define THIS_FILE		"and_vid_mediacodec.cpp"
#define ANMED_KEY_COLOR_FMT     "color-format"
#define ANMED_KEY_WIDTH         "width"
#define ANMED_KEY_HEIGHT        "height"
#define ANMED_KEY_BIT_RATE      "bitrate"
#define ANMED_KEY_PROFILE       "profile"
#define ANMED_KEY_FRAME_RATE    "frame-rate"
#define ANMED_KEY_IFR_INTTERVAL "i-frame-interval"
#define ANMED_KEY_MIME          "mime"
#define ANMED_KEY_REQUEST_SYNCF	"request-sync"
#define ANMED_KEY_CSD0	        "csd-0"
#define ANMED_KEY_CSD1	        "csd-1"
#define ANMED_KEY_MAX_INPUT_SZ  "max-input-size"
#define ANMED_KEY_ENCODER	"encoder"
#define ANMED_KEY_PRIORITY	"priority"
#define ANMED_KEY_STRIDE	"stride"
#define ANMED_I420_PLANAR_FMT   0x13
#define ANMED_QUEUE_TIMEOUT     2000*100

#define DEFAULT_WIDTH		352
#define DEFAULT_HEIGHT		288

#define DEFAULT_FPS		15
#define DEFAULT_AVG_BITRATE	256000
#define DEFAULT_MAX_BITRATE	256000

#define SPS_PPS_BUF_SIZE	64

#define MAX_RX_WIDTH		1200
#define MAX_RX_HEIGHT		800

/* Maximum duration from one key frame to the next (in seconds). */
#define KEYFRAME_INTERVAL	1

#define CODEC_WAIT_RETRY 	10
#define CODEC_THREAD_WAIT 	10
/* Timeout until the buffer is ready in ms. */
#define CODEC_DEQUEUE_TIMEOUT 	20

/*
 * Factory operations.
 */
static pj_status_t anmed_test_alloc(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info );
static pj_status_t anmed_default_attr(pjmedia_vid_codec_factory *factory,
                                      const pjmedia_vid_codec_info *info,
                                      pjmedia_vid_codec_param *attr );
static pj_status_t anmed_enum_info(pjmedia_vid_codec_factory *factory,
                                   unsigned *count,
                                   pjmedia_vid_codec_info codecs[]);
static pj_status_t anmed_alloc_codec(pjmedia_vid_codec_factory *factory,
                                     const pjmedia_vid_codec_info *info,
                                     pjmedia_vid_codec **p_codec);
static pj_status_t anmed_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                       pjmedia_vid_codec *codec );


/*
 * Codec operations
 */
static pj_status_t anmed_codec_init(pjmedia_vid_codec *codec,
                                    pj_pool_t *pool );
static pj_status_t anmed_codec_open(pjmedia_vid_codec *codec,
                                    pjmedia_vid_codec_param *param );
static pj_status_t anmed_codec_close(pjmedia_vid_codec *codec);
static pj_status_t anmed_codec_modify(pjmedia_vid_codec *codec,
                                      const pjmedia_vid_codec_param *param);
static pj_status_t anmed_codec_get_param(pjmedia_vid_codec *codec,
                                         pjmedia_vid_codec_param *param);
static pj_status_t anmed_codec_encode_begin(pjmedia_vid_codec *codec,
                                            const pjmedia_vid_encode_opt *opt,
                                            const pjmedia_frame *input,
                                            unsigned out_size,
                                            pjmedia_frame *output,
                                            pj_bool_t *has_more);
static pj_status_t anmed_codec_encode_more(pjmedia_vid_codec *codec,
                                           unsigned out_size,
                                           pjmedia_frame *output,
                                           pj_bool_t *has_more);
static pj_status_t anmed_codec_decode(pjmedia_vid_codec *codec,
                                      pj_size_t count,
                                      pjmedia_frame packets[],
                                      unsigned out_size,
                                      pjmedia_frame *output);

/* Definition for Android AMediaCodec operations. */
static pjmedia_vid_codec_op anmed_codec_op =
{
    &anmed_codec_init,
    &anmed_codec_open,
    &anmed_codec_close,
    &anmed_codec_modify,
    &anmed_codec_get_param,
    &anmed_codec_encode_begin,
    &anmed_codec_encode_more,
    &anmed_codec_decode,
    NULL
};

/* Definition for Android AMediaCodec factory operations. */
static pjmedia_vid_codec_factory_op anmed_factory_op =
{
    &anmed_test_alloc,
    &anmed_default_attr,
    &anmed_enum_info,
    &anmed_alloc_codec,
    &anmed_dealloc_codec
};

static struct anmed_factory
{
    pjmedia_vid_codec_factory    base;
    pjmedia_vid_codec_mgr	*mgr;
    pj_pool_factory             *pf;
    pj_pool_t		        *pool;
} anmed_factory;

enum anmed_frm_type {
    ANMED_FRM_TYPE_DEFAULT = 0,
    ANMED_FRM_TYPE_KEYFRAME = 1,
    ANMED_FRM_TYPE_CONFIG = 2
};

typedef struct avc_codec_data {
    pjmedia_h264_packetizer	*pktz;

    pj_uint8_t			 enc_sps_pps_buf[SPS_PPS_BUF_SIZE];
    unsigned			 enc_sps_pps_len;
    pj_bool_t			 enc_sps_pps_ex;

    pj_uint8_t			*dec_sps_buf;
    unsigned			 dec_sps_len;
    pj_uint8_t			*dec_pps_buf;
    unsigned			 dec_pps_len;
} avc_codec_data;

typedef struct anmed_codec_data
{
    pj_pool_t			*pool;
    pj_uint8_t                   codec_idx;
    pjmedia_vid_codec_param	*prm;
    pj_bool_t			 whole;
    void                        *ex_data;

    /* Encoder state */
    AMediaCodec                 *enc;
    unsigned		 	 enc_input_size;
    pj_uint8_t			*enc_frame_whole;
    unsigned			 enc_frame_size;
    unsigned			 enc_processed;
    AMediaCodecBufferInfo        enc_buf_info;
    int				 enc_output_buf_idx;

    /* Decoder state */
    AMediaCodec                 *dec;
    pj_uint8_t			*dec_buf;
    pj_uint8_t			*dec_input_buf;
    unsigned			 dec_input_buf_len;
    pj_size_t			 dec_input_buf_max_size;
    pj_ssize_t			 dec_input_buf_idx;
    unsigned			 dec_has_output_frame;
    unsigned			 dec_stride_len;
    unsigned			 dec_buf_size;
    AMediaCodecBufferInfo        dec_buf_info;
} anmed_codec_data;

/* Custom callbacks. */

/* This callback is useful when specific method is needed when opening
 * the codec (e.g: applying fmtp or setting up the packetizer)
 */
typedef pj_status_t (*open_cb)(anmed_codec_data *anmed_data);

/* This callback is useful for handling configure frame produced by encoder.
 * Output frame might want to be stored the configuration frame and append it
 * to a keyframe for sending later (e.g: on avc codec). The default behavior
 * is to send the configuration frame regardless.
 */
typedef pj_status_t (*process_encode_cb)(anmed_codec_data *anmed_data);

/* This callback is to process more encoded packets/payloads from the codec.*/
typedef pj_status_t(*encode_more_cb)(anmed_codec_data *anmed_data,
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
static pj_status_t open_avc(anmed_codec_data *anmed_data);
static pj_status_t open_vpx(anmed_codec_data *anmed_data);
static pj_status_t process_encode_avc(anmed_codec_data *anmed_data);
static pj_status_t encode_more_avc(anmed_codec_data *anmed_data,
                                   unsigned out_size,
                                   pjmedia_frame *output,
                                   pj_bool_t *has_more);
static pj_status_t encode_more_vpx(anmed_codec_data *anmed_data,
                                   unsigned out_size,
                                   pjmedia_frame *output,
                                   pj_bool_t *has_more);
static pj_status_t decode_avc(pjmedia_vid_codec *codec,
                              pj_size_t count,
                              pjmedia_frame packets[],
                              unsigned out_size,
                              pjmedia_frame *output);
static pj_status_t decode_vpx(pjmedia_vid_codec *codec,
                              pj_size_t count,
                              pjmedia_frame packets[],
                              unsigned out_size,
                              pjmedia_frame *output);

static struct anmed_codec {
    int		       enabled;		  /* Is this codec enabled?	     */
    const char	      *name;		  /* Codec name.		     */
    const char        *description;       /* Codec description.              */
    const char        *mime_type;         /* Mime type.                      */
    const char        *encoder_name;      /* Encoder name.                   */
    const char        *decoder_name;      /* Decoder name.                   */
    pj_uint8_t	       pt;		  /* Payload type.		     */
    pjmedia_format_id  fmt_id;		  /* Format id.   		     */
    pj_uint8_t         keyframe_interval; /* Keyframe interval.              */

    open_cb            open_codec;
    process_encode_cb  process_encode;
    encode_more_cb     encode_more;
    decode_cb          decode;

    pjmedia_codec_fmtp dec_fmtp;	  /* Decoder's fmtp params.	     */
}

anmed_codec[] {
#if PJMEDIA_HAS_ANMED_AVC
    {1, "H264",	"Android MediaCodec AVC/H264 codec", "video/avc",
        "OMX.google.h264.encoder", "OMX.qcom.video.decoder.avc",
        PJMEDIA_RTP_PT_H264, PJMEDIA_FORMAT_H264, KEYFRAME_INTERVAL,
        &open_avc, &process_encode_avc, &encode_more_avc, &decode_avc,
        {2, {{{(char *)"profile-level-id", 16}, {(char *)"42e01e", 6}},
             {{(char *)" packetization-mode", 19}, {(char *)"1", 1}}}
        }
    },
#endif
#if PJMEDIA_HAS_ANMED_VP8
    {1, "VP8",	"Android MediaCodec VP8 codec", "video/x-vnd.on2.vp8",
        "OMX.google.vp8.encoder", "OMX.qcom.video.decoder.vp8",
        PJMEDIA_RTP_PT_VP8, PJMEDIA_FORMAT_VP8, KEYFRAME_INTERVAL,
        &open_vpx, NULL, &encode_more_vpx, &decode_vpx,
        {2, {{{(char *)"max-fr", 6}, {(char *)"30", 2}},
             {{(char *)" max-fs", 7}, {(char *)"580", 3}}}
        }
    },
#endif
#if PJMEDIA_HAS_ANMED_VP9
    {1, "VP9",	"Android MediaCodec VP9 codec", "video/x-vnd.on2.vp9",
        "OMX.google.vp9.encoder", "OMX.qcom.video.decoder.vp9",
        PJMEDIA_RTP_PT_VP9, PJMEDIA_FORMAT_VP9, KEYFRAME_INTERVAL,
        &open_vpx, NULL, &encode_more_vpx, &decode_vpx,
        {2, {{{(char *)"max-fr", 6}, {(char *)"30", 2}},
             {{(char *)" max-fs", 7}, {(char *)"580", 3}}}
        }
    }
#endif
};

static pj_status_t open_avc(anmed_codec_data *anmed_data)
{
    pj_status_t status;
    pjmedia_vid_codec_param *param = anmed_data->prm;
    pjmedia_h264_packetizer_cfg  pktz_cfg;
    pjmedia_vid_codec_h264_fmtp  h264_fmtp;
    avc_codec_data *avc_data;

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
    avc_data = PJ_POOL_ZALLOC_T(anmed_data->pool, avc_codec_data);
    if (!avc_data)
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

    pktz_cfg.mode = PJMEDIA_H264_PACKETIZER_MODE_NON_INTERLEAVED;
    status = pjmedia_h264_packetizer_create(anmed_data->pool, &pktz_cfg,
                                            &avc_data->pktz);
    if (status != PJ_SUCCESS)
        return status;

    anmed_data->ex_data = avc_data;

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
            pj_bool_t found = PJ_TRUE;
            for (j = 0; j < code_size; j++) {
                if (h264_fmtp.sprop_param_sets[i + j] != start_code[j]) {
                    found = PJ_FALSE;
                    break;
                }
            }
        }

        if (i >= code_size) {
            avc_data->dec_sps_len = i + med_code_size - code_size;
            avc_data->dec_pps_len = h264_fmtp.sprop_param_sets_len +
                med_code_size - code_size - i;

            avc_data->dec_sps_buf = (pj_uint8_t *)pj_pool_alloc(
                anmed_data->pool,
                avc_data->dec_sps_len);
            avc_data->dec_pps_buf = (pj_uint8_t *)pj_pool_alloc(
                anmed_data->pool,
                avc_data->dec_pps_len);

            pj_memcpy(avc_data->dec_sps_buf, med_start_code,
                      med_code_size);
            pj_memcpy(avc_data->dec_sps_buf + med_code_size,
                      &h264_fmtp.sprop_param_sets[code_size],
                      avc_data->dec_sps_len - med_code_size);
            pj_memcpy(avc_data->dec_pps_buf, med_start_code,
                      med_code_size);
            pj_memcpy(avc_data->dec_pps_buf + med_code_size,
                      &h264_fmtp.sprop_param_sets[i + code_size],
                      avc_data->dec_pps_len - med_code_size);
        }
    }
    return status;
}

static pj_status_t process_encode_avc(anmed_codec_data *anmed_data)
{
    pj_status_t status = PJ_SUCCESS;
    avc_codec_data *avc_data;

    avc_data = (avc_codec_data *)anmed_data->ex_data;
    if (anmed_data->enc_buf_info.flags & ANMED_FRM_TYPE_CONFIG) {

        /*
            * Config data or SPS+PPS. Update the SPS and PPS buffer,
            * this will be sent later when sending Keyframe.
            */
        avc_data->enc_sps_pps_len = PJ_MIN(anmed_data->enc_buf_info.size,
                                        sizeof(avc_data->enc_sps_pps_buf));
        pj_memcpy(avc_data->enc_sps_pps_buf, anmed_data->enc_frame_whole,
                    avc_data->enc_sps_pps_len);

        AMediaCodec_releaseOutputBuffer(anmed_data->enc,
                                        anmed_data->enc_output_buf_idx,
                                        0);

        return PJ_EIGNORED;
    }
    if (anmed_data->enc_buf_info.flags & ANMED_FRM_TYPE_KEYFRAME) {
        avc_data->enc_sps_pps_ex = PJ_TRUE;
        anmed_data->enc_frame_size = avc_data->enc_sps_pps_len;
    } else {
        avc_data->enc_sps_pps_ex = PJ_FALSE;
    }

    return status;
}

static pj_status_t encode_more_avc(anmed_codec_data *anmed_data,
				   unsigned out_size,
				   pjmedia_frame *output,
				   pj_bool_t *has_more)
{
    const pj_uint8_t *payload;
    pj_size_t payload_len;
    pj_status_t status;
    pj_uint8_t *data_buf = NULL;
    avc_codec_data *avc_data;

    avc_data = (avc_codec_data *)anmed_data->ex_data;
    if (avc_data->enc_sps_pps_ex) {
	data_buf = avc_data->enc_sps_pps_buf;
    } else {
	data_buf = anmed_data->enc_frame_whole;
    }
    /* We have outstanding frame in packetizer */
    status = pjmedia_h264_packetize(avc_data->pktz,
				    data_buf,
				    anmed_data->enc_frame_size,
				    &anmed_data->enc_processed,
				    &payload, &payload_len);
    if (status != PJ_SUCCESS) {
	/* Reset */
	anmed_data->enc_frame_size = anmed_data->enc_processed = 0;
	*has_more = (anmed_data->enc_processed < anmed_data->enc_frame_size);

	if (!(*has_more)) {
	    AMediaCodec_releaseOutputBuffer(anmed_data->enc,
					    anmed_data->enc_output_buf_idx,
					    0);
	}
	PJ_PERROR(3,(THIS_FILE, status, "pjmedia_h264_packetize() error"));
	return status;
    }

    PJ_ASSERT_RETURN(payload_len <= out_size, PJMEDIA_CODEC_EFRMTOOSHORT);

    output->type = PJMEDIA_FRAME_TYPE_VIDEO;
    pj_memcpy(output->buf, payload, payload_len);
    output->size = payload_len;

    if (anmed_data->enc_processed >= anmed_data->enc_frame_size) {
	avc_codec_data *avc_data = (avc_codec_data *)anmed_data->ex_data;

	if (avc_data->enc_sps_pps_ex) {
	    *has_more = PJ_TRUE;
	    avc_data->enc_sps_pps_ex = PJ_FALSE;
	    anmed_data->enc_processed = 0;
	    anmed_data->enc_frame_size = anmed_data->enc_buf_info.size;
	} else {
	    *has_more = PJ_FALSE;
	    AMediaCodec_releaseOutputBuffer(anmed_data->enc,
					    anmed_data->enc_output_buf_idx,
					    0);
	}
    } else {
	*has_more = PJ_TRUE;
    }

    return PJ_SUCCESS;
}

static pj_status_t open_vpx(anmed_codec_data *anmed_data)
{
    pj_status_t status = PJ_SUCCESS;
    if (!anmed_data->prm->ignore_fmtp) {
        status = pjmedia_vid_codec_vpx_apply_fmtp(anmed_data->prm);
    }
    return status;
}

static pj_status_t encode_more_vpx(anmed_codec_data *anmed_data,
				   unsigned out_size,
				   pjmedia_frame *output,
				   pj_bool_t *has_more)
{
    PJ_ASSERT_RETURN(anmed_data && out_size && output && has_more,
                     PJ_EINVAL);

    if ((anmed_data->prm->enc_fmt.id != PJMEDIA_FORMAT_VP8) &&
	(anmed_data->prm->enc_fmt.id != PJMEDIA_FORMAT_VP9))
    {
    	*has_more = PJ_FALSE;
    	output->size = 0;
    	output->type = PJMEDIA_FRAME_TYPE_NONE;

	return PJ_SUCCESS;
    }

    if (anmed_data->enc_processed < anmed_data->enc_frame_size) {
    	unsigned payload_desc_size = 1;
        unsigned max_size = anmed_data->prm->enc_mtu - payload_desc_size;
        unsigned remaining_size = anmed_data->enc_frame_size -
        			  anmed_data->enc_processed;
        unsigned payload_len = PJ_MIN(remaining_size, max_size);
        pj_uint8_t *p = (pj_uint8_t *)output->buf;
        pj_bool_t is_keyframe = PJ_FALSE;

	if (payload_len + payload_desc_size > out_size)
	    return PJMEDIA_CODEC_EFRMTOOSHORT;

        output->type = PJMEDIA_FRAME_TYPE_VIDEO;
        output->bit_info = 0;

        is_keyframe = anmed_data->enc_buf_info.flags & ANMED_FRM_TYPE_KEYFRAME;
        if (is_keyframe) {
            output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;
        }

        /* Set payload header */
        p[0] = 0;
        if (anmed_data->prm->enc_fmt.id == PJMEDIA_FORMAT_VP8) {
	    /* Set N: Non-reference frame */
            if (!is_keyframe) p[0] |= 0x20;
            /* Set S: Start of VP8 partition. */
            if (anmed_data->enc_processed == 0) p[0] |= 0x10;
        } else if (anmed_data->prm->enc_fmt.id == PJMEDIA_FORMAT_VP9) {
	    /* Set P: Inter-picture predicted frame */
            if (!is_keyframe) p[0] |= 0x40;
            /* Set B: Start of a frame */
            if (anmed_data->enc_processed == 0) p[0] |= 0x8;
            /* Set E: End of a frame */
            if (anmed_data->enc_processed + payload_len ==
            	anmed_data->enc_frame_size)
            {
	    	p[0] |= 0x4;
	    }
	}

        pj_memcpy(p + payload_desc_size,
        	  (anmed_data->enc_frame_whole + anmed_data->enc_processed),
        	  payload_len);
        output->size = payload_len + payload_desc_size;

        anmed_data->enc_processed += payload_len;
        *has_more = (anmed_data->enc_processed < anmed_data->enc_frame_size);
    }

    return PJ_SUCCESS;
}

static pj_status_t configure_encoder(anmed_codec_data *anmed_data) 
{
    media_status_t am_status;
    AMediaFormat *vid_fmt;
    pjmedia_vid_codec_param *param = anmed_data->prm;

    vid_fmt = AMediaFormat_new();
    if (!vid_fmt) {
        return PJ_ENOMEM;
    }

    AMediaFormat_setString(vid_fmt, ANMED_KEY_MIME,
                           anmed_codec[anmed_data->codec_idx].mime_type);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_COLOR_FMT, ANMED_I420_PLANAR_FMT);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_HEIGHT,
			  param->enc_fmt.det.vid.size.h);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_WIDTH,
                          param->enc_fmt.det.vid.size.w);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_BIT_RATE,
                          param->enc_fmt.det.vid.avg_bps);
    //AMediaFormat_setInt32(vid_fmt, ANMED_KEY_PROFILE, 1);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_IFR_INTTERVAL, KEYFRAME_INTERVAL);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_FRAME_RATE,
                          (param->enc_fmt.det.vid.fps.num /
                           param->enc_fmt.det.vid.fps.denum));
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_PRIORITY, 0);

    /* Configure and start encoder. */
    am_status = AMediaCodec_configure(anmed_data->enc, vid_fmt, NULL, NULL,
                                      AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    AMediaFormat_delete(vid_fmt);
    if (am_status != AMEDIA_OK) {
        PJ_LOG(4, (THIS_FILE, "Encoder configure failed, status=%d",
        	   am_status));
        return PJMEDIA_CODEC_EFAILED;
    }
    am_status = AMediaCodec_start(anmed_data->enc);
    if (am_status != AMEDIA_OK) {
	PJ_LOG(4, (THIS_FILE, "Encoder start failed, status=%d",
		am_status));
	return PJMEDIA_CODEC_EFAILED;
    }
    return PJ_SUCCESS;
}

static pj_status_t configure_decoder(anmed_codec_data *anmed_data) {
    media_status_t am_status;
    AMediaFormat *vid_fmt;

    vid_fmt = AMediaFormat_new();
    if (!vid_fmt) {
	PJ_LOG(4, (THIS_FILE, "Decoder failed creating media format"));
        return PJ_ENOMEM;
    }
    AMediaFormat_setString(vid_fmt, ANMED_KEY_MIME,
                           anmed_codec[anmed_data->codec_idx].mime_type);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_COLOR_FMT, ANMED_I420_PLANAR_FMT);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_HEIGHT,
	                  anmed_data->prm->dec_fmt.det.vid.size.h);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_WIDTH,
	                  anmed_data->prm->dec_fmt.det.vid.size.w);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_MAX_INPUT_SZ, 0);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_ENCODER, 0);
    AMediaFormat_setInt32(vid_fmt, ANMED_KEY_PRIORITY, 0);

    if (anmed_data->prm->dec_fmt.id == PJMEDIA_FORMAT_H264) {
	avc_codec_data *avc_data = (avc_codec_data *)anmed_data->ex_data;

	if (avc_data->dec_sps_len) {
	    AMediaFormat_setBuffer(vid_fmt, ANMED_KEY_CSD0,
				   avc_data->dec_sps_buf,
				    avc_data->dec_sps_len);
	}
	if (avc_data->dec_pps_len) {
	    AMediaFormat_setBuffer(vid_fmt, ANMED_KEY_CSD1,
				   avc_data->dec_pps_buf,
				   avc_data->dec_pps_len);
	}
    }
    am_status = AMediaCodec_configure(anmed_data->dec, vid_fmt, NULL,
				      NULL, 0);

    AMediaFormat_delete(vid_fmt);
    if (am_status != AMEDIA_OK) {
        PJ_LOG(4, (THIS_FILE, "Decoder configure failed, status=%d, fmt_id=%d",
        	   am_status, anmed_data->prm->dec_fmt.id));
        return PJMEDIA_CODEC_EFAILED;
    }

    am_status = AMediaCodec_start(anmed_data->dec);
    if (am_status != AMEDIA_OK) {
	PJ_LOG(4, (THIS_FILE, "Decoder start failed, status=%d",
		   am_status));
	return PJMEDIA_CODEC_EFAILED;
    }
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_codec_anmed_vid_init(pjmedia_vid_codec_mgr *mgr,
                                                 pj_pool_factory *pf)
{
    const pj_str_t h264_name = { (char*)"H264", 4};
    pj_status_t status;

    if (anmed_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    if (!mgr) mgr = pjmedia_vid_codec_mgr_instance();
    PJ_ASSERT_RETURN(mgr, PJ_EINVAL);

    /* Create Android AMediaCodec codec factory. */
    anmed_factory.base.op = &anmed_factory_op;
    anmed_factory.base.factory_data = NULL;
    anmed_factory.mgr = mgr;
    anmed_factory.pf = pf;
    anmed_factory.pool = pj_pool_create(pf, "anmed_vid_factory", 256, 256,
                                        NULL);
    if (!anmed_factory.pool)
	return PJ_ENOMEM;

#if PJMEDIA_HAS_ANMED_AVC
    /* Registering format match for SDP negotiation */
    status = pjmedia_sdp_neg_register_fmt_match_cb(
					&h264_name,
					&pjmedia_vid_codec_h264_match_sdp);
    if (status != PJ_SUCCESS)
	goto on_error;
#endif

    /* Register codec factory to codec manager. */
    status = pjmedia_vid_codec_mgr_register_factory(mgr,
						    &anmed_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    PJ_LOG(4,(THIS_FILE, "Android AMediaCodec initialized"));

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(anmed_factory.pool);
    anmed_factory.pool = NULL;
    return status;
}

/*
 * Unregister Android AMediaCodec factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_anmed_vid_deinit(void)
{
    pj_status_t status = PJ_SUCCESS;

    if (anmed_factory.pool == NULL) {
	/* Already deinitialized */
	return PJ_SUCCESS;
    }

    /* Unregister Android AMediaCodec factory. */
    status = pjmedia_vid_codec_mgr_unregister_factory(anmed_factory.mgr,
						      &anmed_factory.base);

    /* Destroy pool. */
    pj_pool_release(anmed_factory.pool);
    anmed_factory.pool = NULL;

    return status;
}

static pj_status_t anmed_test_alloc(pjmedia_vid_codec_factory *factory,
                                    const pjmedia_vid_codec_info *info )
{
    unsigned i;

    PJ_ASSERT_RETURN(factory == &anmed_factory.base, PJ_EINVAL);

    for (i = 0; i < PJ_ARRAY_SIZE(anmed_codec); ++i) {
        if (anmed_codec[i].enabled && info->pt != 0 &&
            (info->fmt_id == anmed_codec[i].fmt_id))
        {
            return PJ_SUCCESS;
        }
    }

    return PJMEDIA_CODEC_EUNSUP;
}

static pj_status_t anmed_default_attr(pjmedia_vid_codec_factory *factory,
                                      const pjmedia_vid_codec_info *info,
                                      pjmedia_vid_codec_param *attr )
{
    unsigned i;

    PJ_ASSERT_RETURN(factory == &anmed_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(info && attr, PJ_EINVAL);

    for (i = 0; i < PJ_ARRAY_SIZE(anmed_codec); ++i) {
        if (anmed_codec[i].enabled && info->pt != 0 &&
            (info->fmt_id == anmed_codec[i].fmt_id))
        {
            break;
        }
    }

    if (i == PJ_ARRAY_SIZE(anmed_codec))
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

    attr->dec_fmtp = anmed_codec[i].dec_fmtp;

    /* Bitrate */
    attr->enc_fmt.det.vid.avg_bps = DEFAULT_AVG_BITRATE;
    attr->enc_fmt.det.vid.max_bps = DEFAULT_MAX_BITRATE;

    /* Encoding MTU */
    attr->enc_mtu = PJMEDIA_MAX_VID_PAYLOAD_SIZE;

    return PJ_SUCCESS;
}

static pj_status_t anmed_enum_info(pjmedia_vid_codec_factory *factory,
                                   unsigned *count,
                                   pjmedia_vid_codec_info info[])
{
    unsigned i, max;

    PJ_ASSERT_RETURN(info && *count > 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &anmed_factory.base, PJ_EINVAL);

    max = *count;
    for (i = 0, *count = 0; i < PJ_ARRAY_SIZE(anmed_codec) && *count < max;++i)
    {
	AMediaCodec *codec;
	char const *enc_name = anmed_codec[i].encoder_name;
	char const *dec_name = anmed_codec[i].decoder_name;

        if (!anmed_codec[i].enabled)
            continue;

        codec = AMediaCodec_createCodecByName(enc_name);
	if (!codec) {
	    PJ_LOG(4, (THIS_FILE, "Failed creating encoder: %s", enc_name));
	    anmed_codec[i].enabled = PJ_FALSE;
	    continue;
	}
	AMediaCodec_delete(codec);
	codec = AMediaCodec_createCodecByName(dec_name);
	if (!codec) {
	    PJ_LOG(4, (THIS_FILE, "Failed creating decoder: %s", dec_name));
	    anmed_codec[i].enabled = PJ_FALSE;
	    continue;
	}
	AMediaCodec_delete(codec);

        info[*count].fmt_id = anmed_codec[i].fmt_id;
        info[*count].pt = anmed_codec[i].pt;
        info[*count].encoding_name = pj_str((char *)anmed_codec[i].name);
        info[*count].encoding_desc = pj_str((char *)anmed_codec[i].description);

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

    return PJ_SUCCESS;
}

static void create_codec(struct anmed_codec_data *anmed_data)
{
    char const *enc_name = anmed_codec[anmed_data->codec_idx].encoder_name;
    char const *dec_name = anmed_codec[anmed_data->codec_idx].decoder_name;

    if (!anmed_data->enc) {
	anmed_data->enc = AMediaCodec_createCodecByName(enc_name);
	if (!anmed_data->enc) {
	    PJ_LOG(4, (THIS_FILE, "Failed creating encoder: %s", enc_name));
	}
    }

    if (!anmed_data->dec) {
	anmed_data->dec = AMediaCodec_createCodecByName(dec_name);
	if (!anmed_data->dec) {
	    PJ_LOG(4, (THIS_FILE, "Failed creating decoder: %s", dec_name));
	}
    }
}

static pj_status_t anmed_alloc_codec(pjmedia_vid_codec_factory *factory,
                                     const pjmedia_vid_codec_info *info,
                                     pjmedia_vid_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_vid_codec *codec;
    anmed_codec_data *anmed_data;
    int i, idx;

    PJ_ASSERT_RETURN(factory == &anmed_factory.base && info && p_codec,
                     PJ_EINVAL);

    idx = -1;
    for (i = 0; i < PJ_ARRAY_SIZE(anmed_codec); ++i) {
	if ((info->fmt_id == anmed_codec[i].fmt_id) &&
            (anmed_codec[i].enabled))
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
    pool = pj_pool_create(anmed_factory.pf, "anmedvid%p", 512, 512, NULL);
    if (!pool)
	return PJ_ENOMEM;

    /* codec instance */
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_codec);
    codec->factory = factory;
    codec->op = &anmed_codec_op;

    /* codec data */
    anmed_data = PJ_POOL_ZALLOC_T(pool, anmed_codec_data);
    anmed_data->pool = pool;
    anmed_data->codec_idx = idx;
    codec->codec_data = anmed_data;

    create_codec(anmed_data);
    if (!anmed_data->enc || !anmed_data->dec) {
	goto on_error;
    }

    *p_codec = codec;
    return PJ_SUCCESS;

on_error:
    anmed_dealloc_codec(factory, codec);
    return PJMEDIA_CODEC_EFAILED;
}

static pj_status_t anmed_dealloc_codec(pjmedia_vid_codec_factory *factory,
                                       pjmedia_vid_codec *codec )
{
    anmed_codec_data *anmed_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    PJ_UNUSED_ARG(factory);

    anmed_data = (anmed_codec_data*) codec->codec_data;
    if (anmed_data->enc) {
        AMediaCodec_stop(anmed_data->enc);
        AMediaCodec_delete(anmed_data->enc);
        anmed_data->enc = NULL;
    }

    if (anmed_data->dec) {
        AMediaCodec_stop(anmed_data->dec);
        AMediaCodec_delete(anmed_data->dec);
        anmed_data->dec = NULL;
    }
    pj_pool_release(anmed_data->pool);
    return PJ_SUCCESS;
}

static pj_status_t anmed_codec_init(pjmedia_vid_codec *codec,
                                    pj_pool_t *pool )
{
    PJ_ASSERT_RETURN(codec && pool, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

static pj_status_t anmed_codec_open(pjmedia_vid_codec *codec,
                                    pjmedia_vid_codec_param *codec_param)
{
    anmed_codec_data *anmed_data;
    pjmedia_vid_codec_param *param;
    pj_status_t status = PJ_SUCCESS;

    anmed_data = (anmed_codec_data*) codec->codec_data;
    anmed_data->prm = pjmedia_vid_codec_param_clone( anmed_data->pool,
                                                     codec_param);
    param = anmed_data->prm;
    if (anmed_codec[anmed_data->codec_idx].open_codec) {
        status = anmed_codec[anmed_data->codec_idx].open_codec(anmed_data);
        if (status != PJ_SUCCESS)
            return status;
    }
    anmed_data->whole = (param->packing == PJMEDIA_VID_PACKING_WHOLE);
    status = configure_encoder(anmed_data);
    if (status != PJ_SUCCESS) {
        return PJMEDIA_CODEC_EFAILED;
    }
    status = configure_decoder(anmed_data);
    if (status != PJ_SUCCESS) {
        return PJMEDIA_CODEC_EFAILED;
    }
    anmed_data->dec_buf_size = (MAX_RX_WIDTH * MAX_RX_HEIGHT * 3 >> 1) +
			       (MAX_RX_WIDTH);
    anmed_data->dec_buf = (pj_uint8_t*)pj_pool_alloc(anmed_data->pool,
                                                     anmed_data->dec_buf_size);
    /* Need to update param back after values are negotiated */
    pj_memcpy(codec_param, param, sizeof(*codec_param));

    return PJ_SUCCESS;
}

static pj_status_t anmed_codec_close(pjmedia_vid_codec *codec)
{
    PJ_ASSERT_RETURN(codec, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    return PJ_SUCCESS;
}

static pj_status_t anmed_codec_modify(pjmedia_vid_codec *codec,
                                      const pjmedia_vid_codec_param *param)
{
    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(param);
    return PJ_EINVALIDOP;
}

static pj_status_t anmed_codec_get_param(pjmedia_vid_codec *codec,
                                         pjmedia_vid_codec_param *param)
{
    struct anmed_codec_data *anmed_data;

    PJ_ASSERT_RETURN(codec && param, PJ_EINVAL);

    anmed_data = (anmed_codec_data*) codec->codec_data;
    pj_memcpy(param, anmed_data->prm, sizeof(*param));

    return PJ_SUCCESS;
}

static pj_status_t anmed_codec_encode_begin(pjmedia_vid_codec *codec,
                                            const pjmedia_vid_encode_opt *opt,
                                            const pjmedia_frame *input,
                                            unsigned out_size,
                                            pjmedia_frame *output,
                                            pj_bool_t *has_more)
{
    struct anmed_codec_data *anmed_data;
    unsigned i;
    pj_ssize_t buf_idx;

    PJ_ASSERT_RETURN(codec && input && out_size && output && has_more,
                     PJ_EINVAL);

    anmed_data = (anmed_codec_data*) codec->codec_data;

    if (opt && opt->force_keyframe) {
#if __ANDROID_API__ >=26
	AMediaFormat *vid_fmt = NULL;
	media_status_t am_status;

	vid_fmt = AMediaFormat_new();
	if (!vid_fmt) {
	    return PJMEDIA_CODEC_EFAILED;
	}
	AMediaFormat_setInt32(vid_fmt, ANMED_KEY_REQUEST_SYNCF, 0);
	am_status = AMediaCodec_setParameters(anmed_data->enc, vid_fmt);

	if (am_status != AMEDIA_OK)
	    PJ_LOG(4,(THIS_FILE, "Encoder setParameters failed %d", am_status));

	AMediaFormat_delete(vid_fmt);
#else
	PJ_LOG(5, (THIS_FILE, "Encoder cannot be forced to send keyframe"));
#endif
    }

    buf_idx = AMediaCodec_dequeueInputBuffer(anmed_data->enc,
					     CODEC_DEQUEUE_TIMEOUT);
    if (buf_idx >= 0) {
	media_status_t am_status;
	pj_size_t output_size;
	pj_uint8_t *input_buf = AMediaCodec_getInputBuffer(anmed_data->enc,
						    buf_idx, &output_size);
	if (input_buf && output_size >= input->size) {
	    pj_memcpy(input_buf, input->buf, input->size);
	    am_status = AMediaCodec_queueInputBuffer(anmed_data->enc,
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
				     "size: %d, expecting %d.",
				     input_buf, output_size, input->size));
	    }
	    goto on_return;
	}
    } else {
	PJ_LOG(4,(THIS_FILE, "Encoder dequeueInputBuffer failed[%d]", buf_idx));
	goto on_return;
    }

    for (i = 0; i < CODEC_WAIT_RETRY; ++i) {
	buf_idx = AMediaCodec_dequeueOutputBuffer(anmed_data->enc,
						  &anmed_data->enc_buf_info,
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
        pj_uint8_t *output_buf = AMediaCodec_getOutputBuffer(anmed_data->enc,
                                                             buf_idx,
                                                             &output_size);
        if (!output_buf) {
            PJ_LOG(4, (THIS_FILE, "Encoder failed getting output buffer, "
		       "buffer size %d, offset %d, flags %d",
		       anmed_data->enc_buf_info.size,
		       anmed_data->enc_buf_info.offset,
		       anmed_data->enc_buf_info.flags));
            goto on_return;
        }
        anmed_data->enc_processed = 0;
        anmed_data->enc_frame_whole = output_buf;
        anmed_data->enc_output_buf_idx = buf_idx;
        anmed_data->enc_frame_size = anmed_data->enc_buf_info.size;

        if (anmed_codec[anmed_data->codec_idx].process_encode) {
            pj_status_t status;

            status = anmed_codec[anmed_data->codec_idx].process_encode(
                                                                   anmed_data);

            if (status != PJ_SUCCESS)
                goto on_return;
        }

        if(anmed_data->enc_buf_info.flags & ANMED_FRM_TYPE_KEYFRAME) {
            output->bit_info |= PJMEDIA_VID_FRM_KEYFRAME;
        }

        if (anmed_data->whole) {
            unsigned payload_size = 0;
            unsigned start_data = 0;

            *has_more = PJ_FALSE;

            if ((anmed_data->prm->enc_fmt.id == PJMEDIA_FORMAT_H264) &&
        	(anmed_data->enc_buf_info.flags & ANMED_FRM_TYPE_KEYFRAME))
            {
        	avc_codec_data *avc_data =
        				  (avc_codec_data *)anmed_data->ex_data;
        	start_data = avc_data->enc_sps_pps_len;
                pj_memcpy(output->buf, avc_data->enc_sps_pps_buf,
                	  avc_data->enc_sps_pps_len);
            }

            payload_size = anmed_data->enc_buf_info.size + start_data;

            if (payload_size > out_size)
                return PJMEDIA_CODEC_EFRMTOOSHORT;

            output->type = PJMEDIA_FRAME_TYPE_VIDEO;
            output->size = payload_size;
            output->timestamp = input->timestamp;
            pj_memcpy((pj_uint8_t*)output->buf+start_data,
        	      anmed_data->enc_frame_whole,
        	      anmed_data->enc_buf_info.size);

	    AMediaCodec_releaseOutputBuffer(anmed_data->enc,
					    buf_idx,
					    0);

            return PJ_SUCCESS;
        }
    } else {
        if (buf_idx == -2) {
	    int width, height, color_fmt, stride;

	    /* Format change. */
	    AMediaFormat *vid_fmt = AMediaCodec_getOutputFormat(
							       anmed_data->enc);

	    AMediaFormat_getInt32(vid_fmt, ANMED_KEY_WIDTH, &width);
	    AMediaFormat_getInt32(vid_fmt, ANMED_KEY_HEIGHT, &height);
	    AMediaFormat_getInt32(vid_fmt, ANMED_KEY_COLOR_FMT, &color_fmt);
	    AMediaFormat_getInt32(vid_fmt, ANMED_KEY_STRIDE, &stride);
	    PJ_LOG(5, (THIS_FILE, "Encoder detect new width %d, height %d, "
		       "color_fmt 0x%X, stride %d buf_size %d",
		       width, height, color_fmt, stride,
		       anmed_data->enc_buf_info.size));

	    AMediaFormat_delete(vid_fmt);
        } else {
	    PJ_LOG(4, (THIS_FILE, "Encoder dequeueOutputBuffer failed[%d]",
		       buf_idx));
        }
        goto on_return;
    }

    return anmed_codec_encode_more(codec, out_size, output, has_more);

on_return:
    output->size = 0;
    output->type = PJMEDIA_FRAME_TYPE_NONE;
    *has_more = PJ_FALSE;
    return PJ_SUCCESS;
}

static pj_status_t anmed_codec_encode_more(pjmedia_vid_codec *codec,
                                           unsigned out_size,
                                           pjmedia_frame *output,
                                           pj_bool_t *has_more)
{
    struct anmed_codec_data *anmed_data;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(codec && out_size && output && has_more, PJ_EINVAL);

    anmed_data = (anmed_codec_data*) codec->codec_data;

    status = anmed_codec[anmed_data->codec_idx].encode_more(anmed_data,
							    out_size, output,
							    has_more);
    if (!(*has_more)) {
	AMediaCodec_releaseOutputBuffer(anmed_data->enc,
					anmed_data->enc_output_buf_idx,
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

static void anmed_get_input_buffer(struct anmed_codec_data *anmed_data)
{
    pj_ssize_t buf_idx = -1;

    buf_idx = AMediaCodec_dequeueInputBuffer(anmed_data->dec,
					     CODEC_DEQUEUE_TIMEOUT);

    if (buf_idx < 0) {
	PJ_LOG(4,(THIS_FILE, "Decoder dequeueInputBuffer failed return %d",
		  buf_idx));

	anmed_data->dec_input_buf = NULL;

	if (buf_idx == -10000) {
	    PJ_LOG(5, (THIS_FILE, "Resetting decoder"));
	    AMediaCodec_stop(anmed_data->dec);
	    AMediaCodec_delete(anmed_data->dec);
	    anmed_data->dec = NULL;

	    create_codec(anmed_data);
	    if (anmed_data->dec)
		configure_decoder(anmed_data);
	}
	return;
    }

    anmed_data->dec_input_buf_len = 0;
    anmed_data->dec_input_buf_idx = buf_idx;
    anmed_data->dec_input_buf = AMediaCodec_getInputBuffer(anmed_data->dec,
				       buf_idx,
				       &anmed_data->dec_input_buf_max_size);
}

static pj_status_t anmed_decode(pjmedia_vid_codec *codec,
			        struct anmed_codec_data *anmed_data,
			        pj_uint8_t *input_buf, unsigned buf_size,
			        int buf_flag, pj_timestamp *input_ts,
			        pj_bool_t write_output, pjmedia_frame *output)
{
    pj_ssize_t buf_idx = 0;
    pj_status_t status = PJ_SUCCESS;
    media_status_t am_status;

    if ((anmed_data->dec_input_buf_max_size > 0) &&
	(anmed_data->dec_input_buf_len + buf_size >
         anmed_data->dec_input_buf_max_size))
    {
	am_status = AMediaCodec_queueInputBuffer(anmed_data->dec,
					         anmed_data->dec_input_buf_idx,
					         0,
					         anmed_data->dec_input_buf_len,
					         input_ts->u32.lo,
					         buf_flag);
	if (am_status != AMEDIA_OK) {
	    PJ_LOG(4,(THIS_FILE, "Decoder queueInputBuffer idx[%d] return %d",
		    anmed_data->dec_input_buf_idx, am_status));
	    return status;
	}
	anmed_data->dec_input_buf = NULL;
    }

    if (anmed_data->dec_input_buf == NULL)
    {
	anmed_get_input_buffer(anmed_data);

	if (anmed_data->dec_input_buf == NULL) {
	    PJ_LOG(4,(THIS_FILE, "Decoder failed getting input buffer"));
	    return status;
	}
    }
    pj_memcpy(anmed_data->dec_input_buf + anmed_data->dec_input_buf_len,
	      input_buf, buf_size);

    anmed_data->dec_input_buf_len += buf_size;

    if (!write_output)
	return status;

    am_status = AMediaCodec_queueInputBuffer(anmed_data->dec,
					     anmed_data->dec_input_buf_idx,
					     0,
					     anmed_data->dec_input_buf_len,
					     input_ts->u32.lo,
					     buf_flag);
    if (am_status != AMEDIA_OK) {
	PJ_LOG(4,(THIS_FILE, "Decoder queueInputBuffer failed return %d",
		  am_status));
	anmed_data->dec_input_buf = NULL;
	return status;
    }
    anmed_data->dec_input_buf_len += buf_size;

    buf_idx = AMediaCodec_dequeueOutputBuffer(anmed_data->dec,
					      &anmed_data->dec_buf_info,
					      CODEC_DEQUEUE_TIMEOUT);

    if (buf_idx >= 0) {
	pj_size_t output_size;
	int len;

	pj_uint8_t *output_buf = AMediaCodec_getOutputBuffer(anmed_data->dec,
							     buf_idx,
							     &output_size);
	if (output_buf == NULL) {
	    am_status = AMediaCodec_releaseOutputBuffer(anmed_data->dec,
					    buf_idx,
					    0);
	    PJ_LOG(4,(THIS_FILE, "Decoder getOutputBuffer failed"));
	    return status;
	}
	len = write_yuv((pj_uint8_t *)output->buf,
			anmed_data->dec_buf_info.size,
			output_buf,
			anmed_data->dec_stride_len,
			anmed_data->prm->dec_fmt.det.vid.size.w,
			anmed_data->prm->dec_fmt.det.vid.size.h);

	am_status = AMediaCodec_releaseOutputBuffer(anmed_data->dec,
						    buf_idx,
						    0);

	if (len > 0) {
	    if (!anmed_data->dec_has_output_frame) {
		output->type = PJMEDIA_FRAME_TYPE_VIDEO;
		output->size = len;
		output->timestamp = *input_ts;

		anmed_data->dec_has_output_frame = PJ_TRUE;
	    }
	} else {
	    status = PJMEDIA_CODEC_EFRMTOOSHORT;
	}
    } else if (buf_idx == -2) {
	int width, height, stride;
	AMediaFormat *vid_fmt;
	/* Get output format */
	vid_fmt = AMediaCodec_getOutputFormat(anmed_data->dec);

	AMediaFormat_getInt32(vid_fmt, ANMED_KEY_WIDTH, &width);
	AMediaFormat_getInt32(vid_fmt, ANMED_KEY_HEIGHT, &height);
	AMediaFormat_getInt32(vid_fmt, ANMED_KEY_STRIDE, &stride);

	AMediaFormat_delete(vid_fmt);
	anmed_data->dec_stride_len = stride;
	if (width != anmed_data->prm->dec_fmt.det.vid.size.w ||
	    height != anmed_data->prm->dec_fmt.det.vid.size.h)
	{
	    pjmedia_event event;

	    anmed_data->prm->dec_fmt.det.vid.size.w = width;
	    anmed_data->prm->dec_fmt.det.vid.size.h = height;

	    PJ_LOG(4,(THIS_FILE, "Frame size changed to %dx%d",
		      anmed_data->prm->dec_fmt.det.vid.size.w,
		      anmed_data->prm->dec_fmt.det.vid.size.h));

	    /* Broadcast format changed event */
	    pjmedia_event_init(&event, PJMEDIA_EVENT_FMT_CHANGED,
			       &output->timestamp, codec);
	    event.data.fmt_changed.dir = PJMEDIA_DIR_DECODING;
	    pjmedia_format_copy(&event.data.fmt_changed.new_fmt,
				&anmed_data->prm->dec_fmt);
	    pjmedia_event_publish(NULL, codec, &event,
				  PJMEDIA_EVENT_PUBLISH_DEFAULT);
	}
    } else {
	PJ_LOG(4,(THIS_FILE, "Decoder dequeueOutputBuffer failed [%d]",
		  buf_idx));
    }
    return status;
}

static pj_status_t decode_avc(pjmedia_vid_codec *codec,
			      pj_size_t count,
			      pjmedia_frame packets[],
			      unsigned out_size,
			      pjmedia_frame *output)
{
    struct anmed_codec_data *anmed_data;
    const pj_uint8_t start_code[] = { 0, 0, 0, 1 };
    const int code_size = PJ_ARRAY_SIZE(start_code);
    unsigned buf_pos, whole_len = 0;
    unsigned i, frm_cnt;
    pj_status_t status;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    anmed_data = (anmed_codec_data*) codec->codec_data;

    /*
     * Step 1: unpacketize the packets/frames
     */
    whole_len = 0;
    if (anmed_data->whole) {
	for (i=0; i<count; ++i) {
	    if (whole_len + packets[i].size > anmed_data->dec_buf_size) {
		PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [1]"));
		return PJMEDIA_CODEC_EFRMTOOSHORT;
	    }

	    pj_memcpy( anmed_data->dec_buf + whole_len,
	               (pj_uint8_t*)packets[i].buf,
	               packets[i].size);
	    whole_len += packets[i].size;
	}

    } else {
        avc_codec_data *avc_data = (avc_codec_data *)anmed_data->ex_data;

	for (i=0; i<count; ++i) {

	    if (whole_len + packets[i].size + code_size >
                anmed_data->dec_buf_size)
	    {
		PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [2]"));
		return PJMEDIA_CODEC_EFRMTOOSHORT;
	    }

	    status = pjmedia_h264_unpacketize( avc_data->pktz,
					       (pj_uint8_t*)packets[i].buf,
					       packets[i].size,
					       anmed_data->dec_buf,
					       anmed_data->dec_buf_size,
					       &whole_len);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(4,(THIS_FILE, status, "Unpacketize error"));
		continue;
	    }
	}
    }

    if (whole_len + code_size > anmed_data->dec_buf_size ||
    	whole_len <= code_size + 1)
    {
	PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow or unpacketize error "
			     "size: %d, buffer: %d", whole_len,
			     anmed_data->dec_buf_size));
	return PJMEDIA_CODEC_EFRMTOOSHORT;
    }

    /* Dummy NAL sentinel */
    pj_memcpy(anmed_data->dec_buf + whole_len, start_code, code_size);

    /*
     * Step 2: parse the individual NAL and give to decoder
     */
    buf_pos = 0;
    for ( frm_cnt=0; ; ++frm_cnt) {
	pj_uint32_t frm_size;
	pj_bool_t write_output = PJ_FALSE;
	unsigned char *start;

	for (i = code_size - 1; buf_pos + i < whole_len; i++) {
	    if (anmed_data->dec_buf[buf_pos + i] == 0 &&
		anmed_data->dec_buf[buf_pos + i + 1] == 0 &&
		anmed_data->dec_buf[buf_pos + i + 2] == 0 &&
		anmed_data->dec_buf[buf_pos + i + 3] == 1)
	    {
		break;
	    }
	}

	frm_size = i;
	start = anmed_data->dec_buf + buf_pos;
	write_output = (buf_pos + frm_size >= whole_len);

	status = anmed_decode(codec, anmed_data, start, frm_size, 0,
		              &packets[0].timestamp, write_output, output);
	if (status != PJ_SUCCESS)
	    return status;

	if (write_output)
	    break;

	buf_pos += frm_size;
    }

    return PJ_SUCCESS;
}

static pj_status_t vpx_unpacketize(struct anmed_codec_data *anmed_data,
				   const pj_uint8_t *buf,
                                   pj_size_t packet_size,
				   unsigned *p_desc_len)
{
    unsigned desc_len = 1;
    pj_uint8_t *p = (pj_uint8_t *)buf;

#define INC_DESC_LEN() {if (++desc_len >= packet_size) return PJ_ETOOSMALL;}

    if (packet_size <= desc_len) return PJ_ETOOSMALL;

    if (anmed_data->prm->enc_fmt.id == PJMEDIA_FORMAT_VP8) {
        /*  0 1 2 3 4 5 6 7
         * +-+-+-+-+-+-+-+-+
         * |X|R|N|S|R| PID | (REQUIRED)
         */
	/* X: Extended control bits present. */
	if (p[0] & 0x80) {
	    INC_DESC_LEN();
	    /* |I|L|T|K| RSV   | */
	    /* I: PictureID present. */
	    if (p[1] & 0x80) {
	    	INC_DESC_LEN();
	    	/* If M bit is set, the PID field MUST contain 15 bits. */
	    	if (p[2] & 0x80) INC_DESC_LEN();
	    }
	    /* L: TL0PICIDX present. */
	    if (p[1] & 0x40) INC_DESC_LEN();
	    /* T: TID present or K: KEYIDX present. */
	    if ((p[1] & 0x20) || (p[1] & 0x10)) INC_DESC_LEN();
	}

    } else if (anmed_data->prm->enc_fmt.id == PJMEDIA_FORMAT_VP9) {
        /*  0 1 2 3 4 5 6 7
         * +-+-+-+-+-+-+-+-+
         * |I|P|L|F|B|E|V|-| (REQUIRED)
         */
        /* I: Picture ID (PID) present. */
	if (p[0] & 0x80) {
	    INC_DESC_LEN();
	    /* If M bit is set, the PID field MUST contain 15 bits. */
	    if (p[1] & 0x80) INC_DESC_LEN();
	}
	/* L: Layer indices present. */
	if (p[0] & 0x20) {
	    INC_DESC_LEN();
	    if (!(p[0] & 0x10)) INC_DESC_LEN();
	}
	/* F: Flexible mode.
	 * I must also be set to 1, and if P is set, there's up to 3
	 * reference index.
	 */
	if ((p[0] & 0x10) && (p[0] & 0x80) && (p[0] & 0x40)) {
	    unsigned char *q = p + desc_len;

	    INC_DESC_LEN();
	    if (*q & 0x1) {
	    	q++;
	    	INC_DESC_LEN();
	    	if (*q & 0x1) {
	    	    q++;
	    	    INC_DESC_LEN();
	    	}
	    }
	}
	/* V: Scalability structure (SS) data present. */
	if (p[0] & 0x2) {
	    unsigned char *q = p + desc_len;
	    unsigned N_S = (*q >> 5) + 1;

	    INC_DESC_LEN();
	    /* Y: Each spatial layer's frame resolution present. */
	    if (*q & 0x10) desc_len += N_S * 4;

	    /* G: PG description present flag. */
	    if (*q & 0x8) {
	    	unsigned j;
	    	unsigned N_G = *(p + desc_len);

	    	INC_DESC_LEN();
	    	for (j = 0; j< N_G; j++) {
	    	    unsigned R;

	    	    q = p + desc_len;
	    	    INC_DESC_LEN();
	    	    R = (*q & 0x0F) >> 2;
	    	    desc_len += R;
	    	    if (desc_len >= packet_size)
	    	    	return PJ_ETOOSMALL;
	    	}
	    }
	}
    }
#undef INC_DESC_LEN

    *p_desc_len = desc_len;
    return PJ_SUCCESS;
}

static pj_status_t decode_vpx(pjmedia_vid_codec *codec,
			      pj_size_t count,
			      pjmedia_frame packets[],
			      unsigned out_size,
			      pjmedia_frame *output)
{
    anmed_codec_data *anmed_data;
    unsigned i, whole_len = 0;
    pj_status_t status;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    anmed_data = (anmed_codec_data*) codec->codec_data;

    whole_len = 0;
    if (anmed_data->whole) {
	for (i = 0; i < count; ++i) {
	    if (whole_len + packets[i].size > anmed_data->dec_buf_size) {
		PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [1]"));
		return PJMEDIA_CODEC_EFRMTOOSHORT;
	    }

	    pj_memcpy( anmed_data->dec_buf + whole_len,
	               (pj_uint8_t*)packets[i].buf,
	               packets[i].size);
	    whole_len += packets[i].size;
	}
	status = anmed_decode(codec, anmed_data, anmed_data->dec_buf,
			      whole_len, 0, &packets[0].timestamp,
			      PJ_TRUE, output);

	if (status != PJ_SUCCESS)
	    return status;

    } else {
    	for (i = 0; i < count; ++i) {
    	    unsigned desc_len;
    	    unsigned packet_size = packets[i].size;
    	    pj_status_t status;
    	    pj_bool_t write_output;

    	    status = vpx_unpacketize(anmed_data, (pj_uint8_t *)packets[i].buf,
    				     packet_size, &desc_len);
    	    if (status != PJ_SUCCESS) {
	    	PJ_LOG(4,(THIS_FILE, "Unpacketize error packet size[%d]",
	    		  packet_size));
	    	return status;
    	    }

    	    packet_size -= desc_len;
    	    if (whole_len + packet_size > anmed_data->dec_buf_size) {
	    	PJ_LOG(4,(THIS_FILE, "Decoding buffer overflow [2]"));
	    	return PJMEDIA_CODEC_EFRMTOOSHORT;
            }

    	    write_output = (i == count - 1);

    	    status = anmed_decode(codec, anmed_data,
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

static pj_status_t anmed_codec_decode(pjmedia_vid_codec *codec,
                                      pj_size_t count,
                                      pjmedia_frame packets[],
                                      unsigned out_size,
                                      pjmedia_frame *output)
{
    struct anmed_codec_data *anmed_data;
    pj_status_t status = PJ_EINVAL;

    PJ_ASSERT_RETURN(codec && count && packets && out_size && output,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(output->buf, PJ_EINVAL);

    anmed_data = (anmed_codec_data*) codec->codec_data;
    anmed_data->dec_has_output_frame = PJ_FALSE;
    anmed_data->dec_input_buf = NULL;
    anmed_data->dec_input_buf_len = 0;

    if (anmed_codec[anmed_data->codec_idx].decode) {
        status = anmed_codec[anmed_data->codec_idx].decode(codec, count,
                                                           packets, out_size,
                                                           output);
    }
    if (status != PJ_SUCCESS) {
	return status;
    }
    if (!anmed_data->dec_has_output_frame) {
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

#endif	/* PJMEDIA_HAS_ANDROID_MEDIACODEC */

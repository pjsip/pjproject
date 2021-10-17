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
#include <pjmedia-codec/and_aud_mediacodec.h>
#include <pjmedia-codec/amr_sdp_match.h>
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/plc.h>
#include <pjmedia/port.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/os.h>

/*
 * Only build this file if PJMEDIA_HAS_ANDROID_MEDIACODEC != 0
 */
#if defined(PJMEDIA_HAS_ANDROID_MEDIACODEC) && \
            PJMEDIA_HAS_ANDROID_MEDIACODEC != 0

/* Android AMediaCodec: */
#include "media/NdkMediaCodec.h"

#define THIS_FILE  "and_aud_mediacodec.cpp"

#define AND_MEDIA_KEY_PCM_ENCODING       "pcm-encoding"
#define AND_MEDIA_KEY_CHANNEL_COUNT      "channel-count"
#define AND_MEDIA_KEY_SAMPLE_RATE        "sample-rate"
#define AND_MEDIA_KEY_BITRATE            "bitrate"
#define AND_MEDIA_KEY_MIME               "mime"

#define CODEC_WAIT_RETRY 	10
#define CODEC_THREAD_WAIT 	10
/* Timeout until the buffer is ready in ms. */
#define CODEC_DEQUEUE_TIMEOUT 	10

/* Prototypes for Android MediaCodec codecs factory */
static pj_status_t and_media_test_alloc(pjmedia_codec_factory *factory,
					const pjmedia_codec_info *id );
static pj_status_t and_media_default_attr(pjmedia_codec_factory *factory,
					  const pjmedia_codec_info *id,
					  pjmedia_codec_param *attr );
static pj_status_t and_media_enum_codecs(pjmedia_codec_factory *factory,
					 unsigned *count,
					 pjmedia_codec_info codecs[]);
static pj_status_t and_media_alloc_codec(pjmedia_codec_factory *factory,
					 const pjmedia_codec_info *id,
					 pjmedia_codec **p_codec);
static pj_status_t and_media_dealloc_codec(pjmedia_codec_factory *factory,
					   pjmedia_codec *codec );

/* Prototypes for Android MediaCodec codecs implementation. */
static pj_status_t  and_media_codec_init(pjmedia_codec *codec,
					 pj_pool_t *pool );
static pj_status_t  and_media_codec_open(pjmedia_codec *codec,
					 pjmedia_codec_param *attr );
static pj_status_t  and_media_codec_close(pjmedia_codec *codec );
static pj_status_t  and_media_codec_modify(pjmedia_codec *codec,
					   const pjmedia_codec_param *attr );
static pj_status_t  and_media_codec_parse(pjmedia_codec *codec,
					  void *pkt,
					  pj_size_t pkt_size,
					  const pj_timestamp *ts,
					  unsigned *frame_cnt,
					  pjmedia_frame frames[]);
static pj_status_t  and_media_codec_encode(pjmedia_codec *codec,
					   const struct pjmedia_frame *input,
					   unsigned output_buf_len,
					   struct pjmedia_frame *output);
static pj_status_t  and_media_codec_decode(pjmedia_codec *codec,
					   const struct pjmedia_frame *input,
					   unsigned output_buf_len,
					   struct pjmedia_frame *output);
static pj_status_t  and_media_codec_recover(pjmedia_codec *codec,
					    unsigned output_buf_len,
					    struct pjmedia_frame *output);

/* Definition for Android MediaCodec codecs operations. */
static pjmedia_codec_op and_media_op =
{
    &and_media_codec_init,
    &and_media_codec_open,
    &and_media_codec_close,
    &and_media_codec_modify,
    &and_media_codec_parse,
    &and_media_codec_encode,
    &and_media_codec_decode,
    &and_media_codec_recover
};

/* Definition for Android MediaCodec codecs factory operations. */
static pjmedia_codec_factory_op and_media_factory_op =
{
    &and_media_test_alloc,
    &and_media_default_attr,
    &and_media_enum_codecs,
    &and_media_alloc_codec,
    &and_media_dealloc_codec,
    &pjmedia_codec_and_media_aud_deinit
};

/* Android MediaCodec codecs factory */
static struct and_media_factory {
    pjmedia_codec_factory    base;
    pjmedia_endpt	    *endpt;
    pj_pool_t		    *pool;
    pj_mutex_t        	    *mutex;
} and_media_factory;

typedef enum and_aud_codec_id {
    /* AMRNB codec. */
    AND_AUD_CODEC_AMRNB,

    /* AMRWB codec. */
    AND_AUD_CODEC_AMRWB
} and_aud_codec_id;

/* Android MediaCodec codecs private data. */
typedef struct and_media_private {
    int			 codec_idx;	    /**< Codec index.		    */
    void		*codec_setting;	    /**< Specific codec setting.    */
    pj_pool_t		*pool;		    /**< Pool for each instance.    */
    AMediaCodec         *enc;               /**< Encoder state.		    */
    AMediaCodec         *dec;               /**< Decoder state.		    */

    pj_uint16_t		 frame_size;	    /**< Bitstream frame size.	    */

    pj_bool_t		 plc_enabled;	    /**< PLC enabled flag.	    */
    pjmedia_plc		*plc;		    /**< PJMEDIA PLC engine, NULL if 
						 codec has internal PLC.    */

    pj_bool_t		 vad_enabled;	    /**< VAD enabled flag.	    */
    pjmedia_silence_det	*vad;		    /**< PJMEDIA VAD engine, NULL if 
						 codec has internal VAD.    */
    pj_timestamp	 last_tx;	    /**< Timestamp of last transmit.*/
} and_media_private_t;

/* CUSTOM CALLBACKS */

/* Parse frames from a packet. Default behaviour of frame parsing is 
 * just separating frames based on calculating frame length derived 
 * from bitrate. Implement this callback when the default behaviour is 
 * unapplicable.
 */
typedef pj_status_t (*parse_cb)(and_media_private_t *codec_data, void *pkt,
				pj_size_t pkt_size, const pj_timestamp *ts,
				unsigned *frame_cnt, pjmedia_frame frames[]);

/* Pack frames into a packet. Default behaviour of packing frames is 
 * just stacking the frames with octet aligned without adding any 
 * payload header. Implement this callback when the default behaviour is
 * unapplicable.
 */
typedef pj_status_t (*pack_cb)(and_media_private_t *codec_data,
			       unsigned nframes, void *pkt, pj_size_t *pkt_size,
			       pj_size_t max_pkt_size);

/* This callback is useful for preparing a frame before pass it to decoder.
 */
typedef void (*predecode_cb)(and_media_private_t  *codec_data,
			     const pjmedia_frame *rtp_frame,
			     pjmedia_frame *out);

#if PJMEDIA_HAS_AND_MEDIA_AMRNB || PJMEDIA_HAS_AND_MEDIA_AMRWB
/* Custom callback implementations. */
static pj_status_t parse_amr(and_media_private_t *codec_data, void *pkt,
			     pj_size_t pkt_size, const pj_timestamp *ts,
			     unsigned *frame_cnt, pjmedia_frame frames[]);
static  pj_status_t pack_amr(and_media_private_t *codec_data, unsigned nframes,
			     void *pkt, pj_size_t *pkt_size,
			     pj_size_t max_pkt_size);
static void predecode_amr(and_media_private_t  *codec_data,
			  const pjmedia_frame *input,
			  pjmedia_frame *out);
#endif

#if PJMEDIA_HAS_AND_MEDIA_AMRNB

static pj_str_t AMRNB_encoder[] = {{(char *)"OMX.google.amrnb.encoder\0", 24},
				   {(char *)"c2.android.amrnb.encoder\0", 24}};

static pj_str_t AMRNB_decoder[] = {{(char *)"OMX.google.amrnb.decoder\0", 24},
			           {(char *)"c2.android.amrnb.decoder\0", 24}};
#endif

#if PJMEDIA_HAS_AND_MEDIA_AMRWB

static pj_str_t AMRWB_encoder[] = {{(char *)"OMX.google.amrwb.encoder\0", 24},
				   {(char *)"c2.android.amrwb.encoder\0", 24}};

static pj_str_t AMRWB_decoder[] = {{(char *)"OMX.google.amrwb.decoder\0", 24},
				   {(char *)"c2.android.amrwb.decoder\0", 24}};
#endif

/* Android MediaCodec codec implementation descriptions. */
static struct and_media_codec {
    int		     enabled;		/* Is this codec enabled?	    */
    const char	    *name;		/* Codec name.			    */
    const char      *mime_type;         /* Mime type.                       */
    pj_str_t        *encoder_name;      /* Encoder name.                    */
    pj_str_t        *decoder_name;      /* Decoder name.                    */

    pj_uint8_t	     pt;		/* Payload type.		    */
    and_aud_codec_id codec_id;		/* Codec id.                        */
    unsigned	     clock_rate;	/* Codec's clock rate.		    */
    unsigned	     channel_count;	/* Codec's channel count.	    */
    unsigned	     samples_per_frame;	/* Codec's samples count.	    */
    unsigned	     def_bitrate;	/* Default bitrate of this codec.   */
    unsigned	     max_bitrate;	/* Maximum bitrate of this codec.   */
    pj_uint8_t	     frm_per_pkt;	/* Default num of frames per packet.*/
    int		     has_native_vad;	/* Codec has internal VAD?	    */
    int		     has_native_plc;	/* Codec has internal PLC?	    */

    parse_cb	     parse;		/* Callback to parse bitstream.	    */
    pack_cb	     pack;		/* Callback to pack bitstream.	    */
    predecode_cb     predecode;         /* Callback to prepare bitstream
                                           before passing it to decoder.    */

    pjmedia_codec_fmtp dec_fmtp;	/* Decoder's fmtp params.	    */
}

and_media_codec[] =
{
#   if PJMEDIA_HAS_AND_MEDIA_AMRNB
    {0, "AMR", "audio/3gpp", NULL, NULL,
        PJMEDIA_RTP_PT_AMR, AND_AUD_CODEC_AMRNB, 8000, 1, 160, 7400, 12200,
        2, 0, 0, &parse_amr, &pack_amr, &predecode_amr,
        {1, {{{(char *)"octet-align", 11}, {(char *)"1", 1}}}}
    },
#   endif

#   if PJMEDIA_HAS_AND_MEDIA_AMRWB
    {0, "AMR-WB", "audio/amr-wb", NULL, NULL,
        PJMEDIA_RTP_PT_AMRWB, AND_AUD_CODEC_AMRWB, 16000, 1, 320, 15850, 23850,
        2, 0, 0, &parse_amr, &pack_amr, &predecode_amr,
	{1, {{{(char *)"octet-align", 11}, {(char *)"1", 1}}}}
    },
#   endif
};

#if PJMEDIA_HAS_AND_MEDIA_AMRNB || PJMEDIA_HAS_AND_MEDIA_AMRWB

#include <pjmedia-codec/amr_helper.h>

typedef struct amr_settings_t {
    pjmedia_codec_amr_pack_setting enc_setting;
    pjmedia_codec_amr_pack_setting dec_setting;
    pj_int8_t enc_mode;
} amr_settings_t;

/* Pack AMR payload */
static pj_status_t pack_amr(and_media_private_t *codec_data, unsigned nframes,
			    void *pkt, pj_size_t *pkt_size,
			    pj_size_t max_pkt_size)
{
    enum {MAX_FRAMES_PER_PACKET = PJMEDIA_MAX_FRAME_DURATION_MS / 20};

    pjmedia_frame frames[MAX_FRAMES_PER_PACKET];
    pj_uint8_t *p; /* Read cursor */
    pjmedia_codec_amr_pack_setting *setting;
    unsigned i;
    pj_status_t status;

    setting = &((amr_settings_t*)codec_data->codec_setting)->enc_setting;

    /* Align pkt buf right */
    p = (pj_uint8_t*)pkt + max_pkt_size - *pkt_size;
    pj_memmove(p, pkt, *pkt_size);

    /* Get frames */
    for (i = 0; i < nframes; ++i) {
	pjmedia_codec_amr_bit_info *info = (pjmedia_codec_amr_bit_info*)
					    &frames[i].bit_info;
	pj_bzero(info, sizeof(*info));
	info->frame_type = (pj_uint8_t)((*p >> 3) & 0x0F);
	info->good_quality = (pj_uint8_t)((*p >> 2) & 0x01);
	info->mode = ((amr_settings_t*)codec_data->codec_setting)->enc_mode;
	info->start_bit = 0;
	frames[i].buf = p + 1;
        if (setting->amr_nb) {
            frames[i].size = (info->frame_type <= 8)?
                             pjmedia_codec_amrnb_framelen[info->frame_type] : 0;
        } else {
            frames[i].size = (info->frame_type <= 9)?
                             pjmedia_codec_amrwb_framelen[info->frame_type] : 0;
        }
	p += frames[i].size + 1;
    }
    /* Pack */
    *pkt_size = max_pkt_size;
    status = pjmedia_codec_amr_pack(frames, nframes, setting, pkt, pkt_size);

    return status;
}

/* Parse AMR payload into frames. */
static pj_status_t parse_amr(and_media_private_t *codec_data, void *pkt,
			     pj_size_t pkt_size, const pj_timestamp *ts,
			     unsigned *frame_cnt, pjmedia_frame frames[])
{
    amr_settings_t* s = (amr_settings_t*)codec_data->codec_setting;
    pjmedia_codec_amr_pack_setting *setting;
    pj_status_t status;
    pj_uint8_t cmr;

    setting = &s->dec_setting;
    status = pjmedia_codec_amr_parse(pkt, pkt_size, ts, setting, frames, 
				     frame_cnt, &cmr);
    if (status != PJ_SUCCESS)
	return status;

    /* Check Change Mode Request. */
    if (((setting->amr_nb && cmr <= 7) || (!setting->amr_nb && cmr <= 8)) &&
	s->enc_mode != cmr)
    {
	s->enc_mode = cmr;
    }
    return PJ_SUCCESS;
}

static void predecode_amr(and_media_private_t *codec_data,
			  const pjmedia_frame *input,
			  pjmedia_frame *out)
{
    pjmedia_codec_amr_bit_info *info;
    pj_uint8_t *bitstream = (pj_uint8_t *)out->buf;
    pjmedia_codec_amr_pack_setting *setting;

    out->buf = &bitstream[1];
    setting = &((amr_settings_t*)codec_data->codec_setting)->dec_setting;
    pjmedia_codec_amr_predecode(input, setting, out);
    info = (pjmedia_codec_amr_bit_info*)&out->bit_info;
    bitstream[0] = (info->frame_type << 3) | (info->good_quality << 2);
    out->buf = &bitstream[0];
    ++out->size;
}

#endif /* PJMEDIA_HAS_AND_MEDIA_AMRNB || PJMEDIA_HAS_AND_MEDIA_AMRWB */

static pj_status_t configure_codec(and_media_private_t *and_media_data,
				   pj_bool_t is_encoder)
{
    media_status_t am_status;
    AMediaFormat *aud_fmt;
    int idx = and_media_data->codec_idx;
    AMediaCodec *codec = (is_encoder?and_media_data->enc:and_media_data->dec);

    aud_fmt = AMediaFormat_new();
    if (!aud_fmt) {
        return PJ_ENOMEM;
    }
    AMediaFormat_setString(aud_fmt, AND_MEDIA_KEY_MIME,
                           and_media_codec[idx].mime_type);
    AMediaFormat_setInt32(aud_fmt, AND_MEDIA_KEY_PCM_ENCODING, 2);
    AMediaFormat_setInt32(aud_fmt, AND_MEDIA_KEY_CHANNEL_COUNT,
                          and_media_codec[idx].channel_count);
    AMediaFormat_setInt32(aud_fmt, AND_MEDIA_KEY_SAMPLE_RATE,
			  and_media_codec[idx].clock_rate);
    AMediaFormat_setInt32(aud_fmt, AND_MEDIA_KEY_BITRATE,
			  and_media_codec[idx].def_bitrate);

    /* Configure and start encoder. */
    am_status = AMediaCodec_configure(codec, aud_fmt, NULL, NULL, is_encoder);
    AMediaFormat_delete(aud_fmt);
    if (am_status != AMEDIA_OK) {
        PJ_LOG(4, (THIS_FILE, "%s [0x%x] configure failed, status=%d",
               is_encoder?"Encoder":"Decoder", codec, am_status));
        return PJMEDIA_CODEC_EFAILED;
    }
    am_status = AMediaCodec_start(codec);
    if (am_status != AMEDIA_OK) {
	PJ_LOG(4, (THIS_FILE, "%s [0x%x] start failed, status=%d",
	       is_encoder?"Encoder":"Decoder", codec, am_status));
	return PJMEDIA_CODEC_EFAILED;
    }
    PJ_LOG(4, (THIS_FILE, "%s [0x%x] started", is_encoder?"Encoder":"Decoder",
	   codec));
    return PJ_SUCCESS;
}

/*
 * Initialize and register Android MediaCodec codec factory to pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_and_media_aud_init( pjmedia_endpt *endpt )
{
    pjmedia_codec_mgr *codec_mgr;
    pj_str_t codec_name;
    pj_status_t status;

    if (and_media_factory.pool != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    PJ_LOG(4, (THIS_FILE, "Initing codec"));

    /* Create Android MediaCodec codec factory. */
    and_media_factory.base.op = &and_media_factory_op;
    and_media_factory.base.factory_data = NULL;
    and_media_factory.endpt = endpt;

    and_media_factory.pool = pjmedia_endpt_create_pool(endpt,
                                                   "Android MediaCodec codecs",
                                                   4000, 4000);
    if (!and_media_factory.pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(and_media_factory.pool,
                                    "Android MediaCodec codecs",
				    &and_media_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	status = PJ_EINVALIDOP;
	goto on_error;
    }

#if PJMEDIA_HAS_AND_MEDIA_AMRNB
    PJ_LOG(4, (THIS_FILE, "Registering AMRNB codec"));

    pj_cstr(&codec_name, "AMR");
    status = pjmedia_sdp_neg_register_fmt_match_cb(
						&codec_name,
						&pjmedia_codec_amr_match_sdp);
    if (status != PJ_SUCCESS)
	goto on_error;
#endif

#if PJMEDIA_HAS_AND_MEDIA_AMRWB
    PJ_LOG(4, (THIS_FILE, "Registering AMRWB codec"));

    pj_cstr(&codec_name, "AMR-WB");
    status = pjmedia_sdp_neg_register_fmt_match_cb(
						&codec_name,
						&pjmedia_codec_amr_match_sdp);
    if (status != PJ_SUCCESS)
	goto on_error;
#endif

    /* Suppress compile warning */
    PJ_UNUSED_ARG(codec_name);

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr, 
						&and_media_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done. */
    return PJ_SUCCESS;

on_error:
    pj_pool_release(and_media_factory.pool);
    and_media_factory.pool = NULL;
    return status;
}

/*
 * Unregister Android MediaCodec codecs factory from pjmedia endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_codec_and_media_aud_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (and_media_factory.pool == NULL) {
	/* Already deinitialized */
	return PJ_SUCCESS;
    }

    pj_mutex_lock(and_media_factory.mutex);

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(and_media_factory.endpt);
    if (!codec_mgr) {
	pj_pool_release(and_media_factory.pool);
	and_media_factory.pool = NULL;
	pj_mutex_unlock(and_media_factory.mutex);
	return PJ_EINVALIDOP;
    }

    /* Unregister Android MediaCodec codecs factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &and_media_factory.base);

    /* Destroy mutex. */
    pj_mutex_unlock(and_media_factory.mutex);
    pj_mutex_destroy(and_media_factory.mutex);
    and_media_factory.mutex = NULL;

    /* Destroy pool. */
    pj_pool_release(and_media_factory.pool);
    and_media_factory.pool = NULL;

    return status;
}

/*
 * Check if factory can allocate the specified codec. 
 */
static pj_status_t and_media_test_alloc(pjmedia_codec_factory *factory,
					const pjmedia_codec_info *info )
{
    unsigned i;

    PJ_UNUSED_ARG(factory);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    for (i = 0; i < PJ_ARRAY_SIZE(and_media_codec); ++i) {
	pj_str_t name = pj_str((char*)and_media_codec[i].name);
	if ((pj_stricmp(&info->encoding_name, &name) == 0) &&
	    (info->clock_rate == (unsigned)and_media_codec[i].clock_rate) &&
	    (info->channel_cnt == (unsigned)and_media_codec[i].channel_count) &&
	    (and_media_codec[i].enabled))
	{
	    return PJ_SUCCESS;
	}
    }

    /* Unsupported, or mode is disabled. */
    return PJMEDIA_CODEC_EUNSUP;
}

/*
 * Generate default attribute.
 */
static pj_status_t and_media_default_attr (pjmedia_codec_factory *factory,
					   const pjmedia_codec_info *id,
					   pjmedia_codec_param *attr)
{
    unsigned i;

    PJ_ASSERT_RETURN(factory==&and_media_factory.base, PJ_EINVAL);

    pj_bzero(attr, sizeof(pjmedia_codec_param));

    for (i = 0; i < PJ_ARRAY_SIZE(and_media_codec); ++i) {
	pj_str_t name = pj_str((char*)and_media_codec[i].name);
	if ((and_media_codec[i].enabled) &&
	    (pj_stricmp(&id->encoding_name, &name) == 0) &&
	    (id->clock_rate == (unsigned)and_media_codec[i].clock_rate) &&
	    (id->channel_cnt == (unsigned)and_media_codec[i].channel_count) &&
	    (id->pt == (unsigned)and_media_codec[i].pt))
	{
	    attr->info.pt = (pj_uint8_t)id->pt;
	    attr->info.channel_cnt = and_media_codec[i].channel_count;
	    attr->info.clock_rate = and_media_codec[i].clock_rate;
	    attr->info.avg_bps = and_media_codec[i].def_bitrate;
	    attr->info.max_bps = and_media_codec[i].max_bitrate;
	    attr->info.pcm_bits_per_sample = 16;
	    attr->info.frm_ptime =  (pj_uint16_t)
				(and_media_codec[i].samples_per_frame * 1000 /
				and_media_codec[i].channel_count /
				and_media_codec[i].clock_rate);
	    attr->setting.frm_per_pkt = and_media_codec[i].frm_per_pkt;

	    /* Default flags. */
	    attr->setting.plc = 1;
	    attr->setting.penh= 0;
	    attr->setting.vad = 1;
	    attr->setting.cng = attr->setting.vad;
	    attr->setting.dec_fmtp = and_media_codec[i].dec_fmtp;

	    return PJ_SUCCESS;
	}
    }

    return PJMEDIA_CODEC_EUNSUP;
}

static pj_bool_t codec_exists(const pj_str_t *codec_name)
{
    AMediaCodec *codec;
    char *codec_txt;

    codec_txt = codec_name->ptr;

    codec = AMediaCodec_createCodecByName(codec_txt);
    if (!codec) {
	PJ_LOG(4, (THIS_FILE, "Failed creating codec : %.*s", codec_name->slen,
		   codec_name->ptr));
	return PJ_FALSE;
    }
    AMediaCodec_delete(codec);

    return PJ_TRUE;
}

/*
 * Enum codecs supported by this factory.
 */
static pj_status_t and_media_enum_codecs(pjmedia_codec_factory *factory,
					 unsigned *count,
					 pjmedia_codec_info codecs[])
{
    unsigned max;
    unsigned i;

    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    max = *count;

    for (i = 0, *count = 0; i < PJ_ARRAY_SIZE(and_media_codec) &&
         *count < max; ++i)
    {
	unsigned enc_idx, dec_idx;
	pj_str_t *enc_name = NULL;
	unsigned num_enc = 0;
	pj_str_t *dec_name = NULL;
	unsigned num_dec = 0;

	switch (and_media_codec[i].codec_id) {

	case AND_AUD_CODEC_AMRNB:
#if PJMEDIA_HAS_AND_MEDIA_AMRNB
	    enc_name = &AMRNB_encoder[0];
	    dec_name = &AMRNB_decoder[0];
	    num_enc = PJ_ARRAY_SIZE(AMRNB_encoder);
	    num_dec = PJ_ARRAY_SIZE(AMRNB_decoder);
#endif
	    break;
	case AND_AUD_CODEC_AMRWB:
#if PJMEDIA_HAS_AND_MEDIA_AMRWB
	    enc_name = &AMRWB_encoder[0];
	    dec_name = &AMRWB_decoder[0];
	    num_enc = PJ_ARRAY_SIZE(AMRWB_encoder);
	    num_dec = PJ_ARRAY_SIZE(AMRWB_decoder);
#endif

	    break;
	default:
	    continue;
	};
	if (!enc_name || !dec_name) {
	    continue;
	}

	for (enc_idx = 0; enc_idx < num_enc ;++enc_idx, ++enc_name) {
	    if (codec_exists(enc_name)) {
		break;
	    }
	}
	if (enc_idx == num_enc)
	    continue;

	for (dec_idx = 0; dec_idx < num_dec ;++dec_idx, ++dec_name) {
	    if (codec_exists(dec_name)) {
		break;
	    }
	}
	if (dec_idx == num_dec)
	    continue;

	and_media_codec[i].encoder_name = enc_name;
	and_media_codec[i].decoder_name = dec_name;
	pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
	codecs[*count].encoding_name = pj_str((char*)and_media_codec[i].name);
	codecs[*count].pt = and_media_codec[i].pt;
	codecs[*count].type = PJMEDIA_TYPE_AUDIO;
	codecs[*count].clock_rate = and_media_codec[i].clock_rate;
	codecs[*count].channel_cnt = and_media_codec[i].channel_count;
	and_media_codec[i].enabled = PJ_TRUE;
	PJ_LOG(4, (THIS_FILE, "Found encoder [%d]: %.*s and decoder: %.*s ",
		   *count, enc_name->slen, enc_name->ptr, dec_name->slen,
		   dec_name->ptr));
	++*count;
    }

    return PJ_SUCCESS;
}

static void create_codec(and_media_private_t *and_media_data)
{
    char const *enc_name =
		   and_media_codec[and_media_data->codec_idx].encoder_name->ptr;
    char const *dec_name =
		   and_media_codec[and_media_data->codec_idx].decoder_name->ptr;

    if (!and_media_data->enc) {
	and_media_data->enc = AMediaCodec_createCodecByName(enc_name);
	if (!and_media_data->enc) {
	    PJ_LOG(4, (THIS_FILE, "Failed creating encoder: %s", enc_name));
	}
	PJ_LOG(4, (THIS_FILE, "Done creating encoder: %s [0x%x]", enc_name,
	       and_media_data->enc));
    }

    if (!and_media_data->dec) {
	and_media_data->dec = AMediaCodec_createCodecByName(dec_name);
	if (!and_media_data->dec) {
	    PJ_LOG(4, (THIS_FILE, "Failed creating decoder: %s", dec_name));
	}
	PJ_LOG(4, (THIS_FILE, "Done creating decoder: %s [0x%x]", dec_name,
	       and_media_data->dec));
    }
}

/*
 * Allocate a new codec instance.
 */
static pj_status_t and_media_alloc_codec(pjmedia_codec_factory *factory,
					 const pjmedia_codec_info *id,
					 pjmedia_codec **p_codec)
{
    and_media_private_t *codec_data;
    pjmedia_codec *codec;
    int idx;
    pj_pool_t *pool;
    unsigned i;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &and_media_factory.base, PJ_EINVAL);

    pj_mutex_lock(and_media_factory.mutex);

    /* Find codec's index */
    idx = -1;
    for (i = 0; i < PJ_ARRAY_SIZE(and_media_codec); ++i) {
	pj_str_t name = pj_str((char*)and_media_codec[i].name);
	if ((pj_stricmp(&id->encoding_name, &name) == 0) &&
	    (id->clock_rate == (unsigned)and_media_codec[i].clock_rate) &&
	    (id->channel_cnt == (unsigned)and_media_codec[i].channel_count) &&
	    (and_media_codec[i].enabled))
	{
	    idx = i;
	    break;
	}
    }
    if (idx == -1) {
	*p_codec = NULL;
	pj_mutex_unlock(and_media_factory.mutex);
	return PJMEDIA_CODEC_EFAILED;
    }

    /* Create pool for codec instance */
    pool = pjmedia_endpt_create_pool(and_media_factory.endpt, "andmedaud%p",
                                     512, 512);
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    PJ_ASSERT_RETURN(codec != NULL, PJ_ENOMEM);
    codec->op = &and_media_op;
    codec->factory = factory;
    codec->codec_data = PJ_POOL_ZALLOC_T(pool, and_media_private_t);
    codec_data = (and_media_private_t*) codec->codec_data;

    /* Create PLC if codec has no internal PLC */
    if (!and_media_codec[idx].has_native_plc) {
	pj_status_t status;
	status = pjmedia_plc_create(pool, and_media_codec[idx].clock_rate,
				    and_media_codec[idx].samples_per_frame, 0,
				    &codec_data->plc);
	if (status != PJ_SUCCESS) {
	    goto on_error;
	}
    }

    /* Create silence detector if codec has no internal VAD */
    if (!and_media_codec[idx].has_native_vad) {
	pj_status_t status;
	status = pjmedia_silence_det_create(pool,
					and_media_codec[idx].clock_rate,
					and_media_codec[idx].samples_per_frame,
					&codec_data->vad);
	if (status != PJ_SUCCESS) {
	    goto on_error;
	}
    }

    codec_data->pool = pool;
    codec_data->codec_idx = idx;

    create_codec(codec_data);
    if (!codec_data->enc || !codec_data->dec) {
	goto on_error;
    }
    pj_mutex_unlock(and_media_factory.mutex);

    *p_codec = codec;
    return PJ_SUCCESS;

on_error:
    pj_mutex_unlock(and_media_factory.mutex);
    and_media_dealloc_codec(factory, codec);
    return PJMEDIA_CODEC_EFAILED;
}

/*
 * Free codec.
 */
static pj_status_t and_media_dealloc_codec(pjmedia_codec_factory *factory,
					   pjmedia_codec *codec )
{
    and_media_private_t *codec_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &and_media_factory.base, PJ_EINVAL);

    /* Close codec, if it's not closed. */
    codec_data = (and_media_private_t*) codec->codec_data;
    if (codec_data->enc) {
        AMediaCodec_stop(codec_data->enc);
        AMediaCodec_delete(codec_data->enc);
        codec_data->enc = NULL;
    }

    if (codec_data->dec) {
        AMediaCodec_stop(codec_data->dec);
        AMediaCodec_delete(codec_data->dec);
        codec_data->dec = NULL;
    }
    pj_pool_release(codec_data->pool);

    return PJ_SUCCESS;
}

/*
 * Init codec.
 */
static pj_status_t and_media_codec_init(pjmedia_codec *codec,
				        pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}

/*
 * Open codec.
 */
static pj_status_t and_media_codec_open(pjmedia_codec *codec,
					pjmedia_codec_param *attr)
{
    and_media_private_t *codec_data = (and_media_private_t*) codec->codec_data;
    struct and_media_codec *and_media_data =
					&and_media_codec[codec_data->codec_idx];
    pj_status_t status;

    PJ_ASSERT_RETURN(codec && attr, PJ_EINVAL);
    PJ_ASSERT_RETURN(codec_data != NULL, PJ_EINVALIDOP);

    PJ_LOG(5,(THIS_FILE, "Opening codec.."));

    codec_data->vad_enabled = (attr->setting.vad != 0);
    codec_data->plc_enabled = (attr->setting.plc != 0);
    and_media_data->clock_rate = attr->info.clock_rate;

#if PJMEDIA_HAS_AND_MEDIA_AMRNB
    if (and_media_data->codec_id == AND_AUD_CODEC_AMRNB ||
	and_media_data->codec_id == AND_AUD_CODEC_AMRWB)
    {
	amr_settings_t *s;
	pj_uint8_t octet_align = 0;
	pj_int8_t enc_mode;
	unsigned i;

	enc_mode = pjmedia_codec_amr_get_mode(attr->info.avg_bps);

	pj_assert(enc_mode >= 0 && enc_mode <= 8);

	/* Check AMR specific attributes */
	for (i = 0; i < attr->setting.dec_fmtp.cnt; ++i) {
	    /* octet-align, one of the parameters that must have same value
	     * in offer & answer (RFC 4867 Section 8.3.1). Just check fmtp
	     * in the decoder side, since it's value is guaranteed to fulfil
	     * above requirement (by SDP negotiator).
	     */
	    const pj_str_t STR_FMTP_OCTET_ALIGN = {(char *)"octet-align", 11};

	    if (pj_stricmp(&attr->setting.dec_fmtp.param[i].name,
			   &STR_FMTP_OCTET_ALIGN) == 0)
	    {
		octet_align=(pj_uint8_t)
			    pj_strtoul(&attr->setting.dec_fmtp.param[i].val);
		break;
	    }
	}
	for (i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
	    /* mode-set, encoding mode is chosen based on local default mode
	     * setting:
	     * - if local default mode is included in the mode-set, use it
	     * - otherwise, find the closest mode to local default mode;
	     *   if there are two closest modes, prefer to use the higher
	     *   one, e.g: local default mode is 4, the mode-set param
	     *   contains '2,3,5,6', then 5 will be chosen.
	     */
	    const pj_str_t STR_FMTP_MODE_SET = {(char *)"mode-set", 8};

	    if (pj_stricmp(&attr->setting.enc_fmtp.param[i].name,
			   &STR_FMTP_MODE_SET) == 0)
	    {
		const char *p;
		pj_size_t l;
		pj_int8_t diff = 99;

		p = pj_strbuf(&attr->setting.enc_fmtp.param[i].val);
		l = pj_strlen(&attr->setting.enc_fmtp.param[i].val);

		while (l--) {
		    if ((and_media_data->codec_id == AND_AUD_CODEC_AMRNB &&
			 *p>='0' && *p<='7') ||
		        (and_media_data->codec_id == AND_AUD_CODEC_AMRWB &&
		         *p>='0' && *p<='8'))
		    {
			pj_int8_t tmp = (pj_int8_t)(*p - '0' - enc_mode);

			if (PJ_ABS(diff) > PJ_ABS(tmp) ||
			    (PJ_ABS(diff) == PJ_ABS(tmp) && tmp > diff))
			{
			    diff = tmp;
			    if (diff == 0) break;
			}
		    }
		    ++p;
		}
		if (diff == 99)
		    goto on_error;

		enc_mode = (pj_int8_t)(enc_mode + diff);

		break;
	    }
	}
	/* Initialize AMR specific settings */
	s = PJ_POOL_ZALLOC_T(codec_data->pool, amr_settings_t);
	codec_data->codec_setting = s;

	s->enc_setting.amr_nb = (pj_uint8_t)
			      (and_media_data->codec_id == AND_AUD_CODEC_AMRNB);
	s->enc_setting.octet_aligned = octet_align;
	s->enc_setting.reorder = 0;
	s->enc_setting.cmr = 15;
	s->dec_setting.amr_nb = (pj_uint8_t)
			      (and_media_data->codec_id == AND_AUD_CODEC_AMRNB);
	s->dec_setting.octet_aligned = octet_align;
	s->dec_setting.reorder = 0;
	/* Apply encoder mode/bitrate */
	s->enc_mode = enc_mode;

	PJ_LOG(4, (THIS_FILE, "Encoder setting octet_aligned=%d reorder=%d"
		   " cmr=%d enc_mode=%d",
		   s->enc_setting.octet_aligned, s->enc_setting.reorder,
		   s->enc_setting.cmr, enc_mode));
	PJ_LOG(4, (THIS_FILE, "Decoder setting octet_aligned=%d reorder=%d",
		   s->dec_setting.octet_aligned, s->dec_setting.reorder));
    }
#endif
    status = configure_codec(codec_data, PJ_TRUE);
    if (status != PJ_SUCCESS) {
        goto on_error;
    }
    status = configure_codec(codec_data, PJ_FALSE);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    return PJ_SUCCESS;

on_error:
    return PJMEDIA_CODEC_EFAILED;
}

/*
 * Close codec.
 */
static pj_status_t and_media_codec_close(pjmedia_codec *codec)
{
    PJ_UNUSED_ARG(codec);

    return PJ_SUCCESS;
}

/*
 * Modify codec settings.
 */
static pj_status_t  and_media_codec_modify(pjmedia_codec *codec,
					   const pjmedia_codec_param *attr)
{
    and_media_private_t *codec_data = (and_media_private_t*) codec->codec_data;

    codec_data->vad_enabled = (attr->setting.vad != 0);
    codec_data->plc_enabled = (attr->setting.plc != 0);

    return PJ_SUCCESS;
}

/*
 * Get frames in the packet.
 */
static pj_status_t and_media_codec_parse(pjmedia_codec *codec,
					 void *pkt,
					 pj_size_t pkt_size,
					 const pj_timestamp *ts,
					 unsigned *frame_cnt,
					 pjmedia_frame frames[])
{
    and_media_private_t *codec_data = (and_media_private_t*) codec->codec_data;
    struct and_media_codec *and_media_data =
					&and_media_codec[codec_data->codec_idx];
    unsigned count = 0;

    PJ_ASSERT_RETURN(frame_cnt, PJ_EINVAL);

    if (and_media_data->parse != NULL) {
	return and_media_data->parse(codec_data, pkt,  pkt_size, ts, frame_cnt,
				     frames);
    }

    while (pkt_size >= codec_data->frame_size && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = codec_data->frame_size;
	frames[count].timestamp.u64 = ts->u64 +
				      count*and_media_data->samples_per_frame;
	pkt = ((char*)pkt) + codec_data->frame_size;
	pkt_size -= codec_data->frame_size;
	++count;
    }

    if (pkt_size && count < *frame_cnt) {
	frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
	frames[count].buf = pkt;
	frames[count].size = pkt_size;
	frames[count].timestamp.u64 = ts->u64 +
				      count*and_media_data->samples_per_frame;
	++count;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}

/*
 * Encode frames.
 */
static pj_status_t and_media_codec_encode(pjmedia_codec *codec,
					  const struct pjmedia_frame *input,
					  unsigned output_buf_len,
					  struct pjmedia_frame *output)
{
    and_media_private_t *codec_data = (and_media_private_t*) codec->codec_data;
    struct and_media_codec *and_media_data =
					&and_media_codec[codec_data->codec_idx];
    unsigned samples_per_frame;
    unsigned nsamples;
    unsigned nframes;
    pj_size_t tx = 0;
    pj_int16_t *pcm_in = (pj_int16_t*)input->buf;
    pj_uint8_t  *bits_out = (pj_uint8_t*) output->buf;
    pj_uint8_t pt;

    /* Invoke external VAD if codec has no internal VAD */
    if (codec_data->vad && codec_data->vad_enabled) {
	pj_bool_t is_silence;
	pj_int32_t silence_duration;

	silence_duration = pj_timestamp_diff32(&codec_data->last_tx, 
					       &input->timestamp);

	is_silence = pjmedia_silence_det_detect(codec_data->vad, 
					        (const pj_int16_t*) input->buf,
						(input->size >> 1),
						NULL);
	if (is_silence &&
	    (PJMEDIA_CODEC_MAX_SILENCE_PERIOD == -1 ||
	     silence_duration < (PJMEDIA_CODEC_MAX_SILENCE_PERIOD *
	 			 (int)and_media_data->clock_rate / 1000)))
	{
	    output->type = PJMEDIA_FRAME_TYPE_NONE;
	    output->buf = NULL;
	    output->size = 0;
	    output->timestamp = input->timestamp;
	    return PJ_SUCCESS;
	} else {
	    codec_data->last_tx = input->timestamp;
	}
    }
    nsamples = input->size >> 1;
    samples_per_frame = and_media_data->samples_per_frame;
    pt = and_media_data->pt;
    nframes = nsamples / samples_per_frame;

    PJ_ASSERT_RETURN(nsamples % samples_per_frame == 0, 
		     PJMEDIA_CODEC_EPCMFRMINLEN);

    /* Encode the frames */
    while (nsamples >= samples_per_frame) {
        pj_ssize_t buf_idx;
        unsigned i;
        pj_size_t output_size;
        pj_uint8_t *output_buf;
        AMediaCodecBufferInfo buf_info;

        buf_idx = AMediaCodec_dequeueInputBuffer(codec_data->enc,
					         CODEC_DEQUEUE_TIMEOUT);

        if (buf_idx >= 0) {
	    media_status_t am_status;
	    pj_size_t output_size;
            unsigned input_size = samples_per_frame << 1;

	    pj_uint8_t *input_buf = AMediaCodec_getInputBuffer(codec_data->enc,
						        buf_idx, &output_size);

	    if (input_buf && output_size >= input_size) {
	        pj_memcpy(input_buf, pcm_in, input_size);

	        am_status = AMediaCodec_queueInputBuffer(codec_data->enc,
				                  buf_idx, 0, input_size, 0, 0);
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
				         input_buf, output_size, input_size));
	        }
	        goto on_return;
	    }
        } else {
	    PJ_LOG(4,(THIS_FILE, "Encoder dequeueInputBuffer failed[%d]",
                      buf_idx));
	    goto on_return;
        }

        for (i = 0; i < CODEC_WAIT_RETRY; ++i) {
	    buf_idx = AMediaCodec_dequeueOutputBuffer(codec_data->enc,
						      &buf_info,
						      CODEC_DEQUEUE_TIMEOUT);
	    if (buf_idx == -1) {
	        /* Timeout, wait until output buffer is availble. */
	        pj_thread_sleep(CODEC_THREAD_WAIT);
	    } else {
	        break;
	    }
        }

        if (buf_idx < 0) {
	    PJ_LOG(4, (THIS_FILE, "Encoder dequeueOutputBuffer failed %d",
		   buf_idx));
            goto on_return;
        }

        output_buf = AMediaCodec_getOutputBuffer(codec_data->enc,
                                                 buf_idx,
                                                 &output_size);
        if (!output_buf) {
            PJ_LOG(4, (THIS_FILE, "Encoder failed getting output buffer, "
                       "buffer size=%d, flags %d",
                       buf_info.size, buf_info.flags));
            goto on_return;
        }

        pj_memcpy(bits_out, output_buf, buf_info.size);
        AMediaCodec_releaseOutputBuffer(codec_data->enc,
                                        buf_idx,
                                        0);
        bits_out += buf_info.size;
        tx += buf_info.size;
	pcm_in += samples_per_frame;
	nsamples -= samples_per_frame;
    }
    if (and_media_data->pack != NULL) {
	and_media_data->pack(codec_data, nframes, output->buf, &tx,
			     output_buf_len);
    }
    /* Check if we don't need to transmit the frame (DTX) */
    if (tx == 0) {
	output->buf = NULL;
	output->size = 0;
	output->timestamp.u64 = input->timestamp.u64;
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }
    output->size = tx;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;

on_return:
    output->size = 0;
    output->buf = NULL;
    output->type = PJMEDIA_FRAME_TYPE_NONE;
    output->timestamp.u64 = input->timestamp.u64;
    return PJ_SUCCESS;
}

/*
 * Decode frame.
 */
static pj_status_t and_media_codec_decode(pjmedia_codec *codec,
					  const struct pjmedia_frame *input,
					  unsigned output_buf_len,
					  struct pjmedia_frame *output)
{
    and_media_private_t *codec_data = (and_media_private_t*) codec->codec_data;
    struct and_media_codec *and_media_data =
					&and_media_codec[codec_data->codec_idx];
    unsigned samples_per_frame;
    unsigned i;

    pj_uint8_t pt;
    pj_ssize_t buf_idx = -1;
    pj_uint8_t *input_buf;
    pj_size_t input_size;
    pj_size_t output_size;
    media_status_t am_status;
    AMediaCodecBufferInfo buf_info;
    pj_uint8_t *output_buf;
    pjmedia_frame input_;

    pj_bzero(&input_, sizeof(pjmedia_frame));
    pt = and_media_data->pt;
    samples_per_frame = and_media_data->samples_per_frame;

    PJ_ASSERT_RETURN(output_buf_len >= samples_per_frame << 1,
		     PJMEDIA_CODEC_EPCMTOOSHORT);

    if (input->type != PJMEDIA_FRAME_TYPE_AUDIO)
    {
	goto on_return;
    }

    buf_idx = AMediaCodec_dequeueInputBuffer(codec_data->dec,
					     CODEC_DEQUEUE_TIMEOUT);

    if (buf_idx < 0) {
	PJ_LOG(4,(THIS_FILE, "Decoder dequeueInputBuffer failed return %d",
		  buf_idx));
	goto on_return;
    }

    input_buf = AMediaCodec_getInputBuffer(codec_data->dec,
					   buf_idx,
					   &input_size);
    if (input_buf == 0) {
	PJ_LOG(4,(THIS_FILE, "Decoder getInputBuffer failed "
		  "return input_buf=%d, size=%d", input_buf, input_size));
	goto on_return;
    }

    if (and_media_data->predecode) {
	input_.buf = input_buf;
	and_media_data->predecode(codec_data, input, &input_);
    } else {
	input_.size = input->size;
	pj_memcpy(input_buf, input->buf, input->size);
    }

    am_status = AMediaCodec_queueInputBuffer(codec_data->dec,
					     buf_idx,
					     0,
					     input_.size,
					     input->timestamp.u32.lo,
					     0);
    if (am_status != AMEDIA_OK) {
	PJ_LOG(4,(THIS_FILE, "Decoder queueInputBuffer failed return %d",
		  am_status));
	goto on_return;
    }

    for (i = 0; i < CODEC_WAIT_RETRY; ++i) {
	buf_idx = AMediaCodec_dequeueOutputBuffer(codec_data->dec,
						  &buf_info,
						  CODEC_DEQUEUE_TIMEOUT);
	if (buf_idx == -1) {
	    /* Timeout, wait until output buffer is availble. */
	    PJ_LOG(5, (THIS_FILE, "Decoder dequeueOutputBuffer timeout[%d]",
		       i+1));
	    pj_thread_sleep(CODEC_THREAD_WAIT);
	} else {
	    break;
	}
    }
    if (buf_idx < 0) {
	PJ_LOG(5, (THIS_FILE, "Decoder dequeueOutputBuffer failed [%d]",
		   buf_idx));
	goto on_return;
    }

    output_buf = AMediaCodec_getOutputBuffer(codec_data->dec,
					     buf_idx,
					     &output_size);
    if (output_buf == NULL) {
	am_status = AMediaCodec_releaseOutputBuffer(codec_data->dec,
					buf_idx,
					0);
	if (am_status != AMEDIA_OK) {
	    PJ_LOG(4,(THIS_FILE, "Decoder releaseOutputBuffer failed %d",
		      am_status));
	}
	PJ_LOG(4,(THIS_FILE, "Decoder getOutputBuffer failed"));
	goto on_return;
    }
    pj_memcpy(output->buf, output_buf, buf_info.size);
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = buf_info.size;
    output->timestamp.u64 = input->timestamp.u64;
    am_status = AMediaCodec_releaseOutputBuffer(codec_data->dec,
						buf_idx,
						0);

    /* Invoke external PLC if codec has no internal PLC */
    if (codec_data->plc && codec_data->plc_enabled)
	pjmedia_plc_save(codec_data->plc, (pj_int16_t*)output->buf);

    return PJ_SUCCESS;

on_return:
    pjmedia_zero_samples((pj_int16_t*)output->buf, samples_per_frame);
    output->size = samples_per_frame << 1;
    output->timestamp.u64 = input->timestamp.u64;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    return PJ_SUCCESS;
}

/* 
 * Recover lost frame.
 */
static pj_status_t  and_media_codec_recover(pjmedia_codec *codec,
					    unsigned output_buf_len,
					    struct pjmedia_frame *output)
{
    and_media_private_t *codec_data = (and_media_private_t*) codec->codec_data;
    struct and_media_codec *and_media_data =
					&and_media_codec[codec_data->codec_idx];
    unsigned samples_per_frame;
    pj_bool_t generate_plc = (codec_data->plc_enabled && codec_data->plc);

    PJ_UNUSED_ARG(output_buf_len);

    samples_per_frame = and_media_data->samples_per_frame;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->size = samples_per_frame << 1;

    if (generate_plc)
	pjmedia_plc_generate(codec_data->plc, (pj_int16_t*)output->buf);
    else
	pjmedia_zero_samples((pj_int16_t*)output->buf, samples_per_frame);

    return PJ_SUCCESS;
}


#endif	/* PJMEDIA_HAS_ANDROID_MEDIACODEC */


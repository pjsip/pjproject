/* $Id$ */
/*
 * Copyright (C) 2017 Teluu Inc. (http://www.teluu.com)
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

#include <pjmedia-codec/bcg729.h>
#include <pjmedia/codec.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/log.h>

#if defined(PJMEDIA_HAS_BCG729) && (PJMEDIA_HAS_BCG729!=0)

#include <bcg729/encoder.h>
#include <bcg729/decoder.h>

#define THIS_FILE		"bcg729.c"

/* Prototypes for BCG729 factory */
static pj_status_t bcg729_test_alloc(pjmedia_codec_factory *factory,
				     const pjmedia_codec_info *id);
static pj_status_t bcg729_default_attr(pjmedia_codec_factory *factory,
				       const pjmedia_codec_info *id,
				       pjmedia_codec_param *attr);
static pj_status_t bcg729_enum_codecs (pjmedia_codec_factory *factory,
				       unsigned *count,
				       pjmedia_codec_info codecs[]);
static pj_status_t bcg729_alloc_codec(pjmedia_codec_factory *factory,
				      const pjmedia_codec_info *id,
				      pjmedia_codec **p_codec);
static pj_status_t bcg729_dealloc_codec(pjmedia_codec_factory *factory,
				        pjmedia_codec *codec);

/* Prototypes for BCG729 implementation. */
static pj_status_t  bcg729_codec_init(pjmedia_codec *codec,
				      pj_pool_t *pool );
static pj_status_t  bcg729_codec_open(pjmedia_codec *codec,
				      pjmedia_codec_param *attr);
static pj_status_t  bcg729_codec_close(pjmedia_codec *codec);
static pj_status_t  bcg729_codec_modify(pjmedia_codec *codec,
				        const pjmedia_codec_param *attr);
static pj_status_t  bcg729_codec_parse(pjmedia_codec *codec,
				       void *pkt,
				       pj_size_t pkt_size,
				       const pj_timestamp *timestamp,
				       unsigned *frame_cnt,
				       pjmedia_frame frames[]);
static pj_status_t  bcg729_codec_encode(pjmedia_codec *codec,
				        const struct pjmedia_frame *input,
				        unsigned output_buf_len,
				        struct pjmedia_frame *output);
static pj_status_t  bcg729_codec_decode(pjmedia_codec *codec,
				        const struct pjmedia_frame *input,
				        unsigned output_buf_len,
				        struct pjmedia_frame *output);
static pj_status_t  bcg729_codec_recover(pjmedia_codec *codec,
					 unsigned output_buf_len,
					 struct pjmedia_frame *output);

/* Codec const. */
#define G729_TAG 		"G729"
#define G729_CLOCK_RATE 	8000
#define G729_CHANNEL_COUNT 	1
#define G729_SAMPLES_PER_FRAME	80
#define G729_DEFAULT_BIT_RATE	8000
#define G729_MAX_BIT_RATE	11800
#define G729_FRAME_PER_PACKET	2
#define G729_FRAME_SIZE		10

/* Definition for BCG729 codec operations. */
static pjmedia_codec_op bcg729_op =
{
    &bcg729_codec_init,
    &bcg729_codec_open,
    &bcg729_codec_close,
    &bcg729_codec_modify,
    &bcg729_codec_parse,
    &bcg729_codec_encode,
    &bcg729_codec_decode,
    &bcg729_codec_recover
};

/* Definition for BCG729 codec factory operations. */
static pjmedia_codec_factory_op bcg729_factory_op =
{
    &bcg729_test_alloc,
    &bcg729_default_attr,
    &bcg729_enum_codecs,
    &bcg729_alloc_codec,
    &bcg729_dealloc_codec,
    &pjmedia_codec_bcg729_deinit
};


/* BCG729 factory private data */
static struct bcg729_factory
{
    pjmedia_codec_factory	base;
    pjmedia_endpt	       *endpt;
    pj_pool_t		       *pool;
    pj_mutex_t		       *mutex;
} bcg729_factory;


/* BCG729 codec private data. */
typedef struct bcg729_private
{
    pj_pool_t	*pool;		/**< Pool for each instance.    */

    bcg729EncoderChannelContextStruct	*encoder;
    bcg729DecoderChannelContextStruct	*decoder;

    pj_bool_t		 vad_enabled;
    pj_bool_t		 plc_enabled;
} bcg729_private;


PJ_DEF(pj_status_t) pjmedia_codec_bcg729_init(pjmedia_endpt *endpt)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (bcg729_factory.endpt != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    /* Init factory */
    pj_bzero(&bcg729_factory, sizeof(bcg729_factory));
    bcg729_factory.base.op = &bcg729_factory_op;
    bcg729_factory.base.factory_data = NULL;
    bcg729_factory.endpt = endpt;

    /* Create pool */
    bcg729_factory.pool = pjmedia_endpt_create_pool(endpt, "bcg729", 4000,
						    4000);
    if (!bcg729_factory.pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(bcg729_factory.pool, "bcg729",
				    &bcg729_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr,
						&bcg729_factory.base);
    if (status != PJ_SUCCESS)
	goto on_error;

    PJ_LOG(4,(THIS_FILE, "BCG729 codec initialized"));
    return PJ_SUCCESS;

on_error:
    if (bcg729_factory.mutex) {
	pj_mutex_destroy(bcg729_factory.mutex);
	bcg729_factory.mutex = NULL;
    }
    if (bcg729_factory.pool) {
	pj_pool_release(bcg729_factory.pool);
	bcg729_factory.pool = NULL;
    }

    return status;
}

/*
 * Unregister BCG729 codec factory from pjmedia endpoint and deinitialize
 * the BCG729 codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_bcg729_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (bcg729_factory.endpt == NULL) {
	/* Not registered. */
	return PJ_SUCCESS;
    }

    /* Lock mutex. */
    pj_mutex_lock(bcg729_factory.mutex);

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(bcg729_factory.endpt);
    if (!codec_mgr) {
	bcg729_factory.endpt = NULL;
	pj_mutex_unlock(bcg729_factory.mutex);
	return PJ_EINVALIDOP;
    }

    /* Unregister bcg729 codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &bcg729_factory.base);
    bcg729_factory.endpt = NULL;

    /* Destroy mutex. */
    pj_mutex_unlock(bcg729_factory.mutex);
    pj_mutex_destroy(bcg729_factory.mutex);
    bcg729_factory.mutex = NULL;

    /* Release pool. */
    pj_pool_release(bcg729_factory.pool);
    bcg729_factory.pool = NULL;

    return status;
}


/*
 * Check if factory can allocate the specified codec.
 */
static pj_status_t bcg729_test_alloc(pjmedia_codec_factory *factory,
				     const pjmedia_codec_info *info )
{
    pj_str_t g729_tag = pj_str(G729_TAG);
    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(factory==&bcg729_factory.base, PJ_EINVAL);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check encoding name. */
    if (pj_stricmp(&info->encoding_name, &g729_tag) != 0)
	return PJMEDIA_CODEC_EUNSUP;

    /* Channel count must be one */
    if (info->channel_cnt != G729_CHANNEL_COUNT)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check clock-rate */
    if (info->clock_rate != G729_CLOCK_RATE)
	return PJMEDIA_CODEC_EUNSUP;	

    return PJ_SUCCESS;
}


/*
 * Generate default attribute.
 */
static pj_status_t bcg729_default_attr(pjmedia_codec_factory *factory,
				       const pjmedia_codec_info *id,
				       pjmedia_codec_param *attr )
{    
    PJ_ASSERT_RETURN(factory==&bcg729_factory.base, PJ_EINVAL);

    if (id->pt != PJMEDIA_RTP_PT_G729)
	return PJMEDIA_CODEC_EUNSUP;

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.pt = PJMEDIA_RTP_PT_G729;
    attr->info.channel_cnt = G729_CHANNEL_COUNT;
    attr->info.clock_rate = G729_CLOCK_RATE;
    attr->info.avg_bps = G729_DEFAULT_BIT_RATE;
    attr->info.max_bps = G729_MAX_BIT_RATE;

    attr->info.pcm_bits_per_sample = G729_FRAME_PER_PACKET * 8;
    attr->info.frm_ptime =  (pj_uint16_t)
			    (G729_SAMPLES_PER_FRAME * 1000 /
			     G729_CHANNEL_COUNT /
			     G729_CLOCK_RATE);
    attr->setting.frm_per_pkt = G729_FRAME_PER_PACKET;

    /* Default flags. */
    attr->setting.plc = 1;
    attr->setting.penh= 0;
    attr->setting.vad = 1;
    attr->setting.cng = attr->setting.vad;

    if (attr->setting.vad == 0) {
	attr->setting.dec_fmtp.cnt = 1;
	pj_strset2(&attr->setting.dec_fmtp.param[0].name, "annexb");
	pj_strset2(&attr->setting.dec_fmtp.param[0].val, "no");
    }
    return PJ_SUCCESS;
}


/*
 * Enum codecs supported by this factory.
 */
static pj_status_t bcg729_enum_codecs(pjmedia_codec_factory *factory,
				    unsigned *count,
				    pjmedia_codec_info codecs[])
{
    PJ_ASSERT_RETURN(factory==&bcg729_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    pj_bzero(&codecs[0], sizeof(pjmedia_codec_info));
    codecs[0].encoding_name = pj_str(G729_TAG);
    codecs[0].pt = PJMEDIA_RTP_PT_G729;
    codecs[0].type = PJMEDIA_TYPE_AUDIO;
    codecs[0].clock_rate = G729_CLOCK_RATE;
    codecs[0].channel_cnt = G729_CHANNEL_COUNT;

    *count = 1;

    return PJ_SUCCESS;
}


/*
 * Allocate a new BCG729 codec instance.
 */
static pj_status_t bcg729_alloc_codec(pjmedia_codec_factory *factory,
				      const pjmedia_codec_info *id,
				      pjmedia_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_codec *codec;
    bcg729_private *bcg729_data;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &bcg729_factory.base, PJ_EINVAL);

    pj_mutex_lock(bcg729_factory.mutex);

    /* Create pool for codec instance */
    pool = pjmedia_endpt_create_pool(bcg729_factory.endpt, "bcg729", 512, 512);

    /* Allocate codec */
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    codec->op = &bcg729_op;
    codec->factory = factory;
    codec->codec_data = PJ_POOL_ZALLOC_T(pool, bcg729_private);
    bcg729_data = (bcg729_private*) codec->codec_data;
    bcg729_data->pool = pool;

    pj_mutex_unlock(bcg729_factory.mutex);

    *p_codec = codec;
    return PJ_SUCCESS;
}


/*
 * Free codec.
 */
static pj_status_t bcg729_dealloc_codec(pjmedia_codec_factory *factory,
				        pjmedia_codec *codec )
{
    bcg729_private *bcg729_data;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &bcg729_factory.base, PJ_EINVAL);

    bcg729_data = (bcg729_private*)codec->codec_data;

    /* Close codec, if it's not closed. */
    if (bcg729_data->encoder || bcg729_data->decoder) {
    	bcg729_codec_close(codec);
    }

    pj_pool_release(bcg729_data->pool);

    return PJ_SUCCESS;
}


/*
 * Init codec.
 */
static pj_status_t bcg729_codec_init(pjmedia_codec *codec,
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}


/*
 * Open codec.
 */
static pj_status_t bcg729_codec_open(pjmedia_codec *codec,
				     pjmedia_codec_param *attr )
{
    bcg729_private *bcg729_data;
    unsigned i;

    PJ_ASSERT_RETURN(codec && attr, PJ_EINVAL);

    bcg729_data = (bcg729_private*) codec->codec_data;

    /* Already open? */
    if (bcg729_data->encoder && bcg729_data->decoder)
	return PJ_SUCCESS;

    bcg729_data->vad_enabled = (attr->setting.vad != 0);
    bcg729_data->plc_enabled = (attr->setting.plc != 0);

    /* Check if G729 Annex B is signaled to be disabled */
    for (i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
	if (pj_stricmp2(&attr->setting.enc_fmtp.param[i].name, "annexb")==0)
	{
	    if (pj_stricmp2(&attr->setting.enc_fmtp.param[i].val, "no")==0)
	    {
		attr->setting.vad = 0;
		bcg729_data->vad_enabled = PJ_FALSE;
	    }
	    break;
	}
    }

    bcg729_data->encoder = initBcg729EncoderChannel(
						 bcg729_data->vad_enabled?1:0);
    if (!bcg729_data->encoder)
        return PJMEDIA_CODEC_EFAILED;

    bcg729_data->decoder = initBcg729DecoderChannel();
    if (!bcg729_data->decoder)
        return PJMEDIA_CODEC_EFAILED;

    return PJ_SUCCESS;
}


/*
 * Close codec.
 */
static pj_status_t bcg729_codec_close( pjmedia_codec *codec )
{
    bcg729_private *bcg729_data;

    PJ_ASSERT_RETURN(codec, PJ_EINVAL);

    bcg729_data = (bcg729_private *)codec->codec_data;

    if (bcg729_data->encoder) {
        closeBcg729EncoderChannel(bcg729_data->encoder);
        bcg729_data->encoder = NULL;
    }
    if (bcg729_data->decoder) {
        closeBcg729DecoderChannel(bcg729_data->decoder);
        bcg729_data->decoder = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t  bcg729_codec_parse(pjmedia_codec *codec,
				       void *pkt,
				       pj_size_t pkt_size,
				       const pj_timestamp *ts,
				       unsigned *frame_cnt,
				       pjmedia_frame frames[])
{
    unsigned count = 0;
    PJ_UNUSED_ARG(codec);

    PJ_ASSERT_RETURN(codec && ts && frames && frame_cnt, PJ_EINVAL);

    while (pkt_size >= G729_FRAME_SIZE && count < *frame_cnt) {
        frames[count].type = PJMEDIA_FRAME_TYPE_AUDIO;
        frames[count].buf = pkt;
        frames[count].size = G729_FRAME_SIZE;
        frames[count].timestamp.u64 = ts->u64 + count * G729_SAMPLES_PER_FRAME;

        pkt = ((char*)pkt) + 10;
        pkt_size -= 10;

        ++count;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  bcg729_codec_modify(pjmedia_codec *codec,
				        const pjmedia_codec_param *attr )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(attr);

    return PJ_SUCCESS;
}


/*
 * Encode frame.
 */
static pj_status_t bcg729_codec_encode(pjmedia_codec *codec,
				       const struct pjmedia_frame *input,
				       unsigned output_buf_len,
				       struct pjmedia_frame *output)
{
    bcg729_private *bcg729_data;
    pj_int16_t *pcm_in;
    unsigned nsamples;    
    pj_uint8_t stream_len;

    PJ_ASSERT_RETURN(codec && input && output_buf_len && output, PJ_EINVAL);

    bcg729_data = (bcg729_private*)codec->codec_data;
    pcm_in   = (pj_int16_t*)input->buf;

    /* Check frame in size */
    nsamples = input->size >> 1;
    output->size = 0;

    /* Encode */
    while (nsamples >= G729_SAMPLES_PER_FRAME) {
        bcg729Encoder(bcg729_data->encoder, pcm_in,
                      (unsigned char*)output->buf + output->size, &stream_len);

	pcm_in += G729_SAMPLES_PER_FRAME;
	nsamples -= G729_SAMPLES_PER_FRAME;

	if (stream_len == 0) {
	    /* Untransmitted */
	    break;
	} else {
	    output->size += stream_len;
	    if (stream_len == 2) {
		/* SID */
		break;
            }
	}
    }
    
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}


static pj_status_t bcg729_codec_decode(pjmedia_codec *codec,
				       const struct pjmedia_frame *input,
				       unsigned output_buf_len,
				       struct pjmedia_frame *output)
{
    bcg729_private *bcg729_data;

    PJ_ASSERT_RETURN(codec && input && output_buf_len && output, PJ_EINVAL);

    bcg729_data = (struct bcg729_private*) codec->codec_data;

    bcg729Decoder(bcg729_data->decoder,
                  (unsigned char*)input->buf,
                  G729_FRAME_SIZE,
                  0,
                  0,
                  0,
                  (short*)output->buf);

    output->size = G729_SAMPLES_PER_FRAME * G729_FRAME_PER_PACKET;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}


/*
 * Recover lost frame.
 */
static pj_status_t  bcg729_codec_recover(pjmedia_codec *codec,
				         unsigned output_buf_len,
				         struct pjmedia_frame *output)
{
    bcg729_private *bcg729_data;

    PJ_ASSERT_RETURN(codec && output_buf_len && output, PJ_EINVAL);

    bcg729_data = (struct bcg729_private*) codec->codec_data;

    output->size = G729_SAMPLES_PER_FRAME * G729_FRAME_PER_PACKET;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;

    if (bcg729_data->plc_enabled) {
	bcg729Decoder(bcg729_data->decoder,
		      NULL,
		      G729_FRAME_SIZE,
		      1,
		      0,
		      0,
		      (short*)output->buf);
    } else {
	pjmedia_zero_samples((pj_int16_t*)output->buf, G729_SAMPLES_PER_FRAME);
    }

    return PJ_SUCCESS;
}

#endif /* PJMEDIA_HAS_BCG729 */

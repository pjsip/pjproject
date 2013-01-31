/* $Id$ */
/* 
 * Copyright (C) 2012-2012 Teluu Inc. (http://www.teluu.com)
 * Contributed by Regis Montoya (aka r3gis - www.r3gis.fr)
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

#include <pjmedia-codec/silk.h>
#include <pjmedia/codec.h>
#include <pjmedia/delaybuf.h>
#include <pjmedia/endpoint.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/log.h>

#if defined(PJMEDIA_HAS_SILK_CODEC) && (PJMEDIA_HAS_SILK_CODEC!=0)

#include "SKP_Silk_SDK_API.h"

#define THIS_FILE		"silk.c"

#ifndef PJMEDIA_SILK_DELAY_BUF_OPTIONS
    #define PJMEDIA_SILK_DELAY_BUF_OPTIONS PJMEDIA_DELAY_BUF_SIMPLE_FIFO
#endif

#define FRAME_LENGTH_MS                 20
#define SILK_ENC_CTL_PACKET_LOSS_PCT    10
#define SILK_MIN_BITRATE                5000
#define CALC_BITRATE_QUALITY(quality, max_br) \
                (quality * max_br / 10)
#define CALC_BITRATE(max_br) \
                CALC_BITRATE_QUALITY(PJMEDIA_CODEC_SILK_DEFAULT_QUALITY, \
                                     max_br);


/* Prototypes for SILK factory */
static pj_status_t silk_test_alloc( pjmedia_codec_factory *factory,
				    const pjmedia_codec_info *id );
static pj_status_t silk_default_attr( pjmedia_codec_factory *factory,
				      const pjmedia_codec_info *id,
				      pjmedia_codec_param *attr );
static pj_status_t silk_enum_codecs ( pjmedia_codec_factory *factory,
				      unsigned *count,
				      pjmedia_codec_info codecs[]);
static pj_status_t silk_alloc_codec( pjmedia_codec_factory *factory,
				     const pjmedia_codec_info *id,
				     pjmedia_codec **p_codec);
static pj_status_t silk_dealloc_codec( pjmedia_codec_factory *factory,
				       pjmedia_codec *codec );

/* Prototypes for SILK implementation. */
static pj_status_t  silk_codec_init( pjmedia_codec *codec,
				     pj_pool_t *pool );
static pj_status_t  silk_codec_open( pjmedia_codec *codec,
				     pjmedia_codec_param *attr );
static pj_status_t  silk_codec_close( pjmedia_codec *codec );
static pj_status_t  silk_codec_modify( pjmedia_codec *codec,
				       const pjmedia_codec_param *attr );
static pj_status_t  silk_codec_parse( pjmedia_codec *codec,
				      void *pkt,
				      pj_size_t pkt_size,
				      const pj_timestamp *timestamp,
				      unsigned *frame_cnt,
				      pjmedia_frame frames[]);
static pj_status_t  silk_codec_encode( pjmedia_codec *codec,
				       const struct pjmedia_frame *input,
				       unsigned output_buf_len,
				       struct pjmedia_frame *output);
static pj_status_t  silk_codec_decode( pjmedia_codec *codec,
				       const struct pjmedia_frame *input,
				       unsigned output_buf_len,
				       struct pjmedia_frame *output);
static pj_status_t  silk_codec_recover( pjmedia_codec *codec,
					unsigned output_buf_len,
					struct pjmedia_frame *output);


typedef enum
{
    PARAM_NB,   /* Index for narrowband parameter.	*/
    PARAM_MB,	/* Index for medium parameter.		*/
    PARAM_WB,	/* Index for wideband parameter.	*/
    PARAM_SWB,	/* Index for super-wideband parameter	*/
} silk_mode;


/* Silk default parameter */
typedef struct silk_param
{
    int		 enabled;	    /* Is this mode enabled?		    */
    int		 pt;		    /* Payload type.			    */
    unsigned	 clock_rate;	    /* Default sampling rate to be used.    */
    pj_uint16_t	 ptime;		    /* packet length (in ms).		    */
    pj_uint32_t  bitrate;	    /* Bit rate for current mode.	    */
    pj_uint32_t  max_bitrate;	    /* Max bit rate for current mode.	    */
    int 	 complexity;	    /* Complexity mode: 0/lowest to 2.	    */
} silk_param;


/* Definition for SILK codec operations. */
static pjmedia_codec_op silk_op =
{
    &silk_codec_init,
    &silk_codec_open,
    &silk_codec_close,
    &silk_codec_modify,
    &silk_codec_parse,
    &silk_codec_encode,
    &silk_codec_decode,
    &silk_codec_recover
};

/* Definition for SILK codec factory operations. */
static pjmedia_codec_factory_op silk_factory_op =
{
    &silk_test_alloc,
    &silk_default_attr,
    &silk_enum_codecs,
    &silk_alloc_codec,
    &silk_dealloc_codec,
    &pjmedia_codec_silk_deinit
};


/* SILK factory private data */
static struct silk_factory
{
    pjmedia_codec_factory	base;
    pjmedia_endpt	       *endpt;
    pj_pool_t		       *pool;
    pj_mutex_t		       *mutex;
    struct silk_param		silk_param[4];
} silk_factory;


/* SILK codec private data. */
typedef struct silk_private
{
    silk_mode	 mode;		/**< Silk mode.	*/
    pj_pool_t	*pool;		/**< Pool for each instance.    */
    unsigned	 samples_per_frame;
    pj_uint8_t   pcm_bytes_per_sample;

    pj_bool_t	 enc_ready;
    SKP_SILK_SDK_EncControlStruct enc_ctl;
    void	*enc_st;

    pj_bool_t	 dec_ready;
    SKP_SILK_SDK_DecControlStruct dec_ctl;
    void	*dec_st;

    /* Buffer to hold decoded frames. */
    void        *dec_buf[SILK_MAX_FRAMES_PER_PACKET-1];
    SKP_int16    dec_buf_size[SILK_MAX_FRAMES_PER_PACKET-1];
    pj_size_t    dec_buf_sz;
    unsigned     dec_buf_cnt;
    pj_uint32_t  pkt_info;    /**< Packet info for buffered frames.  */
} silk_private;


silk_mode silk_get_mode_from_clock_rate(unsigned clock_rate) {
    if (clock_rate <= silk_factory.silk_param[PARAM_NB].clock_rate) {
	return PARAM_NB;
    } else if (clock_rate <= silk_factory.silk_param[PARAM_MB].clock_rate) {
	return PARAM_MB;
    } else if (clock_rate <= silk_factory.silk_param[PARAM_WB].clock_rate) {
	return PARAM_WB;
    }
    return PARAM_SWB;
}


PJ_DEF(pj_status_t) pjmedia_codec_silk_init(pjmedia_endpt *endpt)
{
    pjmedia_codec_mgr *codec_mgr;
    silk_param *sp;
    pj_status_t status;

    if (silk_factory.endpt != NULL) {
	/* Already initialized. */
	return PJ_SUCCESS;
    }

    /* Init factory */
    pj_bzero(&silk_factory, sizeof(silk_factory));
    silk_factory.base.op = &silk_factory_op;
    silk_factory.base.factory_data = NULL;
    silk_factory.endpt = endpt;

    /* Create pool */
    silk_factory.pool = pjmedia_endpt_create_pool(endpt, "silk", 4000, 4000);
    if (!silk_factory.pool)
	return PJ_ENOMEM;

    /* Create mutex. */
    status = pj_mutex_create_simple(silk_factory.pool, "silk",
				    &silk_factory.mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Initialize default codec params */

    /* From SILK docs:
       - SILK bitrate tables:
         +----------------+---------+-----------+
         |                | fs (Hz) | BR (kbps) |
         +----------------+---------+-----------+
         |   Narrowband   |   8000  |   6 - 20  |
         |   Mediumband   |  12000  |   7 - 25  |
         |    Wideband    |  16000  |   8 - 30  |
         | Super Wideband |  24000  |  12 - 40  |
         +----------------+---------+-----------+
       - The upper limits of the bit rate ranges in this table are
         recommended values.
     */

    sp = &silk_factory.silk_param[PARAM_NB];
    sp->pt = PJMEDIA_RTP_PT_SILK_NB;
    sp->clock_rate = 8000;
    sp->max_bitrate = 22000;
    sp->bitrate = CALC_BITRATE(sp->max_bitrate);
    sp->ptime = FRAME_LENGTH_MS;
    sp->complexity = PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY;
    sp->enabled = 1;

    sp = &silk_factory.silk_param[PARAM_MB];
    sp->pt = PJMEDIA_RTP_PT_SILK_MB;
    sp->clock_rate = 12000;
    sp->max_bitrate = 28000;
    sp->bitrate = CALC_BITRATE(sp->max_bitrate);
    sp->ptime = FRAME_LENGTH_MS;
    sp->complexity = PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY;
    sp->enabled = 0;

    sp = &silk_factory.silk_param[PARAM_WB];
    sp->pt = PJMEDIA_RTP_PT_SILK_WB;
    sp->clock_rate = 16000;
    sp->max_bitrate = 36000;
    sp->bitrate = CALC_BITRATE(sp->max_bitrate);
    sp->ptime = FRAME_LENGTH_MS;
    sp->complexity = PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY;
    sp->enabled = 1;

    sp = &silk_factory.silk_param[PARAM_SWB];
    sp->pt = PJMEDIA_RTP_PT_SILK_SWB;
    sp->clock_rate = 24000;
    sp->max_bitrate = 46000;
    sp->bitrate = CALC_BITRATE(sp->max_bitrate);
    sp->ptime = FRAME_LENGTH_MS;
    sp->complexity = PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY;
    sp->enabled = 0;


    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(endpt);
    if (!codec_mgr) {
	return PJ_EINVALIDOP;
    }

    /* Register codec factory to endpoint. */
    status = pjmedia_codec_mgr_register_factory(codec_mgr,
						&silk_factory.base);
    if (status != PJ_SUCCESS)
	return status;

    PJ_LOG(4,(THIS_FILE, "SILK codec version %s initialized",
	      SKP_Silk_SDK_get_version()));
    return PJ_SUCCESS;

on_error:
    if (silk_factory.mutex) {
	pj_mutex_destroy(silk_factory.mutex);
	silk_factory.mutex = NULL;
    }
    if (silk_factory.pool) {
	pj_pool_release(silk_factory.pool);
	silk_factory.pool = NULL;
    }

    return status;
}


/*
 * Change the configuration setting of the SILK codec for the specified
 * clock rate.
 */
PJ_DEF(pj_status_t) pjmedia_codec_silk_set_config(
				    unsigned clock_rate, 
				    const pjmedia_codec_silk_setting *opt)
{
    unsigned i;

    /* Look up in factory modes table */
    for (i = 0; i < sizeof(silk_factory.silk_param)/
                    sizeof(silk_factory.silk_param[0]); ++i)
    {
        if (silk_factory.silk_param[i].clock_rate == clock_rate) {
            int quality = PJMEDIA_CODEC_SILK_DEFAULT_QUALITY;
            int complexity = PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY;

	    silk_factory.silk_param[i].enabled = opt->enabled;
            if (opt->complexity >= 0)
                complexity = opt->complexity;
            silk_factory.silk_param[i].complexity = complexity;
            if (opt->quality >= 0)
                quality = opt->quality;
            silk_factory.silk_param[i].bitrate =
                CALC_BITRATE_QUALITY(quality,
                                     silk_factory.silk_param[i].max_bitrate);
            if (silk_factory.silk_param[i].bitrate < SILK_MIN_BITRATE)
                silk_factory.silk_param[i].bitrate = SILK_MIN_BITRATE;

	    return PJ_SUCCESS;
	}
    }

    return PJ_ENOTFOUND;
}


/*
 * Unregister SILK codec factory from pjmedia endpoint and deinitialize
 * the SILK codec library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_silk_deinit(void)
{
    pjmedia_codec_mgr *codec_mgr;
    pj_status_t status;

    if (silk_factory.endpt == NULL) {
	/* Not registered. */
	return PJ_SUCCESS;
    }

    /* Lock mutex. */
    pj_mutex_lock(silk_factory.mutex);

    /* Get the codec manager. */
    codec_mgr = pjmedia_endpt_get_codec_mgr(silk_factory.endpt);
    if (!codec_mgr) {
	silk_factory.endpt = NULL;
	pj_mutex_unlock(silk_factory.mutex);
	return PJ_EINVALIDOP;
    }

    /* Unregister silk codec factory. */
    status = pjmedia_codec_mgr_unregister_factory(codec_mgr,
						  &silk_factory.base);
    silk_factory.endpt = NULL;

    /* Destroy mutex. */
    pj_mutex_destroy(silk_factory.mutex);
    silk_factory.mutex = NULL;


    /* Release pool. */
    pj_pool_release(silk_factory.pool);
    silk_factory.pool = NULL;

    return status;
}


/*
 * Check if factory can allocate the specified codec.
 */
static pj_status_t silk_test_alloc(pjmedia_codec_factory *factory,
				   const pjmedia_codec_info *info )
{
    const pj_str_t silk_tag = {"SILK", 4};
    unsigned i;

    PJ_UNUSED_ARG(factory);
    PJ_ASSERT_RETURN(factory==&silk_factory.base, PJ_EINVAL);

    /* Type MUST be audio. */
    if (info->type != PJMEDIA_TYPE_AUDIO)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check encoding name. */
    if (pj_stricmp(&info->encoding_name, &silk_tag) != 0)
	return PJMEDIA_CODEC_EUNSUP;

    /* Channel count must be one */
    if (info->channel_cnt != 1)
	return PJMEDIA_CODEC_EUNSUP;

    /* Check clock-rate */
    for (i=0; i<PJ_ARRAY_SIZE(silk_factory.silk_param); ++i) {
	silk_param *sp = &silk_factory.silk_param[i];
	if (sp->enabled && info->clock_rate == sp->clock_rate)
	{
	    return PJ_SUCCESS;
	}
    }
    /* Clock rate not supported */
    return PJMEDIA_CODEC_EUNSUP;
}


/*
 * Generate default attribute.
 */
static pj_status_t silk_default_attr( pjmedia_codec_factory *factory,
				      const pjmedia_codec_info *id,
				      pjmedia_codec_param *attr )
{
    silk_param *sp;
    int i;
    
    PJ_ASSERT_RETURN(factory==&silk_factory.base, PJ_EINVAL);
    
    i = silk_get_mode_from_clock_rate(id->clock_rate);
    pj_assert(i >= PARAM_NB && i <= PARAM_SWB);

    sp = &silk_factory.silk_param[i];

    pj_bzero(attr, sizeof(pjmedia_codec_param));
    attr->info.channel_cnt = 1;
    attr->info.clock_rate = sp->clock_rate;
    attr->info.avg_bps = sp->bitrate;
    attr->info.max_bps = sp->max_bitrate;
    attr->info.frm_ptime = sp->ptime;
    attr->info.pcm_bits_per_sample = 16;
    attr->info.pt = (pj_uint8_t) sp->pt;
    attr->setting.frm_per_pkt = 1;
    attr->setting.vad = 0; /* DTX is not recommended for quality reason */
    attr->setting.plc = 1;

    i = 0;
    attr->setting.dec_fmtp.param[i].name = pj_str("useinbandfec");
    attr->setting.dec_fmtp.param[i++].val = pj_str("0");
    /*
    attr->setting.dec_fmtp.param[i].name = pj_str("maxaveragebitrate");
    attr->setting.dec_fmtp.param[i++].val = pj_str(mode->bitrate_str);
    */
    attr->setting.dec_fmtp.cnt = (pj_uint8_t)i;

    return PJ_SUCCESS;
}


/*
 * Enum codecs supported by this factory.
 */
static pj_status_t silk_enum_codecs(pjmedia_codec_factory *factory,
				    unsigned *count,
				    pjmedia_codec_info codecs[])
{
    unsigned max;
    int i;

    PJ_ASSERT_RETURN(factory==&silk_factory.base, PJ_EINVAL);
    PJ_ASSERT_RETURN(codecs && *count > 0, PJ_EINVAL);

    max = *count;
    *count = 0;

    for (i = 0; i<PJ_ARRAY_SIZE(silk_factory.silk_param) && *count<max; ++i)
    {
	silk_param *sp = &silk_factory.silk_param[i];

    	if (!sp->enabled)
    	    continue;

    	pj_bzero(&codecs[*count], sizeof(pjmedia_codec_info));
    	codecs[*count].encoding_name = pj_str("SILK");
    	codecs[*count].pt = sp->pt;
    	codecs[*count].type = PJMEDIA_TYPE_AUDIO;
    	codecs[*count].clock_rate = sp->clock_rate;
    	codecs[*count].channel_cnt = 1;

    	++*count;
    }

    return PJ_SUCCESS;
}


/*
 * Allocate a new SILK codec instance.
 */
static pj_status_t silk_alloc_codec(pjmedia_codec_factory *factory,
				    const pjmedia_codec_info *id,
				    pjmedia_codec **p_codec)
{
    pj_pool_t *pool;
    pjmedia_codec *codec;
    silk_private *silk;

    PJ_ASSERT_RETURN(factory && id && p_codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &silk_factory.base, PJ_EINVAL);

    /* Create pool for codec instance */
    pool = pjmedia_endpt_create_pool(silk_factory.endpt, "silk", 512, 512);

    /* Allocate codec */
    codec = PJ_POOL_ZALLOC_T(pool, pjmedia_codec);
    codec->op = &silk_op;
    codec->factory = factory;
    codec->codec_data = PJ_POOL_ZALLOC_T(pool, silk_private);
    silk = (silk_private*) codec->codec_data;
    silk->pool = pool;
    silk->enc_ready = PJ_FALSE;
    silk->dec_ready = PJ_FALSE;
    silk->mode = silk_get_mode_from_clock_rate(id->clock_rate);

    *p_codec = codec;
    return PJ_SUCCESS;
}


/*
 * Free codec.
 */
static pj_status_t silk_dealloc_codec( pjmedia_codec_factory *factory,
				      pjmedia_codec *codec )
{
    silk_private *silk;

    PJ_ASSERT_RETURN(factory && codec, PJ_EINVAL);
    PJ_ASSERT_RETURN(factory == &silk_factory.base, PJ_EINVAL);

    silk = (silk_private*)codec->codec_data;

    /* Close codec, if it's not closed. */
    if (silk->enc_ready == PJ_TRUE || silk->dec_ready == PJ_TRUE) {
    	silk_codec_close(codec);
    }

    pj_pool_release(silk->pool);

    return PJ_SUCCESS;
}


/*
 * Init codec.
 */
static pj_status_t silk_codec_init(pjmedia_codec *codec,
				   pj_pool_t *pool )
{
    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(pool);
    return PJ_SUCCESS;
}


/*
 * Open codec.
 */
static pj_status_t silk_codec_open(pjmedia_codec *codec,
				   pjmedia_codec_param *attr )
{

    silk_private *silk;
    silk_param *sp;
    SKP_int st_size, err;
    pj_bool_t enc_use_fec;
    unsigned enc_bitrate, i;

    PJ_ASSERT_RETURN(codec && attr, PJ_EINVAL);

    silk = (silk_private*)codec->codec_data;
    sp = &silk_factory.silk_param[silk->mode];

    /* Already opened? */
    if (silk->enc_ready || silk->dec_ready)
	return PJ_SUCCESS;

    /* Allocate and initialize encoder */
    err = SKP_Silk_SDK_Get_Encoder_Size(&st_size);
    if (err) {
        PJ_LOG(3,(THIS_FILE, "Failed to get encoder state size (err=%d)",
		  err));
	return PJMEDIA_CODEC_EFAILED;
    }
    silk->enc_st = pj_pool_zalloc(silk->pool, st_size);
    err = SKP_Silk_SDK_InitEncoder(silk->enc_st, &silk->enc_ctl);
    if (err) {
        PJ_LOG(3,(THIS_FILE, "Failed to init encoder (err=%d)", err));
	return PJMEDIA_CODEC_EFAILED;
    }

    /* Check fmtp params */
    enc_use_fec = PJ_TRUE;
    enc_bitrate = sp->bitrate;
    for (i = 0; i < attr->setting.enc_fmtp.cnt; ++i) {
	pjmedia_codec_fmtp *fmtp = &attr->setting.enc_fmtp;
	const pj_str_t STR_USEINBANDFEC = {"useinbandfec", 12};
	const pj_str_t STR_MAXAVERAGEBITRATE = {"maxaveragebitrate", 17};

	if (!pj_stricmp(&fmtp->param[i].name, &STR_USEINBANDFEC)) {
	    enc_use_fec = pj_strtoul(&fmtp->param[i].val) != 0;
	} else if (!pj_stricmp(&fmtp->param[i].name, &STR_MAXAVERAGEBITRATE)) {
	    enc_bitrate = pj_strtoul(&fmtp->param[i].val);
	    if (enc_bitrate > sp->max_bitrate) {
		enc_bitrate = sp->max_bitrate;
	    }
	}
    }

    /* Setup encoder control for encoding process */
    silk->enc_ready = PJ_TRUE;
    silk->samples_per_frame = FRAME_LENGTH_MS *
			      attr->info.clock_rate / 1000;
    silk->pcm_bytes_per_sample = attr->info.pcm_bits_per_sample / 8;

    silk->enc_ctl.API_sampleRate        = attr->info.clock_rate;
    silk->enc_ctl.maxInternalSampleRate = attr->info.clock_rate;
    silk->enc_ctl.packetSize            = attr->setting.frm_per_pkt *
                                          silk->samples_per_frame;
    /* For useInBandFEC setting to be useful, we need to set
     * packetLossPercentage greater than LBRR_LOSS_THRES (1)
     */
    silk->enc_ctl.packetLossPercentage  = SILK_ENC_CTL_PACKET_LOSS_PCT;
    silk->enc_ctl.useInBandFEC          = enc_use_fec;
    silk->enc_ctl.useDTX                = attr->setting.vad;
    silk->enc_ctl.complexity            = sp->complexity;
    silk->enc_ctl.bitRate               = enc_bitrate;
    

    /* Allocate and initialize decoder */
    err = SKP_Silk_SDK_Get_Decoder_Size(&st_size);
    if (err) {
        PJ_LOG(3,(THIS_FILE, "Failed to get decoder state size (err=%d)",
		  err));
	return PJMEDIA_CODEC_EFAILED;
    }
    silk->dec_st = pj_pool_zalloc(silk->pool, st_size);
    err = SKP_Silk_SDK_InitDecoder(silk->dec_st);
    if (err) {
        PJ_LOG(3,(THIS_FILE, "Failed to init decoder (err=%d)", err));
	return PJMEDIA_CODEC_EFAILED;
    }

    /* Setup decoder control for decoding process */
    silk->dec_ctl.API_sampleRate        = attr->info.clock_rate;
    silk->dec_ctl.framesPerPacket	= 1; /* for proper PLC at start */
    silk->dec_ready = PJ_TRUE;
    silk->dec_buf_sz = attr->info.clock_rate * attr->info.channel_cnt *
                       attr->info.frm_ptime / 1000 *
                       silk->pcm_bytes_per_sample;

    /* Inform the stream to prepare a larger buffer since we cannot parse
     * SILK packets and split it into individual frames.
     */
    attr->info.max_rx_frame_size = attr->info.max_bps * 
			           attr->info.frm_ptime / 8 / 1000;
    if ((attr->info.max_bps * attr->info.frm_ptime) % 8000 != 0)
    {
	++attr->info.max_rx_frame_size;
    }
    attr->info.max_rx_frame_size *= SILK_MAX_FRAMES_PER_PACKET;

    return PJ_SUCCESS;
}


/*
 * Close codec.
 */
static pj_status_t silk_codec_close( pjmedia_codec *codec )
{
    silk_private *silk;
    silk = (silk_private*)codec->codec_data;

    silk->enc_ready = PJ_FALSE;
    silk->dec_ready = PJ_FALSE;

    return PJ_SUCCESS;
}


/*
 * Modify codec settings.
 */
static pj_status_t  silk_codec_modify(pjmedia_codec *codec,
				      const pjmedia_codec_param *attr )
{
    PJ_TODO(implement_silk_codec_modify);

    PJ_UNUSED_ARG(codec);
    PJ_UNUSED_ARG(attr);

    return PJ_SUCCESS;
}


/*
 * Encode frame.
 */
static pj_status_t silk_codec_encode(pjmedia_codec *codec,
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len,
				     struct pjmedia_frame *output)
{
    silk_private *silk;
    SKP_int err;
    unsigned nsamples;
    SKP_int16 out_size;

    PJ_ASSERT_RETURN(codec && input && output_buf_len && output, PJ_EINVAL);
    silk = (silk_private*)codec->codec_data;

    /* Check frame in size */
    nsamples = input->size >> 1;
    PJ_ASSERT_RETURN(nsamples % silk->samples_per_frame == 0,
		     PJMEDIA_CODEC_EPCMFRMINLEN);

    /* Encode */
    output->size = 0;
    out_size = (SKP_int16)output_buf_len;
    err = SKP_Silk_SDK_Encode(silk->enc_st, &silk->enc_ctl,
			     (SKP_int16*)input->buf, nsamples,
			     (SKP_uint8*)output->buf, &out_size);
    if (err) {
	PJ_LOG(3, (THIS_FILE, "Failed to encode frame (err=%d)", err));
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	if (err == SKP_SILK_ENC_PAYLOAD_BUF_TOO_SHORT)
	    return PJMEDIA_CODEC_EFRMTOOSHORT;
	return PJMEDIA_CODEC_EFAILED;
    }

    output->size = out_size;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}


/*
 * Get frames in the packet.
 */
static pj_status_t  silk_codec_parse( pjmedia_codec *codec,
				     void *pkt,
				     pj_size_t pkt_size,
				     const pj_timestamp *ts,
				     unsigned *frame_cnt,
				     pjmedia_frame frames[])
{
    silk_private *silk;
    SKP_Silk_TOC_struct toc;
    unsigned i, count;

    PJ_ASSERT_RETURN(codec && ts && frames && frame_cnt, PJ_EINVAL);
    silk = (silk_private*)codec->codec_data;

    SKP_Silk_SDK_get_TOC(pkt, pkt_size, &toc);
    count = toc.framesInPacket;
    pj_assert(count <= SILK_MAX_FRAMES_PER_PACKET);

    for (i = 0; i < count; i++) {
        frames[i].type = PJMEDIA_FRAME_TYPE_AUDIO;
        frames[i].bit_info = (((unsigned)ts->u64 & 0xFFFF) << 16) |
                              (((unsigned)pkt & 0xFF) << 8) |
                              (toc.framesInPacket << 4) | i;
        frames[i].buf = pkt;
        frames[i].size = pkt_size;
        frames[i].timestamp.u64 = ts->u64 + i * silk->samples_per_frame;
    }

    *frame_cnt = count;
    return PJ_SUCCESS;
}

static pj_status_t silk_codec_decode(pjmedia_codec *codec,
				     const struct pjmedia_frame *input,
				     unsigned output_buf_len,
				     struct pjmedia_frame *output)
{
    silk_private *silk;
    SKP_int16 out_size;
    SKP_Silk_TOC_struct toc;
    SKP_int err = 0;
    unsigned pkt_info, frm_info;

    PJ_ASSERT_RETURN(codec && input && output_buf_len && output, PJ_EINVAL);
    silk = (silk_private*)codec->codec_data;
    PJ_ASSERT_RETURN(output_buf_len >= silk->dec_buf_sz, PJ_ETOOSMALL);

    SKP_Silk_SDK_get_TOC(input->buf, input->size, &toc);
    pkt_info = input->bit_info & 0xFFFFFF00;
    frm_info = input->bit_info & 0xF;

    if (toc.framesInPacket == 0) {
        /* In SILK ARM version, the table of content can indicate
         * that the number of frames in the packet is 0.
         * Try to get the number of frames in packet that we save
         * in the frame instead.
         */
        toc.framesInPacket = (input->bit_info & 0xF0) >> 4;
    }
    
    if (toc.framesInPacket == 0) {
        output->size = 0;
    } else if (silk->pkt_info != pkt_info || input->bit_info == 0) {
        unsigned i;
        SKP_int16 nsamples;

        silk->pkt_info = pkt_info;
        nsamples = (SKP_int16)silk->dec_buf_sz / silk->pcm_bytes_per_sample;

        if (toc.framesInPacket-1 > (SKP_int)silk->dec_buf_cnt) {
            /* Grow the buffer */
            for (i = silk->dec_buf_cnt+1; i < (unsigned)toc.framesInPacket;
                i++)
            {
                silk->dec_buf[i-1] = pj_pool_alloc(silk->pool,
                                                   silk->dec_buf_sz);
            }
            silk->dec_buf_cnt = toc.framesInPacket-1;
        }

        /* We need to decode all the frames in the packet. */
        for (i = 0; i < (unsigned)toc.framesInPacket;) {
            void *buf;
            SKP_int16 *size;

            if (i == 0 || i == frm_info) {
                buf = output->buf;
                size = &out_size;
            } else {
                buf = silk->dec_buf[i-1];
                size = &silk->dec_buf_size[i-1];
            }

            *size = nsamples;
            err = SKP_Silk_SDK_Decode(silk->dec_st, &silk->dec_ctl,
			              0, /* Normal frame flag */
			              input->buf, input->size,
			              buf, size);
            if (err) {
	        PJ_LOG(3, (THIS_FILE, "Failed to decode frame (err=%d)",
                                      err));
                *size = 0;
            } else {
                *size = *size * silk->pcm_bytes_per_sample;
            }

            if (i == frm_info) {
                output->size = *size;
            }

            i++;
            if (!silk->dec_ctl.moreInternalDecoderFrames &&
                i < (unsigned)toc.framesInPacket)
            {
                /* It turns out that the packet does not have
                 * the number of frames as mentioned in the TOC.
                 */
                for (; i < (unsigned)toc.framesInPacket; i++) {
                    silk->dec_buf_size[i-1] = 0;
                    if (i == frm_info) {
                        output->size = 0;
                    }
                }
            }
        }
    } else {
        /* We have already decoded this packet. */
        if (frm_info == 0 || silk->dec_buf_size[frm_info-1] == 0) {
            /* The decoding was a failure. */
            output->size = 0;
        } else {
            /* Copy the decoded frame from the buffer. */
            pj_assert(frm_info-1 < silk->dec_buf_cnt);
            if (frm_info-1 >= silk->dec_buf_cnt) {
                output->size = 0;
            } else {
                pj_memcpy(output->buf, silk->dec_buf[frm_info-1],
                          silk->dec_buf_size[frm_info-1]);
                output->size = silk->dec_buf_size[frm_info-1];
            }
        }
    }

    if (output->size == 0) {
        output->type = PJMEDIA_TYPE_NONE;
        output->buf = NULL;
        return PJMEDIA_CODEC_EFAILED;
    }

    output->type = PJMEDIA_TYPE_AUDIO;
    output->timestamp = input->timestamp;

    return PJ_SUCCESS;
}


/*
 * Recover lost frame.
 */
static pj_status_t  silk_codec_recover(pjmedia_codec *codec,
				       unsigned output_buf_len,
				       struct pjmedia_frame *output)
{
    silk_private *silk;
    SKP_int16 out_size;
    SKP_int err;

    PJ_ASSERT_RETURN(codec && output_buf_len && output, PJ_EINVAL);
    silk = (silk_private*)codec->codec_data;

    out_size = (SKP_int16)output_buf_len / silk->pcm_bytes_per_sample;
    err = SKP_Silk_SDK_Decode(silk->dec_st, &silk->dec_ctl,
			      1, /* Lost frame flag */
			      NULL,
			      0,
			      output->buf,
			      &out_size);
    if (err) {
	PJ_LOG(3, (THIS_FILE, "Failed to conceal lost frame (err=%d)", err));
	output->type = PJMEDIA_FRAME_TYPE_NONE;
	output->buf = NULL;
	output->size = 0;
	return PJMEDIA_CODEC_EFAILED;
    }

    output->size = out_size * silk->pcm_bytes_per_sample;
    output->type = PJMEDIA_FRAME_TYPE_AUDIO;

    return PJ_SUCCESS;
}

#if defined(_MSC_VER)
#  if PJ_DEBUG
#   pragma comment(lib, "SKP_Silk_FLP_Win32_debug.lib")
#  else
#   pragma comment(lib, "SKP_Silk_FLP_Win32_mt.lib")
#  endif
#endif


#endif /* PJMEDIA_HAS_SILK_CODEC */
